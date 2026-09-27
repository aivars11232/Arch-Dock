#pragma once

#include <QDBusContext>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

namespace ArchDock
{

// Runtime-only application feedback. This listens alongside Plasma; it never
// owns the notification service or derives unread counts from notifications.
class OverlayModel final : public QObject, protected QDBusContext
{
    Q_OBJECT
public:
    static constexpr qint64 MaximumAgeMs = 300000;
    explicit OverlayModel(QObject *parent = nullptr);
    bool ingest(const QString &sender, const QString &desktopId,
                const QVariantMap &values, qint64 now);
    void removeSource(const QString &sender);
    void expire(qint64 now);
    void setPublishing(bool enabled);
    void setTemporaryStatus(const QString &desktopId, const QString &text);
    [[nodiscard]] QVariantMap snapshot(const QString &desktopId) const;
    [[nodiscard]] bool available() const;

signals:
    void changed();

private slots:
    void update(const QString &uri, const QVariantMap &values);

private:
    struct Entry { QVariantMap values; qint64 updated = 0; };
    struct Temporary { QString text; qint64 expires = 0; };
    void scheduleChange();
    QHash<QString, QHash<QString, Entry>> m_sources;
    QHash<QString, Temporary> m_temporary;
    QElapsedTimer m_clock;
    QTimer m_expiry;
    QTimer m_publish;
    bool m_publishing = true;
    bool m_dirty = false;
};

}
