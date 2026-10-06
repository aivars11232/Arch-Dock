// PanelWindow: where native panels stand and when they hide. Placement (edge,
// alignment, length, thickness, offset) and visibility modes are normalized,
// applied to the Plasma panel and recorded with their outcome, and panels are
// hidden or restored to follow the visibility policy.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

#include "../NativeContainmentLifecycle.h"
#include "../PanelPlacement.h"
#include "../PanelVisibility.h"
#include "../ScreenIdentity.h"
#include "../integration/PlasmaPanelAdapter.h"
#include "../model/PanelSettingsSchema.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

#include <utility>

using PanelWindowHelpers::isNativeDockPanelEdge;
using PanelWindowHelpers::panelTypeNeedsDockApplet;

namespace
{
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
        {QStringLiteral("status"), QStringLiteral("not-attempted")},
        {QStringLiteral("errorCode"), QString{}},
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
        {QStringLiteral("errorCode"), QString{}},
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

QVariantMap PanelWindow::applyNativePanelVisibilityMode(
    const QString &panelId,
    const QString &visibilityMode)
{
    if (profileBusy()) return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")}};

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
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition || definition->host.kind != ArchDock::PanelHostKind::NativeEdge)
    {
        return ArchDock::PanelVisibilityDecision::Reveal;
    }

    return nativePanelVisibilityDecision(*definition, mode, visible);
}

ArchDock::PanelVisibilityDecision PanelWindow::nativePanelVisibilityDecision(
    const ArchDock::PanelDefinition &definition,
    ArchDock::PanelVisibilityMode mode, bool visible) const
{
    const auto screens = QGuiApplication::screens();
    QStringList ids;
    for (const auto *screen : screens) ids.append(ArchDock::persistentScreenId(screen));
    const int screenIndex = ArchDock::resolvedScreenIndex(ids, definition.host.screenId, definition.host.screenIndex);
    const QScreen *screen = screenIndex >= 0 && screenIndex < screens.size() ? screens.at(screenIndex) : nullptr;
    const ArchDock::NativePanelPlacementResult placementResult =
        normalizedNativePanelPlacement(definition);
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
    input.locks = m_panelInteractionGuards.value(definition.identity.id);
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
    if (profileBusy()) return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")}};

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
