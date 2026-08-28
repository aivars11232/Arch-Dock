import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    id: root

    property bool vertical: false
    property bool active: false
    property bool urgent: false
    property int windowCount: 1
    property bool reducedMotion: false
    property var style: ({})

    readonly property string mode: String(style.mode || "bar").toLowerCase()
    readonly property real resolvedThickness: Math.max(1, Number(
        style.thickness || Math.max(3, Kirigami.Units.smallSpacing / 2)))
    readonly property real resolvedLength: Math.max(
        resolvedThickness,
        Number(style.length || (active ? Kirigami.Units.gridUnit
            : Math.max(5, Math.max(1, windowCount)
                * Number(style.windowStep || 4)))))
    readonly property bool pulseEnabled: urgent && !reducedMotion
        && style.pulse !== false
    readonly property color indicatorColor: urgent
        ? (style.urgentColor || Kirigami.Theme.negativeTextColor)
        : active
            ? (style.activeColor || style.color
                || Kirigami.Theme.highlightColor)
            : (style.color || Kirigami.Theme.highlightColor)

    objectName: "running-indicator"
    visible: mode !== "none"
    width: mode === "dot" ? resolvedThickness
        : vertical ? resolvedThickness : resolvedLength
    height: mode === "dot" ? resolvedThickness
        : vertical ? resolvedLength : resolvedThickness
    radius: mode === "square" ? 0 : Math.min(width, height) / 2
    color: indicatorColor

    SequentialAnimation on opacity {
        running: root.pulseEnabled
        loops: Animation.Infinite
        NumberAnimation { from: 1; to: 0.35; duration: 420 }
        NumberAnimation { from: 0.35; to: 1; duration: 420 }
    }
}
