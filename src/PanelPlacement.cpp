#include "PanelPlacement.h"

#include <QtGlobal>

namespace ArchDock
{
int edgeReserve(const QList<EdgePanel> &panels, const QString &edge, int screenIndex)
{
    int reserve = 8;
    for (const EdgePanel &panel : panels)
    {
        if (panel.visible && panel.edge == edge && panel.screenIndex == screenIndex)
        {
            reserve += qMax(0, panel.thickness) + 8;
        }
    }
    return reserve;
}

int edgeOffset(const QList<EdgePanel> &panels, const QString &panelId)
{
    const auto current = std::find_if(
        panels.cbegin(),
        panels.cend(),
        [&panelId](const EdgePanel &panel)
        {
            return panel.id == panelId;
        });
    if (current == panels.cend())
    {
        return 8;
    }

    int offset = 8;
    for (auto panel = panels.cbegin(); panel != current; ++panel)
    {
        if (panel->visible && panel->edge == current->edge &&
            panel->screenIndex == current->screenIndex)
        {
            offset += qMax(0, panel->thickness) + 8;
        }
    }
    return offset;
}
}