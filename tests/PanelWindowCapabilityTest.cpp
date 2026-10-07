#include "model/PanelCapabilityResolver.h"
#include "RendererBuildConfig.h"
#include "model/PanelDefinition.h"
#include "model/PanelSettingsSchema.h"
#include "PanelRegistry.h"
#include "panel/PanelWindow.h"
#include "presets/PresetPreviewSession.h"
#include "themes/ThemePackage.h"
#include "ScreenIdentity.h"
#include "WindowModel.h"

#include <QDir>
#include <QDBusConnection>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
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
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QWheelEvent>
#include <QtMath>
#include <QtTest>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <functional>

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

QQuickItem *visibleItem(QQuickWindow *window, const QString &name)
{
    QList<QQuickItem *> pending{window->contentItem()};
    while (!pending.isEmpty()) {
        auto *item = pending.takeLast();
        if (item->isVisible() && item->objectName() == name) return item;
        pending.append(item->childItems());
    }
    return nullptr;
}

// ADREP-TASK-001 Studio truth matrix (tests/data/studio-truth-matrix.json).
QVariantMap truthMatrixFixture()
{
    QFile file(QTest::qFindTestData(QStringLiteral("data/studio-truth-matrix.json"),
                                    __FILE__, __LINE__));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
}

QString sourceText(const QString &relativePath)
{
    QFile file(QTest::qFindTestData(QStringLiteral("../") + relativePath, __FILE__, __LINE__));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString{};
}

bool sameValue(const QVariant &left, const QVariant &right)
{
    return QJsonValue::fromVariant(left) == QJsonValue::fromVariant(right);
}

// One panel drawn the way the applet draws it (tests/TruthMatrixPanel.qml),
// grabbed once it has come to rest.
class TruthFrame
{
public:
    TruthFrame(QQmlEngine &engine, bool freeSurface, bool vertical, const QVariantMap &configuration)
    {
        QQmlComponent component(&engine, QUrl::fromLocalFile(QTest::qFindTestData(
            QStringLiteral("TruthMatrixPanel.qml"), __FILE__, __LINE__)));
        if (!component.isReady()) {
            m_error = component.errorString();
            return;
        }
        m_window = std::make_unique<QQuickWindow>();
        m_window->setColor(QColor(QStringLiteral("#101418")));
        // Entries are created with the configuration they draw, as in the
        // applet, whose delegates require their values when they are made.
        m_item.reset(qobject_cast<QQuickItem *>(component.createWithInitialProperties(
            {{QStringLiteral("freeSurface"), freeSurface}, {QStringLiteral("vertical"), vertical},
             {QStringLiteral("configuration"), configuration}})));
        if (!m_item) {
            m_error = component.errorString();
            return;
        }
        // A host of fixed size, the panel's own with room to grow, with the
        // scene centred in it: an icon growing under the pointer stays under
        // it, as on the desktop.
        m_item->setParentItem(m_window->contentItem());
        const QSize host(qCeil(m_item->width()) + 240, qCeil(m_item->height()) + 200);
        m_item->setProperty("hostWidth", host.width());
        m_item->setProperty("hostHeight", host.height());
        m_window->resize(host);
        m_window->show();
        if (!QTest::qWaitForWindowExposed(m_window.get()))
            m_error = QStringLiteral("the harness window was never exposed");
    }

    QString error() const { return m_error; }
    QQuickItem *item() const { return m_item.get(); }
    // Qt Quick's software renderer draws no shader effects.
    bool softwareRenderer() const
    {
        return m_window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software;
    }

    // The resting frame, with the pointer away or over one entry, moved there
    // as a person moves it. It is drawn as for a person with reduced motion
    // and without tooltips, so that every frame comes to rest; motion and
    // tooltips are proven by their own runtime properties. A frame that still
    // equals the reference is given time for a late paint: the procedural
    // surface paints on a worker thread.
    QImage draw(QVariantMap configuration, int hoveredIndex, const QImage &reference = {})
    {
        configuration.insert(QStringLiteral("reducedMotion"), true);
        configuration.insert(QStringLiteral("showTooltips"), false);
        m_item->setProperty("configuration", configuration);
        QImage last;
        QElapsedTimer timer;
        timer.start();
        // The pointer is aimed once and held there, as a hand holds it while
        // the icon under it grows.
        QPoint pointer(1, 1);
        bool aimed = hoveredIndex < 0;
        QElapsedTimer late;
        while (timer.elapsed() < 4000) {
            (void)QQuickTest::qWaitForPolish(m_window.get());
            if (!aimed) {
                QVariant point;
                QMetaObject::invokeMethod(m_item.get(), "entryPoint", Q_RETURN_ARG(QVariant, point),
                                          Q_ARG(QVariant, hoveredIndex));
                pointer = point.toPointF().toPoint();
                aimed = true;
            }
            QTest::mouseMove(m_window.get(), pointer);
            QTest::qWait(25);
            const QImage frame = m_window->grabWindow();
            if (!last.isNull() && frame == last) {
                if (reference.isNull() || frame != reference)
                    return frame;
                if (!late.isValid())
                    late.start();
                if (late.elapsed() > 800)
                    return frame;
                continue;
            }
            m_unsettled = {last, frame};
            last = frame;
        }
        return {};
    }

    // The last two different frames of a draw that never came to rest.
    QPair<QImage, QImage> unsettled() const { return m_unsettled; }

    // A frame every later draw is compared with: drawn until two draws agree,
    // so that no late paint is mistaken for a change.
    QImage reference(const QVariantMap &configuration, int hoveredIndex)
    {
        QImage previous = draw(configuration, hoveredIndex);
        for (int attempt = 0; attempt < 3 && !previous.isNull(); ++attempt) {
            const QImage next = draw(configuration, hoveredIndex, previous);
            if (next == previous)
                return next;
            previous = next;
        }
        return {};
    }

private:
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<QQuickItem> m_item;
    QString m_error;
    QPair<QImage, QImage> m_unsettled;
};

// A different valid value for a shown field: the fixture's, or one its
// control offers.
QVariant truthChangedValue(const QVariantMap &field, const QVariant &current, const QVariantMap &effect)
{
    const QStringList choices = field.value(QStringLiteral("choices")).toStringList();
    if (effect.contains(QStringLiteral("value"))) {
        const QVariant value = sameValue(effect.value(QStringLiteral("value")), current)
            ? effect.value(QStringLiteral("alternate")) : effect.value(QStringLiteral("value"));
        // A choice this panel does not offer gives way to one it does.
        if (choices.isEmpty() || choices.contains(value.toString()))
            return value;
    }
    const QString type = field.value(QStringLiteral("type")).toString();
    const QString control = field.value(QStringLiteral("control")).toString();
    if (type == QStringLiteral("boolean") || control == QStringLiteral("switch"))
        return !current.toBool();
    if (!choices.isEmpty()) {
        for (const QString &choice : choices)
            if (choice != current.toString())
                return choice;
        return {};
    }
    if (control == QStringLiteral("color"))
        return current.toString().compare(QStringLiteral("#c83c3c"), Qt::CaseInsensitive) == 0
            ? QStringLiteral("#3cc85a") : QStringLiteral("#c83c3c");
    const QVariant minimum = field.value(QStringLiteral("minimumValue"));
    const QVariant maximum = field.value(QStringLiteral("maximumValue"));
    const double low = minimum.isValid() ? minimum.toDouble() : current.toDouble() - 100;
    const double high = maximum.isValid() ? maximum.toDouble() : current.toDouble() + 100;
    const double delta = effect.contains(QStringLiteral("delta"))
        ? effect.value(QStringLiteral("delta")).toDouble() : (high - low) / 4;
    double next = current.toDouble() + delta;
    if (next > high)
        next = current.toDouble() - delta;
    next = qBound(low, next, high);
    if (type == QStringLiteral("integer"))
        return int(qRound(next));
    return next;
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
    void studioPanelMotionControls();
    void tiltEditorsFollowTheSelectedRenderer();
    void themeCardsResolveEachThemesOwnRenderer();
    void sceneEditAuditionsOnlyAFree3DPanel();
    void settingsLatencyOnTheOwners3DPanel();
    void studioIconTiles_data();
    void studioIconTiles();
    void studioFolderItemNames_data();
    void studioFolderItemNames();
    void studioPlainSurfaceExplains3D();
    void wholePanelRotationFieldsAreGatedByTheResolver();
    void meshSceneEditorIsGatedAndTransactional();
    void rendererSwitchRetainsOnlyUnchangedInactiveFields();
    void groupedWindowsFollowLiveKWinUpdates();
    void desktopLaunchIsBoundToTheSelectedPanelEntry();
    void folderRequestsValidatePanelAndChild();
    void studioPresetPagesBrowseWithoutChangingAnyPanel();
    void studioPresetListsMatchTheSelectedPanel();
    void profileServicePersistsAndHonorsPanelGuards();
    void presetAuditionGuardsAndInvalidRequestsLeaveNoWrites();
    void presetDefaultsDoNotRewriteExistingInstances();
    void screenSignalsCoalesceAndUtilityWindowFitsWorkArea();
    void closedStudioIsReleased();
    void studioArtworkPersistenceFailure_data();
    void studioArtworkPersistenceFailure();
    void ownersFreeCircleOffersOnlyWhatWorks();
    void studioTruthMatrixHarnessIsTheApplet();
    void studioTruthMatrix_data();
    void studioTruthMatrix();

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

void PanelWindowCapabilityTest::studioIconTiles_data()
{
    QTest::addColumn<bool>("native");
    QTest::newRow("native") << true;
    QTest::newRow("free") << false;
}

void PanelWindowCapabilityTest::studioFolderItemNames_data()
{
    QTest::addColumn<bool>("native");
    QTest::newRow("native") << true;
    QTest::newRow("free") << false;
}

void PanelWindowCapabilityTest::studioFolderItemNames()
{
    QFETCH(bool, native);
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const QString panel = native ? QStringLiteral("bottom") : registry->addFreePanel();
    // Folder settings are offered for content that can hold folders, the
    // launcher and hybrid content that take drops (ADREP-TASK-001).
    if (!native)
        QVERIFY(backend.applyPanelSettingsTransaction(panel,
            registry->panelDefinition(panel)->settingsRevision, {{"type", "launcher"}})
            .value("success").toBool());
    const auto before = registry->panelDefinition(panel)->toPersistedMap();
    QVERIFY(before.value("folderShowNames").toBool());
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    for (const bool apply : {false, true}) {
        std::unique_ptr<QObject> popup(component.createWithInitialProperties({
            {"selectedPanelId", panel}, {"mainTabIndex", 1}, {"subTabIndex", 3},
            {"width", 980}, {"height", 720}}));
        QVERIFY(popup);
        auto *window = qobject_cast<QQuickWindow *>(popup.get());
        QVERIFY(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY(QQuickTest::qWaitForPolish(window));
        auto *toggle = visibleItem(window, "studio-switch-folderShowNames");
        QVERIFY(toggle && toggle->isEnabled());
        toggle->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        QVERIFY(popup->property("hasSettingsChanges").toBool());
        const auto candidate = popup->property("selectedRendererCandidate").value<QJSValue>().toVariant().toMap();
        QVERIFY(!candidate.value("folderShowNames").toBool());
        QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
        auto *button = visibleItem(window, apply ? "studio-apply" : "studio-cancel");
        QVERIFY(button && button->isEnabled());
        button->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        if (!apply) {
            QTRY_VERIFY(!window->isVisible());
            QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
        }
        else {
            QVERIFY(QQuickTest::qWaitForPolish(window));
            QCOMPARE(popup->property("studioError").toString(), QString{});
            QVERIFY(!popup->property("hasSettingsChanges").toBool());
            PanelRegistry reloaded;
            QVERIFY(reloaded.panelDefinition(panel));
            QVERIFY(!reloaded.panelDefinition(panel)->content.folderShowNames);
            QVERIFY(!backend.panelRendererConfiguration(panel).value("folderShowNames").toBool());
        }
    }
}

void PanelWindowCapabilityTest::studioPlainSurfaceExplains3D()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const QString panel = registry->addFreePanel();
    const auto before = registry->panelDefinition(panel)->toPersistedMap();
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> popup(component.createWithInitialProperties({
        {"selectedPanelId", panel}, {"mainTabIndex", 1}, {"subTabIndex", 2}}));
    QVERIFY(popup);
    // A plain circular free panel can be drawn in 3D wherever the session has
    // a 3D renderer: the private native session does, offscreen runs do not.
    const bool session3D = qEnvironmentVariableIsSet("ARCHDOCK_NATIVE_UI_PROBE")
        && ARCHDOCK_QUICK3D_BUILT && ARCHDOCK_SCENE3D_BUILT;
    QTRY_COMPARE(popup->property("scene3DControlsAvailable").toBool(), session3D);
    // Appearance no longer sends people to the themes for 3D: it points to
    // the one 3D page where 3D works. Contract change, ADREP-TASK-001 (owner,
    // 2026-10-07: "If the tab is empty, there's no need for that tab"): a
    // session that cannot draw 3D has no 3D page, and Appearance says why.
    QVariant rows;
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "panelAppearanceRows", Q_RETURN_ARG(QVariant, rows)));
    const auto actionsOf = [](const QVariant &list) {
        QStringList result;
        for (const auto &row : list.toList())
            for (const auto &action : row.toMap().value("actions").toList())
                result.append(action.toMap().value("action").toString());
        return result;
    };
    const auto explains = [](const QVariant &list) {
        for (const auto &row : list.toList())
            if (row.toMap().value("text").toString().contains("unavailable in this session"))
                return true;
        return false;
    };
    QVariant offered;
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "subtabAvailable", Q_RETURN_ARG(QVariant, offered),
        Q_ARG(QVariant, 1), Q_ARG(QVariant, 10)));
    QCOMPARE(offered.toBool(), session3D);
    QCOMPARE(actionsOf(rows).contains("open-3d-page"), session3D);
    QVERIFY(!actionsOf(rows).contains("browse-3d-themes"));
    QCOMPARE(explains(rows), !session3D);
    if (session3D) {
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "performStudioAction",
            Q_ARG(QVariant, "open-3d-page"), Q_ARG(QVariant, QVariantMap{})));
        QCOMPARE(popup->property("subTabIndex").toInt(), 10);
        // With a 3D renderer the page offers the switch for this very panel.
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "panel3DRows", Q_RETURN_ARG(QVariant, rows)));
        bool switchOffered = false;
        for (const auto &row : rows.toList())
            switchOffered |= row.toMap().value("rendererToggle").toBool();
        QVERIFY(switchOffered);
        QVERIFY(!explains(rows));
    }
    QVERIFY(QMetaObject::invokeMethod(popup.get(), "performStudioAction",
        Q_ARG(QVariant, "browse-3d-themes"), Q_ARG(QVariant, QVariantMap{})));
    QCOMPARE(popup->property("subTabIndex").toInt(), 6);
    QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
    QVERIFY(!popup->property("hasPendingChanges").toBool());
}

void PanelWindowCapabilityTest::studioIconTiles()
{
    QFETCH(bool, native);
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const QString panel = native ? QStringLiteral("bottom") : registry->addFreePanel();
    const auto before = registry->panelDefinition(panel)->toPersistedMap();
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const auto open = [&] {
        return std::unique_ptr<QObject>(component.createWithInitialProperties({
            {"selectedPanelId", panel}, {"mainTabIndex", 3}, {"width", 980}, {"height", 720}}));
    };
    const auto item = [](QObject *popup, const QString &name) -> QQuickItem * {
        // Repeater delegates belong to the visual tree, which can differ from
        // QObject ownership. Walk the displayed controls like the wheel probe.
        return visibleItem(qobject_cast<QQuickWindow *>(popup), name);
    };
    const auto edit = [&](QObject *popup) {
        auto *window = qobject_cast<QQuickWindow *>(popup);
        window->show();
        if (!QTest::qWaitForWindowExposed(window) || !QQuickTest::qWaitForPolish(window)) return false;
        auto *toggle = item(popup, QStringLiteral("studio-switch-iconTilesEnabled"));
        if (!toggle || !toggle->isVisible()) {
            qWarning() << "Tile toggle missing or hidden:" << toggle
                << "section:" << popup->property("mainTabIndex")
                << "editor error:" << popup->property("studioError");
            return false;
        }
        QTest::mouseClick(window, Qt::LeftButton, {}, toggle->mapToScene(QPointF(toggle->width()/2, toggle->height()/2)).toPoint());
        if (!QQuickTest::qWaitForPolish(window)) return false;
        // Editing rebuilds the form delegates; reacquire the next control.
        auto *mode = item(popup, QStringLiteral("studio-combo-iconTileMode"));
        if (!mode || !mode->isVisible()) {
            qWarning() << "Tile mode missing or hidden:" << mode;
            return false;
        }
        mode->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Down);
        return QQuickTest::qWaitForPolish(window);
    };
    const auto click = [&](QObject *popup, const char *name) {
        auto *button = item(popup, QLatin1String(name));
        if (!button || !button->isVisible() || !button->isEnabled()) return false;
        QTest::mouseClick(qobject_cast<QQuickWindow *>(popup), Qt::LeftButton, {},
            button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
        return true;
    };
    auto popup = open(); QVERIFY2(popup != nullptr, qPrintable(component.errorString()));
    QVERIFY(edit(popup.get()));
    QVERIFY(popup->property("hasSettingsChanges").toBool());
    const auto candidate = popup->property("selectedRendererCandidate").value<QJSValue>().toVariant().toMap();
    QCOMPARE(candidate.value("iconTileMode").toString(), QStringLiteral("custom"));
    QVERIFY(!candidate.value("iconTilesEnabled").toBool());
    QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
    QVERIFY(click(popup.get(), "studio-cancel"));
    QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
    popup.reset();
    popup = open(); QVERIFY(popup);
    QVERIFY(edit(popup.get()));
    QVERIFY(click(popup.get(), "studio-apply"));
    QCOMPARE(popup->property("studioError").toString(), QString{});
    QVERIFY(!popup->property("hasSettingsChanges").toBool());
    PanelRegistry reloaded;
    const auto saved = reloaded.panelDefinition(panel);
    QVERIFY(saved);
    QCOMPARE(saved->iconStyle.tileMode, QStringLiteral("custom"));
    QVERIFY(!saved->iconStyle.tilesEnabled);
    QCOMPARE(saved->settingsRevision, before.value("settingsRevision").toULongLong() + 1);
}

void PanelWindowCapabilityTest::studioPanelMotionControls()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const QString panel = registry->addFreePanel();
    QVERIFY(backend.applyPanelSettingsTransaction(panel,
        registry->panelDefinition(panel)->settingsRevision,
        {{"type", "launcher"}, {"layout", "ring"}, {"x", 340}, {"y", 220}}).value("success").toBool());
    const auto before = registry->panelDefinition(panel)->toPersistedMap();
    const auto freeFields = fieldKeys(backend.panelSettingsEditorSnapshot(panel, "studio").value("panelFields").toList());
    for (const QString &key : {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("panelRotationMode")})
        QVERIFY2(freeFields.contains(key), qPrintable(key));
    // Contract change, ADREP-TASK-001 (PD-01): a free panel has no opening or
    // closing mechanism, so none of its settings is offered there.
    for (const QString &key : {QStringLiteral("presentationMode"), QStringLiteral("openDelay"),
         QStringLiteral("closeDelay"), QStringLiteral("collapseMechanism")})
        QVERIFY2(!freeFields.contains(key), qPrintable(key));
    // A curved free panel opens its folders along its own curve by default,
    // and offers that first; native panels keep the five popup layouts.
    QCOMPARE(registry->panelValue(panel, QStringLiteral("folderLayout")).toString(), QStringLiteral("track"));
    const QVariantMap freeFolder = fieldByKey(backend.panelSettingsEditorSnapshot(panel, "studio")
        .value("panelFields").toList(), QStringLiteral("folderLayout"));
    QCOMPARE(freeFolder.value("choices").toStringList(),
             QStringList({QStringLiteral("track"), QStringLiteral("fan"), QStringLiteral("grid"),
                          QStringLiteral("stack"), QStringLiteral("arc"), QStringLiteral("ring")}));
    QCOMPARE(freeFolder.value("options").toList().constFirst().toMap().value("label").toString(),
             QStringLiteral("Along the dock"));
    const auto nativeFields = fieldKeys(backend.panelSettingsEditorSnapshot("bottom", "studio").value("panelFields").toList());
    QVERIFY(nativeFields.contains("presentationMode"));
    QVERIFY(!nativeFields.contains("x") && !nativeFields.contains("y"));
    const auto nativeBefore = registry->panelDefinition("bottom")->toPersistedMap();
    const auto refused = backend.applyPanelSettingsTransaction("bottom",
        registry->panelDefinition("bottom")->settingsRevision, {{"x", 340}});
    QCOMPARE(refused.value("errorCode").toString(), QStringLiteral("unavailable-panel-field"));
    QCOMPARE(registry->panelDefinition("bottom")->toPersistedMap(), nativeBefore);
    for (const auto &status : {backend.nativePanelPlacementStatus("bottom"), backend.nativePanelVisibilityStatus("bottom")}) {
        QCOMPARE(status.value("status").toString(), QStringLiteral("not-attempted"));
        QCOMPARE(status.value("errorCode").toString(), QString{});
        QVERIFY(!status.value("success").toBool());
    }
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const auto open = [&](const QString &panelId) {
        return std::unique_ptr<QObject>(component.createWithInitialProperties({
            {"selectedPanelId", panelId}, {"mainTabIndex", 1}, {"subTabIndex", 9},
            {"width", 980}, {"height", 720}}));
    };
    const auto item = [](QObject *popup, const QString &name) -> QQuickItem * {
        QList<QQuickItem *> pending{qobject_cast<QQuickWindow *>(popup)->contentItem()};
        while (!pending.isEmpty()) {
            auto *candidate = pending.takeLast();
            if (candidate->isVisible() && candidate->objectName() == name) return candidate;
            pending.append(candidate->childItems());
        }
        return nullptr;
    };
    const auto press = [&](QObject *popup, const QString &name) {
        auto *window = qobject_cast<QQuickWindow *>(popup);
        auto *button = item(popup, name);
        QVERIFY(button && button->isEnabled());
        QTest::mouseClick(window, Qt::LeftButton, {},
            button->mapToScene({button->width()/2, button->height()/2}).toPoint());
    };
    // The free panel: position and rotation are edited and kept or dropped.
    for (const bool apply : {false, true}) {
        auto popup = open(panel); QVERIFY(popup);
        auto *window = qobject_cast<QQuickWindow *>(popup.get());
        QVERIFY(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY(QQuickTest::qWaitForPolish(window));
        QVERIFY(!item(popup.get(), "studio-combo-presentationMode"));
        popup->setProperty("subTabIndex", 0);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        for (const QString &key : {QStringLiteral("x"), QStringLiteral("y")}) {
            auto *position = item(popup.get(), "studio-spin-" + key);
            QVERIFY(position);
            position->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Up);
            QVERIFY(QQuickTest::qWaitForPolish(window));
        }
        popup->setProperty("subTabIndex", 9);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        auto *rotation = item(popup.get(), "studio-combo-panelRotationMode");
        QVERIFY(rotation);
        rotation->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Down);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        QCOMPARE(popup->property("studioError").toString(), QString{});
        QCOMPARE(popup->property("selectedRendererCandidate").value<QJSValue>().toVariant().toMap()
            .value("panelRotationMode").toString(), QStringLiteral("clockwise"));
        QVERIFY(popup->property("hasSettingsChanges").toBool());
        QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
        press(popup.get(), apply ? "studio-apply" : "studio-cancel");
        if (apply) QVERIFY(QQuickTest::qWaitForPolish(window));
        else QTRY_VERIFY(!window->isVisible());
        QCOMPARE(popup->property("studioError").toString(), QString{});
        if (!apply) QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
    }
    PanelRegistry reloaded;
    const auto saved = reloaded.panelDefinition(panel);
    QVERIFY(saved);
    QCOMPARE(saved->placement.x, 341);
    QCOMPARE(saved->placement.y, 221);
    QCOMPARE(saved->presentation.mode, QStringLiteral("open"));
    QCOMPARE(saved->layout.rotationMode, QStringLiteral("clockwise"));
    // The edge panel keeps its opening and closing: the resting state and its
    // mechanism are edited on Animations, previewed, and kept on Apply.
    auto popup = open(QStringLiteral("bottom")); QVERIFY(popup);
    auto *window = qobject_cast<QQuickWindow *>(popup.get());
    QVERIFY(window);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(QQuickTest::qWaitForPolish(window));
    auto *resting = item(popup.get(), "studio-combo-presentationMode");
    QVERIFY(resting);
    resting->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Down);
    QVERIFY(QQuickTest::qWaitForPolish(window));
    QCOMPARE(popup->property("studioError").toString(), QString{});
    const auto candidate = popup->property("selectedRendererCandidate").value<QJSValue>().toVariant().toMap();
    QCOMPARE(candidate.value("presentationMode").toString(), QStringLiteral("collapsed"));
    QCOMPARE(candidate.value("collapseMechanism").toString(), QStringLiteral("collapse-horizontal"));
    auto *preview = item(popup.get(), "panel-studio-live-renderer-preview");
    QVERIFY(preview);
    QTRY_COMPARE(preview->property("collapseProgress").toDouble(), 1.0);
    press(popup.get(), "studio-apply");
    QVERIFY(QQuickTest::qWaitForPolish(window));
    QCOMPARE(popup->property("studioError").toString(), QString{});
    PanelRegistry edgeReloaded;
    QCOMPARE(edgeReloaded.panelDefinition("bottom")->presentation.mode, QStringLiteral("collapsed"));
    QCOMPARE(edgeReloaded.panelDefinition("bottom")->presentation.collapseMechanism,
             QStringLiteral("collapse-horizontal"));
}

void PanelWindowCapabilityTest::tiltEditorsFollowTheSelectedRenderer()
{
    QQmlApplicationEngine engine;
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const auto panel = registry->addFreePanel();
    auto fields = backend.panelSettingsEditorSnapshot(panel, "studio").value("panelFields").toList();
    QVERIFY(!fieldKeys(fields).contains("bakedTilt"));
    QVERIFY(!fieldKeys(fields).contains("scene3DCameraPitch"));
    const auto baked = registry->themeCandidate(panel, "ring-platform-blue", "complete");
    QVERIFY(baked.value("success").toBool());
    QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
        baked.value("values").toMap()).value("success").toBool());
    fields = backend.panelSettingsEditorSnapshot(panel, "studio").value("panelFields").toList();
    const auto tilt = fieldByKey(fields, "bakedTilt");
    QVERIFY(!tilt.isEmpty());
    QCOMPARE(tilt.value("minimumValue").toDouble(), -10.0);
    QCOMPARE(tilt.value("maximumValue").toDouble(), 10.0);
    QVERIFY(!fieldKeys(fields).contains("scene3DCameraPitch"));
    QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
        {{"bakedTilt", 8.0}}).value("success").toBool());
    QCOMPARE(registry->panelDefinition(panel)->surface.parameters2_5D.value("tilt").toDouble(), 8.0);
    const auto before = registry->panelDefinition(panel)->toPersistedMap();
    QCOMPARE(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
        {{"bakedTilt", 11.0}}).value("errorCode").toString(), QStringLiteral("tilt-out-of-range"));
    QCOMPARE(registry->panelDefinition(panel)->toPersistedMap(), before);
    const auto mesh = registry->themeCandidate(panel, "mesh-platform-cyan", "complete");
    QVERIFY(mesh.value("success").toBool());
    QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
        mesh.value("values").toMap()).value("success").toBool());
    fields = backend.panelSettingsEditorSnapshot(panel, "studio").value("panelFields").toList();
    const bool available = ARCHDOCK_QUICK3D_BUILT && ARCHDOCK_SCENE3D_BUILT;
    QCOMPARE(fieldKeys(fields).contains("scene3DCameraPitch"), available);
    QCOMPARE(fieldKeys(fields).contains("scene3DCameraYaw"), available);
    QCOMPARE(fieldKeys(fields).contains("scene3DThickness"), available);
    QCOMPARE(fieldKeys(fields).contains("scene3DIconElevation"), available);
    QVERIFY(!fieldKeys(fields).contains("bakedTilt"));
    if (available) {
        QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
            {{"scene3DCameraPitch", -35.0}, {"scene3DCameraYaw", -20.0},
             {"scene3DThickness", 1.6}, {"scene3DIconElevation", 0.5}}).value("success").toBool());
        QCOMPARE(backend.panelRendererConfiguration(panel).value("scene3DCameraPitch").toDouble(), -35.0);
        const auto parameters = registry->panelDefinition(panel)->surface.parameters3D;
        QCOMPARE(parameters.value("cameraYaw").toDouble(), -20.0);
        QCOMPARE(parameters.value("thickness").toDouble(), 1.6);
        QCOMPARE(parameters.value("iconElevation").toDouble(), 0.5);
    }
    const auto nativeBefore = registry->panelDefinition("bottom")->toPersistedMap();
    QVERIFY(!backend.applyPanelSettingsTransaction("bottom", registry->panelDefinition("bottom")->settingsRevision,
        {{"scene3DCameraPitch", 30.0}}).value("success").toBool());
    QCOMPARE(registry->panelDefinition("bottom")->toPersistedMap(), nativeBefore);
    PanelRegistry reloaded;
    QCOMPARE(reloaded.panelDefinition(panel)->surface.parameters2_5D.value("tilt").toDouble(), 8.0);
}

// AD3D-TASK-002: desktop 3D editing starts only for a free panel drawn in 3D,
// and the panel's gizmo cannot change anything outside such an edit.
void PanelWindowCapabilityTest::sceneEditAuditionsOnlyAFree3DPanel()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    auto *session = qobject_cast<ArchDock::PresetPreviewSession *>(
        engine.rootContext()->contextProperty(QStringLiteral("presetAudition")).value<QObject *>());
    QVERIFY(registry && session);
    const auto begin = [&](const QString &panelId) {
        return session->beginPreview({{"kind", "scene3d"}, {"panelId", panelId}});
    };
    QCOMPARE(begin("bottom").value("errorCode").toString(), QStringLiteral("scene-edit-needs-free-panel"));
    const QString panel = registry->addFreePanel();
    const auto blue = registry->themeCandidate(panel, "ring-platform-blue", "complete");
    QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
        blue.value("values").toMap()).value("success").toBool());
    QCOMPARE(begin(panel).value("errorCode").toString(), QStringLiteral("scene-edit-needs-3d"));
    QCOMPARE(session->beginPreview({{"kind", "scene3d"}, {"panelId", panel}, {"presetId", "x"}})
                 .value("errorCode").toString(), QStringLiteral("invalid-preview-request"));
    QVERIFY(!backend.panelRendererConfiguration(panel).value("sceneEditActive").toBool());
    QCOMPARE(backend.updateSceneEditDraft(panel, {{"scene3DRoll", 10.0}}).value("errorCode").toString(),
             QStringLiteral("scene-edit-not-active"));
    if (!(ARCHDOCK_QUICK3D_BUILT && ARCHDOCK_SCENE3D_BUILT))
        return;
    // Drawn in 3D, the panel passes preparation; this session has no Plasma
    // host to verify, which the private desktop matrix provides.
    QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
        {{"rendererTier", "true3d"}}).value("success").toBool());
    const auto started = begin(panel);
    QVERIFY(!started.value("success").toBool());
    const QString reason = started.value("errorCode").toString();
    QVERIFY2(!reason.startsWith("scene-edit") && reason != "invalid-preview-request"
             && reason != "panel-not-found", qPrintable(reason));
    QCOMPARE(session->state(), QStringLiteral("IDLE"));
    QVERIFY(!backend.panelRendererConfiguration(panel).value("sceneEditActive").toBool());
}

// ADFIX UF-08: what the owner waits for while editing a 3D panel in Panel
// Studio. The owner's panel is Orange in its own 3D on a circle of radius 300
// tilted to 60 degrees. Each operation is timed several times; with
// ARCHDOCK_BENCHMARK_DIR set, p50 and p95 and the theme packages read from
// disk are written there as settings-latency.json.
void PanelWindowCapabilityTest::settingsLatencyOnTheOwners3DPanel()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const QString panel = registry->addFreePanel();
    const auto orange = registry->themeCandidate(panel, "arc-platform-orange", "complete");
    QVERIFY(orange.value("success").toBool());
    QVariantMap values = orange.value("values").toMap();
    values.insert("layout", "circular");
    values.insert("layoutRadius", 300);
    values.insert("rendererTier", "true3d");
    values.insert("scene3DCameraPitch", 60.0);
    QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
        values).value("success").toBool());

    struct Measured { QString name; QList<double> ms; double loads; };
    QList<Measured> results;
    const auto measure = [&](const QString &name, int count, const std::function<void(int)> &operation) {
        Measured result{name, {}, 0};
        const quint64 before = ArchDock::ThemePackage::loadCount();
        for (int index = 0; index < count; ++index) {
            QElapsedTimer timer;
            timer.start();
            operation(index);
            result.ms.append(timer.nsecsElapsed() / 1e6);
        }
        result.loads = double(ArchDock::ThemePackage::loadCount() - before) / count;
        std::sort(result.ms.begin(), result.ms.end());
        results.append(result);
        return result;
    };
    const auto percentile = [](const QList<double> &sorted, double share) {
        return sorted.value(qMin(sorted.size() - 1, int(std::ceil(share * sorted.size())) - 1));
    };
    // Warm up once: the first projection may load what later ones reuse.
    backend.resolvePanelSettingsEditorDraft(panel, registry->panelDefinition(panel)->settingsRevision,
        {{"spacing", 9}}, {}, "studio");
    // Every change in Studio re-projects the draft.
    const auto edit = measure("studio edit (draft projection)", 15, [&](int index) {
        QVERIFY(backend.resolvePanelSettingsEditorDraft(panel, registry->panelDefinition(panel)->settingsRevision,
            {{"spacing", 2 + index % 12}}, {}, "studio").value("success").toBool());
    });
    measure("studio page load (editor snapshot)", 10, [&](int) {
        QVERIFY(!backend.panelSettingsEditorSnapshot(panel, "studio").isEmpty());
    });
    measure("live panel renderer configuration", 10, [&](int) {
        QVERIFY(!backend.panelRendererConfiguration(panel).isEmpty());
    });
    measure("apply (settings transaction)", 10, [&](int index) {
        QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
            {{"spacing", 2 + index % 12}}).value("success").toBool());
    });
    QJsonArray rows;
    for (const auto &result : results) {
        const double p50 = percentile(result.ms, 0.5), p95 = percentile(result.ms, 0.95);
        qInfo().noquote() << QStringLiteral("SETTINGS LATENCY %1: p50 %2 ms, p95 %3 ms, %4 package reads each")
            .arg(result.name).arg(p50, 0, 'f', 1).arg(p95, 0, 'f', 1).arg(result.loads, 0, 'f', 1);
        rows.append(QJsonObject{{"operation", result.name}, {"p50Ms", p50}, {"p95Ms", p95},
                                {"samples", result.ms.size()}, {"packageReadsEach", result.loads}});
    }
    const QString directory = qEnvironmentVariable("ARCHDOCK_BENCHMARK_DIR");
    if (!directory.isEmpty()) {
        QVERIFY(QDir().mkpath(directory));
        QFile file(QDir(directory).filePath("settings-latency.json"));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(rows).toJson());
    }
    // The measured hot path: each Studio edit read the panel's theme package
    // from disk about 70 times (every asset hashed, the 3D mesh parsed). Once
    // read, a package is reused while its manifest is unchanged.
    for (const auto &result : results)
        QVERIFY2(result.loads == 0, qPrintable(QStringLiteral("%1 read %2 theme packages each")
            .arg(result.name).arg(result.loads)));
    Q_UNUSED(edit);
}

void PanelWindowCapabilityTest::themeCardsResolveEachThemesOwnRenderer()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    // The panel the broken cards were observed on wears the Blue Ring
    // platform, so its saved renderer tier is baked 2.5D.
    const QString panel = registry->addFreePanel();
    const auto platform = registry->themeCandidate(panel, "ring-platform-blue", "complete");
    QVERIFY(platform.value("success").toBool());
    QVERIFY(backend.applyPanelSettingsTransaction(panel, registry->panelDefinition(panel)->settingsRevision,
        platform.value("values").toMap()).value("success").toBool());
    QCOMPARE(registry->panelDefinition(panel)->surface.rendererTier, QStringLiteral("baked2.5d"));

    const auto themesFor = [&](const QString &panelId) {
        QHash<QString, QVariantMap> result;
        for (const auto &value : backend.resolvedThemeDefinitions(panelId))
            result.insert(value.toMap().value("id").toString(), value.toMap());
        return result;
    };
    const auto rendererOf = [](const QVariantMap &theme) {
        return theme.value("capabilityResolution").toMap().value("renderer").toMap();
    };
    const auto themes = themesFor(panel);
    QCOMPARE(themes.size(), 16);

    // Each skin is resolved with its own tier, not the platform's.
    for (const QString id : {"energy-frame-cyan", "energy-frame-green", "energy-frame-orange",
                             "energy-frame-purple", "sci-fi-chassis-dark", "sci-fi-chassis-red",
                             "sci-fi-chassis-blue"}) {
        const auto theme = themes.value(id);
        QVERIFY2(theme.value("available").toBool(), qPrintable(id));
        const auto renderer = rendererOf(theme);
        QCOMPARE(renderer.value("requestedTier").toString(), QStringLiteral("skinned2d"));
        QCOMPARE(renderer.value("effectiveTier").toString(), QStringLiteral("skinned2d"));
        QVERIFY2(!renderer.value("fallbackApplied").toBool(),
            qPrintable(id + ": " + renderer.value("reasonCode").toString()));
    }

    // The true-3D themes. The mesh platform asks for its mesh scene, and the
    // answer stays truthful in a build that has no 3D renderer.
    const bool spatial = ARCHDOCK_QUICK3D_BUILT && ARCHDOCK_SCENE3D_BUILT;
    QVERIFY(themes.value("mesh-platform-cyan").value("available").toBool());
    const auto mesh = rendererOf(themes.value("mesh-platform-cyan"));
    QCOMPARE(mesh.value("requestedTier").toString(), QStringLiteral("true3d"));
    QCOMPARE(mesh.value("effectiveTier").toString(),
        spatial ? QStringLiteral("true3d") : QStringLiteral("procedural2d"));
    QCOMPARE(mesh.value("fallbackApplied").toBool(), !spatial);
    if (!spatial)
        QVERIFY(mesh.value("reasonCode").toString().startsWith("renderer-"));
    const auto orange = rendererOf(themes.value("arc-platform-orange"));
    QCOMPARE(orange.value("effectiveTier").toString(), QStringLiteral("baked2.5d"));
    QVERIFY(!orange.value("fallbackApplied").toBool());

    // A theme whose style names no tier is listed again and resolves with
    // its own renderer instead of inheriting the platform's.
    for (const QString id : {"holographic-ring", "obsidian-glass", "neon-segments",
                             "metallic-shelf", "minimal-underline"}) {
        const auto theme = themes.value(id);
        QVERIFY2(theme.value("available").toBool(),
            qPrintable(id + ": " + theme.value("reasonCode").toString()));
        QCOMPARE(rendererOf(theme).value("effectiveTier").toString(), QStringLiteral("procedural2d"));
        QVERIFY2(!rendererOf(theme).value("fallbackApplied").toBool(), qPrintable(id));
    }

    // A card says what Load does: one resolution answers both.
    for (auto it = themes.cbegin(); it != themes.cend(); ++it) {
        const auto candidate = registry->themeCandidate(panel, it.key(), "complete");
        QVERIFY2(candidate.value("success").toBool() == it.value().value("available").toBool(),
            qPrintable(it.key() + ": " + candidate.value("errorCode").toString()));
        if (candidate.value("success").toBool())
            QCOMPARE(candidate.value("capabilityResolution").toMap().value("renderer").toMap(),
                rendererOf(it.value()));
    }

    // A genuine incompatibility is still reported: an edge panel cannot take
    // a free-only platform, and says why.
    const auto native = themesFor("bottom");
    QVERIFY(!native.value("ring-platform-blue").value("available").toBool());
    QCOMPARE(native.value("ring-platform-blue").value("reasonCode").toString(),
        QStringLiteral("host-layout-unsupported"));
    QVERIFY(native.value("energy-frame-cyan").value("available").toBool());
    QCOMPARE(rendererOf(native.value("energy-frame-cyan")).value("effectiveTier").toString(),
        QStringLiteral("skinned2d"));

    // The cards Panel Studio draws for the platform panel.
    {
        QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> popup(component.createWithInitialProperties({
            {"selectedPanelId", panel}, {"mainTabIndex", 1}, {"subTabIndex", 6},
            {"width", 980}, {"height", 720}}));
        QVERIFY(popup);
        auto *window = qobject_cast<QQuickWindow *>(popup.get());
        QVERIFY(window);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QVERIFY(QQuickTest::qWaitForPolish(window));
        for (auto it = themes.cbegin(); it != themes.cend(); ++it) {
            const QString id = it.key();
            QQuickItem *card = nullptr;
            QTRY_VERIFY2((card = visibleItem(window, "theme-live-preview-" + id)) != nullptr, qPrintable(id));
            const QString resolved = rendererOf(it.value()).value("effectiveTier").toString();
            // The offscreen consumer has no mesh backend; its truthful
            // fallback is covered by the renderer capability tests.
            if (resolved != QStringLiteral("true3d")) {
                QTRY_COMPARE(card->property("activeRendererTier").toString(), resolved);
                QVERIFY2(!card->property("fallbackApplied").toBool(),
                    qPrintable(id + ": " + card->property("rendererStatusText").toString()));
            }
            // Readable at card size: the preview is as tall as a preset
            // card's and the drawn icons are not specks.
            QVERIFY2(card->height() >= 110, qPrintable(id));
            auto *scene = card->property("panelSceneItem").value<QQuickItem *>();
            QVERIFY2(scene, qPrintable(id));
            const double iconPixels = scene->property("layoutGeometry").value<QJSValue>().toVariant()
                .toMap().value("iconSize").toDouble() * card->property("sceneFitScale").toDouble();
            QVERIFY2(iconPixels >= 14, qPrintable(id + ": " + QString::number(iconPixels)));
            if (id.startsWith("energy-frame-"))
                QVERIFY2(iconPixels >= 24, qPrintable(id + ": " + QString::number(iconPixels)));
        }
        QVERIFY(!popup->property("hasPendingChanges").toBool());
    }

    // Loading a theme with no tier of its own replaces the platform's tier.
    const auto ring = registry->themeCandidate(panel, "holographic-ring", "complete");
    QVERIFY2(ring.value("success").toBool(), qPrintable(ring.value("errorCode").toString()));
    QCOMPARE(ring.value("values").toMap().value("rendererTier").toString(), QStringLiteral("procedural2d"));
    const auto loaded = backend.applyPanelSettingsTransaction(panel,
        registry->panelDefinition(panel)->settingsRevision, ring.value("values").toMap());
    QVERIFY2(loaded.value("success").toBool(), qPrintable(loaded.value("errorCode").toString()));
    const auto applied = backend.panelRendererConfiguration(panel);
    QCOMPARE(applied.value("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
    QVERIFY(!applied.value("capabilityResolution").toMap().value("renderer").toMap()
        .value("fallbackApplied").toBool());
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
    // Panels > Behavior keeps enough rows to overflow at this size, and the
    // Panels tabs overflow its width; Appearance lost its Shape row (PD-04)
    // and Icons now has too few tabs to overflow (ADREP-TASK-001).
    std::unique_ptr<QObject> popup(component.createWithInitialProperties({
        {"selectedPanelId", panel}, {"mainTabIndex", 1}, {"subTabIndex", 3},
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
    auto *previousTabs = item("studio-tabs-previous");
    auto *nextTabs = item("studio-tabs-next");
    QVERIFY2(previousTabs && nextTabs, "Overflowing Studio tabs need visible navigation controls");
    // Keep simulated input separate from the compositor-driven probe.
    if (!native) {
        const auto click = [&](QQuickItem *button) {
            QTest::mouseClick(window, Qt::LeftButton, {},
                button->mapToScene({button->width() / 2, button->height() / 2}).toPoint());
            return QQuickTest::qWaitForPolish(window);
        };
        popup->setProperty("mainTabIndex", 0);
        popup->setProperty("subTabIndex", 0);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        QVERIFY(!previousTabs->isVisible() && !nextTabs->isVisible());
        popup->setProperty("mainTabIndex", 1);
        popup->setProperty("subTabIndex", 0);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        QVERIFY(previousTabs->isVisible() && nextTabs->isVisible());
        QVERIFY(!previousTabs->isEnabled() && nextTabs->isEnabled());
        nextTabs->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        QVERIFY(tabView->property("contentX").toDouble() > 0);
        QCOMPARE(popup->property("subTabIndex").toInt(), 0);
        for (int step = 0; nextTabs->isEnabled() && step < 20; ++step) {
            const double position = tabView->property("contentX").toDouble();
            QVERIFY(click(nextTabs));
            QVERIFY2(tabView->property("contentX").toDouble() > position,
                qPrintable(QStringLiteral("step=%1 before=%2 after=%3 button=(%4,%5) enabled=%6 view-width=%7 content-width=%8")
                    .arg(step).arg(position).arg(tabView->property("contentX").toDouble())
                    .arg(nextTabs->mapToScene({0, 0}).x()).arg(nextTabs->mapToScene({0, 0}).y())
                    .arg(nextTabs->isEnabled()).arg(tabView->width())
                    .arg(tabView->property("contentWidth").toDouble())));
        }
        QVERIFY(!nextTabs->isEnabled());
        QQuickItem *lastTab = nullptr;
        QVERIFY(QMetaObject::invokeMethod(tabs, "itemAt", Q_RETURN_ARG(QQuickItem *, lastTab), Q_ARG(int, 9)));
        QVERIFY(lastTab);
        const auto lastPosition = lastTab->mapToItem(tabView, {0, 0});
        QVERIFY(lastPosition.x() >= -1);
        QVERIFY(lastPosition.x() + lastTab->width() <= tabView->width() + 1);
        QVERIFY(click(lastTab));
        QCOMPARE(popup->property("subTabIndex").toInt(), 9);
        for (int step = 0; previousTabs->isEnabled() && step < 20; ++step)
            QVERIFY(click(previousTabs));
        QVERIFY(!previousTabs->isEnabled());
        QCOMPARE(popup->property("subTabIndex").toInt(), 9);
        popup->setProperty("mainTabIndex", 1);
        popup->setProperty("subTabIndex", 3);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        tabView->setProperty("contentX", 0);
    }
    if (native) {
        QVERIFY(inner->property("contentHeight").toDouble() > inner->height()
            || outer->property("contentHeight").toDouble() > outer->height());
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
                {"previousTabs", QVariantMap{{"point", point(previousTabs, {previousTabs->width()/2, previousTabs->height()/2})},
                    {"enabled", previousTabs->isEnabled()}}},
                {"nextTabs", QVariantMap{{"point", point(nextTabs, {nextTabs->width()/2, nextTabs->height()/2})},
                    {"enabled", nextTabs->isEnabled()}}},
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
    QCOMPARE(popup->property("subTabIndex").toInt(), 3);
    const double x = tabView->property("contentX").toDouble();
    wheel(tabs, {150, 15}, {-17, 0}, {});
    QCOMPARE(tabView->property("contentX").toDouble(), x + 17);
    wheel(tabs, {150, 15}, {0, -22}, {}, Qt::ShiftModifier);
    QCOMPARE(tabView->property("contentX").toDouble(), x + 39);
    QCOMPARE(popup->property("subTabIndex").toInt(), 3);
    primary->setProperty("contentY", 0);
    wheel(tabs, {150, 15}, {0, -30}, {});
    QCOMPARE(primary->property("contentY").toDouble(), 30.0);
    QCOMPARE(popup->property("subTabIndex").toInt(), 3);
    struct Selection { int section; int page; QString prefix; const char *property; };
    for (const auto &selection : {Selection{2, 0, "studio-spin-", "value"},
                                 Selection{1, 3, "studio-combo-", "currentIndex"}}) {
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

// Panel Studio is the heaviest thing the backend loads (its live preview,
// preset cards and their renderers). A closed Studio is destroyed rather than
// kept hidden: hidden, it held its whole scene in memory and its preview
// animations kept waking the backend. Opening it again builds a new one.
void PanelWindowCapabilityTest::closedStudioIsReleased()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    const auto findStudio = [] {
        QPointer<QWindow> found;
        for (QWindow *window : QGuiApplication::allWindows())
            if (window->title() == QStringLiteral("Arch Dock Panel Studio")) found = window;
        return found;
    };
    backend.showSettings();
    QPointer<QWindow> studio = findStudio();
    QVERIFY(studio);
    QTRY_VERIFY(studio->isVisible());
    studio->close();
    QTRY_VERIFY_WITH_TIMEOUT(studio.isNull(), 5000);
    QVERIFY(findStudio().isNull());

    backend.showSettings();
    QPointer<QWindow> reopened = findStudio();
    QVERIFY(reopened);
    QTRY_VERIFY(reopened->isVisible());
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
        {QStringLiteral("subTabIndex"), 10}}));
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
        popup->setProperty("subTabIndex", 10);
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
    if (available) {
        // The existing Orange artwork must remain usable while its 3D switch
        // offers a circular world-space platform without changing saved state.
        popup.reset();
        const auto orange = registry->themeCandidate(panelId, "arc-platform-orange", "complete");
        QVERIFY(orange.value("success").toBool());
        auto values = orange.value("values").toMap();
        values.insert("layout", "arc");
        QVERIFY(window.applyPanelSettingsTransaction(panelId,
            registry->panelDefinition(panelId)->settingsRevision, values, {}).value("success").toBool());
        const auto saved = registry->panelDefinition(panelId)->toPersistedMap();
        popup.reset(component.createWithInitialProperties({
            {"selectedPanelId", panelId}, {"mainTabIndex", 1}, {"subTabIndex", 10}}));
        QVERIFY2(popup != nullptr, qPrintable(component.errorString()));
        studio = qobject_cast<QQuickWindow *>(popup.get());
        QVERIFY(studio);
        studio->show();
        QVERIFY(QTest::qWaitForWindowExposed(studio));
        QTRY_VERIFY_WITH_TIMEOUT(rendererToggle(), 5000);
        QVERIFY(!rendererToggle()->property("checked").toBool());
        QTest::mouseClick(studio, Qt::LeftButton, Qt::NoModifier,
            rendererToggle()->mapToScene(QPointF(rendererToggle()->width()/2, rendererToggle()->height()/2)).toPoint());
        QTRY_VERIFY(popup->property("scene3DQualityVisible").toBool());
        QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), saved);
        QVERIFY(QQuickTest::qWaitForPolish(studio));
        QTRY_VERIFY(rendererToggle() && rendererToggle()->property("checked").toBool());
        QTest::mouseClick(studio, Qt::LeftButton, Qt::NoModifier,
            rendererToggle()->mapToScene(QPointF(rendererToggle()->width()/2, rendererToggle()->height()/2)).toPoint());
        QTRY_VERIFY2(!rendererToggle()->property("checked").toBool(),
            qPrintable(QStringLiteral("draft=%1 off=%2 error=%3 position=%4,%5")
                .arg(popup->property("selectedRendererCandidate").value<QJSValue>().toVariant().toMap().value("rendererTier").toString(),
                     popup->property("scene3DOffTier").toString(), popup->property("studioError").toString())
                .arg(rendererToggle()->mapToScene(QPointF(0, 0)).x())
                .arg(rendererToggle()->mapToScene(QPointF(0, 0)).y())));
        QTRY_VERIFY(!popup->property("scene3DQualityVisible").toBool());
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "cancelStudioChanges"));
        QCOMPARE(registry->panelDefinition(panelId)->toPersistedMap(), saved);

        // AD3D-TASK-002: a baked 2.5D panel is switched to 3D on the 3D page
        // and back, and keeps its own theme throughout.
        popup.reset();
        const auto blue = registry->themeCandidate(panelId, "ring-platform-blue", "complete");
        QVERIFY(blue.value("success").toBool());
        QVERIFY(window.applyPanelSettingsTransaction(panelId,
            registry->panelDefinition(panelId)->settingsRevision, blue.value("values").toMap(), {})
                .value("success").toBool());
        QCOMPARE(window.panelRendererConfiguration(panelId).value("effectiveRendererTier").toString(),
                 QStringLiteral("baked2.5d"));
        popup.reset(component.createWithInitialProperties({
            {"selectedPanelId", panelId}, {"mainTabIndex", 1}, {"subTabIndex", 10}}));
        QVERIFY2(popup != nullptr, qPrintable(component.errorString()));
        studio = qobject_cast<QQuickWindow *>(popup.get());
        QVERIFY(studio);
        studio->show();
        QVERIFY(QTest::qWaitForWindowExposed(studio));
        QTRY_VERIFY_WITH_TIMEOUT(rendererToggle(), 5000);
        QVERIFY(!rendererToggle()->property("checked").toBool());
        QVERIFY(QQuickTest::qWaitForPolish(studio));
        QTest::mouseClick(studio, Qt::LeftButton, Qt::NoModifier,
            rendererToggle()->mapToScene(QPointF(rendererToggle()->width() / 2,
                                                rendererToggle()->height() / 2)).toPoint());
        QTRY_VERIFY2(popup->property("scene3DQualityVisible").toBool(),
                     qPrintable(popup->property("studioError").toString()));
        // The page holds the whole 3D editor, and Reset returns the transform.
        QVariant rows;
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "panel3DRows", Q_RETURN_ARG(QVariant, rows)));
        QStringList keys, actions;
        for (const auto &row : rows.toList()) {
            keys.append(row.toMap().value("key").toString());
            for (const auto &action : row.toMap().value("actions").toList())
                actions.append(action.toMap().value("action").toString());
        }
        for (const QString &key : {"scene3DCameraPitch", "scene3DCameraYaw", "scene3DRoll",
                 "scene3DPositionX", "scene3DPositionY", "scene3DPositionZ", "scene3DScale",
                 "scene3DFieldOfView", "scene3DThickness", "scene3DIconElevation", "scene3DQuality",
                 "scene3DKeyLight", "scene3DFillLight", "scene3DTransitions", "scene3DFloat"})
            QVERIFY2(keys.contains(key), qPrintable(key));
        // ADFIX UF-06: the blue ring stands on a generated platform, so the
        // platform's width and its bend are offered.
        for (const QString &key : {"scene3DBand", "scene3DBend"})
            QVERIFY2(keys.contains(key), qPrintable(key));
        // Contract change, ADREP-TASK-001 (no setting on two pages, PD-08):
        // icon spacing is on Icons > Appearance and the Dock layout on Layout,
        // where the blue ring's ring and circle, which draw the same, are not
        // two choices.
        for (const QString &key : {"spacing", "layout"})
            QVERIFY2(!keys.contains(key), qPrintable(key));
        QVERIFY(actions.contains("reset-3d-transform"));
        // Desktop editing is offered from the same page.
        QVERIFY(actions.contains("edit-3d-on-desktop"));
        const QVariantMap rollField{{"key", "scene3DRoll"}, {"scope", "panel"}};
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "setFieldValue",
            Q_ARG(QVariant, rollField), Q_ARG(QVariant, 45.0)));
        QCOMPARE(popup->property("selectedRendererCandidate").value<QJSValue>().toVariant().toMap()
                     .value("scene3DRoll").toDouble(), 45.0);
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "performStudioAction",
            Q_ARG(QVariant, "reset-3d-transform"), Q_ARG(QVariant, QVariantMap{})));
        QCOMPARE(popup->property("selectedRendererCandidate").value<QJSValue>().toVariant().toMap()
                     .value("scene3DRoll").toDouble(), 0.0);
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "applyStudioChanges"));
        QTRY_COMPARE(window.panelRendererConfiguration(panelId).value("effectiveRendererTier").toString(),
                     QStringLiteral("true3d"));
        QCOMPARE(registry->panelValue(panelId, "completeThemeId").toString(), QStringLiteral("ring-platform-blue"));
        QVERIFY(window.panelRendererConfiguration(panelId).value("themeDefinition").toMap()
                    .value("genericScene3D").toBool());
        // Off returns the same theme in its own baked renderer.
        QVERIFY(QQuickTest::qWaitForPolish(studio));
        QTRY_VERIFY(rendererToggle() && rendererToggle()->property("checked").toBool());
        QTest::mouseClick(studio, Qt::LeftButton, Qt::NoModifier,
            rendererToggle()->mapToScene(QPointF(rendererToggle()->width() / 2,
                                                rendererToggle()->height() / 2)).toPoint());
        QTRY_VERIFY(!popup->property("scene3DQualityVisible").toBool());
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "applyStudioChanges"));
        QTRY_COMPARE(window.panelRendererConfiguration(panelId).value("effectiveRendererTier").toString(),
                     QStringLiteral("baked2.5d"));
        QCOMPARE(registry->panelValue(panelId, "completeThemeId").toString(), QStringLiteral("ring-platform-blue"));
        studio->close();
    }
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
    QCOMPARE(nativeKeys.size(), 9);
    QVERIFY(nativeKeys.contains(QStringLiteral("visible")));
    QVERIFY(nativeKeys.contains(QStringLiteral("visibilityMode")));
    QVERIFY(nativeKeys.contains(QStringLiteral("acceptDrops")));
    QVERIFY(nativeKeys.contains(QStringLiteral("iconStyle")));
    for (const QString &key : {QStringLiteral("folderLayout"), QStringLiteral("folderSpeed"),
         QStringLiteral("folderEasing"), QStringLiteral("folderExpandOnClick"),
         QStringLiteral("folderShowNames")})
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
    // Contract change, ADREP-TASK-001 (settings truth): an edge panel's applet
    // lays its row out from the panel's own orientation and padding, so the
    // record's Dock layout, Layout scale and Panel padding reach nothing there
    // and are not offered.
    for (const QString &noEffect : {QStringLiteral("layout"), QStringLiteral("layoutScale"),
                                    QStringLiteral("layoutPadding")})
        QVERIFY2(fieldByKey(studioFields, noEffect).isEmpty(), qPrintable(noEffect));
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
    // kind of non-working control the interface rules forbid. Opacity is not
    // one of them: every renderer applies it to the surface it draws.
    QVERIFY(!validKeys.contains(QStringLiteral("appearance")));
    QVERIFY(!validKeys.contains(QStringLiteral("shape")));
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
             QStringLiteral("color"),
             QStringLiteral("themeFit"),
             QStringLiteral("iconShape"),
             QStringLiteral("iconSize"),
             QStringLiteral("spacing")})
    {
        QVERIFY2(!invalidKeys.contains(unsupported), qPrintable(unsupported));
    }
    // Opacity depends on no theme capability: whatever surface is drawn in
    // place of the broken package still applies it.
    QVERIFY(invalidKeys.contains(QStringLiteral("opacity")));
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

    // ADREP-TASK-001: Studio loads a theme through panelThemeCandidate(),
    // which leaves out values for fields that do not act on the panel. An edge
    // panel's applet draws its own row, so the theme's layout is not set.
    const QVariantMap studioValues = window.panelThemeCandidate(
        QStringLiteral("bottom"), QStringLiteral("obsidian-glass")).value(QStringLiteral("values")).toMap();
    QVERIFY(!studioValues.contains(QStringLiteral("layout")));
    QCOMPARE(studioValues.value(QStringLiteral("completeThemeId")), values.value(QStringLiteral("completeThemeId")));
    const QVariantMap result = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"), revision, studioValues, {});
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

    // ADREP-TASK-001: Studio loads a theme through panelThemeCandidate(),
    // which leaves out values for fields that do not act on the panel. An edge
    // panel's applet draws its own row, so the theme's layout is not set.
    const QVariantMap studioValues = window.panelThemeCandidate(
        QStringLiteral("bottom"), QStringLiteral("sci-fi-chassis-red")).value(QStringLiteral("values")).toMap();
    QVERIFY(!studioValues.contains(QStringLiteral("layout")));
    QCOMPARE(studioValues.value(QStringLiteral("rendererTier")).toString(), QStringLiteral("skinned2d"));
    const QString layoutBefore = window.panelRendererConfiguration(QStringLiteral("bottom"))
        .value(QStringLiteral("layout")).toString();
    const QVariantMap result = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"), revision, studioValues, {});
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
    // The theme leaves the Dock layout, which an edge panel does not offer,
    // as it was. The applet draws the row along the bottom edge, the
    // horizontal layout this theme is made for: plasma-dock-widget sets an
    // edge panel's layout from its form factor.
    QCOMPARE(renderer.value(QStringLiteral("layout")).toString(), layoutBefore);
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

    // ADREP-TASK-001: Studio loads a theme through panelThemeCandidate(),
    // which leaves out values for fields that do not act on the panel. An edge
    // panel's applet draws its own row, so the theme's layout is not set.
    const QVariantMap studioValues = window.panelThemeCandidate(
        QStringLiteral("bottom"), QStringLiteral("energy-frame-cyan")).value(QStringLiteral("values")).toMap();
    QVERIFY(!studioValues.contains(QStringLiteral("layout")));
    QCOMPARE(studioValues.value(QStringLiteral("glowIntensity")), cyanValues.value(QStringLiteral("glowIntensity")));
    const QVariantMap result = window.applyPanelSettingsTransaction(
        QStringLiteral("bottom"), revision, studioValues, {});
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
    // Launch feedback is offered for content that launches applications
    // (ADREP-TASK-001, PD-07), so this panel holds launcher content.
    const auto applied = window.applyPanelSettingsTransaction(panelId, panel.settingsRevision,
        {{"type", "launcher"}, {"layout", "horizontal"},
         {"segments", QVariantList{panel.segments.first().toVariantMap()}},
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
    // The style definition is sent once per entry, not twice.
    QVERIFY(!resolvedFirst.value(QStringLiteral("iconOverrideResolution")).toMap()
                 .contains(QStringLiteral("iconStyleDefinition")));
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

    // The preset pages list two separate catalogs, and selecting a card
    // replaces the panel's preview with the preset's without applying it.
    // Contract change, ADREP-TASK-001: the icon preset pages are 2:4 and 2:5
    // since the Icon Styles tab was removed (PD-05); the bottom edge panel
    // lists the 11 presets made for horizontal edge panels (owner,
    // 2026-10-07: "if I make free panel why would I need to see horizontal
    // and vertical panel presets"); and a catalog of your own with nothing
    // in it has no tab (owner: "If the tab is empty, there's no need for that
    // tab").
    const auto offered = [&](int section, int subtab) {
        QVariant result;
        return QMetaObject::invokeMethod(popup.get(), "subtabAvailable",
                   Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, section),
                   Q_ARG(QVariant, subtab)) && result.toBool();
    };
    QVERIFY(!offered(1, 8));
    QVERIFY(!offered(2, 5));
    struct Page { int section; int subtab; QString kind; QString scope; int count; };
    for (const Page &page : {Page{1, 7, QStringLiteral("panel"), QStringLiteral("builtin"), 11},
                             Page{2, 4, QStringLiteral("icon"), QStringLiteral("builtin"), 15}})
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

    // Themes have a page of their own and are not presets. The icon style is
    // chosen in one place, the Icon style choice on Icons > Appearance; the
    // Icon Styles tab that repeated it is gone (PD-05).
    QVERIFY(openPage(1, 2));
    QVERIFY(!findVisible(QStringLiteral("theme-live-preview-obsidian-glass")));
    QVERIFY(openPage(1, 6));
    QVERIFY(plain(popup->property("currentPresetPage")).isNull());
    QTRY_VERIFY(findVisible(QStringLiteral("theme-live-preview-obsidian-glass")));
    QVERIFY(!findVisible(browser));
    QVERIFY(openPage(2, 0));
    QVERIFY(!plain(popup->property("currentSubtabs")).toStringList()
                 .contains(QStringLiteral("Icon Styles")));
    QTRY_VERIFY(findVisible(QStringLiteral("studio-combo-iconStyle")));
    QVERIFY(!findVisible(QStringLiteral("icon-style-live-preview-metallic-blue")));
    QVERIFY(!findVisible(QStringLiteral("theme-live-preview-obsidian-glass")));

    // Duplicate, rename and delete reach only the user's preset store.
    const auto presetAction = [&](const QString &action, const QString &id, const QString &name) {
        return QMetaObject::invokeMethod(popup.get(), "performPresetAction",
            Q_ARG(QVariant, action), Q_ARG(QVariant, id), Q_ARG(QVariant, name));
    };
    // The copy is of a preset made for this edge panel, so it is listed for
    // it, and its tab appears.
    QVERIFY(openPage(1, 7));
    QVERIFY(presetAction(QStringLiteral("duplicate"), QStringLiteral("obsidian-glass-dock"),
                         QStringLiteral("My Ring")));
    QVERIFY(!popup->property("presetNoticeIsError").toBool());
    QVERIFY(popup->property("presetNoticeText").toString().contains(QStringLiteral("My Panel Presets")));
    QCOMPARE(library->property("revision").toInt(), 1);
    QCOMPARE(cards().size(), 11);
    QVERIFY(QFileInfo(presetStore).isDir());
    QVERIFY(offered(1, 8));
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
    QVERIFY(presetAction(QStringLiteral("rename"), QStringLiteral("obsidian-glass-dock"),
                         QStringLiteral("Mine")));
    QVERIFY(popup->property("presetNoticeIsError").toBool());
    QVERIFY(popup->property("presetNoticeText").toString().contains(QStringLiteral("not-user-preset")));
    QCOMPARE(library->property("revision").toInt(), 2);
    QVERIFY(presetAction(QStringLiteral("remove"), userId, QString{}));
    QVERIFY(!popup->property("presetNoticeIsError").toBool());
    QTRY_COMPARE(cards().size(), 0);
    QCOMPARE(popup->property("selectedPresetId").toString(), QString{});
    QVERIFY(!findVisible(presetPreview));
    // The last preset of your own is gone, and so are its tab and page.
    QVERIFY(!offered(1, 8));
    QTRY_VERIFY(popup->property("subTabIndex").toInt() != 8);
    QVERIFY(openPage(1, 7));
    QCOMPARE(cards().size(), 11);

    QVERIFY(nothingChanged());
    QVERIFY2(studioWarnings.isEmpty(), qPrintable(studioWarnings.join(QLatin1Char('\n'))));
    studio->close();
}

void PanelWindowCapabilityTest::studioPresetListsMatchTheSelectedPanel()
{
    // ADREP-TASK-001, the owner (2026-10-07): "if I make free panel why would
    // I need to see horizontal and vertical panel presets". Built-in Panel
    // Presets lists the presets made for the selected panel: a free panel its
    // free-panel presets, an edge panel the edge presets of its orientation.
    // Icon presets set only icon values, which act on every panel, so every
    // panel lists all of them.
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml-imports"));
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QVERIFY(registry);
    const QString freePanel = registry->addFreePanel();
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const auto listed = [&](const QString &panel, int section, int subtab) {
        const std::unique_ptr<QObject> popup(component.createWithInitialProperties({
            {QStringLiteral("selectedPanelId"), panel}, {QStringLiteral("mainTabIndex"), section},
            {QStringLiteral("subTabIndex"), subtab}}));
        QStringList ids;
        if (!popup)
            return ids;
        const QVariant value = popup->property("presetCards");
        const QVariantList cards = value.metaType() == QMetaType::fromType<QJSValue>()
            ? value.value<QJSValue>().toVariant().toList() : value.toList();
        for (const QVariant &card : cards)
            ids.append(card.toMap().value(QStringLiteral("id")).toString());
        ids.sort();
        return ids;
    };
    QCOMPARE(listed(freePanel, 1, 7),
             QStringList({QStringLiteral("circular-blue-ring"), QStringLiteral("holographic-semicircle"),
                          QStringLiteral("octagonal-platform"), QStringLiteral("orange-arc-dock")}));
    QCOMPARE(listed(QStringLiteral("bottom"), 1, 7),
             QStringList({QStringLiteral("energy-frame-cyan"), QStringLiteral("energy-frame-green"),
                          QStringLiteral("energy-frame-orange"), QStringLiteral("energy-frame-purple"),
                          QStringLiteral("mechanical-collapsible-rail"), QStringLiteral("metallic-shelf-dock"),
                          QStringLiteral("minimal-neon-rail"), QStringLiteral("obsidian-glass-dock"),
                          QStringLiteral("sci-fi-chassis-blue"), QStringLiteral("sci-fi-chassis-dark"),
                          QStringLiteral("sci-fi-chassis-red")}));
    registry->setPanelValue(QStringLiteral("bottom"), QStringLiteral("edge"), QStringLiteral("left"));
    QCOMPARE(listed(QStringLiteral("bottom"), 1, 7),
             QStringList({QStringLiteral("minimal-neon-rail"), QStringLiteral("obsidian-glass-dock")}));
    const QStringList icons = listed(freePanel, 2, 4);
    QCOMPARE(icons.size(), 15);
    QCOMPARE(listed(QStringLiteral("bottom"), 2, 4), icons);
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

// ADREP-TASK-001 (OF-02 to OF-10, PD-01 to PD-08): the owner's free circular
// panel from the video audit of 2026-10-06 ("Free panel 13": a launcher circle
// whose Width was set to 700). Each Studio page it opens is read as drawn,
// and only controls that act on this panel may be offered, each on one page.
// With ARCHDOCK_STUDIO_CAPTURE_DIR set, every page is also saved as a capture.
void PanelWindowCapabilityTest::ownersFreeCircleOffersOnlyWhatWorks()
{
    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + "/qml-imports");
    PanelWindow backend(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty("panelRegistry").value<QObject *>());
    QVERIFY(registry);
    const QString panel = registry->addFreePanel();
    QVERIFY(backend.applyPanelSettingsTransaction(panel,
        registry->panelDefinition(panel)->settingsRevision,
        {{"type", "launcher"}, {"layout", "circular"}, {"appearance", "futuristic"},
         {"iconStyle", "dark-orb"}, {"panelRotationMode", "clockwise"}}).value("success").toBool());
    // Values the owner stored through controls that did nothing there: the
    // Width they typed and a shape Dark Orb's own layers ignore.
    registry->setPanelValue(panel, QStringLiteral("width"), 700);
    registry->setPanelValue(panel, QStringLiteral("iconShape"), QStringLiteral("squircle"));
    QCOMPARE(registry->panelValue(panel, QStringLiteral("width")).toInt(), 700);
    const QString captureDirectory = qEnvironmentVariable("ARCHDOCK_STUDIO_CAPTURE_DIR");
    QQmlComponent component(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    // A capture shows each whole page, so the window is tall enough for it.
    std::unique_ptr<QObject> popup(component.createWithInitialProperties({
        {"selectedPanelId", panel}, {"mainTabIndex", 1}, {"subTabIndex", 0},
        {"width", 1100}, {"height", captureDirectory.isEmpty() ? 820 : 2200}}));
    QVERIFY(popup);
    auto *window = qobject_cast<QQuickWindow *>(popup.get());
    QVERIFY(window);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    struct Page { int section; int subtab; QString name; };
    const QList<Page> pages{
        {1, 0, "panels-general"}, {1, 1, "panels-size"}, {1, 2, "panels-appearance"},
        {1, 3, "panels-behavior"}, {1, 4, "panels-layout"}, {1, 5, "panels-segments"},
        {1, 9, "panels-animations"}, {1, 10, "panels-3d"}, {2, 0, "icons-appearance"},
        {2, 1, "icons-behavior"}, {2, 2, "icons-indicators"}, {2, 3, "icons-notifications"},
        {2, 4, "icons-fifth-tab"}, {3, 0, "icon-tiles"}};
    QHash<QString, QVariantList> rowsByPage;
    QHash<QString, bool> offeredByPage;
    QHash<QString, QString> tabLabelByPage;
    for (const Page &page : pages) {
        popup->setProperty("mainTabIndex", page.section);
        popup->setProperty("subTabIndex", page.subtab);
        QVERIFY(QQuickTest::qWaitForPolish(window));
        QTest::qWait(30);
        QVariant rows;
        QVERIFY(QMetaObject::invokeMethod(popup.get(), "rowsForCurrentPage", Q_RETURN_ARG(QVariant, rows)));
        rowsByPage.insert(page.name, rows.toList());
        // A page is offered when its tab is shown; before ADREP-TASK-001
        // every tab was shown.
        QVariant offered = true;
        QMetaObject::invokeMethod(popup.get(), "subtabAvailable", Q_RETURN_ARG(QVariant, offered),
                                  Q_ARG(QVariant, page.section), Q_ARG(QVariant, page.subtab));
        offeredByPage.insert(page.name, offered.toBool());
        const QStringList labels = popup->property("currentSubtabs").value<QJSValue>()
            .toVariant().toStringList();
        tabLabelByPage.insert(page.name, page.subtab < labels.size() ? labels.at(page.subtab) : QString{});
        if (!captureDirectory.isEmpty() && offered.toBool()) {
            const QString file = QDir(captureDirectory).filePath(
                QStringLiteral("%1-%2.png").arg(page.section * 100 + page.subtab, 4, 10, QChar('0'))
                    .arg(page.name));
            QVERIFY2(window->grabWindow().save(file), qPrintable(file));
        }
    }
    if (!captureDirectory.isEmpty()) {
        // The same pages as text: each row's kind, key, label and sentence.
        QJsonObject listing;
        for (const Page &page : pages) {
            QJsonArray rows;
            for (const QVariant &value : rowsByPage.value(page.name)) {
                const QVariantMap row = value.toMap();
                rows.append(QJsonObject{{"kind", row.value("kind").toString()},
                    {"key", row.value("key").toString()}, {"label", row.value("label").toString()},
                    {"text", row.value("description").toString() + row.value("text").toString()}});
            }
            listing.insert(page.name, QJsonObject{{"tab", tabLabelByPage.value(page.name)},
                {"offered", offeredByPage.value(page.name)}, {"rows", rows}});
        }
        QFile file(QDir(captureDirectory).filePath(QStringLiteral("pages.json")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(listing).toJson());
    }

    const auto controls = [&](const QString &page) {
        QStringList keys;
        if (!offeredByPage.value(page))
            return keys;
        for (const QVariant &value : rowsByPage.value(page)) {
            const QVariantMap row = value.toMap();
            const QString kind = row.value("kind").toString();
            if (!row.value("key").toString().isEmpty() && kind != "readonly"
                    && (!row.contains("available") || row.value("available").toBool()))
                keys.append(row.value("key").toString());
        }
        return keys;
    };
    const auto sections = [&](const QString &page) {
        QStringList labels;
        for (const QVariant &value : rowsByPage.value(page))
            if (value.toMap().value("kind").toString() == "section")
                labels.append(value.toMap().value("label").toString());
        return labels;
    };
    const QStringList presentation{"presentationMode", "presentationTrigger", "collapseMechanism",
        "collapseAxis", "revealHandle", "openDelay", "closeDelay"};
    const QStringList rotation{"panelRotationMode", "panelRotationSpeed", "panelRotationTrigger"};

    // OF-02: no Alignment (nor any other edge-panel placement) on General.
    for (const QString &key : {QStringLiteral("alignment"), QStringLiteral("edge")})
        QVERIFY2(!controls("panels-general").contains(key), qPrintable(key));
    QVERIFY(controls("panels-general").contains("x") && controls("panels-general").contains("y"));
    // OF-03, PD-02: no Width or Height that does nothing. A tab with nothing
    // to change is not shown (the owner, 2026-10-07: "If the tab is empty,
    // there's no need for that tab"), so the Layout page says what sets a
    // radius-driven circle's size.
    QVERIFY(!offeredByPage.value("panels-size"));
    bool sizeExplained = false;
    for (const QVariant &value : rowsByPage.value("panels-layout"))
        sizeExplained |= value.toMap().value("kind").toString() == "notice"
            && value.toMap().value("text").toString().contains("Radius");
    QVERIFY2(sizeExplained, "the Layout page does not say what sets this panel's size");
    // OF-05, PD-04: Appearance has no Shape.
    QVERIFY(!controls("panels-appearance").contains("shape"));
    // OF-06, OF-07, PD-01: Behavior has no Dynamic and no opening or closing.
    QVERIFY(!controls("panels-behavior").contains("dynamic"));
    QVERIFY(!controls("panels-behavior").contains("visibilityMode"));
    for (const QString &page : rowsByPage.keys())
        for (const QString &key : presentation)
            QVERIFY2(!controls(page).contains(key), qPrintable(page + ": " + key));
    QVERIFY2(!sections("panels-animations").contains("Opening and closing"),
             "Animations offers an opening and closing section on a free panel");
    // OF-04, PD-08: rotation only on Animations.
    for (const QString &key : rotation) {
        QVERIFY2(!controls("panels-layout").contains(key), qPrintable(key));
        QVERIFY2(controls("panels-animations").contains(key), qPrintable(key));
    }
    // OF-08, PD-06: no Indicators on a free panel.
    QVERIFY(!offeredByPage.value("icons-indicators"));
    for (const QString &page : rowsByPage.keys())
        QVERIFY2(!controls(page).contains("showIndicators"), qPrintable(page));
    // OF-09, PD-07: every notification item says in a sentence what it does.
    for (const QVariant &value : rowsByPage.value("icons-notifications")) {
        const QVariantMap row = value.toMap();
        if (!row.value("key").toString().isEmpty())
            QVERIFY2(row.value("description").toString().endsWith('.'),
                     qPrintable(row.value("key").toString()));
    }
    // OF-10, PD-05: one icon style selector, on Icons > Appearance.
    QVERIFY(!(offeredByPage.value("icons-fifth-tab")
              && tabLabelByPage.value("icons-fifth-tab") == QStringLiteral("Icon Styles")));
    QStringList styleSelectors;
    for (const QString &page : rowsByPage.keys())
        if (controls(page).contains("iconStyle"))
            styleSelectors.append(page);
    QCOMPARE(styleSelectors, QStringList{QStringLiteral("icons-appearance")});
    // No setting is offered on two pages.
    QHash<QString, QString> owner;
    for (const QString &page : rowsByPage.keys())
        for (const QString &key : controls(page)) {
            QVERIFY2(!owner.contains(key) || owner.value(key) == page,
                     qPrintable(key + " on " + owner.value(key) + " and " + page));
            owner.insert(key, page);
        }
}

void PanelWindowCapabilityTest::studioTruthMatrixHarnessIsTheApplet()
{
    // The truth matrix draws a panel through tests/TruthMatrixPanel.qml. Each
    // line of it marked "// applet" is the applet's own, and both build their
    // scene definition with SceneDefinition.js.
    const QString harness = sourceText(QStringLiteral("tests/TruthMatrixPanel.qml"));
    const QString applet = sourceText(QStringLiteral("plasma-dock-widget/contents/ui/main.qml"));
    QVERIFY(!harness.isEmpty() && !applet.isEmpty());
    QSet<QString> appletLines;
    for (const QString &line : applet.split(QLatin1Char('\n')))
        appletLines.insert(line.trimmed());
    int mirrored = 0;
    for (const QString &line : harness.split(QLatin1Char('\n'))) {
        const qsizetype tag = line.indexOf(QStringLiteral("// applet"));
        if (tag < 0)
            continue;
        const QString code = line.left(tag).trimmed();
        QVERIFY2(appletLines.contains(code), qPrintable(QStringLiteral("main.qml no longer has: ") + code));
        ++mirrored;
    }
    QVERIFY(mirrored > 60);
    for (const QString &source : {harness, applet})
        QVERIFY(source.contains(QStringLiteral("SceneDefinition.js\" as SceneDefinition")));
    QVERIFY(applet.contains(QStringLiteral("return SceneDefinition.build(configuration, {")));
}

void PanelWindowCapabilityTest::studioTruthMatrix_data()
{
    QTest::addColumn<QVariantMap>("combination");
    for (const QVariant &value : truthMatrixFixture().value(QStringLiteral("combinations")).toList()) {
        const QVariantMap combination = value.toMap();
        QTest::newRow(qPrintable(combination.value(QStringLiteral("id")).toString())) << combination;
    }
}

void PanelWindowCapabilityTest::studioTruthMatrix()
{
    QFETCH(QVariantMap, combination);
    const QVariantMap fixture = truthMatrixFixture();
    QVERIFY(!fixture.isEmpty());
    const QVariantMap effects = fixture.value(QStringLiteral("effects")).toMap();
    const bool free = combination.value(QStringLiteral("host")).toString() == QStringLiteral("free");
    const QString layout = combination.value(QStringLiteral("layout")).toString();
    const bool vertical = !free && QStringList{QStringLiteral("left"), QStringLiteral("right")}
        .contains(combination.value(QStringLiteral("edge")).toString());

    QQmlApplicationEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml-imports"));
    PanelWindow window(engine);
    auto *registry = qobject_cast<PanelRegistry *>(engine.rootContext()
        ->contextProperty(QStringLiteral("panelRegistry")).value<QObject *>());
    QVERIFY(registry);
    const QString panel = free ? registry->addFreePanel() : QStringLiteral("bottom");
    QVariantMap base = fixture.value(QStringLiteral("base")).toMap()
        .value(free ? QStringLiteral("free") : QStringLiteral("edge")).toMap();
    base.insert(combination.value(QStringLiteral("base")).toMap());
    if (free)
        base.insert(QStringLiteral("layout"), layout);
    else
        base.insert(QStringLiteral("edge"), combination.value(QStringLiteral("edge")));
    const QString theme = combination.value(QStringLiteral("theme")).toString();
    base.insert(QStringLiteral("rendererTier"), combination.value(QStringLiteral("tier")));
    base.insert(QStringLiteral("panelThemeId"), theme);
    base.insert(QStringLiteral("completeThemeId"), theme);
    QVERIFY2(registry->updatePanelChecked(panel, base), "the base state was refused");
    const QString tier = window.resolvePanelCapabilities(panel).value(QStringLiteral("renderer")).toMap()
        .value(QStringLiteral("effectiveTier")).toString();
    QVERIFY2(!tier.isEmpty(), "the panel resolves to no renderer");

    // What the backend offers, and what Studio shows on its pages.
    const QVariantMap snapshot = window.panelSettingsEditorSnapshot(panel, QStringLiteral("studio"));
    QVERIFY(snapshot.value(QStringLiteral("success")).toBool());
    QHash<QString, QVariantMap> offered;
    for (const QString &scope : {QStringLiteral("panelFields"), QStringLiteral("globalFields")})
        for (const QVariant &value : snapshot.value(scope).toList())
            offered.insert(value.toMap().value(QStringLiteral("key")).toString(), value.toMap());

    QQmlComponent studioComponent(&engine, QUrl::fromLocalFile(QFINDTESTDATA("../qml/runtime/SettingsPopup.qml")));
    QVERIFY2(studioComponent.isReady(), qPrintable(studioComponent.errorString()));
    std::unique_ptr<QObject> studio(studioComponent.createWithInitialProperties({
        {QStringLiteral("selectedPanelId"), panel}, {QStringLiteral("mainTabIndex"), 0},
        {QStringLiteral("subTabIndex"), 0}}));
    QVERIFY(studio);
    // Studio is read as a person sees it: shown, with its own preview drawn.
    // Its 3D settings appear once that preview draws the panel in 3D.
    auto *studioWindow = qobject_cast<QQuickWindow *>(studio.get());
    QVERIFY(studioWindow);
    studioWindow->show();
    QVERIFY(QTest::qWaitForWindowExposed(studioWindow));
    if (tier == QStringLiteral("true3d") && studio->property("scene3DControlsAvailable").toBool())
        QTRY_VERIFY2_WITH_TIMEOUT(studio->property("scene3DQualityVisible").toBool(),
                                  "Studio's preview never drew the 3D panel", 10000);
    QHash<QString, QString> pageOf;
    QStringList failures;
    const auto pageKeys = [&](int section, int subtab) {
        QStringList keys;
        QVariant isOffered;
        QMetaObject::invokeMethod(studio.get(), "subtabAvailable", Q_RETURN_ARG(QVariant, isOffered),
                                  Q_ARG(QVariant, section), Q_ARG(QVariant, subtab));
        if (!isOffered.toBool())
            return keys;
        studio->setProperty("mainTabIndex", section);
        studio->setProperty("subTabIndex", subtab);
        QVariant rows;
        QMetaObject::invokeMethod(studio.get(), "rowsForCurrentPage", Q_RETURN_ARG(QVariant, rows));
        for (const QVariant &value : rows.toList()) {
            const QVariantMap row = value.toMap();
            const QString key = row.value(QStringLiteral("key")).toString();
            if (!key.isEmpty() && row.value(QStringLiteral("kind")).toString() != QStringLiteral("readonly")
                    && (!row.contains(QStringLiteral("available")) || row.value(QStringLiteral("available")).toBool()))
                keys.append(key);
        }
        return keys;
    };
    for (int section = 1; section <= 3; ++section) {
        const int subtabs = section == 3 ? 1 : (section == 1 ? 11 : 6);
        for (int subtab = 0; subtab < subtabs; ++subtab) {
            const QString page = QStringLiteral("%1:%2").arg(section).arg(subtab);
            for (const QString &key : pageKeys(section, subtab)) {
                // (c) No setting is shown on two pages.
                if (pageOf.contains(key) && pageOf.value(key) != page)
                    failures.append(QStringLiteral("%1 is on %2 and %3").arg(key, pageOf.value(key), page));
                pageOf.insert(key, page);
            }
        }
    }
    studioWindow->hide();
    QStringList shown = pageOf.keys();
    shown.removeAll(QStringLiteral("rendererTier")); // the 3D page's own switch, checked below
    std::sort(shown.begin(), shown.end());
    for (const QString &key : std::as_const(shown)) {
        if (!offered.contains(key))
            failures.append(key + QStringLiteral(" is shown but the backend refuses it"));
        // A setting that draws nothing in the panel's present state is kept
        // by the backend and never shown.
        else if (offered.value(key).value(QStringLiteral("inactive")).toBool())
            failures.append(key + QStringLiteral(" is shown although it draws nothing here"));
    }
    // (c) Each setting named under homes is on its page.
    const QVariantMap homes = fixture.value(QStringLiteral("homes")).toMap();
    for (auto it = homes.cbegin(); it != homes.cend(); ++it)
        if (it.key() != QStringLiteral("why") && pageOf.contains(it.key()) && pageOf.value(it.key()) != it.value().toString())
            failures.append(QStringLiteral("%1 is on %2, not on %3").arg(it.key(), pageOf.value(it.key()), it.value().toString()));
    // (b) Nothing forbidden for this host, layout or renderer is offered.
    const QVariantMap forbidden = fixture.value(QStringLiteral("forbidden")).toMap();
    QStringList refused = forbidden.value(QStringLiteral("everywhere")).toMap().value(QStringLiteral("fields")).toStringList();
    refused += forbidden.value(free ? QStringLiteral("free") : QStringLiteral("edge")).toMap()
        .value(QStringLiteral("fields")).toStringList();
    refused += forbidden.value(QStringLiteral("layouts")).toMap().value(layout).toStringList();
    refused += forbidden.value(QStringLiteral("tiers")).toMap().value(tier).toStringList();
    for (const QString &key : std::as_const(refused))
        if (offered.contains(key) || pageOf.contains(key))
            failures.append(key + QStringLiteral(" is offered although it is forbidden here"));

    // (a) Every shown field changes the drawn panel or its documented runtime
    // property.
    const QVariantMap baseConfiguration = window.panelRendererConfiguration(panel);
    TruthFrame frame(engine, free, vertical, baseConfiguration);
    QVERIFY2(frame.error().isEmpty(), qPrintable(frame.error()));
    const QImage baseFrame = frame.reference(baseConfiguration, -1);
    QVERIFY2(!baseFrame.isNull(), "the panel never came to rest");
    QImage baseHoverFrame;
    QHash<QString, QString> sources;
    QJsonArray evidence;
    if (pageOf.contains(QStringLiteral("rendererTier")))
        shown.prepend(QStringLiteral("rendererTier"));
    quint64 revision = snapshot.value(QStringLiteral("revision")).toULongLong();
    for (const QString &key : std::as_const(shown)) {
        const QVariantMap effect = key == QStringLiteral("rendererTier")
            ? QVariantMap{{QStringLiteral("probe"), QStringLiteral("frame")}}
            : effects.value(key).toMap();
        const QString probe = effect.value(QStringLiteral("probe")).toString();
        if (probe.isEmpty()) {
            failures.append(key + QStringLiteral(" is shown with no documented effect"));
            continue;
        }
        // Each field is restored before the next, so the row's snapshot
        // holds every base value.
        const bool global = offered.value(key).value(QStringLiteral("scope")).toString() == QStringLiteral("global")
            || snapshot.value(QStringLiteral("globalValues")).toMap().contains(key);
        const QVariant original = key == QStringLiteral("rendererTier")
            ? QVariant(tier)
            : snapshot.value(global ? QStringLiteral("globalValues") : QStringLiteral("panelValues")).toMap().value(key);
        const QVariant value = key == QStringLiteral("rendererTier")
            ? (tier == QStringLiteral("true3d") ? studio->property("scene3DOffTier") : QVariant(QStringLiteral("true3d")))
            : truthChangedValue(offered.value(key), original, effect);
        const auto observe = [&]() -> QVariant {
            if (probe == QStringLiteral("runtime")) {
                return effect.value(QStringLiteral("source")).toString() == QStringLiteral("dock")
                    ? window.dockConfiguration(panel).value(key)
                    : window.panelRendererConfiguration(panel).value(key);
            }
            if (probe == QStringLiteral("placement"))
                return window.nativePanelPlacementStatus(panel).value(QStringLiteral("savedIntent")).toMap().value(key);
            if (probe == QStringLiteral("visibility"))
                return window.nativePanelVisibilityStatus(panel).value(QStringLiteral("requestedMode"));
            if (probe == QStringLiteral("conceal"))
                return window.shouldConcealPanel(panel);
            return {};
        };
        const QVariant before = observe();
        const auto apply = [&](const QVariant &next) {
            const QVariantMap result = window.applyPanelSettingsTransaction(panel, revision,
                global ? QVariantMap{} : QVariantMap{{key, next}},
                global ? QVariantMap{{key, next}} : QVariantMap{});
            if (result.value(QStringLiteral("success")).toBool())
                revision = result.value(QStringLiteral("revision")).toULongLong();
            return result;
        };
        const QVariantMap applied = apply(value);
        if (!applied.value(QStringLiteral("success")).toBool()) {
            failures.append(QStringLiteral("%1 = %2 could not be applied: %3 %4").arg(key,
                QString::fromUtf8(QJsonDocument(QJsonArray{QJsonValue::fromVariant(value)}).toJson(QJsonDocument::Compact)),
                applied.value(QStringLiteral("errorCode")).toString(), applied.value(QStringLiteral("errorMessage")).toString()));
            continue;
        }
        bool changed = false;
        bool deferred = false;
        QString detail;
        if (probe == QStringLiteral("frame") || probe == QStringLiteral("hoverFrame")) {
            const int hovered = probe == QStringLiteral("hoverFrame") ? 1 : -1;
            if (hovered >= 0 && baseHoverFrame.isNull())
                baseHoverFrame = frame.reference(baseConfiguration, hovered);
            const QImage after = frame.draw(window.panelRendererConfiguration(panel), hovered,
                                            hovered >= 0 ? baseHoverFrame : baseFrame);
            changed = !after.isNull() && after != (hovered >= 0 ? baseHoverFrame : baseFrame);
            const QString diagnostics = qEnvironmentVariable("ARCHDOCK_TRUTH_MATRIX_OUTPUT");
            if (!changed && !diagnostics.isEmpty()) {
                const QString stem = QDir(diagnostics).filePath(
                    combination.value(QStringLiteral("id")).toString() + QLatin1Char('-') + key);
                if (after.isNull()) {
                    frame.unsettled().first.save(stem + QStringLiteral("-unsettled-a.png"));
                    frame.unsettled().second.save(stem + QStringLiteral("-unsettled-b.png"));
                } else {
                    (hovered >= 0 ? baseHoverFrame : baseFrame).save(stem + QStringLiteral("-before.png"));
                    after.save(stem + QStringLiteral("-after.png"));
                }
            }
            detail = after.isNull() ? QStringLiteral("never came to rest")
                : QStringLiteral("%1x%2").arg(after.width()).arg(after.height());
            // A shader-drawn effect cannot appear under the software
            // renderer; studio-truth-matrix-smoke requires it with the real
            // graphics backend.
            if (!changed && !after.isNull() && frame.softwareRenderer()
                    && effect.value(QStringLiteral("needsShaders")).toStringList().contains(tier)) {
                deferred = true;
                detail += QStringLiteral(" drawn by a shader: proved by studio-truth-matrix-smoke");
            }
        } else {
            const QVariant after = observe();
            changed = !sameValue(before, after);
            detail = QString::fromUtf8(QJsonDocument(QJsonArray{QJsonValue::fromVariant(before),
                QJsonValue::fromVariant(after)}).toJson(QJsonDocument::Compact));
            for (const QVariant &consumer : effect.value(QStringLiteral("consumers")).toList()) {
                const QStringList pair = consumer.toStringList();
                if (!sources.contains(pair.value(0)))
                    sources.insert(pair.value(0), sourceText(pair.value(0)));
                if (!sources.value(pair.value(0)).contains(pair.value(1))) {
                    changed = false;
                    detail += QStringLiteral(" consumer missing in ") + pair.value(0);
                }
            }
        }
        if (!changed && !deferred)
            failures.append(QStringLiteral("%1 (%2) changes nothing: %3").arg(key, probe, detail));
        evidence.append(QJsonObject{{QStringLiteral("field"), key}, {QStringLiteral("page"), pageOf.value(key)},
            {QStringLiteral("probe"), probe}, {QStringLiteral("changed"), changed},
            {QStringLiteral("deferred"), deferred},
            {QStringLiteral("value"), QJsonValue::fromVariant(value)}, {QStringLiteral("detail"), detail},
            {QStringLiteral("proof"), effect.value(QStringLiteral("proof")).toString()}});
        // Back to the base state for the next field.
        const QVariantMap restored = apply(original);
        QVERIFY2(restored.value(QStringLiteral("success")).toBool(),
                 qPrintable(key + QStringLiteral(" could not be restored: ") + restored.value(QStringLiteral("errorCode")).toString()));
        // Back means the values Studio shows are the base ones again; a value
        // set back to its default may now be stored where it was absent.
        const QVariantMap restoredSnapshot = window.panelSettingsEditorSnapshot(panel, QStringLiteral("studio"));
        QVariantMap now = restoredSnapshot.value(QStringLiteral("panelValues")).toMap();
        now.insert(restoredSnapshot.value(QStringLiteral("globalValues")).toMap());
        QVariantMap then = snapshot.value(QStringLiteral("panelValues")).toMap();
        then.insert(snapshot.value(QStringLiteral("globalValues")).toMap());
        QStringList differing;
        for (const QString &name : QSet<QString>(now.keyBegin(), now.keyEnd()) + QSet<QString>(then.keyBegin(), then.keyEnd()))
            if (!sameValue(now.value(name), then.value(name)))
                differing.append(name);
        QVERIFY2(differing.isEmpty(), qPrintable(key + QStringLiteral(" did not restore the panel: ") + differing.join(QLatin1Char(' '))));
    }

    const QString output = qEnvironmentVariable("ARCHDOCK_TRUTH_MATRIX_OUTPUT");
    if (!output.isEmpty()) {
        QJsonArray absent;
        QStringList all = effects.keys();
        all.removeAll(QStringLiteral("about"));
        for (const QString &key : std::as_const(all))
            if (!pageOf.contains(key))
                absent.append(key);
        QDir().mkpath(output);
        QFile file(QDir(output).filePath(combination.value(QStringLiteral("id")).toString() + QStringLiteral(".json")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("combination"), QJsonObject::fromVariantMap(combination)},
            {QStringLiteral("effectiveTier"), tier}, {QStringLiteral("shown"), evidence},
            {QStringLiteral("absent"), absent}, {QStringLiteral("failures"), QJsonArray::fromStringList(failures)}})
            .toJson());
        frame.draw(baseConfiguration, -1).save(QDir(output).filePath(combination.value(QStringLiteral("id")).toString() + QStringLiteral(".png")));
    }
    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QStringLiteral("\n"))));
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
