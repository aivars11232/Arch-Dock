#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace ArchDock
{
inline constexpr int kNativePanelMinimumDimension = 48;
inline constexpr int kNativePanelMaximumDimension = 4096;

enum class NativePanelEdge
{
    Top,
    Bottom,
    Left,
    Right,
};

enum class NativePanelAlignment
{
    Start,
    Center,
    End,
};

enum class NativePanelLengthMode
{
    Fit,
    Fixed,
    Fill,
};

enum class NativePanelVisibilityMode
{
    Always,
    AutoHide,
    Dodge,
    Cover,
};

enum class NativePanelReservedSpace
{
    Automatic,
    Reserve,
    DoNotReserve,
};

enum class NativePlacementField
{
    ScreenStableId,
    ScreenFallbackIndex,
    Edge,
    Alignment,
    Offset,
    Thickness,
    LengthMode,
    MinimumLength,
    MaximumLength,
    FixedLength,
    FloatingMargin,
    VisibilityMode,
    ReservedSpace,
};

enum class NativePlacementIssueKind
{
    ValidationError,
    UnsupportedRequest,
};

enum class NativePlacementIssueCode
{
    UnknownAlias,
    OutOfRange,
    ConflictingValues,
    MinimumExceedsMaximum,
    FixedLengthOutsideRange,
    CapabilityUnavailable,
};

struct NativePanelScreenTarget
{
    QString stableId;
    int fallbackIndex = 0;
};

struct NativePanelPlacement
{
    NativePanelScreenTarget screen;
    NativePanelEdge edge = NativePanelEdge::Bottom;
    NativePanelAlignment alignment = NativePanelAlignment::Center;
    int offset = 0;
    int thickness = 76;
    NativePanelLengthMode lengthMode = NativePanelLengthMode::Fixed;
    int minimumLength = kNativePanelMinimumDimension;
    int maximumLength = kNativePanelMaximumDimension;
    int fixedLength = 720;
    int floatingMargin = 0;
    NativePanelVisibilityMode visibilityMode = NativePanelVisibilityMode::Always;
    NativePanelReservedSpace reservedSpace = NativePanelReservedSpace::Automatic;
};

struct NativePanelPlacementRequest
{
    QString screenStableId;
    int screenFallbackIndex = 0;
    QString edge;
    QString alignment;
    int offset = 0;
    int thickness = 76;
    QString lengthMode;
    std::optional<bool> dynamicLength;
    int minimumLength = kNativePanelMinimumDimension;
    int maximumLength = kNativePanelMaximumDimension;
    int fixedLength = 720;
    int floatingMargin = 0;
    QString visibilityMode;
    QString reservedSpace;
};

struct NativePlacementCapabilities
{
    bool supportsFloatingMargin = false;
    bool supportsReservedSpace = false;
};

struct NativePlacementIssue
{
    NativePlacementField field = NativePlacementField::Edge;
    NativePlacementIssueKind kind = NativePlacementIssueKind::ValidationError;
    NativePlacementIssueCode code = NativePlacementIssueCode::UnknownAlias;
};

struct NativePanelPlacementResult
{
    std::optional<NativePanelPlacement> placement;
    QList<NativePlacementIssue> issues;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isSupported() const;
};

[[nodiscard]] NativePanelPlacement defaultNativePanelPlacement();
[[nodiscard]] NativePanelPlacementResult normalizeNativePanelPlacement(
    const NativePanelPlacementRequest &request,
    const NativePlacementCapabilities &capabilities = {});

struct EdgePanel
{
    QString id;
    QString edge;
    int screenIndex = -1;
    int thickness = 0;
    bool visible = false;
};

int edgeReserve(const QList<EdgePanel> &panels, const QString &edge, int screenIndex);
int edgeOffset(const QList<EdgePanel> &panels, const QString &panelId);
}
