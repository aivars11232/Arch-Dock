#include "themes/ThemePackage.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using ArchDock::ThemePackage;

namespace
{

QString fixturePath(const QString &name)
{
    return QFINDTESTDATA(QStringLiteral("fixtures/theme-v2/") + name);
}

bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return QDir().mkpath(QFileInfo(path).absolutePath()) &&
        file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray versionOneManifest(const QString &asset,
                              const QString &shaFit = QStringLiteral("contain"))
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("id"), QStringLiteral("Legacy Example")},
        {QStringLiteral("name"), QStringLiteral("Legacy example")},
        {QStringLiteral("author"), QStringLiteral("Fixture author")},
        {QStringLiteral("surface"), QJsonObject{
             {QStringLiteral("asset"), asset},
             {QStringLiteral("fit"), shaFit},
         }},
    }).toJson(QJsonDocument::Indented);
}

QByteArray versionTwoRasterManifest(const QString &path,
                                    const QString &sha256 = {})
{
    QJsonObject asset{
        {QStringLiteral("id"), QStringLiteral("surface")},
        {QStringLiteral("path"), path},
        {QStringLiteral("kind"), QStringLiteral("raster")},
        {QStringLiteral("naturalSize"), QJsonObject{
             {QStringLiteral("width"), 1},
             {QStringLiteral("height"), 1},
         }},
    };
    if (!sha256.isEmpty())
    {
        asset.insert(QStringLiteral("sha256"), sha256);
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
        {QStringLiteral("version"), 2},
        {QStringLiteral("id"), QStringLiteral("test-raster")},
        {QStringLiteral("name"), QStringLiteral("Test raster")},
        {QStringLiteral("capabilities"), QJsonObject{
             {QStringLiteral("hosts"), QJsonArray{QStringLiteral("native-edge")}},
             {QStringLiteral("rendererTiers"),
              QJsonArray{QStringLiteral("skinned2d"), QStringLiteral("procedural2d")}},
             {QStringLiteral("preferredRendererTier"), QStringLiteral("skinned2d")},
             {QStringLiteral("fallbackRendererTiers"),
              QJsonArray{QStringLiteral("procedural2d")}},
             {QStringLiteral("layouts"), QJsonArray{QStringLiteral("horizontal")}},
             {QStringLiteral("orientations"), QJsonArray{QStringLiteral("horizontal")}},
             {QStringLiteral("features"), QJsonArray{}},
             {QStringLiteral("presentationMechanisms"), QJsonArray{}},
             {QStringLiteral("rotation"),
              QJsonObject{{QStringLiteral("mode"), QStringLiteral("none")}}},
         }},
        {QStringLiteral("assets"), QJsonArray{asset}},
    }).toJson(QJsonDocument::Compact);
}

}

class ThemePackageTest final : public QObject
{
    Q_OBJECT

private slots:
    void fixtures_data();
    void fixtures();
    void versionOneAdaptsWithoutRewritingSource();
    void repeatedLoadsAreDeterministic();
    void symlinkEscapeIsRejectedBeforeUse();
    void suppliedHashMustMatch();
    void manifestLimitIsEnforcedBeforeParsing();
    void managedCopyIsAtomicAndReusable();
    void dynamicGlowCapabilityIsVersionedAndStrict();
};

void ThemePackageTest::fixtures_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<bool>("expectValid");
    QTest::addColumn<QString>("expectedCode");

    const QString indexPath = fixturePath(QStringLiteral("fixture-index.json"));
    QVERIFY2(!indexPath.isEmpty(), "theme fixture index was not found");
    QFile file(indexPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QJsonArray fixtures = QJsonDocument::fromJson(file.readAll())
                                    .object()
                                    .value(QStringLiteral("fixtures"))
                                    .toArray();
    QCOMPARE(fixtures.size(), 11);
    for (const QJsonValue &value : fixtures)
    {
        const QJsonObject fixture = value.toObject();
        const QString fileName = fixture.value(QStringLiteral("path")).toString();
        QTest::newRow(qPrintable(fileName))
            << fileName
            << fixture.value(QStringLiteral("expectValid")).toBool()
            << fixture.value(QStringLiteral("expectedCode")).toString();
    }
}

void ThemePackageTest::fixtures()
{
    QFETCH(QString, fileName);
    QFETCH(bool, expectValid);
    QFETCH(QString, expectedCode);

    const QString path = fixturePath(fileName);
    QVERIFY2(!path.isEmpty(), qPrintable(fileName));
    const auto result = ThemePackage::load(path);
    QCOMPARE(result.isValid(), expectValid);
    QCOMPARE(result.primaryCode(), expectedCode);
    if (expectValid)
    {
        QVERIFY(result.package.has_value());
        QCOMPARE(result.package->sourceVersion(), 2);
        QCOMPARE(result.package->definition().version, 2);
        QCOMPARE(result.package->definition().id,
                 QFileInfo(path).completeBaseName() == QStringLiteral("valid-procedural")
                     ? QStringLiteral("fixture-procedural")
                     : result.package->definition().id);
        QVERIFY(!result.package->contentDigest().isEmpty());
        QVERIFY(result.package->runtimeProjection().value(
                    QStringLiteral("valid")).toBool());
    }
}

void ThemePackageTest::versionOneAdaptsWithoutRewritingSource()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString assetPath = root.filePath(QStringLiteral("assets/surface.svg"));
    QVERIFY(writeBytes(assetPath, QByteArrayLiteral("<svg/>")));
    const QString manifestPath = root.filePath(QStringLiteral("archdock-theme.json"));
    const QByteArray original = versionOneManifest(QStringLiteral("assets/surface.svg"));
    QVERIFY(writeBytes(manifestPath, original));

    const auto result = ThemePackage::load(manifestPath);
    QVERIFY2(result.isValid(), qPrintable(result.primaryMessage()));
    QCOMPARE(result.package->sourceVersion(), 1);
    QCOMPARE(result.package->definition().sourceVersion, 1);
    QVERIFY(result.package->definition().adaptedFromVersion1);
    QCOMPARE(result.package->definition().id, QStringLiteral("legacy-example"));
    QCOMPARE(result.package->definition().states.size(), 1);
    QCOMPARE(result.package->definition().layers.size(), 1);
    QCOMPARE(result.package->definition().capabilities.preferredRendererTier,
             QStringLiteral("skinned2d"));
    QCOMPARE(result.package->definition().capabilities.fallbackRendererTiers,
             QStringList{QStringLiteral("procedural2d")});
    QCOMPARE(result.package->legacyFit(), QStringLiteral("contain"));

    QFile unchanged(manifestPath);
    QVERIFY(unchanged.open(QIODevice::ReadOnly));
    QCOMPARE(unchanged.readAll(), original);
}

void ThemePackageTest::repeatedLoadsAreDeterministic()
{
    const QString validPath = fixturePath(QStringLiteral("valid-skinned2d-states.json"));
    const auto first = ThemePackage::load(validPath);
    const auto second = ThemePackage::load(validPath);
    QVERIFY(first.isValid());
    QVERIFY(second.isValid());
    QCOMPARE(first.package->definition(), second.package->definition());
    QCOMPARE(first.package->contentDigest(), second.package->contentDigest());
    QCOMPARE(first.package->runtimeProjection(), second.package->runtimeProjection());

    const QString invalidPath = fixturePath(QStringLiteral("invalid-bounds.json"));
    const auto invalidFirst = ThemePackage::load(invalidPath);
    const auto invalidSecond = ThemePackage::load(invalidPath);
    QVERIFY(!invalidFirst.isValid());
    QCOMPARE(invalidFirst.diagnostics, invalidSecond.diagnostics);
}

void ThemePackageTest::symlinkEscapeIsRejectedBeforeUse()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString packagePath = root.filePath(QStringLiteral("package"));
    QVERIFY(QDir().mkpath(packagePath));
    const QString outside = root.filePath(QStringLiteral("outside.png"));
    QVERIFY(writeBytes(outside, QByteArrayLiteral("outside")));
    const QString link = QDir(packagePath).filePath(QStringLiteral("surface.png"));
    if (!QFile::link(outside, link))
    {
        QSKIP("filesystem does not permit creating the symlink fixture");
    }
    const QString manifest = QDir(packagePath).filePath(
        QStringLiteral("archdock-theme.json"));
    QVERIFY(writeBytes(manifest, versionTwoRasterManifest(QStringLiteral("surface.png"))));

    const auto result = ThemePackage::load(manifest);
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("unsafe-path"));
}

void ThemePackageTest::suppliedHashMustMatch()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeBytes(root.filePath(QStringLiteral("surface.png")),
                       QByteArrayLiteral("synthetic")));
    const QString manifest = root.filePath(QStringLiteral("archdock-theme.json"));
    QVERIFY(writeBytes(manifest,
                       versionTwoRasterManifest(
                           QStringLiteral("surface.png"), QString(64, QLatin1Char('0')))));

    const auto result = ThemePackage::load(manifest);
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("asset-hash-mismatch"));
}

void ThemePackageTest::manifestLimitIsEnforcedBeforeParsing()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QByteArray oversized(ThemePackage::MaximumManifestBytes + 1, ' ');
    const auto result = ThemePackage::loadBytes(oversized, root.path());
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), QStringLiteral("manifest-too-large"));
}

void ThemePackageTest::managedCopyIsAtomicAndReusable()
{
    const QString sourceManifest = fixturePath(
        QStringLiteral("valid-skinned2d-states.json"));
    QFile sourceFile(sourceManifest);
    QVERIFY(sourceFile.open(QIODevice::ReadOnly));
    const QByteArray sourceBytes = sourceFile.readAll();

    const auto source = ThemePackage::load(sourceManifest);
    QVERIFY(source.isValid());
    QTemporaryDir managed;
    QVERIFY(managed.isValid());

    const auto first = source.package->materialize(managed.path());
    QVERIFY2(first.isValid(), qPrintable(first.diagnostics.isEmpty()
                                             ? QString{}
                                             : first.diagnostics.first().message));
    QVERIFY(!first.reusedExisting);
    QVERIFY(first.package->manifestPath().startsWith(managed.path()));
    QCOMPARE(first.package->contentDigest(), source.package->contentDigest());
    QVERIFY(QFileInfo(first.package->assetPath(QStringLiteral("surface"))).isFile());
    QVERIFY(QFileInfo(first.package->assetPath(QStringLiteral("input-mask"))).isFile());
    QCOMPARE(first.package->assetPath(QStringLiteral("surface")),
             first.package->assetPath(QStringLiteral("input-mask")));

    const auto second = source.package->materialize(managed.path());
    QVERIFY(second.isValid());
    QVERIFY(second.reusedExisting);
    QCOMPARE(second.package->manifestPath(), first.package->manifestPath());

    QFile sourceAfter(sourceManifest);
    QVERIFY(sourceAfter.open(QIODevice::ReadOnly));
    QCOMPARE(sourceAfter.readAll(), sourceBytes);
    const QStringList staging = QDir(managed.path()).entryList(
        QStringList{QStringLiteral(".archdock-theme-*")}, QDir::Dirs | QDir::Hidden);
    QVERIFY(staging.isEmpty());
}

void ThemePackageTest::dynamicGlowCapabilityIsVersionedAndStrict()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeBytes(root.filePath(QStringLiteral("surface.png")),
                       QByteArrayLiteral("synthetic")));

    QJsonObject manifest = QJsonDocument::fromJson(
        versionTwoRasterManifest(QStringLiteral("surface.png"))).object();
    QJsonObject capabilities = manifest.value(
        QStringLiteral("capabilities")).toObject();
    capabilities.insert(QStringLiteral("features"),
                        QJsonArray{QStringLiteral("dynamic-glow")});
    manifest.insert(QStringLiteral("capabilities"), capabilities);

    const QString accepted = root.filePath(QStringLiteral("accepted.json"));
    QVERIFY(writeBytes(accepted, QJsonDocument(manifest).toJson()));
    const auto valid = ThemePackage::load(accepted);
    QVERIFY2(valid.isValid(), qPrintable(valid.primaryMessage()));
    QVERIFY(valid.package->definition().capabilities.features.contains(
        QStringLiteral("dynamic-glow")));

    capabilities.insert(QStringLiteral("features"),
                        QJsonArray{QStringLiteral("unapproved-runtime-shader")});
    manifest.insert(QStringLiteral("capabilities"), capabilities);
    const QString rejected = root.filePath(QStringLiteral("rejected.json"));
    QVERIFY(writeBytes(rejected, QJsonDocument(manifest).toJson()));
    const auto invalid = ThemePackage::load(rejected);
    QVERIFY(!invalid.isValid());
    QCOMPARE(invalid.primaryCode(), QStringLiteral("invalid-enum"));
}

QTEST_GUILESS_MAIN(ThemePackageTest)

#include "ThemePackageTest.moc"
