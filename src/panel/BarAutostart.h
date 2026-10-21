#pragma once

#include <QString>

// Arch Dock starts at login by itself while it has bars of its own: they are
// shown by its bar host, which the backend starts, and no Plasma widget is
// there to wake the backend as it wakes it for its panels in Plasma. This is
// an entry in the user's autostart folder, written while there are bars and
// removed when the last one goes.
namespace ArchDock::BarAutostart
{
// $XDG_CONFIG_HOME/autostart/org.archdock.ArchDock.Bars.autostart.desktop
[[nodiscard]] QString entryPath();
// The entry: `program` started in the background, after Plasma's desktop.
[[nodiscard]] QString entryText(const QString &program);
// Writes the entry (wanted) or removes it; whether the file is as wanted.
bool sync(bool wanted, const QString &path, const QString &program, QString *error = nullptr);
}
