// Preset preparation shared by auditions, application and defaults.
#include "PresetApplication.h"
#include "UserPresetStore.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
using namespace ArchDock;

void setError(QString *error, const QString &value)
{
    if (error) *error = value;
}

std::optional<PanelSettingsTransactionDraft> prepare(
    const PanelDefinition &snapshot, const QVariantMap &globals,
    const QVariantMap &values, QString *error)
{
    PanelSettingsTransactionOutcome outcome;
    auto result = PanelSettingsTransaction::prepare(snapshot, globals, globals,
        {snapshot.identity.id, snapshot.settingsRevision, values, {}}, &outcome);
    setError(error, result ? QString{} : outcome.errorCode);
    return result;
}

void applyIconMetadata(PanelDefinition *candidate, const IconPresetDefinition &preset)
{
    if (candidate->iconStyle.styleReference == preset.icon.iconStyleId)
        candidate->iconStyle.globalDefaults.insert(
            QStringLiteral("presetOverrides"), preset.icon.toVariantMap());
    else
        candidate->iconStyle.globalDefaults.remove(QStringLiteral("presetOverrides"));
    auto origin = candidate->presetOrigin.value_or(PanelPresetOrigin{});
    origin.iconPresetId = preset.identity.id;
    origin.iconPresetRevision = preset.identity.revision;
    candidate->presetOrigin = origin;
}
}

namespace ArchDock
{

std::optional<PanelSettingsTransactionDraft> PresetApplication::preparePanel(
    const PanelDefinition &snapshot, const QVariantMap &globals,
    const PanelPresetDefinition &preset, const QVariantMap &customizations,
    const std::optional<IconPresetDefinition> &recommendedIcons, QString *errorCode)
{
    QVariantMap values = preset.panelValues();
    if (!recommendedIcons && snapshot.presetOrigin &&
        !snapshot.presetOrigin->iconPresetId.isEmpty())
    {
        for (const QString &key : IconPresetDefinition::panelValueKeys())
            values.remove(key);
    }
    if (recommendedIcons) values.insert(recommendedIcons->panelValues());
    values.insert(customizations);
    auto result = prepare(snapshot, globals, values, errorCode);
    if (!result) return std::nullopt;
    auto &candidate = result->candidatePanel;
    // Theme ids travel together, as in the existing built-in-theme draft.
    candidate.surface.panelThemeId = candidate.surface.completeThemeId;
    auto origin = snapshot.presetOrigin.value_or(PanelPresetOrigin{});
    origin.panelPresetId = preset.identity.id;
    origin.panelPresetRevision = preset.identity.revision;
    origin.customizedAfterApply = !customizations.isEmpty();
    origin.detachedFromPreset = false;
    candidate.presetOrigin = origin;
    if (recommendedIcons) applyIconMetadata(&candidate, *recommendedIcons);
    const QString storedStyle = candidate.iconStyle.globalDefaults.value(
        QStringLiteral("presetOverrides")).toMap().value(QStringLiteral("iconStyleId")).toString();
    if (!storedStyle.isEmpty() && storedStyle != candidate.iconStyle.styleReference)
        candidate.iconStyle.globalDefaults.remove(QStringLiteral("presetOverrides"));
    candidate = candidate.normalized();
    return result;
}

bool PresetApplication::isSceneEditKey(const QString &key)
{
    return key.startsWith(QStringLiteral("scene3D"));
}

std::optional<PanelSettingsTransactionDraft> PresetApplication::prepareSceneEdit(
    const PanelDefinition &snapshot, const QVariantMap &globals,
    const QVariantMap &customizations, QString *errorCode)
{
    for (auto it = customizations.cbegin(); it != customizations.cend(); ++it)
    {
        if (!isSceneEditKey(it.key()))
        {
            setError(errorCode, QStringLiteral("unavailable-scene-edit-field"));
            return std::nullopt;
        }
    }
    return prepare(snapshot, globals, customizations, errorCode);
}

std::optional<PanelSettingsTransactionDraft> PresetApplication::prepareIcon(
    const PanelDefinition &snapshot, const QVariantMap &globals,
    const IconPresetDefinition &preset, const QVariantMap &customizations,
    QString *errorCode)
{
    for (auto it = customizations.cbegin(); it != customizations.cend(); ++it)
    {
        if (!IconPresetDefinition::panelValueKeys().contains(it.key()))
        {
            setError(errorCode, QStringLiteral("icon-only-violation"));
            return std::nullopt;
        }
    }
    QVariantMap values = preset.panelValues();
    values.insert(customizations);
    auto result = prepare(snapshot, globals, values, errorCode);
    if (!result) return std::nullopt;
    applyIconMetadata(&result->candidatePanel, preset);
    result->candidatePanel.presetOrigin->customizedAfterApply =
        !customizations.isEmpty();
    if (!iconOnlyChange(snapshot, result->candidatePanel))
    {
        setError(errorCode, QStringLiteral("icon-only-violation"));
        return std::nullopt;
    }
    return result;
}

bool PresetApplication::iconOnlyChange(
    const PanelDefinition &snapshot, const PanelDefinition &candidate)
{
    const auto originalOrigin = snapshot.presetOrigin.value_or(PanelPresetOrigin{});
    const auto nextOrigin = candidate.presetOrigin.value_or(PanelPresetOrigin{});
    if (originalOrigin.panelPresetId != nextOrigin.panelPresetId ||
        originalOrigin.panelPresetRevision != nextOrigin.panelPresetRevision ||
        originalOrigin.detachedFromPreset != nextOrigin.detachedFromPreset)
        return false;
    // PanelDefinition normalizes this legacy alias to the selected style.
    if (candidate.iconStyle.themeId != candidate.iconStyle.styleReference)
        return false;
    PanelDefinition compared = candidate;
    compared.settingsRevision = snapshot.settingsRevision;
    compared.presetOrigin = snapshot.presetOrigin;
    compared.iconStyle.styleReference = snapshot.iconStyle.styleReference;
    compared.iconStyle.themeId = snapshot.iconStyle.themeId;
    compared.iconStyle.globalDefaults = snapshot.iconStyle.globalDefaults;
    // PD-20/21/22: appearance and tile values belong to an icon preset.
    // Keep size, spacing and per-entry overrides in the comparison: those
    // must not be changed by an icon preset or by this scope allowance.
    compared.iconStyle.shape = snapshot.iconStyle.shape;
    compared.iconStyle.diameter = snapshot.iconStyle.diameter;
    compared.iconStyle.logoSize = snapshot.iconStyle.logoSize;
    compared.iconStyle.outlineWidth = snapshot.iconStyle.outlineWidth;
    compared.iconStyle.bodyColor = snapshot.iconStyle.bodyColor;
    compared.iconStyle.outlineColor = snapshot.iconStyle.outlineColor;
    compared.iconStyle.glowColor = snapshot.iconStyle.glowColor;
    compared.iconStyle.pedestalEnabled = snapshot.iconStyle.pedestalEnabled;
    compared.iconStyle.pedestalHeight = snapshot.iconStyle.pedestalHeight;
    compared.iconStyle.pedestalColor = snapshot.iconStyle.pedestalColor;
    compared.iconStyle.tilesEnabled = snapshot.iconStyle.tilesEnabled;
    compared.iconStyle.tileMode = snapshot.iconStyle.tileMode;
    compared.iconStyle.tileColor = snapshot.iconStyle.tileColor;
    compared.iconStyle.tileOpacity = snapshot.iconStyle.tileOpacity;
    compared.iconStyle.tileBorderColor = snapshot.iconStyle.tileBorderColor;
    compared.iconStyle.tileBorderWidth = snapshot.iconStyle.tileBorderWidth;
    compared.iconStyle.tileTexture = snapshot.iconStyle.tileTexture;
    compared.iconStyle.tileThickness = snapshot.iconStyle.tileThickness;
    compared.iconStyle.tileIconOffsetX = snapshot.iconStyle.tileIconOffsetX;
    compared.iconStyle.tileIconOffsetY = snapshot.iconStyle.tileIconOffsetY;
    compared.iconStyle.tileIconScale = snapshot.iconStyle.tileIconScale;
    compared.iconStyle.tileBevel = snapshot.iconStyle.tileBevel;
    compared.iconStyle.tileMaterial = snapshot.iconStyle.tileMaterial;
    compared.iconStyle.tileElevation = snapshot.iconStyle.tileElevation;
    compared.motion.iconProfile = snapshot.motion.iconProfile;
    compared.motion.trigger = snapshot.motion.trigger;
    compared.motion.speed = snapshot.motion.speed;
    compared.motion.intensity = snapshot.motion.intensity;
    compared.motion.magnifyRadius = snapshot.motion.magnifyRadius;
    compared.motion.magnifyFalloff = snapshot.motion.magnifyFalloff;
    return compared == snapshot;
}

PanelPresetDefinition PresetApplication::panelSnapshot(
    const PanelPresetDefinition &source, const PanelDefinition &draft,
    const QString &name)
{
    auto result = UserPresetStore::derivedFrom(source, name);
    result.panel.configuration = draft.normalized();
    const bool free = draft.host.kind == PanelHostKind::FreeDesktop;
    result.preview.previewMode = free ? QStringLiteral("free")
        : draft.placement.edge == QStringLiteral("left") ||
          draft.placement.edge == QStringLiteral("right")
            ? QStringLiteral("vertical") : QStringLiteral("horizontal");
    result.preview.rendererTier = draft.surface.rendererTier.isEmpty()
        ? source.preview.rendererTier : draft.surface.rendererTier;
    result.compatibility.hostKinds = {PanelDefinition::hostKindName(draft.host.kind)};
    result.compatibility.layouts = {draft.layout.pathType};
    result.compatibility.orientations = {result.preview.previewMode};
    result.preview.deterministicPreviewSeed = QString::fromLatin1(
        QCryptographicHash::hash(QJsonDocument(QJsonObject::fromVariantMap(
            result.panelValues())).toJson(QJsonDocument::Compact),
            QCryptographicHash::Sha256).toHex().left(48));
    return result;
}

IconPresetDefinition PresetApplication::iconSnapshot(
    const IconPresetDefinition &source, const PanelDefinition &draft,
    const QString &name)
{
    auto result = UserPresetStore::derivedFrom(source, name);
    if (draft.iconStyle.styleReference != source.icon.iconStyleId)
    {
        result.icon = IconPresetIcon{};
        result.compatibility.requiredStyleCapabilities.clear();
    }
    // Keep the resolved visual/state/glyph snapshot even if its source goes away.
    const auto block = draft.iconStyle.globalDefaults.value(
        QStringLiteral("presetOverrides")).toMap();
    if (!block.isEmpty())
    {
        auto object = source.toVariantMap();
        object.insert(QStringLiteral("icon"), block);
        if (const auto stored = IconPresetDefinition::fromVariantMap(object))
            result.icon = stored->icon;
    }
    result.icon.iconStyleId = draft.iconStyle.styleReference;
    const auto iconValues = draft.normalized().toLegacyMap();
    result.icon.parameters.clear();
    for (const auto &key : IconPresetDefinition::parameterValueKeys())
        result.icon.parameters.insert(key, iconValues.value(key));
    result.icon.motion.profileId = draft.motion.iconProfile;
    result.icon.motion.trigger = draft.motion.trigger;
    result.icon.motion.speed = draft.motion.speed;
    result.icon.motion.intensity = draft.motion.intensity;
    result.icon.motion.magnificationRadius = draft.motion.magnifyRadius;
    result.icon.motion.magnificationFalloff = draft.motion.magnifyFalloff;
    return result;
}

void PresetApplication::markCustomized(
    const PanelDefinition &previous, PanelDefinition *candidate)
{
    if (!candidate || !previous.presetOrigin ||
        candidate->presetOrigin != previous.presetOrigin) return;
    const auto before = previous.toLegacyMap();
    const auto after = candidate->toLegacyMap();
    QStringList keys = PanelPresetDefinition::panelValueKeys();
    keys.append(IconPresetDefinition::panelValueKeys());
    keys.append(QStringLiteral("iconGlobalDefaults"));
    keys.append(QStringLiteral("iconOverrides"));
    keys.append({QStringLiteral("iconTilesEnabled"), QStringLiteral("iconTileMode"),
        QStringLiteral("iconTileColor"), QStringLiteral("iconTileOpacity"),
        QStringLiteral("iconTileBorderColor"), QStringLiteral("iconTileBorderWidth")});
    for (const QString &key : keys)
    {
        if (before.value(key) != after.value(key))
        {
            candidate->presetOrigin->customizedAfterApply = true;
            return;
        }
    }
}

}
