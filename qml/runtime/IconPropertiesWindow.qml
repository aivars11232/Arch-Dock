import QtQuick
import QtQuick.Window

Window {
    id: root

    property string selectedIconName: ""

    width: 540
    height: 640
    visible: false
    color: "transparent"
    flags: Qt.Window

    IconProperties {
        anchors.fill: parent
        iconName: root.selectedIconName
    }

}
