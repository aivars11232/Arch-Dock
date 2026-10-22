import QtQuick
import QtTest
import "../qml/runtime" as Runtime

TestCase {
    id: testCase
    name: "StudioWidgetsPage"
    when: windowShown
    width: 920
    height: 700
    visible: true
    Component { id: pageComponent; Runtime.StudioWidgetsPage { width: 900; height: 680 } }
    Component { id: spyComponent; SignalSpy {} }
    readonly property var barWidgets: [
        { key: "dock", plugin: "org.archdock.dock", name: "Arch Dock icons", icon: "org.archdock.ArchDock" },
        { key: "w1", plugin: "org.kde.plasma.digitalclock", name: "Digital Clock", icon: "clock" },
        { key: "w2", plugin: "org.kde.plasma.systemtray", name: "System Tray", icon: "preferences-desktop-notification" }]
    readonly property var installed: [
        { plugin: "org.kde.plasma.digitalclock", name: "Digital Clock", icon: "clock", description: "Date and time" },
        { plugin: "org.kde.plasma.kickoff", name: "Application Launcher", icon: "start-here-kde",
          description: "Launcher to start applications" },
        { plugin: "org.kde.plasma.systemtray", name: "System Tray", icon: "preferences-desktop-notification",
          description: "Access hidden applications minimized in the system tray" }]
    function page(properties) {
        const item = createTemporaryObject(pageComponent, testCase, properties || {})
        verify(item !== null)
        verify(waitForRendering(item))
        return item
    }
    function spy(item, signalName) {
        const value = createTemporaryObject(spyComponent, testCase, { target: item, signalName: signalName })
        verify(value.valid)
        return value
    }
    function shown(item, name) {
        const child = findChild(item, name)
        return child !== null && child.visible
    }

    // The bar's widgets in their order, each moved along the bar or removed;
    // the dock's icons stay, and nothing moves past the bar's ends.
    function test_widgetsMoveAlongTheBarAndGo() {
        const item = page({ widgets: barWidgets })
        const moves = spy(item, "moveRequested")
        const removals = spy(item, "removeRequested")
        verify(shown(item, "studio-bar-widget-dock"))
        verify(!shown(item, "studio-bar-widget-earlier-dock"))
        verify(shown(item, "studio-bar-widget-later-dock"))
        verify(!shown(item, "studio-bar-widget-remove-dock"))
        verify(!shown(item, "studio-bar-widget-later-w2"))
        compare(findChild(item, "studio-bar-widget-earlier-w1").text, "Move left")
        mouseClick(findChild(item, "studio-bar-widget-earlier-w1"))
        compare(moves.count, 1)
        compare(moves.signalArguments[0][0], "w1")
        compare(moves.signalArguments[0][1], 0)
        mouseClick(findChild(item, "studio-bar-widget-remove-w2"))
        compare(removals.count, 1)
        compare(removals.signalArguments[0][0], "w2")
        // A side bar's widgets stand one above another.
        item.upright = true
        compare(findChild(item, "studio-bar-widget-earlier-w1").text, "Move up")
        compare(findChild(item, "studio-bar-widget-later-w1").text, "Move down")
        // While the bar cannot change, nothing is offered to press.
        item.actionsEnabled = false
        verify(!findChild(item, "studio-bar-widget-remove-w1").enabled)
    }

    // Plasma's installed widgets are listed as the list opens, read anew each
    // time, found by any word of their name, description or id, and added.
    function test_widgetsAreFoundAndAdded() {
        const item = page({ widgets: barWidgets })
        const opened = spy(item, "installedListOpened")
        const added = spy(item, "addRequested")
        verify(!shown(item, "studio-bar-widget-search"))
        mouseClick(findChild(item, "studio-bar-widget-choose"))
        compare(opened.count, 1)
        item.installed = installed
        verify(shown(item, "studio-bar-widget-search"))
        compare(item.found.length, 3)
        findChild(item, "studio-bar-widget-search").text = "launcher start"
        compare(item.found.length, 1)
        compare(item.found[0].plugin, "org.kde.plasma.kickoff")
        verify(waitForRendering(item))
        mouseClick(findChild(item, "studio-bar-widget-add-org.kde.plasma.kickoff"))
        compare(added.count, 1)
        compare(added.signalArguments[0][0], "org.kde.plasma.kickoff")
        findChild(item, "studio-bar-widget-search").text = "no such widget"
        compare(item.found.length, 0)
        verify(shown(item, "studio-bar-widget-none-found"))
        // Closed and opened again, the list is read anew and the search is empty.
        mouseClick(findChild(item, "studio-bar-widget-choose"))
        verify(!shown(item, "studio-bar-widget-search"))
        mouseClick(findChild(item, "studio-bar-widget-choose"))
        compare(opened.count, 2)
        compare(findChild(item, "studio-bar-widget-search").text, "")
    }

    // A change the bar could not take is said in the page.
    function test_aRefusedChangeIsSaid() {
        const item = page({ widgets: barWidgets, notice: "The bar's widgets could not be changed just now." })
        verify(shown(item, "studio-bar-widget-notice"))
        item.notice = ""
        verify(!shown(item, "studio-bar-widget-notice"))
    }
}
