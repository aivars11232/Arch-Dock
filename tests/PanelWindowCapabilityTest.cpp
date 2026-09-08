#include "model/PanelCapabilityResolver.h"
#include "RendererBuildConfig.h"
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
    void interactionGuardsReachTheHostVisibilityDecision();
    void presentationStateAndRequestsAreObservableThroughTheBackend();
    void presentationProfileIsPublishedForLaterPresets();
    void freePanelContentFollowsItsRecordAndOrdersItsOwnEntries();
    void wholePanelRotationFieldsAreGatedByTheResolver();
    void meshSceneEditorIsGatedAndTransactional();

private:
    QTemporaryDir m_settingsDirectory;
};

void PanelWindowCapabilityTest::meshSceneEditorIsGatedAndTransactional()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(qEnvironmentVariable("QML_IMPORT_PATH",
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml-imports")));
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QVERIFY(registry);
    const QString panelId = registry->addFreePanel();
    QVERIFY(!panelId.isEmpty());
    const auto theme = registry->themeCandidate(panelId, QStringLiteral("mesh-platform-cyan"),
                                               QStringLiteral("complete"));
    QVERIFY(theme.value(QStringLiteral("success")).toBool());
    const auto applied = window.applyPanelSettingsTransaction(panelId,
        registry->panelDefinition(panelId)->settingsRevision, theme.value(QStringLiteral("values")).toMap(), {});
    QVERIFY2(applied.value(QStringLiteral("success")).toBool(),
             qPrintable(applied.value(QStringLiteral("errorMessage")).toString()));
    const bool available = ARCHDOCK_QUICK3D_BUILT && ARCHDOCK_SCENE3D_BUILT;
    const auto snapshot = window.panelSettingsEditorSnapshot(panelId, QStringLiteral("studio"));
    QCOMPARE(fieldKeys(snapshot.value(QStringLiteral("panelFields")).toList())
        .contains(QStringLiteral("scene3DQuality")), available);
    const auto configuration = window.panelRendererConfiguration(panelId);
    QCOMPARE(configuration.value(QStringLiteral("effectiveRendererTier")).toString(),
             available ? QStringLiteral("true3d") : QStringLiteral("procedural2d"));
    QVERIFY(configuration.value(QStringLiteral("themeDefinition")).toMap()
        .contains(QStringLiteral("scene3DResources")));
    if (available)
    {
        for (const QString &quality : {QStringLiteral("low"), QStringLiteral("high"), QStringLiteral("low")})
        {
            const auto changed = window.applyPanelSettingsTransaction(panelId,
                registry->panelDefinition(panelId)->settingsRevision,
                {{QStringLiteral("scene3DQuality"), quality}}, {});
            QVERIFY2(changed.value(QStringLiteral("success")).toBool(),
                     qPrintable(changed.value(QStringLiteral("errorMessage")).toString()));
            QCOMPARE(window.panelRendererConfiguration(panelId).value(QStringLiteral("scene3DQuality")).toString(), quality);
        }
    }
    else
    {
        const auto rejected = window.applyPanelSettingsTransaction(panelId,
            registry->panelDefinition(panelId)->settingsRevision,
            {{QStringLiteral("scene3DQuality"), QStringLiteral("high")}}, {});
        QVERIFY(!rejected.value(QStringLiteral("success")).toBool());
    }
    if (qEnvironmentVariableIsEmpty("ARCHDOCK_PRIVATE_INTERACTION_TEST"))
        return;
    const QString popupSource = QFINDTESTDATA("../qml/runtime/SettingsPopup.qml");
    QQmlComponent component(&engine, QUrl::fromLocalFile(popupSource));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> popup(component.createWithInitialProperties({
        {QStringLiteral("selectedPanelId"), panelId}, {QStringLiteral("mainTabIndex"), 1},
        {QStringLiteral("subTabIndex"), 2}}));
    QVERIFY2(popup != nullptr, qPrintable(component.errorString()));
    auto *studio = qobject_cast<QQuickWindow *>(popup.get());
    QVERIFY(studio);
    studio->show();
    QVERIFY(QTest::qWaitForWindowExposed(studio));
    if (available)
    {
        QTRY_VERIFY_WITH_TIMEOUT(popup->property("scene3DControlsAvailable").toBool(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(popup->property("scene3DQualityVisible").toBool(), 5000);
        const QVariant field = QVariantMap{{QStringLiteral("scope"), QStringLiteral("panel")},
                                           {QStringLiteral("key"), QStringLiteral("rendererTier")}};
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "setFieldValue", Q_ARG(QVariant, field),
                                          Q_ARG(QVariant, QVariant(QStringLiteral("procedural2d")))));
        QTRY_VERIFY(!popup->property("scene3DQualityVisible").toBool());
        QVERIFY(popup->property("scene3DControlsAvailable").toBool());
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "applyStudioChanges"));
        QTRY_COMPARE(window.panelRendererConfiguration(panelId).value(QStringLiteral("effectiveRendererTier")).toString(),
                     QStringLiteral("procedural2d"));
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "setFieldValue", Q_ARG(QVariant, field),
                                          Q_ARG(QVariant, QVariant(QStringLiteral("true3d")))));
        QTRY_VERIFY2(popup->property("scene3DQualityVisible").toBool(),
                     qPrintable(popup->property("studioError").toString()));
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "cancelStudioChanges"));
        QTRY_VERIFY(!popup->property("scene3DQualityVisible").toBool());
    }
    else
    {
        QVERIFY(!popup->property("scene3DControlsAvailable").toBool());
        QVERIFY(!popup->property("scene3DQualityVisible").toBool());
    }
    studio->close();
    qInfo() << "Private Studio mesh controls and transaction checks passed; build available:" << available;
}

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
    // The free host presents the baked renderer TASK-0034 installed, so this
    // package now resolves to the tier it asked for instead of falling back.
    QCOMPARE(validRenderer.value(QStringLiteral("requestedTier")).toString(),
             QStringLiteral("baked2.5d"));
    QCOMPARE(validRenderer.value(QStringLiteral("effectiveTier")).toString(),
             QStringLiteral("baked2.5d"));
    QVERIFY(!validRenderer.value(QStringLiteral("fallbackApplied")).toBool());
    QCOMPARE(validRenderer.value(QStringLiteral("reasonCode")).toString(),
             QStringLiteral("available"));

    const QVariantList validFields = validSnapshot.value(
        QStringLiteral("panelFields")).toList();
    QCOMPARE(fieldByKey(validFields, QStringLiteral("layout"))
                 .value(QStringLiteral("choices")).toStringList(),
             QStringList({QStringLiteral("ring"), QStringLiteral("polygon")}));
    const QSet<QString> validKeys = fieldKeys(validFields);
    QVERIFY(validKeys.contains(QStringLiteral("layoutAngle")));
    QVERIFY(validKeys.contains(QStringLiteral("layoutRadius")));
    QVERIFY(validKeys.contains(QStringLiteral("pathOrientation")));
    // The procedural surface controls belong to the procedural renderer. This
    // package is drawn by the baked renderer, so offering them would be the
    // kind of non-working control the interface rules forbid.
    QVERIFY(!validKeys.contains(QStringLiteral("appearance")));
    QVERIFY(!validKeys.contains(QStringLiteral("shape")));
    QVERIFY(!validKeys.contains(QStringLiteral("opacity")));
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
    QCOMPARE(themes.size(), 16);
    const auto meshTheme = std::find_if(themes.cbegin(), themes.cend(), [](const QVariant &value) {
        return value.toMap().value(QStringLiteral("id")).toString() == QStringLiteral("mesh-platform-cyan");
    });
    QVERIFY(meshTheme != themes.cend());
    QVERIFY(meshTheme->toMap().value(QStringLiteral("valid")).toBool());
    QVERIFY(!meshTheme->toMap().value(QStringLiteral("available")).toBool());
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
    // The module contains generated build facts. Normal tests consume the
    // build module; the private smoke supplies its staged QML_IMPORT_PATH.
    if (!qEnvironmentVariableIsSet("ARCHDOCK_PRIVATE_INTERACTION_TEST"))
    {
        engine.addImportPath(QDir(QCoreApplication::applicationDirPath())
                                 .filePath(QStringLiteral("qml-imports")));
    }
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

// TASK-0032 Phase D: the applet's guards reach the backend.
//
// decidePanelVisibility already refused to conceal a locked panel, and
// PanelVisibilityTest proves that rule for every guard. What was missing is the
// channel: nothing ever populated those locks, so the decision always ran with
// every guard false. Only the applet knows a menu is open. This proves the
// channel carries it, is idempotent, and refuses a panel Arch Dock does not own.
void PanelWindowCapabilityTest::interactionGuardsReachTheHostVisibilityDecision()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);

    const QVariantMap initial = window.panelInteractionGuards(
        QStringLiteral("bottom"));
    QCOMPARE(initial.value(QStringLiteral("popupOpen")).toBool(), false);
    QCOMPARE(initial.value(QStringLiteral("pointerInside")).toBool(), false);

    const int revisionBefore = window.visibilityRevision();
    QVERIFY(window.reportPanelInteractionGuards(
        QStringLiteral("bottom"),
        {{QStringLiteral("popupOpen"), true},
         {QStringLiteral("pointerInside"), true}}));
    QVERIFY(window.visibilityRevision() > revisionBefore);

    const QVariantMap stored = window.panelInteractionGuards(
        QStringLiteral("bottom"));
    QCOMPARE(stored.value(QStringLiteral("popupOpen")).toBool(), true);
    QCOMPARE(stored.value(QStringLiteral("pointerInside")).toBool(), true);
    QCOMPARE(stored.value(QStringLiteral("dragActive")).toBool(), false);
    QCOMPARE(stored.value(QStringLiteral("editMode")).toBool(), false);

    // Reporting the same guards again must not spin the visibility revision
    // and wake every listener for nothing.
    const int revisionAfterFirst = window.visibilityRevision();
    QVERIFY(window.reportPanelInteractionGuards(
        QStringLiteral("bottom"),
        {{QStringLiteral("popupOpen"), true},
         {QStringLiteral("pointerInside"), true}}));
    QCOMPARE(window.visibilityRevision(), revisionAfterFirst);

    // A panel Arch Dock does not own cannot report anything.
    QVERIFY(!window.reportPanelInteractionGuards(
        QStringLiteral("not-a-panel"),
        {{QStringLiteral("popupOpen"), true}}));

    // Whatever the mode, a panel holding a guard is never concealed.
    window.setPanelVisibilityMode(QStringLiteral("bottom"),
                                  QStringLiteral("auto-hide"));
    QVERIFY(!window.shouldConcealPanel(QStringLiteral("bottom")));

    QVERIFY(window.reportPanelInteractionGuards(
        QStringLiteral("bottom"),
        {{QStringLiteral("popupOpen"), false},
         {QStringLiteral("pointerInside"), false}}));
    QCOMPARE(window.panelInteractionGuards(QStringLiteral("bottom"))
                 .value(QStringLiteral("popupOpen")).toBool(),
             false);
}

// TASK-0032 closure: the live applet's resting presentation state is
// observable, and an explicit request reaches it. The first is how a harness
// proves a real applet collapsed without injecting input; the second is the
// producer the `manual` trigger lacked, which had left a collapsed manual
// panel with no way to open.
void PanelWindowCapabilityTest::presentationStateAndRequestsAreObservableThroughTheBackend()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);

    const QVariantMap initial = window.panelPresentationState(
        QStringLiteral("bottom"));
    QCOMPARE(initial.value(QStringLiteral("reported")).toBool(), false);
    QVERIFY(initial.value(QStringLiteral("surfaceState")).toString().isEmpty());

    // Only the controller's vocabulary is accepted, and only for owned panels.
    QVERIFY(!window.reportPanelPresentationState(
        QStringLiteral("bottom"),
        {{QStringLiteral("surfaceState"), QStringLiteral("sideways")}}));
    QVERIFY(!window.reportPanelPresentationState(
        QStringLiteral("not-a-panel"),
        {{QStringLiteral("surfaceState"), QStringLiteral("collapsed")}}));
    QCOMPARE(window.panelPresentationState(QStringLiteral("bottom"))
                 .value(QStringLiteral("reported")).toBool(),
             false);

    QVERIFY(window.reportPanelPresentationState(
        QStringLiteral("bottom"),
        {{QStringLiteral("surfaceState"), QStringLiteral("Collapsed")}}));
    const QVariantMap reported = window.panelPresentationState(
        QStringLiteral("bottom"));
    QCOMPARE(reported.value(QStringLiteral("reported")).toBool(), true);
    QCOMPARE(reported.value(QStringLiteral("surfaceState")).toString(),
             QStringLiteral("collapsed"));
    QCOMPARE(reported.value(QStringLiteral("transitionState")).toString(),
             QStringLiteral("idle"));
    QCOMPARE(reported.value(QStringLiteral("hostPhase")).toString(),
             QStringLiteral("revealed"));

    // Requests: invalid vocabulary and unknown panels change nothing.
    const qulonglong revisionBefore = window.presentationRequestRevision();
    QVERIFY(!window.requestPanelPresentation(
        QStringLiteral("bottom"), QStringLiteral("explode")));
    QVERIFY(!window.requestPanelPresentation(
        QStringLiteral("not-a-panel"), QStringLiteral("open")));
    QCOMPARE(window.presentationRequestRevision(), revisionBefore);
    QCOMPARE(window.takePanelPresentationRequest(QStringLiteral("bottom"))
                 .value(QStringLiteral("pending")).toBool(),
             false);

    // A valid request advances the revision the applet listens to, and is
    // taken exactly once. The most recent request wins if several queue up.
    QVERIFY(window.requestPanelPresentation(
        QStringLiteral("bottom"), QStringLiteral("collapse")));
    QVERIFY(window.requestPanelPresentation(
        QStringLiteral("bottom"), QStringLiteral("Open")));
    QCOMPARE(window.presentationRequestRevision(), revisionBefore + 2);
    const QVariantMap taken = window.takePanelPresentationRequest(
        QStringLiteral("bottom"));
    QCOMPARE(taken.value(QStringLiteral("pending")).toBool(), true);
    QCOMPARE(taken.value(QStringLiteral("request")).toString(),
             QStringLiteral("open"));
    QCOMPARE(window.takePanelPresentationRequest(QStringLiteral("bottom"))
                 .value(QStringLiteral("pending")).toBool(),
             false);

    // Requests are per panel: another panel's queue is untouched.
    QVERIFY(window.requestPanelPresentation(
        QStringLiteral("top"), QStringLiteral("open")));
    QCOMPARE(window.takePanelPresentationRequest(QStringLiteral("bottom"))
                 .value(QStringLiteral("pending")).toBool(),
             false);
    QCOMPARE(window.takePanelPresentationRequest(QStringLiteral("top"))
                 .value(QStringLiteral("request")).toString(),
             QStringLiteral("open"));
}

// TASK-0033 Phase A: a free panel's content type decides what it shows, its
// entries are panel-specific and ordered, the order is committed as a
// revision and survives a registry reload, and free ids never enter the
// shared application reorder.
void PanelWindowCapabilityTest::freePanelContentFollowsItsRecordAndOrdersItsOwnEntries()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    PanelRegistry *registry = qobject_cast<PanelRegistry *>(
        engine.rootContext()
            ->contextProperty(QStringLiteral("panelRegistry"))
            .value<QObject *>());
    QVERIFY(registry);

    QTemporaryDir files;
    QVERIFY(files.isValid());
    const auto writeDesktop = [&files](const QString &name, const QString &title)
    {
        QFile file(files.filePath(name));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            return QString{};
        }
        file.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\n"
                                  "Icon=applications-system\nExec=/bin/true\n")
                       .arg(title).toUtf8());
        file.close();
        return QFileInfo(file.fileName()).absoluteFilePath();
    };
    const QString alphaPath = writeDesktop(QStringLiteral("alpha.desktop"), QStringLiteral("Alpha"));
    const QString betaPath = writeDesktop(QStringLiteral("beta.desktop"), QStringLiteral("Beta"));
    QVERIFY(!alphaPath.isEmpty() && !betaPath.isEmpty());
    const QString folderPath = files.filePath(QStringLiteral("Folder"));
    QVERIFY(QDir().mkpath(folderPath));
    const QUrl alphaUrl = QUrl::fromLocalFile(alphaPath);
    const QUrl betaUrl = QUrl::fromLocalFile(betaPath);
    const QUrl folderUrl = QUrl::fromLocalFile(folderPath);
    const QString alphaId = ArchDock::PanelContent::urlEntryId(alphaUrl.toString());
    const QString betaId = ArchDock::PanelContent::urlEntryId(betaUrl.toString());
    const QString folderId = ArchDock::PanelContent::urlEntryId(folderUrl.toString());

    const QString panelId = registry->addFreePanel();
    QVERIFY(!panelId.isEmpty());
    QCOMPARE(registry->panelDefinition(panelId)->content.type, QStringLiteral("empty"));

    // Native panels never accept panel-specific content.
    QVERIFY(!window.addPanelEntries(QStringLiteral("bottom"), {alphaUrl.toString()}));
    QVERIFY(!window.movePanelEntryBefore(QStringLiteral("bottom"), alphaId, QString{}));

    // Entries are stored even while the type is empty, but nothing is shown.
    const quint64 revisionBefore = registry->panelDefinition(panelId)->settingsRevision;
    QVERIFY(window.addPanelEntries(panelId, {alphaUrl.toString(), folderUrl.toString()}));
    QCOMPARE(window.panelEntryOrder(panelId), QStringList({alphaId, folderId}));
    QCOMPARE(registry->panelDefinition(panelId)->settingsRevision, revisionBefore + 1);
    QVERIFY(window.dockEntriesForPanel(panelId, QStringLiteral("hybrid")).isEmpty());
    // Missing files and unknown application ids add nothing and spend no revision.
    QVERIFY(!window.addPanelEntries(panelId, {QStringLiteral("file:///nonexistent/x.desktop"),
                                             QStringLiteral("org.example.unknown")}));
    QCOMPARE(registry->panelDefinition(panelId)->settingsRevision, revisionBefore + 1);

    const auto setType = [&window, registry, &panelId](const QString &type)
    {
        const QVariantMap result = window.applyPanelSettingsTransaction(
            panelId,
            registry->panelDefinition(panelId)->settingsRevision,
            {{QStringLiteral("type"), type}});
        return result.value(QStringLiteral("success")).toBool();
    };
    const auto shownIds = [&window, &panelId]
    {
        QStringList ids;
        for (const QVariant &value : window.dockEntriesForPanel(panelId, QStringLiteral("empty")))
        {
            ids.append(value.toMap().value(QStringLiteral("appId")).toString());
        }
        return ids;
    };

    // Launcher shows the panel's own ordered entries and ignores the applet's
    // requested type.
    QVERIFY(setType(QStringLiteral("launcher")));
    QCOMPARE(shownIds(), QStringList({alphaId, folderId}));
    const QVariantMap alphaEntry = window.dockEntriesForPanel(
        panelId, QStringLiteral("empty")).constFirst().toMap();
    QCOMPARE(alphaEntry.value(QStringLiteral("displayName")).toString(), QStringLiteral("Alpha"));
    QCOMPARE(alphaEntry.value(QStringLiteral("panelEntryId")).toString(), alphaId);
    QVERIFY(alphaEntry.value(QStringLiteral("pinned")).toBool());
    QVERIFY(!alphaEntry.value(QStringLiteral("running")).toBool());
    QVERIFY(alphaEntry.value(QStringLiteral("iconPropertiesSupported")).toBool());

    // Reordering is a panel operation, refuses unknown ids, and persists.
    QVERIFY(window.addPanelEntries(panelId, {betaUrl.toString()}));
    QVERIFY(window.movePanelEntryBefore(panelId, betaId, alphaId));
    QCOMPARE(shownIds(), QStringList({betaId, alphaId, folderId}));
    QVERIFY(!window.movePanelEntryBefore(panelId, QStringLiteral("free-url:file:///nope"), alphaId));
    QVERIFY(!window.setPanelEntryOrder(panelId, {alphaId, betaId}));
    QVERIFY(window.setPanelEntryOrder(panelId, {folderId, alphaId, betaId}));
    QCOMPARE(shownIds(), QStringList({folderId, alphaId, betaId}));
    QVERIFY(!window.moveDockEntryBefore(alphaId, betaId));
    {
        PanelRegistry reloaded;
        QCOMPARE(reloaded.panelDefinition(panelId)->content.entryOrder,
                 QStringList({folderId, alphaId, betaId}));
    }

    // Tasks shows running applications only; nothing runs here, so nothing
    // is shown and the panel's own entries stay stored.
    QVERIFY(setType(QStringLiteral("tasks")));
    QVERIFY(shownIds().isEmpty());
    QCOMPARE(window.panelEntryOrder(panelId), QStringList({folderId, alphaId, betaId}));

    // Hybrid shows the panel's entries and would append running-only apps.
    QVERIFY(setType(QStringLiteral("hybrid")));
    QCOMPARE(shownIds(), QStringList({folderId, alphaId, betaId}));

    // Removal drops exactly one entry and is also refused for unknown ids.
    QVERIFY(!window.removePanelEntry(panelId, QStringLiteral("free-url:file:///nope")));
    QVERIFY(window.removePanelEntry(panelId, alphaId));
    QCOMPARE(shownIds(), QStringList({folderId, betaId}));
    QVERIFY(window.removePanelContent(panelId, folderId));
    QCOMPARE(window.panelEntryOrder(panelId), QStringList({betaId}));
}

// TASK-0033 Phase C: the rotation controls exist only where the resolver says
// the host can turn the scene. A native edge panel never sees them; a free
// radial panel does, with the static angle carrying the resolved degree range
// and the speed keeping its own schema bounds.
void PanelWindowCapabilityTest::wholePanelRotationFieldsAreGatedByTheResolver()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    PanelRegistry *registry = qobject_cast<PanelRegistry *>(
        engine.rootContext()
            ->contextProperty(QStringLiteral("panelRegistry"))
            .value<QObject *>());
    QVERIFY(registry);

    const auto fieldMap = [](const QVariantMap &snapshot)
    {
        QHash<QString, QVariantMap> fields;
        for (const QVariant &value : snapshot.value(QStringLiteral("panelFields")).toList())
        {
            const QVariantMap field = value.toMap();
            fields.insert(field.value(QStringLiteral("key")).toString(), field);
        }
        return fields;
    };
    const QStringList rotationKeys{
        QStringLiteral("panelRotationMode"),
        QStringLiteral("panelRotationSpeed"),
        QStringLiteral("panelRotationTrigger"),
    };

    const QHash<QString, QVariantMap> nativeFields = fieldMap(
        window.panelSettingsEditorSnapshot(QStringLiteral("bottom"), QStringLiteral("studio")));
    for (const QString &key : rotationKeys)
    {
        QVERIFY2(!nativeFields.contains(key),
                 qPrintable(QStringLiteral("native panel exposes %1").arg(key)));
    }
    QVERIFY(!nativeFields.contains(QStringLiteral("layoutAngle")));

    const QString panelId = registry->addFreePanel();
    QVERIFY(!panelId.isEmpty());
    registry->setPanelValue(panelId, QStringLiteral("layout"), QStringLiteral("ring"));
    const QHash<QString, QVariantMap> freeFields = fieldMap(
        window.panelSettingsEditorSnapshot(panelId, QStringLiteral("studio")));
    for (const QString &key : rotationKeys)
    {
        QVERIFY2(freeFields.contains(key),
                 qPrintable(QStringLiteral("free ring panel lacks %1").arg(key)));
    }
    QVERIFY(freeFields.contains(QStringLiteral("layoutAngle")));
    QCOMPARE(freeFields.value(QStringLiteral("layoutAngle"))
                 .value(QStringLiteral("minimumValue")).toReal(), -180.0);
    QCOMPARE(freeFields.value(QStringLiteral("panelRotationSpeed"))
                 .value(QStringLiteral("minimumValue")).toReal(), 1.0);
    QCOMPARE(freeFields.value(QStringLiteral("panelRotationSpeed"))
                 .value(QStringLiteral("maximumValue")).toReal(), 180.0);
    QCOMPARE(freeFields.value(QStringLiteral("panelRotationMode"))
                 .value(QStringLiteral("choices")).toStringList(),
             QStringList({QStringLiteral("none"), QStringLiteral("clockwise"),
                          QStringLiteral("counter-clockwise")}));

    // A linear free layout has no centre to turn about: no rotation controls.
    registry->setPanelValue(panelId, QStringLiteral("layout"), QStringLiteral("horizontal"));
    const QHash<QString, QVariantMap> linearFields = fieldMap(
        window.panelSettingsEditorSnapshot(panelId, QStringLiteral("studio")));
    for (const QString &key : rotationKeys)
    {
        QVERIFY2(!linearFields.contains(key),
                 qPrintable(QStringLiteral("linear free panel exposes %1").arg(key)));
    }

    // The values persist through the ordinary transaction and reach the
    // renderer configuration the applet consumes.
    registry->setPanelValue(panelId, QStringLiteral("layout"), QStringLiteral("ring"));
    const QVariantMap result = window.applyPanelSettingsTransaction(
        panelId,
        registry->panelDefinition(panelId)->settingsRevision,
        {{QStringLiteral("panelRotationMode"), QStringLiteral("Counter-Clockwise")},
         {QStringLiteral("panelRotationSpeed"), 999},
         {QStringLiteral("panelRotationTrigger"), QStringLiteral("hover")}});
    QVERIFY2(result.value(QStringLiteral("success")).toBool(),
             qPrintable(result.value(QStringLiteral("errorMessage")).toString()));
    const QVariantMap configuration = window.panelRendererConfiguration(panelId);
    QCOMPARE(configuration.value(QStringLiteral("panelRotationMode")).toString(),
             QStringLiteral("counter-clockwise"));
    QCOMPARE(configuration.value(QStringLiteral("panelRotationSpeed")).toReal(), 180.0);
    QCOMPARE(configuration.value(QStringLiteral("panelRotationTrigger")).toString(),
             QStringLiteral("hover"));
    QVERIFY(configuration.value(QStringLiteral("capabilityResolution")).toMap()
                .value(QStringLiteral("rotation")).toMap()
                .value(QStringLiteral("available")).toBool());
}

void PanelWindowCapabilityTest::presentationProfileIsPublishedForLaterPresets()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);

    const QVariantMap configuration = window.panelRendererConfiguration(
        QStringLiteral("bottom"));
    const QVariantMap profile = configuration.value(
        QStringLiteral("presentationProfile")).toMap();
    QVERIFY(!profile.isEmpty());
    QCOMPARE(profile.value(QStringLiteral("restingState")).toString(),
             QStringLiteral("open"));
    QCOMPARE(profile.value(QStringLiteral("mechanism")).toString(),
             QStringLiteral("open"));
    QCOMPARE(profile.value(QStringLiteral("trigger")).toString(),
             QStringLiteral("hover"));
    QCOMPARE(profile.value(QStringLiteral("axis")).toString(),
             QStringLiteral("horizontal"));

    // The id is derived from the resolved values, so two panels that present
    // identically carry the same id and a later preset can compare them.
    QCOMPARE(profile.value(QStringLiteral("id")).toString(),
             QStringLiteral("open:open:horizontal:hover:edge-strip"));

    // Being open is always reachable; a real collapse is not, because the
    // default procedural theme declares no mechanism to perform one.
    const QStringList available = profile.value(
        QStringLiteral("availableMechanisms")).toStringList();
    QVERIFY(available.contains(QStringLiteral("open")));
    QVERIFY(!available.contains(QStringLiteral("split")));
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
