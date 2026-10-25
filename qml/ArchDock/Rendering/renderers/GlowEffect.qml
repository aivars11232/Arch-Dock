import QtQuick
import QtQuick.Effects

// Light round a flat item, as its layer effect: the item's own shape,
// blurred, in the glow's colour, behind it - a halo, not a brighter item.
// Intensity is how strong it is, from 0 to 1; reach how far it
// spreads out of the item, from about 8 to 64 pixels. The layer grows to
// hold the halo, so nothing round the item cuts it off.
MultiEffect {
    property color glowColor: "#a8dcff"
    property real intensity: 0
    property real reach: 0.75
    readonly property real boundedReach: Math.max(0, Math.min(1, reach))

    shadowEnabled: true
    shadowColor: glowColor
    shadowOpacity: Math.min(1, 0.3 + 0.7 * Math.max(0, Math.min(1, intensity)))
    shadowBlur: 1
    blurMax: Math.round(8 + 56 * boundedReach)
    shadowScale: 1 + 0.03 * boundedReach
    shadowHorizontalOffset: 0
    shadowVerticalOffset: 0
    autoPaddingEnabled: true
}
