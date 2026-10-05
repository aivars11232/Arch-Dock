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
    // The reference folder for the half-circle contract: 43 children in the
    // 640 x 420 popup a free panel opens, with and without names.
    function referenceFolder(layout, names) {
        const item = createTemporaryObject(component, testCase, {
            layout: layout, reducedMotion: true, folderTitle: "Reference",
            showNames: names, maximumWidth: 640, maximumHeight: 420,
            snapshot: { status: "ready", entries: rows(43), truncated: false }
        })
        verify(item !== null)
        item.width = item.implicitWidth
        item.height = item.implicitHeight
        verify(waitForRendering(item))
        selection.target = item; selection.clear()
        dismissal.target = item; dismissal.clear()
        return item
    }
    // The children a person can see and press, in order, with their centres
    // in viewport coordinates.
    function childrenOnPath(item, viewport) {
        const result = []
        for (let index = 0; index < item.entries.length; ++index) {
            const child = findChild(item, "folder-child-" + index)
            if (child.opacity < 0.999)
                continue
            const corner = child.mapToItem(viewport, 0, 0)
            result.push({ index: index, x: corner.x + child.width / 2,
                          y: corner.y + child.height / 2 })
        }
        return result
    }
    // First and last stand on one vertical diameter, every child is on the
    // circle through them, and the curve bulges by its radius: no straight run.
    function verifyHalfCircle(children, message) {
        verify(children.length >= 3, message + ": at least three children show a curve")
        const first = children[0]
        const last = children[children.length - 1]
        const radius = (last.y - first.y) / 2
        verify(radius > 40, message + ": the path has a real radius, " + radius)
        fuzzyCompare(first.x, last.x, 0.5)
        const centre = { x: first.x, y: (first.y + last.y) / 2 }
        let reach = 0
        for (const child of children) {
            fuzzyCompare(Math.hypot(child.x - centre.x, child.y - centre.y), radius, 0.5)
            verify(child.x >= centre.x - 0.5, message + ": no child is behind the diameter")
            reach = Math.max(reach, child.x - centre.x)
        }
        verify(reach >= radius * Math.cos(Math.PI / (children.length - 1) / 2) - 0.5,
               message + ": the curve reaches " + reach + " of radius " + radius)
        for (let slot = 1; slot < children.length; ++slot)
            verify(children[slot].index === children[slot - 1].index + 1, message + ": logical order")
    }
    function test_referenceFolderFollowsAnExactHalfCircle_data() {
        return [
            { tag: "arc/names", layout: "arc", names: true },
            { tag: "fan/names", layout: "fan", names: true },
            { tag: "arc/icons", layout: "arc", names: false },
            { tag: "fan/icons", layout: "fan", names: false }
        ]
    }
    function test_referenceFolderFollowsAnExactHalfCircle(data) {
        const item = referenceFolder(data.layout, data.names)
        const viewport = findChild(item, "folderViewport")
        verify(item.width <= 640 && item.height <= 420)
        verify(viewport.contentWidth <= viewport.width, "one scrolling axis")
        const resting = childrenOnPath(item, viewport)
        compare(resting[0].index, 0)
        verifyHalfCircle(resting, "at rest")
        for (const child of resting) {
            verify(child.x >= 0 && child.x < viewport.width, "child " + child.index + " is inside the popup")
            verify(child.y >= 0 && child.y < viewport.height, "child " + child.index + " is inside the popup")
        }
        compare(findChild(item, "folder-child-" + resting.length).opacity, 0,
                "the child beyond the end of the path is not shown")

        // One wheel notch moves every child one slot along the same curve,
        // in both directions, and stops at the ends.
        const middle = Qt.point(viewport.width / 2, viewport.height / 2)
        mouseWheel(viewport, middle.x, middle.y, 0, -120)
        tryVerify(function() { return childrenOnPath(item, viewport)[0].index === 1 })
        const moved = childrenOnPath(item, viewport)
        compare(moved.length, resting.length)
        for (let slot = 0; slot < moved.length; ++slot) {
            compare(moved[slot].index, resting[slot].index + 1)
            fuzzyCompare(moved[slot].x, resting[slot].x, 0.5)
            fuzzyCompare(moved[slot].y, resting[slot].y, 0.5)
        }
        mouseWheel(viewport, middle.x, middle.y, 0, 120)
        tryVerify(function() { return childrenOnPath(item, viewport)[0].index === 0 })
        mouseWheel(viewport, middle.x, middle.y, 0, 120)
        wait(50)
        compare(childrenOnPath(item, viewport)[0].index, 0, "the first child stops at the top end")

        // Hover and press targets are the curved positions.
        const hovered = childrenOnPath(item, viewport)[2]
        mouseMove(viewport, hovered.x, hovered.y - 2)
        mouseMove(viewport, hovered.x, hovered.y)
        tryCompare(item, "selectedChildId", "child-" + hovered.index)
        const pressed = childrenOnPath(item, viewport)[1]
        mouseClick(viewport, pressed.x, pressed.y)
        compare(selection.count, 1)
        compare(selection.signalArguments[0][0], "child-" + pressed.index)

        // Keyboard selection walks past the window and stays on the path.
        item.forceActiveFocus()
        for (let step = 0; step < 20; ++step)
            keyClick(Qt.Key_Down)
        const selected = Number(item.selectedChildId.replace("child-", ""))
        verify(selected > resting.length, "the selection left the first window: " + selected)
        const walked = childrenOnPath(item, viewport)
        verify(walked.some(function(child) { return child.index === selected }),
               "the selected child is on the path")
        verifyHalfCircle(walked, "after keyboard navigation")

        // The far end: the last child stops on the bottom end of the curve.
        for (let notch = 0; notch < 60; ++notch)
            mouseWheel(viewport, middle.x, middle.y, 0, -120)
        tryVerify(function() {
            const shown = childrenOnPath(item, viewport)
            return shown.length > 0 && shown[shown.length - 1].index === 42
        })
        const end = childrenOnPath(item, viewport)
        compare(end.length, resting.length)
        verifyHalfCircle(end, "at the end")
        compare(selection.count, 1, "browsing never opens a child")
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
            folderItem: findChild(window, "anchor"), reducedMotion: true,
            folderLayout: "arc", snapshot: { status: "ready", entries: rows(48) }
        })
        verify(host.openFolder())
        const screen = window.screen
        const anchor = findChild(window, "anchor")
        host.closeFolder()
        host.panelEdge = "free"
        window.x = screen.virtualX
        window.y = screen.virtualY
        for (const y of [10, screen.height - 70]) {
            anchor.y = y
            verify(host.openFolder())
            tryVerify(function() { return host.y >= screen.virtualY
                && host.y + host.height <= screen.virtualY + screen.height })
            host.closeFolder()
        }
    }
    // The popup opens against the clicked icon on the side that faces out of
    // the dock, with the folder on the contents' anchor; a native panel opens
    // away from its screen edge.
    function test_hostOpensAgainstTheFolderOnItsOutwardSide_data() {
        const rows = []
        for (const layout of ["fan", "grid", "stack", "arc", "ring"]) {
            rows.push({ tag: layout + "/up", layout: layout, edge: "free", normal: { x: 0, y: -1 }, side: "top" })
            rows.push({ tag: layout + "/down", layout: layout, edge: "free", normal: { x: 0, y: 1 }, side: "bottom" })
            rows.push({ tag: layout + "/left", layout: layout, edge: "free", normal: { x: -1, y: 0 }, side: "left" })
            rows.push({ tag: layout + "/right", layout: layout, edge: "free", normal: { x: 1, y: 0 }, side: "right" })
            rows.push({ tag: layout + "/diagonal", layout: layout, edge: "free",
                        normal: { x: 0.6, y: -0.8 }, side: "top" })
            rows.push({ tag: layout + "/native-bottom", layout: layout, edge: "bottom",
                        normal: { x: 0, y: -1 }, side: "top" })
        }
        return rows
    }
    function test_hostOpensAgainstTheFolderOnItsOutwardSide(data) {
        const window = createTemporaryObject(anchorComponent, null)
        const icon = findChild(window, "anchor")
        const screen = window.screen
        window.x = screen.virtualX
        window.y = screen.virtualY
        icon.x = screen.width / 2 - icon.width / 2
        icon.y = screen.height / 2 - icon.height / 2
        const host = createTemporaryObject(hostComponent, testCase, {
            folderItem: icon, reducedMotion: true, folderLayout: data.layout,
            panelEdge: data.edge, outwardNormal: data.normal,
            snapshot: { status: "ready", entries: rows(3) }
        })
        verify(host.openFolder())
        compare(host.expansionSide, data.side)
        const content = host.mainItem
        compare(content.geometry.side, data.side)
        const corner = icon.mapToGlobal(0, 0)
        const centre = icon.mapToGlobal(icon.width / 2, icon.height / 2)
        const gap = host.folderGap
        // Plasma puts the popup against the attachment's edge and centres it
        // across the attachment: that centre carries the contents' anchor
        // onto the folder. The edge is checked on the real popup; the offscreen
        // platform recentres a popup's first show across it, so the across
        // placement is proved natively by folder-anchor-smoke.
        const place = host.placement
        const size = data.side === "top" || data.side === "bottom" ? content.width : content.height
        const across = data.side === "top" || data.side === "bottom"
            ? corner.x + place.x + place.width / 2 : corner.y + place.y + place.height / 2
        fuzzyCompare(across - size / 2 + content.anchorAcross,
                     data.side === "top" || data.side === "bottom" ? centre.x : centre.y, 0.01)
        tryVerify(function() {
            if (!host.visible) return false
            if (data.side === "top") return Math.abs(host.y + host.height - (corner.y - gap)) <= 1
            if (data.side === "bottom") return Math.abs(host.y - (corner.y + icon.height + gap)) <= 1
            if (data.side === "left") return Math.abs(host.x + host.width - (corner.x - gap)) <= 1
            return Math.abs(host.x - (corner.x + icon.width + gap)) <= 1
        }, 3000, data.tag + ": the popup opens against the folder, on its outward side, "
                 + JSON.stringify([host.x, host.y, host.width, host.height, corner.x, corner.y]))
        if (data.normal.x === 0.6) {
            verify(host.expansionLean > 0.5, "a diagonal folder leans its contents")
            verify(content.anchorAcross < content.width / 2 || data.layout === "stack",
                   "the contents grow toward the lean")
        }
        // The contents unfold from the clicked icon, wherever the popup is.
        tryVerify(function() {
            return Math.abs(content.expansionOrigin.x - (centre.x - host.x - content.x)) <= 1
                && Math.abs(content.expansionOrigin.y - (centre.y - host.y - content.y)) <= 1
        }, 3000, "the opening animation starts at the folder icon")
        host.closeFolder()
    }
    function test_nativeHostLifecycle() {
        const window = createTemporaryObject(anchorComponent, null)
        verify(window !== null)
        const host = createTemporaryObject(hostComponent, testCase, {
            folderItem: findChild(window, "anchor"), reducedMotion: true,
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
