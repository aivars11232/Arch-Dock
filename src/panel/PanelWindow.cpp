#include "PanelWindow.h"

#include "../NativeContainmentLifecycle.h"
#include "../PanelPlacement.h"
#include "../PanelVisibility.h"
#include "../PlasmaScriptResult.h"
#include "../ScreenIdentity.h"
#include "../integration/PlasmaPanelAdapter.h"
#include "../model/IconEntryIdentity.h"
#include "../model/FolderContentModel.h"
#include "../model/PanelSettingsSchema.h"
#include "IconOverrideTransaction.h"
#include "../presets/PresetApplication.h"

#include <KIO/OpenUrlJob>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDBusConnection>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegion>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QStandardPaths>
#include <QSettings>
#include <QSize>
#include <QTimer>
#include <QUuid>
#include <QWindow>

#include <array>
#include <limits>
#include <utility>

namespace
{
// D-Bus variants preserve compound values as QDBusArgument. Decode only the
// bounded segment payload here; the model still validates every key and type.
QVariant decodedSegmentValue(QVariant value, int depth = 0)
{
    if (depth > 8)
        return {};
    if (value.metaType() == QMetaType::fromType<QDBusVariant>())
        return decodedSegmentValue(value.value<QDBusVariant>().variant(), depth + 1);
    if (value.metaType() == QMetaType::fromType<QDBusArgument>())
    {
        const auto argument = value.value<QDBusArgument>();
        const QString signature = argument.currentSignature();
        if (signature == QStringLiteral("av"))
            value = qdbus_cast<QVariantList>(argument);
        else if (signature == QStringLiteral("a{sv}"))
            value = qdbus_cast<QVariantMap>(argument);
        else if (signature == QStringLiteral("aa{sv}"))
        {
            QVariantList records;
            for (const auto &record : qdbus_cast<QList<QVariantMap>>(argument))
                records.append(record);
            value = records;
        }
        else
            return {};
    }
    if (value.metaType().id() == QMetaType::QVariantList)
    {
        auto list = value.toList();
        if (list.size() > 512)
            return {};
        for (auto &item : list)
            item = decodedSegmentValue(item, depth + 1);
        return list;
    }
    if (value.metaType().id() == QMetaType::QVariantMap)
    {
        auto map = value.toMap();
        if (map.size() > 16)
            return {};
        for (auto it = map.begin(); it != map.end(); ++it)
            it.value() = decodedSegmentValue(it.value(), depth + 1);
        return map;
    }
    return value;
}

QString plasmaScriptStringLiteral(const QString &value)
{
    QJsonArray values;
    values.append(value);
    const QString encoded = QString::fromUtf8(
        QJsonDocument(values).toJson(QJsonDocument::Compact));
    return encoded.mid(1, encoded.size() - 2);
}

bool isNativeDockPanelType(const QString &type)
{
    return type == QStringLiteral("empty") ||
        type == QStringLiteral("launcher") ||
        type == QStringLiteral("tasks") ||
        type == QStringLiteral("hybrid");
}

    bool isNativeDockPanelEdge(const QString &edge)
    {
        return edge == QStringLiteral("top") ||
        edge == QStringLiteral("bottom") ||
        edge == QStringLiteral("left") ||
        edge == QStringLiteral("right");
    }

QRect panelGeometryForVisibility(const QRect &screenGeometry,
                                 const ArchDock::NativePanelPlacement &placement)
{
    if (!screenGeometry.isValid())
    {
        return {};
    }

    const bool vertical = placement.edge == ArchDock::NativePanelEdge::Left ||
        placement.edge == ArchDock::NativePanelEdge::Right;
    const int axisStart = vertical ? screenGeometry.top() : screenGeometry.left();
    const int axisLength = vertical ? screenGeometry.height() : screenGeometry.width();
    const int crossLength = vertical ? screenGeometry.width() : screenGeometry.height();
    const int panelLength = placement.lengthMode == ArchDock::NativePanelLengthMode::Fill
        ? axisLength
        : qBound(1, placement.fixedLength, axisLength);
    const int thickness = qBound(1, placement.thickness, crossLength);
    const int offset = qMax(0, placement.offset);

    int requestedStart = axisStart;
    switch (placement.alignment)
    {
    case ArchDock::NativePanelAlignment::Start:
        requestedStart = axisStart + offset;
        break;
    case ArchDock::NativePanelAlignment::Center:
        requestedStart = axisStart + ((axisLength - panelLength) / 2) + offset;
        break;
    case ArchDock::NativePanelAlignment::End:
        requestedStart = axisStart + axisLength - panelLength - offset;
        break;
    }
    const int alongStart = qBound(
        axisStart, requestedStart, axisStart + axisLength - panelLength);

    switch (placement.edge)
    {
    case ArchDock::NativePanelEdge::Top:
        return {alongStart, screenGeometry.top(), panelLength, thickness};
    case ArchDock::NativePanelEdge::Bottom:
        return {alongStart,
                screenGeometry.bottom() - thickness + 1,
                panelLength,
                thickness};
    case ArchDock::NativePanelEdge::Left:
        return {screenGeometry.left(), alongStart, thickness, panelLength};
    case ArchDock::NativePanelEdge::Right:
        return {screenGeometry.right() - thickness + 1,
                alongStart,
                thickness,
                panelLength};
    }

    return {};
}

bool panelTypeNeedsDockApplet(const QString &type)
{
    return type == QStringLiteral("launcher") ||
        type == QStringLiteral("tasks") ||
        type == QStringLiteral("hybrid");
}

QString screenResolutionReasonName(ArchDock::ScreenResolutionReason reason)
{
    switch (reason)
    {
    case ArchDock::ScreenResolutionReason::NoScreens:
        return QStringLiteral("no-screens");
    case ArchDock::ScreenResolutionReason::StableIdMatch:
        return QStringLiteral("stable-id-match");
    case ArchDock::ScreenResolutionReason::StoredIndexFallback:
        return QStringLiteral("stored-index-fallback");
    case ArchDock::ScreenResolutionReason::BoundedIndexFallback:
        return QStringLiteral("bounded-index-fallback");
    }
    return QStringLiteral("unknown");
}

QString nativePlacementIssueCodeName(ArchDock::NativePlacementIssueCode code)
{
    switch (code)
    {
    case ArchDock::NativePlacementIssueCode::UnknownAlias:
        return QStringLiteral("unknown-alias");
    case ArchDock::NativePlacementIssueCode::OutOfRange:
        return QStringLiteral("out-of-range");
    case ArchDock::NativePlacementIssueCode::ConflictingValues:
        return QStringLiteral("conflicting-values");
    case ArchDock::NativePlacementIssueCode::MinimumExceedsMaximum:
        return QStringLiteral("minimum-exceeds-maximum");
    case ArchDock::NativePlacementIssueCode::FixedLengthOutsideRange:
        return QStringLiteral("fixed-length-outside-range");
    case ArchDock::NativePlacementIssueCode::CapabilityUnavailable:
        return QStringLiteral("capability-unavailable");
    }
    return QStringLiteral("unknown-issue");
}

ArchDock::FreePanelRemovalOutcome freePanelRemovalOutcome(
    const std::optional<int> &marker)
{
    if (!marker.has_value())
    {
        return ArchDock::FreePanelRemovalOutcome::QueryFailed;
    }
    if (*marker == 1)
    {
        return ArchDock::FreePanelRemovalOutcome::Removed;
    }
    if (*marker == 2)
    {
        return ArchDock::FreePanelRemovalOutcome::AlreadyAbsent;
    }
    return ArchDock::FreePanelRemovalOutcome::Refused;
}
}

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
    for (const QString &panelId : m_panelRegistry.panelIds())
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
        m_engine.rootContext()->setContextProperty(
            QStringLiteral("systemStatus"),
            &m_systemStatus);
        m_engine.rootContext()->setContextProperty(
            QStringLiteral("panelController"),
            this);

        QDBusConnection sessionBus = QDBusConnection::sessionBus();
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
    const auto updateVisibility = [this]
    {
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

    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
    {
        connect(application, &QGuiApplication::screenAdded, this, [this]
                {
                    handleScreensChanged();
                });
        connect(application, &QGuiApplication::screenRemoved, this, [this]
                {
                    handleScreensChanged();
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

void PanelWindow::recordNativePanelPlacementResult(
    const QString &panelId,
    ArchDock::PlasmaPanelPlacementApplyResult result)
{
    result.savedIntent = nativePanelPlacementIntent(panelId);
    QVariantMap structured = result.toVariantMap();
    structured.insert(QStringLiteral("panelId"), panelId);
    m_nativePanelPlacementResults.insert(panelId, structured);
    ++m_nativePlacementRevision;
    emit nativePlacementRevisionChanged();

    QDBusMessage propertiesChanged = QDBusMessage::createSignal(
        QStringLiteral("/Control"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    propertiesChanged << QStringLiteral("local.PanelWindow")
                      << QVariantMap{{QStringLiteral("nativePlacementRevision"),
                                      m_nativePlacementRevision}}
                      << QStringList{};
    QDBusConnection::sessionBus().send(propertiesChanged);
}

QVariantMap PanelWindow::nativePanelPlacementStatus(const QString &panelId) const
{
    const auto result = m_nativePanelPlacementResults.constFind(panelId);
    if (result != m_nativePanelPlacementResults.cend())
    {
        return result.value();
    }
    return {
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("success"), false},
        {QStringLiteral("status"), QStringLiteral("failed")},
        {QStringLiteral("errorCode"), QStringLiteral("not-attempted")},
        {QStringLiteral("requested"), QVariantMap{}},
        {QStringLiteral("applied"), QVariantMap{}},
        {QStringLiteral("unsupported"), QVariantList{}},
        {QStringLiteral("failed"), QVariantList{}},
        {QStringLiteral("savedIntent"), nativePanelPlacementIntent(panelId)},
        {QStringLiteral("hostState"), QVariantMap{}},
        {QStringLiteral("ownershipVerified"), false},
        {QStringLiteral("rollbackAttempted"), false},
        {QStringLiteral("rollbackSucceeded"), false},
        {QStringLiteral("rollbackErrorCode"), QString{}},
    };
}

void PanelWindow::recordNativePanelVisibilityResult(const QString &panelId,
                                                     QVariantMap result)
{
    result.insert(QStringLiteral("panelId"), panelId);
    m_nativePanelVisibilityResults.insert(panelId, std::move(result));
    notifyNativeVisibilityRevision();
}

void PanelWindow::notifyNativeVisibilityRevision()
{
    ++m_nativeVisibilityRevision;
    emit nativeVisibilityRevisionChanged();

    QDBusMessage propertiesChanged = QDBusMessage::createSignal(
        QStringLiteral("/Control"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    propertiesChanged << QStringLiteral("local.PanelWindow")
                      << QVariantMap{{QStringLiteral("nativeVisibilityRevision"),
                                      m_nativeVisibilityRevision}}
                      << QStringList{};
    QDBusConnection::sessionBus().send(propertiesChanged);
}

QVariantMap PanelWindow::nativePanelVisibilityStatus(const QString &panelId) const
{
    const auto recorded = m_nativePanelVisibilityResults.constFind(panelId);
    if (recorded != m_nativePanelVisibilityResults.cend())
    {
        return recorded.value();
    }

    const bool nativePanel = m_panelRegistry.panelIds().contains(panelId) &&
        isNativeDockPanelEdge(
            m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString());
    const ArchDock::NativeVisibilityCapabilities capabilities =
        nativeVisibilityCapabilities(panelId);
    return {
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("success"), false},
        {QStringLiteral("verified"), false},
        {QStringLiteral("status"), QStringLiteral("not-attempted")},
        {QStringLiteral("errorCode"), QStringLiteral("not-attempted")},
        {QStringLiteral("requestedMode"),
         nativePanel
             ? m_panelRegistry.panelValue(
                   panelId, QStringLiteral("visibilityMode")).toString()
             : QString{}},
        {QStringLiteral("effectiveMode"), QString{}},
        {QStringLiteral("hostMode"), QString{}},
        {QStringLiteral("supportedModes"),
         nativePanel
             ? ArchDock::supportedNativeVisibilityModes(capabilities)
             : QStringList{}},
        {QStringLiteral("watcherAvailable"), m_windowWatcher.available()},
        {QStringLiteral("fallbackAttempted"), false},
        {QStringLiteral("fallbackApplied"), false},
        {QStringLiteral("fallbackReason"), QString{}},
        {QStringLiteral("fallbackErrorCode"), QString{}},
        {QStringLiteral("ownershipVerified"), false},
        {QStringLiteral("rollbackAttempted"), false},
        {QStringLiteral("rollbackSucceeded"), false},
        {QStringLiteral("rollbackErrorCode"), QString{}},
    };
}

QVariantMap PanelWindow::nativePanelHostState(const QString &panelId, const QVariantMap &bounds) const
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition || definition->host.kind != ArchDock::PanelHostKind::NativeEdge
        || definition->host.nativePanelId < 0 || definition->host.nativeOwnershipToken.isEmpty())
        return {{QStringLiteral("available"), false}};
    return m_windowWatcher.nativePanelState(QRectF(bounds.value(QStringLiteral("x")).toDouble(),
        bounds.value(QStringLiteral("y")).toDouble(), bounds.value(QStringLiteral("width")).toDouble(),
        bounds.value(QStringLiteral("height")).toDouble()));
}

QVariantMap PanelWindow::dockConfiguration(const QString &panelId) const
{
    QVariantMap configuration = m_settings.transactionSnapshot();
    const QVariantMap panel = m_panelRegistry.panelSnapshot(panelId);
    if (panel.isEmpty())
    {
        return {};
    }
    for (auto it = panel.cbegin(); it != panel.cend(); ++it)
    {
        configuration.insert(it.key(), it.value());
    }
    configuration.insert(QStringLiteral("panel"), panel);
    configuration.insert(
        QStringLiteral("globalSettings"), m_settings.transactionSnapshot());
    const QVariantMap capabilityResolution = resolvePanelCapabilities(panelId);
    configuration.insert(
        QStringLiteral("capabilityResolution"), capabilityResolution);
    configuration.insert(
        QStringLiteral("effectiveRendererTier"),
        capabilityResolution.value(QStringLiteral("renderer"))
            .toMap()
            .value(QStringLiteral("effectiveTier")));
    return configuration;
}

std::optional<ArchDock::PanelDefinition> PanelWindow::runtimePanelDefinition(
    const QString &panelId) const
{
    if (m_presetAudition)
    {
        const auto preview = m_presetAudition->previewDefinition(panelId);
        if (preview) return preview;
    }
    return m_panelRegistry.panelDefinition(panelId);
}

QVariantMap PanelWindow::presetResource(const QString &kind, const QString &presetId,
    bool builtInOnly) const
{
    if (!ArchDock::PresetIdentity::isValidId(presetId) ||
        (kind != QStringLiteral("panel") && kind != QStringLiteral("icon"))) return {};
    if (ArchDock::PresetIdentity::isUserId(presetId))
    {
        if (builtInOnly) return {};
        const ArchDock::UserPresetStore store(m_presetLibrary.userRoot());
        if (kind == QStringLiteral("panel"))
        {
            for (const auto &preset : store.panelPresets())
                if (preset.identity.id == presetId) return preset.toVariantMap();
        }
        else
        {
            for (const auto &preset : store.iconPresets())
                if (preset.identity.id == presetId) return preset.toVariantMap();
        }
        return {};
    }
    const auto cards = kind == QStringLiteral("panel")
        ? m_presetLibrary.panelPresets(QStringLiteral("builtin"))
        : m_presetLibrary.iconPresets(QStringLiteral("builtin"));
    for (const auto &entry : cards)
    {
        if (entry.toMap().value(QStringLiteral("id")).toString() != presetId) continue;
        QVector<ArchDock::PresetValidationDiagnostic> diagnostics;
        return ArchDock::readPresetDefinitionFile(QDir(m_presetLibrary.builtInRoot()).filePath(
            (kind == QStringLiteral("panel") ? QStringLiteral("panels/") : QStringLiteral("icons/")) +
                presetId + QStringLiteral(".json")), &diagnostics).value_or(QVariantMap{});
    }
    return {};
}

std::optional<ArchDock::PresetPreviewSession::Prepared> PanelWindow::preparePresetPreview(
    const QVariantMap &request, QString *errorCode) const
{
    using namespace ArchDock;
    const auto fail = [errorCode](const QString &code) -> std::optional<PresetPreviewSession::Prepared>
    {
        if (errorCode) *errorCode = code;
        return std::nullopt;
    };
    const QSet<QString> allowed{QStringLiteral("kind"), QStringLiteral("presetId"),
        QStringLiteral("panelId"), QStringLiteral("newPanel"), QStringLiteral("useRecommendedIcons")};
    for (auto it = request.cbegin(); it != request.cend(); ++it)
        if (!allowed.contains(it.key())) return fail(QStringLiteral("invalid-preview-request"));
    const QString kind = request.value(QStringLiteral("kind")).toString();
    const QString presetId = request.value(QStringLiteral("presetId")).toString();
    auto snapshot = m_panelRegistry.panelDefinition(request.value(QStringLiteral("panelId")).toString());
    PresetPreviewSession::Prepared prepared;
    prepared.record.kind = kind;
    const auto object = presetResource(kind, presetId);
    if (object.isEmpty()) return fail(QStringLiteral("preset-not-found"));
    const QString scope = PresetIdentity::isUserId(presetId) ? QStringLiteral("user") : QStringLiteral("builtin");
    const auto cards = kind == QStringLiteral("panel") ? m_presetLibrary.panelPresets(scope)
                                                        : m_presetLibrary.iconPresets(scope);
    for (const auto &card : cards)
        if (card.toMap().value(QStringLiteral("id")).toString() == presetId)
            prepared.fallback = card.toMap().value(QStringLiteral("compatibility")).toMap();
    if (!prepared.fallback.value(QStringLiteral("available")).toBool())
        return fail(QStringLiteral("preset-unavailable"));
    if (kind == QStringLiteral("panel"))
    {
        prepared.panelPreset = PanelPresetDefinition::fromVariantMap(object);
        if (!prepared.panelPreset) return fail(QStringLiteral("invalid-panel-preset"));
        auto &preset = *prepared.panelPreset;
        const bool temporary = request.value(QStringLiteral("newPanel")).toBool() || !snapshot ||
            !preset.compatibility.hostKinds.contains(PanelDefinition::hostKindName(snapshot->host.kind)) ||
            (snapshot->host.kind == PanelHostKind::NativeEdge &&
                panelTypeNeedsDockApplet(snapshot->content.type) != panelTypeNeedsDockApplet(preset.panel.configuration.content.type));
        prepared.record.temporary = temporary;
        if (temporary)
        {
            const bool free = preset.panel.configuration.host.kind == PanelHostKind::FreeDesktop;
            const QString id = (free ? QStringLiteral("free-x") : QStringLiteral("panel-x")) +
                QUuid::createUuid().toString(QUuid::Id128).left(8);
            snapshot = PanelDefinition::defaults(id, preset.identity.name,
                free ? QStringLiteral("free") : preset.panel.configuration.placement.edge, false).normalized();
            snapshot->visibility.visible = true;
            snapshot->settingsRevision = 0;
            snapshot->host.screenIndex = qBound(0, m_settings.monitorIndex(), QGuiApplication::screens().size() - 1);
            if (QGuiApplication::screens().isEmpty()) return fail(QStringLiteral("screen-unavailable"));
            snapshot->host.screenId = persistentScreenId(QGuiApplication::screens().at(snapshot->host.screenIndex));
            prepared.record.previewToken = QStringLiteral("archdock-preview-") +
                QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        // A compatible free host keeps its host kind even when a reusable
        // preset's canonical example is an edge panel.
        if (snapshot->host.kind == PanelHostKind::FreeDesktop)
        {
            preset.panel.configuration.host.kind = PanelHostKind::FreeDesktop;
            preset.panel.configuration.placement.edge = QStringLiteral("free");
        }
        if (prepared.fallback.value(QStringLiteral("fallbackApplied")).toBool())
        {
            preset.panel.configuration.surface.completeThemeId =
                prepared.fallback.value(QStringLiteral("effectiveThemeId")).toString();
            preset.panel.configuration.surface.panelThemeId = preset.panel.configuration.surface.completeThemeId;
            preset.panel.configuration.surface.rendererTier =
                prepared.fallback.value(QStringLiteral("effectiveRendererTier")).toString();
        }
        const bool recommended = request.value(QStringLiteral("useRecommendedIcons"), temporary ||
            !snapshot->presetOrigin || snapshot->presetOrigin->iconPresetId.isEmpty()).toBool();
        if (recommended && !preset.panel.recommendedIconPresetId.isEmpty())
        {
            prepared.recommendedIcons = IconPresetDefinition::fromVariantMap(
                presetResource(QStringLiteral("icon"), preset.panel.recommendedIconPresetId));
            if (!prepared.recommendedIcons) return fail(QStringLiteral("recommended-icons-unavailable"));
        }
        auto draft = PresetApplication::preparePanel(*snapshot, m_settings.transactionSnapshot(),
            preset, {}, prepared.recommendedIcons, errorCode);
        if (!draft) return std::nullopt;
        prepared.draft = *draft;
    }
    else if (kind == QStringLiteral("icon"))
    {
        if (!snapshot || request.value(QStringLiteral("newPanel")).toBool())
            return fail(QStringLiteral("icon-preview-needs-panel"));
        prepared.iconPreset = IconPresetDefinition::fromVariantMap(object);
        if (!prepared.iconPreset) return fail(QStringLiteral("invalid-icon-preset"));
        if (prepared.fallback.value(QStringLiteral("fallbackApplied")).toBool())
        {
            auto &icon = prepared.iconPreset->icon;
            icon.iconStyleId = prepared.fallback.value(QStringLiteral("effectiveIconStyleId")).toString();
            icon.motion.profileId = prepared.fallback.value(QStringLiteral("effectiveMotionProfileId")).toString();
            icon.visualOverrides.clear(); icon.stateOverrides.clear();
        }
        auto draft = PresetApplication::prepareIcon(*snapshot, m_settings.transactionSnapshot(),
            *prepared.iconPreset, {}, errorCode);
        if (!draft) return std::nullopt;
        prepared.draft = *draft;
    }
    else return fail(QStringLiteral("invalid-preset-kind"));
    prepared.record.panelId = snapshot->identity.id;
    prepared.record.hostKind = PanelDefinition::hostKindName(snapshot->host.kind);
    prepared.record.snapshot = *snapshot;
    if (!prepared.record.temporary)
    {
        const bool free = snapshot->host.kind == PanelHostKind::FreeDesktop;
        prepared.record.previewToken = free ? snapshot->host.freeOwnershipToken : snapshot->host.nativeOwnershipToken;
        prepared.record.containmentId = free ? snapshot->host.freeDesktopContainmentId : snapshot->host.nativePanelId;
        prepared.record.appletId = free ? snapshot->host.freeDockAppletId : snapshot->host.nativeDockAppletId;
    }
    return prepared;
}

QVariantMap PanelWindow::presetEditorProjection(const ArchDock::PanelDefinition &candidate) const
{
    const auto resolution = m_panelRegistry.resolvePanelCapabilities(candidate);
    const auto fields = panelSettingsEditorFields(candidate, resolution, QStringLiteral("studio"));
    QString themeError, iconError;
    const auto theme = m_panelRegistry.themeRuntimeProjection(candidate, &themeError);
    const auto icons = m_panelRegistry.iconStyleRuntimeProjection(candidate, &iconError);
    return {{QStringLiteral("success"), true}, {QStringLiteral("status"), QStringLiteral("loaded")},
        {QStringLiteral("panelId"), candidate.identity.id}, {QStringLiteral("revision"), candidate.settingsRevision},
        {QStringLiteral("consumer"), QStringLiteral("studio")},
        {QStringLiteral("panelValues"), panelSettingsEditorValues(candidate, fields)},
        {QStringLiteral("panelFields"), fields}, {QStringLiteral("globalFields"), QVariantList{}},
        {QStringLiteral("globalValues"), m_settings.transactionSnapshot()},
        {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
        {QStringLiteral("themes"), m_panelRegistry.themeDefinitions()},
        {QStringLiteral("iconStyles"), m_panelRegistry.iconStyleDefinitions()},
        {QStringLiteral("animationProfiles"), m_panelRegistry.animationProfileDefinitions()},
        {QStringLiteral("themeDefinition"), theme.value_or(QVariantMap{})},
        {QStringLiteral("themeProjectionStatus"), theme ? QStringLiteral("ready") : QStringLiteral("unavailable")},
        {QStringLiteral("themeProjectionError"), themeError},
        {QStringLiteral("iconStyleDefinition"), icons.value_or(QVariantMap{})},
        {QStringLiteral("iconStyleProjectionStatus"), icons ? QStringLiteral("ready") : QStringLiteral("error")},
        {QStringLiteral("iconStyleProjectionError"), iconError}};
}

ArchDock::PresetPreviewSession::Operations PanelWindow::presetAuditionOperations()
{
    using namespace ArchDock;
    PresetPreviewSession::Operations operations;
    operations.prepare = [this](const QVariantMap &request, QString *error) { return preparePresetPreview(request, error); };
    operations.guard = [this](const QString &panelId) {
        QStringList targets{panelId};
        if (m_presetAudition && m_presetAudition->active())
            targets.append(m_presetAudition->status().value(QStringLiteral("panelId")).toString());
        for (const QString &target : targets)
        {
            const auto locks = m_panelInteractionGuards.value(target);
            if (locks.editMode) return QStringLiteral("edit-mode-active");
            if (locks.popupOpen) return QStringLiteral("popup-open");
            if (locks.dragActive) return QStringLiteral("drag-active");
        }
        return QString{};
    };
    operations.captureHost = [this](PresetPreviewRecord &record, QString *error) {
        if (!capturePresetPreviewHost(record, error)) return false;
        if (record.hostKind == QStringLiteral("native-edge") && record.appletId >= 0 &&
            record.hostState.value(QStringLiteral("panelType")).toString() != record.snapshot.content.type)
        {
            if (error) *error = QStringLiteral("preview-renderer-not-verified");
            return false;
        }
        return true;
    };
    operations.createHost = [this](PresetPreviewRecord &record, PanelDefinition &candidate, QString *error) {
        if (candidate.host.kind == PanelHostKind::FreeDesktop)
        {
            const auto created = createConfiguredFreePanelHost(candidate.host.screenIndex, record.panelId, record.previewToken);
            record.containmentId = created.host.desktopContainmentId; record.appletId = created.host.dockAppletId;
            if (created.outcome != FreePanelHostMutationOutcome::Verified)
            {
                if (error) *error = QStringLiteral("preview-free-host-creation-failed");
                return false;
            }
            candidate.host.freeDesktopContainmentId = record.containmentId;
            candidate.host.freeDockAppletId = record.appletId;
            candidate.host.freeOwnershipToken = record.previewToken;
            candidate.host.freeHostState = QStringLiteral("hosted-owned");
            candidate.host.freeCreationState = QStringLiteral("idle");
        }
        else
        {
            record.containmentId = createNativePanelCandidate(candidate, record.previewToken, error);
            if (record.containmentId < 0) return false;
            const auto dockId = verifiedNativeDockAppletId(record.panelId, record.containmentId, candidate.content.type);
            if (!dockId || !nativePanelIsOwned(record.panelId, record.containmentId, record.previewToken))
            {
                if (error) *error = QStringLiteral("preview-native-host-not-verified");
                return false;
            }
            record.appletId = *dockId;
            candidate.host.nativePanelId = record.containmentId; candidate.host.nativeDockAppletId = *dockId;
            candidate.host.nativeOwnershipToken = record.previewToken;
            candidate.host.nativeRecoveryState = QStringLiteral("ready");
        }
        candidate = candidate.normalized();
        return true;
    };
    operations.applyHost = [this](const PanelDefinition &previous, const PanelDefinition &candidate, QString *error) {
        const bool native = candidate.host.kind == PanelHostKind::NativeEdge;
        if (!(native ? nativePanelIsOwned(candidate.identity.id, candidate.host.nativePanelId, candidate.host.nativeOwnershipToken)
            : freePanelHostVerification(candidate.host.freeDesktopContainmentId, candidate.host.freeDockAppletId,
                candidate.identity.id, candidate.host.freeOwnershipToken) == FreePanelHostVerificationOutcome::Owned))
        {
            if (error) *error = QStringLiteral("preview-host-not-verified");
            return false;
        }
        if (native && previous.host.nativePanelId < 0)
        {
            const auto placement = normalizedNativePanelPlacement(candidate);
            const PlasmaPanelAdapter adapter([this](const QString &script) { return evaluatePlasmaScriptResultOptional(script); });
            if (!placement.isValid() || !placement.isSupported() || !placement.placement ||
                !adapter.applyPlacement(candidate.host.nativePanelId, candidate.identity.id,
                    candidate.host.nativeOwnershipToken, *placement.placement).success())
            {
                if (error) *error = QStringLiteral("preview-native-placement-failed");
                return false;
            }
        }
        const auto results = applyPanelSettingsHosts({previous, candidate, {}, {}}, false);
        if (!PanelSettingsTransaction::requiredHostsSucceeded(results))
        {
            for (const auto &result : results)
                if (result.required && !result.success && error) { *error = result.errorCode; break; }
            return false;
        }
        return true;
    };
    operations.restoreHost = [this](const PresetPreviewRecord &record, const PanelDefinition &, QString *error) {
        return restorePresetPreviewHost(record, error);
    };
    operations.commitExisting = [this](const PanelSettingsTransactionDraft &draft, QString *error) {
        PanelSettingsTransactionOutcome outcome;
        outcome.panelId = draft.previousPanel.identity.id;
        outcome.expectedRevision = outcome.previousRevision = outcome.revision = draft.previousPanel.settingsRevision;
        const auto result = commitPanelSettingsDraft(draft, outcome);
        if (!result.value(QStringLiteral("success")).toBool() && error)
            *error = result.value(QStringLiteral("errorCode")).toString();
        return result.value(QStringLiteral("success")).toBool();
    };
    operations.convertHost = [this](const PresetPreviewRecord &record, PanelDefinition &candidate, QString *error) {
        return convertPresetPreviewHost(record, candidate, error);
    };
    operations.adoptHost = [this](const PanelDefinition &candidate, QString *error) {
        const bool owned = candidate.host.kind == PanelHostKind::FreeDesktop
            ? freePanelHostVerification(candidate.host.freeDesktopContainmentId, candidate.host.freeDockAppletId,
                candidate.identity.id, candidate.host.freeOwnershipToken) == FreePanelHostVerificationOutcome::Owned
            : nativePanelIsOwned(candidate.identity.id, candidate.host.nativePanelId, candidate.host.nativeOwnershipToken);
        if (!owned) { if (error) *error = QStringLiteral("managed-host-not-verified"); return false; }
        return m_panelRegistry.adoptPreviewPanel(candidate, error);
    };
    operations.removeHost = [this](const PresetPreviewRecord &record, QString *error) { return removePresetPreviewHost(record, error); };
    operations.recover = [this](const PresetPreviewRecord &record, QString *error) { return recoverPresetPreviewHost(record, error); };
    operations.saveCustom = [this](const PresetPreviewSession::Prepared &prepared, const QString &name) {
        const UserPresetStore store(m_presetLibrary.userRoot());
        QString error, id;
        if (prepared.panelPreset)
        {
            const auto saved = store.save(PresetApplication::panelSnapshot(*prepared.panelPreset,
                prepared.draft.candidatePanel, name), &error);
            if (saved) id = saved->identity.id;
        }
        else if (prepared.iconPreset)
        {
            const auto saved = store.save(PresetApplication::iconSnapshot(*prepared.iconPreset,
                prepared.draft.candidatePanel, name), &error);
            if (saved) id = saved->identity.id;
        }
        return QVariantMap{{QStringLiteral("success"), !id.isEmpty()}, {QStringLiteral("errorCode"), error},
            {QStringLiteral("presetId"), id}};
    };
    operations.restoreBuiltIn = [this](const PresetPreviewSession::Prepared &current, QString *error)
        -> std::optional<PresetPreviewSession::Prepared> {
        const auto identity = current.panelPreset ? current.panelPreset->identity : current.iconPreset->identity;
        QString id = identity.builtIn ? identity.id : identity.derivedFromPresetId;
        QVariantMap resource;
        for (int depth = 0; depth < 16 && !id.isEmpty(); ++depth)
        {
            resource = presetResource(current.record.kind, id);
            const auto source = resource.value(QStringLiteral("identity")).toMap();
            if (source.value(QStringLiteral("builtIn")).toBool()) break;
            id = source.value(QStringLiteral("derivedFromPresetId")).toString();
            resource.clear();
        }
        if (resource.isEmpty()) { if (error) *error = QStringLiteral("no-built-in-source"); return std::nullopt; }
        const auto resolved = preparePresetPreview({{QStringLiteral("kind"), current.record.kind},
            {QStringLiteral("presetId"), id}, {QStringLiteral("panelId"), current.draft.previousPanel.identity.id},
            {QStringLiteral("useRecommendedIcons"), current.recommendedIcons.has_value()}}, error);
        if (!resolved) return std::nullopt;
        auto restored = current;
        restored.fallback = resolved->fallback;
        std::optional<PanelSettingsTransactionDraft> draft;
        if (current.panelPreset)
        {
            restored.panelPreset = resolved->panelPreset;
            if (!restored.panelPreset || !restored.panelPreset->compatibility.hostKinds.contains(
                PanelDefinition::hostKindName(current.draft.previousPanel.host.kind)))
            { if (error) *error = QStringLiteral("built-in-host-incompatible"); return std::nullopt; }
            if (current.draft.previousPanel.host.kind == PanelHostKind::FreeDesktop)
            {
                restored.panelPreset->panel.configuration.host.kind = PanelHostKind::FreeDesktop;
                restored.panelPreset->panel.configuration.placement.edge = QStringLiteral("free");
            }
            restored.recommendedIcons = resolved->recommendedIcons;
            draft = PresetApplication::preparePanel(current.draft.previousPanel, current.draft.previousGlobals,
                *restored.panelPreset, {}, restored.recommendedIcons, error);
        }
        else
        {
            restored.iconPreset = resolved->iconPreset;
            if (!restored.iconPreset) return std::nullopt;
            draft = PresetApplication::prepareIcon(current.draft.previousPanel, current.draft.previousGlobals,
                *restored.iconPreset, {}, error);
        }
        if (!draft) return std::nullopt;
        restored.draft = *draft;
        return restored;
    };
    operations.validDefault = [this](const QString &kind, const QString &id) {
        if (presetResource(kind, id).isEmpty()) return false;
        const QString scope = PresetIdentity::isUserId(id) ? QStringLiteral("user") : QStringLiteral("builtin");
        const auto cards = kind == QStringLiteral("panel") ? m_presetLibrary.panelPresets(scope) : m_presetLibrary.iconPresets(scope);
        for (const auto &card : cards)
            if (card.toMap().value(QStringLiteral("id")).toString() == id)
                return card.toMap().value(QStringLiteral("compatibility")).toMap().value(QStringLiteral("available")).toBool();
        return false;
    };
    operations.validateDraft = [this](PresetPreviewSession::Prepared &prepared, QString *error) {
        auto &candidate = prepared.draft.candidatePanel;
        const auto fail = [error](const QString &code) { if (error) *error = code; return false; };
        if (candidate.identity != prepared.draft.previousPanel.identity ||
            candidate.host.kind != prepared.draft.previousPanel.host.kind ||
            prepared.draft.candidateGlobals != prepared.draft.previousGlobals)
            return fail(QStringLiteral("preview-protected-state-change"));
        const auto screens = QGuiApplication::screens();
        if (candidate.host.screenIndex < 0 || candidate.host.screenIndex >= screens.size())
            return fail(QStringLiteral("screen-unavailable"));
        if (prepared.record.temporary || candidate.host.screenIndex != prepared.draft.previousPanel.host.screenIndex)
            candidate.host.screenId = persistentScreenId(screens.at(candidate.host.screenIndex));
        if (!candidate.isValid(error)) return false;
        if (prepared.record.kind == QStringLiteral("icon") && !PresetApplication::iconOnlyChange(prepared.draft.previousPanel, candidate))
            return fail(QStringLiteral("icon-only-violation"));
        if ((prepared.iconPreset && !prepared.iconPreset->icon.perStateAnimationOverrides.isEmpty()) ||
            (prepared.recommendedIcons && !prepared.recommendedIcons->icon.perStateAnimationOverrides.isEmpty()))
            return fail(QStringLiteral("per-state-icon-motion-unavailable"));
        if (!m_panelRegistry.resolvePanelCapabilities(candidate).available)
            return fail(QStringLiteral("preview-capability-unavailable"));
        QString projectionError;
        if (!m_panelRegistry.themeRuntimeProjection(candidate, &projectionError) && !projectionError.isEmpty())
            return fail(projectionError);
        const auto style = m_panelRegistry.iconStyleRuntimeProjection(candidate, &projectionError);
        if (!style || !projectionError.isEmpty() || (candidate.iconStyle.globalDefaults.contains(QStringLiteral("presetOverrides")) &&
            !style->value(QStringLiteral("presetOverridesApplied")).toBool())) return fail(QStringLiteral("preview-icons-unavailable"));
        if (!prepared.record.temporary)
        {
            const auto stored = m_panelRegistry.panelDefinition(prepared.record.panelId);
            if (!stored || *stored != prepared.draft.previousPanel) return fail(QStringLiteral("preview-revision-conflict"));
            QVariantMap changes;
            const auto before = stored->toLegacyMap();
            const auto after = candidate.toLegacyMap();
            for (auto it = after.cbegin(); it != after.cend(); ++it)
                if (PanelSettingsSchema::isTransactionPanelField(it.key()) && before.value(it.key()) != it.value())
                    changes.insert(it.key(), it.value());
            PanelSettingsTransactionOutcome outcome;
            const auto validated = preparePanelSettingsDraft(prepared.record.panelId, stored->settingsRevision,
                changes, {}, &outcome);
            if (!validated) return fail(outcome.errorCode);
            const auto origin = candidate.presetOrigin;
            const auto iconDefaults = candidate.iconStyle.globalDefaults;
            candidate = validated->candidatePanel;
            candidate.presetOrigin = origin;
            candidate.iconStyle.globalDefaults = iconDefaults;
        }
        else if (candidate.segments != QList<PanelSegmentDefinition>{PanelSegmentDefinition{}})
        {
            const auto capabilities = PanelCapabilityResolver::segmentCapabilities(candidate,
                m_panelRegistry.resolvePanelCapabilities(candidate), !m_systemStatus.availableSources().isEmpty());
            if (!capabilities.value(QStringLiteral("available")).toBool()) return fail(QStringLiteral("unavailable-segment-feature"));
            for (const auto &segment : candidate.segments)
                if (!capabilities.value(QStringLiteral("sources")).toStringList().contains(segment.source) ||
                    !capabilities.value(QStringLiteral("motionProfiles")).toStringList().contains(segment.motionProfile) ||
                    (segment.background != QStringLiteral("solid") && segment.corners != QStringLiteral("inherited")))
                    return fail(QStringLiteral("unavailable-segment-feature"));
            auto entries = candidate.host.kind == PanelHostKind::FreeDesktop ? freePanelEntries(candidate)
                : m_dockModel.panelEntries(candidate.content.type);
            entries.append(statusEntriesFor(candidate, true));
            QString segmentError;
            const auto projected = PanelContentTransaction::segmentEntries(candidate, entries, &segmentError, true);
            Q_UNUSED(projected);
            if (!segmentError.isEmpty()) return fail(QStringLiteral("invalid-segment-ownership"));
        }
        return true;
    };
    operations.editorProjection = [this](const PanelDefinition &candidate) { return presetEditorProjection(candidate); };
    return operations;
}

bool PanelWindow::capturePresetPreviewHost(ArchDock::PresetPreviewRecord &record,
    QString *errorCode) const
{
    const auto fail = [errorCode] { if (errorCode) *errorCode = QStringLiteral("preview-host-not-verified"); return false; };
    if (record.previewToken.isEmpty()) return fail();
    if (record.hostKind == QStringLiteral("free-desktop"))
    {
        if (freePanelHostVerification(record.containmentId, record.appletId, record.panelId,
            record.previewToken) != ArchDock::FreePanelHostVerificationOutcome::Owned) return fail();
        record.hostState = {{QStringLiteral("containmentId"), record.containmentId},
            {QStringLiteral("appletId"), record.appletId}, {QStringLiteral("ownerToken"), record.previewToken},
            {QStringLiteral("panelId"), record.panelId}, {QStringLiteral("verified"), true}};
        return true;
    }
    QDBusInterface shell(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"), QDBusConnection::sessionBus());
    if (!shell.isValid()) return fail();
    const QString script = QStringLiteral(R"JS(
var state = (function() {
    var panel = panelById(%1);
    if (!panel) return {};
    panel.currentConfigGroup = ['ArchDock'];
    if (String(panel.readConfig('panelId', '')) !== %2 ||
        String(panel.readConfig('ownerToken', '')) !== %3) return {};
    var state = {};
    var keys = ['location', 'screen', 'alignment', 'offset', 'height',
        'minimumLength', 'maximumLength', 'length', 'lengthMode'];
    for (var i = 0; i < keys.length; ++i) {
        if (typeof panel[keys[i]] === 'undefined') return {};
        state[keys[i]] = panel[keys[i]];
    }
    state.panelType = '';
    if (%4 >= 0) {
        var dock = panel.widgetById(%4);
        if (!dock || dock.type !== 'org.archdock.dock') return {};
        dock.currentConfigGroup = ['General'];
        if (String(dock.readConfig('panelId', '')) !== %2) return {};
        state.panelType = String(dock.readConfig('panelType', ''));
    }
    return state;
})();
print('ARCHDOCK_PREVIEW_HOST:' + JSON.stringify(state));
)JS").arg(record.containmentId).arg(plasmaScriptStringLiteral(record.panelId))
        .arg(plasmaScriptStringLiteral(record.previewToken)).arg(record.appletId);
    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
    const QString prefix = QStringLiteral("ARCHDOCK_PREVIEW_HOST:");
    if (!reply.isValid() || reply.value().size() > 8192 || !reply.value().trimmed().startsWith(prefix)) return fail();
    const auto document = QJsonDocument::fromJson(reply.value().trimmed().mid(prefix.size()).toUtf8());
    if (!document.isObject()) return fail();
    const auto state = document.object().toVariantMap();
    if (state.size() != 10 || !QStringList{QStringLiteral("top"), QStringLiteral("bottom"),
        QStringLiteral("left"), QStringLiteral("right")}.contains(state.value(QStringLiteral("location")).toString()) ||
        !QStringList{QStringLiteral("left"), QStringLiteral("center"), QStringLiteral("right")}.contains(
            state.value(QStringLiteral("alignment")).toString()) ||
        !QStringList{QStringLiteral("fit"), QStringLiteral("custom"), QStringLiteral("fill")}.contains(
            state.value(QStringLiteral("lengthMode")).toString())) return fail();
    for (const QString &key : {QStringLiteral("screen"), QStringLiteral("offset"), QStringLiteral("height"),
        QStringLiteral("minimumLength"), QStringLiteral("maximumLength"), QStringLiteral("length")})
        if (!ArchDock::PresetParsing::isInteger(state.value(key)) || state.value(key).toInt() < 0) return fail();
    if (record.appletId >= 0 && !isNativeDockPanelType(state.value(QStringLiteral("panelType")).toString())) return fail();
    record.hostState = state;
    return true;
}

bool PanelWindow::restorePresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
    QString *errorCode) const
{
    const auto fail = [errorCode](const QString &code) { if (errorCode) *errorCode = code; return false; };
    const auto stored = m_panelRegistry.panelDefinition(record.panelId);
    if (!stored) return fail(QStringLiteral("preview-panel-missing"));
    auto comparable = *stored;
    comparable.settingsRevision = record.snapshot.settingsRevision;
    if (comparable != record.snapshot) return fail(QStringLiteral("preview-revision-conflict"));
    if (record.hostKind == QStringLiteral("free-desktop"))
        return freePanelHostVerification(record.containmentId, record.appletId, record.panelId,
            record.previewToken) == ArchDock::FreePanelHostVerificationOutcome::Owned || fail(QStringLiteral("preview-host-not-verified"));
    // Revalidate the journal's bounded property shape before generating any
    // mutation. Property names and write order are fixed in this function.
    ArchDock::PresetPreviewRecord current = record;
    if (!capturePresetPreviewHost(current, errorCode)) return false;
    if (record.hostState.size() != current.hostState.size() || record.hostState.keys() != current.hostState.keys())
        return fail(QStringLiteral("invalid-preview-host-snapshot"));
    for (auto it = record.hostState.cbegin(); it != record.hostState.cend(); ++it)
        if (it.value().metaType() != current.hostState.value(it.key()).metaType())
            return fail(QStringLiteral("invalid-preview-host-snapshot"));
    if (current.hostState == record.hostState) return true;
    const QString snapshotJson = QString::fromUtf8(QJsonDocument::fromVariant(record.hostState).toJson(QJsonDocument::Compact));
    const int restored = evaluatePlasmaScriptResult(QStringLiteral(R"JS(
var result = (function() {
    try {
        var panel = panelById(%1);
        if (!panel) return 0;
        panel.currentConfigGroup = ['ArchDock'];
        if (String(panel.readConfig('panelId', '')) !== %2 ||
            String(panel.readConfig('ownerToken', '')) !== %3) return 0;
        var state = JSON.parse(%4);
        var keys = ['location', 'screen', 'alignment', 'offset', 'height',
            'maximumLength', 'minimumLength', 'length', 'lengthMode'];
        for (var i = 0; i < keys.length; ++i)
            if (panel[keys[i]] !== state[keys[i]]) panel[keys[i]] = state[keys[i]];
        if (%5 >= 0) {
            var dock = panel.widgetById(%5);
            if (!dock || dock.type !== 'org.archdock.dock') return 0;
            dock.currentConfigGroup = ['General'];
            if (String(dock.readConfig('panelId', '')) !== %2) return 0;
            if (String(dock.readConfig('panelType', '')) !== state.panelType) {
                dock.writeConfig('panelType', state.panelType); dock.reloadConfig();
            }
        }
        return 1;
    } catch (error) { return -1; }
})(); print('ARCHDOCK_RESULT:' + String(result));
)JS").arg(record.containmentId).arg(plasmaScriptStringLiteral(record.panelId))
        .arg(plasmaScriptStringLiteral(record.previewToken)).arg(plasmaScriptStringLiteral(snapshotJson)).arg(record.appletId));
    if (restored != 1 || !capturePresetPreviewHost(current, errorCode) || current.hostState != record.hostState)
        return fail(QStringLiteral("preview-host-rollback-readback-failed"));
    return true;
}

bool PanelWindow::removePresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
    QString *errorCode) const
{
    const auto fail = [errorCode] { if (errorCode) *errorCode = QStringLiteral("preview-host-cleanup-blocked"); return false; };
    if (!record.temporary || m_panelRegistry.panelDefinition(record.panelId)) return fail();
    for (const QString &token : {record.previewToken, record.managedToken})
    {
        if (token.isEmpty()) continue;
        if (record.hostKind == QStringLiteral("free-desktop"))
        {
            const auto outcome = removeOwnedFreePanelHostByIdentity(record.panelId, token);
            if (outcome != ArchDock::FreePanelRemovalOutcome::Removed &&
                outcome != ArchDock::FreePanelRemovalOutcome::AlreadyAbsent) return fail();
        }
        else
        {
            const auto found = discoverNativePanel(record.panelId, token, record.snapshot.content.type);
            if (found.status == NativePanelDiscoveryStatus::Missing) continue;
            // Renderer mismatch does not change the already-verified unique
            // host identity. A partial preview can still be safely removed.
            if ((found.status != NativePanelDiscoveryStatus::Unique &&
                 found.status != NativePanelDiscoveryStatus::RendererConflict) ||
                !rollbackNativePanelCandidate(record.panelId, found.containmentId, token)) return fail();
            if (discoverNativePanel(record.panelId, token, record.snapshot.content.type).status !=
                NativePanelDiscoveryStatus::Missing) return fail();
        }
    }
    return true;
}

bool PanelWindow::recoverPresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
    QString *errorCode)
{
    const auto fail = [errorCode] { if (errorCode) *errorCode = QStringLiteral("preview-recovery-blocked"); return false; };
    const auto stored = m_panelRegistry.panelDefinition(record.panelId);
    if (record.temporary)
    {
        if (!stored) return removePresetPreviewHost(record, errorCode);
        if (record.managedToken.isEmpty() || stored->identity.builtIn || stored->settingsRevision < 1) return fail();
        // Adoption may have committed before the journal's COMMITTED write.
        // Preserve it only when both the durable association and host agree.
        if (stored->host.kind == ArchDock::PanelHostKind::FreeDesktop)
            return stored->host.freeOwnershipToken == record.managedToken &&
                stored->host.freeDesktopContainmentId == record.containmentId && stored->host.freeDockAppletId == record.appletId &&
                freePanelHostVerification(record.containmentId, record.appletId, record.panelId, record.managedToken) ==
                    ArchDock::FreePanelHostVerificationOutcome::Owned ? true : fail();
        return stored->host.nativeOwnershipToken == record.managedToken && stored->host.nativePanelId == record.containmentId &&
            stored->host.nativeDockAppletId == record.appletId && nativePanelIsOwned(record.panelId, record.containmentId,
                record.managedToken) ? true : fail();
    }
    if (!stored) return fail();
    if (*stored == record.snapshot)
    {
        auto verified = record;
        return record.kind == QStringLiteral("icon")
            ? capturePresetPreviewHost(verified, errorCode)
            : restorePresetPreviewHost(record, errorCode);
    }
    if (stored->host != record.snapshot.host) return fail();
    // A committed revision or another valid settings transaction is durable
    // truth. Reapply that definition instead of an older snapshot.
    if (stored->host.kind == ArchDock::PanelHostKind::FreeDesktop)
        return freePanelHostVerification(record.containmentId, record.appletId, record.panelId,
            record.previewToken) == ArchDock::FreePanelHostVerificationOutcome::Owned ? true : fail();
    const ArchDock::PlasmaPanelAdapter adapter([this](const QString &script) { return evaluatePlasmaScriptResultOptional(script); });
    const auto placement = normalizedNativePanelPlacement(*stored);
    if (!placement.isValid() || !placement.isSupported() || !placement.placement ||
        !adapter.applyPlacement(record.containmentId, record.panelId, record.previewToken, *placement.placement).success()) return fail();
    if (record.appletId >= 0 && evaluatePlasmaScriptResult(QStringLiteral(
        "var result = (function() { var panel = panelById(%1); if (!panel) return 0; "
        "panel.currentConfigGroup = ['ArchDock']; if (String(panel.readConfig('ownerToken', '')) !== %2 || "
        "String(panel.readConfig('panelId', '')) !== %3) return 0; var dock = panel.widgetById(%4); "
        "if (!dock || dock.type !== 'org.archdock.dock') return 0; dock.currentConfigGroup = ['General']; "
        "if (String(dock.readConfig('panelId', '')) !== %3) return 0; dock.writeConfig('panelType', %5); "
        "dock.reloadConfig(); return String(dock.readConfig('panelType', '')) === %5 ? 1 : 0; })(); "
        "print('ARCHDOCK_RESULT:' + String(result));").arg(record.containmentId).arg(plasmaScriptStringLiteral(record.previewToken))
        .arg(plasmaScriptStringLiteral(record.panelId)).arg(record.appletId).arg(plasmaScriptStringLiteral(stored->content.type))) != 1) return fail();
    return true;
}

bool PanelWindow::convertPresetPreviewHost(const ArchDock::PresetPreviewRecord &record,
    ArchDock::PanelDefinition &candidate, QString *errorCode) const
{
    if (!record.temporary || record.managedToken.isEmpty() ||
        !record.previewToken.startsWith(QLatin1String("archdock-preview-")))
    {
        if (errorCode) *errorCode = QStringLiteral("invalid-preview-conversion");
        return false;
    }
    const bool free = candidate.host.kind == ArchDock::PanelHostKind::FreeDesktop;
    const QString host = free
        ? QStringLiteral("var desktop = desktopById(%1); var host = desktop ? desktop.widgetById(%2) : null;")
            .arg(candidate.host.freeDesktopContainmentId).arg(candidate.host.freeDockAppletId)
        : QStringLiteral("var host = panelById(%1);").arg(candidate.host.nativePanelId);
    const QString extra = free
        ? QStringLiteral("if (host.type !== 'org.archdock.dock' || String(host.readConfig('panelType', '')) !== 'empty' || "
            "!['false', '0'].includes(String(host.readConfig('bootstrapFreeDock', true)).toLowerCase())) return 0;")
        : QStringLiteral("var dock = host.widgetById(%1); if (!dock || dock.type !== 'org.archdock.dock') return 0; "
            "dock.currentConfigGroup = ['General']; if (String(dock.readConfig('panelId', '')) !== %2 || "
            "String(dock.readConfig('panelType', '')) !== %3) return 0;")
            .arg(candidate.host.nativeDockAppletId).arg(plasmaScriptStringLiteral(record.panelId))
            .arg(plasmaScriptStringLiteral(candidate.content.type));
    const QString group = free ? QStringLiteral("General") : QStringLiteral("ArchDock");
    const int converted = evaluatePlasmaScriptResult(QStringLiteral(
        "var result = (function() { try { %1 if (!host) return 0; "
        "host.currentConfigGroup = [%2]; "
        "if (String(host.readConfig('panelId', '')) !== %3 || "
        "String(host.readConfig('ownerToken', '')) !== %4) return 0; %5 "
        "host.currentConfigGroup = [%2]; host.writeConfig('ownerToken', %6); host.reloadConfig(); "
        "if (String(host.readConfig('ownerToken', '')) === %6 && "
        "String(host.readConfig('panelId', '')) === %3) return 1; "
        "host.writeConfig('ownerToken', %4); host.reloadConfig(); return 0; "
        "} catch (error) { return -1; } })(); print('ARCHDOCK_RESULT:' + String(result));")
        .arg(host, plasmaScriptStringLiteral(group), plasmaScriptStringLiteral(record.panelId),
            plasmaScriptStringLiteral(record.previewToken), extra, plasmaScriptStringLiteral(record.managedToken)));
    const bool verified = converted == 1 && (free
        ? freePanelHostVerification(candidate.host.freeDesktopContainmentId,
            candidate.host.freeDockAppletId, record.panelId, record.managedToken) ==
            ArchDock::FreePanelHostVerificationOutcome::Owned
        : nativePanelIsOwned(record.panelId, candidate.host.nativePanelId, record.managedToken));
    if (!verified)
    {
        if (errorCode) *errorCode = QStringLiteral("preview-token-conversion-failed");
        return false;
    }
    if (free) candidate.host.freeOwnershipToken = record.managedToken;
    else candidate.host.nativeOwnershipToken = record.managedToken;
    candidate.settingsRevision = 1;
    candidate.identity.builtIn = false;
    candidate = candidate.normalized();
    return true;
}

QVariantMap PanelWindow::panelRendererConfiguration(const QString &panelId) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        runtimePanelDefinition(panelId);
    if (!definition.has_value())
    {
        return {};
    }

    QVariantMap configuration = ArchDock::PanelSettingsSchema::runtimeValues(
        ArchDock::PanelSettingsFieldScope::Global,
        m_settings.transactionSnapshot());
    const QVariantMap panelValues = ArchDock::PanelSettingsSchema::runtimeValues(
        ArchDock::PanelSettingsFieldScope::Panel,
        definition->toLegacyMap());
    for (auto it = panelValues.cbegin(); it != panelValues.cend(); ++it)
    {
        configuration.insert(it.key(), it.value());
    }

    const QVariantMap capabilityResolution =
        m_panelRegistry.resolvePanelCapabilities(*definition).toVariantMap();
    configuration.insert(QStringLiteral("segmentCapabilities"),
        ArchDock::PanelCapabilityResolver::segmentCapabilities(*definition,
            m_panelRegistry.resolvePanelCapabilities(*definition), !m_systemStatus.availableSources().isEmpty()));
    configuration.insert(
        QStringLiteral("capabilityResolution"), capabilityResolution);
    configuration.insert(
        QStringLiteral("effectiveRendererTier"),
        capabilityResolution.value(QStringLiteral("renderer"))
            .toMap()
            .value(QStringLiteral("effectiveTier")));
    QString themeProjectionError;
    const std::optional<QVariantMap> themeProjection =
        m_panelRegistry.themeRuntimeProjection(
            *definition, &themeProjectionError);
    configuration.insert(
        QStringLiteral("themeDefinition"),
        themeProjection.value_or(QVariantMap{}));
    configuration.insert(
        QStringLiteral("themeProjectionStatus"),
        themeProjection.has_value()
            ? QStringLiteral("ready")
            : themeProjectionError.isEmpty()
                ? QStringLiteral("unavailable")
                : QStringLiteral("error"));
    configuration.insert(
        QStringLiteral("themeProjectionError"), themeProjectionError);
    QString iconStyleProjectionError;
    const std::optional<QVariantMap> iconStyleProjection =
        m_panelRegistry.iconStyleRuntimeProjection(
            *definition, &iconStyleProjectionError);
    configuration.insert(
        QStringLiteral("iconStyleDefinition"),
        iconStyleProjection.value_or(QVariantMap{}));
    configuration.insert(
        QStringLiteral("iconStyleProjectionStatus"),
        iconStyleProjection.has_value()
            ? QStringLiteral("ready")
            : QStringLiteral("error"));
    configuration.insert(
        QStringLiteral("iconStyleProjectionError"), iconStyleProjectionError);
    const QVariantMap animationResolution =
        m_panelRegistry.animationProfileResolution(definition->motion.iconProfile);
    configuration.insert(
        QStringLiteral("animationProfile"),
        animationResolution.value(QStringLiteral("profile")).toMap());
    configuration.insert(
        QStringLiteral("animationProfileFallbackApplied"),
        animationResolution.value(QStringLiteral("fallbackApplied")));
    configuration.insert(
        QStringLiteral("animationProfiles"),
        m_panelRegistry.animationProfileDefinitions());

    // A stable description of how this panel presents itself, published as one
    // record so a later Panel Preset can capture and restore it without having
    // to know which individual settings keys made it up. The id is derived
    // from the resolved values, so two panels that present identically carry
    // the same id and a preset can compare them.
    QStringList availableMechanisms;
    for (const QVariant &value :
         capabilityResolution.value(QStringLiteral("presentationMechanisms")).toList())
    {
        const QVariantMap decision = value.toMap();
        if (decision.value(QStringLiteral("available")).toBool())
        {
            availableMechanisms.append(
                decision.value(QStringLiteral("id")).toString());
        }
    }
    const QString profileId = QStringLiteral("%1:%2:%3:%4:%5")
        .arg(definition->presentation.mode,
             definition->presentation.collapseMechanism,
             definition->presentation.collapseAxis,
             definition->presentation.trigger,
             definition->presentation.revealHandle);
    configuration.insert(
        QStringLiteral("presentationProfile"),
        QVariantMap{
            {QStringLiteral("id"), profileId},
            {QStringLiteral("restingState"), definition->presentation.mode},
            {QStringLiteral("trigger"), definition->presentation.trigger},
            {QStringLiteral("mechanism"),
             definition->presentation.collapseMechanism},
            {QStringLiteral("axis"), definition->presentation.collapseAxis},
            {QStringLiteral("revealHandle"),
             definition->presentation.revealHandle},
            {QStringLiteral("openDelay"), definition->visibility.openDelay},
            {QStringLiteral("closeDelay"), definition->visibility.closeDelay},
            {QStringLiteral("availableMechanisms"), availableMechanisms},
        });
    return configuration;
}

std::optional<ArchDock::PanelDefinition>
PanelWindow::capabilityCandidateDefinition(
    const QString &panelId,
    const QVariantMap &candidateValues) const
{
    QString definitionError;
    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId, &definitionError);
    if (!current.has_value())
    {
        return std::nullopt;
    }

    for (auto iterator = candidateValues.cbegin();
         iterator != candidateValues.cend();
         ++iterator)
    {
        if (!ArchDock::PanelSettingsSchema::isTransactionPanelField(
                iterator.key()))
        {
            return std::nullopt;
        }
    }

    QVariantMap candidateRecord = current->toLegacyMap();
    for (auto iterator = candidateValues.cbegin();
         iterator != candidateValues.cend();
         ++iterator)
    {
        candidateRecord.insert(iterator.key(), iterator.key() == QStringLiteral("segments")
            ? decodedSegmentValue(iterator.value()) : iterator.value());
    }
    if (candidateValues.contains(QStringLiteral("screen")))
    {
        const int requestedScreen = candidateValues.value(
            QStringLiteral("screen")).toInt();
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (requestedScreen < 0 || requestedScreen >= screens.size())
        {
            return std::nullopt;
        }
        candidateRecord.insert(
            QStringLiteral("screenId"),
            ArchDock::persistentScreenId(screens.at(requestedScreen)));
    }
    candidateRecord.insert(
        QStringLiteral("settingsRevision"),
        QString::number(current->settingsRevision));
    const std::optional<ArchDock::PanelDefinition> candidate =
        ArchDock::PanelDefinition::fromLegacyMap(
            candidateRecord, &definitionError);
    return candidate;
}

QVariantMap PanelWindow::resolvePanelCapabilities(
    const QString &panelId,
    const QVariantMap &candidateValues) const
{
    const std::optional<ArchDock::PanelDefinition> candidate =
        capabilityCandidateDefinition(panelId, candidateValues);
    if (!candidate.has_value())
    {
        ArchDock::CapabilityResolution unavailable;
        unavailable.reason = ArchDock::CapabilityReasonCode::InvalidCapabilityInput;
        return unavailable.toVariantMap();
    }

    return m_panelRegistry.resolvePanelCapabilities(*candidate).toVariantMap();
}

QVariantList PanelWindow::resolvedThemeDefinitions(
    const QString &panelId,
    const QVariantMap &candidateValues) const
{
    const std::optional<ArchDock::PanelDefinition> candidate =
        capabilityCandidateDefinition(panelId, candidateValues);
    if (!candidate.has_value())
    {
        return {};
    }

    QVariantList result;
    const QVariantList themes = m_panelRegistry.themeDefinitions();
    result.reserve(themes.size());
    for (const QVariant &value : themes)
    {
        QVariantMap theme = value.toMap();
        if (theme.contains(QStringLiteral("packageManifest")))
        {
            QString projectionError;
            const std::optional<QVariantMap> projection =
                m_panelRegistry.builtInThemeRuntimeProjection(
                    theme.value(QStringLiteral("id")).toString(),
                    &projectionError);
            if (!projection.has_value())
            {
                theme.insert(QStringLiteral("available"), false);
                theme.insert(
                    QStringLiteral("reasonCode"),
                    projectionError.isEmpty()
                        ? QStringLiteral("theme-package-unavailable")
                        : projectionError);
                theme.insert(
                    QStringLiteral("themeProjectionStatus"),
                    QStringLiteral("error"));
                theme.insert(
                    QStringLiteral("themeProjectionError"), projectionError);
                result.append(theme);
                continue;
            }
            for (auto iterator = projection->cbegin();
                 iterator != projection->cend(); ++iterator)
            {
                theme.insert(iterator.key(), iterator.value());
            }
            theme.insert(
                QStringLiteral("themeProjectionStatus"),
                QStringLiteral("ready"));
            theme.insert(
                QStringLiteral("themeProjectionError"), QString{});
        }
        const std::optional<ArchDock::ThemeCapabilityProfile> profile =
            ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(theme);
        if (!profile.has_value())
        {
            continue;
        }

        ArchDock::PanelDefinition themedCandidate = *candidate;
        const QVariantMap layoutStyle = theme.value(
            QStringLiteral("layoutStyle")).toMap();
        if (layoutStyle.contains(QStringLiteral("layout")))
        {
            themedCandidate.layout.pathType =
                ArchDock::PanelDefinition::normalizeLegacyValue(
                    QStringLiteral("layout"),
                    layoutStyle.value(QStringLiteral("layout"))).toString();
        }
        const ArchDock::CapabilityResolution resolution =
            ArchDock::PanelCapabilityResolver::resolve(
                themedCandidate,
                ArchDock::PanelCapabilityResolver::productionHostProfile(
                    themedCandidate.host.kind),
                *profile,
                ArchDock::PanelCapabilityResolver::productionRenderers(),
                ArchDock::PanelCapabilityResolver::productionPlatform());
        theme.insert(QStringLiteral("available"), resolution.available);
        theme.insert(
            QStringLiteral("reasonCode"),
            ArchDock::capabilityReasonCodeName(resolution.reason));
        theme.insert(
            QStringLiteral("capabilityResolution"),
            resolution.toVariantMap());
        result.append(theme);
    }
    return result;
}

QVariantList PanelWindow::iconStyleDefinitions() const
{
    return m_panelRegistry.iconStyleDefinitions();
}

QVariantList PanelWindow::panelSettingsEditorFields(
    const ArchDock::PanelDefinition &candidate,
    const ArchDock::CapabilityResolution &resolution,
    const QString &consumer) const
{
    const QString normalizedConsumer = consumer.trimmed().toLower() ==
            QStringLiteral("studio")
        ? QStringLiteral("studio")
        : QStringLiteral("native");
    const bool freeHost = candidate.host.kind == ArchDock::PanelHostKind::FreeDesktop;
    const auto controlAvailable = [&resolution](const QString &id)
    {
        for (const ArchDock::CapabilityDecision &decision : resolution.controls)
        {
            if (decision.id == id)
            {
                return decision.available;
            }
        }
        return false;
    };
    const auto layoutAvailable = [&resolution](const QString &id)
    {
        for (const ArchDock::CapabilityDecision &decision : resolution.layouts)
        {
            if (decision.id == id)
            {
                return decision.available;
            }
        }
        return false;
    };

    QVariantList result;
    const QVariantList schemaFields = ArchDock::PanelSettingsSchema::editorDescriptors(
        ArchDock::PanelSettingsFieldScope::Panel, normalizedConsumer);
    for (const QVariant &value : schemaFields)
    {
        QVariantMap field = value.toMap();
        const QString key = field.value(QStringLiteral("key")).toString();
        const ArchDock::PanelSettingsFieldDescriptor *descriptor =
            ArchDock::PanelSettingsSchema::panelDescriptor(key);
        if (!descriptor)
        {
            continue;
        }
        if (!descriptor->editor.layouts.isEmpty() &&
            !descriptor->editor.layouts.contains(candidate.layout.pathType))
        {
            continue;
        }
        if ((key == QStringLiteral("edge") ||
             key == QStringLiteral("visibilityMode")) && freeHost)
        {
            continue;
        }

        bool available = true;
        const QString capability = descriptor->editor.capability;
        if (capability == QStringLiteral("whole-panel-rotation"))
        {
            available = resolution.rotation.available;
            // Only the static layout angle takes the resolved degree range;
            // the rotation mode, speed and trigger fields share the gate but
            // keep their own schema bounds.
            if (available && key == QStringLiteral("layoutAngle"))
            {
                field.insert(QStringLiteral("minimumValue"),
                             resolution.rotation.minimumDegrees);
                field.insert(QStringLiteral("maximumValue"),
                             resolution.rotation.maximumDegrees);
            }
        }
        else if (capability == QStringLiteral("presentation-mechanism"))
        {
            // Only mechanisms this panel's host and theme both declare may be
            // offered. The whole presentation group disappears when the panel
            // has no way to collapse at all, rather than presenting a resting
            // state or a reveal handle that nothing could ever draw.
            QStringList mechanisms;
            for (const ArchDock::CapabilityDecision &decision :
                 resolution.presentationMechanisms)
            {
                if (decision.available)
                {
                    mechanisms.append(decision.id);
                }
            }
            const bool collapsible = std::any_of(
                mechanisms.cbegin(),
                mechanisms.cend(),
                [](const QString &mechanism)
                {
                    return mechanism != QStringLiteral("open");
                });
            available = resolution.available && collapsible;
            if (available && key == QStringLiteral("collapseMechanism"))
            {
                field.insert(QStringLiteral("choices"), mechanisms);
            }
        }
        else if (capability == QStringLiteral("layout"))
        {
            available = key == QStringLiteral("layout")
                ? true
                : layoutAvailable(candidate.layout.pathType);
        }
        else if (capability == QStringLiteral("procedural-surface"))
        {
            available = resolution.available &&
                resolution.renderer.effectiveTier.has_value() &&
                *resolution.renderer.effectiveTier ==
                    ArchDock::RendererTier::Procedural2D;
        }
        else if (capability == QStringLiteral("segments"))
        {
            const auto capabilities = ArchDock::PanelCapabilityResolver::segmentCapabilities(
                candidate, resolution, !m_systemStatus.availableSources().isEmpty());
            available = capabilities.value(QStringLiteral("available")).toBool();
            field.insert(QStringLiteral("segmentCapabilities"), capabilities);
            auto entries = candidate.host.kind == ArchDock::PanelHostKind::FreeDesktop
                ? freePanelEntries(candidate) : m_dockModel.panelEntries(candidate.content.type);
            entries.append(statusEntriesFor(candidate, true));
            for (auto &value : entries) {
                auto entry = value.toMap();
                if (!entry.value(QStringLiteral("isStatus")).toBool()) {
                    const auto overlay = entryOverlay(entry);
                    for (auto it = overlay.cbegin(); it != overlay.cend(); ++it) entry.insert(it.key(), it.value());
                    if (!candidate.content.showBadges) entry[QStringLiteral("badgeText")] = QString{};
                    if (!candidate.content.showProgress) entry[QStringLiteral("progress")] = -1.0;
                    if (!candidate.content.showTemporaryStatus) entry[QStringLiteral("temporaryStatus")] = QString{};
                }
                value = entry;
            }
            field.insert(QStringLiteral("availableEntries"), entries);
            field.insert(QStringLiteral("segmentEntries"),
                ArchDock::PanelContentTransaction::segmentEntries(candidate, entries));
        }
        else if (capability == QStringLiteral("artwork-fit"))
        {
            available = resolution.available &&
                !candidate.surface.themeSource.trimmed().isEmpty();
        }
        else if (capability == QStringLiteral("application-overlays"))
        {
            available = m_overlayModel.available();
        }
        else if (capability == QStringLiteral("scene3d-quality"))
        {
            const auto renderer = std::find_if(resolution.rendererChoices.cbegin(),
                resolution.rendererChoices.cend(), [](const auto &choice) {
                    return choice.tier == ArchDock::RendererTier::True3D && choice.available;
                });
            available = freeHost && resolution.available
                && resolution.renderer.effectiveTier == ArchDock::RendererTier::True3D
                && renderer != resolution.rendererChoices.cend();
            if (available)
            {
                const auto theme = m_panelRegistry.themeRuntimeProjection(candidate);
                available = theme && theme->value(QStringLiteral("valid")).toBool()
                    && !theme->value(QStringLiteral("scene3D")).toMap().isEmpty()
                    && !theme->value(QStringLiteral("scene3DResources")).toMap().isEmpty();
            }
        }
        else if (const std::optional<ArchDock::PanelCapability> parsed =
                     ArchDock::panelCapabilityFromName(capability);
                 parsed.has_value())
        {
            available = controlAvailable(capability);
            if (freeHost && (capability == QStringLiteral("length-mutation") ||
                             capability == QStringLiteral("thickness-mutation")))
            {
                available = true;
            }
        }
        if (!available)
        {
            continue;
        }

        QStringList choices = field.value(QStringLiteral("choices")).toStringList();
        if (key == QStringLiteral("folderLayout"))
        {
            choices = {QStringLiteral("fan"), QStringLiteral("grid"),
                       QStringLiteral("stack"), QStringLiteral("arc"), QStringLiteral("ring")};
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("iconStyle"))
        {
            choices.clear();
            for (const QVariant &styleValue : m_panelRegistry.iconStyleDefinitions())
            {
                const QString styleId = styleValue.toMap()
                    .value(QStringLiteral("id")).toString();
                if (!styleId.isEmpty())
                {
                    choices.append(styleId);
                }
            }
            if (choices.isEmpty())
            {
                continue;
            }
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("layout"))
        {
            QStringList availableChoices;
            for (const QString &choice : choices)
            {
                if (layoutAvailable(choice))
                {
                    availableChoices.append(choice);
                }
            }
            choices = availableChoices;
            if (choices.isEmpty())
            {
                continue;
            }
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("edge"))
        {
            choices = {
                QStringLiteral("top"),
                QStringLiteral("bottom"),
                QStringLiteral("left"),
                QStringLiteral("right"),
            };
            field.insert(QStringLiteral("choices"), choices);
        }
        else if (key == QStringLiteral("visibilityMode") && !freeHost)
        {
            const QStringList supportedModes =
                ArchDock::supportedNativeVisibilityModes(
                    nativeVisibilityCapabilities(candidate.identity.id));
            QStringList supportedChoices;
            for (const QString &choice : choices)
            {
                if (supportedModes.contains(choice))
                {
                    supportedChoices.append(choice);
                }
            }
            choices = supportedChoices;
            field.insert(QStringLiteral("choices"), choices);
        }

        QVariantList options;
        if (key == QStringLiteral("iconStyle"))
        {
            for (const QVariant &styleValue : m_panelRegistry.iconStyleDefinitions())
            {
                const QVariantMap style = styleValue.toMap();
                options.append(QVariantMap{
                    {QStringLiteral("description"),
                     style.value(QStringLiteral("description"))},
                    {QStringLiteral("label"), style.value(QStringLiteral("name"))},
                    {QStringLiteral("value"), style.value(QStringLiteral("id"))},
                });
            }
        }
        else for (const QString &choice : choices)
        {
            options.append(QVariantMap{
                {QStringLiteral("label"), choice},
                {QStringLiteral("value"), choice},
            });
        }
        if (key == QStringLiteral("screen"))
        {
            options.clear();
            const QVariantList screens = availableScreens();
            for (const QVariant &screenValue : screens)
            {
                const QVariantMap screen = screenValue.toMap();
                options.append(QVariantMap{
                    {QStringLiteral("label"), screen.value(QStringLiteral("label"))},
                    {QStringLiteral("value"), screen.value(QStringLiteral("index"))},
                });
            }
            field.insert(QStringLiteral("minimumValue"), 0);
            field.insert(QStringLiteral("maximumValue"), qMax(0, options.size() - 1));
        }
        if (!options.isEmpty())
        {
            field.insert(QStringLiteral("options"), options);
        }
        result.append(field);
    }
    return result;
}

QVariantMap PanelWindow::panelSettingsEditorValues(
    const ArchDock::PanelDefinition &candidate,
    const QVariantList &fields) const
{
    QSet<QString> includedKeys;
    for (const QVariant &value : fields)
    {
        includedKeys.insert(value.toMap().value(QStringLiteral("key")).toString());
    }
    for (const ArchDock::PanelSettingsFieldDescriptor &field :
         ArchDock::PanelSettingsSchema::fields())
    {
        if (field.scope == ArchDock::PanelSettingsFieldScope::Panel &&
            field.access == ArchDock::PanelSettingsFieldAccess::Editor &&
            !field.editor.isPresented())
        {
            includedKeys.insert(field.key);
        }
    }

    const QVariantMap record = candidate.toLegacyMap();
    QVariantMap result;
    for (const QString &key : std::as_const(includedKeys))
    {
        const ArchDock::PanelSettingsFieldDescriptor *field =
            ArchDock::PanelSettingsSchema::panelDescriptor(key);
        if (field)
        {
            result.insert(key, record.value(key, field->defaultValue));
        }
    }
    return result;
}

std::optional<ArchDock::PanelSettingsTransactionDraft>
PanelWindow::preparePanelSettingsDraft(
    const QString &panelId,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QVariantMap &globalValues,
    ArchDock::PanelSettingsTransactionOutcome *outcome) const
{
    if (outcome)
    {
        *outcome = {};
        outcome->panelId = panelId;
        outcome->expectedRevision = expectedRevision;
    }

    QString snapshotError;
    const std::optional<ArchDock::PanelDefinition> currentPanel =
        m_panelRegistry.panelDefinition(panelId, &snapshotError);
    if (!currentPanel.has_value())
    {
        if (outcome)
        {
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->errorCode = QStringLiteral("panel-not-found");
            outcome->errorMessage = snapshotError;
        }
        return std::nullopt;
    }
    if (outcome)
    {
        outcome->previousRevision = currentPanel->settingsRevision;
        outcome->revision = currentPanel->settingsRevision;
    }

    const QVariantMap currentGlobals = m_settings.transactionSnapshot();
    QVariantMap candidateGlobals;
    QString globalError;
    if (!m_settings.stageTransaction(globalValues, &candidateGlobals, &globalError))
    {
        if (outcome)
        {
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->errorCode = QStringLiteral("invalid-global-settings");
            outcome->errorMessage = globalError;
        }
        return std::nullopt;
    }

    ArchDock::PanelSettingsTransactionRequest request;
    request.panelId = panelId;
    request.expectedRevision = expectedRevision;
    request.panelValues = panelValues;
    if (request.panelValues.contains(QStringLiteral("segments")))
        request.panelValues.insert(QStringLiteral("segments"),
            decodedSegmentValue(request.panelValues.value(QStringLiteral("segments"))));
    request.globalValues = globalValues;
    std::optional<ArchDock::PanelSettingsTransactionDraft> draft =
        ArchDock::PanelSettingsTransaction::prepare(
            *currentPanel,
            currentGlobals,
            candidateGlobals,
            request,
            outcome,
            [this](const ArchDock::PanelDefinition &candidate)
            {
                return m_panelRegistry.resolvePanelCapabilities(candidate);
            });
    if (!draft.has_value())
    {
        return std::nullopt;
    }

    if (draft->candidatePanel.segments != currentPanel->segments)
    {
        QVariantList entries = draft->candidatePanel.host.kind == ArchDock::PanelHostKind::FreeDesktop
            ? freePanelEntries(draft->candidatePanel)
            : m_dockModel.panelEntries(draft->candidatePanel.content.type);
        entries.append(statusEntriesFor(draft->candidatePanel, true));
        QString segmentError;
        const auto projected = ArchDock::PanelContentTransaction::segmentEntries(
            draft->candidatePanel, entries, &segmentError, true);
        Q_UNUSED(projected);
        if (!segmentError.isEmpty())
        {
            if (outcome)
            {
                outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
                outcome->revision = currentPanel->settingsRevision;
                outcome->errorCode = QStringLiteral("invalid-segment-ownership");
                outcome->errorMessage = segmentError;
            }
            return std::nullopt;
        }
    }

    if (panelValues.contains(QStringLiteral("screen")))
    {
        const int requestedScreen = panelValues.value(QStringLiteral("screen")).toInt();
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (requestedScreen < 0 || requestedScreen >= screens.size())
        {
            if (outcome)
            {
                outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
                outcome->revision = currentPanel->settingsRevision;
                outcome->errorCode = QStringLiteral("screen-out-of-range");
                outcome->errorMessage = QStringLiteral(
                    "the requested screen is not available");
            }
            return std::nullopt;
        }
        draft->candidatePanel.host.screenIndex = requestedScreen;
        draft->candidatePanel.host.screenId = ArchDock::persistentScreenId(
            screens.at(requestedScreen));
    }

    const ArchDock::CapabilityResolution resolution =
        m_panelRegistry.resolvePanelCapabilities(draft->candidatePanel);
    const auto segmentCapabilities = ArchDock::PanelCapabilityResolver::segmentCapabilities(
        draft->candidatePanel, resolution, !m_systemStatus.availableSources().isEmpty());
    const bool inheritedSegment = draft->candidatePanel.segments ==
        QList<ArchDock::PanelSegmentDefinition>{ArchDock::PanelSegmentDefinition{}};
    QString segmentError;
    if (!inheritedSegment && !segmentCapabilities.value(QStringLiteral("available")).toBool())
        segmentError = segmentCapabilities.value(QStringLiteral("reasonCode")).toString();
    if (!inheritedSegment)
    {
        for (const auto &segment : draft->candidatePanel.segments)
        {
            const bool retainedStatus = segment.source == QStringLiteral("status")
                && std::any_of(currentPanel->segments.cbegin(), currentPanel->segments.cend(),
                    [&segment](const auto &old) { return old == segment; });
            if ((!retainedStatus && !segmentCapabilities.value(QStringLiteral("sources")).toStringList().contains(segment.source)) ||
                !segmentCapabilities.value(QStringLiteral("motionProfiles")).toStringList().contains(segment.motionProfile))
                segmentError = QStringLiteral("the segment source or motion profile is unavailable");
            if (segment.background != QStringLiteral("solid") && segment.corners != QStringLiteral("inherited"))
                segmentError = QStringLiteral("segment corner overrides require a solid background");
        }
    }
    if (!segmentError.isEmpty())
    {
        if (outcome)
        {
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->revision = currentPanel->settingsRevision;
            outcome->errorCode = QStringLiteral("unavailable-segment-feature");
            outcome->errorMessage = segmentError;
        }
        return std::nullopt;
    }
    if (outcome)
    {
        outcome->capabilityResolution = resolution;
    }
    const QVariantList broadFields = panelSettingsEditorFields(
        draft->candidatePanel, resolution, QStringLiteral("studio"));
    QSet<QString> availableFields;
    for (const QVariant &value : broadFields)
    {
        availableFields.insert(
            value.toMap().value(QStringLiteral("key")).toString());
    }
    QSet<QString> previousFields;
    for (const QVariant &value : panelSettingsEditorFields(*currentPanel,
             m_panelRegistry.resolvePanelCapabilities(*currentPanel), QStringLiteral("studio")))
    {
        previousFields.insert(value.toMap().value(QStringLiteral("key")).toString());
    }
    const QVariantMap currentValues = currentPanel->toLegacyMap();
    const QVariantMap candidateValues = draft->candidatePanel.toLegacyMap();
    for (auto it = panelValues.cbegin(); it != panelValues.cend(); ++it)
    {
        const ArchDock::PanelSettingsFieldDescriptor *field =
            ArchDock::PanelSettingsSchema::panelDescriptor(it.key());
        if (field && field->access == ArchDock::PanelSettingsFieldAccess::Editor &&
            field->editor.isPresented() && !availableFields.contains(it.key()))
        {
            // Studio submits its full snapshot when changing capabilities.
            // Retain inactive values only if they were previously available
            // and the normalized candidate leaves them unchanged.
            if ((previousFields.contains(it.key()) || field->editor.capability == QStringLiteral("application-overlays")) &&
                candidateValues.value(it.key(), field->defaultValue) ==
                    currentValues.value(it.key(), field->defaultValue))
            {
                continue;
            }
            if (outcome)
            {
                outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
                outcome->revision = currentPanel->settingsRevision;
                outcome->errorCode = QStringLiteral("unavailable-panel-field");
                outcome->errorMessage = QStringLiteral(
                    "the field '%1' is unavailable for the resolved candidate")
                    .arg(it.key());
            }
            return std::nullopt;
        }
    }

    QString validationError;
    if (!draft->candidatePanel.isValid(&validationError))
    {
        if (outcome)
        {
            outcome->status = ArchDock::PanelSettingsTransactionStatus::ValidationFailed;
            outcome->revision = currentPanel->settingsRevision;
            outcome->errorCode = QStringLiteral("invalid-panel-definition");
            outcome->errorMessage = validationError;
        }
        return std::nullopt;
    }
    return draft;
}

QVariantMap PanelWindow::panelSettingsEditorSnapshot(
    const QString &panelId,
    const QString &consumer) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    if (!definition.has_value())
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("validation-failed")},
            {QStringLiteral("errorCode"), QStringLiteral("panel-not-found")},
            {QStringLiteral("panelId"), panelId},
        };
    }

    const QString normalizedConsumer = consumer.trimmed().toLower() ==
            QStringLiteral("studio")
        ? QStringLiteral("studio")
        : QStringLiteral("native");
    const ArchDock::CapabilityResolution resolution =
        m_panelRegistry.resolvePanelCapabilities(*definition);
    const QVariantList panelFields = panelSettingsEditorFields(
        *definition, resolution, normalizedConsumer);
    const QVariantList globalFields = ArchDock::PanelSettingsSchema::editorDescriptors(
        ArchDock::PanelSettingsFieldScope::Global, normalizedConsumer);
    QSet<QString> globalKeys;
    for (const QVariant &value : globalFields)
    {
        globalKeys.insert(value.toMap().value(QStringLiteral("key")).toString());
    }
    QVariantMap globalValues;
    const QVariantMap globalSnapshot = m_settings.editorTransactionSnapshot();
    for (const QString &key : std::as_const(globalKeys))
    {
        globalValues.insert(key, globalSnapshot.value(key));
    }
    QString themeProjectionError;
    const std::optional<QVariantMap> themeProjection =
        m_panelRegistry.themeRuntimeProjection(
            *definition, &themeProjectionError);
    QString iconStyleProjectionError;
    const std::optional<QVariantMap> iconStyleProjection =
        m_panelRegistry.iconStyleRuntimeProjection(
            *definition, &iconStyleProjectionError);

    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("status"), QStringLiteral("loaded")},
        {QStringLiteral("errorCode"), QString{}},
        {QStringLiteral("schemaVersion"), ArchDock::PanelSettingsSchema::CurrentVersion},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("revision"),
         QVariant::fromValue<qulonglong>(definition->settingsRevision)},
        {QStringLiteral("consumer"), normalizedConsumer},
        {QStringLiteral("panelValues"),
         panelSettingsEditorValues(*definition, panelFields)},
        {QStringLiteral("globalValues"), globalValues},
        {QStringLiteral("panelFields"), panelFields},
        {QStringLiteral("globalFields"), globalFields},
        {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
        {QStringLiteral("themeDefinition"),
         themeProjection.value_or(QVariantMap{})},
        {QStringLiteral("themeProjectionStatus"),
         themeProjection.has_value()
             ? QStringLiteral("ready")
             : themeProjectionError.isEmpty()
                 ? QStringLiteral("unavailable")
                 : QStringLiteral("error")},
        {QStringLiteral("themeProjectionError"), themeProjectionError},
        {QStringLiteral("iconStyleDefinition"),
         iconStyleProjection.value_or(QVariantMap{})},
        {QStringLiteral("iconStyleProjectionStatus"),
         iconStyleProjection.has_value()
             ? QStringLiteral("ready") : QStringLiteral("error")},
        {QStringLiteral("iconStyleProjectionError"), iconStyleProjectionError},
        {QStringLiteral("iconStyles"), m_panelRegistry.iconStyleDefinitions()},
        {QStringLiteral("animationProfiles"),
         m_panelRegistry.animationProfileDefinitions()},
        {QStringLiteral("themes"), resolvedThemeDefinitions(panelId)},
    };
}

QVariantMap PanelWindow::resolvePanelSettingsEditorDraft(
    const QString &panelId,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QVariantMap &globalValues,
    const QString &consumer) const
{
    ArchDock::PanelSettingsTransactionOutcome outcome;
    const std::optional<ArchDock::PanelSettingsTransactionDraft> draft =
        preparePanelSettingsDraft(
            panelId,
            expectedRevision,
            panelValues,
            globalValues,
            &outcome);
    if (!draft.has_value())
    {
        return outcome.toVariantMap();
    }

    const QString normalizedConsumer = consumer.trimmed().toLower() ==
            QStringLiteral("studio")
        ? QStringLiteral("studio")
        : QStringLiteral("native");
    const ArchDock::CapabilityResolution resolution =
        m_panelRegistry.resolvePanelCapabilities(draft->candidatePanel);
    const QVariantList panelFields = panelSettingsEditorFields(
        draft->candidatePanel, resolution, normalizedConsumer);
    const QVariantList globalFields = ArchDock::PanelSettingsSchema::editorDescriptors(
        ArchDock::PanelSettingsFieldScope::Global, normalizedConsumer);
    QSet<QString> globalKeys;
    for (const QVariant &value : globalFields)
    {
        globalKeys.insert(value.toMap().value(QStringLiteral("key")).toString());
    }
    QVariantMap projectedGlobals;
    for (const QString &key : std::as_const(globalKeys))
    {
        projectedGlobals.insert(key, draft->candidateGlobals.value(key));
    }
    QString themeProjectionError;
    const std::optional<QVariantMap> themeProjection =
        m_panelRegistry.themeRuntimeProjection(
            draft->candidatePanel, &themeProjectionError);
    QString iconStyleProjectionError;
    const std::optional<QVariantMap> iconStyleProjection =
        m_panelRegistry.iconStyleRuntimeProjection(
            draft->candidatePanel, &iconStyleProjectionError);

    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("status"), QStringLiteral("resolved")},
        {QStringLiteral("errorCode"), QString{}},
        {QStringLiteral("schemaVersion"), ArchDock::PanelSettingsSchema::CurrentVersion},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("revision"), QVariant::fromValue<qulonglong>(expectedRevision)},
        {QStringLiteral("candidateRevision"),
         QVariant::fromValue<qulonglong>(draft->candidatePanel.settingsRevision)},
        {QStringLiteral("consumer"), normalizedConsumer},
        {QStringLiteral("panelValues"),
         panelSettingsEditorValues(draft->candidatePanel, panelFields)},
        {QStringLiteral("globalValues"), projectedGlobals},
        {QStringLiteral("panelFields"), panelFields},
        {QStringLiteral("globalFields"), globalFields},
        {QStringLiteral("capabilityResolution"), resolution.toVariantMap()},
        {QStringLiteral("themeDefinition"),
         themeProjection.value_or(QVariantMap{})},
        {QStringLiteral("themeProjectionStatus"),
         themeProjection.has_value()
             ? QStringLiteral("ready")
             : themeProjectionError.isEmpty()
                 ? QStringLiteral("unavailable")
                 : QStringLiteral("error")},
        {QStringLiteral("themeProjectionError"), themeProjectionError},
        {QStringLiteral("iconStyleDefinition"),
         iconStyleProjection.value_or(QVariantMap{})},
        {QStringLiteral("iconStyleProjectionStatus"),
         iconStyleProjection.has_value()
             ? QStringLiteral("ready") : QStringLiteral("error")},
        {QStringLiteral("iconStyleProjectionError"), iconStyleProjectionError},
        {QStringLiteral("iconStyles"), m_panelRegistry.iconStyleDefinitions()},
        {QStringLiteral("animationProfiles"),
         m_panelRegistry.animationProfileDefinitions()},
        {QStringLiteral("themes"),
         resolvedThemeDefinitions(panelId, panelValues)},
    };
}

QVariantMap PanelWindow::applyPanelSettingsTransaction(
    const QString &panelId,
    qulonglong expectedRevision,
    const QVariantMap &panelValues,
    const QVariantMap &globalValues)
{
    ArchDock::PanelSettingsTransactionOutcome outcome;
    const std::optional<ArchDock::PanelSettingsTransactionDraft> draft =
        preparePanelSettingsDraft(
            panelId,
            expectedRevision,
            panelValues,
            globalValues,
            &outcome);
    if (!draft.has_value())
    {
        return outcome.toVariantMap();
    }

    return commitPanelSettingsDraft(*draft, outcome);
}

QVariantMap PanelWindow::commitPanelSettingsDraft(
    ArchDock::PanelSettingsTransactionDraft draft,
    ArchDock::PanelSettingsTransactionOutcome outcome)
{
    ArchDock::PresetApplication::markCustomized(draft.previousPanel, &draft.candidatePanel);
    QString persistenceError;
    if (!m_panelRegistry.persistPanelSettingsTransaction(draft, &persistenceError))
    {
        const std::optional<ArchDock::PanelDefinition> latest =
            m_panelRegistry.panelDefinition(draft.previousPanel.identity.id);
        const bool conflict = latest.has_value() &&
            latest->settingsRevision != draft.previousPanel.settingsRevision;
        outcome.status = conflict
            ? ArchDock::PanelSettingsTransactionStatus::RevisionConflict
            : ArchDock::PanelSettingsTransactionStatus::PersistenceFailed;
        outcome.errorCode = conflict
            ? QStringLiteral("stale-revision")
            : QStringLiteral("persistence-failed");
        outcome.errorMessage = persistenceError;
        return outcome.toVariantMap();
    }

    outcome.hostResults = applyPanelSettingsHosts(draft);
    if (ArchDock::PanelSettingsTransaction::requiredHostsSucceeded(
            outcome.hostResults))
    {
        m_settingsTransactionAdoptionActive = true;
        m_settings.adoptTransaction(draft.candidateGlobals);
        m_settingsTransactionAdoptionActive = false;

        ArchDock::PanelSettingsHostResult renderer;
        renderer.component = QStringLiteral("renderer-notification");
        renderer.required = true;
        renderer.success = true;
        renderer.status = QStringLiteral("published");
        outcome.hostResults.append(renderer);
        outcome.status = ArchDock::PanelSettingsTransactionStatus::Succeeded;
        outcome.revision = draft.candidatePanel.settingsRevision;
        m_panelRegistry.notifyPanelSettingsTransactionAdopted(
            panelSettingsTopologyChanged(
                draft.previousPanel, draft.candidatePanel));
        return outcome.toVariantMap();
    }

    const QList<ArchDock::PanelSettingsHostResult> hostRollbackResults =
        rollbackPanelSettingsHosts(draft);
    outcome.hostResults.append(hostRollbackResults);
    quint64 rollbackRevision = 0;
    QString rollbackError;
    if (m_panelRegistry.rollbackPanelSettingsTransaction(
            draft,
            draft.candidatePanel.settingsRevision,
            &rollbackRevision,
            &rollbackError))
    {
        outcome.status = ArchDock::PanelSettingsTransactionStatus::HostFailed;
        outcome.errorCode = QStringLiteral("required-host-apply-failed");
        outcome.errorMessage = QStringLiteral(
            "a required host change failed; the persisted draft was rolled back");
        outcome.rolledBack = true;
        outcome.rollbackRevision = rollbackRevision;
        m_panelRegistry.notifyPanelSettingsTransactionAdopted(
            panelSettingsTopologyChanged(
                draft.previousPanel, draft.candidatePanel));
        return outcome.toVariantMap();
    }

    m_settingsTransactionAdoptionActive = true;
    m_settings.adoptTransaction(draft.candidateGlobals);
    m_settingsTransactionAdoptionActive = false;
    outcome.status = ArchDock::PanelSettingsTransactionStatus::RollbackFailed;
    outcome.errorCode = QStringLiteral("rollback-persistence-failed");
    outcome.errorMessage = rollbackError;
    m_panelRegistry.notifyPanelSettingsTransactionAdopted(
        panelSettingsTopologyChanged(draft.previousPanel, draft.candidatePanel));
    return outcome.toVariantMap();
}

bool PanelWindow::setDockConfiguration(const QString &panelId,
                                       const QString &key,
                                       const QVariant &value)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    const bool globalField =
        ArchDock::PanelSettingsSchema::supportsMutationInterface(
            ArchDock::PanelSettingsFieldScope::Global,
            key,
            QStringLiteral("dock-configuration"));
    const bool panelField =
        ArchDock::PanelSettingsSchema::supportsMutationInterface(
            ArchDock::PanelSettingsFieldScope::Panel,
            key,
            QStringLiteral("dock-configuration"));
    if (globalField == panelField)
    {
        return false;
    }
    const qulonglong expectedRevision = m_panelRegistry.panelValue(
        panelId, QStringLiteral("settingsRevision")).toULongLong();
    const QVariantMap result = applyPanelSettingsTransaction(
        panelId,
        expectedRevision,
        panelField ? QVariantMap{{key, value}} : QVariantMap{},
        globalField ? QVariantMap{{key, value}} : QVariantMap{});
    return result.value(QStringLiteral("success")).toBool();
}

QList<ArchDock::PanelSettingsHostResult> PanelWindow::applyPanelSettingsHosts(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    bool includeVisibility)
{
    QList<ArchDock::PanelSettingsHostResult> results;
    const QVariantMap before = draft.previousPanel.toLegacyMap();
    const QVariantMap candidate = draft.candidatePanel.toLegacyMap();
    const QString panelId = draft.candidatePanel.identity.id;
    const bool nativeHost =
        draft.candidatePanel.host.kind == ArchDock::PanelHostKind::NativeEdge;
    const bool ownedHost = nativeHost &&
        draft.candidatePanel.host.nativePanelId >= 0 &&
        !draft.candidatePanel.host.nativeOwnershipToken.trimmed().isEmpty();
    const auto anyChanged = [&before, &candidate](const QStringList &keys)
    {
        for (const QString &key : keys)
        {
            if (before.value(key) != candidate.value(key))
            {
                return true;
            }
        }
        return false;
    };

    static const QStringList placementKeys{
        QStringLiteral("edge"),
        QStringLiteral("screen"),
        QStringLiteral("screenId"),
        QStringLiteral("alignment"),
        QStringLiteral("dynamic"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("floatingMargin"),
        QStringLiteral("thickness"),
        QStringLiteral("lengthMode"),
        QStringLiteral("minimumLength"),
        QStringLiteral("maximumLength"),
    };
    ArchDock::PanelSettingsHostResult placement;
    placement.component = QStringLiteral("native-placement");
    placement.required = ownedHost && anyChanged(placementKeys);
    if (placement.required)
    {
        QVariantMap placementValues;
        for (const QString &key : placementKeys)
        {
            if (candidate.contains(key))
            {
                placementValues.insert(key, candidate.value(key));
            }
        }
        ArchDock::PlasmaPanelPlacementApplyResult applied =
            applyNativePanelPlacementTransaction(
                panelId,
                draft.candidatePanel.host.nativePanelId,
                draft.candidatePanel.host.nativeOwnershipToken,
                placementValues,
                false);
        placement.success = applied.success();
        placement.status = applied.status;
        placement.errorCode = applied.errorCode;
        placement.details = applied.toVariantMap();
        recordNativePanelPlacementResult(panelId, std::move(applied));
    }
    results.append(placement);

    const bool visibilityChanged = anyChanged({
        QStringLiteral("visible"),
        QStringLiteral("visibilityMode"),
    });
    ArchDock::PanelSettingsHostResult visibility;
    visibility.component = QStringLiteral("native-visibility");
    visibility.required = includeVisibility && ownedHost && visibilityChanged;
    if (visibility.required)
    {
        const ArchDock::PanelVisibilityMode mode =
            ArchDock::panelVisibilityModeFromString(
                draft.candidatePanel.visibility.hostMode);
        const bool applied = reconcileNativePanelVisibility(
            panelId,
            draft.candidatePanel.host.nativePanelId,
            draft.candidatePanel.host.nativeOwnershipToken,
            mode,
            draft.candidatePanel.visibility.visible);
        visibility.details = nativePanelVisibilityStatus(panelId);
        const QString effectiveMode = visibility.details.value(
            QStringLiteral("effectiveMode")).toString();
        visibility.success = applied &&
            (effectiveMode.isEmpty() ||
             effectiveMode == draft.candidatePanel.visibility.hostMode);
        visibility.status = visibility.details.value(
            QStringLiteral("status"),
            visibility.success ? QStringLiteral("applied")
                               : QStringLiteral("failed")).toString();
        visibility.errorCode = visibility.details.value(
            QStringLiteral("errorCode")).toString();
        if (applied && !visibility.success)
        {
            visibility.errorCode = QStringLiteral("uncommitted-visibility-fallback");
        }
    }
    results.append(visibility);

    ArchDock::PanelSettingsHostResult rendererHost;
    rendererHost.component = QStringLiteral("native-renderer");
    const bool typeChanged = before.value(QStringLiteral("type")) !=
        candidate.value(QStringLiteral("type"));
    rendererHost.required = ownedHost && typeChanged;
    if (rendererHost.required)
    {
        const QString oldType = draft.previousPanel.content.type;
        const QString newType = draft.candidatePanel.content.type;
        if (panelTypeNeedsDockApplet(oldType) && panelTypeNeedsDockApplet(newType) &&
            nativeDockAppletIsOwned(
                panelId,
                draft.candidatePanel.host.nativePanelId,
                draft.candidatePanel.host.nativeDockAppletId))
        {
            const int applied = evaluatePlasmaScriptResult(
                QStringLiteral(
                    "var panel = panelById(%1);"
                    "var dock = panel ? panel.widgetById(%2) : null;"
                    "if (!dock || dock.type !== 'org.archdock.dock') { print(0); }"
                    "else { dock.currentConfigGroup = ['General'];"
                    "dock.writeConfig('panelType', %3); dock.reloadConfig();"
                    "print(String(dock.readConfig('panelType', '')) === %3 ? 1 : 0); }")
                    .arg(draft.candidatePanel.host.nativePanelId)
                    .arg(draft.candidatePanel.host.nativeDockAppletId)
                    .arg(plasmaScriptStringLiteral(newType)));
            rendererHost.success = applied == 1;
            rendererHost.status = rendererHost.success
                ? QStringLiteral("applied")
                : QStringLiteral("failed");
            rendererHost.errorCode = rendererHost.success
                ? QString{}
                : QStringLiteral("renderer-type-readback-failed");
        }
        else
        {
            rendererHost.success = false;
            rendererHost.status = QStringLiteral("unsupported");
            rendererHost.errorCode = QStringLiteral(
                "renderer-topology-change-not-atomic");
        }
    }
    results.append(rendererHost);
    return results;
}

QList<ArchDock::PanelSettingsHostResult> PanelWindow::rollbackPanelSettingsHosts(
    const ArchDock::PanelSettingsTransactionDraft &draft,
    bool includeVisibility)
{
    ArchDock::PanelSettingsTransactionDraft reverseDraft{
        draft.candidatePanel,
        draft.previousPanel,
        draft.candidateGlobals,
        draft.previousGlobals,
    };
    QList<ArchDock::PanelSettingsHostResult> results =
        applyPanelSettingsHosts(reverseDraft, includeVisibility);
    for (ArchDock::PanelSettingsHostResult &result : results)
    {
        result.component.prepend(QStringLiteral("rollback-"));
    }
    return results;
}

bool PanelWindow::panelSettingsTopologyChanged(
    const ArchDock::PanelDefinition &before,
    const ArchDock::PanelDefinition &after)
{
    return before.host != after.host ||
        before.placement != after.placement ||
        before.visibility != after.visibility ||
        before.content.type != after.content.type;
}

bool PanelWindow::setDockStringConfiguration(const QString &panelId,
                                             const QString &key,
                                             const QString &value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockIntegerConfiguration(const QString &panelId,
                                              const QString &key,
                                              int value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockRealConfiguration(const QString &panelId,
                                           const QString &key,
                                           double value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockBooleanConfiguration(const QString &panelId,
                                              const QString &key,
                                              bool value)
{
    return setDockConfiguration(panelId, key, value);
}

QVariantList PanelWindow::dockEntries(const QString &panelType) const
{
    return m_dockModel.panelEntries(panelType);
}

namespace
{

// One panel-specific free entry built from a local URL. A desktop file also
// records the desktop identity it launches, so a running instance of the same
// application can be merged into this entry on a hybrid panel.
QVariantMap freeUrlEntry(const QUrl &url,
                         const QString &entryId,
                         QMimeDatabase &mimeDatabase,
                         QString *desktopIdentity)
{
    const QFileInfo info(url.toLocalFile());
    QString iconName;
    QString displayName = info.fileName();
    if (info.isDir())
    {
        iconName = QStringLiteral("folder");
    }
    else if (info.suffix().compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0)
    {
        QSettings desktopEntry(info.absoluteFilePath(), QSettings::IniFormat);
        desktopEntry.beginGroup(QStringLiteral("Desktop Entry"));
        displayName = desktopEntry.value(
            QStringLiteral("Name"), info.completeBaseName()).toString();
        iconName = desktopEntry.value(
            QStringLiteral("Icon"), QStringLiteral("application-x-executable")).toString();
        desktopEntry.endGroup();
        if (desktopIdentity)
        {
            *desktopIdentity = ArchDock::IconEntryIdentity::forApplication(
                QString{}, info.fileName());
        }
    }
    else
    {
        iconName = mimeDatabase.mimeTypeForFile(info).iconName();
    }
    QVariantMap entry{
        {QStringLiteral("appId"), entryId},
        {QStringLiteral("panelEntryId"), entryId},
        {QStringLiteral("desktopFileName"), QString{}},
        {QStringLiteral("baseIconName"), iconName},
        {QStringLiteral("iconName"), iconName},
        {QStringLiteral("baseDisplayName"), displayName},
        {QStringLiteral("displayName"), displayName},
        {QStringLiteral("pinned"), true},
        {QStringLiteral("running"), false},
        {QStringLiteral("active"), false},
        {QStringLiteral("minimized"), false},
        {QStringLiteral("windowCount"), 0},
        {QStringLiteral("windowIds"), QStringList{}},
        {QStringLiteral("windowTitles"), QStringList{}},
        {QStringLiteral("isFolder"), info.isDir()}};
    entry.insert(
        QStringLiteral("stableIdentity"),
        ArchDock::IconEntryIdentity::forEntry(entry));
    const QVariantMap launcher = DockModel::desktopEntryActions(info.absoluteFilePath());
    entry.insert(QStringLiteral("canPin"), true);
    entry.insert(QStringLiteral("canNewInstance"), launcher.value(QStringLiteral("canNewInstance")));
    entry.insert(QStringLiteral("desktopActions"), launcher.value(QStringLiteral("desktopActions")));
    return entry;
}

}

// The free-panel content semantics: `empty` shows nothing, `launcher` shows
// the panel's own ordered entries, `tasks` shows running applications, and
// `hybrid` shows the panel's entries with running-only applications after
// them. A running instance of a pinned desktop entry is merged into that
// entry: the entry keeps its own identity, label and glyph, gains the running
// state, and carries the application id under `runningAppId` so window
// actions reach the application model.
QVariantList PanelWindow::freePanelEntries(
    const ArchDock::PanelDefinition &definition) const
{
    QVariantList entries;
    const QString contentType = definition.content.type;
    if (contentType == QStringLiteral("empty"))
    {
        return entries;
    }

    QHash<QString, int> panelIndexByIdentity;
    if (contentType != QStringLiteral("tasks"))
    {
        QMimeDatabase mimeDatabase;
        for (const QString &entryId : definition.content.canonicalEntryOrder())
        {
            if (ArchDock::PanelContent::isUrlEntryId(entryId))
            {
                const QUrl url = QUrl::fromEncoded(entryId.mid(9).toUtf8());
                if (!url.isLocalFile() || !QFileInfo::exists(url.toLocalFile()))
                {
                    continue;
                }
                QString desktopIdentity;
                QVariantMap entry = freeUrlEntry(
                    url, entryId, mimeDatabase, &desktopIdentity);
                if (!desktopIdentity.isEmpty())
                {
                    panelIndexByIdentity.insert(desktopIdentity, entries.size());
                }
                entries.append(entry);
                continue;
            }
            QVariantMap entry = m_dockModel.applicationEntry(entryId);
            if (entry.isEmpty())
            {
                continue;
            }
            entry.insert(QStringLiteral("pinned"), true);
            entry.insert(QStringLiteral("panelEntryId"), entryId);
            panelIndexByIdentity.insert(
                entry.value(QStringLiteral("stableIdentity")).toString(),
                entries.size());
            entries.append(entry);
        }
    }

    if (contentType == QStringLiteral("tasks") || contentType == QStringLiteral("hybrid"))
    {
        static const QStringList runningStateKeys{
            QStringLiteral("running"),
            QStringLiteral("active"),
            QStringLiteral("minimized"),
            QStringLiteral("windowCount"),
            QStringLiteral("windowIds"),
            QStringLiteral("windowTitles"),
            QStringLiteral("windowPreviews"),
        };
        for (const QVariant &value : m_dockModel.panelEntries(QStringLiteral("tasks")))
        {
            QVariantMap running = value.toMap();
            const auto merged = panelIndexByIdentity.constFind(
                running.value(QStringLiteral("stableIdentity")).toString());
            if (merged != panelIndexByIdentity.cend())
            {
                QVariantMap target = entries.at(*merged).toMap();
                for (const QString &key : runningStateKeys)
                {
                    target.insert(key, running.value(key));
                }
                target.insert(
                    QStringLiteral("runningAppId"),
                    running.value(QStringLiteral("appId")));
                entries[*merged] = target;
                continue;
            }
            running.insert(QStringLiteral("pinned"), false);
            running.insert(QStringLiteral("panelEntryId"), QString{});
            entries.append(running);
        }
    }
    return entries;
}

QVariantList PanelWindow::dockEntriesForPanel(const QString &panelId,
                                              const QString &panelType) const
{
    QVariantList entries;
    const std::optional<ArchDock::PanelDefinition> definition =
        runtimePanelDefinition(panelId);
    if (definition && definition->host.kind == ArchDock::PanelHostKind::FreeDesktop)
    {
        // A free host's applet stores `panelType=empty` as an ownership
        // marker; the record's content type is authoritative for what it
        // shows, so the requested type is deliberately ignored here.
        if (!definition.has_value())
        {
            return entries;
        }
        entries = freePanelEntries(*definition);
    }
    else
    {
        const bool auditioning = m_presetAudition &&
            m_presetAudition->previewDefinition(panelId).has_value();
        entries = m_dockModel.panelEntries(auditioning ? definition->content.type : panelType);
    }

    if (!definition.has_value())
    {
        return entries;
    }
    for (QVariant &value : entries)
    {
        QVariantMap entry = value.toMap();
        const auto overlay = entryOverlay(entry);
        for (auto it = overlay.cbegin(); it != overlay.cend(); ++it) entry.insert(it.key(), it.value());
        if (!definition->content.showBadges) entry[QStringLiteral("badgeText")] = QString{};
        if (!definition->content.showProgress) entry[QStringLiteral("progress")] = -1.0;
        if (!definition->content.showTemporaryStatus) entry[QStringLiteral("temporaryStatus")] = QString{};
        const QString identity = ArchDock::IconEntryIdentity::forEntry(entry);
        entry.insert(QStringLiteral("stableIdentity"), identity);
        entry.insert(
            QStringLiteral("iconPropertiesSupported"),
            ArchDock::IconEntryIdentity::isValid(identity) &&
                entry.value(QStringLiteral("pinned")).toBool());
        const QVariantMap resolution =
            m_panelRegistry.resolveIconEntryOverride(*definition, entry);
        entry.insert(QStringLiteral("iconOverrideResolution"), resolution);
        entry.insert(
            QStringLiteral("iconOverride"),
            resolution.value(QStringLiteral("override")));
        entry.insert(
            QStringLiteral("iconOverrideApplied"),
            resolution.value(QStringLiteral("overrideApplied")));
        entry.insert(
            QStringLiteral("resolvedGlyph"),
            resolution.value(QStringLiteral("resolvedGlyph")));
        entry.insert(
            QStringLiteral("resolvedLabel"),
            resolution.value(QStringLiteral("resolvedLabel")));
        entry.insert(
            QStringLiteral("tileEnabled"),
            resolution.value(QStringLiteral("tileEnabled")));
        entry.insert(
            QStringLiteral("animationProfileReference"),
            resolution.value(QStringLiteral("animationProfileReference")));
        entry.insert(
            QStringLiteral("resolvedIconStyleDefinition"),
            resolution.value(QStringLiteral("iconStyleDefinition")));
        entry.insert(
            QStringLiteral("iconName"),
            resolution.value(QStringLiteral("resolvedGlyph")));
        entry.insert(
            QStringLiteral("displayName"),
            resolution.value(QStringLiteral("resolvedLabel")));
        value = entry;
    }
    entries.append(statusEntriesFor(*definition));
    return ArchDock::PanelContentTransaction::segmentEntries(*definition, entries);
}

std::optional<QVariantMap> PanelWindow::iconEntryForIdentity(
    const QString &panelId,
    const QString &entryIdentity) const
{
    const QString identity = entryIdentity.trimmed();
    if (!ArchDock::IconEntryIdentity::isValid(identity))
    {
        return std::nullopt;
    }
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    if (!definition.has_value())
    {
        return std::nullopt;
    }
    for (const QVariant &value : dockEntriesForPanel(
             panelId, definition->content.type))
    {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("stableIdentity")).toString() == identity)
        {
            return entry;
        }
    }
    return std::nullopt;
}

QVariantMap PanelWindow::iconOverrideSnapshot(
    const QString &panelId,
    const QVariantMap &entry) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    const QString identity = ArchDock::IconEntryIdentity::forEntry(entry);
    if (!definition.has_value() ||
        !ArchDock::IconEntryIdentity::isValid(identity))
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("entryIdentity"), identity},
            {QStringLiteral("errorCode"),
             definition.has_value()
                 ? QStringLiteral("invalid-entry-identity")
                 : QStringLiteral("panel-not-found")},
        };
    }

    const QVariantMap resolution =
        m_panelRegistry.resolveIconEntryOverride(*definition, entry);
    return {
        {QStringLiteral("success"), true},
        {QStringLiteral("status"), QStringLiteral("loaded")},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("revision"),
         QVariant::fromValue<qulonglong>(definition->settingsRevision)},
        {QStringLiteral("entryIdentity"), identity},
        {QStringLiteral("baseGlyph"),
         entry.value(QStringLiteral("baseIconName"),
                     entry.value(QStringLiteral("iconName")))},
        {QStringLiteral("baseLabel"),
         entry.value(QStringLiteral("baseDisplayName"),
                     entry.value(QStringLiteral("displayName")))},
        {QStringLiteral("override"),
         resolution.value(QStringLiteral("override"))},
        {QStringLiteral("resolution"), resolution},
        {QStringLiteral("iconStyles"), m_panelRegistry.iconStyleDefinitions()},
        {QStringLiteral("animationProfiles"),
         m_panelRegistry.animationProfileDefinitions()},
    };
}

QVariantMap PanelWindow::iconOverrideSnapshotForIdentity(
    const QString &panelId,
    const QString &entryIdentity) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    const QString identity = entryIdentity.trimmed();
    if (!definition.has_value() ||
        !ArchDock::IconEntryIdentity::isValid(identity))
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("entryIdentity"), identity},
            {QStringLiteral("errorCode"),
             definition.has_value()
                 ? QStringLiteral("invalid-entry-identity")
                 : QStringLiteral("panel-not-found")},
        };
    }

    const std::optional<QVariantMap> entry = iconEntryForIdentity(
        panelId, identity);
    if (!entry.has_value())
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("entryIdentity"), identity},
            {QStringLiteral("errorCode"), QStringLiteral("entry-not-found")},
        };
    }
    if (!entry->value(QStringLiteral("iconPropertiesSupported")).toBool())
    {
        return {
            {QStringLiteral("success"), false},
            {QStringLiteral("status"), QStringLiteral("unavailable")},
            {QStringLiteral("panelId"), panelId},
            {QStringLiteral("entryIdentity"), identity},
            {QStringLiteral("errorCode"), QStringLiteral("entry-not-supported")},
        };
    }
    return iconOverrideSnapshot(panelId, *entry);
}

QVariantMap PanelWindow::applyIconOverrideTransaction(
    const QString &panelId,
    qulonglong expectedRevision,
    const QString &entryIdentity,
    const QVariantMap &overrideValues)
{
    return commitIconOverrideTransaction(
        panelId,
        expectedRevision,
        entryIdentity,
        overrideValues,
        false);
}

QVariantMap PanelWindow::resetIconOverrideTransaction(
    const QString &panelId,
    qulonglong expectedRevision,
    const QString &entryIdentity)
{
    return commitIconOverrideTransaction(
        panelId,
        expectedRevision,
        entryIdentity,
        {},
        true);
}

QVariantMap PanelWindow::commitIconOverrideTransaction(
    const QString &panelId,
    qulonglong expectedRevision,
    const QString &entryIdentity,
    const QVariantMap &overrideValues,
    bool reset)
{
    ArchDock::IconOverrideTransactionOutcome outcome;
    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId);
    if (!current.has_value())
    {
        outcome.panelId = panelId;
        outcome.entryIdentity = entryIdentity;
        outcome.expectedRevision = expectedRevision;
        outcome.errorCode = QStringLiteral("panel-not-found");
        outcome.errorMessage = QStringLiteral("the target panel does not exist");
        return outcome.toVariantMap();
    }

    bool currentEntry = false;
    bool iconPropertiesSupported = false;
    QVariantMap entryAfterCommit;
    for (const QVariant &value : dockEntriesForPanel(panelId, current->content.type))
    {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("stableIdentity")).toString() ==
            entryIdentity.trimmed())
        {
            currentEntry = true;
            iconPropertiesSupported = entry.value(
                QStringLiteral("iconPropertiesSupported")).toBool();
            entryAfterCommit = entry;
            break;
        }
    }
    if (!currentEntry)
    {
        outcome.panelId = panelId;
        outcome.entryIdentity = entryIdentity;
        outcome.expectedRevision = expectedRevision;
        outcome.previousRevision = current->settingsRevision;
        outcome.revision = current->settingsRevision;
        outcome.reset = reset;
        outcome.errorCode = QStringLiteral("entry-not-found");
        outcome.errorMessage = QStringLiteral(
            "the target entry is no longer present on this panel");
        return outcome.toVariantMap();
    }
    if (!iconPropertiesSupported)
    {
        outcome.panelId = panelId;
        outcome.entryIdentity = entryIdentity;
        outcome.expectedRevision = expectedRevision;
        outcome.previousRevision = current->settingsRevision;
        outcome.revision = current->settingsRevision;
        outcome.reset = reset;
        outcome.errorCode = QStringLiteral("entry-not-supported");
        outcome.errorMessage = QStringLiteral(
            "running-only or transient entries cannot store icon properties");
        return outcome.toVariantMap();
    }

    const auto draft = ArchDock::IconOverrideTransaction::prepare(
        *current,
        {panelId,
         expectedRevision,
         entryIdentity,
         overrideValues,
         reset},
        &outcome,
        [this](const QString &styleReference)
        {
            const QVariantMap style =
                m_panelRegistry.iconStyleDefinition(styleReference);
            return style.value(QStringLiteral("selectionStatus")).toString() ==
                    QStringLiteral("selected") &&
                style.value(QStringLiteral("resolvedStyleId")).toString() ==
                    styleReference;
        });
    if (!draft.has_value())
    {
        return outcome.toVariantMap();
    }

    QString persistenceError;
    if (!m_panelRegistry.persistPanelDefinitionTransaction(
            draft->previousPanel,
            draft->candidatePanel,
            m_settings.transactionSnapshot(),
            &persistenceError))
    {
        const std::optional<ArchDock::PanelDefinition> latest =
            m_panelRegistry.panelDefinition(panelId);
        const bool conflict = latest.has_value() &&
            latest->settingsRevision != expectedRevision;
        outcome.status = conflict
            ? ArchDock::IconOverrideTransactionStatus::RevisionConflict
            : ArchDock::IconOverrideTransactionStatus::PersistenceFailed;
        outcome.errorCode = conflict
            ? QStringLiteral("stale-revision")
            : QStringLiteral("persistence-failed");
        outcome.errorMessage = persistenceError;
        return outcome.toVariantMap();
    }

    outcome.status = ArchDock::IconOverrideTransactionStatus::Succeeded;
    outcome.revision = draft->candidatePanel.settingsRevision;
    m_panelRegistry.notifyPanelSettingsTransactionAdopted(false);
    QVariantMap result = outcome.toVariantMap();
    for (const QVariant &value : dockEntriesForPanel(
             panelId, draft->candidatePanel.content.type))
    {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("stableIdentity")).toString() ==
            entryIdentity.trimmed())
        {
            entryAfterCommit = entry;
            break;
        }
    }
    result.insert(
        QStringLiteral("resolution"),
        entryAfterCommit.value(QStringLiteral("iconOverrideResolution")));
    return result;
}

bool PanelWindow::activateDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    if (appId.startsWith(QStringLiteral("free-url:")))
    {
        return m_dockModel.openUrl(
            QUrl::fromEncoded(appId.mid(9).toUtf8()));
    }
    return m_dockModel.activateApplication(appId);
}

// The same activation as activateDockEntry, but reporting what happened.
//
// `succeeded` means a program was started and the start was confirmed.
// `requested` means the activation was handed to the compositor, which does not
// report back: the caller must not treat it as success. `failed` means the
// entry could not be launched at all. The bool-returning activateDockEntry is
// unchanged, so existing callers keep their exact contract.
QVariantMap PanelWindow::activateDockEntryOutcome(const QString &appId)
{
    QVariantMap result;
    result.insert(QStringLiteral("appId"), appId);
    result.insert(QStringLiteral("reason"), QString{});
    if (appId.startsWith(QStringLiteral("status:")))
        return {{QStringLiteral("outcome"), QStringLiteral("failed")},
                {QStringLiteral("reason"), QStringLiteral("informational-entry")}};

    if (appId.startsWith(QStringLiteral("free-url:")))
    {
        const bool opened =
            m_dockModel.openUrl(QUrl::fromEncoded(appId.mid(9).toUtf8()));
        result.insert(QStringLiteral("outcome"),
                      opened ? QStringLiteral("succeeded")
                             : QStringLiteral("failed"));
        if (!opened)
        {
            result.insert(QStringLiteral("reason"),
                          QStringLiteral("no-url-handler"));
        }
        const auto desktop = QFileInfo(QUrl::fromEncoded(appId.mid(9).toUtf8()).toLocalFile()).fileName();
        m_overlayModel.setTemporaryStatus(desktop,
            opened ? tr("Launch request accepted") : tr("Launch request failed"));
        return result;
    }

    switch (m_dockModel.activateApplicationOutcome(appId))
    {
    case DockModel::ActivationOutcome::Launched:
        result.insert(QStringLiteral("outcome"), QStringLiteral("succeeded"));
        break;
    case DockModel::ActivationOutcome::ActivationRequested:
        result.insert(QStringLiteral("outcome"), QStringLiteral("requested"));
        result.insert(QStringLiteral("reason"),
                      QStringLiteral("activation-not-verifiable"));
        break;
    case DockModel::ActivationOutcome::Failed:
        result.insert(QStringLiteral("outcome"), QStringLiteral("failed"));
        result.insert(QStringLiteral("reason"),
                      QStringLiteral("launch-failed"));
        break;
    case DockModel::ActivationOutcome::UnknownEntry:
        result.insert(QStringLiteral("outcome"), QStringLiteral("failed"));
        result.insert(QStringLiteral("reason"),
                      QStringLiteral("unknown-entry"));
        break;
    }
    if (result.value(QStringLiteral("outcome")).toString() != QStringLiteral("requested"))
    {
        for (const auto &value : m_dockModel.panelEntries(QStringLiteral("hybrid"))) {
            const auto entry = value.toMap();
            if (entry.value(QStringLiteral("appId")).toString() != appId) continue;
            QString desktop = entry.value(QStringLiteral("desktopFileName"), appId).toString();
            if (!desktop.endsWith(QStringLiteral(".desktop"))) desktop += QStringLiteral(".desktop");
            m_overlayModel.setTemporaryStatus(QFileInfo(desktop).fileName(),
                result.value(QStringLiteral("outcome")).toString() == QStringLiteral("succeeded")
                    ? tr("Application started") : tr("Launch failed"));
        }
    }
    return result;
}

bool PanelWindow::activateDockWindow(const QString &appId, const QString &windowId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.activateApplicationWindow(appId, windowId);
}

bool PanelWindow::requestDockWindowAction(
    const QString &appId, const QString &windowId, const QString &action)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.requestApplicationWindowAction(appId, windowId, action);
}

bool PanelWindow::launchDockEntry(
    const QString &panelId, const QString &appId, const QString &desktopAction)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
    {
        return false;
    }
    for (const auto &value : dockEntriesForPanel(panelId, definition->content.type))
    {
        const auto entry = value.toMap();
        if (entry.value(QStringLiteral("appId")).toString() != appId)
        {
            continue;
        }
        QString desktopPath;
        if (ArchDock::PanelContent::isUrlEntryId(appId))
        {
            const auto url = QUrl::fromEncoded(appId.mid(9).toUtf8());
            if (url.isLocalFile())
                desktopPath = url.toLocalFile();
        }
        else
        {
            desktopPath = m_dockModel.desktopFileForApplication(appId);
        }
        return m_dockModel.launchDesktopEntry(desktopPath, desktopAction);
    }
    return false;
}

bool PanelWindow::minimizeDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.minimizeApplication(appId);
}

bool PanelWindow::closeDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.closeApplication(appId);
}

bool PanelWindow::closeAllDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("status:"))) return false;
    return m_dockModel.closeAllApplication(appId);
}

bool PanelWindow::togglePinnedDockEntry(const QString &appId)
{
    return m_dockModel.togglePinnedApplication(appId);
}

bool PanelWindow::moveDockEntryBefore(const QString &appId, const QString &beforeAppId)
{
    // Free entries are panel-specific and reorder through
    // movePanelEntryBefore; the shared application model cannot order them
    // and must never receive them.
    if (ArchDock::PanelContent::isUrlEntryId(appId) ||
        ArchDock::PanelContent::isUrlEntryId(beforeAppId))
    {
        return false;
    }
    return m_dockModel.moveApplicationBefore(appId, beforeAppId);
}

bool PanelWindow::pinDockUrl(const QString &url)
{
    return pinDockUrls({url});
}

bool PanelWindow::pinDockUrls(const QStringList &urls)
{
    bool pinnedAny = false;
    for (const QString &urlString : urls)
    {
        const QUrl url = QUrl::fromUserInput(urlString);
        if (url.isValid() && !url.isEmpty())
        {
            pinnedAny = m_dockModel.pinUrl(url) || pinnedAny;
        }
    }
    return pinnedAny;
}

bool PanelWindow::pinPanelUrls(const QString &panelId, const QStringList &urls)
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
        QStringLiteral("free"))
    {
        return pinDockUrls(urls);
    }
    return addPanelEntries(panelId, urls);
}

bool PanelWindow::removePanelContent(const QString &panelId, const QString &entryId)
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
        QStringLiteral("free"))
    {
        return false;
    }
    return removePanelEntry(panelId, entryId);
}

bool PanelWindow::commitPanelContentTransaction(
    const QString &panelId,
    const ArchDock::PanelContentRequest &request)
{
    const std::optional<ArchDock::PanelDefinition> current =
        m_panelRegistry.panelDefinition(panelId);
    if (!current.has_value())
    {
        return false;
    }
    ArchDock::PanelContentOutcome outcome;
    const std::optional<ArchDock::PanelDefinition> candidate =
        ArchDock::PanelContentTransaction::prepare(*current, request, &outcome);
    if (!outcome.success)
    {
        qWarning().noquote() << "Arch Dock panel content"
                             << ArchDock::PanelContentTransaction::operationName(
                                    request.operation)
                             << "refused for" << panelId << outcome.errorCode
                             << outcome.errorMessage;
        return false;
    }
    if (!candidate.has_value())
    {
        // Valid but nothing to change: no revision is spent.
        return true;
    }
    QString persistenceError;
    if (!m_panelRegistry.persistPanelDefinitionTransaction(
            *current, *candidate, m_settings.transactionSnapshot(), &persistenceError))
    {
        qWarning().noquote() << "Arch Dock panel content" << panelId
                             << "could not be persisted:" << persistenceError;
        return false;
    }
    m_panelRegistry.notifyPanelSettingsTransactionAdopted(false);
    return true;
}

bool PanelWindow::addPanelEntries(const QString &panelId, const QStringList &entries)
{
    ArchDock::PanelContentRequest request;
    request.operation = ArchDock::PanelContentOperation::Add;
    for (const QString &item : entries)
    {
        const QString trimmed = item.trimmed();
        if (trimmed.isEmpty())
        {
            continue;
        }
        QUrl url;
        if (ArchDock::PanelContent::isUrlEntryId(trimmed))
        {
            url = QUrl::fromEncoded(trimmed.mid(9).toUtf8());
        }
        else if (trimmed.contains(QStringLiteral("://")) ||
                 trimmed.startsWith(QLatin1Char('/')))
        {
            url = QUrl::fromUserInput(trimmed);
        }
        else
        {
            // An application id is pinned to the panel as its desktop entry,
            // so the panel owns a plain local file rather than a reference
            // into the shared model.
            const QString desktopFile = m_dockModel.desktopFileForApplication(trimmed);
            if (desktopFile.isEmpty())
            {
                continue;
            }
            url = QUrl::fromLocalFile(desktopFile);
        }
        if (!url.isLocalFile() || !QFileInfo::exists(url.toLocalFile()))
        {
            continue;
        }
        request.urls.append(url.toString());
    }
    if (request.urls.isEmpty())
    {
        return false;
    }
    return commitPanelContentTransaction(panelId, request);
}

bool PanelWindow::removePanelEntry(const QString &panelId, const QString &entryId)
{
    ArchDock::PanelContentRequest request;
    request.operation = ArchDock::PanelContentOperation::Remove;
    request.entryId = entryId;
    return commitPanelContentTransaction(panelId, request);
}

bool PanelWindow::movePanelEntryBefore(const QString &panelId,
                                       const QString &entryId,
                                       const QString &beforeEntryId)
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
        return false;
    if (definition->host.kind == ArchDock::PanelHostKind::NativeEdge)
    {
        QHash<QString, QString> owners;
        for (const auto &value : dockEntriesForPanel(panelId, definition->content.type))
        {
            const auto entry = value.toMap();
            owners.insert(entry.value(QStringLiteral("appId")).toString(),
                entry.value(QStringLiteral("segmentId")).toString());
        }
        if (!owners.contains(entryId) || (!beforeEntryId.isEmpty() &&
            (!owners.contains(beforeEntryId) || owners.value(entryId) != owners.value(beforeEntryId))))
            return false;
        return m_dockModel.moveApplicationBefore(entryId, beforeEntryId);
    }
    ArchDock::PanelContentRequest request;
    request.operation = ArchDock::PanelContentOperation::MoveBefore;
    request.entryId = entryId;
    request.beforeEntryId = beforeEntryId;
    return commitPanelContentTransaction(panelId, request);
}

bool PanelWindow::setPanelEntryOrder(const QString &panelId, const QStringList &entryIds)
{
    ArchDock::PanelContentRequest request;
    request.operation = ArchDock::PanelContentOperation::SetOrder;
    request.order = entryIds;
    return commitPanelContentTransaction(panelId, request);
}

QStringList PanelWindow::panelEntryOrder(const QString &panelId) const
{
    const std::optional<ArchDock::PanelDefinition> definition =
        m_panelRegistry.panelDefinition(panelId);
    return definition.has_value()
        ? definition->content.canonicalEntryOrder()
        : QStringList{};
}

QVariantList PanelWindow::dockFolderEntries(const QString &appId) const
{
    QVariantList entries = m_dockModel.folderEntriesForApplication(appId);
    for (QVariant &entry : entries)
    {
        QVariantMap value = entry.toMap();
        value.insert(QStringLiteral("url"), value.value(QStringLiteral("url")).toUrl().toString());
        entry = value;
    }
    return entries;
}

QUrl PanelWindow::panelFolderUrl(const QString &panelId, const QString &appId) const
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
        return {};
    for (const auto &value : dockEntriesForPanel(panelId, definition->content.type))
    {
        if (value.toMap().value(QStringLiteral("appId")).toString() != appId)
            continue;
        if (ArchDock::PanelContent::isUrlEntryId(appId))
            return QUrl::fromEncoded(ArchDock::PanelContent::urlFromEntryId(appId).toUtf8());
        return m_dockModel.urlForApplicationId(appId);
    }
    return {};
}

QVariantMap PanelWindow::panelFolderSnapshot(const QString &panelId, const QString &appId) const
{
    const QUrl folder = panelFolderUrl(panelId, appId);
    QVariantMap result = ArchDock::FolderContentModel::snapshot(folder);
    if (folder.isEmpty())
        result[QStringLiteral("errorCode")] = QStringLiteral("folder-entry-unavailable");
    result.insert(QStringLiteral("panelId"), panelId);
    result.insert(QStringLiteral("appId"), appId);
    return result;
}

QVariantMap PanelWindow::openPanelFolderChild(const QString &panelId, const QString &appId,
                                            const QString &childId)
{
    QVariantMap result{{QStringLiteral("success"), false},
                       {QStringLiteral("status"), QStringLiteral("rejected")},
                       {QStringLiteral("errorCode"), QStringLiteral("folder-entry-unavailable")}};
    const QUrl folder = panelFolderUrl(panelId, appId);
    if (folder.isEmpty())
        return result;
    QString error;
    const QUrl child = ArchDock::FolderContentModel::resolveChild(folder, childId, &error);
    if (child.isEmpty())
    {
        result[QStringLiteral("errorCode")] = error;
        return result;
    }
    auto *job = new KIO::OpenUrlJob(child, this);
    job->setRunExecutables(false);
    job->setShowOpenOrExecuteDialog(false);
    connect(job, &KJob::result, this, [](KJob *completed) {
        if (completed->error())
            qWarning() << "Arch Dock folder document open failed:" << completed->errorText();
    });
    job->start();
    // Native opening is asynchronous; acceptance is not proof of app startup.
    result[QStringLiteral("success")] = true;
    result[QStringLiteral("status")] = QStringLiteral("accepted");
    result[QStringLiteral("errorCode")] = QString{};
    return result;
}

bool PanelWindow::openDockUrl(const QString &urlString)
{
    const QUrl url = QUrl::fromUserInput(urlString);
    return url.isValid() && !url.isEmpty() && m_dockModel.openUrl(url);
}

QScreen *PanelWindow::screenForPanel(const QString &panelId) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return nullptr;
    }

    QStringList screenIds;
    screenIds.reserve(screens.size());
    for (const QScreen *screen : screens)
    {
        screenIds.append(ArchDock::persistentScreenId(screen));
    }

    const int requestedScreen = m_panelRegistry.panelValue(panelId, QStringLiteral("screen")).toInt();
    const QString requestedScreenId = m_panelRegistry.panelValue(
        panelId,
        QStringLiteral("screenId")).toString();
    return screens.at(ArchDock::resolvedScreenIndex(screenIds, requestedScreenId, requestedScreen));
}

QString PanelWindow::screenIdForIndex(int screenIndex) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return {};
    }
    return ArchDock::persistentScreenId(screens.at(qBound(0, screenIndex, screens.size() - 1)));
}

QVariantList PanelWindow::availableScreens() const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    QVariantList entries;
    entries.reserve(screens.size());
    for (int index = 0; index < screens.size(); ++index)
    {
        const QScreen *screen = screens.at(index);
        const QString outputName = screen->name().trimmed();
        const QString manufacturer = screen->manufacturer().trimmed();
        const QString model = screen->model().trimmed();
        const QString deviceName = (manufacturer + QLatin1Char(' ') + model).trimmed();
        const QString label = outputName.isEmpty()
            ? tr("Display %1").arg(index + 1)
            : deviceName.isEmpty() || deviceName == outputName
                ? outputName
                : tr("%1 (%2)").arg(outputName, deviceName);
        entries.append(QVariant::fromValue(QVariantMap{
            {QStringLiteral("index"), index},
            {QStringLiteral("id"), ArchDock::persistentScreenId(screen)},
            {QStringLiteral("label"), label}}));
    }
    return entries;
}

int PanelWindow::screenIndexForPanel(const QString &panelId) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return -1;
    }

    QStringList screenIds;
    screenIds.reserve(screens.size());
    for (const QScreen *screen : screens)
    {
        screenIds.append(ArchDock::persistentScreenId(screen));
    }
    return ArchDock::resolvedScreenIndex(
        screenIds,
        m_panelRegistry.panelValue(panelId, QStringLiteral("screenId")).toString(),
        m_panelRegistry.panelValue(panelId, QStringLiteral("screen")).toInt());
}

void PanelWindow::setPanelScreen(const QString &panelId, int screenIndex)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return;
    }

    const int boundedIndex = qBound(0, screenIndex, screens.size() - 1);
    const QVariantMap values{
        {QStringLiteral("screen"), boundedIndex},
        {QStringLiteral("screenId"), ArchDock::persistentScreenId(screens.at(boundedIndex))},
    };
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() ==
        QStringLiteral("free"))
    {
        m_panelRegistry.updatePanel(panelId, values);
        return;
    }

    const QVariantMap result = applyNativePanelPlacementDraft(
        panelId,
        {{QStringLiteral("screen"), boundedIndex}});
    if (!result.value(QStringLiteral("success")).toBool())
    {
        qWarning() << "Native Plasma panel screen transaction failed for" << panelId
                   << "status" << result.value(QStringLiteral("status")).toString()
                   << "error" << result.value(QStringLiteral("errorCode")).toString();
    }
}

bool PanelWindow::setPanelVisible(const QString &panelId, bool visible)
{
    if (!m_panelRegistry.panelIds().contains(panelId) ||
        m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() ==
            QStringLiteral("free"))
    {
        return false;
    }

    return synchronizeNativePanelVisibility(
        panelId,
        visible,
        true,
        std::nullopt,
        {{QStringLiteral("visible"), visible}});
}

void PanelWindow::setPanelVisibilityMode(const QString &panelId, const QString &visibilityMode)
{
    const QVariantMap result = applyNativePanelVisibilityMode(panelId, visibilityMode);
    if (!result.value(QStringLiteral("success")).toBool() ||
        panelId != QStringLiteral("bottom"))
    {
        return;
    }

    const bool autoHide = m_panelRegistry.panelValue(
        panelId,
        QStringLiteral("visibilityMode")).toString() == QStringLiteral("auto-hide");
    if (m_settings.autoHide() != autoHide)
    {
        m_settings.setAutoHide(autoHide);
    }
}

QVariantMap PanelWindow::applyNativePanelVisibilityMode(
    const QString &panelId,
    const QString &visibilityMode)
{
    const bool nativePanel = m_panelRegistry.panelIds().contains(panelId) &&
        isNativeDockPanelEdge(
            m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString());
    const ArchDock::NativeVisibilityCapabilities capabilities =
        nativeVisibilityCapabilities(panelId);
    const std::optional<ArchDock::PanelVisibilityMode> requestedMode =
        ArchDock::normalizedPanelVisibilityMode(visibilityMode);
    if (!nativePanel || !requestedMode.has_value())
    {
        QVariantMap result{
            {QStringLiteral("success"), false},
            {QStringLiteral("verified"), false},
            {QStringLiteral("status"),
             nativePanel ? QStringLiteral("failed") : QStringLiteral("unsupported")},
            {QStringLiteral("errorCode"),
             nativePanel
                 ? QStringLiteral("invalid-visibility-mode")
                 : QStringLiteral("visibility-unsupported-host")},
            {QStringLiteral("requestedMode"), visibilityMode},
            {QStringLiteral("effectiveMode"), QString{}},
            {QStringLiteral("hostMode"), QString{}},
            {QStringLiteral("supportedModes"),
             nativePanel
                 ? ArchDock::supportedNativeVisibilityModes(capabilities)
                 : QStringList{}},
            {QStringLiteral("watcherAvailable"), m_windowWatcher.available()},
            {QStringLiteral("fallbackAttempted"), false},
            {QStringLiteral("fallbackApplied"), false},
            {QStringLiteral("fallbackReason"), QString{}},
            {QStringLiteral("fallbackErrorCode"), QString{}},
            {QStringLiteral("ownershipVerified"), false},
            {QStringLiteral("rollbackAttempted"), false},
            {QStringLiteral("rollbackSucceeded"), false},
            {QStringLiteral("rollbackErrorCode"), QString{}},
        };
        recordNativePanelVisibilityResult(panelId, std::move(result));
        return nativePanelVisibilityStatus(panelId);
    }

    const QString canonicalMode = ArchDock::panelVisibilityModeToString(*requestedMode);
    synchronizeNativePanelVisibility(
        panelId,
        m_panelRegistry.panelValue(
            panelId, QStringLiteral("visible")).toBool(),
        true,
        requestedMode,
        {{QStringLiteral("visibilityMode"), canonicalMode}});
    return nativePanelVisibilityStatus(panelId);
}

QList<ArchDock::EdgePanel> PanelWindow::edgePanels() const
{
    QList<ArchDock::EdgePanel> panels;
    const QStringList panelIds = m_panelRegistry.panelIds();
    panels.reserve(panelIds.size());
    for (const QString &panelId : panelIds)
    {
        const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
        if (!isNativeDockPanelEdge(edge))
        {
            continue;
        }
        const bool vertical = edge == QStringLiteral("left") || edge == QStringLiteral("right");
        panels.append({panelId,
                       edge,
                       screenIndexForPanel(panelId),
                       m_panelRegistry.panelValue(
                           panelId,
                           vertical ? QStringLiteral("width") : QStringLiteral("height")).toInt(),
                       m_panelRegistry.panelValue(panelId, QStringLiteral("visible")).toBool()});
    }
    return panels;
}

ArchDock::NativeVisibilityCapabilities PanelWindow::nativeVisibilityCapabilities(
    const QString &panelId) const
{
    if (!m_panelRegistry.panelIds().contains(panelId) ||
        !isNativeDockPanelEdge(
            m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString()))
    {
        return {};
    }

    return {true, true, m_windowWatcher.available()};
}

ArchDock::PanelVisibilityDecision PanelWindow::nativePanelVisibilityDecision(
    const QString &panelId,
    ArchDock::PanelVisibilityMode mode,
    bool visible) const
{
    if (!m_panelRegistry.panelIds().contains(panelId) ||
        !isNativeDockPanelEdge(
            m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString()))
    {
        return ArchDock::PanelVisibilityDecision::Reveal;
    }

    QScreen *screen = screenForPanel(panelId);
    const int screenIndex = screenIndexForPanel(panelId);
    const ArchDock::NativePanelPlacementResult placementResult =
        normalizedNativePanelPlacement(panelId);
    if (!screen || screenIndex < 0 || !placementResult.isValid() ||
        !placementResult.placement.has_value())
    {
        return ArchDock::PanelVisibilityDecision::Reveal;
    }

    ArchDock::PanelVisibilityInput input;
    input.mode = mode;
    input.panelGeometry = panelGeometryForVisibility(
        screen->geometry(), *placementResult.placement);
    input.panelScreenIndex = screenIndex;
    input.manualHideRequested = !visible;
    // A panel the user is actively using may not be concealed. The applet is
    // the only thing that knows a menu is open or a drag is in flight, so its
    // last reported guards are the input here.
    input.locks = m_panelInteractionGuards.value(panelId);
    input.windows.reserve(m_windowModel.windows().size());
    for (const WindowItem &window : m_windowModel.windows())
    {
        input.windows.append({window.frameGeometry,
                              window.screenIndex,
                              window.active,
                              window.minimized,
                              window.maximized,
                              window.fullScreen});
    }
    return ArchDock::decidePanelVisibility(input);
}

bool PanelWindow::reportPanelInteractionGuards(const QString &panelId,
                                              const QVariantMap &guards)
{
    if (!runtimePanelDefinition(panelId))
    {
        return false;
    }

    ArchDock::PanelVisibilityLocks locks;
    locks.pointerInside = guards.value(QStringLiteral("pointerInside")).toBool();
    locks.revealZoneActive =
        guards.value(QStringLiteral("revealZoneActive")).toBool();
    locks.popupOpen = guards.value(QStringLiteral("popupOpen")).toBool();
    locks.dragActive = guards.value(QStringLiteral("dragActive")).toBool();
    locks.keyboardFocus = guards.value(QStringLiteral("keyboardFocus")).toBool();
    locks.editMode = guards.value(QStringLiteral("editMode")).toBool();

    const auto existing = m_panelInteractionGuards.constFind(panelId);
    if (existing != m_panelInteractionGuards.cend() && *existing == locks)
    {
        return true;
    }
    m_panelInteractionGuards.insert(panelId, locks);
    // A guard that has just been raised or cleared can change whether the host
    // should be showing the panel, so the decision is announced rather than
    // waiting for the next unrelated change. Only the revision is published:
    // reporting a guard must not trigger a recovery sweep.
    ++m_visibilityRevision;
    emit visibilityRevisionChanged();
    return true;
}

QVariantMap PanelWindow::panelInteractionGuards(const QString &panelId) const
{
    const ArchDock::PanelVisibilityLocks locks =
        m_panelInteractionGuards.value(panelId);
    return {
        {QStringLiteral("pointerInside"), locks.pointerInside},
        {QStringLiteral("revealZoneActive"), locks.revealZoneActive},
        {QStringLiteral("popupOpen"), locks.popupOpen},
        {QStringLiteral("dragActive"), locks.dragActive},
        {QStringLiteral("keyboardFocus"), locks.keyboardFocus},
        {QStringLiteral("editMode"), locks.editMode},
    };
}

bool PanelWindow::reportPanelPresentationState(const QString &panelId,
                                              const QVariantMap &state)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    static const QSet<QString> surfaceStates{
        QStringLiteral("open"), QStringLiteral("collapsed")};
    static const QSet<QString> transitionStates{
        QStringLiteral("idle"), QStringLiteral("opening"), QStringLiteral("closing")};
    static const QSet<QString> hostPhases{
        QStringLiteral("revealed"), QStringLiteral("concealing"),
        QStringLiteral("concealed"), QStringLiteral("revealing")};

    const QString surfaceState = state.value(
        QStringLiteral("surfaceState")).toString().trimmed().toLower();
    const QString transitionState = state.value(
        QStringLiteral("transitionState"),
        QStringLiteral("idle")).toString().trimmed().toLower();
    const QString hostPhase = state.value(
        QStringLiteral("hostPhase"),
        QStringLiteral("revealed")).toString().trimmed().toLower();
    if (!surfaceStates.contains(surfaceState) ||
        !transitionStates.contains(transitionState) ||
        !hostPhases.contains(hostPhase))
    {
        return false;
    }

    const QVariantMap normalized{
        {QStringLiteral("reported"), true},
        {QStringLiteral("surfaceState"), surfaceState},
        {QStringLiteral("transitionState"), transitionState},
        {QStringLiteral("hostPhase"), hostPhase},
    };
    const auto existing = m_panelPresentationStates.constFind(panelId);
    if (existing != m_panelPresentationStates.cend() && *existing == normalized)
    {
        return true;
    }
    m_panelPresentationStates.insert(panelId, normalized);
    updateContentDemand();
    return true;
}

QVariantMap PanelWindow::panelPresentationState(const QString &panelId) const
{
    const auto existing = m_panelPresentationStates.constFind(panelId);
    if (existing != m_panelPresentationStates.cend())
    {
        return *existing;
    }
    return {
        {QStringLiteral("reported"), false},
        {QStringLiteral("surfaceState"), QString{}},
        {QStringLiteral("transitionState"), QString{}},
        {QStringLiteral("hostPhase"), QString{}},
    };
}

bool PanelWindow::requestPanelPresentation(const QString &panelId,
                                          const QString &request)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }
    const QString normalized = request.trimmed().toLower();
    if (normalized != QStringLiteral("open") &&
        normalized != QStringLiteral("collapse"))
    {
        return false;
    }

    m_pendingPresentationRequests.insert(panelId, normalized);
    ++m_presentationRequestRevision;
    emit presentationRequestRevisionChanged();

    QDBusMessage propertiesChanged = QDBusMessage::createSignal(
        QStringLiteral("/Control"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    propertiesChanged << QStringLiteral("local.PanelWindow")
                      << QVariantMap{{QStringLiteral("presentationRequestRevision"),
                                      m_presentationRequestRevision}}
                      << QStringList{};
    QDBusConnection::sessionBus().send(propertiesChanged);
    return true;
}

QVariantMap PanelWindow::takePanelPresentationRequest(const QString &panelId)
{
    const QString request = m_pendingPresentationRequests.take(panelId);
    return {
        {QStringLiteral("pending"), !request.isEmpty()},
        {QStringLiteral("request"), request},
    };
}

bool PanelWindow::shouldConcealPanel(const QString &panelId) const
{
    const std::optional<ArchDock::PanelVisibilityMode> mode =
        ArchDock::normalizedPanelVisibilityMode(
            m_panelRegistry.panelValue(
                panelId, QStringLiteral("visibilityMode")).toString());
    if (!mode.has_value())
    {
        return false;
    }
    return nativePanelVisibilityDecision(
               panelId,
               *mode,
               m_panelRegistry.panelValue(
                   panelId, QStringLiteral("visible")).toBool()) ==
        ArchDock::PanelVisibilityDecision::Conceal;
}

void PanelWindow::synchronizeScreenAssignments()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return;
    }

    QStringList screenIds;
    screenIds.reserve(screens.size());
    for (const QScreen *screen : screens)
    {
        screenIds.append(ArchDock::persistentScreenId(screen));
    }

    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        const int storedIndex = m_panelRegistry.panelValue(panelId, QStringLiteral("screen")).toInt();
        const QString storedId = m_panelRegistry.panelValue(panelId, QStringLiteral("screenId")).toString();
        const ArchDock::ScreenResolution resolution = ArchDock::resolveScreen(
            screenIds, storedId, storedIndex);
        const int resolvedIndex = resolution.index;
        if (resolution.usedFallback)
        {
            qInfo() << "Arch Dock screen fallback for" << panelId
                    << "requested stable id" << storedId
                    << "requested index" << storedIndex
                    << "resolved stable id" << screenIds.at(resolvedIndex)
                    << "resolved index" << resolvedIndex
                    << "reason" << screenResolutionReasonName(resolution.reason);
        }
        QVariantMap updates;
        if (storedIndex != resolvedIndex)
        {
            updates.insert(QStringLiteral("screen"), resolvedIndex);
        }
        if (storedId.isEmpty() && !screenIds.at(resolvedIndex).isEmpty())
        {
            updates.insert(QStringLiteral("screenId"), screenIds.at(resolvedIndex));
        }
        if (!updates.isEmpty())
        {
            m_panelRegistry.updatePanel(panelId, updates);
        }
    }
}

void PanelWindow::handleScreensChanged()
{
    synchronizeScreenAssignments();
    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        if (nativePanelId(panelId) >= 0 && !synchronizeNativePanelPlacement(panelId))
        {
            qWarning() << "Could not update the native Plasma panel placement for" << panelId;
        }
    }
    ++m_screenRevision;
    emit screenRevisionChanged();
    updateDesktopSuite();
}

void PanelWindow::scheduleNativePanelRecovery()
{
    const int generation = ++m_nativePanelRecoveryGeneration;
    constexpr std::array<int, 3> recoveryDelays{500, 2000, 5000};
    for (int index = 0; index < static_cast<int>(recoveryDelays.size()); ++index)
    {
        QTimer::singleShot(
            recoveryDelays.at(index),
            this,
            [this, generation, finalAttempt = index == static_cast<int>(recoveryDelays.size()) - 1]
            {
                if (generation != m_nativePanelRecoveryGeneration)
                {
                    return;
                }

                recoverNativePanels(finalAttempt);
                if (finalAttempt)
                {
                    emit nativePanelRecoveryFinished();
                }
            });
    }
}

void PanelWindow::recoverNativePanels(bool allowMissingHostRecovery)
{
    QDBusInterface plasmaShell(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"),
        QDBusConnection::sessionBus());
    if (!plasmaShell.isValid())
    {
        return;
    }

    if (m_presetAudition && m_presetAudition->state() == QStringLiteral("BLOCKED") &&
        !m_presetAudition->recoverInterruptedPreview().value(QStringLiteral("success")).toBool())
    {
        qWarning() << "Preset preview recovery remains BLOCKED:" << m_presetAudition->status();
        return;
    }

    m_nativePanelRecoveryActive = true;
    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        if (m_presetAudition && m_presetAudition->previewDefinition(panelId)) continue;
        const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
        if (edge == QStringLiteral("free"))
        {
            continue;
        }
        const bool visible = m_panelRegistry.panelValue(panelId, QStringLiteral("visible")).toBool();
        if (!synchronizeNativePanelVisibility(
                panelId, visible, allowMissingHostRecovery))
        {
            qWarning() << "Could not synchronize the native Plasma panel for" << panelId;
        }
        else if (!synchronizeNativePanelPlacement(panelId))
        {
            qWarning() << "Could not synchronize native Plasma panel placement for" << panelId;
        }
    }
    m_nativePanelRecoveryActive = false;
    synchronizeFreePanels();
}

void PanelWindow::updateDesktopSuite()
{
    synchronizeFreePanels();
    scheduleNativePanelRecovery();
    ++m_visibilityRevision;
    emit visibilityRevisionChanged();
}

void PanelWindow::synchronizeFreePanels()
{
    ArchDock::FreePanelController controller(
        m_panelRegistry, freePanelHostOperations());
    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
            QStringLiteral("free"))
        {
            continue;
        }

        const ArchDock::FreePanelLifecycleResult result = controller.synchronize(panelId);
        if (!result.success)
        {
            qWarning() << "Could not synchronize the free-panel Plasma host for"
                       << panelId << result.errorCode;
        }
    }
}

void PanelWindow::syncRegistryFromLegacySettings()
{
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

void PanelWindow::showSettings()
{
    if (!m_settingsWindow)
    {
        m_settingsWindow = createUtilityWindow(
            QUrl(QStringLiteral("qrc:/qt/qml/ArchDock/qml/runtime/SettingsPopup.qml")));
        if (m_settingsWindow) connect(m_settingsWindow, &QWindow::visibleChanged,
                                     this, &PanelWindow::updateContentDemand);
    }

    presentUtilityWindow(m_settingsWindow);
}

void PanelWindow::showPanelSettings(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    m_panelRegistry.setActivePanelId(panelId);
    if (!m_settingsWindow)
    {
        m_settingsWindow = createUtilityWindow(
            QUrl(QStringLiteral("qrc:/qt/qml/ArchDock/qml/runtime/SettingsPopup.qml")));
        if (m_settingsWindow) connect(m_settingsWindow, &QWindow::visibleChanged,
                                     this, &PanelWindow::updateContentDemand);
    }
    if (m_settingsWindow)
    {
        m_settingsWindow->setProperty("selectedPanelId", panelId);
        m_settingsWindow->setProperty("mainTabIndex", 1);
        m_settingsWindow->setProperty("subTabIndex", 0);
    }
    presentUtilityWindow(m_settingsWindow);
}

QString PanelWindow::createNativePanel(const QString &edge, const QString &type)
{
    const QString normalizedEdge = edge.trimmed().toLower();
    const QString normalizedType = type.trimmed().toLower();
    if (!isNativeDockPanelEdge(normalizedEdge) || !isNativeDockPanelType(normalizedType))
    {
        return {};
    }

    const QString panelId = m_panelRegistry.addPanel(normalizedEdge, normalizedType);
    if (panelId.isEmpty())
    {
        return {};
    }

    if (createNativeKdePanel(panelId))
    {
        QString defaultError;
        if (!applyPresetDefaultsToNewPanel(panelId, &defaultError))
            qWarning() << "Could not apply preset defaults to the new native panel" << panelId << defaultError;
        return panelId;
    }

    m_panelRegistry.removePanel(panelId);
    return {};
}

QVariantMap PanelWindow::createFreePanel()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    ArchDock::FreePanelCreationRequest request;
    request.origin = ArchDock::FreePanelCreationOrigin::Studio;
    request.screenIndex = screens.isEmpty()
        ? -1
        : qBound(0, m_settings.monitorIndex(), screens.size() - 1);

    const ArchDock::FreePanelCreationResult result = createFreePanelTransaction(request);
    if (result.success)
    {
        showPanelSettings(result.panelId);
    }
    else
    {
        qWarning() << "Could not create the free-panel Plasma desktop host:"
                   << result.errorCode << result.failureStage
                   << result.rollbackErrorCode;
    }
    return result.toVariantMap();
}

QVariantMap PanelWindow::createFreePanelFromTemplate(
    int containmentId,
    const QString &ownershipToken)
{
    ArchDock::FreePanelCreationRequest request;
    request.origin = ArchDock::FreePanelCreationOrigin::TemplateBridge;
    request.bridgeContainmentId = containmentId;
    request.ownershipToken = ownershipToken;

    const ArchDock::FreePanelCreationResult result = createFreePanelTransaction(request);
    if (result.success)
    {
        showPanelSettings(result.panelId);
    }
    else
    {
        qWarning() << "Could not complete the free-panel template transaction:"
                   << result.errorCode << result.failureStage
                   << result.rollbackErrorCode;
    }
    return result.toVariantMap();
}

QVariantMap PanelWindow::adoptFreePanelApplet(
    int desktopContainmentId,
    int dockAppletId)
{
    ArchDock::FreePanelCreationRequest request;
    request.origin = ArchDock::FreePanelCreationOrigin::ExistingApplet;
    request.existingDesktopContainmentId = desktopContainmentId;
    request.existingDockAppletId = dockAppletId;

    const ArchDock::FreePanelCreationResult result = createFreePanelTransaction(request);
    if (result.success)
    {
        showPanelSettings(result.panelId);
    }
    else
    {
        qWarning() << "Could not adopt the requesting free-panel desktop applet:"
                   << result.errorCode << result.failureStage
                   << result.rollbackErrorCode;
    }
    return result.toVariantMap();
}

ArchDock::FreePanelCreationResult PanelWindow::createFreePanelTransaction(
    const ArchDock::FreePanelCreationRequest &request)
{
    ArchDock::FreePanelController controller(
        m_panelRegistry, freePanelHostOperations());
    const auto result = controller.create(request);
    if (result.success && result.status != QStringLiteral("existing"))
    {
        QString defaultError;
        if (!applyPresetDefaultsToNewPanel(result.panelId, &defaultError))
            qWarning() << "Could not apply preset defaults to the new free panel" << result.panelId << defaultError;
    }
    return result;
}

bool PanelWindow::applyPresetDefaultsToNewPanel(const QString &panelId, QString *errorCode)
{
    using namespace ArchDock;
    const PresetDefaultStore store;
    const auto defaults = store.load(errorCode);
    if (!defaults) return false;
    if (defaults->panelPresetId.isEmpty() && defaults->iconPresetId.isEmpty()) return true;
    const bool panel = !defaults->panelPresetId.isEmpty();
    const auto request = QVariantMap{{QStringLiteral("kind"), panel ? QStringLiteral("panel") : QStringLiteral("icon")},
        {QStringLiteral("presetId"), panel ? defaults->panelPresetId : defaults->iconPresetId},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("useRecommendedIcons"), defaults->iconPresetId.isEmpty()}};
    auto prepared = preparePresetPreview(request, errorCode);
    if (!prepared) return false;
    if (prepared->record.temporary)
    {
        if (errorCode) *errorCode = QStringLiteral("new-panel-default-host-incompatible");
        return false;
    }
    if (panel && !defaults->iconPresetId.isEmpty())
    {
        const auto icons = preparePresetPreview({{QStringLiteral("kind"), QStringLiteral("icon")},
            {QStringLiteral("presetId"), defaults->iconPresetId}, {QStringLiteral("panelId"), panelId}}, errorCode);
        if (!icons || !icons->iconPreset) return false;
        prepared->recommendedIcons = icons->iconPreset;
        const auto draft = PresetApplication::preparePanel(prepared->draft.previousPanel, prepared->draft.previousGlobals,
            *prepared->panelPreset, {}, prepared->recommendedIcons, errorCode);
        if (!draft) return false;
        prepared->draft = *draft;
    }
    const auto operations = presetAuditionOperations();
    if (!operations.validateDraft(*prepared, errorCode)) return false;
    return operations.commitExisting(prepared->draft, errorCode);
}

ArchDock::FreePanelController::HostOperations PanelWindow::freePanelHostOperations() const
{
    ArchDock::FreePanelController::HostOperations operations;
    operations.verifiedBridgeScreen = [this](int containmentId, const QString &token)
    {
        return verifiedFreeTemplateBridgeScreen(containmentId, token);
    };
    operations.matchingHostCount = [this](const QString &panelId, const QString &token)
    {
        return freePanelHostMatchCount(panelId, token);
    };
    operations.discoverOwnedHost = [this](const QString &panelId, const QString &token)
    {
        return discoverOwnedFreePanelHost(panelId, token);
    };
    operations.createConfiguredHost = [this](
        int screenIndex,
        const QString &panelId,
        const QString &token)
    {
        return createConfiguredFreePanelHost(screenIndex, panelId, token);
    };
    operations.configureAdoptedHost = [this](
        int containmentId,
        int appletId,
        const QString &panelId,
        const QString &token)
    {
        return configureAdoptedFreePanelHost(containmentId, appletId, panelId, token);
    };
    operations.verifyHost = [this](
        int containmentId,
        int appletId,
        const QString &panelId,
        const QString &token)
    {
        return freePanelHostVerification(containmentId, appletId, panelId, token);
    };
    operations.removeOwnedHost = [this](
        int containmentId,
        int appletId,
        const QString &panelId,
        const QString &token)
    {
        return removeOwnedFreePanelHost(containmentId, appletId, panelId, token);
    };
    operations.removeOwnedHostByIdentity = [this](
        const QString &panelId,
        const QString &token)
    {
        return removeOwnedFreePanelHostByIdentity(panelId, token);
    };
    operations.removeVerifiedBridge = [this](int containmentId, const QString &token)
    {
        return removeVerifiedFreeTemplateBridge(containmentId, token);
    };
    operations.screenIdForIndex = [this](int screenIndex)
    {
        return screenIdForIndex(screenIndex);
    };
    return operations;
}

std::optional<int> PanelWindow::verifiedFreeTemplateBridgeScreen(
    int containmentId,
    const QString &ownershipToken) const
{
    if (containmentId < 0 || ownershipToken.isEmpty())
    {
        return std::nullopt;
    }

    const QString script = QStringLiteral(R"JS(
const bridgePanel = panelById(%1);
let verifiedScreen = -1;
let matches = 0;
if (bridgePanel && Number(bridgePanel.id) === %1) {
    const controls = bridgePanel.widgets("org.archdock.control");
    for (let index = 0; index < controls.length; ++index) {
        const control = bridgePanel.widgetById(controls[index].id);
        if (!control || control.type !== "org.archdock.control")
            continue;
        control.currentConfigGroup = ["General"];
        if (String(control.readConfig("bootstrapAction", "")) ===
                "create-circular-free-panel" &&
            String(control.readConfig("bootstrapToken", "")) === %2 &&
            Number(control.readConfig("bootstrapPanelId", -1)) === %1) {
            const screen = Number(bridgePanel.screen);
            if (screen >= 0) {
                verifiedScreen = screen;
                ++matches;
            }
        }
    }
}
if (matches !== 1)
    verifiedScreen = -1;
print("ARCHDOCK_RESULT:" + String(verifiedScreen + 1));
)JS")
                               .arg(containmentId)
                               .arg(plasmaScriptStringLiteral(ownershipToken));
    const std::optional<int> marker = evaluatePlasmaScriptResultOptional(script);
    if (!marker.has_value() || *marker < 1)
    {
        return std::nullopt;
    }
    return *marker - 1;
}

std::optional<int> PanelWindow::freePanelHostMatchCount(
    const QString &panelId,
    const QString &ownershipToken) const
{
    if (panelId.isEmpty() || ownershipToken.isEmpty())
    {
        return std::nullopt;
    }

    const QString script = QStringLiteral(R"JS(
let matches = 0;
const allDesktops = desktops();
for (let desktopIndex = 0; desktopIndex < allDesktops.length; ++desktopIndex) {
    const desktop = desktopById(Number(allDesktops[desktopIndex].id));
    if (!desktop)
        continue;
    const docks = desktop.widgets("org.archdock.dock");
    for (let dockIndex = 0; dockIndex < docks.length; ++dockIndex) {
        const dock = desktop.widgetById(Number(docks[dockIndex].id));
        if (!dock || dock.type !== "org.archdock.dock")
            continue;
        dock.currentConfigGroup = ["General"];
        const bootstrap = String(dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
        if (String(dock.readConfig("panelId", "")) === %1 &&
            String(dock.readConfig("panelType", "")) === "empty" &&
            String(dock.readConfig("ownerToken", "")) === %2 &&
            (bootstrap === "false" || bootstrap === "0")) {
            ++matches;
        }
    }
}
print("ARCHDOCK_RESULT:" + String(matches));
)JS")
                               .arg(plasmaScriptStringLiteral(panelId))
                               .arg(plasmaScriptStringLiteral(ownershipToken));
    const std::optional<int> result = evaluatePlasmaScriptResultOptional(script);
    return result.has_value() && *result >= 0 ? result : std::nullopt;
}

ArchDock::FreePanelHostDiscoveryResult PanelWindow::discoverOwnedFreePanelHost(
    const QString &panelId,
    const QString &ownershipToken) const
{
    ArchDock::FreePanelHostDiscoveryResult result;
    if (panelId.isEmpty() || ownershipToken.isEmpty())
    {
        return result;
    }

    const QString script = QStringLiteral(R"JS(
const matches = [];
const allDesktops = desktops();
for (let desktopIndex = 0; desktopIndex < allDesktops.length; ++desktopIndex) {
    const desktop = desktopById(Number(allDesktops[desktopIndex].id));
    if (!desktop)
        continue;
    const docks = desktop.widgets("org.archdock.dock");
    for (let dockIndex = 0; dockIndex < docks.length; ++dockIndex) {
        const dock = desktop.widgetById(Number(docks[dockIndex].id));
        if (!dock || dock.type !== "org.archdock.dock")
            continue;
        dock.currentConfigGroup = ["General"];
        const bootstrap = String(dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
        if (String(dock.readConfig("panelId", "")) === %1 &&
            String(dock.readConfig("panelType", "")) === "empty" &&
            String(dock.readConfig("ownerToken", "")) === %2 &&
            (bootstrap === "false" || bootstrap === "0")) {
            matches.push({
                desktopContainmentId: Number(desktop.id),
                dockAppletId: Number(dock.id),
                screenIndex: Number(desktop.screen)
            });
        }
    }
}
let payload = { outcome: matches.length === 0 ? "missing" : "conflict" };
if (matches.length === 1) {
    payload = {
        outcome: "unique",
        desktopContainmentId: matches[0].desktopContainmentId,
        dockAppletId: matches[0].dockAppletId,
        screenIndex: matches[0].screenIndex
    };
}
print("ARCHDOCK_FREE_HOST_RESULT:" + JSON.stringify(payload));
)JS")
                               .arg(plasmaScriptStringLiteral(panelId))
                               .arg(plasmaScriptStringLiteral(ownershipToken));

    QDBusInterface shell(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"),
        QDBusConnection::sessionBus());
    if (!shell.isValid())
    {
        return result;
    }

    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
    if (!reply.isValid())
    {
        qWarning() << "Plasma free-host discovery script failed:"
                   << reply.error().message();
        return result;
    }

    static const QRegularExpression pattern(
        QStringLiteral("\\AARCHDOCK_FREE_HOST_RESULT:(\\{.*\\})\\z"));
    const QRegularExpressionMatch match = pattern.match(reply.value().trimmed());
    if (!match.hasMatch())
    {
        qWarning() << "Plasma free-host discovery returned an unverified result:"
                   << reply.value().trimmed();
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        match.captured(1).toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return result;
    }

    const QJsonObject payload = document.object();
    const QString outcome = payload.value(QStringLiteral("outcome")).toString();
    if (outcome == QStringLiteral("missing"))
    {
        result.outcome = ArchDock::FreePanelHostDiscoveryOutcome::Missing;
        return result;
    }
    if (outcome == QStringLiteral("conflict"))
    {
        result.outcome = ArchDock::FreePanelHostDiscoveryOutcome::Conflict;
        return result;
    }
    if (outcome != QStringLiteral("unique"))
    {
        return result;
    }

    const int desktopContainmentId = payload.value(
        QStringLiteral("desktopContainmentId")).toInt(-1);
    const int dockAppletId = payload.value(QStringLiteral("dockAppletId")).toInt(-1);
    const int screenIndex = payload.value(QStringLiteral("screenIndex")).toInt(-1);
    if (desktopContainmentId < 0 || dockAppletId < 0)
    {
        return result;
    }

    result.outcome = ArchDock::FreePanelHostDiscoveryOutcome::Unique;
    result.host = {desktopContainmentId, dockAppletId};
    result.screenIndex = screenIndex;
    return result;
}

ArchDock::FreePanelHostMutationResult PanelWindow::createConfiguredFreePanelHost(
    int screenIndex,
    const QString &panelId,
    const QString &ownershipToken) const
{
    ArchDock::FreePanelHostMutationResult result;
    result.screenIndex = screenIndex;
    if (screenIndex < 0 || panelId.isEmpty() || ownershipToken.isEmpty())
    {
        result.outcome = ArchDock::FreePanelHostMutationOutcome::NoHost;
        return result;
    }

    const std::optional<int> desktopContainmentId = evaluatePlasmaScriptResultOptional(
        QStringLiteral(
            "const desktop = desktopForScreen(%1); "
            "print('ARCHDOCK_RESULT:' + String(desktop ? Number(desktop.id) : -1));")
            .arg(screenIndex));
    if (!desktopContainmentId.has_value())
    {
        return result;
    }
    if (*desktopContainmentId < 0)
    {
        result.outcome = ArchDock::FreePanelHostMutationOutcome::NoHost;
        return result;
    }
    result.host.desktopContainmentId = *desktopContainmentId;

    const QString panel = plasmaScriptStringLiteral(panelId);
    const QString token = plasmaScriptStringLiteral(ownershipToken);
    const QString script = QStringLiteral(R"JS(
const desktop = desktopForScreen(%1);
let marker = 0;
if (desktop && Number(desktop.id) === %2) {
    const size = Math.round(gridUnit * 22);
    let dock = null;
    try {
        dock = desktop.addWidget(
            "org.archdock.dock",
            Math.round(gridUnit * 9),
            Math.round(gridUnit * 7),
            size,
            size);
    } catch (error) {
        dock = null;
    }
    if (dock) {
        const candidateId = Number(dock.id);
        if (candidateId < 0) {
            marker = -2147483648;
        }
        try {
            dock.currentConfigGroup = ["General"];
            dock.writeConfig("ownerToken", %4);
            dock.writeConfig("panelId", %3);
            dock.writeConfig("panelType", "empty");
            dock.writeConfig("bootstrapFreeDock", false);
            dock.reloadConfig();
            const bootstrap = String(dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
            if (candidateId >= 0 && dock.type === "org.archdock.dock" &&
                String(dock.readConfig("panelId", "")) === %3 &&
                String(dock.readConfig("panelType", "")) === "empty" &&
                String(dock.readConfig("ownerToken", "")) === %4 &&
                (bootstrap === "false" || bootstrap === "0")) {
                marker = candidateId + 1;
            }
        } catch (error) {
        }

        if (marker <= 0 && candidateId >= 0) {
            const currentDock = desktop.widgetById(candidateId);
            let transactionOwned = false;
            if (currentDock && currentDock.type === "org.archdock.dock") {
                currentDock.currentConfigGroup = ["General"];
                transactionOwned =
                    String(currentDock.readConfig("panelId", "")) === %3 &&
                    String(currentDock.readConfig("ownerToken", "")) === %4;
            }
            if (transactionOwned) {
                try {
                    currentDock.remove();
                } catch (error) {
                }
            }
            marker = desktop.widgetById(candidateId) ? -(candidateId + 1) : 0;
        }
    }
}
print("ARCHDOCK_RESULT:" + String(marker));
)JS")
                               .arg(screenIndex)
                               .arg(*desktopContainmentId)
                               .arg(panel)
                               .arg(token);
    const std::optional<int> marker = evaluatePlasmaScriptResultOptional(script);
    if (!marker.has_value() || *marker == std::numeric_limits<int>::min())
    {
        return result;
    }
    if (*marker == 0)
    {
        result.outcome = ArchDock::FreePanelHostMutationOutcome::NoHost;
        return result;
    }
    if (*marker > 0)
    {
        result.outcome = ArchDock::FreePanelHostMutationOutcome::Verified;
        result.host.dockAppletId = *marker - 1;
        return result;
    }
    result.outcome = ArchDock::FreePanelHostMutationOutcome::CandidateUnverified;
    result.host.dockAppletId = -*marker - 1;
    return result;
}

ArchDock::FreePanelHostMutationResult PanelWindow::configureAdoptedFreePanelHost(
    int desktopContainmentId,
    int dockAppletId,
    const QString &panelId,
    const QString &ownershipToken) const
{
    ArchDock::FreePanelHostMutationResult result;
    result.host = {desktopContainmentId, dockAppletId};
    if (desktopContainmentId < 0 || dockAppletId < 0 || panelId.isEmpty() ||
        ownershipToken.isEmpty())
    {
        result.outcome = ArchDock::FreePanelHostMutationOutcome::NoHost;
        return result;
    }

    const QString panel = plasmaScriptStringLiteral(panelId);
    const QString token = plasmaScriptStringLiteral(ownershipToken);
    const QString script = QStringLiteral(R"JS(
const desktop = desktopById(%1);
const dock = desktop ? desktop.widgetById(%2) : null;
let marker = 0;
if (desktop && Number(desktop.id) === %1 && dock && Number(dock.id) === %2 &&
    dock.type === "org.archdock.dock") {
    dock.currentConfigGroup = ["General"];
    const originalPanelId = String(dock.readConfig("panelId", ""));
    const originalPanelType = String(dock.readConfig("panelType", ""));
    const originalOwnerToken = String(dock.readConfig("ownerToken", ""));
    const originalBootstrap = String(
        dock.readConfig("bootstrapFreeDock", false)).toLowerCase();
    if (originalPanelId === "" && originalOwnerToken === "" &&
        (originalBootstrap === "true" || originalBootstrap === "1")) {
        const screen = Number(desktop.screen);
        if (screen >= 0) {
            try {
                dock.writeConfig("ownerToken", %4);
                dock.writeConfig("panelId", %3);
                dock.writeConfig("panelType", "empty");
                dock.writeConfig("bootstrapFreeDock", false);
                dock.reloadConfig();
                const bootstrap = String(
                    dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
                if (String(dock.readConfig("panelId", "")) === %3 &&
                    String(dock.readConfig("panelType", "")) === "empty" &&
                    String(dock.readConfig("ownerToken", "")) === %4 &&
                    (bootstrap === "false" || bootstrap === "0")) {
                    marker = screen + 1;
                }
            } catch (error) {
            }

            if (marker === 0) {
                try {
                    dock.writeConfig("panelId", originalPanelId);
                    dock.writeConfig("panelType", originalPanelType);
                    dock.writeConfig("ownerToken", originalOwnerToken);
                    dock.writeConfig(
                        "bootstrapFreeDock",
                        originalBootstrap === "true" || originalBootstrap === "1");
                    dock.reloadConfig();
                } catch (error) {
                }
                const restoredBootstrap = String(
                    dock.readConfig("bootstrapFreeDock", false)).toLowerCase();
                const restored =
                    String(dock.readConfig("panelId", "")) === originalPanelId &&
                    String(dock.readConfig("panelType", "")) === originalPanelType &&
                    String(dock.readConfig("ownerToken", "")) === originalOwnerToken &&
                    restoredBootstrap === originalBootstrap;
                marker = restored ? 0 : -1;
            }
        }
    }
}
print("ARCHDOCK_RESULT:" + String(marker));
)JS")
                               .arg(desktopContainmentId)
                               .arg(dockAppletId)
                               .arg(panel)
                               .arg(token);
    const std::optional<int> marker = evaluatePlasmaScriptResultOptional(script);
    if (!marker.has_value())
    {
        return result;
    }
    if (*marker > 0)
    {
        result.outcome = ArchDock::FreePanelHostMutationOutcome::Verified;
        result.screenIndex = *marker - 1;
    }
    else if (*marker == 0)
    {
        result.outcome = ArchDock::FreePanelHostMutationOutcome::NoHost;
    }
    else
    {
        result.outcome = ArchDock::FreePanelHostMutationOutcome::CandidateUnverified;
    }
    return result;
}

ArchDock::FreePanelHostVerificationOutcome PanelWindow::freePanelHostVerification(
    int desktopContainmentId,
    int dockAppletId,
    const QString &panelId,
    const QString &ownershipToken) const
{
    if (desktopContainmentId < 0 || dockAppletId < 0 || panelId.isEmpty() ||
        ownershipToken.isEmpty())
    {
        return ArchDock::FreePanelHostVerificationOutcome::UnownedOrUnverified;
    }

    const QString script = QStringLiteral(R"JS(
const desktop = desktopById(%1);
const dock = desktop ? desktop.widgetById(%2) : null;
let outcome = 0;
if (desktop && Number(desktop.id) === %1 && dock && Number(dock.id) === %2 &&
    dock.type === "org.archdock.dock") {
    outcome = -1;
    dock.currentConfigGroup = ["General"];
    const bootstrap = String(dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
    if (String(dock.readConfig("panelId", "")) === %3 &&
        String(dock.readConfig("panelType", "")) === "empty" &&
        String(dock.readConfig("ownerToken", "")) === %4 &&
        (bootstrap === "false" || bootstrap === "0")) {
        outcome = 1;
    }
} else if (dock) {
    outcome = -1;
}
print("ARCHDOCK_RESULT:" + String(outcome));
)JS")
                               .arg(desktopContainmentId)
                               .arg(dockAppletId)
                               .arg(plasmaScriptStringLiteral(panelId))
                               .arg(plasmaScriptStringLiteral(ownershipToken));
    const std::optional<int> result = evaluatePlasmaScriptResultOptional(script);
    if (!result.has_value())
    {
        return ArchDock::FreePanelHostVerificationOutcome::QueryFailed;
    }
    if (*result == 1)
    {
        return ArchDock::FreePanelHostVerificationOutcome::Owned;
    }
    if (*result == 0)
    {
        return ArchDock::FreePanelHostVerificationOutcome::Missing;
    }
    return ArchDock::FreePanelHostVerificationOutcome::UnownedOrUnverified;
}

ArchDock::FreePanelRemovalOutcome PanelWindow::removeOwnedFreePanelHost(
    int desktopContainmentId,
    int dockAppletId,
    const QString &panelId,
    const QString &ownershipToken) const
{
    if (desktopContainmentId < 0 || dockAppletId < 0 || panelId.isEmpty() ||
        ownershipToken.isEmpty())
    {
        return ArchDock::FreePanelRemovalOutcome::Refused;
    }

    const QString removalScript = QStringLiteral(R"JS(
function ownedMatches() {
    const matches = [];
    const allDesktops = desktops();
    for (let desktopIndex = 0; desktopIndex < allDesktops.length; ++desktopIndex) {
        const desktop = desktopById(Number(allDesktops[desktopIndex].id));
        if (!desktop)
            continue;
        const docks = desktop.widgets("org.archdock.dock");
        for (let dockIndex = 0; dockIndex < docks.length; ++dockIndex) {
            const dock = desktop.widgetById(Number(docks[dockIndex].id));
            if (!dock || dock.type !== "org.archdock.dock")
                continue;
            dock.currentConfigGroup = ["General"];
            const bootstrap = String(
                dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
            if (String(dock.readConfig("panelId", "")) === %3 &&
                String(dock.readConfig("panelType", "")) === "empty" &&
                String(dock.readConfig("ownerToken", "")) === %4 &&
                (bootstrap === "false" || bootstrap === "0")) {
                matches.push({
                    desktopContainmentId: Number(desktop.id),
                    dockAppletId: Number(dock.id)
                });
            }
        }
    }
    return matches;
}

const matches = ownedMatches();
let outcome = matches.length === 0 ? 2 : 0;
if (matches.length === 1 &&
    matches[0].desktopContainmentId === %1 &&
    matches[0].dockAppletId === %2) {
    const desktop = desktopById(%1);
    const dock = desktop ? desktop.widgetById(%2) : null;
    if (desktop && Number(desktop.id) === %1 && dock && Number(dock.id) === %2 &&
        dock.type === "org.archdock.dock") {
        dock.currentConfigGroup = ["General"];
        const bootstrap = String(
            dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
        if (String(dock.readConfig("panelId", "")) === %3 &&
            String(dock.readConfig("panelType", "")) === "empty" &&
            String(dock.readConfig("ownerToken", "")) === %4 &&
            (bootstrap === "false" || bootstrap === "0")) {
            dock.remove();
            outcome = 1;
        }
    }
}
print("ARCHDOCK_RESULT:" + String(outcome));
)JS")
                               .arg(desktopContainmentId)
                               .arg(dockAppletId)
                               .arg(plasmaScriptStringLiteral(panelId))
                               .arg(plasmaScriptStringLiteral(ownershipToken));
    const ArchDock::FreePanelRemovalOutcome removalOutcome =
        freePanelRemovalOutcome(evaluatePlasmaScriptResultOptional(removalScript));
    if (removalOutcome != ArchDock::FreePanelRemovalOutcome::Removed)
    {
        return removalOutcome;
    }

    // Plasma applet destruction is deferred. Verify absence in a separate D-Bus turn.
    const QString verificationScript = QStringLiteral(R"JS(
const desktop = desktopById(%1);
let outcome = -1;
if (desktop && Number(desktop.id) === %1) {
    outcome = desktop.widgetById(%2) ? 0 : 1;
}
print("ARCHDOCK_RESULT:" + String(outcome));
)JS")
                                           .arg(desktopContainmentId)
                                           .arg(dockAppletId);
    const std::optional<int> verificationResult =
        evaluatePlasmaScriptResultOptional(verificationScript);
    if (!verificationResult.has_value() || *verificationResult < 0)
    {
        return ArchDock::FreePanelRemovalOutcome::QueryFailed;
    }
    return *verificationResult == 1
        ? ArchDock::FreePanelRemovalOutcome::Removed
        : ArchDock::FreePanelRemovalOutcome::Refused;
}

ArchDock::FreePanelRemovalOutcome PanelWindow::removeOwnedFreePanelHostByIdentity(
    const QString &panelId,
    const QString &ownershipToken) const
{
    if (panelId.isEmpty() || ownershipToken.isEmpty())
    {
        return ArchDock::FreePanelRemovalOutcome::Refused;
    }

    const QString script = QStringLiteral(R"JS(
let matches = 0;
let matchingDesktopId = -1;
let matchingAppletId = -1;
const allDesktops = desktops();
for (let desktopIndex = 0; desktopIndex < allDesktops.length; ++desktopIndex) {
    const desktop = desktopById(Number(allDesktops[desktopIndex].id));
    if (!desktop)
        continue;
    const docks = desktop.widgets("org.archdock.dock");
    for (let dockIndex = 0; dockIndex < docks.length; ++dockIndex) {
        const dock = desktop.widgetById(Number(docks[dockIndex].id));
        if (!dock || dock.type !== "org.archdock.dock")
            continue;
        dock.currentConfigGroup = ["General"];
        const bootstrap = String(dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
        if (String(dock.readConfig("panelId", "")) === %1 &&
            String(dock.readConfig("panelType", "")) === "empty" &&
            String(dock.readConfig("ownerToken", "")) === %2 &&
            (bootstrap === "false" || bootstrap === "0")) {
            ++matches;
            matchingDesktopId = Number(desktop.id);
            matchingAppletId = Number(dock.id);
        }
    }
}

let outcome = matches === 0 ? 2 : 0;
if (matches === 1) {
    const desktop = desktopById(matchingDesktopId);
    const dock = desktop ? desktop.widgetById(matchingAppletId) : null;
    if (!desktop || !dock) {
        outcome = 2;
    } else if (dock.type === "org.archdock.dock") {
        dock.currentConfigGroup = ["General"];
        const bootstrap = String(dock.readConfig("bootstrapFreeDock", true)).toLowerCase();
        if (String(dock.readConfig("panelId", "")) === %1 &&
            String(dock.readConfig("panelType", "")) === "empty" &&
            String(dock.readConfig("ownerToken", "")) === %2 &&
            (bootstrap === "false" || bootstrap === "0")) {
            dock.remove();
            outcome = desktop.widgetById(matchingAppletId) ? 0 : 1;
        }
    }
}
print("ARCHDOCK_RESULT:" + String(outcome));
)JS")
                               .arg(plasmaScriptStringLiteral(panelId))
                               .arg(plasmaScriptStringLiteral(ownershipToken));
    return freePanelRemovalOutcome(evaluatePlasmaScriptResultOptional(script));
}

ArchDock::FreePanelRemovalOutcome PanelWindow::removeVerifiedFreeTemplateBridge(
    int containmentId,
    const QString &ownershipToken) const
{
    if (containmentId < 0 || ownershipToken.isEmpty())
    {
        return ArchDock::FreePanelRemovalOutcome::Refused;
    }

    const QString script = QStringLiteral(R"JS(
let outcome = 0;
const bridgePanel = panelById(%1);
if (!bridgePanel) {
    outcome = 2;
} else if (Number(bridgePanel.id) === %1) {
    const controls = bridgePanel.widgets("org.archdock.control");
    let matchingControlId = -1;
    let matches = 0;
    for (let index = 0; index < controls.length; ++index) {
        const control = bridgePanel.widgetById(controls[index].id);
        if (!control || control.type !== "org.archdock.control")
            continue;
        control.currentConfigGroup = ["General"];
        if (String(control.readConfig("bootstrapAction", "")) ===
                "create-circular-free-panel" &&
            String(control.readConfig("bootstrapToken", "")) === %2 &&
            Number(control.readConfig("bootstrapPanelId", -1)) === %1) {
            matchingControlId = Number(control.id);
            ++matches;
        }
    }
    if (matches === 1) {
        const currentPanel = panelById(%1);
        const control = currentPanel ? currentPanel.widgetById(matchingControlId) : null;
        if (currentPanel && Number(currentPanel.id) === %1 && control &&
            control.type === "org.archdock.control") {
            control.currentConfigGroup = ["General"];
            if (String(control.readConfig("bootstrapAction", "")) ===
                    "create-circular-free-panel" &&
                String(control.readConfig("bootstrapToken", "")) === %2 &&
                Number(control.readConfig("bootstrapPanelId", -1)) === %1) {
                currentPanel.remove();
                outcome = panelById(%1) ? 0 : 1;
            }
        }
    }
}
print("ARCHDOCK_RESULT:" + String(outcome));
)JS")
                               .arg(containmentId)
                               .arg(plasmaScriptStringLiteral(ownershipToken));
    return freePanelRemovalOutcome(evaluatePlasmaScriptResultOptional(script));
}

void PanelWindow::saveFreePanelPosition(const QString &panelId, int x, int y)
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() != QStringLiteral("free"))
        return;
    m_panelRegistry.updatePanel(panelId, {
        {QStringLiteral("x"), qMax(0, x)},
        {QStringLiteral("y"), qMax(0, y)}});
}

bool PanelWindow::setNativePanelType(const QString &panelId, const QString &type)
{
    const QString normalizedType = type.trimmed().toLower();
    if (!m_panelRegistry.panelIds().contains(panelId) ||
        !isNativeDockPanelEdge(m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString()) ||
        !isNativeDockPanelType(normalizedType))
    {
        return false;
    }

    m_panelRegistry.setPanelValue(panelId, QStringLiteral("type"), normalizedType);
    return createNativeKdePanel(panelId);
}

QStringList PanelWindow::availableKdeWidgets() const
{
    QSet<QString> ids;
    const QStringList roots = QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("plasma/plasmoids"),
        QStandardPaths::LocateDirectory);
    for (const QString &root : roots)
    {
        const QDir directory(root);
        const QFileInfoList entries = directory.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &entry : entries)
        {
            const QString jsonPath = entry.filePath() + QStringLiteral("/metadata.json");
            const QString desktopPath = entry.filePath() + QStringLiteral("/metadata.desktop");
            QString pluginId;
            QFile jsonFile(jsonPath);
            if (jsonFile.open(QIODevice::ReadOnly))
            {
                const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll());
                pluginId = document.object().value(QStringLiteral("KPlugin")).toObject()
                    .value(QStringLiteral("Id")).toString();
                if (pluginId.isEmpty())
                {
                    pluginId = document.object().value(QStringLiteral("Id")).toString();
                }
            }
            if (pluginId.isEmpty() && QFileInfo::exists(desktopPath))
            {
                QSettings metadata(desktopPath, QSettings::IniFormat);
                pluginId = metadata.value(QStringLiteral("Desktop Entry/X-KDE-PluginInfo-Name")).toString();
            }
            if (!pluginId.isEmpty())
            {
                ids.insert(pluginId);
            }
        }
    }

    QStringList result = ids.values();
    result.sort(Qt::CaseInsensitive);
    return result;
}

int PanelWindow::nativePanelId(const QString &panelId) const
{
    const QVariant value = m_panelRegistry.panelValue(panelId, QStringLiteral("nativePanelId"));
    return value.isValid() ? value.toInt() : -1;
}

int PanelWindow::nativeControlAppletId(const QString &panelId) const
{
    const QVariant value = m_panelRegistry.panelValue(
        panelId,
        QStringLiteral("nativeControlAppletId"));
    return value.isValid() ? value.toInt() : -1;
}

int PanelWindow::nativeDockAppletId(const QString &panelId) const
{
    const QVariant value = m_panelRegistry.panelValue(
        panelId,
        QStringLiteral("nativeDockAppletId"));
    return value.isValid() ? value.toInt() : -1;
}

QString PanelWindow::nativeOwnershipToken(const QString &panelId) const
{
    return m_panelRegistry.panelValue(panelId, QStringLiteral("nativeOwnershipToken")).toString();
}

int PanelWindow::nativePanelOffset(const QString &panelId) const
{
    return ArchDock::edgeOffset(edgePanels(), panelId);
}

int PanelWindow::evaluatePlasmaScript(const QString &script) const
{
    QDBusInterface shell(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"),
        QDBusConnection::sessionBus());
    if (!shell.isValid())
    {
        return -1;
    }

    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
    if (!reply.isValid())
    {
        qWarning() << "Plasma panel script failed:" << reply.error().message();
        return -1;
    }

    const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("(-?\\d+)"))
        .match(reply.value());
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

std::optional<int> PanelWindow::evaluatePlasmaScriptResultOptional(const QString &script) const
{
    QDBusInterface shell(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"),
        QDBusConnection::sessionBus());
    if (!shell.isValid())
    {
        return std::nullopt;
    }

    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
    if (!reply.isValid())
    {
        qWarning() << "Plasma panel script failed:" << reply.error().message();
        return std::nullopt;
    }

    const std::optional<int> result = ArchDock::parsePlasmaScriptResult(reply.value());
    if (!result.has_value())
    {
        qWarning() << "Plasma panel script returned an unverified result:"
                   << reply.value().trimmed();
        return std::nullopt;
    }
    return result;
}

int PanelWindow::evaluatePlasmaScriptResult(const QString &script) const
{
    return evaluatePlasmaScriptResultOptional(script).value_or(-1);
}

std::optional<bool> PanelWindow::nativePanelExistence(int panelId) const
{
    if (panelId < 0)
    {
        return false;
    }

    const std::optional<int> result = evaluatePlasmaScriptResultOptional(
        QStringLiteral("print('ARCHDOCK_RESULT:' + String(panelById(%1) ? 1 : 0));")
            .arg(panelId));
    if (!result.has_value() || (*result != 0 && *result != 1))
    {
        return std::nullopt;
    }
    return *result == 1;
}

bool PanelWindow::nativePanelIsOwned(const QString &panelId, int containmentId) const
{
    return nativePanelIsOwned(panelId, containmentId, nativeOwnershipToken(panelId));
}

bool PanelWindow::nativePanelIsOwned(const QString &panelId,
                                     int containmentId,
                                     const QString &ownershipToken) const
{
    if (containmentId < 0 || ownershipToken.isEmpty())
    {
        return false;
    }

    return evaluatePlasmaScriptResult(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print('ARCHDOCK_RESULT:0'); }"
            "else {"
            "panel.currentConfigGroup = ['ArchDock'];"
            "var owned = panel.readConfig('ownerToken', '') === %2 && "
            "panel.readConfig('panelId', '') === %3;"
            "print('ARCHDOCK_RESULT:' + String(owned ? 1 : 0));"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(ownershipToken))
            .arg(plasmaScriptStringLiteral(panelId))) == 1;
}

PanelWindow::NativePanelDiscoveryResult PanelWindow::discoverNativePanel(
    const QString &panelId,
    const QString &ownershipToken,
    const QString &panelType) const
{
    if (panelId.isEmpty() || ownershipToken.isEmpty() || !isNativeDockPanelType(panelType))
    {
        return {};
    }

    const std::optional<int> hostQuery = evaluatePlasmaScriptResultOptional(
        QStringLiteral(
            "var result = (function() {"
            "try {"
            "var candidates = panels();"
            "var matches = 0;"
            "var matchedId = -1;"
            "for (var index = 0; index < candidates.length; ++index) {"
            "var candidate = candidates[index];"
            "candidate.currentConfigGroup = ['ArchDock'];"
            "if (String(candidate.readConfig('ownerToken', '')) === %1 && "
            "String(candidate.readConfig('panelId', '')) === %2) {"
            "++matches; matchedId = candidate.id;"
            "}"
            "}"
            "return matches === 0 ? -1 : (matches === 1 ? matchedId : -2);"
            "} catch (error) { return -3; }"
            "})();"
            "print('ARCHDOCK_RESULT:' + String(result));")
            .arg(plasmaScriptStringLiteral(ownershipToken))
            .arg(plasmaScriptStringLiteral(panelId)));
    const ArchDock::NativeContainmentMatch hostMatch =
        ArchDock::classifyNativeContainmentMatch(hostQuery);
    switch (hostMatch.status)
    {
    case ArchDock::NativeContainmentMatchStatus::QueryFailed:
        return {};
    case ArchDock::NativeContainmentMatchStatus::Missing:
        return {NativePanelDiscoveryStatus::Missing, -1, -1};
    case ArchDock::NativeContainmentMatchStatus::Conflict:
        return {NativePanelDiscoveryStatus::HostConflict, -1, -1};
    case ArchDock::NativeContainmentMatchStatus::Unique:
        break;
    }

    const QString rendererQuery = panelTypeNeedsDockApplet(panelType)
        ? QStringLiteral(
              "var result = (function() {"
              "try {"
              "var panel = panelById(%1);"
              "if (!panel) { return -3; }"
              "var docks = panel.widgets('org.archdock.dock');"
              "if (docks.length === 0) { return -1; }"
              "if (docks.length !== 1) { return -2; }"
              "var dock = docks[0];"
              "dock.currentConfigGroup = ['General'];"
              "return String(dock.readConfig('panelId', '')) === %2 && "
              "String(dock.readConfig('panelType', '')) === %3 ? dock.id : -2;"
              "} catch (error) { return -3; }"
              "})();"
              "print('ARCHDOCK_RESULT:' + String(result));")
              .arg(hostMatch.containmentId)
              .arg(plasmaScriptStringLiteral(panelId))
              .arg(plasmaScriptStringLiteral(panelType))
        : QStringLiteral(
              "var result = (function() {"
              "try {"
              "var panel = panelById(%1);"
              "if (!panel) { return -3; }"
              "return panel.widgets('org.archdock.dock').length === 0 ? -1 : -2;"
              "} catch (error) { return -3; }"
              "})();"
              "print('ARCHDOCK_RESULT:' + String(result));")
              .arg(hostMatch.containmentId);
    const ArchDock::NativeContainmentMatch rendererMatch =
        ArchDock::classifyNativeContainmentMatch(
            evaluatePlasmaScriptResultOptional(rendererQuery));
    switch (rendererMatch.status)
    {
    case ArchDock::NativeContainmentMatchStatus::QueryFailed:
        return {};
    case ArchDock::NativeContainmentMatchStatus::Conflict:
        return {NativePanelDiscoveryStatus::RendererConflict,
                hostMatch.containmentId,
                -1};
    case ArchDock::NativeContainmentMatchStatus::Missing:
        return {NativePanelDiscoveryStatus::Unique, hostMatch.containmentId, -1};
    case ArchDock::NativeContainmentMatchStatus::Unique:
        return {NativePanelDiscoveryStatus::Unique,
                hostMatch.containmentId,
                rendererMatch.containmentId};
    }

    return {};
}

int PanelWindow::createNativePanelCandidate(const QString &panelId,
    const QString &ownershipToken, QString *errorCode) const
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
    {
        if (errorCode) *errorCode = QStringLiteral("invalid-recovery-record");
        return -1;
    }
    return createNativePanelCandidate(*definition, ownershipToken, errorCode);
}

int PanelWindow::createNativePanelCandidate(const ArchDock::PanelDefinition &definition,
                                            const QString &ownershipToken,
                                            QString *errorCode) const
{
    if (errorCode)
    {
        errorCode->clear();
    }
    const auto fail = [errorCode](const QString &code)
    {
        if (errorCode)
        {
            *errorCode = code;
        }
        return -1;
    };

    const QString panelId = definition.identity.id;
    if (panelId.isEmpty() || ownershipToken.isEmpty())
    {
        return fail(QStringLiteral("invalid-recovery-record"));
    }

    const QString edge = definition.placement.edge;
    const QString type = definition.content.type;
    if (!isNativeDockPanelEdge(edge) || !isNativeDockPanelType(type))
    {
        return fail(QStringLiteral("invalid-native-panel-definition"));
    }

    if (definition.host.screenIndex < 0 || definition.host.screenIndex >= QGuiApplication::screens().size())
    {
        return fail(QStringLiteral("screen-unavailable"));
    }

    const bool needsRenderer = panelTypeNeedsDockApplet(type);
    const QString rendererPreflight = needsRenderer
        ? QStringLiteral(
              "if (knownWidgetTypes.indexOf('org.archdock.dock') < 0) { return -2; }")
        : QString{};
    const QString hostSetup = QStringLiteral(
        "panel = new Panel;"
        "if (!panel || panel.id < 0) { return -1; }"
        "panel.currentConfigGroup = ['ArchDock'];"
        "panel.writeConfig('ownerToken', %1);"
        "panel.writeConfig('panelId', %2);"
        "panel.reloadConfig();"
        "if (panel.readConfig('ownerToken', '') !== %1 || "
        "panel.readConfig('panelId', '') !== %2) "
        "{ panel.remove(); panel = null; return -3; }"
        "panel.writeConfig('temporaryHidden', '0');"
        "panel.hiding = 'none';"
        "panel.reloadConfig();"
        "if (panel.readConfig('ownerToken', '') !== %1 || "
        "panel.readConfig('panelId', '') !== %2 || "
        "String(panel.readConfig('temporaryHidden', '0')) !== '0' || "
        "panel.hiding !== 'none') "
        "{ panel.remove(); panel = null; return -6; }")
        .arg(plasmaScriptStringLiteral(ownershipToken))
        .arg(plasmaScriptStringLiteral(panelId));
    const QString rendererAttachment = needsRenderer
        ? QStringLiteral(
              "var dock = panel.addWidget('org.archdock.dock');"
              "if (!dock) { panel.remove(); panel = null; return -4; }"
              "dock.currentConfigGroup = ['General'];"
              "dock.writeConfig('panelId', %1);"
              "dock.writeConfig('panelType', %2);"
              "dock.reloadConfig();"
              "if (dock.id < 0 || dock.type !== 'org.archdock.dock' || "
              "dock.readConfig('panelId', '') !== %1 || "
              "dock.readConfig('panelType', '') !== %2) "
              "{ panel.remove(); panel = null; return -5; }")
              .arg(plasmaScriptStringLiteral(panelId))
              .arg(plasmaScriptStringLiteral(type))
        : QString{};
    const QString script = QStringLiteral(
        "var result = (function() {"
        "var panel = null;"
        "try {"
        "%1"
        "%2"
        "%3"
        "return panel.id;"
        "} catch (error) {"
        "if (panel) { panel.remove(); }"
        "return -1;"
        "}"
        "})();"
        "print('ARCHDOCK_RESULT:' + String(result));")
        .arg(rendererPreflight, hostSetup, rendererAttachment);
    const int result = evaluatePlasmaScriptResult(script);
    if (result >= 0)
    {
        return result;
    }

    switch (result)
    {
    case -2:
        return fail(QStringLiteral("renderer-unavailable"));
    case -3:
        return fail(QStringLiteral("ownership-verification-failed"));
    case -4:
        return fail(QStringLiteral("renderer-attachment-failed"));
    case -5:
        return fail(QStringLiteral("renderer-verification-failed"));
    case -6:
        return fail(QStringLiteral("host-configuration-failed"));
    default:
        return fail(QStringLiteral("host-creation-failed"));
    }
}

std::optional<int> PanelWindow::verifiedNativeDockAppletId(
    const QString &panelId,
    int containmentId,
    const QString &panelType) const
{
    if (containmentId < 0 || !isNativeDockPanelType(panelType))
    {
        return std::nullopt;
    }

    if (!panelTypeNeedsDockApplet(panelType))
    {
        const int dockCount = evaluatePlasmaScriptResult(
            QStringLiteral(
                "var panel = panelById(%1);"
                "var count = panel ? panel.widgets('org.archdock.dock').length : -1;"
                "print('ARCHDOCK_RESULT:' + String(count));")
                .arg(containmentId));
        return dockCount == 0 ? std::optional<int>{-1} : std::nullopt;
    }

    const int dockId = evaluatePlasmaScriptResult(
        QStringLiteral(
            "var panel = panelById(%1);"
            "var docks = panel ? panel.widgets('org.archdock.dock') : [];"
            "var matches = 0;"
            "var result = -1;"
            "for (var index = 0; index < docks.length; ++index) {"
            "var dock = docks[index];"
            "dock.currentConfigGroup = ['General'];"
            "if (dock.readConfig('panelId', '') === %2 && "
            "dock.readConfig('panelType', '') === %3) "
            "{ ++matches; result = dock.id; }"
            "}"
            "print('ARCHDOCK_RESULT:' + String(matches === 1 ? result : -1));")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(panelId))
            .arg(plasmaScriptStringLiteral(panelType)));
    return dockId >= 0 ? std::optional<int>{dockId} : std::nullopt;
}

bool PanelWindow::rollbackNativePanelCandidate(const QString &panelId,
                                               int containmentId,
                                               const QString &ownershipToken) const
{
    if (containmentId < 0 || ownershipToken.isEmpty())
    {
        return false;
    }

    const int removed = evaluatePlasmaScriptResult(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print('ARCHDOCK_RESULT:1'); }"
            "else {"
            "panel.currentConfigGroup = ['ArchDock'];"
            "var owned = panel.readConfig('ownerToken', '') === %2 && "
            "panel.readConfig('panelId', '') === %3;"
            "if (!owned) { print('ARCHDOCK_RESULT:0'); }"
            "else { panel.remove(); print('ARCHDOCK_RESULT:1'); }"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(ownershipToken))
            .arg(plasmaScriptStringLiteral(panelId)));
    const std::optional<bool> containmentExists = nativePanelExistence(containmentId);
    return removed == 1 && containmentExists.has_value() && !*containmentExists;
}

bool PanelWindow::nativeControlAppletIsOwned(const QString &panelId,
                                              int containmentId,
                                              int appletId) const
{
    if (containmentId < 0 || appletId < 0)
    {
        return false;
    }

    return evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "var control = panel ? panel.widgetById(%2) : null;"
            "if (!control || control.type !== 'org.archdock.control') { print(0); }"
            "else {"
            "control.currentConfigGroup = ['General'];"
            "print(control.readConfig('panelId', '') === %3 ? 1 : 0);"
            "}")
            .arg(containmentId)
            .arg(appletId)
            .arg(plasmaScriptStringLiteral(panelId))) == 1;
}

bool PanelWindow::nativeDockAppletIsOwned(const QString &panelId,
                                           int containmentId,
                                           int appletId) const
{
    if (containmentId < 0 || appletId < 0)
    {
        return false;
    }

    return evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "var dock = panel ? panel.widgetById(%2) : null;"
            "if (!dock || dock.type !== 'org.archdock.dock') { print(0); }"
            "else {"
            "dock.currentConfigGroup = ['General'];"
            "print(dock.readConfig('panelId', '') === %3 ? 1 : 0);"
            "}")
            .arg(containmentId)
            .arg(appletId)
            .arg(plasmaScriptStringLiteral(panelId))) == 1;
}

std::optional<bool> PanelWindow::nativePanelTemporarilyHidden(
    const QString &panelId,
    int containmentId) const
{
    if (!nativePanelIsOwned(panelId, containmentId))
    {
        return std::nullopt;
    }

    const int state = evaluatePlasmaScriptResult(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print('ARCHDOCK_RESULT:-1'); }"
            "else {"
            "panel.currentConfigGroup = ['ArchDock'];"
            "var temporaryHidden = String(panel.readConfig('temporaryHidden', '0')) === '1';"
            "var hiding = panel.hiding;"
            "if (temporaryHidden && hiding !== 'autohide') { print('ARCHDOCK_RESULT:-2'); }"
            "else { print('ARCHDOCK_RESULT:' + String(temporaryHidden ? 1 : 0)); }"
            "}")
            .arg(containmentId));
    if (state == 0 || state == 1)
    {
        return state == 1;
    }

    qWarning() << "Could not read a consistent temporary presentation for native panel"
               << panelId;
    return std::nullopt;
}

bool PanelWindow::reconcileNativePanelVisibility(
    const QString &panelId,
    int containmentId,
    const QString &ownershipToken,
    ArchDock::PanelVisibilityMode requestedMode,
    bool visible,
    QVariantMap persistValues)
{
    const ArchDock::NativeVisibilityCapabilities capabilities =
        nativeVisibilityCapabilities(panelId);
    const QString requestedModeName = ArchDock::panelVisibilityModeToString(requestedMode);
    const QStringList supportedModes = ArchDock::supportedNativeVisibilityModes(capabilities);
    const ArchDock::PanelVisibilityDecision decision = nativePanelVisibilityDecision(
        panelId, requestedMode, visible);
    const ArchDock::NativeVisibilityResolution resolution =
        ArchDock::resolveNativeVisibility(
            requestedMode, decision, !visible, capabilities);

    const auto persistence = [this, panelId](const QVariantMap &values)
        -> ArchDock::PlasmaPanelPersistence
    {
        if (values.isEmpty())
        {
            return {};
        }
        return [this, panelId, values]
        {
            return m_panelRegistry.updatePanelChecked(panelId, values);
        };
    };
    const auto stateKey = [containmentId](ArchDock::PlasmaPanelHidingMode hostMode,
                                          bool temporaryHidden)
    {
        return QStringLiteral("%1|%2|%3")
            .arg(containmentId)
            .arg(ArchDock::plasmaPanelHidingModeToString(hostMode))
            .arg(temporaryHidden ? 1 : 0);
    };
    const auto structuredResult = [&](const ArchDock::PlasmaPanelVisibilityApplyResult &applied,
                                      ArchDock::PanelVisibilityMode effectiveMode,
                                      ArchDock::PlasmaPanelHidingMode requestedHostMode,
                                      bool fallbackAttempted,
                                      const QString &fallbackReason,
                                      const QString &fallbackErrorCode,
                                      const QVariantMap &requestedApply = QVariantMap{})
    {
        QVariantMap structured = applied.toVariantMap();
        structured.insert(QStringLiteral("verified"),
                          applied.success() && applied.ownershipVerified);
        structured.insert(QStringLiteral("requestedMode"), requestedModeName);
        structured.insert(
            QStringLiteral("effectiveMode"),
            ArchDock::panelVisibilityModeToString(effectiveMode));
        structured.insert(
            QStringLiteral("hostMode"),
            applied.actualHostMode.value_or(
                ArchDock::plasmaPanelHidingModeToString(requestedHostMode)));
        structured.insert(QStringLiteral("supportedModes"), supportedModes);
        structured.insert(QStringLiteral("watcherAvailable"), m_windowWatcher.available());
        structured.insert(QStringLiteral("fallbackAttempted"), fallbackAttempted);
        structured.insert(QStringLiteral("fallbackApplied"),
                          fallbackAttempted && applied.success());
        structured.insert(QStringLiteral("fallbackReason"), fallbackReason);
        structured.insert(QStringLiteral("fallbackErrorCode"), fallbackErrorCode);
        if (!requestedApply.isEmpty())
        {
            structured.insert(QStringLiteral("requestedApply"), requestedApply);
        }
        return structured;
    };

    if (containmentId < 0 || ownershipToken.trimmed().isEmpty())
    {
        QVariantMap failed{
            {QStringLiteral("success"), false},
            {QStringLiteral("verified"), false},
            {QStringLiteral("status"), QStringLiteral("failed")},
            {QStringLiteral("errorCode"),
             containmentId < 0
                 ? QStringLiteral("containment-missing")
                 : QStringLiteral("ownership-token-missing")},
            {QStringLiteral("requestedMode"), requestedModeName},
            {QStringLiteral("effectiveMode"), QString{}},
            {QStringLiteral("hostMode"), QString{}},
            {QStringLiteral("supportedModes"), supportedModes},
            {QStringLiteral("watcherAvailable"), m_windowWatcher.available()},
            {QStringLiteral("fallbackAttempted"), false},
            {QStringLiteral("fallbackApplied"), false},
            {QStringLiteral("fallbackReason"), QString{}},
            {QStringLiteral("fallbackErrorCode"), QString{}},
            {QStringLiteral("ownershipVerified"), false},
            {QStringLiteral("rollbackAttempted"), false},
            {QStringLiteral("rollbackSucceeded"), false},
            {QStringLiteral("rollbackErrorCode"), QString{}},
        };
        recordNativePanelVisibilityResult(panelId, std::move(failed));
        return false;
    }

    const bool fallbackRequired = resolution.fallbackApplied;
    const ArchDock::PanelVisibilityMode targetMode = fallbackRequired
        ? ArchDock::PanelVisibilityMode::AlwaysVisible
        : resolution.effectiveMode;
    const ArchDock::PlasmaPanelHidingMode targetHostMode = resolution.hostMode;
    const bool targetTemporaryHidden = fallbackRequired ? false : !visible;
    QVariantMap targetPersistence = persistValues;
    if (fallbackRequired)
    {
        targetPersistence.insert(QStringLiteral("visibilityMode"), QStringLiteral("always"));
        targetPersistence.insert(QStringLiteral("visible"), true);
    }

    const QString requestedStateKey = stateKey(targetHostMode, targetTemporaryHidden);
    if (!fallbackRequired && targetPersistence.isEmpty() &&
        m_nativePanelVisibilityStateCache.value(panelId) == requestedStateKey)
    {
        return true;
    }

    const ArchDock::PlasmaPanelAdapter adapter(
        [this](const QString &script)
        {
            return evaluatePlasmaScriptResultOptional(script);
        });
    ArchDock::PlasmaPanelVisibilityApplyResult applied = adapter.applyVisibility(
        containmentId,
        panelId,
        ownershipToken,
        targetHostMode,
        targetTemporaryHidden,
        persistence(targetPersistence));

    if (applied.success())
    {
        m_nativePanelVisibilityStateCache.insert(panelId, requestedStateKey);
        QVariantMap structured = structuredResult(
            applied,
            targetMode,
            targetHostMode,
            fallbackRequired,
            fallbackRequired ? resolution.errorCode : QString{},
            QString{});
        if (fallbackRequired)
        {
            structured.insert(QStringLiteral("status"), QStringLiteral("fallback-applied"));
            structured.insert(QStringLiteral("errorCode"), resolution.errorCode);
            if (panelId == QStringLiteral("bottom") && m_settings.autoHide())
            {
                m_settings.setAutoHide(false);
            }
        }
        recordNativePanelVisibilityResult(panelId, std::move(structured));
        return true;
    }

    m_nativePanelVisibilityStateCache.remove(panelId);
    const QVariantMap requestedApply = applied.toVariantMap();
    if (fallbackRequired || !applied.ownershipVerified ||
        (requestedMode == ArchDock::PanelVisibilityMode::AlwaysVisible && visible))
    {
        QVariantMap structured = structuredResult(
            applied,
            targetMode,
            targetHostMode,
            fallbackRequired,
            fallbackRequired ? resolution.errorCode : applied.errorCode,
            fallbackRequired ? applied.errorCode : QString{});
        if (fallbackRequired)
        {
            structured.insert(QStringLiteral("status"), QStringLiteral("fallback-failed"));
        }
        recordNativePanelVisibilityResult(panelId, std::move(structured));
        return false;
    }

    QVariantMap fallbackPersistence = persistValues;
    fallbackPersistence.insert(QStringLiteral("visibilityMode"), QStringLiteral("always"));
    fallbackPersistence.insert(QStringLiteral("visible"), true);
    ArchDock::PlasmaPanelVisibilityApplyResult fallback = adapter.applyVisibility(
        containmentId,
        panelId,
        ownershipToken,
        ArchDock::PlasmaPanelHidingMode::None,
        false,
        persistence(fallbackPersistence));
    QVariantMap structured = structuredResult(
        fallback,
        ArchDock::PanelVisibilityMode::AlwaysVisible,
        ArchDock::PlasmaPanelHidingMode::None,
        true,
        applied.errorCode,
        fallback.success() ? QString{} : fallback.errorCode,
        requestedApply);
    if (fallback.success())
    {
        structured.insert(QStringLiteral("status"), QStringLiteral("fallback-applied"));
        structured.insert(QStringLiteral("errorCode"), applied.errorCode);
        m_nativePanelVisibilityStateCache.insert(
            panelId,
            stateKey(ArchDock::PlasmaPanelHidingMode::None, false));
        if (panelId == QStringLiteral("bottom") && m_settings.autoHide())
        {
            m_settings.setAutoHide(false);
        }
    }
    else
    {
        structured.insert(QStringLiteral("status"), QStringLiteral("fallback-failed"));
    }
    recordNativePanelVisibilityResult(panelId, std::move(structured));
    return fallback.success();
}

bool PanelWindow::synchronizeNativePanelVisibility(const QString &panelId,
                                                   bool visible,
                                                   bool allowMissingHostRecovery,
                                                   std::optional<ArchDock::PanelVisibilityMode> requestedMode,
                                                   QVariantMap persistValues)
{
    int containmentId = nativePanelId(panelId);
    const QString type = m_panelRegistry.panelValue(
        panelId, QStringLiteral("type")).toString();
    QString ownershipToken = nativeOwnershipToken(panelId).trimmed();
    bool ownedFromDiscovery = false;

    if (!ownershipToken.isEmpty())
    {
        const NativePanelDiscoveryResult discovery = discoverNativePanel(
            panelId, ownershipToken, type);
        switch (discovery.status)
        {
        case NativePanelDiscoveryStatus::QueryFailed:
            qWarning() << "Could not query owned native panels for" << panelId;
            return false;
        case NativePanelDiscoveryStatus::HostConflict:
            if (!m_panelRegistry.recordNativePanelRecoveryConflict(
                    panelId, ownershipToken, QStringLiteral("multiple-owned-hosts")))
            {
                qWarning() << "Could not persist native host conflict for" << panelId;
            }
            return false;
        case NativePanelDiscoveryStatus::RendererConflict:
            if (!m_panelRegistry.recordNativePanelRecoveryConflict(
                    panelId, ownershipToken, QStringLiteral("multiple-or-unverified-renderers")))
            {
                qWarning() << "Could not persist native renderer conflict for" << panelId;
            }
            return false;
        case NativePanelDiscoveryStatus::Missing:
            if (!allowMissingHostRecovery)
            {
                return true;
            }
            if (!m_panelRegistry.detachMissingNativePanelAssociation(
                    panelId, ownershipToken, visible))
            {
                qWarning() << "Could not detach the missing native panel association for"
                           << panelId;
                return false;
            }
            containmentId = -1;
            ownershipToken.clear();
            break;
        case NativePanelDiscoveryStatus::Unique:
            if (!m_panelRegistry.rebindRecoveredNativePanelAssociation(
                    panelId,
                    discovery.containmentId,
                    discovery.dockAppletId,
                    ownershipToken))
            {
                qWarning() << "Could not persist the recovered native panel association for"
                           << panelId;
                return false;
            }
            containmentId = discovery.containmentId;
            ownedFromDiscovery = true;
            break;
        }
    }

    if (ownedFromDiscovery && !allowMissingHostRecovery)
    {
        return true;
    }

    ArchDock::NativeContainmentLifecycleState state;
    state.recordVisible = visible;

    if (containmentId < 0)
    {
        state.hostStatus = ArchDock::NativeContainmentHostStatus::Missing;
    }
    else if (ownedFromDiscovery)
    {
        state.hostStatus = ArchDock::NativeContainmentHostStatus::Owned;
    }
    else
    {
        const std::optional<bool> containmentExists = nativePanelExistence(containmentId);
        if (!containmentExists.has_value())
        {
            qWarning() << "Could not query the stored native panel for" << panelId;
            return false;
        }
        if (!*containmentExists)
        {
            state.hostStatus = ArchDock::NativeContainmentHostStatus::Missing;
        }
        else if (adoptNativePanelOwnership(panelId, containmentId))
        {
            state.hostStatus = ArchDock::NativeContainmentHostStatus::Owned;
        }
        else
        {
            state.hostStatus = ArchDock::NativeContainmentHostStatus::UnownedOrUnverified;
        }
    }

    if (state.hostStatus == ArchDock::NativeContainmentHostStatus::Owned)
    {
        state.rendererAttached = !panelTypeNeedsDockApplet(type) ||
            (ownedFromDiscovery
                 ? nativeDockAppletId(panelId) >= 0
                 : nativeDockAppletIsOwned(
                       panelId, containmentId, nativeDockAppletId(panelId)));
        const std::optional<bool> temporarilyHidden =
            nativePanelTemporarilyHidden(panelId, containmentId);
        if (!temporarilyHidden.has_value())
        {
            return false;
        }
        state.presentation = *temporarilyHidden
            ? ArchDock::NativeContainmentPresentation::Hidden
            : ArchDock::NativeContainmentPresentation::Shown;
    }

    if (state.hostStatus == ArchDock::NativeContainmentHostStatus::Missing &&
        !allowMissingHostRecovery)
    {
        return true;
    }

    ArchDock::NativeContainmentLifecycleIntent intent =
        ArchDock::nativeContainmentLifecycleIntent(
            ArchDock::NativeContainmentLifecycleRequest::Synchronize,
            state);
    if (intent == ArchDock::NativeContainmentLifecycleIntent::AttachRenderer)
    {
        if (!attachNativeDockApplet(panelId, containmentId))
        {
            return false;
        }
        const std::optional<int> verifiedDockApplet = verifiedNativeDockAppletId(
            panelId, containmentId, type);
        if (!verifiedDockApplet.has_value() || *verifiedDockApplet < 0 ||
            !m_panelRegistry.commitVerifiedNativePanelAssociation(
                panelId,
                containmentId,
                *verifiedDockApplet,
                nativeOwnershipToken(panelId)))
        {
            qWarning() << "Could not commit the recovered native renderer for" << panelId;
            return false;
        }
        state.rendererAttached = true;
        intent = ArchDock::nativeContainmentLifecycleIntent(
            ArchDock::NativeContainmentLifecycleRequest::Synchronize,
            state);
    }

    switch (intent)
    {
    case ArchDock::NativeContainmentLifecycleIntent::NoAction:
        if (state.hostStatus == ArchDock::NativeContainmentHostStatus::UnownedOrUnverified)
        {
            qWarning() << "Refusing to change visibility of an unowned native panel for" << panelId;
            return false;
        }
        if (state.hostStatus == ArchDock::NativeContainmentHostStatus::Missing)
        {
            const bool persisted = persistValues.isEmpty() ||
                m_panelRegistry.updatePanelChecked(panelId, persistValues);
            if (!persistValues.isEmpty())
            {
                const ArchDock::NativeVisibilityCapabilities capabilities =
                    nativeVisibilityCapabilities(panelId);
                QVariantMap deferred{
                    {QStringLiteral("success"), false},
                    {QStringLiteral("verified"), false},
                    {QStringLiteral("status"), QStringLiteral("deferred-no-host")},
                    {QStringLiteral("errorCode"),
                     persisted
                         ? QStringLiteral("containment-missing")
                         : QStringLiteral("persistence-failed")},
                    {QStringLiteral("requestedMode"),
                     ArchDock::panelVisibilityModeToString(
                         requestedMode.value_or(
                             ArchDock::panelVisibilityModeFromString(
                                 m_panelRegistry.panelValue(
                                     panelId,
                                     QStringLiteral("visibilityMode")).toString())))},
                    {QStringLiteral("effectiveMode"), QString{}},
                    {QStringLiteral("hostMode"), QString{}},
                    {QStringLiteral("supportedModes"),
                     ArchDock::supportedNativeVisibilityModes(capabilities)},
                    {QStringLiteral("watcherAvailable"), m_windowWatcher.available()},
                    {QStringLiteral("fallbackAttempted"), false},
                    {QStringLiteral("fallbackApplied"), false},
                    {QStringLiteral("fallbackReason"), QString{}},
                    {QStringLiteral("fallbackErrorCode"), QString{}},
                    {QStringLiteral("ownershipVerified"), false},
                    {QStringLiteral("rollbackAttempted"), false},
                    {QStringLiteral("rollbackSucceeded"), false},
                    {QStringLiteral("rollbackErrorCode"), QString{}},
                };
                recordNativePanelVisibilityResult(panelId, std::move(deferred));
            }
            return persisted;
        }
        break;
    case ArchDock::NativeContainmentLifecycleIntent::ShowHost:
        break;
    case ArchDock::NativeContainmentLifecycleIntent::HideHost:
        break;
    case ArchDock::NativeContainmentLifecycleIntent::RecreateMissingHost:
        if (!createNativeKdePanel(panelId))
        {
            return false;
        }
        containmentId = nativePanelId(panelId);
        ownershipToken = nativeOwnershipToken(panelId).trimmed();
        break;
    case ArchDock::NativeContainmentLifecycleIntent::CreateHost:
    case ArchDock::NativeContainmentLifecycleIntent::AttachRenderer:
    case ArchDock::NativeContainmentLifecycleIntent::RemoveHostPermanently:
        qWarning() << "Unexpected native visibility lifecycle intent for" << panelId;
        return false;
    }

    const ArchDock::PanelVisibilityMode effectiveRequestedMode = requestedMode.value_or(
        ArchDock::panelVisibilityModeFromString(
            m_panelRegistry.panelValue(
                panelId, QStringLiteral("visibilityMode")).toString()));
    return reconcileNativePanelVisibility(
        panelId,
        containmentId,
        ownershipToken,
        effectiveRequestedMode,
        visible,
        std::move(persistValues));
}

bool PanelWindow::adoptNativePanelOwnership(const QString &panelId, int containmentId)
{
    if (nativePanelIsOwned(panelId, containmentId))
    {
        return true;
    }

    if (containmentId < 0 || !nativeOwnershipToken(panelId).isEmpty() ||
        (!nativeControlAppletIsOwned(panelId, containmentId, nativeControlAppletId(panelId)) &&
         !nativeDockAppletIsOwned(panelId, containmentId, nativeDockAppletId(panelId))))
    {
        return false;
    }

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const int adopted = evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print(0); }"
            "else {"
            "panel.currentConfigGroup = ['ArchDock'];"
            "panel.writeConfig('ownerToken', %2);"
            "panel.writeConfig('panelId', %3);"
            "panel.reloadConfig();"
            "print(panel.readConfig('ownerToken', '') === %2 && "
            "panel.readConfig('panelId', '') === %3 ? 1 : 0);"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(token))
            .arg(plasmaScriptStringLiteral(panelId)));
    if (adopted != 1)
    {
        return false;
    }

    m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeOwnershipToken"), token);
    return true;
}

ArchDock::NativePanelPlacementResult PanelWindow::normalizedNativePanelPlacement(
    const QString &panelId, const QVariantMap &overrides) const
{
    const auto definition = runtimePanelDefinition(panelId);
    return definition ? normalizedNativePanelPlacement(*definition, overrides)
        : ArchDock::normalizeNativePanelPlacement({});
}

ArchDock::NativePanelPlacementResult PanelWindow::normalizedNativePanelPlacement(
    const ArchDock::PanelDefinition &definition,
    const QVariantMap &overrides) const
{
    const QString panelId = definition.identity.id;
    const QVariantMap stored = definition.toLegacyMap();
    const ArchDock::NativePanelPlacement defaults = ArchDock::defaultNativePanelPlacement();
    const auto value = [&stored, &overrides](const QString &key)
    {
        return overrides.contains(key)
            ? overrides.value(key)
            : stored.value(key);
    };
    ArchDock::NativePanelPlacementRequest request;
    request.screenStableId = value(QStringLiteral("screenId")).toString();
    request.screenFallbackIndex = overrides.contains(QStringLiteral("screen"))
        ? overrides.value(QStringLiteral("screen")).toInt()
        : definition.host.screenIndex;
    request.edge = value(QStringLiteral("edge")).toString();
    request.alignment = value(QStringLiteral("alignment")).toString();
    request.visibilityMode = value(QStringLiteral("visibilityMode")).toString();

    const bool vertical = request.edge == QStringLiteral("left") ||
        request.edge == QStringLiteral("right");
    const QVariant thickness = value(
        vertical ? QStringLiteral("width") : QStringLiteral("height"));
    const QVariant length = value(
        vertical ? QStringLiteral("height") : QStringLiteral("width"));
    request.thickness = thickness.isValid() ? thickness.toInt() : defaults.thickness;
    request.fixedLength = length.isValid() ? length.toInt() : defaults.fixedLength;
    request.dynamicLength = value(QStringLiteral("dynamic")).toBool();

    QList<ArchDock::EdgePanel> candidatePanels = edgePanels();
    for (ArchDock::EdgePanel &panel : candidatePanels)
    {
        if (panel.id != panelId)
        {
            continue;
        }
        panel.edge = request.edge;
        panel.screenIndex = request.screenFallbackIndex;
        panel.thickness = request.thickness;
        break;
    }
    request.offset = ArchDock::edgeOffset(candidatePanels, panelId);

    const QVariant floatingMargin = value(QStringLiteral("floatingMargin"));
    if (floatingMargin.isValid())
    {
        request.floatingMargin = floatingMargin.toInt();
    }
    return ArchDock::normalizeNativePanelPlacement(request);
}

QVariantMap PanelWindow::nativePanelPlacementIntent(const QString &panelId) const
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return {};
    }

    QVariantMap intent;
    static const QStringList keys{
        QStringLiteral("edge"),
        QStringLiteral("screen"),
        QStringLiteral("screenId"),
        QStringLiteral("alignment"),
        QStringLiteral("dynamic"),
        QStringLiteral("width"),
        QStringLiteral("height"),
        QStringLiteral("floatingMargin"),
    };
    for (const QString &key : keys)
    {
        const QVariant stored = m_panelRegistry.panelValue(panelId, key);
        if (stored.isValid())
        {
            intent.insert(key, stored);
        }
    }
    intent.insert(QStringLiteral("offset"), nativePanelOffset(panelId));
    return intent;
}

ArchDock::PlasmaPanelPlacementApplyResult PanelWindow::applyNativePanelPlacementTransaction(
    const QString &panelId,
    int containmentId,
    const QString &ownershipToken,
    const QVariantMap &values,
    bool persistIntent)
{
    ArchDock::PlasmaPanelPlacementApplyResult result;
    result.savedIntent = nativePanelPlacementIntent(panelId);
    const auto definition = persistIntent ? m_panelRegistry.panelDefinition(panelId)
                                          : runtimePanelDefinition(panelId);
    if (!definition || definition->host.kind != ArchDock::PanelHostKind::NativeEdge)
    {
        result.errorCode = QStringLiteral("placement-invalid-panel");
        return result;
    }
    if (containmentId < 0)
    {
        result.errorCode = QStringLiteral("containment-missing");
        return result;
    }
    if (ownershipToken.trimmed().isEmpty())
    {
        result.errorCode = QStringLiteral("ownership-token-missing");
        return result;
    }

    const ArchDock::NativePanelPlacementResult normalized =
        normalizedNativePanelPlacement(*definition, values);
    if (!normalized.isValid() || !normalized.placement.has_value())
    {
        result.status = QStringLiteral("failed");
        for (const ArchDock::NativePlacementIssue &issue : normalized.issues)
        {
            if (issue.kind != ArchDock::NativePlacementIssueKind::ValidationError)
            {
                continue;
            }
            ArchDock::PlasmaPanelFieldResult field;
            field.field = issue.field;
            field.requestedValue = values.value(
                ArchDock::nativePlacementFieldName(issue.field)).toString();
            field.failure = ArchDock::PlasmaPanelApplyFailure::InvalidRequest;
            result.fields.append(field);
            if (result.errorCode == QLatin1String("invalid-result"))
            {
                result.errorCode = QStringLiteral("validation-%1").arg(
                    nativePlacementIssueCodeName(issue.code));
            }
            qWarning() << "Native Plasma placement validation failed for" << panelId
                       << "field" << ArchDock::nativePlacementFieldName(issue.field)
                       << "issue" << nativePlacementIssueCodeName(issue.code);
        }
        if (result.errorCode == QLatin1String("invalid-result"))
        {
            result.errorCode = QStringLiteral("placement-invalid");
        }
        return result;
    }
    if (!normalized.isSupported())
    {
        result.status = QStringLiteral("unsupported");
        for (const ArchDock::NativePlacementIssue &issue : normalized.issues)
        {
            if (issue.kind == ArchDock::NativePlacementIssueKind::UnsupportedRequest)
            {
                ArchDock::PlasmaPanelFieldResult field;
                field.field = issue.field;
                field.requestedValue = values.value(
                    ArchDock::nativePlacementFieldName(issue.field)).toString();
                field.status = ArchDock::PlasmaPanelApplyStatus::Unsupported;
                field.failure = ArchDock::PlasmaPanelApplyFailure::PropertyUnsupported;
                result.fields.append(field);
                if (result.errorCode == QLatin1String("invalid-result"))
                {
                    result.errorCode = QStringLiteral("unsupported-%1").arg(
                        nativePlacementIssueCodeName(issue.code));
                }
                qWarning() << "Refusing unsupported normalized native panel placement for"
                           << panelId
                           << "field" << ArchDock::nativePlacementFieldName(issue.field)
                           << "issue" << nativePlacementIssueCodeName(issue.code);
            }
        }
        if (result.errorCode == QLatin1String("invalid-result"))
        {
            result.errorCode = QStringLiteral("placement-unsupported");
        }
        return result;
    }

    const ArchDock::PlasmaPanelAdapter adapter(
        [this](const QString &script)
        {
            return evaluatePlasmaScriptResultOptional(script);
        });
    ArchDock::PlasmaPanelPersistence persistence;
    if (persistIntent)
    {
        persistence = [this, panelId, values]
        {
            return m_panelRegistry.updatePanelChecked(panelId, values);
        };
    }
    result = adapter.applyPlacement(
        containmentId,
        panelId,
        ownershipToken,
        *normalized.placement,
        std::move(persistence));
    result.savedIntent = nativePanelPlacementIntent(panelId);

    for (const ArchDock::PlasmaPanelFieldResult &field : result.fields)
    {
        if (field.status == ArchDock::PlasmaPanelApplyStatus::Applied)
        {
            continue;
        }
        if (field.status == ArchDock::PlasmaPanelApplyStatus::Unsupported)
        {
            qWarning() << "Unsupported native Plasma placement field for" << panelId
                       << "field" << ArchDock::nativePlacementFieldName(field.field)
                       << "requested" << field.requestedValue
                       << "host" << field.hostValue.value_or(QStringLiteral("<unavailable>"))
                       << "reason" << ArchDock::plasmaPanelApplyFailureName(field.failure);
            continue;
        }

        qWarning() << "Failed to apply native Plasma placement field for" << panelId
                   << "field" << ArchDock::nativePlacementFieldName(field.field)
                   << "requested" << field.requestedValue
                   << "observed"
                   << field.actualValue.value_or(QStringLiteral("<unavailable>"))
                   << "final-host"
                   << field.hostValue.value_or(QStringLiteral("<unavailable>"))
                   << "reason" << ArchDock::plasmaPanelApplyFailureName(field.failure);
    }
    if (result.success())
    {
        qInfo() << "Native Plasma placement applied and read back for" << panelId
                << "host" << result.hostState;
    }
    else
    {
        qWarning() << "Native Plasma placement transaction ended for" << panelId
                   << "status" << result.status
                   << "error" << result.errorCode
                   << "rollback-attempted" << result.rollbackAttempted
                   << "rollback-succeeded" << result.rollbackSucceeded
                   << "rollback-error" << result.rollbackErrorCode
                   << "saved-intent" << result.savedIntent
                   << "final-host" << result.hostState;
    }
    return result;
}

QVariantMap PanelWindow::applyNativePanelPlacementDraft(const QString &panelId,
                                                        const QVariantMap &values)
{
    QVariantMap candidate = values;
    for (auto iterator = candidate.cbegin(); iterator != candidate.cend(); ++iterator)
    {
        if (ArchDock::PanelSettingsSchema::supportsMutationInterface(
                ArchDock::PanelSettingsFieldScope::Panel,
                iterator.key(),
                QStringLiteral("native-placement")))
        {
            continue;
        }
        ArchDock::PlasmaPanelPlacementApplyResult invalid;
        invalid.errorCode = QStringLiteral("unsupported-draft-key-%1").arg(iterator.key());
        recordNativePanelPlacementResult(panelId, invalid);
        return nativePanelPlacementStatus(panelId);
    }

    if (candidate.contains(QStringLiteral("floatingMargin")) &&
        candidate.value(QStringLiteral("floatingMargin")).toInt() > 0)
    {
        ArchDock::PlasmaPanelPlacementApplyResult unsupported;
        unsupported.status = QStringLiteral("unsupported");
        unsupported.errorCode = QStringLiteral("unsupported-capability-unavailable");
        ArchDock::PlasmaPanelFieldResult field;
        field.field = ArchDock::NativePlacementField::FloatingMargin;
        field.status = ArchDock::PlasmaPanelApplyStatus::Unsupported;
        field.requestedValue = candidate.value(
            QStringLiteral("floatingMargin")).toString();
        field.failure = ArchDock::PlasmaPanelApplyFailure::PropertyUnsupported;
        unsupported.fields.append(field);
        recordNativePanelPlacementResult(panelId, std::move(unsupported));
        return nativePanelPlacementStatus(panelId);
    }

    if (candidate.contains(QStringLiteral("screen")))
    {
        const int requestedScreen = candidate.value(QStringLiteral("screen")).toInt();
        const QList<QScreen *> screens = QGuiApplication::screens();
        if (requestedScreen < 0 || requestedScreen >= screens.size())
        {
            ArchDock::PlasmaPanelPlacementApplyResult invalid;
            invalid.errorCode = QStringLiteral("screen-out-of-range");
            recordNativePanelPlacementResult(panelId, invalid);
            return nativePanelPlacementStatus(panelId);
        }
        candidate.insert(
            QStringLiteral("screenId"),
            ArchDock::persistentScreenId(screens.at(requestedScreen)));
    }

    ArchDock::PlasmaPanelPlacementApplyResult result = applyNativePanelPlacementTransaction(
        panelId,
        nativePanelId(panelId),
        nativeOwnershipToken(panelId).trimmed(),
        candidate,
        true);
    recordNativePanelPlacementResult(panelId, std::move(result));
    return nativePanelPlacementStatus(panelId);
}

bool PanelWindow::applyNativePanelPlacement(const QString &panelId,
                                            int containmentId,
                                            const QString &ownershipToken,
                                            QString *errorCode)
{
    ArchDock::PlasmaPanelPlacementApplyResult result = applyNativePanelPlacementTransaction(
        panelId, containmentId, ownershipToken, {}, false);
    if (errorCode)
    {
        *errorCode = result.errorCode;
    }
    const bool success = result.success();
    recordNativePanelPlacementResult(panelId, std::move(result));
    return success;
}

bool PanelWindow::synchronizeNativePanelPlacement(const QString &panelId)
{
    const int containmentId = nativePanelId(panelId);
    if (containmentId < 0)
    {
        return true;
    }

    const QString ownershipToken = nativeOwnershipToken(panelId).trimmed();
    if (ownershipToken.isEmpty())
    {
        ArchDock::PlasmaPanelPlacementApplyResult failed;
        failed.errorCode = QStringLiteral("ownership-token-missing");
        recordNativePanelPlacementResult(panelId, std::move(failed));
        return false;
    }
    return applyNativePanelPlacement(panelId, containmentId, ownershipToken);
}

bool PanelWindow::removeLegacyControlApplets(const QString &panelId, int containmentId)
{
    if (!nativePanelIsOwned(panelId, containmentId))
    {
        return false;
    }

    const int removed = evaluatePlasmaScript(
        QStringLiteral(
        "var panel = panelById(%1);"
        "if (!panel) { print(-1); }"
        "else {"
        "var widgets = panel.widgets();"
        "var removed = 0;"
        "for (var index = 0; index < widgets.length; ++index) {"
        "var widget = panel.widgetById(widgets[index].id);"
        "if (widget && widget.type === 'org.archdock.control') {"
        "widget.currentConfigGroup = ['General'];"
        "if (widget.readConfig('panelId', '') === %2) { widget.remove(); ++removed; }"
        "}"
        "}"
        "print(removed);"
        "}")
        .arg(containmentId)
        .arg(plasmaScriptStringLiteral(panelId)));
    if (removed < 0)
    {
        return false;
    }

    if (nativeControlAppletId(panelId) >= 0)
    {
        m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeControlAppletId"), -1);
    }
    return true;
}

bool PanelWindow::attachNativeDockApplet(const QString &panelId, int containmentId)
{
    if (!nativePanelIsOwned(panelId, containmentId))
    {
        return false;
    }
    if (!removeLegacyControlApplets(panelId, containmentId))
    {
        return false;
    }

    const QString type = m_panelRegistry.panelValue(panelId, QStringLiteral("type")).toString();
    const int storedDockId = nativeDockAppletId(panelId);
    const bool storedDockIsOwned = nativeDockAppletIsOwned(panelId, containmentId, storedDockId);

    if (!panelTypeNeedsDockApplet(type))
    {
        if (storedDockIsOwned)
        {
            const int removed = evaluatePlasmaScript(
                QStringLiteral(
                    "var panel = panelById(%1);"
                    "var dock = panel ? panel.widgetById(%2) : null;"
                    "if (dock) { dock.remove(); }"
                    "print(1);")
                    .arg(containmentId)
                    .arg(storedDockId));
            if (removed != 1)
            {
                return false;
            }
        }
        if (storedDockId >= 0)
        {
            m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeDockAppletId"), -1);
        }
        return type == QStringLiteral("empty");
    }

    if (!isNativeDockPanelType(type))
    {
        return false;
    }

    if (storedDockIsOwned)
    {
        return evaluatePlasmaScript(
            QStringLiteral(
                "var panel = panelById(%1);"
                "var dock = panel ? panel.widgetById(%2) : null;"
                "if (!dock) { print(0); }"
                "else {"
                "dock.currentConfigGroup = ['General'];"
                "dock.writeConfig('panelType', %3);"
                "dock.reloadConfig();"
                "print(1);"
                "}")
                .arg(containmentId)
                .arg(storedDockId)
                .arg(plasmaScriptStringLiteral(type))) == 1;
    }

    if (storedDockId >= 0)
    {
        m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeDockAppletId"), -1);
    }

    const int dockId = evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print(-1); }"
            "else {"
            "var existing = panel.widgets('org.archdock.dock');"
            "if (existing.length !== 0) { print(-2); }"
            "else {"
            "var dock = panel.addWidget('org.archdock.dock');"
            "if (!dock) { print(-1); }"
            "else {"
            "dock.currentConfigGroup = ['General'];"
            "dock.writeConfig('panelId', %2);"
            "dock.writeConfig('panelType', %3);"
            "dock.reloadConfig();"
            "print(dock.id);"
            "}"
            "}"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(panelId))
            .arg(plasmaScriptStringLiteral(type)));
    if (dockId < 0)
    {
        qWarning() << (dockId == -2
                           ? "Refusing to attach another Arch Dock visual applet to panel"
                           : "Could not attach the Arch Dock visual applet to panel")
                   << panelId;
        return false;
    }

    m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeDockAppletId"), dockId);
    return true;
}

bool PanelWindow::createNativeKdePanel(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
    if (!isNativeDockPanelEdge(edge))
    {
        return false;
    }

    int storedContainmentId = nativePanelId(panelId);
    const QString panelType = m_panelRegistry.panelValue(
        panelId, QStringLiteral("type")).toString();
    const QString storedOwnershipToken = nativeOwnershipToken(panelId).trimmed();
    if (!storedOwnershipToken.isEmpty() &&
        !nativePanelIsOwned(panelId, storedContainmentId, storedOwnershipToken))
    {
        const NativePanelDiscoveryResult discovery = discoverNativePanel(
            panelId, storedOwnershipToken, panelType);
        switch (discovery.status)
        {
        case NativePanelDiscoveryStatus::QueryFailed:
            qWarning() << "Could not query owned native panels before creation for" << panelId;
            return false;
        case NativePanelDiscoveryStatus::HostConflict:
            if (!m_panelRegistry.recordNativePanelRecoveryConflict(
                    panelId, storedOwnershipToken, QStringLiteral("multiple-owned-hosts")))
            {
                qWarning() << "Could not persist native host conflict for" << panelId;
            }
            return false;
        case NativePanelDiscoveryStatus::RendererConflict:
            if (!m_panelRegistry.recordNativePanelRecoveryConflict(
                    panelId,
                    storedOwnershipToken,
                    QStringLiteral("multiple-or-unverified-renderers")))
            {
                qWarning() << "Could not persist native renderer conflict for" << panelId;
            }
            return false;
        case NativePanelDiscoveryStatus::Missing:
        {
            const bool visible = m_panelRegistry.panelValue(
                panelId, QStringLiteral("visible")).toBool();
            if (!m_panelRegistry.detachMissingNativePanelAssociation(
                    panelId, storedOwnershipToken, visible))
            {
                return false;
            }
            if (!visible)
            {
                return true;
            }
            storedContainmentId = -1;
            break;
        }
        case NativePanelDiscoveryStatus::Unique:
            if (!m_panelRegistry.rebindRecoveredNativePanelAssociation(
                    panelId,
                    discovery.containmentId,
                    discovery.dockAppletId,
                    storedOwnershipToken))
            {
                return false;
            }
            storedContainmentId = discovery.containmentId;
            break;
        }
    }

    const std::optional<bool> storedContainmentExists = nativePanelExistence(
        storedContainmentId);
    if (!storedContainmentExists.has_value())
    {
        qWarning() << "Could not query the stored native panel before creation for" << panelId;
        return false;
    }
    if (*storedContainmentExists)
    {
        if (!adoptNativePanelOwnership(panelId, storedContainmentId))
        {
            qWarning() << "Refusing to replace a present but unverified native panel for"
                       << panelId;
            return false;
        }
        return synchronizeNativePanelPlacement(panelId) &&
            attachNativeDockApplet(panelId, storedContainmentId);
    }

    if (m_panelRegistry.panelValue(
            panelId, QStringLiteral("nativeRecoveryError")).toString() ==
        QStringLiteral("candidate-rollback-failed"))
    {
        qWarning() << "Refusing another native panel candidate while rollback is unresolved for"
                   << panelId;
        return false;
    }

    const auto recordFailure = [this, &panelId](const QString &errorCode)
    {
        if (!m_panelRegistry.recordNativePanelRecoveryFailure(panelId, errorCode))
        {
            qWarning() << "Could not persist native panel recovery failure" << errorCode
                       << "for" << panelId;
        }
        return false;
    };
    const auto rollbackAndRecordFailure =
        [this, &panelId, &recordFailure](int containmentId,
                                         const QString &ownershipToken,
                                         const QString &errorCode)
    {
        if (!rollbackNativePanelCandidate(panelId, containmentId, ownershipToken))
        {
            qWarning() << "Could not roll back the verified native panel candidate for"
                       << panelId;
            return recordFailure(QStringLiteral("candidate-rollback-failed"));
        }
        return recordFailure(errorCode);
    };

    const QString ownershipToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString creationError;
    const int createdId = createNativePanelCandidate(panelId, ownershipToken, &creationError);
    if (createdId < 0)
    {
        return recordFailure(creationError.isEmpty()
                ? QStringLiteral("host-creation-failed")
                : creationError);
    }

    if (!nativePanelIsOwned(panelId, createdId, ownershipToken))
    {
        return rollbackAndRecordFailure(
            createdId, ownershipToken, QStringLiteral("ownership-verification-failed"));
    }

    QString placementError;
    if (!applyNativePanelPlacement(
            panelId, createdId, ownershipToken, &placementError))
    {
        return rollbackAndRecordFailure(
            createdId,
            ownershipToken,
            placementError.isEmpty()
                ? QStringLiteral("placement-apply-failed")
                : placementError);
    }

    const std::optional<int> dockAppletId = verifiedNativeDockAppletId(
        panelId, createdId, panelType);
    if (!dockAppletId.has_value())
    {
        return rollbackAndRecordFailure(
            createdId, ownershipToken, QStringLiteral("renderer-verification-failed"));
    }

    if (!m_panelRegistry.commitVerifiedNativePanelAssociation(
            panelId, createdId, *dockAppletId, ownershipToken))
    {
        return rollbackAndRecordFailure(
            createdId, ownershipToken, QStringLiteral("registry-persistence-failed"));
    }

    if (!m_nativePanelRecoveryActive)
    {
        scheduleNativePanelRecovery();
    }
    return true;
}

bool PanelWindow::addKdeWidget(const QString &panelId, const QString &appletId)
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

    if (!createNativeKdePanel(panelId))
    {
        return false;
    }

    const int containmentId = nativePanelId(panelId);
    const QString script = QStringLiteral(
        "var panel = panelById(%1);"
        "if (!panel) { print(-1); }"
        "else { var widget = panel.addWidget('%2'); print(widget ? widget.id : -1); }")
        .arg(containmentId)
        .arg(pluginId);
    const int widgetId = evaluatePlasmaScript(script);
    if (widgetId < 0)
    {
        return false;
    }

    QStringList widgets = m_panelRegistry.panelValue(panelId, QStringLiteral("kdeWidgets")).toStringList();
    if (!widgets.contains(pluginId))
    {
        widgets.append(pluginId);
        m_panelRegistry.setPanelValue(panelId, QStringLiteral("kdeWidgets"), widgets);
    }
    return true;
}

bool PanelWindow::removeNativeKdePanel(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    const QString edge = m_panelRegistry.panelValue(
        panelId, QStringLiteral("edge")).toString();
    const QString panelType = m_panelRegistry.panelValue(
        panelId, QStringLiteral("type")).toString();
    if (!isNativeDockPanelEdge(edge) || !isNativeDockPanelType(panelType))
    {
        return false;
    }

    const auto clearNativeAssociation = [this, &panelId]
    {
        m_panelRegistry.updatePanel(
            panelId,
            {{QStringLiteral("nativePanelId"), -1},
             {QStringLiteral("nativeControlAppletId"), -1},
             {QStringLiteral("nativeDockAppletId"), -1},
             {QStringLiteral("nativeOwnershipToken"), QString{}}});
    };

    const int containmentId = nativePanelId(panelId);
    if (containmentId < 0)
    {
        clearNativeAssociation();
        return true;
    }

    const std::optional<bool> containmentExists = nativePanelExistence(containmentId);
    if (!containmentExists.has_value())
    {
        qWarning() << "Could not query the native panel before permanent removal for"
                   << panelId;
        return false;
    }
    if (!*containmentExists)
    {
        clearNativeAssociation();
        return true;
    }

    if (!adoptNativePanelOwnership(panelId, containmentId))
    {
        qWarning() << "Refusing to permanently remove an unowned native panel for"
                   << panelId;
        return false;
    }

    const QString ownershipToken = nativeOwnershipToken(panelId).trimmed();
    if (ownershipToken.isEmpty() ||
        !nativePanelIsOwned(panelId, containmentId, ownershipToken))
    {
        qWarning() << "Refusing permanent removal without a verified ownership token for"
                   << panelId;
        return false;
    }

    const int storedDockAppletId = nativeDockAppletId(panelId);
    const std::optional<int> verifiedDockAppletId = verifiedNativeDockAppletId(
        panelId, containmentId, panelType);
    const bool rendererAttached = verifiedDockAppletId.has_value() &&
        (panelTypeNeedsDockApplet(panelType)
             ? storedDockAppletId >= 0 && *verifiedDockAppletId == storedDockAppletId
             : storedDockAppletId == -1 && *verifiedDockAppletId == -1);
    const ArchDock::NativeContainmentLifecycleState state{
        false,
        ArchDock::NativeContainmentHostStatus::Owned,
        rendererAttached,
        ArchDock::NativeContainmentPresentation::Hidden,
    };
    if (ArchDock::nativeContainmentLifecycleIntent(
            ArchDock::NativeContainmentLifecycleRequest::RemovePermanently,
            state) != ArchDock::NativeContainmentLifecycleIntent::RemoveHostPermanently)
    {
        qWarning() << "Refusing permanent removal without the expected native renderer for"
                   << panelId;
        return false;
    }

    const QString rendererVerification = panelTypeNeedsDockApplet(panelType)
        ? QStringLiteral(
              "var docks = panel.widgets('org.archdock.dock');"
              "if (docks.length !== 1) { return 0; }"
              "var dock = panel.widgetById(%1);"
              "if (!dock || dock.id !== %1 || dock.type !== 'org.archdock.dock') "
              "{ return 0; }"
              "dock.currentConfigGroup = ['General'];"
              "if (String(dock.readConfig('panelId', '')) !== %2 || "
              "String(dock.readConfig('panelType', '')) !== %3) { return 0; }")
              .arg(storedDockAppletId)
              .arg(plasmaScriptStringLiteral(panelId))
              .arg(plasmaScriptStringLiteral(panelType))
        : QStringLiteral(
              "if (panel.widgets('org.archdock.dock').length !== 0) { return 0; }");
    const int removed = evaluatePlasmaScriptResult(
        QStringLiteral(
            "var result = (function() {"
            "try {"
            "var panel = panelById(%1);"
            "if (!panel) { return 1; }"
            "panel.currentConfigGroup = ['ArchDock'];"
            "if (String(panel.readConfig('ownerToken', '')) !== %2 || "
            "String(panel.readConfig('panelId', '')) !== %3) { return 0; }"
            "%4"
            "panel.remove();"
            "return 1;"
            "} catch (error) { return -1; }"
            "})();"
            "print('ARCHDOCK_RESULT:' + String(result));")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(ownershipToken))
            .arg(plasmaScriptStringLiteral(panelId))
            .arg(rendererVerification));
    if (removed != 1)
    {
        qWarning() << "Plasma refused the verified permanent native panel removal for"
                   << panelId;
        return false;
    }

    const std::optional<bool> removedContainmentExists = nativePanelExistence(containmentId);
    if (!removedContainmentExists.has_value() || *removedContainmentExists)
    {
        qWarning() << "Could not verify permanent native panel removal for" << panelId;
        return false;
    }

    clearNativeAssociation();
    return true;
}

void PanelWindow::removePanel(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    if (m_panelRegistry.isBuiltIn(panelId))
    {
        if (!setPanelVisible(panelId, false))
        {
            qWarning() << "Could not hide built-in native panel" << panelId;
        }
        return;
    }

    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() ==
        QStringLiteral("free"))
    {
        ArchDock::FreePanelController controller(
            m_panelRegistry, freePanelHostOperations());
        const ArchDock::FreePanelLifecycleResult result = controller.remove(panelId);
        if (!result.success)
        {
            qWarning() << "Preserving the free-panel record after host removal was refused for"
                       << panelId << result.errorCode;
            return;
        }
        updateDesktopSuite();
        return;
    }

    if (!removeNativeKdePanel(panelId))
    {
        qWarning() << "Preserving the panel record after native removal was refused for"
                   << panelId;
        return;
    }
    m_panelRegistry.removePanel(panelId);
    updateDesktopSuite();
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
    presentUtilityWindow(m_iconPropertiesWindow);
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

void PanelWindow::presentUtilityWindow(QWindow *window)
{
    if (!window)
    {
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen)
    {
        window->setScreen(screen);
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
    m_settings.reset();
    syncRegistryFromLegacySettings();
}

void PanelWindow::toggleAutoHide()
{
    const bool autoHide = m_panelRegistry.panelValue(
        QStringLiteral("bottom"),
        QStringLiteral("visibilityMode")).toString() == QStringLiteral("auto-hide");
    setPanelVisibilityMode(
        QStringLiteral("bottom"),
        autoHide ? QStringLiteral("always") : QStringLiteral("auto-hide"));
}

void PanelWindow::toggleDesktopSuite()
{
    m_settings.setDesktopSuite(!m_settings.desktopSuite());
    syncRegistryFromLegacySettings();
}

void PanelWindow::toggleTopLauncher()
{
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
