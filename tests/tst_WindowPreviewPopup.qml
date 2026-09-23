import QtQuick
import QtQuick.Window
import QtTest
import ArchDock.Rendering 1.0
import org.kde.plasma.core as PlasmaCore
import "../plasma-dock-widget/contents/ui" as DockUi

TestCase {
    id: testCase
    name: "WindowPreviewPopup"
    when: windowShown
    visible: true
    width: 400
    height: 400

    Component { id: popupComponent; WindowPreviewPopup {} }
    Component { id: hostComponent; DockUi.WindowPreviewHost { thumbnailsEnabled: false } }
    Component {
        id: anchorWindowComponent
        Window {
            width: 1000; height: 600
            flags: Qt.Tool | Qt.FramelessWindowHint
            visible: true
            Item { objectName: "previewAnchor"; x: 468; y: 268; width: 64; height: 64 }
        }
    }
    Component {
        id: thumbnailAdapter
        Item {
            id: adapter
            objectName: "thumbnail-" + windowId
            property string windowId: ""
            property bool ready: false
            // Match the platform boundary: a hidden stream cannot deliver
            // the first frame. No image or compositor success is simulated.
            Timer {
                interval: 1
                running: adapter.visible && !adapter.ready && adapter.windowId.length > 0
                onTriggered: adapter.ready = true
            }
        }
    }
    Component {
        id: controllerComponent
        PanelPresentationController { restingState: "open"; transitionDuration: 0 }
    }
    SignalSpy { id: activation; signalName: "activateRequested" }
    SignalSpy { id: windowAction; signalName: "actionRequested" }
    SignalSpy { id: dismissal; signalName: "dismissRequested" }

    function rows() {
        return [
            { windowId: "one", title: "First document", active: true, minimized: false,
                canActivate: true, canMinimize: true, canClose: true },
            { windowId: "two", title: "Second document", active: false, minimized: true,
                canActivate: true, canMinimize: true, canClose: true }
        ]
    }

    function popup() {
        const item = createTemporaryObject(popupComponent, testCase, {
            width: 340, applicationTitle: "Editor", windows: rows()
        })
        verify(item !== null)
        verify(waitForRendering(item))
        activation.target = item
        windowAction.target = item
        dismissal.target = item
        activation.clear()
        windowAction.clear()
        dismissal.clear()
        return item
    }

    function cleanup() {
        activation.target = null
        windowAction.target = null
        dismissal.target = null
    }

    function test_titleFallbackIsUsableWithoutThumbnails() {
        const item = popup()
        compare(item.thumbnailDelegate, null)
        compare(item.windowCount, 2)
        tryVerify(function() { return findChild(item, "window-preview-two") !== null })
        const second = findChild(item, "window-preview-two")
        verify(second.text.indexOf("Second document") >= 0)
        verify(second.text.indexOf("Minimized") >= 0)
        mouseClick(second, second.width / 2, second.height / 2)
        compare(activation.count, 1)
        compare(activation.signalArguments[0][0], "two")
    }

    function test_selectionSurvivesUpdatesAndRejectsRemovedIds() {
        const item = popup()
        verify(item.activateWindow("two"))
        activation.clear()
        const changed = rows()
        changed[1].title = "Renamed document"
        item.windows = [changed[1], changed[0]]
        compare(item.selectedWindowId, "two")
        tryVerify(function() {
            const row = findChild(item, "window-preview-two")
            return row !== null && row.text.indexOf("Renamed document") >= 0
        })
        item.windows = [changed[0]]
        compare(item.selectedWindowId, "one")
        verify(!item.activateWindow("two"))
        compare(activation.count, 0)
        item.windows = []
        compare(item.windowCount, 0)
        compare(dismissal.count, 1)
        verify(!item.activateWindow("one"))
        compare(activation.count, 0)
    }

    function test_rapidReplacementRejectsStaleActions() {
        const item = popup()
        let removedId = "two"
        for (let index = 0; index < 16; ++index) {
            const next = rows()[1]
            next.windowId = "replacement-" + index
            next.title = ""
            item.windows = [rows()[0], next]
            verify(!item.activateWindow(removedId))
            verify(!item.requestWindowAction(removedId, "close"))
            verify(item.activateWindow(next.windowId))
            compare(item.selectedWindowId, next.windowId)
            removedId = next.windowId
        }
        compare(activation.count, 16)
        compare(windowAction.count, 0)
        verify(waitForRendering(item))
        const last = findChild(item, "window-preview-" + removedId)
        verify(last !== null && last.text.indexOf("Untitled window") >= 0)
        item.windows = []
        compare(dismissal.count, 1)
        verify(!item.requestWindowAction(removedId, "close"))
    }

    function test_actionButtonsTargetExactIdsWithoutActivatingTheRow() {
        const item = popup()
        const firstState = findChild(item, "window-state-one")
        verify(firstState !== null && firstState.visible)
        mouseClick(firstState, firstState.width / 2, firstState.height / 2)
        compare(windowAction.count, 1)
        compare(windowAction.signalArguments[0].length, 2)
        compare(windowAction.signalArguments[0][0], "one")
        compare(windowAction.signalArguments[0][1], "minimize")
        compare(activation.count, 0)
        const secondState = findChild(item, "window-state-two")
        verify(secondState !== null && secondState.visible)
        mouseClick(secondState, secondState.width / 2, secondState.height / 2)
        compare(windowAction.signalArguments[1].length, 2)
        compare(windowAction.signalArguments[1][0], "two")
        compare(windowAction.signalArguments[1][1], "restore")
        const secondClose = findChild(item, "window-close-two")
        verify(secondClose !== null && secondClose.visible)
        mouseClick(secondClose, secondClose.width / 2, secondClose.height / 2)
        compare(windowAction.count, 3)
        compare(windowAction.signalArguments[2].length, 2)
        compare(windowAction.signalArguments[2][0], "two")
        compare(windowAction.signalArguments[2][1], "close")
        compare(activation.count, 0)
        item.windows = [rows()[0]]
        verify(!item.requestWindowAction("two", "close"))
        verify(!item.requestWindowAction("one", "restore"))
        verify(!item.requestWindowAction("one", "unknown"))
        const unavailable = rows()[0]
        unavailable.canMinimize = false
        unavailable.canClose = false
        item.windows = [unavailable]
        verify(waitForRendering(item))
        verify(!findChild(item, "window-state-one").visible)
        verify(!findChild(item, "window-close-one").visible)
        verify(!item.requestWindowAction("one", "minimize"))
        verify(!item.requestWindowAction("one", "close"))
        compare(windowAction.count, 3)
    }

    function test_thumbnailCanReceiveItsFirstFrameBeforeReady() {
        const item = popup()
        item.thumbnailDelegate = thumbnailAdapter
        tryVerify(function() {
            const adapter = findChild(item, "thumbnail-two")
            return adapter !== null && adapter.ready
        })
        const adapter = findChild(item, "thumbnail-two")
        compare(adapter.windowId, "two")
        verify(adapter.visible)
        compare(adapter.width, 72)
        item.thumbnailDelegate = null
        tryVerify(function() { return findChild(item, "thumbnail-two") === null })
        verify(item.activateWindow("two"))
    }

    function test_unavailableActivationAndKeyboardDismissal() {
        const item = popup()
        const changed = rows()
        changed[0].canActivate = false
        item.windows = changed
        verify(!item.activateWindow("one"))
        item.forceActiveFocus()
        tryCompare(item, "activeFocus", true)
        keyClick(Qt.Key_Down)
        compare(item.selectedWindowId, "two")
        keyClick(Qt.Key_Return)
        compare(activation.count, 1)
        compare(activation.signalArguments[0][0], "two")
        keyClick(Qt.Key_Escape)
        compare(dismissal.count, 1)
    }

    function test_hostLocations_data() {
        return [
            { tag: "top", edge: "top", normal: { x: 0, y: 1 }, location: PlasmaCore.Types.TopEdge },
            { tag: "bottom", edge: "bottom", normal: { x: 0, y: -1 }, location: PlasmaCore.Types.BottomEdge },
            { tag: "left", edge: "left", normal: { x: 1, y: 0 }, location: PlasmaCore.Types.LeftEdge },
            { tag: "right", edge: "right", normal: { x: -1, y: 0 }, location: PlasmaCore.Types.RightEdge },
            { tag: "free-ring-right", edge: "free", normal: { x: 1, y: 0 }, location: PlasmaCore.Types.LeftEdge },
            { tag: "free-arc-up", edge: "free", normal: { x: 0, y: -1 }, location: PlasmaCore.Types.BottomEdge }
        ]
    }

    function test_hostLocations(data) {
        const host = createTemporaryObject(hostComponent, testCase, {
            visualParent: testCase, panelEdge: data.edge, outwardNormal: data.normal
        })
        verify(host !== null)
        compare(host.location, data.location)
    }

    function test_liveHostPlacement_data() { return test_hostLocations_data() }

    function test_liveHostPlacement(data) {
        if (Qt.platform.pluginName !== "wayland")
            skip("requires the disposable Wayland rendering-import-smoke session")
        const window = createTemporaryObject(anchorWindowComponent, testCase)
        verify(window !== null)
        const anchor = findChild(window, "previewAnchor")
        verify(waitForRendering(anchor))
        const host = createTemporaryObject(hostComponent, testCase, {
            visualParent: anchor, panelEdge: data.edge, outwardNormal: data.normal,
            windowEntries: rows(), anchorHovered: true
        })
        verify(host !== null)
        verify(host.openPreview(true))
        tryCompare(host, "visible", true)
        verify(waitForRendering(host.mainItem))
        // Native Dialog owns placement. Observe its rendered window geometry,
        // not merely the edge enum supplied to it.
        tryVerify(function() {
            const center = anchor.mapToGlobal(anchor.width / 2, anchor.height / 2)
            const screen = host.mainItem.Screen
            const dx = host.x + host.width / 2 - center.x
            const dy = host.y + host.height / 2 - center.y
            return dx * data.normal.x + dy * data.normal.y > 0
                && host.width > 0 && host.height > 0
                && screen.width > 0 && screen.height > 0
                && host.x >= screen.virtualX
                && host.y >= screen.virtualY
                && host.x + host.width <= screen.virtualX + screen.width
                && host.y + host.height <= screen.virtualY + screen.height
        })
        host.closePreview()
        tryCompare(host, "visible", false)
        window.close()
    }

    function test_visibleHostHoldsGuardAndLastWindowReleasesIt() {
        const host = createTemporaryObject(hostComponent, testCase, {
            visualParent: testCase, windowEntries: rows(), anchorHovered: true
        })
        verify(host !== null)
        const controller = createTemporaryObject(controllerComponent, testCase)
        verify(controller !== null)
        controller.windowPreviewOpen = Qt.binding(function() { return host.visible })
        verify(host.openPreview(false))
        tryCompare(host, "visible", true)
        tryCompare(controller, "windowPreviewOpen", true)
        controller.requestCollapse()
        compare(controller.phase, "open")
        compare(controller.deferredRequest, "collapse")
        host.windowEntries = []
        tryCompare(host, "visible", false)
        tryCompare(controller, "phase", "collapsed")
        compare(host.requested, false)
        host.windowEntries = rows()
        verify(!host.visible)
        host.interactionAllowed = false
        verify(!host.openPreview(true))
    }
}
