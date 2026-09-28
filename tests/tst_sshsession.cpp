#include "sessionfailurelistener.h"
#include "sshsession.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace
{
// Reads everything currently in a pipe after the writer closed it.
QByteArray drain(int fd)
{
    QByteArray result;
    char buffer[4096];
    ssize_t n = 0;
    while ((n = ::read(fd, buffer, sizeof buffer)) > 0) {
        result.append(buffer, int(n));
    }
    return result;
}
}

class TestSshSession : public QObject
{
    Q_OBJECT

private:
    // Creates an executable fake `ssh` in `dir` that records its arguments and
    // whether it got the report socket variable, and exits with `status`. No
    // real ssh connection is ever attempted.
    static void writeFakeSsh(const QTemporaryDir &dir, int status)
    {
        QFile script(dir.filePath(QStringLiteral("ssh")));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(QStringLiteral("#!/bin/sh\nprintf '%s\\n' \"$@\" > \"%1\"\nprintf '%s' \"${%2-unset}\" > \"%3\"\n"
                                    "echo 'fake ssh: Connection refused' >&2\nexit %4\n")
                         .arg(dir.filePath(QStringLiteral("args")), QString::fromLatin1(SshSession::ReportServerVariable), dir.filePath(QStringLiteral("env")))
                         .arg(status)
                         .toUtf8());
        script.close();
        QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    }

    // `reportServer` is what the app would put in SshSession::ReportServerVariable.
    static QProcess *startHelper(const QTemporaryDir &dir, const QStringList &arguments, QObject *parent, const QString &reportServer = QString())
    {
        auto *process = new QProcess(parent);
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("PATH"), dir.path() + QLatin1Char(':') + environment.value(QStringLiteral("PATH")));
        environment.remove(QLatin1String(SshSession::ReportServerVariable));
        if (!reportServer.isEmpty()) {
            environment.insert(QLatin1String(SshSession::ReportServerVariable), reportServer);
        }
        process->setProcessEnvironment(environment);
        process->setProcessChannelMode(QProcess::MergedChannels);
        process->start(QStringLiteral(SSH_SESSION_HELPER), arguments);
        return process;
    }

private Q_SLOTS:
    void runChildReportsExitStatus()
    {
        const SshSession::ChildResult ok = SshSession::runChild({QStringLiteral("true")});
        QVERIFY(ok.started);
        QVERIFY(ok.succeeded());
        QCOMPARE(ok.shellStatus(), 0);

        const SshSession::ChildResult failed = SshSession::runChild({QStringLiteral("false")});
        QVERIFY(failed.started);
        QVERIFY(!failed.succeeded());
        QCOMPARE(failed.exitCode, 1);
        QCOMPARE(failed.shellStatus(), 1);
    }

    void runChildReportsMissingProgram()
    {
        const SshSession::ChildResult result = SshSession::runChild({QStringLiteral("/nonexistent/konsole-ssh-manager-test")});
        QVERIFY(!result.started);
        QCOMPARE(result.shellStatus(), 127);
        QVERIFY(SshSession::failureMessage(QStringLiteral("x"), result).contains(QLatin1String("openssh-client")));
    }

    void runChildReportsSignal()
    {
        const SshSession::ChildResult result = SshSession::runChild({QStringLiteral("sh"), QStringLiteral("-c"), QStringLiteral("kill -TERM $$")});
        QVERIFY(result.started);
        QCOMPARE(result.signal, 15);
        QCOMPARE(result.shellStatus(), 128 + 15);
    }

    void failureMessages()
    {
        SshSession::ChildResult result;
        result.started = true;
        result.exitCode = 255;
        QVERIFY(SshSession::failureMessage(QStringLiteral("prod-db"), result).contains(QLatin1String("prod-db")));
        QVERIFY(SshSession::failureMessage(QStringLiteral("prod-db"), result).contains(QLatin1String("255")));
        result.exitCode = 3;
        QVERIFY(SshSession::failureMessage(QStringLiteral("prod-db"), result).contains(QLatin1String("3")));
    }

    void runWaitsForEnterOnFailure()
    {
        int input[2];
        int output[2];
        QCOMPARE(::pipe(input), 0);
        QCOMPARE(::pipe(output), 0);
        QCOMPARE(::write(input[1], "\n", 1), ssize_t(1));
        ::close(input[1]);

        const int status = SshSession::run(QStringLiteral("false"), QStringLiteral("prod-db"), input[0], output[1], QString());
        ::close(output[1]);
        const QByteArray text = drain(output[0]);
        ::close(input[0]);
        ::close(output[0]);

        QCOMPARE(status, 1);
        QVERIFY(text.contains("prod-db"));
        QVERIFY(text.contains("Press Enter"));
    }

    void runReturnsImmediatelyOnSuccess()
    {
        int input[2];
        int output[2];
        QCOMPARE(::pipe(input), 0);
        QCOMPARE(::pipe(output), 0);
        // Input stays open: if run() waited for Enter, this test would hang.
        const int status = SshSession::run(QStringLiteral("true"), QStringLiteral("prod-db"), input[0], output[1], QString());
        ::close(output[1]);
        QCOMPARE(status, 0);
        QVERIFY(drain(output[0]).isEmpty());
        for (int fd : {input[0], input[1], output[0]}) {
            ::close(fd);
        }
    }

    void helperKeepsFailedSessionOpen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeFakeSsh(dir, 255);

        QProcess *helper = startHelper(dir, {QStringLiteral("prod-db")}, this);
        QVERIFY(helper->waitForStarted());
        // It must be waiting for Enter, not exiting on its own.
        QTRY_VERIFY_WITH_TIMEOUT(helper->bytesAvailable() > 0 && helper->peek(4096).contains("Press Enter"), 5000);
        QCOMPARE(helper->state(), QProcess::Running);

        helper->write("\n");
        QVERIFY(helper->waitForFinished(5000));
        QCOMPARE(helper->exitCode(), 255);
        const QByteArray output = helper->readAll();
        QVERIFY(output.contains("fake ssh: Connection refused"));

        // ssh got the alias and nothing else.
        QFile args(dir.filePath(QStringLiteral("args")));
        QVERIFY(args.open(QIODevice::ReadOnly));
        QCOMPARE(args.readAll(), QByteArray("prod-db\n"));
    }

    void helperExitsAfterNormalLogout()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeFakeSsh(dir, 0);
        SessionFailureListener listener;
        QVERIFY(listener.listen(dir.path()));
        QSignalSpy failed(&listener, &SessionFailureListener::sessionFailed);

        QProcess *helper = startHelper(dir, {QStringLiteral("prod-db")}, this, listener.serverName());
        QVERIFY(helper->waitForFinished(5000));
        QCOMPARE(helper->exitCode(), 0);
        QVERIFY(!helper->readAll().contains("Press Enter"));
        QTest::qWait(100);
        QCOMPARE(failed.count(), 0);
    }

    void reportFailureNeedsListener()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!SshSession::reportFailure(QString()));
        QVERIFY(!SshSession::reportFailure(dir.filePath(QStringLiteral("nobody.socket"))));
    }

    void listenerIdentifiesReportingProcess()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // A socket file left over by a crashed instance with the same process id.
        const QString name = dir.filePath(QStringLiteral("konsole-ssh-manager-%1.socket").arg(QCoreApplication::applicationPid()));
        QFile stale(name);
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.close();

        SessionFailureListener listener;
        QVERIFY(listener.listen(dir.path()));
        QCOMPARE(listener.serverName(), name);
        // Nobody else may connect.
        const QFileDevice::Permissions others =
            QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
        QVERIFY(!(QFileInfo(name).permissions() & others));

        QSignalSpy failed(&listener, &SessionFailureListener::sessionFailed);
        QVERIFY(SshSession::reportFailure(listener.serverName()));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
        QCOMPARE(failed.at(0).at(0).toLongLong(), QCoreApplication::applicationPid());
    }

    void helperReportsFailureToApp()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeFakeSsh(dir, 255);
        SessionFailureListener listener;
        QVERIFY(listener.listen(dir.path()));
        QSignalSpy failed(&listener, &SessionFailureListener::sessionFailed);

        QProcess *helper = startHelper(dir, {QStringLiteral("prod-db")}, this, listener.serverName());
        QVERIFY(helper->waitForStarted());
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 5000);
        QCOMPARE(failed.at(0).at(0).toLongLong(), helper->processId());
        // Reporting doesn't end the wait for Enter.
        QTRY_VERIFY_WITH_TIMEOUT(helper->peek(4096).contains("Press Enter"), 5000);
        QCOMPARE(helper->state(), QProcess::Running);

        helper->write("\n");
        QVERIFY(helper->waitForFinished(5000));
        QCOMPARE(helper->exitCode(), 255);

        // Only the helper talks to the app; ssh doesn't see the socket.
        QFile environment(dir.filePath(QStringLiteral("env")));
        QVERIFY(environment.open(QIODevice::ReadOnly));
        QCOMPARE(environment.readAll(), QByteArray("unset"));
    }

    void helperRejectsInvalidArguments_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("none") << QStringList{};
        QTest::newRow("option injection") << QStringList{QStringLiteral("-oProxyCommand=touch /tmp/x")};
        QTest::newRow("two arguments") << QStringList{QStringLiteral("a"), QStringLiteral("b")};
        QTest::newRow("whitespace") << QStringList{QStringLiteral("a b")};
    }

    void helperRejectsInvalidArguments()
    {
        QFETCH(QStringList, arguments);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        writeFakeSsh(dir, 0);

        QProcess *helper = startHelper(dir, arguments, this);
        QVERIFY(helper->waitForFinished(5000));
        QCOMPARE(helper->exitCode(), 2);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("args")))); // ssh was never started
    }
};

QTEST_GUILESS_MAIN(TestSshSession)

#include "tst_sshsession.moc"
