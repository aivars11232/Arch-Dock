import QtQuick
import ArchDock.Rendering 1.0
import "ThemeStateSelection.js" as ThemeStates

// Chooses and loads the renderer for a panel's surface: the theme's skin
// (skinned 2D), its baked 2.5D platform or its 3D scene when they are
// requested and can be drawn, and the procedural 2D surface otherwise, and
// reports which one is in use and why.
Item {
    id: root

    property string requestedRendererTier: "procedural2d"
    property string themeId: ""
    property string themeSource: ""
    property var themeDefinition: ({})
    property string layout: "horizontal"
    property string presentationState: "open"
    property string transitionState: "idle"
    property real presentationProgress: -1
    property bool hovered: false
    property color tintColor: "#78e9f4"
    property real glowIntensity: 1
    property bool reducedMotion: false
    property var geometry: ({})
    property real layoutAngle: 0
    property int polygonSides: 6
    property string appearance: "glass"
    property string customColor: ""
    property real panelOpacity: 0.9
    property var motionTracks: null
    // Scene geometry for the active baked track, from LayoutEngine.
    property var trackMetrics: ({})
    property bool sceneConcealed: false
    property var true3DCapability: ({ rendererAvailable: false, reasonCode: "renderer-unavailable" })
    property var entryGeometry: []
    property var sceneParameters: ({})
    property var entryVisuals: []
    property real collapseProgress: 0
    property string mechanism: "open"
    property string sceneQuality: "medium"
    property real cameraPitch: NaN
    // Desktop 3D editing: handles on the mesh scene, and the edits it reports.
    property bool sceneEditMode: false
    signal sceneTransformEdited(var values)

    readonly property string normalizedRequestedTier:
        String(requestedRendererTier || "procedural2d").toLowerCase()
    readonly property bool themeRequested:
        themeId.length > 0 || themeSource.length > 0
    readonly property bool themeUsable: isThemeUsable(themeDefinition)
    readonly property bool true3DRequested: normalizedRequestedTier === "true3d"
    readonly property bool sceneResourcesUsable: themeUsable
        && ThemeStates.isThemeProjection(themeDefinition)
        && Boolean(themeDefinition.scene3D) && Boolean(themeDefinition.scene3DResources)
    readonly property bool true3DEnabled: true3DRequested && sceneResourcesUsable
        && Boolean(true3DCapability.rendererAvailable)
    readonly property var true3DItem: true3DRenderer.item
    readonly property bool true3DReady: true3DEnabled && true3DItem
        && Boolean(true3DItem.rendererReady)
    readonly property var themeTiers: {
        const source = themeDefinition && themeDefinition.capabilities
            ? themeDefinition.capabilities.rendererTiers || [] : []
        const result = []
        for (let index = 0; index < Number(source.length || 0); ++index)
            result.push(String(source[index]))
        return result
    }
    readonly property var fallbackTiers: {
        const source = themeDefinition && themeDefinition.capabilities
            ? themeDefinition.capabilities.fallbackRendererTiers || [] : []
        const result = []
        for (let index = 0; index < Number(source.length || 0); ++index) {
            const tier = String(source[index])
            if (tier === "procedural2d")
                break
            if (["baked2.5d", "skinned2d"].includes(tier)
                    && themeTiers.includes(tier) && !result.includes(tier))
                result.push(tier)
        }
        return result
    }
    readonly property int bakedFallbackIndex: fallbackTiers.indexOf("baked2.5d")
    readonly property int skinnedFallbackIndex: fallbackTiers.indexOf("skinned2d")
    readonly property bool skinnedRequested:
        normalizedRequestedTier === "skinned2d"
        || (true3DRequested && !true3DReady && skinnedFallbackIndex >= 0
            && (bakedFallbackIndex < 0 || skinnedFallbackIndex < bakedFallbackIndex || !bakedReady))
    readonly property bool bakedRequested:
        normalizedRequestedTier === "baked2.5d"
        || (true3DRequested && !true3DReady && bakedFallbackIndex >= 0
            && (skinnedFallbackIndex < 0 || bakedFallbackIndex < skinnedFallbackIndex || !skinnedReady))
    readonly property bool rendererSupported:
        ["procedural2d", "skinned2d", "baked2.5d", "true3d"]
            .includes(normalizedRequestedTier)
    readonly property var skinnedItem: skinnedRenderer.item
    readonly property bool skinnedReady:
        skinnedRequested && themeUsable && skinnedItem
        && Boolean(skinnedItem.rendererReady)
    readonly property var bakedItem: bakedRenderer.item
    readonly property bool bakedReady:
        bakedRequested && themeUsable && bakedItem
        && Boolean(bakedItem.rendererReady)
    readonly property bool fallbackApplied:
        !rendererSupported || (themeRequested && !themeUsable)
        || (skinnedRequested && !skinnedReady)
        || (bakedRequested && !bakedReady)
        || (true3DRequested && !true3DReady)
    readonly property string fallbackReason: {
        if (themeRequested && !themeUsable)
            return "theme-unavailable"
        if (!rendererSupported)
            return "renderer-unavailable"
        if (true3DRequested && !true3DReady) {
            if (!true3DCapability.rendererAvailable)
                return String(true3DCapability.reasonCode || "renderer-unavailable")
            if (!sceneResourcesUsable)
                return "scene3d-resources-unavailable"
            if (true3DRenderer.status === Loader.Error)
                return "scene3d-load-failed"
            return true3DItem ? String(true3DItem.errorReason || "renderer-loading") : "renderer-loading"
        }
        if (skinnedRequested && !skinnedReady) {
            if (skinnedItem && skinnedItem.errorReason.length > 0)
                return skinnedItem.errorReason
            return "renderer-loading"
        }
        if (bakedRequested && !bakedReady) {
            if (bakedItem && bakedItem.errorReason.length > 0)
                return bakedItem.errorReason
            return "renderer-loading"
        }
        return ""
    }
    readonly property string effectiveRendererTier:
        true3DReady ? "true3d" : bakedReady ? "baked2.5d" : skinnedReady ? "skinned2d" : "procedural2d"
    readonly property var surfaceItem:
        true3DReady ? true3DItem : bakedReady ? bakedItem
        : skinnedReady ? skinnedItem : proceduralRenderer.item
    readonly property var inputMaskItem:
        bakedReady ? bakedItem.inputMaskItem
        : skinnedReady ? skinnedItem.inputMaskItem : null
    // Where the baked mask sits inside the scene. A skin's mask covers the
    // whole scene, so it needs no offset.
    readonly property real inputMaskOriginX:
        bakedReady ? bakedItem.inputMaskOriginX : 0
    readonly property real inputMaskOriginY:
        bakedReady ? bakedItem.inputMaskOriginY : 0
    // The occlusion layers PanelScene interleaves with its entries, and the
    // depth at which they cut across them.
    readonly property var foregroundComponent:
        bakedReady && bakedItem.hasForeground
        ? bakedItem.foregroundComponent : null
    readonly property real occlusionDepth:
        bakedReady ? bakedItem.occlusionDepth : 0.5
    readonly property var contentRegionDefinition:
        skinnedReady ? skinnedItem.contentRegionDefinition : null
    readonly property var sliceDefinition:
        skinnedReady ? skinnedItem.sliceDefinition : null
    readonly property var effectMargins:
        skinnedReady && themeDefinition.effectMargins
        ? themeDefinition.effectMargins : ({ left: 0, top: 0, right: 0, bottom: 0 })

    function isThemeUsable(candidate) {
        if (!themeRequested)
            return true
        if (!candidate || typeof candidate !== "object"
                || Object.keys(candidate).length === 0)
            return false
        const candidateId = String(candidate.id || candidate.themeId || "")
        if (themeId.length > 0 && candidateId.length > 0
                && candidateId !== themeId)
            return false
        const candidateSource = String(
            candidate.source || candidate.themeAsset || "")
        if (themeSource.length > 0 && candidateSource.length > 0
                && candidateSource !== themeSource)
            return false
        if (candidate.valid === false || candidate.loadable === false)
            return false
        const status = String(
            candidate.status || candidate.themeStatus || "").toLowerCase()
        return !["invalid", "missing", "failed", "error", "unavailable"]
            .includes(status)
    }

    width: Number(geometry.width || 0)
    height: Number(geometry.height || 0)

    function synchronizeScene() {
        if (!true3DEnabled) {
            true3DRenderer.setSource("")
            return
        }
        true3DRenderer.setSource(Qt.resolvedUrl("optional3d/PanelScene3D.qml"), {
            sceneDefinition: Qt.binding(function() {
                const scene = root.themeDefinition && root.themeDefinition.scene3D || ({})
                const parameters = root.sceneParameters || ({})
                const merged = Object.assign({}, scene)
                // A generated platform follows the panel's own layout path.
                if (scene.generated)
                    merged.generated = Object.assign({}, scene.generated,
                        { layout: root.layout, polygonSides: root.polygonSides })
                for (const key of ["cameraYaw", "thickness", "iconElevation", "roll", "positionX",
                        "positionY", "positionZ", "scale", "fieldOfView", "keyLightBrightness",
                        "fillLightBrightness", "band", "bend"])
                    if (parameters[key] !== undefined && parameters[key] !== null
                            && Number.isFinite(Number(parameters[key])))
                        merged[key] = Number(parameters[key])
                for (const key of ["transitions", "float"])
                    if (typeof parameters[key] === "boolean") merged[key] = parameters[key]
                if (isFinite(root.cameraPitch)) merged.cameraPitch = Math.max(-60, Math.min(60, root.cameraPitch))
                return merged
            }),
            sceneResources: Qt.binding(function() {
                return root.themeDefinition && root.themeDefinition.scene3DResources || null
            }),
            textureSource: Qt.binding(function() {
                const definition = root.themeDefinition || ({})
                return ThemeStates.assetUrl(definition, (definition.scene3D || ({})).texture)
            }),
            entryGeometry: Qt.binding(function() { return root.entryGeometry }),
            layoutTrackRadius: Qt.binding(function() { return Number(root.geometry.radius || 0) }),
            appearance: Qt.binding(function() { return root.appearance }),
            customColor: Qt.binding(function() { return root.customColor }),
            entryVisuals: Qt.binding(function() { return root.entryVisuals }),
            layoutAngle: Qt.binding(function() { return root.layoutAngle }),
            collapseProgress: Qt.binding(function() { return root.collapseProgress }),
            mechanism: Qt.binding(function() { return root.mechanism }),
            hovered: Qt.binding(function() { return root.hovered }),
            reducedMotion: Qt.binding(function() { return root.reducedMotion }),
            glowIntensity: Qt.binding(function() { return root.glowIntensity }),
            quality: Qt.binding(function() { return root.sceneQuality }),
            panelOpacity: Qt.binding(function() { return root.panelOpacity }),
            sceneConcealed: Qt.binding(function() { return root.sceneConcealed }),
            editMode: Qt.binding(function() { return root.sceneEditMode })
        })
    }
    onTrue3DEnabledChanged: synchronizeScene()
    Component.onCompleted: synchronizeScene()

    Loader {
        id: proceduralRenderer
        objectName: "procedural-surface-fallback"

        anchors.fill: parent
        sourceComponent: proceduralComponent
        visible: !root.skinnedReady && !root.bakedReady && !root.true3DReady
    }

    Loader {
        id: skinnedRenderer

        anchors.fill: parent
        active: root.skinnedRequested && root.themeUsable
        sourceComponent: skinnedComponent
        visible: root.skinnedReady
    }

    Loader {
        id: true3DRenderer
        anchors.fill: parent
        active: root.true3DEnabled
        visible: root.true3DReady
        onLoaded: item.transformEdited.connect(root.sceneTransformEdited)
    }

    Loader {
        id: bakedRenderer

        anchors.fill: parent
        active: root.bakedRequested && root.themeUsable
        sourceComponent: bakedComponent
        visible: root.bakedReady
    }

    Component {
        id: proceduralComponent

        PanelProcedural2D {
            layout: root.layout
            geometry: root.geometry
            layoutAngle: root.layoutAngle
            polygonSides: root.polygonSides
            appearance: root.appearance
            customColor: root.customColor
            panelOpacity: root.panelOpacity
            motionTracks: root.motionTracks
        }
    }

    Component {
        id: bakedComponent

        PanelBaked25D {
            themeDefinition: root.themeDefinition
            trackMetrics: root.trackMetrics
            presentationState: root.presentationState
            transitionState: root.transitionState
            presentationProgress: root.presentationProgress
            hovered: root.hovered
            tintColor: root.tintColor
            glowIntensity: root.glowIntensity
            reducedMotion: root.reducedMotion
            sceneConcealed: root.sceneConcealed
            panelOpacity: root.panelOpacity
        }
    }

    Component {
        id: skinnedComponent

        PanelSkin2D {
            themeDefinition: root.themeDefinition
            orientation: root.layout === "vertical"
                ? "vertical" : root.layout === "horizontal"
                    ? "horizontal" : "free"
            presentationState: root.presentationState
            transitionState: root.transitionState
            presentationProgress: root.presentationProgress
            hovered: root.hovered
            tintColor: root.tintColor
            glowIntensity: root.glowIntensity
            reducedMotion: root.reducedMotion
            sceneConcealed: root.sceneConcealed
            panelOpacity: root.panelOpacity
            motionTracks: root.motionTracks
        }
    }
}
