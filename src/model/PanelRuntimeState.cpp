#include "PanelRuntimeState.h"

#include <QSet>

namespace ArchDock
{

PanelRuntimeState PanelRuntimeState::defaults()
{
    return {};
}

QVariantMap PanelRuntimeState::toRuntimeMap() const
{
    return {
        {QStringLiteral("hovered"), hovered},
        {QStringLiteral("hoveredEntry"), hoveredEntry},
        {QStringLiteral("editMode"), editMode},
        {QStringLiteral("popupOpen"), popupOpen},
        {QStringLiteral("dragInProgress"), dragInProgress},
        {QStringLiteral("transitionState"), transitionName(transition)},
        {QStringLiteral("windowOverlap"), windowOverlap},
        {QStringLiteral("rendererFallback"), rendererFallback},
        {QStringLiteral("frameQuality"), frameQuality},
        {QStringLiteral("currentScreenGeometry"), currentScreenGeometry},
    };
}

bool PanelRuntimeState::isTransientLegacyKey(const QString &key)
{
    static const QSet<QString> transientKeys{
        QStringLiteral("hover"),
        QStringLiteral("hovered"),
        QStringLiteral("hoveredentry"),
        QStringLiteral("hoveredindex"),
        QStringLiteral("editmode"),
        QStringLiteral("editing"),
        QStringLiteral("popupopen"),
        QStringLiteral("popupvisible"),
        QStringLiteral("draginprogress"),
        QStringLiteral("dragging"),
        QStringLiteral("moving"),
        QStringLiteral("initialized"),
        QStringLiteral("transitionstate"),
        QStringLiteral("currenttransition"),
        QStringLiteral("windowoverlap"),
        QStringLiteral("overlap"),
        QStringLiteral("rendererfallback"),
        QStringLiteral("currentrendererfallback"),
        QStringLiteral("framequality"),
        QStringLiteral("performancequality"),
        QStringLiteral("currentscreengeometry"),
        QStringLiteral("screengeometry"),
    };
    return transientKeys.contains(key.trimmed().toLower());
}

QString PanelRuntimeState::transitionName(PanelTransitionState state)
{
    switch (state)
    {
    case PanelTransitionState::Opening:
        return QStringLiteral("opening");
    case PanelTransitionState::Closing:
        return QStringLiteral("closing");
    case PanelTransitionState::Idle:
        return QStringLiteral("idle");
    }
    return QStringLiteral("idle");
}

}
