#include "PresetCatalog.h"

#include "PresetCapabilityResolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QSet>

namespace
{

using namespace ArchDock;
using namespace ArchDock::PresetParsing;

constexpr int CatalogVersion = 1;

struct CatalogFiles
{
    QString indexPath;
    QStringList ids;
    QHash<QString, QString> paths;
    QHash<QString, QVariantMap> objects;
};

bool isContainedPath(const QString &root, const QString &candidate)
{
    return candidate.startsWith(root + QLatin1Char('/'));
}

// Diagnostics from one definition are reported against its file, so a
// catalog-wide report still says which preset is wrong.
void appendForFile(QVector<PresetValidationDiagnostic> *target,
                   const QString &fileName,
                   const QVector<PresetValidationDiagnostic> &found)
{
    for (PresetValidationDiagnostic diagnostic : found)
    {
        diagnostic.jsonPointer = fileName + QLatin1Char('#') + diagnostic.jsonPointer;
        target->append(diagnostic);
    }
}

std::optional<CatalogFiles> readCatalogFiles(
    const QString &indexPath,
    const QString &expectedFormat,
    QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QFileInfo indexInfo(indexPath);
    const QString indexName = indexInfo.fileName();
    const std::optional<QVariantMap> index =
        readPresetDefinitionFile(indexPath, diagnostics);
    if (!index.has_value())
    {
        return std::nullopt;
    }
    const QString directory = QFileInfo(indexInfo.absolutePath()).canonicalFilePath();
    if (directory.isEmpty())
    {
        addDiagnostic(diagnostics, QStringLiteral("unsafe-path"), indexName,
                      QStringLiteral("preset catalog directory cannot be resolved"));
        return std::nullopt;
    }

    rejectUnknownKeys(*index, {
        QStringLiteral("format"), QStringLiteral("version"),
        QStringLiteral("presets"),
    }, indexName + QLatin1Char('#'), diagnostics);
    if (index->value(QStringLiteral("format")).toString() != expectedFormat)
    {
        addDiagnostic(diagnostics, QStringLiteral("unsupported-format"),
                      indexName + QStringLiteral("#/format"),
                      QStringLiteral("preset catalog format is unsupported"));
        return std::nullopt;
    }
    const QVariant version = index->value(QStringLiteral("version"));
    if (!isInteger(version) || version.toInt() != CatalogVersion)
    {
        addDiagnostic(diagnostics, QStringLiteral("unsupported-version"),
                      indexName + QStringLiteral("#/version"),
                      QStringLiteral("preset catalog version is unsupported"));
        return std::nullopt;
    }
    const QVariant presets = index->value(QStringLiteral("presets"));
    if (presets.metaType().id() != QMetaType::QVariantList)
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-type"),
                      indexName + QStringLiteral("#/presets"),
                      QStringLiteral("presets must be an array of preset ids"));
        return std::nullopt;
    }
    const QVariantList entries = presets.toList();
    if (entries.size() > IconPresetCatalog::MaximumPresets)
    {
        addDiagnostic(diagnostics, QStringLiteral("limit-exceeded"),
                      indexName + QStringLiteral("#/presets"),
                      QStringLiteral("the catalog lists too many presets"));
        return std::nullopt;
    }

    CatalogFiles files;
    files.indexPath = indexInfo.absoluteFilePath();
    QSet<QString> listedFiles{indexName};
    for (qsizetype position = 0; position < entries.size(); ++position)
    {
        const QString pointer = indexName + QStringLiteral("#/presets/") +
            QString::number(position);
        const QVariant entry = entries.at(position);
        const QString id = entry.toString();
        if (entry.metaType().id() != QMetaType::QString ||
            !PresetIdentity::isValidId(id) || PresetIdentity::isUserId(id))
        {
            addDiagnostic(diagnostics, QStringLiteral("invalid-id"), pointer,
                          QStringLiteral("catalog entry is not a built-in preset id"));
            continue;
        }
        if (files.ids.contains(id))
        {
            addDiagnostic(diagnostics, QStringLiteral("duplicate-id"), pointer,
                          QStringLiteral("preset id is listed more than once"));
            continue;
        }

        const QString fileName = id + QStringLiteral(".json");
        listedFiles.insert(fileName);
        const QString path = QDir(directory).filePath(fileName);
        const QString canonicalPath = QFileInfo(path).canonicalFilePath();
        if (!canonicalPath.isEmpty() && !isContainedPath(directory, canonicalPath))
        {
            addDiagnostic(diagnostics, QStringLiteral("unsafe-path"), fileName,
                          QStringLiteral("preset definition is outside its catalog"));
            continue;
        }
        QVector<PresetValidationDiagnostic> fileDiagnostics;
        const std::optional<QVariantMap> object =
            readPresetDefinitionFile(path, &fileDiagnostics);
        diagnostics->append(fileDiagnostics);
        if (!object.has_value())
        {
            continue;
        }
        files.ids.append(id);
        files.paths.insert(id, path);
        files.objects.insert(id, *object);
    }

    // The index is the catalog. A definition nobody listed is a mistake, not
    // an extra preset: it would make the installed count unprovable.
    const QStringList present = QDir(directory).entryList(
        {QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &fileName : present)
    {
        if (!listedFiles.contains(fileName))
        {
            addDiagnostic(diagnostics, QStringLiteral("unlisted-definition"),
                          fileName,
                          QStringLiteral("definition file is not listed by the catalog"));
        }
    }
    return files;
}

// Parses and validates every listed definition. A definition is admitted only
// when it parses, matches its catalog id, is a built-in and every reference
// resolves.
template<typename Definition, typename Validator>
void loadDefinitions(const CatalogFiles &files,
                     Validator validateReferences,
                     QStringList *order,
                     QHash<QString, Definition> *presets,
                     QVector<PresetValidationDiagnostic> *diagnostics)
{
    for (const QString &id : files.ids)
    {
        QVector<PresetValidationDiagnostic> found;
        const std::optional<Definition> definition =
            Definition::fromVariantMap(files.objects.value(id), &found);
        if (definition.has_value())
        {
            if (definition->identity.id != id)
            {
                addDiagnostic(&found, QStringLiteral("catalog-identity-mismatch"),
                              QStringLiteral("/identity/id"),
                              QStringLiteral("definition id differs from its catalog id"));
            }
            if (!definition->identity.builtIn)
            {
                addDiagnostic(&found, QStringLiteral("invalid-value"),
                              QStringLiteral("/identity/builtIn"),
                              QStringLiteral("an installed catalog entry must be a built-in"));
            }
            found.append(validateReferences(*definition));
        }
        appendForFile(diagnostics, id + QStringLiteral(".json"), found);
        if (definition.has_value() && !presetDiagnosticsHaveErrors(found))
        {
            order->append(id);
            presets->insert(id, *definition);
        }
    }
}

QString firstErrorCode(const QVector<PresetValidationDiagnostic> &diagnostics)
{
    for (const PresetValidationDiagnostic &diagnostic : diagnostics)
    {
        if (diagnostic.severity == QStringLiteral("error"))
        {
            return diagnostic.code;
        }
    }
    return {};
}

// A motion profile must exist and must declare every renderer tier the preset
// can be drawn on, so a card never promises motion a tier cannot run.
void checkMotionProfile(const PresetReferenceContext &context,
                        const QString &profileId,
                        const QStringList &rendererTiers,
                        const QString &pointer,
                        QVector<PresetValidationDiagnostic> *diagnostics)
{
    const AnimationProfileDefinition *profile = context.animationProfiles
        ? context.animationProfiles->profileById(profileId) : nullptr;
    if (!profile)
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-reference"), pointer,
                      QStringLiteral("motion profile is not in the animation catalog: ") +
                          profileId);
        return;
    }
    for (const QString &tier : rendererTiers)
    {
        if (!profile->rendererRequirements.contains(tier))
        {
            addDiagnostic(diagnostics, QStringLiteral("inconsistent-declaration"),
                          pointer,
                          QStringLiteral("motion profile '%1' does not declare renderer tier %2")
                              .arg(profileId, tier));
        }
    }
}

bool themeExists(const PresetReferenceContext &context, const QString &themeId)
{
    for (const QVariant &candidate : context.themeCatalog)
    {
        if (candidate.toMap().value(QStringLiteral("id")).toString() == themeId)
        {
            return true;
        }
    }
    return false;
}

}

namespace ArchDock
{

std::optional<QVariantMap> readPresetDefinitionFile(
    const QString &path,
    QVector<PresetValidationDiagnostic> *diagnostics)
{
    const QFileInfo info(path);
    const QString name = info.fileName();
    if (!info.exists())
    {
        addDiagnostic(diagnostics, QStringLiteral("missing-file"), name,
                      QStringLiteral("preset file does not exist"));
        return std::nullopt;
    }
    if (!info.isFile())
    {
        addDiagnostic(diagnostics, QStringLiteral("file-not-regular"), name,
                      QStringLiteral("preset file is not a regular file"));
        return std::nullopt;
    }
    if (info.size() > IconPresetCatalog::MaximumFileBytes)
    {
        addDiagnostic(diagnostics, QStringLiteral("file-too-large"), name,
                      QStringLiteral("preset file exceeds the byte limit"));
        return std::nullopt;
    }
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
    {
        addDiagnostic(diagnostics, QStringLiteral("missing-file"), name,
                      QStringLiteral("preset file could not be read"));
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        file.read(IconPresetCatalog::MaximumFileBytes + 1), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        addDiagnostic(diagnostics, QStringLiteral("invalid-json"), name,
                      QStringLiteral("preset file is not a JSON object"));
        return std::nullopt;
    }
    return document.object().toVariantMap();
}

QVector<PresetValidationDiagnostic> validateIconPresetReferences(
    const IconPresetDefinition &preset,
    const PresetReferenceContext &context)
{
    QVector<PresetValidationDiagnostic> result;
    if (!context.iconStyles || !context.animationProfiles)
    {
        addDiagnostic(&result, QStringLiteral("missing-resource"), QString{},
                      QStringLiteral("icon-style or animation catalog is unavailable"));
        return result;
    }

    const IconStylePackage *style =
        context.iconStyles->packageById(preset.icon.iconStyleId);
    if (!style)
    {
        addDiagnostic(&result, QStringLiteral("invalid-reference"),
                      QStringLiteral("/icon/iconStyleId"),
                      QStringLiteral("icon style is not in the icon-style catalog"));
    }
    if (!context.iconStyles->contains(preset.fallback.iconStyleId))
    {
        addDiagnostic(&result, QStringLiteral("invalid-reference"),
                      QStringLiteral("/fallback/iconStyleId"),
                      QStringLiteral("fallback icon style is not in the icon-style catalog"));
    }

    checkMotionProfile(context, preset.icon.motion.profileId, {},
                       QStringLiteral("/icon/motion/profileId"), &result);
    checkMotionProfile(context, preset.fallback.motionProfileId, {},
                       QStringLiteral("/fallback/motionProfileId"), &result);
    for (auto it = preset.icon.perStateAnimationOverrides.cbegin();
         it != preset.icon.perStateAnimationOverrides.cend(); ++it)
    {
        checkMotionProfile(
            context, it.value(), {},
            QStringLiteral("/icon/perStateAnimationOverrides/") + it.key(), &result);
    }
    if (preset.compatibility.reducedMotionSupport)
    {
        const AnimationProfileDefinition *profile =
            context.animationProfiles->profileById(preset.icon.motion.profileId);
        if (profile && profile->reducedMotion.mode.isEmpty())
        {
            addDiagnostic(&result, QStringLiteral("inconsistent-declaration"),
                          QStringLiteral("/compatibility/reducedMotionSupport"),
                          QStringLiteral("the motion profile declares no reduced-motion behavior"));
        }
    }

    if (style)
    {
        const IconStyleCapabilityDefinition &capabilities =
            style->definition().capabilities;
        for (const QString &capability :
             preset.compatibility.requiredStyleCapabilities)
        {
            if (!capabilities.features.contains(capability))
            {
                addDiagnostic(&result, QStringLiteral("invalid-reference"),
                              QStringLiteral("/compatibility/requiredStyleCapabilities"),
                              QStringLiteral("the icon style does not declare capability: ") +
                                  capability);
            }
        }
        for (const QString &tier : preset.compatibility.rendererTiers)
        {
            if (!capabilities.rendererTiers.contains(tier))
            {
                addDiagnostic(&result, QStringLiteral("inconsistent-declaration"),
                              QStringLiteral("/compatibility/rendererTiers"),
                              QStringLiteral("the icon style does not declare renderer tier: ") +
                                  tier);
            }
        }
        // The overrides must merge into a style the icon-style validator
        // accepts; otherwise the preset would draw something unproven.
        result.append(PresetCapabilityResolver::resolveIconStyle(
            *context.iconStyles, preset).diagnostics);
    }
    return result;
}

QVector<PresetValidationDiagnostic> validatePanelPresetReferences(
    const PanelPresetDefinition &preset,
    const PresetReferenceContext &context)
{
    QVector<PresetValidationDiagnostic> result;
    if (!context.animationProfiles)
    {
        addDiagnostic(&result, QStringLiteral("missing-resource"), QString{},
                      QStringLiteral("animation catalog is unavailable"));
        return result;
    }

    const QString themeId = preset.panel.themeId();
    if (!themeId.isEmpty() && !themeExists(context, themeId))
    {
        addDiagnostic(&result, QStringLiteral("invalid-reference"),
                      QStringLiteral("/panel/theme/completeThemeId"),
                      QStringLiteral("theme is not in the built-in theme catalog"));
    }
    if (!preset.fallback.themeId.isEmpty() &&
        !themeExists(context, preset.fallback.themeId))
    {
        addDiagnostic(&result, QStringLiteral("invalid-reference"),
                      QStringLiteral("/fallback/themeId"),
                      QStringLiteral("fallback theme is not in the built-in theme catalog"));
    }

    const auto iconPreset = [&context](const QString &id)
        -> const IconPresetDefinition *
    {
        return context.iconPresetById ? context.iconPresetById(id) : nullptr;
    };
    const IconPresetDefinition *recommended = nullptr;
    if (!preset.panel.recommendedIconPresetId.isEmpty())
    {
        recommended = iconPreset(preset.panel.recommendedIconPresetId);
        if (!recommended)
        {
            addDiagnostic(&result, QStringLiteral("invalid-reference"),
                          QStringLiteral("/panel/recommendedIconPresetId"),
                          QStringLiteral("recommended icon preset does not exist"));
        }
    }
    if (!preset.fallback.iconPresetId.isEmpty() &&
        !iconPreset(preset.fallback.iconPresetId))
    {
        addDiagnostic(&result, QStringLiteral("invalid-reference"),
                      QStringLiteral("/fallback/iconPresetId"),
                      QStringLiteral("fallback icon preset does not exist"));
    }

    QStringList tiers{preset.preview.rendererTier};
    if (!tiers.contains(preset.preview.fallbackTier))
    {
        tiers.append(preset.preview.fallbackTier);
    }
    checkMotionProfile(context, preset.panel.configuration.motion.iconProfile, tiers,
                       QStringLiteral("/panel/motion/iconAnimation"), &result);
    if (recommended)
    {
        checkMotionProfile(context, recommended->icon.motion.profileId, tiers,
                           QStringLiteral("/panel/recommendedIconPresetId"), &result);
    }
    // The theme, host, layout, presentation, tiers and declared fallback must
    // actually fit together under the panel capability model.
    result.append(PresetCapabilityResolver::validatePanelPreset(
        preset, context.themeCatalog));
    return result;
}

QString IconPresetCatalog::indexFileName()
{
    return QStringLiteral("builtin-icon-presets.json");
}

QString IconPresetCatalog::indexFormat()
{
    return QStringLiteral("org.archdock.icon-preset-catalog");
}

IconPresetCatalogLoadResult IconPresetCatalog::loadBuiltIn(
    const QString &indexPath,
    const PresetReferenceContext &context)
{
    IconPresetCatalogLoadResult result;
    const std::optional<CatalogFiles> files =
        readCatalogFiles(indexPath, indexFormat(), &result.diagnostics);
    if (!files.has_value())
    {
        return result;
    }
    IconPresetCatalog catalog;
    catalog.m_indexPath = files->indexPath;
    catalog.m_paths = files->paths;
    loadDefinitions<IconPresetDefinition>(
        *files,
        [&context](const IconPresetDefinition &preset)
        {
            return validateIconPresetReferences(preset, context);
        },
        &catalog.m_order, &catalog.m_presets, &result.diagnostics);
    if (!presetDiagnosticsHaveErrors(result.diagnostics))
    {
        result.catalog = std::move(catalog);
    }
    return result;
}

bool IconPresetCatalog::contains(const QString &presetId) const
{
    return m_presets.contains(presetId);
}

QStringList IconPresetCatalog::presetIds() const
{
    return m_order;
}

const IconPresetDefinition *IconPresetCatalog::presetById(
    const QString &presetId) const
{
    const auto match = m_presets.constFind(presetId);
    return match == m_presets.cend() ? nullptr : &match.value();
}

QString IconPresetCatalog::indexPath() const
{
    return m_indexPath;
}

QString IconPresetCatalog::definitionPath(const QString &presetId) const
{
    return m_paths.value(presetId);
}

bool IconPresetCatalogLoadResult::isValid() const
{
    return catalog.has_value() && !presetDiagnosticsHaveErrors(diagnostics);
}

QString IconPresetCatalogLoadResult::primaryCode() const
{
    return firstErrorCode(diagnostics);
}

QString PanelPresetCatalog::indexFileName()
{
    return QStringLiteral("builtin-panel-presets.json");
}

QString PanelPresetCatalog::indexFormat()
{
    return QStringLiteral("org.archdock.panel-preset-catalog");
}

PanelPresetCatalogLoadResult PanelPresetCatalog::loadBuiltIn(
    const QString &indexPath,
    const PresetReferenceContext &context)
{
    PanelPresetCatalogLoadResult result;
    const std::optional<CatalogFiles> files =
        readCatalogFiles(indexPath, indexFormat(), &result.diagnostics);
    if (!files.has_value())
    {
        return result;
    }
    PanelPresetCatalog catalog;
    catalog.m_indexPath = files->indexPath;
    catalog.m_paths = files->paths;
    loadDefinitions<PanelPresetDefinition>(
        *files,
        [&context](const PanelPresetDefinition &preset)
        {
            return validatePanelPresetReferences(preset, context);
        },
        &catalog.m_order, &catalog.m_presets, &result.diagnostics);
    if (!presetDiagnosticsHaveErrors(result.diagnostics))
    {
        result.catalog = std::move(catalog);
    }
    return result;
}

bool PanelPresetCatalog::contains(const QString &presetId) const
{
    return m_presets.contains(presetId);
}

QStringList PanelPresetCatalog::presetIds() const
{
    return m_order;
}

const PanelPresetDefinition *PanelPresetCatalog::presetById(
    const QString &presetId) const
{
    const auto match = m_presets.constFind(presetId);
    return match == m_presets.cend() ? nullptr : &match.value();
}

QString PanelPresetCatalog::indexPath() const
{
    return m_indexPath;
}

QString PanelPresetCatalog::definitionPath(const QString &presetId) const
{
    return m_paths.value(presetId);
}

bool PanelPresetCatalogLoadResult::isValid() const
{
    return catalog.has_value() && !presetDiagnosticsHaveErrors(diagnostics);
}

QString PanelPresetCatalogLoadResult::primaryCode() const
{
    return firstErrorCode(diagnostics);
}

}
