#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

// App side of SshSession::reportFailure(): the session helper of a terminal tab
// connects here when ssh fails, so that tab can offer to retry. The helper is
// recognized by the process id of the connecting peer, which the tab knows.
class SessionFailureListener : public QObject
{
    Q_OBJECT

public:
    explicit SessionFailureListener(QObject *parent = nullptr);

    // Listens on a socket in `directory` that only the current user can use.
    bool listen(const QString &directory);
    // Full path of the socket, for SshSession::ReportServerVariable.
    QString serverName() const;

Q_SIGNALS:
    void sessionFailed(qint64 helperPid);

private:
    void onNewConnection();

    QLocalServer *m_server = nullptr;
};
