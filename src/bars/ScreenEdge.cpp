// SPDX-License-Identifier: GPL-3.0-or-later
#include "ScreenEdge.h"

namespace ArchDock::Bars
{
ScreenEdgeManager::ScreenEdgeManager(QObject *parent)
    : QWaylandClientExtensionTemplate(1, parent)
{
    initialize();
}

AutoHideEdge::AutoHideEdge(::kde_auto_hide_screen_edge_v1 *edge)
    : QtWayland::kde_auto_hide_screen_edge_v1(edge)
{
}

AutoHideEdge::~AutoHideEdge()
{
    destroy();
}

void AutoHideEdge::hide()
{
    activate();
}

void AutoHideEdge::show()
{
    deactivate();
}
}
