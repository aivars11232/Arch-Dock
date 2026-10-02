#include "persistence/ProfileStore.h"
#include "persistence/ConfigurationBackup.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace ArchDock;

namespace
{
ProfileDefinition example()
{
    auto native = PanelDefinition::defaults("bottom", "Bottom", "bottom", true);
    native.visibility.visible = true;
    native.content.applicationIds = {"org.kde.kate.desktop"};
    native.content.urls = {"file:///tmp/example-folder"};
    native.content.entryOrder = native.content.knownEntryIds();
    native.surface.panelThemeId = "sci-fi-chassis-blue";
    native.iconStyle.styleReference = native.iconStyle.themeId = "metallic-blue";
    native.motion.iconProfile = "bounce";
    native.host.nativePanelId = 9;
    native.host.nativeDockAppletId = 10;
    native.host.nativeOwnershipToken = "live-token";
    native.host.nativeRecoveryState = "ready";
    native.settingsRevision = 23;
    native.presetOrigin = PanelPresetOrigin{"old-panel-preset", 4, "old-icon-preset", 2, true, true};
    native.extensions = {{"futureDurable", QVariantMap{{"option", true}}}};
    auto free = PanelDefinition::defaults("free-example", "Free", "free", false);
    free.visibility.visible = true;
    free.layout.pathType = "ring";
    free.layout.radius = 240;
    free.host.screenId = "output:DP-2";
    free.host.screenIndex = 1;
    free.placement.x = 200;
    free.placement.y = 300;
    return ProfileDefinition::capture("Current arrangement", {native.normalized(), free.normalized()});
}
bool writeBytes(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
}

class ProfileStoreTest final : public QObject
{
    Q_OBJECT
private slots:
    void completeConfigurationRoundTrips()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ProfileStore store(directory.filePath("profiles"));
        const auto profile = example();
        QString error;
        const auto saved = store.save(profile, &error);
        QVERIFY2(saved.has_value(), qPrintable(error));
        const auto restored = store.load(profile.id, &error);
        QVERIFY2(restored.has_value(), qPrintable(error));
        QVERIFY(*restored == profile);
        QCOMPARE(restored->panels.first().content, profile.panels.first().content);
        QCOMPARE(restored->panels.first().presetOrigin, profile.panels.first().presetOrigin);
        QCOMPARE(restored->panels.first().extensions, profile.panels.first().extensions);
        QCOMPARE(restored->panels.last().layout.radius, 240);
        QCOMPARE(restored->panels.last().host.screenId, QString("output:DP-2"));
        QCOMPARE(restored->panels.first().host.nativePanelId, -1);
        QVERIFY(restored->panels.first().host.nativeOwnershipToken.isEmpty());
        QCOMPARE(restored->panels.first().settingsRevision, quint64(0));
    }
    void loadingDoesNotCreateOrApplyAnything()
    {
        QTemporaryDir directory;
        const QString root = directory.filePath("absent");
        ProfileStore store(root);
        QVERIFY(store.profiles().isEmpty());
        QVERIFY(!store.load("missing"));
        QVERIFY(!QFileInfo::exists(root));
        auto value = example().toVariantMap();
        auto panel = value["panels"].toList().first().toMap();
        panel.insert("hovered", true);
        value["panels"] = QVariantList{panel};
        QString error;
        QVERIFY(!ProfileDefinition::fromVariantMap(value, &error));
        QCOMPARE(error, QString("profile-runtime-state"));
        panel.remove("hovered");
        panel["nativeOwnershipToken"] = "foreign-token";
        value["panels"] = QVariantList{panel};
        QVERIFY(!ProfileDefinition::fromVariantMap(value, &error));
        QCOMPARE(error, QString("profile-live-host-association"));
    }
    void resourcesFallBackWithoutLosingStoredRequests()
    {
        const auto profile = example();
        QStringList diagnostics;
        const auto effective = profile.resolved({{}, {"plain-original"}, {"none"}}, &diagnostics);
        QVERIFY(effective.first().surface.panelThemeId.isEmpty());
        QCOMPARE(effective.first().surface.rendererTier, QString("procedural2d"));
        QCOMPARE(effective.first().iconStyle.styleReference, QString("plain-original"));
        QCOMPARE(effective.first().motion.iconProfile, QString("none"));
        QCOMPARE(diagnostics.size(), 4); // Native theme/style/motion and free default scale.
        QCOMPARE(profile.panels.first().surface.panelThemeId, QString("sci-fi-chassis-blue"));
        QCOMPARE(profile.panels.first().iconStyle.styleReference, QString("metallic-blue"));
        QVERIFY(profile.panels.first().presetOrigin.has_value());
        const auto resolved = profile.resolved(
            {{"sci-fi-chassis-blue"}, {"plain-original", "metallic-blue"}, {"bounce", "scale"}});
        QVERIFY(resolved == profile.panels);
    }
    void revisionsRejectStaleSave()
    {
        QTemporaryDir directory;
        ProfileStore store(directory.path());
        const auto source = example();
        auto first = store.save(source);
        QVERIFY(first.has_value());
        first->name = "Renamed";
        const auto second = store.save(*first);
        QVERIFY(second.has_value());
        QCOMPARE(second->revision, 2);
        QString error;
        QVERIFY(!store.save(source, &error));
        QCOMPARE(error, QString("stale-profile-revision"));
        QCOMPARE(store.load(source.id)->name, QString("Renamed"));
    }
    void malformedVersionsAndDuplicatePanelsAreRejected()
    {
        auto value = example().toVariantMap();
        QString error;
        value["schemaVersion"] = 2;
        QVERIFY(!ProfileDefinition::fromVariantMap(value, &error));
        QCOMPARE(error, QString("unsupported-profile-version"));
        value["schemaVersion"] = 1;
        auto records = value["panels"].toList();
        records.append(records.first());
        value["panels"] = records;
        QVERIFY(!ProfileDefinition::fromVariantMap(value, &error));
        QCOMPARE(error, QString("duplicate-or-invalid-panel-id"));
        value = example().toVariantMap();
        value["panels"] = QVariantList{};
        QVERIFY(!ProfileDefinition::fromVariantMap(value));
        value = example().toVariantMap();
        value["revision"] = 1.5;
        QVERIFY(!ProfileDefinition::fromVariantMap(value));
        value = example().toVariantMap();
        value["execute"] = "touch /tmp/forbidden";
        QVERIFY(!ProfileDefinition::fromVariantMap(value));
    }
    void nestedLegacyPanelsUseExistingMigration()
    {
        auto value = example().toVariantMap();
        QVariantMap legacy{{"id", "legacy"}, {"name", "Legacy"}, {"edge", "bottom"},
            {"iconSize", 48}, {"contentAppIds", QStringList{"org.kde.kate.desktop"}}};
        value["panels"] = QVariantList{legacy};
        QString error;
        const auto profile = ProfileDefinition::fromVariantMap(value, &error);
        QVERIFY2(profile.has_value(), qPrintable(error));
        QCOMPARE(profile->panels.first().schemaVersion, 2);
        QCOMPARE(profile->panels.first().iconStyle.size, 48);
        QCOMPARE(profile->panels.first().segments.size(), 1);
    }
    void legacyRewriteBacksUpAndCanRestoreExactSource()
    {
        QTemporaryDir directory;
        auto value = example().toVariantMap();
        value["panels"] = QVariantList{QVariantMap{{"id", "legacy"}, {"name", "Legacy"},
            {"edge", "bottom"}, {"iconSize", 48}}};
        const auto bytes = QJsonDocument::fromVariant(value).toJson();
        const auto path = directory.filePath(value["id"].toString() + ".json");
        QVERIFY(writeBytes(path, bytes));
        ProfileStore store(directory.path()); QString error;
        const auto loaded = store.load(value["id"].toString(), &error);
        QVERIFY2(loaded.has_value(), qPrintable(error));
        ConfigurationBackup backup(directory.filePath(".config-backups"),
            {{"profiles", {directory.path(), true}}});
        QVERIFY(backup.backups().isEmpty());
        QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), bytes); original.close();
        const auto saved = store.save(*loaded, &error);
        QVERIFY2(saved.has_value(), qPrintable(error));
        QCOMPARE(saved->revision, loaded->revision + 1);
        QCOMPARE(backup.backups().size(), 1);
        QVERIFY2(backup.restore(backup.backups().first(), &error), qPrintable(error));
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), bytes); original.close();
        QCOMPARE(store.load(loaded->id)->panels.first().iconStyle.size, 48);
        QVERIFY(QDir(directory.filePath(".config-backups")).removeRecursively());
        QVERIFY(writeBytes(directory.filePath(".config-backups"), "blocked"));
        QVERIFY(!store.save(*loaded, &error)); QCOMPARE(error, QString("backup-root-unwritable"));
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), bytes);
    }
    void corruptFilesRemainAndDoNotHideValidProfiles()
    {
        QTemporaryDir directory;
        ProfileStore store(directory.path());
        const auto profile = example();
        QVERIFY(store.save(profile).has_value());
        QVERIFY(writeBytes(directory.filePath("broken.json"), "{"));
        auto future = profile.toVariantMap();
        future["schemaVersion"] = 99;
        const auto bytes = QJsonDocument::fromVariant(future).toJson();
        QVERIFY(writeBytes(directory.filePath("future.json"), bytes));
        QStringList diagnostics;
        QCOMPARE(store.profiles(&diagnostics).size(), 1);
        QCOMPARE(diagnostics.size(), 2);
        QFile retained(directory.filePath("future.json"));
        QVERIFY(retained.open(QIODevice::ReadOnly));
        QCOMPARE(retained.readAll(), bytes);
    }
    void unsafePathsAndUnwritableStoresFailClosed()
    {
        QTemporaryDir directory;
        ProfileStore store(directory.path());
        QVERIFY(!store.load("../outside"));
        QVERIFY(!ProfileDefinition::validId(QStringLiteral("profile-safe\n")));
        const auto profile = example();
        const QString outside = directory.filePath("outside");
        QVERIFY(QDir().mkpath(outside));
        QVERIFY(QFile::link(outside, directory.filePath("linked")));
        ProfileStore linked(directory.filePath("linked/profiles"));
        QString error;
        QVERIFY(!linked.save(profile, &error));
        QCOMPARE(error, QString("unsafe-profile-root"));
        const QString obstruction = directory.filePath("file");
        QVERIFY(writeBytes(obstruction, "preserved"));
        ProfileStore blocked(obstruction + "/profiles");
        QVERIFY(!blocked.save(profile));
        QVERIFY(!QFileInfo::exists(obstruction + "/profiles"));
        const QString path = directory.filePath(profile.id + ".json");
        QVERIFY(QFile::link(obstruction, path));
        QVERIFY(!store.save(profile));
    }
    void crudPersistsAndRejectsStaleActions()
    {
        QTemporaryDir directory;
        ProfileStore store(directory.filePath("profiles"));
        const auto created = store.create("Created", example().panels);
        QVERIFY(created);
        const auto renamed = store.rename(created->id, 1, "Renamed");
        QVERIFY(renamed);
        QCOMPARE(renamed->revision, 2);
        QString error;
        QVERIFY(!store.rename(created->id, 1, "Stale", &error));
        QCOMPARE(error, QString("stale-profile-revision"));
        QVERIFY(!store.remove(created->id, 1, &error));
        const auto copy = store.duplicate(renamed->id, 2, "Duplicate", &error);
        QVERIFY2(copy.has_value(), qPrintable(error));
        QVERIFY(copy->id != renamed->id);
        QCOMPARE(copy->revision, 1);
        QVERIFY(copy->panels == renamed->panels);
        QVERIFY(store.remove(renamed->id, 2, &error));
        QVERIFY(!store.load(renamed->id));
        QCOMPARE(ProfileStore(store.rootDirectory()).profiles().size(), 1);
        QVERIFY(store.load(copy->id));
    }
    void interchangeIsDataOnlyAndImportsNewIdentities()
    {
        QTemporaryDir directory;
        ProfileStore store(directory.filePath("profiles"));
        auto source = example();
        source.panels.first().content.urls = {"https://example.invalid/inert"};
        source.panels.first().content.entryOrder = source.panels.first().content.knownEntryIds();
        const auto saved = store.save(source);
        QVERIFY(saved);
        const QString path = directory.filePath("export.json");
        QString error;
        QVERIFY2(store.exportProfile(saved->id, 1, path, &error), qPrintable(error));
        QVERIFY(!store.exportProfile(saved->id, 1, path, &error)); // No implicit overwrite.
        const auto imported = store.importProfile(path, "Imported", &error);
        QVERIFY2(imported.has_value(), qPrintable(error));
        QVERIFY(imported->id != source.id);
        QCOMPARE(imported->name, QString("Imported"));
        QCOMPARE(imported->revision, 1);
        QCOMPARE(imported->panels.first().content.urls, source.panels.first().content.urls);
        QCOMPARE(imported->panels.first().presetOrigin, source.panels.first().presetOrigin);
        for (qsizetype index = 0; index < imported->panels.size(); ++index)
        {
            const auto &panel = imported->panels.at(index);
            QVERIFY(panel.identity.id != source.panels.at(index).identity.id);
            QVERIFY(!panel.identity.builtIn);
            QCOMPARE(panel.host.nativePanelId, -1);
            QCOMPARE(panel.host.freeDockAppletId, -1);
            QCOMPARE(panel.settingsRevision, quint64(0));
        }
        QVERIFY(imported->panels.last().identity.id.startsWith("free-"));
    }
    void validatedArtworkTravelsInsideTheManagedPackage_data()
    {
        QTest::addColumn<QString>("filename");
        QTest::addColumn<bool>("portable");
        QTest::newRow("unicode-and-spaces") << QString::fromUtf8("panel artwork ē.svg") << true;
        QTest::newRow("encoded-path-name") << QString("panel%20artwork.svg") << false;
    }
    void validatedArtworkTravelsInsideTheManagedPackage()
    {
        QFETCH(QString, filename);
        QFETCH(bool, portable);
        QTemporaryDir directory;
        ProfileStore sourceStore(directory.filePath("source"));
        auto profile = example();
        const QString originalFixture = QFINDTESTDATA("fixtures/theme-v2/assets/surface.svg");
        QVERIFY(!originalFixture.isEmpty());
        const QString fixture = directory.filePath(filename);
        QVERIFY(QFile::copy(originalFixture, fixture));
        profile.panels.first().surface.themeAsset = QUrl::fromLocalFile(fixture).toString();
        profile.panels.first().surface.themeSource = profile.panels.first().surface.themeAsset;
        const QString manifest = QFINDTESTDATA("fixtures/theme-v2/valid-skinned2d-states.json");
        QVERIFY(!manifest.isEmpty());
        profile.panels.first().surface.themePackageManifest = QUrl::fromLocalFile(manifest).toString();
        profile.panels.first().iconStyle.perEntryOverrides["desktop.org.kde.kate.desktop"].customGlyph =
            profile.panels.first().surface.themeAsset;
        const auto saved = sourceStore.save(profile);
        QVERIFY(saved);
        const QString path = directory.filePath("artwork.json");
        QString error;
        if (!portable)
        {
            QVERIFY(!sourceStore.exportProfile(saved->id, 1, path, &error));
            QCOMPARE(error, QString("unsafe-profile-asset-reference"));
            QVERIFY(!QFileInfo::exists(path));
            QVERIFY(!QFileInfo::exists(path + ".assets"));
            return;
        }
        QVERIFY2(sourceStore.exportProfile(saved->id, 1, path, &error), qPrintable(error));
        ProfileStore destination(directory.filePath("destination"));
        const auto imported = destination.importProfile(path, {}, &error);
        QVERIFY2(imported.has_value(), qPrintable(error));
        const auto &surface = imported->panels.first().surface;
        const QString asset = QUrl(surface.themeAsset).toLocalFile();
        QVERIFY(asset.startsWith(destination.rootDirectory() + "/assets/"));
        QVERIFY(QFileInfo::exists(asset));
        QVERIFY(QFileInfo::exists(QUrl(surface.themePackageManifest).toLocalFile()));
        QFile original(fixture), copied(asset);
        QVERIFY(original.open(QIODevice::ReadOnly));
        QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(), original.readAll());
    }
    void importsRejectEscapingPathsAndLinks()
    {
        QTemporaryDir directory;
        ProfileStore store(directory.filePath("profiles"));
        const QString path = directory.filePath("import.json");
        const QString assets = path + ".assets";
        QVERIFY(QDir().mkpath(assets));
        auto profile = example();
        QString error;
        for (const QString &unsafe : {QString("file:///etc/passwd"), QString("https://example.invalid/a.svg"),
            QString("profile-asset:../outside.svg"), QString("profile-asset:/etc/passwd"),
            QString("profile-asset:%2e%2e/outside.svg")})
        {
            profile.panels.first().surface.themeAsset = unsafe;
            QVERIFY(writeBytes(path, QJsonDocument::fromVariant(profile.toVariantMap()).toJson()));
            QVERIFY(!store.importProfile(path, {}, &error));
            QVERIFY(!error.isEmpty());
        }
        const QString outside = directory.filePath("outside.svg");
        QVERIFY(writeBytes(outside, "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'/>"));
        QVERIFY(QFile::link(outside, assets + "/linked.svg"));
        profile.panels.first().surface.themeAsset = "profile-asset:linked.svg";
        QVERIFY(writeBytes(path, QJsonDocument::fromVariant(profile.toVariantMap()).toJson()));
        QVERIFY(!store.importProfile(path, {}, &error));
        QVERIFY(store.profiles().isEmpty());
    }
    void executableArtworkIsRejectedWithoutPartialExport()
    {
        QTemporaryDir directory;
        const QString source = directory.filePath("unsafe.svg");
        QVERIFY(writeBytes(source, "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'><script>alert(1)</script></svg>"));
        auto profile = example();
        profile.panels.first().surface.themeAsset = QUrl::fromLocalFile(source).toString();
        ProfileStore store(directory.filePath("profiles"));
        QVERIFY(store.save(profile));
        const QString path = directory.filePath("export.json");
        QString error;
        QVERIFY(!store.exportProfile(profile.id, 1, path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(path));
        QVERIFY(!QFileInfo::exists(path + ".assets"));
        QVERIFY(store.load(profile.id));
    }
};
QTEST_GUILESS_MAIN(ProfileStoreTest)
#include "ProfileStoreTest.moc"
