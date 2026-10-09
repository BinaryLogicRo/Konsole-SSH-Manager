#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

class QLocalServer;
class QLocalSocket;
class QTemporaryDir;

// The app doubles as ssh-add's askpass program. That process shows nothing
// itself: it forwards ssh-add's prompt to the app that started ssh-add, which
// asks in its own window, so the prompt belongs to the main window and stays
// in front of it. The prompt never offers to remember the passphrase (as
// ksshaskpass does).
namespace Askpass
{
// Environment variable with the socket of the app's AskpassServer. Set only for
// ssh-add, so it also tells the app that ssh-add started it as askpass.
constexpr const char *ServerVariable = "KONSOLE_SSH_MANAGER_ASKPASS";

// True when ssh-add started the app to ask for a passphrase.
bool isRequested();
// Askpass side: sends `prompt` to the app, writes its answer to stdout for
// ssh-add and returns the exit code (non-zero if the user declined).
int run(const QString &prompt);
}

// App side of Askpass::run(). Accepts prompts only from the askpass processes
// that the requester (ssh-add) starts, one at a time.
class AskpassServer : public QObject
{
    Q_OBJECT

public:
    explicit AskpassServer(QObject *parent = nullptr);
    ~AskpassServer() override;

    // Listens on a socket in a new private directory. Returns false on failure.
    bool listen();
    // Stops listening and drops any pending request.
    void close();
    // Full path of the socket, for Askpass::ServerVariable.
    QString serverName() const;
    // Only children of this process may ask.
    void setRequesterPid(qint64 pid);

    // Answers the pending request.
    void answer(const QString &passphrase);
    // Declines the pending request; ssh-add then gives up loading the key.
    void decline();

Q_SIGNALS:
    void passphraseRequested(const QString &prompt);

private:
    void onNewConnection();
    bool isFromRequester(QLocalSocket *socket) const;
    void readPrompt();

    QLocalServer *m_server = nullptr;
    std::unique_ptr<QTemporaryDir> m_directory;
    QPointer<QLocalSocket> m_request;
    qint64 m_requesterPid = 0;
    bool m_prompted = false;
};
