#include "PanelRegistry.h"
#include "NativeContainmentLifecycle.h"
#include "panel/FreePanelController.h"
#include "PanelPlacement.h"
#include "ScreenIdentity.h"
#include "PanelVisibility.h"
#include "DockSettings.h"
#include "DockModel.h"
#include "WindowModel.h"
#include "panel/IconOverrideTransaction.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QtGlobal>

#include <array>
#include <functional>
#include <optional>
#include <utility>

namespace
{
class ScopedEnvironmentVariable final
{
public:
    ScopedEnvironmentVariable(QByteArray name, QByteArray value)
        : m_name(std::move(name)), m_wasSet(qEnvironmentVariableIsSet(m_name.constData())),
          m_previousValue(qgetenv(m_name.constData()))
    {
        qputenv(m_name.constData(), value);
    }

    ~ScopedEnvironmentVariable()
    {
        if (m_wasSet)
        {
            qputenv(m_name.constData(), m_previousValue);
        }
        else
        {
            qunsetenv(m_name.constData());
        }
    }

private:
    QByteArray m_name;
    bool m_wasSet;
    QByteArray m_previousValue;
};

class ScopedNativeSettingsPath final
{
public:
    ScopedNativeSettingsPath(QString path, QString restorePath)
        : m_restorePath(std::move(restorePath))
    {
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, path);
    }

    ~ScopedNativeSettingsPath()
    {
        QSettings::setPath(
            QSettings::NativeFormat,
            QSettings::UserScope,
            m_restorePath);
    }

private:
    QString m_restorePath;
};

struct FreePanelHostHarness
{
    std::optional<int> bridgeScreen = 1;
    std::optional<int> matchingCount = 0;
    ArchDock::FreePanelHostDiscoveryResult discoveryResult{
        ArchDock::FreePanelHostDiscoveryOutcome::Unique,
        {42, 73},
        1};
    ArchDock::FreePanelHostMutationResult createResult{
        ArchDock::FreePanelHostMutationOutcome::Verified,
        {42, 73},
        1};
    ArchDock::FreePanelHostMutationResult adoptResult{
        ArchDock::FreePanelHostMutationOutcome::Verified,
        {42, 73},
        1};
    QList<ArchDock::FreePanelHostVerificationOutcome> verificationResults{
        ArchDock::FreePanelHostVerificationOutcome::Owned,
        ArchDock::FreePanelHostVerificationOutcome::Owned};
    ArchDock::FreePanelRemovalOutcome exactRemoval =
        ArchDock::FreePanelRemovalOutcome::Removed;
    ArchDock::FreePanelRemovalOutcome identityRemoval =
        ArchDock::FreePanelRemovalOutcome::AlreadyAbsent;
    ArchDock::FreePanelRemovalOutcome bridgeRemoval =
        ArchDock::FreePanelRemovalOutcome::Removed;
    std::function<void(int)> onVerify;
    std::function<void()> onDiscover;
    std::function<void()> onExactRemoval;
    std::function<void()> onCreate;
    int bridgeScreenCalls = 0;
    int matchingCountCalls = 0;
    int discoverCalls = 0;
    int createCalls = 0;
    int adoptCalls = 0;
    int verifyCalls = 0;
    int exactRemovalCalls = 0;
    int identityRemovalCalls = 0;
    int bridgeRemovalCalls = 0;
    ArchDock::FreePanelHost removedHost;
    QString discoveredPanelId;
    QString discoveredToken;
    QString removedPanelId;
    QString removedToken;

    ArchDock::FreePanelController::HostOperations operations()
    {
        ArchDock::FreePanelController::HostOperations result;
        result.verifiedBridgeScreen = [this](int, const QString &)
        {
            ++bridgeScreenCalls;
            return bridgeScreen;
        };
        result.matchingHostCount = [this](const QString &, const QString &)
        {
            ++matchingCountCalls;
            return matchingCount;
        };
        result.discoverOwnedHost = [this](const QString &panelId, const QString &token)
        {
            ++discoverCalls;
            discoveredPanelId = panelId;
            discoveredToken = token;
            if (onDiscover)
            {
                onDiscover();
            }
            return discoveryResult;
        };
        result.createConfiguredHost = [this](int, const QString &, const QString &)
        {
            ++createCalls;
            if (onCreate)
            {
                onCreate();
            }
            return createResult;
        };
        result.configureAdoptedHost = [this](
            int,
            int,
            const QString &,
            const QString &)
        {
            ++adoptCalls;
            return adoptResult;
        };
        result.verifyHost = [this](
            int,
            int,
            const QString &,
            const QString &)
        {
            const int callIndex = verifyCalls++;
            if (onVerify)
            {
                onVerify(callIndex);
            }
            if (verificationResults.isEmpty())
            {
                return ArchDock::FreePanelHostVerificationOutcome::Owned;
            }
            return verificationResults.at(qMin(callIndex, verificationResults.size() - 1));
        };
        result.removeOwnedHost = [this](
            int containmentId,
            int appletId,
            const QString &panelId,
            const QString &token)
        {
            ++exactRemovalCalls;
            removedHost = {containmentId, appletId};
            removedPanelId = panelId;
            removedToken = token;
            if (onExactRemoval)
            {
                onExactRemoval();
            }
            return exactRemoval;
        };
        result.removeOwnedHostByIdentity = [this](
            const QString &panelId,
            const QString &token)
        {
            ++identityRemovalCalls;
            removedPanelId = panelId;
            removedToken = token;
            return identityRemoval;
        };
        result.removeVerifiedBridge = [this](int, const QString &)
        {
            ++bridgeRemovalCalls;
            return bridgeRemoval;
        };
        result.screenIdForIndex = [](int screenIndex)
        {
            return QStringLiteral("output:%1").arg(screenIndex);
        };
        return result;
    }
};

struct FreePanelControllerRun
{
    ArchDock::FreePanelCreationResult result;
    QStringList panelIdsBefore;
    QStringList panelIdsAfter;
};

FreePanelControllerRun runFreePanelController(
    FreePanelHostHarness &harness,
    const ArchDock::FreePanelCreationRequest &request)
{
    PanelRegistry registry;
    FreePanelControllerRun run;
    run.panelIdsBefore = registry.panelIds();
    ArchDock::FreePanelController controller(registry, harness.operations());
    run.result = controller.create(request);
    run.panelIdsAfter = registry.panelIds();
    return run;
}

ArchDock::FreePanelCreationRequest studioFreePanelRequest()
{
    ArchDock::FreePanelCreationRequest request;
    request.origin = ArchDock::FreePanelCreationOrigin::Studio;
    request.screenIndex = 1;
    return request;
}

QString createOwnedFreePanelRecord(
    PanelRegistry &registry,
    const QString &ownershipToken,
    int desktopContainmentId = 42,
    int dockAppletId = 73,
    int screenIndex = 1)
{
    const QString panelId = registry.beginFreePanelCreation(ownershipToken);
    if (panelId.isEmpty() ||
        !registry.commitVerifiedFreeHostAssociation(
            panelId,
            desktopContainmentId,
            dockAppletId,
            ownershipToken,
            screenIndex,
            QStringLiteral("output:%1").arg(screenIndex),
            QStringLiteral("desktop")) ||
        !registry.completeFreePanelCreation(panelId, ownershipToken))
    {
        return {};
    }
    return panelId;
}

void clearPanelRegistrySettings()
{
    QSettings settings;
    settings.clear();
    settings.sync();
}

QVariantList taskThemeDefinitions()
{
    const QString catalogPath = QDir(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath())
        .absoluteFilePath(QStringLiteral("../data/themes/builtin-themes.json"));
    QFile file(catalogPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    const QJsonObject catalog = document.object();
    if (catalog.value(QStringLiteral("format")).toString() !=
            QStringLiteral("org.archdock.theme-catalog") ||
        catalog.value(QStringLiteral("version")).toInt() != 1)
    {
        return {};
    }
    return catalog.value(QStringLiteral("themes")).toArray().toVariantList();
}
}

class PanelRegistryTest final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void provisionsPanelFamiliesAndNativeBridgeState();
    void migratesLegacyBottomPanelSettings();
    void preservesFreePanelForFreeSurface();
    void normalizesFreeHostAssociationStates();
    void migratesLegacyFreeHostAssociationWithoutDataLoss();
    void backsUpFlatRecordsBeforeStableMigration();
    void rejectsCorruptAndUnsupportedStoredRecords();
    void refusesMigrationWhenLegacyBackupConflicts();
    void roundTripsFreeHostAssociation();
    void migratesLegacyThemeSource();
    void batchesNormalizedPanelUpdates();
    void checksBatchPersistenceBeforeRevision();
    void persistsAndRollsBackSettingsTransactionsAtomically();
    void rejectsPreparedTransactionAfterInterveningUpdate();
    void persistsAndResolvesIconOverridesAtomically();
    void persistsNativePanelRecoveryOutcomes();
    void persistsNativePanelRediscoveryOutcomes();
    void reconcilesNativeContainmentLifecycle();
    void classifiesNativeContainmentMatches();
    void selectsNativeContainmentLifecycleIntent();
    void selectsFreePanelCreationIntent();
    void persistsFreePanelCreationTransactionState();
    void rollsBackFreePanelCreationFailures();
    void reportsFreePanelRollbackFailures();
    void recoversFreePanelHostLifecycle();
    void removesFreePanelHostLifecycleSafely();
    void resolvesStableScreenIdentityBeforeFallbackIndex();
    void reservesAndOffsetsOnlySameScreenPanels();
    void concealsOnlyForRelevantActiveWindows();
    void persistsReducedMotionPreference();
    void normalizesLayoutAndMotionValues();
    void separatesVisualChangesFromPanelTopology();
    void filtersEntriesByPanelContentType();
    void exposesStablePanelEntrySnapshots();
    void targetsStableApplicationWindowIds();
    void supportsPinnedFolderSnapshotsAndReordering();
    void validatesBuiltInCapabilityCatalog();
    void resolvesIconStylesIndependentlyFromPanelThemes();
    void resolvesBuiltInChassisPackagesAndImportPrecedence();
    void resolvesThemeCandidatesWithoutMutation();
    void rejectsIncompatibleThemeWithoutRecordMutation();
    void mapsVersionOneArtworkToProceduralFallback();
    void importsVersionedThemePackage();
    void importsVersionTwoThemePackageWithSafeFallback();
    void usesManagedVersionTwoCapabilitiesAndRendererFallback();
    void analyzesAdaptive2DThemeArtwork();
    void retainsSceneSourcesWithoutExternalConversion();
    void rendersResponsivePanelSkins();
    void doesNotExecuteExternalRenderers();

private:
    QTemporaryDir m_settingsDirectory;
};

void PanelRegistryTest::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QStandardPaths::setTestModeEnabled(false);
    QSettings::setPath(
        QSettings::NativeFormat,
        QSettings::UserScope,
        m_settingsDirectory.path());
}

void PanelRegistryTest::cleanup()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).removeRecursively();
}

void PanelRegistryTest::provisionsPanelFamiliesAndNativeBridgeState()
{
    PanelRegistry registry;

    QCOMPARE(registry.panelIds(),
             QStringList({QStringLiteral("bottom"),
                          QStringLiteral("top"),
                          QStringLiteral("side")}));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), -1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeControlAppletId")).toInt(), -1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeDockAppletId")).toInt(), -1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(), QString{});
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("idle"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QString{});
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("screen")).toInt(), 0);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("screenId")).toString(), QString{});
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("visibilityMode")).toString(),
             QStringLiteral("always"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("pathSides")).toInt(), 6);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("pathOrientation")).toString(),
             QStringLiteral("upright"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("pathAnchor")).toString(),
             QStringLiteral("center"));
    QCOMPARE(registry.panelValue(QStringLiteral("top"), QStringLiteral("edge")).toString(),
             QStringLiteral("top"));
    QCOMPARE(registry.panelValue(QStringLiteral("side"), QStringLiteral("edge")).toString(),
             QStringLiteral("right"));
}

void PanelRegistryTest::migratesLegacyBottomPanelSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("dock/position"), QStringLiteral("right"));
    settings.setValue(QStringLiteral("dock/alignment"), QStringLiteral("end"));
    settings.setValue(QStringLiteral("dock/monitorIndex"), 2);
    settings.setValue(QStringLiteral("dock/iconSize"), 68);
    settings.setValue(QStringLiteral("dock/spacing"), 14.0);
    settings.setValue(QStringLiteral("dock/appearancePreset"), QStringLiteral("neon"));
    settings.setValue(QStringLiteral("dock/panelOpacity"), 0.72);
    settings.setValue(QStringLiteral("dock/bottomPanelType"), QStringLiteral("tasks"));
    settings.setValue(QStringLiteral("dock/autoHide"), true);
    settings.sync();

    PanelRegistry registry;

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("edge")).toString(),
             QStringLiteral("right"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("alignment")).toString(),
             QStringLiteral("end"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("screen")).toInt(), 2);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("iconSize")).toInt(), 68);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("spacing")).toReal(), 14.0);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("appearance")).toString(),
             QStringLiteral("neon"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("opacity")).toReal(), 0.72);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("type")).toString(),
             QStringLiteral("tasks"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("visibilityMode")).toString(),
             QStringLiteral("auto-hide"));
    QCOMPARE(registry.panelValue(QStringLiteral("top"), QStringLiteral("screen")).toInt(), 2);
    QCOMPARE(registry.panelValue(QStringLiteral("side"), QStringLiteral("screen")).toInt(), 2);
}

void PanelRegistryTest::preservesFreePanelForFreeSurface()
{
    QJsonArray panels;
    panels.append(QJsonObject::fromVariantMap(
        {{QStringLiteral("id"), QStringLiteral("free")},
         {QStringLiteral("name"), QStringLiteral("Free panel")},
         {QStringLiteral("builtIn"), true},
         {QStringLiteral("visible"), true},
         {QStringLiteral("edge"), QStringLiteral("free")},
         {QStringLiteral("type"), QStringLiteral("launcher")}}));
    QSettings settings;
    settings.setValue(QStringLiteral("dock/panels"), QJsonDocument(panels).toJson(QJsonDocument::Compact));
    settings.sync();

    PanelRegistry registry;

    QCOMPARE(registry.panelValue(QStringLiteral("free"), QStringLiteral("edge")).toString(),
             QStringLiteral("free"));
    QCOMPARE(registry.panelValue(QStringLiteral("free"), QStringLiteral("type")).toString(),
             QStringLiteral("launcher"));
}

void PanelRegistryTest::normalizesFreeHostAssociationStates()
{
    PanelRegistry registry;
    const QString panelId = registry.addFreePanel();
    QVERIFY(!panelId.isEmpty());
    QVERIFY(!registry.freeHostAssociation(QStringLiteral("bottom")).has_value());

    const auto unhosted = registry.freeHostAssociation(panelId);
    QVERIFY(unhosted.has_value());
    QCOMPARE(unhosted->desktopContainmentId, -1);
    QCOMPARE(unhosted->dockAppletId, -1);
    QCOMPARE(unhosted->ownershipToken, QString{});
    QCOMPARE(unhosted->screenIndex, 0);
    QCOMPARE(unhosted->screenId, QString{});
    QCOMPARE(unhosted->hostMode, QStringLiteral("desktop"));
    QVERIFY(unhosted->state == PanelRegistry::FreeHostState::Unhosted);

    const QString genericFreePanelId = registry.addPanel(
        QStringLiteral("free"), QStringLiteral("empty"));
    const auto genericUnhosted = registry.freeHostAssociation(genericFreePanelId);
    QVERIFY(genericUnhosted.has_value());
    QCOMPARE(genericUnhosted->hostMode, QStringLiteral("desktop"));
    QVERIFY(genericUnhosted->state == PanelRegistry::FreeHostState::Unhosted);

    const int initialRevision = registry.revision();
    QVERIFY(!registry.commitVerifiedFreeHostAssociation(
        QStringLiteral("missing"), 42, 73, QStringLiteral("token-a"), 1,
        QStringLiteral("output:DP-1"), QStringLiteral("desktop")));
    QVERIFY(!registry.commitVerifiedFreeHostAssociation(
        QStringLiteral("bottom"), 42, 73, QStringLiteral("token-a"), 1,
        QStringLiteral("output:DP-1"), QStringLiteral("desktop")));
    QVERIFY(!registry.commitVerifiedFreeHostAssociation(
        panelId, -1, 73, QStringLiteral("token-a"), 1,
        QStringLiteral("output:DP-1"), QStringLiteral("desktop")));
    QVERIFY(!registry.commitVerifiedFreeHostAssociation(
        panelId, 42, -1, QStringLiteral("token-a"), 1,
        QStringLiteral("output:DP-1"), QStringLiteral("desktop")));
    QVERIFY(!registry.commitVerifiedFreeHostAssociation(
        panelId, 42, 73, QString{}, 1,
        QStringLiteral("output:DP-1"), QStringLiteral("desktop")));
    QVERIFY(!registry.commitVerifiedFreeHostAssociation(
        panelId, 42, 73, QString(97, QLatin1Char('x')), 1,
        QStringLiteral("output:DP-1"), QStringLiteral("desktop")));
    QVERIFY(!registry.commitVerifiedFreeHostAssociation(
        panelId, 42, 73, QStringLiteral("token-a"), -1,
        QStringLiteral("output:DP-1"), QStringLiteral("desktop")));
    QVERIFY(!registry.commitVerifiedFreeHostAssociation(
        panelId, 42, 73, QStringLiteral("token-a"), 1,
        QStringLiteral("output:DP-1"), QStringLiteral("overlay")));
    QCOMPARE(registry.revision(), initialRevision);

    QVERIFY(registry.commitVerifiedFreeHostAssociation(
        panelId, 42, 73, QStringLiteral("  token-a  "), 1,
        QStringLiteral("  output:DP-1  "), QStringLiteral("  DESKTOP  ")));
    const auto owned = registry.freeHostAssociation(panelId);
    QVERIFY(owned.has_value());
    QCOMPARE(owned->desktopContainmentId, 42);
    QCOMPARE(owned->dockAppletId, 73);
    QCOMPARE(owned->ownershipToken, QStringLiteral("token-a"));
    QCOMPARE(owned->screenIndex, 1);
    QCOMPARE(owned->screenId, QStringLiteral("output:DP-1"));
    QCOMPARE(owned->hostMode, QStringLiteral("desktop"));
    QVERIFY(owned->state == PanelRegistry::FreeHostState::HostedOwned);

    registry.updatePanel(
        panelId,
        {{QStringLiteral("freeOwnershipToken"), QString{}},
         {QStringLiteral("freeHostState"), QStringLiteral("hosted-owned")}});
    const auto stale = registry.freeHostAssociation(panelId);
    QVERIFY(stale.has_value());
    QCOMPARE(stale->desktopContainmentId, 42);
    QCOMPARE(stale->dockAppletId, 73);
    QCOMPARE(stale->ownershipToken, QString{});
    QVERIFY(stale->state == PanelRegistry::FreeHostState::HostedStale);

    registry.updatePanel(
        panelId,
        {{QStringLiteral("freeDesktopContainmentId"), -1},
         {QStringLiteral("freeDockAppletId"), -1},
         {QStringLiteral("freeOwnershipToken"), QString{}},
         {QStringLiteral("freeHostMode"), QStringLiteral("desktop")},
         {QStringLiteral("freeHostState"), QStringLiteral("detached")}});
    const auto detached = registry.freeHostAssociation(panelId);
    QVERIFY(detached.has_value());
    QCOMPARE(detached->desktopContainmentId, -1);
    QCOMPARE(detached->dockAppletId, -1);
    QCOMPARE(detached->ownershipToken, QString{});
    QVERIFY(detached->state == PanelRegistry::FreeHostState::Detached);

    registry.updatePanel(
        genericFreePanelId,
        {{QStringLiteral("freeDesktopContainmentId"), 84},
         {QStringLiteral("freeDockAppletId"), 91},
         {QStringLiteral("freeOwnershipToken"), QStringLiteral("token-overlay")},
         {QStringLiteral("freeHostMode"), QStringLiteral("overlay")},
         {QStringLiteral("freeHostState"), QStringLiteral("hosted-owned")}});
    const auto unsupportedMode = registry.freeHostAssociation(genericFreePanelId);
    QVERIFY(unsupportedMode.has_value());
    QCOMPARE(unsupportedMode->hostMode, QStringLiteral("desktop"));
    QVERIFY(unsupportedMode->state == PanelRegistry::FreeHostState::HostedStale);
}

void PanelRegistryTest::migratesLegacyFreeHostAssociationWithoutDataLoss()
{
    QJsonArray panels;
    panels.append(QJsonObject::fromVariantMap(
        {{QStringLiteral("id"), QStringLiteral("legacy-free")},
         {QStringLiteral("name"), QStringLiteral("Legacy free panel")},
         {QStringLiteral("builtIn"), false},
         {QStringLiteral("visible"), true},
         {QStringLiteral("edge"), QStringLiteral("free")},
         {QStringLiteral("type"), QStringLiteral("launcher")},
         {QStringLiteral("contentAppIds"), QStringList{QStringLiteral("org.kde.dolphin")}},
         {QStringLiteral("legacyExtensionData"), QStringLiteral("keep-me")}}));
    QSettings settings;
    settings.setValue(QStringLiteral("dock/monitorIndex"), 2);
    settings.setValue(
        QStringLiteral("dock/panels"),
        QJsonDocument(panels).toJson(QJsonDocument::Compact));
    settings.sync();

    PanelRegistry registry;
    const auto migrated = registry.freeHostAssociation(QStringLiteral("legacy-free"));
    QVERIFY(migrated.has_value());
    QCOMPARE(migrated->desktopContainmentId, -1);
    QCOMPARE(migrated->dockAppletId, -1);
    QCOMPARE(migrated->ownershipToken, QString{});
    QCOMPARE(migrated->screenIndex, 2);
    QCOMPARE(migrated->screenId, QString{});
    QCOMPARE(migrated->hostMode, QStringLiteral("desktop"));
    QVERIFY(migrated->state == PanelRegistry::FreeHostState::Unhosted);
    QCOMPARE(registry.panelName(QStringLiteral("legacy-free")), QStringLiteral("Legacy free panel"));
    QCOMPARE(registry.panelValue(QStringLiteral("legacy-free"), QStringLiteral("type")).toString(),
             QStringLiteral("launcher"));
    QCOMPARE(registry.panelValue(
                 QStringLiteral("legacy-free"), QStringLiteral("contentAppIds")).toStringList(),
             QStringList{QStringLiteral("org.kde.dolphin")});
    QCOMPARE(registry.panelValue(
                 QStringLiteral("legacy-free"), QStringLiteral("legacyExtensionData")).toString(),
             QStringLiteral("keep-me"));

    PanelRegistry reloaded;
    const auto persisted = reloaded.freeHostAssociation(QStringLiteral("legacy-free"));
    QVERIFY(persisted.has_value());
    QVERIFY(persisted->state == PanelRegistry::FreeHostState::Unhosted);
    QCOMPARE(reloaded.panelValue(
                 QStringLiteral("legacy-free"), QStringLiteral("legacyExtensionData")).toString(),
             QStringLiteral("keep-me"));
    QCOMPARE(reloaded.panelValue(
                 QStringLiteral("legacy-free"), QStringLiteral("contentAppIds")).toStringList(),
             QStringList{QStringLiteral("org.kde.dolphin")});
}

void PanelRegistryTest::backsUpFlatRecordsBeforeStableMigration()
{
    const QByteArray legacySource = QByteArray(R"JSON([
  {
    "id": "custom-native",
    "name": "Custom native",
    "builtIn": false,
    "edge": "left",
    "visible": true,
    "screen": 1,
    "visibilityMode": "dodge",
    "legacyExtensionData": "keep-me",
    "hovered": true
  }
])JSON");
    QSettings settings;
    settings.setValue(QStringLiteral("dock/panels"), legacySource);
    settings.sync();

    PanelRegistry registry;
    QCOMPARE(registry.migrationDiagnostic(), QString{});
    QCOMPARE(registry.panelValue(
                 QStringLiteral("custom-native"),
                 QStringLiteral("legacyExtensionData")).toString(),
             QStringLiteral("keep-me"));
    QVERIFY(!registry.panelValue(
        QStringLiteral("custom-native"), QStringLiteral("hovered")).isValid());

    settings.sync();
    QCOMPARE(
        settings.value(QStringLiteral("dock/panelsLegacyV1Backup")).toByteArray(),
        legacySource);
    const QByteArray migratedSource =
        settings.value(QStringLiteral("dock/panels")).toByteArray();
    QVERIFY(migratedSource != legacySource);
    const QJsonObject migratedRecord = QJsonDocument::fromJson(migratedSource)
        .array().at(0).toObject();
    QCOMPARE(migratedRecord.value(QStringLiteral("schemaVersion")).toInt(), 2);
    QVERIFY(!migratedRecord.contains(QStringLiteral("legacyExtensionData")));
    QVERIFY(!migratedRecord.contains(QStringLiteral("hovered")));
    QCOMPARE(
        migratedRecord.value(QStringLiteral("extensions")).toObject()
            .value(QStringLiteral("legacyExtensionData")).toString(),
        QStringLiteral("keep-me"));

    PanelRegistry reloaded;
    QCOMPARE(reloaded.migrationDiagnostic(), QString{});
    settings.sync();
    QCOMPARE(settings.value(QStringLiteral("dock/panels")).toByteArray(), migratedSource);
    QCOMPARE(
        settings.value(QStringLiteral("dock/panelsLegacyV1Backup")).toByteArray(),
        legacySource);
    QCOMPARE(reloaded.panelValue(
                 QStringLiteral("custom-native"),
                 QStringLiteral("legacyExtensionData")).toString(),
             QStringLiteral("keep-me"));
}

void PanelRegistryTest::rejectsCorruptAndUnsupportedStoredRecords()
{
    const auto verifyBlockedSource = [](const QByteArray &source,
                                        const QString &diagnosticPrefix)
    {
        QSettings settings;
        settings.clear();
        settings.setValue(QStringLiteral("dock/panels"), source);
        settings.sync();

        PanelRegistry registry;
        QVERIFY(registry.panelIds().isEmpty());
        QVERIFY2(
            registry.migrationDiagnostic().startsWith(diagnosticPrefix),
            qPrintable(registry.migrationDiagnostic()));
        settings.sync();
        QCOMPARE(settings.value(QStringLiteral("dock/panels")).toByteArray(), source);
        QVERIFY(!settings.contains(QStringLiteral("dock/panelsLegacyV1Backup")));
    };

    verifyBlockedSource(
        QByteArrayLiteral("{not-json"),
        QStringLiteral("invalid-json:"));
    verifyBlockedSource(
        QByteArrayLiteral("[{\"name\":\"Missing id\"}]"),
        QStringLiteral("invalid-record:"));
    verifyBlockedSource(
        QByteArrayLiteral("[{\"schemaVersion\":99,\"id\":\"future\"}]"),
        QStringLiteral("unsupported-version:"));
}

void PanelRegistryTest::refusesMigrationWhenLegacyBackupConflicts()
{
    const QByteArray legacySource = QByteArrayLiteral(
        "[{\"id\":\"custom-native\",\"edge\":\"top\"}]");
    const QByteArray conflictingBackup = QByteArrayLiteral("different-source");
    QSettings settings;
    settings.setValue(QStringLiteral("dock/panels"), legacySource);
    settings.setValue(
        QStringLiteral("dock/panelsLegacyV1Backup"),
        conflictingBackup);
    settings.sync();

    PanelRegistry registry;
    QVERIFY2(
        registry.migrationDiagnostic().startsWith(QStringLiteral("backup-conflict:")),
        qPrintable(registry.migrationDiagnostic()));
    settings.sync();
    QCOMPARE(settings.value(QStringLiteral("dock/panels")).toByteArray(), legacySource);
    QCOMPARE(
        settings.value(QStringLiteral("dock/panelsLegacyV1Backup")).toByteArray(),
        conflictingBackup);
}

void PanelRegistryTest::roundTripsFreeHostAssociation()
{
    PanelRegistry registry;
    const QString panelId = registry.addFreePanel();
    QVERIFY(!panelId.isEmpty());
    QVERIFY(registry.commitVerifiedFreeHostAssociation(
        panelId, 421, 733, QStringLiteral("archdock-free-owner-7"), 2,
        QStringLiteral("edid:0123456789abcdef"), QStringLiteral("desktop")));

    const auto committed = registry.freeHostAssociation(panelId);
    QVERIFY(committed.has_value());
    QCOMPARE(committed->desktopContainmentId, 421);
    QCOMPARE(committed->dockAppletId, 733);
    QCOMPARE(committed->ownershipToken, QStringLiteral("archdock-free-owner-7"));
    QCOMPARE(committed->screenIndex, 2);
    QCOMPARE(committed->screenId, QStringLiteral("edid:0123456789abcdef"));
    QCOMPARE(committed->hostMode, QStringLiteral("desktop"));
    QVERIFY(committed->state == PanelRegistry::FreeHostState::HostedOwned);

    PanelRegistry reloaded;
    const auto persisted = reloaded.freeHostAssociation(panelId);
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->desktopContainmentId, committed->desktopContainmentId);
    QCOMPARE(persisted->dockAppletId, committed->dockAppletId);
    QCOMPARE(persisted->ownershipToken, committed->ownershipToken);
    QCOMPARE(persisted->screenIndex, committed->screenIndex);
    QCOMPARE(persisted->screenId, committed->screenId);
    QCOMPARE(persisted->hostMode, committed->hostMode);
    QVERIFY(persisted->state == committed->state);
    QCOMPARE(reloaded.panelValue(panelId, QStringLiteral("freeHostState")).toString(),
             QStringLiteral("hosted-owned"));
}

void PanelRegistryTest::migratesLegacyThemeSource()
{
    const QString legacyDirectory = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/themes/bottom");
    QVERIFY(QDir().mkpath(legacyDirectory));
    const QString legacySource = legacyDirectory + QStringLiteral("/legacy-surface.png");
    QImage image(12, 12, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::magenta);
    QVERIFY(image.save(legacySource));

    QJsonArray panels;
    panels.append(QJsonObject::fromVariantMap(
        {{QStringLiteral("id"), QStringLiteral("bottom")},
         {QStringLiteral("themeSource"), QUrl::fromLocalFile(legacySource).toString()},
         {QStringLiteral("themeFit"), QStringLiteral("tile")}}));
    QSettings settings;
    settings.setValue(QStringLiteral("dock/panels"), QJsonDocument(panels).toJson(QJsonDocument::Compact));
    settings.sync();

    PanelRegistry registry;

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSource")).toString(),
             QUrl::fromLocalFile(legacySource).toString());
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageFormat")).toString(),
             QStringLiteral("org.archdock.theme"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageVersion")).toInt(), 1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeFit")).toString(),
             QStringLiteral("tile"));
    const QUrl manifest(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageManifest")).toString());
    QVERIFY(QFileInfo::exists(manifest.toLocalFile()));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceKind")).toString(),
             QStringLiteral("raster"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceWidth")).toInt(), 12);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceHeight")).toInt(), 12);
    const QUrl preview(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePreview")).toString());
    QVERIFY(QFileInfo::exists(preview.toLocalFile()));
}

void PanelRegistryTest::batchesNormalizedPanelUpdates()
{
    PanelRegistry registry;
    const int revision = registry.revision();

    registry.updatePanel(
        QStringLiteral("bottom"),
        {{QStringLiteral("edge"), QStringLiteral("invalid")},
         {QStringLiteral("screen"), -1},
         {QStringLiteral("iconSize"), 999},
         {QStringLiteral("spacing"), -10.0},
         {QStringLiteral("visibilityMode"), QStringLiteral("invalid")},
         {QStringLiteral("revealZone"), 999}});

    QCOMPARE(registry.revision(), revision + 1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("edge")).toString(),
             QStringLiteral("bottom"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("screen")).toInt(), 0);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("iconSize")).toInt(), 128);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("spacing")).toReal(), 0.0);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("visibilityMode")).toString(),
             QStringLiteral("always"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("revealZone")).toInt(), 64);
}

void PanelRegistryTest::checksBatchPersistenceBeforeRevision()
{
    PanelRegistry registry;
    QSignalSpy revisionSpy(&registry, &PanelRegistry::revisionChanged);
    const int initialRevision = registry.revision();

    QVERIFY(registry.updatePanelChecked(
        QStringLiteral("bottom"),
        {{QStringLiteral("edge"), QStringLiteral("TOP")},
         {QStringLiteral("screen"), 2},
         {QStringLiteral("height"), 91}}));
    QCOMPARE(registry.revision(), initialRevision + 1);
    QCOMPARE(revisionSpy.count(), 1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("edge")).toString(),
             QStringLiteral("top"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("screen")).toInt(), 2);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("height")).toInt(), 91);

    const QString blockedSettingsPath = m_settingsDirectory.filePath(
        QStringLiteral("not-a-directory"));
    QFile blocker(blockedSettingsPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(blocker.write("blocked"), qint64(7));
    blocker.close();

    {
        const ScopedNativeSettingsPath blockedPath(
            blockedSettingsPath, m_settingsDirectory.path());
        QVERIFY(!registry.updatePanelChecked(
            QStringLiteral("bottom"),
            {{QStringLiteral("edge"), QStringLiteral("right")},
             {QStringLiteral("screen"), 4},
             {QStringLiteral("height"), 101}}));
    }

    QCOMPARE(registry.revision(), initialRevision + 1);
    QCOMPARE(revisionSpy.count(), 1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("edge")).toString(),
             QStringLiteral("top"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("screen")).toInt(), 2);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("height")).toInt(), 91);
}

void PanelRegistryTest::persistsAndRollsBackSettingsTransactionsAtomically()
{
    PanelRegistry registry;
    DockSettings dockSettings;
    const auto beforePanel = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(beforePanel.has_value());
    const QVariantMap beforeGlobals = dockSettings.transactionSnapshot();
    QVariantMap candidateGlobals;
    QString errorMessage;
    QVERIFY(dockSettings.stageTransaction(
        {{QStringLiteral("magnification"), 2.1}},
        &candidateGlobals,
        &errorMessage));

    ArchDock::PanelSettingsTransactionRequest request{
        QStringLiteral("bottom"),
        beforePanel->settingsRevision,
        {
            {QStringLiteral("opacity"), 0.51},
            {QStringLiteral("width"), 911},
        },
        {{QStringLiteral("magnification"), 2.1}},
    };
    ArchDock::PanelSettingsTransactionOutcome outcome;
    const auto draft = ArchDock::PanelSettingsTransaction::prepare(
        *beforePanel,
        beforeGlobals,
        candidateGlobals,
        request,
        &outcome);
    QVERIFY(draft.has_value());

    QSignalSpy revisionSpy(&registry, &PanelRegistry::revisionChanged);
    QSignalSpy panelsSpy(&registry, &PanelRegistry::panelsChanged);
    QVERIFY2(registry.persistPanelSettingsTransaction(*draft, &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(revisionSpy.count(), 0);
    QCOMPARE(panelsSpy.count(), 0);
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("opacity")).toReal(),
             0.51);
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("width")).toInt(),
             911);
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("settingsRevision"))
                 .toULongLong(),
             beforePanel->settingsRevision + 1);
    QSettings persistedCandidate;
    QCOMPARE(persistedCandidate.value(QStringLiteral("dock/magnification")).toReal(),
             2.1);

    quint64 rollbackRevision = 0;
    QVERIFY2(registry.rollbackPanelSettingsTransaction(
                 *draft,
                 draft->candidatePanel.settingsRevision,
                 &rollbackRevision,
                 &errorMessage),
             qPrintable(errorMessage));
    QCOMPARE(rollbackRevision, beforePanel->settingsRevision + 2);
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("opacity")).toReal(),
             beforePanel->surface.opacity);
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("width")).toInt(),
             beforePanel->placement.width);
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("settingsRevision"))
                 .toULongLong(),
             rollbackRevision);
    QSettings persistedRollback;
    QCOMPARE(persistedRollback.value(QStringLiteral("dock/magnification")).toReal(),
             beforeGlobals.value(QStringLiteral("magnification")).toReal());
    QCOMPARE(revisionSpy.count(), 0);
    QCOMPARE(panelsSpy.count(), 0);

    registry.notifyPanelSettingsTransactionAdopted(true);
    QCOMPARE(revisionSpy.count(), 1);
    QCOMPARE(panelsSpy.count(), 1);
}

void PanelRegistryTest::rejectsPreparedTransactionAfterInterveningUpdate()
{
    PanelRegistry registry;
    DockSettings dockSettings;
    const auto beforePanel = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(beforePanel.has_value());
    const QVariantMap globals = dockSettings.transactionSnapshot();
    ArchDock::PanelSettingsTransactionRequest request{
        QStringLiteral("bottom"),
        beforePanel->settingsRevision,
        {{QStringLiteral("opacity"), 0.4}},
        {},
    };
    ArchDock::PanelSettingsTransactionOutcome outcome;
    const auto draft = ArchDock::PanelSettingsTransaction::prepare(
        *beforePanel, globals, globals, request, &outcome);
    QVERIFY(draft.has_value());

    QVERIFY(registry.updatePanelChecked(
        QStringLiteral("bottom"),
        {{QStringLiteral("iconSize"), 61}}));
    QString errorMessage;
    QVERIFY(!registry.persistPanelSettingsTransaction(*draft, &errorMessage));
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("opacity")).toReal(),
             beforePanel->surface.opacity);
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("iconSize")).toInt(),
             61);
    QVERIFY(errorMessage.contains(QStringLiteral("changed")));
}

void PanelRegistryTest::persistsAndResolvesIconOverridesAtomically()
{
    PanelRegistry registry;
    DockSettings dockSettings;
    const auto before = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(before.has_value());
    const QString firstIdentity = QStringLiteral(
        "desktop.org.example.first.desktop");
    const QString secondIdentity = QStringLiteral(
        "desktop.org.example.second.desktop");
    ArchDock::IconOverrideTransactionOutcome outcome;
    const auto draft = ArchDock::IconOverrideTransaction::prepare(
        *before,
        {QStringLiteral("bottom"), before->settingsRevision, firstIdentity,
         {
             {QStringLiteral("customGlyph"),
              QStringLiteral("file:///missing/custom-icon.svg")},
             {QStringLiteral("customLabel"), QStringLiteral("First custom")},
             {QStringLiteral("tileEnabled"), false},
             {QStringLiteral("styleReference"), QStringLiteral("dark-orb")},
             {QStringLiteral("animationProfileReference"),
              QStringLiteral("future-orbit")},
         },
         false},
        &outcome,
        [&registry](const QString &styleId)
        {
            return registry.iconStyleDefinition(styleId)
                    .value(QStringLiteral("selectionStatus")).toString() ==
                QStringLiteral("selected");
        });
    QVERIFY(draft.has_value());

    QString persistenceError;
    QSignalSpy revisionSpy(&registry, &PanelRegistry::revisionChanged);
    QVERIFY2(registry.persistPanelDefinitionTransaction(
                 draft->previousPanel,
                 draft->candidatePanel,
                 dockSettings.transactionSnapshot(),
                 &persistenceError),
             qPrintable(persistenceError));
    QCOMPARE(revisionSpy.count(), 0);
    registry.notifyPanelSettingsTransactionAdopted(false);
    QCOMPARE(revisionSpy.count(), 1);

    const auto persisted = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(persisted.has_value());
    QCOMPARE(persisted->settingsRevision, before->settingsRevision + 1);
    QVERIFY(persisted->iconStyle.perEntryOverrides.contains(firstIdentity));

    const QVariantMap firstEntry{
        {QStringLiteral("stableIdentity"), firstIdentity},
        {QStringLiteral("baseIconName"), QStringLiteral("applications-system")},
        {QStringLiteral("baseDisplayName"), QStringLiteral("First")},
    };
    const QVariantMap firstResolution = registry.resolveIconEntryOverride(
        *persisted, firstEntry);
    QVERIFY(firstResolution.value(
        QStringLiteral("overrideApplied")).toBool());
    QCOMPARE(firstResolution.value(QStringLiteral("resolvedGlyph")).toString(),
             QStringLiteral("applications-system"));
    QVERIFY(firstResolution.value(
        QStringLiteral("glyphFallbackApplied")).toBool());
    QCOMPARE(firstResolution.value(QStringLiteral("resolvedLabel")).toString(),
             QStringLiteral("First custom"));
    QVERIFY(!firstResolution.value(QStringLiteral("tileEnabled")).toBool());
    QCOMPARE(firstResolution.value(QStringLiteral("styleReference")).toString(),
             QStringLiteral("dark-orb"));
    QCOMPARE(firstResolution.value(QStringLiteral("iconStyleDefinition"))
                 .toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("dark-orb"));

    const QVariantMap secondEntry{
        {QStringLiteral("stableIdentity"), secondIdentity},
        {QStringLiteral("baseIconName"), QStringLiteral("utilities-terminal")},
        {QStringLiteral("baseDisplayName"), QStringLiteral("Second")},
    };
    const QVariantMap secondResolution = registry.resolveIconEntryOverride(
        *persisted, secondEntry);
    QVERIFY(!secondResolution.value(
        QStringLiteral("overrideApplied")).toBool());
    QCOMPARE(secondResolution.value(QStringLiteral("resolvedGlyph")).toString(),
             QStringLiteral("utilities-terminal"));
    QCOMPARE(secondResolution.value(QStringLiteral("resolvedLabel")).toString(),
             QStringLiteral("Second"));
    QCOMPARE(secondResolution.value(QStringLiteral("styleReference")).toString(),
             QStringLiteral("plain-original"));

    QVERIFY(!registry.persistPanelDefinitionTransaction(
        draft->previousPanel,
        draft->candidatePanel,
        dockSettings.transactionSnapshot(),
        &persistenceError));
    QVERIFY(persistenceError.contains(QStringLiteral("changed")));
}

void PanelRegistryTest::persistsNativePanelRecoveryOutcomes()
{
    PanelRegistry registry;
    const int initialRevision = registry.revision();

    QVERIFY(!registry.commitVerifiedNativePanelAssociation(
        QStringLiteral("missing"), 42, 73, QStringLiteral("token-a")));
    QVERIFY(!registry.commitVerifiedNativePanelAssociation(
        QStringLiteral("bottom"), 42, -1, QStringLiteral("token-a")));
    QVERIFY(!registry.commitVerifiedNativePanelAssociation(
        QStringLiteral("bottom"), 42, 73, QString{}));
    QVERIFY(!registry.recordNativePanelRecoveryFailure(QStringLiteral("bottom"), QString{}));
    QCOMPARE(registry.revision(), initialRevision);

    QVERIFY(registry.commitVerifiedNativePanelAssociation(
        QStringLiteral("bottom"), 42, 73, QStringLiteral("token-a")));
    QCOMPARE(registry.revision(), initialRevision + 1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), 42);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeControlAppletId")).toInt(), -1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeDockAppletId")).toInt(), 73);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(),
             QStringLiteral("token-a"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QString{});

    PanelRegistry persistedSuccess;
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), 42);
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeDockAppletId")).toInt(), 73);
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(),
             QStringLiteral("token-a"));

    const int failureRevision = persistedSuccess.revision();
    QVERIFY(persistedSuccess.recordNativePanelRecoveryFailure(
        QStringLiteral("bottom"), QStringLiteral("renderer-verification-failed")));
    QCOMPARE(persistedSuccess.revision(), failureRevision + 1);
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), -1);
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeControlAppletId")).toInt(), -1);
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeDockAppletId")).toInt(), -1);
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(),
             QString{});
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("recoverable-error"));
    QCOMPARE(persistedSuccess.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QStringLiteral("renderer-verification-failed"));

    PanelRegistry persistedFailure;
    QCOMPARE(persistedFailure.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), -1);
    QCOMPARE(persistedFailure.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("recoverable-error"));
    QCOMPARE(persistedFailure.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QStringLiteral("renderer-verification-failed"));

    const int recoveryRevision = persistedFailure.revision();
    QVERIFY(persistedFailure.commitVerifiedNativePanelAssociation(
        QStringLiteral("bottom"), 84, 91, QStringLiteral("token-b")));
    QCOMPARE(persistedFailure.revision(), recoveryRevision + 1);
    QCOMPARE(persistedFailure.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), 84);
    QCOMPARE(persistedFailure.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeDockAppletId")).toInt(), 91);
    QCOMPARE(persistedFailure.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(),
             QStringLiteral("token-b"));
    QCOMPARE(persistedFailure.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("ready"));
    QCOMPARE(persistedFailure.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QString{});
}

void PanelRegistryTest::persistsNativePanelRediscoveryOutcomes()
{
    PanelRegistry registry;
    QVERIFY(registry.commitVerifiedNativePanelAssociation(
        QStringLiteral("bottom"), 42, 73, QStringLiteral("token-a")));

    const int initialRevision = registry.revision();
    QVERIFY(!registry.rebindRecoveredNativePanelAssociation(
        QStringLiteral("bottom"), 84, -1, QStringLiteral("wrong-token")));
    QVERIFY(!registry.recordNativePanelRecoveryConflict(
        QStringLiteral("bottom"), QStringLiteral("wrong-token"),
        QStringLiteral("multiple-owned-hosts")));
    QCOMPARE(registry.revision(), initialRevision);

    QVERIFY(registry.rebindRecoveredNativePanelAssociation(
        QStringLiteral("bottom"), 84, -1, QStringLiteral("token-a")));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), 84);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeDockAppletId")).toInt(), -1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(),
             QStringLiteral("token-a"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("recovering"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QString{});

    QVERIFY(registry.recordNativePanelRecoveryConflict(
        QStringLiteral("bottom"), QStringLiteral("token-a"),
        QStringLiteral("multiple-owned-hosts")));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), 84);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeDockAppletId")).toInt(), -1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(),
             QStringLiteral("token-a"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("conflict"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QStringLiteral("multiple-owned-hosts"));

    PanelRegistry persistedConflict;
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), 84);
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(),
             QStringLiteral("token-a"));
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("conflict"));

    QVERIFY(persistedConflict.detachMissingNativePanelAssociation(
        QStringLiteral("bottom"), QStringLiteral("token-a"), true));
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativePanelId")).toInt(), -1);
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeDockAppletId")).toInt(), -1);
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeOwnershipToken")).toString(),
             QString{});
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("recovering"));
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QStringLiteral("owned-host-not-found"));

    QVERIFY(persistedConflict.commitVerifiedNativePanelAssociation(
        QStringLiteral("bottom"), 91, 102, QStringLiteral("token-hidden")));
    persistedConflict.setPanelValue(QStringLiteral("bottom"), QStringLiteral("visible"), false);
    QVERIFY(persistedConflict.detachMissingNativePanelAssociation(
        QStringLiteral("bottom"), QStringLiteral("token-hidden"), false));
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryState")).toString(),
             QStringLiteral("detached"));
    QCOMPARE(persistedConflict.panelValue(QStringLiteral("bottom"), QStringLiteral("nativeRecoveryError")).toString(),
             QStringLiteral("owned-host-not-found"));
}

void PanelRegistryTest::reconcilesNativeContainmentLifecycle()
{
    const ArchDock::NativeContainmentAssociation active =
        ArchDock::reconciledNativeContainmentAssociation(42, 73, true, true);
    QCOMPARE(active.containmentId, 42);
    QCOMPARE(active.controlAppletId, 73);

    const ArchDock::NativeContainmentAssociation missing =
        ArchDock::reconciledNativeContainmentAssociation(42, 73, false, false);
    QCOMPARE(missing.containmentId, -1);
    QCOMPARE(missing.controlAppletId, -1);

    const ArchDock::NativeContainmentAssociation invalid =
        ArchDock::reconciledNativeContainmentAssociation(-1, 73, true, true);
    QCOMPARE(invalid.containmentId, -1);
    QCOMPARE(invalid.controlAppletId, -1);

    const ArchDock::NativeContainmentAssociation noControl =
        ArchDock::reconciledNativeContainmentAssociation(42, -5, true, true);
    QCOMPARE(noControl.containmentId, 42);
    QCOMPARE(noControl.controlAppletId, -1);

    const ArchDock::NativeContainmentAssociation unowned =
        ArchDock::reconciledNativeContainmentAssociation(42, 73, true, false);
    QCOMPARE(unowned.containmentId, -1);
    QCOMPARE(unowned.controlAppletId, -1);
}

void PanelRegistryTest::classifiesNativeContainmentMatches()
{
    using MatchStatus = ArchDock::NativeContainmentMatchStatus;

    const ArchDock::NativeContainmentMatch queryFailed =
        ArchDock::classifyNativeContainmentMatch(std::nullopt);
    QCOMPARE(queryFailed.status, MatchStatus::QueryFailed);
    QCOMPARE(queryFailed.containmentId, -1);

    const ArchDock::NativeContainmentMatch missing =
        ArchDock::classifyNativeContainmentMatch(-1);
    QCOMPARE(missing.status, MatchStatus::Missing);
    QCOMPARE(missing.containmentId, -1);

    const ArchDock::NativeContainmentMatch unique =
        ArchDock::classifyNativeContainmentMatch(42);
    QCOMPARE(unique.status, MatchStatus::Unique);
    QCOMPARE(unique.containmentId, 42);

    const ArchDock::NativeContainmentMatch conflict =
        ArchDock::classifyNativeContainmentMatch(-2);
    QCOMPARE(conflict.status, MatchStatus::Conflict);
    QCOMPARE(conflict.containmentId, -1);

    const ArchDock::NativeContainmentMatch malformed =
        ArchDock::classifyNativeContainmentMatch(-3);
    QCOMPARE(malformed.status, MatchStatus::QueryFailed);
    QCOMPARE(malformed.containmentId, -1);
}

void PanelRegistryTest::selectsNativeContainmentLifecycleIntent()
{
    using HostStatus = ArchDock::NativeContainmentHostStatus;
    using Intent = ArchDock::NativeContainmentLifecycleIntent;
    using Presentation = ArchDock::NativeContainmentPresentation;
    using Request = ArchDock::NativeContainmentLifecycleRequest;
    using State = ArchDock::NativeContainmentLifecycleState;

    struct LifecycleCase
    {
        const char *name;
        Request request;
        State state;
        Intent expectedIntent;
    };

    const std::array<LifecycleCase, 16> cases{{
        {"create a new missing host",
         Request::CreateNew,
         {true, HostStatus::Missing, false, Presentation::Hidden},
         Intent::CreateHost},
        {"attach renderer before showing an owned host",
         Request::Synchronize,
         {true, HostStatus::Owned, false, Presentation::Hidden},
         Intent::AttachRenderer},
        {"show an owned renderer host for a visible record",
         Request::Synchronize,
         {true, HostStatus::Owned, true, Presentation::Hidden},
         Intent::ShowHost},
        {"hide an owned renderer host for a hidden record",
         Request::Synchronize,
         {false, HostStatus::Owned, true, Presentation::Shown},
         Intent::HideHost},
        {"recreate a missing host for a visible record",
         Request::Synchronize,
         {true, HostStatus::Missing, false, Presentation::Hidden},
         Intent::RecreateMissingHost},
        {"permanently remove an owned host",
         Request::RemovePermanently,
         {false, HostStatus::Owned, true, Presentation::Hidden},
         Intent::RemoveHostPermanently},
        {"do not remove an owned host without a verified renderer association",
         Request::RemovePermanently,
         {false, HostStatus::Owned, false, Presentation::Hidden},
         Intent::NoAction},
        {"leave a hidden missing host absent",
         Request::Synchronize,
         {false, HostStatus::Missing, false, Presentation::Hidden},
         Intent::NoAction},
        {"leave a removed missing host absent",
         Request::RemovePermanently,
         {false, HostStatus::Missing, false, Presentation::Hidden},
         Intent::NoAction},
        {"leave a converged visible host unchanged",
         Request::Synchronize,
         {true, HostStatus::Owned, true, Presentation::Shown},
         Intent::NoAction},
        {"leave a converged hidden host unchanged",
         Request::Synchronize,
         {false, HostStatus::Owned, true, Presentation::Hidden},
         Intent::NoAction},
        {"do not attach a renderer to an unowned host",
         Request::Synchronize,
         {true, HostStatus::UnownedOrUnverified, false, Presentation::Hidden},
         Intent::NoAction},
        {"do not show an unowned host",
         Request::Synchronize,
         {true, HostStatus::UnownedOrUnverified, true, Presentation::Hidden},
         Intent::NoAction},
        {"do not hide an unowned host",
         Request::Synchronize,
         {false, HostStatus::UnownedOrUnverified, true, Presentation::Shown},
         Intent::NoAction},
        {"do not remove an unowned host",
         Request::RemovePermanently,
         {false, HostStatus::UnownedOrUnverified, true, Presentation::Shown},
         Intent::NoAction},
        {"do not replace an unowned host during creation",
         Request::CreateNew,
         {true, HostStatus::UnownedOrUnverified, false, Presentation::Hidden},
         Intent::NoAction},
    }};

    for (const LifecycleCase &testCase : cases)
    {
        const Intent actualIntent = ArchDock::nativeContainmentLifecycleIntent(
            testCase.request,
            testCase.state);
        QVERIFY2(actualIntent == testCase.expectedIntent, testCase.name);
    }
}

void PanelRegistryTest::selectsFreePanelCreationIntent()
{
    using Intent = ArchDock::FreePanelCreationIntent;
    using Origin = ArchDock::FreePanelCreationOrigin;
    using State = ArchDock::FreePanelCreationDecisionState;

    struct CreationCase
    {
        const char *name;
        State state;
        Intent expectedIntent;
    };

    const std::array<CreationCase, 8> cases{{
        {"create a Studio host",
         {Origin::Studio, true, 0, false},
         Intent::CreateHost},
        {"adopt the requesting desktop applet",
         {Origin::ExistingApplet, true, 0, false},
         Intent::AdoptHost},
        {"create the first host for a template token",
         {Origin::TemplateBridge, true, 0, false},
         Intent::CreateHost},
        {"return the one verified host for a repeated template token",
         {Origin::TemplateBridge, true, 1, true},
         Intent::ReturnExisting},
        {"reject a stale host for a repeated template token",
         {Origin::TemplateBridge, true, 1, false},
         Intent::Reject},
        {"reject conflicting hosts for one template token",
         {Origin::TemplateBridge, true, 2, true},
         Intent::Reject},
        {"reject an invalid template request",
         {Origin::TemplateBridge, false, 0, false},
         Intent::Reject},
        {"reject an invalid existing-applet request",
         {Origin::ExistingApplet, false, 0, false},
         Intent::Reject},
    }};

    for (const CreationCase &testCase : cases)
    {
        const Intent actualIntent = ArchDock::freePanelCreationIntent(testCase.state);
        QVERIFY2(actualIntent == testCase.expectedIntent, testCase.name);
    }
}

void PanelRegistryTest::persistsFreePanelCreationTransactionState()
{
    PanelRegistry registry;
    const QString token = QStringLiteral("archdock-free-test-transaction");
    const QString panelId = registry.beginFreePanelCreation(token);
    QVERIFY(!panelId.isEmpty());

    const auto pending = registry.freeHostAssociation(panelId);
    QVERIFY(pending.has_value());
    QVERIFY(pending->state == PanelRegistry::FreeHostState::HostedStale);
    QCOMPARE(pending->desktopContainmentId, -1);
    QCOMPARE(pending->dockAppletId, -1);
    QCOMPARE(pending->ownershipToken, token);
    QCOMPARE(registry.panelValue(
                 panelId, QStringLiteral("freeCreationState")).toString(),
             QStringLiteral("pending"));

    QVERIFY(registry.commitVerifiedFreeHostAssociation(
        panelId,
        42,
        73,
        token,
        1,
        QStringLiteral("output:1"),
        QStringLiteral("desktop")));
    const auto committed = registry.freeHostAssociation(panelId);
    QVERIFY(committed.has_value());
    QVERIFY(committed->state == PanelRegistry::FreeHostState::HostedOwned);
    QCOMPARE(registry.panelValue(
                 panelId, QStringLiteral("freeCreationState")).toString(),
             QStringLiteral("pending"));

    QVERIFY(registry.completeFreePanelCreation(panelId, token));
    QCOMPARE(registry.panelValue(
                 panelId, QStringLiteral("freeCreationState")).toString(),
             QStringLiteral("complete"));
    QVERIFY(!registry.discardFreePanelCreation(panelId, token));

    PanelRegistry reloaded;
    const auto persisted = reloaded.freeHostAssociation(panelId);
    QVERIFY(persisted.has_value());
    QVERIFY(persisted->state == PanelRegistry::FreeHostState::HostedOwned);
    QCOMPARE(persisted->desktopContainmentId, 42);
    QCOMPARE(persisted->dockAppletId, 73);
    QCOMPARE(persisted->ownershipToken, token);
    QCOMPARE(reloaded.panelValue(
                 panelId, QStringLiteral("freeCreationState")).toString(),
             QStringLiteral("complete"));

    const QString recoveryToken = QStringLiteral("archdock-free-test-recovery");
    const QString recoveryPanelId = registry.beginFreePanelCreation(recoveryToken);
    QVERIFY(!recoveryPanelId.isEmpty());
    QVERIFY(registry.recordRecoverableFreePanelCreation(
        recoveryPanelId,
        84,
        91,
        recoveryToken,
        2,
        QStringLiteral("output:2"),
        QStringLiteral("association-persist-failed"),
        QStringLiteral("host-rollback-query-failed")));
    const auto recoverable = registry.freeHostAssociation(recoveryPanelId);
    QVERIFY(recoverable.has_value());
    QVERIFY(recoverable->state == PanelRegistry::FreeHostState::HostedStale);
    QCOMPARE(recoverable->desktopContainmentId, 84);
    QCOMPARE(recoverable->dockAppletId, 91);
    QCOMPARE(recoverable->ownershipToken, recoveryToken);
    QCOMPARE(registry.panelValue(
                 recoveryPanelId, QStringLiteral("freeCreationState")).toString(),
             QStringLiteral("rollback-pending"));
    QCOMPARE(registry.panelValue(
                 recoveryPanelId, QStringLiteral("freeCreationError")).toString(),
             QStringLiteral("association-persist-failed"));
    QCOMPARE(registry.panelValue(
                 recoveryPanelId, QStringLiteral("freeRollbackError")).toString(),
             QStringLiteral("host-rollback-query-failed"));
    QVERIFY(!registry.discardFreePanelCreation(
        recoveryPanelId, QStringLiteral("wrong-token")));
    QVERIFY(registry.discardFreePanelCreation(recoveryPanelId, recoveryToken));
    QVERIFY(!registry.panelIds().contains(recoveryPanelId));

    QTemporaryDir blockedRoot;
    QVERIFY(blockedRoot.isValid());
    const QString blockedPath = blockedRoot.filePath(QStringLiteral("not-a-directory"));
    QFile blocker(blockedPath);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.write("blocked");
    blocker.close();
    const QStringList idsBeforeFailure = registry.panelIds();
    {
        ScopedNativeSettingsPath blockedSettings(
            blockedPath, m_settingsDirectory.path());
        QVERIFY(registry.beginFreePanelCreation(
            QStringLiteral("archdock-free-test-allocation-failure")).isEmpty());
        QCOMPARE(registry.panelIds(), idsBeforeFailure);
    }
}

void PanelRegistryTest::rollsBackFreePanelCreationFailures()
{
    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QStringList panelIdsBefore = registry.panelIds();
        QTemporaryDir blockedRoot;
        QVERIFY(blockedRoot.isValid());
        const QString blockedPath = blockedRoot.filePath(QStringLiteral("not-a-directory"));
        QFile blocker(blockedPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.write("blocked");
        blocker.close();

        FreePanelHostHarness harness;
        ArchDock::FreePanelCreationResult result;
        {
            ScopedNativeSettingsPath blockedSettings(
                blockedPath, m_settingsDirectory.path());
            ArchDock::FreePanelController controller(registry, harness.operations());
            result = controller.create(studioFreePanelRequest());
        }
        QCOMPARE(result.errorCode, QStringLiteral("record-allocation-failed"));
        QCOMPARE(result.failureStage, QStringLiteral("record-allocation"));
        QVERIFY(!result.rollbackAttempted);
        QCOMPARE(registry.panelIds(), panelIdsBefore);
        QCOMPARE(harness.matchingCountCalls, 0);
        QCOMPARE(harness.createCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.bridgeScreen = std::nullopt;
        ArchDock::FreePanelCreationRequest request;
        request.origin = ArchDock::FreePanelCreationOrigin::TemplateBridge;
        request.bridgeContainmentId = 17;
        request.ownershipToken = QStringLiteral("archdock-free-template-17:test");
        const FreePanelControllerRun run = runFreePanelController(harness, request);
        QCOMPARE(run.result.errorCode, QStringLiteral("bootstrap-bridge-unverified"));
        QCOMPARE(run.result.failureStage, QStringLiteral("bridge-verification"));
        QVERIFY(!run.result.rollbackAttempted);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.bridgeScreenCalls, 1);
        QCOMPARE(harness.createCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.matchingCount = std::nullopt;
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QCOMPARE(run.result.errorCode, QStringLiteral("host-preflight-query-failed"));
        QCOMPARE(run.result.failureStage, QStringLiteral("host-preflight"));
        QVERIFY(run.result.rollbackAttempted);
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.matchingCountCalls, 1);
        QCOMPARE(harness.createCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.createResult.outcome = ArchDock::FreePanelHostMutationOutcome::NoHost;
        harness.createResult.host = {};
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QCOMPARE(run.result.errorCode, QStringLiteral("host-creation-failed"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.createCalls, 1);
        QCOMPARE(harness.exactRemovalCalls, 0);
        QCOMPARE(harness.identityRemovalCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.createResult.outcome =
            ArchDock::FreePanelHostMutationOutcome::CandidateUnverified;
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QCOMPARE(run.result.errorCode, QStringLiteral("host-creation-unverified"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.exactRemovalCalls, 1);
        QCOMPARE(harness.identityRemovalCalls, 0);
        QCOMPARE(harness.removedHost.desktopContainmentId, 42);
        QCOMPARE(harness.removedHost.dockAppletId, 73);
        QVERIFY(harness.removedPanelId.startsWith(QStringLiteral("free-")));
        QVERIFY(harness.removedToken.startsWith(QStringLiteral("archdock-free-")));
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.createResult.outcome =
            ArchDock::FreePanelHostMutationOutcome::Indeterminate;
        harness.createResult.host = {42, -1};
        harness.identityRemoval = ArchDock::FreePanelRemovalOutcome::Removed;
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QCOMPARE(run.result.errorCode, QStringLiteral("host-creation-indeterminate"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.exactRemovalCalls, 0);
        QCOMPARE(harness.identityRemovalCalls, 1);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.verificationResults = {
            ArchDock::FreePanelHostVerificationOutcome::Missing};
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QCOMPARE(run.result.errorCode, QStringLiteral("host-verification-missing"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.verifyCalls, 1);
        QCOMPARE(harness.exactRemovalCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        QTemporaryDir blockedRoot;
        QVERIFY(blockedRoot.isValid());
        const QString blockedPath = blockedRoot.filePath(QStringLiteral("not-a-directory"));
        QFile blocker(blockedPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.write("blocked");
        blocker.close();

        FreePanelHostHarness harness;
        harness.onVerify = [blockedPath](int callIndex)
        {
            if (callIndex == 0)
            {
                QSettings::setPath(
                    QSettings::NativeFormat,
                    QSettings::UserScope,
                    blockedPath);
            }
        };
        harness.onExactRemoval = [this]
        {
            QSettings::setPath(
                QSettings::NativeFormat,
                QSettings::UserScope,
                m_settingsDirectory.path());
        };
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QSettings::setPath(
            QSettings::NativeFormat,
            QSettings::UserScope,
            m_settingsDirectory.path());
        QCOMPARE(run.result.errorCode, QStringLiteral("association-persist-failed"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.exactRemovalCalls, 1);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QStringList panelIdsBefore = registry.panelIds();
        bool associationCorrupted = false;
        QString corruptedPanelId;
        const QMetaObject::Connection connection = connect(
            &registry,
            &PanelRegistry::revisionChanged,
            &registry,
            [&]
            {
                if (associationCorrupted)
                {
                    return;
                }
                for (const QString &panelId : registry.panelIds())
                {
                    const auto association = registry.freeHostAssociation(panelId);
                    if (!association.has_value() ||
                        association->state != PanelRegistry::FreeHostState::HostedOwned ||
                        registry.panelValue(
                            panelId,
                            QStringLiteral("freeCreationState")).toString() !=
                            QStringLiteral("pending"))
                    {
                        continue;
                    }

                    associationCorrupted = true;
                    corruptedPanelId = panelId;
                    registry.updatePanel(
                        panelId,
                        {{QStringLiteral("screenId"),
                          QStringLiteral("output:readback-mismatch")}});
                    return;
                }
            });

        FreePanelHostHarness harness;
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelCreationResult result = controller.create(
            studioFreePanelRequest());
        disconnect(connection);

        QVERIFY(associationCorrupted);
        QVERIFY(!corruptedPanelId.isEmpty());
        QCOMPARE(result.errorCode, QStringLiteral("association-readback-failed"));
        QCOMPARE(result.failureStage, QStringLiteral("association-readback"));
        QVERIFY(result.rollbackAttempted);
        QVERIFY(result.rollbackSucceeded);
        QCOMPARE(registry.panelIds(), panelIdsBefore);
        QVERIFY(!registry.panelIds().contains(corruptedPanelId));
        QCOMPARE(harness.exactRemovalCalls, 1);
        QCOMPARE(harness.identityRemovalCalls, 0);
        QCOMPARE(harness.removedHost.desktopContainmentId, 42);
        QCOMPARE(harness.removedHost.dockAppletId, 73);
        QCOMPARE(harness.removedPanelId, corruptedPanelId);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.bridgeRemoval = ArchDock::FreePanelRemovalOutcome::Refused;
        ArchDock::FreePanelCreationRequest request;
        request.origin = ArchDock::FreePanelCreationOrigin::TemplateBridge;
        request.bridgeContainmentId = 19;
        request.ownershipToken = QStringLiteral("archdock-free-template-19:test");
        const FreePanelControllerRun run = runFreePanelController(harness, request);
        QCOMPARE(run.result.errorCode, QStringLiteral("bootstrap-cleanup-refused"));
        QCOMPARE(run.result.failureStage, QStringLiteral("bootstrap-cleanup"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.bridgeScreenCalls, 1);
        QCOMPARE(harness.bridgeRemovalCalls, 1);
        QCOMPARE(harness.exactRemovalCalls, 1);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.verificationResults = {
            ArchDock::FreePanelHostVerificationOutcome::Owned,
            ArchDock::FreePanelHostVerificationOutcome::Missing};
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QCOMPARE(run.result.errorCode, QStringLiteral("final-host-readback-missing"));
        QCOMPARE(run.result.failureStage, QStringLiteral("final-readback"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.verifyCalls, 2);
        QCOMPARE(harness.exactRemovalCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        QTemporaryDir blockedRoot;
        QVERIFY(blockedRoot.isValid());
        const QString blockedPath = blockedRoot.filePath(QStringLiteral("not-a-directory"));
        QFile blocker(blockedPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.write("blocked");
        blocker.close();

        FreePanelHostHarness harness;
        harness.onVerify = [blockedPath](int callIndex)
        {
            if (callIndex == 1)
            {
                QSettings::setPath(
                    QSettings::NativeFormat,
                    QSettings::UserScope,
                    blockedPath);
            }
        };
        harness.onExactRemoval = [this]
        {
            QSettings::setPath(
                QSettings::NativeFormat,
                QSettings::UserScope,
                m_settingsDirectory.path());
        };
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QSettings::setPath(
            QSettings::NativeFormat,
            QSettings::UserScope,
            m_settingsDirectory.path());
        QCOMPARE(
            run.result.errorCode,
            QStringLiteral("creation-completion-persist-failed"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.verifyCalls, 2);
        QCOMPARE(harness.exactRemovalCalls, 1);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QVERIFY(run.result.success);
        QCOMPARE(run.result.status, QStringLiteral("created"));
        QVERIFY(run.result.ownershipVerified);
        QCOMPARE(run.panelIdsAfter.size(), run.panelIdsBefore.size() + 1);
        QCOMPARE(harness.matchingCountCalls, 1);
        QCOMPARE(harness.createCalls, 1);
        QCOMPARE(harness.verifyCalls, 2);
        QCOMPARE(harness.exactRemovalCalls, 0);
        PanelRegistry reloaded;
        const auto association = reloaded.freeHostAssociation(run.result.panelId);
        QVERIFY(association.has_value());
        QCOMPARE(association->state, PanelRegistry::FreeHostState::HostedOwned);
        QCOMPARE(association->desktopContainmentId, 42);
        QCOMPARE(association->dockAppletId, 73);
        QVERIFY(association->ownershipToken.startsWith(
            QStringLiteral("archdock-free-")));
        QCOMPARE(reloaded.panelValue(
                     run.result.panelId,
                     QStringLiteral("freeCreationState")).toString(),
                 QStringLiteral("complete"));
    }

    {
        clearPanelRegistrySettings();
        ArchDock::FreePanelCreationRequest request;
        request.origin = ArchDock::FreePanelCreationOrigin::TemplateBridge;
        request.bridgeContainmentId = 29;
        request.ownershipToken = QStringLiteral("archdock-free-template-29:test");

        FreePanelHostHarness firstHarness;
        const FreePanelControllerRun firstRun = runFreePanelController(
            firstHarness, request);
        QVERIFY(firstRun.result.success);
        QCOMPARE(firstRun.result.status, QStringLiteral("created"));
        QCOMPARE(firstHarness.bridgeRemovalCalls, 1);
        PanelRegistry afterFirstCreation;
        const auto firstAssociation = afterFirstCreation.freeHostAssociation(
            firstRun.result.panelId);
        QVERIFY(firstAssociation.has_value());
        QCOMPARE(firstAssociation->state, PanelRegistry::FreeHostState::HostedOwned);
        QCOMPARE(firstAssociation->ownershipToken, request.ownershipToken);

        FreePanelHostHarness repeatedHarness;
        repeatedHarness.bridgeRemoval =
            ArchDock::FreePanelRemovalOutcome::AlreadyAbsent;
        const FreePanelControllerRun repeatedRun = runFreePanelController(
            repeatedHarness, request);
        QVERIFY(repeatedRun.result.success);
        QCOMPARE(repeatedRun.result.status, QStringLiteral("existing"));
        QCOMPARE(repeatedRun.result.panelId, firstRun.result.panelId);
        QCOMPARE(repeatedRun.panelIdsAfter, repeatedRun.panelIdsBefore);
        QCOMPARE(repeatedHarness.createCalls, 0);
        QCOMPARE(repeatedHarness.verifyCalls, 1);
        QCOMPARE(repeatedHarness.bridgeRemovalCalls, 1);
        PanelRegistry afterRepeatedCreation;
        QCOMPARE(afterRepeatedCreation.panelIds(), repeatedRun.panelIdsBefore);
        const auto repeatedAssociation = afterRepeatedCreation.freeHostAssociation(
            repeatedRun.result.panelId);
        QVERIFY(repeatedAssociation.has_value());
        QCOMPARE(repeatedAssociation->ownershipToken, request.ownershipToken);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.adoptResult = {
            ArchDock::FreePanelHostMutationOutcome::Verified,
            {55, 66},
            2};
        ArchDock::FreePanelCreationRequest request;
        request.origin = ArchDock::FreePanelCreationOrigin::ExistingApplet;
        request.existingDesktopContainmentId = 55;
        request.existingDockAppletId = 66;
        const FreePanelControllerRun run = runFreePanelController(harness, request);
        QVERIFY(run.result.success);
        QCOMPARE(run.result.status, QStringLiteral("adopted"));
        QCOMPARE(run.result.desktopContainmentId, 55);
        QCOMPARE(run.result.dockAppletId, 66);
        QCOMPARE(harness.adoptCalls, 1);
        QCOMPARE(harness.verifyCalls, 2);
        PanelRegistry reloaded;
        const auto association = reloaded.freeHostAssociation(run.result.panelId);
        QVERIFY(association.has_value());
        QCOMPARE(association->state, PanelRegistry::FreeHostState::HostedOwned);
        QCOMPARE(association->desktopContainmentId, 55);
        QCOMPARE(association->dockAppletId, 66);
        QCOMPARE(reloaded.panelValue(
                     run.result.panelId,
                     QStringLiteral("freeCreationState")).toString(),
                 QStringLiteral("complete"));
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.adoptResult.outcome = ArchDock::FreePanelHostMutationOutcome::NoHost;
        ArchDock::FreePanelCreationRequest request;
        request.origin = ArchDock::FreePanelCreationOrigin::ExistingApplet;
        request.existingDesktopContainmentId = 900;
        request.existingDockAppletId = 901;
        const FreePanelControllerRun run = runFreePanelController(harness, request);
        QCOMPARE(run.result.errorCode, QStringLiteral("host-adoption-failed"));
        QVERIFY(run.result.rollbackSucceeded);
        QCOMPARE(run.panelIdsAfter, run.panelIdsBefore);
        QCOMPARE(harness.exactRemovalCalls, 0);
        QCOMPARE(harness.identityRemovalCalls, 0);
    }
}

void PanelRegistryTest::reportsFreePanelRollbackFailures()
{
    {
        clearPanelRegistrySettings();
        QTemporaryDir blockedRoot;
        QVERIFY(blockedRoot.isValid());
        const QString blockedPath = blockedRoot.filePath(QStringLiteral("not-a-directory"));
        QFile blocker(blockedPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.write("blocked");
        blocker.close();

        FreePanelHostHarness harness;
        harness.exactRemoval = ArchDock::FreePanelRemovalOutcome::Refused;
        harness.onVerify = [blockedPath](int callIndex)
        {
            if (callIndex == 0)
            {
                QSettings::setPath(
                    QSettings::NativeFormat,
                    QSettings::UserScope,
                    blockedPath);
            }
        };
        harness.onExactRemoval = [this]
        {
            QSettings::setPath(
                QSettings::NativeFormat,
                QSettings::UserScope,
                m_settingsDirectory.path());
        };
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QSettings::setPath(
            QSettings::NativeFormat,
            QSettings::UserScope,
            m_settingsDirectory.path());

        QCOMPARE(run.result.errorCode, QStringLiteral("association-persist-failed"));
        QCOMPARE(run.result.failureStage, QStringLiteral("association-persistence"));
        QVERIFY(run.result.rollbackAttempted);
        QVERIFY(!run.result.rollbackSucceeded);
        QCOMPARE(run.result.rollbackErrorCode, QStringLiteral("host-rollback-refused"));
        QCOMPARE(run.result.status, QStringLiteral("rollback-pending"));
        QVERIFY(run.result.recoverable);
        QCOMPARE(harness.exactRemovalCalls, 1);
        QCOMPARE(harness.identityRemovalCalls, 0);
        QCOMPARE(harness.removedPanelId, run.result.panelId);

        PanelRegistry reloaded;
        const auto retained = reloaded.freeHostAssociation(run.result.panelId);
        QVERIFY(retained.has_value());
        QVERIFY(retained->state == PanelRegistry::FreeHostState::HostedStale);
        QCOMPARE(retained->desktopContainmentId, 42);
        QCOMPARE(retained->dockAppletId, 73);
        QCOMPARE(retained->ownershipToken, harness.removedToken);
        QCOMPARE(reloaded.panelValue(
                     run.result.panelId,
                     QStringLiteral("freeCreationError")).toString(),
                 QStringLiteral("association-persist-failed"));
        QCOMPARE(reloaded.panelValue(
                     run.result.panelId,
                     QStringLiteral("freeRollbackError")).toString(),
                 QStringLiteral("host-rollback-refused"));
    }

    {
        clearPanelRegistrySettings();
        QTemporaryDir blockedRoot;
        QVERIFY(blockedRoot.isValid());
        const QString blockedPath = blockedRoot.filePath(QStringLiteral("not-a-directory"));
        QFile blocker(blockedPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.write("blocked");
        blocker.close();

        FreePanelHostHarness harness;
        harness.createResult.outcome = ArchDock::FreePanelHostMutationOutcome::NoHost;
        harness.createResult.host = {};
        harness.onCreate = [blockedPath]
        {
            QSettings::setPath(
                QSettings::NativeFormat,
                QSettings::UserScope,
                blockedPath);
        };
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QSettings::setPath(
            QSettings::NativeFormat,
            QSettings::UserScope,
            m_settingsDirectory.path());

        QCOMPARE(run.result.errorCode, QStringLiteral("host-creation-failed"));
        QVERIFY(!run.result.rollbackSucceeded);
        QCOMPARE(
            run.result.rollbackErrorCode,
            QStringLiteral(
                "record-discard-failed+recovery-record-persist-failed"));
        QVERIFY(run.result.recoverable);
        QVERIFY(run.panelIdsAfter.contains(run.result.panelId));
        QCOMPARE(harness.createCalls, 1);
        QCOMPARE(harness.exactRemovalCalls, 0);
        QCOMPARE(harness.identityRemovalCalls, 0);

        PanelRegistry reloaded;
        const auto retained = reloaded.freeHostAssociation(run.result.panelId);
        QVERIFY(retained.has_value());
        QVERIFY(retained->state == PanelRegistry::FreeHostState::HostedStale);
        QVERIFY(retained->ownershipToken.startsWith(QStringLiteral("archdock-free-")));
        QCOMPARE(reloaded.panelValue(
                     run.result.panelId,
                     QStringLiteral("freeCreationState")).toString(),
                 QStringLiteral("pending"));
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.createResult.outcome =
            ArchDock::FreePanelHostMutationOutcome::Indeterminate;
        harness.createResult.host = {42, -1};
        harness.identityRemoval = ArchDock::FreePanelRemovalOutcome::AlreadyAbsent;
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QCOMPARE(run.result.errorCode, QStringLiteral("host-creation-indeterminate"));
        QCOMPARE(
            run.result.rollbackErrorCode,
            QStringLiteral("host-rollback-absence-unverified"));
        QVERIFY(run.result.recoverable);
        QCOMPARE(harness.identityRemovalCalls, 1);
        QCOMPARE(harness.exactRemovalCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        FreePanelHostHarness harness;
        harness.createResult.outcome =
            ArchDock::FreePanelHostMutationOutcome::Indeterminate;
        harness.createResult.host = {42, -1};
        harness.identityRemoval = ArchDock::FreePanelRemovalOutcome::QueryFailed;
        const FreePanelControllerRun run = runFreePanelController(
            harness, studioFreePanelRequest());
        QCOMPARE(run.result.errorCode, QStringLiteral("host-creation-indeterminate"));
        QCOMPARE(
            run.result.rollbackErrorCode,
            QStringLiteral("host-rollback-query-failed"));
        QVERIFY(run.result.recoverable);
        QCOMPARE(harness.identityRemovalCalls, 1);
        QCOMPARE(harness.exactRemovalCalls, 0);
        QCOMPARE(harness.createCalls, 1);
    }
}

void PanelRegistryTest::recoversFreePanelHostLifecycle()
{
    using DiscoveryOutcome = ArchDock::FreePanelHostDiscoveryOutcome;
    using LifecycleOutcome = ArchDock::FreePanelLifecycleOutcome;

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-recovery-unique");
        const QString panelId = createOwnedFreePanelRecord(registry, token, 900, 901, 0);
        QVERIFY(!panelId.isEmpty());
        registry.updatePanel(
            panelId,
            {{QStringLiteral("freeHostState"), QStringLiteral("hosted-stale")}});

        FreePanelHostHarness harness;
        harness.discoveryResult = {DiscoveryOutcome::Unique, {42, 73}, 2};
        QSignalSpy revisionSpy(&registry, &PanelRegistry::revisionChanged);
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult first = controller.synchronize(panelId);
        QVERIFY(first.success);
        QCOMPARE(first.outcome, LifecycleOutcome::Rebound);
        const auto rebound = registry.freeHostAssociation(panelId);
        QVERIFY(rebound.has_value());
        QCOMPARE(rebound->state, PanelRegistry::FreeHostState::HostedOwned);
        QCOMPARE(rebound->desktopContainmentId, 42);
        QCOMPARE(rebound->dockAppletId, 73);
        QCOMPARE(rebound->ownershipToken, token);
        QCOMPARE(rebound->screenIndex, 2);
        QCOMPARE(rebound->screenId, QStringLiteral("output:2"));
        QCOMPARE(registry.panelValue(
                     panelId, QStringLiteral("freeRecoveryError")).toString(),
                 QString{});
        const int revisionCountAfterFirstSync = revisionSpy.count();

        const ArchDock::FreePanelLifecycleResult repeated = controller.synchronize(panelId);
        QVERIFY(repeated.success);
        QCOMPARE(repeated.outcome, LifecycleOutcome::Rebound);
        QCOMPARE(harness.discoverCalls, 2);
        QCOMPARE(harness.verifyCalls, 2);
        QCOMPARE(revisionSpy.count(), revisionCountAfterFirstSync);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-recovery-missing");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.discoveryResult = {DiscoveryOutcome::Missing, {}, -1};
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult missing = controller.synchronize(panelId);
        QVERIFY(missing.success);
        QCOMPARE(missing.outcome, LifecycleOutcome::Detached);
        const auto detached = registry.freeHostAssociation(panelId);
        QVERIFY(detached.has_value());
        QCOMPARE(detached->state, PanelRegistry::FreeHostState::Detached);
        QCOMPARE(detached->desktopContainmentId, -1);
        QCOMPARE(detached->dockAppletId, -1);
        QCOMPARE(detached->ownershipToken, QString{});
        QCOMPARE(registry.panelValue(
                     panelId, QStringLiteral("freeRecoveryError")).toString(),
                 QStringLiteral("owned-host-not-found"));

        const ArchDock::FreePanelLifecycleResult repeated = controller.synchronize(panelId);
        QVERIFY(repeated.success);
        QCOMPARE(repeated.outcome, LifecycleOutcome::Detached);
        QCOMPARE(harness.discoverCalls, 1);
        QCOMPARE(harness.verifyCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-recovery-conflict");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());
        const auto before = registry.freeHostAssociation(panelId);

        FreePanelHostHarness harness;
        harness.discoveryResult = {DiscoveryOutcome::Conflict, {}, -1};
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult conflict = controller.synchronize(panelId);
        QVERIFY(!conflict.success);
        QCOMPARE(conflict.outcome, LifecycleOutcome::Conflict);
        const auto retained = registry.freeHostAssociation(panelId);
        QVERIFY(before.has_value());
        QVERIFY(retained.has_value());
        QCOMPARE(retained->state, PanelRegistry::FreeHostState::HostedStale);
        QCOMPARE(retained->desktopContainmentId, before->desktopContainmentId);
        QCOMPARE(retained->dockAppletId, before->dockAppletId);
        QCOMPARE(retained->ownershipToken, token);
        QCOMPARE(registry.panelValue(
                     panelId, QStringLiteral("freeRecoveryError")).toString(),
                 QStringLiteral("owned-host-conflict"));
        QCOMPARE(harness.verifyCalls, 0);
        QCOMPARE(harness.exactRemovalCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-recovery-query");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.discoveryResult = {DiscoveryOutcome::QueryFailed, {}, -1};
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult queryFailed = controller.synchronize(panelId);
        QVERIFY(!queryFailed.success);
        QCOMPARE(queryFailed.outcome, LifecycleOutcome::QueryFailed);
        const auto retained = registry.freeHostAssociation(panelId);
        QVERIFY(retained.has_value());
        QCOMPARE(retained->desktopContainmentId, 42);
        QCOMPARE(retained->dockAppletId, 73);
        QCOMPARE(retained->ownershipToken, token);
        QCOMPARE(registry.panelValue(
                     panelId, QStringLiteral("freeRecoveryError")).toString(),
                 QStringLiteral("owned-host-query-failed"));
        QCOMPARE(harness.verifyCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-recovery-unowned");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.verificationResults = {
            ArchDock::FreePanelHostVerificationOutcome::UnownedOrUnverified};
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult refused = controller.synchronize(panelId);
        QVERIFY(!refused.success);
        QCOMPARE(refused.outcome, LifecycleOutcome::Refused);
        QVERIFY(registry.panelIds().contains(panelId));
        QCOMPARE(harness.exactRemovalCalls, 0);
    }
}

void PanelRegistryTest::removesFreePanelHostLifecycleSafely()
{
    using DiscoveryOutcome = ArchDock::FreePanelHostDiscoveryOutcome;
    using LifecycleOutcome = ArchDock::FreePanelLifecycleOutcome;
    using RemovalOutcome = ArchDock::FreePanelRemovalOutcome;

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-removal-owned");
        const QString panelId = createOwnedFreePanelRecord(registry, token, 900, 901, 0);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.discoveryResult = {DiscoveryOutcome::Unique, {42, 73}, 2};
        bool recordPresentDuringRemoval = false;
        bool associationReboundBeforeRemoval = false;
        harness.onExactRemoval = [&]
        {
            recordPresentDuringRemoval = registry.panelIds().contains(panelId);
            const auto association = registry.freeHostAssociation(panelId);
            associationReboundBeforeRemoval = association.has_value() &&
                association->state == PanelRegistry::FreeHostState::HostedOwned &&
                association->desktopContainmentId == 42 &&
                association->dockAppletId == 73 &&
                association->ownershipToken == token;
        };

        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult removed = controller.remove(panelId);
        QVERIFY(removed.success);
        QCOMPARE(removed.outcome, LifecycleOutcome::Removed);
        QVERIFY(recordPresentDuringRemoval);
        QVERIFY(associationReboundBeforeRemoval);
        QVERIFY(!registry.panelIds().contains(panelId));
        QCOMPARE(harness.exactRemovalCalls, 1);
        QCOMPARE(harness.removedHost.desktopContainmentId, 42);
        QCOMPARE(harness.removedHost.dockAppletId, 73);
        QCOMPARE(harness.removedPanelId, panelId);
        QCOMPARE(harness.removedToken, token);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-removal-missing");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.discoveryResult = {DiscoveryOutcome::Missing, {}, -1};
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult absent = controller.remove(panelId);
        QVERIFY(absent.success);
        QCOMPARE(absent.outcome, LifecycleOutcome::AlreadyAbsent);
        QVERIFY(!registry.panelIds().contains(panelId));
        QCOMPARE(harness.exactRemovalCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-removal-already-absent");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.exactRemoval = RemovalOutcome::AlreadyAbsent;
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult absent = controller.remove(panelId);
        QVERIFY(absent.success);
        QCOMPARE(absent.outcome, LifecycleOutcome::AlreadyAbsent);
        QVERIFY(!registry.panelIds().contains(panelId));
        QCOMPARE(harness.exactRemovalCalls, 1);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-removal-conflict");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.discoveryResult = {DiscoveryOutcome::Conflict, {}, -1};
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult conflict = controller.remove(panelId);
        QVERIFY(!conflict.success);
        QCOMPARE(conflict.outcome, LifecycleOutcome::Conflict);
        QVERIFY(registry.panelIds().contains(panelId));
        QCOMPARE(registry.freeHostAssociation(panelId)->ownershipToken, token);
        QCOMPARE(harness.exactRemovalCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-removal-refused");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.exactRemoval = RemovalOutcome::Refused;
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult refused = controller.remove(panelId);
        QVERIFY(!refused.success);
        QCOMPARE(refused.outcome, LifecycleOutcome::Refused);
        QVERIFY(registry.panelIds().contains(panelId));
        QCOMPARE(registry.freeHostAssociation(panelId)->ownershipToken, token);
        QCOMPARE(registry.panelValue(
                     panelId, QStringLiteral("freeRecoveryError")).toString(),
                 QStringLiteral("owned-host-removal-refused"));
        QCOMPARE(harness.exactRemovalCalls, 1);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-removal-query");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.exactRemoval = RemovalOutcome::QueryFailed;
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult queryFailed = controller.remove(panelId);
        QVERIFY(!queryFailed.success);
        QCOMPARE(queryFailed.outcome, LifecycleOutcome::QueryFailed);
        QVERIFY(registry.panelIds().contains(panelId));
        QCOMPARE(registry.freeHostAssociation(panelId)->ownershipToken, token);
        QCOMPARE(harness.exactRemovalCalls, 1);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-removal-unowned");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        FreePanelHostHarness harness;
        harness.verificationResults = {
            ArchDock::FreePanelHostVerificationOutcome::UnownedOrUnverified};
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult refused = controller.remove(panelId);
        QVERIFY(!refused.success);
        QCOMPARE(refused.outcome, LifecycleOutcome::Refused);
        QVERIFY(registry.panelIds().contains(panelId));
        QCOMPARE(harness.exactRemovalCalls, 0);
    }

    {
        clearPanelRegistrySettings();
        PanelRegistry registry;
        const QString token = QStringLiteral("archdock-free-removal-persist-failure");
        const QString panelId = createOwnedFreePanelRecord(registry, token);
        QVERIFY(!panelId.isEmpty());

        QTemporaryDir blockedRoot;
        QVERIFY(blockedRoot.isValid());
        const QString blockedPath = blockedRoot.filePath(QStringLiteral("not-a-directory"));
        QFile blocker(blockedPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.write("blocked");
        blocker.close();
        FreePanelHostHarness harness;
        harness.onExactRemoval = [&registry, blockedPath]
        {
            connect(
                &registry,
                &PanelRegistry::revisionChanged,
                &registry,
                [blockedPath]
                {
                    QSettings::setPath(
                        QSettings::NativeFormat,
                        QSettings::UserScope,
                        blockedPath);
                },
                Qt::SingleShotConnection);
        };
        ArchDock::FreePanelController controller(registry, harness.operations());
        const ArchDock::FreePanelLifecycleResult failed = controller.remove(panelId);
        QSettings::setPath(
            QSettings::NativeFormat,
            QSettings::UserScope,
            m_settingsDirectory.path());

        QVERIFY(!failed.success);
        QCOMPARE(failed.outcome, LifecycleOutcome::PersistenceFailed);
        QCOMPARE(failed.errorCode, QStringLiteral("free-record-removal-persist-failed"));
        QCOMPARE(harness.exactRemovalCalls, 1);
        const auto retained = registry.freeHostAssociation(panelId);
        QVERIFY(retained.has_value());
        QCOMPARE(retained->state, PanelRegistry::FreeHostState::Detached);
        QCOMPARE(retained->desktopContainmentId, -1);
        QCOMPARE(retained->dockAppletId, -1);
        QCOMPARE(retained->ownershipToken, QString{});
    }
}

void PanelRegistryTest::resolvesStableScreenIdentityBeforeFallbackIndex()
{
    using Reason = ArchDock::ScreenResolutionReason;

    const QStringList firstOrder{QStringLiteral("output:DP-1"), QStringLiteral("output:HDMI-A-1")};
    const ArchDock::ScreenResolution firstMatch = ArchDock::resolveScreen(
        firstOrder, QStringLiteral("output:HDMI-A-1"), 0);
    QCOMPARE(firstMatch.index, 1);
    QCOMPARE(firstMatch.reason, Reason::StableIdMatch);
    QVERIFY(!firstMatch.usedFallback);

    const QStringList reordered{QStringLiteral("output:HDMI-A-1"), QStringLiteral("output:DP-1")};
    const ArchDock::ScreenResolution reorderedMatch = ArchDock::resolveScreen(
        reordered, QStringLiteral("output:HDMI-A-1"), 1);
    QCOMPARE(reorderedMatch.index, 0);
    QCOMPARE(reorderedMatch.reason, Reason::StableIdMatch);
    QVERIFY(!reorderedMatch.usedFallback);

    const ArchDock::ScreenResolution storedFallback = ArchDock::resolveScreen(
        reordered, QStringLiteral("output:missing"), 1);
    QCOMPARE(storedFallback.index, 1);
    QCOMPARE(storedFallback.reason, Reason::StoredIndexFallback);
    QVERIFY(storedFallback.usedFallback);

    const ArchDock::ScreenResolution stableMatchAfterBounding = ArchDock::resolveScreen(
        reordered, QStringLiteral("output:DP-1"), 9);
    QCOMPARE(stableMatchAfterBounding.index, 1);
    QCOMPARE(stableMatchAfterBounding.reason, Reason::StableIdMatch);
    QVERIFY(!stableMatchAfterBounding.usedFallback);

    const ArchDock::ScreenResolution boundedFallback = ArchDock::resolveScreen(
        reordered, QStringLiteral("output:missing"), 9);
    QCOMPARE(boundedFallback.index, 1);
    QCOMPARE(boundedFallback.reason, Reason::BoundedIndexFallback);
    QVERIFY(boundedFallback.usedFallback);

    const QStringList duplicated{QStringLiteral("output:DP-1"), QStringLiteral("output:DP-1")};
    QCOMPARE(ArchDock::resolvedScreenIndex(duplicated, QStringLiteral("output:DP-1"), 1), 1);

    const ArchDock::ScreenResolution noScreens = ArchDock::resolveScreen(
        {}, QStringLiteral("output:DP-1"), 0);
    QCOMPARE(noScreens.index, -1);
    QCOMPARE(noScreens.reason, Reason::NoScreens);
    QVERIFY(!noScreens.usedFallback);
}

void PanelRegistryTest::reservesAndOffsetsOnlySameScreenPanels()
{
    const QList<ArchDock::EdgePanel> panels{
        {QStringLiteral("first"), QStringLiteral("left"), 0, 40, true},
        {QStringLiteral("second"), QStringLiteral("left"), 0, 60, true},
        {QStringLiteral("other-screen"), QStringLiteral("left"), 1, 100, true},
        {QStringLiteral("hidden"), QStringLiteral("left"), 0, 80, false},
        {QStringLiteral("top"), QStringLiteral("top"), 0, 36, true}};

    QCOMPARE(ArchDock::edgeReserve(panels, QStringLiteral("left"), 0), 124);
    QCOMPARE(ArchDock::edgeReserve(panels, QStringLiteral("left"), 1), 116);
    QCOMPARE(ArchDock::edgeOffset(panels, QStringLiteral("first")), 8);
    QCOMPARE(ArchDock::edgeOffset(panels, QStringLiteral("second")), 56);
    QCOMPARE(ArchDock::edgeOffset(panels, QStringLiteral("other-screen")), 8);
    QCOMPARE(ArchDock::edgeOffset(panels, QStringLiteral("missing")), 8);
}

void PanelRegistryTest::concealsOnlyForRelevantActiveWindows()
{
    const QRect panelGeometry(0, 1040, 1920, 40);
    const QList<ArchDock::WindowOcclusion> windows{
        {QRect(0, 0, 1920, 1080), 0, true, false, false, false},
        {QRect(0, 0, 1920, 1080), 0, true, false, true, false},
        {QRect(0, 0, 1920, 1080), 1, true, false, true, false},
        {QRect(0, 1040, 1920, 40), 0, false, false, true, false},
        {QRect(0, 1040, 1920, 40), 0, true, true, true, false}};

    QVERIFY(ArchDock::shouldConcealForWindows(QStringLiteral("dodge"), panelGeometry, 0, windows));
    QVERIFY(ArchDock::shouldConcealForWindows(QStringLiteral("cover"), panelGeometry, 0, windows));
    QVERIFY(!ArchDock::shouldConcealForWindows(QStringLiteral("always"), panelGeometry, 0, windows));
    QVERIFY(ArchDock::shouldConcealForWindows(QStringLiteral("auto-hide"), panelGeometry, 0, windows));
    QVERIFY(!ArchDock::shouldConcealForWindows(
        QStringLiteral("cover"),
        panelGeometry,
        0,
        {{QRect(0, 0, 1920, 1080), 0, true, false, false, false}}));
    QVERIFY(!ArchDock::shouldConcealForWindows(
        QStringLiteral("dodge"),
        panelGeometry,
        1,
        {{QRect(0, 1040, 1920, 40), 0, true, false, true, false}}));
    QVERIFY(!ArchDock::shouldConcealForWindows(
        QStringLiteral("cover"),
        panelGeometry,
        0,
        {{QRect(0, 1040, 1920, 10), 0, true, false, false, false}}));
}

void PanelRegistryTest::persistsReducedMotionPreference()
{
    DockSettings settings;
    QVERIFY(!settings.reducedMotion());

    settings.setReducedMotion(true);
    QVERIFY(settings.reducedMotion());

    DockSettings restored;
    QVERIFY(restored.reducedMotion());
}

void PanelRegistryTest::normalizesLayoutAndMotionValues()
{
    PanelRegistry registry;

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("layout")).toString(),
             QStringLiteral("adaptive"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("iconAnimation")).toString(),
             QStringLiteral("scale"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("folderLayout")).toString(),
             QStringLiteral("fan"));

    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("layout"), QStringLiteral("circular"));
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("layoutScale"), 9.0);
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("layoutRows"), -2);
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("iconAnimation"), QStringLiteral("orbit"));
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("animationTrigger"), QStringLiteral("idle"));
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("folderLayout"), QStringLiteral("spiral"));
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("folderSpeed"), 20);
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("appearance"), QStringLiteral("floating-glass"));
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("type"), QStringLiteral("empty"));
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("opacity"), 0.0);

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("layout")).toString(),
             QStringLiteral("circular"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("layoutScale")).toReal(), 2.5);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("layoutRows")).toInt(), 1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("iconAnimation")).toString(),
             QStringLiteral("orbit"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("animationTrigger")).toString(),
             QStringLiteral("idle"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("folderLayout")).toString(),
             QStringLiteral("spiral"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("folderSpeed")).toInt(), 80);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("appearance")).toString(),
             QStringLiteral("floating-glass"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("type")).toString(),
             QStringLiteral("empty"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("opacity")).toReal(), 0.0);

    const QString emptyPanelId = registry.addPanel(QStringLiteral("top"), QStringLiteral("empty"));
    QCOMPARE(registry.panelValue(emptyPanelId, QStringLiteral("type")).toString(),
             QStringLiteral("empty"));
    QVERIFY(!registry.panelValue(emptyPanelId, QStringLiteral("dynamic")).toBool());
    const QString freePanelId = registry.addPanel(QStringLiteral("free"), QStringLiteral("empty"));
    QVERIFY(!freePanelId.isEmpty());
    QCOMPARE(registry.panelValue(freePanelId, QStringLiteral("edge")).toString(),
             QStringLiteral("free"));

    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("layout"), QStringLiteral("polygon"));
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("pathSides"), 99);
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("pathOrientation"), QStringLiteral("invalid"));
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("pathAnchor"), QStringLiteral("bottom-right"));

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("layout")).toString(),
             QStringLiteral("polygon"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("pathSides")).toInt(), 12);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("pathOrientation")).toString(),
             QStringLiteral("upright"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("pathAnchor")).toString(),
             QStringLiteral("bottom-right"));
}

void PanelRegistryTest::separatesVisualChangesFromPanelTopology()
{
    PanelRegistry registry;
    const QString panelId = registry.addFreePanel();
    QSignalSpy topologySpy(&registry, &PanelRegistry::nativePanelTopologyChanged);
    QSignalSpy revisionSpy(&registry, &PanelRegistry::revisionChanged);

    registry.setPanelValue(panelId, QStringLiteral("layout"), QStringLiteral("hexagon"));
    registry.setPanelValue(panelId, QStringLiteral("shape"), QStringLiteral("hexagon"));
    registry.setPanelValue(panelId, QStringLiteral("appearance"), QStringLiteral("futuristic"));

    QCOMPARE(topologySpy.count(), 0);
    QCOMPARE(revisionSpy.count(), 3);

    registry.setPanelValue(panelId, QStringLiteral("screen"), 1);
    QCOMPARE(topologySpy.count(), 0);
    QCOMPARE(revisionSpy.count(), 4);

    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("screen"), 1);
    QCOMPARE(topologySpy.count(), 1);
    QCOMPARE(revisionSpy.count(), 5);
}

void PanelRegistryTest::filtersEntriesByPanelContentType()
{
    WindowModel windowModel;
    DockModel dockModel(windowModel);

    WindowItem window;
    window.internalId = QStringLiteral("window-1");
    window.resourceClass = QStringLiteral("archdock-test");
    window.resourceName = QStringLiteral("archdock-test");
    window.caption = QStringLiteral("Arch Dock test window");
    windowModel.setWindows({window});

    QCOMPARE(dockModel.rowCount(), 1);
    QCOMPARE(dockModel.panelEntryCount(QStringLiteral("empty")), 0);
    QCOMPARE(dockModel.panelEntryCount(QStringLiteral("launcher")), 0);
    QCOMPARE(dockModel.panelEntryCount(QStringLiteral("tasks")), 1);
    QCOMPARE(dockModel.panelEntryCount(QStringLiteral("hybrid")), 1);
    QVERIFY(!dockModel.panelEntryMatches(0, QStringLiteral("empty")));
    QVERIFY(dockModel.panelEntryMatches(0, QStringLiteral("tasks")));
}

void PanelRegistryTest::exposesStablePanelEntrySnapshots()
{
    WindowModel windowModel;
    DockModel dockModel(windowModel);

    WindowItem window;
    window.internalId = QStringLiteral("window-stable-id");
    window.resourceClass = QStringLiteral("archdock-snapshot-test");
    window.resourceName = QStringLiteral("archdock-snapshot-test");
    window.caption = QStringLiteral("Snapshot test window");
    window.iconName = QStringLiteral("applications-system");
    window.active = true;
    windowModel.setWindows({window});

    const QVariantList entries = dockModel.panelEntries(QStringLiteral("tasks"));
    QCOMPARE(entries.size(), 1);
    const QVariantMap entry = entries.constFirst().toMap();
    QCOMPARE(entry.value(QStringLiteral("appId")).toString(), QStringLiteral("archdock-snapshot-test"));
    QCOMPARE(entry.value(QStringLiteral("displayName")).toString(), QStringLiteral("Snapshot test window"));
    QCOMPARE(entry.value(QStringLiteral("iconName")).toString(), QStringLiteral("applications-system"));
    QVERIFY(entry.value(QStringLiteral("running")).toBool());
    QVERIFY(entry.value(QStringLiteral("active")).toBool());
    QCOMPARE(entry.value(QStringLiteral("windowIds")).toStringList(), QStringList({QStringLiteral("window-stable-id")}));
}

void PanelRegistryTest::targetsStableApplicationWindowIds()
{
    WindowModel windowModel;
    DockModel dockModel(windowModel);

    WindowItem firstWindow;
    firstWindow.internalId = QStringLiteral("window-first");
    firstWindow.resourceClass = QStringLiteral("archdock-window-actions");
    firstWindow.resourceName = QStringLiteral("archdock-window-actions");
    firstWindow.caption = QStringLiteral("First window");

    WindowItem secondWindow = firstWindow;
    secondWindow.internalId = QStringLiteral("window-second");
    secondWindow.caption = QStringLiteral("Second window");
    windowModel.setWindows({firstWindow, secondWindow});

    QList<QPair<QString, QString>> actions;
    connect(&dockModel,
            &DockModel::windowActionRequested,
            this,
            [&actions](const QString &windowId, const QString &action)
            {
                actions.append({windowId, action});
            });

    QVERIFY(dockModel.activateApplicationWindow(
        QStringLiteral("archdock-window-actions"), QStringLiteral("window-second")));
    QCOMPARE(actions.size(), 1);
    QCOMPARE(actions.at(0).first, QStringLiteral("window-second"));
    QCOMPARE(actions.at(0).second, QStringLiteral("activate"));

    QVERIFY(dockModel.closeAllApplication(QStringLiteral("archdock-window-actions")));
    QCOMPARE(actions.size(), 3);
    QCOMPARE(actions.at(1).first, QStringLiteral("window-first"));
    QCOMPARE(actions.at(1).second, QStringLiteral("close"));
    QCOMPARE(actions.at(2).first, QStringLiteral("window-second"));
    QCOMPARE(actions.at(2).second, QStringLiteral("close"));
    QVERIFY(!dockModel.activateApplicationWindow(
        QStringLiteral("archdock-window-actions"), QStringLiteral("missing-window")));
}

void PanelRegistryTest::supportsPinnedFolderSnapshotsAndReordering()
{
    QTemporaryDir folderRoot;
    QVERIFY(folderRoot.isValid());
    QVERIFY(QDir().mkpath(folderRoot.filePath(QStringLiteral("first/nested"))));
    QFile firstFile(folderRoot.filePath(QStringLiteral("first/readme.txt")));
    QVERIFY(firstFile.open(QIODevice::WriteOnly));
    firstFile.write("Arch Dock folder test\n");
    firstFile.close();
    QVERIFY(QDir().mkpath(folderRoot.filePath(QStringLiteral("second"))));
    QVERIFY(QDir().mkpath(folderRoot.filePath(QStringLiteral("third"))));

    WindowModel windowModel;
    DockModel dockModel(windowModel);
    const QUrl firstUrl = QUrl::fromLocalFile(folderRoot.filePath(QStringLiteral("first")));
    const QUrl secondUrl = QUrl::fromLocalFile(folderRoot.filePath(QStringLiteral("second")));
    const QUrl thirdUrl = QUrl::fromLocalFile(folderRoot.filePath(QStringLiteral("third")));
    QVERIFY(dockModel.pinUrl(firstUrl));
    QVERIFY(dockModel.pinUrl(secondUrl));
    QVERIFY(dockModel.pinUrl(thirdUrl));

    const QString firstId = QStringLiteral("file:") + QFileInfo(firstUrl.toLocalFile()).canonicalFilePath();
    const QString secondId = QStringLiteral("file:") + QFileInfo(secondUrl.toLocalFile()).canonicalFilePath();
    const QString thirdId = QStringLiteral("file:") + QFileInfo(thirdUrl.toLocalFile()).canonicalFilePath();
    QVariantList entries = dockModel.panelEntries(QStringLiteral("launcher"));
    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("appId")).toString(), firstId);
    QVERIFY(entries.at(0).toMap().value(QStringLiteral("isFolder")).toBool());

    const QVariantList folderEntries = dockModel.folderEntriesForApplication(firstId);
    QCOMPARE(folderEntries.size(), 2);
    QCOMPARE(folderEntries.at(0).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("nested"));
    QCOMPARE(folderEntries.at(1).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("readme.txt"));

    QVERIFY(dockModel.moveApplicationBefore(firstId, thirdId));
    entries = dockModel.panelEntries(QStringLiteral("launcher"));
    QCOMPARE(entries.at(0).toMap().value(QStringLiteral("appId")).toString(), secondId);
    QCOMPARE(entries.at(1).toMap().value(QStringLiteral("appId")).toString(), firstId);
    QCOMPARE(entries.at(2).toMap().value(QStringLiteral("appId")).toString(), thirdId);
}

void PanelRegistryTest::validatesBuiltInCapabilityCatalog()
{
    const QVariantList definitions = taskThemeDefinitions();
    QCOMPARE(definitions.size(), 12);
    PanelRegistry registry(definitions);
    QCOMPARE(registry.themeDefinitions().size(), definitions.size());

    int packagedThemeCount = 0;
    for (const QVariant &candidate : registry.themeDefinitions())
    {
        const QVariantMap theme = candidate.toMap();
        QString errorCode;
        const std::optional<ArchDock::ThemeCapabilityProfile> profile =
            ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(
                theme, &errorCode);
        QVERIFY2(profile.has_value(), qPrintable(errorCode));
        QVERIFY(profile->rendererTiers.contains(
            ArchDock::RendererTier::Procedural2D));
        QVERIFY(!profile->rendererTiers.contains(
            ArchDock::RendererTier::Baked2_5D));
        QVERIFY(!profile->rendererTiers.contains(
            ArchDock::RendererTier::True3D));
        QVERIFY(!profile->capabilities.contains(
            ArchDock::PanelCapability::NonRectangularInput));
        if (theme.contains(QStringLiteral("packageManifest")))
        {
            ++packagedThemeCount;
            QVERIFY(profile->rendererTiers.contains(
                ArchDock::RendererTier::Skinned2D));
            QCOMPARE(profile->preferredRendererTier,
                     std::optional<ArchDock::RendererTier>(
                         ArchDock::RendererTier::Skinned2D));
            QCOMPARE(profile->fallbackRendererTiers,
                     QVector<ArchDock::RendererTier>{
                         ArchDock::RendererTier::Procedural2D});
            QCOMPARE(profile->layouts,
                     QVector<ArchDock::PanelLayoutKind>{
                         ArchDock::PanelLayoutKind::Horizontal});
            QCOMPARE(profile->presentationMechanisms.size(), 3);
            const QVariantMap preview = theme.value(
                QStringLiteral("previewConfiguration")).toMap();
            QCOMPARE(preview.value(QStringLiteral("mode")).toString(),
                     QStringLiteral("horizontal"));
            QCOMPARE(preview.value(
                         QStringLiteral("presentationState")).toString(),
                     QStringLiteral("open"));
            QVERIFY(!theme.value(
                QStringLiteral("iconStyleRef")).toMap().isEmpty());
        }
        else
        {
            QVERIFY(!profile->rendererTiers.contains(
                ArchDock::RendererTier::Skinned2D));
            QVERIFY(profile->presentationMechanisms.isEmpty());
        }
    }
    QCOMPARE(packagedThemeCount, 7);

    const QVariantMap ringTheme = registry.themeDefinitions().at(3).toMap();
    QCOMPARE(ringTheme.value(QStringLiteral("id")).toString(),
             QStringLiteral("holographic-ring"));
    const auto ringProfile =
        ArchDock::PanelCapabilityResolver::themeProfileFromVariantMap(ringTheme);
    QVERIFY(ringProfile.has_value());
    QCOMPARE(ringProfile->hostKinds,
             QVector<ArchDock::PanelHostKind>{ArchDock::PanelHostKind::FreeDesktop});
    QCOMPARE(ringProfile->layouts,
             QVector<ArchDock::PanelLayoutKind>{ArchDock::PanelLayoutKind::Ring});
}

void PanelRegistryTest::resolvesBuiltInChassisPackagesAndImportPrecedence()
{
    PanelRegistry registry(taskThemeDefinitions());
    const QString panelId = QStringLiteral("bottom");

    const QVariantMap candidate = registry.themeCandidate(
        panelId,
        QStringLiteral("sci-fi-chassis-dark"),
        QStringLiteral("complete"));
    QVERIFY(candidate.value(QStringLiteral("success")).toBool());
    const QVariantMap values = candidate.value(QStringLiteral("values")).toMap();
    QCOMPARE(values.value(QStringLiteral("layout")).toString(),
             QStringLiteral("horizontal"));
    QCOMPARE(values.value(QStringLiteral("rendererTier")).toString(),
             QStringLiteral("skinned2d"));
    QCOMPARE(values.value(QStringLiteral("completeThemeId")).toString(),
             QStringLiteral("sci-fi-chassis-dark"));
    QCOMPARE(candidate.value(QStringLiteral("capabilityResolution"))
                 .toMap()
                 .value(QStringLiteral("renderer"))
                 .toMap()
                 .value(QStringLiteral("effectiveTier"))
                 .toString(),
             QStringLiteral("skinned2d"));

    QVERIFY(registry.applyTheme(
        panelId,
        QStringLiteral("sci-fi-chassis-dark"),
        QStringLiteral("complete")));
    const auto builtInDefinition = registry.panelDefinition(panelId);
    QVERIFY(builtInDefinition.has_value());
    QString projectionError;
    const std::optional<QVariantMap> builtInProjection =
        registry.themeRuntimeProjection(*builtInDefinition, &projectionError);
    QVERIFY2(builtInProjection.has_value(), qPrintable(projectionError));
    QCOMPARE(builtInProjection->value(QStringLiteral("id")).toString(),
             QStringLiteral("sci-fi-chassis-dark"));
    QVERIFY(builtInProjection->value(QStringLiteral("valid")).toBool());
    const QVariantMap assetPaths = builtInProjection->value(
        QStringLiteral("assetPaths")).toMap();
    QVERIFY(QFileInfo(assetPaths.value(
        QStringLiteral("surface")).toString()).isAbsolute());
    QVERIFY(QFileInfo(assetPaths.value(
        QStringLiteral("glow")).toString()).isFile());
    QVERIFY(QFileInfo(assetPaths.value(
        QStringLiteral("input-mask")).toString()).isFile());

    const QString importedManifest = QFINDTESTDATA(
        QStringLiteral("fixtures/theme-v2/valid-skinned2d-states.json"));
    QVERIFY(!importedManifest.isEmpty());
    QVERIFY(registry.importTheme(
        panelId, QUrl::fromLocalFile(importedManifest)));
    QCOMPARE(registry.panelValue(
                 panelId, QStringLiteral("panelThemeId")).toString(),
             QString{});
    QCOMPARE(registry.panelValue(
                 panelId, QStringLiteral("completeThemeId")).toString(),
             QString{});
    const auto importedDefinition = registry.panelDefinition(panelId);
    QVERIFY(importedDefinition.has_value());
    const auto importedProjection = registry.themeRuntimeProjection(
        *importedDefinition, &projectionError);
    QVERIFY2(importedProjection.has_value(), qPrintable(projectionError));
    QCOMPARE(importedProjection->value(QStringLiteral("id")).toString(),
             QStringLiteral("fixture-split-skin"));

    QVERIFY(registry.applyTheme(
        panelId,
        QStringLiteral("sci-fi-chassis-blue"),
        QStringLiteral("complete")));
    const auto selectedDefinition = registry.panelDefinition(panelId);
    QVERIFY(selectedDefinition.has_value());
    const auto selectedProjection = registry.themeRuntimeProjection(
        *selectedDefinition, &projectionError);
    QVERIFY2(selectedProjection.has_value(), qPrintable(projectionError));
    QCOMPARE(selectedProjection->value(QStringLiteral("id")).toString(),
             QStringLiteral("sci-fi-chassis-blue"));
}

void PanelRegistryTest::resolvesIconStylesIndependentlyFromPanelThemes()
{
    PanelRegistry registry(taskThemeDefinitions());
    const QVariantList styles = registry.iconStyleDefinitions();
    QCOMPARE(styles.size(), 6);
    QCOMPARE(styles.constFirst().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("plain-original"));
    QCOMPARE(styles.constLast().toMap().value(QStringLiteral("id")).toString(),
             QStringLiteral("dark-orb"));

    const auto initial = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(initial.has_value());
    QCOMPARE(initial->iconStyle.styleReference,
             QStringLiteral("plain-original"));
    QString projectionError;
    const auto projection = registry.iconStyleRuntimeProjection(
        *initial, &projectionError);
    QVERIFY2(projection.has_value(), qPrintable(projectionError));
    QCOMPARE(projection->value(QStringLiteral("id")).toString(),
             QStringLiteral("plain-original"));
    QCOMPARE(projection->value(QStringLiteral("selectionStatus")).toString(),
             QStringLiteral("selected"));

    const QVariantMap unknown = registry.iconStyleDefinition(
        QStringLiteral("not-installed"));
    QVERIFY(unknown.value(QStringLiteral("valid")).toBool());
    QVERIFY(unknown.value(QStringLiteral("fellBack")).toBool());
    QCOMPARE(unknown.value(QStringLiteral("resolvedStyleId")).toString(),
             QStringLiteral("plain-original"));

    registry.setPanelValue(QStringLiteral("bottom"),
                           QStringLiteral("iconStyle"),
                           QStringLiteral("metallic-blue"));
    const auto styled = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(styled.has_value());
    QCOMPARE(styled->iconStyle.styleReference,
             QStringLiteral("metallic-blue"));
    const auto styledProjection = registry.iconStyleRuntimeProjection(
        *styled, &projectionError);
    QVERIFY2(styledProjection.has_value(), qPrintable(projectionError));
    QCOMPARE(styledProjection->value(QStringLiteral("id")).toString(),
             QStringLiteral("metallic-blue"));
    QCOMPARE(styledProjection->value(QStringLiteral("glyphPolicy")).toMap()
                 .value(QStringLiteral("mode")).toString(),
             QStringLiteral("original"));

    const QVariantMap candidate = registry.themeCandidate(
        QStringLiteral("bottom"),
        QStringLiteral("sci-fi-chassis-dark"),
        QStringLiteral("complete"));
    QVERIFY(candidate.value(QStringLiteral("success")).toBool());
    QCOMPARE(candidate.value(QStringLiteral("recommendedIconStyleId")).toString(),
             QStringLiteral("dark-orb"));
    const QVariantMap values = candidate.value(QStringLiteral("values")).toMap();
    QVERIFY(!values.contains(QStringLiteral("iconStyle")));
    QVERIFY(!values.contains(QStringLiteral("iconThemeId")));

    QVERIFY(registry.applyTheme(
        QStringLiteral("bottom"),
        QStringLiteral("sci-fi-chassis-dark"),
        QStringLiteral("complete")));
    const auto themed = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(themed.has_value());
    QCOMPARE(themed->iconStyle.styleReference,
             QStringLiteral("metallic-blue"));
    QCOMPARE(themed->iconStyle.themeId,
             QStringLiteral("metallic-blue"));
}

void PanelRegistryTest::resolvesThemeCandidatesWithoutMutation()
{
    PanelRegistry registry(taskThemeDefinitions());
    const QVariantMap before = registry.panelSnapshot(QStringLiteral("bottom"));
    const int revisionBefore = registry.revision();

    const QVariantMap candidate = registry.themeCandidate(
        QStringLiteral("bottom"),
        QStringLiteral("obsidian-glass"),
        QStringLiteral("complete"));

    QVERIFY(candidate.value(QStringLiteral("success")).toBool());
    QCOMPARE(candidate.value(QStringLiteral("status")).toString(),
             QStringLiteral("resolved"));
    const QVariantMap values = candidate.value(QStringLiteral("values")).toMap();
    QCOMPARE(values.value(QStringLiteral("completeThemeId")).toString(),
             QStringLiteral("obsidian-glass"));
    QCOMPARE(values.value(QStringLiteral("appearance")).toString(),
             QStringLiteral("glass"));
    QCOMPARE(values.value(QStringLiteral("opacity")).toReal(), 0.88);
    QVERIFY(!values.contains(QStringLiteral("id")));
    QVERIFY(!values.contains(QStringLiteral("builtIn")));
    QVERIFY(!values.contains(QStringLiteral("screenId")));
    QVERIFY(!values.contains(QStringLiteral("surface3D")));
    QCOMPARE(registry.panelSnapshot(QStringLiteral("bottom")), before);
    QCOMPARE(registry.revision(), revisionBefore);

    const QVariantMap unavailable = registry.themeCandidate(
        QStringLiteral("bottom"),
        QStringLiteral("holographic-ring"),
        QStringLiteral("complete"));
    QVERIFY(!unavailable.value(QStringLiteral("success")).toBool());
    QCOMPARE(unavailable.value(QStringLiteral("status")).toString(),
             QStringLiteral("capability-unavailable"));
    QVERIFY(unavailable.value(QStringLiteral("values")).toMap().isEmpty());
    QCOMPARE(registry.panelSnapshot(QStringLiteral("bottom")), before);
    QCOMPARE(registry.revision(), revisionBefore);
}

void PanelRegistryTest::rejectsIncompatibleThemeWithoutRecordMutation()
{
    const QVariantList definitions = taskThemeDefinitions();
    QCOMPARE(definitions.size(), 12);
    PanelRegistry registry(definitions);
    const QVariantMap before = registry.panelSnapshot(QStringLiteral("bottom"));
    const int registryRevisionBefore = registry.revision();
    const auto definitionBefore = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(definitionBefore.has_value());

    QVERIFY(!registry.applyTheme(
        QStringLiteral("bottom"),
        QStringLiteral("holographic-ring"),
        QStringLiteral("complete")));

    QCOMPARE(registry.panelSnapshot(QStringLiteral("bottom")), before);
    QCOMPARE(registry.revision(), registryRevisionBefore);
    QCOMPARE(registry.panelDefinition(QStringLiteral("bottom")), definitionBefore);

    const QString freePanelId = registry.addFreePanel();
    QVERIFY(!freePanelId.isEmpty());
    QVERIFY(registry.applyTheme(
        freePanelId,
        QStringLiteral("holographic-ring"),
        QStringLiteral("complete")));
    QCOMPARE(registry.panelValue(freePanelId, QStringLiteral("layout")).toString(),
             QStringLiteral("ring"));
}

void PanelRegistryTest::mapsVersionOneArtworkToProceduralFallback()
{
    PanelRegistry registry(taskThemeDefinitions());
    ArchDock::PanelDefinition definition = ArchDock::PanelDefinition::defaults(
        QStringLiteral("legacy-artwork"),
        QStringLiteral("Legacy Artwork"),
        QStringLiteral("bottom"),
        false);
    definition.surface.themeSource = QStringLiteral("file:///tmp/example.blend");
    definition.surface.themeSourceFormat = QStringLiteral("blend");
    definition.surface.themePackageFormat = QStringLiteral("org.archdock.theme");
    definition.surface.themePackageVersion = 1;
    definition.surface.themePackageId = QStringLiteral("legacy-scene");

    const auto profile = registry.themeCapabilityProfile(definition);
    QVERIFY(profile.has_value());
    QCOMPARE(profile->preferredRendererTier,
             std::optional<ArchDock::RendererTier>(
                 ArchDock::RendererTier::Procedural2D));
    QVERIFY(profile->fallbackRendererTiers.isEmpty());
    QVERIFY(!profile->rendererTiers.contains(ArchDock::RendererTier::Skinned2D));
    QVERIFY(profile->rendererTiers.contains(ArchDock::RendererTier::Procedural2D));
    QVERIFY(!profile->rendererTiers.contains(ArchDock::RendererTier::Baked2_5D));
    QVERIFY(!profile->rendererTiers.contains(ArchDock::RendererTier::True3D));

    const ArchDock::CapabilityResolution resolution =
        registry.resolvePanelCapabilities(definition);
    QVERIFY(resolution.available);
    QVERIFY(!resolution.renderer.fallbackApplied);
    QCOMPARE(resolution.renderer.effectiveTier,
             std::optional<ArchDock::RendererTier>(
                 ArchDock::RendererTier::Procedural2D));
    QCOMPARE(resolution.renderer.evaluatedTiers.constFirst().reason,
             ArchDock::CapabilityReasonCode::None);
}

void PanelRegistryTest::importsVersionedThemePackage()
{
    QTemporaryDir packageDirectory;
    QVERIFY(packageDirectory.isValid());

    const QString artworkPath = packageDirectory.filePath(QStringLiteral("surface.png"));
    QImage artwork(24, 16, QImage::Format_ARGB32_Premultiplied);
    artwork.fill(Qt::green);
    QVERIFY(artwork.save(artworkPath));

    const QString manifestPath = packageDirectory.filePath(QStringLiteral("archdock-theme.json"));
    QFile manifestFile(manifestPath);
    QVERIFY(manifestFile.open(QIODevice::WriteOnly));
    manifestFile.write(QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("id"), QStringLiteral("test-surface")},
        {QStringLiteral("name"), QStringLiteral("Test surface")},
           {QStringLiteral("surface"), QJsonObject{
               {QStringLiteral("asset"), QStringLiteral("surface.png")},
               {QStringLiteral("fit"), QStringLiteral("contain")}}}}).toJson(QJsonDocument::Compact));
    manifestFile.close();

    PanelRegistry registry;
    QVERIFY(registry.importTheme(QStringLiteral("bottom"), QUrl::fromLocalFile(manifestPath)));

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageFormat")).toString(),
             QStringLiteral("org.archdock.theme"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageVersion")).toInt(), 1);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageId")).toString(),
             QStringLiteral("test-surface"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageName")).toString(),
             QStringLiteral("Test surface"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeFit")).toString(),
             QStringLiteral("contain"));

    const QUrl copiedSource(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSource")).toString());
    const QUrl managedManifest(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageManifest")).toString());
    QVERIFY(QFileInfo::exists(copiedSource.toLocalFile()));
    QVERIFY(QFileInfo::exists(managedManifest.toLocalFile()));
    QVERIFY(copiedSource.toLocalFile() != artworkPath);

    QTRY_COMPARE_WITH_TIMEOUT(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome")).toString(),
        QStringLiteral("ready"),
        10000);
    const QUrl renderedAsset(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeAsset")).toString());
    QVERIFY(QFileInfo::exists(renderedAsset.toLocalFile()));

    const QString activeSource = registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSource")).toString();
    const QString activeAsset = registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeAsset")).toString();
    const QString activePackageId = registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageId")).toString();
    const auto writeManifest = [&packageDirectory](const QString &fileName, const QJsonObject &contents)
    {
        QFile file(packageDirectory.filePath(fileName));
        if (!file.open(QIODevice::WriteOnly))
        {
            return QString{};
        }
        file.write(QJsonDocument(contents).toJson(QJsonDocument::Compact));
        return file.fileName();
    };

    const QString unsupportedPath = writeManifest(
        QStringLiteral("future-theme.json"),
        {{QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
         {QStringLiteral("version"), 2},
         {QStringLiteral("surface"), QJsonObject{{QStringLiteral("asset"), QStringLiteral("surface.png")}}}});
    QVERIFY(!unsupportedPath.isEmpty());
    QVERIFY(!registry.importTheme(QStringLiteral("bottom"), QUrl::fromLocalFile(unsupportedPath)));
    QVERIFY(registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeStatus"))
                .toString()
                .contains(QStringLiteral("unknown-field")));

    const QString traversalPath = writeManifest(
        QStringLiteral("traversal-theme.json"),
        {{QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
         {QStringLiteral("version"), 1},
         {QStringLiteral("surface"), QJsonObject{{QStringLiteral("asset"), QStringLiteral("../surface.png")}}}});
    QVERIFY(!traversalPath.isEmpty());
    QVERIFY(!registry.importTheme(QStringLiteral("bottom"), QUrl::fromLocalFile(traversalPath)));
    QVERIFY(registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeStatus"))
                .toString()
                .contains(QStringLiteral("unsafe-path")));

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSource")).toString(), activeSource);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeAsset")).toString(), activeAsset);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageId")).toString(), activePackageId);
}

void PanelRegistryTest::importsVersionTwoThemePackageWithSafeFallback()
{
    const QString manifestPath = QFINDTESTDATA(
        QStringLiteral("fixtures/theme-v2/valid-procedural.json"));
    QVERIFY(!manifestPath.isEmpty());
    QFile originalFile(manifestPath);
    QVERIFY(originalFile.open(QIODevice::ReadOnly));
    const QByteArray originalBytes = originalFile.readAll();

    PanelRegistry registry;
    QVERIFY(registry.importTheme(
        QStringLiteral("bottom"), QUrl::fromLocalFile(manifestPath)));

    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themePackageVersion")).toInt(),
             2);
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themePackageId")).toString(),
             QStringLiteral("fixture-procedural"));
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themePackageName")).toString(),
             QStringLiteral("Fixture Procedural"));
    QVERIFY(registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeSource")).toString().isEmpty());
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome")).toString(),
             QStringLiteral("fallback"));
    const QUrl managedManifest(registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themePackageManifest")).toString());
    QVERIFY(managedManifest.isLocalFile());
    QVERIFY(QFileInfo(managedManifest.toLocalFile()).isFile());
    QVERIFY(managedManifest.toLocalFile() != manifestPath);

    QFile originalAfter(manifestPath);
    QVERIFY(originalAfter.open(QIODevice::ReadOnly));
    QCOMPARE(originalAfter.readAll(), originalBytes);

    PanelRegistry reloaded;
    QCOMPARE(reloaded.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themePackageVersion")).toInt(),
             2);
    QCOMPARE(reloaded.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themePackageId")).toString(),
             QStringLiteral("fixture-procedural"));

    QFile corruptManagedManifest(managedManifest.toLocalFile());
    QVERIFY(corruptManagedManifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(corruptManagedManifest.write(QByteArrayLiteral("not-json")), qint64{8});
    corruptManagedManifest.close();
    PanelRegistry fallbackReload;
    QCOMPARE(fallbackReload.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themePackageVersion")).toInt(),
             0);
    QVERIFY(fallbackReload.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeSource")).toString().isEmpty());
    QCOMPARE(fallbackReload.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome")).toString(),
             QStringLiteral("fallback"));
    QVERIFY(fallbackReload.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeStatus"))
                .toString()
                .contains(QStringLiteral("invalid-json")));
}

void PanelRegistryTest::usesManagedVersionTwoCapabilitiesAndRendererFallback()
{
    const QString manifestPath = QFINDTESTDATA(
        QStringLiteral("fixtures/theme-v2/valid-skinned2d-states.json"));
    QVERIFY(!manifestPath.isEmpty());

    PanelRegistry registry;
    QVERIFY(registry.importTheme(
        QStringLiteral("bottom"), QUrl::fromLocalFile(manifestPath)));
    const auto definition = registry.panelDefinition(QStringLiteral("bottom"));
    QVERIFY(definition.has_value());
    QString errorCode;
    const auto runtimeProjection = registry.themeRuntimeProjection(
        *definition, &errorCode);
    QVERIFY2(runtimeProjection.has_value(), qPrintable(errorCode));
    QCOMPARE(runtimeProjection->value(QStringLiteral("format")).toString(),
             QStringLiteral("org.archdock.theme"));
    QCOMPARE(runtimeProjection->value(QStringLiteral("version")).toInt(), 2);
    QCOMPARE(runtimeProjection->value(QStringLiteral("id")).toString(),
             QStringLiteral("fixture-split-skin"));
    QVERIFY(runtimeProjection->value(QStringLiteral("valid")).toBool());
    QCOMPARE(runtimeProjection->value(QStringLiteral("manifestPath")).toString(),
             QUrl(definition->surface.themePackageManifest).toLocalFile());
    const QVariantMap assetPaths = runtimeProjection->value(
        QStringLiteral("assetPaths")).toMap();
    QVERIFY(QFileInfo(assetPaths.value(QStringLiteral("surface")).toString())
                .isAbsolute());
    QVERIFY(QFileInfo(assetPaths.value(QStringLiteral("surface")).toString())
                .isFile());
    QVERIFY(QFileInfo(assetPaths.value(QStringLiteral("input-mask")).toString())
                .isFile());

    ArchDock::PanelDefinition mismatchedDefinition = *definition;
    mismatchedDefinition.surface.themePackageId =
        QStringLiteral("forged-package-id");
    QVERIFY(!registry.themeRuntimeProjection(
        mismatchedDefinition, &errorCode).has_value());
    QCOMPARE(errorCode, QStringLiteral("theme-package-identity-mismatch"));
    errorCode.clear();

    const auto profile = registry.themeCapabilityProfile(
        *definition, &errorCode);
    QVERIFY2(profile.has_value(), qPrintable(errorCode));
    QCOMPARE(profile->id, QStringLiteral("fixture-split-skin"));
    const QVector<ArchDock::PanelHostKind> expectedHosts{
        ArchDock::PanelHostKind::NativeEdge,
        ArchDock::PanelHostKind::FreeDesktop,
    };
    QCOMPARE(profile->hostKinds, expectedHosts);
    QCOMPARE(profile->preferredRendererTier,
             std::optional<ArchDock::RendererTier>(
                 ArchDock::RendererTier::Skinned2D));
    QCOMPARE(profile->fallbackRendererTiers,
             QVector<ArchDock::RendererTier>{
                 ArchDock::RendererTier::Procedural2D});
    QVERIFY(profile->layouts.contains(ArchDock::PanelLayoutKind::Adaptive));
    QVERIFY(profile->layouts.contains(ArchDock::PanelLayoutKind::Horizontal));
    QVERIFY(profile->layouts.contains(ArchDock::PanelLayoutKind::Vertical));
    QVERIFY(!profile->layouts.contains(ArchDock::PanelLayoutKind::Ring));
    QVERIFY(profile->presentationMechanisms.contains(
        ArchDock::PanelPresentationMechanism::Split));
    QCOMPARE(profile->rotation.support, ArchDock::RotationSupport::Bounded);
    QCOMPARE(profile->rotation.minimumDegrees, -90.0);
    QCOMPARE(profile->rotation.maximumDegrees, 90.0);

    const ArchDock::CapabilityResolution resolution =
        registry.resolvePanelCapabilities(*definition, &errorCode);
    QVERIFY2(resolution.available, qPrintable(errorCode));
    QCOMPARE(resolution.themeId, QStringLiteral("fixture-split-skin"));
    QVERIFY(!resolution.renderer.fallbackApplied);
    QCOMPARE(resolution.renderer.effectiveTier,
             std::optional<ArchDock::RendererTier>(
                 ArchDock::RendererTier::Skinned2D));
    QCOMPARE(resolution.renderer.reason,
             ArchDock::CapabilityReasonCode::None);
}

void PanelRegistryTest::analyzesAdaptive2DThemeArtwork()
{
    QTemporaryDir sourceDirectory;
    QVERIFY(sourceDirectory.isValid());

    const QString sourcePath = sourceDirectory.filePath(QStringLiteral("adaptive.png"));
    QImage source(80, 40, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::transparent);
    QPainter painter(&source);
    painter.fillRect(12, 8, 56, 24, Qt::cyan);
    painter.end();
    QVERIFY(source.save(sourcePath));

    PanelRegistry registry;
    QVERIFY(registry.importTheme(QStringLiteral("bottom"), QUrl::fromLocalFile(sourcePath)));

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceKind")).toString(),
             QStringLiteral("raster"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceFormat")).toString(),
             QStringLiteral("png"));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceWidth")).toInt(), 80);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceHeight")).toInt(), 40);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceHasAlpha")).toBool(), true);
    QVERIFY(registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeAnalysisStatus"))
                .toString()
                .contains(QStringLiteral("transparent pixels detected")));
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSuggestedFit")).toString(),
             QStringLiteral("contain"));

    const QUrl preview(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePreview")).toString());
    QVERIFY(QFileInfo::exists(preview.toLocalFile()));
    const QImage previewImage(preview.toLocalFile());
    QVERIFY(!previewImage.isNull());
    QCOMPARE(previewImage.size(), QSize(80, 40));

    QTRY_COMPARE_WITH_TIMEOUT(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome")).toString(),
        QStringLiteral("ready"),
        10000);
}

void PanelRegistryTest::retainsSceneSourcesWithoutExternalConversion()
{
    QTemporaryDir sourceDirectory;
    QVERIFY(sourceDirectory.isValid());

    const QString sourcePath = sourceDirectory.filePath(QStringLiteral("scene.blend"));
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    source.write("Not a Blender scene");
    source.close();

    PanelRegistry registry;
    QVERIFY(registry.importTheme(QStringLiteral("bottom"), QUrl::fromLocalFile(sourcePath)));

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSourceKind")).toString(),
             QStringLiteral("scene"));
    QVERIFY(registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeConversionTool"))
                .toString()
                .isEmpty());
    QVERIFY(!registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeConversionAvailable"))
                 .toBool());
    QVERIFY(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePreview")).toString().isEmpty());
    QVERIFY(registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeAnalysisStatus"))
                .toString()
                .contains(QStringLiteral("automatic external conversion is disabled")));

    QTRY_COMPARE_WITH_TIMEOUT(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome")).toString(),
        QStringLiteral("error"),
        10000);
    QVERIFY(registry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("themeStatus"))
                .toString()
                .contains(QStringLiteral("does not execute external scene converters")));
}

void PanelRegistryTest::rendersResponsivePanelSkins()
{
    QTemporaryDir sourceDirectory;
    QVERIFY(sourceDirectory.isValid());

    const QString sourcePath = sourceDirectory.filePath(QStringLiteral("design.png"));
    QImage source(20, 20, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::blue);
    QPainter sourcePainter(&source);
    sourcePainter.fillRect(0, 0, 10, 10, Qt::red);
    sourcePainter.end();
    QVERIFY(source.save(sourcePath));

    PanelRegistry registry;
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("width"), 320);
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("height"), 80);
    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("themeFit"), QStringLiteral("contain"));
    QVERIFY(registry.importTheme(QStringLiteral("bottom"), QUrl::fromLocalFile(sourcePath)));
    QVERIFY(registry.renderTheme(QStringLiteral("bottom"), 320, 80, 1.5, true));

    QTRY_COMPARE_WITH_TIMEOUT(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeRenderWidth")).toInt(),
        480,
        10000);
    QTRY_COMPARE_WITH_TIMEOUT(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome")).toString(),
        QStringLiteral("ready"),
        10000);

    const QUrl copiedSource(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSource")).toString());
    const QUrl containAsset(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeAsset")).toString());
    QVERIFY(QFileInfo::exists(copiedSource.toLocalFile()));
    QVERIFY(QFileInfo::exists(containAsset.toLocalFile()));
    QVERIFY(copiedSource.toLocalFile() != sourcePath);
    QVERIFY(containAsset.toLocalFile() != copiedSource.toLocalFile());
    QVERIFY(containAsset.toLocalFile().contains(
        QStringLiteral("/processed-renders/")));
    QVERIFY(QFileInfo(QFileInfo(containAsset.toLocalFile()).absolutePath() +
                      QStringLiteral("/processing.json"))
                .isFile());

    const QImage containImage(containAsset.toLocalFile());
    QVERIFY(!containImage.isNull());
    QCOMPARE(containImage.size(), QSize(480, 120));
    QCOMPARE(containImage.pixelColor(0, 0).alpha(), 0);
    QVERIFY(containImage.pixelColor(240, 60).alpha() > 0);

    registry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("themeFit"), QStringLiteral("tile"));
    QVERIFY(registry.renderTheme(QStringLiteral("bottom"), 320, 80, 1.0, true));
    QTRY_COMPARE_WITH_TIMEOUT(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeRenderWidth")).toInt(),
        320,
        10000);
    QTRY_COMPARE_WITH_TIMEOUT(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome")).toString(),
        QStringLiteral("ready"),
        10000);

    const QUrl tileAsset(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeAsset")).toString());
    const QImage tileImage(tileAsset.toLocalFile());
    QVERIFY(!tileImage.isNull());
    QCOMPARE(tileImage.size(), QSize(320, 80));
    QVERIFY(tileImage.pixelColor(3, 3).red() > tileImage.pixelColor(3, 3).blue());
    QVERIFY(tileImage.pixelColor(43, 3).red() > tileImage.pixelColor(43, 3).blue());
}

void PanelRegistryTest::doesNotExecuteExternalRenderers()
{
    QTemporaryDir rendererDirectory;
    QVERIFY(rendererDirectory.isValid());

    const QString markerPath = rendererDirectory.filePath(
        QStringLiteral("external-renderer-invoked"));
    for (const QString &name : {QStringLiteral("magick"), QStringLiteral("blender")})
    {
        QFile renderer(rendererDirectory.filePath(name));
        QVERIFY(renderer.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(
            renderer.write(
                "#!/bin/sh\nprintf invoked > \"$ARCHDOCK_RENDER_MARKER\"\nexit 99\n"),
            qint64(61));
        renderer.close();
        QVERIFY(renderer.setPermissions(
            QFileDevice::ReadOwner | QFileDevice::WriteOwner |
            QFileDevice::ExeOwner | QFileDevice::ReadGroup |
            QFileDevice::ExeGroup | QFileDevice::ReadOther |
            QFileDevice::ExeOther));
    }

    const QByteArray originalPath = qgetenv("PATH");
    const ScopedEnvironmentVariable rendererMarker(
        QByteArrayLiteral("ARCHDOCK_RENDER_MARKER"), markerPath.toUtf8());
    const ScopedEnvironmentVariable rendererPath(
        QByteArrayLiteral("PATH"), rendererDirectory.path().toUtf8() + ':' + originalPath);

    const QString sourcePath = rendererDirectory.filePath(QStringLiteral("design.png"));
    QImage source(32, 16, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::darkCyan);
    QVERIFY(source.save(sourcePath));

    PanelRegistry registry;
    QVERIFY(registry.importTheme(
        QStringLiteral("bottom"), QUrl::fromLocalFile(sourcePath)));
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome"))
                 .toString(),
             QStringLiteral("ready"));
    QVERIFY(!QFileInfo::exists(markerPath));

    const QString blendPath = rendererDirectory.filePath(
        QStringLiteral("scene.blend"));
    QFile blend(blendPath);
    QVERIFY(blend.open(QIODevice::WriteOnly));
    QCOMPARE(blend.write(QByteArrayLiteral("BLENDER-v300")), qint64(12));
    blend.close();
    QVERIFY(registry.importTheme(
        QStringLiteral("bottom"), QUrl::fromLocalFile(blendPath)));
    QCOMPARE(registry.panelValue(
                 QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome"))
                 .toString(),
             QStringLiteral("error"));
    QVERIFY(!QFileInfo::exists(markerPath));
}

int main(int argc, char *argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Arch Dock Test"));
    QCoreApplication::setApplicationName(QStringLiteral("Panel Registry Theme Renderer"));

    PanelRegistryTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "PanelRegistryTest.moc"
