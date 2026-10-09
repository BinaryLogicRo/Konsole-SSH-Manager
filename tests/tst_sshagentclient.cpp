#include "sshagent/sshagentclient.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

// Runs SshAgentClient against a private ssh-agent in a temporary directory,
// with throwaway keys and a fake askpass program. The user's own agent, keys
// and ~/.ssh are never used.
class TestSshAgentClient : public QObject
{
    Q_OBJECT

private:
    struct ActionResult {
        bool succeeded = false;
        QString message;
    };

    static SshAgentClient::Snapshot refresh(SshAgentClient &client, const QStringList &files)
    {
        std::optional<SshAgentClient::Snapshot> result;
        const auto connection = QObject::connect(&client, &SshAgentClient::refreshed, [&result](const SshAgentClient::Snapshot &snapshot) {
            result = snapshot;
        });
        client.refresh(files);
        // The caller checks the result, which stays empty on a timeout.
        (void)QTest::qWaitFor([&result] {
            return result.has_value();
        });
        QObject::disconnect(connection);
        return result.value_or(SshAgentClient::Snapshot{});
    }

    // Waits for the action started by `start`; nullopt if it didn't start or never finished.
    static std::optional<ActionResult> runAction(SshAgentClient &client, const std::function<bool()> &start)
    {
        std::optional<ActionResult> result;
        const auto connection = QObject::connect(&client, &SshAgentClient::actionFinished, [&result](bool succeeded, const QString &message) {
            result = ActionResult{succeeded, message};
        });
        if (start()) {
            // The caller checks the result, which stays empty on a timeout.
            (void)QTest::qWaitFor([&result] {
                return result.has_value();
            });
        }
        QObject::disconnect(connection);
        return result;
    }

    QString keyPath(const QString &name) const
    {
        return m_dir.filePath(name);
    }

    QString fingerprintOf(const QString &name) const
    {
        QFile file(keyPath(name) + QStringLiteral(".pub"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        const QList<SshAgentKey> keys = SshAgentFormat::parsePublicKeys(file.readAll());
        return keys.isEmpty() ? QString() : keys.first().fingerprint;
    }

    void generateKey(const QString &name, const QString &passphrase)
    {
        QProcess process;
        process.start(QStringLiteral("ssh-keygen"),
                      {QStringLiteral("-q"),
                       QStringLiteral("-t"),
                       QStringLiteral("ed25519"),
                       QStringLiteral("-N"),
                       passphrase,
                       QStringLiteral("-C"),
                       name,
                       QStringLiteral("-f"),
                       keyPath(name)});
        QVERIFY(process.waitForFinished(15000));
        QCOMPARE(process.exitCode(), 0);
    }

    QTemporaryDir m_dir;
    QProcess m_agent;
    QByteArray m_socket;

private Q_SLOTS:
    void initTestCase()
    {
        for (const char *program : {"ssh-agent", "ssh-add", "ssh-keygen"}) {
            if (QStandardPaths::findExecutable(QString::fromLatin1(program)).isEmpty()) {
                QSKIP("OpenSSH client programs are not installed");
            }
        }
        QVERIFY(m_dir.isValid());

        generateKey(QStringLiteral("plain"), QString());
        generateKey(QStringLiteral("locked"), QStringLiteral("test passphrase"));

        QFile askpass(keyPath(QStringLiteral("askpass")));
        QVERIFY(askpass.open(QIODevice::WriteOnly));
        // Answers only when started as the client's askpass program.
        askpass.write("#!/bin/sh\n[ \"$KONSOLE_SSH_MANAGER_ASKPASS\" = 1 ] && echo 'test passphrase'\n");
        askpass.close();
        QVERIFY(askpass.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

        m_socket = QFile::encodeName(keyPath(QStringLiteral("agent.sock")));
        m_agent.start(QStringLiteral("ssh-agent"), {QStringLiteral("-D"), QStringLiteral("-a"), QFile::decodeName(m_socket)});
        QVERIFY(m_agent.waitForStarted(5000));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(QFile::decodeName(m_socket)), 10000);

        qputenv("SSH_AUTH_SOCK", m_socket);
        qunsetenv("SSH_AGENT_PID");
        // The client's own askpass program must replace the environment's.
        qputenv("SSH_ASKPASS", "/bin/false");
    }

    void cleanupTestCase()
    {
        if (m_agent.state() != QProcess::NotRunning) {
            m_agent.kill();
            m_agent.waitForFinished(5000);
        }
    }

    void cleanup()
    {
        qputenv("SSH_AUTH_SOCK", m_socket);
    }

    void listsEmptyAgentAndFingerprints()
    {
        SshAgentClient client;
        const QString plain = keyPath(QStringLiteral("plain"));
        const QString missing = keyPath(QStringLiteral("missing"));
        const SshAgentClient::Snapshot snapshot = refresh(client, {plain, missing});

        QCOMPARE(snapshot.status, SshAgentClient::Status::Available);
        QVERIFY(snapshot.keys.isEmpty());
        QVERIFY(snapshot.files.value(plain).exists);
        QVERIFY(snapshot.files.value(plain).info);
        QCOMPARE(snapshot.files.value(plain).info->fingerprint, fingerprintOf(QStringLiteral("plain")));
        QCOMPARE(snapshot.files.value(plain).info->type, QStringLiteral("ED25519"));
        QVERIFY(!snapshot.files.value(missing).exists);
    }

    void addsAndRemovesKey()
    {
        SshAgentClient client;
        const QString plain = keyPath(QStringLiteral("plain"));
        auto result = runAction(client, [&] {
            return client.addKey(plain);
        });
        QVERIFY(result);
        QVERIFY2(result->succeeded, qPrintable(result->message));

        SshAgentClient::Snapshot snapshot = refresh(client, {plain});
        QCOMPARE(snapshot.keys.size(), 1);
        QCOMPARE(snapshot.keys.first().fingerprint, fingerprintOf(QStringLiteral("plain")));
        const QList<SshKeyEntry> entries = SshKeyList::build({}, {plain}, snapshot.files, snapshot.keys);
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().state, SshKeyEntry::State::Loaded);

        result = runAction(client, [&] {
            return client.removeKeys(entries.first().agentKeys);
        });
        QVERIFY(result);
        QVERIFY2(result->succeeded, qPrintable(result->message));
        QVERIFY(refresh(client, {}).keys.isEmpty());
    }

    void asksForPassphraseThroughAskpass()
    {
        SshAgentClient client;
        client.setAskpassProgram(keyPath(QStringLiteral("askpass")));
        auto result = runAction(client, [&] {
            return client.addKey(keyPath(QStringLiteral("locked")));
        });
        QVERIFY(result);
        QVERIFY2(result->succeeded, qPrintable(result->message));

        const SshAgentClient::Snapshot snapshot = refresh(client, {});
        QCOMPARE(snapshot.keys.size(), 1);
        QCOMPARE(snapshot.keys.first().fingerprint, fingerprintOf(QStringLiteral("locked")));

        result = runAction(client, [&] {
            return client.removeKeys({snapshot.keys.first().publicKeyLine});
        });
        QVERIFY(result && result->succeeded);
    }

    void reportsSshAddErrors()
    {
        SshAgentClient client;
        auto result = runAction(client, [&] {
            return client.addKey(keyPath(QStringLiteral("missing")));
        });
        QVERIFY(result);
        QVERIFY(!result->succeeded);
        QVERIFY(!result->message.isEmpty());

        // ssh-add refuses keys other users can read, and says why.
        const QString open = keyPath(QStringLiteral("open"));
        QVERIFY(QFile::copy(keyPath(QStringLiteral("plain")), open));
        QVERIFY(QFile::setPermissions(open, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther));
        result = runAction(client, [&] {
            return client.addKey(open);
        });
        QVERIFY(result);
        QVERIFY(!result->succeeded);
        QVERIFY2(result->message.contains(QLatin1String("UNPROTECTED PRIVATE KEY FILE")), qPrintable(result->message));
        QVERIFY(refresh(client, {}).keys.isEmpty());
    }

    void rejectsInvalidRequests()
    {
        SshAgentClient client;
        QVERIFY(!client.addKey(QStringLiteral("relative/key")));
        QVERIFY(!client.addKey(QStringLiteral("-oOption")));
        QVERIFY(!client.removeKeys({}));

        // One action at a time.
        const auto result = runAction(client, [&] {
            const bool started = client.addKey(keyPath(QStringLiteral("plain")));
            return started && client.isBusy() && !client.addKey(keyPath(QStringLiteral("plain")));
        });
        QVERIFY(result && result->succeeded);
        QVERIFY(!client.isBusy());
        const SshAgentClient::Snapshot snapshot = refresh(client, {});
        QCOMPARE(snapshot.keys.size(), 1);
        QVERIFY(runAction(client, [&] {
            return client.removeKeys({snapshot.keys.first().publicKeyLine});
        }));
    }

    void reportsMissingAgent()
    {
        SshAgentClient client;
        qunsetenv("SSH_AUTH_SOCK");
        QCOMPARE(refresh(client, {}).status, SshAgentClient::Status::NoAgent);

        qputenv("SSH_AUTH_SOCK", QFile::encodeName(keyPath(QStringLiteral("no-agent.sock"))));
        const SshAgentClient::Snapshot snapshot = refresh(client, {});
        QCOMPARE(snapshot.status, SshAgentClient::Status::Unreachable);
        QVERIFY(!snapshot.message.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSshAgentClient)
#include "tst_sshagentclient.moc"
