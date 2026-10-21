#pragma once

#include <QDBusContext>
#include <QElapsedTimer>
#include <QDBusMessage>
#include <QHash>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <optional>

#include "../DockModel.h"
#include "../DockSettings.h"
#include "../KWinActionBridge.h"
#include "../NativeContainmentLifecycle.h"
#include "../PanelPlacement.h"
#include "../PanelRegistry.h"
#include "../PanelVisibility.h"
#include "../SystemStatus.h"
#include "../content/OverlayModel.h"
#include "../WindowModel.h"
#include "../WindowWatcher.h"
#include "../integration/PlasmaPanelAdapter.h"
#include "../model/PanelSettingsSchema.h"
#include "../presets/PresetLibrary.h"
#include "../presets/PresetPreviewSession.h"
#include "BarHostSupervisor.h"
#include "../bars/BarWidgets.h"
#include "FreePanelController.h"
#include "PanelContentTransaction.h"
#include "ProfileManager.h"

class QQmlApplicationEngine;
class QScreen;
class QWindow;

// The Arch Dock backend. One object owns the panel registry, the dock and
// window models, the native and free panel hosts and Panel Studio, and is
// the D-Bus service both Plasma applets and Studio talk to: panel
// configurations and renderer projections, settings transactions, content,
// folders, icon overrides, presets and auditions, profiles, presentation
// requests and Quit. Its revisions tell consumers what changed.
//
// The implementation is split by topic:
//   PanelWindow.cpp                 construction, revisions, live content,
//                                   Quit, utility windows, legacy toggles
//   PanelWindowSettings.cpp         configuration, capabilities, the settings
//                                   editor and settings transactions
//   PanelWindowEntries.cpp          entries, icon overrides, content, folders
//   PanelWindowPresets.cpp          presets, scene edits, auditions, profiles
//   PanelWindowScreens.cpp          screens, visibility, presentation
//   PanelWindowFreePanels.cpp       free panel hosts
//   PanelWindowNativePanels.cpp     native panels and Plasma scripting
//   PanelWindowNativePlacement.cpp  native placement and visibility
class PanelWindow final : public QObject, protected QDBusContext
{
    Q_OBJECT

    Q_PROPERTY(int screenRevision READ screenRevision NOTIFY screenRevisionChanged)
    Q_PROPERTY(int visibilityRevision READ visibilityRevision NOTIFY visibilityRevisionChanged)
    Q_PROPERTY(qulonglong dockRevision READ dockRevision NOTIFY dockRevisionChanged)
    Q_PROPERTY(qulonglong contentRevision READ contentRevision NOTIFY contentRevisionChanged)
    Q_PROPERTY(qulonglong dockEntriesRevision READ dockEntriesRevision NOTIFY dockEntriesRevisionChanged)
    Q_PROPERTY(qulonglong nativePlacementRevision READ nativePlacementRevision NOTIFY nativePlacementRevisionChanged)
    Q_PROPERTY(qulonglong nativeVisibilityRevision READ nativeVisibilityRevision NOTIFY nativeVisibilityRevisionChanged)
    Q_PROPERTY(qulonglong presentationRequestRevision READ presentationRequestRevision NOTIFY presentationRequestRevisionChanged)

public:
    explicit PanelWindow(QQmlApplicationEngine &engine,
                         QObject *parent = nullptr);
    ~PanelWindow() override;
    [[nodiscard]] int screenRevision() const;
    [[nodiscard]] int visibilityRevision() const;
    [[nodiscard]] qulonglong dockRevision() const;
    [[nodiscard]] qulonglong dockEntriesRevision() const;
    [[nodiscard]] qulonglong contentRevision() const { return m_contentRevision; }
    [[nodiscard]] qulonglong nativePlacementRevision() const;
    [[nodiscard]] qulonglong nativeVisibilityRevision() const;
    [[nodiscard]] qulonglong presentationRequestRevision() const;
    bool setDockConfiguration(const QString &panelId, const QString &key, const QVariant &value);
    // Leaves the event loop after recording that the user wants Arch Dock to
    // stay stopped, returning any audition to the saved desktop and releasing
    // the KWin watcher whose window reports would start it again.
    QVariantMap stopIntentionally(const QString &reason);
    // A built-in theme's values for Panel Studio to load into a panel's draft,
    // without values for fields that do not act on that panel.
    Q_INVOKABLE QVariantMap panelThemeCandidate(const QString &panelId,
                                                const QString &themeId) const;

public slots:
    void showSettings();
    // The supported way to stop Arch Dock: `arch-dock --quit` calls this.
    QVariantMap quit();
    void showPanelSettings(const QString &panelId);
    // A panel's "Add panel…": the Studio's page for a new free or edge panel.
    void showNewPanelPage();
    // The edges of the screen new panels go to where a panel already
    // stands: Arch Dock's, Plasma's own or another dock's. An edge panel is
    // only added where none does; there is one panel to an edge.
    QStringList occupiedScreenEdges() const;
    QString createNativePanel(const QString &edge, const QString &type);
    // A new edge bar of Arch Dock's own, shown by its bar host
    // (arch-dock-bars) instead of Plasma, so that it can stand beside other
    // bars on its edge. Its id, or nothing when the edge or type is unknown.
    QString createBar(const QString &edge, const QString &type);
    // The bars the bar host shows, as the JSON list it reads (one object per
    // bar: its id, what it holds, its screen, edge, alignment, order,
    // thickness, length, margin, visibility and whether windows keep clear
    // of it). barHostRecordsChanged says when it changes.
    QString barHostRecordsJson() const;
    // Brings a hidden edge panel back as a bar of Arch Dock's own. One that
    // was a Plasma panel becomes a bar only when nothing of it is left in
    // Plasma (no Plasma panel, no ownership token); such a panel keeps its
    // Plasma panel. Whether the panel shows now.
    bool showEdgePanelAsBar(const QString &panelId);
    // The windows that may cover a bar, for bars that hide from them: those
    // shown on the virtual desktop in use, Panel Studio's too, as a JSON list (each one's
    // x, y, width, height, and whether it is active, maximized or full
    // screen). barHostWindowsChanged says when it changes, while there are
    // bars.
    QString barHostWindowsJson() const;
    // What a bar holds, in order along it: one map per thing with its key
    // ("dock" for Arch Dock's dock), its plugin id, name and icon. Nothing for
    // a panel that is no bar.
    QVariantList barWidgetList(const QString &panelId) const;
    // The Plasma widgets installed that a bar can hold, each with its plugin
    // id, name, icon, description and category, in name order.
    QVariantList availableBarWidgets() const;
    // Adds a Plasma widget at the end of a bar; the new widget's key, or
    // nothing when the panel is no bar or no such widget is installed.
    QString addBarWidget(const QString &panelId, const QString &plugin);
    // Takes a widget out of a bar (the dock stays: an empty bar holds none).
    bool removeBarWidget(const QString &panelId, const QString &key);
    // Moves a widget, or the dock, to another place along its bar.
    bool moveBarWidget(const QString &panelId, const QString &key, int index);
    // From the bar host: a widget was removed through its own menu.
    bool barWidgetRemoved(const QString &panelId, const QString &key);
    QVariantMap createFreePanel();
    QVariantMap createFreePanelFromTemplate(int containmentId, const QString &ownershipToken);
    QVariantMap adoptFreePanelApplet(int desktopContainmentId, int dockAppletId);
    // A panel's widget reports that its owner removed it from Plasma, or
    // the Plasma panel it sits in (`removing`), or that the removal was
    // undone. Once Plasma has deleted it, the panel is removed entirely; a
    // built-in edge panel is hidden instead. Profiles keep their own copies.
    void panelRemovalRequested(const QString &panelId, bool removing);
    void saveFreePanelPosition(const QString &panelId, int x, int y);
    bool setNativePanelType(const QString &panelId, const QString &type);
    QVariantMap dockConfiguration(const QString &panelId) const;
    QVariantMap panelRendererConfiguration(const QString &panelId) const;
    // Desktop 3D editing: the panel's gizmo reports a finished drag here. The
    // values join the active scene edit's draft; nothing is saved until the
    // edit is applied, and nothing is accepted outside such an edit.
    QVariantMap updateSceneEditDraft(const QString &panelId, const QVariantMap &values);
    QVariantMap contentRuntimeSnapshot(const QString &panelId) const;
    QVariantMap resolvePanelCapabilities(
        const QString &panelId,
        const QVariantMap &candidateValues = {}) const;
    QVariantList resolvedThemeDefinitions(
        const QString &panelId,
        const QVariantMap &candidateValues = {}) const;
    QVariantList iconStyleDefinitions() const;
    QVariantMap panelSettingsEditorSnapshot(const QString &panelId,
                                            const QString &consumer) const;
    QVariantMap resolvePanelSettingsEditorDraft(
        const QString &panelId,
        qulonglong expectedRevision,
        const QVariantMap &panelValues,
        const QVariantMap &globalValues,
        const QString &consumer) const;
    // Panel Studio's draft of the panel it edits, shown on the desktop as it
    // is edited so that every change is seen at once: the panel is drawn
    // with it, an edge panel's Plasma panel takes its size and place, and a
    // free panel moves to its position. Nothing is saved until the draft is
    // applied. Clearing it (Cancel, closing Panel Studio) shows the saved
    // panel again, where it stood. A change Apply would refuse is not shown;
    // the answer says why ("previewRefusal").
    QVariantMap previewPanelSettingsEditorDraft(
        const QString &panelId,
        qulonglong expectedRevision,
        const QVariantMap &panelValues,
        const QVariantMap &globalValues,
        const QString &consumer);
    void clearPanelSettingsPreview();
    // Panel Studio editing a panel of a saved profile: the same pages and
    // checks as a panel on the desktop, on the profile's own copy of it.
    // Its revision is the profile's. Apply saves the profile; nothing on the
    // desktop changes until the profile is applied. Shared settings belong
    // to Arch Dock, not to a profile, so none are offered.
    QVariantMap profilePanelEditorSnapshot(const QString &profileId, const QString &panelId,
                                           const QString &consumer) const;
    QVariantMap resolveProfilePanelEditorDraft(const QString &profileId, const QString &panelId,
                                               qulonglong expectedRevision, const QVariantMap &panelValues,
                                               const QString &consumer) const;
    QVariantMap applyProfilePanelSettings(const QString &profileId, const QString &panelId,
                                          qulonglong expectedRevision, const QVariantMap &panelValues);
    QVariantMap profilePanelThemeCandidate(const QString &profileId, const QString &panelId,
                                           const QString &themeId) const;
    QVariantMap applyPanelSettingsTransaction(
        const QString &panelId,
        qulonglong expectedRevision,
        const QVariantMap &panelValues,
        const QVariantMap &globalValues = {});
    QVariantMap applyNativePanelPlacementDraft(const QString &panelId,
                                               const QVariantMap &values);
    QVariantMap nativePanelPlacementStatus(const QString &panelId) const;
    bool setDockStringConfiguration(const QString &panelId, const QString &key, const QString &value);
    bool setDockIntegerConfiguration(const QString &panelId, const QString &key, int value);
    bool setDockRealConfiguration(const QString &panelId, const QString &key, double value);
    bool setDockBooleanConfiguration(const QString &panelId, const QString &key, bool value);
    QVariantList dockEntries(const QString &panelType) const;
    QVariantList dockEntriesForPanel(const QString &panelId, const QString &panelType) const;
    QVariantMap iconOverrideSnapshot(
        const QString &panelId,
        const QVariantMap &entry) const;
    QVariantMap iconOverrideSnapshotForIdentity(
        const QString &panelId,
        const QString &entryIdentity) const;
    QVariantMap applyIconOverrideTransaction(
        const QString &panelId,
        qulonglong expectedRevision,
        const QString &entryIdentity,
        const QVariantMap &overrideValues);
    QVariantMap resetIconOverrideTransaction(
        const QString &panelId,
        qulonglong expectedRevision,
        const QString &entryIdentity);
    bool activateDockEntry(const QString &appId);
    QVariantMap activateDockEntryOutcome(const QString &appId);
    bool activateDockWindow(const QString &appId, const QString &windowId);
    bool requestDockWindowAction(const QString &appId, const QString &windowId,
                                 const QString &action);
    bool launchDockEntry(const QString &panelId, const QString &appId,
                         const QString &desktopAction);
    bool minimizeDockEntry(const QString &appId);
    bool closeDockEntry(const QString &appId);
    bool closeAllDockEntry(const QString &appId);
    bool togglePinnedDockEntry(const QString &appId);
    bool moveDockEntryBefore(const QString &appId, const QString &beforeAppId);
    bool pinDockUrl(const QString &url);
    bool pinDockUrls(const QStringList &urls);
    bool pinPanelUrls(const QString &panelId, const QStringList &urls);
    bool removePanelContent(const QString &panelId, const QString &entryId);
    // Panel-specific content operations for free desktop panels. Each one is
    // committed as the panel's next settings revision through the same
    // registry transaction path as every other panel change. Items may be
    // local URLs or application ids the dock model knows; an application is
    // pinned to the panel as its desktop entry. Native panels refuse them.
    bool addPanelEntries(const QString &panelId, const QStringList &entries);
    // The icon that sits in the panel's decoration's launcher seat (a
    // raptor's head) instead of in the row: an entry by its id (empty: the
    // seat is free), or an item dropped on the seat, added to the panel if
    // it is not on it yet.
    bool setPanelDecorationSeat(const QString &panelId, const QString &entryId);
    bool seatPanelUrl(const QString &panelId, const QString &url);
    // How many icons a panel's shape holds side by side (`capacity`, -1 for a
    // shape that grows with its icons) and how many it has (`count`), with
    // the least radius and the largest icon size its icons fit at, and how
    // much of its track its icons get as drawn (`seen`).
    [[nodiscard]] QVariantMap panelIconRoom(const QString &panelId) const;
    bool removePanelEntry(const QString &panelId, const QString &entryId);
    bool movePanelEntryBefore(const QString &panelId,
                              const QString &entryId,
                              const QString &beforeEntryId);
    bool setPanelEntryOrder(const QString &panelId, const QStringList &entryIds);
    [[nodiscard]] QStringList panelEntryOrder(const QString &panelId) const;
    QVariantList dockFolderEntries(const QString &appId) const;
    QVariantMap panelFolderSnapshot(const QString &panelId, const QString &appId) const;
    QVariantMap openPanelFolderChild(const QString &panelId, const QString &appId,
                                    const QString &childId);
    bool openDockUrl(const QString &url);
    QStringList availableKdeWidgets() const;
    bool createNativeKdePanel(const QString &panelId);
    bool addKdeWidget(const QString &panelId, const QString &appletId);
    bool removeNativeKdePanel(const QString &panelId);
    void removePanel(const QString &panelId);
    bool openKdeWidgetPreview(const QString &panelId, const QString &appletId);
    QVariantList availableScreens() const;
    int screenIndexForPanel(const QString &panelId) const;
    void setPanelScreen(const QString &panelId, int screenIndex);
    bool setPanelVisible(const QString &panelId, bool visible);
    void setPanelVisibilityMode(const QString &panelId, const QString &visibilityMode);
    QVariantMap applyNativePanelVisibilityMode(const QString &panelId,
                                               const QString &visibilityMode);
    QVariantMap nativePanelVisibilityStatus(const QString &panelId) const;
    QVariantMap nativePanelHostState(const QString &panelId, const QVariantMap &bounds) const;
    bool shouldConcealPanel(const QString &panelId) const;
    // The applet reports which interaction guards it is currently holding.
    // Without this the host visibility decision runs with every lock false and
    // can conceal a panel under an open context menu.
    bool reportPanelInteractionGuards(const QString &panelId,
                                      const QVariantMap &guards);
    // Where a free panel is drawn on the screen and how much room its
    // smallest folder opening needs there, as its widget last reported:
    // other panels' folders keep clear of it, and free panels keep that far
    // apart. Nothing of it is saved.
    bool reportFreePanelArea(const QString &panelId, double x, double y,
                             double width, double height, double room);
    [[nodiscard]] QVariantList freePanelAreas(const QString &exceptPanelId) const;
    // How much of its track a 3D free panel's icons get as drawn, as its
    // widget last measured it (PanelScene3D.seenTrackFactor): a tilt shortens
    // the track as seen, so the panel holds fewer. Nothing of it is saved.
    bool reportPanelSeenRoom(const QString &panelId, double factor);
    [[nodiscard]] QVariantMap panelInteractionGuards(const QString &panelId) const;
    // The applet reports the resting surface state and host phase its
    // presentation controller has reached. This is runtime state, never
    // persisted; it exists so a harness or Studio can observe the live panel
    // without injecting input into the compositor.
    bool reportPanelPresentationState(const QString &panelId,
                                      const QVariantMap &state);
    [[nodiscard]] QVariantMap panelPresentationState(const QString &panelId) const;
    // Explicit open/collapse requests. They are the producer for the `manual`
    // presentation trigger: a panel resting collapsed with that trigger opens
    // only through this channel. The applet takes the pending request once.
    bool requestPanelPresentation(const QString &panelId, const QString &request);
    QVariantMap takePanelPresentationRequest(const QString &panelId);
    void resetSettings();
    void toggleAutoHide();
    void toggleDesktopSuite();
    void toggleTopLauncher();
    void toggleSideRail();
    void toggleBottomPanel();
    void applyProfile(const QString &profileName);
    void openSystemSettings(const QString &module);
    QVariantMap showIconProperties(const QString &panelId,
                                   const QString &entryIdentity);

signals:
    void screenRevisionChanged();
    void visibilityRevisionChanged();
    void dockRevisionChanged();
    void dockEntriesRevisionChanged();
    void contentRevisionChanged();
    void nativePlacementRevisionChanged();
    void nativeVisibilityRevisionChanged();
    void presentationRequestRevisionChanged();
    void nativePanelRecoveryFinished();
    void barHostRecordsChanged();
    void barHostWindowsChanged();

private:
    [[nodiscard]] ArchDock::ProfileApplyTransaction::Operations profileOperations();
    [[nodiscard]] bool profileBusy() const;
    [[nodiscard]] ArchDock::PresetPreviewRecord profileHostRecord(
        const ArchDock::PanelDefinition &definition) const;
    [[nodiscard]] QUrl panelFolderUrl(const QString &panelId, const QString &appId) const;

    enum class NativePanelDiscoveryStatus
    {
        QueryFailed,
        Missing,
        Unique,
        HostConflict,
        RendererConflict,
    };

    struct NativePanelDiscoveryResult
    {
        NativePanelDiscoveryStatus status = NativePanelDiscoveryStatus::QueryFailed;
        int containmentId = -1;
        int dockAppletId = -1;
    };

    [[nodiscard]] std::optional<ArchDock::PanelDefinition>
    capabilityCandidateDefinition(
        const QString &panelId,
        const QVariantMap &candidateValues) const;
    // The fields Panel Studio offers for a candidate: panel fields, or the
    // global ones that act on this panel.
    [[nodiscard]] QVariantList panelSettingsEditorFields(
        const ArchDock::PanelDefinition &candidate,
        const ArchDock::CapabilityResolution &resolution,
        const QString &consumer,
        ArchDock::PanelSettingsFieldScope scope = ArchDock::PanelSettingsFieldScope::Panel) const;
    [[nodiscard]] QVariantMap panelSettingsEditorValues(
        const ArchDock::PanelDefinition &candidate,
        const QVariantList &fields) const;
    [[nodiscard]] std::optional<ArchDock::PanelSettingsTransactionDraft>
    preparePanelSettingsDraft(
        const QString &panelId,
        qulonglong expectedRevision,
        const QVariantMap &panelValues,
        const QVariantMap &globalValues,
        ArchDock::PanelSettingsTransactionOutcome *outcome) const;
    [[nodiscard]] QVariantMap commitPanelSettingsDraft(
        ArchDock::PanelSettingsTransactionDraft draft,
        ArchDock::PanelSettingsTransactionOutcome outcome);
    [[nodiscard]] std::optional<ArchDock::PanelDefinition> runtimePanelDefinition(
        const QString &panelId) const;
    // The editor's work on a panel definition, whether it is on the desktop
    // or a copy in a profile (takenNames: the names a new automatic name
    // must not repeat; the registry's when null).
    [[nodiscard]] std::optional<ArchDock::PanelSettingsTransactionDraft>
    preparePanelSettingsDraftOn(
        const ArchDock::PanelDefinition &current,
        qulonglong expectedRevision,
        const QVariantMap &panelValues,
        const QVariantMap &globalValues,
        ArchDock::PanelSettingsTransactionOutcome *outcome,
        const QStringList *takenNames = nullptr) const;
    [[nodiscard]] QVariantMap panelSettingsEditorSnapshotOf(
        const ArchDock::PanelDefinition &panel, const QString &consumer) const;
    [[nodiscard]] std::optional<ArchDock::PanelDefinition> capabilityCandidateDefinitionOn(
        const ArchDock::PanelDefinition &current, const QVariantMap &candidateValues) const;
    [[nodiscard]] QVariantList resolvedThemeDefinitionsOn(
        const ArchDock::PanelDefinition &current, const QVariantMap &candidateValues = {}) const;
    [[nodiscard]] QVariantMap panelSettingsEditorProjection(
        const ArchDock::PanelSettingsTransactionDraft &draft,
        qulonglong expectedRevision,
        const QVariantMap &panelValues,
        const QString &consumer) const;
    // Whether a change leaves the panel's icons less room than they need,
    // measured against the panel as it is drawn now.
    [[nodiscard]] std::optional<QString> draftIconRoomRefusal(
        const ArchDock::PanelSettingsTransactionDraft &draft) const;
    // The global values panels are drawn with: the saved ones, with Panel
    // Studio's draft of them.
    [[nodiscard]] QVariantMap drawnGlobalSettings() const;
    // A profile's copy of a panel and the draft of a change to it, checked
    // against the profile's revision.
    [[nodiscard]] std::optional<ArchDock::PanelSettingsTransactionDraft> profilePanelDraft(
        const QString &profileId, const QString &panelId, qulonglong expectedRevision,
        const QVariantMap &panelValues, ArchDock::PanelSettingsTransactionOutcome *outcome) const;
    void showStudioDraftHost();
    void restoreStudioDraftHosts();
    bool convertPresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
        ArchDock::PanelDefinition &candidate, QString *errorCode) const;
    [[nodiscard]] ArchDock::PresetPreviewSession::Operations presetAuditionOperations();
    [[nodiscard]] std::optional<ArchDock::PresetPreviewSession::Prepared> preparePresetPreview(
        const QVariantMap &request, QString *errorCode) const;
    [[nodiscard]] std::optional<ArchDock::PresetPreviewSession::Prepared> prepareSceneEditPreview(
        const QVariantMap &request, QString *errorCode) const;
    [[nodiscard]] bool sceneEditActive(const QString &panelId) const;
    [[nodiscard]] QVariantMap presetResource(const QString &kind, const QString &presetId,
        bool builtInOnly = false) const;
    [[nodiscard]] QVariantMap presetEditorProjection(const ArchDock::PanelDefinition &candidate) const;
    [[nodiscard]] QVariantMap themeCandidateFor(const ArchDock::PanelDefinition &candidate,
                                                const QString &themeId) const;
    [[nodiscard]] std::optional<QVariantMap> presetFreeHostGeometry(
        const ArchDock::PanelDefinition &definition, QString *errorCode) const;
    // `keepApart`: the widget moves on afterwards if it then stands too close
    // to another free panel, as when its owner moved it; a profile, a preview
    // or a panel put back keeps the place it is given.
    bool setPresetFreeHostGeometry(const ArchDock::PanelDefinition &definition,
        const QVariantMap &geometry, QString *errorCode, bool keepApart = false) const;
    bool capturePresetPreviewHost(ArchDock::PresetPreviewRecord &record, QString *errorCode) const;
    bool restorePresetPreviewHost(const ArchDock::PresetPreviewRecord &record, QString *errorCode,
        bool profileTransaction = false) const;
    bool removePresetPreviewHost(const ArchDock::PresetPreviewRecord &record, QString *errorCode,
        bool profileTransaction = false) const;
    bool recoverPresetPreviewHost(const ArchDock::PresetPreviewRecord &record, QString *errorCode);
    bool applyPresetDefaultsToNewPanel(const QString &panelId, QString *errorCode);
    [[nodiscard]] QVariantMap commitIconOverrideTransaction(
        const QString &panelId,
        qulonglong expectedRevision,
        const QString &entryIdentity,
        const QVariantMap &overrideValues,
        bool reset);
    [[nodiscard]] std::optional<QVariantMap> iconEntryForIdentity(
        const QString &panelId,
        const QString &entryIdentity) const;
    bool commitPanelContentTransaction(
        const QString &panelId,
        const ArchDock::PanelContentRequest &request);
    [[nodiscard]] QVariantList freePanelEntries(
        const ArchDock::PanelDefinition &definition) const;

    void updateDesktopSuite();
    void syncRegistryFromLegacySettings();
    void synchronizeScreenAssignments();
    void handleScreensChanged();
    void watchScreen(QScreen *screen);
    void scheduleNativePanelRecovery();
    void recoverNativePanels(bool allowMissingHostRecovery);
    void synchronizeFreePanels();
    void restoreFreePanelHost(const ArchDock::FreePanelController &controller, const QString &panelId);
    [[nodiscard]] ArchDock::FreePanelCreationResult createFreePanelTransaction(
        const ArchDock::FreePanelCreationRequest &request);
    [[nodiscard]] ArchDock::FreePanelController::HostOperations freePanelHostOperations() const;
    [[nodiscard]] std::optional<int> verifiedFreeTemplateBridgeScreen(
        int containmentId,
        const QString &ownershipToken) const;
    [[nodiscard]] std::optional<int> freePanelHostMatchCount(
        const QString &panelId,
        const QString &ownershipToken) const;
    [[nodiscard]] std::optional<int> freePanelWidgetCount(const QString &panelId) const;
    [[nodiscard]] ArchDock::FreePanelHostDiscoveryResult discoverOwnedFreePanelHost(
        const QString &panelId,
        const QString &ownershipToken) const;
    [[nodiscard]] ArchDock::FreePanelHostMutationResult createConfiguredFreePanelHost(
        int screenIndex,
        const QString &panelId,
        const QString &ownershipToken) const;
    [[nodiscard]] ArchDock::FreePanelHostMutationResult configureAdoptedFreePanelHost(
        int desktopContainmentId,
        int dockAppletId,
        const QString &panelId,
        const QString &ownershipToken) const;
    [[nodiscard]] ArchDock::FreePanelHostVerificationOutcome freePanelHostVerification(
        int desktopContainmentId,
        int dockAppletId,
        const QString &panelId,
        const QString &ownershipToken) const;
    [[nodiscard]] ArchDock::FreePanelRemovalOutcome removeOwnedFreePanelHost(
        int desktopContainmentId,
        int dockAppletId,
        const QString &panelId,
        const QString &ownershipToken) const;
    [[nodiscard]] ArchDock::FreePanelRemovalOutcome removeOwnedFreePanelHostByIdentity(
        const QString &panelId,
        const QString &ownershipToken) const;
    [[nodiscard]] ArchDock::FreePanelRemovalOutcome removeVerifiedFreeTemplateBridge(
        int containmentId,
        const QString &ownershipToken) const;
    [[nodiscard]] QScreen *screenForPanel(const QString &panelId) const;
    [[nodiscard]] QString screenIdForIndex(int screenIndex) const;
    [[nodiscard]] QList<ArchDock::EdgePanel> edgePanels() const;
    int nativePanelId(const QString &panelId) const;
    int nativeControlAppletId(const QString &panelId) const;
    int nativeDockAppletId(const QString &panelId) const;
    [[nodiscard]] QString nativeOwnershipToken(const QString &panelId) const;
    int nativePanelOffset(const QString &panelId) const;
    [[nodiscard]] std::optional<bool> nativePanelExistence(int panelId) const;
    bool nativePanelIsOwned(const QString &panelId, int containmentId) const;
    bool nativePanelIsOwned(const QString &panelId,
                            int containmentId,
                            const QString &ownershipToken) const;
    [[nodiscard]] NativePanelDiscoveryResult discoverNativePanel(
        const QString &panelId,
        const QString &ownershipToken,
        const QString &panelType) const;
    int createNativePanelCandidate(const QString &panelId,
                                   const QString &ownershipToken,
                                   QString *errorCode) const;
    int createNativePanelCandidate(const ArchDock::PanelDefinition &definition,
                                   const QString &ownershipToken,
                                   QString *errorCode) const;
    [[nodiscard]] std::optional<int> verifiedNativeDockAppletId(
        const QString &panelId,
        int containmentId,
        const QString &panelType) const;
    bool rollbackNativePanelCandidate(const QString &panelId,
                                      int containmentId,
                                      const QString &ownershipToken) const;
    bool nativeControlAppletIsOwned(const QString &panelId, int containmentId, int appletId) const;
    bool nativeDockAppletIsOwned(const QString &panelId, int containmentId, int appletId) const;
    [[nodiscard]] std::optional<bool> nativePanelTemporarilyHidden(
        const QString &panelId,
        int containmentId) const;
    bool synchronizeNativePanelVisibility(const QString &panelId,
                                          bool visible,
                                          bool allowMissingHostRecovery = true,
                                          std::optional<ArchDock::PanelVisibilityMode> requestedMode =
                                              std::nullopt,
                                          QVariantMap persistValues = {});
    [[nodiscard]] ArchDock::NativeVisibilityCapabilities nativeVisibilityCapabilities(
        const QString &panelId) const;
    [[nodiscard]] ArchDock::PanelVisibilityDecision nativePanelVisibilityDecision(
        const QString &panelId,
        ArchDock::PanelVisibilityMode mode,
        bool visible) const;
    [[nodiscard]] ArchDock::PanelVisibilityDecision nativePanelVisibilityDecision(
        const ArchDock::PanelDefinition &definition,
        ArchDock::PanelVisibilityMode mode,
        bool visible) const;
    bool reconcileNativePanelVisibility(
        const QString &panelId,
        int containmentId,
        const QString &ownershipToken,
        ArchDock::PanelVisibilityMode requestedMode,
        bool visible,
        QVariantMap persistValues = {});
    void recordNativePanelVisibilityResult(const QString &panelId,
                                           QVariantMap result);
    void notifyNativeVisibilityRevision();
    bool adoptNativePanelOwnership(const QString &panelId, int containmentId);
    [[nodiscard]] ArchDock::NativePanelPlacementResult normalizedNativePanelPlacement(
        const QString &panelId,
        const QVariantMap &overrides = {}) const;
    [[nodiscard]] ArchDock::NativePanelPlacementResult normalizedNativePanelPlacement(
        const ArchDock::PanelDefinition &definition,
        const QVariantMap &overrides = {}) const;
    [[nodiscard]] QVariantMap nativePanelPlacementIntent(const QString &panelId) const;
    [[nodiscard]] ArchDock::PlasmaPanelPlacementApplyResult applyNativePanelPlacementTransaction(
        const QString &panelId,
        int containmentId,
        const QString &ownershipToken,
        const QVariantMap &values,
        bool persistIntent);
    bool applyNativePanelPlacement(const QString &panelId,
                                   int containmentId,
                                   const QString &ownershipToken,
                                   QString *errorCode = nullptr);
    bool synchronizeNativePanelPlacement(const QString &panelId);
    void recordNativePanelPlacementResult(
        const QString &panelId,
        ArchDock::PlasmaPanelPlacementApplyResult result);
    [[nodiscard]] QList<ArchDock::PanelSettingsHostResult> applyPanelSettingsHosts(
        const ArchDock::PanelSettingsTransactionDraft &draft,
        bool includeVisibility = true);
    [[nodiscard]] QList<ArchDock::PanelSettingsHostResult> rollbackPanelSettingsHosts(
        const ArchDock::PanelSettingsTransactionDraft &draft,
        bool includeVisibility = true);
    [[nodiscard]] static bool panelSettingsTopologyChanged(
        const ArchDock::PanelDefinition &before,
        const ArchDock::PanelDefinition &after);
    bool removeLegacyControlApplets(const QString &panelId, int containmentId);
    bool attachNativeDockApplet(const QString &panelId, int containmentId);
    void notifyDockRevision();
    void notifyDockEntriesRevision();
    void updateContentDemand();
    void notifyContentRevision();
    bool panelContentVisible(const QString &panelId) const;
    QVariantList statusEntriesFor(const ArchDock::PanelDefinition &definition, bool availableOnly = false) const;
    QVariantMap entryOverlay(const QVariantMap &entry) const;
    int evaluatePlasmaScript(const QString &script) const;
    [[nodiscard]] std::optional<int> evaluatePlasmaScriptResultOptional(
        const QString &script) const;
    [[nodiscard]] std::optional<int> evaluatePlasmaScriptResultOptional(
        const QString &script, bool dispatchRecoveryCalls,
        const std::function<bool()> &stillCurrent = {}) const;
    int evaluatePlasmaScriptResult(const QString &script) const;
    QWindow *createUtilityWindow(const QUrl &source);
    void presentUtilityWindow(QWindow *window, const QString &panelId = {});
    // Panel Studio, created when it is next opened and destroyed when closed.
    QWindow *settingsWindow();
    void showLaunchStatus(const QString &appId, const QString &text);

    QQmlApplicationEngine &m_engine;
    WindowModel m_windowModel;
    DockModel m_dockModel;
    DockSettings m_settings;
    PanelRegistry m_panelRegistry;
    ArchDock::PresetLibrary m_presetLibrary;
    ArchDock::PresetPreviewSession *m_presetAudition = nullptr;
    ArchDock::ProfileManager *m_profileManager = nullptr;
    // Panel Studio's draft as the desktop shows it: the panel drawn with
    // it, the saved revision it was made on, and the global values it
    // changes.
    struct StudioDraft
    {
        ArchDock::PanelDefinition panel;
        qulonglong baseRevision = 0;
        QVariantMap globals;
    };
    std::optional<StudioDraft> m_studioDraft;
    // What a draft changed in Plasma, to put back when it is dropped: the
    // edge placement shown last, and where a free panel stood before the
    // draft first moved it.
    struct StudioDraftHost
    {
        QString panelId;
        QVariantMap placement;
        std::optional<QVariantMap> freeGeometry;
        std::optional<std::pair<int, int>> freePosition;
    };
    StudioDraftHost m_studioDraftHost;
    QList<StudioDraftHost> m_studioDraftRestores;
    QTimer m_studioDraftHostTimer;
    bool m_studioDraftHostBusy = false;
    SystemStatus m_systemStatus;
    ArchDock::OverlayModel m_overlayModel;
    QTimer m_contentTimer;
    qulonglong m_contentRevision = 0;
    bool m_contentPublishing = false;
    KWinActionBridge m_actionBridge;
    WindowWatcher m_windowWatcher;
    QPointer<QWindow> m_settingsWindow;
    QPointer<QWindow> m_iconPropertiesWindow;
    // activateDockEntryOutcome() calls over D-Bus whose application start is
    // still with KDE's launcher: answered when it reports, oldest first.
    QHash<QString, QList<QDBusMessage>> m_pendingLaunchReplies;
    int m_screenRevision = 0;
    QTimer m_screenChangeTimer;
    bool m_screenChangePending = false;
    // Right after the backend or Plasma starts, a free panel whose widget is
    // not found is only checked again: Plasma may still be restoring its
    // layout.
    QElapsedTimer m_freeHostGraceClock;
    QTimer m_freeHostRetryTimer;
    // Free panels whose desktop widget was not found, and since when.
    QHash<QString, QElapsedTimer> m_missingFreeHosts;
    // Free panels already put back on the desktop since Plasma last started:
    // one try each, so a refusing Plasma is not asked again and again.
    QSet<QString> m_restoredFreeHosts;
    // Panels their owner removed in Plasma, waiting for Plasma to delete
    // them (a removal can be undone until then), and the check that waits.
    QSet<QString> m_removalRequested;
    QTimer m_removalCheckTimer;
    void checkRequestedRemovals();
    int m_visibilityRevision = 0;
    QHash<QString, ArchDock::PanelVisibilityLocks> m_panelInteractionGuards;
    // Free panels as drawn on the screen, with the room each one's folder
    // opening needs (reportFreePanelArea).
    QHash<QString, QVariantMap> m_freePanelAreas;
    // What a 3D free panel's tilt leaves of its track (reportPanelSeenRoom).
    QHash<QString, double> m_panelSeenRoom;
    [[nodiscard]] double seenRoomFactor(const ArchDock::PanelDefinition &definition) const;
    // The same once `candidate` is drawn, where its size differs from the
    // panel as it was measured (`measured`, or always without one).
    [[nodiscard]] double predictedSeenRoom(const ArchDock::PanelDefinition &candidate,
                                           const ArchDock::PanelDefinition *measured = nullptr) const;
    QHash<QString, QVariantMap> m_panelPresentationStates;
    QHash<QString, QString> m_pendingPresentationRequests;
    qulonglong m_presentationRequestRevision = 0;
    qulonglong m_dockRevision = 0;
    qulonglong m_dockEntriesRevision = 0;
    qulonglong m_nativePlacementRevision = 0;
    qulonglong m_nativeVisibilityRevision = 0;
    QHash<QString, QVariantMap> m_nativePanelPlacementResults;
    QHash<QString, QVariantMap> m_nativePanelVisibilityResults;
    QHash<QString, QString> m_nativePanelVisibilityStateCache;
    int m_nativePanelRecoveryGeneration = 0;
    bool m_nativePanelRecoveryActive = false;
    bool m_settingsTransactionAdoptionActive = false;

    // Arch Dock's own bars: edge panels whose host is the bar host rather
    // than Plasma. Their list is sent to the bar host when it changes, and
    // the bar host is kept running while there are bars.
    [[nodiscard]] bool barHosted(const QString &panelId) const;
    void scheduleBarRecordsUpdate();
    void publishBarRecords();
    // A moving window reports on every frame: the bar host is told of
    // window changes at most every few frames, and only while there are bars.
    void scheduleBarWindowsUpdate();
    // A bar's visibility needs nothing from Plasma: what the caller asks to
    // keep is saved, and the bar host shows or hides the bar from it.
    bool persistBarVisibility(const QString &panelId,
                              std::optional<ArchDock::PanelVisibilityMode> requestedMode,
                              const QVariantMap &persistValues);
    // A bar's dock and Plasma widgets, as its record keeps them, and a new
    // list saved; the bar host follows the record.
    [[nodiscard]] QList<ArchDock::Bars::BarWidget> barWidgetsOf(const QString &panelId) const;
    bool setBarWidgets(const QString &panelId, const QList<ArchDock::Bars::BarWidget> &widgets);
    // The installed Plasma widgets a bar can hold, by plugin id (each a map
    // of its name, icon, description and category), read once and again
    // whenever the list for Panel Studio is asked for.
    const QVariantMap &installedBarWidgets(bool reread = false) const;
    BarHostSupervisor m_barHost;
    QTimer m_barRecordsTimer;
    QString m_barRecordsJson;
    QTimer m_barWindowsTimer;
    mutable QVariantMap m_installedBarWidgets;
    mutable bool m_installedBarWidgetsRead = false;
};
