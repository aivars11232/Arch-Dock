#pragma once

#include "../animation/AnimationProfileCatalog.h"
#include "../iconstyles/IconStyleStore.h"
#include "../model/IconPresetDefinition.h"
#include "../model/PanelPresetDefinition.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <functional>
#include <optional>

namespace ArchDock
{

// Everything a preset may refer to. A catalog uses it to prove that each
// reference resolves to a resource that is actually implemented, so a preset
// can never name a theme, icon style, motion profile or icon preset that does
// not exist.
struct PresetReferenceContext
{
    QVariantList themeCatalog;
    const IconStyleStore *iconStyles = nullptr;
    const AnimationProfileCatalog *animationProfiles = nullptr;
    // Icon presets a panel preset may recommend or fall back to.
    std::function<const IconPresetDefinition *(const QString &)> iconPresetById;
};

[[nodiscard]] QVector<PresetValidationDiagnostic> validateIconPresetReferences(
    const IconPresetDefinition &preset,
    const PresetReferenceContext &context);
[[nodiscard]] QVector<PresetValidationDiagnostic> validatePanelPresetReferences(
    const PanelPresetDefinition &preset,
    const PresetReferenceContext &context);

// Reads one preset definition file as a JSON object, bounded in size and
// refusing anything that is not a regular file.
[[nodiscard]] std::optional<QVariantMap> readPresetDefinitionFile(
    const QString &path,
    QVector<PresetValidationDiagnostic> *diagnostics);

struct IconPresetCatalogLoadResult;
struct PanelPresetCatalogLoadResult;

// The immutable installed Icon Preset catalog: an index naming each built-in
// in order, and one definition file per preset beside it. It has no write
// path. A separate type from the Panel Preset catalog on purpose.
class IconPresetCatalog
{
public:
    static constexpr qsizetype MaximumFileBytes = 262144;
    static constexpr qsizetype MaximumPresets = 256;

    [[nodiscard]] static QString indexFileName();
    [[nodiscard]] static QString indexFormat();
    [[nodiscard]] static IconPresetCatalogLoadResult loadBuiltIn(
        const QString &indexPath,
        const PresetReferenceContext &context);

    [[nodiscard]] bool contains(const QString &presetId) const;
    [[nodiscard]] QStringList presetIds() const;
    [[nodiscard]] const IconPresetDefinition *presetById(
        const QString &presetId) const;
    [[nodiscard]] QString indexPath() const;
    [[nodiscard]] QString definitionPath(const QString &presetId) const;

private:
    QString m_indexPath;
    QStringList m_order;
    QHash<QString, IconPresetDefinition> m_presets;
    QHash<QString, QString> m_paths;
};

struct IconPresetCatalogLoadResult
{
    std::optional<IconPresetCatalog> catalog;
    QVector<PresetValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString primaryCode() const;
};

// The immutable installed Panel Preset catalog. Same on-disk shape as the
// Icon Preset catalog, a different format, directory and definition type.
class PanelPresetCatalog
{
public:
    static constexpr qsizetype MaximumFileBytes = 262144;
    static constexpr qsizetype MaximumPresets = 256;

    [[nodiscard]] static QString indexFileName();
    [[nodiscard]] static QString indexFormat();
    [[nodiscard]] static PanelPresetCatalogLoadResult loadBuiltIn(
        const QString &indexPath,
        const PresetReferenceContext &context);

    [[nodiscard]] bool contains(const QString &presetId) const;
    [[nodiscard]] QStringList presetIds() const;
    [[nodiscard]] const PanelPresetDefinition *presetById(
        const QString &presetId) const;
    [[nodiscard]] QString indexPath() const;
    [[nodiscard]] QString definitionPath(const QString &presetId) const;

private:
    QString m_indexPath;
    QStringList m_order;
    QHash<QString, PanelPresetDefinition> m_presets;
    QHash<QString, QString> m_paths;
};

struct PanelPresetCatalogLoadResult
{
    std::optional<PanelPresetCatalog> catalog;
    QVector<PresetValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString primaryCode() const;
};

}
