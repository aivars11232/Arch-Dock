pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ConfigPageBase {
    id: root

    property var visibilityResult: ({})
    readonly property var capabilityRenderer:
        capabilityResolution.renderer || ({})
    readonly property string effectiveRendererTier:
        String(capabilityRenderer.effectiveTier || "")
    readonly property string capabilityReasonCode:
        String(capabilityResolution.reasonCode || "invalid-capability-result")

    function optionIndex(options, value) {
        const source = options || []
        for (let index = 0; index < source.length; ++index) {
            if (source[index].value === value)
                return index
        }
        return 0
    }

    function refreshVisibility() {
        if (panelId.length === 0)
            return
        callDock("nativePanelVisibilityStatus", [panelId], function(reply) {
            if (reply && typeof reply === "object")
                root.visibilityResult = reply
        })
    }

    function transactionDetail() {
        if (applyInFlight)
            return qsTr("Waiting for the Arch Dock transaction and saved revision.")
        if (draftStatus === "conflict")
            return qsTr("The panel changed elsewhere. Cancel and reopen this page.")
        if (draftStatus === "failed") {
            return String(draftErrorMessage || draftErrorCode
                || qsTr("The transaction failed."))
        }
        if (draftDirty)
            return qsTr("Pending changes have not been written.")
        return qsTr("Loaded revision %1.").arg(
            Number(editorSnapshot.revision || 0))
    }

    Kirigami.FormLayout {
        Kirigami.InlineMessage {
            visible: Object.keys(root.capabilityResolution).length > 0
            Kirigami.FormData.label: qsTr("Renderer capability")
            text: root.effectiveRendererTier.length > 0
                ? qsTr("Backend-selected renderer: %1")
                    .arg(root.effectiveRendererTier)
                : qsTr("No renderer is available: %1")
                    .arg(root.capabilityReasonCode)
            type: root.capabilityResolution.available === true
                ? Kirigami.MessageType.Information
                : Kirigami.MessageType.Warning
        }

        Repeater {
            model: root.visibleFields

            delegate: Item {
                id: fieldEditor

                required property var modelData
                readonly property var field: modelData
                readonly property var options: field.options || []

                Kirigami.FormData.label: String(field.label || "")
                implicitWidth: Math.max(fieldSwitch.implicitWidth,
                                        fieldCombo.implicitWidth)
                implicitHeight: Math.max(fieldSwitch.implicitHeight,
                                         fieldCombo.implicitHeight)

                QQC2.CheckBox {
                    id: fieldSwitch

                    visible: String(fieldEditor.field.control) === "switch"
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: String(fieldEditor.field.label || "")
                    checked: Boolean(root.valueFor(fieldEditor.field))
                    enabled: !root.applyInFlight
                    onToggled: root.setValue(
                        String(fieldEditor.field.key), checked)
                }

                QQC2.ComboBox {
                    id: fieldCombo

                    visible: ["combo", "screen"].includes(
                        String(fieldEditor.field.control))
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    model: fieldEditor.options
                    textRole: "label"
                    valueRole: "value"
                    currentIndex: root.optionIndex(
                        fieldEditor.options, root.valueFor(fieldEditor.field))
                    enabled: !root.applyInFlight
                    onActivated: root.setValue(
                        String(fieldEditor.field.key), currentValue)
                }
            }
        }

        Kirigami.InlineMessage {
            visible: Object.keys(root.visibilityResult).length > 0
                && (root.visibilityResult.fallbackApplied === true
                    || (String(root.visibilityResult.status || "").length > 0
                        && root.visibilityResult.success !== true))
            Kirigami.FormData.label: qsTr("Native host result")
            text: String(root.visibilityResult.errorCode
                || root.visibilityResult.fallbackReason
                || root.visibilityResult.status)
            type: root.visibilityResult.fallbackApplied === true
                ? Kirigami.MessageType.Warning
                : Kirigami.MessageType.Error
        }

        Kirigami.InlineMessage {
            visible: root.draftDirty || root.applyInFlight
                || root.draftStatus === "failed"
                || root.draftStatus === "conflict"
            Kirigami.FormData.label: qsTr("Arch Dock draft")
            text: root.transactionDetail()
            type: root.draftStatus === "failed"
                || root.draftStatus === "conflict"
                ? Kirigami.MessageType.Error
                : Kirigami.MessageType.Information
        }

        RowLayout {
            Kirigami.FormData.label: qsTr("Arch Dock settings")

            QQC2.Button {
                text: root.applyInFlight ? qsTr("Applying…") : qsTr("Apply")
                icon.name: "dialog-ok-apply"
                enabled: root.draftDirty && !root.applyInFlight
                onClicked: root.applyDraft()
            }

            QQC2.Button {
                text: qsTr("Cancel")
                icon.name: "dialog-cancel"
                enabled: root.draftDirty && !root.applyInFlight
                onClicked: root.cancelDraft()
            }
        }

        QQC2.Label {
            Kirigami.FormData.isSection: true
            text: qsTr("Only the local Apply button commits Arch Dock settings. Closing this page, Cancel, or the outer Plasma Apply/OK leaves a pending local draft uncommitted.")
            wrapMode: Text.WordWrap
        }
    }

    onDraftStatusChanged: {
        if (draftStatus === "loaded")
            refreshVisibility()
    }
    Component.onCompleted: refreshVisibility()
}
