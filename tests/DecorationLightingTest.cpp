// The decoration lighting by itself: what the pictures of a scene hold.
//
// Set ARCHDOCK_DECORATION_BAKE=<scene.json>:<picture.png> to light a scene
// file and save it over the design pictures' background, for comparing with
// them by eye.
#include "DecorationLighting.h"

#include <QColor>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QtTest>

#include <cmath>

using namespace ArchDock;

namespace
{

QString materials() { return QStringLiteral(ARCHDOCK_DECORATION_MATERIALS); }

QJsonArray ring(const QList<QPointF> &points)
{
    QJsonArray result;
    for (const QPointF &p : points) result.append(QJsonArray{p.x(), p.y()});
    return result;
}

QJsonObject rectangle(qreal x, qreal y, qreal w, qreal h)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("fill")},
                       {QStringLiteral("rings"), QJsonArray{ring({{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}})}}};
}

QJsonObject straightPath(qreal x0, qreal x1, qreal y)
{
    QJsonArray points;
    for (int k = 0; k <= 20; ++k) points.append(QJsonArray{x0 + (x1 - x0) * k / 20.0, y});
    return QJsonObject{{QStringLiteral("points"), points}, {QStringLiteral("closed"), false}};
}

QJsonObject sceneOf(const QJsonArray &elements, QRect bounds, qreal hw = 24, QJsonObject path = {})
{
    return QJsonObject{{QStringLiteral("hw"), hw},
                       {QStringLiteral("elements"), elements},
                       {QStringLiteral("bounds"), QJsonObject{{QStringLiteral("x"), bounds.x()}, {QStringLiteral("y"), bounds.y()},
                                                              {QStringLiteral("width"), bounds.width()},
                                                              {QStringLiteral("height"), bounds.height()}}},
                       {QStringLiteral("path"), path.isEmpty() ? straightPath(bounds.left() + 20, bounds.right() - 20,
                                                                              bounds.center().y()) : path}};
}

// A layer's pixel at a point of the scene; a layer's picture covers only
// its own part of the whole.
QColor pixel(const DecorationBake &bake, const QString &layer, QPointF scenePoint)
{
    const QImage image = bake.layers.value(layer);
    const QPoint at = QPoint(int((scenePoint.x() - bake.bounds.x()) * bake.scale), int((scenePoint.y() - bake.bounds.y()) * bake.scale))
                      - bake.rects.value(layer).topLeft();
    return image.isNull() || !image.rect().contains(at) ? QColor(0, 0, 0, 0) : QColor::fromRgba(image.pixel(at));
}

QJsonObject glowingDot(QPointF centre, const QString &group, qreal lit = 1)
{
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("dot")}, {QStringLiteral("role"), QStringLiteral("line")},
                       {QStringLiteral("centre"), QJsonArray{centre.x(), centre.y()}}, {QStringLiteral("radius"), 4},
                       {QStringLiteral("group"), group}, {QStringLiteral("emissive"), 0.9}, {QStringLiteral("lit"), lit},
                       {QStringLiteral("glow"), QStringLiteral("#ffffff")},
                       {QStringLiteral("albedo"), QJsonObject{{QStringLiteral("colour"), QStringLiteral("#ffffff")}}}};
}

} // namespace

class DecorationLightingTest : public QObject
{
    Q_OBJECT

private slots:
    void bakeFileOnRequest()
    {
        const QByteArray request = qgetenv("ARCHDOCK_DECORATION_BAKE");
        if (request.isEmpty()) QSKIP("no scene file asked for");
        const QList<QByteArray> parts = request.split(':');
        QFile file(QString::fromLocal8Bit(parts.value(0)));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject scene = QJsonDocument::fromJson(file.readAll()).object();
        QElapsedTimer timer;
        timer.start();
        const DecorationBake bake = bakeDecoration(scene, materials(), 1);
        qInfo("lit in %lld ms, layers %s", timer.elapsed(), qPrintable(QStringList(bake.layers.keys()).join(u',')));
        QVERIFY(bake.error.isEmpty());
        // The design pictures' background, the base and every layer over it.
        QImage out(bake.layers.value(QStringLiteral("base")).size(), QImage::Format_ARGB32_Premultiplied);
        QPainter painter(&out);
        QLinearGradient background(0, 0, 0, out.height());
        background.setColorAt(0, QColor(0x26, 0x32, 0x4a));
        background.setColorAt(1, QColor(0x12, 0x18, 0x22));
        painter.fillRect(out.rect(), background);
        painter.drawImage(0, 0, bake.layers.value(QStringLiteral("base")));
        for (auto it = bake.layers.cbegin(); it != bake.layers.cend(); ++it)
            if (it.key() != QLatin1String("base")) painter.drawImage(bake.rects.value(it.key()).topLeft(), it.value());
        painter.end();
        QVERIFY(out.save(QString::fromLocal8Bit(parts.value(1))));
    }

    void emptySceneSaysSo()
    {
        const DecorationBake bake = bakeDecoration(QJsonObject{}, materials(), 1);
        QCOMPARE(bake.error, QStringLiteral("empty-scene"));
        QVERIFY(bake.layers.isEmpty());
    }

    void bodyCoversItsShapeAndCastsAShadow()
    {
        QJsonObject body = rectangle(60, 40, 200, 40);
        body.insert(QStringLiteral("role"), QStringLiteral("body"));
        body.insert(QStringLiteral("albedo"), QJsonObject{{QStringLiteral("colour"), QStringLiteral("#c03030")}});
        body.insert(QStringLiteral("profile"), QStringLiteral("bevel"));
        const DecorationBake bake = bakeDecoration(sceneOf({body}, QRect(0, 0, 320, 120)), materials(), 1);
        QVERIFY(bake.error.isEmpty());
        QVERIFY(bake.layers.contains(QStringLiteral("base")));
        const QColor inside = pixel(bake, QStringLiteral("base"), {160, 60});
        QCOMPARE(inside.alpha(), 255);
        QVERIFY2(inside.red() > inside.green() * 2, "the body keeps its red");
        // Far from it: nothing; just below and right of it: its shadow.
        QCOMPARE(pixel(bake, QStringLiteral("base"), {10, 10}).alpha(), 0);
        QVERIFY(pixel(bake, QStringLiteral("base"), {200, 84}).alpha() > 20);
        // Its top edge, bevelled toward the light, is lit more than its
        // bottom edge.
        const QColor top = pixel(bake, QStringLiteral("base"), {160, 41.5});
        const QColor bottom = pixel(bake, QStringLiteral("base"), {160, 78.5});
        QVERIFY(top.lightness() > bottom.lightness());
    }

    void lensGivesItsOwnLightAndAHalo()
    {
        QJsonObject lens{{QStringLiteral("type"), QStringLiteral("fill")}, {QStringLiteral("role"), QStringLiteral("part")},
                         {QStringLiteral("kind"), QStringLiteral("lens")}, {QStringLiteral("group"), QStringLiteral("lights")},
                         {QStringLiteral("profile"), QStringLiteral("dome")}, {QStringLiteral("emissive"), 0.95},
                         {QStringLiteral("glow"), QStringLiteral("#ff3b1f")},
                         {QStringLiteral("albedo"), QJsonObject{{QStringLiteral("colour"), QStringLiteral("#ff3b1f")}}},
                         {QStringLiteral("lens"), QJsonObject{{QStringLiteral("colour"), QStringLiteral("#ff3b1f")},
                                                               {QStringLiteral("centre"), QJsonArray{160, 60}},
                                                               {QStringLiteral("radius"), 20}}}};
        QJsonArray circle;
        QList<QPointF> points;
        for (int k = 0; k < 48; ++k)
            points.append(QPointF(160 + 20 * std::cos(k * 2 * M_PI / 48), 60 + 20 * std::sin(k * 2 * M_PI / 48)));
        lens.insert(QStringLiteral("rings"), QJsonArray{ring(points)});
        const DecorationBake bake = bakeDecoration(sceneOf({lens}, QRect(0, 0, 320, 120)), materials(), 1);
        QVERIFY(bake.layers.contains(QStringLiteral("lights")));
        const QColor core = pixel(bake, QStringLiteral("lights"), {156, 57});
        QVERIFY2(core.red() > 200, "the lens is lit by its own light");
        // The halo reaches past the glass, over nothing in the base.
        const QColor halo = pixel(bake, QStringLiteral("lights"), {160, 86});
        QVERIFY(halo.alpha() > 0);
        QVERIFY(halo.red() >= halo.blue());
        QVERIFY(pixel(bake, QStringLiteral("base"), {160, 104}).alpha() < 60);
    }

    void groupsHaveTheirOwnPictures()
    {
        QJsonArray elements;
        int index = 0;
        for (const QString group : {QStringLiteral("stars"), QStringLiteral("blinkA"), QStringLiteral("blinkB")})
            elements.append(glowingDot(QPointF(60 + 80 * index++, 60), group));
        const DecorationBake bake = bakeDecoration(sceneOf(elements, QRect(0, 0, 320, 120)), materials(), 1);
        for (const QString group : {QStringLiteral("stars"), QStringLiteral("blinkA"), QStringLiteral("blinkB")})
            QVERIFY2(bake.layers.contains(group), qPrintable(group));
        // Each light is in its own group's picture only.
        QVERIFY(pixel(bake, QStringLiteral("stars"), {60, 60}).alpha() > 100);
        QCOMPARE(pixel(bake, QStringLiteral("stars"), {140, 60}).alpha(), 0);
        QVERIFY(pixel(bake, QStringLiteral("blinkA"), {140, 60}).alpha() > 100);
        // A group's picture covers only as far as its light reaches.
        const QRect whole = bake.rects.value(QStringLiteral("base"));
        const QRect stars = bake.rects.value(QStringLiteral("stars"));
        QVERIFY(whole.contains(stars));
        QVERIFY(stars.width() < whole.width());
        QCOMPARE(bake.layers.value(QStringLiteral("stars")).size(), stars.size());
    }

    void chasedLightsGiveLessLight()
    {
        const DecorationBake bake = bakeDecoration(
            sceneOf({glowingDot({80, 60}, QStringLiteral("lights")), glowingDot({240, 60}, QStringLiteral("lights"), 0.3)},
                    QRect(0, 0, 320, 120)),
            materials(), 1);
        const QColor full = pixel(bake, QStringLiteral("lights"), {80, 70});
        const QColor dimmed = pixel(bake, QStringLiteral("lights"), {240, 70});
        QVERIFY(full.alpha() > 0);
        QVERIFY2(dimmed.alpha() < full.alpha() * 0.6, "a light chased away gives a third of its halo");
    }

    void aBakeNoLongerWantedStops()
    {
        int asked = 0;
        const DecorationBake bake = bakeDecoration(sceneOf({rectangle(60, 40, 200, 40)}, QRect(0, 0, 320, 120)), materials(), 1,
                                                   3'000'000, [&asked]() { return ++asked > 1; });
        QCOMPARE(bake.error, QStringLiteral("cancelled"));
        QVERIFY(bake.layers.isEmpty());
    }

    void pictureFollowsARingWithoutHoles()
    {
        // A ring band of a nebula picture laid along a closed path.
        QJsonArray outer, inner, path;
        QList<QPointF> outerPoints, innerPoints;
        for (int k = 0; k < 72; ++k)
        {
            const double a = k * 2 * M_PI / 72;
            outerPoints.append(QPointF(150 + 110 * std::cos(a), 150 + 110 * std::sin(a)));
            innerPoints.prepend(QPointF(150 + 70 * std::cos(a), 150 + 70 * std::sin(a)));
            path.append(QJsonArray{150 + 90 * std::cos(a), 150 + 90 * std::sin(a)});
        }
        QJsonObject body{{QStringLiteral("type"), QStringLiteral("fill")}, {QStringLiteral("role"), QStringLiteral("body")},
                         {QStringLiteral("evenodd"), true}, {QStringLiteral("rings"), QJsonArray{ring(outerPoints), ring(innerPoints)}},
                         {QStringLiteral("albedo"), QJsonObject{{QStringLiteral("texture"), QStringLiteral("nebula-carina")},
                                                                 {QStringLiteral("strip"), true}, {QStringLiteral("relief"), 0.1}}},
                         {QStringLiteral("profile"), QStringLiteral("bevel")}, {QStringLiteral("emissive"), 0.6}};
        const DecorationBake bake = bakeDecoration(
            sceneOf({body}, QRect(0, 0, 300, 300), 20, QJsonObject{{QStringLiteral("points"), path}, {QStringLiteral("closed"), true}}),
            materials(), 1);
        QVERIFY(bake.error.isEmpty());
        int dark = 0, samples = 0;
        for (int k = 0; k < 360; k += 3)
        {
            const double a = k * M_PI / 180;
            const QColor c = pixel(bake, QStringLiteral("base"), {150 + 90 * std::cos(a), 150 + 90 * std::sin(a)});
            QCOMPARE(c.alpha(), 255);
            ++samples;
            if (c.lightness() < 4) ++dark;
        }
        QVERIFY2(dark < samples / 10, "the picture covers the band all the way round");
        // The hole in the middle stays clear.
        QVERIFY(pixel(bake, QStringLiteral("base"), {150, 150}).alpha() < 40);
    }

    void driftMovesThePictureAlongTheBand()
    {
        // The same strip of nebula twice, the second drifted 30 pixels on:
        // what the first shows at x, the second shows 30 pixels sooner.
        const auto bandWith = [](qreal shift) {
            QJsonObject body = rectangle(20, 40, 560, 48);
            body.insert(QStringLiteral("role"), QStringLiteral("body"));
            body.insert(QStringLiteral("albedo"), QJsonObject{{QStringLiteral("texture"), QStringLiteral("nebula-carina")},
                                                               {QStringLiteral("strip"), true}, {QStringLiteral("relief"), 0},
                                                               {QStringLiteral("shift"), shift}});
            return bakeDecoration(sceneOf({body}, QRect(0, 0, 600, 128)), materials(), 1);
        };
        const DecorationBake still = bandWith(0), drifted = bandWith(30);
        double difference = 0, unrelated = 0;
        int count = 0;
        for (int x = 120; x < 480; x += 4)
        {
            const QColor a = pixel(still, QStringLiteral("base"), {double(x + 30), 64});
            const QColor b = pixel(drifted, QStringLiteral("base"), {double(x), 64});
            const QColor c = pixel(still, QStringLiteral("base"), {double(x), 64});
            difference += std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) + std::abs(a.blue() - b.blue());
            unrelated += std::abs(c.red() - b.red()) + std::abs(c.green() - b.green()) + std::abs(c.blue() - b.blue());
            ++count;
        }
        QVERIFY2(difference / count < 6, qPrintable(QString::number(difference / count)));
        QVERIFY(unrelated > difference * 2);
    }

    void recolourReachesTheChosenHue()
    {
        QJsonObject body = rectangle(40, 40, 240, 40);
        body.insert(QStringLiteral("role"), QStringLiteral("body"));
        body.insert(QStringLiteral("albedo"), QJsonObject{{QStringLiteral("texture"), QStringLiteral("leather-red")},
                                                           {QStringLiteral("relief"), 0.4}});
        body.insert(QStringLiteral("recolour"), QJsonObject{{QStringLiteral("hue"), 220}, {QStringLiteral("saturation"), 0.7},
                                                             {QStringLiteral("value"), 0.6}});
        const DecorationBake bake = bakeDecoration(sceneOf({body}, QRect(0, 0, 320, 120)), materials(), 1);
        double sumHue = 0;
        int count = 0;
        for (int x = 60; x < 260; x += 10)
        {
            const QColor c = pixel(bake, QStringLiteral("base"), {double(x), 60});
            if (c.hsvSaturation() > 40) { sumHue += c.hsvHue(); ++count; }
        }
        QVERIFY(count > 5);
        QVERIFY2(std::abs(sumHue / count - 220) < 20, "the leather is blue now");
    }

    void largeScenesAreMadeSmaller()
    {
        QJsonObject body = rectangle(10, 10, 4000, 2000);
        body.insert(QStringLiteral("albedo"), QJsonObject{{QStringLiteral("colour"), QStringLiteral("#808080")}});
        const DecorationBake bake = bakeDecoration(sceneOf({body}, QRect(0, 0, 4020, 2020)), materials(), 2, 1'000'000);
        const QImage base = bake.layers.value(QStringLiteral("base"));
        QVERIFY(qsizetype(base.width()) * base.height() <= 1'010'000);
        QVERIFY(bake.scale < 1);
    }
};

QTEST_MAIN(DecorationLightingTest)
#include "DecorationLightingTest.moc"
