import QtQuick
import ArchDock.Rendering 1.0

Item {
    id: root

    property string requestedRendererTier: "procedural2d"
    property string themeId: ""
    property string themeSource: ""
    property var themeDefinition: ({})
    property string layout: "horizontal"
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
    readonly property bool rendererSupported:
        normalizedRequestedTier === "procedural2d"
    readonly property bool fallbackApplied:
        !rendererSupported || (themeRequested && !themeUsable)
    readonly property string fallbackReason:
        themeRequested && !themeUsable ? "theme-unavailable"
        : !rendererSupported ? "renderer-unavailable" : ""
    readonly property string effectiveRendererTier: "procedural2d"
    readonly property var surfaceItem: renderer.item

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
        id: renderer

        anchors.fill: parent
        sourceComponent: proceduralRenderer
    }

    Component {
        id: proceduralRenderer

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
}
