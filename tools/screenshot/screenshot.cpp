// Renders the README screenshot: the main window as a first-time user sees it.
//
// Usage: konsole-ssh-manager-screenshot <demo-ssh-dir> <work-dir> <output.png>
// (normally via `make screenshot`).
//
// Isolation: <work-dir> is recreated as a fake home whose ~/.ssh is a copy of
// <demo-ssh-dir> (fictional hosts only). HOME and all XDG directories point into
// it and the session D-Bus is dropped before Qt starts, so the user's real SSH
// configuration, settings and desktop session are never read or touched. The
// window is rendered off-screen and no SSH session is opened. The hosts that
// were loaded are printed so the output can be checked for leaks.
//
// AI agents: this runs the app's UI, which the build spec forbids without the
// user's explicit permission.

#include "mainwindow.h"
#include "sshconfig/hoststore.h"

#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <KAboutData>
#include <KLocalizedString>

#include <cstdio>

namespace
{
constexpr qreal Scale = 2.0; // output pixels per logical pixel
constexpr int Margin = 48; // logical pixels of transparent space for the shadow
constexpr int TitleBarHeight = 34; // logical
constexpr qreal Radius = 8.0;

bool copyTree(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString target = to + QLatin1Char('/') + QDir(from).relativeFilePath(source);
        if (!QDir().mkpath(QFileInfo(target).path()) || !QFile::copy(source, target)) {
            return false;
        }
    }
    return true;
}

// Box blur of the alpha channel (the shadow is plain black), `passes` times.
void blurAlpha(QImage &image, int radius, int passes)
{
    const int w = image.width();
    const int h = image.height();
    QVector<int> alpha(w * h);
    for (int y = 0; y < h; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            alpha[y * w + x] = qAlpha(line[x]);
        }
    }
    QVector<int> tmp(w * h);
    const auto boxPass = [radius](const QVector<int> &in, QVector<int> &out, int length, int count, int stride, int step) {
        for (int i = 0; i < count; ++i) {
            const int base = i * stride;
            int sum = 0;
            for (int k = -radius; k <= radius; ++k) {
                sum += in[base + qBound(0, k, length - 1) * step];
            }
            for (int j = 0; j < length; ++j) {
                out[base + j * step] = sum / (2 * radius + 1);
                sum += in[base + qMin(j + radius + 1, length - 1) * step] - in[base + qMax(j - radius, 0) * step];
            }
        }
    };
    for (int p = 0; p < passes; ++p) {
        boxPass(alpha, tmp, w, h, w, 1); // horizontal
        boxPass(tmp, alpha, h, w, 1, w); // vertical
    }
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            line[x] = qRgba(0, 0, 0, alpha[y * w + x]);
        }
    }
}

// Breeze-like title bar, as KWin draws it around the real window.
void drawTitleBar(QPainter &p, const QRectF &bar, const QString &title)
{
    const QPalette palette = QApplication::palette();
    p.fillRect(bar, palette.color(QPalette::Window).darker(104));
    p.setPen(QPen(palette.color(QPalette::Mid), 1.0 / Scale));
    p.drawLine(bar.bottomLeft() - QPointF(0, 0.5 / Scale), bar.bottomRight() - QPointF(0, 0.5 / Scale));

    const qreal iconSize = 18;
    QIcon(QStringLiteral(KSSHM_ICON_FILE)).paint(&p, QRectF(bar.left() + 10, bar.center().y() - iconSize / 2, iconSize, iconSize).toRect());

    QFont font = QApplication::font();
    p.setFont(font);
    p.setPen(palette.color(QPalette::WindowText));
    p.drawText(bar, Qt::AlignCenter, title);

    // Minimize, maximize, close glyphs.
    p.setPen(QPen(palette.color(QPalette::WindowText), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const qreal cy = bar.center().y();
    const qreal g = 4.5;
    qreal cx = bar.right() - 20;
    p.drawLine(QPointF(cx - g, cy - g), QPointF(cx + g, cy + g));
    p.drawLine(QPointF(cx - g, cy + g), QPointF(cx + g, cy - g));
    cx -= 30;
    const QPointF chevron1[] = {QPointF(cx - g, cy + g / 2), QPointF(cx, cy - g / 2), QPointF(cx + g, cy + g / 2)};
    p.drawPolyline(chevron1, 3);
    cx -= 30;
    const QPointF chevron2[] = {QPointF(cx - g, cy - g / 2), QPointF(cx, cy + g / 2), QPointF(cx + g, cy - g / 2)};
    p.drawPolyline(chevron2, 3);
}

// Window with a title bar, rounded corners and a soft drop shadow on a transparent canvas.
QImage decorate(const QImage &window, const QString &title)
{
    const QSizeF content = QSizeF(window.size()) / Scale;
    const QSizeF frame(content.width(), content.height() + TitleBarHeight);
    const QSize canvasLogical = (frame + QSizeF(2 * Margin, 2 * Margin)).toSize();
    const QRectF frameRect(QPointF(Margin, Margin), frame);
    QPainterPath shape;
    shape.addRoundedRect(frameRect, Radius, Radius);

    // Shadow: drawn at quarter resolution, blurred, then scaled up smoothly.
    constexpr int Shrink = 4;
    QImage shadow(canvasLogical / Shrink, QImage::Format_ARGB32_Premultiplied);
    shadow.fill(Qt::transparent);
    {
        QPainter p(&shadow);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(1.0 / Shrink, 1.0 / Shrink);
        p.translate(0, 10);
        p.fillPath(shape, QColor(0, 0, 0, 95));
    }
    blurAlpha(shadow, 5, 3);

    QImage canvas(canvasLogical * Scale, QImage::Format_ARGB32_Premultiplied);
    canvas.setDevicePixelRatio(Scale);
    canvas.fill(Qt::transparent);
    QPainter p(&canvas);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
    p.drawImage(QRectF(QPointF(0, 0), QSizeF(canvasLogical)), shadow);
    p.setClipPath(shape);
    drawTitleBar(p, QRectF(frameRect.topLeft(), QSizeF(frame.width(), TitleBarHeight)), title);
    p.drawImage(QRectF(frameRect.left(), frameRect.top() + TitleBarHeight, content.width(), content.height()), window);
    p.setClipping(false);
    p.setPen(QPen(QColor(0, 0, 0, 70), 1.0 / Scale));
    p.drawPath(shape);
    return canvas;
}
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        std::fprintf(stderr, "Usage: %s <demo-ssh-dir> <work-dir> <output.png>\n", argv[0]);
        return 2;
    }
    const QString demoSsh = QDir(QString::fromLocal8Bit(argv[1])).absolutePath();
    const QString home = QDir(QString::fromLocal8Bit(argv[2])).absolutePath();
    const QString output = QString::fromLocal8Bit(argv[3]);

    // Fresh fake home with only the demo SSH configuration.
    QDir(home).removeRecursively();
    if (!QDir().mkpath(home) || !copyTree(demoSsh, home + QStringLiteral("/.ssh"))) {
        std::fprintf(stderr, "Could not prepare the fake home in %s\n", argv[2]);
        return 1;
    }

    // Isolate from the real user before anything reads the environment.
    const QByteArray homeBytes = QFile::encodeName(home);
    qputenv("HOME", homeBytes);
    qputenv("XDG_CONFIG_HOME", homeBytes + "/.config");
    qputenv("XDG_DATA_HOME", homeBytes + "/.local/share");
    qputenv("XDG_CACHE_HOME", homeBytes + "/.cache");
    qputenv("XDG_STATE_HOME", homeBytes + "/.local/state");
    qunsetenv("DBUS_SESSION_BUS_ADDRESS");
    qunsetenv("SSH_AUTH_SOCK");
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QPA_PLATFORMTHEME", "kde"); // Breeze style and icons, as on Plasma

    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("konsole-ssh-manager");
    KAboutData about(QStringLiteral("konsole-ssh-manager"), i18n("Konsole SSH Manager"), QString(), QString(), KAboutLicense::Unknown);
    about.setDesktopFileName(QStringLiteral("ro.binarylogic.konsole-ssh-manager"));
    KAboutData::setApplicationData(about);

    const SshPaths paths = SshPaths::forHome(QDir::homePath(), home + QStringLiteral("/.local/share/konsole-ssh-manager/backups"));
    if (paths.homeDir != home) {
        std::fprintf(stderr, "Isolation failed: home is %s\n", qPrintable(paths.homeDir));
        return 1;
    }

    // Leak check: list exactly what the window will show.
    std::printf("Hosts shown (from %s):\n", qPrintable(paths.sshDir));
    const HostStore store(paths);
    for (const SshHost &host : store.hosts()) {
        std::printf("  %-28s %s\n", qPrintable(host.displayName()), qPrintable(QDir(home).relativeFilePath(host.sourceFile)));
    }

    MainWindow window(paths);
    window.show();

    int status = 1;
    QTimer::singleShot(1000, &app, [&] {
        QImage image(window.size() * Scale, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(Scale);
        image.fill(Qt::transparent);
        window.render(&image);
        const QImage result = decorate(image, window.windowTitle());
        status = result.save(output) ? 0 : 1;
        std::printf("%s %s (%dx%d)\n", status == 0 ? "Wrote" : "Could not write", qPrintable(output), result.width(), result.height());
        app.quit();
    });
    app.exec();
    return status;
}
