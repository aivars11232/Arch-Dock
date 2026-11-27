// PanelWindow's lifetime and shared plumbing: construction (models, D-Bus
// objects, watchers and the connections between them), the revision counters
// consumers follow, publishing live content (status samples and overlays)
// while a panel shows it, Quit, the Studio and utility windows, and the legacy
// desktop-suite toggles. The rest of the class is split by topic over the
// PanelWindow*.cpp files beside this one; PanelWindow.h lists them.

#include "PanelWindow.h"
#include "updates/ArchDockUpdates.h"
#include "PanelWindowHelpers.h"

#include "../PanelVisibility.h"
#include "../IntentionalStop.h"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QFileInfo>
#include <QCursor>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QSize>
#include <QTimer>
#include <QWindow>

#include <array>
#include <memory>
#include <utility>
#include "updates/UpdateHold.h"

using PanelWindowHelpers::fitUtilityWindow;

PanelWindow::PanelWindow(QQmlApplicationEngine &engine,
                         QObject *parent)
    : QObject(parent),
      m_engine(engine),
      m_windowModel(this),
      m_dockModel(m_windowModel, this),
      m_settings(this),
      m_panelRegistry(this),
      m_presetLibrary(m_panelRegistry, this),
      m_systemStatus(this),
      m_overlayModel(this),
      m_actionBridge(this),
      m_windowWatcher(m_windowModel)
{
    // Read recovery state before any legacy startup rewrite. Loading the
    // journal never mutates a host; recovery remains an explicit action.
    m_profileManager = new ArchDock::ProfileManager(profileOperations(),
        ArchDock::ProfileStore::defaultRoot(), ArchDock::ProfileApplyTransaction::defaultJournalPath(), this);
    // Saved profiles get the light 3D defaults once, as the panels did when
    // the registry loaded (not while an interrupted apply waits: both are
    // given once it is put right), and never while the saved panels could
    // not be read: a configuration Arch Dock cannot read is left as it is.
    if (!m_panelRegistry.persistenceBlocked())
        m_profileManager->lightenSavedProfilesOnce();
    m_lightDefaultsDeferred = profileBusy() && !m_panelRegistry.persistenceBlocked();
    for (const QString &panelId : profileBusy() ? QStringList{} : m_panelRegistry.panelIds())
    {
        if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
            QStringLiteral("free"))
        {
            continue;
        }
        const QStringList legacyIds = m_panelRegistry.panelValue(
            panelId, QStringLiteral("contentAppIds")).toStringList();
        if (legacyIds.isEmpty())
        {
            continue;
        }
        QStringList urls = m_panelRegistry.panelValue(
            panelId, QStringLiteral("contentUrls")).toStringList();
        for (const QString &appId : legacyIds)
        {
            const QUrl url = m_dockModel.urlForApplicationId(appId);
            if (url.isValid() && !urls.contains(url.toString()))
            {
                urls.append(url.toString());
            }
        }
        m_panelRegistry.updatePanel(
            panelId,
            {{QStringLiteral("contentUrls"), urls},
             {QStringLiteral("contentAppIds"), QStringList{}}});
        m_dockModel.removePinnedApplications(legacyIds);
    }

    connect(&m_dockModel,
        &DockModel::windowActionRequested,
        &m_actionBridge,
        &KWinActionBridge::requestAction);
    // KDE's launcher reports whether an accepted start really started: the
    // entry shows it, and a D-Bus caller still waiting for the outcome of its
    // click (the applet, which animates only a confirmed start) is answered.
    connect(&m_dockModel, &DockModel::launchFinished, this,
            [this](const QString &appId, bool started, const QString &)
            {
                showLaunchStatus(appId, started ? tr("Application started") : tr("Launch failed"));
                QList<QDBusMessage> &pending = m_pendingLaunchReplies[appId];
                if (!pending.isEmpty())
                {
                    const QDBusMessage call = pending.takeFirst();
                    QDBusConnection::sessionBus().send(call.createReply(QVariant(QVariantMap{
                        {QStringLiteral("appId"), appId},
                        {QStringLiteral("outcome"), started ? QStringLiteral("succeeded")
                                                            : QStringLiteral("failed")},
                        {QStringLiteral("reason"), started ? QString{}
                                                           : QStringLiteral("launch-failed")}})));
                }
                if (pending.isEmpty())
                    m_pendingLaunchReplies.remove(appId);
            });

    connect(&m_barHost, &BarHostSupervisor::canHideChanged, this, &PanelWindow::barsCanHideChanged);

    // The objects Studio and the utility windows bind to by name.
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("windowModel"),
        &m_windowModel);
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("dockModel"),
        &m_dockModel);
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("dockSettings"),
        &m_settings);
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("panelRegistry"),
        &m_panelRegistry);
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("presetLibrary"),
        &m_presetLibrary);
    m_presetAudition = new ArchDock::PresetPreviewSession(presetAuditionOperations(),
        ArchDock::PresetPreviewRecovery::defaultPath(), ArchDock::PresetDefaultStore::defaultPath(), this);
    m_presetAudition->setObjectName(QStringLiteral("presetAudition"));
    m_engine.rootContext()->setContextProperty(QStringLiteral("presetAudition"), m_presetAudition);
    connect(m_presetAudition, &ArchDock::PresetPreviewSession::changed, this, [this] {
        notifyDockRevision();
        notifyDockEntriesRevision();
        // An audition shown on the desktop may show the status.
        updateContentDemand();
    });
    m_engine.rootContext()->setContextProperty(QStringLiteral("profileManager"), m_profileManager);
    connect(m_profileManager, &ArchDock::ProfileManager::changed, this, [this] {
        if (m_lightDefaultsDeferred && !profileBusy())
        {
            m_lightDefaultsDeferred = false;
            m_panelRegistry.applyDeferredLightDefaults();
            m_profileManager->lightenSavedProfilesOnce();
        }
        notifyDockRevision();
        notifyDockEntriesRevision();
        // So may a profile being tried on the desktop.
        updateContentDemand();
        if (m_screenChangePending && !profileBusy()) m_screenChangeTimer.start();
    });
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("systemStatus"),
        &m_systemStatus);
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("panelController"),
        this);
    // The Update button. It reaches the rest of Arch Dock only through these
    // hooks, and nothing on the bus reaches it.
    ArchDock::Updates::ArchDockUpdates::Hooks updateHooks;
    updateHooks.profileBusy = [this] { return profileBusy(); };
    updateHooks.previewActive = [this] { return m_presetAudition && m_presetAudition->active(); };
    updateHooks.studioOpen = [this] { return m_settingsWindow && m_settingsWindow->isVisible(); };
    updateHooks.studioDraft = [this]
    {
        return m_settingsWindow && m_settingsWindow->isVisible() && m_settingsWindow->property("hasPendingChanges").toBool();
    };
    updateHooks.openStudioOnUpdates = [this] { showUpdatesPage(); };
    m_updates = new ArchDock::Updates::ArchDockUpdates(updateHooks, this);
    m_engine.rootContext()->setContextProperty(QStringLiteral("archDockUpdates"), m_updates);

    // The D-Bus objects the applets and Studio call. When plasmashell goes
    // away, native panel state is stale and an audition in progress is
    // recovered; when it returns, the native panels are recovered.
    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    sessionBus.registerObject(QStringLiteral("/Profiles"), m_profileManager,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties);
    sessionBus.registerObject(QStringLiteral("/PresetAudition"), m_presetAudition,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties);
    sessionBus.registerObject(
        QStringLiteral("/Control"),
        this,
        QDBusConnection::ExportAllSlots |
            QDBusConnection::ExportAllSignals |
            QDBusConnection::ExportAllProperties);
    auto *plasmaShellWatcher = new QDBusServiceWatcher(
        QStringLiteral("org.kde.plasmashell"),
        sessionBus,
        QDBusServiceWatcher::WatchForOwnerChange,
        this);
    connect(plasmaShellWatcher,
            &QDBusServiceWatcher::serviceOwnerChanged,
            this,
            [this](const QString &, const QString &oldOwner, const QString &newOwner)
            {
                if (oldOwner == newOwner)
                {
                    return;
                }
                // A free panel's widget is looked for afresh once Plasma is
                // back, after the same wait as at the backend's start.
                m_freeHostGraceClock.restart();
                m_missingFreeHosts.clear();
                // A removal reported to the Plasma that went away is not
                // trusted across its restart: a widget whose removal was
                // final is missing for good and is put back like any lost
                // one, never deleted on the old word.
                if (!m_removalRequested.isEmpty())
                {
                    qInfo() << "Plasma restarted; removals reported before are not acted on:"
                            << m_removalRequested.keys();
                    m_removalRequested.clear();
                    m_removalCheckTimer.stop();
                }
                if (newOwner.isEmpty())
                {
                    ++m_nativePanelRecoveryGeneration;
                    m_nativePanelVisibilityStateCache.clear();
                    if (m_presetAudition && m_presetAudition->state() != QStringLiteral("IDLE"))
                        m_presetAudition->recoverInterruptedPreview();
                    return;
                }
                m_nativePanelVisibilityStateCache.clear();
                m_restoredFreeHosts.clear();
                scheduleNativePanelRecovery();
            });

    // Live content (status samples, overlays) is published in batches of at
    // most one per 100 ms, and only while a panel shows it.
    m_contentTimer.setSingleShot(true);
    m_contentTimer.setInterval(100);
    connect(&m_contentTimer, &QTimer::timeout, this, &PanelWindow::notifyContentRevision);
    connect(&m_overlayModel, &ArchDock::OverlayModel::changed, this, [this] {
        ++m_overlayRevision;
        emit overlayRevisionChanged();
        if (m_contentPublishing && !m_contentTimer.isActive()) m_contentTimer.start();
    });
    connect(&m_systemStatus, &SystemStatus::statusChanged, this, [this] {
        // A reading is news to the panels only when one of them shows the
        // status; Panel Studio's views read theirs from systemStatus.
        if (m_panelStatusShown && m_contentPublishing && !m_contentTimer.isActive()) m_contentTimer.start();
    });
    connect(&m_panelRegistry, &PanelRegistry::revisionChanged, this, &PanelWindow::updateContentDemand);
    connect(&m_windowWatcher, &WindowWatcher::nativePanelsChanged,
            this, &PanelWindow::notifyNativeVisibilityRevision);
    connect(&m_windowWatcher, &WindowWatcher::nativePanelsChanged,
            this, &PanelWindow::scheduleBarWindowsUpdate);
    updateContentDemand();

    // Settings, registry and model changes become the revisions consumers
    // follow.
    connect(&m_settings, &DockSettings::desktopSuiteChanged, this, &PanelWindow::updateDesktopSuite);
    connect(&m_settings, &DockSettings::monitorIndexChanged, this, &PanelWindow::updateDesktopSuite);
    connect(&m_settings, &DockSettings::topLauncherVisibleChanged, this, &PanelWindow::updateDesktopSuite);
    connect(&m_settings, &DockSettings::sideRailVisibleChanged, this, &PanelWindow::updateDesktopSuite);
    connect(&m_dockModel, &DockModel::countChanged, this, [this]
            {
                notifyDockEntriesRevision();
            });
    connect(&m_panelRegistry, &PanelRegistry::revisionChanged, this, [this]
            {
                notifyDockRevision();
            });
    // Arch Dock's own bars follow their records, as drawn: the bar host is
    // told when the list changes and is kept running while there are bars.
    m_barRecordsTimer.setSingleShot(true);
    m_barRecordsTimer.setInterval(0);
    connect(&m_barRecordsTimer, &QTimer::timeout, this, &PanelWindow::publishBarRecords);
    connect(&m_panelRegistry, &PanelRegistry::panelsChanged, this, &PanelWindow::scheduleBarRecordsUpdate);
    connect(this, &PanelWindow::dockRevisionChanged, this, &PanelWindow::scheduleBarRecordsUpdate);
    connect(this, &PanelWindow::nativePlacementRevisionChanged, this, &PanelWindow::scheduleBarRecordsUpdate);
    connect(this, &PanelWindow::screenRevisionChanged, this, &PanelWindow::scheduleBarRecordsUpdate);
    scheduleBarRecordsUpdate();
    m_barWindowsTimer.setSingleShot(true);
    m_barWindowsTimer.setInterval(50);
    connect(&m_barWindowsTimer, &QTimer::timeout, this, &PanelWindow::barHostWindowsChanged);
    connect(&m_windowModel, &QAbstractItemModel::dataChanged, this, &PanelWindow::scheduleBarWindowsUpdate);
    connect(&m_windowModel, &QAbstractItemModel::rowsInserted, this, &PanelWindow::scheduleBarWindowsUpdate);
    connect(&m_windowModel, &QAbstractItemModel::rowsRemoved, this, &PanelWindow::scheduleBarWindowsUpdate);
    connect(&m_windowModel, &QAbstractItemModel::modelReset, this, &PanelWindow::scheduleBarWindowsUpdate);
    connect(&m_windowWatcher, &WindowWatcher::untrackedWindowsChanged, this, &PanelWindow::scheduleBarWindowsUpdate);
    const auto notifyGlobalVisualChange = [this]
    {
        if (!m_settingsTransactionAdoptionActive)
        {
            notifyDockRevision();
        }
    };
    connect(&m_settings, &DockSettings::magnificationChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::magnificationEnabledChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::showReflectionsChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::showIndicatorsChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::showTooltipsChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::animationDurationChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::reducedMotionChanged, this, notifyGlobalVisualChange);
    connect(&m_panelRegistry, &PanelRegistry::nativePanelTopologyChanged,
            this, [this]
            {
                m_nativePanelVisibilityStateCache.clear();
                updateDesktopSuite();
            });
    // Window changes can conceal or reveal a native panel in the "hide for
    // maximized or fullscreen windows" mode.
    const auto updateVisibility = [this]
    {
        if (profileBusy()) return;
        ++m_visibilityRevision;
        emit visibilityRevisionChanged();
        for (const QString &panelId : m_panelRegistry.panelIds())
        {
            const std::optional<ArchDock::PanelVisibilityMode> mode =
                ArchDock::normalizedPanelVisibilityMode(
                    m_panelRegistry.panelValue(
                        panelId, QStringLiteral("visibilityMode")).toString());
            if (!mode.has_value() ||
                *mode != ArchDock::PanelVisibilityMode::HideForMaximizedOrFullscreen)
            {
                continue;
            }
            const int containmentId = nativePanelId(panelId);
            const QString ownershipToken = nativeOwnershipToken(panelId).trimmed();
            if (containmentId < 0 || ownershipToken.isEmpty())
            {
                continue;
            }
            reconcileNativePanelVisibility(
                panelId,
                containmentId,
                ownershipToken,
                *mode,
                m_panelRegistry.panelValue(
                    panelId, QStringLiteral("visible")).toBool());
        }
    };
    connect(&m_windowModel, &QAbstractItemModel::dataChanged, this, updateVisibility);
    connect(&m_windowModel, &QAbstractItemModel::rowsInserted, this, updateVisibility);
    connect(&m_windowModel, &QAbstractItemModel::rowsRemoved, this, updateVisibility);
    connect(&m_windowModel, &QAbstractItemModel::modelReset, this, updateVisibility);

    // Screens added or removed are handled together once they settle.
    m_screenChangeTimer.setSingleShot(true);
    m_screenChangeTimer.setInterval(100);
    connect(&m_screenChangeTimer, &QTimer::timeout, this, &PanelWindow::handleScreensChanged);
    m_freeHostGraceClock.start();
    m_freeHostRetryTimer.setSingleShot(true);
    m_freeHostRetryTimer.setInterval(15000);
    connect(&m_freeHostRetryTimer, &QTimer::timeout, this, &PanelWindow::synchronizeFreePanels);
    m_removalCheckTimer.setInterval(5000);
    // Windows that come, go, move or change may cover a panel or uncover it.
    m_coverTimer.setSingleShot(true);
    m_coverTimer.setInterval(200);
    connect(&m_coverTimer, &QTimer::timeout, this, &PanelWindow::checkCover);
    connect(&m_windowModel, &QAbstractItemModel::dataChanged, this, &PanelWindow::scheduleCoverCheck);
    connect(&m_windowModel, &QAbstractItemModel::rowsInserted, this, &PanelWindow::scheduleCoverCheck);
    connect(&m_windowModel, &QAbstractItemModel::rowsRemoved, this, &PanelWindow::scheduleCoverCheck);
    connect(&m_windowModel, &QAbstractItemModel::modelReset, this, &PanelWindow::scheduleCoverCheck);
    connect(&m_windowWatcher, &WindowWatcher::nativePanelsChanged, this, &PanelWindow::scheduleCoverCheck);
    QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"),
        QStringLiteral("org.kde.KWin"), QStringLiteral("showingDesktopChanged"),
        this, SLOT(kwinShowingDesktopChanged(bool)));
    askShowingDesktop();
    // A KWin started again shows no desktop until told, and says nothing.
    connect(&m_windowWatcher, &WindowWatcher::kwinOwnerChanged, this, [this](bool present) {
        setShowingDesktop(false);
        if (present)
            askShowingDesktop();
    });
    connect(&m_removalCheckTimer, &QTimer::timeout, this, &PanelWindow::checkRequestedRemovals);
    // Plasma shows Panel Studio's draft once its changes pause.
    m_studioDraftHostTimer.setSingleShot(true);
    m_studioDraftHostTimer.setInterval(120);
    connect(&m_studioDraftHostTimer, &QTimer::timeout, this, &PanelWindow::showStudioDraftHost);
    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
    {
        for (QScreen *screen : QGuiApplication::screens()) watchScreen(screen);
        connect(application, &QGuiApplication::screenAdded, this, [this](QScreen *screen)
                {
                    watchScreen(screen);
                    m_screenChangePending = true;
                    m_screenChangeTimer.start();
                });
        connect(application, &QGuiApplication::screenRemoved, this, [this]
                {
                    m_screenChangePending = true;
                    m_screenChangeTimer.start();
                });
    }

    synchronizeScreenAssignments();
    scheduleNativePanelRecovery();
}

PanelWindow::~PanelWindow()
{
    delete m_settingsWindow;
    delete m_iconPropertiesWindow;
}

int PanelWindow::screenRevision() const
{
    return m_screenRevision;
}

int PanelWindow::visibilityRevision() const
{
    return m_visibilityRevision;
}

qulonglong PanelWindow::dockRevision() const
{
    return m_dockRevision;
}

qulonglong PanelWindow::presentationRequestRevision() const
{
    return m_presentationRequestRevision;
}

qulonglong PanelWindow::dockEntriesRevision() const
{
    return m_dockEntriesRevision;
}

qulonglong PanelWindow::nativePlacementRevision() const
{
    return m_nativePlacementRevision;
}

qulonglong PanelWindow::nativeVisibilityRevision() const
{
    return m_nativeVisibilityRevision;
}

void PanelWindow::notifyDockRevision()
{
    ++m_dockRevision;
    emit dockRevisionChanged();

    QDBusMessage propertiesChanged = QDBusMessage::createSignal(
        QStringLiteral("/Control"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    propertiesChanged << QStringLiteral("local.PanelWindow")
                      << QVariantMap{{QStringLiteral("dockRevision"), m_dockRevision}}
                      << QStringList{};
    QDBusConnection::sessionBus().send(propertiesChanged);
}

void PanelWindow::notifyDockEntriesRevision()
{
    ++m_dockEntriesRevision;
    emit dockEntriesRevisionChanged();

    QDBusMessage propertiesChanged = QDBusMessage::createSignal(
        QStringLiteral("/Control"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    propertiesChanged << QStringLiteral("local.PanelWindow")
                      << QVariantMap{{QStringLiteral("dockEntriesRevision"), m_dockEntriesRevision}}
                      << QStringList{};
    QDBusConnection::sessionBus().send(propertiesChanged);
}

bool PanelWindow::panelContentVisible(const QString &panelId) const
{
    if (!m_panelRegistry.panelValue(panelId, QStringLiteral("visible")).toBool()) return false;
    const auto state = m_panelPresentationStates.value(panelId);
    return state.value(QStringLiteral("hostPhase")).toString() != QStringLiteral("concealed")
        && state.value(QStringLiteral("surfaceState")).toString() != QStringLiteral("collapsed");
}

void PanelWindow::updateContentDemand()
{
    bool visible = m_settingsWindow && m_settingsWindow->isVisible();
    // The system's status is sampled (every 2 s) only while something shows
    // it: a panel as the desktop draws it (a draft, an audition or a profile
    // being tried included), or a Panel Studio view of the readings. Only a
    // panel's need makes a sample news to the panels.
    bool status = false;
    for (const auto &id : m_panelRegistry.panelIds())
    {
        if (!panelContentVisible(id)) continue;
        visible = true;
        const auto definition = runtimePanelDefinition(id);
        if (definition)
            for (const auto &segment : definition->segments)
                status = status || segment.source == QStringLiteral("status");
    }
    const bool revealed = !m_contentPublishing && visible;
    m_contentPublishing = visible;
    m_panelStatusShown = status;
    m_overlayModel.setPublishing(visible);
    m_systemStatus.setEnabled(status || m_studioStatusShown);
    if (!visible) m_contentTimer.stop();
    else if (revealed) m_contentTimer.start();
}

void PanelWindow::setStudioStatusShown(bool shown)
{
    if (m_studioStatusShown == shown) return;
    m_studioStatusShown = shown;
    updateContentDemand();
}

void PanelWindow::notifyContentRevision()
{
    if (!m_contentPublishing) return;
    ++m_contentRevision;
    emit contentRevisionChanged();
    auto signal = QDBusMessage::createSignal(QStringLiteral("/Control"),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
    signal << QStringLiteral("local.PanelWindow")
           << QVariantMap{{QStringLiteral("contentRevision"), m_contentRevision}} << QStringList{};
    QDBusConnection::sessionBus().send(signal);
}

QVariantMap PanelWindow::contentRuntimeSnapshot(const QString &panelId) const
{
    if (!m_panelRegistry.panelIds().contains(panelId)) return {};
    return {{QStringLiteral("sampleCount"), m_systemStatus.sampleCount()},
        {QStringLiteral("contentRevision"), m_contentRevision},
        {QStringLiteral("visible"), panelContentVisible(panelId)},
        {QStringLiteral("availableSources"), m_systemStatus.availableSources()},
        {QStringLiteral("overlayAvailable"), m_overlayModel.available()}};
}

QVariantList PanelWindow::statusEntriesFor(const ArchDock::PanelDefinition &definition, bool availableOnly) const
{
    auto entries = m_systemStatus.entries();
    const auto available = m_systemStatus.availableSources();
    QStringList selected;
    bool automatic = false;
    for (const auto &segment : definition.segments) {
        if (segment.source != QStringLiteral("status")) continue;
        selected.append(segment.entryIds);
        automatic = automatic || segment.entryIds.isEmpty();
    }
    entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const QVariant &entry) {
        const auto id = entry.toMap().value(QStringLiteral("appId")).toString();
        return availableOnly ? !available.contains(id)
            : !selected.contains(id) && !(automatic && available.contains(id));
    }), entries.end());
    return entries;
}

QVariantMap PanelWindow::entryOverlay(const QVariantMap &entry) const
{
    QString desktop = entry.value(QStringLiteral("desktopFileName")).toString();
    const auto id = entry.value(QStringLiteral("appId")).toString();
    if (ArchDock::PanelContent::isUrlEntryId(id))
        desktop = QFileInfo(QUrl::fromEncoded(id.mid(9).toUtf8()).toLocalFile()).fileName();
    if (desktop.isEmpty()) desktop = id;
    if (!desktop.endsWith(QStringLiteral(".desktop"))) desktop += QStringLiteral(".desktop");
    return m_overlayModel.snapshot(QFileInfo(desktop).fileName());
}

QVariantMap PanelWindow::quit()
{
    // `arch-dock --quit` records the stop before it asks, so a refused Quit
    // removes it again and leaves nothing behind. The menu's Quit has no
    // window, so a notice says why nothing happened.
    const auto refuseQuit = [this]
    {
        ArchDock::IntentionalStop::clear();
        if (m_updates)
            m_updates->tellQuitRefused();
    };
    const QVariantMap refused{{QStringLiteral("success"), false},
                              {QStringLiteral("errorCode"), QStringLiteral("update-installing")},
                              {QStringLiteral("message"),
                               tr("Arch Dock is installing its update. Quit again when it has finished.")}};
    if (m_updates && m_updates->quitRefused())
    {
        refuseQuit();
        return refused;
    }
    if (m_updates && m_updates->holdsSettings())
    {
        // Before the password was accepted, Quit stops the update first. A
        // password accepted in the same moment refuses the Quit at once.
        const auto answered = std::make_shared<bool>(false);
        m_updates->stopForQuit([this, refuseQuit, answered](bool refusedNow)
        {
            *answered = true;
            if (refusedNow)
                refuseQuit();
            else
                stopIntentionally(QStringLiteral("quit"));
        });
        if (*answered && m_updates->quitRefused())
            return refused;
        return {{QStringLiteral("success"), true},
                {QStringLiteral("message"), tr("The update stops first, then Arch Dock quits.")}};
    }
    if (profileBusy())
    {
        return {{QStringLiteral("success"), false},
                {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")},
                {QStringLiteral("message"),
                 tr("A profile is being applied. Quit Arch Dock again when it has finished.")}};
    }
    return stopIntentionally(QStringLiteral("quit"));
}

QVariantMap PanelWindow::stopIntentionally(const QString &reason)
{
    // Only Arch Dock's own Quit (the menu entry, `arch-dock --quit`) keeps it
    // stopped for the rest of the login. A stop signal (System Monitor's End
    // or Quit, pkill, a package update's restart) stops it for now, as a
    // crash does: the next request from a panel or the KWin watcher starts
    // it again. That is how people end a program that misbehaves, and a stop
    // that silently outlived it left the desktop without its panels.
    // Record first: from here on an activation request must find the stop.
    const bool quit = reason != QStringLiteral("signal");
    QString error;
    const bool recorded = quit && ArchDock::IntentionalStop::record(reason, &error);
    if (!quit)
    {
        qInfo().noquote() << (ArchDock::IntentionalStop::executableReplaced()
            ? QStringLiteral("Arch Dock stops for its update and starts again when a panel asks for it.")
            : QStringLiteral("Arch Dock was stopped by a signal (System Monitor or pkill); "
                             "it starts again when a panel or the application menu asks for it."));
    }
    else if (recorded)
    {
        qInfo().noquote() << "Arch Dock was quit and stays stopped for this login, until it is"
                          << "started from the application menu or with a panel's Start button.";
    }
    else
    {
        qWarning().noquote() << "Arch Dock stops without staying stopped:" << error;
    }
    if (m_presetAudition && m_presetAudition->state() != QStringLiteral("IDLE"))
    {
        m_presetAudition->cancel();
    }
    // Panel Studio's draft is dropped and Plasma put back before the quit.
    clearPanelSettingsPreview();
    restoreStudioDraftHosts();
    // Arch Dock's own bars go with it. After a Quit the KWin watcher is
    // released, since its window reports would start Arch Dock again; after
    // a signal it stays, so the next window event does, as after a crash.
    m_barHost.stop();
    if (quit)
        WindowWatcher::releaseKWinScripts();
    QMetaObject::invokeMethod(QCoreApplication::instance(), &QCoreApplication::quit,
                              Qt::QueuedConnection);
    return {{QStringLiteral("success"), true},
            {QStringLiteral("persistent"), recorded},
            {QStringLiteral("message"), error}};
}

void PanelWindow::updateDesktopSuite()
{
    synchronizeFreePanels();
    scheduleNativePanelRecovery();
    ++m_visibilityRevision;
    emit visibilityRevisionChanged();
}

void PanelWindow::syncRegistryFromLegacySettings()
{
    if (profileBusy()) return;

    const QVariantMap sharedValues{
        {QStringLiteral("screen"), m_settings.monitorIndex()},
        {QStringLiteral("screenId"), screenIdForIndex(m_settings.monitorIndex())},
        {QStringLiteral("iconSize"), m_settings.iconSize()},
        {QStringLiteral("spacing"), m_settings.spacing()},
        {QStringLiteral("appearance"), m_settings.appearancePreset()},
        {QStringLiteral("shape"), m_settings.panelShape()},
        {QStringLiteral("iconShape"), m_settings.iconTileShape()},
        {QStringLiteral("opacity"), m_settings.panelOpacity()}};

    for (const QString &panelId : {QStringLiteral("bottom"),
                                   QStringLiteral("top"),
                                   QStringLiteral("side")})
    {
        if (m_panelRegistry.isBuiltIn(panelId))
        {
            m_panelRegistry.updatePanel(panelId, sharedValues);
        }
    }

    m_panelRegistry.updatePanel(
        QStringLiteral("bottom"),
        {{QStringLiteral("edge"), m_settings.position()},
         {QStringLiteral("alignment"), m_settings.alignment()},
         {QStringLiteral("type"), m_settings.bottomPanelType()},
         {QStringLiteral("visibilityMode"), m_settings.autoHide()
             ? QStringLiteral("auto-hide")
             : QStringLiteral("always")}});
    m_panelRegistry.updatePanel(
        QStringLiteral("top"),
        {{QStringLiteral("type"), m_settings.topPanelType()}});
    m_panelRegistry.updatePanel(
        QStringLiteral("side"),
        {{QStringLiteral("type"), m_settings.sidePanelType()}});

    // Whether a panel is shown is its own record's: the old global switches
    // (bottomPanelVisible and the like, on by default) no longer show a
    // panel the user hid or removed, at every start.

    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        if (nativePanelId(panelId) >= 0 && !synchronizeNativePanelPlacement(panelId))
        {
            qWarning() << "Could not update the native Plasma panel placement for" << panelId;
        }
    }
}

// A closed Panel Studio is destroyed, not kept hidden. Its live preview and
// preset cards are the heaviest scene the backend loads: hidden, they still
// held hundreds of megabytes and their animations kept waking the backend.
// Studio only hides when it is closed, and closing has already applied or
// discarded its changes, so nothing is lost.
QWindow *PanelWindow::settingsWindow()
{
    if (m_settingsWindow)
        return m_settingsWindow;
    QWindow *window = createUtilityWindow(
        QUrl(QStringLiteral("qrc:/qt/qml/ArchDock/qml/runtime/SettingsPopup.qml")));
    m_settingsWindow = window;
    if (window)
    {
        connect(window, &QWindow::visibleChanged, this, [this, window](bool visible)
                {
                    if (!visible && m_settingsWindow == window)
                    {
                        // Detached first: a request to open Studio before the
                        // deletion builds a new window instead of this one.
                        m_settingsWindow = nullptr;
                        window->deleteLater();
                        // Its draft goes with it, as Cancel takes it.
                        clearPanelSettingsPreview();
                    }
                    // A closed Studio shows no readings, and an update that
                    // has not passed the password window stops with it. An
                    // opened one shows whether a restart is needed.
                    if (!visible)
                    {
                        m_studioStatusShown = false;
                        if (m_updates)
                            m_updates->studioClosing();
                    }
                    else if (m_updates)
                        m_updates->studioOpened();
                    updateContentDemand();
                });
    }
    return window;
}

void PanelWindow::showSettings()
{
    // Opened on its own, not from a panel, the Studio starts on its page for
    // a new panel; a panel's own settings open from its "Configure Arch
    // Dock…".
    showNewPanelPage();
}

void PanelWindow::showPanelSettings(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    m_panelRegistry.setActivePanelId(panelId);
    // The Studio edits one panel at a time, the one it was opened from; one
    // already open moves to this panel (asking first about changes that
    // are not applied yet).
    QWindow *studio = settingsWindow();
    if (studio)
    {
        QVariant moved;
        QMetaObject::invokeMethod(studio, "editPanel", Q_RETURN_ARG(QVariant, moved),
                                  Q_ARG(QVariant, panelId));
    }
    presentUtilityWindow(studio, panelId);
}

void PanelWindow::showUpdatesPage()
{
    QWindow *studio = settingsWindow();
    if (studio)
    {
        QVariant opened;
        QMetaObject::invokeMethod(studio, "openUpdatesPage", Q_RETURN_ARG(QVariant, opened));
    }
    presentUtilityWindow(studio, m_panelRegistry.activePanelId());
}

#ifdef ARCHDOCK_TESTING
void PanelWindow::setUpdatesForTesting(QObject *standIn)
{
    m_engine.rootContext()->setContextProperty(QStringLiteral("archDockUpdates"), standIn);
}
#endif

void PanelWindow::showNewPanelPage()
{
    QWindow *studio = settingsWindow();
    if (studio)
    {
        QVariant opened;
        QMetaObject::invokeMethod(studio, "openNewPanelPage", Q_RETURN_ARG(QVariant, opened),
                                  Q_ARG(QVariant, QStringLiteral("free")));
    }
    presentUtilityWindow(studio, m_panelRegistry.activePanelId());
}

bool PanelWindow::openKdeWidgetPreview(const QString &panelId, const QString &appletId)
{
    const QString pluginId = appletId.trimmed();
    if (!m_panelRegistry.panelIds().contains(panelId) || pluginId.isEmpty())
    {
        return false;
    }

    for (const QChar character : pluginId)
    {
        if (!character.isLetterOrNumber() && character != QLatin1Char('.') &&
            character != QLatin1Char('_') && character != QLatin1Char('-'))
        {
            return false;
        }
    }

    const QString viewer = QStandardPaths::findExecutable(QStringLiteral("plasmoidviewer"));
    if (viewer.isEmpty())
    {
        return false;
    }

    QStringList widgets = m_panelRegistry.panelValue(panelId, QStringLiteral("kdeWidgets")).toStringList();
    if (!widgets.contains(pluginId))
    {
        widgets.append(pluginId);
        m_panelRegistry.setPanelValue(panelId, QStringLiteral("kdeWidgets"), widgets);
    }

    const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
    const QString location = edge == QStringLiteral("top") ? QStringLiteral("topedge")
        : edge == QStringLiteral("bottom") ? QStringLiteral("bottomedge")
        : edge == QStringLiteral("left") ? QStringLiteral("leftedge")
        : edge == QStringLiteral("right") ? QStringLiteral("rightedge")
        : QStringLiteral("bottomedge");
    const QString formFactor = edge == QStringLiteral("left") || edge == QStringLiteral("right")
        ? QStringLiteral("vertical")
        : QStringLiteral("horizontal");
    const QSize size(
        m_panelRegistry.panelValue(panelId, QStringLiteral("width")).toInt(),
        m_panelRegistry.panelValue(panelId, QStringLiteral("height")).toInt());

    return QProcess::startDetached(
        viewer,
        {QStringLiteral("--applet"), pluginId,
         QStringLiteral("--formfactor"), formFactor,
         QStringLiteral("--location"), location,
         QStringLiteral("--xPosition"), m_panelRegistry.panelValue(panelId, QStringLiteral("x")).toString(),
         QStringLiteral("--yPosition"), m_panelRegistry.panelValue(panelId, QStringLiteral("y")).toString(),
         QStringLiteral("--size"), QStringLiteral("%1x%2").arg(size.width()).arg(size.height())});
}

QVariantMap PanelWindow::showIconProperties(
    const QString &panelId,
    const QString &entryIdentity)
{
    QVariantMap snapshot = iconOverrideSnapshotForIdentity(
        panelId, entryIdentity);
    if (!snapshot.value(QStringLiteral("success")).toBool())
    {
        return snapshot;
    }

    if (!m_iconPropertiesWindow)
    {
        m_iconPropertiesWindow = createUtilityWindow(
            QUrl(QStringLiteral("qrc:/qt/qml/ArchDock/qml/runtime/IconPropertiesWindow.qml")));
    }

    if (!m_iconPropertiesWindow)
    {
        snapshot.insert(QStringLiteral("success"), false);
        snapshot.insert(QStringLiteral("status"), QStringLiteral("unavailable"));
        snapshot.insert(
            QStringLiteral("errorCode"),
            QStringLiteral("editor-window-unavailable"));
        snapshot.insert(
            QStringLiteral("errorMessage"),
            QStringLiteral("the Icon Properties window could not be created"));
        return snapshot;
    }

    m_iconPropertiesWindow->setProperty("editorSnapshot", snapshot);
    presentUtilityWindow(m_iconPropertiesWindow, panelId);
    snapshot.insert(QStringLiteral("status"), QStringLiteral("opened"));
    snapshot.insert(QStringLiteral("editorVisible"),
                    m_iconPropertiesWindow->isVisible());
    return snapshot;
}

QWindow *PanelWindow::createUtilityWindow(const QUrl &source)
{
    QQmlComponent component(&m_engine, source, this);
    if (component.status() != QQmlComponent::Ready)
    {
        qWarning().noquote() << component.errorString();
        return nullptr;
    }

    QObject *rootObject = component.create(m_engine.rootContext());
    auto *window = qobject_cast<QWindow *>(rootObject);
    if (window)
    {
        return window;
    }

    qWarning() << "Unable to create Arch Dock utility window:" << source;
    delete rootObject;
    return nullptr;
}

void PanelWindow::presentUtilityWindow(QWindow *window, const QString &panelId)
{
    if (!window)
    {
        return;
    }

    // An editor opens where the panel it edits is. With no panel to follow it
    // opens under the pointer, and on the primary screen only as a last resort.
    QScreen *screen = m_panelRegistry.panelIds().contains(panelId)
        ? screenForPanel(panelId) : nullptr;
    if (!screen)
    {
        screen = QGuiApplication::screenAt(QCursor::pos());
    }
    if (!screen)
    {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen)
    {
        window->setScreen(screen);
        fitUtilityWindow(window);
        const QRect geometry = screen->availableGeometry();
        window->setPosition(
            geometry.x() + (geometry.width() - window->width()) / 2,
            geometry.y() + (geometry.height() - window->height()) / 2);
    }

    window->show();
    window->raise();
    window->requestActivate();

    const QString title = window->title();
    QTimer::singleShot(
        100,
        this,
        [this, title]
        {
            m_actionBridge.focusWindow(QStringLiteral("arch-dock"), title);
        });
}

void PanelWindow::resetSettings()
{
    // While an update runs nothing is changed (UpdateHold), so the restart
    // that ends it cuts off nothing.
    if (ArchDock::Updates::UpdateHold::held())
        return;
    if (profileBusy()) return;

    m_settings.reset();
    syncRegistryFromLegacySettings();
}

void PanelWindow::toggleAutoHide()
{
    if (ArchDock::Updates::UpdateHold::held())
        return;
    if (profileBusy()) return;

    const bool autoHide = m_panelRegistry.panelValue(
        QStringLiteral("bottom"),
        QStringLiteral("visibilityMode")).toString() == QStringLiteral("auto-hide");
    setPanelVisibilityMode(
        QStringLiteral("bottom"),
        autoHide ? QStringLiteral("always") : QStringLiteral("auto-hide"));
}

void PanelWindow::toggleDesktopSuite()
{
    if (ArchDock::Updates::UpdateHold::held())
        return;
    if (profileBusy()) return;

    m_settings.setDesktopSuite(!m_settings.desktopSuite());
    syncRegistryFromLegacySettings();
}

void PanelWindow::toggleTopLauncher()
{
    if (ArchDock::Updates::UpdateHold::held())
        return;
    if (profileBusy()) return;

    const bool launcherVisible = !m_settings.topLauncherVisible();
    if (!setPanelVisible(
            QStringLiteral("top"),
            m_settings.desktopSuite() && launcherVisible))
    {
        return;
    }
    m_settings.setTopLauncherVisible(launcherVisible);
}

void PanelWindow::toggleSideRail()
{
    if (ArchDock::Updates::UpdateHold::held())
        return;
    if (profileBusy()) return;

    const bool railVisible = !m_settings.sideRailVisible();
    if (!setPanelVisible(
            QStringLiteral("side"),
            m_settings.desktopSuite() && railVisible))
    {
        return;
    }
    m_settings.setSideRailVisible(railVisible);
}

void PanelWindow::toggleBottomPanel()
{
    if (ArchDock::Updates::UpdateHold::held())
        return;
    if (profileBusy()) return;

    const bool visible = !m_panelRegistry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("visible")).toBool();
    if (!setPanelVisible(QStringLiteral("bottom"), visible))
    {
        return;
    }
    if (m_settings.bottomPanelVisible() != visible)
    {
        m_settings.setBottomPanelVisible(visible);
    }
}

void PanelWindow::applyProfile(const QString &profileName)
{
    if (ArchDock::Updates::UpdateHold::held())
        return;
    if (profileBusy()) return;

    m_settings.applyProfile(profileName);
    syncRegistryFromLegacySettings();
}

void PanelWindow::openSystemSettings(const QString &module)
{
    QStringList arguments;
    if (module == QStringLiteral("audio"))
    {
        arguments.append(QStringLiteral("kcm_pulseaudio"));
    }
    else if (module == QStringLiteral("network"))
    {
        arguments.append(QStringLiteral("kcm_networkmanagement"));
    }
    else if (module == QStringLiteral("bluetooth"))
    {
        arguments.append(QStringLiteral("kcm_bluetooth"));
    }
    else if (module == QStringLiteral("keyboard"))
    {
        arguments.append(QStringLiteral("kcm_keyboard"));
    }
    else if (module == QStringLiteral("power"))
    {
        arguments.append(QStringLiteral("kcm_powerdevilprofilesconfig"));
    }

    QProcess::startDetached(QStringLiteral("systemsettings"), arguments);
}
