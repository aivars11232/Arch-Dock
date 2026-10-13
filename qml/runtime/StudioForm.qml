pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ArchDock.Rendering 1.0
import org.kde.kirigami as Kirigami

// Draws one Panel Studio page from its rows: section titles, notices,
// controls for settings (switches, sliders, spin boxes, combo boxes, colour
// and file fields), action buttons, and theme and preset sample cards.
ScrollView {
    id: root

    required property var studio
    property var rows: []
    // The page the rows belong to: another page starts at its top.
    property string pageKey: ""
    property var rowsByKey: ({})
    readonly property ListModel rowModel: ListModel {}

    // Every change in the Studio hands the form a new list of rows. With the
    // list as the model every row was built again each time: the page jumped
    // back to its top, the control in use was destroyed under the pointer, so
    // a drag or an edit low on the page was lost, and every row (with every
    // theme card's live preview) was rebuilt (the owner, 2026-10-13). Each row
    // keeps its place under a key now: a changed value updates its row, and
    // only rows that come or go are built or dropped.
    function rowKey(row) {
        const kind = String(row.kind || "value");
        if (row.key !== undefined || row.segmentIndex !== undefined)
            return [kind, row.scope || "", row.key || "", row.segmentIndex === undefined ? "" : row.segmentIndex,
                row.segmentKey || "", row.entryId === undefined ? "" : row.entryId].join("|");
        if (kind === "action")
            return [kind, row.action || "", row.label || ""].join("|");
        return [kind, String(row.label || row.text || "")].join("|");
    }

    function synchronizeRows() {
        const source = rows || [];
        const keys = [];
        const byKey = ({});
        const repeats = ({});
        for (let index = 0; index < source.length; ++index) {
            let key = rowKey(source[index]);
            repeats[key] = (repeats[key] || 0) + 1;
            if (repeats[key] > 1)
                key += "#" + repeats[key];
            keys.push(key);
            byKey[key] = source[index];
        }
        rowsByKey = byKey;
        for (let index = rowModel.count - 1; index >= 0; --index) {
            if (byKey[rowModel.get(index).rowKey] === undefined)
                rowModel.remove(index);
        }
        for (let index = 0; index < keys.length; ++index) {
            if (index < rowModel.count && rowModel.get(index).rowKey === keys[index])
                continue;
            let found = -1;
            for (let later = index + 1; later < rowModel.count && found < 0; ++later) {
                if (rowModel.get(later).rowKey === keys[index])
                    found = later;
            }
            if (found >= 0)
                rowModel.move(found, index, 1);
            else
                rowModel.insert(index, { rowKey: keys[index] });
        }
    }

    onRowsChanged: synchronizeRows()
    onPageKeyChanged: contentItem.contentY = 0
    Component.onCompleted: synchronizeRows()

    clip: true
    implicitWidth: 0 // The containing layout supplies width; avoid the KDE contentWidth loop.
    contentWidth: availableWidth

    ColumnLayout {
        width: root.availableWidth
        spacing: 10

        Repeater {
            model: root.rowModel

            delegate: ColumnLayout {
                id: rowDelegate

                required property string rowKey
                readonly property var modelData: root.rowsByKey[rowKey] || ({})
                readonly property string kind: modelData.kind || "value"
                readonly property bool sampleCards: kind === "themeSamples"

                visible: modelData.available === undefined || modelData.available
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
                        Layout.preferredHeight: 1
                        color: "#31526472"
                    }
                }

                Kirigami.InlineMessage {
                    visible: rowDelegate.kind === "notice"
                    Layout.fillWidth: true
                    text: rowDelegate.modelData.text || ""
                    type: rowDelegate.modelData.warning ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
                }

                // Sample cards: the built-in themes, each drawn on this panel.
                ColumnLayout {
                    id: sampleRow

                    visible: rowDelegate.sampleCards
                    Layout.fillWidth: true
                    spacing: 8

                    Label {
                        text: rowDelegate.modelData.label || ""
                        color: "#f3f8fb"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                    Label {
                        Layout.fillWidth: true
                        text: rowDelegate.modelData.description || ""
                        color: "#91a8b5"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }
                    Repeater {
                        model: (rowDelegate.modelData.themes || []).length
                        delegate: Rectangle {
                            id: themeSample

                            required property int index
                            readonly property var modelData: (rowDelegate.modelData.themes || [])[index] || ({})
                            Layout.fillWidth: true
                            // A theme is drawn at the height of a preset card.
                            implicitHeight: 188
                            radius: 8
                            color: "#1b2831"
                            border.width: 1
                            border.color: "#3a5868"
                            readonly property var rendererCandidate:
                                root.studio.themeRendererCandidate(modelData)
                            // The theme the card is drawn on.
                            readonly property var sampleTheme: modelData
                            readonly property var previewConfiguration:
                                modelData.previewConfiguration || ({})

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 12
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: themeSample.modelData.name
                                        color: "#edf7fa"
                                        font.weight: Font.DemiBold
                                    }
                                    Label {
                                        text: qsTr("Built-in · %1").arg(themeSample.modelData.category
                                            || "theme")
                                        color: "#90a7b4"
                                        font.pixelSize: 11
                                    }

                                    LivePanelPreview {
                                        id: themePreview

                                        objectName: "theme-live-preview-" + themeSample.modelData.id
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 110
                                        panelDefinition:
                                            themeSample.rendererCandidate
                                        hostCapabilities:
                                            themeSample.rendererCandidate
                                                .capabilityResolution || ({})
                                        themeDefinition:
                                            themeSample.sampleTheme
                                        iconStyleDefinition:
                                            themeSample.rendererCandidate
                                                .iconStyleDefinition || ({})
                                        indicatorStyleDefinition:
                                            themeSample.sampleTheme
                                                .indicatorStyle || ({})
                                        animationProfiles:
                                            themeSample.rendererCandidate
                                        previewMode:
                                            String(themeSample
                                                .previewConfiguration.mode
                                                || root.studio
                                                    .rendererPreviewMode(
                                                        themeSample
                                                            .rendererCandidate))
                                        presentationState:
                                            String(themeSample
                                                .previewConfiguration
                                                    .presentationState
                                                || "open")
                                        stateEntry: Number(themeSample
                                            .previewConfiguration.stateEntry
                                            === undefined ? 1
                                            : themeSample.previewConfiguration
                                                .stateEntry)
                                        iconState: String(themeSample
                                            .previewConfiguration.iconState
                                            || "hover")
                                        contentMargin: 4
                                    }

                                    Label {
                                        text: themePreview.rendererStatusText
                                        color: themePreview.fallbackApplied
                                            ? "#ffc66d" : "#72909f"
                                        font.pixelSize: 9
                                    }
                                }
                                Button {
                                    text: qsTr("Load")
                                    icon.name: "dialog-ok-apply"
                                    onClicked: root.studio.performStudioAction("load-built-in-theme", {
                                        themeId: themeSample.modelData.id
                                    })
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    visible: rowDelegate.kind !== "section" && rowDelegate.kind !== "notice" && !rowDelegate.sampleCards
                    Layout.fillWidth: true
                    implicitHeight: fieldRow.implicitHeight + 22
                    radius: 7
                    color: fieldMouse.hovered ? "#223c4d59" : "#172c3b46"
                    border.width: 1
                    border.color: fieldMouse.hovered ? "#426f8a99" : "#263f5260"

                    HoverHandler {
                        id: fieldMouse
                    }

                    GridLayout {
                        id: fieldRow
                        columns: root.availableWidth < 560 ? 1 : 2

                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        anchors.topMargin: 11
                        anchors.bottomMargin: 11
                        rowSpacing: 8
                        columnSpacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 170
                            spacing: 2

                            Label {
                                Layout.fillWidth: true
                                text: rowDelegate.modelData.label || ""
                                color: "#d8e5ec"
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
                            objectName: "studio-switch-" + String(rowDelegate.modelData.key || "")
                            Accessible.name: rowDelegate.modelData.label || ""
                            visible: rowDelegate.kind === "switch"
                            checked: Boolean(root.studio.fieldValue(rowDelegate.modelData))
                            onToggled: root.studio.setFieldValue(rowDelegate.modelData, checked)
                        }

                        ComboBox {
                            // A new model moves the box to its first option; the
                            // options are handed over again only when they change.
                            readonly property string optionsKey: JSON.stringify(rowDelegate.modelData.options || [])

                            wheelEnabled: false
                            objectName: "studio-combo-" + String(rowDelegate.modelData.key || "")
                            Accessible.name: rowDelegate.modelData.label || ""
                            visible: rowDelegate.kind === "combo"
                            Layout.preferredWidth: 270
                            model: JSON.parse(optionsKey)
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.studio.fieldOptionIndex(rowDelegate.modelData)
                            onActivated: root.studio.setFieldValue(rowDelegate.modelData, currentValue)
                        }

                        SpinBox {
                            wheelEnabled: false
                            objectName: "studio-spin-" + String(rowDelegate.modelData.key || "")
                            Accessible.name: rowDelegate.modelData.label || ""
                            visible: rowDelegate.kind === "spin"
                            Layout.preferredWidth: 180
                            from: rowDelegate.modelData.from === undefined ? 0 : rowDelegate.modelData.from
                            to: rowDelegate.modelData.to === undefined ? 100 : rowDelegate.modelData.to
                            stepSize: rowDelegate.modelData.step === undefined ? 1 : rowDelegate.modelData.step
                            editable: true
                            validator: null
                            value: Number(root.studio.fieldValue(rowDelegate.modelData))
                            textFromValue: function(value, locale) {
                                return root.studio.formatFieldValue(rowDelegate.modelData, value)
                            }
                            valueFromText: function(text, locale) {
                                return root.studio.parseFieldValue(rowDelegate.modelData, text,
                                    Number(root.studio.fieldValue(rowDelegate.modelData)))
                            }
                            onValueModified: root.studio.setFieldValue(rowDelegate.modelData, value)
                        }

                        RowLayout {
                            visible: rowDelegate.kind === "slider"
                            Layout.preferredWidth: 300
                            spacing: 8

                            Slider {
                                id: fieldSlider
                                wheelEnabled: false
                                objectName: "studio-slider-" + String(rowDelegate.modelData.key || "")
                                Accessible.name: rowDelegate.modelData.label || ""
                                Layout.fillWidth: true
                                readonly property real low: rowDelegate.modelData.from === undefined ? 0 : rowDelegate.modelData.from
                                readonly property real high: rowDelegate.modelData.to === undefined ? 1 : rowDelegate.modelData.to
                                // On a logarithmic row the slider moves through the
                                // ratio from `low` to `high`, and the value it sets
                                // is rounded to the row's decimals.
                                readonly property bool logarithmic: rowDelegate.modelData.logarithmic === true
                                    && low > 0 && high > low
                                readonly property real fieldNumber: Number(root.studio.fieldValue(rowDelegate.modelData))
                                from: logarithmic ? 0 : low
                                to: logarithmic ? 1 : high
                                stepSize: logarithmic ? 0 : rowDelegate.modelData.step === undefined ? 0.05 : rowDelegate.modelData.step
                                value: logarithmic
                                    ? Math.log(Math.max(low, Math.min(high, fieldNumber)) / low) / Math.log(high / low)
                                    : fieldNumber
                                onMoved: {
                                    if (!logarithmic) {
                                        root.studio.setFieldValue(rowDelegate.modelData, value)
                                        return
                                    }
                                    const decimals = rowDelegate.modelData.decimals === undefined ? 2 : rowDelegate.modelData.decimals
                                    const exact = low * Math.pow(high / low, value)
                                    root.studio.setFieldValue(rowDelegate.modelData, Number(exact.toFixed(decimals)))
                                }
                            }

                            Label {
                                Layout.minimumWidth: 52
                                horizontalAlignment: Text.AlignRight
                                text: root.studio.formatFieldValue(rowDelegate.modelData, root.studio.fieldValue(rowDelegate.modelData))
                                color: "#dce9ef"
                            }
                        }

                        TextField {
                            objectName: "studio-text-" + String(rowDelegate.modelData.key || "")
                            Accessible.name: rowDelegate.modelData.label || ""
                            visible: rowDelegate.kind === "text"
                            Layout.preferredWidth: 270
                            placeholderText: rowDelegate.modelData.placeholder || ""
                            text: String(root.studio.fieldValue(rowDelegate.modelData))
                            onEditingFinished: root.studio.setFieldValue(rowDelegate.modelData, text)
                        }

                        RowLayout {
                            visible: rowDelegate.kind === "color"
                            Layout.preferredWidth: 270
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 28
                                Layout.preferredHeight: 28
                                radius: 4
                                color: root.studio.colorPreviewValue(rowDelegate.modelData)
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
                                Accessible.name: qsTr("Choose %1").arg(rowDelegate.modelData.label || qsTr("color"))
                                Layout.fillWidth: true
                                text: root.studio.colorDisplayValue(rowDelegate.modelData)
                                icon.name: "color-picker"
                                onClicked: root.studio.openColorEditor(rowDelegate.modelData)
                            }

                            ToolButton {
                                Accessible.name: qsTr("Use theme default for %1").arg(rowDelegate.modelData.label || qsTr("color"))
                                enabled: String(root.studio.fieldValue(rowDelegate.modelData)).trim().length > 0
                                icon.name: "edit-clear"
                                onClicked: root.studio.setFieldValue(rowDelegate.modelData, "")

                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Use theme default")
                            }
                        }

                        Button {
                            visible: rowDelegate.kind === "action"
                            text: rowDelegate.modelData.buttonText || rowDelegate.modelData.label
                            icon.name: rowDelegate.modelData.icon || ""
                            onClicked: root.studio.performStudioAction(rowDelegate.modelData.action, rowDelegate.modelData)
                        }

                        RowLayout {
                            visible: rowDelegate.kind === "actions"
                            spacing: 8

                            Repeater {
                                model: rowDelegate.modelData.actions || []

                                delegate: Button {
                                    required property var modelData

                                    visible: modelData.available === undefined || modelData.available
                                    text: modelData.label || ""
                                    icon.name: modelData.icon || ""
                                    onClicked: root.studio.performStudioAction(modelData.action, modelData)
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
