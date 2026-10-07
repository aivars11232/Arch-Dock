import QtQuick
import QtQuick.Window
import ArchDock.Rendering 1.0

// The shared panel renderer. Given a panel's definition, its runtime state,
// its entries and what the host and theme allow, it lays the entries out
// (LayoutEngine), draws the surface with the right renderer (procedural 2D,
// skinned 2D, baked 2.5D or true 3D, through PanelSurfaceLoader), animates
// opening and closing, rotation and icon motion, and reports what the host
// needs: entry rectangles, input region, popup anchors and the folder curve.
// The native applet, the free applet, Panel Studio's preview and preset
// cards all draw panels through it, so they cannot drift apart.
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
        && (!Window.window || Window.window.visible)
    property int entryVisualRevision: 0
    property int motionContextRevision: 0
    onThemeDefinitionChanged: motionContextRevision += 1
    readonly property string motionContextKey: String(motionContextRevision) + ":" + effectiveRendererTier
    readonly property var entryVisuals: {
        const revision = entryVisualRevision
        const result = []
        for (let index = 0; index < entryRepeater.count; ++index)
            result.push(entryRepeater.itemAt(index))
        return result
    }
    readonly property var motionCatalog: {
        const result = ({})
        const list = animationProfiles.animationProfiles || panelDefinition.animationProfiles || []
        for (let index = 0; index < list.length; ++index)
            result[list[index].id] = list[index]
        return result
    }
    readonly property var defaultIconProfiles: {
        const context = motionContextKey
        const selected = animationProfiles.animationProfile || panelDefinition.animationProfile
            || motionCatalog[String(animationProfiles.iconAnimation
                || definitionValue("motion", "iconProfile", "iconAnimation", "none"))]
        if (!selected) return []
        const copy = Object.assign({}, selected)
        const trigger = AnimationProfileRuntime.normalizeTrigger(animationProfiles.animationTrigger
            || panelDefinition.animationTrigger || selected.trigger)
        if (trigger) copy.trigger = trigger
        return [copy]
    }
    property string geometryCompatibilityProfile: "canonical"
    property var entryDelegateContext: ({})
    // Previews freeze whole-scene rotation at the configured angle while still
    // reporting whether it is configured and available.
    property bool rotationAnimationEnabled: true

    readonly property int entryCount:
        orderedEntries && orderedEntries.length !== undefined
        ? orderedEntries.length : 0
    // State snapshots change on every window update. Only membership/order
    // changes may replace delegates; otherwise their open menus lose owners.
    readonly property string entryIdentityOrder: JSON.stringify(
        (orderedEntries || []).map(function(entry, index) {
            return String(entry.id || entry.appId || index)
        }))
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
    // Free-panel motion (ADREP-TASK-002). The entries travel along the
    // panel's own path, the whole scene turns, or both, as "Continuous motion
    // moves" says, and the wheel and a drag move the same thing. Travel is a
    // phase in entry slots that every geometry call receives
    // (LayoutEngine.trackPlacement); a turn is an angle offset added to the
    // configured layout angle. Entries, hover targets, drop targets and popup
    // anchors follow either, because they are positioned by geometry rather
    // than by a visual transform. Both are transient and never saved.
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
    readonly property real travelSpeed: Number(definitionValue(
        "layout", "travelSpeed", "panelTravelSpeed", 0.5))
    // How many slots one wheel notch, or one slot's distance of touchpad
    // travel, moves the entries (PD-16).
    readonly property real scrollSensitivity: {
        const value = Number(definitionValue(
            "layout", "scrollSensitivity", "scrollSensitivity", 1))
        return isFinite(value) ? Math.max(0.25, Math.min(4, value)) : 1
    }
    // Panel Studio offers Scroll sensitivity where the wheel moves the panel's
    // own entries or turns it, a free radial panel; its folders scroll by it
    // there too, and one child or row per notch elsewhere (PD-16).
    readonly property real folderScrollSensitivity:
        freeHost && rotationCapabilityAvailable && LayoutEngine.supportsWholeSceneRotation(layoutPath)
        ? scrollSensitivity : 1
    // A baked panel turns when its own track is closed, whatever the
    // configured path says: the artwork, not the layout, decides whether
    // sweeping the entries keeps them on the platform.
    readonly property bool rotationLayoutSupported:
        bakedTierRequested && activeThemeTrack !== null
        ? LayoutEngine.trackSupportsRotation(activeThemeTrack)
        : LayoutEngine.supportsWholeSceneRotation(layoutPath)
    // What moves (PD-25): the entries along the path, the whole panel, or
    // both. Where the scene cannot turn - an open baked track - only the
    // entries can, whatever the record says.
    readonly property string motionTarget: {
        const value = String(definitionValue(
            "layout", "motionTarget", "panelMotionTarget", "items")).trim().toLowerCase()
        return rotationLayoutSupported && ["panel", "both"].includes(value)
            ? value : "items"
    }
    readonly property bool sceneRotationEnabled: rotationController.enabled
    readonly property bool sceneRotationActive: rotationController.running
    readonly property bool travelMotionEnabled: rotationController.travelEnabled
    readonly property bool travelMotionActive: rotationController.travelRunning
    // The wheel's and a drag's share of the turn and of the travel. A wheel
    // step eases the share towards its target (stepMotion); a drag moves it
    // directly. The remainder is wheel input not yet worth a whole step.
    property real wheelRotationAngle: 0
    property real wheelRotationTarget: 0
    property real wheelTravel: 0
    property real wheelTravelTarget: 0
    property real wheelRemainder: 0
    property bool rotationDragActive: false
    readonly property bool rotationGeometryAvailable:
        freeHost && rotationCapabilityAvailable && rotationLayoutSupported
    // The loop the entries travel round: whether they travel at all, how many
    // of them stand on the path and after how many slots they are back.
    readonly property var trackWindow: bakedMetadataUsable && bakedTrackMetrics
        ? ({ travels: entryCount > 1, closed: bakedTrackMetrics.closed === true,
             windowed: bakedTrackMetrics.windowed === true,
             capacity: Number(bakedTrackMetrics.capacity || 0),
             loop: Number(bakedTrackMetrics.loop || entryCount) })
        : LayoutEngine.pathWindow(layoutPath, entryCount, trackGeometry)
    readonly property bool travelGeometryAvailable:
        freeHost && trackWindow.travels === true
    // A folder or menu holds the dock still; a hover preview does not, so a
    // wheel turn is never swallowed by a preview the pointer passed over.
    readonly property bool modalPopupOpen: Boolean(runtimeState.modalPopupOpen !== undefined
        ? runtimeState.modalPopupOpen : runtimeState.popupOpen)
    readonly property bool wheelInputAvailable:
        entryInteractionEnabled && entriesAnimatable && !dragInProgress
        && !editModeActive && !modalPopupOpen && presentationState !== "collapsed"
    readonly property bool wheelTravelAvailable:
        travelGeometryAvailable && motionTarget !== "panel" && wheelInputAvailable
    readonly property bool wheelRotationAvailable:
        rotationGeometryAvailable && motionTarget !== "items" && wheelInputAvailable
    // Desktop 3D editing, set by the host while Panel Studio holds an edit of
    // this panel. The handles take the whole panel; wheel and drag motion
    // wait until the edit ends.
    property bool sceneEditActive: false
    signal sceneTransformEdited(var values)
    // The wheel moved the entries or turned the dock.
    signal wheelUsed()
    // The last wheel event the scene took, as Qt delivered it.
    property var lastWheelInput: ({})
    // How long one wheel step takes to arrive, eased out (OF-11). It ends
    // within 120 ms of the wheel event, the frame that shows it included.
    readonly property int stepDuration: 100
    readonly property real sceneRotationAngle:
        rotationController.angleOffset + wheelRotationAngle
    readonly property real effectiveLayoutAngle:
        layoutAngle + sceneRotationAngle
    // How far the entries have travelled along their path, in entry slots.
    readonly property real entryTravel: travelGeometryAvailable
        ? wheelTravel + rotationController.travelOffset : 0
    // The distance and the turn between two neighbouring entries at rest:
    // what one slot of travel is on screen. Touchpad pixels and a drag's
    // angle are counted in it.
    readonly property var travelPitch: {
        if (!travelGeometryAvailable || entryCount < 2)
            return ({ pixels: 0, degrees: 0 })
        const centre = travelCentre()
        const points = [0, 1].map(function(index) {
            const bounds = baseEntryGeometryAt(index, 0).entryBounds
            return { x: bounds.x + bounds.width / 2, y: bounds.y + bounds.height / 2 }
        })
        const turn = (Math.atan2(points[1].y - centre.y, points[1].x - centre.x)
            - Math.atan2(points[0].y - centre.y, points[0].x - centre.x)) * 180 / Math.PI
        return ({ pixels: Math.hypot(points[1].x - points[0].x, points[1].y - points[0].y),
                  degrees: Math.abs(((turn % 360) + 540) % 360 - 180) })
    }
    onRotationGeometryAvailableChanged: if (!rotationGeometryAvailable) resetTurn()
    onTravelGeometryAvailableChanged: if (!travelGeometryAvailable) resetTravel()
    // Another layout starts with its entries in their own places and the
    // panel unturned.
    onLayoutPathChanged: {
        resetTravel()
        resetTurn()
    }
    onReducedMotionChanged: if (reducedMotion) foldContinuousTravel()

    // The centre a drag turns about and a slot's turn is measured from: the
    // baked track's centre, otherwise the middle of the laid-out scene.
    function travelCentre() {
        if (bakedMetadataUsable && bakedTrackMetrics && bakedTrackMetrics.center)
            return bakedTrackMetrics.center
        return { x: contentBounds.x + layoutGeometry.width / 2,
                 y: contentBounds.y + layoutGeometry.height / 2 }
    }

    // Wheel input gathers until it makes whole steps (PD-16): 120 angle
    // units are one notch, and touchpad pixels count by the distance between
    // two neighbouring entries. One step moves the entries one slot, or
    // turns the panel 15 degrees, and Scroll sensitivity says how many steps
    // a notch makes. Returns the steps taken.
    function takeWheel(angleDelta, pixelDelta) {
        const pixels = Number(pixelDelta) || 0
        const notches = pixels !== 0 && travelPitch.pixels > 0
            ? pixels / travelPitch.pixels : (Number(angleDelta) || 0) / 120
        wheelRemainder += notches * scrollSensitivity
        const steps = wheelRemainder >= 0
            ? Math.floor(wheelRemainder + 1e-6) : Math.ceil(wheelRemainder - 1e-6)
        wheelRemainder -= steps
        stepMotion(steps)
        return steps
    }

    // A step eases out over stepDuration measured from the wheel event, so the
    // very next frame already shows it moving. Steps that arrive while one is
    // running retarget it from where the entries are, so a spun wheel never
    // builds up a backlog.
    function stepMotion(steps) {
        if (steps === 0)
            return
        if (motionTarget !== "panel" && travelGeometryAvailable) {
            wheelTravelTarget += steps
            easeTravel()
        }
        if (motionTarget !== "items" && rotationGeometryAvailable) {
            wheelRotationTarget += steps * 15
            easeTurn()
        }
    }

    // Where a running step started, and when. A step counts the frame that
    // first shows it and takes that frame's share at once: the frame drawn
    // next always moves, before the frame clock has ticked for the step.
    readonly property real frameInterval: 1000 / 60
    property real travelStepFrom: 0
    property real travelStepStarted: 0
    property bool travelStepping: false
    property real turnStepFrom: 0
    property real turnStepStarted: 0
    property bool turnStepping: false

    function easeTravel() {
        if (reducedMotion || !entriesAnimatable) {
            travelStepping = false
            wheelTravel = wheelTravelTarget
            restTravel()
            return
        }
        travelStepFrom = wheelTravel
        travelStepStarted = Date.now() - frameInterval
        travelStepping = true
        advanceSteps()
    }

    function easeTurn() {
        if (reducedMotion || !entriesAnimatable) {
            turnStepping = false
            wheelRotationAngle = wheelRotationTarget
            restTurn()
            return
        }
        turnStepFrom = wheelRotationAngle
        turnStepStarted = Date.now() - frameInterval
        turnStepping = true
        advanceSteps()
    }

    // Ease out (cubic) by the time since the step began.
    function stepShare(started) {
        const share = Math.max(0, Math.min(1, (Date.now() - started) / stepDuration))
        return { done: share >= 1, eased: 1 - Math.pow(1 - share, 3) }
    }

    function advanceSteps() {
        if (travelStepping) {
            const step = stepShare(travelStepStarted)
            wheelTravel = step.done ? wheelTravelTarget
                : travelStepFrom + (wheelTravelTarget - travelStepFrom) * step.eased
            if (step.done) {
                travelStepping = false
                restTravel()
            }
        }
        if (turnStepping) {
            const step = stepShare(turnStepStarted)
            wheelRotationAngle = step.done ? wheelRotationTarget
                : turnStepFrom + (wheelRotationTarget - turnStepFrom) * step.eased
            if (step.done) {
                turnStepping = false
                restTurn()
            }
        }
    }

    // At rest the shares are brought back into one turn of their loop, which
    // draws the same and keeps the numbers small.
    function restTravel() {
        const loop = Number(trackWindow.loop || 0)
        if (loop > 0 && Math.abs(wheelTravel) >= loop) {
            const turns = Math.trunc(wheelTravel / loop) * loop
            wheelTravel -= turns
            wheelTravelTarget -= turns
        }
    }

    function restTurn() {
        const turns = Math.floor(wheelRotationAngle / 360) * 360
        wheelRotationAngle -= turns
        wheelRotationTarget -= turns
    }

    // A drag moves the entries, or turns the panel, by the pointer's turn
    // about the centre, without easing.
    function dragMotion(degrees) {
        if (motionTarget !== "panel" && travelGeometryAvailable
                && travelPitch.degrees > 0) {
            travelStepping = false
            wheelTravel += degrees / travelPitch.degrees
            wheelTravelTarget = wheelTravel
        }
        if (motionTarget !== "items" && rotationGeometryAvailable) {
            turnStepping = false
            wheelRotationAngle += degrees
            wheelRotationTarget = wheelRotationAngle
        }
    }

    // Released entries come to rest in their slots, so none is left half
    // faded at the end of an open path.
    function endDragMotion() {
        rotationDragActive = false
        if (wheelTravel !== Math.round(wheelTravel)) {
            wheelTravelTarget = Math.round(wheelTravel)
            easeTravel()
        }
        restTurn()
    }

    // Continuous travel that stops leaves its share with the wheel's, and
    // the entries ease into the nearest slots instead of jumping back.
    function foldContinuousTravel() {
        const offset = rotationController.travelOffset
        rotationController.travelOffset = 0
        if (offset === 0 && wheelTravel === Math.round(wheelTravel))
            return
        travelStepping = false
        wheelTravel += offset
        wheelTravelTarget = Math.round(wheelTravel)
        easeTravel()
    }

    function resetTravel() {
        travelStepping = false
        wheelTravel = 0
        wheelTravelTarget = 0
        wheelRemainder = 0
        rotationController.travelOffset = 0
    }

    function resetTurn() {
        turnStepping = false
        wheelRotationAngle = 0
        wheelRotationTarget = 0
    }

    FrameAnimation {
        running: root.travelStepping || root.turnStepping
        onTriggered: root.advanceSteps()
    }

    Connections {
        target: rotationController
        function onTravelEnabledChanged() {
            if (!rotationController.travelEnabled)
                root.foldContinuousTravel()
        }
    }
    readonly property bool dragInProgress:
        Boolean(runtimeState ? runtimeState.dragInProgress : false)
    readonly property bool editModeActive:
        Boolean(runtimeState ? runtimeState.editMode : false)
    readonly property var segmentDefinitions: panelDefinition.segments || []
    readonly property bool segmentedScene:
        resolvedRendererTier === "procedural2d"
        && ["horizontal", "vertical", "adaptive"].includes(layoutPath)
        && (segmentDefinitions.length > 1 || segmentDefinitions.some(function(segment) {
            return ![undefined, "inherited"].includes(segment.background)
                || ![undefined, "inherited"].includes(segment.corners)
                || Number(segment.padding) >= 0 || Number(segment.spacing) >= 0
                || segment.presentation === "closed" || Boolean(segment.motionProfile)
        }))
    readonly property var segmentLayout: segmentedScene ? LayoutEngine.segmentGeometry(
        segmentDefinitions, orderedEntries, layoutPath, iconSize, iconSpacing,
        layoutScale, layoutPadding, verticalLayout, layoutAngle,
        pathOrientation, geometryCompatibilityProfile, placementEdge) : null
    readonly property var segmentSurfaces: segmentRepeater
    // Consumers ask for the segment layout itself. Asking whether the scene is
    // segmented and then reading the layout fails for one evaluation whenever
    // the flag changes first.
    readonly property var configuredLayoutGeometry: segmentLayout
        ? segmentLayout.geometry : LayoutEngine.metrics(
        layoutPath, entryCount, iconSize, iconSpacing, layoutScale,
        layoutRadius, layoutRows, layoutPadding, verticalLayout,
        layoutAngle, polygonSides)
    // While rotation is enabled the scene keeps the square every angle fits
    // in, so the host is not asked to resize on every frame.
    readonly property var layoutGeometry: sceneRotationEnabled || rotationGeometryAvailable
        ? LayoutEngine.rotationEnvelope(configuredLayoutGeometry)
        : configuredLayoutGeometry
    // A mesh scene draws its entries on the platform's own track, which is
    // not the layout radius. Spacing is measured where the icons really are.
    readonly property real meshTrackRadius:
        surfaceLoader.true3DReady && surfaceLoader.true3DItem
        ? Number(surfaceLoader.true3DItem.entryTrackRadius || 0) : 0
    readonly property var trackGeometry: Object.assign({}, layoutGeometry, {
        trackRadius: meshTrackRadius
    })
    // What the entries are placed with: the track geometry plus how far they
    // have travelled along it.
    readonly property var entryLayoutGeometry: Object.assign({}, trackGeometry, {
        travel: entryTravel
    })
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
        || (surfaceLoader.true3DRequested && surfaceLoader.bakedRequested)
    readonly property var activeThemeTrack:
        ThemeStateSelection.trackFor(themeDefinition, presentationState)
    readonly property var bakedArtworkSize: buildBakedArtworkSize()
    readonly property bool bakedMetadataUsable: usableBakedMetadata()
    // Limited perspective tilt, declared and clamped by the theme.
    readonly property real bakedTiltDegrees: {
        // definitionValue() deliberately refuses object values on the flat
        // form, because every flat key is a scalar. The 2.5D parameter map is
        // the exception, so it is read directly from both shapes.
        const definition = panelDefinition || ({})
        if (definition.bakedTilt !== undefined)
            return Number(definition.bakedTilt)
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
            // A free panel's entries can go round its track, by turning or
            // travelling, so its box holds every place they pass through.
            bakedTiltDegrees, freeHost || sceneRotationEnabled,
            // The canonical spacing regulates a baked track as it does every
            // other curved one.
            ({ spacing: configuredLayoutGeometry.spacing,
               spacingReference: configuredLayoutGeometry.spacingReference }))
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

    containmentMask: sceneEditActive ? null
        : geometryHitRegionActive ? geometryHitRegion
        : surfaceLoader.inputMaskItem ? surfaceLoader.inputMaskItem : null

    function containsInputPoint(point) {
        if (segmentLayout) {
            return segmentLayout.segments.some(function(run) {
                return point.x >= run.x && point.y >= run.y
                    && point.x <= run.x + run.width && point.y <= run.y + run.height
            })
        }
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
            // An entry off its path (or leaving or coming back onto an open
            // one) has no rectangle to press. The list keeps one record per
            // entry; this one can contain no point.
            if (output.onTrack === false) {
                rects.push({ x: -100000, y: -100000, width: 0, height: 0 })
                continue
            }
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
        if (String(resolvedRendererTier || "").toLowerCase() !== "skinned2d"
                && !(surfaceLoader.true3DRequested && surfaceLoader.skinnedRequested))
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
        const base = baseEntryGeometryAt(index)
        const mesh = surfaceLoader.true3DReady ? surfaceLoader.true3DItem : null
        const rect = mesh && mesh.projectedEntryGeometry ? mesh.projectedEntryGeometry[index] : null
        if (!rect) return base
        return Object.assign({}, base, {
            x: rect.x, y: rect.y,
            position: {x: rect.x + (rect.width-layoutGeometry.iconSize)/2,
                y: rect.y + (rect.height-layoutGeometry.iconSize)/2},
            entryBounds: {x: rect.x, y: rect.y, width: rect.width, height: rect.height},
            projectedWidth: rect.width, projectedHeight: rect.height, rotation: 0,
            depthOrder: -rect.depth,
            effectAllowance: entryEffectAllowance(rect)
        })
    }
    // `travel` places the entry as if the entries had travelled that far; by
    // default it is where they are now.
    function baseEntryGeometryAt(index, travel) {
        const phase = travel === undefined ? entryTravel : Number(travel)
        if (segmentLayout && segmentLayout.entries[index]) {
            const result = Object.assign({}, segmentLayout.entries[index])
            result.effectBounds = effectBounds
            result.effectAllowance = entryEffectAllowance(result.entryBounds)
            return result
        }
        if (bakedMetadataUsable && bakedTrackMetrics) {
            // Track output is already in scene coordinates, so it needs no
            // content offset. Depth order stays the normalized 0..1 depth,
            // which is the same space the foreground layer's z uses.
            const track = LayoutEngine.trackEntryGeometry(
                activeThemeTrack, index, entryCount, bakedTrackMetrics,
                effectiveLayoutAngle, pathOrientation, bakedTiltDegrees, phase)
            const output = ({})
            const trackKeys = Object.keys(track || ({}))
            for (let keyIndex = 0; keyIndex < trackKeys.length; ++keyIndex)
                output[trackKeys[keyIndex]] = track[trackKeys[keyIndex]]
            output.effectBounds = effectBounds
            output.effectAllowance = entryEffectAllowance(output.entryBounds)
            return output
        }
        const geometry = LayoutEngine.entryGeometry(
            layoutPath, index, entryCount, travel === undefined
                ? entryLayoutGeometry : Object.assign({}, trackGeometry, { travel: phase }),
            surfaceLoader.true3DReady ? 0 : effectiveLayoutAngle,
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

    // The centre a curved free panel is drawn around, in scene coordinates:
    // the projected platform in true 3D, the artwork's track when baked.
    // Other layouts' own normals already point away from what is drawn.
    function drawnCentre() {
        if (!freeHost) return null
        const mesh = surfaceLoader.true3DReady ? surfaceLoader.true3DItem : null
        if (mesh && mesh.projectedCentre) return mesh.projectedCentre
        if (bakedMetadataUsable && bakedTrackMetrics && bakedTrackMetrics.center)
            return bakedTrackMetrics.center
        return null
    }
    // The middle of the dock as drawn, or of its laid-out scene.
    function dockCentre() {
        return drawnCentre() || { x: contentBounds.x + layoutGeometry.width / 2,
                                  y: contentBounds.y + layoutGeometry.height / 2 }
    }
    // The drawn centres of every entry but `folderIndex`: a folder's fan,
    // arc, stack or ring keeps clear of them (ADREP-TASK-003).
    function folderObstacles(folderIndex) {
        const size = layoutGeometry.iconSize
        return popupAnchors.entries.filter(function(anchor) {
            return anchor.index !== folderIndex
        }).map(function(anchor) {
            return { x: anchor.x - anchor.outwardNormal.x * size / 2,
                     y: anchor.y - anchor.outwardNormal.y * size / 2 }
        })
    }

    // "Along the dock": the curve a folder's contents follow, `outward`
    // pixels out of the dock, as `count` scene-coordinate samples spanning
    // `span` radians around the folder at `folderIndex`. True 3D projects the
    // platform's own track; a baked or flat curve is the ellipse through the
    // folder with the drawn track's proportions. `scale` is the icons' drawn
    // size there relative to a flat icon.
    function folderTrackSamples(folderIndex, outward, span, count) {
        if (!freeHost || folderIndex < 0 || folderIndex >= entryCount || count < 2) return []
        const output = entryGeometryAt(folderIndex)
        const folder = { x: output.position.x + layoutGeometry.iconSize / 2,
                         y: output.position.y + layoutGeometry.iconSize / 2 }
        const mesh = surfaceLoader.true3DReady ? surfaceLoader.true3DItem : null
        if (mesh && mesh.folderTrackSamples) {
            const drawn = mesh.projectedEntryGeometry ? mesh.projectedEntryGeometry[folderIndex] : null
            const size = drawn ? drawn.height / Math.max(1, layoutGeometry.iconSize) : 1
            return mesh.folderTrackSamples(folderIndex, outward, span, count).map(function(sample) {
                return { x: sample.x, y: sample.y, scale: sample.scale * size }
            })
        }
        const centre = dockCentre()
        const metrics = bakedMetadataUsable ? bakedTrackMetrics : null
        const ratio = metrics && metrics.radiusX > 0 ? metrics.radiusY / metrics.radiusX
            : layoutPath === "ellipse" ? 0.62 : 1
        const dx = folder.x - centre.x, dy = (folder.y - centre.y) / Math.max(0.05, ratio)
        const angle = Math.atan2(dy, dx)
        // The step out of the dock that appears `outward` pixels long on
        // screen at the folder, however far the drawn track is tilted there.
        const stretch = Math.hypot(Math.cos(angle), ratio * Math.sin(angle))
        const radius = Math.hypot(dx, dy) + Math.max(0, outward) / Math.max(0.05, stretch)
        const result = []
        for (let index = 0; index < count; ++index) {
            const turn = angle - span / 2 + span * index / (count - 1)
            result.push({ x: centre.x + radius * Math.cos(turn),
                          y: centre.y + radius * ratio * Math.sin(turn), scale: 1 })
        }
        return result
    }

    // Popups open away from the dock as it is drawn: a tilted, turned or
    // projected platform faces out of its drawn centre.
    function buildPopupAnchors() {
        const anchors = []
        const centre = drawnCentre()
        for (let index = 0; index < entryCount; ++index) {
            const output = entryGeometryAt(index)
            let normal = output.outwardNormal
            if (centre) {
                const dx = output.position.x + layoutGeometry.iconSize / 2 - centre.x
                const dy = output.position.y + layoutGeometry.iconSize / 2 - centre.y
                const length = Math.hypot(dx, dy)
                if (length > 1)
                    normal = { x: dx / length, y: dy / length, angle: Math.atan2(dy, dx) * 180 / Math.PI }
            }
            anchors.push({
                index: index,
                entryId: String(orderedEntries[index]
                                ? orderedEntries[index].id || "" : ""),
                x: output.position.x + layoutGeometry.iconSize / 2
                    + normal.x * layoutGeometry.iconSize / 2,
                y: output.position.y + layoutGeometry.iconSize / 2
                    + normal.y * layoutGeometry.iconSize / 2,
                outwardNormal: normal
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

        mode: root.rotationLayoutSupported || root.travelGeometryAvailable
            ? root.rotationMode : "none"
        speedDegreesPerSecond: root.rotationSpeed
        trigger: root.rotationTrigger
        available: root.freeHost && root.rotationCapabilityAvailable
            && root.rotationLayoutSupported
        turnsPanel: root.motionTarget !== "items"
        movesItems: root.motionTarget !== "panel"
        travelAvailable: root.travelGeometryAvailable
        travelSpeed: root.travelSpeed
        travelPeriod: Number(root.trackWindow.loop || 0)
        hovered: root.panelHovered
        dragActive: root.dragInProgress
        editMode: root.editModeActive
        configuring: Boolean(root.runtimeState.popupOpen) || root.rotationDragActive
        sceneConcealed: !root.entriesAnimatable || root.presentationState === "collapsed"
        reducedMotion: root.reducedMotion
        animationEnabled: root.rotationAnimationEnabled
    }

    // Scrolling up moves the entries clockwise along the path (or turns the
    // panel clockwise), scrolling down the other way. This is transient
    // geometry, not a saved edit.
    WheelHandler {
        target: null
        enabled: (root.wheelRotationAvailable || root.wheelTravelAvailable) && !root.sceneEditActive
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.NoModifier
        onWheel: function(event) {
            const angle = event.angleDelta.y
            const pixels = event.pixelDelta.y
            if (angle === 0 && pixels === 0) {
                event.accepted = false
                return
            }
            root.lastWheelInput = { angleDelta: angle, pixelDelta: pixels, at: Date.now() }
            root.takeWheel(angle, pixels)
            event.accepted = true
            root.wheelUsed()
        }
    }

    MouseArea {
        anchors.fill: parent
        z: 0.5 // Entry delegates remain above this background gesture.
        enabled: (root.wheelRotationAvailable || root.wheelTravelAvailable) && !root.sceneEditActive
        acceptedButtons: Qt.LeftButton
        property real previousAngle: 0
        onPressed: mouse => {
            if (!root.containsInputPoint(Qt.point(mouse.x, mouse.y))) { mouse.accepted = false; return }
            const centre = root.travelCentre()
            previousAngle = Math.atan2(mouse.y - centre.y, mouse.x - centre.x)
            root.rotationDragActive = true
        }
        onPositionChanged: mouse => {
            if (!pressed || !root.rotationDragActive) return
            const centre = root.travelCentre()
            const angle = Math.atan2(mouse.y - centre.y, mouse.x - centre.x)
            const delta = Math.atan2(Math.sin(angle-previousAngle), Math.cos(angle-previousAngle))
            previousAngle = angle
            root.dragMotion(delta * 180 / Math.PI)
        }
        onReleased: root.endDragMotion()
        onCanceled: root.endDragMotion()
    }

    GeometryHitRegion {
        id: geometryHitRegion

        layout: root.layoutPath
        geometry: root.layoutGeometry
        angle: root.effectiveLayoutAngle
        polygonSides: root.polygonSides
        entryRects: root.entryRects
        projectedScene: surfaceLoader.true3DReady ? surfaceLoader.true3DItem : null
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
        rendererTier: root.effectiveRendererTier
        partMechanisms: ((root.themeDefinition.scene3D || ({})).parts || []).map(function(part) {
            return part.mechanism
        })
    }

    PanelSurfaceLoader {
        id: surfaceLoader

        anchors.fill: parent
        visible: !root.segmentedScene
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
        restingAngle: root.layoutAngle
        polygonSides: root.polygonSides
        appearance: root.appearance
        customColor: root.customColor
        panelOpacity: root.panelOpacity
        motionTracks: root.motionTracks
        collapseProgress: root.collapseProgress
        mechanism: root.collapseMechanism
        trackMetrics: root.bakedTrackMetrics || ({})
        true3DCapability: root.true3DCapability
        sceneEditMode: root.sceneEditActive
        onSceneTransformEdited: function(values) { root.sceneTransformEdited(values) }
        sceneQuality: String(root.definitionValue("surface", "parameters3D", "surface3D", ({})).quality
            || root.panelDefinition.scene3DQuality
            || (root.themeDefinition.scene3D || ({})).defaultQuality || "medium")
        cameraPitch: {
            const definition = root.panelDefinition || ({})
            const parameters = (definition.surface || {}).parameters3D || definition.surface3D || ({})
            return Number(definition.scene3DCameraPitch !== undefined
                ? definition.scene3DCameraPitch : parameters.cameraPitch !== undefined
                    ? parameters.cameraPitch : NaN)
        }
        sceneParameters: {
            const definition = root.panelDefinition || ({})
            const parameters = Object.assign({}, (definition.surface || {}).parameters3D || definition.surface3D || ({}))
            for (const [key, parameter] of [["scene3DCameraYaw", "cameraYaw"],
                    ["scene3DThickness", "thickness"], ["scene3DIconElevation", "iconElevation"],
                    ["scene3DRoll", "roll"], ["scene3DPositionX", "positionX"],
                    ["scene3DPositionY", "positionY"], ["scene3DPositionZ", "positionZ"],
                    ["scene3DScale", "scale"], ["scene3DFieldOfView", "fieldOfView"],
                    ["scene3DKeyLight", "keyLightBrightness"], ["scene3DFillLight", "fillLightBrightness"],
                    ["scene3DTransitions", "transitions"], ["scene3DFloat", "float"],
                    ["scene3DBand", "band"], ["scene3DBend", "bend"]])
                if (definition[key] !== undefined) parameters[parameter] = definition[key]
            return parameters
        }
        entryVisuals: root.entryVisuals
        entryGeometry: root.orderedEntries.map(function(entry, index) {
            const output = root.baseEntryGeometryAt(index)
            const rect = output.entryBounds
            return { centerX: rect.x + rect.width / 2, centerY: rect.y + rect.height / 2,
                     width: rect.width, height: rect.height,
                     rotation: output.rotation, onTrack: output.onTrack !== false,
                     visibility: output.trackVisibility === undefined ? 1 : output.trackVisibility }
        })
        sceneConcealed: !root.entriesAnimatable
    }

    Repeater {
        id: segmentRepeater
        model: root.segmentLayout ? root.segmentLayout.segments.map(function(run) { return run.id }) : []
        delegate: PanelSegment {
            required property int index
            readonly property var run: root.segmentLayout.segments[index]
            x: run.x
            y: run.y
            definition: run.definition
            geometry: run.geometry
            layout: root.verticalLayout ? "vertical" : "horizontal"
            layoutAngle: root.layoutAngle
            runtimeState: root.runtimeState
            entryHovered: run.indices.includes(Number(root.runtimeState.hoveredEntry ?? -1))
            motionCatalog: root.motionCatalog
            appearance: root.appearance
            customColor: root.customColor
            panelOpacity: root.panelOpacity
            reducedMotion: root.reducedMotion
            concealed: !root.entriesAnimatable
            visible: root.collapseProgress < 1
            opacity: 1 - root.collapseProgress
        }
    }

    function segmentItemForEntry(index) {
        if (!segmentedScene) return null
        const owner = String((orderedEntries[index] || {}).segmentId || "main")
        for (let i = 0; i < segmentRepeater.count; ++i) {
            const item = segmentRepeater.itemAt(i)
            if (item && item.definition.id === owner) return item
        }
        return null
    }

    // Entries are clipped by the same track that closes the shell, so an icon
    // is never left drawn outside a panel that has already collapsed. While the
    // panel is open the clipper covers the whole scene and does nothing.
    Item {
        id: entryClipper
        z: 1

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

        model: JSON.parse(root.entryIdentityOrder)
        onItemAdded: Qt.callLater(function() { root.entryVisualRevision += 1 })
        onItemRemoved: Qt.callLater(function() { root.entryVisualRevision += 1 })

        delegate: Item {
            id: entryItem

            required property int index
            readonly property var geometryOutput: root.entryGeometryAt(index)
            readonly property var sceneEntry: root.orderedEntries[index] || ({})
            readonly property int sceneIndex: index
            readonly property var segmentItem: root.segmentItemForEntry(index)
            readonly property bool segmentOpen: !root.segmentedScene
                || Boolean(segmentItem && segmentItem.expanded)
            // False for an entry that is off its path, or leaving or coming
            // back onto an open one; such an entry takes no input.
            readonly property bool onTrack: geometryOutput.onTrack !== false
            readonly property real trackVisibility: geometryOutput.trackVisibility === undefined
                ? 1 : Math.max(0, Math.min(1, Number(geometryOutput.trackVisibility)))
            readonly property bool sceneInputEnabled:
                root.entryInteractionEnabled && segmentOpen && onTrack
            readonly property bool sceneVisible:
                root.entriesAnimatable && segmentOpen && onTrack
            readonly property var sceneGeometry: geometryOutput
            readonly property var sceneRuntimeState: root.runtimeState
            readonly property var scenePanelDefinition: root.panelDefinition
            readonly property var sceneHostCapabilities: root.hostCapabilities
            readonly property var sceneIconStyleDefinition:
                root.iconStyleDefinition
            readonly property var sceneContext: root.entryDelegateContext
            readonly property var delegateItem: entryLoader.item
            readonly property var meshVisualItem: delegateItem ? delegateItem.meshVisualItem || null : null
            readonly property var motionController: delegateItem && delegateItem.motionController
                ? delegateItem.motionController : fallbackMotion.item
            readonly property var motionContext: ({ size: width,
                normal: geometryOutput.outwardNormal, tangentAngle: geometryOutput.tangentAngle,
                allowance: geometryOutput.effectAllowance })
            readonly property var motionChannels: motionController ? motionController.channels : ({})
            readonly property var iconMotion: MotionChannels.motionFor(motionChannels, "icon", motionContext)
            readonly property var glyphMotion: MotionChannels.motionFor(motionChannels, "glyph", motionContext)
            readonly property var tileMotion: MotionChannels.motionFor(motionChannels, "tile", motionContext)
            readonly property var indicatorMotion: MotionChannels.motionFor(motionChannels, "indicator", motionContext)
            readonly property real visualScale: delegateItem && delegateItem.hoverScale !== undefined
                ? delegateItem.hoverScale : 1

            objectName: "panel-entry-" + index
            visible: segmentOpen && trackVisibility > 0
            enabled: sceneInputEnabled
            x: geometryOutput.position.x
            y: geometryOutput.position.y
            z: geometryOutput.depthOrder
            width: root.layoutGeometry.iconSize
            height: width
            rotation: geometryOutput.rotation
            transform: Scale {
                origin.x: entryItem.width / 2; origin.y: entryItem.height / 2
                xScale: Number(entryItem.geometryOutput.projectedWidth || entryItem.width) / entryItem.width
                yScale: Number(entryItem.geometryOutput.projectedHeight || entryItem.height) / entryItem.height
            }
            scale: geometryOutput.scaleFactor
            opacity: trackVisibility * (root.entryDelegate === null && sceneEntry.minimized ? 0.55 : 1)

            Loader {
                id: fallbackMotion
                active: entryLoader.item !== null && !entryLoader.item.motionController
                    && root.defaultIconProfiles.length > 0
                readonly property int sceneIndex: entryItem.index
                readonly property var visual: entryLoader.item
                visible: false
                sourceComponent: Component {
                    IconMotionController {
                        profiles: root.defaultIconProfiles
                        catalog: root.motionCatalog
                        entryIndex: parent.sceneIndex
                        reducedMotion: root.reducedMotion
                        sceneVisible: root.entriesAnimatable
                        intensity: Number(root.animationProfiles.animationIntensity
                            || root.panelDefinition.animationIntensity || 1)
                        speed: 170 / Math.max(80, Math.min(1200,
                            Number(root.animationProfiles.animationDuration || root.panelDefinition.animationDuration || 170)
                            / Math.max(0.2, Number(root.animationProfiles.animationSpeed || root.panelDefinition.animationSpeed || 1))))
                        hovered: Boolean(parent.visual && parent.visual.hovered)
                        pressed: Boolean(parent.visual && parent.visual.pressed)
                        running: Boolean(parent.visual && parent.visual.running)
                        urgent: Boolean(parent.visual && parent.visual.urgent)
                        revealed: root.presentationState === "open"
                    }
                }
            }

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
                readonly property bool sceneMeshActive: surfaceLoader.true3DReady
                readonly property string sceneMotionContextKey: root.motionContextKey
                readonly property var sceneGlyphMotion: entryItem.glyphMotion
                readonly property var sceneTileMotion: entryItem.tileMotion
                readonly property var sceneIndicatorMotion: entryItem.indicatorMotion

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
            tileSettings: root.panelDefinition
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
            meshVisualActive: Boolean(parent.sceneMeshActive)
            glyphMotion: parent.sceneGlyphMotion
            tileMotion: parent.sceneTileMotion
            indicatorMotion: parent.sceneIndicatorMotion
        }
    }

    function entryItemAt(index) {
        return entryRepeater.itemAt(index)
    }
}
