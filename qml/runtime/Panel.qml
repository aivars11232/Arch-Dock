import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string contentMode: "dock"
    property string panelEdge: "bottom"
    property bool popupVisible: false

    signal contextMenuRequested(real globalX, real globalY)
    signal iconContextMenuRequested(string iconName, real globalX, real globalY)
    signal popupDismissRequested()

    function removeIcon(iconName) {
        for (let index = 0; index < iconModel.count; ++index) {
            if (iconModel.get(index).iconName === iconName) {
                iconModel.remove(index);
                return ;
            }
        }
    }

    width: 600
    height: 64

    ListModel {
        id: iconModel
    }

    PanelBackground {
        anchors.fill: parent
    }

    Row {
        visible: root.contentMode === "dock"
        anchors.centerIn: parent
        spacing: 12

        Repeater {
            model: iconModel

            delegate: DockIcon {
                required property string iconName

                onClicked: {
                    root.popupDismissRequested();
                    console.log("Clicked:", iconName);
                }
                onRightClicked: function(scenePosition) {
                    const iconCenter = mapToGlobal(Qt.point(width / 2, 0));
                    root.iconContextMenuRequested(iconName, iconCenter.x, iconCenter.y);
                }
            }

        }

        Repeater {
            model: windowModel

            delegate: DockIcon {
                required property string iconName
                required property string caption

                onClicked: {
                    root.popupDismissRequested();
                    console.log("Clicked window:", caption);
                }
            }

        }

    }

    PanelContextMenu {
        id: panelContextMenu

        onAddWidgetsRequested: {
            console.log("Add or Manage Widgets...");
        }
        onConfigureRequested: {
            console.log("Configure Arch Dock...");
        }
        onEditModeRequested: {
            console.log("Enter Edit Mode");
        }
    }

    MouseArea {
        // While a menu is open, consume the next click anywhere in the panel.
        // Keeping this above the icons prevents that click from also launching
        // an item or opening a different context menu.
        anchors.fill: parent
        z: 1000
        visible: root.popupVisible
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: root.popupDismissRequested()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: function(eventPoint) {
            const globalPosition = root.mapToGlobal(eventPoint.position);
            console.log("Panel received RIGHT click:", globalPosition.x, globalPosition.y);
            root.contextMenuRequested(globalPosition.x, globalPosition.y);
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: function(eventPoint) {
            const globalPosition = root.mapToGlobal(eventPoint.position);
            console.log("Panel received LEFT click:", globalPosition.x, globalPosition.y);
            root.popupDismissRequested();
        }
    }

}
