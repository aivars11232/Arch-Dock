import QtQuick
import QtQuick.Window
import ArchDock.Rendering 1.0
import org.kde.plasma.core as PlasmaCore

// A free panel's folder contents on a path outside the dock, in a transparent
// window just large enough for them and the folder they unfold from
// (ADREP-TASK-003): "Along the dock" on the dock's own curve, which follows
// its shape, tilt and perspective, or a fan, an arc, a stack or a ring that
// opens from the folder along the way it faces out of the dock
// (LayoutEngine.folderShape()). The contents are drawn where they belong on
// the screen, relative to wherever the window really lands. A press beside
// them, or leaving the window, closes the folder through its closing motion.
PlasmaCore.Dialog {
    id: root
    property var snapshot: ({ status: "unavailable", entries: [] })
    property string folderTitle: ""
    // "track" (Along the dock), "fan", "arc", "stack" or "ring".
    property string folderLayout: "track"
    property int folderSpeed: 260
    property string folderEasing: "outBack"
    property bool reducedMotion: false
    property bool showNames: true
    property var iconStyleDefinition: ({})
    property int iconSize: 48
    property int fanOpening: 90
    property int stackLength: 5
    property string ringSize: "small"
    // A function returning the dock's centre in the icon's parent's
    // coordinates, for a ring the size of the panel; null where the dock has
    // no radius.
    property var dockCentre: null
    // The panel's look, which the fan's sector and the ring are drawn in.
    property string appearance: "glass"
    property string customColor: ""
    property real panelOpacity: 0.9
    property real scrollSensitivity: 1
    // The clicked folder's drawn icon square and the way it faces out of the
    // dock; the dock's outer curve, a function returning { x, y, scale }
    // samples in the icon's parent's coordinates, the folder facing the
    // middle one, `extra` pixels further out than the dock's own step when
    // given; and the centres of the dock's other icons there.
    property Item folderItem: null
    property var outwardNormal: ({ x: 0, y: -1 })
    property var trackSamples: null
    property var obstacles: null
    property bool requested: false
    property bool interactionAllowed: true
    // Measured when the folder opens, in screen coordinates.
    property var screenSamples: []
    property var screenOutline: null
    property var screenShape: null
    property point screenOrigin: Qt.point(0, 0)
    property size windowSize: Qt.size(1, 1)
    readonly property real margin: 12
    readonly property var outlineStyle: LayoutEngine.themeStyle(appearance, customColor, iconSize)
    signal childSelected(string childId)
    objectName: "folderTrackHost"
    type: PlasmaCore.Dialog.AppletPopup
    flags: Qt.Tool | Qt.FramelessWindowHint
    backgroundHints: PlasmaCore.Dialog.NoBackground
    color: "transparent"
    // A press elsewhere takes the keyboard from the folder, as Plasma's own
    // popups notice it (the contents' handler below); the folder then closes
    // through its closing motion rather than vanishing at once.
    hideOnWindowDeactivate: false
    onActiveChanged: if (!active && requested) closeFolder()
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

    function local(point) {
        return { x: point.x - root.x, y: point.y - root.y }
    }
    function openFolder() {
        if (!interactionAllowed || !folderItem) return false
        // mapToGlobal does not notify when the rendered dock moves: measure
        // the dock, the folder and the screen now.
        const scene = folderItem.parent
        const centre = folderItem.mapToGlobal(folderItem.width / 2, folderItem.height / 2)
        const screen = folderItem.Screen
        const area = { x: screen.virtualX, y: screen.virtualY, width: screen.width, height: screen.height }
        let samples = []
        let shape = null
        const rectangles = [{ x: centre.x - iconSize, y: centre.y - iconSize,
                              width: 2 * iconSize, height: 2 * iconSize }]
        if (folderLayout === "track") {
            if (typeof trackSamples !== "function") return false
            // The dock's own pitch, not the one a shape opened with before.
            screenShape = null
            screenOutline = null
            const others = typeof obstacles === "function" ? obstacles().map(function(point) {
                const mapped = scene.mapToGlobal(point.x, point.y)
                return { x: mapped.x, y: mapped.y }
            }) : []
            // Whether a child at `place` keeps clear of every other dock icon.
            function clear(place) {
                const scale = Math.max(0.35, Number(place.scale) || 1)
                const reach = iconSize / 2 + 4
                const left = place.x - content.cellWidth * scale / 2, top = place.y - iconSize * scale / 2
                return others.every(function(icon) {
                    return left >= icon.x + reach || left + content.cellWidth * scale <= icon.x - reach
                        || top >= icon.y + reach || top + content.cellHeight * scale <= icon.y - reach
                })
            }
            // The curve a step out of the dock, a little further where a
            // tilted or projected dock would bring a child onto one of its
            // icons: every place a child stands or passes keeps clear.
            let layout = null
            for (let extra = 0; extra <= 64; extra += 8) {
                samples = trackSamples(extra).map(function(sample) {
                    const point = scene.mapToGlobal(sample.x, sample.y)
                    return { x: point.x, y: point.y, scale: sample.scale }
                })
                if (samples.length < 2) return false
                layout = LayoutEngine.folderTrackLayout(samples, content.entries.length, content.pitch, 0)
                const half = LayoutEngine.folderTrackLayout(samples, content.entries.length, content.pitch, 0.5)
                const places = layout.entries.concat(half.entries).filter(function(placed) {
                    return placed.visibility > 0
                })
                if (places.every(clear)) break
            }
            for (const placed of layout.entries.concat(layout.ends || [])) {
                const scale = Math.max(0.35, Number(placed.scale) || 1)
                rectangles.push({ x: placed.x - content.cellWidth * scale / 2,
                                  y: placed.y - iconSize * scale / 2,
                                  width: content.cellWidth * scale, height: content.cellHeight * scale })
            }
        } else {
            const others = typeof obstacles === "function" ? obstacles().map(function(point) {
                const mapped = scene.mapToGlobal(point.x, point.y)
                return { x: mapped.x, y: mapped.y }
            }) : []
            // The dock's radius where the folder stands on it.
            const middle = typeof dockCentre === "function" ? dockCentre() : null
            const dock = middle ? scene.mapToGlobal(middle.x, middle.y) : null
            const panelRadius = dock ? Math.hypot(centre.x - dock.x, centre.y - dock.y) : 0
            shape = LayoutEngine.folderShape(folderLayout, {
                folder: { x: centre.x, y: centre.y }, outward: outwardNormal || ({ x: 0, y: -1 }),
                iconSize: iconSize, cellWidth: content.cellWidth, cellHeight: content.cellHeight,
                gap: 6, count: content.entries.length, fanOpening: fanOpening,
                stackLength: stackLength, ringSize: ringSize, panelRadius: panelRadius,
                obstacles: others, obstacleSize: iconSize, screen: area, screenMargin: margin,
                outlineMargin: outlineStyle.lineWidth / 2 + outlineStyle.blur })
            samples = shape.samples.map(function(point) { return { x: point.x, y: point.y, scale: 1 } })
            rectangles.push(shape.bounds)
        }
        const bounds = LayoutEngine.unitedBounds(rectangles)
        // The window stays on the screen, where Plasma puts it as asked: one
        // that reached past an edge would be moved, or flipped to the
        // folder's other side, away from where its contents stand.
        const left = Math.max(area.x, bounds.x - margin)
        const top = Math.max(area.y, bounds.y - margin)
        const right = Math.min(area.x + area.width, bounds.x + bounds.width + margin)
        const bottom = Math.min(area.y + area.height, bounds.y + bounds.height + margin)
        screenSamples = samples
        screenShape = shape
        screenOutline = shape ? shape.outline : null
        screenOrigin = Qt.point(centre.x, centre.y)
        windowSize = Qt.size(Math.max(1, Math.floor(right - left)), Math.max(1, Math.floor(bottom - top)))
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
        // Opened again while it was closing: it opens again from where it is.
        if (requested && content.closing) content.reopen()
        requested = true
        requestActivate()
        content.forceActiveFocus()
        return true
    }
    // Plays the closing motion, then hides the window.
    function closeFolder() {
        if (!requested) return
        if (visible && content.opened) content.close()
        else requested = false
    }
    onVisibleChanged: if (!visible) {
        requested = false
        visualParent = null
    }
    onInteractionAllowedChanged: if (!interactionAllowed) requested = false
    mainItem: FolderTrack {
        id: content
        width: root.windowSize.width
        height: root.windowSize.height
        shape: root.folderLayout
        snapshot: root.snapshot
        folderTitle: root.folderTitle
        duration: root.folderSpeed
        easing: root.folderEasing
        reducedMotion: root.reducedMotion
        showNames: root.showNames
        iconStyleDefinition: root.iconStyleDefinition
        iconSize: root.iconSize
        scrollSensitivity: root.scrollSensitivity
        opened: root.visible
        // The path where the window really is: Plasma may keep the window
        // inside the screen, and the contents still stand where they belong.
        samples: root.screenSamples.map(function(sample) {
            const point = root.local(sample)
            return { x: point.x, y: point.y, scale: sample.scale }
        })
        closedPath: root.screenShape !== null && root.screenShape.closed === true
        startAtFolder: root.screenShape !== null && root.screenShape.start === "start"
        pathCapacity: root.screenShape !== null ? Number(root.screenShape.capacity || 0) : 0
        pathPitch: root.screenShape !== null ? Number(root.screenShape.pitch || 0) : 0
        outline: root.screenOutline ? ({ closed: root.screenOutline.closed === true,
            points: root.screenOutline.points.map(root.local) }) : null
        outlineFilled: root.folderLayout === "fan"
        outlineStyle: root.outlineStyle
        outlineOpacity: root.panelOpacity
        shapeInfo: {
            const shape = root.screenShape
            if (!shape) return null
            return { shape: shape.shape, direction: shape.direction, turn: shape.turn,
                     apex: shape.apex ? root.local(shape.apex) : null,
                     centre: shape.centre ? root.local(shape.centre) : null,
                     radius: shape.radius || 0, opening: shape.opening || 0, span: shape.span || 0,
                     pitch: shape.pitch, stackLength: shape.stackLength || 0,
                     ringSize: shape.ringSize || "", fewer: shape.fewer || 0 }
        }
        expansionOrigin: Qt.point(root.screenOrigin.x - root.x, root.screenOrigin.y - root.y)
        onChildSelected: childId => {
            root.childSelected(childId)
            root.closeFolder()
        }
        onDismissRequested: root.closeFolder()
        onCloseFinished: root.requested = false
        onActiveFocusChanged: if (!activeFocus && root.requested && root.visible) root.closeFolder()
    }
}
