import QtQuick
import QtQuick3D

// A panel decoration's solid, lit by the scene as it turns
// (decoration-surface.frag): its colour, metal, roughness and coat from its
// surface maps (DecorationSurface), its relief's grain bending the light
// over the upper surface and casting its own shadows away from the key
// light, the photographer's studio of its flat pictures mirrored in its
// metal and lacquer, and each group of its lights glowing as bright as its
// level says, so a group can pulse or blink on its own.
CustomMaterial {
    property TextureInput albedoMap: TextureInput {}
    property TextureInput finishMap: TextureInput {}
    property TextureInput shapeMap: TextureInput {}
    property TextureInput lightsMap: TextureInput {}
    property TextureInput starsMap: TextureInput {}
    property TextureInput starsBMap: TextureInput {}
    property TextureInput blinkAMap: TextureInput {}
    property TextureInput blinkBMap: TextureInput {}
    property TextureInput arcsMap: TextureInput {}
    // Where each group's light map lies on the surface maps (x, y, width,
    // height, 0 to 1), and how bright the group is now (1 as lit).
    property vector4d lightsRect: Qt.vector4d(0, 0, 0, 0)
    property vector4d starsRect: Qt.vector4d(0, 0, 0, 0)
    property vector4d starsBRect: Qt.vector4d(0, 0, 0, 0)
    property vector4d blinkARect: Qt.vector4d(0, 0, 0, 0)
    property vector4d blinkBRect: Qt.vector4d(0, 0, 0, 0)
    property vector4d arcsRect: Qt.vector4d(0, 0, 0, 0)
    property real lightsLevel: 1
    property real starsLevel: 1
    property real starsBLevel: 1
    property real blinkALevel: 1
    property real blinkBLevel: 1
    property real arcsLevel: 1
    // One pixel of the surface maps (1 / their size), and the band's half
    // width in their pixels.
    property vector2d mapTexel: Qt.vector2d(0.001, 0.001)
    property real halfWidthPixels: 24
    // The way toward the key light in the solid's own frame, for its
    // shadows; the scene seen from the camera (its transform inverted), for
    // the studio's reflections.
    property vector3d lightInSolid: Qt.vector3d(-0.5, 0.62, 0.6)
    property matrix4x4 viewMatrix: Qt.matrix4x4()
    // The platform folded (radians): its far half turned about its middle.
    property real foldTurn: 0
    property real emissionScale: 1
    // The round light under the pointer, lit up: its middle and radius on
    // the surface maps (0 to 1 each way), how lit up it is now (0 to 1) and
    // its colour.
    property vector4d hoverSpot: Qt.vector4d(0, 0, 0, 0)
    property real hoverLevel: 0
    property vector3d hoverColour: Qt.vector3d(1, 0.25, 0.1)

    shadingMode: CustomMaterial.Shaded
    cullMode: Material.BackFaceCulling
    vertexShader: "decoration-surface.vert"
    fragmentShader: "decoration-surface.frag"
}
