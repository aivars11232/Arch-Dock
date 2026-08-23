const probeTag = "ARCHDOCK_VISIBILITY_PROBE";

function printable(value) {
    return String(value === undefined || value === null ? "" : value)
        .replace(/\|/g, "/")
        .replace(/[\r\n]+/g, " ");
}

function booleanValue(value) {
    return value === true ? "true" : "false";
}

function reportWindow(window) {
    const geometry = window.frameGeometry;
    const maximizeMode = typeof window.maximizeMode === "number"
        ? window.maximizeMode : 0;
    print([
        probeTag,
        printable(window.internalId),
        printable(window.resourceClass),
        printable(window.resourceName),
        printable(window.caption),
        geometry ? geometry.x : 0,
        geometry ? geometry.y : 0,
        geometry ? geometry.width : 0,
        geometry ? geometry.height : 0,
        booleanValue(window.hidden),
        booleanValue(window.active),
        booleanValue(window.fullScreen),
        maximizeMode,
        booleanValue(window.normalWindow),
        booleanValue(window.dock),
        booleanValue(window.maximized || maximizeMode === 3)
    ].join("|"));
}

print(probeTag + "_BEGIN");
const windows = workspace.windowList();
for (let index = 0; index < windows.length; ++index)
    reportWindow(windows[index]);
print(probeTag + "_END");
