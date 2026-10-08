#pragma once

#include "sshagentformat.h"
#include "sshconfig/sshhost.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// An IdentityFile from the SSH configuration.
struct SshIdentityFile {
    // Absolute, cleaned path when resolved; otherwise the value as written,
    // without quotes (e.g. it uses a token only ssh can expand).
    QString path;
    bool resolved = false;

    bool operator==(const SshIdentityFile &other) const = default;
};

namespace SshIdentityFiles
{
// Expands `~`, `%d`, `%u` and `%%` the way ssh does. Anything else (other
// tokens, environment variables, relative paths) stays unresolved, because it
// depends on the host or on where ssh runs.
SshIdentityFile resolve(const QString &value, const QString &homeDir, const QString &userName);
// Every IdentityFile of every block (Host and Match), in file order, once each.
QList<SshIdentityFile> fromHosts(const QList<SshHost> &hosts, const QString &homeDir, const QString &userName);
}

// What is known about a key file on disk.
struct SshKeyFileStatus {
    bool exists = false;
    std::optional<SshKeyFileInfo> info; // empty when ssh-keygen couldn't read it
};

// One row of the SSH agent panel.
struct SshKeyEntry {
    enum class Source {
        Configuration, // an IdentityFile of the SSH configuration
        Added, // a key file the user chose
        Agent, // loaded in the agent, but none of the above
    };
    enum class State {
        Loaded,
        NotLoaded,
        Missing, // the file doesn't exist or its path can't be resolved
        Unknown, // the file's fingerprint couldn't be read
    };

    Source source = Source::Configuration;
    State state = State::NotLoaded;
    QString path; // empty for agent keys
    QString type;
    QString comment;
    QString fingerprint;
    QList<QByteArray> agentKeys; // public key lines of the matching agent keys, for removal

    // File name, or for agent keys the comment (or fingerprint).
    QString name() const;
    // Identifies the row across refreshes.
    QString id() const;
};

namespace SshKeyList
{
// Configuration keys first, then added keys, then other agent keys. Added keys
// that are also in the configuration are listed only once, as configuration
// keys. Keys are matched to agent keys by fingerprint.
QList<SshKeyEntry> build(const QList<SshIdentityFile> &configFiles,
                         const QStringList &addedFiles,
                         const QHash<QString, SshKeyFileStatus> &fileStatus,
                         const QList<SshAgentKey> &agentKeys);
}
