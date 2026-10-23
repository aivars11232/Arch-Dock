import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

// An icon's own picture as a solid on a flat panel (Icons > 3D): the solid a
// 3D panel draws for it, cut from the picture's own transparency
// (IconSolid.js), seen in a small 3D view of the icon's size in the
// picture's place. Its back lies where the flat picture lay; Icon turn and
// the icon's own turning motion turn it about its upright axis, so its depth
// shows. Lit by the 3D panel's own key and fill lights.
View3D {
    id: root

    // The icon (IconScene) whose solid this view draws.
    property Item icon: null
    readonly property var mesh: icon ? icon.solidMesh : null
    readonly property real depth: icon ? Number(icon.solidDepth || 0) : 0
    readonly property real lighting: icon ? Number(icon.solidLighting) : 0.5

    function toVectors(values, dimension) {
        return (values || []).map(function(value) {
            return dimension === 3 ? Qt.vector3d(value[0], value[1], value[2])
                                   : Qt.vector2d(value[0], value[1])
        })
    }

    camera: camera
    environment: SceneEnvironment {
        backgroundMode: SceneEnvironment.Transparent
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
    }

    // Far off with a narrow view: the solid's back fills the view as the
    // picture filled the icon, and its near side shows only a little larger.
    PerspectiveCamera {
        id: camera
        fieldOfView: 20
        position: Qt.vector3d(0, 0, 50 / Math.tan(10 * Math.PI / 180))
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
        objectName: "flat-icon-solid"
        visible: root.mesh !== null && root.depth > 0
        readonly property real lift: (root.mesh ? root.mesh.depth : 50) * root.depth
        position: Qt.vector3d(0, -lift * Math.sin(root.tilt), lift * Math.cos(root.tilt))
        rotation: Qt.quaternion(Math.cos(root.tilt / 2) * Math.cos(root.turnAngle / 2),
                                Math.sin(root.tilt / 2) * Math.cos(root.turnAngle / 2),
                                Math.cos(root.tilt / 2) * Math.sin(root.turnAngle / 2),
                                Math.sin(root.tilt / 2) * Math.sin(root.turnAngle / 2))
        scale: Qt.vector3d(1, 1, root.depth)
        geometry: ProceduralMesh {
            positions: root.mesh ? root.toVectors(root.mesh.positions, 3) : []
            normals: root.mesh ? root.toVectors(root.mesh.normals, 3) : []
            uv0s: root.mesh ? root.toVectors(root.mesh.uv0s, 2) : []
            indexes: root.mesh ? root.mesh.indexes : []
            primitiveMode: ProceduralMesh.Triangles
        }
        // As on a 3D panel: the light models the parts as far as Icon
        // lighting says; the rest is the picture's own brightness.
        materials: PrincipledMaterial {
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
    }
    }
}
