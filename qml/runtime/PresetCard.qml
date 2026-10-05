pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ArchDock.Rendering 1.0
import "CapabilityModel.js" as CapabilityModel
import "SettingsEditorModel.js" as EditorModel

// One Panel Preset or Icon Preset, drawn by the shared renderer. A card owns
// no data and changes nothing: it shows the record it is given and reports
// what the user asked for.
Rectangle {
    id: root

    required property var preset
    property bool selected: false
    // A card is a still picture of its preset. The selected preset's motion
    // plays in Panel Studio's large preview.
    property bool motionEnabled: false
    property bool actionsEnabled: true

    readonly property string presetId: String(preset.id || "")
    readonly property bool panelPreset: String(preset.kind || "") === "panel"
    readonly property bool builtIn: preset.builtIn === true
    readonly property var compatibility: preset.compatibility || ({})
    // "ready", "fallback" or "incompatible".
    readonly property string presetState: EditorModel.presetState(preset)
    readonly property var rendererCandidate:
        EditorModel.presetRendererCandidate(preset, !motionEnabled)
    readonly property var previewTheme: EditorModel.presetPreviewTheme(preset)
    readonly property LivePanelPreview livePreview:
        previewLoader.item as LivePanelPreview
    readonly property var metadataLines: panelPreset ? panelLines() : iconLines()

    signal selectRequested()
    signal duplicateRequested()
    signal renameRequested()
    signal removeRequested()
    signal previewRequested()
    signal applyRequested()

    function hostLabel(hostKind) {
        return String(hostKind) === "free-desktop" ? qsTr("Free panel") : qsTr("Screen edge");
    }

    function joined(values) {
        const result = [];
        const source = values || [];
        for (let index = 0; index < source.length; ++index)
            result.push(String(source[index]));
        return result.join(", ");
    }

    function motionLine() {
        const profile = String(preset.motionProfileId || "none");
        if (profile === "none")
            return qsTr("Motion: none");
        return qsTr("Motion: %1 on %2")
            .arg(String(preset.motionProfileName || profile))
            .arg(String(preset.motionTrigger || "hover"));
    }

    function behaviorLine() {
        const visibilityLabels = {
            "always": qsTr("always visible"),
            "auto-hide": qsTr("auto-hide"),
            "dodge": qsTr("dodges windows"),
            "cover": qsTr("windows can cover")
        };
        const mode = String(preset.visibilityMode || "always");
        const visibility = visibilityLabels[mode] || mode;
        if (String(preset.presentationMode || "open") !== "collapsed")
            return qsTr("Behavior: %1, rests open").arg(visibility);
        return qsTr("Behavior: %1, rests collapsed and opens on %2 (%3)")
            .arg(visibility)
            .arg(String(preset.presentationTrigger || "hover"))
            .arg(String(preset.collapseMechanism || "open"));
    }

    function panelLines() {
        const hosts = [];
        const hostKinds = preset.hostKinds || [];
        for (let index = 0; index < hostKinds.length; ++index)
            hosts.push(hostLabel(hostKinds[index]));
        const tier = String(preset.rendererTier || "");
        const fallbackTier = String(preset.fallbackTier || tier);
        const lines = [
            qsTr("Host: %1 · Layout: %2 · Orientation: %3")
                .arg(hosts.join(", "))
                .arg(joined(preset.layouts))
                .arg(joined(preset.orientations)),
            (tier === fallbackTier
                ? qsTr("Renderer: %1").arg(tier)
                : qsTr("Renderer: %1, falls back to %2").arg(tier).arg(fallbackTier))
                + " · "
                + (String(preset.themeName || "").length > 0
                    ? qsTr("Theme: %1").arg(String(preset.themeName))
                    : qsTr("Theme: procedural surface")),
            behaviorLine(),
            motionLine()
        ];
        if (String(preset.recommendedIconPresetName || "").length > 0)
            lines.push(qsTr("Recommended icons: %1")
                .arg(String(preset.recommendedIconPresetName)));
        return lines;
    }

    function iconLines() {
        const style = String(preset.iconStyleName || preset.iconStyleId || "");
        const overrides = Object.keys(preset.stateMotionOverrides || ({})).length;
        const lines = [
            preset.customized === true
                ? qsTr("Icon style: %1, customised").arg(style)
                : qsTr("Icon style: %1").arg(style),
            qsTr("Renderer: %1").arg(joined(preset.rendererTiers))
                + " · "
                + (preset.reducedMotionSupport === true
                    ? qsTr("Reduced motion supported")
                    : qsTr("No reduced-motion variant")),
            overrides > 0
                ? qsTr("%1 · %2 per-state override(s)").arg(motionLine()).arg(overrides)
                : motionLine(),
            String(preset.glyphMode || "original") === "original"
                ? qsTr("Glyph: the application's own icon")
                : qsTr("Glyph: %1").arg(String(preset.glyphMode))
        ];
        return lines;
    }

    function subtitle() {
        const kindLabel = panelPreset ? qsTr("Panel Preset") : qsTr("Icon Preset");
        if (builtIn)
            return qsTr("Built-in %1").arg(kindLabel);
        const source = String(preset.derivedFromPresetId || "");
        return source.length > 0
            ? qsTr("My %1 · derived from %2, revision %3")
                .arg(kindLabel).arg(source).arg(Number(preset.sourceRevision || 0))
            : qsTr("My %1").arg(kindLabel);
    }

    function reasonText(code) {
        return CapabilityModel.reasonLabel(code);
    }

    function stateLabel() {
        if (presetState === "ready")
            return qsTr("Ready");
        return presetState === "fallback" ? qsTr("Safe fallback") : qsTr("Incompatible");
    }

    function stateDetail() {
        if (presetState === "ready")
            return "";
        const reason = reasonText(compatibility.reasonCode);
        if (presetState === "incompatible")
            return qsTr("Cannot be used on this system: %1.").arg(reason);
        return panelPreset
            ? qsTr("Shown through its declared fallback (%1) because %2.")
                .arg(CapabilityModel.rendererLabel(compatibility.effectiveRendererTier))
                .arg(reason)
            : qsTr("Shown through its declared fallback because %1.").arg(reason);
    }

    objectName: "preset-card-" + presetId
    implicitHeight: Math.max(176, content.implicitHeight + 20)
    radius: 8
    color: selected ? "#22394a" : "#1b2831"
    border.width: selected || activeFocus ? 2 : 1
    border.color: activeFocus ? "#f4fbff" : selected ? "#73cfe7" : "#3a5868"
    activeFocusOnTab: true

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("%1, %2, %3")
        .arg(String(preset.name || "")).arg(subtitle()).arg(stateLabel())
    Accessible.description: [String(preset.description || "")]
        .concat(metadataLines).concat([stateDetail()]).filter(function(line) {
            return line.length > 0;
        }).join(". ")
    Accessible.focusable: true
    Accessible.selectable: true
    Accessible.selected: selected
    Accessible.onPressAction: root.selectRequested()

    Keys.onReturnPressed: root.selectRequested()
    Keys.onEnterPressed: root.selectRequested()
    Keys.onSpacePressed: root.selectRequested()

    MouseArea {
        anchors.fill: parent
        onClicked: {
            root.forceActiveFocus();
            root.selectRequested();
        }
    }

    RowLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: String(root.preset.name || "")
                    color: "#edf7fa"
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Rectangle {
                    implicitWidth: stateChip.implicitWidth + 14
                    implicitHeight: stateChip.implicitHeight + 4
                    radius: 4
                    color: root.presetState === "ready" ? "#2a80de70"
                        : root.presetState === "fallback" ? "#33ffc66d" : "#33ff8f8f"

                    Label {
                        id: stateChip

                        objectName: "preset-state-" + root.presetId
                        anchors.centerIn: parent
                        text: root.stateLabel()
                        color: root.presetState === "ready" ? "#80de70"
                            : root.presetState === "fallback" ? "#ffc66d" : "#ff9c9c"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: root.subtitle()
                color: "#90a7b4"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            Label {
                visible: text.length > 0
                Layout.fillWidth: true
                text: String(root.preset.description || "")
                color: "#b9cad3"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Repeater {
                model: root.metadataLines

                delegate: Label {
                    required property string modelData

                    Layout.fillWidth: true
                    text: modelData
                    color: "#86a0ae"
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                }
            }

            Label {
                objectName: "preset-state-detail-" + root.presetId
                visible: text.length > 0
                Layout.fillWidth: true
                text: root.stateDetail()
                color: root.presetState === "incompatible" ? "#ff9c9c" : "#ffc66d"
                font.pixelSize: 10
                wrapMode: Text.Wrap

                // The code a report or a log would quote stays one hover away.
                HoverHandler {
                    id: stateDetailHover
                }
                ToolTip.visible: stateDetailHover.hovered
                    && String(root.compatibility.reasonCode || "").length > 0
                ToolTip.text: qsTr("Reported as: %1")
                    .arg(String(root.compatibility.reasonCode || ""))
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.maximumWidth: 300
            Layout.alignment: Qt.AlignTop
            spacing: 4

            Loader {
                id: previewLoader

                Layout.fillWidth: true
                Layout.preferredHeight: 110
                // An incompatible preset has nothing to draw, so no scene is
                // created for it and no stand-in picture is shown instead.
                active: root.presetState !== "incompatible"
                sourceComponent: LivePanelPreview {
                    objectName: "preset-live-preview-" + root.presetId
                    panelDefinition: root.rendererCandidate
                    hostCapabilities:
                        root.rendererCandidate.capabilityResolution || ({})
                    themeDefinition: root.previewTheme
                    iconStyleDefinition:
                        root.rendererCandidate.iconStyleDefinition || ({})
                    indicatorStyleDefinition:
                        root.previewTheme.indicatorStyle || ({})
                    animationProfiles: root.rendererCandidate
                    previewMode: String(
                        (root.preset.preview || ({})).previewMode || "horizontal")
                    presentationState: "open"
                    stateEntry: 1
                    iconState: "hover"
                    contentMargin: 4
                }
            }

            Label {
                objectName: "preset-no-preview-" + root.presetId
                visible: !previewLoader.active
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: qsTr("No preview: this preset cannot be drawn on this system.")
                color: "#8195a1"
                font.pixelSize: 10
                wrapMode: Text.Wrap
            }

            Label {
                visible: root.livePreview !== null
                Layout.fillWidth: true
                text: root.livePreview ? root.livePreview.rendererStatusText : ""
                color: root.livePreview && root.livePreview.fallbackApplied
                    ? "#ffc66d" : "#72909f"
                font.pixelSize: 9
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    objectName: "preset-preview-" + root.presetId
                    text: qsTr("Preview on Desktop")
                    enabled: root.actionsEnabled && root.presetState !== "incompatible"
                    Accessible.name: qsTr("Preview %1 on desktop").arg(String(root.preset.name || ""))
                    onClicked: root.previewRequested()
                }
                Button {
                    objectName: "preset-apply-" + root.presetId
                    text: qsTr("Apply as Active")
                    enabled: root.actionsEnabled && root.presetState !== "incompatible"
                    Accessible.name: qsTr("Apply %1 as active").arg(String(root.preset.name || ""))
                    onClicked: root.applyRequested()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    objectName: "preset-rename-" + root.presetId
                    visible: !root.builtIn
                    text: qsTr("Rename")
                    icon.name: "edit-rename"
                    Accessible.name: qsTr("Rename %1").arg(String(root.preset.name || ""))
                    onClicked: root.renameRequested()
                }

                Button {
                    objectName: "preset-duplicate-" + root.presetId
                    text: root.builtIn ? qsTr("Duplicate to My Presets") : qsTr("Duplicate")
                    icon.name: "edit-copy"
                    Accessible.name: qsTr("Duplicate %1 to my presets")
                        .arg(String(root.preset.name || ""))
                    onClicked: root.duplicateRequested()
                }

                Button {
                    objectName: "preset-delete-" + root.presetId
                    visible: !root.builtIn
                    text: qsTr("Delete")
                    icon.name: "edit-delete"
                    Accessible.name: qsTr("Delete %1").arg(String(root.preset.name || ""))
                    onClicked: root.removeRequested()
                }
            }
        }
    }
}
