import QtQuick
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

    Component { id: tileComponent; IconTile { width: 100; height: 100 } }
    Component { id: iconComponent; IconScene { logicalSize: 100; reducedMotion: true; iconSource: "folder" } }
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
