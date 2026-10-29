import QtQuick
import QtQuick3D

// A 3D icon made into an object (IconModel) drawn as a hologram
// (hologram-object.frag): HologramSolid's light, read from the object's own
// colours instead of a picture laid over it from the front, so it shows the
// object's detail on every side at whatever size it is drawn; its inner
// lines where those colours change on the screen, a narrower rim and finer
// scan lines. The colours and `own` are HologramLook's, as HologramSolid
// takes them.
CustomMaterial {
    property color outlineColour: "#ffffff"
    property color inlineColour: "#e6fdff"
    property color volumeColour: "#5aeaff"
    property vector4d own: Qt.vector4d(0, 0, 0, 0)
    property real time: 0
    property real flicker: 0
    property real seed: 0
    property real brightness: 1.3
    // How many scan lines cross the icon from its foot to its top.
    property real lineCount: 26

    shadingMode: CustomMaterial.Unshaded
    depthDrawMode: Material.NeverDepthDraw
    cullMode: Material.NoCulling
    sourceBlend: CustomMaterial.One
    destinationBlend: CustomMaterial.One
    vertexShader: "hologram-object.vert"
    fragmentShader: "hologram-object.frag"
}
