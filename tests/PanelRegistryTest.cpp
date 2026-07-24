#include "PanelRegistry.h"
#include "NativeContainmentLifecycle.h"
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
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QtGlobal>

#include <cerrno>
#include <csignal>
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
    void migratesLegacyThemeSource();
    void batchesNormalizedPanelUpdates();
    void reconcilesNativeContainmentLifecycle();
    void resolvesStableScreenIdentityBeforeFallbackIndex();
    void reservesAndOffsetsOnlySameScreenPanels();
    void concealsOnlyForRelevantActiveWindows();
    void persistsReducedMotionPreference();
    void normalizesLayoutAndMotionValues();
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

void PanelRegistryTest::resolvesStableScreenIdentityBeforeFallbackIndex()
{
    const QStringList firstOrder{QStringLiteral("output:DP-1"), QStringLiteral("output:HDMI-A-1")};
    QCOMPARE(ArchDock::resolvedScreenIndex(firstOrder, QStringLiteral("output:HDMI-A-1"), 0), 1);

    const QStringList reordered{QStringLiteral("output:HDMI-A-1"), QStringLiteral("output:DP-1")};
    QCOMPARE(ArchDock::resolvedScreenIndex(reordered, QStringLiteral("output:HDMI-A-1"), 1), 0);
    QCOMPARE(ArchDock::resolvedScreenIndex(reordered, QStringLiteral("output:missing"), 1), 1);
    QCOMPARE(ArchDock::resolvedScreenIndex(reordered, QStringLiteral("output:DP-1"), 9), 1);

    const QStringList duplicated{QStringLiteral("output:DP-1"), QStringLiteral("output:DP-1")};
    QCOMPARE(ArchDock::resolvedScreenIndex(duplicated, QStringLiteral("output:DP-1"), 1), 1);
    QCOMPARE(ArchDock::resolvedScreenIndex({}, QStringLiteral("output:DP-1"), 0), -1);
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
