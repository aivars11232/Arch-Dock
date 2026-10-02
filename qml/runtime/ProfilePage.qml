pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ColumnLayout {
    id: root
    property var profiles: []
    property var applyStatus: ({state: "IDLE"})
    property bool shortcutsOnly: false
    property var shortcutStatus: ({enabled: false, bindings: []})
    property bool actionsBlocked: false
    property string notice: ""
    property bool noticeIsError: false
    property string selectedId: ""
    readonly property var selected: {
        for (const profile of profiles)
            if (profile.id === selectedId) return profile;
        return profiles.length ? profiles[0] : null;
    }
    readonly property bool busy: ["IDLE", "APPLIED"].indexOf(String(applyStatus.state || "IDLE")) < 0
    readonly property bool canManage: !actionsBlocked && !busy
    readonly property var selectedBinding: {
        for (const binding of (shortcutStatus.bindings || []))
            if (selected && binding.profileId === selected.id) return binding;
        return null;
    }
    signal actionRequested(string action, string profileId, int revision, string value)

    function request(action, value) {
        actionRequested(action, selected ? selected.id : "", selected ? selected.revision : 0, value || "");
    }

    Label {
        Layout.fillWidth: true
        text: root.shortcutsOnly
            ? qsTr("Enable profile shortcuts to apply saved arrangements through KDE. Conflicting shortcuts stay with their current owner.")
            : qsTr("A profile saves your complete managed panel arrangement. Apply replaces that arrangement after a verified backup.")
        wrapMode: Text.Wrap
    }
    Label {
        visible: root.actionsBlocked
        Layout.fillWidth: true
        text: qsTr("Apply or cancel your settings draft and finish any desktop preview before managing profiles.")
        wrapMode: Text.Wrap
    }
    ComboBox {
        objectName: "profile-selection"
        Layout.fillWidth: true
        model: root.profiles
        textRole: "name"
        currentIndex: {
            for (let i = 0; i < root.profiles.length; ++i)
                if (root.selected && root.profiles[i].id === root.selected.id) return i;
            return -1;
        }
        enabled: root.canManage
        onActivated: function(index) { root.selectedId = root.profiles[index].id; }
        Accessible.name: qsTr("Saved profile")
    }
    Label {
        text: root.selected ? qsTr("%1 panels · revision %2").arg(root.selected.panelCount).arg(root.selected.revision)
            : qsTr("Create a profile from your current panels, or import one.")
    }
    TextField {
        id: nameField
        visible: !root.shortcutsOnly
        objectName: "profile-name"
        Layout.fillWidth: true
        maximumLength: 256
        placeholderText: qsTr("Name for a new, renamed or duplicated profile")
        enabled: root.canManage
        Accessible.name: qsTr("Profile name")
    }
    RowLayout {
        visible: !root.shortcutsOnly
        Button {
            objectName: "profile-create"
            text: qsTr("Create from Current")
            enabled: root.canManage && nameField.text.trim().length > 0
            onClicked: root.request("create", nameField.text.trim())
            Accessible.name: text
        }
        Button {
            objectName: "profile-save"
            text: qsTr("Save Current to Profile")
            enabled: root.canManage && root.selected !== null
            onClicked: root.request("save")
            Accessible.name: text
        }
        Button {
            objectName: "profile-apply"
            text: qsTr("Apply Profile")
            enabled: root.canManage && root.selected !== null
            onClicked: root.request("apply")
            Accessible.name: text
        }
    }
    RowLayout {
        visible: !root.shortcutsOnly
        Repeater {
            model: [{action: "rename", label: qsTr("Rename")}, {action: "duplicate", label: qsTr("Duplicate")},
                {action: "delete", label: qsTr("Delete")}]
            delegate: Button {
                required property var modelData
                objectName: "profile-" + modelData.action
                text: modelData.label
                enabled: root.canManage && root.selected !== null
                    && (modelData.action === "delete" || nameField.text.trim().length > 0)
                onClicked: root.request(modelData.action, nameField.text.trim())
                Accessible.name: text
            }
        }
        Button {
            objectName: "profile-import"
            text: qsTr("Import…")
            enabled: root.canManage
            onClicked: importDialog.open()
            Accessible.name: text
        }
        Button {
            objectName: "profile-export"
            text: qsTr("Export…")
            enabled: root.canManage && root.selected !== null
            onClicked: {
                exportDialog.profileId = root.selected.id;
                exportDialog.revision = root.selected.revision;
                exportDialog.open();
            }
            Accessible.name: text
        }
    }
    CheckBox {
        objectName: "profile-shortcuts-enabled"
        visible: root.shortcutsOnly
        enabled: root.canManage
        text: qsTr("Enable Global Profile Shortcuts")
        checked: root.shortcutStatus.enabled === true
        onClicked: root.request("shortcuts-enabled", checked ? "true" : "false")
        Accessible.name: text
    }
    TextField {
        id: sequenceField
        objectName: "profile-shortcut-sequence"
        visible: root.shortcutsOnly
        Layout.fillWidth: true
        enabled: root.canManage && root.selected !== null
        maximumLength: 128
        text: root.selectedBinding ? root.selectedBinding.sequence : ""
        placeholderText: qsTr("Shortcut, for example Ctrl+Alt+F9")
        Accessible.name: qsTr("Profile shortcut")
    }
    RowLayout {
        visible: root.shortcutsOnly
        Button {
            objectName: "profile-shortcut-set"
            text: qsTr("Assign Shortcut")
            enabled: root.canManage && root.selected !== null && sequenceField.text.trim().length > 0
            onClicked: root.request("shortcut-set", sequenceField.text.trim())
            Accessible.name: text
        }
        Button {
            objectName: "profile-shortcut-clear"
            text: qsTr("Remove Shortcut")
            enabled: root.canManage && root.selectedBinding !== null
            onClicked: root.request("shortcut-clear")
            Accessible.name: text
        }
    }
    Label {
        objectName: "profile-shortcut-registration"
        visible: root.shortcutsOnly
        Layout.fillWidth: true
        text: root.selectedBinding && (root.selectedBinding.registeredKeys || []).length
            ? qsTr("Registered: %1").arg(root.selectedBinding.registeredKeys.join(", "))
            : qsTr("No active shortcut for this profile. Saved assignments register only while the feature is enabled.")
        wrapMode: Text.Wrap
    }
    Label {
        objectName: "profile-shortcut-conflict"
        visible: root.shortcutsOnly && String(root.shortcutStatus.errorCode || "").length > 0
        Layout.fillWidth: true
        text: qsTr("Shortcut action failed (%1). %2").arg(String(root.shortcutStatus.errorCode || ""))
            .arg((root.shortcutStatus.conflicts || []).map(function(conflict) { return conflict.name || conflict.actionId || ""; }).join(", "))
        color: "#ffc66d"
        wrapMode: Text.Wrap
    }
    Label {
        objectName: "profile-notice"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.notice
        color: root.noticeIsError ? "#ffc66d" : "#90a7b4"
        wrapMode: Text.Wrap
        Accessible.name: text
    }
    Label {
        objectName: "profile-recovery-status"
        Layout.fillWidth: true
        visible: root.busy
        text: root.applyStatus.recoveryRequired
            ? qsTr("Profile recovery is required (%1). Your backup and ownership record have been retained.")
                .arg(String(root.applyStatus.errorCode || "interrupted-apply"))
            : qsTr("Profile transaction: %1").arg(String(root.applyStatus.state || ""))
        wrapMode: Text.Wrap
    }
    Button {
        objectName: "profile-recover"
        visible: root.applyStatus.recoveryRequired === true
        enabled: visible && !root.actionsBlocked
        text: qsTr("Recover Interrupted Apply")
        onClicked: root.request("recover")
        Accessible.name: text
    }
    Item { Layout.fillHeight: true }

    FileDialog {
        id: importDialog
        title: qsTr("Import a Profile")
        nameFilters: [qsTr("Profiles (*.json)")]
        onAccepted: root.request("import", String(selectedFile))
    }
    FileDialog {
        id: exportDialog
        property string profileId: ""
        property int revision: 0
        title: qsTr("Export a Profile to a New File")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("Profiles (*.json)")]
        onAccepted: root.actionRequested("export", profileId, revision, String(selectedFile))
    }
}
