#include "sshkeylist.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

SshIdentityFile SshIdentityFiles::resolve(const QString &value, const QString &homeDir, const QString &userName)
{
    QString path = value.trimmed();
    if (path.size() >= 2 && path.startsWith(QLatin1Char('"')) && path.endsWith(QLatin1Char('"'))) {
        path = path.mid(1, path.size() - 2);
    }
    const SshIdentityFile unresolved{path, false};

    QString expanded;
    if (path == QLatin1String("~")) {
        expanded = homeDir;
    } else if (path.startsWith(QLatin1String("~/"))) {
        expanded = homeDir + path.mid(1);
    } else {
        expanded = path;
    }
    if (expanded.contains(QLatin1String("${"))) {
        return unresolved;
    }

    QString result;
    for (qsizetype i = 0; i < expanded.size(); ++i) {
        const QChar c = expanded.at(i);
        if (c != QLatin1Char('%')) {
            result += c;
            continue;
        }
        if (++i >= expanded.size()) {
            return unresolved;
        }
        const QChar token = expanded.at(i);
        if (token == QLatin1Char('%')) {
            result += QLatin1Char('%');
        } else if (token == QLatin1Char('d')) {
            result += homeDir;
        } else if (token == QLatin1Char('u') && !userName.isEmpty()) {
            result += userName;
        } else {
            return unresolved;
        }
    }
    if (!QDir::isAbsolutePath(result)) {
        return unresolved;
    }
    return {QDir::cleanPath(result), true};
}

QList<SshIdentityFile> SshIdentityFiles::fromHosts(const QList<SshHost> &hosts, const QString &homeDir, const QString &userName)
{
    QList<SshIdentityFile> files;
    for (const SshHost &host : hosts) {
        for (const SshOption &option : host.options) {
            if (option.keyword.compare(QLatin1String("IdentityFile"), Qt::CaseInsensitive) != 0) {
                continue;
            }
            const SshIdentityFile file = resolve(option.value, homeDir, userName);
            if (!file.path.isEmpty() && !files.contains(file)) {
                files.append(file);
            }
        }
    }
    return files;
}

QString SshKeyEntry::name() const
{
    if (!path.isEmpty()) {
        const QString fileName = QFileInfo(path).fileName();
        return fileName.isEmpty() ? path : fileName;
    }
    return comment.isEmpty() ? fingerprint : comment;
}

QString SshKeyEntry::id() const
{
    return path.isEmpty() ? QStringLiteral("agent:") + fingerprint : QStringLiteral("file:") + path;
}

QList<SshKeyEntry> SshKeyList::build(const QList<SshIdentityFile> &configFiles,
                                     const QStringList &addedFiles,
                                     const QHash<QString, SshKeyFileStatus> &fileStatus,
                                     const QList<SshAgentKey> &agentKeys)
{
    QList<SshKeyEntry> entries;
    QSet<QString> listedPaths;
    QSet<QString> matchedFingerprints;

    const auto addFile = [&](const QString &path, bool resolved, SshKeyEntry::Source source) {
        if (listedPaths.contains(path)) {
            return;
        }
        listedPaths.insert(path);
        SshKeyEntry entry;
        entry.source = source;
        entry.path = path;
        const SshKeyFileStatus status = resolved ? fileStatus.value(path) : SshKeyFileStatus{};
        if (!status.exists) {
            entry.state = SshKeyEntry::State::Missing;
        } else if (!status.info) {
            entry.state = SshKeyEntry::State::Unknown;
        } else {
            entry.type = status.info->type;
            entry.comment = status.info->comment;
            entry.fingerprint = status.info->fingerprint;
            for (const SshAgentKey &key : agentKeys) {
                if (key.fingerprint == entry.fingerprint) {
                    entry.agentKeys.append(key.publicKeyLine);
                }
            }
            entry.state = entry.agentKeys.isEmpty() ? SshKeyEntry::State::NotLoaded : SshKeyEntry::State::Loaded;
            matchedFingerprints.insert(entry.fingerprint);
        }
        entries.append(entry);
    };

    for (const SshIdentityFile &file : configFiles) {
        addFile(file.path, file.resolved, SshKeyEntry::Source::Configuration);
    }
    for (const QString &path : addedFiles) {
        addFile(path, true, SshKeyEntry::Source::Added);
    }

    // A key and its certificate share a fingerprint: list them as one row.
    for (const SshAgentKey &key : agentKeys) {
        if (matchedFingerprints.contains(key.fingerprint)) {
            continue;
        }
        const auto existing = std::find_if(entries.begin(), entries.end(), [&key](const SshKeyEntry &entry) {
            return entry.source == SshKeyEntry::Source::Agent && entry.fingerprint == key.fingerprint;
        });
        if (existing != entries.end()) {
            existing->agentKeys.append(key.publicKeyLine);
            if (!key.isCertificate()) {
                existing->type = SshAgentFormat::displayType(key.keyType);
            }
            continue;
        }
        SshKeyEntry entry;
        entry.source = SshKeyEntry::Source::Agent;
        entry.state = SshKeyEntry::State::Loaded;
        entry.type = SshAgentFormat::displayType(key.keyType);
        entry.comment = key.comment;
        entry.fingerprint = key.fingerprint;
        entry.agentKeys.append(key.publicKeyLine);
        entries.append(entry);
    }
    return entries;
}
