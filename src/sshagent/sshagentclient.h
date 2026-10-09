#pragma once

#include "sshagentformat.h"
#include "sshconfig/sshresolver.h"
#include "sshkeylist.h"

#include <QHash>
#include <QObject>
#include <QProcessEnvironment>
#include <QStringList>

#include <functional>

// Talks to the SSH agent of the app's environment (SSH_AUTH_SOCK) through
// ssh-add, and reads key file fingerprints with ssh-keygen. Programs are run
// asynchronously with an argument list, never through a shell. The agent
// itself is never started, stopped, locked or reconfigured, and passphrases
// never pass through this class: ssh-add asks for them through the askpass
// program set with setAskpassProgram().
class SshAgentClient : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        Unknown, // not checked yet
        NoAgent, // SSH_AUTH_SOCK isn't set
        NoTool, // ssh-add is missing or couldn't start
        Unreachable, // ssh-add couldn't connect to the agent
        NotResponding,
        Failed, // the agent answered with an error
        Available,
    };

    struct Snapshot {
        Status status = Status::Unknown;
        QString message; // ssh-add's message for Unreachable and Failed
        QList<SshAgentKey> keys;
        QHash<QString, SshKeyFileStatus> files;
    };

    explicit SshAgentClient(QObject *parent = nullptr);
    ~SshAgentClient() override;

    // Lists the agent's keys and reads the fingerprints of `files` (absolute
    // paths), then emits refreshed(). A newer refresh supersedes a running one.
    void refresh(const QStringList &files);
    // Loads a key file (absolute path) with ssh-add. Emits actionFinished().
    // Returns false without starting anything if the path is invalid or an
    // action is already running.
    bool addKey(const QString &path);
    // Removes keys, given as `ssh-add -L` lines, with ssh-add -d. Emits actionFinished().
    bool removeKeys(const QList<QByteArray> &publicKeyLines);
    bool isBusy() const;

    // Environment variable that tells the askpass program ssh-add started it.
    static constexpr const char *AskpassModeVariable = "KONSOLE_SSH_MANAGER_ASKPASS";
    // The program ssh-add runs to ask for passphrases; it replaces $SSH_ASKPASS.
    // When empty, ssh-add uses the environment's askpass.
    void setAskpassProgram(const QString &program);

Q_SIGNALS:
    void refreshed(const SshAgentClient::Snapshot &snapshot);
    void actionFinished(bool succeeded, const QString &message);

private:
    using Callback = std::function<void(const SshCommandResult &)>;
    void run(const QString &program, const QStringList &arguments, int timeoutMs, const QProcessEnvironment &environment, const Callback &done);
    void finishAction(const SshCommandResult &result, const QString &fallbackMessage);

    QString m_askpassProgram;
    quint64 m_generation = 0;
    bool m_busy = false;
};
