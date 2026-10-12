import QtQuick
import QtQuick.Window
import QtQuick3D
import "../GizmoMath.js" as GizmoMath
import "../LayoutEngine.js" as LayoutEngine
import "../PlatformGeometry.js" as PlatformGeometry

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
    property string quality: "medium"
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

    readonly property string effectiveQuality:
        ["low", "medium", "high"].includes(quality) ? quality : "medium"
    readonly property real resolutionScale: effectiveQuality === "low" ? 0.5
        : effectiveQuality === "high" ? 1 : 0.75
    readonly property int textureLimit: effectiveQuality === "low" ? 1024
        : effectiveQuality === "high" ? 2048 : 1536
    readonly property real boundedScale: Math.min(resolutionScale,
        textureLimit / Math.max(1, width, height))
    readonly property int targetWidth: Math.max(1, Math.ceil(width * boundedScale))
    readonly property int targetHeight: Math.max(1, Math.ceil(height * boundedScale))
    // A look whose theme ships no 3D mesh stands on a platform generated from
    // the panel's own layout (`sceneDefinition.generated`), in the look's own
    // colours: the palette sampled from its artwork when it is drawn from
    // artwork, or the colours its 2D surface is drawn with.
    property real layoutTrackRadius: 0
    // The icon cell's size in the same pixels as the track radius.
    property real iconCellSize: 52
    property string appearance: ""
    property string customColor: ""
    readonly property var materialNames: ["glass", "crystal", "neon", "minimal", "plasma", "lime",
        "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal"]
    readonly property string materialLook: materialNames.includes(String(sceneDefinition.materialLook))
        ? String(sceneDefinition.materialLook) : "theme"
    readonly property string materialColour: PlatformGeometry.parseColour(sceneDefinition.materialColor)
        ? String(sceneDefinition.materialColor) : ""
    // Theme on a generated platform is the flat look's own surface: its
    // translucency, metal, roughness, glow and texture, so that turning 3D on
    // keeps the design made flat (the owner's 2026-10-12 report); a look's
    // sampled palette only colours it.
    readonly property string effectiveMaterialLook: materialLook !== "theme" ? materialLook
        : generatedSpec && !textureRequired
            ? LayoutEngine.themeStyle(appearance, "", 48).material : "theme"
    readonly property string textureChoice: ["theme", "none"].concat(materialNames)
        .includes(String(sceneDefinition.materialTexture)) ? String(sceneDefinition.materialTexture) : "theme"
    readonly property string textureMaterial: textureChoice === "none" ? ""
        : textureChoice !== "theme" ? textureChoice
        : materialLook !== "theme" ? materialLook
        : generatedSpec && !textureRequired
            ? LayoutEngine.themeStyle(appearance, "", 48).material : ""
    readonly property url materialTextureSource: textureMaterial.length
        ? Qt.resolvedUrl("../materials/" + textureMaterial + ".png") : ""
    readonly property url materialNormalSource: textureMaterial.length
        ? Qt.resolvedUrl("../materials/" + textureMaterial + "-normal.png") : ""
    // The energy looks flow round the band while the scene is animated.
    property real flowOffset: 0
    NumberAnimation on flowOffset {
        running: root.materialAnimationActive
        loops: Animation.Infinite
        from: 0
        to: 1
        duration: 9000
    }
    // A texture that is being replaced keeps the scene ready: while the new
    // file loads, the platform keeps drawing (without that texture for a
    // moment) instead of the whole panel falling back to its flat renderer,
    // with the icons on the flat ring, and coming back once the file is in.
    // Only a scene that has drawn complete once is kept this way; the first
    // readiness still waits for every resource.
    property bool shownOnce: false
    readonly property bool materialTextureReady: !textureMaterial.length
        || materialImage.status === Image.Ready
        || (shownOnce && materialImage.status !== Image.Error)
    readonly property bool materialAnimationActive: effectiveMaterialLook !== "theme"
        && LayoutEngine.themeStyle(effectiveMaterialLook, "", 48).animated
        && motionAllowed && visible && !sceneConcealed && !reducedMotion
    property real materialPulse: 0
    SequentialAnimation on materialPulse {
        running: root.materialAnimationActive
        loops: Animation.Infinite
        NumberAnimation { to: 1; duration: 2400; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0; duration: 2400; easing.type: Easing.InOutSine }
    }
    readonly property var generatedSpec: (sceneDefinition || ({})).generated || null
    readonly property var generatedShape: generatedSpec
        ? PlatformGeometry.shapeForLayout(generatedSpec.layout, generatedSpec.polygonSides) : null
    readonly property var generatedPalette: materialColour.length || materialLook !== "theme"
        ? PlatformGeometry.paletteFromStyle(LayoutEngine.themeStyle(
            materialLook === "theme" ? appearance : materialLook, materialColour, 48))
        : generatedSpec && generatedSpec.palette ? generatedSpec.palette
        : PlatformGeometry.paletteFromStyle(LayoutEngine.themeStyle(appearance, customColor, 48))
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
        const footprint = 0.18 * iconCellSize * pedestalSize / radius * PlatformGeometry.trackRadius * 1.1
        // A polygon's mitred corners hold a stand only on a wider band: the
        // old default for a triangle or a square, a little less for a
        // pentagon; a ring or an ellipse is as thin as its line allows.
        const sides = generatedShape && generatedShape.kind === "polygon" ? Number(generatedShape.sides || 6) : 0
        const corners = sides > 0 && sides <= 4 ? 0.11 : sides === 5 ? 0.09 : 0
        return Math.max(PlatformGeometry.minimumBand, footprint, corners, Math.min(PlatformGeometry.maximumBand,
            (Number(style.lineWidth) || 12) / 2 / radius))
    }
    readonly property real resolvedBand: bandAuto ? flatLineBand : bandSetting
    readonly property real platformThickness: bounded("thickness", 1, 0.1, 4)
        * (bandAuto ? Math.max(0.4, Math.min(1, resolvedBand / 0.11)) : 1)
    readonly property string generatedKey: generatedShape ? JSON.stringify({
        shape: generatedShape, band: resolvedBand,
        bend: bounded("bend", 0, -1, 1), palette: generatedPalette }) : ""
    readonly property var generatedPlatform: generatedKey.length > 0
        ? PlatformGeometry.platform(JSON.parse(generatedKey)) : null
    readonly property var basePlatformMesh: generatedSpec ? generatedPlatform
        : (sceneResources || ({})).mesh || null
    readonly property real baseTopHeight: generatedSpec ? PlatformGeometry.topHeight
        : Math.max(0, ...((basePlatformMesh || {}).positions || []).map(p => Number(p[2])))
    readonly property real platformFold: bounded("fold", 0, -1, 1)
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
            if (["neon", "plasma", "lime", "futuristic"].includes(look)) {
                material.emissiveColor = materialColour || LayoutEngine.themeStyle(look, "", 48).stroke
                material.emissiveStrength = 0.35 + root.materialPulse * 0.45
                material.emissiveThroughTexture = true
            }
        }
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
    readonly property quaternion iconFacing: entryFrame.sceneRotation.inverted()
        .times(camera.sceneRotation)
    // The stand under every icon (the Pedestals page): "auto" is the
    // platform's own column in its material; a chosen shape floats between
    // the platform and the icon in its own material, size and motion.
    readonly property var materialLooks: ["glass", "crystal", "neon", "minimal", "plasma", "lime",
        "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal"]
    readonly property string pedestalShape: String((sceneDefinition || ({})).pedestalShape || "auto").toLowerCase()
    readonly property string pedestalKind: pedestalShape === "auto" ? "column" : pedestalShape
    readonly property bool pedestalDrawn: pedestalKind !== "none"
    readonly property bool pedestalFloats: pedestalDrawn && pedestalKind !== "column"
    readonly property real pedestalSize: bounded("pedestalSize", 1, 0.5, 2)
    readonly property real pedestalLift: bounded("pedestalLift", 0.3, 0, 1)
    readonly property string pedestalMaterialLook: String((sceneDefinition || ({})).pedestalMaterial || "panel").toLowerCase()
    readonly property string pedestalColour: String((sceneDefinition || ({})).pedestalColor || "")
    readonly property string pedestalAnimation: String((sceneDefinition || ({})).pedestalAnimation || "none").toLowerCase()
    readonly property real pedestalAnimationSpeed: bounded("pedestalAnimationSpeed", 1, 0.25, 3)
    readonly property bool pedestalOwnMaterial: pedestalDrawn && pedestalMaterialLook !== "panel"
        && materialLooks.includes(pedestalMaterialLook)
    readonly property var pedestalMesh: pedestalDrawn ? PlatformGeometry.pedestalShape(pedestalKind, 24) : PlatformGeometry.pedestal(24)
    readonly property real pedestalShapeHeight: PlatformGeometry.pedestalShapeHeight(pedestalKind)
    readonly property int pedestalTriangles: pedestalMesh.indexes.length / 3
    // Each shape's width across, in icon sizes, at pedestal size 1.
    readonly property real pedestalShapeScale: ({ column: 0.18, plate: 0.46, disc: 0.42, ring: 0.42, saucer: 0.4 })[pedestalKind] || 0.18
    readonly property var pedestalMaterialData: pedestalOwnMaterial
        ? materialForLook(pedestalMaterialLook, pedestalColour) : platformMaterial
    readonly property url pedestalTextureSource: pedestalOwnMaterial
        ? Qt.resolvedUrl("../materials/" + pedestalMaterialLook + ".png") : ""
    readonly property url pedestalNormalSource: pedestalOwnMaterial
        ? Qt.resolvedUrl("../materials/" + pedestalMaterialLook + "-normal.png") : ""
    readonly property Texture pedestalTexture: pedestalOwnMaterial
        ? (pedestalImage.status === Image.Ready ? selectedPedestalTexture : null) : themeTexture
    readonly property Texture pedestalNormalTexture: pedestalOwnMaterial
        ? (pedestalNormalImage.status === Image.Ready ? selectedPedestalNormal : null) : themeNormalTexture
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
        duration: Math.round(4000 / Math.max(0.25, root.pedestalAnimationSpeed))
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
        if (["neon", "plasma", "lime", "futuristic"].includes(look)) {
            material.emissiveColor = colour && colour.length ? colour : LayoutEngine.themeStyle(look, "", 48).stroke
            material.emissiveStrength = 0.35 + root.materialPulse * 0.45
            material.emissiveThroughTexture = true
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
            if (bounded("iconElevation", 0.3, 0, 2) > 0)
                count += pedestalTriangles + (generatedSpec ? 0 : iconResource.triangleCount)
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
        return PlatformGeometry.tile({shape: visual.resolvedIconStyle.iconShape,
            thickness: 2 * thickness / logicalSize,
            bevel: 2 * Math.max(0, Math.min(12, root.number(values, "iconTileBevel", 0))) / logicalSize,
            diameter: diameter})
    })
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
            emissiveStrength: ["neon", "plasma", "lime", "futuristic"].includes(look) ? 0.2 : 0,
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
    readonly property real platformScale: Math.min(width / platformReachX, height / platformReachY) * 0.40
        / (1 + Math.abs(platformFold) * platformThickness * 0.4)
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
        scale: "scene3DScale"
    })
    function transformValue(key, fallback, minimum, maximum) {
        const override = dragOverride && dragOverride[key] !== undefined ? Number(dragOverride[key]) : NaN
        return Number.isFinite(override) ? Math.max(minimum, Math.min(maximum, override))
            : bounded(key, fallback, minimum, maximum)
    }
    function wrapDegrees(value) { return ((value + 180) % 360 + 360) % 360 - 180 }
    function savedTransform() {
        return {
            cameraPitch: bounded("cameraPitch", 25, -60, 60), cameraYaw: bounded("cameraYaw", 0, -180, 180),
            roll: bounded("roll", 0, -180, 180), positionX: bounded("positionX", 0, -1, 1),
            positionY: bounded("positionY", 0, -1, 1), positionZ: bounded("positionZ", 0, -1, 1),
            scale: bounded("scale", 1, 0.5, 1.25)
        }
    }
    function gizmoHandleAt(x, y) {
        const hits = view.pickAll(x, y)
        for (let index = 0; index < hits.length; ++index) {
            const name = hits[index].objectHit ? String(hits[index].objectHit.objectName) : ""
            const match = /^gizmo-(move|rotate|scale)-(x|y|z|view)/.exec(name)
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
                next.cameraPitch = GizmoMath.clamp(pitch, -60, 60)
            } else {
                let angle = start.axis === "y"
                    ? start.values.cameraYaw + delta.x * 0.25
                    : start.values.roll + GizmoMath.sweptDegrees(start.centre, start.pointer, pointer)
                if (settings.snap) angle = GizmoMath.snapped(angle, 15)
                next[start.axis === "y" ? "cameraYaw" : "roll"] = wrapDegrees(angle)
            }
        } else {
            let scale = start.values.scale * GizmoMath.scaleRatio(start.centre, start.pointer, pointer)
            if (settings.snap) scale = GizmoMath.snapped(scale, 0.05)
            next.scale = GizmoMath.clamp(scale, 0.5, 1.25)
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
    readonly property real cameraDistance: Math.max(1, height)
        / (2 * Math.tan(bounded("fieldOfView", 40, 20, 70) * Math.PI / 360))
    // The platform and its icons reach a little past the platform's radius.
    readonly property real sceneExtentX: platformScale * platformReachX * 1.05
    readonly property real sceneExtentY: platformScale * platformReachY * 1.05
    readonly property real targetScale: transformValue("scale", 1, 0.5, 1.25)
    readonly property real fitLimit: Math.min(width / 2 / Math.max(1, sceneExtentX),
                                              height / 2 / Math.max(1, sceneExtentY))
    // Nearer is larger: depth stops where the platform would outgrow the panel.
    readonly property real targetDepth: Math.min(transformValue("positionZ", 0, -1, 1) * platformScale * 0.5,
        cameraDistance * Math.max(0, 1 - targetScale / Math.max(targetScale, fitLimit)))
    readonly property real apparentScale: targetScale * cameraDistance
        / Math.max(1, cameraDistance - targetDepth)
    readonly property real roomX: Math.max(0, width / 2 - sceneExtentX * apparentScale)
    readonly property real roomY: Math.max(0, height / 2 - sceneExtentY * apparentScale)
    readonly property vector3d targetPosition: Qt.vector3d(
        transformValue("positionX", 0, -1, 1) * roomX * (cameraDistance - targetDepth) / cameraDistance,
        transformValue("positionY", 0, -1, 1) * roomY * (cameraDistance - targetDepth) / cameraDistance,
        targetDepth)
    readonly property real targetPitch: transformValue("cameraPitch", 25, -60, 60)
    readonly property real targetYaw: transformValue("cameraYaw", 0, -180, 180)
    readonly property real targetRoll: transformValue("roll", 0, -180, 180)
    // What is drawn. It eases toward the targets when transitions are on and
    // jumps to them otherwise. One frame-driven easing instead of a Behavior
    // per value: Qt creates a Behavior's animation on its first use, which
    // would make the scene's objects depend on what it has been through.
    property real shownPitch: 0
    property real shownYaw: 0
    property real shownRoll: 0
    property vector3d shownPosition: Qt.vector3d(0, 0, 0)
    property real shownScale: 1
    // Exact: the easing lands each value on its target, then stops.
    readonly property bool transformSettled: shownPitch === targetPitch
        && shownYaw === targetYaw && shownRoll === targetRoll && shownScale === targetScale
        && shownPosition.x === targetPosition.x && shownPosition.y === targetPosition.y
        && shownPosition.z === targetPosition.z
    function angleStep(from, to) { return ((to - from) % 360 + 540) % 360 - 180 }
    function settleTransform() {
        shownPitch = targetPitch
        shownYaw = targetYaw
        shownRoll = targetRoll
        shownPosition = targetPosition
        shownScale = targetScale
    }
    function followTransform() { if (!transitionsEnabled) settleTransform() }
    function stepTransform(seconds) {
        // About 95 percent of the way in a quarter of a second, the short way
        // round for angles; within a hair of the target it lands on it.
        const share = 1 - Math.exp(-12 * Math.max(0, seconds))
        const toward = function(from, to, step, epsilon) {
            return Math.abs(step) < epsilon ? to : from + step * share
        }
        shownPitch = toward(shownPitch, targetPitch, targetPitch - shownPitch, 1e-3)
        shownYaw = toward(shownYaw, targetYaw, angleStep(shownYaw, targetYaw), 1e-3)
        shownRoll = toward(shownRoll, targetRoll, angleStep(shownRoll, targetRoll), 1e-3)
        const offset = targetPosition.minus(shownPosition)
        shownPosition = offset.length() < 1e-3 ? targetPosition
            : shownPosition.plus(offset.times(share))
        shownScale = toward(shownScale, targetScale, targetScale - shownScale, 1e-4)
    }
    onTargetPitchChanged: followTransform()
    onTargetYawChanged: followTransform()
    onTargetRollChanged: followTransform()
    onTargetPositionChanged: followTransform()
    onTargetScaleChanged: followTransform()
    onTransitionsEnabledChanged: followTransform()
    Component.onCompleted: settleTransform()
    FrameAnimation {
        running: root.transitionsEnabled && !root.transformSettled
        onTriggered: root.stepTransform(frameTime)
    }
    // A slow rise and fall, off unless chosen, and never under reduced motion.
    readonly property bool floatRunning: (sceneDefinition || ({})).float === true
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

    function entryPointVisible(index, point) {
        const key = hitKey((editMode ? "E" : "e") + index, point)
        const known = hitMemoGet(key)
        if (known !== undefined) return known
        return hitMemoSet(key, entryPointVisibleUncached(index, point))
    }
    function entryPointVisibleUncached(index, point) {
        if (!motionAllowed || !worldEntries.objectAt(index) || !worldEntries.objectAt(index).visible)
            return false
        const hits = view.pickAll(point.x, point.y)
        let glyph = null
        for (let i = 0; i < hits.length; ++i)
            if (hits[i].objectHit && String(hits[i].objectHit.objectName) === "mesh-glyph-" + index) {
                glyph = hits[i]; break
            }
        if (!glyph) return false
        const origin = root.unprojectViewPoint(Qt.vector3d(point.x, point.y, 0))
        const distance = glyph.scenePosition.minus(origin).length()
        if (meshHitDistance(platform, platformMesh, point, platformTriangles) < distance - 0.01) return false
        for (let i = 0; i < panelPartNodes.count; ++i) {
            const part = panelPartNodes.objectAt(i)
            if (part && part.visible && meshHitDistance(part, part.meshData, point) < distance - 0.01)
                return false
        }
        // Overlapping entries are picked in the same near-to-far order as drawn.
        for (let i = 0; i < hits.length; ++i) {
            const name = hits[i].objectHit ? String(hits[i].objectHit.objectName) : ""
            if (/^mesh-glyph-\d+$/.test(name)) return name === "mesh-glyph-" + index
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
        const hits = view.pickAll(point.x, point.y)
        for (let i = 0; i < hits.length; ++i) {
            const name = hits[i].objectHit ? String(hits[i].objectHit.objectName) : ""
            const entry = /^mesh-glyph-(\d+)$/.exec(name)
            if (entry && entryPointVisible(Number(entry[1]), point)) return true
            if (editMode && name.startsWith("gizmo-")) return true
        }
        return false
    }
    // "Along the dock": the platform's own icon track, projected like the
    // icons, then moved `outward` screen pixels out of the drawn dock, as
    // `count` samples spanning `span` radians around the folder. `scale` is
    // the perspective size there relative to the folder's own.
    // View3D's mapping overloads recalculate its shared render-camera
    // projection. Camera's normalized public methods read the projection
    // submitted by the renderer instead, without writing from the GUI thread.
    function projectScenePoint(point) {
        const normalized = camera.mapToViewport(point)
        return Qt.vector3d(normalized.x * view.width, normalized.y * view.height, normalized.z)
    }
    function unprojectViewPoint(point) {
        return camera.mapFromViewport(Qt.vector3d(point.x / Math.max(1, view.width),
            point.y / Math.max(1, view.height), point.z))
    }
    function folderTrackSamples(entryIndex, outward, span, count) {
        const node = worldEntries.objectAt(entryIndex)
        if (!rendererReady || !node || !node.inputAnchor || count < 2) return []
        const anchor = node.inputAnchor
        // An icon stands toward the camera's up from where it stands on the
        // track, by the same offset for every icon: the icons' centres lie on
        // the track moved by that offset.
        const foot = sceneContent.mapPositionFromScene(node.scenePosition)
        const local = sceneContent.mapPositionFromScene(anchor.scenePosition)
        const lift = local.minus(foot)
        const radius = Math.hypot(foot.x, foot.y)
        if (radius <= 0) return []
        const angle = Math.atan2(foot.y, foot.x)
        const depth = root.projectScenePoint(anchor.scenePosition).z
        // The icons' track as drawn: `count` projected points spanning `span`
        // around the folder, and one more past each end for the directions.
        const step = span / (count - 1)
        const drawn = []
        for (let index = -1; index <= count; ++index) {
            const turn = angle - span / 2 + step * index
            drawn.push(root.projectScenePoint(sceneContent.mapPositionToScene(
                Qt.vector3d(radius * Math.cos(turn), radius * Math.sin(turn), foot.z).plus(lift))))
        }
        // Each sample stands `outward` screen pixels outside the drawn track,
        // across the track's own direction there and away from the dock's
        // centre. A step out of the platform in its own units is a different
        // distance on screen at every angle - a few pixels on the far side of
        // a tilted platform - so a curve measured in those units at the folder
        // would sweep far out of the dock everywhere else.
        const centre = root.projectedCentre
        const usable = point => Number.isFinite(point.x) && Number.isFinite(point.y) && point.z > 0
        const result = []
        for (let index = 1; index <= count; ++index) {
            const point = drawn[index], before = drawn[index - 1], after = drawn[index + 1]
            if (!usable(point) || !usable(before) || !usable(after)) continue
            let acrossX = -(after.y - before.y), acrossY = after.x - before.x
            let length = Math.hypot(acrossX, acrossY)
            if (!(length > 1e-6)) {
                acrossX = point.x - centre.x
                acrossY = point.y - centre.y
                length = Math.hypot(acrossX, acrossY)
                if (!(length > 1e-6)) continue
            }
            acrossX /= length
            acrossY /= length
            if (acrossX * (point.x - centre.x) + acrossY * (point.y - centre.y) < 0) {
                acrossX = -acrossX
                acrossY = -acrossY
            }
            result.push({ x: point.x + acrossX * outward, y: point.y + acrossY * outward,
                          scale: depth / point.z })
        }
        return result
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
        // The rim along this direction, in the scene's units: an ellipse's
        // reach for a generated platform, the farthest vertex near this
        // angle for a theme's mesh (the mesh turns with the layout).
        let rim = radius
        if (generatedPlatform) {
            const rx = Math.max(0.01, platformReachX), ry = Math.max(0.01, platformReachY)
            rim = platformScale / Math.hypot(ux / rx, uy / ry)
        } else if (platformMesh && platformMesh.positions) {
            const turn = Math.atan2(uy, ux) + layoutAngle * Math.PI / 180
            let farthest = 0
            for (const vertex of platformMesh.positions) {
                const x = Number(vertex[0]), y = Number(vertex[1])
                const along = x * Math.cos(turn) + y * Math.sin(turn)
                const across = -x * Math.sin(turn) + y * Math.cos(turn)
                if (along > 0 && Math.abs(across) <= along * 0.27) farthest = Math.max(farthest, along)
            }
            rim = farthest > 0 ? farthest * platformScale : radius
        }
        const edge = Math.max(radius, rim)
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
            result.push({x: left, y: top, width: right-left, height: bottom-top,
                centerX: center.x, centerY: center.y, depth: center.z})
        }
        if (JSON.stringify(result) !== JSON.stringify(projectedEntryGeometry)) projectedEntryGeometry = result
        // The dock's centre as drawn: the platform's middle at its top. A
        // folder's contents open away from it, not from the icons' heads.
        const centre = root.projectScenePoint(sceneContent.mapPositionToScene(Qt.vector3d(0, 0, platformTop)))
        if (Number.isFinite(centre.x) && Number.isFinite(centre.y)
                && (centre.x !== projectedCentre.x || centre.y !== projectedCentre.y))
            projectedCentre = Qt.point(centre.x, centre.y)
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
            if (root.editMode)
                root.updateEndOnAxes()
        }
    }
    Component.onDestruction: frameUpdates.stop()
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

    Image {
        id: materialImage
        visible: false
        source: root.materialTextureSource
        sourceSize: Qt.size(512, 512)
        asynchronous: true
        cache: true
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
        anchors.fill: parent
        z: 2
        // While editing, the whole panel belongs to the handles: a press that
        // misses them does nothing, so no application starts by accident.
        enabled: root.editMode && root.rendererReady
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        onPressed: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                root.cancelGizmoDrag()
                return
            }
            // A handle first; otherwise a press on the platform or its icons
            // takes the body, to tilt and turn it.
            const handle = root.gizmoHandleAt(mouse.x, mouse.y)
            if (handle)
                root.beginGizmoDrag(handle, mouse.x, mouse.y)
            else if (root.containsInputPoint(Qt.point(mouse.x, mouse.y)))
                root.beginGizmoDrag({ mode: "orbit", axis: "body" }, mouse.x, mouse.y)
        }
        onPositionChanged: function(mouse) {
            if (root.dragStart)
                root.updateGizmoDrag(mouse.x, mouse.y, mouse.modifiers)
        }
        onReleased: function(mouse) {
            if (mouse.button !== Qt.LeftButton || !root.dragStart)
                return
            // The keys held when the button is let go decide the result.
            root.updateGizmoDrag(mouse.x, mouse.y, mouse.modifiers)
            root.finishGizmoDrag()
        }
        onCanceled: root.cancelGizmoDrag()
    }
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.dragStart !== null
        onActivated: root.cancelGizmoDrag()
    }

    View3D {
        id: view
        anchors.fill: parent
        visible: root.rendererReady && !root.sceneConcealed
        opacity: root.panelOpacity
        explicitTextureWidth: root.targetWidth
        explicitTextureHeight: root.targetHeight
        camera: camera
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: root.effectiveQuality === "low"
                ? SceneEnvironment.NoAA : SceneEnvironment.MSAA
            antialiasingQuality: root.effectiveQuality === "high"
                ? SceneEnvironment.High : SceneEnvironment.Medium
        }
        Texture {
            id: selectedMaterialTexture
            sourceItem: root.Window.window && materialImage.Window.window === root.Window.window
                ? materialImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
            // A generated platform wraps the texture round its band, as
            // many times along it as keep the texture's scale the same both
            // ways; a theme's mesh maps its texture as the theme authored it.
            scaleU: root.generatedPlatform ? Number(root.generatedPlatform.textureRepeats || 1) : 1
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
            positionU: root.flowOffset
        }
        Texture {
            id: selectedPedestalTexture
            sourceItem: root.Window.window && pedestalImage.Window.window === root.Window.window
                ? pedestalImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
        }
        Texture {
            id: selectedPedestalNormal
            sourceItem: root.Window.window && pedestalNormalImage.Window.window === root.Window.window
                ? pedestalNormalImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
        }
        Texture {
            id: selectedMaterialNormal
            sourceItem: root.Window.window && materialNormalImage.Window.window === root.Window.window
                ? materialNormalImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
            scaleU: selectedMaterialTexture.scaleU
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
            positionU: root.flowOffset
        }
        Texture {
            id: surfaceTexture
            sourceItem: root.Window.window && textureImage.Window.window === root.Window.window
                ? textureImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
        }
        Node {
            objectName: "mesh-camera-rig"
            eulerRotation.x: root.shownPitch
            eulerRotation.y: root.shownYaw
            PerspectiveCamera {
                id: camera
                fieldOfView: root.bounded("fieldOfView", 40, 20, 70)
                z: root.cameraDistance
                eulerRotation.z: root.shownRoll
                clipNear: 1
                clipFar: Math.max(100, z * 5)
                // Camera-local coordinates preserve the logical pixel centers
                // without querying viewport matrices during scene construction.

            }
        }
        Node {
            id: sceneContent
            objectName: "mesh-scene-content"
            position: Qt.vector3d(root.shownPosition.x, root.shownPosition.y,
                                  root.shownPosition.z + root.floatOffset)
            scale: Qt.vector3d(root.shownScale * root.radialCollapseScale,
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
                // The pedestal stands on the platform at the icon's place.
                // Its height under the icon: a column's own height (Auto
                // keeps the 3D page's Pedestal height), or a shape's own
                // thickness plus the gap the icon floats above it, Lift
                // icon-sizes. The small gap keeps an icon without one off
                // the platform's top.
                readonly property real pedestalScale: size * root.pedestalShapeScale * root.pedestalSize
                readonly property real pedestalGap: root.pedestalFloats ? size * root.pedestalLift * 0.5 : 0
                readonly property real pedestalHeight: !root.pedestalDrawn ? 0
                    : root.pedestalFloats ? root.pedestalShapeHeight * pedestalScale + pedestalGap
                    : size * (root.pedestalShape === "auto" ? elevation : root.pedestalLift)
                // This entry's motion: its own phase along the scene's.
                readonly property real pedestalBeat: root.pedestalMotionActive
                    ? Math.sin((root.pedestalPhase + index * 0.17) * 2 * Math.PI) : 0
                readonly property real pedestalBob: root.pedestalAnimation === "bob" ? pedestalBeat * size * 0.06 : 0
                readonly property real pedestalPulse: root.pedestalAnimation === "pulse" ? 1 + pedestalBeat * 0.06 : 1
                readonly property real pedestalSpin: root.pedestalAnimation === "spin" && root.pedestalMotionActive
                    ? root.pedestalPhase * 360 : 0
                readonly property real baseLift: size * 0.02
                readonly property real tileLift: visual ? Math.max(-24, Math.min(96,
                    root.number(visual.tileSettings, "iconTileElevation", 0)))
                    * size / Math.max(1, Number(visual.logicalSize || 52)) : 0
                readonly property var tileMesh: root.tileMeshes[index] || null
                readonly property var tileMaterial: root.tileMaterial(visual)
                readonly property bool tileShown: visual !== null && visual.tileRenderingEnabled
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
                    ? PlatformGeometry.standPoint(reachX, reachY, root.layoutTrackRadius,
                                                  root.generatedPlatform.track)
                    : [(reach > 0.001 ? reachX / reach : 0) * root.entryTrackRadius / root.platformScale,
                       (reach > 0.001 ? reachY / reach : 1) * root.entryTrackRadius / root.platformScale]
                readonly property var standPoint: PlatformGeometry.foldPoint(
                    [baseStandPoint[0], baseStandPoint[1], root.baseTopHeight],
                    root.platformFold, root.baseTopHeight).map(v => v * root.platformScale)
                property alias glyphModel: glyphModel
                property alias inputAnchor: inputAnchor
                readonly property real glow: Math.max(root.number(iconMotion, "glow", 0),
                    root.number(glyphMotion, "glow", 0), root.number(tileMotion, "glow", 0))
                // An entry off its path is not drawn, and one leaving or coming
                // back onto an open path fades, exactly as its 2D delegate does
                // (ADREP-TASK-002).
                readonly property real trackVisibility: rect.visibility === undefined
                    ? (rect.onTrack === false ? 0 : 1) : Math.max(0, Math.min(1, Number(rect.visibility)))
                visible: trackVisibility > 0
                // The entry's origin is where its icon stands: on the platform's
                // track, on top of its pedestal. Motion offsets move it along.
                // The entry's origin travels with the layout; the icon's own
                // motion moves the icon alone (below) and the pedestal's
                // motion moves the pedestal alone, so neither drags the other.
                position: Qt.vector3d(standPoint[0], standPoint[1],
                    standPoint[2] * root.platformThickness + pedestalHeight + baseLift + tileLift)
                opacity: root.number(iconMotion, "opacity", 1) * trackVisibility

                // The pedestal: a column from the platform's top up to the
                // icon, or a shape floating under it, in the platform's
                // material or its own.
                IconStyle3D {
                    objectName: "mesh-pedestal-" + entryNode.index
                    meshData: root.pedestalMesh
                    materialData: root.pedestalMaterialData
                    surfaceTexture: root.pedestalTexture
                    surfaceNormalTexture: root.pedestalNormalTexture
                    visible: root.pedestalDrawn && (root.pedestalFloats
                        || (entryNode.tileShown && (entryNode.pedestalHeight > 0 || entryNode.tileLift > 0)))
                    // Its foot on the platform: the whole height under the
                    // icon, so a shape's top is the gap below the icon; its
                    // own bob moves it, not the icon.
                    z: -(entryNode.pedestalHeight + entryNode.baseLift + entryNode.tileLift) + entryNode.pedestalBob
                    scale: root.pedestalFloats
                        ? Qt.vector3d(entryNode.pedestalScale * entryNode.pedestalPulse,
                                      entryNode.pedestalScale * entryNode.pedestalPulse, entryNode.pedestalScale)
                        : Qt.vector3d(entryNode.pedestalScale, entryNode.pedestalScale,
                                      Math.max(0, entryNode.pedestalHeight + entryNode.baseLift + entryNode.tileLift))
                    eulerRotation.z: entryNode.pedestalSpin
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
                    z: -(entryNode.pedestalHeight + entryNode.baseLift + entryNode.tileLift)
                    scale: Qt.vector3d(entryNode.size * 0.3, entryNode.size * 0.3, entryNode.size * 0.15)
                    emissionScale: root.emissionScale * (1 + entryNode.glow)
                }
                // A theme's own parts for each entry (such as a ring that sinks
                // as the panel closes), at the pedestal's foot.
                Node {
                    objectName: "mesh-tile-parts-" + entryNode.index
                    visible: !entryNode.visual || (entryNode.visual.tileRenderingEnabled
                        && !entryNode.visual.customTileActive)
                    z: -(entryNode.pedestalHeight + entryNode.baseLift + entryNode.tileLift)
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
                // The icon faces the viewer at every pitch, yaw and roll, so it
                // keeps the application's own glyph readable instead of lying
                // flat on the platform; it stands on its pedestal.
                Node {
                    objectName: "mesh-icon-" + entryNode.index
                    rotation: root.iconFacing
                    // The icon's own motion offsets, on the icon alone.
                    position: Qt.vector3d(root.number(entryNode.iconMotion, "x", 0),
                                          -root.number(entryNode.iconMotion, "y", 0), 0)
                    // The icon's resting square, untouched by hover and motion:
                    // its projection is the entry's input rectangle, so the
                    // rectangle does not breathe under the pointer.
                    Node {
                        id: inputAnchor
                        objectName: "mesh-input-anchor-" + entryNode.index
                        y: entryNode.size / 2
                    }
                    Node {
                        objectName: "mesh-icon-body-" + entryNode.index
                        // Hover magnification and icon motion grow the icon about
                        // its foot, so it stays standing on the pedestal.
                        eulerRotation: Qt.vector3d(0, root.number(entryNode.iconMotion, "rotateY", 0),
                            -root.number(entryNode.iconMotion, "rotateZ", 0) - Number(entryNode.rect.rotation || 0))
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
                            cullMode: Material.NoCulling
                        }
                        IconStyle3D {
                            objectName: "mesh-tile-solid-" + entryNode.index
                            meshData: entryNode.tileMesh
                            materialData: entryNode.tileMaterial
                            faceMaterial: tileFaceMaterial
                            visible: entryNode.tileShown && entryNode.tileMesh !== null && root.collapseProgress < 1
                            position: Qt.vector3d(root.number(entryNode.tileMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.tileMotion, "y", 0), -entryNode.size * 0.001)
                            eulerRotation: Qt.vector3d(0, root.number(entryNode.tileMotion, "rotateY", 0),
                                -root.number(entryNode.tileMotion, "rotateZ", 0))
                            scale: Qt.vector3d(entryNode.size / 2 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleX", 1),
                                entryNode.size / 2 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleY", 1), entryNode.size / 2)
                            opacity: root.number(entryNode.tileMotion, "opacity", 1) * (1 - root.collapseProgress)
                        }
                        Model {
                            objectName: "mesh-tile-" + entryNode.index
                            source: "#Rectangle"
                            pickable: false
                            visible: entryNode.tileShown && entryNode.tileMesh === null && root.collapseProgress < 1
                            position: Qt.vector3d(root.number(entryNode.tileMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.tileMotion, "y", 0), 0)
                            eulerRotation: Qt.vector3d(0, root.number(entryNode.tileMotion, "rotateY", 0),
                                -root.number(entryNode.tileMotion, "rotateZ", 0))
                            scale: Qt.vector3d(entryNode.size / 100 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleX", 1),
                                entryNode.size / 100 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleY", 1), 1)
                            opacity: root.number(entryNode.tileMotion, "opacity", 1) * (1 - root.collapseProgress)
                            materials: tileFaceMaterial
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
                            eulerRotation: Qt.vector3d(0, root.number(entryNode.glyphMotion, "rotateY", 0),
                                -root.number(entryNode.glyphMotion, "rotateZ", 0))
                            scale: Qt.vector3d(entryNode.size / 100 * root.number(entryNode.glyphMotion, "scale", 1)
                                    * root.number(entryNode.glyphMotion, "scaleX", 1),
                                entryNode.size / 100 * root.number(entryNode.glyphMotion, "scale", 1)
                                    * root.number(entryNode.glyphMotion, "scaleY", 1), 1)
                            opacity: root.number(entryNode.glyphMotion, "opacity", 1) * (1 - root.collapseProgress)
                            materials: PrincipledMaterial {
                                lighting: PrincipledMaterial.NoLighting
                                alphaMode: PrincipledMaterial.Blend
                                baseColorMap: glyphTexture
                                cullMode: Material.NoCulling
                            }
                        }
                    }
                }
            }
        }
        }
        Node {
            objectName: "mesh-platform-motion"
            eulerRotation.z: -root.layoutAngle
            scale: Qt.vector3d(root.platformScale, root.platformScale,
                               root.platformScale * root.platformThickness)
            IconStyle3D {
                id: platform
                objectName: "mesh-platform"
                pickable: true
                meshData: root.platformMesh
                materialData: root.platformMaterial
                surfaceTexture: root.themeTexture
                surfaceNormalTexture: root.themeNormalTexture
                emissionScale: root.emissionScale
            }
            Repeater3D {
                id: panelPartNodes
                model: root.panelParts.length
                delegate: IconStyle3D {
                    required property int index
                    objectName: "mesh-panel-part-" + index
                    partDefinition: root.panelParts[index].definition
                    visible: partDefinition.mechanism === root.mechanism
                    meshData: PlatformGeometry.foldMesh(root.panelParts[index].resources.mesh || null,
                        root.platformFold, root.baseTopHeight)
                    materialData: root.panelParts[index].resources.material || ({})
                    surfaceTexture: root.themeTexture
                    surfaceNormalTexture: root.themeNormalTexture
                    openAmount: root.partOpenAmount(partDefinition)
                    emissionScale: root.emissionScale
                }
            }
        }
        }
        Node {
            id: gizmo
            objectName: "mesh-gizmo"
            visible: root.editMode
            position: Qt.vector3d(root.shownPosition.x, root.shownPosition.y,
                root.shownPosition.z + root.floatOffset + root.platformTop * root.shownScale)
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
