// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.layershell as LayerShell

// A bar's widget window: Plasma's own widget explorer page, held for the
// bar the way Plasma's desktop holds it for a panel (views/Desktop.qml of
// Plasma's desktop shell, plasma-workspace, GPL-2.0-or-later). Opened for
// the bar, it lists the widgets that fit a panel, spacers among them, to
// drag onto the bar or add at its end, and Plasma's own edit mode stays off.
Item {
    id: root

    // The bar the widgets are for, Plasma's explorer page, and the screen
    // the window stands on, at its left side from the top.
    property QtObject containment
    property url explorerUrl
    property real screenHeight: 0
    // What Plasma's explorer page keeps in the view holding it: its list of
    // widgets, and the widgets the user asked to uninstall.
    property QtObject widgetExplorer
    readonly property bool shown: sidePanel.visible
    readonly property real shownWidth: sidePanel.width
    signal closed()

    function close() {
        sidePanel.visible = false
    }

    Timer {
        id: pendingUninstallTimer
        property list<string> applets: []
        function uninstall() {
            for (const applet of applets)
                root.widgetExplorer.uninstall(applet)
            applets = []
            if (!sidePanel.visible && root.widgetExplorer) {
                root.widgetExplorer.destroy()
                root.widgetExplorer = null
            }
        }
        interval: 60000
        onTriggered: uninstall()
    }

    PlasmaCore.Dialog {
        id: sidePanel
        location: PlasmaCore.Types.LeftEdge
        type: PlasmaCore.Dialog.Dock
        flags: Qt.WindowStaysOnTopHint
        // As Plasma's: it goes when another window is clicked, but not while
        // a widget is dragged out of it.
        hideOnWindowDeactivate: !sidePanelStack.item || !sidePanelStack.item.preventWindowHide
        opacity: sidePanelStack.item ? sidePanelStack.item.opacity : 1
        outputOnly: sidePanelStack.item ? sidePanelStack.item.outputOnly : false
        LayerShell.Window.scope: "dock"
        LayerShell.Window.layer: LayerShell.Window.LayerTop
        LayerShell.Window.anchors: LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorTop
        LayerShell.Window.exclusionZone: -1
        LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityOnDemand
        LayerShell.Window.activateOnShow: true
        onVisibleChanged: {
            if (!visible)
                root.closed()
        }
        mainItem: Loader {
            id: sidePanelStack
            width: item ? item.width : 0
            height: root.screenHeight - sidePanel.margins.top - sidePanel.margins.bottom
            LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
            LayoutMirroring.childrenInherit: true
        }
    }
    Connections {
        target: sidePanelStack.item
        ignoreUnknownSignals: true
        function onClosed() {
            sidePanel.visible = false
        }
    }

    Component.onCompleted: {
        sidePanelStack.setSource(root.explorerUrl, { containment: root.containment, sidePanel: sidePanel })
        sidePanel.visible = true
    }
}
