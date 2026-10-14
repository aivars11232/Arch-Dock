// PanelIconCapacity: how many icons fit a panel's shape at a 16 pixel gap.
// The shapes' lengths follow LayoutEngine.js (curvedTrack, starPoint).
#include "PanelIconCapacity.h"

#include "PanelDefinition.h"

#include <QList>
#include <QPointF>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace
{
// A track's length for a radius of 1, and whether it closes on itself;
// nullopt for a layout that is not one track. A 3D panel stretched sideways
// or up and down (`stretchX`, `stretchY`) moves where its icons stand, not
// the icons, so its track is measured stretched.
struct Track
{
    double perRadius = 0;
    bool closed = true;
    // A Möbius band's two loops cross: each keeps a slot's gap at both of
    // its ends there, two slots in all (LayoutEngine.lobeProgress).
    int crossingGaps = 0;
};

// A Möbius band's middle line as LayoutEngine.mobiusLine() draws it: a
// lemniscate of Bernoulli made a little fuller, reaching 1.25 of the radius
// either way.
constexpr double mobiusFullness = 1.15;
constexpr double mobiusReach = 1.25;

std::optional<Track> trackFor(const QString &layout, int polygonSides, double stretchX, double stretchY,
                              double seen)
{
    QList<QPointF> outline;
    bool closed = true;
    const auto onCircle = [&outline](double fromDegrees, double sweepDegrees, int steps) {
        for (int step = 0; step <= steps; ++step)
        {
            const double turn = (fromDegrees + sweepDegrees * step / steps) * M_PI / 180;
            outline.append(QPointF(std::cos(turn), std::sin(turn)));
        }
    };
    static const QStringList polygons{QStringLiteral("polygon"), QStringLiteral("triangle"),
        QStringLiteral("square"), QStringLiteral("pentagon"), QStringLiteral("hexagon"),
        QStringLiteral("octagon")};
    int crossingGaps = 0;
    if (layout == QStringLiteral("circular") || layout == QStringLiteral("ring")
        || layout == QStringLiteral("ellipse"))
    {
        onCircle(0, 360, 720);
    }
    else if (layout == QStringLiteral("mobius"))
    {
        for (int step = 0; step <= 720; ++step)
        {
            const double turn = M_PI / 2 - 2 * M_PI * step / 720;
            const double sine = std::sin(turn), cosine = std::cos(turn), divisor = 1 + sine * sine;
            outline.append(QPointF(mobiusReach * cosine / divisor,
                                   mobiusReach * mobiusFullness * sine * cosine / divisor));
        }
        crossingGaps = 2;
    }
    else if (polygons.contains(layout) || layout == QStringLiteral("star"))
    {
        int sides = std::clamp(polygonSides, 3, 12);
        if (layout == QStringLiteral("triangle")) sides = 3;
        if (layout == QStringLiteral("square")) sides = 4;
        if (layout == QStringLiteral("pentagon")) sides = 5;
        if (layout == QStringLiteral("hexagon")) sides = 6;
        if (layout == QStringLiteral("octagon")) sides = 8;
        // A star's points stand on the radius, its notches at 0.46 of it.
        const bool star = layout == QStringLiteral("star");
        const int corners = star ? 2 * sides : sides;
        for (int corner = 0; corner <= corners; ++corner)
        {
            const double reach = star && corner % 2 ? 0.46 : 1.0;
            const double turn = -M_PI / 2 + corner * 2 * M_PI / corners;
            outline.append(QPointF(reach * std::cos(turn), reach * std::sin(turn)));
        }
    }
    else
    {
        // The open paths' sweeps, as LayoutEngine.pathSweeps draws them.
        closed = false;
        if (layout == QStringLiteral("arc")) onCircle(205, 130, 260);
        else if (layout == QStringLiteral("semicircle")) onCircle(180, 180, 360);
        else if (layout == QStringLiteral("fan")) onCircle(-148, 116, 232);
        else if (layout == QStringLiteral("radial")) onCircle(-150, 300, 600);
        else return std::nullopt;
    }
    double length = 0;
    for (int index = 1; index < outline.size(); ++index)
        length += std::hypot((outline[index].x() - outline[index - 1].x()) * stretchX,
                             (outline[index].y() - outline[index - 1].y()) * stretchY);
    const double drawn = std::isfinite(seen) && seen > 0 ? seen : 1.0;
    // Round a Möbius band's tight loop ends neighbours a slot apart along it
    // stand a little closer in a straight line: 5 % of it is kept for that.
    if (crossingGaps > 0)
        length *= 0.95;
    return Track{length * drawn, closed, crossingGaps};
}

// How a panel stretches its track: a 3D panel's sideways and up-and-down
// scale, 1 elsewhere.
QPointF stretchOf(const ArchDock::PanelDefinition &definition)
{
    if (definition.surface.rendererTier != QStringLiteral("true3d"))
        return QPointF(1, 1);
    const auto value = [&definition](const char *key) {
        const double number = definition.surface.parameters3D.value(QLatin1String(key), 1.0).toDouble();
        return std::isfinite(number) && number > 0 ? number : 1.0;
    };
    return QPointF(value("scaleX"), value("scaleY"));
}

// The radius the icons are drawn at: never less than three quarters of an
// icon, as the layout draws it.
double drawnRadius(int radius, int iconSize)
{
    return std::max(0.75 * iconSize, double(radius));
}

// How many slots a panel's track holds, not rounded down: the room a change
// may not take from icons that already have too little.
std::optional<double> spaceOf(const ArchDock::PanelDefinition &definition, double seen)
{
    const QPointF stretch = stretchOf(definition);
    const auto track = trackFor(definition.layout.pathType, definition.layout.polygonSides,
                                stretch.x(), stretch.y(), seen);
    const int size = definition.iconStyle.size;
    if (!track || size <= 0)
        return std::nullopt;
    return track->perRadius * drawnRadius(definition.layout.radius, size) / (size + ArchDock::panelIconGap);
}
}

namespace ArchDock
{
std::optional<int> panelIconCapacity(const QString &layout, int radius, int iconSize, int polygonSides,
                                     double stretchX, double stretchY, double seen)
{
    const auto track = trackFor(layout, polygonSides, stretchX, stretchY, seen);
    if (!track || iconSize <= 0)
        return std::nullopt;
    const double fitting = track->perRadius * drawnRadius(radius, iconSize) / (iconSize + panelIconGap);
    return std::max(1, int(std::floor(fitting + 1e-9)) + (track->closed ? 0 : 1) - track->crossingGaps);
}

std::optional<int> panelIconCapacity(const PanelDefinition &definition, double seen)
{
    const QPointF stretch = stretchOf(definition);
    return panelIconCapacity(definition.layout.pathType, definition.layout.radius,
                             definition.iconStyle.size, definition.layout.polygonSides,
                             stretch.x(), stretch.y(), seen);
}

std::optional<int> panelMinimumRadius(const QString &layout, int count, int iconSize, int polygonSides,
                                      double stretchX, double stretchY, double seen)
{
    const auto track = trackFor(layout, polygonSides, stretchX, stretchY, seen);
    if (!track || iconSize <= 0 || count <= 0)
        return std::nullopt;
    const int spans = (track->closed ? count : count - 1) + track->crossingGaps;
    if (spans <= 0)
        return 0;
    return int(std::ceil(spans * double(iconSize + panelIconGap) / track->perRadius - 1e-9));
}

std::optional<int> panelMaximumIconSize(const QString &layout, int count, int radius, int polygonSides,
                                        double stretchX, double stretchY, double seen)
{
    const auto track = trackFor(layout, polygonSides, stretchX, stretchY, seen);
    if (!track || count <= 0)
        return std::nullopt;
    const int spans = (track->closed ? count : count - 1) + track->crossingGaps;
    if (spans <= 0)
        return std::nullopt;
    // The largest size whose slots fit, found from the top: the drawn radius
    // grows with a large icon too.
    for (int size = 512; size >= 1; --size)
        if (track->perRadius * drawnRadius(radius, size) / (size + panelIconGap) + 1e-9 >= spans)
            return size;
    return std::nullopt;
}

std::optional<int> panelMinimumRadius(const PanelDefinition &definition, double seen)
{
    const QPointF stretch = stretchOf(definition);
    return panelMinimumRadius(definition.layout.pathType, panelIconCount(definition), definition.iconStyle.size,
                              definition.layout.polygonSides, stretch.x(), stretch.y(), seen);
}

std::optional<int> panelMaximumIconSize(const PanelDefinition &definition, double seen)
{
    const QPointF stretch = stretchOf(definition);
    return panelMaximumIconSize(definition.layout.pathType, panelIconCount(definition), definition.layout.radius,
                                definition.layout.polygonSides, stretch.x(), stretch.y(), seen);
}

int panelIconCount(const PanelDefinition &definition)
{
    return int(definition.content.canonicalEntryOrder().size());
}

std::optional<QString> panelIconRoomRefusal(const PanelDefinition &before, const PanelDefinition &after,
                                            double seenBefore, double seenAfter)
{
    const int icons = panelIconCount(after);
    const std::optional<int> room = panelIconCapacity(after, seenAfter);
    const std::optional<double> space = spaceOf(after, seenAfter);
    const std::optional<double> had = spaceOf(before, seenBefore);
    if (!room || !space || icons <= *room || (had && *space >= *had - 1e-9))
        return std::nullopt;
    const auto radius = panelMinimumRadius(after, seenAfter);
    const auto size = panelMaximumIconSize(after, seenAfter);
    return QStringLiteral("its %1 icons fit only %2 here: they need a radius of %3 or more, "
                          "or icons of %4 or less")
        .arg(icons).arg(*room)
        .arg(radius ? QString::number(*radius) : QStringLiteral("-"))
        .arg(size ? QString::number(*size) : QStringLiteral("-"));
}

void growPanelToFitIcons(const PanelDefinition &before, PanelDefinition &after,
                         double seenBefore, double seenAfter)
{
    if (!panelIconRoomRefusal(before, after, seenBefore, seenAfter))
        return;
    if (const auto least = panelMinimumRadius(after, seenAfter))
        after.layout.radius = std::max(after.layout.radius, *least);
}
}
