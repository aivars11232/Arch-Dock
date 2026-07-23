#pragma once

#include <QList>
#include <QRect>
#include <QString>

namespace ArchDock
{
struct WindowOcclusion
{
    QRect frameGeometry;
    int screenIndex = -1;
    bool active = false;
    bool minimized = false;
    bool maximized = false;
    bool fullScreen = false;
};

bool shouldConcealForWindows(const QString &visibilityMode,
                             const QRect &panelGeometry,
                             int panelScreenIndex,
                             const QList<WindowOcclusion> &windows);
}