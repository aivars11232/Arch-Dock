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
#include <QQmlListReference>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QVector3D>
#include <QWheelEvent>
#include <QtTest>
#include <QElapsedTimer>

#include <functional>
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
// A scene position in the panel's own coordinates, as the renderer maps it
// for input (PanelScene3D.projectScenePoint): through the camera's own frame.
// Qt's camera viewport mapping keeps a projection from before the view last
// changed shape, and the view is drawn past the panel's box by margins that
// change as the scene does; the pixel checks below compare this mapping
// with what is drawn.
bool projectScenePoint(QObject *view, const QVector3D &position, QVector3D &point)
{
    auto *item = qobject_cast<QQuickItem *>(view);
    QObject *renderer = item ? item->parentItem() : nullptr;
    QVariant mapped;
    if (!renderer || !QMetaObject::invokeMethod(renderer, "projectScenePoint",
        Q_RETURN_ARG(QVariant, mapped), Q_ARG(QVariant, QVariant::fromValue(position)))) return false;
    point = mapped.value<QVector3D>();
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

// The 3D view's picture, cropped to the panel's box: the view is drawn past
// the box by its margins (PanelScene3D.drawnBounds), and the tests measure in
// the panel's own coordinates.
QImage panelPixels(QQuickItem *view)
{
    const auto grab = view ? view->grabToImage() : nullptr;
    if (!grab)
        return {};
    QSignalSpy ready(grab.get(), &QQuickItemGrabResult::ready);
    if (!ready.wait(5000))
        return {};
    const QImage image = grab->image();
    auto *panel = view->parentItem();
    if (!panel)
        return image;
    const double scale = image.width() / std::max(1.0, view->width());
    return image.copy(qRound(-view->x() * scale), qRound(-view->y() * scale),
                      qRound(panel->width() * scale), qRound(panel->height() * scale));
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
        // A real notch remains a notch even when Wayland also supplies
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
        // Use a drawn icon of the test data (a logo: a triangle with a ring
        // round it, an icon's own kind of outline, not a plate) so this gate
        // measures mesh motion, not icon lookup, and its pictures show an
        // icon with a shape to read.
        const QString glyphFixture = QFINDTESTDATA("data/truth-glyph-logo.svg");
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
                                   panelMotionTarget: "panel", scene3DIconDepth: 0})
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
        // glyph and a tile quad (2 + 2), the theme's icon base and its
        // declared entry part (one platform mesh each). A new panel has no
        // stands.
        const int expectedTriangles = meshTriangles * 7 + 2 * (2 + 2);
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
            return viewport ? panelPixels(viewport) : QImage();
        };
        QTest::qWait(200); // Allow the asynchronous icon source to reach the mesh texture.
        const QImage first = pixels();
        QVERIFY(!first.isNull());
        const QString evidenceRoot = qEnvironmentVariable("ARCHDOCK_SCENE_EVIDENCE_DIR");
        const QString evidence = themeId == QStringLiteral("arc-platform-orange") && !evidenceRoot.isEmpty()
            ? QDir(evidenceRoot).filePath(QStringLiteral("orange")) : evidenceRoot;
        if (!evidence.isEmpty()) QVERIFY(QDir().mkpath(evidence));
        if (!evidence.isEmpty())
            QVERIFY(first.save(QDir(evidence).filePath(QStringLiteral("mesh-scene.png"))));
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
                    // The logo's blues (#2458b0, #3a86e0) are distinct from
                    // the light cyan platform and from the orange one.
                    glyphPixels += color.alpha() > 32 && color.blueF() > 0.55
                        && color.redF() < 0.35 && color.greenF() < 0.6
                        && color.blueF() > color.greenF() + 0.1;
                }
            // Fewer for a picture drawn smaller in its bubble.
            const double share = renderer->findChild<QObject *>(QStringLiteral("mesh-entry-%1").arg(index))
                ->property("iconShare").toDouble();
            QVERIFY2(glyphPixels > 100 * share * share, qPrintable(QStringLiteral(
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
        // A wheel step eases in over 120 ms; let it rest
        // before putting the panel back.
        QTRY_VERIFY(!scene->property("turnStepping").toBool());
        scene->setProperty("wheelRotationAngle", 0.0);
        scene->setProperty("wheelRotationTarget", 0.0);
        // With its entries travelling instead, the panel keeps
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
        // Native material editing and a folded platform must
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
                QStringLiteral("plate"), QStringLiteral("pedestal"), QStringLiteral("river"),
                QStringLiteral("magma")}) {
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
            // The river and the magma: their surface flows along the band in
            // its own material, and water and lava draw differently.
            QImage flows[2];
            int flowIndex = 0;
            for (const QString &look : {QStringLiteral("river"), QStringLiteral("magma")}) {
                edited.insert("scene3DMaterial", look);
                edited.insert("scene3DTexture", "theme");
                scene->setProperty("panelDefinition", edited);
                QTRY_COMPARE(renderer->property("textureMaterial").toString(), look);
                QTRY_VERIFY(renderer->property("flowingSurface").toBool());
                QCOMPARE(renderer->property("moltenFlow").toBool(), look == QStringLiteral("magma"));
                QObject *platformNode = renderer->findChild<QObject *>(QStringLiteral("mesh-platform"));
                QVERIFY(platformNode && objectValue(platformNode->property("reliefMaterial")));
                QTRY_VERIFY_WITH_TIMEOUT(renderer->property("materialTextureReady").toBool(), 5000);
                QTest::qWait(150);
                flows[flowIndex] = pixels();
                QVERIFY2(!flows[flowIndex].isNull(), qPrintable(look));
                if (!evidence.isEmpty()) QVERIFY(flows[flowIndex].save(QDir(evidence).filePath("flow-" + look + ".png")));
                ++flowIndex;
                if (look == QStringLiteral("river")) {
                    // The water mirrors the icons: the mirror sheet holds one
                    // picture per icon, and the water changes when the mirror
                    // is taken away.
                    QTRY_VERIFY(renderer->property("waterReflections").toBool());
                    QObject *mirrorView = renderer->findChild<QObject *>(QStringLiteral("river-mirror-sheet"));
                    QVERIFY(mirrorView && mirrorView->property("width").toReal() > 0);
                    // The sheet's sprites are its visual children (a repeater's
                    // delegates are not found by object name).
                    auto *sheetItem = qobject_cast<QQuickItem *>(mirrorView);
                    QVERIFY(sheetItem);
                    const auto spriteShown = [sheetItem]() {
                        for (QQuickItem *child : sheetItem->childItems())
                            if (child->objectName() == QStringLiteral("mirror-glyph-0")) return true;
                        return false;
                    };
                    QTRY_VERIFY(spriteShown());
                    // Seen from low over the water the mirror images reach out
                    // from under the icons; from straight above the icons would
                    // cover them.
                    auto low = edited;
                    low.insert("scene3DCameraPitch", 60.0);
                    scene->setProperty("panelDefinition", low);
                    QTRY_COMPARE(renderer->property("shownPitch").toDouble(), 60.0);
                    QTest::qWait(300);
                    const QImage mirrored = pixels();
                    if (!evidence.isEmpty()) {
                        QVERIFY(mirrored.save(QDir(evidence).filePath("mirror-river.png")));
                        // What the mirror sheet holds.
                        auto *mirrorItem = qobject_cast<QQuickItem *>(mirrorView);
                        QVERIFY(mirrorItem);
                        auto grab = mirrorItem->grabToImage();
                        QVERIFY(grab);
                        QTRY_VERIFY(!grab->image().isNull());
                        QVERIFY(grab->image().save(QDir(evidence).filePath("mirror-view.png")));
                    }
                    renderer->setProperty("reflectionsAllowed", false);
                    QTRY_VERIFY(!renderer->property("waterReflections").toBool());
                    QTest::qWait(150);
                    if (!evidence.isEmpty()) QVERIFY(pixels().save(QDir(evidence).filePath("mirror-off.png")));
                    QVERIFY2(!mirrored.isNull() && pixels() != mirrored, "the icons' mirror images show in the water");
                    renderer->setProperty("reflectionsAllowed", true);
                    scene->setProperty("panelDefinition", edited);
                    QTRY_COMPARE(renderer->property("shownPitch").toDouble(), edited.value("scene3DCameraPitch").toDouble());
                }
            }
            QVERIFY2(flows[0] != flows[1], "water and lava draw differently");
            // Real 3D icons: with Icon depth each icon's picture stands as a
            // solid of its own outline, and the scene draws differently from
            // the flat pictures.
            {
                auto flat = edited;
                flat.insert("scene3DMaterial", "theme");
                flat.insert("scene3DIconDepth", 0.0);
                scene->setProperty("panelDefinition", flat);
                QTRY_COMPARE(renderer->property("iconDepth").toDouble(), 0.0);
                QTest::qWait(200);
                const QImage flatIcons = pixels();
                auto deep = flat;
                // A 3D panel's icons stay flat pictures until the 3D icons
                // switch is on.
                deep.insert("scene3DIconSolid", true);
                deep.insert("scene3DIconDepth", 0.5);
                deep.insert("scene3DIconTurn", 25.0);
                // Ask for the rounded relief by name first, whatever the
                // icon's own kind.
                deep.insert("scene3DIconRelief", "rounded");
                scene->setProperty("panelDefinition", deep);
                QTRY_COMPARE(renderer->property("iconDepth").toDouble(), 0.5);
                QObject *solid = renderer->findChild<QObject *>(QStringLiteral("mesh-icon-solid-0"));
                QVERIFY(solid);
                QTRY_VERIFY_WITH_TIMEOUT(solid->property("shown").toBool(), 8000);
                QTest::qWait(300);
                const QImage solidIcons = pixels();
                QVERIFY2(!flatIcons.isNull() && solidIcons != flatIcons, "the icons' solids draw differently from the flat pictures");
                // Icon lighting at its full lets the light alone shape the
                // volume, which draws differently from the half-lit default.
                auto sculpted = deep;
                sculpted.insert("scene3DIconLighting", 1.0);
                scene->setProperty("panelDefinition", sculpted);
                QTRY_COMPARE(renderer->property("iconLighting").toDouble(), 1.0);
                QTest::qWait(300);
                const QImage sculptedIcons = pixels();
                QVERIFY2(!sculptedIcons.isNull() && sculptedIcons != solidIcons, "the light shapes the volume differently");
                if (!evidence.isEmpty()) QVERIFY(sculptedIcons.save(QDir(evidence).filePath("icons-sculpted.png")));
                scene->setProperty("panelDefinition", deep);
                QTRY_COMPARE(renderer->property("iconLighting").toDouble(), 0.5);
                // The other shape, a block with walls, draws differently from
                // the rounded relief once its mesh is cut.
                auto blocky = deep;
                blocky.insert("scene3DIconRelief", "block");
                scene->setProperty("panelDefinition", blocky);
                QTRY_COMPARE(renderer->property("iconRelief").toString(), QStringLiteral("block"));
                QTRY_VERIFY_WITH_TIMEOUT(pixels() != solidIcons, 8000);
                QTest::qWait(300);
                const QImage blockIcons = pixels();
                if (!evidence.isEmpty()) {
                    QVERIFY(flatIcons.save(QDir(evidence).filePath("icons-flat.png")));
                    QVERIFY(solidIcons.save(QDir(evidence).filePath("icons-rounded.png")));
                    QVERIFY(blockIcons.save(QDir(evidence).filePath("icons-block.png")));
                }
                scene->setProperty("panelDefinition", edited);
            }
            // A picture on a theme's own mesh is real relief as well: the
            // mesh is cut finely and rises, and Relief depth changes it.
            {
                auto pictured = edited;
                pictured.insert("panelPicture",
                    QUrl::fromLocalFile(QFINDTESTDATA("fixtures/pictures/red-blue.png")).toString());
                pictured.insert("scene3DTexture", "picture");
                pictured.insert("panelPictureDepth", 0.0);
                scene->setProperty("panelDefinition", pictured);
                QTRY_COMPARE(renderer->property("textureMaterial").toString(), QStringLiteral("picture"));
                QObject *platformNode = renderer->findChild<QObject *>(QStringLiteral("mesh-platform"));
                QVERIFY(platformNode && objectValue(platformNode->property("reliefMaterial")));
                QTRY_VERIFY(platformNode->property("risesReady").toBool());
                QVERIFY(renderer->property("triangleCount").toInt() > expectedTriangles);
                QTest::qWait(150);
                const QImage flatPicture = pixels();
                pictured.insert("panelPictureDepth", 1.0);
                scene->setProperty("panelDefinition", pictured);
                QTest::qWait(150);
                QVERIFY2(!flatPicture.isNull() && pixels() != flatPicture, "Relief depth changes a theme's mesh");
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
            // With no box round the panel an entry that travels out of the
            // panel's box is still drawn and still answers there; each
            // entry answers exactly when nothing of the platform is in front
            // of it. (Before, entries out of the box were cut off and read
            // as covered.)
            bool exposed = false, exposedOutside = false;
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
                        QVariant behind;
                        QVERIFY(QMetaObject::invokeMethod(renderer, "entryPointBehindPlatform",
                            Q_RETURN_ARG(QVariant, behind), Q_ARG(QVariant, index),
                            Q_ARG(QVariant, QVariant(QPointF(rect.value("centerX").toDouble(),
                                                             rect.value("centerY").toDouble())))));
                        QVERIFY2(visible.toBool() == !behind.toBool(), qPrintable(QStringLiteral(
                            "entry %1 at pitch %2 step %3 answers %4 behind %5").arg(index).arg(pitch).arg(step)
                            .arg(visible.toBool()).arg(behind.toBool())));
                        exposed |= visible.toBool();
                        const double x = rect.value("centerX").toDouble(), y = rect.value("centerY").toDouble();
                        exposedOutside |= visible.toBool()
                            && (x < 0 || y < 0 || x > scene->width() || y > scene->height());
                    }
                    if (!evidence.isEmpty() && pitch == -60)
                        QVERIFY(travelled.save(QDir(evidence).filePath(QStringLiteral("travel-occlusion-%1.png").arg(step))));
                    QCOMPARE(scene->property("effectiveLayoutAngle").toDouble(), 0.0);
                }
            }
            QVERIFY2(exposed && exposedOutside,
                     "native platform travel must keep its entries drawn and answering, out of its box too");
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
        // Seen in the bubble's glass, the magenta tile colour is a light
        // pink: red and blue well above green.
        const auto pinkPixels = [](const QImage &image) {
            int count = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const auto color = image.pixelColor(x, y);
                    count += color.alpha() > 32 && color.red() - color.green() > 30
                        && color.blue() - color.green() > 15;
                }
            return count;
        };
        const auto customTiles = pixels();
        if (!evidence.isEmpty()) QVERIFY(customTiles.save(QDir(evidence).filePath("mesh-custom-tiles.png")));
        QVERIFY(pinkPixels(customTiles) > 20);
        QTRY_VERIFY(projectionMatches());
        tiles.insert(QStringLiteral("iconShape"), QStringLiteral("circle"));
        scene->setProperty("panelDefinition", tiles);
        QTest::qWait(100);
        const auto circleTiles = pixels();
        QVERIFY(pinkPixels(circleTiles) > 20);
        QVERIFY(circleTiles != customTiles);
        QTRY_VERIFY(projectionMatches());
        tiles.insert(QStringLiteral("iconTilesEnabled"), false);
        scene->setProperty("panelDefinition", tiles);
        QTest::qWait(100);
        QCOMPARE(pinkPixels(pixels()), 0);
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
        // The turn is seen on an icon without a tile: one in a bubble faces
        // the viewer (tileBubblesPlacementAndOrbitRenderUnderRhi).
        QVariantMap untiled = plainValue(scene->property("panelDefinition")).toMap();
        const QVariantMap tiled = untiled;
        untiled.insert(QStringLiteral("iconTilesEnabled"), false);
        scene->setProperty("panelDefinition", untiled);
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

        scene->setProperty("panelDefinition", tiled);
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
        // A radial collapse closes the whole platform toward its
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

    // The 3D page's transform scales, moves and rolls the
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
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120, iconTilesEnabled: false,
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
        QTRY_COMPARE(renderer->property("panelFieldOfView").toDouble(), 60.0);
        // The camera shows the panel's box through that field, widened by as
        // much as the view reaches past the box.
        const auto viewField = [&]() {
            return qRadiansToDegrees(2 * std::atan(std::tan(qDegreesToRadians(30.0))
                * viewport->property("height").toDouble() / scene->height()));
        };
        QTRY_VERIFY2(qAbs(camera->property("fieldOfView").toDouble() - viewField()) < 1e-3,
                     qPrintable(QStringLiteral("%1 for %2").arg(camera->property("fieldOfView").toDouble())
                                    .arg(viewField())));
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

    // How much of its track a 3D panel's icons get as drawn: seen straight
    // on, most of it; tilted, the track's ends are seen shortened, so less,
    // and less again the further it tilts. A panel holds that many fewer
    // icons.
    void seenTrackFactorFollowsTheTilt()
    {
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // Needs the private RHI session, like the scene gates above.
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        QVERIFY(!glyphFixture.isEmpty());
        // A generated platform with a procedural look, as in
        // generatedPlatformsFollowTheLayoutAndLook.
        QVariantMap theme{
            {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
            {QStringLiteral("version"), 2}, {QStringLiteral("valid"), true},
            {QStringLiteral("id"), QStringLiteral("procedural-look")},
            {QStringLiteral("name"), QStringLiteral("Procedural look")},
            {QStringLiteral("capabilities"), QVariantMap{
                {QStringLiteral("rendererTiers"), QVariantList{QStringLiteral("true3d"), QStringLiteral("procedural2d")}},
                {QStringLiteral("preferredRendererTier"), QStringLiteral("procedural2d")},
                {QStringLiteral("fallbackRendererTiers"), QVariantList{QStringLiteral("procedural2d")}}}}};
        theme.insert(QStringLiteral("scene3D"), QVariantMap{
            {QStringLiteral("generic"), true}, {QStringLiteral("generated"), QVariantMap{}},
            {QStringLiteral("fieldOfView"), 40}, {QStringLiteral("cameraPitch"), 0},
            {QStringLiteral("cameraYaw"), 0}, {QStringLiteral("keyLightBrightness"), 1.2},
            {QStringLiteral("fillLightBrightness"), 0.45},
            {QStringLiteral("defaultQuality"), QStringLiteral("low")},
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
                property real pitch: 30
                panelDefinition: ({rendererTier: "true3d", layout: "circular", layoutRadius: 150,
                                   scene3DQuality: "low", iconSize: 48, spacing: 8, layoutPadding: 10,
                                   scene3DCameraPitch: pitch, scene3DCameraYaw: 0,
                                   scene3DTransitions: false, scene3DFloat: false})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}})
                orderedEntries: [0, 1, 2, 3, 4, 5, 6, 7].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/SeenTrackConsumer.qml")));
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
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 8);
        const auto seen = [&](bool asSeen) {
            QVariant result;
            QMetaObject::invokeMethod(scene, "seenTrackFactor", Q_RETURN_ARG(QVariant, result),
                                      Q_ARG(QVariant, 16), Q_ARG(QVariant, asSeen));
            return result.toDouble();
        };
        // Tilted, once the icons are drawn there: the panel's size as drawn
        // and what the camera shows of it; -1 if they never are drawn.
        const auto tilted = [&](double pitch) {
            const QVariant before = plainValue(renderer->property("projectedEntryGeometry"));
            scene->setProperty("pitch", pitch);
            const bool drawn = QTest::qWaitFor([&]() {
                return plainValue(renderer->property("projectedEntryGeometry")) != before;
            }, 5000);
            QTest::qWait(100);
            return drawn ? QPointF(seen(false), seen(true)) : QPointF(-1, -1);
        };
        const QPointF facing = tilted(0);
        const QPointF steep = tilted(60);
        const QPointF steeper = tilted(75);
        const QString values = QStringLiteral("%1 %2 %3 %4 %5 %6").arg(facing.x()).arg(facing.y())
            .arg(steep.x()).arg(steep.y()).arg(steeper.x()).arg(steeper.y());
        // The platform is drawn a little inside the panel's own layout, its
        // icons at their own size; seen straight on, the camera shows that.
        QVERIFY2(facing.x() > 0.4 && facing.x() <= 1.5, qPrintable(values));
        QVERIFY2(qAbs(facing.y() - facing.x()) < 0.15 * facing.x(), qPrintable(values));
        // Its size does not change with the tilt; what the camera shows does.
        QVERIFY2(qAbs(steep.x() - facing.x()) < 0.1 * facing.x(), qPrintable(values));
        QVERIFY2(qAbs(steeper.x() - facing.x()) < 0.1 * facing.x(), qPrintable(values));
        QVERIFY2(steep.y() < 0.75 * facing.y() && steep.y() > 0.25 * facing.y(), qPrintable(values));
        QVERIFY2(steeper.y() < steep.y(), qPrintable(values));
    }

    // Picking the Top or Side panel
    // while a 3D free panel was previewed killed Arch Dock (SIGSEGV in
    // QSGRhiLayer::grab, called from QQuick3DTexture's afterSynchronizing
    // update of an icon texture whose source had already left the window).
    // One event turn swaps the preview's panel, look, entries and mode, as
    // the Studio's panel switch does; the process lives through the frames
    // after it, both ways, several times. A 3D panel losing every entry and
    // a whole preview being unloaded take the same way out. The freed layer
    // is only read, so a run shows the crash reliably with freed memory
    // overwritten (MALLOC_PERTURB_, see run-rendering-import-smoke.sh).
    void previewSwitchFromTrue3DToAnEdgePanel()
    {
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // The private RHI invocation is the real scene gate.
        if (!qEnvironmentVariableIsSet("MALLOC_PERTURB_"))
            QSKIP("Runs in its own invocation, with freed memory overwritten.");
        const QString themeRoot = qEnvironmentVariable("ARCHDOCK_RENDERING_STAGED_THEME_ROOT",
            QStringLiteral(ARCHDOCK_SOURCE_THEME_PACKAGE_ROOT));
        const auto package = ArchDock::ThemePackage::load(themeRoot
            + QStringLiteral("/mesh-platform-cyan/archdock-theme.json"));
        QVERIFY2(package.isValid(), qPrintable(package.primaryCode()));
        const QVariantMap theme = package.package->runtimeProjection();
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        QVERIFY(!glyphFixture.isEmpty());
        const QString glyph = QUrl::fromLocalFile(glyphFixture).toString();
        QQmlEngine engine;
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            Item {
                property alias previewActive: loader.active
                readonly property Item preview: loader.item
                width: 760
                height: 190
                Loader {
                    id: loader
                    anchors.fill: parent
                    sourceComponent: LivePanelPreview {
                        contentMargin: 6
                        animationProfiles: ({reducedMotion: true})
                    }
                }
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/PreviewConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const QVariantList entries{
            QVariantMap{{"id", "one"}, {"displayName", "One"}, {"iconName", glyph}},
            QVariantMap{{"id", "two"}, {"displayName", "Two"}, {"iconName", glyph}},
            QVariantMap{{"id", "three"}, {"displayName", "Three"}, {"iconName", glyph}}};
        const QVariantMap free{{"edge", "free"}, {"type", "launcher"}, {"layout", "ring"},
            {"layoutRadius", 120}, {"iconSize", 40}, {"layoutPadding", 10},
            {"rendererTier", "true3d"}, {"scene3DQuality", "low"},
            {"panelThemeId", theme.value(QStringLiteral("id"))}};
        const auto edge = [](const QString &side) {
            return QVariantMap{{"edge", side}, {"type", "hybrid"}, {"layout", "adaptive"},
                {"dynamic", true}, {"width", 720}, {"height", 76}, {"iconSize", 52},
                {"rendererTier", "procedural2d"}, {"visible", false}};
        };
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *host = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(host);
        host->setParentItem(window.contentItem());
        QObject *preview = nullptr;
        const auto showFreePanel = [&]() {
            preview = objectValue(host->property("preview"));
            if (!preview) return false;
            preview->setProperty("useProvidedEntries", true);
            preview->setProperty("panelDefinition", free);
            preview->setProperty("themeDefinition", theme);
            preview->setProperty("orderedEntries", entries);
            preview->setProperty("previewMode", QStringLiteral("free"));
            return true;
        };
        QVERIFY(showFreePanel());
        window.resize(760, 190);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // Every order the Studio's bindings may hand the changes over in.
        const QList<QByteArrayList> orders{
            {"panelDefinition", "themeDefinition", "orderedEntries", "previewMode"},
            {"orderedEntries", "panelDefinition", "themeDefinition", "previewMode"},
            {"themeDefinition", "orderedEntries", "panelDefinition", "previewMode"},
            {"previewMode", "orderedEntries", "themeDefinition", "panelDefinition"},
            {"orderedEntries", "themeDefinition", "previewMode", "panelDefinition"},
            {"themeDefinition", "panelDefinition", "previewMode", "orderedEntries"}};
        for (int round = 0; round < orders.size(); ++round)
        {
            QTRY_COMPARE_WITH_TIMEOUT(preview->property("activeRendererTier").toString(),
                                     QStringLiteral("true3d"), 5000);
            QTest::qWait(250);
            const bool side = round % 2 == 1;
            const QVariantMap changes{
                {"panelDefinition", edge(side ? QStringLiteral("right") : QStringLiteral("top"))},
                {"themeDefinition", QVariantMap{}}, {"orderedEntries", QVariantList{}},
                {"previewMode", side ? QStringLiteral("vertical") : QStringLiteral("horizontal")}};
            for (const QByteArray &name : orders.at(round))
                preview->setProperty(name.constData(), changes.value(QString::fromLatin1(name)));
            QTest::qWait(300);
            QCOMPARE(preview->property("activeRendererTier").toString(), QStringLiteral("procedural2d"));
            QVERIFY(showFreePanel());
        }
        // The 3D panel keeps its scene and loses every entry, then gets them back.
        for (int round = 0; round < 2; ++round)
        {
            QTRY_COMPARE_WITH_TIMEOUT(preview->property("activeRendererTier").toString(),
                                     QStringLiteral("true3d"), 5000);
            QTest::qWait(250);
            preview->setProperty("orderedEntries", QVariantList{});
            QTest::qWait(300);
            preview->setProperty("orderedEntries", entries);
        }
        // The whole preview goes, as the Studio's preset preview does, and comes back.
        for (int round = 0; round < 2; ++round)
        {
            QTRY_COMPARE_WITH_TIMEOUT(preview->property("activeRendererTier").toString(),
                                     QStringLiteral("true3d"), 5000);
            QTest::qWait(250);
            host->setProperty("previewActive", false);
            QTest::qWait(300);
            host->setProperty("previewActive", true);
            QVERIFY(showFreePanel());
        }
        QTRY_COMPARE_WITH_TIMEOUT(preview->property("activeRendererTier").toString(),
                                 QStringLiteral("true3d"), 5000);
    }

    // On the desktop the panel's own handles edit its 3D
    // transform with the mouse. Each finished drag is reported once; a
    // cancelled drag leaves the scene and reports nothing; outside edit mode
    // there are no handles and a press changes nothing.
    // A look without a 3D mesh of its
    // own stands on a platform generated along its own layout, in its own
    // colours, with upright icons on pedestals. Every supported shape, the
    // the baked blue ring and the steel octagon, drawn by the real RHI.
    void tileBubblesPlacementAndOrbitRenderUnderRhi();

    // A panel decoration on a 3D panel stands as a solid of its own, drawn by
    // the real RHI: a soft one rounded (a nebula, in its own colours), walled
    // ones (gold filigree, a starship's hull with its lights) and one round
    // all the way round (an oak branch's tubes). Its shaders build - one that
    // does not leaves the panel bare with no other sign - and it is drawn
    // across the band between every two icons.
    void decorationsStandAsSolids_data();
    void decorationsStandAsSolids();
    void hologramsDrawAsLight_data();
    void hologramsDrawAsLight();
    void flatHologramsKeepTheirSize();
    // A 3D icon as a hologram on a flat panel is its solid made of light in
    // the icon's own small 3D view, not its flat picture made light.
    void threeDHologramsOnFlatPanels();
    void turningPanelKeepsItsLayout();

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
                property string picture: ""
                property string look: ""
                property real depth: 0.8
                property string rise: "light"
                property string direction: "inward"
                property real contrast: 0.35
                property string standPicture: ""
                panelDefinition: Object.assign({rendererTier: "true3d", layout: layoutName, layoutRadius: 120, iconTilesEnabled: false,
                                   scene3DQuality: "high", iconSize: 40, layoutPadding: 10,
                                   pathSides: 7, appearance: "neon", scene3DCameraPitch: pitch,
                                   scene3DTransitions: false, scene3DFold: fold, scene3DThickness: thickness,
                                   panelMotionTarget: "items", panelRotationMode: moving ? "clockwise" : "none",
                                   panelTravelSpeed: 2, panelRotationTrigger: "idle", scene3DIconDepth: 0},
                                   picture.length ? {panelPicture: picture, scene3DTexture: "picture",
                                                     panelPictureDepth: depth, panelPictureRise: rise,
                                                     panelPictureDirection: direction,
                                                     panelPictureContrast: contrast} : {},
                                   look.length ? {scene3DMaterial: look} : {},
                                   standPicture.length ? {pedestalShape: "disc", pedestalPicture: standPicture,
                                                          pedestalMaterial: "picture", pedestalPictureDepth: depth} : {})
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
            return panelPixels(qobject_cast<QQuickItem *>(viewport));
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

        // A picture is worn as it is: the ring shows the picture's own red
        // and blue, solid and still, with none of the neon look's cyan, metal
        // or flow laid over it.
        if (layout == QStringLiteral("ring")) {
            const QString picture = QFINDTESTDATA("fixtures/pictures/red-blue.png");
            QVERIFY(!picture.isEmpty());
            scene->setProperty("picture", QUrl::fromLocalFile(picture).toString());
            QTRY_COMPARE(renderer->property("textureMaterial").toString(), QStringLiteral("picture"));
            QTRY_VERIFY_WITH_TIMEOUT(renderer->property("materialTextureReady").toBool(), 5000);
            QVERIFY(!renderer->property("materialAnimationActive").toBool());
            const QVariantMap worn = plainValue(renderer->property("platformMaterial")).toMap();
            QCOMPARE(worn.value(QStringLiteral("vertexColours")).toBool(), false);
            QCOMPARE(worn.value(QStringLiteral("metalness")).toDouble(), 0.0);
            QCOMPARE(worn.value(QStringLiteral("opacity")).toDouble(), 1.0);
            QCOMPARE(worn.value(QStringLiteral("emissiveStrength")).toDouble(), 0.0);
            QTest::qWait(200);
            const QImage pictured = pixels();
            QVERIFY(!pictured.isNull());
            const QVariantList entries = plainValue(renderer->property("projectedEntryGeometry")).toList();
            int red = 0, blue = 0, cyan = 0;
            for (int step = 0; step < 48; ++step) {
                const double angle = step * M_PI / 24 + 0.05;
                const QPoint point = drawn(track * std::cos(angle), track * std::sin(angle));
                bool covered = false;
                for (const QVariant &value : entries) {
                    const QVariantMap rect = value.toMap();
                    covered |= QRectF(rect.value("x").toDouble() - 4, rect.value("y").toDouble() - 4,
                                      rect.value("width").toDouble() + 8,
                                      rect.value("height").toDouble() + 8).contains(point);
                }
                const QColor colour = pictured.pixelColor(point);
                if (covered || colour.alpha() < 200) continue;
                red += colour.red() > colour.green() + 60 && colour.red() > colour.blue() + 60;
                blue += colour.blue() > colour.green() + 60 && colour.blue() > colour.red() + 60;
                cyan += colour.green() > colour.red() + 60 && colour.blue() > colour.red() + 60;
            }
            QVERIFY2(red >= 3 && blue >= 3 && cyan == 0, qPrintable(QStringLiteral(
                "%1 red, %2 blue, %3 cyan samples").arg(red).arg(blue).arg(cyan)));
            if (!evidence.isEmpty())
                QVERIFY(pictured.save(evidence + QStringLiteral("/generated/") + tag + QStringLiteral("-picture.png")));
            // It is worn as a real surface, its brightness its height: the
            // platform's mesh is cut finely and rises as deep as Relief depth.
            QObject *platformNode = renderer->findChild<QObject *>(QStringLiteral("mesh-platform"));
            QVERIFY(platformNode);
            QTRY_VERIFY(platformNode->property("risesReady").toBool());
            QVERIFY(objectValue(platformNode->property("reliefMaterial")));
            scene->setProperty("depth", 0.0);
            QTest::qWait(200);
            const QImage flat = pixels();
            scene->setProperty("depth", 1.0);
            QTest::qWait(200);
            const QImage deep = pixels();
            QVERIFY2(!flat.isNull() && flat != deep, "Relief depth changes the drawn surface");
            // Raising the dark parts instead of the light ones changes it too.
            scene->setProperty("rise", QStringLiteral("dark"));
            QTRY_VERIFY(renderer->property("pictureRaisesDark").toBool());
            QTest::qWait(200);
            QVERIFY2(pixels() != deep, "Raised parts changes the drawn surface");
            scene->setProperty("rise", QStringLiteral("light"));
            // Rising out of the band instead of sinking into it changes it too.
            scene->setProperty("direction", QStringLiteral("outward"));
            QTRY_VERIFY(renderer->property("pictureRisesOut").toBool());
            QTest::qWait(200);
            QVERIFY2(pixels() != deep, "Relief direction changes the drawn surface");
            scene->setProperty("direction", QStringLiteral("inward"));
            // A steeper climb through the midtone changes it too.
            scene->setProperty("contrast", 1.0);
            QTRY_COMPARE(renderer->property("pictureContrast").toDouble(), 1.0);
            QTest::qWait(200);
            QVERIFY2(pixels() != deep, "Relief contrast changes the drawn surface");
            scene->setProperty("contrast", 0.35);
            if (!evidence.isEmpty()) {
                QVERIFY(flat.save(evidence + QStringLiteral("/generated/") + tag + QStringLiteral("-picture-flat.png")));
                QVERIFY(deep.save(evidence + QStringLiteral("/generated/") + tag + QStringLiteral("-picture-deep.png")));
            }
            scene->setProperty("depth", 0.8);
            // A stand wearing a picture of its own rises the same way.
            scene->setProperty("standPicture", QUrl::fromLocalFile(QFINDTESTDATA("fixtures/pictures/red-blue.png")).toString());
            QTRY_VERIFY(renderer->property("pedestalPictureRelief").toBool());
            QObject *standNode = renderer->findChild<QObject *>(QStringLiteral("mesh-pedestal-0"));
            QVERIFY(standNode);
            QTRY_VERIFY(standNode->property("risesReady").toBool());
            QVERIFY(objectValue(standNode->property("reliefMaterial")));
            scene->setProperty("standPicture", QString());
            QTRY_VERIFY(!renderer->property("pedestalPictureRelief").toBool());
            // Picture as the platform's Look wears it the same way; without a
            // picture that Look is the theme's own, and the scene stays drawn.
            scene->setProperty("look", QStringLiteral("picture"));
            QTRY_COMPARE(renderer->property("materialLook").toString(), QStringLiteral("picture"));
            QCOMPARE(plainValue(renderer->property("platformMaterial")).toMap()
                .value(QStringLiteral("vertexColours")).toBool(), false);
            scene->setProperty("picture", QString());
            QTRY_VERIFY(renderer->property("textureMaterial").toString() != QStringLiteral("picture"));
            QCOMPARE(renderer->property("materialLook").toString(), QStringLiteral("theme"));
            QTRY_VERIFY_WITH_TIMEOUT(renderer->property("materialTextureReady").toBool(), 5000);
            QVERIFY(renderer->property("rendererReady").toBool());
            scene->setProperty("look", QString());
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
        // All generated outlines fold their actual GPU mesh;
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
            bool behind = false, visible = false;
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
                        QVariant accepted, covered;
                        QVERIFY(QMetaObject::invokeMethod(renderer, "entryPointVisible", Q_RETURN_ARG(QVariant, accepted),
                            Q_ARG(QVariant, i), Q_ARG(QVariant, QVariant(point))));
                        QVERIFY(QMetaObject::invokeMethod(renderer, "entryPointBehindPlatform", Q_RETURN_ARG(QVariant, covered),
                            Q_ARG(QVariant, i), Q_ARG(QVariant, QVariant(point))));
                        behind |= covered.toBool(); visible |= accepted.toBool();
                    }
                }
            }
            // Icons rise clear of the platform at every camera angle, a thick
            // one included: none is ever hidden in it.
            QVERIFY2(!behind && visible, qPrintable(tag + " must show every glyph clear of the platform"));
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

        // Scale: dragging the red arm's cube outward stretches the platform
        // sideways only.
        renderer->setProperty("gizmoMode", QStringLiteral("scale"));
        QTest::qWait(150);
        const QPoint cube = screenPoint(QStringLiteral("gizmo-scale-x-tip"));
        const QPoint scaleCentre = screenPoint(QStringLiteral("mesh-gizmo"));
        const QPointF outward = QPointF(cube - scaleCentre) / std::max(1.0, QLineF(scaleCentre, cube).length());
        drag(cube, cube + (outward * 20).toPoint());
        QTRY_COMPARE(edits.count(), 4);
        QVERIFY2(editedValues(3).value("scene3DScaleX").toDouble() > 1.0, qPrintable(QString::number(
            editedValues(3).value("scene3DScaleX").toDouble())));
        QVERIFY(!editedValues(3).contains("scene3DScale"));
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

        // Dragging the platform's body, away from every handle
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

        // Seen from straight above, the Z arrow points at the
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

    // A scaled, tilted or moved 3D panel was cut off at
    // an invisible box, its widget's size - "there shouldn't be any edge ...
    // the only edge is edge of the screen". The view reaches past the box as
    // far as the scene does, nothing on the screen moves when it grows, and
    // what is drawn out there is drawn and answers the pointer. The handles of
    // a small platform moved to the right of its box reach past the box.
    void sceneDrawsPastItsBoxWithoutMovingAnything()
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
                property real glow: 0
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DScale: 0.5, scene3DPositionX: 1, scene3DTransitions: false,
                                   scene3DCameraPitch: 25, scene3DCameraYaw: 0,
                                   pedestalShape: "disc", pedestalGlow: glow})
                sceneEditActive: editing
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/NoBoxConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        window.setColor(Qt::black);
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), package.package->runtimeProjection()},
            {QStringLiteral("glyphFixture"), QUrl::fromLocalFile(glyphFixture).toString()}}));
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *scene = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(scene);
        scene->setParentItem(window.contentItem());
        // Room round the panel's box, as a desktop has.
        scene->setPosition(QPointF(scene->width(), scene->height() / 2));
        window.resize(qCeil(scene->width() * 3), qCeil(scene->height() * 2));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(),
                                 QStringLiteral("true3d"), 5000);
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QObject *viewport = renderer ? objectValue(renderer->property("viewport")) : nullptr;
        QVERIFY(renderer && viewport);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 6);
        QTest::qWait(250);
        const auto centres = [&]() {
            QList<QPointF> result;
            for (const QVariant &rect : plainValue(renderer->property("projectedEntryGeometry")).toList())
                result.append(QPointF(rect.toMap().value("centerX").toDouble(),
                                      rect.toMap().value("centerY").toDouble()));
            return result;
        };
        const auto drag = [&](const QPoint &from, const QPoint &to) {
            QTest::mousePress(&window, Qt::LeftButton, {}, from);
            for (int step = 1; step <= 5; ++step)
                QTest::mouseMove(&window, from + (to - from) * step / 5);
            QTest::mouseRelease(&window, Qt::LeftButton, {}, to);
        };
        const QList<QPointF> resting = centres();
        const double restingMargin = renderer->property("overflowX").toDouble();
        QSignalSpy edits(scene, SIGNAL(sceneTransformEdited(QVariant)));
        QVERIFY(edits.isValid());

        // A glow round the stands needs room past the box: the view grows
        // round it, on both sides alike, the panel reports where it is
        // drawn, and nothing moves on the screen.
        scene->setProperty("glow", 0.6);
        QTRY_VERIFY2(renderer->property("overflowX").toDouble() > restingMargin,
                     qPrintable(QStringLiteral("margin %1, was %2")
                                    .arg(renderer->property("overflowX").toDouble()).arg(restingMargin)));
        const double glowMargin = renderer->property("overflowX").toDouble();
        QCOMPARE(viewport->property("x").toDouble(), -glowMargin);
        QCOMPARE(viewport->property("width").toDouble(), scene->width() + 2 * glowMargin);
        const QRectF drawn = scene->property("drawnBounds").toRectF();
        QVERIFY2(drawn.left() <= -glowMargin + 0.5 && drawn.right() >= scene->width() + glowMargin - 0.5,
                 qPrintable(QStringLiteral("drawn %1..%2").arg(drawn.left()).arg(drawn.right())));
        QTest::qWait(250);
        const QList<QPointF> glowing = centres();
        QCOMPARE(glowing.size(), resting.size());
        for (int index = 0; index < resting.size(); ++index)
            QVERIFY2(QLineF(resting[index], glowing[index]).length() < 0.75,
                     qPrintable(QStringLiteral("entry %1 moved from %2,%3 to %4,%5").arg(index)
                                    .arg(resting[index].x()).arg(resting[index].y())
                                    .arg(glowing[index].x()).arg(glowing[index].y())));
        scene->setProperty("glow", 0.0);

        // The handles reach past the box, and the view takes them in.
        scene->setProperty("editing", true);
        QTRY_VERIFY(renderer->property("editMode").toBool());
        QTest::qWait(300);
        const double margin = renderer->property("overflowX").toDouble();
        QVERIFY(margin > 0);

        // The view ring stands past the box's right edge, and is drawn there.
        renderer->setProperty("gizmoMode", QStringLiteral("rotate"));
        QTest::qWait(300);
        QObject *ring = renderer->findChild<QObject *>(QStringLiteral("gizmo-rotate-view-0"));
        QObject *gizmo = renderer->findChild<QObject *>(QStringLiteral("mesh-gizmo"));
        QVERIFY(ring && gizmo);
        QVector3D onRing, middle;
        QVERIFY(projectScenePoint(viewport, ring->property("scenePosition").value<QVector3D>(), onRing));
        QVERIFY(projectScenePoint(viewport, gizmo->property("scenePosition").value<QVector3D>(), middle));
        QVERIFY2(onRing.x() > scene->width() + 2 && onRing.x() < scene->width() + margin,
                 qPrintable(QStringLiteral("ring at %1 of %2, margin %3").arg(onRing.x())
                                .arg(scene->width()).arg(margin)));
        const QImage frame = window.grabWindow();
        const qreal ratio = frame.devicePixelRatio();
        const int left = qCeil((scene->x() + scene->width() + 1) * ratio);
        const int right = qMin(frame.width(), qFloor((scene->x() + scene->width() + margin) * ratio));
        int lit = 0;
        for (int y = 0; y < frame.height(); ++y)
            for (int x = left; x < right; ++x)
                if (qGray(frame.pixel(x, y)) > 60)
                    ++lit;
        QVERIFY2(lit > 20, qPrintable(QStringLiteral("%1 lit pixels past the box").arg(lit)));

        // A drag on it out there turns the view.
        const QPointF centre = scene->mapToScene(QPointF(middle.x(), middle.y()));
        const QPoint from = scene->mapToScene(QPointF(onRing.x(), onRing.y())).toPoint();
        const QPointF radius = QPointF(from) - centre;
        const double turn = qDegreesToRadians(25.0);
        const QPointF turned(radius.x() * qCos(turn) - radius.y() * qSin(turn),
                             radius.x() * qSin(turn) + radius.y() * qCos(turn));
        drag(from, (centre + turned).toPoint());
        QTRY_COMPARE(edits.count(), 1);
        const QVariant value = edits.at(0).at(0);
        const QVariantMap values = value.metaType() == QMetaType::fromType<QJSValue>()
            ? value.value<QJSValue>().toVariant().toMap() : value.toMap();
        QVERIFY2(qAbs(values.value(QStringLiteral("scene3DRoll")).toDouble()) > 10,
                 qPrintable(QStringLiteral("roll %1").arg(values.value(QStringLiteral("scene3DRoll")).toDouble())));
        scene->setProperty("editing", false);
        QTRY_VERIFY(!renderer->property("editMode").toBool());
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
    }

    // A stand's glow "isn't really glow, it's just
    // intense color". A glowing stand sheds light round it, over whatever is
    // behind the panel, as far as its Glow reach; without glow the view runs
    // no glow at all.
    void standGlowShinesRoundTheStand()
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
                required property string standColour
                property real glow: 0
                property real reach: 0.75
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "medium", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DTransitions: false, pedestalShape: "disc",
                                   pedestalColor: standColour, pedestalGlow: glow, pedestalGlowReach: reach})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/GlowConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        window.setColor(Qt::black);
        // The chosen green (a colour written in the QML above stops moc).
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), package.package->runtimeProjection()},
            {QStringLiteral("standColour"), QStringLiteral("#07fd28")},
            {QStringLiteral("glyphFixture"), QUrl::fromLocalFile(glyphFixture).toString()}}));
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *scene = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(scene);
        scene->setParentItem(window.contentItem());
        scene->setPosition(QPointF(scene->width() / 2, scene->height() / 2));
        window.resize(qCeil(scene->width() * 2), qCeil(scene->height() * 2));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(),
                                 QStringLiteral("true3d"), 5000);
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QObject *viewport = renderer ? objectValue(renderer->property("viewport")) : nullptr;
        QVERIFY(renderer && viewport);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 6);
        // Green light on the black behind the panel: the stands, and the
        // light round them. The cyan platform is not counted.
        const auto greenLight = [&]() {
            const QImage frame = window.grabWindow();
            int count = 0;
            for (int y = 0; y < frame.height(); ++y)
                for (int x = 0; x < frame.width(); ++x) {
                    const QRgb pixel = frame.pixel(x, y);
                    if (qGreen(pixel) > 12 && qGreen(pixel) > qRed(pixel) + 8 && qGreen(pixel) > qBlue(pixel) + 8)
                        ++count;
                }
            return count;
        };
        QTest::qWait(400);
        QVERIFY(!renderer->property("bloomWanted").toBool());
        QVERIFY(!objectValue(viewport->property("environment"))->property("glowEnabled").toBool());
        const int unlit = greenLight();
        QVERIFY2(unlit > 50, qPrintable(QStringLiteral("%1 stand pixels").arg(unlit)));

        scene->setProperty("glow", 0.8);
        QTRY_VERIFY(renderer->property("bloomWanted").toBool());
        QTRY_VERIFY(objectValue(viewport->property("environment"))->property("glowEnabled").toBool());
        QCOMPARE(renderer->property("bloomLevels").toInt(), 6);
        QTest::qWait(500);
        const int lit = greenLight();
        QVERIFY2(lit > unlit * 1.3 + 300,
                 qPrintable(QStringLiteral("%1 lit pixels with glow, %2 without").arg(lit).arg(unlit)));

        // Its reach: from three halvings of the blur to seven, the light
        // reaching further out of the stands, and the view leaving room.
        scene->setProperty("reach", 0.0);
        QTRY_COMPARE(renderer->property("bloomLevels").toInt(), 3);
        QTest::qWait(500);
        const int near = greenLight();
        scene->setProperty("reach", 1.0);
        QTRY_COMPARE(renderer->property("bloomLevels").toInt(), 7);
        QTest::qWait(500);
        const int far = greenLight();
        QVERIFY2(far > near * 1.2,
                 qPrintable(QStringLiteral("%1 lit pixels at full reach, %2 at none").arg(far).arg(near)));
        QVERIFY(renderer->property("glowReach").toDouble() >= 12 * 32);

        scene->setProperty("glow", 0.0);
        QTRY_VERIFY(!objectValue(viewport->property("environment"))->property("glowEnabled").toBool());
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
    }

    // Lightning, a look of its own that looks like
    // lightning, "together with animations", on a background from clear to
    // dark storm. In 3D it strikes at random - a flash past white, the bolts
    // jumping along the band - between faint flickers, and keeps still under
    // reduced motion.
    void lightningLookStrikesAndKeepsStill()
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
                property bool reduced: false
                property real background: 0.5
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DTransitions: false, scene3DMaterial: "lightning",
                                   panelLookBackground: background, lookMotionSpeed: 3})
                animationProfiles: ({reducedMotion: reduced})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/LightningConsumer.qml")));
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
        QCOMPARE(renderer->property("textureMaterial").toString(), QStringLiteral("lightning"));
        QTRY_VERIFY(renderer->property("materialTextureReady").toBool());
        QTRY_VERIFY(renderer->property("lightningActive").toBool());
        QCOMPARE(renderer->property("panelLookBackground").toDouble(), 0.5);

        // It strikes: a flash, and the bolts somewhere else.
        const double firstShift = renderer->property("lightningShift").toDouble();
        bool flashed = false, moved = false;
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < 8000 && !(flashed && moved)) {
            QTest::qWait(15);
            flashed = flashed || renderer->property("lightningFlash").toDouble() > 0.5;
            moved = moved || renderer->property("lightningShift").toDouble() != firstShift;
        }
        QVERIFY2(flashed && moved, qPrintable(QStringLiteral("flashed %1, moved %2").arg(flashed).arg(moved)));

        // A clear background is clear: the surface blends.
        scene->setProperty("background", 0.0);
        QTRY_COMPARE(renderer->property("panelLookBackground").toDouble(), 0.0);

        // Reduced motion keeps it still and dark.
        scene->setProperty("reduced", true);
        QTRY_VERIFY(!renderer->property("lightningActive").toBool());
        QTRY_COMPARE(renderer->property("lightningFlash").toDouble(), 0.0);
        const double stillShift = renderer->property("lightningShift").toDouble();
        QTest::qWait(700);
        QCOMPARE(renderer->property("lightningShift").toDouble(), stillShift);
        QCOMPARE(renderer->property("lightningFlash").toDouble(), 0.0);
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
    }

    // Phase 2: "freely in 360 every direction", "for
    // scaling there shouldn't be a limit", "scale it also sideways or top or
    // down". The panel tilts all the way over and stays drawn and placed;
    // sideways and up and down stretch the platform and where its icons
    // stand, never the icons; a large scale draws past the box.
    void panelTurnsAllTheWayAndStretchesEachWay()
    {
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // Needs the private RHI session, like the scene gates above.
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        QVariantMap theme{
            {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
            {QStringLiteral("version"), 2}, {QStringLiteral("valid"), true},
            {QStringLiteral("id"), QStringLiteral("procedural-look")},
            {QStringLiteral("name"), QStringLiteral("Procedural look")},
            {QStringLiteral("capabilities"), QVariantMap{
                {QStringLiteral("rendererTiers"), QVariantList{QStringLiteral("true3d"), QStringLiteral("procedural2d")}},
                {QStringLiteral("preferredRendererTier"), QStringLiteral("procedural2d")},
                {QStringLiteral("fallbackRendererTiers"), QVariantList{QStringLiteral("procedural2d")}}}}};
        theme.insert(QStringLiteral("scene3D"), QVariantMap{
            {QStringLiteral("generic"), true}, {QStringLiteral("generated"), QVariantMap{}},
            {QStringLiteral("fieldOfView"), 40}, {QStringLiteral("cameraPitch"), 25},
            {QStringLiteral("cameraYaw"), 10}, {QStringLiteral("keyLightBrightness"), 1.2},
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
                property real pitch: 25
                property real roll: 0
                property real scaleAll: 1
                property real scaleX: 1
                property real scaleY: 1
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DTransitions: false, scene3DMaterial: "neon", scene3DTexture: "neon",
                                   scene3DCameraPitch: pitch, scene3DRoll: roll, scene3DScale: scaleAll,
                                   scene3DScaleX: scaleX, scene3DScaleY: scaleY})
                animationProfiles: ({reducedMotion: true})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/FreeTransformConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        window.setColor(QColor(30, 34, 40));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), theme},
            {QStringLiteral("glyphFixture"), QUrl::fromLocalFile(glyphFixture).toString()}}));
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *scene = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(scene);
        scene->setParentItem(window.contentItem());
        scene->setPosition(QPointF(scene->width() * 1.5, scene->height() * 1.5));
        window.resize(qCeil(scene->width() * 4), qCeil(scene->height() * 4));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(),
                                 QStringLiteral("true3d"), 5000);
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(renderer);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 6);
        const QString frames = qEnvironmentVariable("ARCHDOCK_TEST_FRAME_DIR");
        const auto settle = [&](const QString &name) {
            QTest::qWait(500);
            const QImage frame = window.grabWindow();
            if (!frames.isEmpty())
                frame.save(frames + QLatin1Char('/') + name + QStringLiteral(".png"));
            return frame;
        };
        const auto entries = [&]() {
            QList<QRectF> result;
            for (const QVariant &rect : plainValue(renderer->property("projectedEntryGeometry")).toList()) {
                const QVariantMap map = rect.toMap();
                result.append(QRectF(map.value("x").toDouble(), map.value("y").toDouble(),
                                     map.value("width").toDouble(), map.value("height").toDouble()));
            }
            return result;
        };
        const auto spread = [](const QList<QRectF> &rects) {
            double left = 1e9, right = -1e9, top = 1e9, bottom = -1e9;
            for (const QRectF &rect : rects) {
                left = qMin(left, rect.center().x()); right = qMax(right, rect.center().x());
                top = qMin(top, rect.center().y()); bottom = qMax(bottom, rect.center().y());
            }
            return QSizeF(right - left, bottom - top);
        };
        const auto drawnPixels = [&](const QImage &frame) {
            int count = 0;
            for (int y = 0; y < frame.height(); y += 2)
                for (int x = 0; x < frame.width(); x += 2) {
                    const QRgb pixel = frame.pixel(x, y);
                    if (qAbs(qRed(pixel) - 30) + qAbs(qGreen(pixel) - 34) + qAbs(qBlue(pixel) - 40) > 24)
                        ++count;
                }
            return count;
        };
        settle(QStringLiteral("turn-25"));
        const QList<QRectF> resting = entries();
        const QSizeF restingSpread = spread(resting);

        // All the way over: tilted on edge, past it and upside down, the
        // panel is still drawn and its icons still placed on the screen.
        for (const double pitch : {90.0, 150.0, 180.0, -120.0}) {
            scene->setProperty("pitch", pitch);
            QTRY_COMPARE(renderer->property("targetPitch").toDouble(), pitch);
            const QImage frame = settle(QStringLiteral("turn-%1").arg(pitch));
            QVERIFY2(drawnPixels(frame) > 300, qPrintable(QStringLiteral("pitch %1: %2 drawn pixels")
                                                            .arg(pitch).arg(drawnPixels(frame))));
            for (const QRectF &rect : entries())
                QVERIFY2(std::isfinite(rect.center().x()) && std::isfinite(rect.center().y()),
                         qPrintable(QStringLiteral("pitch %1: an entry has no place").arg(pitch)));
            // Tilted fully horizontal it lies level, whatever its turn (the
            // owner: "I still should be able to tilt 3D panel fully
            // horizontal"): the icons stand on one level line.
            // The two ends of the dock stand at one height (nearer icons
            // stand higher in perspective, the ends are equally near).
            if (pitch == 90.0) {
                const QList<QRectF> edge = entries();
                QPointF leftmost(1e9, 0), rightmost(-1e9, 0);
                for (const QRectF &rect : edge) {
                    if (rect.center().x() < leftmost.x()) leftmost = rect.center();
                    if (rect.center().x() > rightmost.x()) rightmost = rect.center();
                }
                const double across = rightmost.x() - leftmost.x();
                qInfo("tilted 90: ends %.0f apart, %.1f apart up and down", across, rightmost.y() - leftmost.y());
                QVERIFY2(qAbs(rightmost.y() - leftmost.y()) < across * 0.02,
                         qPrintable(QStringLiteral("tilted 90 the ends differ by %1 over %2 across")
                                        .arg(rightmost.y() - leftmost.y()).arg(across)));
            }
        }
        scene->setProperty("pitch", 25.0);
        QTRY_COMPARE(renderer->property("targetPitch").toDouble(), 25.0);
        settle(QStringLiteral("turn-back"));

        // Sideways: twice as wide, as tall as before, every icon its own size.
        scene->setProperty("scaleX", 2.0);
        QTRY_COMPARE(renderer->property("shownScaleX").toDouble(), 2.0);
        settle(QStringLiteral("stretch-sideways-2"));
        const QList<QRectF> wide = entries();
        const QSizeF wideSpread = spread(wide);
        qInfo("spread %.0fx%.0f, sideways x2 %.0fx%.0f", restingSpread.width(), restingSpread.height(),
              wideSpread.width(), wideSpread.height());
        QVERIFY2(wideSpread.width() > restingSpread.width() * 1.7,
                 qPrintable(QStringLiteral("%1 wide, %2 before").arg(wideSpread.width()).arg(restingSpread.width())));
        QVERIFY2(qAbs(wideSpread.height() - restingSpread.height()) < restingSpread.height() * 0.2,
                 qPrintable(QStringLiteral("%1 tall, %2 before").arg(wideSpread.height()).arg(restingSpread.height())));
        for (int index = 0; index < resting.size(); ++index)
            QVERIFY2(qAbs(wide[index].width() - resting[index].width()) < resting[index].width() * 0.25,
                     qPrintable(QStringLiteral("icon %1 %2 wide, %3 before").arg(index)
                                    .arg(wide[index].width()).arg(resting[index].width())));
        // Up and down: half as tall.
        scene->setProperty("scaleX", 1.0);
        scene->setProperty("scaleY", 0.5);
        QTRY_COMPARE(renderer->property("shownScaleY").toDouble(), 0.5);
        settle(QStringLiteral("stretch-up-down-0.5"));
        const QSizeF flat = spread(entries());
        qInfo("up and down x0.5 %.0fx%.0f", flat.width(), flat.height());
        QVERIFY2(flat.height() < restingSpread.height() * 0.65,
                 qPrintable(QStringLiteral("%1 tall, %2 before").arg(flat.height()).arg(restingSpread.height())));
        scene->setProperty("scaleY", 1.0);

        // All ways, three times: past the box, the view grown round it.
        scene->setProperty("scaleAll", 3.0);
        QTRY_COMPARE(renderer->property("shownScale").toDouble(), 3.0);
        settle(QStringLiteral("scale-3"));
        const QSizeF large = spread(entries());
        qInfo("all ways x3 %.0fx%.0f, margin %.0f", large.width(), large.height(),
              renderer->property("overflowX").toDouble());
        QVERIFY2(large.width() > restingSpread.width() * 2.2,
                 qPrintable(QStringLiteral("%1 wide, %2 before").arg(large.width()).arg(restingSpread.width())));
        QVERIFY(renderer->property("overflowX").toDouble() > 0);
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
    }

    // The wheel over an icon turns a panel whose view reaches past its box
    // (the room kept round a glow), as it does one drawn inside it, even
    // straight after the view grew: icons are hit through the camera's own
    // frame, not Qt's picking, whose projection lags a view that changed
    // shape until it renders again.
    void wheelTurnsAPanelDrawnPastItsBox()
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
        engine.addImportPath(importRoot());
        QQmlComponent component(&engine);
        component.setData(R"(
            import QtQuick
            import ArchDock.Rendering 1.0
            PanelScene {
                required property string glyphFixture
                property real glow: 0
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DTransitions: false, panelMotionTarget: "panel",
                                   panelRotationMode: "none", panelGlow: glow, panelGlowReach: 0.7})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3, 4].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/WheelConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), package.package->runtimeProjection()},
            {QStringLiteral("glyphFixture"), QUrl::fromLocalFile(glyphFixture).toString()}}));
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *scene = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(scene);
        scene->setParentItem(window.contentItem());
        scene->setPosition(QPointF(scene->width(), scene->height()));
        window.resize(qCeil(scene->width() * 3), qCeil(scene->height() * 3));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(),
                                 QStringLiteral("true3d"), 5000);
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(renderer);
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 5);
        QTRY_VERIFY(scene->property("wheelRotationAvailable").toBool());
        const auto turnOnce = [&](const QString &when) {
            QSignalSpy frame(&window, &QQuickWindow::frameSwapped);
            window.update();
            QVERIFY(frame.wait(5000));
            QTest::qWait(150);
            const auto rect = plainValue(renderer->property("projectedEntryGeometry")).toList().value(1).toMap();
            const QPointF point = scene->mapToScene(QPointF(rect.value("centerX").toDouble(),
                                                            rect.value("centerY").toDouble()));
            const double before = scene->property("wheelRotationTarget").toDouble();
            const double angle = scene->property("sceneRotationAngle").toDouble();
            QTest::mouseMove(&window, point.toPoint());
            QWheelEvent wheel(point, window.mapToGlobal(point.toPoint()), QPoint(), QPoint(0, 120),
                Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(&window, &wheel);
            QTRY_VERIFY2(qAbs(scene->property("wheelRotationTarget").toDouble() - before - 15) < 0.01,
                         qPrintable(QStringLiteral("%1: margin %2, rotation target %3 from %4")
                                        .arg(when).arg(renderer->property("overflowX").toDouble())
                                        .arg(scene->property("wheelRotationTarget").toDouble()).arg(before)));
            // The panel itself turns there, as a desktop sees it.
            QTRY_VERIFY2(qAbs(std::fmod(scene->property("sceneRotationAngle").toDouble() - angle - 15 + 720, 360)) < 0.01,
                         qPrintable(QStringLiteral("%1: margin %2, angle %3 from %4")
                                        .arg(when).arg(renderer->property("overflowX").toDouble())
                                        .arg(scene->property("sceneRotationAngle").toDouble()).arg(angle)));
        };
        qInfo("resting margin %.0f", renderer->property("overflowX").toDouble());
        turnOnce(QStringLiteral("at rest"));
        if (QTest::currentTestFailed())
            return;
        scene->setProperty("glow", 0.6);
        QTRY_VERIFY(renderer->property("overflowX").toDouble() >= renderer->property("glowReach").toDouble());
        turnOnce(QStringLiteral("glowing"));
        if (QTest::currentTestFailed())
            return;
        // Straight after the view grows again, before it renders.
        const double glowRoom = renderer->property("glowReach").toDouble();
        scene->setProperty("glow", 0.0);
        QTRY_VERIFY(renderer->property("overflowX").toDouble() < glowRoom);
        QTest::qWait(300);
        const auto rect = plainValue(renderer->property("projectedEntryGeometry")).toList().value(2).toMap();
        const QPointF point = scene->mapToScene(QPointF(rect.value("centerX").toDouble(),
                                                        rect.value("centerY").toDouble()));
        QTest::mouseMove(&window, point.toPoint());
        scene->setProperty("glow", 0.6);
        QTRY_VERIFY(renderer->property("overflowX").toDouble() >= glowRoom);
        const double before = scene->property("wheelRotationTarget").toDouble();
        QWheelEvent wheel(point, window.mapToGlobal(point.toPoint()), QPoint(), QPoint(0, 120),
            Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&window, &wheel);
        QTRY_VERIFY2(qAbs(scene->property("wheelRotationTarget").toDouble() - before - 15) < 0.01,
                     qPrintable(QStringLiteral("straight after the view grew: target %1 from %2")
                                    .arg(scene->property("wheelRotationTarget").toDouble()).arg(before)));
    }

    // A ring that fits its box, lightning
    // look, Panel glow on. Its light was cut along the box in a straight
    // line (the view only grew for a shape reaching out of the box), and the
    // glow's shell showed through the see-through band, so the ring was
    // white. The light fades out before the view ends, and the band keeps
    // its look.
    void panelGlowShinesRoundThePanelNotThroughIt()
    {
        if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
            return; // Needs the private RHI session, like the scene gates above.
        const QString glyphFixture = QFINDTESTDATA("fixtures/icon-style-v1/assets/base.svg");
        // The generated platform of a free panel without a 3D theme, as
        // PanelRegistry::genericScene3D projects it, at its own camera.
        QVariantMap theme{
            {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
            {QStringLiteral("version"), 2}, {QStringLiteral("valid"), true},
            {QStringLiteral("id"), QStringLiteral("procedural-look")},
            {QStringLiteral("name"), QStringLiteral("Procedural look")},
            {QStringLiteral("capabilities"), QVariantMap{
                {QStringLiteral("rendererTiers"), QVariantList{QStringLiteral("true3d"), QStringLiteral("procedural2d")}},
                {QStringLiteral("preferredRendererTier"), QStringLiteral("procedural2d")},
                {QStringLiteral("fallbackRendererTiers"), QVariantList{QStringLiteral("procedural2d")}}}}};
        theme.insert(QStringLiteral("scene3D"), QVariantMap{
            {QStringLiteral("generic"), true}, {QStringLiteral("generated"), QVariantMap{}},
            {QStringLiteral("fieldOfView"), 40}, {QStringLiteral("cameraPitch"), 25},
            {QStringLiteral("cameraYaw"), 10}, {QStringLiteral("keyLightBrightness"), 1.2},
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
                property real glow: 0
                property string look: "lightning"
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120, iconTilesEnabled: false,
                                   scene3DQuality: "low", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DTransitions: false, scene3DMaterial: look, scene3DTexture: look,
                                   panelLookBackground: 0.5, panelGlow: glow, panelGlowReach: 0.7,
                                   scene3DIconDepth: 0})
                animationProfiles: ({reducedMotion: true})
                entryDelegateContext: ({hostKind: "free"})
                hostCapabilities: ({rotation: {available: true}, presentationMechanisms: [
                    {id: "open", available: true}]})
                orderedEntries: [0, 1, 2, 3].map(function(index) {
                    return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphFixture}
                })
            }
        )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/PanelGlowConsumer.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QQuickWindow window;
        // A grey desktop: light over it and anything darker both show.
        window.setColor(QColor(48, 48, 48));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("themeDefinition"), theme},
            {QStringLiteral("glyphFixture"), QUrl::fromLocalFile(glyphFixture).toString()}}));
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *scene = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(scene);
        scene->setParentItem(window.contentItem());
        scene->setPosition(QPointF(scene->width() * 1.5, scene->height() * 1.5));
        window.resize(qCeil(scene->width() * 4), qCeil(scene->height() * 4));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(),
                                 QStringLiteral("true3d"), 5000);
        auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
        QVERIFY(renderer);
        QTRY_VERIFY(renderer->property("materialTextureReady").toBool());
        QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 4);
        QTest::qWait(400);
        // The ring fits its box: without its light the view keeps less room
        // round it than the glow below needs.
        const double restingMargin = renderer->property("overflowX").toDouble();
        QVERIFY2(restingMargin < 12 * 16, qPrintable(QStringLiteral("resting margin %1").arg(restingMargin)));
        const QString frames = qEnvironmentVariable("ARCHDOCK_TEST_FRAME_DIR");
        const auto grab = [&](const QString &name) {
            const QImage frame = window.grabWindow();
            if (!frames.isEmpty())
                frame.save(frames + QLatin1Char('/') + name + QStringLiteral(".png"));
            return frame;
        };
        // The band: the ring between its inner and outer edge, in the box.
        const QRectF box = scene->mapRectToScene(QRectF(0, 0, scene->width(), scene->height()));
        const auto bandWhite = [&](const QImage &frame) {
            const qreal ratio = frame.devicePixelRatio();
            int white = 0, band = 0;
            for (int y = qCeil(box.top() * ratio); y < qFloor(box.bottom() * ratio); ++y)
                for (int x = qCeil(box.left() * ratio); x < qFloor(box.right() * ratio); ++x) {
                    const QRgb pixel = frame.pixel(x, y);
                    if (qAbs(qRed(pixel) - 48) + qAbs(qGreen(pixel) - 48) + qAbs(qBlue(pixel) - 48) < 12)
                        continue; // the desktop
                    ++band;
                    if (qMin(qRed(pixel), qMin(qGreen(pixel), qBlue(pixel))) > 225)
                        ++white;
                }
            return std::pair<int, int>(white, band);
        };
        const auto [unlitWhite, unlitBand] = bandWhite(grab(QStringLiteral("panel-glow-off")));
        QVERIFY2(unlitBand > 2000, qPrintable(QStringLiteral("%1 band pixels").arg(unlitBand)));
        auto *view = qobject_cast<QQuickItem *>(renderer);
        QVERIFY(view);

        // One glow intensity: its frame, how white the band is, the light
        // just past the box, and the largest step across the 3D view's
        // border (how much brighter it is inside).
        struct Lit { int white; double pastBox; double step; double margin; };
        const auto glowAt = [&](double amount, const QString &name) {
            scene->setProperty("glow", amount);
            QTest::qWait(900);
            const QImage lit = grab(name);
            const qreal ratio = lit.devicePixelRatio();
            const auto brightness = [&](qreal x, qreal y) {
                const QRgb pixel = lit.pixel(qBound(0, qRound(x * ratio), lit.width() - 1),
                                             qBound(0, qRound(y * ratio), lit.height() - 1));
                return (qRed(pixel) + qGreen(pixel) + qBlue(pixel)) / 3.0;
            };
            const QRectF drawn = view->mapRectToScene(renderer->property("drawnBounds").toRectF());
            double step = 0;
            for (int along = 0; along <= 20; ++along) {
                const qreal x = drawn.left() + drawn.width() * along / 20.0;
                const qreal y = drawn.top() + drawn.height() * along / 20.0;
                step = qMax(step, qAbs(brightness(drawn.left() + 2, y) - brightness(drawn.left() - 2, y)));
                step = qMax(step, qAbs(brightness(drawn.right() - 2, y) - brightness(drawn.right() + 2, y)));
                step = qMax(step, qAbs(brightness(x, drawn.top() + 2) - brightness(x, drawn.top() - 2)));
                step = qMax(step, qAbs(brightness(x, drawn.bottom() - 2) - brightness(x, drawn.bottom() + 2)));
            }
            const Lit result{bandWhite(lit).first, brightness(box.left() - 6, box.center().y()), step,
                             renderer->property("overflowX").toDouble()};
            qInfo("glow %.2f: margin %.0f for a reach of %.0f; band %d px, white %d (%d without glow); "
                  "%.1f just left of the box; drawn %.0f,%.0f %.0fx%.0f, step across its border %.1f",
                  amount, result.margin, renderer->property("glowReach").toDouble(), unlitBand, result.white,
                  unlitWhite, result.pastBox, drawn.x(), drawn.y(), drawn.width(), drawn.height(), step);
            return result;
        };
        const auto noEdge = [&](const Lit &lit, double amount) {
            // Room for the light round the ring, light past the box over the
            // desktop, and no edge in it: across the 3D view's border the
            // desktop brightens by no more than a trace.
            QVERIFY2(lit.margin >= renderer->property("glowReach").toDouble(),
                     qPrintable(QStringLiteral("glow %1: margin %2").arg(amount).arg(lit.margin)));
            QVERIFY2(lit.pastBox > 48 + 1,
                     qPrintable(QStringLiteral("glow %1: %2 just left of the box").arg(amount).arg(lit.pastBox)));
            QVERIFY2(lit.step <= 3,
                     qPrintable(QStringLiteral("glow %1: a step of %2 across the border").arg(amount).arg(lit.step)));
        };

        // At the middle of its range the look stays under its glow, the
        // band does not turn white; further up the light spreads over it.
        for (const double amount : {0.5, 0.8}) {
            const Lit lit = glowAt(amount, amount < 0.6 ? QStringLiteral("panel-glow-mid")
                                                        : QStringLiteral("panel-glow-on"));
            QVERIFY(renderer->property("bloomWanted").toBool());
            if (amount < 0.6)
                QVERIFY2(lit.white < unlitWhite + unlitBand / 4,
                         qPrintable(QStringLiteral("glow %1: %2 white band pixels, %3 without glow, of %4")
                                        .arg(amount).arg(lit.white).arg(unlitWhite).arg(unlitBand)));
            noEdge(lit, amount);
            if (QTest::currentTestFailed())
                return;
        }
        // At full intensity the light covers the shape itself, white-hot
        // ("if it's on max brightness or glow then it's okay").
        const Lit full = glowAt(1.0, QStringLiteral("panel-glow-max"));
        QVERIFY2(full.white > unlitBand / 2,
                 qPrintable(QStringLiteral("full glow: %1 white band pixels of %2").arg(full.white).arg(unlitBand)));
        noEdge(full, 1.0);
        if (QTest::currentTestFailed())
            return;
        QVERIFY2(unexpectedWarnings.isEmpty(), qPrintable(unexpectedWarnings.join(QLatin1Char('\n'))));
    }

    void packagedBuildFacts()
    {
        const QString module = importRoot() + QStringLiteral("/ArchDock/Rendering/");
        QVERIFY(QFileInfo::exists(module + QStringLiteral("RendererBuildConfig.qml")));
        QVERIFY(QFileInfo::exists(module + QStringLiteral("RendererCapabilityProbe.qml")));
        QCOMPARE(QFileInfo::exists(module + QStringLiteral("optional3d/RendererImportProbe.qml")),
                 ARCHDOCK_QUICK3D_BUILT != 0);
        for (const QString &file : {QStringLiteral("PanelScene3D.qml"), QStringLiteral("IconStyle3D.qml"),
                                    QStringLiteral("PictureRelief.qml"), QStringLiteral("picture-relief.vert"),
                                    QStringLiteral("picture-relief.frag"), QStringLiteral("FlowingSurface.qml"),
                                    QStringLiteral("flowing-surface.vert"), QStringLiteral("flowing-surface.frag")})
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

void RendererCapabilityTest::tileBubblesPlacementAndOrbitRenderUnderRhi()
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
        const QImage image = panelPixels(view);
        if (image.isNull()) return QImage{};
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
    // Every tile is a bubble round its icon: a closed glass shell in the
    // tile's shape with its outline round it, and no flat plate, whatever
    // a tile's thickness, bevel or old flat form ask for.
    auto *bubble = renderer->findChild<QObject *>("mesh-tile-bubble-0"); QVERIFY(bubble);
    auto *plate = renderer->findChild<QObject *>("mesh-tile-solid-0");
    QTRY_VERIFY(bubble->property("visible").toBool());
    QVERIFY(!plate || !plate->property("visible").toBool());
    QVERIFY(renderer->findChild<QObject *>("mesh-tile-bubble-outline-0"));
    QVariantMap values{{"iconTileThickness", 8.0}, {"iconTileBevel", 3.0}, {"iconTileForm", "flat"}};
    scene->setProperty("tileValues", values);
    QTest::qWait(100);
    QVERIFY(bubble->property("visible").toBool());
    QVERIFY(!plate || !plate->property("visible").toBool());
    QVERIFY(renderer->property("geometryWithinBudget").toBool());
    // A circle bubble grows past the tile's own width to hold the icon's
    // whole picture, and the bubble sits on the icon's middle, before the
    // tile's face.
    const auto widest = [](const QVariantMap &mesh) {
        double widest = 0;
        for (const QVariant &point : mesh.value("positions").toList())
            widest = qMax(widest, qAbs(point.toList().value(0).toDouble()));
        return widest;
    };
    QTRY_VERIFY(widest(plainValue(bubble->property("meshData")).toMap()) > 1.0);
    QVERIFY(bubble->property("position").value<QVector3D>().z() > 0);
    // Each bubble is as deep as it is wide, round every way: a ball round a
    // circle, a rhombus with depth round a diamond, never a flat lens.
    const auto deepest = [](const QVariantMap &mesh) {
        double deepest = 0;
        for (const QVariant &point : mesh.value("positions").toList())
            deepest = qMax(deepest, qAbs(point.toList().value(2).toDouble()));
        return deepest;
    };
    for (const QString &shape : {QStringLiteral("rounded"), QStringLiteral("square"), QStringLiteral("squircle"),
            QStringLiteral("circle"), QStringLiteral("hexagon"), QStringLiteral("diamond")}) {
        values.insert("iconShape", shape); scene->setProperty("tileValues", values);
        QVERIFY(!capture("bubble-" + shape).isNull());
        QVERIFY2(bubble->property("visible").toBool(), qPrintable(shape));
        const QVariantMap mesh = plainValue(bubble->property("meshData")).toMap();
        QVERIFY(mesh.value("indexes").toList().size() > 12);
        QVERIFY2(qAbs(deepest(mesh) - widest(mesh)) < 0.01, qPrintable(shape));
    }
    // The icon at 150 % widens its bubble, which deepens with it.
    values.insert("iconShape", "circle"); values.insert("iconTileIconScale", 150.0);
    scene->setProperty("tileValues", values);
    QTRY_VERIFY(widest(plainValue(bubble->property("meshData")).toMap()) > 1.6);
    QVERIFY(qAbs(deepest(plainValue(bubble->property("meshData")).toMap())
        - widest(plainValue(bubble->property("meshData")).toMap())) < 0.01);
    values.remove("iconTileIconScale");
    // The tile's glow wraps the bubble; no flat glowing card stands in it.
    values.insert("iconTileGlow", 0.2); scene->setProperty("tileValues", values);
    auto *bubbleGlow = renderer->findChild<QObject *>("mesh-tile-bubble-glow-0"); QVERIFY(bubbleGlow);
    QTRY_VERIFY(bubbleGlow->property("visible").toBool());
    auto *flatGlow = renderer->findChild<QObject *>("mesh-tile-glow-flat-0");
    QVERIFY(!flatGlow || !flatGlow->property("visible").toBool());
    values.remove("iconTileGlow");
    // An icon is a flat icon, a 3D icon or a hologram of either, never a 3D
    // icon with a flat copy of its picture: a 3D icon's glows are shells of
    // its solid, a flat icon's (a flat hologram's thin card too) the flat
    // glows.
    const auto shown = [&renderer](const char *name) {
        auto *model = renderer->findChild<QObject *>(QString::fromLatin1(name));
        return model && model->property("visible").toBool();
    };
    for (const bool hologram : {false, true}) {
        QVariantMap parts = values;
        parts.insert("scene3DIconSolid", true); parts.insert("scene3DIconDepth", 0.3);
        parts.insert("hologramIcons", hologram); parts.insert("hologramIconGlow", 1.0);
        parts.insert("iconGlow", 0.5);
        scene->setProperty("tileValues", parts);
        QTRY_VERIFY(shown("mesh-icon-solid-0"));
        QTRY_VERIFY(shown("mesh-icon-solid-glow-0"));
        QVERIFY(!shown("mesh-glyph-glow-0"));
        QVERIFY(!shown("mesh-hologram-glow-0"));
        QCOMPARE(shown("mesh-hologram-solid-glow-0"), hologram);
        QCOMPARE(renderer->findChild<QObject *>("mesh-glyph-0")->property("opacity").toDouble(), 0.0);
        parts.insert("scene3DIconSolid", false);
        scene->setProperty("tileValues", parts);
        QTRY_VERIFY(shown("mesh-glyph-glow-0"));
        QVERIFY(!shown("mesh-icon-solid-glow-0"));
        QVERIFY(!shown("mesh-hologram-solid-glow-0"));
        QCOMPARE(shown("mesh-hologram-glow-0"), hologram);
    }
    // An icon in a bubble faces the viewer, a turning hologram too, and so
    // does its bubble; without tiles the hologram turns.
    QVariantMap turning = values;
    turning.insert("hologramIcons", true); turning.insert("hologramIconTurning", "turn");
    scene->setProperty("tileValues", turning);
    auto *body = renderer->findChild<QObject *>("mesh-icon-body-0"); QVERIFY(body);
    QTest::qWait(600);
    QCOMPARE(body->property("eulerRotation").value<QVector3D>().y(), 0.0f);
    QCOMPARE(bubble->property("eulerRotation").value<QVector3D>().y(), 0.0f);
    turning.insert("iconTilesEnabled", false);
    scene->setProperty("tileValues", turning);
    QTRY_VERIFY(qAbs(body->property("eulerRotation").value<QVector3D>().y()) > 0.5);
    // The icon stands in its bubble's middle, a hologram too, a little
    // smaller in a bubble with straight sides than in a ball; the pointer
    // keeps its whole picture square.
    QVariantMap centred = values;
    centred.insert("iconShape", "rounded");
    centred.insert("scene3DIconSolid", true); centred.insert("scene3DIconDepth", 1.0);
    auto *solid = renderer->findChild<QObject *>("mesh-icon-solid-0"); QVERIFY(solid);
    auto *picture = renderer->findChild<QObject *>("mesh-glyph-0"); QVERIFY(picture);
    for (const bool hologram : {true, false}) {
        centred.insert("hologramIcons", hologram);
        scene->setProperty("tileValues", centred);
        QTRY_VERIFY(solid->property("visible").toBool() && bubble->property("visible").toBool());
        QTRY_VERIFY2(qAbs(bubble->property("position").value<QVector3D>().z()
            - solid->property("position").value<QVector3D>().z()) < 0.05, hologram ? "hologram" : "solid");
    }
    const double straight = picture->property("scale").value<QVector3D>().x();
    // The pointer still finds the icon near its whole picture square's
    // edge, past the smaller picture drawn in the bubble.
    const auto edgeHits = [&]() {
        const QVariantMap rest = plainValue(renderer->property("projectedEntryGeometry")).toList().value(0).toMap();
        QVariant hit;
        QMetaObject::invokeMethod(renderer, "containsInputPoint", Q_RETURN_ARG(QVariant, hit),
            Q_ARG(QVariant, QVariant(QPointF(rest.value("centerX").toDouble() + 0.46 * rest.value("width").toDouble(),
                rest.value("centerY").toDouble()))));
        return hit.toBool();
    };
    QTRY_VERIFY(edgeHits());
    centred.insert("iconShape", "circle");
    scene->setProperty("tileValues", centred);
    QTRY_VERIFY(qAbs(straight / picture->property("scale").value<QVector3D>().x() - 0.85) < 0.01);
    values.insert("iconShape", "circle"); scene->setProperty("tileValues", values);
    const QImage baseline = capture("baseline"); QVERIFY(!baseline.isNull());
    // The glass takes a light tint of the tile's fill colour and is as clear
    // as the tile's opacity says.
    const auto glass = [&bubble]() { return plainValue(bubble->property("materialData")).toMap(); };
    QVERIFY(glass().value("baseColor").value<QColor>().redF() < glass().value("baseColor").value<QColor>().greenF());
    values.insert("iconTileColor", "#c02060"); scene->setProperty("tileValues", values);
    QTRY_VERIFY(glass().value("baseColor").value<QColor>().redF() > glass().value("baseColor").value<QColor>().greenF());
    QVERIFY(changed(baseline, capture("bubble-tinted")) >= 20);
    const double clearest = glass().value("opacity").toDouble();
    values.insert("iconTileOpacity", 0.5); scene->setProperty("tileValues", values);
    QTRY_VERIFY(qAbs(glass().value("opacity").toDouble() - clearest / 2) < 0.001);
    values.insert("iconTileColor", "#7895b0"); values.insert("iconTileOpacity", 1.0);
    scene->setProperty("tileValues", values);
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

void RendererCapabilityTest::decorationsStandAsSolids_data()
{
    QTest::addColumn<QString>("decoration");
    for (const char *id : {"carina-nebula", "yellow-gold-filigree", "starship-red-core", "oak-branch"})
        QTest::newRow(id) << QString::fromLatin1(id);
}

void RendererCapabilityTest::decorationsStandAsSolids()
{
    QFETCH(QString, decoration);
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
        return; // A decoration stands as a solid only in the real RHI scene.
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_DECORATIONS"))
        QSKIP("Runs in its own private session (ARCHDOCK_RENDERING_DECORATIONS).");
    QTest::failOnWarning(QRegularExpression(
        QStringLiteral("Failed to compile|Failed to parse shader|Failed to build graphics pipeline")));
    // Icon-shaped pictures (a terminal, a folder, a logo), each in its own
    // colours, as a panel's icons are.
    QStringList glyphs;
    for (const char *name : {"data/truth-glyph-terminal.svg", "data/truth-glyph-files.svg", "data/truth-glyph-logo.svg"})
    {
        const QString file = QFINDTESTDATA(name);
        QVERIFY2(!file.isEmpty(), name);
        glyphs << QUrl::fromLocalFile(file).toString();
    }
    QVariantMap theme{
        {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
        {QStringLiteral("version"), 2}, {QStringLiteral("valid"), true},
        {QStringLiteral("id"), QStringLiteral("procedural-look")},
        {QStringLiteral("name"), QStringLiteral("Procedural look")},
        {QStringLiteral("capabilities"), QVariantMap{
            {QStringLiteral("rendererTiers"), QVariantList{QStringLiteral("true3d"), QStringLiteral("procedural2d")}},
            {QStringLiteral("preferredRendererTier"), QStringLiteral("procedural2d")},
            {QStringLiteral("fallbackRendererTiers"), QVariantList{QStringLiteral("procedural2d")}}}},
        {QStringLiteral("scene3D"), QVariantMap{
            {QStringLiteral("generic"), true}, {QStringLiteral("generated"), QVariantMap{}},
            {QStringLiteral("fieldOfView"), 40}, {QStringLiteral("cameraPitch"), 25},
            {QStringLiteral("cameraYaw"), 0}, {QStringLiteral("keyLightBrightness"), 1.2},
            {QStringLiteral("fillLightBrightness"), 0.45},
            {QStringLiteral("defaultQuality"), QStringLiteral("high")},
            {QStringLiteral("transitions"), false}, {QStringLiteral("parts"), QVariantList{}}}},
        {QStringLiteral("scene3DResources"), QVariantMap{
            {QStringLiteral("material"), QVariantMap{
                {QStringLiteral("format"), QStringLiteral("org.archdock.material")},
                {QStringLiteral("version"), 1}, {QStringLiteral("baseColor"), QStringLiteral("#406080")},
                {QStringLiteral("emissiveColor"), QStringLiteral("#000000")},
                {QStringLiteral("emissiveStrength"), 0.0}, {QStringLiteral("metalness"), 0.3},
                {QStringLiteral("roughness"), 0.45}}},
            {QStringLiteral("parts"), QVariantList{}}, {QStringLiteral("indexBudget"), 262144}}}};

    QQmlEngine engine;
    engine.addImportPath(importRoot());
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import ArchDock.Rendering 1.0
        PanelScene {
            required property var glyphs
            required property string decorationId
            panelDefinition: ({rendererTier: "true3d", layout: "circular", layoutRadius: 150,
                               scene3DQuality: "high", iconSize: 40, layoutPadding: 10, appearance: "neon",
                               scene3DCameraPitch: 25, scene3DTransitions: false, scene3DIconDepth: 0,
                               panelDecoration: decorationId, panelDecorationAnimation: false})
            entryDelegateContext: ({hostKind: "free"})
            hostCapabilities: ({rotation: {available: true}})
            orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphs[index % glyphs.length]}
            })
        }
    )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/DecorationConsumer.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    const QColor background(QStringLiteral("#1b2333"));
    window.setColor(background);
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("themeDefinition"), theme},
        {QStringLiteral("decorationId"), decoration},
        {QStringLiteral("glyphs"), glyphs}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto *scene = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(scene);
    scene->setParentItem(window.contentItem());
    window.resize(qCeil(scene->width()), qCeil(scene->height()));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"), 5000);
    auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
    QVERIFY(renderer);
    QTRY_VERIFY_WITH_TIMEOUT(renderer->property("decorationShown").toBool(), 60000);
    QObject *node = renderer->findChild<QObject *>(QStringLiteral("mesh-platform-decoration-node"));
    QVERIFY(node);
    QVERIFY(node->property("triangleCount").toInt() > 0);
    QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 6);
    // A few frames for the solid and its maps to reach the graphics card.
    for (int k = 0; k < 4; ++k)
    {
        QSignalSpy swapped(&window, &QQuickWindow::frameSwapped);
        window.update();
        QVERIFY(swapped.wait(5000));
    }
    QTest::qWait(300);
    const QImage frame = window.grabWindow();
    QVERIFY(!frame.isNull());
    const double scale = frame.width() / std::max(1.0, double(window.width()));

    // Across the band between every two icons (out from the ring's middle
    // through the middle of the two), something is drawn, not the window.
    QList<QPointF> icons;
    QPointF middle;
    for (const QVariant &value : plainValue(renderer->property("projectedEntryGeometry")).toList())
    {
        const QVariantMap rect = value.toMap();
        icons << scene->mapToScene(QPointF(rect.value("centerX").toDouble(), rect.value("centerY").toDouble()));
        middle += icons.last();
    }
    middle /= icons.size();
    QStringList bare;
    for (int k = 0; k < icons.size(); ++k)
    {
        const QPointF between = (icons[k] + icons[(k + 1) % icons.size()]) / 2;
        QPointF out = between - middle;
        out /= std::max(1.0, std::hypot(out.x(), out.y()));
        int drawn = 0;
        for (int step = -30; step <= 30; ++step)
        {
            const QPoint at = ((between + out * step) * scale).toPoint();
            if (!frame.rect().contains(at)) continue;
            const QColor c = frame.pixelColor(at);
            drawn += std::abs(c.red() - background.red()) + std::abs(c.green() - background.green())
                     + std::abs(c.blue() - background.blue()) > 30;
        }
        if (drawn < 6) bare << QStringLiteral("between icons %1 and %2 (%3 drawn)").arg(k).arg((k + 1) % icons.size()).arg(drawn);
    }
    const QString evidence = qEnvironmentVariable("ARCHDOCK_RENDERING_LOG_DIR");
    if (!evidence.isEmpty()) frame.save(QDir(evidence).filePath(QStringLiteral("decoration-3d-%1.png").arg(decoration)));
    QVERIFY2(bare.isEmpty(), qPrintable(decoration + QStringLiteral(" is not drawn ") + bare.join(QStringLiteral(", "))));
}

void RendererCapabilityTest::hologramsDrawAsLight_data()
{
    QTest::addColumn<QVariantMap>("settings");
    QTest::newRow("icons-on-plates") << QVariantMap{{QStringLiteral("hologramIcons"), true},
        {QStringLiteral("pedestalShape"), QStringLiteral("plate")}};
    QTest::newRow("icons-on-the-panel-cyan") << QVariantMap{{QStringLiteral("hologramIcons"), true},
        {QStringLiteral("hologramIconColours"), QStringLiteral("cyan")}};
    QTest::newRow("everything-on-projectors") << QVariantMap{{QStringLiteral("hologramIcons"), true},
        {QStringLiteral("pedestalShape"), QStringLiteral("projector")},
        {QStringLiteral("hologramPedestals"), true}, {QStringLiteral("hologramPanel"), true},
        {QStringLiteral("hologramIconTurning"), QStringLiteral("sway")}};
    // A decoration standing in the platform's place is drawn as light too.
    QTest::newRow("decorated-panel") << QVariantMap{{QStringLiteral("hologramIcons"), true},
        {QStringLiteral("panelDecoration"), QStringLiteral("carina-nebula")},
        {QStringLiteral("hologramPanel"), true}};
    // 3D icons as holograms: each icon's solid made of light.
    QTest::newRow("3d-icons") << QVariantMap{{QStringLiteral("hologramIcons"), true},
        {QStringLiteral("scene3DIconSolid"), true}, {QStringLiteral("scene3DIconDepth"), 0.6}};
}

void RendererCapabilityTest::hologramsDrawAsLight()
{
    QFETCH(QVariantMap, settings);
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
        return; // Holograms are drawn as light only in the real RHI scene.
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_DECORATIONS"))
        QSKIP("Runs in its own private session (ARCHDOCK_RENDERING_DECORATIONS).");
    QTest::failOnWarning(QRegularExpression(
        QStringLiteral("Failed to compile|Failed to parse shader|Failed to build graphics pipeline")));
    // Icon-shaped pictures (a terminal, a folder, a logo), each in its own
    // colours, as a panel's icons are.
    QStringList glyphs;
    for (const char *name : {"data/truth-glyph-terminal.svg", "data/truth-glyph-files.svg", "data/truth-glyph-logo.svg"})
    {
        const QString file = QFINDTESTDATA(name);
        QVERIFY2(!file.isEmpty(), name);
        glyphs << QUrl::fromLocalFile(file).toString();
    }
    QVariantMap theme{
        {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
        {QStringLiteral("version"), 2}, {QStringLiteral("valid"), true},
        {QStringLiteral("id"), QStringLiteral("procedural-look")},
        {QStringLiteral("name"), QStringLiteral("Procedural look")},
        {QStringLiteral("capabilities"), QVariantMap{
            {QStringLiteral("rendererTiers"), QVariantList{QStringLiteral("true3d"), QStringLiteral("procedural2d")}},
            {QStringLiteral("preferredRendererTier"), QStringLiteral("procedural2d")},
            {QStringLiteral("fallbackRendererTiers"), QVariantList{QStringLiteral("procedural2d")}}}},
        {QStringLiteral("scene3D"), QVariantMap{
            {QStringLiteral("generic"), true}, {QStringLiteral("generated"), QVariantMap{}},
            {QStringLiteral("fieldOfView"), 40}, {QStringLiteral("cameraPitch"), 25},
            {QStringLiteral("cameraYaw"), 0}, {QStringLiteral("keyLightBrightness"), 1.2},
            {QStringLiteral("fillLightBrightness"), 0.45},
            {QStringLiteral("defaultQuality"), QStringLiteral("high")},
            {QStringLiteral("transitions"), false}, {QStringLiteral("parts"), QVariantList{}}}},
        {QStringLiteral("scene3DResources"), QVariantMap{
            {QStringLiteral("material"), QVariantMap{
                {QStringLiteral("format"), QStringLiteral("org.archdock.material")},
                {QStringLiteral("version"), 1}, {QStringLiteral("baseColor"), QStringLiteral("#406080")},
                {QStringLiteral("emissiveColor"), QStringLiteral("#000000")},
                {QStringLiteral("emissiveStrength"), 0.0}, {QStringLiteral("metalness"), 0.3},
                {QStringLiteral("roughness"), 0.45}}},
            {QStringLiteral("parts"), QVariantList{}}, {QStringLiteral("indexBudget"), 262144}}}};

    QQmlEngine engine;
    engine.addImportPath(importRoot());
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import ArchDock.Rendering 1.0
        PanelScene {
            required property var glyphs
            required property var hologramSettings
            panelDefinition: Object.assign({rendererTier: "true3d", layout: "circular", layoutRadius: 150,
                               scene3DQuality: "high", iconSize: 40, layoutPadding: 10, appearance: "glass",
                               scene3DCameraPitch: 25, scene3DTransitions: false}, hologramSettings)
            entryDelegateContext: ({hostKind: "free"})
            hostCapabilities: ({rotation: {available: true}})
            orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphs[index % glyphs.length]}
            })
        }
    )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/HologramConsumer.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    const QColor background(QStringLiteral("#1b2333"));
    window.setColor(background);
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("themeDefinition"), theme},
        {QStringLiteral("hologramSettings"), settings},
        {QStringLiteral("glyphs"), glyphs}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto *scene = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(scene);
    scene->setParentItem(window.contentItem());
    window.resize(qCeil(scene->width()), qCeil(scene->height()));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"), 5000);
    auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
    QVERIFY(renderer);
    QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 6);
    QVERIFY(renderer->property("hologramShown").toBool());
    const bool decorated = settings.contains(QStringLiteral("panelDecoration"));
    if (decorated)
    {
        QTRY_VERIFY_WITH_TIMEOUT(renderer->property("decorationShown").toBool(), 60000);
        auto *solid = renderer->findChild<QObject *>(QStringLiteral("mesh-platform-decoration"));
        QVERIFY(solid);
        QQmlListReference materials(solid, "materials");
        QVERIFY(materials.isValid() && materials.count() == 1);
        QCOMPARE(materials.at(0)->objectName(), QStringLiteral("mesh-platform-decoration-hologram"));
    }
    // Every icon has its light and its lens, and its solid wears the hologram.
    for (int index = 0; index < 6; ++index)
    {
        auto *light = renderer->findChild<QObject *>(QStringLiteral("mesh-hologram-light-%1").arg(index));
        QVERIFY2(light && light->property("visible").toBool(), qPrintable(QStringLiteral("light %1").arg(index)));
        const bool stand = settings.contains(QStringLiteral("pedestalShape"));
        const QString lens = stand ? QStringLiteral("mesh-hologram-opening-panel-%1").arg(index)
                                   : QStringLiteral("mesh-hologram-opening-on-panel-%1").arg(index);
        const QString lensFacing = QStringLiteral("mesh-hologram-opening-facing-%1").arg(index);
        auto *opening = renderer->findChild<QObject *>(lens);
        auto *openingFacing = renderer->findChild<QObject *>(lensFacing);
        QVERIFY2((opening && opening->property("visible").toBool())
                     || (openingFacing && openingFacing->property("visible").toBool()),
                 qPrintable(QStringLiteral("lens %1").arg(index)));
    }
    // A 3D icon's hologram is its solid made of light, seen from both sides;
    // a flat hologram's thin card wears its picture's light.
    const bool solidIcons = settings.value(QStringLiteral("scene3DIconSolid")).toBool();
    for (int index = 0; index < 6; ++index)
    {
        auto *solid = renderer->findChild<QObject *>(QStringLiteral("mesh-icon-solid-%1").arg(index));
        QVERIFY(solid);
        QTRY_VERIFY2_WITH_TIMEOUT(solid->property("visible").toBool(), qPrintable(QStringLiteral("solid %1").arg(index)),
                                  10000);
        QQmlListReference materials(solid, "materials");
        QVERIFY(materials.isValid() && materials.count() == 1);
        QCOMPARE(materials.at(0)->objectName() == QStringLiteral("mesh-icon-solid-hologram-%1").arg(index), solidIcons);
        if (solidIcons)
            QCOMPARE(materials.at(0)->property("cullMode").toInt(), 3); // Material.NoCulling
    }
    for (int k = 0; k < 6; ++k)
    {
        QSignalSpy swapped(&window, &QQuickWindow::frameSwapped);
        window.update();
        QVERIFY(swapped.wait(5000));
    }
    QTest::qWait(400);
    const QImage frame = window.grabWindow();
    QVERIFY(!frame.isNull());
    const double scale = frame.width() / std::max(1.0, double(window.width()));
    // Light is added at every icon: brighter than the window round it.
    QStringList dark;
    int index = 0;
    for (const QVariant &value : plainValue(renderer->property("projectedEntryGeometry")).toList())
    {
        const QVariantMap rect = value.toMap();
        const QPointF centre = scene->mapToScene(QPointF(rect.value("centerX").toDouble(), rect.value("centerY").toDouble()));
        const double half = rect.value("width").toDouble() * 0.7;
        int bright = 0;
        for (int dy = -6; dy <= 6; ++dy)
            for (int dx = -6; dx <= 6; ++dx)
            {
                const QPoint at = ((centre + QPointF(dx * half / 6, dy * half / 6)) * scale).toPoint();
                if (!frame.rect().contains(at)) continue;
                const QColor c = frame.pixelColor(at);
                bright += c.red() + c.green() + c.blue() > background.red() + background.green() + background.blue() + 120;
            }
        if (bright < 4) dark << QStringLiteral("icon %1 (%2 lit)").arg(index).arg(bright);
        ++index;
    }
    // A decorated panel's band is drawn between every two icons, as light:
    // out from the ring's middle between them (the icons float above the
    // band, so the band lies anywhere along the way out).
    if (decorated)
    {
        QList<QPointF> icons;
        QPointF middle;
        for (const QVariant &value : plainValue(renderer->property("projectedEntryGeometry")).toList())
        {
            const QVariantMap rect = value.toMap();
            icons << scene->mapToScene(QPointF(rect.value("centerX").toDouble(), rect.value("centerY").toDouble()));
            middle += icons.last();
        }
        middle /= icons.size();
        for (int k = 0; k < icons.size(); ++k)
        {
            const QPointF between = (icons[k] + icons[(k + 1) % icons.size()]) / 2;
            const QPointF out = between - middle;
            int drawn = 0;
            for (int step = 50; step <= 170; ++step)
            {
                const QPoint at = ((middle + out * (step / 100.0)) * scale).toPoint();
                if (!frame.rect().contains(at)) continue;
                const QColor c = frame.pixelColor(at);
                drawn += std::abs(c.red() - background.red()) + std::abs(c.green() - background.green())
                         + std::abs(c.blue() - background.blue()) > 30;
            }
            if (drawn < 6) dark << QStringLiteral("the band between icons %1 and %2 (%3 drawn)").arg(k)
                                       .arg((k + 1) % icons.size()).arg(drawn);
        }
    }
    const QString evidence = qEnvironmentVariable("ARCHDOCK_RENDERING_LOG_DIR");
    if (!evidence.isEmpty())
        frame.save(QDir(evidence).filePath(QStringLiteral("hologram-3d-%1.png").arg(QString::fromLatin1(QTest::currentDataTag()))));
    QVERIFY2(dark.isEmpty(), qPrintable(QStringLiteral("no hologram light at ") + dark.join(QStringLiteral(", "))));
}

// A flat panel's hologram is drawn as large as the panel: the glow's blur
// widens the layer's picture past the panel to make room for itself, and the
// light must still read the panel's own part of it, not squeeze all of it in.
void RendererCapabilityTest::flatHologramsKeepTheirSize()
{
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI"))
        return; // The glow's blur and the light are drawn only in the real RHI scene.
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_DECORATIONS"))
        QSKIP("Runs in its own private session (ARCHDOCK_RENDERING_DECORATIONS).");
    QTest::failOnWarning(QRegularExpression(
        QStringLiteral("Failed to compile|Failed to parse shader|Failed to build graphics pipeline")));
    QStringList glyphs;
    for (const char *name : {"data/truth-glyph-terminal.svg", "data/truth-glyph-files.svg", "data/truth-glyph-logo.svg"})
    {
        const QString file = QFINDTESTDATA(name);
        QVERIFY2(!file.isEmpty(), name);
        glyphs << QUrl::fromLocalFile(file).toString();
    }
    QQmlEngine engine;
    engine.addImportPath(importRoot());
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import ArchDock.Rendering 1.0
        PanelScene {
            required property var glyphs
            property var settings: ({})
            panelDefinition: Object.assign({rendererTier: "procedural2d", iconSize: 44, layoutPadding: 10,
                                            appearance: "glass"}, settings)
            entryDelegateContext: ({hostKind: "free"})
            orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphs[index % glyphs.length]}
            })
        }
    )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/FlatHologramConsumer.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QColor background(QStringLiteral("#1b2333"));
    const QString evidence = qEnvironmentVariable("ARCHDOCK_RENDERING_LOG_DIR");
    // The items the scene draws, the layers' effects among them.
    const std::function<QQuickItem *(QQuickItem *, const QString &)> find =
        [&find](QQuickItem *item, const QString &name) -> QQuickItem * {
            if (!item) return nullptr;
            if (item->objectName() == name) return item;
            for (QQuickItem *child : item->childItems())
                if (QQuickItem *found = find(child, name)) return found;
            return nullptr;
        };
    const auto drawn = [&](QQuickWindow &window, const QString &name) {
        for (int k = 0; k < 6; ++k)
        {
            QSignalSpy swapped(&window, &QQuickWindow::frameSwapped);
            window.update();
            if (!swapped.wait(5000)) return QImage();
        }
        QTest::qWait(300);
        const QImage frame = window.grabWindow();
        if (!evidence.isEmpty()) frame.save(QDir(evidence).filePath(QStringLiteral("flat-hologram-%1.png").arg(name)));
        return frame;
    };
    // What is drawn over the window's colour.
    const auto extent = [&](const QImage &frame) {
        QRect box;
        for (int y = 0; y < frame.height(); ++y)
            for (int x = 0; x < frame.width(); ++x)
            {
                const QColor c = frame.pixelColor(x, y);
                if (std::abs(c.red() - background.red()) + std::abs(c.green() - background.green())
                        + std::abs(c.blue() - background.blue()) > 40)
                    box |= QRect(x, y, 1, 1);
            }
        return box;
    };
    const auto open = [&](QQuickWindow &window, const QVariantMap &settings) -> std::unique_ptr<QObject> {
        std::unique_ptr<QObject> object(component.createWithInitialProperties({
            {QStringLiteral("glyphs"), glyphs}, {QStringLiteral("settings"), settings}}));
        if (auto *scene = qobject_cast<QQuickItem *>(object.get()))
        {
            scene->setParentItem(window.contentItem());
            window.resize(qCeil(scene->width()) + 160, qCeil(scene->height()) + 160);
            scene->setPosition(QPointF(80, 80));
        }
        return object;
    };

    // A bar: its hologram, its glow off, covers what the panel covers.
    {
        QQuickWindow window;
        window.setColor(background);
        auto plain = open(window, {{QStringLiteral("layout"), QStringLiteral("horizontal")}});
        QVERIFY(plain);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const QRect panel = extent(drawn(window, QStringLiteral("bar-plain")));
        plain.reset();
        auto hologram = open(window, {{QStringLiteral("layout"), QStringLiteral("horizontal")},
                                      {QStringLiteral("hologramPanel"), true},
                                      {QStringLiteral("hologramPanelGlow"), 0.0}});
        QVERIFY(hologram);
        const QRect light = extent(drawn(window, QStringLiteral("bar-hologram")));
        QVERIFY2(panel.isValid() && light.isValid(), "both are drawn");
        QVERIFY2(light.height() >= panel.height() * 0.85 && light.width() >= panel.width() * 0.9,
                 qPrintable(QStringLiteral("the panel is %1x%2, its hologram %3x%4").arg(panel.width())
                     .arg(panel.height()).arg(light.width()).arg(light.height())));
        // With its glow, the light reads the panel's own part of the layer's
        // widened picture.
        hologram.reset();
        hologram = open(window, {{QStringLiteral("layout"), QStringLiteral("horizontal")},
                                 {QStringLiteral("hologramPanel"), true}});
        auto *scene = qobject_cast<QQuickItem *>(hologram.get());
        drawn(window, QStringLiteral("bar-hologram-glow"));
        QQuickItem *lightItem = find(scene, QStringLiteral("hologram-light"));
        QVERIFY(lightItem);
        auto *source = qvariant_cast<QQuickItem *>(lightItem->parentItem()->property("source"));
        QVERIFY(source);
        const QRectF picture = source->property("sourceRect").toRectF();
        const QVector4D area = lightItem->property("area").value<QVector4D>();
        if (picture.width() > lightItem->width())
        {
            QVERIFY(qAbs(area.z() - lightItem->width() / picture.width()) < 1e-3);
            QVERIFY(qAbs(area.x() + picture.x() / picture.width()) < 1e-3);
        }
    }

    // A decorated ring: each of Red Core's lenses drawn as light where the
    // lens is, not inside it (squeezed, the reddest light near a lens lay a
    // tenth of the ring's size inward of it).
    {
        QQuickWindow window;
        window.setColor(background);
        const QVariantMap ring{{QStringLiteral("layout"), QStringLiteral("circular")},
                               {QStringLiteral("layoutRadius"), 150},
                               {QStringLiteral("panelDecoration"), QStringLiteral("starship-red-core")}};
        QMap<int, QPoint> lenses;
        for (const bool hologram : {false, true})
        {
            QVariantMap settings = ring;
            if (hologram) settings.insert(QStringLiteral("hologramPanel"), true);
            auto object = open(window, settings);
            QVERIFY(object);
            auto *scene = qobject_cast<QQuickItem *>(object.get());
            if (!window.isVisible())
            {
                window.show();
                QVERIFY(QTest::qWaitForWindowExposed(&window));
            }
            QTRY_VERIFY_WITH_TIMEOUT(find(scene, QStringLiteral("panel-decoration"))
                && find(scene, QStringLiteral("panel-decoration"))->property("rendererReady").toBool(), 60000);
            const QImage frame = drawn(window, hologram ? QStringLiteral("ring-hologram") : QStringLiteral("ring-plain"));
            const double scale = frame.width() / std::max(1.0, double(window.width()));
            for (int index = 0; index < 6; ++index)
            {
                QQuickItem *spot = find(scene, QStringLiteral("panel-decoration-light-spot-%1").arg(index));
                QVERIFY(spot);
                const QPoint at = (spot->mapToScene(QPointF(spot->width() / 2, spot->height() / 2)) * scale).toPoint();
                if (!hologram)
                {
                    lenses.insert(index, at);
                    continue;
                }
                QCOMPARE(at, lenses.value(index));
                // The reddest light round the lens.
                const int reach = qRound(30 * scale);
                QPoint reddest = at;
                int most = -255;
                for (int dy = -reach; dy <= reach; ++dy)
                    for (int dx = -reach; dx <= reach; ++dx)
                    {
                        const QPoint point = at + QPoint(dx, dy);
                        if (!frame.rect().contains(point)) continue;
                        const QColor c = frame.pixelColor(point);
                        const int red = c.red() - (c.green() + c.blue()) / 2;
                        if (red > most) { most = red; reddest = point; }
                    }
                const double away = std::hypot(reddest.x() - at.x(), reddest.y() - at.y()) / scale;
                QVERIFY2(most > 20 && away <= 8,
                         qPrintable(QStringLiteral("lens %1: its reddest light (%2) lies %3 px from it")
                             .arg(index).arg(most).arg(away, 0, 'f', 1)));
            }
        }
    }
}

void RendererCapabilityTest::threeDHologramsOnFlatPanels()
{
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
        return; // A flat panel's 3D icons are drawn only in the real RHI scene.
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_DECORATIONS"))
        QSKIP("Runs in its own private session (ARCHDOCK_RENDERING_DECORATIONS).");
    QTest::failOnWarning(QRegularExpression(
        QStringLiteral("Failed to compile|Failed to parse shader|Failed to build graphics pipeline")));
    QStringList glyphs;
    for (const char *name : {"data/truth-glyph-terminal.svg", "data/truth-glyph-files.svg", "data/truth-glyph-logo.svg"})
    {
        const QString file = QFINDTESTDATA(name);
        QVERIFY2(!file.isEmpty(), name);
        glyphs << QUrl::fromLocalFile(file).toString();
    }
    QQmlEngine engine;
    engine.addImportPath(importRoot());
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import ArchDock.Rendering 1.0
        PanelScene {
            required property var glyphs
            property var settings: ({})
            panelDefinition: Object.assign({rendererTier: "procedural2d", layout: "horizontal", iconSize: 44,
                                            layoutPadding: 10, appearance: "glass"}, settings)
            entryDelegateContext: ({hostKind: "free"})
            orderedEntries: [0, 1, 2].map(function(index) {
                return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyphs[index]}
            })
        }
    )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/FlatSolidHologramConsumer.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    // Only the 3D icons and their hologram: no projector light under them.
    QVariantMap settings{{QStringLiteral("hologramIcons"), true}, {QStringLiteral("hologramBeam"), false},
        {QStringLiteral("scene3DIconSolid"), true}, {QStringLiteral("scene3DIconDepth"), 0.6}};
    QQuickWindow window;
    const QColor background(QStringLiteral("#1b2333"));
    window.setColor(background);
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("glyphs"), glyphs}, {QStringLiteral("settings"), settings}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto *scene = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(scene);
    scene->setParentItem(window.contentItem());
    window.resize(qCeil(scene->width()) + 160, qCeil(scene->height()) + 160);
    scene->setPosition(QPointF(80, 80));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    const std::function<void(QQuickItem *, const QString &, QList<QQuickItem *> &)> collect =
        [&collect](QQuickItem *item, const QString &name, QList<QQuickItem *> &found) {
            if (!item) return;
            if (item->objectName() == name) found << item;
            for (QQuickItem *child : item->childItems()) collect(child, name, found);
        };
    const auto items = [&](const QString &name) {
        QList<QQuickItem *> found;
        collect(scene, name, found);
        return found;
    };
    const auto allShown = [&](const QString &name, bool shown) {
        const QList<QQuickItem *> found = items(name);
        return found.size() == 3 && std::all_of(found.cbegin(), found.cend(), [shown](QQuickItem *item) {
            return item->isVisible() == shown;
        });
    };
    // Each icon's solid in its own view, and its model's material.
    const auto solidMaterial = [&](int index) -> QString {
        const QList<QQuickItem *> layers = items(QStringLiteral("icon-layer-solid"));
        if (index >= layers.size()) return QStringLiteral("(no layer)");
        for (QQuickItem *child : layers.at(index)->childItems())
        {
            QQuickItem *view = qvariant_cast<QQuickItem *>(child->property("item"));
            QObject *model = view ? view->findChild<QObject *>(QStringLiteral("flat-icon-solid")) : nullptr;
            if (!model) continue;
            QQmlListReference materials(model, "materials");
            return materials.isValid() && materials.count() == 1 ? materials.at(0)->objectName() : QStringLiteral("(none)");
        }
        return QStringLiteral("(no view)");
    };
    const auto drawn = [&](const QString &name) {
        for (int k = 0; k < 6; ++k)
        {
            QSignalSpy swapped(&window, &QQuickWindow::frameSwapped);
            window.update();
            if (!swapped.wait(5000)) return QImage();
        }
        QTest::qWait(300);
        const QImage frame = window.grabWindow();
        const QString evidence = qEnvironmentVariable("ARCHDOCK_RENDERING_LOG_DIR");
        if (!evidence.isEmpty()) frame.save(QDir(evidence).filePath(QStringLiteral("flat-solid-hologram-%1.png").arg(name)));
        return frame;
    };

    // The hologram is each icon's solid made of light; its flat picture made
    // light is not drawn.
    QTRY_VERIFY_WITH_TIMEOUT(allShown(QStringLiteral("icon-layer-solid"), true), 10000);
    QVERIFY(allShown(QStringLiteral("icon-hologram"), false));
    for (int index = 0; index < 3; ++index)
        QTRY_COMPARE_WITH_TIMEOUT(solidMaterial(index), QStringLiteral("flat-icon-solid-hologram"), 5000);
    // Its light is added where each icon stands.
    const QImage frame = drawn(QStringLiteral("on"));
    QVERIFY(!frame.isNull());
    const double scale = frame.width() / std::max(1.0, double(window.width()));
    for (QQuickItem *layer : items(QStringLiteral("icon-layer-solid")))
    {
        int lit = 0;
        for (int dy = -6; dy <= 6; ++dy)
            for (int dx = -6; dx <= 6; ++dx)
            {
                const QPointF at = layer->mapToScene(QPointF(layer->width() * (0.5 + dx / 16.0),
                                                             layer->height() * (0.5 + dy / 16.0)));
                const QPoint pixel = (at * scale).toPoint();
                if (!frame.rect().contains(pixel)) continue;
                const QColor c = frame.pixelColor(pixel);
                lit += c.red() + c.green() + c.blue() > background.red() + background.green() + background.blue() + 90;
            }
        QVERIFY2(lit >= 12, qPrintable(QStringLiteral("only %1 lit points on an icon").arg(lit)));
    }
    // Without the hologram the solid is lit as a 3D icon again.
    settings.insert(QStringLiteral("hologramIcons"), false);
    object->setProperty("settings", settings);
    for (int index = 0; index < 3; ++index)
        QTRY_VERIFY_WITH_TIMEOUT(solidMaterial(index) != QStringLiteral("flat-icon-solid-hologram"), 5000);
    // Without 3D icons the hologram is the flat picture made light.
    settings.insert(QStringLiteral("hologramIcons"), true);
    settings.insert(QStringLiteral("scene3DIconSolid"), false);
    object->setProperty("settings", settings);
    QTRY_VERIFY_WITH_TIMEOUT(allShown(QStringLiteral("icon-hologram"), true), 5000);
    QVERIFY(allShown(QStringLiteral("icon-layer-solid"), false));
    drawn(QStringLiteral("flat"));
}

// A 3D panel turned with the mouse wheel keeps its layout: its platform's
// size follows how far its stands rise, and a turn only wobbles that in its
// last digits (the camera's directions in the panel's frame come from a
// single-precision rotation), which once laid the whole scene out again on
// every frame of the turn - projector stands made a fifth of the frames late.
void RendererCapabilityTest::turningPanelKeepsItsLayout()
{
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_RHI") || !ARCHDOCK_SCENE3D_BUILT)
        return; // The 3D scene turns only in the real RHI scene.
    if (!qEnvironmentVariableIsSet("ARCHDOCK_TEST_DECORATIONS"))
        QSKIP("Runs in its own private session (ARCHDOCK_RENDERING_DECORATIONS).");
    const QString glyph = QFINDTESTDATA("data/truth-glyph-terminal.svg");
    QVERIFY(!glyph.isEmpty());
    QVariantMap theme{
        {QStringLiteral("format"), QStringLiteral("org.archdock.theme")},
        {QStringLiteral("version"), 2}, {QStringLiteral("valid"), true},
        {QStringLiteral("id"), QStringLiteral("procedural-look")},
        {QStringLiteral("name"), QStringLiteral("Procedural look")},
        {QStringLiteral("capabilities"), QVariantMap{
            {QStringLiteral("rendererTiers"), QVariantList{QStringLiteral("true3d"), QStringLiteral("procedural2d")}},
            {QStringLiteral("preferredRendererTier"), QStringLiteral("procedural2d")},
            {QStringLiteral("fallbackRendererTiers"), QVariantList{QStringLiteral("procedural2d")}}}},
        {QStringLiteral("scene3D"), QVariantMap{
            {QStringLiteral("generic"), true}, {QStringLiteral("generated"), QVariantMap{}},
            {QStringLiteral("fieldOfView"), 40}, {QStringLiteral("cameraPitch"), 25},
            {QStringLiteral("cameraYaw"), 0}, {QStringLiteral("defaultQuality"), QStringLiteral("high")},
            {QStringLiteral("transitions"), false}, {QStringLiteral("parts"), QVariantList{}}}},
        {QStringLiteral("scene3DResources"), QVariantMap{
            {QStringLiteral("material"), QVariantMap{
                {QStringLiteral("format"), QStringLiteral("org.archdock.material")},
                {QStringLiteral("version"), 1}, {QStringLiteral("baseColor"), QStringLiteral("#406080")},
                {QStringLiteral("emissiveColor"), QStringLiteral("#000000")},
                {QStringLiteral("emissiveStrength"), 0.0}, {QStringLiteral("metalness"), 0.3},
                {QStringLiteral("roughness"), 0.45}}},
            {QStringLiteral("parts"), QVariantList{}}, {QStringLiteral("indexBudget"), 262144}}}};
    QQmlEngine engine;
    engine.addImportPath(importRoot());
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import ArchDock.Rendering 1.0
        PanelScene {
            required property string glyph
            panelDefinition: ({rendererTier: "true3d", layout: "circular", layoutRadius: 210, iconSize: 52,
                               layoutPadding: 10, appearance: "glass", scene3DQuality: "high",
                               scene3DCameraPitch: 25, scene3DTransitions: false,
                               pedestalShape: "projector", panelMotionTarget: "panel", motionTarget: "panel"})
            entryDelegateContext: ({hostKind: "free"})
            hostCapabilities: ({rotation: {available: true}})
            orderedEntries: [0, 1, 2, 3, 4, 5].map(function(index) {
                return {id: "entry-" + index, displayName: "Entry " + index, iconName: glyph}
            })
        }
    )", QUrl::fromLocalFile(importRoot() + QStringLiteral("/TurningPanelConsumer.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.setColor(QColor(QStringLiteral("#1b2333")));
    std::unique_ptr<QObject> object(component.createWithInitialProperties({
        {QStringLiteral("themeDefinition"), theme},
        {QStringLiteral("glyph"), QUrl::fromLocalFile(glyph).toString()}}));
    QVERIFY2(object != nullptr, qPrintable(component.errorString()));
    auto *scene = qobject_cast<QQuickItem *>(object.get());
    QVERIFY(scene);
    scene->setParentItem(window.contentItem());
    window.resize(qCeil(scene->width()), qCeil(scene->height()));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QTRY_COMPARE_WITH_TIMEOUT(scene->property("effectiveRendererTier").toString(), QStringLiteral("true3d"), 5000);
    auto *renderer = objectValue(scene->property("activeSurfaceRenderer"));
    QVERIFY(renderer);
    QTRY_COMPARE(plainValue(renderer->property("projectedEntryGeometry")).toList().size(), 6);
    QVERIFY(scene->property("wheelRotationAvailable").toBool());
    QTest::qWait(500);
    const double angle = scene->property("effectiveLayoutAngle").toDouble();
    QSignalSpy relaid(renderer, SIGNAL(platformScaleChanged()));
    // A wheel spun for a second and a half, a notch every 60 ms: a whole
    // turn, 15 degrees a notch.
    double midway = angle;
    for (int notch = 0; notch < 24; ++notch)
    {
        QVERIFY(QMetaObject::invokeMethod(scene, "stepMotion", Q_ARG(QVariant, 1)));
        QTest::qWait(60);
        if (notch == 8)
            midway = scene->property("effectiveLayoutAngle").toDouble();
    }
    QTest::qWait(200);
    QVERIFY2(std::abs(std::remainder(midway - angle, 360.0)) > 60, "the panel turned");
    QCOMPARE(relaid.count(), 0);
}

QTEST_MAIN(RendererCapabilityTest)
#include "RendererCapabilityTest.moc"
