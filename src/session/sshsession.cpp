#include "sshsession.h"
#include "sshconfig/sshconfigparser.h"

#include <QByteArray>
#include <QFile>
#include <QList>
#include <QLocalSocket>

#include <KLocalizedString>

#include <algorithm>
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

QString SshSession::commandLine(const SshHost &host)
{
    const QString alias = host.alias();
    QString hostName;
    QString user;
    QString port;
    QString proxyJump;
    // ssh refuses -J together with a ProxyCommand. As -o options the first of
    // the two wins, as in a config file, so then both keep their place.
    const bool hasProxyCommand = std::any_of(host.options.cbegin(), host.options.cend(), [](const SshOption &option) {
        return option.keyword.compare(QLatin1String("ProxyCommand"), Qt::CaseInsensitive) == 0;
    });
    QStringList arguments{QStringLiteral("ssh")};
    for (const SshOption &option : host.options) {
        const auto is = [&option](QStringView keyword) {
            return QStringView(option.keyword).compare(keyword, Qt::CaseInsensitive) == 0;
        };
        // OpenSSH uses the first value of these, and reads them unquoted.
        const QString first = SshConfigParser::splitArguments(option.value).value(0);
        if (is(u"HostName")) {
            if (hostName.isEmpty()) {
                // %h and %% are the only tokens HostName expands.
                for (qsizetype i = 0; i < first.size(); ++i) {
                    const QChar next = i + 1 < first.size() ? first.at(i + 1) : QChar();
                    if (first.at(i) == QLatin1Char('%') && next == QLatin1Char('h')) {
                        hostName += alias;
                        ++i;
                    } else if (first.at(i) == QLatin1Char('%') && next == QLatin1Char('%')) {
                        hostName += QLatin1Char('%');
                        ++i;
                    } else {
                        hostName += first.at(i);
                    }
                }
            }
        } else if (is(u"User")) {
            user = user.isEmpty() ? first : user;
        } else if (is(u"Port")) {
            port = port.isEmpty() ? first : port;
        } else if (is(u"ProxyJump") && !hasProxyCommand) {
            proxyJump = proxyJump.isEmpty() ? first : proxyJump;
        } else if (is(u"IdentityFile")) {
            arguments << QStringLiteral("-i") << first; // every IdentityFile is tried, in order
        } else {
            // ssh reads an -o value like a config line, so it keeps its quoting.
            QString value = option.value;
            const qsizetype comment = SshConfigParser::trailingCommentStart(value);
            if (comment >= 0) {
                value.truncate(comment);
            }
            arguments << QStringLiteral("-o") << QStringLiteral("%1=%2").arg(option.keyword, value);
        }
    }
    if (!port.isEmpty() && port.toInt() != 22) { // 22 is ssh's default
        arguments << QStringLiteral("-p") << port;
    }
    if (!proxyJump.isEmpty()) {
        arguments << QStringLiteral("-J") << proxyJump;
    }
    const QString destination = hostName.isEmpty() ? alias : hostName;
    arguments << (user.isEmpty() ? destination : user + QLatin1Char('@') + destination);

    const auto isPlain = [](QChar c) {
        return c.isLetterOrNumber() || QStringView(u"._-@%+=:,/~").contains(c);
    };
    QStringList quoted;
    for (const QString &argument : std::as_const(arguments)) {
        if (!argument.isEmpty() && std::all_of(argument.begin(), argument.end(), isPlain)) {
            quoted << argument;
        } else {
            // Inside single quotes the shell treats everything literally, except a quote itself.
            QString escaped = argument;
            escaped.replace(QLatin1Char('\''), QLatin1String("'\\''"));
            quoted << QLatin1Char('\'') + escaped + QLatin1Char('\'');
        }
    }
    return quoted.join(QLatin1Char(' '));
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

bool SshSession::reportFailure(const QString &serverName)
{
    if (serverName.isEmpty()) {
        return false;
    }
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (!socket.waitForConnected(1000)) {
        return false;
    }
    socket.disconnectFromServer();
    return true;
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

int SshSession::run(const QString &program, const QString &alias, int inputFd, int outputFd, const QString &reportServer)
{
    const ChildResult result = runChild({program, alias});
    if (result.succeeded()) {
        return 0;
    }
    // "\r\n": a crashed ssh may leave the terminal without newline translation.
    const QString text = QStringLiteral("\r\n%1\r\n%2 ").arg(failureMessage(alias, result), i18n("Press Enter to close this tab."));
    writeAll(outputFd, text.toUtf8());
    reportFailure(reportServer);
    waitForEnter(inputFd);
    return result.shellStatus();
}
