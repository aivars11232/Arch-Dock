pragma ComponentBehavior: Bound

import QtQuick
import ArchDock.Rendering 1.0

Item {
    id: root

    property var styleDefinition: ({})
    property var resolvedStyle: ({})
    property string role: "base"
    property real logicalSize: Math.min(width, height)
    property real roleOpacity: 1
    property color stateBorderColor: "transparent"
    property color stateGlowColor: "transparent"

    readonly property var roleLayers: IconStyleResolver.layersForRole(
        styleDefinition, role)
    readonly property int layerCount: roleLayers.length
    readonly property var renderedLayerIds: IconStyleResolver.layerIds(
        styleDefinition, role)

    function clamped(value, minimum, maximum, fallback) {
        const candidate = Number(value)
        return Math.max(minimum, Math.min(maximum,
            isFinite(candidate) ? candidate : fallback))
    }

    function colorFor(layer) {
        if (role === "glow" && stateGlowColor.toString() !== "#00000000")
            return stateGlowColor
        return String(layer.color || "transparent")
    }

    function borderFor(layer) {
        if (role === "front"
                && stateBorderColor.toString() !== "#00000000")
            return stateBorderColor
        if (role === "glow"
                && stateGlowColor.toString() !== "#00000000")
            return stateGlowColor
        return String(layer.borderColor || "transparent")
    }

    objectName: "icon-style-" + role

    Repeater {
        id: layerRepeater

        model: root.roleLayers

        delegate: Item {
            id: layerDelegate

            required property var modelData
            required property int index

            readonly property string layerKind: String(
                modelData.kind || "procedural")
            readonly property string layerShape: String(
                modelData.shape || "rounded-rect")
            readonly property real normalizedInset: root.clamped(
                modelData.inset, 0, 0.45, 0)
            readonly property real insetPixels:
                normalizedInset * root.logicalSize
            readonly property real availableWidth: Math.max(
                0, root.width - insetPixels * 2)
            readonly property real availableHeight: Math.max(
                0, root.height - insetPixels * 2)
            readonly property bool diamond: layerShape === "diamond"
            readonly property bool plate: layerShape === "plate"
            readonly property bool reflection: root.role === "reflection"
            readonly property real shapeWidth: diamond
                ? Math.min(availableWidth, availableHeight) * 0.68
                : plate ? availableWidth * 0.92
                : reflection ? availableWidth * 0.62 : availableWidth
            readonly property real shapeHeight: diamond
                ? shapeWidth : plate ? availableHeight * 0.3
                : reflection ? availableHeight * 0.2 : availableHeight
            readonly property real shapeX:
                (root.width - shapeWidth) / 2
            readonly property real shapeY: plate
                ? root.height - insetPixels - shapeHeight
                : reflection ? insetPixels + availableHeight * 0.08
                : (root.height - shapeHeight) / 2

            objectName: "icon-style-layer-" + String(modelData.id || index)
            anchors.fill: parent
            opacity: root.clamped(modelData.opacity, 0, 1, 1)
                * root.clamped(root.roleOpacity, 0, 1, 1)
            visible: opacity > 0.001

            Rectangle {
                id: proceduralShape

                x: layerDelegate.shapeX
                y: layerDelegate.shapeY
                    + (root.role === "shadow" ? root.logicalSize * 0.045 : 0)
                width: layerDelegate.shapeWidth
                height: layerDelegate.shapeHeight
                rotation: layerDelegate.diamond ? 45 : 0
                visible: layerDelegate.layerKind === "procedural"
                radius: ["circle", "orb"].includes(
                    layerDelegate.layerShape)
                    ? Math.min(width, height) / 2
                    : root.clamped(layerDelegate.modelData.radius,
                                   0, 1, 0.22)
                        * Math.min(width, height)
                color: layerDelegate.layerShape === "ring"
                    ? "transparent" : root.colorFor(layerDelegate.modelData)
                border.width: Math.max(0,
                    root.clamped(layerDelegate.modelData.borderWidth,
                                 0, 0.25, 0) * root.logicalSize)
                border.color: root.borderFor(layerDelegate.modelData)

                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop {
                        position: 0
                        color: layerDelegate.layerShape === "ring"
                            ? "transparent"
                            : String(layerDelegate.modelData.secondaryColor
                                     || root.colorFor(
                                         layerDelegate.modelData))
                    }
                    GradientStop {
                        position: 1
                        color: layerDelegate.layerShape === "ring"
                            ? "transparent"
                            : root.colorFor(layerDelegate.modelData)
                    }
                }
            }

            Image {
                x: layerDelegate.shapeX
                y: layerDelegate.shapeY
                width: layerDelegate.shapeWidth
                height: layerDelegate.shapeHeight
                visible: layerDelegate.layerKind === "asset"
                source: IconStyleResolver.assetSource(
                    root.styleDefinition, layerDelegate.modelData)
                fillMode: Image.PreserveAspectFit
                asynchronous: false
                cache: true
                mipmap: true
            }
        }
    }
}
