pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import ArchDock.Rendering 1.0
import org.kde.kirigami as Kirigami
import "CapabilityModel.js" as CapabilityModel
import "PlacementStatus.js" as PlacementStatus
import "SettingsEditorModel.js" as EditorModel
import "StudioNavigation.js" as StudioNavigation
import "StudioThemes.js" as StudioThemes
import "VisibilityStatus.js" as VisibilityStatus

// Panel Studio: Arch Dock's settings window. It edits one panel at a time
// through a draft (SettingsEditorModel) that the backend validates on every
// change and saves on Apply as one transaction, with a live preview drawn by
// the shared renderer. Its pages cover the panel, its icons, themes, presets,
// profiles and 3D; only settings the panel's capabilities allow are shown.
Window {
    id: root

    // Panel Studio's own look (UI Themes); its controls take the theme's
    // colours from the window's palette.
    readonly property string studioThemeId: typeof dockSettings !== "undefined" && dockSettings
        ? String(dockSettings.studioTheme || "") : ""
    readonly property var studioTheme: StudioThemes.theme(studioThemeId)
    function chooseStudioTheme(themeId) {
        if (typeof dockSettings !== "undefined" && dockSettings)
            dockSettings.studioTheme = String(themeId);
    }
    readonly property bool currentThemesPage: mainTabIndex === 6
    // A bar's Plasma widgets are on its own Panels page, Widgets.
    readonly property bool currentWidgetsPage: mainTabIndex === 1 && subTabIndex === 12
    palette.window: studioTheme.windowBottom
    palette.windowText: studioTheme.text
    palette.base: studioTheme.controlBase
    palette.alternateBase: studioTheme.row
    palette.text: studioTheme.controlText
    palette.button: studioTheme.controlButton
    palette.buttonText: studioTheme.controlText
    palette.brightText: studioTheme.accentText
    palette.highlight: studioTheme.accent
    palette.highlightedText: studioTheme.accentText
    palette.placeholderText: studioTheme.controlPlaceholder
    palette.mid: studioTheme.controlBorder
    palette.midlight: studioTheme.rowBorderHover
    palette.dark: studioTheme.accent
    palette.light: studioTheme.textSecondary
    palette.shadow: "#000000"
    palette.toolTipBase: studioTheme.card
    palette.toolTipText: studioTheme.text
    palette.link: studioTheme.accent
    palette.disabled.windowText: studioTheme.textMuted
    palette.disabled.text: studioTheme.textMuted
    palette.disabled.buttonText: studioTheme.textMuted

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
    readonly property bool currentProfilePage: mainTabIndex === 5 && subTabIndex < 3
    // Panel Studio edits a panel of a saved profile, not the desktop, while
    // this names the profile: the same pages, on the profile's own copy of
    // the panel. Apply saves the profile; the desktop changes when the
    // profile is applied.
    property string profileId: ""
    readonly property bool profileMode: profileId.length > 0
    // The desktop panel to come back to from a profile.
    property string profileReturnPanelId: ""
    // Where the profile's panel is: "free", or its edge. A free panel's
    // editor values do not name it.
    property string profilePanelEdge: ""
    readonly property var editedProfile: {
        for (const profile of profiles)
            if (profile.id === profileId) return profile;
        return null;
    }
    // The edited panel's name as it is drafted.
    readonly property string editedPanelName: profileMode ? String(panelValue("name", ""))
        : String(root.panelValue("name", "")) || panelRegistry.panelName(root.selectedPanelId)

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
        if (!page || profileMode)
            return [];
        return presetsForSelectedPanel(page.kind, page.scope);
    }

    // The presets made for the selected panel. A free panel lists free-panel
    // presets and an edge panel the edge presets of its orientation: a free
    // panel has no use for horizontal or vertical panel presets. Icon
    // presets set only icon
    // values, which act on every panel.
    function presetsForSelectedPanel(kind, scope) {
        const revision = presetLibrary.revision;
        if (kind !== "panel")
            return presetLibrary.iconPresets(scope);
        const free = !isNativePanel();
        const edge = String(panelValue("edge", "bottom"));
        const orientation = free ? "free"
            : edge === "left" || edge === "right" ? "vertical" : "horizontal";
        const cards = scope === "all"
            ? Array.from(presetLibrary.panelPresets("builtin")).concat(Array.from(presetLibrary.panelPresets("user")))
            : presetLibrary.panelPresets(scope);
        return cards.filter(function(card) {
            const freePreset = String(card.hostKind || "") === "free-desktop";
            return free ? freePreset
                : !freePreset && (card.orientations || []).includes(orientation);
        });
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
        (presetPreviewLoader.item as LivePanelPreview)
        || (newPanelPreviewLoader.item as LivePanelPreview) || embeddedRendererPreview

    readonly property int panelRevision: panelRegistry.revision
    readonly property bool barSelected: {
        const revision = panelRevision;
        return String(panelRegistry.panelValue(selectedPanelId, "edgeHost") || "") === "bar";
    }
    // The bar's widgets in their order along it, read while its page is shown.
    readonly property var selectedBarWidgets: {
        const revision = panelRevision;
        return currentWidgetsPage && barSelected && !profileMode
            ? Array.from(panelController.barWidgetList(selectedPanelId) || []) : [];
    }
    // Plasma's installed widgets, read again each time the list to add from opens.
    property var installedBarWidgets: []
    property string barWidgetsNotice: ""
    function changeBarWidgets(done) {
        barWidgetsNotice = done ? "" : qsTr("The bar's widgets could not be changed just now. Try again in a moment.");
    }
    readonly property int placementRevision: panelController.nativePlacementRevision
    readonly property int nativeVisibilityRevision: panelController.nativeVisibilityRevision
    readonly property bool hasSettingsChanges: EditorModel.dirty(editorSession)
    readonly property bool hasArtifactChanges: String(artifactDraft.action || "").length > 0
    readonly property bool hasPendingChanges: hasSettingsChanges || hasArtifactChanges
    readonly property var selectedCapabilityResolution: editorSession.capabilityResolution || ({})
    readonly property var selectedResolvedThemes: editorSession.themes || []
    readonly property var selectedRendererCandidate:
        EditorModel.rendererCandidate(editorSession)
    // The Studio edits one panel: the one whose "Configure Arch Dock…" opened
    // it, as Plasma's own panel settings do. Its "Add panel…" opens the new
    // panel page instead: a free panel, or one on any screen edge, as many on
    // an edge as wanted; the preview shows the bare panel, a free one in the
    // default panel preset's look when one for free panels is set. Once
    // added, the window edits the new panel.
    readonly property var newPanelKinds: ["free", "top", "bottom", "left", "right"]
    // The new panel's place: "free", "top", "bottom", "left" or "right", or
    // "" while the window edits a panel.
    property string newPanelEdge: ""
    readonly property bool newPanelPicked: newPanelEdge.length > 0
    // The screen edges where a panel already stands (Arch Dock's, Plasma's
    // own or another dock's), read when the new panel page opens: there is
    // one panel to an edge, so those cannot take a new one.
    property var occupiedEdges: []
    function newPanelPlaceFree(edge) {
        return edge === "free" || !occupiedEdges.includes(edge);
    }
    // The place of the panel the window edits, as the new panel page names it.
    readonly property string selectedPanelKind: {
        const revision = panelRevision;
        const edge = String((profileMode ? panelValue("edge", profilePanelEdge) : panelRegistry.panelValue(selectedPanelId, "edge")) || "");
        return newPanelKinds.includes(edge) ? edge : "";
    }
    function newPanelKindName(edge) {
        return edge === "free" ? qsTr("Free panel")
            : edge === "top" ? qsTr("Top edge")
            : edge === "bottom" ? qsTr("Bottom edge")
            : edge === "left" ? qsTr("Left edge")
            : edge === "right" ? qsTr("Right edge") : "";
    }
    function newPanelDescription(edge) {
        return edge === "free" ? qsTr("Add a new, empty free panel to the desktop")
            : qsTr("Add a new, empty panel on the screen's %1 edge").arg(edge);
    }
    function newPanelName(edge) {
        return edge === "free" ? qsTr("New free panel")
            : edge === "top" ? qsTr("New top panel")
            : edge === "bottom" ? qsTr("New bottom panel")
            : edge === "left" ? qsTr("New left panel")
            : edge === "right" ? qsTr("New right panel") : "";
    }
    // The panel list and its button belong to Panels; the preview to the
    // pages that show the panel: Overview, Icons,
    // Icon Tiles and Pedestals. In Panels it shows only the new panel being
    // added or a preset being previewed.
    readonly property bool previewSection: mainTabIndex === 0 || mainTabIndex === 2
        || mainTabIndex === 3 || mainTabIndex === 4
        || (mainTabIndex === 1 && (newPanelPicked || presetPreviewActive))
    readonly property var newFreePanelCard: {
        const revision = presetLibrary.revision;
        const presetId = String(auditionDefaults.panelPresetId || "");
        if (presetId.length === 0)
            return null;
        const cards = Array.from(presetLibrary.panelPresets("builtin"))
            .concat(Array.from(presetLibrary.panelPresets("user")));
        return cards.find(function(card) {
            return String(card.id) === presetId
                && String((EditorModel.presetRendererCandidate(card, true) || {}).edge || "") === "free";
        }) || null;
    }
    readonly property var newFreePanelDefinition: newFreePanelCard
        ? EditorModel.presetRendererCandidate(newFreePanelCard, true)
        : ({ edge: "free", type: "empty", layout: "circular", layoutRadius: 145,
             width: 420, height: 420, rendererTier: "procedural2d" })
    readonly property var newFreePanelTheme: newFreePanelCard
        ? EditorModel.presetPreviewTheme(newFreePanelCard) : ({})
    // The picked new panel as the preview draws it: a new edge panel is
    // empty, and drawn bare along its edge.
    readonly property var newPanelDefinition: newPanelEdge === "free" ? newFreePanelDefinition
        : ({ edge: newPanelEdge, type: "empty", rendererTier: "procedural2d" })
    // A panel along an edge takes its length from what is on it: with no
    // applications yet it had nothing to draw, and the preview of a hidden
    // Bottom panel stayed empty. It is shown bare.
    readonly property bool previewShowsBarePanel: !presetPreviewActive && previewMode !== "free"
        && selectedRendererCandidate.segmentEntries !== undefined
        && selectedRendererCandidate.segmentEntries.length === 0
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
    // Plasma's own state of the panel: a profile's copy has none.
    readonly property var selectedPlacementResult: {
        const revision = placementRevision;
        return profileMode ? ({}) : panelController.nativePanelPlacementStatus(selectedPanelId);
    }
    readonly property var selectedVisibilityResult: {
        const revision = nativeVisibilityRevision;
        return profileMode ? ({}) : panelController.nativePanelVisibilityStatus(selectedPanelId);
    }
    readonly property var currentSubtabs: StudioNavigation.subtabsFor(mainTabIndex)
    readonly property var mainTabLabels: StudioNavigation.mainLabels()
    // One icon per section of StudioNavigation.sections, in order.
    // UI Themes wears Arch Dock's own logo.
    readonly property var mainTabIcons: ["view-dashboard", "preferences-desktop-display", "preferences-desktop-icons", "draw-rectangle", "draw-ellipse", "document-save", "org.archdock.ArchDock"]

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
            || (profileMode ? panelValue("edge", profilePanelEdge) : panelRegistry.panelValue(selectedPanelId, "edge"))
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

    function resetRendererPreview(candidate) {
        previewMode = rendererPreviewMode(candidate);
        previewPresentationState = String(candidate.presentationMode
            || (candidate.presentation || {}).mode || "open");
        previewStateEntry = 1;
        previewIconState = "normal";
    }

    // The snapshot the editor was last built from. Rebuilding the form and
    // its live preview is costly, so a revision that leaves this panel's
    // snapshot as it was does not rebuild anything.
    property string loadedSnapshotText: ""
    property bool editorReloadQueued: false

    // Every registry or content revision asks for one reload; they are
    // coalesced into one per event-loop turn.
    function queueEditorReload() {
        if (editorReloadQueued) return;
        editorReloadQueued = true;
        Qt.callLater(function() {
            root.editorReloadQueued = false;
            // A hidden Studio reloads when it is shown; an audition owns the draft.
            if (root.auditionBusy || !root.visible) return;
            const snapshot = root.editorSnapshot(root.selectedPanelId);
            const text = JSON.stringify(snapshot);
            if (text === root.loadedSnapshotText && root.editorSession.loaded) return;
            if (!root.hasPendingChanges) root.loadEditor(root.selectedPanelId, snapshot, text);
            else if (snapshot && snapshot.success === true && root.editorSession.loaded
                     && Number(snapshot.revision) !== Number(root.editorSession.revision)) {
                // The panel's settings changed outside Panel Studio while
                // its changes were not applied: they are kept on the new
                // settings, and the desktop shows them so.
                root.loadedSnapshotText = text;
                root.editorSession = EditorModel.rebase(root.editorSession, snapshot);
                root.refreshProjection();
            } else {
                root.loadedSnapshotText = text;
                root.editorSession = EditorModel.withContentFeedback(root.editorSession, snapshot);
            }
        });
    }

    function loadEditor(panelId, prefetched, prefetchedText) {
        const normalizedPanelId = String(panelId || "");
        if (normalizedPanelId.length === 0) {
            editorSession = EditorModel.emptySession("panel-not-found", qsTr("No panel is selected."));
            loadedSnapshotText = "";
            return false;
        }
        projectionTimer.stop();
        livePreviewShown = false;
        const snapshot = prefetched || editorSnapshot(normalizedPanelId);
        loadedSnapshotText = prefetchedText || JSON.stringify(snapshot);
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

    // The new panel page (a panel's "Add panel…"). The panel being edited
    // keeps its changes until they are applied or dropped: while some are
    // not, the user is asked first.
    function openNewPanelPage(edge) {
        occupiedEdges = Array.from(panelController.occupiedScreenEdges() || []);
        const wanted = newPanelKinds.includes(String(edge)) ? String(edge) : "free";
        const place = newPanelPlaceFree(wanted) ? wanted : "free";
        if (auditionBusy) {
            studioError = qsTr("Apply or cancel desktop audition before adding a panel.");
            return false;
        }
        if (hasPendingChanges) {
            askBeforeLeaving("new:" + place);
            return false;
        }
        if (!leaveProfileMode())
            return false;
        newPanelEdge = place;
        studioError = "";
        setMainTab(1);
        return true;
    }
    // Leaving a panel with changes that are not applied: Apply, Discard or
    // Keep editing. `target` is a panel id, or "new:<place>".
    property string pendingSwitch: ""
    function askBeforeLeaving(target) {
        pendingSwitch = String(target);
        unappliedChangesDialog.open();
    }
    // "Configure Arch Dock…" on a panel: the window edits that panel, from
    // its first page; one with changes not applied yet asks first.
    function editPanel(panelId) {
        const id = String(panelId || "");
        if (id.length === 0)
            return false;
        if (id !== selectedPanelId && auditionBusy) {
            studioError = qsTr("Apply or cancel desktop audition before editing another panel.");
            return false;
        }
        if ((id !== selectedPanelId || profileMode) && hasPendingChanges) {
            askBeforeLeaving(id);
            return false;
        }
        newPanelEdge = "";
        continueSwitch(id);
        return true;
    }
    function continueSwitch(target) {
        const place = String(target);
        if (place === "profiles") {
            leaveProfileMode();
        } else if (place.startsWith("profile:")) {
            const parts = place.slice(8).split("/");
            editProfilePanel(parts[0], parts.slice(1).join("/"));
        } else if (place.startsWith("new:")) {
            leaveProfileMode();
            openNewPanelPage(place.slice(4));
        } else {
            leaveProfileMode();
            if (selectPanel(target)) {
                setMainTab(1);
                setSubTab(0);
            }
        }
    }

    // A panel of a saved profile, edited on the profile's own copy; its
    // changes not applied yet are asked about first.
    function editProfilePanel(profile, panelId) {
        const id = String(panelId || "");
        if (!profilesService || String(profile || "").length === 0 || id.length === 0)
            return false;
        if (auditionBusy) {
            studioError = qsTr("Apply or cancel desktop audition before editing a profile.");
            return false;
        }
        if (hasPendingChanges) {
            askBeforeLeaving("profile:" + profile + "/" + id);
            return false;
        }
        clearLivePreview();
        if (!profileMode)
            profileReturnPanelId = selectedPanelId;
        const listed = Array.from(profilesService.profilePanels(profile) || [])
            .find(function(panel) { return String(panel.id) === id; });
        profilePanelEdge = listed ? String(listed.edge || "") : "";
        newPanelEdge = "";
        artifactDraft = {};
        internalPanelSelection = true;
        profileId = String(profile);
        selectedPanelId = id;
        internalPanelSelection = false;
        if (!loadEditor(id)) {
            leaveProfileMode();
            return false;
        }
        setMainTab(1);
        setSubTab(0);
        return true;
    }
    // Back to the desktop's panels, on the Profiles page.
    function leaveProfileMode() {
        if (!profileMode)
            return true;
        if (hasPendingChanges) {
            askBeforeLeaving("profiles");
            return false;
        }
        const back = profileReturnPanelId.length > 0 && Array.from(panelRegistry.panelIds).includes(profileReturnPanelId)
            ? profileReturnPanelId : panelRegistry.activePanelId;
        profileId = "";
        profileReturnPanelId = "";
        profilePanelEdge = "";
        internalPanelSelection = true;
        selectedPanelId = back;
        internalPanelSelection = false;
        loadEditor(back);
        setMainTab(5);
        setSubTab(0);
        return true;
    }
    // The sidebar: Profiles, while a profile's panel is edited, goes back to
    // the desktop's panels.
    function chooseMainTab(index) {
        if (profileMode && Number(index) === 5) {
            leaveProfileMode();
            return;
        }
        setMainTab(index);
    }

    function selectPanel(panelId) {
        const normalizedPanelId = String(panelId || "");
        if (normalizedPanelId.length === 0 || normalizedPanelId === selectedPanelId)
            return true;
        if (auditionBusy || hasPendingChanges) {
            studioError = qsTr("Apply or cancel the current draft before switching panels.");
            return false;
        }
        newPanelEdge = "";
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

    // Whether a tab has anything to change on the selected panel. A tab with
    // nothing to change is not shown; tabs keep their
    // indices.
    function subtabAvailable(sectionIndex, subtabIndex) {
        const sectionNumber = Number(sectionIndex);
        const subtabNumber = Number(subtabIndex);
        const presetPage = StudioNavigation.presetPage(sectionNumber, subtabNumber);
        if (sectionNumber === 0 && subtabNumber === 2)
            return tilesApplied;
        if (sectionNumber === 0 && subtabNumber === 3)
            return standsApplied;
        if (presetPage) {
            // Presets are tried on the desktop, so a profile's panel has no
            // Presets page; its decorations are on Appearance.
            if (profileMode)
                return false;
            // Your own catalog appears once it holds a preset for this panel.
            return presetPage.scope !== "user"
                || presetsForSelectedPanel(presetPage.kind, "user").length > 0;
        }
        if (sectionNumber === 1) {
            // Their presets are on Presets.
            if (subtabNumber === 7 || subtabNumber === 8)
                return false;
            if (subtabNumber === 1)
                return fieldsForSection("panels-size").length > 0;
            if (subtabNumber === 4)
                return fieldsForSection("panels-layout").length > 0;
            if (subtabNumber === 5)
                return fieldDescriptor("segments", "panel") !== null;
            if (subtabNumber === 9)
                return panelAnimationRows().length > 0;
            if (subtabNumber === 11)
                return fieldsForSection("panels-hologram").length > 0;
            // A bar's own Plasma widgets; a profile's copy of a bar keeps the
            // ones it was saved with.
            if (subtabNumber === 12)
                return barSelected && !profileMode;
            if (subtabNumber === 10)
                return scene3DControlsAvailable || (!isNativePanel()
                    && EditorModel.platformTier(panelValue("rendererTier", "")
                        || (selectedCapabilityResolution.renderer || {}).requestedTier));
        }
        if (sectionNumber === 2) {
            if (subtabNumber === 2)
                return fieldsForSection("icons-indicators").length > 0;
            if (subtabNumber === 3)
                return fieldsForSection("icons-notifications").length > 0;
            if (subtabNumber === 6)
                return fieldsForSection("icons-hologram").length > 0;
            // The icons' own 3D, on a panel drawn in 3D.
            if (subtabNumber === 7)
                return fieldsForSection("icons-3d").length > 0;
        }
        // The stands' hologram where stands are drawn.
        if (sectionNumber === 4 && subtabNumber === 1)
            return fieldsForSection("pedestals-hologram").length > 0;
        // The stands' own 3D, on every panel: its shape is always there.
        if (sectionNumber === 4 && subtabNumber === 2)
            return pedestal3DRows().length > 1;
        return true;
    }

    // The page shown is always one that is offered for the selected panel.
    function ensureOfferedSubtab() {
        if (subtabAvailable(mainTabIndex, subTabIndex))
            return;
        for (let index = 0; index < currentSubtabs.length; ++index) {
            if (subtabAvailable(mainTabIndex, index)) {
                setSubTab(index);
                return;
            }
        }
    }
    onEditorSessionChanged: Qt.callLater(root.ensureOfferedSubtab)
    onMainTabIndexChanged: {
        // A new panel is picked in Panels and left there.
        if (mainTabIndex !== 1)
            newPanelEdge = "";
        Qt.callLater(root.ensureOfferedSubtab);
    }
    onSubTabIndexChanged: Qt.callLater(root.ensureOfferedSubtab)

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
        if (profileMode)
            return String(panelValue("edge", profilePanelEdge || "bottom")) !== "free";
        return String(auditionActive ? panelValue("edge", "bottom") : panelRegistry.panelValue(selectedPanelId, "edge")
            || panelValue("edge", "bottom")) !== "free";
    }

    // A slider sends a value for every step of a drag. The draft takes each
    // value at once and the preview follows it; the backend's check of the
    // draft runs when the drag pauses. Each check takes about a tenth of a
    // second, and run on every step it stalled the window. Apply runs a pending check first.
    Timer {
        id: projectionTimer
        interval: 150
        onTriggered: root.refreshProjection()
    }

    function flushProjection() {
        if (projectionTimer.running)
            refreshProjection();
    }

    function refreshProjection() {
        projectionTimer.stop();
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
        // Each change is shown on the desktop panel as it is made; Apply or
        // OK keeps it and Cancel drops it.
        const panelValues = EditorModel.transactionPanelCandidate(editorSession);
        const globalValues = EditorModel.globalCandidate(editorSession);
        const projected = profileMode
            ? panelController.resolveProfilePanelEditorDraft(profileId, editorSession.panelId, editorSession.revision,
                panelValues, "studio")
            : livePreviewAvailable
            ? panelController.previewPanelSettingsEditorDraft(editorSession.panelId, editorSession.revision,
                panelValues, globalValues, "studio")
            : panelController.resolvePanelSettingsEditorDraft(editorSession.panelId, editorSession.revision,
                panelValues, globalValues, "studio");
        if (!projected || projected.success !== true) {
            editorSession = EditorModel.retainFailure(editorSession, projected);
            studioError = qsTr("Draft validation failed: %1 (%2)").arg(String(projected && projected.status ? projected.status : "failed")).arg(String(projected && (projected.errorMessage || projected.errorCode) ? (projected.errorMessage || projected.errorCode) : qsTr("No details were returned.")));
            return false;
        }
        editorSession = EditorModel.withProjection(editorSession, projected);
        studioError = "";
        livePreviewShown = projected.previewShown === true;
        // Apply would refuse this change, so the desktop keeps the last one
        // it can show.
        studioWarning = String(projected.previewRefusal || "").length > 0
            ? qsTr("Not shown on the desktop, and Apply refuses it: %1.").arg(projected.previewRefusal) : "";
        return true;
    }
    readonly property bool livePreviewAvailable: !profileMode
        && typeof panelController.previewPanelSettingsEditorDraft === "function"
    function editorSnapshot(panelId) {
        return profileMode ? panelController.profilePanelEditorSnapshot(profileId, panelId, "studio")
            : panelController.panelSettingsEditorSnapshot(panelId, "studio");
    }
    // Whether the desktop shows the draft now.
    property bool livePreviewShown: false
    function clearLivePreview() {
        livePreviewShown = false;
        if (livePreviewAvailable)
            panelController.clearPanelSettingsPreview();
    }

    // A palette's colours: as chosen (its colours in order, separated by
    // commas), or as designed.
    function paletteColours(field) {
        const text = String(field.scope === "settings" ? globalValue(field.key, "") : panelValue(field.key, "") || "").trim();
        const colours = text.length > 0 ? text.split(",") : (field.designed || []);
        return colours.map(function(colour) { return String(colour).trim(); });
    }

    function fieldValue(field) {
        if (field.paletteIndex !== undefined)
            return paletteColours(field)[field.paletteIndex] || "";
        if (field.segmentIndex !== undefined) {
            const segment = (panelValue("segments", []) || [])[field.segmentIndex] || {};
            return field.entryId !== undefined ? (segment.entryIds || []).includes(field.entryId)
                : segment[field.segmentKey] === undefined ? field.fallback : segment[field.segmentKey];
        }
        if (field.rendererToggle === true)
            return String(panelValue("rendererTier", "")
                || (selectedCapabilityResolution.renderer || {}).requestedTier) === "true3d";
        if (field.platformToggle === true)
            return EditorModel.platformTier(panelValue("rendererTier", "")
                || (selectedCapabilityResolution.renderer || {}).requestedTier);
        return field.scope === "settings" ? globalValue(field.key, field.fallback) : panelValue(field.key, field.fallback);
    }

    function setFieldValue(field, value) {
        // Holographic stands asked for while the stands have no shape: they
        // get the projector, the stand a hologram rises from.
        if (String(field.key) === "hologramPedestals" && value === true
                && String(panelValue("pedestalShape", "none")) === "none") {
            const shape = fieldDescriptor("pedestalShape", "panel");
            if (shape)
                setFieldValue(shape, "projector");
        }
        if (field.paletteIndex !== undefined) {
            // One colour of a palette: the palette with it changed.
            const colours = paletteColours(field);
            colours[field.paletteIndex] = String(value);
            const whole = Object.assign({}, field);
            delete whole.paletteIndex;
            setFieldValue(whole, colours.join(","));
            return;
        }
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
        if (field.platformToggle === true) {
            if (!value) {
                editorSession = EditorModel.platformOff(editorSession);
                refreshProjection();
                return;
            }
            editorSession = EditorModel.rememberFlatLook(editorSession);
            value = "true3d";
        }
        if (field.rendererToggle === true) {
            value = value ? "true3d" : scene3DOffTier;
            if (!EditorModel.platformTier(value)) {
                editorSession = EditorModel.platformOff(editorSession);
                refreshProjection();
                return;
            }
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
            const previous = String(panelValue(field.key, ""));
            editorSession = EditorModel.setPanelValue(editorSession, field.key, value);
            const pair = lookPairs[field.key];
            if (pair !== undefined)
                editorSession = EditorModel.setPanelValue(editorSession, pair.key, pair.value(value));
            // Imported picture chosen with no picture imported yet: the
            // chooser opens at once; cancelled, the previous choice comes back.
            if (value === "picture" && pictureFor[field.key] !== undefined
                    && !String(panelValue(pictureFor[field.key], "")).length) {
                pictureFallback = { key: field.key, value: previous };
                importPicture({ key: pictureFor[field.key] });
            }
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
        if (field.kind === "slider")
            projectionTimer.restart();
        else
            refreshProjection();
    }

    function fieldOptionIndex(field) {
        return optionIndex(field.options || [], fieldValue(field));
    }

    function formatFieldValue(field, value) {
        if (field.specialValueText && Number(value) === Number(field.from))
            return field.specialValueText;
        const decimals = field.decimals === undefined ? 0 : field.decimals;
        const numeric = Number(value) * (field.displayScale === undefined ? 1 : field.displayScale);
        const text = decimals > 0 ? numeric.toFixed(decimals) : Math.round(numeric);
        return (field.prefix || "") + text + (field.suffix || "");
    }

    function parseFieldValue(field, text, fallback) {
        let value = String(text).trim();
        if (field.specialValueText && value === field.specialValueText)
            return Number(field.from);
        if (field.prefix && value.startsWith(field.prefix))
            value = value.slice(field.prefix.length);
        if (field.suffix && value.endsWith(field.suffix))
            value = value.slice(0, -field.suffix.length);
        const numeric = Number(value.trim());
        return isFinite(numeric) && value.trim().length > 0 ? numeric : fallback;
    }

    function displayFieldValue(field) {
        return field.value !== undefined ? String(field.value) : String(fieldValue(field));
    }

    function colorDisplayValue(field) {
        const value = String(fieldValue(field) || "").trim();
        return value.length > 0 ? value : field.emptyText || qsTr("Default");
    }

    // A colour as chosen; one not chosen shows what it is as designed.
    function colorPreviewValue(field) {
        const value = String(fieldValue(field) || "").trim() || String(field.designed || "");
        return /^#(?:[0-9a-f]{3,4}|[0-9a-f]{6}|[0-9a-f]{8})$/i.test(value) ? value : "transparent";
    }

    function openColorEditor(field) {
        colorField = field;
        const current = String(fieldValue(field) || "").trim();
        colorDialog.selectedColor = current.length > 0 ? current : "#334455";
        colorDialog.open();
    }

    // Whether the selected panel offers a field now (Apply refuses others).
    function fieldOffered(key) {
        const descriptor = fieldDescriptor(key, "panel");
        return descriptor !== null && descriptor.inactive !== true;
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

    // One Look for each surface. The first key is shown
    // as Look; setting it sets its pair, which is not shown.
    readonly property var lookPairs: ({
        scene3DMaterial: { key: "scene3DTexture", value: function(look) { return look; } },
        pedestalMaterial: { key: "pedestalTexture", value: function(look) { return "material"; } },
        folderPedestalMaterial: { key: "folderPedestalTexture", value: function(look) { return "material"; } },
        iconTileTexture: { key: "iconTileMaterial", value: function(look) { return look === "none" ? "minimal" : look; } }
    })
    readonly property var lookPairedKeys: ["scene3DTexture", "pedestalTexture", "folderPedestalTexture", "iconTileMaterial"]
    // A look list's entries: Default is the surface a panel's own design
    // gives it, Imported picture the picture the user imported.
    function lookLabel(value) {
        const names = { theme: qsTr("Default"), picture: qsTr("Imported picture"), panel: qsTr("Same as the panel"),
                        none: qsTr("None"), "floating-glass": qsTr("Floating glass") };
        return names[value] !== undefined ? names[value] : titleCase(value);
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
        // The row for importing a picture shows only while the imported
        // picture is the part's chosen panelpaper or look.
        if (control === "picture")
            row.available = pictureChosen(descriptor.key);
        if (lookPairs[descriptor.key] !== undefined) {
            // A panel's surface is its panelpaper; a stand's or tile's its look.
            row.label = ["appearance", "scene3DMaterial"].includes(descriptor.key) ? qsTr("Panelpaper") : qsTr("Look");
            row.options = (row.options || []).map(function(option) {
                return { label: lookLabel(option.value), value: option.value };
            });
        }
        if (descriptor.minimumValue !== undefined && descriptor.minimumValue !== null)
            row.from = Number(descriptor.minimumValue);
        if (descriptor.maximumValue !== undefined && descriptor.maximumValue !== null)
            row.to = Number(descriptor.maximumValue);
        if (descriptor.step !== undefined)
            row.step = Number(descriptor.step);
        if (descriptor.decimals !== undefined)
            row.decimals = Number(descriptor.decimals);
        // A wide range (a scale from 0.05 to 20) is slid in equal steps of
        // its ratio: each doubling takes the same distance.
        if (descriptor.logarithmic === true)
            row.logarithmic = true;
        if (descriptor.suffix !== undefined)
            row.suffix = descriptor.suffix;
        if (descriptor.specialValueText !== undefined)
            row.specialValueText = descriptor.specialValueText;
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
                // A setting that draws nothing in the panel's present state
                // is kept by the backend but not shown.
                if (source[index].inactive === true)
                    continue;
                if (lookPairedKeys.includes(String(source[index].key)))
                    continue;
                // The panel's own 3D settings show while it is drawn in 3D;
                // the icons' 3D is their own, on every panel.
                if (String(source[index].key).indexOf("scene3D") === 0 && !scene3DQualityVisible
                        && String(source[index].section) !== "icons-3d")
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
                // One plain sentence for what each of these changes on the
                // dock.
                const sentences = {
                    layoutPadding: qsTr("The space between the icons and the edge of the theme's artwork."),
                    showBadges: qsTr("A round number in the top-right corner of an application's icon, such as unread messages, when the application reports one."),
                    showProgress: qsTr("A thin bar along the bottom of an application's icon while the application reports progress, such as a download."),
                    showTemporaryStatus: qsTr("For 5 seconds after you start an application, a small “!” on its icon, and a line in its tooltip, say whether it started.")
                };
                if (sentences[row.key] !== undefined)
                    row.description = sentences[row.key];
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

    // A section's glow settings under their own heading, or nothing where
    // its glow cannot show.
    function glowRows(sectionId, description) {
        const fields = fieldsForSection(sectionId);
        return fields.length === 0 ? [] : [section(qsTr("Glow"), description)].concat(fields);
    }

    function schemaSectionRows(sectionId, label, description) {
        const rows = [section(label, description, true)];
        const fields = fieldsForSection(sectionId);
        for (let index = 0; index < fields.length; ++index)
            rows.push(fields[index]);
        return rows;
    }

    // Whether the selected panel draws icon tiles, or stands under its icons
    // (its own, its folder items' or the icon style's).
    readonly property bool tilesApplied: Boolean(panelValue("iconTilesEnabled", false))
    readonly property bool standsApplied: String(panelValue("pedestalShape", "none")) !== "none"
        || !["same", "none"].includes(String(panelValue("folderPedestalShape", "same")))
        || Boolean(panelValue("iconPedestalEnabled", false))

    // A setting offered on this panel as one Overview line: its label and its
    // value as the setting shows it; nothing for a setting that does not act.
    function summaryRow(key, scope) {
        const descriptor = fieldDescriptor(key, scope || "panel");
        if (!descriptor || descriptor.inactive === true)
            return null;
        const row = editorRow(descriptor);
        const value = row.scope === "settings" ? globalValue(key, row.fallback) : panelValue(key, row.fallback);
        let text = String(value);
        if (row.kind === "switch")
            text = Boolean(value) ? qsTr("On") : qsTr("Off");
        else if (row.kind === "combo") {
            // A choice without a label of its own reads as its words.
            text = optionLabel(key, value, scope || "panel");
            if (text === String(value))
                text = titleCase(String(value));
        }
        else if (row.kind === "color")
            text = String(value || "").trim().length > 0 ? String(value) : qsTr("Default");
        else if (row.kind === "spin" || row.kind === "slider")
            text = formatFieldValue(row, value);
        return readOnlyRow(row.label, text);
    }

    function summaryRows(keys, scope) {
        return keys.map(function(key) { return root.summaryRow(key, scope); })
            .filter(function(row) { return row !== null; });
    }

    function overviewPanelRows() {
        const color = String(panelValue("color", "")).trim();
        const free = !isNativePanel();
        const themeId = String(panelValue("completeThemeId", "") || panelValue("panelThemeId", ""));
        const look = themeId.length > 0 && String((selectedPreviewTheme || {}).id || "") === themeId
            && String(selectedPreviewTheme.name || "").length > 0
            ? String(selectedPreviewTheme.name) : optionLabel("appearance", panelValue("appearance", "glass"));
        return [section(qsTr("Panel"), qsTr("Settings currently applied to the selected panel."), true),
            readOnlyRow(qsTr("Name"), profileMode ? editedPanelName : panelRegistry.panelName(selectedPanelId)),
            readOnlyRow(qsTr("Type"), optionLabel("type", panelValue("type", "empty"))),
            readOnlyRow(qsTr("Position"), free
                ? qsTr("Free, at %1, %2").arg(Math.round(Number(panelValue("x", 0)))).arg(Math.round(Number(panelValue("y", 0))))
                : optionLabel("edge", panelValue("edge", "bottom"))),
            readOnlyRow(qsTr("Layout"), optionLabel("layout", panelValue("layout", "adaptive"))),
            readOnlyRow(qsTr("Panelpaper"), look),
            readOnlyRow(qsTr("Color"), color.length > 0 ? color : qsTr("Default")),
            readOnlyRow(qsTr("Opacity"), Math.round(Number(panelValue("opacity", 0.9)) * 100) + "%")]
            .concat(Number(panelValue("panelGlow", 0)) > 0
                ? summaryRows(["panelGlow", "panelGlowReach", "panelGlowColor"]) : [])
            .concat(Boolean(panelValue("hologramPanel", false))
                ? summaryRows(["hologramPanel", "hologramPanelColours"]) : []);
    }

    function overviewIconRows() {
        return [section(qsTr("Icons"), qsTr("Settings currently applied to icons in the selected panel."), true),
            readOnlyRow(qsTr("Style"), optionLabel("iconStyle", panelValue("iconStyle", "plain-original"))),
            readOnlyRow(qsTr("Size"), panelValue("iconSize", 52) + qsTr(" px")),
            readOnlyRow(qsTr("Spacing"), Math.round(Number(panelValue("spacing", 8))) + qsTr(" px")),
            readOnlyRow(qsTr("Animation"), optionLabel("iconAnimation", panelValue("iconAnimation", "scale"))),
            readOnlyRow(qsTr("Magnification"), globalValue("magnificationEnabled", true) ? Number(globalValue("magnification", 1.65)).toFixed(2) + "×" : qsTr("Off"))]
            .concat(Number(panelValue("iconGlow", 0)) > 0
                ? summaryRows(["iconGlow", "iconGlowReach", "iconGlowColor"]) : [])
            .concat(Boolean(panelValue("hologramIcons", false))
                ? summaryRows(["hologramIcons", "hologramIconColours", "hologramBeam"]) : []);
    }

    function overviewTileRows() {
        const custom = String(panelValue("iconTileMode", "style")) === "custom";
        const rows = [section(qsTr("Icon Tiles"), qsTr("Tiles currently applied to icons in the selected panel."), true),
            readOnlyRow(qsTr("Tile appearance"), custom ? qsTr("Custom tile") : qsTr("From icon style"))];
        // Each is listed where the panel offers it.
        const keys = ["iconShape", "iconTileColor", "iconTileOpacity", "iconTileBorderColor",
            "iconTileBorderWidth", "iconTileTexture", "iconTileThickness", "iconTileIconOffsetX",
            "iconTileIconOffsetY", "iconTileIconScale", "iconTileElevation"]
            .concat(Number(panelValue("iconTileGlow", 0)) > 0
                ? ["iconTileGlow", "iconTileGlowReach", "iconTileGlowColor"] : []);
        return rows.concat(summaryRows(keys));
    }

    function overviewPedestalRows() {
        let rows = [section(qsTr("Pedestals"), qsTr("Stands currently applied to icons in the selected panel."), true)];
        if (String(panelValue("pedestalShape", "none")) !== "none")
            rows = rows.concat(summaryRows(["pedestalShape", "pedestalSize", "pedestalThickness", "pedestalLift",
                "pedestalFacing", "pedestalViewAngle", "pedestalMaterial", "pedestalTexture", "pedestalColor",
                "pedestalOpacity", "pedestalLookBackground", "pedestalLookColour",
                "pedestalGlow", "pedestalGlowReach", "pedestalGlowColor", "pedestalAnimation",
                "pedestalAnimationSpeed"]));
        if (!["same", "none"].includes(String(panelValue("folderPedestalShape", "same")))) {
            rows.push(section(qsTr("Folder item stands"), ""));
            rows = rows.concat(summaryRows(["folderPedestalShape", "folderPedestalOwnStyle", "folderPedestalSize",
                "folderPedestalThickness", "folderPedestalLift", "folderPedestalMaterial", "folderPedestalTexture",
                "folderPedestalColor", "folderPedestalOpacity", "folderPedestalGlow", "folderPedestalGlowReach",
                "folderPedestalAnimation"]));
        }
        if (Boolean(panelValue("iconPedestalEnabled", false))) {
            rows.push(section(qsTr("Icon style pedestal"), ""));
            rows = rows.concat(summaryRows(["iconPedestalHeight", "iconPedestalColor"]));
        }
        rows = rows.concat(summaryRows(["hologramPedestals"]).filter(function() {
            return Boolean(root.panelValue("hologramPedestals", false));
        }));
        return rows;
    }

    function panelGeneralRows() {
        const rows = schemaSectionRows("panels-general", qsTr("General"), qsTr("Identity and placement of the selected panel."));
        // General has no buttons: a panel is added
        // with the button beside the panel list and removed with its own
        // right-click menu, an edge panel is hidden with Behavior > Visible,
        // and applications are added by dropping them on the panel from the
        // application menu, as on Plasma's own panels.
        // Whether a free panel is on the desktop. One that is not is put back
        // or removed with the buttons beside the panel list.
        if (!isNativePanel() && !auditionActive && !profileMode) {
            const hostState = String(panelRegistry.panelValue(selectedPanelId, "freeHostState") || "");
            rows.push(readOnlyRow(qsTr("Desktop widget"),
                hostState === "detached" ? qsTr("Not on the desktop")
                    : hostState === "hosted-owned" ? qsTr("On the desktop")
                    : hostState === "hosted-stale" ? qsTr("Being created or checked")
                    : qsTr("Not on the desktop yet")));
        }
        // Only a placement Plasma refused is reported here; the notes and the
        // raw placement result went.
        if (isNativePanel() && PlacementStatus.isProblem(selectedPlacementResult))
            rows.push(notice(qsTr("Placement detail: %1").arg(PlacementStatus.problemText(selectedPlacementResult)), true));
        return rows;
    }

    // A size control is offered only where it changes what is drawn. A free
    // panel is drawn from its layout, so it has no Size page: Radius and
    // Layout scale on its Layout page set its size.
    function panelLayoutRows() {
        const rows = schemaSectionRows("panels-layout", qsTr("Layout"),
            qsTr("Shape geometry and content placement."));
        const angleIndex = rows.findIndex(function(row) { return row.key === "layoutAngle"; });
        const direction = angleIndex >= 0 ? directionRow(rows[angleIndex]) : null;
        if (direction)
            rows.splice(angleIndex, 0, direction);
        return rows;
    }

    // A 3D platform's look - its material and texture (neon, lightning,
    // aurora...), colour, and the look's own background, colours and
    // motion - on Appearance with the rest of the panel's look. The 3D page keeps where and how the
    // platform stands.
    function scene3DLookRows() {
        if (!scene3DQualityVisible)
            return [];
        const fields = fieldsForSection("panels-3d");
        const picked = ["scene3DMaterial", "scene3DColor", "panelLookBackground",
                        "panelLookColour", "lookMotionSpeed"].map(function(key) {
            return fields.find(function(row) { return row.key === key; });
        }).filter(function(row) { return row !== undefined; });
        if (picked.length === 0)
            return [];
        return [section(qsTr("Panelpaper"), qsTr("The surface the panel wears: a built-in panelpaper, or a picture of your own in relief. Colour tints a panelpaper's texture; Default is the surface the panel's own design gives it."))]
            .concat(picked, [{kind: "actions", label: qsTr("Surface"), actions: [{label: qsTr("Reset material"),
                icon: "edit-reset", action: "reset-3d-material", available: true}]}]);
    }

    // The colour settings each decoration has (DecorationScene.colourControls),
    // worked out once per decoration.
    property var decorationControlsCache: ({})
    function decorationControls(id) {
        if (decorationControlsCache[id] === undefined) {
            const decoration = DecorationCatalogue.decoration(id);
            decorationControlsCache[id] = decoration ? DecorationScene.colourControls(decoration) : ({});
        }
        return decorationControlsCache[id];
    }

    // The panel's decoration: chosen from its tree, then only the settings
    // the chosen one has, since a setting that cannot apply is not shown:
    // its lights' and body's colours and finishes, its palette, its trims'
    // metal, its picture's colours, its body's material, and its motion
    // when it moves.
    function decorationRows(fields) {
        const byKey = ({});
        for (const field of fields)
            byKey[field.key] = field;
        if (!byKey.panelDecoration)
            return [];
        const rows = [Object.assign({}, byKey.panelDecoration, {
            label: qsTr("Decoration"),
            description: qsTr("Remakes the panel's outline: a starship's hull, a nebula, a branch, a wand. None wears the panelpaper again.")
        })];
        const id = String(panelValue("panelDecoration", ""));
        const decoration = id.length > 0 ? DecorationCatalogue.decoration(id) : null;
        if (!decoration)
            return rows;
        const controls = decorationControls(id);
        const add = function(key, extra) {
            if (byKey[key])
                rows.push(Object.assign({}, byKey[key], extra || ({})));
        };
        const finishes = { solid: qsTr("Solid"), chameleon: qsTr("Chameleon"), iridescent: qsTr("Iridescent"),
                           rainbow: qsTr("Rainbow"), "metallic-rainbow": qsTr("Metallic rainbow") };
        const finishOptions = function(field) {
            return ((field || {}).options || []).map(function(option) {
                return { label: finishes[option.value] || option.label, value: option.value };
            });
        };
        if (controls.light) {
            add("panelDecorationLight", { designed: controls.light.colour, emptyText: qsTr("As designed"),
                description: qsTr("The colour of its lights; white-hot ones stay white.") });
            add("panelDecorationLightFinish", { options: finishOptions(byKey.panelDecorationLightFinish) });
        }
        if (controls.main) {
            add("panelDecorationMain", { designed: controls.main.colour, emptyText: qsTr("As designed"),
                description: qsTr("The colour of its body; its material's grain and shine stay.") });
            // Its body's colour can be a whole material, shown as it looks.
            if (controls.material)
                add("panelDecorationMaterial", { label: qsTr("Or made of"),
                    description: qsTr("A material for its body, as it looks lit: stone, wood, gems, metal and more.") });
            add("panelDecorationFinish", { options: finishOptions(byKey.panelDecorationFinish) });
        } else if (controls.material) {
            add("panelDecorationMaterial", { label: qsTr("Made of"),
                description: qsTr("A material for its body, as it looks lit: stone, wood, gems, metal and more.") });
        }
        if (controls.palette)
            add("panelDecorationPalette", { designed: controls.palette.colours,
                description: qsTr("Its colours in order. Click one to change it.") });
        if (controls.metal)
            add("panelDecorationMetal", { description: qsTr("The metal of its trims."),
                options: ((byKey.panelDecorationMetal || {}).options || []).map(function(option) {
                    const material = DecorationCatalogue.material(option.value);
                    return { label: option.value === "designed" ? qsTr("As designed") : material ? material.name : option.label,
                             value: option.value };
                }) });
        if (controls.picture) {
            add("panelDecorationHue", { description: qsTr("Turns the picture's colours round the colour wheel.") });
            add("panelDecorationStrength", { description: qsTr("How strong the picture's colours are.") });
            add("panelDecorationBrightness");
        }
        if ((decoration.animation || []).length > 0) {
            add("panelDecorationAnimation", { description: qsTr("Its lights and motion, as it was made to move.") });
            add("panelDecorationSpeed");
        }
        return rows;
    }

    function panelAppearanceRows() {
        const rows = schemaSectionRows("panels-appearance", qsTr("Appearance"), qsTr("Surface styling for the selected panel."));
        // The decoration's settings go under its own heading.
        const decorationFields = rows.filter(function(row) { return String(row.key || "").indexOf("panelDecoration") === 0; });
        for (let index = rows.length - 1; index >= 0; --index)
            if (String(rows[index].key || "").indexOf("panelDecoration") === 0)
                rows.splice(index, 1);
        // The look first, then how much of it shows and its own glow, then
        // the light round it.
        const look = scene3DLookRows();
        if (look.length > 0) {
            const heading = rows.shift();
            const reset = look.pop();
            rows.unshift.apply(rows, [heading].concat(look));
            rows.push(reset);
        } else if (!DecorationCatalogue.decoration(String(panelValue("panelDecoration", "")))) {
            // A flat panel's panelpaper rows come from the schema; they get
            // their heading here (none while a decoration is worn in their
            // place).
            rows.splice(1, 0, section(qsTr("Panelpaper"),
                qsTr("The surface the panel wears: a built-in panelpaper, or a picture of your own in relief.")));
        }
        // What remakes the panel's outlines, contours and parts: a built-in
        // decoration, or an imported decoration package.
        const decorations = decorationRows(decorationFields).concat(panelThemeRows());
        if (decorations.length > 0)
            rows.push.apply(rows, [section(qsTr("Panel decorations"),
                qsTr("Outlines, contours and parts the panel's design gives it: a built-in decoration, or an imported decoration package."))]
                .concat(decorations));
        rows.push.apply(rows, glowRows("panels-glow",
            qsTr("Light round the panel's shape: how strong, how far it reaches out of the panel, and its colour (the look's own when empty).")));
        if (subtabAvailable(1, 10))
            rows.push({ kind: "actions", label: qsTr("3D"),
                description: qsTr("Drawing this panel in 3D, and every 3D setting, is on the 3D page."),
                actions: [{ label: qsTr("Open the 3D page"), icon: "view-preview", action: "open-3d-page",
                            available: true }] });
        else
            rows.push.apply(rows, scene3DUnavailableRows());
        return rows;
    }

    // Why this panel cannot be drawn in 3D, and the one way out where there
    // is one. Without 3D there is no 3D page, so Appearance says this
    // (a tab with nothing to change is not shown).
    function scene3DUnavailableRows() {
        const blocker = CapabilityModel.scene3DBlocker(
            selectedCapabilityResolution, selectedPreviewTheme,
            embeddedRendererPreview.panelSceneItem.true3DCapability, {
                nativePanel: isNativePanel(),
                layout: String(panelValue("layout", "")),
                importedArtwork: ["themeSource", "themePackageManifest", "themeAsset"].some(function(key) {
                    return String(panelValue(key, "")).trim().length > 0;
                })
            });
        const rows = [];
        if (blocker && (blocker.code === "layout" || blocker.code === "look"))
            rows.push({ kind: "actions", label: qsTr("Themes with their own 3D platform"), actions: [{
                label: qsTr("Browse themes"), icon: "preferences-desktop-theme",
                action: "browse-3d-themes", available: true }] });
        return rows;
    }

    // The one home of 3D editing. A compatible free panel draws its own look
    // as a 3D platform; nothing here asks for a different theme.
    function panel3DRows() {
        const rows = [section(qsTr("3D"), qsTr("Choose the platform's presentation and edit its surface."), true)];
        const platform = EditorModel.platformTier(panelValue("rendererTier", "")
            || (selectedCapabilityResolution.renderer || {}).requestedTier);
        if (platform || scene3DControlsAvailable)
            rows.push({kind: "switch", key: "rendererTier", scope: "panel", platformToggle: true,
                label: qsTr("Platform presentation"),
                description: qsTr("Off restores the exact flat look you used before this platform, including its colour, opacity and layout.")});
        // Say what actually blocks 3D: the panel's kind, the renderer, the
        // look, its resources or its layout.
        if (!scene3DControlsAvailable) {
            rows.push.apply(rows, scene3DUnavailableRows());
            return rows;
        }
        if (!platform)
            return rows;
        if (scene3DOffTier === "baked2.5d")
            rows.push({ kind: "switch", key: "rendererTier", scope: "panel", rendererToggle: true,
                label: qsTr("Use true 3D"),
                description: qsTr("Off uses this platform's perspective artwork. Platform presentation off restores the previous flat look.") });
        if (!scene3DQualityVisible)
            return rows;
        const fields = fieldsForSection("panels-3d");
        const pick = function(keys) {
            return keys.map(function(key) {
                return fields.find(function(row) { return row.key === key; });
            }).filter(function(row) { return row !== undefined; });
        };
        // An edge panel's one 3D setting: how far it tilts from the screen's
        // edge toward the middle.
        if (isNativePanel()) {
            rows.push.apply(rows, pick(["scene3DCameraPitch"]));
            return rows;
        }
        // Moving and scaling are done by hand, on the desktop; the exact turn stays here.
        rows.push(section(qsTr("Orientation"),
            qsTr("Pitch, yaw and roll turn the platform. Edit on desktop moves, turns and scales it by hand.")));
        rows.push.apply(rows, pick(["scene3DCameraPitch", "scene3DCameraYaw", "scene3DRoll"]));
        const sceneEditing = auditionActive && auditionKind === "scene3d";
        const transformActions = [{
            label: qsTr("Reset 3D transform"), icon: "edit-reset", action: "reset-3d-transform",
            available: true }];
        if (!profileMode)
            transformActions.push({
                label: qsTr("Edit on desktop"), icon: "transform-move", action: "edit-3d-on-desktop",
                available: !sceneEditing && !auditionBusy && !hasPendingChanges });
        rows.push({ kind: "actions", label: qsTr("Transform"), actions: transformActions });
        // The platform's shape. A look without a 3D platform of its own stands
        // on one generated along the panel's layout, chosen on the Layout page
        // (one place per setting); its width and bend are set
        // here. A theme's own 3D platform keeps the shape it brings.
        const generatedScene = Boolean(((selectedPreviewTheme || {}).scene3D || {}).generated);
        rows.push(section(qsTr("Shape"), generatedScene
            ? qsTr("The platform follows the Dock layout on the Layout page, so every icon stands on it. Arcs, semicircles and fans stay flat.")
            : qsTr("This theme keeps its own outline. Bend folds its rear half.")));
        if (generatedScene) {
            rows.push.apply(rows, pick(["scene3DBand", "scene3DBend"]));
        }
        rows.push.apply(rows, pick(["scene3DFold"]));
        rows.push(section(qsTr("View and surface"), qsTr("Icon spacing is set on Icons > Appearance.")));
        rows.push.apply(rows, pick(["scene3DFieldOfView", "scene3DThickness", "scene3DQuality"]));
        rows.push(section(qsTr("Lighting"), ""));
        rows.push.apply(rows, pick(["scene3DKeyLight", "scene3DFillLight"]));
        rows.push(section(qsTr("Motion"), ""));
        rows.push.apply(rows, pick(["scene3DTransitions", "scene3DFloat"]));
        return rows;
    }

    // The neutral transform: no roll, centred, full size, and the look's own
    // viewing angle.
    function reset3DTransform() {
        for (const key of ["scene3DCameraPitch", "scene3DCameraYaw", "scene3DRoll",
                           "scene3DPositionX", "scene3DPositionY", "scene3DPositionZ", "scene3DScale",
                           "scene3DScaleX", "scene3DScaleY"]) {
            const descriptor = fieldDescriptor(key, "panel");
            if (descriptor)
                editorSession = EditorModel.setPanelValue(editorSession, key, descriptor.defaultValue);
        }
        refreshProjection();
    }

    // Panel themes and skins are their own resource type, separate from
    // Panel Presets: a theme restyles this panel, it is not a whole panel.
    // A free panel lists the looks made for free docks and an edge panel the
    // looks of its own orientation.
    // A look that names no orientation, a built-in flat look, fits any panel.
    // A free panel laid out as a bar takes the bar looks too.
    function themeFitsSelectedPanel(theme) {
        const edge = String(panelValue("edge", "bottom"));
        const layout = String(panelValue("layout", ""));
        const wanted = !isNativePanel()
            ? ["free"].concat(layout === "horizontal" || layout === "vertical" ? [layout] : [])
            : [edge === "left" || edge === "right" ? "vertical" : "horizontal"];
        const orientations = (theme.capabilities || {}).orientations;
        return !Array.isArray(orientations) || orientations.length === 0
            || orientations.some(function(orientation) { return wanted.includes(orientation); });
    }
    function panelThemeRows() {
        const available = CapabilityModel.availableItems(selectedResolvedThemes)
            .filter(themeFitsSelectedPanel);
        const platform = function(theme) {
            return EditorModel.platformTier((theme.panelStyle || {}).rendererTier
                || (theme.capabilities || {}).preferredRendererTier);
        };
        return [
            {
                kind: "themeSamples",
                label: qsTr("Flat decorations"),
                description: qsTr("Flat skins and procedural materials."),
                themes: available.filter(theme => !platform(theme))
            },
            {
                kind: "themeSamples",
                label: qsTr("Platform decorations"),
                description: qsTr("Perspective and true 3D platforms. Load turns Platform presentation on and remembers your flat panelpaper."),
                themes: available.filter(platform)
            },
            {
                // An imported package belongs to a panel on the desktop.
                kind: profileMode ? "none" : "actions",
                label: qsTr("Decoration package"),
                description: artifactDescription(),
                actions: [
                    {
                        label: qsTr("Import…"),
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
        ].filter(function(row) { return row.kind !== "none" && (row.kind !== "themeSamples" || row.themes.length > 0); });
    }

    // Size: Fit content (as Plasma's panel settings call it), the length
    // (the width along a top or bottom panel, the height along a side one)
    // while not fitting, and the thickness. A fitted panel is as long as
    // what it holds and would take no length.
    function panelSizeRows() {
        const rows = schemaSectionRows("panels-size", qsTr("Size"), qsTr("Panel dimensions."));
        const edge = String(panelValue("edge", "bottom"));
        const lengthKey = edge === "left" || edge === "right" ? "height" : "width";
        const fitting = Boolean(panelValue("dynamic", false));
        return rows.filter(function(row) {
            if (row.key === "dynamic")
                row.description = qsTr("The panel is as long as what it holds. Off, it is as long as set here.");
            return !(fitting && row.key === lengthKey);
        });
    }

    function panelBehaviorRows() {
        const rows = schemaSectionRows("panels-behavior", qsTr("Behavior"), qsTr("Visibility and interaction rules."))
            .filter(function(row) { return row.key !== "showTooltips"; });
        // Only a visibility Plasma refused is reported, not the raw result.
        if (isNativePanel() && VisibilityStatus.isProblem(selectedVisibilityResult))
            rows.push(notice(qsTr("Visibility detail: %1").arg(VisibilityStatus.problemText(selectedVisibilityResult)), true));
        return rows;
    }

    function unavailablePage(label, description) {
        return [section(label, description, true)];
    }

    // The panel's own motion. Opening and closing belong to edge panels whose
    // host and theme can draw a mechanism, motion along the path to free
    // panels; a group with nothing to offer is not shown. Motion is listed
    // in the order it is decided: whether it runs, what it moves, how fast,
    // when, and how far the wheel moves it.
    function panelAnimationRows() {
        const presentationKeys = ["presentationMode", "presentationTrigger", "collapseMechanism",
            "collapseAxis", "revealHandle", "openDelay", "closeDelay"];
        const motionKeys = ["panelRotationMode", "panelMotionTarget", "panelTravelSpeed",
            "panelRotationSpeed", "panelRotationTrigger", "scrollSensitivity"];
        const fields = fieldsForSection("panels-animations");
        const opening = fields.filter(function(row) { return presentationKeys.includes(row.key); });
        const rotation = motionKeys.map(function(key) {
            return fields.find(function(row) { return row.key === key; });
        }).filter(function(row) { return row !== undefined; });
        const rows = [];
        if (opening.length) {
            rows.push(section(qsTr("Opening and closing"),
                qsTr("Choose the resting state, reveal trigger and motion. Preview is a draft until Apply."), true));
            rows.push.apply(rows, opening);
            // The panel on the desktop, opened or closed as saved: not a
            // profile's copy.
            if (!hasPendingChanges && !auditionBusy && !profileMode && Boolean(panelValue("visible", false))
                    && panelValue("presentationMode", "open") === "collapsed") {
                rows.push({kind: "actions", label: qsTr("Saved panel"), actions: [
                    {action: "open-panel", label: qsTr("Open panel")},
                    {action: "close-panel", label: qsTr("Close panel")}
                ]});
            }
        }
        if (rotation.length) {
            rows.push(section(qsTr("Free panel motion"),
                qsTr("Choose what the wheel, dragging and continuous motion move, how fast, and when it runs."),
                rows.length === 0));
            rows.push.apply(rows, rotation);
        }
        return rows;
    }

    // Direction for the open paths: Up, Down, Left
    // and Right are layout angles at which the middle of the path faces that
    // side. The fan, arc and semicircle face up at angle 0, the radial path
    // faces right. The fine Layout angle stays below it.
    function directionRow(angleRow) {
        const up = {fan: 0, arc: 0, semicircle: 0, radial: -90}[String(panelValue("layout", ""))];
        if (up === undefined || !angleRow)
            return null;
        const facing = function(turn) {
            const angle = ((up + turn) % 360 + 540) % 360 - 180;
            return angle === -180 ? 180 : angle;
        };
        const options = [
            {value: facing(0), label: qsTr("Up")},
            {value: facing(90), label: qsTr("Right")},
            {value: facing(180), label: qsTr("Down")},
            {value: facing(-90), label: qsTr("Left")}
        ];
        const current = Number(panelValue("layoutAngle", 0));
        if (!options.some(function(option) { return option.value === current; }))
            options.push({value: current, label: qsTr("Custom (%1°)").arg(Math.round(current))});
        return {
            kind: "combo",
            key: "layoutAngle",
            label: qsTr("Direction"),
            scope: "panel",
            fallback: 0,
            options: options,
            description: qsTr("The side the middle of the path faces. Layout angle below fine-tunes it.")
        };
    }

    function iconAppearanceRows() {
        // One place per setting: the shape under the
        // icon is on Icon Tiles, the style's own pedestal on Pedestals.
        const rows = schemaSectionRows("icons-appearance", qsTr("Appearance"),
            qsTr("Style, size and logo options for the selected panel.")).filter(function(row) {
                return !["iconShape", "iconPedestalEnabled", "iconPedestalHeight", "iconPedestalColor"].includes(row.key);
            });
        rows.push.apply(rows, glowRows("icons-glow",
            qsTr("Light round each icon's picture: how strong, how far it reaches, and its colour.")));
        rows.push({kind: "actions", label: qsTr("Style options"), actions: [
            {label: qsTr("Reset style options"), icon: "edit-reset", action: "reset-icon-style-options"}]});
        return rows;
    }

    // The stands' 3D, on every panel (Pedestals > 3D): their shape, then,
    // once one is chosen, how thick each stand is, how high its icon
    // floats above it and how far from above it is seen, and on a panel
    // drawn in 3D how high its Auto column stands.
    readonly property var pedestal3DKeys: ["pedestalThickness", "pedestalLift", "pedestalViewAngle"]
    function pedestal3DRows() {
        const rows = [section(qsTr("3D stands"),
            qsTr("Each stand's depth: how thick it is, how high its icon floats above it and how far from above it is seen; on a panel drawn in 3D, how high its Auto column stands."), true)];
        const fields = fieldsForSection("pedestals");
        for (const key of ["pedestalShape"].concat(pedestal3DKeys)) {
            const row = fields.find(function(field) { return field.key === key; });
            if (row)
                rows.push(row);
        }
        for (const row of fieldsForSection("pedestals-3d"))
            rows.push(row);
        return rows;
    }

    function pedestalRows() {
        const rows = schemaSectionRows("pedestals", qsTr("Icon stands"),
            qsTr("A stand under each icon, on flat and 3D panels: a horizontal stand seen a little from above, the icon centred over it and floating its float height above it. Its shape, size, material, texture, colour, opacity, motion, and its glow (its depth is on the 3D tab): light round the stand, how strong and how far it reaches."));
        // Their depth is on the 3D tab.
        for (let index = rows.length - 1; index >= 0; --index) {
            if (pedestal3DKeys.includes(rows[index].key))
                rows.splice(index, 1);
        }
        rows.push.apply(rows, glowRows("pedestals-glow",
            qsTr("Light round each stand: how strong, how far it reaches, and its colour (the stand's when empty).")));
        rows.push(section(qsTr("Folder item stands"),
            qsTr("The stands under the items of an opened folder: the icons' own (Same), none, or a shape of their own. With Own settings on, they take their own size, thickness, float height, material, texture, colour, opacity, glow and motion.")));
        const fields = fieldsForSection("pedestals-folder");
        for (let index = 0; index < fields.length; ++index)
            rows.push(fields[index]);
        // The pedestal an icon style draws under its icons, such as Dark Orb's.
        const styleRows = ["iconPedestalEnabled", "iconPedestalHeight", "iconPedestalColor"].map(function(key) {
            return root.fieldDescriptor(key, "panel");
        }).filter(function(descriptor) {
            return descriptor && descriptor.inactive !== true;
        }).map(editorRow);
        if (styleRows.length > 0) {
            rows.push(section(qsTr("Icon style pedestal"),
                qsTr("The pedestal the chosen icon style draws under each icon.")));
            for (const row of styleRows)
                rows.push(row);
        }
        return rows;
    }

    // A part's Hologram tab (Panels, Icons, Pedestals): whether it is drawn
    // as light, then its colour set and each part's own colour (the set's,
    // or its own colours, shown while none is chosen), its glow, flicker
    // and speed; the icons' tab also their float and turning, and the
    // projector light they float in with its colours, strength, height and
    // motion. A setting that cannot apply is not shown.
    readonly property var hologramSetLabels: ({
        original: qsTr("Original colours"), cyan: qsTr("Cyan"), ice: qsTr("Ice"), emerald: qsTr("Emerald"),
        amber: qsTr("Amber"), ruby: qsTr("Ruby"), violet: qsTr("Violet"), rose: qsTr("Rose")
    })
    function hologramRows(sectionId) {
        const part = sectionId === "icons-hologram" ? "Icon" : sectionId === "panels-hologram" ? "Panel" : "Pedestal";
        const prefix = "hologram" + part;
        const byKey = ({});
        for (const field of fieldsForSection(sectionId))
            byKey[field.key] = field;
        const rows = [];
        const add = function(key, extra) {
            if (byKey[key])
                rows.push(Object.assign({}, byKey[key], extra || ({})));
        };
        const titles = { Icon: qsTr("Holographic icons"), Panel: qsTr("Holographic panel"),
                         Pedestal: qsTr("Holographic pedestals") };
        const descriptions = {
            Icon: qsTr("Each icon drawn as light: see-through, its edges brighter, crossed by drifting lines, flickering now and then as a weak signal does. With the projector light it floats in a light rising from the middle of its stand, or from the panel where it has none."),
            Panel: qsTr("The panel drawn as light: see-through, its edges brighter, crossed by drifting lines, flickering now and then as a weak signal does."),
            Pedestal: qsTr("The stands under the icons drawn as light: see-through, their edges brighter, crossed by drifting lines, flickering now and then as a weak signal does.")
        };
        rows.push(section(titles[part], descriptions[part], true));
        add(part === "Icon" ? "hologramIcons" : part === "Panel" ? "hologramPanel" : "hologramPedestals");
        if (!byKey[prefix + "Colours"])
            return rows;
        const setName = String(panelValue(prefix + "Colours", "original"));
        const set = HologramLook.colourSets[setName] || null;
        rows.push(section(qsTr("Colours"),
            qsTr("Original colours keeps its own colours, made light; a colour set gives every part its colour. A part's own colour, when chosen, replaces the set's.")));
        add(prefix + "Colours", { options: HologramLook.setNames.map(function(name) {
            return { label: root.hologramSetLabels[name] || root.titleCase(name), value: name };
        }) });
        const partColour = function(key, role, description, ownText) {
            add(key, { designed: set ? set[role] : "", description: description,
                       emptyText: set ? qsTr("From the set") : ownText || qsTr("Its own colours") });
        };
        partColour(prefix + "OutlineColor", "outline", qsTr("Its edges, where it ends."));
        partColour(prefix + "InlineColor", "inline", qsTr("Its inner lines, where its colours change."));
        partColour(prefix + "VolumeColor", "volume", qsTr("Its body, between the lines."));
        partColour(prefix + "GlowColor", "glow", qsTr("The light round it."));
        rows.push(section(qsTr("Glow and flicker")));
        add(prefix + "Glow", { description: qsTr("How strong the light round it is.") });
        add(prefix + "GlowReach", { description: qsTr("How far the light round it reaches.") });
        add(prefix + "Flicker", { description: qsTr("How often it blinks, as a weak signal does. None never blinks.") });
        if (part === "Icon") {
            rows.push(section(qsTr("Motion")));
            add("hologramIconFloat", { description: qsTr("A gentle rise and fall.") });
            add("hologramIconTurning", { description: qsTr("How the icons turn as they float on a 3D panel."),
                options: [{ label: qsTr("Turning"), value: "turn" }, { label: qsTr("Swaying"), value: "sway" },
                          { label: qsTr("Still"), value: "still" }] });
            add("hologramIconSpeed", { description: qsTr("How fast the lines drift, the icons float and turn, and the light moves.") });
            rows.push(section(qsTr("Projector light"),
                qsTr("The light each icon floats in, rising from the middle of its stand's top, or from the panel where it has none. On a top panel it comes down from the stands; on a side panel it wraps the icon from the side.")));
            add("hologramBeam");
            partColour("hologramLightColor", "light", qsTr("The light's colour."), qsTr("The icon's own colour"));
            partColour("hologramOpeningColor", "opening", qsTr("The lens it leaves."), qsTr("The icon's own colour"));
            add("hologramBeamStrength");
            add("hologramBeamHeight", { description: qsTr("How high the icons float in it.") });
            add("hologramBeamMotion", { description: qsTr("How the light moves: its streaks rising, its rays drifting sideways, rings of light rising through it, or a bright band sweeping across it."),
                options: [{ label: qsTr("Rising and drifting"), value: "rise-drift" }, { label: qsTr("Rising"), value: "rise" },
                          { label: qsTr("Pulsing"), value: "pulse" }, { label: qsTr("Sweeping"), value: "sweep" },
                          { label: qsTr("Still"), value: "still" }] });
        } else {
            add(prefix + "Speed", { description: qsTr("How fast its lines drift.") });
        }
        return rows;
    }

    // The icons' own 3D on a 3D panel: each icon's picture as a solid, or a
    // flat picture as on a flat panel; with it on, how deep the solid is, how
    // far it turns to show its depth, how its parts are shaped and lit.
    function icon3DRows() {
        const rows = [section(qsTr("3D icons"),
            qsTr("Each icon's own picture as a solid, on every panel, flat or 3D. Off, the icons stay flat pictures facing you."), true)];
        const byKey = ({});
        for (const field of fieldsForSection("icons-3d"))
            byKey[field.key] = field;
        if (byKey["scene3DIconSolid"])
            rows.push(Object.assign({}, byKey["scene3DIconSolid"]));
        if (Boolean(panelValue("scene3DIconSolid", false)))
            for (const key of ["scene3DIconDepth", "scene3DIconTurn", "scene3DIconRelief", "scene3DIconLighting"])
                if (byKey[key])
                    rows.push(Object.assign({}, byKey[key]));
        return rows;
    }

    function iconTileRows() {
        const rows = schemaSectionRows("icon-tiles", qsTr("Icon Tiles"),
            qsTr("Tile backgrounds for this panel. Preview changes here, then Apply to save."));
        const custom = String(panelValue("iconTileMode", "style")) === "custom";
        // The fill colour, opacity and border are offered wherever they
        // colour the tile (the panel decides). The shape is offered whenever
        // a tile is drawn with it, after the tile appearance.
        const shapeDescriptor = fieldDescriptor("iconShape", "panel");
        const shape = shapeDescriptor ? [editorRow(shapeDescriptor)] : [];
        const filtered = rows.filter(function(row) { return row.key !== "iconShape"; });
        for (let index = 0; index < filtered.length; ++index) {
            if (filtered[index].key === "iconTileMode") {
                filtered[index].options = [{value: "style", label: qsTr("From icon style")},
                    {value: "custom", label: qsTr("Custom tile")}];
                filtered.splice.apply(filtered, [index + 1, 0].concat(shape));
                break;
            }
        }
        // The icon style is chosen in one place, Icons > Appearance.
        if (!custom)
            filtered.push(readOnlyRow(qsTr("Icon style"),
                optionLabel("iconStyle", panelValue("iconStyle", "plain-original")),
                qsTr("Tiles from the icon style follow the style chosen on Icons > Appearance.")));
        filtered.push.apply(filtered, glowRows("icon-tiles-glow",
            qsTr("Light round each tile: how strong, how far it reaches, and its colour (the border's when empty).")));
        filtered.push({kind: "actions", label: qsTr("Tile options"), actions: [
            {label: qsTr("Reset tile options"), icon: "edit-reset", action: "reset-icon-tile-options"}]});
        return filtered;
    }

    function panelSegmentRows() {
        const descriptor = fieldDescriptor("segments", "panel");
        const rows = [section(qsTr("Segments"), qsTr("Independent content groups. Changes apply together when you press Apply."), true)];
        if (!descriptor)
            return rows;
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
                for (const entry of selectable) {
                    rows.push({ kind: "switch", scope: "panel", key: "segments", segmentIndex: index,
                        entryId: String(entry.appId), label: String(entry.displayName || entry.appId),
                        description: qsTr("Assign exclusively to this segment."), fallback: false });
                }
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
        // A preset page is a browser, and UI Themes and a bar's Widgets
        // pages of their own, not settings forms.
        if (currentPresetPage || currentThemesPage || currentWidgetsPage)
            return [];
        if (mainTabIndex === 0)
            return subTabIndex === 0 ? overviewPanelRows() : subTabIndex === 1 ? overviewIconRows()
                : subTabIndex === 2 ? overviewTileRows() : overviewPedestalRows();
        if (mainTabIndex === 1) {
            if (subTabIndex === 0)
                return panelGeneralRows();
            if (subTabIndex === 1)
                return panelSizeRows();
            if (subTabIndex === 2)
                return panelAppearanceRows();
            if (subTabIndex === 3)
                return panelBehaviorRows();
            if (subTabIndex === 4)
                return panelLayoutRows();
            if (subTabIndex === 5)
                return panelSegmentRows();
            if (subTabIndex === 9)
                return panelAnimationRows();
            if (subTabIndex === 10)
                return panel3DRows();
            if (subTabIndex === 11)
                return hologramRows("panels-hologram");
            return [];
        }
        if (mainTabIndex === 2) {
            if (subTabIndex === 0)
                return iconAppearanceRows();
            if (subTabIndex === 1) {
                const rows = schemaSectionRows("icons-behavior", qsTr("Behavior"), qsTr("Icon motion and magnification."));
                // The falloff shapes how the neighbours join a hover; with a
                // reach of one icon or less no neighbour does, so it is not
                // offered then.
                if (Number(panelValue("magnificationRadius", 1)) <= 1)
                    for (let index = rows.length - 1; index >= 0; --index)
                        if (rows[index] && rows[index].key === "magnificationFalloff")
                            rows.splice(index, 1);
                // A label on hovering an icon (and the window previews of an
                // application with windows): an icon's behaviour, so here.
                const tooltips = fieldDescriptor("showTooltips", "settings");
                if (tooltips && tooltips.inactive !== true) {
                    const row = editorRow(tooltips);
                    row.label = qsTr("Show names on hover");
                    row.description = qsTr("Resting the pointer on an icon shows the application's name, and the previews of its windows when it has any. Applies to all Arch Dock panels.");
                    rows.push(row);
                }
                return rows;
            }
            if (subTabIndex === 2)
                return schemaSectionRows("icons-indicators", qsTr("Indicators"), qsTr("Running and attention markers."));
            if (subTabIndex === 3) {
                const rows = schemaSectionRows("icons-notifications", qsTr("Notifications"),
                    qsTr("What applications can show on their icons in this panel."));
                return rows;
            }
            if (subTabIndex === 6)
                return hologramRows("icons-hologram");
            if (subTabIndex === 7)
                return icon3DRows();
            return [];
        }
        if (mainTabIndex === 3)
            return iconTileRows();
        if (mainTabIndex === 4)
            return subTabIndex === 1 ? hologramRows("pedestals-hologram")
                : subTabIndex === 2 ? pedestal3DRows()
                : pedestalRows();
        return unavailablePage(currentSubtabs.length > 0 ? currentSubtabs[subTabIndex] : qsTr("Profiles"), qsTr("Reusable profiles are outside the current settings contract."));
    }

    // Pictures the user imports (Panels > Appearance, Icon Tiles,
    // Pedestals): the row the picture dialog is open for.
    property var pictureRow: null
    // The picture each Look or texture choice wears as "picture".
    readonly property var pictureFor: ({
        appearance: "panelPicture", scene3DMaterial: "panelPicture", scene3DTexture: "panelPicture",
        pedestalMaterial: "pedestalPicture", pedestalTexture: "pedestalPicture",
        folderPedestalMaterial: "folderPedestalPicture", folderPedestalTexture: "folderPedestalPicture"
    })
    // The Looks each picture is worn as (a flat panel's Look and a 3D
    // panel's Look, a stand's), with the plain one left when the picture is
    // removed.
    readonly property var pictureLooks: ({
        panelPicture: [{ key: "appearance", off: "glass" }, { key: "scene3DMaterial", off: "theme" }],
        pedestalPicture: [{ key: "pedestalMaterial", off: "panel" }],
        folderPedestalPicture: [{ key: "folderPedestalMaterial", off: "panel" }]
    })
    // The Look a part had before it wore a picture, by panel and setting:
    // removing the picture while this window is open puts it back (a Neon
    // panel is Neon again), otherwise the plain one above.
    property var lookBeforePicture: ({})
    // A Look and the setting paired with it, as choosing it in its list sets them.
    function setLook(key, value) {
        editorSession = EditorModel.setPanelValue(editorSession, key, value);
        const pair = lookPairs[key];
        if (pair !== undefined)
            editorSession = EditorModel.setPanelValue(editorSession, pair.key, pair.value(value));
    }
    function putOnPicture(key) {
        const before = String(panelValue(key, ""));
        if (before !== "picture")
            lookBeforePicture[selectedPanelId + "/" + key] = before;
        setLook(key, "picture");
    }
    function takeOffPicture(key, plain) {
        if (String(panelValue(key, "")) !== "picture")
            return;
        const before = String(lookBeforePicture[selectedPanelId + "/" + key] || "");
        setLook(key, before.length ? before : plain);
    }
    function importPicture(row) {
        pictureRow = row;
        pictureDialog.open();
    }
    // Whether a part's chosen panelpaper or look is its imported picture.
    function pictureChosen(pictureKey) {
        return Object.keys(pictureFor).some(function(key) {
            return pictureFor[key] === pictureKey && String(panelValue(key, "")) === "picture";
        });
    }
    // The choice a part had before "Imported picture" was picked with no
    // picture to import yet: put back when the chooser is cancelled.
    property var pictureFallback: null
    function dropPictureFallback() {
        const fallback = pictureFallback;
        pictureFallback = null;
        if (!fallback || String(panelValue(pictureFor[fallback.key] || "", "")).length)
            return;
        setLook(fallback.key, fallback.value);
        refreshProjection();
    }
    // Arch Dock copies the chosen picture and the draft wears the copy; the
    // part it is for shows at once: a stand gets a shape, tiles turn on.
    function usePicture(url) {
        const row = pictureRow;
        pictureRow = null;
        if (!row)
            return;
        const imported = panelRegistry.importPicture(url);
        if (!imported || imported.success !== true) {
            studioError = String(imported && imported.message ? imported.message
                : qsTr("The picture could not be imported."));
            return;
        }
        studioError = "";
        editorSession = EditorModel.setPanelValue(editorSession, row.key, String(imported.url));
        pictureFallback = null;
        // The part is shown first, so that its Look is offered: a stand
        // gets a shape, folder item stands their own settings. Then it wears
        // the picture.
        if (row.key === "pedestalPicture" && String(panelValue("pedestalShape", "none")) === "none")
            editorSession = EditorModel.setPanelValue(editorSession, "pedestalShape", "disc");
        if (row.key === "folderPedestalPicture")
            editorSession = EditorModel.setPanelValue(editorSession, "folderPedestalOwnStyle", true);
        refreshProjection();
        for (const look of pictureLooks[row.key] || [])
            if (fieldOffered(look.key))
                putOnPicture(look.key);
        refreshProjection();
    }
    function removePicture(row) {
        editorSession = EditorModel.setPanelValue(editorSession, row.key, "");
        for (const look of pictureLooks[row.key] || [])
            if (fieldOffered(look.key))
                takeOffPicture(look.key, look.off);
        refreshProjection();
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
        projectionTimer.stop();
        clearLivePreview();
        newPanelEdge = "";
        editorSession = EditorModel.cancel(editorSession);
        resetRendererPreview(EditorModel.rendererCandidate(editorSession));
        artifactDraft = {};
        themeTargetPanelId = "";
        colorField = null;
        studioError = "";
        studioWarning = "";
    }

    function applyStudioChanges() {
        // The draft the desktop shows is exactly the one Apply saves.
        if (livePreviewAvailable && hasSettingsChanges && !auditionBusy)
            refreshProjection();
        else
            flushProjection();
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
            const result = profileMode
                ? panelController.applyProfilePanelSettings(profileId, editorSession.panelId, editorSession.revision,
                    EditorModel.transactionPanelCandidate(editorSession))
                : panelController.applyPanelSettingsTransaction(editorSession.panelId, editorSession.revision, EditorModel.transactionPanelCandidate(editorSession), EditorModel.globalCandidate(editorSession));
            if (!EditorModel.transactionSucceeded(result)) {
                editorSession = EditorModel.retainFailure(editorSession, result);
                studioError = EditorModel.transactionConflict(result) ? qsTr("This panel changed outside Panel Studio. Cancel and reopen it before applying.") : qsTr("Settings transaction failed: %1 (%2)").arg(String(result && result.status ? result.status : "failed")).arg(String(result && (result.errorMessage || result.errorCode) ? (result.errorMessage || result.errorCode) : qsTr("No details were returned.")));
                return false;
            }

            const refreshed = editorSnapshot(editorSession.panelId);
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
        if (action === "reset-icon-tile-options") {
            for (const key of ["iconTileTexture", "iconTileThickness", "iconTileIconOffsetX",
                              "iconTileIconOffsetY", "iconTileIconScale", "iconTileBevel",
                              "iconTileMaterial", "iconTileElevation"]) {
                const descriptor = fieldDescriptor(key, "panel");
                if (descriptor)
                    editorSession = EditorModel.setPanelValue(editorSession, key, descriptor.defaultValue);
            }
            refreshProjection();
            return;
        }
        if (action === "reset-icon-style-options") {
            for (const key of ["iconDiameter", "iconLogoSize", "iconOutlineWidth", "iconBodyColor",
                              "iconOutlineColor", "iconGlowColor", "iconPedestalEnabled",
                              "iconPedestalHeight", "iconPedestalColor"]) {
                const descriptor = fieldDescriptor(key, "panel");
                if (descriptor)
                    editorSession = EditorModel.setPanelValue(editorSession, key, descriptor.defaultValue);
            }
            refreshProjection();
            return;
        }
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
        if (action === "reset-3d-material") {
            for (const key of ["scene3DColor", "scene3DMaterial", "scene3DTexture"])
                editorSession = EditorModel.setPanelValue(editorSession, key, fieldDescriptor(key, "panel").defaultValue);
            refreshProjection();
            return;
        }
        if (action === "edit-3d-on-desktop") {
            startSceneEdit();
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
        if (auditionBusy && ["create-free", "import-theme", "render-theme", "clear-theme"].includes(action)) {
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
        } else if (action === "create-edge") {
            const edge = String(data.edge || "");
            if (hasPendingChanges || !["top", "bottom", "left", "right"].includes(edge))
                return;
            occupiedEdges = Array.from(panelController.occupiedScreenEdges() || []);
            if (!newPanelPlaceFree(edge)) {
                studioError = qsTr("Another panel (Plasma's or another dock's) already stands on the screen's %1 edge.").arg(edge);
                return;
            }
            // An edge panel is a bar of Arch Dock's own, which can stand
            // beside others on its edge. Bottom, Top and Side, the built-in
            // edge panels, come back as bars when their edge is picked while
            // they are hidden; any other one is a new dock with nothing on it
            // yet.
            const builtIn = Array.from(panelRegistry.panelIds).find(function(id) {
                return panelRegistry.isBuiltIn(id) && String(panelRegistry.panelValue(id, "edge")) === edge
                    && !Boolean(panelRegistry.panelValue(id, "visible"));
            });
            if (builtIn !== undefined) {
                if (panelController.showEdgePanelAsBar(builtIn))
                    openPanelEditor(builtIn);
                else
                    studioError = qsTr("Could not add %1 to the desktop.").arg(panelRegistry.panelName(builtIn));
                return;
            }
            const panelId = String(panelController.createBar(edge, "launcher") || "");
            if (panelId.length > 0)
                openPanelEditor(panelId);
            else
                studioError = qsTr("Could not add a panel on the screen's %1 edge.").arg(edge);
        } else if (action === "add-new-panel") {
            if (!newPanelPicked || hasPendingChanges || auditionBusy)
                return;
            performStudioAction(newPanelEdge === "free" ? "create-free" : "create-edge", { edge: newPanelEdge });
        } else if (action === "load-built-in-theme") {
            const candidate = auditionActive
                ? (auditionStatus.editorProjection.themeCandidates || {})[String(data.themeId || "")]
                : profileMode ? panelController.profilePanelThemeCandidate(profileId, selectedPanelId, String(data.themeId || ""))
                : panelController.panelThemeCandidate(selectedPanelId, String(data.themeId || ""));
            if (!candidate || candidate.success !== true) {
                studioError = qsTr("This theme cannot be loaded here: %1.")
                    .arg(CapabilityModel.reasonLabel(candidate ? candidate.errorCode : ""));
                return;
            }
            editorSession = EditorModel.stageThemeLook(editorSession, candidate);
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

    // A desktop 3D edit is an audition of this panel itself: the panel shows
    // the draft and its handles, and the audition bar applies or cancels it.
    function startSceneEdit() {
        if (!auditionService) return false;
        if (auditionBusy || hasPendingChanges) {
            studioError = qsTr("Apply or cancel the current changes before editing on the desktop.");
            return false;
        }
        const result = auditionService.beginPreview({ kind: "scene3d", panelId: selectedPanelId });
        if (!auditionSucceeded(result)) return false;
        auditionOriginalPanelId = selectedPanelId;
        auditionCustomizations = {};
        return loadAuditionEditor();
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
        if (profileNoticeIsError && result && result.errorCode === "profile-creates-too-many-free-panels")
            profileNotice = qsTr("Not applied: this profile would create %1 free panels on the desktop, and Apply Profile creates at most %2 at once. Put only the panels you want on the desktop, then use Save Current to Profile: the profile then holds those panels only.")
                .arg(Number(result.freePanelsToCreate)).arg(Number(result.freePanelLimit));
        else profileNotice = profileNoticeIsError
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

    // A panel of a saved profile: renamed or removed in the profile, or
    // edited there with Panel Studio's pages. The desktop does not change.
    function performProfilePanelAction(action, profileId, revision, panelId, value) {
        if (!profilesService || hasPendingChanges || auditionBusy) return;
        if (action === "edit") {
            editProfilePanel(profileId, panelId);
            return;
        }
        let result;
        if (action === "rename") result = profilesService.renameProfilePanel(profileId, revision, panelId, value);
        else if (action === "remove") result = profilesService.removeProfilePanel(profileId, revision, panelId);
        else return;
        profileNoticeIsError = !(result && result.success === true);
        profileNotice = profileNoticeIsError
            ? qsTr("The profile was not changed (%1).").arg(String(result && result.errorCode || "unknown-error"))
            : action === "rename" ? qsTr("Renamed in the profile. The desktop does not change until you apply it.")
            : qsTr("Removed from the profile. The desktop does not change until you apply it.");
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
            presetNoticeText = qsTr("Saved to %1 as “%2”.").arg(page.kind === "panel" ? qsTr("My presets") : qsTr("My Icon Presets")).arg(name);
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
            // Configure on another panel: this panel stays until its
            // changes are applied or dropped.
            const requested = selectedPanelId;
            const previousPanelId = auditionOriginalPanelId || editorSession.panelId;
            internalPanelSelection = true;
            selectedPanelId = previousPanelId;
            internalPanelSelection = false;
            if (auditionBusy)
                studioError = qsTr("Apply or cancel desktop audition before editing another panel.");
            else
                askBeforeLeaving(requested);
            return;
        }
        newPanelEdge = "";
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
            if (!root.auditionActive) return;
            // Desktop handles change the draft too; Studio edits build on it.
            const status = root.auditionService.status;
            if (String(status.kind || "") === "scene3d")
                root.auditionCustomizations = EditorModel.copyValue(status.customizations || {});
            root.loadAuditionEditor();
        }
    }

    Connections {
        target: panelController
        function onContentRevisionChanged() { root.queueEditorReload(); }
    }

    Connections {
        target: panelRegistry
        function onRevisionChanged() { root.queueEditorReload(); }
    }

    // A profile's panel follows changes to the profile made elsewhere.
    Connections {
        target: root.profilesService
        ignoreUnknownSignals: true
        function onChanged() {
            if (root.profileMode)
                root.queueEditorReload();
        }
    }

    // Removing the last preset of your own also removes its tab.
    Connections {
        target: presetLibrary
        function onRevisionChanged() { Qt.callLater(root.ensureOfferedSubtab); }
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

    StudioFrame {
        anchors.fill: parent
        radius: root.studioTheme.radius
        chamfer: root.studioTheme.chamfer
        color: root.studioTheme.windowTop
        colorBottom: root.studioTheme.windowBottom
        borderColor: root.studioTheme.windowBorder
        glowColor: root.studioTheme.glow
        glowSize: root.studioTheme.edgeGlow
    }

    // The alien themes' honeycomb vents, behind everything else.
    StudioHoneycomb {
        objectName: "studio-honeycomb"
        anchors.fill: parent
        anchors.margins: Math.max(2, root.studioTheme.chamfer / 2)
        color: root.studioTheme.honeycomb
    }

    StudioFrame {
        id: titleStrip

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 38
        radius: Math.max(0, root.studioTheme.radius - 1)
        chamfer: Math.max(0, root.studioTheme.chamfer - 1)
        bottomCorners: false
        color: root.studioTheme.titleStrip

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: root.studioTheme.titleBorder
        }

        // The alien themes' light strip along the title's lower edge.
        StudioLightStrip {
            objectName: "studio-title-light"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.bottom
            width: parent.width * 0.62
            color: root.studioTheme.glow
        }

        Row {
            id: titleLead

            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            // Arch Dock's own logo; the generic icon only where it is not installed.
            Kirigami.Icon {
                width: 18
                height: 18
                source: "org.archdock.ArchDock"
                fallback: "preferences-desktop-theme-global"
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: StudioThemes.label(root.studioTheme, qsTr("Arch Dock"))
                color: root.studioTheme.titleName
                font.letterSpacing: root.studioTheme.letterSpacing
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 17
                color: root.studioTheme.titleDivider
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: StudioThemes.label(root.studioTheme, qsTr("Panel Studio"))
                color: root.studioTheme.titleMuted
                font.letterSpacing: root.studioTheme.letterSpacing
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
            // The name as edited: the panel is shown with its draft.
            text: root.newPanelPicked ? root.newPanelName(root.newPanelEdge)
                : root.profileMode ? qsTr("%1 · profile “%2”").arg(root.editedPanelName)
                    .arg(root.editedProfile ? root.editedProfile.name : "")
                : root.editedPanelName || qsTr("Panel Editor")
            color: root.studioTheme.titleText
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
                color: closeButton.hovered ? root.studioTheme.closeHover : "transparent"
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

        StudioFrame {
            Layout.fillHeight: true
            Layout.preferredWidth: root.compactStudio ? 112 : 164
            radius: root.studioTheme.cardRadius
            chamfer: root.studioTheme.chamfer
            color: root.studioTheme.sidebar
            borderColor: root.studioTheme.sidebarBorder

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
                    color: root.studioTheme.sidebarHeading
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
                        onClicked: root.chooseMainTab(index)

                        contentItem: RowLayout {
                            spacing: 10

                            Kirigami.Icon {
                                objectName: "studio-section-icon-" + mainTabDelegate.index
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                source: mainTabDelegate.icon.name
                                // UI Themes shows the plain theme icon where
                                // Arch Dock's logo is not installed.
                                fallback: mainTabDelegate.index === 6 ? "preferences-desktop-theme-global" : "unknown"
                                color: mainTabDelegate.checked ? root.studioTheme.sidebarIconActive : root.studioTheme.sidebarIcon
                            }

                            Label {
                                Layout.fillWidth: true
                                text: StudioThemes.label(root.studioTheme, mainTabDelegate.text)
                                color: mainTabDelegate.checked ? root.studioTheme.sidebarTextActive : root.studioTheme.sidebarText
                                font.letterSpacing: root.studioTheme.letterSpacing / 2
                                font.weight: mainTabDelegate.checked ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                            }
                        }

                        background: StudioFrame {
                            radius: root.studioTheme.controlRadius
                            chamfer: root.studioTheme.chamfer / 2
                            color: mainTabDelegate.checked ? root.studioTheme.sidebarActive
                                : (mainTabDelegate.hovered ? root.studioTheme.sidebarHover : "transparent")
                            borderColor: mainTabDelegate.checked ? root.studioTheme.sidebarActiveBorder : "transparent"

                            // The alien themes light the chosen section's
                            // left edge.
                            StudioLightStrip {
                                lit: mainTabDelegate.checked
                                vertical: true
                                thickness: 3
                                anchors.left: parent.left
                                anchors.leftMargin: 2
                                anchors.verticalCenter: parent.verticalCenter
                                height: parent.height * 0.8
                                color: root.studioTheme.glow
                            }
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Label {
                    Layout.fillWidth: true
                    Layout.margins: 7
                    text: root.livePreviewAvailable
                        ? qsTr("Changes show on the desktop at once; Apply or OK keeps them, Cancel drops them")
                        : qsTr("Changes are held until Apply or OK")
                    visible: !root.compactStudio
                    color: root.studioTheme.footerText
                    font.pixelSize: 9
                    wrapMode: Text.Wrap
                }
            }
        }

        ScrollView {
            id: studioScroll
            objectName: "studio-page-scroll"
            // Scrolls by the wheel as KDE's own scroll views do.
            readonly property Kirigami.WheelHandler wheelHandler: Kirigami.WheelHandler { target: studioScroll.contentItem }
            implicitWidth: 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

        ColumnLayout {
            width: studioScroll.availableWidth
            height: Math.max(implicitHeight, studioScroll.availableHeight)
            spacing: 8

            // Editing a panel of a saved profile: where the changes go, and
            // the way back.
            RowLayout {
                objectName: "studio-profile-edit"
                visible: root.profileMode
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Editing “%1” in the profile “%2”. Apply saves the profile; the panels on your desktop change only when you apply the profile.")
                        .arg(root.editedPanelName).arg(root.editedProfile ? root.editedProfile.name : "")
                    color: root.studioTheme.text
                    wrapMode: Text.Wrap
                }
                Button {
                    objectName: "studio-profile-edit-back"
                    text: qsTr("Back to Profiles")
                    icon.name: "go-previous"
                    onClicked: root.leaveProfileMode()
                }
            }

            RowLayout {
                visible: root.mainTabIndex === 1 || (root.compactStudio && root.previewSection)
                Layout.fillWidth: true
                spacing: 6

                // Where the panel is: picked when a new panel is added (its
                // bare preview is below, and the button adds it); for the
                // panel the window edits, only its own place is shown, since
                // another cannot be picked there.
                Repeater {
                    model: root.mainTabIndex !== 1 ? []
                        : root.newPanelPicked ? root.newPanelKinds : [root.selectedPanelKind]
                    delegate: Button {
                        required property string modelData

                        objectName: "studio-new-panel-" + modelData
                        text: root.newPanelKindName(modelData)
                        // In a narrow window the buttons shrink, their words
                        // cut short, rather than pushing the page wider than
                        // the window and the tab arrows out of reach.
                        Layout.fillWidth: true
                        Layout.maximumWidth: implicitWidth
                        Layout.minimumWidth: Math.min(implicitWidth, 48)
                        checkable: true
                        enabled: root.newPanelPicked && root.newPanelPlaceFree(modelData)
                        checked: root.newPanelPicked ? root.newPanelEdge === modelData
                            : root.selectedPanelKind === modelData
                        onClicked: root.newPanelEdge = modelData
                        // Disabled items show no tooltip; this one is for the
                        // new panel page, where an edge may be taken.
                        HoverHandler { id: placeHover }
                        ToolTip.visible: placeHover.hovered && root.newPanelPicked && !root.newPanelPlaceFree(modelData)
                        ToolTip.text: qsTr("Another panel (Plasma's or another dock's) already stands on this edge")
                        ToolTip.delay: 400
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: addNewPanelButton
                    objectName: "studio-add-new-panel"
                    visible: root.newPanelPicked
                    icon.name: "list-add"
                    text: qsTr("Add to desktop")
                    enabled: !root.hasPendingChanges && !root.auditionBusy
                        && root.newPanelPlaceFree(root.newPanelEdge)
                    Accessible.name: root.newPanelDescription(root.newPanelEdge)
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    ToolTip.delay: 600
                    onClicked: root.performStudioAction("add-new-panel", {})
                }

                ToolButton {
                    visible: root.compactStudio && root.previewSection
                    text: qsTr("Preview")
                    checkable: true
                    checked: root.expandedCompactPreview
                    onToggled: root.expandedCompactPreview = checked
                }
            }

            StudioFrame {
                visible: (!root.compactStudio || root.expandedCompactPreview) && root.previewSection
                id: rendererPreviewCard

                Layout.fillWidth: true
                Layout.preferredHeight: 210
                radius: root.studioTheme.cardRadius
                chamfer: root.studioTheme.chamfer
                color: root.studioTheme.preview
                borderColor: root.studioTheme.previewBorder

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 6

                    // The picked panel and nothing else: the preview's own
                    // orientation, icon state and open/collapsed controls
                    // changed only this picture and were taken out.
                    // The picture follows the panel.
                    Label {
                        objectName: "panel-studio-preview-title"
                        Layout.fillWidth: true
                        text: root.auditionActive ? qsTr("Desktop preview active — saved settings unchanged") : root.presetPreviewActive
                            ? qsTr("Preset preview — %1").arg(String(root.selectedPresetCard.name || ""))
                            : qsTr("Live renderer preview")
                        color: root.studioTheme.previewTitle
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    // The bare new panel, as the button beside the list adds
                    // it: no icons yet.
                    Loader {
                        id: newPanelPreviewLoader

                        objectName: "panel-studio-new-panel-preview"
                        visible: active
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        active: root.newPanelPicked && !root.presetPreviewActive
                        sourceComponent: LivePanelPreview {
                            panelDefinition: root.newPanelDefinition
                            themeDefinition: root.newPanelEdge === "free" ? root.newFreePanelTheme : ({})
                            orderedEntries: []
                            useProvidedEntries: true
                            showBarePanel: root.newPanelEdge !== "free"
                            previewMode: root.newPanelEdge === "free" ? "free"
                                : root.newPanelEdge === "left" || root.newPanelEdge === "right"
                                    ? "vertical" : "horizontal"
                            animationProfiles: ({ reducedMotion: true })
                            contentMargin: 6
                        }
                    }

                    LivePanelPreview {
                        id: embeddedRendererPreview

                        objectName: "panel-studio-live-renderer-preview"
                        visible: !root.presetPreviewActive && !root.newPanelPicked
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        panelDefinition: root.selectedRendererCandidate
                        orderedEntries: root.selectedRendererCandidate.segmentEntries || []
                        useProvidedEntries: root.selectedRendererCandidate.segmentEntries !== undefined
                        showBarePanel: root.previewShowsBarePanel
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
                            color: root.studioTheme.previewInfo
                            font.pixelSize: 10
                        }

                        Label {
                            visible: root.activeRendererPreview.fallbackApplied
                            text: qsTr("Fallback: %1").arg(
                                root.activeRendererPreview.fallbackReason
                                || qsTr("unspecified"))
                            color: root.studioTheme.warning
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
                                    ? (root.livePreviewShown ? qsTr("On the desktop — not saved until Apply")
                                                             : qsTr("Draft only — desktop unchanged"))
                                    : qsTr("Saved settings")
                            color: root.hasSettingsChanges
                                    || root.presetPreviewActive
                                ? root.studioTheme.ready : root.studioTheme.hint
                            font.pixelSize: 10
                        }
                    }
                }
            }

            RowLayout {
                id: studioTabNavigation
                objectName: "studio-tabs-navigation"
                visible: root.currentSubtabs.length > 0 && !root.newPanelPicked
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
                    ToolTip.delay: Kirigami.Units.toolTipDelay
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
                    // Each tab draws its own frame; the row behind them stays clear.
                    background: Item {}

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
                            id: tabButton
                            required property string modelData
                            required property int index
                            readonly property bool offered: root.subtabAvailable(root.mainTabIndex, index)

                            text: modelData
                            visible: offered
                            width: offered ? Math.max(104, implicitWidth) : 0
                            onClicked: root.setSubTab(index)
                            contentItem: Label {
                                text: StudioThemes.label(root.studioTheme, tabButton.text)
                                color: tabButton.checked ? root.studioTheme.sidebarTextActive : root.studioTheme.textSecondary
                                font.weight: tabButton.checked ? Font.DemiBold : Font.Normal
                                font.letterSpacing: root.studioTheme.letterSpacing / 2
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            background: StudioFrame {
                                implicitHeight: 34
                                color: tabButton.checked ? root.studioTheme.sidebarActive
                                    : tabButton.hovered ? root.studioTheme.rowHover : root.studioTheme.row
                                borderColor: tabButton.checked ? root.studioTheme.sidebarActiveBorder
                                    : root.studioTheme.rowBorder
                                radius: root.studioTheme.controlRadius
                                chamfer: root.studioTheme.chamfer / 2
                                bottomCorners: false

                                // The alien themes light the chosen tab's
                                // lower edge.
                                StudioLightStrip {
                                    lit: tabButton.checked
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 1
                                    width: parent.width * 0.9
                                    color: root.studioTheme.glow
                                }
                            }
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
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: studioTabNavigation.scrollTabs(1)
                }
            }

            PresetAuditionBar {
                objectName: "panel-studio-audition-bar"
                theme: root.studioTheme
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
                visible: root.currentPresetPage !== null && root.currentPresetPage.kind === "panel" && !root.newPanelPicked
                && !root.profileMode
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

            // The pages are hidden while a new panel is picked; this
            // keeps the list and the preview at the top.
            Item {
                visible: root.newPanelPicked
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            StudioForm {
                id: studioForm
                objectName: "studio-page-form"
                visible: root.currentPresetPage === null && !root.currentProfilePage && !root.currentThemesPage
                    && !root.currentWidgetsPage && !root.newPanelPicked
                enabled: !root.profilesService || !root.profilesService.active
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 240
                studio: root
                rows: root.rowsForCurrentPage()
                pageKey: root.mainTabIndex + "/" + root.subTabIndex + "/" + root.selectedPanelId
            }

            StudioThemesPage {
                objectName: "panel-studio-themes-page"
                visible: root.currentThemesPage && !root.newPanelPicked
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentThemeId: root.studioTheme.id
                onThemeChosen: function(themeId) { root.chooseStudioTheme(themeId); }
            }

            StudioWidgetsPage {
                objectName: "panel-studio-widgets-page"
                visible: root.currentWidgetsPage && !root.newPanelPicked
                Layout.fillWidth: true
                Layout.fillHeight: true
                theme: root.studioTheme
                widgets: root.selectedBarWidgets
                installed: root.installedBarWidgets
                upright: ["left", "right"].includes(String(root.panelRevision >= 0
                    ? panelRegistry.panelValue(root.selectedPanelId, "edge") || "" : ""))
                actionsEnabled: !root.auditionBusy && (!root.profilesService || !root.profilesService.active)
                notice: root.barWidgetsNotice
                onInstalledListOpened: root.installedBarWidgets = Array.from(panelController.availableBarWidgets() || [])
                onAddRequested: function(plugin) {
                    root.changeBarWidgets(String(panelController.addBarWidget(root.selectedPanelId, plugin) || "").length > 0);
                }
                onRemoveRequested: function(key) {
                    root.changeBarWidgets(panelController.removeBarWidget(root.selectedPanelId, key));
                }
                onMoveRequested: function(key, index) {
                    root.changeBarWidgets(panelController.moveBarWidget(root.selectedPanelId, key, index));
                }
            }

            ProfilePage {
                objectName: "panel-studio-profile-page"
                visible: root.currentProfilePage && !root.newPanelPicked
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 320
                profiles: root.profiles
                theme: root.studioTheme
                shortcutsOnly: root.subTabIndex === 2
                shortcutStatus: root.profilesService ? root.profilesService.shortcutStatus : ({enabled: false, bindings: []})
                applyStatus: root.profilesService ? root.profilesService.status : ({state: "IDLE"})
                actionsBlocked: !root.profilesService || root.hasPendingChanges || root.auditionBusy
                notice: root.profileNotice
                noticeIsError: root.profileNoticeIsError
                onActionRequested: function(action, profileId, revision, value) {
                    root.performProfileAction(action, profileId, revision, value);
                }
                service: root.profilesService
                onPanelActionRequested: function(action, profileId, revision, panelId, value) {
                    root.performProfilePanelAction(action, profileId, revision, panelId, value);
                }
            }

            PresetBrowser {
                id: presetBrowser
                objectName: "panel-studio-preset-browser"
                visible: root.currentPresetPage !== null && !root.newPanelPicked
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 360
                kind: root.currentPresetPage ? root.currentPresetPage.kind : "panel"
                scope: root.currentPresetPage ? root.currentPresetPage.scope : "builtin"
                presets: root.presetCards
                // The decorations are on Appearance; Presets holds presets.
                themeRows: []
                studio: root
                catalogStatus: root.currentPresetPage
                    ? presetLibrary.catalogStatus() : ({ valid: true })
                selectedPresetId: root.selectedPresetId
                noticeText: root.presetNoticeText
                noticeIsError: root.presetNoticeIsError
                auditionStatus: root.auditionStatus
                actionsEnabled: (!root.auditionBusy || root.auditionActive) && root.auditionGuardError.length === 0
                showAuditionBar: !root.auditionBusy && !root.profileMode
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
                onStudioActionRequested: function(action) {
                    root.performStudioAction(action, {});
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
            color: root.studioTheme.footerText
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
            color: root.studioError.length > 0 ? root.studioTheme.error : root.studioTheme.warning
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
            ToolTip.delay: Kirigami.Units.toolTipDelay
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
            color: root.studioTheme.ready
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
        id: pictureDialog
        objectName: "studio-picture-dialog"

        title: qsTr("Import picture")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Pictures (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.svg *.tif *.tiff *.avif *.jxl)"),
            qsTr("All files (*)")]
        onAccepted: root.usePicture(selectedFile)
        onRejected: {
            root.pictureRow = null;
            root.dropPictureFallback();
        }
    }

    FileDialog {
        id: themeDialog

        title: qsTr("Import decoration package or artwork")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Theme packages (archdock-theme.json *.archdock-theme.json *.json)"), qsTr("Design files (*.png *.jpg *.jpeg *.webp *.avif *.heif *.heic *.jxl *.svg *.svgz *.tif *.tiff *.pdf *.psd *.xcf *.blend)"), qsTr("All files (*)")]
        onAccepted: {
            if (root.themeTargetPanelId === root.selectedPanelId)
                root.stageArtifact("import", selectedFile);
            root.themeTargetPanelId = "";
        }
        onRejected: root.themeTargetPanelId = ""
    }

    MessageDialog {
        id: unappliedChangesDialog
        objectName: "unappliedChangesDialog"

        title: qsTr("Changes not applied")
        text: qsTr("%1 has changes that are not applied yet.").arg(root.editedPanelName)
        informativeText: qsTr("Apply them, discard them, or keep editing this panel.")
        buttons: MessageDialog.Apply | MessageDialog.Discard | MessageDialog.Cancel
        onButtonClicked: function(button, role) {
            const target = root.pendingSwitch;
            root.pendingSwitch = "";
            if (button === MessageDialog.Apply) {
                if (!root.applyStudioChanges())
                    return;
            } else if (button === MessageDialog.Discard) {
                root.discardStudioChanges();
            } else {
                return;
            }
            root.continueSwitch(target);
        }
    }

    ColorDialog {
        id: colorDialog

        title: qsTr("Choose panel color")
        parentWindow: root
        modality: Qt.WindowModal
        // Qt's own colour picker, as it always was: in a widgets application
        // Plasma's platform theme would give it KDE's.
        options: ColorDialog.ShowAlphaChannel | ColorDialog.DontUseNativeDialog
        onAccepted: {
            if (root.colorField !== null) {
                // A decoration's colours are plain colours, as it is drawn
                // opaque: #rrggbb.
                const hex = function(channel) {
                    const text = Math.round(channel * 255).toString(16);
                    return text.length < 2 ? "0" + text : text;
                };
                const plain = "#" + hex(selectedColor.r) + hex(selectedColor.g) + hex(selectedColor.b);
                root.setFieldValue(root.colorField, String(root.colorField.key || "").indexOf("panelDecoration") === 0
                    ? plain : selectedColor.toString());
            }
            root.colorField = null;
        }
        onRejected: root.colorField = null
    }
}
