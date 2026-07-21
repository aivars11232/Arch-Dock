import QtQuick
import QtQuick.Controls

Menu {
    id: root

    property string selectedIconName: ""

    signal removeIconRequested(string iconName)
    signal propertiesRequested(string iconName)

    MenuItem {
        text: "Remove Icon"

        onTriggered: {
            root.removeIconRequested(root.selectedIconName)
        }
    }

    MenuItem {
        text: "Properties"

        onTriggered: {
            root.propertiesRequested(root.selectedIconName)
        }
    }
}