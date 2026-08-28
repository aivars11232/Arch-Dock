pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import ArchDock.Rendering 1.0
import org.kde.kirigami as Kirigami
import "CapabilityModel.js" as CapabilityModel
import "PlacementStatus.js" as PlacementStatus
import "SettingsEditorModel.js" as EditorModel
import "StudioNavigation.js" as StudioNavigation
import "VisibilityStatus.js" as VisibilityStatus

Window {
    id: root

    readonly property bool renderingModuleReady: RenderingModuleProbe.ready
    property string selectedPanelId: panelRegistry.activePanelId
    property bool showPanels: false
    property int mainTabIndex: 0
    property int subTabIndex: 0
    property var subTabMemory: [0, 0, 0, 0, 0]
    property var editorSession: EditorModel.emptySession("", "")
    property var artifactDraft: ({})
    property string themeTargetPanelId: ""
    property var colorField: null
    property string studioError: ""
    property string studioWarning: ""
    property bool internalPanelSelection: false
    property string previewMode: "horizontal"
    property string previewPresentationState: "open"
    property int previewStateEntry: 1
    property string previewIconState: "normal"

    readonly property int panelRevision: panelRegistry.revision
    readonly property int placementRevision: panelController.nativePlacementRevision
    readonly property int nativeVisibilityRevision: panelController.nativeVisibilityRevision
    readonly property bool hasSettingsChanges: EditorModel.dirty(editorSession)
    readonly property bool hasArtifactChanges: String(artifactDraft.action || "").length > 0
    readonly property bool hasPendingChanges: hasSettingsChanges || hasArtifactChanges
    readonly property var selectedCapabilityResolution: editorSession.capabilityResolution || ({})
    readonly property var selectedResolvedThemes: editorSession.themes || []
    readonly property var selectedRendererCandidate:
        EditorModel.rendererCandidate(editorSession)
    readonly property var selectedPreviewTheme:
        rendererPreviewTheme(selectedRendererCandidate)
    readonly property var selectedPlacementResult: {
        const revision = placementRevision;
        return panelController.nativePanelPlacementStatus(selectedPanelId);
    }
    readonly property var selectedVisibilityResult: {
        const revision = nativeVisibilityRevision;
        return panelController.nativePanelVisibilityStatus(selectedPanelId);
    }
    readonly property var currentSubtabs: StudioNavigation.subtabsFor(mainTabIndex)
    readonly property var mainTabLabels: StudioNavigation.mainLabels()
    readonly property var mainTabIcons: ["view-dashboard", "preferences-desktop-display", "preferences-desktop-icons", "draw-rectangle", "document-save"]

    function titleCase(value) {
        const text = String(value || "").replace(/-/g, " ");
        return text.length === 0 ? text : text.charAt(0).toUpperCase() + text.slice(1);
    }

    function optionIndex(options, value) {
        const source = options || [];
        for (let index = 0; index < source.length; ++index) {
            if (source[index].value === value)
                return index;
        }
        return 0;
    }

    function rendererPreviewMode(candidate) {
        const edge = String(candidate && candidate.edge
            || panelRegistry.panelValue(selectedPanelId, "edge")
            || "bottom");
        if (edge === "free")
            return "free";
        return edge === "left" || edge === "right"
            ? "vertical" : "horizontal";
    }

    function rendererPreviewTheme(candidate) {
        const themeId = String(candidate && (candidate.panelThemeId
            || candidate.completeThemeId) || "");
        if (themeId.length === 0)
            return {};
        const themes = CapabilityModel.normalized(selectedResolvedThemes);
        for (let index = 0; index < themes.length; ++index) {
            if (String(themes[index].id || "") === themeId)
                return themes[index];
        }
        return {};
    }

    function themeRendererCandidate(theme) {
        return EditorModel.rendererThemeCandidate(editorSession, theme);
    }

    function resetRendererPreview(candidate) {
        previewMode = rendererPreviewMode(candidate);
        previewPresentationState = "open";
        previewStateEntry = 1;
        previewIconState = "normal";
    }

    function loadEditor(panelId) {
        const normalizedPanelId = String(panelId || "");
        if (normalizedPanelId.length === 0) {
            editorSession = EditorModel.emptySession("panel-not-found", qsTr("No panel is selected."));
            return false;
        }
        const snapshot = panelController.panelSettingsEditorSnapshot(normalizedPanelId, "studio");
        const loaded = EditorModel.load(snapshot);
        editorSession = loaded;
        if (!loaded.loaded) {
            studioError = qsTr("Panel settings could not be loaded (%1).").arg(loaded.errorCode);
            return false;
        }
        resetRendererPreview(EditorModel.rendererCandidate(loaded));
        studioError = "";
        studioWarning = "";
        return true;
    }

    function selectPanel(panelId) {
        const normalizedPanelId = String(panelId || "");
        if (normalizedPanelId.length === 0 || normalizedPanelId === selectedPanelId)
            return true;
        if (hasPendingChanges) {
            studioError = qsTr("Apply or cancel the current draft before switching panels.");
            return false;
        }
        internalPanelSelection = true;
        selectedPanelId = normalizedPanelId;
        internalPanelSelection = false;
        panelRegistry.setActivePanelId(normalizedPanelId);
        artifactDraft = {};
        return loadEditor(normalizedPanelId);
    }

    function openPanelEditor(panelId) {
        if (selectPanel(panelId)) {
            setMainTab(1);
            setSubTab(0);
        }
    }

    function setMainTab(index) {
        const next = StudioNavigation.clampSectionIndex(index);
        mainTabIndex = next;
        subTabIndex = StudioNavigation.clampSubtabIndex(next, subTabMemory[next] || 0);
    }

    function setSubTab(index) {
        const next = StudioNavigation.clampSubtabIndex(mainTabIndex, index);
        subTabIndex = next;
        const memory = subTabMemory.slice();
        memory[mainTabIndex] = next;
        subTabMemory = memory;
    }

    function panelValue(key, fallback) {
        return EditorModel.panelValue(editorSession, key, fallback);
    }

    function globalValue(key, fallback) {
        return EditorModel.globalValue(editorSession, key, fallback);
    }

    function isNativePanel() {
        return String(panelRegistry.panelValue(selectedPanelId, "edge")
            || panelValue("edge", "bottom")) !== "free";
    }

    function refreshProjection() {
        if (!editorSession.loaded)
            return false;
        const projected = panelController.resolvePanelSettingsEditorDraft(editorSession.panelId, editorSession.revision, EditorModel.panelCandidate(editorSession), EditorModel.globalCandidate(editorSession), "studio");
        if (!projected || projected.success !== true) {
            editorSession = EditorModel.retainFailure(editorSession, projected);
            studioError = qsTr("Draft validation failed: %1 (%2)").arg(String(projected && projected.status ? projected.status : "failed")).arg(String(projected && (projected.errorMessage || projected.errorCode) ? (projected.errorMessage || projected.errorCode) : qsTr("No details were returned.")));
            return false;
        }
        editorSession = EditorModel.withProjection(editorSession, projected);
        studioError = "";
        return true;
    }

    function fieldValue(field) {
        return field.scope === "settings" ? globalValue(field.key, field.fallback) : panelValue(field.key, field.fallback);
    }

    function setFieldValue(field, value) {
        if (field.scope === "settings") {
            editorSession = EditorModel.setGlobalValue(editorSession, field.key, value);
        } else {
            editorSession = EditorModel.setPanelValue(editorSession, field.key, value);
        }
        refreshProjection();
    }

    function fieldOptionIndex(field) {
        return optionIndex(field.options || [], fieldValue(field));
    }

    function formatFieldValue(field, value) {
        const decimals = field.decimals === undefined ? 0 : field.decimals;
        const numeric = Number(value) * (field.displayScale === undefined ? 1 : field.displayScale);
        const text = decimals > 0 ? numeric.toFixed(decimals) : Math.round(numeric);
        return (field.prefix || "") + text + (field.suffix || "");
    }

    function displayFieldValue(field) {
        return field.value !== undefined ? String(field.value) : String(fieldValue(field));
    }

    function colorDisplayValue(field) {
        const value = String(fieldValue(field) || "").trim();
        return value.length > 0 ? value : qsTr("Theme default");
    }

    function colorPreviewValue(field) {
        const value = String(fieldValue(field) || "").trim();
        return /^#(?:[0-9a-f]{3,4}|[0-9a-f]{6}|[0-9a-f]{8})$/i.test(value) ? value : "transparent";
    }

    function openColorEditor(field) {
        colorField = field;
        const current = String(fieldValue(field) || "").trim();
        colorDialog.selectedColor = current.length > 0 ? current : "#334455";
        colorDialog.open();
    }

    function fieldDescriptor(key, scope) {
        const fields = scope === "settings" ? editorSession.globalFields : editorSession.panelFields;
        for (let index = 0; index < fields.length; ++index) {
            if (String(fields[index].key) === key)
                return fields[index];
        }
        return null;
    }

    function optionLabel(key, value, scope) {
        const descriptor = fieldDescriptor(key, scope || "panel");
        const options = descriptor ? descriptor.options || [] : [];
        const index = optionIndex(options, value);
        return options.length > 0 && options[index].value === value ? options[index].label : titleCase(value);
    }

    function readOnlyRow(label, value, description) {
        return {
            kind: "readonly",
            label: label,
            value: value,
            description: description || ""
        };
    }

    function section(label, description, first) {
        return {
            kind: "section",
            label: label,
            description: description || "",
            first: first === true
        };
    }

    function notice(text, warning) {
        return {
            kind: "notice",
            text: text,
            warning: warning === true
        };
    }

    function choicesToOptions(choices) {
        const result = [];
        const source = choices || [];
        for (let index = 0; index < source.length; ++index) {
            result.push({
                label: titleCase(source[index]),
                value: source[index]
            });
        }
        return result;
    }

    function editorRow(descriptor) {
        const control = String(descriptor.control || "");
        const row = {
            kind: control === "screen" ? "combo" : control,
            key: descriptor.key,
            label: descriptor.label,
            scope: descriptor.scope === "global" ? "settings" : "panel",
            fallback: descriptor.defaultValue,
            options: descriptor.options && descriptor.options.length > 0 ? descriptor.options : choicesToOptions(descriptor.choices),
            description: descriptor.scope === "global" ? qsTr("Applies to all Arch Dock panels") : ""
        };
        if (descriptor.minimumValue !== undefined && descriptor.minimumValue !== null)
            row.from = Number(descriptor.minimumValue);
        if (descriptor.maximumValue !== undefined && descriptor.maximumValue !== null)
            row.to = Number(descriptor.maximumValue);
        if (descriptor.step !== undefined)
            row.step = Number(descriptor.step);
        if (descriptor.decimals !== undefined)
            row.decimals = Number(descriptor.decimals);
        if (descriptor.suffix !== undefined)
            row.suffix = descriptor.suffix;
        return row;
    }

    function fieldsForSection(sectionId) {
        const result = [];
        const sources = [editorSession.panelFields, editorSession.globalFields];
        for (let sourceIndex = 0; sourceIndex < sources.length; ++sourceIndex) {
            const source = sources[sourceIndex] || [];
            for (let index = 0; index < source.length; ++index) {
                if (String(source[index].section) === sectionId)
                    result.push(editorRow(source[index]));
            }
        }
        return result;
    }

    function schemaSectionRows(sectionId, label, description) {
        const rows = [section(label, description, true)];
        const fields = fieldsForSection(sectionId);
        for (let index = 0; index < fields.length; ++index)
            rows.push(fields[index]);
        if (fields.length === 0) {
            rows.push(notice(qsTr("No settings in this section are available for the resolved panel capabilities.")));
        }
        return rows;
    }

    function overviewPanelRows() {
        const color = String(panelValue("color", "")).trim();
        return [section(qsTr("Panel"), qsTr("Settings currently applied to the selected panel."), true), readOnlyRow(qsTr("Name"), panelRegistry.panelName(selectedPanelId)), readOnlyRow(qsTr("Type"), optionLabel("type", panelValue("type", "empty"))), readOnlyRow(qsTr("Position"), optionLabel("edge", panelValue("edge", "bottom"))), readOnlyRow(qsTr("Layout"), optionLabel("layout", panelValue("layout", "adaptive"))), readOnlyRow(qsTr("Theme"), optionLabel("appearance", panelValue("appearance", "glass"))), readOnlyRow(qsTr("Color"), color.length > 0 ? color : qsTr("Theme default")), readOnlyRow(qsTr("Opacity"), Math.round(Number(panelValue("opacity", 0.9)) * 100) + "%")];
    }

    function overviewIconRows() {
        return [section(qsTr("Icons"), qsTr("Settings currently applied to icons in the selected panel."), true), readOnlyRow(qsTr("Size"), panelValue("iconSize", 52) + qsTr(" px")), readOnlyRow(qsTr("Spacing"), Math.round(Number(panelValue("spacing", 8))) + qsTr(" px")), readOnlyRow(qsTr("Shape"), optionLabel("iconShape", panelValue("iconShape", "rounded"))), readOnlyRow(qsTr("Animation"), optionLabel("iconAnimation", panelValue("iconAnimation", "scale"))), readOnlyRow(qsTr("Magnification"), globalValue("magnificationEnabled", true) ? Number(globalValue("magnification", 1.65)).toFixed(2) + "×" : qsTr("Off"))];
    }

    function panelGeneralRows() {
        const rows = schemaSectionRows("panels-general", qsTr("General"), qsTr("Identity and placement of the selected panel."));
        const builtIn = panelRegistry.isBuiltIn(selectedPanelId);
        rows.splice(1, 0, {
            kind: "actions",
            label: qsTr("Panel"),
            actions: [
                {
                    label: qsTr("Add free panel"),
                    icon: "list-add",
                    action: "create-free",
                    available: !hasPendingChanges
                },
                {
                    label: builtIn ? (Boolean(panelValue("visible", true)) ? qsTr("Hide panel") : qsTr("Show panel")) : qsTr("Remove panel"),
                    icon: builtIn ? (Boolean(panelValue("visible", true)) ? "view-hidden" : "view-visible") : "edit-delete",
                    action: builtIn ? "toggle-panel-visibility" : "remove-panel",
                    available: builtIn || !hasPendingChanges
                }
            ]
        });
        if (isNativePanel()) {
            rows.push(section(qsTr("Native placement result"), PlacementStatus.statusLabel(selectedPlacementResult.status)), readOnlyRow(qsTr("Saved intent"), PlacementStatus.savedIntentText(selectedPlacementResult)), readOnlyRow(qsTr("Actual Plasma host"), PlacementStatus.hostStateText(selectedPlacementResult)));
            if (PlacementStatus.isProblem(selectedPlacementResult)) {
                rows.push(notice(qsTr("Placement detail: %1").arg(PlacementStatus.problemText(selectedPlacementResult)), true));
            }
        }
        return rows;
    }

    function panelAppearanceRows() {
        const rows = schemaSectionRows("panels-appearance", qsTr("Appearance"), qsTr("Surface styling for the selected panel."));
        rows.splice(1, 0, {
            kind: "themeSamples",
            label: qsTr("Built-in themes"),
            description: qsTr("Available themes are resolved by the backend for this panel."),
            themes: CapabilityModel.availableItems(selectedResolvedThemes)
        });
        rows.push({
            kind: "actions",
            label: qsTr("Artwork"),
            description: artifactDescription(),
            actions: [
                {
                    label: qsTr("Import"),
                    icon: "document-import",
                    action: "import-theme"
                },
                {
                    label: qsTr("Re-render"),
                    icon: "view-refresh",
                    action: "render-theme"
                },
                {
                    label: qsTr("Clear"),
                    icon: "edit-clear",
                    action: "clear-theme"
                }
            ]
        });
        return rows;
    }

    function panelBehaviorRows() {
        const rows = schemaSectionRows("panels-behavior", qsTr("Behavior"), qsTr("Visibility and interaction rules."));
        if (isNativePanel()) {
            rows.push(readOnlyRow(qsTr("Native host result"), VisibilityStatus.statusLabel(selectedVisibilityResult.status), qsTr("The last ownership-verified Plasma result")));
            if (VisibilityStatus.isProblem(selectedVisibilityResult)) {
                rows.push(notice(qsTr("Visibility detail: %1").arg(VisibilityStatus.problemText(selectedVisibilityResult)), true));
            }
        }
        return rows;
    }

    function unavailablePage(label, description) {
        return [section(label, description, true), notice(qsTr("This page remains unavailable until its renderer and persistence path are implemented."))];
    }

    function rowsForCurrentPage() {
        if (mainTabIndex === 0)
            return subTabIndex === 0 ? overviewPanelRows() : overviewIconRows();
        if (mainTabIndex === 1) {
            if (subTabIndex === 0)
                return panelGeneralRows();
            if (subTabIndex === 1)
                return schemaSectionRows("panels-size", qsTr("Size"), qsTr("Panel dimensions and dynamic sizing."));
            if (subTabIndex === 2)
                return panelAppearanceRows();
            if (subTabIndex === 3)
                return panelBehaviorRows();
            if (subTabIndex === 4)
                return schemaSectionRows("panels-layout", qsTr("Layout"), qsTr("Shape geometry and content placement."));
            return unavailablePage(qsTr("Segments"), qsTr("Independent panel surface sections."));
        }
        if (mainTabIndex === 2) {
            if (subTabIndex === 0)
                return schemaSectionRows("icons-appearance", qsTr("Appearance"), qsTr("Icon appearance for the selected panel."));
            if (subTabIndex === 1)
                return schemaSectionRows("icons-behavior", qsTr("Behavior"), qsTr("Icon motion and magnification."));
            if (subTabIndex === 2)
                return schemaSectionRows("icons-indicators", qsTr("Indicators"), qsTr("Running and attention markers."));
            if (subTabIndex === 3)
                return unavailablePage(qsTr("Notifications"), qsTr("Badges and transient icon notices."));
            return unavailablePage(qsTr("Icon Style"), qsTr("Reusable icon appearance sets."));
        }
        if (mainTabIndex === 3)
            return unavailablePage(qsTr("Icon Tiles"), qsTr("Independent icon tile rendering."));
        return unavailablePage(currentSubtabs.length > 0 ? currentSubtabs[subTabIndex] : qsTr("Profiles"), qsTr("Reusable profiles are outside the current settings contract."));
    }

    function stageArtifact(action, sourceUrl) {
        artifactDraft = {
            action: String(action || ""),
            sourceUrl: sourceUrl || ""
        };
        studioError = "";
    }

    function artifactDescription() {
        const action = String(artifactDraft.action || "");
        if (action === "import")
            return qsTr("Artwork import pending");
        if (action === "clear")
            return qsTr("Artwork removal pending");
        if (action === "render")
            return qsTr("Artwork re-render pending");
        return String(panelRegistry.panelValue(selectedPanelId, "themeStatus") || qsTr("Preset surface active."));
    }

    function applyArtifact() {
        const action = String(artifactDraft.action || "");
        if (action.length === 0)
            return true;
        if (action === "clear") {
            panelRegistry.clearTheme(selectedPanelId);
            return true;
        }
        if (action === "import")
            return panelRegistry.importTheme(selectedPanelId, artifactDraft.sourceUrl);
        if (action === "render") {
            return panelRegistry.renderTheme(selectedPanelId, Number(panelValue("width", 720)), Number(panelValue("height", 76)), Screen.devicePixelRatio, true);
        }
        return false;
    }

    function discardStudioChanges() {
        editorSession = EditorModel.cancel(editorSession);
        resetRendererPreview(EditorModel.rendererCandidate(editorSession));
        artifactDraft = {};
        themeTargetPanelId = "";
        colorField = null;
        studioError = "";
        studioWarning = "";
    }

    function applyStudioChanges() {
        studioError = "";
        studioWarning = "";
        if (!editorSession.loaded)
            return false;

        if (hasSettingsChanges) {
            const result = panelController.applyPanelSettingsTransaction(editorSession.panelId, editorSession.revision, EditorModel.panelCandidate(editorSession), EditorModel.globalCandidate(editorSession));
            if (!EditorModel.transactionSucceeded(result)) {
                editorSession = EditorModel.retainFailure(editorSession, result);
                studioError = EditorModel.transactionConflict(result) ? qsTr("This panel changed outside Panel Studio. Cancel and reopen it before applying.") : qsTr("Settings transaction failed: %1 (%2)").arg(String(result && result.status ? result.status : "failed")).arg(String(result && (result.errorMessage || result.errorCode) ? (result.errorMessage || result.errorCode) : qsTr("No details were returned.")));
                return false;
            }

            const refreshed = panelController.panelSettingsEditorSnapshot(editorSession.panelId, "studio");
            editorSession = EditorModel.adoptResult(editorSession, result, refreshed);
            if (!editorSession.loaded || EditorModel.dirty(editorSession)) {
                studioError = qsTr("The transaction succeeded, but its saved revision could not be reloaded.");
                return false;
            }
        }

        if (!applyArtifact()) {
            studioError = qsTr("The settings transaction completed, but the separate artwork operation failed.");
            return false;
        }
        artifactDraft = {};
        return loadEditor(selectedPanelId);
    }

    function acceptStudioChanges() {
        if (!hasPendingChanges || applyStudioChanges())
            close();
    }

    function cancelStudioChanges() {
        discardStudioChanges();
        close();
    }

    function performStudioAction(action, data) {
        if (action === "create-free") {
            if (hasPendingChanges)
                return;
            const result = panelController.createFreePanel();
            if (result && result.success === true && String(result.panelId || "").length > 0) {
                openPanelEditor(String(result.panelId));
            } else {
                studioError = qsTr("Could not create the free panel (%1).").arg(String(result && result.errorCode ? result.errorCode : "unknown-error"));
            }
        } else if (action === "remove-panel") {
            if (hasPendingChanges)
                return;
            panelController.removePanel(selectedPanelId);
            internalPanelSelection = true;
            selectedPanelId = panelRegistry.activePanelId;
            internalPanelSelection = false;
            loadEditor(selectedPanelId);
        } else if (action === "toggle-panel-visibility") {
            editorSession = EditorModel.setPanelValue(editorSession, "visible", !Boolean(panelValue("visible", true)));
            refreshProjection();
        } else if (action === "load-built-in-theme") {
            const candidate = panelRegistry.themeCandidate(selectedPanelId, String(data.themeId || ""), "complete");
            if (!candidate || candidate.success !== true) {
                studioError = qsTr("Theme is unavailable: %1").arg(String(candidate && (candidate.errorMessage || candidate.errorCode) ? (candidate.errorMessage || candidate.errorCode) : qsTr("No details were returned.")));
                return;
            }
            editorSession = EditorModel.stagePanelValues(editorSession, candidate.values || {});
            refreshProjection();
        } else if (action === "import-theme") {
            themeTargetPanelId = selectedPanelId;
            themeDialog.open();
        } else if (action === "render-theme") {
            stageArtifact("render", "");
        } else if (action === "clear-theme") {
            stageArtifact("clear", "");
        }
    }

    onSelectedPanelIdChanged: {
        if (internalPanelSelection)
            return;
        if (editorSession.loaded && editorSession.panelId !== selectedPanelId && hasPendingChanges) {
            const previousPanelId = editorSession.panelId;
            studioError = qsTr("Apply or cancel the current draft before switching panels.");
            internalPanelSelection = true;
            selectedPanelId = previousPanelId;
            internalPanelSelection = false;
            return;
        }
        artifactDraft = {};
        loadEditor(selectedPanelId);
    }

    onShowPanelsChanged: {
        if (showPanels)
            setMainTab(1);
    }

    Component.onCompleted: loadEditor(selectedPanelId)

    Connections {
        target: panelRegistry

        function onRevisionChanged() {
            if (!root.hasPendingChanges)
                root.loadEditor(root.selectedPanelId);
        }
    }

    width: 980
    height: 820
    minimumWidth: 820
    minimumHeight: 640
    x: 260
    y: 120
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    title: qsTr("Arch Dock Panel Studio")

    onVisibleChanged: {
        if (visible) {
            if (!hasPendingChanges)
                loadEditor(selectedPanelId);
            requestActivate();
        }
    }
    onClosing: discardStudioChanges()

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.cancelStudioChanges()
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        border.width: 1
        border.color: "#75d9edf2"
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#f62a3848"
            }
            GradientStop {
                position: 1
                color: "#f1081018"
            }
        }
    }

    Rectangle {
        id: titleStrip

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 38
        color: "#e8182b3b"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: "#5e77d6e8"
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Kirigami.Icon {
                width: 18
                height: 18
                source: "preferences-desktop-theme-global"
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Arch Dock")
                color: "#f4f8fb"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 17
                color: "#4d9cb4c1"
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Panel Studio")
                color: "#a9bfcb"
                font.pixelSize: 11
            }
        }

        Label {
            anchors.centerIn: parent
            text: panelRegistry.panelName(root.selectedPanelId) || qsTr("Panel Editor")
            color: "#d5e5ed"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        DragHandler {
            target: null
            onActiveChanged: {
                if (active)
                    root.startSystemMove();
            }
        }

        ToolButton {
            id: closeButton

            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 30
            height: 30
            icon.name: "window-close"
            onClicked: root.cancelStudioChanges()

            background: Rectangle {
                radius: 4
                color: closeButton.hovered ? "#b94b526f" : "transparent"
            }
        }
    }

    RowLayout {
        anchors.top: titleStrip.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 10
        anchors.leftMargin: 10
        anchors.rightMargin: 14
        anchors.bottomMargin: 10
        spacing: 12

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 164
            radius: 8
            color: "#4a101a23"
            border.width: 1
            border.color: "#263d5361"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.topMargin: 4
                    Layout.bottomMargin: 6
                    text: qsTr("STUDIO")
                    color: "#718995"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                }

                Repeater {
                    model: root.mainTabLabels

                    delegate: ItemDelegate {
                        id: mainTabDelegate

                        required property string modelData
                        required property int index

                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        text: modelData
                        icon.name: root.mainTabIcons[index]
                        checkable: true
                        checked: root.mainTabIndex === index
                        onClicked: root.setMainTab(index)

                        contentItem: RowLayout {
                            spacing: 10

                            Kirigami.Icon {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                source: mainTabDelegate.icon.name
                                color: mainTabDelegate.checked ? "#7ce7fa" : "#9db0ba"
                            }

                            Label {
                                Layout.fillWidth: true
                                text: mainTabDelegate.text
                                color: mainTabDelegate.checked ? "#f4fbff" : "#b2c1c8"
                                font.weight: mainTabDelegate.checked ? Font.DemiBold : Font.Normal
                            }
                        }

                        background: Rectangle {
                            radius: 6
                            color: mainTabDelegate.checked ? "#4b23627a" : (mainTabDelegate.hovered ? "#26384a56" : "transparent")
                            border.width: mainTabDelegate.checked ? 1 : 0
                            border.color: "#5e73cfe7"
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Label {
                    Layout.fillWidth: true
                    Layout.margins: 7
                    text: qsTr("Changes are held until Apply or OK")
                    color: "#617985"
                    font.pixelSize: 9
                    wrapMode: Text.Wrap
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Panel")
                    color: "#91a8b5"
                }

                ComboBox {
                    id: panelSelector

                    Layout.fillWidth: true
                    model: panelRegistry.panelIds
                    currentIndex: Math.max(0, model.indexOf(root.selectedPanelId))
                    displayText: panelRegistry.panelName(currentText)
                    delegate: ItemDelegate {
                        required property string modelData

                        width: panelSelector.width
                        text: panelRegistry.panelName(modelData)
                    }
                    onActivated: {
                        focus = false;
                        root.selectPanel(currentText);
                    }
                }

                Button {
                    icon.name: "list-add"
                    text: qsTr("Free panel")
                    enabled: !root.hasPendingChanges
                    onClicked: root.performStudioAction("create-free", {})
                }
            }

            Rectangle {
                id: rendererPreviewCard

                Layout.fillWidth: true
                Layout.preferredHeight: 210
                radius: 8
                color: "#32121f2a"
                border.width: 1
                border.color: "#3d587080"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("Live renderer preview")
                            color: "#e9f5fa"
                            font.weight: Font.DemiBold
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            id: previewModeSelector

                            Layout.preferredWidth: 154
                            model: [
                                {
                                    label: qsTr("Horizontal native"),
                                    value: "horizontal"
                                },
                                {
                                    label: qsTr("Vertical native"),
                                    value: "vertical"
                                },
                                {
                                    label: qsTr("Free"),
                                    value: "free"
                                }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.optionIndex(
                                model, root.previewMode)
                            onActivated: root.previewMode = currentValue
                        }

                        ComboBox {
                            id: previewIconStateSelector

                            Layout.preferredWidth: 118
                            model: [
                                { label: qsTr("Normal"), value: "normal" },
                                { label: qsTr("Hover"), value: "hover" },
                                { label: qsTr("Pressed"), value: "pressed" },
                                { label: qsTr("Active"), value: "active" },
                                { label: qsTr("Running"), value: "running" },
                                { label: qsTr("Minimized"), value: "minimized" },
                                { label: qsTr("Urgent"), value: "urgent" },
                                { label: qsTr("Drop"), value: "drop" },
                                { label: qsTr("Edit"), value: "edit" }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: root.optionIndex(
                                model, root.previewIconState)
                            onActivated:
                                root.previewIconState = currentValue
                        }

                        Button {
                            text: checked ? qsTr("Collapsed") : qsTr("Open")
                            checkable: true
                            checked: root.previewPresentationState
                                === "collapsed"
                            onClicked: root.previewPresentationState = checked
                                ? "collapsed" : "open"
                        }
                    }

                    LivePanelPreview {
                        id: embeddedRendererPreview

                        objectName: "panel-studio-live-renderer-preview"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        panelDefinition: root.selectedRendererCandidate
                        hostCapabilities:
                            root.selectedCapabilityResolution
                        themeDefinition: root.selectedPreviewTheme
                        iconStyleDefinition:
                            root.selectedRendererCandidate
                        indicatorStyleDefinition:
                            root.selectedPreviewTheme.indicatorStyle || ({})
                        animationProfiles:
                            root.selectedRendererCandidate
                        previewMode: root.previewMode
                        presentationState:
                            root.previewPresentationState
                        stateEntry: root.previewStateEntry
                        iconState: root.previewIconState
                        contentMargin: 6
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("Renderer: %1").arg(
                                embeddedRendererPreview.activeRendererTier)
                            color: "#86dff2"
                            font.pixelSize: 10
                        }

                        Label {
                            visible: embeddedRendererPreview.fallbackApplied
                            text: qsTr("Fallback: %1").arg(
                                embeddedRendererPreview.fallbackReason
                                || qsTr("unspecified"))
                            color: "#ffc66d"
                            font.pixelSize: 10
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Label {
                            text: root.hasSettingsChanges
                                ? qsTr("Draft only — desktop unchanged")
                                : qsTr("Saved settings")
                            color: root.hasSettingsChanges
                                ? "#80de70" : "#728995"
                            font.pixelSize: 10
                        }
                    }
                }
            }

            TabBar {
                visible: root.currentSubtabs.length > 0
                Layout.fillWidth: true
                currentIndex: root.subTabIndex

                Repeater {
                    model: root.currentSubtabs

                    delegate: TabButton {
                        required property string modelData
                        required property int index

                        text: modelData
                        width: Math.max(104, implicitWidth)
                        onClicked: root.setSubTab(index)
                    }
                }
            }

            StudioForm {
                Layout.fillWidth: true
                Layout.fillHeight: true
                studio: root
                rows: root.rowsForCurrentPage()
            }
        }
    }

    RowLayout {
        id: footer

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 12
        anchors.rightMargin: 14
        anchors.bottomMargin: 10
        height: 38

        Label {
            text: root.mainTabLabels[root.mainTabIndex] + (root.currentSubtabs.length > 0 ? "  /  " + root.currentSubtabs[root.subTabIndex] : "")
            color: "#69808d"
            font.pixelSize: 10
        }

        Label {
            visible: root.studioError.length > 0
            Layout.maximumWidth: 420
            text: root.studioError
            color: "#ff8c8c"
            font.pixelSize: 10
            elide: Text.ElideRight
        }

        Label {
            visible: root.studioWarning.length > 0
            Layout.maximumWidth: 420
            text: root.studioWarning
            color: "#ffc66d"
            font.pixelSize: 10
            elide: Text.ElideRight
        }

        Label {
            visible: root.hasPendingChanges && root.studioError.length === 0 && root.studioWarning.length === 0
            text: qsTr("Pending changes")
            color: "#80de70"
            font.pixelSize: 10
        }

        Item {
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("OK")
            icon.name: "dialog-ok"
            onClicked: root.acceptStudioChanges()
        }

        Button {
            text: qsTr("Apply")
            icon.name: "dialog-ok-apply"
            enabled: root.hasPendingChanges
            onClicked: root.applyStudioChanges()
        }

        Button {
            text: qsTr("Cancel")
            icon.name: "dialog-cancel"
            onClicked: root.cancelStudioChanges()
        }
    }

    FileDialog {
        id: themeDialog

        title: qsTr("Import theme package or artwork")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Theme packages (archdock-theme.json *.archdock-theme.json *.json)"), qsTr("Design files (*.png *.jpg *.jpeg *.webp *.avif *.heif *.heic *.jxl *.svg *.svgz *.tif *.tiff *.pdf *.psd *.xcf *.blend)"), qsTr("All files (*)")]
        onAccepted: {
            if (root.themeTargetPanelId === root.selectedPanelId)
                root.stageArtifact("import", selectedFile);
            root.themeTargetPanelId = "";
        }
        onRejected: root.themeTargetPanelId = ""
    }

    ColorDialog {
        id: colorDialog

        title: qsTr("Choose panel color")
        parentWindow: root
        modality: Qt.WindowModal
        options: ColorDialog.ShowAlphaChannel
        onAccepted: {
            if (root.colorField !== null)
                root.setFieldValue(root.colorField, selectedColor.toString());
            root.colorField = null;
        }
        onRejected: root.colorField = null
    }
}
