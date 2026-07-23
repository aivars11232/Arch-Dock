#include "ScreenIdentity.h"

#include <QCryptographicHash>
#include <QScreen>
#include <QtGlobal>

namespace ArchDock
{
QString persistentScreenId(const QScreen *screen)
{
    if (!screen)
    {
        return {};
    }

    const QString serial = screen->serialNumber().trimmed();
    if (!serial.isEmpty())
    {
        const QByteArray identity = (screen->manufacturer().trimmed() + QChar::Null +
                                     screen->model().trimmed() + QChar::Null + serial)
                                        .toUtf8();
        return QStringLiteral("edid:") + QCryptographicHash::hash(
            identity,
            QCryptographicHash::Sha256).toHex();
    }

    const QString outputName = screen->name().trimmed();
    return outputName.isEmpty() ? QString{} : QStringLiteral("output:") + outputName;
}

int resolvedScreenIndex(const QStringList &screenIds,
                        const QString &requestedScreenId,
                        int fallbackIndex)
{
    if (screenIds.isEmpty())
    {
        return -1;
    }

    const int boundedFallback = qBound(0, fallbackIndex, screenIds.size() - 1);
    if (requestedScreenId.isEmpty())
    {
        return boundedFallback;
    }

    if (screenIds.at(boundedFallback) == requestedScreenId)
    {
        return boundedFallback;
    }

    const int matchedIndex = screenIds.indexOf(requestedScreenId);
    return matchedIndex >= 0 ? matchedIndex : boundedFallback;
}
}