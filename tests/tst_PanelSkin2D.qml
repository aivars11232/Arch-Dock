import QtQuick 2.15
import QtTest 1.3
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "PanelSkin2D"
    when: windowShown
    width: 1600
    height: 420

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
}
