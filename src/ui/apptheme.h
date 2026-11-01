#pragma once

#include <memory>

class KColorSchemeManager;
class QSettings;
class QWidget;

// Applies the app's light or dark palette and remembers the selected mode.
// Without a saved choice, the desktop palette remains in use.
class AppTheme
{
public:
    AppTheme();
    ~AppTheme();

    bool restore(QSettings &settings);
    bool setDarkMode(bool dark, QSettings &settings);
    static void applySidebarPalette(QWidget *sidebar, bool dark);

private:
    bool apply(bool dark);

    bool m_systemDark = false;
    std::unique_ptr<KColorSchemeManager> m_schemes;
};
