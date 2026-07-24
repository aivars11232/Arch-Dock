// Free docks are real desktop widgets. Plasma owns their geometry, movement,
// persistence, Activities integration, and Edit Mode controls.
const activityDesktops = desktopsForActivity(currentActivity());
if (activityDesktops.length > 0) {
    const desktop = activityDesktops[0];
    const size = Math.round(gridUnit * 22);
    const dock = desktop.addWidget(
        "org.archdock.dock",
        Math.round(gridUnit * 9),
        Math.round(gridUnit * 7),
        size,
        size);
    dock.currentConfigGroup = ["General"];
    dock.writeConfig("bootstrapFreeDock", true);
    dock.writeConfig("panelType", "hybrid");
    dock.reloadConfig();
}
