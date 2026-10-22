#pragma once

#include <QString>
#include <QStringList>

// Logic of the `konsole-ssh-session` helper that each terminal tab runs.
//
// The helper starts `ssh <alias>` as a direct child (no shell, no extra
// options) in the tab's terminal. If ssh fails, it explains why below ssh's
// own output, reports the failure to the app so the tab can offer to retry,
// and waits for Enter, so the tab doesn't vanish before the user can read the
// error. A normal logout (exit status 0) closes the tab at once.
namespace SshSession
{
// Environment variable with the socket where the app listens for failed
// sessions (see SessionFailureListener). The helper doesn't pass it on to ssh.
constexpr const char *ReportServerVariable = "KONSOLE_SSH_MANAGER_SOCKET";

struct ChildResult {
    bool started = false;
    int exitCode = -1; // valid when started and not killed by a signal
    int signal = 0; // non-zero if the child was killed by a signal

    bool succeeded() const
    {
        return started && signal == 0 && exitCode == 0;
    }
    // Exit status in shell convention: 127 not started, 128+N killed by signal N.
    int shellStatus() const;
};

// File name of the helper executable, installed next to the app binary.
QString helperName();

// Runs `arguments` (arguments[0] is looked up in PATH) and waits for it.
// SIGINT/SIGQUIT are ignored by the caller while waiting and reset to their
// defaults in the child, like system(3) does.
ChildResult runChild(const QStringList &arguments);

// User-visible explanation for a failed session.
QString failureMessage(const QString &alias, const ChildResult &result);

// Tells the app listening on `serverName` that this process's session failed.
// The app recognizes the helper by the connecting process, so nothing is sent.
// Returns false if `serverName` is empty or nobody is listening.
bool reportFailure(const QString &serverName);

// Blocks until Enter (or EOF) is read from `fd`. Discards earlier keypresses.
void waitForEnter(int fd);

// Runs `<program> <alias>`. On failure writes the explanation to `outputFd`,
// reports it to `reportServer` (if not empty) and waits for Enter on
// `inputFd`. Returns the shell-convention status.
int run(const QString &program, const QString &alias, int inputFd, int outputFd, const QString &reportServer);
}
