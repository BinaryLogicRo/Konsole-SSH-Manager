#include "ui/mainwindow.h"

#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class FakeSessionManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KSMServerInterface")

public:
    bool shuttingDown = false;

public Q_SLOTS:
    bool isShuttingDown() const
    {
        return shuttingDown;
    }
};

class TestMainWindow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void minimizesUntilPlasmaEndsSession();
};

void TestMainWindow::minimizesUntilPlasmaEndsSession()
{
    QVERIFY2(qEnvironmentVariableIsSet("KSSHM_TEST_PRIVATE_BUS"), "Run this test through CTest to use a private D-Bus session.");
    const QTemporaryDir home(QDir::temp().filePath(QStringLiteral("XXXXXX")));
    QVERIFY(home.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, home.path());
    qputenv("XDG_RUNTIME_DIR", QFile::encodeName(home.path()));
    qunsetenv("SSH_AUTH_SOCK");

    auto bus = QDBusConnection::sessionBus();
    FakeSessionManager session;
    QVERIFY(bus.registerObject(QStringLiteral("/KSMServer"), &session, QDBusConnection::ExportAllSlots));

    MainWindow window(SshPaths::forHome(home.path(), home.filePath(QStringLiteral("backups"))));
    window.show();
    QVERIFY(!window.close()); // KDE's session manager is unavailable.
    QVERIFY(window.isMinimized());

    QVERIFY(bus.registerService(QStringLiteral("org.kde.ksmserver")));
    window.showNormal();
    QVERIFY(!window.close());
    QVERIFY(window.isMinimized());

    auto *tabs = window.findChild<QTabWidget *>();
    QVERIFY(tabs);
    tabs->addTab(new QWidget(tabs), QStringLiteral("SSH session"));
    // Dismiss a confirmation if one appears so the test fails without hanging.
    QTimer::singleShot(0, &window, [&window] {
        if (auto *message = window.findChild<QMessageBox *>()) {
            message->reject();
        }
    });
    session.shuttingDown = true;
    QVERIFY(window.close());
    QVERIFY(!window.isVisible());
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
