import QtQuick
import QtQuick3D

// An imported picture worn as a real surface: its brightness is its height,
// so a picture of rocks is rocks, the light stones standing out of the panel
// and the dark cracks sunk into it, over the band's edges too. The mesh
// rises with the picture blurred (picture-relief.vert) and every pixel is
// traced into the picture's own relief, lit by its slope and shadowed where
// the relief stands between it and the key light (picture-relief.frag). The
// mesh says how far each point may rise (uv1's x, none at an open end; see
// PlatformGeometry.reliefPlatform) and where the picture's v grows (its
// binormals). The picture itself comes in `pictureMap`.
CustomMaterial {
    // Repeats of the picture along the band, and the length on the mesh of
    // one unit of its texture coordinates along the band and across it.
    property real repeatU: 1
    property real lengthU: 1
    property real lengthV: 1
    // How far the surface moves, in mesh units; how dark its shadows and
    // low parts are, 0 to 1.
    property real reliefDepth: 0.08
    property real reliefShade: 1
    // 1 moves the raised parts outward, out of the band; 0 keeps them at
    // its surface and sinks the rest.
    property real reliefOutward: 0
    // The length of the mesh's steps, which the relief is blurred to.
    property real meshStep: 0.02
    property real surfaceRoughness: 0.5
    // The way toward the scene's key light, in the scene's space, which
    // the relief's own shadows fall from.
    property vector3d keyLightDirection: Qt.vector3d(0, 0, 1)
    // On a mesh not cut for a relief (a theme's own): the picture's repeats
    // per unit, laid on each face from the side it faces; 0 keeps the
    // mesh's own texture coordinates.
    property real boxScale: 0
    // 1 raises the picture's dark parts instead of its light ones.
    property real reliefInvert: 0
    property TextureInput pictureMap: TextureInput {}

    shadingMode: CustomMaterial.Shaded
    cullMode: Material.BackFaceCulling
    vertexShader: "picture-relief.vert"
    fragmentShader: "picture-relief.frag"
}
