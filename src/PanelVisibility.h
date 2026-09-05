#pragma once

#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

#include <optional>

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

enum class PlasmaPanelHidingMode
{
    None,
    AutoHide,
    DodgeWindows,
};

struct NativeVisibilityCapabilities
{
    bool autoHide = false;
    bool dodgeWindows = false;
    bool coverController = false;
};

struct NativeVisibilityResolution
{
    PanelVisibilityMode requestedMode = PanelVisibilityMode::AlwaysVisible;
    PanelVisibilityMode effectiveMode = PanelVisibilityMode::AlwaysVisible;
    PlasmaPanelHidingMode hostMode = PlasmaPanelHidingMode::None;
    bool supported = true;
    bool fallbackApplied = false;
    QString errorCode;
};

struct PanelVisibilityLocks
{
    bool pointerInside = false;
    bool revealZoneActive = false;
    bool popupOpen = false;
    bool dragActive = false;
    bool keyboardFocus = false;
    bool editMode = false;

    bool operator==(const PanelVisibilityLocks &) const = default;
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
[[nodiscard]] std::optional<PanelVisibilityMode> normalizedPanelVisibilityMode(
    const QString &visibilityMode);
[[nodiscard]] QString panelVisibilityModeToString(PanelVisibilityMode visibilityMode);
[[nodiscard]] QString plasmaPanelHidingModeToString(PlasmaPanelHidingMode hidingMode);
[[nodiscard]] QStringList supportedNativeVisibilityModes(
    const NativeVisibilityCapabilities &capabilities);
[[nodiscard]] NativeVisibilityResolution resolveNativeVisibility(
    PanelVisibilityMode requestedMode,
    PanelVisibilityDecision controllerDecision,
    bool manualHideRequested,
    const NativeVisibilityCapabilities &capabilities);
[[nodiscard]] PanelVisibilityDecision decidePanelVisibility(
    const PanelVisibilityInput &input);
[[nodiscard]] bool shouldConcealPanel(const PanelVisibilityInput &input);

bool shouldConcealForWindows(const QString &visibilityMode,
                             const QRect &panelGeometry,
                             int panelScreenIndex,
                             const QList<WindowOcclusion> &windows);
}
