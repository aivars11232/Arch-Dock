import QtQuick
import QtQuick.Window
import QtTest
import ArchDock.Rendering 1.0
import org.kde.plasma.core as PlasmaCore
import "../plasma-dock-widget/contents/ui" as DockUi

TestCase {
    id: testCase
    name: "FolderExpansion"
    when: windowShown
    visible: true
    width: 700
    height: 500
    Component { id: component; FolderExpansion {} }
    Component { id: hostComponent; DockUi.FolderExpansionHost {} }
    Component {
        id: anchorComponent
        Window {
            width: 1000; height: 600; visible: true
            Item { objectName: "anchor"; x: 468; y: 400; width: 56; height: 56 }
        }
    }
    SignalSpy { id: selection; signalName: "childSelected" }
    SignalSpy { id: dismissal; signalName: "dismissRequested" }
    function rows(count) {
        const result = []
        for (let i = 0; i < count; ++i)
            result.push({ id: "child-" + i, displayName: "Document " + i,
                iconName: "text-plain", selectable: i !== 2 })
        return result
    }
    function popup(layout, count, reduced) {
        const item = createTemporaryObject(component, testCase, {
            layout: layout, reducedMotion: reduced === undefined ? true : reduced,
            folderTitle: "Documents", snapshot: { status: "ready", entries: rows(count), truncated: count === 48 }
        })
        verify(item !== null)
        item.width = item.implicitWidth
        item.height = item.implicitHeight
        verify(waitForRendering(item))
        selection.target = item; selection.clear()
        dismissal.target = item; dismissal.clear()
        return item
    }
    function cleanup() { selection.target = null; dismissal.target = null }
    function test_layoutSelection_data() {
        return ["fan", "grid", "stack", "arc", "ring"].map(function(layout) {
            return { tag: layout, layout: layout }
        })
    }
    function test_layoutSelection(data) {
        const item = popup(data.layout, 5)
        compare(item.geometry.layout, data.layout)
        const first = findChild(item, "folder-child-0")
        mouseClick(first, 8, 8)
        compare(selection.count, 1)
        compare(selection.signalArguments[0][0], "child-0")
        item.forceActiveFocus()
        keyClick(Qt.Key_Right)
        compare(item.selectedChildId, "child-1")
        keyClick(Qt.Key_Return)
        compare(selection.count, 2)
        compare(selection.signalArguments[1][0], "child-1")
        keyClick(Qt.Key_Right)
        keyClick(Qt.Key_Return)
        compare(selection.count, 2, "blocked child never dispatches")
        verify(!item.selectChild("foreign"))
        keyClick(Qt.Key_Escape)
        compare(dismissal.count, 1)
        item.snapshot = { status: "empty", entries: [] }
        compare(item.selectedChildId, "")
        verify(!item.selectChild("child-1"), "removed children cannot dispatch")
    }
    function test_denseFallbackAndReducedMotion() {
        const item = popup("spiral", 48)
        verify(item.geometry.fallbackApplied)
        compare(item.geometry.layout, "fan")
        verify(item.width <= 640 && item.height <= 420)
        for (let i = 0; i < 47; ++i) item.moveSelection(1)
        compare(item.selectedChildId, "child-47")
        verify(findChild(item, "folderViewport").contentX > 0)
        const child = findChild(item, "folder-child-47")
        verify(child !== null)
        verify(item.selectChild("child-47"))
        item.opened = false
        verify(!item.selectChild("child-47"))
        compare(item.openingProfiles[0].tracks[0].duration, 260)
        item.duration = 9999
        compare(item.openingProfiles[0].tracks[0].duration, 1200)
    }
    function test_motionUsesSharedController() {
        const item = popup("grid", 1, false)
        const child = findChild(item, "folder-child-0")
        const controller = child.children[1]
        tryCompare(controller, "activeProfileIds", ["folder-open"])
        item.reducedMotion = true
        tryCompare(controller, "activeTracks", [])
    }
    function test_nativeHostLifecycle() {
        const window = createTemporaryObject(anchorComponent, null)
        verify(window !== null)
        const host = createTemporaryObject(hostComponent, testCase, {
            visualParent: findChild(window, "anchor"), reducedMotion: true,
            snapshot: { status: "empty", entries: [] }, folderTitle: "Empty folder"
        })
        verify(host !== null)
        compare(host.type, PlasmaCore.Dialog.AppletPopup)
        verify(host.openFolder())
        tryCompare(host, "visible", true)
        host.mainItem.dismissRequested()
        tryCompare(host, "visible", false)
        verify(!host.requested)
        verify(host.openFolder())
        host.interactionAllowed = false
        tryCompare(host, "visible", false)
        verify(!host.requested)
        verify(!host.openFolder())
    }
}
