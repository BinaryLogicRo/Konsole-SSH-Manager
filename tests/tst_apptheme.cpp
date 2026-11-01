#include "ui/apptheme.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeView>
#include <QTreeWidget>
#include <QWidget>

class TestAppTheme : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void switchesLiveAndRestoresSavedMode();
    void lightensSidebarInDarkMode();
};

void TestAppTheme::switchesLiveAndRestoresSavedMode()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString settingsFile = dir.filePath(QStringLiteral("settings.ini"));
    QSettings settings(settingsFile, QSettings::IniFormat);
    const QPalette systemPalette = QApplication::palette();
    QWidget existingWidget;

    {
        AppTheme theme;
        const bool systemDark = systemPalette.color(QPalette::Window).lightness() < systemPalette.color(QPalette::WindowText).lightness();
        QCOMPARE(theme.restore(settings), systemDark);
        QCOMPARE(QApplication::palette(), systemPalette);

        QVERIFY(theme.setDarkMode(true, settings));
        QCoreApplication::processEvents();
        QVERIFY(QApplication::palette().color(QPalette::Window).lightness() < QApplication::palette().color(QPalette::WindowText).lightness());
        QCOMPARE(existingWidget.palette().color(QPalette::Window), QApplication::palette().color(QPalette::Window));
        QCOMPARE(settings.value(QStringLiteral("appearance/darkMode")).toBool(), true);

        QVERIFY(theme.setDarkMode(false, settings));
        QCoreApplication::processEvents();
        QVERIFY(QApplication::palette().color(QPalette::Window).lightness() > QApplication::palette().color(QPalette::WindowText).lightness());
        QCOMPARE(existingWidget.palette().color(QPalette::Window), QApplication::palette().color(QPalette::Window));
        QCOMPARE(settings.value(QStringLiteral("appearance/darkMode")).toBool(), false);

        QVERIFY(theme.setDarkMode(true, settings));
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
    }

    QApplication::setPalette(systemPalette);
    QSettings reopened(settingsFile, QSettings::IniFormat);
    AppTheme restartedTheme;
    QCOMPARE(restartedTheme.restore(reopened), true);
    QVERIFY(QApplication::palette().color(QPalette::Window).lightness() < QApplication::palette().color(QPalette::WindowText).lightness());
    QApplication::setPalette(systemPalette);
}

void TestAppTheme::lightensSidebarInDarkMode()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings settings(dir.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
    const QPalette systemPalette = QApplication::palette();
    AppTheme theme;
    QVERIFY(theme.setDarkMode(true, settings));

    QWidget sidebar;
    QTreeView hosts(&sidebar);
    QTreeWidget agentKeys(&sidebar);
    const QPalette darkPalette = QApplication::palette();
    AppTheme::applySidebarPalette(&sidebar, true);
    QVERIFY(sidebar.palette().color(QPalette::Window).lightness() > darkPalette.color(QPalette::Window).lightness());
    QVERIFY(hosts.viewport()->palette().color(QPalette::Base).lightness() > darkPalette.color(QPalette::Base).lightness());
    QVERIFY(agentKeys.viewport()->palette().color(QPalette::Base).lightness() > darkPalette.color(QPalette::Base).lightness());

    QVERIFY(theme.setDarkMode(false, settings));
    AppTheme::applySidebarPalette(&sidebar, false);
    QCOMPARE(sidebar.palette().color(QPalette::Window), QApplication::palette().color(QPalette::Window));
    QCOMPARE(hosts.viewport()->palette().color(QPalette::Base), QApplication::palette().color(QPalette::Base));
    QCOMPARE(agentKeys.viewport()->palette().color(QPalette::Base), QApplication::palette().color(QPalette::Base));
    QApplication::setPalette(systemPalette);
}

QTEST_MAIN(TestAppTheme)
#include "tst_apptheme.moc"
