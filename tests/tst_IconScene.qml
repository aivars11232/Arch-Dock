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

    function styleWith(overrides) {
        const definition = {
            format: "org.archdock.icon-style",
            version: 1,
            id: "treatment-style",
            valid: true,
            loadable: true,
            glyphPolicy: { mode: "original", compatibleOnly: true },
            layers: { rear: [], base: [], front: [] },
            assetPaths: ({}),
            states: [{ id: "normal", glyphOpacity: 1, glyphScale: 1 }]
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            definition[key] = additions[key]
        return definition
    }

    function test_plainFallbackAppliesPerStateGlyphStyling() {
        // Without a style the glyph still has to read as disabled/minimized.
        const disabledScene = createScene({ disabled: true })
        compare(disabledScene.styleGlyphTreatmentActive, false)
        compare(disabledScene.visualState, "disabled")
        compare(disabledScene.glyphItem.opacity, 0.4)

        const minimizedScene = createScene({ minimized: true })
        compare(minimizedScene.visualState, "minimized")
        compare(minimizedScene.glyphItem.opacity, 0.68)

        const normalScene = createScene()
        compare(normalScene.glyphItem.opacity, 1)
    }

    function test_tintIsNotAppliedToIncompatibleGlyph() {
        // A colorful application icon must never be silently recolored.
        const scene = createScene({
            iconStyleDefinition: styleWith({
                glyphPolicy: {
                    mode: "tinted", tint: "#ff0000", compatibleOnly: true
                }
            })
        })

        compare(scene.resolvedIconStyle.glyphPolicy, "tinted")
        compare(scene.glyphTreatment, "original")
        compare(scene.resolvedIconStyle.glyphTreatmentReason,
                "glyph-not-compatible")
        compare(scene.glyphIsMask, false)
        compare(scene.glyphItem.layer.enabled, false)
        compare(scene.resolvedIconSource, "application-x-executable")
    }

    function test_tintAppliesOnlyToACompatibleGlyph() {
        const scene = createScene({
            entry: {
                appId: "org.example.app",
                iconName: "example-symbolic"
            },
            iconStyleDefinition: styleWith({
                glyphPolicy: {
                    mode: "tinted", tint: "#ff0000", compatibleOnly: true
                }
            })
        })

        compare(scene.resolvedIconStyle.glyphCompatible, true)
        compare(scene.glyphTreatment, "tinted")
        compare(scene.glyphTint, "#ff0000")
        compare(scene.glyphItem.layer.enabled, true)
    }

    function test_monochromeUsesTheNativeMaskPath() {
        const scene = createScene({
            entry: {
                appId: "org.example.app",
                iconName: "application-x-executable",
                glyphCompatible: true
            },
            iconStyleDefinition: styleWith({
                glyphPolicy: {
                    mode: "monochrome", tint: "#00ff00", compatibleOnly: true
                }
            })
        })

        compare(scene.glyphTreatment, "monochrome")
        compare(scene.glyphIsMask, true)
        compare(scene.glyphItem.isMask, true)
        compare(scene.glyphItem.color.toString(), "#00ff00")
        // Monochrome uses Kirigami's own mask path, not a MultiEffect.
        compare(scene.glyphItem.layer.enabled, false)
    }

    function test_mappedReplacementRequiresAnApplicationMapping() {
        const scene = createScene({
            iconStyleDefinition: styleWith({
                glyphPolicy: { mode: "mapped-replacement" },
                mappedReplacements: ({ "org.other.app": "other.svg" }),
                assetPaths: ({ "other.svg": "file:///tmp/other.svg" })
            })
        })

        compare(scene.glyphTreatment, "original")
        compare(scene.resolvedIconStyle.replacementApplied, false)
        compare(scene.resolvedIconStyle.glyphTreatmentReason,
                "no-application-mapping")
        compare(scene.resolvedIconSource, "application-x-executable")
    }

    function test_failedStyleAssetCollapsesToTheSafeOriginalGlyph() {
        const scene = createScene({
            iconStyleDefinition: styleWith({
                glyphPolicy: {
                    mode: "monochrome", tint: "#00ff00", compatibleOnly: false
                },
                layers: {
                    rear: [],
                    base: [{
                        id: "broken-base",
                        kind: "asset",
                        asset: "missing.png",
                        opacity: 1
                    }],
                    front: []
                },
                assetPaths: ({
                    "missing.png":
                        "file:///nonexistent/archdock-missing-asset.png"
                })
            })
        })

        tryVerify(function() { return scene.styleAssetsFailed }, 3000)
        // The whole treatment collapses, not just the failed layer.
        compare(scene.styledLayersActive, false)
        compare(scene.styleGlyphTreatmentActive, false)
        compare(scene.glyphTreatment, "original")
        compare(scene.glyphIsMask, false)
        compare(scene.glyphItem.isMask, false)
        compare(scene.resolvedIconSource, "application-x-executable")
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
