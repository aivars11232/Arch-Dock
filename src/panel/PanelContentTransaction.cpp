#include "PanelContentTransaction.h"

#include <QSet>
#include <QHash>

#include <algorithm>

#include <limits>

namespace ArchDock
{

namespace
{

void fail(PanelContentOutcome *outcome,
          const QStringList &currentOrder,
          const QString &errorCode,
          const QString &errorMessage)
{
    if (!outcome)
    {
        return;
    }
    outcome->success = false;
    outcome->changed = false;
    outcome->errorCode = errorCode;
    outcome->errorMessage = errorMessage;
    outcome->entryOrder = currentOrder;
}

void unchanged(PanelContentOutcome *outcome, const QStringList &currentOrder)
{
    if (!outcome)
    {
        return;
    }
    outcome->success = true;
    outcome->changed = false;
    outcome->errorCode.clear();
    outcome->errorMessage.clear();
    outcome->entryOrder = currentOrder;
}

}

QVariantMap PanelContentOutcome::toVariantMap() const
{
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("changed"), changed},
        {QStringLiteral("errorCode"), errorCode},
        {QStringLiteral("errorMessage"), errorMessage},
        {QStringLiteral("entryOrder"), entryOrder},
    };
}

QString PanelContentTransaction::operationName(PanelContentOperation operation)
{
    switch (operation)
    {
    case PanelContentOperation::Add:
        return QStringLiteral("add");
    case PanelContentOperation::Remove:
        return QStringLiteral("remove");
    case PanelContentOperation::MoveBefore:
        return QStringLiteral("move-before");
    case PanelContentOperation::SetOrder:
        return QStringLiteral("set-order");
    }
    return QStringLiteral("add");
}

QVariantList PanelContentTransaction::segmentEntries(
    const PanelDefinition &definition, const QVariantList &entries,
    QString *error, bool strict)
{
    if (error)
        error->clear();
    auto reject = [error](const QString &message) -> QVariantList {
        if (error)
            *error = message;
        return {};
    };
    QString validationError;
    if (!definition.isValid(&validationError))
        return reject(validationError);

    QHash<QString, QVariantMap> byId;
    QStringList order;
    for (const QVariant &value : entries)
    {
        const QVariantMap entry = value.toMap();
        const QString id = entry.value(QStringLiteral("appId")).toString();
        if (id.isEmpty() || byId.contains(id))
            return reject(QStringLiteral("segment entries require unique host identities"));
        byId.insert(id, entry);
        order.append(id);
    }
    auto segments = definition.segments;
    std::sort(segments.begin(), segments.end(), [](const auto &a, const auto &b) {
        return a.order < b.order;
    });
    QHash<QString, QString> owners;
    QHash<QString, QString> automatic;
    QSet<QString> explicitClaims;
    for (const auto &segment : segments)
    {
        if (segment.source == QStringLiteral("status"))
            return reject(QStringLiteral("no segment status provider is available"));
        if (segment.source != QStringLiteral("custom") && segment.entryIds.isEmpty())
        {
            if (automatic.contains(segment.source))
                return reject(QStringLiteral("a content source may have only one automatic segment"));
            automatic.insert(segment.source, segment.id);
        }
        for (const QString &id : segment.entryIds)
        {
            explicitClaims.insert(id);
            if (!byId.contains(id))
            {
                if (strict)
                    return reject(QStringLiteral("segment '%1' references a foreign or unavailable entry '%2'")
                        .arg(segment.id, id));
                continue;
            }
            const auto entry = byId.value(id);
            if ((segment.source == QStringLiteral("launcher") && !entry.value(QStringLiteral("pinned")).toBool()) ||
                (segment.source == QStringLiteral("tasks") && !entry.value(QStringLiteral("running")).toBool()))
            {
                if (strict)
                    return reject(QStringLiteral("segment entry does not match its content source"));
                continue;
            }
            owners.insert(id, segment.id);
        }
    }
    for (const QString &id : order)
    {
        if (explicitClaims.contains(id))
            continue;
        const auto entry = byId.value(id);
        QString owner;
        if (entry.value(QStringLiteral("pinned")).toBool())
            owner = automatic.value(QStringLiteral("launcher"));
        if (owner.isEmpty() && entry.value(QStringLiteral("running")).toBool())
            owner = automatic.value(QStringLiteral("tasks"));
        if (owner.isEmpty())
            owner = automatic.value(QStringLiteral("inherited"));
        if (!owner.isEmpty())
            owners.insert(id, owner);
    }
    QVariantList result;
    for (const auto &segment : segments)
    {
        for (const QString &id : order)
        {
            if (owners.value(id) != segment.id)
                continue;
            QVariantMap entry = byId.value(id);
            entry.insert(QStringLiteral("segmentId"), segment.id);
            result.append(entry);
        }
    }
    return result;
}

std::optional<PanelDefinition> PanelContentTransaction::prepare(
    const PanelDefinition &current,
    const PanelContentRequest &request,
    PanelContentOutcome *outcome)
{
    const QStringList currentOrder = current.content.canonicalEntryOrder();

    if (current.host.kind != PanelHostKind::FreeDesktop)
    {
        fail(outcome, currentOrder,
             QStringLiteral("native-content-unsupported"),
             QStringLiteral("native panels use the shared application model; "
                            "panel-specific content applies to free panels only"));
        return std::nullopt;
    }
    if (current.settingsRevision == std::numeric_limits<quint64>::max())
    {
        fail(outcome, currentOrder,
             QStringLiteral("revision-exhausted"),
             QStringLiteral("the panel settings revision cannot be advanced"));
        return std::nullopt;
    }

    PanelDefinition candidate = current;
    QStringList order = currentOrder;
    // Free panel entries are pinned content. Derive owners through the same
    // partition used by the live host, without introducing another pin store.
    QVariantList sourceEntries;
    for (const QString &id : currentOrder)
        sourceEntries.append(QVariantMap{{QStringLiteral("appId"), id},
            {QStringLiteral("pinned"), true}});
    QHash<QString, QString> owners;
    for (const QVariant &value : segmentEntries(current, sourceEntries))
    {
        const auto entry = value.toMap();
        owners.insert(entry.value(QStringLiteral("appId")).toString(),
            entry.value(QStringLiteral("segmentId")).toString());
    }

    switch (request.operation)
    {
    case PanelContentOperation::Add:
    {
        bool appended = false;
        for (const QString &url : request.urls)
        {
            const QString entryId = PanelContent::urlEntryId(url);
            if (entryId.isEmpty())
            {
                fail(outcome, currentOrder,
                     QStringLiteral("invalid-url"),
                     QStringLiteral("'%1' is not a valid entry URL").arg(url));
                return std::nullopt;
            }
            if (order.contains(entryId))
            {
                continue;
            }
            candidate.content.urls.append(PanelContent::urlFromEntryId(entryId));
            order.append(entryId);
            appended = true;
        }
        if (!appended)
        {
            unchanged(outcome, currentOrder);
            return std::nullopt;
        }
        break;
    }
    case PanelContentOperation::Remove:
    {
        const QString entryId = request.entryId.trimmed();
        if (!order.contains(entryId))
        {
            fail(outcome, currentOrder,
                 QStringLiteral("unknown-entry"),
                 QStringLiteral("'%1' is not an entry of this panel").arg(entryId));
            return std::nullopt;
        }
        if (PanelContent::isUrlEntryId(entryId))
        {
            QStringList urls;
            for (const QString &url : candidate.content.urls)
            {
                if (PanelContent::urlEntryId(url) != entryId)
                {
                    urls.append(url);
                }
            }
            candidate.content.urls = urls;
        }
        else
        {
            candidate.content.applicationIds.removeAll(entryId);
        }
        order.removeAll(entryId);
        for (auto &segment : candidate.segments)
            segment.entryIds.removeAll(entryId);
        break;
    }
    case PanelContentOperation::MoveBefore:
    {
        const QString entryId = request.entryId.trimmed();
        const QString beforeEntryId = request.beforeEntryId.trimmed();
        if (!order.contains(entryId) ||
            (!beforeEntryId.isEmpty() && !order.contains(beforeEntryId)))
        {
            fail(outcome, currentOrder,
                 QStringLiteral("unknown-entry"),
                 QStringLiteral("'%1' or '%2' is not an entry of this panel")
                     .arg(entryId, beforeEntryId));
            return std::nullopt;
        }
        if (entryId == beforeEntryId)
        {
            unchanged(outcome, currentOrder);
            return std::nullopt;
        }
        if (!beforeEntryId.isEmpty() && owners.value(entryId) != owners.value(beforeEntryId))
        {
            fail(outcome, currentOrder, QStringLiteral("cross-segment-move"),
                 QStringLiteral("change segment assignments through a settings transaction"));
            return std::nullopt;
        }
        order.removeAll(entryId);
        if (beforeEntryId.isEmpty())
        {
            order.append(entryId);
        }
        else
        {
            order.insert(order.indexOf(beforeEntryId), entryId);
        }
        if (order == currentOrder)
        {
            unchanged(outcome, currentOrder);
            return std::nullopt;
        }
        break;
    }
    case PanelContentOperation::SetOrder:
    {
        QStringList requested;
        for (const QString &entryId : request.order)
        {
            requested.append(entryId.trimmed());
        }
        const QSet<QString> requestedSet(requested.cbegin(), requested.cend());
        const QSet<QString> currentSet(currentOrder.cbegin(), currentOrder.cend());
        if (requested.size() != currentOrder.size() || requestedSet != currentSet ||
            requestedSet.size() != requested.size())
        {
            fail(outcome, currentOrder,
                 QStringLiteral("order-mismatch"),
                 QStringLiteral("the requested order is not a permutation of "
                                "this panel's entries"));
            return std::nullopt;
        }
        if (requested == currentOrder)
        {
            unchanged(outcome, currentOrder);
            return std::nullopt;
        }
        for (qsizetype index = 0; index < requested.size(); ++index)
        {
            if (owners.value(requested.at(index)) != owners.value(currentOrder.at(index)))
            {
                fail(outcome, currentOrder, QStringLiteral("cross-segment-move"),
                     QStringLiteral("reorder segments through a settings transaction"));
                return std::nullopt;
            }
        }
        order = requested;
        break;
    }
    }

    candidate.content.entryOrder = order;
    candidate.content.entryOrder = candidate.content.canonicalEntryOrder();
    candidate.settingsRevision = current.settingsRevision + 1;
    if (outcome)
    {
        outcome->success = true;
        outcome->changed = true;
        outcome->errorCode.clear();
        outcome->errorMessage.clear();
        outcome->entryOrder = candidate.content.entryOrder;
    }
    return candidate;
}

}
