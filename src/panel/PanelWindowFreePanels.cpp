// PanelWindow: free panels. Each free panel is an Arch Dock applet that Plasma
// hosts on the desktop; this creates, finds, verifies, configures and removes
// those hosts through Plasma scripts and keeps the registry in step with
// them.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

#include "../persistence/RemovedPanelStore.h"
#include "../presets/PresetApplication.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QScreen>

#include <limits>

using PanelWindowHelpers::plasmaScriptStringLiteral;

namespace
{
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

void PanelWindow::synchronizeFreePanels()
{
    if (profileBusy()) return;

    ArchDock::FreePanelController controller(
        m_panelRegistry, freePanelHostOperations());
    // A free panel goes away only when its owner removes its widget (see
    // checkRequestedRemovals); a profile keeps its own copy. A widget that is
    // simply gone - Plasma lost it, say over a restart - is put back at the
    // panel's place with its current settings, once it has been missing on
    // checks at least half a minute apart. For ninety seconds after the
    // backend or Plasma starts nothing is put back, since Plasma may not
    // have restored its layout yet.
    const bool grace = m_freeHostGraceClock.isValid() && m_freeHostGraceClock.elapsed() < 90000;
    bool recheck = false;
    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
                QStringLiteral("free") ||
            m_removalRequested.contains(panelId))
        {
            continue;
        }

        const ArchDock::FreePanelLifecycleResult result = controller.synchronize(panelId, false);
        if (result.success && result.outcome == ArchDock::FreePanelLifecycleOutcome::Deferred)
        {
            const auto missing = m_missingFreeHosts.constFind(panelId);
            if (missing == m_missingFreeHosts.constEnd())
            {
                QElapsedTimer since;
                since.start();
                m_missingFreeHosts.insert(panelId, since);
                qInfo() << "The desktop widget of free panel" << panelId
                        << "is not in Plasma's layout; checking again before putting it back.";
                recheck = true;
            }
            else if (grace || missing->elapsed() < 30000)
            {
                recheck = true;
            }
            else if (!m_restoredFreeHosts.contains(panelId))
            {
                m_restoredFreeHosts.insert(panelId);
                restoreFreePanelHost(controller, panelId);
            }
            continue;
        }
        m_missingFreeHosts.remove(panelId);
        if (!result.success)
        {
            qWarning() << "Could not synchronize the free-panel Plasma host for"
                       << panelId << result.errorCode;
        }
        // Detached: left behind by an older version, which kept a panel
        // whose widget was gone.
        else if (result.outcome == ArchDock::FreePanelLifecycleOutcome::Detached)
        {
            // Its settings are kept first; without a kept copy it stays.
            const QVariantMap kept = m_panelRegistry.panelSnapshot(panelId);
            const QString cause = QStringLiteral("an older version had left it off the desktop");
            QString keepError;
            const QString keptKey = keepRemovedPanelSettings(kept, cause, &keepError);
            if (keptKey.isEmpty())
                qWarning() << "Free panel" << panelId << "that an older version left off the desktop stays:"
                           << "its settings could not be kept:" << keepError;
            else if (m_panelRegistry.removeDetachedFreePanel(panelId))
                reportRemovedPanel(kept, cause);
            else
            {
                forgetKeptPanelSettings(keptKey);
                qWarning() << "Could not remove free panel" << panelId
                           << "that an older version left off the desktop.";
            }
        }
    }
    if (recheck && !m_freeHostRetryTimer.isActive())
    {
        m_freeHostRetryTimer.start();
    }
}

void PanelWindow::restoreFreePanelHost(const ArchDock::FreePanelController &controller,
                                       const QString &panelId)
{
    // The panel's saved screen where it still exists, else the dock's screen.
    const QList<QScreen *> screens = QGuiApplication::screens();
    const int savedScreen = m_panelRegistry.panelValue(panelId, QStringLiteral("screen")).toInt();
    const int screenIndex = screens.isEmpty()
        ? -1
        : qBound(0, savedScreen >= 0 && savedScreen < screens.size() ? savedScreen : m_settings.monitorIndex(),
                 screens.size() - 1);
    const ArchDock::FreePanelCreationResult result = controller.restore(panelId, screenIndex);
    if (!result.success)
    {
        qWarning() << "Could not put free panel" << panelId << "back on the desktop:"
                   << result.errorCode << result.failureStage << result.rollbackErrorCode;
        return;
    }
    m_missingFreeHosts.remove(panelId);
    qInfo() << "The desktop widget of free panel" << panelId
            << "was gone from Plasma; the panel is back on the desktop.";
    moveFreePanelToSavedPlace(panelId);
}

void PanelWindow::moveFreePanelToSavedPlace(const QString &panelId)
{
    // Where it stood, at its size. A refused move leaves the widget where
    // Plasma placed it.
    if (const auto definition = runtimePanelDefinition(panelId))
    {
        const QVariantMap legacy = definition->toLegacyMap();
        const QVariantMap geometry{
            {QStringLiteral("height"), legacy.value(QStringLiteral("height"), 420).toDouble()},
            {QStringLiteral("width"), legacy.value(QStringLiteral("width"), 420).toDouble()},
            {QStringLiteral("x"), legacy.value(QStringLiteral("x"), 0).toDouble()},
            {QStringLiteral("y"), legacy.value(QStringLiteral("y"), 0).toDouble()}};
        QString geometryError;
        if (!setPresetFreeHostGeometry(*definition, geometry, &geometryError))
            qWarning() << "Free panel" << panelId << "is back on the desktop but not at its place:"
                       << geometryError;
    }
}

void PanelWindow::panelRemovalRequested(const QString &panelId, bool removing)
{
    panelRemovalReported(panelId, removing, -1);
}

void PanelWindow::panelRemovalReported(const QString &panelId, bool removing, int appletId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }
    // A bar of Arch Dock's own is removed by Arch Dock only: its widget
    // going away (the bar host closing, or rebuilding the bar) says nothing.
    if (barHosted(panelId))
    {
        return;
    }
    // Only the panel's own widget speaks for it. A second copy of the panel
    // (one put back while Plasma later restored the first, say) or a widget
    // Arch Dock itself took away going, or coming back, is not the panel
    // being removed or kept.
    const bool free = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() ==
        QStringLiteral("free");
    const int ownApplet = free
        ? m_panelRegistry.panelValue(panelId, QStringLiteral("freeDockAppletId")).toInt()
        : m_panelRegistry.panelValue(panelId, QStringLiteral("nativeDockAppletId")).toInt();
    if (appletId >= 0 && ownApplet >= 0 && appletId != ownApplet)
    {
        if (removing)
            qInfo().noquote() << QStringLiteral("A second copy of panel %1 (widget %2) was removed from Plasma; "
                                                "the panel itself (widget %3) stays.")
                                     .arg(panelId).arg(appletId).arg(ownApplet);
        return;
    }
    if (!removing)
    {
        m_removalRequested.remove(panelId);
        return;
    }
    // Plasma (or Arch Dock) has only just started and is still setting up
    // its layout: a widget going away now is not its owner removing the
    // panel. If the widget is really gone, the panel is put back like any
    // lost one.
    if (m_freeHostGraceClock.isValid() && m_freeHostGraceClock.elapsed() < 90000)
    {
        qInfo() << "Panel" << panelId
                << "was reported removed while Plasma or Arch Dock was starting; the report is not acted on.";
        // Looked at again shortly, so that a widget or Plasma panel that is
        // really gone is put back.
        if (free)
        {
            if (!m_freeHostRetryTimer.isActive())
                m_freeHostRetryTimer.start();
        }
        else
        {
            scheduleNativePanelRecovery();
        }
        return;
    }
    PendingRemoval pending;
    pending.appletId = appletId;
    pending.received.start();
    m_removalRequested.insert(panelId, pending);
    if (!m_removalCheckTimer.isActive())
    {
        m_removalCheckTimer.start();
    }
}

void PanelWindow::checkRequestedRemovals()
{
    if (profileBusy()) return;

    ArchDock::FreePanelController controller(
        m_panelRegistry, freePanelHostOperations());
    // Plasma may still be loading its layout after it or Arch Dock started:
    // a widget not found yet is not a widget deleted.
    const bool grace = m_freeHostGraceClock.isValid() && m_freeHostGraceClock.elapsed() < 90000;
    const QStringList requested = m_removalRequested.keys();
    for (const QString &panelId : requested)
    {
        // The calls below ask Plasma and can let other events run, which may
        // add or drop reports: each is read as a copy and written back.
        if (!m_removalRequested.contains(panelId))
            continue;
        if (!m_panelRegistry.panelIds().contains(panelId))
        {
            m_removalRequested.remove(panelId);
            continue;
        }
        PendingRemoval pending = m_removalRequested.value(panelId);
        const auto keepWaiting = [this, &panelId](const PendingRemoval &state)
        {
            if (state.received.elapsed() > 10 * 60 * 1000)
            {
                m_removalRequested.remove(panelId);
                qInfo() << "Panel" << panelId
                        << "stays: ten minutes after it was reported removed, the removal is neither"
                        << "confirmed nor finished.";
                return;
            }
            const auto found = m_removalRequested.find(panelId);
            if (found != m_removalRequested.end())
                *found = state;
        };
        if (grace)
        {
            keepWaiting(pending);
            continue;
        }
        // Gone means deleted by Plasma: while a removal can still be undone,
        // the widget or the Plasma panel is still there.
        const bool free = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() ==
            QStringLiteral("free");
        if (free)
        {
            const ArchDock::FreePanelLifecycleResult result = controller.synchronize(panelId, false);
            if (!result.success)
            {
                // Plasma did not answer, or answered twice: nothing is decided.
                keepWaiting(pending);
                continue;
            }
            // Deferred: its widget is not in Plasma's layout. Detached: an
            // earlier try here let go of it already, then could not finish.
            if (result.outcome != ArchDock::FreePanelLifecycleOutcome::Deferred &&
                result.outcome != ArchDock::FreePanelLifecycleOutcome::Detached)
            {
                // Still on the desktop: the removal was undone, or never was.
                pending.missingSince.invalidate();
                keepWaiting(pending);
                continue;
            }
        }
        else
        {
            // A panel with no Plasma panel recorded cannot be found gone.
            const int native = nativePanelId(panelId);
            const std::optional<bool> exists = native >= 0 ? nativePanelExistence(native) : std::nullopt;
            if (!exists.has_value() || *exists)
            {
                if (exists.has_value())
                    pending.missingSince.invalidate();
                keepWaiting(pending);
                continue;
            }
        }
        // Gone: confirmed only by a second look at least half a minute later,
        // with Plasma's desktops loaded.
        if (!pending.missingSince.isValid())
        {
            pending.missingSince.start();
            keepWaiting(pending);
            continue;
        }
        if (pending.missingSince.elapsed() < 30000 || !plasmaDesktopsLoaded())
        {
            keepWaiting(pending);
            continue;
        }
        // From here the report stays until the panel is gone from Arch Dock
        // too: a step that fails is tried again at the next look, and the
        // panel is not put back meanwhile.
        if (!free && m_panelRegistry.isBuiltIn(panelId))
        {
            // Bottom, Top and Side stay in the panel list, hidden.
            if (!m_panelRegistry.updatePanelChecked(panelId, {{QStringLiteral("visible"), false}}))
            {
                qWarning() << "Could not hide edge panel" << panelId
                           << "after its Plasma panel was removed; trying again shortly.";
                keepWaiting(pending);
                continue;
            }
            m_removalRequested.remove(panelId);
            qInfo() << "Edge panel" << panelId << "is hidden: its Plasma panel was removed.";
            continue;
        }
        // Its settings are kept before its record goes; without a kept copy
        // nothing is deleted (the report is looked at again later).
        const QVariantMap kept = m_panelRegistry.panelSnapshot(panelId);
        const QString cause = free ? QStringLiteral("its widget was removed in Plasma")
                                   : QStringLiteral("its Plasma panel was removed");
        QString keepError;
        const QString keptKey = keepRemovedPanelSettings(kept, cause, &keepError);
        if (keptKey.isEmpty())
        {
            qWarning().noquote() << QStringLiteral("Panel %1 is not removed: its settings could not be kept (%2).")
                                        .arg(panelId, keepError);
            pending.missingSince.invalidate();
            keepWaiting(pending);
            continue;
        }
        bool removed = false;
        if (free)
        {
            const ArchDock::FreePanelLifecycleResult detached = controller.synchronize(panelId, true);
            removed = detached.success && detached.outcome == ArchDock::FreePanelLifecycleOutcome::Detached
                && m_panelRegistry.removeDetachedFreePanel(panelId);
            m_missingFreeHosts.remove(panelId);
        }
        else
        {
            m_panelRegistry.removePanel(panelId);
            removed = !m_panelRegistry.panelIds().contains(panelId);
        }
        if (removed)
        {
            m_removalRequested.remove(panelId);
            reportRemovedPanel(kept, cause);
        }
        else
        {
            forgetKeptPanelSettings(keptKey);
            qWarning() << "Could not remove panel" << panelId << "after Plasma removed it; trying again shortly.";
            keepWaiting(pending);
        }
    }
    if (m_removalRequested.isEmpty())
    {
        m_removalCheckTimer.stop();
    }
    updateDesktopSuite();
}

bool PanelWindow::plasmaDesktopsLoaded() const
{
    const std::optional<int> count = evaluatePlasmaScriptResultOptional(
        QStringLiteral("print('ARCHDOCK_RESULT:' + String(desktops().length));"));
    return count.has_value() && *count > 0;
}

QString PanelWindow::keepRemovedPanelSettings(const QVariantMap &record, const QString &cause,
                                              QString *error)
{
    if (record.isEmpty())
    {
        if (error) *error = QStringLiteral("removed-panel-record-empty");
        return {};
    }
    ArchDock::RemovedPanelStore store;
    return store.keep(record, cause, error);
}

void PanelWindow::forgetKeptPanelSettings(const QString &key)
{
    ArchDock::RemovedPanelStore store;
    QString error;
    if (!store.forget(key, &error))
        qWarning() << "A kept copy of a panel that was not removed stays listed:" << error;
}

void PanelWindow::reportRemovedPanel(const QVariantMap &record, const QString &cause)
{
    // The removal has gone through: only now does the oldest kept panel
    // make room for this one.
    ArchDock::RemovedPanelStore store;
    QString trimError;
    if (!store.trim(&trimError))
        qWarning() << "The list of removed panels could not be shortened:" << trimError;
    qInfo().noquote() << QStringLiteral("Panel \"%1\" (%2) is removed: %3. Its settings are kept; "
                                        "Panel Studio can bring it back (Panels, then Bring back).")
                             .arg(record.value(QStringLiteral("name")).toString(),
                                  record.value(QStringLiteral("id")).toString(), cause);
}

QVariantMap PanelWindow::createFreePanel()
{
    if (profileBusy()) return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")}};

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
    if (profileBusy()) return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")}};

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
    if (profileBusy()) return {{QStringLiteral("success"), false}, {QStringLiteral("errorCode"), QStringLiteral("profile-recovery-or-apply-active")}};

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
    operations.panelHostCount = [this](const QString &panelId)
    {
        return freePanelWidgetCount(panelId);
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

// Every Arch Dock widget on a desktop that shows this panel, whatever its
// owner token: a panel is never given a second widget.
std::optional<int> PanelWindow::freePanelWidgetCount(const QString &panelId) const
{
    if (panelId.isEmpty())
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
        if (String(dock.readConfig("panelId", "")) === %1)
            ++matches;
    }
}
print("ARCHDOCK_RESULT:" + String(matches));
)JS")
                               .arg(plasmaScriptStringLiteral(panelId));
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

std::optional<int> PanelWindow::removeEveryOwnedFreePanelHost(
    const QString &panelId,
    const QString &ownershipToken) const
{
    if (panelId.isEmpty() || ownershipToken.isEmpty())
    {
        return std::nullopt;
    }
    // Every widget showing this panel under its token, the same match as
    // discovery: used when the owner removes a panel that has two copies on
    // the desktop. Answers how many were removed. Plasma takes a widget away
    // after the script has run, so the answer is the count removed, not a
    // count of what is left; nothing is removed (and -1 answered) while the
    // desktop or a copy is locked, when Plasma would refuse.
    const QString script = QStringLiteral(R"JS(
const allDesktops = desktops();
const found = [];
let locked = false;
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
            found.push(dock);
            locked = locked || desktop.locked === true || dock.locked === true;
        }
    }
}
if (!locked)
    for (const dock of found)
        dock.remove();
print("ARCHDOCK_RESULT:" + String(locked ? -1 : found.length));
)JS")
                               .arg(plasmaScriptStringLiteral(panelId))
                               .arg(plasmaScriptStringLiteral(ownershipToken));
    const std::optional<int> removed = evaluatePlasmaScriptResultOptional(script);
    return removed.has_value() && *removed >= 0 ? removed : std::nullopt;
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
    if (profileBusy()) return;

    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() != QStringLiteral("free"))
        return;
    m_panelRegistry.updatePanel(panelId, {
        {QStringLiteral("x"), qMax(0, x)},
        {QStringLiteral("y"), qMax(0, y)}});
}
