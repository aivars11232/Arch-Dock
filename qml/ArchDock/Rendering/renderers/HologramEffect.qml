import QtQuick
import QtQuick.Effects

// A flat item drawn as a hologram (an icon's picture, a stand, a panel), as
// its layer effect or over a picture of it: its own shapes as light over a
// glow of them, each part in its colour or in its own (HologramLook's
// colours), crossed by drifting scan lines and flickering as a weak signal
// does (shaders/hologram.frag). The glow is the item blurred, kept while
// the item does not change; only the light is drawn anew as it moves.
Item {
    id: effect

    property Item source: null
    // HologramLook.colours(...) for the part.
    property var colours: ({})
    property real time: 0
    property real flicker: 0
    property real glow: 1
    property real glowReach: 0.75
    // The glow a little larger than the item, as far as it reaches.
    property real glowScale: 1
    // The scan lines' spacing, in pixels.
    property real lineSpacing: 2
    property real seed: 0
    // How much of the body shows between the lines (1 an icon's).
    property real body: 1
    readonly property real pixelRatio: Math.max(1, Screen.devicePixelRatio)

    function part(name) {
        return (colours || ({}))[name] || ({ colour: "#ffffff", own: true })
    }

    MultiEffect {
        objectName: "hologram-glow"
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        scale: effect.glowScale
        source: effect.source
        visible: effect.glow > 0 && effect.source !== null
        opacity: Math.min(1, effect.glow * 0.85)
        autoPaddingEnabled: true
        blurEnabled: true
        blurMax: Math.round(16 + 48 * Math.max(0, Math.min(1, effect.glowReach)))
        blur: 1
        // In the glow's colour, all of it, or in the item's own colours.
        brightness: effect.part("glow").own ? 0.15 : 1
        saturation: effect.part("glow").own ? 0.25 : 0
        colorization: effect.part("glow").own ? 0 : 1
        colorizationColor: effect.part("glow").colour
    }

    ShaderEffect {
        objectName: "hologram-light"
        anchors.fill: parent
        visible: effect.source !== null
        property variant source: effect.source
        property color outlineColour: effect.part("outline").colour
        property color inlineColour: effect.part("inline").colour
        property color volumeColour: effect.part("volume").colour
        property vector4d own: Qt.vector4d(effect.part("outline").own ? 1 : 0, effect.part("inline").own ? 1 : 0,
                                           effect.part("volume").own ? 1 : 0, 0)
        property size texel: Qt.size(1 / Math.max(1, effect.width * effect.pixelRatio),
                                     1 / Math.max(1, effect.height * effect.pixelRatio))
        property real time: effect.time
        property real lines: effect.height / Math.max(1, effect.lineSpacing)
        property real flicker: effect.flicker
        property real edges: 0.8
        property real seed: effect.seed
        property real body: effect.body
        // The part of the source's picture this item covers. As a layer's
        // effect, the glow's blur widens the layer's picture past the item
        // on every side to make room for itself (MultiEffect's padding), so
        // the light reads the item's own part of it, not all of it.
        property vector4d area: {
            const rect = effect.source && effect.source.sourceRect !== undefined
                ? effect.source.sourceRect : Qt.rect(0, 0, 0, 0)
            if (!(rect.width > 0 && rect.height > 0) || !(effect.width > 0 && effect.height > 0))
                return Qt.vector4d(0, 0, 1, 1)
            return Qt.vector4d(-rect.x / rect.width, -rect.y / rect.height,
                               effect.width / rect.width, effect.height / rect.height)
        }
        fragmentShader: "../shaders/hologram.frag.qsb"
    }
}
