#include "RendererBuildConfig.h"
#include "themes/ThemePackage.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJSValue>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlEngine>
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
        QQmlEngine engine;
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            PanelScene {
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, layoutPadding: 10})
                entryDelegateContext: ({hostKind: "free"})
                orderedEntries: [{id: "one", displayName: "One"}, {id: "two", displayName: "Two"}]
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/SceneConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), theme}}));
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
        QCOMPARE(renderer->property("triangleCount").toInt(), 288);
        const QVariant geometry = plainValue(scene->property("entryRects"));
        const auto pixels = [renderer]() -> QImage
        {
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
            const QVariantMap expected = plainValue(model->property("modelData")).toMap();
            if (!expected.contains(QStringLiteral("centerX")))
                continue;
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
        QVariantMap missing = theme;
        missing.remove(QStringLiteral("scene3DResources"));
        scene->setProperty("themeDefinition", missing);
        QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
        QVERIFY(scene->property("fallbackApplied").toBool());
        missing = theme;
        QVariantMap paths = missing.value(QStringLiteral("assetPaths")).toMap();
        paths.insert(QStringLiteral("surface"), QStringLiteral("/nonexistent/archdock-scene-texture.svg"));
        missing.insert(QStringLiteral("assetPaths"), paths);
        scene->setProperty("themeDefinition", missing);
        QTRY_COMPARE(scene->property("fallbackReason").toString(), QStringLiteral("scene3d-texture-unavailable"));
        QCOMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("procedural2d"));
        scene->setProperty("themeDefinition", theme);
        QTRY_COMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"));
        qInfo() << "Real staged mesh scene:" << visiblePixels << "visible pixels; camera, quality and missing-resource fallback passed";
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
