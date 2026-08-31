#pragma once

#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace ArchDock
{

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
