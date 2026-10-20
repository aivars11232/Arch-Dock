#include "DecorationBaker.h"

#include "DecorationWorker.h"

#include <QCoreApplication>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <functional>

namespace
{

// A QML value (a JavaScript object or array, or a plain value) as JSON.
QJsonValue jsonOf(const QVariant &value)
{
    QVariant plain = value;
    if (plain.metaType() == QMetaType::fromType<QJSValue>()) plain = plain.value<QJSValue>().toVariant();
    return QJsonValue::fromVariant(plain);
}

// The folder a URL names, as a path the worker can read.
QString folderOf(const QUrl &url)
{
    if (url.isLocalFile()) return url.toLocalFile();
    if (url.scheme() == QLatin1String("qrc")) return QLatin1Char(':') + url.path();
    return url.toString();
}

// A decoration's moving frames are drawn at fewer pixels each than a still
// one, so a whole loop stays a few megabytes.
constexpr qsizetype stillPixels = 3'000'000;
constexpr qsizetype framePixels = 1'200'000;

struct Request
{
    QString input;
    QString directory;
    qreal scale = 1;
    bool relief = false;
    qreal underside = 0;
    int triangles = 60000;
    int generation = 0;
    std::shared_ptr<std::atomic<int>> wanted;
    QPointer<DecorationBaker> baker;
};

void deliver(const Request &request, int index, int count, qreal loop, qreal halfWidth, const QVariant &launcher,
             const QVariantList &lightSpots, const ArchDock::DecorationBake &bake)
{
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [request, index, count, loop, halfWidth, launcher, lightSpots, bake]() {
            if (request.baker)
                request.baker->acceptFrame(request.generation, index, count, loop, halfWidth, launcher, lightSpots, bake);
        },
        Qt::QueuedConnection);
}

void deliverSolid(const Request &request, qreal halfWidth, const QVariant &launcher, const QVariantList &lightSpots,
                  const std::shared_ptr<const ArchDock::DecorationSurface> &surface,
                  const std::shared_ptr<const ArchDock::ReliefMesh> &solid,
                  const std::shared_ptr<const ArchDock::DecorationTubes> &tubes, const QVariantList &iconHeights)
{
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [request, halfWidth, launcher, lightSpots, surface, solid, tubes, iconHeights]() {
            if (request.baker)
                request.baker->acceptSolid(request.generation, halfWidth, launcher, lightSpots, surface, solid, tubes, iconHeights);
        },
        Qt::QueuedConnection);
}

// How high a surface stands round a point (path pixels): the highest it is
// within a third of the band's half width, where an icon's foot rests.
qreal heightUnder(const ArchDock::DecorationSurface &surface, QPointF point)
{
    const int W = surface.size.width(), H = surface.size.height();
    if (W <= 0 || H <= 0) return 0;
    const double scale = surface.scale;
    const double cx = (point.x() - surface.bounds.x()) * scale, cy = (point.y() - surface.bounds.y()) * scale;
    const int r = std::max(1, int(std::lround(surface.halfWidth / 3)));
    float high = 0;
    for (int y = int(cy) - r; y <= int(cy) + r; ++y)
        for (int x = int(cx) - r; x <= int(cx) + r; ++x)
        {
            if (x < 0 || y < 0 || x >= W || y >= H) continue;
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > double(r) * r) continue;
            const std::size_t i = std::size_t(y) * W + x;
            if (surface.cover[i] >= 0.5f) high = std::max(high, surface.rise[i]);
        }
    return high / scale;
}

// On the worker's thread: the decoration worked out as one still scene and
// made a solid with its surface maps, for a 3D scene.
void makeSolid(const QJSValue &job, const Request &request, qreal halfWidth, const std::function<bool()> &cancelled)
{
    const auto fail = [&request](const QString &error) {
        ArchDock::DecorationBake failed;
        failed.error = error;
        deliver(request, 0, 1, 0, 0, QVariant(), {}, failed);
    };
    const QJSValue text = job.property(QStringLiteral("sceneText"))
                              .call({QJSValue(request.input), QJSValue(-1.0), QJSValue(halfWidth)});
    if (text.isError()) return fail(QStringLiteral("scripts-failed: ") + text.toString());
    const QJsonObject scene = QJsonDocument::fromJson(text.toString().toUtf8()).object();
    if (scene.isEmpty()) return fail(QStringLiteral("scripts-failed"));
    auto surface = std::make_shared<ArchDock::DecorationSurface>(ArchDock::decorationSurface(
        scene, request.directory + QStringLiteral("/materials"), request.scale, stillPixels, cancelled));
    if (surface->error == QLatin1String("cancelled") || cancelled()) return;
    if (!surface->error.isEmpty()) return fail(surface->error);
    // Its maps carried past its outline; a soft decoration (a nebula) with no
    // straight wall round it, its edge rounded, the top curving down and the
    // underside up to meet halfway round the outline.
    ArchDock::fillSurfaceMargins(*surface);
    if (scene.value(QLatin1String("roundEdges")).toBool()) ArchDock::roundSurfaceEdges(*surface, float(request.underside));
    if (cancelled()) return;
    auto solid = std::make_shared<ArchDock::ReliefMesh>(
        ArchDock::reliefMesh(*surface, float(request.underside), request.triangles));
    if (cancelled()) return;
    // What is round all the way round, swept as tubes.
    auto tubes = std::make_shared<ArchDock::DecorationTubes>(ArchDock::decorationTubes(
        scene.value(QLatin1String("tubes")).toArray(), scene.value(QLatin1String("tubeMarks")).toArray(), float(halfWidth),
        request.directory + QStringLiteral("/materials")));
    if (cancelled()) return;
    if (solid->indices.empty() && tubes->mesh.indices.empty()) return fail(QStringLiteral("empty-solid"));
    QVariantList heights;
    for (const QJsonValue &point : scene.value(QLatin1String("iconPoints")).toArray())
    {
        const QJsonArray pair = point.toArray();
        const QPointF at(pair.at(0).toDouble(), pair.at(1).toDouble());
        heights.append(std::max(heightUnder(*surface, at), qreal(ArchDock::tubeHeightAt(*tubes, at))));
    }
    // Its round lights, each with how high it stands, to be found under the
    // pointer in the scene.
    QVariantList spots;
    for (const QJsonValue &value : scene.value(QLatin1String("lightSpots")).toArray())
    {
        QVariantMap spot = value.toObject().toVariantMap();
        spot.insert(QStringLiteral("z"), heightUnder(*surface, QPointF(spot.value(QStringLiteral("x")).toDouble(),
                                                                        spot.value(QStringLiteral("y")).toDouble())));
        spots.append(spot);
    }
    deliverSolid(request, halfWidth, scene.value(QLatin1String("launcher")).toVariant(), spots, surface, solid, tubes, heights);
}

// On the worker's thread: the decoration's frames worked out and lit one
// after another, each handed back as soon as it is lit.
void bake(ArchDock::DecorationScripts &scripts, const Request &request)
{
    const auto cancelled = [request]() {
        return ArchDock::DecorationWorker::stopping() || request.wanted->load() != request.generation;
    };
    if (cancelled()) return;
    const auto fail = [&request](const QString &error) {
        ArchDock::DecorationBake failed;
        failed.error = error;
        deliver(request, 0, 1, 0, 0, QVariant(), {}, failed);
    };
    QString error;
    const QJSValue job = scripts.module(request.directory + QStringLiteral("/DecorationJob.js"), &error);
    if (job.isUndefined()) return fail(QStringLiteral("scripts-unavailable: ") + error);
    const QJsonObject prepared =
        QJsonDocument::fromJson(job.property(QStringLiteral("prepare")).call({QJSValue(request.input)}).toString().toUtf8()).object();
    if (prepared.contains(QLatin1String("error")) || prepared.isEmpty())
        return fail(prepared.value(QLatin1String("error")).toString(QStringLiteral("scripts-failed")));
    const int frames = prepared.value(QLatin1String("frames")).toInt();
    const qreal loop = prepared.value(QLatin1String("loop")).toDouble();
    const qreal halfWidth = prepared.value(QLatin1String("halfWidth")).toDouble();
    if (request.relief) return makeSolid(job, request, halfWidth, cancelled);
    const int count = std::max(1, frames);
    QJsonValue sharedBounds;
    for (int index = 0; index < count; ++index)
    {
        if (cancelled()) return;
        const QJSValue text = job.property(QStringLiteral("sceneText"))
                                  .call({QJSValue(request.input), QJSValue(frames ? double(index) / frames : -1.0), QJSValue(halfWidth)});
        if (text.isError()) return fail(QStringLiteral("scripts-failed: ") + text.toString());
        QJsonObject scene = QJsonDocument::fromJson(text.toString().toUtf8()).object();
        if (scene.isEmpty()) return fail(QStringLiteral("scripts-failed"));
        if (frames)
        {
            // Every frame of a loop lies in the same box: the first frame's,
            // with room for what moves out of it.
            if (index == 0)
            {
                QJsonObject box = scene.value(QLatin1String("bounds")).toObject();
                const double room = std::ceil(halfWidth);
                box.insert(QStringLiteral("x"), box.value(QLatin1String("x")).toDouble() - room);
                box.insert(QStringLiteral("y"), box.value(QLatin1String("y")).toDouble() - room);
                box.insert(QStringLiteral("width"), box.value(QLatin1String("width")).toDouble() + 2 * room);
                box.insert(QStringLiteral("height"), box.value(QLatin1String("height")).toDouble() + 2 * room);
                sharedBounds = box;
            }
            scene.insert(QStringLiteral("bounds"), sharedBounds);
        }
        const QVariant launcher = scene.value(QLatin1String("launcher")).toVariant();
        const QVariantList spots = scene.value(QLatin1String("lightSpots")).toArray().toVariantList();
        const ArchDock::DecorationBake lit = ArchDock::bakeDecoration(
            scene, request.directory + QStringLiteral("/materials"), request.scale, frames ? framePixels : stillPixels, cancelled);
        if (lit.error == QLatin1String("cancelled")) return;
        deliver(request, index, count, loop, halfWidth, launcher, spots, lit);
    }
}

} // namespace

DecorationBaker::DecorationBaker(QObject *parent)
    : QObject(parent)
{
    // Settings that change together (a new path and a new size while a
    // panel is laid out) are worked out once.
    m_delay.setSingleShot(true);
    m_delay.setInterval(0);
    connect(&m_delay, &QTimer::timeout, this, &DecorationBaker::start);
}

DecorationBaker::~DecorationBaker()
{
    // Nothing more is wanted: a job still running stops.
    m_wanted->store(-1);
}

void DecorationBaker::setDirectory(const QUrl &directory)
{
    if (m_directory == directory) return;
    m_directory = directory;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setDecoration(const QString &decoration)
{
    if (m_decoration == decoration) return;
    m_decoration = decoration;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setDecorationData(const QVariant &data)
{
    m_decorationData = data;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setPath(const QVariant &path)
{
    m_path = path;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setHalfWidth(qreal halfWidth)
{
    if (qFuzzyCompare(m_halfWidth, halfWidth)) return;
    m_halfWidth = halfWidth;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setIconPlaces(const QVariant &places)
{
    m_iconPlaces = places;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setChoices(const QVariant &choices)
{
    m_choices = choices;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setAnimated(bool animated)
{
    if (m_animated == animated) return;
    m_animated = animated;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setEnds(qreal ends)
{
    if (qFuzzyCompare(m_ends, ends)) return;
    m_ends = ends;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setFit(const QVariant &fit)
{
    m_fit = fit;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setClip(const QVariant &clip)
{
    m_clip = clip;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setScale(qreal scale)
{
    if (qFuzzyCompare(m_scale, scale)) return;
    m_scale = scale;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setRelief(bool relief)
{
    if (m_relief == relief) return;
    m_relief = relief;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setUnderside(qreal underside)
{
    if (qFuzzyCompare(m_underside, underside)) return;
    m_underside = underside;
    emit inputChanged();
    schedule();
}

void DecorationBaker::setTriangles(int triangles)
{
    if (m_triangles == triangles) return;
    m_triangles = triangles;
    emit inputChanged();
    schedule();
}

void DecorationBaker::schedule()
{
    m_delay.start();
}

void DecorationBaker::setBusy(bool busy)
{
    if (m_busy == busy) return;
    m_busy = busy;
    emit busyChanged();
}

void DecorationBaker::clearPictures(const QString &error)
{
    m_frames.clear();
    m_bounds = QRectF();
    m_layers.clear();
    m_frameCount = 0;
    m_loop = 0;
    m_drawnHalfWidth = 0;
    m_launcher = QVariant();
    m_lightSpots.clear();
    m_surface.reset();
    m_solid.reset();
    m_tubes.reset();
    m_iconHeights.clear();
    m_error = error;
    ++m_revision;
    emit baked();
}

void DecorationBaker::start()
{
    const QJsonValue path = jsonOf(m_path);
    const QJsonValue data = jsonOf(m_decorationData);
    const bool hasData = data.isObject() && !data.toObject().isEmpty();
    const QJsonObject input{{QStringLiteral("decoration"), hasData ? data : QJsonValue(m_decoration)},
                            {QStringLiteral("path"), path},
                            {QStringLiteral("halfWidth"), m_halfWidth},
                            {QStringLiteral("slots"), jsonOf(m_iconPlaces)},
                            {QStringLiteral("choices"), jsonOf(m_choices)},
                            {QStringLiteral("animated"), m_animated && !m_relief},
                            {QStringLiteral("relief"), m_relief},
                            {QStringLiteral("ends"), m_ends},
                            {QStringLiteral("fit"), jsonOf(m_fit)},
                            {QStringLiteral("clip"), jsonOf(m_clip)}};
    const QString text = QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact))
                         + QLatin1Char('|') + QString::number(m_scale) + QLatin1Char('|') + m_directory.toString()
                         + (m_relief ? QStringLiteral("|solid|%1|%2").arg(m_underside).arg(m_triangles) : QString());
    // The same settings again (a panel laid out anew where it was): the
    // pictures stand.
    if (text == m_input) return;
    m_input = text;
    const int generation = ++m_generation;
    m_wanted->store(generation);
    if ((m_decoration.isEmpty() && !hasData) || !path.isObject() || m_directory.isEmpty())
    {
        setBusy(false);
        if (!m_frames.isEmpty() || m_solid || m_tubes || !m_error.isEmpty()) clearPictures(QString());
        return;
    }
    setBusy(true);
    Request request;
    request.input = QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact));
    request.directory = folderOf(m_directory);
    request.scale = m_scale;
    request.relief = m_relief;
    request.underside = m_underside;
    request.triangles = m_triangles;
    request.generation = generation;
    request.wanted = m_wanted;
    request.baker = this;
    ArchDock::DecorationWorker::post([request](ArchDock::DecorationScripts &scripts) { bake(scripts, request); });
}

void DecorationBaker::acceptFrame(int generation, int index, int count, qreal loop, qreal halfWidth, const QVariant &launcher,
                                  const QVariantList &lightSpots, const ArchDock::DecorationBake &bake)
{
    // Only the newest settings' pictures are kept.
    if (generation != m_generation) return;
    if (!bake.error.isEmpty())
    {
        setBusy(false);
        clearPictures(bake.error);
        return;
    }
    if (index == 0)
    {
        m_frames.clear();
        m_surface.reset();
        m_solid.reset();
        m_tubes.reset();
        m_iconHeights.clear();
        m_bounds = QRectF(bake.bounds);
        m_pictureScale = bake.scale;
        m_frameCount = count;
        m_loop = loop;
        m_drawnHalfWidth = halfWidth;
        m_launcher = launcher;
        m_lightSpots = lightSpots;
        m_error.clear();
    }
    else if (index != m_frames.size())
    {
        return;
    }
    ArchDock::DecorationBake frame = bake;
    // A picture no different from the first frame's is the first frame's,
    // so a loop keeps one copy of what does not move.
    if (!m_frames.isEmpty())
        for (auto it = frame.layers.begin(); it != frame.layers.end(); ++it)
        {
            const ArchDock::DecorationBake &first = m_frames.constFirst();
            const auto same = first.layers.constFind(it.key());
            if (same != first.layers.constEnd() && first.rects.value(it.key()) == frame.rects.value(it.key()) && *same == *it)
                *it = *same;
        }
    m_frames.append(frame);
    const bool complete = m_frames.size() >= m_frameCount;
    // The first frame shows at once; the others when the loop is whole.
    if (index != 0 && !complete) return;
    QStringList names;
    for (const ArchDock::DecorationBake &each : std::as_const(m_frames))
        for (auto it = each.layers.cbegin(); it != each.layers.cend(); ++it)
            if (it.key() != QLatin1String("base") && !names.contains(it.key())) names.append(it.key());
    names.sort();
    m_layers = names;
    if (complete) setBusy(false);
    ++m_revision;
    emit baked();
}

void DecorationBaker::acceptSolid(int generation, qreal halfWidth, const QVariant &launcher, const QVariantList &lightSpots,
                                  const std::shared_ptr<const ArchDock::DecorationSurface> &surface,
                                  const std::shared_ptr<const ArchDock::ReliefMesh> &solid,
                                  const std::shared_ptr<const ArchDock::DecorationTubes> &tubes, const QVariantList &iconHeights)
{
    if (generation != m_generation) return;
    m_frames.clear();
    m_surface = surface;
    m_solid = solid;
    m_tubes = tubes;
    m_iconHeights = iconHeights;
    m_bounds = QRectF(surface->bounds);
    m_pictureScale = surface->scale;
    m_frameCount = 1;
    m_loop = 0;
    m_drawnHalfWidth = halfWidth;
    m_launcher = launcher;
    m_lightSpots = lightSpots;
    m_error.clear();
    QStringList names = surface->lights.keys();
    names.sort();
    m_layers = names;
    setBusy(false);
    ++m_revision;
    emit baked();
}

QRectF DecorationBaker::lightRect(const QString &name) const
{
    if (!m_surface || m_surface->size.isEmpty()) return {};
    const auto light = m_surface->lights.constFind(name);
    if (light == m_surface->lights.constEnd()) return {};
    const double w = m_surface->size.width(), h = m_surface->size.height();
    return QRectF(light->rect.x() / w, light->rect.y() / h, light->rect.width() / w, light->rect.height() / h);
}

QImage DecorationBaker::layerImage(const QString &name, int frame) const
{
    if (m_frames.isEmpty()) return {};
    return m_frames.at(std::clamp(frame, 0, int(m_frames.size()) - 1)).layers.value(name);
}

QRectF DecorationBaker::layerRect(const QString &name, int frame) const
{
    if (m_frames.isEmpty() || m_pictureScale <= 0) return {};
    const QRect rect = m_frames.at(std::clamp(frame, 0, int(m_frames.size()) - 1)).rects.value(name);
    return QRectF(rect.x() / m_pictureScale, rect.y() / m_pictureScale, rect.width() / m_pictureScale,
                  rect.height() / m_pictureScale);
}

QList<qint64> DecorationBaker::layerKeys(const QString &name) const
{
    QList<qint64> keys;
    for (const ArchDock::DecorationBake &frame : m_frames)
    {
        const QImage image = frame.layers.value(name);
        if (!image.isNull() && !keys.contains(image.cacheKey())) keys.append(image.cacheKey());
    }
    return keys;
}
