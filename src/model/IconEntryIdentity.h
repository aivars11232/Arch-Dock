#pragma once

#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace ArchDock
{

// The identity of a dock entry that survives restarts and renames: an
// application's normalized desktop-entry id, or a free panel's pinned URL.
// Per-icon overrides (Icon Properties) are keyed by it.
class IconEntryIdentity final
{
public:
    [[nodiscard]] static QString forApplication(
        const QString &appId,
        const QString &desktopFileName = {});
    [[nodiscard]] static QString forFreeUrl(const QUrl &url);
    [[nodiscard]] static QString forEntry(const QVariantMap &entry);
    [[nodiscard]] static bool isValid(const QString &identity);

private:
    [[nodiscard]] static QString normalizedDesktopEntryId(
        const QString &identifier);
    [[nodiscard]] static QString namespacedIdentity(
        const QString &nameSpace,
        const QString &value);
};

}
