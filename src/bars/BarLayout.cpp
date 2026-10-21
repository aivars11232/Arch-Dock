// SPDX-License-Identifier: GPL-3.0-or-later
#include "BarLayout.h"

#include <algorithm>

namespace ArchDock::Bars
{
namespace
{
QList<BarRequest> group(const QList<BarRequest> &bars, const QString &alignment)
{
    QList<BarRequest> result;
    for (const BarRequest &bar : bars)
    {
        const QString wanted = bar.alignment == QStringLiteral("start") || bar.alignment == QStringLiteral("end")
            ? bar.alignment : QStringLiteral("center");
        if (wanted == alignment)
            result.append(bar);
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const BarRequest &a, const BarRequest &b) { return a.order < b.order; });
    return result;
}
}

QList<BarPlacement> placeOnEdge(QList<BarRequest> bars, int edgeLength, int gap, int margin)
{
    const int first = std::max(0, margin);
    const int last = std::max(first, edgeLength - std::max(0, margin));
    gap = std::max(0, gap);
    QList<BarPlacement> placed;

    // From the start, each after the one before.
    int cursor = first;
    for (const BarRequest &bar : group(bars, QStringLiteral("start")))
    {
        const int start = std::clamp(cursor + std::max(0, bar.offset), first, last);
        const int length = std::clamp(bar.length, 0, last - start);
        placed.append({bar.id, start, length});
        cursor = start + length + gap;
    }
    const int startTaken = std::min(cursor, last);

    // From the end, each before the one after; never into the start group.
    const QList<BarRequest> ends = group(bars, QStringLiteral("end"));
    cursor = last;
    QList<BarPlacement> endPlaced;
    for (auto it = ends.crbegin(); it != ends.crend(); ++it)
    {
        const int end = std::clamp(cursor - std::max(0, it->offset), startTaken, last);
        const int length = std::clamp(it->length, 0, end - startTaken);
        endPlaced.prepend({it->id, end - length, length});
        cursor = end - length - gap;
    }
    placed.append(endPlaced);
    const int endTaken = std::max(startTaken, cursor);

    // The middle group centred and moved by its first bar's offset, kept in
    // the room the others leave.
    const QList<BarRequest> middle = group(bars, QStringLiteral("center"));
    if (!middle.isEmpty())
    {
        int wanted = 0;
        for (int index = 0; index < middle.size(); ++index)
        {
            wanted += std::max(0, middle.at(index).length);
            if (index > 0)
                wanted += gap + std::max(0, middle.at(index).offset);
        }
        int start = (edgeLength - wanted) / 2 + middle.first().offset;
        start = std::clamp(start, startTaken, std::max(startTaken, endTaken - wanted));
        for (int index = 0; index < middle.size(); ++index)
        {
            const BarRequest &bar = middle.at(index);
            if (index > 0)
                start = std::min(endTaken, start + std::max(0, bar.offset));
            const int length = std::clamp(bar.length, 0, std::max(0, endTaken - start));
            placed.append({bar.id, start, length});
            start = std::min(endTaken, start + length + gap);
        }
    }
    return placed;
}
}
