import QtQuick
import ArchDock.Rendering 1.0

// Baked 2.5D panel surface.
//
// A perspective platform drawn as artwork, with real application icons placed
// on a declared anchor track and scaled by their depth on it. This is not a
// 3D scene: there is no mesh, no camera and no light, and nothing here may be
// described as true 3D. It is layered 2D artwork plus one depth ordering.
//
// The renderer draws the platform. It does not draw entries: PanelScene owns
// them, so that launching, drag and drop, accessibility and keyboard order
// stay host-owned exactly as they are for every other tier. What this file
// contributes to depth is `foregroundComponent`, which the scene instantiates
// among the entries at the theme's declared occlusion depth. An entry nearer
// than that depth is drawn after it and passes in front of the platform rim;
// an entry further away is drawn before it and passes behind.
//
// A theme whose required platform artwork is missing or undecodable reports a
// reason and leaves the scene to fall back; a decorative layer that fails is
// skipped and counted instead, because losing a reflection is not worth
// losing the dock.
Item {
    id: root

    property var themeDefinition: ({})
    // Scene geometry for the active track, from LayoutEngine.trackMetrics.
    property var trackMetrics: ({})
    property string presentationState: "open"
    property string transitionState: "idle"
    property real presentationProgress: -1
    property bool hovered: false
    property color tintColor: "#78e9f4"
    property real glowIntensity: 1
    property bool reducedMotion: false
    property real panelOpacity: 1
    // The host reports that nothing can be seen. Continuous motion stops.
    property bool sceneConcealed: false

    property real glowPhase: 0

    readonly property string effectiveState: ThemeStateSelection.effectiveStateId(
        themeDefinition, presentationState, transitionState,
        presentationProgress, hovered)
    readonly property bool interpolationActive:
        ThemeStateSelection.interpolationActive(
            themeDefinition, transitionState, presentationProgress)
    readonly property var stateDefinition:
        ThemeStateSelection.stateById(themeDefinition, effectiveState)
    readonly property var trackDefinition:
        ThemeStateSelection.trackFor(themeDefinition, effectiveState)
    readonly property var inputMaskDefinition: ThemeStateSelection.recordFor(
        themeDefinition ? themeDefinition.inputMasks : [],
        effectiveState, "free")
    readonly property var maskAssetDefinition: ThemeStateSelection.objectById(
        themeDefinition ? themeDefinition.assets : [],
        inputMaskDefinition ? inputMaskDefinition.asset : "")
    readonly property string maskSource: ThemeStateSelection.assetUrl(
        themeDefinition, inputMaskDefinition ? inputMaskDefinition.asset : "")

    // Layers behind the entries, in manifest order, and the foreground layers
    // that cut across them.
    readonly property var backgroundEntries: buildEntries(false)
    readonly property var foregroundEntries: buildEntries(true)
    readonly property bool hasForeground: foregroundEntries.length > 0
    readonly property var renderedLayerIds: {
        const ids = []
        for (let index = 0; index < backgroundEntries.length; ++index)
            ids.push(String(backgroundEntries[index].definition.id || ""))
        for (let index = 0; index < foregroundEntries.length; ++index)
            ids.push(String(foregroundEntries[index].definition.id || ""))
        return ids
    }

    readonly property real occlusionDepth: {
        const value = Number(trackMetrics && trackMetrics.occlusionDepth !== undefined
                             ? trackMetrics.occlusionDepth : 0.5)
        return isFinite(value) ? Math.max(0, Math.min(1, value)) : 0.5
    }
    readonly property var platformRect: {
        const source = trackMetrics && trackMetrics.platform
            && typeof trackMetrics.platform === "object"
            ? trackMetrics.platform : null
        function number(value, fallback) {
            const candidate = Number(value)
            return isFinite(candidate) ? candidate : fallback
        }
        if (!source)
            return Qt.rect(0, 0, Math.max(0, width), Math.max(0, height))
        return Qt.rect(number(source.x, 0), number(source.y, 0),
                       Math.max(0, number(source.width, width)),
                       Math.max(0, number(source.height, height)))
    }

    readonly property bool dynamicTintSupported:
        ThemeStateSelection.hasFeature(themeDefinition, "dynamic-tint")
    readonly property bool dynamicGlowSupported:
        ThemeStateSelection.hasFeature(themeDefinition, "dynamic-glow")
    readonly property color safeTintColor: safeTint(tintColor)
    readonly property real effectiveGlowIntensity: {
        if (!dynamicGlowSupported)
            return 1
        const bounded = Math.max(0, Math.min(2, Number(glowIntensity)))
        return Math.min(2, bounded * (hovered ? 1.18 : 1))
    }

    // One slow, bounded glow modulation. It runs only when the package asked
    // for a dynamic glow, a glow layer is actually present, reduced motion is
    // off, and the panel can be seen. Everything else holds it at rest.
    readonly property bool hasGlowLayer: glowLayerPresent()
    readonly property bool glowAnimationRunning:
        dynamicGlowSupported && hasGlowLayer && !reducedMotion
        && !sceneConcealed && root.visible && root.enabled && root.opacity > 0
    readonly property real effectiveGlowPhase:
        glowAnimationRunning ? glowPhase : 0

    readonly property bool structuralValid:
        ThemeStateSelection.isThemeProjection(themeDefinition)
        && stateDefinition !== null
        && trackDefinition !== null
        && inputMaskDefinition !== null
        && maskAssetDefinition !== null
        && maskSource.length > 0
        && backgroundEntries.length > 0
        && requiredPlatformDeclared()
        && unsupportedBlendLayerId().length === 0
        && platformRect.width > 0 && platformRect.height > 0
    readonly property bool imagesReady: layersSettled()
    readonly property bool imageFailed: requiredLayerFailed()
    readonly property bool rendererReady:
        structuralValid && imagesReady && !imageFailed && alphaMask.ready
    readonly property string errorReason: validationError()
    readonly property int skippedLayerCount: skippedLayers()

    readonly property var inputMaskItem:
        alphaMask.ready ? alphaMask.containmentObject : null
    readonly property real inputMaskOriginX: platformRect.x
    readonly property real inputMaskOriginY: platformRect.y
    readonly property var surfaceItem: backgroundStack
    readonly property alias foregroundComponent: foregroundLayers

    function values(value) {
        return ThemeStateSelection.values(value)
    }

    function objectById(collection, id) {
        return ThemeStateSelection.objectById(collection, id)
    }

    function assetUrl(assetId) {
        return ThemeStateSelection.assetUrl(themeDefinition, assetId)
    }

    // The platform itself. A `rear` layer is the baked equivalent of a
    // `surface`, and either satisfies the requirement.
    function requiredPlatformDeclared() {
        for (let index = 0; index < backgroundEntries.length; ++index) {
            const role = String(
                backgroundEntries[index].definition.role || "")
            if (role === "rear" || role === "surface")
                return true
        }
        return false
    }

    function isForegroundRole(role) {
        return String(role || "") === "foreground"
    }

    function buildEntries(foreground) {
        const ids = ThemeStateSelection.orderedActiveLayerIds(
            themeDefinition, presentationState, transitionState,
            presentationProgress, hovered)
        const entries = []
        for (let index = 0; index < ids.length; ++index) {
            const layer = objectById(
                themeDefinition ? themeDefinition.layers : [], ids[index])
            if (!layer)
                continue
            // A mesh or material layer belongs to a renderer this file is
            // deliberately not; it is left alone rather than drawn flat.
            const role = String(layer.role || "")
            if (role === "mesh" || role === "material")
                continue
            if (isForegroundRole(role) === Boolean(foreground))
                entries.push({ definition: layer })
        }
        return entries
    }

    function glowLayerPresent() {
        for (let index = 0; index < backgroundEntries.length; ++index) {
            const layer = backgroundEntries[index].definition
            if (String(layer.role || "") === "glow"
                    && assetUrl(layer.asset).length > 0)
                return true
        }
        return false
    }

    function stateOpacityForLayer(layer) {
        return ThemeStateSelection.stateLayerOpacity(
            themeDefinition, layer, transitionState, presentationProgress)
    }

    function runtimeOpacityMultiplier(layer) {
        if (String(layer ? layer.role || "" : "") !== "glow")
            return 1
        const pulse = glowAnimationRunning
            ? 0.92 + 0.08 * Math.cos(effectiveGlowPhase * Math.PI * 2) : 1
        return effectiveGlowIntensity * pulse
    }

    function safeTint(candidate) {
        const luminance = candidate.r * 0.2126
            + candidate.g * 0.7152 + candidate.b * 0.0722
        if (candidate.a < 0.5 || luminance < 0.16)
            return Qt.rgba(0.56, 0.91, 1, 1)
        return candidate
    }

    function unsupportedBlendLayerId() {
        const all = backgroundEntries.concat(foregroundEntries)
        for (let index = 0; index < all.length; ++index) {
            const layer = all[index].definition
            if (String(layer.blendMode || "source-over") !== "source-over")
                return String(layer.id || "unknown")
        }
        return ""
    }

    function missingReferencedLayerCount() {
        const ids = ThemeStateSelection.orderedActiveLayerIds(
            themeDefinition, presentationState, transitionState,
            presentationProgress, hovered)
        let count = 0
        for (let index = 0; index < ids.length; ++index) {
            if (!objectById(themeDefinition ? themeDefinition.layers : [],
                            ids[index]))
                ++count
        }
        return count
    }

    function eachLayerItem(callback) {
        for (let index = 0; index < backgroundRepeater.count; ++index)
            callback(backgroundRepeater.itemAt(index))
        const front = foregroundHost.item
        if (front && front.layerItems) {
            const items = front.layerItems()
            for (let index = 0; index < items.length; ++index)
                callback(items[index])
        }
    }

    function skippedLayers() {
        let count = missingReferencedLayerCount()
        eachLayerItem(function(item) {
            if (item && item.skipped)
                ++count
        })
        return count
    }

    function layersSettled() {
        if (backgroundRepeater.count !== backgroundEntries.length)
            return false
        let settled = true
        eachLayerItem(function(item) {
            if (!item || !item.settled)
                settled = false
        })
        return settled
    }

    function requiredLayerFailed() {
        let failed = false
        eachLayerItem(function(item) {
            if (item && item.failed)
                failed = true
        })
        return failed
    }

    function layerItemById(layerId) {
        const expected = String(layerId || "")
        let found = null
        eachLayerItem(function(item) {
            if (item && item.layerId === expected)
                found = item
        })
        return found
    }

    function validationError() {
        if (!themeDefinition || themeDefinition.valid !== true)
            return "theme-unavailable"
        if (!ThemeStateSelection.isThemeProjection(themeDefinition))
            return "theme-contract-invalid"
        if (!stateDefinition)
            return "theme-state-unavailable"
        if (!trackDefinition)
            return "theme-track-unavailable"
        if (!inputMaskDefinition)
            return "theme-input-mask-unavailable"
        if (!maskAssetDefinition || maskSource.length === 0)
            return "theme-asset-unavailable"
        if (!requiredPlatformDeclared())
            return "theme-platform-unavailable"
        if (unsupportedBlendLayerId().length > 0)
            return "theme-blend-mode-unsupported"
        if (platformRect.width <= 0 || platformRect.height <= 0)
            return "theme-geometry-invalid"
        if (imageFailed)
            return "theme-image-load-failed"
        if (alphaMask.errorReason.length > 0)
            return alphaMask.errorReason
        return ""
    }

    opacity: Math.max(0, Math.min(1, panelOpacity))

    NumberAnimation on glowPhase {
        from: 0
        to: 1
        duration: 4200
        loops: Animation.Infinite
        running: root.glowAnimationRunning
    }

    // The artwork occupies the platform rectangle the track metrics reported.
    // The scene box around it is larger wherever an icon overhangs the edge.
    Item {
        id: backgroundStack

        x: root.platformRect.x
        y: root.platformRect.y
        width: root.platformRect.width
        height: root.platformRect.height

        Repeater {
            id: backgroundRepeater

            model: root.backgroundEntries

            delegate: PanelSkinLayer2D {
                required property var modelData
                required property int index

                anchors.fill: parent
                z: index
                objectName: "panel-baked-layer-"
                    + String(modelData.definition.id || index)
                layerDefinition: modelData.definition
                assetDefinition: root.objectById(
                    root.themeDefinition ? root.themeDefinition.assets : [],
                    modelData.definition.asset)
                source: root.assetUrl(modelData.definition.asset)
                layerOpacity: root.stateOpacityForLayer(modelData.definition)
                    * root.runtimeOpacityMultiplier(modelData.definition)
                tintEnabled: root.dynamicTintSupported
                tintColor: root.safeTintColor
            }
        }
    }

    // Instantiated by PanelScene among the entries, never here: parented into
    // the entry layer it can interleave with icons by depth, which is the
    // whole point of a foreground occlusion layer.
    Component {
        id: foregroundLayers

        Item {
            id: foregroundRoot

            function layerItems() {
                const items = []
                for (let index = 0; index < foregroundRepeater.count; ++index)
                    items.push(foregroundRepeater.itemAt(index))
                return items
            }

            x: root.platformRect.x
            y: root.platformRect.y
            width: root.platformRect.width
            height: root.platformRect.height
            // The platform draws itself; input belongs to the scene's mask and
            // to the entries, so this never swallows a click.
            enabled: false

            Repeater {
                id: foregroundRepeater

                model: root.foregroundEntries

                delegate: PanelSkinLayer2D {
                    required property var modelData
                    required property int index

                    anchors.fill: parent
                    z: index
                    objectName: "panel-baked-foreground-"
                        + String(modelData.definition.id || index)
                    layerDefinition: modelData.definition
                    assetDefinition: root.objectById(
                        root.themeDefinition
                            ? root.themeDefinition.assets : [],
                        modelData.definition.asset)
                    source: root.assetUrl(modelData.definition.asset)
                    layerOpacity: root.stateOpacityForLayer(
                        modelData.definition)
                    tintEnabled: root.dynamicTintSupported
                    tintColor: root.safeTintColor
                }
            }
        }
    }

    Loader {
        id: foregroundHost

        // Kept out of the drawn tree here. PanelScene reparents an instance of
        // foregroundComponent into its entry layer; this instance exists only
        // so the renderer can report whether its foreground layers loaded.
        active: root.hasForeground
        sourceComponent: foregroundLayers
        visible: false
    }

    AlphaHitMask {
        id: alphaMask

        x: root.platformRect.x
        y: root.platformRect.y
        width: root.platformRect.width
        height: root.platformRect.height
        source: root.maskSource
        sourceSize: root.maskAssetDefinition
                && root.maskAssetDefinition.naturalSize
            ? Qt.size(
                Number(root.maskAssetDefinition.naturalSize.width || 0),
                Number(root.maskAssetDefinition.naturalSize.height || 0))
            : Qt.size(0, 0)
        // A baked platform is one whole image: no fixed caps, no tiled centre.
        sourceRect: root.maskAssetDefinition
                && root.maskAssetDefinition.naturalSize
            ? Qt.rect(0, 0,
                      Number(root.maskAssetDefinition.naturalSize.width || 0),
                      Number(root.maskAssetDefinition.naturalSize.height || 0))
            : Qt.rect(0, 0, 0, 0)
        fixedStart: 0
        fixedEnd: 0
        centerMode: "stretch"
        threshold: Number(root.inputMaskDefinition
                          ? root.inputMaskDefinition.threshold : 0.5)
        visible: false
    }
}
