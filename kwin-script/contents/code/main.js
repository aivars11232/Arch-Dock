print("Arch Dock KWin script loaded.");

function sendWindowAdded(window) {
    callDBus(
        "org.archdock.ArchDock",
        "/WindowWatcher",
        "local.WindowWatcher",
        "windowAdded",
        window.internalId.toString(),
        window.desktopFileName,
        window.resourceClass,
        window.resourceName,
        window.caption,
        window.active,
        window.minimized
    );
}
function sendWindowRemoved(window) {
    callDBus(
        "org.archdock.ArchDock",
        "/WindowWatcher",
        "local.WindowWatcher",
        "windowRemoved",
        window.internalId.toString()
    );
}

workspace.windowRemoved.connect(sendWindowRemoved);
workspace.windowAdded.connect(sendWindowAdded);