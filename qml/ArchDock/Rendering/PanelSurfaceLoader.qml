import QtQuick
import ArchDock.Rendering 1.0

Item {
    id: root

    property string requestedRendererTier: "procedural2d"
    property string themeId: ""
    property string themeSource: ""
    property var themeDefinition: ({})
    property string layout: "horizontal"
    property string presentationState: "open"
    property var geometry: ({})
    property real layoutAngle: 0
    property int polygonSides: 6
    property string appearance: "glass"
    property string customColor: ""
    property real panelOpacity: 0.9

    readonly property string normalizedRequestedTier:
        String(requestedRendererTier || "procedural2d").toLowerCase()
    readonly property bool themeRequested:
        themeId.length > 0 || themeSource.length > 0
    readonly property bool themeUsable: isThemeUsable(themeDefinition)
    readonly property bool skinnedRequested:
        normalizedRequestedTier === "skinned2d"
    readonly property bool rendererSupported:
        ["procedural2d", "skinned2d"]
            .includes(normalizedRequestedTier)
    readonly property var skinnedItem: skinnedRenderer.item
    readonly property bool skinnedReady:
        skinnedRequested && themeUsable && skinnedItem
        && Boolean(skinnedItem.rendererReady)
    readonly property bool fallbackApplied:
        !rendererSupported || (themeRequested && !themeUsable)
        || (skinnedRequested && !skinnedReady)
    readonly property string fallbackReason: {
        if (themeRequested && !themeUsable)
            return "theme-unavailable"
        if (!rendererSupported)
            return "renderer-unavailable"
        if (skinnedRequested && !skinnedReady) {
            if (skinnedItem && skinnedItem.errorReason.length > 0)
                return skinnedItem.errorReason
            return "renderer-loading"
        }
        return ""
    }
    readonly property string effectiveRendererTier:
        skinnedReady ? "skinned2d" : "procedural2d"
    readonly property var surfaceItem:
        skinnedReady ? skinnedItem : proceduralRenderer.item
    readonly property var inputMaskItem:
        skinnedReady ? skinnedItem.inputMaskItem : null
    readonly property var contentRegionDefinition:
        skinnedReady ? skinnedItem.contentRegionDefinition : null
    readonly property var sliceDefinition:
        skinnedReady ? skinnedItem.sliceDefinition : null
    readonly property var effectMargins:
        skinnedReady && themeDefinition.effectMargins
        ? themeDefinition.effectMargins : ({ left: 0, top: 0, right: 0, bottom: 0 })

    function isThemeUsable(candidate) {
        if (!themeRequested)
            return true
        if (!candidate || typeof candidate !== "object"
                || Object.keys(candidate).length === 0)
            return false
        const candidateId = String(candidate.id || candidate.themeId || "")
        if (themeId.length > 0 && candidateId.length > 0
                && candidateId !== themeId)
            return false
        const candidateSource = String(
            candidate.source || candidate.themeAsset || "")
        if (themeSource.length > 0 && candidateSource.length > 0
                && candidateSource !== themeSource)
            return false
        if (candidate.valid === false || candidate.loadable === false)
            return false
        const status = String(
            candidate.status || candidate.themeStatus || "").toLowerCase()
        return !["invalid", "missing", "failed", "error", "unavailable"]
            .includes(status)
    }

    width: Number(geometry.width || 0)
    height: Number(geometry.height || 0)

    Loader {
        id: proceduralRenderer

        anchors.fill: parent
        sourceComponent: proceduralComponent
        visible: !root.skinnedReady
    }

    Loader {
        id: skinnedRenderer

        anchors.fill: parent
        active: root.skinnedRequested && root.themeUsable
        sourceComponent: skinnedComponent
        visible: root.skinnedReady
    }

    Component {
        id: proceduralComponent

        PanelProcedural2D {
            layout: root.layout
            geometry: root.geometry
            layoutAngle: root.layoutAngle
            polygonSides: root.polygonSides
            appearance: root.appearance
            customColor: root.customColor
            panelOpacity: root.panelOpacity
        }
    }

    Component {
        id: skinnedComponent

        PanelSkin2D {
            themeDefinition: root.themeDefinition
            orientation: root.layout === "vertical"
                ? "vertical" : root.layout === "horizontal"
                    ? "horizontal" : "free"
            presentationState: root.presentationState
            panelOpacity: root.panelOpacity
        }
    }
}
