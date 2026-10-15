import QtQuick
import QtQuick.Window
import "StudioThemes.js" as StudioThemes

// The window that holds the Icon Properties editor. Closing it without
// Apply discards the draft.
Window {
    id: root

    objectName: "iconPropertiesWindow"
    property var editorSnapshot: ({})
    property var transactionController: panelController
    property bool editorRequestedClose: false
    readonly property string entryIdentity: String(
        editorSnapshot && editorSnapshot.entryIdentity
            ? editorSnapshot.entryIdentity : "")

    // Panel Studio's look (UI Themes), and its controls in its colours.
    readonly property var studioTheme: StudioThemes.theme(typeof dockSettings !== "undefined" && dockSettings
        ? String(dockSettings.studioTheme || "") : "")
    palette.window: studioTheme.windowBottom
    palette.windowText: studioTheme.text
    palette.base: studioTheme.controlBase
    palette.alternateBase: studioTheme.row
    palette.text: studioTheme.controlText
    palette.button: studioTheme.controlButton
    palette.buttonText: studioTheme.controlText
    palette.brightText: studioTheme.accentText
    palette.highlight: studioTheme.accent
    palette.highlightedText: studioTheme.accentText
    palette.placeholderText: studioTheme.controlPlaceholder
    palette.mid: studioTheme.controlBorder
    palette.midlight: studioTheme.rowBorderHover
    palette.dark: studioTheme.accent
    palette.light: studioTheme.textSecondary
    palette.shadow: "#000000"
    palette.toolTipBase: studioTheme.card
    palette.toolTipText: studioTheme.text
    palette.link: studioTheme.accent
    palette.disabled.windowText: studioTheme.textMuted
    palette.disabled.text: studioTheme.textMuted
    palette.disabled.buttonText: studioTheme.textMuted

    width: 620
    height: 590
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    title: qsTr("Arch Dock Icon Properties")
    onClosing: {
        if (!editorRequestedClose)
            editor.discardDraft()
        editorRequestedClose = false
    }

    StudioFrame {
        anchors.fill: parent
        radius: root.studioTheme.radius
        chamfer: root.studioTheme.chamfer
        color: root.studioTheme.windowTop
        colorBottom: root.studioTheme.windowBottom
        borderColor: root.studioTheme.windowBorder
    }

    IconProperties {
        id: editor

        theme: root.studioTheme
        objectName: "iconPropertiesEditor"
        anchors.fill: parent
        anchors.margins: 18
        editorSnapshot: root.editorSnapshot
        transactionController: root.transactionController
        onCloseRequested: {
            root.editorRequestedClose = true
            root.close()
        }
    }
}
