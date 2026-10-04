import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string shape: "rounded"
    property color fillColor: "#334155"
    property color borderColor: "#94a3b8"
    property real borderWidth: 1
    readonly property real radius: shape === "circle" ? Math.min(width, height) / 2
        : shape === "square" ? 0 : Math.min(width, height) * (shape === "squircle" ? 0.32 : 0.22)

    Rectangle {
        anchors.fill: parent
        visible: root.shape !== "hexagon"
        radius: root.radius
        color: root.fillColor
        border.color: root.borderColor
        border.width: root.borderWidth
        antialiasing: true
    }

    Shape {
        anchors.fill: parent
        visible: root.shape === "hexagon"
        // Keep the stroke inside the tile bounds, as Rectangle does.
        ShapePath {
            fillColor: root.fillColor
            strokeColor: root.borderColor
            strokeWidth: root.borderWidth
            joinStyle: ShapePath.RoundJoin
            PathPolyline {
                path: {
                    const inset = root.borderWidth / 2
                    const w = Math.max(0, root.width - 2 * inset)
                    const h = Math.max(0, root.height - 2 * inset)
                    return [Qt.point(inset + w * 0.25, inset), Qt.point(inset + w * 0.75, inset),
                        Qt.point(inset + w, inset + h / 2), Qt.point(inset + w * 0.75, inset + h),
                        Qt.point(inset + w * 0.25, inset + h), Qt.point(inset, inset + h / 2),
                        Qt.point(inset + w * 0.25, inset)]
                }
            }
        }
    }
}
