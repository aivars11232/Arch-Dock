import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// ADREP-TASK-003, PD-15: the four folder easings are four different motions
// in every folder layout - the popup's, which Grid and every edge panel use,
// and a free panel's paths - opening and closing, over the folder animation
// duration; reduced motion shows and hides a folder at once.
TestCase {
    id: testCase
    name: "FolderEasing"
    when: windowShown
    visible: true
    width: 900
    height: 700
    Component { id: popupComponent; FolderExpansion {} }
    Component { id: pathComponent; FolderTrack {} }
    SignalSpy { id: closed; signalName: "closeFinished" }
    readonly property var easings: ["outCubic", "outBack", "outElastic", "spring"]
    // Times through the animation at which every motion is sampled.
    readonly property var times: [0.05, 0.1, 0.15, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9]
    function rows(count) {
        const result = []
        for (let i = 0; i < count; ++i)
            result.push({ id: "child-" + i, displayName: "Document " + i,
                iconName: "text-plain", selectable: true })
        return result
    }
    // The layouts a folder opens in.
    readonly property var layouts: ["popup/grid", "popup/fan", "popup/stack", "popup/arc",
        "popup/ring", "path/track", "path/fan", "path/arc", "path/stack", "path/ring"]
    function trackSamples() {
        const samples = []
        for (let k = 0; k < 91; ++k) {
            const a = Math.PI - Math.PI / 3 + 2 * Math.PI / 3 * k / 90
            samples.push({ x: 450 + 160 * Math.cos(a), y: 350 + 160 * Math.sin(a), scale: 1 })
        }
        return samples
    }
    function folder(layout, easing, properties) {
        const kind = layout.split("/")[1]
        let values
        let component
        if (layout.startsWith("popup/")) {
            component = popupComponent
            values = { layout: kind, expansionSide: "top", expansionOrigin: Qt.point(120, 400),
                       opened: false }
        } else {
            component = pathComponent
            values = { width: 900, height: 700, iconSize: 48, shape: kind,
                       expansionOrigin: Qt.point(290, 350) }
            if (kind === "track") {
                values.samples = trackSamples()
            } else {
                const shape = LayoutEngine.folderShape(kind, { folder: { x: 450, y: 350 },
                    outward: { x: 0, y: -1 }, iconSize: 48, cellWidth: 108, cellHeight: 82, gap: 6,
                    count: 5, fanOpening: 90, stackLength: 5, ringSize: "small",
                    screen: { x: 0, y: 0, width: 900, height: 700 } })
                values.samples = shape.samples
                values.closedPath = shape.closed
                values.startAtFolder = shape.start === "start"
                values.pathCapacity = shape.capacity
                values.pathPitch = shape.pitch
                values.expansionOrigin = Qt.point(450, 350)
            }
        }
        values.easing = easing
        values.duration = 400
        values.reducedMotion = false
        values.snapshot = { status: "ready", entries: rows(5) }
        for (const key in properties || {}) values[key] = properties[key]
        const item = createTemporaryObject(component, testCase, values)
        verify(item !== null)
        if (layout.startsWith("popup/")) {
            item.width = item.implicitWidth
            item.height = item.implicitHeight
        }
        return item
    }
    // Where the first child's icon centre is on the screen: it stands away
    // from the folder in every layout.
    function childPoint(item) {
        const child = findChild(item, "folder-child-0")
        verify(child !== null)
        const size = item.iconSize !== undefined ? item.iconSize * child.drawnScale
                                                 : item.geometry.iconSize
        return child.mapToItem(testCase, child.width / 2, size / 2)
    }
    // The opening sampled at fixed times: once it has run, the animation's
    // clock is held at each of them.
    function sampled(item) {
        item.opened = true
        tryVerify(function() { return !item.openingInProgress }, 2000)
        const result = times.map(function(time) {
            item.openingPhase = time
            return childPoint(item)
        })
        item.openingPhase = 1
        result.push(childPoint(item))
        return result
    }
    function test_fourDifferentMotions_data() {
        return layouts.map(function(layout) { return { tag: layout, layout: layout } })
    }
    function test_fourDifferentMotions(data) {
        const curves = {}
        for (const easing of easings) {
            const item = folder(data.layout, easing)
            curves[easing] = sampled(item)
            item.opened = false
        }
        // Every motion ends where the child rests.
        for (const easing of easings) {
            const end = curves[easing][times.length]
            const reference = curves.outCubic[times.length]
            fuzzyCompare(end.x, reference.x, 0.01)
            fuzzyCompare(end.y, reference.y, 0.01)
        }
        // No two motions give the same curve: at some sampled time the child
        // stands at least 4 pixels apart.
        for (let a = 0; a < easings.length; ++a) {
            for (let b = a + 1; b < easings.length; ++b) {
                let apart = 0
                for (let index = 0; index < times.length; ++index)
                    apart = Math.max(apart, Math.hypot(curves[easings[a]][index].x - curves[easings[b]][index].x,
                                                       curves[easings[a]][index].y - curves[easings[b]][index].y))
                verify(apart >= 4, data.layout + ": " + easings[a] + " and " + easings[b]
                       + " give the same curve (" + apart.toFixed(2) + " px apart at most)")
            }
        }
        // Each is its own curve of the animation's time: the child stands
        // that share of the way from the folder to its place.
        const probe = folder(data.layout, "outCubic")
        probe.opened = true
        const origin = probe.mapToItem(testCase, probe.expansionOrigin.x, probe.expansionOrigin.y)
        for (const easing of easings) {
            const rest = curves[easing][times.length]
            for (let index = 0; index < times.length; ++index) {
                const share = MotionChannels.folderEasing(easing, times[index])
                fuzzyCompare(curves[easing][index].x, origin.x + (rest.x - origin.x) * share, 0.05)
                fuzzyCompare(curves[easing][index].y, origin.y + (rest.y - origin.y) * share, 0.05)
            }
        }
    }
    function test_curvesAreTheirOwn() {
        // outCubic never overshoots; outBack overshoots once, a little;
        // outElastic swings fast from the start; spring starts slower than
        // outElastic and swings past its place once before it settles.
        const at = function(name, t) { return MotionChannels.folderEasing(name, t) }
        for (const time of times) {
            verify(at("outCubic", time) <= 1 + 1e-9)
            verify(at("outBack", time) <= 1.11)
        }
        verify(Math.max.apply(null, times.map(function(t) { return at("outBack", t) })) > 1.05)
        verify(at("outElastic", 0.1) > 1.2, "elastic overshoots at once")
        verify(at("spring", 0.1) < 0.6, "the spring starts from rest")
        verify(at("spring", 0.3) > 1.2 && at("spring", 0.6) < 0.98, "and swings past its place")
        for (const name of easings) {
            compare(at(name, 0), 0)
            compare(at(name, 1), 1)
            compare(at(name, -1), 0)
            compare(at(name, 2), 1)
        }
        compare(at("unknown", 0.5), at("outBack", 0.5), "an unknown easing is outBack")
    }
    function test_durationAppliesAndClosingRunsBackwards_data() {
        return layouts.map(function(layout) { return { tag: layout, layout: layout } })
    }
    function test_durationAppliesAndClosingRunsBackwards(data) {
        const item = folder(data.layout, "spring", { duration: 600 })
        closed.target = item
        closed.clear()
        const start = Date.now()
        item.opened = true
        verify(item.openingInProgress)
        wait(200)
        const phase = item.openingPhase
        const elapsed = Date.now() - start
        verify(Math.abs(phase - elapsed / 600) < 0.2,
               "the opening keeps time with the duration: phase " + phase + " after " + elapsed + " ms")
        tryVerify(function() { return !item.openingInProgress }, 2000)
        compare(item.openingProgress, 1)
        // Closing plays the motion backwards over the same duration, then
        // reports it: the clock runs down from 1.
        const closing = Date.now()
        item.close()
        verify(item.closing)
        wait(200)
        const down = item.openingPhase
        const gone = Date.now() - closing
        verify(Math.abs(down - (1 - gone / 600)) < 0.2,
               "the closing keeps time with the duration: phase " + down + " after " + gone + " ms")
        compare(closed.count, 0)
        tryCompare(closed, "count", 1, 2000)
        compare(item.openingPhase, 0)
        compare(item.openingProgress, 0)
        // A duration beyond the range is held to it.
        const slow = folder(data.layout, "outCubic", { duration: 9999 })
        slow.opened = true
        wait(1000)
        verify(slow.openingInProgress, "1200 ms at most, still opening after 1000")
        tryVerify(function() { return !slow.openingInProgress }, 1000)
    }
    function test_reducedMotionIsImmediate_data() {
        return layouts.map(function(layout) { return { tag: layout, layout: layout } })
    }
    function test_reducedMotionIsImmediate(data) {
        for (const easing of easings) {
            const item = folder(data.layout, easing, { reducedMotion: true })
            closed.target = item
            closed.clear()
            item.opened = true
            compare(item.openingProgress, 1, easing + ": open at once")
            verify(!item.openingInProgress)
            item.close()
            compare(closed.count, 1, easing + ": closed at once")
            compare(item.openingProgress, 0)
        }
    }
}
