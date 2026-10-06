import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

// One 3D model built from mesh and material data: a platform, a pedestal, an
// icon's base collar or a theme part. A theme part can open and close
// (partDefinition, openAmount); a generated platform carries its own vertex
// colours and a glowing rim.
Model {
    id: root

    // Numeric resources come from ThemePackage's bounded, path-checked parser.
    required property var meshData
    required property var materialData
    property Texture surfaceTexture: null
    property real emissionScale: 1
    property var partDefinition: null
    property real openAmount: 1
    readonly property real boundedOpenAmount: Math.max(0, Math.min(1, openAmount))

    function partVector(key, fallback) {
        const values = partDefinition ? partDefinition[key] : null
        return values && values.length === 3
            ? Qt.vector3d(Number(values[0]), Number(values[1]), Number(values[2])) : fallback
    }
    function partTransform(closedKey, openKey) {
        const closed = partVector(closedKey, Qt.vector3d(0, 0, 0))
        const opened = partVector(openKey, Qt.vector3d(0, 0, 0))
        return closed.times(1 - boundedOpenAmount).plus(opened.times(boundedOpenAmount))
    }
    position: partTransform("closedPosition", "openPosition")
    eulerRotation: partTransform("closedRotation", "openRotation")
    pivot: partVector("pivot", Qt.vector3d(0, 0, 0))
    scale: partVector("scale", Qt.vector3d(1, 1, 1))
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

    // A generated platform (PlatformGeometry) also carries a colour per vertex,
    // drawn as the look's own colours, and its rim as the index range
    // [rimOffset, rimOffset + rimCount), drawn with a glowing material of its
    // own. A theme's mesh has neither and draws with one material.
    readonly property bool vertexColoured: meshReady && meshData.colors !== undefined
        && meshData.colors.length === meshData.positions.length
    readonly property int rimOffset: meshReady ? Math.max(0, Number(meshData.rimOffset || 0)) : 0
    readonly property int rimCount: meshReady && rimOffset > 0
        && rimOffset + Number(meshData.rimCount || 0) === meshData.indexes.length
        ? Number(meshData.rimCount) : 0
    function colours(values) {
        return (values || []).map(function(value) {
            return Qt.vector4d(value[0], value[1], value[2], value.length > 3 ? value[3] : 1)
        })
    }

    visible: meshReady
    pickable: false // Input and accessibility stay with the 2D entry delegates.
    geometry: ProceduralMesh {
        positions: root.meshReady ? root.vectors(root.meshData.positions, 3) : []
        normals: root.meshReady ? root.vectors(root.meshData.normals, 3) : []
        uv0s: root.meshReady ? root.vectors(root.meshData.uv0s, 2) : []
        colors: root.vertexColoured ? root.colours(root.meshData.colors) : []
        indexes: root.meshReady ? root.meshData.indexes : []
        primitiveMode: ProceduralMesh.Triangles
        subsets: root.rimCount > 0 ? [bodySubset, rimSubset] : []
    }
    readonly property ProceduralMeshSubset bodySubset: ProceduralMeshSubset {
        offset: 0
        count: root.rimOffset
    }
    readonly property ProceduralMeshSubset rimSubset: ProceduralMeshSubset {
        offset: root.rimOffset
        count: root.rimCount
    }
    materials: rimCount > 0 ? [bodyMaterial, rimMaterial] : [bodyMaterial]
    readonly property PrincipledMaterial bodyMaterial: PrincipledMaterial {
        readonly property var values: root.materialData || ({})
        readonly property color emission: values.emissiveColor || "#000000"
        readonly property real strength: Math.max(0, Math.min(2,
            Number(values.emissiveStrength || 0) * root.emissionScale))
        baseColor: root.vertexColoured ? "#ffffff" : values.baseColor || "#ffffff"
        baseColorMap: root.surfaceTexture
        vertexColorsEnabled: root.vertexColoured
        metalness: Math.max(0, Math.min(1, Number(values.metalness || 0)))
        roughness: Math.max(0, Math.min(1, Number(values.roughness || 0)))
        emissiveFactor: Qt.vector3d(emission.r * strength,
                                   emission.g * strength, emission.b * strength)
        cullMode: Material.BackFaceCulling
    }
    // The rim keeps its vertex colour and glows in the look's glow colour.
    readonly property PrincipledMaterial rimMaterial: PrincipledMaterial {
        readonly property var values: root.materialData || ({})
        readonly property color emission: values.rimEmissiveColor || values.emissiveColor || "#000000"
        readonly property real strength: Math.max(0, Math.min(2,
            Number(values.rimEmissiveStrength !== undefined ? values.rimEmissiveStrength
                   : values.emissiveStrength || 0) * root.emissionScale))
        baseColor: "#ffffff"
        vertexColorsEnabled: root.vertexColoured
        metalness: Math.max(0, Math.min(1, Number(values.metalness || 0)))
        roughness: Math.max(0, Math.min(1, Number(values.roughness || 0)))
        emissiveFactor: Qt.vector3d(emission.r * strength,
                                   emission.g * strength, emission.b * strength)
        cullMode: Material.BackFaceCulling
    }
}
