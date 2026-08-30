import QtQuick 2.15
import QtTest 1.3
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "PanelSkin2D"
    when: windowShown
    width: 2600
    height: 520
    visible: true

    Component {
        id: skinComponent

        PanelSkin2D {}
    }

    function themeDefinition(themeId, glowBlendMode) {
        const packageRoot = Qt.resolvedUrl(
            "../assets/themes/" + themeId + "/")
        const sourceRect = { x: 0, y: 0, width: 1200, height: 160 }
        const contentRect = { x: 168, y: 36, width: 864, height: 88 }
        const blendMode = glowBlendMode || "source-over"
        return {
            format: "org.archdock.theme",
            version: 2,
            id: themeId,
            valid: true,
            loadable: true,
            status: "valid",
            assets: [
                {
                    id: "surface",
                    naturalSize: { width: 1200, height: 160 }
                },
                {
                    id: "glow",
                    naturalSize: { width: 1200, height: 160 }
                },
                {
                    id: "input-mask",
                    naturalSize: { width: 1200, height: 160 }
                }
            ],
            layers: [
                {
                    id: "glow-normal",
                    asset: "glow",
                    role: "glow",
                    sourceRect: sourceRect,
                    opacity: 0.72,
                    blendMode: blendMode
                },
                {
                    id: "glow-open",
                    asset: "glow",
                    role: "glow",
                    sourceRect: sourceRect,
                    opacity: 1.0,
                    blendMode: blendMode
                },
                {
                    id: "glow-collapsed",
                    asset: "glow",
                    role: "glow",
                    sourceRect: sourceRect,
                    opacity: 0.38,
                    blendMode: blendMode
                }
            ],
            states: [
                { id: "normal", layers: ["glow-normal"] },
                { id: "open", layers: ["glow-open"] },
                { id: "collapsed", layers: ["glow-collapsed"] }
            ],
            slices: [
                {
                    id: "normal-horizontal",
                    asset: "surface",
                    state: "normal",
                    orientation: "horizontal",
                    sourceRect: sourceRect,
                    fixedStart: 152,
                    fixedEnd: 152,
                    centerMode: "stretch"
                },
                {
                    id: "open-horizontal",
                    asset: "surface",
                    state: "open",
                    orientation: "horizontal",
                    sourceRect: sourceRect,
                    fixedStart: 152,
                    fixedEnd: 152,
                    centerMode: "stretch"
                },
                {
                    id: "collapsed-horizontal",
                    asset: "surface",
                    state: "collapsed",
                    orientation: "horizontal",
                    sourceRect: sourceRect,
                    fixedStart: 152,
                    fixedEnd: 152,
                    centerMode: "stretch"
                }
            ],
            contentRegions: [
                {
                    id: "normal-content",
                    state: "normal",
                    orientation: "horizontal",
                    shape: "rect",
                    rect: contentRect,
                    baseline: 72
                },
                {
                    id: "open-content",
                    state: "open",
                    orientation: "horizontal",
                    shape: "rect",
                    rect: contentRect,
                    baseline: 72
                },
                {
                    id: "collapsed-content",
                    state: "collapsed",
                    orientation: "horizontal",
                    shape: "rect",
                    rect: contentRect,
                    baseline: 72
                }
            ],
            inputMasks: [
                {
                    id: "normal-input",
                    asset: "input-mask",
                    state: "normal",
                    orientation: "horizontal",
                    threshold: 0.5
                },
                {
                    id: "open-input",
                    asset: "input-mask",
                    state: "open",
                    orientation: "horizontal",
                    threshold: 0.5
                },
                {
                    id: "collapsed-input",
                    asset: "input-mask",
                    state: "collapsed",
                    orientation: "horizontal",
                    threshold: 0.5
                }
            ],
            effectMargins: { left: 14, top: 12, right: 14, bottom: 14 },
            assetPaths: {
                surface: packageRoot + "assets/surface.svg",
                glow: packageRoot + "assets/glow.svg",
                "input-mask": packageRoot + "masks/input.svg"
            }
        }
    }

    function createSkin(themeId, properties) {
        const values = {
            width: 600,
            height: 80,
            themeDefinition: themeDefinition(themeId),
            orientation: "horizontal",
            presentationState: "open",
            panelOpacity: 1
        }
        const additions = properties || ({})
        for (const key of Object.keys(additions))
            values[key] = additions[key]
        const skin = createTemporaryObject(skinComponent, testCase, values)
        verify(skin !== null)
        return skin
    }

    function energyThemeDefinition(themeId, missingOptionalLayer) {
        const packageRoot = Qt.resolvedUrl(
            "../assets/themes/" + themeId + "/")
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
                id: state + "-horizontal", asset: "surface", state: state,
                orientation: "horizontal", sourceRect: sourceRect,
                fixedStart: 152, fixedEnd: 152, centerMode: "stretch"
            })
            contentRegions.push({
                id: state + "-content", state: state,
                orientation: "horizontal", shape: "rect",
                rect: contentRect, baseline: 70
            })
            inputMasks.push({
                id: state + "-input", asset: "input-mask", state: state,
                orientation: "horizontal", threshold: 0.5
            })
        }
        return {
            format: "org.archdock.theme",
            version: 2,
            id: themeId,
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
                "energy-overlay-mask": missingOptionalLayer ? ""
                    : packageRoot + "assets/energy-overlay-mask.svg",
                "highlight-mask": packageRoot + "assets/highlight-mask.svg",
                "input-mask": packageRoot + "masks/input.svg"
            }
        }
    }

    function waitForRenderer(skin) {
        tryVerify(function() { return skin.rendererReady }, 3000)
        compare(skin.errorReason, "")
    }

    function test_originalPackagesRenderWithStableCaps_data() {
        return [
            { tag: "dark", themeId: "sci-fi-chassis-dark" },
            { tag: "red", themeId: "sci-fi-chassis-red" },
            { tag: "blue", themeId: "sci-fi-chassis-blue" }
        ]
    }

    function test_originalPackagesRenderWithStableCaps(data) {
        const skin = createSkin(data.themeId)
        waitForRenderer(skin)

        compare(skin.effectiveState, "open")
        compare(skin.fixedStartPixels, 76)
        compare(skin.fixedEndPixels, 76)
        compare(skin.centerPixels, 448)
        compare(skin.contentRegionDefinition.rect.x, 168)
        compare(skin.contentRegionDefinition.rect.height, 88)
        compare(skin.glowLayerDefinition.opacity, 1)

        const image = grabImage(skin)
        compare(image.width, 600)
        compare(image.height, 80)
        verify(image.alpha(300, 40) > 0)

        verify(skin.inputMaskItem.contains(Qt.point(300, 40)))
        verify(skin.contains(Qt.point(300, 40)))
        verify(!skin.inputMaskItem.contains(Qt.point(0, 0)))
        verify(!skin.inputMaskItem.contains(Qt.point(-1, 40)))
        verify(!skin.inputMaskItem.contains(Qt.point(600, 40)))
    }

    function test_stateAndResizeKeepSliceAndMaskAligned() {
        const skin = createSkin("sci-fi-chassis-dark", {
            width: 900,
            height: 80,
            presentationState: "collapsed"
        })
        waitForRenderer(skin)
        compare(skin.effectiveState, "collapsed")
        compare(skin.fixedStartPixels, 76)
        compare(skin.fixedEndPixels, 76)
        compare(skin.centerPixels, 748)
        compare(skin.glowLayerDefinition.opacity, 0.38)
        verify(skin.inputMaskItem.contains(Qt.point(450, 40)))
        verify(!skin.inputMaskItem.contains(Qt.point(0, 0)))

        skin.presentationState = "normal"
        tryVerify(function() {
            return skin.rendererReady && skin.effectiveState === "normal"
        }, 3000)
        compare(skin.glowLayerDefinition.opacity, 0.72)
    }

    function test_familyVisualScenesAtMultipleScales_data() {
        return [
            {
                tag: "dark-short-open",
                themeId: "sci-fi-chassis-dark",
                targetWidth: 352,
                targetHeight: 64,
                state: "open",
                glowOpacity: 1
            },
            {
                tag: "dark-long-collapsed",
                themeId: "sci-fi-chassis-dark",
                targetWidth: 1500,
                targetHeight: 200,
                state: "collapsed",
                glowOpacity: 0.38
            },
            {
                tag: "red-short-collapsed",
                themeId: "sci-fi-chassis-red",
                targetWidth: 352,
                targetHeight: 64,
                state: "collapsed",
                glowOpacity: 0.38
            },
            {
                tag: "red-long-open",
                themeId: "sci-fi-chassis-red",
                targetWidth: 1500,
                targetHeight: 200,
                state: "open",
                glowOpacity: 1
            },
            {
                tag: "blue-short-open",
                themeId: "sci-fi-chassis-blue",
                targetWidth: 352,
                targetHeight: 64,
                state: "open",
                glowOpacity: 1
            },
            {
                tag: "blue-long-collapsed",
                themeId: "sci-fi-chassis-blue",
                targetWidth: 1500,
                targetHeight: 200,
                state: "collapsed",
                glowOpacity: 0.38
            }
        ]
    }

    function test_familyVisualScenesAtMultipleScales(data) {
        const skin = createSkin(data.themeId, {
            width: data.targetWidth,
            height: data.targetHeight,
            presentationState: data.state
        })
        waitForRenderer(skin)

        const expectedCap = 152 * data.targetHeight / 160
        verify(Math.abs(skin.fixedStartPixels - expectedCap) < 0.001)
        verify(Math.abs(skin.fixedEndPixels - expectedCap) < 0.001)
        verify(Math.abs(skin.centerPixels
                        - (data.targetWidth - expectedCap * 2)) < 0.001)
        compare(skin.effectiveState, data.state)
        compare(skin.glowLayerDefinition.opacity, data.glowOpacity)

        const image = grabImage(skin)
        compare(image.width, data.targetWidth)
        compare(image.height, data.targetHeight)
        verify(image.alpha(Math.round(data.targetWidth / 2),
                           Math.round(data.targetHeight / 2)) > 0)
        verify(skin.inputMaskItem.contains(Qt.point(
            data.targetWidth / 2, data.targetHeight / 2)))
        verify(!skin.inputMaskItem.contains(Qt.point(0, 0)))
    }

    function test_invalidOrientationAndUnsupportedBlendFailClosed() {
        const vertical = createSkin("sci-fi-chassis-dark", {
            orientation: "vertical"
        })
        tryCompare(vertical, "errorReason", "theme-orientation-unavailable")
        verify(!vertical.rendererReady)

        const unsupported = createSkin("sci-fi-chassis-dark", {
            themeDefinition: themeDefinition(
                "sci-fi-chassis-dark", "screen")
        })
        tryCompare(unsupported, "errorReason", "theme-blend-mode-unsupported")
        verify(!unsupported.rendererReady)
    }

    function test_energyLayersUseManifestOrderAndHoverInput() {
        const skin = createSkin("energy-frame-cyan", {
            themeDefinition: energyThemeDefinition("energy-frame-cyan", false),
            presentationState: "open",
            hovered: false,
            tintColor: "#44ddea",
            glowIntensity: 1.25,
            reducedMotion: true
        })
        waitForRenderer(skin)
        compare(skin.renderedLayerIds.join(","),
                "base-start,base-center,base-end,frame,glow-open,energy-open,highlight")
        compare(skin.dynamicTintSupported, true)
        compare(skin.dynamicGlowSupported, true)
        compare(skin.effectiveGlowIntensity, 1.25)
        compare(skin.inputMaskDefinition.state, "open")
        const frameLayer = skin.layerItemById("frame")
        const glowLayer = skin.layerItemById("glow-open")
        const energyLayer = skin.layerItemById("energy-open")
        verify(frameLayer !== null)
        verify(glowLayer !== null)
        verify(energyLayer !== null)
        verify(frameLayer.visible, "frame layer hidden")
        verify(frameLayer.tintActive, "frame tint inactive")
        verify(frameLayer.layerOpacity > 0, "frame opacity zero")
        verify(glowLayer.visible, "glow layer hidden")
        verify(glowLayer.tintActive, "glow tint inactive")
        verify(glowLayer.layerOpacity > 0, "glow opacity zero")
        const openGlowOpacity = glowLayer.layerOpacity
        const openEnergyOpacity = energyLayer.layerOpacity

        skin.hovered = true
        tryCompare(skin, "effectiveState", "hover", 3000)
        waitForRenderer(skin)
        compare(skin.renderedLayerIds.join(","),
                "base-start,base-center,base-end,frame,glow-hover,energy-hover,highlight")
        compare(skin.inputMaskDefinition.state, "hover")
        verify(skin.effectiveGlowIntensity > 1.25)
        const hoverGlow = skin.layerItemById("glow-hover")
        const hoverEnergy = skin.layerItemById("energy-hover")
        verify(hoverGlow !== null && hoverGlow.visible)
        verify(hoverEnergy !== null && hoverEnergy.visible)
        verify(hoverGlow.layerOpacity > openGlowOpacity)
        verify(hoverEnergy.layerOpacity > openEnergyOpacity)
    }

    function test_energyFamilyAtProductionScales_data() {
        const variants = [
            { id: "energy-frame-cyan", tint: "#44ddea" },
            { id: "energy-frame-green", tint: "#4ee68a" },
            { id: "energy-frame-orange", tint: "#ff873c" },
            { id: "energy-frame-purple", tint: "#b96cff" }
        ]
        const scales = [1, 1.5, 2]
        const rows = []
        for (let variantIndex = 0; variantIndex < variants.length;
             ++variantIndex) {
            const variant = variants[variantIndex]
            for (let scaleIndex = 0; scaleIndex < scales.length;
                 ++scaleIndex) {
                const scale = scales[scaleIndex]
                rows.push({
                    tag: variant.id + "-" + Math.round(scale * 100),
                    themeId: variant.id,
                    tint: variant.tint,
                    scale: scale,
                    targetWidth: Math.round(1200 * scale),
                    targetHeight: Math.round(160 * scale)
                })
            }
        }
        return rows
    }

    function test_energyFamilyAtProductionScales(data) {
        const skin = createSkin(data.themeId, {
            width: data.targetWidth,
            height: data.targetHeight,
            themeDefinition: energyThemeDefinition(data.themeId, false),
            presentationState: "open",
            tintColor: data.tint,
            glowIntensity: 1.15,
            reducedMotion: true
        })
        waitForRenderer(skin)
        compare(skin.themeDefinition.id, data.themeId)
        compare(skin.effectiveState, "open")
        compare(skin.fixedStartPixels, Math.round(152 * data.scale))
        compare(skin.fixedEndPixels, Math.round(152 * data.scale))
        compare(skin.centerPixels,
                data.targetWidth - 2 * Math.round(152 * data.scale))
        compare(skin.renderedLayerIds.length, 7)
        compare(skin.dynamicTintSupported, true)
        compare(skin.dynamicGlowSupported, true)
        compare(skin.overlayAnimationRunning, false)
        compare(skin.effectiveOverlayPhase, 0)
        verify(skin.inputMaskItem.contains(
                   Qt.point(data.targetWidth / 2, data.targetHeight / 2)))
        verify(!skin.inputMaskItem.contains(Qt.point(0, 0)))

        const image = grabImage(skin)
        compare(image.width, data.targetWidth)
        compare(image.height, data.targetHeight)
        verify(image.alpha(Math.floor(data.targetWidth / 2),
                           Math.floor(data.targetHeight / 2)) > 0)
        wait(40)
        compare(skin.overlayAnimationRunning, false)
        compare(skin.effectiveOverlayPhase, 0)
    }

    function test_energyStateSnapshotsUseStaticReducedMotion_data() {
        return [
            {
                tag: "normal",
                state: "normal",
                hovered: false,
                glowLayer: "glow-normal",
                energyLayer: "energy-normal"
            },
            {
                tag: "hover",
                state: "open",
                hovered: true,
                glowLayer: "glow-hover",
                energyLayer: "energy-hover"
            },
            {
                tag: "open",
                state: "open",
                hovered: false,
                glowLayer: "glow-open",
                energyLayer: "energy-open"
            },
            {
                tag: "collapsed",
                state: "collapsed",
                hovered: false,
                glowLayer: "glow-collapsed",
                energyLayer: "energy-collapsed"
            },
            {
                tag: "reduced-motion",
                state: "open",
                hovered: false,
                glowLayer: "glow-open",
                energyLayer: "energy-open"
            }
        ]
    }

    function test_energyStateSnapshotsUseStaticReducedMotion(data) {
        const skin = createSkin("energy-frame-cyan", {
            themeDefinition: energyThemeDefinition("energy-frame-cyan", false),
            presentationState: data.state,
            hovered: data.hovered,
            tintColor: "#44ddea",
            glowIntensity: 1.15,
            reducedMotion: true
        })
        waitForRenderer(skin)
        const expectedState = data.hovered ? "hover" : data.state
        compare(skin.effectiveState, expectedState)
        verify(skin.renderedLayerIds.includes(data.glowLayer))
        verify(skin.renderedLayerIds.includes(data.energyLayer))
        verify(skin.layerItemById(data.glowLayer).layerOpacity > 0)
        verify(skin.layerItemById(data.energyLayer).layerOpacity > 0)
        compare(skin.overlayAnimationRunning, false)
        compare(skin.effectiveOverlayPhase, 0)

        const first = grabImage(skin)
        verify(first.alpha(300, 40) > 0)
        wait(40)
        const second = grabImage(skin)
        verify(first.equals(second), data.tag + " reduced-motion snapshot moved")
        compare(skin.overlayAnimationRunning, false)
        compare(skin.effectiveOverlayPhase, 0)
    }

    function test_explicitTransitionProgressCrossfadesStateLayers() {
        const skin = createSkin("energy-frame-cyan", {
            themeDefinition: energyThemeDefinition("energy-frame-cyan", false),
            presentationState: "collapsed",
            transitionState: "closing",
            presentationProgress: 0.5,
            reducedMotion: true
        })
        waitForRenderer(skin)
        compare(skin.interpolationActive, true)
        verify(Math.abs(skin.layerItemById("glow-open").layerOpacity
                        - 0.39) < 0.0001)
        verify(Math.abs(skin.layerItemById("glow-collapsed").layerOpacity
                        - 0.16) < 0.0001)
        compare(skin.layerItemById("base-center").layerOpacity, 1)

        skin.presentationProgress = 1
        wait(0)
        compare(skin.layerItemById("glow-open").layerOpacity, 0)
        compare(skin.layerItemById("glow-collapsed").layerOpacity, 0.32)
    }

    function test_reducedMotionAndVisibilityPauseEnergyOverlay() {
        const skin = createSkin("energy-frame-cyan", {
            themeDefinition: energyThemeDefinition("energy-frame-cyan", false),
            reducedMotion: true
        })
        waitForRenderer(skin)
        compare(skin.overlayAnimationRunning, false)
        compare(skin.effectiveOverlayPhase, 0)

        skin.reducedMotion = false
        tryCompare(skin, "overlayAnimationRunning", true)
        tryVerify(function() { return skin.effectiveOverlayPhase > 0 })

        skin.visible = false
        tryCompare(skin, "overlayAnimationRunning", false)
        compare(skin.effectiveOverlayPhase, 0)
        skin.visible = true
        skin.reducedMotion = true
        compare(skin.overlayAnimationRunning, false)
        compare(skin.effectiveOverlayPhase, 0)
    }

    function test_missingOptionalLayerAndUnsafeTintFailSafe() {
        const skin = createSkin("energy-frame-cyan", {
            themeDefinition: energyThemeDefinition("energy-frame-cyan", true),
            tintColor: "#00000000",
            reducedMotion: true
        })
        waitForRenderer(skin)
        verify(skin.skippedLayerCount >= 1)
        verify(skin.safeTintColor.a > 0.9)
        verify(skin.safeTintColor.r + skin.safeTintColor.g
               + skin.safeTintColor.b > 0.45)
        verify(skin.layerItemById("energy-open") !== null)
        compare(skin.layerItemById("energy-open").skipped, true)
        verify(grabImage(skin).alpha(300, 40) > 0)
    }
}
