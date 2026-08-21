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

ScreenResolution resolveScreen(const QStringList &screenIds,
                               const QString &requestedScreenId,
                               int fallbackIndex)
{
    if (screenIds.isEmpty())
    {
        return {};
    }

    const int boundedFallback = qBound(0, fallbackIndex, screenIds.size() - 1);
    if (!requestedScreenId.isEmpty())
    {
        if (screenIds.at(boundedFallback) == requestedScreenId)
        {
            return {boundedFallback, ScreenResolutionReason::StableIdMatch, false};
        }

        const int matchedIndex = screenIds.indexOf(requestedScreenId);
        if (matchedIndex >= 0)
        {
            return {matchedIndex, ScreenResolutionReason::StableIdMatch, false};
        }
    }

    const ScreenResolutionReason reason = fallbackIndex == boundedFallback
        ? ScreenResolutionReason::StoredIndexFallback
        : ScreenResolutionReason::BoundedIndexFallback;
    return {boundedFallback, reason, true};
}

int resolvedScreenIndex(const QStringList &screenIds,
                        const QString &requestedScreenId,
                        int fallbackIndex)
{
    return resolveScreen(screenIds, requestedScreenId, fallbackIndex).index;
}
}
