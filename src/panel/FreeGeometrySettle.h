// When a free panel's place, read back from Plasma after Arch Dock asked to
// move it, may end the wait.
#pragma once

#include <QtGlobal>

namespace ArchDock
{
// The widget moves itself a moment after the command reaches it (a refresh,
// then a queued call, then a short timer), so three equal readings of the
// right size can come before it has moved. When the command changes the
// place, the wait ends only once Plasma shows the place asked for, or after
// this long: a place the panel is kept from (in a panel's own room, which
// the widget keeps out of) is then taken as it stands.
inline constexpr qint64 kFreeMoveSettleMs = 400;

inline bool freeGeometrySettled(int stableSamples, bool movesPlace, bool atRequestedPlace, qint64 elapsedMs)
{
    if (stableSamples < 3)
        return false;
    return !movesPlace || atRequestedPlace || elapsedMs >= kFreeMoveSettleMs;
}
}
