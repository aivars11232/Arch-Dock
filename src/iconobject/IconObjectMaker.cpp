#include "IconObjectMaker.h"

#include <onnxruntime_cxx_api.h>

#include <QElapsedTimer>

#include <algorithm>
#include <array>
#include <cmath>

namespace ArchDock
{
namespace
{

// The picture the network sees, 512 by 512, its object 0.85 of it.
constexpr int seen = 512;
constexpr double foreground = 0.85;
// TripoSR's box (-0.87 to 0.87 along each axis), its marching cubes' grid
// and the density its surface lies at; the grid asked every fifth point
// first.
constexpr float radius = 0.87f;
constexpr int grid = 256;
constexpr int coarse = 5;
constexpr float level = 25.0f;
// The camera the network takes its picture to be seen by: 1.9 from the
// middle, 40 degrees from top to bottom (TripoSR renders its objects so).
constexpr double cameraDistance = 1.9;
constexpr double halfView = 20.0 * M_PI / 180.0;
// The front made lighter: never further off than this share of its size.
constexpr float roughest = 0.002f;
// Points asked of the field at once.
constexpr std::size_t batch = 65536;

// Weights for resampling a row of `in` samples to `out`, as PIL and
// PyTorch's antialiased bilinear resampling weigh them: a triangle as wide
// as the step when shrinking.
struct Taps
{
    std::vector<int> first;
    std::vector<int> count;
    std::vector<float> weights;
    int widest = 0;
};

Taps resampling(int in, int out)
{
    Taps taps;
    const double scale = double(in) / double(out);
    const double support = scale >= 1 ? scale : 1.0;
    const double inverse = scale >= 1 ? 1.0 / scale : 1.0;
    const std::size_t rowCount = std::size_t(out);
    std::vector<std::vector<double>> rows(rowCount);
    for (int i = 0; i < out; ++i)
    {
        const double centre = scale * (i + 0.5);
        const int low = std::max(int(centre - support + 0.5), 0);
        const int count = std::min(int(centre + support + 0.5), in) - low;
        double total = 0;
        auto &row = rows[std::size_t(i)];
        for (int j = 0; j < count; ++j)
        {
            const double w = std::max(0.0, 1.0 - std::abs((j + low - centre + 0.5) * inverse));
            row.push_back(w);
            total += w;
        }
        for (double &w : row) w = total > 0 ? w / total : 0;
        taps.first.push_back(low);
        taps.count.push_back(count);
        taps.widest = std::max(taps.widest, count);
    }
    for (const auto &row : rows)
    {
        for (int j = 0; j < taps.widest; ++j) taps.weights.push_back(j < int(row.size()) ? float(row[std::size_t(j)]) : 0.0f);
    }
    return taps;
}

// The picture as TripoSR's own tools hand it to the network: cut to its
// opaque part (its last row and column left out, as resize_foreground
// does), padded square, padded again till the part is 0.85 of the side,
// laid over a mid grey, and resampled to 512 by 512; red, green and blue
// planes in 0..1.
struct Prepared
{
    std::vector<float> rgb;
    // Where the square the network saw lies over the picture, in its
    // pixels, and how wide it is.
    int left = 0;
    int top = 0;
    int side = 0;
    // The picture's opaque box, its edges in pixels.
    int boxLeft = 0;
    int boxTop = 0;
    int boxRight = 0;
    int boxBottom = 0;
};

std::optional<Prepared> prepare(const QImage &picture)
{
    const QImage image = picture.convertToFormat(QImage::Format_RGBA8888);
    const int w = image.width(), h = image.height();
    int x1 = w, x2 = -1, y1 = h, y2 = -1;
    for (int y = 0; y < h; ++y)
    {
        const uchar *line = image.constScanLine(y);
        for (int x = 0; x < w; ++x)
            if (line[4 * x + 3] > 0)
            {
                x1 = std::min(x1, x);
                x2 = std::max(x2, x);
                y1 = std::min(y1, y);
                y2 = std::max(y2, y);
            }
    }
    if (x2 - x1 < 2 || y2 - y1 < 2) return std::nullopt;
    Prepared out;
    out.boxLeft = x1;
    out.boxTop = y1;
    out.boxRight = x2 + 1;
    out.boxBottom = y2 + 1;
    const int cutWidth = x2 - x1, cutHeight = y2 - y1;
    const int size = std::max(cutWidth, cutHeight);
    const int full = int(size / foreground);
    const int pad = (full - size) / 2;
    out.left = x1 - (size - cutWidth) / 2 - pad;
    out.top = y1 - (size - cutHeight) / 2 - pad;
    out.side = full;
    // Over the grey, as eight-bit values again (as TripoSR's run keeps it).
    std::vector<float> square(std::size_t(full) * full * 3);
    for (int y = 0; y < full; ++y)
        for (int x = 0; x < full; ++x)
        {
            const int px = out.left + x, py = out.top + y;
            float rgba[4] = {0, 0, 0, 0};
            if (px >= x1 && px < x2 && py >= y1 && py < y2)
            {
                const uchar *p = image.constScanLine(py) + 4 * px;
                for (int k = 0; k < 4; ++k) rgba[k] = float(p[k]) / 255.0f;
            }
            for (int k = 0; k < 3; ++k)
            {
                const float value = rgba[k] * rgba[3] + (1 - rgba[3]) * 0.5f;
                square[(std::size_t(y) * full + std::size_t(x)) * 3 + std::size_t(k)] = float(int(value * 255.0f)) / 255.0f;
            }
        }
    // Resampled along the rows, then the columns.
    const Taps taps = resampling(full, seen);
    std::vector<float> rows(std::size_t(full) * seen * 3, 0);
    for (int y = 0; y < full; ++y)
        for (int x = 0; x < seen; ++x)
            for (int j = 0; j < taps.count[std::size_t(x)]; ++j)
            {
                const float weight = taps.weights[std::size_t(x) * std::size_t(taps.widest) + std::size_t(j)];
                const float *from = &square[(std::size_t(y) * full + std::size_t(taps.first[std::size_t(x)] + j)) * 3];
                float *to = &rows[(std::size_t(y) * seen + std::size_t(x)) * 3];
                for (int k = 0; k < 3; ++k) to[k] += weight * from[k];
            }
    out.rgb.assign(std::size_t(3) * seen * seen, 0);
    for (int y = 0; y < seen; ++y)
        for (int j = 0; j < taps.count[std::size_t(y)]; ++j)
        {
            const float weight = taps.weights[std::size_t(y) * std::size_t(taps.widest) + std::size_t(j)];
            const int from = taps.first[std::size_t(y)] + j;
            for (int x = 0; x < seen; ++x)
                for (int k = 0; k < 3; ++k)
                    out.rgb[(std::size_t(k) * seen + std::size_t(y)) * seen + std::size_t(x)]
                        += weight * rows[(std::size_t(from) * seen + std::size_t(x)) * 3 + std::size_t(k)];
        }
    return out;
}

// A grid point's place in TripoSR's box, worked out as TripoSR does.
float gridPlace(int i)
{
    return float(double(i) / double(grid - 1)) * (2 * radius) - radius;
}

} // namespace

struct IconObjectMaker::Sessions
{
    Ort::Env environment{ORT_LOGGING_LEVEL_WARNING, "arch-dock-icon-object"};
    Ort::Session network{nullptr};
    Ort::Session field{nullptr};
    Ort::MemoryInfo memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
};

IconObjectMaker::IconObjectMaker(const QString &models, int threads)
{
    if (models.isEmpty())
    {
        m_error = QStringLiteral("the models are not installed");
        return;
    }
    try
    {
        auto sessions = std::make_unique<Sessions>();
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(std::max(1, threads));
        options.SetInterOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        sessions->network = Ort::Session(sessions->environment,
                                         (models + QStringLiteral("/triposr-network.onnx")).toLocal8Bit().constData(), options);
        sessions->field = Ort::Session(sessions->environment,
                                       (models + QStringLiteral("/triposr-field.onnx")).toLocal8Bit().constData(), options);
        m_sessions = std::move(sessions);
    }
    catch (const Ort::Exception &error)
    {
        m_error = QString::fromUtf8(error.what());
    }
}

IconObjectMaker::~IconObjectMaker() = default;

std::optional<IconObjectMesh> IconObjectMaker::make(const QImage &source)
{
    if (!m_sessions) return std::nullopt;
    // A picture that is not square is made square first, as an icon is
    // drawn: centred.
    QImage picture = source.convertToFormat(QImage::Format_RGBA8888);
    if (picture.width() != picture.height())
    {
        const int side = std::max(picture.width(), picture.height());
        QImage square(side, side, QImage::Format_RGBA8888);
        square.fill(Qt::transparent);
        for (int y = 0; y < picture.height(); ++y)
            std::copy_n(picture.constScanLine(y), 4 * picture.width(),
                        square.scanLine(y + (side - picture.height()) / 2) + 4 * ((side - picture.width()) / 2));
        picture = square;
    }
    const std::optional<Prepared> in = prepare(picture);
    if (!in) return std::nullopt;
    QElapsedTimer timer;
    timer.start();
    try
    {
        Sessions &s = *m_sessions;
        const std::array<int64_t, 4> pictureShape{1, 3, seen, seen};
        Ort::Value input = Ort::Value::CreateTensor<float>(s.memory, const_cast<float *>(in->rgb.data()), in->rgb.size(),
                                                           pictureShape.data(), pictureShape.size());
        const char *networkIn[] = {"rgb"};
        const char *networkOut[] = {"planes"};
        auto out = s.network.Run(Ort::RunOptions{nullptr}, networkIn, &input, 1, networkOut, 1);
        std::vector<float> planes(out[0].GetTensorData<float>(), out[0].GetTensorData<float>() + 3 * 40 * 64 * 64);
        out.clear();
        networkTime = double(timer.nsecsElapsed()) / 1e6;
        timer.restart();

        // The field at points of TripoSR's box: their density, or colour.
        const std::array<int64_t, 4> planeShape{3, 40, 64, 64};
        const auto ask = [&](const std::vector<float> &places, const char *what, float *answers, int width) {
            const std::size_t count = places.size() / 3;
            for (std::size_t from = 0; from < count; from += batch)
            {
                const std::size_t n = std::min(batch, count - from);
                const std::array<int64_t, 2> pointShape{int64_t(n), 3};
                Ort::Value inputs[2] = {
                    Ort::Value::CreateTensor<float>(s.memory, planes.data(), planes.size(), planeShape.data(), planeShape.size()),
                    Ort::Value::CreateTensor<float>(s.memory, const_cast<float *>(places.data()) + 3 * from, 3 * n,
                                                    pointShape.data(), pointShape.size())};
                const char *fieldIn[] = {"planes", "points"};
                const char *fieldOut[] = {what};
                auto answer = s.field.Run(Ort::RunOptions{nullptr}, fieldIn, inputs, 2, fieldOut, 1);
                std::copy_n(answer[0].GetTensorData<float>(), n * std::size_t(width), answers + from * std::size_t(width));
            }
        };
        std::vector<float> places;
        const AskField density = [&](const int *points, std::size_t count, float *values) {
            places.resize(3 * count);
            for (std::size_t p = 0; p < 3 * count; ++p) places[p] = gridPlace(points[p]);
            ask(places, "density", values, 1);
        };
        std::vector<float> values = trackedField(grid, coarse, level, density);
        fieldTime = double(timer.nsecsElapsed()) / 1e6;
        timer.restart();

        // The surface, in TripoSR's box, each point in its own colour.
        ColouredMesh mesh;
        std::vector<float> steps;
        marchingCubes(values, grid, level, steps, mesh.triangles);
        values = {};
        if (mesh.triangles.empty()) return std::nullopt;
        mesh.points.resize(steps.size());
        for (std::size_t p = 0; p < steps.size(); ++p)
            mesh.points[p] = float(double(steps[p]) / double(grid - 1) * (2 * radius) - radius);
        mesh.colours.resize(mesh.points.size());
        ask(mesh.points, "colour", mesh.colours.data(), 3);
        for (float &c : mesh.colours) c = std::clamp(c, 0.0f, 1.0f);

        // Its front (TripoSR's objects face +x), lighter, and the front
        // again as its back.
        const float cut = widestSlice(mesh);
        const ColouredMesh both = twoFronts(simplified(frontOf(mesh, cut), frontTriangles, roughest), cut);
        mesh = {};
        const std::size_t count = both.points.size() / 3;

        // Each point's normal: its triangles' own, each as large as it is.
        std::vector<float> normals(both.points.size(), 0);
        for (std::size_t t = 0; t + 2 < both.triangles.size(); t += 3)
        {
            const float *a = &both.points[3 * std::size_t(both.triangles[t])];
            const float *b = &both.points[3 * std::size_t(both.triangles[t + 1])];
            const float *c = &both.points[3 * std::size_t(both.triangles[t + 2])];
            const QVector3D n = QVector3D::crossProduct(QVector3D(b[0] - a[0], b[1] - a[1], b[2] - a[2]),
                                                        QVector3D(c[0] - a[0], c[1] - a[1], c[2] - a[2]));
            for (int k = 0; k < 3; ++k)
                for (int axis = 0; axis < 3; ++axis) normals[3 * std::size_t(both.triangles[t + std::size_t(k)]) + std::size_t(axis)] += n[axis];
        }

        // Where each point lies on the picture. Straight on, as the dock's
        // camera sees it: the object's front-on outline laid over the
        // picture's opaque box. As the network's camera saw it (nearer parts
        // larger), its back's points as their fronts': where on the picture
        // it wears; laid over the same box.
        const double half = cameraDistance * std::tan(halfView);
        std::vector<double> straight(2 * count), seenAt(2 * count);
        double box[2][4] = {{1e300, 1e300, -1e300, -1e300}, {1e300, 1e300, -1e300, -1e300}};
        for (std::size_t p = 0; p < count; ++p)
        {
            const double x = both.points[3 * p], y = both.points[3 * p + 1], z = both.points[3 * p + 2];
            const double nearer = cameraDistance / (cameraDistance - (cut + std::abs(x - cut)));
            const double spots[2][2] = {{0.5 + y / (2 * half), 0.5 - z / (2 * half)},
                                        {0.5 + y * nearer / (2 * half), 0.5 - z * nearer / (2 * half)}};
            for (int which = 0; which < 2; ++which)
            {
                const double px = in->left + spots[which][0] * in->side, py = in->top + spots[which][1] * in->side;
                (which ? seenAt : straight)[2 * p] = px;
                (which ? seenAt : straight)[2 * p + 1] = py;
                box[which][0] = std::min(box[which][0], px);
                box[which][1] = std::min(box[which][1], py);
                box[which][2] = std::max(box[which][2], px);
                box[which][3] = std::max(box[which][3], py);
            }
        }
        const double opaque[4] = {double(in->boxLeft), double(in->boxTop), double(in->boxRight), double(in->boxBottom)};
        const auto fitting = [&](const double *b) {
            return 0.5 * ((opaque[2] - opaque[0]) / std::max(1e-9, b[2] - b[0]) + (opaque[3] - opaque[1]) / std::max(1e-9, b[3] - b[1]));
        };
        const double fit[2] = {fitting(box[0]), fitting(box[1])};
        const double side = picture.width();
        const double pixelsPerUnit = in->side / (2 * half) * fit[0];

        IconObjectMesh object;
        object.vertices.reserve(count * IconObjectMesh::stride);
        QVector3D lowest(1e9f, 1e9f, 1e9f), highest(-1e9f, -1e9f, -1e9f);
        double colour[3] = {0, 0, 0};
        for (std::size_t p = 0; p < count; ++p)
        {
            double at[2][2];
            for (int which = 0; which < 2; ++which)
                for (int k = 0; k < 2; ++k)
                {
                    const double middle = 0.5 * (box[which][k] + box[which][k + 2]);
                    const double into = 0.5 * (opaque[k] + opaque[k + 2]);
                    at[which][k] = into + ((which ? seenAt : straight)[2 * p + std::size_t(k)] - middle) * fit[which];
                }
            // TripoSR's +y is the picture's right, +z its up, +x toward the
            // camera.
            const QVector3D place(float((at[0][0] / side - 0.5) * 100), float((0.5 - at[0][1] / side) * 100),
                                  float((both.points[3 * p] - cut) * pixelsPerUnit / side * 100));
            QVector3D normal(normals[3 * p + 1], normals[3 * p + 2], normals[3 * p]);
            normal.normalize();
            const float *c = &both.colours[3 * p];
            object.vertices.insert(object.vertices.end(),
                                   {place.x(), place.y(), place.z(), normal.x(), normal.y(), normal.z(),
                                    float(at[1][0] / side), float(1 - at[1][1] / side), c[0], c[1], c[2], 1.0f});
            for (int k = 0; k < 3; ++k)
            {
                lowest[k] = std::min(lowest[k], place[k]);
                highest[k] = std::max(highest[k], place[k]);
                colour[k] += c[k];
            }
        }
        object.indices = both.triangles;
        object.minimum = lowest;
        object.maximum = highest;
        object.depth = highest.z();
        for (int k = 0; k < 3; ++k) object.colour[k] = float(colour[k] / double(std::max<std::size_t>(1, count)));
        meshTime = double(timer.nsecsElapsed()) / 1e6;
        return object;
    }
    catch (const Ort::Exception &error)
    {
        m_error = QString::fromUtf8(error.what());
        return std::nullopt;
    }
}

} // namespace ArchDock
