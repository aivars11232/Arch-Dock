import QtQuick
import ArchDock.Rendering 1.0

Item {
    id: root

    property var themeDefinition: ({})
    property string orientation: "horizontal"
    property string presentationState: "open"
    property string transitionState: "idle"
    property real presentationProgress: -1
    property bool hovered: false
    property color tintColor: "#78e9f4"
    property real glowIntensity: 1
    property bool reducedMotion: false
    property real panelOpacity: 1

    property real overlayPhase: 0

    readonly property string normalizedPresentationState:
        normalizedState(presentationState)
    readonly property bool interpolationActive:
        ["opening", "closing"].includes(
            String(transitionState || "idle").toLowerCase())
        && isFinite(Number(presentationProgress))
        && Number(presentationProgress) >= 0
        && Number(presentationProgress) <= 1
        && stateById(transitionFromState) !== null
        && stateById(transitionToState) !== null
    readonly property string transitionFromState:
        String(transitionState || "idle").toLowerCase() === "opening"
        ? "collapsed" : "open"
    readonly property string transitionToState:
        String(transitionState || "idle").toLowerCase() === "opening"
        ? "open" : "collapsed"
    readonly property string effectiveState: effectiveStateId()
    readonly property var stateDefinition: stateById(effectiveState)
    readonly property var sliceDefinition: sliceFor(
        effectiveState, orientation)
    readonly property var contentRegionDefinition: contentRegionFor(
        effectiveState, orientation)
    readonly property var inputMaskDefinition: inputMaskFor(
        effectiveState, orientation)
    readonly property var glowLayerDefinition: layerForRole(
        stateDefinition, "glow")
    readonly property var sliceAssetDefinition: objectById(
        themeDefinition ? themeDefinition.assets : [],
        sliceDefinition ? sliceDefinition.asset : "")
    readonly property var maskAssetDefinition: objectById(
        themeDefinition ? themeDefinition.assets : [],
        inputMaskDefinition ? inputMaskDefinition.asset : "")
    readonly property string surfaceSource: assetUrl(
        sliceDefinition ? sliceDefinition.asset : "")
    readonly property string maskSource: assetUrl(
        inputMaskDefinition ? inputMaskDefinition.asset : "")
    readonly property real naturalHeight: Number(
        sliceDefinition && sliceDefinition.sourceRect
        ? sliceDefinition.sourceRect.height : 0)
    readonly property real verticalScale: naturalHeight > 0
        ? height / naturalHeight : 0
    readonly property real fixedStartPixels: Number(
        sliceDefinition ? sliceDefinition.fixedStart || 0 : 0)
        * verticalScale
    readonly property real fixedEndPixels: Number(
        sliceDefinition ? sliceDefinition.fixedEnd || 0 : 0)
        * verticalScale
    readonly property real centerPixels:
        width - fixedStartPixels - fixedEndPixels
    readonly property bool dynamicTintSupported:
        hasFeature("dynamic-tint")
    readonly property bool dynamicGlowSupported:
        hasFeature("dynamic-glow")
    readonly property color safeTintColor: safeTint(tintColor)
    readonly property real effectiveGlowIntensity: {
        if (!dynamicGlowSupported)
            return 1
        const bounded = Math.max(0, Math.min(2, Number(glowIntensity)))
        return Math.min(2, bounded * (hovered ? 1.18 : 1))
    }
    readonly property var renderEntries: buildRenderEntries()
    readonly property var renderedLayerIds: renderEntries.map(function(entry) {
        return String(entry.definition.id || "")
    })
    readonly property bool hasAnimatedOverlay: animatedOverlayAvailable()
    readonly property bool overlayAnimationRunning:
        dynamicGlowSupported && hasAnimatedOverlay && !reducedMotion
        && root.visible && root.enabled && root.opacity > 0
    readonly property real effectiveOverlayPhase:
        overlayAnimationRunning ? overlayPhase : 0
    readonly property int skippedLayerCount: skippedLayers()
    readonly property bool structuralValid:
        themeDefinition && themeDefinition.valid === true
        && String(themeDefinition.format || "") === "org.archdock.theme"
        && Number(themeDefinition.version || 0) === 2
        && stateDefinition !== null
        && sliceDefinition !== null
        && contentRegionDefinition !== null
        && inputMaskDefinition !== null
        && sliceAssetDefinition !== null
        && maskAssetDefinition !== null
        && surfaceSource.length > 0
        && maskSource.length > 0
        && naturalHeight > 0
        && fixedStartPixels >= 0 && fixedEndPixels >= 0
        && centerPixels > 0
        && renderEntries.length > 0
        && unsupportedBlendLayerId().length === 0
    readonly property bool imagesReady: layersSettled()
    readonly property bool imageFailed: requiredLayerFailed()
    readonly property bool rendererReady:
        structuralValid && imagesReady && !imageFailed && alphaMask.ready
    readonly property string errorReason: validationError()
    readonly property var inputMaskItem: alphaMask.containmentObject
    readonly property var surfaceItem: layerStack

    function values(value) {
        return value && value.length !== undefined ? value : []
    }

    function objectById(collection, id) {
        const expected = String(id || "")
        const candidates = values(collection)
        for (let index = 0; index < candidates.length; ++index) {
            if (String(candidates[index].id || "") === expected)
                return candidates[index]
        }
        return null
    }

    function hasFeature(feature) {
        const capabilities = themeDefinition
                && themeDefinition.capabilities
            ? themeDefinition.capabilities : ({})
        const features = values(capabilities.features)
        return features.map(function(value) {
            return String(value || "").toLowerCase()
        }).includes(feature)
    }

    function stateById(id) {
        return objectById(themeDefinition ? themeDefinition.states : [], id)
    }

    function normalizedState(value) {
        const requested = String(value || "normal").toLowerCase()
        return ["normal", "open", "collapsed"].includes(requested)
            ? requested : "normal"
    }

    function effectiveStateId() {
        if (!interpolationActive && hovered && stateById("hover"))
            return "hover"
        return normalizedPresentationState
    }

    function sliceFor(state, requestedOrientation) {
        const candidates = values(themeDefinition
                                  ? themeDefinition.slices : [])
        for (let index = 0; index < candidates.length; ++index) {
            const candidate = candidates[index]
            if (String(candidate.state || "") === state
                    && String(candidate.orientation || "")
                        === requestedOrientation)
                return candidate
        }
        return null
    }

    function contentRegionFor(state, requestedOrientation) {
        const candidates = values(themeDefinition
                                  ? themeDefinition.contentRegions : [])
        for (let index = 0; index < candidates.length; ++index) {
            const candidate = candidates[index]
            if (String(candidate.state || "") === state
                    && String(candidate.orientation || "")
                        === requestedOrientation)
                return candidate
        }
        return null
    }

    function inputMaskFor(state, requestedOrientation) {
        const candidates = values(themeDefinition
                                  ? themeDefinition.inputMasks : [])
        for (let index = 0; index < candidates.length; ++index) {
            const candidate = candidates[index]
            if (String(candidate.state || "") === state
                    && String(candidate.orientation || "")
                        === requestedOrientation)
                return candidate
        }
        return null
    }

    function layerForRole(state, role) {
        if (!state)
            return null
        const layerIds = values(state.layers)
        for (let index = 0; index < layerIds.length; ++index) {
            const layer = objectById(themeDefinition.layers, layerIds[index])
            if (layer && String(layer.role || "") === role)
                return layer
        }
        return null
    }

    function assetUrl(assetId) {
        const paths = themeDefinition && themeDefinition.assetPaths
            && typeof themeDefinition.assetPaths === "object"
            ? themeDefinition.assetPaths : ({})
        const path = String(paths[String(assetId || "")] || "")
        if (path.startsWith("file:") || path.startsWith("qrc:")
                || path.startsWith("image:"))
            return path
        if (path.startsWith("/"))
            return "file://" + encodeURI(path)
        return ""
    }

    function stateLayerIds(stateId) {
        const state = stateById(stateId)
        return state ? values(state.layers) : []
    }

    function layerInState(layerId, stateId) {
        return stateLayerIds(stateId).map(function(value) {
            return String(value || "")
        }).includes(String(layerId || ""))
    }

    function orderedActiveLayerIds() {
        const first = interpolationActive
            ? stateLayerIds(transitionFromState)
            : stateLayerIds(effectiveState)
        const result = []
        for (let index = 0; index < first.length; ++index) {
            const id = String(first[index] || "")
            if (id.length > 0 && !result.includes(id))
                result.push(id)
        }
        if (interpolationActive) {
            const second = stateLayerIds(transitionToState)
            for (let index = 0; index < second.length; ++index) {
                const id = String(second[index] || "")
                if (id.length > 0 && !result.includes(id))
                    result.push(id)
            }
        }
        return result
    }

    function syntheticBaseLayers() {
        if (!sliceDefinition || !sliceDefinition.sourceRect)
            return []
        const source = sliceDefinition.sourceRect
        const x = Number(source.x || 0)
        const y = Number(source.y || 0)
        const width = Number(source.width || 0)
        const height = Number(source.height || 0)
        const start = Number(sliceDefinition.fixedStart || 0)
        const end = Number(sliceDefinition.fixedEnd || 0)
        return [
            {
                id: "__slice-base-start", asset: sliceDefinition.asset,
                role: "split-start", opacity: 1,
                blendMode: "source-over",
                sourceRect: { x: x, y: y, width: start, height: height }
            },
            {
                id: "__slice-base-center", asset: sliceDefinition.asset,
                role: "split-center", opacity: 1,
                blendMode: "source-over",
                sourceRect: {
                    x: x + start, y: y,
                    width: width - start - end, height: height
                }
            },
            {
                id: "__slice-base-end", asset: sliceDefinition.asset,
                role: "split-end", opacity: 1,
                blendMode: "source-over",
                sourceRect: {
                    x: x + width - end, y: y,
                    width: end, height: height
                }
            }
        ]
    }

    function buildRenderEntries() {
        const ids = orderedActiveLayerIds()
        const entries = []
        let hasBase = false
        for (let index = 0; index < ids.length; ++index) {
            const layer = objectById(
                themeDefinition ? themeDefinition.layers : [], ids[index])
            if (!layer)
                continue
            const role = String(layer.role || "")
            if (["surface", "split-start", "split-center", "split-end"]
                    .includes(role))
                hasBase = true
            entries.push({ definition: layer })
        }
        if (!hasBase) {
            const base = syntheticBaseLayers()
            for (let index = base.length - 1; index >= 0; --index)
                entries.unshift({ definition: base[index] })
        }
        return entries
    }

    function stateOpacityForLayer(layer) {
        const base = Math.max(0, Math.min(1, Number(
            layer ? layer.opacity === undefined ? 1 : layer.opacity : 0)))
        if (!interpolationActive || String(layer.id || "").startsWith("__"))
            return base
        const inFrom = layerInState(layer.id, transitionFromState)
        const inTo = layerInState(layer.id, transitionToState)
        if (inFrom && inTo)
            return base
        const progress = Math.max(0, Math.min(1,
            Number(presentationProgress)))
        if (inFrom)
            return base * (1 - progress)
        if (inTo)
            return base * progress
        return 0
    }

    function isEnergyOverlay(layer) {
        return layer && String(layer.role || "") === "overlay"
            && String(layer.asset || "").includes("energy-overlay")
    }

    function runtimeOpacityMultiplier(layer) {
        if (String(layer ? layer.role || "" : "") === "glow")
            return effectiveGlowIntensity
        if (isEnergyOverlay(layer) && overlayAnimationRunning)
            return 0.94 + 0.06 * Math.cos(
                effectiveOverlayPhase * Math.PI * 2)
        return 1
    }

    function motionOffsetForLayer(layer) {
        return isEnergyOverlay(layer) && overlayAnimationRunning
            ? Math.sin(effectiveOverlayPhase * Math.PI * 2) * 2 : 0
    }

    function safeTint(candidate) {
        const luminance = candidate.r * 0.2126
            + candidate.g * 0.7152 + candidate.b * 0.0722
        if (candidate.a < 0.5 || luminance < 0.16)
            return Qt.rgba(0.56, 0.91, 1, 1)
        return candidate
    }

    function animatedOverlayAvailable() {
        for (let index = 0; index < renderEntries.length; ++index) {
            const layer = renderEntries[index].definition
            if (isEnergyOverlay(layer)
                    && assetUrl(layer.asset).length > 0)
                return true
        }
        return false
    }

    function missingReferencedLayerCount() {
        const ids = orderedActiveLayerIds()
        let count = 0
        for (let index = 0; index < ids.length; ++index) {
            if (!objectById(themeDefinition ? themeDefinition.layers : [],
                            ids[index]))
                ++count
        }
        return count
    }

    function skippedLayers() {
        let count = missingReferencedLayerCount()
        for (let index = 0; index < layerRepeater.count; ++index) {
            const item = layerRepeater.itemAt(index)
            if (item && item.skipped)
                ++count
        }
        return count
    }

    function layersSettled() {
        if (layerRepeater.count !== renderEntries.length)
            return false
        for (let index = 0; index < layerRepeater.count; ++index) {
            const item = layerRepeater.itemAt(index)
            if (!item || !item.settled)
                return false
        }
        return true
    }

    function requiredLayerFailed() {
        for (let index = 0; index < layerRepeater.count; ++index) {
            const item = layerRepeater.itemAt(index)
            if (item && item.failed)
                return true
        }
        return false
    }

    function unsupportedBlendLayerId() {
        for (let index = 0; index < renderEntries.length; ++index) {
            const layer = renderEntries[index].definition
            if (String(layer.blendMode || "source-over") !== "source-over")
                return String(layer.id || "unknown")
        }
        return ""
    }

    function layerItemById(layerId) {
        const expected = String(layerId || "")
        for (let index = 0; index < layerRepeater.count; ++index) {
            const item = layerRepeater.itemAt(index)
            if (item && item.layerId === expected)
                return item
        }
        return null
    }

    function validationError() {
        if (!themeDefinition || themeDefinition.valid !== true)
            return "theme-unavailable"
        if (String(themeDefinition.format || "") !== "org.archdock.theme"
                || Number(themeDefinition.version || 0) !== 2)
            return "theme-contract-invalid"
        if (!stateDefinition)
            return "theme-state-unavailable"
        if (!sliceDefinition)
            return "theme-orientation-unavailable"
        if (!contentRegionDefinition)
            return "theme-content-region-unavailable"
        if (!inputMaskDefinition)
            return "theme-input-mask-unavailable"
        if (!sliceAssetDefinition || !maskAssetDefinition
                || surfaceSource.length === 0 || maskSource.length === 0)
            return "theme-asset-unavailable"
        if (unsupportedBlendLayerId().length > 0)
            return "theme-blend-mode-unsupported"
        if (naturalHeight <= 0 || centerPixels <= 0)
            return "theme-geometry-invalid"
        if (imageFailed)
            return "theme-image-load-failed"
        if (alphaMask.errorReason.length > 0)
            return alphaMask.errorReason
        return ""
    }

    opacity: Math.max(0, Math.min(1, panelOpacity))
    containmentMask: alphaMask.ready ? alphaMask.containmentObject : null

    NumberAnimation on overlayPhase {
        from: 0
        to: 1
        duration: 3600
        loops: Animation.Infinite
        running: root.overlayAnimationRunning
    }

    Item {
        id: layerStack

        anchors.fill: parent

        Repeater {
            id: layerRepeater

            model: root.renderEntries

            delegate: PanelSkinLayer2D {
                required property var modelData
                required property int index

                anchors.fill: parent
                z: index
                objectName: "panel-skin-layer-"
                    + String(modelData.definition.id || index)
                layerDefinition: modelData.definition
                assetDefinition: root.objectById(
                    root.themeDefinition ? root.themeDefinition.assets : [],
                    modelData.definition.asset)
                sliceDefinition: root.sliceDefinition
                source: root.assetUrl(modelData.definition.asset)
                fixedStartPixels: root.fixedStartPixels
                fixedEndPixels: root.fixedEndPixels
                layerOpacity: root.stateOpacityForLayer(modelData.definition)
                    * root.runtimeOpacityMultiplier(modelData.definition)
                tintEnabled: root.dynamicTintSupported
                tintColor: root.safeTintColor
                motionOffset: root.motionOffsetForLayer(modelData.definition)
                motionOverflow: root.isEnergyOverlay(modelData.definition)
                    ? 3 : 0
            }
        }
    }

    AlphaHitMask {
        id: alphaMask

        width: root.width
        height: root.height
        source: root.maskSource
        sourceSize: root.maskAssetDefinition
                && root.maskAssetDefinition.naturalSize
            ? Qt.size(
                Number(root.maskAssetDefinition.naturalSize.width || 0),
                Number(root.maskAssetDefinition.naturalSize.height || 0))
            : Qt.size(0, 0)
        sourceRect: root.sliceDefinition && root.sliceDefinition.sourceRect
            ? Qt.rect(
                Number(root.sliceDefinition.sourceRect.x || 0),
                Number(root.sliceDefinition.sourceRect.y || 0),
                Number(root.sliceDefinition.sourceRect.width || 0),
                Number(root.sliceDefinition.sourceRect.height || 0))
            : Qt.rect(0, 0, 0, 0)
        fixedStart: Number(root.sliceDefinition
                           ? root.sliceDefinition.fixedStart || 0 : 0)
        fixedEnd: Number(root.sliceDefinition
                         ? root.sliceDefinition.fixedEnd || 0 : 0)
        centerMode: String(root.sliceDefinition
                           ? root.sliceDefinition.centerMode || "stretch"
                           : "stretch")
        threshold: Number(root.inputMaskDefinition
                          ? root.inputMaskDefinition.threshold : 0.5)
    }
}
