import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: root

    property string selectedIconName: ""
    readonly property real menuHeight: contextMenu.implicitHeight

    signal removeIconRequested(string iconName)
    signal propertiesRequested(string iconName)

    width: contextMenu.implicitWidth
    height: contextMenu.implicitHeight
    visible: false
    color: "transparent"
    flags: Qt.ToolTip | Qt.FramelessWindowHint
    onVisibleChanged: {
        if (visible)
            contextMenu.open();
        else
            contextMenu.close();
    }

    IconContextMenu {
        id: contextMenu

        selectedIconName: root.selectedIconName
        popupType: Popup.Item
        x: 0
        y: 0
        onRemoveIconRequested: function(iconName) {
            root.removeIconRequested(iconName);
        }
        onPropertiesRequested: function(iconName) {
            root.propertiesRequested(iconName);
        }
        onClosed: {
            root.hide();
        }
    }

}
