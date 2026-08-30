#pragma once

#include "../model/ThemeDefinition.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct ThemePackageLoadResult;
struct ThemePackageMaterializeResult;

class ThemePackage
{
public:
    static constexpr qsizetype MaximumManifestBytes = 262144;
    static constexpr qsizetype MaximumStringBytes = 4096;
    static constexpr qsizetype MaximumIdentifierBytes = 64;
    static constexpr qsizetype MaximumAssets = 128;
    static constexpr qsizetype MaximumStates = 32;
    static constexpr qsizetype MaximumLayers = 128;
    static constexpr qsizetype MaximumSlices = 64;
    static constexpr qsizetype MaximumContentRegions = 64;
    static constexpr qsizetype MaximumInputMasks = 64;
    static constexpr qint64 MaximumAssetBytes = 67108864;
    static constexpr qint64 MaximumPackageAssetBytes = 268435456;
    static constexpr int MaximumRasterDimension = 16384;
    static constexpr qint64 MaximumRasterPixels = 16777216;

    [[nodiscard]] static ThemePackageLoadResult load(
        const QString &manifestPath);
    [[nodiscard]] static ThemePackageLoadResult loadBytes(
        const QByteArray &manifestBytes,
        const QString &packageRoot,
        const QString &manifestPath = {});

    [[nodiscard]] ThemePackageMaterializeResult materialize(
        const QString &managedPackagesRoot) const;

    [[nodiscard]] const ThemeDefinition &definition() const;
    [[nodiscard]] int sourceVersion() const;
    [[nodiscard]] QString sourceRoot() const;
    [[nodiscard]] QString manifestPath() const;
    [[nodiscard]] QString assetPath(const QString &assetId) const;
    [[nodiscard]] QString primarySurfacePath() const;
    [[nodiscard]] QString legacyFit() const;
    [[nodiscard]] QByteArray contentDigest() const;
    [[nodiscard]] QVariantMap runtimeProjection() const;

private:
    ThemeDefinition m_definition;
    int m_sourceVersion = ThemeDefinition::CurrentVersion;
    QString m_sourceRoot;
    QString m_manifestPath;
    QString m_legacyFit = QStringLiteral("cover");
    QByteArray m_contentDigest;
    QVariantMap m_sourceManifest;
    QHash<QString, QString> m_assetPaths;

    friend struct ThemePackageLoadResult;
};

struct ThemePackageLoadResult
{
    std::optional<ThemePackage> package;
    QVector<ThemeValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString primaryCode() const;
    [[nodiscard]] QString primaryMessage() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

struct ThemePackageMaterializeResult
{
    std::optional<ThemePackage> package;
    QVector<ThemeValidationDiagnostic> diagnostics;
    bool reusedExisting = false;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

}
