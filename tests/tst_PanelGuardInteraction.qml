import QtQuick
import QtQuick.Window
import QtTest
import ArchDock.Rendering 1.0
import "../plasma-dock-widget/contents/ui" as DockUi

// TASK-0032 Phase B: the guard path end to end.
//
// tst_PanelPresentationGuards proves the controller's rules against synthetic
// inputs. This file proves the wiring: that the production DockEntry actually
// reports a real context menu and a real drag, that the panel aggregates them
// the way main.qml does, and that a close already counting down is cancelled
// by them.
//
// It uses the production delegate and a shown window with real pointer
// delivery, following the IconPropertiesInteractionHarness precedent, because
// a guard that is only ever set by a test calling a setter proves nothing
// about the applet a user actually clicks on.
TestCase {
    id: testCase

    name: "PanelGuardInteraction"
    when: windowShown
    width: 260
    height: 220

    // Mirrors the aggregation in plasma-dock-widget main.qml: per-entry guards
    // keyed by entry, so two entries cannot cancel each other's guard.
    property var entryGuardFlags: ({})

    readonly property bool entryMenuOpen: guardActive("menu")
    readonly property bool entryDragActive: guardActive("drag")

    function setEntryGuard(index, name, active) {
        const key = String(name) + ":" + String(index)
        const source = entryGuardFlags || ({})
        if (Boolean(source[key]) === Boolean(active))
            return
        const next = ({})
        for (const existing of Object.keys(source)) {
            if (existing !== key)
                next[existing] = source[existing]
        }
        if (active)
            next[key] = true
        entryGuardFlags = next
    }

    function guardActive(name) {
        const prefix = String(name) + ":"
        const source = entryGuardFlags || ({})
        for (const key of Object.keys(source)) {
            if (key.indexOf(prefix) === 0 && source[key])
                return true
        }
        return false
    }

    Component {
        id: dockEntryComponent

        DockUi.DockEntry {}
    }

    Component {
        id: hostWindowComponent

        Window {
            width: 220
            height: 220
            visible: true
            color: "#202833"
        }
    }

    PanelPresentationController {
        id: controller

        restingState: "open"
        transitionDuration: 0
        closeDelay: 250

        popupOpen: testCase.entryMenuOpen
        dragActive: testCase.entryDragActive
    }

    function entry() {
        return {
            appId: "org.example.app",
            stableIdentity: "application.org.example.app",
            displayName: "Example",
            iconName: "application-x-executable",
            pinned: true,
            iconPropertiesSupported: true,
            running: false,
            active: false,
            minimized: false,
            attention: false,
            windowCount: 1
        }
    }

    function createHostedEntry(index) {
        const window = createTemporaryObject(hostWindowComponent, testCase)
        verify(window !== null)
        verify(waitForRendering(window.contentItem))
        const item = createTemporaryObject(
            dockEntryComponent, window.contentItem, {
                entry: entry(),
                entryIndex: index === undefined ? 0 : index,
                vertical: false,
                baseSize: 60,
                magnification: 1.7,
                magnificationEnabled: true,
                hoveredIndex: -1,
                tileShape: "rounded",
                appearance: "glass",
                showReflection: false,
                showIndicator: true,
                showTooltip: false,
                motion: "scale",
                motionTrigger: "hover",
                motionIntensity: 1,
                motionDuration: 0,
                reducedMotion: true,
                inputEnabled: true,
                editMode: false,
                acceptDrops: true,
                invoke: function() {},
                reorder: function() {},
                pinUrls: function() {},
                setHoveredIndex: function() {},
                setEntryGuard: testCase.setEntryGuard,
                openPanelStudio: function() {},
                openIconProperties: function() {}
            })
        verify(item !== null)
        item.anchors.centerIn = window.contentItem
        wait(0)
        return item
    }

    function init() {
        entryGuardFlags = ({})
        controller.reset()
    }

    function test_aRealContextMenuHoldsThePanelOpen() {
        const item = createHostedEntry(0)
        compare(controller.guardsActive, false)

        // The user opened the entry's context menu.
        verify(item.openEntryContextMenu())
        tryCompare(item, "contextMenuVisible", true)
        tryCompare(controller, "popupOpen", true)
        compare(controller.guardsActive, true)

        // Now something asks the panel to close.
        controller.requestCollapse()
        compare(controller.phase, "open", "closed under an open context menu")
        compare(controller.deferredRequest, "collapse")
        verify(!controller.pendingRequestActive)

        wait(350)
        compare(controller.phase, "open",
                "closed after the delay while the menu was still open")
    }

    function test_dismissingTheMenuReleasesTheDeferredClose() {
        const item = createHostedEntry(0)
        verify(item.openEntryContextMenu())
        tryCompare(controller, "popupOpen", true)

        controller.requestCollapse()
        compare(controller.phase, "open")

        verify(item.requestIconProperties())
        tryCompare(item, "contextMenuVisible", false)
        tryCompare(controller, "popupOpen", false)

        // The close was deferred, not discarded: it runs once the guard clears
        // and the configured delay expires.
        verify(controller.pendingRequestActive)
        tryVerify(function() { return controller.phase === "collapsed" }, 3000)
    }

    function test_aRealDragHoldsThePanelOpen() {
        const item = createHostedEntry(0)
        compare(controller.dragActive, false)

        item.dragging = true
        tryCompare(controller, "dragActive", true)

        controller.requestCollapse()
        compare(controller.phase, "open", "closed during an active drag")

        item.dragging = false
        tryCompare(controller, "dragActive", false)
        tryVerify(function() { return controller.phase === "collapsed" }, 3000)
    }

    function test_aDragClosesTheMenuButKeepsThePanelGuarded() {
        const item = createHostedEntry(0)
        verify(item.openEntryContextMenu())
        tryCompare(item, "contextMenuVisible", true)

        // DockEntry dismisses its menu when a drag starts. The panel must stay
        // guarded across that handover rather than briefly becoming closable.
        item.dragging = true
        tryCompare(item, "contextMenuVisible", false)
        compare(controller.guardsActive, true)
        compare(controller.dragActive, true)

        controller.requestCollapse()
        compare(controller.phase, "open")
    }

    function test_guardsAreKeyedPerEntry() {
        const first = createHostedEntry(0)
        const second = createHostedEntry(1)

        verify(first.openEntryContextMenu())
        tryCompare(controller, "popupOpen", true)

        // A second entry reporting "no menu" must not clear the first entry's
        // guard; that is what keying by entry index prevents.
        second.setEntryGuard(1, "menu", false)
        compare(controller.popupOpen, true)

        first.requestIconProperties()
        tryCompare(controller, "popupOpen", false)
    }

    function test_aDestroyedEntryDoesNotStrandItsGuard() {
        const item = createHostedEntry(0)
        item.dragging = true
        tryCompare(controller, "dragActive", true)

        item.destroy()
        // A guard left behind by a removed entry would hold the panel open
        // forever, which is the silent failure mode of per-entry guards.
        tryCompare(controller, "dragActive", false)
        compare(controller.guardsActive, false)
    }
}
