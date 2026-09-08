import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Model {
    id: root

    // Numeric resources come from ThemePackage's bounded, path-checked parser.
    required property var meshData
    required property var materialData
    property Texture surfaceTexture: null
    property real emissionScale: 1
    readonly property bool meshReady: meshData !== null
        && meshData.format === "org.archdock.mesh"
        && meshData.positions !== undefined && meshData.positions.length >= 4
        && meshData.indexes !== undefined && meshData.indexes.length >= 3
    readonly property int triangleCount: meshReady ? meshData.indexes.length / 3 : 0

    function vectors(values, dimension) {
        return (values || []).map(function(value) {
            return dimension === 3 ? Qt.vector3d(value[0], value[1], value[2])
                                   : Qt.vector2d(value[0], value[1])
        })
    }

    visible: meshReady
    pickable: false // Input and accessibility stay with the 2D entry delegates.
    geometry: ProceduralMesh {
        positions: root.meshReady ? root.vectors(root.meshData.positions, 3) : []
        normals: root.meshReady ? root.vectors(root.meshData.normals, 3) : []
        uv0s: root.meshReady ? root.vectors(root.meshData.uv0s, 2) : []
        indexes: root.meshReady ? root.meshData.indexes : []
        primitiveMode: ProceduralMesh.Triangles
    }
    materials: PrincipledMaterial {
        readonly property var data: root.materialData || ({})
        readonly property color emission: data.emissiveColor || "#000000"
        readonly property real strength: Math.max(0, Math.min(2,
            Number(data.emissiveStrength || 0) * root.emissionScale))
        baseColor: data.baseColor || "#ffffff"
        baseColorMap: root.surfaceTexture
        metalness: Math.max(0, Math.min(1, Number(data.metalness || 0)))
        roughness: Math.max(0, Math.min(1, Number(data.roughness || 0)))
        emissiveFactor: Qt.vector3d(emission.r * strength,
                                   emission.g * strength, emission.b * strength)
        cullMode: Material.BackFaceCulling
    }
}
