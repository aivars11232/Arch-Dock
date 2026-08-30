#include "themes/ThemeAssetProcessor.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>
#include <QTest>

using ArchDock::ThemeAssetDerivative;
using ArchDock::ThemeAssetLayerInput;
using ArchDock::ThemeAssetProcessingRequest;
using ArchDock::ThemeAssetProcessingResult;
using ArchDock::ThemeAssetProcessor;

namespace
{

QByteArray fileBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QString sha256(const QByteArray &bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QString writeImage(const QString &directory,
                   const QString &name,
                   QImage::Format format,
                   bool transparentPixel = false)
{
    QImage image(QSize(80, 40), format);
    image.fill(format == QImage::Format_RGB32
                   ? qRgb(32, 96, 160) : qRgba(32, 96, 160, 255));
    if (transparentPixel)
    {
        image.setPixel(7, 9, qRgba(32, 96, 160, 0));
    }
    const QString path = QDir(directory).filePath(name);
    if (!image.save(path, "PNG"))
    {
        return {};
    }
    return path;
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) &&
        file.write(bytes) == bytes.size() && file.flush();
}

ThemeAssetProcessingRequest baseRequest(const QString &sourcePath,
                                        const QString &outputRoot,
                                        const QString &assetId)
{
    ThemeAssetProcessingRequest request;
    request.sourcePath = sourcePath;
    request.managedOutputRoot = outputRoot;
    request.assetId = assetId;
    request.previewBounds = QSize(32, 24);
    request.cleanupRequirements = {QStringLiteral("none")};
    return request;
}

const ThemeAssetDerivative *derivativeById(
    const ThemeAssetProcessingResult &result,
    const QString &id)
{
    const auto match = std::find_if(
        result.derivatives.cbegin(), result.derivatives.cend(),
        [&id](const ThemeAssetDerivative &derivative)
        {
            return derivative.id == id;
        });
    return match == result.derivatives.cend() ? nullptr : &*match;
}

bool isContainedPath(const QString &root, const QString &candidate)
{
    return candidate == root || candidate.startsWith(root + QLatin1Char('/'));
}

}

class ThemeAssetProcessorTest final : public QObject
{
    Q_OBJECT

private slots:
    void distinguishesAlphaChannelFromTransparency();
    void producesBoundedCropLayerAndScaleDerivatives();
    void isDeterministicAndReusesValidatedOutput();
    void rejectsTamperedPublishedOutput();
    void preservesManualCleanupRequirements();
    void rejectsUnsafeAndOutOfBoundsRequests();
    void rejectsUnsupportedFormatsWithoutExternalExecution();
};

void ThemeAssetProcessorTest::distinguishesAlphaChannelFromTransparency()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString rgbPath = writeImage(
        temporary.path(), QStringLiteral("rgb.png"), QImage::Format_RGB32);
    const QString opaqueAlphaPath = writeImage(
        temporary.path(), QStringLiteral("opaque-alpha.png"),
        QImage::Format_ARGB32);
    const QString transparentPath = writeImage(
        temporary.path(), QStringLiteral("transparent.png"),
        QImage::Format_ARGB32, true);
    QVERIFY(!rgbPath.isEmpty());
    QVERIFY(!opaqueAlphaPath.isEmpty());
    QVERIFY(!transparentPath.isEmpty());

    const auto rgb = ThemeAssetProcessor::process(baseRequest(
        rgbPath, QDir(temporary.path()).filePath(QStringLiteral("rgb-output")),
        QStringLiteral("rgb")));
    QVERIFY2(rgb.isValid(), qPrintable(rgb.primaryCode()));
    QVERIFY(!rgb.inspection.hasAlphaChannel);
    QVERIFY(!rgb.inspection.hasTransparentPixels);
    QCOMPARE(rgb.inspection.transparentPixelCount, quint64(0));

    const auto opaqueAlpha = ThemeAssetProcessor::process(baseRequest(
        opaqueAlphaPath,
        QDir(temporary.path()).filePath(QStringLiteral("opaque-output")),
        QStringLiteral("opaque-alpha")));
    QVERIFY2(opaqueAlpha.isValid(), qPrintable(opaqueAlpha.primaryCode()));
    QVERIFY(opaqueAlpha.inspection.hasAlphaChannel);
    QVERIFY(!opaqueAlpha.inspection.hasTransparentPixels);
    QCOMPARE(opaqueAlpha.inspection.transparentPixelCount, quint64(0));

    const auto transparent = ThemeAssetProcessor::process(baseRequest(
        transparentPath,
        QDir(temporary.path()).filePath(QStringLiteral("transparent-output")),
        QStringLiteral("transparent")));
    QVERIFY2(transparent.isValid(), qPrintable(transparent.primaryCode()));
    QVERIFY(transparent.inspection.hasAlphaChannel);
    QVERIFY(transparent.inspection.hasTransparentPixels);
    QCOMPARE(transparent.inspection.transparentPixelCount, quint64(1));
}

void ThemeAssetProcessorTest::producesBoundedCropLayerAndScaleDerivatives()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString sourcePath = writeImage(
        temporary.path(), QStringLiteral("source.png"), QImage::Format_ARGB32,
        true);
    QVERIFY(!sourcePath.isEmpty());

    auto request = baseRequest(
        sourcePath, QDir(temporary.path()).filePath(QStringLiteral("output")),
        QStringLiteral("derived"));
    request.cropRect = QRect(10, 5, 40, 20);
    request.previewBounds = QSize(16, 16);
    request.targetSize = QSize(40, 20);
    request.scaleFactors = {1.0, 2.0};
    request.fit = QStringLiteral("cover");
    request.layerInputs = {
        ThemeAssetLayerInput{QStringLiteral("left-layer"), QRect(0, 0, 10, 12)},
        ThemeAssetLayerInput{QStringLiteral("right-layer"), QRect(70, 28, 10, 12)},
    };
    request.cleanupRequirements = {
        QStringLiteral("crop"), QStringLiteral("split-layers")};

    const auto result = ThemeAssetProcessor::process(request);
    QVERIFY2(result.isValid(), qPrintable(result.primaryCode()));
    QVERIFY(result.productionReady);
    QVERIFY(!result.manualReviewRequired);
    QCOMPARE(result.status, QStringLiteral("processed"));
    QCOMPARE(result.derivatives.size(), 5);

    const auto *preview = derivativeById(result, QStringLiteral("preview"));
    const auto *one = derivativeById(result, QStringLiteral("scale-1000"));
    const auto *two = derivativeById(result, QStringLiteral("scale-2000"));
    const auto *left = derivativeById(result, QStringLiteral("left-layer"));
    const auto *right = derivativeById(result, QStringLiteral("right-layer"));
    QVERIFY(preview);
    QVERIFY(one);
    QVERIFY(two);
    QVERIFY(left);
    QVERIFY(right);
    QCOMPARE(preview->pixelSize, QSize(16, 8));
    QCOMPARE(one->pixelSize, QSize(40, 20));
    QCOMPARE(two->pixelSize, QSize(80, 40));
    QCOMPARE(left->pixelSize, QSize(10, 12));
    QCOMPARE(right->pixelSize, QSize(10, 12));

    const QString canonicalRoot = QFileInfo(request.managedOutputRoot)
                                      .canonicalFilePath();
    QVERIFY(!canonicalRoot.isEmpty());
    for (const ThemeAssetDerivative &derivative : result.derivatives)
    {
        QVERIFY(QFileInfo(derivative.absolutePath).isFile());
        QVERIFY(isContainedPath(canonicalRoot, derivative.absolutePath));
        QVERIFY(!derivative.relativePath.contains(QStringLiteral("..")));
    }
    QVERIFY(isContainedPath(canonicalRoot,
                            QFileInfo(result.metadataPath).canonicalFilePath()));
}

void ThemeAssetProcessorTest::isDeterministicAndReusesValidatedOutput()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString sourcePath = writeImage(
        temporary.path(), QStringLiteral("source.png"), QImage::Format_ARGB32,
        true);
    QVERIFY(!sourcePath.isEmpty());
    const QByteArray before = fileBytes(sourcePath);
    QVERIFY(!before.isEmpty());

    auto firstRequest = baseRequest(
        sourcePath, QDir(temporary.path()).filePath(QStringLiteral("first")),
        QStringLiteral("stable"));
    firstRequest.targetSize = QSize(64, 20);
    firstRequest.scaleFactors = {1.0, 1.5};
    firstRequest.expectedSourceSha256 = sha256(before);
    const auto first = ThemeAssetProcessor::process(firstRequest);
    QVERIFY2(first.isValid(), qPrintable(first.primaryCode()));
    QVERIFY(!first.reusedExisting);

    auto secondRequest = firstRequest;
    secondRequest.managedOutputRoot = QDir(temporary.path()).filePath(
        QStringLiteral("second"));
    const auto second = ThemeAssetProcessor::process(secondRequest);
    QVERIFY2(second.isValid(), qPrintable(second.primaryCode()));
    QCOMPARE(second.contentKey, first.contentKey);
    QCOMPARE(second.metadataSha256, first.metadataSha256);
    QCOMPARE(second.derivatives.size(), first.derivatives.size());
    for (qsizetype index = 0; index < first.derivatives.size(); ++index)
    {
        QCOMPARE(second.derivatives.at(index).id, first.derivatives.at(index).id);
        QCOMPARE(second.derivatives.at(index).relativePath,
                 first.derivatives.at(index).relativePath);
        QCOMPARE(second.derivatives.at(index).sha256,
                 first.derivatives.at(index).sha256);
        QCOMPARE(second.derivatives.at(index).pixelSize,
                 first.derivatives.at(index).pixelSize);
    }

    const auto reused = ThemeAssetProcessor::process(firstRequest);
    QVERIFY2(reused.isValid(), qPrintable(reused.primaryCode()));
    QVERIFY(reused.reusedExisting);
    QCOMPARE(reused.outputDirectory, first.outputDirectory);
    QCOMPARE(reused.metadataSha256, first.metadataSha256);
    QCOMPARE(fileBytes(sourcePath), before);
}

void ThemeAssetProcessorTest::rejectsTamperedPublishedOutput()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString sourcePath = writeImage(
        temporary.path(), QStringLiteral("source.png"), QImage::Format_ARGB32);
    auto request = baseRequest(
        sourcePath, QDir(temporary.path()).filePath(QStringLiteral("output")),
        QStringLiteral("tamper-test"));
    const auto first = ThemeAssetProcessor::process(request);
    QVERIFY2(first.isValid(), qPrintable(first.primaryCode()));
    QVERIFY(!first.derivatives.isEmpty());
    QVERIFY(writeBytes(first.derivatives.constFirst().absolutePath,
                       QByteArrayLiteral("tampered")));

    const auto second = ThemeAssetProcessor::process(request);
    QVERIFY(!second.isValid());
    QCOMPARE(second.primaryCode(), QStringLiteral("invalid-value"));
    QCOMPARE(fileBytes(first.derivatives.constFirst().absolutePath),
             QByteArrayLiteral("tampered"));
}

void ThemeAssetProcessorTest::preservesManualCleanupRequirements()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString sourcePath = writeImage(
        temporary.path(), QStringLiteral("source.png"), QImage::Format_ARGB32);
    const QByteArray before = fileBytes(sourcePath);
    auto request = baseRequest(
        sourcePath, QDir(temporary.path()).filePath(QStringLiteral("output")),
        QStringLiteral("manual"));
    request.cleanupRequirements = {
        QStringLiteral("remove-logo"),
        QStringLiteral("remove-placeholder"),
        QStringLiteral("alpha-mask-review"),
    };

    const auto result = ThemeAssetProcessor::process(request);
    QVERIFY2(result.isValid(), qPrintable(result.primaryCode()));
    QVERIFY(result.manualReviewRequired);
    QVERIFY(!result.productionReady);
    QCOMPARE(result.status, QStringLiteral("pending-manual-review"));
    QCOMPARE(fileBytes(sourcePath), before);

    const QByteArray metadata = fileBytes(result.metadataPath);
    QVERIFY(metadata.contains("remove-logo"));
    QVERIFY(metadata.contains("remove-placeholder"));
    QVERIFY(metadata.contains("pending-manual-review"));
}

void ThemeAssetProcessorTest::rejectsUnsafeAndOutOfBoundsRequests()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString sourcePath = writeImage(
        temporary.path(), QStringLiteral("source.png"), QImage::Format_RGB32);

    auto unsafeId = baseRequest(
        sourcePath, QDir(temporary.path()).filePath(QStringLiteral("unsafe")),
        QStringLiteral("../outside"));
    auto result = ThemeAssetProcessor::process(unsafeId);
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("invalid-id"));

    auto crop = baseRequest(
        sourcePath, QDir(temporary.path()).filePath(QStringLiteral("crop")),
        QStringLiteral("crop"));
    crop.cropRect = QRect(79, 39, 2, 2);
    result = ThemeAssetProcessor::process(crop);
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("invalid-bounds"));

    auto scale = baseRequest(
        sourcePath, QDir(temporary.path()).filePath(QStringLiteral("scale")),
        QStringLiteral("scale"));
    scale.targetSize = QSize(ThemeAssetProcessor::MaximumOutputDimension + 1, 2);
    scale.scaleFactors = {1.0};
    result = ThemeAssetProcessor::process(scale);
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("invalid-bounds"));

    auto wrongHash = baseRequest(
        sourcePath, QDir(temporary.path()).filePath(QStringLiteral("hash")),
        QStringLiteral("hash"));
    wrongHash.expectedSourceSha256 = QString(64, QLatin1Char('0'));
    result = ThemeAssetProcessor::process(wrongHash);
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("asset-hash-mismatch"));
}

void ThemeAssetProcessorTest::rejectsUnsupportedFormatsWithoutExternalExecution()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString blendPath = QDir(temporary.path()).filePath(
        QStringLiteral("scene.blend"));
    const QString textPath = QDir(temporary.path()).filePath(
        QStringLiteral("notes.txt"));
    QVERIFY(writeBytes(blendPath, QByteArrayLiteral("BLENDER-v300")));
    QVERIFY(writeBytes(textPath, QByteArrayLiteral("not an image")));

    for (const QString &sourcePath : {blendPath, textPath})
    {
        const auto result = ThemeAssetProcessor::process(baseRequest(
            sourcePath,
            QDir(temporary.path()).filePath(
                QFileInfo(sourcePath).completeBaseName() + QStringLiteral("-out")),
            QFileInfo(sourcePath).completeBaseName()));
        QVERIFY(!result.isValid());
        QCOMPARE(result.primaryCode(),
                 QStringLiteral("unsupported-asset-format"));
        QVERIFY(result.derivatives.isEmpty());
    }
}

QTEST_GUILESS_MAIN(ThemeAssetProcessorTest)

#include "ThemeAssetProcessorTest.moc"
