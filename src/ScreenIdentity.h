#pragma once

#include <QString>
#include <QStringList>

class QScreen;

// Which screen a panel belongs on, across reboots and reconnections. A screen
// is named by a hash of its EDID manufacturer, model and serial, or by its
// output name when it has none; a saved panel finds its screen by that name
// first and falls back to its saved index.
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
