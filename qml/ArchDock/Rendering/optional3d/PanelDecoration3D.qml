import QtQuick
import QtQuick3D
import ArchDock.Rendering 1.0
import ArchDock.Decorations 1.0
import ArchDock.Decorations3D 1.0
import "../PlatformGeometry.js" as PlatformGeometry

// A panel decoration on a 3D panel, in the generated platform's place: the
// decoration worked out along the platform's centre line as on a flat
// panel, made a solid (DecorationBaker's relief: its upper surface as high
// as the decoration stands and an underside, curving round to meet each
// other halfway all round its outline), and lit by the scene as it turns
// (DecorationSurface). Its lights pulse, flicker, twinkle, blink or crackle
// as on a flat panel; its motion is not drawn. On a holographic panel
// (Panels > Hologram) it is drawn as light instead, its own colours made
// light as the platform's would be (HologramSurface).
// Placed in the platform's own frame (its units, y up, z out of its top),
// its ground on the platform's top.
Node {
    id: root

    // The decoration's id and the viewer's colours (DecorationScene.scene's
    // choices), its light animations on or off and their speed.
    property string decoration: ""
    property var choices: ({})
    property bool animated: true
    property real motionSpeed: 1
    property bool running: true
    // The platform's centre line (PlatformGeometry.centreLine's runs) and
    // the icons' places on the platform, in its units; the scene's pixels
    // per unit; the band's half width in the scene's pixels.
    property var centreRuns: []
    property var iconPoints: []
    property real unit: 100
    property real halfWidth: 30
    // The platform's top and bottom, in its units.
    property real top: PlatformGeometry.topHeight
    property real bottom: PlatformGeometry.bottomHeight
    property real fold: 0
    // The way toward the key light, in the scene; the camera's place and
    // turn in the scene (its scene transform); the surface maps' pixels per
    // scene pixel.
    property vector3d lightDirection: Qt.vector3d(0.5, 0.6, 0.6)
    property matrix4x4 cameraTransform: Qt.matrix4x4()
    // The pointer over the scene (its view's pixels; null when it is not),
    // and the scene's way to the view's pixels, to find the round light
    // under it.
    property var pointer: null
    property var project: null
    property real pictureScale: 1
    property real emissionScale: 1
    // Drawn as a hologram: HologramSurface's colours and `own`, its time,
    // flicker and brightness, as the panel's platform wears them.
    property bool hologram: false
    property color hologramOutline: "#ffffff"
    property color hologramInline: "#e6fdff"
    property color hologramVolume: "#5aeaff"
    property vector4d hologramOwn: Qt.vector4d(1, 1, 1, 0)
    property real hologramTime: 0
    property real hologramFlicker: 0
    property real hologramBrightness: 1

    readonly property var entry: decoration.length ? DecorationCatalogue.decoration(decoration) : null
    readonly property bool ready: decorationBaker.triangleCount > 0
    readonly property string errorReason: decorationBaker.error
    // How high the decoration stands under each icon, in the scene's
    // pixels, for the icons to stand on it.
    readonly property var iconHeights: decorationBaker.iconHeights
    readonly property bool glows: decorationBaker.layers.length > 0
    // The round light (a lens) under the pointer, -1 for none: each lens's
    // middle and edge carried to the view's pixels as the panel stands now.
    readonly property var lightSpots: decorationBaker.lightSpots
    readonly property int hoveredSpot: {
        solid.sceneTransform
        cameraTransform
        if (!pointer || !project || !ready) return -1
        let best = -1, nearest = Infinity
        for (let k = 0; k < lightSpots.length; ++k) {
            const s = lightSpots[k]
            const middle = project(solid.mapPositionToScene(Qt.vector3d(Number(s.x), -Number(s.y), Number(s.z) || 0)))
            const edge = project(solid.mapPositionToScene(Qt.vector3d(Number(s.x) + Number(s.radius), -Number(s.y),
                                                                    Number(s.z) || 0)))
            const radius = Math.max(4, Math.hypot(edge.x - middle.x, edge.y - middle.y))
            const away = Math.hypot(pointer.x - middle.x, pointer.y - middle.y)
            if (away <= radius * 1.3 && away < nearest) {
                best = k
                nearest = away
            }
        }
        return best
    }
    // It lights up and fades back; the last one lit stays where it fades.
    property int litSpot: -1
    property real hoverLevel: 0
    Behavior on hoverLevel {
        enabled: root.animated
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }
    onHoveredSpotChanged: {
        if (hoveredSpot >= 0) litSpot = hoveredSpot
        hoverLevel = hoveredSpot >= 0 ? 1 : 0
    }
    readonly property int triangleCount: decorationBaker.triangleCount
    property alias baker: decorationBaker
    // The decoration's outline as drawn, for what keeps clear of it: about
    // six hundred of its solid's vertices as one flat list of x, y, z in the
    // solid's own frame, read once per bake, and the solid's frame in the
    // scene - its origin and a unit step along each of its axes - to carry
    // them with.
    readonly property var outlineVertices: ready && decorationBaker.revision >= 0 ? solidGeometry.outline(600) : []
    function solidFrame() {
        const seen = function(x, y, z) { return solid.mapPositionToScene(Qt.vector3d(x, y, z)) }
        return { origin: seen(0, 0, 0), x: seen(1, 0, 0), y: seen(0, 1, 0), z: seen(0, 0, 1) }
    }

    // The centre line in the decoration's pixels (the scene's, y down as
    // on a flat panel): a closed shape's runs joined round, an arc's one run.
    readonly property var path: {
        const runs = centreRuns || []
        const points = []
        const closed = runs.length > 0 && (runs.length > 1 || runs[0].closed === true)
        for (const run of runs) {
            const samples = run.samples || []
            const count = runs.length > 1 ? 1 : samples.length - (run.closed === true ? 1 : 0)
            for (let k = 0; k < count; ++k)
                points.push({ x: samples[k].c[0] * unit, y: -samples[k].c[1] * unit })
        }
        return { points: points, closed: closed }
    }
    readonly property var iconPlaces: DecorationScene.slotsOnPath(path, (iconPoints || []).map(function(p) {
        return { x: p[0] * unit, y: -p[1] * unit }
    }))

    DecorationBaker {
        id: decorationBaker
        relief: true
        directory: Qt.resolvedUrl("..")
        decoration: root.entry ? root.decoration : ""
        path: root.path.points.length > 1 ? root.path : null
        halfWidth: root.halfWidth
        iconPlaces: root.iconPlaces
        choices: root.choices
        animated: false
        // The band runs on past an arc's end icons as it was designed.
        ends: 2.5
        underside: (root.top - root.bottom) * root.unit
        // Fine enough for the relief's smaller parts to stand as they are
        // drawn.
        triangles: 120000
        scale: root.pictureScale
    }

    // ---- The lights' animation, as on a flat panel ----------------------
    readonly property var lightAnimations: DecorationScene.lightAnimations(entry)
    readonly property bool playing: animated && running && ready && lightAnimations.length > 0
    property real clock: 0
    Timer {
        interval: 33
        repeat: true
        running: root.playing
        onTriggered: root.clock += interval / 1000 * Math.max(0.1, Math.min(3, root.motionSpeed))
    }
    function level(group) {
        return root.playing ? DecorationScene.lightLevel(root.lightAnimations, group, root.clock) : 1
    }
    function lightRect(group) {
        const r = decorationBaker.revision >= 0 ? decorationBaker.lightRect(group) : Qt.rect(0, 0, 0, 0)
        return Qt.vector4d(r.x, r.y, r.width, r.height)
    }
    readonly property size mapSize: decorationBaker.mapSize.width > 0 ? decorationBaker.mapSize : Qt.size(1, 1)

    // Each map read clamped at its edges; a light's map, like the others,
    // with its blurrier levels for the steep sides.
    component SurfaceMap: Texture {
        tilingModeHorizontal: Texture.ClampToEdge
        tilingModeVertical: Texture.ClampToEdge
        generateMipmaps: true
        mipFilter: Texture.Linear
    }
    // Its own colours, worn lit or as a hologram's.
    SurfaceMap {
        id: albedoTexture
        textureData: DecorationMap { baker: decorationBaker; map: "albedo" }
    }
    DecorationSurface {
        id: surfaceMaterial
        albedoMap: TextureInput { texture: albedoTexture }
        finishMap: TextureInput { texture: SurfaceMap { textureData: DecorationMap { baker: decorationBaker; map: "finish" } } }
        shapeMap: TextureInput { texture: SurfaceMap { textureData: DecorationMap { baker: decorationBaker; map: "shape" } } }
        lightsMap: TextureInput { texture: SurfaceMap { textureData: DecorationMap { baker: decorationBaker; map: "lights" } } }
        starsMap: TextureInput { texture: SurfaceMap { textureData: DecorationMap { baker: decorationBaker; map: "stars" } } }
        starsBMap: TextureInput { texture: SurfaceMap { textureData: DecorationMap { baker: decorationBaker; map: "starsB" } } }
        blinkAMap: TextureInput { texture: SurfaceMap { textureData: DecorationMap { baker: decorationBaker; map: "blinkA" } } }
        blinkBMap: TextureInput { texture: SurfaceMap { textureData: DecorationMap { baker: decorationBaker; map: "blinkB" } } }
        arcsMap: TextureInput { texture: SurfaceMap { textureData: DecorationMap { baker: decorationBaker; map: "arcs" } } }
        lightsRect: root.lightRect("lights")
        starsRect: root.lightRect("stars")
        starsBRect: root.lightRect("starsB")
        blinkARect: root.lightRect("blinkA")
        blinkBRect: root.lightRect("blinkB")
        arcsRect: root.lightRect("arcs")
        lightsLevel: root.level("lights")
        starsLevel: root.level("stars")
        starsBLevel: root.level("starsB")
        blinkALevel: root.level("blinkA")
        blinkBLevel: root.level("blinkB")
        arcsLevel: root.level("arcs")
        mapTexel: Qt.vector2d(1 / root.mapSize.width, 1 / root.mapSize.height)
        halfWidthPixels: Math.max(1, decorationBaker.mapHalfWidth)
        // Followed as the platform turns.
        lightInSolid: {
            solid.sceneRotation
            return solid.mapDirectionFromScene(root.lightDirection).normalized()
        }
        viewMatrix: root.cameraTransform.inverted()
        foldTurn: Math.max(-1, Math.min(1, root.fold)) * Math.PI / 2
        emissionScale: root.emissionScale
        hoverSpot: {
            const s = root.litSpot >= 0 ? root.lightSpots[root.litSpot] : null
            const b = decorationBaker.bounds
            if (!s || !(b.width > 0)) return Qt.vector4d(0, 0, 0, 0)
            return Qt.vector4d((Number(s.x) - b.x) / b.width, (Number(s.y) - b.y) / b.height,
                               Number(s.radius) / b.width, Number(s.radius) / b.height)
        }
        hoverLevel: root.hoverLevel
        hoverColour: {
            const s = root.litSpot >= 0 ? root.lightSpots[root.litSpot] : null
            const c = Qt.color(String(s && s.colour || "#ff3b1f"))
            return Qt.vector3d(c.r, c.g, c.b)
        }
    }

    // The bark of what is round all the way round.
    Texture {
        id: barkTexture
        textureData: DecorationMap { baker: decorationBaker; map: "tubeBark" }
        generateMipmaps: true
        mipFilter: Texture.Linear
        tilingModeHorizontal: Texture.Repeat
        tilingModeVertical: Texture.Repeat
    }
    // As a hologram: the solid and its tubes see-through, their colours
    // made light, folded with the platform.
    component HologramPart: HologramSurface {
        property real foldTurn: 0
        vertexShader: "hologram-folded.vert"
        usePicture: 1
        keepColour: 1
        seed: 0.37
        body: 0.75
        cullMode: Material.BackFaceCulling
    }
    HologramPart {
        id: hologramSolid
        objectName: "mesh-platform-decoration-hologram"
        picture: TextureInput { texture: albedoTexture }
        foldTurn: surfaceMaterial.foldTurn
        outlineColour: root.hologramOutline
        inlineColour: root.hologramInline
        volumeColour: root.hologramVolume
        own: root.hologramOwn
        time: root.hologramTime
        flicker: root.hologramFlicker
        brightness: root.hologramBrightness
    }
    HologramPart {
        id: hologramTubes
        objectName: "mesh-platform-decoration-tubes-hologram"
        picture: TextureInput { texture: barkTexture }
        foldTurn: hologramSolid.foldTurn
        outlineColour: hologramSolid.outlineColour
        inlineColour: hologramSolid.inlineColour
        volumeColour: hologramSolid.volumeColour
        own: hologramSolid.own
        time: hologramSolid.time
        flicker: hologramSolid.flicker
        brightness: hologramSolid.brightness
    }

    // The decoration's pixels are the scene's: the platform's units are
    // `unit` of them; its ground lies on the platform's top.
    Node {
        position: Qt.vector3d(0, 0, root.top)
        scale: Qt.vector3d(1 / root.unit, 1 / root.unit, 1 / root.unit)
        Model {
            id: solid
            objectName: "mesh-platform-decoration"
            visible: root.ready
            pickable: true
            geometry: DecorationSolid { id: solidGeometry; baker: decorationBaker }
            materials: root.hologram ? hologramSolid : surfaceMaterial
        }
        // What is round all the way round (a trunk and its branches), as
        // tubes wearing their bark.
        Model {
            objectName: "mesh-platform-decoration-tubes"
            visible: root.ready
            pickable: true
            geometry: DecorationSolid { baker: decorationBaker; part: "tubes" }
            materials: root.hologram ? hologramTubes : tubeMaterial
        }
    }
    DecorationTube {
        id: tubeMaterial
        barkMap: TextureInput { texture: barkTexture }
        reliefMap: TextureInput { texture: Texture { textureData: DecorationMap { baker: decorationBaker; map: "tubeRelief" }
                                                     generateMipmaps: true; mipFilter: Texture.Linear
                                                     tilingModeHorizontal: Texture.Repeat; tilingModeVertical: Texture.Repeat } }
        capMap: TextureInput { texture: Texture { textureData: DecorationMap { baker: decorationBaker; map: "tubeCap" }
                                                  generateMipmaps: true; mipFilter: Texture.Linear } }
        hasRelief: decorationBaker.tubeReliefDepth > 0 ? 1 : 0
        reliefDepth: Math.max(0.01, decorationBaker.tubeReliefDepth)
        roughness: decorationBaker.tubeRoughness
        metalness: decorationBaker.tubeMetalness
        foldTurn: surfaceMaterial.foldTurn
    }
}
