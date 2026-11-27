import QtQuick
import QtQuick.Window
import QtTest
import org.kde.layershell 1.0 as LayerShell

// The clear window an always-shown Plasma edge panel's dock is drawn in
// (OverflowHost.qml), past the panel: the dock lies over the placeholder it
// leaves in the panel wherever Plasma moves it, and the window is deep
// enough for all the dock shows past the panel. The window is never shown
// here (not wanted); only its numbers are read.
TestCase {
    id: testCase
    name: "OverflowHost"
    width: 600
    height: 200
    when: windowShown

    // A widget's container in the panel, and the placeholder the dock
    // leaves in it, thinner than the panel's window.
    Item {
        id: container
        x: 100
        y: 20
        width: 300
        height: 60
        Item {
            id: placeholder
            x: 10
            y: 4
            width: 200
            height: 48
        }
    }

    function makeHost(edge, reach) {
        const component = Qt.createComponent(Qt.resolvedUrl("../plasma-dock-widget/contents/ui/OverflowHost.qml"))
        verify(component.status === Component.Ready, component.errorString())
        const host = createTemporaryObject(component, testCase,
            { edge: edge, placeholder: placeholder, reach: reach, wanted: false })
        verify(host)
        verify(!host.visible, "a window not wanted is not shown")
        return host
    }

    function cleanup() {
        container.x = 100
        container.y = 20
        placeholder.x = 10
    }

    // Plasma moves the dock's container when a widget beside it grows or
    // goes: the dock drawn over the panel follows it there.
    function test_theDockFollowsItsPlaceholderThroughItsContainer() {
        const host = makeHost("bottom", 40)
        compare(host.placeholderInPanel, Qt.point(110, 24))
        container.x = 160
        tryCompare(host, "placeholderInPanel", Qt.point(170, 24))
        container.y = 26
        tryCompare(host, "placeholderInPanel", Qt.point(170, 30))
        const panelWindow = testCase.Window.window
        compare(host.dockPlace, Qt.point(panelWindow.x + 170 - host.origin.x, panelWindow.y + 30 - host.origin.y),
                "the dock lies over its placeholder in this window")
        placeholder.x = 30
        tryCompare(host, "placeholderInPanel", Qt.point(190, 30))
    }

    // As deep as the panel's window, which reaches the screen's edge across
    // a floating panel's gap, and the dock's reach past it, on every edge.
    function test_asDeepAsThePanelWindowAndTheReach_data() {
        return [
            { tag: "bottom", edge: "bottom", across: "height" },
            { tag: "top", edge: "top", across: "height" },
            { tag: "left", edge: "left", across: "width" },
            { tag: "right", edge: "right", across: "width" }
        ]
    }

    function test_asDeepAsThePanelWindowAndTheReach(data) {
        const host = makeHost(data.edge, 40)
        const panelWindow = testCase.Window.window
        compare(host.depth, Math.ceil(panelWindow[data.across] + 40),
                "the placeholder alone (" + placeholder[data.across] + ") would cut what reaches past the panel")
    }

    // The window keeps Arch Dock's own kind, so KWin never counts it as a
    // Plasma panel (Arch Dock reads Plasma's panels from KWin's list).
    function test_keepsItsOwnKind() {
        const host = makeHost("bottom", 40)
        compare(host.LayerShell.Window.scope, "archdock-dock")
    }
}
