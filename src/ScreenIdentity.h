#pragma once

#include <QString>
#include <QStringList>

class QScreen;

namespace ArchDock
{
enum class ScreenResolutionReason
{
    NoScreens,
    StableIdMatch,
    StoredIndexFallback,
    BoundedIndexFallback,
};

struct ScreenResolution
{
    int index = -1;
    ScreenResolutionReason reason = ScreenResolutionReason::NoScreens;
    bool usedFallback = false;
};

QString persistentScreenId(const QScreen *screen);
ScreenResolution resolveScreen(const QStringList &screenIds,
                               const QString &requestedScreenId,
                               int fallbackIndex);
int resolvedScreenIndex(const QStringList &screenIds,
                        const QString &requestedScreenId,
                        int fallbackIndex);
}
