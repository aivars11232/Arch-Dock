pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "StudioThemes.js" as StudioThemes

// Panel Studio's UI Themes page: every look Panel Studio can wear, drawn
// small, and the one in use. A theme chosen here changes Panel Studio at
// once and is remembered; panels on the desktop are not touched.
ColumnLayout {
    id: root

    property string currentThemeId: "classic"
    readonly property var theme: StudioThemes.theme(currentThemeId)

    signal themeChosen(string themeId)

    spacing: 10

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Label {
            text: StudioThemes.label(root.theme, qsTr("UI Themes"))
            color: root.theme.heading
            font.pixelSize: 16
            font.weight: root.theme.headingWeight
            font.letterSpacing: root.theme.letterSpacing
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("How Panel Studio itself looks. Your panels on the desktop are not changed.")
            color: root.theme.description
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 5
            Layout.preferredHeight: 1
            color: root.theme.separator
        }
    }

    GridLayout {
        id: grid
        Layout.fillWidth: true
        columns: width >= 660 ? 2 : 1
        columnSpacing: 12
        rowSpacing: 12

        Repeater {
            model: StudioThemes.list()

            delegate: StudioFrame {
                id: card
                required property var modelData
                readonly property bool inUse: modelData.id === root.currentThemeId

                objectName: "studio-theme-card-" + modelData.id
                Layout.fillWidth: true
                Layout.preferredHeight: cardLayout.implicitHeight + 20
                color: inUse ? root.theme.cardSelected : root.theme.card
                borderColor: inUse ? root.theme.cardBorderSelected : root.theme.cardBorder
                borderWidth: inUse ? 2 : 1
                radius: root.theme.cardRadius
                chamfer: root.theme.chamfer

                ColumnLayout {
                    id: cardLayout
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 6

                    StudioThemePreview {
                        objectName: "studio-theme-preview-" + card.modelData.id
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 220 / 320
                        theme: card.modelData
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: card.modelData.name
                                color: root.theme.cardTitle
                                font.weight: Font.DemiBold
                            }
                            Label {
                                Layout.fillWidth: true
                                text: card.modelData.summary
                                color: root.theme.textSecondary
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                            }
                        }
                        Button {
                            objectName: "studio-theme-use-" + card.modelData.id
                            text: card.inUse ? qsTr("In use") : qsTr("Use this theme")
                            icon.name: card.inUse ? "dialog-ok" : ""
                            enabled: !card.inUse
                            Accessible.name: card.inUse ? qsTr("%1 is in use").arg(card.modelData.name)
                                : qsTr("Use the %1 theme").arg(card.modelData.name)
                            onClicked: root.themeChosen(card.modelData.id)
                        }
                    }
                }
            }
        }
    }

    Item { Layout.fillHeight: true }
}
