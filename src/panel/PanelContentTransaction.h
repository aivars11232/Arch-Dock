#pragma once

#include "../model/PanelDefinition.h"

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace ArchDock
{

// Panel-specific content operations for free desktop panels.
//
// A free panel owns an ordered list of entries: local URLs (desktop files,
// folders, documents) that are pinned to that panel alone. Native panels keep
// using the shared application model, so every operation here refuses a
// native host rather than silently touching the global pin list. The
// transaction is pure: it takes the current definition and produces the
// candidate the registry persists as the next revision, or a precise error.
enum class PanelContentOperation
{
    Add,
    Remove,
    MoveBefore,
    SetOrder
};

struct PanelContentRequest
{
    PanelContentOperation operation = PanelContentOperation::Add;
    // Add: URL strings the host layer has already validated.
    QStringList urls;
    // Remove and MoveBefore: the entry acted on.
    QString entryId;
    // MoveBefore: empty moves the entry to the end.
    QString beforeEntryId;
    // SetOrder: a permutation of the current canonical order.
    QStringList order;
};

struct PanelContentOutcome
{
    bool success = false;
    // False when the operation was valid but changed nothing, so the caller
    // has no revision to persist.
    bool changed = false;
    QString errorCode;
    QString errorMessage;
    QStringList entryOrder;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

class PanelContentTransaction final
{
public:
    [[nodiscard]] static std::optional<PanelDefinition> prepare(
        const PanelDefinition &current,
        const PanelContentRequest &request,
        PanelContentOutcome *outcome);
    [[nodiscard]] static QString operationName(PanelContentOperation operation);
};

}
