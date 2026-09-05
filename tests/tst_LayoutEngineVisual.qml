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
