import QtQuick
import QtQuick.Window
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

    // Real pointer delivery needs a shown window, matching the existing
    // IconPropertiesInteractionHarness pattern.
    Component {
        id: hostWindowComponent

        Window {
            width: 220
            height: 220
            visible: true
            color: "#202833"
        }
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

    function createEntry(properties, host) {
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
            dockEntryComponent, host || testCase, values)
        verify(item !== null)
        wait(0)
        return item
    }

    function createHostedEntry(properties) {
        const window = createTemporaryObject(hostWindowComponent, testCase)
        verify(window !== null)
        verify(waitForRendering(window.contentItem))
        const item = createEntry(properties, window.contentItem)
        item.anchors.centerIn = window.contentItem
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

    function motionProfile(overrides) {
        const result = {
            id: "test-motion",
            name: "Test Motion",
            valid: true,
            target: "icon",
            trigger: "hover-hold",
            timing: { baseDuration: 200, speedScale: 1, startDelay: 0 },
            tracks: [{
                id: "travel",
                property: "translate-y",
                from: 0,
                to: -24,
                easing: "linear",
                duration: 80,
                delay: 0,
                phase: 0,
                direction: "normal",
                repeat: 1,
                intensityScale: 1,
                blend: "replace",
                priority: 0
            }],
            rendererRequirements: ["procedural2d"],
            reducedMotion: { mode: "none" },
            totalDuration: 80,
            continuous: false
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    // A visual transform must never move the logical pointer target. This is
    // the boundary that keeps profile motion from stealing clicks.
    function test_profileMotionNeverMovesTheLogicalInputRegion() {
        const item = createEntry({ reducedMotion: false })
        // The hit-area guarantee belongs to the controller, so bind it there
        // directly rather than through the effect-selection gate.
        item.motionController.profiles = [motionProfile()]
        wait(0)
        compare(item.logicalInputRegion.x, 0)
        compare(item.logicalInputRegion.y, 0)
        compare(item.logicalInputRegion.width, 60)
        compare(item.logicalInputRegion.height, 60)

        item.motionController.hovered = true
        wait(0)
        compare(item.motionController.activeTracks.length, 1)

        // Let the track travel, then confirm the entry itself never moved.
        tryVerify(function() {
            return item.motionController.channelValue("icon", "translate-y")
                < -2
        }, 2000)

        compare(item.width, 60)
        compare(item.height, 60)
        compare(item.x, 0)
        compare(item.y, 0)
        compare(item.logicalInputRegion.x, 0)
        compare(item.logicalInputRegion.y, 0)
        compare(item.logicalInputRegion.width, 60)
        compare(item.logicalInputRegion.height, 60)
    }

    // Composed scale is a visual transform too, and must not resize the entry.
    function test_profileScaleDoesNotResizeTheEntry() {
        const item = createEntry({ reducedMotion: false })
        item.motionController.profiles = [motionProfile({
                id: "grow",
                tracks: [{
                    id: "swell",
                    property: "scale",
                    from: 1,
                    to: 2,
                    easing: "linear",
                    duration: 80,
                    delay: 0,
                    phase: 0,
                    direction: "normal",
                    repeat: 1,
                    intensityScale: 1,
                    blend: "replace",
                    priority: 0
                }]
        })]
        wait(0)
        item.motionController.hovered = true
        wait(0)
        tryVerify(function() {
            return item.motionController.channelValue("icon", "scale") > 1.2
        }, 2000)

        compare(item.width, 60)
        compare(item.height, 60)
        compare(item.logicalInputRegion.width, 60)
        compare(item.logicalInputRegion.height, 60)
    }

    function test_pointerHandlersDispatchTruthfulMotionEvents() {
        const item = createHostedEntry({ reducedMotion: false })
        const seen = []
        item.motionController.eventDispatched.connect(function(name) {
            seen.push(name)
        })

        mouseMove(item, 30, 30)
        mouseClick(item, 30, 30)
        wait(0)

        verify(seen.indexOf("click") >= 0)
        // A click asks for a launch; it never reports one as succeeded.
        verify(seen.indexOf("launch-requested") >= 0)
        verify(seen.indexOf("launch-succeeded") < 0)
        verify(seen.indexOf("launch-failed") < 0)
    }

    function test_runningTransitionsAreReportedTruthfully() {
        const item = createEntry({ reducedMotion: false })
        const seen = []
        item.motionController.eventDispatched.connect(function(name) {
            seen.push(name)
        })

        // Running is continuous: the controller carries it as state, which is
        // what a `running-started` trigger reads.
        item.entry = entry({ running: true })
        wait(0)
        compare(item.motionController.running, true)
        verify(seen.indexOf("running-stopped") < 0)

        // Stopping is a discrete transition and is dispatched as an event.
        item.entry = entry({ running: false })
        wait(0)
        compare(item.motionController.running, false)
        verify(seen.indexOf("running-stopped") >= 0)
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
