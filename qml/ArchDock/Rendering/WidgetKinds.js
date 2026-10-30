.pragma library

// Which Plasma widgets standing beside an Arch Dock (in a bar, in one of
// Plasma's own panels) the dock draws as its own, and how. One place for the
// bars and the dock.

var launcherPlugins = ["org.kde.plasma.kickoff", "org.kde.plasma.kicker",
    "org.kde.plasma.kickerdash", "org.kde.plasma.simplekickoff", "AndromedaLauncher"]

// Widgets that show only their icon on a panel: the launchers, Plasma's
// Icon and Folder widgets, and the status widgets whose icon tells how
// things are (a network's signal, the sound muted). The dock draws that
// icon as one of its own (its tile, 3D icons, holograms), following it as
// it changes; the widget stays under it, unseen, for its clicks and its
// popup. Widgets that show more than one icon (the clock, a battery's
// level, unread notifications) are not among them.
var iconPlugins = launcherPlugins.concat([
    "org.kde.plasma.icon", "org.kde.plasma.folder", "org.kde.plasma.networkmanagement",
    "org.kde.plasma.bluetooth", "org.kde.plasma.volume", "org.kde.plasma.clipboard",
    "org.kde.plasma.devicenotifier", "org.kde.kdeconnect", "org.kde.plasma.printmanager",
    "org.kde.plasma.brightness", "org.kde.kscreen"])

// The status widgets among them show a line drawing, which the dock draws
// with a body (thicker strokes) for 3D icons and holograms.
var symbolPlugins = ["org.kde.plasma.networkmanagement",
    "org.kde.plasma.bluetooth", "org.kde.plasma.volume", "org.kde.plasma.clipboard",
    "org.kde.plasma.devicenotifier", "org.kde.kdeconnect", "org.kde.plasma.printmanager",
    "org.kde.plasma.brightness", "org.kde.kscreen"]

function iconWidget(plugin) {
    return iconPlugins.indexOf(String(plugin)) >= 0
}

// A line drawing: a status widget's icon, or any symbolic icon.
function symbolWidget(plugin, icon) {
    return symbolPlugins.indexOf(String(plugin)) >= 0 || /-symbolic$/.test(String(icon || ""))
}

// Widgets that shape or colour the panel rather than show something
// (spacers, separators, Panel Colorizer): left as Plasma draws them.
function layoutWidget(plugin) {
    return /spacer|separator|colorizer/i.test(String(plugin))
}

// Every other widget shows more than an icon (the clock, a battery's level,
// unread notifications): while the dock draws 3D icons or holograms, it
// draws that widget's face as one, from a picture of it taken each second.
// The system tray's icons are its own.
function faceWidget(plugin) {
    const name = String(plugin)
    return name.length > 0 && !iconWidget(name) && !layoutWidget(name)
        && name !== "org.kde.plasma.systemtray" && name !== "org.archdock.dock"
}
