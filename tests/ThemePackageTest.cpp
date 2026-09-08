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

QJsonObject sceneMeshFixture()
{
    return QJsonDocument::fromJson(R"({
        "format":"org.archdock.mesh", "version":1,
        "positions":[[0,0,0],[10,0,0],[0,10,0],[0,0,10]],
        "normals":[[0,0,1],[0,0,1],[0,0,1],[0,0,1]],
        "uv0s":[[0,0],[1,0],[0,1],[1,1]],
        "indexes":[0,2,1,0,1,3,1,2,3,2,0,3]
    })").object();
}

QJsonObject sceneMaterialFixture()
{
    return QJsonDocument::fromJson(R"({
        "format":"org.archdock.material", "version":1,
        "baseColor":"#6688aa", "emissiveColor":"#006688",
        "emissiveStrength":0.5, "metalness":0.6, "roughness":0.35
    })").object();
}

QJsonObject sceneManifestFixture()
{
    QJsonObject manifest = QJsonDocument::fromJson(
        versionTwoRasterManifest(QStringLiteral("texture.svg"))).object();
    QJsonObject capabilities = manifest[QStringLiteral("capabilities")].toObject();
    capabilities[QStringLiteral("hosts")] = QJsonArray{QStringLiteral("free-desktop")};
    capabilities[QStringLiteral("rendererTiers")] = QJsonArray{
        QStringLiteral("true3d"), QStringLiteral("procedural2d")};
    capabilities[QStringLiteral("preferredRendererTier")] = QStringLiteral("true3d");
    manifest[QStringLiteral("capabilities")] = capabilities;
    QJsonArray assets = manifest[QStringLiteral("assets")].toArray();
    assets.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("mesh")},
                              {QStringLiteral("path"), QStringLiteral("mesh.json")},
                              {QStringLiteral("kind"), QStringLiteral("mesh")}});
    assets.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("metal")},
                              {QStringLiteral("path"), QStringLiteral("material.json")},
                              {QStringLiteral("kind"), QStringLiteral("material")}});
    manifest[QStringLiteral("assets")] = assets;
    manifest[QStringLiteral("scene3D")] = QJsonObject{
        {QStringLiteral("mesh"), QStringLiteral("mesh")},
        {QStringLiteral("iconMesh"), QStringLiteral("mesh")},
        {QStringLiteral("material"), QStringLiteral("metal")},
        {QStringLiteral("texture"), QStringLiteral("surface")}};
    return manifest;
}

bool writeSceneFixture(const QTemporaryDir &root)
{
    return writeBytes(root.filePath(QStringLiteral("mesh.json")),
                      QJsonDocument(sceneMeshFixture()).toJson()) &&
        writeBytes(root.filePath(QStringLiteral("material.json")),
                   QJsonDocument(sceneMaterialFixture()).toJson()) &&
        writeBytes(root.filePath(QStringLiteral("texture.svg")), QByteArrayLiteral(
            "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><path fill='white' d='M0 0h1v1H0z'/></svg>"));
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
    void scene3DResourcesAreValidatedAndMaterialized();
    void scene3DRejectsUnsafeData_data();
    void scene3DRejectsUnsafeData();
    void scene3DMissingAndOversizedResourcesFailClosed();
    void originalMeshThemeLoadsAndCopies();
};

void ThemePackageTest::originalMeshThemeLoadsAndCopies()
{
    const QString path = QFINDTESTDATA("../assets/themes/mesh-platform-cyan/archdock-theme.json");
    QVERIFY(!path.isEmpty());
    const auto result = ThemePackage::load(path);
    QVERIFY2(result.isValid(), qPrintable(result.primaryCode()));
    QVERIFY(result.package->definition().scene3D.has_value());
    const QVariantMap mesh = result.package->runtimeProjection()
        .value(QStringLiteral("scene3DResources")).toMap().value(QStringLiteral("mesh")).toMap();
    QCOMPARE(mesh.value(QStringLiteral("positions")).toList().size(), 192);
    QCOMPARE(mesh.value(QStringLiteral("indexes")).toList().size(), 288);
    QTemporaryDir managed;
    const auto copied = result.package->materialize(managed.path());
    QVERIFY2(copied.isValid(), qPrintable(copied.diagnostics.isEmpty()
        ? QString{} : copied.diagnostics.first().message));
    QCOMPARE(copied.package->runtimeProjection().value(QStringLiteral("scene3DResources")),
             result.package->runtimeProjection().value(QStringLiteral("scene3DResources")));
}

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

void ThemePackageTest::scene3DResourcesAreValidatedAndMaterialized()
{
    QTemporaryDir root;
    QTemporaryDir managed;
    QVERIFY(root.isValid() && managed.isValid());
    QVERIFY(writeSceneFixture(root));
    const auto result = ThemePackage::loadBytes(
        QJsonDocument(sceneManifestFixture()).toJson(), root.path());
    QVERIFY2(result.isValid(), qPrintable(result.primaryMessage()));
    QVERIFY(result.package->definition().scene3D.has_value());
    QCOMPARE(result.package->definition().scene3D->fieldOfView, 40);
    QCOMPARE(result.package->definition().scene3D->defaultQuality, QStringLiteral("medium"));
    const QVariantMap resources = result.package->runtimeProjection()
        .value(QStringLiteral("scene3DResources")).toMap();
    QCOMPARE(resources.value(QStringLiteral("mesh")).toMap(), sceneMeshFixture().toVariantMap());
    QCOMPARE(resources.value(QStringLiteral("material")).toMap(), sceneMaterialFixture().toVariantMap());
    QVERIFY(!result.package->definition().toVariantMap().contains(QStringLiteral("scene3DResources")));
    const auto copy = result.package->materialize(managed.path());
    QVERIFY2(copy.isValid(), qPrintable(copy.diagnostics.isEmpty() ? QString{} : copy.diagnostics.first().message));
    QCOMPARE(copy.package->runtimeProjection().value(QStringLiteral("scene3DResources")).toMap(), resources);
    QVERIFY(copy.package->assetPath(QStringLiteral("mesh")).startsWith(managed.path()));
}

void ThemePackageTest::scene3DRejectsUnsafeData_data()
{
    QTest::addColumn<QString>("target");
    QTest::addColumn<QString>("key");
    QTest::addColumn<QVariant>("value");
    QTest::addColumn<QString>("code");
    const auto row = [](const char *name, const char *target, const char *key,
                        QVariant value, const char *code)
    {
        QTest::newRow(name) << QString::fromLatin1(target) << QString::fromLatin1(key)
                            << value << QString::fromLatin1(code);
    };
    row("wide-camera", "scene", "fieldOfView", 100, "invalid-bounds");
    row("camera-pitch", "scene", "cameraPitch", 61, "invalid-bounds");
    row("camera-yaw", "scene", "cameraYaw", -181, "invalid-bounds");
    row("key-light", "scene", "keyLightBrightness", 4.1, "invalid-bounds");
    row("fill-light", "scene", "fillLightBrightness", -1, "invalid-bounds");
    row("quality", "scene", "defaultQuality", "ultra", "invalid-enum");
    row("scene-script", "scene", "script", "run.qml", "unknown-field");
    row("wrong-mesh-kind", "scene", "mesh", "surface", "invalid-scene3d-resource");
    row("missing-texture-reference", "scene", "texture", "absent", "invalid-scene3d-resource");
    row("material-script", "material", "shader", "shader.frag", "unknown-field");
    row("unbounded-emission", "material", "emissiveStrength", 100, "invalid-scene3d-material");
    row("material-color", "material", "baseColor", "javascript:run()", "invalid-scene3d-material");
    row("material-metalness", "material", "metalness", -1, "invalid-scene3d-material");
    row("material-roughness", "material", "roughness", 1.1, "invalid-scene3d-material");
    row("out-of-range-index", "mesh", "indexes", QVariantList{0, 1, 4}, "invalid-scene3d-mesh");
    row("fractional-index", "mesh", "indexes", QVariantList{0, 1, 1.5}, "invalid-scene3d-mesh");
    row("missing-triangle", "mesh", "indexes", QVariantList{0, 1}, "invalid-scene3d-mesh");
    row("degenerate-triangle", "mesh", "indexes", QVariantList{0, 0, 0}, "invalid-scene3d-mesh");
    row("unused-depth-vertex", "mesh", "indexes", QVariantList{0, 2, 1}, "invalid-scene3d-mesh");
    row("missing-normals", "mesh", "normals", QVariantList{}, "invalid-scene3d-mesh");
    row("mesh-script", "mesh", "generator", "generate.js", "unknown-field");
    QJsonArray flat = sceneMeshFixture()[QStringLiteral("positions")].toArray();
    flat[3] = QJsonArray{1, 1, 0};
    row("flat-is-not-3d", "mesh", "positions", flat.toVariantList(), "invalid-scene3d-mesh");
    QJsonArray oversized = sceneMeshFixture()[QStringLiteral("positions")].toArray();
    oversized[3] = QJsonArray{0, 0, 1001};
    row("unbounded-coordinate", "mesh", "positions", oversized.toVariantList(), "invalid-scene3d-mesh");
    row("asset-escape", "asset", "path", "../outside.json", "unsafe-path");
}

void ThemePackageTest::scene3DRejectsUnsafeData()
{
    QFETCH(QString, target);
    QFETCH(QString, key);
    QFETCH(QVariant, value);
    QFETCH(QString, code);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeSceneFixture(root));
    QJsonObject manifest = sceneManifestFixture();
    if (target == QStringLiteral("scene"))
    {
        QJsonObject scene = manifest[QStringLiteral("scene3D")].toObject();
        scene[key] = QJsonValue::fromVariant(value);
        manifest[QStringLiteral("scene3D")] = scene;
    }
    else if (target == QStringLiteral("asset"))
    {
        QJsonArray assets = manifest[QStringLiteral("assets")].toArray();
        QJsonObject asset = assets[1].toObject();
        asset[key] = QJsonValue::fromVariant(value);
        assets[1] = asset;
        manifest[QStringLiteral("assets")] = assets;
    }
    else
    {
        QJsonObject resource = target == QStringLiteral("mesh")
            ? sceneMeshFixture() : sceneMaterialFixture();
        resource[key] = QJsonValue::fromVariant(value);
        QVERIFY(writeBytes(root.filePath(target + QStringLiteral(".json")), QJsonDocument(resource).toJson()));
    }
    const auto result = ThemePackage::loadBytes(QJsonDocument(manifest).toJson(), root.path());
    QVERIFY(!result.isValid());
    QCOMPARE(result.primaryCode(), code);
}

void ThemePackageTest::scene3DMissingAndOversizedResourcesFailClosed()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    for (const QString &file : {QStringLiteral("mesh.json"), QStringLiteral("material.json"),
                                QStringLiteral("texture.svg")})
    {
        QVERIFY(writeSceneFixture(root));
        QVERIFY(QFile::remove(root.filePath(file)));
        const auto result = ThemePackage::loadBytes(QJsonDocument(sceneManifestFixture()).toJson(), root.path());
        QVERIFY(!result.isValid());
        QCOMPARE(result.primaryCode(), QStringLiteral("missing-asset"));
    }
    QVERIFY(writeSceneFixture(root));
    QVERIFY(writeBytes(root.filePath(QStringLiteral("mesh.json")),
                       QByteArray(ThemePackage::MaximumSceneMeshBytes + 1, ' ')));
    const auto oversized = ThemePackage::loadBytes(QJsonDocument(sceneManifestFixture()).toJson(), root.path());
    QVERIFY(!oversized.isValid());
    QCOMPARE(oversized.primaryCode(), QStringLiteral("scene3d-resource-limit"));
}

QTEST_GUILESS_MAIN(ThemePackageTest)

#include "ThemePackageTest.moc"
