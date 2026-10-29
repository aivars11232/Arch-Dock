#include "IconObject.h"

#include "marchingcubes.h"
#include "meshoptimizer.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace ArchDock
{
namespace
{

// The file: this header, then the points, then the indexes.
struct Header
{
    char magic[8];
    quint32 vertexCount;
    quint32 indexCount;
    float minimum[3];
    float maximum[3];
    float depth;
    float colour[4];
};
constexpr char fileMagic[8] = {'A', 'D', 'I', 'C', 'O', 'N', '1', '\0'};

} // namespace

QString iconObjectModels()
{
    const auto holds = [](const QString &folder) {
        return QFileInfo::exists(folder + QStringLiteral("/triposr-network.onnx"))
               && QFileInfo::exists(folder + QStringLiteral("/triposr-field.onnx"));
    };
    const QString chosen = qEnvironmentVariable("ARCHDOCK_ICON_OBJECT_MODELS");
    if (!chosen.isEmpty()) return holds(chosen) ? chosen : QString();
    for (const QString &data : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
    {
        const QString folder = data + QStringLiteral("/arch-dock/models");
        if (holds(folder)) return folder;
    }
    return {};
}

bool saveIconObject(const IconObjectMesh &mesh, const QString &path)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    Header header{};
    std::memcpy(header.magic, fileMagic, sizeof fileMagic);
    header.vertexCount = quint32(mesh.vertices.size() / IconObjectMesh::stride);
    header.indexCount = quint32(mesh.indices.size());
    for (int k = 0; k < 3; ++k)
    {
        header.minimum[k] = mesh.minimum[k];
        header.maximum[k] = mesh.maximum[k];
    }
    header.depth = mesh.depth;
    std::copy(std::begin(mesh.colour), std::end(mesh.colour), header.colour);
    file.write(reinterpret_cast<const char *>(&header), sizeof header);
    file.write(reinterpret_cast<const char *>(mesh.vertices.data()), qint64(mesh.vertices.size() * sizeof(float)));
    file.write(reinterpret_cast<const char *>(mesh.indices.data()), qint64(mesh.indices.size() * sizeof(quint32)));
    return file.commit();
}

std::optional<IconObjectMesh> loadIconObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    Header header{};
    if (file.read(reinterpret_cast<char *>(&header), sizeof header) != qint64(sizeof header)
        || std::memcmp(header.magic, fileMagic, sizeof fileMagic) != 0)
        return std::nullopt;
    const qint64 floats = qint64(header.vertexCount) * IconObjectMesh::stride;
    if (file.size() != qint64(sizeof header) + floats * qint64(sizeof(float))
                           + qint64(header.indexCount) * qint64(sizeof(quint32)))
        return std::nullopt;
    IconObjectMesh mesh;
    mesh.vertices.resize(std::size_t(floats));
    mesh.indices.resize(header.indexCount);
    file.read(reinterpret_cast<char *>(mesh.vertices.data()), floats * qint64(sizeof(float)));
    file.read(reinterpret_cast<char *>(mesh.indices.data()), qint64(header.indexCount) * qint64(sizeof(quint32)));
    for (const quint32 index : mesh.indices)
        if (index >= header.vertexCount) return std::nullopt;
    mesh.minimum = QVector3D(header.minimum[0], header.minimum[1], header.minimum[2]);
    mesh.maximum = QVector3D(header.maximum[0], header.maximum[1], header.maximum[2]);
    mesh.depth = header.depth;
    std::copy(std::begin(header.colour), std::end(header.colour), mesh.colour);
    return mesh;
}

std::vector<float> trackedField(int side, int stride, float level, const AskField &ask, std::size_t *asked)
{
    const int n = side, s = stride;
    const std::size_t total = std::size_t(n) * n * n;
    std::vector<float> values(total, std::numeric_limits<float>::quiet_NaN());
    const auto at = [n](int i, int j, int k) { return (std::size_t(i) * n + std::size_t(j)) * n + std::size_t(k); };
    std::size_t count = 0;
    std::vector<int> points;
    std::vector<float> answers;
    const auto askAll = [&]() {
        if (points.empty()) return;
        answers.resize(points.size() / 3);
        ask(points.data(), answers.size(), answers.data());
        for (std::size_t p = 0; p < answers.size(); ++p)
            values[at(points[3 * p], points[3 * p + 1], points[3 * p + 2])] = answers[p];
        count += answers.size();
        points.clear();
    };

    // Every `stride`th point first.
    const int m = (n - 1) / s;
    for (int i = 0; i <= m; ++i)
        for (int j = 0; j <= m; ++j)
            for (int k = 0; k <= m; ++k) points.insert(points.end(), {i * s, j * s, k * s});
    askAll();
    const auto inside = [&](int i, int j, int k) { return values[at(i, j, k)] > level; };
    const auto cell = [m](int a, int b, int c) { return (std::size_t(a) * m + std::size_t(b)) * m + std::size_t(c); };

    // The cells where inside and outside meet at their corners.
    std::vector<unsigned char> done(std::size_t(m) * m * m, 0);
    std::vector<std::array<int, 3>> todo;
    for (int a = 0; a < m; ++a)
        for (int b = 0; b < m; ++b)
            for (int c = 0; c < m; ++c)
            {
                bool any = false, all = true;
                for (int corner = 0; corner < 8; ++corner)
                {
                    const bool in = inside((a + (corner >> 2 & 1)) * s, (b + (corner >> 1 & 1)) * s, (c + (corner & 1)) * s);
                    any = any || in;
                    all = all && in;
                }
                if (any && !all)
                {
                    todo.push_back({a, b, c});
                    done[cell(a, b, c)] = 1;
                }
            }

    std::vector<unsigned char> queued(total, 0);
    while (!todo.empty())
    {
        // Every point of these cells.
        for (const auto &t : todo)
            for (int di = 0; di <= s; ++di)
                for (int dj = 0; dj <= s; ++dj)
                    for (int dk = 0; dk <= s; ++dk)
                    {
                        const int i = t[0] * s + di, j = t[1] * s + dj, k = t[2] * s + dk;
                        const std::size_t index = at(i, j, k);
                        if (!std::isnan(values[index]) || queued[index]) continue;
                        queued[index] = 1;
                        points.insert(points.end(), {i, j, k});
                    }
        askAll();
        // Where the surface crosses a face of one, the cell beyond that face
        // holds surface too.
        std::vector<std::array<int, 3>> grown;
        for (const auto &t : todo)
            for (int axis = 0; axis < 3; ++axis)
                for (int end = 0; end < 2; ++end)
                {
                    std::array<int, 3> beyond = t;
                    beyond[axis] += end ? 1 : -1;
                    if (beyond[axis] < 0 || beyond[axis] >= m || done[cell(beyond[0], beyond[1], beyond[2])]) continue;
                    const int u = axis == 0 ? 1 : 0, v = axis == 2 ? 1 : 2;
                    bool any = false, all = true;
                    std::array<int, 3> p{};
                    p[axis] = (t[axis] + end) * s;
                    for (int du = 0; du <= s && (all || !any); ++du)
                        for (int dv = 0; dv <= s; ++dv)
                        {
                            p[u] = t[u] * s + du;
                            p[v] = t[v] * s + dv;
                            const bool in = inside(p[0], p[1], p[2]);
                            any = any || in;
                            all = all && in;
                        }
                    if (any && !all)
                    {
                        done[cell(beyond[0], beyond[1], beyond[2])] = 1;
                        grown.push_back(beyond);
                    }
                }
        todo.swap(grown);
    }

    // The rest from the nearest point asked first.
    const std::size_t sideCount = std::size_t(n);
    std::vector<int> nearest(sideCount);
    for (int i = 0; i < n; ++i) nearest[std::size_t(i)] = int(std::lround(double(i) / s)) * s;
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            for (int k = 0; k < n; ++k)
            {
                float &value = values[at(i, j, k)];
                if (std::isnan(value))
                    value = values[at(nearest[std::size_t(i)], nearest[std::size_t(j)], nearest[std::size_t(k)])];
            }
    if (asked) *asked = count;
    return values;
}

void marchingCubes(const std::vector<float> &values, int side, float level, std::vector<float> &points,
                   std::vector<quint32> &triangles)
{
    const long n = side;
    const std::array<long, 3> lower{0, 0, 0};
    const std::array<long, 3> upper{n - 1, n - 1, n - 1};
    std::vector<double> vertices;
    std::vector<std::size_t> polygons;
    mc::marching_cubes(
        lower, upper, int(n), int(n), int(n),
        [&](long x, long y, long z) -> double { return double(values[std::size_t((x * n + y) * n + z)] - level); },
        0.0, vertices, polygons);
    // Points at the very same place (where the surface passes through a
    // grid point itself) made one, as trimesh makes TripoSR's own meshes,
    // and the triangles that leaves without area dropped.
    struct Place
    {
        quint32 bits[3];
        bool operator==(const Place &other) const
        {
            return bits[0] == other.bits[0] && bits[1] == other.bits[1] && bits[2] == other.bits[2];
        }
    };
    struct Hash
    {
        std::size_t operator()(const Place &place) const
        {
            return (std::size_t(place.bits[0]) * 73856093u) ^ (std::size_t(place.bits[1]) * 19349663u)
                   ^ (std::size_t(place.bits[2]) * 83492791u);
        }
    };
    std::unordered_map<Place, quint32, Hash> at;
    std::vector<quint32> renumbered(vertices.size() / 3);
    points.clear();
    for (std::size_t v = 0; v < vertices.size(); v += 3)
    {
        const float p[3] = {float(vertices[v]), float(vertices[v + 1]), float(vertices[v + 2])};
        Place place;
        std::memcpy(place.bits, p, sizeof p);
        const auto found = at.emplace(place, quint32(points.size() / 3));
        if (found.second) points.insert(points.end(), p, p + 3);
        renumbered[v / 3] = found.first->second;
    }
    triangles.clear();
    for (std::size_t t = 0; t + 2 < polygons.size(); t += 3)
    {
        const quint32 a = renumbered[polygons[t]], b = renumbered[polygons[t + 1]], c = renumbered[polygons[t + 2]];
        if (a != b && b != c && a != c) triangles.insert(triangles.end(), {a, b, c});
    }
}

namespace
{

// The area of a closed mesh's cross-section at x = `height`: each triangle
// the plane cuts gives one edge of the section, turned by the triangle's
// own outward normal, and the shoelace formula sums them.
double sliceArea(const ColouredMesh &mesh, double height)
{
    double area = 0;
    for (std::size_t t = 0; t + 2 < mesh.triangles.size(); t += 3)
    {
        double corner[3][3], side[3];
        double lowest = 1e300, highest = -1e300;
        for (int c = 0; c < 3; ++c)
        {
            const float *p = &mesh.points[3 * std::size_t(mesh.triangles[t + std::size_t(c)])];
            for (int k = 0; k < 3; ++k) corner[c][k] = p[k];
            side[c] = corner[c][0] - height;
            lowest = std::min(lowest, side[c]);
            highest = std::max(highest, side[c]);
        }
        if (!(lowest < 0 && highest >= 0)) continue;
        double ends[2][3];
        int found = 0;
        for (int e = 0; e < 3 && found < 2; ++e)
        {
            const int a = e, b = (e + 1) % 3;
            if ((side[a] < 0) == (side[b] < 0)) continue;
            const double share = side[a] / (side[a] - side[b]);
            for (int k = 0; k < 3; ++k) ends[found][k] = corner[a][k] + (corner[b][k] - corner[a][k]) * share;
            ++found;
        }
        if (found < 2) continue;
        double u[3], w[3], normal[3];
        for (int k = 0; k < 3; ++k)
        {
            u[k] = corner[1][k] - corner[0][k];
            w[k] = corner[2][k] - corner[0][k];
        }
        normal[0] = u[1] * w[2] - u[2] * w[1];
        normal[1] = u[2] * w[0] - u[0] * w[2];
        normal[2] = u[0] * w[1] - u[1] * w[0];
        const double dy = ends[1][1] - ends[0][1], dz = ends[1][2] - ends[0][2];
        // The inside lies left of each edge walked anticlockwise in (y, z).
        const bool flip = -dz * normal[1] + dy * normal[2] > 0;
        const double *p = flip ? ends[1] : ends[0];
        const double *q = flip ? ends[0] : ends[1];
        area += 0.5 * (p[1] * q[2] - q[1] * p[2]);
    }
    return area;
}

ColouredMesh compacted(const std::vector<float> &points, const std::vector<float> &colours,
                       const std::vector<quint32> &triangles)
{
    ColouredMesh mesh;
    std::vector<quint32> renumbered(points.size() / 3, std::numeric_limits<quint32>::max());
    mesh.triangles.reserve(triangles.size());
    for (const quint32 index : triangles)
    {
        if (renumbered[index] == std::numeric_limits<quint32>::max())
        {
            renumbered[index] = quint32(mesh.points.size() / 3);
            mesh.points.insert(mesh.points.end(), &points[3 * std::size_t(index)], &points[3 * std::size_t(index)] + 3);
            mesh.colours.insert(mesh.colours.end(), &colours[3 * std::size_t(index)],
                                &colours[3 * std::size_t(index)] + 3);
        }
        mesh.triangles.push_back(renumbered[index]);
    }
    return mesh;
}

} // namespace

float widestSlice(const ColouredMesh &mesh)
{
    if (mesh.points.empty()) return 0;
    double lowest = 1e300, highest = -1e300;
    for (std::size_t p = 0; p < mesh.points.size(); p += 3)
    {
        lowest = std::min(lowest, double(mesh.points[p]));
        highest = std::max(highest, double(mesh.points[p]));
    }
    // Two hundred slices between the ends.
    constexpr int slices = 200;
    std::vector<double> heights(slices), areas(slices);
    double widest = 0;
    for (int i = 0; i < slices; ++i)
    {
        heights[std::size_t(i)] = lowest + (highest - lowest) * double(i + 1) / double(slices + 1);
        areas[std::size_t(i)] = sliceArea(mesh, heights[std::size_t(i)]);
        widest = std::max(widest, areas[std::size_t(i)]);
    }
    std::vector<double> near;
    for (int i = 0; i < slices; ++i)
        if (areas[std::size_t(i)] >= 0.97 * widest) near.push_back(heights[std::size_t(i)]);
    if (near.empty()) return float((lowest + highest) / 2);
    const std::size_t half = near.size() / 2;
    return float(near.size() % 2 ? near[half] : (near[half - 1] + near[half]) / 2);
}

ColouredMesh frontOf(const ColouredMesh &mesh, float cut)
{
    std::vector<float> points = mesh.points, colours = mesh.colours;
    const std::size_t count = mesh.points.size() / 3;
    std::vector<unsigned char> front(count);
    for (std::size_t p = 0; p < count; ++p) front[p] = mesh.points[3 * p] >= cut;
    std::unordered_map<quint64, quint32> made;
    // The point where the edge from `a` to `b` meets the cut, made once for
    // both triangles beside the edge.
    const auto onCut = [&](quint32 a, quint32 b) {
        const quint64 key = quint64(std::min(a, b)) << 32 | std::max(a, b);
        const auto found = made.find(key);
        if (found != made.end()) return found->second;
        const float share = (cut - points[3 * std::size_t(a)]) / (points[3 * std::size_t(b)] - points[3 * std::size_t(a)]);
        const quint32 index = quint32(points.size() / 3);
        for (int k = 0; k < 3; ++k)
        {
            const float pa = points[3 * std::size_t(a) + std::size_t(k)], pb = points[3 * std::size_t(b) + std::size_t(k)];
            const float ca = colours[3 * std::size_t(a) + std::size_t(k)], cb = colours[3 * std::size_t(b) + std::size_t(k)];
            points.push_back(k == 0 ? cut : pa + (pb - pa) * share);
            colours.push_back(ca + (cb - ca) * share);
        }
        made.emplace(key, index);
        return index;
    };
    std::vector<quint32> triangles;
    triangles.reserve(mesh.triangles.size());
    for (std::size_t t = 0; t + 2 < mesh.triangles.size(); t += 3)
    {
        quint32 corner[3] = {mesh.triangles[t], mesh.triangles[t + 1], mesh.triangles[t + 2]};
        const int kept = front[corner[0]] + front[corner[1]] + front[corner[2]];
        if (kept == 3)
        {
            triangles.insert(triangles.end(), corner, corner + 3);
            continue;
        }
        if (kept == 0) continue;
        // Turned, its order kept: the kept corner first when one is kept,
        // the cut-away corner last when two are.
        int first = 0;
        for (int c = 0; c < 3; ++c)
            if (kept == 1 ? bool(front[corner[c]]) : !front[corner[c]]) first = kept == 1 ? c : (c + 1) % 3;
        const quint32 a = corner[first], b = corner[(first + 1) % 3], c = corner[(first + 2) % 3];
        if (kept == 1)
        {
            if (points[3 * std::size_t(a)] == cut) continue;
            triangles.insert(triangles.end(), {a, onCut(a, b), onCut(a, c)});
        }
        else
        {
            const quint32 bc = onCut(b, c), ca = onCut(c, a);
            triangles.insert(triangles.end(), {a, b, bc, a, bc, ca});
        }
    }
    return compacted(points, colours, triangles);
}

ColouredMesh simplified(const ColouredMesh &mesh, std::size_t triangles, float error)
{
    if (mesh.triangles.size() / 3 <= triangles) return mesh;
    std::vector<unsigned int> kept(mesh.triangles.size());
    const float weights[3] = {1, 1, 1};
    const std::size_t count = meshopt_simplifyWithAttributes(
        kept.data(), mesh.triangles.data(), mesh.triangles.size(), mesh.points.data(), mesh.points.size() / 3,
        3 * sizeof(float), mesh.colours.data(), 3 * sizeof(float), weights, 3, nullptr, triangles * 3, error,
        meshopt_SimplifyLockBorder, nullptr);
    kept.resize(count);
    return compacted(mesh.points, mesh.colours, std::vector<quint32>(kept.begin(), kept.end()));
}

ColouredMesh twoFronts(const ColouredMesh &whole, float cut)
{
    // A triangle lying in the cut itself (made lighter, the front may join
    // points of its cut edge so) would be a sheet across the middle, seen
    // through a hologram: without them the front's open edge still lies
    // all in the cut, so its mirror still closes it.
    std::vector<quint32> standing;
    standing.reserve(whole.triangles.size());
    for (std::size_t t = 0; t + 2 < whole.triangles.size(); t += 3)
    {
        const bool flat = whole.points[3 * std::size_t(whole.triangles[t])] == cut
                          && whole.points[3 * std::size_t(whole.triangles[t + 1])] == cut
                          && whole.points[3 * std::size_t(whole.triangles[t + 2])] == cut;
        if (!flat) standing.insert(standing.end(), &whole.triangles[t], &whole.triangles[t] + 3);
    }
    const ColouredMesh front = compacted(whole.points, whole.colours, standing);
    ColouredMesh both = front;
    const std::size_t count = front.points.size() / 3;
    std::vector<quint32> mirror(count);
    for (std::size_t p = 0; p < count; ++p)
    {
        if (front.points[3 * p] == cut)
        {
            mirror[p] = quint32(p);
            continue;
        }
        mirror[p] = quint32(both.points.size() / 3);
        both.points.insert(both.points.end(), {2 * cut - front.points[3 * p], front.points[3 * p + 1], front.points[3 * p + 2]});
        both.colours.insert(both.colours.end(), &front.colours[3 * p], &front.colours[3 * p] + 3);
    }
    for (std::size_t t = 0; t + 2 < front.triangles.size(); t += 3)
        both.triangles.insert(both.triangles.end(),
                              {mirror[front.triangles[t]], mirror[front.triangles[t + 2]], mirror[front.triangles[t + 1]]});
    return both;
}

} // namespace ArchDock
