#pragma once

#include "sshconfigdocument.h"

#include <QByteArray>
#include <QFileDevice>
#include <QSet>
#include <QString>

// Edits documents with minimal changes and writes files atomically.
//
// One writer instance should live for the whole app session: it remembers
// which files were already backed up, so each file gets exactly one
// timestamped backup before its first write in the session.
class SshConfigWriter
{
public:
    explicit SshConfigWriter(const QString &backupDirectory);

    static constexpr QFileDevice::Permissions FilePermissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    static constexpr QFileDevice::Permissions DirectoryPermissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;

    // --- Document edits (no file access) ---

    // Appends a Host block. Returns false if the host isn't a valid Host entry.
    static bool addHost(SshConfigDocument &document, const SshHost &host);
    // Rewrites the block whose alias is `alias`. Lines that didn't change keep
    // their exact bytes; comments and unknown keywords are left alone.
    static bool updateHost(SshConfigDocument &document, QStringView alias, const SshHost &host);
    // Removes the block (and its metadata comment). Comments directly above the
    // next block are kept.
    static bool removeHost(SshConfigDocument &document, QStringView alias);
    // Returns `config` with `Include <argument>` as a new first line, so it comes
    // before the first Host or Match block. Nothing else is changed.
    static QByteArray withIncludeLine(const QByteArray &config, const QString &argument);
    // True if `host` can be written to the managed file.
    static bool isWritableHost(const SshHost &host);

    // --- File access ---

    // Atomically replaces `path` with `data` (QSaveFile) and sets `permissions`.
    // Backs the existing file up first if this is its first write this session.
    bool writeFile(const QString &path, const QByteArray &data, QFileDevice::Permissions permissions, QString *error);
    // Creates `path` if needed and sets it to 0700.
    static bool ensurePrivateDirectory(const QString &path, QString *error);

    QString backupDirectory() const;

private:
    bool backupOnce(const QString &path, QString *error);

    QString m_backupDirectory;
    QSet<QString> m_backedUp;
};
