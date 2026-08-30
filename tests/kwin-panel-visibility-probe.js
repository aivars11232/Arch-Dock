const probeTag = "ARCHDOCK_VISIBILITY_PROBE";
const fixtureGeometryRequest =
    /^Arch Dock Visibility Fixture \[(normal|overlap):(-?\d+):(-?\d+):(\d+):(\d+)\]$/;

function printable(value) {
    return String(value === undefined || value === null ? "" : value)
        .replace(/\|/g, "/")
        .replace(/[\r\n]+/g, " ");
}

function booleanValue(value) {
    return value === true ? "true" : "false";
}

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

function applyRequestedFixtureGeometry(window) {
    const match = printable(window.caption).match(fixtureGeometryRequest);
    if (!match)
        return;

    const requestedGeometry = {
        x: parseInt(match[2], 10),
        y: parseInt(match[3], 10),
        width: parseInt(match[4], 10),
        height: parseInt(match[5], 10)
    };
    const currentGeometry = window.frameGeometry;
    if (!currentGeometry
        || currentGeometry.x !== requestedGeometry.x
        || currentGeometry.y !== requestedGeometry.y
        || currentGeometry.width !== requestedGeometry.width
        || currentGeometry.height !== requestedGeometry.height) {
        window.frameGeometry = requestedGeometry;
    }
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
        booleanValue(window.maximized || maximizeMode === 3),
        outputIndex(window.output)
    ].join("|"));
}

print(probeTag + "_BEGIN");
const windows = workspace.windowList();
for (let index = 0; index < windows.length; ++index) {
    applyRequestedFixtureGeometry(windows[index]);
    reportWindow(windows[index]);
}
print(probeTag + "_END");
