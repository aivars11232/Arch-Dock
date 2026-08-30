import QtQuick
import ArchDock.Rendering 1.0

Item {
    id: root

    property var themeDefinition: ({})
    property string orientation: "horizontal"
    property string presentationState: "open"
    property real panelOpacity: 1

    readonly property string effectiveState: normalizedState()
    readonly property var stateDefinition: objectById(
        themeDefinition ? themeDefinition.states : [], effectiveState)
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
    readonly property var glowAssetDefinition: objectById(
        themeDefinition ? themeDefinition.assets : [],
        glowLayerDefinition ? glowLayerDefinition.asset : "")
    readonly property var maskAssetDefinition: objectById(
        themeDefinition ? themeDefinition.assets : [],
        inputMaskDefinition ? inputMaskDefinition.asset : "")
    readonly property string surfaceSource: assetUrl(
        sliceDefinition ? sliceDefinition.asset : "")
    readonly property string glowSource: assetUrl(
        glowLayerDefinition ? glowLayerDefinition.asset : "")
    readonly property string maskSource: assetUrl(
        inputMaskDefinition ? inputMaskDefinition.asset : "")
    readonly property var glowSourceRect:
        glowLayerDefinition && glowLayerDefinition.sourceRect
        ? glowLayerDefinition.sourceRect
        : sliceDefinition ? sliceDefinition.sourceRect : null
    readonly property string glowBlendMode: String(
        glowLayerDefinition ? glowLayerDefinition.blendMode || "source-over"
                            : "source-over")
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
        && (!glowLayerDefinition
            || (glowAssetDefinition !== null && glowSource.length > 0
                && glowBlendMode === "source-over"))
        && surfaceSource.length > 0
        && maskSource.length > 0
        && naturalHeight > 0
        && fixedStartPixels >= 0 && fixedEndPixels >= 0
        && centerPixels > 0
    readonly property bool imagesReady:
        startImage.status === Image.Ready
        && centerImage.status === Image.Ready
        && endImage.status === Image.Ready
        && (!glowLayerDefinition
            || (glowStartImage.status === Image.Ready
                && glowCenterImage.status === Image.Ready
                && glowEndImage.status === Image.Ready))
    readonly property bool imageFailed:
        startImage.status === Image.Error
        || centerImage.status === Image.Error
        || endImage.status === Image.Error
        || (glowLayerDefinition
            && (glowStartImage.status === Image.Error
                || glowCenterImage.status === Image.Error
                || glowEndImage.status === Image.Error))
    readonly property bool rendererReady:
        structuralValid && imagesReady && alphaMask.ready
    readonly property string errorReason: validationError()
    readonly property var inputMaskItem: alphaMask.containmentObject
    readonly property var surfaceItem: surfaceParts

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

    function normalizedState() {
        const requested = String(presentationState || "normal").toLowerCase()
        return ["open", "collapsed"].includes(requested)
            ? requested : "normal"
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
        if (glowLayerDefinition && (!glowAssetDefinition
                || glowSource.length === 0))
            return "theme-asset-unavailable"
        if (glowLayerDefinition && glowBlendMode !== "source-over")
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

    Item {
        id: surfaceParts

        anchors.fill: parent

        Image {
            id: startImage

            x: 0
            y: 0
            width: Math.max(0, root.fixedStartPixels)
            height: parent.height
            source: root.surfaceSource
            sourceClipRect: root.sliceDefinition && root.sliceDefinition.sourceRect
                ? Qt.rect(
                    Number(root.sliceDefinition.sourceRect.x || 0),
                    Number(root.sliceDefinition.sourceRect.y || 0),
                    Number(root.sliceDefinition.fixedStart || 0),
                    Number(root.sliceDefinition.sourceRect.height || 0))
                : Qt.rect(0, 0, 0, 0)
            fillMode: Image.Stretch
            smooth: true
            asynchronous: false
            cache: true
        }

        Image {
            id: centerImage

            x: root.fixedStartPixels
            y: 0
            width: Math.max(0, root.centerPixels)
            height: parent.height
            source: root.surfaceSource
            sourceClipRect: root.sliceDefinition && root.sliceDefinition.sourceRect
                ? Qt.rect(
                    Number(root.sliceDefinition.sourceRect.x || 0)
                        + Number(root.sliceDefinition.fixedStart || 0),
                    Number(root.sliceDefinition.sourceRect.y || 0),
                    Number(root.sliceDefinition.sourceRect.width || 0)
                        - Number(root.sliceDefinition.fixedStart || 0)
                        - Number(root.sliceDefinition.fixedEnd || 0),
                    Number(root.sliceDefinition.sourceRect.height || 0))
                : Qt.rect(0, 0, 0, 0)
            fillMode: root.sliceDefinition
                    && String(root.sliceDefinition.centerMode || "stretch")
                        === "tile"
                ? Image.TileHorizontally : Image.Stretch
            smooth: true
            asynchronous: false
            cache: true
        }

        Image {
            id: endImage

            x: parent.width - root.fixedEndPixels
            y: 0
            width: Math.max(0, root.fixedEndPixels)
            height: parent.height
            source: root.surfaceSource
            sourceClipRect: root.sliceDefinition && root.sliceDefinition.sourceRect
                ? Qt.rect(
                    Number(root.sliceDefinition.sourceRect.x || 0)
                        + Number(root.sliceDefinition.sourceRect.width || 0)
                        - Number(root.sliceDefinition.fixedEnd || 0),
                    Number(root.sliceDefinition.sourceRect.y || 0),
                    Number(root.sliceDefinition.fixedEnd || 0),
                    Number(root.sliceDefinition.sourceRect.height || 0))
                : Qt.rect(0, 0, 0, 0)
            fillMode: Image.Stretch
            smooth: true
            asynchronous: false
            cache: true
        }

        Item {
            id: glowParts

            anchors.fill: parent
            opacity: Number(root.glowLayerDefinition
                            ? root.glowLayerDefinition.opacity : 0)
            visible: root.glowLayerDefinition !== null

            Image {
                id: glowStartImage

                x: 0
                y: 0
                width: Math.max(0, root.fixedStartPixels)
                height: parent.height
                source: root.glowSource
                sourceClipRect: root.glowSourceRect && root.sliceDefinition
                    ? Qt.rect(
                        Number(root.glowSourceRect.x || 0),
                        Number(root.glowSourceRect.y || 0),
                        Number(root.sliceDefinition.fixedStart || 0),
                        Number(root.glowSourceRect.height || 0))
                    : Qt.rect(0, 0, 0, 0)
                fillMode: Image.Stretch
                smooth: true
                asynchronous: false
                cache: true
            }

            Image {
                id: glowCenterImage

                x: root.fixedStartPixels
                y: 0
                width: Math.max(0, root.centerPixels)
                height: parent.height
                source: root.glowSource
                sourceClipRect: root.glowSourceRect && root.sliceDefinition
                    ? Qt.rect(
                        Number(root.glowSourceRect.x || 0)
                            + Number(root.sliceDefinition.fixedStart || 0),
                        Number(root.glowSourceRect.y || 0),
                        Number(root.glowSourceRect.width || 0)
                            - Number(root.sliceDefinition.fixedStart || 0)
                            - Number(root.sliceDefinition.fixedEnd || 0),
                        Number(root.glowSourceRect.height || 0))
                    : Qt.rect(0, 0, 0, 0)
                fillMode: root.sliceDefinition
                        && String(root.sliceDefinition.centerMode || "stretch")
                            === "tile"
                    ? Image.TileHorizontally : Image.Stretch
                smooth: true
                asynchronous: false
                cache: true
            }

            Image {
                id: glowEndImage

                x: parent.width - root.fixedEndPixels
                y: 0
                width: Math.max(0, root.fixedEndPixels)
                height: parent.height
                source: root.glowSource
                sourceClipRect: root.glowSourceRect && root.sliceDefinition
                    ? Qt.rect(
                        Number(root.glowSourceRect.x || 0)
                            + Number(root.glowSourceRect.width || 0)
                            - Number(root.sliceDefinition.fixedEnd || 0),
                        Number(root.glowSourceRect.y || 0),
                        Number(root.sliceDefinition.fixedEnd || 0),
                        Number(root.glowSourceRect.height || 0))
                    : Qt.rect(0, 0, 0, 0)
                fillMode: Image.Stretch
                smooth: true
                asynchronous: false
                cache: true
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
