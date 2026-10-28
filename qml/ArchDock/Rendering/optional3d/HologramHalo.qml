import QtQuick
import QtQuick3D

// A 3D hologram's glow where no view spreads light round it (a flat
// panel's icon, in its own small view): its solid a little larger, seen
// only from inside, its light added, strongest straight behind the solid
// and fading out toward its own edge (hologram-halo.frag), so the glow lies
// round the solid's shape from every side.
CustomMaterial {
    property color glowColour: "#22d9ff"
    property real strength: 1

    shadingMode: CustomMaterial.Unshaded
    depthDrawMode: Material.NeverDepthDraw
    cullMode: Material.FrontFaceCulling
    sourceBlend: CustomMaterial.One
    destinationBlend: CustomMaterial.One
    vertexShader: "hologram-surface.vert"
    fragmentShader: "hologram-halo.frag"
}
