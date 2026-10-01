pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var auditionStatus: ({ state: "IDLE" })
    property string presetId: ""
    property bool resourceAvailable: false
    property bool selectedDefault: false
    property string guardError: ""
    readonly property string sessionState: String(auditionStatus.state || "IDLE")
    readonly property bool active: sessionState === "ACTIVE"
    readonly property bool blocked: sessionState === "BLOCKED"
    readonly property bool idle: sessionState === "IDLE"
    readonly property bool canRestore: active && guardError.length === 0

    signal actionRequested(string action, string name)

    function explanation() {
        if (blocked)
            return qsTr("Preview recovery is blocked. Cancel retries restoration (%1).")
                .arg(String(auditionStatus.errorCode || "host-unavailable"));
        if (guardError === "edit-mode-active")
            return qsTr("Leave Plasma Edit Mode before previewing or applying.");
        if (guardError === "popup-open")
            return qsTr("Close the panel popup before previewing or applying.");
        if (guardError === "drag-active")
            return qsTr("Finish the drag before previewing or applying.");
        if (active)
            return auditionStatus.temporary === true
                ? qsTr("Desktop preview active on a temporary panel. Apply keeps it; Cancel removes it.")
                : qsTr("Desktop preview active. Your saved panel stays unchanged until Apply.");
        if (!idle)
            return qsTr("Desktop preview: %1").arg(sessionState.toLowerCase().replace(/_/g, " "));
        return qsTr("Preview on Desktop to customize, apply or save a preset.");
    }

    objectName: "preset-audition-bar"
    implicitHeight: body.implicitHeight + 16
    radius: 7
    color: "#22394a"
    border.color: blocked || guardError.length > 0 ? "#ffc66d" : "#5e73cfe7"
    Accessible.role: Accessible.Grouping
    Accessible.name: qsTr("Desktop preset audition")

    Keys.onEscapePressed: function(event) {
        if (root.active || root.blocked) {
            root.actionRequested("cancel", "");
            event.accepted = true;
        }
    }

    ColumnLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        spacing: 6

        Label {
            objectName: "preset-audition-status"
            Layout.fillWidth: true
            text: root.explanation()
            color: root.blocked || root.guardError.length > 0 ? "#ffc66d" : "#d8e5ec"
            wrapMode: Text.Wrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
        Label {
            objectName: "preset-audition-visibility"
            visible: root.active
            Layout.fillWidth: true
            text: qsTr("Visibility changes take effect on Apply.")
            color: "#90a7b4"
            wrapMode: Text.Wrap
        }
        Label {
            objectName: "preset-audition-fallback"
            visible: root.active && (root.auditionStatus.fallback || {}).fallbackApplied === true
            Layout.fillWidth: true
            text: qsTr("Safe fallback: %1 (%2)")
                .arg(String((root.auditionStatus.fallback || {}).effectiveRendererTier || "available style"))
                .arg(String((root.auditionStatus.fallback || {}).reasonCode || "primary unavailable"))
            color: "#ffc66d"
            wrapMode: Text.Wrap
        }
        Label {
            objectName: "preset-audition-conflict"
            visible: root.active && String(root.auditionStatus.errorCode || "").length > 0
            Layout.fillWidth: true
            text: qsTr("Preview action refused (%1). Cancel restores or reports a recovery conflict.")
                .arg(String(root.auditionStatus.errorCode || ""))
            color: "#ffc66d"
            wrapMode: Text.Wrap
        }
        RowLayout {
            visible: root.active
            Layout.fillWidth: true
            TextField {
                id: nameField
                objectName: "preset-audition-name"
                Layout.fillWidth: true
                maximumLength: 120
                placeholderText: qsTr("Custom preset name")
                Accessible.name: qsTr("Custom preset name")
                onAccepted: {
                    if (text.trim().length > 0) root.actionRequested("save-custom", text.trim());
                }
            }
            Button {
                objectName: "preset-audition-save"
                text: qsTr("Save as Custom Preset")
                enabled: root.active && nameField.text.trim().length > 0
                Accessible.name: text
                onClicked: root.actionRequested("save-custom", nameField.text.trim())
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Button {
                objectName: "preset-audition-apply"
                text: qsTr("Apply as Active")
                enabled: root.canRestore
                Accessible.name: text
                onClicked: root.actionRequested("apply", "")
            }
            Button {
                objectName: "preset-audition-default"
                text: root.selectedDefault ? qsTr("Remove as Default") : qsTr("Set as Default")
                enabled: (root.idle || root.active) && root.resourceAvailable && root.presetId.length > 0
                Accessible.name: text
                onClicked: root.actionRequested(root.selectedDefault ? "remove-default" : "set-default", "")
            }
            Button {
                objectName: "preset-audition-cancel"
                text: qsTr("Cancel")
                enabled: root.active || root.blocked
                Accessible.name: qsTr("Cancel desktop preview")
                onClicked: root.actionRequested("cancel", "")
            }
            Button {
                objectName: "preset-audition-revert"
                text: qsTr("Revert")
                enabled: root.active || root.blocked
                Accessible.name: qsTr("Revert desktop preview")
                onClicked: root.actionRequested("revert", "")
            }
            Button {
                objectName: "preset-audition-restore"
                text: qsTr("Restore Built-in Defaults")
                enabled: root.canRestore
                Accessible.name: text
                onClicked: root.actionRequested("restore-built-in", "")
            }
        }
    }
}
