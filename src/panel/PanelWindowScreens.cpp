// PanelWindow: screens and presentation. Which screen each panel is on (kept
// across screen changes by stable screen ids), whether a panel is shown, and
// the presentation state and interaction guards the applets report, which
// decide when a panel may conceal itself.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

#include "../PanelVisibility.h"
#include "../ScreenIdentity.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QSet>
#include <QTimer>

#include <array>

using PanelWindowHelpers::fitUtilityWindow;

namespace
{
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
    if (profileBusy()) return;

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
    if (profileBusy()) return false;

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
    if (profileBusy()) return;

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

void PanelWindow::watchScreen(QScreen *screen)
{
    if (!screen) return;
    const auto schedule = [this] {
        m_screenChangePending = true;
        m_screenChangeTimer.start();
    };
    connect(screen, &QScreen::geometryChanged, this, schedule);
    connect(screen, &QScreen::availableGeometryChanged, this, schedule);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, schedule);
    connect(screen, &QScreen::physicalDotsPerInchChanged, this, schedule);
}

void PanelWindow::handleScreensChanged()
{
    if (m_presetAudition && m_presetAudition->active())
    {
        const auto preview = m_presetAudition->previewDefinition(
            m_presetAudition->status().value(QStringLiteral("panelId")).toString());
        const auto screens = QGuiApplication::screens();
        if (preview && !preview->host.screenId.isEmpty() &&
            std::none_of(screens.cbegin(), screens.cend(), [&preview](const QScreen *screen) {
                return ArchDock::persistentScreenId(screen) == preview->host.screenId;
            }))
        {
            const auto cancelled = m_presetAudition->cancel();
            if (!cancelled.value(QStringLiteral("success")).toBool())
                qWarning() << "Could not recover preset audition after output removal:"
                           << cancelled.value(QStringLiteral("errorCode")).toString();
        }
    }
    if (profileBusy()) return;
    m_screenChangePending = false;

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
    fitUtilityWindow(m_settingsWindow);
    fitUtilityWindow(m_iconPropertiesWindow);
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
                // Recovery's native script wait dispatches incoming backend
                // calls so a synchronous Plasma drop can finish. A later
                // timer must not reenter that recovery or complete a stale one.
                if (generation != m_nativePanelRecoveryGeneration || m_nativePanelRecoveryActive)
                {
                    return;
                }

                recoverNativePanels(finalAttempt);
                if (finalAttempt && generation == m_nativePanelRecoveryGeneration)
                {
                    emit nativePanelRecoveryFinished();
                }
            });
    }
}

void PanelWindow::recoverNativePanels(bool allowMissingHostRecovery)
{
    if (profileBusy()) return;

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
