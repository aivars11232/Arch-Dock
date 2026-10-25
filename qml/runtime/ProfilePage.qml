pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import "StudioThemes.js" as StudioThemes

// Panel Studio's Profiles page: the saved sets of panels, with create,
// rename, duplicate, delete, import, export and apply, the panels each one
// holds (renamed, edited in Panel Studio or removed, in the profile only),
// and the global shortcuts that apply them.
ColumnLayout {
    id: root
    property var profiles: []
    property var applyStatus: ({state: "IDLE"})
    property bool shortcutsOnly: false
    property var shortcutStatus: ({enabled: false, bindings: []})
    property bool actionsBlocked: false
    // The profile service the panel list is read from.
    property var service: null
    // Panel Studio's look.
    property var theme: StudioThemes.theme("classic")
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
    signal panelActionRequested(string action, string profileId, int revision, string panelId, string value)

    function request(action, value) {
        actionRequested(action, selected ? selected.id : "", selected ? selected.revision : 0, value || "");
    }
    function panelRequest(action, panelId, value) {
        panelActionRequested(action, selected ? selected.id : "", selected ? selected.revision : 0,
            String(panelId || ""), value || "");
    }
    // The panels the selected profile shows on the desktop; the hidden
    // built-in edge panels every arrangement keeps are not listed.
    readonly property var selectedPanels: {
        const changed = root.profiles;
        if (!root.service || !root.selected)
            return [];
        return Array.from(root.service.profilePanels(root.selected.id) || [])
            .filter(function(panel) { return panel.shown === true; });
    }
    function panelPlace(panel) {
        const edge = String(panel.edge || "");
        return edge === "free" ? qsTr("Free panel")
            : edge === "top" ? qsTr("Top edge")
            : edge === "bottom" ? qsTr("Bottom edge")
            : edge === "left" ? qsTr("Left edge")
            : edge === "right" ? qsTr("Right edge") : edge;
    }

    Label {
        Layout.fillWidth: true
        color: root.theme.text
        text: root.shortcutsOnly
            ? qsTr("Enable profile shortcuts to apply saved arrangements through KDE. Conflicting shortcuts stay with their current owner.")
            : qsTr("A profile saves your complete managed panel arrangement. Apply replaces that arrangement after a verified backup.")
        wrapMode: Text.Wrap
    }
    Label {
        visible: root.actionsBlocked
        Layout.fillWidth: true
        color: root.theme.textSecondary
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
        objectName: "profile-summary"
        Layout.fillWidth: true
        color: root.theme.textSecondary
        wrapMode: Text.Wrap
        text: {
            if (!root.selected) return qsTr("Create a profile from your current panels, or import one.");
            // What is shown on the desktop: the hidden built-in edge panels
            // every arrangement keeps are not counted.
            const shown = root.selected.shownPanelCount === undefined
                ? Number(root.selected.panelCount) : Number(root.selected.shownPanelCount);
            let summary = (shown === 1 ? qsTr("1 panel") : qsTr("%1 panels").arg(shown))
                + qsTr(" · revision %1").arg(root.selected.revision);
            const creating = Number(root.selected.freePanelsToCreate);
            if (creating === 1) summary += qsTr(" · Apply creates 1 free panel on the desktop");
            else if (creating > 1) summary += qsTr(" · Apply creates %1 free panels on the desktop").arg(creating);
            return summary;
        }
    }
    Label {
        // Once one Apply created 13 free panels from a
        // profile that had captured every dead record. The backend refuses
        // more than its limit; this says so before the click.
        objectName: "profile-free-panel-notice"
        readonly property int creating: root.selected ? Number(root.selected.freePanelsToCreate) : 0
        readonly property int limit: root.selected && !isNaN(Number(root.selected.freePanelLimit)) ? Number(root.selected.freePanelLimit) : 4
        visible: creating > limit
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        color: root.theme.warning
        text: qsTr("This profile holds %1 free panels that are not on the desktop. Apply Profile creates at most %2 at once. Put only the panels you want on the desktop, then use Save Current to Profile: the profile then holds those panels only.").arg(creating).arg(limit)
        Accessible.name: text
    }
    Label {
        objectName: "profile-panels-heading"
        visible: !root.shortcutsOnly && root.selected !== null && root.selectedPanels.length > 0
        Layout.fillWidth: true
        Layout.topMargin: 4
        text: qsTr("Panels in this profile")
        color: root.theme.text
        font.weight: Font.DemiBold
    }
    Repeater {
        model: root.shortcutsOnly ? [] : root.selectedPanels
        delegate: RowLayout {
            id: panelRow
            required property var modelData
            Layout.fillWidth: true
            spacing: 8

            TextField {
                objectName: "profile-panel-name-" + panelRow.modelData.id
                Layout.fillWidth: true
                text: panelRow.modelData.name
                maximumLength: 120
                enabled: root.canManage
                Accessible.name: qsTr("Name of %1 in this profile").arg(panelRow.modelData.name)
                onEditingFinished: {
                    const name = text.trim();
                    if (name.length > 0 && name !== panelRow.modelData.name)
                        root.panelRequest("rename", panelRow.modelData.id, name);
                    else
                        text = panelRow.modelData.name;
                }
            }
            Label {
                text: root.panelPlace(panelRow.modelData)
                color: root.theme.textSecondary
            }
            Button {
                objectName: "profile-panel-edit-" + panelRow.modelData.id
                text: qsTr("Edit…")
                icon.name: "document-edit"
                enabled: root.canManage
                onClicked: root.panelRequest("edit", panelRow.modelData.id)
                Accessible.name: qsTr("Edit %1 in this profile").arg(panelRow.modelData.name)
            }
            Button {
                objectName: "profile-panel-remove-" + panelRow.modelData.id
                text: panelRow.modelData.builtIn ? qsTr("Hide") : qsTr("Remove")
                icon.name: "list-remove"
                enabled: root.canManage
                onClicked: {
                    removeQuestion.panel = panelRow.modelData;
                    removeQuestion.open();
                }
                Accessible.name: qsTr("%1 %2 in this profile").arg(text).arg(panelRow.modelData.name)
            }
        }
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
    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: root.theme.textSecondary
        // A saved arrangement must reach the desktop
        // even when its free panel is gone.
        text: qsTr("A profile holds the panels that are on the desktop when it is saved. Apply Profile puts the whole saved arrangement back: a free panel the profile holds that is not on the desktop is created again with its saved look. For one saved panel look, use Panels > Presets with \"Preview on a new panel\".")
        Accessible.name: text
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
        color: root.theme.textSecondary
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
        color: root.theme.warning
        wrapMode: Text.Wrap
    }
    Label {
        objectName: "profile-notice"
        Layout.fillWidth: true
        visible: text.length > 0
        text: root.notice
        color: root.noticeIsError ? root.theme.warning : root.theme.textSecondary
        wrapMode: Text.Wrap
        Accessible.name: text
    }
    Label {
        objectName: "profile-recovery-status"
        Layout.fillWidth: true
        color: root.theme.textSecondary
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

    MessageDialog {
        id: removeQuestion
        objectName: "profile-panel-remove-question"
        property var panel: null
        title: panel && panel.builtIn ? qsTr("Hide in the profile") : qsTr("Remove from the profile")
        text: !panel || !root.selected ? ""
            : panel.builtIn ? qsTr("Hide “%1” in the profile “%2”?").arg(panel.name).arg(root.selected.name)
            : qsTr("Remove “%1” from the profile “%2”?").arg(panel.name).arg(root.selected.name)
        informativeText: panel && panel.builtIn
            ? qsTr("Applying the profile then hides this edge panel. The panels on your desktop do not change now.")
            : qsTr("Applying the profile then leaves this panel out. The panels on your desktop do not change now.")
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onButtonClicked: function(button, role) {
            if (button === MessageDialog.Ok && panel)
                root.panelRequest("remove", panel.id);
            panel = null;
        }
    }
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
