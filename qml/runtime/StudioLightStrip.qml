import QtQuick

// A thin light like the light strips on a gaming machine: a bright core that
// fades out towards both ends, inside a soft glow of wider, fainter layers.
// Lying along the item's width, or its height when vertical. Shown only while
// `lit` and in a colour that is not fully transparent.
Item {
    id: root

    property color color: "transparent"
    property bool lit: true
    property bool vertical: false
    property real thickness: 2

    visible: lit && color.a > 0
    implicitWidth: vertical ? thickness : 120
    implicitHeight: vertical ? 120 : thickness

    Repeater {
        // From the widest, faintest glow to the core.
        model: [{spread: 9, alpha: 0.08}, {spread: 5, alpha: 0.16}, {spread: 2, alpha: 0.34}, {spread: 0, alpha: 1}]

        delegate: Rectangle {
            id: layer
            required property var modelData
            readonly property color tone: Qt.alpha(root.color, layer.modelData.alpha)

            x: root.vertical ? (root.width - width) / 2 : 0
            y: root.vertical ? 0 : (root.height - height) / 2
            width: root.vertical ? root.thickness + 2 * layer.modelData.spread : root.width
            height: root.vertical ? root.height : root.thickness + 2 * layer.modelData.spread
            radius: Math.min(width, height) / 2
            gradient: Gradient {
                orientation: root.vertical ? Gradient.Vertical : Gradient.Horizontal
                GradientStop { position: 0; color: Qt.alpha(root.color, 0) }
                GradientStop { position: 0.25; color: layer.tone }
                GradientStop { position: 0.75; color: layer.tone }
                GradientStop { position: 1; color: Qt.alpha(root.color, 0) }
            }
        }
    }
}
