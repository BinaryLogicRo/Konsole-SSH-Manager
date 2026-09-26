#include "sshconfigloader.h"
#include "logging.h"
#include "sshconfigparser.h"

#include <QFile>
#include <QFileInfo>

#include <KLocalizedString>

#include <fnmatch.h>
#include <glob.h>

namespace
{
QString canonicalKey(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

bool matchesPattern(const QString &pattern, const QString &path)
{
    return ::fnmatch(QFile::encodeName(pattern).constData(), QFile::encodeName(path).constData(), FNM_PATHNAME) == 0;
}
}

QString SshPaths::includeArgument()
{
    return QStringLiteral("config.d/*");
}

SshPaths SshPaths::forHome(const QString &homeDir, const QString &backupDir)
{
    SshPaths paths;
    paths.homeDir = homeDir;
    paths.sshDir = homeDir + QStringLiteral("/.ssh");
    paths.userConfig = paths.sshDir + QStringLiteral("/config");
    paths.managedDir = paths.sshDir + QStringLiteral("/config.d");
    paths.managedFile = paths.managedDir + QStringLiteral("/konsole-ssh-manager.conf");
    paths.backupDir = backupDir;
    return paths;
}

SshConfigLoader::SshConfigLoader(const SshPaths &paths)
    : m_paths(paths)
{
}

SshConfigLoader::Result SshConfigLoader::load() const
{
    Result result;
    QSet<QString> visited;
    result.watchDirectories << m_paths.sshDir << m_paths.managedDir;

    if (QFileInfo::exists(m_paths.userConfig)) {
        loadFile(m_paths.userConfig, 0, result, visited);
    }
    // The managed file is shown even if the user config doesn't include it yet.
    if (!visited.contains(canonicalKey(m_paths.managedFile)) && QFileInfo::exists(m_paths.managedFile)) {
        loadFile(m_paths.managedFile, 0, result, visited);
    }
    result.watchDirectories.removeDuplicates();
    return result;
}

QString SshConfigLoader::expandIncludePath(const QString &pattern) const
{
    if (pattern == QLatin1String("~")) {
        return m_paths.homeDir;
    }
    if (pattern.startsWith(QLatin1String("~/"))) {
        return m_paths.homeDir + pattern.mid(1);
    }
    if (pattern.startsWith(QLatin1Char('/'))) {
        return pattern;
    }
    return m_paths.sshDir + QLatin1Char('/') + pattern;
}

QStringList SshConfigLoader::resolveInclude(const QString &pattern) const
{
    QStringList result;
    glob_t matches{};
    if (::glob(QFile::encodeName(expandIncludePath(pattern)).constData(), 0, nullptr, &matches) == 0) {
        for (size_t i = 0; i < matches.gl_pathc; ++i) {
            result.append(QFile::decodeName(matches.gl_pathv[i]));
        }
    }
    ::globfree(&matches);
    return result;
}

void SshConfigLoader::loadFile(const QString &path, int depth, Result &result, QSet<QString> &visited) const
{
    const QString key = canonicalKey(path);
    if (visited.contains(key)) {
        return;
    }
    visited.insert(key);

    const QFileInfo info(path);
    if (!info.isFile()) {
        return;
    }
    SshConfigFile file;
    file.path = path;
    file.managed = key == canonicalKey(m_paths.managedFile);
    QString error;
    if (!SshConfigParser::parseFile(path, &file.document, &error)) {
        result.errors.append(i18n("Could not read %1: %2", path, error));
        return;
    }
    qCDebug(KSSHM_CONFIG) << "Loaded" << path << "with" << file.document.lines.size() << "lines";
    const QStringList includes = file.document.includePatterns();
    result.files.append(file);

    for (const QString &pattern : includes) {
        const QString expanded = expandIncludePath(pattern);
        if (!file.managed && matchesPattern(expanded, m_paths.managedFile)) {
            result.managedFileIncluded = true;
        }
        result.watchDirectories.append(QFileInfo(expanded).path());
        if (depth + 1 > MaxIncludeDepth) {
            result.errors.append(i18n("Include nested too deeply in %1.", path));
            continue;
        }
        const QStringList matches = resolveInclude(pattern);
        for (const QString &match : matches) {
            loadFile(match, depth + 1, result, visited);
        }
    }
}
