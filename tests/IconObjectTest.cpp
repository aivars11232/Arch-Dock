// The icons' 3D objects by themselves (IconObject.h): the steps they are made
// by, tried on fields and meshes drawn for the test, and their files. The
// network that sees an icon's picture needs its models and is left to the
// icon helper.
#include "IconObject.h"

#include <QTemporaryDir>
#include <QtTest>

#include <cmath>
#include <map>

using namespace ArchDock;

namespace
{

// A field as dense as a TripoSR object at `level` 25: high inside balls,
// falling off with the distance from them.
struct Balls
{
    struct Ball
    {
        double x, y, z, radius;
    };
    std::vector<Ball> balls;

    float at(int i, int j, int k) const
    {
        double best = 0;
        for (const Ball &b : balls)
        {
            const double d = std::sqrt((i - b.x) * (i - b.x) + (j - b.y) * (j - b.y) + (k - b.z) * (k - b.z));
            best = std::max(best, 25.0 + 4.0 * (b.radius - d));
        }
        return float(best);
    }
};

std::vector<float> dense(const Balls &field, int side)
{
    std::vector<float> values(std::size_t(side) * side * side);
    for (int i = 0; i < side; ++i)
        for (int j = 0; j < side; ++j)
            for (int k = 0; k < side; ++k) values[(std::size_t(i) * side + std::size_t(j)) * side + std::size_t(k)] = field.at(i, j, k);
    return values;
}

AskField asking(const Balls &field)
{
    return [&field](const int *points, std::size_t count, float *values) {
        for (std::size_t p = 0; p < count; ++p) values[p] = field.at(points[3 * p], points[3 * p + 1], points[3 * p + 2]);
    };
}

// Every edge of a closed mesh lies between two of its triangles, once each
// way.
bool closed(const std::vector<quint32> &triangles)
{
    std::map<std::pair<quint32, quint32>, int> edges;
    for (std::size_t t = 0; t + 2 < triangles.size(); t += 3)
        for (int e = 0; e < 3; ++e) ++edges[{triangles[t + std::size_t(e)], triangles[t + std::size_t((e + 1) % 3)]}];
    for (const auto &[edge, count] : edges)
        if (count != 1 || !edges.count({edge.second, edge.first})) return false;
    return !edges.empty();
}

// A ball as a coloured mesh, its colour its height.
ColouredMesh ball(double radius, double x)
{
    Balls field;
    field.balls.push_back({24 + x, 24, 24, radius});
    ColouredMesh mesh;
    marchingCubes(dense(field, 49), 49, 25.0f, mesh.points, mesh.triangles);
    for (std::size_t p = 0; p < mesh.points.size(); p += 3) mesh.colours.insert(mesh.colours.end(), {mesh.points[p + 2] / 48, 0.5f, 0.5f});
    return mesh;
}

} // namespace

class IconObjectTest : public QObject
{
    Q_OBJECT

private slots:
    // Asked only near its surface, a field gives marching cubes the very
    // surface the whole grid gives, two balls apart both found, at a share
    // of the asking.
    void trackedFieldIsTheWholeGridsSurface()
    {
        Balls field;
        field.balls = {{20, 30, 30, 12}, {44, 30, 33, 7.5}};
        const int side = 61;
        std::vector<float> wholePoints, trackedPoints;
        std::vector<quint32> wholeTriangles, trackedTriangles;
        marchingCubes(dense(field, side), side, 25.0f, wholePoints, wholeTriangles);
        std::size_t asked = 0;
        const std::vector<float> tracked = trackedField(side, 5, 25.0f, asking(field), &asked);
        marchingCubes(tracked, side, 25.0f, trackedPoints, trackedTriangles);
        QVERIFY(wholeTriangles.size() > 3000);
        QCOMPARE(trackedTriangles, wholeTriangles);
        QCOMPARE(trackedPoints, wholePoints);
        QVERIFY2(asked < std::size_t(side) * side * side / 3, qPrintable(QString::number(asked)));
    }

    // Marching cubes makes a ball closed, its points on its sphere.
    void marchingCubesMakesAClosedBall()
    {
        const ColouredMesh mesh = ball(14, 0);
        QVERIFY(closed(mesh.triangles));
        for (std::size_t p = 0; p < mesh.points.size(); p += 3)
        {
            const double d = std::hypot(mesh.points[p] - 24, mesh.points[p + 1] - 24, mesh.points[p + 2] - 24);
            QVERIFY2(std::abs(d - 14) < 0.6, qPrintable(QString::number(d)));
        }
    }

    // A ball's widest slice lies through its middle.
    void widestSliceIsTheMiddle()
    {
        QVERIFY(std::abs(widestSlice(ball(14, 0)) - 24) < 0.8);
        QVERIFY(std::abs(widestSlice(ball(10, 6)) - 30) < 0.8);
    }

    // The front half of a ball, cut at its middle and mirrored behind
    // itself, is a closed ball again, the same from either side.
    void twoFrontsMakeAClosedWhole()
    {
        const ColouredMesh whole = ball(14, 0);
        const float cut = 24.3f;
        const ColouredMesh front = frontOf(whole, cut);
        for (std::size_t p = 0; p < front.points.size(); p += 3) QVERIFY(front.points[p] >= cut);
        QVERIFY(!closed(front.triangles));
        const ColouredMesh both = twoFronts(front, cut);
        QVERIFY(closed(both.triangles));
        QCOMPARE(both.triangles.size(), 2 * front.triangles.size());
        QCOMPARE(both.colours.size(), both.points.size());
        // Every point has its mirror image, in the same colour.
        std::map<std::tuple<int, int, int>, std::size_t> at;
        const auto key = [](float x, float y, float z) {
            return std::tuple<int, int, int>(int(std::lround(x * 1000)), int(std::lround(y * 1000)), int(std::lround(z * 1000)));
        };
        for (std::size_t p = 0; p < both.points.size(); p += 3) at[key(both.points[p], both.points[p + 1], both.points[p + 2])] = p;
        for (std::size_t p = 0; p < both.points.size(); p += 3)
        {
            const auto found = at.find(key(2 * cut - both.points[p], both.points[p + 1], both.points[p + 2]));
            QVERIFY(found != at.end());
            QCOMPARE(both.colours[found->second], both.colours[p]);
        }
    }

    // Made lighter, the front keeps its cut edge where it was, so its mirror
    // still closes it.
    void simplifiedKeepsTheCutEdge()
    {
        const float cut = 24.3f;
        const ColouredMesh front = frontOf(ball(18, 0), cut);
        std::size_t onCut = 0;
        for (std::size_t p = 0; p < front.points.size(); p += 3) onCut += front.points[p] == cut;
        const ColouredMesh lighter = simplified(front, front.triangles.size() / 3 / 4, 0.05f);
        QVERIFY(lighter.triangles.size() < front.triangles.size() / 2);
        std::size_t stillOnCut = 0;
        for (std::size_t p = 0; p < lighter.points.size(); p += 3) stillOnCut += lighter.points[p] == cut;
        QCOMPARE(stillOnCut, onCut);
        const ColouredMesh both = twoFronts(lighter, cut);
        QVERIFY(closed(both.triangles));
        // Nothing lies in the cut itself: no sheet across the middle.
        for (std::size_t t = 0; t + 2 < both.triangles.size(); t += 3)
            QVERIFY(!(both.points[3 * std::size_t(both.triangles[t])] == cut && both.points[3 * std::size_t(both.triangles[t + 1])] == cut
                      && both.points[3 * std::size_t(both.triangles[t + 2])] == cut));
    }

    // An object's file reads back as it was written; anything else is not
    // read as one.
    void filesReadBack()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        IconObjectMesh mesh;
        for (int v = 0; v < 3; ++v)
            for (int k = 0; k < IconObjectMesh::stride; ++k) mesh.vertices.push_back(float(v * 100 + k) / 7);
        mesh.indices = {0, 1, 2};
        mesh.minimum = QVector3D(-1, -2, -3);
        mesh.maximum = QVector3D(1, 2, 3);
        mesh.depth = 3;
        mesh.colour[0] = 0.25f;
        const QString path = folder.filePath(QStringLiteral("one.adicon"));
        QVERIFY(saveIconObject(mesh, path));
        const auto read = loadIconObject(path);
        QVERIFY(read.has_value());
        QCOMPARE(read->vertices, mesh.vertices);
        QCOMPARE(read->indices, mesh.indices);
        QCOMPARE(read->maximum, mesh.maximum);
        QCOMPARE(read->depth, 3.0f);
        QCOMPARE(read->colour[0], 0.25f);
        QFile broken(folder.filePath(QStringLiteral("broken.adicon")));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("ADICON1");
        broken.close();
        QVERIFY(!loadIconObject(broken.fileName()).has_value());
    }

    // The models are found where they are named, both of them or none.
    void modelsAreFoundWhereNamed()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        qputenv("ARCHDOCK_ICON_OBJECT_MODELS", folder.path().toLocal8Bit());
        QVERIFY(iconObjectModels().isEmpty());
        for (const char *name : {"triposr-network.onnx", "triposr-field.onnx"})
        {
            QFile file(folder.filePath(QString::fromLatin1(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QCOMPARE(iconObjectModels(), folder.path());
        qunsetenv("ARCHDOCK_ICON_OBJECT_MODELS");
    }
};

QTEST_MAIN(IconObjectTest)
#include "IconObjectTest.moc"
