// Arch Dock's KWin window watcher. It runs inside KWin and reports every
// window to the Arch Dock backend over D-Bus (/WindowWatcher) when it appears,
// changes (active, minimized, caption, geometry, screen, maximized,
// fullscreen, virtual desktop, activity) and closes. The backend's window model, the task entries and the
// "hide for maximized windows" visibility mode are built on these reports.
print("Arch Dock KWin script loaded.");

function outputIndex(output) {
    if (!output || !workspace.screens)
        return -1;

    const outputName = typeof output.name === "string" ? output.name : "";
    for (let index = 0; index < workspace.screens.length; ++index) {
        const candidate = workspace.screens[index];
        if (candidate === output
                || (candidate && outputName.length > 0 && candidate.name === outputName))
            return index;
    }
    return -1;
}

// Whether the window shows on the virtual desktop and activity in use: one
// elsewhere covers nothing on the screen.
function onCurrentDesktop(window) {
    const current = workspace.currentDesktop;
    const desktops = window.desktops || [];
    const onDesktop = window.onAllDesktops === true || desktops.length === 0 || !current
        || desktops.some(function(desktop) { return desktop && desktop.id === current.id; });
    const activities = window.activities || [];
    const onActivity = activities.length === 0 || !workspace.currentActivity
        || activities.indexOf(workspace.currentActivity) >= 0;
    return onDesktop && onActivity;
}

function windowState(window) {
    const geometry = window.frameGeometry;
    const output = window.output;
    const resolvedOutputIndex = outputIndex(output);
    const maximizeMode = typeof window.maximizeMode === "number" ? window.maximizeMode : 0;
    return {
        dock: window.dock === true,
        hidden: window.hidden === true,
        x: geometry ? geometry.x : 0,
        y: geometry ? geometry.y : 0,
        width: geometry ? geometry.width : 0,
        height: geometry ? geometry.height : 0,
        screen: resolvedOutputIndex >= 0
            ? resolvedOutputIndex
            : (typeof window.screen === "number" ? window.screen : -1),
        outputName: output && typeof output.name === "string" ? output.name : "",
        maximized: typeof window.maximized === "boolean" ? window.maximized : maximizeMode === 3,
        fullScreen: window.fullScreen === true,
        onCurrentDesktop: onCurrentDesktop(window),
        canActivate: window.wantsInput === true && window.deleted !== true,
        canMinimize: window.minimizable === true && window.deleted !== true,
        canClose: window.closeable === true && window.deleted !== true
    };
}

function sendWindowAdded(window) {
    const state = windowState(window);
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
        window.minimized,
        JSON.stringify(state)
    );
}

function sendWindowUpdated(window) {
    const state = windowState(window);
    callDBus(
        "org.archdock.ArchDock",
        "/WindowWatcher",
        "local.WindowWatcher",
        "windowUpdated",
        window.internalId.toString(),
        window.desktopFileName,
        window.resourceClass,
        window.resourceName,
        window.caption,
        window.active,
        window.minimized,
        JSON.stringify(state)
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

function watchWindow(window) {
    sendWindowAdded(window);
    window.activeChanged.connect(function() { sendWindowUpdated(window); });
    window.minimizedChanged.connect(function() { sendWindowUpdated(window); });
    window.desktopFileNameChanged.connect(function() { sendWindowUpdated(window); });
    window.captionChanged.connect(function() { sendWindowUpdated(window); });
    if (window.hiddenChanged)
        window.hiddenChanged.connect(function() { sendWindowUpdated(window); });
    if (window.frameGeometryChanged)
        window.frameGeometryChanged.connect(function() { sendWindowUpdated(window); });
    if (window.outputChanged)
        window.outputChanged.connect(function() { sendWindowUpdated(window); });
    else if (window.screenChanged)
        window.screenChanged.connect(function() { sendWindowUpdated(window); });
    if (window.maximizedChanged)
        window.maximizedChanged.connect(function() { sendWindowUpdated(window); });
    else if (window.maximizeModeChanged)
        window.maximizeModeChanged.connect(function() { sendWindowUpdated(window); });
    if (window.fullScreenChanged)
        window.fullScreenChanged.connect(function() { sendWindowUpdated(window); });
    if (window.desktopsChanged)
        window.desktopsChanged.connect(function() { sendWindowUpdated(window); });
    if (window.activitiesChanged)
        window.activitiesChanged.connect(function() { sendWindowUpdated(window); });
}

// Switching the virtual desktop or activity changes which windows show.
function sendAllWindowsUpdated() {
    const all = workspace.windowList();
    for (let index = 0; index < all.length; ++index)
        sendWindowUpdated(all[index]);
}
if (workspace.currentDesktopChanged)
    workspace.currentDesktopChanged.connect(sendAllWindowsUpdated);
if (workspace.currentActivityChanged)
    workspace.currentActivityChanged.connect(sendAllWindowsUpdated);

workspace.windowRemoved.connect(sendWindowRemoved);
workspace.windowAdded.connect(watchWindow);

const windows = workspace.windowList();
for (let index = 0; index < windows.length; ++index) {
    watchWindow(windows[index]);
}
