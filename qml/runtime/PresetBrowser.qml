pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// One preset catalog page. Every piece of data arrives through a property and
// every request leaves through a signal, so one component serves the four
// preset pages and none of them can reach a panel or the desktop.
Item {
    id: root

    // "panel" or "icon". The two catalogs are separate and never mixed.
    property string kind: "panel"
    // "builtin" or "user".
    property string scope: "builtin"
    property var presets: []
    property var catalogStatus: ({ valid: true, errorCode: "" })
    property string selectedPresetId: ""
    // The outcome of the last store action, and whether it failed.
    property string noticeText: ""
    property bool noticeIsError: false
    // The rename or delete the user has started but not yet confirmed.
    property var pendingAction: null
    property var auditionStatus: ({ state: "IDLE" })
    property bool actionsEnabled: true
    property bool showAuditionBar: true
    property bool selectedDefault: false
    property string auditionGuardError: ""

    readonly property bool panelKind: kind === "panel"
    readonly property bool builtInScope: scope === "builtin"
    readonly property bool catalogValid: catalogStatus && catalogStatus.valid === true
    readonly property alias listView: list
    readonly property string title: builtInScope
        ? (panelKind ? qsTr("Built-in Panel Presets") : qsTr("Built-in Icon Presets"))
        : (panelKind ? qsTr("My Panel Presets") : qsTr("My Icon Presets"))
    readonly property string description: builtInScope
        ? (panelKind
            ? qsTr("Complete one-panel starting configurations installed with Arch Dock. They are read-only: duplicate one to make it yours.")
            : qsTr("Reusable icon appearance and motion sets installed with Arch Dock. They are read-only: duplicate one to make it yours."))
        : (panelKind
            ? qsTr("Your own Panel Presets. Each is a complete snapshot and stays usable whatever happens to the preset it came from.")
            : qsTr("Your own Icon Presets. Each is a complete snapshot and stays usable whatever happens to the preset it came from."))

    signal presetSelected(string presetId)
    signal duplicateRequested(string presetId, string name)
    signal renameRequested(string presetId, string name)
    signal removeRequested(string presetId)
    signal previewRequested(string presetId)
    signal applyRequested(string presetId)
    signal auditionActionRequested(string action, string name)

    function selectedResourceAvailable() {
        for (let index = 0; index < presets.length; ++index) {
            const preset = presets[index];
            if (String(preset.id || "") === selectedPresetId)
                return (preset.compatibility || {}).available === true;
        }
        return false;
    }

    function beginRename(preset) {
        pendingAction = {
            type: "rename",
            presetId: String(preset.id || ""),
            name: String(preset.name || "")
        };
        renameField.text = pendingAction.name;
        renameField.selectAll();
        renameField.forceActiveFocus();
    }

    function beginRemove(preset) {
        pendingAction = {
            type: "remove",
            presetId: String(preset.id || ""),
            name: String(preset.name || "")
        };
        cancelButton.forceActiveFocus();
    }

    function confirmPendingAction() {
        const action = pendingAction;
        if (!action)
            return;
        if (action.type === "rename") {
            const name = renameField.text.trim();
            if (name.length === 0)
                return;
            pendingAction = null;
            renameRequested(action.presetId, name);
        } else {
            pendingAction = null;
            removeRequested(action.presetId);
        }
    }

    function cancelPendingAction() {
        pendingAction = null;
    }

    function focusCard(index) {
        if (index < 0 || index >= list.count)
            return;
        list.currentIndex = index;
        list.positionViewAtIndex(index, ListView.Contain);
        const card = list.itemAtIndex(index);
        if (card)
            card.forceActiveFocus();
    }

    // A pending rename or delete names a preset on this page only.
    onPresetsChanged: pendingAction = null

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                objectName: "preset-browser-title"
                text: root.title
                color: "#f3f8fb"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                Accessible.role: Accessible.Heading
            }

            Label {
                Layout.fillWidth: true
                text: root.description
                color: "#91a8b5"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Selecting a preset shows it in the preview above. It does not change any panel.")
                color: "#728995"
                font.pixelSize: 10
                wrapMode: Text.Wrap
            }
        }

        Kirigami.InlineMessage {
            objectName: "preset-catalog-error"
            visible: !root.catalogValid
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            text: qsTr("The preset catalog could not be loaded (%1). No preset is offered until it is repaired.")
                .arg(String(root.catalogStatus && root.catalogStatus.errorCode
                    ? root.catalogStatus.errorCode : "unknown-error"))
        }

        Kirigami.InlineMessage {
            objectName: "preset-notice"
            visible: root.noticeText.length > 0
            Layout.fillWidth: true
            type: root.noticeIsError ? Kirigami.MessageType.Warning
                                     : Kirigami.MessageType.Positive
            text: root.noticeText
        }

        PresetAuditionBar {
            visible: root.showAuditionBar && (root.selectedPresetId.length > 0
                || String(root.auditionStatus.state || "IDLE") !== "IDLE")
            Layout.fillWidth: true
            auditionStatus: root.auditionStatus
            presetId: root.selectedPresetId
            resourceAvailable: root.selectedResourceAvailable()
            selectedDefault: root.selectedDefault
            guardError: root.auditionGuardError
            onActionRequested: function(action, name) { root.auditionActionRequested(action, name); }
        }

        Rectangle {
            objectName: "preset-pending-action"
            visible: root.pendingAction !== null
            Layout.fillWidth: true
            implicitHeight: pendingRow.implicitHeight + 16
            radius: 7
            color: "#22394a"
            border.width: 1
            border.color: "#5e73cfe7"

            RowLayout {
                id: pendingRow

                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Label {
                    Layout.fillWidth: !renameField.visible
                    text: !root.pendingAction ? ""
                        : root.pendingAction.type === "rename"
                            ? qsTr("Rename “%1” to").arg(root.pendingAction.name)
                            : qsTr("Delete “%1”? This cannot be undone.")
                                .arg(root.pendingAction.name)
                    color: "#d8e5ec"
                    wrapMode: Text.Wrap
                }

                TextField {
                    id: renameField

                    objectName: "preset-rename-field"
                    visible: root.pendingAction !== null
                        && root.pendingAction.type === "rename"
                    Layout.fillWidth: true
                    maximumLength: 120
                    Accessible.name: qsTr("New preset name")
                    onAccepted: root.confirmPendingAction()
                    Keys.onEscapePressed: root.cancelPendingAction()
                }

                Button {
                    objectName: "preset-confirm-action"
                    enabled: !renameField.visible
                        || renameField.text.trim().length > 0
                    text: root.pendingAction && root.pendingAction.type === "remove"
                        ? qsTr("Delete") : qsTr("Rename")
                    icon.name: root.pendingAction && root.pendingAction.type === "remove"
                        ? "edit-delete" : "dialog-ok-apply"
                    onClicked: root.confirmPendingAction()
                }

                Button {
                    id: cancelButton

                    objectName: "preset-cancel-action"
                    text: qsTr("Cancel")
                    icon.name: "dialog-cancel"
                    onClicked: root.cancelPendingAction()
                }
            }
        }

        Label {
            objectName: "preset-empty-state"
            visible: root.catalogValid && list.count === 0
            Layout.fillWidth: true
            Layout.topMargin: 12
            horizontalAlignment: Text.AlignHCenter
            text: root.builtInScope
                ? qsTr("This catalog has no presets.")
                : (root.panelKind
                    ? qsTr("You have no Panel Presets yet. Open Built-in Panel Presets and choose Duplicate to My Presets.")
                    : qsTr("You have no Icon Presets yet. Open Built-in Icon Presets and choose Duplicate to My Presets."))
            color: "#8195a1"
            wrapMode: Text.Wrap
        }

        ListView {
            id: list

            objectName: "preset-list"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            // Only the cards near the viewport exist, so a long catalog does
            // not keep one renderer scene per preset alive.
            cacheBuffer: 240
            model: root.catalogValid ? root.presets : []
            boundsBehavior: Flickable.StopAtBounds
            Accessible.role: Accessible.List
            Accessible.name: root.title

            ScrollBar.vertical: ScrollBar {}

            delegate: PresetCard {
                id: card

                required property var modelData
                required property int index

                width: ListView.view.width - 12
                preset: modelData
                actionsEnabled: root.actionsEnabled
                selected: presetId.length > 0 && presetId === root.selectedPresetId
                onSelectRequested: {
                    list.currentIndex = index;
                    root.presetSelected(presetId);
                }
                onDuplicateRequested: root.duplicateRequested(
                    presetId, qsTr("%1 copy").arg(String(modelData.name || "")))
                onRenameRequested: root.beginRename(modelData)
                onRemoveRequested: root.beginRemove(modelData)
                onPreviewRequested: root.previewRequested(presetId)
                onApplyRequested: root.applyRequested(presetId)
                Keys.onDownPressed: root.focusCard(index + 1)
                Keys.onUpPressed: root.focusCard(index - 1)
            }
        }
    }
}
