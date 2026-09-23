#include "RendererBuildConfig.h"
#include "themes/ThemePackage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QPointer>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QVector3D>
#include <QtTest>

#include <memory>

namespace
{
QString importRoot()
{
    return qEnvironmentVariable("ARCHDOCK_RENDERING_IMPORT_ROOT",
                                QStringLiteral(ARCHDOCK_RENDERING_IMPORT_ROOT));
}

QVariantMap capability(QObject *scene)
{
    const QVariant value = scene->property("true3DCapability");
    return value.metaType() == QMetaType::fromType<QJSValue>()
        ? value.value<QJSValue>().toVariant().toMap() : value.toMap();
}

QVariant plainValue(const QVariant &value)
{
    return value.metaType() == QMetaType::fromType<QJSValue>()
        ? value.value<QJSValue>().toVariant() : value;
}

QObject *objectValue(const QVariant &value)
{
    return value.metaType() == QMetaType::fromType<QJSValue>()
        ? value.value<QJSValue>().toQObject() : value.value<QObject *>();
}

// Each missing-module case runs in a separate process: another engine cannot
// leave a registered optional plugin behind and accidentally satisfy it.
class MissingModule final : public QQmlAbstractUrlInterceptor
{
public:
    int blockedUrls = 0;
    QUrl intercept(const QUrl &url, DataType) override
    {
        if (url.path().contains(QStringLiteral("/QtQuick3D/")) ||
            url.path().contains(QStringLiteral("/QtQuick3D.")))
        {
            ++blockedUrls;
            return QUrl(QStringLiteral("file:///__archdock_missing_module__/qmldir"));
        }
        return url;
    }
};
}

class RendererCapabilityTest final : public QObject
{
    Q_OBJECT

private slots:
    void realScenePixelsQualityAndFallback()
    {
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // The private RHI invocation below is the real scene gate.
        const QString themeRoot = qEnvironmentVariable("ARCHDOCK_RENDERING_STAGED_THEME_ROOT",
            QStringLiteral(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT));
        const auto package = ArchDock::ThemePackage::load(themeRoot
            + QStringLiteral("/mesh-platform-cyan/archdock-theme.json"));
        QVERIFY2(package.isValid(), qPrintable(package.primaryCode()));
        QVariantMap theme = package.package->runtimeProjection();
        // A private XDG session need not have a desktop icon theme configured.
        // Reuse a local fixture so this gate measures mesh motion, not icon lookup.
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        QVERIFY(!glyphFixture.isEmpty());
        QQmlEngine engine;
        QStringList unexpectedWarnings;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings)
                if (!warning.description().contains(QStringLiteral("/nonexistent/archdock-scene-texture.svg")))
                    unexpectedWarnings.append(warning.toString());
        });
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            PanelScene {
                required property string glyphFixture
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, layoutPadding: 10})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}, {id: "collapse-radial", available: true}]})
                orderedEntries: [{id: "one", displayName: "One", iconName: glyphFixture},
                                 {id: "two", displayName: "Two", iconName: glyphFixture}]
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/SceneConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), theme},
            {QStringLiteral("glyphFixture"), QUrl::fromLocalFile(glyphFixture).toString()}}));
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *scene = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(scene);
        scene->setParentItem(window.contentItem());
        window.resize(qCeil(scene->width()), qCeil(scene->height()));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(),
                                 QStringLiteral("true3d"), 5000);
        QVERIFY(capability(scene).value(QStringLiteral("rendererAvailable")).toBool());
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(renderer);
        QCOMPARE(renderer->property("triangleCount").toInt(), 696);
        const QVariant geometry = plainValue(scene->property("entryRects"));
        const auto pixels = [scene]() -> QImage
        {
            auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
            if (!renderer)
                return {};
            auto *viewport = qobject_cast<QQuickItem *>(objectValue(renderer->property("viewport")));
            if (!viewport)
                return {};
            const auto grab = viewport->grabToImage();
            if (!grab)
                return {};
            QSignalSpy ready(grab.get(), &QQuickItemGrabResult::ready);
            if (!ready.wait(5000))
                return {};
            return grab->image();
        };
        const QImage first = pixels();
        QVERIFY(!first.isNull());
        int checkedIcons = 0;
        QObject *viewport = objectValue(renderer->property("viewport"));
        for (QObject *model : renderer->findChildren<QObject *>())
        {
            if (!model->objectName().startsWith(QStringLiteral("mesh-entry-"))
                || model->objectName().startsWith(QStringLiteral("mesh-entry-part-")))
                continue;
            const QVariantMap expected = plainValue(model->property("rect")).toMap();
            const QVector3D position = model->property("scenePosition").value<QVector3D>();
            QVector3D projected;
            QVERIFY(QMetaObject::invokeMethod(viewport, "mapFrom3DScene",
                Q_RETURN_ARG(QVector3D, projected), Q_ARG(QVector3D, position)));
            QVERIFY2(qAbs(projected.x() - expected.value(QStringLiteral("centerX")).toDouble()) < 1,
                qPrintable(QStringLiteral("Mesh x=%1, logical x=%2").arg(projected.x())
                    .arg(expected.value(QStringLiteral("centerX")).toDouble())));
            QVERIFY2(qAbs(projected.y() - expected.value(QStringLiteral("centerY")).toDouble()) < 1,
                qPrintable(QStringLiteral("Mesh y=%1, logical y=%2").arg(projected.y())
                    .arg(expected.value(QStringLiteral("centerY")).toDouble())));
            ++checkedIcons;
        }
        QCOMPARE(checkedIcons, 2);
        int visiblePixels = 0;
        for (int y = 0; y < first.height(); ++y)
            for (int x = 0; x < first.width(); ++x)
                if (first.pixelColor(x, y).alpha() > 32)
                    ++visiblePixels;
        QVERIFY2(visiblePixels > 1000, qPrintable(QStringLiteral("Only %1 mesh pixels").arg(visiblePixels)));
        const QString evidence = qEnvironmentVariable("ARCHDOCK_SCENE_EVIDENCE_DIR");
        if (!evidence.isEmpty())
            QVERIFY(first.save(QDir(evidence).filePath(QStringLiteral("mesh-scene.png"))));
        QVariantMap sceneSettings = theme.value(QStringLiteral("scene3D")).toMap();
        sceneSettings.insert(QStringLiteral("cameraYaw"), 60);
        theme.insert(QStringLiteral("scene3D"), sceneSettings);
        scene->setProperty("themeDefinition", theme);
        QTest::qWait(100);
        const QImage rotated = pixels();
        QVERIFY(!rotated.isNull());
        QVERIFY(first != rotated);
        QVariantMap definition = plainValue(scene->property("panelDefinition")).toMap();
        QVariantMap low;
        for (const QString &quality : {QStringLiteral("low"), QStringLiteral("high"), QStringLiteral("low")})
        {
            definition.insert(QStringLiteral("scene3DQuality"), quality);
            scene->setProperty("panelDefinition", definition);
            QTRY_COMPARE(renderer->property("effectiveQuality").toString(), quality);
            const QVariantMap state = plainValue(renderer->property("qualityState")).toMap();
            QVERIFY(state.value(QStringLiteral("targetWidth")).toInt() <= 2048);
            QVERIFY(state.value(QStringLiteral("targetHeight")).toInt() <= 2048);
            if (quality == QStringLiteral("low"))
            {
                if (low.isEmpty()) low = state;
                else QCOMPARE(state, low);
            }
            else
                QVERIFY(state.value(QStringLiteral("targetWidth")).toInt()
                        > low.value(QStringLiteral("targetWidth")).toInt());
            QVERIFY(!pixels().isNull());
            QCOMPARE(plainValue(scene->property("entryRects")), geometry);
        }

        // Use the shipped logical profile with the same controller used by 2D.
        QFile catalog(QFINDTESTDATA("../data/animation-profiles/builtin-animation-profiles.json"));
        QVERIFY(catalog.open(QIODevice::ReadOnly));
        const auto profiles = QJsonDocument::fromJson(catalog.readAll()).object()
            .toVariantMap().value(QStringLiteral("animationProfiles")).toList();
        QVariantMap turn;
        QVariantMap glow;
        for (const QVariant &profile : profiles)
        {
            const auto value = profile.toMap();
            if (value.value(QStringLiteral("id")) == QStringLiteral("slow-y-turn")) turn = value;
            if (value.value(QStringLiteral("id")) == QStringLiteral("glow")) glow = value;
        }
        QVERIFY(!turn.isEmpty() && !glow.isEmpty());
        QVariantMap motion{{QStringLiteral("animationProfile"), turn},
                           {QStringLiteral("animationTrigger"), QStringLiteral("idle")}};
        scene->setProperty("animationProfiles", motion);
        QVariant entryValue;
        QVERIFY(QMetaObject::invokeMethod(scene, "entryItemAt",
            Q_RETURN_ARG(QVariant, entryValue), Q_ARG(QVariant, QVariant(0))));
        QObject *entry = objectValue(entryValue);
        QVERIFY(entry);
        QTRY_VERIFY(objectValue(entry->property("motionController")));
        QObject *controller = objectValue(entry->property("motionController"));
        QObject *glyph = renderer->findChild<QObject *>(QStringLiteral("mesh-glyph-0"));
        QVERIFY(glyph);
        QTRY_VERIFY(qAbs(glyph->property("eulerRotation").value<QVector3D>().y()) > 5);
        const auto channels = plainValue(controller->property("channels")).toMap();
        QVERIFY(qAbs(glyph->property("eulerRotation").value<QVector3D>().y()
            - channels.value(QStringLiteral("glyph/rotate-y")).toDouble()) < 0.01);
        QVERIFY(!controller->property("hasConflict").toBool());
        QCOMPARE(plainValue(scene->property("entryRects")), geometry);
        QObject *visual = objectValue(entry->property("meshVisualItem"));
        QVERIFY(visual);
        QCOMPARE(plainValue(visual->property("resolvedGlyphMotion")).toMap()
            .value(QStringLiteral("rotateY")).toDouble(), 0.0);
        auto *glyphSource = qobject_cast<QQuickItem *>(objectValue(visual->property("glyphItem")));
        QVERIFY(glyphSource);
        const QMetaProperty status = glyphSource->metaObject()->property(
            glyphSource->metaObject()->indexOfProperty("status"));
        const int readyStatus = status.enumerator().keyToValue("Ready");
        QVERIFY(readyStatus >= 0);
        QTRY_COMPARE(glyphSource->property("status").toInt(), readyStatus);
        const auto glyphGrab = glyphSource->grabToImage();
        QVERIFY(glyphGrab);
        QSignalSpy glyphReady(glyphGrab.get(), &QQuickItemGrabResult::ready);
        QVERIFY(glyphReady.wait(5000));
        const QImage glyphImage = glyphGrab->image();
        QVERIFY(!glyphImage.isNull());
        int glyphPixels = 0;
        for (int y = 0; y < glyphImage.height(); ++y)
            for (int x = 0; x < glyphImage.width(); ++x)
                glyphPixels += glyphImage.pixelColor(x, y).alpha() > 32;
        QVERIFY2(glyphPixels > 100, qPrintable(QStringLiteral(
            "Expected visible glyph texture, got %1 pixels").arg(glyphPixels)));
        const QImage turning = pixels();
        QVERIFY(!turning.isNull());
        const auto firstAngle = glyph->property("eulerRotation").value<QVector3D>().y();
        QTest::qWait(180);
        const QImage advanced = pixels();
        QVERIFY(!advanced.isNull());
        if (!evidence.isEmpty())
        {
            QVERIFY(turning.save(QDir(evidence).filePath(QStringLiteral("mesh-motion-before.png"))));
            QVERIFY(advanced.save(QDir(evidence).filePath(QStringLiteral("mesh-motion-after.png"))));
        }
        if (turning == advanced && !evidence.isEmpty())
        {
            const QImage wholeBefore = window.grabWindow();
            QVERIFY(wholeBefore.save(QDir(evidence).filePath(QStringLiteral("whole-before.png"))));
            QTest::qWait(180);
            const QImage wholeAfter = window.grabWindow();
            QVERIFY(wholeAfter.save(QDir(evidence).filePath(QStringLiteral("whole-after.png"))));
            qInfo() << "Whole window motion changes pixels:" << (wholeBefore != wholeAfter);
        }
        QVERIFY2(turning != advanced, qPrintable(QStringLiteral(
            "Mesh glyph angle %1 -> %2 produced unchanged pixels; glyph source=%3 valid=%4 size=%5x%6")
            .arg(firstAngle).arg(glyph->property("eulerRotation").value<QVector3D>().y())
            .arg(glyphSource->property("source").toString())
            .arg(glyphSource->property("valid").toBool())
            .arg(glyphSource->property("width").toDouble()).arg(glyphSource->property("height").toDouble())));

        scene->setProperty("sceneConcealed", true);
        QTRY_COMPARE(plainValue(controller->property("activeTracks")).toList().size(), 0);
        QCOMPARE(glyph->property("eulerRotation").value<QVector3D>().y(), 0.0f);
        scene->setProperty("sceneConcealed", false);
        QTRY_VERIFY(!plainValue(controller->property("activeTracks")).toList().isEmpty());
        window.hide();
        QTRY_COMPARE(plainValue(controller->property("activeTracks")).toList().size(), 0);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(!plainValue(controller->property("activeTracks")).toList().isEmpty());
        motion.insert(QStringLiteral("reducedMotion"), true);
        scene->setProperty("animationProfiles", motion);
        QTRY_COMPARE(plainValue(controller->property("activeTracks")).toList().size(), 0);
        QCOMPARE(glyph->property("eulerRotation").value<QVector3D>().y(), 0.0f);

        motion.insert(QStringLiteral("reducedMotion"), false);
        motion.insert(QStringLiteral("animationProfile"), glow);
        motion.insert(QStringLiteral("animationTrigger"), QStringLiteral("hover"));
        scene->setProperty("animationProfiles", motion);
        scene->setProperty("runtimeState", QVariantMap{{QStringLiteral("hovered"), true},
            {QStringLiteral("hoveredEntry"), 0}});
        QTRY_VERIFY(plainValue(controller->property("channels")).toMap()
            .value(QStringLiteral("icon/glow")).toDouble() > 0);
        QVERIFY(renderer->property("emissionScale").toDouble() > 1);
        QObject *meshEntry = renderer->findChild<QObject *>(QStringLiteral("mesh-entry-0"));
        QVERIFY(meshEntry);
        QVERIFY(meshEntry->property("glow").toDouble() > 0);
        const double baseEmission = theme.value(QStringLiteral("scene3DResources")).toMap()
            .value(QStringLiteral("material")).toMap().value(QStringLiteral("emissiveStrength")).toDouble()
            * renderer->property("emissionScale").toDouble();
        bool emitted = false;
        for (QObject *child : meshEntry->findChildren<QObject *>())
            if (child->property("strength").isValid())
                emitted |= child->property("strength").toDouble() > baseEmission;
        QVERIFY(emitted);

        definition.insert(QStringLiteral("panelRotationMode"), QStringLiteral("clockwise"));
        definition.insert(QStringLiteral("panelRotationSpeed"), 30);
        definition.insert(QStringLiteral("collapseMechanism"), QStringLiteral("collapse-radial"));
        scene->setProperty("panelDefinition", definition);
        QTRY_VERIFY(scene->property("sceneRotationAngle").toDouble() > 2);
        QObject *platform = renderer->findChild<QObject *>(QStringLiteral("mesh-platform-motion"));
        QVERIFY(platform);
        QVERIFY(qAbs(platform->property("eulerRotation").value<QVector3D>().z()
            + scene->property("effectiveLayoutAngle").toDouble()) < 0.01);
        QVERIFY(plainValue(scene->property("entryRects")) != geometry);
        motion.insert(QStringLiteral("reducedMotion"), true);
        scene->setProperty("animationProfiles", motion);
        QTRY_VERIFY(!scene->property("sceneRotationActive").toBool());
        QCOMPARE(scene->property("sceneRotationAngle").toDouble(), 0.0);
        QObject *part = renderer->findChild<QObject *>(QStringLiteral("mesh-panel-part-0"));
        QVERIFY(part);
        const QVector3D openPosition = part->property("position").value<QVector3D>();
        scene->setProperty("runtimeState", QVariantMap{
            {QStringLiteral("presentationState"), QStringLiteral("collapsed")},
            {QStringLiteral("presentationProgress"), 1.0}});
        QTRY_COMPARE(part->property("openAmount").toDouble(), 0.0);
        QVERIFY(part->property("position").value<QVector3D>() != openPosition);
        scene->setProperty("runtimeState", QVariantMap{
            {QStringLiteral("presentationState"), QStringLiteral("open")},
            {QStringLiteral("presentationProgress"), 0.0}});
        QTRY_COMPARE(part->property("position").value<QVector3D>(), openPosition);

        motion.insert(QStringLiteral("reducedMotion"), false);
        motion.insert(QStringLiteral("animationProfile"), turn);
        motion.insert(QStringLiteral("animationTrigger"), QStringLiteral("idle"));
        scene->setProperty("animationProfiles", motion);
        QTRY_VERIFY(!plainValue(controller->property("activeTracks")).toList().isEmpty());
        QPointer<QObject> oldRenderer(renderer);
        QVariantMap missing = theme;
        missing.remove(QStringLiteral("scene3DResources"));
        scene->setProperty("themeDefinition", missing);
        QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
        QVERIFY(scene->property("fallbackApplied").toBool());
        QTRY_VERIFY(oldRenderer.isNull());
        QVERIFY(!scene->findChild<QObject *>(QStringLiteral("mesh-glyph-0")));
        QVERIFY(!controller->property("hasConflict").toBool());
        QVERIFY(!visual->property("meshVisualActive").toBool());
        missing = theme;
        QVariantMap paths = missing.value(QStringLiteral("assetPaths")).toMap();
        paths.insert(QStringLiteral("surface"), QStringLiteral("/nonexistent/archdock-scene-texture.svg"));
        missing.insert(QStringLiteral("assetPaths"), paths);
        scene->setProperty("themeDefinition", missing);
        QTRY_COMPARE(scene->property("fallbackReason").toString(), QStringLiteral("scene3d-texture-unavailable"));
        QCOMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
        scene->setProperty("themeDefinition", theme);
        QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"));
        renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(!pixels().isNull());
        const int resourcesAfterRecovery = renderer->findChildren<QObject *>().size();
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            QPointer<QObject> previous(renderer);
            scene->setProperty("themeDefinition", QVariantMap{});
            QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
            QTRY_VERIFY(previous.isNull());
            scene->setProperty("themeDefinition", theme);
            QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"));
            renderer = objectValue(scene->property("activeSurfaceRenderer"));
            QVERIFY(!pixels().isNull());
            QCOMPARE(renderer->findChildren<QObject *>().size(), resourcesAfterRecovery);
            QCOMPARE(renderer->property("triangleCount").toInt(), 696);
        }
        const auto bakedPackage = ArchDock::ThemePackage::load(themeRoot
            + QStringLiteral("/ring-platform-blue/archdock-theme.json"));
        QVERIFY2(bakedPackage.isValid(), qPrintable(bakedPackage.primaryCode()));
        QVariantMap fallbackTheme = bakedPackage.package->runtimeProjection();
        auto fallbackCapabilities = fallbackTheme.value(QStringLiteral("capabilities")).toMap();
        fallbackCapabilities.insert(QStringLiteral("rendererTiers"),
            QStringList{QStringLiteral("true3d"), QStringLiteral("baked2.5d"), QStringLiteral("procedural2d")});
        fallbackCapabilities.insert(QStringLiteral("fallbackRendererTiers"),
            QStringList{QStringLiteral("baked2.5d"), QStringLiteral("procedural2d")});
        fallbackTheme.insert(QStringLiteral("capabilities"), fallbackCapabilities);
        // Keep valid baked artwork while making the requested mesh unavailable.
        fallbackTheme.insert(QStringLiteral("scene3D"), theme.value(QStringLiteral("scene3D")));
        QPointer<QObject> meshBeforeFallback(renderer);
        scene->setProperty("themeDefinition", fallbackTheme);
        QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("baked2.5d"));
        QCOMPARE(scene->property("fallbackReason").toString(), QStringLiteral("scene3d-resources-unavailable"));
        QTRY_VERIFY(meshBeforeFallback.isNull());
        QCOMPARE(plainValue(scene->property("entryRects")).toList().size(), 2);
        QVERIFY(!window.grabWindow().isNull());
        fallbackTheme.insert(QStringLiteral("assetPaths"), QVariantMap{});
        scene->setProperty("themeDefinition", fallbackTheme);
        QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
        QCOMPARE(plainValue(scene->property("panelDefinition")).toMap()
            .value(QStringLiteral("rendererTier")).toString(), QStringLiteral("true3d"));
        QCOMPARE(plainValue(scene->property("entryRects")).toList().size(), 2);
        scene->setProperty("themeDefinition", theme);
        QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"));
        renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(!pixels().isNull());
        QVariantList excessiveEntries;
        for (int index = 0; index < 1000; ++index)
            excessiveEntries.append(QVariantMap{{QStringLiteral("width"), 40}});
        // Detach the live geometry binding so rotation cannot undo this fault.
        QVERIFY(QQmlProperty::write(renderer, QStringLiteral("entryGeometry"), excessiveEntries));
        QTRY_COMPARE(scene->property("fallbackReason").toString(), QStringLiteral("scene3d-resource-limit"));
        // Repeater3D releases removed delegates through Qt's deferred deletion.
        QTRY_VERIFY(!renderer->findChild<QObject *>(QStringLiteral("mesh-entry-0")));
        QCOMPARE(plainValue(renderer->property("entryGeometry")).toList().size(), 1000);
        QCOMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
        qInfo() << "Real staged mesh scene:" << visiblePixels
                << "visible pixels; shared motion, parts, concealment, reduced motion, camera, quality,"
                   " active fallback and bounded recovery resources passed";
    }

    void packagedBuildFacts()
    {
        const QString module = importRoot() + QStringLiteral("/ArchDock/Rendering/");
        QVERIFY(QFileInfo::exists(module + QStringLiteral("RendererBuildConfig.qml")));
        QVERIFY(QFileInfo::exists(module + QStringLiteral("RendererCapabilityProbe.qml")));
        QCOMPARE(QFileInfo::exists(module + QStringLiteral("optional3d/RendererImportProbe.qml")),
                 ARCHDOCK_QUICK3D_BUILT != 0);
        for (const QString &file : {QStringLiteral("PanelScene3D.qml"), QStringLiteral("IconStyle3D.qml")})
            QCOMPARE(QFileInfo::exists(module + QStringLiteral("optional3d/") + file),
                     ARCHDOCK_SCENE3D_BUILT != 0);
    }

    void missingSceneInputsFailClosed()
    {
        if (!ARCHDOCK_SCENE3D_BUILT || qEnvironmentVariableIsSet("ARCHDOCK_TEST_MISSING_3D"))
            return;
        QQmlEngine engine;
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine, QUrl::fromLocalFile(importRoot()
            + QStringLiteral("/ArchDock/Rendering/optional3d/PanelScene3D.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> scene(component.createWithInitialProperties({
            {QStringLiteral("resources"), QVariantMap{}},
            {QStringLiteral("sceneDefinition"), QVariantMap{}}}));
        QVERIFY2(scene != nullptr, qPrintable(component.errorString()));
        QVERIFY(!scene->property("rendererReady").toBool());
        QCOMPARE(scene->property("errorReason").toString(), QStringLiteral("scene3d-mesh-unavailable"));
    }

    void consumerFactsAndSafeFallback()
    {
        const bool missing = qEnvironmentVariableIsSet("ARCHDOCK_TEST_MISSING_3D");
        const bool rhi = qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI");
        MissingModule interceptor;
        QQmlEngine engine;
        engine.addImportPath(importRoot());
        if (missing)
            engine.addUrlInterceptor(&interceptor);
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            PanelScene {
                panelDefinition: ({rendererTier: "true3d", layout: "horizontal",
                                   iconSize: 40, layoutPadding: 10})
                orderedEntries: [{id: "one", displayName: "One"}]
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/CapabilityConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *scene = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(scene);
        scene->setParentItem(window.contentItem());
        window.resize(320, 160);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY_WITH_TIMEOUT(
            capability(scene).value(QStringLiteral("reasonCode")).toString()
                != QStringLiteral("renderer-import-loading"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(
            capability(scene).value(QStringLiteral("reasonCode")).toString()
                != QStringLiteral("renderer-backend-uninitialized"), 5000);
        const QVariantMap facts = capability(scene);
        QCOMPARE(facts.value(QStringLiteral("buildAvailable")).toBool(),
                 ARCHDOCK_QUICK3D_BUILT != 0);
        QCOMPARE(facts.value(QStringLiteral("sceneBuilt")).toBool(),
                 ARCHDOCK_SCENE3D_BUILT != 0);
        const bool imports = ARCHDOCK_QUICK3D_BUILT && !missing;
        QCOMPARE(facts.value(QStringLiteral("importAvailable")).toBool(), imports);
        if (missing && ARCHDOCK_QUICK3D_BUILT)
        {
            QVERIFY(interceptor.blockedUrls > 0);
            QVERIFY(!facts.value(QStringLiteral("importDiagnostic")).toString().isEmpty());
        }
        if (rhi)
            QVERIFY(facts.value(QStringLiteral("backendSupported")).toBool());
        else
            QVERIFY(!facts.value(QStringLiteral("backendSupported")).toBool());
        QCOMPARE(facts.value(QStringLiteral("moduleAvailable")).toBool(), imports && rhi);
        QCOMPARE(facts.value(QStringLiteral("rendererAvailable")).toBool(),
                 imports && rhi && ARCHDOCK_SCENE3D_BUILT);
        const QString reason = !ARCHDOCK_QUICK3D_BUILT ? QStringLiteral("renderer-not-installed")
            : missing ? QStringLiteral("renderer-import-unavailable")
            : !rhi ? QStringLiteral("renderer-backend-unsupported")
            : !ARCHDOCK_SCENE3D_BUILT ? QStringLiteral("renderer-scene-unavailable")
            : QStringLiteral("available");
        QCOMPARE(facts.value(QStringLiteral("reasonCode")).toString(), reason);
        // Saved intent plus an absent scene package must still draw safe 2D.
        QCOMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
        QVERIFY(scene->property("fallbackApplied").toBool());
        scene->setProperty("panelDefinition", QVariantMap{
            {QStringLiteral("rendererTier"), QStringLiteral("procedural2d")}});
        QCOMPARE(capability(scene), facts);
        QCOMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
        qInfo().noquote() << "Consumer renderer facts:" << facts;
        if (missing)
            engine.removeUrlInterceptor(&interceptor);
    }
};

QTEST_MAIN(RendererCapabilityTest)
#include "RendererCapabilityTest.moc"
