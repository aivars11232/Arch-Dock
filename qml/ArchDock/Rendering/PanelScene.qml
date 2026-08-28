import QtQuick
import ArchDock.Rendering 1.0

Item {
    id: root

    property var panelDefinition: ({})
    property var runtimeState: ({})
    property var orderedEntries: []
    property var hostCapabilities: ({})
    property var themeDefinition: ({})
    property var iconStyleDefinition: ({})
    property var animationProfiles: ({})
    property var screenBounds: ({ x: 0, y: 0, width: 0, height: 0 })
    property var availableBounds: ({ x: 0, y: 0, width: 0, height: 0 })

    readonly property int entryCount:
        orderedEntries && orderedEntries.length !== undefined
        ? orderedEntries.length : 0
    readonly property string layoutPath: String(definitionValue(
        "layout", "pathType", "layout", "horizontal"))
    readonly property bool verticalLayout:
        layoutPath === "vertical"
        || (layoutPath === "adaptive"
            && ["left", "right"].includes(String(definitionValue(
                "placement", "edge", "edge", "bottom"))))
    readonly property real layoutScale: Number(definitionValue(
        "layout", "scale", "layoutScale", 1))
    readonly property real layoutAngle: Number(definitionValue(
        "layout", "angle", "layoutAngle", 0))
    readonly property real layoutRadius: Number(definitionValue(
        "layout", "radius", "layoutRadius", 150))
    readonly property int layoutRows: Number(definitionValue(
        "layout", "rows", "layoutRows", 2))
    readonly property real layoutPadding: Number(definitionValue(
        "layout", "padding", "layoutPadding", 18))
    readonly property int polygonSides: Number(definitionValue(
        "layout", "polygonSides", "pathSides", 6))
    readonly property string pathOrientation: String(definitionValue(
        "layout", "orientation", "pathOrientation", "upright"))
    readonly property real iconSize: Number(iconValue("size", "iconSize", 52))
    readonly property real iconSpacing: Number(iconValue("spacing", "spacing", 8))
    readonly property string iconShape: String(iconValue(
        "shape", "iconShape", "rounded"))
    readonly property string appearance: String(definitionValue(
        "surface", "appearance", "appearance", "glass"))
    readonly property string customColor: String(definitionValue(
        "surface", "color", "color", ""))
    readonly property real panelOpacity: Number(definitionValue(
        "surface", "opacity", "opacity", 0.9))
    readonly property string themeId: String(definitionValue(
        "surface", "panelThemeId", "panelThemeId", ""))
    readonly property string themeSource: String(definitionValue(
        "surface", "themeAsset", "themeAsset", ""))
    readonly property string requestedRendererTier: String(definitionValue(
        "surface", "rendererTier", "rendererTier", "procedural2d")
        || "procedural2d")
    readonly property var capabilityRenderer:
        hostCapabilities && typeof hostCapabilities.renderer === "object"
        ? hostCapabilities.renderer : ({})
    readonly property string resolvedRendererTier: String(
        capabilityRenderer.effectiveTier
        || (hostCapabilities ? hostCapabilities.effectiveRendererTier : "")
        || requestedRendererTier || "procedural2d")
    readonly property var layoutGeometry: LayoutEngine.metrics(
        layoutPath, entryCount, iconSize, iconSpacing, layoutScale,
        layoutRadius, layoutRows, layoutPadding, verticalLayout,
        layoutAngle, polygonSides)
    readonly property var rendererStyle: LayoutEngine.themeStyle(
        appearance, customColor, layoutGeometry.iconSize)
    readonly property real effectMargin: Math.ceil(
        Math.max(0, Number(rendererStyle.blur || 0))
        + Number(rendererStyle.lineWidth || 0) / 2)

    readonly property var contentBounds: ({
        x: 0,
        y: 0,
        width: layoutGeometry.width,
        height: layoutGeometry.height
    })
    readonly property var visualBounds: contentBounds
    readonly property var effectBounds: ({
        x: -effectMargin,
        y: -effectMargin,
        width: layoutGeometry.width + effectMargin * 2,
        height: layoutGeometry.height + effectMargin * 2
    })
    readonly property var inputRegion: ({
        x: 0,
        y: 0,
        width: layoutGeometry.width,
        height: layoutGeometry.height
    })
    readonly property var revealHandle: buildRevealHandle()
    readonly property var popupAnchors: buildPopupAnchors()
    readonly property var previewAnchors: ({
        center: {
            x: layoutGeometry.width / 2,
            y: layoutGeometry.height / 2
        },
        entries: popupAnchors.entries
    })
    readonly property bool fallbackApplied:
        surfaceLoader.fallbackApplied
        || Boolean(capabilityRenderer.fallbackApplied)
        || String(runtimeState ? runtimeState.rendererFallback || "" : "")
            .length > 0
    readonly property string fallbackReason: {
        const runtimeReason = String(
            runtimeState ? runtimeState.rendererFallback || "" : "")
        if (runtimeReason.length > 0)
            return runtimeReason
        if (surfaceLoader.fallbackReason.length > 0)
            return surfaceLoader.fallbackReason
        if (capabilityRenderer.fallbackApplied)
            return String(capabilityRenderer.reasonCode || "capability-fallback")
        return ""
    }
    readonly property string effectiveRendererTier:
        surfaceLoader.effectiveRendererTier
    readonly property var runtimeCapabilityStatus: ({
        available: hostCapabilities
            && hostCapabilities.available !== undefined
            ? Boolean(hostCapabilities.available) : true,
        requestedRendererTier: requestedRendererTier,
        resolvedRendererTier: resolvedRendererTier,
        effectiveRendererTier: effectiveRendererTier,
        fallbackApplied: fallbackApplied,
        fallbackReason: fallbackReason
    })
    readonly property var visualPanel: surfaceLoader.surfaceItem
    readonly property var iconDelegates: entryRepeater

    function definitionValue(sectionName, key, flatKey, fallback) {
        const definition = panelDefinition || ({})
        const section = definition[sectionName]
        if (section && typeof section === "object"
                && section[key] !== undefined && section[key] !== null)
            return section[key]
        const flatValue = definition[flatKey]
        if (flatValue !== undefined && flatValue !== null
                && typeof flatValue !== "object")
            return flatValue
        return fallback
    }

    function iconValue(key, flatKey, fallback) {
        if (iconStyleDefinition && typeof iconStyleDefinition === "object"
                && iconStyleDefinition[key] !== undefined
                && iconStyleDefinition[key] !== null)
            return iconStyleDefinition[key]
        return definitionValue("iconStyle", key, flatKey, fallback)
    }

    function entryGeometryAt(index) {
        return LayoutEngine.entryGeometry(
            layoutPath, index, entryCount, layoutGeometry, layoutAngle,
            polygonSides, pathOrientation, "canonical")
    }

    function entryLabel(entry, index) {
        const source = String(entry && (entry.displayName || entry.name
                              || entry.label || entry.id) || index + 1)
        return source.length > 0 ? source.charAt(0).toUpperCase() : "•"
    }

    function entryColor(entry) {
        if (entry && entry.active)
            return "#60d6ff"
        const configured = iconStyleDefinition
            ? iconStyleDefinition.color || iconStyleDefinition.foreground : ""
        return configured || "#d4e3eb"
    }

    function buildRevealHandle() {
        const edge = String(definitionValue(
            "placement", "edge", "edge", "bottom"))
        const configuredSize = Number(definitionValue(
            "visibility", "revealZone", "revealZone", 8))
        const size = Math.max(1, Math.min(
            Math.max(layoutGeometry.width, layoutGeometry.height),
            isFinite(configuredSize) ? configuredSize : 8))
        let rect = {
            x: 0,
            y: layoutGeometry.height - size,
            width: layoutGeometry.width,
            height: size
        }
        if (edge === "top")
            rect = { x: 0, y: 0, width: layoutGeometry.width, height: size }
        else if (edge === "left")
            rect = { x: 0, y: 0, width: size, height: layoutGeometry.height }
        else if (edge === "right") {
            rect = {
                x: layoutGeometry.width - size,
                y: 0,
                width: size,
                height: layoutGeometry.height
            }
        }
        return {
            mode: String(definitionValue(
                "presentation", "revealHandle", "revealHandle", "edge-strip")),
            edge: edge,
            rect: rect,
            x: rect.x,
            y: rect.y,
            width: rect.width,
            height: rect.height
        }
    }

    function buildPopupAnchors() {
        const anchors = []
        for (let index = 0; index < entryCount; ++index) {
            const output = entryGeometryAt(index)
            anchors.push({
                index: index,
                entryId: String(orderedEntries[index]
                                ? orderedEntries[index].id || "" : ""),
                x: output.position.x + layoutGeometry.iconSize / 2
                    + output.outwardNormal.x * layoutGeometry.iconSize / 2,
                y: output.position.y + layoutGeometry.iconSize / 2
                    + output.outwardNormal.y * layoutGeometry.iconSize / 2,
                outwardNormal: output.outwardNormal
            })
        }
        const hovered = Number(runtimeState
                               ? runtimeState.hoveredEntry : -1)
        const primaryIndex = hovered >= 0 && hovered < anchors.length
            ? hovered : Math.max(0, Math.floor((anchors.length - 1) / 2))
        return {
            primary: anchors.length > 0 ? anchors[primaryIndex] : {
                index: -1,
                entryId: "",
                x: layoutGeometry.width / 2,
                y: layoutGeometry.height / 2,
                outwardNormal: { x: 0, y: -1, angle: -90 }
            },
            entries: anchors
        }
    }

    width: layoutGeometry.width
    height: layoutGeometry.height

    PanelSurfaceLoader {
        id: surfaceLoader

        anchors.fill: parent
        requestedRendererTier: root.resolvedRendererTier
        themeId: root.themeId
        themeSource: root.themeSource
        themeDefinition: root.themeDefinition
        layout: root.layoutPath
        geometry: root.layoutGeometry
        layoutAngle: root.layoutAngle
        polygonSides: root.polygonSides
        appearance: root.appearance
        customColor: root.customColor
        panelOpacity: root.panelOpacity
    }

    Repeater {
        id: entryRepeater

        model: root.orderedEntries || []

        delegate: Item {
            id: entryItem

            required property var modelData
            required property int index
            readonly property var geometryOutput: root.entryGeometryAt(index)

            objectName: "panel-entry-" + index
            x: geometryOutput.position.x
            y: geometryOutput.position.y
            z: geometryOutput.depthOrder
            width: root.layoutGeometry.iconSize
            height: width
            rotation: geometryOutput.rotation
            scale: geometryOutput.scaleFactor
            opacity: modelData && modelData.minimized ? 0.55 : 1

            Rectangle {
                anchors.fill: parent
                radius: root.iconShape === "circle" ? width / 2 : width * 0.24
                color: root.entryColor(entryItem.modelData)
                border.width: 1
                border.color: "#e8ffffff"
            }

            Text {
                anchors.centerIn: parent
                text: root.entryLabel(entryItem.modelData, entryItem.index)
                color: "#18232b"
                font.bold: true
                font.pixelSize: Math.max(10, parent.width * 0.42)
            }
        }
    }

    function entryItemAt(index) {
        return entryRepeater.itemAt(index)
    }
}
