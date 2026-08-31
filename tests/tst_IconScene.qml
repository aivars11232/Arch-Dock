import QtQuick
import QtTest
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "IconScene"
    when: windowShown
    width: 240
    height: 240

    Component {
        id: iconSceneComponent

        IconScene {}
    }

    function createScene(properties) {
        const values = {
            entry: {
                appId: "org.example.app",
                displayName: "Example",
                iconName: "application-x-executable",
                windowCount: 2
            },
            logicalSize: 64,
            tileShape: "rounded",
            appearance: "glass",
            showReflection: true,
            showIndicator: true,
            indicatorStyle: ({ mode: "bar", thickness: 4 }),
            reducedMotion: true
        }
        const additions = properties || ({})
        for (const key of Object.keys(additions))
            values[key] = additions[key]
        const scene = createTemporaryObject(
            iconSceneComponent, testCase, values)
        verify(scene !== null)
        wait(0)
        return scene
    }

    function test_namedLayersAndSlots() {
        const scene = createScene({
            running: true,
            badgeText: "3",
            progress: 0.5
        })

        compare(scene.rearLayerItem.objectName, "icon-layer-rear")
        compare(scene.baseLayerItem.objectName, "icon-layer-base")
        compare(scene.glyphLayerItem.objectName, "icon-layer-glyph")
        compare(scene.frontLayerItem.objectName, "icon-layer-front")
        compare(scene.indicatorItem.objectName, "running-indicator")
        compare(scene.statusLayerItem.objectName, "icon-layer-status")
        compare(scene.indicatorItem.mode, "bar")
        compare(scene.indicatorItem.resolvedThickness, 4)
        compare(scene.badgeText, "3")
        compare(scene.progress, 0.5)
        compare(scene.badgeSlotActive, true)
        compare(scene.progressSlotActive, true)
        verify(scene.badgeItem !== null)
        verify(scene.progressItem !== null)
    }

    function test_explicitVisualStates() {
        const scene = createScene()
        compare(scene.visualState, "normal")

        scene.running = true
        compare(scene.visualState, "running")
        scene.minimized = true
        compare(scene.visualState, "minimized")
        scene.launching = true
        compare(scene.visualState, "launching")
        scene.launching = false
        scene.active = true
        compare(scene.visualState, "active")
        scene.hovered = true
        compare(scene.visualState, "hover")
        scene.pressed = true
        compare(scene.visualState, "pressed")
        scene.urgent = true
        compare(scene.visualState, "urgent")
        scene.dropTarget = true
        compare(scene.visualState, "drop")
        scene.editMode = true
        compare(scene.visualState, "edit")
        scene.editMode = false
        scene.disabled = true
        compare(scene.visualState, "disabled")
    }

    function test_invalidStyleFallsBackWithoutReplacingTheGlyph() {
        const scene = createScene({
            iconStyleDefinition: ({
                format: "org.archdock.icon-style",
                version: 1,
                id: "invalid-style",
                valid: false,
                loadable: false,
                states: []
            })
        })

        compare(scene.resolvedIconStyle.valid, false)
        compare(scene.resolvedIconStyle.styleId, "plain-original")
        compare(scene.resolvedIconStyle.fallbackApplied, true)
        compare(scene.resolvedIconStyle.replacementApplied, false)
        compare(scene.resolvedIconSource, "application-x-executable")
        compare(scene.styledLayersActive, false)
    }

    function test_visualScaleDoesNotChangeLogicalBounds() {
        const scene = createScene()
        const logicalBounds = JSON.stringify(scene.logicalInputRegion)
        compare(scene.width, 64)
        compare(scene.height, 64)

        scene.visualScale = 1.75
        scene.hovered = true
        scene.pressed = true
        wait(0)

        compare(scene.width, 64)
        compare(scene.height, 64)
        compare(JSON.stringify(scene.logicalInputRegion), logicalBounds)
        compare(scene.visualLayerItem.scale, 1.75)
    }

    function test_indicatorStyleAndStateAreSharedInputs() {
        const scene = createScene({
            vertical: true,
            active: true,
            running: true,
            urgent: true,
            windowCount: 4,
            indicatorStyle: ({
                mode: "dot",
                activeColor: "#112233",
                urgentColor: "#ff3344",
                thickness: 5,
                length: 14,
                pulse: false
            })
        })

        compare(scene.indicatorItem.vertical, true)
        compare(scene.indicatorItem.active, true)
        compare(scene.indicatorItem.urgent, true)
        compare(scene.indicatorItem.windowCount, 4)
        compare(scene.indicatorItem.mode, "dot")
        compare(scene.indicatorItem.resolvedThickness, 5)
        compare(scene.indicatorItem.resolvedLength, 14)
        compare(scene.indicatorItem.pulseEnabled, false)
        compare(scene.indicatorItem.indicatorColor.toString(), "#ff3344")
    }
}
