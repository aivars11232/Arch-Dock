#include "WindowPreviewModel.h"

#include <QCoreApplication>
#include <QVariantMap>

QVariantList ArchDock::WindowPreviewModel::entries(
    const QList<WindowItem> &windows, const QString &applicationTitle)
{
    QVariantList result;
    result.reserve(windows.size());
    const QString fallback = applicationTitle.trimmed().isEmpty()
        ? QCoreApplication::translate("WindowPreviewModel", "Untitled window")
        : applicationTitle;
    for (const WindowItem &window : windows)
    {
        if (window.internalId.isEmpty())
        {
            continue;
        }
        result.append(QVariantMap{
            {QStringLiteral("windowId"), window.internalId},
            {QStringLiteral("title"), window.caption.trimmed().isEmpty()
                 ? fallback : window.caption},
            {QStringLiteral("iconName"), window.iconName},
            {QStringLiteral("active"), window.active},
            {QStringLiteral("minimized"), window.minimized},
            {QStringLiteral("canActivate"), window.canActivate},
            {QStringLiteral("canMinimize"), window.canMinimize},
            {QStringLiteral("canClose"), window.canClose}});
    }
    return result;
}
