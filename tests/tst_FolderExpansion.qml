import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtTest
import ArchDock.Rendering 1.0
import org.kde.plasma.core as PlasmaCore
import "../plasma-dock-widget/contents/ui" as DockUi

TestCase {
    id: testCase
    name: "FolderExpansion"
    when: windowShown
    visible: true
    width: 700
    height: 500
    Component { id: component; FolderExpansion {} }
    Component { id: hostComponent; DockUi.FolderExpansionHost {} }
    Component {
        id: anchorComponent
        Window {
            width: 1000; height: 600; visible: true
            Item { objectName: "anchor"; x: 468; y: 400; width: 56; height: 56 }
        }
    }
    SignalSpy { id: selection; signalName: "childSelected" }
    SignalSpy { id: dismissal; signalName: "dismissRequested" }
    function rows(count) {
        const result = []
        for (let i = 0; i < count; ++i)
            result.push({ id: "child-" + i, displayName: "Document " + i,
                iconName: "text-plain", selectable: i !== 2 })
        return result
    }
    function popup(layout, count, reduced) {
        const item = createTemporaryObject(component, testCase, {
            layout: layout, reducedMotion: reduced === undefined ? true : reduced,
            folderTitle: "Documents", snapshot: { status: "ready", entries: rows(count), truncated: count === 48 }
        })
        verify(item !== null)
        item.width = item.implicitWidth
        item.height = item.implicitHeight
        verify(waitForRendering(item))
        selection.target = item; selection.clear()
        dismissal.target = item; dismissal.clear()
        return item
    }
    function cleanup() { selection.target = null; dismissal.target = null }
    function test_transparentContents() {
        const item = popup("fan", 5)
        compare(item.background, null, "folder contents have no opaque Pane background")
    }
    function test_namesRemainVisibleWithoutHoverAndCanBeHidden() {
        const item = popup("fan", 5)
        const name = findChild(item, "folder-name-0")
        verify(name !== null)
        compare(name.text, "Document 0")
        verify(name.visible)
        item.showNames = false
        tryCompare(name, "visible", false)
        item.showNames = true
        tryCompare(name, "visible", true)
    }
    function test_verticalFanWheelAndDragDoNotLaunchChildren() {
        const item = popup("fan", 48)
        const viewport = findChild(item, "folderViewport")
        verify(viewport.contentWidth <= viewport.width,
               "a dense fan is reachable with vertical scrolling alone")
        verify(viewport.contentHeight > viewport.height)
        mouseWheel(viewport, viewport.width / 2, 30, 0, -120)
        tryVerify(function() { return viewport.contentY > 0 })
        mouseWheel(viewport, viewport.width / 2, 30, 0, 120)
        tryCompare(viewport, "contentY", 0)
        const first = findChild(item, "folder-child-0")
        const start = first.mapToItem(viewport, first.width / 2, first.height / 2)
        mousePress(viewport, start.x, start.y, Qt.LeftButton)
        mouseMove(viewport, start.x, start.y - 16, 20, Qt.LeftButton)
        mouseMove(viewport, start.x, start.y - 32, 20, Qt.LeftButton)
        tryCompare(viewport, "dragging", true)
        mouseMove(viewport, start.x, start.y - 140, 20, Qt.LeftButton)
        mouseRelease(viewport, start.x, start.y - 140, Qt.LeftButton)
        tryVerify(function() { return viewport.contentY > 0 })
        compare(selection.count, 0, "scroll dragging never opens the pressed child")
        tryCompare(viewport, "moving", false)
        item.forceActiveFocus()
        for (let i = 0; i < 47; ++i) keyClick(Qt.Key_Right)
        compare(item.selectedChildId, "child-47")
        const last = findChild(item, "folder-child-47")
        const point = last.mapToItem(viewport, last.width / 2, 20)
        verify(point.x >= 0 && point.x < viewport.width)
        verify(point.y >= 0 && point.y < viewport.height)
    }
    function test_layoutSelection_data() {
        return ["fan", "grid", "stack", "arc", "ring"].map(function(layout) {
            return { tag: layout, layout: layout }
        })
    }
    function test_layoutSelection(data) {
        const item = popup(data.layout, 5)
        compare(item.geometry.layout, data.layout)
        const first = findChild(item, "folder-child-0")
        mouseClick(first, 8, 8)
        compare(selection.count, 1)
        compare(selection.signalArguments[0][0], "child-0")
        item.forceActiveFocus()
        keyClick(Qt.Key_Right)
        compare(item.selectedChildId, "child-1")
        keyClick(Qt.Key_Return)
        compare(selection.count, 2)
        compare(selection.signalArguments[1][0], "child-1")
        keyClick(Qt.Key_Right)
        keyClick(Qt.Key_Return)
        compare(selection.count, 2, "blocked child never dispatches")
        verify(!item.selectChild("foreign"))
        keyClick(Qt.Key_Escape)
        compare(dismissal.count, 1)
        item.snapshot = { status: "empty", entries: [] }
        compare(item.selectedChildId, "")
        verify(!item.selectChild("child-1"), "removed children cannot dispatch")
    }
    function test_denseFallbackAndReducedMotion() {
        const item = popup("spiral", 48)
        verify(item.geometry.fallbackApplied)
        compare(item.geometry.layout, "fan")
        verify(item.width <= 640 && item.height <= 420)
        for (let i = 0; i < 47; ++i) item.moveSelection(1)
        compare(item.selectedChildId, "child-47")
        verify(findChild(item, "folderViewport").contentY > 0)
        const child = findChild(item, "folder-child-47")
        verify(child !== null)
        verify(item.selectChild("child-47"))
        item.opened = false
        verify(!item.selectChild("child-47"))
        compare(item.openingProfiles[0].tracks[0].duration, 260)
        item.duration = 9999
        compare(item.openingProfiles[0].tracks[0].duration, 1200)
    }
    function test_motionUsesSharedController() {
        const item = popup("grid", 1, false)
        const child = findChild(item, "folder-child-0")
        const controller = findChild(child, "folder-motion-0")
        tryCompare(controller, "activeProfileIds", ["folder-open"])
        item.reducedMotion = true
        tryCompare(controller, "activeTracks", [])
    }
    function test_horizontalWheelAndShiftRespectBounds() {
        const item = popup("stack", 48)
        const viewport = findChild(item, "folderViewport")
        verify(viewport.contentWidth > viewport.width)
        mouseWheel(viewport, 100, 20, -120, 0)
        tryVerify(function() { return viewport.contentX > 0 }, 500)
        tryCompare(viewport, "moving", false, 1000)
        const x = viewport.contentX
        mouseWheel(viewport, 100, 20, 0, -120, Qt.NoButton, Qt.ShiftModifier)
        tryVerify(function() { return viewport.contentX > x }, 500)
        viewport.contentX = viewport.contentWidth - viewport.width
        const end = viewport.contentX
        mouseWheel(viewport, 100, 20, -120, 0)
        wait(100)
        compare(viewport.contentX, end)
        const small = popup("grid", 1)
        const fitted = findChild(small, "folderViewport")
        mouseWheel(fitted, 20, 20, -120, 0)
        wait(100)
        compare(fitted.contentX, 0)
    }
    function test_compactArcFollowsCurveWithoutChrome() {
        const item = popup("arc", 48)
        const viewport = findChild(item, "folderViewport")
        compare(viewport.ScrollBar.horizontal, null)
        compare(viewport.ScrollBar.vertical, null)
        verify(viewport.contentWidth <= viewport.width)
        verify(item.width < 300, "a long folder never expands its arc across the desktop")
        const child = findChild(item, "folder-child-1")
        const startX = child.x
        const startY = child.mapToItem(viewport, 0, 0).y
        mouseWheel(viewport, viewport.width / 2, 30, 0, -120)
        tryVerify(function() { return viewport.contentY > 0 })
        verify(Math.abs(child.x - startX) > 1, "scrolling follows the curve, not a straight translation")
        verify(child.mapToItem(viewport, 0, 0).y < startY)
        mouseWheel(viewport, viewport.width / 2, 30, 0, 120)
        tryCompare(viewport, "contentY", 0)
        fuzzyCompare(child.x, startX, 0.01)
        item.forceActiveFocus()
        for (let i = 0; i < 47; ++i) keyClick(Qt.Key_Right)
        compare(item.selectedChildId, "child-47")
        verify(viewport.contentY > 0)
        const last = findChild(item, "folder-child-47")
        const point = last.mapToItem(viewport, last.width / 2, last.height / 2)
        verify(point.x >= 0 && point.x < viewport.width)
        verify(point.y >= 0 && point.y < viewport.height)
    }
    function test_contentsUnfoldFromOriginAndReducedMotionIsImmediate() {
        const item = createTemporaryObject(component, testCase, {
            opened: false, reducedMotion: false, duration: 500,
            expansionOrigin: Qt.point(30, 300),
            snapshot: { status: "ready", entries: rows(5) }
        })
        verify(item !== null)
        item.width = item.implicitWidth
        item.height = item.implicitHeight
        const first = findChild(item, "folder-child-0")
        const last = findChild(item, "folder-child-4")
        const origin = item.mapToItem(testCase, 30, 300)
        const firstStart = first.mapToItem(testCase, first.width / 2, 28)
        const lastStart = last.mapToItem(testCase, last.width / 2, 28)
        fuzzyCompare(firstStart.x, origin.x, 0.01)
        fuzzyCompare(firstStart.y, origin.y, 0.01)
        fuzzyCompare(lastStart.x, origin.x, 0.01)
        fuzzyCompare(lastStart.y, origin.y, 0.01)
        item.opened = true
        tryVerify(function() { return item.openingProgress > 0 && item.openingProgress < 1 })
        verify(!item.selectChild("child-0"), "moving contents cannot accidentally launch")
        tryCompare(item, "openingInProgress", false)
        compare(item.openingProgress, 1)
        const firstEnd = first.mapToItem(testCase, first.width / 2, 28)
        const lastEnd = last.mapToItem(testCase, last.width / 2, 28)
        verify(Math.abs(lastEnd.y - firstEnd.y) > 200)
        verify(item.selectChild("child-0"))
        item.opened = false
        compare(item.openingProgress, 0, "a hidden popup resets to the clicked origin immediately")
        item.opened = true
        tryVerify(function() { return item.openingProgress > 0 && item.openingProgress < 1 })
        tryCompare(item, "openingInProgress", false)
        compare(item.openingProgress, 1)
        item.opened = false
        item.reducedMotion = true
        item.opened = true
        compare(item.openingProgress, 1)
        verify(!item.openingInProgress)
        verify(item.selectChild("child-0"))
    }
    function test_nativeHostStaysInsideAnchorScreen() {
        const window = createTemporaryObject(anchorComponent, null)
        const host = createTemporaryObject(hostComponent, testCase, {
            visualParent: findChild(window, "anchor"), reducedMotion: true,
            folderLayout: "arc", snapshot: { status: "ready", entries: rows(48) }
        })
        verify(host.openFolder())
        const screen = window.screen
        host.x = screen.virtualX - 80
        host.y = screen.virtualY - 80
        tryVerify(function() { return host.x >= screen.virtualX && host.y >= screen.virtualY })
        host.x = screen.virtualX + screen.width - 4
        host.y = screen.virtualY + screen.height - 4
        tryVerify(function() { return host.x + host.width <= screen.virtualX + screen.width
            && host.y + host.height <= screen.virtualY + screen.height })
    }
    function test_nativeHostLifecycle() {
        const window = createTemporaryObject(anchorComponent, null)
        verify(window !== null)
        const host = createTemporaryObject(hostComponent, testCase, {
            visualParent: findChild(window, "anchor"), reducedMotion: true,
            snapshot: { status: "empty", entries: [] }, folderTitle: "Empty folder"
        })
        verify(host !== null)
        compare(host.type, PlasmaCore.Dialog.AppletPopup)
        compare(host.backgroundHints, PlasmaCore.Dialog.NoBackground)
        compare(host.color.a, 0)
        verify(host.openFolder())
        tryCompare(host, "visible", true)
        host.mainItem.dismissRequested()
        tryCompare(host, "visible", false)
        verify(!host.requested)
        verify(host.openFolder())
        host.interactionAllowed = false
        tryCompare(host, "visible", false)
        verify(!host.requested)
        verify(!host.openFolder())
    }
}
