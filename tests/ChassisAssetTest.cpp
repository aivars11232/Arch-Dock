#include "themes/ThemePackage.h"

#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSet>
#include <QTest>

using ArchDock::ThemePackage;

namespace
{

struct ChassisSpec
{
    QString id;
    QString excludedReferenceId;
    QString iconStyleId;
};

const QList<ChassisSpec> chassisSpecs{
    {QStringLiteral("sci-fi-chassis-dark"),
     QStringLiteral("panel-screenshot-20260802-010007"),
     QStringLiteral("dark-orb")},
    {QStringLiteral("sci-fi-chassis-red"),
     QStringLiteral("panel-screenshot-20260802-010039"),
     QStringLiteral("metallic-red")},
    {QStringLiteral("sci-fi-chassis-blue"),
     QStringLiteral("panel-screenshot-20260802-010048"),
     QStringLiteral("metallic-blue")},
};

QString repositoryFile(const QString &relativePath)
{
    return QFINDTESTDATA(QStringLiteral("../") + relativePath);
}

QByteArray fileBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}

QString fileSha256(const QString &path)
{
    QFile file(path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return file.open(QIODevice::ReadOnly) && hash.addData(&file)
        ? QString::fromLatin1(hash.result().toHex())
        : QString{};
}

QJsonObject jsonObject(const QString &path)
{
    return QJsonDocument::fromJson(fileBytes(path)).object();
}

QImage decodedImage(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    return reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QRect alphaBounds(const QImage &image)
{
    int minimumX = image.width();
    int minimumY = image.height();
    int maximumX = -1;
    int maximumY = -1;
    for (int y = 0; y < image.height(); ++y)
    {
        for (int x = 0; x < image.width(); ++x)
        {
            if (qAlpha(image.pixel(x, y)) == 0)
            {
                continue;
            }
            minimumX = qMin(minimumX, x);
            minimumY = qMin(minimumY, y);
            maximumX = qMax(maximumX, x);
            maximumY = qMax(maximumY, y);
        }
    }
    return maximumX < minimumX || maximumY < minimumY
        ? QRect{} : QRect(QPoint(minimumX, minimumY), QPoint(maximumX, maximumY));
}

QImage renderFixedCaps(const QImage &surface, const QSize &outputSize)
{
    constexpr int naturalWidth = 1200;
    constexpr int naturalHeight = 160;
    constexpr int fixedCap = 152;
    constexpr int centerWidth = naturalWidth - fixedCap * 2;
    const int outputCap = qRound(
        static_cast<qreal>(fixedCap) * outputSize.height() / naturalHeight);
    const int outputCenter = outputSize.width() - outputCap * 2;
    if (surface.size() != QSize(naturalWidth, naturalHeight) || outputCenter <= 0)
    {
        return {};
    }

    QImage output(outputSize, QImage::Format_ARGB32_Premultiplied);
    output.fill(Qt::transparent);
    QPainter painter(&output);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(
        QRect(0, 0, outputCap, outputSize.height()), surface,
        QRect(0, 0, fixedCap, naturalHeight));
    painter.drawImage(
        QRect(outputCap, 0, outputCenter, outputSize.height()), surface,
        QRect(fixedCap, 0, centerWidth, naturalHeight));
    painter.drawImage(
        QRect(outputCap + outputCenter, 0, outputCap, outputSize.height()),
        surface, QRect(naturalWidth - fixedCap, 0, fixedCap, naturalHeight));
    return output;
}

}

class ChassisAssetTest final : public QObject
{
    Q_OBJECT

private slots:
    void cleanRoomRecipeExcludesReferencePixels();
    void productionPackagesValidateAndShareGeometry();
    void catalogDeclaresOriginalFamilyPackages();
    void vectorAssetsHaveCleanAlphaAndBounds();
    void fixedCapsStretchWithoutTransparentSeams();
};

void ChassisAssetTest::cleanRoomRecipeExcludesReferencePixels()
{
    const QJsonObject recipe = jsonObject(repositoryFile(
        QStringLiteral("assets/source-samples/chassis/original-artwork-recipe.json")));
    QCOMPARE(recipe.value(QStringLiteral("format")).toString(),
             QStringLiteral("org.archdock.clean-room-artwork-recipe"));
    QVERIFY(!recipe.value(QStringLiteral("pixelInput")).toBool(true));
    QVERIFY(!recipe.value(QStringLiteral("sourceDerivative")).toBool(true));
    const QJsonArray references = recipe.value(
        QStringLiteral("inspirationReferences")).toArray();
    QCOMPARE(references.size(), chassisSpecs.size());
    for (const QJsonValue &value : references)
    {
        const QJsonObject reference = value.toObject();
        QCOMPARE(reference.value(QStringLiteral("permittedUse")).toString(),
                 QStringLiteral("broad-concept-only"));
        QVERIFY(!reference.value(QStringLiteral("pixelInput")).toBool(true));
        QCOMPARE(reference.value(QStringLiteral("sha256")).toString().size(), 64);
    }

    const QJsonArray catalog = jsonObject(repositoryFile(
        QStringLiteral("data/source-assets/source-asset-catalog.json")))
                                   .value(QStringLiteral("assets"))
                                   .toArray();
    QSet<QString> expectedReferences;
    for (const ChassisSpec &spec : chassisSpecs)
    {
        expectedReferences.insert(spec.excludedReferenceId);
    }
    QSet<QString> observedReferences;
    for (const QJsonValue &value : catalog)
    {
        const QJsonObject record = value.toObject();
        const QString id = record.value(QStringLiteral("id")).toString();
        if (!expectedReferences.contains(id))
        {
            continue;
        }
        observedReferences.insert(id);
        QCOMPARE(record.value(QStringLiteral("status")).toString(),
                 QStringLiteral("reference-only"));
        QVERIFY(record.value(QStringLiteral("opaqueScreenshot")).toBool());
        QVERIFY(!record.value(QStringLiteral("isolatedCleanAsset")).toBool(true));
        const QJsonObject provenance = record.value(
            QStringLiteral("provenance")).toObject();
        QCOMPARE(provenance.value(QStringLiteral("licenseSpdx")).toString(),
                 QStringLiteral("NOASSERTION"));
        QCOMPARE(provenance.value(QStringLiteral("redistribution")).toString(),
                 QStringLiteral("unknown"));
    }
    QCOMPARE(observedReferences, expectedReferences);

    const QString themesRoot = repositoryFile(QStringLiteral(
        "assets/themes/sci-fi-chassis-dark/archdock-theme.json"));
    QVERIFY(!themesRoot.isEmpty());
    QDirIterator iterator(
        QFileInfo(themesRoot).absolutePath() + QStringLiteral("/.."),
        QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        const QString path = iterator.next();
        QVERIFY2(!QFileInfo(path).fileName().startsWith(
                     QStringLiteral("Screenshot_")), qPrintable(path));
        QVERIFY2(QFileInfo(path).suffix().compare(
                     QStringLiteral("png"), Qt::CaseInsensitive) != 0,
                 qPrintable(path));
    }
}

void ChassisAssetTest::productionPackagesValidateAndShareGeometry()
{
    QVariantList familySlices;
    QVariantList familyContentRegions;
    QVariantMap familyEffectMargins;
    QString commonMaskHash;
    QSet<QString> surfaceHashes;
    QSet<QString> glowHashes;

    for (const ChassisSpec &spec : chassisSpecs)
    {
        const QString packageRoot = QStringLiteral("assets/themes/") + spec.id;
        const QString manifestPath = repositoryFile(
            packageRoot + QStringLiteral("/archdock-theme.json"));
        QVERIFY2(!manifestPath.isEmpty(), qPrintable(spec.id));
        const auto loaded = ThemePackage::load(manifestPath);
        QVERIFY2(loaded.isValid(), qPrintable(loaded.primaryCode() +
                                              QStringLiteral(": ") +
                                              loaded.primaryMessage()));
        const auto &package = *loaded.package;
        const auto &definition = package.definition();
        QCOMPARE(definition.id, spec.id);
        QCOMPARE(definition.license.spdx, QStringLiteral("NOASSERTION"));
        QCOMPARE(definition.license.redistribution, QStringLiteral("allowed"));
        QCOMPARE(definition.capabilities.hosts,
                 QStringList({QStringLiteral("native-edge"),
                              QStringLiteral("free-desktop")}));
        QCOMPARE(definition.capabilities.orientations,
                 QStringList({QStringLiteral("horizontal")}));
        QCOMPARE(definition.capabilities.preferredRendererTier,
                 QStringLiteral("skinned2d"));
        QCOMPARE(definition.states.size(), 3);
        QCOMPARE(definition.slices.size(), 3);
        QCOMPARE(definition.contentRegions.size(), 3);
        QCOMPARE(definition.inputMasks.size(), 3);
        QVERIFY(definition.iconStyleRef.has_value());
        QCOMPARE(definition.iconStyleRef->id, spec.iconStyleId);
        QVERIFY(!package.primarySurfacePath().isEmpty());

        const QVariantMap projection = package.runtimeProjection();
        if (familySlices.isEmpty())
        {
            familySlices = projection.value(QStringLiteral("slices")).toList();
            familyContentRegions = projection.value(
                QStringLiteral("contentRegions")).toList();
            familyEffectMargins = projection.value(
                QStringLiteral("effectMargins")).toMap();
        }
        else
        {
            QCOMPARE(projection.value(QStringLiteral("slices")).toList(),
                     familySlices);
            QCOMPARE(projection.value(QStringLiteral("contentRegions")).toList(),
                     familyContentRegions);
            QCOMPARE(projection.value(QStringLiteral("effectMargins")).toMap(),
                     familyEffectMargins);
        }

        const QJsonObject production = jsonObject(repositoryFile(
            packageRoot + QStringLiteral("/metadata/production-record.json")));
        QVERIFY(!production.value(QStringLiteral("pixelInput")).toBool(true));
        QVERIFY(!production.value(QStringLiteral("sourceDerivative")).toBool(true));
        const QJsonArray excluded = production.value(
            QStringLiteral("excludedReferences")).toArray();
        QCOMPARE(excluded.size(), 1);
        QCOMPARE(excluded.first().toObject()
                     .value(QStringLiteral("catalogId")).toString(),
                 spec.excludedReferenceId);
        QVERIFY(!excluded.first().toObject()
                     .value(QStringLiteral("pixelInput")).toBool(true));
        const QJsonObject parameters = production.value(
            QStringLiteral("parameters")).toObject();
        QCOMPARE(parameters.value(QStringLiteral("fixedStart")).toInt(), 152);
        QCOMPARE(parameters.value(QStringLiteral("fixedEnd")).toInt(), 152);
        QCOMPARE(parameters.value(QStringLiteral("centerMode")).toString(),
                 QStringLiteral("stretch"));
        QCOMPARE(parameters.value(QStringLiteral("orientation")).toString(),
                 QStringLiteral("horizontal"));

        const QJsonArray outputs = production.value(
            QStringLiteral("outputs")).toArray();
        QCOMPARE(outputs.size(), 3);
        for (const QJsonValue &value : outputs)
        {
            const QJsonObject output = value.toObject();
            const QString relative = output.value(QStringLiteral("path")).toString();
            QCOMPARE(fileSha256(repositoryFile(packageRoot + QLatin1Char('/') + relative)),
                     output.value(QStringLiteral("sha256")).toString());
        }

        const QJsonObject review = jsonObject(repositoryFile(
            packageRoot + QStringLiteral("/metadata/visual-review.json")));
        QCOMPARE(review.value(QStringLiteral("status")).toString(),
                 QStringLiteral("production-approved"));
        QCOMPARE(review.value(QStringLiteral("contactSheetSha256")).toString(),
                 QStringLiteral("d63e2edf2e15186df0fa14e74d2758e9a3ed67270551fbb36745834fb574e9cc"));
        const QJsonObject checks = review.value(QStringLiteral("checks")).toObject();
        for (auto iterator = checks.constBegin();
             iterator != checks.constEnd(); ++iterator)
        {
            const QString result = iterator.value().toString();
            QVERIFY2(result == QStringLiteral("pass") ||
                         result == QStringLiteral("absent"),
                     qPrintable(spec.id + QLatin1Char(':') + iterator.key()));
        }

        const auto *surface = definition.assetById(QStringLiteral("surface"));
        const auto *glow = definition.assetById(QStringLiteral("glow"));
        const auto *mask = definition.assetById(QStringLiteral("input-mask"));
        QVERIFY(surface && glow && mask);
        surfaceHashes.insert(surface->sha256);
        glowHashes.insert(glow->sha256);
        if (commonMaskHash.isEmpty())
        {
            commonMaskHash = mask->sha256;
        }
        QCOMPARE(mask->sha256, commonMaskHash);
    }
    QCOMPARE(surfaceHashes.size(), chassisSpecs.size());
    QCOMPARE(glowHashes.size(), chassisSpecs.size());
}

void ChassisAssetTest::catalogDeclaresOriginalFamilyPackages()
{
    const QJsonArray themes = jsonObject(repositoryFile(
        QStringLiteral("data/themes/builtin-themes.json")))
                                  .value(QStringLiteral("themes"))
                                  .toArray();
    QCOMPARE(themes.size(), 16);

    QHash<QString, QJsonObject> chassisThemes;
    for (const QJsonValue &value : themes)
    {
        const QJsonObject theme = value.toObject();
        if (theme.value(QStringLiteral("category")).toString() ==
            QStringLiteral("chassis"))
        {
            chassisThemes.insert(
                theme.value(QStringLiteral("id")).toString(), theme);
        }
    }
    QCOMPARE(chassisThemes.size(), chassisSpecs.size());

    for (const ChassisSpec &spec : chassisSpecs)
    {
        QVERIFY2(chassisThemes.contains(spec.id), qPrintable(spec.id));
        const QJsonObject theme = chassisThemes.value(spec.id);
        QCOMPARE(theme.value(QStringLiteral("version")).toInt(), 2);
        QVERIFY(theme.value(QStringLiteral("builtIn")).toBool());
        QCOMPARE(theme.value(QStringLiteral("packageManifest")).toString(),
                 spec.id + QStringLiteral("/archdock-theme.json"));
        QCOMPARE(theme.value(QStringLiteral("iconStyleRef"))
                     .toObject()
                     .value(QStringLiteral("id"))
                     .toString(),
                 spec.iconStyleId);

        const QJsonObject capabilities = theme.value(
            QStringLiteral("capabilities")).toObject();
        QCOMPARE(capabilities.value(QStringLiteral("hosts")).toArray(),
                 QJsonArray({QStringLiteral("native-edge"),
                             QStringLiteral("free-desktop")}));
        QCOMPARE(capabilities.value(QStringLiteral("layouts")).toArray(),
                 QJsonArray({QStringLiteral("horizontal")}));
        QCOMPARE(capabilities.value(
                     QStringLiteral("orientations")).toArray(),
                 QJsonArray({QStringLiteral("horizontal")}));
        QCOMPARE(capabilities.value(
                     QStringLiteral("preferredRendererTier")).toString(),
                 QStringLiteral("skinned2d"));
        QVERIFY(!capabilities.value(QStringLiteral("features"))
                     .toArray()
                     .contains(QStringLiteral("nonrectangular-input")));

        const QJsonObject panelStyle = theme.value(
            QStringLiteral("panelStyle")).toObject();
        QCOMPARE(panelStyle.value(QStringLiteral("rendererTier")).toString(),
                 QStringLiteral("skinned2d"));
        QCOMPARE(theme.value(QStringLiteral("layoutStyle"))
                     .toObject()
                     .value(QStringLiteral("layout"))
                     .toString(),
                 QStringLiteral("horizontal"));
        const QJsonObject preview = theme.value(
            QStringLiteral("previewConfiguration")).toObject();
        QCOMPARE(preview.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("horizontal"));
        QCOMPARE(preview.value(QStringLiteral("presentationState")).toString(),
                 QStringLiteral("open"));
        QCOMPARE(preview.value(QStringLiteral("seed")).toString(),
                 QStringLiteral("chassis-family-v1"));
    }
}

void ChassisAssetTest::vectorAssetsHaveCleanAlphaAndBounds()
{
    for (const ChassisSpec &spec : chassisSpecs)
    {
        const QString root = QStringLiteral("assets/themes/") + spec.id;
        for (const QString &relative : {
                 QStringLiteral("assets/surface.svg"),
                 QStringLiteral("assets/glow.svg"),
                 QStringLiteral("masks/input.svg")})
        {
            const QString path = repositoryFile(root + QLatin1Char('/') + relative);
            const QByteArray markup = fileBytes(path).toLower();
            QVERIFY2(!markup.isEmpty(), qPrintable(path));
            QVERIFY2(!markup.contains("<text"), qPrintable(path));
            QVERIFY2(!markup.contains("<image"), qPrintable(path));
            QVERIFY2(!markup.contains("alienware"), qPrintable(path));
            QVERIFY2(!markup.contains("data:image"), qPrintable(path));

            const QImage image = decodedImage(path);
            QVERIFY2(!image.isNull(), qPrintable(path));
            QCOMPARE(image.size(), QSize(1200, 160));
            QVERIFY(image.hasAlphaChannel());
            QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
            QCOMPARE(qAlpha(image.pixel(1199, 159)), 0);
            QVERIFY(qAlpha(image.pixel(600, 80)) > 0 ||
                    relative == QStringLiteral("assets/glow.svg"));

            const QRect bounds = alphaBounds(image);
            QVERIFY2(bounds.isValid(), qPrintable(path));
            if (relative != QStringLiteral("assets/glow.svg"))
            {
                QVERIFY(bounds.width() >= 1188);
                QVERIFY(bounds.height() >= 148);
            }
        }
    }
}

void ChassisAssetTest::fixedCapsStretchWithoutTransparentSeams()
{
    const QList<QSize> sizes{
        QSize(352, 64), QSize(720, 96), QSize(1200, 160), QSize(1500, 200)};
    for (const ChassisSpec &spec : chassisSpecs)
    {
        const QImage surface = decodedImage(repositoryFile(
            QStringLiteral("assets/themes/") + spec.id +
            QStringLiteral("/assets/surface.svg")));
        QVERIFY(!surface.isNull());
        for (const QSize &size : sizes)
        {
            const QImage rendered = renderFixedCaps(surface, size);
            QVERIFY2(!rendered.isNull(), qPrintable(spec.id));
            QCOMPARE(rendered.size(), size);
            QCOMPARE(qAlpha(rendered.pixel(0, 0)), 0);
            QVERIFY(qAlpha(rendered.pixel(size.width() / 2,
                                          size.height() / 2)) > 240);

            const int cap = qRound(152.0 * size.height() / 160.0);
            const QList<int> seams{cap, size.width() - cap};
            for (const int seam : seams)
            {
                for (int y = qMax(1, size.height() / 10);
                     y < size.height() - qMax(1, size.height() / 10); ++y)
                {
                    QVERIFY2(qAlpha(rendered.pixel(seam - 1, y)) > 0,
                             qPrintable(spec.id));
                    QVERIFY2(qAlpha(rendered.pixel(seam, y)) > 0,
                             qPrintable(spec.id));
                }
            }
        }
    }
}

QTEST_GUILESS_MAIN(ChassisAssetTest)

#include "ChassisAssetTest.moc"
