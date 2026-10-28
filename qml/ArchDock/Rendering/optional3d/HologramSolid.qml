import QtQuick
import QtQuick3D

// A 3D icon drawn as a hologram (hologram-solid.frag): its solid made of
// light, its picture's colours, outline and inline as a flat hologram's,
// with its shape showing - the far side through the near side, the light
// modelling its rounded parts, the scan lines lying across it. Drawn from
// both sides. The colours and `own` are HologramLook's, as HologramSurface
// takes them.
CustomMaterial {
    property TextureInput picture: TextureInput {}
    property color outlineColour: "#ffffff"
    property color inlineColour: "#e6fdff"
    property color volumeColour: "#5aeaff"
    property vector4d own: Qt.vector4d(0, 0, 0, 0)
    property real time: 0
    property real flicker: 0
    property real seed: 0
    property real brightness: 1.3
    // How many scan lines cross the icon from its foot to its top.
    property real lineCount: 11

    shadingMode: CustomMaterial.Unshaded
    depthDrawMode: Material.NeverDepthDraw
    cullMode: Material.NoCulling
    sourceBlend: CustomMaterial.One
    destinationBlend: CustomMaterial.One
    vertexShader: "hologram-solid.vert"
    fragmentShader: "hologram-solid.frag"
}
