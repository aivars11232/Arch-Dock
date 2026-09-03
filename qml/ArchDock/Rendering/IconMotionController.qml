import QtQuick
import "AnimationProfileRuntime.js" as Runtime

// The single motion controller for one icon entry.
//
// It receives events, asks AnimationProfileRuntime which tracks should run,
// runs each accepted track in its own MotionTrackRunner, and publishes the
// composed value per target/property. It never writes to a scene item and never
// touches the logical input area: the host binds visual transforms to the
// published channels, so a visual transform can never move a pointer target.
Item {
    id: root

    // Profile projections bound to this entry, and the catalog used to resolve
    // reduced-motion substitutes.
    property var profiles: []
    property var catalog: ({})

    property int entryIndex: 0
    property real intensity: 1
    property real speed: 1
    property bool reducedMotion: false

    property bool hovered: false
    property bool pressed: false
    property bool running: false
    property bool urgent: false
    property bool dropActive: false
    property bool revealed: false
    // Continuous motion is pointless when nothing can be seen; the composed
    // state simply reports no active tracks.
    property bool sceneVisible: true

    readonly property var motionState: ({
        hovered: root.hovered,
        pressed: root.pressed,
        running: root.running,
        urgent: root.urgent,
        dropActive: root.dropActive,
        revealed: root.revealed,
        visible: root.sceneVisible,
        reducedMotion: root.reducedMotion,
        entryIndex: root.entryIndex,
        intensity: root.intensity,
        speed: root.speed,
        pulses: root.livePulses
    })

    readonly property var composition: root.sceneVisible
        ? Runtime.compose(root.profiles, root.motionState, root.catalog)
        : ({ tracks: [], holds: [], conflicts: [], active: [] })

    readonly property var activeTracks: composition.tracks || []
    readonly property var conflicts: composition.conflicts || []
    readonly property var holds: composition.holds || []
    readonly property var activeProfileIds: composition.active || []
    readonly property bool hasConflict: conflicts.length > 0

    // Live one-shot events, as event name -> expiry timestamp.
    property var pulseDeadlines: ({})
    readonly property var livePulses: {
        const result = ({})
        const deadlines = pulseDeadlines || ({})
        for (const name in deadlines) {
            if (deadlines.hasOwnProperty(name))
                result[name] = true
        }
        return result
    }

    // Bumped by every runner so `channels` recomputes as animations advance.
    property int runnerRevision: 0

    readonly property var channels: {
        // Reading the revision is what makes this binding follow the runners.
        const revision = root.runnerRevision
        const result = ({})
        const additive = ({})

        for (let index = 0; index < trackRunners.count; ++index) {
            const runner = trackRunners.objectAt(index)
            if (!runner || !runner.key)
                continue
            if (runner.colorTrack) {
                result[runner.key] = runner.colorValue
                continue
            }
            if (runner.blend === "add") {
                additive[runner.key] = (additive[runner.key] || 0)
                    + runner.displacement
                continue
            }
            result[runner.key] = runner.value
        }

        for (const key in additive) {
            if (!additive.hasOwnProperty(key))
                continue
            const property = key.split("/")[1] || ""
            result[key] = Runtime.restingValue(property) + additive[key]
        }

        const holdList = root.holds || []
        for (let holdIndex = 0; holdIndex < holdList.length; ++holdIndex) {
            const hold = holdList[holdIndex]
            const holdKey = hold.target + "/" + hold.property
            result[holdKey] = hold.property === "tint"
                ? hold.color : hold.value
        }
        return result
    }

    signal conflictDetected(var conflict)
    signal eventDispatched(string event)

    // Reads `channels`, so a binding on this function follows the animation.
    function channelValue(target, property) {
        const key = String(target) + "/" + String(property)
        const values = root.channels
        if (values.hasOwnProperty(key))
            return values[key]
        return Runtime.restingValue(property)
    }

    function hasChannel(target, property) {
        return root.channels.hasOwnProperty(
            String(target) + "/" + String(property))
    }

    // How long a one-shot event stays live: long enough for the profiles that
    // answer it to finish, and never zero.
    function pulseLifetime(event) {
        let longest = 0
        const list = root.profiles || []
        for (let index = 0; index < list.length; ++index) {
            const profile = list[index]
            if (!profile || Runtime.normalizeTrigger(profile.trigger) !== event)
                continue
            const total = Number(profile.totalDuration)
            longest = Math.max(longest, isFinite(total) && total > 0
                ? total : 600)
        }
        const scaled = longest / Math.max(0.05, Math.min(10, Number(root.speed) || 1))
        return Math.max(120, Math.round(scaled || 0))
    }

    // Delivers one event. State events are derived from the state properties,
    // so only discrete events are accepted here.
    function dispatch(event) {
        const name = Runtime.normalizeTrigger(event)
        if (name === "" || !Runtime.isPulseEvent(name))
            return false
        const next = ({})
        const deadlines = root.pulseDeadlines || ({})
        for (const existing in deadlines) {
            if (deadlines.hasOwnProperty(existing))
                next[existing] = deadlines[existing]
        }
        next[name] = Date.now() + pulseLifetime(name)
        root.pulseDeadlines = next
        pulseExpiry.reschedule()
        root.eventDispatched(name)
        return true
    }

    function isPulseLive(event) {
        return Boolean(root.livePulses[Runtime.normalizeTrigger(event)])
    }

    // Terminates every running track and clears pending events, leaving each
    // property at its resting value.
    function reset() {
        for (let index = 0; index < trackRunners.count; ++index) {
            const runner = trackRunners.objectAt(index)
            if (runner)
                runner.rest()
        }
        root.pulseDeadlines = ({})
        pulseExpiry.stop()
        root.runnerRevision = root.runnerRevision + 1
    }

    // A profile change must not leave a half-applied transform behind: the old
    // runners are torn down and the new set starts from its own origin.
    onProfilesChanged: reset()
    onReducedMotionChanged: reset()

    onConflictsChanged: {
        const list = root.conflicts || []
        for (let index = 0; index < list.length; ++index)
            root.conflictDetected(list[index])
    }

    width: 0
    height: 0
    visible: false

    Instantiator {
        id: trackRunners

        model: root.activeTracks
        delegate: MotionTrackRunner {
            required property var modelData

            track: modelData
            active: true
            onValueChanged: root.runnerRevision = root.runnerRevision + 1
            onColorValueChanged: root.runnerRevision = root.runnerRevision + 1
        }

        onObjectAdded: root.runnerRevision = root.runnerRevision + 1
        onObjectRemoved: root.runnerRevision = root.runnerRevision + 1
    }

    Timer {
        id: pulseExpiry

        repeat: false

        function reschedule() {
            const deadlines = root.pulseDeadlines || ({})
            const now = Date.now()
            let nearest = -1
            for (const name in deadlines) {
                if (!deadlines.hasOwnProperty(name))
                    continue
                if (nearest < 0 || deadlines[name] < nearest)
                    nearest = deadlines[name]
            }
            if (nearest < 0) {
                stop()
                return
            }
            interval = Math.max(1, nearest - now)
            restart()
        }

        onTriggered: {
            const deadlines = root.pulseDeadlines || ({})
            const now = Date.now()
            const next = ({})
            let changed = false
            for (const name in deadlines) {
                if (!deadlines.hasOwnProperty(name))
                    continue
                if (deadlines[name] > now)
                    next[name] = deadlines[name]
                else
                    changed = true
            }
            if (changed)
                root.pulseDeadlines = next
            reschedule()
        }
    }
}
