// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qwayland-kde-screen-edge-v1.h"

#include <QWaylandClientExtensionTemplate>

namespace ArchDock::Bars
{
// KWin's auto-hide screen edges (kde_screen_edge_v1, from Plasma's Wayland
// protocols): a bar that hides is hidden through one, and KWin shows it
// again when the pointer hits the screen edge along it, as it does for
// Plasma's own panels.
class ScreenEdgeManager
    : public QWaylandClientExtensionTemplate<ScreenEdgeManager, &QtWayland::kde_screen_edge_manager_v1::destroy>,
      public QtWayland::kde_screen_edge_manager_v1
{
public:
    explicit ScreenEdgeManager(QObject *parent = nullptr);
};

// One bar's auto-hide edge: hide() hides the bar until the screen edge is
// triggered (or show() is called), show() shows it. Destroyed, it shows the
// bar.
class AutoHideEdge : public QtWayland::kde_auto_hide_screen_edge_v1
{
public:
    explicit AutoHideEdge(::kde_auto_hide_screen_edge_v1 *edge);
    ~AutoHideEdge() override;

    void hide();
    void show();
};
}
