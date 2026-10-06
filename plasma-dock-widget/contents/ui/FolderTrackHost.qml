import QtQuick
import QtQuick.Window
import ArchDock.Rendering 1.0
import org.kde.plasma.core as PlasmaCore

// "Along the dock": a folder's contents on the dock's own curve, outside it,
// in a transparent window just large enough for them and the folder they
// unfold from. The contents are drawn where the dock is drawn, relative to
// wherever the window really lands, so they follow the dock's shape, tilt and
// perspective. A click outside the window closes it, as with any popup.
PlasmaCore.Dialog {
    id: root
    property var snapshot: ({ status: "unavailable", entries: [] })
    property string folderTitle: ""
    property int folderSpeed: 260
    property string folderEasing: "outBack"
    property bool reducedMotion: false
    property bool showNames: true
    property var iconStyleDefinition: ({})
    property int iconSize: 48
    // The clicked folder's drawn icon square, and the dock's outer curve: a
    // function returning { x, y, scale } samples in the icon's parent's
    // coordinates, the folder facing the middle one.
    property Item folderItem: null
    property var trackSamples: null
    property bool requested: false
    property bool interactionAllowed: true
    // Measured when the folder opens, in screen coordinates.
    property var screenSamples: []
    property point screenOrigin: Qt.point(0, 0)
    property size windowSize: Qt.size(1, 1)
    readonly property real margin: 12
    signal childSelected(string childId)
    objectName: "folderTrackHost"
    type: PlasmaCore.Dialog.AppletPopup
    flags: Qt.Tool | Qt.FramelessWindowHint
    backgroundHints: PlasmaCore.Dialog.NoBackground
    color: "transparent"
    hideOnWindowDeactivate: true
    visible: requested && interactionAllowed
    // Plasma puts the window on top of the attachment, centred across it:
    // the attachment alone chooses where the window lands. It is the visual
    // parent only while the folder is open: a hidden dialog that follows an
    // item on the dock slows every change of the dock, such as a drop.
    location: PlasmaCore.Types.BottomEdge
    property Item placementItem: null
    // A Dialog's default property is its main item, so the attachment's
    // component is a property.
    readonly property Component placementComponent: Component { Item {} }
    function releaseAttachment() {
        visualParent = null
        if (placementItem) placementItem.destroy()
        placementItem = null
    }
    onFolderItemChanged: releaseAttachment()
    Component.onDestruction: releaseAttachment()

    function openFolder() {
        if (!interactionAllowed || !folderItem || typeof trackSamples !== "function")
            return false
        // mapToGlobal does not notify when the rendered dock moves: measure
        // the curve and the folder now.
        const scene = folderItem.parent
        const samples = trackSamples().map(function(sample) {
            const point = scene.mapToGlobal(sample.x, sample.y)
            return { x: point.x, y: point.y, scale: sample.scale }
        })
        if (samples.length < 2) return false
        const centre = folderItem.mapToGlobal(folderItem.width / 2, folderItem.height / 2)
        // The window covers every child's place and the folder it leaves.
        const layout = LayoutEngine.folderTrackLayout(samples, content.entries.length, content.pitch, 0)
        let left = centre.x - iconSize, right = centre.x + iconSize
        let top = centre.y - iconSize, bottom = centre.y + iconSize
        for (const placed of layout.entries) {
            const scale = Math.max(0.35, Number(placed.scale) || 1)
            left = Math.min(left, placed.x - content.cellWidth * scale / 2)
            right = Math.max(right, placed.x + content.cellWidth * scale / 2)
            top = Math.min(top, placed.y - iconSize * scale / 2)
            bottom = Math.max(bottom, placed.y - iconSize * scale / 2 + content.cellHeight * scale)
        }
        left -= margin; top -= margin; right += margin; bottom += margin
        screenSamples = samples
        screenOrigin = Qt.point(centre.x, centre.y)
        windowSize = Qt.size(Math.ceil(right - left), Math.ceil(bottom - top))
        // Against the top of an attachment centred under the window's bottom
        // edge: the window's top-left lands on (left, top). The attachment is
        // twice as wide as the window: Plasma moves a popup whose narrow
        // attachment leaves the middle of the screen inside it to that
        // middle (PlasmaQuick::Dialog::popupPosition).
        const corner = folderItem.mapToGlobal(0, 0)
        if (!placementItem) placementItem = placementComponent.createObject(folderItem)
        placementItem.x = left - windowSize.width / 2 - corner.x
        placementItem.y = top + windowSize.height - corner.y
        placementItem.width = 2 * windowSize.width
        placementItem.height = 1
        visualParent = placementItem
        requested = true
        requestActivate()
        content.forceActiveFocus()
        return true
    }
    function closeFolder() { requested = false }
    onVisibleChanged: if (!visible) {
        requested = false
        visualParent = null
    }
    onInteractionAllowedChanged: if (!interactionAllowed) closeFolder()
    mainItem: FolderTrack {
        id: content
        width: root.windowSize.width
        height: root.windowSize.height
        snapshot: root.snapshot
        folderTitle: root.folderTitle
        duration: root.folderSpeed
        easing: root.folderEasing
        reducedMotion: root.reducedMotion
        showNames: root.showNames
        iconStyleDefinition: root.iconStyleDefinition
        iconSize: root.iconSize
        opened: root.visible
        // The curve where the window really is: Plasma may keep it inside
        // the screen, and the contents still stand on the dock's curve.
        samples: root.screenSamples.map(function(sample) {
            return { x: sample.x - root.x, y: sample.y - root.y, scale: sample.scale }
        })
        expansionOrigin: Qt.point(root.screenOrigin.x - root.x, root.screenOrigin.y - root.y)
        onChildSelected: childId => {
            root.childSelected(childId)
            root.closeFolder()
        }
        onDismissRequested: root.closeFolder()
    }
}
