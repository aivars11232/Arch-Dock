#include "ThemeAssetProcessor.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>

namespace
{

using namespace ArchDock;

const QString metadataFileName = QStringLiteral("processing.json");

ThemeValidationDiagnostic processorDiagnostic(const QString &code,
                                              const QString &pointer,
                                              const QString &message)
{
    return {code, pointer, QStringLiteral("error"), message};
}

bool containsErrors(const QVector<ThemeValidationDiagnostic> &diagnostics)
{
    return std::any_of(diagnostics.cbegin(), diagnostics.cend(),
                       [](const ThemeValidationDiagnostic &entry)
                       {
                           return entry.severity == QStringLiteral("error");
                       });
}

bool isContainedPath(const QString &root, const QString &candidate)
{
    return candidate == root || candidate.startsWith(root + QLatin1Char('/'));
}

QString fileSha256(const QString &path)
{
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return file.open(QIODevice::ReadOnly) && hash.addData(&file)
        ? QString::fromLatin1(hash.result().toHex()) : QString{};
}

QVariantMap rectMap(const QRect &rect)
{
    return {
        {QStringLiteral("height"), rect.height()},
        {QStringLiteral("width"), rect.width()},
        {QStringLiteral("x"), rect.x()},
        {QStringLiteral("y"), rect.y()},
    };
}

QImage fittedImage(const QImage &source,
                   const QSize &targetSize,
                   const QString &fit)
{
    QImage result(targetSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (fit == QStringLiteral("tile"))
    {
        for (int y = 0; y < targetSize.height(); y += source.height())
        {
            for (int x = 0; x < targetSize.width(); x += source.width())
            {
                painter.drawImage(QPoint(x, y), source);
            }
        }
        return result;
    }

    const Qt::AspectRatioMode aspectMode = fit == QStringLiteral("stretch")
        ? Qt::IgnoreAspectRatio
        : (fit == QStringLiteral("cover")
               ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio);
    const QImage scaled = source.scaled(
        targetSize, aspectMode, Qt::SmoothTransformation);
    painter.drawImage(
        QPoint((targetSize.width() - scaled.width()) / 2,
               (targetSize.height() - scaled.height()) / 2),
        scaled);
    return result;
}

bool writePng(const QString &path, const QImage &image, QString *errorMessage)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }
    QImageWriter writer(&file, "png");
    writer.setCompression(9);
    writer.setQuality(100);
    if (!writer.write(image) || !file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = writer.errorString().isEmpty()
                ? file.errorString() : writer.errorString();
        }
        return false;
    }
    return true;
}

QVariantMap requestSettings(const ThemeAssetProcessingRequest &request,
                            const QString &sourceSha256)
{
    QVariantList scales;
    for (qreal scale : request.scaleFactors)
    {
        scales.append(scale);
    }
    QVariantList layers;
    for (const ThemeAssetLayerInput &layer : request.layerInputs)
    {
        layers.append(layer.toVariantMap());
    }
    QVariantMap result{
        {QStringLiteral("assetId"), request.assetId},
        {QStringLiteral("cleanupRequirements"), request.cleanupRequirements},
        {QStringLiteral("fit"), request.fit},
        {QStringLiteral("layerInputs"), layers},
        {QStringLiteral("previewBounds"), QVariantMap{
             {QStringLiteral("height"), request.previewBounds.height()},
             {QStringLiteral("width"), request.previewBounds.width()},
         }},
        {QStringLiteral("scaleFactors"), scales},
        {QStringLiteral("sourceSha256"), sourceSha256},
        {QStringLiteral("targetSize"), QVariantMap{
             {QStringLiteral("height"), request.targetSize.height()},
             {QStringLiteral("width"), request.targetSize.width()},
         }},
    };
    if (request.cropRect.has_value())
    {
        result.insert(QStringLiteral("cropRect"), rectMap(*request.cropRect));
    }
    return result;
}

QByteArray settingsDigest(const QVariantMap &settings)
{
    return QCryptographicHash::hash(
        QJsonDocument(QJsonObject::fromVariantMap(settings))
            .toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256);
}

QVariantMap derivativeMetadata(const ThemeAssetDerivative &derivative)
{
    return derivative.toVariantMap(false);
}

QVariantMap processingMetadata(const ThemeAssetProcessingRequest &request,
                               const ThemeAssetProcessingResult &result,
                               const QVariantMap &settings)
{
    QVariantList derivatives;
    derivatives.reserve(result.derivatives.size());
    for (const ThemeAssetDerivative &derivative : result.derivatives)
    {
        derivatives.append(derivativeMetadata(derivative));
    }
    return {
        {QStringLiteral("assetId"), request.assetId},
        {QStringLiteral("contentKey"), result.contentKey},
        {QStringLiteral("derivatives"), derivatives},
        {QStringLiteral("format"), QStringLiteral("org.archdock.processed-theme-asset")},
        {QStringLiteral("inspection"), result.inspection.toVariantMap()},
        {QStringLiteral("manualReviewRequired"), result.manualReviewRequired},
        {QStringLiteral("productionReady"), result.productionReady},
        {QStringLiteral("settings"), settings},
        {QStringLiteral("status"), result.status},
        {QStringLiteral("version"), 1},
    };
}

bool appendDerivative(const QString &stagingPath,
                      const QString &id,
                      const QString &role,
                      const QString &relativePath,
                      const QImage &image,
                      qreal scale,
                      ThemeAssetProcessingResult *result)
{
    const QString absolutePath = QDir(stagingPath).filePath(relativePath);
    if (!QDir().mkpath(QFileInfo(absolutePath).absolutePath()))
    {
        result->diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-value"), QStringLiteral("/managedOutputRoot"),
            QStringLiteral("derivative directory could not be created")));
        return false;
    }
    QString error;
    if (!writePng(absolutePath, image, &error))
    {
        result->diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-value"), QStringLiteral("/managedOutputRoot"),
            QStringLiteral("derivative could not be written: %1").arg(error)));
        return false;
    }
    ThemeAssetDerivative derivative;
    derivative.id = id;
    derivative.role = role;
    derivative.relativePath = relativePath;
    derivative.absolutePath = absolutePath;
    derivative.sha256 = fileSha256(absolutePath);
    derivative.pixelSize = image.size();
    derivative.scaleFactor = scale;
    if (derivative.sha256.isEmpty())
    {
        result->diagnostics.append(processorDiagnostic(
            QStringLiteral("asset-hash-mismatch"), QStringLiteral("/derivatives"),
            QStringLiteral("derivative hash could not be recorded")));
        return false;
    }
    result->derivatives.append(derivative);
    return true;
}

std::optional<ThemeAssetProcessingResult> loadPublished(
    const QString &directory,
    const QString &contentKey,
    const ThemeAssetInspection &inspection)
{
    const QString metadataPath = QDir(directory).filePath(metadataFileName);
    QFile metadataFile(metadataPath);
    if (!metadataFile.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    const QByteArray metadataBytes = metadataFile.readAll();
    const QJsonDocument document = QJsonDocument::fromJson(metadataBytes);
    if (!document.isObject())
    {
        return std::nullopt;
    }
    const QJsonObject metadata = document.object();
    if (metadata.value(QStringLiteral("format")).toString() !=
            QStringLiteral("org.archdock.processed-theme-asset") ||
        metadata.value(QStringLiteral("version")).toInt() != 1 ||
        metadata.value(QStringLiteral("contentKey")).toString() != contentKey)
    {
        return std::nullopt;
    }

    ThemeAssetProcessingResult result;
    result.status = metadata.value(QStringLiteral("status")).toString();
    result.contentKey = contentKey;
    result.outputDirectory = directory;
    result.metadataPath = metadataPath;
    result.metadataSha256 = QString::fromLatin1(
        QCryptographicHash::hash(metadataBytes, QCryptographicHash::Sha256).toHex());
    result.inspection = inspection;
    result.manualReviewRequired = metadata.value(
        QStringLiteral("manualReviewRequired")).toBool();
    result.productionReady = metadata.value(
        QStringLiteral("productionReady")).toBool();
    result.reusedExisting = true;

    const QJsonArray derivatives = metadata.value(
        QStringLiteral("derivatives")).toArray();
    if (derivatives.isEmpty())
    {
        return std::nullopt;
    }
    const QString canonicalDirectory = QFileInfo(directory).canonicalFilePath();
    for (const QJsonValue &value : derivatives)
    {
        if (!value.isObject())
        {
            return std::nullopt;
        }
        const QJsonObject object = value.toObject();
        ThemeAssetDerivative derivative;
        derivative.id = object.value(QStringLiteral("id")).toString();
        derivative.role = object.value(QStringLiteral("role")).toString();
        derivative.relativePath = object.value(
            QStringLiteral("relativePath")).toString();
        derivative.sha256 = object.value(QStringLiteral("sha256")).toString();
        const QJsonObject size = object.value(QStringLiteral("pixelSize")).toObject();
        derivative.pixelSize = QSize(size.value(QStringLiteral("width")).toInt(),
                                     size.value(QStringLiteral("height")).toInt());
        derivative.scaleFactor = object.value(
            QStringLiteral("scaleFactor")).toDouble(1.0);
        if (derivative.relativePath.isEmpty() ||
            QDir::isAbsolutePath(derivative.relativePath) ||
            QDir::cleanPath(derivative.relativePath) != derivative.relativePath)
        {
            return std::nullopt;
        }
        const QFileInfo fileInfo(QDir(directory).filePath(derivative.relativePath));
        derivative.absolutePath = fileInfo.canonicalFilePath();
        if (!fileInfo.isFile() || derivative.absolutePath.isEmpty() ||
            !isContainedPath(canonicalDirectory, derivative.absolutePath) ||
            fileSha256(derivative.absolutePath) != derivative.sha256)
        {
            return std::nullopt;
        }
        result.derivatives.append(derivative);
    }
    return result;
}

}

namespace ArchDock
{

QVariantMap ThemeAssetLayerInput::toVariantMap() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("sourceRect"), rectMap(sourceRect)},
    };
}

QVariantMap ThemeAssetInspection::toVariantMap() const
{
    return {
        {QStringLiteral("hasAlphaChannel"), hasAlphaChannel},
        {QStringLiteral("hasTransparentPixels"), hasTransparentPixels},
        {QStringLiteral("sourceByteSize"), sourceByteSize},
        {QStringLiteral("sourceSha256"), sourceSha256},
        {QStringLiteral("sourceSize"), QVariantMap{
             {QStringLiteral("height"), sourceSize.height()},
             {QStringLiteral("width"), sourceSize.width()},
         }},
        {QStringLiteral("transparentPixelCount"),
         QVariant::fromValue<qulonglong>(transparentPixelCount)},
    };
}

QVariantMap ThemeAssetDerivative::toVariantMap(bool includeAbsolutePath) const
{
    QVariantMap result{
        {QStringLiteral("id"), id},
        {QStringLiteral("pixelSize"), QVariantMap{
             {QStringLiteral("height"), pixelSize.height()},
             {QStringLiteral("width"), pixelSize.width()},
         }},
        {QStringLiteral("relativePath"), relativePath},
        {QStringLiteral("role"), role},
        {QStringLiteral("scaleFactor"), scaleFactor},
        {QStringLiteral("sha256"), sha256},
    };
    if (includeAbsolutePath)
    {
        result.insert(QStringLiteral("absolutePath"), absolutePath);
    }
    return result;
}

bool ThemeAssetProcessingResult::isValid() const
{
    return !outputDirectory.isEmpty() && !metadataPath.isEmpty() &&
        !derivatives.isEmpty() && !containsErrors(diagnostics);
}

QString ThemeAssetProcessingResult::primaryCode() const
{
    return diagnostics.isEmpty() ? QString{} : diagnostics.first().code;
}

QVariantMap ThemeAssetProcessingResult::toVariantMap() const
{
    QVariantList derivativeList;
    derivativeList.reserve(derivatives.size());
    for (const ThemeAssetDerivative &derivative : derivatives)
    {
        derivativeList.append(derivative.toVariantMap());
    }
    return {
        {QStringLiteral("contentKey"), contentKey},
        {QStringLiteral("derivatives"), derivativeList},
        {QStringLiteral("diagnostics"), themeDiagnosticsToVariantList(diagnostics)},
        {QStringLiteral("inspection"), inspection.toVariantMap()},
        {QStringLiteral("manualReviewRequired"), manualReviewRequired},
        {QStringLiteral("metadataPath"), metadataPath},
        {QStringLiteral("metadataSha256"), metadataSha256},
        {QStringLiteral("outputDirectory"), outputDirectory},
        {QStringLiteral("productionReady"), productionReady},
        {QStringLiteral("reusedExisting"), reusedExisting},
        {QStringLiteral("status"), status},
        {QStringLiteral("valid"), isValid()},
    };
}

ThemeAssetProcessingResult ThemeAssetProcessor::process(
    const ThemeAssetProcessingRequest &request)
{
    ThemeAssetProcessingResult result;
    static const QRegularExpression identifier(
        QStringLiteral("^[a-z0-9][a-z0-9._-]{0,63}$"));
    if (!identifier.match(request.assetId).hasMatch())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-id"), QStringLiteral("/assetId"),
            QStringLiteral("processor asset ID is invalid")));
        return result;
    }
    const QFileInfo sourceInfo(request.sourcePath);
    const QString sourcePath = sourceInfo.canonicalFilePath();
    if (!sourceInfo.exists())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("missing-asset"), QStringLiteral("/sourcePath"),
            QStringLiteral("processor source does not exist")));
        return result;
    }
    if (!sourceInfo.isFile() || sourcePath.isEmpty())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("asset-not-regular"), QStringLiteral("/sourcePath"),
            QStringLiteral("processor source is not a regular file")));
        return result;
    }
    if (sourceInfo.size() > MaximumSourceBytes)
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("asset-too-large"), QStringLiteral("/sourcePath"),
            QStringLiteral("processor source exceeds the byte limit")));
        return result;
    }
    result.inspection.sourceByteSize = sourceInfo.size();
    result.inspection.sourceSha256 = fileSha256(sourcePath);
    if (result.inspection.sourceSha256.isEmpty())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("asset-hash-mismatch"), QStringLiteral("/sourcePath"),
            QStringLiteral("processor source could not be hashed")));
        return result;
    }
    if (!request.expectedSourceSha256.isEmpty() &&
        request.expectedSourceSha256 != result.inspection.sourceSha256)
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("asset-hash-mismatch"),
            QStringLiteral("/expectedSourceSha256"),
            QStringLiteral("processor source hash differs from expectation")));
        return result;
    }

    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    const QSize declaredSize = reader.size();
    if (!reader.canRead() || !declaredSize.isValid())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("unsupported-asset-format"), QStringLiteral("/sourcePath"),
            QStringLiteral("Qt cannot decode this asset without external delegates")));
        return result;
    }
    if (declaredSize.width() > MaximumSourceDimension ||
        declaredSize.height() > MaximumSourceDimension ||
        qint64(declaredSize.width()) * declaredSize.height() > MaximumSourcePixels)
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("limit-exceeded"), QStringLiteral("/sourcePath"),
            QStringLiteral("source dimensions exceed the decode limit")));
        return result;
    }
    QImage image = reader.read();
    if (image.isNull() || image.size() != declaredSize)
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("unsupported-asset-format"), QStringLiteral("/sourcePath"),
            QStringLiteral("source image could not be decoded deterministically")));
        return result;
    }
    const bool sourceHasAlphaChannel = image.hasAlphaChannel();
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    result.inspection.sourceSize = image.size();
    result.inspection.hasAlphaChannel = sourceHasAlphaChannel;
    quint64 transparentPixels = 0;
    for (int y = 0; y < image.height(); ++y)
    {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x)
        {
            transparentPixels += qAlpha(line[x]) < 255;
        }
    }
    result.inspection.transparentPixelCount = transparentPixels;
    result.inspection.hasTransparentPixels = transparentPixels > 0;

    const QRect sourceBounds(QPoint(0, 0), image.size());
    QImage working = image;
    if (request.cropRect.has_value())
    {
        if (request.cropRect->width() <= 0 || request.cropRect->height() <= 0 ||
            !sourceBounds.contains(*request.cropRect))
        {
            result.diagnostics.append(processorDiagnostic(
                QStringLiteral("invalid-bounds"), QStringLiteral("/cropRect"),
                QStringLiteral("crop rectangle must fit inside the source image")));
            return result;
        }
        working = image.copy(*request.cropRect);
    }
    if (request.previewBounds.width() <= 0 || request.previewBounds.height() <= 0 ||
        request.previewBounds.width() > MaximumOutputDimension ||
        request.previewBounds.height() > MaximumOutputDimension ||
        qint64(request.previewBounds.width()) * request.previewBounds.height() >
            MaximumOutputPixels)
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-bounds"), QStringLiteral("/previewBounds"),
            QStringLiteral("preview bounds exceed processor limits")));
        return result;
    }
    if (!QSet<QString>{QStringLiteral("cover"), QStringLiteral("contain"),
                       QStringLiteral("stretch"), QStringLiteral("tile")}
             .contains(request.fit))
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-enum"), QStringLiteral("/fit"),
            QStringLiteral("unsupported processor fit mode")));
        return result;
    }
    if (request.layerInputs.size() > MaximumLayerInputs)
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("limit-exceeded"), QStringLiteral("/layerInputs"),
            QStringLiteral("too many layer extraction inputs")));
        return result;
    }
    QSet<QString> layerIds;
    for (qsizetype index = 0; index < request.layerInputs.size(); ++index)
    {
        const ThemeAssetLayerInput &layer = request.layerInputs.at(index);
        if (!identifier.match(layer.id).hasMatch())
        {
            result.diagnostics.append(processorDiagnostic(
                QStringLiteral("invalid-id"),
                QStringLiteral("/layerInputs/") + QString::number(index) +
                    QStringLiteral("/id"),
                QStringLiteral("layer extraction ID is invalid")));
            return result;
        }
        if (layerIds.contains(layer.id))
        {
            result.diagnostics.append(processorDiagnostic(
                QStringLiteral("duplicate-id"),
                QStringLiteral("/layerInputs/") + QString::number(index) +
                    QStringLiteral("/id"),
                QStringLiteral("layer extraction ID is duplicated")));
            return result;
        }
        layerIds.insert(layer.id);
        if (layer.sourceRect.width() <= 0 || layer.sourceRect.height() <= 0 ||
            !sourceBounds.contains(layer.sourceRect))
        {
            result.diagnostics.append(processorDiagnostic(
                QStringLiteral("invalid-bounds"),
                QStringLiteral("/layerInputs/") + QString::number(index) +
                    QStringLiteral("/sourceRect"),
                QStringLiteral("layer extraction rectangle exceeds source bounds")));
            return result;
        }
    }
    const QSet<QString> knownCleanup{
        QStringLiteral("none"), QStringLiteral("manual-review"),
        QStringLiteral("crop"), QStringLiteral("alpha-mask-review"),
        QStringLiteral("remove-placeholder"), QStringLiteral("remove-logo"),
        QStringLiteral("split-layers"), QStringLiteral("derive-state-pair"),
        QStringLiteral("isolate-clean-assets")};
    QSet<QString> seenCleanup;
    for (qsizetype index = 0; index < request.cleanupRequirements.size(); ++index)
    {
        const QString cleanup = request.cleanupRequirements.at(index);
        if (!knownCleanup.contains(cleanup))
        {
            result.diagnostics.append(processorDiagnostic(
                QStringLiteral("invalid-enum"),
                QStringLiteral("/cleanupRequirements/") + QString::number(index),
                QStringLiteral("unsupported cleanup requirement")));
            return result;
        }
        if (seenCleanup.contains(cleanup))
        {
            result.diagnostics.append(processorDiagnostic(
                QStringLiteral("duplicate-value"),
                QStringLiteral("/cleanupRequirements/") + QString::number(index),
                QStringLiteral("cleanup requirements must be unique")));
            return result;
        }
        seenCleanup.insert(cleanup);
    }
    result.manualReviewRequired = std::any_of(
        request.cleanupRequirements.cbegin(), request.cleanupRequirements.cend(),
        [](const QString &cleanup)
        {
            return cleanup != QStringLiteral("none") && cleanup != QStringLiteral("crop") &&
                cleanup != QStringLiteral("split-layers");
        });
    result.productionReady = !result.manualReviewRequired;
    result.status = result.manualReviewRequired
        ? QStringLiteral("pending-manual-review") : QStringLiteral("processed");

    const bool hasTarget = request.targetSize.isValid();
    if ((request.targetSize.width() > 0 || request.targetSize.height() > 0) && !hasTarget)
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-bounds"), QStringLiteral("/targetSize"),
            QStringLiteral("target size must have positive width and height")));
        return result;
    }
    if (hasTarget && (request.scaleFactors.isEmpty() ||
                      request.scaleFactors.size() > MaximumScaleFactors))
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("limit-exceeded"), QStringLiteral("/scaleFactors"),
            QStringLiteral("scale-factor count is outside processor limits")));
        return result;
    }
    QSet<int> scaleKeys;
    if (hasTarget)
    {
        for (qsizetype index = 0; index < request.scaleFactors.size(); ++index)
        {
            const qreal scale = request.scaleFactors.at(index);
            const int scaleKey = qRound(scale * 1000.0);
            const QSize outputSize(qRound(request.targetSize.width() * scale),
                                   qRound(request.targetSize.height() * scale));
            if (!std::isfinite(scale) || scale <= 0.0 || scale > 4.0 ||
                scaleKeys.contains(scaleKey) || outputSize.width() <= 0 ||
                outputSize.height() <= 0 ||
                outputSize.width() > MaximumOutputDimension ||
                outputSize.height() > MaximumOutputDimension ||
                qint64(outputSize.width()) * outputSize.height() > MaximumOutputPixels)
            {
                result.diagnostics.append(processorDiagnostic(
                    QStringLiteral("invalid-bounds"),
                    QStringLiteral("/scaleFactors/") + QString::number(index),
                    QStringLiteral("scale factor produces an invalid output size")));
                return result;
            }
            scaleKeys.insert(scaleKey);
        }
    }

    if (!QDir().mkpath(request.managedOutputRoot))
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("unsafe-package-root"),
            QStringLiteral("/managedOutputRoot"),
            QStringLiteral("managed processor root could not be created")));
        return result;
    }
    const QString managedRoot = QFileInfo(
        request.managedOutputRoot).canonicalFilePath();
    if (managedRoot.isEmpty() || !QFileInfo(managedRoot).isDir())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("unsafe-package-root"),
            QStringLiteral("/managedOutputRoot"),
            QStringLiteral("managed processor root is not canonical")));
        return result;
    }

    const QVariantMap settings = requestSettings(
        request, result.inspection.sourceSha256);
    result.contentKey = QString::fromLatin1(settingsDigest(settings).toHex());
    const QString finalDirectory = QDir(managedRoot).filePath(
        request.assetId + QLatin1Char('-') + result.contentKey.left(16));
    if (QFileInfo::exists(finalDirectory))
    {
        const auto existing = loadPublished(
            finalDirectory, result.contentKey, result.inspection);
        if (existing.has_value())
        {
            return *existing;
        }
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-value"), QStringLiteral("/managedOutputRoot"),
            QStringLiteral("existing derivative directory failed revalidation")));
        return result;
    }

    QTemporaryDir staging(QDir(managedRoot).filePath(
        QStringLiteral(".archdock-process-XXXXXX")));
    if (!staging.isValid())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-value"), QStringLiteral("/managedOutputRoot"),
            QStringLiteral("processor staging directory could not be created")));
        return result;
    }

    QSize previewSize = working.size();
    if (previewSize.width() > request.previewBounds.width() ||
        previewSize.height() > request.previewBounds.height())
    {
        previewSize.scale(
            request.previewBounds, Qt::KeepAspectRatio);
    }
    const QImage preview = previewSize == working.size()
        ? working
        : working.scaled(
              previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!appendDerivative(staging.path(), QStringLiteral("preview"),
                          QStringLiteral("preview"), QStringLiteral("preview.png"),
                          preview, 1.0, &result))
    {
        return result;
    }
    if (hasTarget)
    {
        for (qreal scale : request.scaleFactors)
        {
            const int scaleKey = qRound(scale * 1000.0);
            const QSize outputSize(qRound(request.targetSize.width() * scale),
                                   qRound(request.targetSize.height() * scale));
            if (!appendDerivative(
                    staging.path(), QStringLiteral("scale-%1").arg(scaleKey),
                    QStringLiteral("variant"),
                    QStringLiteral("variant-%1.png").arg(scaleKey),
                    fittedImage(working, outputSize, request.fit), scale, &result))
            {
                return result;
            }
        }
    }
    for (const ThemeAssetLayerInput &layer : request.layerInputs)
    {
        if (!appendDerivative(
                staging.path(), layer.id, QStringLiteral("layer"),
                QStringLiteral("layer-%1.png").arg(layer.id),
                image.copy(layer.sourceRect), 1.0, &result))
        {
            return result;
        }
    }

    const QVariantMap metadata = processingMetadata(request, result, settings);
    const QByteArray metadataBytes = QJsonDocument(
        QJsonObject::fromVariantMap(metadata)).toJson(QJsonDocument::Indented);
    const QString stagedMetadata = QDir(staging.path()).filePath(metadataFileName);
    QSaveFile metadataFile(stagedMetadata);
    if (!metadataFile.open(QIODevice::WriteOnly) ||
        metadataFile.write(metadataBytes) != metadataBytes.size() ||
        !metadataFile.commit())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-value"), QStringLiteral("/managedOutputRoot"),
            QStringLiteral("processor metadata could not be written")));
        return result;
    }
    if (fileSha256(sourcePath) != result.inspection.sourceSha256)
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("asset-hash-mismatch"), QStringLiteral("/sourcePath"),
            QStringLiteral("source changed while derivatives were generated")));
        return result;
    }
    if (!QDir().rename(staging.path(), finalDirectory))
    {
        const auto concurrent = loadPublished(
            finalDirectory, result.contentKey, result.inspection);
        if (concurrent.has_value())
        {
            return *concurrent;
        }
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("invalid-value"), QStringLiteral("/managedOutputRoot"),
            QStringLiteral("processor output could not be published atomically")));
        return result;
    }
    staging.setAutoRemove(false);
    const auto published = loadPublished(
        finalDirectory, result.contentKey, result.inspection);
    if (!published.has_value())
    {
        result.diagnostics.append(processorDiagnostic(
            QStringLiteral("asset-hash-mismatch"), QStringLiteral("/derivatives"),
            QStringLiteral("published processor output failed revalidation")));
        return result;
    }
    ThemeAssetProcessingResult publishedResult = *published;
    publishedResult.reusedExisting = false;
    return publishedResult;
}

}
