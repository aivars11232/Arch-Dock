import QtQuick
import QtQuick3D

// A projector's light in a 3D scene (hologram-light.frag), on a strip that
// faces the viewer from the opening up to the hologram: the flat panels'
// light (shaders/hologram-beam.frag), added to the scene.
CustomMaterial {
    property TextureInput picture: TextureInput {}
    property color tint: "#22d8ff"
    property vector4d motion: Qt.vector4d(1, 1, 0, 0)
    property real time: 0
    property real startHalf: 0.3
    property real endHalf: 0.5
    property real streaks: 30
    property real sparks: 0.1
    property real seed: 0
    property real fade: 1.6
    property real strength: 1
    property real own: 0
    property real brightness: 1.2

    shadingMode: CustomMaterial.Unshaded
    cullMode: Material.NoCulling
    depthDrawMode: Material.NeverDepthDraw
    sourceBlend: CustomMaterial.One
    destinationBlend: CustomMaterial.One
    vertexShader: "hologram-surface.vert"
    fragmentShader: "hologram-light.frag"
}
