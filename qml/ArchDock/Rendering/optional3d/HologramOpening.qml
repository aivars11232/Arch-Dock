import QtQuick
import QtQuick3D

// A projector's opening in a 3D scene (hologram-opening.frag): the lens the
// light leaves, a glowing disc lying on its stand's top or on the panel,
// added to the scene.
CustomMaterial {
    property TextureInput picture: TextureInput {}
    property color tint: "#22d8ff"
    property real time: 0
    property real lens: 0.625
    property real own: 0
    property real strength: 1
    property real brightness: 0.8

    shadingMode: CustomMaterial.Unshaded
    cullMode: Material.NoCulling
    depthDrawMode: Material.NeverDepthDraw
    sourceBlend: CustomMaterial.One
    destinationBlend: CustomMaterial.One
    vertexShader: "hologram-surface.vert"
    fragmentShader: "hologram-opening.frag"
}
