#include "sshsession.h"

#include <QByteArray>
#include <QFile>
#include <QList>

#include <KLocalizedString>

#include <cerrno>
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace
{
void writeAll(int fd, const QByteArray &data)
{
    qsizetype written = 0;
    while (written < data.size()) {
        const ssize_t n = ::write(fd, data.constData() + written, size_t(data.size() - written));
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return;
        }
        written += n;
    }
}
}

int SshSession::ChildResult::shellStatus() const
{
    if (!started) {
        return 127;
    }
    return signal != 0 ? 128 + signal : exitCode;
}

QString SshSession::helperName()
{
    return QStringLiteral("konsole-ssh-session");
}

SshSession::ChildResult SshSession::runChild(const QStringList &arguments)
{
    ChildResult result;
    if (arguments.isEmpty()) {
        return result;
    }
    QList<QByteArray> encoded;
    for (const QString &argument : arguments) {
        encoded.append(QFile::encodeName(argument));
    }
    std::vector<char *> argv;
    for (QByteArray &argument : encoded) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    struct sigaction ignore = {};
    struct sigaction oldInt = {};
    struct sigaction oldQuit = {};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    sigaction(SIGINT, &ignore, &oldInt);
    sigaction(SIGQUIT, &ignore, &oldQuit);

    posix_spawnattr_t attributes;
    posix_spawnattr_init(&attributes);
    sigset_t defaults;
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGINT);
    sigaddset(&defaults, SIGQUIT);
    posix_spawnattr_setsigdefault(&attributes, &defaults);
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETSIGDEF);

    pid_t pid = 0;
    if (posix_spawnp(&pid, argv.front(), nullptr, &attributes, argv.data(), environ) == 0) {
        result.started = true;
        int status = 0;
        pid_t waited = -1;
        do {
            waited = ::waitpid(pid, &status, 0);
        } while (waited < 0 && errno == EINTR);
        if (waited == pid && WIFEXITED(status)) {
            result.exitCode = WEXITSTATUS(status);
        } else if (waited == pid && WIFSIGNALED(status)) {
            result.signal = WTERMSIG(status);
        }
    }
    posix_spawnattr_destroy(&attributes);

    sigaction(SIGINT, &oldInt, nullptr);
    sigaction(SIGQUIT, &oldQuit, nullptr);
    return result;
}

QString SshSession::failureMessage(const QString &alias, const ChildResult &result)
{
    if (!result.started) {
        return i18n("Could not start ssh. Make sure the OpenSSH client is installed (Debian package \"openssh-client\").");
    }
    if (result.signal != 0) {
        return i18n("ssh was terminated by signal %1.", result.signal);
    }
    if (result.exitCode == 255) {
        return i18n("ssh could not connect to %1, or the connection was lost (exit status 255). See ssh's message above.", alias);
    }
    return i18n("The session to %1 ended with exit status %2.", alias, result.exitCode);
}

void SshSession::waitForEnter(int fd)
{
    if (::isatty(fd)) {
        ::tcflush(fd, TCIFLUSH);
    }
    char c = 0;
    while (true) {
        const ssize_t n = ::read(fd, &c, 1);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0 || c == '\n' || c == '\r') {
            return;
        }
    }
}

int SshSession::run(const QString &program, const QString &alias, int inputFd, int outputFd)
{
    const ChildResult result = runChild({program, alias});
    if (result.succeeded()) {
        return 0;
    }
    // "\r\n": a crashed ssh may leave the terminal without newline translation.
    const QString text = QStringLiteral("\r\n%1\r\n%2 ").arg(failureMessage(alias, result), i18n("Press Enter to close this tab."));
    writeAll(outputFd, text.toUtf8());
    waitForEnter(inputFd);
    return result.shellStatus();
}
