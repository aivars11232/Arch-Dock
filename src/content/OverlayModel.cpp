#include "OverlayModel.h"

#include "../model/IconEntryIdentity.h"

#include <KService>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QRegularExpression>
#include <cmath>

namespace ArchDock
{
namespace
{
QString applicationKey(const QString &desktopId)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,247}\\.desktop$"));
    return pattern.match(desktopId).hasMatch()
        ? IconEntryIdentity::forApplication({}, desktopId) : QString{};
}
}

OverlayModel::OverlayModel(QObject *parent) : QObject(parent)
{
    m_clock.start();
    m_publish.setSingleShot(true);
    m_publish.setInterval(100);
    connect(&m_publish, &QTimer::timeout, this, [this] {
        if (m_dirty && m_publishing) { m_dirty = false; emit changed(); }
    });
    m_expiry.setInterval(1000);
    connect(&m_expiry, &QTimer::timeout, this, [this] { expire(m_clock.elapsed()); });
    m_expiry.start();
    auto bus = QDBusConnection::sessionBus();
    bus.connect({}, {}, QStringLiteral("com.canonical.Unity.LauncherEntry"),
                QStringLiteral("Update"), this, SLOT(update(QString,QVariantMap)));
    auto *watcher = new QDBusServiceWatcher(this);
    watcher->setConnection(bus);
    watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &OverlayModel::removeSource);
    // The bus delivers ownership changes for unique names too, including
    // senders which do not own an application-specific well-known name.
    connect(this, &OverlayModel::changed, watcher, [this, watcher] {
        watcher->setWatchedServices(m_sources.keys());
    });
}

void OverlayModel::update(const QString &uri, const QVariantMap &values)
{
    if (!calledFromDBus() || !uri.startsWith(QStringLiteral("application://")))
        return;
    const QString id = uri.mid(14);
    if (applicationKey(id).isEmpty())
        return;
    const auto service = KService::serviceByStorageId(id);
    if (service)
        ingest(message().service(), service->storageId(), values, m_clock.elapsed());
}

bool OverlayModel::ingest(const QString &sender, const QString &desktopId,
                          const QVariantMap &values, qint64 now)
{
    const QString id = applicationKey(desktopId);
    if (!sender.startsWith(QLatin1Char(':')) || id.isEmpty()
        || values.isEmpty() || values.size() > 16 || now < 0)
        return false;
    QVariantMap accepted;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
    {
        const auto &key = it.key();
        const auto &value = it.value();
        if (key == QStringLiteral("count"))
        {
            const int type = value.metaType().id();
            if (type != QMetaType::LongLong && type != QMetaType::Int
                && type != QMetaType::UInt && type != QMetaType::ULongLong)
                return false;
            bool ok = false;
            const qlonglong count = value.toLongLong(&ok);
            if (!ok || count < 0 || count > 1000000000) return false;
            accepted.insert(key, count);
        }
        else if (key == QStringLiteral("progress"))
        {
            if (value.metaType().id() != QMetaType::Double) return false;
            const double progress = value.toDouble();
            if (!std::isfinite(progress) || progress < 0 || progress > 1) return false;
            accepted.insert(key, progress);
        }
        else if (key == QStringLiteral("count-visible")
                 || key == QStringLiteral("progress-visible") || key == QStringLiteral("urgent"))
        {
            if (value.metaType().id() != QMetaType::Bool) return false;
            accepted.insert(key, value);
        }
    }
    if (accepted.isEmpty() || (!m_sources.contains(sender) && m_sources.size() >= 128)
        || (!m_sources.value(sender).contains(id) && m_sources.value(sender).size() >= 64))
        return false;
    auto &entry = m_sources[sender][id];
    for (auto it = accepted.cbegin(); it != accepted.cend(); ++it)
        entry.values.insert(it.key(), it.value());
    entry.updated = now;
    scheduleChange();
    // Ownership tracking must stay active even while rendering is concealed.
    const auto watchers = findChildren<QDBusServiceWatcher *>();
    if (!watchers.isEmpty()) watchers.first()->addWatchedService(sender);
    return true;
}

void OverlayModel::scheduleChange()
{
    m_dirty = true;
    if (m_publishing && !m_publish.isActive()) m_publish.start();
}

void OverlayModel::setPublishing(bool enabled)
{
    m_publishing = enabled;
    if (!enabled) m_publish.stop();
    else if (m_dirty) scheduleChange();
}

void OverlayModel::removeSource(const QString &sender)
{
    if (m_sources.remove(sender)) scheduleChange();
}

void OverlayModel::expire(qint64 now)
{
    bool changed = false;
    for (auto source = m_sources.begin(); source != m_sources.end();)
    {
        for (auto entry = source->begin(); entry != source->end();)
        {
            if (now - entry->updated >= MaximumAgeMs)
            { entry = source->erase(entry); changed = true; }
            else ++entry;
        }
        if (source->isEmpty()) source = m_sources.erase(source);
        else ++source;
    }
    for (auto it = m_temporary.begin(); it != m_temporary.end();)
    {
        if (now >= it->expires) { it = m_temporary.erase(it); changed = true; }
        else ++it;
    }
    if (changed) scheduleChange();
}

void OverlayModel::setTemporaryStatus(const QString &desktopId, const QString &text)
{
    const QString id = applicationKey(desktopId);
    if (id.isEmpty() || text.isEmpty() || text.size() > 160 || m_temporary.size() >= 128) return;
    m_temporary.insert(id, {text, m_clock.elapsed() + 5000});
    scheduleChange();
}

bool OverlayModel::available() const { return !m_sources.isEmpty(); }

QVariantMap OverlayModel::snapshot(const QString &desktopId) const
{
    const QString id = applicationKey(desktopId);
    QVariantMap values;
    int owners = 0;
    for (const auto &source : m_sources)
        if (source.contains(id)) { values = source.value(id).values; ++owners; }
    QVariantMap result{{QStringLiteral("overlayAvailable"), owners == 1},
                       {QStringLiteral("badgeText"), QString{}},
                       {QStringLiteral("progress"), -1.0},
                       {QStringLiteral("urgent"), false},
                       {QStringLiteral("temporaryStatus"), m_temporary.value(id).text}};
    if (owners != 1) return result;
    if (values.value(QStringLiteral("count-visible")).toBool()
        && values.value(QStringLiteral("count")).toLongLong() > 0)
        result[QStringLiteral("badgeText")] = values.value(QStringLiteral("count")).toString();
    if (values.value(QStringLiteral("progress-visible")).toBool()
        && values.contains(QStringLiteral("progress")))
        result[QStringLiteral("progress")] = values.value(QStringLiteral("progress"));
    result[QStringLiteral("urgent")] = values.value(QStringLiteral("urgent"), false);
    return result;
}
}
