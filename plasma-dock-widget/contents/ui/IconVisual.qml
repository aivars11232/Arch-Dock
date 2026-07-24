import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: root

    required property var entry
    required property real iconSize
    required property string tileShape
    required property string appearance
    required property bool hovered
    required property bool pressed
    required property bool showReflection
    property real glowAmount: 0

    function alphaColor(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha);
    }

    function tileRadius() {
        if (tileShape === "circle")
            return width / 2;
        if (tileShape === "square")
            return 3;
        if (tileShape === "squircle")
            return width * 0.32;
        return width * 0.22;
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: Math.max(1, root.iconSize * 0.04)
        radius: root.tileRadius()
        visible: root.appearance === "plates" || root.appearance === "platform"
                 || root.hovered || root.entry.active
        color: root.entry.active
            ? root.alphaColor(Kirigami.Theme.highlightColor, 0.28)
            : root.alphaColor(Kirigami.Theme.backgroundColor, root.hovered ? 0.42 : 0.22)
        border.width: 1
        border.color: root.alphaColor(Kirigami.Theme.textColor, root.hovered ? 0.28 : 0.12)
        scale: root.pressed ? 0.92 : 1
    }

    Kirigami.Icon {
        id: icon
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: root.iconSize * 0.72
        height: width
        source: root.entry.iconName || "application-x-executable"
        opacity: root.entry.minimized ? 0.52 : 1
    }

    Kirigami.Icon {
        anchors.horizontalCenter: icon.horizontalCenter
        anchors.top: icon.bottom
        anchors.topMargin: -root.iconSize * 0.08
        width: icon.width
        height: icon.height * 0.28
        source: icon.source
        opacity: root.showReflection ? 0.16 : 0
        transform: Scale { yScale: -0.28; origin.y: 0 }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 0.94
        height: width
        radius: width / 2
        color: "transparent"
        border.width: root.glowAmount * 3
        border.color: root.alphaColor(Kirigami.Theme.highlightColor, root.glowAmount * 0.75)
        visible: root.glowAmount > 0.01
    }
}
