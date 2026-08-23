#pragma once

#include "../PanelPlacement.h"
#include "../PanelVisibility.h"

#include <QList>
#include <QString>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace ArchDock
{
enum class PlasmaPanelApplyStatus
{
    Applied,
    Unsupported,
    Failed,
};

enum class PlasmaPanelApplyFailure
{
    None,
    InvalidRequest,
    ContainmentMissing,
    OwnershipDenied,
    PropertyUnsupported,
    ScriptFailure,
    ReadbackMismatch,
};

struct PlasmaPanelFieldResult
{
    NativePlacementField field = NativePlacementField::Edge;
    PlasmaPanelApplyStatus status = PlasmaPanelApplyStatus::Failed;
    QString requestedValue;
    std::optional<QString> actualValue;
    std::optional<QString> hostValue;
    PlasmaPanelApplyFailure failure = PlasmaPanelApplyFailure::ScriptFailure;
};

struct PlasmaPanelPlacementApplyResult
{
    bool ownershipVerified = false;
    QList<PlasmaPanelFieldResult> fields;
    QString status = QStringLiteral("failed");
    QString errorCode = QStringLiteral("invalid-result");
    QVariantMap savedIntent;
    QVariantMap hostState;
    bool rollbackAttempted = false;
    bool rollbackSucceeded = false;
    QString rollbackErrorCode;

    [[nodiscard]] bool success() const;
    [[nodiscard]] bool allApplied() const;
    [[nodiscard]] bool hasUnsupported() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

struct PlasmaPanelVisibilityApplyResult
{
    bool ownershipVerified = false;
    QString requestedHostMode;
    bool requestedTemporaryHidden = false;
    std::optional<QString> actualHostMode;
    std::optional<bool> actualTemporaryHidden;
    QString status = QStringLiteral("failed");
    QString errorCode = QStringLiteral("invalid-result");
    bool rollbackAttempted = false;
    bool rollbackSucceeded = false;
    QString rollbackErrorCode;

    [[nodiscard]] bool success() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

using PlasmaPanelScriptExecutor = std::function<std::optional<int>(const QString &)>;
using PlasmaPanelPersistence = std::function<bool()>;

[[nodiscard]] QString nativePlacementFieldName(NativePlacementField field);
[[nodiscard]] QString plasmaPanelApplyFailureName(PlasmaPanelApplyFailure failure);

class PlasmaPanelAdapter final
{
public:
    explicit PlasmaPanelAdapter(PlasmaPanelScriptExecutor executor);

    [[nodiscard]] PlasmaPanelPlacementApplyResult applyPlacement(
        int containmentId,
        const QString &panelId,
        const QString &ownershipToken,
        const NativePanelPlacement &placement,
        PlasmaPanelPersistence persist = {}) const;

    [[nodiscard]] PlasmaPanelVisibilityApplyResult applyVisibility(
        int containmentId,
        const QString &panelId,
        const QString &ownershipToken,
        PlasmaPanelHidingMode hidingMode,
        bool temporaryHidden,
        PlasmaPanelPersistence persist = {}) const;

private:
    PlasmaPanelScriptExecutor m_executor;
};
}
