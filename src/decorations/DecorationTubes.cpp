// A decoration's round solids (a trunk, its branches) as swept tubes for a 3D
// scene: along each centre line a frame - the line's way, a side across it
// in the panel's plane and an up - carries a ring of points round it at the
// radius there, the rings joined into the tube's skin. The bark's picture is
// wrapped round it, its grain along the tube, as many whole times round and
// along a closed one as fit its size, so it meets itself without a seam; its
// heights move the skin in and out, so the bark stands out of the tube, and
// the normals are then taken from the faces round each point.
#include "DecorationTubes.h"

#include "DecorationLighting.h"

#include <QColor>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ArchDock
{
namespace
{

// Points round a tube.
constexpr int around = 24;

// A grey picture's value (0 to 1) at a texture coordinate, repeating.
float heightIn(const QImage &picture, float u, float v)
{
    if (picture.isNull()) return 0.5f;
    const int w = picture.width(), h = picture.height();
    const float x = (u - std::floor(u)) * float(w) - 0.5f, y = (v - std::floor(v)) * float(h) - 0.5f;
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    const float fx = x - float(x0), fy = y - float(y0);
    const auto at = [&](int xx, int yy) {
        xx = ((xx % w) + w) % w;
        yy = ((yy % h) + h) % h;
        return float(picture.constScanLine(yy)[xx]) / 255.0f;
    };
    return (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy)
           + (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
}

class Builder
{
public:
    quint32 vertex(const QVector3D &p, const QVector3D &n, float u, float v, float kind, bool smooth)
    {
        const quint32 at = quint32(mesh.vertices.size() / TubeMesh::stride);
        mesh.vertices.insert(mesh.vertices.end(), {p.x(), p.y(), p.z(), n.x(), n.y(), n.z(), u, v, kind, 0.0f});
        smoothed.push_back(smooth);
        return at;
    }

    QVector3D position(quint32 v) const
    {
        const float *p = &mesh.vertices[std::size_t(v) * TubeMesh::stride];
        return QVector3D(p[0], p[1], p[2]);
    }

    QVector3D normal(quint32 v) const
    {
        const float *p = &mesh.vertices[std::size_t(v) * TubeMesh::stride];
        return QVector3D(p[3], p[4], p[5]);
    }

    // The last vertex's spare value (a mark's blue).
    float &lastSpare() { return mesh.vertices.back(); }

    // A triangle facing `facing`'s way; one with no area is left out.
    void triangle(quint32 a, quint32 b, quint32 c, const QVector3D &facing)
    {
        const QVector3D n = QVector3D::crossProduct(position(b) - position(a), position(c) - position(a));
        if (n.lengthSquared() < 1e-12f) return;
        if (QVector3D::dotProduct(n, facing) < 0) std::swap(b, c);
        mesh.indices.insert(mesh.indices.end(), {a, b, c});
    }

    // Points that are one point of the skin twice (where its picture's
    // edges meet), smoothed as one.
    void twins(quint32 a, quint32 b) { pairs.emplace_back(a, b); }

    TubeMesh finish()
    {
        // Each skin point's normal from the faces round it, weighted by
        // their size; a cut end keeps its own.
        const std::size_t count = mesh.vertices.size() / TubeMesh::stride;
        std::vector<QVector3D> sum(count);
        for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
        {
            const quint32 a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
            const QVector3D n = QVector3D::crossProduct(position(b) - position(a), position(c) - position(a));
            for (quint32 v : {a, b, c}) if (smoothed[v]) sum[v] += n;
        }
        for (const auto &[a, b] : pairs)
        {
            const QVector3D both = sum[a] + sum[b];
            sum[a] = sum[b] = both;
        }
        QVector3D lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
        for (std::size_t v = 0; v < count; ++v)
        {
            float *p = &mesh.vertices[v * TubeMesh::stride];
            if (smoothed[v] && sum[v].lengthSquared() > 1e-20f)
            {
                const QVector3D n = sum[v].normalized();
                p[3] = n.x();
                p[4] = n.y();
                p[5] = n.z();
            }
            lo = QVector3D(std::min(lo.x(), p[0]), std::min(lo.y(), p[1]), std::min(lo.z(), p[2]));
            hi = QVector3D(std::max(hi.x(), p[0]), std::max(hi.y(), p[1]), std::max(hi.z(), p[2]));
        }
        if (count)
        {
            mesh.minimum = lo;
            mesh.maximum = hi;
        }
        return std::move(mesh);
    }

private:
    TubeMesh mesh;
    std::vector<bool> smoothed;
    std::vector<std::pair<quint32, quint32>> pairs;
};

} // namespace

DecorationTubes decorationTubes(const QJsonArray &tubes, const QJsonArray &marks, float halfWidth,
                                const QString &materialDirectory)
{
    DecorationTubes result;
    if (tubes.isEmpty()) return result;
    // What they are made of: the body's (its branches are of the same wood).
    QJsonObject looks = tubes.first().toObject();
    for (const QJsonValue &value : tubes)
        if (value.toObject().value(QLatin1String("body")).toBool())
        {
            looks = value.toObject();
            break;
        }
    const QJsonObject albedo = looks.value(QLatin1String("albedo")).toObject();
    result.bark = materialAlbedo(materialDirectory, albedo, looks.value(QLatin1String("recolour")).toObject(),
                                 looks.value(QLatin1String("adjust")).toObject());
    result.relief = materialHeights(materialDirectory, albedo.value(QLatin1String("texture")).toString());
    if (looks.value(QLatin1String("cap")).isObject())
        result.cap = materialAlbedo(materialDirectory, looks.value(QLatin1String("cap")).toObject(), {}, {});
    const QJsonObject surface = albedo.value(QLatin1String("surface")).toObject();
    result.metalness = float(looks.value(QLatin1String("metal")).toDouble(surface.value(QLatin1String("metal")).toDouble(0)));
    result.roughness = float(looks.value(QLatin1String("rough")).toDouble(surface.value(QLatin1String("rough")).toDouble(0.7)));
    // An upright grain (bark, wood) runs along the tube; one repeat of the
    // picture is as long as on a flat panel; the bark stands out of the tube
    // as far as its relief stands out of a flat one.
    const bool grain = albedo.value(QLatin1String("grain")).toBool();
    const float repeat = std::max(8.0f, 128.0f * (halfWidth / 24.0f) * float(albedo.value(QLatin1String("scale")).toDouble(1)));
    const float relief = result.relief.isNull() ? 0.0f
        : float(albedo.value(QLatin1String("relief")).toDouble(0.4)) * halfWidth * 0.09f;
    result.reliefDepth = relief;

    // The bodies' centre lines, which the branches rise away from.
    std::vector<QVector3D> bodies;
    for (const QJsonValue &value : tubes)
    {
        const QJsonObject tube = value.toObject();
        if (!tube.value(QLatin1String("body")).toBool()) continue;
        for (const QJsonValue &point : tube.value(QLatin1String("points")).toArray())
        {
            const QJsonArray p = point.toArray();
            bodies.emplace_back(float(p.at(0).toDouble()), float(p.at(1).toDouble()), float(p.at(2).toDouble()));
        }
    }
    // A branch grows out of the trunk's middle and rises as it grows away:
    // a lift by its distance beyond the trunk's skin, so a twig forking off
    // it starts where it is.
    const auto riseAt = [&](float x, float y) {
        if (bodies.empty()) return 0.0f;
        float beyond = 1e30f;
        for (const QVector3D &b : bodies) beyond = std::min(beyond, float(std::hypot(x - b.x(), y - b.y())) - b.z());
        return 0.3f * std::max(0.0f, beyond);
    };

    Builder builder;
    for (const QJsonValue &value : tubes)
    {
        const QJsonObject tube = value.toObject();
        const bool body = tube.value(QLatin1String("body")).toBool();
        const bool closed = tube.value(QLatin1String("closed")).toBool();
        std::vector<QVector3D> centre;
        std::vector<float> radius, lift;
        DecorationTubes::Spine spine;
        for (const QJsonValue &point : tube.value(QLatin1String("points")).toArray())
        {
            const QJsonArray p = point.toArray();
            const float x = float(p.at(0).toDouble()), y = float(p.at(1).toDouble());
            const float r = std::max(0.3f, float(p.at(2).toDouble()));
            const float z = body ? 0.0f : riseAt(x, y);
            const QVector3D at(x, -y, z);
            // A point no further on than the last is the last again.
            if (!centre.empty() && (at - centre.back()).lengthSquared() < 1e-6f) continue;
            centre.push_back(at);
            radius.push_back(r);
            lift.push_back(z);
            spine.points.emplace_back(x, y, r);
            spine.lift.push_back(z);
        }
        if (closed && centre.size() > 2 && (centre.front() - centre.back()).lengthSquared() < 1e-4f)
        {
            centre.pop_back();
            radius.pop_back();
            lift.pop_back();
        }
        const int count = int(centre.size());
        if (count < 2) continue;
        result.spines.push_back(std::move(spine));

        // How far along each point lies, and the repeat that fits a closed
        // tube a whole number of times.
        std::vector<float> along(std::size_t(count) + 1, 0.0f);
        for (int k = 1; k < count; ++k) along[k] = along[k - 1] + (centre[k] - centre[k - 1]).length();
        along[count] = along[count - 1] + (closed ? (centre[0] - centre[count - 1]).length() : 0.0f);
        const float total = closed ? along[count] : along[count - 1];
        const float length = closed ? total / std::max(1.0f, std::round(total / repeat)) : repeat;
        float meanRadius = 0;
        for (float r : radius) meanRadius += r;
        meanRadius /= float(count);
        const float roundRepeats = std::max(1.0f, std::round(float(2 * M_PI) * meanRadius / repeat));

        // The frame at each point: its way along, a side in the panel's
        // plane, and its up.
        std::vector<QVector3D> way(count), side(count), up(count);
        for (int k = 0; k < count; ++k)
        {
            const QVector3D before = closed ? centre[(k - 1 + count) % count] : centre[std::max(0, k - 1)];
            const QVector3D after = closed ? centre[(k + 1) % count] : centre[std::min(count - 1, k + 1)];
            QVector3D t = after - before;
            t = t.lengthSquared() > 1e-12f ? t.normalized() : QVector3D(1, 0, 0);
            QVector3D s = QVector3D::crossProduct(QVector3D(0, 0, 1), t);
            s = s.lengthSquared() > 1e-8f ? s.normalized() : QVector3D(0, 1, 0);
            way[k] = t;
            side[k] = s;
            up[k] = QVector3D::crossProduct(t, s).normalized();
        }

        // The skin: a ring of points at each point (a closed tube's first
        // ring again at its end, where the picture's last repeat ends).
        const int rings = closed ? count + 1 : count;
        std::vector<quint32> firsts(rings);
        for (int k = 0; k < rings; ++k)
        {
            const int c = k % count;
            firsts[k] = 0;
            for (int j = 0; j <= around; ++j)
            {
                const float theta = float(2 * M_PI) * float(j) / float(around);
                const QVector3D out = side[c] * std::cos(theta) + up[c] * std::sin(theta);
                const float u = along[k] / length, v = float(j) / float(around) * roundRepeats;
                const float tu = grain ? v : u, tv = grain ? u : v;
                // A thin twig is not broken up by bark deeper than itself.
                const float bump = relief > 0
                    ? (heightIn(result.relief, tu, tv) - 0.5f) * 2 * relief * std::min(1.0f, radius[c] / (3 * relief))
                    : 0.0f;
                const quint32 at = builder.vertex(centre[c] + out * (radius[c] + bump), out, tu, tv, 1, true);
                if (j == 0) firsts[k] = at;
            }
            builder.twins(firsts[k], firsts[k] + quint32(around));
        }
        if (closed)
            for (int j = 0; j <= around; ++j) builder.twins(firsts[0] + quint32(j), firsts[rings - 1] + quint32(j));
        for (int k = 0; k + 1 < rings; ++k)
            for (int j = 0; j < around; ++j)
            {
                const quint32 a = firsts[k] + quint32(j), b = a + 1;
                const quint32 c = firsts[k + 1] + quint32(j), d = c + 1;
                const QVector3D outward = builder.position(a) - centre[k % count];
                builder.triangle(a, c, b, outward);
                builder.triangle(b, c, d, outward);
            }

        // A cut end: a disc, its face the design's cut end where it has one.
        if (!closed)
            for (const bool start : {true, false})
            {
                const int c = start ? 0 : count - 1;
                const QVector3D facing = start ? -way[c] : way[c];
                const float kind = start && body && !result.cap.isNull() ? 2.0f : 1.0f;
                const quint32 middle = builder.vertex(centre[c], facing, 0.5f, 0.5f, kind, false);
                std::vector<quint32> rim;
                for (int j = 0; j <= around; ++j)
                {
                    const float theta = float(2 * M_PI) * float(j) / float(around);
                    const QVector3D out = side[c] * std::cos(theta) + up[c] * std::sin(theta);
                    rim.push_back(builder.vertex(centre[c] + out * radius[c], facing, 0.5f + 0.5f * std::cos(theta),
                                                 0.5f + 0.5f * std::sin(theta), kind, false));
                }
                for (int j = 0; j < around; ++j) builder.triangle(middle, rim[j], rim[j + 1], facing);
            }
    }
    // The marks, each point laid on the bark under it, a little above it,
    // facing out of the tube there.
    for (const QJsonValue &value : marks)
    {
        const QJsonObject mark = value.toObject();
        const QColor colour(mark.value(QLatin1String("colour")).toString().left(7));
        for (const QJsonValue &ringValue : mark.value(QLatin1String("rings")).toArray())
        {
            std::vector<quint32> ring;
            for (const QJsonValue &pointValue : ringValue.toArray())
            {
                const QJsonArray p = pointValue.toArray();
                const float x = float(p.at(0).toDouble()), y = float(p.at(1).toDouble());
                // The tube under it: the nearest point of any centre line,
                // how far across from it the point lies.
                float best = 1e30f, z = 0;
                QVector3D axis;
                for (const DecorationTubes::Spine &spine : result.spines)
                    for (std::size_t k = 0; k < spine.points.size(); ++k)
                    {
                        const QVector3D &c = spine.points[k];
                        const float d = float(std::hypot(x - c.x(), y - c.y()));
                        if (d - c.z() < best)
                        {
                            best = d - c.z();
                            const float r = c.z(), across = std::min(d, r * 0.98f);
                            z = spine.lift[k] + std::sqrt(std::max(0.0f, r * r - across * across)) + 0.35f;
                            axis = QVector3D(c.x(), -c.y(), spine.lift[k]);
                        }
                    }
                if (best > 1e29f) continue;
                const QVector3D at(x, -y, z);
                QVector3D out = at - axis;
                out = out.lengthSquared() > 1e-8f ? out.normalized() : QVector3D(0, 0, 1);
                ring.push_back(builder.vertex(at, out, float(colour.redF()), float(colour.greenF()), 3, false));
                builder.lastSpare() = float(colour.blueF());
            }
            for (std::size_t k = 1; k + 1 < ring.size(); ++k)
            {
                const QVector3D facing = builder.normal(ring[0]);
                builder.triangle(ring[0], ring[k], ring[k + 1], facing);
            }
        }
    }
    result.mesh = builder.finish();
    return result;
}

float tubeHeightAt(const DecorationTubes &tubes, QPointF point)
{
    float high = 0;
    for (const DecorationTubes::Spine &spine : tubes.spines)
        for (std::size_t k = 0; k < spine.points.size(); ++k)
        {
            const QVector3D &p = spine.points[k];
            const float d = float(std::hypot(point.x() - p.x(), point.y() - p.y()));
            if (d < p.z()) high = std::max(high, spine.lift[k] + std::sqrt(p.z() * p.z() - d * d));
        }
    return high;
}

} // namespace ArchDock
