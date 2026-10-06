import QtQuick
import QtQuick.Controls as QQC2
import "MotionChannels.js" as MotionChannels

// "Along the dock": a folder's contents on a curve beside the dock, where the
// dock's own track continues outside it. The host traces that curve in this
// item's coordinates; this item stands the children on it, unfolds them from
// the folder icon, and takes the pointer, wheel and keyboard. Shared
// presentation only; the panel backend revalidates every selected ID.
Item {
    id: root
    property var snapshot: ({ status: "unavailable", entries: [] })
    property string folderTitle: ""
    property int duration: 260
    property string easing: "outBack"
    property bool reducedMotion: false
    property bool opened: false
    property bool showNames: true
    property var iconStyleDefinition: ({})
    property int iconSize: 48
    // The curve as { x, y, scale } samples, the folder facing the middle one,
    // and the folder icon's centre: the children unfold from there.
    property var samples: []
    property point expansionOrigin: Qt.point(width / 2, height / 2)
    property real openingProgress: opened ? 1 : 0
    readonly property bool openingInProgress: openingAnimation.running
    property string selectedChildId: ""
    property bool keyboardSelection: false
    readonly property var entries: (snapshot.entries || []).slice(0, 48)
    readonly property real cellWidth: showNames ? 108 : iconSize
    readonly property real cellHeight: iconSize + (showNames ? nameMetrics.height * 2 + 4 : 0)
    // Neighbours along the curve: a name's width apart, or an icon and a gap.
    readonly property real pitch: showNames ? cellWidth + 6 : iconSize + 14
    // How far the wheel or keys have moved the children along; never saved.
    property real trackOffset: 0
    readonly property var track: LayoutEngine.folderTrackLayout(
        samples, entries.length, pitch, trackOffset)
    // What the popup reports about its layout, for the same consumers.
    readonly property var geometry: ({ layout: "track", side: "track" })
    signal childSelected(string childId)
    signal dismissRequested()
    objectName: "folderTrack"
    focus: true
    activeFocusOnTab: true
    Accessible.name: folderTitle
    Behavior on trackOffset {
        enabled: root.opened && !root.openingInProgress && !root.reducedMotion
            && !root.keyboardSelection
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }
    NumberAnimation {
        id: openingAnimation
        target: root
        property: "openingProgress"
        from: 0
        to: 1
        duration: Math.max(80, Math.min(1200, root.duration))
        easing.type: root.easing === "outCubic" ? Easing.OutCubic
            : root.easing === "outElastic" || root.easing === "spring" ? Easing.OutElastic
            : Easing.OutBack
    }
    onReducedMotionChanged: if (reducedMotion) {
        openingAnimation.stop()
        openingProgress = opened ? 1 : 0
    }
    onOpenedChanged: {
        openingAnimation.stop()
        openingProgress = opened && reducedMotion ? 1 : 0
        if (opened) {
            if (!reducedMotion) openingAnimation.start()
            trackOffset = 0
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
    function moveAlong(delta) {
        trackOffset = Math.max(0, Math.min(track.maximumOffset, Math.round(trackOffset) + delta))
    }
    function moveSelection(delta) {
        if (!entries.length) return
        keyboardSelection = true
        const index = Math.max(0, Math.min(entries.length - 1, indexForId(selectedChildId) + delta))
        selectedChildId = String(entries[index].id)
        // Bring the selected child onto the curve with the least travel.
        const offset = Math.max(index - (track.capacity - 1), Math.min(index, Math.round(trackOffset)))
        trackOffset = Math.max(0, Math.min(track.maximumOffset, offset))
    }
    Keys.onEscapePressed: event => { dismissRequested(); event.accepted = true }
    Keys.onLeftPressed: event => { moveSelection(-1); event.accepted = true }
    Keys.onUpPressed: event => { moveSelection(-1); event.accepted = true }
    Keys.onRightPressed: event => { moveSelection(1); event.accepted = true }
    Keys.onDownPressed: event => { moveSelection(1); event.accepted = true }
    Keys.onReturnPressed: event => { selectChild(selectedChildId); event.accepted = true }
    Keys.onEnterPressed: event => { selectChild(selectedChildId); event.accepted = true }

    // A press beside the children closes the folder; the wheel moves the
    // children along the curve when they do not all fit on it.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onPressed: root.dismissRequested()
        onWheel: wheel => {
            const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
            if (root.track.windowed && delta !== 0) root.moveAlong(delta > 0 ? -1 : 1)
            wheel.accepted = true
        }
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
            readonly property real drawnScale: Math.max(0.35, Number(placed.scale) || 1)
                * (0.3 + 0.7 * root.openingProgress)
            objectName: "folder-child-" + index
            // The icon's centre travels from the folder icon to its place.
            readonly property real centreX: root.expansionOrigin.x
                + (placed.x - root.expansionOrigin.x) * root.openingProgress
            readonly property real centreY: root.expansionOrigin.y
                + (placed.y - root.expansionOrigin.y) * root.openingProgress
            x: centreX - width / 2
            y: centreY - root.iconSize * drawnScale / 2
            width: root.cellWidth * drawnScale
            height: root.cellHeight * drawnScale
            opacity: placed.visibility * Math.min(1, root.openingProgress * 1.5)
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
                x: (parent.width - width) / 2
                width: root.iconSize * child.drawnScale
                height: width
                entry: child.modelData
                logicalSize: root.iconSize
                iconStyleDefinition: root.iconStyleDefinition
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
                y: root.iconSize * child.drawnScale + 4
                width: parent.width
                visible: root.showNames
                text: String(child.modelData.displayName || child.modelData.name || "")
                textFormat: Text.PlainText
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                scale: child.drawnScale
                transformOrigin: Item.Top
                style: Text.Outline
                styleColor: palette.base
            }
            MouseArea {
                id: pointer
                anchors.fill: parent
                enabled: root.opened && !root.openingInProgress && child.placed.onTrack === true
                hoverEnabled: true
                onEntered: if (!root.keyboardSelection) root.selectedChildId = String(child.modelData.id)
                onPositionChanged: {
                    root.keyboardSelection = false
                    root.selectedChildId = String(child.modelData.id)
                }
                onClicked: {
                    root.keyboardSelection = false
                    root.selectChild(String(child.modelData.id))
                }
                onWheel: wheel => {
                    const delta = wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x
                    if (root.track.windowed && delta !== 0) root.moveAlong(delta > 0 ? -1 : 1)
                    wheel.accepted = true
                }
            }
            QQC2.ToolTip.visible: pointer.containsMouse && !root.showNames
            // Plain text: "<" followed by a zero-width space is never a tag.
            QQC2.ToolTip.text: String(modelData.displayName || modelData.name || "")
                .replace(/</g, "<\u200B")
        }
    }
    FontMetrics { id: nameMetrics; font: Qt.application.font }
}
