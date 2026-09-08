import QtQuick
import ArchDock.Rendering 1.0

Item {
    id: root

    property var panelDefinition: ({})
    property var runtimeState: ({})
    property var orderedEntries: []
    property var hostCapabilities: ({})
    property var themeDefinition: ({})
    property var iconStyleDefinition: ({})
    property var animationProfiles: ({})
    property var screenBounds: ({ x: 0, y: 0, width: 0, height: 0 })
    property var availableBounds: ({ x: 0, y: 0, width: 0, height: 0 })
    property Component entryDelegate: null
    property bool entryInteractionEnabled: true
    // Set by the host when the panel cannot be seen - hidden window, concealed
    // by auto-hide, or drawn at zero opacity. Continuous motion stops while it
    // is true. The default is false so a host that reports nothing still
    // animates rather than silently freezing.
    property bool sceneConcealed: false
    readonly property bool entriesAnimatable:
        !sceneConcealed && visible && opacity > 0
    property string geometryCompatibilityProfile: "canonical"
    property var entryDelegateContext: ({})
    // Previews freeze whole-scene rotation at the configured angle while still
    // reporting whether it is configured and available.
    property bool rotationAnimationEnabled: true

    readonly property int entryCount:
        orderedEntries && orderedEntries.length !== undefined
        ? orderedEntries.length : 0
    readonly property string layoutPath: String(definitionValue(
        "layout", "pathType", "layout", "horizontal"))
    readonly property bool verticalLayout:
        layoutPath === "vertical"
        || (layoutPath === "adaptive"
            && ["left", "right"].includes(String(definitionValue(
                "placement", "edge", "edge", "bottom"))))
    readonly property real layoutScale: Number(definitionValue(
        "layout", "scale", "layoutScale", 1))
    readonly property real layoutAngle: Number(definitionValue(
        "layout", "angle", "layoutAngle", 0))
    readonly property real layoutRadius: Number(definitionValue(
        "layout", "radius", "layoutRadius", 150))
    readonly property int layoutRows: Number(definitionValue(
        "layout", "rows", "layoutRows", 2))
    readonly property real layoutPadding: Number(definitionValue(
        "layout", "padding", "layoutPadding", 18))
    readonly property int polygonSides: Number(definitionValue(
        "layout", "polygonSides", "pathSides", 6))
    readonly property string pathOrientation: String(definitionValue(
        "layout", "orientation", "pathOrientation", "upright"))
    // The host edge this panel is docked to. A linear row has no outward side
    // of its own, so the edge is what tells motion which way is "out".
    readonly property string placementEdge: String(definitionValue(
        "placement", "edge", "edge", "bottom"))
    readonly property real iconSize: Number(iconValue("size", "iconSize", 52))
    readonly property real iconSpacing: Number(iconValue("spacing", "spacing", 8))
    readonly property string iconShape: String(iconValue(
        "shape", "iconShape", "rounded"))
    readonly property string appearance: String(definitionValue(
        "surface", "appearance", "appearance", "glass"))
    readonly property string customColor: String(definitionValue(
        "surface", "color", "color", ""))
    readonly property real panelOpacity: Number(definitionValue(
        "surface", "opacity", "opacity", 0.9))
    readonly property real glowIntensity: {
        const candidate = Number(definitionValue(
            "surface", "glowIntensity", "glowIntensity", 1))
        return isFinite(candidate) ? candidate : 1
    }
    readonly property color tintColor:
        customColor.length > 0 ? customColor : "#78e9f4"
    readonly property string themeId: String(definitionValue(
        "surface", "panelThemeId", "panelThemeId", ""))
    readonly property string themeSource: String(definitionValue(
        "surface", "themeAsset", "themeAsset", ""))
    readonly property string requestedRendererTier: String(definitionValue(
        "surface", "rendererTier", "rendererTier", "procedural2d")
        || "procedural2d")
    readonly property var capabilityRenderer:
        hostCapabilities && typeof hostCapabilities.renderer === "object"
        ? hostCapabilities.renderer : ({})
    readonly property string resolvedRendererTier: String(
        capabilityRenderer.effectiveTier
        || (hostCapabilities ? hostCapabilities.effectiveRendererTier : "")
        || requestedRendererTier || "procedural2d")
    readonly property string presentationState: {
        const requested = String(runtimeState
                                 ? runtimeState.presentationState || "open"
                                 : "open").toLowerCase()
        return ["normal", "open", "collapsed"].includes(requested)
            ? requested : "open"
    }
    readonly property string transitionState: {
        const requested = String(runtimeState
            ? runtimeState.transitionState || "idle" : "idle").toLowerCase()
        return ["opening", "closing"].includes(requested)
            ? requested : "idle"
    }
    readonly property real presentationProgress: {
        const candidate = Number(runtimeState
            ? runtimeState.presentationProgress : -1)
        return isFinite(candidate) && candidate >= 0 && candidate <= 1
            ? candidate : -1
    }
    readonly property bool panelHovered:
        Boolean(runtimeState ? runtimeState.hovered : false)
    readonly property bool reducedMotion:
        animationProfiles
            && animationProfiles.reducedMotion !== undefined
        ? Boolean(animationProfiles.reducedMotion)
        : Boolean(definitionValue(
            "motion", "reducedMotion", "reducedMotion", false))
    readonly property string themeOrientation:
        verticalLayout ? "vertical"
        : ["horizontal", "adaptive"].includes(layoutPath)
            ? "horizontal" : "free"
    // Whole-scene rotation (free radial layouts only). The controller yields
    // one angle offset; it is added to the configured layout angle and the sum
    // is what every geometry call receives, so entries, hover targets, drop
    // targets, popup anchors and the drawn surface all turn together.
    readonly property bool freeHost:
        String(entryDelegateContext && entryDelegateContext.hostKind
               ? entryDelegateContext.hostKind : "") === "free"
    readonly property bool rotationCapabilityAvailable:
        Boolean(hostCapabilities && hostCapabilities.rotation
                && typeof hostCapabilities.rotation === "object"
                && hostCapabilities.rotation.available === true)
    readonly property string rotationMode: String(definitionValue(
        "layout", "rotationMode", "panelRotationMode", "none"))
    readonly property real rotationSpeed: Number(definitionValue(
        "layout", "rotationSpeed", "panelRotationSpeed", 12))
    readonly property string rotationTrigger: String(definitionValue(
        "layout", "rotationTrigger", "panelRotationTrigger", "idle"))
    // A baked panel turns when its own track is closed, whatever the
    // configured path says: the artwork, not the layout, decides whether
    // sweeping the entries keeps them on the platform.
    readonly property bool rotationLayoutSupported:
        bakedTierRequested && activeThemeTrack !== null
        ? LayoutEngine.trackSupportsRotation(activeThemeTrack)
        : LayoutEngine.supportsWholeSceneRotation(layoutPath)
    readonly property bool sceneRotationEnabled: rotationController.enabled
    readonly property bool sceneRotationActive: rotationController.running
    readonly property real sceneRotationAngle: rotationController.angleOffset
    readonly property real effectiveLayoutAngle:
        layoutAngle + rotationController.angleOffset
    readonly property bool dragInProgress:
        Boolean(runtimeState ? runtimeState.dragInProgress : false)
    readonly property bool editModeActive:
        Boolean(runtimeState ? runtimeState.editMode : false)
    readonly property var configuredLayoutGeometry: LayoutEngine.metrics(
        layoutPath, entryCount, iconSize, iconSpacing, layoutScale,
        layoutRadius, layoutRows, layoutPadding, verticalLayout,
        layoutAngle, polygonSides)
    // While rotation is enabled the scene keeps the square every angle fits
    // in, so the host is not asked to resize on every frame.
    readonly property var layoutGeometry: sceneRotationEnabled
        ? LayoutEngine.rotationEnvelope(configuredLayoutGeometry)
        : configuredLayoutGeometry
    readonly property var activeThemeSlice: themeRecord(
        themeDefinition ? themeDefinition.slices : [],
        presentationState, themeOrientation)
    readonly property var activeThemeContentRegion: themeRecord(
        themeDefinition ? themeDefinition.contentRegions : [],
        presentationState, themeOrientation)
    readonly property bool skinMetadataUsable: usableSkinMetadata()

    // ---- Baked 2.5D ------------------------------------------------------
    // A baked panel is laid out on the theme's declared anchor track rather
    // than by the path engine. The artwork fixes where an icon may stand, so
    // the track is the geometry; the configured radius only says how large the
    // artwork is drawn, and the configured angle turns it.
    readonly property bool bakedTierRequested:
        String(resolvedRendererTier || "").toLowerCase() === "baked2.5d"
    readonly property var activeThemeTrack:
        ThemeStateSelection.trackFor(themeDefinition, presentationState)
    readonly property var bakedArtworkSize: buildBakedArtworkSize()
    readonly property bool bakedMetadataUsable: usableBakedMetadata()
    // Limited tilt, declared and clamped by the theme. It lives in the
    // internal 2.5D parameter map rather than as a visible control, because no
    // editor exposes it yet and a control that nothing reads would be untrue.
    readonly property real bakedTiltDegrees: {
        // definitionValue() deliberately refuses object values on the flat
        // form, because every flat key is a scalar. The 2.5D parameter map is
        // the exception, so it is read directly from both shapes.
        const definition = panelDefinition || ({})
        const section = definition.surface
        const nested = section && typeof section === "object"
            ? section.parameters2_5D : null
        const parameters = nested && typeof nested === "object"
            ? nested
            : definition.surface2_5D && typeof definition.surface2_5D === "object"
                ? definition.surface2_5D : null
        if (!parameters)
            return NaN
        const value = Number(parameters.tilt)
        return isFinite(value) ? value : NaN
    }
    readonly property var bakedTrackMetrics: bakedMetadataUsable
        ? LayoutEngine.trackMetrics(
            activeThemeTrack, bakedArtworkSize.width, bakedArtworkSize.height,
            entryCount, configuredLayoutGeometry.iconSize,
            configuredLayoutGeometry.padding, configuredLayoutGeometry.radius,
            bakedTiltDegrees, rotationController.enabled)
        : null

    readonly property var surfaceMetrics: buildSurfaceMetrics()
    readonly property var rendererGeometry: buildRendererGeometry()
    readonly property var rendererStyle: LayoutEngine.themeStyle(
        appearance, customColor, layoutGeometry.iconSize)
    // Requested presentation mechanism and axis, and the mechanisms the
    // resolver actually allowed. The scene passes all three to the motion
    // controller and never decides availability itself.
    readonly property string collapseMechanism: String(definitionValue(
        "presentation", "collapseMechanism", "collapseMechanism", "open"))
    readonly property string collapseAxis: String(definitionValue(
        "presentation", "collapseAxis", "collapseAxis", ""))
    readonly property var availablePresentationMechanisms:
        availableMechanismIds()

    readonly property real effectMargin: Math.ceil(
        Math.max(0, Number(rendererStyle.blur || 0))
        + Number(rendererStyle.lineWidth || 0) / 2)

    // A baked panel's content area is the whole scene box: its entries are
    // placed by the track in scene coordinates, not inset into a rectangle.
    readonly property var contentBounds: ({
        x: surfaceMetrics.contentX,
        y: surfaceMetrics.contentY,
        width: bakedMetadataUsable
            ? surfaceMetrics.width : layoutGeometry.width,
        height: bakedMetadataUsable
            ? surfaceMetrics.height : layoutGeometry.height
    })
    readonly property var visualBounds: ({
        x: 0,
        y: 0,
        width: surfaceMetrics.width,
        height: surfaceMetrics.height
    })
    readonly property var effectBounds: ({
        x: -surfaceMetrics.effectLeft,
        y: -surfaceMetrics.effectTop,
        width: surfaceMetrics.width + surfaceMetrics.effectLeft
            + surfaceMetrics.effectRight,
        height: surfaceMetrics.height + surfaceMetrics.effectTop
            + surfaceMetrics.effectBottom
    })
    readonly property var inputRegion: ({
        x: 0,
        y: 0,
        width: surfaceMetrics.width,
        height: surfaceMetrics.height
    })
    // Presentation tracks. These are outputs of one controller, so the live
    // applet, the Studio preview and a preset card cannot draw a collapse
    // differently from one another.
    readonly property var motionTracks: motionController.tracks
    readonly property string presentationTrackForm: motionController.trackForm
    readonly property string resolvedPresentationMechanism:
        motionController.resolvedMechanism
    readonly property string mechanismFallbackReason:
        motionController.fallbackReason
    readonly property string mechanismRequiredRendererTier:
        motionController.requiresRendererTier
    readonly property real collapseProgress: motionController.collapseProgress
    // Where entries may still be drawn. While the panel is open this is the
    // whole scene box, so magnification and motion headroom are untouched.
    readonly property var entryClipRect: collapseProgress > 0
        ? motionController.contentClip
        : ({ x: 0, y: 0, width: surfaceMetrics.width,
             height: surfaceMetrics.height })
    readonly property bool entryClipActive: collapseProgress > 0

    readonly property var revealHandle: buildRevealHandle()
    readonly property var popupAnchors: buildPopupAnchors()
    readonly property var previewAnchors: ({
        center: {
            x: surfaceMetrics.width / 2,
            y: surfaceMetrics.height / 2
        },
        entries: popupAnchors.entries
    })
    readonly property bool fallbackApplied:
        surfaceLoader.fallbackApplied
        || Boolean(capabilityRenderer.fallbackApplied)
        || String(runtimeState ? runtimeState.rendererFallback || "" : "")
            .length > 0
    readonly property string fallbackReason: {
        const runtimeReason = String(
            runtimeState ? runtimeState.rendererFallback || "" : "")
        if (runtimeReason.length > 0)
            return runtimeReason
        if (surfaceLoader.fallbackReason.length > 0)
            return surfaceLoader.fallbackReason
        if (capabilityRenderer.fallbackApplied)
            return String(capabilityRenderer.reasonCode || "capability-fallback")
        return ""
    }
    readonly property string effectiveRendererTier:
        surfaceLoader.effectiveRendererTier
    readonly property var true3DCapability: rendererCapabilityProbe.capability
    readonly property var runtimeCapabilityStatus: ({
        available: hostCapabilities
            && hostCapabilities.available !== undefined
            ? Boolean(hostCapabilities.available) : true,
        requestedRendererTier: requestedRendererTier,
        resolvedRendererTier: resolvedRendererTier,
        effectiveRendererTier: effectiveRendererTier,
        true3d: true3DCapability,
        fallbackApplied: fallbackApplied,
        fallbackReason: fallbackReason
    })
    RendererCapabilityProbe {
        id: rendererCapabilityProbe
    }
    readonly property var visualPanel: surfaceLoader.surfaceItem
    readonly property var iconDelegates: entryRepeater
    // Baked 2.5D outputs. `activeTrackMetrics` is null whenever the scene is
    // not laid out on a track, so a consumer cannot mistake a procedural or
    // skinned panel for a perspective one.
    // The instantiated renderer item for the active tier. `visualPanel` is the
    // drawn stack inside it; this is the renderer that owns that stack, which
    // is what a host or a test asks about resource and animation state.
    readonly property var activeSurfaceRenderer:
        surfaceLoader.true3DReady ? surfaceLoader.true3DItem
        : surfaceLoader.bakedReady ? surfaceLoader.bakedItem
        : surfaceLoader.skinnedReady ? surfaceLoader.skinnedItem : null
    readonly property var activeTrackMetrics: bakedTrackMetrics
    readonly property real occlusionDepth: surfaceLoader.occlusionDepth
    readonly property var foregroundOcclusionItem: foregroundOcclusion.item
    // Which input region is active: the package alpha mask for a skin, the
    // geometry band for a free radial scene, or the plain rectangle.
    // A baked platform is a solid shape inside a much larger box, so its input
    // region is the package's own alpha mask combined with the entry
    // rectangles: the empty desktop around and inside the platform passes
    // through, while an icon standing proud of the rim stays clickable.
    readonly property bool bakedInputActive:
        bakedMetadataUsable && surfaceLoader.inputMaskItem !== null
    readonly property bool geometryHitRegionActive:
        freeHost && (bakedInputActive
                     || (!surfaceLoader.inputMaskItem
                         && LayoutEngine.supportsWholeSceneRotation(layoutPath)))
    readonly property string activeInputRegionKind:
        bakedInputActive && geometryHitRegionActive ? "platform-mask"
        : surfaceLoader.inputMaskItem ? "alpha-mask"
        : geometryHitRegionActive ? "geometry-band" : "rectangle"
    readonly property var entryRects: buildEntryRects()

    containmentMask: geometryHitRegionActive
        ? geometryHitRegion
        : surfaceLoader.inputMaskItem ? surfaceLoader.inputMaskItem : null

    function containsInputPoint(point) {
        if (geometryHitRegionActive)
            return geometryHitRegion.contains(point)
        if (surfaceLoader.inputMaskItem)
            return surfaceLoader.inputMaskItem.contains(point)
        const x = Number(point.x)
        const y = Number(point.y)
        return x >= 0 && y >= 0 && x <= width && y <= height
    }

    function buildEntryRects() {
        const rects = []
        for (let index = 0; index < entryCount; ++index) {
            const output = entryGeometryAt(index)
            rects.push(output.entryBounds || {
                x: output.position.x, y: output.position.y,
                width: layoutGeometry.iconSize, height: layoutGeometry.iconSize
            })
        }
        return rects
    }

    function definitionValue(sectionName, key, flatKey, fallback) {
        const definition = panelDefinition || ({})
        const section = definition[sectionName]
        if (section && typeof section === "object"
                && section[key] !== undefined && section[key] !== null)
            return section[key]
        const flatValue = definition[flatKey]
        if (flatValue !== undefined && flatValue !== null
                && typeof flatValue !== "object")
            return flatValue
        return fallback
    }

    function iconValue(key, flatKey, fallback) {
        if (iconStyleDefinition && typeof iconStyleDefinition === "object"
                && iconStyleDefinition[key] !== undefined
                && iconStyleDefinition[key] !== null)
            return iconStyleDefinition[key]
        return definitionValue("iconStyle", key, flatKey, fallback)
    }

    function values(value) {
        return value && value.length !== undefined ? value : []
    }

    function themeRecord(collection, state, orientation) {
        const candidates = values(collection)
        for (let index = 0; index < candidates.length; ++index) {
            const candidate = candidates[index]
            if (String(candidate.state || "") === state
                    && String(candidate.orientation || "") === orientation)
                return candidate
        }
        return null
    }

    function availableMechanismIds() {
        const source = hostCapabilities && typeof hostCapabilities === "object"
            ? hostCapabilities.presentationMechanisms : null
        const decisions = values(source)
        const result = []
        for (let index = 0; index < decisions.length; ++index) {
            const decision = decisions[index]
            if (decision && typeof decision === "object"
                    && decision.available === true)
                result.push(String(decision.id || ""))
        }
        return result
    }

    function usableSkinMetadata() {
        if (String(resolvedRendererTier || "").toLowerCase() !== "skinned2d")
            return false
        const theme = themeDefinition || ({})
        if (theme.valid !== true
                || String(theme.format || "") !== "org.archdock.theme"
                || Number(theme.version || 0) !== 2)
            return false
        const candidateId = String(theme.id || theme.themeId || "")
        if (themeId.length > 0 && candidateId.length > 0
                && candidateId !== themeId)
            return false
        const slice = activeThemeSlice
        const region = activeThemeContentRegion
        if (!slice || !slice.sourceRect || !region || !region.rect
                || String(region.shape || "rect") !== "rect")
            return false
        const source = slice.sourceRect
        const content = region.rect
        const sourceWidth = Number(source.width || 0)
        const sourceHeight = Number(source.height || 0)
        const fixedStart = Number(slice.fixedStart || 0)
        const fixedEnd = Number(slice.fixedEnd || 0)
        const centerStart = Number(source.x || 0) + fixedStart
        const centerEnd = Number(source.x || 0) + sourceWidth - fixedEnd
        const contentStart = Number(content.x || 0)
        const contentEnd = contentStart + Number(content.width || 0)
        return sourceWidth > 0 && sourceHeight > 0
            && fixedStart >= 0 && fixedEnd >= 0
            && fixedStart + fixedEnd < sourceWidth
            && Number(content.width || 0) > 0
            && Number(content.height || 0) > 0
            && Number(content.y || 0) >= Number(source.y || 0)
            && Number(content.y || 0) + Number(content.height || 0)
                <= Number(source.y || 0) + sourceHeight
            && contentStart >= centerStart && contentEnd <= centerEnd
    }

    // Which declared state's artwork describes the panel's size. Kept strict:
    // a theme that does not declare the state it is asked for has no baked
    // geometry, and the scene falls back rather than guessing at one.
    function buildBakedArtworkSize() {
        const stateId = ThemeStateSelection.normalizedState(presentationState)
        const roles = ["rear", "surface"]
        for (let index = 0; index < roles.length; ++index) {
            const layer = ThemeStateSelection.layerForRole(
                themeDefinition, stateId, roles[index])
            if (!layer)
                continue
            const rect = layer.sourceRect
            if (rect && Number(rect.width) > 0 && Number(rect.height) > 0) {
                return {
                    width: Number(rect.width),
                    height: Number(rect.height)
                }
            }
            const asset = ThemeStateSelection.objectById(
                themeDefinition ? themeDefinition.assets : [], layer.asset)
            const natural = asset ? asset.naturalSize : null
            if (natural && Number(natural.width) > 0
                    && Number(natural.height) > 0) {
                return {
                    width: Number(natural.width),
                    height: Number(natural.height)
                }
            }
        }
        return { width: 0, height: 0 }
    }

    function usableBakedMetadata() {
        if (!bakedTierRequested)
            return false
        if (!ThemeStateSelection.isThemeProjection(themeDefinition))
            return false
        const candidateId = String(
            themeDefinition.id || themeDefinition.themeId || "")
        if (themeId.length > 0 && candidateId.length > 0
                && candidateId !== themeId)
            return false
        const track = activeThemeTrack
        if (!track)
            return false
        return bakedArtworkSize.width > 0 && bakedArtworkSize.height > 0
            && Number(track.radiusX) > 0 && Number(track.radiusY) > 0
    }

    function buildSurfaceMetrics() {
        if (bakedMetadataUsable && bakedTrackMetrics) {
            const margins = themeDefinition.effectMargins || ({})
            const scale = Number(bakedTrackMetrics.scale || 1)
            return {
                width: bakedTrackMetrics.width,
                height: bakedTrackMetrics.height,
                contentX: 0,
                contentY: 0,
                effectLeft: Math.max(0, Number(margins.left || 0)) * scale,
                effectTop: Math.max(0, Number(margins.top || 0)) * scale,
                effectRight: Math.max(0, Number(margins.right || 0)) * scale,
                effectBottom: Math.max(0, Number(margins.bottom || 0)) * scale,
                // A baked platform declares no end caps, so a collapse keeps
                // no handle of its own; the presentation controller supplies
                // the bounded minimum instead.
                handleExtent: 0
            }
        }
        if (!skinMetadataUsable) {
            return {
                width: layoutGeometry.width,
                height: layoutGeometry.height,
                contentX: 0,
                contentY: 0,
                effectLeft: effectMargin,
                effectTop: effectMargin,
                effectRight: effectMargin,
                effectBottom: effectMargin,
                handleExtent: 0
            }
        }

        const source = activeThemeSlice.sourceRect
        const content = activeThemeContentRegion.rect
        const sourceX = Number(source.x || 0)
        const sourceY = Number(source.y || 0)
        const sourceWidth = Number(source.width || 0)
        const sourceHeight = Number(source.height || 0)
        const fixedStart = Number(activeThemeSlice.fixedStart || 0)
        const fixedEnd = Number(activeThemeSlice.fixedEnd || 0)
        const centerSourceWidth = sourceWidth - fixedStart - fixedEnd
        const verticalScale = layoutGeometry.height
            / Number(content.height || 1)
        const centerScale = layoutGeometry.width
            / Number(content.width || 1)
        const startWidth = fixedStart * verticalScale
        const endWidth = fixedEnd * verticalScale
        const centerWidth = centerSourceWidth * centerScale
        const margins = themeDefinition.effectMargins || ({})
        return {
            width: startWidth + centerWidth + endWidth,
            height: sourceHeight * verticalScale,
            contentX: startWidth
                + (Number(content.x || 0) - sourceX - fixedStart)
                    * centerScale,
            contentY: (Number(content.y || 0) - sourceY) * verticalScale,
            effectLeft: Math.max(0, Number(margins.left || 0))
                * verticalScale,
            effectTop: Math.max(0, Number(margins.top || 0))
                * verticalScale,
            effectRight: Math.max(0, Number(margins.right || 0))
                * verticalScale,
            effectBottom: Math.max(0, Number(margins.bottom || 0))
                * verticalScale,
            // The declared end caps are what a fully collapsed shell keeps,
            // and therefore what the user has left to hover it back open.
            handleExtent: startWidth + endWidth
        }
    }

    function buildRendererGeometry() {
        const result = ({})
        const keys = Object.keys(layoutGeometry || ({}))
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = layoutGeometry[keys[index]]
        result.width = surfaceMetrics.width
        result.height = surfaceMetrics.height
        return result
    }

    function entryGeometryAt(index) {
        if (bakedMetadataUsable && bakedTrackMetrics) {
            // Track output is already in scene coordinates, so it needs no
            // content offset. Depth order stays the normalized 0..1 depth,
            // which is the same space the foreground layer's z uses.
            const track = LayoutEngine.trackEntryGeometry(
                activeThemeTrack, index, entryCount, bakedTrackMetrics,
                effectiveLayoutAngle, pathOrientation, bakedTiltDegrees)
            const output = ({})
            const trackKeys = Object.keys(track || ({}))
            for (let keyIndex = 0; keyIndex < trackKeys.length; ++keyIndex)
                output[trackKeys[keyIndex]] = track[trackKeys[keyIndex]]
            output.effectBounds = effectBounds
            output.effectAllowance = entryEffectAllowance(output.entryBounds)
            return output
        }
        const geometry = LayoutEngine.entryGeometry(
            layoutPath, index, entryCount, layoutGeometry, effectiveLayoutAngle,
            polygonSides, pathOrientation, geometryCompatibilityProfile,
            placementEdge)
        const result = ({})
        const keys = Object.keys(geometry || ({}))
        for (let keyIndex = 0; keyIndex < keys.length; ++keyIndex)
            result[keys[keyIndex]] = geometry[keys[keyIndex]]
        result.position = {
            x: Number(geometry.position.x || 0) + contentBounds.x,
            y: Number(geometry.position.y || 0) + contentBounds.y
        }
        result.x = result.position.x
        result.y = result.position.y
        result.depthOrder = Number(geometry.depthOrder || 0) + contentBounds.y
        result.bounds = contentBounds
        result.panelBounds = contentBounds
        result.safeInputRegion = contentBounds
        if (geometry.entryBounds) {
            result.entryBounds = {
                x: Number(geometry.entryBounds.x || 0) + contentBounds.x,
                y: Number(geometry.entryBounds.y || 0) + contentBounds.y,
                width: Number(geometry.entryBounds.width || 0),
                height: Number(geometry.entryBounds.height || 0)
            }
        }
        result.effectBounds = effectBounds
        result.effectAllowance = entryEffectAllowance(result.entryBounds)
        return result
    }

    // How far this entry may be displaced on each side before it escapes the
    // declared effect bounds. Motion clamps against these, so an effect can
    // never draw outside the margin the theme actually reserved.
    function entryEffectAllowance(entryBounds) {
        const bounds = entryBounds || ({})
        const left = Number(bounds.x || 0)
        const top = Number(bounds.y || 0)
        const right = left + Number(bounds.width || 0)
        const bottom = top + Number(bounds.height || 0)
        return {
            left: Math.max(0, left - effectBounds.x),
            top: Math.max(0, top - effectBounds.y),
            right: Math.max(0, effectBounds.x + effectBounds.width - right),
            bottom: Math.max(0, effectBounds.y + effectBounds.height - bottom)
        }
    }

    function entryLabel(entry, index) {
        const source = String(entry && (entry.displayName || entry.name
                              || entry.label || entry.id) || index + 1)
        return source.length > 0 ? source.charAt(0).toUpperCase() : "•"
    }

    function entryColor(entry) {
        if (entry && entry.active)
            return "#60d6ff"
        const configured = iconStyleDefinition
            ? iconStyleDefinition.color || iconStyleDefinition.foreground : ""
        return configured || "#d4e3eb"
    }

    function buildRevealHandle() {
        const edge = placementEdge
        const configuredSize = Number(definitionValue(
            "visibility", "revealZone", "revealZone", 8))
        const size = Math.max(1, Math.min(
            Math.max(surfaceMetrics.width, surfaceMetrics.height),
            isFinite(configuredSize) ? configuredSize : 8))
        let rect = {
            x: 0,
            y: surfaceMetrics.height - size,
            width: surfaceMetrics.width,
            height: size
        }
        if (edge === "top")
            rect = { x: 0, y: 0, width: surfaceMetrics.width, height: size }
        else if (edge === "left")
            rect = { x: 0, y: 0, width: size, height: surfaceMetrics.height }
        else if (edge === "right") {
            rect = {
                x: surfaceMetrics.width - size,
                y: 0,
                width: size,
                height: surfaceMetrics.height
            }
        }
        return {
            mode: String(definitionValue(
                "presentation", "revealHandle", "revealHandle", "edge-strip")),
            edge: edge,
            rect: rect,
            x: rect.x,
            y: rect.y,
            width: rect.width,
            height: rect.height
        }
    }

    function buildPopupAnchors() {
        const anchors = []
        for (let index = 0; index < entryCount; ++index) {
            const output = entryGeometryAt(index)
            anchors.push({
                index: index,
                entryId: String(orderedEntries[index]
                                ? orderedEntries[index].id || "" : ""),
                x: output.position.x + layoutGeometry.iconSize / 2
                    + output.outwardNormal.x * layoutGeometry.iconSize / 2,
                y: output.position.y + layoutGeometry.iconSize / 2
                    + output.outwardNormal.y * layoutGeometry.iconSize / 2,
                outwardNormal: output.outwardNormal
            })
        }
        const hovered = Number(runtimeState
                               ? runtimeState.hoveredEntry : -1)
        const primaryIndex = hovered >= 0 && hovered < anchors.length
            ? hovered : Math.max(0, Math.floor((anchors.length - 1) / 2))
        return {
            primary: anchors.length > 0 ? anchors[primaryIndex] : {
                index: -1,
                entryId: "",
                x: surfaceMetrics.width / 2,
                y: surfaceMetrics.height / 2,
                outwardNormal: { x: 0, y: -1, angle: -90 }
            },
            entries: anchors
        }
    }

    width: surfaceMetrics.width
    height: surfaceMetrics.height

    SceneRotationController {
        id: rotationController

        mode: root.rotationLayoutSupported ? root.rotationMode : "none"
        speedDegreesPerSecond: root.rotationSpeed
        trigger: root.rotationTrigger
        available: root.rotationCapabilityAvailable
        hovered: root.panelHovered
        dragActive: root.dragInProgress
        editMode: root.editModeActive
        sceneConcealed: root.sceneConcealed || !root.visible
        reducedMotion: root.reducedMotion
        animationEnabled: root.rotationAnimationEnabled
    }

    GeometryHitRegion {
        id: geometryHitRegion

        layout: root.layoutPath
        geometry: root.layoutGeometry
        angle: root.effectiveLayoutAngle
        polygonSides: root.polygonSides
        entryRects: root.entryRects
        bandWidth: root.layoutGeometry.iconSize * 1.2
        entryMargin: root.layoutGeometry.iconSize * 0.4
        enabled: root.geometryHitRegionActive
        maskItem: root.bakedInputActive ? surfaceLoader.inputMaskItem : null
        maskOriginX: surfaceLoader.inputMaskOriginX
        maskOriginY: surfaceLoader.inputMaskOriginY
    }

    PanelMotionController {
        id: motionController

        surfaceWidth: root.surfaceMetrics.width
        surfaceHeight: root.surfaceMetrics.height
        handleExtent: Number(root.surfaceMetrics.handleExtent || 0)
        contentBounds: root.contentBounds
        mechanism: root.collapseMechanism
        axis: root.collapseAxis
        availableMechanisms: root.availablePresentationMechanisms
        surfaceState: root.presentationState
        transitionState: root.transitionState
        presentationProgress: root.presentationProgress
        reducedMotion: root.reducedMotion
    }

    PanelSurfaceLoader {
        id: surfaceLoader

        anchors.fill: parent
        requestedRendererTier: root.resolvedRendererTier
        themeId: root.themeId
        themeSource: root.themeSource
        themeDefinition: root.themeDefinition
        layout: root.layoutPath
        presentationState: root.presentationState
        transitionState: root.transitionState
        presentationProgress: root.presentationProgress
        hovered: root.panelHovered
        tintColor: root.tintColor
        glowIntensity: root.glowIntensity
        reducedMotion: root.reducedMotion
        geometry: root.rendererGeometry
        layoutAngle: root.effectiveLayoutAngle
        polygonSides: root.polygonSides
        appearance: root.appearance
        customColor: root.customColor
        panelOpacity: root.panelOpacity
        motionTracks: root.motionTracks
        trackMetrics: root.bakedTrackMetrics || ({})
        true3DCapability: root.true3DCapability
        sceneQuality: String(root.definitionValue("surface", "parameters3D", "surface3D", ({})).quality
            || root.panelDefinition.scene3DQuality
            || (root.themeDefinition.scene3D || ({})).defaultQuality || "medium")
        entryGeometry: root.entryRects.map(function(rect) {
            return { centerX: rect.x + rect.width / 2, centerY: rect.y + rect.height / 2,
                     width: rect.width, height: rect.height }
        })
        sceneConcealed: root.sceneConcealed || !root.visible
    }

    // Entries are clipped by the same track that closes the shell, so an icon
    // is never left drawn outside a panel that has already collapsed. While the
    // panel is open the clipper covers the whole scene and does nothing.
    Item {
        id: entryClipper

        x: root.entryClipRect.x
        y: root.entryClipRect.y
        width: root.entryClipRect.width
        height: root.entryClipRect.height
        clip: root.entryClipActive

    Item {
        id: entryLayer

        x: -entryClipper.x
        y: -entryClipper.y
        width: root.surfaceMetrics.width
        height: root.surfaceMetrics.height

    // Foreground occlusion. Declared before the entries and given the theme's
    // occlusion depth as its z, so an entry nearer than that depth paints over
    // it and a further one paints under it. This is the only reason a real
    // application icon can pass behind a perspective platform rim.
    Loader {
        id: foregroundOcclusion

        anchors.fill: parent
        active: root.bakedMetadataUsable
            && surfaceLoader.foregroundComponent !== null
        sourceComponent: surfaceLoader.foregroundComponent
        z: surfaceLoader.occlusionDepth
    }

    Repeater {
        id: entryRepeater

        model: root.orderedEntries || []

        delegate: Item {
            id: entryItem

            required property var modelData
            required property int index
            readonly property var geometryOutput: root.entryGeometryAt(index)
            readonly property var sceneEntry: modelData
            readonly property int sceneIndex: index
            readonly property bool sceneInputEnabled:
                root.entryInteractionEnabled
            readonly property bool sceneVisible: root.entriesAnimatable
            readonly property var sceneGeometry: geometryOutput
            readonly property var sceneRuntimeState: root.runtimeState
            readonly property var scenePanelDefinition: root.panelDefinition
            readonly property var sceneHostCapabilities: root.hostCapabilities
            readonly property var sceneIconStyleDefinition:
                root.iconStyleDefinition
            readonly property var sceneContext: root.entryDelegateContext
            readonly property var delegateItem: entryLoader.item

            objectName: "panel-entry-" + index
            x: geometryOutput.position.x
            y: geometryOutput.position.y
            z: geometryOutput.depthOrder
            width: root.layoutGeometry.iconSize
            height: width
            rotation: geometryOutput.rotation
            scale: geometryOutput.scaleFactor
            opacity: root.entryDelegate === null && modelData
                && modelData.minimized ? 0.55 : 1

            Loader {
                id: entryLoader

                readonly property var sceneEntry: entryItem.sceneEntry
                readonly property int sceneIndex: entryItem.sceneIndex
                readonly property bool sceneInputEnabled:
                    entryItem.sceneInputEnabled
                readonly property bool sceneVisible: entryItem.sceneVisible
                readonly property var sceneGeometry: entryItem.sceneGeometry
                readonly property var sceneRuntimeState:
                    entryItem.sceneRuntimeState
                readonly property var scenePanelDefinition:
                    entryItem.scenePanelDefinition
                readonly property var sceneHostCapabilities:
                    entryItem.sceneHostCapabilities
                readonly property var sceneIconStyleDefinition:
                    entryItem.sceneIconStyleDefinition
                readonly property var sceneContext: entryItem.sceneContext

                anchors.fill: parent
                sourceComponent: root.entryDelegate || defaultEntryDelegate
            }
        }
    }
    }
    }

    Component {
        id: defaultEntryDelegate

        IconScene {
            readonly property int entryIndex: parent.sceneIndex

            anchors.fill: parent
            entry: parent.sceneEntry
            iconStyleDefinition: parent.sceneIconStyleDefinition
            logicalSize: Number(parent.sceneGeometry.iconSize || width)
            tileShape: root.iconShape
            appearance: root.appearance
            vertical: root.verticalLayout
            hovered: Number(root.runtimeState
                            ? root.runtimeState.hoveredEntry : -1)
                === entryIndex
            editMode: Boolean(root.runtimeState
                              ? root.runtimeState.editMode : false)
            showReflection: Boolean(root.definitionValue(
                "iconStyle", "showReflection", "showReflections", false))
            showIndicator: Boolean(root.definitionValue(
                "indicator", "visible", "showIndicators", true))
            reducedMotion: root.reducedMotion
        }
    }

    function entryItemAt(index) {
        return entryRepeater.itemAt(index)
    }
}
