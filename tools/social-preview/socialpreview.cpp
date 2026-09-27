// Renders the GitHub social preview image: the app icon next to the app name.
//
// Usage: konsole-ssh-manager-social-preview <icon.svg> <output.png> [scale]
// (normally via `make social-preview`). The image is 640x320 logical pixels, the
// size GitHub recommends; `scale` multiplies it (2 gives 1280x640).
//
// Only draws with QPainter; the app itself is not started and no SSH
// configuration is read. The prompt shows a fictional host.

#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <cstdio>

namespace
{
constexpr int Width = 640; // logical
constexpr int Height = 320; // logical
constexpr qreal TextLeft = 232;
constexpr qreal TextRight = Width - 40; // keeps text clear of GitHub's edge cropping

QFont makeFont(const QString &family, QFont::StyleHint hint, int pixelSize, QFont::Weight weight = QFont::Normal)
{
    QFont font(family);
    font.setStyleHint(hint);
    font.setPixelSize(pixelSize);
    font.setWeight(weight);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

// Breeze dark gradient with a faint dot grid.
void drawBackground(QPainter &p)
{
    QLinearGradient gradient(0, 0, 0, Height);
    gradient.setColorAt(0, QColor(QStringLiteral("#2a2e32")));
    gradient.setColorAt(1, QColor(QStringLiteral("#1b1e20")));
    p.fillRect(QRectF(0, 0, Width, Height), gradient);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(252, 252, 252, 14));
    for (int y = 12; y < Height; y += 20) {
        for (int x = 12; x < Width; x += 20) {
            p.drawEllipse(QPointF(x, y), 1.0, 1.0);
        }
    }

    // Blue to green bar along the bottom edge, the icon's two accent colors.
    QLinearGradient bar(0, 0, Width, 0);
    bar.setColorAt(0, QColor(QStringLiteral("#3daee9")));
    bar.setColorAt(1, QColor(QStringLiteral("#2ecc71")));
    p.fillRect(QRectF(0, Height - 4, Width, 4), bar);
}

// The app icon over a soft Breeze-blue glow.
void drawIcon(QPainter &p, const QString &iconFile)
{
    const QRectF rect(52, 84, 152, 152);
    constexpr qreal GlowRadius = 130;
    QRadialGradient glow(rect.center(), GlowRadius);
    glow.setColorAt(0, QColor(61, 174, 233, 70));
    glow.setColorAt(1, QColor(61, 174, 233, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(glow);
    p.drawEllipse(rect.center(), GlowRadius, GlowRadius);

    QIcon(iconFile).paint(&p, rect.toRect());
}

// App name, accent underline and tagline.
void drawTitle(QPainter &p)
{
    const QString title = QStringLiteral("Konsole SSH Manager");
    // Largest size that fits, in case the fallback font is wider than Noto Sans.
    int size = 36;
    QFont font = makeFont(QStringLiteral("Noto Sans"), QFont::SansSerif, size, QFont::Bold);
    while (size > 20 && QFontMetricsF(font).horizontalAdvance(title) > TextRight - TextLeft) {
        font.setPixelSize(--size);
    }
    p.setFont(font);
    p.setPen(QColor(QStringLiteral("#fcfcfc")));
    p.drawText(QPointF(TextLeft, 138), title);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#3daee9")));
    p.drawRoundedRect(QRectF(TextLeft + 1, 154, 48, 4), 2, 2);

    p.setFont(makeFont(QStringLiteral("Noto Sans"), QFont::SansSerif, 16));
    p.setPen(QColor(QStringLiteral("#bdc3c7")));
    p.drawText(QRectF(TextLeft, 172, TextRight - TextLeft, 48),
               int(Qt::AlignLeft | Qt::AlignTop) | int(Qt::TextWordWrap),
               QStringLiteral("SSH host profiles and embedded Konsole terminals for KDE Plasma"));
}

// Terminal-style pill: "$ ssh prod-db_" in Konsole's default font.
void drawPrompt(QPainter &p)
{
    const QFont font = makeFont(QStringLiteral("Hack"), QFont::Monospace, 14);
    const QFontMetricsF metrics(font);
    const QString prompt = QStringLiteral("$ ");
    const QString command = QStringLiteral("ssh prod-db");
    const QString cursor = QStringLiteral("_");
    constexpr qreal Padding = 14;

    const QRectF pill(TextLeft, 232, metrics.horizontalAdvance(prompt + command + cursor) + 2 * Padding, 30);
    QPainterPath path;
    path.addRoundedRect(pill, 7, 7);
    p.fillPath(path, QColor(0, 0, 0, 90));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(252, 252, 252, 40), 1));
    p.drawPath(path);

    p.setFont(font);
    const qreal baseline = pill.center().y() + (metrics.ascent() - metrics.descent()) / 2;
    qreal x = pill.left() + Padding;
    const auto drawPart = [&](const QString &text, const QColor &color) {
        p.setPen(color);
        p.drawText(QPointF(x, baseline), text);
        x += metrics.horizontalAdvance(text);
    };
    drawPart(prompt, QColor(QStringLiteral("#2ecc71")));
    drawPart(command, QColor(QStringLiteral("#eff0f1")));
    drawPart(cursor, QColor(QStringLiteral("#3daee9")));
}
}

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        std::fprintf(stderr, "Usage: %s <icon.svg> <output.png> [scale]\n", argv[0]);
        return 2;
    }
    const QString iconFile = QString::fromLocal8Bit(argv[1]);
    const QString output = QString::fromLocal8Bit(argv[2]);
    bool scaleOk = true;
    const qreal scale = argc == 4 ? QString::fromLocal8Bit(argv[3]).toDouble(&scaleOk) : 1.0;
    if (!scaleOk || scale < 1.0 || scale > 4.0) {
        std::fprintf(stderr, "scale must be a number from 1 to 4\n");
        return 2;
    }

    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);

    if (QIcon(iconFile).isNull()) {
        std::fprintf(stderr, "Could not load the icon %s\n", argv[1]);
        return 1;
    }

    QImage image(qRound(Width * scale), qRound(Height * scale), QImage::Format_RGB32);
    {
        QPainter p(&image);
        p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
        p.scale(scale, scale);
        drawBackground(p);
        drawIcon(p, iconFile);
        drawTitle(p);
        drawPrompt(p);
    }

    const bool ok = image.save(output);
    std::printf("%s %s (%dx%d)\n", ok ? "Wrote" : "Could not write", qPrintable(output), image.width(), image.height());
    return ok ? 0 : 1;
}
