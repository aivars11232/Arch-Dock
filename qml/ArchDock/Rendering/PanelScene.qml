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
    property Component entryDelegate: null
    property bool entryInteractionEnabled: true
    property string geometryCompatibilityProfile: "canonical"
    property var entryDelegateContext: ({})

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
    readonly property real glowIntensity: {
        const candidate = Number(definitionValue(
            "surface", "glowIntensity", "glowIntensity", 1))
        return isFinite(candidate) ? candidate : 1
    }
    readonly property color tintColor:
        customColor.length > 0 ? customColor : "#78e9f4"
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
    readonly property string presentationState: {
        const requested = String(runtimeState
                                 ? runtimeState.presentationState || "open"
                                 : "open").toLowerCase()
        return ["normal", "open", "collapsed"].includes(requested)
            ? requested : "open"
    }
    readonly property string transitionState: {
        const requested = String(runtimeState
            ? runtimeState.transitionState || "idle" : "idle").toLowerCase()
        return ["opening", "closing"].includes(requested)
            ? requested : "idle"
    }
    readonly property real presentationProgress: {
        const candidate = Number(runtimeState
            ? runtimeState.presentationProgress : -1)
        return isFinite(candidate) && candidate >= 0 && candidate <= 1
            ? candidate : -1
    }
    readonly property bool panelHovered:
        Boolean(runtimeState ? runtimeState.hovered : false)
    readonly property bool reducedMotion:
        animationProfiles
            && animationProfiles.reducedMotion !== undefined
        ? Boolean(animationProfiles.reducedMotion)
        : Boolean(definitionValue(
            "motion", "reducedMotion", "reducedMotion", false))
    readonly property string themeOrientation:
        verticalLayout ? "vertical"
        : ["horizontal", "adaptive"].includes(layoutPath)
            ? "horizontal" : "free"
    readonly property var layoutGeometry: LayoutEngine.metrics(
        layoutPath, entryCount, iconSize, iconSpacing, layoutScale,
        layoutRadius, layoutRows, layoutPadding, verticalLayout,
        layoutAngle, polygonSides)
    readonly property var activeThemeSlice: themeRecord(
        themeDefinition ? themeDefinition.slices : [],
        presentationState, themeOrientation)
    readonly property var activeThemeContentRegion: themeRecord(
        themeDefinition ? themeDefinition.contentRegions : [],
        presentationState, themeOrientation)
    readonly property bool skinMetadataUsable: usableSkinMetadata()
    readonly property var surfaceMetrics: buildSurfaceMetrics()
    readonly property var rendererGeometry: buildRendererGeometry()
    readonly property var rendererStyle: LayoutEngine.themeStyle(
        appearance, customColor, layoutGeometry.iconSize)
    readonly property real effectMargin: Math.ceil(
        Math.max(0, Number(rendererStyle.blur || 0))
        + Number(rendererStyle.lineWidth || 0) / 2)

    readonly property var contentBounds: ({
        x: surfaceMetrics.contentX,
        y: surfaceMetrics.contentY,
        width: layoutGeometry.width,
        height: layoutGeometry.height
    })
    readonly property var visualBounds: ({
        x: 0,
        y: 0,
        width: surfaceMetrics.width,
        height: surfaceMetrics.height
    })
    readonly property var effectBounds: ({
        x: -surfaceMetrics.effectLeft,
        y: -surfaceMetrics.effectTop,
        width: surfaceMetrics.width + surfaceMetrics.effectLeft
            + surfaceMetrics.effectRight,
        height: surfaceMetrics.height + surfaceMetrics.effectTop
            + surfaceMetrics.effectBottom
    })
    readonly property var inputRegion: ({
        x: 0,
        y: 0,
        width: surfaceMetrics.width,
        height: surfaceMetrics.height
    })
    readonly property var revealHandle: buildRevealHandle()
    readonly property var popupAnchors: buildPopupAnchors()
    readonly property var previewAnchors: ({
        center: {
            x: surfaceMetrics.width / 2,
            y: surfaceMetrics.height / 2
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

    containmentMask: surfaceLoader.inputMaskItem

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

    function values(value) {
        return value && value.length !== undefined ? value : []
    }

    function themeRecord(collection, state, orientation) {
        const candidates = values(collection)
        for (let index = 0; index < candidates.length; ++index) {
            const candidate = candidates[index]
            if (String(candidate.state || "") === state
                    && String(candidate.orientation || "") === orientation)
                return candidate
        }
        return null
    }

    function usableSkinMetadata() {
        if (String(resolvedRendererTier || "").toLowerCase() !== "skinned2d")
            return false
        const theme = themeDefinition || ({})
        if (theme.valid !== true
                || String(theme.format || "") !== "org.archdock.theme"
                || Number(theme.version || 0) !== 2)
            return false
        const candidateId = String(theme.id || theme.themeId || "")
        if (themeId.length > 0 && candidateId.length > 0
                && candidateId !== themeId)
            return false
        const slice = activeThemeSlice
        const region = activeThemeContentRegion
        if (!slice || !slice.sourceRect || !region || !region.rect
                || String(region.shape || "rect") !== "rect")
            return false
        const source = slice.sourceRect
        const content = region.rect
        const sourceWidth = Number(source.width || 0)
        const sourceHeight = Number(source.height || 0)
        const fixedStart = Number(slice.fixedStart || 0)
        const fixedEnd = Number(slice.fixedEnd || 0)
        const centerStart = Number(source.x || 0) + fixedStart
        const centerEnd = Number(source.x || 0) + sourceWidth - fixedEnd
        const contentStart = Number(content.x || 0)
        const contentEnd = contentStart + Number(content.width || 0)
        return sourceWidth > 0 && sourceHeight > 0
            && fixedStart >= 0 && fixedEnd >= 0
            && fixedStart + fixedEnd < sourceWidth
            && Number(content.width || 0) > 0
            && Number(content.height || 0) > 0
            && Number(content.y || 0) >= Number(source.y || 0)
            && Number(content.y || 0) + Number(content.height || 0)
                <= Number(source.y || 0) + sourceHeight
            && contentStart >= centerStart && contentEnd <= centerEnd
    }

    function buildSurfaceMetrics() {
        if (!skinMetadataUsable) {
            return {
                width: layoutGeometry.width,
                height: layoutGeometry.height,
                contentX: 0,
                contentY: 0,
                effectLeft: effectMargin,
                effectTop: effectMargin,
                effectRight: effectMargin,
                effectBottom: effectMargin
            }
        }

        const source = activeThemeSlice.sourceRect
        const content = activeThemeContentRegion.rect
        const sourceX = Number(source.x || 0)
        const sourceY = Number(source.y || 0)
        const sourceWidth = Number(source.width || 0)
        const sourceHeight = Number(source.height || 0)
        const fixedStart = Number(activeThemeSlice.fixedStart || 0)
        const fixedEnd = Number(activeThemeSlice.fixedEnd || 0)
        const centerSourceWidth = sourceWidth - fixedStart - fixedEnd
        const verticalScale = layoutGeometry.height
            / Number(content.height || 1)
        const centerScale = layoutGeometry.width
            / Number(content.width || 1)
        const startWidth = fixedStart * verticalScale
        const endWidth = fixedEnd * verticalScale
        const centerWidth = centerSourceWidth * centerScale
        const margins = themeDefinition.effectMargins || ({})
        return {
            width: startWidth + centerWidth + endWidth,
            height: sourceHeight * verticalScale,
            contentX: startWidth
                + (Number(content.x || 0) - sourceX - fixedStart)
                    * centerScale,
            contentY: (Number(content.y || 0) - sourceY) * verticalScale,
            effectLeft: Math.max(0, Number(margins.left || 0))
                * verticalScale,
            effectTop: Math.max(0, Number(margins.top || 0))
                * verticalScale,
            effectRight: Math.max(0, Number(margins.right || 0))
                * verticalScale,
            effectBottom: Math.max(0, Number(margins.bottom || 0))
                * verticalScale
        }
    }

    function buildRendererGeometry() {
        const result = ({})
        const keys = Object.keys(layoutGeometry || ({}))
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = layoutGeometry[keys[index]]
        result.width = surfaceMetrics.width
        result.height = surfaceMetrics.height
        return result
    }

    function entryGeometryAt(index) {
        const geometry = LayoutEngine.entryGeometry(
            layoutPath, index, entryCount, layoutGeometry, layoutAngle,
            polygonSides, pathOrientation, geometryCompatibilityProfile)
        const result = ({})
        const keys = Object.keys(geometry || ({}))
        for (let keyIndex = 0; keyIndex < keys.length; ++keyIndex)
            result[keys[keyIndex]] = geometry[keys[keyIndex]]
        result.position = {
            x: Number(geometry.position.x || 0) + contentBounds.x,
            y: Number(geometry.position.y || 0) + contentBounds.y
        }
        result.x = result.position.x
        result.y = result.position.y
        result.depthOrder = Number(geometry.depthOrder || 0) + contentBounds.y
        result.bounds = contentBounds
        result.panelBounds = contentBounds
        result.safeInputRegion = contentBounds
        if (geometry.entryBounds) {
            result.entryBounds = {
                x: Number(geometry.entryBounds.x || 0) + contentBounds.x,
                y: Number(geometry.entryBounds.y || 0) + contentBounds.y,
                width: Number(geometry.entryBounds.width || 0),
                height: Number(geometry.entryBounds.height || 0)
            }
        }
        return result
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
            Math.max(surfaceMetrics.width, surfaceMetrics.height),
            isFinite(configuredSize) ? configuredSize : 8))
        let rect = {
            x: 0,
            y: surfaceMetrics.height - size,
            width: surfaceMetrics.width,
            height: size
        }
        if (edge === "top")
            rect = { x: 0, y: 0, width: surfaceMetrics.width, height: size }
        else if (edge === "left")
            rect = { x: 0, y: 0, width: size, height: surfaceMetrics.height }
        else if (edge === "right") {
            rect = {
                x: surfaceMetrics.width - size,
                y: 0,
                width: size,
                height: surfaceMetrics.height
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
                x: surfaceMetrics.width / 2,
                y: surfaceMetrics.height / 2,
                outwardNormal: { x: 0, y: -1, angle: -90 }
            },
            entries: anchors
        }
    }

    width: surfaceMetrics.width
    height: surfaceMetrics.height

    PanelSurfaceLoader {
        id: surfaceLoader

        anchors.fill: parent
        requestedRendererTier: root.resolvedRendererTier
        themeId: root.themeId
        themeSource: root.themeSource
        themeDefinition: root.themeDefinition
        layout: root.layoutPath
        presentationState: root.presentationState
        transitionState: root.transitionState
        presentationProgress: root.presentationProgress
        hovered: root.panelHovered
        tintColor: root.tintColor
        glowIntensity: root.glowIntensity
        reducedMotion: root.reducedMotion
        geometry: root.rendererGeometry
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
            readonly property var sceneEntry: modelData
            readonly property int sceneIndex: index
            readonly property bool sceneInputEnabled:
                root.entryInteractionEnabled
            readonly property var sceneGeometry: geometryOutput
            readonly property var sceneRuntimeState: root.runtimeState
            readonly property var scenePanelDefinition: root.panelDefinition
            readonly property var sceneHostCapabilities: root.hostCapabilities
            readonly property var sceneContext: root.entryDelegateContext
            readonly property var delegateItem: entryLoader.item

            objectName: "panel-entry-" + index
            x: geometryOutput.position.x
            y: geometryOutput.position.y
            z: geometryOutput.depthOrder
            width: root.layoutGeometry.iconSize
            height: width
            rotation: geometryOutput.rotation
            scale: geometryOutput.scaleFactor
            opacity: root.entryDelegate === null && modelData
                && modelData.minimized ? 0.55 : 1

            Loader {
                id: entryLoader

                readonly property var sceneEntry: entryItem.sceneEntry
                readonly property int sceneIndex: entryItem.sceneIndex
                readonly property bool sceneInputEnabled:
                    entryItem.sceneInputEnabled
                readonly property var sceneGeometry: entryItem.sceneGeometry
                readonly property var sceneRuntimeState:
                    entryItem.sceneRuntimeState
                readonly property var scenePanelDefinition:
                    entryItem.scenePanelDefinition
                readonly property var sceneHostCapabilities:
                    entryItem.sceneHostCapabilities
                readonly property var sceneContext: entryItem.sceneContext

                anchors.fill: parent
                sourceComponent: root.entryDelegate || defaultEntryDelegate
            }
        }
    }

    Component {
        id: defaultEntryDelegate

        Item {
            id: defaultEntry

            readonly property var entry: parent.sceneEntry
            readonly property int entryIndex: parent.sceneIndex

            Rectangle {
                anchors.fill: parent
                radius: root.iconShape === "circle" ? width / 2 : width * 0.24
                color: root.entryColor(defaultEntry.entry)
                border.width: 1
                border.color: "#e8ffffff"
            }

            Text {
                anchors.centerIn: parent
                text: root.entryLabel(defaultEntry.entry,
                                      defaultEntry.entryIndex)
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
