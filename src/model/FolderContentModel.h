#pragma once

#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace ArchDock
{
// Value snapshots serve both native application entries and free URL entries.
// No recursive traversal, launch policy, or second panel-content store lives here.
class FolderContentModel final
{
public:
    static constexpr int EntryLimit = 48;

    static QVariantMap snapshot(const QUrl &folderUrl);
    static QUrl resolveChild(const QUrl &folderUrl, const QString &childId,
                             QString *errorCode = nullptr);
};
}
