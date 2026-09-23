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
    property var iconStyleDefinition: ({})
    property int maximumWidth: 640
    property int maximumHeight: 420
    property string selectedChildId: ""
    readonly property var entries: (snapshot.entries || []).slice(0, 48)
    readonly property var geometry: LayoutEngine.expansionGeometry(
        layout, entries.length, 56, 12, 140, Math.ceil(Math.sqrt(entries.length)))
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
    focus: true
    activeFocusOnTab: true
    implicitWidth: Math.min(Math.max(280, geometry.width + 20), Math.max(160, maximumWidth))
    implicitHeight: column.implicitHeight + 20

    function indexForId(id) {
        for (let i = 0; i < entries.length; ++i)
            if (String(entries[i].id) === id) return i
        return -1
    }
    function selectChild(id) {
        const index = indexForId(id)
        if (!opened || index < 0 || entries[index].selectable !== true) return false
        selectedChildId = id
        childSelected(id)
        return true
    }
    function moveSelection(delta) {
        if (!entries.length) return
        const index = Math.max(0, Math.min(entries.length - 1, indexForId(selectedChildId) + delta))
        selectedChildId = String(entries[index].id)
        const point = geometry.entries[index]
        viewport.contentX = Math.max(0, Math.min(point.x, viewport.contentWidth - viewport.width))
        viewport.contentY = Math.max(0, Math.min(point.y, viewport.contentHeight - viewport.height))
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
        }
        QQC2.Label {
            id: layoutNotice
            width: parent.width
            visible: root.geometry.fallbackApplied
            text: qsTr("This saved layout uses Fan.")
            wrapMode: Text.Wrap
        }
        QQC2.Label {
            id: emptyNotice
            width: parent.width
            visible: root.entries.length === 0
            text: root.snapshot.status === "empty" ? qsTr("This folder is empty.")
                : qsTr("This folder is unavailable.")
            wrapMode: Text.Wrap
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
            QQC2.ScrollBar.horizontal: QQC2.ScrollBar {}
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
            Repeater {
                model: root.entries
                delegate: Item {
                    id: child
                    required property var modelData
                    required property int index
                    readonly property var point: root.geometry.entries[index] || ({ x: 0, y: 0 })
                    objectName: "folder-child-" + index
                    x: point.x
                    y: point.y
                    width: root.geometry.iconSize
                    height: width
                    Accessible.role: Accessible.Button
                    Accessible.name: String(modelData.displayName || modelData.name || "")
                    Accessible.onPressAction: root.selectChild(String(modelData.id))
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: root.selectedChildId === String(child.modelData.id) ? "#406ca5dd" : "transparent"
                        border.color: root.selectedChildId === String(child.modelData.id) ? "#8fbaff" : "transparent"
                    }
                    IconMotionController {
                        id: motion
                        profiles: root.openingProfiles
                        reducedMotion: root.reducedMotion
                        revealed: root.opened
                        sceneVisible: root.visible && root.opened
                        entryIndex: child.index
                    }
                    IconScene {
                        anchors.fill: parent
                        entry: child.modelData
                        logicalSize: child.width
                        iconStyleDefinition: root.iconStyleDefinition
                        showIndicator: false
                        disabled: child.modelData.selectable !== true
                        hovered: pointer.containsMouse
                        reducedMotion: root.reducedMotion
                        glyphMotion: MotionChannels.motionFor(motion.channels, "icon", { size: child.width })
                    }
                    MouseArea {
                        id: pointer
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: root.selectedChildId = String(child.modelData.id)
                        onClicked: root.selectChild(String(child.modelData.id))
                    }
                    QQC2.ToolTip.visible: pointer.containsMouse
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
            visible: root.entries.length > 0
        }
        QQC2.Label {
            id: pageNotice
            width: parent.width
            visible: root.snapshot.truncated === true
            text: qsTr("Showing the first 48 items.")
            wrapMode: Text.Wrap
        }
    }
}
