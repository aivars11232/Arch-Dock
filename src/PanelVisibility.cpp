#include "PanelVisibility.h"

namespace ArchDock
{
namespace
{
bool hasConcealmentLock(const PanelVisibilityLocks &locks)
{
    return locks.pointerInside || locks.revealZoneActive || locks.popupOpen ||
        locks.dragActive || locks.keyboardFocus || locks.editMode;
}

bool isRelevantWindow(const WindowOcclusion &window, int panelScreenIndex)
{
    return window.active && !window.minimized && window.frameGeometry.isValid() &&
        window.screenIndex == panelScreenIndex;
}
}

PanelVisibilityMode panelVisibilityModeFromString(const QString &visibilityMode)
{
    QString alias = visibilityMode.trimmed().toLower();
    alias.remove(QLatin1Char('-'));
    alias.remove(QLatin1Char('_'));
    alias.remove(QLatin1Char(' '));

    if (alias == QStringLiteral("autohide"))
    {
        return PanelVisibilityMode::AutoHide;
    }
    if (alias == QStringLiteral("dodge") || alias == QStringLiteral("dodgewindows") ||
        alias == QStringLiteral("dodgeactivewindow"))
    {
        return PanelVisibilityMode::DodgeActiveWindow;
    }
    if (alias == QStringLiteral("cover") || alias == QStringLiteral("hidemaximized") ||
        alias == QStringLiteral("hideundermaximized") ||
        alias == QStringLiteral("hideformaximizedorfullscreen"))
    {
        return PanelVisibilityMode::HideForMaximizedOrFullscreen;
    }
    return PanelVisibilityMode::AlwaysVisible;
}

PanelVisibilityDecision decidePanelVisibility(const PanelVisibilityInput &input)
{
    if (!input.panelGeometry.isValid() || input.panelScreenIndex < 0 ||
        hasConcealmentLock(input.locks))
    {
        return PanelVisibilityDecision::Reveal;
    }

    if (input.manualHideRequested)
    {
        return PanelVisibilityDecision::Conceal;
    }

    switch (input.mode)
    {
    case PanelVisibilityMode::AlwaysVisible:
        return PanelVisibilityDecision::Reveal;
    case PanelVisibilityMode::AutoHide:
        return PanelVisibilityDecision::Conceal;
    case PanelVisibilityMode::DodgeActiveWindow:
        for (const WindowOcclusion &window : input.windows)
        {
            if (isRelevantWindow(window, input.panelScreenIndex) &&
                window.frameGeometry.intersects(input.panelGeometry))
            {
                return PanelVisibilityDecision::Conceal;
            }
        }
        return PanelVisibilityDecision::Reveal;
    case PanelVisibilityMode::HideForMaximizedOrFullscreen:
        for (const WindowOcclusion &window : input.windows)
        {
            if (isRelevantWindow(window, input.panelScreenIndex) &&
                (window.maximized || window.fullScreen))
            {
                return PanelVisibilityDecision::Conceal;
            }
        }
        return PanelVisibilityDecision::Reveal;
    }

    return PanelVisibilityDecision::Reveal;
}

bool shouldConcealPanel(const PanelVisibilityInput &input)
{
    return decidePanelVisibility(input) == PanelVisibilityDecision::Conceal;
}

bool shouldConcealForWindows(const QString &visibilityMode,
                             const QRect &panelGeometry,
                             int panelScreenIndex,
                             const QList<WindowOcclusion> &windows)
{
    PanelVisibilityInput input;
    input.mode = panelVisibilityModeFromString(visibilityMode);
    input.panelGeometry = panelGeometry;
    input.panelScreenIndex = panelScreenIndex;
    input.windows = windows;
    return shouldConcealPanel(input);
}
}
