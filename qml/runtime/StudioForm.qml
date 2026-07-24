import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ScrollView {
    id: root

    required property var studio
    property var rows: []

    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: 10

        Repeater {
            model: root.rows

            delegate: ColumnLayout {
                id: rowDelegate

                required property var modelData
                readonly property string kind: modelData.kind || "value"
                readonly property bool fieldAvailable: modelData.available === undefined
                    || modelData.available

                Layout.fillWidth: true
                spacing: 6

                ColumnLayout {
                    visible: rowDelegate.kind === "section"
                    Layout.fillWidth: true
                    Layout.topMargin: rowDelegate.modelData.first ? 0 : 10
                    spacing: 2

                    Label {
                        text: rowDelegate.modelData.label || ""
                        color: "#f3f8fb"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }

                    Label {
                        visible: text.length > 0
                        Layout.fillWidth: true
                        text: rowDelegate.modelData.description || ""
                        color: "#91a8b5"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 5
                        height: 1
                        color: "#31526472"
                    }
                }

                Kirigami.InlineMessage {
                    visible: rowDelegate.kind === "notice"
                    Layout.fillWidth: true
                    text: rowDelegate.modelData.text || ""
                    type: rowDelegate.modelData.warning
                        ? Kirigami.MessageType.Warning
                        : Kirigami.MessageType.Information
                }

                Rectangle {
                    visible: rowDelegate.kind !== "section"
                        && rowDelegate.kind !== "notice"
                    Layout.fillWidth: true
                    implicitHeight: fieldRow.implicitHeight + 22
                    radius: 7
                    color: fieldMouse.hovered ? "#223c4d59" : "#172c3b46"
                    border.width: 1
                    border.color: fieldMouse.hovered ? "#426f8a99" : "#263f5260"

                    HoverHandler {
                        id: fieldMouse
                    }

                    RowLayout {
                        id: fieldRow

                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 11
                        anchors.bottomMargin: 11
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 170
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: rowDelegate.modelData.label || ""
                                color: rowDelegate.fieldAvailable ? "#d8e5ec" : "#80919a"
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                            }

                            Label {
                                visible: text.length > 0
                                Layout.fillWidth: true
                                text: rowDelegate.modelData.description || ""
                                color: "#728995"
                                font.pixelSize: 10
                                wrapMode: Text.Wrap
                            }
                        }

                        Label {
                            visible: rowDelegate.kind === "readonly"
                            Layout.preferredWidth: 260
                            horizontalAlignment: Text.AlignRight
                            text: root.studio.displayFieldValue(rowDelegate.modelData)
                            color: "#b9cad3"
                            wrapMode: Text.Wrap
                        }

                        Switch {
                            visible: rowDelegate.kind === "switch"
                            enabled: rowDelegate.fieldAvailable
                            checked: Boolean(root.studio.fieldValue(rowDelegate.modelData))
                            onToggled: root.studio.setFieldValue(
                                rowDelegate.modelData,
                                checked)
                        }

                        ComboBox {
                            visible: rowDelegate.kind === "combo"
                            enabled: rowDelegate.fieldAvailable
                            Layout.preferredWidth: 270
                            model: rowDelegate.modelData.options || []
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.studio.fieldOptionIndex(
                                rowDelegate.modelData)
                            onActivated: root.studio.setFieldValue(
                                rowDelegate.modelData,
                                currentValue)
                        }

                        SpinBox {
                            visible: rowDelegate.kind === "spin"
                            enabled: rowDelegate.fieldAvailable
                            Layout.preferredWidth: 180
                            from: rowDelegate.modelData.from === undefined
                                ? 0 : rowDelegate.modelData.from
                            to: rowDelegate.modelData.to === undefined
                                ? 100 : rowDelegate.modelData.to
                            stepSize: rowDelegate.modelData.step === undefined
                                ? 1 : rowDelegate.modelData.step
                            editable: true
                            value: Number(root.studio.fieldValue(rowDelegate.modelData))
                            onValueModified: root.studio.setFieldValue(
                                rowDelegate.modelData,
                                value)
                        }

                        RowLayout {
                            visible: rowDelegate.kind === "slider"
                            enabled: rowDelegate.fieldAvailable
                            Layout.preferredWidth: 300
                            spacing: 8

                            Slider {
                                Layout.fillWidth: true
                                from: rowDelegate.modelData.from === undefined
                                    ? 0 : rowDelegate.modelData.from
                                to: rowDelegate.modelData.to === undefined
                                    ? 1 : rowDelegate.modelData.to
                                stepSize: rowDelegate.modelData.step === undefined
                                    ? 0.05 : rowDelegate.modelData.step
                                value: Number(root.studio.fieldValue(
                                    rowDelegate.modelData))
                                onMoved: root.studio.setFieldValue(
                                    rowDelegate.modelData,
                                    value)
                            }

                            Label {
                                Layout.minimumWidth: 52
                                horizontalAlignment: Text.AlignRight
                                text: root.studio.formatFieldValue(
                                    rowDelegate.modelData,
                                    root.studio.fieldValue(rowDelegate.modelData))
                                color: "#dce9ef"
                            }
                        }

                        TextField {
                            visible: rowDelegate.kind === "text"
                            enabled: rowDelegate.fieldAvailable
                            Layout.preferredWidth: 270
                            placeholderText: rowDelegate.modelData.placeholder || ""
                            text: String(root.studio.fieldValue(rowDelegate.modelData))
                            onEditingFinished: root.studio.setFieldValue(
                                rowDelegate.modelData,
                                text)
                        }

                        RowLayout {
                            visible: rowDelegate.kind === "color"
                            enabled: rowDelegate.fieldAvailable
                            Layout.preferredWidth: 270
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: 4
                                color: root.studio.colorPreviewValue(
                                    rowDelegate.modelData)
                                border.width: 1
                                border.color: "#8195a1"

                                Kirigami.Icon {
                                    visible: parent.color.a === 0
                                    anchors.centerIn: parent
                                    width: 16
                                    height: 16
                                    source: "edit-clear"
                                    color: "#8799a3"
                                }
                            }

                            Button {
                                Layout.fillWidth: true
                                text: root.studio.colorDisplayValue(
                                    rowDelegate.modelData)
                                icon.name: "color-picker"
                                onClicked: root.studio.openColorEditor(
                                    rowDelegate.modelData)
                            }

                            ToolButton {
                                enabled: String(root.studio.fieldValue(
                                    rowDelegate.modelData)).trim().length > 0
                                icon.name: "edit-clear"
                                onClicked: root.studio.setFieldValue(
                                    rowDelegate.modelData, "")

                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Use theme default")
                            }
                        }

                        Button {
                            visible: rowDelegate.kind === "action"
                            enabled: rowDelegate.fieldAvailable
                            text: rowDelegate.modelData.buttonText
                                || rowDelegate.modelData.label
                            icon.name: rowDelegate.modelData.icon || ""
                            onClicked: root.studio.performStudioAction(
                                rowDelegate.modelData.action,
                                rowDelegate.modelData)
                        }

                        RowLayout {
                            visible: rowDelegate.kind === "actions"
                            spacing: 8

                            Repeater {
                                model: rowDelegate.modelData.actions || []

                                delegate: Button {
                                    required property var modelData

                                    text: modelData.label || ""
                                    icon.name: modelData.icon || ""
                                    enabled: modelData.available === undefined
                                        || modelData.available
                                    onClicked: root.studio.performStudioAction(
                                        modelData.action,
                                        modelData)
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.minimumHeight: 10
        }
    }
}
