// A decoration made solid for a 3D scene: its surface maps' height becomes
// an upper surface and an underside, with walls from the one to the other
// round its outline; or, a soft decoration's (a nebula's), the two curving
// round to meet each other halfway all round its outline (roundSurfaceEdges
// says how far round that curve each point lies).
//
// The walled solid's outline is traced by marching squares: each cell of a
// regular grid takes the shape its four corners' coverage gives it from the
// sixteen cases (the two-dimensional form of Lorensen and Cline's marching
// cubes, "Marching Cubes: A High Resolution 3D Surface Construction
// Algorithm", 1987), the crossing on each edge found by linear interpolation
// and the ambiguous saddle decided by the cell's middle.
//
// The rounded solid is cut from a regular grid of triangles, each square
// cell split corner to corner, and each triangle sliced where the curve
// passes set turns of its quarter circle: the way contour bands are drawn
// through a grid of samples by marching triangles (over a triangle the
// samples vary evenly, so each slice is a convex polygon, with no ambiguous
// cases to decide). Every point on one of those lines stands at the same
// height on the curve, so the curve runs evenly along the outline instead of
// in the steps the grid's own points would give it.
#include "DecorationRelief.h"

#include <QPointF>
#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ArchDock
{
namespace
{

float sampled(const std::vector<float> &plane, int w, int h, float x, float y)
{
    x = std::clamp(x, 0.0f, float(w - 1));
    y = std::clamp(y, 0.0f, float(h - 1));
    const int x0 = int(x), y0 = int(y);
    const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
    const float fx = x - float(x0), fy = y - float(y0);
    const float a = plane[std::size_t(y0) * w + x0], b = plane[std::size_t(y0) * w + x1];
    const float c = plane[std::size_t(y1) * w + x0], d = plane[std::size_t(y1) * w + x1];
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

// A running mean over a square of 2 * radius + 1 pixels, the edges held.
std::vector<float> boxBlurred(const std::vector<float> &plane, int w, int h, int radius)
{
    if (radius < 1) return plane;
    std::vector<float> rows(plane.size()), result(plane.size());
    const float scale = 1.0f / float(2 * radius + 1);
    auto pass = [&](const float *in, float *out, int n, int stride) {
        double sum = 0;
        for (int k = -radius; k <= radius; ++k) sum += in[std::size_t(std::clamp(k, 0, n - 1)) * stride];
        for (int i = 0; i < n; ++i)
        {
            out[std::size_t(i) * stride] = float(sum) * scale;
            sum += in[std::size_t(std::clamp(i + radius + 1, 0, n - 1)) * stride]
                   - in[std::size_t(std::clamp(i - radius, 0, n - 1)) * stride];
        }
    };
    for (int y = 0; y < h; ++y) pass(&plane[std::size_t(y) * w], &rows[std::size_t(y) * w], w, 1);
    for (int x = 0; x < w; ++x) pass(&rows[std::size_t(x)], &result[std::size_t(x)], h, w);
    return result;
}

// The box a mesh fills.
void boundsOf(ReliefMesh &mesh)
{
    if (mesh.vertices.empty()) return;
    QVector3D lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
    for (std::size_t v = 0; v < mesh.vertices.size(); v += ReliefMesh::stride)
    {
        const QVector3D p(mesh.vertices[v], mesh.vertices[v + 1], mesh.vertices[v + 2]);
        lo = QVector3D(std::min(lo.x(), p.x()), std::min(lo.y(), p.y()), std::min(lo.z(), p.z()));
        hi = QVector3D(std::max(hi.x(), p.x()), std::max(hi.y(), p.y()), std::max(hi.z(), p.z()));
    }
    mesh.minimum = lo;
    mesh.maximum = hi;
}

// The walled solid: its top, its underside flat on the slab, and walls.
class WallBuilder
{
public:
    WallBuilder(const DecorationSurface &surface, float underside, float step)
        : s(surface), W(surface.size.width()), H(surface.size.height()), below(underside)
    {
        NX = std::max(2, int(std::ceil(float(W - 1) / step)) + 1);
        NY = std::max(2, int(std::ceil(float(H - 1) / step)) + 1);
        sx = float(W - 1) / float(NX - 1);
        sy = float(H - 1) / float(NY - 1);
        mesh.step = std::max(sx, sy);
        // The grid follows the relief blurred to its own step; the maps
        // carry the finer grain to the light.
        const std::vector<float> rise = boxBlurred(s.rise, W, H, int(std::lround(mesh.step * 0.5f)));
        // The walls face the way the coverage falls, smoothed over a few of
        // the grid's steps, so they turn with the outline smoothly instead
        // of facing each little stretch of it.
        const int soft = std::max(1, int(std::lround(mesh.step)));
        smoothCover = boxBlurred(boxBlurred(s.cover, W, H, soft), W, H, soft);
        // The underside lies on the slab, `underside` below the ground.
        const std::vector<float> sink(s.cover.size(), below * float(s.scale));
        cover.resize(std::size_t(NX) * NY);
        height.resize(cover.size());
        under.resize(cover.size());
        for (int j = 0; j < NY; ++j)
            for (int i = 0; i < NX; ++i)
            {
                cover[index(i, j)] = sampled(s.cover, W, H, px(i), py(j));
                height[index(i, j)] = sampled(rise, W, H, px(i), py(j));
                under[index(i, j)] = sampled(sink, W, H, px(i), py(j));
            }
    }

    ReliefMesh build()
    {
        for (int j = 0; j + 1 < NY; ++j)
            for (int i = 0; i + 1 < NX; ++i) cell(i, j);
        // Each wall point's normal: where the smoothed coverage falls, or
        // the mean of the walls meeting there.
        struct WallPoint
        {
            quint32 top, bottom;
            QPointF place;
            QVector3D normal;
        };
        std::vector<WallPoint> points;
        points.reserve(wallTop.size());
        for (const auto &[key, at] : wallTop)
        {
            QVector3D n = wallNormal[key];
            n.setZ(0);
            const QPointF place = wallPlace[key];
            const float d = std::max(1.0f, mesh.step);
            const float gx = sampled(smoothCover, W, H, float(place.x()) + d, float(place.y()))
                             - sampled(smoothCover, W, H, float(place.x()) - d, float(place.y()));
            const float gy = sampled(smoothCover, W, H, float(place.x()), float(place.y()) + d)
                             - sampled(smoothCover, W, H, float(place.x()), float(place.y()) - d);
            // Down the coverage's slope, y turned up.
            const QVector3D falling(-gx, gy, 0);
            if (falling.lengthSquared() > 1e-8f && QVector3D::dotProduct(falling, n) > 0) n = falling;
            points.push_back({at, wallBottom[key], place, n.lengthSquared() > 1e-12f ? n.normalized() : QVector3D(1, 0, 0)});
        }
        // Those turned smoothly along the outline: each the mean, weighted
        // by nearness, of the wall points round it facing its own way (a
        // thin part's other side faces away), as a smooth curve's normals
        // turn - the outline's pixels would otherwise make them waver, and a
        // polished wall mirrors every waver as a stripe.
        const float reach = std::max(4.0f, 2.0f * mesh.step);
        const auto cellOf = [reach](const QPointF &p) {
            return (qint64(std::floor(p.x() / reach)) << 32) ^ qint64(quint32(qint32(std::floor(p.y() / reach))));
        };
        std::unordered_map<qint64, std::vector<std::size_t>> near;
        for (std::size_t k = 0; k < points.size(); ++k) near[cellOf(points[k].place)].push_back(k);
        for (WallPoint &w : points)
        {
            QVector3D sum;
            const int cx = int(std::floor(w.place.x() / reach)), cy = int(std::floor(w.place.y() / reach));
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const auto found = near.find((qint64(cx + dx) << 32) ^ qint64(quint32(qint32(cy + dy))));
                    if (found == near.end()) continue;
                    for (std::size_t k : found->second)
                    {
                        const WallPoint &o = points[k];
                        const float ex = float(o.place.x() - w.place.x()), ey = float(o.place.y() - w.place.y());
                        const float gap = ex * ex + ey * ey;
                        if (gap > reach * reach || QVector3D::dotProduct(o.normal, w.normal) < 0.3f) continue;
                        sum += o.normal * std::exp(-gap / (0.5f * reach * reach));
                    }
                }
            const QVector3D n = sum.lengthSquared() > 1e-12f ? sum.normalized() : w.normal;
            // The wall wears what the decoration is made of a pixel and a
            // half inside its outline, not the outline's own half-covered
            // pixels, which differ from one to the next.
            const float u = (float(w.place.x()) + 0.5f - n.x() * 1.5f) / float(W);
            const float v = (float(w.place.y()) + 0.5f + n.y() * 1.5f) / float(H);
            for (quint32 index : {w.top, w.bottom})
            {
                float *p = &mesh.vertices[std::size_t(index) * ReliefMesh::stride];
                p[3] = n.x();
                p[4] = n.y();
                p[5] = n.z();
                p[6] = u;
                p[7] = v;
            }
        }
        boundsOf(mesh);
        return std::move(mesh);
    }

private:
    // A point of the outline or the grid, as the cell walk meets it.
    struct Point
    {
        bool crossing = false;
        qint64 key = 0; // a grid point's index, or an edge's
        float x = 0, y = 0, z = 0; // the maps' pixels, and the height
        float depth = 0;           // how deep the underside lies there
        QVector3D normal, downward;
    };

    std::size_t index(int i, int j) const { return std::size_t(j) * NX + i; }
    float px(int i) const { return float(i) * sx; }
    float py(int j) const { return float(j) * sy; }
    bool inside(int i, int j) const { return cover[index(i, j)] >= 0.5f; }

    // The upper surface's normal at a grid point, from its height's slopes.
    QVector3D slopeNormal(int i, int j) const
    {
        const int i0 = std::max(0, i - 1), i1 = std::min(NX - 1, i + 1);
        const int j0 = std::max(0, j - 1), j1 = std::min(NY - 1, j + 1);
        const float gx = (height[index(i1, j)] - height[index(i0, j)]) / std::max(1e-6f, px(i1) - px(i0));
        const float gy = (height[index(i, j1)] - height[index(i, j0)]) / std::max(1e-6f, py(j1) - py(j0));
        // Down the maps is down the scene: y turns over.
        return QVector3D(-gx, gy, 1).normalized();
    }

    // The underside's normal at a grid point, facing down and out.
    QVector3D undersideNormal(int i, int j) const
    {
        const int i0 = std::max(0, i - 1), i1 = std::min(NX - 1, i + 1);
        const int j0 = std::max(0, j - 1), j1 = std::min(NY - 1, j + 1);
        const float gx = (under[index(i1, j)] - under[index(i0, j)]) / std::max(1e-6f, px(i1) - px(i0));
        const float gy = (under[index(i, j1)] - under[index(i, j0)]) / std::max(1e-6f, py(j1) - py(j0));
        return QVector3D(-gx, gy, -1).normalized();
    }

    Point corner(int i, int j) const
    {
        Point p;
        p.key = qint64(index(i, j));
        p.x = px(i);
        p.y = py(j);
        p.z = height[index(i, j)];
        p.depth = under[index(i, j)];
        p.normal = slopeNormal(i, j);
        p.downward = undersideNormal(i, j);
        return p;
    }

    // Where the outline crosses the edge from grid point a to grid point b:
    // where the decoration's own coverage is half, looked for along the edge
    // every half pixel - not where a straight line between the grid's two
    // samples of it is half, which wavers in and out of the drawn outline
    // from one grid step to the next (and so would everything read a set
    // way in from the outline, a wall's colour among them).
    Point crossing(int ia, int ja, int ib, int jb, qint64 key) const
    {
        const float ca = cover[index(ia, ja)], cb = cover[index(ib, jb)];
        float t = std::abs(cb - ca) > 1e-6f ? std::clamp((0.5f - ca) / (cb - ca), 0.0f, 1.0f) : 0.5f;
        {
            const float ax = px(ia), ay = py(ja), bx = px(ib), by = py(jb);
            const int samples = std::max(2, int(std::ceil(std::hypot(bx - ax, by - ay) / 0.5f)));
            float best = 2, previous = sampled(s.cover, W, H, ax, ay) - 0.5f;
            for (int k = 1; k <= samples; ++k)
            {
                const float u = float(k) / float(samples);
                const float here = sampled(s.cover, W, H, ax + (bx - ax) * u, ay + (by - ay) * u) - 0.5f;
                if ((previous < 0) != (here < 0))
                {
                    const float at = (float(k - 1) + previous / (previous - here)) / float(samples);
                    if (std::abs(at - t) < std::abs(best - t)) best = at;
                }
                previous = here;
            }
            if (best <= 1) t = best;
        }
        Point p;
        p.crossing = true;
        p.key = key;
        p.x = px(ia) + (px(ib) - px(ia)) * t;
        p.y = py(ja) + (py(jb) - py(ja)) * t;
        p.z = height[index(ia, ja)] * (1 - t) + height[index(ib, jb)] * t;
        p.depth = under[index(ia, ja)] * (1 - t) + under[index(ib, jb)] * t;
        p.normal = (slopeNormal(ia, ja) * (1 - t) + slopeNormal(ib, jb) * t).normalized();
        p.downward = (undersideNormal(ia, ja) * (1 - t) + undersideNormal(ib, jb) * t).normalized();
        return p;
    }

    quint32 vertex(float x, float y, float z, const QVector3D &n, float top)
    {
        const float scale = float(s.scale);
        const quint32 at = quint32(mesh.vertices.size() / ReliefMesh::stride);
        mesh.vertices.insert(mesh.vertices.end(), {
            // A grid point at a pixel's index stands for that pixel's middle.
            float(s.bounds.x()) + (x + 0.5f) / scale, -(float(s.bounds.y()) + (y + 0.5f) / scale), z / scale,
            n.x(), n.y(), n.z(),
            (x + 0.5f) / float(W), (y + 0.5f) / float(H),
            // No curve round its edge: as if its curve has ended.
            top, 1.0f});
        return at;
    }

    quint32 upper(const Point &p)
    {
        auto &table = p.crossing ? topCrossings : topCorners;
        const auto found = table.find(p.key);
        if (found != table.end()) return found->second;
        const quint32 v = vertex(p.x, p.y, p.z, p.normal, 1);
        table.emplace(p.key, v);
        return v;
    }

    quint32 lower(const Point &p)
    {
        auto &table = p.crossing ? bottomCrossings : bottomCorners;
        const auto found = table.find(p.key);
        if (found != table.end()) return found->second;
        const quint32 v = vertex(p.x, p.y, -p.depth, p.downward, 0);
        table.emplace(p.key, v);
        return v;
    }

    void wallPoint(const Point &p, const QVector3D &outward)
    {
        if (wallTop.find(p.key) == wallTop.end())
        {
            wallTop.emplace(p.key, vertex(p.x, p.y, p.z, outward, 0));
            wallBottom.emplace(p.key, vertex(p.x, p.y, -p.depth, outward, 0));
            wallPlace.emplace(p.key, QPointF(p.x, p.y));
        }
        wallNormal[p.key] += outward;
    }

    QVector3D position(quint32 v) const
    {
        const float *p = &mesh.vertices[std::size_t(v) * ReliefMesh::stride];
        return QVector3D(p[0], p[1], p[2]);
    }

    // A triangle facing the way `facing` says (its points taken in the
    // order that makes it so); one with no area is left out.
    void triangle(quint32 a, quint32 b, quint32 c, const QVector3D &facing)
    {
        const QVector3D n = QVector3D::crossProduct(position(b) - position(a), position(c) - position(a));
        if (n.lengthSquared() < 1e-14f) return;
        if (QVector3D::dotProduct(n, facing) < 0) std::swap(b, c);
        mesh.indices.insert(mesh.indices.end(), {a, b, c});
    }

    void polygon(const std::vector<Point> &points)
    {
        if (points.size() < 3) return;
        std::vector<quint32> ids;
        ids.reserve(points.size());
        for (const Point &p : points) ids.push_back(upper(p));
        for (std::size_t k = 1; k + 1 < ids.size(); ++k) triangle(ids[0], ids[k], ids[k + 1], QVector3D(0, 0, 1));
        // The underside under it, facing down.
        std::vector<quint32> down;
        down.reserve(points.size());
        for (const Point &p : points) down.push_back(lower(p));
        for (std::size_t k = 1; k + 1 < down.size(); ++k) triangle(down[0], down[k], down[k + 1], QVector3D(0, 0, -1));
        // The walls under its stretches of outline: the points go round
        // clockwise on the maps (y down), so the outside lies to the left
        // of each stretch, seen down the maps.
        for (std::size_t k = 0; k < points.size(); ++k)
        {
            const Point &a = points[k], &b = points[(k + 1) % points.size()];
            if (!a.crossing || !b.crossing) continue;
            const float dx = b.x - a.x, dy = b.y - a.y;
            if (dx * dx + dy * dy < 1e-10f) continue;
            // Where the top and the underside meet there is no wall to stand.
            if (a.z + a.depth < 0.05f && b.z + b.depth < 0.05f) continue;
            const QVector3D outward = QVector3D(dy, dx, 0).normalized();
            wallPoint(a, outward);
            wallPoint(b, outward);
            const quint32 at = wallTop[a.key], bt = wallTop[b.key];
            const quint32 ab = wallBottom[a.key], bb = wallBottom[b.key];
            triangle(at, bt, bb, outward);
            triangle(at, bb, ab, outward);
        }
    }

    void cell(int i, int j)
    {
        const bool tl = inside(i, j), tr = inside(i + 1, j), br = inside(i + 1, j + 1), bl = inside(i, j + 1);
        if (!tl && !tr && !br && !bl) return;
        // The edges' keys: a horizontal edge by its left point, a vertical
        // one by its upper point.
        const qint64 topEdge = qint64(index(i, j)) * 2, bottomEdge = qint64(index(i, j + 1)) * 2;
        const qint64 leftEdge = qint64(index(i, j)) * 2 + 1, rightEdge = qint64(index(i + 1, j)) * 2 + 1;
        std::vector<Point> walk;
        walk.reserve(8);
        if (tl) walk.push_back(corner(i, j));
        if (tl != tr) walk.push_back(crossing(i, j, i + 1, j, topEdge));
        if (tr) walk.push_back(corner(i + 1, j));
        if (tr != br) walk.push_back(crossing(i + 1, j, i + 1, j + 1, rightEdge));
        if (br) walk.push_back(corner(i + 1, j + 1));
        if (br != bl) walk.push_back(crossing(i, j + 1, i + 1, j + 1, bottomEdge));
        if (bl) walk.push_back(corner(i, j + 1));
        if (bl != tl) walk.push_back(crossing(i, j, i, j + 1, leftEdge));
        const bool saddle = (tl && br && !tr && !bl) || (tr && bl && !tl && !br);
        const float middle = (cover[index(i, j)] + cover[index(i + 1, j)] + cover[index(i + 1, j + 1)]
                              + cover[index(i, j + 1)]) / 4;
        if (saddle && middle < 0.5f)
        {
            // Two separate corners: each with its own two crossings.
            if (tl)
            {
                polygon({walk[5], walk[0], walk[1]});
                polygon({walk[2], walk[3], walk[4]});
            }
            else
            {
                polygon({walk[0], walk[1], walk[2]});
                polygon({walk[3], walk[4], walk[5]});
            }
            return;
        }
        polygon(walk);
    }

    const DecorationSurface &s;
    const int W, H;
    const float below;
    int NX = 2, NY = 2;
    float sx = 1, sy = 1;
    std::vector<float> cover, height, under, smoothCover;
    std::unordered_map<qint64, quint32> topCorners, topCrossings, bottomCorners, bottomCrossings, wallTop, wallBottom;
    std::unordered_map<qint64, QVector3D> wallNormal;
    std::unordered_map<qint64, QPointF> wallPlace;
    ReliefMesh mesh;
};

// The rounded solid: its top and underside curving round to meet.
class RoundBuilder
{
public:
    RoundBuilder(const DecorationSurface &surface, float step)
        : s(surface), W(surface.size.width()), H(surface.size.height())
    {
        NX = std::max(2, int(std::ceil(float(W - 1) / step)) + 1);
        NY = std::max(2, int(std::ceil(float(H - 1) / step)) + 1);
        sx = float(W - 1) / float(NX - 1);
        sy = float(H - 1) / float(NY - 1);
        mesh.step = std::max(sx, sy);
        // The top follows the relief blurred to the grid's step; the maps
        // carry the finer grain to the light.
        rise = boxBlurred(s.rise, W, H, int(std::lround(mesh.step * 0.5f)));
        // Where the curve turns by an equal angle from one line to the
        // next: so many that each band is a smooth enough stretch of it.
        for (int k = 0; k <= bands; ++k) turns[k] = 1 - std::cos(float(k) / bands * float(M_PI) / 2);
        along.resize(std::size_t(NX) * NY);
        for (int j = 0; j < NY; ++j)
            for (int i = 0; i < NX; ++i)
            {
                float u = sampled(s.edgeAlong, W, H, px(i), py(j));
                // A grid point exactly on a line would split a slice in two
                // at one place.
                for (float turn : turns)
                    if (std::abs(u - turn) < 1e-5f) u = turn + 2e-5f;
                along[index(i, j)] = u;
            }
    }

    ReliefMesh build()
    {
        for (int j = 0; j + 1 < NY; ++j)
            for (int i = 0; i + 1 < NX; ++i)
            {
                // Both halves of the cell go round the same way, facing up
                // on the solid (its y turns the maps' over).
                const Point a = corner(i, j), b = corner(i + 1, j), c = corner(i + 1, j + 1), d = corner(i, j + 1);
                slice({a, c, b});
                slice({a, d, c});
            }
        boundsOf(mesh);
        return std::move(mesh);
    }

private:
    static constexpr int bands = 8;

    // A point of a slice: a grid point (`a`), or where line `turn` crosses
    // the grid's edge between points `a` and `b` (a below b).
    struct Point
    {
        int a = -1, b = -1, turn = -1;
        float x = 0, y = 0, u = 0;
    };

    std::size_t index(int i, int j) const { return std::size_t(j) * NX + i; }
    float px(int i) const { return float(i) * sx; }
    float py(int j) const { return float(j) * sy; }
    float gx(int k) const { return px(k % NX); }
    float gy(int k) const { return py(k / NX); }

    Point corner(int i, int j) const
    {
        Point p;
        p.a = int(index(i, j));
        p.x = px(i);
        p.y = py(j);
        p.u = along[index(i, j)];
        return p;
    }

    // Where line `turn` crosses the grid's edge under the two points: worked
    // out from that edge's own ends, so both triangles beside it agree.
    Point crossing(const Point &p, const Point &q, int turn) const
    {
        int a = p.a, b = q.a;
        if (p.b >= 0) b = p.b;
        else if (q.b >= 0)
        {
            a = q.a;
            b = q.b;
        }
        if (a > b) std::swap(a, b);
        const float ua = along[std::size_t(a)], ub = along[std::size_t(b)];
        const float t = std::abs(ub - ua) > 1e-12f ? std::clamp((turns[turn] - ua) / (ub - ua), 0.0f, 1.0f) : 0.5f;
        Point c;
        c.a = a;
        c.b = b;
        c.turn = turn;
        c.x = gx(a) + (gx(b) - gx(a)) * t;
        c.y = gy(a) + (gy(b) - gy(a)) * t;
        c.u = turns[turn];
        return c;
    }

    // The part of a convex polygon on one side of line `turn` (Sutherland
    // and Hodgman's clipping); the side `above` has more of the curve.
    std::vector<Point> clipped(const std::vector<Point> &polygon, int turn, bool above) const
    {
        std::vector<Point> result;
        const float line = turns[turn];
        for (std::size_t k = 0; k < polygon.size(); ++k)
        {
            const Point &p = polygon[k], &q = polygon[(k + 1) % polygon.size()];
            const bool pin = above ? p.u >= line : p.u <= line;
            const bool qin = above ? q.u >= line : q.u <= line;
            if (pin) result.push_back(p);
            if (pin != qin) result.push_back(crossing(p, q, turn));
        }
        return result;
    }

    // A triangle of the grid, cut into the bands between the lines and what
    // lies past the last, where the curve has ended.
    void slice(const std::vector<Point> &triangle)
    {
        float lo = triangle[0].u, hi = triangle[0].u;
        for (const Point &p : triangle)
        {
            lo = std::min(lo, p.u);
            hi = std::max(hi, p.u);
        }
        if (hi < 0) return;
        if (lo >= 1)
        {
            lay(triangle);
            return;
        }
        std::vector<Point> rest = clipped(triangle, 0, true);
        for (int k = 0; k < bands && rest.size() >= 3; ++k)
        {
            lay(clipped(rest, k + 1, false));
            rest = clipped(rest, k + 1, true);
        }
        lay(rest);
    }

    void lay(const std::vector<Point> &polygon)
    {
        if (polygon.size() < 3) return;
        std::vector<quint32> up, down;
        up.reserve(polygon.size());
        down.reserve(polygon.size());
        for (const Point &p : polygon)
        {
            up.push_back(vertexOf(p, true));
            down.push_back(vertexOf(p, false));
        }
        for (std::size_t k = 1; k + 1 < polygon.size(); ++k)
        {
            triangle(up[0], up[k], up[k + 1]);
            triangle(down[0], down[k + 1], down[k]);
        }
    }

    // A height field's slope at a point of the maps, over a pixel either way.
    QVector2D slope(const std::vector<float> &field, float x, float y) const
    {
        return QVector2D((sampled(field, W, H, x + 1, y) - sampled(field, W, H, x - 1, y)) * 0.5f,
                         (sampled(field, W, H, x, y + 1) - sampled(field, W, H, x, y - 1)) * 0.5f);
    }

    // A point's vertex on the top or the underside, made once.
    quint32 vertexOf(const Point &p, bool upper)
    {
        qint64 key = p.a;
        if (p.b >= 0)
        {
            const int way = p.b - p.a == 1 ? 0 : p.b - p.a == NX ? 1 : 2;
            key = -1 - ((qint64(p.a) * 3 + way) * (bands + 1) + p.turn);
        }
        auto &table = upper ? tops : bottoms;
        const auto found = table.find(key);
        if (found != table.end()) return found->second;
        const QVector2D alongSlope = slope(s.edgeAlong, p.x, p.y);
        const EdgePoint e = edgePoint(p.u, alongSlope, sampled(rise, W, H, p.x, p.y), slope(rise, p.x, p.y),
                                      sampled(s.edgeLevel, W, H, p.x, p.y), s.edgeDeep);
        // The maps read further in, round the curve.
        const QVector2D in = alongSlope.lengthSquared() > 1e-12f ? alongSlope.normalized() : QVector2D();
        const quint32 v = vertex(p.x, p.y, upper ? e.top : e.bottom, upper ? e.upward : e.downward,
                                 p.x + in.x() * e.inward, p.y + in.y() * e.inward, upper, std::clamp(p.u, 0.0f, 1.0f));
        table.emplace(key, v);
        return v;
    }

    quint32 vertex(float x, float y, float z, const QVector3D &n, float mapX, float mapY, bool upper, float round)
    {
        const float scale = float(s.scale);
        const quint32 at = quint32(mesh.vertices.size() / ReliefMesh::stride);
        mesh.vertices.insert(mesh.vertices.end(), {
            // A grid point at a pixel's index stands for that pixel's middle.
            float(s.bounds.x()) + (x + 0.5f) / scale, -(float(s.bounds.y()) + (y + 0.5f) / scale), z / scale,
            n.x(), n.y(), n.z(),
            (mapX + 0.5f) / float(W), (mapY + 0.5f) / float(H),
            upper ? 1.0f : 0.0f, round});
        return at;
    }

    // A triangle as its points are given (the slices keep the grid's way
    // round); one with no area is left out.
    void triangle(quint32 a, quint32 b, quint32 c)
    {
        const auto position = [this](quint32 v) {
            const float *p = &mesh.vertices[std::size_t(v) * ReliefMesh::stride];
            return QVector3D(p[0], p[1], p[2]);
        };
        if (QVector3D::crossProduct(position(b) - position(a), position(c) - position(a)).lengthSquared() < 1e-14f) return;
        mesh.indices.insert(mesh.indices.end(), {a, b, c});
    }

    const DecorationSurface &s;
    const int W, H;
    int NX = 2, NY = 2;
    float sx = 1, sy = 1;
    float turns[bands + 1] = {};
    // How far round the edge's curve each grid point lies; the top's height
    // at each of the maps' pixels, blurred to the grid's step.
    std::vector<float> along, rise;
    std::unordered_map<qint64, quint32> tops, bottoms;
    ReliefMesh mesh;
};

} // namespace

ReliefMesh reliefMesh(const DecorationSurface &surface, float underside, int maximumTriangles)
{
    const int W = surface.size.width(), H = surface.size.height();
    const std::size_t n = std::size_t(W) * H;
    if (W < 2 || H < 2 || surface.cover.size() != n || surface.rise.size() != n) return {};
    const bool rounded = surface.edgeAlong.size() == n && surface.edgeLevel.size() == n;
    std::size_t covered = 0;
    for (float c : surface.cover) covered += c >= 0.5f;
    if (covered == 0) return {};
    // Two triangles a cell on the upper surface, two on the underside, and
    // walls round them, or more where a rounded edge's curve slices them:
    // as fine a grid as the triangles allow, never finer than a pixel.
    const int budget = std::max(1000, maximumTriangles);
    float step = std::max(1.0f, std::sqrt((rounded ? 5.0f : 4.4f) * float(covered) / (0.9f * float(budget))));
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        ReliefMesh mesh = rounded ? RoundBuilder(surface, step).build() : WallBuilder(surface, underside, step).build();
        if (int(mesh.indices.size() / 3) <= budget || attempt == 7) return mesh;
        step *= 1.25f;
    }
    return {};
}

} // namespace ArchDock
