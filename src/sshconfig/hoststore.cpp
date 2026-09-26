#include "hoststore.h"
#include "logging.h"
#include "sshconfigparser.h"

#include <QDir>
#include <QFileInfo>

namespace
{
constexpr int ReloadDelayMs = 300;
}

HostStore::HostStore(const SshPaths &paths, QObject *parent)
    : QObject(parent)
    , m_paths(paths)
    , m_writer(paths.backupDir)
{
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(ReloadDelayMs);
    connect(&m_reloadTimer, &QTimer::timeout, this, &HostStore::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reloadTimer, qOverload<>(&QTimer::start));
    reload();
}

const SshPaths &HostStore::paths() const
{
    return m_paths;
}

const QList<SshHost> &HostStore::hosts() const
{
    return m_hosts;
}

bool HostStore::isManagedFileIncluded() const
{
    return m_managedIncluded;
}

const QStringList &HostStore::loadErrors() const
{
    return m_errors;
}

void HostStore::reload()
{
    m_reloadTimer.stop();
    const SshConfigLoader::Result result = SshConfigLoader(m_paths).load();

    m_hosts.clear();
    QStringList files;
    for (const SshConfigFile &file : result.files) {
        m_hosts += file.document.hosts(file.path, !file.managed);
        files.append(file.path);
    }
    // Watch the managed file and user config even before they exist (via their directories).
    files << m_paths.userConfig << m_paths.managedFile;
    m_managedIncluded = result.managedFileIncluded;
    m_errors = result.errors;
    updateWatcher(files, result.watchDirectories);
    qCDebug(KSSHM_CONFIG) << "Reloaded" << m_hosts.size() << "blocks from" << result.files.size() << "files";
    Q_EMIT hostsChanged();
}

std::optional<SshHost> HostStore::readManagedHost(const QString &alias) const
{
    SshConfigDocument document;
    if (!readManagedDocument(&document).ok()) {
        return std::nullopt;
    }
    const qsizetype index = document.findHost(alias);
    if (index < 0) {
        return std::nullopt;
    }
    return document.hosts(m_paths.managedFile, false).at(index);
}

bool HostStore::isAliasDefinedOutsideManagedFile(const QString &alias) const
{
    for (const SshHost &host : m_hosts) {
        if (!host.readOnly || host.kind != SshHost::Kind::Host) {
            continue;
        }
        for (const QString &pattern : host.patterns) {
            if (pattern.compare(alias, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }
    return false;
}

HostStore::Result HostStore::addHost(const SshHost &host)
{
    SshConfigDocument document;
    Result result = readManagedDocument(&document);
    if (!result.ok()) {
        return result;
    }
    for (const QString &pattern : host.patterns) {
        if (document.containsPattern(pattern)) {
            return {Error::AliasExists, pattern};
        }
    }
    if (!SshConfigWriter::addHost(document, host)) {
        return {Error::InvalidHost, {}};
    }
    return saveManagedDocument(document);
}

HostStore::Result HostStore::updateHost(const QString &alias, const SshHost &host)
{
    SshConfigDocument document;
    Result result = readManagedDocument(&document);
    if (!result.ok()) {
        return result;
    }
    const qsizetype index = document.findHost(alias);
    if (index < 0) {
        return {Error::HostNotFound, alias};
    }
    // A renamed host must not collide with another block in the managed file.
    const QStringList oldPatterns = document.hosts().at(index).patterns;
    for (const QString &pattern : host.patterns) {
        const bool ownPattern = oldPatterns.contains(pattern, Qt::CaseInsensitive);
        if (!ownPattern && document.containsPattern(pattern)) {
            return {Error::AliasExists, pattern};
        }
    }
    if (!SshConfigWriter::updateHost(document, alias, host)) {
        return {Error::InvalidHost, {}};
    }
    return saveManagedDocument(document);
}

HostStore::Result HostStore::removeHost(const QString &alias)
{
    SshConfigDocument document;
    Result result = readManagedDocument(&document);
    if (!result.ok()) {
        return result;
    }
    if (!SshConfigWriter::removeHost(document, alias)) {
        return {Error::HostNotFound, alias};
    }
    return saveManagedDocument(document);
}

HostStore::Result HostStore::addIncludeLine()
{
    if (SshConfigLoader(m_paths).load().managedFileIncluded) {
        return {};
    }
    QByteArray config;
    QFileDevice::Permissions permissions = SshConfigWriter::FilePermissions;
    if (QFileInfo::exists(m_paths.userConfig)) {
        QFile file(m_paths.userConfig);
        if (!file.open(QIODevice::ReadOnly)) {
            return {Error::ReadFailed, file.errorString()};
        }
        config = file.readAll();
        permissions = file.permissions();
    } else if (!QFileInfo::exists(m_paths.sshDir)) {
        QString error;
        if (!SshConfigWriter::ensurePrivateDirectory(m_paths.sshDir, &error)) {
            return {Error::WriteFailed, error};
        }
    }

    QString error;
    const QByteArray updated = SshConfigWriter::withIncludeLine(config, SshPaths::includeArgument());
    if (!m_writer.writeFile(m_paths.userConfig, updated, permissions, &error)) {
        return {Error::WriteFailed, error};
    }
    reload();
    return {};
}

HostStore::Result HostStore::readManagedDocument(SshConfigDocument *document) const
{
    if (!QFileInfo::exists(m_paths.managedFile)) {
        *document = SshConfigDocument();
        return {};
    }
    QString error;
    if (!SshConfigParser::parseFile(m_paths.managedFile, document, &error)) {
        return {Error::ReadFailed, error};
    }
    return {};
}

HostStore::Result HostStore::saveManagedDocument(const SshConfigDocument &document)
{
    QString error;
    // Don't touch the permissions of an existing ~/.ssh; only create it privately.
    if (!QFileInfo::exists(m_paths.sshDir) && !SshConfigWriter::ensurePrivateDirectory(m_paths.sshDir, &error)) {
        return {Error::WriteFailed, error};
    }
    if (!SshConfigWriter::ensurePrivateDirectory(m_paths.managedDir, &error)) {
        return {Error::WriteFailed, error};
    }
    if (!m_writer.writeFile(m_paths.managedFile, document.serialize(), SshConfigWriter::FilePermissions, &error)) {
        return {Error::WriteFailed, error};
    }
    reload();
    return {};
}

void HostStore::updateWatcher(const QStringList &files, const QStringList &directories)
{
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty()) {
        m_watcher.removePaths(watched);
    }
    QStringList paths;
    for (const QString &path : files + directories) {
        if (QFileInfo::exists(path) && !paths.contains(path)) {
            paths.append(path);
        }
    }
    if (!paths.isEmpty()) {
        m_watcher.addPaths(paths);
    }
}
