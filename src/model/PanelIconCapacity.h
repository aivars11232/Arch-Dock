#pragma once

#include <QString>

#include <optional>

namespace ArchDock
{
struct PanelDefinition;

// How many icons a panel's shape holds side by side. Each icon's slot is its
// own size and a 16 pixel gap - the gap between the icons of a Plasma
// panel, kept at every icon size, so bigger icons fit fewer. A ring or
// a polygon holds its outline's length in slots, an open arc one more, a
// star the length of its outline, a Möbius band two fewer than its length
// holds, for the gaps where its loops cross. A row, a grid or a spiral grows with its
// icons and has no limit (nullopt). The layout's scale changes the icons and
// the radius alike, so the count does not depend on it. `seen` is how much of
// its track the icons get as drawn (a 3D panel's tilt shortens it), 1 for a
// panel drawn at its own size.
inline constexpr int panelIconGap = 16;

[[nodiscard]] std::optional<int> panelIconCapacity(const QString &layout, int radius,
                                                   int iconSize, int polygonSides,
                                                   double stretchX = 1, double stretchY = 1,
                                                   double seen = 1);
[[nodiscard]] std::optional<int> panelIconCapacity(const PanelDefinition &definition, double seen = 1);
// The least radius at which `count` icons of `iconSize` fit `layout`, and
// the largest icon size at which they fit at `radius`; nullopt where the
// layout has no limit or nothing fits.
[[nodiscard]] std::optional<int> panelMinimumRadius(const QString &layout, int count,
                                                    int iconSize, int polygonSides,
                                                    double stretchX = 1, double stretchY = 1,
                                                    double seen = 1);
[[nodiscard]] std::optional<int> panelMaximumIconSize(const QString &layout, int count,
                                                      int radius, int polygonSides,
                                                      double stretchX = 1, double stretchY = 1,
                                                      double seen = 1);
// The limits for a panel as it is: its layout, its icons and on a 3D panel
// its stretch.
[[nodiscard]] std::optional<int> panelMinimumRadius(const PanelDefinition &definition, double seen = 1);
[[nodiscard]] std::optional<int> panelMaximumIconSize(const PanelDefinition &definition, double seen = 1);
// How many icons a panel shows: its own entries.
[[nodiscard]] int panelIconCount(const PanelDefinition &definition);
// Why a change from `before` to `after` leaves the panel's icons less room
// than they need, or nullopt when it does not. A change may not make a panel
// smaller or its icons bigger than its own icons fit; a panel that already
// held more keeps them, and may only be given more room. Each is measured
// with its own `seen`.
[[nodiscard]] std::optional<QString> panelIconRoomRefusal(const PanelDefinition &before,
                                                          const PanelDefinition &after,
                                                          double seenBefore = 1, double seenAfter = 1);
// Grows `after`'s radius to the least its icons fit at, where a change from
// `before` would leave them too little room (a look with bigger icons).
void growPanelToFitIcons(const PanelDefinition &before, PanelDefinition &after,
                         double seenBefore = 1, double seenAfter = 1);
}
