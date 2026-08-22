#include "PanelRegistry.h"
#include "NativeContainmentLifecycle.h"
#include "panel/FreePanelController.h"
#include "PanelPlacement.h"
#include "ScreenIdentity.h"
#include "PanelVisibility.h"
#include "DockSettings.h"
#include "DockModel.h"
#include "WindowModel.h"

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
#include <cerrno>
#include <csignal>
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

bool processIsRunning(qint64 processId)
{
    errno = 0;
    return processId > 0 && ::kill(static_cast<pid_t>(processId), 0) == 0;
}

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
    std::function<void()> onExactRemoval;
    std::function<void()> onCreate;
    int bridgeScreenCalls = 0;
    int matchingCountCalls = 0;
    int createCalls = 0;
    int adoptCalls = 0;
    int verifyCalls = 0;
    int exactRemovalCalls = 0;
    int identityRemovalCalls = 0;
    int bridgeRemovalCalls = 0;
    ArchDock::FreePanelHost removedHost;
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

void clearPanelRegistrySettings()
{
    QSettings settings;
    settings.clear();
    settings.sync();
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
    void roundTripsFreeHostAssociation();
    void migratesLegacyThemeSource();
    void batchesNormalizedPanelUpdates();
    void persistsNativePanelRecoveryOutcomes();
    void persistsNativePanelRediscoveryOutcomes();
    void reconcilesNativeContainmentLifecycle();
    void classifiesNativeContainmentMatches();
    void selectsNativeContainmentLifecycleIntent();
    void selectsFreePanelCreationIntent();
    void persistsFreePanelCreationTransactionState();
    void rollsBackFreePanelCreationFailures();
    void reportsFreePanelRollbackFailures();
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
    void importsVersionedThemePackage();
    void analyzesAdaptive2DThemeArtwork();
    void reportsOptionalSceneConversionCapability();
    void rendersResponsivePanelSkins();
    void stopsActiveRendererOnDestruction();

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
        {QRect(0, 0, 1920, 1080), 1, true, false, true, false},
        {QRect(0, 1040, 1920, 40), 0, false, false, true, false},
        {QRect(0, 1040, 1920, 40), 0, true, true, true, false}};

    QVERIFY(ArchDock::shouldConcealForWindows(QStringLiteral("dodge"), panelGeometry, 0, windows));
    QVERIFY(ArchDock::shouldConcealForWindows(QStringLiteral("cover"), panelGeometry, 0, windows));
    QVERIFY(!ArchDock::shouldConcealForWindows(QStringLiteral("always"), panelGeometry, 0, windows));
    QVERIFY(!ArchDock::shouldConcealForWindows(QStringLiteral("auto-hide"), panelGeometry, 0, windows));
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

    const QString traversalPath = writeManifest(
        QStringLiteral("traversal-theme.json"),
        {{QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
         {QStringLiteral("version"), 1},
         {QStringLiteral("surface"), QJsonObject{{QStringLiteral("asset"), QStringLiteral("../surface.png")}}}});
    QVERIFY(!traversalPath.isEmpty());
    QVERIFY(!registry.importTheme(QStringLiteral("bottom"), QUrl::fromLocalFile(traversalPath)));

    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeSource")).toString(), activeSource);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeAsset")).toString(), activeAsset);
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePackageId")).toString(), activePackageId);
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

void PanelRegistryTest::reportsOptionalSceneConversionCapability()
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
    QCOMPARE(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeConversionTool")).toString(),
             QStringLiteral("Blender"));
    QCOMPARE(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeConversionAvailable")).toBool(),
        !QStandardPaths::findExecutable(QStringLiteral("blender")).isEmpty());
    QVERIFY(registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themePreview")).toString().isEmpty());

    QTRY_COMPARE_WITH_TIMEOUT(
        registry.panelValue(QStringLiteral("bottom"), QStringLiteral("themeRenderOutcome")).toString(),
        QStringLiteral("error"),
        10000);
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

void PanelRegistryTest::stopsActiveRendererOnDestruction()
{
    QTemporaryDir rendererDirectory;
    QVERIFY(rendererDirectory.isValid());

    const QString pidPath = rendererDirectory.filePath(QStringLiteral("renderer.pid"));
    QFile renderer(rendererDirectory.filePath(QStringLiteral("magick")));
    QVERIFY(renderer.open(QIODevice::WriteOnly | QIODevice::Truncate));
    renderer.write("#!/bin/sh\nprintf '%s\\n' \"$$\" > \"$ARCHDOCK_RENDER_PID_FILE\"\nexec sleep 60\n");
    renderer.close();
    QVERIFY(renderer.setPermissions(
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther | QFileDevice::ExeOther));

    const QByteArray originalPath = qgetenv("PATH");
    const ScopedEnvironmentVariable rendererPidFile(
        QByteArrayLiteral("ARCHDOCK_RENDER_PID_FILE"), pidPath.toUtf8());
    const ScopedEnvironmentVariable rendererPath(
        QByteArrayLiteral("PATH"), rendererDirectory.path().toUtf8() + ':' + originalPath);

    const QString sourcePath = rendererDirectory.filePath(QStringLiteral("design.png"));
    QImage source(32, 16, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::darkCyan);
    QVERIFY(source.save(sourcePath));

    auto registry = std::make_unique<PanelRegistry>();
    QVERIFY(registry->importTheme(QStringLiteral("bottom"), QUrl::fromLocalFile(sourcePath)));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(pidPath), 5000);

    QFile pidFile(pidPath);
    QVERIFY(pidFile.open(QIODevice::ReadOnly));
    bool parsedPid = false;
    const qint64 rendererPid = QString::fromUtf8(pidFile.readAll()).trimmed().toLongLong(&parsedPid);
    QVERIFY(parsedPid);
    QVERIFY(processIsRunning(rendererPid));

    registry.reset();
    QTRY_VERIFY_WITH_TIMEOUT(!processIsRunning(rendererPid), 5000);
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
