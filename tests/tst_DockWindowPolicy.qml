import QtQuick
import QtTest
import "../plasma-dock-widget/contents/ui/DockWindowPolicy.js" as DockWindowPolicy

TestCase {
    name: "DockWindowPolicy"

    // Only a Plasma edge panel always shown draws its stands and holograms
    // in Arch Dock's window over the panel; a bar never does.
    function test_overlayOnlyForPlasmaPanels() {
        verify(DockWindowPolicy.overlayWanted(true, true, "always"))
        verify(DockWindowPolicy.overlayWanted(true, true, undefined))
        verify(!DockWindowPolicy.overlayWanted(true, true, "auto-hide"))
        verify(!DockWindowPolicy.overlayWanted(true, false, "always"))
        verify(!DockWindowPolicy.overlayWanted(false, true, "always"), "a bar or a free panel")
    }

    // Every bar with stands or holograms reaches past itself in its own
    // window, whether it hides at times or not.
    function test_everyBarWithStandsOrHologramsReachesPast() {
        verify(DockWindowPolicy.barReachWanted(true, true))
        verify(!DockWindowPolicy.barReachWanted(true, false))
        verify(!DockWindowPolicy.barReachWanted(false, true))
    }

    // A place on the screen is taken from where the widget area lies on the
    // screen (below a top panel, beside a side one, or at the screen's start
    // on a right-to-left desktop); a command without a space (an older Arch
    // Dock's) is already in the widget area.
    function test_freePlaceOnTheScreen() {
        compare(DockWindowPolicy.freeTarget({ x: 128, y: 210 }, { x: 0, y: 66 }, "screen"), { x: 128, y: 144 })
        compare(DockWindowPolicy.freeTarget({ x: 128, y: 210 }, { x: 48, y: 66 }, "screen"), { x: 80, y: 144 })
        compare(DockWindowPolicy.freeTarget({ x: 128, y: 210 }, { x: 0, y: 66 }, undefined), { x: 128, y: 210 })
        compare(DockWindowPolicy.freeTarget({ x: 128, y: 210 }, null, "screen"), { x: 128, y: 210 })
    }

    // A place in a panel's own room keeps to the widget area's edge, as
    // Plasma keeps its widgets: never under a top or side panel.
    function test_freePlaceKeepsOutOfThePanelsRoom() {
        compare(DockWindowPolicy.freeTarget({ x: 128, y: 10 }, { x: 0, y: 82 }, "screen"), { x: 128, y: 0 })
        compare(DockWindowPolicy.freeTarget({ x: 20, y: 210 }, { x: 48, y: 0 }, "screen"), { x: 0, y: 210 })
        compare(DockWindowPolicy.freeTarget({ x: 128, y: 10 }, { x: 0, y: 82 }, undefined), { x: 128, y: 10 },
                "an older command is not changed")
    }
}
