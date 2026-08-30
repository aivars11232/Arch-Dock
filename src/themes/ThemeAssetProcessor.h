#pragma once

#include "../model/ThemeDefinition.h"

#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include <optional>

namespace ArchDock
{

struct ThemeAssetLayerInput
{
    QString id;
    QRect sourceRect;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeAssetLayerInput &) const = default;
};

struct ThemeAssetProcessingRequest
{
    QString sourcePath;
    QString managedOutputRoot;
    QString assetId;
    QString expectedSourceSha256;
    std::optional<QRect> cropRect;
    QSize previewBounds{320, 180};
    QSize targetSize;
    QVector<qreal> scaleFactors{1.0};
    QString fit = QStringLiteral("contain");
    QVector<ThemeAssetLayerInput> layerInputs;
    QStringList cleanupRequirements;
};

struct ThemeAssetInspection
{
    QString sourceSha256;
    qint64 sourceByteSize = 0;
    QSize sourceSize;
    bool hasAlphaChannel = false;
    bool hasTransparentPixels = false;
    quint64 transparentPixelCount = 0;

    [[nodiscard]] QVariantMap toVariantMap() const;
    bool operator==(const ThemeAssetInspection &) const = default;
};

struct ThemeAssetDerivative
{
    QString id;
    QString role;
    QString relativePath;
    QString absolutePath;
    QString sha256;
    QSize pixelSize;
    qreal scaleFactor = 1.0;

    [[nodiscard]] QVariantMap toVariantMap(bool includeAbsolutePath = true) const;
    bool operator==(const ThemeAssetDerivative &) const = default;
};

struct ThemeAssetProcessingResult
{
    QString status;
    QString contentKey;
    QString outputDirectory;
    QString metadataPath;
    QString metadataSha256;
    ThemeAssetInspection inspection;
    QVector<ThemeAssetDerivative> derivatives;
    QVector<ThemeValidationDiagnostic> diagnostics;
    bool manualReviewRequired = false;
    bool productionReady = false;
    bool reusedExisting = false;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QString primaryCode() const;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

class ThemeAssetProcessor
{
public:
    static constexpr qint64 MaximumSourceBytes = 67108864;
    static constexpr int MaximumSourceDimension = 16384;
    static constexpr qint64 MaximumSourcePixels = 16777216;
    static constexpr int MaximumOutputDimension = 4096;
    static constexpr qint64 MaximumOutputPixels = 16777216;
    static constexpr qsizetype MaximumScaleFactors = 8;
    static constexpr qsizetype MaximumLayerInputs = 32;

    [[nodiscard]] static ThemeAssetProcessingResult process(
        const ThemeAssetProcessingRequest &request);
};

}
