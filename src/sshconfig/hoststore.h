#pragma once

#include "sshconfigloader.h"
#include "sshconfigwriter.h"

#include <QFileSystemWatcher>
#include <QList>
#include <QObject>
#include <QTimer>

#include <optional>

// Loads every host the user can see and applies edits to the managed file.
//
// - Only the managed file is ever changed, except for the one-time
//   `Include config.d/*` line in ~/.ssh/config (addIncludeLine()), which the
//   UI must only call after the user confirmed it.
// - Every edit re-reads the managed file from disk first, so changes made by
//   other programs are never silently discarded.
// - All files and their directories are watched; hostsChanged() is emitted
//   after a (debounced) reload.
class HostStore : public QObject
{
    Q_OBJECT

public:
    enum class Error {
        None,
        InvalidHost,
        AliasExists,
        HostNotFound,
        ReadFailed,
        WriteFailed,
    };

    struct Result {
        Error error = Error::None;
        QString detail;

        bool ok() const
        {
            return error == Error::None;
        }
    };

    explicit HostStore(const SshPaths &paths, QObject *parent = nullptr);

    const SshPaths &paths() const;
    const QList<SshHost> &hosts() const;
    bool isManagedFileIncluded() const;
    const QStringList &loadErrors() const;

    void reload();

    // Current on-disk version of a managed host, or nullopt if it's gone.
    std::optional<SshHost> readManagedHost(const QString &alias) const;
    // True if another (non-managed) file defines a Host pattern equal to `alias`.
    bool isAliasDefinedOutsideManagedFile(const QString &alias) const;

    Result addHost(const SshHost &host);
    Result updateHost(const QString &alias, const SshHost &host);
    Result removeHost(const QString &alias);
    // Inserts `Include config.d/*` at the top of ~/.ssh/config (creating it if
    // needed). Does nothing if the managed file is already included.
    Result addIncludeLine();

Q_SIGNALS:
    void hostsChanged();

private:
    Result readManagedDocument(SshConfigDocument *document) const;
    Result saveManagedDocument(const SshConfigDocument &document);
    void updateWatcher(const QStringList &files, const QStringList &directories);

    SshPaths m_paths;
    SshConfigWriter m_writer;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    QList<SshHost> m_hosts;
    bool m_managedIncluded = false;
    QStringList m_errors;
};
