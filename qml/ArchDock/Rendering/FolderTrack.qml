import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Effects
import "MotionChannels.js" as MotionChannels
import "PlatformGeometry.js" as PlatformGeometry

// A free panel's folder contents on a path: "Along the dock",
// where the dock's own track continues outside it, or the fan, arc and
// ring of LayoutEngine.folderShape(). The host traces the path, and the small
// panel drawn under it, in this item's coordinates; this item stands the
// children on it, unfolds them from the folder icon and folds them back, and
// takes the pointer, wheel and keyboard. Shared presentation only; the panel
// backend revalidates every selected ID.
Item {
    id: root
    property var snapshot: ({ status: "unavailable", entries: [] })
    property string folderTitle: ""
    // "track" (Along the dock), "fan", "arc", "ring" or "mobius".
    property string shape: "track"
    property int duration: 260
    property string easing: "outBack"
    property bool reducedMotion: false
    property bool opened: false
    property bool showNames: true
    property var iconStyleDefinition: ({})
    // The stands under the children: the panel's pedestal settings, with the
    // Pedestals page's "Folder item stands" choice as their shape.
    property var pedestalSettings: ({})
    property int iconSize: 48
    // The path as { x, y, scale } samples - an open path's children stand
    // centred on its middle sample, which the folder faces, or from its first
    // when they start at the folder - and the folder icon's centre: the
    // children unfold from there.
    property var samples: []
    // A ring folder's second circle round the first, as samples like
    // `samples`: the children the ring cannot hold go on round it.
    property var rings: []
    property bool closedPath: false
    property bool startAtFolder: false
    // At most this many children on the path at once; 0
    // is as many as fit.
    property int pathCapacity: 0
    // The distance between neighbours on the path; 0 keeps the dock's pitch.
    property real pathPitch: 0
    // A shape on a 3D panel's plane: its children are drawn at their
    // samples' scale, and neighbours stand a pitch apart at that size.
    property bool measureAsDrawn: false
    // At most this many at once on each of a ring's circles; 0 is as many
    // as fit.
    property var ringLimits: []
    // A Möbius band's path: each of its loops holds half the children,
    // none where they cross (LayoutEngine.lobedTrackLayout()).
    property bool lobedPath: false
    // A grid's cells in this item's coordinates ({ columns, rows, cells },
    // LayoutEngine.gridTrackLayout()); null for a path. The wheel moves a
    // grid a row at a time.
    property var gridCells: null
    // The small panel under the children, { points, closed } in this item's
    // coordinates, drawn in the panel's look (LayoutEngine.themeStyle()).
    property var outline: null
    // On a drawn band each child's stand has its base on the band and the
    // icon stands above it; elsewhere the icon's middle is on the path.
    property bool standsOnPath: false
    // On a tilted 3D panel, the dock's middle in these coordinates and its
    // drawn half height: a child standing above the middle (the far side)
    // carries its name above its icon, away from the ring, and stands on
    // the path by its stand's base; one below it carries its name below and
    // its icon on the path, its stand below; between, the standing point
    // glides from the one to the other across the dock's height.
    property real dockMiddleY: NaN
    property real dockHalfHeight: 0
    property bool outlineFilled: false
    property var outlineStyle: null
    property real outlineOpacity: 0.9
    // Where the shape stands - apex, centre, radius - for diagnostics.
    property var shapeInfo: null
    // Steps per wheel notch, or per pitch of touchpad travel.
    property real scrollSensitivity: 1
    property point expansionOrigin: Qt.point(width / 2, height / 2)
    // An opening runs this phase from 0 to 1 over the folder animation, a
    // closing from 1 to 0; `openingProgress` is the chosen motion at that
    // time, so a closing plays the opening backwards.
    property real openingPhase: opened ? 1 : 0
    readonly property real openingProgress: MotionChannels.folderEasing(easing, openingPhase)
    readonly property bool openingInProgress: phaseAnimation.running
    property bool closing: false
    property string selectedChildId: ""
    property bool keyboardSelection: false
    // Where the pointer last moved over a child, in the window.
    property point pointerAt: Qt.point(-1, -1)
    readonly property var entries: (snapshot.entries || []).slice(0, 48)
    // The stands under the children reach above and below an icon's cell:
    // the icon floats over its stand (the Pedestals page), and its name
    // stays clear under the stand.
    readonly property var standLayout: PlatformGeometry.pedestalLayout(pedestalSettings, iconSize)
    readonly property real cellWidth: Math.max(showNames ? 108 : iconSize, standLayout.width)
    readonly property real cellHeight: iconSize + standLayout.above + standLayout.below + nameHeight
    // Two lines of a name and its gap from the body; nothing without names.
    readonly property real nameHeight: showNames ? nameMetrics.height * 2 + 4 : 0
    // The stand's base below the cell's top: on a free panel's path the
    // icon is raised inside its cell and the stand sits about the cell's
    // middle, so the base is not the cell's bottom. Without a stand it is
    // the icon's middle.
    readonly property real standFoot: (iconSize + standLayout.above + standLayout.below) / 2
        + standLayout.standCentre + standLayout.height / 2
    // Neighbours along the dock's curve: a name's width apart, or an icon
    // and a gap.
    readonly property real pitch: pathPitch > 0 ? pathPitch
        : showNames ? cellWidth + 6 : Math.max(iconSize, standLayout.width) + 14
            + standLayout.above + standLayout.below
    // How far the wheel or keys have moved the children along the path, in
    // slots, and where a wheel step is taking them; never saved.
    property real trackOffset: 0
    property real travelTarget: 0
    // Wheel input not yet worth a whole step.
    property real wheelRemainder: 0
    readonly property var track: gridCells ? LayoutEngine.gridTrackLayout(gridCells, entries.length, trackOffset)
        : LayoutEngine.folderTrackLayout(
        samples, entries.length, pitch, trackOffset,
        { closed: closedPath, start: startAtFolder ? "start" : "centre", capacity: pathCapacity, rings: rings,
          unscaled: measureAsDrawn, ringLimits: ringLimits, lobes: lobedPath })
    // What the popup reports about its layout, for the same consumers.
    readonly property var geometry: ({ layout: shape, side: "track" })
    signal childSelected(string childId)
    signal dismissRequested()
    // The closing has played out: the host may hide its window.
    signal closeFinished()
    // A wheel event the folder took, as Qt delivered it, and the whole steps
    // it made.
    signal wheelTaken(real angleDelta, real pixelDelta, int steps)
    objectName: "folderTrack"
    focus: true
    activeFocusOnTab: true
    Accessible.name: folderTitle

    NumberAnimation {
        id: phaseAnimation
        target: root
        property: "openingPhase"
        easing.type: Easing.Linear
        onFinished: if (root.closing) root.finishClosing()
    }
    // One wheel step eases the children to their next places, as on the dock;
    // a step that arrives meanwhile retargets it.
    NumberAnimation {
        id: travelAnimation
        target: root
        property: "trackOffset"
        duration: 100
        easing.type: Easing.OutCubic
    }
    function runPhase(to) {
        phaseAnimation.stop()
        phaseAnimation.from = openingPhase
        phaseAnimation.to = to
        phaseAnimation.duration = Math.max(1, Math.round(
            Math.max(80, Math.min(1200, duration)) * Math.abs(to - openingPhase)))
        phaseAnimation.start()
    }
    function finishClosing() {
        closing = false
        closeFinished()
    }
    // Fold the children back into the folder, then report it.
    function close() {
        if (closing) return
        closing = true
        if (reducedMotion || !opened || openingPhase <= 0) {
            phaseAnimation.stop()
            openingPhase = 0
            finishClosing()
            return
        }
        runPhase(0)
    }
    // Opened again while it was closing: it opens from where it has got to.
    function reopen() {
        if (!closing) return
        closing = false
        if (reducedMotion) {
            phaseAnimation.stop()
            openingPhase = 1
        } else {
            runPhase(1)
        }
    }
    onReducedMotionChanged: if (reducedMotion) {
        phaseAnimation.stop()
        openingPhase = opened && !closing ? 1 : 0
        if (closing) finishClosing()
    }
    onOpenedChanged: {
        phaseAnimation.stop()
        travelAnimation.stop()
        closing = false
        openingPhase = opened && reducedMotion ? 1 : 0
        if (opened) {
            if (!reducedMotion) runPhase(1)
            trackOffset = 0
            travelTarget = 0
            wheelRemainder = 0
            selectedChildId = entries.length ? String(entries[0].id) : ""
            keyboardSelection = false
        }
    }
    onEntriesChanged: if (indexForId(selectedChildId) < 0)
        selectedChildId = entries.length ? String(entries[0].id) : ""

    function indexForId(id) {
        for (let i = 0; i < entries.length; ++i)
            if (String(entries[i].id) === id) return i
        return -1
    }
    function selectChild(id) {
        const index = indexForId(id)
        if (!opened || openingInProgress || index < 0 || entries[index].selectable !== true) return false
        const placed = track.entries[index]
        if (!placed || !placed.onTrack) return false
        selectedChildId = id
        childSelected(id)
        return true
    }
    // Moves every child `steps` slots along the path, round past its ends.
    function moveAlong(steps) {
        if (!steps || !entries.length) return
        travelTarget = Math.round(travelTarget) + steps
        travelAnimation.stop()
        if (reducedMotion || !opened) {
            trackOffset = travelTarget
            return
        }
        travelAnimation.from = trackOffset
        travelAnimation.to = travelTarget
        travelAnimation.start()
    }
    // Wheel input gathers into whole steps: 120 angle units are one
    // notch, a touchpad's pixels count by the pitch between two children, and
    // the panel's Scroll sensitivity says how many steps that makes. As on the
    // dock, turning the wheel away moves the children on towards the end of
    // the path; turning it back brings the next ones in there. Returns the
    // steps taken.
    function takeWheel(angleDelta, pixelDelta) {
        const pixels = Number(pixelDelta) || 0
        const notches = pixels !== 0 ? pixels / Math.max(1, pitch) : (Number(angleDelta) || 0) / 120
        const sensitivity = Math.max(0.25, Math.min(4, Number(scrollSensitivity) || 1))
        wheelRemainder += notches * sensitivity
        const steps = wheelRemainder >= 0
            ? Math.floor(wheelRemainder + 1e-6) : Math.ceil(wheelRemainder - 1e-6)
        wheelRemainder -= steps
        moveAlong(steps)
        wheelTaken(Number(angleDelta) || 0, pixels, steps)
        return steps
    }
    function wheelEvent(wheel, normalizedPixels) {
        const delta = normalizedPixels || wheel.pixelDelta
        const pixels = delta.y !== 0 ? delta.y : delta.x
        const angle = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
        if (opened && !closing)
            takeWheel(angle, pixels)
        wheel.accepted = true
    }
    function moveSelection(delta) {
        if (!entries.length) return
        keyboardSelection = true
        const index = Math.max(0, Math.min(entries.length - 1, indexForId(selectedChildId) + delta))
        selectedChildId = String(entries[index].id)
        // Bring the selected child onto the path with the least travel.
        travelAnimation.stop()
        trackOffset = travelTarget
        wheelRemainder = 0
        travelTarget = LayoutEngine.folderTravelTo(track, index, trackOffset)
        trackOffset = travelTarget
    }
    // Whether a point is on the small panel drawn under the children.
    function onOutline(x, y) {
        const loops = outline && outline.closed ? outline.loops || [] : []
        const points = loops.length ? loops[loops.length - 1]
            : outline && outline.closed ? outline.points || [] : []
        let inside = false
        for (let i = 0, j = points.length - 1; i < points.length; j = i++) {
            const a = points[i], b = points[j]
            if ((a.y > y) !== (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)
                inside = !inside
        }
        return inside
    }
    Keys.onEscapePressed: event => { dismissRequested(); event.accepted = true }
    Keys.onLeftPressed: event => { moveSelection(-1); event.accepted = true }
    Keys.onUpPressed: event => { moveSelection(-1); event.accepted = true }
    Keys.onRightPressed: event => { moveSelection(1); event.accepted = true }
    Keys.onDownPressed: event => { moveSelection(1); event.accepted = true }
    Keys.onReturnPressed: event => { selectChild(selectedChildId); event.accepted = true }
    Keys.onEnterPressed: event => { selectChild(selectedChildId); event.accepted = true }

    // A press beside the children and their small panel closes the folder;
    // the wheel moves the children along the path.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onPressed: mouse => {
            if (!root.onOutline(mouse.x, mouse.y)) root.dismissRequested()
        }
    }
    ScrollInput {
        wheelConsumer: function(wheel, pixels) { root.wheelEvent(wheel, pixels) }
    }
    // The fan's sector or the second ring, in the panel's own look: its
    // track's colour, width and glow. It grows from the folder as it opens.
    // The faint fill and the line are separate canvases, so only the line
    // glows. The glow is blurred on the graphics card; a Canvas shadow blur
    // over a window-sized canvas runs on the main thread and took about half
    // a second at every opening.
    Canvas {
        id: outlineFill
        objectName: "folderOutlineFill"
        anchors.fill: parent
        visible: outlineCanvas.visible && root.outlineFilled
        opacity: outlineCanvas.opacity
        transform: Scale {
            origin.x: root.expansionOrigin.x
            origin.y: root.expansionOrigin.y
            xScale: Math.max(0, root.openingProgress)
            yScale: Math.max(0, root.openingProgress)
        }
        onPaint: {
            const context = getContext("2d")
            context.reset()
            const points = root.outline ? root.outline.points || [] : []
            const style = root.outlineStyle
            if (points.length < 3 || !style || !root.outlineFilled) return
            root.traceOutline(context, points, 0)
            if (root.outline.closed) context.closePath()
            context.globalAlpha = 0.18
            context.fillStyle = style.stroke
            // A band that goes round twice is filled between its edges only.
            context.fillRule = root.outline.evenOdd ? Qt.OddEvenFill : Qt.WindingFill
            context.fill()
        }
        onVisibleChanged: if (visible) requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections {
            target: root
            function onOutlineChanged() { outlineFill.requestPaint() }
            function onOutlineStyleChanged() { outlineFill.requestPaint() }
            function onOutlineFilledChanged() { outlineFill.requestPaint() }
        }
    }
    Canvas {
        id: outlineCanvas
        objectName: "folderOutline"
        anchors.fill: parent
        visible: root.outline !== null && root.outlineStyle !== null
            && root.outlineStyle.trackVisible !== false
        opacity: Math.max(0, Math.min(1, root.outlineOpacity))
            * Math.max(0, Math.min(1, root.openingProgress * 1.5))
        transform: Scale {
            origin.x: root.expansionOrigin.x
            origin.y: root.expansionOrigin.y
            xScale: Math.max(0, root.openingProgress)
            yScale: Math.max(0, root.openingProgress)
        }
        // The panel's track glow: its colour, as far as its blur reached.
        // Round a solid band (a Möbius strip) the glow stays close, so its
        // loops stay open.
        readonly property real glowBlur: !root.outlineStyle ? 0
            : (Number(root.outlineStyle.blur) || 0) * (root.outline && root.outline.ribbon ? 0.35 : 1)
        layer.enabled: visible && glowBlur > 0
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: root.outlineStyle ? root.outlineStyle.shadow : "transparent"
            shadowOpacity: 1
            shadowBlur: 1
            blurMax: Math.max(2, Math.round(1.5 * outlineCanvas.glowBlur))
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 0
            autoPaddingEnabled: true
        }
        onPaint: {
            const context = getContext("2d")
            context.reset()
            const points = root.outline ? root.outline.points || [] : []
            const style = root.outlineStyle
            if (points.length < 2 || !style) return
            if (root.outline.ribbon) {
                root.paintRibbon(context, root.outline.ribbon, style)
                return
            }
            context.lineWidth = style.lineWidth
            context.strokeStyle = style.stroke
            context.lineCap = "round"
            context.lineJoin = "round"
            // A shape may stroke only part of its outline (a fan's arc):
            // the rest is the faint fill's border alone.
            const first = Math.max(0, Math.floor(Number(root.outline.strokeFrom) || 0))
            const partial = first > 0 && first < points.length - 1
            root.traceOutline(context, points, partial ? first : 0)
            if (!partial && root.outline.closed) context.closePath()
            context.stroke()
            // A ring's second circle round the first.
            for (const loop of root.outline.loops || []) {
                if (loop.length < 3) continue
                root.traceOutline(context, loop, 0)
                context.closePath()
                context.stroke()
            }
        }
        onVisibleChanged: if (visible) requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections {
            target: root
            function onOutlineChanged() { outlineCanvas.requestPaint() }
            function onOutlineStyleChanged() { outlineCanvas.requestPaint() }
        }
    }
    // A Möbius band as one solid ribbon in the panel's look, one colour and
    // one fill: each step of it two triangles, all turning the same way, so
    // where they meet, where the band twists and its sides cross as seen, or
    // where it crosses itself, nothing shows between them.
    function paintRibbon(context, ribbon, style) {
        const left = ribbon.left || [], right = ribbon.right || []
        const count = Math.min(left.length, right.length)
        if (count < 3) return
        const triangle = function(a, b, c) {
            const area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)
            const corners = area < 0 ? [a, c, b] : [a, b, c]
            context.moveTo(corners[0].x, corners[0].y)
            context.lineTo(corners[1].x, corners[1].y)
            context.lineTo(corners[2].x, corners[2].y)
            context.closePath()
        }
        context.globalAlpha = 1
        context.fillRule = Qt.WindingFill
        context.fillStyle = Qt.lighter(style.stroke, 1.1)
        context.beginPath()
        for (let index = 0; index < count; ++index) {
            const next = (index + 1) % count
            triangle(left[index], left[next], right[next])
            triangle(left[index], right[next], right[index])
        }
        context.fill()
    }
    function traceOutline(context, points, from) {
        context.beginPath()
        context.moveTo(points[from].x, points[from].y)
        for (let index = from + 1; index < points.length; ++index)
            context.lineTo(points[index].x, points[index].y)
    }
    QQC2.Label {
        objectName: "folderTrackNotice"
        x: root.expansionOrigin.x - width / 2
        y: root.expansionOrigin.y - root.iconSize - height
        visible: root.entries.length === 0
        text: root.snapshot.status === "empty" ? qsTr("This folder is empty.")
            : qsTr("This folder is unavailable.")
        style: Text.Outline
        styleColor: palette.base
    }
    Repeater {
        model: root.entries
        delegate: Item {
            id: child
            required property var modelData
            required property int index
            readonly property var placed: root.track.entries[index] || ({ x: 0, y: 0, scale: 1,
                onTrack: false, visibility: 0 })
            // The opening's growth: a motion that swings past its place
            // swings the size with it.
            readonly property real grownScale: 0.3 + 0.7 * Math.max(0, Math.min(1.3, root.openingProgress))
            // The icon's drawn size: its depth on a tilted or projected
            // panel, and the growth.
            readonly property real drawnScale: Math.max(0.35, Number(placed.scale) || 1) * grownScale
            readonly property bool nameAbove: isFinite(root.dockMiddleY) && centreY < root.dockMiddleY
            // How much of the way from the icon's middle to the stand's base
            // the standing point is: all of it behind a tilted dock's middle
            // or on any drawn band, none of it before the middle or off a band.
            readonly property real standShare: !root.standsOnPath ? 0
                : isFinite(root.dockMiddleY) && root.dockHalfHeight > 0
                    ? Math.max(0, Math.min(1, (root.dockMiddleY - centreY) / root.dockHalfHeight * 0.5 + 0.5)) : 1
            objectName: "folder-child-" + index
            // The icon's centre travels from the folder icon to its place.
            readonly property real centreX: root.expansionOrigin.x
                + (placed.x - root.expansionOrigin.x) * root.openingProgress
            readonly property real centreY: root.expansionOrigin.y
                + (placed.y - root.expansionOrigin.y) * root.openingProgress
            x: centreX - width / 2
            // The cell hung from its standing point: the icon's middle on the
            // place, or the stand's base, or between.
            y: centreY - (root.iconSize / 2 + root.standLayout.above
                          + (root.standFoot - root.iconSize / 2 - root.standLayout.above) * standShare) * drawnScale
            width: root.cellWidth * drawnScale
            height: root.cellHeight * drawnScale
            opacity: placed.visibility * Math.max(0, Math.min(1, root.openingProgress * 1.5))
            Accessible.role: Accessible.Button
            Accessible.name: String(modelData.displayName || modelData.name || "")
            Accessible.onPressAction: root.selectChild(String(modelData.id))
            IconMotionController {
                id: motion
                profiles: []
                reducedMotion: root.reducedMotion
                revealed: root.opened
                sceneVisible: root.visible && root.opened
                entryIndex: child.index
            }
            IconScene {
                id: iconView
                x: (parent.width - width) / 2
                y: root.standLayout.above * child.drawnScale
                width: root.iconSize * child.drawnScale
                height: width
                entry: child.modelData
                logicalSize: root.iconSize
                iconStyleDefinition: root.iconStyleDefinition
                tileSettings: root.pedestalSettings
                showIndicator: false
                disabled: child.modelData.selectable !== true
                hovered: pointer.containsMouse || (root.keyboardSelection
                    && root.selectedChildId === String(child.modelData.id))
                reducedMotion: root.reducedMotion
                glyphMotion: MotionChannels.motionFor(motion.channels, "icon",
                                                      { size: root.iconSize })
            }
            QQC2.Label {
                objectName: "folder-name-" + child.index
                // The name keeps the dock's own letter size and its cell's
                // width wherever its child stands along the depth: only the
                // opening's growth scales it, with the child.
                x: (parent.width - width) / 2
                // Below the cell and below the stand as drawn, so the name never
                // crosses the stand or the icon; above the icon for a child on
                // the far side of a tilted ring, away from the ring.
                y: child.nameAbove ? -(height + 4)
                    : Math.max((root.iconSize + root.standLayout.above + root.standLayout.below) * child.drawnScale,
                               iconView.y + iconView.standDrawnBottom) + 4
                width: root.cellWidth
                visible: root.showNames
                text: String(child.modelData.displayName || child.modelData.name || "")
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                scale: child.grownScale
                transformOrigin: child.nameAbove ? Item.Bottom : Item.Top
                style: Text.Outline
                styleColor: palette.base
            }
            MouseArea {
                id: pointer
                anchors.fill: parent
                enabled: root.opened && !root.openingInProgress && child.placed.onTrack === true
                hoverEnabled: true
                onEntered: if (!root.keyboardSelection) root.selectedChildId = String(child.modelData.id)
                onPositionChanged: mouse => {
                    // A child moving under a pointer that stays still is no
                    // move of the pointer: the keys keep their selection.
                    const at = pointer.mapToItem(null, mouse.x, mouse.y)
                    const still = Math.abs(at.x - root.pointerAt.x) < 0.5 && Math.abs(at.y - root.pointerAt.y) < 0.5
                    root.pointerAt = at
                    if (still && root.keyboardSelection) return
                    root.keyboardSelection = false
                    root.selectedChildId = String(child.modelData.id)
                }
                onClicked: {
                    root.keyboardSelection = false
                    root.selectChild(String(child.modelData.id))
                }
            }
            QQC2.ToolTip.visible: pointer.containsMouse && !root.showNames
            // Plain text: "<" followed by a zero-width space is never a tag.
            QQC2.ToolTip.text: String(modelData.displayName || modelData.name || "")
                .replace(/</g, "<​")
        }
    }
    FontMetrics { id: nameMetrics; font: Qt.application.font }
}
