#include "model/PanelCapabilityResolver.h"
#include "RendererBuildConfig.h"
#include "model/PanelDefinition.h"
#include "model/PanelSettingsSchema.h"
#include "PanelRegistry.h"
#include "panel/PanelWindow.h"
#include "ScreenIdentity.h"
#include "WindowModel.h"

#include <QDir>
#include <QDBusConnection>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJSValue>
#include <QtQuickTest>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QScreen>
#include <QScopeGuard>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QWheelEvent>
#include <QtTest>
#include <unistd.h>

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
    void segmentsUseRevisionedTransactionsAndHostAuthority();
    void contentProvidersUseTransactionsAndVisibility();
    void nativePanelObservationsRequireUniqueFrame();
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
    void freeFolderIconsUseNativeMetadata();
    void studioPageWheelInput();
    void wholePanelRotationFieldsAreGatedByTheResolver();
    void meshSceneEditorIsGatedAndTransactional();
    void rendererSwitchRetainsOnlyUnchangedInactiveFields();
    void groupedWindowsFollowLiveKWinUpdates();
    void desktopLaunchIsBoundToTheSelectedPanelEntry();
    void folderRequestsValidatePanelAndChild();
    void studioPresetPagesBrowseWithoutChangingAnyPanel();
    void profileServicePersistsAndHonorsPanelGuards();
    void presetAuditionGuardsAndInvalidRequestsLeaveNoWrites();
    void presetDefaultsDoNotRewriteExistingInstances();
    void screenSignalsCoalesceAndUtilityWindowFitsWorkArea();
    void studioArtworkPersistenceFailure_data();
    void studioArtworkPersistenceFailure();

private:
    QTemporaryDir m_settingsDirectory;
};

void PanelWindowCapabilityTest::freeFolderIconsUseNativeMetadata()
{
    QQmlApplicationEngine engine;
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    QTemporaryDir files;
    QVERIFY(files.isValid());
    QImage png(32, 32, QImage::Format_ARGB32);
    png.fill(Qt::red);
    const QString pngPath = files.filePath("custom icon.png");
    QVERIFY(png.save(pngPath));
    const QString svgPath = QFileInfo(QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg")).absoluteFilePath();
    const QString panel = registry->addFreePanel();
    const auto outcome = backend.applyPanelSettingsTransaction(panel,
        registry->panelDefinition(panel)->settingsRevision, {{"type", "launcher"}});
    QVERIFY(outcome.value("success").toBool());
    const QStringList icons{"folder-documents", svgPath, pngPath};
    for (int index = 0; index < icons.size(); ++index) {
        const QString folder = files.filePath("Folder " + QString::number(index));
        QVERIFY(QDir().mkpath(folder));
        QFile metadata(folder + "/.directory");
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write(("[Desktop Entry]\nIcon=" + icons[index] + "\n").toUtf8());
        metadata.close();
        QVERIFY(backend.addPanelEntries(panel, {QUrl::fromLocalFile(folder).toString()}));
        const auto rows = backend.dockEntriesForPanel(panel, "launcher");
        const auto entry = rows.last().toMap();
        QCOMPARE(entry.value("iconName").toString(), icons[index]);
        QCOMPARE(entry.value("baseIconName").toString(), icons[index]);
        QCOMPARE(entry.value("resolvedGlyph").toString(), icons[index]);
        QVERIFY(entry.value("isFolder").toBool());
        QCOMPARE(rows.size(), index + 1);
    }
    PanelRegistry reloaded;
    QCOMPARE(reloaded.panelDefinition(panel)->content.urls.size(), icons.size());
}

void PanelWindowCapabilityTest::studioPageWheelInput()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    QStringList warnings;
    connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors) warnings.append(error.toString());
    });
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const QString panel = registry->addFreePanel();
    QVERIFY(backend.applyPanelSettingsTransaction(panel,
        registry->panelDefinition(panel)->settingsRevision, {{"type", "launcher"}})
        .value("success").toBool());
    const auto before = registry->panelDefinition(panel)->toPersistedMap();
    const bool native = qEnvironmentVariable("ARCHDOCK_NATIVE_UI_PROBE") == "1";
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> popup(component.createWithInitialProperties({
        {"selectedPanelId", panel}, {"mainTabIndex", native ? 2 : 1}, {"subTabIndex", native ? 0 : 2},
        {"width", 640}, {"height", 520}}));
    QVERIFY2(popup != nullptr, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(popup.get());
    QVERIFY(window);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(QQuickTest::qWaitForPolish(window));
    const auto item = [&](const QString &name) {
        return popup->findChild<QQuickItem *>(name);
    };
    auto *page = item("studio-page-scroll");
    auto *form = item("studio-page-form");
    auto *tabs = item("studio-page-tabs");
    QVERIFY(page && form && tabs);
    auto *inner = form->property("contentItem").value<QQuickItem *>();
    auto *outer = page->property("contentItem").value<QQuickItem *>();
    auto *tabView = tabs->property("contentItem").value<QQuickItem *>();
    QVERIFY(inner && outer && tabView);
    if (native) {
        const QString root = qEnvironmentVariable("ARCHDOCK_RENDERING_SESSION_ROOT");
        const QFileInfo directory(root);
        QVERIFY(directory.isDir() && directory.ownerId() == getuid());
        QVERIFY(directory.absolutePath() == "/tmp" && directory.fileName().startsWith("archdock-rendering-import."));
        QCOMPARE(qEnvironmentVariable("XDG_RUNTIME_DIR"), root + "/runtime");
        window->setTitle("Arch Dock UI input probe");
        window->contentItem()->forceActiveFocus();
        inner->setProperty("contentY", 0);
        outer->setProperty("contentY", 0);
        int sample = 0;
        QElapsedTimer deadline;
        deadline.start();
        while (!QFileInfo::exists(root + "/ui-probe.done") && deadline.elapsed() < 25000) {
            QVariantList controls;
            QList<QQuickItem *> pending{form};
            while (!pending.isEmpty()) {
                auto *control = pending.takeLast();
                if (control->isVisible() && (control->objectName().startsWith("studio-spin-")
                    || control->objectName().startsWith("studio-combo-"))) {
                    const QPointF point = control->mapToScene({30, 15});
                    controls.append(QVariantMap{{"name", control->objectName()},
                        {"point", QVariantList{point.x(), point.y()}},
                        {"value", control->property(control->objectName().startsWith("studio-spin-")
                            ? "value" : "currentIndex")}});
                }
                pending.append(control->childItems());
            }
            const auto point = [](QQuickItem *item, QPointF local) {
                const auto mapped = item->mapToScene(local);
                return QVariantList{mapped.x(), mapped.y()};
            };
            const QPointF top = page->mapToScene({0, 0});
            const QVariantMap state{{"sample", ++sample}, {"size", QVariantList{window->width(), window->height()}},
                {"header", point(page, {50, 20})}, {"tabs", point(tabs, {150, 15})},
                {"viewport", QVariantList{top.x(), top.y(), page->width(), page->height()}},
                {"innerY", inner->property("contentY")}, {"outerY", outer->property("contentY")},
                {"tabX", tabView->property("contentX")}, {"selection", popup->property("subTabIndex")},
                {"controls", controls}};
            QSaveFile output(root + "/ui-probe.json");
            QVERIFY(output.open(QIODevice::WriteOnly));
            QVERIFY(output.write(QJsonDocument::fromVariant(state).toJson(QJsonDocument::Compact)) > 0);
            QVERIFY(output.commit());
            QTest::qWait(50);
        }
        QVERIFY2(QFileInfo::exists(root + "/ui-probe.done"), "Native EIS UI driver did not complete");
        QVERIFY(!popup->property("hasSettingsChanges").toBool());
        QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
        return;
    }
    const auto wheel = [&](QQuickItem *target, QPointF point, QPoint pixels, QPoint angles,
                           Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        const QPointF position = target->mapToScene(point);
        QWheelEvent event(position, window->mapToGlobal(position.toPoint()), pixels, angles,
                          Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(window, &event);
        QCoreApplication::processEvents();
    };
    const auto overflows = [](QQuickItem *view) {
        return view->property("contentHeight").toDouble() > view->height();
    };
    QQuickItem *primary = overflows(inner) ? inner : outer;
    QVERIFY(overflows(primary));
    inner->setProperty("contentY", 0);
    outer->setProperty("contentY", 0);
    wheel(page, {50, 20}, {0, -32}, {});
    QCOMPARE(primary->property("contentY").toDouble(), 32.0);
    if (primary == inner) QCOMPARE(outer->property("contentY").toDouble(), 0.0);
    primary->setProperty("contentY", primary->property("contentHeight").toDouble() - primary->height());
    const double end = primary->property("contentY").toDouble();
    wheel(page, {50, 20}, {0, -32}, {});
    QCOMPARE(primary->property("contentY").toDouble(), end);
    if (primary == inner) QVERIFY(outer->property("contentY").toDouble() > 0);
    outer->setProperty("contentY", 0);
    QVERIFY(tabView->property("contentWidth").toDouble() > tabView->width());
    wheel(tabs, {150, 15}, {}, {-120, 0});
    QVERIFY(tabView->property("contentX").toDouble() > 0);
    QCOMPARE(popup->property("subTabIndex").toInt(), 2);
    const double x = tabView->property("contentX").toDouble();
    wheel(tabs, {150, 15}, {-17, 0}, {});
    QCOMPARE(tabView->property("contentX").toDouble(), x + 17);
    wheel(tabs, {150, 15}, {0, -22}, {}, Qt::ShiftModifier);
    QCOMPARE(tabView->property("contentX").toDouble(), x + 39);
    QCOMPARE(popup->property("subTabIndex").toInt(), 2);
    primary->setProperty("contentY", 0);
    wheel(tabs, {150, 15}, {0, -30}, {});
    QCOMPARE(primary->property("contentY").toDouble(), 30.0);
    QCOMPARE(popup->property("subTabIndex").toInt(), 2);
    struct Selection { int section; int page; QString prefix; const char *property; };
    for (const auto &selection : {Selection{2, 0, "studio-spin-", "value"},
                                 Selection{1, 2, "studio-combo-", "currentIndex"}}) {
        popup->setProperty("mainTabIndex", selection.section);
        popup->setProperty("subTabIndex", selection.page);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        inner->setProperty("contentY", 0);
        QQuickItem *control = nullptr;
        QList<QQuickItem *> pending{form};
        while (!pending.isEmpty()) {
            auto *candidate = pending.takeLast();
            if (candidate->isVisible() && candidate->objectName().startsWith(selection.prefix)) {
                control = candidate;
                break;
            }
            pending.append(candidate->childItems());
        }
        QVERIFY(control);
        const char *property = selection.property;
        const auto value = control->property(property);
        auto *outerContent = outer->property("contentItem").value<QQuickItem *>();
        QVERIFY(outerContent);
        const double maxY = qMax(0.0, outer->property("contentHeight").toDouble() - outer->height());
        outer->setProperty("contentY", qBound(0.0,
            control->mapToItem(outerContent, {30, 15}).y() - outer->height() / 2, maxY));
        QQuickItem *scrolling = overflows(inner) ? inner : outer;
        QVERIFY(overflows(scrolling));
        const double y = scrolling->property("contentY").toDouble();
        const bool atEnd = y >= scrolling->property("contentHeight").toDouble() - scrolling->height();
        wheel(control, {30, 15}, {}, {0, atEnd ? 120 : -120});
        QCOMPARE(control->property(property), value);
        QVERIFY(scrolling->property("contentY").toDouble() != y);
    }
    QVERIFY(!popup->property("hasSettingsChanges").toBool());
    QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
}

void PanelWindowCapabilityTest::studioArtworkPersistenceFailure_data()
{
    QTest::addColumn<QString>("action");
    QTest::addColumn<bool>("partial");
    QTest::newRow("import") << QString("import") << false;
    QTest::newRow("clear") << QString("clear") << false;
    QTest::newRow("committed-settings-failed-artwork") << QString("import") << true;
}

void PanelWindowCapabilityTest::studioArtworkPersistenceFailure()
{
    QFETCH(QString, action); QFETCH(bool, partial);
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const auto source = QUrl::fromLocalFile(QFINDTESTDATA("fixtures/theme-v2/valid-procedural.json"));
    QVERIFY(registry->importTheme("bottom", source));
    registry->setPanelValue("bottom", "themeStatus", "Previously adopted artwork");
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> popup(component.createWithInitialProperties({{"selectedPanelId", "bottom"}}));
    QVERIFY2(popup != nullptr, qPrintable(component.errorString()));
    auto *studio = qobject_cast<QQuickWindow *>(popup.get()); QVERIFY(studio);
    studio->show(); QVERIFY(QTest::qWaitForWindowExposed(studio));
    QVERIFY(QQuickTest::qWaitForPolish(studio));
    const auto before = registry->panelSnapshot("bottom");
    const QString path = QSettings().fileName();
    const auto permissions = QFile::permissions(path);
    const auto restore = qScopeGuard([&] { QFile::setPermissions(path, permissions); });
    bool blocked = false;
    const auto block = [&] { blocked = QFile::setPermissions(path, QFileDevice::ReadOwner); };
    QMetaObject::Connection afterCommit;
    if (partial)
    {
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "setFieldValue",
            Q_ARG(QVariant, (QVariantMap{{"key", "opacity"}, {"scope", "panel"}})), Q_ARG(QVariant, 0.61)));
        QVERIFY(popup->property("hasSettingsChanges").toBool());
        afterCommit = connect(registry, &PanelRegistry::revisionChanged, popup.get(), block);
    }
    else block();
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "stageArtifact",
        Q_ARG(QVariant, action), Q_ARG(QVariant, source)));
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "acceptStudioChanges"));
    if (partial) disconnect(afterCommit);
    QVERIFY(blocked);
    QVERIFY(studio->isVisible());
    QCOMPARE(popup->property("artifactDraft").value<QJSValue>().toVariant().toMap().value("action").toString(), action);
    const QString error = popup->property("studioError").toString();
    QVERIFY2(error.contains("sav", Qt::CaseInsensitive), qPrintable(error));
    QCOMPARE(error.contains("settings transaction completed", Qt::CaseInsensitive), partial);
    QCOMPARE(registry->panelValue("bottom", "themePackageManifest"), before.value("themePackageManifest"));
    if (partial) QCOMPARE(registry->panelValue("bottom", "opacity").toDouble(), 0.61);
    else QCOMPARE(registry->panelValue("bottom", "settingsRevision"), before.value("settingsRevision"));
    QVERIFY(QFile::setPermissions(path, permissions));
    PanelRegistry reloaded;
    QCOMPARE(reloaded.panelValue("bottom", "themePackageManifest"), before.value("themePackageManifest"));
    if (partial) QCOMPARE(reloaded.panelValue("bottom", "opacity").toDouble(), 0.61);
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "acceptStudioChanges"));
    QVERIFY(!studio->isVisible());
    QVERIFY(!popup->property("hasPendingChanges").toBool());
    if (action == "clear") QVERIFY(registry->panelValue("bottom", "themePackageManifest").toString().isEmpty());
}

void PanelWindowCapabilityTest::screenSignalsCoalesceAndUtilityWindowFitsWorkArea()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(qEnvironmentVariable("QML_IMPORT_PATH",
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml-imports")));
    QVERIFY(QFileInfo::exists(QStringLiteral(":/qt/qml/ArchDock/qml/runtime/SettingsPopup.qml")));
    QVERIFY(QFileInfo::exists(QStringLiteral(":/qt/qml/ArchDock/qml/runtime/StudioForm.qml")));
    PanelWindow backend(engine);
    QScreen *screen = QGuiApplication::primaryScreen();
    QVERIFY(screen);
    QSignalSpy changes(&backend, &PanelWindow::screenRevisionChanged);
    screen->geometryChanged(screen->geometry());
    screen->availableGeometryChanged(screen->availableGeometry());
    screen->logicalDotsPerInchChanged(screen->logicalDotsPerInch());
    QTRY_COMPARE(changes.count(), 1);
    QTest::qWait(150);
    QCOMPARE(changes.count(), 1);
    backend.showSettings();
    QWindow *studio = nullptr;
    for (QWindow *window : QGuiApplication::allWindows())
        if (window->title() == QStringLiteral("Arch Dock Panel Studio")) studio = window;
    QVERIFY(studio);
    QVERIFY(studio->width() <= studio->screen()->availableGeometry().width());
    QVERIFY(studio->height() <= studio->screen()->availableGeometry().height());
    QVERIFY(studio->width() > 0 && studio->height() > 0);
}

void PanelWindowCapabilityTest::groupedWindowsFollowLiveKWinUpdates()
{
    if (!qEnvironmentVariableIsSet("ARCHDOCK_PRIVATE_INTERACTION_TEST"))
        QSKIP("Requires the disposable KWin Wayland rendering-import-smoke session");
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("wayland"));
    QVERIFY(!qEnvironmentVariableIsEmpty("ARCHDOCK_RENDERING_SESSION_ROOT"));
    auto bus = QDBusConnection::sessionBus();
    const QString service = QStringLiteral("org.archdock.ArchDock");
    QVERIFY(bus.registerService(service));
    const QString oldDesktopFile = QGuiApplication::desktopFileName();
    const auto release = qScopeGuard([&] {
        bus.unregisterService(service);
        QGuiApplication::setDesktopFileName(oldDesktopFile);
    });
    QGuiApplication::setDesktopFileName(QStringLiteral("org.archdock.previewfixture"));
    QQmlApplicationEngine engine;
    PanelWindow backend(engine);
    auto *windows = qobject_cast<WindowModel *>(engine.rootContext()
        ->contextProperty(QStringLiteral("windowModel")).value<QObject *>());
    auto *dock = qobject_cast<DockModel *>(engine.rootContext()
        ->contextProperty(QStringLiteral("dockModel")).value<QObject *>());
    QVERIFY(windows);
    QVERIFY(dock);
    QQuickWindow first;
    QQuickWindow second;
    first.setTitle(QStringLiteral("Arch Dock preview first"));
    second.setTitle(QStringLiteral("Arch Dock preview second"));
    first.resize(320, 180);
    second.resize(320, 180);
    first.show();
    second.show();
    const auto idForTitle = [&](const QString &title) {
        for (const WindowItem &item : windows->windows())
            if (item.caption == title)
                return item.internalId;
        return QString{};
    };
    QTRY_VERIFY_WITH_TIMEOUT(!idForTitle(first.title()).isEmpty(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!idForTitle(second.title()).isEmpty(), 5000);
    const QString firstId = idForTitle(first.title());
    const QString secondId = idForTitle(second.title());
    QVERIFY(firstId != secondId);
    QString appId;
    for (const QVariant &value : dock->panelEntries(QStringLiteral("tasks")))
    {
        const auto entry = value.toMap();
        if (entry.value("windowIds").toStringList().contains(firstId))
            appId = entry.value("appId").toString();
    }
    QVERIFY(!appId.isEmpty());
    const auto rows = [&] { return dock->applicationEntry(appId).value("windowPreviews").toList(); };
    const auto rowForId = [&](const QString &id) {
        for (const QVariant &value : rows())
            if (value.toMap().value("windowId").toString() == id)
                return value.toMap();
        return QVariantMap{};
    };
    QCOMPARE(rows().size(), 2);
    QVERIFY(rowForId(secondId).value("canActivate").toBool());
    second.setTitle(QStringLiteral("Arch Dock preview renamed"));
    QTRY_COMPARE(rowForId(secondId).value("title").toString(), second.title());
    QVERIFY(backend.requestDockWindowAction(appId, firstId, QStringLiteral("minimize")));
    QTRY_VERIFY(rowForId(firstId).value("minimized").toBool());
    QVERIFY(!rowForId(secondId).value("minimized").toBool());
    QVERIFY(backend.requestDockWindowAction(appId, firstId, QStringLiteral("restore")));
    QTRY_VERIFY(!rowForId(firstId).value("minimized").toBool());
    second.showMinimized();
    QTRY_VERIFY(rowForId(secondId).value("minimized").toBool());
    QVERIFY(backend.activateDockWindow(appId, secondId));
    QTRY_VERIFY(!rowForId(secondId).value("minimized").toBool());
    QTRY_VERIFY(rowForId(secondId).value("active").toBool());
    QVERIFY(backend.requestDockWindowAction(appId, firstId, QStringLiteral("close")));
    QTRY_COMPARE(rows().size(), 1);
    QTRY_VERIFY(!first.isVisible());
    QVERIFY(rowForId(firstId).isEmpty());
    QVERIFY(!backend.activateDockWindow(appId, firstId));
    QVERIFY(!backend.requestDockWindowAction(appId, firstId, QStringLiteral("close")));
    QVERIFY(rowForId(secondId).value("active").toBool());
    QVERIFY(backend.requestDockWindowAction(appId, secondId, QStringLiteral("close")));
    QTRY_VERIFY(rows().isEmpty());
    QTRY_VERIFY(!second.isVisible());
}

void PanelWindowCapabilityTest::desktopLaunchIsBoundToTheSelectedPanelEntry()
{
    QTemporaryDir files;
    QVERIFY(files.isValid());
    const QString mainMarker = files.filePath(QStringLiteral("main"));
    const QString actionMarker = files.filePath(QStringLiteral("action"));
    const QString path = files.filePath(QStringLiteral("launcher.desktop"));
    QFile desktop(path);
    QVERIFY(desktop.open(QIODevice::WriteOnly));
    desktop.write(QStringLiteral(
        "[Desktop Entry]\nType=Application\nName=Fixture\nIcon=applications-system\n"
        "Exec=/usr/bin/touch \"%1\"\nActions=Write;\n"
        "[Desktop Action Write]\nName=Write\nExec=/usr/bin/touch \"%2\"\n")
        .arg(mainMarker, actionMarker).toUtf8());
    desktop.close();
    QVERIFY(desktop.setPermissions(desktop.permissions() | QFileDevice::ExeOwner));
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QVERIFY(registry);
    const QString panelId = registry->addFreePanel();
    const QString url = QUrl::fromLocalFile(path).toString();
    const QString entryId = ArchDock::PanelContent::urlEntryId(url);
    QVERIFY(window.addPanelEntries(panelId, {url}));
    QVERIFY(window.applyPanelSettingsTransaction(panelId,
        registry->panelDefinition(panelId)->settingsRevision,
        {{QStringLiteral("type"), QStringLiteral("launcher")}})
        .value(QStringLiteral("success")).toBool());
    const auto entries = window.dockEntriesForPanel(panelId, QStringLiteral("launcher"));
    QCOMPARE(entries.size(), 1);
    const auto entry = entries.first().toMap();
    QCOMPARE(entry.value("appId").toString(), entryId);
    QVERIFY(entry.value("canNewInstance").toBool());
    QCOMPARE(entry.value("desktopActions").toList().size(), 1);
    QVERIFY(!window.launchDockEntry(QStringLiteral("missing"), entryId, QString{}));
    QVERIFY(!window.launchDockEntry(QStringLiteral("bottom"), entryId, QString{}));
    QVERIFY(!window.launchDockEntry(panelId, QStringLiteral("unknown"), QString{}));
    QVERIFY(!window.launchDockEntry(panelId, entryId, QStringLiteral("unknown")));
    QVERIFY(window.launchDockEntry(panelId, entryId, QString{}));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(mainMarker), 5000);
    QVERIFY(window.launchDockEntry(panelId, entryId, QStringLiteral("Write")));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(actionMarker), 5000);
    QCOMPARE(window.dockEntriesForPanel(panelId, QStringLiteral("launcher"))
        .first().toMap().value("stableIdentity"), entry.value("stableIdentity"));
    QVERIFY(window.removePanelEntry(panelId, entryId));
    QVERIFY(!window.launchDockEntry(panelId, entryId, QString{}));
}

void PanelWindowCapabilityTest::folderRequestsValidatePanelAndChild()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    for (const auto &name : {QStringLiteral("document.txt"), QStringLiteral("unsafe.desktop")})
    {
        QFile file(folder.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("document\n");
    }
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QVERIFY(registry);
    const QString freePanel = registry->addFreePanel();
    const QString foreignPanel = registry->addFreePanel();
    const QString url = QUrl::fromLocalFile(folder.path()).toString();
    const QString nativeId = QStringLiteral("file:") + QFileInfo(folder.path()).canonicalFilePath();
    const QString freeId = ArchDock::PanelContent::urlEntryId(url);
    QVERIFY(window.pinDockUrl(url));
    QVERIFY(window.addPanelEntries(freePanel, {url}));
    QVERIFY(window.applyPanelSettingsTransaction(freePanel,
        registry->panelDefinition(freePanel)->settingsRevision,
        {{QStringLiteral("type"), QStringLiteral("launcher")}})
        .value(QStringLiteral("success")).toBool());
    const auto before = settingsSnapshot();
    const auto native = window.panelFolderSnapshot(QStringLiteral("bottom"), nativeId);
    const auto free = window.panelFolderSnapshot(freePanel, freeId);
    QCOMPARE(native.value("status").toString(), QStringLiteral("ready"));
    QCOMPARE(free.value("entries"), native.value("entries"));
    const auto children = native.value("entries").toList();
    QCOMPARE(children.size(), 2);
    QCOMPARE(window.dockFolderEntries(nativeId).size(), 2);
    const QString documentId = children.first().toMap().value("id").toString();
    QCOMPARE(children.first().toMap().value("name").toString(), QStringLiteral("document.txt"));
    const QString unsafeId = children.last().toMap().value("id").toString();
    for (const auto &panel : {QStringLiteral("missing"), foreignPanel})
    {
        QCOMPARE(window.panelFolderSnapshot(panel, freeId).value("errorCode").toString(),
                 QStringLiteral("folder-entry-unavailable"));
        QVERIFY(!window.openPanelFolderChild(panel, freeId, documentId).value("success").toBool());
    }
    QVERIFY(!window.openPanelFolderChild(freePanel, nativeId, documentId).value("success").toBool());
    QVERIFY(!window.openPanelFolderChild(freePanel, freeId, QStringLiteral("folder-child:file:///etc/passwd"))
                 .value("success").toBool());
    QCOMPARE(window.openPanelFolderChild(freePanel, freeId, unsafeId).value("errorCode").toString(),
             QStringLiteral("executable-entry"));
    QCOMPARE(settingsSnapshot(), before);
    QVERIFY(QFile::remove(folder.filePath("document.txt")));
    QCOMPARE(window.openPanelFolderChild(freePanel, freeId, documentId).value("errorCode").toString(),
             QStringLiteral("child-not-listed"));
    QVERIFY(window.removePanelEntry(freePanel, freeId));
    QCOMPARE(window.panelFolderSnapshot(freePanel, freeId).value("errorCode").toString(),
             QStringLiteral("folder-entry-unavailable"));
    QVERIFY(QDir(folder.path()).removeRecursively());
    QCOMPARE(window.panelFolderSnapshot(QStringLiteral("bottom"), nativeId).value("status").toString(),
             QStringLiteral("unavailable"));
    QVERIFY(!window.openPanelFolderChild(QStringLiteral("bottom"), nativeId, documentId)
                 .value("success").toBool());
}

void PanelWindowCapabilityTest::rendererSwitchRetainsOnlyUnchangedInactiveFields()
{
    if (!(ARCHDOCK_QUICK3D_BUILT && ARCHDOCK_SCENE3D_BUILT))
        return;
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QVERIFY(registry);
    const QString panelId = registry->addFreePanel();
    auto values = registry->themeCandidate(panelId, QStringLiteral("mesh-platform-cyan"),
        QStringLiteral("complete")).value(QStringLiteral("values")).toMap();
    values.insert(QStringLiteral("rendererTier"), QStringLiteral("procedural2d"));
    QVERIFY(window.applyPanelSettingsTransaction(panelId,
        registry->panelDefinition(panelId)->settingsRevision, values, {})
        .value(QStringLiteral("success")).toBool());
    const auto snapshot = window.panelSettingsEditorSnapshot(panelId, QStringLiteral("studio"));
    QVERIFY(!fieldKeys(snapshot.value(QStringLiteral("panelFields")).toList())
        .contains(QStringLiteral("scene3DQuality")));
    const auto revision = registry->panelDefinition(panelId)->settingsRevision;
    const auto before = registry->panelDefinition(panelId)->toPersistedMap();
    QCOMPARE(window.applyPanelSettingsTransaction(panelId, revision,
        {{QStringLiteral("scene3DQuality"), QStringLiteral("high")}}, {})
        .value(QStringLiteral("errorCode")).toString(), QStringLiteral("unavailable-panel-field"));
    QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), before);
    values = snapshot.value(QStringLiteral("panelValues")).toMap();
    QVERIFY(values.contains(QStringLiteral("appearance")));
    values.insert(QStringLiteral("rendererTier"), QStringLiteral("true3d"));
    const auto resolved = window.resolvePanelSettingsEditorDraft(panelId, revision,
        values, {}, QStringLiteral("studio"));
    QVERIFY2(resolved.value(QStringLiteral("success")).toBool(),
             qPrintable(resolved.value(QStringLiteral("errorMessage")).toString()));
    QVERIFY(!fieldKeys(resolved.value(QStringLiteral("panelFields")).toList())
        .contains(QStringLiteral("appearance")));
    QVERIFY(fieldKeys(resolved.value(QStringLiteral("panelFields")).toList())
        .contains(QStringLiteral("scene3DQuality")));
    QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), before);
    auto invalid = values;
    invalid.insert(QStringLiteral("appearance"), values.value(QStringLiteral("appearance")) ==
        QStringLiteral("platform") ? QStringLiteral("flat") : QStringLiteral("platform"));
    const auto rejected = window.applyPanelSettingsTransaction(panelId, revision, invalid, {});
    QCOMPARE(rejected.value(QStringLiteral("errorCode")).toString(),
             QStringLiteral("unavailable-panel-field"));
    invalid = values;
    invalid.insert(QStringLiteral("hostKind"), QStringLiteral("free-desktop"));
    QCOMPARE(window.applyPanelSettingsTransaction(panelId, revision, invalid, {})
        .value(QStringLiteral("errorCode")).toString(), QStringLiteral("protected-panel-field"));
    QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), before);
    QVERIFY(window.applyPanelSettingsTransaction(panelId, revision, values, {})
        .value(QStringLiteral("success")).toBool());
    QCOMPARE(window.panelRendererConfiguration(panelId)
        .value(QStringLiteral("effectiveRendererTier")).toString(), QStringLiteral("true3d"));
}

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
    const auto keys = fieldKeys(snapshot.value(QStringLiteral("panelFields")).toList());
    for (const QString &key : {QStringLiteral("layoutAngle"), QStringLiteral("panelRotationMode"),
                               QStringLiteral("panelRotationSpeed"), QStringLiteral("panelRotationTrigger")})
        QVERIFY(keys.contains(key));
    const auto configuration = window.panelRendererConfiguration(panelId);
    QCOMPARE(configuration.value(QStringLiteral("capabilityResolution")).toMap()
        .value(QStringLiteral("rotation")).toMap().value(QStringLiteral("available")).toBool(), true);
    QCOMPARE(configuration.value(QStringLiteral("effectiveRendererTier")).toString(),
             available ? QStringLiteral("true3d") : QStringLiteral("procedural2d"));
    QVERIFY(configuration.value(QStringLiteral("themeDefinition")).toMap()
        .contains(QStringLiteral("scene3DResources")));
    if (available)
    {
        const auto rotationDraft = window.resolvePanelSettingsEditorDraft(panelId,
            registry->panelDefinition(panelId)->settingsRevision,
            {{QStringLiteral("panelRotationMode"), QStringLiteral("clockwise")}}, {}, QStringLiteral("studio"));
        QVERIFY2(rotationDraft.value(QStringLiteral("success")).toBool(),
                 qPrintable(rotationDraft.value(QStringLiteral("errorMessage")).toString()));
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
    const auto rendererToggle = [&]() -> QQuickItem * {
        QList<QQuickItem *> pending{studio->contentItem()};
        while (!pending.isEmpty())
        {
            QQuickItem *item = pending.takeLast();
            pending.append(item->childItems());
            if (!item->inherits("QQuickSwitch") || !item->isVisible())
                continue;
            for (QQuickItem *parent = item->parentItem(); parent; parent = parent->parentItem())
            {
                const QVariant value = parent->property("modelData");
                const auto row = value.metaType() == QMetaType::fromType<QJSValue>()
                    ? value.value<QJSValue>().toVariant().toMap() : value.toMap();
                if (row.value("key").toString() == QStringLiteral("rendererTier")
                    && row.value("rendererToggle").toBool())
                    return item;
            }
        }
        return nullptr;
    };
    if (available)
    {
        QTRY_VERIFY_WITH_TIMEOUT(popup->property("scene3DControlsAvailable").toBool(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(popup->property("scene3DQualityVisible").toBool(), 5000);
        // Recreating the form must not let a click use its unpolished geometry.
        popup->setProperty("subTabIndex", 1);
        popup->setProperty("subTabIndex", 2);
        QVERIFY(QQuickTest::qWaitForPolish(studio));
        QTRY_VERIFY(rendererToggle());
        QVERIFY(rendererToggle()->property("checked").toBool());
        QTest::mouseClick(studio, Qt::LeftButton, Qt::NoModifier,
            rendererToggle()->mapToScene(QPointF(rendererToggle()->width() / 2,
                                                rendererToggle()->height() / 2)).toPoint());
        QTRY_VERIFY(!popup->property("scene3DQualityVisible").toBool());
        QVERIFY(popup->property("scene3DControlsAvailable").toBool());
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "applyStudioChanges"));
        QTRY_COMPARE(window.panelRendererConfiguration(panelId).value(QStringLiteral("effectiveRendererTier")).toString(),
                     QStringLiteral("procedural2d"));
        QVERIFY(!fieldKeys(window.panelSettingsEditorSnapshot(panelId, QStringLiteral("studio"))
            .value(QStringLiteral("panelFields")).toList()).contains(QStringLiteral("scene3DQuality")));
        QVERIFY(QQuickTest::qWaitForPolish(studio));
        QTRY_VERIFY(rendererToggle());
        QVERIFY(!rendererToggle()->property("checked").toBool());
        QTest::mouseClick(studio, Qt::LeftButton, Qt::NoModifier,
            rendererToggle()->mapToScene(QPointF(rendererToggle()->width() / 2,
                                                rendererToggle()->height() / 2)).toPoint());
        QTRY_VERIFY2(popup->property("scene3DQualityVisible").toBool(),
                     qPrintable(popup->property("studioError").toString()));
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "cancelStudioChanges"));
        QVERIFY(!studio->isVisible());
        QCOMPARE(window.panelRendererConfiguration(panelId)
            .value(QStringLiteral("effectiveRendererTier")).toString(), QStringLiteral("procedural2d"));
        studio->show();
        QVERIFY(QTest::qWaitForWindowExposed(studio));
        QTRY_VERIFY(!popup->property("scene3DQualityVisible").toBool());
    }
    else
    {
        QVERIFY(!popup->property("scene3DControlsAvailable").toBool());
        QVERIFY(!popup->property("scene3DQualityVisible").toBool());
        QVERIFY(!rendererToggle());
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
    QCOMPARE(nativeKeys.size(), 8);
    QVERIFY(nativeKeys.contains(QStringLiteral("visible")));
    QVERIFY(nativeKeys.contains(QStringLiteral("visibilityMode")));
    QVERIFY(nativeKeys.contains(QStringLiteral("acceptDrops")));
    QVERIFY(nativeKeys.contains(QStringLiteral("iconStyle")));
    for (const QString &key : {QStringLiteral("folderLayout"), QStringLiteral("folderSpeed"),
         QStringLiteral("folderEasing"), QStringLiteral("folderExpandOnClick")})
        QVERIFY(nativeKeys.contains(key));
    QCOMPARE(fieldByKey(nativeFields, QStringLiteral("folderLayout"))
                 .value(QStringLiteral("choices")).toStringList(),
             QStringList({QStringLiteral("fan"), QStringLiteral("grid"), QStringLiteral("stack"),
                          QStringLiteral("arc"), QStringLiteral("ring")}));
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
    QVERIFY(studioKeys.contains(QStringLiteral("folderLayout")));
    QCOMPARE(fieldByKey(studioFields, QStringLiteral("folderLayout"))
                 .value(QStringLiteral("choices")),
             fieldByKey(nativeFields, QStringLiteral("folderLayout")).value(QStringLiteral("choices")));
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

void PanelWindowCapabilityTest::nativePanelObservationsRequireUniqueFrame()
{
    WindowModel model;
    WindowWatcher watcher(model);
    QSignalSpy changed(&watcher, &WindowWatcher::nativePanelsChanged);
    const QRectF bounds(288, 628, 720, 92);
    const auto update = [&](const QString &id, int x, bool hidden) {
        watcher.windowUpdated(id, {}, QStringLiteral("plasmashell"), {}, {}, false, false,
            QString::fromUtf8(QJsonDocument::fromVariant(
                QVariantMap{{"dock", true}, {"hidden", hidden}, {"x", x}, {"y", 628},
                            {"width", 720}, {"height", 92}}).toJson(QJsonDocument::Compact)));
    };
    update("native-1", 280, false);
    QVERIFY(watcher.nativePanelState(bounds).value("available").toBool());
    QVERIFY(!watcher.nativePanelState(bounds).value("hidden").toBool());
    for (int i = 0; i < 10; ++i) update("native-1", 280, false);
    QCOMPARE(changed.count(), 1);
    update("native-1", 280, true);
    QCOMPARE(changed.count(), 2);
    QVERIFY(watcher.nativePanelState(bounds).value("hidden").toBool());
    update("native-2", 288, false);
    QVERIFY(!watcher.nativePanelState(bounds).value("available").toBool());
    watcher.windowRemoved("native-2");
    QVERIFY(watcher.nativePanelState(bounds).value("hidden").toBool());
    update("native-1", 2000, false);
    QVERIFY(!watcher.nativePanelState(bounds).value("available").toBool());
    watcher.windowRemoved("native-1");
    QVERIFY(!watcher.nativePanelState(QRectF(2000, 628, 720, 92)).value("available").toBool());
    QVERIFY(!watcher.nativePanelState({}).value("available").toBool());
}

void PanelWindowCapabilityTest::contentProvidersUseTransactionsAndVisibility()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    auto *status = qobject_cast<SystemStatus *>(engine.rootContext()
        ->contextProperty(QStringLiteral("systemStatus")).value<QObject *>());
    QVERIFY(registry && status);
    for (const auto &id : registry->panelIds()) {
        registry->updatePanel(id, {{"visible", false}});
        QVERIFY(!registry->panelValue(id, QStringLiteral("visible")).toBool());
    }
    QTRY_VERIFY(status->availableSources().contains(QStringLiteral("status:cpu")));
    const auto panelId = registry->addFreePanel();
    auto panel = *registry->panelDefinition(panelId);
    panel.segments.first().source = QStringLiteral("status");
    panel.segments.first().entryIds = {QStringLiteral("status:cpu"), QStringLiteral("status:memory")};
    const auto applied = window.applyPanelSettingsTransaction(panelId, panel.settingsRevision,
        {{"layout", "horizontal"}, {"segments", QVariantList{panel.segments.first().toVariantMap()}},
         {"showTemporaryStatus", false}});
    QVERIFY2(applied.value("success").toBool(), qPrintable(applied.value("errorMessage").toString()));
    const auto rows = window.dockEntriesForPanel(panelId, "launcher");
    QCOMPARE(rows.size(), 2);
    for (const auto &value : rows) {
        const auto entry = value.toMap();
        QVERIFY(entry.value("isStatus").toBool());
        QVERIFY(entry.value("statusAvailable").toBool());
        QVERIFY(!window.activateDockEntry(entry.value("appId").toString()));
        QVERIFY(!window.launchDockEntry(panelId, entry.value("appId").toString(), {}));
    }
    PanelRegistry reloaded;
    QCOMPARE(reloaded.panelDefinition(panelId)->segments, registry->panelDefinition(panelId)->segments);
    QVERIFY(!reloaded.panelDefinition(panelId)->content.showTemporaryStatus);
    registry->updatePanel(panelId, {{"visible", true}});
    QVERIFY(registry->panelValue(panelId, QStringLiteral("visible")).toBool());
    const auto before = status->sampleCount();
    QVERIFY(window.reportPanelPresentationState(panelId,
        {{"surfaceState", "open"}, {"hostPhase", "revealed"}}));
    QTRY_VERIFY(status->sampleCount() > before);
    QVERIFY(window.reportPanelPresentationState(panelId,
        {{"surfaceState", "collapsed"}, {"hostPhase", "concealed"}}));
    QTest::qWait(150);
    const auto concealed = status->sampleCount();
    const auto revision = window.contentRevision();
    QTest::qWait(2150);
    QCOMPARE(status->sampleCount(), concealed);
    QCOMPARE(window.contentRevision(), revision);
    QVERIFY(window.reportPanelPresentationState(panelId,
        {{"surfaceState", "open"}, {"hostPhase", "revealed"}}));
    QTRY_VERIFY(status->sampleCount() > concealed);
}

void PanelWindowCapabilityTest::segmentsUseRevisionedTransactionsAndHostAuthority()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(qEnvironmentVariable("QML_IMPORT_PATH",
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml-imports")));
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QVERIFY(registry);
    const QString panelId = registry->addFreePanel();
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString url = QUrl::fromLocalFile(folder.path()).toString();
    QVERIFY(window.addPanelEntries(panelId, {url}));
    const QString entryId = ArchDock::PanelContent::urlEntryId(url);
    auto base = *registry->panelDefinition(panelId);
    ArchDock::PanelSegmentDefinition custom;
    custom.id = QStringLiteral("files");
    custom.source = QStringLiteral("custom");
    custom.order = 1;
    custom.entryIds = {entryId};
    custom.background = QStringLiteral("solid");
    custom.padding = 12;
    const QVariantList segments{base.segments.first().toVariantMap(), custom.toVariantMap()};
    const auto applied = window.applyPanelSettingsTransaction(panelId, base.settingsRevision,
        {{QStringLiteral("layout"), QStringLiteral("horizontal")},
         {QStringLiteral("type"), QStringLiteral("launcher")},
         {QStringLiteral("segments"), segments}});
    QVERIFY2(applied.value("success").toBool(), qPrintable(QString::fromUtf8(canonicalBytes(applied))));
    QCOMPARE(registry->panelDefinition(panelId)->settingsRevision, base.settingsRevision + 1);
    QCOMPARE(registry->panelDefinition(panelId)->segments.size(), 2);
    QCOMPARE(window.dockEntriesForPanel(panelId, QStringLiteral("launcher")).first()
        .toMap().value("segmentId").toString(), QStringLiteral("files"));
    const auto persisted = registry->panelDefinition(panelId)->toPersistedMap();
    const auto stale = window.applyPanelSettingsTransaction(panelId, base.settingsRevision,
        {{QStringLiteral("segments"), segments}});
    QCOMPARE(stale.value("errorCode").toString(), QStringLiteral("stale-revision"));
    QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), persisted);
    const auto revision = registry->panelDefinition(panelId)->settingsRevision;
    for (const auto &bad : {
        QVariantMap{{"entryIds", QStringList{QStringLiteral("foreign-entry")}}},
        QVariantMap{{"source", QStringLiteral("status")}, {"entryIds", QStringList{QStringLiteral("status:unknown")}}},
        QVariantMap{{"motionProfile", QStringLiteral("missing-profile")}}})
    {
        auto invalid = custom.toVariantMap();
        for (auto it = bad.cbegin(); it != bad.cend(); ++it)
            invalid.insert(it.key(), it.value());
        const auto rejected = window.applyPanelSettingsTransaction(panelId, revision,
            {{QStringLiteral("segments"), QVariantList{base.segments.first().toVariantMap(), invalid}}});
        QVERIFY(!rejected.value("success").toBool());
        QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), persisted);
    }
    const auto radial = window.applyPanelSettingsTransaction(panelId, revision,
        {{QStringLiteral("layout"), QStringLiteral("ring")}});
    QCOMPARE(radial.value("errorCode").toString(), QStringLiteral("unavailable-segment-feature"));
    QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), persisted);
    auto first = base.segments.first();
    first.order = 1;
    custom.order = 0;
    QVERIFY(window.applyPanelSettingsTransaction(panelId, revision,
        {{QStringLiteral("segments"), QVariantList{first.toVariantMap(), custom.toVariantMap()}}})
        .value("success").toBool());
    QCOMPARE(registry->panelDefinition(panelId)->segments.first().id, QStringLiteral("files"));
    PanelRegistry reloaded;
    QCOMPARE(reloaded.panelDefinition(panelId)->segments, registry->panelDefinition(panelId)->segments);
    const auto native = window.panelSettingsEditorSnapshot(QStringLiteral("bottom"), QStringLiteral("studio"));
    const auto descriptor = fieldByKey(native.value("panelFields").toList(), QStringLiteral("segments"));
    QVERIFY(descriptor.value("segmentCapabilities").toMap().value("available").toBool());
    QCOMPARE(descriptor.value("segmentCapabilities").toMap().value("sources").toStringList().contains(QStringLiteral("status")),
        !window.contentRuntimeSnapshot(QStringLiteral("bottom")).value("availableSources").toStringList().isEmpty());
    first.order = 0;
    QVERIFY(window.applyPanelSettingsTransaction(panelId, revision + 1,
        {{QStringLiteral("segments"), QVariantList{first.toVariantMap()}}}).value("success").toBool());
    QCOMPARE(window.dockEntriesForPanel(panelId, QStringLiteral("launcher")).first()
        .toMap().value("segmentId").toString(), QStringLiteral("main"));
    if (qEnvironmentVariableIsEmpty("ARCHDOCK_PRIVATE_INTERACTION_TEST"))
        return;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> popup(component.createWithInitialProperties({
        {QStringLiteral("selectedPanelId"), panelId}, {QStringLiteral("mainTabIndex"), 1},
        {QStringLiteral("subTabIndex"), 5}}));
    QVERIFY2(popup != nullptr, qPrintable(component.errorString()));
    auto *studio = qobject_cast<QQuickWindow *>(popup.get());
    QVERIFY(studio);
    studio->show();
    QVERIFY(QTest::qWaitForWindowExposed(studio));
    QVERIFY(QQuickTest::qWaitForPolish(studio));
    const auto candidate = [&]() {
        return popup->property("selectedRendererCandidate").value<QJSValue>().toVariant().toMap();
    };
    const auto action = [&](const QString &name, int index = 0) {
        return QMetaObject::invokeMethod(popup.get(), "performStudioAction",
            Q_ARG(QVariant, name), Q_ARG(QVariant, (QVariantMap{{QStringLiteral("segmentIndex"), index}})));
    };
    const auto beforeStudio = registry->panelDefinition(panelId)->toPersistedMap();
    QVERIFY(action(QStringLiteral("segment-add")));
    QTRY_COMPARE(candidate().value("segments").toList().size(), 2);
    QVERIFY(popup->property("studioError").toString().isEmpty());
    QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), beforeStudio);
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "setFieldValue",
        Q_ARG(QVariant, (QVariantMap{{"segmentIndex", 1}, {"entryId", entryId}})), Q_ARG(QVariant, true)));
    QTRY_COMPARE(candidate().value("segmentEntries").toList().first().toMap()
        .value("segmentId").toString(), QStringLiteral("segment-1"));
    QVERIFY(action(QStringLiteral("segment-up"), 1));
    QCOMPARE(candidate().value("segments").toList().first().toMap().value("id").toString(),
        QStringLiteral("segment-1"));
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "cancelStudioChanges"));
    QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), beforeStudio);
    studio->show();
    QVERIFY(QTest::qWaitForWindowExposed(studio));
    QTRY_COMPARE(candidate().value("segments").toList().size(), 1);
    QVERIFY(action(QStringLiteral("segment-add")));
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "applyStudioChanges"));
    QTRY_COMPARE(registry->panelDefinition(panelId)->segments.size(), 2);
    QVERIFY(!popup->property("hasPendingChanges").toBool());
    QVERIFY(action(QStringLiteral("segment-remove"), 1));
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "applyStudioChanges"));
    QTRY_COMPARE(registry->panelDefinition(panelId)->segments.size(), 1);
    studio->close();
    qInfo() << "Private Studio segment draft, preview ownership, reorder, Cancel, Apply and removal passed";
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

    // QTest sends Qt events without a compositor input serial. This editor
    // transaction fixture therefore uses Popup.Item (0); the EIS-driven
    // window-interaction-smoke covers the production native popup window.
    QObject *menu = propertiesAction->property("menu").value<QObject *>();
    QVERIFY(menu);
    QVERIFY(QQmlProperty::write(menu, QStringLiteral("popupType"), 0));

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

    auto *windows = qobject_cast<WindowModel *>(engine.rootContext()
        ->contextProperty(QStringLiteral("windowModel")).value<QObject *>());
    QVERIFY(windows);
    WindowItem first;
    first.internalId = QStringLiteral("alpha-one");
    first.desktopFileName = alphaUrl.toLocalFile();
    first.caption = QStringLiteral("First Alpha document");
    first.canActivate = true;
    WindowItem second = first;
    second.internalId = QStringLiteral("alpha-two");
    second.caption = QStringLiteral("Second Alpha document");
    second.minimized = true;
    windows->setWindows({first, second});
    const auto alphaSnapshot = [&window, &panelId, &alphaId] {
        for (const QVariant &value : window.dockEntriesForPanel(panelId, QStringLiteral("hybrid")))
        {
            const auto entry = value.toMap();
            if (entry.value(QStringLiteral("appId")).toString() == alphaId)
                return entry;
        }
        return QVariantMap{};
    };
    auto merged = alphaSnapshot();
    QCOMPARE(merged.value("iconName"), alphaEntry.value("iconName"));
    QCOMPARE(merged.value("baseIconName"), alphaEntry.value("baseIconName"));
    QCOMPARE(merged.value("resolvedGlyph"), alphaEntry.value("resolvedGlyph"));
    QCOMPARE(merged.value("panelEntryId").toString(), alphaId);
    QVERIFY(!merged.value("runningAppId").toString().isEmpty());
    QCOMPARE(merged.value("windowPreviews").toList().size(), 2);
    QCOMPARE(merged.value("windowPreviews").toList().at(1).toMap()
                 .value("windowId").toString(), second.internalId);
    second.caption = QStringLiteral("Renamed Alpha document");
    QVERIFY(windows->updateWindow(second));
    QCOMPARE(alphaSnapshot().value("windowPreviews").toList().at(1).toMap()
                 .value("title").toString(), second.caption);
    windows->clearWindows();
    QVERIFY(alphaSnapshot().value("windowPreviews").toList().isEmpty());
    QCOMPARE(alphaSnapshot().value("iconName"), alphaEntry.value("iconName"));
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

void PanelWindowCapabilityTest::profileServicePersistsAndHonorsPanelGuards()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    auto *profiles = qobject_cast<ArchDock::ProfileManager *>(engine.rootContext()
        ->contextProperty(QStringLiteral("profileManager")).value<QObject *>());
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QVERIFY(profiles);
    QVERIFY(registry);
    const auto before = registry->panelDefinitions();
    QVERIFY(profiles->listProfiles().isEmpty());
    const auto created = profiles->createProfile(QStringLiteral("Current arrangement"));
    QVERIFY2(created.value(QStringLiteral("success")).toBool(), qPrintable(created.value(QStringLiteral("errorCode")).toString()));
    const QString id = created.value(QStringLiteral("profileId")).toString();
    const auto saved = ArchDock::ProfileStore().load(id);
    QVERIFY(saved);
    QCOMPARE(saved->panels, ArchDock::ProfileDefinition::capture(saved->name, before).panels);
    QVERIFY(window.reportPanelInteractionGuards(QStringLiteral("bottom"),
        {{QStringLiteral("editMode"), true}, {QStringLiteral("popupOpen"), false}, {QStringLiteral("dragActive"), false}}));
    const auto refused = profiles->applyProfile(id, 1);
    QVERIFY(!refused.value(QStringLiteral("success")).toBool());
    QCOMPARE(refused.value(QStringLiteral("errorCode")).toString(), QStringLiteral("edit-mode-active"));
    QCOMPARE(registry->panelDefinitions(), before);
    QVERIFY(!QFileInfo::exists(ArchDock::ProfileApplyTransaction::defaultJournalPath()));
    QVERIFY(window.reportPanelInteractionGuards(QStringLiteral("bottom"),
        {{QStringLiteral("editMode"), false}, {QStringLiteral("popupOpen"), false}, {QStringLiteral("dragActive"), false}}));
    QVERIFY(profiles->renameProfile(id, 1, QStringLiteral("Renamed arrangement")).value(QStringLiteral("success")).toBool());
    QCOMPARE(ArchDock::ProfileStore().load(id)->name, QStringLiteral("Renamed arrangement"));
    QVERIFY(profiles->deleteProfile(id, 2).value(QStringLiteral("success")).toBool());
    QVERIFY(profiles->listProfiles().isEmpty());
}

void PanelWindowCapabilityTest::studioPresetPagesBrowseWithoutChangingAnyPanel()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(qEnvironmentVariable("QML_IMPORT_PATH",
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml-imports")));
    // Warnings raised by Panel Studio's own files. The shared renderer module
    // is held to zero warnings for every preset by preset-library-test.
    QStringList studioWarnings;
    connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
        {
            if (warning.url().path().contains(QStringLiteral("/qml/runtime/")))
                studioWarnings.append(warning.toString());
        }
    });
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QObject *library = engine.rootContext()
        ->contextProperty(QStringLiteral("presetLibrary")).value<QObject *>();
    QVERIFY(registry);
    QVERIFY(library);

    // The user preset store lives in this test's own data directory, and it
    // is removed again whatever happens below.
    const QString presetStore = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation)).filePath(QStringLiteral("presets"));
    const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    QVERIFY2(!dataHome.isEmpty() &&
                 presetStore.startsWith(QDir(dataHome).absolutePath() + QLatin1Char('/')),
             "refusing to use a preset store outside the CTest-provided XDG_DATA_HOME");
    const auto removeStore = qScopeGuard([&presetStore] { QDir(presetStore).removeRecursively(); });
    QVERIFY(!QFileInfo::exists(presetStore));

    const QString panelId = QStringLiteral("bottom");
    QVariantMap panelsBefore;
    for (const QString &id : registry->panelIds())
        panelsBefore.insert(id, registry->panelDefinition(id)->toPersistedMap());
    const int revisionBefore = registry->revision();
    const QVariantMap settingsBefore = settingsSnapshot();
    const auto nothingChanged = [&]() {
        QVariantMap panels;
        for (const QString &id : registry->panelIds())
            panels.insert(id, registry->panelDefinition(id)->toPersistedMap());
        return panels == panelsBefore && registry->revision() == revisionBefore &&
            settingsSnapshot() == settingsBefore;
    };

    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> popup(component.createWithInitialProperties({
        {QStringLiteral("selectedPanelId"), panelId}, {QStringLiteral("mainTabIndex"), 1},
        {QStringLiteral("subTabIndex"), 7}}));
    QVERIFY2(popup != nullptr, qPrintable(component.errorString()));
    auto *studio = qobject_cast<QQuickWindow *>(popup.get());
    QVERIFY(studio);
    studio->show();
    QVERIFY(QTest::qWaitForWindowExposed(studio));
    QVERIFY(QQuickTest::qWaitForPolish(studio));

    const auto plain = [](const QVariant &value) {
        return value.metaType() == QMetaType::fromType<QJSValue>()
            ? value.value<QJSValue>().toVariant() : value;
    };
    const auto findVisible = [&](const QString &name) -> QQuickItem * {
        QList<QQuickItem *> pending{studio->contentItem()};
        while (!pending.isEmpty())
        {
            QQuickItem *item = pending.takeLast();
            if (item->objectName() == name && item->isVisible())
                return item;
            pending.append(item->childItems());
        }
        return nullptr;
    };
    const auto openPage = [&](int section, int subtab) {
        popup->setProperty("mainTabIndex", section);
        popup->setProperty("subTabIndex", subtab);
        return QQuickTest::qWaitForPolish(studio);
    };
    const auto cards = [&]() { return plain(popup->property("presetCards")).toList(); };
    const auto browser = QStringLiteral("panel-studio-preset-browser");
    const auto presetPreview = QStringLiteral("panel-studio-preset-preview");

    // The four preset pages list two separate catalogs, and selecting a card
    // replaces the panel's preview with the preset's without applying it.
    struct Page { int section; int subtab; QString kind; QString scope; int count; };
    for (const Page &page : {Page{1, 7, QStringLiteral("panel"), QStringLiteral("builtin"), 15},
                             Page{1, 8, QStringLiteral("panel"), QStringLiteral("user"), 0},
                             Page{2, 5, QStringLiteral("icon"), QStringLiteral("builtin"), 15},
                             Page{2, 6, QStringLiteral("icon"), QStringLiteral("user"), 0}})
    {
        QVERIFY(openPage(page.section, page.subtab));
        const QVariantMap current = plain(popup->property("currentPresetPage")).toMap();
        QCOMPARE(current.value(QStringLiteral("kind")).toString(), page.kind);
        QCOMPARE(current.value(QStringLiteral("scope")).toString(), page.scope);
        QQuickItem *list = findVisible(browser);
        QVERIFY(list);
        QCOMPARE(list->property("kind").toString(), page.kind);
        QCOMPARE(list->property("scope").toString(), page.scope);
        QVERIFY(list->property("catalogValid").toBool());
        QCOMPARE(cards().size(), page.count);
        // Opening a page selects nothing: the preview is still the panel's.
        QCOMPARE(popup->property("selectedPresetId").toString(), QString{});
        QVERIFY(!popup->property("presetPreviewActive").toBool());
        QVERIFY(!findVisible(presetPreview));
        QVERIFY(findVisible(QStringLiteral("panel-studio-live-renderer-preview")));

        const QVariantList listed = cards();
        for (int index = 0; index < listed.size(); ++index)
        {
            const QVariantMap card = listed.at(index).toMap();
            const QString id = card.value(QStringLiteral("id")).toString();
            QCOMPARE(card.value(QStringLiteral("kind")).toString(), page.kind);
            if (index == 0)
            {
                // The first card is selected the way a person does it.
                QQuickItem *item = findVisible(QStringLiteral("preset-card-") + id);
                QVERIFY2(item, qPrintable(id));
                QTest::mouseClick(studio, Qt::LeftButton, Qt::NoModifier,
                    item->mapToScene(QPointF(40, 20)).toPoint());
            }
            else
            {
                popup->setProperty("selectedPresetId", id);
            }
            QTRY_COMPARE(popup->property("selectedPresetId").toString(), id);
            QTRY_VERIFY2(popup->property("presetPreviewActive").toBool(), qPrintable(id));
            QQuickItem *preview = nullptr;
            QTRY_VERIFY2((preview = findVisible(presetPreview)) != nullptr, qPrintable(id));
            QVERIFY(!findVisible(QStringLiteral("panel-studio-live-renderer-preview")));
            QTRY_COMPARE(preview->property("activeRendererTier").toString(),
                card.value(QStringLiteral("compatibility")).toMap()
                    .value(QStringLiteral("effectiveRendererTier")).toString());
            QVERIFY2(!preview->property("fallbackApplied").toBool(), qPrintable(id));
            QCOMPARE(findVisible(QStringLiteral("panel-studio-preview-title"))->property("text").toString(),
                     QStringLiteral("Preset preview — ") + card.value(QStringLiteral("name")).toString());
            QCOMPARE(findVisible(QStringLiteral("panel-studio-preview-status"))->property("text").toString(),
                     QStringLiteral("Preset preview only — no panel is changed"));
            // A selected preset is not a draft: there is nothing to apply.
            QVERIFY(!popup->property("hasPendingChanges").toBool());
        }
    }
    QVERIFY(nothingChanged());
    QVERIFY(!QFileInfo::exists(presetStore));

    // Leaving a preset page drops the selection and returns the panel's own
    // preview.
    QVERIFY(openPage(1, 0));
    QCOMPARE(popup->property("selectedPresetId").toString(), QString{});
    QVERIFY(!findVisible(browser));
    QVERIFY(!findVisible(presetPreview));
    QVERIFY(findVisible(QStringLiteral("panel-studio-live-renderer-preview")));

    // Themes and icon styles have pages of their own and are not presets.
    QVERIFY(openPage(1, 2));
    QVERIFY(!findVisible(QStringLiteral("theme-live-preview-obsidian-glass")));
    QVERIFY(openPage(1, 6));
    QVERIFY(plain(popup->property("currentPresetPage")).isNull());
    QTRY_VERIFY(findVisible(QStringLiteral("theme-live-preview-obsidian-glass")));
    QVERIFY(!findVisible(browser));
    QVERIFY(openPage(2, 4));
    QTRY_VERIFY(findVisible(QStringLiteral("icon-style-live-preview-metallic-blue")));
    QVERIFY(findVisible(QStringLiteral("icon-style-live-preview-dark-orb")));
    QVERIFY(!findVisible(QStringLiteral("theme-live-preview-obsidian-glass")));
    // Loading an icon style stages it in the draft; the panel keeps its own.
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "performStudioAction",
        Q_ARG(QVariant, QStringLiteral("load-icon-style")),
        Q_ARG(QVariant, (QVariantMap{{QStringLiteral("styleId"), QStringLiteral("metallic-blue")}}))));
    QTRY_VERIFY(popup->property("hasPendingChanges").toBool());
    QCOMPARE(plain(popup->property("selectedRendererCandidate")).toMap()
        .value(QStringLiteral("iconStyle")).toString(), QStringLiteral("metallic-blue"));
    QVERIFY(popup->property("studioError").toString().isEmpty());
    QVERIFY(nothingChanged());
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "discardStudioChanges"));
    QVERIFY(!popup->property("hasPendingChanges").toBool());

    // Duplicate, rename and delete reach only the user's preset store.
    const auto presetAction = [&](const QString &action, const QString &id, const QString &name) {
        return QMetaObject::invokeMethod(popup.get(), "performPresetAction",
            Q_ARG(QVariant, action), Q_ARG(QVariant, id), Q_ARG(QVariant, name));
    };
    QVERIFY(openPage(1, 7));
    QVERIFY(presetAction(QStringLiteral("duplicate"), QStringLiteral("circular-blue-ring"),
                         QStringLiteral("My Ring")));
    QVERIFY(!popup->property("presetNoticeIsError").toBool());
    QVERIFY(popup->property("presetNoticeText").toString().contains(QStringLiteral("My Panel Presets")));
    QCOMPARE(library->property("revision").toInt(), 1);
    QCOMPARE(cards().size(), 15);
    QVERIFY(QFileInfo(presetStore).isDir());
    QVERIFY(openPage(1, 8));
    QTRY_COMPARE(cards().size(), 1);
    const QString userId = cards().first().toMap().value(QStringLiteral("id")).toString();
    QVERIFY(userId.startsWith(QStringLiteral("user-")));
    QVERIFY(!cards().first().toMap().value(QStringLiteral("builtIn")).toBool());
    QCOMPARE(popup->property("presetNoticeText").toString(), QString{});
    popup->setProperty("selectedPresetId", userId);
    QTRY_VERIFY(findVisible(presetPreview));
    QVERIFY(presetAction(QStringLiteral("rename"), userId, QStringLiteral("Desk Ring")));
    QVERIFY(!popup->property("presetNoticeIsError").toBool());
    QTRY_COMPARE(cards().first().toMap().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Desk Ring"));
    QCOMPARE(popup->property("selectedPresetId").toString(), userId);
    // A built-in cannot be renamed or deleted, and the refusal is reported.
    QVERIFY(presetAction(QStringLiteral("rename"), QStringLiteral("circular-blue-ring"),
                         QStringLiteral("Mine")));
    QVERIFY(popup->property("presetNoticeIsError").toBool());
    QVERIFY(popup->property("presetNoticeText").toString().contains(QStringLiteral("not-user-preset")));
    QCOMPARE(library->property("revision").toInt(), 2);
    QVERIFY(presetAction(QStringLiteral("remove"), userId, QString{}));
    QVERIFY(!popup->property("presetNoticeIsError").toBool());
    QTRY_COMPARE(cards().size(), 0);
    QCOMPARE(popup->property("selectedPresetId").toString(), QString{});
    QVERIFY(!findVisible(presetPreview));
    QVERIFY(openPage(1, 7));
    QCOMPARE(cards().size(), 15);

    QVERIFY(nothingChanged());
    QVERIFY2(studioWarnings.isEmpty(), qPrintable(studioWarnings.join(QLatin1Char('\n'))));
    studio->close();
}

void PanelWindowCapabilityTest::presetAuditionGuardsAndInvalidRequestsLeaveNoWrites()
{
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    auto *session = qobject_cast<ArchDock::PresetPreviewSession *>(
        engine.rootContext()->contextProperty(QStringLiteral("presetAudition")).value<QObject *>());
    QVERIFY(session);
    const auto before = settingsSnapshot();
    const QVariantMap request{{QStringLiteral("kind"), QStringLiteral("panel")},
        {QStringLiteral("presetId"), QStringLiteral("obsidian-glass-dock")},
        {QStringLiteral("panelId"), QStringLiteral("bottom")}};
    const QList<QPair<QString, QString>> guards{{QStringLiteral("editMode"), QStringLiteral("edit-mode-active")},
        {QStringLiteral("popupOpen"), QStringLiteral("popup-open")}, {QStringLiteral("dragActive"), QStringLiteral("drag-active")}};
    for (const auto &guard : guards)
    {
        QVERIFY(window.reportPanelInteractionGuards(QStringLiteral("bottom"), {{guard.first, true}}));
        const auto result = session->beginPreview(request);
        QVERIFY(!result.value(QStringLiteral("success")).toBool());
        QCOMPARE(result.value(QStringLiteral("errorCode")).toString(), guard.second);
        QCOMPARE(session->state(), QStringLiteral("IDLE"));
        QCOMPARE(settingsSnapshot(), before);
    }
    QVERIFY(window.reportPanelInteractionGuards(QStringLiteral("bottom"), {}));
    auto invalid = request;
    invalid.insert(QStringLiteral("ownerToken"), QStringLiteral("forged"));
    QCOMPARE(session->beginPreview(invalid).value(QStringLiteral("errorCode")).toString(),
        QStringLiteral("invalid-preview-request"));
    invalid = request;
    invalid.insert(QStringLiteral("presetId"), QStringLiteral("../outside"));
    QCOMPARE(session->beginPreview(invalid).value(QStringLiteral("errorCode")).toString(), QStringLiteral("preset-not-found"));
    QCOMPARE(session->state(), QStringLiteral("IDLE"));
    QVERIFY(!session->saveAsCustomPreset(QStringLiteral("No draft")).value(QStringLiteral("success")).toBool());
    QCOMPARE(settingsSnapshot(), before);
    QVERIFY(session->cancel().value(QStringLiteral("success")).toBool());
}

void PanelWindowCapabilityTest::presetDefaultsDoNotRewriteExistingInstances()
{
    QTemporaryDir data;
    QVERIFY(data.isValid());
    const QByteArray previousDataHome = qgetenv("XDG_DATA_HOME");
    const bool hadDataHome = qEnvironmentVariableIsSet("XDG_DATA_HOME");
    const auto restoreDataHome = qScopeGuard([&] {
        if (hadDataHome) qputenv("XDG_DATA_HOME", previousDataHome);
        else qunsetenv("XDG_DATA_HOME");
    });
    qputenv("XDG_DATA_HOME", data.path().toUtf8());
    QQmlApplicationEngine engine;
    PanelWindow window(engine);
    auto *session = qobject_cast<ArchDock::PresetPreviewSession *>(
        engine.rootContext()->contextProperty(QStringLiteral("presetAudition")).value<QObject *>());
    QVERIFY(session);
    const auto before = settingsSnapshot();
    const auto bottom = window.panelRendererConfiguration(QStringLiteral("bottom"));
    QVERIFY(session->setAsDefault(QStringLiteral("panel"), QStringLiteral("obsidian-glass-dock"), false)
        .value(QStringLiteral("success")).toBool());
    QVERIFY(session->setAsDefault(QStringLiteral("icon"), QStringLiteral("glass-tile"), false)
        .value(QStringLiteral("success")).toBool());
    const auto defaults = session->defaultSelection();
    QCOMPARE(defaults.value(QStringLiteral("panelPresetId")).toString(), QStringLiteral("obsidian-glass-dock"));
    QCOMPARE(defaults.value(QStringLiteral("iconPresetId")).toString(), QStringLiteral("glass-tile"));
    QCOMPARE(settingsSnapshot(), before);
    QCOMPARE(window.panelRendererConfiguration(QStringLiteral("bottom")), bottom);
    QVERIFY(session->cancel().value(QStringLiteral("success")).toBool());
    QCOMPARE(session->defaultSelection(), defaults);
    QVERIFY(session->setAsDefault(QStringLiteral("panel"), QStringLiteral("obsidian-glass-dock"), true)
        .value(QStringLiteral("success")).toBool());
    QCOMPARE(session->defaultSelection().value(QStringLiteral("panelPresetId")).toString(), QString{});
    QCOMPARE(session->defaultSelection().value(QStringLiteral("iconPresetId")).toString(), QStringLiteral("glass-tile"));
    QCOMPARE(settingsSnapshot(), before);
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
