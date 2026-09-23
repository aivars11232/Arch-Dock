import QtQuick
import QtQuick.Controls as QQC2

// Shared content only. The host supplies positioning, optional thumbnails,
// and the action callback; this component never talks to the desktop.
QQC2.Pane {
    id: root

    property var windows: []
    property string applicationTitle: ""
    property string selectedWindowId: ""
    property Component thumbnailDelegate: null
    readonly property int windowCount: windows ? windows.length : 0
    signal activateRequested(string windowId)
    signal actionRequested(string windowId, string action)
    signal dismissRequested()

    objectName: "windowPreviewPopup"
    implicitWidth: 340
    implicitHeight: heading.implicitHeight + Math.min(windowList.contentHeight, 320) + 24
    padding: 8
    focus: true
    activeFocusOnTab: true

    function indexForId(windowId) {
        const current = windows || []
        for (let index = 0; index < current.length; ++index) {
            if (String(current[index].windowId) === windowId)
                return index
        }
        return -1
    }

    function activateWindow(windowId) {
        const index = indexForId(windowId)
        if (index < 0 || windows[index].canActivate !== true)
            return false
        selectedWindowId = windowId
        activateRequested(windowId)
        return true
    }

    function moveSelection(delta) {
        if (windowCount === 0)
            return
        const previous = indexForId(selectedWindowId)
        const next = Math.max(0, Math.min(windowCount - 1, previous + delta))
        selectedWindowId = String(windows[next].windowId)
        windowList.positionViewAtIndex(next, ListView.Contain)
    }

    function requestWindowAction(windowId, action) {
        if (action === "activate")
            return activateWindow(windowId)
        const index = indexForId(windowId)
        if (index < 0)
            return false
        const window = windows[index]
        const allowed = action === "close" ? window.canClose === true
            : action === "minimize" ? window.canMinimize === true && !window.minimized
            : action === "restore" && window.canMinimize === true && window.minimized
        if (!allowed)
            return false
        actionRequested(windowId, action)
        return true
    }

    onWindowsChanged: {
        if (!windows || windows.length === 0) {
            selectedWindowId = ""
            dismissRequested()
        } else if (indexForId(selectedWindowId) < 0) {
            selectedWindowId = String(windows[0].windowId)
        }
    }
    Keys.onEscapePressed: event => {
        dismissRequested()
        event.accepted = true
    }
    Keys.onUpPressed: event => { moveSelection(-1); event.accepted = true }
    Keys.onDownPressed: event => { moveSelection(1); event.accepted = true }
    Keys.onReturnPressed: event => { activateWindow(selectedWindowId); event.accepted = true }
    Keys.onEnterPressed: event => { activateWindow(selectedWindowId); event.accepted = true }

    contentItem: Column {
        spacing: 8
        QQC2.Label {
            id: heading
            width: parent.width
            text: root.applicationTitle
            textFormat: Text.PlainText
            font.bold: true
            elide: Text.ElideRight
        }
        ListView {
            id: windowList
            objectName: "windowPreviewList"
            width: parent.width
            height: Math.min(contentHeight, 320)
            model: root.windows
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

            delegate: QQC2.ItemDelegate {
                id: row
                required property var modelData
                width: windowList.width
                height: 52
                objectName: "window-preview-" + String(modelData.windowId)
                text: String(modelData.title || qsTr("Untitled window"))
                    + (modelData.minimized ? qsTr(" — Minimized")
                       : modelData.active ? qsTr(" — Active") : "")
                enabled: modelData.canActivate === true || modelData.canMinimize === true
                    || modelData.canClose === true
                highlighted: String(modelData.windowId) === root.selectedWindowId
                Accessible.name: text
                onClicked: root.activateWindow(String(modelData.windowId))

                contentItem: Row {
                    spacing: 8
                    Loader {
                        id: thumbnail
                        readonly property string thumbnailWindowId: String(row.modelData.windowId)
                        // PipeWire pauses an invisible item. Allow its first
                        // frame to arrive before reserving thumbnail space.
                        width: item && item.ready === true ? 72 : 0
                        height: 40
                        active: root.visible && root.thumbnailDelegate !== null
                        sourceComponent: root.thumbnailDelegate
                        onLoaded: {
                            if (item && item.windowId !== undefined)
                                item.windowId = Qt.binding(function() { return thumbnail.thumbnailWindowId })
                        }
                        visible: status === Loader.Ready && item !== null
                    }
                    QQC2.Label {
                        width: Math.max(0, parent.width
                            - (thumbnail.width > 0 ? thumbnail.width + 8 : 0)
                            - (actionButtons.visible ? actionButtons.width + 8 : 0))
                        height: parent.height
                        text: row.text
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    Row {
                        id: actionButtons
                        height: parent.height
                        spacing: 2
                        visible: stateAction.visible || closeAction.visible
                        QQC2.ToolButton {
                            id: stateAction
                            objectName: "window-state-" + String(row.modelData.windowId)
                            width: 36; height: 40
                            visible: row.modelData.canMinimize === true
                            icon.name: row.modelData.minimized ? "window-restore" : "window-minimize"
                            text: row.modelData.minimized ? qsTr("Restore") : qsTr("Minimize")
                            display: QQC2.AbstractButton.IconOnly
                            Accessible.name: text + " " + String(row.modelData.title || "")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: text
                            onClicked: root.requestWindowAction(String(row.modelData.windowId),
                                row.modelData.minimized ? "restore" : "minimize")
                        }
                        QQC2.ToolButton {
                            id: closeAction
                            objectName: "window-close-" + String(row.modelData.windowId)
                            width: 36; height: 40
                            visible: row.modelData.canClose === true
                            icon.name: "window-close"
                            text: qsTr("Close")
                            display: QQC2.AbstractButton.IconOnly
                            Accessible.name: text + " " + String(row.modelData.title || "")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: text
                            onClicked: root.requestWindowAction(String(row.modelData.windowId), "close")
                        }
                    }
                }
            }
        }
    }
}
