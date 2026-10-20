// The decoration lighting: a decoration's scene lit per pixel, the way its
// reference design pictures were lit.
//
// Methods, by their textbook formulas: exact Euclidean distance transform by
// lower envelopes of parabolas (Felzenszwalb and Huttenlocher, "Distance
// Transforms of Sampled Functions", 2012); separable Gaussian blur, with
// three box passes for wide radii (Kovesi, "Fast Almost-Gaussian Filtering",
// 2010); sliding-window maximum with a monotonic queue; Blinn-Phong
// highlights normalised by (n + 8) / 8 pi with Schlick's Fresnel
// approximation (1994); thin-film colours drawn round the hue wheel.
#include "DecorationLighting.h"

#include <QColor>
#include <QFileInfo>
#include <QFloat16>
#include <QJsonArray>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QRadialGradient>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <optional>
#include <random>
#include <vector>

namespace ArchDock
{
namespace
{

using Rgb = std::array<float, 3>;

// A float picture of `channels` values per pixel.
struct Plane
{
    int width = 0;
    int height = 0;
    int channels = 1;
    std::vector<float> data;

    Plane() = default;
    Plane(int w, int h, int c = 1, float value = 0)
        : width(w), height(h), channels(c), data(std::size_t(w) * std::size_t(h) * std::size_t(c), value) {}
    float &at(int x, int y, int c = 0) { return data[(std::size_t(y) * width + x) * channels + c]; }
    float at(int x, int y, int c = 0) const { return data[(std::size_t(y) * width + x) * channels + c]; }
};

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
float smoothstep01(float x) { x = clamp01(x); return x * x * (3 - 2 * x); }

Rgb colourOf(const QString &text, float *alpha = nullptr)
{
    // #rrggbbaa (the scene's own way of writing a colour with its opacity)
    // or anything QColor reads.
    QString value = text.trimmed();
    float a = 1;
    if (value.size() == 9 && value.startsWith(QLatin1Char('#')))
    {
        bool ok = false;
        a = float(value.mid(7, 2).toInt(&ok, 16)) / 255.0f;
        value = value.left(7);
    }
    const QColor colour(value);
    if (alpha) *alpha = a;
    if (!colour.isValid()) return {0.5f, 0.5f, 0.5f};
    return {float(colour.redF()), float(colour.greenF()), float(colour.blueF())};
}

// HSV with every value in 0..1.
Rgb rgbToHsv(const Rgb &c)
{
    const float r = clamp01(c[0]), g = clamp01(c[1]), b = clamp01(c[2]);
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
    float h = 0;
    if (d > 1e-6f)
    {
        if (mx == r) h = std::fmod((g - b) / d, 6.0f);
        else if (mx == g) h = (b - r) / d + 2;
        else h = (r - g) / d + 4;
        h /= 6;
        if (h < 0) h += 1;
    }
    return {h - std::floor(h), mx > 1e-6f ? d / mx : 0, mx};
}

Rgb hsvToRgb(const Rgb &hsv)
{
    const float h = (hsv[0] - std::floor(hsv[0])) * 6, s = hsv[1], v = hsv[2];
    const int i = int(std::floor(h)) % 6;
    const float f = h - std::floor(h);
    const float p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i)
    {
    case 0: return {v, t, p};
    case 1: return {q, v, p};
    case 2: return {p, v, t};
    case 3: return {p, q, v};
    case 4: return {t, p, v};
    default: return {v, p, q};
    }
}

// The app's rainbow: a bright spectral colour round the wheel.
Rgb spectrum(float hue) { return hsvToRgb({hue - std::floor(hue), 0.85f, 1.0f}); }

// One line of a separable filter, read and written with a stride.
void convolveLine(const float *in, float *out, int n, int stride, const std::vector<float> &kernel)
{
    const int r = int(kernel.size() / 2);
    for (int i = 0; i < n; ++i)
    {
        float sum = 0;
        for (int k = -r; k <= r; ++k)
        {
            const int j = std::clamp(i + k, 0, n - 1);
            sum += in[std::size_t(j) * stride] * kernel[std::size_t(k + r)];
        }
        out[std::size_t(i) * stride] = sum;
    }
}

void boxLine(const float *in, float *out, int n, int stride, int radius)
{
    // A running sum over [i - radius, i + radius], the ends held.
    const float scale = 1.0f / float(2 * radius + 1);
    double sum = 0;
    for (int k = -radius; k <= radius; ++k) sum += in[std::size_t(std::clamp(k, 0, n - 1)) * stride];
    for (int i = 0; i < n; ++i)
    {
        out[std::size_t(i) * stride] = float(sum) * scale;
        const int add = std::clamp(i + radius + 1, 0, n - 1), drop = std::clamp(i - radius, 0, n - 1);
        sum += in[std::size_t(add) * stride] - in[std::size_t(drop) * stride];
    }
}

// A Gaussian blur of every channel, sigma in pixels.
Plane gaussian(const Plane &source, float sigma)
{
    Plane result = source;
    if (sigma < 0.25f || source.width == 0 || source.height == 0) return result;
    const int w = source.width, h = source.height, c = source.channels;
    const std::size_t longest = std::size_t(std::max(w, h));
    std::vector<float> line(longest);
    std::vector<float> line2(longest);
    auto passes = [&](auto &&filterLine) {
        for (int ch = 0; ch < c; ++ch)
        {
            for (int y = 0; y < h; ++y)
            {
                float *row = &result.data[(std::size_t(y) * w) * c + ch];
                for (int x = 0; x < w; ++x) line[std::size_t(x)] = row[std::size_t(x) * c];
                filterLine(line.data(), line2.data(), w);
                for (int x = 0; x < w; ++x) row[std::size_t(x) * c] = line2[std::size_t(x)];
            }
            for (int x = 0; x < w; ++x)
            {
                float *column = &result.data[std::size_t(x) * c + ch];
                for (int y = 0; y < h; ++y) line[std::size_t(y)] = column[std::size_t(y) * w * c];
                filterLine(line.data(), line2.data(), h);
                for (int y = 0; y < h; ++y) column[std::size_t(y) * w * c] = line2[std::size_t(y)];
            }
        }
    };
    if (sigma <= 2.0f)
    {
        const int r = int(std::ceil(sigma * 3.0f));
        std::vector<float> kernel(std::size_t(2 * r + 1));
        float sum = 0;
        for (int k = -r; k <= r; ++k)
        {
            const float v = std::exp(-0.5f * float(k * k) / (sigma * sigma));
            kernel[std::size_t(k + r)] = v;
            sum += v;
        }
        for (float &v : kernel) v /= sum;
        passes([&](const float *in, float *out, int n) { convolveLine(in, out, n, 1, kernel); });
        return result;
    }
    // Three box passes whose widths average to the Gaussian's variance.
    const int n = 3;
    const float ideal = std::sqrt(12.0f * sigma * sigma / n + 1);
    int lower = int(std::floor(ideal));
    if (lower % 2 == 0) --lower;
    const int upper = lower + 2;
    const int m = int(std::lround((12.0f * sigma * sigma - n * lower * lower - 4.0f * n * lower - 3.0f * n)
                                  / (-4.0f * lower - 4.0f)));
    for (int pass = 0; pass < n; ++pass)
    {
        const int radius = ((pass < m ? lower : upper) - 1) / 2;
        passes([&](const float *in, float *out, int count) { boxLine(in, out, count, 1, radius); });
    }
    return result;
}

// The exact distance from every pixel inside a mask to the nearest pixel
// outside it (0 outside).
std::vector<float> distanceInside(const std::vector<uint8_t> &inside, int w, int h)
{
    const double big = 1e20;
    std::vector<double> f(std::size_t(w) * h);
    for (std::size_t i = 0; i < f.size(); ++i) f[i] = inside[i] ? big : 0;
    const std::size_t n = std::size_t(std::max(w, h));
    std::vector<double> line(n), out(n), z(n + 1);
    std::vector<int> v(n);
    auto envelope = [&](int count) {
        int k = 0;
        v[0] = 0;
        z[0] = -big;
        z[1] = big;
        for (int q = 1; q < count; ++q)
        {
            double s = ((line[std::size_t(q)] + double(q) * q) - (line[std::size_t(v[k])] + double(v[k]) * v[k]))
                       / (2.0 * q - 2.0 * v[k]);
            while (k > 0 && s <= z[std::size_t(k)])
            {
                --k;
                s = ((line[std::size_t(q)] + double(q) * q) - (line[std::size_t(v[k])] + double(v[k]) * v[k]))
                    / (2.0 * q - 2.0 * v[k]);
            }
            ++k;
            v[std::size_t(k)] = q;
            z[std::size_t(k)] = s;
            z[std::size_t(k) + 1] = big;
        }
        k = 0;
        for (int q = 0; q < count; ++q)
        {
            while (z[std::size_t(k) + 1] < q) ++k;
            const double d = double(q - v[std::size_t(k)]);
            out[std::size_t(q)] = d * d + line[std::size_t(v[std::size_t(k)])];
        }
    };
    for (int x = 0; x < w; ++x)
    {
        for (int y = 0; y < h; ++y) line[std::size_t(y)] = f[std::size_t(y) * w + x];
        envelope(h);
        for (int y = 0; y < h; ++y) f[std::size_t(y) * w + x] = out[std::size_t(y)];
    }
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x) line[std::size_t(x)] = f[std::size_t(y) * w + x];
        envelope(w);
        for (int x = 0; x < w; ++x) f[std::size_t(y) * w + x] = out[std::size_t(x)];
    }
    std::vector<float> result(f.size());
    for (std::size_t i = 0; i < f.size(); ++i) result[i] = float(std::sqrt(std::min(f[i], 1e12)));
    return result;
}

// The largest value in a square window of `size` pixels round each pixel.
std::vector<float> maximumFilter(const std::vector<float> &source, int w, int h, int size)
{
    std::vector<float> rows(source.size()), result(source.size());
    const int before = size / 2, after = (size - 1) / 2;
    auto pass = [&](const float *in, float *out, int n, int stride) {
        std::deque<int> window;
        int next = 0;
        for (int i = 0; i < n; ++i)
        {
            const int end = std::min(n - 1, i + after);
            while (next <= end)
            {
                while (!window.empty() && in[std::size_t(window.back()) * stride] <= in[std::size_t(next) * stride])
                    window.pop_back();
                window.push_back(next++);
            }
            while (window.front() < i - before) window.pop_front();
            out[std::size_t(i) * stride] = in[std::size_t(window.front()) * stride];
        }
    };
    for (int y = 0; y < h; ++y) pass(&source[std::size_t(y) * w], &rows[std::size_t(y) * w], w, 1);
    for (int x = 0; x < w; ++x) pass(&rows[std::size_t(x)], &result[std::size_t(x)], h, w);
    return result;
}

// For every pixel, the nearest pixel of a mask (its index; -1 when the mask
// is empty): the distance transform's two passes, each remembering which
// pixel gave it its distance (a feature transform) - first the nearest down
// each column, then the lower envelope of those columns' parabolas along
// each row.
std::vector<int> nearestOf(const std::vector<uint8_t> &mask, int w, int h)
{
    std::vector<int> nearestRow(std::size_t(w) * h, -1);
    for (int x = 0; x < w; ++x)
    {
        int last = -1;
        for (int y = 0; y < h; ++y)
        {
            if (mask[std::size_t(y) * w + x]) last = y;
            nearestRow[std::size_t(y) * w + x] = last;
        }
        last = -1;
        for (int y = h - 1; y >= 0; --y)
        {
            if (mask[std::size_t(y) * w + x]) last = y;
            int &best = nearestRow[std::size_t(y) * w + x];
            if (last >= 0 && (best < 0 || last - y < y - best)) best = last;
        }
    }
    const double big = 1e20;
    std::vector<int> nearest(std::size_t(w) * h, -1);
    const std::size_t columns = std::size_t(w);
    std::vector<double> f(columns), z(columns + 1);
    std::vector<int> v(columns);
    for (int y = 0; y < h; ++y)
    {
        const int *row = &nearestRow[std::size_t(y) * w];
        // The columns holding a pixel of the mask, each a parabola.
        int k = -1;
        for (int q = 0; q < w; ++q)
        {
            if (row[q] < 0) continue;
            f[std::size_t(q)] = double(row[q] - y) * (row[q] - y);
            if (k < 0)
            {
                k = 0;
                v[0] = q;
                z[0] = -big;
                z[1] = big;
                continue;
            }
            const auto meet = [&](int a) {
                return ((f[std::size_t(q)] + double(q) * q) - (f[std::size_t(a)] + double(a) * a)) / (2.0 * q - 2.0 * a);
            };
            double s = meet(v[std::size_t(k)]);
            while (k > 0 && s <= z[std::size_t(k)])
            {
                --k;
                s = meet(v[std::size_t(k)]);
            }
            ++k;
            v[std::size_t(k)] = q;
            z[std::size_t(k)] = s;
            z[std::size_t(k) + 1] = big;
        }
        if (k < 0) continue;
        int j = 0;
        for (int q = 0; q < w; ++q)
        {
            while (z[std::size_t(j) + 1] < q) ++j;
            const int x = v[std::size_t(j)];
            nearest[std::size_t(y) * w + q] = row[x] * w + x;
        }
    }
    return nearest;
}

// How wide a shape's part is at each of its pixels, as the distance from
// its middle to its outline: each ridge of the distance to the outline
// carried down that distance's slope to the outline (the pixels taken from
// the farthest in, each keeping the widest its farther-in neighbours hold),
// close to the radius of the widest disc inside the shape over the pixel,
// the "local thickness" of Hildebrand and Ruegsegger ("A new method for the
// model-independent assessment of thickness in three-dimensional images",
// 1997).
std::vector<float> partHalfWidth(const std::vector<float> &distance, int w, int h)
{
    std::vector<float> widest(distance);
    float farthest = 0;
    for (float d : distance) farthest = std::max(farthest, d);
    if (farthest <= 0) return widest;
    // A counting sort by the distance in quarter pixels, farthest first.
    const int buckets = int(farthest * 4) + 1;
    const auto bucketOf = [&](float d) { return buckets - 1 - std::min(buckets - 1, int(d * 4)); };
    std::vector<int> start(std::size_t(buckets) + 1, 0);
    for (float d : distance)
        if (d > 0) ++start[std::size_t(bucketOf(d)) + 1];
    for (int b = 0; b < buckets; ++b) start[std::size_t(b) + 1] += start[std::size_t(b)];
    std::vector<int> order(std::size_t(start[std::size_t(buckets)]));
    for (std::size_t i = 0; i < distance.size(); ++i)
        if (distance[i] > 0) order[std::size_t(start[std::size_t(bucketOf(distance[i]))]++)] = int(i);
    for (int p : order)
    {
        const int x = p % w, y = p / w;
        float best = widest[std::size_t(p)];
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int xx = x + dx, yy = y + dy;
                if ((dx == 0 && dy == 0) || xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
                const std::size_t q = std::size_t(yy) * w + xx;
                if (distance[q] > distance[std::size_t(p)]) best = std::max(best, widest[q]);
            }
        widest[std::size_t(p)] = best;
    }
    return widest;
}

// The materials' pictures, read once.
QImage materialPicture(const QString &directory, const QString &texture, bool heights)
{
    static QMutex mutex;
    static QHash<QString, QImage> cache;
    const QString key = directory + QLatin1Char('|') + texture + (heights ? QStringLiteral("|h") : QString());
    QMutexLocker lock(&mutex);
    const auto found = cache.constFind(key);
    if (found != cache.constEnd()) return found.value();
    QImage image;
    if (heights)
        image.load(directory + QStringLiteral("/deco-") + texture + QStringLiteral("-height.png"));
    else
    {
        const QString jpeg = directory + QStringLiteral("/deco-") + texture + QStringLiteral(".jpg");
        image.load(QFileInfo::exists(jpeg) ? jpeg : directory + QStringLiteral("/deco-") + texture + QStringLiteral(".png"));
    }
    if (!image.isNull()) image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    cache.insert(key, image);
    return image;
}

// The band's path in the pictures' pixels: its points, the length along it
// at each, and which way it mostly runs.
struct BandPath
{
    std::vector<QPointF> points;
    std::vector<double> at;
    bool closed = false;
    double length = 0;
    QRectF box;
    QPointF centre;
    bool horizontal = true;

    float along(float x, float y) const
    {
        // Where a pixel lies along the band, 0 to 1: round its centre for a
        // closed band, across its main direction for an open one.
        if (closed)
            return float((std::atan2(y - centre.y(), x - centre.x()) + M_PI) / (2 * M_PI));
        if (horizontal) return box.width() > 0 ? float((x - box.left()) / box.width()) : 0;
        return box.height() > 0 ? float((y - box.top()) / box.height()) : 0;
    }
};

// One element of the scene, read from its JSON with every length in the
// pictures' pixels.
struct Element
{
    QString type, role, kind, profile, group, cap, finish, lightFinish;
    QPainterPath shape;          // a fill's rings, a stroke's line, a dot's circle
    std::vector<QPointF> points; // a stroke's points
    bool closed = false, evenOdd = false, taper = false, clip = false, stack = false;
    bool ink = false, overlay = false, sheen = false, flame = false, hasLens = false;
    float width = 1, radius = 0, height = 0.3f, bevel = 0.15f, period = 2.6f, feather = 0;
    float metal = 0, rough = 0.5f, coat = 0, opacity = 1, emissive = 0, glowGain = 1, strength = 0.8f;
    // How much of its light it gives at this moment (lights chasing along).
    float lit = 1;
    // How brightly it is lit round about, as jewellery is photographed in a
    // light tent: above 1 its reflections show the bright surround in its
    // own colour (gold that shines as gold), 1 the studio as it is.
    float gleam = 1;
    // How much of its colour is its own light (a nebula's picture), which a
    // 3D scene's lights only shade a little.
    float own = 0;
    std::optional<float> lift;
    std::vector<qreal> dash;
    QPointF centre;
    // albedo
    QString texture;
    bool strip = false, fit = false, grain = false;
    // How far a picture laid along the band has drifted along it (pixels).
    float shift = 0;
    std::optional<Rgb> tint;
    float relief = 0.4f, textureScale = 1, alpha = 1;
    Rgb colour{0.5f, 0.5f, 0.5f};
    bool gradient = false;
    Rgb gradientInner{1, 1, 1}, gradientOuter{0, 0, 0};
    QPointF gradientCentre;
    float gradientRadius = 1;
    std::optional<Rgb> glow;
    // the lens
    Rgb lensColour{1, 0, 0};
    QPointF lensCentre;
    float lensRadius = 1;
    // the viewer's colours
    bool recolour = false, adjust = false;
    float recolourHue = 0, recolourSaturation = 0, recolourValue = 0;
    float adjustHue = 0, adjustSaturation = 1, adjustBrightness = 1;
    std::optional<float> flameHue;
};

float number(const QJsonObject &object, const char *key, float fallback)
{
    const QJsonValue value = object.value(QLatin1String(key));
    return value.isDouble() ? float(value.toDouble()) : fallback;
}

QPointF pointOf(const QJsonValue &value, const QTransform &toPicture)
{
    const QJsonArray pair = value.toArray();
    return toPicture.map(QPointF(pair.at(0).toDouble(), pair.at(1).toDouble()));
}

Element readElement(const QJsonObject &object, const QTransform &toPicture, double scale)
{
    Element e;
    e.type = object.value(QLatin1String("type")).toString();
    e.role = object.value(QLatin1String("role")).toString();
    e.kind = object.value(QLatin1String("kind")).toString();
    e.profile = object.value(QLatin1String("profile")).toString(QStringLiteral("flat"));
    e.group = object.value(QLatin1String("group")).toString();
    e.cap = object.value(QLatin1String("cap")).toString(QStringLiteral("round"));
    e.finish = object.value(QLatin1String("finish")).toString();
    e.lightFinish = object.value(QLatin1String("lightFinish")).toString();
    e.closed = object.value(QLatin1String("closed")).toBool();
    e.evenOdd = object.value(QLatin1String("evenodd")).toBool();
    e.taper = object.value(QLatin1String("taper")).toBool();
    e.clip = object.value(QLatin1String("clip")).toBool();
    e.stack = object.value(QLatin1String("stack")).toBool();
    e.ink = object.value(QLatin1String("ink")).toBool();
    e.overlay = object.value(QLatin1String("overlay")).toBool();
    e.sheen = object.value(QLatin1String("sheen")).toBool();
    e.flame = object.value(QLatin1String("flame")).toBool();
    e.width = number(object, "width", 1) * float(scale);
    e.radius = number(object, "radius", 0) * float(scale);
    e.height = number(object, "height", 0.3f);
    e.bevel = number(object, "bevel", 0.15f);
    e.period = number(object, "period", 2.6f);
    e.feather = number(object, "feather", 0) * float(scale);
    e.metal = number(object, "metal", 0);
    e.rough = number(object, "rough", 0.5f);
    e.coat = number(object, "coat", 0);
    e.opacity = number(object, "opacity", 1);
    e.emissive = number(object, "emissive", 0);
    e.glowGain = number(object, "glowGain", 1);
    e.strength = number(object, "strength", 0.8f);
    e.lit = std::clamp(number(object, "lit", 1), 0.0f, 1.0f);
    e.gleam = std::clamp(number(object, "gleam", 1), 0.5f, 3.0f);
    e.own = std::clamp(number(object, "ownLight", 0), 0.0f, 1.0f);
    if (object.value(QLatin1String("lift")).isDouble()) e.lift = float(object.value(QLatin1String("lift")).toDouble());
    for (const QJsonValue &v : object.value(QLatin1String("dash")).toArray()) e.dash.push_back(v.toDouble() * scale);
    if (object.value(QLatin1String("glow")).isString()) e.glow = colourOf(object.value(QLatin1String("glow")).toString());

    const QJsonObject albedo = object.value(QLatin1String("albedo")).toObject();
    if (albedo.contains(QLatin1String("texture")))
    {
        e.texture = albedo.value(QLatin1String("texture")).toString();
        e.strip = albedo.value(QLatin1String("strip")).toBool();
        e.fit = albedo.value(QLatin1String("fit")).toBool();
        e.grain = albedo.value(QLatin1String("grain")).toBool();
        if (albedo.value(QLatin1String("tint")).isString()) e.tint = colourOf(albedo.value(QLatin1String("tint")).toString());
        e.relief = number(albedo, "relief", 0.4f);
        e.textureScale = number(albedo, "scale", 1);
        e.shift = number(albedo, "shift", 0) * float(scale);
    }
    else if (albedo.contains(QLatin1String("gradient")))
    {
        const QJsonArray stops = albedo.value(QLatin1String("gradient")).toArray();
        e.gradient = true;
        e.gradientInner = colourOf(stops.at(0).toString());
        e.gradientOuter = colourOf(stops.at(1).toString());
        e.gradientCentre = pointOf(albedo.value(QLatin1String("centre")), toPicture);
        e.gradientRadius = number(albedo, "radius", 1) * float(scale);
    }
    else
    {
        e.colour = colourOf(albedo.value(QLatin1String("colour")).toString(QStringLiteral("#808080")));
        e.alpha = number(albedo, "alpha", 1);
    }
    const QJsonObject lens = object.value(QLatin1String("lens")).toObject();
    if (!lens.isEmpty())
    {
        e.hasLens = true;
        e.lensColour = colourOf(lens.value(QLatin1String("colour")).toString());
        e.lensCentre = pointOf(lens.value(QLatin1String("centre")), toPicture);
        e.lensRadius = number(lens, "radius", 1) * float(scale);
    }
    if (object.value(QLatin1String("flameHue")).isDouble()) e.flameHue = float(object.value(QLatin1String("flameHue")).toDouble());
    const QJsonObject recolour = object.value(QLatin1String("recolour")).toObject();
    if (!recolour.isEmpty())
    {
        e.recolour = true;
        e.recolourHue = number(recolour, "hue", 0);
        e.recolourSaturation = number(recolour, "saturation", 0);
        e.recolourValue = number(recolour, "value", 0.5f);
    }
    const QJsonObject adjust = object.value(QLatin1String("adjust")).toObject();
    if (!adjust.isEmpty())
    {
        e.adjust = true;
        e.adjustHue = number(adjust, "hue", 0);
        e.adjustSaturation = number(adjust, "saturation", 1);
        e.adjustBrightness = number(adjust, "brightness", 1);
    }

    e.shape.setFillRule(e.evenOdd ? Qt::OddEvenFill : Qt::WindingFill);
    if (e.type == QLatin1String("fill"))
    {
        for (const QJsonValue &ringValue : object.value(QLatin1String("rings")).toArray())
        {
            const QJsonArray ring = ringValue.toArray();
            if (ring.size() < 3) continue;
            e.shape.moveTo(pointOf(ring.at(0), toPicture));
            for (qsizetype i = 1; i < ring.size(); ++i) e.shape.lineTo(pointOf(ring.at(i), toPicture));
            e.shape.closeSubpath();
        }
    }
    else if (e.type == QLatin1String("stroke"))
    {
        const QJsonArray points = object.value(QLatin1String("points")).toArray();
        for (const QJsonValue &p : points) e.points.push_back(pointOf(p, toPicture));
        if (!e.points.empty())
        {
            e.shape.moveTo(e.points.front());
            for (std::size_t i = 1; i < e.points.size(); ++i) e.shape.lineTo(e.points[i]);
            if (e.closed) e.shape.closeSubpath();
        }
    }
    else
    {
        e.centre = pointOf(object.value(QLatin1String("centre")), toPicture);
        e.shape.addEllipse(e.centre, e.radius, e.radius);
    }
    return e;
}

// The element painted with `brush` into a box `w` x `h` whose origin lies
// at `origin` in the pictures.
QImage drawInto(int w, int h, const Element &e, QPointF origin, const QBrush &brush)
{
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(-origin);
    if (e.type == QLatin1String("stroke"))
    {
        const Qt::PenCapStyle cap = e.cap == QLatin1String("butt") ? Qt::FlatCap : Qt::RoundCap;
        if (e.taper && e.points.size() > 1)
        {
            const std::size_t n = e.points.size();
            for (std::size_t k = 0; k + 1 < n; ++k)
            {
                const float width = e.width * (1 - 0.7f * float(k) / float(std::max<std::size_t>(1, n - 1)));
                painter.setPen(QPen(brush, std::max(0.4f, width), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawLine(e.points[k], e.points[k + 1]);
            }
        }
        else
        {
            QPen pen(brush, std::max(0.4f, e.width), Qt::SolidLine, cap, Qt::RoundJoin);
            if (!e.dash.empty())
            {
                QList<qreal> pattern;
                for (qreal v : e.dash) pattern.append(std::max<qreal>(0.1, v / std::max<qreal>(0.4, e.width)));
                pen.setDashPattern(pattern);
            }
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(e.shape);
        }
    }
    else
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(brush);
        painter.drawPath(e.shape);
    }
    return image;
}

// The outline a stroke covers, as a shape to fill.
QPainterPath strokeOutline(const Element &e)
{
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(0.4f, e.width));
    stroker.setCapStyle(e.cap == QLatin1String("butt") ? Qt::FlatCap : Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    if (!e.dash.empty())
    {
        QList<qreal> pattern;
        for (qreal v : e.dash) pattern.append(std::max<qreal>(0.1, v / std::max<qreal>(0.4, e.width)));
        stroker.setDashPattern(pattern);
    }
    return stroker.createStroke(e.shape);
}

// A material laid along the band, the way a label wraps round a tin: each
// piece of the band's path paints its stretch of the element with the
// picture turned to the path's direction there and moved on by the length
// travelled. A long picture (a nebula) runs along the band at 2.7
// half-widths high; an upright one (bark, wood, straw) is turned so its
// grain runs along. A closed band carries it round its first half and back,
// mirrored, round its second, so the ends meet. Screen tiling under it
// covers whatever the pieces leave (the outer side of a sharp corner).
QImage drawAlongBand(int w, int h, const Element &e, QPointF origin, const QImage &picture,
                     const BandPath &band, float hw, const QBrush &underneath)
{
    QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.translate(-origin);
    const QPainterPath shape = e.type == QLatin1String("stroke") ? strokeOutline(e) : e.shape;
    painter.setPen(Qt::NoPen);
    painter.fillPath(shape, underneath);
    const std::size_t count = band.points.size();
    if (count < 2 || picture.isNull()) return image;
    const std::size_t last = band.closed ? count : count - 1;
    const double total = band.length;
    const float k = e.strip ? 2.7f * hw / float(picture.height())
                            : 128.0f * (hw / 24.0f) * e.textureScale / float(picture.width());
    const QRectF reach = shape.boundingRect().adjusted(-hw, -hw, hw, hw);
    painter.setClipPath(shape);
    for (std::size_t i = 1; i <= last; ++i)
    {
        const QPointF a = band.points[i - 1], b = band.points[i % count];
        const double s0 = band.at[i - 1], s1 = band.at[i];
        const double len = s1 - s0;
        if (len < 0.01) continue;
        const QPointF dir = (b - a) / len;
        const QPointF nd(-dir.y(), dir.x());
        const double before = (!band.closed && i == 1) ? 8.0 * hw : 1.0;
        const double after = (!band.closed && i == last) ? 8.0 * hw : 1.0;
        const double across = 6.0 * hw;
        QPolygonF quad;
        quad << a - dir * before + nd * across << b + dir * after + nd * across
             << b + dir * after - nd * across << a - dir * before - nd * across;
        if (!reach.intersects(quad.boundingRect())) continue;
        const bool mirrored = band.closed && (s0 + len / 2) > total / 2;
        QTransform t;
        if (e.strip)
        {
            const double lift = 1.35 * hw;
            const double from = s0 + e.shift, back = total - s0 + e.shift;
            if (!mirrored)
                t = QTransform(k * dir.x(), k * dir.y(), k * nd.x(), k * nd.y(),
                               a.x() - from * dir.x() - lift * nd.x(), a.y() - from * dir.y() - lift * nd.y());
            else
                t = QTransform(-k * dir.x(), -k * dir.y(), k * nd.x(), k * nd.y(),
                               a.x() + back * dir.x() - lift * nd.x(), a.y() + back * dir.y() - lift * nd.y());
        }
        else if (!mirrored)
            t = QTransform(k * nd.x(), k * nd.y(), k * dir.x(), k * dir.y(), a.x() - s0 * dir.x(), a.y() - s0 * dir.y());
        else
            t = QTransform(k * nd.x(), k * nd.y(), -k * dir.x(), -k * dir.y(),
                           a.x() + (total - s0) * dir.x(), a.y() + (total - s0) * dir.y());
        QBrush brush(picture);
        brush.setTransform(t);
        // The shape's own edge was drawn smooth underneath; inside it each
        // piece of the band lays its stretch of the picture.
        painter.setBrush(brush);
        painter.drawPolygon(quad);
    }
    return image;
}

// A halo: the sum of Gaussian blurs of `source` at several radii. The
// narrow ones are blurred at full size; the wide ones together from one
// copy a quarter (or half) the size, scaled back up once (the usual way
// bloom is made), which looks the same at a fraction of the work.
Plane halo(const Plane &source, const std::vector<std::pair<float, float>> &radii)
{
    Plane result(source.width, source.height, source.channels, 0);
    const int w = source.width, h = source.height, c = source.channels;
    float narrowest = 1e9f;
    for (const auto &[sigma, weight] : radii)
    {
        if (sigma < 3.0f)
        {
            const Plane blurred = gaussian(source, sigma);
            for (std::size_t i = 0; i < result.data.size(); ++i) result.data[i] += blurred.data[i] * weight;
        }
        else narrowest = std::min(narrowest, sigma);
    }
    if (narrowest > 1e8f) return result;
    const int f = narrowest >= 6.0f ? 4 : 2;
    const int sw = (w + f - 1) / f, sh = (h + f - 1) / f;
    Plane small(sw, sh, c, 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int k = 0; k < c; ++k) small.at(x / f, y / f, k) += source.at(x, y, k);
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x)
        {
            const int cw = std::min(f, w - x * f), ch = std::min(f, h - y * f);
            const float share = 1.0f / float(std::max(1, cw * ch));
            for (int k = 0; k < c; ++k) small.at(x, y, k) *= share;
        }
    Plane wide(sw, sh, c, 0);
    for (const auto &[sigma, weight] : radii)
    {
        if (sigma < 3.0f) continue;
        const Plane blurred = gaussian(small, sigma / float(f));
        for (std::size_t i = 0; i < wide.data.size(); ++i) wide.data[i] += blurred.data[i] * weight;
    }
    for (int y = 0; y < h; ++y)
    {
        const float fy = std::clamp((y + 0.5f) / f - 0.5f, 0.0f, float(sh - 1));
        const int y0 = int(fy), y1 = std::min(sh - 1, y0 + 1);
        const float ty = fy - y0;
        for (int x = 0; x < w; ++x)
        {
            const float fx = std::clamp((x + 0.5f) / f - 0.5f, 0.0f, float(sw - 1));
            const int x0 = int(fx), x1 = std::min(sw - 1, x0 + 1);
            const float tx = fx - x0;
            for (int k = 0; k < c; ++k)
            {
                const float top = wide.at(x0, y0, k) * (1 - tx) + wide.at(x1, y0, k) * tx;
                const float bottom = wide.at(x0, y1, k) * (1 - tx) + wide.at(x1, y1, k) * tx;
                result.at(x, y, k) += top * (1 - ty) + bottom * ty;
            }
        }
    }
    return result;
}

// A smooth drifting field for the thin-film sheens (chameleon, iridescent):
// three octaves of blurred noise, the same every time.
Plane filmNoise(int w, int h, float scale)
{
    Plane field(w, h, 1, 0);
    std::mt19937 random(29);
    std::normal_distribution<float> normal(0.0f, 1.0f);
    const std::array<std::pair<float, float>, 3> octaves{{{0.6f, scale}, {0.28f, scale / 2}, {0.12f, scale / 4}}};
    for (const auto &[weight, sigma] : octaves)
    {
        Plane noise(w, h, 1, 0);
        for (float &v : noise.data) v = normal(random);
        noise = gaussian(noise, std::max(1.0f, sigma));
        double mean = 0, square = 0;
        for (float v : noise.data) { mean += v; square += double(v) * v; }
        mean /= std::max<std::size_t>(1, noise.data.size());
        const double deviation = std::sqrt(std::max(1e-12, square / std::max<std::size_t>(1, noise.data.size()) - mean * mean));
        for (std::size_t i = 0; i < field.data.size(); ++i)
            field.data[i] += weight * float((noise.data[i] - mean) / deviation * 0.18 + 0.5);
    }
    return field;
}

// The light of one group of lights, gathered while the elements are laid.
struct LightGroup
{
    Plane emission, glow, halo;
    bool used = false;
    // The pixels this group's light reaches, before its halo spreads.
    int left = 1 << 30, top = 1 << 30, right = -1, bottom = -1;
    void touch(int x, int y)
    {
        left = std::min(left, x); top = std::min(top, y);
        right = std::max(right, x); bottom = std::max(bottom, y);
    }
};

// A part of a plane, and the same part written back.
Plane cropped(const Plane &source, QRect area)
{
    Plane result(area.width(), area.height(), source.channels, 0);
    for (int y = 0; y < area.height(); ++y)
        std::copy_n(&source.data[(std::size_t(area.y() + y) * source.width + area.x()) * source.channels],
                    std::size_t(area.width()) * source.channels,
                    &result.data[std::size_t(y) * area.width() * source.channels]);
    return result;
}

// A picture seen through a window (an edge panel's own): what lies outside
// it is cleared, and within `fade` pixels of its edge it fades out, so a
// halo or a shadow ends softly where the panel ends. `at` is where the
// picture lies in the whole, `window` the window in the same pixels.
void fadeToWindow(QImage &image, QPoint at, const QRectF &window, float fade)
{
    const float soft = std::max(1.0f, fade);
    for (int y = 0; y < image.height(); ++y)
    {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        const float py = float(at.y() + y) + 0.5f;
        const float dy = std::min(py - float(window.top()), float(window.bottom()) - py);
        for (int x = 0; x < image.width(); ++x)
        {
            const float px = float(at.x() + x) + 0.5f;
            const float inside = std::min(dy, std::min(px - float(window.left()), float(window.right()) - px));
            if (inside >= soft) continue;
            const float u = clamp01(inside / soft);
            const float keep = u * u * (3 - 2 * u);
            const QRgb p = line[x];
            line[x] = qRgba(int(qRed(p) * keep + 0.5f), int(qGreen(p) * keep + 0.5f), int(qBlue(p) * keep + 0.5f),
                            int(qAlpha(p) * keep + 0.5f));
        }
    }
}


// A decoration's elements stacked into one field, before any light: what
// covers each pixel, its colour, its height and finish, and each group's
// light; in the pictures' pixels.
struct Field
{
    QRect bounds;
    double scale = 1;
    int W = 0, H = 0;
    float hw = 0;
    QTransform toPicture;
    BandPath band;
    Plane A, RGB, HEIGHT, METAL, ROUGH, COAT, GLEAM, INK, BODY, LIGHT, LINE, OWN;
    QHash<QString, LightGroup> groups;
};

// Draws the scene's elements, each into its own box (its coverage, its
// albedo - a colour, a gradient or a real material laid along the band -
// its relief and the height its shape gives it from its distance to its own
// edge), and stacks them into `field`. Says why when there is nothing to
// light: "empty-scene", or "cancelled" when `cancelled` says the pictures
// are no longer wanted.
QString buildField(Field &field, const QJsonObject &scene, const QString &materialDirectory, qreal requestedScale,
                   qsizetype maximumPixels, const std::function<bool()> &cancelled)
{
    const QJsonObject boundsObject = scene.value(QLatin1String("bounds")).toObject();
    const QRect bounds(int(std::floor(boundsObject.value(QLatin1String("x")).toDouble())),
                       int(std::floor(boundsObject.value(QLatin1String("y")).toDouble())),
                       int(std::ceil(boundsObject.value(QLatin1String("width")).toDouble())),
                       int(std::ceil(boundsObject.value(QLatin1String("height")).toDouble())));
    field.bounds = bounds;
    if (bounds.width() <= 0 || bounds.height() <= 0) return QStringLiteral("empty-scene");
    double scale = std::max(0.25, double(requestedScale));
    const double pixels = double(bounds.width()) * bounds.height() * scale * scale;
    if (pixels > double(maximumPixels)) scale *= std::sqrt(double(maximumPixels) / pixels);
    field.scale = scale;
    const int W = std::max(1, int(std::ceil(bounds.width() * scale)));
    const int H = std::max(1, int(std::ceil(bounds.height() * scale)));
    const QTransform toPicture = QTransform::fromScale(scale, scale) * QTransform::fromTranslate(-bounds.x() * scale, -bounds.y() * scale);
    const float hw = float(scene.value(QLatin1String("hw")).toDouble(24) * scale);

    BandPath band;
    {
        const QJsonObject pathObject = scene.value(QLatin1String("path")).toObject();
        band.closed = pathObject.value(QLatin1String("closed")).toBool();
        for (const QJsonValue &p : pathObject.value(QLatin1String("points")).toArray()) band.points.push_back(pointOf(p, toPicture));
        band.at.push_back(0);
        const std::size_t count = band.points.size();
        const std::size_t last = band.closed ? count : (count ? count - 1 : 0);
        for (std::size_t i = 1; i <= last && count > 1; ++i)
        {
            const QPointF d = band.points[i % count] - band.points[i - 1];
            band.at.push_back(band.at.back() + std::hypot(d.x(), d.y()));
        }
        band.length = band.at.back();
        if (count)
        {
            QPolygonF polygon;
            for (const QPointF &p : band.points) polygon << p;
            band.box = polygon.boundingRect();
            QPointF sum;
            for (const QPointF &p : band.points) sum += p;
            band.centre = sum / double(count);
            band.horizontal = band.box.width() >= band.box.height();
        }
    }

    Plane A(W, H), RGB(W, H, 3), HEIGHT(W, H), METAL(W, H), ROUGH(W, H, 1, 0.5f), COAT(W, H), GLEAM(W, H, 1, 1.0f), OWN(W, H);
    Plane INK(W, H, 3), BODY(W, H), LIGHT(W, H), LINE(W, H);
    QHash<QString, LightGroup> groups;
    auto groupFor = [&](const QString &name) -> LightGroup & {
        LightGroup &g = groups[name];
        if (!g.used)
        {
            g.used = true;
            g.emission = Plane(W, H, 3);
            g.glow = Plane(W, H, 3);
        }
        return g;
    };
    groupFor(QString());
    std::optional<Plane> film;
    const int pad = int(hw * 0.25f) + 2;

    for (const QJsonValue &value : scene.value(QLatin1String("elements")).toArray())
    {
        if (cancelled && cancelled()) return QStringLiteral("cancelled");
        Element e = readElement(value.toObject(), toPicture, scale);
        QRectF area = e.shape.boundingRect();
        if (e.type == QLatin1String("stroke")) area.adjust(-e.width, -e.width, e.width, e.width);
        area.adjust(-pad, -pad, pad, pad);
        const int x0 = std::max(0, int(std::floor(area.left()))), y0 = std::max(0, int(std::floor(area.top())));
        const int x1 = std::min(W, int(std::ceil(area.right()))), y1 = std::min(H, int(std::ceil(area.bottom())));
        if (x1 - x0 < 1 || y1 - y0 < 1) continue;
        const int w = x1 - x0, h = y1 - y0;
        const QPointF origin(x0, y0);
        const std::size_t n = std::size_t(w) * h;

        // Coverage.
        const QImage coverImage = drawInto(w, h, e, origin, QBrush(Qt::white));
        std::vector<float> cover(n);
        for (int y = 0; y < h; ++y)
        {
            const QRgb *line = reinterpret_cast<const QRgb *>(coverImage.constScanLine(y));
            for (int x = 0; x < w; ++x) cover[std::size_t(y) * w + x] = float(qAlpha(line[x])) / 255.0f;
        }
        std::vector<uint8_t> inside(n);
        for (std::size_t i = 0; i < n; ++i) inside[i] = cover[i] > 0.5f;
        if (e.feather > 0)
        {
            // A soft outline: the coverage fades over `feather` inside the
            // edge, and the element's inside is what stays more than half.
            const std::vector<float> d = distanceInside(inside, w, h);
            for (std::size_t i = 0; i < n; ++i)
            {
                cover[i] *= smoothstep01(d[i] / e.feather);
                inside[i] = cover[i] > 0.5f;
            }
        }
        std::vector<float> a(n);
        for (std::size_t i = 0; i < n; ++i) a[i] = cover[i] * e.opacity * e.alpha;
        if (e.clip)
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) a[std::size_t(y) * w + x] *= BODY.at(x0 + x, y0 + y);
        if (e.role == QLatin1String("body"))
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    BODY.at(x0 + x, y0 + y) = std::max(BODY.at(x0 + x, y0 + y), cover[std::size_t(y) * w + x]);
        if (*std::max_element(a.begin(), a.end()) <= 0.002f) continue;

        // Albedo, and the relief a material brings.
        std::vector<Rgb> colour(n, e.colour);
        std::vector<float> relief;
        float reliefScale = 0;
        if (!e.texture.isEmpty())
        {
            const QImage picture = materialPicture(materialDirectory, e.texture, false);
            const QImage heights = materialPicture(materialDirectory, e.texture, true);
            const bool alongBand = e.strip || (e.grain && e.role != QLatin1String("part"));
            auto tiled = [&](const QImage &source) {
                QBrush brush(source);
                QTransform t;
                if (e.fit)
                {
                    t.translate(x0, y0);
                    t.scale(double(w) / std::max(1, source.width()), double(h) / std::max(1, source.height()));
                }
                else
                {
                    const double size = 128.0 * (hw / 24.0) * e.textureScale;
                    if (e.grain && band.horizontal) t.rotate(90);
                    t.scale(size / std::max(1, source.width()), size / std::max(1, source.width()));
                }
                brush.setTransform(t);
                return brush;
            };
            const QImage colourImage = alongBand ? drawAlongBand(w, h, e, origin, picture, band, hw, tiled(picture))
                                                 : drawInto(w, h, e, origin, tiled(picture));
            for (int y = 0; y < h; ++y)
            {
                const QRgb *line = reinterpret_cast<const QRgb *>(colourImage.constScanLine(y));
                for (int x = 0; x < w; ++x)
                {
                    const float al = std::max(1e-4f, float(qAlpha(line[x])) / 255.0f);
                    Rgb c{float(qRed(line[x])) / 255.0f / al, float(qGreen(line[x])) / 255.0f / al,
                          float(qBlue(line[x])) / 255.0f / al};
                    if (e.tint) for (int k = 0; k < 3; ++k) c[k] *= (*e.tint)[k];
                    colour[std::size_t(y) * w + x] = c;
                }
            }
            if (!heights.isNull())
            {
                const QImage reliefImage = alongBand ? drawAlongBand(w, h, e, origin, heights, band, hw, tiled(heights))
                                                     : drawInto(w, h, e, origin, tiled(heights));
                relief.resize(n);
                for (int y = 0; y < h; ++y)
                {
                    const QRgb *line = reinterpret_cast<const QRgb *>(reliefImage.constScanLine(y));
                    for (int x = 0; x < w; ++x)
                    {
                        const float al = std::max(1e-4f, float(qAlpha(line[x])) / 255.0f);
                        relief[std::size_t(y) * w + x] = float(qRed(line[x])) / 255.0f / al;
                    }
                }
                reliefScale = e.relief * hw * 0.09f;
            }
        }
        else if (e.gradient)
        {
            QRadialGradient gradient(e.gradientCentre, e.gradientRadius);
            gradient.setColorAt(0, QColor::fromRgbF(e.gradientInner[0], e.gradientInner[1], e.gradientInner[2]));
            gradient.setColorAt(1, QColor::fromRgbF(e.gradientOuter[0], e.gradientOuter[1], e.gradientOuter[2]));
            const QImage image = drawInto(w, h, e, origin, QBrush(gradient));
            for (int y = 0; y < h; ++y)
            {
                const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
                for (int x = 0; x < w; ++x)
                {
                    const float al = std::max(1e-4f, float(qAlpha(line[x])) / 255.0f);
                    colour[std::size_t(y) * w + x] = {float(qRed(line[x])) / 255.0f / al, float(qGreen(line[x])) / 255.0f / al,
                                                      float(qBlue(line[x])) / 255.0f / al};
                }
            }
        }

        // The viewer's colours: a material moved to the chosen colour (its
        // hue, and its strength and brightness on average, each pixel keeping
        // how far it lies from the average, the material's own grain); a
        // picture's hue, strength and brightness.
        if (e.recolour)
        {
            double meanS = 0, meanV = 0;
            int inner = 0;
            std::vector<Rgb> hsv(n);
            for (std::size_t i = 0; i < n; ++i)
            {
                hsv[i] = rgbToHsv(colour[i]);
                if (inside[i]) { meanS += hsv[i][1]; meanV += hsv[i][2]; ++inner; }
            }
            if (inner > 0)
            {
                const float ms = std::max(0.05f, float(meanS / inner)), mv = std::max(0.05f, float(meanV / inner));
                for (std::size_t i = 0; i < n; ++i)
                    colour[i] = hsvToRgb({e.recolourHue / 360.0f, clamp01(hsv[i][1] / ms * std::max(0.02f, e.recolourSaturation)),
                                          clamp01(hsv[i][2] / mv * std::max(0.04f, e.recolourValue))});
            }
        }
        if (e.adjust)
            for (std::size_t i = 0; i < n; ++i)
            {
                Rgb hsv = rgbToHsv(colour[i]);
                colour[i] = hsvToRgb({hsv[0] + e.adjustHue / 360.0f, clamp01(hsv[1] * e.adjustSaturation),
                                      clamp01(hsv[2] * e.adjustBrightness)});
            }
        // The finishes: the app's chameleon (a faint thin-film sheen
        // drifting through purple, blue, gold and green), iridescent (the
        // stronger film), rainbow along the band, and metallic rainbow
        // (rainbow-coloured polished metal).
        if (!e.finish.isEmpty())
        {
            if ((e.finish == QLatin1String("chameleon") || e.finish == QLatin1String("iridescent")) && !film)
                film = filmNoise(W, H, hw * 3);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const std::size_t i = std::size_t(y) * w + x;
                    const float X = float(x0 + x), Y = float(y0 + y);
                    const float light = clamp01(colour[i][0] * 0.3f + colour[i][1] * 0.59f + colour[i][2] * 0.11f);
                    if (e.finish == QLatin1String("chameleon") || e.finish == QLatin1String("iridescent"))
                    {
                        const float hue = 0.7f + 0.75f * film->at(x0 + x, y0 + y)
                                          + 0.05f * std::sin(float(2 * M_PI) * (X / W + Y / H));
                        const Rgb f = spectrum(hue);
                        const float share = e.finish == QLatin1String("chameleon") ? 0.08f + 0.12f * light : 0.3f + 0.25f * light;
                        for (int k = 0; k < 3; ++k) colour[i][k] = colour[i][k] * (1 - share) + f[k] * light * share * 1.6f;
                    }
                    else
                    {
                        const float along = band.along(X, Y);
                        const Rgb rainbow = spectrum(along * 1.1f + 0.06f * std::sin(float(2 * M_PI) * along));
                        const float level = clamp01(light * 1.25f);
                        if (e.finish == QLatin1String("metallic-rainbow"))
                            for (int k = 0; k < 3; ++k) colour[i][k] = rainbow[k] * (0.55f + 0.45f * level);
                        else
                        {
                            const float pale = clamp01((light - 0.8f) / 0.2f) * 0.5f;
                            for (int k = 0; k < 3; ++k) colour[i][k] = (rainbow[k] + (1 - rainbow[k]) * pale) * level;
                        }
                    }
                }
            if (e.finish == QLatin1String("metallic-rainbow"))
            {
                e.metal = 1;
                e.rough = std::min(0.2f, e.rough);
            }
        }
        std::vector<Rgb> rainbowLight;
        if (e.lightFinish == QLatin1String("rainbow"))
        {
            rainbowLight.resize(n);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const std::size_t i = std::size_t(y) * w + x;
                    rainbowLight[i] = spectrum(band.along(float(x0 + x), float(y0 + y)) * 1.1f);
                    const float lum = clamp01(colour[i][0] * 0.3f + colour[i][1] * 0.59f + colour[i][2] * 0.11f);
                    const float pale = clamp01((lum - 0.85f) / 0.15f) * 0.7f;
                    for (int k = 0; k < 3; ++k) colour[i][k] = rainbowLight[i][k] * (1 - pale) + pale;
                }
        }
        // The lens: deep glass round a hot core; one resting dim (its `lit`
        // below 1) is darker glass with a fainter core, lighting up when the
        // pointer is over it.
        if (e.hasLens)
        {
            const Rgb base = e.lightFinish == QLatin1String("rainbow")
                                 ? spectrum(band.along(float(e.lensCentre.x()), float(e.lensCentre.y())) * 1.1f)
                                 : e.lensColour;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float r = float(std::hypot(x0 + x - e.lensCentre.x() + e.lensRadius * 0.12,
                                                     y0 + y - e.lensCentre.y() + e.lensRadius * 0.14))
                                    / std::max(1.0f, e.lensRadius);
                    const float core = std::pow(clamp01(1 - r / 0.5f), 1.5f) * e.lit;
                    const float body = (0.18f + 0.75f * clamp01(1.1f - r)) * (0.5f + 0.5f * e.lit);
                    for (int k = 0; k < 3; ++k) colour[std::size_t(y) * w + x][k] = base[k] * body + core * (1 - base[k]) * 0.55f;
                }
            e.rough = 0.05f;
            e.metal = 0;
            e.coat = 1;
            e.emissive = 0.75f;
        }
        // A candle's flame: white at its foot, gold, orange, fading at its tip.
        if (e.flame)
        {
            int top = -1, bottom = -1;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (cover[std::size_t(y) * w + x] > 0.05f) { if (top < 0) top = y; bottom = y; break; }
            if (top >= 0)
                for (int y = 0; y < h; ++y)
                {
                    const float u = clamp01(float(y - top) / float(std::max(1, bottom - top)));
                    Rgb hot{1.0f, 0.55f + 0.45f * u, 0.15f + 0.85f * u * u * u};
                    if (e.flameHue)
                    {
                        Rgb hsv = rgbToHsv(hot);
                        hsv[0] = *e.flameHue / 360.0f;
                        hot = hsvToRgb(hsv);
                    }
                    for (int x = 0; x < w; ++x)
                    {
                        const std::size_t i = std::size_t(y) * w + x;
                        colour[i] = hot;
                        a[i] *= clamp01(0.35f + 0.65f * u * 1.3f);
                    }
                }
        }

        // The height its shape gives it.
        std::vector<float> profile(n, 0.0f);
        if (e.profile != QLatin1String("flat"))
        {
            std::vector<float> d = distanceInside(inside, w, h);
            float dmax = 0;
            for (std::size_t i = 0; i < n; ++i)
            {
                d[i] = std::max(0.0f, d[i] + cover[i] - 0.5f);
                if (!inside[i]) d[i] = 0;
                dmax = std::max(dmax, d[i]);
            }
            const float H0 = e.height * hw;
            if (e.profile == QLatin1String("cylinder") || e.profile == QLatin1String("dome") || e.profile == QLatin1String("tube"))
            {
                const int reach = std::max(3, int(std::min(4.0f * hw, 2.0f * dmax + 3.0f)));
                const std::vector<float> R = maximumFilter(d, w, h, reach);
                for (std::size_t i = 0; i < n; ++i)
                {
                    if (!inside[i]) continue;
                    const float r = std::max(R[i], 0.5f), u = clamp01(d[i] / r), top = std::min(H0, r * 1.15f);
                    profile[i] = top * std::sqrt(clamp01(1 - (1 - u) * (1 - u)));
                }
            }
            else if (e.profile == QLatin1String("bevel"))
            {
                const float b = std::max(0.8f, e.bevel * hw);
                for (std::size_t i = 0; i < n; ++i) if (inside[i]) profile[i] = H0 * smoothstep01(d[i] / b);
            }
            else if (e.profile == QLatin1String("folds"))
            {
                const float period = std::max(4.0f, e.period * hw), b = std::max(0.8f, e.bevel * hw);
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                    {
                        const std::size_t i = std::size_t(y) * w + x;
                        if (!inside[i]) continue;
                        const float along = float(band.horizontal ? x0 + x : y0 + y);
                        const float tri = std::abs(std::fmod(along / period, 1.0f) * 2 - 1);
                        profile[i] = ((tri - 0.5f) * H0 + 0.5f * H0) * smoothstep01(d[i] / b);
                    }
            }
            else if (e.profile == QLatin1String("groove"))
            {
                const float R = std::max(0.8f, dmax);
                for (std::size_t i = 0; i < n; ++i)
                    if (inside[i])
                    {
                        const float u = clamp01(d[i] / R);
                        profile[i] = -H0 * std::sqrt(clamp01(1 - (1 - u) * (1 - u)));
                    }
            }
        }
        if (e.profile == QLatin1String("groove")) for (Rgb &c : colour) for (float &v : c) v *= 0.85f;

        // Stack it into the field.
        if (e.ink)
        {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float aa = a[std::size_t(y) * w + x];
                    for (int k = 0; k < 3; ++k)
                        INK.at(x0 + x, y0 + y, k) = INK.at(x0 + x, y0 + y, k) * (1 - aa) + (1 - e.colour[k]) * aa * e.strength;
                }
            continue;
        }
        if (e.overlay)
        {
            const bool black = e.colour[0] == 0 && e.colour[1] == 0 && e.colour[2] == 0;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const std::size_t i = std::size_t(y) * w + x;
                    const float aa = a[i];
                    for (int k = 0; k < 3; ++k)
                    {
                        float &v = RGB.at(x0 + x, y0 + y, k);
                        v = black ? v * (1 - aa * 0.6f) : v * (1 - aa) + (v * 0.5f + e.colour[k] * 0.5f) * aa;
                    }
                    HEIGHT.at(x0 + x, y0 + y) += profile[i] * aa;
                }
            continue;
        }
        LightGroup &own = groupFor(e.group);
        std::vector<Plane *> emissions;
        for (auto g = groups.begin(); g != groups.end(); ++g) emissions.push_back(&g->emission);
        if (e.sheen)
        {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const std::size_t i = std::size_t(y) * w + x;
                    for (int k = 0; k < 3; ++k) groupFor(QString()).emission.at(x0 + x, y0 + y, k) += colour[i][k] * a[i] * 0.6f;
                    groupFor(QString()).touch(x0 + x, y0 + y);
                }
            continue;
        }
        const bool lightElement = e.emissive > 0.5f && !e.hasLens && e.coat <= 0;
        const bool lineElement = e.emissive > 0.5f && e.type == QLatin1String("stroke");
        const float liftHeight = e.lift ? *e.lift * hw : 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const std::size_t i = std::size_t(y) * w + x;
                const float aa = a[i];
                if (aa <= 0) continue;
                const int X = x0 + x, Y = y0 + y;
                const float base = e.lift ? liftHeight : e.stack ? HEIGHT.at(X, Y) : 0.0f;
                const float he = base + profile[i] + (relief.empty() ? 0.0f : relief[i] * reliefScale);
                HEIGHT.at(X, Y) = HEIGHT.at(X, Y) * (1 - aa) + he * aa;
                for (int k = 0; k < 3; ++k) RGB.at(X, Y, k) = RGB.at(X, Y, k) * (1 - aa) + colour[i][k] * aa;
                METAL.at(X, Y) = METAL.at(X, Y) * (1 - aa) + e.metal * aa;
                ROUGH.at(X, Y) = ROUGH.at(X, Y) * (1 - aa) + e.rough * aa;
                COAT.at(X, Y) = COAT.at(X, Y) * (1 - aa) + e.coat * aa;
                GLEAM.at(X, Y) = GLEAM.at(X, Y) * (1 - aa) + e.gleam * aa;
                OWN.at(X, Y) = OWN.at(X, Y) * (1 - aa) + e.own * aa;
                // The element covers every group's light under it, and gives
                // its own.
                const std::size_t at = (std::size_t(Y) * W + X) * 3;
                for (Plane *emission : emissions)
                {
                    float *v = emission->data.data() + at;
                    v[0] *= 1 - aa;
                    v[1] *= 1 - aa;
                    v[2] *= 1 - aa;
                }
                for (int k = 0; k < 3; ++k) own.emission.at(X, Y, k) += colour[i][k] * e.emissive * e.lit * aa;
                if (e.emissive > 0 || e.glow) own.touch(X, Y);
                LIGHT.at(X, Y) = lightElement ? std::max(LIGHT.at(X, Y) * (1 - aa), aa) : LIGHT.at(X, Y) * (1 - aa);
                LINE.at(X, Y) = lineElement ? std::max(LINE.at(X, Y) * (1 - aa), aa) : LINE.at(X, Y) * (1 - aa);
                if (e.glow)
                {
                    const Rgb g = rainbowLight.empty() ? *e.glow : rainbowLight[i];
                    const float strength = (e.emissive > 0 ? 1.0f : 0.7f) * std::min(e.glowGain, 1.6f) * e.lit;
                    for (int k = 0; k < 3; ++k) own.glow.at(X, Y, k) += g[k] * aa * strength;
                    if (e.glowGain > 1.6f)
                    {
                        if (own.halo.data.empty()) own.halo = Plane(W, H, 3);
                        for (int k = 0; k < 3; ++k) own.halo.at(X, Y, k) += g[k] * aa * e.glowGain * e.lit;
                    }
                }
                A.at(X, Y) = A.at(X, Y) + aa * (1 - A.at(X, Y));
            }
    }

    field.W = W;
    field.H = H;
    field.hw = hw;
    field.toPicture = toPicture;
    field.band = std::move(band);
    field.A = std::move(A);
    field.RGB = std::move(RGB);
    field.HEIGHT = std::move(HEIGHT);
    field.METAL = std::move(METAL);
    field.ROUGH = std::move(ROUGH);
    field.COAT = std::move(COAT);
    field.GLEAM = std::move(GLEAM);
    field.INK = std::move(INK);
    field.BODY = std::move(BODY);
    field.LIGHT = std::move(LIGHT);
    field.LINE = std::move(LINE);
    field.OWN = std::move(OWN);
    field.groups = std::move(groups);
    return QString();
}

// A solid's outline made smooth where it is finely notched (a row of studs
// or ribs along an edge, which would stand as fins down a 3D wall): the
// notches narrower than 2 * radius are closed (the coverage grown by
// `radius` and taken back, by exact distances), and round about them every
// map is evened out from the covered pixels near it - a blur weighted by the
// coverage, normalized convolution (Knutsson and Westin, "Normalized and
// Differential Convolution", 1993) - so the edge there is one smooth
// surface. Wider openings and the rest of the decoration stay as drawn.
void smoothNotches(Field &f, float radius)
{
    const int W = f.W, H = f.H;
    const std::size_t n = std::size_t(W) * H;
    std::vector<uint8_t> outside(n), grown(n), filled(n), elsewhere(n);
    for (std::size_t i = 0; i < n; ++i) outside[i] = f.A.data[i] < 0.5f;
    const std::vector<float> reach = distanceInside(outside, W, H);
    for (std::size_t i = 0; i < n; ++i) grown[i] = !outside[i] || reach[i] <= radius;
    const std::vector<float> depth = distanceInside(grown, W, H);
    bool any = false;
    for (std::size_t i = 0; i < n; ++i)
    {
        filled[i] = outside[i] && depth[i] > radius;
        any = any || filled[i];
    }
    if (!any) return;
    for (std::size_t i = 0; i < n; ++i) elsewhere[i] = !filled[i];
    // How far each pixel lies from the nearest notch.
    const std::vector<float> nearest = distanceInside(elsewhere, W, H);
    const float zone = 2 * radius;
    // The maps weighted by the coverage, the weights, and the closed outline.
    constexpr int maps = 13;
    Plane weighted(W, H, maps), weight(W, H), closed(W, H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const std::size_t i = std::size_t(y) * W + x;
            const float c = f.A.data[i];
            weight.at(x, y) = c;
            closed.at(x, y) = !outside[i] || filled[i] ? 1.0f : 0.0f;
            const float values[maps] = {f.RGB.at(x, y, 0), f.RGB.at(x, y, 1), f.RGB.at(x, y, 2),
                                        f.INK.at(x, y, 0), f.INK.at(x, y, 1), f.INK.at(x, y, 2),
                                        f.METAL.at(x, y), f.ROUGH.at(x, y), f.COAT.at(x, y), f.GLEAM.at(x, y),
                                        f.HEIGHT.at(x, y), f.LIGHT.at(x, y), f.OWN.at(x, y)};
            for (int k = 0; k < maps; ++k) weighted.at(x, y, k) = values[k] * c;
        }
    const float sigma = std::max(1.0f, radius * 0.6f);
    const Plane spread = gaussian(weighted, sigma), spreadWeight = gaussian(weight, sigma), outline = gaussian(closed, sigma);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const std::size_t i = std::size_t(y) * W + x;
            if (nearest[i] > zone) continue;
            const float w = spreadWeight.at(x, y);
            if (w > 1e-4f)
            {
                float v[maps];
                for (int k = 0; k < maps; ++k) v[k] = spread.at(x, y, k) / w;
                for (int k = 0; k < 3; ++k)
                {
                    f.RGB.at(x, y, k) = v[k];
                    f.INK.at(x, y, k) = v[3 + k];
                }
                f.METAL.at(x, y) = v[6];
                f.ROUGH.at(x, y) = v[7];
                f.COAT.at(x, y) = v[8];
                f.GLEAM.at(x, y) = v[9];
                f.HEIGHT.at(x, y) = v[10];
                f.LIGHT.at(x, y) = v[11];
                f.OWN.at(x, y) = v[12];
            }
            f.A.at(x, y) = outline.at(x, y);
        }
}

} // namespace

DecorationBake bakeDecoration(const QJsonObject &scene, const QString &materialDirectory, qreal requestedScale,
                              qsizetype maximumPixels, const std::function<bool()> &cancelled)
{
    DecorationBake bake;
    Field field;
    const QString failed = buildField(field, scene, materialDirectory, requestedScale, maximumPixels, cancelled);
    bake.bounds = field.bounds;
    bake.scale = field.scale;
    if (!failed.isEmpty())
    {
        bake.error = failed;
        return bake;
    }
    const auto stop = [&]() {
        if (!cancelled || !cancelled()) return false;
        bake.layers.clear();
        bake.rects.clear();
        bake.error = QStringLiteral("cancelled");
        return true;
    };
    const int W = field.W, H = field.H;
    const double scale = field.scale;
    const float hw = field.hw;
    const QTransform &toPicture = field.toPicture;
    const Plane &A = field.A, &RGB = field.RGB, &HEIGHT = field.HEIGHT, &METAL = field.METAL, &ROUGH = field.ROUGH,
                &COAT = field.COAT, &GLEAM = field.GLEAM, &INK = field.INK, &LIGHT = field.LIGHT, &LINE = field.LINE;
    QHash<QString, LightGroup> &groups = field.groups;

    // Lighting.
    if (stop()) return bake;
    const Plane smooth = gaussian(HEIGHT, 0.7f * 1.0f);
    std::vector<float> nx(std::size_t(W) * H), ny(nx.size()), nz(nx.size());
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const float gx = W > 1 ? (x == 0 ? smooth.at(1, y) - smooth.at(0, y)
                                     : x == W - 1 ? smooth.at(W - 1, y) - smooth.at(W - 2, y)
                                                  : (smooth.at(x + 1, y) - smooth.at(x - 1, y)) * 0.5f) : 0;
            const float gy = H > 1 ? (y == 0 ? smooth.at(x, 1) - smooth.at(x, 0)
                                     : y == H - 1 ? smooth.at(x, H - 1) - smooth.at(x, H - 2)
                                                  : (smooth.at(x, y + 1) - smooth.at(x, y - 1)) * 0.5f) : 0;
            const float len = std::sqrt(gx * gx + gy * gy + 1);
            const std::size_t i = std::size_t(y) * W + x;
            nx[i] = -gx / len;
            ny[i] = -gy / len;
            nz[i] = 1 / len;
        }
    float L[3] = {-0.5f, -0.62f, 0.6f};
    {
        const float l = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
        for (float &v : L) v /= l;
    }
    float Hv[3] = {L[0], L[1], L[2] + 1};
    {
        const float l = std::sqrt(Hv[0] * Hv[0] + Hv[1] * Hv[1] + Hv[2] * Hv[2]);
        for (float &v : Hv) v /= l;
    }
    // The shadows the relief casts away from the light.
    std::vector<float> shadow(nx.size(), 1.0f);
    {
        const float step = std::max(1.0f, hw / 16.0f);
        const float planar = std::hypot(L[0], L[1]);
        const float rise = L[2] / planar, dirx = L[0] / planar, diry = L[1] / planar;
        const float soft = std::max(1.0f, hw * 0.06f);
        for (int k = 1; k < 25; ++k)
        {
            const int sx = int(std::lround(dirx * k * step)), sy = int(std::lround(diry * k * step));
            const float drop = k * step * rise;
            for (int y = 0; y < H; ++y)
            {
                const int yy = std::clamp(y + sy, 0, H - 1);
                for (int x = 0; x < W; ++x)
                {
                    const int xx = std::clamp(x + sx, 0, W - 1);
                    const float block = (HEIGHT.at(xx, yy) - HEIGHT.at(x, y) - drop) / soft;
                    float &s = shadow[std::size_t(y) * W + x];
                    s = std::min(s, 1 - clamp01(block) * 0.75f);
                }
            }
        }
    }
    const Plane hollow = gaussian(HEIGHT, hw * 0.12f);
    // Every group's light and halo, as the lights layers will show them.
    QHash<QString, Plane> heldByGroup;
    for (auto g = groups.begin(); g != groups.end(); ++g)
    {
        Plane held(W, H, 3);
        if (g->right < 0) { heldByGroup.insert(g.key(), held); continue; }
        for (int y = g->top; y <= g->bottom; ++y)
            for (int x = g->left; x <= g->right; ++x)
            {
                float E[3];
                for (int k = 0; k < 3; ++k) E[k] = g->emission.at(x, y, k) * 1.4f;
                const float peak = std::max({E[0], E[1], E[2]});
                const float hold = 1.0f / std::max(peak, 1.0f);
                for (int k = 0; k < 3; ++k)
                {
                    held.at(x, y, k) = E[k] * hold;
                    // Light keeps its colour: what its colour cannot show
                    // becomes halo.
                    g->glow.at(x, y, k) += E[k] * hold * std::clamp(peak - 1.0f, 0.0f, 2.0f);
                }
            }
        heldByGroup.insert(g.key(), held);
    }
    // The cast shadow on the desktop below.
    Plane shifted(W, H);
    {
        const int dy = int(hw * 0.12f), dx = int(hw * 0.06f);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const int sx = x - dx, sy = y - dy;
                if (sx >= 0 && sy >= 0 && sx < W && sy < H) shifted.at(x, y) = A.at(sx, sy);
            }
    }
    const Plane cast = gaussian(shifted, hw * 0.14f);
    auto toneMap = [](float v) { return clamp01(v / (1 + 0.12f * v)); };

    // The base: the surfaces lit, over the shadow they cast.
    QImage base(W, H, QImage::Format_ARGB32_Premultiplied);
    const Plane &ungroupedHeld = heldByGroup[QString()];
    for (int y = 0; y < H; ++y)
    {
        QRgb *out = reinterpret_cast<QRgb *>(base.scanLine(y));
        for (int x = 0; x < W; ++x)
        {
            const std::size_t i = std::size_t(y) * W + x;
            const float coverage = A.at(x, y);
            const float ndl = std::max(0.0f, nx[i] * L[0] + ny[i] * L[1] + nz[i] * L[2]);
            const float ndh = std::max(0.0f, nx[i] * Hv[0] + ny[i] * Hv[1] + nz[i] * Hv[2]);
            const float ndv = std::max(0.0f, nz[i]);
            const float ao = std::clamp(1 - (hollow.at(x, y) - HEIGHT.at(x, y)) * (2.2f / hw), 0.45f, 1.0f);
            const float rough = ROUGH.at(x, y), metal = METAL.at(x, y);
            const float r2 = std::max(rough, 0.04f) * std::max(rough, 0.04f);
            const float power = std::clamp(2 / r2, 4.0f, 600.0f);
            const float spec = ndh > 1e-3f ? std::exp(power * std::log(ndh)) * (power + 8) / (8 * float(M_PI)) * 0.12f : 0.0f;
            // The studio mirrored, as product photographs light glossy
            // things: a large softbox overhead, a strip light at the left, a
            // dark room, a little bounce from the floor; sharp on polished
            // surfaces, spread out on rough ones.
            const float rx = 2 * ndv * nx[i], ry = 2 * ndv * ny[i];
            auto soft = [](float v, float lo, float hi, float width) {
                return clamp01((v - lo) / width) * clamp01((hi - v) / width);
            };
            const float softbox = soft(-ry, 0.58f, 1.05f, 0.08f) * soft(rx, -0.5f, 0.5f, 0.14f);
            const float stripLight = soft(-rx, 0.72f, 1.1f, 0.05f) * soft(-ry, -0.3f, 0.55f, 0.1f);
            const float sharp = 0.02f + 3.2f * softbox + 1.4f * stripLight + 0.16f * clamp01(ry - 0.2f);
            const float blur = clamp01(rough * 1.7f);
            // A light tent round shining things fills the dark of the room.
            const float tent = GLEAM.at(x, y) - 1;
            const float env = sharp * (1 - blur) + 0.7f * blur + std::max(0.0f, tent) * 0.85f;
            const float ambient = 0.3f + 0.12f * clamp01(-ny[i]);
            const float grazing = 1 - ndv, grazing2 = grazing * grazing;
            const float fresnel = grazing2 * grazing2 * grazing;
            const float coatShare = (0.05f + 0.95f * fresnel) * COAT.at(x, y);
            const float lightCover = LIGHT.at(x, y);
            float rgb[3];
            for (int k = 0; k < 3; ++k)
            {
                const float albedo = RGB.at(x, y, k);
                const float F0 = 0.04f * (1 - metal) + albedo * metal;
                const float fres = F0 + (1 - F0) * fresnel;
                const float diffuse = albedo * (1 - metal) * (ambient + 0.95f * ndl * shadow[i]) * ao;
                const float reflect = env * fres * (0.35f + 0.65f * metal) * ao;
                const float specular = spec * shadow[i] * fres * 3.0f * (1 + std::max(0.0f, tent) * 0.5f);
                float surface = diffuse + reflect + specular;
                surface = surface * (1 - coatShare) + sharp * coatShare * 1.6f * ao;
                float lit = surface * (1 - lightCover * 0.85f) + ungroupedHeld.at(x, y, k);
                lit *= 1 - INK.at(x, y, k);
                rgb[k] = toneMap(lit) * coverage;
            }
            const float alpha = clamp01(coverage + cast.at(x, y) * 0.6f * (1 - coverage));
            out[x] = qRgba(int(std::lround(std::min(rgb[0], alpha) * 255)), int(std::lround(std::min(rgb[1], alpha) * 255)),
                           int(std::lround(std::min(rgb[2], alpha) * 255)), int(std::lround(alpha * 255)));
        }
    }
    // The window the panel is seen through, in the pictures' pixels.
    std::optional<QRectF> window;
    float windowFade = 0;
    if (scene.value(QLatin1String("clip")).isObject())
    {
        const QJsonObject clip = scene.value(QLatin1String("clip")).toObject();
        window = toPicture.mapRect(QRectF(clip.value(QLatin1String("x")).toDouble(), clip.value(QLatin1String("y")).toDouble(),
                                          clip.value(QLatin1String("width")).toDouble(), clip.value(QLatin1String("height")).toDouble()));
        windowFade = float(clip.value(QLatin1String("fade")).toDouble(0.35 * hw / scale) * scale);
        fadeToWindow(base, QPoint(0, 0), *window, windowFade);
    }
    bake.layers.insert(QStringLiteral("base"), base);
    bake.rects.insert(QStringLiteral("base"), QRect(0, 0, W, H));
    if (stop()) return bake;

    // Each group's lights: its light where it lies and its halo round it.
    // Halo from lights in no group goes with the lights.
    QHash<QString, Plane> blooms;
    QHash<QString, QRect> reaches;
    for (auto g = groups.begin(); g != groups.end(); ++g)
    {
        const QString name = g.key().isEmpty() ? QStringLiteral("lights") : g.key();
        Plane bloom(W, H, 3);
        bool any = false;
        if (g->right >= 0)
        {
            // The halo spreads three of its widest radii round the lights.
            const int spread = int(std::ceil(3 * std::max(32.0f * hw / 48.0f, g->halo.data.empty() ? 0.0f : 0.9f * hw))) + 2;
            const QRect area = QRect(QPoint(g->left, g->top), QPoint(g->right, g->bottom))
                                   .adjusted(-spread, -spread, spread, spread).intersected(QRect(0, 0, W, H));
            for (int y = area.top(); y <= area.bottom() && !any; ++y)
                for (int x = area.left(); x <= area.right() && !any; ++x)
                    if (g->glow.at(x, y, 0) + g->glow.at(x, y, 1) + g->glow.at(x, y, 2) > 1e-5f) any = true;
            Plane local(area.width(), area.height(), 3, 0);
            if (any)
                local = halo(cropped(g->glow, area), {{1.5f * hw / 48.0f, 0.8f}, {5.0f * hw / 48.0f, 0.65f},
                                                      {14.0f * hw / 48.0f, 0.45f}, {32.0f * hw / 48.0f, 0.28f}});
            if (!g->halo.data.empty())
            {
                any = true;
                const Plane wide = halo(cropped(g->halo, area), {{0.35f * hw, 4.0f}, {0.9f * hw, 2.2f}});
                for (std::size_t i = 0; i < local.data.size(); ++i) local.data[i] += wide.data[i];
            }
            for (int y = 0; y < area.height(); ++y)
                std::copy_n(&local.data[std::size_t(y) * area.width() * 3], std::size_t(area.width()) * 3,
                            &bloom.data[(std::size_t(area.y() + y) * W + area.x()) * 3]);
            reaches[name] = reaches.value(name).united(area);
            if (!g.key().isEmpty()) reaches[g.key()] = reaches.value(g.key()).united(area);
        }
        if (!any && g.key().isEmpty()) continue;
        auto existing = blooms.find(name);
        if (existing == blooms.end()) blooms.insert(name, bloom);
        else for (std::size_t i = 0; i < bloom.data.size(); ++i) existing->data[i] += bloom.data[i];
    }
    QStringList names = blooms.keys();
    for (auto g = groups.begin(); g != groups.end(); ++g)
        if (!g.key().isEmpty() && !names.contains(g.key())) names.append(g.key());
    for (const QString &name : names)
    {
        const Plane *bloom = blooms.contains(name) ? &blooms[name] : nullptr;
        const Plane *held = heldByGroup.contains(name) ? &heldByGroup[name] : nullptr;
        const QRect area = reaches.value(name);
        if ((!bloom && !held) || area.isEmpty()) continue;
        if (stop()) return bake;
        // The picture covers only as far as the group's light reaches.
        QImage layer(area.width(), area.height(), QImage::Format_ARGB32_Premultiplied);
        layer.fill(Qt::transparent);
        bool lit = false;
        for (int y = area.top(); y <= area.bottom(); ++y)
        {
            QRgb *out = reinterpret_cast<QRgb *>(layer.scanLine(y - area.top())) - area.left();
            for (int x = area.left(); x <= area.right(); ++x)
            {
                float rgb[3];
                for (int k = 0; k < 3; ++k)
                {
                    const float own = held ? held->at(x, y, k) * A.at(x, y) * (1 - INK.at(x, y, k)) : 0.0f;
                    const float halo = bloom ? bloom->at(x, y, k) * (1 - LINE.at(x, y) * 0.85f) : 0.0f;
                    rgb[k] = toneMap(own + halo);
                }
                // Over whatever lies below, a light adds itself: its colour,
                // and as much cover as its brightest channel.
                const float alpha = std::max({rgb[0], rgb[1], rgb[2]});
                if (alpha > 0.002f) lit = true;
                out[x] = qRgba(int(std::lround(rgb[0] * 255)), int(std::lround(rgb[1] * 255)),
                               int(std::lround(rgb[2] * 255)), int(std::lround(alpha * 255)));
            }
        }
        if (lit)
        {
            if (window) fadeToWindow(layer, area.topLeft(), *window, windowFade);
            bake.layers.insert(name, layer);
            bake.rects.insert(name, area);
        }
    }
    return bake;
}

QImage materialAlbedo(const QString &materialDirectory, const QJsonObject &albedo, const QJsonObject &recolour,
                      const QJsonObject &adjust)
{
    const QString texture = albedo.value(QLatin1String("texture")).toString();
    QImage picture = texture.isEmpty() ? QImage() : materialPicture(materialDirectory, texture, false);
    if (picture.isNull())
    {
        picture = QImage(4, 4, QImage::Format_ARGB32_Premultiplied);
        picture.fill(QColor(albedo.value(QLatin1String("colour")).toString(QStringLiteral("#808080")).left(7)));
    }
    QImage out = picture.convertToFormat(QImage::Format_RGBA8888);
    const int w = out.width(), h = out.height();
    std::vector<Rgb> colour(std::size_t(w) * h);
    for (int y = 0; y < h; ++y)
    {
        const uchar *line = out.constScanLine(y);
        for (int x = 0; x < w; ++x)
            colour[std::size_t(y) * w + x] = {line[x * 4] / 255.0f, line[x * 4 + 1] / 255.0f, line[x * 4 + 2] / 255.0f};
    }
    if (albedo.value(QLatin1String("tint")).isString())
    {
        const Rgb tint = colourOf(albedo.value(QLatin1String("tint")).toString());
        for (Rgb &c : colour) for (int k = 0; k < 3; ++k) c[k] *= tint[k];
    }
    if (!recolour.isEmpty())
    {
        double meanS = 0, meanV = 0;
        std::vector<Rgb> hsv(colour.size());
        for (std::size_t i = 0; i < colour.size(); ++i)
        {
            hsv[i] = rgbToHsv(colour[i]);
            meanS += hsv[i][1];
            meanV += hsv[i][2];
        }
        const float ms = std::max(0.05f, float(meanS / double(colour.size())));
        const float mv = std::max(0.05f, float(meanV / double(colour.size())));
        const float hue = number(recolour, "hue", 0), saturation = number(recolour, "saturation", 0);
        const float value = number(recolour, "value", 0.5f);
        for (std::size_t i = 0; i < colour.size(); ++i)
            colour[i] = hsvToRgb({hue / 360.0f, clamp01(hsv[i][1] / ms * std::max(0.02f, saturation)),
                                  clamp01(hsv[i][2] / mv * std::max(0.04f, value))});
    }
    if (!adjust.isEmpty())
    {
        const float hue = number(adjust, "hue", 0), saturation = number(adjust, "saturation", 1);
        const float brightness = number(adjust, "brightness", 1);
        for (Rgb &c : colour)
        {
            const Rgb hsv = rgbToHsv(c);
            c = hsvToRgb({hsv[0] + hue / 360.0f, clamp01(hsv[1] * saturation), clamp01(hsv[2] * brightness)});
        }
    }
    for (int y = 0; y < h; ++y)
    {
        uchar *line = out.scanLine(y);
        for (int x = 0; x < w; ++x)
        {
            const Rgb &c = colour[std::size_t(y) * w + x];
            for (int k = 0; k < 3; ++k) line[x * 4 + k] = uchar(std::lround(clamp01(c[k]) * 255));
            line[x * 4 + 3] = 255;
        }
    }
    return out;
}

QImage materialHeights(const QString &materialDirectory, const QString &texture)
{
    if (texture.isEmpty()) return {};
    const QImage heights = materialPicture(materialDirectory, texture, true);
    return heights.isNull() ? QImage() : heights.convertToFormat(QImage::Format_Grayscale8);
}

DecorationSurface decorationSurface(const QJsonObject &scene, const QString &materialDirectory, qreal requestedScale,
                                    qsizetype maximumPixels, const std::function<bool()> &cancelled)
{
    DecorationSurface surface;
    Field field;
    const QString failed = buildField(field, scene, materialDirectory, requestedScale, maximumPixels, cancelled);
    surface.bounds = field.bounds;
    surface.scale = field.scale;
    if (!failed.isEmpty())
    {
        surface.error = failed;
        return surface;
    }
    // A solid stands on a smooth outline where the flat pictures have a fine
    // row of notches.
    smoothNotches(field, std::max(1.5f, 0.07f * field.hw));
    const int W = field.W, H = field.H;
    const float hw = field.hw;
    const std::size_t n = std::size_t(W) * H;
    surface.halfWidth = hw;
    surface.size = QSize(W, H);
    surface.cover.assign(field.A.data.begin(), field.A.data.end());
    surface.rise.assign(field.HEIGHT.data.begin(), field.HEIGHT.data.end());
    // The light reaching hollows and the light picking out the relief, as
    // the flat lighting has them.
    const Plane hollow = gaussian(field.HEIGHT, hw * 0.12f);
    const Plane smooth = gaussian(field.HEIGHT, 0.7f);
    // How much of the round about the decoration covers, evened out over a
    // few pixels: a wall's share of what it is made of (a thin part's
    // walls, seen edge on, show less of it than a broad one's).
    const Plane around = gaussian(field.A, 3.0f);
    surface.albedo.resize(qsizetype(n) * 4);
    surface.finish.resize(qsizetype(n) * 4);
    surface.shape.resize(qsizetype(n) * 4 * qsizetype(sizeof(qfloat16)));
    auto *albedo = reinterpret_cast<uchar *>(surface.albedo.data());
    auto *finish = reinterpret_cast<uchar *>(surface.finish.data());
    auto *shape = reinterpret_cast<qfloat16 *>(surface.shape.data());
    const auto byte = [](float v) { return uchar(std::lround(clamp01(v) * 255)); };
    for (int y = 0; y < H; ++y)
    {
        if (cancelled && cancelled())
        {
            surface = DecorationSurface{};
            surface.error = QStringLiteral("cancelled");
            return surface;
        }
        for (int x = 0; x < W; ++x)
        {
            const std::size_t i = std::size_t(y) * W + x;
            const float ao = std::clamp(1 - (hollow.at(x, y) - field.HEIGHT.at(x, y)) * (2.2f / hw), 0.45f, 1.0f);
            for (int k = 0; k < 3; ++k) albedo[i * 4 + k] = byte(field.RGB.at(x, y, k) * (1 - field.INK.at(x, y, k)));
            albedo[i * 4 + 3] = byte(ao);
            finish[i * 4 + 0] = byte(field.METAL.at(x, y));
            finish[i * 4 + 1] = byte(field.ROUGH.at(x, y));
            finish[i * 4 + 2] = byte(field.COAT.at(x, y));
            finish[i * 4 + 3] = byte(field.GLEAM.at(x, y) / 3);
            shape[i * 4 + 0] = qfloat16(smooth.at(x, y));
            shape[i * 4 + 1] = qfloat16(field.LIGHT.at(x, y));
            shape[i * 4 + 2] = qfloat16(around.at(x, y));
            shape[i * 4 + 3] = qfloat16(field.OWN.at(x, y));
        }
    }
    // Each group's own light, as bright as the flat lighting has it before
    // it is held to what a picture can show (a 3D scene's bloom spreads what
    // is brighter), and a share of the glow its lights give round them.
    for (auto g = field.groups.cbegin(); g != field.groups.cend(); ++g)
    {
        if (g->right < 0) continue;
        const QString name = g.key().isEmpty() ? QStringLiteral("lights") : g.key();
        const QRect rect = QRect(QPoint(g->left, g->top), QPoint(g->right, g->bottom)).intersected(QRect(0, 0, W, H));
        if (rect.isEmpty()) continue;
        DecorationSurface::Light light;
        light.rect = rect;
        light.data.resize(qsizetype(rect.width()) * rect.height() * 4 * qsizetype(sizeof(qfloat16)));
        auto *out = reinterpret_cast<qfloat16 *>(light.data.data());
        bool lit = false;
        for (int y = rect.top(); y <= rect.bottom(); ++y)
            for (int x = rect.left(); x <= rect.right(); ++x)
            {
                const std::size_t o = (std::size_t(y - rect.top()) * rect.width() + (x - rect.left())) * 4;
                for (int k = 0; k < 3; ++k)
                {
                    const float v = (g->emission.at(x, y, k) * 1.4f + g->glow.at(x, y, k) * 0.35f) * (1 - field.INK.at(x, y, k));
                    out[o + k] = qfloat16(v);
                    if (v > 0.002f) lit = true;
                }
                out[o + 3] = qfloat16(1.0f);
            }
        if (!lit) continue;
        auto existing = surface.lights.find(name);
        if (existing == surface.lights.end())
        {
            surface.lights.insert(name, light);
            continue;
        }
        // Lights in no group join the "lights" group: one picture over
        // both rects.
        const QRect both = existing->rect.united(rect);
        QByteArray merged(qsizetype(both.width()) * both.height() * 4 * qsizetype(sizeof(qfloat16)), 0);
        auto *m = reinterpret_cast<qfloat16 *>(merged.data());
        for (const DecorationSurface::Light *part : {&existing.value(), &light})
        {
            const auto *in = reinterpret_cast<const qfloat16 *>(part->data.constData());
            for (int y = 0; y < part->rect.height(); ++y)
                for (int x = 0; x < part->rect.width(); ++x)
                {
                    const std::size_t from = (std::size_t(y) * part->rect.width() + x) * 4;
                    const std::size_t to = (std::size_t(part->rect.top() - both.top() + y) * both.width()
                                            + (part->rect.left() - both.left() + x)) * 4;
                    for (int k = 0; k < 3; ++k) m[to + k] = qfloat16(float(m[to + k]) + float(in[from + k]));
                    m[to + 3] = qfloat16(1.0f);
                }
        }
        existing->rect = both;
        existing->data = merged;
    }
    return surface;
}

namespace
{

// Which of a surface's pixels it covers (at least half), none if it is empty.
std::vector<uint8_t> coveredPixels(const DecorationSurface &surface)
{
    const std::size_t n = std::size_t(surface.size.width()) * surface.size.height();
    std::vector<uint8_t> inside(n);
    bool any = false;
    for (std::size_t i = 0; i < n && i < surface.cover.size(); ++i)
    {
        inside[i] = surface.cover[i] >= 0.5f;
        any = any || inside[i];
    }
    return any ? inside : std::vector<uint8_t>();
}

// The colour, finish and own-light maps: on the outline's half-covered
// pixels the decoration is taken back out of its mix with the empty dark
// round it (the finish from the defaults under it), and past the outline
// each carries the nearest covered pixel's - the margin texture bakers leave
// round what they bake - so filtering and the maps' blurrier levels do not
// pull the dark in at the edge.
void fillColourMargins(DecorationSurface &surface, const std::vector<uint8_t> &inside, const std::vector<int> &nearest)
{
    const std::size_t n = inside.size();
    auto *albedo = reinterpret_cast<uchar *>(surface.albedo.data());
    auto *finish = reinterpret_cast<uchar *>(surface.finish.data());
    auto *shape = reinterpret_cast<qfloat16 *>(surface.shape.data());
    const auto byte = [](float v) { return uchar(std::lround(clamp01(v) * 255)); };
    const float defaults[4] = {0.0f, 0.5f, 0.0f, 1.0f / 3};
    for (std::size_t i = 0; i < n; ++i)
    {
        const float c = surface.cover[i];
        if (!inside[i] || c >= 0.999f) continue;
        for (int k = 0; k < 3; ++k) albedo[i * 4 + k] = byte(float(albedo[i * 4 + k]) / 255.0f / c);
        for (int k = 0; k < 4; ++k)
            finish[i * 4 + k] = byte((float(finish[i * 4 + k]) / 255.0f - defaults[k] * (1 - c)) / c);
    }
    for (std::size_t i = 0; i < n; ++i)
    {
        if (inside[i] || nearest[i] < 0) continue;
        const std::size_t j = std::size_t(nearest[i]);
        std::copy_n(albedo + j * 4, 4, albedo + i * 4);
        std::copy_n(finish + j * 4, 4, finish + i * 4);
        shape[i * 4 + 1] = shape[j * 4 + 1];
        shape[i * 4 + 3] = shape[j * 4 + 3];
    }
}

// Each light's map the same way, carried only a pixel or two past the
// outline: wider, a star by the edge would spread into a blot.
void fillLightMargins(DecorationSurface &surface, const std::vector<uint8_t> &inside, const std::vector<int> &nearest)
{
    const int W = surface.size.width();
    for (auto light = surface.lights.begin(); light != surface.lights.end(); ++light)
    {
        const QRect rect = light->rect;
        auto *data = reinterpret_cast<qfloat16 *>(light->data.data());
        const auto at = [&](int x, int y) {
            return data + (std::size_t(y - rect.top()) * rect.width() + (x - rect.left())) * 4;
        };
        for (int y = rect.top(); y <= rect.bottom(); ++y)
            for (int x = rect.left(); x <= rect.right(); ++x)
            {
                const std::size_t i = std::size_t(y) * W + x;
                const float c = surface.cover[i];
                if (!inside[i] || c >= 0.999f) continue;
                qfloat16 *v = at(x, y);
                for (int k = 0; k < 3; ++k) v[k] = qfloat16(float(v[k]) / c);
            }
        for (int y = rect.top(); y <= rect.bottom(); ++y)
            for (int x = rect.left(); x <= rect.right(); ++x)
            {
                const std::size_t i = std::size_t(y) * W + x;
                if (inside[i] || nearest[i] < 0) continue;
                const int fx = nearest[i] % W, fy = nearest[i] / W;
                const bool near = (fx - x) * (fx - x) + (fy - y) * (fy - y) <= 4 && rect.contains(fx, fy);
                qfloat16 *v = at(x, y);
                for (int k = 0; k < 3; ++k) v[k] = near ? at(fx, fy)[k] : qfloat16(0.0f);
            }
    }
}

bool mapsFit(const DecorationSurface &surface)
{
    const qsizetype n = qsizetype(surface.size.width()) * surface.size.height();
    return n > 0 && qsizetype(surface.cover.size()) == n && surface.albedo.size() == n * 4 && surface.finish.size() == n * 4
           && surface.shape.size() == n * 4 * qsizetype(sizeof(qfloat16));
}

} // namespace

EdgePoint edgePoint(float along, QVector2D alongSlope, float height, QVector2D heightSlope, float level, float deep)
{
    EdgePoint p;
    const float u = clamp01(along);
    // Half the solid's thickness, and its middle, where top and underside meet.
    const float half = std::max(0.5f, (level + deep) * 0.5f);
    const float middle = level - half;
    // A quarter circle: upright at the outline, level where it ends.
    const float q = std::sqrt(std::max(0.0f, 1 - (1 - u) * (1 - u)));
    p.top = middle + (height - middle) * q;
    // The underside's curve is the top's mirrored (without the top's raised
    // parts), flat by the time the curve ends.
    const float mirrored = std::min(height, level) - middle;
    const float lower = mirrored + (half - mirrored) * smoothstep01(u);
    const float lowerSlope = (half - mirrored) * 6 * u * (1 - u);
    p.bottom = std::min(p.top, middle - lower * q);
    // Each surface's slope times the curve's height there, which stays
    // finite on the outline, where the curve stands upright; the maps' y
    // turns over into the solid's.
    const QVector2D topSlope = alongSlope * ((height - middle) * (1 - u)) + heightSlope * (q * q);
    const QVector2D lowSlope = alongSlope * -(lower * (1 - u) + lowerSlope * q * q);
    const QVector3D out(-alongSlope.x(), alongSlope.y(), 0);
    p.upward = QVector3D(-topSlope.x(), topSlope.y(), q);
    p.downward = QVector3D(lowSlope.x(), -lowSlope.y(), -q);
    for (QVector3D *n : {&p.upward, &p.downward})
    {
        if (n->lengthSquared() > 1e-12f) *n = n->normalized();
        else *n = out.lengthSquared() > 1e-12f ? out.normalized() : QVector3D(0, 0, n == &p.upward ? 1 : -1);
    }
    // The picture laid round the curve evenly: a point a share of the
    // quarter circle's length round from the outline reads the maps that
    // share of its radius in.
    const float rate = alongSlope.length();
    if (rate > 1e-6f && u > 0 && u < 1)
        p.inward = (std::acos(1 - u) * 2 / float(M_PI) - u) / rate;
    return p;
}

void roundSurfaceEdges(DecorationSurface &surface, float underside)
{
    const int W = surface.size.width(), H = surface.size.height();
    const std::size_t n = std::size_t(W) * H;
    if (W < 2 || H < 2 || surface.cover.size() != n || surface.rise.size() != n) return;
    std::vector<uint8_t> inside(n), outside(n);
    bool any = false;
    for (std::size_t i = 0; i < n; ++i)
    {
        inside[i] = surface.cover[i] >= 0.5f;
        outside[i] = !inside[i];
        any = any || inside[i];
    }
    if (!any) return;
    const float deep = std::max(0.0f, underside) * float(surface.scale);
    // The distance from the outline (which runs half way between a covered
    // pixel's middle and an uncovered one's), inside above 0: each side's
    // exact distance to the other's pixels, a pixel on the outline placed
    // within itself by how much of it is covered, lightly smoothed.
    const std::vector<float> in = distanceInside(inside, W, H), out = distanceInside(outside, W, H);
    Plane signedDistance(W, H);
    for (std::size_t i = 0; i < n; ++i)
    {
        float s = inside[i] ? in[i] - 0.5f : 0.5f - out[i];
        const float c = surface.cover[i];
        if ((inside[i] ? in[i] : out[i]) <= 1.0f && c > 0.0f && c < 1.0f) s = c - 0.5f;
        signedDistance.data[i] = s;
    }
    const Plane distance = gaussian(signedDistance, 0.8f);
    // How wide each part is, and the top's height in general (evened out
    // over half the solid's depth), each from the covered pixels only and
    // carried a little past the outline.
    std::vector<float> depth(n);
    for (std::size_t i = 0; i < n; ++i) depth[i] = inside[i] ? std::max(0.0f, distance.data[i]) : 0.0f;
    const std::vector<float> widest = partHalfWidth(depth, W, H);
    Plane weight(W, H), raised(W, H), wide(W, H);
    for (std::size_t i = 0; i < n; ++i)
    {
        const float m = inside[i] ? 1.0f : 0.0f;
        weight.data[i] = m;
        raised.data[i] = surface.rise[i] * m;
        wide.data[i] = widest[i] * m;
    }
    const float sigma = std::max(2.0f, deep * 0.5f);
    const Plane spreadWeight = gaussian(weight, sigma), spreadRaised = gaussian(raised, sigma);
    const Plane nearWeight = gaussian(weight, 1.5f), nearWide = gaussian(wide, 1.5f);
    surface.edgeAlong.resize(n);
    surface.edgeLevel.resize(n);
    surface.edgeDeep = deep;
    for (std::size_t i = 0; i < n; ++i)
    {
        const float level = spreadWeight.data[i] > 1e-4f ? spreadRaised.data[i] / spreadWeight.data[i] : 0.0f;
        const float half = std::max(0.5f, (level + deep) * 0.5f);
        const float across = nearWeight.data[i] > 1e-3f ? nearWide.data[i] / nearWeight.data[i] : half;
        // The curve reaches right across, to the part's middle: a soft body
        // rounds like a cushion, lit and shaded all the way over.
        const float radius = std::max(0.5f, across);
        surface.edgeAlong[i] = distance.data[i] / radius;
        surface.edgeLevel[i] = level;
    }
}


void fillSurfaceMargins(DecorationSurface &surface)
{
    if (!mapsFit(surface)) return;
    const std::vector<uint8_t> inside = coveredPixels(surface);
    if (inside.empty()) return;
    const std::vector<int> nearest = nearestOf(inside, surface.size.width(), surface.size.height());
    fillColourMargins(surface, inside, nearest);
    fillLightMargins(surface, inside, nearest);
}

} // namespace ArchDock
