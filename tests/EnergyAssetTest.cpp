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

struct EnergySpec
{
    QString id;
    QString referenceId;
    QString referenceHash;
    QString tint;
    QString iconStyleId;
};

const QList<EnergySpec> energySpecs{
    {QStringLiteral("energy-frame-cyan"),
     QStringLiteral("panel-screenshot-20260811-193105"),
     QStringLiteral("ce30234a578093e5a943c6c2045ff8025c952da4cc08ec2568d2ac445397b427"),
     QStringLiteral("#44ddea"),
     QStringLiteral("metallic-blue")},
    {QStringLiteral("energy-frame-green"),
     QStringLiteral("panel-screenshot-20260811-192733"),
     QStringLiteral("638662753e315b59ffe295d640699c207bf834d233a82422497ef48794596a2c"),
     QStringLiteral("#4ee68a"),
     QStringLiteral("neon-green")},
    {QStringLiteral("energy-frame-orange"),
     QStringLiteral("panel-screenshot-20260811-192943"),
     QStringLiteral("a9aab4f4612cf4f1ec7bee06a1c99b6e7fec8a7698f978f62e8979b229ed9588"),
     QStringLiteral("#ff873c"),
     QStringLiteral("neon-orange")},
    {QStringLiteral("energy-frame-purple"),
     QStringLiteral("panel-screenshot-20260811-193642"),
     QStringLiteral("bd3268a2607f04a10bff2a606817ae67c8800bc43006ece749e6ca88f51d0e57"),
     QStringLiteral("#b96cff"),
     QStringLiteral("dark-orb")},
};

const QStringList productionSvgPaths{
    QStringLiteral("assets/surface.svg"),
    QStringLiteral("assets/frame-mask.svg"),
    QStringLiteral("assets/glow-mask.svg"),
    QStringLiteral("assets/energy-overlay-mask.svg"),
    QStringLiteral("assets/highlight-mask.svg"),
    QStringLiteral("masks/input.svg"),
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
    painter.drawImage(QRect(0, 0, outputCap, outputSize.height()), surface,
                      QRect(0, 0, fixedCap, naturalHeight));
    painter.drawImage(QRect(outputCap, 0, outputCenter, outputSize.height()), surface,
                      QRect(fixedCap, 0, centerWidth, naturalHeight));
    painter.drawImage(QRect(outputCap + outputCenter, 0, outputCap, outputSize.height()),
                      surface,
                      QRect(naturalWidth - fixedCap, 0, fixedCap, naturalHeight));
    return output;
}

}

class EnergyAssetTest final : public QObject
{
    Q_OBJECT

private slots:
    void cleanRoomRecipeMatchesReferenceCatalog();
    void productionPackagesValidateAndShareGeometry();
    void vectorLayersHaveCleanAlphaAndNeutralMasks();
    void fixedCapsRenderAtRequiredScales();
    void visualApprovalAndOutputHashesAreRecorded();
    void energyFamilyIsCataloguedWithRuntimeCapabilities();
};

void EnergyAssetTest::cleanRoomRecipeMatchesReferenceCatalog()
{
    const QJsonObject recipe = jsonObject(repositoryFile(
        QStringLiteral("assets/source-samples/energy/original-artwork-recipe.json")));
    QCOMPARE(recipe.value(QStringLiteral("format")).toString(),
             QStringLiteral("org.archdock.clean-room-artwork-recipe"));
    QCOMPARE(recipe.value(QStringLiteral("task")).toString(),
             QStringLiteral("TASK-0028"));
    QVERIFY(!recipe.value(QStringLiteral("pixelInput")).toBool(true));
    QVERIFY(!recipe.value(QStringLiteral("sourceDerivative")).toBool(true));

    QHash<QString, EnergySpec> expected;
    QSet<QString> expectedIds;
    for (const EnergySpec &spec : energySpecs)
    {
        expected.insert(spec.referenceId, spec);
        expectedIds.insert(spec.referenceId);
    }
    const QJsonArray references = recipe.value(
        QStringLiteral("inspirationReferences")).toArray();
    QCOMPARE(references.size(), energySpecs.size());
    for (const QJsonValue &value : references)
    {
        const QJsonObject reference = value.toObject();
        const QString id = reference.value(QStringLiteral("catalogId")).toString();
        QVERIFY2(expected.contains(id), qPrintable(id));
        QCOMPARE(reference.value(QStringLiteral("sha256")).toString(),
                 expected.value(id).referenceHash);
        QCOMPARE(reference.value(QStringLiteral("variant")).toString(),
                 expected.value(id).id);
        QCOMPARE(reference.value(QStringLiteral("permittedUse")).toString(),
                 QStringLiteral("broad-concept-only"));
        QVERIFY(!reference.value(QStringLiteral("pixelInput")).toBool(true));
    }

    QSet<QString> observed;
    const QJsonArray catalog = jsonObject(repositoryFile(
        QStringLiteral("data/source-assets/source-asset-catalog.json")))
                                   .value(QStringLiteral("assets"))
                                   .toArray();
    for (const QJsonValue &value : catalog)
    {
        const QJsonObject record = value.toObject();
        const QString id = record.value(QStringLiteral("id")).toString();
        if (!expected.contains(id))
        {
            continue;
        }
        observed.insert(id);
        QCOMPARE(record.value(QStringLiteral("sha256")).toString(),
                 expected.value(id).referenceHash);
        QCOMPARE(record.value(QStringLiteral("status")).toString(),
                 QStringLiteral("reference-only"));
        QCOMPARE(record.value(QStringLiteral("assetClass")).toString(),
                 QStringLiteral("D-energy-glow-frame-candidate"));
        QVERIFY(record.value(QStringLiteral("opaqueScreenshot")).toBool());
        QVERIFY(!record.value(QStringLiteral("isolatedCleanAsset")).toBool(true));
        QCOMPARE(record.value(QStringLiteral("provenance")).toObject()
                     .value(QStringLiteral("redistribution")).toString(),
                 QStringLiteral("unknown"));
    }
    QCOMPARE(observed, expectedIds);
}

void EnergyAssetTest::productionPackagesValidateAndShareGeometry()
{
    QVariantList familySlices;
    QVariantList familyContentRegions;
    QVariantList familyInputMasks;
    QVariantMap familyEffectMargins;
    QSet<QString> surfaceHashes;

    for (const EnergySpec &spec : energySpecs)
    {
        const QString root = QStringLiteral("assets/themes/") + spec.id;
        const auto loaded = ThemePackage::load(repositoryFile(
            root + QStringLiteral("/archdock-theme.json")));
        QVERIFY2(loaded.isValid(), qPrintable(
            loaded.primaryCode() + QStringLiteral(": ") + loaded.primaryMessage()));
        const auto &definition = loaded.package->definition();
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
        QVERIFY(definition.capabilities.features.contains(
            QStringLiteral("dynamic-tint")));
        QCOMPARE(definition.states.size(), 4);
        QCOMPARE(definition.layers.size(), 13);
        QCOMPARE(definition.slices.size(), 4);
        QCOMPARE(definition.contentRegions.size(), 4);
        QCOMPARE(definition.inputMasks.size(), 4);
        QCOMPARE(definition.effectMargins.left, 18.0);
        QCOMPARE(definition.effectMargins.top, 16.0);
        QCOMPARE(definition.effectMargins.right, 18.0);
        QCOMPARE(definition.effectMargins.bottom, 16.0);
        QVERIFY(!loaded.package->primarySurfacePath().isEmpty());

        const QVariantMap projection = loaded.package->runtimeProjection();
        if (familySlices.isEmpty())
        {
            familySlices = projection.value(QStringLiteral("slices")).toList();
            familyContentRegions = projection.value(
                QStringLiteral("contentRegions")).toList();
            familyInputMasks = projection.value(QStringLiteral("inputMasks")).toList();
            familyEffectMargins = projection.value(
                QStringLiteral("effectMargins")).toMap();
        }
        else
        {
            QCOMPARE(projection.value(QStringLiteral("slices")).toList(), familySlices);
            QCOMPARE(projection.value(QStringLiteral("contentRegions")).toList(),
                     familyContentRegions);
            QCOMPARE(projection.value(QStringLiteral("inputMasks")).toList(),
                     familyInputMasks);
            QCOMPARE(projection.value(QStringLiteral("effectMargins")).toMap(),
                     familyEffectMargins);
        }
        surfaceHashes.insert(definition.assetById(QStringLiteral("surface"))->sha256);
    }
    QCOMPARE(surfaceHashes.size(), energySpecs.size());
}

void EnergyAssetTest::vectorLayersHaveCleanAlphaAndNeutralMasks()
{
    for (const EnergySpec &spec : energySpecs)
    {
        const QString root = QStringLiteral("assets/themes/") + spec.id;
        QDirIterator iterator(QFileInfo(repositoryFile(
            root + QStringLiteral("/archdock-theme.json"))).absolutePath(),
            QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext())
        {
            const QFileInfo info(iterator.next());
            QVERIFY2(info.suffix().compare(QStringLiteral("png"),
                                           Qt::CaseInsensitive) != 0,
                     qPrintable(info.filePath()));
            QVERIFY2(!info.fileName().startsWith(QStringLiteral("Screenshot_")),
                     qPrintable(info.filePath()));
        }

        for (const QString &relative : productionSvgPaths)
        {
            const QString path = repositoryFile(root + QLatin1Char('/') + relative);
            const QByteArray markup = fileBytes(path).toLower();
            QVERIFY2(!markup.isEmpty(), qPrintable(path));
            QVERIFY2(!markup.contains("<text"), qPrintable(path));
            QVERIFY2(!markup.contains("<image"), qPrintable(path));
            QVERIFY2(!markup.contains("data:image"), qPrintable(path));
            QVERIFY2(!markup.contains("<filter"), qPrintable(path));
            QVERIFY2(!markup.contains("foreignobject"), qPrintable(path));

            const QImage image = decodedImage(path);
            QVERIFY2(!image.isNull(), qPrintable(path));
            QCOMPARE(image.size(), QSize(1200, 160));
            QVERIFY(image.hasAlphaChannel());
            QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
            QCOMPARE(qAlpha(image.pixel(1199, 159)), 0);
            QVERIFY2(alphaBounds(image).isValid(), qPrintable(path));

            if (relative != QStringLiteral("assets/surface.svg"))
            {
                for (int y = 0; y < image.height(); ++y)
                {
                    for (int x = 0; x < image.width(); ++x)
                    {
                        const QRgb pixel = image.pixel(x, y);
                        if (qAlpha(pixel) == 0)
                        {
                            continue;
                        }
                        QVERIFY2(qRed(pixel) == qGreen(pixel) &&
                                     qGreen(pixel) == qBlue(pixel),
                                 qPrintable(path));
                    }
                }
            }
        }

        const QImage surface = decodedImage(repositoryFile(
            root + QStringLiteral("/assets/surface.svg")));
        const QImage input = decodedImage(repositoryFile(
            root + QStringLiteral("/masks/input.svg")));
        QVERIFY(qAlpha(surface.pixel(600, 80)) > 240);
        QVERIFY(qAlpha(input.pixel(600, 80)) > 240);
        QCOMPARE(qAlpha(input.pixel(0, 80)), 0);
    }
}

void EnergyAssetTest::fixedCapsRenderAtRequiredScales()
{
    const QList<QSize> sizes{
        QSize(352, 64), QSize(720, 96), QSize(1200, 160), QSize(1500, 200)};
    for (const EnergySpec &spec : energySpecs)
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
            for (const int seam : {cap, size.width() - cap})
            {
                for (int y = qMax(4, size.height() / 8);
                     y < size.height() - qMax(4, size.height() / 8); ++y)
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

void EnergyAssetTest::visualApprovalAndOutputHashesAreRecorded()
{
    const QString expectedContactSheet = QStringLiteral(
        "0a9bc4a12cbfe59e858e37c50ae36e65783f34b7ac2ef4ba83c2ab8f49459ea7");
    for (const EnergySpec &spec : energySpecs)
    {
        const QString root = QStringLiteral("assets/themes/") + spec.id;
        const QJsonObject production = jsonObject(repositoryFile(
            root + QStringLiteral("/metadata/production-record.json")));
        QVERIFY(!production.value(QStringLiteral("pixelInput")).toBool(true));
        QVERIFY(!production.value(QStringLiteral("sourceDerivative")).toBool(true));
        QCOMPARE(production.value(QStringLiteral("outputs")).toArray().size(),
                 productionSvgPaths.size());
        for (const QJsonValue &value : production.value(
                 QStringLiteral("outputs")).toArray())
        {
            const QJsonObject output = value.toObject();
            QCOMPARE(fileSha256(repositoryFile(
                         root + QLatin1Char('/') +
                         output.value(QStringLiteral("path")).toString())),
                     output.value(QStringLiteral("sha256")).toString());
        }
        const QJsonObject parameters = production.value(
            QStringLiteral("parameters")).toObject();
        QCOMPARE(parameters.value(QStringLiteral("fixedStart")).toInt(), 152);
        QCOMPARE(parameters.value(QStringLiteral("fixedEnd")).toInt(), 152);
        QCOMPARE(parameters.value(QStringLiteral("centerMode")).toString(),
                 QStringLiteral("stretch"));
        QCOMPARE(parameters.value(QStringLiteral("tintableMasks")).toArray().size(), 4);

        const QJsonObject review = jsonObject(repositoryFile(
            root + QStringLiteral("/metadata/visual-review.json")));
        QCOMPARE(review.value(QStringLiteral("status")).toString(),
                 QStringLiteral("production-approved"));
        QCOMPARE(review.value(QStringLiteral("contactSheetSha256")).toString(),
                 expectedContactSheet);
        QCOMPARE(review.value(QStringLiteral("contactSheetSizes")).toArray().size(), 4);
        const QJsonObject checks = review.value(QStringLiteral("checks")).toObject();
        for (auto iterator = checks.constBegin(); iterator != checks.constEnd();
             ++iterator)
        {
            const QString result = iterator.value().toString();
            QVERIFY2(result == QStringLiteral("pass") ||
                         result == QStringLiteral("absent"),
                     qPrintable(spec.id + QLatin1Char(':') + iterator.key()));
        }
    }
}

void EnergyAssetTest::energyFamilyIsCataloguedWithRuntimeCapabilities()
{
    const QJsonArray themes = jsonObject(repositoryFile(
        QStringLiteral("data/themes/builtin-themes.json")))
                                  .value(QStringLiteral("themes"))
                                  .toArray();
    QCOMPARE(themes.size(), 12);

    int observedEnergyThemes = 0;
    for (const EnergySpec &spec : energySpecs)
    {
        const QJsonObject package = jsonObject(repositoryFile(
            QStringLiteral("assets/themes/") + spec.id +
            QStringLiteral("/archdock-theme.json")));
        const QJsonObject packageCapabilities = package.value(
            QStringLiteral("capabilities")).toObject();
        QVERIFY(packageCapabilities.value(QStringLiteral("features")).toArray()
                    .contains(QStringLiteral("dynamic-glow")));

        QJsonObject catalogTheme;
        for (const QJsonValue &value : themes)
        {
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("id")).toString() == spec.id)
            {
                catalogTheme = candidate;
                break;
            }
        }
        QVERIFY2(!catalogTheme.isEmpty(), qPrintable(spec.id));
        ++observedEnergyThemes;
        QCOMPARE(catalogTheme.value(QStringLiteral("category")).toString(),
                 QStringLiteral("energy"));
        QCOMPARE(catalogTheme.value(QStringLiteral("packageManifest")).toString(),
                 spec.id + QStringLiteral("/archdock-theme.json"));
        QCOMPARE(catalogTheme.value(QStringLiteral("capabilities")).toObject(),
                 packageCapabilities);
        QCOMPARE(catalogTheme.value(QStringLiteral("iconStyleRef"))
                     .toObject()
                     .value(QStringLiteral("id"))
                     .toString(),
                 spec.iconStyleId);

        const QJsonObject panelStyle = catalogTheme.value(
            QStringLiteral("panelStyle")).toObject();
        QCOMPARE(panelStyle.value(QStringLiteral("rendererTier")).toString(),
                 QStringLiteral("skinned2d"));
        QCOMPARE(panelStyle.value(QStringLiteral("color")).toString(), spec.tint);
        QCOMPARE(panelStyle.value(QStringLiteral("glowIntensity")).toDouble(),
                 1.15);

        const QJsonObject preview = catalogTheme.value(
            QStringLiteral("previewConfiguration")).toObject();
        QVERIFY(preview.value(QStringLiteral("active")).toBool());
        QCOMPARE(preview.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("horizontal"));
        QCOMPARE(preview.value(QStringLiteral("presentationState")).toString(),
                 QStringLiteral("open"));
        QCOMPARE(preview.value(QStringLiteral("stateEntry")).toInt(), 1);
        QCOMPARE(preview.value(QStringLiteral("iconState")).toString(),
                 QStringLiteral("hover"));
        QCOMPARE(preview.value(QStringLiteral("seed")).toString(),
                 QStringLiteral("energy-family-v1-") +
                     spec.id.mid(QStringLiteral("energy-frame-").size()));
    }
    QCOMPARE(observedEnergyThemes, energySpecs.size());
}

QTEST_GUILESS_MAIN(EnergyAssetTest)

#include "EnergyAssetTest.moc"
