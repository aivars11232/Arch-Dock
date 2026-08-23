#include "PlasmaPanelAdapter.h"

#include <QVariantList>

#include <algorithm>
#include <utility>

namespace ArchDock
{
namespace
{
constexpr int kContainmentMissing = -1000;
constexpr int kOwnershipDenied = -1001;
constexpr int kPropertyUnsupported = -1002;
constexpr int kScriptFailure = -1003;
constexpr int kUnrecognizedReadback = -1004;

QString plasmaScriptStringLiteral(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    escaped.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    escaped.replace(QChar(0x2028), QStringLiteral("\\u2028"));
    escaped.replace(QChar(0x2029), QStringLiteral("\\u2029"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString edgeName(NativePanelEdge edge)
{
    switch (edge)
    {
    case NativePanelEdge::Top:
        return QStringLiteral("top");
    case NativePanelEdge::Bottom:
        return QStringLiteral("bottom");
    case NativePanelEdge::Left:
        return QStringLiteral("left");
    case NativePanelEdge::Right:
        return QStringLiteral("right");
    }
    return {};
}

QString edgeName(int rawEdge)
{
    switch (rawEdge)
    {
    case 0:
        return QStringLiteral("top");
    case 1:
        return QStringLiteral("bottom");
    case 2:
        return QStringLiteral("left");
    case 3:
        return QStringLiteral("right");
    default:
        return {};
    }
}

int edgeValue(NativePanelEdge edge)
{
    switch (edge)
    {
    case NativePanelEdge::Top:
        return 0;
    case NativePanelEdge::Bottom:
        return 1;
    case NativePanelEdge::Left:
        return 2;
    case NativePanelEdge::Right:
        return 3;
    }
    return -1;
}

bool verticalEdge(int rawEdge)
{
    return rawEdge == 2 || rawEdge == 3;
}

QString alignmentName(NativePanelAlignment alignment)
{
    switch (alignment)
    {
    case NativePanelAlignment::Start:
        return QStringLiteral("start");
    case NativePanelAlignment::Center:
        return QStringLiteral("center");
    case NativePanelAlignment::End:
        return QStringLiteral("end");
    }
    return {};
}

int alignmentValue(NativePanelAlignment alignment, bool vertical)
{
    switch (alignment)
    {
    case NativePanelAlignment::Start:
        return vertical ? 2 : 0;
    case NativePanelAlignment::Center:
        return 1;
    case NativePanelAlignment::End:
        return vertical ? 0 : 2;
    }
    return -1;
}

QString plasmaAlignmentName(int rawAlignment)
{
    switch (rawAlignment)
    {
    case 0:
        return QStringLiteral("left");
    case 1:
        return QStringLiteral("center");
    case 2:
        return QStringLiteral("right");
    default:
        return {};
    }
}

QString lengthModeName(NativePanelLengthMode mode)
{
    switch (mode)
    {
    case NativePanelLengthMode::Fit:
        return QStringLiteral("fit");
    case NativePanelLengthMode::Fixed:
        return QStringLiteral("fixed");
    case NativePanelLengthMode::Fill:
        return QStringLiteral("fill");
    }
    return {};
}

int lengthModeValue(NativePanelLengthMode mode)
{
    switch (mode)
    {
    case NativePanelLengthMode::Fit:
        return 0;
    case NativePanelLengthMode::Fixed:
        return 1;
    case NativePanelLengthMode::Fill:
        return 2;
    }
    return -1;
}

QString plasmaLengthModeName(int rawMode)
{
    switch (rawMode)
    {
    case 0:
        return QStringLiteral("fit");
    case 1:
        return QStringLiteral("custom");
    case 2:
        return QStringLiteral("fill");
    default:
        return {};
    }
}

QString placementScript(int containmentId,
                        const QString &panelId,
                        const QString &ownershipToken,
                        const QString &operationBody)
{
    return QStringLiteral(
        "var result = (function() {"
        "try {"
        "var panel = panelById(%1);"
        "if (!panel) { return %2; }"
        "panel.currentConfigGroup = ['ArchDock'];"
        "if (panel.readConfig('ownerToken', '') !== %3 || "
        "panel.readConfig('panelId', '') !== %4) { return %5; }"
        "%6"
        "} catch (error) { return %7; }"
        "})();"
        "print('ARCHDOCK_RESULT:' + String(result));")
        .arg(containmentId)
        .arg(kContainmentMissing)
        .arg(plasmaScriptStringLiteral(ownershipToken))
        .arg(plasmaScriptStringLiteral(panelId))
        .arg(kOwnershipDenied)
        .arg(operationBody)
        .arg(kScriptFailure);
}

QString edgeScript(int containmentId,
                   const QString &panelId,
                   const QString &ownershipToken,
                   std::optional<int> rawValue)
{
    QString mutation;
    if (rawValue.has_value())
    {
        mutation = QStringLiteral("panel.location = %1;")
                       .arg(plasmaScriptStringLiteral(edgeName(*rawValue)));
    }
    const QString body = QStringLiteral(
        "if (typeof panel.location === 'undefined') { return %1; }"
        "%2"
        "var actual = String(panel.location).toLowerCase();"
        "if (actual === 'top') { return 0; }"
        "if (actual === 'bottom') { return 1; }"
        "if (actual === 'left') { return 2; }"
        "if (actual === 'right') { return 3; }"
        "return %3;")
        .arg(kPropertyUnsupported)
        .arg(mutation)
        .arg(kUnrecognizedReadback);
    return placementScript(containmentId, panelId, ownershipToken, body);
}

QString alignmentScript(int containmentId,
                        const QString &panelId,
                        const QString &ownershipToken,
                        std::optional<int> rawValue)
{
    QString mutation;
    if (rawValue.has_value())
    {
        mutation = QStringLiteral("panel.alignment = %1;")
                       .arg(plasmaScriptStringLiteral(plasmaAlignmentName(*rawValue)));
    }
    const QString body = QStringLiteral(
        "if (typeof panel.alignment === 'undefined') { return %1; }"
        "%2"
        "var actual = String(panel.alignment).toLowerCase();"
        "if (actual === 'left') { return 0; }"
        "if (actual === 'center') { return 1; }"
        "if (actual === 'right') { return 2; }"
        "return %3;")
        .arg(kPropertyUnsupported)
        .arg(mutation)
        .arg(kUnrecognizedReadback);
    return placementScript(containmentId, panelId, ownershipToken, body);
}

QString lengthModeScript(int containmentId,
                         const QString &panelId,
                         const QString &ownershipToken,
                         std::optional<int> rawValue)
{
    QString mutation;
    if (rawValue.has_value())
    {
        mutation = QStringLiteral("panel.lengthMode = %1;")
                       .arg(plasmaScriptStringLiteral(plasmaLengthModeName(*rawValue)));
    }
    const QString body = QStringLiteral(
        "if (typeof panel.lengthMode === 'undefined') { return %1; }"
        "%2"
        "var actual = String(panel.lengthMode).toLowerCase();"
        "if (actual === 'fit') { return 0; }"
        "if (actual === 'custom') { return 1; }"
        "if (actual === 'fill') { return 2; }"
        "return %3;")
        .arg(kPropertyUnsupported)
        .arg(mutation)
        .arg(kUnrecognizedReadback);
    return placementScript(containmentId, panelId, ownershipToken, body);
}

QString integerScript(int containmentId,
                      const QString &panelId,
                      const QString &ownershipToken,
                      const QString &propertyName,
                      std::optional<int> value)
{
    QString mutation;
    if (value.has_value())
    {
        mutation = QStringLiteral("panel.%1 = %2;").arg(propertyName).arg(*value);
    }
    const QString body = QStringLiteral(
        "if (typeof panel.%1 === 'undefined') { return %2; }"
        "%3"
        "var actual = Number(panel.%1);"
        "if (!isFinite(actual) || Math.floor(actual) !== actual) { return %4; }"
        "return actual;")
        .arg(propertyName)
        .arg(kPropertyUnsupported)
        .arg(mutation)
        .arg(kUnrecognizedReadback);
    return placementScript(containmentId, panelId, ownershipToken, body);
}

std::optional<QString> decodedEdge(int result, bool)
{
    const QString value = edgeName(result);
    return value.isEmpty() ? std::nullopt : std::optional<QString>(value);
}

std::optional<QString> decodedAlignment(int result, bool vertical)
{
    switch (result)
    {
    case 0:
        return vertical ? QStringLiteral("end") : QStringLiteral("start");
    case 1:
        return QStringLiteral("center");
    case 2:
        return vertical ? QStringLiteral("start") : QStringLiteral("end");
    default:
        return std::nullopt;
    }
}

std::optional<QString> decodedLengthMode(int result, bool)
{
    switch (result)
    {
    case 0:
        return QStringLiteral("fit");
    case 1:
        return QStringLiteral("fixed");
    case 2:
        return QStringLiteral("fill");
    default:
        return std::nullopt;
    }
}

std::optional<QString> decodedNonNegativeInteger(int result, bool)
{
    return result >= 0 ? std::optional<QString>(QString::number(result)) : std::nullopt;
}

std::optional<QString> decodedScreen(int result, bool)
{
    return result >= -1 ? std::optional<QString>(QString::number(result)) : std::nullopt;
}

struct FieldOperation
{
    NativePlacementField field;
    QString requestedValue;
    int requestedRaw = 0;
    QString readScript;
    QString mutationScript;
    std::function<QString(int)> restoreScript;
    std::function<std::optional<QString>(int, bool)> decode;
};

PlasmaPanelFieldResult fieldResult(const FieldOperation &operation)
{
    PlasmaPanelFieldResult result;
    result.field = operation.field;
    result.requestedValue = operation.requestedValue;
    return result;
}

std::optional<PlasmaPanelApplyFailure> executionFailure(
    const std::optional<int> &scriptResult)
{
    if (!scriptResult.has_value() || *scriptResult == kScriptFailure)
    {
        return PlasmaPanelApplyFailure::ScriptFailure;
    }
    if (*scriptResult == kContainmentMissing)
    {
        return PlasmaPanelApplyFailure::ContainmentMissing;
    }
    if (*scriptResult == kOwnershipDenied)
    {
        return PlasmaPanelApplyFailure::OwnershipDenied;
    }
    if (*scriptResult == kPropertyUnsupported)
    {
        return PlasmaPanelApplyFailure::PropertyUnsupported;
    }
    if (*scriptResult == kUnrecognizedReadback)
    {
        return PlasmaPanelApplyFailure::ReadbackMismatch;
    }
    return std::nullopt;
}

void failEveryField(PlasmaPanelPlacementApplyResult *result,
                    PlasmaPanelApplyFailure failure)
{
    for (PlasmaPanelFieldResult &field : result->fields)
    {
        field.status = failure == PlasmaPanelApplyFailure::PropertyUnsupported
            ? PlasmaPanelApplyStatus::Unsupported
            : PlasmaPanelApplyStatus::Failed;
        field.failure = failure;
    }
    result->errorCode = plasmaPanelApplyFailureName(failure);
}

QString firstOperationError(const PlasmaPanelPlacementApplyResult &result)
{
    for (const PlasmaPanelFieldResult &field : result.fields)
    {
        if (field.status != PlasmaPanelApplyStatus::Applied)
        {
            return plasmaPanelApplyFailureName(field.failure);
        }
    }
    return QStringLiteral("placement-failed");
}

bool populateHostState(PlasmaPanelPlacementApplyResult *result,
                       const QList<FieldOperation> &operations,
                       const QList<std::optional<int>> &rawValues)
{
    result->hostState.clear();
    bool complete = rawValues.size() == operations.size();
    bool vertical = false;
    if (!rawValues.isEmpty() && rawValues.constFirst().has_value())
    {
        vertical = verticalEdge(*rawValues.constFirst());
    }

    for (qsizetype index = 0; index < operations.size(); ++index)
    {
        PlasmaPanelFieldResult &field = result->fields[index];
        field.hostValue.reset();
        if (index >= rawValues.size() || !rawValues[index].has_value() ||
            *rawValues[index] == kPropertyUnsupported)
        {
            complete = false;
            continue;
        }
        const std::optional<QString> decoded = operations[index].decode(*rawValues[index], vertical);
        if (!decoded.has_value())
        {
            complete = false;
            continue;
        }
        field.hostValue = decoded;
        result->hostState.insert(nativePlacementFieldName(field.field), *decoded);
    }
    return complete;
}

QList<FieldOperation> placementOperations(int containmentId,
                                          const QString &panelId,
                                          const QString &ownershipToken,
                                          const NativePanelPlacement &placement)
{
    const int requestedEdge = edgeValue(placement.edge);
    const bool requestedVertical = verticalEdge(requestedEdge);
    const auto edgeRestore = [containmentId, panelId, ownershipToken](int rawValue)
    {
        return edgeScript(containmentId, panelId, ownershipToken, rawValue);
    };
    const auto alignmentRestore = [containmentId, panelId, ownershipToken](int rawValue)
    {
        return alignmentScript(containmentId, panelId, ownershipToken, rawValue);
    };
    const auto integerOperation = [containmentId, panelId, ownershipToken](
                                      NativePlacementField field,
                                      const QString &propertyName,
                                      int requestedValue,
                                      const std::function<std::optional<QString>(int, bool)> &decode)
    {
        return FieldOperation{
            field,
            QString::number(requestedValue),
            requestedValue,
            integerScript(containmentId, panelId, ownershipToken, propertyName, std::nullopt),
            integerScript(containmentId, panelId, ownershipToken, propertyName, requestedValue),
            [containmentId, panelId, ownershipToken, propertyName](int rawValue)
            {
                return integerScript(
                    containmentId, panelId, ownershipToken, propertyName, rawValue);
            },
            decode};
    };

    QList<FieldOperation> operations{
        FieldOperation{
            NativePlacementField::Edge,
            edgeName(placement.edge),
            requestedEdge,
            edgeScript(containmentId, panelId, ownershipToken, std::nullopt),
            edgeScript(containmentId, panelId, ownershipToken, requestedEdge),
            edgeRestore,
            decodedEdge},
        integerOperation(
            NativePlacementField::ScreenFallbackIndex,
            QStringLiteral("screen"),
            placement.screen.fallbackIndex,
            decodedScreen),
        FieldOperation{
            NativePlacementField::Alignment,
            alignmentName(placement.alignment),
            alignmentValue(placement.alignment, requestedVertical),
            alignmentScript(containmentId, panelId, ownershipToken, std::nullopt),
            alignmentScript(
                containmentId,
                panelId,
                ownershipToken,
                alignmentValue(placement.alignment, requestedVertical)),
            alignmentRestore,
            decodedAlignment},
        integerOperation(
            NativePlacementField::Offset,
            QStringLiteral("offset"),
            placement.offset,
            decodedNonNegativeInteger),
        integerOperation(
            NativePlacementField::Thickness,
            QStringLiteral("height"),
            placement.thickness,
            decodedNonNegativeInteger),
    };

    const auto appendIntegerOperation = [&](NativePlacementField field,
                                            const QString &propertyName,
                                            int requestedValue)
    {
        operations.append(integerOperation(
            field, propertyName, requestedValue, decodedNonNegativeInteger));
    };
    const auto appendLengthModeOperation = [&]
    {
        const int requestedMode = lengthModeValue(placement.lengthMode);
        operations.append(FieldOperation{
            NativePlacementField::LengthMode,
            lengthModeName(placement.lengthMode),
            requestedMode,
            lengthModeScript(containmentId, panelId, ownershipToken, std::nullopt),
            lengthModeScript(containmentId, panelId, ownershipToken, requestedMode),
            [containmentId, panelId, ownershipToken](int rawValue)
            {
                return lengthModeScript(
                    containmentId, panelId, ownershipToken, rawValue);
            },
            decodedLengthMode});
    };

    if (placement.lengthMode == NativePanelLengthMode::Fixed)
    {
        appendIntegerOperation(
            NativePlacementField::MaximumLength,
            QStringLiteral("maximumLength"),
            placement.fixedLength);
        appendIntegerOperation(
            NativePlacementField::MinimumLength,
            QStringLiteral("minimumLength"),
            placement.fixedLength);
        appendIntegerOperation(
            NativePlacementField::FixedLength,
            QStringLiteral("length"),
            placement.fixedLength);
        appendLengthModeOperation();
    }
    else
    {
        appendLengthModeOperation();
        appendIntegerOperation(
            NativePlacementField::MaximumLength,
            QStringLiteral("maximumLength"),
            placement.maximumLength);
        appendIntegerOperation(
            NativePlacementField::MinimumLength,
            QStringLiteral("minimumLength"),
            placement.minimumLength);
    }
    return operations;
}
}

QString nativePlacementFieldName(NativePlacementField field)
{
    switch (field)
    {
    case NativePlacementField::Edge:
        return QStringLiteral("edge");
    case NativePlacementField::ScreenStableId:
        return QStringLiteral("screenId");
    case NativePlacementField::ScreenFallbackIndex:
        return QStringLiteral("screen");
    case NativePlacementField::Alignment:
        return QStringLiteral("alignment");
    case NativePlacementField::Offset:
        return QStringLiteral("offset");
    case NativePlacementField::Thickness:
        return QStringLiteral("height");
    case NativePlacementField::LengthMode:
        return QStringLiteral("lengthMode");
    case NativePlacementField::MinimumLength:
        return QStringLiteral("minimumLength");
    case NativePlacementField::MaximumLength:
        return QStringLiteral("maximumLength");
    case NativePlacementField::FixedLength:
        return QStringLiteral("fixedLength");
    case NativePlacementField::FloatingMargin:
        return QStringLiteral("floatingMargin");
    case NativePlacementField::VisibilityMode:
        return QStringLiteral("visibilityMode");
    case NativePlacementField::ReservedSpace:
        return QStringLiteral("reservedSpace");
    }
    return QStringLiteral("unknown");
}

QString plasmaPanelApplyFailureName(PlasmaPanelApplyFailure failure)
{
    switch (failure)
    {
    case PlasmaPanelApplyFailure::None:
        return {};
    case PlasmaPanelApplyFailure::InvalidRequest:
        return QStringLiteral("invalid-request");
    case PlasmaPanelApplyFailure::ContainmentMissing:
        return QStringLiteral("containment-missing");
    case PlasmaPanelApplyFailure::OwnershipDenied:
        return QStringLiteral("ownership-denied");
    case PlasmaPanelApplyFailure::PropertyUnsupported:
        return QStringLiteral("property-unsupported");
    case PlasmaPanelApplyFailure::ScriptFailure:
        return QStringLiteral("script-failure");
    case PlasmaPanelApplyFailure::ReadbackMismatch:
        return QStringLiteral("readback-mismatch");
    }
    return QStringLiteral("unknown-failure");
}

bool PlasmaPanelPlacementApplyResult::success() const
{
    return status == QLatin1String("applied");
}

bool PlasmaPanelPlacementApplyResult::allApplied() const
{
    return !fields.isEmpty() && std::all_of(
        fields.cbegin(),
        fields.cend(),
        [](const PlasmaPanelFieldResult &field)
        {
            return field.status == PlasmaPanelApplyStatus::Applied;
        });
}

bool PlasmaPanelPlacementApplyResult::hasUnsupported() const
{
    return std::any_of(
        fields.cbegin(),
        fields.cend(),
        [](const PlasmaPanelFieldResult &field)
        {
            return field.status == PlasmaPanelApplyStatus::Unsupported;
        });
}

QVariantMap PlasmaPanelPlacementApplyResult::toVariantMap() const
{
    QVariantMap requested;
    QVariantMap applied;
    QVariantList unsupported;
    QVariantList failed;
    for (const PlasmaPanelFieldResult &field : fields)
    {
        const QString fieldName = nativePlacementFieldName(field.field);
        requested.insert(fieldName, field.requestedValue);
        if (field.status == PlasmaPanelApplyStatus::Applied && field.actualValue.has_value())
        {
            applied.insert(fieldName, *field.actualValue);
            continue;
        }

        QVariantMap diagnostic{
            {QStringLiteral("field"), fieldName},
            {QStringLiteral("requested"), field.requestedValue},
            {QStringLiteral("errorCode"), plasmaPanelApplyFailureName(field.failure)},
        };
        if (field.actualValue.has_value())
        {
            diagnostic.insert(QStringLiteral("observed"), *field.actualValue);
        }
        if (field.status == PlasmaPanelApplyStatus::Unsupported)
        {
            unsupported.append(diagnostic);
        }
        else
        {
            failed.append(diagnostic);
        }
    }

    return {
        {QStringLiteral("success"), success()},
        {QStringLiteral("status"), status},
        {QStringLiteral("errorCode"), errorCode},
        {QStringLiteral("requested"), requested},
        {QStringLiteral("applied"), applied},
        {QStringLiteral("unsupported"), unsupported},
        {QStringLiteral("failed"), failed},
        {QStringLiteral("savedIntent"), savedIntent},
        {QStringLiteral("hostState"), hostState},
        {QStringLiteral("ownershipVerified"), ownershipVerified},
        {QStringLiteral("rollbackAttempted"), rollbackAttempted},
        {QStringLiteral("rollbackSucceeded"), rollbackSucceeded},
        {QStringLiteral("rollbackErrorCode"), rollbackErrorCode},
    };
}

PlasmaPanelAdapter::PlasmaPanelAdapter(PlasmaPanelScriptExecutor executor)
    : m_executor(std::move(executor))
{
}

PlasmaPanelPlacementApplyResult PlasmaPanelAdapter::applyPlacement(
    int containmentId,
    const QString &panelId,
    const QString &ownershipToken,
    const NativePanelPlacement &placement,
    PlasmaPanelPersistence persist) const
{
    const QList<FieldOperation> operations = placementOperations(
        containmentId, panelId, ownershipToken, placement);
    PlasmaPanelPlacementApplyResult result;
    result.fields.reserve(operations.size());
    for (const FieldOperation &operation : operations)
    {
        result.fields.append(fieldResult(operation));
    }

    if (containmentId < 0 || panelId.trimmed().isEmpty() || ownershipToken.trimmed().isEmpty() ||
        !m_executor)
    {
        failEveryField(&result, PlasmaPanelApplyFailure::InvalidRequest);
        return result;
    }

    QList<std::optional<int>> snapshots;
    snapshots.reserve(operations.size());
    for (qsizetype index = 0; index < operations.size(); ++index)
    {
        const std::optional<int> scriptResult = m_executor(operations[index].readScript);
        const std::optional<PlasmaPanelApplyFailure> failure = executionFailure(scriptResult);
        if (failure.has_value() && *failure != PlasmaPanelApplyFailure::PropertyUnsupported)
        {
            if (*failure == PlasmaPanelApplyFailure::ContainmentMissing ||
                *failure == PlasmaPanelApplyFailure::OwnershipDenied)
            {
                result.ownershipVerified = false;
            }
            failEveryField(&result, *failure);
            return result;
        }
        result.ownershipVerified = true;
        if (failure.has_value())
        {
            snapshots.append(kPropertyUnsupported);
            continue;
        }

        const bool vertical = !snapshots.isEmpty() && snapshots.constFirst().has_value() &&
            verticalEdge(*snapshots.constFirst());
        if (!operations[index].decode(*scriptResult, vertical).has_value())
        {
            failEveryField(&result, PlasmaPanelApplyFailure::ReadbackMismatch);
            return result;
        }
        snapshots.append(*scriptResult);
    }
    populateHostState(&result, operations, snapshots);

    for (qsizetype index = 0; index < operations.size(); ++index)
    {
        const std::optional<int> scriptResult = m_executor(operations[index].mutationScript);
        const std::optional<PlasmaPanelApplyFailure> failure = executionFailure(scriptResult);
        PlasmaPanelFieldResult &field = result.fields[index];
        if (failure.has_value())
        {
            field.status = *failure == PlasmaPanelApplyFailure::PropertyUnsupported
                ? PlasmaPanelApplyStatus::Unsupported
                : PlasmaPanelApplyStatus::Failed;
            field.failure = *failure;
            if (*failure == PlasmaPanelApplyFailure::ContainmentMissing ||
                *failure == PlasmaPanelApplyFailure::OwnershipDenied)
            {
                result.ownershipVerified = false;
                for (qsizetype remaining = index + 1; remaining < operations.size(); ++remaining)
                {
                    result.fields[remaining].failure = *failure;
                }
                break;
            }
            continue;
        }

        const bool vertical = verticalEdge(edgeValue(placement.edge));
        field.actualValue = operations[index].decode(*scriptResult, vertical);
        if (!field.actualValue.has_value())
        {
            field.status = PlasmaPanelApplyStatus::Failed;
            field.failure = PlasmaPanelApplyFailure::ReadbackMismatch;
        }
        else if (*field.actualValue == field.requestedValue)
        {
            field.status = PlasmaPanelApplyStatus::Applied;
            field.failure = PlasmaPanelApplyFailure::None;
        }
        else
        {
            field.status = PlasmaPanelApplyStatus::Failed;
            field.failure = PlasmaPanelApplyFailure::ReadbackMismatch;
        }
    }

    QList<std::optional<int>> attemptReadback;
    attemptReadback.reserve(operations.size());
    bool readbackOwnershipValid = true;
    for (qsizetype index = 0; index < operations.size(); ++index)
    {
        const std::optional<int> scriptResult = m_executor(operations[index].readScript);
        const std::optional<PlasmaPanelApplyFailure> failure = executionFailure(scriptResult);
        PlasmaPanelFieldResult &field = result.fields[index];
        if (failure.has_value())
        {
            attemptReadback.append(
                *failure == PlasmaPanelApplyFailure::PropertyUnsupported
                    ? std::optional<int>(kPropertyUnsupported)
                    : std::nullopt);
            field.actualValue.reset();
            field.status = *failure == PlasmaPanelApplyFailure::PropertyUnsupported
                ? PlasmaPanelApplyStatus::Unsupported
                : PlasmaPanelApplyStatus::Failed;
            field.failure = *failure;
            if (*failure == PlasmaPanelApplyFailure::ContainmentMissing ||
                *failure == PlasmaPanelApplyFailure::OwnershipDenied)
            {
                readbackOwnershipValid = false;
            }
            continue;
        }
        attemptReadback.append(*scriptResult);
    }

    const bool attemptVertical = !attemptReadback.isEmpty() &&
        attemptReadback.constFirst().has_value() &&
        verticalEdge(*attemptReadback.constFirst());
    for (qsizetype index = 0; index < operations.size(); ++index)
    {
        if (!attemptReadback[index].has_value() ||
            *attemptReadback[index] == kPropertyUnsupported)
        {
            continue;
        }
        PlasmaPanelFieldResult &field = result.fields[index];
        field.actualValue = operations[index].decode(*attemptReadback[index], attemptVertical);
        if (field.actualValue.has_value() && *field.actualValue == field.requestedValue)
        {
            field.status = PlasmaPanelApplyStatus::Applied;
            field.failure = PlasmaPanelApplyFailure::None;
        }
        else
        {
            field.status = PlasmaPanelApplyStatus::Failed;
            field.failure = PlasmaPanelApplyFailure::ReadbackMismatch;
        }
    }
    result.ownershipVerified = result.ownershipVerified && readbackOwnershipValid;
    populateHostState(&result, operations, attemptReadback);

    bool persistenceFailed = false;
    if (result.allApplied() && persist)
    {
        persistenceFailed = !persist();
    }
    if (result.allApplied() && !persistenceFailed)
    {
        result.status = QStringLiteral("applied");
        result.errorCode.clear();
        return result;
    }

    result.errorCode = persistenceFailed
        ? QStringLiteral("persistence-failed")
        : firstOperationError(result);
    result.rollbackAttempted = true;
    bool rollbackOk = true;
    QString rollbackError;
    for (qsizetype index = 0; index < operations.size(); ++index)
    {
        if (!snapshots[index].has_value() || *snapshots[index] == kPropertyUnsupported)
        {
            continue;
        }
        const std::optional<int> rollbackResult = m_executor(
            operations[index].restoreScript(*snapshots[index]));
        const std::optional<PlasmaPanelApplyFailure> failure = executionFailure(rollbackResult);
        if (failure.has_value() || !rollbackResult.has_value() ||
            *rollbackResult != *snapshots[index])
        {
            if (failure == PlasmaPanelApplyFailure::ContainmentMissing ||
                failure == PlasmaPanelApplyFailure::OwnershipDenied)
            {
                result.ownershipVerified = false;
            }
            rollbackOk = false;
            if (rollbackError.isEmpty())
            {
                rollbackError = failure.has_value()
                    ? plasmaPanelApplyFailureName(*failure)
                    : QStringLiteral("rollback-readback-mismatch");
            }
        }
    }

    QList<std::optional<int>> finalReadback;
    finalReadback.reserve(operations.size());
    for (qsizetype index = 0; index < operations.size(); ++index)
    {
        const std::optional<int> scriptResult = m_executor(operations[index].readScript);
        const std::optional<PlasmaPanelApplyFailure> failure = executionFailure(scriptResult);
        if (failure.has_value())
        {
            if (*failure == PlasmaPanelApplyFailure::ContainmentMissing ||
                *failure == PlasmaPanelApplyFailure::OwnershipDenied)
            {
                result.ownershipVerified = false;
            }
            finalReadback.append(
                *failure == PlasmaPanelApplyFailure::PropertyUnsupported
                    ? std::optional<int>(kPropertyUnsupported)
                    : std::nullopt);
            if ((*failure == PlasmaPanelApplyFailure::PropertyUnsupported &&
                 *snapshots[index] != kPropertyUnsupported) ||
                *failure != PlasmaPanelApplyFailure::PropertyUnsupported)
            {
                rollbackOk = false;
                if (rollbackError.isEmpty())
                {
                    rollbackError = plasmaPanelApplyFailureName(*failure);
                }
            }
            continue;
        }
        finalReadback.append(*scriptResult);
        if (!snapshots[index].has_value() || *scriptResult != *snapshots[index])
        {
            rollbackOk = false;
            if (rollbackError.isEmpty())
            {
                rollbackError = QStringLiteral("rollback-readback-mismatch");
            }
        }
    }
    populateHostState(&result, operations, finalReadback);

    result.rollbackSucceeded = rollbackOk;
    result.rollbackErrorCode = rollbackOk ? QString{} : rollbackError;
    result.status = rollbackOk
        ? QStringLiteral("rolled-back")
        : QStringLiteral("rollback-failed");
    return result;
}
}
