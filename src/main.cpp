#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QStandardPaths>

#include <KAboutData>
#include <KLocalizedString>

int main(int argc, char **argv)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 enables high-DPI scaling by default; Qt 5 needs it before QApplication exists.
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("konsole-ssh-manager");

    KAboutData about(QStringLiteral("konsole-ssh-manager"),
                     i18n("Konsole SSH Manager"),
                     QStringLiteral(KSSHM_VERSION_STRING),
                     i18n("Manage SSH hosts and open them in embedded Konsole terminals"),
                     KAboutLicense::Unknown);
    // Must match data/ro.binarylogic.konsole-ssh-manager.desktop.in so the desktop
    // (e.g. the Wayland taskbar) shows this window with the menu entry's name and icon.
    about.setDesktopFileName(QStringLiteral("ro.binarylogic.konsole-ssh-manager"));
    KAboutData::setApplicationData(about);
    // The installed icon (hicolor theme) wins; the embedded copy covers running from the build directory.
    QApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral("ro.binarylogic.konsole-ssh-manager"), QIcon(QStringLiteral(":/icons/konsole-ssh-manager.svg"))));

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.process(app);
    about.processCommandLine(&parser);

    // Backups live outside ~/.ssh/config.d so `Include config.d/*` never reads them.
    const QString backupDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/backups");

    MainWindow window(SshPaths::forHome(QDir::homePath(), backupDir));
    window.show();
    return app.exec();
}
