#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QVariantMap>
#include <optional>

namespace ArchDock
{
// The saved settings of panels that were removed, newest first, so that a
// panel removed by mistake (or by Plasma losing its widget) can be brought
// back from Panel Studio. A panel's record is kept as it was the moment
// before it was deleted; once a removal has gone through, only the newest
// MaximumEntries are kept.
class RemovedPanelStore final
{
public:
    struct Entry
    {
        // A random key naming this entry, never reused.
        QString key;
        QDateTime removedAt;
        // Why it went, in plain words for the log and Panel Studio.
        QString cause;
        QVariantMap record;
    };

    static constexpr int MaximumEntries = 10;
    // At most this many are read or written: room past MaximumEntries for
    // removals still under way, kept and neither trimmed nor forgotten yet.
    static constexpr int StoredEntriesLimit = 2 * MaximumEntries;
    static constexpr qint64 MaximumBytes = 8 * 1024 * 1024;

    static QString defaultPath();
    explicit RemovedPanelStore(QString path = defaultPath());

    [[nodiscard]] QList<Entry> entries(QString *error = nullptr) const;
    // Adds a record as the newest entry: the new entry's key, or empty when
    // it could not be kept. Nothing older is dropped yet, so forgetting the
    // entry again (its panel was not removed after all) leaves the store as
    // it was; trim() makes room once the removal has gone through.
    [[nodiscard]] QString keep(const QVariantMap &record, const QString &cause,
                               QString *error = nullptr);
    // Drops the oldest entries past MaximumEntries.
    [[nodiscard]] bool trim(QString *error = nullptr);
    [[nodiscard]] std::optional<Entry> entry(const QString &key) const;
    // Drops one entry: its panel is back, or was not removed after all.
    [[nodiscard]] bool forget(const QString &key, QString *error = nullptr);

private:
    QString m_path;
    [[nodiscard]] bool write(const QList<Entry> &entries, QString *error) const;
};
}
