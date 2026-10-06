// Whether each preset can be used on a panel, and how it would be drawn.
#include "PresetCapabilityResolver.h"

#include "../iconstyles/IconStylePackage.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

namespace
{

using namespace ArchDock;
using namespace ArchDock::PresetParsing;

// Fields only a procedural layer has. The renderer ignores them on an asset
// layer, so overriding one there would be a setting that silently does
// nothing.
bool isProceduralOnlyField(const QString &field)
{
    return field == QStringLiteral("shape") || field == QStringLiteral("color") ||
        field == QStringLiteral("secondaryColor") ||
        field == QStringLiteral("radius");
}

void applyLayerOverride(QVariantMap *layer,
                        const QVariantMap &fields,
                        const QString &pointer,
                        QVector<PresetValidationDiagnostic> *diagnostics)
{
    const bool assetLayer = layer->value(QStringLiteral("kind")).toString() ==
        QStringLiteral("asset");
    for (auto field = fields.cbegin(); field != fields.cend(); ++field)
    {
        if (assetLayer && isProceduralOnlyField(field.key()))
        {
            addDiagnostic(diagnostics, QStringLiteral("invalid-value"),
                          pointerChild(pointer, field.key()),
                          QStringLiteral("an asset layer has no procedural fields"));
            continue;
        }
        layer->insert(field.key(), field.value());
    }
}

std::optional<QVariantMap> findTheme(const QVariantList &themeCatalog,
                                     const QString &themeId)
{
    for (const QVariant &candidate : themeCatalog)
    {
        const QVariantMap theme = candidate.toMap();
        if (theme.value(QStringLiteral("id")).toString() == themeId)
        {
            return theme;
        }
    }
    return std::nullopt;
}

// No theme reference means the built-in procedural surface.
std::optional<ThemeCapabilityProfile> themeProfile(const QVariantList &themeCatalog,
                                                   const QString &themeId)
{
    if (themeId.isEmpty())
    {
        return PanelCapabilityResolver::proceduralThemeProfile();
    }
    const std::optional<QVariantMap> theme = findTheme(themeCatalog, themeId);
    return theme.has_value()
        ? PanelCapabilityResolver::themeProfileFromVariantMap(*theme)
        : std::nullopt;
}

bool decisionAvailable(const QVector<CapabilityDecision> &decisions,
                       const QString &id)
{
    return std::any_of(decisions.cbegin(), decisions.cend(),
                       [&id](const CapabilityDecision &decision)
                       {
                           return decision.id == id && decision.available;
                       });
}

// A capability name is a renderer tier, a panel capability or a presentation
// mechanism. A tier counts only when the theme declares it and the host and
// platform can run it.
bool capabilityAvailable(const CapabilityResolution &resolution,
                         const ThemeCapabilityProfile &theme,
                         const QString &name)
{
    if (rendererTierNames().contains(name))
    {
        const std::optional<RendererTier> tier = rendererTierFromName(name);
        return tier.has_value() && theme.rendererTiers.contains(*tier) &&
            std::any_of(resolution.rendererChoices.cbegin(),
                        resolution.rendererChoices.cend(),
                        [&tier](const RendererCandidateDecision &choice)
                        {
                            return choice.tier == *tier && choice.available;
                        });
    }
    return decisionAvailable(resolution.controls, name) ||
        decisionAvailable(resolution.presentationMechanisms, name);
}

struct PanelAttempt
{
    bool available = false;
    QString reasonCode;
    CapabilityResolution resolution;
    QStringList missingRequired;
    QStringList missingOptional;
};

PanelAttempt attemptPanel(const PanelPresetDefinition &preset,
                          const PanelDefinition &definition,
                          const QVariantList &themeCatalog,
                          const QVector<RendererAvailability> &renderers,
                          const PlatformCapabilityProfile &platform,
                          const std::function<bool(const QString &)> &themeUsable)
{
    PanelAttempt attempt;
    const QString themeId = definition.surface.completeThemeId.trimmed();
    if (!themeId.isEmpty())
    {
        if (!findTheme(themeCatalog, themeId).has_value())
        {
            attempt.reasonCode = QStringLiteral("theme-not-found");
            return attempt;
        }
        if (themeUsable && !themeUsable(themeId))
        {
            attempt.reasonCode = QStringLiteral("theme-package-unavailable");
            return attempt;
        }
    }
    const std::optional<ThemeCapabilityProfile> profile =
        themeProfile(themeCatalog, themeId);
    if (!profile.has_value())
    {
        attempt.reasonCode = capabilityReasonCodeName(
            CapabilityReasonCode::ThemeCapabilityUndeclared);
        return attempt;
    }

    attempt.resolution = PanelCapabilityResolver::resolve(
        definition,
        PanelCapabilityResolver::productionHostProfile(definition.host.kind),
        *profile, renderers, platform);
    if (!attempt.resolution.available)
    {
        attempt.reasonCode = capabilityReasonCodeName(attempt.resolution.reason);
        return attempt;
    }
    for (const QString &capability : preset.compatibility.requiredCapabilities)
    {
        if (!capabilityAvailable(attempt.resolution, *profile, capability))
        {
            attempt.missingRequired.append(capability);
        }
    }
    for (const QString &capability : preset.compatibility.optionalCapabilities)
    {
        if (!capabilityAvailable(attempt.resolution, *profile, capability))
        {
            attempt.missingOptional.append(capability);
        }
    }
    if (!attempt.missingRequired.isEmpty())
    {
        attempt.reasonCode = QStringLiteral("required-capability-unavailable");
        return attempt;
    }
    attempt.available = true;
    return attempt;
}

QString effectiveTierName(const CapabilityResolution &resolution)
{
    return resolution.renderer.effectiveTier.has_value()
        ? rendererTierName(*resolution.renderer.effectiveTier) : QString{};
}

}

namespace ArchDock
{

QVariantMap PresetCompatibility::toVariantMap() const
{
    return {
        {QStringLiteral("available"), available},
        {QStringLiteral("effectiveIconStyleId"), effectiveIconStyleId},
        {QStringLiteral("effectiveMotionProfileId"), effectiveMotionProfileId},
        {QStringLiteral("effectiveRendererTier"), effectiveRendererTier},
        {QStringLiteral("effectiveThemeId"), effectiveThemeId},
        {QStringLiteral("fallbackApplied"), fallbackApplied},
        {QStringLiteral("hostKind"), hostKind},
        {QStringLiteral("missingOptionalCapabilities"), missingOptionalCapabilities},
        {QStringLiteral("missingRequiredCapabilities"), missingRequiredCapabilities},
        {QStringLiteral("reasonCode"), reasonCode},
        {QStringLiteral("requestedRendererTier"), requestedRendererTier},
    };
}

IconStyleResolution PresetCapabilityResolver::resolveIconStyle(
    const IconStyleStore &styles,
    const IconPresetDefinition &preset)
{
    IconStyleResolution result;
    const IconStylePackage *package = styles.packageById(preset.icon.iconStyleId);
    if (!package)
    {
        addDiagnostic(&result.diagnostics, QStringLiteral("invalid-reference"),
                      QStringLiteral("/icon/iconStyleId"),
                      QStringLiteral("icon style is not in the icon-style catalog"));
        return result;
    }

    const bool overridden = !preset.icon.visualOverrides.isEmpty() ||
        !preset.icon.stateOverrides.isEmpty() ||
        preset.icon.glyphPolicy != package->definition().glyphPolicy;
    if (!overridden)
    {
        // Untouched: this is the store's own projection of the style.
        result.projection = styles.resolve(preset.icon.iconStyleId);
        result.valid = result.projection.value(QStringLiteral("valid")).toBool();
        return result;
    }

    // Overrides are merged into the style's own manifest, so everything the
    // preset does not name stays exactly what the package declares.
    QFile manifestFile(package->manifestPath());
    const QJsonDocument document = manifestFile.open(QIODevice::ReadOnly)
        ? QJsonDocument::fromJson(
              manifestFile.read(IconStylePackage::MaximumManifestBytes + 1))
        : QJsonDocument{};
    if (!document.isObject())
    {
        addDiagnostic(&result.diagnostics, QStringLiteral("missing-resource"),
                      QStringLiteral("/icon/iconStyleId"),
                      QStringLiteral("the icon style manifest could not be read"));
        return result;
    }
    QVariantMap manifest = document.object().toVariantMap();

    const QString visualPointer = QStringLiteral("/icon/visualOverrides");
    QVariantMap layers = manifest.value(QStringLiteral("layers")).toMap();
    QSet<QString> unmatched(preset.icon.visualOverrides.keyBegin(),
                            preset.icon.visualOverrides.keyEnd());
    const auto overrideLayer = [&](QVariantMap *layer)
    {
        const QString id = layer->value(QStringLiteral("id")).toString();
        if (preset.icon.visualOverrides.contains(id))
        {
            applyLayerOverride(layer, preset.icon.visualOverrides.value(id).toMap(),
                               pointerChild(visualPointer, id), &result.diagnostics);
            unmatched.remove(id);
        }
    };
    for (const QString &role : {QStringLiteral("rear"), QStringLiteral("base"),
                                QStringLiteral("front")})
    {
        QVariantList list = layers.value(role).toList();
        for (QVariant &entry : list)
        {
            QVariantMap layer = entry.toMap();
            overrideLayer(&layer);
            entry = layer;
        }
        layers.insert(role, list);
    }
    for (const QString &role : {QStringLiteral("mask"), QStringLiteral("reflection"),
                                QStringLiteral("shadow"), QStringLiteral("glow")})
    {
        if (layers.contains(role))
        {
            QVariantMap layer = layers.value(role).toMap();
            overrideLayer(&layer);
            layers.insert(role, layer);
        }
    }
    manifest.insert(QStringLiteral("layers"), layers);
    for (const QString &id : std::as_const(unmatched))
    {
        addDiagnostic(&result.diagnostics, QStringLiteral("invalid-reference"),
                      pointerChild(visualPointer, id),
                      QStringLiteral("the icon style has no layer with this id"));
    }

    QVariantList states = manifest.value(QStringLiteral("states")).toList();
    QSet<QString> unmatchedStates(preset.icon.stateOverrides.keyBegin(),
                                  preset.icon.stateOverrides.keyEnd());
    for (QVariant &entry : states)
    {
        QVariantMap state = entry.toMap();
        const QString id = state.value(QStringLiteral("id")).toString();
        if (!preset.icon.stateOverrides.contains(id))
        {
            continue;
        }
        const QVariantMap fields = preset.icon.stateOverrides.value(id).toMap();
        for (auto field = fields.cbegin(); field != fields.cend(); ++field)
        {
            state.insert(field.key(), field.value());
        }
        entry = state;
        unmatchedStates.remove(id);
    }
    manifest.insert(QStringLiteral("states"), states);
    for (const QString &id : std::as_const(unmatchedStates))
    {
        addDiagnostic(&result.diagnostics, QStringLiteral("invalid-reference"),
                      pointerChild(QStringLiteral("/icon/stateOverrides"), id),
                      QStringLiteral("the icon style has no state with this id"));
    }
    manifest.insert(QStringLiteral("glyphPolicy"),
                    preset.icon.glyphPolicy.toVariantMap());

    if (presetDiagnosticsHaveErrors(result.diagnostics))
    {
        return result;
    }

    // The same parser that admits an installed style decides whether the
    // merged one is valid: ranges, colors, shapes, states and glyph policy.
    const IconStylePackageLoadResult merged = IconStylePackage::loadBytes(
        QJsonDocument(QJsonObject::fromVariantMap(manifest))
            .toJson(QJsonDocument::Compact),
        package->sourceRoot(), package->manifestPath());
    if (!merged.isValid())
    {
        for (const IconStyleValidationDiagnostic &diagnostic : merged.diagnostics)
        {
            addDiagnostic(&result.diagnostics, diagnostic.code,
                          QStringLiteral("/icon/resolvedStyle") +
                              diagnostic.jsonPointer,
                          diagnostic.message, diagnostic.severity);
        }
        if (!presetDiagnosticsHaveErrors(result.diagnostics))
        {
            addDiagnostic(&result.diagnostics, QStringLiteral("invalid-value"),
                          QStringLiteral("/icon/resolvedStyle"),
                          QStringLiteral("the overridden icon style is not valid"));
        }
        return result;
    }

    result.projection = merged.package->runtimeProjection();
    // The selection fields IconStyleStore::resolve() adds for a chosen style.
    result.projection.insert(QStringLiteral("fellBack"), false);
    result.projection.insert(QStringLiteral("requestedStyleId"),
                             preset.icon.iconStyleId);
    result.projection.insert(QStringLiteral("resolvedStyleId"),
                             preset.icon.iconStyleId);
    result.projection.insert(QStringLiteral("selectionStatus"),
                             QStringLiteral("selected"));
    result.projection.insert(QStringLiteral("fallbackReason"), QString{});
    result.overridesApplied = true;
    result.valid = true;
    return result;
}

PresetCompatibility PresetCapabilityResolver::resolvePanelPreset(
    const PanelPresetDefinition &preset,
    const QVariantList &themeCatalog,
    const QVector<RendererAvailability> &renderers,
    const PlatformCapabilityProfile &platform,
    const std::function<bool(const QString &themeId)> &themeUsable)
{
    const PanelDefinition &declared = preset.panel.configuration;
    PresetCompatibility result;
    result.hostKind = PanelDefinition::hostKindName(declared.host.kind);
    result.requestedRendererTier = preset.preview.rendererTier;

    const PanelAttempt primary = attemptPanel(
        preset, declared, themeCatalog, renderers, platform, themeUsable);
    if (primary.available)
    {
        // A renderer-tier fallback inside the same theme is reported by the
        // capability resolver itself.
        result.available = true;
        result.fallbackApplied = primary.resolution.renderer.fallbackApplied;
        result.reasonCode = result.fallbackApplied
            ? capabilityReasonCodeName(primary.resolution.renderer.reason)
            : QString{};
        result.effectiveRendererTier = effectiveTierName(primary.resolution);
        result.effectiveThemeId = preset.panel.themeId();
        result.missingOptionalCapabilities = primary.missingOptional;
        result.capabilityResolution = primary.resolution.toVariantMap();
        result.effectiveConfiguration = declared;
        return result;
    }

    // The declared safe fallback: the same panel on the fallback theme, or on
    // the built-in procedural surface when none is named, at that theme's own
    // preferred tier.
    PanelDefinition fallbackDefinition = declared;
    fallbackDefinition.surface.completeThemeId = preset.fallback.themeId;
    fallbackDefinition.surface.panelThemeId = preset.fallback.themeId;
    fallbackDefinition.surface.rendererTier.clear();
    const PanelAttempt fallback = attemptPanel(
        preset, fallbackDefinition, themeCatalog, renderers, platform, themeUsable);

    result.reasonCode = primary.reasonCode;
    result.missingRequiredCapabilities = primary.missingRequired;
    if (!fallback.available)
    {
        result.missingOptionalCapabilities = primary.missingOptional;
        result.capabilityResolution = primary.resolution.toVariantMap();
        return result;
    }
    result.available = true;
    result.fallbackApplied = true;
    result.effectiveRendererTier = effectiveTierName(fallback.resolution);
    result.effectiveThemeId = preset.fallback.themeId;
    result.missingRequiredCapabilities.clear();
    result.missingOptionalCapabilities = fallback.missingOptional;
    result.capabilityResolution = fallback.resolution.toVariantMap();
    result.effectiveConfiguration = fallbackDefinition;
    return result;
}

PresetCompatibility PresetCapabilityResolver::resolveIconPreset(
    const IconPresetDefinition &preset,
    const IconStyleStore &styles,
    const AnimationProfileCatalog &profiles)
{
    PresetCompatibility result;
    result.requestedRendererTier = preset.compatibility.rendererTiers.value(0);
    result.effectiveRendererTier = result.requestedRendererTier;

    const bool styleUsable = resolveIconStyle(styles, preset).valid;
    const bool motionUsable = profiles.contains(preset.icon.motion.profileId);
    result.effectiveIconStyleId = styleUsable
        ? preset.icon.iconStyleId : preset.fallback.iconStyleId;
    result.effectiveMotionProfileId = motionUsable
        ? preset.icon.motion.profileId : preset.fallback.motionProfileId;
    if (styleUsable && motionUsable)
    {
        result.available = true;
        return result;
    }

    result.reasonCode = styleUsable ? QStringLiteral("motion-profile-unavailable")
                                    : QStringLiteral("icon-style-unavailable");
    result.fallbackApplied = true;
    result.available = styles.contains(result.effectiveIconStyleId) &&
        profiles.contains(result.effectiveMotionProfileId);
    if (!result.available)
    {
        result.fallbackApplied = false;
        result.effectiveIconStyleId.clear();
        result.effectiveMotionProfileId.clear();
    }
    return result;
}

QVector<RendererAvailability> PresetCapabilityResolver::idealRenderers()
{
    QVector<RendererAvailability> renderers =
        PanelCapabilityResolver::productionRenderers();
    for (RendererAvailability &renderer : renderers)
    {
        renderer.installed = true;
        renderer.enabled = true;
        renderer.sceneImplemented = true;
    }
    return renderers;
}

QVector<PresetValidationDiagnostic> PresetCapabilityResolver::validatePanelPreset(
    const PanelPresetDefinition &preset,
    const QVariantList &themeCatalog)
{
    QVector<PresetValidationDiagnostic> result;
    const QString themeId = preset.panel.themeId();
    const std::optional<ThemeCapabilityProfile> profile =
        themeProfile(themeCatalog, themeId);
    if (!profile.has_value())
    {
        // A missing theme is reported by the reference check.
        return result;
    }

    const PlatformCapabilityProfile platform =
        PanelCapabilityResolver::productionPlatform();
    const PresetCompatibility ideal = resolvePanelPreset(
        preset, themeCatalog, idealRenderers(), platform);
    if (!ideal.available || ideal.fallbackApplied)
    {
        addDiagnostic(&result, QStringLiteral("inconsistent-declaration"),
                      QStringLiteral("/panel"),
                      QStringLiteral("the preset cannot be drawn as declared: ") +
                          ideal.reasonCode);
        return result;
    }
    if (ideal.effectiveRendererTier != preset.preview.rendererTier)
    {
        addDiagnostic(&result, QStringLiteral("inconsistent-declaration"),
                      QStringLiteral("/preview/rendererTier"),
                      QStringLiteral("the preset resolves to renderer tier ") +
                          ideal.effectiveRendererTier);
    }
    if (!ideal.missingOptionalCapabilities.isEmpty())
    {
        addDiagnostic(&result, QStringLiteral("inconsistent-declaration"),
                      QStringLiteral("/compatibility/optionalCapabilities"),
                      QStringLiteral("capability can never be available here: ") +
                          ideal.missingOptionalCapabilities.join(QStringLiteral(", ")));
    }

    // The declared fallback tier is the tier the theme itself falls back to;
    // a theme with no fallback tier has none to declare.
    const std::optional<RendererTier> fallbackTier =
        rendererTierFromName(preset.preview.fallbackTier);
    const bool fallbackTierDeclared = profile->fallbackRendererTiers.isEmpty()
        ? preset.preview.fallbackTier == preset.preview.rendererTier
        : fallbackTier.has_value() &&
              profile->fallbackRendererTiers.contains(*fallbackTier);
    if (!fallbackTierDeclared)
    {
        addDiagnostic(&result, QStringLiteral("inconsistent-declaration"),
                      QStringLiteral("/preview/fallbackTier"),
                      QStringLiteral("the theme does not fall back to this renderer tier"));
    }

    // A separate fallback theme must really be able to draw this preset when
    // the primary theme's resources cannot be loaded.
    if (preset.fallback.themeId != themeId)
    {
        const PresetCompatibility degraded = resolvePanelPreset(
            preset, themeCatalog, idealRenderers(), platform,
            [&themeId](const QString &candidate)
            {
                return candidate != themeId;
            });
        if (!degraded.available)
        {
            addDiagnostic(&result, QStringLiteral("inconsistent-declaration"),
                          QStringLiteral("/fallback/themeId"),
                          QStringLiteral("the declared fallback cannot draw this preset: ") +
                              degraded.reasonCode);
        }
    }
    return result;
}

}
