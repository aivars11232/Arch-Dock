import QtQuick
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
    SignalSpy { id: dismissal; signalName: "dismissRequested" }

    function rows() {
        return [
            { windowId: "one", title: "First document", active: true, minimized: false, canActivate: true },
            { windowId: "two", title: "Second document", active: false, minimized: true, canActivate: true }
        ]
    }

    function popup() {
        const item = createTemporaryObject(popupComponent, testCase, {
            width: 340, applicationTitle: "Editor", windows: rows()
        })
        verify(item !== null)
        verify(waitForRendering(item))
        activation.target = item
        dismissal.target = item
        activation.clear()
        dismissal.clear()
        return item
    }

    function cleanup() {
        activation.target = null
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
