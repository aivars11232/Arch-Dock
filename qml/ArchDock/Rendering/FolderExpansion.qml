import QtQuick
import QtQuick.Controls as QQC2
import "MotionChannels.js" as MotionChannels

// Shared presentation only; the panel backend revalidates every selected ID.
QQC2.Pane {
    id: root
    property var snapshot: ({ status: "unavailable", entries: [] })
    property string folderTitle: ""
    property string layout: "fan"
    property int duration: 260
    property string easing: "outBack"
    property bool reducedMotion: false
    property bool opened: true
    property bool showNames: true
    property point expansionOrigin: Qt.point(width / 2, height)
    // An opening runs this phase from 0 to 1 over the folder animation, a
    // closing from 1 to 0; `openingProgress` is the chosen motion at that
    // time, so a closing plays the opening backwards (PD-15).
    property real openingPhase: opened ? 1 : 0
    readonly property real openingProgress: MotionChannels.folderEasing(easing, openingPhase)
    readonly property bool openingInProgress: openingAnimation.running
    property bool closing: false
    // Wheel notches, or rows of touchpad travel, per row or child (PD-16).
    property real scrollSensitivity: 1
    property var iconStyleDefinition: ({})
    property int maximumWidth: 640
    property int maximumHeight: 420
    property string selectedChildId: ""
    property bool keyboardSelection: false
    property point lastPointerPosition: Qt.point(-1, -1)
    // The side of the clicked folder the contents open on, and how far the
    // folder's outward direction leans along that side. Empty keeps the
    // original frame, opening to the right.
    property string expansionSide: ""
    property real expansionLean: 0
    readonly property var entries: (snapshot.entries || []).slice(0, 48)
    readonly property var geometry: LayoutEngine.expansionGeometry(
        layout, entries.length, 48, 6, 140, Math.ceil(Math.sqrt(entries.length)), {
            maximumWidth: Math.max(56, maximumWidth - padding * 2),
            maximumHeight: Math.max(56, maximumHeight - padding * 2),
            labelWidth: showNames ? 108 : 0,
            labelHeight: showNames ? nameMetrics.height * 2 + 4 : 0,
            compactPath: true,
            side: expansionSide,
            lean: expansionLean
        })
    readonly property bool expansionVertical: geometry.side === "top" || geometry.side === "bottom"
    // Where the folder stands along the popup edge nearest it, in this
    // item's coordinates: the host attaches the popup there.
    readonly property real anchorAcross: expansionVertical
        ? padding + viewport.x + Math.max(0, Math.min(geometry.anchor.x, viewport.width))
        : padding + viewport.y + Math.max(0, Math.min(geometry.anchor.y, viewport.height))
    // A compact Fan or Arc holds as many children as stand on its half circle
    // and moves the rest along it: one wheel notch is one child.
    readonly property real pathScrollStep: 20 * Math.max(1, Qt.styleHints.wheelScrollLines)
    // How far one wheel notch scrolls (PD-16): one child along a path or a
    // stack, one row of a grid or a ring, one column sideways.
    readonly property real stackStep: Math.max(geometry.iconSize * 0.65, 6) / geometry.iconSize
    readonly property real notchHeight: geometry.followsPath ? pathScrollStep
        : geometry.layout === "stack" ? stackStep * geometry.cellHeight : geometry.cellHeight + 6
    readonly property real notchWidth: geometry.layout === "stack"
        ? stackStep * geometry.cellWidth : geometry.cellWidth + 6
    readonly property var path: LayoutEngine.expansionPath(geometry, viewport.height)
    readonly property int pathTarget: geometry.followsPath
        ? Math.max(0, Math.min(path.maximumOffset,
            Math.round(viewport.contentY / pathScrollStep))) : 0
    // The path rests on whole children, so both ends of the curve stay filled
    // wherever a drag happens to let go.
    property real pathOffset: pathTarget
    Behavior on pathOffset {
        enabled: root.opened && !root.openingInProgress && !root.reducedMotion
            && !root.keyboardSelection
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }
    readonly property var openingProfiles: [{
        id: "folder-open", target: "icon", trigger: "panel-reveal",
        reducedMotion: { mode: "none" },
        // Each icon also rises into its place; the chosen motion is the
        // contents' own (openingProgress), so a spring rises softly.
        tracks: [{ id: "rise", property: "translate-y", from: 0.35, to: 0,
            duration: Math.max(80, Math.min(1200, duration)),
            easing: easing === "outElastic" ? "out-elastic"
                : easing === "outBack" ? "out-back" : "out-cubic" }]
    }]
    signal childSelected(string childId)
    signal dismissRequested()
    // The closing has played out: the host may hide its window.
    signal closeFinished()
    objectName: "folderExpansion"
    padding: 10
    background: null
    focus: true
    activeFocusOnTab: true
    Accessible.name: folderTitle
    implicitWidth: Math.min(Math.max(160, geometry.width + 20), Math.max(160, maximumWidth))
    implicitHeight: column.implicitHeight + 20

    FontMetrics { id: nameMetrics; font: root.font }
    HoverHandler {
        id: positionObserver
        target: null
        onPointChanged: {
            const position = point.position
            if (Math.abs(position.x - root.lastPointerPosition.x) > 0.5
                    || Math.abs(position.y - root.lastPointerPosition.y) > 0.5)
                root.keyboardSelection = false
            root.lastPointerPosition = position
        }
    }
    opacity: openingProgress
    transform: Scale {
        origin.x: root.expansionOrigin.x
        origin.y: root.expansionOrigin.y
        xScale: root.openingProgress
        yScale: root.openingProgress
    }
    NumberAnimation {
        id: openingAnimation
        target: root
        property: "openingPhase"
        easing.type: Easing.Linear
        onFinished: if (root.closing) root.finishClosing()
    }
    function runPhase(to) {
        openingAnimation.stop()
        openingAnimation.from = openingPhase
        openingAnimation.to = to
        openingAnimation.duration = Math.max(1, Math.round(
            Math.max(80, Math.min(1200, duration)) * Math.abs(to - openingPhase)))
        openingAnimation.start()
    }
    function finishClosing() {
        closing = false
        closeFinished()
    }
    // Fold the contents back into the folder, then report it.
    function close() {
        if (closing) return
        closing = true
        if (reducedMotion || !opened || openingPhase <= 0) {
            openingAnimation.stop()
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
            openingAnimation.stop()
            openingPhase = 1
        } else {
            runPhase(1)
        }
    }
    onReducedMotionChanged: if (reducedMotion) {
        openingAnimation.stop()
        openingPhase = opened && !closing ? 1 : 0
        if (closing) finishClosing()
    }
    onOpenedChanged: {
        openingAnimation.stop()
        closing = false
        openingPhase = opened && reducedMotion ? 1 : 0
        if (opened) {
            if (!reducedMotion) runPhase(1)
            viewport.cancelFlick()
            // A ring or stack starts beside the folder: when it is larger
            // than the popup, the folder's end of it is shown first.
            const fromFolder = geometry.layout === "ring" || geometry.layout === "stack"
            viewport.contentX = fromFolder && geometry.side === "left"
                ? Math.max(0, viewport.contentWidth - viewport.width) : 0
            viewport.contentY = fromFolder && geometry.side === "top"
                ? Math.max(0, viewport.contentHeight - viewport.height) : 0
            selectedChildId = entries.length ? String(entries[0].id) : ""
            keyboardSelection = false
        }
    }

    function indexForId(id) {
        for (let i = 0; i < entries.length; ++i)
            if (String(entries[i].id) === id) return i
        return -1
    }
    function selectChild(id) {
        const index = indexForId(id)
        if (!opened || openingInProgress || index < 0 || entries[index].selectable !== true) return false
        selectedChildId = id
        childSelected(id)
        return true
    }
    function moveSelection(delta) {
        if (!entries.length) return
        keyboardSelection = true
        lastPointerPosition = positionObserver.point.position
        const index = Math.max(0, Math.min(entries.length - 1, indexForId(selectedChildId) + delta))
        selectedChildId = String(entries[index].id)
        viewport.cancelFlick()
        if (geometry.followsPath) {
            // Bring the selected child onto the path with the least travel.
            const offset = Math.max(index - (path.capacity - 1), Math.min(index, pathTarget))
            viewport.contentY = Math.max(0, Math.min(path.maximumOffset, offset)) * pathScrollStep
            return
        }
        const point = geometry.entries[index]
        const x = point.x < viewport.contentX ? point.x
            : Math.max(viewport.contentX, point.x + geometry.cellWidth - viewport.width)
        const y = point.y < viewport.contentY ? point.y
            : Math.max(viewport.contentY, point.y + geometry.cellHeight - viewport.height)
        viewport.contentX = Math.max(0, Math.min(x, viewport.contentWidth - viewport.width))
        viewport.contentY = Math.max(0, Math.min(y, viewport.contentHeight - viewport.height))
    }
    onEntriesChanged: if (indexForId(selectedChildId) < 0)
        selectedChildId = entries.length ? String(entries[0].id) : ""
    Keys.onEscapePressed: event => { dismissRequested(); event.accepted = true }
    Keys.onLeftPressed: event => { moveSelection(-1); event.accepted = true }
    Keys.onUpPressed: event => { moveSelection(-1); event.accepted = true }
    Keys.onRightPressed: event => { moveSelection(1); event.accepted = true }
    Keys.onDownPressed: event => { moveSelection(1); event.accepted = true }
    Keys.onReturnPressed: event => { selectChild(selectedChildId); event.accepted = true }
    Keys.onEnterPressed: event => { selectChild(selectedChildId); event.accepted = true }

    contentItem: Column {
        id: column
        spacing: 8
        QQC2.Label {
            id: heading
            width: parent.width
            text: root.folderTitle
            textFormat: Text.PlainText
            font.bold: true
            elide: Text.ElideRight
            visible: root.entries.length === 0
            style: Text.Outline
            styleColor: root.palette.base
        }
        QQC2.Label {
            id: layoutNotice
            width: parent.width
            visible: root.geometry.fallbackApplied
            text: qsTr("This saved layout uses Fan.")
            wrapMode: Text.Wrap
            style: Text.Outline
            styleColor: root.palette.base
        }
        QQC2.Label {
            id: emptyNotice
            width: parent.width
            visible: root.entries.length === 0
            text: root.snapshot.status === "empty" ? qsTr("This folder is empty.")
                : qsTr("This folder is unavailable.")
            wrapMode: Text.Wrap
            style: Text.Outline
            styleColor: root.palette.base
        }
        Flickable {
            id: viewport
            objectName: "folderViewport"
            // Contents opening to the left keep against the folder's edge.
            x: root.geometry.side === "left" ? parent.width - width : 0
            width: root.geometry.side === "left"
                ? Math.min(parent.width, root.geometry.width) : parent.width
            height: root.entries.length ? Math.min(root.geometry.height,
                Math.max(56, root.maximumHeight - 20
                    - [heading, layoutNotice, emptyNotice, currentName, pageNotice]
                        .reduce(function(total, label) {
                            return total + (label.visible ? label.implicitHeight + column.spacing : 0)
                        }, 0))) : 0
            contentWidth: root.geometry.width
            // A path does not scroll as a list: the view only supplies how far
            // the folder has moved along it.
            contentHeight: root.geometry.followsPath
                ? height + root.path.maximumOffset * root.pathScrollStep
                : root.geometry.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.AutoFlickIfNeeded
            Behavior on contentY {
                enabled: root.opened && !root.openingInProgress && !root.reducedMotion
                    && !root.keyboardSelection && !viewport.dragging && !viewport.flicking
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }
            ScrollInput {
                parent: viewport
                flickables: [viewport]
                verticalNotch: root.notchHeight
                horizontalNotch: root.notchWidth
                // A touchpad moves a path one child per pitch of travel.
                verticalPixelScale: root.geometry.followsPath
                    ? root.pathScrollStep / Math.max(1, root.geometry.pathPitch) : 1
                sensitivity: root.scrollSensitivity
            }
            Repeater {
                model: root.entries
                delegate: Item {
                    id: child
                    required property var modelData
                    required property int index
                    readonly property var point: root.geometry.followsPath
                        ? LayoutEngine.expansionPathPoint(root.geometry, root.path,
                                                          index, root.pathOffset)
                        : root.geometry.entries[index] || ({ x: 0, y: 0 })
                    readonly property bool onPath:
                        !root.geometry.followsPath || point.onPath === true
                    objectName: "folder-child-" + index
                    // The curve stays where it is while the view moves under
                    // it: wheel, held dragging and keys all walk the same path.
                    x: point.x
                    y: root.geometry.followsPath ? viewport.contentY + point.y : point.y
                    width: root.geometry.cellWidth
                    height: root.geometry.cellHeight
                    // A child beyond either end stays in the scene for
                    // assistive tools but is neither drawn nor pressed.
                    opacity: root.geometry.followsPath ? point.visibility : 1
                    Accessible.role: Accessible.Button
                    Accessible.name: String(modelData.displayName || modelData.name || "")
                    Accessible.onPressAction: root.selectChild(String(modelData.id))
                    IconMotionController {
                        id: motion
                        objectName: "folder-motion-" + child.index
                        profiles: root.openingProfiles
                        reducedMotion: root.reducedMotion
                        revealed: root.opened
                        sceneVisible: root.visible && root.opened
                        entryIndex: child.index
                    }
                    IconScene {
                        x: (parent.width - width) / 2
                        width: root.geometry.iconSize
                        height: width
                        entry: child.modelData
                        logicalSize: root.geometry.iconSize
                        iconStyleDefinition: root.iconStyleDefinition
                        showIndicator: false
                        disabled: child.modelData.selectable !== true
                        hovered: pointer.containsMouse || (root.keyboardSelection
                            && root.selectedChildId === String(child.modelData.id))
                        reducedMotion: root.reducedMotion
                        glyphMotion: MotionChannels.motionFor(motion.channels, "icon", { size: root.geometry.iconSize })
                    }
                    QQC2.Label {
                        objectName: "folder-name-" + child.index
                        x: 0
                        y: root.geometry.iconSize + 4
                        width: parent.width
                        height: Math.max(0, root.geometry.cellHeight - y)
                        visible: root.showNames
                        text: String(child.modelData.displayName || child.modelData.name || "")
                        textFormat: Text.PlainText
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        style: Text.Outline
                        styleColor: root.palette.base
                    }
                    MouseArea {
                        id: pointer
                        anchors.fill: parent
                        enabled: root.opened && !root.openingInProgress && child.onPath
                        preventStealing: false
                        hoverEnabled: true
                        onEntered: if (!root.keyboardSelection)
                            root.selectedChildId = String(child.modelData.id)
                        onPositionChanged: if (!root.keyboardSelection)
                            root.selectedChildId = String(child.modelData.id)
                        onClicked: {
                            root.keyboardSelection = false
                            root.selectChild(String(child.modelData.id))
                        }
                    }
                    QQC2.ToolTip.visible: pointer.containsMouse && !root.showNames
                    // Plain text: "<" followed by a zero-width space is never a tag.
                    QQC2.ToolTip.text: String(modelData.displayName || modelData.name || "")
                        .replace(/</g, "<\u200B")
                }
            }
        }
        QQC2.Label {
            id: currentName
            width: parent.width
            text: {
                const index = root.indexForId(root.selectedChildId)
                return index >= 0 ? String(root.entries[index].displayName || root.entries[index].name || "") : ""
            }
            textFormat: Text.PlainText
            elide: Text.ElideMiddle
            visible: root.entries.length > 0 && !root.showNames
            style: Text.Outline
            styleColor: root.palette.base
        }
        QQC2.Label {
            id: pageNotice
            width: parent.width
            visible: root.snapshot.truncated === true
            text: qsTr("Showing the first 48 items.")
            wrapMode: Text.Wrap
            style: Text.Outline
            styleColor: root.palette.base
        }
    }
}
