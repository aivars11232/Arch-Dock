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

enum class PanelVisibilityMode
{
    AlwaysVisible,
    AutoHide,
    DodgeActiveWindow,
    HideForMaximizedOrFullscreen,
};

enum class PanelVisibilityDecision
{
    Reveal,
    Conceal,
};

struct PanelVisibilityLocks
{
    bool pointerInside = false;
    bool revealZoneActive = false;
    bool popupOpen = false;
    bool dragActive = false;
    bool keyboardFocus = false;
    bool editMode = false;
};

struct PanelVisibilityInput
{
    PanelVisibilityMode mode = PanelVisibilityMode::AlwaysVisible;
    QRect panelGeometry;
    int panelScreenIndex = -1;
    QList<WindowOcclusion> windows;
    PanelVisibilityLocks locks;
    bool manualHideRequested = false;
};

[[nodiscard]] PanelVisibilityMode panelVisibilityModeFromString(const QString &visibilityMode);
[[nodiscard]] PanelVisibilityDecision decidePanelVisibility(
    const PanelVisibilityInput &input);
[[nodiscard]] bool shouldConcealPanel(const PanelVisibilityInput &input);

bool shouldConcealForWindows(const QString &visibilityMode,
                             const QRect &panelGeometry,
                             int panelScreenIndex,
                             const QList<WindowOcclusion> &windows);
}
