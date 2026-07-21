import QtQuick
import QtQuick.Controls

Item {
    id: root

    signal exitRequested()

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 2
        border.color: "#4CAF50"
        radius: 12
    }

    Button {
        text: qsTr("Exit Edit Mode")
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        onClicked: root.exitRequested()
    }

}
