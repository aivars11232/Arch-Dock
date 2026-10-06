// The preset library Panel Studio browses.
#include "PresetLibrary.h"

#include "PresetCapabilityResolver.h"
#include "../PanelRegistry.h"
#include "../model/PanelSettingsSchema.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace
{

using namespace ArchDock;

const QString builtInScope = QStringLiteral("builtin");
const QString userScope = QStringLiteral("user");
const QString panelKind = QStringLiteral("panel");
const QString iconKind = QStringLiteral("icon");

// The display name a catalog lists for `id`, or nothing when it has no entry.
QString entryName(const QVariantList &entries, const QString &id)
{
    for (const QVariant &candidate : entries)
    {
        const QVariantMap entry = candidate.toMap();
        if (entry.value(QStringLiteral("id")).toString() == id)
        {
            return entry.value(QStringLiteral("name")).toString();
        }
    }
    return {};
}

QVariantMap identityCard(const PresetIdentity &identity, const QString &kind)
{
    return {
        {QStringLiteral("kind"), kind},
        {QStringLiteral("id"), identity.id},
        {QStringLiteral("name"), identity.name},
        {QStringLiteral("description"), identity.description},
        {QStringLiteral("builtIn"), identity.builtIn},
        {QStringLiteral("revision"), identity.revision},
        {QStringLiteral("derivedFromPresetId"), identity.derivedFromPresetId},
        {QStringLiteral("sourceRevision"), identity.sourceRevision},
    };
}

// The style an icon preset is drawn with: its own when that resolves,
// otherwise the safe fallback style it declares.
QVariantMap iconStyleProjection(const IconStyleStore &styles,
                                const IconPresetDefinition &preset)
{
    const IconStyleResolution own =
        PresetCapabilityResolver::resolveIconStyle(styles, preset);
    return own.valid ? own.projection : styles.resolve(preset.fallback.iconStyleId);
}

// Saves a user-owned copy of `source` and returns its new id.
template<typename Definition>
QString saveCopy(const UserPresetStore &store,
                 const std::optional<Definition> &source,
                 const QString &name,
                 QString *errorCode)
{
    if (!source.has_value())
    {
        *errorCode = QStringLiteral("preset-not-found");
        return {};
    }
    const std::optional<Definition> saved =
        store.save(UserPresetStore::derivedFrom(*source, name), errorCode);
    return saved.has_value() ? saved->identity.id : QString{};
}

template<typename Definition>
QString saveRenamed(const UserPresetStore &store,
                    std::optional<Definition> preset,
                    const QString &name,
                    QString *errorCode)
{
    if (!preset.has_value())
    {
        *errorCode = QStringLiteral("preset-not-found");
        return {};
    }
    if (preset->identity.builtIn)
    {
        *errorCode = QStringLiteral("not-user-preset");
        return {};
    }
    preset->identity.name = name.trimmed();
    const std::optional<Definition> saved = store.save(*preset, errorCode);
    return saved.has_value() ? saved->identity.id : QString{};
}

}

namespace ArchDock
{

PresetLibrary::PresetLibrary(const PanelRegistry &registry, QObject *parent)
    : PresetLibrary(registry, locateBuiltInRoot(), defaultUserRoot(), parent)
{
}

PresetLibrary::PresetLibrary(const PanelRegistry &registry,
                             QString builtInRoot,
                             QString userRoot,
                             QObject *parent)
    : QObject(parent),
      m_registry(registry),
      m_builtInRoot(std::move(builtInRoot)),
      m_userStore(std::move(userRoot))
{
}

QString PresetLibrary::locateBuiltInRoot()
{
    const QString installedIndex = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("arch-dock/presets/panels/") +
            PanelPresetCatalog::indexFileName(),
        QStandardPaths::LocateFile);
    if (!installedIndex.isEmpty())
    {
        return QFileInfo(QFileInfo(installedIndex).absolutePath()).absolutePath();
    }
#if defined(ARCHDOCK_SOURCE_PRESET_ROOT)
    const QString sourceRoot = QString::fromUtf8(ARCHDOCK_SOURCE_PRESET_ROOT);
    if (QFileInfo(sourceRoot).isDir())
    {
        return sourceRoot;
    }
#endif
    return {};
}

QString PresetLibrary::defaultUserRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        QStringLiteral("/presets");
}

int PresetLibrary::revision() const
{
    return m_revision;
}

QString PresetLibrary::builtInRoot() const
{
    return m_builtInRoot;
}

QString PresetLibrary::userRoot() const
{
    return m_userStore.rootDirectory();
}

QVariantMap PresetLibrary::catalogStatus() const
{
    const bool valid = ensureLoaded();
    return {
        {QStringLiteral("valid"), valid},
        {QStringLiteral("errorCode"), m_errorCode},
        {QStringLiteral("diagnostics"),
         presetDiagnosticsToVariantList(m_diagnostics)},
        {QStringLiteral("panelPresetCount"),
         valid ? int(m_panelCatalog->presetIds().size()) : 0},
        {QStringLiteral("iconPresetCount"),
         valid ? int(m_iconCatalog->presetIds().size()) : 0},
        {QStringLiteral("builtInRoot"), m_builtInRoot},
        {QStringLiteral("userRoot"), m_userStore.rootDirectory()},
    };
}

QVariantList PresetLibrary::panelPresets(const QString &scope) const
{
    if (!ensureLoaded())
    {
        return {};
    }
    if (scope == builtInScope)
    {
        if (!m_builtInPanelCards.has_value())
        {
            QVariantList cards;
            for (const QString &id : m_panelCatalog->presetIds())
            {
                cards.append(panelCard(*m_panelCatalog->presetById(id)));
            }
            m_builtInPanelCards = cards;
        }
        return *m_builtInPanelCards;
    }
    QVariantList cards;
    if (scope == userScope)
    {
        for (const PanelPresetDefinition &preset : m_userStore.panelPresets())
        {
            cards.append(panelCard(preset));
        }
    }
    return cards;
}

QVariantList PresetLibrary::iconPresets(const QString &scope) const
{
    if (!ensureLoaded())
    {
        return {};
    }
    if (scope == builtInScope)
    {
        if (!m_builtInIconCards.has_value())
        {
            QVariantList cards;
            for (const QString &id : m_iconCatalog->presetIds())
            {
                cards.append(iconCard(*m_iconCatalog->presetById(id)));
            }
            m_builtInIconCards = cards;
        }
        return *m_builtInIconCards;
    }
    QVariantList cards;
    if (scope == userScope)
    {
        for (const IconPresetDefinition &preset : m_userStore.iconPresets())
        {
            cards.append(iconCard(preset));
        }
    }
    return cards;
}

QVariantMap PresetLibrary::duplicatePreset(const QString &kind,
                                           const QString &presetId,
                                           const QString &name)
{
    QString errorCode = QStringLiteral("invalid-kind");
    QString savedId;
    if (kind == panelKind)
    {
        savedId = saveCopy(m_userStore, panelPreset(presetId), name, &errorCode);
    }
    else if (kind == iconKind)
    {
        savedId = saveCopy(m_userStore, iconPreset(presetId), name, &errorCode);
    }
    return finished(errorCode, savedId);
}

QVariantMap PresetLibrary::renamePreset(const QString &kind,
                                        const QString &presetId,
                                        const QString &name)
{
    QString errorCode = QStringLiteral("invalid-kind");
    QString savedId;
    if (kind == panelKind)
    {
        savedId = saveRenamed(m_userStore, panelPreset(presetId), name, &errorCode);
    }
    else if (kind == iconKind)
    {
        savedId = saveRenamed(m_userStore, iconPreset(presetId), name, &errorCode);
    }
    return finished(errorCode, savedId);
}

QVariantMap PresetLibrary::removePreset(const QString &kind,
                                        const QString &presetId)
{
    QString errorCode = QStringLiteral("invalid-kind");
    bool removed = false;
    if (kind == panelKind)
    {
        removed = m_userStore.removePanelPreset(presetId, &errorCode);
    }
    else if (kind == iconKind)
    {
        removed = m_userStore.removeIconPreset(presetId, &errorCode);
    }
    return finished(errorCode, removed ? presetId : QString{});
}

bool PresetLibrary::ensureLoaded() const
{
    if (m_loadAttempted)
    {
        return m_panelCatalog.has_value();
    }
    m_loadAttempted = true;

    PresetReferenceContext context;
    context.themeCatalog = m_registry.themeDefinitions();
    context.iconStyles = m_registry.iconStyleStore();
    context.animationProfiles = m_registry.animationProfileCatalog();
    if (m_builtInRoot.isEmpty())
    {
        m_errorCode = QStringLiteral("preset-catalog-unavailable");
        return false;
    }
    if (!context.iconStyles || !context.animationProfiles)
    {
        m_errorCode = QStringLiteral("preset-resources-unavailable");
        return false;
    }

    const QDir root(m_builtInRoot);
    IconPresetCatalogLoadResult icons = IconPresetCatalog::loadBuiltIn(
        root.filePath(QStringLiteral("icons/") + IconPresetCatalog::indexFileName()),
        context);
    m_diagnostics = icons.diagnostics;
    if (!icons.isValid())
    {
        m_errorCode = icons.primaryCode();
        return false;
    }
    context.iconPresetById = [&icons](const QString &id)
    {
        return icons.catalog->presetById(id);
    };
    PanelPresetCatalogLoadResult panels = PanelPresetCatalog::loadBuiltIn(
        root.filePath(QStringLiteral("panels/") + PanelPresetCatalog::indexFileName()),
        context);
    m_diagnostics += panels.diagnostics;
    if (!panels.isValid())
    {
        m_errorCode = panels.primaryCode();
        return false;
    }
    m_iconCatalog = std::move(icons.catalog);
    m_panelCatalog = std::move(panels.catalog);
    return true;
}

std::optional<IconPresetDefinition> PresetLibrary::iconPreset(
    const QString &presetId) const
{
    if (presetId.isEmpty() || !ensureLoaded())
    {
        return std::nullopt;
    }
    if (const IconPresetDefinition *builtIn = m_iconCatalog->presetById(presetId))
    {
        return *builtIn;
    }
    if (PresetIdentity::isUserId(presetId))
    {
        for (const IconPresetDefinition &preset : m_userStore.iconPresets())
        {
            if (preset.identity.id == presetId)
            {
                return preset;
            }
        }
    }
    return std::nullopt;
}

std::optional<PanelPresetDefinition> PresetLibrary::panelPreset(
    const QString &presetId) const
{
    if (presetId.isEmpty() || !ensureLoaded())
    {
        return std::nullopt;
    }
    if (const PanelPresetDefinition *builtIn = m_panelCatalog->presetById(presetId))
    {
        return *builtIn;
    }
    if (PresetIdentity::isUserId(presetId))
    {
        for (const PanelPresetDefinition &preset : m_userStore.panelPresets())
        {
            if (preset.identity.id == presetId)
            {
                return preset;
            }
        }
    }
    return std::nullopt;
}

bool PresetLibrary::themeUsable(const QString &themeId) const
{
    // A catalog theme without a package has nothing to load: the registry
    // then reports no projection and no error.
    QString errorCode;
    return m_registry.builtInThemeRuntimeProjection(themeId, &errorCode)
               .has_value() ||
        errorCode.isEmpty();
}

QVariantMap PresetLibrary::resolvedTheme(const QString &themeId) const
{
    if (themeId.isEmpty())
    {
        return {};
    }
    const QVariantList themes = m_registry.themeDefinitions();
    for (const QVariant &candidate : themes)
    {
        QVariantMap theme = candidate.toMap();
        if (theme.value(QStringLiteral("id")).toString() != themeId)
        {
            continue;
        }
        // The catalog entry with its package's runtime projection merged in,
        // which is the theme record the Studio theme cards draw from.
        const std::optional<QVariantMap> projection =
            m_registry.builtInThemeRuntimeProjection(themeId);
        if (projection.has_value())
        {
            theme.insert(*projection);
        }
        return theme;
    }
    return {};
}

QVariantMap PresetLibrary::previewCandidate(
    const PanelDefinition &definition,
    const QVariantMap &iconStyleProjection) const
{
    // The record a live panel's renderer receives, with the schema's global
    // defaults standing in for this desktop's global settings so a preset
    // previews the same on every machine.
    QVariantMap candidate = PanelSettingsSchema::runtimeValues(
        PanelSettingsFieldScope::Global, {});
    candidate.insert(PanelSettingsSchema::runtimeValues(
        PanelSettingsFieldScope::Panel, definition.toLegacyMap()));
    const QVariantMap resolution =
        m_registry.resolvePanelCapabilities(definition).toVariantMap();
    candidate.insert(QStringLiteral("capabilityResolution"), resolution);
    candidate.insert(
        QStringLiteral("effectiveRendererTier"),
        resolution.value(QStringLiteral("renderer")).toMap()
            .value(QStringLiteral("effectiveTier")));
    candidate.insert(QStringLiteral("iconStyleDefinition"), iconStyleProjection);
    candidate.insert(QStringLiteral("animationProfiles"),
                     m_registry.animationProfileDefinitions());
    return candidate;
}

QVariantMap PresetLibrary::panelCard(const PanelPresetDefinition &preset) const
{
    const IconStyleStore &styles = *m_registry.iconStyleStore();
    const QVariantList themes = m_registry.themeDefinitions();
    const PresetCompatibility compatibility =
        PresetCapabilityResolver::resolvePanelPreset(
            preset, themes, PanelCapabilityResolver::productionRenderers(),
            PanelCapabilityResolver::productionPlatform(),
            [this](const QString &themeId)
            {
                return themeUsable(themeId);
            });
    const PanelDefinition &declared = preset.panel.configuration;
    const std::optional<IconPresetDefinition> recommended =
        iconPreset(preset.panel.recommendedIconPresetId);

    QVariantMap card = identityCard(preset.identity, panelKind);
    card.insert(QStringLiteral("hostKinds"), preset.compatibility.hostKinds);
    card.insert(QStringLiteral("orientations"), preset.compatibility.orientations);
    card.insert(QStringLiteral("layouts"), preset.compatibility.layouts);
    card.insert(QStringLiteral("hostKind"),
                PanelDefinition::hostKindName(declared.host.kind));
    card.insert(QStringLiteral("layout"), declared.layout.pathType);
    card.insert(QStringLiteral("rendererTier"), preset.preview.rendererTier);
    card.insert(QStringLiteral("fallbackTier"), preset.preview.fallbackTier);
    card.insert(QStringLiteral("themeId"), preset.panel.themeId());
    card.insert(QStringLiteral("themeName"),
                entryName(themes, preset.panel.themeId()));
    card.insert(QStringLiteral("visibilityMode"), declared.visibility.hostMode);
    card.insert(QStringLiteral("presentationMode"), declared.presentation.mode);
    card.insert(QStringLiteral("presentationTrigger"),
                declared.presentation.trigger);
    card.insert(QStringLiteral("collapseMechanism"),
                declared.presentation.collapseMechanism);
    card.insert(QStringLiteral("motionProfileId"), declared.motion.iconProfile);
    card.insert(QStringLiteral("motionProfileName"),
                entryName(m_registry.animationProfileDefinitions(),
                          declared.motion.iconProfile));
    card.insert(QStringLiteral("motionTrigger"), declared.motion.trigger);
    card.insert(QStringLiteral("recommendedIconPresetId"),
                preset.panel.recommendedIconPresetId);
    card.insert(QStringLiteral("recommendedIconPresetName"),
                recommended.has_value() ? recommended->identity.name : QString{});
    card.insert(QStringLiteral("compatibility"), compatibility.toVariantMap());

    if (compatibility.effectiveConfiguration.has_value())
    {
        // A panel preset has no icon style of its own: it is previewed with
        // the icon preset it recommends, or with its declared fallback.
        PanelDefinition drawn = *compatibility.effectiveConfiguration;
        const std::optional<IconPresetDefinition> icons = recommended.has_value()
            ? recommended : iconPreset(preset.fallback.iconPresetId);
        const QVariantMap style = icons.has_value()
            ? iconStyleProjection(styles, *icons)
            : styles.resolve(drawn.iconStyle.styleReference);
        drawn.iconStyle.styleReference =
            style.value(QStringLiteral("resolvedStyleId")).toString();
        card.insert(QStringLiteral("preview"), QVariantMap{
            {QStringLiteral("panelDefinition"), previewCandidate(drawn, style)},
            {QStringLiteral("themeDefinition"),
             resolvedTheme(compatibility.effectiveThemeId)},
            {QStringLiteral("previewMode"), preset.preview.previewMode},
        });
    }
    return card;
}

QVariantMap PresetLibrary::iconCard(const IconPresetDefinition &preset) const
{
    const IconStyleStore &styles = *m_registry.iconStyleStore();
    const PresetCompatibility compatibility =
        PresetCapabilityResolver::resolveIconPreset(
            preset, styles, *m_registry.animationProfileCatalog());

    QVariantMap stateMotion;
    for (auto entry = preset.icon.perStateAnimationOverrides.cbegin();
         entry != preset.icon.perStateAnimationOverrides.cend(); ++entry)
    {
        stateMotion.insert(entry.key(), entry.value());
    }

    QVariantMap card = identityCard(preset.identity, iconKind);
    card.insert(QStringLiteral("iconStyleId"), preset.icon.iconStyleId);
    card.insert(QStringLiteral("iconStyleName"),
                entryName(styles.catalogEntries(), preset.icon.iconStyleId));
    card.insert(QStringLiteral("customized"),
                !preset.icon.visualOverrides.isEmpty() ||
                    !preset.icon.stateOverrides.isEmpty());
    card.insert(QStringLiteral("rendererTiers"), preset.compatibility.rendererTiers);
    card.insert(QStringLiteral("reducedMotionSupport"),
                preset.compatibility.reducedMotionSupport);
    card.insert(QStringLiteral("glyphMode"), preset.icon.glyphPolicy.mode);
    card.insert(QStringLiteral("motionProfileId"), preset.icon.motion.profileId);
    card.insert(QStringLiteral("motionProfileName"),
                entryName(m_registry.animationProfileDefinitions(),
                          preset.icon.motion.profileId));
    card.insert(QStringLiteral("motionTrigger"), preset.icon.motion.trigger);
    card.insert(QStringLiteral("stateMotionOverrides"), stateMotion);
    card.insert(QStringLiteral("compatibility"), compatibility.toVariantMap());

    if (compatibility.available)
    {
        // An icon preset carries no panel: it is previewed on a plain default
        // dock that takes only this preset's icon-layer settings.
        QVariantMap record = PanelDefinition::defaults(
            QStringLiteral("preset-preview"), preset.identity.name,
            QStringLiteral("bottom"), false).toLegacyMap();
        record.insert(preset.panelValues());
        record.insert(QStringLiteral("iconStyle"),
                      compatibility.effectiveIconStyleId);
        record.insert(QStringLiteral("iconAnimation"),
                      compatibility.effectiveMotionProfileId);
        const std::optional<PanelDefinition> dock =
            PanelDefinition::fromLegacyMap(record);
        if (dock.has_value())
        {
            card.insert(QStringLiteral("preview"), QVariantMap{
                {QStringLiteral("panelDefinition"),
                 previewCandidate(*dock, iconStyleProjection(styles, preset))},
                {QStringLiteral("themeDefinition"), QVariantMap{}},
                {QStringLiteral("previewMode"), QStringLiteral("horizontal")},
            });
        }
    }
    return card;
}

QVariantMap PresetLibrary::finished(const QString &errorCode,
                                    const QString &presetId)
{
    const bool success = !presetId.isEmpty();
    if (success)
    {
        ++m_revision;
        emit revisionChanged();
    }
    return {
        {QStringLiteral("success"), success},
        {QStringLiteral("errorCode"), success ? QString{} : errorCode},
        {QStringLiteral("presetId"), presetId},
    };
}

}
