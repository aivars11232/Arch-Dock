#include "PanelCapabilityResolver.h"

#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{

using namespace ArchDock;

template<typename T>
bool contains(const QVector<T> &values, T value)
{
    return std::find(values.cbegin(), values.cend(), value) != values.cend();
}

template<typename T>
void appendUnique(QVector<T> *values, T value)
{
    if (!contains(*values, value))
    {
        values->append(value);
    }
}

const std::array<PanelCapability,
                 static_cast<std::size_t>(PanelCapability::Count)> &allCapabilities()
{
    static const std::array<PanelCapability,
                            static_cast<std::size_t>(PanelCapability::Count)> values{
        PanelCapability::NativeEdgePlacement,
        PanelCapability::ArbitraryXyPlacement,
        PanelCapability::WholePanelRotation,
        PanelCapability::NonRectangularInput,
        PanelCapability::NativeAutoHide,
        PanelCapability::StandardPlasmaWidgets,
        PanelCapability::ThicknessMutation,
        PanelCapability::LengthMutation,
        PanelCapability::DynamicTint,
        PanelCapability::DynamicGlow,
        PanelCapability::IconStateStyling,
    };
    return values;
}

const std::array<PanelPresentationMechanism,
                 static_cast<std::size_t>(PanelPresentationMechanism::Count)> &
allPresentationMechanisms()
{
    static const std::array<PanelPresentationMechanism,
                            static_cast<std::size_t>(PanelPresentationMechanism::Count)> values{
        PanelPresentationMechanism::Open,
        PanelPresentationMechanism::CollapseHorizontal,
        PanelPresentationMechanism::CollapseVertical,
        PanelPresentationMechanism::CollapseRadial,
        PanelPresentationMechanism::Split,
        PanelPresentationMechanism::Shutter,
    };
    return values;
}

QVector<PanelLayoutKind> allLayouts()
{
    QVector<PanelLayoutKind> result;
    result.reserve(static_cast<qsizetype>(PanelLayoutKind::Count));
    for (int value = 0; value < static_cast<int>(PanelLayoutKind::Count); ++value)
    {
        result.append(static_cast<PanelLayoutKind>(value));
    }
    return result;
}

QVariantList decisionsToVariantList(const QVector<CapabilityDecision> &decisions)
{
    QVariantList result;
    result.reserve(decisions.size());
    for (const CapabilityDecision &decision : decisions)
    {
        result.append(decision.toVariantMap());
    }
    return result;
}

bool themeRequiredForCapability(PanelCapability capability)
{
    return capability == PanelCapability::NonRectangularInput ||
        capability == PanelCapability::DynamicTint ||
        capability == PanelCapability::DynamicGlow ||
        capability == PanelCapability::IconStateStyling;
}

RotationCapabilityDecision resolveRotation(const HostCapabilityProfile &host,
                                            const ThemeCapabilityProfile &theme)
{
    RotationCapabilityDecision decision;
    if (host.rotation.support == RotationSupport::None)
    {
        decision.reason = CapabilityReasonCode::HostCapabilityUnavailable;
        decision.blockedBy = host.id;
        return decision;
    }
    if (theme.rotation.support == RotationSupport::None)
    {
        decision.reason = CapabilityReasonCode::ThemeCapabilityUndeclared;
        decision.blockedBy = theme.id;
        return decision;
    }

    const bool bounded = host.rotation.support == RotationSupport::Bounded ||
        theme.rotation.support == RotationSupport::Bounded;
    decision.support = bounded ? RotationSupport::Bounded : RotationSupport::Arbitrary;
    decision.minimumDegrees = qMax(
        host.rotation.minimumDegrees,
        theme.rotation.minimumDegrees);
    decision.maximumDegrees = qMin(
        host.rotation.maximumDegrees,
        theme.rotation.maximumDegrees);
    if (decision.minimumDegrees > decision.maximumDegrees)
    {
        decision.support = RotationSupport::None;
        decision.reason = CapabilityReasonCode::RotationRangeIncompatible;
        decision.blockedBy = theme.id;
        return decision;
    }

    decision.available = true;
    return decision;
}

CapabilityDecision resolveControl(PanelCapability capability,
                                  const HostCapabilityProfile &host,
                                  const ThemeCapabilityProfile &theme,
                                  const RotationCapabilityDecision &rotation)
{
    CapabilityDecision decision;
    decision.id = panelCapabilityName(capability);
    if (capability == PanelCapability::WholePanelRotation)
    {
        decision.available = rotation.available;
        decision.reason = rotation.reason;
        decision.blockedBy = rotation.blockedBy;
        return decision;
    }
    if (!contains(host.capabilities, capability))
    {
        decision.reason = CapabilityReasonCode::HostCapabilityUnavailable;
        decision.blockedBy = host.id;
        return decision;
    }
    if (themeRequiredForCapability(capability) &&
        !contains(theme.capabilities, capability))
    {
        decision.reason = CapabilityReasonCode::ThemeCapabilityUndeclared;
        decision.blockedBy = theme.id;
        return decision;
    }

    decision.available = true;
    return decision;
}

CapabilityDecision resolveLayout(PanelLayoutKind layout,
                                 const HostCapabilityProfile &host,
                                 const ThemeCapabilityProfile &theme)
{
    CapabilityDecision decision;
    decision.id = panelLayoutKindName(layout);
    if (!contains(host.layouts, layout))
    {
        decision.reason = CapabilityReasonCode::HostLayoutUnsupported;
        decision.blockedBy = host.id;
        return decision;
    }
    if (!contains(theme.hostKinds, host.kind))
    {
        decision.reason = CapabilityReasonCode::ThemeHostUnsupported;
        decision.blockedBy = theme.id;
        return decision;
    }
    if (!contains(theme.layouts, layout))
    {
        decision.reason = CapabilityReasonCode::ThemeLayoutUnsupported;
        decision.blockedBy = theme.id;
        return decision;
    }

    decision.available = true;
    return decision;
}

CapabilityDecision resolvePresentation(
    PanelPresentationMechanism mechanism,
    const HostCapabilityProfile &host,
    const ThemeCapabilityProfile &theme)
{
    CapabilityDecision decision;
    decision.id = panelPresentationMechanismName(mechanism);
    if (mechanism == PanelPresentationMechanism::Open)
    {
        // Being open is not a capability. It is what a panel does when it is
        // not collapsed, so refusing it would mean refusing to show the panel
        // at all. Only the mechanisms that actually move something have to be
        // declared by both the host and the theme.
        decision.available = true;
        return decision;
    }
    if (!contains(host.presentationMechanisms, mechanism))
    {
        decision.reason = CapabilityReasonCode::PresentationMechanismUnavailable;
        decision.blockedBy = host.id;
        return decision;
    }
    if (!contains(theme.hostKinds, host.kind) ||
        !contains(theme.presentationMechanisms, mechanism))
    {
        decision.reason = CapabilityReasonCode::PresentationMechanismUnavailable;
        decision.blockedBy = theme.id;
        return decision;
    }

    decision.available = true;
    return decision;
}

const RendererAvailability *rendererForTier(
    const QVector<RendererAvailability> &renderers,
    RendererTier tier)
{
    const auto found = std::find_if(
        renderers.cbegin(),
        renderers.cend(),
        [tier](const RendererAvailability &renderer)
        {
            return renderer.tier == tier;
        });
    return found == renderers.cend() ? nullptr : &*found;
}

RendererCandidateDecision resolveRendererAvailability(
    RendererTier tier,
    const HostCapabilityProfile &host,
    const QVector<RendererAvailability> &renderers,
    const PlatformCapabilityProfile &platform)
{
    RendererCandidateDecision decision;
    decision.tier = tier;
    const RendererAvailability *renderer = rendererForTier(renderers, tier);
    if (!renderer || !renderer->installed)
    {
        decision.reason = CapabilityReasonCode::RendererNotInstalled;
        return decision;
    }
    if (!renderer->enabled)
    {
        decision.reason = CapabilityReasonCode::RendererDisabled;
        return decision;
    }
    if (!contains(host.rendererTiers, tier) ||
        !contains(renderer->hostKinds, host.kind))
    {
        decision.reason = CapabilityReasonCode::RendererHostUnsupported;
        return decision;
    }

    const bool operatingSystemSupported = renderer->operatingSystems.isEmpty() ||
        renderer->operatingSystems.contains(
            platform.operatingSystem,
            Qt::CaseInsensitive);
    if (!operatingSystemSupported ||
        platform.plasmaMajorVersion < renderer->minimumPlasmaMajorVersion ||
        (platform.wayland && !renderer->supportsWayland))
    {
        decision.reason = CapabilityReasonCode::RendererPlatformUnsupported;
        return decision;
    }

    decision.available = true;
    return decision;
}

RendererCandidateDecision resolveRendererCandidate(
    RendererTier tier,
    const HostCapabilityProfile &host,
    const ThemeCapabilityProfile &theme,
    const QVector<RendererAvailability> &renderers,
    const PlatformCapabilityProfile &platform)
{
    if (!contains(theme.rendererTiers, tier))
    {
        return {tier, false, CapabilityReasonCode::ThemeCapabilityUndeclared};
    }
    return resolveRendererAvailability(tier, host, renderers, platform);
}

RendererSelection resolveRenderer(
    RendererTier requestedTier,
    const HostCapabilityProfile &host,
    const ThemeCapabilityProfile &theme,
    const QVector<RendererAvailability> &renderers,
    const PlatformCapabilityProfile &platform)
{
    RendererSelection selection;
    selection.requestedTier = requestedTier;

    QVector<RendererTier> candidates{requestedTier};
    for (RendererTier fallback : theme.fallbackRendererTiers)
    {
        appendUnique(&candidates, fallback);
    }

    CapabilityReasonCode requestedReason = CapabilityReasonCode::None;
    for (RendererTier candidate : candidates)
    {
        const RendererCandidateDecision decision = resolveRendererCandidate(
            candidate, host, theme, renderers, platform);
        selection.evaluatedTiers.append(decision);
        if (candidate == requestedTier)
        {
            requestedReason = decision.reason;
        }
        if (decision.available)
        {
            selection.effectiveTier = candidate;
            selection.fallbackApplied = candidate != requestedTier;
            selection.reason = selection.fallbackApplied
                ? requestedReason
                : CapabilityReasonCode::None;
            return selection;
        }
    }

    selection.reason = CapabilityReasonCode::NoSafeRendererFallback;
    return selection;
}

std::optional<PanelPresentationMechanism> requestedPresentation(
    const PanelPresentationDefinition &presentation)
{
    if (!presentation.collapseMechanism.trimmed().isEmpty())
    {
        return panelPresentationMechanismFromName(
            presentation.collapseMechanism);
    }
    if (!presentation.mode.trimmed().isEmpty())
    {
        return panelPresentationMechanismFromName(presentation.mode);
    }
    return std::nullopt;
}

const CapabilityDecision *decisionById(const QVector<CapabilityDecision> &decisions,
                                       const QString &id)
{
    const auto found = std::find_if(
        decisions.cbegin(),
        decisions.cend(),
        [&id](const CapabilityDecision &decision)
        {
            return decision.id == id;
        });
    return found == decisions.cend() ? nullptr : &*found;
}

void setError(QString *errorCode, const QString &value)
{
    if (errorCode)
    {
        *errorCode = value;
    }
}

template<typename T, typename Parser>
bool parseEnumList(const QVariant &value, QVector<T> *result, Parser parser)
{
    const QVariantList candidates = value.toList();
    for (const QVariant &candidate : candidates)
    {
        const std::optional<T> parsed = parser(candidate.toString());
        if (!parsed.has_value())
        {
            return false;
        }
        appendUnique(result, *parsed);
    }
    return true;
}

std::optional<PanelHostKind> hostKindFromCapabilityName(const QString &name)
{
    const QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("native-edge"))
    {
        return PanelHostKind::NativeEdge;
    }
    if (normalized == QStringLiteral("free-desktop"))
    {
        return PanelHostKind::FreeDesktop;
    }
    return std::nullopt;
}

RotationCapability parseRotation(const QVariantMap &rotation, bool *ok)
{
    RotationCapability result;
    const QString mode = rotation.value(QStringLiteral("mode"),
                                        QStringLiteral("none"))
                             .toString()
                             .trimmed()
                             .toLower();
    if (mode == QStringLiteral("none"))
    {
        *ok = true;
        return result;
    }
    if (mode == QStringLiteral("bounded"))
    {
        result.support = RotationSupport::Bounded;
    }
    else if (mode == QStringLiteral("free") ||
             mode == QStringLiteral("arbitrary"))
    {
        result.support = RotationSupport::Arbitrary;
    }
    else
    {
        *ok = false;
        return result;
    }

    bool minimumOk = false;
    bool maximumOk = false;
    result.minimumDegrees = rotation.value(
        QStringLiteral("minimumDegrees"), -180.0).toDouble(&minimumOk);
    result.maximumDegrees = rotation.value(
        QStringLiteral("maximumDegrees"), 180.0).toDouble(&maximumOk);
    *ok = minimumOk && maximumOk &&
        std::isfinite(result.minimumDegrees) &&
        std::isfinite(result.maximumDegrees) &&
        result.minimumDegrees <= result.maximumDegrees;
    return result;
}

}

namespace ArchDock
{

QVariantMap CapabilityDecision::toVariantMap() const
{
    return {
        {QStringLiteral("available"), available},
        {QStringLiteral("blockedBy"), blockedBy},
        {QStringLiteral("id"), id},
        {QStringLiteral("reasonCode"), capabilityReasonCodeName(reason)},
    };
}

QVariantMap RotationCapabilityDecision::toVariantMap() const
{
    return {
        {QStringLiteral("available"), available},
        {QStringLiteral("blockedBy"), blockedBy},
        {QStringLiteral("maximumDegrees"), maximumDegrees},
        {QStringLiteral("minimumDegrees"), minimumDegrees},
        {QStringLiteral("reasonCode"), capabilityReasonCodeName(reason)},
        {QStringLiteral("support"), rotationSupportName(support)},
    };
}

QVariantMap RendererCandidateDecision::toVariantMap() const
{
    return {
        {QStringLiteral("available"), available},
        {QStringLiteral("reasonCode"), capabilityReasonCodeName(reason)},
        {QStringLiteral("tier"), rendererTierName(tier)},
    };
}

QVariantMap RendererSelection::toVariantMap() const
{
    QVariantList evaluated;
    evaluated.reserve(evaluatedTiers.size());
    for (const RendererCandidateDecision &decision : evaluatedTiers)
    {
        evaluated.append(decision.toVariantMap());
    }
    return {
        {QStringLiteral("effectiveTier"), effectiveTier.has_value()
             ? rendererTierName(*effectiveTier)
             : QString{}},
        {QStringLiteral("evaluatedTiers"), evaluated},
        {QStringLiteral("fallbackApplied"), fallbackApplied},
        {QStringLiteral("reasonCode"), capabilityReasonCodeName(reason)},
        {QStringLiteral("requestedTier"), rendererTierName(requestedTier)},
    };
}

QVariantMap CapabilityResolution::toVariantMap() const
{
    QVariantList serializedRendererChoices;
    serializedRendererChoices.reserve(rendererChoices.size());
    for (const RendererCandidateDecision &decision : rendererChoices)
    {
        serializedRendererChoices.append(decision.toVariantMap());
    }
    return {
        {QStringLiteral("available"), available},
        {QStringLiteral("controls"), decisionsToVariantList(controls)},
        {QStringLiteral("hostProfileId"), hostProfileId},
        {QStringLiteral("layouts"), decisionsToVariantList(layouts)},
        {QStringLiteral("presentationMechanisms"),
         decisionsToVariantList(presentationMechanisms)},
        {QStringLiteral("reasonCode"), capabilityReasonCodeName(reason)},
        {QStringLiteral("renderer"), renderer.toVariantMap()},
        {QStringLiteral("rendererChoices"), serializedRendererChoices},
        {QStringLiteral("rotation"), rotation.toVariantMap()},
        {QStringLiteral("themeId"), themeId},
    };
}

QString panelCapabilityName(PanelCapability capability)
{
    switch (capability)
    {
    case PanelCapability::NativeEdgePlacement:
        return QStringLiteral("native-edge-placement");
    case PanelCapability::ArbitraryXyPlacement:
        return QStringLiteral("arbitrary-xy-placement");
    case PanelCapability::WholePanelRotation:
        return QStringLiteral("whole-panel-rotation");
    case PanelCapability::NonRectangularInput:
        return QStringLiteral("nonrectangular-input");
    case PanelCapability::NativeAutoHide:
        return QStringLiteral("native-auto-hide");
    case PanelCapability::StandardPlasmaWidgets:
        return QStringLiteral("standard-plasma-widgets");
    case PanelCapability::ThicknessMutation:
        return QStringLiteral("thickness-mutation");
    case PanelCapability::LengthMutation:
        return QStringLiteral("length-mutation");
    case PanelCapability::DynamicTint:
        return QStringLiteral("dynamic-tint");
    case PanelCapability::DynamicGlow:
        return QStringLiteral("dynamic-glow");
    case PanelCapability::IconStateStyling:
        return QStringLiteral("icon-state-styling");
    case PanelCapability::Count:
        break;
    }
    return QString{};
}

std::optional<PanelCapability> panelCapabilityFromName(const QString &name)
{
    const QString normalized = name.trimmed().toLower();
    for (PanelCapability capability : allCapabilities())
    {
        if (normalized == panelCapabilityName(capability))
        {
            return capability;
        }
    }
    return std::nullopt;
}

QString panelPresentationMechanismName(PanelPresentationMechanism mechanism)
{
    switch (mechanism)
    {
    case PanelPresentationMechanism::Open:
        return QStringLiteral("open");
    case PanelPresentationMechanism::CollapseHorizontal:
        return QStringLiteral("collapse-horizontal");
    case PanelPresentationMechanism::CollapseVertical:
        return QStringLiteral("collapse-vertical");
    case PanelPresentationMechanism::CollapseRadial:
        return QStringLiteral("collapse-radial");
    case PanelPresentationMechanism::Split:
        return QStringLiteral("split");
    case PanelPresentationMechanism::Shutter:
        return QStringLiteral("shutter");
    case PanelPresentationMechanism::Count:
        break;
    }
    return QString{};
}

std::optional<PanelPresentationMechanism> panelPresentationMechanismFromName(
    const QString &name)
{
    QString normalized = name.trimmed().toLower();
    if (normalized == QStringLiteral("horizontal"))
    {
        normalized = QStringLiteral("collapse-horizontal");
    }
    else if (normalized == QStringLiteral("vertical"))
    {
        normalized = QStringLiteral("collapse-vertical");
    }
    else if (normalized == QStringLiteral("radial"))
    {
        normalized = QStringLiteral("collapse-radial");
    }
    for (PanelPresentationMechanism mechanism : allPresentationMechanisms())
    {
        if (normalized == panelPresentationMechanismName(mechanism))
        {
            return mechanism;
        }
    }
    return std::nullopt;
}

QString rendererTierName(RendererTier tier)
{
    switch (tier)
    {
    case RendererTier::Procedural2D:
        return QStringLiteral("procedural2d");
    case RendererTier::Skinned2D:
        return QStringLiteral("skinned2d");
    case RendererTier::Baked2_5D:
        return QStringLiteral("baked2.5d");
    case RendererTier::True3D:
        return QStringLiteral("true3d");
    case RendererTier::Count:
        break;
    }
    return QString{};
}

std::optional<RendererTier> rendererTierFromName(const QString &name)
{
    const QString normalized = name.trimmed().toLower();
    for (int value = 0; value < static_cast<int>(RendererTier::Count); ++value)
    {
        const auto tier = static_cast<RendererTier>(value);
        if (normalized == rendererTierName(tier))
        {
            return tier;
        }
    }
    return std::nullopt;
}

QString rotationSupportName(RotationSupport support)
{
    switch (support)
    {
    case RotationSupport::None:
        return QStringLiteral("none");
    case RotationSupport::Bounded:
        return QStringLiteral("bounded");
    case RotationSupport::Arbitrary:
        return QStringLiteral("arbitrary");
    }
    return QStringLiteral("none");
}

QString capabilityReasonCodeName(CapabilityReasonCode reason)
{
    switch (reason)
    {
    case CapabilityReasonCode::None:
        return QStringLiteral("available");
    case CapabilityReasonCode::InvalidCapabilityInput:
        return QStringLiteral("invalid-capability-input");
    case CapabilityReasonCode::PlatformUnsupported:
        return QStringLiteral("platform-unsupported");
    case CapabilityReasonCode::HostCapabilityUnavailable:
        return QStringLiteral("host-capability-unavailable");
    case CapabilityReasonCode::HostLayoutUnsupported:
        return QStringLiteral("host-layout-unsupported");
    case CapabilityReasonCode::ThemeCapabilityUndeclared:
        return QStringLiteral("theme-capability-undeclared");
    case CapabilityReasonCode::ThemeHostUnsupported:
        return QStringLiteral("theme-host-unsupported");
    case CapabilityReasonCode::ThemeLayoutUnsupported:
        return QStringLiteral("theme-layout-unsupported");
    case CapabilityReasonCode::RendererNotInstalled:
        return QStringLiteral("renderer-not-installed");
    case CapabilityReasonCode::RendererDisabled:
        return QStringLiteral("renderer-disabled");
    case CapabilityReasonCode::RendererHostUnsupported:
        return QStringLiteral("renderer-host-unsupported");
    case CapabilityReasonCode::RendererPlatformUnsupported:
        return QStringLiteral("renderer-platform-unsupported");
    case CapabilityReasonCode::PresentationMechanismUnavailable:
        return QStringLiteral("presentation-mechanism-unavailable");
    case CapabilityReasonCode::RotationRangeIncompatible:
        return QStringLiteral("rotation-range-incompatible");
    case CapabilityReasonCode::NoSafeRendererFallback:
        return QStringLiteral("no-safe-renderer-fallback");
    }
    return QStringLiteral("invalid-capability-input");
}

HostCapabilityProfile PanelCapabilityResolver::productionHostProfile(
    PanelHostKind kind)
{
    HostCapabilityProfile profile;
    profile.kind = kind;
    if (kind == PanelHostKind::FreeDesktop)
    {
        profile.id = QStringLiteral("free-desktop-plasma6-wayland");
        profile.capabilities = {
            PanelCapability::ArbitraryXyPlacement,
            PanelCapability::WholePanelRotation,
            PanelCapability::DynamicTint,
            PanelCapability::DynamicGlow,
            PanelCapability::IconStateStyling,
        };
        profile.layouts = allLayouts();
        // The free host animates entirely inside its own desktop applet, so
        // every declared mechanism is reachable here. Radial is included only
        // for this host: it needs a surface that is not a fixed rectangle.
        profile.presentationMechanisms = {
            PanelPresentationMechanism::Open,
            PanelPresentationMechanism::CollapseHorizontal,
            PanelPresentationMechanism::CollapseVertical,
            PanelPresentationMechanism::CollapseRadial,
            PanelPresentationMechanism::Split,
            PanelPresentationMechanism::Shutter,
        };
        // Baked 2.5D is offered on the free desktop host only. A perspective
        // platform needs a surface that is not a fixed rectangle, which is
        // exactly what a Plasma edge panel cannot give it.
        profile.rendererTiers = {
            RendererTier::Procedural2D,
            RendererTier::Skinned2D,
            RendererTier::Baked2_5D,
        };
        profile.rotation = {RotationSupport::Arbitrary, -180.0, 180.0};
        return profile;
    }

    profile.id = QStringLiteral("native-edge-plasma6-wayland");
    profile.capabilities = {
        PanelCapability::NativeEdgePlacement,
        PanelCapability::NativeAutoHide,
        PanelCapability::StandardPlasmaWidgets,
        PanelCapability::ThicknessMutation,
        PanelCapability::LengthMutation,
        PanelCapability::DynamicTint,
        PanelCapability::DynamicGlow,
        PanelCapability::IconStateStyling,
    };
    profile.layouts = {
        PanelLayoutKind::Adaptive,
        PanelLayoutKind::Horizontal,
        PanelLayoutKind::Vertical,
    };
    // A native panel collapses its own surface inside a containment whose
    // geometry stays fixed, so the linear mechanisms are all available. Radial
    // is not: a Plasma edge panel is a rectangle and an iris inside it would
    // be a decoration pretending to be a mechanism.
    profile.presentationMechanisms = {
        PanelPresentationMechanism::Open,
        PanelPresentationMechanism::CollapseHorizontal,
        PanelPresentationMechanism::CollapseVertical,
        PanelPresentationMechanism::Split,
        PanelPresentationMechanism::Shutter,
    };
    profile.rendererTiers = {
        RendererTier::Procedural2D,
        RendererTier::Skinned2D,
    };
    return profile;
}

ThemeCapabilityProfile PanelCapabilityResolver::proceduralThemeProfile(
    const QString &id)
{
    ThemeCapabilityProfile profile;
    profile.id = id.trimmed().isEmpty()
        ? QStringLiteral("procedural-default")
        : id.trimmed();
    profile.hostKinds = {
        PanelHostKind::NativeEdge,
        PanelHostKind::FreeDesktop,
    };
    profile.capabilities = {
        PanelCapability::DynamicTint,
        PanelCapability::IconStateStyling,
    };
    profile.layouts = allLayouts();
    profile.rendererTiers = {RendererTier::Procedural2D};
    profile.preferredRendererTier = RendererTier::Procedural2D;
    profile.rotation = {RotationSupport::Bounded, -180.0, 180.0};
    return profile;
}

ThemeCapabilityProfile PanelCapabilityResolver::legacyThemeProfile(
    const PanelDefinition &definition)
{
    const bool hasManagedArtwork = !definition.surface.themeAsset.trimmed().isEmpty() ||
        !definition.surface.themeSource.trimmed().isEmpty() ||
        !definition.surface.themePackageManifest.trimmed().isEmpty();
    if (!hasManagedArtwork)
    {
        const QString id = !definition.surface.completeThemeId.trimmed().isEmpty()
            ? definition.surface.completeThemeId
            : definition.surface.panelThemeId;
        return proceduralThemeProfile(id);
    }

    ThemeCapabilityProfile profile = proceduralThemeProfile(
        !definition.surface.themePackageId.trimmed().isEmpty()
            ? definition.surface.themePackageId
            : QStringLiteral("legacy-theme-v1"));
    return profile;
}

std::optional<ThemeCapabilityProfile>
PanelCapabilityResolver::themeProfileFromVariantMap(const QVariantMap &theme,
                                                    QString *errorCode)
{
    ThemeCapabilityProfile profile;
    profile.id = theme.value(QStringLiteral("id")).toString().trimmed();
    const QVariantMap capabilities = theme.value(
        QStringLiteral("capabilities")).toMap();
    if (profile.id.isEmpty() || capabilities.isEmpty())
    {
        setError(errorCode, QStringLiteral("invalid-capability-input"));
        return std::nullopt;
    }

    if (!parseEnumList(
            capabilities.value(QStringLiteral("hosts")),
            &profile.hostKinds,
            hostKindFromCapabilityName) ||
        profile.hostKinds.isEmpty() ||
        !parseEnumList(
            capabilities.value(QStringLiteral("layouts")),
            &profile.layouts,
            panelLayoutKindFromName) ||
        profile.layouts.isEmpty() ||
        !parseEnumList(
            capabilities.value(QStringLiteral("features"), QVariantList{}),
            &profile.capabilities,
            panelCapabilityFromName) ||
        !parseEnumList(
            capabilities.value(
                QStringLiteral("presentationMechanisms"), QVariantList{}),
            &profile.presentationMechanisms,
            panelPresentationMechanismFromName) ||
        !parseEnumList(
            capabilities.value(QStringLiteral("rendererTiers")),
            &profile.rendererTiers,
            rendererTierFromName) ||
        profile.rendererTiers.isEmpty())
    {
        setError(errorCode, QStringLiteral("invalid-capability-input"));
        return std::nullopt;
    }

    const QString preferredName = capabilities.value(
        QStringLiteral("preferredRendererTier")).toString();
    const std::optional<RendererTier> preferred = rendererTierFromName(preferredName);
    if (!preferred.has_value() || !contains(profile.rendererTiers, *preferred))
    {
        setError(errorCode, QStringLiteral("invalid-capability-input"));
        return std::nullopt;
    }
    profile.preferredRendererTier = *preferred;

    if (!parseEnumList(
            capabilities.value(
                QStringLiteral("fallbackRendererTiers"), QVariantList{}),
            &profile.fallbackRendererTiers,
            rendererTierFromName))
    {
        setError(errorCode, QStringLiteral("invalid-capability-input"));
        return std::nullopt;
    }
    for (RendererTier fallback : profile.fallbackRendererTiers)
    {
        if (!contains(profile.rendererTiers, fallback))
        {
            setError(errorCode, QStringLiteral("invalid-capability-input"));
            return std::nullopt;
        }
    }

    bool rotationOk = false;
    profile.rotation = parseRotation(
        capabilities.value(QStringLiteral("rotation")).toMap(),
        &rotationOk);
    if (!rotationOk)
    {
        setError(errorCode, QStringLiteral("invalid-capability-input"));
        return std::nullopt;
    }

    setError(errorCode, QString{});
    return profile;
}

QVector<RendererAvailability> PanelCapabilityResolver::productionRenderers()
{
    return {
        {RendererTier::Procedural2D,
         true,
         true,
         {PanelHostKind::NativeEdge, PanelHostKind::FreeDesktop},
         {QStringLiteral("arch")},
         6,
         true},
        {RendererTier::Skinned2D,
         true,
         true,
         {PanelHostKind::NativeEdge, PanelHostKind::FreeDesktop},
         {QStringLiteral("arch")},
         6,
         true},
        {RendererTier::Baked2_5D,
         true,
         true,
         {PanelHostKind::FreeDesktop},
         {QStringLiteral("arch")},
         6,
         true},
        {RendererTier::True3D,
         false,
         false,
         {PanelHostKind::FreeDesktop},
         {QStringLiteral("arch")},
         6,
         true},
    };
}

PlatformCapabilityProfile PanelCapabilityResolver::productionPlatform()
{
    return {};
}

CapabilityResolution PanelCapabilityResolver::resolve(
    const PanelDefinition &definition,
    const HostCapabilityProfile &host,
    const ThemeCapabilityProfile &theme,
    const QVector<RendererAvailability> &renderers,
    const PlatformCapabilityProfile &platform)
{
    CapabilityResolution result;
    result.hostProfileId = host.id;
    result.themeId = theme.id;
    result.rotation = resolveRotation(host, theme);

    for (PanelCapability capability : allCapabilities())
    {
        result.controls.append(resolveControl(
            capability, host, theme, result.rotation));
    }
    for (PanelLayoutKind layout : allLayouts())
    {
        result.layouts.append(resolveLayout(layout, host, theme));
    }
    for (PanelPresentationMechanism mechanism : allPresentationMechanisms())
    {
        result.presentationMechanisms.append(resolvePresentation(
            mechanism, host, theme));
    }
    for (int value = 0; value < static_cast<int>(RendererTier::Count); ++value)
    {
        result.rendererChoices.append(resolveRendererAvailability(
            static_cast<RendererTier>(value),
            host,
            renderers,
            platform));
    }

    const QString requestedTierName = definition.surface.rendererTier.trimmed();
    const std::optional<RendererTier> parsedRequestedTier = requestedTierName.isEmpty()
        ? theme.preferredRendererTier
        : rendererTierFromName(requestedTierName);
    const RendererTier requestedTier = parsedRequestedTier.value_or(
        RendererTier::Procedural2D);
    result.renderer = resolveRenderer(
        requestedTier, host, theme, renderers, platform);

    if (definition.host.kind != host.kind || host.id.trimmed().isEmpty() ||
        theme.id.trimmed().isEmpty() || platform.operatingSystem.trimmed().isEmpty() ||
        (!requestedTierName.isEmpty() && !parsedRequestedTier.has_value()))
    {
        result.reason = CapabilityReasonCode::InvalidCapabilityInput;
        return result;
    }

    const std::optional<PanelLayoutKind> requestedLayout = panelLayoutKindFromName(
        definition.layout.pathType);
    if (!requestedLayout.has_value())
    {
        result.reason = CapabilityReasonCode::InvalidCapabilityInput;
        return result;
    }
    const CapabilityDecision *layoutDecision = decisionById(
        result.layouts, panelLayoutKindName(*requestedLayout));
    if (!layoutDecision || !layoutDecision->available)
    {
        result.reason = layoutDecision
            ? layoutDecision->reason
            : CapabilityReasonCode::InvalidCapabilityInput;
        return result;
    }

    if (!qFuzzyIsNull(definition.layout.angle) && !result.rotation.available)
    {
        result.reason = result.rotation.reason;
        return result;
    }

    const bool presentationRequested =
        !definition.presentation.mode.trimmed().isEmpty() ||
        !definition.presentation.collapseMechanism.trimmed().isEmpty();
    const std::optional<PanelPresentationMechanism> presentation =
        requestedPresentation(definition.presentation);
    if (presentationRequested && !presentation.has_value())
    {
        result.reason = CapabilityReasonCode::InvalidCapabilityInput;
        return result;
    }
    if (presentation.has_value())
    {
        const CapabilityDecision *presentationDecision = decisionById(
            result.presentationMechanisms,
            panelPresentationMechanismName(*presentation));
        if (!presentationDecision || !presentationDecision->available)
        {
            result.reason = presentationDecision
                ? presentationDecision->reason
                : CapabilityReasonCode::PresentationMechanismUnavailable;
            return result;
        }
    }

    if (!result.renderer.effectiveTier.has_value())
    {
        result.reason = CapabilityReasonCode::NoSafeRendererFallback;
        return result;
    }

    result.available = true;
    return result;
}

}
