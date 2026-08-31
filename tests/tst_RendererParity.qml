import QtQuick
import QtTest
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "RendererParity"
    width: 1000
    height: 620
    when: windowShown

    readonly property var entries: [
        { id: "one", displayName: "One", running: false },
        { id: "two", displayName: "Two", running: true, active: true },
        { id: "three", displayName: "Three", running: true }
    ]
    readonly property var capabilities: ({
        available: true,
        renderer: {
            requestedTier: "procedural2d",
            effectiveTier: "procedural2d",
            fallbackApplied: false,
            reasonCode: ""
        }
    })
    readonly property var skinnedCapabilities: ({
        available: true,
        renderer: {
            requestedTier: "skinned2d",
            effectiveTier: "skinned2d",
            fallbackApplied: false,
            reasonCode: ""
        }
    })

    Component {
        id: previewComponent

        LivePanelPreview {
            width: 480
            height: 280
            orderedEntries: testCase.entries
            hostCapabilities: testCase.capabilities
            animationProfiles: ({ reducedMotion: true })
        }
    }

    Component {
        id: liveSceneComponent

        PanelScene {
            orderedEntries: testCase.entries
            hostCapabilities: testCase.capabilities
            animationProfiles: ({ reducedMotion: true })
            geometryCompatibilityProfile: "canonical"
        }
    }

    function caseDefinition(mode) {
        const common = {
            schemaVersion: 2,
            layoutPadding: 12,
            iconSize: 40,
            spacing: 6,
            appearance: "minimal",
            opacity: 0.87,
            rendererTier: "procedural2d"
        }
        if (mode === "vertical") {
            common.layout = "vertical"
            common.edge = "left"
        } else if (mode === "free") {
            common.layout = "ring"
            common.edge = "free"
            common.layoutRadius = 72
        } else {
            common.layout = "horizontal"
            common.edge = "bottom"
        }
        return common
    }

    function energyCaseDefinition() {
        return {
            schemaVersion: 2,
            layout: "horizontal",
            edge: "bottom",
            layoutPadding: 12,
            iconSize: 40,
            spacing: 6,
            appearance: "minimal",
            opacity: 1,
            rendererTier: "skinned2d",
            panelThemeId: "energy-frame-cyan",
            color: "#44ddea",
            glowIntensity: 1.15
        }
    }

    function energyThemeDefinition() {
        const packageRoot = Qt.resolvedUrl(
            "../assets/themes/energy-frame-cyan/")
        const sourceRect = { x: 0, y: 0, width: 1200, height: 160 }
        const contentRect = { x: 168, y: 38, width: 864, height: 84 }
        const layers = [
            { id: "base-start", asset: "surface", role: "split-start", sourceRect: { x: 0, y: 0, width: 152, height: 160 }, opacity: 1, blendMode: "source-over" },
            { id: "base-center", asset: "surface", role: "split-center", sourceRect: { x: 152, y: 0, width: 896, height: 160 }, opacity: 1, blendMode: "source-over" },
            { id: "base-end", asset: "surface", role: "split-end", sourceRect: { x: 1048, y: 0, width: 152, height: 160 }, opacity: 1, blendMode: "source-over" },
            { id: "frame", asset: "frame-mask", role: "overlay", sourceRect: sourceRect, opacity: 0.84, blendMode: "source-over" },
            { id: "glow-normal", asset: "glow-mask", role: "glow", sourceRect: sourceRect, opacity: 0.55, blendMode: "source-over" },
            { id: "glow-hover", asset: "glow-mask", role: "glow", sourceRect: sourceRect, opacity: 0.92, blendMode: "source-over" },
            { id: "glow-open", asset: "glow-mask", role: "glow", sourceRect: sourceRect, opacity: 0.78, blendMode: "source-over" },
            { id: "glow-collapsed", asset: "glow-mask", role: "glow", sourceRect: sourceRect, opacity: 0.32, blendMode: "source-over" },
            { id: "energy-normal", asset: "energy-overlay-mask", role: "overlay", sourceRect: sourceRect, opacity: 0.42, blendMode: "source-over" },
            { id: "energy-hover", asset: "energy-overlay-mask", role: "overlay", sourceRect: sourceRect, opacity: 0.76, blendMode: "source-over" },
            { id: "energy-open", asset: "energy-overlay-mask", role: "overlay", sourceRect: sourceRect, opacity: 0.66, blendMode: "source-over" },
            { id: "energy-collapsed", asset: "energy-overlay-mask", role: "overlay", sourceRect: sourceRect, opacity: 0.22, blendMode: "source-over" },
            { id: "highlight", asset: "highlight-mask", role: "overlay", sourceRect: sourceRect, opacity: 0.58, blendMode: "source-over" }
        ]
        const stateLayers = function(glow, energy) {
            return ["base-start", "base-center", "base-end", "frame",
                    glow, energy, "highlight"]
        }
        const stateIds = ["normal", "hover", "open", "collapsed"]
        const slices = []
        const contentRegions = []
        const inputMasks = []
        for (let index = 0; index < stateIds.length; ++index) {
            const state = stateIds[index]
            slices.push({
                id: state + "-horizontal",
                asset: "surface",
                state: state,
                orientation: "horizontal",
                sourceRect: sourceRect,
                fixedStart: 152,
                fixedEnd: 152,
                centerMode: "stretch"
            })
            contentRegions.push({
                id: state + "-content",
                state: state,
                orientation: "horizontal",
                shape: "rect",
                rect: contentRect,
                baseline: 70
            })
            inputMasks.push({
                id: state + "-input",
                asset: "input-mask",
                state: state,
                orientation: "horizontal",
                threshold: 0.5
            })
        }
        return {
            format: "org.archdock.theme",
            version: 2,
            id: "energy-frame-cyan",
            valid: true,
            loadable: true,
            status: "valid",
            capabilities: {
                features: ["dynamic-tint", "dynamic-glow"]
            },
            assets: [
                { id: "surface", kind: "vector", naturalSize: { width: 1200, height: 160 } },
                { id: "frame-mask", kind: "mask", naturalSize: { width: 1200, height: 160 } },
                { id: "glow-mask", kind: "mask", naturalSize: { width: 1200, height: 160 } },
                { id: "energy-overlay-mask", kind: "mask", naturalSize: { width: 1200, height: 160 } },
                { id: "highlight-mask", kind: "mask", naturalSize: { width: 1200, height: 160 } },
                { id: "input-mask", kind: "mask", naturalSize: { width: 1200, height: 160 } }
            ],
            layers: layers,
            states: [
                { id: "normal", layers: stateLayers("glow-normal", "energy-normal") },
                { id: "hover", layers: stateLayers("glow-hover", "energy-hover") },
                { id: "open", layers: stateLayers("glow-open", "energy-open") },
                { id: "collapsed", layers: stateLayers("glow-collapsed", "energy-collapsed") }
            ],
            slices: slices,
            contentRegions: contentRegions,
            inputMasks: inputMasks,
            effectMargins: { left: 18, top: 16, right: 18, bottom: 16 },
            assetPaths: {
                surface: packageRoot + "assets/surface.svg",
                "frame-mask": packageRoot + "assets/frame-mask.svg",
                "glow-mask": packageRoot + "assets/glow-mask.svg",
                "energy-overlay-mask": packageRoot + "assets/energy-overlay-mask.svg",
                "highlight-mask": packageRoot + "assets/highlight-mask.svg",
                "input-mask": packageRoot + "masks/input.svg"
            }
        }
    }

    function contractSnapshot(scene) {
        const entryGeometry = []
        for (let index = 0; index < scene.entryCount; ++index) {
            const geometry = scene.entryGeometryAt(index)
            entryGeometry.push({
                x: geometry.position.x,
                y: geometry.position.y,
                rotation: geometry.rotation,
                scaleFactor: geometry.scaleFactor,
                depthOrder: geometry.depthOrder,
                normalX: geometry.outwardNormal.x,
                normalY: geometry.outwardNormal.y
            })
        }
        return {
            layoutPath: scene.layoutPath,
            verticalLayout: scene.verticalLayout,
            layoutGeometry: scene.layoutGeometry,
            contentBounds: scene.contentBounds,
            effectBounds: scene.effectBounds,
            inputRegion: scene.inputRegion,
            revealHandle: scene.revealHandle,
            popupAnchors: scene.popupAnchors,
            previewAnchors: scene.previewAnchors,
            effectiveRendererTier: scene.effectiveRendererTier,
            fallbackApplied: scene.fallbackApplied,
            fallbackReason: scene.fallbackReason,
            entries: entryGeometry
        }
    }

    function test_sharedPanelSceneContract_data() {
        return [
            { tag: "horizontal-native", mode: "horizontal" },
            { tag: "vertical-native", mode: "vertical" },
            { tag: "free-ring", mode: "free" }
        ]
    }

    function test_sharedPanelSceneContract(data) {
        const definition = caseDefinition(data.mode)
        const preview = createTemporaryObject(previewComponent, testCase, {
            panelDefinition: definition,
            previewMode: data.mode,
            hoveredEntry: 1
        })
        verify(preview !== null)
        const liveScene = createTemporaryObject(liveSceneComponent, testCase, {
            panelDefinition: definition,
            runtimeState: { hoveredEntry: 1 }
        })
        verify(liveScene !== null)
        wait(30)

        compare(JSON.stringify(contractSnapshot(preview.panelSceneItem)),
                JSON.stringify(contractSnapshot(liveScene)))

        const previewSurface = grabImage(
            preview.panelSceneItem.visualPanel)
        const liveSurface = grabImage(liveScene.visualPanel)
        compare(previewSurface.width, liveSurface.width)
        compare(previewSurface.height, liveSurface.height)
        verify(previewSurface.alpha(
            Math.round(previewSurface.width / 2),
            Math.round(previewSurface.height / 2)) > 0)
        verify(previewSurface.equals(liveSurface),
               data.mode + " shared procedural surface diverged")
    }

    function test_capabilityFallbackParity() {
        const definition = caseDefinition("horizontal")
        definition.rendererTier = "skinned2d"
        const fallbackCapabilities = {
            available: true,
            renderer: {
                requestedTier: "skinned2d",
                effectiveTier: "procedural2d",
                fallbackApplied: true,
                reasonCode: "renderer-host-unsupported"
            }
        }
        const preview = createTemporaryObject(previewComponent, testCase, {
            panelDefinition: definition,
            previewMode: "horizontal",
            hostCapabilities: fallbackCapabilities
        })
        verify(preview !== null)
        const liveScene = createTemporaryObject(liveSceneComponent, testCase, {
            panelDefinition: definition,
            hostCapabilities: fallbackCapabilities
        })
        verify(liveScene !== null)
        wait(0)

        compare(preview.activeRendererTier,
                liveScene.effectiveRendererTier)
        compare(preview.fallbackApplied, liveScene.fallbackApplied)
        compare(preview.fallbackReason, liveScene.fallbackReason)
        compare(preview.fallbackReason, "renderer-host-unsupported")
    }

    function test_energyPanelSceneAndPreviewParity_data() {
        return [
            { tag: "normal", state: "normal", hovered: false },
            { tag: "hover", state: "open", hovered: true },
            { tag: "open", state: "open", hovered: false },
            { tag: "collapsed", state: "collapsed", hovered: false }
        ]
    }

    function test_energyPanelSceneAndPreviewParity(data) {
        const definition = energyCaseDefinition()
        const theme = energyThemeDefinition()
        const hoveredEntry = data.hovered ? 1 : -1
        const preview = createTemporaryObject(previewComponent, testCase, {
            panelDefinition: definition,
            previewMode: "horizontal",
            presentationState: data.state,
            hoveredEntry: hoveredEntry,
            hostCapabilities: skinnedCapabilities,
            themeDefinition: theme
        })
        verify(preview !== null)
        const liveScene = createTemporaryObject(liveSceneComponent, testCase, {
            panelDefinition: definition,
            runtimeState: {
                presentationState: data.state,
                hovered: data.hovered,
                hoveredEntry: hoveredEntry
            },
            hostCapabilities: skinnedCapabilities,
            themeDefinition: theme
        })
        verify(liveScene !== null)

        tryCompare(preview.panelSceneItem, "effectiveRendererTier",
                   "skinned2d", 3000)
        tryCompare(liveScene, "effectiveRendererTier", "skinned2d", 3000)
        compare(preview.fallbackApplied, false)
        compare(liveScene.fallbackApplied, false)
        compare(JSON.stringify(contractSnapshot(preview.panelSceneItem)),
                JSON.stringify(contractSnapshot(liveScene)))

        const previewSkin = preview.panelSceneItem.visualPanel
        const liveSkin = liveScene.visualPanel
        verify(previewSkin !== null)
        verify(liveSkin !== null)
        tryVerify(function() {
            return previewSkin.rendererReady && liveSkin.rendererReady
        }, 3000)

        const expectedState = data.hovered ? "hover" : data.state
        compare(previewSkin.effectiveState, expectedState)
        compare(previewSkin.effectiveState, liveSkin.effectiveState)
        compare(previewSkin.renderedLayerIds.join(","),
                liveSkin.renderedLayerIds.join(","))
        compare(previewSkin.effectiveGlowIntensity,
                liveSkin.effectiveGlowIntensity)
        compare(previewSkin.overlayAnimationRunning, false)
        compare(liveSkin.overlayAnimationRunning, false)
        compare(previewSkin.effectiveOverlayPhase, 0)
        compare(liveSkin.effectiveOverlayPhase, 0)
        verify(previewSkin.contains(Qt.point(
            previewSkin.width / 2, previewSkin.height / 2)))
        verify(liveSkin.contains(Qt.point(
            liveSkin.width / 2, liveSkin.height / 2)))
        verify(!previewSkin.contains(Qt.point(0, 0)))
        verify(!liveSkin.contains(Qt.point(0, 0)))

        const previewSurface = grabImage(previewSkin)
        const liveSurface = grabImage(liveSkin)
        compare(previewSurface.width, liveSurface.width)
        compare(previewSurface.height, liveSurface.height)
        verify(previewSurface.alpha(
            Math.round(previewSurface.width / 2),
            Math.round(previewSurface.height / 2)) > 0)
        verify(previewSurface.equals(liveSurface),
               data.tag + " energy surface diverged")
    }
}
