#pragma once

#include <QString>
#include <QStringList>

class QScreen;

namespace ArchDock
{
QString persistentScreenId(const QScreen *screen);
int resolvedScreenIndex(const QStringList &screenIds,
                        const QString &requestedScreenId,
                        int fallbackIndex);
}