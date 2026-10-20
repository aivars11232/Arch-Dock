import QtQuick
import QtQuick3D

// A decoration's tubes (a trunk and its branches, ArchDock.Decorations3D's
// DecorationSolid with part "tubes"), lit by the scene as it turns
// (decoration-tube.frag): the bark wrapped round them in its own colours, its
// finer grain bending the light and its cracks darker, a cut end showing the
// design's cut face, the marks drawn on it (a knot hole) in their colours.
CustomMaterial {
    property TextureInput barkMap: TextureInput {}
    property TextureInput reliefMap: TextureInput {}
    property TextureInput capMap: TextureInput {}
    // Whether the bark has heights, and how deep they are in the scene's
    // pixels; its roughness and metalness.
    property real hasRelief: 0
    property real reliefDepth: 1
    property real roughness: 0.7
    property real metalness: 0
    // The platform folded (radians): its far half turned about its middle.
    property real foldTurn: 0

    shadingMode: CustomMaterial.Shaded
    cullMode: Material.BackFaceCulling
    vertexShader: "decoration-tube.vert"
    fragmentShader: "decoration-tube.frag"
}
