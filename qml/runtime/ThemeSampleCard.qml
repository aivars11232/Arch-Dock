pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ArchDock.Rendering 1.0
import "StudioThemes.js" as StudioThemes

// One built-in look, drawn on the selected panel as a bare panel, with no
// icons, so the look itself shows. Load puts it on the panel's draft.
Rectangle {
    id: root

    required property var studio
    required property var theme

    readonly property var rendererCandidate: studio.themeRendererCandidate(theme)
    readonly property var previewConfiguration: theme.previewConfiguration || ({})
    readonly property alias preview: themePreview
    // Panel Studio's look.
    readonly property var studioTheme: studio && studio.studioTheme ? studio.studioTheme : StudioThemes.theme("classic")

    // A look is drawn at the height of a preset card.
    implicitHeight: 188
    // The card is drawn by its frame, in the theme's shape.
    color: "transparent"

    StudioFrame {
        anchors.fill: parent
        radius: root.studioTheme.cardRadius
        chamfer: root.studioTheme.chamfer
        color: root.studioTheme.card
        borderColor: root.studioTheme.cardBorder
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 12

        ColumnLayout {
            Layout.fillWidth: true

            Label {
                text: root.theme.name
                color: root.studioTheme.cardTitle
                font.weight: Font.DemiBold
            }
            Label {
                text: qsTr("Built-in · %1").arg(root.theme.category || "theme")
                color: root.studioTheme.textSecondary
                font.pixelSize: 11
            }

            LivePanelPreview {
                id: themePreview

                objectName: "theme-live-preview-" + root.theme.id
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                panelDefinition: root.rendererCandidate
                hostCapabilities: root.rendererCandidate.capabilityResolution || ({})
                themeDefinition: root.theme
                iconStyleDefinition: root.rendererCandidate.iconStyleDefinition || ({})
                indicatorStyleDefinition: root.theme.indicatorStyle || ({})
                animationProfiles: root.rendererCandidate
                previewMode: String(root.previewConfiguration.mode
                    || root.studio.rendererPreviewMode(root.rendererCandidate))
                presentationState: String(root.previewConfiguration.presentationState || "open")
                stateEntry: Number(root.previewConfiguration.stateEntry === undefined
                    ? 1 : root.previewConfiguration.stateEntry)
                iconState: String(root.previewConfiguration.iconState || "hover")
                showBarePanel: true
                contentMargin: 4
            }

            Label {
                text: themePreview.rendererStatusText
                color: themePreview.fallbackApplied ? root.studioTheme.warning : root.studioTheme.hint
                font.pixelSize: 9
            }
        }
        Button {
            text: qsTr("Load")
            icon.name: "dialog-ok-apply"
            onClicked: root.studio.performStudioAction("load-built-in-theme", { themeId: root.theme.id })
        }
    }
}
