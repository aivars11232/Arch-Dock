import QtQuick
import QtQuick.Controls

Menu {
    id: root

    property bool iconContext: false
    property string selectedIconName: ""

    signal removeIconRequested(string iconName)
    signal propertiesRequested(string iconName)
    signal addWidgetsRequested()
    signal configureRequested()
    signal editModeRequested()

    popupType: Popup.Window
    closePolicy: Popup.NoAutoClose

    MenuItem {
        visible: root.iconContext
        text: qsTr("Remove Icon")
        onTriggered: root.removeIconRequested(root.selectedIconName)
    }

    MenuItem {
        visible: root.iconContext
        text: qsTr("Properties")
        onTriggered: root.propertiesRequested(root.selectedIconName)
    }

    MenuSeparator {
        visible: root.iconContext
    }

    MenuItem {
        text: qsTr("Add or Manage Widgets...")
        onTriggered: root.addWidgetsRequested()
    }

    MenuSeparator {
    }

    MenuItem {
        text: qsTr("Configure Arch Dock...")
        onTriggered: root.configureRequested()
    }

    MenuItem {
        text: qsTr("Enter Edit Mode")
        onTriggered: root.editModeRequested()
    }

}
