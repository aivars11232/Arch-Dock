import QtQuick
import QtQuick.Effects
import QtCore
import org.kde.kirigami as Kirigami
import ArchDock.Decorations 1.0
import "MotionChannels.js" as MotionChannels
import "IconSolid.js" as IconSolid
import "LayoutEngine.js" as LayoutEngine
import "PlatformGeometry.js" as PlatformGeometry
import "StandFrames.js" as StandFrames
import "HologramLook.js" as HologramLook

// One dock icon, drawn in layers: rear effects, the icon style's base and
// tile, the application's glyph, front treatments, the running indicator,
// badge and progress. Its state (hover, pressed, active, running, urgent,
// launching, disabled, drop target, edit mode) selects how each layer is
// drawn, and motion channels move them. Its input rectangle never changes
// size with hover magnification.
Item {
    id: root

    property var entry: ({})
    property var iconStyleDefinition: ({})
    property var tileSettings: ({})
    readonly property bool customTileActive: tileSettings.iconTileMode === "custom"
    // The Pedestals page: a stand under the icon on a flat panel and under
    // an opened folder's items, painted from the 3D scene's own shapes: a
    // horizontal stand seen a little from above, the icon centred over it
    // and floating its Lift above the stand's top, so no part of the stand
    // shows through the icon. Auto is a column, as
    // in 3D, unless the icon style draws its own pedestal; None, the
    // default, draws nothing.
    // On a free panel's path the stand stands on the path at the icon's
    // place and the icon floats above it; in an edge panel's row and a folder opening
    // icon and stand are centred together on the cell.
    property bool pedestalOnPath: false
    // The screen edge an edge panel stands on: its stands stand at that
    // side, their icons away from it (above a bottom panel's, hanging under
    // a top panel's, inward from a side panel's), so nothing reaches past
    // the screen's edge. The stand is drawn in its own frame as on a bottom
    // panel, turned so; the icon is only moved and stays upright.
    property string screenEdge: "bottom"
    readonly property var standDirection: PlatformGeometry.standDirection(screenEdge, pedestalOnPath)
    // A panel's icons stand on the panel itself: each stand on the panel at
    // its icon's place, its middle on the cell's, the icon floating over
    // it. An opened folder's items keep icon and stand centred together on
    // their cell.
    property bool standsOnPanel: false
    // How thick the panel is across its edge (an edge panel's), which its
    // stands fit within: each stands on the panel inside its thickness, as
    // large as it is made or as the panel allows, so a thicker panel has
    // larger stands; the icon floating over it, and its hologram's light,
    // may reach past the panel. 0 keeps the stands as they are made (a free
    // panel's, a folder's).
    property real panelThickness: 0
    readonly property var madeStandLayout: PlatformGeometry.pedestalLayout(tileSettings, logicalSize,
        pedestalOnPath || standsOnPanel)
    readonly property real standFit: panelThickness > 0 && madeStandLayout.drawn
        ? Math.max(0.05, Math.min(1, panelThickness * 0.86 / Math.max(1, madeStandLayout.height))) : 1
    // The settings the stand and its hologram are drawn with: the stand's
    // size scaled to fit.
    readonly property var standSettings: standFit < 0.999
        ? Object.assign({}, tileSettings, { pedestalSize: tileValue("pedestalSize", 1, 0.2, 4) * standFit })
        : tileSettings
    readonly property var pedestalLayout: standFit < 0.999
        ? PlatformGeometry.pedestalLayout(standSettings, logicalSize, pedestalOnPath || standsOnPanel)
        : madeStandLayout
    readonly property string pedestalShape: pedestalLayout.shape
    readonly property string pedestalKind: pedestalLayout.kind === "none" ? "column" : pedestalLayout.kind
    readonly property bool styleDrawsPedestal: Boolean(tileSettings.iconPedestalEnabled)
    // Drawn here on a flat panel only: a 3D scene draws its own pedestals
    // and takes this scene's glyph and tile as textures.
    readonly property bool pedestalDrawn: pedestalLayout.drawn && !meshVisualActive
    readonly property real pedestalSize: tileValue("pedestalSize", 1, 0.2, 4)
    readonly property real pedestalThickness: tileValue("pedestalThickness", 1, 0.2, 4)
    readonly property real pedestalLift: tileValue("pedestalLift", 0.3, 0, 2)
    readonly property real pedestalOpacity: tileValue("pedestalOpacity", 1, 0.1, 1)
    readonly property real pedestalGlow: tileValue("pedestalGlow", 0, 0, 1)
    // Light round the icon's picture and round its tile, each with its own
    // strength, reach and colour. A 3D panel draws
    // these itself, so its flat icons, which are the 3D icons' pictures,
    // draw none.
    readonly property real iconGlow: tileValue("iconGlow", 0, 0, 1)
    readonly property real iconGlowReach: tileValue("iconGlowReach", 0.75, 0, 1)
    readonly property string iconGlowColour: String(tileSettings.iconGlowColor || "").length
        ? String(tileSettings.iconGlowColor) : "#a8dcff"
    readonly property bool iconGlowActive: iconGlow > 0 && !meshVisualActive && !hologramDrawn
    readonly property real tileGlow: tileValue("iconTileGlow", 0, 0, 1)
    readonly property real tileGlowReach: tileValue("iconTileGlowReach", 0.75, 0, 1)
    readonly property string tileGlowColour: String(tileSettings.iconTileGlowColor || "").length
        ? String(tileSettings.iconTileGlowColor)
        : String(tileSettings.iconTileBorderColor || "#94a3b8")
    readonly property bool tileGlowActive: tileGlow > 0 && tileRenderingEnabled && !meshVisualActive
    readonly property string pedestalMaterial: String(tileSettings.pedestalMaterial || "panel").toLowerCase()
    readonly property string pedestalTexture: String(tileSettings.pedestalTexture || "material").toLowerCase()
    readonly property string pedestalColor: String(tileSettings.pedestalColor || "")
    readonly property string pedestalAnimation: String(tileSettings.pedestalAnimation || "none").toLowerCase()
    readonly property real pedestalAnimationSpeed: tileValue("pedestalAnimationSpeed", 1, 0.1, 5)
    // Holograms (Icons > Hologram, Pedestals > Hologram): the icon, and its
    // stand, drawn as light; with the projector light on, the icon floats
    // over its light's opening in the middle of its stand's top, or on the
    // panel where it has none. A 3D panel draws its own.
    readonly property var hologram: HologramLook.settings(tileSettings, "Icon")
    readonly property bool hologramDrawn: hologram.on && !meshVisualActive
    readonly property var hologramColours: HologramLook.colours(tileSettings, "Icon")
    readonly property var hologramLayout: hologramDrawn
        ? PlatformGeometry.hologramLayout(standSettings, logicalSize, pedestalOnPath, screenEdge) : ({ on: false })
    readonly property bool hologramLight: hologramDrawn && hologramLayout.on === true
    readonly property var standHologram: HologramLook.settings(tileSettings, "Pedestal")
    readonly property bool standHologramDrawn: standHologram.on && pedestalDrawn
    readonly property var standHologramColours: HologramLook.colours(tileSettings, "Pedestal")
    // Each icon's own phase, so a panel's holograms do not blink together.
    readonly property real hologramSeed: (Math.abs(String(iconSource).split("").reduce(function(sum, letter) {
        return (sum * 31 + letter.charCodeAt(0)) % 9973
    }, 7)) % 97) / 9.7
    // The holograms' time, from the one clock they all share; still, and
    // without flicker, under reduced motion and while the icon is not seen.
    readonly property bool hologramMoving: (hologramDrawn || standHologramDrawn) && visible && !reducedMotion
    readonly property real hologramTime: hologramMoving ? HologramClock.time * hologram.speed : 0
    readonly property real standHologramTime: hologramMoving ? HologramClock.time * standHologram.speed : 0
    readonly property bool hologramLightMoving: hologramMoving
        && HologramLook.beamMotionFlags(hologram.beamMotion).moving
    // How far the hologram floats up and down where Float is on.
    readonly property real hologramFloating: hologramDrawn && hologram.float
        ? Math.sin(hologramTime * 1.3 + hologramSeed) * logicalSize * 0.025 : 0
    property bool holdingHologramClock: false
    function holdHologramClock(wanted) {
        if (wanted === holdingHologramClock)
            return
        holdingHologramClock = wanted
        if (wanted)
            HologramClock.hold()
        else
            HologramClock.release()
    }
    onHologramMovingChanged: holdHologramClock(hologramMoving)
    Component.onCompleted: holdHologramClock(hologramMoving)
    Component.onDestruction: holdHologramClock(false)
    // How far the icon lies from its cell's middle toward its stand's
    // direction, and where the stand's top edge is in its frame (the icon
    // toward -y): the Lift gap, or a hologram's light, between them.
    readonly property real iconRaise: hologramLight ? hologramLayout.raise : pedestalDrawn ? pedestalLayout.raise : 0
    readonly property real standTop: hologramLight ? hologramLayout.standTop
        : PlatformGeometry.standTopOffset(pedestalLayout, logicalSize)
    // The stand's drawn bottom edge below this item's top, in its own
    // pixels, for whatever stands under the stand (a folder child's name).
    readonly property real standDrawnBottom: pedestalDrawn
        ? root.mapFromItem(pedestalLayer, 0, pedestalLayer.height).y : height
    // The icon at rest in this scene's coordinates: in its cell, or away
    // from it over its stand or in its hologram's light - not following
    // its motion or magnification - where the pointer finds it.
    readonly property rect restingIconRect: Qt.rect(standDirection.x * iconRaise, standDirection.y * iconRaise,
                                                    logicalSize, logicalSize)
    readonly property bool pedestalMotionActive: pedestalDrawn && pedestalAnimation !== "none"
        && !reducedMotion && visible
    property real pedestalPhase: 0
    NumberAnimation on pedestalPhase {
        running: root.pedestalMotionActive
        loops: Animation.Infinite
        from: 0
        to: 1
        duration: Math.round(4000 / Math.max(0.1, root.pedestalAnimationSpeed))
    }
    property real logicalSize: 52
    property real visualScale: 1
    property string tileShape: "style-default"
    property string appearance: "glass"
    // The panel wears a decoration: its icons stand on it bare, with no
    // tile of the look's under them (one chosen for the icons still shows).
    property bool panelDecorated: false
    property bool showReflection: false
    property bool showIndicator: true
    property bool vertical: false
    property bool hovered: false
    property bool pressed: false
    property bool active: Boolean(entry && entry.active)
    property bool running: Boolean(entry && entry.running)
    property bool minimized: Boolean(entry && entry.minimized)
    property bool urgent: Boolean(entry && (entry.attention || entry.urgent))
    property bool launching: Boolean(entry && entry.launching)
    property bool disabled: Boolean(entry && entry.disabled)
    property bool dropTarget: false
    property bool editMode: false
    property int windowCount: Math.max(1, Number(
        entry && entry.windowCount || 1))
    property string iconSource: String(
        entry && entry.iconName || "application-x-executable")
    property string badgeText: String(
        entry && (entry.badgeText || entry.badge) || "")
    property real progress: entry && entry.progress !== undefined
        ? Number(entry.progress) : -1
    property var indicatorStyle: ({})
    property bool reducedMotion: false
    property real glowAmount: 0
    property bool glowAnimating: false
    property int glowDuration: 170
    // Each consumer owns its own texture sources. Input, status overlays and
    // accessibility remain on the original entry, outside the mesh scene.
    property bool meshVisualActive: false
    readonly property var meshVisualItem: root
    readonly property alias glyphTextureItem: glyphTexture
    readonly property alias tileTextureItem: tileTexture

    // Per-layer motion, as resolved by MotionChannels. The scene applies these
    // to visual transforms only; `logicalInputRegion` never follows them, so a
    // moving icon keeps a stationary pointer target.
    property var glyphMotion: ({})
    property var tileMotion: ({})
    property var indicatorMotion: ({})
    readonly property var resolvedGlyphMotion: normalizedMotion(meshVisualActive ? null : glyphMotion)
    readonly property var resolvedTileMotion: normalizedMotion(meshVisualActive ? null : tileMotion)
    readonly property var resolvedIndicatorMotion:
        normalizedMotion(indicatorMotion)
    // A flat-card turn is a Y-axis rotation, not the flat Z spin: the card
    // narrows towards its centre and one edge recedes under perspective.
    readonly property real glyphTurnAngle: Number(resolvedGlyphMotion.rotateY)
    readonly property bool glyphTurnActive: Math.abs(glyphTurnAngle) > 0.01

    readonly property var resolvedIconStyle: IconStyleResolver.resolve(
        iconStyleDefinition,
        {
            edit: editMode,
            disabled: disabled,
            drop: dropTarget,
            urgent: urgent,
            pressed: pressed,
            hover: hovered,
            launching: launching,
            active: active,
            minimized: minimized,
            running: running
        },
        resolverEntry(), styleSettings(), logicalSize)
    readonly property string visualState: resolvedIconStyle.stateId
    readonly property var styleState: resolvedIconStyle.state || ({})
    readonly property var styleInset:
        resolvedIconStyle.safeGlyphInset || ({
            left: 0, top: 0, right: 0, bottom: 0
        })
    // Any style asset that fails at load time collapses the whole treatment
    // to the safe original glyph rather than leaving a half-drawn icon.
    readonly property bool styleAssetsFailed:
        styleShadow.assetFailed || styleGlow.assetFailed
        || styleRear.assetFailed || styleBase.assetFailed
        || styleReflection.assetFailed || styleFront.assetFailed
        || styleMaskFailed
    // Resolve identity independently of fallback: clearing the Image source
    // on its own Error both creates a binding loop and loses that identity.
    readonly property string currentStyleMaskSource:
        resolvedIconStyle.maskLayer
        ? String(resolvedIconStyle.maskLayer.source) : ""
    property var failedStyleMaskSources: ({})
    readonly property bool styleMaskFailed:
        currentStyleMaskSource.length > 0
        && failedStyleMaskSources[currentStyleMaskSource] === true

    readonly property bool styledLayersActive:
        Boolean(resolvedIconStyle.renderStyledLayers) && !styleAssetsFailed
    readonly property bool styleGlyphTreatmentActive:
        Boolean(resolvedIconStyle.valid)
        && (Boolean(resolvedIconStyle.parametersActive)
            || String(resolvedIconStyle.styleId || "plain-original") !== "plain-original")
        && !styleAssetsFailed

    // Effective glyph treatment, already compatibility-gated by the resolver.
    readonly property string glyphTreatment: styleAssetsFailed ? "original"
        : String(resolvedIconStyle.glyphTreatment || "original")
    readonly property string glyphTint:
        String(resolvedIconStyle.glyphTint || "")
    readonly property bool glyphIsMask:
        !styleAssetsFailed && Boolean(resolvedIconStyle.glyphIsMask)
        && glyphTint.length > 0
    readonly property var styleMaskLayer:
        styleAssetsFailed ? null : (resolvedIconStyle.maskLayer || null)
    readonly property bool styleMaskActive:
        styledLayersActive && styleMaskLayer !== null
    readonly property bool tileRenderingEnabled:
        resolvedIconStyle.tileEnabled === undefined
        ? true : Boolean(resolvedIconStyle.tileEnabled)
    // Whether the icon style draws a tile of its own (its base layers).
    // Without one, a shown tile is the panel's own (Icon Tiles: its fill,
    // border and opacity): every icon shown with tiles stands in one.
    readonly property bool styleDrawsTile: styledLayersActive && styleBase.layerCount > 0
    readonly property bool panelTileDrawn: tileRenderingEnabled && (customTileActive || !styleDrawsTile)
    // How wide a tile has to be to hold the icon whole, as a share of its
    // cell: the picture at its size on the tile, moved by the tile's icon
    // offsets, and as far as its own motion carries it past the tile's - a
    // picture that swells more than its tile, moves or spins on it - with a
    // margin for the outline, as a 3D panel's bubble holds its icon
    // (PlatformGeometry.containingScale()). `atRest` leaves the tile options
    // and the motion out: the picture as the icon style lays it out.
    function tileHoldingShare(atRest) {
        const glyphMotion = resolvedGlyphMotion, tileMotion = resolvedTileMotion
        const swell = atRest ? 1 : Math.max(1, glyphMotion.scale * Math.max(glyphMotion.scaleX, glyphMotion.scaleY)
            / Math.max(1e-6, tileMotion.scale * Math.min(tileMotion.scaleX, tileMotion.scaleY)))
        const turn = atRest ? 0 : (glyphMotion.rotateZ - tileMotion.rotateZ) * Math.PI / 180
        const spin = Math.abs(Math.cos(turn)) + Math.abs(Math.sin(turn))
        const onTile = atRest || !tileRenderingEnabled ? 1 : tileValue("iconTileIconScale", 100, 25, 150) / 100
        const stateScale = styleState.glyphScale === undefined ? 1 : Number(styleState.glyphScale)
        const picture = Math.max(glyph.width, glyph.height) * stateScale * onTile
        const cell = Math.max(1, logicalSize)
        const shiftX = atRest ? 0 : tileValue("iconTileIconOffsetX", 0, -40, 40) + glyphMotion.x - tileMotion.x
        const shiftY = atRest ? 0 : tileValue("iconTileIconOffsetY", 0, -40, 40) + glyphMotion.y - tileMotion.y
        return 1.08 * PlatformGeometry.containingScale(resolvedIconStyle.iconShape || tileShape,
            picture / cell * swell * spin, 2 * shiftX / cell, 2 * shiftY / cell)
    }
    readonly property real tileHolding: tileHoldingShare(false)
    // How far an icon style's own tile grows to hold a picture that
    // outgrew it.
    readonly property real styleTileGrowth: Math.max(1, tileHolding / Math.max(1e-6, tileHoldingShare(true)))
    readonly property var effectiveIconStyleDefinition:
        resolvedIconStyle.styleDefinition || iconStyleDefinition
    readonly property string resolvedIconSource: String(
        resolvedIconStyle.glyphSource || iconSource)
    readonly property var resolvedIndicatorStyle:
        IconStyleResolver.indicatorStyle(
            indicatorStyle, resolvedIconStyle)
    readonly property var logicalInputRegion: ({
        x: 0,
        y: 0,
        width: logicalSize,
        height: logicalSize
    })
    readonly property bool badgeSlotActive: badgeText.length > 0
    readonly property bool progressSlotActive: isFinite(progress)
        && progress >= 0 && progress <= 1
    readonly property alias visualLayerItem: visualLayer
    readonly property alias rearLayerItem: rearLayer
    readonly property alias baseLayerItem: baseLayer
    readonly property alias glyphLayerItem: glyphLayer
    readonly property alias glyphItem: glyph
    readonly property alias frontLayerItem: frontLayer
    readonly property alias indicatorItem: runningIndicator
    readonly property alias tileTransformItem: baseLayer
    readonly property alias glyphTransformItem: glyphLayer
    readonly property alias indicatorTransformItem: runningIndicator
    readonly property alias styleRearItem: styleRear
    readonly property alias styleBaseItem: styleBase
    readonly property alias styleFrontItem: styleFront
    readonly property alias statusLayerItem: statusLayer
    readonly property alias badgeItem: badge
    readonly property alias progressItem: progressTrack
    readonly property alias attentionItem: attentionMarker
    readonly property alias temporaryStatusItem: temporaryNotice
    readonly property alias statusTextItem: statusLabel


    function recordStyleMaskFailure(source) {
        if (source.length === 0)
            return
        const next = ({})
        const keys = Object.keys(failedStyleMaskSources)
        for (let index = 0; index < keys.length; ++index)
            next[keys[index]] = true
        next[source] = true
        failedStyleMaskSources = next
    }

    function alphaColor(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    function styleSettings() {
        const values = IconStyleResolver.copyMap(tileSettings)
        if (values.iconShape === undefined)
            values.iconShape = tileShape
        return values
    }

    function tileValue(key, fallback, minimum, maximum) {
        const value = Number(tileSettings[key] === undefined ? fallback : tileSettings[key])
        return Math.max(minimum, Math.min(maximum, isFinite(value) ? value : fallback))
    }

    function tileRadius() {
        if (tileShape === "circle")
            return logicalSize / 2
        if (tileShape === "square")
            return 3
        if (tileShape === "squircle")
            return logicalSize * 0.32
        return logicalSize * 0.22
    }

    function normalizedMotion(value) {
        const resting = MotionChannels.restingMotion()
        const source = value || ({})
        const keys = Object.keys(source)
        for (let index = 0; index < keys.length; ++index) {
            const key = keys[index]
            if (resting.hasOwnProperty(key) && source[key] !== undefined
                    && source[key] !== null)
                resting[key] = source[key]
        }
        return resting
    }

    function resolverEntry() {
        const source = entry || ({})
        const result = ({})
        const keys = Object.keys(source)
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = source[keys[index]]
        result.iconSource = iconSource
        return result
    }

    width: logicalSize
    height: logicalSize

    // The icon as a solid for a 3D panel (Icon depth). Made into an object
    // ("modelled", IconModel: the icon helper's object, once made, its front
    // seen from either side), laid where the picture lies in the glyph
    // layer; till it is made, and where objects cannot be made, the glyph
    // layer's picture, taken small, gives the solid its outline from its own
    // transparency (IconSolid.js), the scene scaling its depth. Taken anew
    // when the icon changes.
    property real solidDepth: 0
    // "modelled": the icon made into an object; "rounded": the picture's
    // parts inflated into rounded volumes; "block": the picture cut out as
    // a block.
    property string solidRelief: "modelled"
    // How far the scene stretches the solid's depth to show it at Icon
    // depth (IconSolid.depthStretch).
    readonly property real solidStretch: IconSolid.depthStretch(solidMesh, solidDepth)
    // Inside a 3D panel's bubble, where every icon shown with a tile stands,
    // a round picture keeps a flat back instead of rising as a ball: no
    // bubble in the bubble.
    readonly property bool bubbleTile: meshVisualActive && tileRenderingEnabled
    onBubbleTileChanged: if (solidWanted) refreshSolid()
    // The same solid on a flat panel (Icons > 3D): drawn in a small 3D view
    // of its own in the picture's place, turned by Icon turn and lit as
    // Icon lighting says; a hologram icon's solid made of light, a 3D
    // hologram, not its flat picture made light. Without Qt Quick 3D the
    // flat picture stays.
    property bool solidOnFlat: false
    // A line drawing (a widget's status icon) drawn with a body: its strokes
    // thicker (shaders/symbol-body.frag), so 3D icons give it depth and a
    // hologram's lines show across it, at twice the screen's resolution.
    property bool boldSymbol: false
    property real solidTurn: 15
    property real solidLighting: 0.5
    readonly property bool flatSolidWanted: solidOnFlat && !meshVisualActive && solidDepth > 0
    readonly property bool flatSolidShown: flatSolidWanted && solidMesh !== null && solidPictureUrl.length > 0
        && flatSolidLoader.status === Loader.Ready
    // The picture a flat panel's solid wears: a copy of the icon's picture
    // saved among the session's passing files (one per picture, written
    // over) and loaded by the 3D view as an ordinary picture. Qt Quick 3D
    // refreshes a picture taken from a live layer every frame, even after
    // the layer's item is gone, and crashed doing so.
    property string solidPictureUrl: ""
    function solidPictureName(text) {
        let hash = 2166136261
        for (let index = 0; index < text.length; ++index) {
            hash ^= text.charCodeAt(index)
            hash = Math.imul(hash, 16777619) >>> 0
        }
        return "archdock-icon-solid-" + hash.toString(16) + ".png"
    }
    function refreshSolidPicture() {
        if (!flatSolidWanted) {
            solidPictureUrl = ""
            return
        }
        const pixels = Math.max(64, Math.min(512, Math.ceil(logicalSize * Math.max(1, Screen.devicePixelRatio) * 2)))
        const folder = String(StandardPaths.writableLocation(StandardPaths.RuntimeLocation)).replace(/^file:\/\//, "")
        const path = folder + "/" + solidPictureName([resolvedIconSource, pixels, glyphTint, glyphIsMask].join("|"))
        grabGlyph(function(result) {
            if (result.saveToFile(path))
                root.solidPictureUrl = "file://" + path
        }, Qt.size(pixels, pixels))
    }
    // A picture is taken only while the icon's window is shown: an icon moved
    // into a window not shown yet (an edge panel's dock moving into its window
    // over the panel as holograms are switched on) takes its pictures again
    // once that window shows, or it would keep none.
    property bool glyphGrabWaiting: false
    function grabGlyph(done, size) {
        if (!glyphLayer.grabToImage(done, size))
            glyphGrabWaiting = true
    }
    function takeWaitingGrab() {
        const window = Window.window
        if (!glyphGrabWaiting || !window || !window.visible)
            return
        glyphGrabWaiting = false
        refreshSolid()
    }
    Window.onWindowChanged: takeWaitingGrab()
    Connections {
        target: root.Window.window
        function onVisibleChanged() { root.takeWaitingGrab() }
    }
    readonly property bool solidWanted: (meshVisualActive || flatSolidWanted) && solidDepth > 0
    property var solidMesh: null
    readonly property bool solidModelled: solidRelief === "modelled" && solidModel.ready
    function refreshSolid() {
        if (!solidWanted) {
            solidMesh = null
            return
        }
        if (solidModelled)
            solidMesh = solidModel
        else
            grabGlyph(function(result) { solidCanvas.take(result.url) },
                                   Qt.size(solidCanvas.width, solidCanvas.height))
        refreshSolidPicture()
    }
    // Where the icon's own picture lies in the glyph layer the 3D scenes
    // draw from (0 to 1 across and down): the object is laid there.
    readonly property rect solidPictureFrame: glyphLayer.width > 0 && glyphLayer.height > 0
        ? Qt.rect((glyph.x + glyph.width * (1 - glyph.scale) / 2) / glyphLayer.width,
                  (glyph.y + glyph.height * (1 - glyph.scale) / 2) / glyphLayer.height,
                  glyph.width * glyph.scale / glyphLayer.width, glyph.height * glyph.scale / glyphLayer.height)
        : Qt.rect(0.14, 0.14, 0.72, 0.72)
    IconModel {
        id: solidModel
        iconSource: root.solidWanted && root.solidRelief === "modelled" ? root.resolvedIconSource : ""
        frame: root.solidPictureFrame
        onModelled: if (root.solidWanted) root.refreshSolid()
    }
    onSolidWantedChanged: refreshSolid()
    onResolvedIconSourceChanged: if (solidWanted) refreshSolid()
    onSolidReliefChanged: if (solidWanted) refreshSolid()
    // The picture may arrive after the solid was asked for, and the layer
    // may still be unsized then: cut the solid again once both are there.
    Connections {
        target: glyph
        function onStatusChanged() {
            if (root.solidWanted && glyph.status === Kirigami.Icon.Ready) root.refreshSolid()
        }
    }
    Connections {
        target: glyphLayer
        function onWidthChanged() { if (root.solidWanted && root.solidMesh === null) root.refreshSolid() }
        function onHeightChanged() { if (root.solidWanted && root.solidMesh === null) root.refreshSolid() }
    }
    Canvas {
        id: solidCanvas
        visible: false
        width: 64
        height: 64
        renderTarget: Canvas.Image
        renderStrategy: Canvas.Immediate
        property string pending: ""
        function take(url) {
            pending = url
            if (!isImageLoaded(url)) loadImage(url)
            // A grabbed picture is already in the engine's cache, so it
            // loads at once and no loaded signal follows.
            if (isImageLoaded(url)) requestPaint()
        }
        onImageLoaded: if (pending.length && isImageLoaded(pending)) requestPaint()
        onPaint: {
            if (!pending.length || !isImageLoaded(pending)) return
            const context = getContext("2d")
            context.reset()
            context.clearRect(0, 0, width, height)
            context.drawImage(pending, 0, 0, width, height)
            const data = context.getImageData(0, 0, width, height)
            const mesh = IconSolid.iconMesh(data.data, width, height, root.solidRelief, { globe: !root.bubbleTile })
            // An object made meanwhile comes first.
            root.solidMesh = root.solidModelled ? solidModel : mesh.indexes.length ? mesh : null
            unloadImage(pending)
            pending = ""
        }
    }

    ShaderEffectSource {
        id: glyphTexture
        readonly property bool taken: root.meshVisualActive || root.flatSolidShown
        sourceItem: taken ? glyphLayer : null
        hideSource: taken
        // Only a 3D panel draws from it; on a flat panel it only hides the
        // flat picture while the solid is shown.
        live: root.meshVisualActive
        visible: false
        // Four times the icon's size: a glyph seen large on the near side
        // of a tilted platform stays crisp.
        textureSize: Qt.size(Math.min(1024, Math.max(1, root.logicalSize * 4)),
                             Math.min(1024, Math.max(1, root.logicalSize * 4)))
    }
    ShaderEffectSource {
        id: tileTexture
        sourceItem: root.meshVisualActive ? baseLayer : null
        hideSource: root.meshVisualActive
        live: root.meshVisualActive
        visible: false
        textureSize: glyphTexture.textureSize
    }

    // The stand's frame: the stand, its hologram light's opening and the
    // light itself, laid out as on a bottom panel (the icon toward -y) and
    // turned away from the screen edge the panel stands on.
    Item {
        id: standFrame

        objectName: "icon-stand-frame"
        anchors.fill: parent
        rotation: root.standDirection.turn

        // The stand under the icon: the shape's own solid, projected as the 3D
        // scene shows it, each face shaded by its angle to the light, in the
        // material's texture and the stand's colour, with an optional glow
        // round it. Its own motion moves it, not the icon.
        Item {
            id: pedestalLayer
            objectName: "icon-layer-pedestal"
            visible: root.pedestalDrawn
            readonly property real beat: root.pedestalMotionActive
                ? Math.sin(root.pedestalPhase * 2 * Math.PI) : 0
            // The glow is a halo of light round the stand: Glow intensity sets how
            // strong it is, Glow reach how far it spreads. It is the same
            // GlowEffect the icons and tiles use, blurred on the graphics card.
            // A Canvas shadow blur runs on the main thread instead, and for the
            // stands of one open folder it took several seconds. The halo gets
            // its room from the layer's own padding, so the canvas keeps only a
            // small margin for its edges.
            readonly property real glowStrength: root.pedestalAnimation === "glow"
                ? Math.max(0.5, root.pedestalGlow) : root.pedestalGlow
            readonly property real glowReach: root.tileValue("pedestalGlowReach", 0.75, 0, 1)
            readonly property real margin: 2
            readonly property real standWidth: root.pedestalLayout.width
            readonly property real standHeight: root.pedestalLayout.height
            width: standWidth + 2 * margin
            height: standHeight + 2 * margin
            x: (standFrame.width - width) / 2
            // Its top edge at its place in the frame: the Lift gap, or a
            // hologram's light, under the icon.
            y: standFrame.height / 2 + root.standTop - margin + bob
            readonly property real bob: root.pedestalAnimation === "bob" ? beat * root.logicalSize * 0.04
                : root.pedestalAnimation === "float" ? beat * root.logicalSize * 0.03 : 0
            opacity: root.pedestalOpacity
            scale: root.pedestalAnimation === "pulse" ? 1 + beat * 0.06 : 1
            rotation: root.pedestalAnimation === "sway" ? beat * 8 : 0
            transformOrigin: Item.Bottom
            // A picture the user imported for the stands.
            readonly property string ownPicture: String(root.tileSettings.pedestalPicture || "")
            // The panel's look, or the stand's own; Picture without a picture
            // is the panel's look. A stand made for a hologram (a projector)
            // wears its own finish in the panel's place.
            readonly property bool panelLook: root.pedestalMaterial === "panel"
                || (root.pedestalMaterial === "picture" && !ownPicture.length)
            readonly property var finish: panelLook ? PlatformGeometry.pedestalFinish(root.pedestalKind) : null
            readonly property string look: finish ? finish.look : panelLook ? root.appearance : root.pedestalMaterial
            // The picture worn: the stand's own, or in the panel's look the
            // panel's picture.
            readonly property string picture: root.pedestalTexture === "picture" || !panelLook || finish ? ownPicture
                : String(root.tileSettings.panelPicture || "")
            // The Picture texture without a picture falls back to the look.
            readonly property string textureLook: root.pedestalTexture === "material"
                || (root.pedestalTexture === "picture" && !ownPicture.length) ? look : root.pedestalTexture
            // A glowing look in the stands' palette.
            readonly property string lookPalette: String(root.tileSettings.pedestalLookColour || "")
            readonly property url textureSource: textureLook === "none" ? ""
                : textureLook === "picture" ? picture
                : Qt.resolvedUrl("materials/" + LayoutEngine.lookTexture(textureLook, lookPalette) + ".png")
            readonly property string tint: root.pedestalColor.length ? root.pedestalColor
                : finish && finish.colour.length ? finish.colour
                : String(LayoutEngine.themeStyle(look, "", 48).stroke || "#b5d3e0")
            // The glow's own colour, or the stand's.
            readonly property string glowTint: String(root.tileSettings.pedestalGlowColor || "").length
                ? String(root.tileSettings.pedestalGlowColor) : tint
            readonly property real spin: !root.pedestalMotionActive ? 0
                : root.pedestalAnimation === "spin" ? root.pedestalPhase * 2 * Math.PI
                : root.pedestalAnimation === "float" ? root.pedestalPhase * Math.PI / 2 : 0
            readonly property real wobble: root.pedestalAnimation === "wobble" ? beat * 10 : 0
            // The motion as drawn: 120 steps a turn and half-degree steps of a
            // wobble. A stand is painted again only when its step changes, and
            // each step is painted once for every stand drawn the same way
            // (StandFrames.js).
            readonly property int spinStep: Math.round(spin / (2 * Math.PI) * 120) % 120
            readonly property int wobbleStep: Math.round(wobble * 2)
            readonly property real drawnSpin: spinStep * 2 * Math.PI / 120
            readonly property real drawnWobble: wobbleStep / 2
            readonly property var mesh: root.pedestalDrawn ? PlatformGeometry.pedestalShape(root.pedestalKind, 16) : null
            // Everything a repaint depends on, in one value.
            readonly property string paintKey: [root.pedestalKind, root.pedestalLayout.unit, root.pedestalLayout.depth,
                root.pedestalLayout.tilt, tint, textureSource, textureLook, root.pedestalColor, width, height,
                lightningBackground, visible].join("|")

            // The stand's silhouette and faces, seen from `tilt` degrees above,
            // turned `spin` about its axis: canvas points, nearest faces last.
            function project(tilt, spinAngle) {
                const layout = root.pedestalLayout
                const unit = layout.unit
                const stretch = layout.depth / Math.max(1e-6, unit)
                const t = tilt * Math.PI / 180
                const ct = Math.cos(t), st = Math.sin(t)
                const cs = Math.cos(spinAngle), sn = Math.sin(spinAngle)
                const cx = width / 2
                const baseY = margin + layout.outline.top * unit
                const positions = mesh ? mesh.positions : []
                const points = [], turned = []
                for (const p of positions) {
                    const x = p[0] * cs - p[1] * sn, y = p[0] * sn + p[1] * cs, z = p[2] * stretch
                    turned.push([x, y, z])
                    points.push([cx + x * unit, baseY - (z * ct + y * st) * unit])
                }
                const light = [-0.42, -0.5, 0.76]
                const faces = []
                const indexes = mesh ? mesh.indexes : []
                for (let i = 0; i < indexes.length; i += 3) {
                    const a = turned[indexes[i]], b = turned[indexes[i + 1]], c = turned[indexes[i + 2]]
                    const u = [b[0] - a[0], b[1] - a[1], b[2] - a[2]]
                    const w = [c[0] - a[0], c[1] - a[1], c[2] - a[2]]
                    const n = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
                    const length = Math.hypot(n[0], n[1], n[2])
                    if (length < 1e-12) continue
                    // Facing the viewer, who looks from the front (-y) and above.
                    if (n[1] * -ct + n[2] * st <= 0) continue
                    const lambert = Math.max(0, (n[0] * light[0] + n[1] * light[1] + n[2] * light[2]) / length)
                    const depth = (-(a[1] + b[1] + c[1]) * ct + (a[2] + b[2] + c[2]) * st) / 3
                    faces.push({ corners: [points[indexes[i]], points[indexes[i + 1]], points[indexes[i + 2]]],
                                 shade: 0.42 + 0.58 * lambert, shine: Math.max(0, lambert - 0.82) * 2.2, depth: depth })
                }
                faces.sort(function(first, second) { return first.depth - second.depth })
                return faces
            }
            // A lightning stand: its faces in the dark storm, as strong as its
            // Lightning background asks (half by default; none shows only the
            // bolts), the bolts over them as light, tinted by the
            // stand's colour.
            readonly property real lightningBackground: root.tileValue("pedestalLookBackground", 0.5, 0, 1)
            // A glowing look is light, the way the 3D scene draws it: the faces
            // in the stand's own colour, the look's texture laid over them as
            // more light. Multiplied in like a paint, its dark texture turned a
            // coloured stand nearly black.
            readonly property bool glowingLook: ["plasma", "neon", "futuristic"].includes(textureLook)
            function paintLight(context, faces, textureLoaded, width, height) {
                const colour = Qt.color(tint);
                context.lineWidth = 0.8;
                context.lineJoin = "round";
                for (const face of faces) {
                    const shade = Math.min(1, face.shade);
                    const fill = "rgb(" + Math.round(255 * colour.r * shade) + "," + Math.round(255 * colour.g * shade)
                        + "," + Math.round(255 * colour.b * shade) + ")";
                    context.fillStyle = fill;
                    context.strokeStyle = fill;
                    context.beginPath();
                    context.moveTo(face.corners[0][0], face.corners[0][1]);
                    context.lineTo(face.corners[1][0], face.corners[1][1]);
                    context.lineTo(face.corners[2][0], face.corners[2][1]);
                    context.closePath();
                    context.fill();
                    context.stroke();
                }
                if (!textureLoaded) return;
                context.save();
                trace(context, faces);
                context.clip();
                context.globalCompositeOperation = "qt-screen";
                fillTexture(context, width, height);
                context.restore();
            }
            function paintLightning(context, faces, textureLoaded, width, height) {
                context.lineWidth = 0.8
                context.lineJoin = "round"
                const sky = String(LayoutEngine.themeStyle(textureLook, "", 48).skyColour || "12, 10, 31")
                    .split(",").map(Number)
                for (const face of faces) {
                    const shade = Math.min(1, face.shade)
                    const colour = "rgba(" + Math.round(sky[0] * shade) + "," + Math.round(sky[1] * shade) + ","
                        + Math.round(sky[2] * shade) + "," + lightningBackground + ")"
                    context.fillStyle = colour
                    context.strokeStyle = colour
                    context.beginPath()
                    context.moveTo(face.corners[0][0], face.corners[0][1])
                    context.lineTo(face.corners[1][0], face.corners[1][1])
                    context.lineTo(face.corners[2][0], face.corners[2][1])
                    context.closePath()
                    context.fill()
                    context.stroke()
                }
                if (!textureLoaded) return
                context.save()
                trace(context, faces)
                context.clip()
                fillTexture(context, width, height)
                context.restore()
                // A picture is worn in its own colours; a look takes the tint.
                if (root.pedestalColor.length && textureLook !== "picture") {
                    context.save()
                    context.globalCompositeOperation = "source-atop"
                    context.globalAlpha = 0.5
                    context.fillStyle = tint
                    context.fillRect(0, 0, width, height)
                    context.restore()
                }
            }
            // The look's texture over the whole canvas, turned with the stand,
            // at the size it has on a stand without a glow. Only the canvas is
            // filled, not the texture's whole square.
            function fillTexture(context, width, height) {
                context.translate(width / 2, height / 2)
                context.rotate(drawnSpin)
                if (textureLook === "picture") {
                    // The picture once, centred, covering the stand however far
                    // it turns.
                    const pictureWidth = standPicture.implicitWidth
                    const pictureHeight = standPicture.implicitHeight
                    if (pictureWidth > 0 && pictureHeight > 0) {
                        const cover = Math.hypot(width, height) / Math.min(pictureWidth, pictureHeight)
                        context.drawImage(String(textureSource), -pictureWidth * cover / 2, -pictureHeight * cover / 2,
                                          pictureWidth * cover, pictureHeight * cover)
                    }
                    return
                }
                const s = Math.max(width, height) / 512 * 1.1
                context.scale(s, s)
                const reach = Math.ceil(Math.hypot(width, height) / 2 / s) + 2
                context.fillStyle = context.createPattern(String(textureSource), "repeat")
                context.fillRect(-reach, -reach, 2 * reach, 2 * reach)
            }
            function trace(context, faces) {
                context.beginPath()
                for (const face of faces) {
                    context.moveTo(face.corners[0][0], face.corners[0][1])
                    context.lineTo(face.corners[1][0], face.corners[1][1])
                    context.lineTo(face.corners[2][0], face.corners[2][1])
                    context.closePath()
                }
            }

            // The stand picture's own size, for fitting it.
            Image {
                id: standPicture
                visible: false
                asynchronous: true
                source: pedestalLayer.textureLook === "picture" ? pedestalLayer.picture : ""
                onStatusChanged: if (status === Image.Ready) pedestalCanvas.requestPaint()
            }

            Canvas {
                id: pedestalCanvas
                objectName: "icon-pedestal-canvas"
                anchors.fill: parent
                // The glow pulses with the Glow motion. A holographic stand
                // (Pedestals > Hologram) is drawn as light instead, its own
                // glow round it.
                layer.enabled: pedestalLayer.glowStrength > 0 || root.standHologramDrawn
                layer.effect: root.standHologramDrawn ? standHologramEffect : standGlowEffect
                Component {
                    id: standGlowEffect

                    GlowEffect {
                        glowColor: pedestalLayer.glowTint
                        intensity: pedestalLayer.glowStrength
                            * (root.pedestalAnimation === "glow" ? 0.55 + 0.45 * pedestalLayer.beat : 1)
                        reach: pedestalLayer.glowReach
                    }
                }
                Component {
                    id: standHologramEffect

                    HologramEffect {
                        colours: root.standHologramColours
                        time: root.standHologramTime
                        flicker: root.hologramMoving ? root.standHologram.flicker : 0
                        glow: root.standHologram.glow
                        glowReach: root.standHologram.glowReach
                        lineSpacing: Math.max(1.5, root.logicalSize / 26)
                        seed: root.hologramSeed + 1.7
                    }
                }
                property bool textureLoaded: false
                function loadTexture() {
                    const source = String(pedestalLayer.textureSource)
                    if (source.length && !isImageLoaded(source)) loadImage(source)
                    textureLoaded = source.length > 0 && isImageLoaded(source)
                    requestPaint()
                }
                onImageLoaded: { textureLoaded = String(pedestalLayer.textureSource).length > 0
                    && isImageLoaded(String(pedestalLayer.textureSource)); requestPaint() }
                Component.onCompleted: loadTexture()
                onVisibleChanged: if (visible) requestPaint()
                onAvailableChanged: if (available) requestPaint()
                Connections {
                    target: pedestalLayer
                    function onTextureSourceChanged() { pedestalCanvas.loadTexture() }
                    function onPaintKeyChanged() { pedestalCanvas.requestPaint() }
                    function onSpinStepChanged() { pedestalCanvas.requestPaint() }
                    function onWobbleStepChanged() { pedestalCanvas.requestPaint() }
                }
                onPaint: {
                    const context = getContext("2d")
                    context.reset()
                    if (!pedestalLayer.mesh || !(width > 0 && height > 0)) return
                    // A moving stand's step painted before, here or under
                    // another icon with the same stand, is copied.
                    const moving = root.pedestalMotionActive
                    const key = moving ? [pedestalLayer.paintKey, textureLoaded, pedestalLayer.spinStep,
                                          pedestalLayer.wobbleStep].join("|") : ""
                    const painted = moving ? StandFrames.frame(key) : null
                    if (painted) {
                        context.drawImage(painted, 0, 0)
                        return
                    }
                    paintStand(context)
                    if (moving)
                        StandFrames.keep(key, context.getImageData(0, 0, width, height))
                }
                function paintStand(context) {
                    const faces = pedestalLayer.project(root.pedestalLayout.tilt + pedestalLayer.drawnWobble,
                                                        pedestalLayer.drawnSpin)
                    if (!faces.length) return
                    if (pedestalLayer.textureLook === "lightning" || pedestalLayer.textureLook === "aurora") {
                        pedestalLayer.paintLightning(context, faces, textureLoaded, width, height)
                        return
                    }
                    if (pedestalLayer.glowingLook) {
                        pedestalLayer.paintLight(context, faces, textureLoaded, width, height)
                        paintShine(context, faces)
                        return
                    }
                    // Each face in its shade of grey, far ones first; a hairline
                    // in the same grey closes the seams between neighbours.
                    context.lineWidth = 0.8
                    context.lineJoin = "round"
                    for (const face of faces) {
                        const grey = Math.round(255 * Math.min(1, face.shade))
                        const colour = "rgb(" + grey + "," + grey + "," + grey + ")"
                        context.fillStyle = colour
                        context.strokeStyle = colour
                        context.beginPath()
                        context.moveTo(face.corners[0][0], face.corners[0][1])
                        context.lineTo(face.corners[1][0], face.corners[1][1])
                        context.lineTo(face.corners[2][0], face.corners[2][1])
                        context.closePath()
                        context.fill()
                        context.stroke()
                    }
                    // The material's texture and the stand's colour, multiplied
                    // into the shading inside the silhouette.
                    context.save()
                    pedestalLayer.trace(context, faces)
                    context.clip()
                    context.globalCompositeOperation = "qt-multiply"
                    if (textureLoaded) {
                        context.save()
                        pedestalLayer.fillTexture(context, width, height)
                        context.restore()
                    }
                    context.fillStyle = pedestalLayer.tint
                    context.fillRect(0, 0, width, height)
                    context.restore()
                    paintShine(context, faces)
                }
                // A little shine on the faces turned to the light.
                function paintShine(context, faces) {
                    context.save()
                    for (const face of faces) {
                        if (face.shine <= 0) continue
                        context.globalAlpha = Math.min(0.5, face.shine)
                        context.fillStyle = "#ffffff"
                        context.beginPath()
                        context.moveTo(face.corners[0][0], face.corners[0][1])
                        context.lineTo(face.corners[1][0], face.corners[1][1])
                        context.lineTo(face.corners[2][0], face.corners[2][1])
                        context.closePath()
                        context.fill()
                    }
                    context.restore()
                }
            }
        }

        // The light's opening: on the stand's top, its lens, or on the
        // panel where there is no stand; a projector's plate rings it with
        // metal.
        ShaderEffect {
            id: hologramOpening

            objectName: "icon-hologram-opening"
            readonly property var spot: root.hologramLayout.opening || ({ y: 0, rx: 1, ry: 1 })
            visible: root.hologramLight
            width: Math.max(2, 2 * spot.rx * 1.6)
            height: Math.max(2, 2 * spot.ry * 1.6)
            x: (standFrame.width - width) / 2
            y: standFrame.height / 2 + spot.y - height / 2 + (root.pedestalDrawn ? pedestalLayer.bob : 0)
            property color tint: root.hologramColours.opening.colour
            property real time: root.hologramTime
            property real lens: 1 / 1.6
            property real ring: root.pedestalDrawn && root.pedestalKind === "projector" ? 1 : 0
            property real own: root.hologramColours.opening.own ? 1 : 0
            property real strength: Math.min(1.2, root.hologram.beamStrength)
            property variant source: hologramSource
            fragmentShader: "shaders/hologram-opening.frag.qsb"
        }
        // The light, rising from the opening to the icon and widening, the
        // icon's near part in its wide end; once more, fainter, in front of
        // the icon (hologramLightFront).
        HologramBeam {
            id: hologramBeam

            objectName: "icon-hologram-beam"
        }
    }

    component HologramBeam: ShaderEffect {
        readonly property var beam: root.hologramLayout.beam
            || ({ start: 0, length: 1, startHalf: 1, endHalf: 1, fade: 1.6 })
        // How much of the light this sheet draws, from the opening up.
        property real share: 1
        readonly property real sheet: 2 * Math.max((root.hologramLayout.opening || ({ rx: 1 })).rx * 1.04,
                                                   beam.endHalf, beam.startHalf)
        readonly property var flags: HologramLook.beamMotionFlags(root.hologram.beamMotion)
        visible: root.hologramLight
        width: Math.max(2, sheet)
        height: Math.max(2, beam.length * share)
        x: (standFrame.width - width) / 2
        y: standFrame.height / 2 + beam.start - height + (root.pedestalDrawn ? pedestalLayer.bob : 0)
        property color tint: root.hologramColours.light.colour
        property vector4d motion: Qt.vector4d(flags.rise, flags.drift, flags.pulse, flags.sweep)
        property real time: root.hologramLightMoving ? root.hologramTime : 0
        property real startHalf: beam.startHalf / Math.max(1, width)
        property real endHalf: (beam.startHalf + (beam.endHalf - beam.startHalf)
            * Math.pow(Math.max(0, Math.min(1, share)), 0.85)) / Math.max(1, width)
        property real streaks: 30
        property real sparks: 0.1
        property real seed: root.hologramSeed
        property real fade: beam.fade
        property real strength: root.hologram.beamStrength
        property real own: root.hologramColours.light.own ? 1 : 0
        property variant source: hologramSource
        fragmentShader: "shaders/hologram-beam.frag.qsb"
    }

    Item {
        id: visualLayer

        objectName: "icon-visual-layer"
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: root.standDirection.x * root.iconRaise
        anchors.verticalCenterOffset: root.standDirection.y * root.iconRaise
        width: root.logicalSize
        height: root.logicalSize
        scale: Math.max(0, root.visualScale)
        // Over a stand, or its hologram's light, it grows about its foot
        // (the side toward the stand), so it never sinks into it.
        transformOrigin: !(root.pedestalDrawn || root.hologramLight) ? Item.Center
            : root.standDirection.name === "down" ? Item.Top
            : root.standDirection.name === "right" ? Item.Left
            : root.standDirection.name === "left" ? Item.Right : Item.Bottom

        Item {
            id: rearLayer

            objectName: "icon-layer-rear"
            anchors.fill: parent

            IconStyle2D {
                id: styleShadow

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "shadow"
                logicalSize: root.logicalSize
                roleOpacity: root.styleState.rearOpacity === undefined
                    ? 1 : Number(root.styleState.rearOpacity)
                visible: root.styledLayersActive
            }

            IconStyle2D {
                id: styleGlow

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "glow"
                logicalSize: root.logicalSize
                roleOpacity: Math.max(
                    Number(root.styleState.glowOpacity || 0),
                    root.glowAmount)
                stateGlowColor: String(
                    root.styleState.glowColor || "transparent")
                visible: root.styledLayersActive
            }

            IconStyle2D {
                id: styleRear

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "rear"
                logicalSize: root.logicalSize
                roleOpacity: root.styleState.rearOpacity === undefined
                    ? 1 : Number(root.styleState.rearOpacity)
                visible: root.styledLayersActive
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.98
                height: width
                radius: width / 2
                color: "transparent"
                border.width: root.dropTarget ? 3
                    : root.urgent ? 2
                    : root.glowAmount * 3
                border.color: root.dropTarget
                    ? root.alphaColor(Kirigami.Theme.positiveTextColor, 0.9)
                    : root.urgent
                        ? root.alphaColor(
                            Kirigami.Theme.negativeTextColor, 0.85)
                        : root.alphaColor(
                            Kirigami.Theme.highlightColor,
                            root.glowAmount * 0.75)
                visible: !root.styledLayersActive
                    && (root.dropTarget || root.urgent
                        || root.glowAmount > 0.01)

                SequentialAnimation on opacity {
                    running: root.glowAnimating && !root.reducedMotion
                    loops: Animation.Infinite
                    OpacityAnimator {
                        from: 0.25
                        to: 1
                        duration: root.glowDuration
                    }
                    OpacityAnimator {
                        from: 1
                        to: 0.25
                        duration: root.glowDuration
                    }
                }
            }
        }

        // Mask source for the tile silhouette. Kept outside baseLayer so it
        // is not consumed by the layer it masks.
        Image {
            id: styleMaskImage

            objectName: "icon-style-mask-source"
            anchors.fill: parent
            visible: false
            asynchronous: false
            cache: true
            fillMode: Image.PreserveAspectFit
            source: root.currentStyleMaskSource
            onStatusChanged: {
                if (status === Image.Error)
                    root.recordStyleMaskFailure(source.toString())
            }
        }

        // The tile's glow: light round the tile's shape (Icon Tiles >
        // Glow), on a frame of its own so the style's mask, which shapes the
        // tile, does not cut the halo off. A 3D panel draws its own.
        Item {
            id: tileGlowFrame
            objectName: "icon-tile-glow"
            anchors.fill: parent
            layer.enabled: root.tileGlowActive
            layer.effect: GlowEffect {
                glowColor: root.tileGlowColour
                intensity: root.tileGlow
                reach: root.tileGlowReach
            }

            Item {
                id: baseLayer

                objectName: "icon-layer-base"
                anchors.fill: parent
                opacity: root.resolvedTileMotion.opacity
                transform: [
                    Scale {
                        origin.x: baseLayer.width / 2
                        origin.y: baseLayer.height / 2
                        xScale: root.resolvedTileMotion.scale
                            * root.resolvedTileMotion.scaleX
                        yScale: root.resolvedTileMotion.scale
                            * root.resolvedTileMotion.scaleY
                    },
                    Rotation {
                        origin.x: baseLayer.width / 2
                        origin.y: baseLayer.height / 2
                        angle: root.resolvedTileMotion.rotateZ
                    },
                    Translate {
                        x: root.resolvedTileMotion.x
                        y: root.resolvedTileMotion.y
                    }
                ]

                // MultiEffect is instantiated only when a mask is actually
                // declared and its source loaded.
                layer.enabled: root.styleMaskActive
                    && styleMaskImage.status === Image.Ready
                layer.effect: MultiEffect {
                    maskEnabled: true
                    maskSource: styleMaskImage
                }

                IconStyle2D {
                    id: styleBase

                    anchors.fill: parent
                    scale: root.styleTileGrowth
                    styleDefinition: root.effectiveIconStyleDefinition
                    resolvedStyle: root.resolvedIconStyle
                    role: "base"
                    tileSettings: Object.assign({}, root.tileSettings, {
                        iconTileThickness: root.meshVisualActive ? 0 : root.tileValue("iconTileThickness", 0, 0, 24),
                        iconTileBevel: 0
                    })
                    logicalSize: root.logicalSize
                    roleOpacity: root.styleState.baseOpacity === undefined
                        ? 1 : Number(root.styleState.baseOpacity)
                    visible: root.styledLayersActive
                }

                // The panel's own tile, as large as its shape needs to be to
                // hold the icon whole.
                IconTile {
                    objectName: "icon-custom-tile"
                    anchors.centerIn: parent
                    anchors.alignWhenCentered: false
                    width: Math.max(Math.max(0, root.logicalSize - 2 * Math.max(1, root.logicalSize * 0.04))
                        * Number(root.resolvedIconStyle.diameter || 1), root.logicalSize * root.tileHolding)
                    height: width
                    visible: root.panelTileDrawn
                    texture: String(root.tileSettings.iconTileTexture || "none")
                    thickness: root.meshVisualActive ? 0 : root.tileValue("iconTileThickness", 0, 0, 24)
                    bevel: 0
                    shape: root.resolvedIconStyle.iconShape || root.tileShape
                    fillColor: String(root.tileSettings.iconTileColor || "#334155")
                    borderColor: String(root.tileSettings.iconTileBorderColor || "#94a3b8")
                    borderWidth: root.tileSettings.iconTileBorderWidth === undefined ? 1
                        : Math.max(0, Math.min(8, Number(root.tileSettings.iconTileBorderWidth)))
                    opacity: root.tileSettings.iconTileOpacity === undefined ? 0.8
                        : Math.max(0, Math.min(1, Number(root.tileSettings.iconTileOpacity)))
                }

                Rectangle {
                    objectName: "icon-highlight-tile"
                    anchors.fill: parent
                    anchors.margins: Math.max(1, root.logicalSize * 0.04)
                    radius: root.tileRadius()
                    // Over the panel's own tile it shows the icon's state
                    // alone, never a second tile.
                    visible: root.tileRenderingEnabled && !root.customTileActive && !root.styledLayersActive
                        && (root.panelDecorated ? root.dropTarget
                            : (!root.panelTileDrawn && (root.appearance === "plate"
                                || root.appearance === "platform"
                                || root.appearance === "floating-glass"))
                            || root.hovered || root.active || root.dropTarget)
                    color: root.active
                        ? root.alphaColor(Kirigami.Theme.highlightColor, 0.28)
                        : root.dropTarget
                            ? root.alphaColor(
                                Kirigami.Theme.positiveTextColor, 0.22)
                            : root.alphaColor(
                                Kirigami.Theme.backgroundColor,
                                root.hovered ? 0.42 : 0.22)
                    border.width: 1
                    border.color: root.alphaColor(
                        Kirigami.Theme.textColor, root.hovered ? 0.28 : 0.12)
                    scale: root.pressed ? 0.92 : 1
                }

                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: parent.width * 0.92
                    height: parent.height * 0.32
                    visible: root.tileRenderingEnabled && !root.customTileActive && !root.styledLayersActive
                        && root.appearance === "pedestal" && !root.panelDecorated

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * 0.52
                        radius: height / 2
                        color: root.alphaColor(
                            Kirigami.Theme.backgroundColor, 0.72)
                        border.width: 1
                        border.color: root.alphaColor(
                            Kirigami.Theme.textColor, 0.28)
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: parent.height * 0.34
                        width: parent.width * 0.38
                        height: parent.height * 0.56
                        radius: width * 0.22
                        color: root.alphaColor(
                            Kirigami.Theme.backgroundColor, 0.62)
                    }
                }
            }
        }

        Item {
            id: glyphLayer

            objectName: "icon-layer-glyph"
            anchors.fill: parent
            // The icon's glow: light round its picture (Icons > Glow). A 3D
            // panel draws its own round the picture it shows; a solid on a
            // flat panel wears it round the solid.
            layer.enabled: root.iconGlowActive && !root.flatSolidShown
            layer.effect: GlowEffect {
                glowColor: root.iconGlowColour
                intensity: root.iconGlow
                reach: root.iconGlowReach
            }
            opacity: root.resolvedGlyphMotion.opacity
            transform: [
                // The turn comes first so scale, spin and displacement act on
                // the already-turned card rather than fighting it.
                Matrix4x4 {
                    matrix: root.glyphTurnActive
                        ? MotionChannels.turnMatrix(
                            root.glyphTurnAngle, glyphLayer.width,
                            glyphLayer.height, root.logicalSize * 2.4)
                        : Qt.matrix4x4()
                },
                Scale {
                    origin.x: glyphLayer.width / 2
                    origin.y: glyphLayer.height / 2
                    xScale: root.resolvedGlyphMotion.scale
                        * root.resolvedGlyphMotion.scaleX
                    yScale: root.resolvedGlyphMotion.scale
                        * root.resolvedGlyphMotion.scaleY
                },
                Rotation {
                    origin.x: glyphLayer.width / 2
                    origin.y: glyphLayer.height / 2
                    angle: root.resolvedGlyphMotion.rotateZ
                },
                Translate {
                    x: root.resolvedGlyphMotion.x
                    y: root.resolvedGlyphMotion.y
                }
            ]

            Kirigami.Icon {
                id: glyph

                anchors.centerIn: parent
                anchors.alignWhenCentered: false
                anchors.horizontalCenterOffset: root.tileRenderingEnabled
                    ? root.tileValue("iconTileIconOffsetX", 0, -40, 40) : 0
                anchors.verticalCenterOffset: root.tileRenderingEnabled
                    ? root.tileValue("iconTileIconOffsetY", 0, -40, 40) : 0
                width: root.styleGlyphTreatmentActive
                    ? root.logicalSize * Math.max(
                        0.1, 1 - Number(root.styleInset.left || 0)
                            - Number(root.styleInset.right || 0))
                    : root.logicalSize * 0.72
                height: root.styleGlyphTreatmentActive
                    ? root.logicalSize * Math.max(
                        0.1, 1 - Number(root.styleInset.top || 0)
                            - Number(root.styleInset.bottom || 0))
                    : width
                source: root.resolvedIconSource
                // The logo size is continuous. Native
                // bucket rounding shrinks e.g. a 59 px logo to 48 px.
                roundToIconSize: false

                // Kirigami's native monochrome path: only ever reached for a
                // glyph the resolver proved safe to recolor.
                isMask: root.glyphIsMask
                color: root.glyphIsMask ? root.glyphTint : "transparent"

                // State styling applies to the plain fallback too; the
                // resolver supplies per-state values in both cases.
                opacity: Number(root.styleState.glyphOpacity === undefined
                                ? 1 : root.styleState.glyphOpacity)
                scale: Number(root.styleState.glyphScale === undefined
                              ? 1 : root.styleState.glyphScale)
                    * (root.tileRenderingEnabled ? root.tileValue("iconTileIconScale", 100, 25, 150) / 100 : 1)

                // MultiEffect is instantiated only when a tint is requested.
                layer.enabled: root.boldSymbol || (root.glyphTreatment === "tinted"
                    && root.glyphTint.length > 0)
                layer.effect: root.boldSymbol ? symbolBodyEffect : glyphTintEffect
                layer.smooth: true
                layer.textureSize: root.boldSymbol
                    ? Qt.size(Math.ceil(width * 2 * Math.max(1, Screen.devicePixelRatio)),
                              Math.ceil(height * 2 * Math.max(1, Screen.devicePixelRatio)))
                    : Qt.size(0, 0)
            }

            Kirigami.Icon {
                anchors.horizontalCenter: glyph.horizontalCenter
                anchors.top: glyph.bottom
                anchors.topMargin: -root.logicalSize * 0.08
                width: glyph.width
                height: glyph.height * 0.28
                source: glyph.source
                opacity: !root.styledLayersActive && root.showReflection
                    ? 0.16 : 0
                transform: Scale {
                    yScale: -0.28
                    origin.y: 0
                }
            }

            IconStyle2D {
                id: styleReflection

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "reflection"
                logicalSize: root.logicalSize
                roleOpacity: Number(
                    root.styleState.reflectionOpacity || 0)
                visible: root.styledLayersActive && root.showReflection
            }

            // Lighting for the turn: a soft band that sweeps across the card as
            // it rotates. It is derived from the turn itself, so it cannot fall
            // out of step, and it is completely absent while the card rests.
            Item {
                id: glyphHighlight

                objectName: "icon-layer-glyph-highlight"
                anchors.fill: parent
                // A solid turns in its own light.
                visible: root.glyphTurnActive && !root.flatSolidShown
                opacity: Math.min(0.42, Math.abs(
                    root.resolvedGlyphMotion.highlight) * 0.42)

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: parent.height
                    width: Math.max(1, parent.width * 0.5
                        * MotionChannels.turnCompression(root.glyphTurnAngle))
                    x: (parent.width - width) / 2
                        + root.resolvedGlyphMotion.highlight
                            * parent.width * 0.3
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.5; color: "#ffffff" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }
            }
        }

        // The icon as a solid on a flat panel (Icons > 3D), in its picture's
        // place and moving with it; it turns as a solid, in 3D, not as a
        // card.
        Item {
            id: solidLayer

            objectName: "icon-layer-solid"
            anchors.fill: parent
            visible: root.flatSolidShown
            layer.enabled: root.iconGlowActive && root.flatSolidShown
            layer.effect: GlowEffect {
                glowColor: root.iconGlowColour
                intensity: root.iconGlow
                reach: root.iconGlowReach
            }
            opacity: root.resolvedGlyphMotion.opacity
            transform: [
                Scale {
                    origin.x: solidLayer.width / 2
                    origin.y: solidLayer.height / 2
                    xScale: root.resolvedGlyphMotion.scale
                        * root.resolvedGlyphMotion.scaleX
                    yScale: root.resolvedGlyphMotion.scale
                        * root.resolvedGlyphMotion.scaleY
                },
                Rotation {
                    origin.x: solidLayer.width / 2
                    origin.y: solidLayer.height / 2
                    angle: root.resolvedGlyphMotion.rotateZ
                },
                Translate {
                    x: root.resolvedGlyphMotion.x
                    y: root.resolvedGlyphMotion.y + root.hologramFloating
                }
            ]

            Loader {
                id: flatSolidLoader

                anchors.fill: parent
                // A hologram's view is larger than the icon, to hold its
                // glow round it.
                anchors.margins: root.hologramDrawn ? -0.3 * root.logicalSize : 0
                active: root.flatSolidWanted
                source: Qt.resolvedUrl("optional3d/IconSolidView.qml")
                onLoaded: item.icon = root
            }
        }

        // The icon as a hologram (Icons > Hologram), in its picture's place
        // and moving with it: the picture blurred into a glow behind it (in
        // the glow's colour, or its own), then the picture drawn as light.
        // The glow is made once and kept; only the light is drawn anew as
        // it flickers. It floats gently where Float is on.
        Item {
            id: hologramBody

            objectName: "icon-hologram"
            anchors.fill: parent
            // A 3D icon's hologram is its solid, drawn as light above.
            visible: root.hologramDrawn && !root.flatSolidShown
            opacity: root.resolvedGlyphMotion.opacity
            readonly property real floating: root.hologramFloating
            transform: [
                Matrix4x4 {
                    matrix: root.glyphTurnActive
                        ? MotionChannels.turnMatrix(
                            root.glyphTurnAngle, hologramBody.width,
                            hologramBody.height, root.logicalSize * 2.4)
                        : Qt.matrix4x4()
                },
                Scale {
                    origin.x: hologramBody.width / 2
                    origin.y: hologramBody.height / 2
                    xScale: root.resolvedGlyphMotion.scale
                        * root.resolvedGlyphMotion.scaleX
                    yScale: root.resolvedGlyphMotion.scale
                        * root.resolvedGlyphMotion.scaleY
                },
                Rotation {
                    origin.x: hologramBody.width / 2
                    origin.y: hologramBody.height / 2
                    angle: root.resolvedGlyphMotion.rotateZ
                },
                Translate {
                    x: root.resolvedGlyphMotion.x
                    y: root.resolvedGlyphMotion.y + hologramBody.floating
                }
            ]

            HologramEffect {
                anchors.fill: parent
                source: hologramSource
                colours: root.hologramColours
                time: root.hologramTime
                flicker: root.hologramMoving ? root.hologram.flicker : 0
                glow: root.hologram.glow
                glowReach: root.hologram.glowReach
                glowScale: 1 + 0.2 * root.hologram.glowReach
                lineSpacing: root.logicalSize / 26
                seed: root.hologramSeed
            }
        }

        Item {
            id: frontLayer

            objectName: "icon-layer-front"
            anchors.fill: parent

            IconStyle2D {
                id: styleFront

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "front"
                logicalSize: root.logicalSize
                roleOpacity: root.styleState.frontOpacity === undefined
                    ? 1 : Number(root.styleState.frontOpacity)
                stateBorderColor: String(
                    root.styleState.borderColor || "transparent")
                visible: root.styledLayersActive
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: root.tileRadius()
                color: "transparent"
                border.width: root.editMode || root.dropTarget ? 2 : 0
                border.color: root.editMode
                    ? Kirigami.Theme.neutralTextColor
                    : Kirigami.Theme.positiveTextColor
                visible: !root.styledLayersActive && border.width > 0
            }
        }

        RunningIndicator {
            id: runningIndicator

            visible: root.showIndicator
                && (root.running || root.active || root.launching)
                && Number(root.styleState.indicatorOpacity === undefined
                          ? 1 : root.styleState.indicatorOpacity) > 0
            opacity: Number(root.styleState.indicatorOpacity === undefined
                            ? 1 : root.styleState.indicatorOpacity)
            vertical: root.vertical
            active: root.active
            urgent: root.urgent
            windowCount: root.windowCount
            reducedMotion: root.reducedMotion
            style: root.resolvedIndicatorStyle
            anchors.horizontalCenter: root.vertical
                ? undefined : parent.horizontalCenter
            anchors.verticalCenter: root.vertical
                ? parent.verticalCenter : undefined
            anchors.bottom: root.vertical ? undefined : parent.bottom
            anchors.left: root.vertical ? parent.left : undefined
            transform: [
                Scale {
                    origin.x: runningIndicator.width / 2
                    origin.y: runningIndicator.height / 2
                    xScale: root.resolvedIndicatorMotion.scale
                        * root.resolvedIndicatorMotion.scaleX
                    yScale: root.resolvedIndicatorMotion.scale
                        * root.resolvedIndicatorMotion.scaleY
                },
                Translate {
                    x: root.resolvedIndicatorMotion.x
                    y: root.resolvedIndicatorMotion.y
                }
            ]
        }

        Item {
            id: statusLayer

            objectName: "icon-layer-status"
            anchors.fill: parent

            Rectangle {
                id: attentionMarker
                objectName: "icon-attention-marker"
                visible: root.urgent
                anchors.left: parent.left
                anchors.top: parent.top
                width: Math.max(8, root.logicalSize * 0.17)
                height: width
                radius: width / 2
                color: Kirigami.Theme.neutralTextColor
            }

            Rectangle {
                id: temporaryNotice
                objectName: "icon-temporary-status"
                visible: String(root.entry.temporaryStatus || "").length > 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(14, root.logicalSize * 0.25)
                height: width
                radius: width / 2
                color: Kirigami.Theme.backgroundColor
                Text {
                    anchors.centerIn: parent
                    text: "!"
                    font.bold: true
                    color: Kirigami.Theme.textColor
                }
            }

            Rectangle {
                visible: root.entry.isStatus === true
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: statusLabel.implicitHeight + 4
                radius: 3
                color: Kirigami.Theme.backgroundColor
                Text {
                    id: statusLabel
                    objectName: "icon-status-reading"
                    anchors.fill: parent
                    anchors.margins: 2
                    text: String(root.entry.statusText || "")
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    color: root.entry.statusAvailable === true
                        ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                    font.pixelSize: Math.max(9, root.logicalSize * 0.2)
                }
            }

            Rectangle {
                id: badge

                visible: root.badgeSlotActive
                anchors.top: parent.top
                anchors.right: parent.right
                width: Math.max(height, badgeLabel.implicitWidth
                    + Kirigami.Units.smallSpacing)
                height: Math.max(14, root.logicalSize * 0.28)
                radius: height / 2
                color: Kirigami.Theme.highlightColor

                Text {
                    id: badgeLabel

                    anchors.centerIn: parent
                    text: root.badgeText
                    color: Kirigami.Theme.highlightedTextColor
                    font.bold: true
                    font.pixelSize: Math.max(9, parent.height * 0.64)
                }
            }

            Rectangle {
                id: progressTrack

                visible: root.progressSlotActive
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Math.max(2, root.logicalSize * 0.08)
                height: Math.max(2, root.logicalSize * 0.05)
                radius: height / 2
                color: root.alphaColor(Kirigami.Theme.textColor, 0.2)

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1,
                        root.progress))
                    height: parent.height
                    radius: parent.radius
                    color: Kirigami.Theme.highlightColor
                }
            }
        }
    }

    // The light once more, fainter, in front of the hologram's near part,
    // which rises out of it; in the stand's frame, turned as it is.
    Item {
        objectName: "icon-hologram-light-front-frame"
        anchors.fill: parent
        rotation: root.standDirection.turn
        visible: root.hologramLight

        HologramBeam {
            id: hologramLightFront

            objectName: "icon-hologram-beam-front"
            share: root.hologramLight
                ? Math.min(1, (root.hologramLayout.lift + root.logicalSize * 0.3) / Math.max(1, beam.length)) : 1
            opacity: 0.28
            seed: root.hologramSeed * 0.71 + 3.1
        }
    }

    // The icon's picture for its hologram: taken from the glyph layer, which
    // it hides, at the screen's own resolution.
    Component {
        id: glyphTintEffect
        MultiEffect {
            colorization: 1
            colorizationColor: root.glyphTint
        }
    }
    Component {
        id: symbolBodyEffect
        ShaderEffect {
            // How far each stroke grows, in the glyph's own size.
            property vector2d reach: Qt.vector2d(0.045, 0.045)
            fragmentShader: "shaders/symbol-body.frag.qsb"
        }
    }

    ShaderEffectSource {
        id: hologramSource

        readonly property int pixels: Math.max(16, Math.min(512,
            Math.ceil(root.logicalSize * Math.max(1, Screen.devicePixelRatio) * (root.boldSymbol ? 2 : 1))))
        sourceItem: root.hologramDrawn ? glyphLayer : null
        hideSource: root.hologramDrawn
        live: true
        visible: false
        smooth: true
        textureSize: Qt.size(pixels, pixels)
    }
}
