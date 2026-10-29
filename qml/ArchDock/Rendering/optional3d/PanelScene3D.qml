import QtQuick
import QtQuick.Window
import QtQuick3D
import QtQuick3D.Helpers
import "../GizmoMath.js" as GizmoMath
import "../LayoutEngine.js" as LayoutEngine
import "../PlatformGeometry.js" as PlatformGeometry
import "../HologramLook.js" as HologramLook
import "../IconSolid.js" as IconSolid
import ".."
import ArchDock.Decorations3D 1.0

// The true-3D panel renderer (Qt Quick 3D), loaded only when the system can
// draw it. It draws the theme's own platform mesh, or a platform generated
// along the panel's layout in the look's colours, with every icon standing
// upright on a pedestal and facing the viewer. It projects each icon back to
// the screen for input, popup anchors and the folder curve, runs the 3D
// parts of opening and closing, and in desktop edit mode carries the
// move, rotate and scale handles and lets the body be dragged to tilt it.
Item {
    id: root

    required property var sceneDefinition
    required property var sceneResources
    property url textureSource: ""
    property var entryGeometry: []
    property var entryVisuals: []
    property string quality: "high"
    property real panelOpacity: 1
    property bool sceneConcealed: false
    property real layoutAngle: 0
    property real collapseProgress: 0
    property string mechanism: "open"
    // A radial collapse closes the platform toward its centre like an iris,
    // while the theme's declared parts make their own moves, and stops at a
    // small ring the pointer can still find to open the panel again.
    readonly property real radialCollapseKept: 0.2
    readonly property real radialCollapseScale: mechanism === "collapse-radial"
        ? 1 - (1 - radialCollapseKept) * Math.max(0, Math.min(1, collapseProgress)) : 1
    property bool hovered: false
    property bool reducedMotion: false
    property real glowIntensity: 1
    // A panel decoration (DecorationCatalogue.js) worn in a generated
    // platform's place (PanelDecoration3D), with the viewer's colours, its
    // light animations on or off and their speed.
    property string decoration: ""
    property var decorationChoices: ({})
    property bool decorationAnimated: true
    property real decorationSpeed: 1
    readonly property bool motionAllowed: rendererReady && !sceneConcealed && visible && panelOpacity > 0
    readonly property real emissionScale: Math.max(0, Math.min(2, glowIntensity)) * (hovered ? 1.18 : 1)
    readonly property var panelParts: partsForScope("panel")
    readonly property var entryParts: partsForScope("entry")
    readonly property bool partsReady: {
        const definitions = (sceneDefinition || ({})).parts || []
        const data = (sceneResources || ({})).parts || []
        return definitions.length === data.length && data.every(function(part) {
            return part && part.mesh && part.mesh.format === "org.archdock.mesh"
                && part.material && part.material.format === "org.archdock.material"
        })
    }

    // Quality: low draws at three quarters of the size without smoothing;
    // medium at full size with multisampling; high (the default) at full
    // size, supersampled, so edges, textures and the stands are not pixelly.
    readonly property string effectiveQuality:
        ["low", "medium", "high"].includes(quality) ? quality : "high"
    readonly property real resolutionScale: effectiveQuality === "low" ? 0.75 : 1
    readonly property int textureLimit: effectiveQuality === "low" ? 1024
        : effectiveQuality === "high" ? 2048 : 1536
    readonly property real boundedScale: Math.min(resolutionScale,
        textureLimit / Math.max(1, viewWidth, viewHeight))
    readonly property int targetWidth: Math.max(1, Math.ceil(viewWidth * boundedScale))
    readonly property int targetHeight: Math.max(1, Math.ceil(viewHeight * boundedScale))
    // No box. The view is drawn past the panel's own box by as far as the
    // scene reaches out of it - scaled, tilted, moved, an icon magnified, the
    // handles while editing - by the same amount on opposite sides, so the
    // camera stays on the panel's centre, and its field of view widens by as
    // much, so nothing moves on the screen. The reach is measured after each
    // frame in which something moved (updateOverflow); the margin grows in
    // steps with one to spare and gives back only when two are free, so a
    // turning or scaling scene does not make a new picture every frame. It
    // stops at the screen's size: past that nothing could be seen.
    property real overflowX: 0
    property real overflowY: 0
    readonly property real viewWidth: width + 2 * overflowX
    readonly property real viewHeight: height + 2 * overflowY
    // Where the scene is drawn, in the panel's own coordinates.
    readonly property rect drawnBounds: Qt.rect(-overflowX, -overflowY, viewWidth, viewHeight)
    // The panel's field of view, and the camera's, which shows the panel's
    // box through that field with the margins around it.
    readonly property real panelFieldOfView: edgePanel ? 40 : bounded("fieldOfView", 40, 20, 70)
    // An edge panel's field of view spans its thickness, so a left or right
    // panel is seen as a top or bottom one is.
    readonly property real viewFieldOfView: edgePanel
        ? Math.atan(viewHeight / 2 / cameraDistance) * 360 / Math.PI
        : Math.atan(Math.tan(panelFieldOfView * Math.PI / 360)
            * viewHeight / Math.max(1, height)) * 360 / Math.PI
    // A look whose theme ships no 3D mesh stands on a platform generated from
    // the panel's own layout (`sceneDefinition.generated`), in the look's own
    // colours: the palette sampled from its artwork when it is drawn from
    // artwork, or the colours its 2D surface is drawn with.
    property real layoutTrackRadius: 0
    // The icon cell's size in the same pixels as the track radius.
    property real iconCellSize: 52
    // The screen edge an edge panel stands on (Plasma's panel or an Arch
    // Dock bar), empty for a free panel. An edge panel stands on a straight
    // platform as large as the panel, tilted about the screen's edge so its
    // far side goes into the screen toward the middle; its tilt is its one 3D
    // setting, and nothing else of the 3D page acts on it. A free panel's
    // scene is not touched by any of this.
    property string screenEdge: ""
    readonly property bool edgePanel: ["top", "bottom", "left", "right"].includes(screenEdge)
    readonly property bool edgeStanding: screenEdge === "left" || screenEdge === "right"
    // How thick the edge panel itself is: this scene is deeper (room for
    // magnification) and centred across it. 0 where it is not known.
    property real panelThickness: 0
    // Its size across the edge and along it.
    readonly property real edgeAcross: Math.max(1, Math.min(edgeStanding ? width : height,
        panelThickness > 0 ? panelThickness : Infinity))
    readonly property real edgeAlong: Math.max(1, edgeStanding ? height : width)
    // How far it tilts: 0 faces the viewer; at 60 degrees its icons still
    // fit in the panel.
    readonly property real edgeTiltLimit: 60
    readonly property real edgeTilt: edgePanel
        ? Math.max(0, Math.min(edgeTiltLimit, bounded("cameraPitch", 25, -180, 180))) : 0
    // The platform's band across the panel, in its own units, bevels
    // included; its pixels per unit fill the panel's thickness with it.
    readonly property real edgeBandReach: PlatformGeometry.maximumBand
        + (PlatformGeometry.innerBevel + PlatformGeometry.outerBevel) / 2
    readonly property real edgeUnit: edgeAcross / 2 / edgeBandReach
    // It is a thin plate: tilted far, a slab's near wall comes into view as
    // large as its top and looks like the top itself, the panel then seeming
    // tilted the other way.
    readonly property real edgePlateThickness: 0.25
    // The platform's frame: its top in the panel's own plane and its outer
    // edge on the screen's edge, turned about that edge by the tilt. Its near
    // wall then shows below its top, so it moves in by as much; where its far
    // side would still reach out of the panel (at a small tilt, the near wall
    // pushing it out), the whole is drawn a little smaller.
    readonly property var edgeFrame: {
        if (!edgePanel) return null
        const theta = edgeTilt * Math.PI / 180
        const sine = Math.sin(theta), cosine = Math.cos(theta)
        const across = edgeAcross, depth = cameraDistance
        const wall = (PlatformGeometry.topHeight - PlatformGeometry.bottomHeight) * edgeUnit * edgePlateThickness
        const reach = (across * cosine + wall * sine) * depth - across * across / 2 * sine
        const scale = reach > across * depth ? across * depth / reach : 1
        const inward = scale * wall * sine
        const top = PlatformGeometry.topHeight * edgeUnit * edgePlateThickness
        const frames = {
            bottom: { hinge: [0, -across / 2], inward: [0, 1], turn: Qt.vector3d(-edgeTilt, 0, 0) },
            top: { hinge: [0, across / 2], inward: [0, -1], turn: Qt.vector3d(edgeTilt, 0, 0) },
            left: { hinge: [-across / 2, 0], inward: [1, 0], turn: Qt.vector3d(0, edgeTilt, 0) },
            right: { hinge: [across / 2, 0], inward: [-1, 0], turn: Qt.vector3d(0, -edgeTilt, 0) }
        }
        const frame = frames[screenEdge]
        return {
            position: Qt.vector3d(frame.hinge[0] + frame.inward[0] * inward,
                                  frame.hinge[1] + frame.inward[1] * inward, 0),
            pivot: Qt.vector3d(frame.hinge[0], frame.hinge[1], top),
            turn: frame.turn,
            scale: scale
        }
    }
    property string appearance: ""
    property string customColor: ""
    readonly property var materialNames: ["glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime",
        "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma", "picture"]
    // Pictures the user imported for the "picture" look: the platform's
    // and the stands', Arch Dock's copies of them.
    readonly property string panelPicture: String((sceneDefinition || ({})).panelPicture || "")
    readonly property string pedestalPicture: String((sceneDefinition || ({})).pedestalPicture || "")
    // A look's texture: its own file, in its palette, or for "picture" the
    // imported picture.
    function lookTextureSource(look, palette, picture) {
        if (look === "picture")
            return picture
        return Qt.resolvedUrl("../materials/" + LayoutEngine.lookTexture(look, palette) + ".png")
    }
    // Picture without a picture is the theme's own look.
    readonly property string materialLook: materialNames.includes(String(sceneDefinition.materialLook))
        && (String(sceneDefinition.materialLook) !== "picture" || panelPicture.length)
        ? String(sceneDefinition.materialLook) : "theme"
    readonly property string materialColour: PlatformGeometry.parseColour(sceneDefinition.materialColor)
        ? String(sceneDefinition.materialColor) : ""
    // Theme on a generated platform is the flat look's own surface: its
    // translucency, metal, roughness, glow and texture, so that turning 3D on
    // keeps the design made flat; a look's
    // sampled palette only colours it.
    readonly property string effectiveMaterialLook: materialLook !== "theme" ? materialLook
        : generatedSpec && !textureRequired
            ? LayoutEngine.themeStyle(appearance, "", 48).material : "theme"
    // Picture without a picture is the panel's own look.
    readonly property string textureChoice: ["theme", "none"].concat(materialNames)
        .includes(String(sceneDefinition.materialTexture))
        && (String(sceneDefinition.materialTexture) !== "picture" || panelPicture.length)
        ? String(sceneDefinition.materialTexture) : "theme"
    readonly property string textureMaterial: textureChoice === "none" ? ""
        : textureChoice !== "theme" ? textureChoice
        : materialLook !== "theme" ? materialLook
        : generatedSpec && !textureRequired
            ? LayoutEngine.themeStyle(appearance, "", 48).material : ""
    readonly property url materialTextureSource: textureMaterial.length
        ? lookTextureSource(textureMaterial, lookPalette(textureMaterial, "panel"), panelPicture) : ""
    readonly property url materialNormalSource: textureMaterial.length && textureMaterial !== "picture"
        ? Qt.resolvedUrl("../materials/" + textureMaterial + "-normal.png") : ""
    // The energy looks flow round the band while the scene is animated.
    property real flowOffset: 0
    // How fast an animated look moves (its Flow speed or Strike rate).
    readonly property real motionPace: bounded("lookMotionSpeed", 1, 0.1, 3)
    NumberAnimation on flowOffset {
        running: root.materialAnimationActive
        loops: Animation.Infinite
        from: 0
        to: 1
        // Lava creeps; the other looks flow.
        duration: Math.round((root.moltenFlow ? 26000 : 9000) / root.motionPace)
    }
    // A texture that is being replaced keeps the scene ready: while the new
    // file loads, the platform keeps drawing (without that texture for a
    // moment) instead of the whole panel falling back to its flat renderer,
    // with the icons on the flat ring, and coming back once the file is in.
    // Only a scene that has drawn complete once is kept this way; the first
    // readiness still waits for every resource.
    property bool shownOnce: false
    readonly property bool materialTextureReady: !textureMaterial.length
        || (clearLooks.includes(textureMaterial) ? panelLightning.ready : materialImage.status === Image.Ready)
        || (shownOnce && materialImage.status !== Image.Error)
    // A picture holds still: it does not flow or breathe like a look's own
    // texture.
    readonly property bool materialAnimationActive: effectiveMaterialLook !== "theme"
        && textureMaterial !== "picture"
        && LayoutEngine.themeStyle(effectiveMaterialLook, "", 48).animated
        && motionAllowed && visible && !sceneConcealed && !reducedMotion
    property real materialPulse: 0
    SequentialAnimation on materialPulse {
        running: root.materialAnimationActive
        loops: Animation.Infinite
        NumberAnimation { to: 1; duration: Math.round(2400 / root.motionPace); easing.type: Easing.InOutSine }
        NumberAnimation { to: 0; duration: Math.round(2400 / root.motionPace); easing.type: Easing.InOutSine }
    }
    // An energy look's light: the energy looks breathe; lightning flickers
    // and flashes brighter than white when it strikes, which a glow spreads.
    function energyStrength(look) {
        return look === "lightning" ? 0.22 + 1.5 * lightningFlash : 0.35 + materialPulse * 0.45
    }
    // Lightning: at random moments it strikes - a flash
    // that dies away in a fifth of a second, its bolts jumping to another
    // place along the band - or flickers faintly between strikes. Off under
    // reduced motion and while the panel is hidden.
    readonly property bool lightningInScene: [effectiveMaterialLook, textureMaterial,
        pedestalDrawn ? pedestalMaterialLook : "", pedestalDrawn ? pedestalTextureLook : ""].includes("lightning")
    readonly property bool lightningActive: lightningInScene && motionAllowed && visible
        && !sceneConcealed && !reducedMotion
    property real lightningFlash: 0
    property real lightningShift: 0
    onLightningActiveChanged: if (!lightningActive) { flashFade.stop(); lightningFlash = 0 }
    Timer {
        id: lightningTimer
        interval: 120
        repeat: true
        running: root.lightningActive
        onTriggered: {
            interval = Math.round((60 + Math.random() * 520) / root.motionPace)
            if (Math.random() < 0.28) {
                root.lightningShift = Math.random()
                flashFade.stop()
                root.lightningFlash = 0.75 + Math.random() * 0.25
                flashFade.start()
            } else if (!flashFade.running) {
                root.lightningFlash = Math.random() * 0.3
            }
        }
    }
    NumberAnimation {
        id: flashFade
        target: root
        property: "lightningFlash"
        to: 0
        duration: 200
        easing.type: Easing.OutQuad
    }
    readonly property var generatedSpec: (sceneDefinition || ({})).generated || null
    readonly property var generatedShape: !generatedSpec ? null
        : edgePanel ? { kind: "bar", vertical: edgeStanding, length: edgeAlong / 2 / edgeUnit }
        : PlatformGeometry.shapeForLayout(generatedSpec.layout, generatedSpec.polygonSides)
    readonly property var shapedPalette: materialColour.length || materialLook !== "theme"
        ? PlatformGeometry.paletteFromStyle(LayoutEngine.themeStyle(
            materialLook === "theme" ? appearance : materialLook, materialColour, 48))
        : generatedSpec && generatedSpec.palette ? generatedSpec.palette
        : PlatformGeometry.paletteFromStyle(LayoutEngine.themeStyle(appearance, customColor, 48))
    // A platform wearing a material (an aurora, plasma, water) wears it the
    // same all round: its walls, rims and underside in the top's colour, so
    // the material shows on its sides too and no lighter rim draws a line
    // between them.
    readonly property var generatedPalette: !shapedPalette || textureChoice === "none" || !textureMaterial.length
        ? shapedPalette
        : Object.assign({}, shapedPalette, { wall: shapedPalette.top, rim: shapedPalette.top,
                                             under: shapedPalette.top })
    // The platform is rebuilt only when what it is built from changes: a
    // string compares by value, so an edit elsewhere in the scene's
    // definition leaves the mesh, and the GPU buffers made from it, alone.
    // Platform width: 0 (Auto, the default) makes the band as wide as the
    // flat look's line at the panel's own radius, and the platform as deep in
    // proportion, so the ring turned 3D is the ring that was designed flat.
    readonly property real bandSetting: bounded("band", 0, 0, PlatformGeometry.maximumBand)
    readonly property bool bandAuto: bandSetting < PlatformGeometry.minimumBand
    readonly property real flatLineBand: {
        const style = LayoutEngine.themeStyle(appearance, customColor, 48)
        const radius = layoutTrackRadius > 0 ? layoutTrackRadius : 120
        // Never narrower than a column stand's footprint, mapped onto the
        // platform the way the icons' stand points are (px -> track units).
        const footprint = 0.18 * iconCellSize * Math.min(1, pedestalSize) / radius * PlatformGeometry.trackRadius * 1.1
        // A polygon's mitred corners hold a stand only on a wider band: the
        // old default for a triangle or a square, a little less for a
        // pentagon; a ring or an ellipse is as thin as its line allows.
        const sides = generatedShape && generatedShape.kind === "polygon" ? Number(generatedShape.sides || 6) : 0
        const corners = sides > 0 && sides <= 4 ? 0.11 : sides === 5 ? 0.09 : 0
        return Math.max(PlatformGeometry.minimumBand, footprint, corners, Math.min(PlatformGeometry.maximumBand,
            (Number(style.lineWidth) || 12) / 2 / radius))
    }
    readonly property real resolvedBand: edgePanel ? PlatformGeometry.maximumBand
        : bandAuto ? flatLineBand : bandSetting
    readonly property real platformThickness: edgePanel ? edgePlateThickness : transformValue("thickness", 1, 0.05, 20)
        * (bandAuto ? Math.max(0.4, Math.min(1, resolvedBand / 0.11)) : 1)
    // A platform wearing a picture wears it as a real surface, its
    // brightness its height (PictureRelief), as deep as Relief depth: traced
    // per pixel on any platform, and a generated one's mesh is cut finely
    // enough to rise with it as well.
    readonly property bool pictureRelief: textureMaterial === "picture" && panelPicture.length > 0
    // The panel as a river or as magma: its water or lava flows along the
    // band in a loop (FlowingSurface). Lava creeps.
    readonly property bool flowingSurface: textureMaterial === "river" || moltenFlow
    readonly property bool moltenFlow: textureMaterial === "magma"
    // The water mirrors the icons: each icon's picture again, upside down,
    // at the place on the screen where the icon's mirror image in the
    // band's plane lies, on a sheet the water samples at each point's own
    // place on the screen. The sheet is a plain item (a second 3D view in
    // the scene crashes the scene's teardown), and the mirror images follow
    // the icons' every move through their bindings.
    property bool reflectionsAllowed: true
    readonly property bool waterReflections: flowingSurface && !moltenFlow && reflectionsAllowed
        && rendererReady && !sceneConcealed
    readonly property vector3d mirrorNormal: sceneContent.mapDirectionToScene(Qt.vector3d(0, 0, 1)).normalized()
    readonly property vector3d mirrorPoint: sceneContent.mapPositionToScene(Qt.vector3d(0, 0, platformTop))
    function reflectedPoint(p) {
        const n = mirrorNormal
        const o = mirrorPoint
        const d = 2 * ((p.x - o.x) * n.x + (p.y - o.y) * n.y + (p.z - o.z) * n.z)
        return Qt.vector3d(p.x - d * n.x, p.y - d * n.y, p.z - d * n.z)
    }
    // Relief depth 0.8, the default, sinks the gaps 0.08 of the scene's
    // units; 1 is a little deeper, the band never worn thin.
    readonly property real pictureDepth: bounded("panelPictureDepth", 0.8, 0, 1) * 0.1
    // Which parts of the picture stand out: its light ones, or its dark ones;
    // and whether they rise out of the band or the rest sinks into it.
    readonly property bool pictureRaisesDark: String((sceneDefinition || ({})).panelPictureRise || "light") === "dark"
    readonly property bool pictureRisesOut: String((sceneDefinition || ({})).panelPictureDirection || "inward") === "outward"
    // The depth map's regulators: its midtone, the steepness through it,
    // and its blur.
    readonly property real pictureLevel: bounded("panelPictureLevel", 0.38, 0, 1)
    readonly property real pictureContrast: bounded("panelPictureContrast", 0.35, 0, 1)
    readonly property real pictureSmoothing: bounded("panelPictureSmoothing", 0, 0, 1)
    // A picture keeps its own proportions round the band: a wide one
    // repeats fewer times, so it is never squeezed into a square.
    readonly property int pictureRepeats: !generatedPlatform ? 1
        : materialImage.implicitHeight > 0
            ? Math.max(1, Math.round(Number(generatedPlatform.textureRepeats || 1)
                * materialImage.implicitHeight / Math.max(1, materialImage.implicitWidth)))
            : Number(generatedPlatform.textureRepeats || 1)
    readonly property string generatedKey: generatedShape ? JSON.stringify(Object.assign({
        shape: generatedShape, band: resolvedBand,
        bend: edgePanel ? 0 : bounded("bend", 0, -1, 1), palette: generatedPalette,
        allRound: textureChoice !== "none" && textureMaterial.length > 0 }, pictureRelief ? { relief: true } : {})) : ""
    readonly property var generatedPlatform: generatedKey.length > 0
        ? PlatformGeometry.platform(JSON.parse(generatedKey)) : null
    // A decoration is worn in a generated platform's place, along its
    // centre line with the icons where they stand; a Möbius strip, whose
    // band turns over, and a theme's own mesh keep their own look.
    readonly property bool decorationWanted: decoration.length > 0 && generatedPlatform !== null
        && generatedShape !== null && generatedShape.kind !== "mobius"
    readonly property bool decorationShown: decorationWanted && platformDecoration.ready
    readonly property var decorationRuns: decorationWanted
        ? PlatformGeometry.centreLine(generatedShape, 96, generatedPlatform.track,
            Math.max(PlatformGeometry.minimumBand, Math.min(PlatformGeometry.maximumBand, resolvedBand))
            + PlatformGeometry.outerBevel)
        : []
    // The icons' places at rest in the panel's own pixels (PanelScene's
    // restEntryCentres): the decoration is worked out round them once. The
    // icons travel over it as they turn round the dock; their drawn places
    // (entryGeometry) move every frame and would start the bake over each
    // time, so it never ended while the dock turned.
    property var restEntryCentres: []
    readonly property var decorationIconPoints: decorationWanted ? restEntryCentres.map(function(centre) {
        return platformPlace(Number(centre.x) - width / 2, height / 2 - Number(centre.y))
    }) : []
    // Where on a generated platform an icon stands whose place in the flat
    // layout is (x, y) pixels from the panel's centre: carried onto the
    // layout's track, or on an edge panel's straight platform just where it
    // is.
    function platformPlace(x, y) {
        return edgePanel ? [x / platformScale, y / platformScale]
            : PlatformGeometry.standPoint(x, y, layoutTrackRadius, generatedPlatform.track)
    }
    // How far an icon stands raised on the decoration under it, in the
    // scene's pixels (its height there, Thickness stretching it).
    function decorationLift(index) {
        if (!decorationShown) return 0
        return Number((platformDecoration.iconHeights || [])[index] || 0) * platformThickness
    }
    // A theme's own mesh wearing a picture is cut finely enough for its
    // relief to be real geometry.
    readonly property var themeReliefMesh: pictureRelief && !generatedSpec
        ? PlatformGeometry.reliefMesh((sceneResources || ({})).mesh || null, 0.015) : null
    readonly property var basePlatformMesh: generatedSpec ? generatedPlatform
        : themeReliefMesh || (sceneResources || ({})).mesh || null
    readonly property real baseTopHeight: generatedSpec ? PlatformGeometry.topHeight
        : Math.max(0, ...((basePlatformMesh || {}).positions || []).map(p => Number(p[2])))
    readonly property real platformFold: edgePanel ? 0 : bounded("fold", 0, -1, 1)
    readonly property var platformMesh: PlatformGeometry.foldMesh(basePlatformMesh, platformFold, baseTopHeight)
    // A generated platform's pedestals take its top's colour, and its rim
    // glows in the look's glow colour.
    readonly property var platformMaterial: {
        const material = Object.assign({}, (sceneResources || ({})).material || ({}))
        if (generatedSpec && generatedPalette && generatedPalette.top) {
            const top = generatedPalette.top
            material.baseColor = Qt.rgba(top[0], top[1], top[2], 1)
        }
        if (generatedSpec && generatedPalette && generatedPalette.glow) {
            const glow = generatedPalette.glow
            material.rimEmissiveColor = Qt.rgba(glow[0], glow[1], glow[2], 1)
            material.rimEmissiveStrength = Number(generatedPalette.glowStrength || 0)
        }
        if (materialColour.length) material.baseColor = materialColour
        if (effectiveMaterialLook !== "theme") {
            const look = effectiveMaterialLook
            material.metalness = ["metallic", "platform", "plate", "pedestal", "futuristic"].includes(look) ? 0.85 : 0
            material.roughness = ["glass", "floating-glass", "crystal"].includes(look) ? 0.12
                : look === "organic" || look === "minimal" ? 0.85 : 0.38
            material.opacity = LayoutEngine.themeStyle(look, "", 48).alpha
            if (["neon", "plasma", "lightning", "aurora", "futuristic"].includes(look)) {
                material.emissiveColor = materialColour || LayoutEngine.themeStyle(look, "", 48).stroke
                material.emissiveStrength = root.energyStrength(look)
                material.emissiveThroughTexture = true
            }
            // A clear-ground look shows its texture's own colours on a clear
            // ground: no tint, no vertex shades, its rim as strong as its sky.
            if (clearLooks.includes(look) || clearLooks.includes(textureMaterial) || panelRepainted) {
                material.alphaBlend = clearLooks.includes(look) || clearLooks.includes(textureMaterial)
                if (material.alphaBlend) material.rimOpacity = root.panelLookBackground
                material.vertexColours = false
                material.baseColor = materialColour || "#ffffff"
                material.emissiveColor = materialColour || "#ffffff"
            }
        }
        if (textureMaterial === "picture")
            wearPicture(material)
        return material
    }
    // The theme's own surface texture, for everything drawn in its material:
    // its meshes map it from above, so a pedestal or a part takes the colour
    // of the platform where it stands.
    readonly property Texture themeTexture: textureChoice === "none" ? null
        : textureMaterial.length ? (materialTextureReady ? selectedMaterialTexture : null)
        : textureRequired && textureReady ? surfaceTexture : null
    // The material's relief map, for meshes that can be lit with it.
    readonly property Texture themeNormalTexture: textureChoice !== "none" && textureMaterial.length
        && materialTextureReady && materialNormalImage.status === Image.Ready ? selectedMaterialNormal : null
    // Turns an icon, from inside the entries' frame, to face the camera: the
    // frame's own rotation undone, the camera's taken.
    // Real 3D icons: how deep each icon's solid is, as a share of the icon's
    // size (0 keeps the flat picture), and how far the solid turns about
    // its upright axis, so its depth shows (the icon still faces the user,
    // turned that much).
    // Only icons set to be 3D (Icons > 3D) are solids: a 3D panel alone
    // keeps them flat pictures. A hologram icon is drawn on a solid, so it
    // shows as it turns; kept flat, its solid is as thin as a card and does
    // not turn to show a depth it has not got.
    readonly property bool iconSolid: (sceneDefinition || ({})).iconSolid === true
    readonly property real iconDepth: !iconSolid ? (iconHologram.on ? 0.02 : 0)
        : iconHologram.on ? Math.max(0.3, bounded("iconDepth", 0.3, 0, 1))
        : bounded("iconDepth", 0.3, 0, 1)
    readonly property real iconTurn: iconSolid ? bounded("iconTurn", 15, -60, 60) : 0
    // How much the light models the icons' volumes (Icon lighting): 0 keeps
    // the picture's own brightness, 1 lets the light alone shape them.
    readonly property real iconLighting: bounded("iconLighting", 0.5, 0, 1)
    readonly property string iconRelief: {
        const word = String((sceneDefinition || ({})).iconRelief || "")
        return word === "block" || word === "rounded" ? word : "modelled"
    }
    readonly property quaternion iconFacing: entryFrame.sceneRotation.inverted()
        .times(camera.sceneRotation)
    // The stand under every icon (the Pedestals page). It turns with its
    // icon to face the viewer: a horizontal stand seen a little from above,
    // the icon centred over it and floating its Lift above the stand's top,
    // at every camera angle. "auto" is a column in the platform's
    // material, the 3D page's Pedestal height tall, the icon on its top;
    // "none", the default, draws none.
    readonly property var materialLooks: ["glass", "crystal", "neon", "minimal", "plasma", "lightning", "aurora", "lime",
        "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal", "river", "magma", "picture"]
    readonly property string pedestalShape: String((sceneDefinition || ({})).pedestalShape || "none").toLowerCase()
    readonly property string pedestalKind: pedestalShape === "auto" ? "column"
        : PlatformGeometry.pedestalShapes.includes(pedestalShape) ? pedestalShape : "none"
    readonly property bool pedestalDrawn: pedestalKind !== "none"
        && (pedestalShape !== "auto" || bounded("iconElevation", 0.3, 0, 2) > 0)
    readonly property real pedestalSize: bounded("pedestalSize", 1, 0.2, 4)
    readonly property real pedestalThickness: bounded("pedestalThickness", 1, 0.2, 4)
    readonly property real pedestalLift: bounded("pedestalLift", 0.3, 0, 2)
    readonly property real pedestalOpacity: bounded("pedestalOpacity", 1, 0.1, 1)
    readonly property real pedestalGlow: bounded("pedestalGlow", 0, 0, 1)
    // A stand made for a hologram (a projector) wears its own finish in the
    // panel's look's place.
    readonly property var pedestalFinish: String((sceneDefinition || ({})).pedestalMaterial || "panel").toLowerCase()
        === "panel" ? PlatformGeometry.pedestalFinish(pedestalKind) : null
    readonly property string pedestalMaterialLook: pedestalFinish ? pedestalFinish.look
        : String((sceneDefinition || ({})).pedestalMaterial || "panel").toLowerCase()
    readonly property string pedestalTextureChoice: String((sceneDefinition || ({})).pedestalTexture || "material").toLowerCase()
    readonly property string pedestalColour: String((sceneDefinition || ({})).pedestalColor || "")
        || (pedestalFinish ? pedestalFinish.colour : "")
    // A stand wearing a picture of its own stands out in relief as the
    // panel does: its mesh is cut finely and rises by the picture's
    // brightness, in its own units. A stand is small, so its relief is a
    // quarter of its size at the full Relief depth, and its stones stand
    // out of it (Outward) unless told to sink in.
    readonly property bool pedestalPictureRelief: pedestalDrawn && pedestalTextureLook === "picture"
        && pedestalPicture.length > 0
    readonly property real pedestalPictureDepth: bounded("pedestalPictureDepth", 0.8, 0, 1) * 0.25
    readonly property bool pedestalPictureRaisesDark: String((sceneDefinition || ({})).pedestalPictureRise || "light") === "dark"
    readonly property bool pedestalPictureRisesOut: String((sceneDefinition || ({})).pedestalPictureDirection || "outward") === "outward"
    readonly property real pedestalPictureLevel: bounded("pedestalPictureLevel", 0.38, 0, 1)
    readonly property real pedestalPictureContrast: bounded("pedestalPictureContrast", 0.35, 0, 1)
    readonly property real pedestalPictureSmoothing: bounded("pedestalPictureSmoothing", 0, 0, 1)
    readonly property string pedestalAnimation: String((sceneDefinition || ({})).pedestalAnimation || "none").toLowerCase()
    readonly property real pedestalAnimationSpeed: bounded("pedestalAnimationSpeed", 1, 0.1, 5)
    // Picture without a picture is the panel's look.
    readonly property bool pedestalOwnMaterial: pedestalDrawn && pedestalMaterialLook !== "panel"
        && materialLooks.includes(pedestalMaterialLook)
        && (pedestalMaterialLook !== "picture" || pedestalPicture.length)
    // Its look from a little above, in its icon's frame: the top shows.
    readonly property real pedestalTilt: 20
    readonly property var pedestalMesh: PlatformGeometry.pedestalShape(pedestalDrawn ? pedestalKind : "column", 24)
    // Cut finely for its relief, within fewer triangles than the platform
    // (each icon has a stand of its own).
    readonly property var pedestalReliefMesh: pedestalPictureRelief
        ? PlatformGeometry.reliefMesh(pedestalMesh, 0.05, 6000) : null
    readonly property var pedestalShownMesh: pedestalReliefMesh || pedestalMesh
    readonly property real pedestalShapeHeight: PlatformGeometry.pedestalShapeHeight(pedestalKind)
    readonly property int pedestalTriangles: pedestalShownMesh.indexes.length / 3
    // Each shape's width per unit, in icon sizes at Size 1, and its reach.
    readonly property real pedestalShapeScale: PlatformGeometry.pedestalShapeScale(pedestalKind)
    readonly property real pedestalShapeReach: PlatformGeometry.pedestalShapeReach(pedestalKind)
    // Bend tilts a generated band's top across its width (0.9 per unit of
    // bend), so a stand on the centre line would sink into the rim on the
    // high side: the band's rise under the stand's width.
    readonly property real platformBendSlope: generatedSpec && !edgePanel ? Math.abs(bounded("bend", 0, -1, 1)) * 0.9 : 0
    // Its material: the platform's or a look's, in its colour, opacity and
    // glow; its texture: the material's own, none, or a look's.
    readonly property string pedestalTextureLook: pedestalTextureChoice === "material"
        ? (pedestalOwnMaterial ? pedestalMaterialLook : "panel")
        : pedestalTextureChoice === "picture" && !pedestalPicture.length ? "panel"
        : pedestalTextureChoice === "none" || materialLooks.includes(pedestalTextureChoice)
            ? pedestalTextureChoice : "panel"
    readonly property var pedestalMaterialData: {
        const base = Object.assign({}, pedestalOwnMaterial
            ? materialForLook(pedestalMaterialLook, pedestalColour) : platformMaterial || ({}))
        if (!pedestalOwnMaterial && pedestalColour.length) base.baseColor = pedestalColour
        if (pedestalTextureLook === "picture")
            wearPicture(base)
        base.opacity = Math.min(Number(base.opacity === undefined ? 1 : base.opacity), pedestalOpacity)
        // A palette of its own paints the stand's look in its own colours.
        if (LayoutEngine.lookTexture(pedestalTextureLook, pedestalPalette) !== pedestalTextureLook) {
            base.baseColor = pedestalColour.length ? pedestalColour : "#ffffff"
            base.emissiveColor = base.baseColor
        }
        // A glowing stand is only lit a little from within; its light shines
        // round it from its glow shell (standGlowMaterial), instead of the
        // stand turning one bright colour.
        if (pedestalGlowAmount > 0) {
            base.emissiveColor = Qt.rgba(pedestalGlowRgb[0], pedestalGlowRgb[1], pedestalGlowRgb[2], 1)
            base.emissiveStrength = Math.max(Number(base.emissiveStrength || 0), pedestalGlowAmount * 0.3)
            base.emissiveThroughTexture = false
        }
        return base
    }
    // The stands' glow: how strong, in what colour (the stand's own, or its
    // look's line colour), and the light of its shell - brighter than white
    // by as much as the glow is strong, since the view spreads what is
    // brighter than white round it. The shell stands a little out of the
    // stand on every side, centred on it, and is seen only from inside, so
    // it shows round the stand's edges and never over its faces.
    // A stand wearing a picture does not glow, as stone does not.
    readonly property real pedestalGlowAmount: !pedestalDrawn || pedestalTextureLook === "picture" ? 0
        : pedestalAnimation === "glow" ? Math.max(0.5, pedestalGlow) : pedestalGlow
    readonly property string pedestalGlowColour: String((sceneDefinition || ({})).pedestalGlowColor || "")
    readonly property var pedestalGlowRgb: PlatformGeometry.parseColour(pedestalGlowColour.length
        ? pedestalGlowColour : pedestalColour.length ? pedestalColour
        : LayoutEngine.themeStyle(pedestalOwnMaterial ? pedestalMaterialLook : appearance, "", 48).stroke)
        || [0.6, 0.85, 1, 1]
    // A glow's light: its colour brighter than white, by as much as the glow
    // is strong; only the colour's strongest part goes past white, so the
    // halo keeps the colour instead of burning white.
    function glowLight(rgb, amount) {
        const light = (1.35 + 1.3 * amount) / Math.max(0.05, rgb[0], rgb[1], rgb[2])
        return Qt.vector3d(rgb[0] * light, rgb[1] * light, rgb[2] * light)
    }
    readonly property vector3d pedestalGlowLight: glowLight(pedestalGlowRgb, pedestalGlowAmount)
    // The panel's own glow, light round its shape: in its colour or its
    // look's line colour, from a shell of the platform pushed out along its
    // surface, so a ring glows on its inner and outer edges alike. A panel
    // wearing a picture does not glow, as stone does not.
    readonly property real panelGlowAmount: pictureRelief || !LayoutEngine.glowFitsLook(textureMaterial)
        ? 0 : bounded("panelGlow", 0, 0, 1)
    readonly property var panelGlowRgb: PlatformGeometry.parseColour(
        String((sceneDefinition || ({})).panelGlowColor || "").length
            ? String(sceneDefinition.panelGlowColor) : materialColour.length ? materialColour
            : LayoutEngine.themeStyle(effectiveMaterialLook !== "theme" ? effectiveMaterialLook : appearance,
                                      customColor, 48).stroke) || [0.6, 0.85, 1, 1]
    readonly property vector3d panelGlowLight: glowLight(panelGlowRgb, panelGlowAmount)
    function extrudedMesh(mesh, distance) {
        if (!mesh || !mesh.positions || !mesh.normals || mesh.normals.length !== mesh.positions.length)
            return null
        return { format: mesh.format, indexes: mesh.indexes, normals: mesh.normals,
                 positions: mesh.positions.map(function(point, index) {
                     const normal = mesh.normals[index]
                     return [Number(point[0]) + Number(normal[0]) * distance,
                             Number(point[1]) + Number(normal[1]) * distance,
                             Number(point[2]) + Number(normal[2]) * distance]
                 }) }
    }
    readonly property var platformGlowMesh: panelGlowAmount > 0
        ? extrudedMesh(platformMesh, 0.02 + 0.04 * panelGlowReach) : null
    // At the top of its range the panel's glow lights the shape itself as
    // well, white-hot at full intensity; below that the look stays and the
    // light is round it.
    readonly property real panelGlowWash: panelGlowAmount > 0.75
        ? Math.pow((panelGlowAmount - 0.75) / 0.25, 2) : 0
    // Just past white: the shape looks white-hot without pouring more light
    // round it than its glow's reach leaves room for.
    readonly property vector3d panelWashLight: {
        const rgb = panelGlowRgb
        const light = 1.15 / Math.max(0.05, rgb[0], rgb[1], rgb[2])
        return Qt.vector3d(rgb[0] * light, rgb[1] * light, rgb[2] * light)
    }
    // Over its faces, not its rim: at full glow the shape still shows its
    // edge.
    readonly property var platformWashMesh: {
        if (panelGlowWash <= 0)
            return null
        const mesh = extrudedMesh(platformMesh, 0.004)
        const rim = Number((platformMesh || ({})).rimOffset || 0)
        if (mesh && rim > 0)
            mesh.indexes = mesh.indexes.slice(0, rim)
        return mesh
    }
    // Light round each icon's picture: its shape, a little larger, brighter
    // than white behind it.
    readonly property real iconGlowAmount: bounded("iconGlow", 0, 0, 1)
    readonly property real iconGlowReach: bounded("iconGlowReach", 0.75, 0, 1)
    readonly property var iconGlowRgb: PlatformGeometry.parseColour(String((sceneDefinition || ({})).iconGlowColor || ""))
        || [0.66, 0.86, 1, 1]
    readonly property vector3d iconGlowLight: glowLight(iconGlowRgb, iconGlowAmount)
    readonly property real iconGlowSpread: 1.04 + 0.1 * iconGlowReach
    // Light round each tile, from a shell of the tile.
    readonly property real tileGlowAmount: bounded("iconTileGlow", 0, 0, 1)
    readonly property real tileGlowReach: bounded("iconTileGlowReach", 0.75, 0, 1)
    readonly property var tileGlowRgb: PlatformGeometry.parseColour(String((sceneDefinition || ({})).iconTileGlowColor || ""))
        || [0.5, 0.83, 1, 1]
    readonly property vector3d tileGlowLight: glowLight(tileGlowRgb, tileGlowAmount)
    readonly property real tileGlowSpread: 1.04 + 0.08 * tileGlowReach
    readonly property real pedestalGlowSpread: 1.06 + 0.08 * pedestalGlowReach
    readonly property vector3d pedestalGlowShift: {
        const box = pedestalBox || [0, 0, 0, 0, 0, 0]
        const share = pedestalGlowSpread - 1
        return Qt.vector3d(-(box[0] + box[3]) / 2 * share, -(box[1] + box[4]) / 2 * share,
                           -(box[2] + box[5]) / 2 * share)
    }
    // A real glow: the view spreads the light of what is brighter
    // than white round it, over the desktop as well - a glowing stand's
    // shell, and the panel's glowing rim or energy look when Glow intensity
    // is turned past 1. Only while something glows: the glow costs a few
    // passes of every frame. Its halo reaches about this far.
    readonly property bool panelGlowing: glowIntensity > 1.001
        && (Number(platformMaterial.rimEmissiveStrength || 0) > 0
            || Number(platformMaterial.emissiveStrength || 0) > 0)
    readonly property bool bloomWanted: pedestalGlowAmount > 0 || panelGlowing || panelGlowAmount > 0
        || iconGlowAmount > 0 || tileGlowAmount > 0 || (decorationShown && platformDecoration.glows)
        || hologramShown
    // How far a glow reaches out of what glows (Glow reach, 0 to 1): the
    // stands' and the panel's own. A view has one glow, so it reaches as far
    // as the farthest that is lit. Each further halving of the blur doubles
    // the reach: three halvings at none, seven at the full reach, six at the
    // default. The halo reaches about the
    // margin given here past what glows.
    readonly property real pedestalGlowReach: bounded("pedestalGlowReach", 0.75, 0, 1)
    readonly property real panelGlowReach: bounded("panelGlowReach", 0.75, 0, 1)
    readonly property real bloomReach: Math.max(pedestalGlowAmount > 0 ? pedestalGlowReach : 0,
                                                panelGlowing || panelGlowAmount > 0 ? panelGlowReach : 0,
                                                iconGlowAmount > 0 ? iconGlowReach : 0,
                                                tileGlowAmount > 0 ? tileGlowReach : 0)
    readonly property int bloomLevels: 3 + Math.round(4 * bloomReach)
    readonly property real glowReach: bloomWanted ? 12 * Math.pow(2, bloomLevels - 2) : 0
    readonly property bool pedestalOwnTexture: pedestalDrawn && pedestalTextureLook !== "panel"
        && pedestalTextureLook !== "none"
    readonly property url pedestalTextureSource: pedestalOwnTexture
        ? lookTextureSource(pedestalTextureLook, lookPalette(pedestalTextureLook, "pedestal"), pedestalPicture)
        : ""
    readonly property url pedestalNormalSource: pedestalOwnTexture && pedestalTextureLook !== "picture"
        ? Qt.resolvedUrl("../materials/" + pedestalTextureLook + "-normal.png") : ""
    readonly property Texture pedestalTexture: pedestalTextureLook === "none" ? null
        : pedestalOwnTexture ? ((clearLooks.includes(pedestalTextureLook) ? standLightning.ready
                                 : pedestalImage.status === Image.Ready) ? selectedPedestalTexture : null)
        : themeTexture
    readonly property Texture pedestalNormalTexture: pedestalTextureLook === "none" ? null
        : pedestalOwnTexture ? (pedestalNormalImage.status === Image.Ready ? selectedPedestalNormal : null)
        : themeNormalTexture
    // The pedestals' motion: one phase for the scene, each entry a little
    // behind the one before it.
    readonly property bool pedestalMotionActive: pedestalDrawn && pedestalAnimation !== "none"
        && motionAllowed && visible && !sceneConcealed && !reducedMotion
    property real pedestalPhase: 0
    NumberAnimation on pedestalPhase {
        running: root.pedestalMotionActive
        loops: Animation.Infinite
        from: 0
        to: 1
        duration: Math.round(4000 / Math.max(0.1, root.pedestalAnimationSpeed))
    }
    // Holograms (the Hologram tabs): the icons' solids, the stands and the
    // platform drawn as light (HologramSurface), each icon floating over
    // its projector's light (HologramLight) where that is on, the light
    // leaving a lens (HologramOpening) in the middle of its stand's top or
    // on the platform, the icons turning as Turning says - all on the
    // holograms' one clock, still under reduced motion.
    readonly property var iconHologram: HologramLook.settings(sceneDefinition, "Icon")
    readonly property var iconHologramColours: HologramLook.colours(sceneDefinition, "Icon")
    readonly property var panelHologram: HologramLook.settings(sceneDefinition, "Panel")
    readonly property var panelHologramColours: HologramLook.colours(sceneDefinition, "Panel")
    readonly property var standHologram: HologramLook.settings(sceneDefinition, "Pedestal")
    readonly property var standHologramColours: HologramLook.colours(sceneDefinition, "Pedestal")
    readonly property bool standHologramShown: standHologram.on && pedestalDrawn
    // The panel's hologram: its platform, or the decoration standing in its
    // place, drawn as light.
    readonly property bool panelHologramShown: panelHologram.on
    readonly property bool hologramShown: iconHologram.on || panelHologramShown || standHologramShown
    readonly property bool hologramMoving: hologramShown && motionAllowed && visible && !sceneConcealed
        && !reducedMotion
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
    readonly property real hologramClock: hologramMoving ? HologramClock.time : 0
    readonly property real iconHologramTime: hologramClock * iconHologram.speed
    // The icons' turn as holograms, degrees about their upright axis.
    readonly property real hologramTurn: !iconHologram.on || !hologramMoving ? 0
        : iconHologram.turning === "turn" ? (iconHologramTime * 40) % 360
        : iconHologram.turning === "sway" ? Math.sin(iconHologramTime * 0.9) * 28 : 0
    // How far a hologram icon floats over its light's opening, in icon
    // sizes (Light height), and the light's motion.
    readonly property real hologramLift: iconHologram.beam ? 0.95 * iconHologram.beamHeight : 0
    readonly property var hologramLightFlags: HologramLook.beamMotionFlags(iconHologram.beamMotion)
    // A colour as the shaders take it.
    function hologramColour(part) {
        return (part && part.colour) ? part.colour : "#ffffff"
    }
    function hologramOwn(colours) {
        const c = colours || ({})
        return Qt.vector4d(c.outline && c.outline.own ? 1 : 0, c.inline && c.inline.own ? 1 : 0,
                           c.volume && c.volume.own ? 1 : 0, 0)
    }
    // The stands and the platform as holograms: in their own material's
    // colour, or their set's.
    HologramSurface {
        id: standHologramMaterial
        ownColour: String((pedestalMaterialData || ({})).baseColor || "#9fb8c8")
        outlineColour: root.hologramColour(root.standHologramColours.outline)
        inlineColour: root.hologramColour(root.standHologramColours.inline)
        volumeColour: root.hologramColour(root.standHologramColours.volume)
        own: root.hologramOwn(root.standHologramColours)
        time: root.hologramClock * root.standHologram.speed
        flicker: root.hologramMoving ? root.standHologram.flicker : 0
        seed: 1.7
        brightness: 0.55 + 0.35 * root.standHologram.glow
        cullMode: Material.BackFaceCulling
    }
    HologramSurface {
        id: platformHologramMaterial
        ownColour: String((platformMaterial || ({})).baseColor || "#9fb8c8")
        outlineColour: root.hologramColour(root.panelHologramColours.outline)
        inlineColour: root.hologramColour(root.panelHologramColours.inline)
        volumeColour: root.hologramColour(root.panelHologramColours.volume)
        own: root.hologramOwn(root.panelHologramColours)
        time: root.hologramClock * root.panelHologram.speed
        flicker: root.hologramMoving ? root.panelHologram.flicker : 0
        seed: 0.37
        body: 0.75
        brightness: 0.6 + 0.4 * root.panelHologram.glow
        cullMode: Material.BackFaceCulling
    }
    // The camera's right and up, seen from the entries' frame: how far an
    // icon turned to face the viewer leans toward the platform. Its stand
    // and icon rise by that much, so no part of them dips into the platform
    // at any camera angle.
    readonly property vector3d iconRight: iconFacing.times(Qt.vector3d(1, 0, 0))
    readonly property vector3d iconUp: iconFacing.times(Qt.vector3d(0, 1, 0))
    readonly property vector3d iconForward: iconFacing.times(Qt.vector3d(0, 0, 1))
    // How a stand turns (Stand facing): lying on the panel, in step with
    // its tilt and turn, in the panel's own proportion (Panel, the
    // default), or level and facing the viewer, seen View
    // angle degrees from above (Viewer). Auto lies on a panel seen from 40
    // degrees or less above its plane and stays level on one seen from 60
    // degrees up, blending between.
    readonly property string pedestalFacing: String((sceneDefinition || ({})).pedestalFacing || "panel").toLowerCase()
    readonly property real pedestalViewAngle: bounded("pedestalViewAngle", 20, 0, 90)
    readonly property real panelElevation: Math.asin(Math.max(0, Math.min(1, Math.abs(iconForward.z)))) * 180 / Math.PI
    readonly property real pedestalPanelShare: pedestalFacing === "panel" ? 1
        : pedestalFacing === "viewer" ? 0
        : Math.max(0, Math.min(1, (60 - panelElevation) / 20))
    readonly property vector3d pedestalAxis: {
        const t = pedestalViewAngle * Math.PI / 180
        const level = Qt.vector3d(0, Math.cos(t), Math.sin(t))
        // The platform's normal as the viewer sees it, kept upright on the
        // screen where the camera looks from the other side.
        const lying = Qt.vector3d(iconRight.z, Math.abs(iconUp.z), Math.abs(iconForward.z))
        const share = pedestalPanelShare
        const mixed = level.times(1 - share).plus(lying.length() > 1e-6 ? lying.normalized().times(share) : level.times(share))
        return mixed.length() > 1e-6 ? mixed.normalized() : level
    }
    // Panel: each stand is fixed to the panel at its icon's place, as if it
    // were one more layer of the panel in pieces, so every tilt, turn, move
    // and rotation of the panel
    // carries it. It stays a stand, not a panel.
    readonly property bool pedestalOnPanel: pedestalDrawn && pedestalPanelShare >= 0.999
    // The camera and the point it looks at, in the entries' frame. A stand
    // fixed to the panel is turned by the small angle between the camera's
    // line to the panel's centre and its line to the stand, so every stand
    // round the ring shows the panel's own orientation rather than each a
    // slightly different side of itself.
    function toEntryFrame(point) {
        const offset = edgePanel && edgeFrame ? edgeContentPoint(point)
            : point.minus(sceneContent.position).times(1 / Math.max(1e-6, sceneContent.scale.x))
        const turn = root.layoutAngle * Math.PI / 180
        return Qt.vector3d(offset.x * Math.cos(turn) - offset.y * Math.sin(turn),
                           offset.x * Math.sin(turn) + offset.y * Math.cos(turn), offset.z)
    }
    // A point of the scene in an edge panel's content, its turn about the
    // screen's edge, its scale and its move undone (edgeFrame). It turns about
    // one axis only, x or y.
    function edgeContentPoint(point) {
        const moved = point.minus(edgeFrame.position)
        const ax = edgeFrame.turn.x * Math.PI / 180, ay = edgeFrame.turn.y * Math.PI / 180
        const x = moved.x * Math.cos(ay) - moved.z * Math.sin(ay)
        const z = moved.x * Math.sin(ay) + moved.z * Math.cos(ay)
        const unturned = Qt.vector3d(x, moved.y * Math.cos(ax) + z * Math.sin(ax),
                                     -moved.y * Math.sin(ax) + z * Math.cos(ax))
        return unturned.times(1 / Math.max(1e-6, edgeFrame.scale)).plus(edgeFrame.pivot)
    }
    readonly property vector3d cameraInEntries: toEntryFrame(camera.scenePosition)
    readonly property vector3d sightCentreInEntries: toEntryFrame(Qt.vector3d(0, 0, 0))
    readonly property real pedestalAxisTilt: Math.asin(Math.max(-1, Math.min(1, pedestalAxis.z))) * 180 / Math.PI
    readonly property real pedestalAxisLean: Math.atan2(pedestalAxis.x, pedestalAxis.y) * 180 / Math.PI
    // How tall a stand is per unit of its width, and its outline for its
    // axis per unit: how far it reaches up the screen, where its top's
    // middle is, how far it would reach below the platform's plane.
    readonly property real pedestalStretch: pedestalShape === "auto"
        ? bounded("iconElevation", 0.3, 0, 2) / Math.max(1e-6, 0.18 * pedestalSize)
        : pedestalKind === "column" ? 0.4 * pedestalThickness / 0.18 : pedestalThickness
    readonly property var pedestalExtentsUnit: PlatformGeometry.pedestalExtents(pedestalDrawn ? pedestalKind : "column",
        pedestalStretch, [pedestalAxis.x, pedestalAxis.y, pedestalAxis.z], iconRight.z, iconUp.z, iconForward.z)
    // A picture is worn as it is: in its own colours and solid, with no
    // tint, vertex shades, metal sheen or look glow laid over it. The rim
    // keeps the look's own colour and glow.
    function wearPicture(material) {
        material.baseColor = "#ffffff"
        material.vertexColours = false
        material.metalness = 0
        material.roughness = 0.6
        material.opacity = 1
        material.alphaBlend = false
        material.emissiveColor = "#000000"
        material.emissiveStrength = 0
        material.emissiveThroughTexture = false
        return material
    }
    // A look's material properties, as the platform takes them for a chosen
    // look: metalness, roughness, opacity and an energy look's glow.
    function materialForLook(look, colour) {
        const material = ({})
        material.baseColor = colour && colour.length ? colour : "#ffffff"
        material.metalness = ["metallic", "platform", "plate", "pedestal", "futuristic"].includes(look) ? 0.85 : 0
        material.roughness = ["glass", "floating-glass", "crystal"].includes(look) ? 0.12
            : look === "organic" || look === "minimal" ? 0.85 : 0.38
        material.opacity = LayoutEngine.themeStyle(look, "", 48).alpha
        if (["neon", "plasma", "lightning", "aurora", "futuristic", "magma"].includes(look)) {
            // Magma's light is the lava's own colours in its texture.
            material.emissiveColor = look === "magma" ? "#ffffff"
                : colour && colour.length ? colour : LayoutEngine.themeStyle(look, "", 48).stroke
            material.emissiveStrength = root.energyStrength(look)
            material.emissiveThroughTexture = true
        }
        if (clearLooks.includes(look)) {
            material.alphaBlend = true
            material.emissiveColor = colour && colour.length ? colour : "#ffffff"
        }
        return material
    }
    readonly property bool resourcesReady: sceneResources !== null
        && sceneResources.material !== undefined
        && sceneResources.material.format === "org.archdock.material"
        && platform.meshReady && partsReady
        && (generatedSpec !== null || (sceneResources.iconMesh !== undefined && iconResource.meshReady))
    readonly property bool textureRequired: Boolean(sceneDefinition && sceneDefinition.texture)
    readonly property bool textureReady: !textureRequired || textureImage.status === Image.Ready
        || (shownOnce && textureImage.status !== Image.Error)
    readonly property bool geometryWithinBudget: triangleCount * 3 <= Math.min(262144,
        Number((sceneResources || ({})).indexBudget || 262144))
    readonly property bool rendererReady: resourcesReady && textureReady && materialTextureReady && geometryWithinBudget
    readonly property string errorReason: !resourcesReady ? "scene3d-mesh-unavailable"
        : textureRequired && textureImage.status === Image.Error ? "scene3d-texture-unavailable"
        : !geometryWithinBudget ? "scene3d-resource-limit"
        : textureMaterial.length && materialImage.status === Image.Error ? "material-texture-unavailable"
        : !textureReady || !materialTextureReady ? "renderer-loading" : ""
    readonly property int entryTriangleCount: {
        let count = 0
        for (let index = 0; index < entryGeometry.length; ++index) {
            const visual = entryVisuals[index] ? entryVisuals[index].meshVisualItem : null
            count += 2 // Glyph.
            if (visual && !visual.tileRenderingEnabled) continue
            count += tileMeshes[index] ? tileMeshes[index].indexes.length / 3 : 2
            if (pedestalDrawn) count += pedestalTriangles
            if (!generatedSpec && bounded("iconElevation", 0.3, 0, 2) > 0) count += iconResource.triangleCount
            if (!visual || !visual.customTileActive) count += partTriangles(entryParts)
        }
        return count
    }
    readonly property var tileMeshes: entryVisuals.map(function(entry) {
        const visual = entry ? entry.meshVisualItem : null
        if (!visual || !visual.tileRenderingEnabled) return null
        const values = visual.tileSettings || ({})
        const logicalSize = Math.max(1, Number(visual.logicalSize || 52))
        const thickness = Math.max(0, Math.min(24, root.number(values, "iconTileThickness", 0)))
        if (thickness <= 0) return null
        let diameter = 0.92 * Number(visual.resolvedIconStyle.diameter || 1)
        if (!visual.customTileActive && visual.styledLayersActive) {
            const layer = visual.styleBaseItem.roleLayers.find(function(layer) {
                return layer.followsIconShape && layer.option !== "pedestal"
            })
            if (layer) diameter = (1 - 2 * Number(layer.inset || 0))
                * (layer.shape === "diamond" ? 0.96 : 1)
        }
        // A bubble takes the plate's place.
        if (bubbleWanted(visual)) return null
        return PlatformGeometry.tile({shape: visual.resolvedIconStyle.iconShape,
            thickness: 2 * thickness / logicalSize,
            bevel: 2 * Math.max(0, Math.min(12, root.number(values, "iconTileBevel", 0))) / logicalSize,
            diameter: diameter})
    })
    // How large an icon is drawn in its bubble: a little smaller in a bubble
    // with straight sides, which hug the icon's own, so it stands clear of
    // them; a ball round it is wider than the icon by its corners and
    // leaves room enough as it is.
    function bubbleIconShare(shape) {
        return String(shape) === "circle" ? 1 : 0.85
    }
    // Whether an entry has a bubble: every icon shown with a tile stands in
    // one, whatever the tile's shape.
    function bubbleWanted(visual) {
        return visual !== null && visual.tileRenderingEnabled
    }
    // How wide and how high an entry's bubble has to be to hold its icon
    // whole: wide enough for the icon's whole picture, whatever the shape
    // (a circle wider than the picture's diagonal, a diamond wider still),
    // with a margin for the outline; as deep as it is wide, so it is round
    // every way, and deeper still when the icon's volume and its rings at
    // its depth need it. In the tile's frame (1 = half the cell).
    function bubbleFit(visual) {
        const values = visual ? visual.tileSettings || ({}) : ({})
        const logicalSize = Math.max(1, Number(visual && visual.logicalSize || 52))
        const pictureScale = Math.max(0.25, Math.min(1.5, root.number(values, "iconTileIconScale", 100) / 100))
        const halfSide = 0.72 * pictureScale
        const offsetX = 2 * root.number(values, "iconTileIconOffsetX", 0) / logicalSize
        const offsetY = -2 * root.number(values, "iconTileIconOffsetY", 0) / logicalSize
        const shape = visual ? visual.resolvedIconStyle.iconShape : "rounded"
        const own = 0.92 * Number(visual && visual.resolvedIconStyle.diameter || 1)
        const diameter = Math.max(own, 1.08 * PlatformGeometry.containingScale(shape, halfSide, offsetX, offsetY))
        const mesh = visual ? visual.solidMesh : null
        const reach = mesh ? Number(mesh.reach || mesh.depth || 0) : 0
        // The icon's frame is a hundred units over 0.72 of the cell: a unit
        // is 0.0144 of half the cell.
        // Each half of the shell, from the icon's middle: as deep as the
        // bubble is wide (a ball round a circle, a rhombus with depth round a
        // diamond, never a flat lens), and past the icon's reach.
        const stretch = IconSolid.depthStretch(mesh, root.iconDepth)
        const height = Math.max(diameter, 0.15 + 1.25 * stretch * reach * 0.0144 * pictureScale)
        // The icon's middle: its picture's plane plus half its volume, in
        // the tile's frame (the glyph's plane sits a hundredth of the cell
        // before the tile, the volume's back on it; a hologram's volume
        // stands half as far forward), at the size it is drawn in its bubble.
        const middle = 0.02 + 0.02 * (mesh ? Number(mesh.depth || 0) : 0) * stretch
            * (root.iconHologram.on ? 0.5 : 1) * bubbleIconShare(shape)
        return { diameter: diameter, height: height, middle: middle }
    }
    // The bubble over each tile: the tile's outline risen as a dome round
    // the icon, standing on the tile's face.
    readonly property var bubbleMeshes: entryVisuals.map(function(entry) {
        const visual = entry ? entry.meshVisualItem : null
        if (!bubbleWanted(visual)) return null
        const fit = bubbleFit(visual)
        return cachedBubble(String(visual.resolvedIconStyle.iconShape), fit.height, fit.diameter)
    })
    // Bubbles already built, by what shapes them: icons with alike bubbles
    // share one, and whatever leaves an icon's bubble as it was (a hover, a
    // press, a new size) hands back the same mesh and builds nothing.
    property var bubbleCache: ({})
    function cachedBubble(shape, height, diameter) {
        const key = shape + "|" + height.toFixed(4) + "|" + diameter.toFixed(4)
        if (!bubbleCache[key]) {
            // A slider dragged through many sizes keeps only the latest.
            const keys = Object.keys(bubbleCache)
            if (keys.length >= 32) keys.forEach(function(old) { delete bubbleCache[old] })
            bubbleCache[key] = PlatformGeometry.bubble({shape: shape,
                height: Number(height.toFixed(4)), diameter: Number(diameter.toFixed(4))})
        }
        return bubbleCache[key]
    }
    // A bubble's glass: clear and glossy, a light tint of the tile's fill
    // colour (glass is paler than what it stands on), as clear as the tile's
    // opacity allows.
    function bubbleMaterial(visual) {
        const values = visual ? visual.tileSettings || ({}) : ({})
        const base = tileMaterial(visual)
        const tint = Qt.color(String(values.iconTileColor || "#334155"))
        return {format: "org.archdock.material",
            baseColor: Qt.rgba(0.62 + 0.38 * tint.r, 0.62 + 0.38 * tint.g, 0.62 + 0.38 * tint.b, 1),
            metalness: 0, roughness: 0.06,
            opacity: 0.4 * Math.max(0.05, Math.min(1, root.number(values, "iconTileOpacity", 0.8))),
            surfaceOpacity: 1, emissiveColor: base.emissiveColor, emissiveStrength: 0, look: "glass"}
    }
    function tileMaterial(visual) {
        const values = visual ? visual.tileSettings || ({}) : ({})
        const requested = String(values.iconTileMaterial || "minimal")
        const look = materialNames.includes(requested) ? requested : "minimal"
        const style = LayoutEngine.themeStyle(look, "", 52)
        const palette = PlatformGeometry.paletteFromStyle(style)
        const colour = Qt.rgba(palette.top[0],palette.top[1],palette.top[2],1)
        return {format: "org.archdock.material", baseColor: look === "minimal"
                ? String(values.iconTileColor || "#334155") : colour,
            metalness: ["metallic", "platform", "plate", "pedestal", "futuristic"].includes(look) ? 0.85 : 0,
            roughness: ["glass", "floating-glass", "crystal"].includes(look) ? 0.12
                : ["organic", "minimal"].includes(look) ? 0.85 : 0.38,
            opacity: style.alpha * (visual && visual.customTileActive
                ? Math.max(0, Math.min(1, root.number(values, "iconTileOpacity", 0.8))) : 1),
            surfaceOpacity: style.alpha, emissiveColor: colour,
            emissiveStrength: ["neon", "plasma", "lightning", "aurora", "futuristic"].includes(look) ? 0.2 : 0,
            look: look}
    }
    readonly property int triangleCount: platform.triangleCount
        + partTriangles(panelParts) + entryTriangleCount
    readonly property var qualityState: ({ quality: effectiveQuality,
        targetWidth: targetWidth, targetHeight: targetHeight,
        samples: effectiveQuality === "low" ? 1 : effectiveQuality === "high" ? 4 : 2 })
    readonly property var viewport: view
    // Set once by observeCompletedFrame() after a real window frame; a binding
    // to view.renderStats here re-evaluated on every rendered frame for nothing.
    readonly property bool frameRendered: completedFrameObserved
    property bool completedFrameObserved: false
    // Hit tests are asked for far more often than the scene changes: every
    // pointer move asks every entry's mask, and a test harness may sample
    // them continuously. Their answers are kept in a memo that is dropped
    // whenever anything they depend on changes, and the triangle test below
    // runs on flat arrays rather than a vector object per triangle.
    property int hitRevision: 0
    property var hitMemo: ({ revision: -1, values: ({}), count: 0 })
    // Flat corner coordinates of the meshes the hit tests walk, rebuilt only
    // when a mesh changes (plain arrays: the JavaScript engine's typed arrays
    // and weak maps stay out of this hot path).
    readonly property var platformTriangles: flatTriangles(platformMesh)
    function hitTestsChanged() { root.hitRevision++ }
    onPlatformMeshChanged: hitTestsChanged()
    onEntryGeometryChanged: hitTestsChanged()
    onEntryVisualsChanged: hitTestsChanged()
    onWidthChanged: hitTestsChanged()
    onHeightChanged: hitTestsChanged()
    onLayoutAngleChanged: hitTestsChanged()
    onMotionAllowedChanged: hitTestsChanged()
    onShownPitchChanged: hitTestsChanged()
    onShownYawChanged: hitTestsChanged()
    onShownRollChanged: hitTestsChanged()
    onShownPositionChanged: hitTestsChanged()
    onShownScaleChanged: hitTestsChanged()
    onPlatformScaleChanged: hitTestsChanged()
    onProjectedEntryGeometryChanged: hitTestsChanged()
    onProjectedCentreChanged: hitTestsChanged()
    onRendererReadyChanged: {
        hitTestsChanged()
        if (rendererReady) shownOnce = true
    }
    onCameraDistanceChanged: hitTestsChanged()
    onMechanismChanged: hitTestsChanged()
    onCollapseProgressChanged: hitTestsChanged()
    function hitKey(prefix, point) {
        return prefix + ":" + Math.round(point.x * 2) + ":" + Math.round(point.y * 2)
    }
    function hitMemoGet(key) {
        const memo = root.hitMemo
        if (memo.revision !== root.hitRevision) {
            memo.revision = root.hitRevision
            memo.values = ({})
            memo.count = 0
        }
        return memo.values[key]
    }
    function hitMemoSet(key, value) {
        const memo = root.hitMemo
        if (memo.count >= 512) {
            memo.values = ({})
            memo.count = 0
        }
        memo.values[key] = value
        memo.count++
        return value
    }
    // A mesh's triangles as one flat array of corner coordinates.
    function flatTriangles(mesh) {
        if (!mesh || !mesh.positions || !mesh.indexes) return null
        const positions = mesh.positions, indexes = mesh.indexes
        const flat = []
        for (let i = 0; i < indexes.length; ++i) {
            const vertex = positions[indexes[i]]
            flat.push(Number(vertex[0]), Number(vertex[1]), Number(vertex[2]))
        }
        return flat
    }
    property var projectedEntryGeometry: []
    // Where the dock is centred on screen: the platform's middle at its top,
    // the dock a folder opens away from.
    property point projectedCentre: Qt.point(width / 2, height / 2)
    // The platform's reach from its centre across and up, in its own units:
    // 1 for a ring or a theme's mesh; an ellipse is wider than it is tall.
    readonly property real platformReachX: generatedPlatform ? generatedPlatform.reachX : 1
    readonly property real platformReachY: generatedPlatform ? generatedPlatform.reachY : 1
    // How far an icon's foot floats above its place with a stand under it,
    // for an icon of the panel's cell size: the platform shrinks by that
    // much, so a stand and its icon at the top of the panel stay inside it.
    readonly property real standRise: {
        if (!pedestalDrawn) return 0
        const unit = iconCellSize * pedestalShapeScale * pedestalSize
        const gap = pedestalShape === "auto" ? iconCellSize * 0.02 : iconCellSize * (0.03 + pedestalLift * 0.3)
        // To a quarter of a pixel: the platform's size follows it, and its
        // last digits wobble as the panel turns (the camera's directions in
        // the panel's frame come from a single-precision rotation), which
        // would otherwise lay the whole scene out again on every frame of
        // the turn.
        return Math.round(Math.max(0, pedestalExtentsUnit.top * unit + gap - iconCellSize * 0.07) * 4) / 4
    }
    readonly property real platformScale: {
        if (edgePanel) return edgeUnit
        const fitted = Math.min(width / platformReachX, height / platformReachY) * 0.40
            / (1 + Math.abs(platformFold) * platformThickness * 0.4)
        // The icons' line is 0.8 of the platform's reach; above its top
        // stands the stand, the gap and the icon. Only what would leave the
        // panel is taken off the platform.
        const lineReach = Math.max(0.01, platformReachY * PlatformGeometry.trackRadius / PlatformGeometry.reach)
        const excess = fitted * lineReach + standRise + iconCellSize * 1.05 - height / 2 * 0.98
        return standRise > 0 && excess > 0 ? Math.max(fitted * 0.4, fitted - excess / lineReach) : fitted
    }
    // Where the icons stand: a generated platform's flat top is level along
    // its centre line even when bent; a theme's top is its highest point.
    readonly property real platformTop: baseTopHeight * platformScale * platformThickness
    // The ring the icons stand on: the middle of the platform's flat top.
    readonly property real entryTrackRadius: platformScale * (generatedPlatform
        ? generatedPlatform.track : bounded("entryRadius", 0.84, 0.1, 1))

    // The scene's own transform from the Panels > 3D page. Pitch and yaw orbit
    // the camera, as they always have; roll turns it about its view axis.
    // Position moves the platform and its icons by a fraction of the room the
    // panel has around them, and scale resizes them, so no value can carry
    // the platform out of its panel. Everything is drawn, picked and pressed
    // through the same nodes, so input and anchors follow without a copy.
    readonly property bool gizmoDragging: dragStart !== null

    // Desktop 3D editing. In edit mode the platform carries Blender-style
    // handles, one set per operation: arrows move it along X (red), Y (green)
    // and Z (blue); the red and green rings tilt it (pitch and yaw) and the
    // white ring turns the view (roll); the cube handles scale it. Ctrl snaps,
    // Shift slows the drag for fine work, and Esc or the right button cancels.
    // A finished drag is reported once through transformEdited(); the host
    // keeps it as a draft until the edit is applied or cancelled.
    property bool editMode: false
    property string gizmoMode: "move"
    signal transformEdited(var values)
    // While a handle is dragged, and until the panel's settings come back with
    // the result, these values stand in for the saved ones.
    property var dragOverride: null
    property var dragStart: null
    readonly property real gizmoLength: platformScale * 0.55
    readonly property var transformKeys: ({
        cameraPitch: "scene3DCameraPitch", cameraYaw: "scene3DCameraYaw", roll: "scene3DRoll",
        positionX: "scene3DPositionX", positionY: "scene3DPositionY", positionZ: "scene3DPositionZ",
        scale: "scene3DScale", scaleX: "scene3DScaleX", scaleY: "scene3DScaleY", thickness: "scene3DThickness"
    })
    function transformValue(key, fallback, minimum, maximum) {
        const override = dragOverride && dragOverride[key] !== undefined ? Number(dragOverride[key]) : NaN
        return Number.isFinite(override) ? Math.max(minimum, Math.min(maximum, override))
            : bounded(key, fallback, minimum, maximum)
    }
    function wrapDegrees(value) { return ((value + 180) % 360 + 360) % 360 - 180 }
    function savedTransform() {
        return {
            cameraPitch: bounded("cameraPitch", 25, -180, 180), cameraYaw: bounded("cameraYaw", 0, -180, 180),
            roll: bounded("roll", 0, -180, 180), positionX: bounded("positionX", 0, -1, 1),
            positionY: bounded("positionY", 0, -1, 1), positionZ: bounded("positionZ", 0, -1, 1),
            scale: bounded("scale", 1, 0.05, 20), scaleX: bounded("scaleX", 1, 0.05, 20),
            scaleY: bounded("scaleY", 1, 0.05, 20), thickness: bounded("thickness", 1, 0.05, 20)
        }
    }
    function gizmoHandleAt(x, y) {
        const hits = pickAllAt(x, y)
        for (let index = 0; index < hits.length; ++index) {
            const name = hits[index].objectHit ? String(hits[index].objectHit.objectName) : ""
            const match = /^gizmo-(move|rotate|scale)-(x|y|z|view|all)/.exec(name)
            if (match && match[1] === gizmoMode)
                return { mode: match[1], axis: match[2] }
        }
        return null
    }
    function beginGizmoDrag(handle, x, y) {
        const origin = gizmo.scenePosition
        const centre = root.projectScenePoint(origin)
        const values = savedTransform()
        if (dragOverride)
            for (const key of Object.keys(dragOverride)) values[key] = Number(dragOverride[key])
        dragStart = { mode: handle.mode, axis: handle.axis, pointer: Qt.point(x, y),
                      centre: Qt.point(centre.x, centre.y), origin: origin, values: values,
                      endOn: handle.mode === "move" && gizmoAxisEndOn(handle.axis) }
        dragOverride = Object.assign({}, values)
    }
    function gizmoAxisVector(axis) {
        return axis === "x" ? Qt.vector3d(1, 0, 0)
            : axis === "y" ? Qt.vector3d(0, 1, 0) : Qt.vector3d(0, 0, 1)
    }
    // Whether a move arrow points at the viewer, as drawn now.
    function gizmoAxisEndOn(axis) {
        const origin = gizmo.scenePosition
        const from = root.projectScenePoint(origin)
        const to = root.projectScenePoint(origin.plus(gizmoAxisVector(axis).times(gizmoLength)))
        return GizmoMath.axisEndOn(Qt.point(from.x, from.y), Qt.point(to.x, to.y))
    }
    // The move arrows drawn end-on, kept current while editing so the
    // toolbar can say how to drag them.
    property var endOnAxes: []
    function updateEndOnAxes() {
        const axes = editMode && rendererReady
            ? ["x", "y", "z"].filter(function(axis) { return gizmoAxisEndOn(axis) }) : []
        if (axes.join() !== endOnAxes.join()) endOnAxes = axes
    }
    function updateGizmoDrag(x, y, modifiers) {
        const start = dragStart
        const settings = GizmoMath.modifierSettings(modifiers)
        const delta = Qt.point((x - start.pointer.x) * settings.precision,
                               (y - start.pointer.y) * settings.precision)
        const pointer = Qt.point(start.pointer.x + delta.x, start.pointer.y + delta.y)
        const next = Object.assign({}, start.values)
        if (start.mode === "orbit") {
            // The platform's body: across turns it, up and down tilts it.
            const turned = GizmoMath.orbit(start.values.cameraPitch, start.values.cameraYaw,
                                           delta, settings.snap)
            next.cameraPitch = turned.pitch
            next.cameraYaw = turned.yaw
        } else if (start.mode === "move") {
            const axis = gizmoAxisVector(start.axis)
            const from = root.projectScenePoint(start.origin)
            const to = root.projectScenePoint(start.origin.plus(axis.times(gizmoLength)))
            // An arrow seen end-on has no direction on screen: up and down
            // move it instead of a drag that would do nothing.
            const travel = start.endOn ? GizmoMath.endOnTravel(delta, gizmoLength, 120)
                : GizmoMath.axisTravel(Qt.point(from.x, from.y), Qt.point(to.x, to.y),
                                       gizmoLength, delta)
            // Scene units back to the fractions the settings store.
            const perspective = (cameraDistance - targetDepth) / cameraDistance
            const span = start.axis === "x" ? roomX * perspective
                : start.axis === "y" ? roomY * perspective : platformScale * 0.5
            const key = start.axis === "x" ? "positionX" : start.axis === "y" ? "positionY" : "positionZ"
            let value = span > 1e-6 ? start.values[key] + travel / span : start.values[key]
            if (settings.snap) value = GizmoMath.snapped(value, 0.05)
            next[key] = GizmoMath.clamp(value, -1, 1)
        } else if (start.mode === "rotate") {
            if (start.axis === "x") {
                let pitch = start.values.cameraPitch - delta.y * 0.25
                if (settings.snap) pitch = GizmoMath.snapped(pitch, 15)
                next.cameraPitch = wrapDegrees(pitch)
            } else {
                let angle = start.axis === "y"
                    ? start.values.cameraYaw + delta.x * 0.25
                    : start.values.roll + GizmoMath.sweptDegrees(start.centre, start.pointer, pointer)
                if (settings.snap) angle = GizmoMath.snapped(angle, 15)
                next[start.axis === "y" ? "cameraYaw" : "roll"] = wrapDegrees(angle)
            }
        } else {
            // The arms scale their own way - sideways, up and down, the
            // platform's thickness - and the cube in the middle all ways.
            const key = start.axis === "x" ? "scaleX" : start.axis === "y" ? "scaleY"
                : start.axis === "z" ? "thickness" : "scale"
            let scale = start.values[key] * (start.axis === "all" ? GizmoMath.uniformScaleRatio(delta)
                : GizmoMath.scaleRatio(start.centre, start.pointer, pointer))
            if (settings.snap) scale = GizmoMath.snapped(scale, 0.05)
            next[key] = GizmoMath.clamp(scale, 0.05, 20)
        }
        dragOverride = next
    }
    function finishGizmoDrag() {
        const start = dragStart
        const result = dragOverride || ({})
        dragStart = null
        const values = {}
        for (const key of Object.keys(transformKeys))
            if (Math.abs(Number(result[key]) - Number(start.values[key])) > 1e-6)
                values[transformKeys[key]] = Number(result[key])
        if (Object.keys(values).length === 0) {
            dragOverride = null
            return
        }
        overrideRelease.restart()
        transformEdited(values)
    }
    function cancelGizmoDrag() {
        dragStart = null
        dragOverride = null
    }
    // The override gives way once the settings carry the edited values, or
    // after a while if they never do.
    onSceneDefinitionChanged: {
        if (dragStart || !dragOverride) return
        const saved = savedTransform()
        if (Object.keys(dragOverride).every(function(key) {
                return Math.abs(Number(saved[key]) - Number(dragOverride[key])) < 1e-3 }))
            dragOverride = null
    }
    onEditModeChanged: if (!editMode) cancelGizmoDrag()
    Timer {
        id: overrideRelease
        interval: 3000
        onTriggered: if (!root.dragStart) root.dragOverride = null
    }
    readonly property bool transitionsEnabled: !reducedMotion && !gizmoDragging
        && (sceneDefinition || ({})).transitions !== false
    readonly property real cameraDistance: Math.max(1, edgePanel ? edgeAcross : height)
        / (2 * Math.tan(panelFieldOfView * Math.PI / 360))
    // The platform and its icons reach a little past the platform's radius.
    readonly property real sceneExtentX: platformScale * platformReachX * 1.05 * targetScaleX
    readonly property real sceneExtentY: platformScale * platformReachY * 1.05 * targetScaleY
    // An edge panel is neither turned, moved nor scaled by these: it stands
    // on its edge, tilted by edgeFrame.
    readonly property real targetScale: edgePanel ? 1 : transformValue("scale", 1, 0.05, 20)
    // Sideways and up and down on its own, in the panel's own frame: the
    // platform and where its icons stand stretch, the icons keep their
    // shape.
    readonly property real targetScaleX: edgePanel ? 1 : transformValue("scaleX", 1, 0.05, 20)
    readonly property real targetScaleY: edgePanel ? 1 : transformValue("scaleY", 1, 0.05, 20)
    readonly property real fitLimit: Math.min(width / 2 / Math.max(1, sceneExtentX),
                                              height / 2 / Math.max(1, sceneExtentY))
    // Nearer is larger: depth stops where the platform would outgrow the panel.
    readonly property real targetDepth: edgePanel ? 0
        : Math.min(transformValue("positionZ", 0, -1, 1) * platformScale * 0.5,
            cameraDistance * Math.max(0, 1 - targetScale / Math.max(targetScale, fitLimit)))
    readonly property real apparentScale: targetScale * cameraDistance
        / Math.max(1, cameraDistance - targetDepth)
    readonly property real roomX: Math.max(0, width / 2 - sceneExtentX * apparentScale)
    readonly property real roomY: Math.max(0, height / 2 - sceneExtentY * apparentScale)
    readonly property vector3d targetPosition: edgePanel ? Qt.vector3d(0, 0, 0) : Qt.vector3d(
        transformValue("positionX", 0, -1, 1) * roomX * (cameraDistance - targetDepth) / cameraDistance,
        transformValue("positionY", 0, -1, 1) * roomY * (cameraDistance - targetDepth) / cameraDistance,
        targetDepth)
    // An edge panel is seen straight on: its tilt turns the platform itself.
    readonly property real targetPitch: edgePanel ? 0 : transformValue("cameraPitch", 25, -180, 180)
    readonly property real targetYaw: edgePanel ? 0 : transformValue("cameraYaw", 0, -180, 180)
    readonly property real targetRoll: edgePanel ? 0 : transformValue("roll", 0, -180, 180)
    // What is drawn. It eases toward the targets when transitions are on and
    // jumps to them otherwise. One frame-driven easing instead of a Behavior
    // per value: Qt creates a Behavior's animation on its first use, which
    // would make the scene's objects depend on what it has been through.
    property real shownPitch: 0
    property real shownYaw: 0
    property real shownRoll: 0
    property vector3d shownPosition: Qt.vector3d(0, 0, 0)
    property real shownScale: 1
    property real shownScaleX: 1
    property real shownScaleY: 1
    // Exact: the easing lands each value on its target, then stops.
    readonly property bool transformSettled: shownPitch === targetPitch
        && shownYaw === targetYaw && shownRoll === targetRoll && shownScale === targetScale
        && shownScaleX === targetScaleX && shownScaleY === targetScaleY
        && shownPosition.x === targetPosition.x && shownPosition.y === targetPosition.y
        && shownPosition.z === targetPosition.z
    function angleStep(from, to) { return ((to - from) % 360 + 540) % 360 - 180 }
    function settleTransform() {
        shownPitch = targetPitch
        shownYaw = targetYaw
        shownRoll = targetRoll
        shownPosition = targetPosition
        shownScale = targetScale
        shownScaleX = targetScaleX
        shownScaleY = targetScaleY
    }
    function followTransform() { if (!transitionsEnabled) settleTransform() }
    function stepTransform(seconds) {
        // About 95 percent of the way in a quarter of a second, the short way
        // round for angles; within a hair of the target it lands on it.
        const share = 1 - Math.exp(-12 * Math.max(0, seconds))
        const toward = function(from, to, step, epsilon) {
            return Math.abs(step) < epsilon ? to : from + step * share
        }
        shownPitch = toward(shownPitch, targetPitch, angleStep(shownPitch, targetPitch), 1e-3)
        shownYaw = toward(shownYaw, targetYaw, angleStep(shownYaw, targetYaw), 1e-3)
        shownRoll = toward(shownRoll, targetRoll, angleStep(shownRoll, targetRoll), 1e-3)
        const offset = targetPosition.minus(shownPosition)
        shownPosition = offset.length() < 1e-3 ? targetPosition
            : shownPosition.plus(offset.times(share))
        shownScale = toward(shownScale, targetScale, targetScale - shownScale, 1e-4)
        shownScaleX = toward(shownScaleX, targetScaleX, targetScaleX - shownScaleX, 1e-4)
        shownScaleY = toward(shownScaleY, targetScaleY, targetScaleY - shownScaleY, 1e-4)
    }
    onTargetPitchChanged: followTransform()
    onTargetYawChanged: followTransform()
    onTargetRollChanged: followTransform()
    onTargetPositionChanged: followTransform()
    onTargetScaleChanged: followTransform()
    // Later, not inside the scene's extent being worked out from them.
    onTargetScaleXChanged: Qt.callLater(root.followTransform)
    onTargetScaleYChanged: Qt.callLater(root.followTransform)
    onShownScaleXChanged: hitTestsChanged()
    onShownScaleYChanged: hitTestsChanged()
    onTransitionsEnabledChanged: followTransform()
    Component.onCompleted: {
        settleTransform()
        holdHologramClock(hologramMoving)
    }
    FrameAnimation {
        running: root.transitionsEnabled && !root.transformSettled
        onTriggered: root.stepTransform(frameTime)
    }
    // A slow rise and fall, off unless chosen, and never under reduced motion.
    readonly property bool floatRunning: (sceneDefinition || ({})).float === true && !edgePanel
        && !reducedMotion && motionAllowed && !gizmoDragging
    property real floatOffset: 0
    onFloatRunningChanged: if (!floatRunning) floatOffset = 0
    SequentialAnimation on floatOffset {
        running: root.floatRunning
        loops: Animation.Infinite
        NumberAnimation { to: root.platformScale * 0.03; duration: 1800; easing.type: Easing.InOutSine }
        NumberAnimation { to: -root.platformScale * 0.03; duration: 3600; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0; duration: 1800; easing.type: Easing.InOutSine }
    }

    // Qt 6.11 picks custom meshes against their bounding volume. Refine that
    // coarse hit against the same validated triangles the GPU draws, including
    // a bend, so holes and hidden rear entries do not receive clicks.
    function meshHitDistance(model, mesh, point, flat) {
        if (!model || !mesh || !mesh.positions || !mesh.indexes) return Infinity
        const triangles = flat || flatTriangles(mesh)
        if (!triangles) return Infinity
        const sceneOrigin = root.unprojectViewPoint(Qt.vector3d(point.x, point.y, 0))
        const origin = model.mapPositionFromScene(sceneOrigin)
        const far = model.mapPositionFromScene(root.unprojectViewPoint(Qt.vector3d(point.x, point.y, 1000)))
        let dx = far.x - origin.x, dy = far.y - origin.y, dz = far.z - origin.z
        const length = Math.hypot(dx, dy, dz)
        if (!(length > 1e-9)) return Infinity
        dx /= length; dy /= length; dz /= length
        const ox = origin.x, oy = origin.y, oz = origin.z
        // Moeller-Trumbore against every triangle; the nearest hit along the
        // ray is the nearest in the scene as well, since the model transform
        // keeps the ray a straight line.
        let nearest = Infinity
        for (let i = 0; i < triangles.length; i += 9) {
            const ax = triangles[i], ay = triangles[i + 1], az = triangles[i + 2]
            const abx = triangles[i + 3] - ax, aby = triangles[i + 4] - ay, abz = triangles[i + 5] - az
            const acx = triangles[i + 6] - ax, acy = triangles[i + 7] - ay, acz = triangles[i + 8] - az
            const px = dy * acz - dz * acy, py = dz * acx - dx * acz, pz = dx * acy - dy * acx
            const determinant = abx * px + aby * py + abz * pz
            if (Math.abs(determinant) < 1e-8) continue
            const tx = ox - ax, ty = oy - ay, tz = oz - az
            const u = (tx * px + ty * py + tz * pz) / determinant
            if (u < 0 || u > 1) continue
            const qx = ty * abz - tz * aby, qy = tz * abx - tx * abz, qz = tx * aby - ty * abx
            const v = (dx * qx + dy * qy + dz * qz) / determinant
            if (v < 0 || u + v > 1) continue
            const along = (acx * qx + acy * qy + acz * qz) / determinant
            if (along >= 0 && along < nearest) nearest = along
        }
        if (nearest === Infinity) return Infinity
        const hit = model.mapPositionToScene(Qt.vector3d(ox + dx * nearest, oy + dy * nearest, oz + dz * nearest))
        return hit.minus(sceneOrigin).length()
    }

    // Each icon's picture, a #Rectangle in its own units, as two triangles.
    readonly property var pictureQuad: ({ positions: [[-50, -50, 0], [50, -50, 0], [50, 50, 0], [-50, 50, 0]],
                                          indexes: [0, 1, 2, 0, 2, 3] })
    readonly property var pictureTriangles: flatTriangles(pictureQuad)
    // An icon drawn smaller in its bubble keeps its full picture square for
    // the pointer.
    readonly property var bubblePictureQuad: ({ positions: [[-50 / 0.85, -50 / 0.85, 0], [50 / 0.85, -50 / 0.85, 0],
                                                [50 / 0.85, 50 / 0.85, 0], [-50 / 0.85, 50 / 0.85, 0]],
                                                indexes: [0, 1, 2, 0, 2, 3] })
    readonly property var bubblePictureTriangles: flatTriangles(bubblePictureQuad)
    // The icons' pictures under a point, nearest first, through the camera's
    // own frame like the platform's. Qt's picking (View3D.pickAll) answers
    // with the projection from before the view last changed shape until it
    // renders again, and a static scene keeps that answer: on the desktop a
    // panel whose view had just grown round it took no wheel over its icons.
    function pictureHits(point) {
        const hits = []
        for (let index = 0; index < worldEntries.count; ++index) {
            const node = worldEntries.objectAt(index)
            if (!node || !node.visible || !node.glyphModel || !node.glyphModel.visible) continue
            const shrunk = node.iconShare < 1
            const distance = meshHitDistance(node.glyphModel, shrunk ? bubblePictureQuad : pictureQuad, point,
                shrunk ? bubblePictureTriangles : pictureTriangles)
            if (Number.isFinite(distance)) hits.push({ index: index, distance: distance })
        }
        hits.sort(function(a, b) { return a.distance - b.distance })
        return hits
    }
    function entryPointVisible(index, point) {
        const key = hitKey((editMode ? "E" : "e") + index, point)
        const known = hitMemoGet(key)
        if (known !== undefined) return known
        return hitMemoSet(key, entryPointVisibleUncached(index, point))
    }
    function entryPointVisibleUncached(index, point) {
        if (!motionAllowed || !worldEntries.objectAt(index) || !worldEntries.objectAt(index).visible)
            return false
        const hits = pictureHits(point)
        const own = hits.find(function(hit) { return hit.index === index })
        if (!own) return false
        const distance = own.distance
        if (meshHitDistance(platform, platformMesh, point, platformTriangles) < distance - 0.01) return false
        for (let i = 0; i < panelPartNodes.count; ++i) {
            const part = panelPartNodes.objectAt(i)
            if (part && part.visible && meshHitDistance(part, part.meshData, point) < distance - 0.01)
                return false
        }
        // Overlapping entries are picked in the same near-to-far order as drawn.
        return hits[0].index === index
    }

    // Whether the platform or one of its parts is nearer than this entry's
    // glyph at the point. Stands and icons rise clear of the platform, so
    // none is ever hidden in it; the renderer test proves it.
    function entryPointBehindPlatform(index, point) {
        const own = pictureHits(point).find(function(hit) { return hit.index === index })
        if (!own) return false
        const distance = own.distance
        if (meshHitDistance(platform, platformMesh, point, platformTriangles) < distance - 0.01) return true
        for (let i = 0; i < panelPartNodes.count; ++i) {
            const part = panelPartNodes.objectAt(i)
            if (part && part.visible && meshHitDistance(part, part.meshData, point) < distance - 0.01)
                return true
        }
        return false
    }

    function containsInputPoint(point) {
        const key = hitKey(editMode ? "C" : "c", point)
        const known = hitMemoGet(key)
        if (known !== undefined) return known
        return hitMemoSet(key, containsInputPointUncached(point))
    }
    function containsInputPointUncached(point) {
        if (!motionAllowed) return false
        if (Number.isFinite(meshHitDistance(platform, platformMesh, point, platformTriangles))) return true
        for (const hit of pictureHits(point))
            if (entryPointVisible(hit.index, point)) return true
        // The handles while editing: Qt's picking, only while a session runs.
        if (editMode) {
            const hits = pickAllAt(point.x, point.y)
            for (let i = 0; i < hits.length; ++i)
                if (hits[i].objectHit && String(hits[i].objectHit.objectName).startsWith("gizmo-")) return true
        }
        return false
    }
    // "Along the dock": the platform's own icon track, projected like the
    // icons, then moved `outward` screen pixels out of the drawn dock, as
    // `count` samples spanning `span` radians around the folder. `scale` is
    // the perspective size there relative to the folder's own.
    // Scene positions to the panel's own pixels and back, through the
    // camera's own frame: one scene unit is one pixel at the depth of the
    // camera's target, wherever the view's margins stand, and z is the depth
    // from the near plane, as the camera's viewport mapping gives it. No
    // projection matrix is read: View3D's mapping overloads recalculate the
    // render camera's shared projection from the GUI thread, and the one the
    // renderer submitted lags a frame behind a view that has just grown.
    function projectScenePoint(point) {
        const local = camera.mapPositionFromScene(point)
        const depth = -local.z
        if (!(depth > 1e-6)) return Qt.vector3d(NaN, NaN, depth - camera.clipNear)
        const factor = cameraDistance / depth
        return Qt.vector3d(width / 2 + local.x * factor, height / 2 - local.y * factor,
                           depth - camera.clipNear)
    }
    function unprojectViewPoint(point) {
        const depth = camera.clipNear + point.z
        const factor = depth / Math.max(1e-6, cameraDistance)
        return camera.mapPositionToScene(Qt.vector3d((point.x - width / 2) * factor,
            (height / 2 - point.y) * factor, -depth))
    }
    // The view's own picking, at a point in the panel's coordinates.
    function pickAllAt(x, y) { return view.pickAll(x + overflowX, y + overflowY) }
    // The curve a folder's contents follow ("Along the dock"): the dock's
    // own track round the folder, `outward` pixels further out on the
    // platform's own plane, so the curve keeps the panel's tilt - a step out
    // on the far side of a tilted panel looks as short as the panel makes it
    // - and each child stands on it as the dock's icons stand on theirs.
    // `count` samples span `span` radians, centred on the folder; `scale` is
    // how large a child is drawn there, 1 at the folder's depth. A folder
    // tries its curve at several distances out of the dock: the track is
    // traced once per opening, and each distance is only arithmetic on it.
    property var folderTrackMemo: null
    function forgetFolderTrack() { folderTrackMemo = null }
    function folderTrackSamples(entryIndex, outward, span, count) {
        const key = [entryIndex, span, count, width, height].join("|")
        let base = folderTrackMemo && folderTrackMemo.key === key ? folderTrackMemo.base : null
        if (!base) {
            base = folderTrackBase(entryIndex, span, count)
            folderTrackMemo = { key: key, base: base }
            Qt.callLater(root.forgetFolderTrack)
        }
        const distance = Number(outward) || 0
        const result = []
        for (const sample of base.samples) {
            // The child's middle in the camera's frame, projected as
            // projectScenePoint() does.
            const depth = -(sample.at.z + sample.out.z * distance)
            if (!(depth > camera.clipNear + 1e-6)) continue
            const factor = cameraDistance / depth
            result.push({ x: width / 2 + (sample.at.x + sample.out.x * distance) * factor,
                          y: height / 2 - (sample.at.y + sample.out.y * distance) * factor,
                          scale: Math.min(base.biggest, base.depth / (depth - camera.clipNear)) })
        }
        return result
    }
    // The dock's track round the folder in the camera's frame: at each
    // sample where a child's middle is half an icon inside the platform's
    // rim, and how it moves for each pixel further out. A child's size
    // follows its depth, as the dock's icons' sizes do, but is never bigger
    // than the dock's nearest icon: the opening is a window before the
    // scene, and close to the camera the perspective would blow a child up
    // past anything the dock shows.
    function folderTrackBase(entryIndex, span, count) {
        const empty = { samples: [], depth: 1, biggest: Infinity }
        const node = worldEntries.objectAt(entryIndex)
        if (!rendererReady || !node || !node.inputAnchor || count < 2) return empty
        const anchor = node.inputAnchor
        // A child stands toward the camera's up from where it stands, by the
        // same offset as the folder's own icon.
        const foot = sceneContent.mapPositionFromScene(node.scenePosition)
        const lift = sceneContent.mapPositionFromScene(anchor.scenePosition).minus(foot)
        // The track in the panel's own frame, unstretched: a circle there is
        // the stretched dock's ellipse once stretched back.
        const own = entryFrame.mapPositionFromScene(node.scenePosition)
        const unstretched = Qt.vector3d(own.x / Math.max(1e-6, shownScaleX), own.y / Math.max(1e-6, shownScaleY), own.z)
        const radius = Math.hypot(unstretched.x, unstretched.y)
        if (radius <= 0) return empty
        const angle = Math.atan2(unstretched.y, unstretched.x)
        // A child a step out stands that far beyond the platform's rim, not
        // the icons' line on it: the curve starts half an icon inside the
        // rim along the folder's own direction (the rim measured on the
        // stretched platform, carried back to the unstretched track).
        const reach = Math.hypot(foot.x, foot.y)
        const rim = reach > 0 ? root.folderRimDistance(foot.x / reach, foot.y / reach, reach) * radius / reach : radius
        const start = Math.max(radius, rim - node.size / 2)
        const frame = root.cameraMap(function(x, y) {
            return sceneContent.mapPositionFromScene(entryFrame.mapPositionToScene(Qt.vector3d(x, y, own.z))).plus(lift)
        })
        const samples = []
        for (let index = 0; index < count; ++index) {
            const turn = angle - span / 2 + span * index / (count - 1)
            const x = Math.cos(turn) * shownScaleX, y = Math.sin(turn) * shownScaleY
            samples.push({ at: frame.at(start * x, start * y), out: frame.step(x, y) })
        }
        const drawn = projectedEntryGeometry[entryIndex]
        const tallest = Math.max.apply(null, projectedEntryGeometry.map(function(rect) { return Number(rect.height) || 0 }))
        return { samples: samples, depth: root.projectScenePoint(anchor.scenePosition).z,
                 biggest: drawn && drawn.height > 0 && tallest > 0 ? tallest / drawn.height : Infinity }
    }
    // How much of its track the dock's icons get as drawn, against the track
    // in the panel's own pixels, for neighbours that need their drawn size
    // and `gap` pixels between them. By default the panel at its size as
    // drawn, however it is turned: the platform's track and its icons at the
    // platform's middle. `asSeen` measures what the camera shows instead:
    // less where a tilt shortens the track or far icons stand closer, at the
    // worst place along it (a closed track's icons turn all the way round
    // it, an open one's stay between its ends), for neighbours as far apart
    // as they would stand once it is full.
    function seenTrackFactor(gap, closed, asSeen) {
        const node = worldEntries.count > 0 ? worldEntries.objectAt(0) : null
        const drawn = projectedEntryGeometry[0]
        if (!rendererReady || !node || !node.inputAnchor || !drawn || !(drawn.height > 0)) return 1
        const size = Math.max(1, Number(node.size) || 1)
        const sx = Math.max(1e-6, shownScaleX), sy = Math.max(1e-6, shownScaleY)
        const own = entryFrame.mapPositionFromScene(node.scenePosition)
        const radius = Math.hypot(own.x / sx, own.y / sy)
        const plane = Math.max(1, layoutTrackRadius)
        if (!(radius > 0)) return 1
        const foot = sceneContent.mapPositionFromScene(node.scenePosition)
        const lift = sceneContent.mapPositionFromScene(node.inputAnchor.scenePosition).minus(foot)
        const frame = root.cameraMap(function(x, y) {
            return sceneContent.mapPositionFromScene(entryFrame.mapPositionToScene(Qt.vector3d(x, y, own.z))).plus(lift)
        })
        // A Möbius strip's track is its ∞, its strands at their heights;
        // every other one a circle (or ellipse) through the first icon.
        const mobius = generatedPlatform !== null && generatedPlatform.shape === "mobius"
        const trackScale = mobius ? generatedPlatform.track * platformScale : radius
        const ownLift = mobius ? PlatformGeometry.standLift("mobius", (entryGeometry[0] || ({})).progress) : 0
        const raised = root.cameraMap(function(x, y) {
            return sceneContent.mapPositionFromScene(entryFrame.mapPositionToScene(Qt.vector3d(x, y, own.z + 1))).plus(lift)
        }).at(0, 0)
        const level = frame.at(0, 0)
        const upward = { x: raised.x - level.x, y: raised.y - level.y, z: raised.z - level.z }
        // A place along the track `share` of the way round: in the camera's
        // frame, and in the panel's own pixels.
        const trackAt = function(share, from, to) {
            if (mobius) {
                const at = LayoutEngine.mobiusPoint(share, trackScale * LayoutEngine.mobiusReach)
                const own2d = LayoutEngine.mobiusPoint(share, plane * LayoutEngine.mobiusReach)
                const rise = (PlatformGeometry.standLift("mobius", share) - ownLift) * platformScale
                const base = frame.at(at.x * sx, -at.y * sy)
                return { point: { x: base.x + upward.x * rise, y: base.y + upward.y * rise, z: base.z + upward.z * rise },
                         flat: { x: own2d.x * sx, y: own2d.y * sy } }
            }
            const turn = from + (to - from) * share
            return { point: frame.at(radius * Math.cos(turn) * sx, radius * Math.sin(turn) * sy),
                     flat: { x: plane * Math.cos(turn) * sx, y: plane * Math.sin(turn) * sy } }
        }
        const nearDepth = Number(drawn.depth) + camera.clipNear
        if (!asSeen) {
            const middle = -frame.at(0, 0).z
            if (!(middle > camera.clipNear + 1e-6)) return 1
            const length = function(v) { return Math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z) }
            const unit = (length(frame.step(1, 0)) + length(frame.step(0, 1))) / 2 * cameraDistance / middle
            const icon = drawn.height * nearDepth / middle
            const factor = trackScale * unit / plane * (size + gap) / (icon + gap)
            return Number.isFinite(factor) && factor > 0 ? Math.max(0.05, Math.min(1.5, factor)) : 1
        }
        // An open track runs between its end icons: the circle less the
        // widest gap between neighbouring icons.
        let from = 0, to = 2 * Math.PI
        if (!closed && worldEntries.count > 1) {
            const turns = []
            for (let index = 0; index < worldEntries.count; ++index) {
                const entry = worldEntries.objectAt(index)
                if (!entry) continue
                const at = entryFrame.mapPositionFromScene(entry.scenePosition)
                turns.push(Math.atan2(at.y / sy, at.x / sx))
            }
            turns.sort(function(a, b) { return a - b })
            let widest = turns[0] + 2 * Math.PI - turns[turns.length - 1], after = 0
            for (let index = 1; index < turns.length; ++index)
                if (turns[index] - turns[index - 1] > widest) {
                    widest = turns[index] - turns[index - 1]
                    after = index
                }
            from = turns[after]
            to = from + 2 * Math.PI - widest
        }
        // The track as drawn, in fine steps: where each place is seen, how
        // large an icon is drawn there (it goes with the distance from the
        // camera) and how far along the track it lies in the panel's pixels.
        const places = []
        let along = 0, previous = null
        for (let step = 0; step <= 360; ++step) {
            const sample = trackAt(step / 360, from, to)
            const point = sample.point
            const depth = -point.z
            if (!(depth > camera.clipNear + 1e-6)) return 1
            if (previous)
                along += Math.hypot(sample.flat.x - previous.x, sample.flat.y - previous.y)
            previous = sample.flat
            const factor = cameraDistance / depth
            places.push({ x: point.x * factor, y: -point.y * factor, icon: drawn.height * nearDepth / depth,
                          along: along })
        }
        const length = along
        if (!(length > 0)) return 1
        const placeAt = function(distance) {
            let low = 0, high = places.length - 1
            while (high - low > 1) {
                const middle = (low + high) >> 1
                if (places[middle].along <= distance) low = middle
                else high = middle
            }
            const a = places[low], b = places[high]
            const share = b.along > a.along ? (distance - a.along) / (b.along - a.along) : 0
            return { x: a.x + (b.x - a.x) * share, y: a.y + (b.y - a.y) * share,
                     icon: a.icon + (b.icon - a.icon) * share }
        }
        // Neighbours `spacing` apart, everywhere along the track: the least
        // of how far apart they are seen against what their drawn size and
        // the gap need, as a share of what the spacing gives a flat panel.
        const worst = function(spacing) {
            let least = Infinity
            for (let index = 0; index < places.length - 1; ++index) {
                let target = places[index].along + spacing
                if (target > length) {
                    if (!closed) break
                    target -= length
                }
                const a = places[index], b = placeAt(target)
                const apart = Math.hypot(b.x - a.x, b.y - a.y)
                least = Math.min(least, apart / spacing * (size + gap) / ((a.icon + b.icon) / 2 + gap))
            }
            return least
        }
        // Neighbours a step apart first, then as far apart as that many
        // icons would stand, until it settles.
        let factor = worst(length / 360)
        for (let round = 0; round < 4 && Number.isFinite(factor) && factor > 0; ++round)
            factor = worst(Math.min(length / 2, (size + gap) / factor))
        return Number.isFinite(factor) && factor > 0 ? Math.max(0.05, Math.min(1.5, factor)) : 1
    }
    // Points on a plane in the camera's frame, from `place(x, y)` giving
    // their places in the scene's content: the whole way there is affine,
    // so three points give every other one, as plain numbers. `at(x, y)` is
    // a point, `step(dx, dy)` how a point moves for that change.
    function cameraMap(place) {
        const seen = function(x, y) {
            const point = camera.mapPositionFromScene(sceneContent.mapPositionToScene(place(x, y)))
            return { x: point.x, y: point.y, z: point.z }
        }
        const origin = seen(0, 0), ex = seen(1, 0), ey = seen(0, 1)
        const xx = ex.x - origin.x, xy = ex.y - origin.y, xz = ex.z - origin.z
        const yx = ey.x - origin.x, yy = ey.y - origin.y, yz = ey.z - origin.z
        return {
            at: function(x, y) {
                return { x: origin.x + xx * x + yx * y, y: origin.y + xy * x + yy * y, z: origin.z + xz * x + yz * y }
            },
            step: function(dx, dy) {
                return { x: xx * dx + yx * dy, y: xy * dx + yy * dy, z: xz * dx + yz * dy }
            }
        }
    }
    // The platform's plane round an opened folder, for a fan, arc, stack or
    // ring that lies on it: the scene's own x and y, in the panel's pixels,
    // with the dock's middle at the origin. `origin` is the rim beside the
    // folder, `outward` the way out of the dock there, `radius` the rim's
    // distance there. `toView(x, y)` is where a child standing at that point
    // has its middle in this item's pixels, and how large it is drawn there
    // (1 at the folder's depth), or null behind the camera: children stand
    // on the plane as the dock's icons stand on the platform. `rimAt(turn)`
    // is the rim's distance in any direction, `icons` where the dock's icons
    // stand on the plane.
    function folderPlane(entryIndex) {
        const node = worldEntries.objectAt(entryIndex)
        if (!rendererReady || !node || !node.inputAnchor) return null
        const foot = sceneContent.mapPositionFromScene(node.scenePosition)
        const lift = sceneContent.mapPositionFromScene(node.inputAnchor.scenePosition).minus(foot)
        const radius = Math.hypot(foot.x, foot.y)
        if (!(radius > 0)) return null
        const ux = foot.x / radius, uy = foot.y / radius
        const edge = Math.max(radius, folderRimDistance(ux, uy, radius))
        const depth = root.projectScenePoint(node.inputAnchor.scenePosition).z
        const frame = root.cameraMap(function(x, y) { return Qt.vector3d(x, y, foot.z).plus(lift) })
        // A unit above the plane, in the camera's frame: a band twisting
        // out of the plane stands that way.
        const raised = root.cameraMap(function(x, y) { return Qt.vector3d(x, y, foot.z + 1).plus(lift) }).at(0, 0)
        const level = frame.at(0, 0)
        const up = { x: raised.x - level.x, y: raised.y - level.y, z: raised.z - level.z }
        const near = camera.clipNear, distance = cameraDistance, middleX = width / 2, middleY = height / 2
        // `h` raises the point that far above the plane.
        const view = function(x, y, h) {
            const base = frame.at(x, y), rise = Number(h) || 0
            const point = { x: base.x + up.x * rise, y: base.y + up.y * rise, z: base.z + up.z * rise }
            const ahead = -point.z
            if (!(ahead > near + 1e-6)) return null
            const factor = distance / ahead
            return { x: middleX + point.x * factor, y: middleY - point.y * factor, scale: depth / (ahead - near) }
        }
        // Where every icon of the dock, the folder's own among them, stands.
        const icons = []
        for (let index = 0; index < worldEntries.count; ++index) {
            const entry = worldEntries.objectAt(index)
            if (!entry) continue
            const base = sceneContent.mapPositionFromScene(entry.scenePosition)
            icons.push({ x: base.x, y: base.y })
        }
        return {
            origin: { x: ux * edge, y: uy * edge }, outward: { x: ux, y: uy }, radius: edge, icons: icons,
            // The rim's distance the way `turn` radians point on the plane.
            rimAt: function(turn) {
                return Math.max(radius, root.folderRimDistance(Math.cos(turn), Math.sin(turn), radius))
            },
            // Projected as projectScenePoint() does.
            toView: view,
            // The plane about (ox, oy) as the panel's middle is seen: a shape
            // beside the dock keeps the panel's own tilt and proportions
            // instead of turning with the perspective far out to one side,
            // placed and sized as seen at that point; each child keeps the
            // size its own place gives it.
            alignedAt: function(ox, oy) {
                const centre = view(0, 0), atX = view(1, 0), atY = view(0, 1), atZ = view(0, 0, 1), origin = view(ox, oy)
                if (!centre || !atX || !atY || !atZ || !origin) return null
                const k = origin.scale / centre.scale
                const xx = (atX.x - centre.x) * k, xy = (atX.y - centre.y) * k
                const yx = (atY.x - centre.x) * k, yy = (atY.y - centre.y) * k
                const zx = (atZ.x - centre.x) * k, zy = (atZ.y - centre.y) * k
                return function(x, y, h) {
                    const own = view(x, y, h)
                    if (!own) return null
                    const dx = x - ox, dy = y - oy, rise = Number(h) || 0
                    return { x: origin.x + xx * dx + yx * dy + zx * rise,
                             y: origin.y + xy * dx + yy * dy + zy * rise, scale: own.scale }
                }
            }
        }
    }
    // What an opened folder's children keep clear of, in this item's
    // pixels: the platform as drawn (`hull`, the convex outline round its
    // mesh as the camera sees it) and every icon with the stand under it
    // (`rects`). The mesh's corners go through one affine map to the
    // camera, so even a large platform is only arithmetic.
    function folderKeepOut() {
        if (!rendererReady) return null
        const points = []
        const mesh = platform.visible && platformMesh && platformMesh.positions ? platformMesh.positions : []
        if (mesh.length) {
            const seen = function(x, y, z) {
                const point = camera.mapPositionFromScene(platform.mapPositionToScene(Qt.vector3d(x, y, z)))
                return { x: point.x, y: point.y, z: point.z }
            }
            const o = seen(0, 0, 0), ex = seen(1, 0, 0), ey = seen(0, 1, 0), ez = seen(0, 0, 1)
            const stride = Math.max(1, Math.floor(mesh.length / 1500))
            for (let index = 0; index < mesh.length; index += stride) {
                const x = Number(mesh[index][0]), y = Number(mesh[index][1]), z = Number(mesh[index][2])
                const ahead = -(o.z + (ex.z - o.z) * x + (ey.z - o.z) * y + (ez.z - o.z) * z)
                if (!(ahead > camera.clipNear + 1e-6)) continue
                const factor = cameraDistance / ahead
                points.push({ x: width / 2 + (o.x + (ex.x - o.x) * x + (ey.x - o.x) * y + (ez.x - o.x) * z) * factor,
                              y: height / 2 - (o.y + (ex.y - o.y) * x + (ey.y - o.y) * y + (ez.y - o.y) * z) * factor })
            }
        }
        // Under a decoration the platform is not drawn: the decoration's
        // solid is the panel's outline then, its vertices carried through
        // its frame as the platform's are.
        if (decorationShown) {
            const frame = platformDecoration.solidFrame()
            const seen = function(point) {
                const local = camera.mapPositionFromScene(point)
                return { x: local.x, y: local.y, z: local.z }
            }
            const o = seen(frame.origin), ex = seen(frame.x), ey = seen(frame.y), ez = seen(frame.z)
            const flat = platformDecoration.outlineVertices
            for (let index = 0; index + 2 < flat.length; index += 3) {
                const x = Number(flat[index]), y = Number(flat[index + 1]), z = Number(flat[index + 2])
                const ahead = -(o.z + (ex.z - o.z) * x + (ey.z - o.z) * y + (ez.z - o.z) * z)
                if (!(ahead > camera.clipNear + 1e-6)) continue
                const factor = cameraDistance / ahead
                points.push({ x: width / 2 + (o.x + (ex.x - o.x) * x + (ey.x - o.x) * y + (ez.x - o.x) * z) * factor,
                              y: height / 2 - (o.y + (ex.y - o.y) * x + (ey.y - o.y) * y + (ez.y - o.y) * z) * factor })
            }
        }
        // An icon's square with its tile round it, and its stand down to the
        // platform, as drawn.
        const rects = (projectedEntryGeometry || []).map(function(rect) {
            const footX = Number.isFinite(rect.footX) ? rect.footX : rect.x + rect.width / 2
            const footY = Number.isFinite(rect.footY) ? rect.footY : rect.y + rect.height
            const footHalf = Number(rect.footHalf) || 0
            const left = Math.min(rect.x - rect.width * 0.15, footX - footHalf)
            const right = Math.max(rect.x + rect.width * 1.15, footX + footHalf)
            const top = rect.y - rect.height * 0.15
            const bottom = Math.max(rect.y + rect.height * 1.5, footY + footHalf * 0.5)
            return { x: left, y: top, width: right - left, height: bottom - top }
        })
        return { hull: LayoutEngine.convexHull(points), rects: rects }
    }
    // How far the platform's rim is from the dock's middle along (ux, uy),
    // in the scene's units: an ellipse's reach for a generated platform, the
    // farthest vertex near that direction for a theme's mesh (the mesh turns
    // with the layout); `fallback` without a platform to measure.
    function folderRimDistance(ux, uy, fallback) {
        if (generatedPlatform) {
            const rx = Math.max(0.01, platformReachX), ry = Math.max(0.01, platformReachY)
            return platformScale / Math.hypot(ux / rx, uy / ry)
        }
        if (platformMesh && platformMesh.positions) {
            const turn = Math.atan2(uy, ux) + layoutAngle * Math.PI / 180
            let farthest = 0
            for (const vertex of platformMesh.positions) {
                const x = Number(vertex[0]), y = Number(vertex[1])
                const along = x * Math.cos(turn) + y * Math.sin(turn)
                const across = -x * Math.sin(turn) + y * Math.cos(turn)
                if (along > 0 && Math.abs(across) <= along * 0.27) farthest = Math.max(farthest, along)
            }
            if (farthest > 0) return farthest * platformScale
        }
        return fallback
    }
    // The platform's edge beside the entry and the way out of the platform
    // there, as drawn: where a folder's fan, arc, stack or ring opens from on
    // a 3D panel. `start` lies `margin` screen pixels outside the platform's
    // rim on the entry's own radial line, `direction` is that line as the
    // camera sees it; both in view pixels.
    function folderOutwardRay(entryIndex, margin) {
        const node = worldEntries.objectAt(entryIndex)
        if (!rendererReady || !node || !node.inputAnchor) return null
        const foot = sceneContent.mapPositionFromScene(node.scenePosition)
        const radius = Math.hypot(foot.x, foot.y)
        if (!(radius > 0)) return null
        const ux = foot.x / radius, uy = foot.y / radius
        const edge = Math.max(radius, folderRimDistance(ux, uy, radius))
        const usable = point => Number.isFinite(point.x) && Number.isFinite(point.y) && point.z > 0
        const at = distance => root.projectScenePoint(sceneContent.mapPositionToScene(
            Qt.vector3d(ux * distance, uy * distance, foot.z)))
        const start = at(edge), further = at(edge + Math.max(1, radius) * 0.25)
        if (!usable(start) || !usable(further)) return null
        const dx = further.x - start.x, dy = further.y - start.y, length = Math.hypot(dx, dy)
        if (!(length > 1e-6)) return null
        const gap = Math.max(0, Number(margin) || 0)
        return { start: { x: start.x + dx / length * gap, y: start.y + dy / length * gap },
                 direction: { x: dx / length, y: dy / length } }
    }
    function updateProjection() {
        if (!rendererReady || width <= 0 || height <= 0) return
        const result = []
        for (let i = 0; i < worldEntries.count; ++i) {
            const node = worldEntries.objectAt(i)
            if (!node || !node.inputAnchor) return
            // The icon's resting square, as the camera sees it.
            const anchor = node.inputAnchor, half = node.size / 2
            const points = [[-half, -half], [half, -half], [half, half], [-half, half]].map(p =>
                root.projectScenePoint(anchor.mapPositionToScene(Qt.vector3d(p[0], p[1], 0))))
            const left = Math.min(...points.map(p => p.x)), top = Math.min(...points.map(p => p.y))
            const right = Math.max(...points.map(p => p.x)), bottom = Math.max(...points.map(p => p.y))
            if (![left, top, right, bottom].every(Number.isFinite) || right <= left || bottom <= top) return
            const center = root.projectScenePoint(anchor.scenePosition)
            // Its stand as drawn: the foot on the platform and the stand's
            // reach there, so an opened folder keeps clear of the whole.
            // The stand's foot: the entry stands at the top of its stand, in the
            // platform's frame, so the foot is that place lowered by the stand's
            // height along the platform's normal. The stand's reach is its
            // shape's reach at the entry's pedestal scale; a bowl reaches the
            // same way all round, so the frame's x serves.
            const standHeight = root.pedestalDrawn ? Number(node.pedestalTop) || 0 : 0
            const reach = root.pedestalDrawn
                ? PlatformGeometry.pedestalShapeReach(root.pedestalKind) * (Number(node.pedestalScale) || 0) : 0
            const frame = node.parent && node.parent.mapPositionToScene ? node.parent : null
            const footPlace = Qt.vector3d(node.position.x, node.position.y, node.position.z - standHeight)
            const foot = frame && standHeight > 0 ? root.projectScenePoint(frame.mapPositionToScene(footPlace)) : center
            const footEdge = frame && standHeight > 0 && reach > 0
                ? root.projectScenePoint(frame.mapPositionToScene(footPlace.plus(Qt.vector3d(reach, 0, 0)))) : foot
            const footHalf = Number.isFinite(footEdge.x) && Number.isFinite(footEdge.y)
                ? Math.hypot(footEdge.x - foot.x, footEdge.y - foot.y) : 0
            result.push({x: left, y: top, width: right-left, height: bottom-top,
                centerX: center.x, centerY: center.y, depth: center.z,
                footX: Number.isFinite(foot.x) ? foot.x : center.x, footY: Number.isFinite(foot.y) ? foot.y : center.y,
                footHalf: footHalf})
        }
        if (JSON.stringify(result) !== JSON.stringify(projectedEntryGeometry)) projectedEntryGeometry = result
        // The dock's centre as drawn: the platform's middle at its top. A
        // folder's contents open away from it, not from the icons' heads.
        const centre = root.projectScenePoint(sceneContent.mapPositionToScene(Qt.vector3d(0, 0, platformTop)))
        if (Number.isFinite(centre.x) && Number.isFinite(centre.y)
                && (centre.x !== projectedCentre.x || centre.y !== projectedCentre.y))
            projectedCentre = Qt.point(centre.x, centre.y)
    }

    // The box round a mesh's corners in its own units, [least x, y, z,
    // greatest x, y, z], or null without a mesh.
    function meshBox(mesh) {
        const positions = mesh && mesh.positions
        if (!positions || !positions.length) return null
        const box = [Infinity, Infinity, Infinity, -Infinity, -Infinity, -Infinity]
        for (const vertex of positions)
            for (let axis = 0; axis < 3; ++axis) {
                const value = Number(vertex[axis])
                if (value < box[axis]) box[axis] = value
                if (value > box[axis + 3]) box[axis + 3] = value
            }
        return box.every(Number.isFinite) ? box : null
    }
    readonly property var platformBox: meshBox(platformMesh)
    readonly property var pedestalBox: meshBox(pedestalMesh)
    // An icon's picture, a #Rectangle, in its own units.
    readonly property var pictureBox: [-50, -50, 0, 50, 50, 0]
    // Kept round what reaches out of the box: the glow's halo, and a little
    // for antialiasing and a stand's own motion.
    readonly property real overflowPadding: 12 + glowReach
    readonly property real overflowStep: 32
    property int overflowRevision: -1
    onOverflowXChanged: hitTestsChanged()
    onOverflowYChanged: hitTestsChanged()
    // How far the drawn scene reaches out of the panel's box, measured on the
    // corners of the platform and its parts, of every icon's picture with its
    // magnification and motion, of every stand, and of the handles while
    // editing, as the camera sees them now.
    function updateOverflow() {
        if (!rendererReady || width <= 0 || height <= 0) return
        const revision = hitRevision * 2 + (editMode ? 1 : 0)
        if (revision === overflowRevision) return
        overflowRevision = revision
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity, lost = false
        const take = function(point) {
            if (!Number.isFinite(point.x) || !Number.isFinite(point.y)) {
                lost = true
                return
            }
            left = Math.min(left, point.x)
            right = Math.max(right, point.x)
            top = Math.min(top, point.y)
            bottom = Math.max(bottom, point.y)
        }
        const takeBox = function(node, box) {
            if (!node || !box || !node.visible) return
            for (let corner = 0; corner < 8; ++corner)
                take(projectScenePoint(node.mapPositionToScene(Qt.vector3d(
                    box[corner & 1 ? 3 : 0], box[corner & 2 ? 4 : 1], box[corner & 4 ? 5 : 2]))))
        }
        takeBox(platform, platformBox)
        for (let index = 0; index < panelPartNodes.count; ++index) {
            const part = panelPartNodes.objectAt(index)
            if (part) takeBox(part, part.meshBox)
        }
        for (let index = 0; index < worldEntries.count; ++index) {
            const node = worldEntries.objectAt(index)
            if (!node || !node.visible) continue
            takeBox(node.glyphModel, pictureBox)
            if (pedestalDrawn)
                takeBox(pedestalOnPanel ? node.standOnPanel : node.standFacing, pedestalBox)
        }
        if (editMode) {
            const origin = gizmo.scenePosition, reach = gizmoLength * 1.25
            for (const direction of [Qt.vector3d(1, 0, 0), Qt.vector3d(0, 1, 0), Qt.vector3d(0, 0, 1),
                                     camera.right, camera.up])
                for (const sign of [-1, 1])
                    take(projectScenePoint(origin.plus(direction.times(sign * reach))))
        }
        // A scene that reaches behind the camera fills all it can. Otherwise
        // the reach is how far the scene reaches out of the box, less than
        // nothing while it stays inside: a ring that fits its box still
        // needs room for its glow's light.
        const limitX = Math.max(width, Screen.width), limitY = Math.max(height, Screen.height)
        overflowX = steppedOverflow(overflowX, lost ? limitX : Math.min(limitX, Math.max(-left, right - width)))
        overflowY = steppedOverflow(overflowY, lost ? limitY : Math.min(limitY, Math.max(-top, bottom - height)))
    }
    // A margin with room for `reach` and the padding round it: none while
    // nothing reaches out and nothing glows (a larger view costs every frame
    // its pixels), room for the light as soon as something glows, grown in
    // steps with one to spare, given back only once two are free.
    function steppedOverflow(current, reach) {
        const needed = reach > 0.5 || glowReach > 0 ? reach + overflowPadding : 0
        const stepped = needed > 0.5 ? (Math.ceil(needed / overflowStep) + 1) * overflowStep : 0
        return needed > current || needed <= current - 2 * overflowStep ? stepped : current
    }

    // RenderStats throttles change notifications. A static scene can stop
    // before that interval, so also inspect it after an actual window frame.
    function observeCompletedFrame() {
        if (view.visible && view.renderStats.frameTime > 0)
            completedFrameObserved = true
    }

    // Native frame updates belong to this scene. A Loader replacement may
    // invalidate its QML context before an engine-wide callLater runs.
    Timer {
        id: frameUpdates
        interval: 0
        repeat: false
        onTriggered: {
            if (!root.completedFrameObserved)
                root.observeCompletedFrame()
            root.updateProjection()
            root.updateOverflow()
            if (root.editMode)
                root.updateEndOnAxes()
        }
    }
    Component.onDestruction: {
        holdHologramClock(false)
        frameUpdates.stop()
        // Let go of the mirror sheet and the icons' pictures before they
        // are deleted with the scene.
        reflectionsAllowed = false
        reflectionTexture.sourceItem = null
    }
    Connections {
        target: root.Window.window
        function onFrameSwapped() { frameUpdates.start() }
    }

    function bounded(key, fallback, minimum, maximum) {
        const value = Number((sceneDefinition || ({}))[key])
        return Number.isFinite(value) ? Math.max(minimum, Math.min(maximum, value)) : fallback
    }

    function partsForScope(scope) {
        const definitions = (sceneDefinition || ({})).parts || []
        const data = (sceneResources || ({})).parts || []
        const result = []
        for (let index = 0; index < definitions.length; ++index) {
            if (definitions[index].scope === scope)
                result.push({ definition: definitions[index], resources: data[index] || ({}) })
        }
        return result
    }
    function partTriangles(parts) {
        return parts.reduce(function(count, part) {
            return count + Number((part.resources.mesh || ({})).indexes?.length || 0) / 3
        }, 0)
    }
    function partOpenAmount(part) {
        return part.mechanism === mechanism ? Math.max(0, Math.min(1, 1 - collapseProgress)) : 1
    }
    function number(object, key, fallback) {
        const value = Number((object || ({}))[key])
        return Number.isFinite(value) ? value : fallback
    }

    // Lightning's bolts over the dark storm, as strong as each Lightning
    // background asks, for the 3D surfaces drawn in lightning: the
    // platform's and the stands'. Drawn only into their textures.
    // A clear-ground look - lightning, aurora - in its palette over its dark
    // sky, as strong as the Background asks.
    component LookComposite: Item {
        // Whether a surface is drawn in this look: only then is it loaded.
        property bool active: false
        property string look: "lightning"
        property string lookPalette: ""
        property real background: 0
        readonly property bool ready: light.status === Image.Ready
        visible: active
        width: 512
        height: 512
        Rectangle {
            anchors.fill: parent
            color: "rgb(" + LayoutEngine.themeStyle(parent.look, "", 48).skyColour + ")"
            opacity: Math.max(0, Math.min(1, parent.background))
        }
        Image {
            id: light
            anchors.fill: parent
            // Only a surface drawn in a clear-ground look loads its light.
            source: parent.active && parent.look.length
                ? Qt.resolvedUrl("../materials/" + LayoutEngine.lookTexture(parent.look, parent.lookPalette) + ".png") : ""
            sourceSize: Qt.size(512, 512)
            asynchronous: true
            cache: true
        }
    }
    readonly property var clearLooks: ["lightning", "aurora"]
    function lookPalette(look, prefix) {
        return String((sceneDefinition || ({}))[prefix + "LookColour"] || "")
    }
    readonly property string panelPalette: lookPalette(textureMaterial, "panel")
    readonly property string pedestalPalette: lookPalette(pedestalTextureLook, "pedestal")
    // A palette of its own replaces the look's colours, so nothing tints it.
    readonly property bool panelRepainted: LayoutEngine.lookTexture(textureMaterial, panelPalette) !== textureMaterial
    readonly property real panelLookBackground: bounded("panelLookBackground", 0.5, 0, 1)
    readonly property real pedestalLookBackground: bounded("pedestalLookBackground", 0.5, 0, 1)
    LookComposite {
        id: panelLightning
        active: root.clearLooks.includes(root.textureMaterial)
        look: root.textureMaterial
        lookPalette: root.lookPalette(root.textureMaterial, "panel")
        background: root.panelLookBackground
    }
    ShaderEffectSource {
        id: panelLightningTexture
        sourceItem: panelLightning.active ? panelLightning : null
        hideSource: true
        live: true
        visible: false
        textureSize: Qt.size(512, 512)
    }
    LookComposite {
        id: standLightning
        active: root.clearLooks.includes(root.pedestalTextureLook)
        look: root.pedestalTextureLook
        lookPalette: root.lookPalette(root.pedestalTextureLook, "pedestal")
        background: root.pedestalLookBackground
    }
    ShaderEffectSource {
        id: standLightningTexture
        sourceItem: standLightning.active ? standLightning : null
        hideSource: true
        live: true
        visible: false
        textureSize: Qt.size(512, 512)
    }

    Image {
        id: materialImage
        visible: false
        source: root.materialTextureSource
        sourceSize: Qt.size(512, 512)
        asynchronous: true
        cache: true
    }

    // The material and its relief map as a generated band wears them: a
    // photograph's two ends do not match, and where the band closes they
    // would meet as a line across it (a hologram's inline draws it bright).
    // Each is faded at its ends into itself (shaders/seamless.frag).
    ShaderEffect {
        id: seamlessMaterialEffect
        width: Math.max(1, materialImage.implicitWidth)
        height: Math.max(1, materialImage.implicitHeight)
        property variant source: materialImage
        property real blend: 0.25
        fragmentShader: "../shaders/seamless.frag.qsb"
    }
    ShaderEffectSource {
        id: seamlessMaterial
        visible: false
        sourceItem: seamlessMaterialEffect
        hideSource: true
        width: seamlessMaterialEffect.width
        height: seamlessMaterialEffect.height
    }
    ShaderEffect {
        id: seamlessNormalEffect
        width: Math.max(1, materialNormalImage.implicitWidth)
        height: Math.max(1, materialNormalImage.implicitHeight)
        property variant source: materialNormalImage
        property real blend: 0.25
        fragmentShader: "../shaders/seamless.frag.qsb"
    }
    ShaderEffectSource {
        id: seamlessNormal
        visible: false
        sourceItem: seamlessNormalEffect
        hideSource: true
        width: seamlessNormalEffect.width
        height: seamlessNormalEffect.height
    }

    Image {
        id: pedestalImage
        visible: false
        source: root.pedestalTextureSource
        sourceSize: Qt.size(512, 512)
        asynchronous: true
        cache: true
    }

    Image {
        id: pedestalNormalImage
        visible: false
        source: root.pedestalNormalSource
        sourceSize: Qt.size(512, 512)
        asynchronous: true
        cache: true
    }

    Image {
        id: materialNormalImage
        visible: false
        source: root.materialNormalSource
        sourceSize: Qt.size(512, 512)
        asynchronous: true
        cache: true
    }

    Image {
        id: textureImage
        visible: false
        source: root.textureSource
        asynchronous: true
        sourceSize: Qt.size(1024, 1024)
        cache: false
    }

    Row {
        objectName: "mesh-gizmo-toolbar"
        visible: root.editMode && root.rendererReady
        anchors.top: parent.top
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4
        z: 3
        Repeater {
            model: [{ mode: "move", label: qsTr("Move") }, { mode: "rotate", label: qsTr("Rotate") },
                    { mode: "scale", label: qsTr("Scale") }]
            delegate: Rectangle {
                id: modeButton
                required property var modelData
                objectName: "mesh-gizmo-mode-" + modelData.mode
                width: modeLabel.implicitWidth + 16
                height: modeLabel.implicitHeight + 8
                radius: 4
                color: root.gizmoMode === modelData.mode ? "#2f6f88" : "#cc1b2831"
                border.color: "#73cfe7"
                Accessible.role: Accessible.Button
                Accessible.name: modelData.label
                Accessible.onPressAction: root.gizmoMode = modelData.mode
                Text {
                    id: modeLabel
                    anchors.centerIn: parent
                    text: modeButton.modelData.label
                    color: "#f4f8fb"
                    font.pixelSize: 11
                }
                TapHandler { onTapped: root.gizmoMode = modeButton.modelData.mode }
            }
        }
    }
    // How to drag what is not obvious: the body itself, and an arrow that
    // points at the viewer.
    Text {
        objectName: "mesh-gizmo-hint"
        visible: root.editMode && root.rendererReady
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(implicitWidth, parent.width - 8)
        z: 3
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        color: "#f4f8fb"
        style: Text.Outline
        styleColor: "#cc1b2831"
        font.pixelSize: 11
        text: root.gizmoMode === "move" && root.endOnAxes.length > 0
            ? qsTr("The %1 arrow points at you: drag it up or down.")
                .arg(root.endOnAxes.map(function(axis) { return axis.toUpperCase() }).join(", "))
            : qsTr("Drag the platform to tilt and turn it.")
    }

    MouseArea {
        objectName: "mesh-gizmo-input"
        // Wherever the scene is drawn, the handles included.
        x: view.x
        y: view.y
        width: view.width
        height: view.height
        z: 2
        // While editing, the whole panel belongs to the handles: a press that
        // misses them does nothing, so no application starts by accident.
        // Outside the panel's box such a press is left to the desktop.
        enabled: root.editMode && root.rendererReady
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        onPressed: function(mouse) {
            const x = mouse.x + view.x, y = mouse.y + view.y
            const outside = x < 0 || y < 0 || x > root.width || y > root.height
            if (mouse.button === Qt.RightButton) {
                if (outside && !root.dragStart) mouse.accepted = false
                root.cancelGizmoDrag()
                return
            }
            // A handle first; otherwise a press on the platform or its icons
            // takes the body, to tilt and turn it.
            const handle = root.gizmoHandleAt(x, y)
            if (handle)
                root.beginGizmoDrag(handle, x, y)
            else if (root.containsInputPoint(Qt.point(x, y)))
                root.beginGizmoDrag({ mode: "orbit", axis: "body" }, x, y)
            else if (outside)
                mouse.accepted = false
        }
        onPositionChanged: function(mouse) {
            if (root.dragStart)
                root.updateGizmoDrag(mouse.x + view.x, mouse.y + view.y, mouse.modifiers)
        }
        onReleased: function(mouse) {
            if (mouse.button !== Qt.LeftButton || !root.dragStart)
                return
            // The keys held when the button is let go decide the result.
            root.updateGizmoDrag(mouse.x + view.x, mouse.y + view.y, mouse.modifiers)
            root.finishGizmoDrag()
        }
        onCanceled: root.cancelGizmoDrag()
    }
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.dragStart !== null
        onActivated: root.cancelGizmoDrag()
    }

    // The mirror images' sheet: as wide as the view, clipped away on screen,
    // drawn into the water's texture only.
    Item {
        clip: true
        width: 1
        height: 1
        Item {
            id: mirrorSheet
            objectName: "river-mirror-sheet"
            width: view.width
            height: view.height
            Repeater {
                model: root.waterReflections ? worldEntries.count : 0
                // A shader effect with Qt's own default shaders draws its
                // source's texture as it is: the icon's picture item is an
                // offscreen texture provider with no size of its own, which
                // a layer of it would render as nothing.
                delegate: ShaderEffect {
                    id: mirrorSprite
                    required property int index
                    objectName: "mirror-glyph-" + index
                    readonly property var entryNode: root.entryVisuals.length >= 0 ? worldEntries.objectAt(index) : null
                    readonly property var glyph: entryNode ? entryNode.glyphModel : null
                    // Where the icon's mirror image lies on the view, and how
                    // large it is there: the icon's picture is a hundred units
                    // square at the glyph's scale, seen at the mirror point's
                    // depth. Read with the camera's angles so a turn of the
                    // scene moves it too.
                    readonly property var place: {
                        if (!glyph || !glyph.visible) return null
                        const angles = [root.shownPitch, root.shownYaw, root.shownRoll, root.edgeTilt,
                                        root.cameraDistance, view.width, view.height]
                        const mapped = view.mapFrom3DScene(root.reflectedPoint(glyph.scenePosition))
                        if (!(mapped.z > 0)) return null
                        const focal = (view.height / 2) / Math.tan(root.viewFieldOfView * Math.PI / 360)
                        const size = 100 * glyph.sceneScale.y * focal / (mapped.z + camera.clipNear)
                        return { x: mapped.x, y: mapped.y, size: size }
                    }
                    // The icon's own picture item, as the entry's texture
                    // holds it: when the entry lets go of it, this does too.
                    property variant source: entryNode && entryNode.glyphTexture ? entryNode.glyphTexture.sourceItem : null
                    visible: place !== null && source !== null
                    width: place ? place.size : 1
                    height: place ? place.size : 1
                    x: place ? place.x - width / 2 : 0
                    y: place ? place.y - height / 2 : 0
                    opacity: glyph ? glyph.pictureOpacity : 1
                    // Upside down, as a mirror image in water is.
                    transform: Scale { origin.y: mirrorSprite.height / 2; yScale: -1 }
                }
            }
        }
    }
    // The pointer over the panel, only watched, for a decoration's lights.
    HoverHandler {
        id: decorationPointer
        enabled: root.decorationShown
    }
    View3D {
        id: view
        x: -root.overflowX
        y: -root.overflowY
        width: root.viewWidth
        height: root.viewHeight
        visible: root.rendererReady && !root.sceneConcealed
        opacity: root.panelOpacity
        explicitTextureWidth: root.targetWidth
        explicitTextureHeight: root.targetHeight
        camera: camera
        environment: root.bloomWanted ? glowEnvironment : plainEnvironment
        SceneEnvironment {
            id: plainEnvironment
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: root.effectiveQuality === "low" ? SceneEnvironment.NoAA
                : root.effectiveQuality === "high" ? SceneEnvironment.SSAA : SceneEnvironment.MSAA
            antialiasingQuality: root.effectiveQuality === "high"
                ? SceneEnvironment.VeryHigh : SceneEnvironment.High
        }
        ExtendedSceneEnvironment {
            id: glowEnvironment
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: plainEnvironment.antialiasingMode
            antialiasingQuality: plainEnvironment.antialiasingQuality
            // Every halving of the blur up to the reach (bloomLevels), each
            // scaled up again smoothly (bicubic): a round halo with a soft
            // edge at every quality - bilinear steps drew it square;
            // this strength and intensity are the chosen look.
            glowEnabled: true
            glowQualityHigh: root.effectiveQuality !== "low"
            glowUseBicubicUpscale: true
            glowStrength: 0.9
            glowIntensity: 1.6
            glowBloom: 0
            glowHDRMinimumValue: 1
            glowBlendMode: ExtendedSceneEnvironment.GlowBlendMode.Additive
            glowLevel: (1 << root.bloomLevels) - 1
        }
        PrincipledMaterial {
            id: standGlowMaterial
            baseColor: "#000000"
            roughness: 1
            specularAmount: 0
            emissiveFactor: root.pedestalGlowLight
            // Drawn with the see-through objects, after what it wraps
            // (underGlow), which hides its inside.
            alphaMode: PrincipledMaterial.Blend
            cullMode: Material.FrontFaceCulling
        }
        PrincipledMaterial {
            id: platformGlowMaterial
            baseColor: "#000000"
            roughness: 1
            specularAmount: 0
            emissiveFactor: root.panelGlowLight
            // Drawn with the see-through objects, after what it wraps
            // (underGlow), which hides its inside.
            alphaMode: PrincipledMaterial.Blend
            cullMode: Material.FrontFaceCulling
        }
        // The glow over the shape itself at the top of its range: its light
        // laid on the platform's surface, as much as panelGlowWash.
        PrincipledMaterial {
            id: platformWashMaterial
            baseColor: "#000000"
            roughness: 1
            specularAmount: 0
            emissiveFactor: root.panelWashLight
            alphaMode: PrincipledMaterial.Blend
            cullMode: Material.BackFaceCulling
        }
        PrincipledMaterial {
            id: tileGlowMaterial
            baseColor: "#000000"
            roughness: 1
            specularAmount: 0
            emissiveFactor: root.tileGlowLight
            // Drawn with the see-through objects, after what it wraps
            // (underGlow), which hides its inside.
            alphaMode: PrincipledMaterial.Blend
            cullMode: Material.FrontFaceCulling
        }
        Texture {
            id: selectedMaterialTexture
            sourceItem: !root.Window.window ? null
                : root.clearLooks.includes(root.textureMaterial)
                    ? (panelLightningTexture.Window.window === root.Window.window ? panelLightningTexture : null)
                : materialImage.Window.window !== root.Window.window ? null
                : root.generatedPlatform ? seamlessMaterial : materialImage
            generateMipmaps: true
            mipFilter: Texture.Linear
            // A generated platform wraps the texture round its band, as
            // many times along it as keep the texture's scale the same both
            // ways; a theme's mesh maps its texture as the theme authored it.
            scaleU: !root.generatedPlatform ? 1
                : root.textureMaterial === "picture" ? root.pictureRepeats
                : Number(root.generatedPlatform.textureRepeats || 1)
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
            positionU: root.textureMaterial === "picture" ? 0
                : root.flowOffset + (root.textureMaterial === "lightning" ? root.lightningShift : 0)
        }
        Texture {
            id: selectedPedestalTexture
            sourceItem: !root.Window.window ? null
                : root.clearLooks.includes(root.pedestalTextureLook)
                    ? (standLightningTexture.Window.window === root.Window.window ? standLightningTexture : null)
                : pedestalImage.Window.window === root.Window.window ? pedestalImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
            positionU: root.pedestalTextureLook === "lightning" ? root.lightningShift : 0
        }
        Texture {
            id: selectedPedestalNormal
            sourceItem: root.Window.window && pedestalNormalImage.Window.window === root.Window.window
                ? pedestalNormalImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
            positionU: selectedPedestalTexture.positionU
        }
        Texture {
            id: selectedMaterialNormal
            sourceItem: !root.Window.window || materialNormalImage.Window.window !== root.Window.window ? null
                : root.generatedPlatform ? seamlessNormal : materialNormalImage
            generateMipmaps: true
            mipFilter: Texture.Linear
            scaleU: selectedMaterialTexture.scaleU
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
            positionU: root.flowOffset + (root.textureMaterial === "lightning" ? root.lightningShift : 0)
        }
        Texture {
            id: surfaceTexture
            sourceItem: root.Window.window && textureImage.Window.window === root.Window.window
                ? textureImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
        }
        // The picture a generated platform wears as a real surface, at its
        // full size, with the blurred halvings its rise follows.
        Texture {
            id: pictureReliefTexture
            source: root.pictureRelief ? root.panelPicture : ""
            generateMipmaps: true
            mipFilter: Texture.Linear
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
        }
        PictureRelief {
            id: platformRelief
            pictureMap: TextureInput {
                texture: pictureReliefTexture
                enabled: root.pictureRelief
            }
            repeatU: root.pictureRepeats
            lengthU: root.generatedPlatform ? Number(root.generatedPlatform.alongLength || 1) : 1
            lengthV: root.generatedPlatform ? Number(root.generatedPlatform.sectionLength || 1) : 1
            reliefDepth: root.pictureDepth
            keyLightDirection: keyLight.forward.times(-1)
            // A theme's own mesh gets the picture at a generated band's
            // scale, about one picture across 0.9 of the scene's units.
            boxScale: root.generatedPlatform ? 0 : 1.1
            reliefInvert: root.pictureRaisesDark ? 1 : 0
            reliefOutward: root.pictureRisesOut ? 1 : 0
            reliefLevel: root.pictureLevel
            reliefContrast: root.pictureContrast
            reliefSmoothing: root.pictureSmoothing
            meshStep: Number((root.platformMesh || ({})).meshStep || 0.02)
        }
        // The picture a stand wears as a real surface.
        Texture {
            id: pedestalReliefTexture
            source: root.pedestalPictureRelief ? root.pedestalPicture : ""
            generateMipmaps: true
            mipFilter: Texture.Linear
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
        }
        PictureRelief {
            id: standRelief
            pictureMap: TextureInput {
                texture: pedestalReliefTexture
                enabled: root.pedestalPictureRelief
            }
            reliefDepth: root.pedestalPictureDepth
            keyLightDirection: keyLight.forward.times(-1)
            // Laid on each face from the side it faces, about ten stones
            // across a stand, so they still read on a small one.
            boxScale: 0.4
            reliefInvert: root.pedestalPictureRaisesDark ? 1 : 0
            reliefOutward: root.pedestalPictureRisesOut ? 1 : 0
            reliefLevel: root.pedestalPictureLevel
            reliefContrast: root.pedestalPictureContrast
            reliefSmoothing: root.pedestalPictureSmoothing
            meshStep: Number((root.pedestalReliefMesh || ({})).meshStep || 0.05)
        }
        // The river or the magma: its water or lava and its relief flowing
        // along the band. A custom material reads its textures from files,
        // not from the rendered image items the built-in materials share.
        Texture {
            id: flowSurfaceTexture
            source: root.flowingSurface ? root.materialTextureSource : ""
            generateMipmaps: true
            mipFilter: Texture.Linear
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
        }
        Texture {
            id: flowReliefTexture
            source: root.flowingSurface ? root.materialNormalSource : ""
            generateMipmaps: true
            mipFilter: Texture.Linear
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
        }
        Texture {
            id: reflectionTexture
            sourceItem: root.waterReflections && mirrorSheet.Window.window === root.Window.window
                ? mirrorSheet : null
        }
        FlowingSurface {
            id: platformFlow
            surfaceMap: TextureInput {
                texture: flowSurfaceTexture
                enabled: root.flowingSurface
            }
            reliefMap: TextureInput {
                texture: flowReliefTexture
                enabled: root.flowingSurface
            }
            reflectionMap: TextureInput {
                texture: reflectionTexture
                enabled: root.waterReflections
            }
            mirrored: root.waterReflections
            molten: root.moltenFlow
            repeatU: root.generatedPlatform ? Number(root.generatedPlatform.textureRepeats || 1) : 1
            flowOffset: root.flowOffset
        }
        // Tilt first, then turn about the tilted view's own up: tilted 90
        // degrees the panel lies level whatever its turn, which then spins
        // it like a turntable.
        Node {
            objectName: "mesh-camera-rig"
            eulerRotation.x: root.shownPitch
            Node {
            objectName: "mesh-camera-turn"
            eulerRotation.y: root.shownYaw
            PerspectiveCamera {
                id: camera
                fieldOfView: root.viewFieldOfView
                z: root.cameraDistance
                eulerRotation.z: root.shownRoll
                clipNear: 1
                // Picking (View3D.pickAll) and Qt's viewport mapping use a
                // projection the camera keeps from its last change, which a
                // view that only changes shape - a margin growing across the
                // panel - leaves behind, so picks missed what is drawn. The
                // far plane moves by a hair with the view's size, so every
                // new shape renews that projection too.
                clipFar: Math.max(100, z * 5) + (root.viewWidth + root.viewHeight) * 1e-4
                // Camera-local coordinates preserve the logical pixel centers
                // without querying viewport matrices during scene construction.

            }
            }
        }
        Node {
            id: sceneContent
            objectName: "mesh-scene-content"
            // An edge panel turns about its screen's edge (edgeFrame).
            position: root.edgePanel ? root.edgeFrame.position
                : Qt.vector3d(root.shownPosition.x, root.shownPosition.y,
                              root.shownPosition.z + root.floatOffset)
            pivot: root.edgePanel ? root.edgeFrame.pivot : Qt.vector3d(0, 0, 0)
            eulerRotation: root.edgePanel ? root.edgeFrame.turn : Qt.vector3d(0, 0, 0)
            scale: root.edgePanel
                ? Qt.vector3d(root.edgeFrame.scale, root.edgeFrame.scale, root.edgeFrame.scale)
                : Qt.vector3d(root.shownScale * root.radialCollapseScale,
                              root.shownScale * root.radialCollapseScale,
                              root.shownScale * root.radialCollapseScale)
        Node {
            id: entryFrame
            eulerRotation.z: -root.layoutAngle
        Repeater3D {
            id: worldEntries
            // A count model preserves nodes and textures during rotation.
            model: root.geometryWithinBudget ? root.entryGeometry.length : 0
            delegate: Node {
                id: entryNode
                required property int index
                objectName: "mesh-entry-" + index
                readonly property var rect: root.entryGeometry[index] || ({})
                readonly property var entry: root.entryVisuals[index] || null
                readonly property var visual: entry ? entry.meshVisualItem : null
                readonly property var iconMotion: root.motionAllowed && entry ? entry.iconMotion : ({})
                readonly property var glyphMotion: root.motionAllowed && entry ? entry.glyphMotion : ({})
                readonly property var tileMotion: root.motionAllowed && entry ? entry.tileMotion : ({})
                readonly property real size: Number(rect.width || 1)
                readonly property real hoverScale: entry ? Number(entry.visualScale || 1) : 1
                readonly property real elevation: root.bounded("iconElevation", 0.3, 0, 2)
                // The stand, in the viewer's frame (x right, y up the screen,
                // z toward the viewer): its foot's middle on the platform at
                // the icon's place, turned by Stand facing; its width per
                // unit and its height (Thickness stretches it; Auto's column is
                // the 3D page's Pedestal height tall). The icon floats its
                // Float height above the stand's top, centred over it.
                readonly property real pedestalScale: size * root.pedestalShapeScale * root.pedestalSize
                readonly property real pedestalDepth: root.pedestalShape === "auto" ? size * elevation
                    : root.pedestalKind === "column" ? size * 0.4 * root.pedestalSize * root.pedestalThickness
                    : pedestalScale * root.pedestalThickness
                readonly property real pedestalTop: root.pedestalDrawn ? root.pedestalExtentsUnit.top * pedestalScale : 0
                readonly property real pedestalTopX: root.pedestalDrawn ? root.pedestalExtentsUnit.topX * pedestalScale : 0
                readonly property real pedestalGap: !root.pedestalDrawn ? 0
                    : root.pedestalShape === "auto" ? size * 0.02 : size * (0.03 + root.pedestalLift * 0.3)
                // The icon's foot above the entry's root. The picture keeps a
                // margin of 0.07 icon sizes inside its square, as on a flat
                // panel, so the gap the eye sees is the Float height's.
                readonly property real standFootLift: root.pedestalDrawn
                    ? Math.max(pedestalTop, pedestalTop + pedestalGap - size * 0.07) : 0
                // A hologram's light leaves a lens in the middle of the stand's
                // top, or on the platform where there is none; where it is in
                // the icon's frame (x right, y up, z toward the viewer), how
                // wide, and the icon floating its Light height over it.
                readonly property var openingSpot: root.pedestalDrawn ? PlatformGeometry.pedestalOpening(root.pedestalKind)
                    : ({ radius: 0, height: 0, hole: false })
                readonly property vector3d openingInIcon: root.pedestalDrawn
                    ? root.pedestalAxis.times(openingSpot.height * pedestalDepth + pedestalBob)
                    : facing.inverted().times(Qt.vector3d(0, 0, -(baseLift + tileLift)))
                readonly property real openingRadius: root.pedestalDrawn ? openingSpot.radius * pedestalScale : size * 0.4
                readonly property bool hologramLightShown: root.iconHologram.beam && visual !== null
                // On an edge panel an icon without a stand is centred on its
                // place, as on the flat panel, rather than standing on it, and
                // grows about its middle.
                readonly property bool edgeCentred: root.edgePanel && !root.pedestalDrawn && !hologramLightShown
                readonly property real footLift: hologramLightShown ? openingInIcon.y + root.hologramLift * size
                    : edgeCentred ? -size / 2 : standFootLift
                readonly property real iconX: hologramLightShown ? openingInIcon.x : pedestalTopX
                // The lens the hologram's light leaves, in its colour or its
                // icon's main colour.
                property Material openingMaterial: HologramOpening {
                    picture.texture: glyphTexture
                    tint: root.hologramColour(root.iconHologramColours.opening)
                    own: root.iconHologramColours.opening && root.iconHologramColours.opening.own ? 1 : 0
                    time: root.iconHologramTime
                    lens: 1 / 1.6
                    strength: Math.min(1.2, root.iconHologram.beamStrength)
                }
                // A hologram's gentle float, each icon a little after the one
                // before it.
                readonly property real hologramFloat: root.iconHologram.on && root.iconHologram.float
                    ? Math.sin(root.iconHologramTime * 1.3 + index * 0.7) * size * 0.025 : 0
                // This entry's motion: its own phase along the scene's.
                readonly property real pedestalBeat: root.pedestalMotionActive
                    ? Math.sin((root.pedestalPhase + index * 0.17) * 2 * Math.PI) : 0
                readonly property real pedestalBob: root.pedestalAnimation === "bob" ? pedestalBeat * size * 0.06
                    : root.pedestalAnimation === "float" ? pedestalBeat * size * 0.04 : 0
                readonly property real pedestalPulse: root.pedestalAnimation === "pulse" ? 1 + pedestalBeat * 0.06 : 1
                readonly property real pedestalSpin: !root.pedestalMotionActive ? 0
                    : root.pedestalAnimation === "spin" ? root.pedestalPhase * 360
                    : root.pedestalAnimation === "float" ? root.pedestalPhase * 90 : 0
                readonly property real pedestalSway: root.pedestalAnimation === "sway" ? pedestalBeat * 8 : 0
                readonly property real pedestalWobble: root.pedestalAnimation === "wobble" ? pedestalBeat * 10 : 0
                readonly property real pedestalShine: root.pedestalAnimation === "glow" ? 0.6 + 0.4 * pedestalBeat : 1
                readonly property real baseLift: size * 0.02
                // On a Möbius strip each entry rides the band's one face: its
                // stand and icon stay as on any panel - the stand on the face,
                // turned with the band there, the icon standing up from it
                // facing the viewer - and the band only decides whether the
                // two are in front of it or behind it (`depthPush` below).
                // Going round they pass through the twist to what looks like
                // the band's other side, behind it, and two laps on are back
                // in front. `surfaceTurn` turns the entry with the band; what
                // faces the viewer is turned back.
                readonly property bool riding: root.generatedPlatform !== null
                    && root.generatedPlatform.shape === "mobius"
                readonly property real surfaceTwist: Math.PI * (Number(rect.surface) || 0) - Math.PI / 2
                readonly property vector3d surfaceWay: {
                    if (!riding) return Qt.vector3d(1, 0, 0)
                    const turn = LayoutEngine.mobiusPoint((Number(rect.surface) || 0) % 1, 1).tangent * Math.PI / 180
                    return Qt.vector3d(Math.cos(turn), -Math.sin(turn), 0)
                }
                readonly property quaternion surfaceTurn: riding
                    ? Qt.quaternion(Math.cos(surfaceTwist / 2), surfaceWay.x * Math.sin(surfaceTwist / 2),
                                    surfaceWay.y * Math.sin(surfaceTwist / 2), 0)
                    : Qt.quaternion(1, 0, 0, 0)
                readonly property vector3d surfaceUp: surfaceTurn.times(Qt.vector3d(0, 0, 1))
                readonly property quaternion facing: riding ? surfaceTurn.inverted().times(root.iconFacing) : root.iconFacing
                readonly property vector3d facingRight: riding ? surfaceTurn.inverted().times(root.iconRight) : root.iconRight
                readonly property vector3d facingUp: riding ? surfaceTurn.inverted().times(root.iconUp) : root.iconUp
                readonly property vector3d facingForward: riding ? surfaceTurn.inverted().times(root.iconForward) : root.iconForward
                // How far the stand and its icon move along the line of sight,
                // as a share of their distance from the camera: moved so, each
                // of their points stays exactly where it is seen and as large,
                // and only what covers what changes. Riding the face turned
                // toward the viewer they come just in front of the band where
                // it is seen across the icon; riding the face turned away they
                // go just behind it, so the band hides what of them it covers
                // and nothing of the band cuts through them. The stand lies on
                // the face, on the right side of the band already.
                readonly property real depthPush: {
                    if (!riding) return root.edgePanel ? edgePush : 1
                    const camera = root.cameraInEntries, ahead = root.iconForward
                    const right = root.iconRight, upward = root.iconUp
                    const rest = position
                    const view = function(point) {
                        const away = point.minus(camera)
                        return { x: away.dotProduct(right), y: away.dotProduct(upward), depth: -away.dotProduct(ahead) }
                    }
                    // The icon's square on its stand, as seen: it faces the
                    // viewer, so all of it is as far away as its middle. A
                    // stand facing the viewer stands below it.
                    const shown = size * hoverScale
                    const middle = view(rest.plus(root.iconFacing.times(Qt.vector3d(pedestalTopX, footLift + shown / 2, 0))))
                    const foot = view(rest)
                    if (!(middle.depth > 0) || !(foot.depth > 0)) return 1
                    const box = { left: (middle.x - shown / 2) / middle.depth, right: (middle.x + shown / 2) / middle.depth,
                                  low: (middle.y - shown / 2) / middle.depth, high: (middle.y + shown / 2) / middle.depth }
                    if (!root.pedestalOnPanel)
                        box.low = Math.min(box.low, foot.y / foot.depth)
                    // The part of a straight piece of the band, p to q, seen
                    // inside the square: how far away its two ends are.
                    const inside = function(p, q) {
                        let from = 0, to = 1
                        const dx = q.x - p.x, dy = q.y - p.y, dd = q.depth - p.depth
                        const sides = [[p.x - box.left * p.depth, dx - box.left * dd],
                                       [box.right * p.depth - p.x, box.right * dd - dx],
                                       [p.y - box.low * p.depth, dy - box.low * dd],
                                       [box.high * p.depth - p.y, box.high * dd - dy]]
                        for (const side of sides) {
                            if (Math.abs(side[1]) < 1e-12) {
                                if (side[0] < 0) return null
                                continue
                            }
                            const t = -side[0] / side[1]
                            if (side[1] > 0) from = Math.max(from, t)
                            else to = Math.min(to, t)
                            if (from > to) return null
                        }
                        return [p.depth + dd * from, p.depth + dd * to]
                    }
                    // The band an icon and a half along it either way, as the
                    // platform builds it, a cross-section every sixth of an
                    // icon: its two faces and its two edges, with the shell of
                    // its glow round it, which is drawn as part of it.
                    const platform = root.generatedPlatform
                    const trackReach = platform.track * LayoutEngine.mobiusReach
                    const glowShell = root.panelGlowAmount > 0 ? 0.02 + 0.04 * root.panelGlowReach : 0
                    const half = Math.min(PlatformGeometry.maximumBand, Math.max(PlatformGeometry.minimumBand,
                        Number(root.resolvedBand) || 0.11)) + (PlatformGeometry.innerBevel + PlatformGeometry.outerBevel) / 2
                        + glowShell
                    const thick = (PlatformGeometry.topHeight - PlatformGeometry.bottomHeight) / 2 + glowShell
                    const sx = root.platformScale * root.shownScaleX, sy = root.platformScale * root.shownScaleY
                    const sz = root.platformScale * root.platformThickness
                    const step = size / 6 / Math.max(1e-6, LayoutEngine.mobiusLength() * trackReach * root.platformScale)
                    let near = Infinity, far = -Infinity
                    for (let index = -9; index <= 9; ++index) {
                        const share = (Number(rect.progress) || 0) + index * step
                        const at = LayoutEngine.mobiusPoint(share, trackReach)
                        const turn = at.tangent * Math.PI / 180
                        const nx = Math.sin(turn), ny = Math.cos(turn)
                        const twist = Math.PI * share - Math.PI / 2
                        const cosine = Math.cos(twist), sine = Math.sin(twist)
                        const lift = PlatformGeometry.standLift("mobius", share)
                        const point = function(a, h) {
                            return view(Qt.vector3d((at.x + nx * cosine * a - nx * sine * h) * sx,
                                                    (-at.y + ny * cosine * a - ny * sine * h) * sy,
                                                    (lift + sine * a + cosine * h) * sz))
                        }
                        const pieces = [[point(-half, -thick), point(half, -thick)], [point(-half, thick), point(half, thick)],
                                        [point(-half, -thick), point(-half, thick)], [point(half, -thick), point(half, thick)]]
                        for (const piece of pieces) {
                            if (!(piece[0].depth > 0) || !(piece[1].depth > 0)) continue
                            const seen = inside(piece[0], piece[1])
                            if (!seen) continue
                            near = Math.min(near, seen[0], seen[1])
                            far = Math.max(far, seen[0], seen[1])
                        }
                    }
                    if (!Number.isFinite(near)) return 1
                    const margin = size * 0.02
                    const before = Math.min(1, (near - margin) / middle.depth)
                    const behind = Math.max(1, (far + margin) / middle.depth)
                    // While the face turns across the line of sight the two
                    // move from one side of the band to the other gradually,
                    // so the band, which is thick even seen edge on, uncovers
                    // the icon as it comes out from behind its edge instead of
                    // letting it jump out at once.
                    const turned = surfaceUp.dotProduct(camera.minus(rest).normalized())
                    const share = Math.max(0, Math.min(1, (turned + 0.2) / 0.4))
                    const smooth = share * share * (3 - 2 * share)
                    return behind + (before - behind) * smooth
                }
                // That move, in the entry's own turned frame.
                readonly property vector3d depthShift: surfaceTurn.inverted()
                    .times(root.cameraInEntries.minus(position).times(1 - depthPush))
                // On an edge panel the same move brings the stand and the icon
                // in front of the tilted platform: the platform lies behind
                // the panel's own plane (its near edge in it), so they come
                // forward of that plane by an icon's size, which their own
                // depth does not reach back through. Moving the way they are
                // seen and shrinking in step, they stay where they are on the
                // screen and as large; a free panel's clearance move, along
                // the camera's axis, would enlarge them under an edge panel's
                // near camera.
                readonly property real edgePush: {
                    if (!root.edgePanel) return 1
                    const camera = root.cameraDistance
                    const front = Math.min(size, camera / 2)
                    const depth = scenePosition.z
                    return depth < front ? (camera - front) / Math.max(1e-6, camera - depth) : 1
                }
                // On a bent band the stand and its icon rise to the band's
                // highest point beneath the stand.
                readonly property real bendRise: root.platformBendSlope > 0 && root.pedestalDrawn
                    ? root.platformBendSlope * Math.min(pedestalScale * root.pedestalShapeReach / Math.max(1e-6, root.platformScale),
                        root.resolvedBand + PlatformGeometry.outerBevel) * root.platformScale * root.platformThickness
                    : 0
                // The stand and the icon face the viewer. Where the camera
                // looks across the platform a part of them would reach into it,
                // so the two move straight toward the viewer, which keeps them
                // in place on the screen, until nothing of them, the icon
                // magnified included, is below the platform's top.
                readonly property real facingClearance: {
                    const rz = facingRight.z, uz = facingUp.z
                    let lowest = root.pedestalDrawn ? root.pedestalExtentsUnit.lowest * pedestalScale : 0
                    const half = size * 0.85
                    for (const x of [pedestalTopX - half, pedestalTopX + half])
                        for (const y of [footLift, footLift + size * 1.7])
                            lowest = Math.min(lowest, x * rz + y * uz)
                    return Math.max(0, -lowest)
                }
                // Riding a Möbius strip, `depthPush` keeps them clear instead.
                readonly property real clearanceTravel: riding || root.edgePanel ? 0
                    : facingClearance / Math.max(0.3, Math.abs(facingForward.z))
                readonly property real tileLift: visual ? Math.max(-24, Math.min(96,
                    root.number(visual.tileSettings, "iconTileElevation", 0)))
                    * size / Math.max(1, Number(visual.logicalSize || 52)) : 0
                readonly property var tileMesh: root.tileMeshes[index] || null
                readonly property var tileMaterial: root.tileMaterial(visual)
                readonly property var bubbleMesh: root.bubbleMeshes[index] || null
                readonly property var bubbleMaterial: root.bubbleMaterial(visual)
                // Where the bubble's middle lies: the icon's own middle, in
                // the tile's frame.
                readonly property real bubbleMiddle: bubbleMesh !== null ? root.bubbleFit(visual).middle : 0
                readonly property color bubbleOutlineColour: String((visual && visual.tileSettings || ({})).iconTileBorderColor || "#94a3b8")
                readonly property real bubbleOutlineWidth: Math.max(0.5, Math.min(12,
                    root.number(visual ? visual.tileSettings : null, "iconTileBorderWidth", 1)))
                readonly property bool tileShown: visual !== null && visual.tileRenderingEnabled
                // An icon in a bubble faces the viewer: neither it nor its
                // bubble turns about the upright axis (no Spin, Wobble or
                // Swing turn, no hologram turning), so neither shows its thin
                // side; Icon turn still shows a 3D icon's depth.
                readonly property bool facesViewer: tileShown && bubbleMesh !== null
                readonly property real iconShare: facesViewer && visual
                    ? root.bubbleIconShare(visual.resolvedIconStyle.iconShape) : 1
                // Where the entry stands. On a generated platform the layout's
                // own position, carried onto the platform's centre line: the
                // platform is that layout's path. On a theme's mesh, its
                // direction from the scene centre on the theme's own track;
                // scaling a layout that is not the mesh's shape could put an
                // icon in the hole.
                readonly property real reachX: Number(rect.centerX) - root.width / 2
                readonly property real reachY: root.height / 2 - Number(rect.centerY)
                readonly property real reach: Math.hypot(reachX, reachY)
                onIconMotionChanged: root.hitTestsChanged()
                onGlyphMotionChanged: root.hitTestsChanged()
                onHoverScaleChanged: root.hitTestsChanged()
                onVisibleChanged: root.hitTestsChanged()
                readonly property var baseStandPoint: root.generatedPlatform
                    ? root.platformPlace(reachX, reachY)
                    : [(reach > 0.001 ? reachX / reach : 0) * root.entryTrackRadius / root.platformScale,
                       (reach > 0.001 ? reachY / reach : 1) * root.entryTrackRadius / root.platformScale]
                // The path's direction at the icon's place, degrees from x.
                readonly property real pathAngle: Math.atan2(baseStandPoint[1], baseStandPoint[0]) * 180 / Math.PI
                // A stand fixed to the panel: its turn so that it shows the
                // panel's orientation as seen at the panel's centre, and the
                // lift that keeps its tilted foot on the platform.
                readonly property real panelStandTilt: {
                    if (!root.pedestalOnPanel) return 0
                    const centre = root.cameraInEntries.minus(root.sightCentreInEntries).normalized()
                    const own = root.cameraInEntries.minus(position).normalized()
                    return Math.atan2(centre.crossProduct(own).length(), centre.dotProduct(own))
                }
                readonly property quaternion panelStandRotation: {
                    const half = function(angle) { return angle / 2 }
                    let view = Qt.quaternion(1, 0, 0, 0)
                    if (root.pedestalOnPanel) {
                        const centre = root.cameraInEntries.minus(root.sightCentreInEntries).normalized()
                        const own = root.cameraInEntries.minus(position).normalized()
                        const axis = centre.crossProduct(own)
                        const length = axis.length()
                        if (length > 1e-9) {
                            const angle = Math.atan2(length, centre.dotProduct(own))
                            const unit = axis.times(1 / length)
                            view = Qt.quaternion(Math.cos(half(angle)), unit.x * Math.sin(half(angle)),
                                                 unit.y * Math.sin(half(angle)), unit.z * Math.sin(half(angle)))
                        }
                        // Riding a Möbius strip the stand is turned with the
                        // band first; that turn is undone here, so the view's
                        // turn acts as on the panel and the band's after it.
                        if (riding)
                            view = surfaceTurn.inverted().times(view).times(surfaceTurn)
                    }
                    const along = (pathAngle + 90 + pedestalSpin) * Math.PI / 180
                    const sway = pedestalSway * Math.PI / 180, wobble = pedestalWobble * Math.PI / 180
                    return view.times(Qt.quaternion(Math.cos(along / 2), 0, 0, Math.sin(along / 2)))
                        .times(Qt.quaternion(Math.cos(sway / 2), Math.sin(sway / 2), 0, 0))
                        .times(Qt.quaternion(Math.cos(wobble / 2), 0, Math.sin(wobble / 2), 0))
                }
                readonly property var standPoint: {
                    // On a Möbius strip, as high as its strand is there.
                    const lift = root.generatedPlatform
                        ? PlatformGeometry.standLift(root.generatedPlatform.shape, rect.progress) : 0
                    const point = PlatformGeometry.foldPoint(
                        [baseStandPoint[0], baseStandPoint[1], root.baseTopHeight + lift],
                        root.platformFold, root.baseTopHeight).map(v => v * root.platformScale)
                    return [point[0] * root.shownScaleX, point[1] * root.shownScaleY, point[2]]
                }
                property alias glyphModel: glyphModel
                property alias glyphTexture: glyphTexture
                property alias inputAnchor: inputAnchor
                property alias standOnPanel: standOnPanel
                property alias standFacing: standFacing
                readonly property real glow: Math.max(root.number(iconMotion, "glow", 0),
                    root.number(glyphMotion, "glow", 0), root.number(tileMotion, "glow", 0))
                // An entry off its path is not drawn, and one leaving or coming
                // back onto an open path fades, exactly as its 2D delegate does.
                readonly property real trackVisibility: rect.visibility === undefined
                    ? (rect.onTrack === false ? 0 : 1) : Math.max(0, Math.min(1, Number(rect.visibility)))
                visible: trackVisibility > 0
                // The entry's origin is where its icon stands: on the platform's
                // track, on top of its pedestal. Motion offsets move it along.
                // The entry's origin travels with the layout; the icon's own
                // motion moves the icon alone (below) and the pedestal's
                // motion moves the pedestal alone, so neither drags the other.
                position: {
                    const rise = bendRise + baseLift + tileLift
                    if (!riding)
                        return Qt.vector3d(standPoint[0], standPoint[1], standPoint[2] * root.platformThickness + rise
                                       + root.decorationLift(index))
                    // On the band's face: its middle line at its strand's
                    // height, then out of the face as far as its top is.
                    const middle = PlatformGeometry.standLift("mobius", rect.progress) * root.platformScale
                    const out = root.baseTopHeight * root.platformScale * root.platformThickness + rise
                    return Qt.vector3d(standPoint[0] + surfaceUp.x * out, standPoint[1] + surfaceUp.y * out,
                                       middle + surfaceUp.z * out)
                }
                rotation: surfaceTurn
                opacity: root.number(iconMotion, "opacity", 1) * trackVisibility

                Texture {
                    id: glyphTexture
                    readonly property Item candidateSource:
                        entryNode.visual ? entryNode.visual.glyphTextureItem : null
                    // A detached source releases its layer before QObject destruction.
                    sourceItem: candidateSource && root.Window.window
                        && candidateSource.Window.window === root.Window.window
                        && entryNode.visual.meshVisualActive ? candidateSource : null
                }
                Texture {
                    id: tileTexture
                    readonly property Item candidateSource:
                        entryNode.visual ? entryNode.visual.tileTextureItem : null
                    sourceItem: candidateSource && root.Window.window
                        && candidateSource.Window.window === root.Window.window
                        && entryNode.visual.meshVisualActive ? candidateSource : null
                }
                // Qt Quick 3D refreshes a texture taken from an icon's layer
                // after every frame until the texture is deleted. The Loader
                // that drops this scene, or the repeater that drops this
                // entry, first turns the bindings above off and deletes the
                // node a moment later; an icon leaving the window in that
                // moment left a texture refreshing a freed layer, and Arch
                // Dock crashed when Panel Studio went from a 3D free panel to
                // the Top or Side panel. Destruction is
                // announced while the bindings still run: let go here.
                Component.onDestruction: {
                    glyphTexture.sourceItem = null
                    tileTexture.sourceItem = null
                }
                // What `depthPush` moves along the line of sight: the stand,
                // the theme's own parts for the entry and the icon.
                Node {
                    objectName: "mesh-stand-and-icon-" + entryNode.index
                    position: entryNode.depthShift
                    scale: Qt.vector3d(entryNode.depthPush, entryNode.depthPush, entryNode.depthPush)
                // With no stand, the hologram's lens lies on the platform under
                // its icon.
                Model {
                    objectName: "mesh-hologram-opening-on-panel-" + entryNode.index
                    source: "#Rectangle"
                    pickable: false
                    visible: entryNode.hologramLightShown && !root.pedestalDrawn
                    z: -(entryNode.baseLift + entryNode.tileLift) + entryNode.size * 0.005
                    scale: Qt.vector3d(entryNode.openingRadius * 3.2 / 100, entryNode.openingRadius * 3.2 / 100, 1)
                    materials: entryNode.openingMaterial
                }
                // The stand: its foot's middle on the platform at the icon's
                // place, turned by Stand facing, in the platform's material or
                // its own. Its own motion moves it, not the icon. It and the
                // icon share the move toward the viewer.
                // Panel: the stand lies on the platform at the icon's place,
                // part of the panel, turned with its path there, its foot on
                // the platform's top whatever moves the icon.
                Node {
                    objectName: "mesh-stand-panel-" + entryNode.index
                    visible: root.pedestalOnPanel
                    z: -(entryNode.baseLift + entryNode.tileLift) + entryNode.pedestalBob
                        + entryNode.pedestalScale * root.pedestalShapeReach * Math.sin(entryNode.panelStandTilt)
                    rotation: entryNode.panelStandRotation
                    IconStyle3D {
                        id: standOnPanel
                        objectName: "mesh-pedestal-" + entryNode.index
                        underGlow: root.pedestalGlowAmount > 0 && !root.standHologramShown
                        meshData: root.pedestalShownMesh
                        materialData: root.pedestalMaterialData
                        shellMaterial: root.standHologramShown ? standHologramMaterial : null
                        reliefMaterial: root.pedestalPictureRelief ? standRelief : null
                        surfaceTexture: root.pedestalTexture
                        surfaceNormalTexture: root.pedestalNormalTexture
                        emissionScale: root.emissionScale * entryNode.pedestalShine * (1 + entryNode.glow)
                        scale: Qt.vector3d(entryNode.pedestalScale * entryNode.pedestalPulse,
                                           entryNode.pedestalScale * entryNode.pedestalPulse,
                                           Math.max(1e-3, entryNode.pedestalDepth))
                        // The hologram's light leaves its lens here, in the
                        // middle of the stand's top (or, through a ring or a
                        // halo, the panel inside it).
                        Model {
                            objectName: "mesh-hologram-opening-panel-" + entryNode.index
                            source: "#Rectangle"
                            pickable: false
                            visible: entryNode.hologramLightShown
                            z: entryNode.openingSpot.height + 0.004
                            scale: Qt.vector3d(entryNode.openingSpot.radius * 3.2 / 100,
                                               entryNode.openingSpot.radius * 3.2 / 100, 1)
                            materials: entryNode.openingMaterial
                        }
                        IconStyle3D {
                            objectName: "mesh-pedestal-glow-" + entryNode.index
                            visible: root.pedestalGlowAmount > 0 && !root.standHologramShown
                            meshData: root.pedestalMesh
                            materialData: ({})
                            shellMaterial: standGlowMaterial
                            opacity: entryNode.pedestalShine
                            position: root.pedestalGlowShift
                            scale: Qt.vector3d(root.pedestalGlowSpread, root.pedestalGlowSpread, root.pedestalGlowSpread)
                        }
                    }
                }
                Node {
                    objectName: "mesh-stand-frame-" + entryNode.index
                    position: entryNode.facingForward.times(entryNode.clearanceTravel)
                    rotation: entryNode.facing
                    visible: root.pedestalDrawn && !root.pedestalOnPanel
                    Node {
                        position: Qt.vector3d(root.pedestalAxis.x * entryNode.pedestalBob,
                                              root.pedestalAxis.y * entryNode.pedestalBob,
                                              root.pedestalAxis.z * entryNode.pedestalBob)
                        eulerRotation.z: -root.pedestalAxisLean + entryNode.pedestalSway
                        Node {
                            eulerRotation.x: -90 + root.pedestalAxisTilt + entryNode.pedestalWobble
                            IconStyle3D {
                                id: standFacing
                                objectName: "mesh-pedestal-facing-" + entryNode.index
                                underGlow: root.pedestalGlowAmount > 0 && !root.standHologramShown
                                meshData: root.pedestalShownMesh
                                materialData: root.pedestalMaterialData
                                shellMaterial: root.standHologramShown ? standHologramMaterial : null
                                reliefMaterial: root.pedestalPictureRelief ? standRelief : null
                                surfaceTexture: root.pedestalTexture
                                surfaceNormalTexture: root.pedestalNormalTexture
                                emissionScale: root.emissionScale * entryNode.pedestalShine * (1 + entryNode.glow)
                                eulerRotation.z: entryNode.pedestalSpin
                                scale: Qt.vector3d(entryNode.pedestalScale * entryNode.pedestalPulse,
                                                   entryNode.pedestalScale * entryNode.pedestalPulse,
                                                   Math.max(1e-3, entryNode.pedestalDepth))
                                // The hologram's light leaves its lens here, in the
                                // middle of the stand's top (or, through a ring or a
                                // halo, the panel inside it).
                                Model {
                                    objectName: "mesh-hologram-opening-facing-" + entryNode.index
                                    source: "#Rectangle"
                                    pickable: false
                                    visible: entryNode.hologramLightShown
                                    z: entryNode.openingSpot.height + 0.004
                                    scale: Qt.vector3d(entryNode.openingSpot.radius * 3.2 / 100,
                                                       entryNode.openingSpot.radius * 3.2 / 100, 1)
                                    materials: entryNode.openingMaterial
                                }
                                IconStyle3D {
                                    objectName: "mesh-pedestal-facing-glow-" + entryNode.index
                                    visible: root.pedestalGlowAmount > 0 && !root.standHologramShown
                                    meshData: root.pedestalMesh
                                    materialData: ({})
                                    shellMaterial: standGlowMaterial
                                    opacity: entryNode.pedestalShine
                                    position: root.pedestalGlowShift
                                    scale: Qt.vector3d(root.pedestalGlowSpread, root.pedestalGlowSpread,
                                                       root.pedestalGlowSpread)
                                }
                            }
                        }
                    }
                }
                // A theme's own icon base, lying on the platform around the
                // pedestal's foot.
                IconStyle3D {
                    objectName: "mesh-icon-base-" + entryNode.index
                    meshData: iconResource.meshData
                    materialData: iconResource.materialData
                    surfaceTexture: root.themeTexture
                    surfaceNormalTexture: root.themeNormalTexture
                    visible: !root.generatedSpec && entryNode.tileShown && entryNode.elevation > 0
                    z: -(entryNode.baseLift + entryNode.tileLift)
                    scale: Qt.vector3d(entryNode.size * 0.3, entryNode.size * 0.3, entryNode.size * 0.15)
                    emissionScale: root.emissionScale * (1 + entryNode.glow)
                }
                // A theme's own parts for each entry (such as a ring that sinks
                // as the panel closes), at the pedestal's foot.
                Node {
                    objectName: "mesh-tile-parts-" + entryNode.index
                    visible: !entryNode.visual || (entryNode.visual.tileRenderingEnabled
                        && !entryNode.visual.customTileActive)
                    z: -(entryNode.baseLift + entryNode.tileLift)
                    scale: Qt.vector3d(entryNode.size * 0.55, entryNode.size * 0.55, entryNode.size * 0.55)
                    Repeater3D {
                        model: root.entryParts.length
                        delegate: IconStyle3D {
                            required property int index
                            objectName: "mesh-entry-part-" + entryNode.index + "-" + index
                            partDefinition: root.entryParts[index].definition
                            visible: partDefinition.mechanism === root.mechanism
                            meshData: root.entryParts[index].resources.mesh || null
                            materialData: root.entryParts[index].resources.material || ({})
                            surfaceTexture: root.themeTexture
                            surfaceNormalTexture: root.themeNormalTexture
                            openAmount: root.partOpenAmount(partDefinition)
                            emissionScale: root.emissionScale * (1 + entryNode.glow)
                        }
                    }
                }
                // The icon faces the viewer at every pitch, yaw and roll, so it
                // keeps the application's own glyph readable instead of lying
                // flat on the platform; it stands on its pedestal.
                Node {
                    objectName: "mesh-icon-" + entryNode.index
                    rotation: entryNode.facing
                    // The icon's own motion offsets, on the icon alone, and the
                    // stand's move toward the viewer.
                    position: entryNode.facingForward.times(entryNode.clearanceTravel).plus(
                        Qt.vector3d(root.number(entryNode.iconMotion, "x", 0),
                                    -root.number(entryNode.iconMotion, "y", 0), 0))
                    // The hologram's light, a strip facing the viewer from its
                    // lens up into the icon's near part.
                    Model {
                        id: hologramLightStrip
                        objectName: "mesh-hologram-light-" + entryNode.index
                        source: "#Rectangle"
                        pickable: false
                        visible: entryNode.hologramLightShown
                        readonly property vector3d from: entryNode.openingInIcon
                        readonly property vector3d to: Qt.vector3d(entryNode.iconX,
                            entryNode.footLift + entryNode.size * 0.42, 0)
                        readonly property real length: Math.max(1, Math.hypot(to.y - from.y, to.z - from.z))
                        readonly property real sheet: 2 * Math.max(entryNode.openingRadius * 1.04, entryNode.size * 0.59)
                        position: Qt.vector3d(from.x, (from.y + to.y) / 2, (from.z + to.z) / 2)
                        eulerRotation.x: Math.atan2(to.z - from.z, to.y - from.y) * 180 / Math.PI
                        scale: Qt.vector3d(sheet / 100, length / 100, 1)
                        materials: HologramLight {
                            picture.texture: glyphTexture
                            tint: root.hologramColour(root.iconHologramColours.light)
                            own: root.iconHologramColours.light && root.iconHologramColours.light.own ? 1 : 0
                            motion: Qt.vector4d(root.hologramLightFlags.rise, root.hologramLightFlags.drift,
                                                root.hologramLightFlags.pulse, root.hologramLightFlags.sweep)
                            time: root.hologramLightFlags.moving ? root.iconHologramTime : 0
                            startHalf: entryNode.openingRadius * 0.95 / hologramLightStrip.sheet
                            endHalf: entryNode.size * 0.59 / hologramLightStrip.sheet
                            streaks: 30
                            sparks: 0.1
                            seed: entryNode.index * 0.37
                            fade: 1.6
                            strength: root.iconHologram.beamStrength
                        }
                    }
                    Node {
                        objectName: "mesh-icon-body-" + entryNode.index
                        // Its foot floats over the stand's top, centred on it,
                        // or over its hologram's light.
                        x: entryNode.iconX
                        y: entryNode.footLift + entryNode.hologramFloat - (entryNode.edgeCentred
                            ? (entryNode.hoverScale * root.number(entryNode.iconMotion, "scale", 1)
                               * root.number(entryNode.iconMotion, "scaleY", 1) - 1) * entryNode.size / 2 : 0)
                        // Hover magnification and icon motion grow the icon about
                        // its foot, so it stays standing on the pedestal. A turn
                        // in the icon's plane (Spin, Wobble, Swing) becomes a turn
                        // about the vertical axis here, the way a stand spins,
                        // instead of a roll of the picture; so does a hologram's
                        // turning, each icon a little after the one before it.
                        eulerRotation: Qt.vector3d(0, entryNode.facesViewer ? 0
                            : root.number(entryNode.iconMotion, "rotateY", 0)
                            + root.number(entryNode.iconMotion, "rotateZ", 0)
                            + (root.hologramTurn !== 0 ? root.hologramTurn
                               + (root.iconHologram.turning === "turn" ? entryNode.index * 37 : 0) : 0),
                            -Number(entryNode.rect.rotation || 0))
                        scale: Qt.vector3d(entryNode.hoverScale * root.number(entryNode.iconMotion, "scale", 1)
                                * root.number(entryNode.iconMotion, "scaleX", 1),
                            entryNode.hoverScale * root.number(entryNode.iconMotion, "scale", 1)
                                * root.number(entryNode.iconMotion, "scaleY", 1), 1)
                        PrincipledMaterial {
                            id: tileFaceMaterial
                            lighting: entryNode.tileMaterial.look === "minimal"
                                ? PrincipledMaterial.NoLighting : PrincipledMaterial.FragmentLighting
                            alphaMode: PrincipledMaterial.Blend
                            baseColor: entryNode.tileMaterial.look === "minimal" ? "#ffffff" : entryNode.tileMaterial.baseColor
                            baseColorMap: tileTexture
                            metalness: entryNode.tileMaterial.metalness
                            roughness: entryNode.tileMaterial.roughness
                            opacity: entryNode.tileMaterial.surfaceOpacity
                            // Riding a Möbius strip, its solid part keeps whatever
                            // see-through part of the panel is drawn after it (a
                            // glow, a rim) behind it.
                            depthDrawMode: root.tileGlowAmount > 0 ? Material.AlwaysDepthDraw
                                : entryNode.riding ? Material.OpaquePrePassDepthDraw : Material.OpaqueOnlyDepthDraw
                            cullMode: Material.NoCulling
                        }
                        IconStyle3D {
                            objectName: "mesh-tile-solid-" + entryNode.index
                            underGlow: root.tileGlowAmount > 0
                            meshData: entryNode.tileMesh
                            materialData: entryNode.tileMaterial
                            faceMaterial: tileFaceMaterial
                            // The tile's glow (Icon Tiles > Glow): a shell of the
                            // tile, a little larger, seen only from inside.
                            IconStyle3D {
                                objectName: "mesh-tile-glow-" + entryNode.index
                                visible: root.tileGlowAmount > 0
                                meshData: entryNode.tileMesh
                                materialData: ({})
                                shellMaterial: tileGlowMaterial
                                scale: Qt.vector3d(root.tileGlowSpread, root.tileGlowSpread, root.tileGlowSpread)
                            }
                            visible: entryNode.tileShown && entryNode.tileMesh !== null && root.collapseProgress < 1
                            position: Qt.vector3d(root.number(entryNode.tileMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.tileMotion, "y", 0), -entryNode.size * 0.001)
                            eulerRotation: Qt.vector3d(0, root.number(entryNode.tileMotion, "rotateY", 0)
                                + root.number(entryNode.tileMotion, "rotateZ", 0), 0)
                            scale: Qt.vector3d(entryNode.size / 2 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleX", 1),
                                entryNode.size / 2 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleY", 1), entryNode.size / 2)
                            opacity: root.number(entryNode.tileMotion, "opacity", 1) * (1 - root.collapseProgress)
                        }
                        // The bubble: a closed glass shell in the tile's shape
                        // round the icon, in the tile's place, its middle on the
                        // icon's own middle so the icon sits at its centre from
                        // every side, its outline drawn round it (a shell a
                        // little larger, seen only from inside) so the bubble
                        // shows for all its clearness. It follows the tile and
                        // grows as far as the icon swells past it, so the icon
                        // never leaves it.
                        IconStyle3D {
                            id: bubbleShell
                            objectName: "mesh-tile-bubble-" + entryNode.index
                            readonly property real holdScale: Math.max(root.number(entryNode.tileMotion, "scale", 1),
                                root.number(entryNode.glyphMotion, "scale", 1))
                            meshData: entryNode.bubbleMesh
                            materialData: entryNode.bubbleMaterial
                            // Drawn after the icon inside it, whose middle it
                            // shares, so the glass lies over the picture and never
                            // hides it (see-through parts are drawn furthest first,
                            // and a tie falls either way), and before its outline:
                            // sorted a quarter of the icon nearer, far less than
                            // lies between icons that overlap on the screen.
                            depthBias: -Math.sqrt(0.25 * entryNode.size * Math.abs(sceneContent.scale.x))
                            visible: entryNode.tileShown && entryNode.bubbleMesh !== null && root.collapseProgress < 1
                            position: Qt.vector3d(root.number(entryNode.tileMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.tileMotion, "y", 0),
                                entryNode.bubbleMiddle * entryNode.size / 2)
                            scale: Qt.vector3d(entryNode.size / 2 * holdScale
                                    * root.number(entryNode.tileMotion, "scaleX", 1),
                                entryNode.size / 2 * holdScale
                                    * root.number(entryNode.tileMotion, "scaleY", 1), entryNode.size / 2 * holdScale)
                            opacity: root.number(entryNode.tileMotion, "opacity", 1) * (1 - root.collapseProgress)
                            Model {
                                objectName: "mesh-tile-bubble-outline-" + entryNode.index
                                readonly property real spread: 1 + 2 * entryNode.bubbleOutlineWidth / Math.max(1, entryNode.size)
                                pickable: false
                                visible: entryNode.bubbleMesh !== null
                                // After the glass, whose depth hides all of it but
                                // the rim.
                                depthBias: -Math.sqrt(0.3 * entryNode.size * Math.abs(sceneContent.scale.x))
                                scale: Qt.vector3d(spread, spread, spread)
                                // The glass's own mesh, not built a second time.
                                geometry: bubbleShell.geometry
                                materials: PrincipledMaterial {
                                    lighting: PrincipledMaterial.NoLighting
                                    baseColor: entryNode.bubbleOutlineColour
                                    opacity: 0.85
                                    alphaMode: PrincipledMaterial.Blend
                                    cullMode: Material.FrontFaceCulling
                                }
                            }
                            // The tile's glow (Icon Tiles > Glow) round the bubble:
                            // the glass's mesh a little larger, seen only from
                            // inside, after the glass, whose depth keeps its light
                            // round the bubble and never through it.
                            Model {
                                objectName: "mesh-tile-bubble-glow-" + entryNode.index
                                pickable: false
                                visible: root.tileGlowAmount > 0 && entryNode.bubbleMesh !== null
                                depthBias: -Math.sqrt(0.35 * entryNode.size * Math.abs(sceneContent.scale.x))
                                scale: Qt.vector3d(root.tileGlowSpread, root.tileGlowSpread, root.tileGlowSpread)
                                geometry: bubbleShell.geometry
                                materials: [tileGlowMaterial]
                            }
                        }
                        Model {
                            objectName: "mesh-tile-glow-flat-" + entryNode.index
                            source: "#Rectangle"
                            pickable: false
                            visible: root.tileGlowAmount > 0 && entryNode.tileShown && entryNode.tileMesh === null
                                && entryNode.bubbleMesh === null && root.collapseProgress < 1
                            position: Qt.vector3d(root.number(entryNode.tileMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.tileMotion, "y", 0), -entryNode.size * 0.004)
                            scale: Qt.vector3d(entryNode.size / 100 * root.tileGlowSpread,
                                               entryNode.size / 100 * root.tileGlowSpread, 1)
                            materials: PrincipledMaterial {
                                baseColor: "#000000"
                                roughness: 1
                                specularAmount: 0
                                emissiveFactor: root.tileGlowLight
                                opacityMap: tileTexture
                                opacityChannel: Material.A
                                alphaMode: PrincipledMaterial.Blend
                                cullMode: Material.NoCulling
                            }
                        }
                        Model {
                            objectName: "mesh-tile-" + entryNode.index
                            source: "#Rectangle"
                            pickable: false
                            // Not under a bubble, which takes the tile's place.
                            visible: entryNode.tileShown && entryNode.tileMesh === null && entryNode.bubbleMesh === null
                                && root.collapseProgress < 1
                            position: Qt.vector3d(root.number(entryNode.tileMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.tileMotion, "y", 0), 0)
                            eulerRotation: Qt.vector3d(0, root.number(entryNode.tileMotion, "rotateY", 0)
                                + root.number(entryNode.tileMotion, "rotateZ", 0), 0)
                            scale: Qt.vector3d(entryNode.size / 100 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleX", 1),
                                entryNode.size / 100 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleY", 1), 1)
                            opacity: root.number(entryNode.tileMotion, "opacity", 1) * (1 - root.collapseProgress)
                            materials: tileFaceMaterial
                        }
                        // The icon as a solid (Icon depth): made into an object
                        // (Icon relief "Modelled", IconModel: its front seen
                        // from either side) or its own picture's outline cut
                        // from its transparency, drawn as a block or its parts
                        // rounded (the picture on front and back); the solid's
                        // back where the flat picture would lie, turned by Icon
                        // turn so its depth shows.
                        Model {
                            id: solidModel
                            objectName: "mesh-icon-solid-" + entryNode.index
                            readonly property var mesh: entryNode.visual ? entryNode.visual.solidMesh : null
                            readonly property bool shown: root.iconDepth > 0 && mesh !== null
                                && entryNode.visual !== null && root.collapseProgress < 1
                            // A 3D icon (or a 3D hologram), not a flat hologram's
                            // thin card: an icon is one or the other, never a 3D
                            // icon with a flat copy of its picture.
                            readonly property bool threeD: root.iconSolid && shown
                            function toVectors(values, dimension) {
                                return (values || []).map(function(value) {
                                    return dimension === 3 ? Qt.vector3d(value[0], value[1], value[2])
                                                           : Qt.vector2d(value[0], value[1])
                                })
                            }
                            // An object is drawn from its model, in a material of
                            // its own (IconSurface).
                            readonly property bool twoSided: mesh !== null && mesh.native === true
                            pickable: false
                            visible: shown
                            // Its back where the flat picture lay.
                            // As a hologram, its middle on the axis it turns about.
                            position: Qt.vector3d(glyphModel.position.x, glyphModel.position.y,
                                                  glyphModel.position.z + (mesh ? mesh.depth : 50) * scale.z
                                                      * (root.iconHologram.on ? 0.5 : 1))
                            eulerRotation: Qt.vector3d(0, glyphModel.eulerRotation.y + root.iconTurn, 0)
                            scale: Qt.vector3d(glyphModel.scale.x, glyphModel.scale.y,
                                               glyphModel.scale.x * IconSolid.depthStretch(mesh, root.iconDepth))
                            opacity: glyphModel.pictureOpacity
                            geometry: twoSided ? modelledGeometry : pointsGeometry
                            property Geometry modelledGeometry: IconModelGeometry {
                                model: solidModel.twoSided ? solidModel.mesh : null
                            }
                            property Geometry pointsGeometry: ProceduralMesh {
                                readonly property var points: solidModel.mesh && !solidModel.twoSided ? solidModel.mesh : null
                                positions: points ? solidModel.toVectors(points.positions, 3) : []
                                normals: points ? solidModel.toVectors(points.normals, 3) : []
                                uv0s: points ? solidModel.toVectors(points.uv0s, 2) : []
                                indexes: points ? points.indexes : []
                                primitiveMode: ProceduralMesh.Triangles
                            }
                            // An object's surface: each point in the object's
                            // own colour there, the light modelling it as far as
                            // Icon lighting says.
                            property Material surfaceMaterial: IconSurface {
                                roughness: 0.7 - 0.5 * root.iconLighting
                                own: 0.5 * (1 - root.iconLighting)
                            }
                            property Material litMaterial: PrincipledMaterial {
                                lighting: PrincipledMaterial.FragmentLighting
                                baseColorMap: glyphTexture
                                // The light models the parts as far as Icon
                                // lighting says; the rest is the picture's own
                                // brightness, so its colours stay readable in
                                // shade.
                                emissiveMap: glyphTexture
                                emissiveFactor: Qt.vector3d(0.5 * (1 - root.iconLighting), 0.5 * (1 - root.iconLighting),
                                                            0.5 * (1 - root.iconLighting))
                                // A see-through part inside the picture stays
                                // see-through on the caps; the cut lies below
                                // the outline's own threshold, so the caps reach
                                // their walls.
                                alphaMode: PrincipledMaterial.Mask
                                alphaCutoff: 0.25
                                // Glossier as the light takes over.
                                roughness: 0.7 - 0.5 * root.iconLighting
                                metalness: 0
                                cullMode: Material.BackFaceCulling
                            }
                            // As a flat hologram's thin card: its picture drawn
                            // as light.
                            property Material hologramMaterial: HologramSurface {
                                picture.texture: glyphTexture
                                usePicture: 1
                                outlineColour: root.hologramColour(root.iconHologramColours.outline)
                                inlineColour: root.hologramColour(root.iconHologramColours.inline)
                                volumeColour: root.hologramColour(root.iconHologramColours.volume)
                                own: root.hologramOwn(root.iconHologramColours)
                                time: root.iconHologramTime
                                flicker: root.hologramMoving ? root.iconHologram.flicker : 0
                                seed: entryNode.index * 1.37
                                brightness: 1 + 0.6 * root.iconHologram.glow
                                cullMode: Material.BackFaceCulling
                            }
                            // As a 3D hologram: the solid itself made of light,
                            // its shape showing from every side.
                            property Material solidHologramMaterial: HologramSolid {
                                objectName: "mesh-icon-solid-hologram-" + entryNode.index
                                picture.texture: glyphTexture
                                outlineColour: root.hologramColour(root.iconHologramColours.outline)
                                inlineColour: root.hologramColour(root.iconHologramColours.inline)
                                volumeColour: root.hologramColour(root.iconHologramColours.volume)
                                own: root.hologramOwn(root.iconHologramColours)
                                time: root.iconHologramTime
                                flicker: root.hologramMoving ? root.iconHologram.flicker : 0
                                seed: entryNode.index * 1.37
                                brightness: 1 + 0.6 * root.iconHologram.glow
                            }
                            // An object as a 3D hologram: made of light from its own
                            // colours (HologramObject).
                            property Material objectHologramMaterial: HologramObject {
                                objectName: "mesh-icon-object-hologram-" + entryNode.index
                                outlineColour: solidModel.solidHologramMaterial.outlineColour
                                inlineColour: solidModel.solidHologramMaterial.inlineColour
                                volumeColour: solidModel.solidHologramMaterial.volumeColour
                                own: solidModel.solidHologramMaterial.own
                                time: root.iconHologramTime
                                flicker: solidModel.solidHologramMaterial.flicker
                                seed: entryNode.index * 1.37
                                brightness: solidModel.solidHologramMaterial.brightness
                            }
                            materials: !root.iconHologram.on ? (twoSided ? [surfaceMaterial] : [litMaterial])
                                : threeD ? [twoSided ? objectHologramMaterial : solidHologramMaterial] : [hologramMaterial]
                            // A 3D icon's glows (Icons > Glow, a hologram's glow):
                            // its own solid a little larger, seen only from inside,
                            // so the light shines round its shape from every side.
                            Model {
                                objectName: "mesh-icon-solid-glow-" + entryNode.index
                                pickable: false
                                visible: solidModel.threeD && root.iconGlowAmount > 0
                                scale: Qt.vector3d(root.iconGlowSpread, root.iconGlowSpread, root.iconGlowSpread)
                                geometry: solidModel.geometry
                                materials: PrincipledMaterial {
                                    baseColor: "#000000"
                                    roughness: 1
                                    specularAmount: 0
                                    emissiveFactor: root.iconGlowLight
                                    alphaMode: PrincipledMaterial.Blend
                                    cullMode: Material.FrontFaceCulling
                                }
                            }
                            Model {
                                objectName: "mesh-hologram-solid-glow-" + entryNode.index
                                readonly property real spread: 1.06 + 0.12 * root.iconHologram.glowReach
                                // In the glow's colour, or the picture's own as one
                                // colour, its mean, as bright as a flat hologram's glow
                                // in its picture's colours: a solid's glow wears no
                                // picture.
                                readonly property bool ownGlow: Boolean(root.iconHologramColours.glow
                                    && root.iconHologramColours.glow.own && solidModel.mesh && solidModel.mesh.colour)
                                readonly property var glowRgb: ownGlow ? solidModel.mesh.colour
                                    : PlatformGeometry.parseColour(root.hologramColour(root.iconHologramColours.glow))
                                        || [0.13, 0.85, 1, 1]
                                pickable: false
                                visible: solidModel.threeD && root.iconHologram.on && root.iconHologram.glow > 0
                                scale: Qt.vector3d(spread, spread, spread)
                                opacity: 0.85
                                geometry: solidModel.geometry
                                materials: PrincipledMaterial {
                                    baseColor: "#000000"
                                    roughness: 1
                                    specularAmount: 0
                                    emissiveFactor: parent.ownGlow
                                        ? Qt.vector3d(1.4 * root.iconHologram.glow * parent.glowRgb[0],
                                                      1.4 * root.iconHologram.glow * parent.glowRgb[1],
                                                      1.4 * root.iconHologram.glow * parent.glowRgb[2])
                                        : root.glowLight(parent.glowRgb, root.iconHologram.glow)
                                    alphaMode: PrincipledMaterial.Blend
                                    cullMode: Material.FrontFaceCulling
                                    depthDrawMode: Material.NeverDepthDraw
                                }
                            }
                        }
                        Model {
                            id: glyphModel
                            pickable: true
                            objectName: "mesh-glyph-" + entryNode.index
                            source: "#Rectangle"
                            visible: entryNode.visual !== null && root.collapseProgress < 1
                            // In front of its tile.
                            position: Qt.vector3d(root.number(entryNode.glyphMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.glyphMotion, "y", 0),
                                entryNode.size * 0.01)
                            eulerRotation: Qt.vector3d(0, entryNode.facesViewer ? 0
                                : root.number(entryNode.glyphMotion, "rotateY", 0)
                                + root.number(entryNode.glyphMotion, "rotateZ", 0), 0)
                            // In a bubble at the share it is drawn at there; its
                            // solid, glows and mirror image take their size from it.
                            scale: Qt.vector3d(entryNode.size / 100 * entryNode.iconShare
                                    * root.number(entryNode.glyphMotion, "scale", 1)
                                    * root.number(entryNode.glyphMotion, "scaleX", 1),
                                entryNode.size / 100 * entryNode.iconShare * root.number(entryNode.glyphMotion, "scale", 1)
                                    * root.number(entryNode.glyphMotion, "scaleY", 1), 1)
                            // The picture's own opacity, which its solid, its glow
                            // and its mirror image share.
                            readonly property real pictureOpacity: root.number(entryNode.glyphMotion, "opacity", 1)
                                * (1 - root.collapseProgress)
                            // Drawn clear while the icon's solid stands in for it,
                            // yet still there: the scene's hit tests and the
                            // water's mirror images go by this picture.
                            opacity: pictureOpacity * (solidModel.shown ? 0 : 1)
                            property Material litMaterial: PrincipledMaterial {
                                lighting: PrincipledMaterial.NoLighting
                                alphaMode: PrincipledMaterial.Blend
                                baseColorMap: glyphTexture
                                cullMode: Material.NoCulling
                                // Riding a Möbius strip, the picture's solid part keeps
                                // the panel's see-through parts drawn after it (its
                                // glowing rim) behind it, as they are; its clear corners
                                // hide nothing.
                                depthDrawMode: entryNode.riding ? Material.OpaquePrePassDepthDraw
                                    : Material.OpaqueOnlyDepthDraw
                            }
                            // As a hologram, until its solid is cut: its picture
                            // drawn as light, seen from both sides as it turns.
                            property Material hologramMaterial: HologramSurface {
                                picture.texture: glyphTexture
                                usePicture: 1
                                outlineColour: root.hologramColour(root.iconHologramColours.outline)
                                inlineColour: root.hologramColour(root.iconHologramColours.inline)
                                volumeColour: root.hologramColour(root.iconHologramColours.volume)
                                own: root.hologramOwn(root.iconHologramColours)
                                time: root.iconHologramTime
                                flicker: root.hologramMoving ? root.iconHologram.flicker : 0
                                seed: entryNode.index * 1.37
                                brightness: 1 + 0.6 * root.iconHologram.glow
                                cullMode: Material.NoCulling
                            }
                            materials: root.iconHologram.on ? [hologramMaterial] : [litMaterial]
                        }
                        // A flat hologram's glow: its picture's shape, a little
                        // larger, just behind it, brighter than white in the glow's
                        // colour or the picture's own, which the view spreads round
                        // it. A 3D hologram's is a shell of its solid.
                        Model {
                            objectName: "mesh-hologram-glow-" + entryNode.index
                            source: "#Rectangle"
                            pickable: false
                            visible: root.iconHologram.on && root.iconHologram.glow > 0 && glyphModel.visible
                                && !solidModel.threeD
                            position: Qt.vector3d(glyphModel.position.x, glyphModel.position.y, -entryNode.size * 0.02)
                            eulerRotation: glyphModel.eulerRotation
                            scale: Qt.vector3d(glyphModel.scale.x * (1.06 + 0.12 * root.iconHologram.glowReach),
                                               glyphModel.scale.y * (1.06 + 0.12 * root.iconHologram.glowReach), 1)
                            opacity: glyphModel.pictureOpacity * 0.85
                            readonly property var glowRgb: PlatformGeometry.parseColour(
                                root.hologramColour(root.iconHologramColours.glow)) || [0.13, 0.85, 1, 1]
                            readonly property bool ownGlow: Boolean(root.iconHologramColours.glow
                                && root.iconHologramColours.glow.own)
                            materials: PrincipledMaterial {
                                baseColor: "#000000"
                                roughness: 1
                                specularAmount: 0
                                emissiveMap: parent.ownGlow ? glyphTexture : null
                                emissiveFactor: parent.ownGlow
                                    ? Qt.vector3d(1.4 * root.iconHologram.glow, 1.4 * root.iconHologram.glow,
                                                  1.4 * root.iconHologram.glow)
                                    : root.glowLight(parent.glowRgb, root.iconHologram.glow)
                                opacityMap: glyphTexture
                                opacityChannel: Material.A
                                alphaMode: PrincipledMaterial.Blend
                                cullMode: Material.NoCulling
                                depthDrawMode: Material.NeverDepthDraw
                            }
                        }
                        // A flat icon's glow (Icons > Glow): its picture's shape, a
                        // little larger, just behind it and brighter than white,
                        // which the view spreads round the picture. A 3D icon's is
                        // a shell of its solid.
                        Model {
                            objectName: "mesh-glyph-glow-" + entryNode.index
                            source: "#Rectangle"
                            pickable: false
                            visible: root.iconGlowAmount > 0 && glyphModel.visible && !solidModel.threeD
                            position: Qt.vector3d(glyphModel.position.x, glyphModel.position.y, entryNode.size * 0.006)
                            eulerRotation: glyphModel.eulerRotation
                            scale: Qt.vector3d(glyphModel.scale.x * root.iconGlowSpread,
                                               glyphModel.scale.y * root.iconGlowSpread, 1)
                            opacity: glyphModel.pictureOpacity
                            materials: PrincipledMaterial {
                                baseColor: "#000000"
                                roughness: 1
                                specularAmount: 0
                                emissiveFactor: root.iconGlowLight
                                opacityMap: glyphTexture
                                opacityChannel: Material.A
                                alphaMode: PrincipledMaterial.Blend
                                cullMode: Material.NoCulling
                            }
                        }
                    }
                }
                }
                // The icon's resting square, untouched by hover, motion and
                // the move along the line of sight: its projection is the
                // entry's input rectangle, so the rectangle does not breathe
                // under the pointer, and the folder openings stand where the
                // icon stands.
                Node {
                    objectName: "mesh-icon-rest-" + entryNode.index
                    rotation: entryNode.facing
                    position: entryNode.facingForward.times(entryNode.clearanceTravel).plus(
                        Qt.vector3d(root.number(entryNode.iconMotion, "x", 0),
                                    -root.number(entryNode.iconMotion, "y", 0), 0))
                    Node {
                        id: inputAnchor
                        objectName: "mesh-input-anchor-" + entryNode.index
                        x: entryNode.iconX
                        y: entryNode.footLift + entryNode.size / 2
                    }
                }
            }
        }
        }
        Node {
            objectName: "mesh-platform-motion"
            eulerRotation.z: -root.layoutAngle
            scale: Qt.vector3d(root.platformScale * root.shownScaleX, root.platformScale * root.shownScaleY,
                               root.platformScale * root.platformThickness)
            IconStyle3D {
                id: platform
                objectName: "mesh-platform"
                // Under a decoration it is not drawn; its mesh stays the
                // panel's shape for input.
                visible: !root.decorationShown
                pickable: true
                underGlow: root.platformGlowMesh !== null
                meshData: root.platformMesh
                materialData: root.platformMaterial
                surfaceTexture: root.themeTexture
                surfaceNormalTexture: root.themeNormalTexture
                reliefMaterial: root.pictureRelief ? platformRelief : root.flowingSurface ? platformFlow : null
                // A holographic panel (Panels > Hologram) is drawn as light.
                shellMaterial: root.panelHologramShown ? platformHologramMaterial : null
                emissionScale: root.emissionScale
                IconStyle3D {
                    objectName: "mesh-platform-glow"
                    visible: root.platformGlowMesh !== null && !root.panelHologramShown
                    meshData: root.platformGlowMesh
                    materialData: ({})
                    shellMaterial: platformGlowMaterial
                    // On a Möbius strip, whose icons ride in front of it and
                    // behind it, drawn before every see-through part of them,
                    // as if furthest away: its light, all round the band,
                    // never shows through an icon's soft edge in front of it.
                    depthBias: root.generatedPlatform !== null && root.generatedPlatform.shape === "mobius"
                        ? root.cameraDistance * 8 : 0
                }
                IconStyle3D {
                    objectName: "mesh-platform-glow-wash"
                    visible: root.platformWashMesh !== null && !root.panelHologramShown
                    meshData: root.platformWashMesh
                    materialData: ({})
                    shellMaterial: platformWashMaterial
                    opacity: root.panelGlowWash
                }
            }
            Repeater3D {
                id: panelPartNodes
                model: root.panelParts.length
                delegate: IconStyle3D {
                    required property int index
                    readonly property var meshBox: root.meshBox(meshData)
                    objectName: "mesh-panel-part-" + index
                    partDefinition: root.panelParts[index].definition
                    visible: partDefinition.mechanism === root.mechanism && !root.decorationShown
                    meshData: PlatformGeometry.foldMesh(root.panelParts[index].resources.mesh || null,
                        root.platformFold, root.baseTopHeight)
                    materialData: root.panelParts[index].resources.material || ({})
                    surfaceTexture: root.themeTexture
                    surfaceNormalTexture: root.themeNormalTexture
                    openAmount: root.partOpenAmount(partDefinition)
                    emissionScale: root.emissionScale
                }
            }
            PanelDecoration3D {
                id: platformDecoration
                objectName: "mesh-platform-decoration-node"
                decoration: root.decorationWanted ? root.decoration : ""
                choices: root.decorationChoices
                animated: root.decorationAnimated && !root.reducedMotion
                motionSpeed: root.decorationSpeed
                running: root.motionAllowed
                centreRuns: root.decorationRuns
                iconPoints: root.decorationIconPoints
                unit: root.platformScale
                // As wide as the decorations were designed round an icon.
                halfWidth: Math.max(6, root.iconCellSize / 1.7)
                fold: root.platformFold
                lightDirection: keyLight.forward.times(-1)
                cameraTransform: camera.sceneTransform
                // A round light under the pointer lights up.
                pointer: decorationPointer.hovered ? decorationPointer.point.position : null
                project: root.projectScenePoint
                // Twice as many pixels as the panel shows at rest, so its
                // grain holds as it turns toward the viewer or is scaled up.
                pictureScale: 2 * (root.Window.window ? root.Window.window.devicePixelRatio : 1)
                emissionScale: root.emissionScale
                // A holographic panel's decoration is drawn as light, as its
                // platform would be.
                hologram: root.panelHologramShown
                hologramOutline: platformHologramMaterial.outlineColour
                hologramInline: platformHologramMaterial.inlineColour
                hologramVolume: platformHologramMaterial.volumeColour
                hologramOwn: platformHologramMaterial.own
                hologramTime: platformHologramMaterial.time
                hologramFlicker: platformHologramMaterial.flicker
                hologramBrightness: platformHologramMaterial.brightness
            }
        }
        }
        Node {
            id: gizmo
            objectName: "mesh-gizmo"
            visible: root.editMode
            position: Qt.vector3d(root.shownPosition.x, root.shownPosition.y,
                root.shownPosition.z + root.floatOffset + root.platformTop * root.shownScale)
            // In the middle, while scaling: the cube that scales all ways.
            Model {
                objectName: "gizmo-scale-all-tip"
                visible: root.gizmoMode === "scale"
                source: "#Cube"
                pickable: true
                scale: Qt.vector3d(root.gizmoLength * 0.0024, root.gizmoLength * 0.0024, root.gizmoLength * 0.0024)
                materials: PrincipledMaterial {
                    lighting: PrincipledMaterial.NoLighting
                    baseColor: "#f4f8fb"
                }
            }
            // Arrows to move, along the scene's own directions; the scale
            // arms turn with the panel, the ways its own stretch goes.
            Node {
            eulerRotation.z: root.gizmoMode === "scale" ? -root.layoutAngle : 0
            Repeater3D {
                // Arrows to move, cube-tipped arms to scale.
                model: [
                    { axis: "x", color: "#ff5a5a", turn: Qt.vector3d(0, 0, -90) },
                    { axis: "y", color: "#7be37b", turn: Qt.vector3d(0, 0, 0) },
                    { axis: "z", color: "#5aa8ff", turn: Qt.vector3d(90, 0, 0) }
                ]
                delegate: Node {
                    id: arm
                    required property var modelData
                    readonly property string handleMode: root.gizmoMode === "scale" ? "scale" : "move"
                    visible: root.gizmoMode !== "rotate"
                    eulerRotation: modelData.turn
                    Model {
                        objectName: "gizmo-" + arm.handleMode + "-" + arm.modelData.axis + "-shaft"
                        source: "#Cylinder"
                        pickable: true
                        position: Qt.vector3d(0, root.gizmoLength / 2, 0)
                        scale: Qt.vector3d(root.gizmoLength * 0.0006, root.gizmoLength / 100,
                                           root.gizmoLength * 0.0006)
                        materials: PrincipledMaterial {
                            lighting: PrincipledMaterial.NoLighting
                            baseColor: arm.modelData.color
                        }
                    }
                    Model {
                        objectName: "gizmo-" + arm.handleMode + "-" + arm.modelData.axis + "-tip"
                        source: arm.handleMode === "scale" ? "#Cube" : "#Cone"
                        pickable: true
                        position: Qt.vector3d(0, root.gizmoLength, 0)
                        scale: Qt.vector3d(root.gizmoLength * 0.0016, root.gizmoLength * 0.0022,
                                           root.gizmoLength * 0.0016)
                        materials: PrincipledMaterial {
                            lighting: PrincipledMaterial.NoLighting
                            baseColor: arm.modelData.color
                        }
                    }
                }
            }
            }
            Repeater3D {
                // Rings to turn: pitch about X, yaw about Y, roll about the view.
                model: [
                    { axis: "x", color: "#ff5a5a", turn: Qt.vector3d(0, 90, 0) },
                    { axis: "y", color: "#7be37b", turn: Qt.vector3d(90, 0, 0) },
                    { axis: "view", color: "#f4f8fb", turn: Qt.vector3d(0, 0, 0) }
                ]
                delegate: Node {
                    id: ring
                    required property var modelData
                    visible: root.gizmoMode === "rotate"
                    readonly property real radius: root.gizmoLength * (modelData.axis === "view" ? 1.15 : 0.9)
                    // The view ring always faces the camera.
                    rotation: modelData.axis === "view" ? camera.sceneRotation : Qt.quaternion(1, 0, 0, 0)
                    eulerRotation: modelData.axis === "view" ? Qt.vector3d(0, 0, 0) : modelData.turn
                    Repeater3D {
                        model: 32
                        delegate: Model {
                            required property int index
                            readonly property real angle: index * Math.PI * 2 / 32
                            objectName: "gizmo-rotate-" + ring.modelData.axis + "-" + index
                            source: "#Cube"
                            pickable: true
                            position: Qt.vector3d(ring.radius * Math.cos(angle), ring.radius * Math.sin(angle), 0)
                            eulerRotation: Qt.vector3d(0, 0, angle * 180 / Math.PI + 90)
                            scale: Qt.vector3d(ring.radius * 2 * Math.PI / 32 / 100 * 0.9,
                                               root.gizmoLength * 0.0005, root.gizmoLength * 0.0005)
                            materials: PrincipledMaterial {
                                lighting: PrincipledMaterial.NoLighting
                                baseColor: ring.modelData.color
                            }
                        }
                    }
                }
            }
        }
        // Lights stay in world space; only the platform and its icons move.
        DirectionalLight {
            id: keyLight
            objectName: "mesh-key-light"
            eulerRotation: Qt.vector3d(-35, -35, 0)
            brightness: root.bounded("keyLightBrightness", 1, 0, 4)
            ambientColor: "#202630"
        }
        DirectionalLight {
            objectName: "mesh-fill-light"
            eulerRotation: Qt.vector3d(25, 140, 0)
            brightness: root.bounded("fillLightBrightness", 0.4, 0, 2)
            color: "#78c8ff"
        }
        // The key light's twin from behind: a panel turned past its edge shows
        // its underside lit as its face is. It only reaches surfaces facing
        // away from the front, so the face keeps its look.
        DirectionalLight {
            objectName: "mesh-back-light"
            eulerRotation: Qt.vector3d(-35, 145, 0)
            brightness: root.bounded("keyLightBrightness", 1, 0, 4) * 0.85
            ambientColor: "#000000"
        }
        // The shared geometry is retained even with no entries. Its readiness
        // participates in the whole scene's safe fallback decision.
        IconStyle3D {
            id: iconResource
            meshData: (root.sceneResources || ({})).iconMesh || null
            materialData: (root.sceneResources || ({})).material || ({})
            visible: false
        }
    }
}
