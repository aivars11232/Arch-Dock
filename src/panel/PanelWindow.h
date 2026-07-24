#pragma once

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "../DockModel.h"
#include "../DockSettings.h"
#include "../KWinActionBridge.h"
#include "../PanelPlacement.h"
#include "../PanelRegistry.h"
#include "../SystemStatus.h"
#include "../WindowModel.h"
#include "../WindowWatcher.h"

class QQmlApplicationEngine;
class QScreen;
class QWindow;

class PanelWindow final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int screenRevision READ screenRevision NOTIFY screenRevisionChanged)
    Q_PROPERTY(int visibilityRevision READ visibilityRevision NOTIFY visibilityRevisionChanged)
    Q_PROPERTY(qulonglong dockRevision READ dockRevision NOTIFY dockRevisionChanged)

public:
    explicit PanelWindow(QQmlApplicationEngine &engine,
                         QObject *parent = nullptr);
    ~PanelWindow() override;
    [[nodiscard]] int screenRevision() const;
    [[nodiscard]] int visibilityRevision() const;
    [[nodiscard]] qulonglong dockRevision() const;

public slots:
    void showSettings();
    void showPanelSettings(const QString &panelId);
    QString createNativePanel(const QString &edge, const QString &type);
    QString createFreePanel();
    QString createFreePanelFromTemplate(int containmentId, const QString &ownershipToken);
    void saveFreePanelPosition(const QString &panelId, int x, int y);
    bool setNativePanelType(const QString &panelId, const QString &type);
    QVariantMap dockConfiguration(const QString &panelId) const;
    bool setDockConfiguration(const QString &panelId, const QString &key, const QVariant &value);
    QVariantList dockEntries(const QString &panelType) const;
    QVariantList dockEntriesForPanel(const QString &panelId, const QString &panelType) const;
    bool activateDockEntry(const QString &appId);
    bool activateDockWindow(const QString &appId, const QString &windowId);
    bool minimizeDockEntry(const QString &appId);
    bool closeDockEntry(const QString &appId);
    bool closeAllDockEntry(const QString &appId);
    bool togglePinnedDockEntry(const QString &appId);
    bool moveDockEntryBefore(const QString &appId, const QString &beforeAppId);
    bool pinDockUrl(const QString &url);
    bool pinDockUrls(const QStringList &urls);
    bool pinPanelUrls(const QString &panelId, const QStringList &urls);
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
    void setPanelVisibilityMode(const QString &panelId, const QString &visibilityMode);
    bool shouldConcealPanel(const QString &panelId) const;
    void resetSettings();
    void toggleAutoHide();
    void toggleDesktopSuite();
    void toggleTopLauncher();
    void toggleSideRail();
    void toggleBottomPanel();
    void applyProfile(const QString &profileName);
    void openSystemSettings(const QString &module);

    void showIconProperties(int row);

signals:
    void screenRevisionChanged();
    void visibilityRevisionChanged();
    void dockRevisionChanged();
    void nativePanelRecoveryFinished();

private:
    void updateDesktopSuite();
    void syncRegistryFromLegacySettings();
    void synchronizeScreenAssignments();
    void handleScreensChanged();
    void scheduleNativePanelRecovery();
    void recoverNativePanels();
    void synchronizeFreePanels();
    [[nodiscard]] QScreen *screenForPanel(const QString &panelId) const;
    [[nodiscard]] QString screenIdForIndex(int screenIndex) const;
    [[nodiscard]] QList<ArchDock::EdgePanel> edgePanels() const;
    int nativePanelId(const QString &panelId) const;
    int nativeControlAppletId(const QString &panelId) const;
    int nativeDockAppletId(const QString &panelId) const;
    [[nodiscard]] QString nativeOwnershipToken(const QString &panelId) const;
    int nativePanelOffset(const QString &panelId) const;
    bool nativePanelExists(int panelId) const;
    bool nativePanelIsOwned(const QString &panelId, int containmentId) const;
    bool nativeControlAppletIsOwned(const QString &panelId, int containmentId, int appletId) const;
    bool nativeDockAppletIsOwned(const QString &panelId, int containmentId, int appletId) const;
    bool adoptNativePanelOwnership(const QString &panelId, int containmentId);
    bool synchronizeNativePanelScreen(const QString &panelId) const;
    bool removeLegacyControlApplets(const QString &panelId, int containmentId);
    bool attachNativeDockApplet(const QString &panelId, int containmentId);
    int evaluatePlasmaScript(const QString &script) const;
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
    QHash<QString, QPointer<QWindow>> m_freePanelWindows;
    int m_screenRevision = 0;
    int m_visibilityRevision = 0;
    qulonglong m_dockRevision = 0;
    int m_nativePanelRecoveryGeneration = 0;
    bool m_nativePanelRecoveryActive = false;
};
