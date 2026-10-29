import QtQuick
import QtQuick3D

// A 3D icon made into an object (IconModel), lit by the scene's own lamps:
// each point in the colour the object has there (icon-surface.frag).
CustomMaterial {
    // How glossy it is, and the share of its own colour that shows besides
    // the light, as Icon lighting keeps it.
    property real roughness: 0.5
    property real own: 0.25

    shadingMode: CustomMaterial.Shaded
    cullMode: Material.BackFaceCulling
    vertexShader: "icon-surface.vert"
    fragmentShader: "icon-surface.frag"
}
