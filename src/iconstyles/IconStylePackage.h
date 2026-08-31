#pragma once

#include "../model/IconStyleDefinition.h"

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct IconStylePackageLoadResult;

class IconStylePackage
{
public:
    static constexpr qsizetype MaximumManifestBytes = 262144;
    static constexpr qsizetype MaximumStringBytes = 4096;
    static constexpr qsizetype MaximumIdentifierBytes = 64;
    static constexpr qsizetype MaximumLayersPerRole = 32;
    static constexpr qsizetype MaximumStates = 32;
    static constexpr qsizetype MaximumMappedReplacements = 512;
    static constexpr qint64 MaximumAssetBytes = 16777216;
    static constexpr qint64 MaximumPackageAssetBytes = 67108864;

    [[nodiscard]] static IconStylePackageLoadResult load(
        const QString &manifestPath);
    [[nodiscard]] static IconStylePackageLoadResult loadBytes(
        const QByteArray &manifestBytes,
        const QString &packageRoot,
        const QString &manifestPath = {});

    [[nodiscard]] const IconStyleDefinition &definition() const;
    [[nodiscard]] QString sourceRoot() const;
    [[nodiscard]] QString manifestPath() const;
    [[nodiscard]] QString assetPath(const QString &relativePath) const;
    [[nodiscard]] QByteArray contentDigest() const;
    [[nodiscard]] QVariantMap runtimeProjection() const;

private:
    IconStyleDefinition m_definition;
    QString m_sourceRoot;
    QString m_manifestPath;
    QByteArray m_contentDigest;
    QHash<QString, QString> m_assetPaths;

    friend struct IconStylePackageLoadResult;
};

struct IconStylePackageLoadResult
{
    std::optional<IconStylePackage> package;
    QVector<IconStyleValidationDiagnostic> diagnostics;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString primaryCode() const;
    [[nodiscard]] QString primaryMessage() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

}
