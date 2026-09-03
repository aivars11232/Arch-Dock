import QtQuick

// Runs exactly one resolved animation track.
//
// A runner animates only its own `progress` property. It never writes to a
// scene item, so two runners can never race on the same visual property; the
// controller composes their values and the host binds the result.
QtObject {
    id: runner

    property var track: ({})
    property bool active: true

    readonly property string key: String(track.key || "")
    readonly property string target: String(track.target || "")
    readonly property string property: String(track.property || "")
    readonly property string blend: String(track.blend || "replace")
    readonly property bool colorTrack: property === "tint"
    readonly property real fromValue: Number(track.from || 0)
    readonly property real toValue: Number(track.to || 0)
    readonly property int duration: Math.max(0, Number(track.duration || 0))
    readonly property int delay: Math.max(0, Number(track.delay || 0))
    readonly property string direction: String(track.direction || "normal")
    readonly property bool alternating: direction === "alternate"
    readonly property bool reversed: direction === "reverse"
    readonly property int loopCount: Number(track.repeat) === -1
        ? Animation.Infinite : Math.max(1, Number(track.repeat || 1))
    readonly property real startProgress: reversed ? 1 : 0
    readonly property real endProgress: reversed ? 0 : 1

    property real progress: startProgress

    readonly property real value: fromValue + (toValue - fromValue) * progress
    readonly property color colorValue: colorTrack
        ? runner.mixColor(track.fromColor, track.toColor, progress)
        : "transparent"

    // Displacement from the track's own origin, used for additive blending so
    // several contributions can sum without one of them redefining the origin.
    readonly property real displacement: value - fromValue

    readonly property int easingType: {
        switch (String(track.easing || "linear")) {
        case "in-quad": return Easing.InQuad
        case "out-quad": return Easing.OutQuad
        case "in-out-quad": return Easing.InOutQuad
        case "in-cubic": return Easing.InCubic
        case "out-cubic": return Easing.OutCubic
        case "in-out-cubic": return Easing.InOutCubic
        case "in-sine": return Easing.InSine
        case "out-sine": return Easing.OutSine
        case "in-out-sine": return Easing.InOutSine
        case "in-back": return Easing.InBack
        case "out-back": return Easing.OutBack
        case "in-out-back": return Easing.InOutBack
        case "out-bounce": return Easing.OutBounce
        case "out-elastic": return Easing.OutElastic
        default: return Easing.Linear
        }
    }

    function mixColor(first, second, amount) {
        const from = Qt.color(first || "transparent")
        const to = Qt.color(second || "transparent")
        const ratio = Math.max(0, Math.min(1, Number(amount) || 0))
        return Qt.rgba(from.r + (to.r - from.r) * ratio,
                       from.g + (to.g - from.g) * ratio,
                       from.b + (to.b - from.b) * ratio,
                       from.a + (to.a - from.a) * ratio)
    }

    // Stops cleanly at the track's own origin so a profile change never leaves
    // a half-applied transform behind.
    function rest() {
        motion.stop()
        progress = startProgress
    }

    property SequentialAnimation motion: SequentialAnimation {
        running: runner.active && runner.duration > 0

        PauseAnimation { duration: runner.delay }

        SequentialAnimation {
            loops: runner.loopCount

            NumberAnimation {
                target: runner
                property: "progress"
                from: runner.startProgress
                to: runner.endProgress
                duration: runner.duration
                easing.type: runner.easingType
            }

            // When the track alternates this returns it; otherwise it is a
            // zero-length no-op that holds the end value for the next loop.
            NumberAnimation {
                target: runner
                property: "progress"
                from: runner.endProgress
                to: runner.alternating
                    ? runner.startProgress : runner.endProgress
                duration: runner.alternating ? runner.duration : 0
                easing.type: runner.easingType
            }
        }
    }

    onActiveChanged: if (!active) rest()
}
