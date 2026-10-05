#include "RendererBuildConfig.h"
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
        const int meshTriangles = theme.value("scene3DResources").toMap()
            .value("mesh").toMap().value("indexes").toList().size() / 3;
        const int expectedTriangles = meshTriangles * 9 + 24;
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
                if (!glyph || !QMetaObject::invokeMethod(view, "mapFrom3DScene",
                    Q_RETURN_ARG(QVector3D, center),
                    Q_ARG(QVector3D, glyph->property("scenePosition").value<QVector3D>()))) return false;
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
        QVERIFY(QMetaObject::invokeMethod(viewport, "mapFrom3DScene", Q_RETURN_ARG(QVector3D, hole),
            Q_ARG(QVector3D, QVector3D(0, 0, renderer->property("platformTop").toDouble()))));
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
            QVERIFY(QMetaObject::invokeMethod(viewport, "mapFrom3DScene",
                Q_RETURN_ARG(QVector3D, projected), Q_ARG(QVector3D, position)));
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
                    // The fixture's dark blue fill is distinct from the cyan platform.
                    glyphPixels += color.alpha() > 32 && color.redF() < 0.4
                        && color.greenF() < 0.4 && color.blueF() < 0.4;
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
            QMetaObject::invokeMethod(viewport, "mapFrom3DScene", Q_RETURN_ARG(QVector3D, point),
                Q_ARG(QVector3D, QVector3D(radius * qCos(angle), radius * qSin(angle),
                    renderer->property("platformTop").toDouble())));
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
        scene->setProperty("wheelRotationAngle", 0.0);
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
                                   spacing: trackSpacing, layoutPadding: 10})
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
            QMetaObject::invokeMethod(viewport, "mapFrom3DScene", Q_RETURN_ARG(QVector3D, point),
                Q_ARG(QVector3D, position));
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
            QMetaObject::invokeMethod(viewport, "mapFrom3DScene", Q_RETURN_ARG(QVector3D, view),
                Q_ARG(QVector3D, world));
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
        const double restingSpread = spread(resting);
        const QPointF restingSurface = surfacePoint();
        QVERIFY(accepts(restingSurface));

        // Scale shrinks the platform and its icons about their centre.
        scene->setProperty("sceneScale", 0.6);
        QTRY_VERIFY2(qAbs(spread(centres()) - restingSpread * 0.6) < restingSpread * 0.05,
                     qPrintable(QStringLiteral("spread %1 of %2").arg(spread(centres())).arg(restingSpread)));
        QVERIFY(QLineF(centroid(centres()), restingMiddle).length() < 3);
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
                panelDefinition: ({rendererTier: "true3d", layout: "ring", layoutRadius: 120,
                                   scene3DQuality: "low", iconSize: 40, spacing: 8, layoutPadding: 10,
                                   scene3DScale: 0.9, scene3DTransitions: false})
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
            QMetaObject::invokeMethod(viewport, "mapFrom3DScene", Q_RETURN_ARG(QVector3D, view),
                Q_ARG(QVector3D, handle->property("scenePosition").value<QVector3D>()));
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
