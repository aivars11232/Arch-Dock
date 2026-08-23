#include "PanelVisibility.h"

namespace ArchDock
{
namespace
{
QString normalizedModeAlias(const QString &visibilityMode)
{
    QString alias = visibilityMode.trimmed().toLower();
    alias.remove(QLatin1Char('-'));
    alias.remove(QLatin1Char('_'));
    alias.remove(QLatin1Char(' '));
    return alias;
}

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
    return normalizedPanelVisibilityMode(visibilityMode).value_or(
        PanelVisibilityMode::AlwaysVisible);
}

std::optional<PanelVisibilityMode> normalizedPanelVisibilityMode(
    const QString &visibilityMode)
{
    const QString alias = normalizedModeAlias(visibilityMode);

    if (alias == QStringLiteral("always") || alias == QStringLiteral("alwaysvisible") ||
        alias == QStringLiteral("none"))
    {
        return PanelVisibilityMode::AlwaysVisible;
    }
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
    return std::nullopt;
}

QString panelVisibilityModeToString(PanelVisibilityMode visibilityMode)
{
    switch (visibilityMode)
    {
    case PanelVisibilityMode::AlwaysVisible:
        return QStringLiteral("always");
    case PanelVisibilityMode::AutoHide:
        return QStringLiteral("auto-hide");
    case PanelVisibilityMode::DodgeActiveWindow:
        return QStringLiteral("dodge");
    case PanelVisibilityMode::HideForMaximizedOrFullscreen:
        return QStringLiteral("cover");
    }

    return QStringLiteral("always");
}

QString plasmaPanelHidingModeToString(PlasmaPanelHidingMode hidingMode)
{
    switch (hidingMode)
    {
    case PlasmaPanelHidingMode::None:
        return QStringLiteral("none");
    case PlasmaPanelHidingMode::AutoHide:
        return QStringLiteral("autohide");
    case PlasmaPanelHidingMode::DodgeWindows:
        return QStringLiteral("dodgewindows");
    }

    return QStringLiteral("none");
}

QStringList supportedNativeVisibilityModes(
    const NativeVisibilityCapabilities &capabilities)
{
    QStringList modes{QStringLiteral("always")};
    if (capabilities.autoHide)
    {
        modes.append(QStringLiteral("auto-hide"));
    }
    if (capabilities.dodgeWindows)
    {
        modes.append(QStringLiteral("dodge"));
    }
    if (capabilities.autoHide && capabilities.coverController)
    {
        modes.append(QStringLiteral("cover"));
    }
    return modes;
}

NativeVisibilityResolution resolveNativeVisibility(
    PanelVisibilityMode requestedMode,
    PanelVisibilityDecision controllerDecision,
    bool manualHideRequested,
    const NativeVisibilityCapabilities &capabilities)
{
    NativeVisibilityResolution result;
    result.requestedMode = requestedMode;
    result.effectiveMode = requestedMode;

    const auto applyFallback = [&result](const QString &errorCode) {
        result.effectiveMode = PanelVisibilityMode::AlwaysVisible;
        result.hostMode = PlasmaPanelHidingMode::None;
        result.supported = false;
        result.fallbackApplied = true;
        result.errorCode = errorCode;
    };

    switch (requestedMode)
    {
    case PanelVisibilityMode::AlwaysVisible:
        result.hostMode = PlasmaPanelHidingMode::None;
        break;
    case PanelVisibilityMode::AutoHide:
        if (!capabilities.autoHide)
        {
            applyFallback(QStringLiteral("auto-hide-unsupported"));
            return result;
        }
        result.hostMode = PlasmaPanelHidingMode::AutoHide;
        break;
    case PanelVisibilityMode::DodgeActiveWindow:
        if (!capabilities.dodgeWindows)
        {
            applyFallback(QStringLiteral("dodge-windows-unsupported"));
            return result;
        }
        result.hostMode = PlasmaPanelHidingMode::DodgeWindows;
        break;
    case PanelVisibilityMode::HideForMaximizedOrFullscreen:
        if (!capabilities.autoHide || !capabilities.coverController)
        {
            applyFallback(QStringLiteral("cover-controller-unsupported"));
            return result;
        }
        result.hostMode = controllerDecision == PanelVisibilityDecision::Conceal
            ? PlasmaPanelHidingMode::AutoHide
            : PlasmaPanelHidingMode::None;
        break;
    }

    if (manualHideRequested)
    {
        if (!capabilities.autoHide)
        {
            applyFallback(QStringLiteral("manual-hide-unsupported"));
            return result;
        }
        result.hostMode = PlasmaPanelHidingMode::AutoHide;
    }

    return result;
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
