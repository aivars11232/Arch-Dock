#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <optional>

#include "../DockModel.h"
#include "../DockSettings.h"
#include "../KWinActionBridge.h"
#include "../NativeContainmentLifecycle.h"
#include "../PanelPlacement.h"
#include "../PanelRegistry.h"
#include "../SystemStatus.h"
#include "../WindowModel.h"
#include "../WindowWatcher.h"
#include "../integration/PlasmaPanelAdapter.h"
#include "FreePanelController.h"

class QQmlApplicationEngine;
class QScreen;
class QWindow;

class PanelWindow final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int screenRevision READ screenRevision NOTIFY screenRevisionChanged)
    Q_PROPERTY(int visibilityRevision READ visibilityRevision NOTIFY visibilityRevisionChanged)
    Q_PROPERTY(qulonglong dockRevision READ dockRevision NOTIFY dockRevisionChanged)
    Q_PROPERTY(qulonglong dockEntriesRevision READ dockEntriesRevision NOTIFY dockEntriesRevisionChanged)
    Q_PROPERTY(qulonglong nativePlacementRevision READ nativePlacementRevision NOTIFY nativePlacementRevisionChanged)
    Q_PROPERTY(qulonglong nativeVisibilityRevision READ nativeVisibilityRevision NOTIFY nativeVisibilityRevisionChanged)

public:
    explicit PanelWindow(QQmlApplicationEngine &engine,
                         QObject *parent = nullptr);
    ~PanelWindow() override;
    [[nodiscard]] int screenRevision() const;
    [[nodiscard]] int visibilityRevision() const;
    [[nodiscard]] qulonglong dockRevision() const;
    [[nodiscard]] qulonglong dockEntriesRevision() const;
    [[nodiscard]] qulonglong nativePlacementRevision() const;
    [[nodiscard]] qulonglong nativeVisibilityRevision() const;
    bool setDockConfiguration(const QString &panelId, const QString &key, const QVariant &value);

public slots:
    void showSettings();
    void showPanelSettings(const QString &panelId);
    QString createNativePanel(const QString &edge, const QString &type);
    QVariantMap createFreePanel();
    QVariantMap createFreePanelFromTemplate(int containmentId, const QString &ownershipToken);
    QVariantMap adoptFreePanelApplet(int desktopContainmentId, int dockAppletId);
    void saveFreePanelPosition(const QString &panelId, int x, int y);
    bool setNativePanelType(const QString &panelId, const QString &type);
    QVariantMap dockConfiguration(const QString &panelId) const;
    QVariantMap panelRendererConfiguration(const QString &panelId) const;
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
    bool minimizeDockEntry(const QString &appId);
    bool closeDockEntry(const QString &appId);
    bool closeAllDockEntry(const QString &appId);
    bool togglePinnedDockEntry(const QString &appId);
    bool moveDockEntryBefore(const QString &appId, const QString &beforeAppId);
    bool pinDockUrl(const QString &url);
    bool pinDockUrls(const QStringList &urls);
    bool pinPanelUrls(const QString &panelId, const QStringList &urls);
    bool removePanelContent(const QString &panelId, const QString &entryId);
    QVariantList dockFolderEntries(const QString &appId) const;
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
    bool shouldConcealPanel(const QString &panelId) const;
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
    void nativePlacementRevisionChanged();
    void nativeVisibilityRevisionChanged();
    void nativePanelRecoveryFinished();

private:
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
    [[nodiscard]] QVariantList panelSettingsEditorFields(
        const ArchDock::PanelDefinition &candidate,
        const ArchDock::CapabilityResolution &resolution,
        const QString &consumer) const;
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
    [[nodiscard]] QVariantMap commitIconOverrideTransaction(
        const QString &panelId,
        qulonglong expectedRevision,
        const QString &entryIdentity,
        const QVariantMap &overrideValues,
        bool reset);
    [[nodiscard]] std::optional<QVariantMap> iconEntryForIdentity(
        const QString &panelId,
        const QString &entryIdentity) const;

    void updateDesktopSuite();
    void syncRegistryFromLegacySettings();
    void synchronizeScreenAssignments();
    void handleScreensChanged();
    void scheduleNativePanelRecovery();
    void recoverNativePanels(bool allowMissingHostRecovery);
    void synchronizeFreePanels();
    [[nodiscard]] ArchDock::FreePanelCreationResult createFreePanelTransaction(
        const ArchDock::FreePanelCreationRequest &request);
    [[nodiscard]] ArchDock::FreePanelController::HostOperations freePanelHostOperations() const;
    [[nodiscard]] std::optional<int> verifiedFreeTemplateBridgeScreen(
        int containmentId,
        const QString &ownershipToken) const;
    [[nodiscard]] std::optional<int> freePanelHostMatchCount(
        const QString &panelId,
        const QString &ownershipToken) const;
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
    bool reconcileNativePanelVisibility(
        const QString &panelId,
        int containmentId,
        const QString &ownershipToken,
        ArchDock::PanelVisibilityMode requestedMode,
        bool visible,
        QVariantMap persistValues = {});
    void recordNativePanelVisibilityResult(const QString &panelId,
                                           QVariantMap result);
    bool adoptNativePanelOwnership(const QString &panelId, int containmentId);
    [[nodiscard]] ArchDock::NativePanelPlacementResult normalizedNativePanelPlacement(
        const QString &panelId,
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
        const ArchDock::PanelSettingsTransactionDraft &draft);
    [[nodiscard]] QList<ArchDock::PanelSettingsHostResult> rollbackPanelSettingsHosts(
        const ArchDock::PanelSettingsTransactionDraft &draft);
    [[nodiscard]] static bool panelSettingsTopologyChanged(
        const ArchDock::PanelDefinition &before,
        const ArchDock::PanelDefinition &after);
    bool removeLegacyControlApplets(const QString &panelId, int containmentId);
    bool attachNativeDockApplet(const QString &panelId, int containmentId);
    void notifyDockRevision();
    void notifyDockEntriesRevision();
    int evaluatePlasmaScript(const QString &script) const;
    [[nodiscard]] std::optional<int> evaluatePlasmaScriptResultOptional(
        const QString &script) const;
    int evaluatePlasmaScriptResult(const QString &script) const;
    QWindow *createUtilityWindow(const QUrl &source);
    void presentUtilityWindow(QWindow *window);

    QQmlApplicationEngine &m_engine;
    WindowModel m_windowModel;
    DockModel m_dockModel;
    DockSettings m_settings;
    PanelRegistry m_panelRegistry;
    SystemStatus m_systemStatus;
    KWinActionBridge m_actionBridge;
    WindowWatcher m_windowWatcher;
    QPointer<QWindow> m_settingsWindow;
    QPointer<QWindow> m_iconPropertiesWindow;
    int m_screenRevision = 0;
    int m_visibilityRevision = 0;
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
};
