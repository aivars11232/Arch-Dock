import QtQuick
import QtQuick.Window
import ArchDock.Rendering 1.0
import org.kde.plasma.core as PlasmaCore

PlasmaCore.Dialog {
    id: root
    property var snapshot: ({ status: "unavailable", entries: [] })
    property string folderTitle: ""
    property string folderLayout: "fan"
    property int folderSpeed: 260
    property string folderEasing: "outBack"
    property bool reducedMotion: false
    property var iconStyleDefinition: ({})
    property string panelEdge: "bottom"
    property var outwardNormal: ({ x: 0, y: -1 })
    property bool requested: false
    property bool interactionAllowed: true
    signal childSelected(string childId)
    objectName: "folderExpansionHost"
    type: PlasmaCore.Dialog.AppletPopup
    flags: Qt.Tool | Qt.FramelessWindowHint
    hideOnWindowDeactivate: true
    visible: requested && interactionAllowed && visualParent !== null
    location: {
        if (panelEdge === "top") return PlasmaCore.Types.TopEdge
        if (panelEdge === "bottom") return PlasmaCore.Types.BottomEdge
        if (panelEdge === "left") return PlasmaCore.Types.LeftEdge
        if (panelEdge === "right") return PlasmaCore.Types.RightEdge
        if (Math.abs(outwardNormal.x) > Math.abs(outwardNormal.y))
            return outwardNormal.x > 0 ? PlasmaCore.Types.LeftEdge : PlasmaCore.Types.RightEdge
        return outwardNormal.y > 0 ? PlasmaCore.Types.TopEdge : PlasmaCore.Types.BottomEdge
    }
    function openFolder() {
        if (!interactionAllowed || !visualParent) return false
        requested = true
        requestActivate()
        content.forceActiveFocus()
        return true
    }
    function closeFolder() { requested = false }
    onVisibleChanged: if (!visible) requested = false
    onInteractionAllowedChanged: if (!interactionAllowed) closeFolder()
    mainItem: FolderExpansion {
        id: content
        width: implicitWidth
        height: implicitHeight
        snapshot: root.snapshot
        folderTitle: root.folderTitle
        layout: root.folderLayout
        duration: root.folderSpeed
        easing: root.folderEasing
        reducedMotion: root.reducedMotion
        iconStyleDefinition: root.iconStyleDefinition
        opened: root.visible
        maximumWidth: Math.min(640, Math.max(160, Screen.width - 40))
        maximumHeight: Math.min(420, Math.max(160, Screen.height - 40))
        onChildSelected: childId => {
            root.childSelected(childId)
            root.closeFolder()
        }
        onDismissRequested: root.closeFolder()
    }
}
