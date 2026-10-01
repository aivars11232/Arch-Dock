#pragma once

#include "../animation/AnimationProfileCatalog.h"
#include "../iconstyles/IconStyleStore.h"
#include "../model/IconPresetDefinition.h"
#include "../model/PanelCapabilityResolver.h"
#include "../model/PanelPresetDefinition.h"

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <functional>
#include <optional>

namespace ArchDock
{

// An icon style as an Icon Preset draws it: the referenced style with the
// preset's layer, state and glyph-policy overrides applied.
struct IconStyleResolution
{
    bool valid = false;
    bool overridesApplied = false;
    // The runtime projection the shared IconScene consumes.
    QVariantMap projection;
    QVector<PresetValidationDiagnostic> diagnostics;
};

// Whether a preset can be used here and now, and through what.
struct PresetCompatibility
{
    // Usable, either as declared or through the declared safe fallback.
    bool available = false;
    bool fallbackApplied = false;
    // Why a fallback was applied, or why the preset is unavailable.
    QString reasonCode;
    QString hostKind;
    QString requestedRendererTier;
    QString effectiveRendererTier;
    QString effectiveThemeId;
    QString effectiveIconStyleId;
    QString effectiveMotionProfileId;
    QStringList missingRequiredCapabilities;
    QStringList missingOptionalCapabilities;
    QVariantMap capabilityResolution;
    // The panel configuration that is actually drawn: the preset's own, or the
    // preset with its declared fallback theme.
    std::optional<PanelDefinition> effectiveConfiguration;

    [[nodiscard]] QVariantMap toVariantMap() const;
};

// Resolves presets against the same capability model every panel uses. It
// decides nothing by itself about renderers, hosts or themes: those answers
// come from PanelCapabilityResolver and the icon-style validator.
class PresetCapabilityResolver
{
public:
    // The style an icon preset names, with its overrides merged in. The merged
    // manifest is validated by the icon-style package parser exactly as an
    // installed style is, so an override can never produce a style the
    // renderer has not been proven to draw.
    [[nodiscard]] static IconStyleResolution resolveIconStyle(
        const IconStyleStore &styles,
        const IconPresetDefinition &preset);

    // `themeUsable` reports whether a theme's resources can be loaded now; an
    // absent callback means every catalog theme is usable.
    [[nodiscard]] static PresetCompatibility resolvePanelPreset(
        const PanelPresetDefinition &preset,
        const QVariantList &themeCatalog,
        const QVector<RendererAvailability> &renderers,
        const PlatformCapabilityProfile &platform,
        const std::function<bool(const QString &themeId)> &themeUsable = {});

    [[nodiscard]] static PresetCompatibility resolveIconPreset(
        const IconPresetDefinition &preset,
        const IconStyleStore &styles,
        const AnimationProfileCatalog &profiles);

    // The production renderers with every tier present: the environment in
    // which a preset must work exactly as it is declared.
    [[nodiscard]] static QVector<RendererAvailability> idealRenderers();

    // Structural checks a catalog runs once: the preset resolves as declared
    // when every renderer exists, and its declared fallback really can draw
    // it when the primary theme cannot be loaded.
    [[nodiscard]] static QVector<PresetValidationDiagnostic> validatePanelPreset(
        const PanelPresetDefinition &preset,
        const QVariantList &themeCatalog);
};

}
