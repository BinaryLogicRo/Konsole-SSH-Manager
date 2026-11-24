#pragma once

#include <QObject>
#include <QString>

class QLocalServer;
class QLocalSocket;

// Lets a new launch of the app hand over to an instance that is already
// running. The first instance listens on a socket in the user's runtime
// directory. A later launch connects to it. In single instance mode, the later
// launch asks the running instance to show its window and then exits.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    enum class StartResult {
        Listening, // This launch is the first one and waits for later launches.
        HandedOver, // A running instance accepted the request; this launch exits.
        NotListening, // This launch keeps its own window but doesn't listen.
    };

    // `directory` must be private to the user, such as the runtime directory.
    explicit SingleInstance(const QString &directory, QObject *parent = nullptr);

    // Looks for a running instance. If one answers and `activateRunning` is true,
    // asks it to show its window and passes `activationToken` on. If none
    // answers, listens so that later launches find this one.
    StartResult start(bool activateRunning, const QString &activationToken);

    // Full path of the socket.
    QString serverName() const;

    // A token the desktop gave the launch to let a window take the focus.
    static bool isValidActivationToken(const QString &token);

Q_SIGNALS:
    // A later launch asks this instance to show its window.
    void activationRequested(const QString &activationToken);

private:
    static bool requestActivation(QLocalSocket &socket, const QString &activationToken);
    void onNewConnection();
    void readRequest(QLocalSocket *socket);

    QString m_directory;
    QLocalServer *m_server = nullptr;
};
