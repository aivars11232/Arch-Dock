#include "PanelPlacement.h"

#include <QtGlobal>

#include <algorithm>

namespace ArchDock
{
namespace
{
QString normalizedAlias(const QString &value)
{
    QString alias = value.trimmed().toLower();
    alias.remove(QLatin1Char('-'));
    alias.remove(QLatin1Char('_'));
    alias.remove(QLatin1Char(' '));
    return alias;
}

std::optional<NativePanelEdge> normalizedEdge(const QString &value)
{
    const QString alias = normalizedAlias(value);
    if (alias.isEmpty() || alias == QStringLiteral("bottom") ||
        alias == QStringLiteral("bottomedge"))
    {
        return NativePanelEdge::Bottom;
    }
    if (alias == QStringLiteral("top") || alias == QStringLiteral("topedge"))
    {
        return NativePanelEdge::Top;
    }
    if (alias == QStringLiteral("left") || alias == QStringLiteral("leftedge"))
    {
        return NativePanelEdge::Left;
    }
    if (alias == QStringLiteral("right") || alias == QStringLiteral("rightedge"))
    {
        return NativePanelEdge::Right;
    }
    return std::nullopt;
}

std::optional<NativePanelAlignment> normalizedAlignment(const QString &value,
                                                        NativePanelEdge edge)
{
    const QString alias = normalizedAlias(value);
    if (alias.isEmpty() || alias == QStringLiteral("center") ||
        alias == QStringLiteral("centre"))
    {
        return NativePanelAlignment::Center;
    }
    if (alias == QStringLiteral("start"))
    {
        return NativePanelAlignment::Start;
    }
    if (alias == QStringLiteral("end"))
    {
        return NativePanelAlignment::End;
    }

    const bool vertical = edge == NativePanelEdge::Left || edge == NativePanelEdge::Right;
    if ((!vertical && alias == QStringLiteral("left")) ||
        (vertical && (alias == QStringLiteral("right") || alias == QStringLiteral("top"))))
    {
        return NativePanelAlignment::Start;
    }
    if ((!vertical && alias == QStringLiteral("right")) ||
        (vertical && (alias == QStringLiteral("left") || alias == QStringLiteral("bottom"))))
    {
        return NativePanelAlignment::End;
    }
    return std::nullopt;
}

std::optional<NativePanelLengthMode> normalizedLengthMode(const QString &value)
{
    const QString alias = normalizedAlias(value);
    if (alias.isEmpty() || alias == QStringLiteral("fixed") ||
        alias == QStringLiteral("custom"))
    {
        return NativePanelLengthMode::Fixed;
    }
    if (alias == QStringLiteral("fit") || alias == QStringLiteral("dynamic") ||
        alias == QStringLiteral("auto"))
    {
        return NativePanelLengthMode::Fit;
    }
    if (alias == QStringLiteral("fill") || alias == QStringLiteral("full"))
    {
        return NativePanelLengthMode::Fill;
    }
    return std::nullopt;
}

std::optional<NativePanelVisibilityMode> normalizedVisibilityMode(const QString &value)
{
    const QString alias = normalizedAlias(value);
    if (alias.isEmpty() || alias == QStringLiteral("always") ||
        alias == QStringLiteral("none") || alias == QStringLiteral("visible"))
    {
        return NativePanelVisibilityMode::Always;
    }
    if (alias == QStringLiteral("autohide"))
    {
        return NativePanelVisibilityMode::AutoHide;
    }
    if (alias == QStringLiteral("dodge") || alias == QStringLiteral("dodgewindows"))
    {
        return NativePanelVisibilityMode::Dodge;
    }
    if (alias == QStringLiteral("cover"))
    {
        return NativePanelVisibilityMode::Cover;
    }
    return std::nullopt;
}

std::optional<NativePanelReservedSpace> normalizedReservedSpace(const QString &value)
{
    const QString alias = normalizedAlias(value);
    if (alias.isEmpty() || alias == QStringLiteral("automatic") ||
        alias == QStringLiteral("auto"))
    {
        return NativePanelReservedSpace::Automatic;
    }
    if (alias == QStringLiteral("reserve") || alias == QStringLiteral("reserved") ||
        alias == QStringLiteral("exclusive"))
    {
        return NativePanelReservedSpace::Reserve;
    }
    if (alias == QStringLiteral("donotreserve") || alias == QStringLiteral("noreserve") ||
        alias == QStringLiteral("overlay") || alias == QStringLiteral("windowsgobelow"))
    {
        return NativePanelReservedSpace::DoNotReserve;
    }
    return std::nullopt;
}

void addIssue(QList<NativePlacementIssue> *issues,
              NativePlacementField field,
              NativePlacementIssueKind kind,
              NativePlacementIssueCode code)
{
    issues->append({field, kind, code});
}

bool hasIssueKind(const QList<NativePlacementIssue> &issues, NativePlacementIssueKind kind)
{
    return std::any_of(
        issues.cbegin(),
        issues.cend(),
        [kind](const NativePlacementIssue &issue)
        {
            return issue.kind == kind;
        });
}
}

bool NativePanelPlacementResult::isValid() const
{
    return placement.has_value() &&
        !hasIssueKind(issues, NativePlacementIssueKind::ValidationError);
}

bool NativePanelPlacementResult::isSupported() const
{
    return isValid() &&
        !hasIssueKind(issues, NativePlacementIssueKind::UnsupportedRequest);
}

NativePanelPlacement defaultNativePanelPlacement()
{
    return {};
}

NativePanelPlacementResult normalizeNativePanelPlacement(
    const NativePanelPlacementRequest &request,
    const NativePlacementCapabilities &capabilities)
{
    NativePanelPlacement placement = defaultNativePanelPlacement();
    QList<NativePlacementIssue> issues;

    placement.screen.stableId = request.screenStableId.trimmed();
    placement.screen.fallbackIndex = request.screenFallbackIndex;
    placement.offset = request.offset;
    placement.thickness = request.thickness;
    placement.minimumLength = request.minimumLength;
    placement.maximumLength = request.maximumLength;
    placement.fixedLength = request.fixedLength;
    placement.floatingMargin = request.floatingMargin;

    if (request.screenFallbackIndex < 0)
    {
        addIssue(&issues,
                 NativePlacementField::ScreenFallbackIndex,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::OutOfRange);
    }
    if (request.offset < 0)
    {
        addIssue(&issues,
                 NativePlacementField::Offset,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::OutOfRange);
    }
    if (request.thickness < kNativePanelMinimumDimension ||
        request.thickness > kNativePanelMaximumDimension)
    {
        addIssue(&issues,
                 NativePlacementField::Thickness,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::OutOfRange);
    }
    if (request.minimumLength < kNativePanelMinimumDimension ||
        request.minimumLength > kNativePanelMaximumDimension)
    {
        addIssue(&issues,
                 NativePlacementField::MinimumLength,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::OutOfRange);
    }
    if (request.maximumLength < kNativePanelMinimumDimension ||
        request.maximumLength > kNativePanelMaximumDimension)
    {
        addIssue(&issues,
                 NativePlacementField::MaximumLength,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::OutOfRange);
    }
    if (request.fixedLength < kNativePanelMinimumDimension ||
        request.fixedLength > kNativePanelMaximumDimension)
    {
        addIssue(&issues,
                 NativePlacementField::FixedLength,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::OutOfRange);
    }
    if (request.floatingMargin < 0)
    {
        addIssue(&issues,
                 NativePlacementField::FloatingMargin,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::OutOfRange);
    }

    const std::optional<NativePanelEdge> edge = normalizedEdge(request.edge);
    if (edge.has_value())
    {
        placement.edge = *edge;
    }
    else
    {
        addIssue(&issues,
                 NativePlacementField::Edge,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::UnknownAlias);
    }

    const std::optional<NativePanelAlignment> alignment = normalizedAlignment(
        request.alignment, placement.edge);
    if (alignment.has_value())
    {
        placement.alignment = *alignment;
    }
    else
    {
        addIssue(&issues,
                 NativePlacementField::Alignment,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::UnknownAlias);
    }

    const std::optional<NativePanelLengthMode> lengthMode = normalizedLengthMode(
        request.lengthMode);
    if (lengthMode.has_value())
    {
        placement.lengthMode = *lengthMode;
    }
    else
    {
        addIssue(&issues,
                 NativePlacementField::LengthMode,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::UnknownAlias);
    }
    if (request.dynamicLength.has_value())
    {
        const NativePanelLengthMode dynamicMode = *request.dynamicLength
            ? NativePanelLengthMode::Fit
            : NativePanelLengthMode::Fixed;
        if (!request.lengthMode.trimmed().isEmpty() && lengthMode.has_value() &&
            *lengthMode != dynamicMode)
        {
            addIssue(&issues,
                     NativePlacementField::LengthMode,
                     NativePlacementIssueKind::ValidationError,
                     NativePlacementIssueCode::ConflictingValues);
        }
        else
        {
            placement.lengthMode = dynamicMode;
        }
    }

    const std::optional<NativePanelVisibilityMode> visibilityMode =
        normalizedVisibilityMode(request.visibilityMode);
    if (visibilityMode.has_value())
    {
        placement.visibilityMode = *visibilityMode;
    }
    else
    {
        addIssue(&issues,
                 NativePlacementField::VisibilityMode,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::UnknownAlias);
    }

    const std::optional<NativePanelReservedSpace> reservedSpace =
        normalizedReservedSpace(request.reservedSpace);
    if (reservedSpace.has_value())
    {
        placement.reservedSpace = *reservedSpace;
    }
    else
    {
        addIssue(&issues,
                 NativePlacementField::ReservedSpace,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::UnknownAlias);
    }

    if (request.minimumLength > request.maximumLength)
    {
        addIssue(&issues,
                 NativePlacementField::MaximumLength,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::MinimumExceedsMaximum);
    }
    if (request.fixedLength < request.minimumLength ||
        request.fixedLength > request.maximumLength)
    {
        addIssue(&issues,
                 NativePlacementField::FixedLength,
                 NativePlacementIssueKind::ValidationError,
                 NativePlacementIssueCode::FixedLengthOutsideRange);
    }

    if (request.floatingMargin > 0 && !capabilities.supportsFloatingMargin)
    {
        addIssue(&issues,
                 NativePlacementField::FloatingMargin,
                 NativePlacementIssueKind::UnsupportedRequest,
                 NativePlacementIssueCode::CapabilityUnavailable);
    }
    if (reservedSpace.has_value() &&
        *reservedSpace != NativePanelReservedSpace::Automatic &&
        !capabilities.supportsReservedSpace)
    {
        addIssue(&issues,
                 NativePlacementField::ReservedSpace,
                 NativePlacementIssueKind::UnsupportedRequest,
                 NativePlacementIssueCode::CapabilityUnavailable);
    }

    NativePanelPlacementResult result;
    result.issues = issues;
    if (!hasIssueKind(issues, NativePlacementIssueKind::ValidationError))
    {
        result.placement = placement;
    }
    return result;
}

int edgeReserve(const QList<EdgePanel> &panels, const QString &edge, int screenIndex)
{
    int reserve = 8;
    for (const EdgePanel &panel : panels)
    {
        if (panel.visible && panel.edge == edge && panel.screenIndex == screenIndex)
        {
            reserve += qMax(0, panel.thickness) + 8;
        }
    }
    return reserve;
}

int edgeOffset(const QList<EdgePanel> &panels, const QString &panelId)
{
    const auto current = std::find_if(
        panels.cbegin(),
        panels.cend(),
        [&panelId](const EdgePanel &panel)
        {
            return panel.id == panelId;
        });
    if (current == panels.cend())
    {
        return 8;
    }

    int offset = 8;
    for (auto panel = panels.cbegin(); panel != current; ++panel)
    {
        if (panel->visible && panel->edge == current->edge &&
            panel->screenIndex == current->screenIndex)
        {
            offset += qMax(0, panel->thickness) + 8;
        }
    }
    return offset;
}
}
