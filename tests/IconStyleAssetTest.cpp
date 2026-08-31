#include "iconstyles/IconStylePackage.h"
#include "iconstyles/IconStyleStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTest>

using ArchDock::IconStylePackage;
using ArchDock::IconStyleStore;

namespace
{

struct StyleSpec
{
    QString id;
    QString family;
    QString previewSeed;
    int minimumLayers;
};

const QList<StyleSpec> styleSpecs{
    {QStringLiteral("metallic-blue"), QStringLiteral("metallic"),
     QStringLiteral("metallic-blue-v1"), 6},
    {QStringLiteral("metallic-red"), QStringLiteral("metallic"),
     QStringLiteral("metallic-red-v1"), 6},
    {QStringLiteral("neon-green"), QStringLiteral("neon"),
     QStringLiteral("neon-green-v1"), 5},
    {QStringLiteral("neon-orange"), QStringLiteral("neon"),
     QStringLiteral("neon-orange-v1"), 5},
    {QStringLiteral("dark-orb"), QStringLiteral("orb"),
     QStringLiteral("dark-orb-v1"), 7},
};

QString repositoryFile(const QString &relativePath)
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath())
        .absoluteFilePath(QStringLiteral("../") + relativePath);
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
        ? QString::fromLatin1(hash.result().toHex()) : QString{};
}

QJsonObject jsonObject(const QString &path)
{
    return QJsonDocument::fromJson(fileBytes(path)).object();
}

int layerCount(const ArchDock::IconStyleDefinition &definition)
{
    int result = definition.layers.rear.size() + definition.layers.base.size() +
        definition.layers.front.size();
    for (const auto *optional : {
             &definition.layers.mask, &definition.layers.reflection,
             &definition.layers.shadow, &definition.layers.glow})
    {
        if (optional->has_value())
        {
            ++result;
        }
    }
    return result;
}

}

class IconStyleAssetTest final : public QObject
{
    Q_OBJECT

private slots:
    void cleanRoomRecipeMatchesReferenceCatalog();
    void productionPackagesValidateAndPreserveGlyphs();
    void productionRecordsMatchExactOutputs();
    void catalogHasStableCapabilitiesAndPreviews();
    void requiredStatesRemainLegible();
};

void IconStyleAssetTest::cleanRoomRecipeMatchesReferenceCatalog()
{
    const QJsonObject recipe = jsonObject(repositoryFile(
        QStringLiteral("assets/source-samples/icon-styles/"
                       "original-artwork-recipe.json")));
    QCOMPARE(recipe.value(QStringLiteral("format")).toString(),
             QStringLiteral("org.archdock.clean-room-artwork-recipe"));
    QCOMPARE(recipe.value(QStringLiteral("task")).toString(),
             QStringLiteral("TASK-0029"));
    QVERIFY(!recipe.value(QStringLiteral("pixelInput")).toBool(true));
    QVERIFY(!recipe.value(QStringLiteral("sourceDerivative")).toBool(true));
    QVERIFY(!recipe.value(QStringLiteral("aiGenerated")).toBool(true));

    QHash<QString, QString> recipeReferences;
    const QJsonArray references = recipe.value(
        QStringLiteral("inspirationReferences")).toArray();
    QCOMPARE(references.size(), 16);
    for (const QJsonValue &value : references)
    {
        const QJsonObject reference = value.toObject();
        recipeReferences.insert(
            reference.value(QStringLiteral("catalogId")).toString(),
            reference.value(QStringLiteral("sha256")).toString());
    }

    QSet<QString> observed;
    const QJsonArray catalog = jsonObject(repositoryFile(
        QStringLiteral("data/source-assets/source-asset-catalog.json")))
                                   .value(QStringLiteral("assets")).toArray();
    for (const QJsonValue &value : catalog)
    {
        const QJsonObject record = value.toObject();
        if (record.value(QStringLiteral("assetClass")).toString() !=
            QStringLiteral("H-icon-style-reference-sheet"))
        {
            continue;
        }
        const QString id = record.value(QStringLiteral("id")).toString();
        observed.insert(id);
        QVERIFY2(recipeReferences.contains(id), qPrintable(id));
        QCOMPARE(recipeReferences.value(id),
                 record.value(QStringLiteral("sha256")).toString());
        QCOMPARE(record.value(QStringLiteral("status")).toString(),
                 QStringLiteral("reference-only"));
        QVERIFY(record.value(QStringLiteral("opaqueScreenshot")).toBool());
        QVERIFY(!record.value(QStringLiteral("isolatedCleanAsset")).toBool(true));
        QCOMPARE(record.value(QStringLiteral("provenance")).toObject()
                     .value(QStringLiteral("redistribution")).toString(),
                 QStringLiteral("unknown"));
    }
    QCOMPARE(observed.size(), 16);
    QCOMPARE(QSet<QString>(recipeReferences.keyBegin(), recipeReferences.keyEnd()),
             observed);
}

void IconStyleAssetTest::productionPackagesValidateAndPreserveGlyphs()
{
    for (const StyleSpec &spec : styleSpecs)
    {
        const QString root = QStringLiteral("assets/icon-styles/") + spec.id;
        const auto loaded = IconStylePackage::load(repositoryFile(
            root + QStringLiteral("/archdock-icon-style.json")));
        QVERIFY2(loaded.isValid(), qPrintable(
            loaded.primaryCode() + QStringLiteral(": ") + loaded.primaryMessage()));
        const auto &definition = loaded.package->definition();
        QCOMPARE(definition.id, spec.id);
        QCOMPARE(definition.glyphPolicy.mode, QStringLiteral("original"));
        QVERIFY(definition.glyphPolicy.compatibleOnly);
        QVERIFY(definition.mappedReplacements.isEmpty());
        QCOMPARE(definition.states.size(), 11);
        QVERIFY(layerCount(definition) >= spec.minimumLayers);
        QCOMPARE(definition.license.spdx, QStringLiteral("NOASSERTION"));
        QCOMPARE(definition.license.redistribution, QStringLiteral("allowed"));
        QCOMPARE(definition.capabilities.rendererTiers,
                 QStringList{QStringLiteral("procedural2d")});
        QVERIFY(definition.capabilities.features.contains(
            QStringLiteral("state-styling")));
        QVERIFY(definition.capabilities.features.contains(
            QStringLiteral("safe-glyph-inset")));
        QVERIFY(!definition.capabilities.supports3D);
        QCOMPARE(definition.preview.seed, spec.previewSeed);
        QVERIFY(!definition.preview.iconName.isEmpty());
        QCOMPARE(definition.preview.tileSize, 64);

        const QVariantMap projection = loaded.package->runtimeProjection();
        QVERIFY(projection.value(QStringLiteral("valid")).toBool());
        QVERIFY(projection.value(QStringLiteral("assetPaths")).toMap().isEmpty());
    }
}

void IconStyleAssetTest::productionRecordsMatchExactOutputs()
{
    for (const StyleSpec &spec : styleSpecs)
    {
        const QString root = QStringLiteral("assets/icon-styles/") + spec.id;
        QDirIterator iterator(repositoryFile(root), QDir::Files,
                              QDirIterator::Subdirectories);
        const QSet<QString> rasterExtensions{
            QStringLiteral("png"), QStringLiteral("jpg"),
            QStringLiteral("jpeg"), QStringLiteral("webp"),
            QStringLiteral("bmp")};
        while (iterator.hasNext())
        {
            const QFileInfo info(iterator.next());
            QVERIFY2(!info.fileName().startsWith(QStringLiteral("Screenshot_")),
                     qPrintable(info.filePath()));
            QVERIFY2(!rasterExtensions.contains(info.suffix().toLower()),
                     qPrintable(info.filePath()));
        }

        const QJsonObject production = jsonObject(repositoryFile(
            root + QStringLiteral("/metadata/production-record.json")));
        QCOMPARE(production.value(QStringLiteral("iconStyleId")).toString(), spec.id);
        QVERIFY(!production.value(QStringLiteral("pixelInput")).toBool(true));
        QVERIFY(!production.value(QStringLiteral("sourceDerivative")).toBool(true));
        QVERIFY(!production.value(QStringLiteral("aiGenerated")).toBool(true));
        QCOMPARE(production.value(QStringLiteral("parameters")).toObject()
                     .value(QStringLiteral("family")).toString(), spec.family);
        const QJsonArray outputs = production.value(
            QStringLiteral("outputs")).toArray();
        QCOMPARE(outputs.size(), 1);
        const QJsonObject output = outputs.at(0).toObject();
        QCOMPARE(output.value(QStringLiteral("path")).toString(),
                 QStringLiteral("archdock-icon-style.json"));
        QCOMPARE(fileSha256(repositoryFile(
                     root + QStringLiteral("/archdock-icon-style.json"))),
                 output.value(QStringLiteral("sha256")).toString());

        const QJsonObject review = jsonObject(repositoryFile(
            root + QStringLiteral("/metadata/visual-review.json")));
        QCOMPARE(review.value(QStringLiteral("iconStyleId")).toString(), spec.id);
        QCOMPARE(review.value(QStringLiteral("checks")).toObject()
                     .value(QStringLiteral("applicationGlyphPreserved")).toString(),
                 QStringLiteral("pass"));
        QCOMPARE(review.value(QStringLiteral("checks")).toObject()
                     .value(QStringLiteral("liveAndPreviewRendering")).toString(),
                 QStringLiteral("pass"));
        QCOMPARE(review.value(QStringLiteral("status")).toString(),
                 QStringLiteral("runtime-verified"));
    }
}

void IconStyleAssetTest::catalogHasStableCapabilitiesAndPreviews()
{
    const auto loaded = IconStyleStore::loadCatalog(
        repositoryFile(QStringLiteral(
            "data/icon-styles/builtin-icon-styles.json")),
        repositoryFile(QStringLiteral("assets/icon-styles")));
    QVERIFY2(loaded.isValid(), qPrintable(loaded.primaryMessage()));
    QCOMPARE(loaded.store->catalogEntries().size(), 6);
    QCOMPARE(loaded.store->fallbackStyleId(), QStringLiteral("plain-original"));

    QSet<QString> expected{QStringLiteral("plain-original")};
    for (const StyleSpec &spec : styleSpecs)
    {
        expected.insert(spec.id);
    }
    const QStringList styleIds = loaded.store->styleIds();
    QCOMPARE(QSet<QString>(styleIds.cbegin(), styleIds.cend()), expected);
    QSet<QString> previewSeeds;
    for (const QVariant &value : loaded.store->catalogEntries())
    {
        const QVariantMap entry = value.toMap();
        const QString id = entry.value(QStringLiteral("id")).toString();
        QVERIFY(QRegularExpression(
            QStringLiteral("\\A[a-z0-9]+(?:-[a-z0-9]+)*\\z"))
                    .match(id)
                    .hasMatch());
        const QVariantMap capabilities = entry.value(
            QStringLiteral("capabilities")).toMap();
        QVERIFY(!capabilities.value(
            QStringLiteral("rendererTiers")).toList().isEmpty());
        QVERIFY(capabilities.value(QStringLiteral("features")).toList()
                    .contains(QStringLiteral("safe-glyph-inset")));
        const QVariantMap preview = entry.value(QStringLiteral("preview")).toMap();
        const QString previewSeed = preview.value(
            QStringLiteral("seed")).toString();
        QVERIFY(!previewSeed.isEmpty());
        QVERIFY(!previewSeeds.contains(previewSeed));
        previewSeeds.insert(previewSeed);
        QVERIFY(!preview.value(QStringLiteral("iconName")).toString().isEmpty());
        QVERIFY(!preview.value(QStringLiteral("state")).toString().isEmpty());
        QCOMPARE(preview.value(QStringLiteral("tileSize")).toInt(), 64);
    }
    QCOMPARE(previewSeeds.size(), expected.size());

    const auto reloaded = IconStyleStore::loadCatalog(
        repositoryFile(QStringLiteral(
            "data/icon-styles/builtin-icon-styles.json")),
        repositoryFile(QStringLiteral("assets/icon-styles")));
    QVERIFY(reloaded.isValid());
    QCOMPARE(reloaded.store->catalogEntries(),
             loaded.store->catalogEntries());
}

void IconStyleAssetTest::requiredStatesRemainLegible()
{
    for (const StyleSpec &spec : styleSpecs)
    {
        const auto loaded = IconStylePackage::load(repositoryFile(
            QStringLiteral("assets/icon-styles/") + spec.id +
            QStringLiteral("/archdock-icon-style.json")));
        QVERIFY(loaded.isValid());
        const auto &definition = loaded.package->definition();
        for (const auto &state : definition.states)
        {
            QVERIFY2(state.glyphOpacity >= 0.3, qPrintable(spec.id +
                QLatin1Char(':') + state.id));
            QVERIFY2(state.glyphScale >= 0.5, qPrintable(spec.id +
                QLatin1Char(':') + state.id));
            QVERIFY2(state.baseOpacity >= 0.3, qPrintable(spec.id +
                QLatin1Char(':') + state.id));
        }
        QVERIFY(definition.stateById(QStringLiteral("hover"))->glowOpacity >= 0.6);
        QVERIFY(definition.stateById(QStringLiteral("active"))->indicatorOpacity >= 0.9);
        QVERIFY(definition.stateById(QStringLiteral("urgent"))->glowOpacity >= 0.9);
        QVERIFY(definition.stateById(QStringLiteral("minimized"))->glyphOpacity >= 0.5);
    }
}

QTEST_GUILESS_MAIN(IconStyleAssetTest)

#include "IconStyleAssetTest.moc"
