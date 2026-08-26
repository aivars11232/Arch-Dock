#pragma once

#include "../model/PanelCapabilityResolver.h"
#include "../model/PanelDefinition.h"

#include <QList>
#include <QString>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace ArchDock
{

enum class PanelSettingsTransactionStatus
{
    Prepared,
    Succeeded,
    ValidationFailed,
    RevisionConflict,
    PersistenceFailed,
    HostFailed,
    RollbackFailed
};

struct PanelSettingsTransactionRequest
{
    QString panelId;
    quint64 expectedRevision = 0;
    QVariantMap panelValues;
    QVariantMap globalValues;
};

struct PanelSettingsTransactionDraft
{
    PanelDefinition previousPanel;
    PanelDefinition candidatePanel;
    QVariantMap previousGlobals;
    QVariantMap candidateGlobals;
};

struct PanelSettingsHostResult
{
    QString component;
    bool required = false;
    bool success = true;
    QString status = QStringLiteral("not-required");
    QString errorCode;
    QVariantMap details;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

struct PanelSettingsTransactionOutcome
{
    PanelSettingsTransactionStatus status =
        PanelSettingsTransactionStatus::ValidationFailed;
    QString panelId;
    quint64 expectedRevision = 0;
    quint64 previousRevision = 0;
    quint64 revision = 0;
    quint64 rollbackRevision = 0;
    QString errorCode;
    QString errorMessage;
    bool rolledBack = false;
    std::optional<CapabilityResolution> capabilityResolution;
    QList<PanelSettingsHostResult> hostResults;

    [[nodiscard]] bool success() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

class PanelSettingsTransaction final
{
public:
    using CapabilityValidator = std::function<CapabilityResolution(
        const PanelDefinition &candidate)>;

    [[nodiscard]] static std::optional<PanelSettingsTransactionDraft> prepare(
        const PanelDefinition &currentPanel,
        const QVariantMap &currentGlobals,
        const QVariantMap &candidateGlobals,
        const PanelSettingsTransactionRequest &request,
        PanelSettingsTransactionOutcome *outcome,
        const CapabilityValidator &capabilityValidator = {});

    [[nodiscard]] static QString statusName(PanelSettingsTransactionStatus status);
    [[nodiscard]] static bool requiredHostsSucceeded(
        const QList<PanelSettingsHostResult> &results);
};

}
