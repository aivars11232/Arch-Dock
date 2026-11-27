import QtQuick
import QtTest

// An Arch Dock bar's view (the bar shell's Bar.qml): the bar's containment
// fills the window, but for the room the window reaches past the bar's inner
// side for its dock's stands and holograms, where only what the dock shows
// takes the pointer, and the gap at the screen's edge a hiding bar's window
// covers, which takes the pointer with the bar.
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

    function makeView(edge, reach, shown, gap) {
        const component = Qt.createComponent(Qt.resolvedUrl("../plasma-bar-shell/contents/views/Bar.qml"))
        verify(component.status === Component.Ready, component.errorString())
        const vertical = edge === "left" || edge === "right"
        const depth = 60 + reach + (gap || 0)
        const view = createTemporaryObject(component, testCase, {
            width: vertical ? depth : 300, height: vertical ? 300 : depth,
            vertical: vertical, edge: edge, barThickness: 60, edgeGap: gap || 0 })
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

    // A hiding bar off the screen's edge: its window also covers the gap
    // between the bar and the edge, where KWin shows it again; the bar keeps
    // its place off the edge, the gap is not room past it, and the gap takes
    // the pointer with the bar.
    function test_aHidingBarsWindowCoversItsEdgeGap_data() {
        return [
            { tag: "top", edge: "top", bar: [0, 8, 300, 60], band: [0, 0, 300, 68],
              shown: [10, 70, 20, 30], inWindow: [10, 78, 20, 30] },
            { tag: "bottom", edge: "bottom", bar: [0, 40, 300, 60], band: [0, 40, 300, 68],
              shown: [10, -30, 20, 30], inWindow: [10, 10, 20, 30] },
            { tag: "left", edge: "left", bar: [8, 0, 60, 300], band: [0, 0, 68, 300],
              shown: [70, 10, 30, 20], inWindow: [78, 10, 30, 20] },
            { tag: "right", edge: "right", bar: [40, 0, 60, 300], band: [40, 0, 68, 300],
              shown: [-30, 10, 30, 20], inWindow: [10, 10, 30, 20] }
        ]
    }

    function test_aHidingBarsWindowCoversItsEdgeGap(data) {
        const s = data.shown
        const view = makeView(data.edge, 40, [Qt.rect(s[0], s[1], s[2], s[3])], 8)
        const b = data.bar
        compareRect(view.containment, b[0], b[1], b[2], b[3], "the bar keeps its place off the edge")
        compare(view.reachPast, 40, "the gap is not room past the bar")
        const shape = inputShape(view)
        verify(shape.enabled)
        compare(shape.rects.length, 2)
        const band = data.band
        compareRect(shape.rects[0], band[0], band[1], band[2], band[3], "the bar and the gap take the pointer")
        const w = data.inWindow
        compareRect(shape.rects[1], w[0], w[1], w[2], w[3], "and what the dock shows past the bar")
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
