// PanelWindow: free panels. Each free panel is an Arch Dock applet that Plasma
// hosts on the desktop; this creates, finds, verifies, configures and removes
// those hosts through Plasma scripts and keeps the registry in step with
// them.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

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
    if (profileBusy()) return;

    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() != QStringLiteral("free"))
        return;
    m_panelRegistry.updatePanel(panelId, {
        {QStringLiteral("x"), qMax(0, x)},
        {QStringLiteral("y"), qMax(0, y)}});
}
