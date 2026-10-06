#pragma once

#include "../model/PanelDefinition.h"

#include <QString>
#include <QVariantMap>

#include <functional>
#include <optional>

// Changing or resetting one entry's icon overrides (Icon Properties) as a
// revision-checked transaction: the panel's other entries and settings stay
// as they are, and a stale revision is refused.
namespace ArchDock
{

enum class IconOverrideTransactionStatus
{
    Prepared,
    Succeeded,
    ValidationFailed,
    RevisionConflict,
    PersistenceFailed
};

struct IconOverrideTransactionRequest
{
    QString panelId;
    quint64 expectedRevision = 0;
    QString entryIdentity;
    QVariantMap overrideValues;
    bool reset = false;
};

struct IconOverrideTransactionDraft
{
    PanelDefinition previousPanel;
    PanelDefinition candidatePanel;
    QString entryIdentity;
    bool reset = false;
};

struct IconOverrideTransactionOutcome
{
    IconOverrideTransactionStatus status =
        IconOverrideTransactionStatus::ValidationFailed;
    QString panelId;
    QString entryIdentity;
    quint64 expectedRevision = 0;
    quint64 previousRevision = 0;
    quint64 revision = 0;
    bool reset = false;
    QString errorCode;
    QString errorMessage;

    [[nodiscard]] bool success() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

class IconOverrideTransaction final
{
public:
    using StyleValidator = std::function<bool(const QString &styleReference)>;
    using StyleResolver = std::function<QVariantMap(
        const QString &styleReference)>;

    [[nodiscard]] static std::optional<IconOverrideTransactionDraft> prepare(
        const PanelDefinition &currentPanel,
        const IconOverrideTransactionRequest &request,
        IconOverrideTransactionOutcome *outcome,
        const StyleValidator &styleValidator = {});

    [[nodiscard]] static QVariantMap resolve(
        const PanelDefinition &panel,
        const QString &entryIdentity,
        const QString &baseGlyph,
        const QString &baseLabel,
        const StyleResolver &styleResolver);

    [[nodiscard]] static QString statusName(
        IconOverrideTransactionStatus status);
};

}
