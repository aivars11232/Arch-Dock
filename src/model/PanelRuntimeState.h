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
    Closing
};

struct PanelRuntimeState
{
    bool hovered = false;
    int hoveredEntry = -1;
    bool editMode = false;
    bool popupOpen = false;
    bool dragInProgress = false;
    PanelTransitionState transition = PanelTransitionState::Idle;
    bool windowOverlap = false;
    QString rendererFallback;
    QString frameQuality = QStringLiteral("normal");
    QRect currentScreenGeometry;

    [[nodiscard]] static PanelRuntimeState defaults();
    [[nodiscard]] QVariantMap toRuntimeMap() const;
    [[nodiscard]] static bool isTransientLegacyKey(const QString &key);
    [[nodiscard]] static QString transitionName(PanelTransitionState state);

    bool operator==(const PanelRuntimeState &) const = default;
};

}
