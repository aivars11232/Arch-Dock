pragma ComponentBehavior: Bound

import QtQuick
import ArchDock.Rendering 1.0

// Draws one role of an icon style (base, tile, glyph treatment...) in 2D:
// every declared layer of that role, coloured and faded as the resolved
// state asks. A layer that fails to load drops the whole style back to the
// plain glyph rather than leaving it half drawn.
Item {
    id: root

    property var styleDefinition: ({})
    property var resolvedStyle: ({})
    property var tileSettings: ({})
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

    // A layer asset that fails at load time must not simply disappear and
    // leave a half-drawn style behind; the scene collapses to the safe
    // original glyph instead.
    //
    // Failures are remembered per asset URL rather than counted, so a result
    // cannot be lost to load/reset ordering and cannot go stale when the
    // declared layer set changes.
    property var failedAssetSources: ({})
    readonly property bool assetFailed: {
        for (let index = 0; index < roleLayers.length; ++index) {
            const assetUrl = IconStyleResolver.assetSource(
                styleDefinition, roleLayers[index])
            if (assetUrl.length > 0 && failedAssetSources[assetUrl] === true)
                return true
        }
        return false
    }

    function recordAssetFailure(assetUrl) {
        const next = ({})
        const keys = Object.keys(failedAssetSources)
        for (let index = 0; index < keys.length; ++index)
            next[keys[index]] = true
        next[assetUrl] = true
        failedAssetSources = next
    }

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
            readonly property bool adjustableTile: root.role === "base"
                && Boolean(modelData.followsIconShape)
                && String(modelData.option || "") !== "pedestal"
            readonly property real shapeWidth: diamond
                ? Math.min(availableWidth, availableHeight) * 0.96
                : plate ? availableWidth * 0.92
                : reflection ? availableWidth * 0.62 : availableWidth
            readonly property real shapeHeight: diamond
                ? shapeWidth : plate ? availableHeight * root.clamped(
                    modelData.heightFactor, 0.05, 0.5, 0.3)
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

            IconTile {
                id: proceduralShape

                x: layerDelegate.shapeX
                y: layerDelegate.shapeY
                    + (root.role === "shadow" ? root.logicalSize * 0.045 : 0)
                width: layerDelegate.shapeWidth
                height: layerDelegate.shapeHeight
                visible: layerDelegate.layerKind === "procedural"
                texture: layerDelegate.adjustableTile ? String(root.tileSettings.iconTileTexture || "none") : "none"
                thickness: layerDelegate.adjustableTile
                    ? root.clamped(root.tileSettings.iconTileThickness, 0, 24, 0) : 0
                bevel: layerDelegate.adjustableTile
                    ? root.clamped(root.tileSettings.iconTileBevel, 0, 12, 0) : 0
                shape: ["circle", "orb", "ring"].includes(layerDelegate.layerShape)
                    ? "circle" : ["rounded-rect", "plate"].includes(layerDelegate.layerShape)
                        ? "rounded" : layerDelegate.layerShape
                radiusFactor: root.clamped(layerDelegate.modelData.radius, 0, 1, 0.22)
                fillColor: layerDelegate.layerShape === "ring"
                    ? "transparent" : root.colorFor(layerDelegate.modelData)
                borderWidth: Math.min(Math.min(width, height) / 2, Math.max(0,
                    root.clamped(layerDelegate.modelData.borderWidth,
                                 0, 0.5, 0) * root.logicalSize))
                borderColor: root.borderFor(layerDelegate.modelData)
                secondaryColor: layerDelegate.layerShape === "ring"
                    ? "transparent" : String(layerDelegate.modelData.secondaryColor
                        || root.colorFor(layerDelegate.modelData))
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
                onStatusChanged: {
                    if (status === Image.Error
                            && source.toString().length > 0)
                        root.recordAssetFailure(source.toString())
                }
            }
        }
    }
}
