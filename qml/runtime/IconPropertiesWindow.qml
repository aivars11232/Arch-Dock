import QtQuick
import QtQuick.Window

Window {
    id: root

    property int targetRow: -1
    property string appName: ""
    property string currentIconName: ""

    width: 520
    height: 310
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    title: qsTr("Customize Arch Dock icon")

    Rectangle {
        anchors.fill: parent
        radius: 12
        border.width: 1
        border.color: "#75d9edf2"
        gradient: Gradient {
            GradientStop { position: 0; color: "#f62a3848" }
            GradientStop { position: 1; color: "#f1081018" }
        }
    }

    IconProperties {
        anchors.fill: parent
        anchors.margins: 18
        targetRow: root.targetRow
        appName: root.appName
        currentIconName: root.currentIconName
        onCloseRequested: root.close()
    }

}
