import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import "../IconSolid.js" as IconSolid
import ArchDock.Decorations3D 1.0

// An icon's own picture as a solid on a flat panel (Icons > 3D): the solid a
// 3D panel draws for it, cut from the picture's own transparency
// (IconSolid.js), seen in a small 3D view of the icon's size in the
// picture's place. Its back lies where the flat picture lay; Icon turn and
// the icon's own turning motion turn it about its upright axis, so its depth
// shows. Lit by the 3D panel's own key and fill lights; as a hologram, made
// of light, with its glow round it (the view then a little larger than the
// icon, to hold the glow).
View3D {
    id: root

    // The icon (IconScene) whose solid this view draws.
    property Item icon: null
    readonly property var mesh: icon ? icon.solidMesh : null
    readonly property real depth: icon ? Number(icon.solidDepth || 0) : 0
    readonly property real lighting: icon ? Number(icon.solidLighting) : 0.5
    readonly property bool hologram: icon ? icon.hologramDrawn === true : false
    readonly property var hologramColours: icon ? icon.hologramColours || ({}) : ({})
    // The view's size in icons.
    readonly property real frame: icon && icon.logicalSize > 0 ? Math.max(1, width / icon.logicalSize) : 1
    function hologramColour(part) {
        return (part && part.colour) ? part.colour : "#ffffff"
    }

    function toVectors(values, dimension) {
        return (values || []).map(function(value) {
            return dimension === 3 ? Qt.vector3d(value[0], value[1], value[2])
                                   : Qt.vector2d(value[0], value[1])
        })
    }
    // An icon made into an object is drawn from its model (IconModel), in a
    // material of its own (IconSurface).
    readonly property bool twoSided: mesh !== null && mesh.native === true

    camera: camera
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Transparent
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }

    // Far off with a narrow view: the solid's back fills the icon's square
    // as the picture filled it, and its near side shows only a little larger.
    PerspectiveCamera {
        id: camera
        fieldOfView: 20
        position: Qt.vector3d(0, 0, 50 * root.frame / Math.tan(10 * Math.PI / 180))
        clipNear: 10
        clipFar: 1000
    }
    DirectionalLight {
        eulerRotation: Qt.vector3d(-35, -35, 0)
        ambientColor: "#202630"
    }
    DirectionalLight {
        eulerRotation: Qt.vector3d(25, 140, 0)
        brightness: 0.4
        color: "#78c8ff"
    }

    // The icon's picture as a saved copy (IconScene), not a live layer.
    Texture {
        id: picture
        source: root.icon ? root.icon.solidPictureUrl : ""
    }

    // Seen a little from above, as the near side of a 3D panel is, so the
    // solid's top and sides show (seen straight on, its depth hid behind its
    // own front), and turned about its upright axis: one rotation on the
    // solid itself. A parent node holding the tilt made Qt Quick 3D look up
    // the parent's place in its list while views were being made, past the
    // list's end, and Plasma stopped.
    readonly property real tilt: 24 * Math.PI / 180
    readonly property real turnAngle: (icon ? Number(icon.solidTurn) + Number(icon.glyphTurnAngle) : 0) * Math.PI / 180
    Model {
        id: solid
        objectName: "flat-icon-solid"
        visible: root.mesh !== null && root.depth > 0
        readonly property real stretch: IconSolid.depthStretch(root.mesh, root.depth)
        readonly property real lift: (root.mesh ? root.mesh.depth : 50) * stretch
        position: Qt.vector3d(0, -lift * Math.sin(root.tilt), lift * Math.cos(root.tilt))
        rotation: Qt.quaternion(Math.cos(root.tilt / 2) * Math.cos(root.turnAngle / 2),
                                Math.sin(root.tilt / 2) * Math.cos(root.turnAngle / 2),
                                Math.cos(root.tilt / 2) * Math.sin(root.turnAngle / 2),
                                Math.sin(root.tilt / 2) * Math.sin(root.turnAngle / 2))
        scale: Qt.vector3d(1, 1, stretch)
        geometry: root.twoSided ? modelledGeometry : pointsGeometry
        property Geometry modelledGeometry: IconModelGeometry {
            model: root.twoSided ? root.mesh : null
        }
        property Geometry pointsGeometry: ProceduralMesh {
            readonly property var points: root.mesh && !root.twoSided ? root.mesh : null
            positions: points ? root.toVectors(points.positions, 3) : []
            normals: points ? root.toVectors(points.normals, 3) : []
            uv0s: points ? root.toVectors(points.uv0s, 2) : []
            indexes: points ? points.indexes : []
            primitiveMode: ProceduralMesh.Triangles
        }
        // An object's surface: each point in the object's own colour there,
        // the light modelling it as far as Icon lighting says.
        property Material surfaceMaterial: IconSurface {
            roughness: 0.7 - 0.5 * root.lighting
            own: 0.5 * (1 - root.lighting)
        }
        // As on a 3D panel: the light models the parts as far as Icon
        // lighting says; the rest is the picture's own brightness.
        property Material litMaterial: PrincipledMaterial {
            lighting: PrincipledMaterial.FragmentLighting
            baseColorMap: picture
            emissiveMap: picture
            emissiveFactor: Qt.vector3d(0.5 * (1 - root.lighting), 0.5 * (1 - root.lighting),
                                        0.5 * (1 - root.lighting))
            alphaMode: PrincipledMaterial.Mask
            alphaCutoff: 0.25
            roughness: 0.7 - 0.5 * root.lighting
            metalness: 0
            cullMode: Material.BackFaceCulling
        }
        // As a hologram, as on a 3D panel: the solid made of light.
        property Material hologramMaterial: HologramSolid {
            objectName: "flat-icon-solid-hologram"
            picture.texture: picture
            outlineColour: root.hologramColour(root.hologramColours.outline)
            inlineColour: root.hologramColour(root.hologramColours.inline)
            volumeColour: root.hologramColour(root.hologramColours.volume)
            own: Qt.vector4d(root.hologramColours.outline && root.hologramColours.outline.own ? 1 : 0,
                             root.hologramColours.inline && root.hologramColours.inline.own ? 1 : 0,
                             root.hologramColours.volume && root.hologramColours.volume.own ? 1 : 0, 0)
            time: root.icon ? root.icon.hologramTime : 0
            flicker: root.icon && root.icon.hologramMoving ? root.icon.hologram.flicker : 0
            seed: root.icon ? root.icon.hologramSeed : 0
            brightness: 1 + 0.6 * (root.icon ? root.icon.hologram.glow : 0)
        }
        // An object as a hologram: made of light from its own colours.
        property Material objectHologramMaterial: HologramObject {
            objectName: "flat-icon-object-hologram"
            outlineColour: solid.hologramMaterial.outlineColour
            inlineColour: solid.hologramMaterial.inlineColour
            volumeColour: solid.hologramMaterial.volumeColour
            own: solid.hologramMaterial.own
            time: solid.hologramMaterial.time
            flicker: solid.hologramMaterial.flicker
            seed: solid.hologramMaterial.seed
            brightness: solid.hologramMaterial.brightness
        }
        materials: root.hologram ? [root.twoSided ? objectHologramMaterial : hologramMaterial]
            : root.twoSided ? [surfaceMaterial] : [litMaterial]
        // The hologram's glow (Icons > Hologram > Glow), in its colour or
        // in the picture's own as one colour, its mean: its solid a little
        // larger round it.
        Model {
            id: halo
            objectName: "flat-icon-solid-glow"
            readonly property var glow: root.hologramColours.glow || ({})
            readonly property real reach: root.icon ? Number(root.icon.hologram.glowReach) : 0.75
            readonly property real spread: 1.12 + 0.25 * reach
            visible: root.hologram && root.icon !== null && root.icon.hologram.glow > 0
            scale: Qt.vector3d(spread, spread, spread)
            geometry: solid.geometry
            materials: HologramHalo {
                glowColour: halo.glow.own && root.mesh && root.mesh.colour
                    ? Qt.rgba(root.mesh.colour[0], root.mesh.colour[1], root.mesh.colour[2], 1)
                    : root.hologramColour(halo.glow)
                strength: 0.9 * (root.icon ? root.icon.hologram.glow : 0)
            }
        }
    }
}
