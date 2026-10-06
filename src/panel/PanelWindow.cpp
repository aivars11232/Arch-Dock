// PanelWindow's lifetime and shared plumbing: construction (models, D-Bus
// objects, watchers and the connections between them), the revision counters
// consumers follow, publishing live content (status samples and overlays)
// while a panel shows it, Quit, the Studio and utility windows, and the legacy
// desktop-suite toggles. The rest of the class is split by topic over the
// PanelWindow*.cpp files beside this one; PanelWindow.h lists them.

#include "PanelWindow.h"
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
#include <utility>

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
    });
    m_engine.rootContext()->setContextProperty(QStringLiteral("profileManager"), m_profileManager);
    connect(m_profileManager, &ArchDock::ProfileManager::changed, this, [this] {
        notifyDockRevision();
        notifyDockEntriesRevision();
        if (m_screenChangePending && !profileBusy()) m_screenChangeTimer.start();
    });
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("systemStatus"),
        &m_systemStatus);
    m_engine.rootContext()->setContextProperty(
        QStringLiteral("panelController"),
        this);

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
                if (newOwner.isEmpty())
                {
                    ++m_nativePanelRecoveryGeneration;
                    m_nativePanelVisibilityStateCache.clear();
                    if (m_presetAudition && m_presetAudition->state() != QStringLiteral("IDLE"))
                        m_presetAudition->recoverInterruptedPreview();
                    return;
                }
                m_nativePanelVisibilityStateCache.clear();
                scheduleNativePanelRecovery();
            });

    // Live content (status samples, overlays) is published in batches of at
    // most one per 100 ms, and only while a panel shows it.
    m_contentTimer.setSingleShot(true);
    m_contentTimer.setInterval(100);
    connect(&m_contentTimer, &QTimer::timeout, this, &PanelWindow::notifyContentRevision);
    connect(&m_overlayModel, &ArchDock::OverlayModel::changed, this, [this] {
        if (m_contentPublishing && !m_contentTimer.isActive()) m_contentTimer.start();
    });
    connect(&m_systemStatus, &SystemStatus::statusChanged, this, [this] {
        if (m_contentPublishing && !m_contentTimer.isActive()) m_contentTimer.start();
    });
    connect(&m_panelRegistry, &PanelRegistry::revisionChanged, this, &PanelWindow::updateContentDemand);
    connect(&m_windowWatcher, &WindowWatcher::nativePanelsChanged,
            this, &PanelWindow::notifyNativeVisibilityRevision);
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
    bool status = visible; // Studio needs current provider capabilities and preview data.
    for (const auto &id : m_panelRegistry.panelIds())
    {
        if (!panelContentVisible(id)) continue;
        visible = true;
        const auto definition = m_panelRegistry.panelDefinition(id);
        if (definition)
            for (const auto &segment : definition->segments)
                status = status || segment.source == QStringLiteral("status");
    }
    const bool revealed = !m_contentPublishing && visible;
    m_contentPublishing = visible;
    m_overlayModel.setPublishing(visible);
    m_systemStatus.setEnabled(status);
    if (!visible) m_contentTimer.stop();
    else if (revealed) m_contentTimer.start();
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
    // Record first: from here on an activation request must find the stop.
    QString error;
    const bool recorded = ArchDock::IntentionalStop::record(reason, &error);
    if (!recorded)
    {
        qWarning().noquote() << "Arch Dock stops without staying stopped:" << error;
    }
    if (m_presetAudition && m_presetAudition->state() != QStringLiteral("IDLE"))
    {
        m_presetAudition->cancel();
    }
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

    const std::array visibilityRequests{
        std::pair{QStringLiteral("bottom"), m_settings.bottomPanelVisible()},
        std::pair{QStringLiteral("top"), m_settings.desktopSuite() && m_settings.topLauncherVisible()},
        std::pair{QStringLiteral("side"), m_settings.desktopSuite() && m_settings.sideRailVisible()},
    };
    for (const auto &[panelId, visible] : visibilityRequests)
    {
        if (!setPanelVisible(panelId, visible))
        {
            qWarning() << "Could not synchronize native panel visibility for" << panelId;
        }
    }

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
                    }
                    updateContentDemand();
                });
    }
    return window;
}

void PanelWindow::showSettings()
{
    QWindow *studio = settingsWindow();
    const QString selected = studio
        ? studio->property("selectedPanelId").toString() : QString{};
    presentUtilityWindow(studio,
                         selected.isEmpty() ? m_panelRegistry.activePanelId() : selected);
}

void PanelWindow::showPanelSettings(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    m_panelRegistry.setActivePanelId(panelId);
    QWindow *studio = settingsWindow();
    if (studio)
    {
        studio->setProperty("selectedPanelId", panelId);
        studio->setProperty("mainTabIndex", 1);
        studio->setProperty("subTabIndex", 0);
    }
    presentUtilityWindow(studio, panelId);
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
    if (profileBusy()) return;

    m_settings.reset();
    syncRegistryFromLegacySettings();
}

void PanelWindow::toggleAutoHide()
{
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
    if (profileBusy()) return;

    m_settings.setDesktopSuite(!m_settings.desktopSuite());
    syncRegistryFromLegacySettings();
}

void PanelWindow::toggleTopLauncher()
{
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
