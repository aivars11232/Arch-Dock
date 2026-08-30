#include "model/PanelCapabilityResolver.h"
#include "model/PanelDefinition.h"
#include "model/PanelSettingsSchema.h"
#include "PanelRegistry.h"
#include "panel/PanelWindow.h"
#include "ScreenIdentity.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

namespace
{

QByteArray canonicalBytes(const QVariantMap &value)
{
    return QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);
}

QVariantMap settingsSnapshot()
{
    QSettings settings;
    settings.sync();
    QVariantMap result;
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys)
    {
        result.insert(key, settings.value(key));
    }
    return result;
}

ArchDock::CapabilityResolution directResolution(
    const ArchDock::PanelDefinition &definition)
{
    return ArchDock::PanelCapabilityResolver::resolve(
        definition,
        ArchDock::PanelCapabilityResolver::productionHostProfile(
            definition.host.kind),
        ArchDock::PanelCapabilityResolver::proceduralThemeProfile(),
        ArchDock::PanelCapabilityResolver::productionRenderers(),
        ArchDock::PanelCapabilityResolver::productionPlatform());
}

QSet<QString> fieldKeys(const QVariantList &fields)
{
    QSet<QString> result;
    for (const QVariant &value : fields)
    {
        result.insert(value.toMap().value(QStringLiteral("key")).toString());
    }
    return result;
}

QVariantMap fieldByKey(const QVariantList &fields, const QString &key)
{
    for (const QVariant &value : fields)
    {
        const QVariantMap field = value.toMap();
        if (field.value(QStringLiteral("key")).toString() == key)
        {
            return field;
        }
    }
    return {};
}

}

class PanelWindowCapabilityTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void backendResolutionMatchesDirectResolverWithoutWrites();
    void editorSnapshotsExposeOnlyProjectedEditableState();
    void managedVersionTwoCapabilitiesDriveFallbackAndEditorVisibility();
    void rendererProjectionPreservesConsumedValuesWithoutProtectedState();
    void editorDraftResolutionIsReadOnlyAndCannotAuthorizeHiddenState();
    void builtInThemeCandidateCommitsThroughUnifiedTransaction();
    void screenIdentityIsDerivedServerSideAndCannotBeForged();
    void compatibilityConfigurationSurfaceRemainsExactlyBounded();
    void rejectedCapabilityTransactionStopsBeforePersistenceAndHosts();

private:
    QTemporaryDir m_settingsDirectory;
};

void PanelWindowCapabilityTest::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QSettings::setPath(
        QSettings::NativeFormat,
        QSettings::UserScope,
        m_settingsDirectory.path());
}

void PanelWindowCapabilityTest::cleanup()
{
    QSettings settings;
    settings.clear();
    settings.sync();

    const QString themesRoot = QDir(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                   .filePath(QStringLiteral("themes"));
    if (QFileInfo::exists(themesRoot))
    {
        QVERIFY2(QDir(themesRoot).removeRecursively(), qPrintable(themesRoot));
    }
}

void PanelWindowCapabilityTest::backendResolutionMatchesDirectResolverWithoutWrites()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    const QVariantMap configurationBefore = window.dockConfiguration(
        QStringLiteral("bottom"));
    const QVariantMap panelBefore = configurationBefore.value(
        QStringLiteral("panel")).toMap();
    QVERIFY(!panelBefore.isEmpty());
    QString definitionError;
    const std::optional<ArchDock::PanelDefinition> definition =
        ArchDock::PanelDefinition::fromLegacyMap(
            panelBefore, &definitionError);
    QVERIFY2(definition.has_value(), qPrintable(definitionError));
    const QVariantMap settingsBefore = settingsSnapshot();

    const QVariantMap direct = directResolution(*definition).toVariantMap();
    const QVariantMap exposed = window.resolvePanelCapabilities(
        QStringLiteral("bottom"));
    const QVariantMap configurationResolution = configurationBefore.value(
        QStringLiteral("capabilityResolution")).toMap();

    QCOMPARE(canonicalBytes(exposed), canonicalBytes(direct));
    QCOMPARE(canonicalBytes(configurationResolution), canonicalBytes(direct));
    QCOMPARE(configurationBefore.value(QStringLiteral("effectiveRendererTier")).toString(),
             QStringLiteral("procedural2d"));
    QCOMPARE(canonicalBytes(window.resolvePanelCapabilities(
                 QStringLiteral("bottom"))),
             canonicalBytes(exposed));

    QVariantMap candidateRecord = panelBefore;
    candidateRecord.insert(QStringLiteral("opacity"), 0.55);
    candidateRecord.insert(
        QStringLiteral("settingsRevision"),
        QString::number(definition->settingsRevision));
    const std::optional<ArchDock::PanelDefinition> candidate =
        ArchDock::PanelDefinition::fromLegacyMap(
            candidateRecord, &definitionError);
    QVERIFY2(candidate.has_value(), qPrintable(definitionError));
    const QVariantMap exposedDraft = window.resolvePanelCapabilities(
        QStringLiteral("bottom"),
        {{QStringLiteral("opacity"), 0.55}});
    QCOMPARE(canonicalBytes(exposedDraft),
             canonicalBytes(directResolution(*candidate).toVariantMap()));

    const QVariantMap configurationAfter = window.dockConfiguration(
        QStringLiteral("bottom"));
    QCOMPARE(configurationAfter.value(QStringLiteral("panel")).toMap(), panelBefore);
    QCOMPARE(configurationAfter.value(QStringLiteral("settingsRevision")).toULongLong(),
             definition->settingsRevision);
    QCOMPARE(settingsSnapshot(), settingsBefore);
}

void PanelWindowCapabilityTest::editorSnapshotsExposeOnlyProjectedEditableState()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);

    const QVariantMap nativeSnapshot = window.panelSettingsEditorSnapshot(
        QStringLiteral("bottom"), QStringLiteral("native"));
    QVERIFY(nativeSnapshot.value(QStringLiteral("success")).toBool());
    QCOMPARE(nativeSnapshot.value(QStringLiteral("status")).toString(),
             QStringLiteral("loaded"));
    QCOMPARE(nativeSnapshot.value(QStringLiteral("consumer")).toString(),
             QStringLiteral("native"));

    const QVariantList nativeFields = nativeSnapshot.value(
        QStringLiteral("panelFields")).toList();
    const QSet<QString> nativeKeys = fieldKeys(nativeFields);
    QCOMPARE(nativeKeys.size(), 3);
    QVERIFY(nativeKeys.contains(QStringLiteral("visible")));
    QVERIFY(nativeKeys.contains(QStringLiteral("visibilityMode")));
    QVERIFY(nativeKeys.contains(QStringLiteral("acceptDrops")));
    QVERIFY(!nativeKeys.contains(QStringLiteral("layout")));
    QVERIFY(!nativeKeys.contains(QStringLiteral("layoutAngle")));
    QVERIFY(!nativeKeys.contains(QStringLiteral("layoutRadius")));
    QVERIFY(!nativeKeys.contains(QStringLiteral("surface3D")));
    const QVariantMap visibilityMode = fieldByKey(
        nativeFields, QStringLiteral("visibilityMode"));
    QCOMPARE(visibilityMode.value(QStringLiteral("choices")).toStringList(),
             window.nativePanelVisibilityStatus(QStringLiteral("bottom"))
                 .value(QStringLiteral("supportedModes")).toStringList());

    const QSet<QString> nativeGlobalKeys = fieldKeys(nativeSnapshot.value(
        QStringLiteral("globalFields")).toList());
    QCOMPARE(nativeGlobalKeys.size(), 1);
    QVERIFY(nativeGlobalKeys.contains(QStringLiteral("showTooltips")));

    const QVariantMap nativeValues = nativeSnapshot.value(
        QStringLiteral("panelValues")).toMap();
    for (auto it = nativeValues.cbegin(); it != nativeValues.cend(); ++it)
    {
        const auto *descriptor =
            ArchDock::PanelSettingsSchema::panelDescriptor(it.key());
        QVERIFY2(descriptor, qPrintable(it.key()));
        QCOMPARE(descriptor->access,
                 ArchDock::PanelSettingsFieldAccess::Editor);
    }
    for (const QString &protectedOrInternal : {
             QStringLiteral("id"),
             QStringLiteral("builtIn"),
             QStringLiteral("hostKind"),
             QStringLiteral("screenId"),
             QStringLiteral("nativePanelId"),
             QStringLiteral("nativeOwnershipToken"),
             QStringLiteral("nativeRecoveryState"),
             QStringLiteral("physicsEnabled"),
             QStringLiteral("folderLayout"),
             QStringLiteral("pathAnchor"),
             QStringLiteral("surface3D")})
    {
        QVERIFY2(!nativeValues.contains(protectedOrInternal),
                 qPrintable(protectedOrInternal));
    }

    const QVariantMap studioSnapshot = window.panelSettingsEditorSnapshot(
        QStringLiteral("bottom"), QStringLiteral("studio"));
    QVERIFY(studioSnapshot.value(QStringLiteral("success")).toBool());
    const QVariantList studioFields = studioSnapshot.value(
        QStringLiteral("panelFields")).toList();
    const QVariantMap layout = fieldByKey(studioFields, QStringLiteral("layout"));
    QVERIFY(!layout.isEmpty());
    QCOMPARE(layout.value(QStringLiteral("choices")).toStringList(),
             QStringList({QStringLiteral("adaptive"),
                          QStringLiteral("horizontal"),
                          QStringLiteral("vertical")}));
    const QSet<QString> studioKeys = fieldKeys(studioFields);
    QVERIFY(!studioKeys.contains(QStringLiteral("layoutAngle")));
    QVERIFY(!studioKeys.contains(QStringLiteral("layoutRadius")));
    QVERIFY(!studioKeys.contains(QStringLiteral("pathSides")));
    QVERIFY(!studioKeys.contains(QStringLiteral("surface3D")));
}

void PanelWindowCapabilityTest::managedVersionTwoCapabilitiesDriveFallbackAndEditorVisibility()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    PanelRegistry *registry = qobject_cast<PanelRegistry *>(
        engine.rootContext()
            ->contextProperty(QStringLiteral("panelRegistry"))
            .value<QObject *>());
    QVERIFY(registry);

    const QString panelId = registry->addFreePanel();
    QVERIFY(!panelId.isEmpty());
    registry->setPanelValue(
        panelId, QStringLiteral("layout"), QStringLiteral("ring"));
    QCOMPARE(registry->panelValue(panelId, QStringLiteral("layout")).toString(),
             QStringLiteral("ring"));

    const QString manifestPath = QFINDTESTDATA(
        QStringLiteral("fixtures/theme-v2/valid-baked25d-ring.json"));
    QVERIFY(!manifestPath.isEmpty());
    QVERIFY(registry->importTheme(panelId, QUrl::fromLocalFile(manifestPath)));

    const QVariantMap validSnapshot = window.panelSettingsEditorSnapshot(
        panelId, QStringLiteral("studio"));
    QVERIFY(validSnapshot.value(QStringLiteral("success")).toBool());
    const QVariantMap validResolution = validSnapshot.value(
        QStringLiteral("capabilityResolution")).toMap();
    QVERIFY(validResolution.value(QStringLiteral("available")).toBool());
    QCOMPARE(validResolution.value(QStringLiteral("themeId")).toString(),
             QStringLiteral("fixture-baked-ring"));
    const QVariantMap validRenderer = validResolution.value(
        QStringLiteral("renderer")).toMap();
    QCOMPARE(validRenderer.value(QStringLiteral("requestedTier")).toString(),
             QStringLiteral("baked2.5d"));
    QCOMPARE(validRenderer.value(QStringLiteral("effectiveTier")).toString(),
             QStringLiteral("procedural2d"));
    QVERIFY(validRenderer.value(QStringLiteral("fallbackApplied")).toBool());
    QCOMPARE(validRenderer.value(QStringLiteral("reasonCode")).toString(),
             QStringLiteral("renderer-not-installed"));

    const QVariantList validFields = validSnapshot.value(
        QStringLiteral("panelFields")).toList();
    QCOMPARE(fieldByKey(validFields, QStringLiteral("layout"))
                 .value(QStringLiteral("choices")).toStringList(),
             QStringList({QStringLiteral("ring"), QStringLiteral("polygon")}));
    const QSet<QString> validKeys = fieldKeys(validFields);
    QVERIFY(validKeys.contains(QStringLiteral("layoutAngle")));
    QVERIFY(validKeys.contains(QStringLiteral("layoutRadius")));
    QVERIFY(validKeys.contains(QStringLiteral("pathOrientation")));
    QVERIFY(validKeys.contains(QStringLiteral("appearance")));
    QVERIFY(validKeys.contains(QStringLiteral("shape")));
    QVERIFY(validKeys.contains(QStringLiteral("opacity")));
    QVERIFY(validKeys.contains(QStringLiteral("themeFit")));
    QVERIFY(validKeys.contains(QStringLiteral("iconShape")));
    QVERIFY(!validKeys.contains(QStringLiteral("color")));
    QVERIFY(!validKeys.contains(QStringLiteral("layoutRows")));
    QVERIFY(!validKeys.contains(QStringLiteral("pathSides")));

    const QUrl managedManifest(registry->panelValue(
        panelId, QStringLiteral("themePackageManifest")).toString());
    QVERIFY(managedManifest.isLocalFile());
    QFile corruptManifest(managedManifest.toLocalFile());
    QVERIFY(corruptManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(corruptManifest.write(QByteArrayLiteral("not-json")), qint64{8});
    corruptManifest.close();

    const QVariantMap invalidSnapshot = window.panelSettingsEditorSnapshot(
        panelId, QStringLiteral("studio"));
    QVERIFY(invalidSnapshot.value(QStringLiteral("success")).toBool());
    const QVariantMap invalidResolution = invalidSnapshot.value(
        QStringLiteral("capabilityResolution")).toMap();
    QVERIFY(!invalidResolution.value(QStringLiteral("available")).toBool());
    QCOMPARE(invalidResolution.value(QStringLiteral("reasonCode")).toString(),
             QStringLiteral("invalid-capability-input"));
    QVERIFY(invalidResolution.value(QStringLiteral("renderer"))
                .toMap()
                .value(QStringLiteral("effectiveTier"))
                .toString()
                .isEmpty());

    const QSet<QString> invalidKeys = fieldKeys(invalidSnapshot.value(
        QStringLiteral("panelFields")).toList());
    for (const QString &unsupported : {
             QStringLiteral("layout"),
             QStringLiteral("layoutScale"),
             QStringLiteral("layoutAngle"),
             QStringLiteral("layoutRadius"),
             QStringLiteral("pathOrientation"),
             QStringLiteral("appearance"),
             QStringLiteral("shape"),
             QStringLiteral("opacity"),
             QStringLiteral("color"),
             QStringLiteral("themeFit"),
             QStringLiteral("iconShape"),
             QStringLiteral("iconSize"),
             QStringLiteral("spacing")})
    {
        QVERIFY2(!invalidKeys.contains(unsupported), qPrintable(unsupported));
    }
}

void PanelWindowCapabilityTest::rendererProjectionPreservesConsumedValuesWithoutProtectedState()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    const QVariantMap legacy = window.dockConfiguration(QStringLiteral("bottom"));
    const QVariantMap renderer = window.panelRendererConfiguration(
        QStringLiteral("bottom"));
    QVERIFY(!renderer.isEmpty());

    const QStringList consumedKeys{
        QStringLiteral("acceptDrops"),
        QStringLiteral("animationDuration"),
        QStringLiteral("animationIntensity"),
        QStringLiteral("animationSpeed"),
        QStringLiteral("animationTrigger"),
        QStringLiteral("appearance"),
        QStringLiteral("color"),
        QStringLiteral("iconAnimation"),
        QStringLiteral("iconShape"),
        QStringLiteral("iconSize"),
        QStringLiteral("layout"),
        QStringLiteral("layoutAngle"),
        QStringLiteral("layoutPadding"),
        QStringLiteral("layoutRadius"),
        QStringLiteral("layoutRows"),
        QStringLiteral("layoutScale"),
        QStringLiteral("magnification"),
        QStringLiteral("magnificationEnabled"),
        QStringLiteral("opacity"),
        QStringLiteral("pathOrientation"),
        QStringLiteral("pathSides"),
        QStringLiteral("reducedMotion"),
        QStringLiteral("showIndicators"),
        QStringLiteral("showReflections"),
        QStringLiteral("showTooltips"),
        QStringLiteral("spacing"),
        QStringLiteral("themeAsset"),
    };
    for (const QString &key : consumedKeys)
    {
        QVERIFY2(renderer.contains(key), qPrintable(key));
        QCOMPARE(renderer.value(key), legacy.value(key));
    }
    QVERIFY(renderer.contains(QStringLiteral("capabilityResolution")));
    QVERIFY(renderer.contains(QStringLiteral("effectiveRendererTier")));

    for (const QString &protectedOrDiagnostic : {
             QStringLiteral("id"),
             QStringLiteral("builtIn"),
             QStringLiteral("hostKind"),
             QStringLiteral("screenId"),
             QStringLiteral("nativePanelId"),
             QStringLiteral("nativeOwnershipToken"),
             QStringLiteral("nativeRecoveryState"),
             QStringLiteral("surface3D"),
             QStringLiteral("themeStatus")})
    {
        QVERIFY2(!renderer.contains(protectedOrDiagnostic),
                 qPrintable(protectedOrDiagnostic));
    }
}

void PanelWindowCapabilityTest::editorDraftResolutionIsReadOnlyAndCannotAuthorizeHiddenState()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    const QVariantMap snapshot = window.panelSettingsEditorSnapshot(
        QStringLiteral("bottom"), QStringLiteral("native"));
    QVERIFY(snapshot.value(QStringLiteral("success")).toBool());
    const quint64 revision = snapshot.value(QStringLiteral("revision")).toULongLong();
    const QVariantMap panelBefore = window.dockConfiguration(
        QStringLiteral("bottom")).value(QStringLiteral("panel")).toMap();
    const QVariantMap settingsBefore = settingsSnapshot();

    const QVariantMap resolved = window.resolvePanelSettingsEditorDraft(
        QStringLiteral("bottom"),
        revision,
        {{QStringLiteral("opacity"), 0.42}},
        {},
        QStringLiteral("native"));
    QVERIFY(resolved.value(QStringLiteral("success")).toBool());
    QCOMPARE(resolved.value(QStringLiteral("status")).toString(),
             QStringLiteral("resolved"));
    QCOMPARE(resolved.value(QStringLiteral("consumer")).toString(),
             QStringLiteral("native"));
    QCOMPARE(resolved.value(QStringLiteral("candidateRevision")).toULongLong(),
             revision + 1);
    QVERIFY(!resolved.value(QStringLiteral("panelValues")).toMap().contains(
        QStringLiteral("opacity")));
    QCOMPARE(window.dockConfiguration(QStringLiteral("bottom"))
                 .value(QStringLiteral("panel")).toMap(),
             panelBefore);
    QCOMPARE(settingsSnapshot(), settingsBefore);

    const QVariantMap protectedResult = window.resolvePanelSettingsEditorDraft(
        QStringLiteral("bottom"),
        revision,
        {{QStringLiteral("id"), QStringLiteral("forged")}},
        {},
        QStringLiteral("studio"));
    QVERIFY(!protectedResult.value(QStringLiteral("success")).toBool());
    QCOMPARE(protectedResult.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("protected-panel-field"));

    const QVariantMap hiddenResult = window.resolvePanelSettingsEditorDraft(
        QStringLiteral("bottom"),
        revision,
        {{QStringLiteral("surface3D"),
          QVariantMap{{QStringLiteral("depth"), 12}}}},
        {},
        QStringLiteral("studio"));
    QVERIFY(!hiddenResult.value(QStringLiteral("success")).toBool());
    QCOMPARE(hiddenResult.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("unavailable-panel-field"));

    const QVariantMap unavailableResult = window.resolvePanelSettingsEditorDraft(
        QStringLiteral("bottom"),
        revision,
        {{QStringLiteral("layoutRadius"), 240}},
        {},
        QStringLiteral("studio"));
    QVERIFY(!unavailableResult.value(QStringLiteral("success")).toBool());
    QCOMPARE(unavailableResult.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("unavailable-panel-field"));

    QCOMPARE(window.dockConfiguration(QStringLiteral("bottom"))
                 .value(QStringLiteral("panel")).toMap(),
             panelBefore);
    QCOMPARE(settingsSnapshot(), settingsBefore);
}

void PanelWindowCapabilityTest::builtInThemeCandidateCommitsThroughUnifiedTransaction()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    PanelRegistry *registry = qobject_cast<PanelRegistry *>(
        engine.rootContext()
            ->contextProperty(QStringLiteral("panelRegistry"))
            .value<QObject *>());
    QVERIFY(registry);

    const QVariantMap snapshot = window.panelSettingsEditorSnapshot(
        QStringLiteral("bottom"), QStringLiteral("studio"));
    const quint64 revision = snapshot.value(QStringLiteral("revision")).toULongLong();
    const QVariantMap theme = registry->themeCandidate(
        QStringLiteral("bottom"),
        QStringLiteral("obsidian-glass"),
        QStringLiteral("complete"));
    QVERIFY(theme.value(QStringLiteral("success")).toBool());
    const QVariantMap values = theme.value(QStringLiteral("values")).toMap();
    QVERIFY(!values.isEmpty());
    QVERIFY(!values.contains(QStringLiteral("id")));
    QVERIFY(!values.contains(QStringLiteral("builtIn")));
    QVERIFY(!values.contains(QStringLiteral("screenId")));
    QVERIFY(!values.contains(QStringLiteral("surface3D")));

    const QVariantMap result = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"), revision, values, {});
    QVERIFY(result.value(QStringLiteral("success")).toBool());
    QCOMPARE(result.value(QStringLiteral("status")).toString(),
             QStringLiteral("succeeded"));
    QCOMPARE(result.value(QStringLiteral("revision")).toULongLong(),
             revision + 1);
    QVERIFY(!result.contains(QStringLiteral("panelValues")));

    const QVariantMap persisted = window.dockConfiguration(
        QStringLiteral("bottom")).value(QStringLiteral("panel")).toMap();
    QCOMPARE(persisted.value(QStringLiteral("completeThemeId")).toString(),
             QStringLiteral("obsidian-glass"));
    QCOMPARE(persisted.value(QStringLiteral("appearance")).toString(),
             QStringLiteral("glass"));
    QCOMPARE(persisted.value(QStringLiteral("opacity")).toReal(), 0.88);
    QCOMPARE(persisted.value(QStringLiteral("settingsRevision")).toULongLong(),
             revision + 1);
}

void PanelWindowCapabilityTest::screenIdentityIsDerivedServerSideAndCannotBeForged()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    QVERIFY(QGuiApplication::primaryScreen());
    const QString expectedScreenId = ArchDock::persistentScreenId(
        QGuiApplication::primaryScreen());
    const QVariantMap snapshot = window.panelSettingsEditorSnapshot(
        QStringLiteral("bottom"), QStringLiteral("studio"));
    const quint64 revision = snapshot.value(QStringLiteral("revision")).toULongLong();

    const QVariantMap applied = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"),
        revision,
        {{QStringLiteral("screen"), 0}},
        {});
    QVERIFY(applied.value(QStringLiteral("success")).toBool());
    QCOMPARE(applied.value(QStringLiteral("status")).toString(),
             QStringLiteral("succeeded"));
    const quint64 appliedRevision = applied.value(
        QStringLiteral("revision")).toULongLong();
    QCOMPARE(appliedRevision, revision + 1);
    const QVariantMap persisted = window.dockConfiguration(
        QStringLiteral("bottom")).value(QStringLiteral("panel")).toMap();
    QCOMPARE(persisted.value(QStringLiteral("screen")).toInt(), 0);
    QCOMPARE(persisted.value(QStringLiteral("screenId")).toString(),
             expectedScreenId);

    const QVariantMap forged = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"),
        appliedRevision,
        {{QStringLiteral("screenId"), QStringLiteral("forged-client-id")}},
        {});
    QVERIFY(!forged.value(QStringLiteral("success")).toBool());
    QCOMPARE(forged.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("protected-panel-field"));
    const QVariantMap afterForgery = window.dockConfiguration(
        QStringLiteral("bottom")).value(QStringLiteral("panel")).toMap();
    QCOMPARE(afterForgery.value(QStringLiteral("settingsRevision")).toULongLong(),
             appliedRevision);
    QCOMPARE(afterForgery.value(QStringLiteral("screenId")).toString(),
             expectedScreenId);
}

void PanelWindowCapabilityTest::compatibilityConfigurationSurfaceRemainsExactlyBounded()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    const QVariantMap before = window.dockConfiguration(
        QStringLiteral("bottom")).value(QStringLiteral("panel")).toMap();
    const quint64 revisionBefore = before.value(
        QStringLiteral("settingsRevision")).toULongLong();

    QVERIFY(window.setDockConfiguration(
        QStringLiteral("bottom"), QStringLiteral("opacity"), 0.43));
    const QVariantMap applied = window.dockConfiguration(
        QStringLiteral("bottom")).value(QStringLiteral("panel")).toMap();
    QCOMPARE(applied.value(QStringLiteral("opacity")).toReal(), 0.43);
    QCOMPARE(applied.value(QStringLiteral("settingsRevision")).toULongLong(),
             revisionBefore + 1);

    QVERIFY(!window.setDockConfiguration(
        QStringLiteral("bottom"), QStringLiteral("layout"),
        QStringLiteral("horizontal")));
    QVERIFY(!window.setDockConfiguration(
        QStringLiteral("bottom"), QStringLiteral("physicsEnabled"), true));
    const QVariantMap rejected = window.dockConfiguration(
        QStringLiteral("bottom")).value(QStringLiteral("panel")).toMap();
    QCOMPARE(rejected.value(QStringLiteral("layout")).toString(),
             applied.value(QStringLiteral("layout")).toString());
    QCOMPARE(rejected.value(QStringLiteral("settingsRevision")).toULongLong(),
             revisionBefore + 1);
}

void PanelWindowCapabilityTest::rejectedCapabilityTransactionStopsBeforePersistenceAndHosts()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    const QVariantMap configurationBefore = window.dockConfiguration(
        QStringLiteral("bottom"));
    const QVariantMap panelBefore = configurationBefore.value(
        QStringLiteral("panel")).toMap();
    const quint64 revisionBefore = panelBefore.value(
        QStringLiteral("settingsRevision")).toULongLong();
    const QVariantMap settingsBefore = settingsSnapshot();

    const QVariantMap outcome = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"),
        revisionBefore,
        {{QStringLiteral("layout"), QStringLiteral("ring")}},
        {});

    QCOMPARE(outcome.value(QStringLiteral("status")).toString(),
             QStringLiteral("validation-failed"));
    QCOMPARE(outcome.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("capability-unavailable"));
    QCOMPARE(outcome.value(QStringLiteral("revision")).toULongLong(),
             revisionBefore);
    QCOMPARE(outcome.value(QStringLiteral("hostResults")).toList().size(), 0);
    const QVariantMap resolution = outcome.value(
        QStringLiteral("capabilityResolution")).toMap();
    QVERIFY(!resolution.value(QStringLiteral("available")).toBool());
    QCOMPARE(resolution.value(QStringLiteral("reasonCode")).toString(),
             QStringLiteral("host-layout-unsupported"));

    const QVariantMap configurationAfter = window.dockConfiguration(
        QStringLiteral("bottom"));
    QCOMPARE(configurationAfter.value(QStringLiteral("panel")).toMap(), panelBefore);
    QCOMPARE(configurationAfter.value(QStringLiteral("settingsRevision")).toULongLong(),
             revisionBefore);
    QCOMPARE(settingsSnapshot(), settingsBefore);
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    qputenv(
        "DBUS_SESSION_BUS_ADDRESS",
        QByteArrayLiteral("unix:path=/nonexistent/archdock-phase-b-session-bus"));
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ArchDockTests"));
    QCoreApplication::setApplicationName(QStringLiteral("PanelWindowCapabilityTest"));
    PanelWindowCapabilityTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "PanelWindowCapabilityTest.moc"
