#include "singleinstance.h"
#include "logging.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QTimer>

#include <algorithm>

#include <sys/socket.h>
#include <unistd.h>

namespace
{
constexpr int TimeoutMs = 3000;
constexpr qint64 MaxRequestBytes = 1024;
constexpr int MaxTokenLength = 256;
const QByteArray ActivateCommand = QByteArrayLiteral("activate");
const QByteArray AcceptedReply = QByteArrayLiteral("ok\n");
}

SingleInstance::SingleInstance(const QString &directory, QObject *parent)
    : QObject(parent)
    , m_directory(directory)
    , m_server(new QLocalServer(this))
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
}

QString SingleInstance::serverName() const
{
    return m_directory + QStringLiteral("/konsole-ssh-manager-instance.socket");
}

SingleInstance::StartResult SingleInstance::start(bool activateRunning, const QString &activationToken)
{
    if (m_directory.isEmpty()) {
        return StartResult::NotListening;
    }
    // The lock stops two launches at the same moment from both deciding they are first.
    QLockFile lock(m_directory + QStringLiteral("/konsole-ssh-manager-instance.lock"));
    if (!lock.tryLock(TimeoutMs)) {
        qCWarning(KSSHM_APP) << "Cannot lock the single instance check, continuing without it";
    }

    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (socket.waitForConnected(TimeoutMs)) {
        if (activateRunning && requestActivation(socket, activationToken)) {
            return StartResult::HandedOver;
        }
        return StartResult::NotListening;
    }

    // No instance answers, so a file at this path is left over from a crashed instance.
    QLocalServer::removeServer(serverName());
    if (!m_server->listen(serverName())) {
        qCWarning(KSSHM_APP) << "Cannot listen for later launches:" << m_server->errorString();
        return StartResult::NotListening;
    }
    return StartResult::Listening;
}

bool SingleInstance::isValidActivationToken(const QString &token)
{
    if (token.isEmpty() || token.size() > MaxTokenLength) {
        return false;
    }
    return std::all_of(token.cbegin(), token.cend(), [](QChar c) {
        return c.unicode() > 0x20 && c.unicode() < 0x7f;
    });
}

bool SingleInstance::requestActivation(QLocalSocket &socket, const QString &activationToken)
{
    QByteArray request = ActivateCommand;
    if (isValidActivationToken(activationToken)) {
        request += ' ' + activationToken.toLatin1();
    }
    request += '\n';
    socket.write(request);
    socket.waitForBytesWritten(TimeoutMs);

    // The running instance answers only after it accepted the request. If it
    // doesn't answer, it may hang, so this launch opens its own window.
    QByteArray reply;
    while (!reply.contains('\n') && reply.size() < MaxRequestBytes && socket.waitForReadyRead(TimeoutMs)) {
        reply += socket.readAll();
    }
    return reply == AcceptedReply;
}

void SingleInstance::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        // The kernel records the peer when it connects; it can't be forged.
        struct ucred peer = {};
        socklen_t length = sizeof peer;
        const bool known = ::getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &peer, &length) == 0;
        if (!known || peer.uid != ::getuid()) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            readRequest(socket);
        });
        // A launch that never finishes its request must not keep the connection open.
        QTimer::singleShot(TimeoutMs, socket, &QLocalSocket::abort);
    }
}

void SingleInstance::readRequest(QLocalSocket *socket)
{
    if (!socket->canReadLine()) {
        if (socket->bytesAvailable() > MaxRequestBytes) {
            socket->abort();
        }
        return;
    }
    const QByteArray line = socket->readLine(MaxRequestBytes).trimmed();
    QString token;
    if (line.startsWith(QByteArray(ActivateCommand + ' '))) {
        token = QString::fromLatin1(line.mid(ActivateCommand.size() + 1));
    } else if (line != ActivateCommand) {
        socket->abort();
        return;
    }
    if (!isValidActivationToken(token)) {
        token.clear();
    }
    socket->write(AcceptedReply);
    // Sends the reply before the connection closes.
    socket->disconnectFromServer();
    Q_EMIT activationRequested(token);
}
