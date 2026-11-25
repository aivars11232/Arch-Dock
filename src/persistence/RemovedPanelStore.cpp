#include "persistence/RemovedPanelStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace ArchDock
{
namespace
{
void fail(QString *error, const QString &message)
{
    if (error) *error = message;
}
}

QString RemovedPanelStore::defaultPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("removed-panels.json"));
}

RemovedPanelStore::RemovedPanelStore(QString path)
    : m_path(QFileInfo(path).absoluteFilePath())
{
}

QList<RemovedPanelStore::Entry> RemovedPanelStore::entries(QString *error) const
{
    QList<Entry> result;
    const QFileInfo info(m_path);
    if (!info.exists()) return result;
    // A regular file of reasonable size only: never follow a link elsewhere.
    if (info.isSymLink() || !info.isFile() || info.size() > MaximumBytes)
    {
        fail(error, QStringLiteral("removed-panels-file-unsafe"));
        return result;
    }
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        fail(error, QStringLiteral("removed-panels-file-unreadable"));
        return result;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray())
    {
        fail(error, QStringLiteral("removed-panels-file-invalid"));
        return result;
    }
    for (const QJsonValue &value : document.array())
    {
        const QJsonObject object = value.toObject();
        Entry entry;
        entry.key = object.value(QStringLiteral("key")).toString();
        entry.removedAt = QDateTime::fromString(
            object.value(QStringLiteral("removedAt")).toString(), Qt::ISODate);
        entry.cause = object.value(QStringLiteral("cause")).toString();
        entry.record = object.value(QStringLiteral("record")).toObject().toVariantMap();
        if (entry.key.isEmpty() || entry.record.isEmpty()) continue;
        result.append(entry);
        if (result.size() >= StoredEntriesLimit) break;
    }
    return result;
}

QString RemovedPanelStore::keep(const QVariantMap &record, const QString &cause, QString *error)
{
    if (record.isEmpty())
    {
        fail(error, QStringLiteral("removed-panel-record-empty"));
        return {};
    }
    // A damaged file must not stop a removal from being kept: it is replaced.
    QList<Entry> list = entries();
    Entry entry;
    entry.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.removedAt = QDateTime::currentDateTimeUtc();
    entry.cause = cause;
    entry.record = record;
    list.prepend(entry);
    // Removals that never finished cannot grow the file without end.
    while (list.size() > StoredEntriesLimit) list.removeLast();
    return write(list, error) ? entry.key : QString{};
}

bool RemovedPanelStore::trim(QString *error)
{
    QList<Entry> list = entries(error);
    if (list.size() <= MaximumEntries) return true;
    list.resize(MaximumEntries);
    return write(list, error);
}

std::optional<RemovedPanelStore::Entry> RemovedPanelStore::entry(const QString &key) const
{
    for (const Entry &candidate : entries())
    {
        if (candidate.key == key) return candidate;
    }
    return std::nullopt;
}

bool RemovedPanelStore::forget(const QString &key, QString *error)
{
    QList<Entry> list = entries(error);
    const qsizetype before = list.size();
    list.removeIf([&key](const Entry &candidate) { return candidate.key == key; });
    return list.size() == before || write(list, error);
}

bool RemovedPanelStore::write(const QList<Entry> &list, QString *error) const
{
    QJsonArray array;
    for (const Entry &entry : list)
    {
        array.append(QJsonObject{
            {QStringLiteral("key"), entry.key},
            {QStringLiteral("removedAt"), entry.removedAt.toString(Qt::ISODate)},
            {QStringLiteral("cause"), entry.cause},
            {QStringLiteral("record"), QJsonObject::fromVariantMap(entry.record)}});
    }
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
    {
        fail(error, QStringLiteral("removed-panels-folder-unwritable"));
        return false;
    }
    // The file is written in place of a link, never through it.
    if (QFileInfo(m_path).isSymLink() && !QFile::remove(m_path))
    {
        fail(error, QStringLiteral("removed-panels-file-unsafe"));
        return false;
    }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(array).toJson(QJsonDocument::Indented)) < 0 ||
        !file.commit())
    {
        fail(error, QStringLiteral("removed-panels-file-unwritable"));
        return false;
    }
    return true;
}
}
