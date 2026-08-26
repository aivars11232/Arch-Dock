#include "PanelSettingsTransaction.h"
#include "../model/PanelSettingsSchema.h"

#include <QVariantList>

#include <limits>

namespace ArchDock
{

namespace
{

QVariant revisionValue(quint64 revision)
{
    return QVariant::fromValue<qulonglong>(revision);
}

void fail(PanelSettingsTransactionOutcome *outcome,
          PanelSettingsTransactionStatus status,
          const QString &errorCode,
          const QString &errorMessage)
{
    if (!outcome)
    {
        return;
    }
    outcome->status = status;
    outcome->errorCode = errorCode;
    outcome->errorMessage = errorMessage;
}

}

QVariantMap PanelSettingsHostResult::toVariantMap() const
{
    return {
        {QStringLiteral("component"), component},
        {QStringLiteral("required"), required},
        {QStringLiteral("success"), success},
        {QStringLiteral("status"), status},
        {QStringLiteral("errorCode"), errorCode},
        {QStringLiteral("details"), details},
    };
}

bool PanelSettingsTransactionOutcome::success() const
{
    return status == PanelSettingsTransactionStatus::Succeeded;
}

QVariantMap PanelSettingsTransactionOutcome::toVariantMap() const
{
    QVariantList serializedHostResults;
    serializedHostResults.reserve(hostResults.size());
    for (const PanelSettingsHostResult &result : hostResults)
    {
        serializedHostResults.append(result.toVariantMap());
    }

    return {
        {QStringLiteral("capabilityResolution"),
         capabilityResolution.has_value()
             ? capabilityResolution->toVariantMap()
             : QVariantMap{}},
        {QStringLiteral("success"), success()},
        {QStringLiteral("status"), PanelSettingsTransaction::statusName(status)},
        {QStringLiteral("panelId"), panelId},
        {QStringLiteral("expectedRevision"), revisionValue(expectedRevision)},
        {QStringLiteral("previousRevision"), revisionValue(previousRevision)},
        {QStringLiteral("revision"), revisionValue(revision)},
        {QStringLiteral("rollbackRevision"), revisionValue(rollbackRevision)},
        {QStringLiteral("errorCode"), errorCode},
        {QStringLiteral("errorMessage"), errorMessage},
        {QStringLiteral("rolledBack"), rolledBack},
        {QStringLiteral("hostResults"), serializedHostResults},
    };
}

std::optional<PanelSettingsTransactionDraft> PanelSettingsTransaction::prepare(
    const PanelDefinition &currentPanel,
    const QVariantMap &currentGlobals,
    const QVariantMap &candidateGlobals,
    const PanelSettingsTransactionRequest &request,
    PanelSettingsTransactionOutcome *outcome,
    const CapabilityValidator &capabilityValidator)
{
    if (outcome)
    {
        *outcome = {};
        outcome->panelId = request.panelId;
        outcome->expectedRevision = request.expectedRevision;
        outcome->previousRevision = currentPanel.settingsRevision;
        outcome->revision = currentPanel.settingsRevision;
    }

    if (request.panelId.trimmed().isEmpty() ||
        request.panelId != currentPanel.identity.id)
    {
        fail(outcome,
             PanelSettingsTransactionStatus::ValidationFailed,
             QStringLiteral("panel-id-mismatch"),
             QStringLiteral("the transaction panel id does not match the snapshot"));
        return std::nullopt;
    }
    if (request.expectedRevision != currentPanel.settingsRevision)
    {
        fail(outcome,
             PanelSettingsTransactionStatus::RevisionConflict,
             QStringLiteral("stale-revision"),
             QStringLiteral("the panel changed after this draft was opened"));
        return std::nullopt;
    }
    if (currentPanel.settingsRevision == std::numeric_limits<quint64>::max())
    {
        fail(outcome,
             PanelSettingsTransactionStatus::ValidationFailed,
             QStringLiteral("revision-exhausted"),
             QStringLiteral("the panel settings revision cannot be advanced"));
        return std::nullopt;
    }

    for (auto it = request.panelValues.cbegin(); it != request.panelValues.cend(); ++it)
    {
        const PanelSettingsFieldDescriptor *field =
            PanelSettingsSchema::panelDescriptor(it.key());
        if (!field)
        {
            fail(outcome,
                 PanelSettingsTransactionStatus::ValidationFailed,
                 QStringLiteral("unknown-panel-field"),
                 QStringLiteral("the field '%1' is not part of the panel schema")
                     .arg(it.key()));
            return std::nullopt;
        }
        if (field->access == PanelSettingsFieldAccess::Protected)
        {
            fail(outcome,
                 PanelSettingsTransactionStatus::ValidationFailed,
                 QStringLiteral("protected-panel-field"),
                 QStringLiteral("the field '%1' cannot be changed by a settings transaction")
                     .arg(it.key()));
            return std::nullopt;
        }
        if (!PanelSettingsSchema::isTransactionPanelField(it.key()))
        {
            fail(outcome,
                 PanelSettingsTransactionStatus::ValidationFailed,
                 QStringLiteral("unavailable-panel-field"),
                 QStringLiteral("the field '%1' has no active settings transaction path")
                     .arg(it.key()));
            return std::nullopt;
        }
    }
    for (auto it = request.globalValues.cbegin(); it != request.globalValues.cend(); ++it)
    {
        if (!PanelSettingsSchema::isKnownGlobalField(it.key()))
        {
            fail(outcome,
                 PanelSettingsTransactionStatus::ValidationFailed,
                 QStringLiteral("unknown-global-field"),
                 QStringLiteral("the field '%1' is not part of the global settings schema")
                     .arg(it.key()));
            return std::nullopt;
        }
        if (!PanelSettingsSchema::isTransactionGlobalField(it.key()))
        {
            fail(outcome,
                 PanelSettingsTransactionStatus::ValidationFailed,
                 QStringLiteral("protected-global-field"),
                 QStringLiteral("the field '%1' cannot be changed by a settings transaction")
                     .arg(it.key()));
            return std::nullopt;
        }
    }

    QVariantMap candidateRecord = currentPanel.toLegacyMap();
    for (auto it = request.panelValues.cbegin(); it != request.panelValues.cend(); ++it)
    {
        candidateRecord.insert(it.key(), it.value());
    }
    candidateRecord.insert(
        QStringLiteral("settingsRevision"),
        QString::number(currentPanel.settingsRevision + 1));

    QString validationError;
    std::optional<PanelDefinition> candidate = PanelDefinition::fromLegacyMap(
        candidateRecord, &validationError);
    if (!candidate.has_value())
    {
        fail(outcome,
             PanelSettingsTransactionStatus::ValidationFailed,
             QStringLiteral("invalid-panel-definition"),
             validationError);
        return std::nullopt;
    }
    if (candidate->identity.id != currentPanel.identity.id ||
        candidate->identity.builtIn != currentPanel.identity.builtIn)
    {
        fail(outcome,
             PanelSettingsTransactionStatus::ValidationFailed,
             QStringLiteral("panel-identity-changed"),
             QStringLiteral("the candidate changed protected panel identity"));
        return std::nullopt;
    }

    if (capabilityValidator)
    {
        const CapabilityResolution resolution = capabilityValidator(*candidate);
        if (outcome)
        {
            outcome->capabilityResolution = resolution;
        }
        if (!resolution.available)
        {
            fail(outcome,
                 PanelSettingsTransactionStatus::ValidationFailed,
                 QStringLiteral("capability-unavailable"),
                 capabilityReasonCodeName(resolution.reason));
            return std::nullopt;
        }
    }

    if (outcome)
    {
        outcome->status = PanelSettingsTransactionStatus::Prepared;
        outcome->revision = candidate->settingsRevision;
        outcome->errorCode.clear();
        outcome->errorMessage.clear();
    }
    return PanelSettingsTransactionDraft{
        currentPanel,
        *candidate,
        currentGlobals,
        candidateGlobals,
    };
}

QString PanelSettingsTransaction::statusName(PanelSettingsTransactionStatus status)
{
    switch (status)
    {
    case PanelSettingsTransactionStatus::Prepared:
        return QStringLiteral("prepared");
    case PanelSettingsTransactionStatus::Succeeded:
        return QStringLiteral("succeeded");
    case PanelSettingsTransactionStatus::ValidationFailed:
        return QStringLiteral("validation-failed");
    case PanelSettingsTransactionStatus::RevisionConflict:
        return QStringLiteral("revision-conflict");
    case PanelSettingsTransactionStatus::PersistenceFailed:
        return QStringLiteral("persistence-failed");
    case PanelSettingsTransactionStatus::HostFailed:
        return QStringLiteral("host-failed");
    case PanelSettingsTransactionStatus::RollbackFailed:
        return QStringLiteral("rollback-failed");
    }
    return QStringLiteral("validation-failed");
}

bool PanelSettingsTransaction::requiredHostsSucceeded(
    const QList<PanelSettingsHostResult> &results)
{
    for (const PanelSettingsHostResult &result : results)
    {
        if (result.required && !result.success)
        {
            return false;
        }
    }
    return true;
}

}
