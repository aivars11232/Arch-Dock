import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: root

    readonly property real menuHeight: contextMenu.implicitHeight

    onYChanged: {
        console.log("Popup Y:", y);
    }
    width: contextMenu.implicitWidth
    height: contextMenu.implicitHeight
    visible: false
    color: "transparent"
    onVisibleChanged: {
        console.log("PanelPopupWindow visible changed:", visible, "context menu visible:", contextMenu.visible);
        if (visible)
            contextMenu.open();
        else
            contextMenu.close();
    }
    // Qt.Popup grabs the next click and can hide before the panel receives it,
    // which makes a right-click immediately reopen the menu. A tool window
    // leaves panel clicks available for deterministic toggle handling.
    flags: Qt.ToolTip | Qt.FramelessWindowHint

    PanelContextMenu {
        id: contextMenu

        popupType: Popup.Item
        x: 0
        y: 0
        onVisibleChanged: {
            console.log("PanelContextMenu visible changed:", visible, "outer window visible:", root.visible);
        }
    }

}
