import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: root

    readonly property int panelMargin: 16
    width: 600
    height: 64
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint

    Panel {
        id: panel

        anchors.centerIn: parent
        popupVisible: popupWindow.visible || iconPopupWindow.visible
        onContextMenuRequested: function(globalX, globalY) {
            if (popupWindow.visible || iconPopupWindow.visible) {
                root.dismissPopups();
                return ;
            }
            const panelTopLeft = panel.mapToGlobal(Qt.point(0, 0));
            popupWindow.x = globalX;
            popupWindow.y = panelTopLeft.y - popupWindow.menuHeight - 4;
            popupWindow.show();
        }
        onIconContextMenuRequested: function(iconName, globalX, globalY) {
            if (popupWindow.visible || iconPopupWindow.visible) {
                root.dismissPopups();
                return ;
            }
            const panelTopLeft = panel.mapToGlobal(Qt.point(0, 0));
            iconPopupWindow.selectedIconName = iconName;
            iconPopupWindow.x = globalX - iconPopupWindow.width / 2;
            iconPopupWindow.y = panelTopLeft.y - iconPopupWindow.menuHeight - 4;
            iconPopupWindow.show();
        }
        onPopupDismissRequested: root.dismissPopups()
    }

    function dismissPopups() {
        popupWindow.hide();
        iconPopupWindow.hide();
    }

    PanelPopupWindow {
        id: popupWindow
    }

    IconPopupWindow {
        id: iconPopupWindow

        onRemoveIconRequested: function(iconName) {
            panel.removeIcon(iconName);
        }
        onPropertiesRequested: function(iconName) {
            iconPropertiesWindow.selectedIconName = iconName;
            iconPropertiesWindow.show();
        }
    }

    IconPropertiesWindow {
        id: iconPropertiesWindow
    }

    Connections {
        function onMenuHeightChanged() {
            if (!popupWindow.visible)
                return ;

            const panelTopLeft = panel.mapToGlobal(Qt.point(0, 0));
            popupWindow.y = panelTopLeft.y - popupWindow.menuHeight - 4;
        }

        target: popupWindow
    }

    Connections {
        function onMenuHeightChanged() {
            if (!iconPopupWindow.visible)
                return ;

            const panelTopLeft = panel.mapToGlobal(Qt.point(0, 0));
            iconPopupWindow.y = panelTopLeft.y - iconPopupWindow.menuHeight - 4;
        }

        target: iconPopupWindow
    }

}
