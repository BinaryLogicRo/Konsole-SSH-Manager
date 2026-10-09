#include "askpass.h"
#include "logging.h"

#include <QDataStream>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>

#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>

namespace
{
// Parent of process `pid`, or 0 if it can't be read.
qint64 parentPid(qint64 pid)
{
    QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    // "pid (comm) state ppid ...", where comm may hold spaces and parentheses.
    const QByteArray stat = file.readAll();
    const auto end = stat.lastIndexOf(')');
    if (end < 0) {
        return 0;
    }
    const QList<QByteArray> fields = stat.mid(end + 2).split(' ');
    return fields.size() > 1 ? fields.at(1).toLongLong() : 0;
}
}

bool Askpass::isRequested()
{
    return !qEnvironmentVariableIsEmpty(ServerVariable);
}

int Askpass::run(const QString &prompt)
{
    // Loading a key only asks for passphrases; refuse confirmations and notices.
    if (!qEnvironmentVariableIsEmpty("SSH_ASKPASS_PROMPT")) {
        return 1;
    }
    QLocalSocket socket;
    socket.connectToServer(QFile::decodeName(qgetenv(ServerVariable)));
    if (!socket.waitForConnected(1000)) {
        return 1;
    }
    QDataStream stream(&socket);
    stream << prompt.trimmed(); // ssh-add's prompt names the key file
    socket.flush();

    // The user may take their time; the app closes the connection to decline.
    QByteArray answer;
    while (true) {
        stream.startTransaction();
        stream >> answer;
        if (stream.commitTransaction()) {
            break;
        }
        if (!socket.waitForReadyRead(-1)) {
            return 1;
        }
    }
    answer.append('\n');
    const bool written = std::fwrite(answer.constData(), 1, size_t(answer.size()), stdout) == size_t(answer.size()) && std::fflush(stdout) == 0;
    answer.fill('\0');
    return written ? 0 : 1;
}

AskpassServer::AskpassServer(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &AskpassServer::onNewConnection);
}

AskpassServer::~AskpassServer()
{
    close();
}

bool AskpassServer::listen()
{
    close();
    // Only the current user can enter the directory, whatever the socket's permissions.
    auto directory = std::make_unique<QTemporaryDir>();
    if (!directory->isValid()) {
        return false;
    }
    if (!m_server->listen(directory->filePath(QStringLiteral("askpass.socket")))) {
        qCWarning(KSSHM_AGENT) << "Cannot listen for passphrase prompts:" << m_server->errorString();
        return false;
    }
    m_directory = std::move(directory);
    return true;
}

void AskpassServer::close()
{
    decline();
    m_server->close();
    m_directory.reset();
    m_requesterPid = 0;
}

QString AskpassServer::serverName() const
{
    return m_server->fullServerName();
}

void AskpassServer::setRequesterPid(qint64 pid)
{
    m_requesterPid = pid;
}

void AskpassServer::answer(const QString &passphrase)
{
    QLocalSocket *request = m_request;
    m_request = nullptr;
    if (!request) {
        return;
    }
    if (!m_prompted) {
        request->abort();
        return;
    }
    QByteArray bytes = passphrase.toUtf8();
    QDataStream stream(request);
    stream << bytes;
    bytes.fill('\0');
    // Closes once the answer is sent.
    request->disconnectFromServer();
}

void AskpassServer::decline()
{
    QLocalSocket *request = m_request;
    m_request = nullptr;
    if (request) {
        request->abort();
    }
}

void AskpassServer::onNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        if (m_request || !isFromRequester(socket)) {
            socket->abort();
            continue;
        }
        m_request = socket;
        m_prompted = false;
        connect(socket, &QLocalSocket::readyRead, this, &AskpassServer::readPrompt);
        readPrompt();
    }
}

bool AskpassServer::isFromRequester(QLocalSocket *socket) const
{
    // The kernel records the peer when it connects; it can't be forged.
    struct ucred peer = {};
    socklen_t length = sizeof peer;
    if (::getsockopt(int(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &peer, &length) != 0) {
        return false;
    }
    return peer.uid == ::getuid() && m_requesterPid > 0 && parentPid(peer.pid) == m_requesterPid;
}

void AskpassServer::readPrompt()
{
    if (!m_request || m_prompted) {
        return;
    }
    QDataStream stream(m_request.data());
    stream.startTransaction();
    QString prompt;
    stream >> prompt;
    if (!stream.commitTransaction()) {
        return;
    }
    m_prompted = true;
    Q_EMIT passphraseRequested(prompt);
}
