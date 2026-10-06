#pragma once

#include <QVariantMap>

#include <optional>

namespace ArchDock::LookPalette
{
// The colours a look is drawn in, read from its own artwork, so a 3D platform
// generated for the look is recognisably that look. `projection` is the
// look's theme runtime projection: its platform layers (roles "rear" and
// "foreground") are decoded from `assetPaths` and composed in order.
//
// Returns { top, wall, under, rim } as [r, g, b, a] lists in 0..1 (the
// colour most of the platform is, a darker shade of it, a darker one still,
// and its brightest edge), plus `source: "artwork"`. Nothing when the look
// has no platform artwork Qt can decode.
[[nodiscard]] std::optional<QVariantMap> fromArtwork(const QVariantMap &projection);
}
