import QtQuick
import QtQuick3D

// A solid drawn as a hologram (hologram-surface.frag): an icon's solid or
// card, a stand, a platform, see-through and added to the light behind it,
// its rim and its picture's edges brighter, crossed by drifting scan lines,
// blinking now and then. A part with no picture (a stand, a platform) is
// drawn in `ownColour`. The colours and `own` are HologramLook's.
CustomMaterial {
    property TextureInput picture: TextureInput {}
    property real usePicture: 0
    property color ownColour: "#9fdcff"
    property color outlineColour: "#ffffff"
    property color inlineColour: "#e6fdff"
    property color volumeColour: "#5aeaff"
    property vector4d own: Qt.vector4d(0, 0, 0, 0)
    property real time: 0
    property real flicker: 0
    property real seed: 0
    // The scan lines' spacing in the view's pixels, and how bright it is:
    // past 1 the view's glow spreads its light round it.
    property real lineSpacing: 2.2
    property real brightness: 1.3
    property real body: 1
    // How much a picture's own colours are kept (a decoration's, often
    // dark): lifted to a light of the same colour, its lines less white.
    property real keepColour: 0

    shadingMode: CustomMaterial.Unshaded
    depthDrawMode: Material.NeverDepthDraw
    sourceBlend: CustomMaterial.One
    destinationBlend: CustomMaterial.One
    vertexShader: "hologram-surface.vert"
    fragmentShader: "hologram-surface.frag"
}
