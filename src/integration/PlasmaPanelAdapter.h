#pragma once

#include "../PanelPlacement.h"

#include <QList>
#include <QString>

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
    PlasmaPanelApplyFailure failure = PlasmaPanelApplyFailure::ScriptFailure;
};

struct PlasmaPanelPlacementApplyResult
{
    bool ownershipVerified = false;
    QList<PlasmaPanelFieldResult> fields;

    [[nodiscard]] bool allApplied() const;
    [[nodiscard]] bool hasUnsupported() const;
};

using PlasmaPanelScriptExecutor = std::function<std::optional<int>(const QString &)>;

class PlasmaPanelAdapter final
{
public:
    explicit PlasmaPanelAdapter(PlasmaPanelScriptExecutor executor);

    [[nodiscard]] PlasmaPanelPlacementApplyResult applyPlacement(
        int containmentId,
        const QString &panelId,
        const QString &ownershipToken,
        const NativePanelPlacement &placement) const;

private:
    PlasmaPanelScriptExecutor m_executor;
};
}
