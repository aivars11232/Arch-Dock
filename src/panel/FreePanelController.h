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

struct FreePanelCreationResult
{
    bool success = false;
    QString status = QStringLiteral("failed");
    QString errorCode;
    QString panelId;
    int desktopContainmentId = -1;
    int dockAppletId = -1;
    bool ownershipVerified = false;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

class FreePanelController final
{
public:
    struct HostOperations
    {
        std::function<std::optional<int>(int, const QString &)> verifiedBridgeScreen;
        std::function<std::optional<FreePanelHost>(int, const QString &, const QString &)>
            createConfiguredHost;
        std::function<std::optional<int>(int, int, const QString &, const QString &)>
            configureAdoptedHost;
        std::function<bool(int, int, const QString &, const QString &)> verifyHost;
        std::function<bool(int, int, const QString &, const QString &)> removeOwnedHost;
        std::function<bool(int, const QString &)> removeVerifiedBridge;
        std::function<QString(int)> screenIdForIndex;
    };

    FreePanelController(PanelRegistry &registry, HostOperations operations);

    [[nodiscard]] FreePanelCreationResult create(const FreePanelCreationRequest &request);

private:
    [[nodiscard]] FreePanelCreationResult failure(const QString &errorCode) const;

    PanelRegistry &m_registry;
    HostOperations m_operations;
};
}
