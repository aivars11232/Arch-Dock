#include "iconstyles/IconStylePackage.h"
#include "iconstyles/IconStyleStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

using ArchDock::IconStylePackage;
using ArchDock::IconStyleStore;

namespace
{

QString fixturePath(const QString &name)
{
    return QFINDTESTDATA(QStringLiteral("fixtures/icon-style-v1/") + name);
}

QString repositoryFile(const QString &relativePath)
{
    return QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath())
        .absoluteFilePath(QStringLiteral("../") + relativePath);
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return QDir().mkpath(QFileInfo(path).absolutePath()) &&
        file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

bool containsDiagnostic(
    const QVector<ArchDock::IconStyleValidationDiagnostic> &diagnostics,
    const QString &code)
{
    return std::any_of(diagnostics.cbegin(), diagnostics.cend(),
                       [&code](const auto &entry)
                       {
                           return entry.code == code;
                       });
}

}

class IconStylePackageTest final : public QObject
{
    Q_OBJECT

private slots:
    void fixtures_data();
    void fixtures();
    void repeatedLoadsAreDeterministic();
    void symlinkEscapeIsRejectedBeforeUse();
    void manifestLimitIsEnforcedBeforeParsing();
    void invalidResultDoesNotProjectPartialPackage();
    void undecodableRenderAssetIsRejected();
    void threeDReferenceAssetsAreNotDecodeProbed();
    void builtInCatalogMatchesPackages();
    void unknownSelectionFallsBackToPlainOriginal();
};

void IconStylePackageTest::fixtures_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("expectValid");
    QTest::addColumn<QString>("expectedCode");

    const QString indexPath = fixturePath(QStringLiteral("fixture-index.json"));
    QVERIFY2(!indexPath.isEmpty(), "icon-style fixture index was not found");
    QFile file(indexPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonArray fixtures = QJsonDocument::fromJson(file.readAll())
                                    .object()
                                    .value(QStringLiteral("fixtures"))
                                    .toArray();
    QCOMPARE(fixtures.size(), 9);
    for (const QJsonValue &value : fixtures)
    {
        const QJsonObject fixture = value.toObject();
        const QString fileName = fixture.value(QStringLiteral("file")).toString();
        QTest::newRow(qPrintable(fileName))
            << fileName
            << fixture.value(QStringLiteral("valid")).toBool()
            << fixture.value(QStringLiteral("diagnostic")).toString();
    }
}

void IconStylePackageTest::fixtures()
{
    QFETCH(QString, fileName);
    QFETCH(bool, expectValid);
    QFETCH(QString, expectedCode);

    const QString path = fixturePath(fileName);
    QVERIFY2(!path.isEmpty(), qPrintable(fileName));
    const auto result = IconStylePackage::load(path);
    QCOMPARE(result.isValid(), expectValid);
    if (!expectedCode.isEmpty())
    {
        QVERIFY2(containsDiagnostic(result.diagnostics, expectedCode),
                 qPrintable(result.primaryMessage()));
    }
    if (!expectValid)
    {
        QVERIFY(!result.package.has_value());
        return;
    }

    QVERIFY(result.package.has_value());
    QCOMPARE(result.package->definition().format,
             QStringLiteral("org.archdock.icon-style"));
    QCOMPARE(result.package->definition().version, 1);
    QCOMPARE(result.package->definition().states.size(), 11);
    QVERIFY(!result.package->contentDigest().isEmpty());
    const QVariantMap projection = result.package->runtimeProjection();
    QVERIFY(projection.value(QStringLiteral("valid")).toBool());
    QVERIFY(projection.value(QStringLiteral("loadable")).toBool());
    if (fileName == QStringLiteral("valid-asset.json"))
    {
        const QString asset = result.package->assetPath(
            QStringLiteral("assets/base.svg"));
        QVERIFY(QFileInfo(asset).isFile());
        QCOMPARE(projection.value(QStringLiteral("assetPaths")).toMap()
                     .value(QStringLiteral("assets/base.svg")).toString(),
                 asset);
    }
}

void IconStylePackageTest::repeatedLoadsAreDeterministic()
{
    const QString path = fixturePath(QStringLiteral("valid-asset.json"));
    const auto first = IconStylePackage::load(path);
    const auto second = IconStylePackage::load(path);
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QCOMPARE(first.package->definition(), second.package->definition());
    QCOMPARE(first.package->contentDigest(), second.package->contentDigest());
    QCOMPARE(first.package->runtimeProjection(), second.package->runtimeProjection());
}

void IconStylePackageTest::symlinkEscapeIsRejectedBeforeUse()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString packageRoot = root.filePath(QStringLiteral("package"));
    QVERIFY(QDir().mkpath(QDir(packageRoot).filePath(QStringLiteral("assets"))));
    const QString outside = root.filePath(QStringLiteral("outside.svg"));
    QVERIFY(writeBytes(outside, QByteArrayLiteral("<svg/>")));
    const QString link = QDir(packageRoot).filePath(QStringLiteral("assets/base.svg"));
    if (!QFile::link(outside, link))
    {
        QSKIP("filesystem does not permit creating the symlink fixture");
    }

    QFile fixture(fixturePath(QStringLiteral("valid-asset.json")));
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const QString manifestPath = QDir(packageRoot).filePath(
        QStringLiteral("archdock-icon-style.json"));
    QVERIFY(writeBytes(manifestPath, fixture.readAll()));

    const auto result = IconStylePackage::load(manifestPath);
    QVERIFY(!result.isValid());
    QVERIFY(containsDiagnostic(result.diagnostics, QStringLiteral("unsafe-path")));
    QVERIFY(!result.package.has_value());
}

void IconStylePackageTest::manifestLimitIsEnforcedBeforeParsing()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray oversized(IconStylePackage::MaximumManifestBytes + 1, ' ');
    const auto result = IconStylePackage::loadBytes(oversized, root.path());
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("manifest-too-large"));
}

void IconStylePackageTest::invalidResultDoesNotProjectPartialPackage()
{
    const auto result = IconStylePackage::load(
        fixturePath(QStringLiteral("invalid-missing-asset.json")));
    QVERIFY(!result.isValid());
    const QVariantMap projection = result.toVariantMap();
    QVERIFY(!projection.value(QStringLiteral("valid")).toBool());
    QVERIFY(!projection.contains(QStringLiteral("iconStyle")));
}

void IconStylePackageTest::builtInCatalogMatchesPackages()
{
    const QString catalog = repositoryFile(
        QStringLiteral("data/icon-styles/builtin-icon-styles.json"));
    const QString packageRoot = repositoryFile(QStringLiteral("assets/icon-styles"));
    const auto result = IconStyleStore::loadCatalog(catalog, packageRoot);
    QVERIFY2(result.isValid(), qPrintable(result.primaryMessage()));
    QCOMPARE(result.store->fallbackStyleId(), QStringLiteral("plain-original"));
    QVERIFY(result.store->contains(QStringLiteral("plain-original")));
    QCOMPARE(result.store->catalogEntries().size(), 6);
    const QStringList expectedIds{
        QStringLiteral("dark-orb"), QStringLiteral("metallic-blue"),
        QStringLiteral("metallic-red"), QStringLiteral("neon-green"),
        QStringLiteral("neon-orange"), QStringLiteral("plain-original")};
    QCOMPARE(result.store->styleIds(), expectedIds);
    for (const QString &styleId : expectedIds)
    {
        const QVariantMap projection = result.store->resolve(styleId);
        QVERIFY2(projection.value(QStringLiteral("valid")).toBool(),
                 qPrintable(styleId));
        QCOMPARE(projection.value(QStringLiteral("resolvedStyleId")).toString(),
                 styleId);
        QCOMPARE(projection.value(QStringLiteral("glyphPolicy")).toMap()
                     .value(QStringLiteral("mode")).toString(),
                 QStringLiteral("original"));
    }
}

void IconStylePackageTest::unknownSelectionFallsBackToPlainOriginal()
{
    const auto result = IconStyleStore::loadCatalog(
        repositoryFile(QStringLiteral("data/icon-styles/builtin-icon-styles.json")),
        repositoryFile(QStringLiteral("assets/icon-styles")));
    QVERIFY(result.isValid());

    const QVariantMap selection = result.store->resolve(
        QStringLiteral("not-installed"));
    QVERIFY(selection.value(QStringLiteral("valid")).toBool());
    QVERIFY(selection.value(QStringLiteral("fellBack")).toBool());
    QCOMPARE(selection.value(QStringLiteral("requestedStyleId")).toString(),
             QStringLiteral("not-installed"));
    QCOMPARE(selection.value(QStringLiteral("resolvedStyleId")).toString(),
             QStringLiteral("plain-original"));
    QCOMPARE(selection.value(QStringLiteral("fallbackReason")).toString(),
             QStringLiteral("unknown-style"));

    const QVariantMap defaulted = result.store->resolve(QString{});
    QCOMPARE(defaulted.value(QStringLiteral("selectionStatus")).toString(),
             QStringLiteral("defaulted"));
    QCOMPARE(defaulted.value(QStringLiteral("resolvedStyleId")).toString(),
             QStringLiteral("plain-original"));
}

void IconStylePackageTest::undecodableRenderAssetIsRejected()
{
    // A render asset may be present, contained and within the byte limits and
    // still be unusable. Digest acceptance alone would let it reach the scene
    // and produce a partially styled icon instead of the plain original glyph.
    const QString manifestPath = fixturePath(
        QStringLiteral("invalid-undecodable-layer-asset.json"));
    QVERIFY(!manifestPath.isEmpty());
    const QString assetPath = fixturePath(QStringLiteral("assets/corrupt.svg"));
    QVERIFY(QFileInfo(assetPath).isFile());
    QVERIFY(QFileInfo(assetPath).size() > 0);

    const auto layerResult = IconStylePackage::load(manifestPath);
    QVERIFY(!layerResult.isValid());
    QVERIFY(!layerResult.package.has_value());
    QVERIFY2(containsDiagnostic(layerResult.diagnostics,
                                QStringLiteral("asset-not-decodable")),
             qPrintable(layerResult.primaryMessage()));

    const auto mappedResult = IconStylePackage::load(
        fixturePath(QStringLiteral("invalid-undecodable-mapped-asset.json")));
    QVERIFY(!mappedResult.isValid());
    QVERIFY(!mappedResult.package.has_value());
    QVERIFY2(containsDiagnostic(mappedResult.diagnostics,
                                QStringLiteral("asset-not-decodable")),
             qPrintable(mappedResult.primaryMessage()));

    // The readable sibling asset must stay acceptable, so the probe rejects
    // the undecodable bytes rather than the asset layer kind itself.
    const auto valid = IconStylePackage::load(
        fixturePath(QStringLiteral("valid-asset.json")));
    QVERIFY(valid.isValid());
}

void IconStylePackageTest::threeDReferenceAssetsAreNotDecodeProbed()
{
    // Optional 3D mesh/material resources are bounded path-only assets. They
    // are not images, so the render-asset decode probe must not reach them.
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString root = directory.path();

    QFile source(fixturePath(QStringLiteral("valid-original.json")));
    QVERIFY(source.open(QIODevice::ReadOnly));
    QJsonObject manifest = QJsonDocument::fromJson(source.readAll()).object();
    source.close();

    manifest.insert(QStringLiteral("id"), QStringLiteral("fixture-three-d"));
    QJsonObject capabilities =
        manifest.value(QStringLiteral("capabilities")).toObject();
    capabilities.insert(QStringLiteral("supports3D"), true);
    QJsonArray rendererTiers =
        capabilities.value(QStringLiteral("rendererTiers")).toArray();
    rendererTiers.append(QStringLiteral("true3d"));
    capabilities.insert(QStringLiteral("rendererTiers"), rendererTiers);
    manifest.insert(QStringLiteral("capabilities"), capabilities);
    manifest.insert(QStringLiteral("threeD"),
                    QJsonObject{{QStringLiteral("mesh"),
                                 QStringLiteral("assets/tile.mesh")},
                                {QStringLiteral("material"),
                                 QStringLiteral("assets/tile.material")},
                                {QStringLiteral("fallbackStyleId"),
                                 QStringLiteral("plain-original")}});

    QVERIFY(writeBytes(root + QStringLiteral("/assets/tile.mesh"),
                       QByteArrayLiteral("archdock-mesh-placeholder")));
    QVERIFY(writeBytes(root + QStringLiteral("/assets/tile.material"),
                       QByteArrayLiteral("archdock-material-placeholder")));
    const QString manifestPath = root + QStringLiteral("/archdock-icon-style.json");
    QVERIFY(writeBytes(manifestPath,
                       QJsonDocument(manifest).toJson(QJsonDocument::Compact)));

    const auto result = IconStylePackage::load(manifestPath);
    QVERIFY2(result.isValid(), qPrintable(result.primaryMessage()));
    QVERIFY(!containsDiagnostic(result.diagnostics,
                                QStringLiteral("asset-not-decodable")));
    QVERIFY(result.package.has_value());
    QVERIFY(QFileInfo(result.package->assetPath(
                          QStringLiteral("assets/tile.mesh")))
                .isFile());
    QVERIFY(QFileInfo(result.package->assetPath(
                          QStringLiteral("assets/tile.material")))
                .isFile());
}

QTEST_GUILESS_MAIN(IconStylePackageTest)

#include "IconStylePackageTest.moc"
