#include "FreePanelController.h"

#include "../PanelRegistry.h"

#include <QUuid>

#include <utility>

namespace
{
constexpr auto kTemplateTokenPrefix = "archdock-free-template-";
constexpr int kMaximumOwnershipTokenLength = 96;

bool requestIsValid(const ArchDock::FreePanelCreationRequest &request)
{
    switch (request.origin)
    {
    case ArchDock::FreePanelCreationOrigin::Studio:
        return request.screenIndex >= 0;
    case ArchDock::FreePanelCreationOrigin::TemplateBridge:
        return request.bridgeContainmentId >= 0 &&
            request.ownershipToken.startsWith(QLatin1String(kTemplateTokenPrefix)) &&
            request.ownershipToken.size() <= kMaximumOwnershipTokenLength;
    case ArchDock::FreePanelCreationOrigin::ExistingApplet:
        return request.existingDesktopContainmentId >= 0 &&
            request.existingDockAppletId >= 0;
    }
    return false;
}

QString generatedOwnershipToken()
{
    return QStringLiteral("archdock-free-") +
        QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}

namespace ArchDock
{
FreePanelCreationIntent freePanelCreationIntent(
    const FreePanelCreationDecisionState &state)
{
    if (!state.requestValid || state.matchingAssociationCount < 0)
    {
        return FreePanelCreationIntent::Reject;
    }

    switch (state.origin)
    {
    case FreePanelCreationOrigin::Studio:
        return FreePanelCreationIntent::CreateHost;
    case FreePanelCreationOrigin::ExistingApplet:
        return FreePanelCreationIntent::AdoptHost;
    case FreePanelCreationOrigin::TemplateBridge:
        if (state.matchingAssociationCount == 0)
        {
            return FreePanelCreationIntent::CreateHost;
        }
        if (state.matchingAssociationCount == 1 && state.matchingAssociationVerified)
        {
            return FreePanelCreationIntent::ReturnExisting;
        }
        return FreePanelCreationIntent::Reject;
    }
    return FreePanelCreationIntent::Reject;
}

QVariantMap FreePanelCreationResult::toVariantMap() const
{
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("status"), status},
        {QStringLiteral("errorCode"), errorCode},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("desktopContainmentId"), desktopContainmentId},
        {QStringLiteral("dockAppletId"), dockAppletId},
        {QStringLiteral("ownershipVerified"), ownershipVerified},
    };
}

FreePanelController::FreePanelController(PanelRegistry &registry, HostOperations operations)
    : m_registry(registry),
      m_operations(std::move(operations))
{
}

FreePanelCreationResult FreePanelController::failure(const QString &errorCode) const
{
    FreePanelCreationResult result;
    result.errorCode = errorCode;
    return result;
}

FreePanelCreationResult FreePanelController::create(const FreePanelCreationRequest &request)
{
    const bool validRequest = requestIsValid(request);
    if (!validRequest)
    {
        return failure(QStringLiteral("invalid-request"));
    }

    QString matchingPanelId;
    std::optional<PanelRegistry::FreeHostAssociation> matchingAssociation;
    int matchingAssociationCount = 0;
    if (request.origin == FreePanelCreationOrigin::TemplateBridge)
    {
        for (const QString &panelId : m_registry.panelIds())
        {
            const auto association = m_registry.freeHostAssociation(panelId);
            if (!association.has_value() ||
                association->ownershipToken != request.ownershipToken)
            {
                continue;
            }

            ++matchingAssociationCount;
            matchingPanelId = panelId;
            matchingAssociation = association;
        }
    }

    const bool matchingAssociationVerified = matchingAssociationCount == 1 &&
        matchingAssociation.has_value() &&
        matchingAssociation->state == PanelRegistry::FreeHostState::HostedOwned &&
        m_operations.verifyHost &&
        m_operations.verifyHost(
            matchingAssociation->desktopContainmentId,
            matchingAssociation->dockAppletId,
            matchingPanelId,
            request.ownershipToken);

    const FreePanelCreationIntent intent = freePanelCreationIntent(
        {request.origin,
         validRequest,
         matchingAssociationCount,
         matchingAssociationVerified});

    if (intent == FreePanelCreationIntent::Reject)
    {
        return failure(matchingAssociationCount == 0
            ? QStringLiteral("invalid-request")
            : QStringLiteral("bootstrap-association-conflict"));
    }

    if (intent == FreePanelCreationIntent::ReturnExisting)
    {
        FreePanelCreationResult result;
        result.panelId = matchingPanelId;
        result.desktopContainmentId = matchingAssociation->desktopContainmentId;
        result.dockAppletId = matchingAssociation->dockAppletId;
        result.ownershipVerified = true;
        if (!m_operations.removeVerifiedBridge ||
            !m_operations.removeVerifiedBridge(
                request.bridgeContainmentId, request.ownershipToken))
        {
            result.errorCode = QStringLiteral("bootstrap-cleanup-failed");
            return result;
        }
        result.success = true;
        result.status = QStringLiteral("existing");
        return result;
    }

    int screenIndex = request.screenIndex;
    if (request.origin == FreePanelCreationOrigin::TemplateBridge)
    {
        if (!m_operations.verifiedBridgeScreen)
        {
            return failure(QStringLiteral("operation-unavailable"));
        }
        const std::optional<int> verifiedScreen = m_operations.verifiedBridgeScreen(
            request.bridgeContainmentId, request.ownershipToken);
        if (!verifiedScreen.has_value() || *verifiedScreen < 0)
        {
            return failure(QStringLiteral("bootstrap-bridge-unverified"));
        }
        screenIndex = *verifiedScreen;
    }

    const QString panelId = m_registry.addFreePanel();
    if (panelId.isEmpty())
    {
        return failure(QStringLiteral("record-allocation-failed"));
    }

    const QString ownershipToken = request.origin == FreePanelCreationOrigin::TemplateBridge
        ? request.ownershipToken
        : generatedOwnershipToken();
    FreePanelHost host;
    bool createdHost = false;

    if (intent == FreePanelCreationIntent::AdoptHost)
    {
        if (!m_operations.configureAdoptedHost)
        {
            m_registry.removePanel(panelId);
            return failure(QStringLiteral("operation-unavailable"));
        }
        const std::optional<int> adoptedScreen = m_operations.configureAdoptedHost(
            request.existingDesktopContainmentId,
            request.existingDockAppletId,
            panelId,
            ownershipToken);
        if (!adoptedScreen.has_value() || *adoptedScreen < 0)
        {
            m_registry.removePanel(panelId);
            return failure(QStringLiteral("host-adoption-unverified"));
        }
        screenIndex = *adoptedScreen;
        host = {request.existingDesktopContainmentId, request.existingDockAppletId};
    }
    else
    {
        if (screenIndex < 0 || !m_operations.createConfiguredHost)
        {
            m_registry.removePanel(panelId);
            return failure(QStringLiteral("operation-unavailable"));
        }
        const std::optional<FreePanelHost> created = m_operations.createConfiguredHost(
            screenIndex, panelId, ownershipToken);
        if (!created.has_value() || created->desktopContainmentId < 0 ||
            created->dockAppletId < 0)
        {
            m_registry.removePanel(panelId);
            return failure(QStringLiteral("host-creation-failed"));
        }
        host = *created;
        createdHost = true;
    }

    const QString screenId = m_operations.screenIdForIndex
        ? m_operations.screenIdForIndex(screenIndex)
        : QString{};
    if (!m_registry.commitVerifiedFreeHostAssociation(
            panelId,
            host.desktopContainmentId,
            host.dockAppletId,
            ownershipToken,
            screenIndex,
            screenId,
            QStringLiteral("desktop")))
    {
        if (createdHost && m_operations.removeOwnedHost &&
            m_operations.removeOwnedHost(
                host.desktopContainmentId,
                host.dockAppletId,
                panelId,
                ownershipToken))
        {
            m_registry.removePanel(panelId);
        }

        FreePanelCreationResult result = failure(
            QStringLiteral("association-persist-failed"));
        result.panelId = panelId;
        result.desktopContainmentId = host.desktopContainmentId;
        result.dockAppletId = host.dockAppletId;
        result.ownershipVerified = true;
        return result;
    }

    if (request.origin == FreePanelCreationOrigin::TemplateBridge &&
        (!m_operations.removeVerifiedBridge ||
         !m_operations.removeVerifiedBridge(
             request.bridgeContainmentId, request.ownershipToken)))
    {
        FreePanelCreationResult result = failure(
            QStringLiteral("bootstrap-cleanup-failed"));
        result.panelId = panelId;
        result.desktopContainmentId = host.desktopContainmentId;
        result.dockAppletId = host.dockAppletId;
        result.ownershipVerified = true;
        return result;
    }

    FreePanelCreationResult result;
    result.success = true;
    result.status = intent == FreePanelCreationIntent::AdoptHost
        ? QStringLiteral("adopted")
        : QStringLiteral("created");
    result.panelId = panelId;
    result.desktopContainmentId = host.desktopContainmentId;
    result.dockAppletId = host.dockAppletId;
    result.ownershipVerified = true;
    return result;
}
}
