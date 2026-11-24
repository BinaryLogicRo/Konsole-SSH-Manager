#include "singleinstance.h"

#include <QFile>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <memory>

// Runs the running instance and a later launch in one test process. The later
// launch waits for an answer with blocking calls, so it runs in its own thread
// while the test's event loop lets the running instance answer.
class TestSingleInstance : public QObject
{
    Q_OBJECT

private:
    static SingleInstance::StartResult launch(const QString &directory, bool activateRunning, const QString &token)
    {
        SingleInstance::StartResult result = SingleInstance::StartResult::Listening;
        std::unique_ptr<QThread> thread(QThread::create([&] {
            SingleInstance later(directory);
            result = later.start(activateRunning, token);
        }));
        thread->start();
        // The caller checks the result; a launch that never finishes fails the check.
        (void)QTest::qWaitFor(
            [&thread] {
                return thread->isFinished();
            },
            10000);
        thread->wait();
        return result;
    }

private Q_SLOTS:
    void firstLaunchListens();
    void laterLaunchHandsOver();
    void laterLaunchKeepsItsWindowWhenModeIsOff();
    void replacesLeftOverSocketFile();
    void ignoresUnknownRequests();
    void validatesActivationTokens_data();
    void validatesActivationTokens();
};

void TestSingleInstance::firstLaunchListens()
{
    const QTemporaryDir dir;
    SingleInstance first(dir.path());
    QCOMPARE(first.start(true, QString()), SingleInstance::StartResult::Listening);
    QVERIFY(QFile::exists(first.serverName()));
}

void TestSingleInstance::laterLaunchHandsOver()
{
    const QTemporaryDir dir;
    SingleInstance first(dir.path());
    QCOMPARE(first.start(true, QString()), SingleInstance::StartResult::Listening);
    QSignalSpy spy(&first, &SingleInstance::activationRequested);

    QCOMPARE(launch(dir.path(), true, QStringLiteral("kwin-1234_abc")), SingleInstance::StartResult::HandedOver);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("kwin-1234_abc"));

    // An invalid token isn't passed on, but the window is still shown.
    QCOMPARE(launch(dir.path(), true, QStringLiteral("two words")), SingleInstance::StartResult::HandedOver);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toString(), QString());
}

void TestSingleInstance::laterLaunchKeepsItsWindowWhenModeIsOff()
{
    const QTemporaryDir dir;
    SingleInstance first(dir.path());
    QCOMPARE(first.start(false, QString()), SingleInstance::StartResult::Listening);
    QSignalSpy spy(&first, &SingleInstance::activationRequested);

    QCOMPARE(launch(dir.path(), false, QString()), SingleInstance::StartResult::NotListening);
    QTest::qWait(50);
    QCOMPARE(spy.count(), 0);
    QVERIFY(QFile::exists(first.serverName())); // The running instance keeps listening.
}

void TestSingleInstance::replacesLeftOverSocketFile()
{
    const QTemporaryDir dir;
    SingleInstance first(dir.path());
    QFile leftOver(first.serverName());
    QVERIFY(leftOver.open(QIODevice::WriteOnly));
    leftOver.close();

    QCOMPARE(first.start(true, QString()), SingleInstance::StartResult::Listening);
}

void TestSingleInstance::ignoresUnknownRequests()
{
    const QTemporaryDir dir;
    SingleInstance first(dir.path());
    QCOMPARE(first.start(true, QString()), SingleInstance::StartResult::Listening);
    QSignalSpy spy(&first, &SingleInstance::activationRequested);

    QLocalSocket socket;
    socket.connectToServer(first.serverName());
    QVERIFY(socket.waitForConnected(1000));
    socket.write("quit now\n");
    QVERIFY(socket.waitForBytesWritten(1000));
    QTRY_COMPARE(socket.state(), QLocalSocket::UnconnectedState);
    QCOMPARE(spy.count(), 0);
}

void TestSingleInstance::validatesActivationTokens_data()
{
    QTest::addColumn<QString>("token");
    QTest::addColumn<bool>("valid");

    QTest::newRow("typical") << QStringLiteral("kwin-7_ab.cd-EF") << true;
    QTest::newRow("empty") << QString() << false;
    QTest::newRow("space") << QStringLiteral("a b") << false;
    QTest::newRow("newline") << QStringLiteral("a\nb") << false;
    QTest::newRow("non-ASCII") << QStringLiteral("tokené") << false;
    QTest::newRow("too long") << QString(257, QLatin1Char('a')) << false;
    QTest::newRow("longest") << QString(256, QLatin1Char('a')) << true;
}

void TestSingleInstance::validatesActivationTokens()
{
    QFETCH(QString, token);
    QFETCH(bool, valid);
    QCOMPARE(SingleInstance::isValidActivationToken(token), valid);
}

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
