#pragma once

#include <QString>
#include <QVariantMap>

#include <functional>
#include <optional>

class PanelRegistry;

namespace ArchDock
{
enum class FreePanelCreationOrigin
{
    Studio,
    TemplateBridge,
    ExistingApplet,
};

enum class FreePanelCreationIntent
{
    CreateHost,
    AdoptHost,
    ReturnExisting,
    Reject,
};

struct FreePanelCreationDecisionState
{
    FreePanelCreationOrigin origin = FreePanelCreationOrigin::Studio;
    bool requestValid = false;
    int matchingAssociationCount = 0;
    bool matchingAssociationVerified = false;
};

[[nodiscard]] FreePanelCreationIntent freePanelCreationIntent(
    const FreePanelCreationDecisionState &state);

struct FreePanelCreationRequest
{
    FreePanelCreationOrigin origin = FreePanelCreationOrigin::Studio;
    int screenIndex = -1;
    int bridgeContainmentId = -1;
    int existingDesktopContainmentId = -1;
    int existingDockAppletId = -1;
    QString ownershipToken;
};

struct FreePanelHost
{
    int desktopContainmentId = -1;
    int dockAppletId = -1;
};

enum class FreePanelHostDiscoveryOutcome
{
    Unique,
    Missing,
    Conflict,
    QueryFailed,
};

struct FreePanelHostDiscoveryResult
{
    FreePanelHostDiscoveryOutcome outcome = FreePanelHostDiscoveryOutcome::QueryFailed;
    FreePanelHost host;
    int screenIndex = -1;
};

enum class FreePanelHostMutationOutcome
{
    Verified,
    NoHost,
    CandidateUnverified,
    Indeterminate,
};

struct FreePanelHostMutationResult
{
    FreePanelHostMutationOutcome outcome = FreePanelHostMutationOutcome::Indeterminate;
    FreePanelHost host;
    int screenIndex = -1;
};

enum class FreePanelHostVerificationOutcome
{
    Owned,
    Missing,
    UnownedOrUnverified,
    QueryFailed,
};

enum class FreePanelRemovalOutcome
{
    Removed,
    AlreadyAbsent,
    Refused,
    QueryFailed,
};

enum class FreePanelLifecycleOutcome
{
    Rebound,
    Detached,
    Removed,
    AlreadyAbsent,
    InvalidRecord,
    Conflict,
    Refused,
    QueryFailed,
    PersistenceFailed,
};

struct FreePanelLifecycleResult
{
    bool success = false;
    FreePanelLifecycleOutcome outcome = FreePanelLifecycleOutcome::InvalidRecord;
    QString errorCode;
};

struct FreePanelCreationResult
{
    bool success = false;
    QString status = QStringLiteral("failed");
    QString errorCode;
    QString panelId;
    int desktopContainmentId = -1;
    int dockAppletId = -1;
    bool ownershipVerified = false;
    QString failureStage;
    bool rollbackAttempted = false;
    bool rollbackSucceeded = false;
    QString rollbackErrorCode;
    bool recoverable = false;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

class FreePanelController final
{
public:
    struct HostOperations
    {
        std::function<std::optional<int>(int, const QString &)> verifiedBridgeScreen;
        std::function<std::optional<int>(const QString &, const QString &)>
            matchingHostCount;
        std::function<FreePanelHostDiscoveryResult(const QString &, const QString &)>
            discoverOwnedHost;
        std::function<FreePanelHostMutationResult(int, const QString &, const QString &)>
            createConfiguredHost;
        std::function<FreePanelHostMutationResult(int, int, const QString &, const QString &)>
            configureAdoptedHost;
        std::function<FreePanelHostVerificationOutcome(
            int, int, const QString &, const QString &)> verifyHost;
        std::function<FreePanelRemovalOutcome(
            int, int, const QString &, const QString &)> removeOwnedHost;
        std::function<FreePanelRemovalOutcome(const QString &, const QString &)>
            removeOwnedHostByIdentity;
        std::function<FreePanelRemovalOutcome(int, const QString &)> removeVerifiedBridge;
        std::function<QString(int)> screenIdForIndex;
    };

    FreePanelController(PanelRegistry &registry, HostOperations operations);

    [[nodiscard]] FreePanelCreationResult create(const FreePanelCreationRequest &request);
    [[nodiscard]] FreePanelLifecycleResult synchronize(const QString &panelId) const;
    [[nodiscard]] FreePanelLifecycleResult remove(const QString &panelId) const;

private:
    [[nodiscard]] FreePanelCreationResult failure(
        const QString &errorCode,
        const QString &failureStage = {}) const;
    [[nodiscard]] FreePanelCreationResult rollBack(
        const QString &errorCode,
        const QString &failureStage,
        const QString &panelId,
        const QString &ownershipToken,
        const FreePanelHost &host,
        int screenIndex,
        const QString &screenId,
        bool hostMayExist,
        bool removeByIdentity,
        bool ownershipVerified) const;

    PanelRegistry &m_registry;
    HostOperations m_operations;
};
}
