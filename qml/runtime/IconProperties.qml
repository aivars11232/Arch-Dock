import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "IconPropertiesDraft.js" as IconPropertiesDraft

Item {
    id: root

    property var editorSnapshot: ({})
    property var transactionController: null
    property string customGlyph: ""
    property string customLabel: ""
    property string tileMode: "panel-default"
    property string styleReference: ""
    property bool applyInFlight: false
    property string draftStatus: "unavailable"
    property string draftErrorCode: ""
    property string draftErrorMessage: ""
    property var lastTransaction: ({})
    property int transactionRequestCount: 0

    readonly property bool snapshotLoaded: editorSnapshot
        && editorSnapshot.success === true
        && String(editorSnapshot.status || "") === "loaded"
        && String(editorSnapshot.panelId || "").length > 0
        && String(editorSnapshot.entryIdentity || "").length > 0
    readonly property var styleChoices: IconPropertiesDraft.styleChoices(
        editorSnapshot && editorSnapshot.iconStyles
            ? editorSnapshot.iconStyles : [])
    readonly property var currentOverride: IconPropertiesDraft.overrideFromEditor(
        editorSnapshot, {
            customGlyph: customGlyph,
            customLabel: customLabel,
            tileMode: tileMode,
            styleReference: styleReference
        })
    readonly property bool draftDirty: snapshotLoaded
        && IconPropertiesDraft.isDirty(editorSnapshot, {
            customGlyph: customGlyph,
            customLabel: customLabel,
            tileMode: tileMode,
            styleReference: styleReference
        })
    readonly property bool hasStoredOverride: snapshotLoaded
        && !IconPropertiesDraft.isEmpty(
            IconPropertiesDraft.snapshotOverride(editorSnapshot))
    readonly property bool canApply: snapshotLoaded && draftDirty
        && !applyInFlight && transactionController !== null
    readonly property bool canReset: snapshotLoaded && hasStoredOverride
        && !applyInFlight && transactionController !== null
    readonly property string previewGlyph: customGlyph.length > 0
        ? customGlyph : String(editorSnapshot.baseGlyph || "application-x-executable")
    readonly property string previewLabel: customLabel.length > 0
        ? customLabel : String(editorSnapshot.baseLabel || "")

    signal closeRequested(string reason)

    function acceptSnapshot(snapshot) {
        const values = IconPropertiesDraft.editorValues(snapshot)
        customGlyph = values.customGlyph
        customLabel = values.customLabel
        tileMode = values.tileMode
        styleReference = values.styleReference
        applyInFlight = false
        lastTransaction = ({})
        draftErrorCode = ""
        draftErrorMessage = ""
        draftStatus = snapshotLoaded ? "loaded" : "unavailable"
    }

    function setCustomGlyph(value) {
        if (!snapshotLoaded || applyInFlight)
            return false
        customGlyph = String(value === undefined || value === null ? "" : value)
        return true
    }

    function setCustomLabel(value) {
        if (!snapshotLoaded || applyInFlight)
            return false
        customLabel = String(value === undefined || value === null ? "" : value)
        return true
    }

    function setTileMode(value) {
        const normalized = String(value || "")
        if (!snapshotLoaded || applyInFlight
                || !["panel-default", "enabled", "disabled"].includes(normalized))
            return false
        tileMode = normalized
        return true
    }

    function setStyleReference(value) {
        const normalized = String(value === undefined || value === null
                                  ? "" : value).trim()
        let available = normalized.length === 0
        for (let index = 0; index < styleChoices.length && !available; ++index)
            available = String(styleChoices[index].id || "") === normalized
        if (!snapshotLoaded || applyInFlight || !available)
            return false
        styleReference = normalized
        return true
    }

    function finishTransaction(result, closeReason) {
        applyInFlight = false
        lastTransaction = result || ({})
        if (IconPropertiesDraft.transactionSucceeded(result)) {
            draftStatus = "succeeded"
            draftErrorCode = ""
            draftErrorMessage = ""
            closeRequested(closeReason)
            return
        }
        draftStatus = IconPropertiesDraft.transactionConflict(result)
            ? "conflict" : "failed"
        draftErrorCode = String(result && result.errorCode
                                ? result.errorCode : "transaction-failed")
        draftErrorMessage = String(result && result.errorMessage
                                   ? result.errorMessage
                                   : qsTr("The icon override could not be saved."))
    }

    function applyDraft() {
        if (!canApply)
            return false
        applyInFlight = true
        draftStatus = "applying"
        draftErrorCode = ""
        draftErrorMessage = ""
        transactionRequestCount += 1
        let result
        try {
            if (IconPropertiesDraft.isEmpty(currentOverride)) {
                result = transactionController.resetIconOverrideTransaction(
                    String(editorSnapshot.panelId),
                    Number(editorSnapshot.revision),
                    String(editorSnapshot.entryIdentity))
            } else {
                result = transactionController.applyIconOverrideTransaction(
                    String(editorSnapshot.panelId),
                    Number(editorSnapshot.revision),
                    String(editorSnapshot.entryIdentity),
                    currentOverride)
            }
        } catch (error) {
            result = {
                success: false,
                status: "persistence-failed",
                errorCode: "transaction-exception",
                errorMessage: String(error)
            }
        }
        finishTransaction(result, "apply")
        return true
    }

    function resetOverride() {
        if (!canReset)
            return false
        applyInFlight = true
        draftStatus = "resetting"
        draftErrorCode = ""
        draftErrorMessage = ""
        transactionRequestCount += 1
        let result
        try {
            result = transactionController.resetIconOverrideTransaction(
                String(editorSnapshot.panelId),
                Number(editorSnapshot.revision),
                String(editorSnapshot.entryIdentity))
        } catch (error) {
            result = {
                success: false,
                status: "persistence-failed",
                errorCode: "transaction-exception",
                errorMessage: String(error)
            }
        }
        finishTransaction(result, "reset")
        return true
    }

    function discardDraft() {
        if (applyInFlight)
            return false
        acceptSnapshot(editorSnapshot)
        draftStatus = "cancelled"
        return true
    }

    function cancelDraft() {
        if (!discardDraft())
            return false
        closeRequested("cancel")
        return true
    }

    onEditorSnapshotChanged: acceptSnapshot(editorSnapshot)
    Component.onCompleted: acceptSnapshot(editorSnapshot)

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        Label {
            text: qsTr("Icon Properties")
            color: "#f4f8fb"
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }

        Label {
            Layout.fillWidth: true
            text: root.previewLabel
            color: "#a9bfcb"
            elide: Text.ElideRight
            font.pixelSize: 13
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            spacing: Kirigami.Units.largeSpacing

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
                    source: root.previewGlyph
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("The original application glyph remains the safe fallback if a custom file is unavailable.")
                wrapMode: Text.WordWrap
                color: "#cbd8e2"
            }
        }

        Label {
            text: qsTr("Custom glyph")
            color: "#cbd8e2"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            TextField {
                id: glyphField
                objectName: "customGlyphField"

                Layout.fillWidth: true
                text: root.customGlyph
                enabled: root.snapshotLoaded && !root.applyInFlight
                placeholderText: qsTr("Panel default glyph")
                selectByMouse: true
                maximumLength: 4096
                onTextEdited: root.customGlyph = text
            }

            Button {
                objectName: "chooseGlyphButton"
                icon.name: "document-open"
                text: qsTr("Choose image")
                enabled: root.snapshotLoaded && !root.applyInFlight
                onClicked: imageDialog.open()
            }
        }

        Label {
            text: qsTr("Custom label")
            color: "#cbd8e2"
        }

        TextField {
            id: labelField
            objectName: "customLabelField"

            Layout.fillWidth: true
            text: root.customLabel
            enabled: root.snapshotLoaded && !root.applyInFlight
            placeholderText: qsTr("Panel default label")
            selectByMouse: true
            maximumLength: 1024
            onTextEdited: root.customLabel = text
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            ColumnLayout {
                Layout.fillWidth: true

                Label {
                    text: qsTr("Tile")
                    color: "#cbd8e2"
                }

                ComboBox {
                    id: tileModeCombo
                    objectName: "tileModeCombo"

                    Layout.fillWidth: true
                    enabled: root.snapshotLoaded && !root.applyInFlight
                    model: [
                        { label: qsTr("Use panel setting"), value: "panel-default" },
                        { label: qsTr("Enabled"), value: "enabled" },
                        { label: qsTr("Disabled"), value: "disabled" }
                    ]
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.tileMode === "enabled" ? 1
                        : root.tileMode === "disabled" ? 2 : 0
                    onActivated: root.tileMode = String(currentValue)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true

                Label {
                    text: qsTr("Icon style")
                    color: "#cbd8e2"
                }

                ComboBox {
                    id: styleCombo
                    objectName: "styleCombo"

                    Layout.fillWidth: true
                    enabled: root.snapshotLoaded && !root.applyInFlight
                    model: root.styleChoices
                    textRole: "name"
                    valueRole: "id"
                    currentIndex: IconPropertiesDraft.styleIndex(
                        root.styleChoices, root.styleReference)
                    onActivated: root.styleReference = String(currentValue)
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.draftErrorMessage.length > 0
            text: root.draftErrorMessage
            color: "#ff8a9b"
            wrapMode: Text.WordWrap
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                objectName: "resetButton"
                text: qsTr("Reset")
                enabled: root.canReset
                onClicked: root.resetOverride()
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                objectName: "cancelButton"
                text: qsTr("Cancel")
                enabled: !root.applyInFlight
                onClicked: root.cancelDraft()
            }

            Button {
                objectName: "applyButton"
                text: qsTr("Apply")
                enabled: root.canApply
                onClicked: root.applyDraft()
            }
        }
    }

    FileDialog {
        id: imageDialog

        title: qsTr("Choose custom icon")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Image files (*.png *.svg *.svgz *.jpg *.jpeg *.webp)")]
        onAccepted: root.customGlyph = selectedFile.toString()
    }
}
