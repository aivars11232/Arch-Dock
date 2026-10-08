#include "RendererBuildConfig.h"
#include "themes/LookPalette.h"
#include "themes/ThemePackage.h"

#include <QLineF>
#include <QSignalSpy>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QtMath>
#include <QPointer>
#include <QPointingDevice>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QVector3D>
#include <QWheelEvent>
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

// Keep the native pixel/geometry oracle independent of the QML helper. The
// public Camera overload reads the renderer's projection; View3D's overload
// writes that shared projection while the threaded renderer can be using it.
bool projectScenePoint(QObject *view, const QVector3D &position, QVector3D &point)
{
    auto *camera = view ? objectValue(view->property("camera")) : nullptr;
    QVector3D normalized;
    if (!camera || !QMetaObject::invokeMethod(camera, "mapToViewport",
        Q_RETURN_ARG(QVector3D, normalized), Q_ARG(QVector3D, position))) return false;
    point = normalized * QVector3D(view->property("width").toFloat(),
                                  view->property("height").toFloat(), 1);
    return true;
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
    void wheelSourcesPreserveNotchesAndSmoothPixels()
    {
        // PD-16: a real notch remains a notch even when Wayland also supplies
        // pixels. A synthesized smooth event keeps its pixel travel, including
        // continuous devices that do not provide scroll phases.
        QQmlEngine engine;
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import "inputs" as Inputs
            Item {
                width: 240; height: 160
                property alias contentY: view.contentY
                Flickable {
                    id: view
                    anchors.fill: parent
                    contentWidth: width; contentHeight: 800
                }
                Inputs.ScrollInput {
                    flickables: [view]
                    verticalNotch: 60
                    verticalPixelScale: 0.4
                }
            }
        )", QUrl::fromLocalFile(importRoot() + "/ArchDock/Rendering/WheelConsumer.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.create());
        auto *item = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(item);
        item->setParentItem(window.contentItem());
        window.resize(240, 160);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto send = [&](Qt::MouseEventSource source, Qt::ScrollPhase phase, int angle,
                              const QPointingDevice *device = nullptr) {
            item->setProperty("contentY", 0);
            const QPointF point(50, 50);
            QWheelEvent event(point, window.mapToGlobal(point.toPoint()), QPoint(0, -15),
                QPoint(0, angle), Qt::NoButton, Qt::NoModifier, phase, false, source,
                device ? device : QPointingDevice::primaryPointingDevice());
            QCoreApplication::sendEvent(&window, &event);
        };
        send(Qt::MouseEventNotSynthesized, Qt::NoScrollPhase, -120);
        QTRY_COMPARE(item->property("contentY").toDouble(), 60.0);
        send(Qt::MouseEventSynthesizedBySystem, Qt::NoScrollPhase, -180);
        QTRY_COMPARE(item->property("contentY").toDouble(), 6.0);
        send(Qt::MouseEventSynthesizedBySystem, Qt::ScrollUpdate, -180);
        QTRY_COMPARE(item->property("contentY").toDouble(), 6.0);
        QPointingDevice seat(QStringLiteral("opaque Wayland seat"), 1,
            QInputDevice::DeviceType::TouchPad, QPointingDevice::PointerType::Finger,
            QInputDevice::Capability::Position | QInputDevice::Capability::Scroll
                | QInputDevice::Capability::PixelScroll, 1, 0);
        send(Qt::MouseEventNotSynthesized, Qt::NoScrollPhase, -120, &seat);
        QTRY_COMPARE(item->property("contentY").toDouble(), 60.0);
        send(Qt::MouseEventSynthesizedBySystem, Qt::NoScrollPhase, -180, &seat);
        QTRY_COMPARE(item->property("contentY").toDouble(), 6.0);
    }

    void realScenePixelsQualityAndFallback_data()
    {
        QTest::addColumn<QString>("themeId");
        QTest::newRow("cyan") << QStringLiteral("mesh-platform-cyan");
        QTest::newRow("orange") << QStringLiteral("arc-platform-orange");
    }

    void realScenePixelsQualityAndFallback()
    {
        QFETCH(QString, themeId);
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // The private RHI invocation below is the real scene gate.
        const QString themeRoot = qEnvironmentVariable("ARCHDOCK_RENDERING_STAGED_THEME_ROOT",
            QStringLiteral(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT));
        const auto package = ArchDock::ThemePackage::load(themeRoot
            + QStringLiteral("/") + themeId + QStringLiteral("/archdock-theme.json"));
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
                                   scene3DQuality: "low", iconSize: 40, layoutPadding: 10,
                                   panelMotionTarget: "panel"})
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
        const int meshTriangles = theme.value("scene3DResources").toMap()
            .value("mesh").toMap().value("indexes").toList().size() / 3;
        // The platform and its two declared panel parts, and per entry: a
        // glyph and a tile quad (2 + 2), a 72-triangle pedestal, the theme's
        // icon base and its declared entry part (one platform mesh each).
        const int expectedTriangles = meshTriangles * 7 + 2 * (2 + 2 + 72);
        QCOMPARE(renderer->property("triangleCount").toInt(), expectedTriangles);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 2);
        const QVariant geometry = plainValue(scene->property("entryRects"));
        const auto projectionMatches = [&]() {
            const auto rects = plainValue(scene->property("entryRects")).toList();
            const auto projected = plainValue(renderer->property("projectedEntryGeometry")).toList();
            auto *view = objectValue(renderer->property("viewport"));
            if (!view || rects.size() != projected.size() || rects.isEmpty()) return false;
            for (int i = 0; i < rects.size(); ++i) {
                const auto bounds = rects[i].toMap();
                const auto output = projected[i].toMap();
                auto *glyph = renderer->findChild<QObject *>(QStringLiteral("mesh-glyph-%1").arg(i));
                QVector3D center;
                if (!glyph || !projectScenePoint(view, glyph->property("scenePosition").value<QVector3D>(), center)) return false;
                for (const auto &key : {"x", "y", "width", "height"})
                    if (qAbs(bounds.value(key).toDouble() - output.value(key).toDouble()) > 1) return false;
                if (qAbs(center.x() - output.value("centerX").toDouble()) > 1
                    || qAbs(center.y() - output.value("centerY").toDouble()) > 1) return false;
            }
            return true;
        };
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
        QTest::qWait(200); // Allow the asynchronous icon source to reach the mesh texture.
        const QImage first = pixels();
        QVERIFY(!first.isNull());
        int checkedIcons = 0;
        QObject *viewport = objectValue(renderer->property("viewport"));
        QVector3D hole;
        QVERIFY(projectScenePoint(viewport, QVector3D(0, 0, renderer->property("platformTop").toDouble()), hole));
        QVariant holeHit;
        QVERIFY(QMetaObject::invokeMethod(renderer, "containsInputPoint", Q_RETURN_ARG(QVariant, holeHit),
            Q_ARG(QVariant, QVariant(QPointF(hole.x(), hole.y())))));
        QVERIFY2(!holeHit.toBool(), "The empty ring centre must pass through native input");
        for (QObject *model : renderer->findChildren<QObject *>())
        {
            if (!model->objectName().startsWith(QStringLiteral("mesh-entry-"))
                || model->objectName().startsWith(QStringLiteral("mesh-entry-part-")))
                continue;
            const int index = model->objectName().mid(QStringLiteral("mesh-entry-").size()).toInt();
            const QVariantMap expected = plainValue(renderer->property("projectedEntryGeometry")).toList()[index].toMap();
            model = renderer->findChild<QObject *>(QStringLiteral("mesh-glyph-%1").arg(index));
            QVERIFY(model);
            const QVector3D position = model->property("scenePosition").value<QVector3D>();
            QVector3D projected;
            QVERIFY(projectScenePoint(viewport, position, projected));
            QVERIFY2(qAbs(projected.x() - expected.value(QStringLiteral("centerX")).toDouble()) < 1,
                qPrintable(QStringLiteral("Mesh x=%1, logical x=%2").arg(projected.x())
                    .arg(expected.value(QStringLiteral("centerX")).toDouble())));
            QVERIFY2(qAbs(projected.y() - expected.value(QStringLiteral("centerY")).toDouble()) < 1,
                qPrintable(QStringLiteral("Mesh y=%1, logical y=%2").arg(projected.y())
                    .arg(expected.value(QStringLiteral("centerY")).toDouble())));
            int glyphPixels = 0;
            const int radius = expected.value(QStringLiteral("width")).toInt() / 2;
            for (int y = qMax(0, qRound(projected.y()) - radius);
                 y < qMin(first.height(), qRound(projected.y()) + radius); ++y)
                for (int x = qMax(0, qRound(projected.x()) - radius);
                     x < qMin(first.width(), qRound(projected.x()) + radius); ++x) {
                    const QColor color = first.pixelColor(x, y);
                    // The fixture's dark blue fill (#243447) is distinct from the
                    // light cyan platform and from the dark brown orange one.
                    glyphPixels += color.alpha() > 32 && color.redF() < 0.4
                        && color.greenF() < 0.4 && color.blueF() < 0.4
                        && color.blueF() > color.redF() + 0.05;
                }
            QVERIFY2(glyphPixels > 100, qPrintable(QStringLiteral(
                "Entry %1 glyph was covered by the platform: %2 visible pixels")
                .arg(model->objectName()).arg(glyphPixels)));
            ++checkedIcons;
        }
        QCOMPARE(checkedIcons, 2);
        int visiblePixels = 0;
        for (int y = 0; y < first.height(); ++y)
            for (int x = 0; x < first.width(); ++x)
                if (first.pixelColor(x, y).alpha() > 32)
                    ++visiblePixels;
        QVERIFY2(visiblePixels > 1000, qPrintable(QStringLiteral("Only %1 mesh pixels").arg(visiblePixels)));
        const QString evidenceRoot = qEnvironmentVariable("ARCHDOCK_SCENE_EVIDENCE_DIR");
        const QString evidence = themeId == QStringLiteral("arc-platform-orange") && !evidenceRoot.isEmpty()
            ? QDir(evidenceRoot).filePath(QStringLiteral("orange")) : evidenceRoot;
        if (!evidence.isEmpty()) QVERIFY(QDir().mkpath(evidence));
        if (!evidence.isEmpty())
            QVERIFY(first.save(QDir(evidence).filePath(QStringLiteral("mesh-scene.png"))));
        auto tiltedDefinition = plainValue(scene->property("panelDefinition")).toMap();
        for (const double pitch : {10.0, -35.0}) {
            tiltedDefinition.insert(QStringLiteral("scene3DCameraPitch"), pitch);
            scene->setProperty("panelDefinition", tiltedDefinition);
            QTRY_COMPARE(plainValue(renderer->property("sceneDefinition")).toMap()
                .value(QStringLiteral("cameraPitch")).toDouble(), pitch);
            QTest::qWait(100);
            const auto tilted = pixels();
            QVERIFY(!tilted.isNull());
            QVERIFY(tilted != first);
            QTRY_VERIFY(projectionMatches());
            QVERIFY(plainValue(scene->property("entryRects")) != geometry);
            if (!evidence.isEmpty())
                QVERIFY(tilted.save(QDir(evidence).filePath(QStringLiteral("mesh-tilt-%1.png").arg(pitch))));
        }
        // Wheel rotation moves real world-space icons between near and far
        // positions; projected size and input bounds must follow their depth.
        scene->setProperty("wheelRotationAngle", 90.0);
        QTest::qWait(150);
        QVERIFY(!pixels().isNull());
        QTRY_VERIFY(projectionMatches());
        const auto depthRects = plainValue(renderer->property("projectedEntryGeometry")).toList();
        QVERIFY(qAbs(depthRects[0].toMap().value("depth").toDouble()
            - depthRects[1].toMap().value("depth").toDouble()) > 1);
        QVERIFY(qAbs(depthRects[0].toMap().value("width").toDouble()
            - depthRects[1].toMap().value("width").toDouble()) > 1);
        if (!evidence.isEmpty()) QVERIFY(pixels().save(QDir(evidence).filePath("mesh-depth-rotation.png")));
        scene->setProperty("wheelRotationAngle", 0.0);
        QTest::qWait(100);
        const auto surfacePoint = [&](double angle) {
            const double radius = renderer->property("platformScale").toDouble() * 0.84;
            QVector3D point;
            projectScenePoint(viewport, QVector3D(radius * qCos(angle), radius * qSin(angle),
                    renderer->property("platformTop").toDouble()), point);
            return QPointF(point.x(), point.y());
        };
        const QPointF dragStart = surfacePoint(M_PI / 4);
        const QPointF dragFinish = surfacePoint(M_PI / 3);
        QTest::mouseMove(&window, dragStart.toPoint());
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, dragStart.toPoint());
        QTRY_VERIFY(scene->property("rotationDragActive").toBool());
        QTest::mouseMove(&window, dragFinish.toPoint(), 30);
        QTRY_VERIFY(qAbs(scene->property("wheelRotationAngle").toDouble()) > 5);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragFinish.toPoint());
        QTRY_VERIFY(!scene->property("rotationDragActive").toBool());
        const double dragged = scene->property("wheelRotationAngle").toDouble();
        QWheelEvent wheel(dragFinish, window.mapToGlobal(dragFinish.toPoint()), QPoint(), QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&window, &wheel);
        QTRY_VERIFY(scene->property("wheelRotationAngle").toDouble() != dragged);
        QTRY_VERIFY(projectionMatches());
        // A wheel step eases in over 120 ms (ADREP-TASK-002); let it rest
        // before putting the panel back.
        QTRY_VERIFY(!scene->property("turnStepping").toBool());
        scene->setProperty("wheelRotationAngle", 0.0);
        scene->setProperty("wheelRotationTarget", 0.0);
        // ADREP-TASK-002: with its entries travelling instead, the panel keeps
        // still and the 3D entries, their projection and their input move
        // along its track.
        {
            auto travelling = plainValue(scene->property("panelDefinition")).toMap();
            travelling.insert(QStringLiteral("panelMotionTarget"), QStringLiteral("items"));
            scene->setProperty("panelDefinition", travelling);
            QTRY_VERIFY(projectionMatches());
            const auto rested = plainValue(scene->property("entryRects"));
            QObject *entry = renderer->findChild<QObject *>(QStringLiteral("mesh-entry-0"));
            QVERIFY(entry);
            const QVector3D stood = entry->property("position").value<QVector3D>();
            QWheelEvent travel(dragFinish, window.mapToGlobal(dragFinish.toPoint()), QPoint(), QPoint(0, 120),
                Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(&window, &travel);
            QTRY_COMPARE(scene->property("entryTravel").toDouble(), 1.0);
            QCOMPARE(scene->property("effectiveLayoutAngle").toDouble(), 0.0);
            QTRY_VERIFY(projectionMatches());
            QVERIFY(plainValue(scene->property("entryRects")) != rested);
            QVERIFY((entry->property("position").value<QVector3D>() - stood).length() > 1);
            scene->setProperty("wheelTravel", 0.0);
            scene->setProperty("wheelTravelTarget", 0.0);
            travelling.insert(QStringLiteral("panelMotionTarget"), QStringLiteral("panel"));
            scene->setProperty("panelDefinition", travelling);
            QTRY_VERIFY(projectionMatches());
        }
        // ADREP-TASK-004: native material editing and a folded platform must
        // change the GPU output, preserve the track and keep input projection.
        {
            const auto own = plainValue(scene->property("panelDefinition")).toMap();
            auto edited = own;
            edited.insert("scene3DTransitions", false);
            edited.insert("scene3DFloat", false);
            edited.insert("scene3DFold", 0.0);
            scene->setProperty("panelDefinition", edited);
            scene->setProperty("animationProfiles", QVariantMap{{"reducedMotion", true}});
            // Material and track captures hold hover still; the separate
            // hover gate below proves that input anchors do not breathe.
            QTest::mouseMove(&window, QPoint(0, 0));
            QTest::qWait(150);
            const auto ownPixels = pixels();
            QVERIFY(!ownPixels.isNull());
            QImage preceding = ownPixels;
            for (const QString &look : {QStringLiteral("glass"), QStringLiteral("crystal"),
                QStringLiteral("neon"), QStringLiteral("minimal"), QStringLiteral("plasma"),
                QStringLiteral("lime"), QStringLiteral("floating-glass"), QStringLiteral("metallic"),
                QStringLiteral("futuristic"), QStringLiteral("organic"), QStringLiteral("platform"),
                QStringLiteral("plate"), QStringLiteral("pedestal")}) {
                edited.insert("scene3DMaterial", look);
                edited.insert("scene3DTexture", "theme");
                scene->setProperty("panelDefinition", edited);
                QTRY_VERIFY_WITH_TIMEOUT(renderer->property("materialTextureReady").toBool(), 5000);
                QTest::qWait(100);
                const auto material = pixels();
                QVERIFY2(!material.isNull() && material != preceding, qPrintable(look));
                if (!evidence.isEmpty()) QVERIFY(material.save(QDir(evidence).filePath("material-" + look + ".png")));
                preceding = material;
                QTRY_VERIFY(projectionMatches());
            }
            edited.insert("scene3DColor", "#dd3399");
            edited.insert("scene3DTexture", "none");
            scene->setProperty("panelDefinition", edited);
            QTest::qWait(120);
            const auto coloured = pixels();
            QVERIFY(coloured != preceding);
            edited.insert("scene3DTexture", "organic");
            scene->setProperty("panelDefinition", edited);
            QTRY_VERIFY_WITH_TIMEOUT(renderer->property("materialTextureReady").toBool(), 5000);
            QTest::qWait(120);
            QVERIFY(pixels() != coloured);
            edited.remove("scene3DColor"); edited.remove("scene3DTexture"); edited.remove("scene3DMaterial");
            scene->setProperty("panelDefinition", edited);
            QTest::qWait(150);
            QCOMPARE(pixels(), ownPixels);
            const auto flatGeometry = plainValue(renderer->property("projectedEntryGeometry"));
            for (double fold : {0.75, -0.75}) {
                edited.insert("scene3DFold", fold);
                scene->setProperty("panelDefinition", edited);
                QTRY_COMPARE(renderer->property("platformFold").toDouble(), fold);
                QTRY_VERIFY(projectionMatches());
                QTest::qWait(120);
                const auto folded = pixels();
                QVERIFY(folded != ownPixels);
                QCOMPARE(renderer->property("triangleCount").toInt(), expectedTriangles);
                QVERIFY(plainValue(renderer->property("projectedEntryGeometry")) != flatGeometry);
                if (!evidence.isEmpty()) QVERIFY(folded.save(QDir(evidence).filePath(QStringLiteral("fold-%1.png").arg(fold))));
            }
            edited.insert("scene3DFold", 0.0);
            edited.insert("scene3DIconElevation", 0.0);
            edited.insert("scene3DThickness", 4.0);
            edited.insert("panelMotionTarget", "items");
            bool covered = false, exposed = false;
            const double period = plainValue(scene->property("trackWindow")).toMap().value("loop").toDouble();
            QVERIFY(period > 0);
            for (double pitch : {-60.0, -35.0, 35.0, 60.0}) {
                edited.insert("scene3DCameraPitch", pitch);
                scene->setProperty("panelDefinition", edited);
                QTest::qWait(150);
                for (int step = 0; step < 8; ++step) {
                    scene->setProperty("wheelTravel", period * step / 8);
                    scene->setProperty("wheelTravelTarget", period * step / 8);
                    QTest::qWait(60);
                    // A View3D's GPU output follows the window render pass.
                    // Observe an actual submitted frame after the track edit
                    // before starting an independent offscreen item grab.
                    QSignalSpy completedFrame(&window, &QQuickWindow::frameSwapped);
                    window.update();
                    QVERIFY(completedFrame.wait(5000));
                    const auto projectionDiagnostic = [&]() {
                        QVariantMap values;
                        for (QObject *item : QList<QObject *>{scene, renderer, viewport}) {
                            QVariantMap fields;
                            for (const auto *key : {"width", "height", "platformScale", "cameraDistance", "platformTop",
                                "shownPitch", "shownYaw", "shownScale", "entryTravel", "layoutTrackRadius"})
                                if (item->property(key).isValid()) fields.insert(QLatin1String(key), plainValue(item->property(key)));
                            values.insert(item == scene ? "scene" : item == renderer ? "renderer" : "viewport", fields);
                        }
                        for (int i = 0; i < 2; ++i) {
                            auto *node = renderer->findChild<QObject *>(QStringLiteral("mesh-entry-%1").arg(i));
                            auto *glyph = renderer->findChild<QObject *>(QStringLiteral("mesh-glyph-%1").arg(i));
                            for (QObject *item : {node, glyph}) {
                                const auto pos = item->property("scenePosition").value<QVector3D>();
                                values.insert(item->objectName(), QVariantList{pos.x(), pos.y(), pos.z()});
                            }
                        }
                        return QString::fromUtf8(QJsonDocument::fromVariant(values).toJson(QJsonDocument::Compact));
                    };
                    QTRY_VERIFY2(projectionMatches(), qPrintable(QStringLiteral("travel pitch=%1 step=%2 period=%3 rects=%4 projected=%5 state=%6")
                        .arg(pitch).arg(step).arg(period)
                        .arg(QString::fromUtf8(QJsonDocument::fromVariant(plainValue(scene->property("entryRects"))).toJson(QJsonDocument::Compact)),
                             QString::fromUtf8(QJsonDocument::fromVariant(plainValue(renderer->property("projectedEntryGeometry"))).toJson(QJsonDocument::Compact)))
                        .arg(projectionDiagnostic())));
                    QVERIFY(renderer->property("rendererReady").toBool());
                    QCOMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"));
                    if (qEnvironmentVariableIsSet("ARCHDOCK_SCENE_CAPTURE_TRACE")) {
                        const auto before = window.grabWindow();
                        int coloured = 0;
                        for (int y = 0; y < before.height(); y += 2)
                            for (int x = 0; x < before.width(); x += 2)
                                coloured += before.pixelColor(x, y) != window.color();
                        qInfo() << "TRAVEL_WINDOW_BEFORE" << pitch << step << coloured
                                << "window" << window.size() << window.isExposed()
                                << "scene" << scene->position() << scene->scale() << scene->rotation();
                        if (!evidence.isEmpty()) QVERIFY(before.save(QDir(evidence).filePath(
                            QStringLiteral("travel-window-before-%1-%2.png").arg(pitch).arg(step))));
                    }
                    const auto travelled = pixels();
                    QVERIFY(!travelled.isNull());
                    int drawn = 0;
                    for (int y = 0; y < travelled.height(); y += 2)
                        for (int x = 0; x < travelled.width(); x += 2)
                            drawn += travelled.pixelColor(x, y).alpha() > 32;
                    if (drawn <= 250) {
                        if (!evidence.isEmpty()) QVERIFY(travelled.save(QDir(evidence).filePath("travel-empty.png")));
                        const auto windowFrame = window.grabWindow();
                        if (!evidence.isEmpty()) QVERIFY(windowFrame.save(QDir(evidence).filePath("travel-empty-window.png")));
                        QVariantMap diagnostic{{"pitch", pitch}, {"step", step}, {"period", period}, {"drawn", drawn}};
                        for (QObject *item : QList<QObject *>{scene, renderer, viewport}) {
                            QVariantMap fields;
                            for (const auto *key : {"visible", "opacity", "width", "height", "sceneConcealed",
                                "platformScale", "platformTop", "platformFold", "shownPitch", "rendererReady", "errorReason",
                                "entryTravel", "panelOpacity", "targetWidth", "targetHeight", "motionOpacity", "motionTracks"})
                                if (item->property(key).isValid()) fields.insert(QLatin1String(key), plainValue(item->property(key)));
                            diagnostic.insert(item == scene ? "scene" : item == renderer ? "renderer" : "viewport", fields);
                        }
                        qInfo().noquote() << "TRAVEL_EMPTY" << QJsonDocument::fromVariant(diagnostic).toJson(QJsonDocument::Compact);
                    }
                    QVERIFY2(drawn > 250, "the native platform must remain drawn throughout its track");
                    const auto rects = plainValue(renderer->property("projectedEntryGeometry")).toList();
                    for (int index = 0; index < rects.size(); ++index) {
                        const auto rect = rects[index].toMap();
                        QVariant visible;
                        QVERIFY(QMetaObject::invokeMethod(renderer, "entryPointVisible", Q_RETURN_ARG(QVariant, visible),
                            Q_ARG(QVariant, index), Q_ARG(QVariant, QVariant(QPointF(rect.value("centerX").toDouble(),
                                                                                rect.value("centerY").toDouble())))));
                        covered |= !visible.toBool(); exposed |= visible.toBool();
                    }
                    if (!evidence.isEmpty() && pitch == -60)
                        QVERIFY(travelled.save(QDir(evidence).filePath(QStringLiteral("travel-occlusion-%1.png").arg(step))));
                    QCOMPARE(scene->property("effectiveLayoutAngle").toDouble(), 0.0);
                }
            }
            QVERIFY2(covered && exposed, "native platform travel must expose front entries and exclude covered rear entries");
            edited.insert("panelRotationMode", "clockwise");
            edited.insert("panelRotationTrigger", "idle");
            edited.insert("panelTravelSpeed", 2.0);
            scene->setProperty("animationProfiles", QVariantMap{{"reducedMotion", false}});
            scene->setProperty("panelDefinition", edited);
            QTRY_VERIFY(scene->property("travelMotionActive").toBool());
            QTest::qWait(1200);
            QObject *stats = objectValue(viewport->property("renderStats"));
            QVERIFY(stats);
            qInfo().noquote() << "NATIVE_FRAME_TIME" << themeId
                << "intervalMs=" << stats->property("frameTime").toDouble()
                << "renderMs=" << stats->property("renderTime").toDouble()
                << "syncMs=" << stats->property("syncTime").toDouble()
                << "fps=" << stats->property("fps").toInt();
            edited.insert("panelRotationMode", "none");
            scene->setProperty("panelDefinition", edited);
            QTRY_VERIFY(!scene->property("travelMotionActive").toBool());
            QTRY_VERIFY(!scene->property("travelStepping").toBool());
            QVERIFY(QMetaObject::invokeMethod(scene, "resetTravel"));
            scene->setProperty("wheelTravel", 0.0); scene->setProperty("wheelTravelTarget", 0.0);
            scene->setProperty("panelDefinition", own);
            scene->setProperty("animationProfiles", QVariantMap{});
            QTest::qWait(150);
        }
        if (themeId == QStringLiteral("arc-platform-orange")) return;
        tiltedDefinition.remove(QStringLiteral("scene3DCameraPitch"));
        scene->setProperty("panelDefinition", tiltedDefinition);
        QVariantMap tiles = plainValue(scene->property("panelDefinition")).toMap();
        tiles.insert(QStringLiteral("iconTileMode"), QStringLiteral("custom"));
        tiles.insert(QStringLiteral("iconTileColor"), QStringLiteral("#ff22cc"));
        tiles.insert(QStringLiteral("iconTileOpacity"), 1.0);
        tiles.insert(QStringLiteral("iconTileBorderWidth"), 0.0);
        scene->setProperty("panelDefinition", tiles);
        QTest::qWait(100);
        const auto magentaPixels = [](const QImage &image) {
            int count = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const auto color = image.pixelColor(x, y);
                    count += color.red() > 200 && color.green() < 80 && color.blue() > 140;
                }
            return count;
        };
        const auto customTiles = pixels();
        QVERIFY(magentaPixels(customTiles) > 20);
        QTRY_VERIFY(projectionMatches());
        if (!evidence.isEmpty()) QVERIFY(customTiles.save(QDir(evidence).filePath("mesh-custom-tiles.png")));
        tiles.insert(QStringLiteral("iconShape"), QStringLiteral("circle"));
        scene->setProperty("panelDefinition", tiles);
        QTest::qWait(100);
        const auto circleTiles = pixels();
        QVERIFY(magentaPixels(circleTiles) > 20);
        QVERIFY(circleTiles != customTiles);
        QTRY_VERIFY(projectionMatches());
        tiles.insert(QStringLiteral("iconTilesEnabled"), false);
        scene->setProperty("panelDefinition", tiles);
        QTest::qWait(100);
        QCOMPARE(magentaPixels(pixels()), 0);
        // Restore the default appearance before the existing motion matrix.
        tiles.remove(QStringLiteral("iconTileMode"));
        tiles.remove(QStringLiteral("iconTileColor"));
        tiles.remove(QStringLiteral("iconTileOpacity"));
        tiles.remove(QStringLiteral("iconTileBorderWidth"));
        tiles.remove(QStringLiteral("iconTilesEnabled"));
        tiles.remove(QStringLiteral("iconShape"));
        scene->setProperty("panelDefinition", tiles);
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
            QTRY_VERIFY(projectionMatches());
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
        QTRY_VERIFY(projectionMatches());
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
        QObject *content = renderer->findChild<QObject *>(QStringLiteral("mesh-scene-content"));
        QVERIFY(content);
        const QVector3D openPosition = part->property("position").value<QVector3D>();
        const QVector3D openScale = content->property("scale").value<QVector3D>();
        scene->setProperty("runtimeState", QVariantMap{
            {QStringLiteral("presentationState"), QStringLiteral("collapsed")},
            {QStringLiteral("presentationProgress"), 1.0}});
        QTRY_COMPARE(part->property("openAmount").toDouble(), 0.0);
        QVERIFY(part->property("position").value<QVector3D>() != openPosition);
        // ADFIX UF-05: a radial collapse closes the whole platform toward its
        // centre, down to a small ring, instead of leaving it full size.
        QTRY_VERIFY(qAbs(content->property("scale").value<QVector3D>().x() - openScale.x() * 0.2f)
            < 1e-4f * qMax(1.0f, openScale.x()));
        scene->setProperty("runtimeState", QVariantMap{
            {QStringLiteral("presentationState"), QStringLiteral("open")},
            {QStringLiteral("presentationProgress"), 0.0}});
        QTRY_COMPARE(part->property("position").value<QVector3D>(), openPosition);
        QTRY_COMPARE(content->property("scale").value<QVector3D>(), openScale);

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
            QCOMPARE(renderer->property("triangleCount").toInt(), expectedTriangles);
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

    void platformTrackSpacingAndWheelSurface_data()
    {
        QTest::addColumn<QString>("themeId");
        QTest::addColumn<QStringList>("layouts");
        // Every layout each theme's catalogue entry allows for a closed track.
        QTest::newRow("cyan") << QStringLiteral("mesh-platform-cyan")
            << QStringList{QStringLiteral("ring:6"), QStringLiteral("circular:6"),
                           QStringLiteral("octagon:8"), QStringLiteral("polygon:4"),
                           QStringLiteral("polygon:3")};
        QTest::newRow("orange") << QStringLiteral("arc-platform-orange")
            << QStringList{QStringLiteral("circular:6"), QStringLiteral("ring:6")};
    }

    // Icons stand on the platform's own track in every allowed layout, the
    // canonical spacing regulates their separation there, and the wheel acts
    // on the bare platform between two icons and nowhere off the platform.
    void platformTrackSpacingAndWheelSurface()
    {
        QFETCH(QString, themeId);
        QFETCH(QStringList, layouts);
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // Needs the private RHI session, like the scene gate above.
        const QString themeRoot = qEnvironmentVariable("ARCHDOCK_RENDERING_STAGED_THEME_ROOT",
            QStringLiteral(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT));
        const auto package = ArchDock::ThemePackage::load(themeRoot
            + QStringLiteral("/") + themeId + QStringLiteral("/archdock-theme.json"));
        QVERIFY2(package.isValid(), qPrintable(package.primaryCode()));
        const QVariantMap theme = package.package->runtimeProjection();
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        QVERIFY(!glyphFixture.isEmpty());
        QQmlEngine engine;
        QStringList unexpectedWarnings;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings)
                unexpectedWarnings.append(warning.toString());
        });
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            PanelScene {
                required property string glyphFixture
                property string trackLayout: "ring"
                property int trackSides: 6
                property real trackSpacing: 8
                panelDefinition: ({rendererTier: "true3d", layout: trackLayout, pathSides: trackSides,
                                   layoutRadius: 120, scene3DQuality: "low", iconSize: 40,
                                   spacing: trackSpacing, layoutPadding: 10,
                                   panelMotionTarget: "panel"})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3, 4, 5, 6, 7].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/TrackConsumer.qml")));
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
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(renderer);
        QObject *viewport = objectValue(renderer->property("viewport"));
        QVERIFY(viewport);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 8);

        // The platform's flat top spans 0.72 to 0.94 of its scale; the icon
        // track is the ring at 0.84 of it.
        const double track = renderer->property("platformScale").toDouble() * 0.84;
        const double top = renderer->property("platformTop").toDouble();
        const auto entryPosition = [&](int index) {
            auto *node = renderer->findChild<QObject *>(QStringLiteral("mesh-entry-%1").arg(index));
            return node ? node->property("scenePosition").value<QVector3D>() : QVector3D();
        };
        const auto offTrack = [&]() -> QString {
            for (int index = 0; index < 8; ++index) {
                const QVector3D position = entryPosition(index);
                const double reach = std::hypot(position.x(), position.y());
                if (qAbs(reach - track) > 0.5 || position.z() <= top)
                    return QStringLiteral("entry %1 at radius %2 (track %3), height %4 (platform top %5)")
                        .arg(index).arg(reach).arg(track).arg(position.z()).arg(top);
            }
            return {};
        };
        const auto projectionMatches = [&]() {
            const auto rects = plainValue(scene->property("entryRects")).toList();
            const auto projected = plainValue(renderer->property("projectedEntryGeometry")).toList();
            if (rects.size() != 8 || projected.size() != 8) return false;
            for (int i = 0; i < 8; ++i)
                for (const auto &key : {"x", "y", "width", "height"})
                    if (qAbs(rects[i].toMap().value(key).toDouble()
                             - projected[i].toMap().value(key).toDouble()) > 1) return false;
            return true;
        };

        for (const QString &layout : std::as_const(layouts)) {
            const QStringList parts = layout.split(QLatin1Char(':'));
            scene->setProperty("trackSides", parts.value(1).toInt());
            scene->setProperty("trackLayout", parts.value(0));
            QTest::qWait(150);
            QCOMPARE(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"));
            QTRY_VERIFY2(offTrack().isEmpty(), qPrintable(layout + QStringLiteral(": ") + offTrack()));
            QTRY_VERIFY2(projectionMatches(), qPrintable(layout));
        }

        // Spacing: the default keeps the even ring; smaller values close the
        // icons up along the platform's own circle, down to touching.
        scene->setProperty("trackLayout", QStringLiteral("ring"));
        const auto neighbours = [&]() { return double((entryPosition(3) - entryPosition(4)).length()); };
        const double even = 2 * track * qSin(M_PI / 8);
        const double touching = 2 * track * qSin(40.0 / (2 * track));
        QTRY_VERIFY2(qAbs(neighbours() - even) < 0.5, qPrintable(QString::number(neighbours())));
        scene->setProperty("trackSpacing", 0.0);
        QTRY_VERIFY2(qAbs(neighbours() - touching) < 0.5,
            qPrintable(QStringLiteral("zero spacing: %1, expected %2").arg(neighbours()).arg(touching)));
        QVERIFY2(offTrack().isEmpty(), qPrintable(offTrack()));
        QTRY_VERIFY(projectionMatches());
        scene->setProperty("trackSpacing", 4.0);
        QTRY_VERIFY2(neighbours() > touching + 5 && neighbours() < even - 5,
            qPrintable(QString::number(neighbours())));
        QVERIFY2(offTrack().isEmpty(), qPrintable(offTrack()));
        scene->setProperty("trackSpacing", 30.0);
        QTRY_VERIFY2(qAbs(neighbours() - even) < 0.5, qPrintable(QString::number(neighbours())));
        QTRY_VERIFY(projectionMatches());

        // Wheel: the bare platform between two icons turns the scene both
        // ways; the hole and the transparent corner do not.
        const auto mapped = [&](const QVector3D &position) {
            QVector3D point;
            projectScenePoint(viewport, position, point);
            return QPointF(point.x(), point.y());
        };
        const auto wheel = [&](const QPointF &point, int delta) {
            QWheelEvent event(point, window.mapToGlobal(point.toPoint()), QPoint(), QPoint(0, delta),
                Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(&window, &event);
        };
        // Eight icons stand 45 degrees apart; halfway between two is bare.
        const QPointF surface = mapped(QVector3D(track * qCos(M_PI / 8), track * qSin(M_PI / 8), top));
        for (const QVariant &rect : plainValue(scene->property("entryRects")).toList())
            QVERIFY2(!rect.toRectF().isValid() || !rect.toRectF().contains(surface)
                     || !QRectF(rect.toMap().value("x").toDouble(), rect.toMap().value("y").toDouble(),
                                rect.toMap().value("width").toDouble(), rect.toMap().value("height").toDouble())
                            .contains(surface),
                     "the probe point is over an icon");
        QVariant accepted;
        QVERIFY(QMetaObject::invokeMethod(scene, "containsInputPoint", Q_RETURN_ARG(QVariant, accepted),
            Q_ARG(QVariant, QVariant(surface))));
        QVERIFY2(accepted.toBool(), "the bare platform must take input");
        QCOMPARE(scene->property("wheelRotationAngle").toDouble(), 0.0);
        wheel(surface, 120);
        QTRY_COMPARE(scene->property("wheelRotationAngle").toDouble(), 15.0);
        wheel(surface, -120);
        QTRY_COMPARE(scene->property("wheelRotationAngle").toDouble(), 0.0);
        wheel(surface, -120);
        QTRY_COMPARE(scene->property("wheelRotationAngle").toDouble(), 345.0);
        wheel(surface, 120);
        QTRY_COMPARE(scene->property("wheelRotationAngle").toDouble(), 0.0);
        const QPointF hole = mapped(QVector3D(0, 0, top));
        for (const QPointF &outside : {hole, QPointF(2, 2)}) {
            QVERIFY(QMetaObject::invokeMethod(scene, "containsInputPoint", Q_RETURN_ARG(QVariant, accepted),
                Q_ARG(QVariant, QVariant(outside))));
            QVERIFY2(!accepted.toBool(), "transparent desktop must pass through");
            wheel(outside, 120);
            QTest::qWait(60);
            QCOMPARE(scene->property("wheelRotationAngle").toDouble(), 0.0);
        }
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
    }

    // AD3D-TASK-002: the 3D page's transform scales, moves and rolls the
    // platform and its icons together, their input follows them, the view and
    // light settings reach the scene, and motion settings respect reduced
    // motion.
    void sceneTransformMovesPlatformIconsAndInputTogether()
    {
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // Needs the private RHI session, like the scene gates above.
        const QString themeRoot = qEnvironmentVariable("ARCHDOCK_RENDERING_STAGED_THEME_ROOT",
            QStringLiteral(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT));
        const auto package = ArchDock::ThemePackage::load(
            themeRoot + QStringLiteral("/mesh-platform-cyan/archdock-theme.json"));
        QVERIFY2(package.isValid(), qPrintable(package.primaryCode()));
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        QVERIFY(!glyphFixture.isEmpty());
        QQmlEngine engine;
        QStringList unexpectedWarnings;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings)
                unexpectedWarnings.append(warning.toString());
        });
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            PanelScene {
                required property string glyphFixture
                property real sceneScale: 1
                property real scenePositionX: 0
                property real sceneRoll: 0
                property bool sceneTransitions: false
                property bool sceneFloat: false
                property real sceneFieldOfView: 40
                property real sceneKeyLight: 1
                property bool reduced: false
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DScale: sceneScale, scene3DPositionX: scenePositionX,
                                   scene3DRoll: sceneRoll, scene3DTransitions: sceneTransitions,
                                   scene3DFloat: sceneFloat, scene3DFieldOfView: sceneFieldOfView,
                                   scene3DKeyLight: sceneKeyLight})
                animationProfiles: ({reducedMotion: reduced})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3, 4, 5, 6, 7].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/TransformConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), package.package->runtimeProjection()},
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
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(renderer);
        QObject *viewport = objectValue(renderer->property("viewport"));
        QObject *content = renderer->findChild<QObject *>(QStringLiteral("mesh-scene-content"));
        QVERIFY(viewport && content);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 8);

        const auto centres = [&]() {
            QList<QPointF> result;
            for (const QVariant &rect : plainValue(renderer->property("projectedEntryGeometry")).toList())
                result.append(QPointF(rect.toMap().value("centerX").toDouble(),
                                      rect.toMap().value("centerY").toDouble()));
            return result;
        };
        const auto centroid = [](const QList<QPointF> &points) {
            QPointF sum;
            for (const QPointF &point : points) sum += point;
            return points.isEmpty() ? sum : sum / points.size();
        };
        // Where each icon stands on the platform, as the camera sees it. The
        // icons stand upright on pedestals above it, so their centres move
        // with the platform's height when it is scaled; their feet show
        // where the transform keeps the platform.
        const auto feet = [&]() {
            QList<QPointF> result;
            for (int index = 0; index < 8; ++index) {
                QObject *node = renderer->findChild<QObject *>(QStringLiteral("mesh-entry-%1").arg(index));
                QVector3D view;
                if (node)
                    projectScenePoint(viewport, node->property("scenePosition").value<QVector3D>(), view);
                result.append(QPointF(view.x(), view.y()));
            }
            return result;
        };
        const auto spread = [&](const QList<QPointF> &points) {
            const QPointF middle = centroid(points);
            double reach = 0;
            for (const QPointF &point : points) reach = std::max(reach, QLineF(middle, point).length());
            return reach;
        };
        const auto projectionMatches = [&]() {
            const auto rects = plainValue(scene->property("entryRects")).toList();
            const auto projected = plainValue(renderer->property("projectedEntryGeometry")).toList();
            if (rects.size() != 8 || projected.size() != 8) return false;
            for (int i = 0; i < 8; ++i)
                for (const auto &key : {"x", "y", "width", "height"})
                    if (qAbs(rects[i].toMap().value(key).toDouble()
                             - projected[i].toMap().value(key).toDouble()) > 1) return false;
            return true;
        };
        // A point on the bare platform between two icons, wherever the
        // transform has put the platform.
        const double track = renderer->property("platformScale").toDouble() * 0.84;
        const double top = renderer->property("platformTop").toDouble();
        const auto surfacePoint = [&]() {
            QVector3D world;
            QMetaObject::invokeMethod(content, "mapPositionToScene", Q_RETURN_ARG(QVector3D, world),
                Q_ARG(QVector3D, QVector3D(track * qCos(M_PI / 8), track * qSin(M_PI / 8), top)));
            QVector3D view;
            projectScenePoint(viewport, world, view);
            return QPointF(view.x(), view.y());
        };
        const auto accepts = [&](const QPointF &point) {
            QVariant accepted;
            QMetaObject::invokeMethod(scene, "containsInputPoint", Q_RETURN_ARG(QVariant, accepted),
                Q_ARG(QVariant, QVariant(point)));
            return accepted.toBool();
        };

        const QList<QPointF> resting = centres();
        const QPointF restingMiddle = centroid(resting);
        const QPointF restingFeet = centroid(feet());
        const double restingSpread = spread(resting);
        const QPointF restingSurface = surfacePoint();
        QVERIFY(accepts(restingSurface));

        // Scale shrinks the platform and its icons about their centre.
        scene->setProperty("sceneScale", 0.6);
        QTRY_VERIFY2(qAbs(spread(centres()) - restingSpread * 0.6) < restingSpread * 0.05,
                     qPrintable(QStringLiteral("spread %1 of %2").arg(spread(centres())).arg(restingSpread)));
        QVERIFY2(QLineF(centroid(feet()), restingFeet).length() < 3,
                 qPrintable(QStringLiteral("feet centred at %1,%2, were %3,%4")
                     .arg(centroid(feet()).x()).arg(centroid(feet()).y())
                     .arg(restingFeet.x()).arg(restingFeet.y())));
        QTRY_VERIFY(projectionMatches());
        QVERIFY(accepts(surfacePoint()));
        QVERIFY2(!accepts(restingSurface), "the platform's old edge passes through once it shrinks");

        // Position moves them right, within the panel; input moves with them.
        scene->setProperty("scenePositionX", 1.0);
        QTRY_VERIFY(centroid(centres()).x() > restingMiddle.x() + 10);
        QTRY_VERIFY(projectionMatches());
        for (const QPointF &point : centres())
            QVERIFY2(point.x() > 0 && point.x() < scene->width(), "an icon left its panel");
        QVERIFY(accepts(surfacePoint()));

        // Roll turns the picture about the centre.
        scene->setProperty("scenePositionX", 0.0);
        scene->setProperty("sceneScale", 1.0);
        QTRY_VERIFY(qAbs(spread(centres()) - restingSpread) < restingSpread * 0.05);
        scene->setProperty("sceneRoll", 90.0);
        QTRY_VERIFY(QLineF(centroid(centres()), restingMiddle).length() < 3);
        const auto angleOf = [&](const QPointF &point, const QPointF &middle) {
            return qRadiansToDegrees(std::atan2(point.y() - middle.y(), point.x() - middle.x()));
        };
        QTRY_VERIFY2(qAbs(std::remainder(angleOf(centres().value(0), restingMiddle)
                                          - angleOf(resting.value(0), restingMiddle), 360.0)) > 75,
                     "entry 0 turned with the roll");
        QTRY_VERIFY(projectionMatches());
        scene->setProperty("sceneRoll", 0.0);

        // View and light settings reach the camera and the key light.
        scene->setProperty("sceneFieldOfView", 60.0);
        scene->setProperty("sceneKeyLight", 2.0);
        QObject *camera = objectValue(viewport->property("camera"));
        QVERIFY(camera);
        QTRY_COMPARE(camera->property("fieldOfView").toDouble(), 60.0);
        QTRY_COMPARE(renderer->findChild<QObject *>(QStringLiteral("mesh-key-light"))
                         ->property("brightness").toDouble(), 2.0);
        QTRY_VERIFY(projectionMatches());

        // Transitions animate a change only without reduced motion.
        scene->setProperty("sceneTransitions", true);
        scene->setProperty("sceneScale", 0.6);
        QTest::qWait(40);
        const double midway = renderer->property("shownScale").toDouble();
        QVERIFY2(midway > 0.6 + 1e-3 && midway < 1.0, qPrintable(QString::number(midway)));
        QTRY_COMPARE(renderer->property("shownScale").toDouble(), 0.6);
        scene->setProperty("reduced", true);
        scene->setProperty("sceneScale", 1.0);
        QCoreApplication::processEvents();
        QCOMPARE(renderer->property("shownScale").toDouble(), 1.0);

        // Float is off unless chosen and stops, at rest, under reduced motion.
        scene->setProperty("reduced", false);
        QTest::qWait(300);
        QCOMPARE(renderer->property("floatOffset").toDouble(), 0.0);
        scene->setProperty("sceneFloat", true);
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(renderer->property("floatOffset").toDouble()) > 0.01, 3000);
        scene->setProperty("reduced", true);
        QTRY_COMPARE(renderer->property("floatOffset").toDouble(), 0.0);
        QTest::qWait(200);
        QCOMPARE(renderer->property("floatOffset").toDouble(), 0.0);
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
    }

    // AD3D-TASK-002: on the desktop the panel's own handles edit its 3D
    // transform with the mouse. Each finished drag is reported once; a
    // cancelled drag leaves the scene and reports nothing; outside edit mode
    // there are no handles and a press changes nothing.
    // ADFIX-TASK-002 (UF-03, AUD-02, UF-09): a look without a 3D mesh of its
    // own stands on a platform generated along its own layout, in its own
    // colours, with upright icons on pedestals. Every supported shape, the
    // owner's baked blue ring and the steel octagon, drawn by the real RHI.
    void tileDepthMaterialsPlacementAndOrbitRenderUnderRhi();

    void generatedPlatformsFollowTheLayoutAndLook_data()
    {
        QTest::addColumn<QString>("layout");
        QTest::addColumn<QString>("themeId");
        QTest::addColumn<QString>("shape");
        for (const char *layout : {"circular", "ring", "ellipse"})
            QTest::newRow(layout) << QString::fromLatin1(layout) << QString() << QStringLiteral("ring");
        QTest::newRow("radial") << QStringLiteral("radial") << QString() << QStringLiteral("arc");
        for (const char *layout : {"triangle", "square", "pentagon", "hexagon", "octagon", "polygon"})
            QTest::newRow(layout) << QString::fromLatin1(layout) << QString() << QStringLiteral("polygon");
        QTest::newRow("baked blue ring") << QStringLiteral("ring")
            << QStringLiteral("ring-platform-blue") << QStringLiteral("ring");
        QTest::newRow("baked steel octagon") << QStringLiteral("octagon")
            << QStringLiteral("octagon-platform-steel") << QStringLiteral("polygon");
    }

    void generatedPlatformsFollowTheLayoutAndLook()
    {
        QFETCH(QString, layout);
        QFETCH(QString, themeId);
        QFETCH(QString, shape);
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // A generated platform is drawn only by the real RHI scene.
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        QVERIFY(!glyphFixture.isEmpty());

        // The look as PanelRegistry::genericScene3D projects it: a procedural
        // neon style, or a baked package with the palette of its artwork.
        QVariantMap theme{
            {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
            {QStringLiteral("version"), 2}, {QStringLiteral("valid"), true},
            {QStringLiteral("id"), QStringLiteral("procedural-look")},
            {QStringLiteral("name"), QStringLiteral("Procedural look")},
            {QStringLiteral("capabilities"), QVariantMap{
                {QStringLiteral("rendererTiers"), QVariantList{QStringLiteral("true3d"), QStringLiteral("procedural2d")}},
                {QStringLiteral("preferredRendererTier"), QStringLiteral("procedural2d")},
                {QStringLiteral("fallbackRendererTiers"), QVariantList{QStringLiteral("procedural2d")}}}}};
        QVariantMap generated;
        QVariantMap palette;
        if (!themeId.isEmpty())
        {
            const QString themeRoot = qEnvironmentVariable("ARCHDOCK_RENDERING_STAGED_THEME_ROOT",
                QStringLiteral(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT));
            const auto package = ArchDock::ThemePackage::load(themeRoot + QStringLiteral("/")
                + themeId + QStringLiteral("/archdock-theme.json"));
            QVERIFY2(package.isValid(), qPrintable(package.primaryCode()));
            theme = package.package->runtimeProjection();
            QVariantMap capabilities = theme.value(QStringLiteral("capabilities")).toMap();
            QVariantList tiers = capabilities.value(QStringLiteral("rendererTiers")).toList();
            tiers.prepend(QStringLiteral("true3d"));
            capabilities.insert(QStringLiteral("rendererTiers"), tiers);
            theme.insert(QStringLiteral("capabilities"), capabilities);
            const auto sampled = ArchDock::LookPalette::fromArtwork(theme);
            QVERIFY(sampled.has_value());
            palette = *sampled;
            generated.insert(QStringLiteral("palette"), palette);
        }
        theme.insert(QStringLiteral("scene3D"), QVariantMap{
            {QStringLiteral("generic"), true}, {QStringLiteral("generated"), generated},
            {QStringLiteral("fieldOfView"), 40}, {QStringLiteral("cameraPitch"), 0},
            {QStringLiteral("cameraYaw"), 0}, {QStringLiteral("keyLightBrightness"), 1.2},
            {QStringLiteral("fillLightBrightness"), 0.45},
            {QStringLiteral("defaultQuality"), QStringLiteral("high")},
            {QStringLiteral("transitions"), false}, {QStringLiteral("parts"), QVariantList{}}});
        theme.insert(QStringLiteral("scene3DResources"), QVariantMap{
            {QStringLiteral("material"), QVariantMap{
                {QStringLiteral("format"), QStringLiteral("org.archdock.material")},
                {QStringLiteral("version"), 1}, {QStringLiteral("baseColor"), QStringLiteral("#406080")},
                {QStringLiteral("emissiveColor"), QStringLiteral("#000000")},
                {QStringLiteral("emissiveStrength"), 0.0}, {QStringLiteral("metalness"), 0.3},
                {QStringLiteral("roughness"), 0.45}}},
            {QStringLiteral("parts"), QVariantList{}}, {QStringLiteral("indexBudget"), 262144}});

        QQmlEngine engine;
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            PanelScene {
                required property string glyphFixture
                required property string layoutName
                property real pitch: 0
                property real fold: 0
                property real thickness: 1
                property bool moving: false
                panelDefinition: ({rendererTier: "true3d", layout: layoutName, layoutRadius: 120,
                                   scene3DQuality: "high", iconSize: 40, layoutPadding: 10,
                                   pathSides: 7, appearance: "neon", scene3DCameraPitch: pitch,
                                   scene3DTransitions: false, scene3DFold: fold, scene3DThickness: thickness,
                                   panelMotionTarget: "items", panelRotationMode: moving ? "clockwise" : "none",
                                   panelTravelSpeed: 2, panelRotationTrigger: "idle"})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}})
                orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/GeneratedConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), theme},
            {QStringLiteral("layoutName"), layout},
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
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(renderer);
        QObject *viewport = objectValue(renderer->property("viewport"));
        QObject *content = renderer->findChild<QObject *>(QStringLiteral("mesh-scene-content"));
        QVERIFY(viewport && content);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 6);
        if (layout == QStringLiteral("circular")) {
            // A static production scene must settle after projection, even
            // when frame completion updates its input geometry.
            QSignalSpy idleFrames(&window, &QQuickWindow::frameSwapped);
            QTest::qWait(250);
            idleFrames.clear();
            QTest::qWait(1000);
            qInfo() << "NATIVE_STATIC_FRAMES" << idleFrames.count() << "over 1000 ms";
            QVERIFY2(idleFrames.count() <= 4,
                     "A settled static 3D scene must not continuously request frames");
        }

        // The platform is the generated one, of the layout's own shape.
        const QVariantMap platform = plainValue(renderer->property("generatedPlatform")).toMap();
        QCOMPARE(platform.value(QStringLiteral("shape")).toString(), shape);
        const double scale = renderer->property("platformScale").toDouble();
        const double top = renderer->property("platformTop").toDouble();
        const double track = platform.value(QStringLiteral("track")).toDouble();
        const double band = platform.value(QStringLiteral("band")).toDouble();

        // Every icon stands on the platform's flat top.
        const QVariantList positions = platform.value(QStringLiteral("positions")).toList();
        const QVariantList normals = platform.value(QStringLiteral("normals")).toList();
        const QVariantList indexes = platform.value(QStringLiteral("indexes")).toList();
        const int rimOffset = platform.value(QStringLiteral("rimOffset")).toInt();
        const auto vertex = [&](int index) {
            const QVariantList p = positions.value(indexes.value(index).toInt()).toList();
            return QPointF(p.value(0).toDouble(), p.value(1).toDouble());
        };
        const auto onTop = [&](const QPointF &point) {
            for (int i = 0; i + 2 < rimOffset; i += 3) {
                if (normals.value(indexes.value(i).toInt()).toList().value(2).toDouble() < 0.999)
                    continue;
                const QPointF a = vertex(i), b = vertex(i + 1), c = vertex(i + 2);
                const auto side = [](QPointF p, QPointF q, QPointF r) {
                    return (p.x() - r.x()) * (q.y() - r.y()) - (q.x() - r.x()) * (p.y() - r.y());
                };
                const double d1 = side(point, a, b), d2 = side(point, b, c), d3 = side(point, c, a);
                if (!((d1 < -1e-9 || d2 < -1e-9 || d3 < -1e-9) && (d1 > 1e-9 || d2 > 1e-9 || d3 > 1e-9)))
                    return true;
            }
            return false;
        };
        for (int index = 0; index < 6; ++index) {
            QObject *node = renderer->findChild<QObject *>(QStringLiteral("mesh-entry-%1").arg(index));
            QVERIFY(node);
            QVector3D local;
            QVERIFY(QMetaObject::invokeMethod(content, "mapPositionFromScene", Q_RETURN_ARG(QVector3D, local),
                Q_ARG(QVector3D, node->property("scenePosition").value<QVector3D>())));
            // The pedestal's whole footprint, not only its middle.
            const double footprint = 0.18 * 40 / scale;
            for (int step = 0; step <= 8; ++step) {
                const double x = local.x() / scale + (step < 8 ? footprint * std::cos(step * M_PI / 4) : 0);
                const double y = local.y() / scale + (step < 8 ? footprint * std::sin(step * M_PI / 4) : 0);
                QVERIFY2(onTop(QPointF(x, y)), qPrintable(QStringLiteral(
                    "%1 entry %2's pedestal reaches %3,%4, off the platform").arg(layout).arg(index)
                    .arg(x).arg(y)));
            }
            QVERIFY(local.z() > top);
        }

        const auto pixels = [&]() -> QImage {
            auto *view = qobject_cast<QQuickItem *>(viewport);
            const auto grab = view ? view->grabToImage() : nullptr;
            if (!grab) return {};
            QSignalSpy ready(grab.get(), &QQuickItemGrabResult::ready);
            return ready.wait(5000) ? grab->image() : QImage();
        };
        // A point of the platform's own frame (units of its scale) as drawn.
        const auto drawn = [&](double x, double y) {
            QVector3D world, view;
            QMetaObject::invokeMethod(content, "mapPositionToScene", Q_RETURN_ARG(QVector3D, world),
                Q_ARG(QVector3D, QVector3D(x * scale, y * scale, top)));
            projectScenePoint(viewport, world, view);
            return QPoint(qRound(view.x()), qRound(view.y()));
        };
        QTest::qWait(200);
        const QImage seen = pixels();
        QVERIFY(!seen.isNull());
        const auto at = [&](double x, double y) { return seen.pixelColor(drawn(x, y)); };
        const auto opaque = [&](double x, double y) { return at(x, y).alpha() > 200; };
        const auto clear = [&](double x, double y) { return at(x, y).alpha() < 32; };
        QVERIFY2(clear(0, 0), "the platform's middle is open");

        // The silhouette is the layout's shape, which a ring would not be.
        if (layout == QStringLiteral("ellipse")) {
            QVERIFY2(opaque(0, track * 0.62), "the ellipse's top edge");
            QVERIFY2(clear(0, track), "no circle above the ellipse");
            QVERIFY2(opaque(track, 0), "the ellipse's side");
        } else if (layout == QStringLiteral("radial")) {
            // The 300-degree arc opens on the left, where the layout's path does.
            QVERIFY2(clear(-track, 0), "the radial arc's gap is open");
            QVERIFY2(opaque(track, 0), "the radial arc's far side");
        } else if (shape == QStringLiteral("polygon")) {
            const int sides = platform.value(QStringLiteral("shape")).toString() == QStringLiteral("polygon")
                ? (layout == QStringLiteral("triangle") ? 3 : layout == QStringLiteral("square") ? 4
                   : layout == QStringLiteral("pentagon") ? 5 : layout == QStringLiteral("hexagon") ? 6
                   : layout == QStringLiteral("octagon") ? 8 : 7) : 0;
            const double mitre = 1 / std::cos(M_PI / sides);
            const double corner = track + (band + 0.05) * mitre;
            const double edge = track * std::cos(M_PI / sides) + band + 0.05;
            const double probe = (corner + edge) / 2;
            // Every corner, the first straight up, reaches past the probe and
            // no edge's middle does: exactly this many sides, this way up.
            for (int k = 0; k < sides; ++k) {
                const double vertexAngle = M_PI / 2 - k * 2 * M_PI / sides;
                const double edgeAngle = vertexAngle - M_PI / sides;
                QVERIFY2(opaque(probe * std::cos(vertexAngle), probe * std::sin(vertexAngle)),
                         qPrintable(QStringLiteral("%1 corner %2 at %3").arg(layout).arg(k).arg(probe)));
                QVERIFY2(clear(probe * std::cos(edgeAngle), probe * std::sin(edgeAngle)),
                         qPrintable(QStringLiteral("%1 edge %2 middle at %3").arg(layout).arg(k).arg(probe)));
            }
        } else {
            for (int step = 0; step < 8; ++step)
                QVERIFY(opaque(track * std::cos(step * M_PI / 4 + 0.2), track * std::sin(step * M_PI / 4 + 0.2)));
        }

        // The look's own colours: between the icons, on the platform's top.
        if (!themeId.isEmpty() || layout == QStringLiteral("ring")) {
            double r = 0, g = 0, b = 0;
            int count = 0;
            const QVariantList rects = plainValue(renderer->property("projectedEntryGeometry")).toList();
            for (int step = 0; step < 24; ++step) {
                const double angle = step * M_PI / 12 + 0.13;
                double x = track * std::cos(angle), y = track * std::sin(angle);
                if (shape == QStringLiteral("polygon")) {
                    // Along the polygon's own centre line, half way along each side.
                    const int sides = layout == QStringLiteral("octagon") ? 8 : 7;
                    const double side = std::floor(step / 3.0);
                    const double a0 = M_PI / 2 - side * 2 * M_PI / sides;
                    const double a1 = a0 - 2 * M_PI / sides;
                    const double t = (step % 3 + 1) / 4.0;
                    x = track * ((1 - t) * std::cos(a0) + t * std::cos(a1));
                    y = track * ((1 - t) * std::sin(a0) + t * std::sin(a1));
                }
                const QPoint point = drawn(x, y);
                bool covered = false;
                for (const QVariant &value : rects) {
                    const QVariantMap rect = value.toMap();
                    covered |= QRectF(rect.value("x").toDouble() - 4, rect.value("y").toDouble() - 4,
                                      rect.value("width").toDouble() + 8,
                                      rect.value("height").toDouble() + 8).contains(point);
                }
                const QColor colour = seen.pixelColor(point);
                if (covered || colour.alpha() < 200) continue;
                r += colour.redF(); g += colour.greenF(); b += colour.blueF();
                ++count;
            }
            QVERIFY2(count >= 6, qPrintable(QStringLiteral("%1 platform samples").arg(count)));
            const QColor body = QColor::fromRgbF(r / count, g / count, b / count);
            if (themeId == QStringLiteral("ring-platform-blue")) {
                // Dark teal like its artwork, not the cyan mesh's light steel blue.
                QVERIFY2(std::abs(body.hslHueF() - QColor(QStringLiteral("#123c52")).hslHueF()) < 0.06
                         && body.lightnessF() < 0.4, qPrintable(body.name()));
                QVERIFY2(QColor(QStringLiteral("#7098ae")).lightnessF() - body.lightnessF() > 0.2,
                         qPrintable(body.name()));
            } else if (themeId == QStringLiteral("octagon-platform-steel")) {
                QVERIFY2(body.hslSaturationF() < 0.3, qPrintable(body.name()));
            } else {
                // The neon look's own cyan stroke colour.
                QVERIFY2(std::abs(body.hslHueF() - QColor(QStringLiteral("#50e6ff")).hslHueF()) < 0.05,
                         qPrintable(body.name()));
            }
        }
        const QString evidence = qEnvironmentVariable("ARCHDOCK_SCENE_EVIDENCE_DIR");
        const QString tag = QString::fromLatin1(QTest::currentDataTag()).replace(QLatin1Char(' '), QLatin1Char('-'));
        if (!evidence.isEmpty()) {
            QVERIFY(QDir().mkpath(evidence + QStringLiteral("/generated")));
            QVERIFY(seen.save(evidence + QStringLiteral("/generated/") + tag + QStringLiteral("-top.png")));
        }

        // Tilted: icons face the viewer and stand on pedestals above the top.
        const QVariant flatRects = plainValue(renderer->property("projectedEntryGeometry"));
        scene->setProperty("pitch", 45.0);
        QTRY_COMPARE(renderer->property("shownPitch").toDouble(), 45.0);
        // The input rectangles follow the tilt once a frame has drawn it.
        QTRY_VERIFY(plainValue(renderer->property("projectedEntryGeometry")) != flatRects);
        QTest::qWait(200);
        const QImage tilted = pixels();
        QVERIFY(!tilted.isNull());
        if (!evidence.isEmpty())
            QVERIFY(tilted.save(evidence + QStringLiteral("/generated/") + tag + QStringLiteral("-tilted.png")));
        const QVariantList rects = plainValue(renderer->property("projectedEntryGeometry")).toList();
        QCOMPARE(rects.size(), 6);
        for (int index = 0; index < 6; ++index) {
            const QVariantMap rect = rects.value(index).toMap();
            const double width = rect.value("width").toDouble(), height = rect.value("height").toDouble();
            QVERIFY2(width / height > 0.8 && width / height < 1.25, qPrintable(QStringLiteral(
                "entry %1 is drawn %2 x %3, not facing the viewer").arg(index).arg(width).arg(height)));
            QVERIFY2(height > 40 * 0.6, qPrintable(QStringLiteral("entry %1 is %2 high").arg(index).arg(height)));
            QObject *node = renderer->findChild<QObject *>(QStringLiteral("mesh-entry-%1").arg(index));
            QObject *anchor = renderer->findChild<QObject *>(QStringLiteral("mesh-input-anchor-%1").arg(index));
            QVERIFY(node && anchor);
            QVector3D local, world, foot, base;
            QMetaObject::invokeMethod(content, "mapPositionFromScene", Q_RETURN_ARG(QVector3D, local),
                Q_ARG(QVector3D, node->property("scenePosition").value<QVector3D>()));
            QMetaObject::invokeMethod(content, "mapPositionToScene", Q_RETURN_ARG(QVector3D, world),
                Q_ARG(QVector3D, QVector3D(local.x(), local.y(), top)));
            projectScenePoint(viewport, world, foot);
            // The icon's bottom edge, where it stands.
            QMetaObject::invokeMethod(anchor, "mapPositionToScene", Q_RETURN_ARG(QVector3D, world),
                Q_ARG(QVector3D, QVector3D(0, -20, 0)));
            projectScenePoint(viewport, world, base);
            const double bottom = base.y();
            // Above the platform where it stands, not sunk into it ...
            QVERIFY2(bottom < foot.y(), qPrintable(QStringLiteral(
                "entry %1 reaches %2, its platform point is at %3").arg(index).arg(bottom).arg(foot.y())));
            // ... and held up by a pedestal right under it.
            const QPoint under(qRound((base.x() + foot.x()) / 2), qRound((bottom + foot.y()) / 2));
            QVERIFY2(tilted.pixelColor(under).alpha() > 200, qPrintable(QStringLiteral(
                "entry %1 has nothing under it at %2,%3").arg(index).arg(under.x()).arg(under.y())));
        }
        // ADREP-TASK-004: all generated outlines fold their actual GPU mesh;
        // the entry feet and input projection move with it, then reset.
        const int triangles = renderer->property("triangleCount").toInt();
        const auto unfolded = plainValue(renderer->property("projectedEntryGeometry"));
        for (double fold : {0.75, -0.75}) {
            scene->setProperty("fold", fold);
            QTRY_COMPARE(renderer->property("platformFold").toDouble(), fold);
            QTRY_VERIFY(plainValue(renderer->property("projectedEntryGeometry")) != unfolded);
            QTest::qWait(150);
            const auto folded = pixels();
            QVERIFY(!folded.isNull() && folded != tilted);
            QCOMPARE(renderer->property("triangleCount").toInt(), triangles);
            if (!evidence.isEmpty()) QVERIFY(folded.save(evidence + "/generated/" + tag
                + QStringLiteral("-fold-%1.png").arg(fold)));
        }
        scene->setProperty("fold", 0.0);
        QTRY_COMPARE(renderer->property("platformFold").toDouble(), 0.0);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")), unfolded);
        QSignalSpy resetFrame(&window, &QQuickWindow::frameSwapped);
        window.update(); QVERIFY(resetFrame.wait(5000));
        QTest::mouseMove(&window, QPoint(0, 0));
        const auto wheelRects = plainValue(renderer->property("projectedEntryGeometry")).toList();
        const auto wheelRect = wheelRects[2].toMap();
        const QPointF wheelPoint(wheelRect.value("centerX").toDouble(), wheelRect.value("centerY").toDouble());
        QWheelEvent wheel(wheelPoint, window.mapToGlobal(wheelPoint.toPoint()), QPoint(), QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&window, &wheel);
        QTRY_COMPARE(scene->property("entryTravel").toDouble(), 1.0);
        QCOMPARE(scene->property("effectiveLayoutAngle").toDouble(), 0.0);
        scene->setProperty("moving", true);
        QTRY_VERIFY(scene->property("travelMotionActive").toBool());
        const double started = scene->property("entryTravel").toDouble();
        QTRY_VERIFY(scene->property("entryTravel").toDouble() > started + 0.2);
        QCOMPARE(scene->property("effectiveLayoutAngle").toDouble(), 0.0);
        if (layout == QStringLiteral("circular")) {
            QTest::qWait(1200);
            QObject *stats = objectValue(viewport->property("renderStats"));
            QVERIFY(stats);
            qInfo().noquote() << "NATIVE_FRAME_TIME generated-circle"
                << "intervalMs=" << stats->property("frameTime").toDouble()
                << "renderMs=" << stats->property("renderTime").toDouble()
                << "syncMs=" << stats->property("syncTime").toDouble()
                << "fps=" << stats->property("fps").toInt();
        }
        scene->setProperty("moving", false);
        QTRY_VERIFY(!scene->property("travelStepping").toBool());
        scene->setProperty("wheelTravel", 0.0); scene->setProperty("wheelTravelTarget", 0.0);
        if (layout == QStringLiteral("circular") || !themeId.isEmpty()) {
            bool hidden = false, visible = false;
            const double period = plainValue(scene->property("trackWindow")).toMap().value("loop").toDouble();
            scene->setProperty("thickness", 4.0);
            for (double pitch : {45.0, -60.0}) {
                scene->setProperty("pitch", pitch);
                for (int step = 0; step <= 8; ++step) {
                    scene->setProperty("wheelTravel", period * step / 8);
                    scene->setProperty("wheelTravelTarget", period * step / 8);
                    QSignalSpy swapped(&window, &QQuickWindow::frameSwapped);
                    window.update(); QVERIFY(swapped.wait(5000));
                    const auto picture = pixels();
                    QVERIFY(!picture.isNull());
                    QVERIFY(renderer->property("rendererReady").toBool());
                    if (!evidence.isEmpty()) QVERIFY(picture.save(evidence + "/generated/" + tag
                        + QStringLiteral("-travel-%1-%2.png").arg(pitch).arg(step)));
                    const auto projected = plainValue(renderer->property("projectedEntryGeometry")).toList();
                    for (int i = 0; i < projected.size(); ++i) {
                        const auto rect = projected[i].toMap();
                        const QPointF point(rect.value("centerX").toDouble(), rect.value("centerY").toDouble());
                        if (point.x() < 0 || point.y() < 0 || point.x() >= picture.width() || point.y() >= picture.height()) continue;
                        QVariant accepted;
                        QVERIFY(QMetaObject::invokeMethod(renderer, "entryPointVisible", Q_RETURN_ARG(QVariant, accepted),
                            Q_ARG(QVariant, i), Q_ARG(QVariant, QVariant(point))));
                        hidden |= !accepted.toBool(); visible |= accepted.toBool();
                    }
                }
            }
            QVERIFY2(hidden && visible, qPrintable(tag + " must expose front glyphs and exclude rear ones"));
        }
    }

    void gizmoEditsTheTransformWithTheMouse()
    {
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // Needs the private RHI session, like the scene gates above.
        const QString themeRoot = qEnvironmentVariable("ARCHDOCK_RENDERING_STAGED_THEME_ROOT",
            QStringLiteral(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT));
        const auto package = ArchDock::ThemePackage::load(
            themeRoot + QStringLiteral("/mesh-platform-cyan/archdock-theme.json"));
        QVERIFY2(package.isValid(), qPrintable(package.primaryCode()));
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        QQmlEngine engine;
        QStringList unexpectedWarnings;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &warnings) {
            for (const auto &warning : warnings)
                unexpectedWarnings.append(warning.toString());
        });
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            PanelScene {
                required property string glyphFixture
                property bool editing: false
                property real pitch: 25
                property real yaw: 10
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DScale: 0.9, scene3DTransitions: false,
                                   scene3DCameraPitch: pitch, scene3DCameraYaw: yaw})
                sceneEditActive: editing
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/GizmoConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), package.package->runtimeProjection()},
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
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QObject *viewport = renderer ? objectValue(renderer->property("viewport")) : nullptr;
        QVERIFY(renderer && viewport);
        QSignalSpy edits(scene, SIGNAL(sceneTransformEdited(QVariant)));
        QVERIFY(edits.isValid());
        const auto editedValues = [&](int index) {
            const QVariant value = edits.at(index).at(0);
            return value.metaType() == QMetaType::fromType<QJSValue>()
                ? value.value<QJSValue>().toVariant().toMap() : value.toMap();
        };
        const auto screenPoint = [&](const QString &name) -> QPoint {
            QObject *handle = renderer->findChild<QObject *>(name);
            if (!handle) return {};
            QVector3D view;
            projectScenePoint(viewport, handle->property("scenePosition").value<QVector3D>(), view);
            return scene->mapToScene(QPointF(view.x(), view.y())).toPoint();
        };
        const auto drag = [&](const QPoint &from, const QPoint &to, Qt::KeyboardModifiers modifiers = {}) {
            QTest::mousePress(&window, Qt::LeftButton, modifiers, from);
            for (int step = 1; step <= 5; ++step)
                QTest::mouseMove(&window, from + (to - from) * step / 5);
            QTest::mouseRelease(&window, Qt::LeftButton, modifiers, to);
        };
        QObject *gizmo = renderer->findChild<QObject *>(QStringLiteral("mesh-gizmo"));
        QVERIFY(gizmo);

        // Outside edit mode: no handles, and a press changes nothing.
        QVERIFY(!gizmo->property("visible").toBool());
        const QPoint centre = screenPoint(QStringLiteral("mesh-gizmo"));
        drag(centre, centre + QPoint(40, 0));
        QTest::qWait(100);
        QCOMPARE(edits.count(), 0);

        scene->setProperty("editing", true);
        QTRY_VERIFY(renderer->property("editMode").toBool());
        QTRY_VERIFY(gizmo->property("visible").toBool());
        QTest::qWait(150);

        // Move: the red arrow carries the platform along X.
        const QPoint tip = screenPoint(QStringLiteral("gizmo-move-x-tip"));
        const QPoint middle = screenPoint(QStringLiteral("mesh-gizmo"));
        QVERIFY(!tip.isNull());
        const QPointF along = QPointF(tip - middle) / std::max(1.0, QLineF(middle, tip).length());
        drag(tip, tip + (along * 40).toPoint());
        QTRY_COMPARE(edits.count(), 1);
        const double movedX = editedValues(0).value("scene3DPositionX").toDouble();
        QVERIFY2(movedX > 0.01, qPrintable(QString::number(movedX)));
        QCOMPARE(editedValues(0).keys(), QStringList{QStringLiteral("scene3DPositionX")});
        // Picking answers from the last rendered frame: let the moved handle
        // be drawn before pressing it again.
        QTest::qWait(150);

        // Ctrl snaps the move to steps of 0.05. Back toward the centre, so the
        // handle stays inside the panel whatever room it has.
        drag(screenPoint(QStringLiteral("gizmo-move-x-tip")),
             screenPoint(QStringLiteral("gizmo-move-x-tip")) - (along * 23).toPoint(), Qt::ControlModifier);
        QTRY_COMPARE(edits.count(), 2);
        const double snappedX = editedValues(1).value("scene3DPositionX").toDouble();
        QVERIFY2(qAbs(snappedX / 0.05 - qRound(snappedX / 0.05)) < 1e-6, qPrintable(QString::number(snappedX)));
        QTest::qWait(150);

        // Rotate: the white ring turns the view.
        renderer->setProperty("gizmoMode", QStringLiteral("rotate"));
        QTest::qWait(150);
        const QPoint ringCentre = screenPoint(QStringLiteral("mesh-gizmo"));
        const QPoint onRing = screenPoint(QStringLiteral("gizmo-rotate-view-0"));
        QVERIFY(!onRing.isNull());
        const QPointF radius = QPointF(onRing - ringCentre);
        const double turn = qDegreesToRadians(30.0);
        const QPointF turned(radius.x() * qCos(turn) - radius.y() * qSin(turn),
                             radius.x() * qSin(turn) + radius.y() * qCos(turn));
        drag(onRing, ringCentre + turned.toPoint());
        QTRY_COMPARE(edits.count(), 3);
        const double roll = editedValues(2).value("scene3DRoll").toDouble();
        QVERIFY2(qAbs(qAbs(roll) - 30) < 6, qPrintable(QString::number(roll)));
        QTest::qWait(150);

        // Scale: dragging a cube handle outward enlarges the platform.
        renderer->setProperty("gizmoMode", QStringLiteral("scale"));
        QTest::qWait(150);
        const QPoint cube = screenPoint(QStringLiteral("gizmo-scale-x-tip"));
        const QPoint scaleCentre = screenPoint(QStringLiteral("mesh-gizmo"));
        const QPointF outward = QPointF(cube - scaleCentre) / std::max(1.0, QLineF(scaleCentre, cube).length());
        drag(cube, cube + (outward * 20).toPoint());
        QTRY_COMPARE(edits.count(), 4);
        QVERIFY(editedValues(3).value("scene3DScale").toDouble() > 0.9);
        QTest::qWait(150);

        // A right button cancels the drag in progress: nothing is reported
        // and the scene shows the saved values again.
        const QPoint again = screenPoint(QStringLiteral("gizmo-scale-x-tip"));
        QTest::mousePress(&window, Qt::LeftButton, {}, again);
        QTest::mouseMove(&window, again + (outward * 15).toPoint());
        QVERIFY(renderer->property("gizmoDragging").toBool());
        QTest::mousePress(&window, Qt::RightButton, {}, again + (outward * 15).toPoint());
        QTest::mouseRelease(&window, Qt::RightButton, {}, again + (outward * 15).toPoint());
        QTest::mouseRelease(&window, Qt::LeftButton, {}, again + (outward * 15).toPoint());
        QTest::qWait(100);
        QCOMPARE(edits.count(), 4);
        QVERIFY(!renderer->property("gizmoDragging").toBool());
        QVERIFY(renderer->property("dragOverride").isNull()
                || !renderer->property("dragOverride").isValid()
                || plainValue(renderer->property("dragOverride")).isNull());

        // ADFIX UF-07: dragging the platform's body, away from every handle
        // and icon, turns it across and tilts it up and down.
        renderer->setProperty("gizmoMode", QStringLiteral("move"));
        QTest::qWait(150);
        QObject *content = renderer->findChild<QObject *>(QStringLiteral("mesh-scene-content"));
        QVERIFY(content);
        const auto bodyPoint = [&]() {
            // On the track between the icons at -30 and +30 degrees.
            const double track = renderer->property("platformScale").toDouble() * 0.84;
            QVector3D world, view;
            QMetaObject::invokeMethod(content, "mapPositionToScene", Q_RETURN_ARG(QVector3D, world),
                Q_ARG(QVector3D, QVector3D(track, 0, renderer->property("platformTop").toDouble())));
            projectScenePoint(viewport, world, view);
            return scene->mapToScene(QPointF(view.x(), view.y())).toPoint();
        };
        const double yawBefore = renderer->property("targetYaw").toDouble();
        const double pitchBefore = renderer->property("targetPitch").toDouble();
        drag(bodyPoint(), bodyPoint() + QPoint(40, 0));
        QTRY_COMPARE(edits.count(), 5);
        QVERIFY2(qAbs(editedValues(4).value("scene3DCameraYaw").toDouble() - (yawBefore + 10)) < 1.5,
                 qPrintable(QString::number(editedValues(4).value("scene3DCameraYaw").toDouble())));
        QVERIFY(!editedValues(4).contains("scene3DPositionX"));
        QTest::qWait(150);
        // Ctrl snaps the tilt to 15 degrees; Shift drags a tenth as far.
        drag(bodyPoint(), bodyPoint() + QPoint(0, -30), Qt::ControlModifier);
        QTRY_COMPARE(edits.count(), 6);
        const double tilted = editedValues(5).value("scene3DCameraPitch").toDouble();
        QVERIFY2(qAbs(tilted / 15 - qRound(tilted / 15)) < 1e-6 && tilted != pitchBefore,
                 qPrintable(QString::number(tilted)));
        QTest::qWait(150);
        const double yawNow = renderer->property("targetYaw").toDouble();
        drag(bodyPoint(), bodyPoint() + QPoint(40, 0), Qt::ShiftModifier);
        QTRY_COMPARE(edits.count(), 7);
        QVERIFY2(qAbs(editedValues(6).value("scene3DCameraYaw").toDouble() - (yawNow + 1)) < 0.5,
                 qPrintable(QString::number(editedValues(6).value("scene3DCameraYaw").toDouble())));
        QTest::qWait(150);

        // ADFIX AUD-04: seen from straight above, the Z arrow points at the
        // viewer. It says so, and dragging it up still moves the platform.
        scene->setProperty("pitch", 0.0);
        scene->setProperty("yaw", 0.0);
        // The drags above stand in for the saved values until those come back
        // or a few seconds pass; this host never saves them.
        QTRY_COMPARE_WITH_TIMEOUT(renderer->property("shownPitch").toDouble(), 0.0, 6000);
        QTRY_COMPARE_WITH_TIMEOUT(renderer->property("shownYaw").toDouble(), 0.0, 6000);
        QTRY_VERIFY(plainValue(renderer->property("endOnAxes")).toStringList().contains(QStringLiteral("z")));
        QObject *hint = renderer->findChild<QObject *>(QStringLiteral("mesh-gizmo-hint"));
        QVERIFY(hint);
        QTRY_VERIFY2(hint->property("text").toString().contains(QStringLiteral("Z arrow")),
                     qPrintable(hint->property("text").toString()));
        QTest::qWait(150);
        const QPoint zTip = screenPoint(QStringLiteral("gizmo-move-z-tip"));
        QVERIFY(!zTip.isNull());
        drag(zTip, zTip + QPoint(0, -60));
        QTRY_COMPARE(edits.count(), 8);
        QVERIFY2(editedValues(7).value("scene3DPositionZ").toDouble() > 0.01,
                 qPrintable(QString::number(editedValues(7).value("scene3DPositionZ").toDouble())));

        // Leaving edit mode removes the handles.
        scene->setProperty("editing", false);
        QTRY_VERIFY(!gizmo->property("visible").toBool());
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
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
            {QStringLiteral("sceneResources"), QVariantMap{}},
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

void RendererCapabilityTest::tileDepthMaterialsPlacementAndOrbitRenderUnderRhi()
{
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
        return; // Executed by the private KWin installed-module harness.
    QQmlEngine engine;
    engine.addImportPath(importRoot());
    QStringList warnings;
    connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &errors) {
        for (const auto &error : errors) warnings.append(error.toString());
    });
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import ArchDock.Rendering 1.0
        PanelScene {
            property var tileValues: ({})
            panelDefinition: Object.assign({edge: "free", type: "launcher", rendererTier: "true3d",
                layout: "circular", layoutRadius: 100, iconSize: 52, appearance: "minimal",
                scene3DCameraPitch: 25, scene3DTransitions: false, scene3DQuality: "high",
                iconShape: "circle", iconTilesEnabled: true, iconTileMode: "custom",
                iconTileColor: "#7895b0", iconTileOpacity: 1, iconTileBorderWidth: 1,
                pathOrientation: "upright", panelMotionTarget: "items"}, tileValues)
            entryDelegateContext: ({hostKind: "free"})
            hostCapabilities: ({rotation: {available: true}})
            orderedEntries: [0,1,2,3].map(function(i) {
                return {id:"app-"+i, iconName:"file:///usr/share/icons/hicolor/scalable/apps/firefox.svg"}
            })
        }
    )", QUrl::fromLocalFile(importRoot() + "/TileDepthConsumer.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantMap theme{{"format", "org.archdock.theme"}, {"version", 2}, {"valid", true},
        {"id", "tile-test-platform"}, {"name", "Tile test platform"},
        {"capabilities", QVariantMap{{"rendererTiers", QVariantList{"true3d", "procedural2d"}},
            {"preferredRendererTier", "procedural2d"}, {"fallbackRendererTiers", QVariantList{"procedural2d"}}}},
        {"scene3D", QVariantMap{{"generic", true}, {"generated", QVariantMap{}}, {"parts", QVariantList{}},
            {"transitions", false}, {"fieldOfView", 40}, {"keyLightBrightness", 1.2}, {"fillLightBrightness", 0.45}}},
        {"scene3DResources", QVariantMap{{"material", QVariantMap{{"format", "org.archdock.material"},
            {"version", 1}, {"baseColor", "#406080"}, {"roughness", 0.45}, {"metalness", 0.3}}},
            {"parts", QVariantList{}}, {"indexBudget", 262144}}}};
    std::unique_ptr<QObject> object(component.createWithInitialProperties({{"themeDefinition", theme}}));
    QVERIFY2(object, qPrintable(component.errorString()));
    auto *scene = qobject_cast<QQuickItem *>(object.get()); QVERIFY(scene);
    QQuickWindow window;
    scene->setParentItem(window.contentItem());
    window.resize(qCeil(scene->width()), qCeil(scene->height()));
    window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"), 5000);
    auto *renderer = objectValue(scene->property("activeSurfaceRenderer")); QVERIFY(renderer);
    auto *view = qobject_cast<QQuickItem *>(objectValue(renderer->property("viewport"))); QVERIFY(view);
    const QString evidence = qEnvironmentVariable("ARCHDOCK_SCENE_EVIDENCE_DIR");
    if (!evidence.isEmpty()) QVERIFY(QDir().mkpath(evidence + "/tiles"));
    const auto capture = [&](const QString &name) {
        QTest::qWait(100);
        const auto grab = view->grabToImage();
        if (!grab) return QImage{};
        QSignalSpy ready(grab.get(), &QQuickItemGrabResult::ready);
        if (!ready.wait(5000)) return QImage{};
        const QImage image = grab->image();
        if (!evidence.isEmpty() && !image.save(evidence + "/tiles/" + name + ".png")) return QImage{};
        return image;
    };
    const auto changed = [](const QImage &a, const QImage &b) {
        if (a.size() != b.size() || a.isNull() || b.isNull()) return -1;
        int count = 0;
        for (int y = 0; y < a.height(); ++y) for (int x = 0; x < a.width(); ++x) {
            const QColor p = a.pixelColor(x,y), q = b.pixelColor(x,y);
            if (qAbs(p.red()-q.red())+qAbs(p.green()-q.green())+qAbs(p.blue()-q.blue())
                    +qAbs(p.alpha()-q.alpha()) > 12) ++count;
        }
        return count;
    };
    QTest::qWait(200);
    const QImage flat = capture("flat"); QVERIFY(!flat.isNull());
    QVariantMap values{{"iconTileThickness", 8.0}, {"iconTileBevel", 3.0}};
    scene->setProperty("tileValues", values);
    const QImage solid = capture("solid"); QVERIFY(changed(flat,solid) >= 20);
    auto *tile = renderer->findChild<QObject *>("mesh-tile-solid-0"); QVERIFY(tile);
    const auto mesh = plainValue(tile->property("meshData")).toMap();
    double minimumZ = 0;
    for (const QVariant &point : mesh.value("positions").toList())
        minimumZ = qMin(minimumZ, point.toList().value(2).toDouble());
    QVERIFY(qAbs(minimumZ * tile->property("scale").value<QVector3D>().z() + 8) < 0.001);
    QVERIFY(renderer->property("geometryWithinBudget").toBool());
    for (const QString &shape : {QStringLiteral("rounded"), QStringLiteral("square"), QStringLiteral("squircle"),
            QStringLiteral("circle"), QStringLiteral("hexagon"), QStringLiteral("diamond")}) {
        values.insert("iconShape", shape); scene->setProperty("tileValues", values);
        QVERIFY(!capture("shape-" + shape).isNull());
        QVERIFY(plainValue(tile->property("meshData")).toMap().value("indexes").toList().size() > 12);
    }
    values.insert("iconShape", "circle"); scene->setProperty("tileValues", values);
    const QImage baseline = capture("baseline"); QVERIFY(!baseline.isNull());
    for (const QString &look : {QStringLiteral("glass"), QStringLiteral("crystal"), QStringLiteral("neon"),
            QStringLiteral("minimal"), QStringLiteral("plasma"), QStringLiteral("lime"), QStringLiteral("floating-glass"),
            QStringLiteral("metallic"), QStringLiteral("futuristic"), QStringLiteral("organic"),
            QStringLiteral("platform"), QStringLiteral("plate"), QStringLiteral("pedestal")}) {
        values.insert("iconTileTexture", look); scene->setProperty("tileValues", values);
        QVERIFY2(changed(baseline,capture("texture-"+look)) >= 20, qPrintable(look));
        values.insert("iconTileTexture", "none"); values.insert("iconTileMaterial", look);
        scene->setProperty("tileValues", values);
        const auto image = capture("material-"+look); QVERIFY(!image.isNull());
        if (look != "minimal") QVERIFY2(changed(baseline,image) >= 20, qPrintable(look));
        values.insert("iconTileMaterial", "minimal");
    }
    scene->setProperty("tileValues", values);
    QVector3D opacityWorld, opacityScreen;
    QVERIFY(QMetaObject::invokeMethod(tile, "mapPositionToScene", Q_RETURN_ARG(QVector3D, opacityWorld),
        Q_ARG(QVector3D, QVector3D(0.78f,0,0))));
    QVERIFY(projectScenePoint(view, opacityWorld, opacityScreen));
    const QPoint opacityPoint(qRound(opacityScreen.x()), qRound(opacityScreen.y()));
    const auto opaque = capture("opacity-100");
    QVERIFY(opaque.rect().contains(opacityPoint));
    QVERIFY(opaque.pixelColor(opacityPoint).alpha() > 200);
    values.insert("iconTileOpacity", 0.5); scene->setProperty("tileValues", values);
    const auto half = capture("opacity-50"); QVERIFY(!half.isNull());
    QVERIFY2(qAbs(half.pixelColor(opacityPoint).alpha() - 128) <= 15,
        qPrintable(QStringLiteral("tile 50 percent alpha was %1").arg(half.pixelColor(opacityPoint).alpha())));
    values.insert("iconTileOpacity", 0.0); scene->setProperty("tileValues", values);
    const auto clear = capture("opacity-0"); QVERIFY(!clear.isNull());
    QVERIFY(clear.pixelColor(opacityPoint).alpha() < 32);
    values.insert("iconTileOpacity", 1.0); scene->setProperty("tileValues", values);
    auto *entry = renderer->findChild<QObject *>("mesh-entry-0"); QVERIFY(entry);
    const QVector3D before = entry->property("position").value<QVector3D>();
    values.insert("iconTileElevation", 12.0); scene->setProperty("tileValues", values);
    QVERIFY(changed(baseline,capture("elevated")) >= 20);
    QVERIFY(qAbs(entry->property("position").value<QVector3D>().z() - before.z() - 12) < 0.001);
    scene->setProperty("wheelTravel", 1.0);
    const QImage travelled = capture("upright-orbit"); QVERIFY(!travelled.isNull());
    QVERIFY((entry->property("position").value<QVector3D>() - before).length() > 50);
    auto *glyph = renderer->findChild<QObject *>("mesh-glyph-0"); QVERIFY(glyph);
    const auto orientation = glyph->property("sceneRotation");
    scene->setProperty("wheelTravel", 2.0); QVERIFY(!capture("upright-orbit-2").isNull());
    QCOMPARE(glyph->property("sceneRotation"), orientation);
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
}

QTEST_MAIN(RendererCapabilityTest)
#include "RendererCapabilityTest.moc"
