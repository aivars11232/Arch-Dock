import QtQuick
import "StudioThemes.js" as StudioThemes

// A small Panel Studio drawn in one theme: its frame and title, the section
// list, a page with two settings and a card, and the buttons at its foot.
// Only shapes and words; nothing in it can be clicked.
Item {
    id: root

    required property var theme
    readonly property bool hud: theme.chamfer > 0
    function label(text) { return StudioThemes.label(root.theme, text); }

    implicitWidth: 320
    implicitHeight: 220

    StudioFrame {
        anchors.fill: parent
        color: root.theme.windowTop
        colorBottom: root.theme.windowBottom
        borderColor: root.theme.windowBorder
        radius: root.theme.radius
        chamfer: root.theme.chamfer
        glowColor: root.theme.glow
        glowSize: root.theme.edgeGlow / 2
    }

    StudioHoneycomb {
        anchors.fill: parent
        anchors.margins: 2
        cell: 6
        color: root.theme.honeycomb
    }

    // The title strip.
    StudioFrame {
        id: title
        x: 6; y: 6
        width: parent.width - 12; height: 22
        color: root.theme.titleStrip
        borderColor: root.theme.titleBorder
        radius: Math.max(0, root.theme.radius - 4)
        chamfer: root.hud ? 6 : 0
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            Rectangle {
                width: 7; height: 7; radius: root.hud ? 0 : 3.5
                rotation: root.hud ? 45 : 0
                color: root.theme.accent
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.label("Arch Dock")
                color: root.theme.titleName
                font.pixelSize: 8; font.weight: Font.DemiBold
                font.letterSpacing: root.theme.letterSpacing / 2
            }
            Text {
                text: root.label("Panel Studio")
                color: root.theme.titleMuted
                font.pixelSize: 8
                font.letterSpacing: root.theme.letterSpacing / 2
            }
        }
        Text {
            anchors.centerIn: parent
            text: root.label("Ring")
            color: root.theme.titleText
            font.pixelSize: 8
            font.letterSpacing: root.theme.letterSpacing / 2
        }
        StudioLightStrip {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.bottom
            width: parent.width * 0.62
            thickness: 1
            color: root.theme.glow
        }
    }

    // The section list.
    StudioFrame {
        id: sidebar
        x: 6; y: title.y + title.height + 5
        width: 76; height: parent.height - y - 30
        color: root.theme.sidebar
        borderColor: root.theme.sidebarBorder
        radius: Math.max(0, root.theme.radius - 4)
        chamfer: root.hud ? 7 : 0
        Column {
            x: 6; y: 6
            spacing: 3
            Text {
                text: "STUDIO"
                color: root.theme.sidebarHeading
                font.pixelSize: 6; font.letterSpacing: 1
                bottomPadding: 2
            }
            Repeater {
                model: ["Overview", "Panels", "Icons", "Profiles", "UI Themes"]
                delegate: StudioFrame {
                    required property string modelData
                    required property int index
                    readonly property bool active: index === 1
                    width: 64; height: 15
                    color: active ? root.theme.sidebarActive : "transparent"
                    borderColor: active ? root.theme.sidebarActiveBorder : "transparent"
                    radius: root.hud ? 0 : 4
                    chamfer: root.hud ? 4 : 0
                    StudioLightStrip {
                        lit: parent.active
                        vertical: true
                        thickness: 1.5
                        x: 1
                        anchors.verticalCenter: parent.verticalCenter
                        height: parent.height * 0.8
                        color: root.theme.glow
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 6
                        text: root.label(parent.modelData)
                        color: parent.active ? root.theme.sidebarTextActive : root.theme.sidebarText
                        font.pixelSize: 7; font.weight: parent.active ? Font.DemiBold : Font.Normal
                        font.letterSpacing: root.theme.letterSpacing / 3
                    }
                }
            }
        }
    }

    // The page.
    Item {
        id: page
        x: sidebar.x + sidebar.width + 8; y: sidebar.y
        width: parent.width - x - 8; height: sidebar.height
        Text {
            id: heading
            text: root.label("Appearance")
            color: root.theme.heading
            font.pixelSize: 10; font.weight: root.theme.headingWeight
            font.letterSpacing: root.theme.letterSpacing / 2
        }
        Text {
            id: description
            anchors.top: heading.bottom
            anchors.topMargin: 1
            text: "Surface styling for the selected panel."
            color: root.theme.description
            font.pixelSize: 6
        }
        Column {
            anchors.top: description.bottom
            anchors.topMargin: 6
            width: parent.width
            spacing: 4
            // A list and a slider, as Appearance shows them.
            Repeater {
                model: [{ label: "Theme", kind: "list" }, { label: "Glow intensity", kind: "slider" }]
                delegate: StudioFrame {
                    required property var modelData
                    width: page.width; height: 20
                    color: root.theme.row
                    borderColor: root.theme.rowBorder
                    radius: root.hud ? 0 : 4
                    chamfer: root.hud ? 5 : 0
                    Text {
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.modelData.label
                        color: root.theme.text
                        font.pixelSize: 7
                    }
                    StudioFrame {
                        visible: parent.modelData.kind === "list"
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 70; height: 12
                        color: root.theme.controlBase
                        borderColor: root.theme.controlBorder
                        radius: root.theme.controlRadius / 2
                        chamfer: root.hud ? 3 : 0
                        Text {
                            x: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Lightning"
                            color: root.theme.controlText
                            font.pixelSize: 6
                        }
                    }
                    Item {
                        visible: parent.modelData.kind === "slider"
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 90; height: 8
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width; height: 2
                            color: root.theme.controlBorder
                        }
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width * 0.6; height: 2
                            color: root.theme.accent
                        }
                        Rectangle {
                            x: parent.width * 0.6 - 4
                            anchors.verticalCenter: parent.verticalCenter
                            width: 8; height: 8; radius: root.hud ? 0 : 4
                            rotation: root.hud ? 45 : 0
                            color: root.theme.accent
                        }
                    }
                }
            }
            // A card, as the Presets page shows them.
            StudioFrame {
                width: page.width; height: 34
                color: root.theme.cardSelected
                borderColor: root.theme.cardBorderSelected
                radius: root.theme.cardRadius / 2
                chamfer: root.hud ? 7 : 0
                Text {
                    x: 6; y: 5
                    text: "Circular Blue Ring"
                    color: root.theme.cardTitle
                    font.pixelSize: 7; font.weight: Font.DemiBold
                }
                Text {
                    x: 6; y: 16
                    text: "Built-in Panel Preset"
                    color: root.theme.textSecondary
                    font.pixelSize: 6
                }
                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 40; height: 16; radius: 8
                    color: "transparent"
                    border.width: 2
                    border.color: root.theme.accent
                }
            }
        }
    }

    // The buttons at its foot.
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 7
        spacing: 4
        Repeater {
            model: ["OK", "Apply", "Cancel"]
            delegate: StudioFrame {
                required property string modelData
                width: 34; height: 15
                color: root.theme.controlButton
                borderColor: root.theme.controlBorder
                radius: root.theme.controlRadius / 2
                chamfer: root.hud ? 4 : 0
                Text {
                    anchors.centerIn: parent
                    text: root.label(parent.modelData)
                    color: root.theme.controlText
                    font.pixelSize: 7
                    font.letterSpacing: root.theme.letterSpacing / 3
                }
            }
        }
    }
}
