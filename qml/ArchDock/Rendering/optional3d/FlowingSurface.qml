import QtQuick
import QtQuick3D

// A panel whose surface flows: the river's water or the magma's lava, the
// look's texture and relief map running along the band in a loop
// (flowing-surface.frag). Water: the bed stands still under the water and
// wobbles with the ripples passing over it (refraction); two layers of
// ripples run along at different speeds and bend the light; the sky is
// mirrored where the surface tilts toward the viewer (Schlick's
// approximation of Fresnel) and the lights glitter on it. Lava: the crust
// and the lava run along together, the lava glowing with its own light in
// a slow shimmer, a second layer of lava showing dimly through the crust.
// The surface and its relief come in `surfaceMap` and `reliefMap`, the
// look's own texture and relief map.
CustomMaterial {
    property TextureInput surfaceMap: TextureInput {}
    property TextureInput reliefMap: TextureInput {}
    // Lava instead of water.
    property bool molten: false
    // Repeats of the texture along the band.
    property real repeatU: 1
    // How far the surface has flowed along the band, in repeats: 0 to 1 is
    // one loop.
    property real flowOffset: 0

    shadingMode: CustomMaterial.Shaded
    cullMode: Material.BackFaceCulling
    vertexShader: "flowing-surface.vert"
    fragmentShader: "flowing-surface.frag"
}
