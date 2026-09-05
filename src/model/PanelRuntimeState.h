#pragma once

#include <QRect>
#include <QString>
#include <QVariantMap>

namespace ArchDock
{

enum class PanelTransitionState
{
    Idle,
    Opening,
    Closing,
    // Host visibility, deliberately part of the same transient record but
    // never reported to a surface renderer as an open/collapsed
    // interpolation. Concealing a panel is not collapsing it.
    Concealing,
    Revealing
};

// What the panel surface is drawing. Independent of whether the host is
// showing the panel at all, so a panel can be host-visible and collapsed.
enum class PanelSurfaceState
{
    Open,
    Collapsed
};

struct PanelRuntimeState
{
    bool hovered = false;
    int hoveredEntry = -1;
    bool editMode = false;
    bool popupOpen = false;
    bool dragInProgress = false;
    PanelSurfaceState surfaceState = PanelSurfaceState::Open;
    bool hostConcealed = false;
    PanelTransitionState transition = PanelTransitionState::Idle;
    qreal presentationProgress = -1.0;
    bool windowOverlap = false;
    // The whole-scene rotation offset a free panel is currently drawn at, in
    // degrees. Transient: a restart draws the configured layout angle.
    qreal sceneRotation = 0.0;
    QString rendererFallback;
    QString frameQuality = QStringLiteral("normal");
    QRect currentScreenGeometry;

    [[nodiscard]] static PanelRuntimeState defaults();
    [[nodiscard]] QVariantMap toRuntimeMap() const;
    [[nodiscard]] static bool isTransientLegacyKey(const QString &key);
    [[nodiscard]] static QString transitionName(PanelTransitionState state);
    [[nodiscard]] static QString surfaceStateName(PanelSurfaceState state);

    bool operator==(const PanelRuntimeState &) const = default;
};

}
