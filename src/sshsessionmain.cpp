// konsole-ssh-session: what each terminal tab runs. See sshsession.h.

#include "sshconfig/sshhost.h"
#include "sshsession.h"

#include <QCoreApplication>
#include <QFile>

#include <KLocalizedString>

#include <cstdio>
#include <unistd.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("konsole-ssh-manager");

    const QStringList arguments = QCoreApplication::arguments();
    // Defense in depth: the app validates too, but never pass anything that
    // could be read as an ssh option.
    if (arguments.size() != 2 || !SshValidation::isValidAlias(arguments.at(1))) {
        std::fprintf(stderr, "%s\n", i18n("Usage: %1 <alias>", SshSession::helperName()).toLocal8Bit().constData());
        return 2;
    }
    // Only the helper talks to the app; ssh and the commands it runs don't inherit the socket.
    const QString reportServer = QFile::decodeName(qgetenv(SshSession::ReportServerVariable));
    qunsetenv(SshSession::ReportServerVariable);
    return SshSession::run(QStringLiteral("ssh"), arguments.at(1), STDIN_FILENO, STDOUT_FILENO, reportServer);
}
