#include "PanelVisibility.h"

namespace ArchDock
{
bool shouldConcealForWindows(const QString &visibilityMode,
                             const QRect &panelGeometry,
                             int panelScreenIndex,
                             const QList<WindowOcclusion> &windows)
{
    if ((visibilityMode != QStringLiteral("dodge") && visibilityMode != QStringLiteral("cover")) ||
        !panelGeometry.isValid() || panelScreenIndex < 0)
    {
        return false;
    }

    for (const WindowOcclusion &window : windows)
    {
        if (!window.active || window.minimized || window.screenIndex != panelScreenIndex)
        {
            continue;
        }

        if (visibilityMode == QStringLiteral("cover"))
        {
            if (window.maximized || window.fullScreen || window.frameGeometry.contains(panelGeometry.center()))
            {
                return true;
            }
            continue;
        }

        if (window.frameGeometry.intersects(panelGeometry))
        {
            return true;
        }
    }
    return false;
}
}