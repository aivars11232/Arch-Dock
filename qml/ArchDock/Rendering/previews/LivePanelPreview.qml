pragma ComponentBehavior: Bound

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
    property var indicatorStyleDefinition: ({})
    property var animationProfiles: ({})
    property string previewMode: defaultPreviewMode()
    property string presentationState: "open"
    property string transitionState: "idle"
    property real presentationProgress: -1
    property int hoveredEntry: -1
    property int stateEntry: 1
    property string iconState: "normal"
    property real contentMargin: 12

    readonly property var resolvedPanelDefinition:
        definitionForMode(panelDefinition, previewMode)
    readonly property var previewRuntimeState:
        runtimeForPreview(runtimeState)
    readonly property var effectiveEntries:
        orderedEntries && orderedEntries.length > 0
        ? orderedEntries : defaultEntries
    readonly property string activeRendererTier:
        panelScene.effectiveRendererTier
    readonly property bool fallbackApplied: panelScene.fallbackApplied
    readonly property string fallbackReason: panelScene.fallbackReason
    readonly property string rendererStatusText:
        activeRendererTier + (fallbackApplied
            ? " · " + (fallbackReason || "fallback") : "")
    // The collapse a viewer sees is the panel's own, drawn by PanelScene from
    // PanelMotionController. The preview used to fade and shrink the whole
    // card on its own, which meant a preset card could show a collapse the
    // desktop would never perform. These forward the scene's answer instead.
    readonly property real collapseProgress: panelScene.collapseProgress
    readonly property string presentationTrackForm:
        panelScene.presentationTrackForm
    readonly property string mechanismFallbackReason:
        panelScene.mechanismFallbackReason
    readonly property real sceneFitScale: Math.max(0, Math.min(
        1,
        (width - contentMargin * 2) / Math.max(1, panelScene.width),
        (height - contentMargin * 2) / Math.max(1, panelScene.height)))
    readonly property alias panelSceneItem: panelScene
    readonly property alias sceneLayerItem: sceneLayer

    readonly property var defaultEntries: [
        {
            id: "preview-launcher",
            displayName: qsTr("Launcher"),
            iconName: "start-here-kde",
            running: false
        },
        {
            id: "preview-browser",
            displayName: qsTr("Browser"),
            iconName: "applications-internet",
            running: true,
            active: true,
            windowCount: 1
        },
        {
            id: "preview-files",
            displayName: qsTr("Files"),
            iconName: "system-file-manager",
            running: true,
            minimized: true,
            windowCount: 2
        },
        {
            id: "preview-terminal",
            displayName: qsTr("Terminal"),
            iconName: "utilities-terminal",
            running: false,
            badgeText: "2"
        }
    ]

    function copied(value) {
        if (Array.isArray(value)) {
            const result = []
            for (let index = 0; index < value.length; ++index)
                result.push(copied(value[index]))
            return result
        }
        if (value !== null && value !== undefined
                && typeof value === "object") {
            const result = {}
            const keys = Object.keys(value)
            for (let index = 0; index < keys.length; ++index)
                result[keys[index]] = copied(value[keys[index]])
            return result
        }
        return value
    }

    function definitionValue(definition, sectionName, key, flatKey,
                             fallback) {
        const source = definition || {}
        const section = source[sectionName]
        if (section && typeof section === "object"
                && section[key] !== undefined && section[key] !== null)
            return section[key]
        if (source[flatKey] !== undefined && source[flatKey] !== null
                && typeof source[flatKey] !== "object")
            return source[flatKey]
        return fallback
    }

    function setDefinitionValue(definition, sectionName, key, flatKey,
                                value) {
        if (definition[sectionName]
                && typeof definition[sectionName] === "object") {
            const section = copied(definition[sectionName])
            section[key] = value
            definition[sectionName] = section
        } else {
            definition[flatKey] = value
        }
    }

    function defaultPreviewMode() {
        const edge = String(definitionValue(
            panelDefinition, "placement", "edge", "edge", "bottom"))
        if (edge === "free")
            return "free"
        return edge === "left" || edge === "right"
            ? "vertical" : "horizontal"
    }

    function definitionForMode(definition, mode) {
        const result = copied(definition || {})
        const normalizedMode = ["horizontal", "vertical", "free"]
            .includes(String(mode)) ? String(mode) : defaultPreviewMode()
        let layout = "horizontal"
        let edge = "bottom"
        if (normalizedMode === "vertical") {
            layout = "vertical"
            edge = "left"
        } else if (normalizedMode === "free") {
            const configuredLayout = String(definitionValue(
                result, "layout", "pathType", "layout", "circular"))
            layout = ["", "adaptive", "horizontal", "vertical"]
                .includes(configuredLayout) ? "circular" : configuredLayout
            edge = "free"
        }
        setDefinitionValue(result, "layout", "pathType", "layout", layout)
        setDefinitionValue(result, "placement", "edge", "edge", edge)
        return result
    }

    function runtimeForPreview(state) {
        const result = copied(state || {})
        result.hoveredEntry = hoveredEntry
        result.hovered = hoveredEntry >= 0 || iconState === "hover"
        result.presentationState = presentationState
        result.transitionState = transitionState
        result.presentationProgress = presentationProgress
        return result
    }

    function iconValue(key, flatKey, fallback) {
        const style = iconStyleDefinition || {}
        if (style[key] !== undefined && style[key] !== null)
            return style[key]
        return definitionValue(
            resolvedPanelDefinition, "iconStyle", key, flatKey, fallback)
    }

    function animationValue(key, flatKey, fallback) {
        const profiles = animationProfiles || {}
        if (profiles[key] !== undefined && profiles[key] !== null)
            return profiles[key]
        return definitionValue(
            resolvedPanelDefinition, "animation", key, flatKey, fallback)
    }

    function stateEnabled(state, index) {
        return index === stateEntry && iconState === state
    }

    implicitWidth: 420
    implicitHeight: 180
    clip: true

    Item {
        id: sceneLayer

        anchors.centerIn: parent
        width: panelScene.width
        height: panelScene.height
        scale: root.sceneFitScale

        PanelScene {
            id: panelScene

            panelDefinition: root.resolvedPanelDefinition
            runtimeState: root.previewRuntimeState
            orderedEntries: root.effectiveEntries
            hostCapabilities: root.hostCapabilities
            themeDefinition: root.themeDefinition
            iconStyleDefinition: root.iconStyleDefinition
            animationProfiles: root.animationProfiles
            entryDelegate: previewIconDelegate
            entryInteractionEnabled: false
            geometryCompatibilityProfile: "canonical"
            // A preview shows the configured angle and reports whether the
            // scene would rotate; it never animates the turn itself.
            rotationAnimationEnabled: false
            entryDelegateContext: ({
                preview: true,
                hostKind: root.previewMode === "free" ? "free" : "native"
            })
        }
    }

    Component {
        id: previewIconDelegate

        IconScene {
            id: previewIcon

            readonly property var previewEntry: parent.sceneEntry || ({})
            readonly property int previewIndex: parent.sceneIndex

            anchors.fill: parent
            entry: previewEntry
            iconStyleDefinition: root.iconStyleDefinition
            logicalSize: Number(parent.sceneGeometry.iconSize || width)
            tileShape: String(root.iconValue(
                "shape", "iconShape", "rounded"))
            appearance: String(root.iconValue(
                "appearance", "appearance", "glass"))
            showReflection: Boolean(root.iconValue(
                "showReflection", "showReflections", false))
            showIndicator: Boolean(root.iconValue(
                "showIndicator", "showIndicators", true))
            vertical: panelScene.verticalLayout
            hovered: previewIndex === root.hoveredEntry
                || root.stateEnabled("hover", previewIndex)
            pressed: root.stateEnabled("pressed", previewIndex)
            active: Boolean(previewEntry.active)
                || root.stateEnabled("active", previewIndex)
            running: Boolean(previewEntry.running)
                || root.stateEnabled("running", previewIndex)
            minimized: Boolean(previewEntry.minimized)
                || root.stateEnabled("minimized", previewIndex)
            urgent: Boolean(previewEntry.attention || previewEntry.urgent)
                || root.stateEnabled("urgent", previewIndex)
            launching: Boolean(previewEntry.launching)
                || root.stateEnabled("launching", previewIndex)
            disabled: Boolean(previewEntry.disabled)
                || root.stateEnabled("disabled", previewIndex)
            dropTarget: root.stateEnabled("drop", previewIndex)
            editMode: root.stateEnabled("edit", previewIndex)
            indicatorStyle: root.indicatorStyleDefinition
            reducedMotion: Boolean(root.animationValue(
                "reducedMotion", "reducedMotion", true))
        }
    }
}
