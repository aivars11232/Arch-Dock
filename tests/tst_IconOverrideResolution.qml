import QtQuick
import QtTest
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "IconOverrideResolution"
    when: windowShown
    width: 520
    height: 320

    Component {
        id: iconSceneComponent

        IconScene {
            logicalSize: 64
            reducedMotion: true
        }
    }

    Component {
        id: previewComponent

        LivePanelPreview {
            width: 420
            height: 180
            previewMode: "horizontal"
            panelDefinition: ({
                edge: "bottom",
                layout: "horizontal",
                iconSize: 52,
                spacing: 8
            })
            hostCapabilities: ({
                rendererTiers: ["procedural2d"],
                supportedLayouts: ["horizontal"],
                supportsCustomGeometry: true,
                supportsInputMask: true
            })
            runtimeState: ({})
            animationProfiles: ({ reducedMotion: true })
        }
    }

    function style(id, color) {
        return {
            format: "org.archdock.icon-style",
            version: 1,
            id: id,
            valid: true,
            loadable: true,
            glyphPolicy: { mode: "original", compatibleOnly: true },
            safeGlyphInset: { left: 0.12, top: 0.12,
                              right: 0.12, bottom: 0.12 },
            layers: {
                rear: [],
                base: [{
                    id: id + "-base",
                    kind: "procedural",
                    shape: "rounded",
                    color: color,
                    borderColor: color,
                    borderWidth: 1,
                    inset: 0.04,
                    opacity: 1,
                    radius: 0.2
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
                borderColor: color,
                glowColor: color,
                glowOpacity: 0.4,
                reflectionOpacity: 0,
                indicatorColor: color,
                indicatorOpacity: 0
            }]
        }
    }

    function baseEntry(id, glyph) {
        return {
            appId: id,
            stableIdentity: "application." + id,
            baseIconName: glyph,
            iconName: glyph,
            resolvedGlyph: glyph,
            baseDisplayName: id,
            displayName: id,
            windowCount: 1
        }
    }

    function test_oneEntryOverrideDoesNotAffectAnother() {
        const panelStyle = style("metallic-blue", "#3aa9ff")
        const overrideStyle = style("dark-orb", "#9d70ff")
        overrideStyle.states[0].glyphOpacity = 0.64
        overrideStyle.states[0].glyphScale = 0.84
        const firstEntry = baseEntry("first", "utilities-terminal")
        firstEntry.resolvedIconStyleDefinition = overrideStyle
        firstEntry.tileEnabled = false
        firstEntry.iconOverrideApplied = true
        const secondEntry = baseEntry("second", "system-file-manager")

        const first = createTemporaryObject(iconSceneComponent, testCase, {
            entry: firstEntry,
            iconStyleDefinition: panelStyle
        })
        const second = createTemporaryObject(iconSceneComponent, testCase, {
            entry: secondEntry,
            iconStyleDefinition: panelStyle
        })
        verify(first !== null)
        verify(second !== null)
        wait(0)

        compare(first.resolvedIconStyle.styleId, "dark-orb")
        compare(first.effectiveIconStyleDefinition.id, "dark-orb")
        compare(first.resolvedIconSource, "utilities-terminal")
        compare(first.tileRenderingEnabled, false)
        compare(first.styledLayersActive, false)
        fuzzyCompare(first.glyphItem.width, 64 * 0.76, 0.0001)
        fuzzyCompare(first.glyphItem.height, 64 * 0.76, 0.0001)
        fuzzyCompare(first.glyphItem.opacity, 0.64, 0.0001)
        fuzzyCompare(first.glyphItem.scale, 0.84, 0.0001)
        compare(second.resolvedIconStyle.styleId, "metallic-blue")
        compare(second.effectiveIconStyleDefinition.id, "metallic-blue")
        compare(second.resolvedIconSource, "system-file-manager")
        compare(second.tileRenderingEnabled, true)
        compare(second.styledLayersActive, true)
    }

    function test_invalidEntryStyleFallsBackToPanelStyleAndRealGlyph() {
        const entry = baseEntry("fallback", "applications-system")
        entry.resolvedIconStyleDefinition = {
            format: "org.archdock.icon-style",
            version: 1,
            id: "missing-style",
            valid: false,
            loadable: false,
            states: []
        }
        entry.resolvedGlyph = "applications-system"
        const scene = createTemporaryObject(iconSceneComponent, testCase, {
            entry: entry,
            iconStyleDefinition: style("neon-green", "#44ff99")
        })
        verify(scene !== null)
        wait(0)

        compare(scene.resolvedIconStyle.styleId, "neon-green")
        compare(scene.resolvedIconStyle.fallbackApplied, true)
        compare(scene.resolvedIconStyle.fallbackReason,
                "entry-style-unavailable")
        compare(scene.resolvedIconSource, "applications-system")
    }

    function test_resetEntryReturnsToPanelDefault() {
        const panelStyle = style("neon-orange", "#ff8a38")
        const entry = baseEntry("reset", "applications-graphics")
        const scene = createTemporaryObject(iconSceneComponent, testCase, {
            entry: entry,
            iconStyleDefinition: panelStyle
        })
        verify(scene !== null)
        wait(0)

        compare(scene.resolvedIconStyle.styleId, "neon-orange")
        compare(scene.resolvedIconSource, "applications-graphics")
        compare(scene.tileRenderingEnabled, true)
    }

    function test_livePreviewUsesResolvedEntryOverride() {
        const panelStyle = style("metallic-red", "#ff5068")
        const entry = baseEntry("preview", "applications-internet")
        entry.resolvedIconStyleDefinition = style("dark-orb", "#9d70ff")
        entry.tileEnabled = true
        const preview = createTemporaryObject(previewComponent, testCase, {
            iconStyleDefinition: panelStyle,
            orderedEntries: [entry]
        })
        verify(preview !== null)
        tryCompare(preview.panelSceneItem, "entryCount", 1)
        const entryItem = preview.panelSceneItem.entryItemAt(0)
        verify(entryItem !== null)
        tryVerify(function() { return entryItem.delegateItem !== null })
        compare(entryItem.delegateItem.resolvedIconStyle.styleId, "dark-orb")
        compare(entryItem.delegateItem.resolvedIconSource,
                "applications-internet")
    }
}
