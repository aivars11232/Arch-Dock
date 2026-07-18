import QtQuick
import QtQuick.Window

Window {
    id: root

    width: 800
    height: 200
    visible: true
    color: "transparent"
    flags: Qt.FramelessWindowHint

    Panel {
        anchors.centerIn: parent
    }
}
