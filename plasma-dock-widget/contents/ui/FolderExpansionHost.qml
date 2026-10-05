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
    property bool showNames: true
    property var iconStyleDefinition: ({})
    property string panelEdge: "bottom"
    property var outwardNormal: ({ x: 0, y: -1 })
    // The clicked folder's drawn icon square, in its panel's scene.
    property Item folderItem: null
    property bool requested: false
    property bool interactionAllowed: true
    // Measured when the folder opens: the side of the icon the contents open
    // on, how far the folder's outward direction leans along that side, and
    // the room there. A native panel opens away from its screen edge. A free
    // panel opens the way the folder faces out of the dock and turns to the
    // other side only when the screen leaves no room for it.
    property string expansionSide: "top"
    property real expansionLean: 0
    property real expansionRoom: 420
    property size expansionScreen: Qt.size(Screen.width, Screen.height)
    readonly property bool expansionVertical: expansionSide === "top" || expansionSide === "bottom"
    readonly property real folderGap: 4
    // Where the popup is attached, relative to the folder icon. Plasma puts
    // the popup against this rectangle's edge and centres it across the
    // rectangle, so the contents' anchor lands on the folder. It is set when
    // the folder opens, from the popup's final size: a live binding would
    // move the attachment while Plasma sizes and places the popup.
    property rect placement: Qt.rect(0, 0, 1, 1)
    signal childSelected(string childId)
    objectName: "folderExpansionHost"
    type: PlasmaCore.Dialog.AppletPopup
    flags: Qt.Tool | Qt.FramelessWindowHint
    backgroundHints: PlasmaCore.Dialog.NoBackground
    color: "transparent"
    hideOnWindowDeactivate: true
    // openFolder() refuses to request a popup without its attachment.
    visible: requested && interactionAllowed
    visualParent: placementItem
    location: expansionSide === "top" ? PlasmaCore.Types.BottomEdge
        : expansionSide === "bottom" ? PlasmaCore.Types.TopEdge
        : expansionSide === "left" ? PlasmaCore.Types.RightEdge : PlasmaCore.Types.LeftEdge
    property Item placementItem: null
    // A Dialog's default property is its main item, so the attachment's
    // component is a property.
    readonly property Component placementComponent: Component { Item {} }
    onFolderItemChanged: {
        if (placementItem) placementItem.destroy()
        placementItem = folderItem ? placementComponent.createObject(folderItem) : null
    }
    Component.onDestruction: if (placementItem) placementItem.destroy()

    function chooseSide() {
        // mapToGlobal does not notify when the rendered icon moves: measure
        // the icon and its screen now, before the popup is sized and placed.
        const screen = folderItem.Screen
        const corner = folderItem.mapToGlobal(0, 0)
        const left = corner.x - screen.virtualX
        const top = corner.y - screen.virtualY
        const room = { left: left, top: top,
            right: screen.width - left - folderItem.width,
            bottom: screen.height - top - folderItem.height }
        let side = { bottom: "top", top: "bottom", left: "right", right: "left" }[panelEdge]
        let lean = 0
        if (!side) {
            const normal = outwardNormal || ({ x: 0, y: -1 })
            const x = Number(normal.x) || 0, y = Number(normal.y) || 0
            side = Math.abs(y) >= Math.abs(x) ? (y > 0 ? "bottom" : "top") : (x > 0 ? "right" : "left")
            lean = side === "top" || side === "bottom" ? x : y
            const opposite = { top: "bottom", bottom: "top", left: "right", right: "left" }[side]
            if (room[side] < 180 && room[opposite] > room[side]) side = opposite
        }
        expansionScreen = Qt.size(screen.width, screen.height)
        expansionRoom = room[side]
        expansionLean = Math.max(-1, Math.min(1, lean))
        expansionSide = side
    }
    function attach() {
        const icon = folderItem.width
        const across = expansionVertical ? content.width / 2 - content.anchorAcross
                                         : content.height / 2 - content.anchorAcross
        placement = expansionVertical
            ? Qt.rect(icon / 2 + across - 0.5, -folderGap, 1, icon + 2 * folderGap)
            : Qt.rect(-folderGap, icon / 2 + across - 0.5, icon + 2 * folderGap, 1)
        placementItem.x = placement.x
        placementItem.y = placement.y
        placementItem.width = placement.width
        placementItem.height = placement.height
    }
    function openFolder() {
        if (!interactionAllowed || !folderItem || !placementItem) return false
        chooseSide()
        attach()
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
        showNames: root.showNames
        iconStyleDefinition: root.iconStyleDefinition
        opened: root.visible
        expansionSide: root.expansionSide
        expansionLean: root.expansionLean
        expansionOrigin: {
            const anchor = root.folderItem
                ? root.folderItem.mapToGlobal(root.folderItem.width / 2,
                                              root.folderItem.height / 2)
                : Qt.point(root.x + content.width / 2, root.y + content.height)
            // Window coordinates avoid feeding the animated item's inverse
            // scale back into the origin while it unfolds.
            return Qt.point(anchor.x - root.x - content.x,
                            anchor.y - root.y - content.y)
        }
        // Size before native placement: Wayland can retain the anchor-side
        // position after a resize, even when a later move is requested.
        maximumWidth: root.expansionVertical
            ? Math.min(640, Math.max(160, root.expansionScreen.width - 40))
            : Math.min(640, Math.max(160, root.expansionRoom - root.folderGap - 20))
        maximumHeight: root.expansionVertical
            ? Math.min(420, Math.max(56, root.expansionRoom - root.folderGap - 20))
            : Math.min(420, Math.max(160, root.expansionScreen.height - 40))
        onChildSelected: childId => {
            root.childSelected(childId)
            root.closeFolder()
        }
        onDismissRequested: root.closeFolder()
    }
}
