import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: root

    property int targetRow: -1
    property string appName: ""
    property string currentIconName: ""

    signal closeRequested()

    onCurrentIconNameChanged: iconSourceField.text = currentIconName

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        Label {
            text: qsTr("Customize icon")
            color: "#f4f8fb"
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }

        Label {
            text: root.appName
            color: "#a9bfcb"
            font.pixelSize: 13
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            spacing: 18

            Rectangle {
                Layout.preferredWidth: 76
                Layout.preferredHeight: 76
                radius: 10
                color: "#26000000"
                border.width: 1
                border.color: "#35ffffff"

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: 56
                    height: 56
                    source: iconSourceField.text.length > 0
                        ? iconSourceField.text
                        : "application-x-executable"
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Choose an image file or enter a KDE icon name.")
                wrapMode: Text.WordWrap
                color: "#cbd8e2"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: iconSourceField

                Layout.fillWidth: true
                text: root.currentIconName
                placeholderText: qsTr("Icon name or image path")
                selectByMouse: true
            }

            Button {
                icon.name: "document-open"
                text: qsTr("Choose image")
                onClicked: imageDialog.open()
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: qsTr("Reset")
                enabled: root.targetRow >= 0
                onClicked: {
                    dockModel.clearCustomIcon(root.targetRow);
                    root.closeRequested();
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Cancel")
                onClicked: root.closeRequested()
            }

            Button {
                text: qsTr("Apply")
                enabled: root.targetRow >= 0
                onClicked: {
                    dockModel.setCustomIcon(root.targetRow, iconSourceField.text);
                    root.closeRequested();
                }
            }
        }
    }

    FileDialog {
        id: imageDialog

        title: qsTr("Choose custom icon")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Image files (*.png *.svg *.svgz *.jpg *.jpeg *.webp)")]
        onAccepted: iconSourceField.text = selectedFile.toString()
    }
}
