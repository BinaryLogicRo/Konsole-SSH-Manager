#include "sessionfailurelistener.h"
#include "logging.h"

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>

#include <sys/socket.h>
#include <unistd.h>

SessionFailureListener::SessionFailureListener(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &SessionFailureListener::onNewConnection);
}

bool SessionFailureListener::listen(const QString &directory)
{
    // The process id makes the name unique, so an existing file can only be
    // left over from a crashed instance.
    const QString name = QStringLiteral("%1/konsole-ssh-manager-%2.socket").arg(directory).arg(QCoreApplication::applicationPid());
    QLocalServer::removeServer(name);
    if (!m_server->listen(name)) {
        qCWarning(KSSHM_TERMINAL) << "Cannot listen for failed sessions:" << m_server->errorString();
        return false;
    }
    return true;
}

QString SessionFailureListener::serverName() const
{
    return m_server->fullServerName();
}

void SessionFailureListener::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        // The kernel records the peer when it connects; it can't be forged.
        struct ucred peer = {};
        socklen_t length = sizeof peer;
        const bool known = ::getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &peer, &length) == 0;
        socket->abort();
        socket->deleteLater();
        if (known && peer.uid == ::getuid()) {
            Q_EMIT sessionFailed(peer.pid);
        }
    }
}
