#pragma once

#include "sshconfigdocument.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

// Every path the app touches. Built from explicit directories so tests can
// point it at a temporary home instead of the real ~/.ssh.
struct SshPaths {
    QString homeDir;
    QString sshDir; // ~/.ssh
    QString userConfig; // ~/.ssh/config
    QString managedDir; // ~/.ssh/config.d
    QString managedFile; // ~/.ssh/config.d/konsole-ssh-manager.conf
    QString backupDir; // where timestamped backups go (outside config.d, so Include never picks them up)

    // The Include argument that pulls the managed file into ~/.ssh/config.
    static QString includeArgument();
    static SshPaths forHome(const QString &homeDir, const QString &backupDir);
};

struct SshConfigFile {
    QString path;
    SshConfigDocument document;
    bool managed = false;
};

class SshConfigLoader
{
public:
    struct Result {
        QList<SshConfigFile> files; // user config and everything it includes, then the managed file
        QStringList watchDirectories; // directories whose contents affect the result
        bool managedFileIncluded = false; // an Include in the user config matches the managed file
        QStringList errors; // human-readable problems (unreadable files, include depth)
    };

    explicit SshConfigLoader(const SshPaths &paths);

    Result load() const;

    // Expands `~` and resolves relative paths against ~/.ssh, like OpenSSH does
    // for the user config. The result may still contain glob characters.
    QString expandIncludePath(const QString &pattern) const;
    // Files matching an Include pattern, sorted like glob(3).
    QStringList resolveInclude(const QString &pattern) const;

    // Same limit as OpenSSH's READCONF_MAX_DEPTH.
    static constexpr int MaxIncludeDepth = 16;

private:
    void loadFile(const QString &path, int depth, Result &result, QSet<QString> &visited) const;

    SshPaths m_paths;
};
