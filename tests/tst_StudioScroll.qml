import QtQuick
import QtQuick.Controls
import QtTest
import "../qml/runtime" as Runtime
import "../qml/ArchDock/Rendering/inputs" as Inputs

TestCase {
    id: testCase
    name: "StudioScroll"
    when: windowShown
    visible: true
    width: 700
    height: 400

    property var studio: ({
        fieldValue: function() { return 5 }, fieldOptionIndex: function() { return 0 },
        displayFieldValue: function() { return "Value" }, formatFieldValue: function() { return "5" },
        colorPreviewValue: function() { return "red" }, colorDisplayValue: function() { return "red" },
        setFieldValue: function() {}, performStudioAction: function() {}, openColorEditor: function() {}
    })

    Component { id: formComponent; Runtime.StudioForm { width: 640; height: 300 } }

    function form() {
        const rows = []
        for (let i = 0; i < 20; ++i)
            rows.push({key: "field" + i, kind: i === 0 ? "spin" : i === 1 ? "combo" : "readonly",
                       label: "Field " + i, options: [{label: "First", value: 1}, {label: "Second", value: 2}]})
        const item = createTemporaryObject(formComponent, testCase, {studio: studio, rows: rows})
        verify(item !== null)
        verify(waitForRendering(item))
        wait(100)
        return item
    }

    function test_wheelOverControlsScrollsWithoutChangingValues() {
        const item = form()
        const spin = findChild(item, "studio-spin-field0")
        const combo = findChild(item, "studio-combo-field1")
        compare(spin.value, 5)
        mouseWheel(spin, 30, 15, 0, -120)
        wait(100)
        compare(spin.value, 5)
        verify(item.contentItem.contentY > 0)
        item.contentItem.contentY = 0
        mouseWheel(combo, 30, 15, 0, -120)
        wait(100)
        compare(combo.currentIndex, 0)
        verify(item.contentItem.contentY > 0)
    }

    function test_wheelOverLabelRespectsBoundsAndNoHorizontalOverflow() {
        const item = form()
        mouseWheel(item, 100, 100, 0, -120)
        wait(100)
        verify(item.contentItem.contentY > 0)
        mouseWheel(item, 100, 100, -120, 0)
        compare(item.contentItem.contentX, 0)
        item.contentItem.contentY = item.contentItem.contentHeight - item.contentItem.height
        const end = item.contentItem.contentY
        mouseWheel(item, 100, 100, 0, -120)
        wait(100)
        compare(item.contentItem.contentY, end)
    }

    Component {
        id: pageComponent
        Item {
            width: 640; height: 360
            property alias inner: innerForm.contentItem
            property alias outer: outerView
            Flickable {
                id: outerView
                anchors.fill: parent
                contentWidth: width; contentHeight: 600
                Runtime.StudioForm {
                    id: innerForm
                    y: 70; width: 640; height: 240
                    studio: testCase.studio
                    rows: Array.from({length: 20}, function(_, i) {
                        return {kind: "readonly", key: "row" + i, label: "Row " + i}
                    })
                }
            }
            Inputs.ScrollInput { flickables: [innerForm.contentItem, outerView] }
        }
    }

    function test_pageHeaderRoutesToInnerThenOuterAtBoundary() {
        const page = createTemporaryObject(pageComponent, testCase)
        verify(waitForRendering(page)); wait(100)
        mouseWheel(page, 100, 20, 0, -120)
        compare(page.outer.contentY, 0)
        verify(page.inner.contentY > 0)
        page.inner.contentY = page.inner.contentHeight - page.inner.height
        const end = page.inner.contentY
        mouseWheel(page, 100, 20, 0, -120)
        compare(page.inner.contentY, end)
        verify(page.outer.contentY > 0)
    }

    Component {
        id: tabsComponent
        TabBar {
            id: tabs
            width: 400
            Repeater {
                model: 20
                TabButton { required property int index; text: "Tab " + index; width: 104 }
            }
            Inputs.ScrollInput { parent: tabs; flickables: [tabs.contentItem]; horizontalOnly: true }
        }
    }

    function test_overflowingTabsAcceptHorizontalAndShiftWheelWithoutSelectionChanges() {
        const tabs = createTemporaryObject(tabsComponent, testCase)
        verify(waitForRendering(tabs)); wait(100)
        compare(tabs.count, 20)
        mouseWheel(tabs, 200, 15, -120, 0)
        verify(tabs.contentItem.contentX > 0)
        compare(tabs.currentIndex, 0)
        const first = tabs.contentItem.contentX
        mouseWheel(tabs, 200, 15, 0, -120, Qt.NoButton, Qt.ShiftModifier)
        verify(tabs.contentItem.contentX > first)
        compare(tabs.currentIndex, 0)
        tabs.contentItem.contentX = 0
        mouseClick(tabs.itemAt(1), 40, 15)
        compare(tabs.currentIndex, 1)
    }
}
