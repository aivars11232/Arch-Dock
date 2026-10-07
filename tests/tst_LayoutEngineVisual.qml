import QtQuick 2.15
import QtTest 1.3
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "LayoutEngineVisual"
    when: windowShown
    width: 760
    height: 380

    property string currentLayout: "horizontal"
    property int entryCount: 6
    property string folderLayout: "fan"
    readonly property var folderGeometry: LayoutEngine.expansionGeometry(
        folderLayout, 8, 32, 8, 120, 3)
    Item {
        id: folderFixture
        width: testCase.folderGeometry.width
        height: testCase.folderGeometry.height
        Repeater {
            model: testCase.folderGeometry.entries
            delegate: Rectangle {
                required property var modelData
                x: modelData.x
                y: modelData.y
                width: 32
                height: 32
                color: "cyan"
            }
        }
    }

    function test_folderLayoutRendersEveryExposedEntry() {
        for (const layout of ["fan", "grid", "stack", "arc", "ring"]) {
            folderLayout = layout
            wait(0)
            const image = grabImage(folderFixture)
            compare(image.width, folderGeometry.width)
            compare(image.height, folderGeometry.height)
            for (const point of folderGeometry.entries)
                verify(image.alpha(Math.floor(point.x + 8), Math.floor(point.y + 8)) > 0,
                       layout + " child renders within its declared bounds")
        }
    }
    readonly property var currentGeometry: LayoutEngine.metrics(
        currentLayout, entryCount, 40, 8, 1, 120, 2, 12, false, 0, 6)

    Item {
        id: liveFixture

        x: 8
        y: 8
        width: testCase.currentGeometry.width
        height: testCase.currentGeometry.height

        Rectangle {
            anchors.fill: parent
            color: "#101820"
        }

        Repeater {
            model: testCase.entryCount

            delegate: Rectangle {
                required property int index
                readonly property var point: LayoutEngine.position(
                    testCase.currentLayout, index, testCase.entryCount,
                    testCase.currentGeometry, 0, 6, "upright", "live")

                x: point.x
                y: point.y
                width: testCase.currentGeometry.iconSize
                height: width
                rotation: point.rotation
                radius: width * 0.22
                color: Qt.hsla(index / testCase.entryCount, 0.72, 0.58, 1)
                border.width: 2
                border.color: "white"
            }
        }
    }

    Item {
        id: canonicalFixture

        x: 8
        y: 8
        width: testCase.currentGeometry.width
        height: testCase.currentGeometry.height

        Rectangle {
            anchors.fill: parent
            color: "#101820"
        }

        Repeater {
            model: testCase.entryCount

            delegate: Rectangle {
                required property int index
                readonly property var point: LayoutEngine.position(
                    testCase.currentLayout, index, testCase.entryCount,
                    testCase.currentGeometry, 0, 6, "upright", "canonical")

                x: point.x
                y: point.y
                width: testCase.currentGeometry.iconSize
                height: width
                rotation: point.rotation
                radius: width * 0.22
                color: Qt.hsla(index / testCase.entryCount, 0.72, 0.58, 1)
                border.width: 2
                border.color: "white"
            }
        }
    }

    // ---- ADREP-TASK-002: entries travel along the drawn path -------------
    property real travel: 0
    readonly property var travelGeometry: Object.assign({}, currentGeometry, { travel: travel })
    readonly property var travelOutline: LayoutEngine.surface(currentLayout, currentGeometry, 0, 6)

    Item {
        id: travelFixture

        x: 8
        y: 8
        width: testCase.currentGeometry.width
        height: testCase.currentGeometry.height

        Rectangle {
            anchors.fill: parent
            color: "#101820"
        }

        Repeater {
            model: testCase.travelOutline.points

            delegate: Rectangle {
                required property var modelData
                x: modelData.x - 1
                y: modelData.y - 1
                width: 3
                height: 3
                color: "#3a5a6a"
            }
        }

        Repeater {
            model: testCase.entryCount

            delegate: Rectangle {
                required property int index
                readonly property var entry: LayoutEngine.entryGeometry(
                    testCase.currentLayout, index, testCase.entryCount,
                    testCase.travelGeometry, 0, 6, "upright", "canonical")

                x: entry.position.x
                y: entry.position.y
                width: testCase.currentGeometry.iconSize
                height: width
                visible: entry.trackVisibility > 0
                opacity: entry.trackVisibility
                radius: width * 0.22
                color: Qt.hsla(index / testCase.entryCount, 0.72, 0.58, 1)
            }
        }
    }

    function outlineDistance(x, y) {
        const points = travelOutline.points
        const last = travelOutline.closed ? points.length : points.length - 1
        let best = Infinity
        for (let index = 0; index < last; ++index) {
            const a = points[index]
            const b = points[(index + 1) % points.length]
            const dx = b.x - a.x
            const dy = b.y - a.y
            const share = Math.max(0, Math.min(1, ((x - a.x) * dx + (y - a.y) * dy)
                                                / Math.max(1e-9, dx * dx + dy * dy)))
            best = Math.min(best, Math.hypot(x - a.x - share * dx, y - a.y - share * dy))
        }
        return best
    }

    // Criterion: travelling entries are drawn on the panel's own drawn path,
    // at every phase of a step, and a whole loop brings back the same image.
    function test_travelKeepsEntriesOnTheDrawnPath_data() {
        return ["circular", "ellipse", "hexagon", "triangle", "square", "star",
                "spiral", "arc", "fan", "semicircle", "radial"].map(function(layout) {
            return { tag: layout, layout: layout }
        })
    }

    function test_travelKeepsEntriesOnTheDrawnPath(data) {
        currentLayout = data.layout
        entryCount = 6
        travel = 0
        wait(0)
        const rest = grabImage(travelFixture)
        const loop = LayoutEngine.pathWindow(currentLayout, entryCount, currentGeometry).loop
        verify(loop >= entryCount)
        for (const step of [0.25, 0.5, 1, 2.5, -1.5]) {
            travel = step
            wait(0)
            const image = grabImage(travelFixture)
            let drawn = 0
            for (let index = 0; index < entryCount; ++index) {
                const entry = LayoutEngine.entryGeometry(
                    currentLayout, index, entryCount, travelGeometry, 0, 6,
                    "upright", "canonical")
                if (entry.trackVisibility < 1)
                    continue
                const x = entry.position.x + currentGeometry.iconSize / 2
                const y = entry.position.y + currentGeometry.iconSize / 2
                verify(outlineDistance(x, y) < 1, data.tag + " entry " + index
                       + " left the drawn path at travel " + step)
                verify(!Qt.colorEqual(image.pixel(Math.round(x), Math.round(y)), "#101820"),
                       data.tag + " entry " + index + " is drawn where it travelled")
                ++drawn
            }
            // On an open path at most one entry is leaving and one arriving.
            verify(drawn >= entryCount - 2, data.tag + " shows its entries while they travel")
        }
        travel = loop
        wait(0)
        verify(grabImage(travelFixture).equals(rest),
               data.tag + ": a whole loop brings every entry back")
        travel = 0
    }

    function test_currentLayoutPixelParity_data() {
        return [
            { tag: "horizontal", layout: "horizontal" },
            { tag: "vertical", layout: "vertical" },
            { tag: "ring", layout: "ring" },
            { tag: "arc", layout: "arc" },
            { tag: "polygon", layout: "polygon" },
            { tag: "fan", layout: "fan" },
            { tag: "spiral", layout: "spiral" }
        ]
    }

    // TASK-0033 Phase B: sparse and dense free layouts render, and every
    // entry stays inside the panel the engine sized for it.
    function test_denseAndSparseLayoutsStayInsideTheirBounds_data() {
        const rows = []
        for (const layout of ["ring", "arc", "semicircle", "fan", "spiral",
                              "octagon"]) {
            for (const count of [1, 24])
                rows.push({ tag: layout + "/" + count, layout: layout,
                            count: count })
        }
        return rows
    }

    function test_denseAndSparseLayoutsStayInsideTheirBounds(data) {
        currentLayout = data.layout
        entryCount = data.count
        wait(0)

        const image = grabImage(canonicalFixture)
        compare(image.width, currentGeometry.width)
        compare(image.height, currentGeometry.height)
        verify(image.alpha(1, 1) > 0, data.tag + " fixture did not render")
        for (let index = 0; index < entryCount; ++index) {
            const point = LayoutEngine.position(
                currentLayout, index, entryCount, currentGeometry, 0, 6,
                "upright", "canonical")
            verify(point.x >= -0.0001 && point.y >= -0.0001
                   && point.x + currentGeometry.iconSize
                       <= currentGeometry.width + 0.0001
                   && point.y + currentGeometry.iconSize
                       <= currentGeometry.height + 0.0001,
                   data.tag + "[" + index + "] is inside the panel")
        }
        entryCount = 6
    }

    function test_currentLayoutPixelParity(data) {
        currentLayout = data.layout
        wait(0)

        const liveImage = grabImage(liveFixture)
        const canonicalImage = grabImage(canonicalFixture)
        compare(liveImage.width, currentGeometry.width)
        compare(liveImage.height, currentGeometry.height)
        compare(canonicalImage.width, liveImage.width)
        compare(canonicalImage.height, liveImage.height)
        verify(liveImage.alpha(1, 1) > 0,
               data.layout + " fixture did not render")
        verify(liveImage.equals(canonicalImage),
               data.layout + " visual placement changed")
    }
}
