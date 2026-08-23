#include "PlasmaPanelAdapter.h"

#include <algorithm>
#include <array>
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

QString plasmaAlignmentName(NativePanelAlignment alignment, bool vertical)
{
    switch (alignment)
    {
    case NativePanelAlignment::Start:
        return vertical ? QStringLiteral("right") : QStringLiteral("left");
    case NativePanelAlignment::Center:
        return QStringLiteral("center");
    case NativePanelAlignment::End:
        return vertical ? QStringLiteral("left") : QStringLiteral("right");
    }
    return {};
}

QString placementMutationScript(int containmentId,
                                const QString &panelId,
                                const QString &ownershipToken,
                                const QString &mutationBody)
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
        .arg(mutationBody)
        .arg(kScriptFailure);
}

QString edgeMutationScript(int containmentId,
                           const QString &panelId,
                           const QString &ownershipToken,
                           NativePanelEdge edge)
{
    const QString body = QStringLiteral(
        "if (typeof panel.location === 'undefined') { return %1; }"
        "panel.location = %2;"
        "var actual = String(panel.location).toLowerCase();"
        "if (actual === 'top') { return 0; }"
        "if (actual === 'bottom') { return 1; }"
        "if (actual === 'left') { return 2; }"
        "if (actual === 'right') { return 3; }"
        "return %3;")
        .arg(kPropertyUnsupported)
        .arg(plasmaScriptStringLiteral(edgeName(edge)))
        .arg(kUnrecognizedReadback);
    return placementMutationScript(containmentId, panelId, ownershipToken, body);
}

QString screenMutationScript(int containmentId,
                             const QString &panelId,
                             const QString &ownershipToken,
                             int screen)
{
    const QString body = QStringLiteral(
        "if (typeof panel.screen === 'undefined') { return %1; }"
        "panel.screen = %2;"
        "var actual = Number(panel.screen);"
        "if (!isFinite(actual) || Math.floor(actual) !== actual) { return %3; }"
        "return actual;")
        .arg(kPropertyUnsupported)
        .arg(screen)
        .arg(kUnrecognizedReadback);
    return placementMutationScript(containmentId, panelId, ownershipToken, body);
}

QString alignmentMutationScript(int containmentId,
                                const QString &panelId,
                                const QString &ownershipToken,
                                NativePanelAlignment alignment,
                                bool vertical)
{
    const QString body = QStringLiteral(
        "if (typeof panel.alignment === 'undefined') { return %1; }"
        "panel.alignment = %2;"
        "var actual = String(panel.alignment).toLowerCase();"
        "if (actual === 'left') { return 0; }"
        "if (actual === 'center') { return 1; }"
        "if (actual === 'right') { return 2; }"
        "return %3;")
        .arg(kPropertyUnsupported)
        .arg(plasmaScriptStringLiteral(plasmaAlignmentName(alignment, vertical)))
        .arg(kUnrecognizedReadback);
    return placementMutationScript(containmentId, panelId, ownershipToken, body);
}

QString offsetMutationScript(int containmentId,
                             const QString &panelId,
                             const QString &ownershipToken,
                             int offset)
{
    const QString body = QStringLiteral(
        "if (typeof panel.offset === 'undefined') { return %1; }"
        "panel.offset = %2;"
        "var actual = Number(panel.offset);"
        "if (!isFinite(actual) || Math.floor(actual) !== actual) { return %3; }"
        "return actual;")
        .arg(kPropertyUnsupported)
        .arg(offset)
        .arg(kUnrecognizedReadback);
    return placementMutationScript(containmentId, panelId, ownershipToken, body);
}

std::optional<QString> decodedEdge(int result)
{
    switch (result)
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
        return std::nullopt;
    }
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

struct FieldOperation
{
    NativePlacementField field;
    QString requestedValue;
    QString script;
    std::function<std::optional<QString>(int)> decode;
};

PlasmaPanelFieldResult failedField(const FieldOperation &operation,
                                   PlasmaPanelApplyFailure failure)
{
    PlasmaPanelFieldResult result;
    result.field = operation.field;
    result.requestedValue = operation.requestedValue;
    result.failure = failure;
    return result;
}
}

bool PlasmaPanelPlacementApplyResult::allApplied() const
{
    return fields.size() == 4 && std::all_of(
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

PlasmaPanelAdapter::PlasmaPanelAdapter(PlasmaPanelScriptExecutor executor)
    : m_executor(std::move(executor))
{
}

PlasmaPanelPlacementApplyResult PlasmaPanelAdapter::applyPlacement(
    int containmentId,
    const QString &panelId,
    const QString &ownershipToken,
    const NativePanelPlacement &placement) const
{
    const bool vertical = placement.edge == NativePanelEdge::Left ||
        placement.edge == NativePanelEdge::Right;
    const std::array<FieldOperation, 4> operations{
        FieldOperation{
            NativePlacementField::Edge,
            edgeName(placement.edge),
            edgeMutationScript(containmentId, panelId, ownershipToken, placement.edge),
            decodedEdge},
        FieldOperation{
            NativePlacementField::ScreenFallbackIndex,
            QString::number(placement.screen.fallbackIndex),
            screenMutationScript(
                containmentId, panelId, ownershipToken, placement.screen.fallbackIndex),
            [](int result) -> std::optional<QString>
            {
                return result >= -1 ? std::optional<QString>(QString::number(result))
                                    : std::nullopt;
            }},
        FieldOperation{
            NativePlacementField::Alignment,
            alignmentName(placement.alignment),
            alignmentMutationScript(
                containmentId, panelId, ownershipToken, placement.alignment, vertical),
            [vertical](int result)
            {
                return decodedAlignment(result, vertical);
            }},
        FieldOperation{
            NativePlacementField::Offset,
            QString::number(placement.offset),
            offsetMutationScript(containmentId, panelId, ownershipToken, placement.offset),
            [](int result) -> std::optional<QString>
            {
                return result >= 0 ? std::optional<QString>(QString::number(result))
                                   : std::nullopt;
            }},
    };

    PlasmaPanelPlacementApplyResult result;
    if (containmentId < 0 || panelId.trimmed().isEmpty() || ownershipToken.trimmed().isEmpty() ||
        !m_executor)
    {
        for (const FieldOperation &operation : operations)
        {
            result.fields.append(failedField(operation, PlasmaPanelApplyFailure::InvalidRequest));
        }
        return result;
    }

    for (auto iterator = operations.cbegin(); iterator != operations.cend(); ++iterator)
    {
        const std::optional<int> scriptResult = m_executor(iterator->script);
        if (!scriptResult.has_value())
        {
            result.fields.append(failedField(*iterator, PlasmaPanelApplyFailure::ScriptFailure));
            continue;
        }
        if (*scriptResult == kContainmentMissing || *scriptResult == kOwnershipDenied)
        {
            const PlasmaPanelApplyFailure failure = *scriptResult == kContainmentMissing
                ? PlasmaPanelApplyFailure::ContainmentMissing
                : PlasmaPanelApplyFailure::OwnershipDenied;
            result.ownershipVerified = false;
            for (; iterator != operations.cend(); ++iterator)
            {
                result.fields.append(failedField(*iterator, failure));
            }
            break;
        }

        result.ownershipVerified = true;
        if (*scriptResult == kPropertyUnsupported)
        {
            PlasmaPanelFieldResult field = failedField(
                *iterator, PlasmaPanelApplyFailure::PropertyUnsupported);
            field.status = PlasmaPanelApplyStatus::Unsupported;
            result.fields.append(field);
            continue;
        }
        if (*scriptResult == kScriptFailure)
        {
            result.fields.append(failedField(*iterator, PlasmaPanelApplyFailure::ScriptFailure));
            continue;
        }
        if (*scriptResult == kUnrecognizedReadback)
        {
            result.fields.append(failedField(*iterator, PlasmaPanelApplyFailure::ReadbackMismatch));
            continue;
        }

        const std::optional<QString> actualValue = iterator->decode(*scriptResult);
        if (!actualValue.has_value())
        {
            result.fields.append(failedField(*iterator, PlasmaPanelApplyFailure::ScriptFailure));
            continue;
        }

        PlasmaPanelFieldResult field;
        field.field = iterator->field;
        field.requestedValue = iterator->requestedValue;
        field.actualValue = actualValue;
        if (*actualValue == iterator->requestedValue)
        {
            field.status = PlasmaPanelApplyStatus::Applied;
            field.failure = PlasmaPanelApplyFailure::None;
        }
        else
        {
            field.failure = PlasmaPanelApplyFailure::ReadbackMismatch;
        }
        result.fields.append(field);
    }

    return result;
}
}
