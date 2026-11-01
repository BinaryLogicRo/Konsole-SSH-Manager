#include "apptheme.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QFileInfo>
#include <QPalette>
#include <QSettings>

#include <KColorSchemeManager>

namespace
{
bool isDark(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < palette.color(QPalette::WindowText).lightness();
}
}

AppTheme::AppTheme()
    : m_systemDark(isDark(QApplication::palette()))
{
}

AppTheme::~AppTheme() = default;

bool AppTheme::restore(QSettings &settings)
{
    if (!settings.contains(QStringLiteral("appearance/darkMode"))) {
        return m_systemDark;
    }
    const bool dark = settings.value(QStringLiteral("appearance/darkMode")).toBool();
    return apply(dark) ? dark : m_systemDark;
}

bool AppTheme::setDarkMode(bool dark, QSettings &settings)
{
    if (!apply(dark)) {
        return false;
    }
    settings.setValue(QStringLiteral("appearance/darkMode"), dark);
    return true;
}

bool AppTheme::apply(bool dark)
{
    if (!m_schemes) {
        m_schemes = std::make_unique<KColorSchemeManager>();
        m_schemes->setAutosaveChanges(false);
    }

    // Match the file name, because the scheme's display name is translated.
    const QString schemeId = dark ? QStringLiteral("BreezeDark") : QStringLiteral("BreezeLight");
    QAbstractItemModel *model = m_schemes->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (QFileInfo(index.data(Qt::UserRole).toString()).baseName() == schemeId) {
            m_schemes->activateScheme(index);
            return true;
        }
    }
    return false;
}
