// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>

namespace ArchDock::Bars
{
// One bar asking for room along a screen edge.
struct BarRequest
{
    QString id;
    // Where along the edge it belongs: "start" (left, or top on a side
    // edge), "center" or "end".
    QString alignment = QStringLiteral("center");
    // Its place among the bars with the same alignment, nearer the edge's
    // start first.
    int order = 0;
    // How long it is along the edge, in pixels.
    int length = 0;
    // How far it is moved from where its alignment puts it: away from the
    // edge's start for a start bar, away from the end for an end bar, and
    // towards the end from the middle for a middle bar, as Plasma moves a
    // panel by its offset.
    int offset = 0;
};

// Where a bar stands along its edge: its start, from the edge's start, and
// how long it is there.
struct BarPlacement
{
    QString id;
    int start = 0;
    int length = 0;

    bool operator==(const BarPlacement &) const = default;
};

// Places one screen edge's bars side by side, `gap` apart and `margin` in
// from the edge's ends: those aligned to the start from the start, those to
// the end from the end, the middle ones centred as one group, moved aside
// where the others already stand. Nothing is placed past the edge: a bar
// that does not fit is shortened to the room left, down to nothing.
QList<BarPlacement> placeOnEdge(QList<BarRequest> bars, int edgeLength, int gap = 8, int margin = 0);
}
