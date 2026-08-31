import QtQuick
import QtQuick.Window

Window {
    id: root

    objectName: "iconPropertiesWindow"
    property var editorSnapshot: ({})
    property var transactionController: panelController
    property bool editorRequestedClose: false
    readonly property string entryIdentity: String(
        editorSnapshot && editorSnapshot.entryIdentity
            ? editorSnapshot.entryIdentity : "")

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

    Rectangle {
        anchors.fill: parent
        radius: 12
        border.width: 1
        border.color: "#75d9edf2"
        gradient: Gradient {
            GradientStop { position: 0; color: "#f62a3848" }
            GradientStop { position: 1; color: "#f1081018" }
        }
    }

    IconProperties {
        id: editor

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
