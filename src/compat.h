#pragma once

// All Qt5/Qt6 and KF5/KF6 differences live here. Everything else in the code
// base must be version-neutral.

#include <QString>
#include <QtGlobal>

// TerminalInterface is installed by KParts under the same name in KF5 and KF6.
#include <kde_terminal_interface.h>

namespace Compat
{
// Plugin id of the Konsole KPart for the KF major version we build against.
inline QString konsolePartPluginId()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QStringLiteral("kf6/parts/konsolepart");
#else
    return QStringLiteral("kf5/parts/konsolepart");
#endif
}

// Older or unusual installs put the part directly in the plugin root
// (e.g. Konsole 22.12 on Debian 12 installs plugins/konsolepart.so).
inline QString konsolePartFallbackPluginId()
{
    return QStringLiteral("konsolepart");
}
}
