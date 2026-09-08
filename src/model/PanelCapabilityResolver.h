#pragma once

#include "PanelDefinition.h"

#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

enum class PanelCapability
{
    NativeEdgePlacement,
    ArbitraryXyPlacement,
    WholePanelRotation,
    NonRectangularInput,
    NativeAutoHide,
    StandardPlasmaWidgets,
    ThicknessMutation,
    LengthMutation,
    DynamicTint,
    DynamicGlow,
    IconStateStyling,
    Count
};

enum class PanelPresentationMechanism
{
    Open,
    CollapseHorizontal,
    CollapseVertical,
    CollapseRadial,
    Split,
    Shutter,
    Count
};

enum class RendererTier
{
    Procedural2D,
    Skinned2D,
    Baked2_5D,
    True3D,
    Count
};

enum class RotationSupport
{
    None,
    Bounded,
    Arbitrary
};

enum class CapabilityReasonCode
{
    None,
    InvalidCapabilityInput,
    PlatformUnsupported,
    HostCapabilityUnavailable,
    HostLayoutUnsupported,
    ThemeCapabilityUndeclared,
    ThemeHostUnsupported,
    ThemeLayoutUnsupported,
    RendererNotInstalled,
    RendererDisabled,
    RendererSceneUnavailable,
    RendererHostUnsupported,
    RendererPlatformUnsupported,
    PresentationMechanismUnavailable,
    RotationRangeIncompatible,
    NoSafeRendererFallback
};

struct RotationCapability
{
    RotationSupport support = RotationSupport::None;
    qreal minimumDegrees = 0.0;
    qreal maximumDegrees = 0.0;

    bool operator==(const RotationCapability &) const = default;
};

struct HostCapabilityProfile
{
    QString id;
    PanelHostKind kind = PanelHostKind::NativeEdge;
    QVector<PanelCapability> capabilities;
    QVector<PanelLayoutKind> layouts;
    QVector<PanelPresentationMechanism> presentationMechanisms;
    QVector<RendererTier> rendererTiers;
    RotationCapability rotation;

    bool operator==(const HostCapabilityProfile &) const = default;
};

struct ThemeCapabilityProfile
{
    QString id;
    QVector<PanelHostKind> hostKinds;
    QVector<PanelCapability> capabilities;
    QVector<PanelLayoutKind> layouts;
    QVector<PanelPresentationMechanism> presentationMechanisms;
    QVector<RendererTier> rendererTiers;
    std::optional<RendererTier> preferredRendererTier;
    QVector<RendererTier> fallbackRendererTiers;
    RotationCapability rotation;

    bool operator==(const ThemeCapabilityProfile &) const = default;
};

struct RendererAvailability
{
    RendererTier tier = RendererTier::Procedural2D;
    bool installed = false;
    bool enabled = false;
    QVector<PanelHostKind> hostKinds;
    QStringList operatingSystems;
    int minimumPlasmaMajorVersion = 6;
    bool supportsWayland = true;
    // Build eligibility only; each consumer also checks its engine/window.
    bool sceneImplemented = true;

    bool operator==(const RendererAvailability &) const = default;
};

struct PlatformCapabilityProfile
{
    QString operatingSystem = QStringLiteral("arch");
    int plasmaMajorVersion = 6;
    bool wayland = true;

    bool operator==(const PlatformCapabilityProfile &) const = default;
};

struct CapabilityDecision
{
    QString id;
    bool available = false;
    CapabilityReasonCode reason = CapabilityReasonCode::None;
    QString blockedBy;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const CapabilityDecision &) const = default;
};

struct RotationCapabilityDecision
{
    bool available = false;
    RotationSupport support = RotationSupport::None;
    qreal minimumDegrees = 0.0;
    qreal maximumDegrees = 0.0;
    CapabilityReasonCode reason = CapabilityReasonCode::None;
    QString blockedBy;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const RotationCapabilityDecision &) const = default;
};

struct RendererCandidateDecision
{
    RendererTier tier = RendererTier::Procedural2D;
    bool available = false;
    CapabilityReasonCode reason = CapabilityReasonCode::None;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const RendererCandidateDecision &) const = default;
};

struct RendererSelection
{
    RendererTier requestedTier = RendererTier::Procedural2D;
    std::optional<RendererTier> effectiveTier;
    bool fallbackApplied = false;
    CapabilityReasonCode reason = CapabilityReasonCode::None;
    QVector<RendererCandidateDecision> evaluatedTiers;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const RendererSelection &) const = default;
};

struct CapabilityResolution
{
    bool available = false;
    CapabilityReasonCode reason = CapabilityReasonCode::None;
    QString hostProfileId;
    QString themeId;
    QVector<CapabilityDecision> controls;
    QVector<CapabilityDecision> layouts;
    QVector<CapabilityDecision> presentationMechanisms;
    RotationCapabilityDecision rotation;
    RendererSelection renderer;
    QVector<RendererCandidateDecision> rendererChoices;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const CapabilityResolution &) const = default;
};

[[nodiscard]] QString panelCapabilityName(PanelCapability capability);
[[nodiscard]] std::optional<PanelCapability> panelCapabilityFromName(
    const QString &name);
[[nodiscard]] QString panelPresentationMechanismName(
    PanelPresentationMechanism mechanism);
[[nodiscard]] std::optional<PanelPresentationMechanism>
panelPresentationMechanismFromName(const QString &name);
[[nodiscard]] QString rendererTierName(RendererTier tier);
[[nodiscard]] std::optional<RendererTier> rendererTierFromName(const QString &name);
[[nodiscard]] QString rotationSupportName(RotationSupport support);
[[nodiscard]] QString capabilityReasonCodeName(CapabilityReasonCode reason);

class PanelCapabilityResolver
{
public:
    [[nodiscard]] static HostCapabilityProfile productionHostProfile(
        PanelHostKind kind);
    [[nodiscard]] static ThemeCapabilityProfile proceduralThemeProfile(
        const QString &id = QStringLiteral("procedural-default"));
    [[nodiscard]] static ThemeCapabilityProfile legacyThemeProfile(
        const PanelDefinition &definition);
    [[nodiscard]] static std::optional<ThemeCapabilityProfile>
    themeProfileFromVariantMap(const QVariantMap &theme,
                               QString *errorCode = nullptr);
    [[nodiscard]] static QVector<RendererAvailability> productionRenderers();
    [[nodiscard]] static PlatformCapabilityProfile productionPlatform();
    [[nodiscard]] static CapabilityResolution resolve(
        const PanelDefinition &definition,
        const HostCapabilityProfile &host,
        const ThemeCapabilityProfile &theme,
        const QVector<RendererAvailability> &renderers,
        const PlatformCapabilityProfile &platform);
};

}
