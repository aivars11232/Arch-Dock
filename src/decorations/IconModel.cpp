#include "IconModel.h"

#include "DecorationWorker.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHash>
#include <QIcon>
#include <QImageReader>
#include <QMutex>
#include <QPainter>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

#include <list>

namespace
{

// The pictures the helper is given: square, this many pixels a side.
constexpr int pictureSide = 512;
// Named into each object's fingerprint, so objects made another way are
// made anew.
constexpr char madeHow[] = "triposr two fronts 1";

QString objectsFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/arch-dock/icon-objects");
}

QString objectFile(const QString &name)
{
    return objectsFolder() + QLatin1Char('/') + name + QStringLiteral(".adicon");
}

QString pictureFile(const QString &name)
{
    return objectsFolder() + QLatin1Char('/') + name + QStringLiteral(".png");
}

QString helperProgram()
{
    const QString chosen = qEnvironmentVariable("ARCHDOCK_ICON_OBJECT_HELPER");
    if (!chosen.isEmpty()) return QFileInfo(chosen).isExecutable() ? chosen : QString();
#ifdef ARCHDOCK_ICON_OBJECT_HELPER
    if (QFileInfo(QStringLiteral(ARCHDOCK_ICON_OBJECT_HELPER)).isExecutable()) return QStringLiteral(ARCHDOCK_ICON_OBJECT_HELPER);
#endif
    return QStandardPaths::findExecutable(QStringLiteral("arch-dock-icon-object"));
}

// The picture as the helper takes it: as large as fits the square, centred
// on a clear ground, as an icon is drawn.
QImage squared(const QImage &picture)
{
    QImage square(pictureSide, pictureSide, QImage::Format_RGBA8888);
    square.fill(Qt::transparent);
    const QImage fitted = picture.convertToFormat(QImage::Format_RGBA8888)
                              .scaled(pictureSide, pictureSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter painter(&square);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage((pictureSide - fitted.width()) / 2, (pictureSide - fitted.height()) / 2, fitted);
    painter.end();
    return square.convertToFormat(QImage::Format_RGBA8888);
}

QString fingerprint(const QImage &square)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArrayView(reinterpret_cast<const char *>(square.constBits()), square.sizeInBytes()));
    hash.addData(QByteArrayView(madeHow));
    return QString::fromLatin1(hash.result().toHex());
}

QImage readPicture(const QString &path)
{
    QImageReader reader(path);
    // A drawing (an SVG) is drawn at the size it is wanted at.
    const QSize size = reader.size();
    if (size.isValid() && reader.format().startsWith("svg"))
        reader.setScaledSize(size.scaled(pictureSide, pictureSide, Qt::KeepAspectRatio));
    return reader.read();
}

// The objects read last, by name: the same icon shown again (on another
// panel, after a panel is made anew) is not read again.
class Read
{
public:
    std::shared_ptr<const ArchDock::IconObjectMesh> find(const QString &name)
    {
        QMutexLocker lock(&m_mutex);
        const auto found = m_index.constFind(name);
        if (found == m_index.constEnd()) return nullptr;
        m_order.splice(m_order.begin(), m_order, *found);
        return m_order.front().second;
    }

    void keep(const QString &name, std::shared_ptr<const ArchDock::IconObjectMesh> mesh)
    {
        QMutexLocker lock(&m_mutex);
        const auto found = m_index.constFind(name);
        if (found != m_index.constEnd()) m_order.erase(*found);
        m_order.emplace_front(name, std::move(mesh));
        m_index.insert(name, m_order.begin());
        while (m_order.size() > 48)
        {
            m_index.remove(m_order.back().first);
            m_order.pop_back();
        }
    }

private:
    using Entry = std::pair<QString, std::shared_ptr<const ArchDock::IconObjectMesh>>;
    QMutex m_mutex;
    std::list<Entry> m_order;
    QHash<QString, std::list<Entry>::iterator> m_index;
};

Read &readObjects()
{
    static Read objects;
    return objects;
}

// The objects this program waits for: one helper at a time is given every
// picture waiting; an object another program's helper makes is taken as
// its file appears.
class Making
{
public:
    static Making &instance()
    {
        static Making making;
        return making;
    }

    void want(const QString &name, IconModel *model)
    {
        if (m_failed.contains(name))
        {
            model->objectFailed(name);
            return;
        }
        m_waiting[name].append(QPointer<IconModel>(model));
        if (QFileInfo::exists(objectFile(name)))
        {
            done(name);
            return;
        }
        if (!m_running.contains(name) && !m_queue.contains(name)) m_queue.append(name);
        watch();
        start();
    }

private:
    // The objects' folder watched for objects other programs' helpers make;
    // the watcher goes with the application, before this.
    void watch()
    {
        if (m_watcher) return;
        m_watcher = new QFileSystemWatcher(QCoreApplication::instance());
        m_watcher->addPath(objectsFolder());
        QObject::connect(m_watcher, &QFileSystemWatcher::directoryChanged, m_watcher, [this]() {
            const QStringList names = m_waiting.keys();
            for (const QString &name : names)
                if (QFileInfo::exists(objectFile(name))) done(name);
        });
    }

    void start()
    {
        if (m_helper || m_queue.isEmpty()) return;
        const QString program = helperProgram();
        if (program.isEmpty())
        {
            const QStringList names = m_queue;
            m_queue.clear();
            for (const QString &name : names) fail(name);
            return;
        }
        m_running = m_queue;
        m_queue.clear();
        QStringList arguments;
        for (const QString &name : std::as_const(m_running)) arguments << pictureFile(name) << objectFile(name);
        m_helper = new QProcess;
        m_helper->setProgram(program);
        m_helper->setArguments(arguments);
        m_helper->setStandardOutputFile(QProcess::nullDevice());
        m_helper->setProcessChannelMode(QProcess::ForwardedErrorChannel);
        QObject::connect(m_helper, &QProcess::finished, m_helper, [this]() { finished(); });
        QObject::connect(m_helper, &QProcess::errorOccurred, m_helper, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) finished();
        });
        m_helper->start();
    }

    void finished()
    {
        if (!m_helper) return;
        m_helper->deleteLater();
        m_helper = nullptr;
        const QStringList names = m_running;
        m_running.clear();
        for (const QString &name : names)
        {
            if (QFileInfo::exists(objectFile(name))) done(name);
            else fail(name);
        }
        start();
    }

    void done(const QString &name)
    {
        const auto models = m_waiting.take(name);
        for (const auto &model : models)
            if (model) model->objectMade(name);
    }

    void fail(const QString &name)
    {
        m_failed.insert(name);
        const auto models = m_waiting.take(name);
        for (const auto &model : models)
            if (model) model->objectFailed(name);
    }

    QPointer<QFileSystemWatcher> m_watcher;
    // Not the application's: a helper at work when the program ends goes on
    // and keeps what it makes.
    QProcess *m_helper = nullptr;
    QStringList m_queue;
    QStringList m_running;
    QHash<QString, QList<QPointer<IconModel>>> m_waiting;
    QSet<QString> m_failed;
};

// The object laid in the frame: its picture's square (100 across, the
// middle at 0) made the frame's rectangle within the scene's square, its
// depth as wide as the frame, where on the picture each point lies made
// where on the scene's square.
std::shared_ptr<const ArchDock::IconObjectMesh> framed(const ArchDock::IconObjectMesh &object, const QRectF &frame)
{
    auto mesh = std::make_shared<ArchDock::IconObjectMesh>(object);
    const float left = float(frame.x()), top = float(frame.y()), width = float(frame.width()),
                height = float(frame.height());
    QVector3D lowest(1e9f, 1e9f, 1e9f), highest(-1e9f, -1e9f, -1e9f);
    for (std::size_t v = 0; v < mesh->vertices.size(); v += ArchDock::IconObjectMesh::stride)
    {
        float *p = &mesh->vertices[v];
        p[0] = (left + (p[0] / 100 + 0.5f) * width - 0.5f) * 100;
        p[1] = (0.5f - (top + (0.5f - p[1] / 100) * height)) * 100;
        p[2] *= width;
        p[6] = left + p[6] * width;
        p[7] = 1 - (top + (1 - p[7]) * height);
        for (int k = 0; k < 3; ++k)
        {
            lowest[k] = std::min(lowest[k], p[k]);
            highest[k] = std::max(highest[k], p[k]);
        }
    }
    mesh->minimum = lowest;
    mesh->maximum = highest;
    mesh->depth = object.depth * width;
    return mesh;
}

} // namespace

IconModel::IconModel(QObject *parent)
    : QObject(parent)
{
}

IconModel::~IconModel()
{
    // An object still being looked for for this one is no longer wanted.
    m_wanted->fetch_add(1);
}

bool IconModel::available()
{
    static const bool there = !helperProgram().isEmpty() && !ArchDock::iconObjectModels().isEmpty();
    return there;
}

void IconModel::setIconSource(const QString &source)
{
    if (m_source == source) return;
    m_source = source;
    emit iconSourceChanged();
    restart();
}

void IconModel::setFrame(const QRectF &frame)
{
    if (m_frame == frame) return;
    m_frame = frame;
    emit frameChanged();
    if (m_object) show(m_object);
}

QVariantList IconModel::colour() const
{
    if (!m_mesh) return {1.0, 1.0, 1.0, 1.0};
    return {m_mesh->colour[0], m_mesh->colour[1], m_mesh->colour[2], m_mesh->colour[3]};
}

void IconModel::restart()
{
    const int generation = m_wanted->fetch_add(1) + 1;
    m_name.clear();
    setMaking(false);
    if (m_source.isEmpty() || !available())
    {
        m_object = nullptr;
        show(nullptr);
        return;
    }
    // A theme's icon is drawn here, where its theme is; a file is read off
    // this thread.
    const QUrl url(m_source);
    QString file;
    QImage themed;
    if (url.isLocalFile()) file = url.toLocalFile();
    else if (url.scheme() == QLatin1String("qrc")) file = QLatin1Char(':') + url.path();
    else if (m_source.startsWith(QLatin1Char('/')) || m_source.startsWith(QLatin1Char(':'))) file = m_source;
    else if (url.scheme().isEmpty())
        themed = QIcon::fromTheme(m_source).pixmap(QSize(pictureSide, pictureSide), 1.0).toImage();
    if (file.isEmpty() && themed.isNull())
    {
        m_object = nullptr;
        show(nullptr);
        return;
    }
    const auto wanted = m_wanted;
    QPointer<IconModel> self(this);
    ArchDock::DecorationWorker::post([=](ArchDock::DecorationScripts &) {
        if (ArchDock::DecorationWorker::stopping() || wanted->load() != generation) return;
        const QImage picture = themed.isNull() ? readPicture(file) : themed;
        QString name;
        std::shared_ptr<const ArchDock::IconObjectMesh> object;
        bool waiting = false;
        if (!picture.isNull())
        {
            const QImage square = squared(picture);
            name = fingerprint(square);
            object = readObjects().find(name);
            if (!object && QFileInfo::exists(objectFile(name)))
            {
                if (auto loaded = ArchDock::loadIconObject(objectFile(name)))
                {
                    object = std::make_shared<const ArchDock::IconObjectMesh>(std::move(*loaded));
                    readObjects().keep(name, object);
                }
            }
            if (!object)
            {
                QDir().mkpath(objectsFolder());
                waiting = QFileInfo::exists(pictureFile(name)) || square.save(pictureFile(name));
            }
        }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, wanted, generation, name, object, waiting]() {
                if (!self || wanted->load() != generation) return;
                self->m_name = name;
                self->m_object = object;
                self->show(object);
                if (!object && waiting)
                {
                    self->setMaking(true);
                    Making::instance().want(name, self);
                }
            },
            Qt::QueuedConnection);
    });
}

void IconModel::objectMade(const QString &name)
{
    if (name != m_name) return;
    load(name, m_wanted->load());
}

void IconModel::objectFailed(const QString &name)
{
    if (name == m_name) setMaking(false);
}

void IconModel::load(const QString &name, int generation)
{
    const auto wanted = m_wanted;
    QPointer<IconModel> self(this);
    ArchDock::DecorationWorker::post([=](ArchDock::DecorationScripts &) {
        if (ArchDock::DecorationWorker::stopping() || wanted->load() != generation) return;
        std::shared_ptr<const ArchDock::IconObjectMesh> object = readObjects().find(name);
        if (!object)
            if (auto loaded = ArchDock::loadIconObject(objectFile(name)))
            {
                object = std::make_shared<const ArchDock::IconObjectMesh>(std::move(*loaded));
                readObjects().keep(name, object);
            }
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [self, wanted, generation, object]() {
                if (!self || wanted->load() != generation) return;
                self->m_object = object;
                self->setMaking(false);
                self->show(object);
            },
            Qt::QueuedConnection);
    });
}

void IconModel::show(std::shared_ptr<const ArchDock::IconObjectMesh> object)
{
    m_mesh = object ? framed(*object, m_frame) : nullptr;
    emit modelled();
}

void IconModel::setMaking(bool making)
{
    if (m_making == making) return;
    m_making = making;
    emit makingChanged();
}
