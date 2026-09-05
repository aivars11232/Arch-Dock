#include "PanelContentTransaction.h"

#include <QSet>

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
