#pragma once

#include "sshhost.h"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// Runs `ssh -G` so OpenSSH itself resolves and validates configuration,
// instead of reimplementing its rules. ssh is always started with an argv
// list, never through a shell.
struct SshCommandResult {
    bool started = false;
    bool timedOut = false;
    int exitCode = -1;
    QByteArray standardOutput;
    QByteArray standardError;

    bool succeeded() const
    {
        return started && !timedOut && exitCode == 0;
    }
};

namespace SshResolver
{
// Absolute path of the ssh client, or empty if it isn't installed.
QString sshProgram();

// Writes `host` alone to a temporary config file and runs `ssh -G -F <tmp> <alias>`,
// so a keyword or value OpenSSH rejects produces its own error message. The
// temporary path is removed from the error text.
SshCommandResult validateHost(const SshHost &host, int timeoutMs = 5000);
}

// Asynchronous `ssh -G <alias>` against the user's real configuration.
class SshEffectiveConfigJob : public QObject
{
    Q_OBJECT

public:
    explicit SshEffectiveConfigJob(QObject *parent = nullptr);

    // Returns false if the alias is invalid or ssh is missing.
    bool start(const QString &alias);

Q_SIGNALS:
    void finished(const SshCommandResult &result);

private:
    QProcess m_process;
};
