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
    property var propertiesRequests: []

    Component {
        id: dockEntryComponent

        DockUi.DockEntry {}
    }

    function entry(overrides) {
        const result = {
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
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    function createEntry(properties) {
        lastHoveredIndex = -2
        propertiesRequests = []
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
            openPanelStudio: function() {},
            openIconProperties: function(value) {
                testCase.propertiesRequests.push(value)
            }
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

    function iconStyleDefinition() {
        return {
            format: "org.archdock.icon-style",
            version: 1,
            id: "test-style",
            valid: true,
            loadable: true,
            glyphPolicy: { mode: "original", compatibleOnly: true },
            safeGlyphInset: { left: 0.1, top: 0.1, right: 0.1, bottom: 0.1 },
            layers: {
                rear: [],
                base: [{
                    id: "test-base",
                    kind: "procedural",
                    shape: "rounded-rect",
                    color: "#123456",
                    opacity: 1,
                    inset: 0.05,
                    radius: 0.2,
                    borderColor: "#abcdef",
                    borderWidth: 0.02
                }],
                front: []
            },
            states: [{
                id: "normal",
                rearOpacity: 1,
                baseOpacity: 1,
                frontOpacity: 1,
                glyphOpacity: 1,
                glyphScale: 1,
                borderColor: "#abcdef",
                glowColor: "transparent",
                glowOpacity: 0,
                reflectionOpacity: 0,
                indicatorColor: "#abcdef",
                indicatorOpacity: 0
            }],
            assetPaths: ({})
        }
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

    function test_iconStyleProjectionIsForwardedToSharedScene() {
        const item = createEntry({
            iconStyleDefinition: iconStyleDefinition()
        })
        compare(item.resolvedIconStyleId, "test-style")
    }

    function test_entryOverrideResolutionAffectsOnlyTheSharedVisual() {
        const overrideStyle = iconStyleDefinition()
        overrideStyle.id = "override-style"
        const item = createEntry({
            iconStyleDefinition: iconStyleDefinition(),
            entry: entry({
                resolvedGlyph: "utilities-terminal",
                tileEnabled: false
            }),
            iconOverrideResolution: ({
                overrideApplied: true,
                resolvedGlyph: "utilities-terminal",
                tileEnabled: false,
                iconStyleDefinition: overrideStyle
            })
        })
        compare(item.resolvedIconStyleId, "override-style")
        compare(item.resolvedIconSource, "utilities-terminal")
        compare(item.tileRenderingEnabled, false)
    }

    function test_iconPropertiesUsesStableEntryAndDismissesThePopup() {
        const item = createEntry()
        verify(item.iconPropertiesActionVisible)
        verify(item.openEntryContextMenu())
        tryCompare(item, "contextMenuVisible", true)

        verify(item.requestIconProperties())
        tryCompare(item, "contextMenuVisible", false)
        compare(propertiesRequests.length, 1)
        compare(propertiesRequests[0].stableIdentity,
                "application.org.example.app")
    }

    function test_runningOnlyEditAndDragStatesCannotOpenProperties() {
        const runningOnly = createEntry({
            entry: entry({
                pinned: false,
                running: true,
                iconPropertiesSupported: false
            })
        })
        verify(!runningOnly.iconPropertiesActionVisible)
        verify(!runningOnly.requestIconProperties())
        compare(propertiesRequests.length, 0)

        const supported = createEntry()
        supported.editMode = true
        verify(!supported.openEntryContextMenu())
        verify(!supported.requestIconProperties())
        supported.editMode = false
        supported.dragging = true
        verify(!supported.openEntryContextMenu())
        verify(!supported.requestIconProperties())
        compare(propertiesRequests.length, 0)
    }
}
