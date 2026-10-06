import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

// Loaded only to prove that the QtQuick3D import works in this engine (see
// RendererCapabilityProbe): it compiles the 3D types Arch Dock uses without
// creating a scene.
QtObject {
    readonly property bool ready: true
    // Compile the actual types without instantiating a scene or allocating a
    // rendering target. This file is never imported by the core QML module.
    property Component viewType: Component { View3D {} }
    property Component assetType: Component { ProceduralMesh {} }
    property Component materialType: Component { PrincipledMaterial {} }
}
