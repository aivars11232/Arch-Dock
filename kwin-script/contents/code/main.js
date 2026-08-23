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

function windowState(window) {
    const geometry = window.frameGeometry;
    const output = window.output;
    const resolvedOutputIndex = outputIndex(output);
    const maximizeMode = typeof window.maximizeMode === "number" ? window.maximizeMode : 0;
    return {
        x: geometry ? geometry.x : 0,
        y: geometry ? geometry.y : 0,
        width: geometry ? geometry.width : 0,
        height: geometry ? geometry.height : 0,
        screen: resolvedOutputIndex >= 0
            ? resolvedOutputIndex
            : (typeof window.screen === "number" ? window.screen : -1),
        outputName: output && typeof output.name === "string" ? output.name : "",
        maximized: typeof window.maximized === "boolean" ? window.maximized : maximizeMode === 3,
        fullScreen: window.fullScreen === true
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
}

workspace.windowRemoved.connect(sendWindowRemoved);
workspace.windowAdded.connect(watchWindow);

const windows = workspace.windowList();
for (let index = 0; index < windows.length; ++index) {
    watchWindow(windows[index]);
}
