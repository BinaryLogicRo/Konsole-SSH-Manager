#include "ui/mainwindow.h"

#include <QAction>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
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
    void settingsMenuHoldsTabAndInstanceOptions();
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

void TestMainWindow::settingsMenuHoldsTabAndInstanceOptions()
{
    const QTemporaryDir home(QDir::temp().filePath(QStringLiteral("XXXXXX")));
    QVERIFY(home.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, home.path());
    qputenv("XDG_RUNTIME_DIR", QFile::encodeName(home.path()));
    qunsetenv("SSH_AUTH_SOCK");

    QVERIFY(MainWindow::isSingleInstanceEnabled()); // On until the user turns it off.
    MainWindow window(SshPaths::forHome(home.path(), home.filePath(QStringLiteral("backups"))));

    QMenu *settingsMenu = nullptr;
    const QList<QAction *> menus = window.menuBar()->actions();
    for (QAction *menuAction : menus) {
        if (menuAction->text() == QStringLiteral("&Settings")) {
            settingsMenu = menuAction->menu();
        }
    }
    QVERIFY(settingsMenu);
    QStringList texts;
    QAction *singleInstance = nullptr;
    const QList<QAction *> items = settingsMenu->actions();
    for (QAction *action : items) {
        texts.append(action->text());
        if (action->text() == QStringLiteral("&Single Instance Mode")) {
            singleInstance = action;
        }
    }
    QCOMPARE(texts, QStringList({QStringLiteral("Close Button on &Active Tab Only"), QStringLiteral("&Single Instance Mode")}));

    QVERIFY(singleInstance->isChecked());
    singleInstance->setChecked(false);
    QVERIFY(!MainWindow::isSingleInstanceEnabled()); // Saved at once for new launches.
}

QTEST_MAIN(TestMainWindow)
#include "tst_mainwindow.moc"
