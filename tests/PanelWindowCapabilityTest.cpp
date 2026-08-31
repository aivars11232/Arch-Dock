#include "model/PanelCapabilityResolver.h"
#include "model/PanelDefinition.h"
#include "model/PanelSettingsSchema.h"
#include "PanelRegistry.h"
#include "panel/PanelWindow.h"
#include "ScreenIdentity.h"
#include "WindowModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
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
    void builtInChassisCandidateProjectsIntoStudioAndRenderer();
    void builtInEnergyCandidateProjectsGlowAndTheme();
    void screenIdentityIsDerivedServerSideAndCannotBeForged();
    void compatibilityConfigurationSurfaceRemainsExactlyBounded();
    void rejectedCapabilityTransactionStopsBeforePersistenceAndHosts();
    void iconOverridesCommitResolveAndResetOneEntryOnly();
    void iconPropertiesPublicInteractionIsTransactional();
    void runningOnlyIconPropertiesAreUnavailable();

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
    QCOMPARE(nativeKeys.size(), 4);
    QVERIFY(nativeKeys.contains(QStringLiteral("visible")));
    QVERIFY(nativeKeys.contains(QStringLiteral("visibilityMode")));
    QVERIFY(nativeKeys.contains(QStringLiteral("acceptDrops")));
    QVERIFY(nativeKeys.contains(QStringLiteral("iconStyle")));
    QVERIFY(!nativeKeys.contains(QStringLiteral("layout")));
    QVERIFY(!nativeKeys.contains(QStringLiteral("layoutAngle")));
    QVERIFY(!nativeKeys.contains(QStringLiteral("layoutRadius")));
    QVERIFY(!nativeKeys.contains(QStringLiteral("surface3D")));
    const QVariantMap visibilityMode = fieldByKey(
        nativeFields, QStringLiteral("visibilityMode"));
    QCOMPARE(visibilityMode.value(QStringLiteral("choices")).toStringList(),
             window.nativePanelVisibilityStatus(QStringLiteral("bottom"))
                 .value(QStringLiteral("supportedModes")).toStringList());
    const QVariantMap iconStyle = fieldByKey(
        nativeFields, QStringLiteral("iconStyle"));
    QCOMPARE(iconStyle.value(QStringLiteral("choices")).toStringList(),
             QStringList({QStringLiteral("plain-original"),
                          QStringLiteral("metallic-blue"),
                          QStringLiteral("metallic-red"),
                          QStringLiteral("neon-green"),
                          QStringLiteral("neon-orange"),
                          QStringLiteral("dark-orb")}));
    const QVariantList iconStyleOptions = iconStyle.value(
        QStringLiteral("options")).toList();
    QCOMPARE(iconStyleOptions.size(), 6);
    QCOMPARE(iconStyleOptions.constFirst().toMap()
                 .value(QStringLiteral("label")).toString(),
             QStringLiteral("Plain Original"));
    QCOMPARE(iconStyleOptions.constLast().toMap()
                 .value(QStringLiteral("label")).toString(),
             QStringLiteral("Dark Orb"));

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
             QStringLiteral("iconThemeId"),
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
    QCOMPARE(studioSnapshot.value(
                 QStringLiteral("iconStyleProjectionStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(studioSnapshot.value(QStringLiteral("iconStyleDefinition"))
                 .toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("plain-original"));
    QVERIFY(!studioSnapshot.value(QStringLiteral("iconStyles")).toList().isEmpty());
    QVERIFY(!window.iconStyleDefinitions().isEmpty());
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
    QCOMPARE(validSnapshot.value(
                 QStringLiteral("themeProjectionStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(validSnapshot.value(QStringLiteral("themeDefinition"))
                 .toMap()
                 .value(QStringLiteral("id"))
                 .toString(),
             QStringLiteral("fixture-baked-ring"));
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

    const QVariantMap validRendererConfiguration =
        window.panelRendererConfiguration(panelId);
    QCOMPARE(validRendererConfiguration.value(
                 QStringLiteral("themeProjectionStatus")).toString(),
             QStringLiteral("ready"));
    QVERIFY(validRendererConfiguration.value(
        QStringLiteral("themeProjectionError")).toString().isEmpty());
    const QVariantMap validThemeDefinition = validRendererConfiguration.value(
        QStringLiteral("themeDefinition")).toMap();
    QCOMPARE(validThemeDefinition.value(QStringLiteral("id")).toString(),
             QStringLiteral("fixture-baked-ring"));
    QVERIFY(validThemeDefinition.value(QStringLiteral("valid")).toBool());
    QVERIFY(!validThemeDefinition.value(
        QStringLiteral("assetPaths")).toMap().isEmpty());

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
    QCOMPARE(invalidSnapshot.value(
                 QStringLiteral("themeProjectionStatus")).toString(),
             QStringLiteral("error"));
    QCOMPARE(invalidSnapshot.value(
                 QStringLiteral("themeProjectionError")).toString(),
             QStringLiteral("invalid-json"));
    QVERIFY(invalidSnapshot.value(
        QStringLiteral("themeDefinition")).toMap().isEmpty());
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
    const QVariantMap invalidRendererConfiguration =
        window.panelRendererConfiguration(panelId);
    QCOMPARE(invalidRendererConfiguration.value(
                 QStringLiteral("themeProjectionStatus")).toString(),
             QStringLiteral("error"));
    QCOMPARE(invalidRendererConfiguration.value(
                 QStringLiteral("themeProjectionError")).toString(),
             QStringLiteral("invalid-json"));
    QVERIFY(invalidRendererConfiguration.value(
        QStringLiteral("themeDefinition")).toMap().isEmpty());

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
        QStringLiteral("iconStyle"),
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
    QCOMPARE(renderer.value(
                 QStringLiteral("themeProjectionStatus")).toString(),
             QStringLiteral("unavailable"));
    QVERIFY(renderer.value(
        QStringLiteral("themeProjectionError")).toString().isEmpty());
    QVERIFY(renderer.value(QStringLiteral("themeDefinition")).toMap().isEmpty());
    QCOMPARE(renderer.value(
                 QStringLiteral("iconStyleProjectionStatus")).toString(),
             QStringLiteral("ready"));
    QVERIFY(renderer.value(
        QStringLiteral("iconStyleProjectionError")).toString().isEmpty());
    const QVariantMap iconStyleDefinition = renderer.value(
        QStringLiteral("iconStyleDefinition")).toMap();
    QCOMPARE(iconStyleDefinition.value(QStringLiteral("id")).toString(),
             QStringLiteral("plain-original"));
    QVERIFY(iconStyleDefinition.value(QStringLiteral("valid")).toBool());

    for (const QString &protectedOrDiagnostic : {
             QStringLiteral("id"),
             QStringLiteral("builtIn"),
             QStringLiteral("hostKind"),
             QStringLiteral("screenId"),
             QStringLiteral("nativePanelId"),
             QStringLiteral("nativeOwnershipToken"),
             QStringLiteral("nativeRecoveryState"),
             QStringLiteral("iconThemeId"),
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
    QVERIFY2(result.value(QStringLiteral("success")).toBool(),
             qPrintable(QStringLiteral("%1/%2: %3")
                            .arg(result.value(QStringLiteral("status")).toString(),
                                 result.value(QStringLiteral("errorCode")).toString(),
                                 result.value(QStringLiteral("errorMessage")).toString())));
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

void PanelWindowCapabilityTest::builtInChassisCandidateProjectsIntoStudioAndRenderer()
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
    QVERIFY(snapshot.value(QStringLiteral("success")).toBool());
    const quint64 revision = snapshot.value(QStringLiteral("revision")).toULongLong();
    const QVariantList themes = snapshot.value(QStringLiteral("themes")).toList();
    QCOMPARE(themes.size(), 12);
    int chassisThemeCount = 0;
    for (const QVariant &value : themes)
    {
        const QVariantMap theme = value.toMap();
        if (theme.value(QStringLiteral("category")).toString() !=
            QStringLiteral("chassis"))
        {
            continue;
        }
        ++chassisThemeCount;
        QVERIFY(theme.value(QStringLiteral("available")).toBool());
        QCOMPARE(theme.value(
                     QStringLiteral("themeProjectionStatus")).toString(),
                 QStringLiteral("ready"));
        QVERIFY(theme.value(QStringLiteral("valid")).toBool());
        QVERIFY(!theme.value(
            QStringLiteral("assetPaths")).toMap().isEmpty());
        QCOMPARE(theme.value(QStringLiteral("previewConfiguration"))
                     .toMap()
                     .value(QStringLiteral("seed"))
                     .toString(),
                 QStringLiteral("chassis-family-v1"));
    }
    QCOMPARE(chassisThemeCount, 3);

    const QVariantMap theme = registry->themeCandidate(
        QStringLiteral("bottom"),
        QStringLiteral("sci-fi-chassis-red"),
        QStringLiteral("complete"));
    QVERIFY(theme.value(QStringLiteral("success")).toBool());
    const QVariantMap values = theme.value(QStringLiteral("values")).toMap();
    QCOMPARE(values.value(QStringLiteral("rendererTier")).toString(),
             QStringLiteral("skinned2d"));
    QCOMPARE(values.value(QStringLiteral("layout")).toString(),
             QStringLiteral("horizontal"));

    const QVariantMap result = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"), revision, values, {});
    QVERIFY2(result.value(QStringLiteral("success")).toBool(),
             qPrintable(QStringLiteral("%1/%2: %3")
                            .arg(result.value(QStringLiteral("status")).toString(),
                                 result.value(QStringLiteral("errorCode")).toString(),
                                 result.value(QStringLiteral("errorMessage")).toString())));
    QCOMPARE(result.value(QStringLiteral("status")).toString(),
             QStringLiteral("succeeded"));

    const QVariantMap renderer = window.panelRendererConfiguration(
        QStringLiteral("bottom"));
    QCOMPARE(renderer.value(QStringLiteral("effectiveRendererTier")).toString(),
             QStringLiteral("skinned2d"));
    QCOMPARE(renderer.value(
                 QStringLiteral("themeProjectionStatus")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(renderer.value(QStringLiteral("themeDefinition"))
                 .toMap()
                 .value(QStringLiteral("id"))
                 .toString(),
             QStringLiteral("sci-fi-chassis-red"));
    QCOMPARE(renderer.value(QStringLiteral("layout")).toString(),
             QStringLiteral("horizontal"));
}

void PanelWindowCapabilityTest::builtInEnergyCandidateProjectsGlowAndTheme()
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
    QVERIFY(snapshot.value(QStringLiteral("success")).toBool());
    const quint64 revision = snapshot.value(QStringLiteral("revision")).toULongLong();

    const QVariantMap expectedTints{
        {QStringLiteral("energy-frame-cyan"), QStringLiteral("#44ddea")},
        {QStringLiteral("energy-frame-green"), QStringLiteral("#4ee68a")},
        {QStringLiteral("energy-frame-orange"), QStringLiteral("#ff873c")},
        {QStringLiteral("energy-frame-purple"), QStringLiteral("#b96cff")},
    };
    QVariantMap projectedThemes;
    for (const QVariant &value : snapshot.value(QStringLiteral("themes")).toList())
    {
        const QVariantMap candidate = value.toMap();
        const QString candidateId = candidate.value(QStringLiteral("id")).toString();
        if (expectedTints.contains(candidateId))
        {
            projectedThemes.insert(candidateId, candidate);
        }
    }
    QCOMPARE(projectedThemes.size(), expectedTints.size());

    QVariantMap cyanValues;
    for (auto iterator = expectedTints.constBegin();
         iterator != expectedTints.constEnd(); ++iterator)
    {
        const QString themeId = iterator.key();
        const QVariantMap projectedTheme = projectedThemes.value(themeId).toMap();
        QVERIFY2(!projectedTheme.isEmpty(), qPrintable(themeId));
        QVERIFY2(projectedTheme.value(QStringLiteral("available")).toBool(),
                 qPrintable(themeId));
        QCOMPARE(projectedTheme.value(
                     QStringLiteral("themeProjectionStatus")).toString(),
                 QStringLiteral("ready"));
        QVERIFY2(projectedTheme.value(QStringLiteral("valid")).toBool(),
                 qPrintable(themeId));
        QVERIFY(projectedTheme.value(QStringLiteral("capabilities"))
                    .toMap()
                    .value(QStringLiteral("features"))
                    .toList()
                    .contains(QStringLiteral("dynamic-glow")));

        const QVariantMap theme = registry->themeCandidate(
            QStringLiteral("bottom"), themeId, QStringLiteral("complete"));
        QVERIFY2(theme.value(QStringLiteral("success")).toBool(),
                 qPrintable(themeId));
        const QVariantMap values = theme.value(QStringLiteral("values")).toMap();
        QCOMPARE(values.value(QStringLiteral("rendererTier")).toString(),
                 QStringLiteral("skinned2d"));
        QCOMPARE(values.value(QStringLiteral("color")).toString(),
                 iterator.value().toString());
        QCOMPARE(values.value(QStringLiteral("glowIntensity")).toReal(), 1.15);
        if (themeId == QStringLiteral("energy-frame-cyan"))
        {
            cyanValues = values;
        }
    }
    QVERIFY(!cyanValues.isEmpty());

    const QVariantMap result = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"), revision, cyanValues, {});
    QVERIFY2(result.value(QStringLiteral("success")).toBool(),
             qPrintable(result.value(QStringLiteral("errorMessage")).toString()));

    const QVariantMap renderer = window.panelRendererConfiguration(
        QStringLiteral("bottom"));
    QCOMPARE(renderer.value(QStringLiteral("effectiveRendererTier")).toString(),
             QStringLiteral("skinned2d"));
    QCOMPARE(renderer.value(QStringLiteral("color")).toString(),
             QStringLiteral("#44ddea"));
    QCOMPARE(renderer.value(QStringLiteral("glowIntensity")).toReal(), 1.15);
    const QVariantMap runtimeTheme = renderer.value(
        QStringLiteral("themeDefinition")).toMap();
    QCOMPARE(runtimeTheme.value(QStringLiteral("id")).toString(),
             QStringLiteral("energy-frame-cyan"));
    QVERIFY(runtimeTheme.value(QStringLiteral("capabilities"))
                .toMap()
                .value(QStringLiteral("features"))
                .toList()
                .contains(QStringLiteral("dynamic-glow")));
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

void PanelWindowCapabilityTest::iconOverridesCommitResolveAndResetOneEntryOnly()
{
    QTemporaryDir desktopEntries;
    QVERIFY(desktopEntries.isValid());
    const auto writeDesktopEntry = [&desktopEntries](
        const QString &fileName,
        const QString &name,
        const QString &iconName)
    {
        const QString path = desktopEntries.filePath(fileName);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return QString{};
        }
        file.write("[Desktop Entry]\nType=Application\nName=");
        file.write(name.toUtf8());
        file.write("\nIcon=");
        file.write(iconName.toUtf8());
        file.write("\nExec=/bin/true\n");
        file.close();
        return path;
    };
    const QString firstPath = writeDesktopEntry(
        QStringLiteral("org.example.first.desktop"),
        QStringLiteral("First app"),
        QStringLiteral("applications-system"));
    const QString secondPath = writeDesktopEntry(
        QStringLiteral("org.example.second.desktop"),
        QStringLiteral("Second app"),
        QStringLiteral("utilities-terminal"));
    QVERIFY(!firstPath.isEmpty());
    QVERIFY(!secondPath.isEmpty());

    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    QVERIFY(window.pinDockUrls({QUrl::fromLocalFile(firstPath).toString(),
                               QUrl::fromLocalFile(secondPath).toString()}));
    QVariantList entries = window.dockEntriesForPanel(
        QStringLiteral("bottom"), QStringLiteral("hybrid"));
    QCOMPARE(entries.size(), 2);
    const QVariantMap first = entries.at(0).toMap();
    const QVariantMap second = entries.at(1).toMap();
    QVERIFY(first.value(QStringLiteral("iconPropertiesSupported")).toBool());
    QVERIFY(second.value(QStringLiteral("iconPropertiesSupported")).toBool());
    const QVariantMap snapshot = window.iconOverrideSnapshot(
        QStringLiteral("bottom"), first);
    QVERIFY(snapshot.value(QStringLiteral("success")).toBool());
    QCOMPARE(snapshot.value(QStringLiteral("status")).toString(),
             QStringLiteral("loaded"));
    const QString firstIdentity = snapshot.value(
        QStringLiteral("entryIdentity")).toString();
    QVERIFY(!firstIdentity.isEmpty());
    const QVariantMap stableSnapshot = window.iconOverrideSnapshotForIdentity(
        QStringLiteral("bottom"), firstIdentity);
    QVERIFY(stableSnapshot.value(QStringLiteral("success")).toBool());
    QCOMPARE(stableSnapshot.value(QStringLiteral("entryIdentity")).toString(),
             firstIdentity);
    QCOMPARE(stableSnapshot.value(QStringLiteral("baseGlyph")).toString(),
             first.value(QStringLiteral("baseIconName")).toString());
    QCOMPARE(stableSnapshot.value(QStringLiteral("baseLabel")).toString(),
             first.value(QStringLiteral("baseDisplayName")).toString());
    const quint64 revision = snapshot.value(
        QStringLiteral("revision")).toULongLong();

    const QVariantMap applied = window.applyIconOverrideTransaction(
        QStringLiteral("bottom"),
        revision,
        firstIdentity,
        {
            {QStringLiteral("customGlyph"),
             QStringLiteral("file:///missing/window-test.svg")},
            {QStringLiteral("customLabel"), QStringLiteral("Only first")},
            {QStringLiteral("tileEnabled"), false},
            {QStringLiteral("styleReference"), QStringLiteral("dark-orb")},
            {QStringLiteral("animationProfileReference"),
             QStringLiteral("future-orbit")},
        });
    QVERIFY2(applied.value(QStringLiteral("success")).toBool(),
             qPrintable(applied.value(QStringLiteral("errorMessage")).toString()));
    QCOMPARE(applied.value(QStringLiteral("status")).toString(),
             QStringLiteral("succeeded"));
    QCOMPARE(applied.value(QStringLiteral("revision")).toULongLong(),
             revision + 1);

    entries = window.dockEntriesForPanel(
        QStringLiteral("bottom"), QStringLiteral("hybrid"));
    QCOMPARE(entries.size(), 2);
    const QVariantMap resolvedFirst = entries.at(0).toMap();
    const QVariantMap resolvedSecond = entries.at(1).toMap();
    QCOMPARE(resolvedFirst.value(QStringLiteral("stableIdentity")).toString(),
             firstIdentity);
    QVERIFY(resolvedFirst.value(
        QStringLiteral("iconOverrideApplied")).toBool());
    QCOMPARE(resolvedFirst.value(QStringLiteral("iconName")).toString(),
             QStringLiteral("applications-system"));
    QCOMPARE(resolvedFirst.value(QStringLiteral("displayName")).toString(),
             QStringLiteral("Only first"));
    QVERIFY(!resolvedFirst.value(QStringLiteral("tileEnabled")).toBool());
    QCOMPARE(resolvedFirst.value(
                 QStringLiteral("resolvedIconStyleDefinition")).toMap()
                 .value(QStringLiteral("id")).toString(),
             QStringLiteral("dark-orb"));
    QVERIFY(!resolvedSecond.value(
        QStringLiteral("iconOverrideApplied")).toBool());
    QCOMPARE(resolvedSecond.value(QStringLiteral("iconName")).toString(),
             second.value(QStringLiteral("iconName")).toString());
    QCOMPARE(resolvedSecond.value(QStringLiteral("displayName")).toString(),
             second.value(QStringLiteral("displayName")).toString());

    const QVariantMap stale = window.applyIconOverrideTransaction(
        QStringLiteral("bottom"),
        revision,
        firstIdentity,
        {{QStringLiteral("customLabel"), QStringLiteral("Stale")}});
    QVERIFY(!stale.value(QStringLiteral("success")).toBool());
    QCOMPARE(stale.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("stale-revision"));

    const QVariantMap reset = window.resetIconOverrideTransaction(
        QStringLiteral("bottom"), revision + 1, firstIdentity);
    QVERIFY(reset.value(QStringLiteral("success")).toBool());
    QCOMPARE(reset.value(QStringLiteral("revision")).toULongLong(),
             revision + 2);
    entries = window.dockEntriesForPanel(
        QStringLiteral("bottom"), QStringLiteral("hybrid"));
    QVERIFY(!entries.at(0).toMap().value(
        QStringLiteral("iconOverrideApplied")).toBool());
    QVERIFY(!entries.at(1).toMap().value(
        QStringLiteral("iconOverrideApplied")).toBool());
    QCOMPARE(entries.at(1).toMap().value(QStringLiteral("iconName")).toString(),
             second.value(QStringLiteral("iconName")).toString());
}

void PanelWindowCapabilityTest::iconPropertiesPublicInteractionIsTransactional()
{
    QTemporaryDir desktopEntries;
    QVERIFY(desktopEntries.isValid());
    const QString desktopPath = desktopEntries.filePath(
        QStringLiteral("org.example.interaction.desktop"));
    QFile desktopFile(desktopPath);
    QVERIFY(desktopFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    desktopFile.write(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Interaction app\n"
        "Icon=applications-development\n"
        "Exec=/bin/true\n");
    desktopFile.close();

    const QDir sourceRoot(QFileInfo(QString::fromUtf8(__FILE__))
                              .absoluteDir()
                              .filePath(QStringLiteral("..")));
    QQmlApplicationEngine engine;
    engine.addImportPath(sourceRoot.filePath(QStringLiteral("qml")));
    PanelWindow window(engine);
    QVERIFY(window.pinDockUrl(QUrl::fromLocalFile(desktopPath).toString()));

    const QVariantList initialEntries = window.dockEntriesForPanel(
        QStringLiteral("bottom"), QStringLiteral("hybrid"));
    QCOMPARE(initialEntries.size(), 1);
    const QVariantMap initialEntry = initialEntries.constFirst().toMap();
    QVERIFY(initialEntry.value(
        QStringLiteral("iconPropertiesSupported")).toBool());
    const QString identity = initialEntry.value(
        QStringLiteral("stableIdentity")).toString();
    QVERIFY(!identity.isEmpty());

    QQmlComponent harnessComponent(
        &engine,
        QUrl::fromLocalFile(sourceRoot.filePath(
            QStringLiteral("tests/IconPropertiesInteractionHarness.qml"))));
    QVERIFY2(harnessComponent.isReady(),
             qPrintable(harnessComponent.errorString()));
    std::unique_ptr<QObject> harnessObject(
        harnessComponent.createWithInitialProperties({
            {QStringLiteral("interactionEntry"), initialEntry},
        }));
    QVERIFY2(harnessObject, qPrintable(harnessComponent.errorString()));
    auto *harnessWindow = qobject_cast<QQuickWindow *>(harnessObject.get());
    QVERIFY(harnessWindow);
    QVERIFY(QTest::qWaitForWindowExposed(harnessWindow));

    auto *liveEntry = harnessWindow->findChild<QQuickItem *>(
        QStringLiteral("liveDockEntry"));
    auto *pointerTarget = harnessWindow->findChild<QQuickItem *>(
        QStringLiteral("dockEntryPointerTarget"));
    auto *propertiesAction = harnessWindow->findChild<QQuickItem *>(
        QStringLiteral("iconPropertiesAction"));
    QVERIFY(liveEntry);
    QVERIFY(pointerTarget);
    QVERIFY(propertiesAction);

    const auto clickItem = [](QQuickItem *item, Qt::MouseButton button)
    {
        if (!item || !item->window())
        {
            return false;
        }
        const QPointF sceneCenter = item->mapToScene(
            QPointF(item->width() / 2.0, item->height() / 2.0));
        QTest::mouseClick(item->window(), button, Qt::NoModifier,
                          sceneCenter.toPoint());
        return true;
    };
    const auto waitUntil = [](const auto &predicate, int timeout = 5000)
    {
        QElapsedTimer timer;
        timer.start();
        while (!predicate() && timer.elapsed() < timeout)
        {
            QTest::qWait(20);
        }
        return predicate();
    };
    const auto editorWindow = []() -> QQuickWindow *
    {
        for (QWindow *candidate : QGuiApplication::allWindows())
        {
            if (candidate->objectName() == QStringLiteral("iconPropertiesWindow"))
            {
                return qobject_cast<QQuickWindow *>(candidate);
            }
        }
        return nullptr;
    };
    const auto openEditorFromLiveMenu = [&]()
    {
        if (!clickItem(pointerTarget, Qt::RightButton) ||
            !waitUntil([&]
            {
                return liveEntry->property("contextMenuVisible").toBool() &&
                    propertiesAction->isVisible();
            }))
        {
            return static_cast<QQuickWindow *>(nullptr);
        }
        if (!clickItem(propertiesAction, Qt::LeftButton) ||
            !waitUntil([&]
            {
                QQuickWindow *candidate = editorWindow();
                return candidate && candidate->isVisible();
            }))
        {
            return static_cast<QQuickWindow *>(nullptr);
        }
        if (liveEntry->property("contextMenuVisible").toBool())
        {
            return static_cast<QQuickWindow *>(nullptr);
        }
        return editorWindow();
    };
    const auto replaceText = [&](QQuickItem *field, const QString &text)
    {
        if (!clickItem(field, Qt::LeftButton) || !field->window())
        {
            return false;
        }
        if (!waitUntil([&]
            {
                return field->hasActiveFocus();
            }))
        {
            return false;
        }
        QTest::keySequence(field->window(), QKeySequence::SelectAll);
        for (const QChar character : text)
        {
            QTest::keyClick(field->window(), character.toLatin1());
        }
        return waitUntil([&]
        {
            return field->property("text").toString() == text;
        });
    };

    QQuickWindow *propertiesWindow = openEditorFromLiveMenu();
    QVERIFY(propertiesWindow);
    const QVariantMap openResult = harnessWindow->property(
        "lastOpenResult").toMap();
    QVERIFY(openResult.value(QStringLiteral("success")).toBool());
    QCOMPARE(openResult.value(QStringLiteral("status")).toString(),
             QStringLiteral("opened"));
    QCOMPARE(openResult.value(QStringLiteral("entryIdentity")).toString(),
             identity);

    auto *labelField = propertiesWindow->findChild<QQuickItem *>(
        QStringLiteral("customLabelField"));
    auto *applyButton = propertiesWindow->findChild<QQuickItem *>(
        QStringLiteral("applyButton"));
    QVERIFY(labelField);
    QVERIFY(applyButton);
    QVERIFY(replaceText(labelField, QStringLiteral("Applied through live UI")));
    QVERIFY(waitUntil([&]
    {
        return applyButton->isEnabled();
    }));
    QVERIFY(clickItem(applyButton, Qt::LeftButton));
    QVERIFY(waitUntil([&]
    {
        return !propertiesWindow->isVisible();
    }));

    QVariantList entries = window.dockEntriesForPanel(
        QStringLiteral("bottom"), QStringLiteral("hybrid"));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.constFirst().toMap().value(
                 QStringLiteral("displayName")).toString(),
             QStringLiteral("Applied through live UI"));
    QVERIFY(entries.constFirst().toMap().value(
        QStringLiteral("iconOverrideApplied")).toBool());
    const quint64 appliedRevision = window.dockConfiguration(
        QStringLiteral("bottom"))
                                        .value(QStringLiteral("settingsRevision"))
                                        .toULongLong();

    propertiesWindow = openEditorFromLiveMenu();
    QVERIFY(propertiesWindow);
    labelField = propertiesWindow->findChild<QQuickItem *>(
        QStringLiteral("customLabelField"));
    auto *cancelButton = propertiesWindow->findChild<QQuickItem *>(
        QStringLiteral("cancelButton"));
    QVERIFY(labelField);
    QVERIFY(cancelButton);
    QVERIFY(replaceText(labelField, QStringLiteral("Discarded draft")));
    QVERIFY(clickItem(cancelButton, Qt::LeftButton));
    QVERIFY(waitUntil([&]
    {
        return !propertiesWindow->isVisible();
    }));
    QCOMPARE(window.dockConfiguration(QStringLiteral("bottom"))
                 .value(QStringLiteral("settingsRevision"))
                 .toULongLong(),
             appliedRevision);
    entries = window.dockEntriesForPanel(
        QStringLiteral("bottom"), QStringLiteral("hybrid"));
    QCOMPARE(entries.constFirst().toMap().value(
                 QStringLiteral("displayName")).toString(),
             QStringLiteral("Applied through live UI"));

    propertiesWindow = openEditorFromLiveMenu();
    QVERIFY(propertiesWindow);
    labelField = propertiesWindow->findChild<QQuickItem *>(
        QStringLiteral("customLabelField"));
    QVERIFY(labelField);
    QVERIFY(replaceText(labelField, QStringLiteral("Window-close draft")));
    propertiesWindow->close();
    QVERIFY(waitUntil([&]
    {
        return !propertiesWindow->isVisible();
    }));
    QCOMPARE(window.dockConfiguration(QStringLiteral("bottom"))
                 .value(QStringLiteral("settingsRevision"))
                 .toULongLong(),
             appliedRevision);

    propertiesWindow = openEditorFromLiveMenu();
    QVERIFY(propertiesWindow);
    labelField = propertiesWindow->findChild<QQuickItem *>(
        QStringLiteral("customLabelField"));
    QVERIFY(labelField);
    QCOMPARE(labelField->property("text").toString(),
             QStringLiteral("Applied through live UI"));
    auto *resetButton = propertiesWindow->findChild<QQuickItem *>(
        QStringLiteral("resetButton"));
    QVERIFY(resetButton);
    QVERIFY(waitUntil([&]
    {
        return resetButton->isEnabled();
    }));
    QVERIFY(clickItem(resetButton, Qt::LeftButton));
    QVERIFY(waitUntil([&]
    {
        return !propertiesWindow->isVisible();
    }));
    entries = window.dockEntriesForPanel(
        QStringLiteral("bottom"), QStringLiteral("hybrid"));
    QCOMPARE(entries.constFirst().toMap().value(
                 QStringLiteral("displayName")).toString(),
             QStringLiteral("Interaction app"));
    QVERIFY(!entries.constFirst().toMap().value(
        QStringLiteral("iconOverrideApplied")).toBool());
}

void PanelWindowCapabilityTest::runningOnlyIconPropertiesAreUnavailable()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    WindowModel *windowModel = qobject_cast<WindowModel *>(
        engine.rootContext()
            ->contextProperty(QStringLiteral("windowModel"))
            .value<QObject *>());
    QVERIFY(windowModel);

    WindowItem running;
    running.internalId = QStringLiteral("transient-window");
    running.resourceClass = QStringLiteral("transient-only-app");
    running.iconName = QStringLiteral("application-x-executable");
    running.caption = QStringLiteral("Transient only");
    windowModel->setWindows({running});

    const QVariantList entries = window.dockEntriesForPanel(
        QStringLiteral("bottom"), QStringLiteral("tasks"));
    QCOMPARE(entries.size(), 1);
    const QVariantMap entry = entries.constFirst().toMap();
    QVERIFY(entry.value(QStringLiteral("running")).toBool());
    QVERIFY(!entry.value(QStringLiteral("pinned")).toBool());
    QVERIFY(!entry.value(
        QStringLiteral("iconPropertiesSupported")).toBool());
    const QString identity = entry.value(
        QStringLiteral("stableIdentity")).toString();
    QVERIFY(!identity.isEmpty());

    const QVariantMap snapshot = window.iconOverrideSnapshotForIdentity(
        QStringLiteral("bottom"), identity);
    QVERIFY(!snapshot.value(QStringLiteral("success")).toBool());
    QCOMPARE(snapshot.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("entry-not-supported"));

    const QVariantMap shown = window.showIconProperties(
        QStringLiteral("bottom"), identity);
    QVERIFY(!shown.value(QStringLiteral("success")).toBool());
    QCOMPARE(shown.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("entry-not-supported"));

    const quint64 revision = window.dockConfiguration(
        QStringLiteral("bottom"))
                                   .value(QStringLiteral("settingsRevision"))
                                   .toULongLong();
    const QVariantMap applied = window.applyIconOverrideTransaction(
        QStringLiteral("bottom"), revision, identity,
        {{QStringLiteral("customLabel"), QStringLiteral("Not allowed")}});
    QVERIFY(!applied.value(QStringLiteral("success")).toBool());
    QCOMPARE(applied.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("entry-not-supported"));
}

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    }
    if (qEnvironmentVariableIsEmpty("ARCHDOCK_PRIVATE_INTERACTION_TEST"))
    {
        qputenv(
            "DBUS_SESSION_BUS_ADDRESS",
            QByteArrayLiteral("unix:path=/nonexistent/archdock-phase-b-session-bus"));
    }
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ArchDockTests"));
    QCoreApplication::setApplicationName(QStringLiteral("PanelWindowCapabilityTest"));
    PanelWindowCapabilityTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "PanelWindowCapabilityTest.moc"
