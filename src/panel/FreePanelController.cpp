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

bool validHost(const ArchDock::FreePanelHost &host)
{
    return host.desktopContainmentId >= 0 && host.dockAppletId >= 0;
}

bool removalCompleted(ArchDock::FreePanelRemovalOutcome outcome)
{
    return outcome == ArchDock::FreePanelRemovalOutcome::Removed ||
        outcome == ArchDock::FreePanelRemovalOutcome::AlreadyAbsent;
}

QString removalErrorCode(ArchDock::FreePanelRemovalOutcome outcome)
{
    switch (outcome)
    {
    case ArchDock::FreePanelRemovalOutcome::Removed:
    case ArchDock::FreePanelRemovalOutcome::AlreadyAbsent:
        return {};
    case ArchDock::FreePanelRemovalOutcome::Refused:
        return QStringLiteral("host-rollback-refused");
    case ArchDock::FreePanelRemovalOutcome::QueryFailed:
        return QStringLiteral("host-rollback-query-failed");
    }
    return QStringLiteral("host-rollback-query-failed");
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
        {QStringLiteral("failureStage"), failureStage},
        {QStringLiteral("rollbackAttempted"), rollbackAttempted},
        {QStringLiteral("rollbackSucceeded"), rollbackSucceeded},
        {QStringLiteral("rollbackErrorCode"), rollbackErrorCode},
        {QStringLiteral("recoverable"), recoverable},
    };
}

FreePanelController::FreePanelController(PanelRegistry &registry, HostOperations operations)
    : m_registry(registry),
      m_operations(std::move(operations))
{
}

FreePanelCreationResult FreePanelController::failure(
    const QString &errorCode,
    const QString &failureStage) const
{
    FreePanelCreationResult result;
    result.errorCode = errorCode;
    result.failureStage = failureStage;
    return result;
}

FreePanelCreationResult FreePanelController::rollBack(
    const QString &errorCode,
    const QString &failureStage,
    const QString &panelId,
    const QString &ownershipToken,
    const FreePanelHost &host,
    int screenIndex,
    const QString &screenId,
    bool hostMayExist,
    bool removeByIdentity,
    bool ownershipVerified) const
{
    FreePanelCreationResult result = failure(errorCode, failureStage);
    result.panelId = panelId;
    result.desktopContainmentId = host.desktopContainmentId;
    result.dockAppletId = host.dockAppletId;
    result.ownershipVerified = ownershipVerified;
    result.rollbackAttempted = true;

    bool hostResolved = !hostMayExist;
    if (hostMayExist)
    {
        FreePanelRemovalOutcome removalOutcome = FreePanelRemovalOutcome::QueryFailed;
        if (removeByIdentity)
        {
            if (m_operations.removeOwnedHostByIdentity)
            {
                removalOutcome = m_operations.removeOwnedHostByIdentity(
                    panelId, ownershipToken);
            }
        }
        else if (validHost(host) && m_operations.removeOwnedHost)
        {
            removalOutcome = m_operations.removeOwnedHost(
                host.desktopContainmentId,
                host.dockAppletId,
                panelId,
                ownershipToken);
        }
        hostResolved = removalCompleted(removalOutcome);
        result.rollbackErrorCode = removalErrorCode(removalOutcome);
        if (removeByIdentity &&
            removalOutcome == FreePanelRemovalOutcome::AlreadyAbsent)
        {
            hostResolved = false;
            result.rollbackErrorCode =
                QStringLiteral("host-rollback-absence-unverified");
        }
    }

    if (hostResolved && m_registry.discardFreePanelCreation(panelId, ownershipToken))
    {
        result.status = QStringLiteral("rolled-back");
        result.rollbackSucceeded = true;
        result.panelId.clear();
        result.desktopContainmentId = -1;
        result.dockAppletId = -1;
        result.ownershipVerified = false;
        return result;
    }

    if (hostResolved)
    {
        result.rollbackErrorCode = QStringLiteral("record-discard-failed");
    }
    if (result.rollbackErrorCode.isEmpty())
    {
        result.rollbackErrorCode = QStringLiteral("rollback-unverified");
    }

    if (!m_registry.recordRecoverableFreePanelCreation(
            panelId,
            host.desktopContainmentId,
            host.dockAppletId,
            ownershipToken,
            screenIndex,
            screenId,
            errorCode,
            result.rollbackErrorCode))
    {
        result.rollbackErrorCode += QStringLiteral("+recovery-record-persist-failed");
    }

    const auto retainedAssociation = m_registry.freeHostAssociation(panelId);
    result.status = QStringLiteral("rollback-pending");
    result.recoverable = retainedAssociation.has_value() &&
        retainedAssociation->ownershipToken == ownershipToken;
    return result;
}

FreePanelLifecycleResult FreePanelController::synchronize(const QString &panelId) const
{
    const auto association = m_registry.freeHostAssociation(panelId);
    if (!association.has_value())
    {
        return {false,
                FreePanelLifecycleOutcome::InvalidRecord,
                QStringLiteral("free-record-not-found")};
    }

    if (association->state == PanelRegistry::FreeHostState::Detached &&
        association->desktopContainmentId < 0 && association->dockAppletId < 0 &&
        association->ownershipToken.trimmed().isEmpty())
    {
        return {true, FreePanelLifecycleOutcome::Detached, {}};
    }

    const QString ownershipToken = association->ownershipToken.trimmed();
    if (ownershipToken.isEmpty())
    {
        return {false,
                FreePanelLifecycleOutcome::InvalidRecord,
                QStringLiteral("free-ownership-token-missing")};
    }

    const auto recordError = [this, &panelId, &ownershipToken](
                                 FreePanelLifecycleOutcome outcome,
                                 const QString &errorCode)
    {
        if (!m_registry.recordFreeHostRecoveryError(
                panelId, ownershipToken, errorCode))
        {
            return FreePanelLifecycleResult{
                false,
                FreePanelLifecycleOutcome::PersistenceFailed,
                QStringLiteral("free-recovery-error-persist-failed")};
        }
        return FreePanelLifecycleResult{false, outcome, errorCode};
    };

    if (!m_operations.discoverOwnedHost)
    {
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            QStringLiteral("free-host-discovery-unavailable"));
    }

    const FreePanelHostDiscoveryResult discovery = m_operations.discoverOwnedHost(
        panelId, ownershipToken);
    switch (discovery.outcome)
    {
    case FreePanelHostDiscoveryOutcome::Missing:
        if (!m_registry.detachFreeHostAssociation(
                panelId, ownershipToken, QStringLiteral("owned-host-not-found")))
        {
            return {false,
                    FreePanelLifecycleOutcome::PersistenceFailed,
                    QStringLiteral("free-host-detach-persist-failed")};
        }
        return {true, FreePanelLifecycleOutcome::Detached, {}};
    case FreePanelHostDiscoveryOutcome::Conflict:
        return recordError(
            FreePanelLifecycleOutcome::Conflict,
            QStringLiteral("owned-host-conflict"));
    case FreePanelHostDiscoveryOutcome::QueryFailed:
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            QStringLiteral("owned-host-query-failed"));
    case FreePanelHostDiscoveryOutcome::Unique:
        break;
    }

    if (!validHost(discovery.host) || !m_operations.verifyHost)
    {
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            QStringLiteral("owned-host-discovery-invalid"));
    }

    const FreePanelHostVerificationOutcome verification = m_operations.verifyHost(
        discovery.host.desktopContainmentId,
        discovery.host.dockAppletId,
        panelId,
        ownershipToken);
    if (verification != FreePanelHostVerificationOutcome::Owned)
    {
        if (verification == FreePanelHostVerificationOutcome::UnownedOrUnverified)
        {
            return recordError(
                FreePanelLifecycleOutcome::Refused,
                QStringLiteral("owned-host-verification-refused"));
        }
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            verification == FreePanelHostVerificationOutcome::Missing
                ? QStringLiteral("owned-host-changed-during-recovery")
                : QStringLiteral("owned-host-verification-query-failed"));
    }

    const int screenIndex = discovery.screenIndex >= 0
        ? discovery.screenIndex
        : association->screenIndex;
    const QString screenId = m_operations.screenIdForIndex
        ? m_operations.screenIdForIndex(screenIndex)
        : association->screenId;
    if (!m_registry.rebindRecoveredFreeHostAssociation(
            panelId,
            discovery.host.desktopContainmentId,
            discovery.host.dockAppletId,
            ownershipToken,
            screenIndex,
            screenId))
    {
        return {false,
                FreePanelLifecycleOutcome::PersistenceFailed,
                QStringLiteral("free-host-rebind-persist-failed")};
    }

    return {true, FreePanelLifecycleOutcome::Rebound, {}};
}

FreePanelLifecycleResult FreePanelController::remove(const QString &panelId) const
{
    const auto association = m_registry.freeHostAssociation(panelId);
    if (!association.has_value())
    {
        return {false,
                FreePanelLifecycleOutcome::InvalidRecord,
                QStringLiteral("free-record-not-found")};
    }

    if (association->state == PanelRegistry::FreeHostState::Detached &&
        association->desktopContainmentId < 0 && association->dockAppletId < 0 &&
        association->ownershipToken.trimmed().isEmpty())
    {
        if (!m_registry.removeDetachedFreePanel(panelId))
        {
            return {false,
                    FreePanelLifecycleOutcome::PersistenceFailed,
                    QStringLiteral("free-record-removal-persist-failed")};
        }
        return {true, FreePanelLifecycleOutcome::AlreadyAbsent, {}};
    }

    const QString ownershipToken = association->ownershipToken.trimmed();
    if (ownershipToken.isEmpty())
    {
        return {false,
                FreePanelLifecycleOutcome::InvalidRecord,
                QStringLiteral("free-ownership-token-missing")};
    }

    const auto recordError = [this, &panelId, &ownershipToken](
                                 FreePanelLifecycleOutcome outcome,
                                 const QString &errorCode)
    {
        if (!m_registry.recordFreeHostRecoveryError(
                panelId, ownershipToken, errorCode))
        {
            return FreePanelLifecycleResult{
                false,
                FreePanelLifecycleOutcome::PersistenceFailed,
                QStringLiteral("free-removal-error-persist-failed")};
        }
        return FreePanelLifecycleResult{false, outcome, errorCode};
    };
    const auto finalizeRemoval = [this, &panelId, &ownershipToken](
                                     FreePanelLifecycleOutcome outcome,
                                     const QString &detachReason)
    {
        if (!m_registry.detachFreeHostAssociation(
                panelId, ownershipToken, detachReason))
        {
            return FreePanelLifecycleResult{
                false,
                FreePanelLifecycleOutcome::PersistenceFailed,
                QStringLiteral("free-host-detach-persist-failed")};
        }
        if (!m_registry.removeDetachedFreePanel(panelId))
        {
            return FreePanelLifecycleResult{
                false,
                FreePanelLifecycleOutcome::PersistenceFailed,
                QStringLiteral("free-record-removal-persist-failed")};
        }
        return FreePanelLifecycleResult{true, outcome, {}};
    };

    if (!m_operations.discoverOwnedHost)
    {
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            QStringLiteral("free-host-discovery-unavailable"));
    }

    const FreePanelHostDiscoveryResult discovery = m_operations.discoverOwnedHost(
        panelId, ownershipToken);
    switch (discovery.outcome)
    {
    case FreePanelHostDiscoveryOutcome::Missing:
        return finalizeRemoval(
            FreePanelLifecycleOutcome::AlreadyAbsent,
            QStringLiteral("owned-host-not-found"));
    case FreePanelHostDiscoveryOutcome::Conflict:
        return recordError(
            FreePanelLifecycleOutcome::Conflict,
            QStringLiteral("owned-host-conflict"));
    case FreePanelHostDiscoveryOutcome::QueryFailed:
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            QStringLiteral("owned-host-query-failed"));
    case FreePanelHostDiscoveryOutcome::Unique:
        break;
    }

    if (!validHost(discovery.host) || !m_operations.verifyHost ||
        !m_operations.removeOwnedHost)
    {
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            QStringLiteral("owned-host-operation-unavailable"));
    }

    const FreePanelHostVerificationOutcome verification = m_operations.verifyHost(
        discovery.host.desktopContainmentId,
        discovery.host.dockAppletId,
        panelId,
        ownershipToken);
    if (verification != FreePanelHostVerificationOutcome::Owned)
    {
        if (verification == FreePanelHostVerificationOutcome::UnownedOrUnverified)
        {
            return recordError(
                FreePanelLifecycleOutcome::Refused,
                QStringLiteral("owned-host-verification-refused"));
        }
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            verification == FreePanelHostVerificationOutcome::Missing
                ? QStringLiteral("owned-host-changed-before-removal")
                : QStringLiteral("owned-host-verification-query-failed"));
    }

    const int screenIndex = discovery.screenIndex >= 0
        ? discovery.screenIndex
        : association->screenIndex;
    const QString screenId = m_operations.screenIdForIndex
        ? m_operations.screenIdForIndex(screenIndex)
        : association->screenId;
    if (!m_registry.rebindRecoveredFreeHostAssociation(
            panelId,
            discovery.host.desktopContainmentId,
            discovery.host.dockAppletId,
            ownershipToken,
            screenIndex,
            screenId))
    {
        return {false,
                FreePanelLifecycleOutcome::PersistenceFailed,
                QStringLiteral("free-host-rebind-persist-failed")};
    }

    const FreePanelRemovalOutcome removal = m_operations.removeOwnedHost(
        discovery.host.desktopContainmentId,
        discovery.host.dockAppletId,
        panelId,
        ownershipToken);
    switch (removal)
    {
    case FreePanelRemovalOutcome::Removed:
        return finalizeRemoval(FreePanelLifecycleOutcome::Removed, {});
    case FreePanelRemovalOutcome::AlreadyAbsent:
        return finalizeRemoval(
            FreePanelLifecycleOutcome::AlreadyAbsent,
            QStringLiteral("owned-host-not-found"));
    case FreePanelRemovalOutcome::Refused:
        return recordError(
            FreePanelLifecycleOutcome::Refused,
            QStringLiteral("owned-host-removal-refused"));
    case FreePanelRemovalOutcome::QueryFailed:
        return recordError(
            FreePanelLifecycleOutcome::QueryFailed,
            QStringLiteral("owned-host-removal-query-failed"));
    }

    return recordError(
        FreePanelLifecycleOutcome::QueryFailed,
        QStringLiteral("owned-host-removal-query-failed"));
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

    const QString matchingCreationState = matchingAssociationCount == 1
        ? m_registry.panelValue(
              matchingPanelId, QStringLiteral("freeCreationState")).toString()
        : QString{};
    const bool matchingAssociationComplete = matchingCreationState.isEmpty() ||
        matchingCreationState == QStringLiteral("complete");
    const bool matchingAssociationVerified = matchingAssociationCount == 1 &&
        matchingAssociation.has_value() &&
        matchingAssociationComplete &&
        matchingAssociation->state == PanelRegistry::FreeHostState::HostedOwned &&
        m_operations.verifyHost &&
        m_operations.verifyHost(
            matchingAssociation->desktopContainmentId,
            matchingAssociation->dockAppletId,
            matchingPanelId,
            request.ownershipToken) == FreePanelHostVerificationOutcome::Owned;

    const FreePanelCreationIntent intent = freePanelCreationIntent(
        {request.origin,
         validRequest,
         matchingAssociationCount,
         matchingAssociationVerified});

    if (intent == FreePanelCreationIntent::Reject)
    {
        return failure(
            matchingAssociationCount == 0
                ? QStringLiteral("invalid-request")
                : QStringLiteral("bootstrap-association-conflict"),
            QStringLiteral("intent-selection"));
    }

    if (intent == FreePanelCreationIntent::ReturnExisting)
    {
        FreePanelCreationResult result;
        result.panelId = matchingPanelId;
        result.desktopContainmentId = matchingAssociation->desktopContainmentId;
        result.dockAppletId = matchingAssociation->dockAppletId;
        result.ownershipVerified = true;
        const FreePanelRemovalOutcome bridgeOutcome = m_operations.removeVerifiedBridge
            ? m_operations.removeVerifiedBridge(
                  request.bridgeContainmentId, request.ownershipToken)
            : FreePanelRemovalOutcome::QueryFailed;
        if (!removalCompleted(bridgeOutcome))
        {
            result.errorCode = bridgeOutcome == FreePanelRemovalOutcome::Refused
                ? QStringLiteral("bootstrap-cleanup-refused")
                : QStringLiteral("bootstrap-cleanup-indeterminate");
            result.failureStage = QStringLiteral("bootstrap-cleanup");
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
            return failure(
                QStringLiteral("operation-unavailable"),
                QStringLiteral("bridge-verification"));
        }
        const std::optional<int> verifiedScreen = m_operations.verifiedBridgeScreen(
            request.bridgeContainmentId, request.ownershipToken);
        if (!verifiedScreen.has_value() || *verifiedScreen < 0)
        {
            return failure(
                QStringLiteral("bootstrap-bridge-unverified"),
                QStringLiteral("bridge-verification"));
        }
        screenIndex = *verifiedScreen;
    }

    const QString ownershipToken = request.origin == FreePanelCreationOrigin::TemplateBridge
        ? request.ownershipToken
        : generatedOwnershipToken();
    const QString panelId = m_registry.beginFreePanelCreation(ownershipToken);
    if (panelId.isEmpty())
    {
        return failure(
            QStringLiteral("record-allocation-failed"),
            QStringLiteral("record-allocation"));
    }

    QString screenId = m_operations.screenIdForIndex
        ? m_operations.screenIdForIndex(screenIndex)
        : QString{};
    if (!m_operations.matchingHostCount)
    {
        return rollBack(
            QStringLiteral("operation-unavailable"),
            QStringLiteral("host-preflight"),
            panelId,
            ownershipToken,
            {},
            screenIndex,
            screenId,
            false,
            false,
            false);
    }
    const std::optional<int> preexistingMatches = m_operations.matchingHostCount(
        panelId, ownershipToken);
    if (!preexistingMatches.has_value())
    {
        return rollBack(
            QStringLiteral("host-preflight-query-failed"),
            QStringLiteral("host-preflight"),
            panelId,
            ownershipToken,
            {},
            screenIndex,
            screenId,
            false,
            false,
            false);
    }
    if (*preexistingMatches != 0)
    {
        return rollBack(
            QStringLiteral("host-identity-conflict"),
            QStringLiteral("host-preflight"),
            panelId,
            ownershipToken,
            {},
            screenIndex,
            screenId,
            false,
            false,
            false);
    }

    FreePanelHostMutationResult mutation;

    if (intent == FreePanelCreationIntent::AdoptHost)
    {
        if (!m_operations.configureAdoptedHost)
        {
            return rollBack(
                QStringLiteral("operation-unavailable"),
                QStringLiteral("host-adoption"),
                panelId,
                ownershipToken,
                {},
                screenIndex,
                screenId,
                false,
                false,
                false);
        }
        mutation = m_operations.configureAdoptedHost(
            request.existingDesktopContainmentId,
            request.existingDockAppletId,
            panelId,
            ownershipToken);
        if (!validHost(mutation.host))
        {
            mutation.host = {
                request.existingDesktopContainmentId,
                request.existingDockAppletId};
        }
    }
    else
    {
        if (screenIndex < 0 || !m_operations.createConfiguredHost)
        {
            return rollBack(
                QStringLiteral("operation-unavailable"),
                QStringLiteral("host-creation"),
                panelId,
                ownershipToken,
                {},
                screenIndex,
                screenId,
                false,
                false,
                false);
        }
        mutation = m_operations.createConfiguredHost(
            screenIndex, panelId, ownershipToken);
    }

    const QString mutationStage = intent == FreePanelCreationIntent::AdoptHost
        ? QStringLiteral("host-adoption")
        : QStringLiteral("host-creation");
    const QString mutationErrorPrefix = intent == FreePanelCreationIntent::AdoptHost
        ? QStringLiteral("host-adoption")
        : QStringLiteral("host-creation");
    if (mutation.outcome != FreePanelHostMutationOutcome::Verified ||
        !validHost(mutation.host))
    {
        QString mutationError = mutationErrorPrefix + QStringLiteral("-failed");
        bool hostMayExist = false;
        if (mutation.outcome == FreePanelHostMutationOutcome::CandidateUnverified)
        {
            mutationError = mutationErrorPrefix + QStringLiteral("-unverified");
            hostMayExist = true;
        }
        else if (mutation.outcome == FreePanelHostMutationOutcome::Indeterminate)
        {
            mutationError = mutationErrorPrefix + QStringLiteral("-indeterminate");
            hostMayExist = true;
        }
        return rollBack(
            mutationError,
            mutationStage,
            panelId,
            ownershipToken,
            mutation.host,
            mutation.screenIndex >= 0 ? mutation.screenIndex : screenIndex,
            screenId,
            hostMayExist,
            !validHost(mutation.host),
            false);
    }

    screenIndex = mutation.screenIndex >= 0 ? mutation.screenIndex : screenIndex;
    screenId = m_operations.screenIdForIndex
        ? m_operations.screenIdForIndex(screenIndex)
        : QString{};
    const FreePanelHost host = mutation.host;
    const FreePanelHostVerificationOutcome initialVerification = m_operations.verifyHost
        ? m_operations.verifyHost(
              host.desktopContainmentId,
              host.dockAppletId,
              panelId,
              ownershipToken)
        : FreePanelHostVerificationOutcome::QueryFailed;
    if (initialVerification != FreePanelHostVerificationOutcome::Owned)
    {
        const bool definitelyMissing =
            initialVerification == FreePanelHostVerificationOutcome::Missing;
        const QString verificationError = initialVerification ==
                FreePanelHostVerificationOutcome::QueryFailed
            ? QStringLiteral("host-verification-query-failed")
            : initialVerification == FreePanelHostVerificationOutcome::Missing
                ? QStringLiteral("host-verification-missing")
                : QStringLiteral("host-verification-refused");
        return rollBack(
            verificationError,
            QStringLiteral("host-verification"),
            panelId,
            ownershipToken,
            host,
            screenIndex,
            screenId,
            !definitelyMissing,
            false,
            false);
    }

    if (!m_registry.commitVerifiedFreeHostAssociation(
            panelId,
            host.desktopContainmentId,
            host.dockAppletId,
            ownershipToken,
            screenIndex,
            screenId,
            QStringLiteral("desktop")))
    {
        return rollBack(
            QStringLiteral("association-persist-failed"),
            QStringLiteral("association-persistence"),
            panelId,
            ownershipToken,
            host,
            screenIndex,
            screenId,
            true,
            false,
            true);
    }

    const auto persistedAssociation = m_registry.freeHostAssociation(panelId);
    if (!persistedAssociation.has_value() ||
        persistedAssociation->state != PanelRegistry::FreeHostState::HostedOwned ||
        persistedAssociation->desktopContainmentId != host.desktopContainmentId ||
        persistedAssociation->dockAppletId != host.dockAppletId ||
        persistedAssociation->ownershipToken != ownershipToken ||
        persistedAssociation->screenIndex != screenIndex ||
        persistedAssociation->screenId != screenId ||
        persistedAssociation->hostMode != QStringLiteral("desktop"))
    {
        return rollBack(
            QStringLiteral("association-readback-failed"),
            QStringLiteral("association-readback"),
            panelId,
            ownershipToken,
            host,
            screenIndex,
            screenId,
            true,
            false,
            true);
    }

    if (request.origin == FreePanelCreationOrigin::TemplateBridge)
    {
        const FreePanelRemovalOutcome bridgeOutcome = m_operations.removeVerifiedBridge
            ? m_operations.removeVerifiedBridge(
                  request.bridgeContainmentId, request.ownershipToken)
            : FreePanelRemovalOutcome::QueryFailed;
        if (!removalCompleted(bridgeOutcome))
        {
            return rollBack(
                bridgeOutcome == FreePanelRemovalOutcome::Refused
                    ? QStringLiteral("bootstrap-cleanup-refused")
                    : QStringLiteral("bootstrap-cleanup-indeterminate"),
                QStringLiteral("bootstrap-cleanup"),
                panelId,
                ownershipToken,
                host,
                screenIndex,
                screenId,
                true,
                false,
                true);
        }
    }

    const FreePanelHostVerificationOutcome finalVerification = m_operations.verifyHost
        ? m_operations.verifyHost(
              host.desktopContainmentId,
              host.dockAppletId,
              panelId,
              ownershipToken)
        : FreePanelHostVerificationOutcome::QueryFailed;
    if (finalVerification != FreePanelHostVerificationOutcome::Owned)
    {
        const bool definitelyMissing =
            finalVerification == FreePanelHostVerificationOutcome::Missing;
        return rollBack(
            finalVerification == FreePanelHostVerificationOutcome::QueryFailed
                ? QStringLiteral("final-host-readback-query-failed")
                : finalVerification == FreePanelHostVerificationOutcome::Missing
                    ? QStringLiteral("final-host-readback-missing")
                    : QStringLiteral("final-host-readback-refused"),
            QStringLiteral("final-readback"),
            panelId,
            ownershipToken,
            host,
            screenIndex,
            screenId,
            !definitelyMissing,
            false,
            false);
    }

    if (!m_registry.completeFreePanelCreation(panelId, ownershipToken))
    {
        return rollBack(
            QStringLiteral("creation-completion-persist-failed"),
            QStringLiteral("transaction-completion"),
            panelId,
            ownershipToken,
            host,
            screenIndex,
            screenId,
            true,
            false,
            true);
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
