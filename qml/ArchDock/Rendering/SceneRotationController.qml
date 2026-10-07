import QtQuick

// Continuous motion for free panels, expressed as two offsets: an angle the
// whole scene turns by, and a travel phase, in entry slots, by which the
// entries move along the panel's own path (ADREP-TASK-002). Which of them
// runs is the panel's "Continuous motion moves" choice, each at its own speed.
//
// The controller owns nothing but the offsets. PanelScene adds the angle to
// the configured layout angle and the travel to its own, and feeds both to
// LayoutEngine, so entries, hover targets, drop targets and popup anchors all
// move together because they are positioned by geometry rather than by a
// visual transform. Nothing here touches a host or a configuration value: the
// mode, speeds and trigger come in as inputs, the pause conditions come in as
// observed facts, and the offsets are runtime state that a restart forgets.
Item {
    id: root

    // Configuration projection.
    property string mode: "none"
    property real speedDegreesPerSecond: 12
    property string trigger: "idle"

    // The resolver's verdict for this panel. A native panel never rotates,
    // whatever its record says, because its host cannot present it.
    property bool available: false

    // What moves (PD-25), whether the entries have a path to travel along,
    // how fast they travel in slots per second, and the slots in their loop.
    property bool turnsPanel: true
    property bool movesItems: false
    property bool travelAvailable: false
    property real travelSpeed: 0.5
    property real travelPeriod: 0

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
    readonly property real safeTravelSpeed: {
        const value = Number(travelSpeed)
        return isFinite(value) ? Math.max(0.05, Math.min(5, value)) : 0.5
    }
    // The whole panel's turn is configured and permitted for this panel.
    enabled: available && normalizedMode !== "none" && turnsPanel
    // The entries' travel is configured and possible on this panel.
    readonly property bool travelEnabled:
        travelAvailable && normalizedMode !== "none" && movesItems
    readonly property bool triggerSatisfied:
        normalizedTrigger === "hover" ? hovered : true
    readonly property bool motionAllowed: animationEnabled
        && triggerSatisfied && !dragActive && !editMode && !configuring
        && !sceneConcealed && !reducedMotion
    // The turn and the travel are actually advancing right now.
    readonly property bool running: enabled && motionAllowed
    readonly property bool travelRunning: travelEnabled && motionAllowed

    // The current turn in degrees, kept in [0, 360).
    property real angleOffset: 0
    // The current travel in entry slots, kept in [0, travelPeriod). The scene
    // takes it over when travel stops, so the entries never jump back.
    property real travelOffset: 0

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
        const seconds = Math.max(0, Number(elapsedMilliseconds) || 0) / 1000
        if (root.running) {
            let next = (root.angleOffset + root.safeSpeed * root.direction * seconds) % 360
            if (next < 0)
                next += 360
            root.angleOffset = next
        }
        if (root.travelRunning) {
            let next = root.travelOffset + root.safeTravelSpeed * root.direction * seconds
            const period = Number(root.travelPeriod)
            if (period > 0) {
                next %= period
                if (next < 0)
                    next += period
            }
            root.travelOffset = next
        }
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
        running: root.running || root.travelRunning
        onTriggered: root.advance(interval)
    }
}
