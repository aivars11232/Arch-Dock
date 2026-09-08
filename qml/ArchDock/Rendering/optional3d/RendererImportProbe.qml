import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

QtObject {
    readonly property bool ready: true
    // Compile the actual types without instantiating a scene or allocating a
    // rendering target. This file is never imported by the core QML module.
    property Component viewType: Component { View3D {} }
    property Component assetType: Component { ProceduralMesh {} }
    property Component materialType: Component { PrincipledMaterial {} }
}
