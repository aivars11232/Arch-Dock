import QtQuick

// Whole-scene rotation for free panels, expressed as one angle offset.
//
// The controller owns nothing but the offset. PanelScene adds it to the
// configured layout angle and feeds the sum to LayoutEngine, so entries,
// hover targets, drop targets and popup anchors all move together because they
// are positioned by geometry rather than by a visual transform. Nothing here
// touches a host or a configuration value: the mode, speed and trigger come in
// as inputs, the pause conditions come in as observed facts, and the offset is
// runtime state that a restart simply forgets.
Item {
    id: root

    // Configuration projection.
    property string mode: "none"
    property real speedDegreesPerSecond: 12
    property string trigger: "idle"

    // The resolver's verdict for this panel. A native panel never rotates,
    // whatever its record says, because its host cannot present it.
    property bool available: false

    // Observed facts that pause the motion. A panel being configured or
    // dragged must hold still so its input stays where the user aimed, and a
    // concealed or reduced-motion panel must not spend frames turning.
    property bool hovered: false
    property bool dragActive: false
    property bool editMode: false
    property bool configuring: false
    property bool sceneConcealed: false
    property bool reducedMotion: false
    // Previews and tests can freeze the controller without changing what it
    // reports about the configuration.
    property bool animationEnabled: true

    readonly property string normalizedMode: normalizeMode(mode)
    readonly property string normalizedTrigger: normalizeTrigger(trigger)
    readonly property real direction:
        normalizedMode === "counter-clockwise" ? -1 : 1
    readonly property real safeSpeed: {
        const value = Number(speedDegreesPerSecond)
        return isFinite(value) ? Math.max(1, Math.min(180, value)) : 12
    }
    // Rotation is configured and permitted for this panel.
    readonly property bool enabled: available && normalizedMode !== "none"
    readonly property bool triggerSatisfied:
        normalizedTrigger === "hover" ? hovered : true
    // Rotation is actually advancing right now.
    readonly property bool running: enabled && animationEnabled
        && triggerSatisfied && !dragActive && !editMode && !configuring
        && !sceneConcealed && !reducedMotion

    // The current offset in degrees, kept in [0, 360).
    property real angleOffset: 0

    function normalizeMode(value) {
        const name = String(value || "").trim().toLowerCase()
        return ["none", "clockwise", "counter-clockwise"].includes(name)
            ? name : "none"
    }

    function normalizeTrigger(value) {
        const name = String(value || "").trim().toLowerCase()
        return ["idle", "hover"].includes(name) ? name : "idle"
    }

    function advance(elapsedMilliseconds) {
        const step = root.safeSpeed * root.direction
            * Math.max(0, Number(elapsedMilliseconds) || 0) / 1000
        let next = (root.angleOffset + step) % 360
        if (next < 0)
            next += 360
        root.angleOffset = next
    }

    // Rotation that is switched off, unavailable or suppressed by reduced
    // motion returns to the configured layout, so a panel never rests at an
    // arbitrary angle it cannot explain. A pause for drag, edit, hover or
    // concealment holds the current angle so resuming continues smoothly.
    onEnabledChanged: if (!enabled) angleOffset = 0
    onReducedMotionChanged: if (reducedMotion) angleOffset = 0

    width: 0
    height: 0
    visible: false

    Timer {
        id: ticker

        interval: 16
        repeat: true
        running: root.running
        onTriggered: root.advance(interval)
    }
}
