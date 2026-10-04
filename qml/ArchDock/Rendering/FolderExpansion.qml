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
    property real openingProgress: opened ? 1 : 0
    readonly property bool openingInProgress: openingAnimation.running
    property var iconStyleDefinition: ({})
    property int maximumWidth: 640
    property int maximumHeight: 420
    property string selectedChildId: ""
    property bool keyboardSelection: false
    property point lastPointerPosition: Qt.point(-1, -1)
    readonly property var entries: (snapshot.entries || []).slice(0, 48)
    readonly property var geometry: LayoutEngine.expansionGeometry(
        layout, entries.length, 48, 6, 140, Math.ceil(Math.sqrt(entries.length)), {
            maximumWidth: Math.max(56, maximumWidth - padding * 2),
            labelWidth: showNames ? 108 : 0,
            labelHeight: showNames ? nameMetrics.height * 2 + 4 : 0,
            compactPath: true
        })
    readonly property var openingProfiles: [{
        id: "folder-open", target: "icon", trigger: "panel-reveal",
        reducedMotion: { mode: "none" },
        tracks: [{ id: "rise", property: "translate-y", from: 0.35, to: 0,
            duration: Math.max(80, Math.min(1200, duration)),
            easing: easing === "outCubic" ? "out-cubic"
                : easing === "outElastic" || easing === "spring" ? "out-elastic" : "out-back" }]
    }]
    signal childSelected(string childId)
    signal dismissRequested()
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
            viewport.cancelFlick()
            viewport.contentX = 0
            viewport.contentY = 0
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
        const point = geometry.entries[index]
        viewport.cancelFlick()
        const x = geometry.followsPath ? 0 : point.x < viewport.contentX ? point.x
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
            width: parent.width
            height: root.entries.length ? Math.min(root.geometry.height,
                Math.max(56, root.maximumHeight - 20
                    - [heading, layoutNotice, emptyNotice, currentName, pageNotice]
                        .reduce(function(total, label) {
                            return total + (label.visible ? label.implicitHeight + column.spacing : 0)
                        }, 0))) : 0
            contentWidth: root.geometry.width
            contentHeight: root.geometry.height
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
            }
            Repeater {
                model: root.entries
                delegate: Item {
                    id: child
                    required property var modelData
                    required property int index
                    readonly property var point: root.geometry.entries[index] || ({ x: 0, y: 0 })
                    objectName: "folder-child-" + index
                    // Re-evaluate x at the child's current visible height:
                    // both wheel and held dragging follow the same curve.
                    x: {
                        if (!root.geometry.followsPath) return point.x
                        const progress = Math.max(0, Math.min(1,
                            (point.y - viewport.contentY) / Math.max(1, viewport.height - height)))
                        return Math.sin(progress * Math.PI
                            * (root.geometry.layout === "fan" ? 0.5 : 1)) * root.geometry.pathBend
                    }
                    y: point.y
                    width: root.geometry.cellWidth
                    height: root.geometry.cellHeight
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
                        enabled: root.opened && !root.openingInProgress
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
                    QQC2.ToolTip.text: String(modelData.displayName || modelData.name || "")
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
