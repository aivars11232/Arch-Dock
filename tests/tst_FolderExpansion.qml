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
    Component { id: trackComponent; FolderTrack {} }
    Component { id: trackHostComponent; DockUi.FolderTrackHost {} }
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
        // Plasma moves an applet popup with a short attachment to the middle
        // of the screen when that middle lies inside it; an attachment at
        // least one and a half times the popup's length never qualifies.
        verify((data.side === "top" || data.side === "bottom" ? place.width : place.height) >= 1.5 * size,
               data.tag + ": the attachment outlasts Plasma's centring rule")
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
    // "Along the dock": a third of a circle of radius 160 around (350, 250),
    // the folder at (190, 250) facing its middle from inside the dock.
    function trackSamples() {
        const samples = []
        for (let k = 0; k < 91; ++k) {
            const a = Math.PI - Math.PI / 3 + 2 * Math.PI / 3 * k / 90
            samples.push({ x: 350 + 160 * Math.cos(a), y: 250 + 160 * Math.sin(a), scale: 1 })
        }
        return samples
    }
    function track(count, reduced, names) {
        const item = createTemporaryObject(trackComponent, testCase, {
            width: 700, height: 500, samples: trackSamples(), iconSize: 48,
            reducedMotion: reduced === undefined ? true : reduced, showNames: names === true,
            duration: 500, expansionOrigin: Qt.point(250, 250), folderTitle: "Documents",
            snapshot: { status: "ready", entries: rows(count), truncated: count === 48 }
        })
        verify(item !== null)
        item.opened = true
        selection.target = item; selection.clear()
        dismissal.target = item; dismissal.clear()
        return item
    }
    function childCentre(item, index) {
        const child = findChild(item, "folder-child-" + index)
        return Qt.point(child.x + child.width / 2, child.y + item.iconSize * child.drawnScale / 2)
    }
    function test_trackStandsChildrenOnTheCurve() {
        const item = track(5)
        compare(item.track.capacity, 5)
        for (let index = 0; index < 5; ++index) {
            const centre = childCentre(item, index)
            verify(Math.abs(Math.hypot(centre.x - 350, centre.y - 250) - 160) < 0.5,
                   "child " + index + " stands on the dock's curve")
        }
        const middle = childCentre(item, 2)
        fuzzyCompare(middle.x, 190, 0.5)
        fuzzyCompare(middle.y, 250, 0.5)
        const first = childCentre(item, 0)
        mouseMove(item, first.x, first.y)
        tryCompare(item, "selectedChildId", "child-0")
        mouseClick(item, first.x, first.y)
        compare(selection.count, 1)
        compare(selection.signalArguments[0][0], "child-0")
        const blocked = childCentre(item, 2)
        mouseClick(item, blocked.x, blocked.y)
        compare(selection.count, 1, "a blocked child never opens")
        compare(item.selectedChildId, "child-2", "the pointer selects what it hovers")
        item.forceActiveFocus()
        keyClick(Qt.Key_Right)
        compare(item.selectedChildId, "child-3")
        keyClick(Qt.Key_Return)
        compare(selection.count, 2)
        compare(selection.signalArguments[1][0], "child-3")
        mouseClick(item, 650, 450)
        compare(dismissal.count, 1, "a press beside the children closes the folder")
        keyClick(Qt.Key_Escape)
        compare(dismissal.count, 2)
    }
    function test_trackMovesAnOvercrowdedFolderAlongTheCurve() {
        const item = track(48, true, true)
        verify(item.track.windowed)
        const capacity = item.track.capacity
        compare(findChild(item, "folder-child-" + capacity).opacity, 0, "a child past the end is not shown")
        const first = childCentre(item, 0)
        // Turning the wheel back brings the next child in at the end.
        mouseWheel(item, first.x, first.y, 0, -120)
        tryCompare(item, "trackOffset", -1)
        verify(findChild(item, "folder-child-0").opacity < 0.01, "the wheel moved the first child off the curve")
        verify(findChild(item, "folder-child-" + capacity).opacity > 0.99)
        const shown = childCentre(item, capacity)
        verify(Math.abs(Math.hypot(shown.x - 350, shown.y - 250) - 160) < 0.5)
        mouseWheel(item, first.x, first.y, 0, 120)
        tryCompare(item, "trackOffset", 0)
        // And on past the start: the last child comes round to the first place.
        mouseWheel(item, first.x, first.y, 0, 120)
        tryCompare(item, "trackOffset", 1)
        verify(findChild(item, "folder-child-47").opacity > 0.99, "the last child came round")
        fuzzyCompare(childCentre(item, 47).x, first.x, 0.5)
        fuzzyCompare(childCentre(item, 47).y, first.y, 0.5)
        mouseWheel(item, first.x, first.y, 0, -120)
        tryCompare(item, "trackOffset", 0)
        item.forceActiveFocus()
        for (let step = 0; step < 47; ++step) keyClick(Qt.Key_Right)
        compare(item.selectedChildId, "child-47")
        compare(item.trackOffset, -(48 - capacity), "keys bring the selection onto the curve")
        verify(item.track.entries[47].onTrack)
        compare(selection.count, 0, "browsing never opens a child")
    }
    function test_trackUnfoldsFromTheFolder() {
        const item = track(3, false)
        const start = childCentre(item, 1)
        verify(Math.hypot(start.x - 250, start.y - 250) < 30, "children start at the folder icon")
        verify(!item.selectChild("child-0"), "moving contents cannot launch")
        tryCompare(item, "openingInProgress", false)
        const end = childCentre(item, 1)
        fuzzyCompare(end.x, 190, 0.5)
        item.opened = false
        item.reducedMotion = true
        item.opened = true
        compare(item.openingProgress, 1)
    }
    function test_trackHostDrawsTheContentsOnTheDocksCurve() {
        const window = createTemporaryObject(anchorComponent, null)
        const icon = findChild(window, "anchor")
        const screen = window.screen
        window.x = screen.virtualX
        window.y = screen.virtualY
        icon.x = screen.width / 2 - icon.width / 2
        icon.y = screen.height / 2 - icon.height / 2
        // The dock is centred 150 pixels left of the folder; its outer curve
        // passes 64 pixels outside the folder.
        const dock = Qt.point(icon.x + icon.width / 2 - 150, icon.y + icon.height / 2)
        const samples = []
        for (let k = 0; k < 91; ++k) {
            const a = -Math.PI / 3 + 2 * Math.PI / 3 * k / 90
            samples.push({ x: dock.x + 214 * Math.cos(a), y: dock.y + 214 * Math.sin(a), scale: 1 })
        }
        const host = createTemporaryObject(trackHostComponent, testCase, {
            folderItem: icon, iconSize: 48, reducedMotion: true,
            trackSamples: function() { return samples },
            snapshot: { status: "ready", entries: rows(5) }
        })
        verify(host.openFolder())
        tryCompare(host, "visible", true)
        const dockGlobal = icon.parent.mapToGlobal(dock.x, dock.y)
        tryVerify(function() {
            for (let index = 0; index < 5; ++index) {
                const child = findChild(host.mainItem, "folder-child-" + index)
                const centre = child.mapToGlobal(child.width / 2, host.iconSize * child.drawnScale / 2)
                if (Math.abs(Math.hypot(centre.x - dockGlobal.x, centre.y - dockGlobal.y) - 214) > 1)
                    return false
            }
            return true
        }, 3000, "every child stands on the dock's curve, wherever the window is")
        const folder = icon.mapToGlobal(icon.width / 2, icon.height / 2)
        fuzzyCompare(host.mainItem.expansionOrigin.x + host.x, folder.x, 1)
        fuzzyCompare(host.mainItem.expansionOrigin.y + host.y, folder.y, 1)
        host.mainItem.dismissRequested()
        tryCompare(host, "visible", false)
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

    // ADREP-TASK-003: a free panel's own folder shapes. The folder stands at
    // the middle of a large screen with a dock icon either side of it, a
    // little behind it, as on a ring.
    readonly property point shapeFolder: Qt.point(1920, 1080)
    function unit(vector) {
        const length = Math.hypot(vector.x, vector.y)
        return { x: vector.x / length, y: vector.y / length }
    }
    function neighbours(outward) {
        const n = unit(outward), across = { x: -n.y, y: n.x }
        return [-1, 1].map(function(side) {
            return { x: shapeFolder.x + side * 64 * across.x - 12 * n.x,
                     y: shapeFolder.y + side * 64 * across.y - 12 * n.y }
        })
    }
    function shapeOf(kind, outward, count, names, extra) {
        const options = { folder: { x: shapeFolder.x, y: shapeFolder.y }, outward: outward, iconSize: 48,
            cellWidth: names ? 108 : 48, cellHeight: names ? 82 : 48, gap: 6, count: count,
            fanOpening: 90, stackLength: 5, ringSize: "small", panelRadius: 150,
            obstacles: neighbours(outward), obstacleSize: 48,
            screen: { x: 0, y: 0, width: 3840, height: 2160 }, outlineMargin: 20 }
        for (const key in extra || {}) options[key] = extra[key]
        return LayoutEngine.folderShape(kind, options)
    }
    function layoutOf(shape, count, travel) {
        return LayoutEngine.folderTrackLayout(shape.samples, count, shape.pitch, travel || 0,
            { closed: shape.closed, start: shape.start, capacity: shape.capacity })
    }
    // Degrees from `from` to `to`, both directions, clockwise on the screen.
    function turnBetween(from, to) {
        const turn = (Math.atan2(to.y, to.x) - Math.atan2(from.y, from.x)) * 180 / Math.PI
        return ((turn % 360) + 540) % 360 - 180
    }
    function direction(from, to) { return unit({ x: to.x - from.x, y: to.y - from.y }) }
    function cellOverlaps(point, obstacle, names) {
        const width = names ? 108 : 48, height = names ? 82 : 48, reach = 24
        return point.x - width / 2 < obstacle.x + reach && point.x + width / 2 > obstacle.x - reach
            && point.y - 24 < obstacle.y + reach && point.y - 24 + height > obstacle.y - reach
    }
    function test_folderShapes_data() {
        const rows = []
        const outwards = { up: { x: 0, y: -1 }, down: { x: 0, y: 1 }, left: { x: -1, y: 0 },
                           right: { x: 1, y: 0 }, diagonal: { x: 0.6, y: -0.8 } }
        for (const kind of ["fan", "arc", "stack", "ring"])
            for (const way in outwards)
                for (const count of [3, 8, 24])
                    for (const names of [false, true])
                        rows.push({ tag: kind + "/" + way + "/" + count + (names ? "/names" : ""),
                                    kind: kind, outward: outwards[way], count: count, names: names })
        return rows
    }
    function test_folderShapes(data) {
        const shape = shapeOf(data.kind, data.outward, data.count, data.names)
        const n = unit(data.outward)
        compare(shape.shape, data.kind)
        compare(shape.turn, 0, "it opens along the folder's outward direction")
        const samples = shape.samples
        const layout = layoutOf(shape, data.count)
        const shown = layout.entries.filter(function(entry) { return entry.onTrack })
        compare(shown.length, Math.min(data.count, layout.capacity))
        verify(layout.capacity >= Math.min(data.count, 2), "the shape holds children: " + layout.capacity)
        if (data.kind === "fan") {
            // PD-11: the apex just outside the folder, the children on the arc
            // joining two edges 90 degrees apart about the outward direction.
            const reach = 24 / Math.max(Math.abs(n.x), Math.abs(n.y)) + 6
            fuzzyCompare(shape.apex.x, shapeFolder.x + n.x * reach, 0.01)
            fuzzyCompare(shape.apex.y, shapeFolder.y + n.y * reach, 0.01)
            for (const point of samples)
                fuzzyCompare(Math.hypot(point.x - shape.apex.x, point.y - shape.apex.y), shape.radius, 0.01)
            fuzzyCompare(turnBetween(n, direction(shape.apex, samples[0])), -45, 0.01)
            fuzzyCompare(turnBetween(n, direction(shape.apex, samples[samples.length - 1])), 45, 0.01)
            verify(shape.outline.closed)
            compare(shape.outline.points[0], shape.apex)
            for (const entry of shown)
                fuzzyCompare(Math.hypot(entry.x - shape.apex.x, entry.y - shape.apex.y), shape.radius, 0.05)
        } else if (data.kind === "arc") {
            // PD-13: at one distance from the folder, symmetric about its
            // outward direction.
            for (const point of samples)
                fuzzyCompare(Math.hypot(point.x - shapeFolder.x, point.y - shapeFolder.y), shape.radius, 0.01)
            fuzzyCompare(turnBetween(n, direction(shapeFolder, samples[0])), -75, 0.01)
            fuzzyCompare(turnBetween(n, direction(shapeFolder, samples[samples.length - 1])), 75, 0.01)
            const middle = layout.entries.filter(function(entry) { return entry.onTrack })
            const first = middle[0], last = middle[middle.length - 1]
            fuzzyCompare(turnBetween(n, direction(shapeFolder, first)),
                         -turnBetween(n, direction(shapeFolder, last)), 0.05)
        } else if (data.kind === "stack") {
            // PD-12: a straight line from the folder outward, as long as
            // Stack length allows.
            compare(layout.capacity, Math.min(data.count, 5))
            for (let index = 0; index < shown.length; ++index) {
                const entry = shown[index]
                const along = (entry.x - shapeFolder.x) * n.x + (entry.y - shapeFolder.y) * n.y
                const aside = (entry.x - shapeFolder.x) * -n.y + (entry.y - shapeFolder.y) * n.x
                verify(Math.abs(aside) < 0.01, "on the line: " + aside)
                verify(along > 24, "beyond the folder: " + along)
                if (index > 0) fuzzyCompare(Math.hypot(entry.x - shown[index - 1].x,
                                                       entry.y - shown[index - 1].y), shape.pitch, 0.01)
            }
        } else {
            // PD-14: a second circle beside the folder along its outward
            // direction, the children on its circumference.
            verify(shape.closed)
            for (const point of samples)
                fuzzyCompare(Math.hypot(point.x - shape.centre.x, point.y - shape.centre.y), shape.radius, 0.01)
            const away = direction(shapeFolder, shape.centre)
            fuzzyCompare(away.x, n.x, 1e-6)
            fuzzyCompare(away.y, n.y, 1e-6)
            verify(Math.hypot(shape.centre.x - shapeFolder.x, shape.centre.y - shapeFolder.y) > shape.radius + 24)
            if (data.count <= layout.capacity)
                compare(shown.length, data.count, "a small ring holds every child")
        }
        // No place a child passes through covers the folder or a dock icon,
        // and neighbours on the path keep their cells apart.
        const obstacles = neighbours(data.outward).concat([{ x: shapeFolder.x, y: shapeFolder.y }])
        for (const point of samples)
            for (const obstacle of obstacles)
                verify(!cellOverlaps(point, obstacle, data.names),
                       "a child at " + JSON.stringify(point) + " covers " + JSON.stringify(obstacle))
        for (let index = 1; index < shown.length; ++index) {
            const a = shown[index - 1], b = shown[index]
            const width = data.names ? 108 : 48, height = data.names ? 82 : 48
            verify(Math.abs(a.x - b.x) >= width - 0.5 || Math.abs(a.y - b.y) >= height - 0.5
                   || !data.names, "neighbours " + index + " overlap")
        }
        // The room the window needs holds them all.
        for (const point of samples) {
            verify(point.x - (data.names ? 54 : 24) >= shape.bounds.x - 0.01)
            verify(point.x + (data.names ? 54 : 24) <= shape.bounds.x + shape.bounds.width + 0.01)
        }
    }
    function test_folderShapeSettings() {
        const up = { x: 0, y: -1 }
        // Fan opening: the angle between the fan's edges.
        for (const opening of [40, 90, 160]) {
            const fan = shapeOf("fan", up, 8, false, { fanOpening: opening })
            fuzzyCompare(fan.opening, opening, 0.01)
            const s = fan.samples
            fuzzyCompare(turnBetween(direction(fan.apex, s[0]), direction(fan.apex, s[s.length - 1])), opening, 0.01)
        }
        compare(shapeOf("fan", up, 8, false, { fanOpening: 400 }).opening, 160)
        compare(shapeOf("fan", up, 8, false, { fanOpening: 2 }).opening, 40)
        verify(layoutOf(shapeOf("fan", up, 24, false, { fanOpening: 160 }), 24).capacity
               > layoutOf(shapeOf("fan", up, 24, false, { fanOpening: 40 }), 24).capacity,
               "a wider fan shows more children at once")
        // Stack length: how many children the line shows.
        for (const length of [2, 5, 12])
            compare(layoutOf(shapeOf("stack", up, 24, false, { stackLength: length }), 24).capacity, length)
        compare(layoutOf(shapeOf("stack", up, 3, false, { stackLength: 12 }), 3).capacity, 3)
        compare(shapeOf("stack", up, 24, false, { stackLength: 40 }).capacity, 12)
        // Ring size: fits the children, or has the dock's radius.
        const small = shapeOf("ring", up, 8, false)
        compare(small.ringSize, "small")
        // Just wide enough that neighbours stand a pitch apart in a straight line.
        fuzzyCompare(small.radius, 62 / (2 * Math.sin(Math.PI / 8)), 0.01)
        const spread = layoutOf(small, 8).entries
        fuzzyCompare(Math.hypot(spread[1].x - spread[0].x, spread[1].y - spread[0].y), 62, 0.5)
        const panel = shapeOf("ring", up, 8, false, { ringSize: "panel" })
        compare(panel.ringSize, "panel")
        compare(panel.radius, 150)
        compare(layoutOf(panel, 8).capacity, 8)
        const noRadius = shapeOf("ring", up, 8, false, { ringSize: "panel", panelRadius: 0 })
        compare(noRadius.ringSize, "small", "a dock without a radius keeps the small ring")
    }
    function test_folderShapeGivesWayOnlyToTheScreen() {
        // Facing up from 60 pixels below the top of the screen: the fan holds
        // fewer children at once, then turns, and never covers the dock.
        const options = { screen: { x: 0, y: 0, width: 3840, height: 1080 + 60 + 200 } }
        const roomy = shapeOf("fan", { x: 0, y: -1 }, 24, true, options)
        compare(roomy.turn, 0)
        compare(roomy.fewer, 0)
        const tight = shapeOf("fan", { x: 0, y: -1 }, 24, true,
                              { screen: { x: 0, y: 1080 - 260, width: 3840, height: 1000 } })
        verify(tight.fewer > 0 || tight.turn !== 0, "the fan gave way to the screen")
        verify(tight.bounds.y >= 1080 - 260 - 0.5, "and fits: " + JSON.stringify(tight.bounds))
        // A ring with no room above but room beside the folder turns
        // towards a side, not over the dock.
        const ring = shapeOf("ring", { x: 0, y: -1 }, 8, true,
                             { screen: { x: 0, y: 1080 - 260, width: 3840, height: 1100 } })
        verify(ring.turn !== 0, "a ring that does not fit above turns to where it does")
        verify(Math.abs(ring.turn) <= 90, "towards a side before the other way: " + ring.turn)
        verify(ring.bounds.y >= 1080 - 260 - 0.5)
        // A small ring short of room holds fewer children at once, still
        // beside the folder; the others come round as it turns.
        const shortRing = shapeOf("ring", { x: 0, y: -1 }, 24, true,
                                  { screen: { x: 0, y: 1080 - 620, width: 3840, height: 2000 } })
        compare(shortRing.turn, 0)
        verify(shortRing.fewer > 0, "fewer children at once")
        verify(layoutOf(shortRing, 24).capacity < 24 && layoutOf(shortRing, 24).capacity >= 3)
        // A stack too long for the room shows fewer children, still outward.
        const stack = shapeOf("stack", { x: 1, y: 0 }, 24, true,
                              { stackLength: 12, screen: { x: 0, y: 0, width: 1920 + 700, height: 2160 } })
        compare(stack.turn, 0)
        verify(stack.capacity < 12 && stack.capacity >= 2, "a shorter stack: " + stack.capacity)
        verify(stack.bounds.x + stack.bounds.width <= 1920 + 700 + 0.5)
        // With no room above nor beside, it opens the other way.
        const flipped = shapeOf("ring", { x: 0, y: -1 }, 8, true,
                                { screen: { x: 1920 - 160, y: 1080 - 200, width: 320, height: 1100 } })
        compare(flipped.turn, 180)
        verify(flipped.bounds.x >= 1920 - 160 - 0.5 && flipped.bounds.y >= 1080 - 200 - 0.5)
    }
    function test_folderPathsWrapAround_data() {
        return [
            { tag: "fan/24", kind: "fan", count: 24 }, { tag: "arc/24", kind: "arc", count: 24 },
            { tag: "stack/24", kind: "stack", count: 24 }, { tag: "stack/3", kind: "stack", count: 3 },
            { tag: "fan/3", kind: "fan", count: 3 }, { tag: "ring/24", kind: "ring", count: 24 },
            { tag: "ring/3", kind: "ring", count: 3 }
        ]
    }
    function test_folderPathsWrapAround(data) {
        // PD-10: an open path's child that leaves one end comes back at the
        // other, whether or not every child fits; a ring carries them round.
        const shape = shapeOf(data.kind, { x: 0, y: -1 }, data.count, true, { ringSize: "panel" })
        const rest = layoutOf(shape, data.count, 0)
        const loop = rest.loop
        verify(loop >= data.count)
        const around = layoutOf(shape, data.count, loop)
        for (let index = 0; index < data.count; ++index) {
            fuzzyCompare(around.entries[index].x, rest.entries[index].x, 1e-6)
            fuzzyCompare(around.entries[index].y, rest.entries[index].y, 1e-6)
            compare(around.entries[index].onTrack, rest.entries[index].onTrack)
        }
        const places = rest.entries.filter(function(entry) { return entry.onTrack })
        if (!shape.closed) {
            // One step on: the child in the last place leaves past the end and
            // waits; one step back: the first leaves at the start.
            const lastShown = rest.entries.reduce(function(found, entry) {
                return entry.onTrack ? entry.index : found }, -1)
            const forward = layoutOf(shape, data.count, 1)
            verify(!forward.entries[lastShown].onTrack)
            compare(forward.entries[lastShown].visibility, 0)
            const back = layoutOf(shape, data.count, -1)
            verify(!back.entries[places[0].index].onTrack)
            compare(back.entries[places[0].index].visibility, 0)
            // Far enough on, it comes back in the first place.
            let steps = 1
            while (steps <= loop && !(layoutOf(shape, data.count, steps).entries[lastShown].onTrack
                   && layoutOf(shape, data.count, steps).entries[lastShown].slot < 0.5))
                ++steps
            verify(steps < loop, "the child came back at the start")
            const returned = layoutOf(shape, data.count, steps).entries[lastShown]
            const firstPlace = layoutOf(shape, data.count, 0).entries[places[0].index]
            if (data.count > rest.capacity) {
                fuzzyCompare(returned.x, firstPlace.x, 0.01)
                fuzzyCompare(returned.y, firstPlace.y, 0.01)
            }
            // Half a step: the leaving child is half faded past the end.
            const half = layoutOf(shape, data.count, 0.5).entries[lastShown]
            verify(!half.onTrack)
            fuzzyCompare(half.visibility, 0.5, 1e-6)
        } else {
            const step = layoutOf(shape, data.count, 1)
            for (let index = 0; index < data.count; ++index) {
                if (!step.entries[index].onTrack || index === data.count - 1) continue
                const next = rest.entries[index + 1]
                if (!next.onTrack) continue
                fuzzyCompare(step.entries[index].x, next.x, 0.01)
                fuzzyCompare(step.entries[index].y, next.y, 0.01)
            }
        }
    }
    // A fan's path inside a 700 x 500 view, the folder at (350, 400).
    function fanView(count, names, properties) {
        const outward = { x: 0, y: -1 }
        const fan = LayoutEngine.folderShape("fan", { folder: { x: 350, y: 400 }, outward: outward,
            iconSize: 48, cellWidth: names ? 108 : 48, cellHeight: names ? 82 : 48, gap: 6,
            count: count, fanOpening: 90, obstacles: [], obstacleSize: 48,
            screen: { x: 0, y: 0, width: 700, height: 500 }, outlineMargin: 20 })
        const values = { width: 700, height: 500, iconSize: 48, shape: "fan", reducedMotion: true,
            showNames: names, duration: 260, expansionOrigin: Qt.point(350, 400), folderTitle: "Documents",
            samples: fan.samples, pathPitch: fan.pitch, outline: fan.outline, outlineFilled: true,
            outlineStyle: LayoutEngine.themeStyle("futuristic", "", 48),
            snapshot: { status: "ready", entries: rows(count) } }
        for (const key in properties || {}) values[key] = properties[key]
        const item = createTemporaryObject(trackComponent, testCase, values)
        verify(item !== null)
        item.opened = true
        selection.target = item; selection.clear()
        dismissal.target = item; dismissal.clear()
        return { item: item, fan: fan }
    }
    function test_folderWheelGathersNotches() {
        // PD-16: one notch, or one pitch of touchpad travel, moves one child,
        // however finely the wheel reports it, scaled by Scroll sensitivity.
        const view = fanView(24, false)
        const item = view.item
        const first = item.track.entries.find(function(entry) { return entry.onTrack })
        const point = Qt.point(first.x, first.y)
        for (let event = 0; event < 7; ++event) {
            mouseWheel(item, point.x, point.y, 0, 15)
            compare(item.trackOffset, 0, "a fraction of a notch moves nothing yet")
        }
        mouseWheel(item, point.x, point.y, 0, 15)
        compare(item.trackOffset, 1, "eight fine events are one notch")
        mouseWheel(item, point.x, point.y, 0, -120)
        compare(item.trackOffset, 0)
        item.scrollSensitivity = 2
        mouseWheel(item, point.x, point.y, 0, 120)
        compare(item.trackOffset, 2, "twice the sensitivity, two children a notch")
        item.scrollSensitivity = 0.5
        mouseWheel(item, point.x, point.y, 0, -120)
        compare(item.trackOffset, 2)
        mouseWheel(item, point.x, point.y, 0, -120)
        compare(item.trackOffset, 1, "half the sensitivity, a child every two notches")
        item.scrollSensitivity = 1
        compare(item.takeWheel(0, item.pitch / 2), 0)
        compare(item.takeWheel(0, item.pitch / 2), 1, "a pitch of touchpad travel is one child")
        compare(item.trackOffset, 2)
        // Everything fits on a short folder, and it still goes round (PD-10).
        const small = fanView(3, false).item
        const before = small.track.entries.map(function(entry) { return entry.onTrack })
        verify(before.every(function(shown) { return shown }))
        small.takeWheel(120, 0)
        compare(small.trackOffset, 1)
        verify(!small.track.entries[2].onTrack, "the last child left past the end")
        small.takeWheel(120, 0)
        verify(small.track.entries[2].onTrack, "and came back at the start")
        compare(selection.count, 0, "browsing never opens a child")
    }
    function test_fanDrawsItsPanelAndKeepsPressesOnIt() {
        const view = fanView(8, true)
        const item = view.item
        const canvas = findChild(item, "folderOutline")
        verify(canvas.visible, "the fan's sector is drawn")
        verify(waitForRendering(item))
        // Inside the sector, between the folder and the arc: no dismissal.
        const inside = { x: view.fan.apex.x, y: view.fan.apex.y - view.fan.radius / 2 }
        mousePress(item, inside.x, inside.y)
        mouseRelease(item, inside.x, inside.y)
        compare(dismissal.count, 0, "a press on the folder's small panel keeps it open")
        mouseClick(item, 20, 20)
        compare(dismissal.count, 1, "a press beside it closes the folder")
        // The children's hit targets are where they are drawn, on the arc.
        const shown = item.track.entries.filter(function(entry) { return entry.onTrack })
        const target = shown[1]
        mouseMove(item, target.x, target.y)
        tryCompare(item, "selectedChildId", "child-" + target.index)
        mouseClick(item, target.x, target.y)
        compare(selection.count, 1)
        compare(selection.signalArguments[0][0], "child-" + target.index)
        // Keys walk past the end and bring each child onto the arc.
        item.forceActiveFocus()
        for (let step = 0; step < 7; ++step) keyClick(Qt.Key_Right)
        compare(item.selectedChildId, "child-7")
        verify(item.track.entries[7].onTrack)
        const label = findChild(item, "folder-name-7")
        verify(label.visible && label.text === "Document 7")
        item.showNames = false
        tryCompare(label, "visible", false)
    }
    function test_gridScrollsOneRowPerNotch() {
        const item = popup("grid", 48)
        const viewport = findChild(item, "folderViewport")
        const row = item.geometry.cellHeight + 6
        verify(viewport.contentHeight > viewport.height + 2 * row)
        mouseWheel(viewport, viewport.width / 2, 30, 0, -120)
        tryCompare(viewport, "contentY", row)
        mouseWheel(viewport, viewport.width / 2, 30, 0, -120)
        tryCompare(viewport, "contentY", 2 * row)
        mouseWheel(viewport, viewport.width / 2, 30, 0, 120)
        tryCompare(viewport, "contentY", row)
        item.scrollSensitivity = 2
        mouseWheel(viewport, viewport.width / 2, 30, 0, -120)
        tryCompare(viewport, "contentY", 3 * row)
        compare(selection.count, 0)
    }
}
