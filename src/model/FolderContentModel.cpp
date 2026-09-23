#include "FolderContentModel.h"

#include <QDirListing>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>

#include <algorithm>

namespace ArchDock
{
QVariantMap FolderContentModel::snapshot(const QUrl &folderUrl)
{
    QVariantMap result{
        {QStringLiteral("status"), QStringLiteral("error")},
        {QStringLiteral("errorCode"), QStringLiteral("invalid-folder-url")},
        {QStringLiteral("folderUrl"), folderUrl.toString(QUrl::FullyEncoded)},
        {QStringLiteral("entries"), QVariantList{}},
        {QStringLiteral("truncated"), false},
        {QStringLiteral("limit"), EntryLimit}};
    if (!folderUrl.isValid() || !folderUrl.isLocalFile() || !folderUrl.host().isEmpty()
        || folderUrl.hasQuery() || folderUrl.hasFragment()
        || folderUrl.toLocalFile().contains(QChar::Null))
    {
        return result;
    }

    const QFileInfo folder(folderUrl.toLocalFile());
    if (!folder.exists() || !folder.isDir() || !folder.isReadable()
        || !folder.isExecutable())
    {
        result[QStringLiteral("status")] = QStringLiteral("unavailable");
        result[QStringLiteral("errorCode")] = !folder.exists()
            ? QStringLiteral("folder-missing") : !folder.isDir()
            ? QStringLiteral("not-a-folder") : QStringLiteral("folder-unreadable");
        return result;
    }

    const QString root = folder.canonicalFilePath();
    QMimeDatabase mimeDatabase;
    QVariantList entries;
    entries.reserve(EntryLimit);
    // Default excludes hidden/dot entries and does not recurse or follow links.
    // Read at most one extra name to detect truncation, then sort only this page.
    for (const auto &child : QDirListing(root))
    {
        if (entries.size() == EntryLimit)
        {
            result[QStringLiteral("truncated")] = true;
            break;
        }
        const QFileInfo file = child.fileInfo();
        QString blocked;
        if (file.isSymLink())
            blocked = QStringLiteral("symbolic-link");
        else if (!file.exists() || !file.isReadable())
            blocked = QStringLiteral("child-unavailable");
        else if (!file.isDir() && !file.isFile())
            blocked = QStringLiteral("unsupported-file-type");
        else if (file.isFile() && (file.isExecutable()
                 || file.suffix().compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0
                 || file.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0))
            blocked = QStringLiteral("executable-entry");

        const QString url = QUrl::fromLocalFile(file.absoluteFilePath())
                                .toString(QUrl::FullyEncoded);
        entries.append(QVariantMap{
            {QStringLiteral("id"), QStringLiteral("folder-child:") + url},
            {QStringLiteral("url"), url},
            {QStringLiteral("name"), file.fileName()},
            {QStringLiteral("displayName"), file.fileName()},
            {QStringLiteral("isDirectory"), file.isDir() && !file.isSymLink()},
            {QStringLiteral("iconName"), file.isDir() && !file.isSymLink()
                ? QStringLiteral("folder")
                : mimeDatabase.mimeTypeForFile(file, QMimeDatabase::MatchExtension).iconName()},
            {QStringLiteral("selectable"), blocked.isEmpty()},
            {QStringLiteral("blockedReason"), blocked}});
    }
    const QFileInfo after(folderUrl.toLocalFile());
    if (!after.isDir() || !after.isReadable() || !after.isExecutable()
        || after.canonicalFilePath() != root)
    {
        result[QStringLiteral("status")] = QStringLiteral("unavailable");
        result[QStringLiteral("errorCode")] = QStringLiteral("folder-changed");
        return result;
    }
    std::sort(entries.begin(), entries.end(), [](const QVariant &left, const QVariant &right) {
        const auto a = left.toMap();
        const auto b = right.toMap();
        if (a.value(QStringLiteral("isDirectory")) != b.value(QStringLiteral("isDirectory")))
            return a.value(QStringLiteral("isDirectory")).toBool();
        const int compared = QString::compare(a.value(QStringLiteral("name")).toString(),
                                              b.value(QStringLiteral("name")).toString(),
                                              Qt::CaseInsensitive);
        return compared ? compared < 0
            : a.value(QStringLiteral("id")).toString() < b.value(QStringLiteral("id")).toString();
    });
    result[QStringLiteral("status")] = entries.isEmpty()
        ? QStringLiteral("empty") : QStringLiteral("ready");
    result[QStringLiteral("errorCode")] = QString{};
    result[QStringLiteral("entries")] = entries;
    return result;
}

QUrl FolderContentModel::resolveChild(const QUrl &folderUrl, const QString &childId,
                                    QString *errorCode)
{
    const auto current = snapshot(folderUrl);
    QString error = current.value(QStringLiteral("errorCode")).toString();
    if (error.isEmpty())
    {
        error = QStringLiteral("child-not-listed");
        for (const auto &value : current.value(QStringLiteral("entries")).toList())
        {
            const auto entry = value.toMap();
            if (entry.value(QStringLiteral("id")).toString() != childId)
                continue;
            if (entry.value(QStringLiteral("selectable")).toBool())
            {
                if (errorCode)
                    errorCode->clear();
                return QUrl(entry.value(QStringLiteral("url")).toString());
            }
            error = entry.value(QStringLiteral("blockedReason")).toString();
            break;
        }
    }
    if (errorCode)
        *errorCode = error;
    return {};
}
}
