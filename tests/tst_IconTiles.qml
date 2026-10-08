import QtQuick
import QtQuick.Window
import QtTest
import ArchDock.Rendering 1.0
import "../qml/runtime/SettingsEditorModel.js" as EditorModel

TestCase {
    id: testCase
    name: "IconTiles"
    when: windowShown
    visible: true
    width: 520
    height: 320
    Window { id: captureWindow; visible: true; width: 520; height: 320; color: "white" }

    Component { id: tileComponent; IconTile { width: 100; height: 100 } }
    Component { id: iconComponent; IconScene { logicalSize: 100; reducedMotion: true; iconSource: "folder" } }
    Component { id: panelComponent; PanelScene {} }
    Component {
        id: previewComponent
        LivePanelPreview {
            width: 420; height: 180
            previewMode: "horizontal"
            hostCapabilities: ({rendererTiers: ["procedural2d"], supportedLayouts: ["horizontal"]})
        }
    }

    function test_shapePixels_data() {
        return ["rounded", "square", "squircle", "circle", "hexagon"].map(function(shape) {
            return {tag: shape, shape: shape}
        })
    }

    function test_shapePixels(data) {
        const tile = createTemporaryObject(tileComponent, testCase,
            {shape: data.shape, fillColor: "#ff22cc", borderWidth: 0})
        verify(waitForRendering(tile))
        const image = grabImage(tile)
        compare(image.red(50, 50), 255)
        compare(image.green(50, 50), 34)
        compare(image.blue(50, 50), 204)
        // grabImage composites onto the opaque test window. Rounded and
        // polygon corners expose its white background; square corners fill.
        compare(image.green(2, 2), data.shape === "square" ? 34 : 255)
    }

    function changedPixels(first, second) {
        let count = 0
        for (let y = 0; y < first.height; ++y) for (let x = 0; x < first.width; ++x)
            if (Math.abs(first.red(x,y) - second.red(x,y))
                    + Math.abs(first.green(x,y) - second.green(x,y))
                    + Math.abs(first.blue(x,y) - second.blue(x,y)) > 12) ++count
        return count
    }

    function test_tileTexture_data() {
        return ["glass", "crystal", "neon", "minimal", "plasma", "lime", "floating-glass",
            "metallic", "futuristic", "organic", "platform", "plate", "pedestal"].map(function(texture) {
                return {tag: texture, texture: texture}
            })
    }

    function test_tileTexture(data) {
        // OF-41: a loaded choice alone is insufficient; static textured pixels
        // must differ from the otherwise identical custom fill, on both RHI
        // and the software backend, without filling the hexagon's corners.
        const tile = createTemporaryObject(tileComponent, captureWindow.contentItem,
            {shape: "hexagon", fillColor: "#7895b0", borderWidth: 0})
        verify(waitForRendering(tile))
        const plain = grabImage(tile)
        tile.texture = data.texture
        tryCompare(tile, "textureReady", true)
        tryVerify(function() { return changedPixels(plain, grabImage(tile)) >= 50 })
        const capture = grabImage(tile)
        compare(capture.red(2,2), 255)
        capture.save("tile-texture-" + data.texture + ".png")
    }

    function test_tileThicknessDrawsABevel() {
        const tile = createTemporaryObject(tileComponent, captureWindow.contentItem,
            {shape: "circle", fillColor: "#7895b0", borderWidth: 0})
        verify(waitForRendering(tile))
        const plain = grabImage(tile)
        tile.thickness = 8
        tryVerify(function() { return changedPixels(plain, grabImage(tile)) >= 50 })
        compare(grabImage(tile).red(2,2), 255)
    }

    function test_iconPlacementPreservesTheInputRectangle() {
        const icon = createTemporaryObject(iconComponent, captureWindow.contentItem,
            {iconSource: "file:///usr/share/icons/hicolor/scalable/apps/firefox.svg",
             tileSettings: {iconTileMode: "custom", iconTilesEnabled: true}})
        verify(waitForRendering(icon))
        tryVerify(function() { return icon.glyphItem.paintedWidth > 0 })
        const input = JSON.stringify(icon.logicalInputRegion)
        const before = icon.glyphItem.mapToItem(icon, 0, 0)
        const scale = icon.glyphItem.scale
        icon.tileSettings = {iconTileMode: "custom", iconTilesEnabled: true,
            iconTileIconOffsetX: 9, iconTileIconOffsetY: -6, iconTileIconScale: 75}
        const after = icon.glyphItem.mapToItem(icon, 0, 0)
        fuzzyCompare(icon.glyphItem.scale, scale * 0.75, 0.001)
        // Scale acts about the glyph centre; its centre is the placement anchor.
        const centre = icon.glyphItem.mapToItem(icon, icon.glyphItem.width / 2, icon.glyphItem.height / 2)
        fuzzyCompare(centre.x, 59, 0.001)
        fuzzyCompare(centre.y, 44, 0.001)
        compare(JSON.stringify(icon.logicalInputRegion), input)
        compare(icon.resolvedIconSource, "file:///usr/share/icons/hicolor/scalable/apps/firefox.svg")
        verify(after.x !== before.x && after.y !== before.y)
    }

    function test_styleTileTexturesReachEveryBuiltInStyle() {
        for (const styleId of ["plain-original", "metallic-blue", "metallic-red", "neon-green", "neon-orange", "dark-orb"]) {
            const xhr = new XMLHttpRequest()
            xhr.open("GET", Qt.resolvedUrl("../assets/icon-styles/" + styleId + "/archdock-icon-style.json"), false)
            xhr.send()
            verify(xhr.status === 0 || xhr.status === 200)
            const style = JSON.parse(xhr.responseText)
            style.valid = true; style.loadable = true; style.assetPaths = ({})
            const icon = createTemporaryObject(iconComponent, captureWindow.contentItem,
                {iconStyleDefinition: style, tileSettings: {iconShape: "circle"}, showIndicator: false, showReflection: false})
            verify(waitForRendering(icon))
            const plain = grabImage(icon)
            icon.tileSettings = {iconShape: "circle", iconTileTexture: "organic"}
            tryVerify(function() { return changedPixels(plain, grabImage(icon)) >= 50 },
                5000, styleId + " texture must change its declared tile body")
            icon.destroy(); wait(0)
        }
    }

    function test_tilesAndUprightGlyphsOrbitTogether() {
        // PD-22: actual scene transforms, not just the layout's returned values.
        const scene = createTemporaryObject(panelComponent, captureWindow.contentItem, {
            orderedEntries: [0,1,2,3].map(function(index) {
                return {id: "app-" + index, iconName: "file:///usr/share/icons/hicolor/scalable/apps/firefox.svg"}
            }),
            panelDefinition: {edge: "free", type: "launcher", layout: "circular", layoutRadius: 100,
                iconSize: 52, rendererTier: "procedural2d", iconShape: "hexagon", iconTileMode: "custom",
                iconTilesEnabled: true, iconTileTexture: "organic", panelMotionTarget: "items"},
            entryDelegateContext: {hostKind: "free"}, animationProfiles: {reducedMotion: true},
            hostCapabilities: {available: true, renderer: {effectiveTier: "procedural2d", fallbackApplied: false},
                rotation: {available: true}}
        })
        verify(scene !== null); verify(waitForRendering(scene))
        const tile = findChild(scene, "icon-custom-tile"); verify(tile)
        let icon = tile.parent
        while (icon && icon.glyphItem === undefined) icon = icon.parent
        verify(icon !== null)
        const glyph = icon.glyphItem
        const tileBefore = tile.mapToItem(captureWindow.contentItem, tile.width / 2, tile.height / 2)
        const glyphBefore = glyph.mapToItem(captureWindow.contentItem, glyph.width / 2, glyph.height / 2)
        scene.wheelTravel = 1
        verify(waitForRendering(scene))
        const tileAfter = tile.mapToItem(captureWindow.contentItem, tile.width / 2, tile.height / 2)
        const glyphAfter = glyph.mapToItem(captureWindow.contentItem, glyph.width / 2, glyph.height / 2)
        verify(Math.hypot(tileAfter.x - tileBefore.x, tileAfter.y - tileBefore.y) > 50)
        fuzzyCompare(glyphAfter.x - glyphBefore.x, tileAfter.x - tileBefore.x, 0.001)
        fuzzyCompare(glyphAfter.y - glyphBefore.y, tileAfter.y - tileBefore.y, 0.001)
        fuzzyCompare(glyphAfter.x, tileAfter.x, 0.001)
        fuzzyCompare(glyphAfter.y, tileAfter.y, 0.001)
        const origin = glyph.mapToItem(captureWindow.contentItem, 0, 0)
        const right = glyph.mapToItem(captureWindow.contentItem, 10, 0)
        fuzzyCompare(right.y, origin.y, 0.001)
        verify(right.x > origin.x)
        grabImage(scene).save("tile-upright-orbit.png")
    }

    function test_togglePreservesGlyphAndExplicitOverride() {
        const icon = createTemporaryObject(iconComponent, testCase,
            {tileSettings: {iconTileMode: "custom", iconTilesEnabled: true,
                            iconTileColor: "#ff22cc", iconTileOpacity: 0.35}})
        const tile = findChild(icon, "icon-custom-tile")
        verify(tile.visible)
        compare(tile.opacity, 0.35)
        compare(icon.resolvedIconSource, "folder")
        icon.entry = {tileEnabled: true, iconOverrideResolution: {override: {}, tileEnabled: true}}
        icon.tileSettings = {iconTileMode: "custom", iconTilesEnabled: false}
        verify(!tile.visible)
        compare(icon.resolvedIconSource, "folder")
        icon.entry = {iconOverrideResolution: {override: {tileEnabled: true}}}
        verify(tile.visible)
        icon.entry = {iconOverrideResolution: {override: {tileEnabled: false}}}
        icon.tileSettings = {iconTileMode: "custom", iconTilesEnabled: true}
        verify(!tile.visible)
    }

    function test_draftPreviewAndCancelRestoreTheTile() {
        const values = {iconTileMode: "style", iconTilesEnabled: true, iconTileColor: "#334155",
            iconTileOpacity: 0.8, iconTileBorderWidth: 1, iconTileBorderColor: "#94a3b8", iconShape: "rounded"}
        const session = EditorModel.load({success: true, status: "loaded", panelId: "bottom", revision: 1,
            panelValues: values, globalValues: {}, panelFields: Object.keys(values).map(function(key) {
                return {key: key, scope: "panel"}
            }), globalFields: []})
        verify(session.loaded)
        const draft = EditorModel.setPanelValue(EditorModel.setPanelValue(session, "iconTileMode", "custom"),
            "iconTileColor", "#ff22cc")
        verify(EditorModel.dirty(draft))
        const preview = createTemporaryObject(previewComponent, testCase,
            {panelDefinition: EditorModel.rendererCandidate(draft)})
        verify(waitForRendering(preview))
        const tile = findChild(preview, "icon-custom-tile")
        verify(tile !== null && tile.visible)
        compare(tile.fillColor, "#ff22cc")
        const cancelled = EditorModel.cancel(draft)
        verify(!EditorModel.dirty(cancelled))
        preview.panelDefinition = EditorModel.rendererCandidate(cancelled)
        tryCompare(tile, "visible", false)
        compare(EditorModel.panelCandidate(cancelled).iconTileColor, "#334155")
    }
}
