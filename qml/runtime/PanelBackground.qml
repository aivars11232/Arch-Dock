import QtQuick

Rectangle {
    id: root

    property color panelColor: "#cc20242b"
    property color borderColor: "#40ffffff"
    property real cornerRadius: 24
    property real borderThickness: 1

    radius: cornerRadius
    color: panelColor
    border.width: borderThickness
    border.color: borderColor
}
