import QtQuick
import QtQuick.Controls

Pane {
    id: root

    property string iconName: ""

    implicitWidth: 320
    implicitHeight: 140

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        Label {
            text: "Icon Properties"
            font.bold: true
        }

        Label {
            text: "Icon: " + root.iconName
        }

        Label {
            text: "Properties are not implemented yet."
            wrapMode: Text.WordWrap
        }

        Item {
            width: 1
            height: 16
        }

        Item {
            width: 1
            height: 1
        }

        Item {
            width: parent.width
            height: 48

            Button {
                text: "OK"
                anchors.right: cancelButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    root.window.hide();
                }
            }

            Button {
                id: cancelButton

                text: "Cancel"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                onClicked: {
                    root.window.hide();
                }
            }

        }

    }

}
