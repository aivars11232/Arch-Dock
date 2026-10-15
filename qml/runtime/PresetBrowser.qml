pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "StudioThemes.js" as StudioThemes

// One preset catalog page. Every piece of data arrives through a property and
// every request leaves through a signal, so one component serves the preset
// pages and none of them can reach a panel or the desktop. Panels > Presets
// lists the panel presets, built-in and the user's own, and the built-in
// looks, each drawn as a bare panel with no icons.
Item {
    id: root
    readonly property alias scrollFlickable: list

    // "panel" or "icon". The two catalogs are separate and never mixed.
    property string kind: "panel"
    // "builtin", "user" or "all" (both, then the looks).
    property string scope: "builtin"
    property var presets: []
    // Panel Studio's look rows (SettingsPopup.panelThemeRows): sample looks
    // under their headings, and the artwork actions. The looks are drawn by
    // `studio`, which also loads them.
    property var themeRows: []
    property var studio: null
    // Panel Studio's look.
    readonly property var theme: studio && studio.studioTheme ? studio.studioTheme : StudioThemes.theme("classic")
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
    readonly property bool allScope: scope === "all"
    readonly property bool catalogValid: catalogStatus && catalogStatus.valid === true
    readonly property alias listView: list
    // Presets and looks in one list: each item names its kind and the
    // heading it sits under.
    readonly property var items: {
        const result = [];
        if (catalogValid)
            for (const preset of presets || [])
                result.push({ type: "preset", preset: preset, group: !allScope ? ""
                    : preset.builtIn === true ? qsTr("Built-in presets") : qsTr("My presets") });
        for (const row of themeRows || [])
            if (row.kind === "themeSamples")
                for (const theme of row.themes || [])
                    result.push({ type: "theme", theme: theme, group: String(row.label || "") });
        return result;
    }
    readonly property var artworkRow: (themeRows || []).find(function(row) { return row.kind === "actions"; }) || null
    // Only looks, no presets (a profile's panel): the page is its looks.
    readonly property bool looksOnly: allScope && (presets || []).length === 0 && (themeRows || []).length > 0
    readonly property string title: looksOnly ? qsTr("Looks")
        : allScope ? qsTr("Presets")
        : builtInScope
        ? (panelKind ? qsTr("Built-in Panel Presets") : qsTr("Built-in Icon Presets"))
        : (panelKind ? qsTr("My Panel Presets") : qsTr("My Icon Presets"))
    readonly property string description: looksOnly
        ? qsTr("Looks for this panel. Load puts one on it; Apply saves it.")
        : allScope
        ? qsTr("Complete starting configurations for this panel and looks for it. Built-in presets are read-only: duplicate one to make it yours. Load puts a look on this panel; Apply saves it.")
        : builtInScope
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
    // An artwork action of the look rows (import, re-render, clear).
    signal studioActionRequested(string action)

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
        const entry = list.itemAtIndex(index);
        const card = entry && entry.item ? entry.item : entry;
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
                text: StudioThemes.label(root.theme, root.title)
                color: root.theme.heading
                font.pixelSize: 16
                font.letterSpacing: root.theme.letterSpacing
                font.weight: root.theme.headingWeight
                Accessible.role: Accessible.Heading
            }

            Label {
                Layout.fillWidth: true
                text: root.description
                color: root.theme.description
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Label {
                visible: !root.looksOnly
                Layout.fillWidth: true
                text: qsTr("Selecting a preset shows it in the preview above. It does not change any panel.")
                color: root.theme.hint
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
            theme: root.theme
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

        StudioFrame {
            objectName: "preset-pending-action"
            visible: root.pendingAction !== null
            Layout.fillWidth: true
            implicitHeight: pendingRow.implicitHeight + 16
            radius: root.theme.controlRadius + 1
            chamfer: root.theme.chamfer / 2
            color: root.theme.bar
            borderColor: root.theme.barBorder

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
                    color: root.theme.text
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
            visible: root.catalogValid && list.count === 0 && !root.allScope
            Layout.fillWidth: true
            Layout.topMargin: 12
            horizontalAlignment: Text.AlignHCenter
            text: root.builtInScope
                ? qsTr("This catalog has no presets.")
                : (root.panelKind
                    ? qsTr("You have no Panel Presets yet. Open Built-in Panel Presets and choose Duplicate to My Presets.")
                    : qsTr("You have no Icon Presets yet. Open Built-in Icon Presets and choose Duplicate to My Presets."))
            color: root.theme.textMuted
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
            model: root.items
            boundsBehavior: Flickable.StopAtBounds
            Accessible.role: Accessible.List
            Accessible.name: root.title

            ScrollBar.vertical: ScrollBar {}

            section.property: "group"
            section.delegate: Label {
                required property string section

                visible: section.length > 0
                height: visible ? implicitHeight + 10 : 0
                topPadding: 6
                text: StudioThemes.label(root.theme, section)
                color: root.theme.heading
                font.letterSpacing: root.theme.letterSpacing
                font.pixelSize: 14
                font.weight: root.theme.headingWeight
                Accessible.role: Accessible.Heading
            }

            delegate: Loader {
                id: entry

                required property var modelData
                required property int index

                width: ListView.view.width - 12
                sourceComponent: modelData.type === "theme" ? themeCard : presetCard

                Component {
                    id: presetCard

                    PresetCard {
                        theme: root.theme
                        preset: entry.modelData.preset
                        // A panel preset shows the bare panel, no icons;
                        // an icon preset shows its icons.
                        barePreview: root.panelKind
                        actionsEnabled: root.actionsEnabled
                        selected: presetId.length > 0 && presetId === root.selectedPresetId
                        onSelectRequested: {
                            list.currentIndex = entry.index;
                            root.presetSelected(presetId);
                        }
                        onDuplicateRequested: root.duplicateRequested(
                            presetId, qsTr("%1 copy").arg(String(entry.modelData.preset.name || "")))
                        onRenameRequested: root.beginRename(entry.modelData.preset)
                        onRemoveRequested: root.beginRemove(entry.modelData.preset)
                        onPreviewRequested: root.previewRequested(presetId)
                        onApplyRequested: root.applyRequested(presetId)
                        Keys.onDownPressed: root.focusCard(entry.index + 1)
                        Keys.onUpPressed: root.focusCard(entry.index - 1)
                    }
                }
                Component {
                    id: themeCard

                    ThemeSampleCard {
                        studio: root.studio
                        theme: entry.modelData.theme
                    }
                }
            }

            footer: ColumnLayout {
                width: ListView.view.width - 12
                visible: root.artworkRow !== null
                spacing: 6

                Label {
                    Layout.topMargin: 12
                    text: StudioThemes.label(root.theme, root.artworkRow ? String(root.artworkRow.label || "") : "")
                    color: root.theme.heading
                    font.pixelSize: 14
                    font.letterSpacing: root.theme.letterSpacing
                    font.weight: root.theme.headingWeight
                }
                Label {
                    Layout.fillWidth: true
                    text: root.artworkRow ? String(root.artworkRow.description || "") : ""
                    color: root.theme.description
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    spacing: 8
                    Repeater {
                        model: root.artworkRow ? root.artworkRow.actions || [] : []
                        delegate: Button {
                            required property var modelData

                            text: String(modelData.label || "")
                            icon.name: String(modelData.icon || "")
                            enabled: modelData.available !== false
                            onClicked: root.studioActionRequested(String(modelData.action || ""))
                        }
                    }
                }
            }
        }
    }
}
