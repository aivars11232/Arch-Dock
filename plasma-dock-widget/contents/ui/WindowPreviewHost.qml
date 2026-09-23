import QtQuick
import ArchDock.Rendering 1.0
import org.kde.plasma.core as PlasmaCore

PlasmaCore.Dialog {
    id: root

    property var windowEntries: []
    property string applicationTitle: ""
    property string panelEdge: "bottom"
    property var outwardNormal: ({ x: 0, y: -1 })
    property bool requested: false
    property bool anchorHovered: false
    property bool keyboardOpened: false
    property bool interactionAllowed: true
    property bool thumbnailsEnabled: true
    property Component thumbnailComponent: null
    property string thumbnailUnavailableReason: "not-probed"
    signal windowSelected(string windowId)
    signal windowActionRequested(string windowId, string action)

    objectName: "windowPreviewHost"
    // AppletPopup supplies Plasma's Wayland role above the owning dock.
    // PopupMenu only assigns an X11 type and leaves a Tool below native panels.
    type: PlasmaCore.Dialog.AppletPopup
    flags: Qt.Tool | Qt.FramelessWindowHint
    hideOnWindowDeactivate: true
    visible: requested && interactionAllowed && visualParent !== null
        && windowEntries !== null && windowEntries.length > 0
    location: {
        if (panelEdge === "top") return PlasmaCore.Types.TopEdge
        if (panelEdge === "bottom") return PlasmaCore.Types.BottomEdge
        if (panelEdge === "left") return PlasmaCore.Types.LeftEdge
        if (panelEdge === "right") return PlasmaCore.Types.RightEdge
        if (Math.abs(outwardNormal.x) > Math.abs(outwardNormal.y))
            return outwardNormal.x > 0 ? PlasmaCore.Types.LeftEdge : PlasmaCore.Types.RightEdge
        return outwardNormal.y > 0 ? PlasmaCore.Types.TopEdge : PlasmaCore.Types.BottomEdge
    }

    function openPreview(keyboard) {
        if (!interactionAllowed || !visualParent || !windowEntries || windowEntries.length === 0)
            return false
        keyboardOpened = Boolean(keyboard)
        requested = true
        if (keyboardOpened) {
            requestActivate()
            preview.forceActiveFocus()
        }
        return true
    }

    function closePreview() { requested = false }

    onVisibleChanged: {
        // Native deactivation may hide the dialog. Clear the request too so
        // the next model update cannot reopen a dismissed preview.
        if (!visible)
            requested = false
    }
    onInteractionAllowedChanged: if (!interactionAllowed) closePreview()
    onWindowEntriesChanged: if (!windowEntries || windowEntries.length === 0) closePreview()

    Component.onCompleted: {
        const candidate = Qt.createComponent(Qt.resolvedUrl("KdeWindowThumbnail.qml"),
                                             Component.PreferSynchronous, root)
        if (candidate.status === Component.Ready) {
            thumbnailComponent = candidate
            thumbnailUnavailableReason = ""
        } else {
            thumbnailUnavailableReason = candidate.errorString()
        }
    }

    mainItem: WindowPreviewPopup {
        id: preview
        width: implicitWidth
        height: implicitHeight
        applicationTitle: root.applicationTitle
        windows: root.windowEntries
        thumbnailDelegate: root.visible && root.thumbnailsEnabled ? root.thumbnailComponent : null
        onActivateRequested: windowId => {
            root.windowSelected(windowId)
            root.closePreview()
        }
        onActionRequested: (windowId, action) => root.windowActionRequested(windowId, action)
        onDismissRequested: root.closePreview()

        HoverHandler { id: popupHover }
        Timer {
            interval: 250
            running: root.visible && !root.anchorHovered && !popupHover.hovered && !root.keyboardOpened
            onTriggered: root.closePreview()
        }
    }
}
