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
    readonly property bool compactStudio: width < 820 || height < 600
    property bool expandedCompactPreview: false

    function revealFocus(item) {
        let ancestor = item
        while (ancestor && ancestor !== studioScroll) ancestor = ancestor.parent
        const flick = studioScroll.contentItem
        if (ancestor !== studioScroll || !flick || !flick.contentItem) return
        const position = item.mapToItem(flick.contentItem, 0, 0)
        if (position.y < flick.contentY) flick.contentY = Math.max(0, position.y)
        else if (position.y + item.height > flick.contentY + flick.height)
            flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height,
                position.y + item.height - flick.height))
    }
    onActiveFocusItemChanged: revealFocus(activeFocusItem)
    // Selection drives the embedded preview; explicit actions start desktop audition.
    property string selectedPresetId: ""
    property string presetNoticeText: ""
    property bool presetNoticeIsError: false
    property string profileNotice: ""
    property bool profileNoticeIsError: false
    readonly property var profilesService: typeof profileManager === "undefined" ? null : profileManager
    readonly property var profiles: {
        const revision = profilesService ? profilesService.revision : 0;
        return profilesService ? profilesService.listProfiles() : [];
    }
    readonly property bool currentProfilePage: mainTabIndex === 4 && subTabIndex < 3

    readonly property var auditionService: typeof presetAudition === "undefined" ? null : presetAudition
    readonly property var auditionStatus: {
        const revision = auditionService ? auditionService.revision : 0;
        return auditionService ? auditionService.status : ({ state: "IDLE" });
    }
    readonly property bool auditionActive: auditionStatus.state === "ACTIVE"
    readonly property bool auditionBusy: auditionStatus.state !== "IDLE"
    property var auditionCustomizations: ({})
    property string auditionOriginalPanelId: ""
    property bool auditionNewPanel: false
    property bool auditionRecommendIcons: !String((panelRegistry.panelValue(selectedPanelId, "presetOrigin") || {}).iconPresetId || "").length
    readonly property var auditionDefaults: {
        const revision = auditionService ? auditionService.revision : 0;
        return auditionService ? auditionService.defaultSelection() : ({});
    }
    readonly property string auditionPresetId: auditionBusy
        ? String(auditionStatus.presetId || "") : selectedPresetId
    readonly property string auditionKind: auditionBusy
        ? String(auditionStatus.kind || "panel") : String(currentPresetPage && currentPresetPage.kind || "panel")
    readonly property bool auditionSelectedDefault: auditionPresetId.length > 0
        && String(auditionDefaults[auditionKind === "panel" ? "panelPresetId" : "iconPresetId"] || "") === auditionPresetId
    readonly property string auditionGuardError: {
        const revision = panelController.visibilityRevision;
        const guards = panelController.panelInteractionGuards(auditionBusy
            ? String(auditionStatus.panelId || selectedPanelId) : selectedPanelId);
        return guards.editMode ? "edit-mode-active" : guards.popupOpen ? "popup-open"
            : guards.dragActive ? "drag-active" : "";
    }
    readonly property bool showingAuditionDraft: auditionActive && currentPresetPage !== null
        && currentPresetPage.kind === auditionKind && selectedPresetId === auditionPresetId

    // The catalog the current page lists, or null on every other page.
    readonly property var currentPresetPage:
        StudioNavigation.presetPage(mainTabIndex, subTabIndex)
    readonly property var presetCards: {
        const revision = presetLibrary.revision;
        const auditionRevision = auditionService ? auditionService.revision : 0;
        const page = currentPresetPage;
        if (!page)
            return [];
        return page.kind === "panel" ? presetLibrary.panelPresets(page.scope)
                                     : presetLibrary.iconPresets(page.scope);
    }
    readonly property var selectedPresetCard: {
        const cards = presetCards;
        for (let index = 0; index < cards.length; ++index) {
            if (String(cards[index].id || "") === selectedPresetId)
                return cards[index];
        }
        return null;
    }
    // The selected preset plays its motion here unless reduced motion is on.
    readonly property var selectedPresetCandidate:
        showingAuditionDraft ? selectedRendererCandidate : EditorModel.presetRendererCandidate(selectedPresetCard,
            Boolean(globalValue("reducedMotion", false)))
    readonly property var selectedPresetTheme:
        showingAuditionDraft ? selectedPreviewTheme : EditorModel.presetPreviewTheme(selectedPresetCard)
    readonly property string selectedPresetPreviewMode:
        showingAuditionDraft ? rendererPreviewMode(selectedRendererCandidate) : String(selectedPresetCard && selectedPresetCard.preview
            && selectedPresetCard.preview.previewMode || "horizontal")
    readonly property bool presetPreviewActive:
        EditorModel.presetState(selectedPresetCard) !== "incompatible"
    readonly property LivePanelPreview activeRendererPreview:
        (presetPreviewLoader.item as LivePanelPreview) || embeddedRendererPreview

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
    readonly property string scene3DOffTier:
        CapabilityModel.scene3DOffTier(selectedCapabilityResolution, selectedPreviewTheme)
    readonly property bool scene3DControlsAvailable: Boolean(
        CapabilityModel.scene3DControlsAvailable(selectedCapabilityResolution,
            selectedPreviewTheme, embeddedRendererPreview.panelSceneItem.true3DCapability))
        && scene3DOffTier.length > 0
    readonly property bool scene3DQualityVisible: scene3DControlsAvailable
        && embeddedRendererPreview.panelSceneItem.effectiveRendererTier === "true3d"
    readonly property var scenePresentationMechanisms:
        CapabilityModel.scenePresentationMechanisms(selectedCapabilityResolution,
            selectedPreviewTheme, embeddedRendererPreview.panelSceneItem.effectiveRendererTier)
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
        const activeTheme = editorSession.themeDefinition || {};
        if (themeId.length === 0 || String(activeTheme.id || "") === themeId)
            return activeTheme;
        const themes = CapabilityModel.normalized(selectedResolvedThemes);
        for (let index = 0; index < themes.length; ++index) {
            if (String(themes[index].id || "") === themeId)
                return themes[index];
        }
        if (String(activeTheme.id || "") === themeId)
            return activeTheme;
        return {};
    }

    function themeRendererCandidate(theme) {
        return EditorModel.rendererThemeCandidate(editorSession, theme);
    }

    // The current draft drawn with one icon style, for the Icon Styles page.
    function iconStyleRendererCandidate(style) {
        // Copied so the native lists in the draft reach the preview as arrays.
        const candidate = EditorModel.copyValue(EditorModel.rendererCandidate(editorSession));
        candidate.iconStyle = String(style && style.id || "");
        candidate.iconStyleDefinition = panelRegistry.iconStyleDefinition(candidate.iconStyle);
        return candidate;
    }

    function resetRendererPreview(candidate) {
        previewMode = rendererPreviewMode(candidate);
        previewPresentationState = String(candidate.presentationMode
            || (candidate.presentation || {}).mode || "open");
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
        if (auditionBusy || hasPendingChanges) {
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
        return String(auditionActive ? panelValue("edge", "bottom") : panelRegistry.panelValue(selectedPanelId, "edge")
            || panelValue("edge", "bottom")) !== "free";
    }

    function refreshProjection() {
        if (!editorSession.loaded)
            return false;
        if (auditionBusy) {
            if (!auditionActive || Object.keys(editorSession.globalChanges || {}).length > 0) {
                studioError = qsTr("Global settings are unavailable during desktop audition.");
                return false;
            }
            const changes = EditorModel.copyValue(auditionCustomizations);
            const edits = EditorModel.copyValue(editorSession.panelChanges || {});
            for (const key of Object.keys(edits)) changes[key] = edits[key];
            const result = auditionService.updateDraft(changes);
            if (!auditionSucceeded(result)) return false;
            auditionCustomizations = changes;
            return loadAuditionEditor();
        }
        const projected = panelController.resolvePanelSettingsEditorDraft(editorSession.panelId, editorSession.revision, EditorModel.transactionPanelCandidate(editorSession), EditorModel.globalCandidate(editorSession), "studio");
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
        if (field.segmentIndex !== undefined) {
            const segment = (panelValue("segments", []) || [])[field.segmentIndex] || {};
            return field.entryId !== undefined ? (segment.entryIds || []).includes(field.entryId)
                : segment[field.segmentKey] === undefined ? field.fallback : segment[field.segmentKey];
        }
        if (field.rendererToggle === true)
            return String(panelValue("rendererTier", "")
                || (selectedCapabilityResolution.renderer || {}).requestedTier) === "true3d";
        return field.scope === "settings" ? globalValue(field.key, field.fallback) : panelValue(field.key, field.fallback);
    }

    function setFieldValue(field, value) {
        if (field.segmentIndex !== undefined) {
            const segments = EditorModel.copyValue(panelValue("segments", []));
            const segment = segments[field.segmentIndex];
            if (!segment) return;
            if (field.entryId !== undefined) {
                for (const item of segments)
                    item.entryIds = (item.entryIds || []).filter(function(id) { return id !== field.entryId; });
                if (value) segment.entryIds.push(field.entryId);
            } else {
                segment[field.segmentKey] = value;
                if (field.segmentKey === "source") segment.entryIds = [];
                if (field.segmentKey === "background" && value !== "solid") segment.corners = "inherited";
            }
            editorSession = EditorModel.setPanelValue(editorSession, "segments", segments);
            refreshProjection();
            return;
        }
        if (field.rendererToggle === true) {
            value = value ? "true3d" : scene3DOffTier;
            if (value.length === 0)
                return;
            // A volumetric ring needs a closed path. Keep this change in the
            // draft so Cancel still restores the saved artwork and layout.
            if (value === "true3d" && ["arc", "semicircle"].includes(panelValue("layout", "")))
                editorSession = EditorModel.setPanelValue(editorSession, "layout", "circular");
            else if (value === "baked2.5d" && panelValue("layout", "") === "circular"
                && (selectedPreviewTheme.tracks || []).some(track => track.shape === "arc"))
                editorSession = EditorModel.setPanelValue(editorSession, "layout", "arc");
        }
        if (field.scope === "settings") {
            editorSession = EditorModel.setGlobalValue(editorSession, field.key, value);
        } else {
            editorSession = EditorModel.setPanelValue(editorSession, field.key, value);
            if (field.key === "presentationMode") {
                if (value === "collapsed" && panelValue("collapseMechanism", "open") === "open") {
                    const preferred = previewMode === "vertical" ? "collapse-vertical" : "collapse-horizontal";
                    const mechanism = scenePresentationMechanisms.includes(preferred) ? preferred
                        : scenePresentationMechanisms.find(function(id) { return id !== "open"; });
                    if (mechanism) {
                        editorSession = EditorModel.setPanelValue(editorSession, "collapseMechanism", mechanism);
                        if (["collapse-horizontal", "collapse-vertical"].includes(mechanism))
                            editorSession = EditorModel.setPanelValue(editorSession, "collapseAxis",
                                mechanism === "collapse-vertical" ? "vertical" : "horizontal");
                    }
                }
                previewPresentationState = value;
            } else if (field.key === "collapseMechanism") {
                if (value === "open") {
                    editorSession = EditorModel.setPanelValue(editorSession, "presentationMode", "open");
                    previewPresentationState = "open";
                } else if (["collapse-horizontal", "collapse-vertical"].includes(value)) {
                    editorSession = EditorModel.setPanelValue(editorSession, "collapseAxis",
                        value === "collapse-vertical" ? "vertical" : "horizontal");
                }
            }
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
                if (String(source[index].section) !== sectionId)
                    continue;
                if (String(source[index].key).indexOf("scene3D") === 0 && !scene3DQualityVisible)
                    continue;
                if (String(source[index].key) === "bakedTilt"
                        && embeddedRendererPreview.panelSceneItem.effectiveRendererTier !== "baked2.5d")
                    continue;
                const row = editorRow(source[index]);
                // On a curved path the control has two ranges, and a slider
                // that seems to stop working above 8 needs saying so.
                if (String(source[index].key) === "spacing"
                        && ["circular", "ring", "ellipse", "radial", "polygon", "triangle",
                            "square", "pentagon", "hexagon", "octagon", "arc", "semicircle",
                            "fan"].includes(String(panelValue("layout", ""))))
                    row.description = qsTr("On a curved path, values below 8 draw the icons together and 0 makes them touch. From 8 up they are spread evenly, unless there are more icons than fit: then the value is the gap between the ones shown.");
                if (source[index].capability === "presentation-mechanism") {
                    if (!scenePresentationMechanisms.some(function(id) { return id !== "open"; }))
                        continue;
                    if (row.key === "collapseMechanism")
                        row.options = row.options.filter(function(option) {
                            return root.scenePresentationMechanisms.includes(option.value);
                        });
                }
                result.push(row);
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
        return [section(qsTr("Icons"), qsTr("Settings currently applied to icons in the selected panel."), true), readOnlyRow(qsTr("Style"), optionLabel("iconStyle", panelValue("iconStyle", "plain-original"))), readOnlyRow(qsTr("Size"), panelValue("iconSize", 52) + qsTr(" px")), readOnlyRow(qsTr("Spacing"), Math.round(Number(panelValue("spacing", 8))) + qsTr(" px")), readOnlyRow(qsTr("Shape"), optionLabel("iconShape", panelValue("iconShape", "rounded"))), readOnlyRow(qsTr("Animation"), optionLabel("iconAnimation", panelValue("iconAnimation", "scale"))), readOnlyRow(qsTr("Magnification"), globalValue("magnificationEnabled", true) ? Number(globalValue("magnification", 1.65)).toFixed(2) + "×" : qsTr("Off"))];
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
        rows.push({ kind: "actions", label: qsTr("3D"),
            description: qsTr("Drawing this panel in 3D, and every 3D setting, is on the 3D page."),
            actions: [{ label: qsTr("Open the 3D page"), icon: "view-preview", action: "open-3d-page",
                        available: true }] });
        rows.push(notice(qsTr("Built-in themes and imported artwork are on the Panel Themes / Skins page.")));
        return rows;
    }

    // The one home of 3D editing. A compatible free panel draws its own look
    // as a 3D platform; nothing here asks for a different theme.
    function panel3DRows() {
        const rows = [section(qsTr("3D"), qsTr("Draw this panel's own look as a 3D platform and place it in 3D."), true)];
        const capability = embeddedRendererPreview.panelSceneItem.true3DCapability;
        if (isNativePanel()) {
            rows.push(notice(qsTr("3D is available for free panels. Edge panels stay flat.")));
            return rows;
        }
        if (capability.rendererAvailable !== true) {
            rows.push(notice(qsTr("3D rendering is unavailable in this session. The 2D renderer remains available.")));
            return rows;
        }
        if (!scene3DControlsAvailable) {
            rows.push(notice(qsTr("This layout cannot stand on a 3D platform. Choose a ring, circle or polygon layout on the Layout page. Arcs and semicircles stay flat unless their theme brings its own 3D platform.")));
            rows.push({ kind: "actions", label: qsTr("Themes with their own 3D platform"), actions: [{
                label: qsTr("Browse themes"), icon: "preferences-desktop-theme",
                action: "browse-3d-themes", available: true }] });
            return rows;
        }
        rows.push({ kind: "switch", key: "rendererTier", scope: "panel", rendererToggle: true,
            label: qsTr("Enable 3D"),
            description: qsTr("Keeps this panel's theme and colours. Off returns to its own %1 surface.")
                .arg(CapabilityModel.rendererLabel(scene3DOffTier)) });
        if (!scene3DQualityVisible)
            return rows;
        const fields = fieldsForSection("panels-3d");
        const pick = function(keys) {
            return keys.map(function(key) {
                return fields.find(function(row) { return row.key === key; });
            }).filter(function(row) { return row !== undefined; });
        };
        rows.push(section(qsTr("Orientation and position"),
            qsTr("Pitch, yaw and roll turn the platform. Scene position moves it inside the panel's own area; where the panel sits on the desktop is set on the General page.")));
        rows.push.apply(rows, pick(["scene3DCameraPitch", "scene3DCameraYaw", "scene3DRoll",
            "scene3DPositionX", "scene3DPositionY", "scene3DPositionZ", "scene3DScale"]));
        rows.push({ kind: "actions", label: qsTr("Transform"), actions: [{
            label: qsTr("Reset 3D transform"), icon: "edit-reset", action: "reset-3d-transform",
            available: true }] });
        rows.push(section(qsTr("View and surface"), ""));
        rows.push.apply(rows, pick(["scene3DFieldOfView", "scene3DThickness", "scene3DIconElevation",
            "scene3DQuality"]));
        rows.push.apply(rows, fieldsForSection("icons-appearance").filter(function(row) {
            return row.key === "spacing";
        }));
        rows.push(section(qsTr("Lighting"), ""));
        rows.push.apply(rows, pick(["scene3DKeyLight", "scene3DFillLight"]));
        rows.push(section(qsTr("Motion"), ""));
        rows.push.apply(rows, pick(["scene3DTransitions", "scene3DFloat"]));
        rows.push(notice(qsTr("Whole-panel rotation works in 2D and 3D and is on the Animations page. Reduced motion stops the float and the animated changes.")));
        return rows;
    }

    // The neutral transform: no roll, centred, full size, and the look's own
    // viewing angle.
    function reset3DTransform() {
        for (const key of ["scene3DCameraPitch", "scene3DCameraYaw", "scene3DRoll",
                           "scene3DPositionX", "scene3DPositionY", "scene3DPositionZ", "scene3DScale"]) {
            const descriptor = fieldDescriptor(key, "panel");
            if (descriptor)
                editorSession = EditorModel.setPanelValue(editorSession, key, descriptor.defaultValue);
        }
        refreshProjection();
    }

    // Panel themes and skins are their own resource type, separate from
    // Panel Presets: a theme restyles this panel, it is not a whole panel.
    function panelThemeRows() {
        return [
            {
                kind: "themeSamples",
                label: qsTr("Panel Themes / Skins"),
                description: qsTr("Available themes are resolved by the backend for this panel. Load stages a theme in the draft; Apply saves it."),
                themes: CapabilityModel.availableItems(selectedResolvedThemes)
            },
            {
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
            }
        ];
    }

    // Icon styles are their own resource type, separate from Icon Presets.
    function iconStyleRows() {
        if (!fieldDescriptor("iconStyle", "panel")) {
            return [section(qsTr("Icon Styles"), qsTr("Reusable icon appearance sets."), true),
                notice(qsTr("Icon styles are not available for the resolved panel capabilities."))];
        }
        return [{
            kind: "iconStyleSamples",
            label: qsTr("Icon Styles"),
            description: qsTr("The installed icon styles, drawn on this panel. Load stages a style in the draft; Apply saves it."),
            styles: editorSession.iconStyles || []
        }];
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

    function panelAnimationRows() {
        const presentationKeys = ["presentationMode", "presentationTrigger", "collapseMechanism",
            "collapseAxis", "revealHandle", "openDelay", "closeDelay"];
        const opening = fieldsForSection("panels-behavior").filter(function(row) {
            return presentationKeys.includes(row.key);
        });
        const rows = [section(qsTr("Opening and closing"),
            qsTr("Choose the resting state, reveal trigger and motion. Preview is a draft until Apply."), true)];
        rows.push.apply(rows, opening);
        if (opening.length && !hasPendingChanges && !auditionBusy
                && Boolean(panelValue("visible", false)) && panelValue("presentationMode", "open") === "collapsed") {
            rows.push({kind: "actions", label: qsTr("Saved panel"), actions: [
                {action: "open-panel", label: qsTr("Open panel")},
                {action: "close-panel", label: qsTr("Close panel")}
            ]});
        }
        if (!opening.length)
            rows.push(notice(qsTr("This theme does not declare an opening or closing mechanism.")));
        rows.push.apply(rows, fieldsForSection("icons-behavior").filter(function(row) {
            return row.key === "animationDuration";
        }));
        rows.push(section(qsTr("Free panel rotation"),
            qsTr("Choose continuous clockwise or counterclockwise rotation, its speed, and when it runs.")));
        const rotation = fieldsForSection("panels-layout").filter(function(row) {
            return ["panelRotationMode", "panelRotationSpeed", "panelRotationTrigger"].includes(row.key);
        });
        rows.push.apply(rows, rotation);
        rows.push(notice(rotation.length
            ? qsTr("Hover the free panel and scroll up to turn clockwise, or down to turn counterclockwise. Wheel rotation works with continuous rotation off. Hover the preview to try hover-triggered continuous motion.")
            : qsTr("Rotation requires a free panel with a supported radial layout or closed theme track.")));
        return rows;
    }

    function iconTileRows() {
        const rows = schemaSectionRows("icon-tiles", qsTr("Icon Tiles"),
            qsTr("Tile backgrounds for this panel. Preview changes here, then Apply to save."));
        const custom = String(panelValue("iconTileMode", "style")) === "custom";
        const filtered = rows.filter(function(row) {
            return !row.key || row.key === "iconTilesEnabled" || row.key === "iconTileMode" || custom;
        });
        for (const row of filtered) {
            if (row.key === "iconTileMode") {
                row.options = [{value: "style", label: qsTr("From icon style")},
                    {value: "custom", label: qsTr("Custom tile")}];
            }
        }
        const descriptor = fieldDescriptor(custom ? "iconShape" : "iconStyle", "panel");
        if (descriptor) {
            const row = editorRow(descriptor);
            row.label = custom ? qsTr("Tile shape") : qsTr("Icon style");
            filtered.push(row);
        }
        filtered.push(notice(qsTr("Individual icons can override the tile default in Icon Properties. Custom tiles preserve the icon glyph and use the same tile in 2D and 3D.")));
        return filtered;
    }

    function panelSegmentRows() {
        const descriptor = fieldDescriptor("segments", "panel");
        const rows = [section(qsTr("Segments"), qsTr("Independent content groups. Changes apply together when you press Apply."), true)];
        if (!descriptor) {
            rows.push(notice(qsTr("Segments require a horizontal or vertical panel with a procedural surface.")));
            return rows;
        }
        const capabilities = descriptor.segmentCapabilities || {};
        const segments = panelValue("segments", []);
        const entries = descriptor.availableEntries || [];
        for (let index = 0; index < segments.length; ++index) {
            const segment = segments[index];
            rows.push(section(qsTr("%1. %2").arg(index + 1).arg(segment.id), "", false));
            function row(key, label, kind, fallback, choices) {
                const result = { segmentIndex: index, segmentKey: key, key: "segments",
                    scope: "panel", label: label, kind: kind, fallback: fallback };
                if (choices) result.options = root.choicesToOptions(choices);
                return result;
            }
            const sources = (capabilities.sources || []).filter(function(source) {
                return source === "custom" || source === segment.source || !segments.some(function(other, at) {
                    return at !== index && other.source === source && !(other.entryIds || []).length;
                });
            });
            rows.push(row("source", qsTr("Content source"), "combo", "inherited", sources));
            rows.push(row("background", qsTr("Background"), "combo", "inherited", capabilities.backgrounds));
            if (segment.background === "solid") {
                rows.push(row("color", qsTr("Color"), "color", "#202b36"));
                rows.push(row("corners", qsTr("Corners"), "combo", "inherited", capabilities.corners));
            }
            for (const key of ["padding", "spacing"]) {
                const item = row(key, key === "padding" ? qsTr("Padding") : qsTr("Spacing"), "spin", -1);
                item.from = -1; item.to = 64;
                item.description = qsTr("Use -1 to inherit the panel setting.");
                rows.push(item);
            }
            rows.push(row("presentation", qsTr("Resting state"), "combo", "open", ["open", "closed"]));
            const motion = row("motionProfile", qsTr("Surface motion on hover"), "combo", "", capabilities.motionProfiles);
            motion.options = motion.options.map(function(option) {
                return { value: option.value, label: option.value ? option.label : qsTr("None") };
            });
            rows.push(motion);
            if (segment.source === "custom" || segment.source === "status") {
                const selectable = entries.filter(function(entry) {
                    return (entry.isStatus === true) === (segment.source === "status");
                });
                if (segment.source === "status")
                    rows.push(notice(qsTr("Choose readings for this segment. With no selection, all available readings are shown.")));
                for (const entry of selectable) {
                    rows.push({ kind: "switch", scope: "panel", key: "segments", segmentIndex: index,
                        entryId: String(entry.appId), label: String(entry.displayName || entry.appId),
                        description: qsTr("Assign exclusively to this segment."), fallback: false });
                }
                if (!selectable.length) rows.push(notice(segment.source === "status"
                    ? qsTr("No system readings are currently available.")
                    : qsTr("Add content to this panel before assigning entries.")));
            }
            rows.push({ kind: "actions", label: qsTr("Order"), actions: [
                { action: "segment-up", segmentIndex: index, label: qsTr("Move up"), available: index > 0 },
                { action: "segment-down", segmentIndex: index, label: qsTr("Move down"), available: index + 1 < segments.length },
                { action: "segment-remove", segmentIndex: index, label: qsTr("Remove"), available: segments.length > 1 }
            ] });
        }
        rows.push({ kind: "action", action: "segment-add", label: qsTr("Add segment"),
            available: segments.length < Number(capabilities.maximumCount || 16) });
        return rows;
    }

    function rowsForCurrentPage() {
        // A preset page is a browser, not a settings form.
        if (currentPresetPage)
            return [];
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
            if (subTabIndex === 5)
                return panelSegmentRows();
            if (subTabIndex === 9)
                return panelAnimationRows();
            if (subTabIndex === 10)
                return panel3DRows();
            return panelThemeRows();
        }
        if (mainTabIndex === 2) {
            if (subTabIndex === 0)
                return schemaSectionRows("icons-appearance", qsTr("Appearance"), qsTr("Icon appearance for the selected panel."));
            if (subTabIndex === 1)
                return schemaSectionRows("icons-behavior", qsTr("Behavior"), qsTr("Icon motion and magnification."));
            if (subTabIndex === 2)
                return schemaSectionRows("icons-indicators", qsTr("Indicators"), qsTr("Running and attention markers."));
            if (subTabIndex === 3) {
                const rows = schemaSectionRows("icons-notifications", qsTr("Notifications"),
                    qsTr("Application badges, task progress and temporary launch feedback."));
                rows.push(notice(qsTr("Badges and progress appear when an application supplies them. Use Plasma widgets for desktop notifications, sound, Bluetooth and the clock.")));
                return rows;
            }
            return iconStyleRows();
        }
        if (mainTabIndex === 3)
            return iconTileRows();
        return unavailablePage(currentSubtabs.length > 0 ? currentSubtabs[subTabIndex] : qsTr("Profiles"), qsTr("Reusable profiles are outside the current settings contract."));
    }

    function stageArtifact(action, sourceUrl) {
        if (auditionBusy) {
            studioError = qsTr("Apply or cancel desktop audition before changing artwork.");
            return;
        }
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
            return panelRegistry.clearTheme(selectedPanelId);
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
        if (auditionBusy) {
            studioError = qsTr("Use Apply as Active or Cancel for the desktop preview.");
            return false;
        }
        studioError = "";
        studioWarning = "";
        let settingsCommitted = false;
        if (!editorSession.loaded)
            return false;

        if (hasSettingsChanges) {
            const result = panelController.applyPanelSettingsTransaction(editorSession.panelId, editorSession.revision, EditorModel.transactionPanelCandidate(editorSession), EditorModel.globalCandidate(editorSession));
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
            settingsCommitted = true;
        }

        if (!applyArtifact()) {
            const detail = String(panelRegistry.panelValue(selectedPanelId, "themeStatus") || qsTr("No details were returned."));
            studioError = settingsCommitted
                ? qsTr("The settings transaction completed, but the separate artwork operation failed: %1").arg(detail)
                : qsTr("Artwork operation failed: %1").arg(detail);
            return false;
        }
        artifactDraft = {};
        return loadEditor(selectedPanelId);
    }

    function acceptStudioChanges() {
        if (!auditionBusy && (!hasPendingChanges || applyStudioChanges()))
            close();
    }

    function cancelStudioChanges() {
        if (auditionBusy && !performAuditionAction("cancel", "")) return;
        discardStudioChanges();
        close();
    }

    function performStudioAction(action, data) {
        if (action === "browse-3d-themes") {
            setSubTab(6);
            return;
        }
        if (action === "open-3d-page") {
            setSubTab(10);
            return;
        }
        if (action === "reset-3d-transform") {
            reset3DTransform();
            return;
        }
        if (action === "open-panel" || action === "close-panel") {
            if (hasPendingChanges || auditionBusy || !Boolean(panelValue("visible", false))
                || panelValue("presentationMode", "open") !== "collapsed"
                || !scenePresentationMechanisms.some(function(id) {return id !== "open";})) return;
            if (!panelController.requestPanelPresentation(selectedPanelId, action === "open-panel" ? "open" : "collapse"))
                studioError = qsTr("The panel could not accept the presentation request.");
            return;
        }
        if (auditionBusy && ["create-free", "remove-panel", "import-theme", "render-theme", "clear-theme"].includes(action)) {
            studioError = qsTr("Apply or cancel desktop audition before changing panels or artwork.");
            return;
        }
        if (String(action).indexOf("segment-") === 0) {
            if (!fieldDescriptor("segments", "panel")) return;
            const segments = EditorModel.copyValue(panelValue("segments", []));
            const index = Number(data.segmentIndex);
            if (action === "segment-add" && segments.length < 16) {
                let number = 1;
                while (segments.some(function(segment) { return segment.id === "segment-" + number; })) ++number;
                segments.push({ id: "segment-" + number, source: "custom", order: segments.length,
                    entryIds: [], background: "solid", color: "#202b36", padding: -1,
                    spacing: -1, corners: "rounded", presentation: "open", motionProfile: "" });
            } else if (action === "segment-remove" && segments.length > 1 && index >= 0 && index < segments.length) {
                segments.splice(index, 1);
            } else if (["segment-up", "segment-down"].includes(action)) {
                const target = index + (action === "segment-up" ? -1 : 1);
                if (index < 0 || index >= segments.length || target < 0 || target >= segments.length) return;
                const moved = segments.splice(index, 1)[0];
                segments.splice(target, 0, moved);
            } else return;
            segments.forEach(function(segment, at) { segment.order = at; });
            editorSession = EditorModel.setPanelValue(editorSession, "segments", segments);
            refreshProjection();
            return;
        }
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
            const candidate = auditionActive
                ? (auditionStatus.editorProjection.themeCandidates || {})[String(data.themeId || "")]
                : panelRegistry.themeCandidate(selectedPanelId, String(data.themeId || ""), "complete");
            if (!candidate || candidate.success !== true) {
                studioError = qsTr("This theme cannot be loaded here: %1.")
                    .arg(CapabilityModel.reasonLabel(candidate ? candidate.errorCode : ""));
                return;
            }
            editorSession = EditorModel.stagePanelValues(editorSession, candidate.values || {});
            refreshProjection();
        } else if (action === "load-icon-style") {
            setFieldValue({ key: "iconStyle", scope: "panel" }, String(data.styleId || ""));
        } else if (action === "import-theme") {
            themeTargetPanelId = selectedPanelId;
            themeDialog.open();
        } else if (action === "render-theme") {
            stageArtifact("render", "");
        } else if (action === "clear-theme") {
            stageArtifact("clear", "");
        }
    }

    function auditionSucceeded(result) {
        if (result && result.success === true) {
            studioError = "";
            return true;
        }
        studioError = qsTr("Desktop preview action refused (%1).")
            .arg(String(result && result.errorCode || "unavailable"));
        return false;
    }

    function loadAuditionEditor() {
        if (!auditionActive) return false;
        const loaded = EditorModel.load(EditorModel.copyValue(auditionStatus.editorProjection || {}));
        if (!loaded.loaded) {
            studioError = qsTr("The desktop preview draft could not be loaded.");
            return false;
        }
        editorSession = loaded;
        resetRendererPreview(EditorModel.rendererCandidate(loaded));
        return true;
    }

    function startPresetAudition(presetId, applyImmediately) {
        if (!auditionService || !currentPresetPage) return false;
        if (auditionActive && auditionKind === currentPresetPage.kind && auditionPresetId === presetId)
            return applyImmediately ? performAuditionAction("apply", "") : true;
        if ((!auditionBusy && hasPendingChanges) || hasArtifactChanges) {
            studioError = qsTr("Apply or cancel the current draft before previewing a preset.");
            return false;
        }
        const originalPanelId = auditionOriginalPanelId || selectedPanelId;
        const result = auditionService.beginPreview({ kind: currentPresetPage.kind,
            presetId: presetId, panelId: originalPanelId,
            newPanel: currentPresetPage.kind === "panel" && auditionNewPanel,
            useRecommendedIcons: auditionNewPanel || auditionRecommendIcons });
        if (!auditionSucceeded(result)) return false;
        auditionOriginalPanelId = originalPanelId;
        auditionCustomizations = {};
        selectedPresetId = presetId;
        if (!loadAuditionEditor()) return false;
        return applyImmediately ? performAuditionAction("apply", "") : true;
    }

    function performAuditionAction(action, name) {
        if (!auditionService) return false;
        const originalPanelId = auditionOriginalPanelId || selectedPanelId;
        let result = null;
        if (action === "apply") result = auditionService.applyAsActive();
        else if (action === "save-custom") result = auditionService.saveAsCustomPreset(name);
        else if (action === "set-default" || action === "remove-default")
            result = auditionService.setAsDefault(auditionKind, auditionPresetId, action === "remove-default");
        else if (action === "cancel") result = auditionService.cancel();
        else if (action === "revert") result = auditionService.revert();
        else if (action === "restore-built-in") result = auditionService.restoreBuiltInDefaults();
        else return false;
        if (!auditionSucceeded(result)) return false;
        if (action === "apply" || action === "cancel" || action === "revert") {
            auditionCustomizations = {};
            auditionOriginalPanelId = "";
            internalPanelSelection = true;
            selectedPanelId = action === "apply" ? String(result.panelId || originalPanelId) : originalPanelId;
            internalPanelSelection = false;
            loadEditor(selectedPanelId);
        } else if (action === "restore-built-in") {
            auditionCustomizations = {};
            selectedPresetId = auditionPresetId;
            loadAuditionEditor();
        } else if (action === "save-custom") {
            presetNoticeText = qsTr("Saved a reusable custom preset. The desktop preview is still active.");
            presetNoticeIsError = false;
        }
        return true;
    }

    // Duplicate, rename and delete act on the user's preset store only. They
    // never touch an installed preset, a panel or the desktop.
    function performProfileAction(action, profileId, revision, value) {
        if (!profilesService || hasPendingChanges || auditionBusy) return;
        let result;
        if (action === "create") result = profilesService.createProfile(value);
        else if (action === "save") result = profilesService.saveProfile(profileId, revision);
        else if (action === "rename") result = profilesService.renameProfile(profileId, revision, value);
        else if (action === "duplicate") result = profilesService.duplicateProfile(profileId, revision, value);
        else if (action === "delete") result = profilesService.deleteProfile(profileId, revision);
        else if (action === "import") result = profilesService.importProfile(value);
        else if (action === "export") result = profilesService.exportProfile(profileId, revision, value);
        else if (action === "apply") result = profilesService.applyProfile(profileId, revision);
        else if (action === "recover") result = profilesService.recoverInterruptedApply();
        else if (action === "shortcuts-enabled") result = profilesService.setShortcutsEnabled(value === "true");
        else if (action === "shortcut-set") result = profilesService.setProfileShortcut(profileId, value);
        else if (action === "shortcut-clear") result = profilesService.clearProfileShortcut(profileId);
        else return;
        profileNoticeIsError = !(result && result.success === true);
        profileNotice = profileNoticeIsError
            ? qsTr("Profile action failed (%1).%2").arg(String(result && result.errorCode || "unknown-error"))
                .arg(result && result.rollbackStatus === "complete" ? qsTr(" The previous arrangement was restored.") : "")
            : action === "apply" ? qsTr("The profile arrangement was applied.")
                : action === "recover" ? qsTr("Profile recovery completed.") : qsTr("Profile action completed.");
        if (!profileNoticeIsError && (action === "apply" || action === "recover")) {
            internalPanelSelection = true;
            selectedPanelId = panelRegistry.activePanelId;
            internalPanelSelection = false;
            loadEditor(selectedPanelId);
        }
    }

    function performPresetAction(action, presetId, name) {
        const page = currentPresetPage;
        if (!page)
            return;
        let result = null;
        if (action === "duplicate")
            result = presetLibrary.duplicatePreset(page.kind, presetId, name);
        else if (action === "rename")
            result = presetLibrary.renamePreset(page.kind, presetId, name);
        else if (action === "remove")
            result = presetLibrary.removePreset(page.kind, presetId);
        else
            return;

        presetNoticeIsError = !(result && result.success === true);
        if (presetNoticeIsError) {
            presetNoticeText = qsTr("The preset store was not changed (%1).").arg(String(result && result.errorCode ? result.errorCode : "unknown-error"));
        } else if (action === "duplicate") {
            presetNoticeText = qsTr("Saved to %1 as “%2”.").arg(page.kind === "panel" ? qsTr("My Panel Presets") : qsTr("My Icon Presets")).arg(name);
        } else if (action === "rename") {
            presetNoticeText = qsTr("Renamed to “%1”.").arg(name);
        } else {
            presetNoticeText = qsTr("The preset was deleted.");
            if (selectedPresetId === presetId)
                selectedPresetId = "";
        }
    }

    onCurrentPresetPageChanged: {
        selectedPresetId = "";
        presetNoticeText = "";
        presetNoticeIsError = false;
    }

    onSelectedPanelIdChanged: {
        if (internalPanelSelection)
            return;
        if (auditionBusy || (editorSession.loaded && editorSession.panelId !== selectedPanelId && hasPendingChanges)) {
            const previousPanelId = auditionOriginalPanelId || editorSession.panelId;
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
        target: root.auditionService
        function onChanged() {
            if (root.auditionActive) root.loadAuditionEditor();
        }
    }

    Connections {
        target: panelController
        function onContentRevisionChanged() {
            if (!root.auditionBusy && root.visible && root.editorSession.loaded) {
                const snapshot = panelController.panelSettingsEditorSnapshot(root.selectedPanelId, "studio");
                if (!root.hasPendingChanges) root.editorSession = EditorModel.load(snapshot);
                else root.editorSession = EditorModel.withContentFeedback(root.editorSession, snapshot);
            }
        }
    }

    Connections {
        target: panelRegistry

        function onRevisionChanged() {
            if (!root.auditionBusy && !root.hasPendingChanges)
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
            if (!auditionBusy && !hasPendingChanges)
                loadEditor(selectedPanelId);
            requestActivate();
        }
    }
    onClosing: function(event) {
        if (auditionBusy && !performAuditionAction("cancel", "")) {
            event.accepted = false;
            return;
        }
        discardStudioChanges();
    }

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
            id: titleLead

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
            // Centred, so it may only be as wide as the narrower side allows:
            // a long panel name is elided instead of running under the
            // product name or the close button.
            width: Math.min(implicitWidth, Math.max(0, parent.width - 2 * (12
                + Math.max(titleLead.x + titleLead.width,
                           parent.width - closeButton.x))))
            text: panelRegistry.panelName(root.selectedPanelId) || qsTr("Panel Editor")
            color: "#d5e5ed"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
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
            Accessible.name: qsTr("Close Panel Studio")
            ToolTip.text: Accessible.name
            ToolTip.visible: hovered
            ToolTip.delay: 600
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
            Layout.preferredWidth: root.compactStudio ? 112 : 164
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
                        Layout.preferredHeight: root.height < 600 ? 32 : 48
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
                                elide: Text.ElideRight
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
                    visible: !root.compactStudio
                    color: "#617985"
                    font.pixelSize: 9
                    wrapMode: Text.Wrap
                }
            }
        }

        ScrollView {
            id: studioScroll
            objectName: "studio-page-scroll"
            implicitWidth: 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

        ColumnLayout {
            width: studioScroll.availableWidth
            height: Math.max(implicitHeight, studioScroll.availableHeight)
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
                    wheelEnabled: false
                    Accessible.name: qsTr("Panel")

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

                ToolButton {
                    visible: root.compactStudio
                    text: qsTr("Preview")
                    checkable: true
                    checked: root.expandedCompactPreview
                    onToggled: root.expandedCompactPreview = checked
                }
            }

            Rectangle {
                visible: !root.compactStudio || root.expandedCompactPreview
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

                    GridLayout {
                        columns: root.compactStudio ? 1 : 5
                        Layout.fillWidth: true
                        rowSpacing: 8
                        columnSpacing: 8

                        Label {
                            objectName: "panel-studio-preview-title"
                            text: root.auditionActive ? qsTr("Desktop preview active — saved settings unchanged") : root.presetPreviewActive
                                ? qsTr("Preset preview — %1").arg(String(root.selectedPresetCard.name || ""))
                                : qsTr("Live renderer preview")
                            color: "#e9f5fa"
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            Layout.maximumWidth: 320
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            wheelEnabled: false
                            id: previewModeSelector
                            Accessible.name: qsTr("Preview orientation")

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
                            // A preset is previewed in the mode it declares.
                            enabled: !root.presetPreviewActive
                            currentIndex: root.optionIndex(
                                model, root.presetPreviewActive
                                    ? root.selectedPresetPreviewMode
                                    : root.previewMode)
                            onActivated: root.previewMode = currentValue
                        }

                        ComboBox {
                            wheelEnabled: false
                            id: previewIconStateSelector
                            Accessible.name: qsTr("Preview icon state")

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
                        visible: !root.presetPreviewActive
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        panelDefinition: root.selectedRendererCandidate
                        orderedEntries: root.selectedRendererCandidate.segmentEntries || []
                        useProvidedEntries: root.selectedRendererCandidate.segmentEntries !== undefined
                        hostCapabilities:
                            root.selectedCapabilityResolution
                        themeDefinition: root.selectedPreviewTheme
                        iconStyleDefinition:
                            root.selectedRendererCandidate
                                .iconStyleDefinition || ({})
                        indicatorStyleDefinition:
                            root.selectedPreviewTheme.indicatorStyle || ({})
                        animationProfiles:
                            root.selectedRendererCandidate
                        previewMode: root.previewMode
                        presentationState:
                            root.previewPresentationState
                        animateRotation: root.mainTabIndex === 1 && root.subTabIndex === 9
                        stateEntry: root.previewStateEntry
                        iconState: root.previewIconState
                        contentMargin: 6
                    }

                    // The selected preset, drawn from its own record. It takes
                    // the place of the panel's preview and is never applied.
                    Loader {
                        id: presetPreviewLoader

                        visible: active
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: root.presetPreviewActive
                        sourceComponent: LivePanelPreview {
                            objectName: "panel-studio-preset-preview"
                            panelDefinition: root.selectedPresetCandidate
                            hostCapabilities:
                                root.selectedPresetCandidate
                                    .capabilityResolution || ({})
                            themeDefinition: root.selectedPresetTheme
                            iconStyleDefinition:
                                root.selectedPresetCandidate
                                    .iconStyleDefinition || ({})
                            indicatorStyleDefinition:
                                root.selectedPresetTheme.indicatorStyle || ({})
                            animationProfiles: root.selectedPresetCandidate
                            previewMode: root.selectedPresetPreviewMode
                            presentationState:
                                root.previewPresentationState
                            stateEntry: root.previewStateEntry
                            iconState: root.previewIconState
                            contentMargin: 6
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("Renderer: %1").arg(
                                root.activeRendererPreview.activeRendererTier)
                            color: "#86dff2"
                            font.pixelSize: 10
                        }

                        Label {
                            visible: root.activeRendererPreview.fallbackApplied
                            text: qsTr("Fallback: %1").arg(
                                root.activeRendererPreview.fallbackReason
                                || qsTr("unspecified"))
                            color: "#ffc66d"
                            font.pixelSize: 10
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Label {
                            objectName: "panel-studio-preview-status"
                            text: root.presetPreviewActive
                                ? qsTr("Preset preview only — no panel is changed")
                                : root.hasSettingsChanges
                                    ? qsTr("Draft only — desktop unchanged")
                                    : qsTr("Saved settings")
                            color: root.hasSettingsChanges
                                    || root.presetPreviewActive
                                ? "#80de70" : "#728995"
                            font.pixelSize: 10
                        }
                    }
                }
            }

            RowLayout {
                id: studioTabNavigation
                objectName: "studio-tabs-navigation"
                visible: root.currentSubtabs.length > 0
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                // KDE reserves this space in its padding; overlay styles
                // otherwise put the scrollbar over the navigation button.
                Layout.leftMargin: studioScroll.mirrored
                    ? Math.max(0, studioScroll.effectiveScrollBarWidth - studioScroll.leftPadding) : 0
                Layout.rightMargin: !studioScroll.mirrored
                    ? Math.max(0, studioScroll.effectiveScrollBarWidth - studioScroll.rightPadding) : 0
                spacing: 0
                readonly property bool overflowing: studioTabs.contentWidth
                    > width - studioTabs.leftPadding - studioTabs.rightPadding + 1

                function scrollTabs(direction) {
                    const view = studioTabs.contentItem
                    view.cancelFlick()
                    const start = view.originX
                    const end = start + Math.max(0, view.contentWidth - view.width)
                    view.contentX = Math.max(start, Math.min(end,
                        view.contentX + direction * Math.max(104, view.width * 0.8)))
                }

                ToolButton {
                    objectName: "studio-tabs-previous"
                    visible: studioTabNavigation.overflowing
                    enabled: studioTabs.contentItem.contentX > studioTabs.contentItem.originX + 0.5
                    text: qsTr("Scroll tabs left")
                    display: AbstractButton.IconOnly
                    icon.name: "go-previous"
                    ToolTip.text: text
                    ToolTip.visible: hovered
                    onClicked: studioTabNavigation.scrollTabs(-1)
                }

                TabBar {
                    id: studioTabs
                    objectName: "studio-page-tabs"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    currentIndex: root.subTabIndex
                    contentItem.clip: true
                    contentItem.implicitHeight: count > 0 && itemAt(0) ? itemAt(0).height : 0

                    ScrollInput {
                        parent: studioTabs
                        flickables: [studioTabs.contentItem,
                            studioForm.visible ? studioForm.contentItem
                                : presetBrowser.visible ? presetBrowser.scrollFlickable : null,
                            studioScroll.contentItem]
                    }

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

                ToolButton {
                    objectName: "studio-tabs-next"
                    visible: studioTabNavigation.overflowing
                    enabled: studioTabs.contentItem.contentX + studioTabs.contentItem.width
                        < studioTabs.contentItem.originX + studioTabs.contentItem.contentWidth - 0.5
                    text: qsTr("Scroll tabs right")
                    display: AbstractButton.IconOnly
                    icon.name: "go-next"
                    ToolTip.text: text
                    ToolTip.visible: hovered
                    onClicked: studioTabNavigation.scrollTabs(1)
                }
            }

            PresetAuditionBar {
                objectName: "panel-studio-audition-bar"
                visible: root.auditionBusy
                Layout.fillWidth: true
                auditionStatus: root.auditionStatus
                presetId: root.auditionPresetId
                resourceAvailable: root.auditionActive
                selectedDefault: root.auditionSelectedDefault
                guardError: root.auditionGuardError
                onActionRequested: function(action, name) { root.performAuditionAction(action, name); }
            }

            RowLayout {
                visible: root.currentPresetPage !== null && root.currentPresetPage.kind === "panel"
                Layout.fillWidth: true
                CheckBox {
                    objectName: "preset-audition-new-panel"
                    text: qsTr("Preview on a new panel")
                    checked: root.auditionNewPanel
                    enabled: !root.auditionBusy
                    onToggled: root.auditionNewPanel = checked
                }
                CheckBox {
                    objectName: "preset-audition-recommended-icons"
                    text: qsTr("Use recommended icons")
                    checked: root.auditionNewPanel || root.auditionRecommendIcons
                    enabled: !root.auditionBusy && !root.auditionNewPanel
                    onToggled: root.auditionRecommendIcons = checked
                }
            }

            StudioForm {
                id: studioForm
                objectName: "studio-page-form"
                visible: root.currentPresetPage === null && !root.currentProfilePage
                enabled: !root.profilesService || !root.profilesService.active
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 240
                studio: root
                rows: root.rowsForCurrentPage()
            }

            ProfilePage {
                objectName: "panel-studio-profile-page"
                visible: root.currentProfilePage
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 320
                profiles: root.profiles
                shortcutsOnly: root.subTabIndex === 2
                shortcutStatus: root.profilesService ? root.profilesService.shortcutStatus : ({enabled: false, bindings: []})
                applyStatus: root.profilesService ? root.profilesService.status : ({state: "IDLE"})
                actionsBlocked: !root.profilesService || root.hasPendingChanges || root.auditionBusy
                notice: root.profileNotice
                noticeIsError: root.profileNoticeIsError
                onActionRequested: function(action, profileId, revision, value) {
                    root.performProfileAction(action, profileId, revision, value);
                }
            }

            PresetBrowser {
                id: presetBrowser
                objectName: "panel-studio-preset-browser"
                visible: root.currentPresetPage !== null
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 360
                kind: root.currentPresetPage ? root.currentPresetPage.kind : "panel"
                scope: root.currentPresetPage ? root.currentPresetPage.scope : "builtin"
                presets: root.presetCards
                catalogStatus: root.currentPresetPage
                    ? presetLibrary.catalogStatus() : ({ valid: true })
                selectedPresetId: root.selectedPresetId
                noticeText: root.presetNoticeText
                noticeIsError: root.presetNoticeIsError
                auditionStatus: root.auditionStatus
                actionsEnabled: (!root.auditionBusy || root.auditionActive) && root.auditionGuardError.length === 0
                showAuditionBar: !root.auditionBusy
                selectedDefault: root.auditionSelectedDefault
                auditionGuardError: root.auditionGuardError
                onPreviewRequested: function(presetId) { root.startPresetAudition(presetId, false); }
                onApplyRequested: function(presetId) { root.startPresetAudition(presetId, true); }
                onAuditionActionRequested: function(action, name) { root.performAuditionAction(action, name); }
                onPresetSelected: function(presetId) {
                    root.selectedPresetId = presetId;
                }
                onDuplicateRequested: function(presetId, name) {
                    root.performPresetAction("duplicate", presetId, name);
                }
                onRenameRequested: function(presetId, name) {
                    root.performPresetAction("rename", presetId, name);
                }
                onRemoveRequested: function(presetId) {
                    root.performPresetAction("remove", presetId, "");
                }
            }
        }
            ScrollInput {
                parent: studioScroll
                excludedItems: [studioTabs]
                flickables: [studioForm.visible ? studioForm.contentItem
                    : presetBrowser.visible ? presetBrowser.scrollFlickable : null,
                    studioScroll.contentItem]
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
            visible: !root.compactStudio
            color: "#69808d"
            font.pixelSize: 10
        }

        // The message takes the room the footer has, on up to two lines. What
        // still does not fit is one hover away, and the whole text can be
        // copied for a report.
        Label {
            id: studioMessage

            objectName: "studio-message"
            visible: text.length > 0
            Layout.fillWidth: true
            Layout.maximumHeight: footer.height
            text: [root.studioError, root.studioWarning].filter(function(message) {
                return message.length > 0;
            }).join("  ·  ")
            color: root.studioError.length > 0 ? "#ff8c8c" : "#ffc66d"
            font.pixelSize: 11
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text

            HoverHandler {
                id: studioMessageHover
            }
            ToolTip.visible: studioMessageHover.hovered && studioMessage.truncated
            ToolTip.text: text
        }

        ToolButton {
            objectName: "studio-message-copy"
            visible: studioMessage.visible
            icon.name: "edit-copy"
            Accessible.name: qsTr("Copy this message")
            ToolTip.text: Accessible.name
            ToolTip.visible: hovered
            ToolTip.delay: 600
            onClicked: {
                messageClipboard.text = studioMessage.text;
                messageClipboard.selectAll();
                messageClipboard.copy();
                messageClipboard.text = "";
            }
        }

        TextEdit {
            id: messageClipboard

            visible: false
        }

        Label {
            visible: root.hasPendingChanges && root.studioError.length === 0 && root.studioWarning.length === 0
            text: qsTr("Pending changes")
            color: "#80de70"
            font.pixelSize: 10
        }

        Item {
            visible: !studioMessage.visible
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("OK")
            enabled: !root.auditionBusy
            icon.name: "dialog-ok"
            onClicked: root.acceptStudioChanges()
        }

        Button {
            text: qsTr("Apply")
            objectName: "studio-apply"
            icon.name: "dialog-ok-apply"
            enabled: !root.auditionBusy && root.hasPendingChanges
            onClicked: root.applyStudioChanges()
        }

        Button {
            text: qsTr("Cancel")
            objectName: "studio-cancel"
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
