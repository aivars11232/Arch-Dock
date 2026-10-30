import QtQuick
import QtTest

// An Arch Dock bar's view (the bar shell's Bar.qml): the bar's containment
// fills the window, but for the room the window reaches past the bar's inner
// side while the bar hides at times, for its dock's stands and holograms;
// there only what the dock shows takes the pointer.
TestCase {
    id: testCase
    name: "BarView"
    width: 400
    height: 400
    when: windowShown

    Component {
        id: containmentComponent
        Item {
            property real wantedReach: 0
            property var shownPastRects: []
            implicitWidth: 300
            implicitHeight: 60
        }
    }

    function makeView(edge, reach, shown) {
        const component = Qt.createComponent(Qt.resolvedUrl("../plasma-bar-shell/contents/views/Bar.qml"))
        verify(component.status === Component.Ready, component.errorString())
        const vertical = edge === "left" || edge === "right"
        const view = createTemporaryObject(component, testCase, {
            width: vertical ? 60 + reach : 300, height: vertical ? 300 : 60 + reach,
            vertical: vertical, edge: edge, barThickness: 60 })
        verify(view)
        view.containment = createTemporaryObject(containmentComponent, testCase,
            { wantedReach: reach, shownPastRects: shown || [] })
        return view
    }

    function inputShape(view) {
        for (let index = 0; index < view.data.length; ++index)
            if (view.data[index] && view.data[index].rects !== undefined)
                return view.data[index]
        return null
    }

    function compareRect(actual, x, y, width, height, what) {
        compare(Qt.rect(actual.x, actual.y, actual.width, actual.height), Qt.rect(x, y, width, height), what)
    }

    function test_theRoomLiesPastTheInnerSide_data() {
        return [
            { tag: "top", edge: "top", bar: [0, 0, 300, 60], shown: [10, 70, 20, 30], inWindow: [10, 70, 20, 30] },
            { tag: "bottom", edge: "bottom", bar: [0, 40, 300, 60], shown: [10, -30, 20, 30], inWindow: [10, 10, 20, 30] },
            { tag: "left", edge: "left", bar: [0, 0, 60, 300], shown: [70, 10, 30, 20], inWindow: [70, 10, 30, 20] },
            { tag: "right", edge: "right", bar: [40, 0, 60, 300], shown: [-30, 10, 30, 20], inWindow: [10, 10, 30, 20] }
        ]
    }

    function test_theRoomLiesPastTheInnerSide(data) {
        const s = data.shown
        const view = makeView(data.edge, 40, [Qt.rect(s[0], s[1], s[2], s[3])])
        const b = data.bar
        compareRect(view.containment, b[0], b[1], b[2], b[3], "the bar stays at its edge, as deep as it is")
        compare(view.wantedReach, 40, "the room the bar asks for is read from it")
        const shape = inputShape(view)
        verify(shape, "the view shapes the window's input")
        verify(shape.enabled, "with room past the bar, only parts of the window take the pointer")
        compare(shape.rects.length, 2)
        compareRect(shape.rects[0], b[0], b[1], b[2], b[3], "the bar takes the pointer")
        const w = data.inWindow
        compareRect(shape.rects[1], w[0], w[1], w[2], w[3], "and what the dock shows past it")
    }

    function test_noRoomNoShape() {
        const view = makeView("top", 0)
        compareRect(view.containment, 0, 0, 300, 60, "the bar fills its window")
        verify(!inputShape(view).enabled, "the whole window takes the pointer, as before")
    }

    function test_theBarKeepsItsDepthWhileTheWindowGrows() {
        // The bar host sets the bar's depth before its window has grown.
        const view = makeView("top", 0)
        view.containment.wantedReach = 40
        compareRect(view.containment, 0, 0, 300, 60, "the bar keeps its depth")
        verify(!inputShape(view).enabled)
        view.height = 100
        compare(view.reachPast, 40, "the room past the bar is what the window has grown by")
        compareRect(view.containment, 0, 0, 300, 60, "the bar stays at its edge")
        verify(inputShape(view).enabled)
    }

    function test_editRoomAndReach() {
        const view = makeView("top", 40)
        view.editRoomBefore = 30
        view.width = 330
        compareRect(view.containment, 30, 0, 300, 60, "edit mode's room before the bar, the reach below it")
        compareRect(inputShape(view).rects[0], 0, 0, 330, 60, "the bar's band takes the pointer, edit room included")
    }
}
