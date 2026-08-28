import QtQuick
import QtTest
import "../plasma-dock-widget/contents/ui" as DockUi

TestCase {
    id: testCase

    name: "DockEntry"
    when: windowShown
    width: 260
    height: 220

    property int lastHoveredIndex: -2

    Component {
        id: dockEntryComponent

        DockUi.DockEntry {}
    }

    function entry(overrides) {
        const result = {
            appId: "org.example.app",
            displayName: "Example",
            iconName: "application-x-executable",
            pinned: true,
            running: false,
            active: false,
            minimized: false,
            attention: false,
            windowCount: 1
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    function createEntry(properties) {
        lastHoveredIndex = -2
        const values = {
            entry: entry(),
            entryIndex: 1,
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
            setHoveredIndex: function(value) {
                testCase.lastHoveredIndex = value
            },
            openPanelStudio: function() {}
        }
        const additions = properties || ({})
        for (const key of Object.keys(additions))
            values[key] = additions[key]
        const item = createTemporaryObject(
            dockEntryComponent, testCase, values)
        verify(item !== null)
        wait(0)
        return item
    }

    function test_magnificationKeepsLogicalBoundsStable() {
        const item = createEntry()
        compare(item.width, 60)
        compare(item.height, 60)
        compare(item.visualScale, 1)
        compare(item.logicalInputRegion.width, 60)
        compare(item.logicalInputRegion.height, 60)

        item.hoveredIndex = 1
        wait(0)

        compare(item.width, 60)
        compare(item.height, 60)
        compare(item.visualScale, 1.7)
        compare(item.logicalInputRegion.width, 60)
        compare(item.logicalInputRegion.height, 60)
    }

    function test_entryStateIsForwardedToSharedScene() {
        const item = createEntry({
            entry: entry({ running: true })
        })
        compare(item.iconVisualState, "running")

        item.entry = entry({ active: true, running: true })
        compare(item.iconVisualState, "active")
        item.entry = entry({ minimized: true })
        compare(item.iconVisualState, "minimized")
        item.clickPulse = true
        compare(item.iconVisualState, "pressed")
        item.clickPulse = false
        item.dragging = true
        compare(item.iconVisualState, "drop")
        item.editMode = true
        compare(item.iconVisualState, "edit")
    }
}
