// Plasma reads a panel's stored settings (its alignment, offset, height,
// lengths, length mode, hiding, floating and opacity) through the screen it
// stands on whenever the panel has no window, as while that screen is
// switched off. With the screen gone the settings group it reads is not
// there, and plasmashell crashes reading it (Panel::panelConfig in Plasma's
// shell/scripting/panel.cpp, 6.8). Every Arch Dock script that reads or
// writes one of these asks first whether the panel's screen is there, the
// way Plasma itself looks it up: a panel on no screen counts as on the first.
#pragma once

#include <QString>

namespace ArchDock
{
// What a Plasma script returns when it left a panel alone because its screen
// is gone.
inline constexpr int kPlasmaPanelScreenUnavailable = -1005;

// A Plasma script condition, true while the screen a script expression names
// (a number, or a panel's `screen`) is one Plasma does not have now. A panel
// is never moved to such a screen either: it would lose its window, and its
// stored settings could not be read after.
inline QString plasmaScreenMissing(const QString &screen)
{
    return QStringLiteral("(!(screenCount > Math.max(Number(%1), 0)))").arg(screen);
}

// The same for the panel held in the script variable `panel`: true while it
// stands on a screen Plasma does not have now.
inline QString plasmaPanelScreenGone(const QString &panel = QStringLiteral("panel"))
{
    return plasmaScreenMissing(panel + QStringLiteral(".screen"));
}
}
