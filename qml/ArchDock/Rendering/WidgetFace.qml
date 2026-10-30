import QtQuick
import "PlatformGeometry.js" as PlatformGeometry
import "HologramLook.js" as HologramLook
import "MotionChannels.js" as MotionChannels

// A Plasma widget's face on a panel (its clock, a battery's level, whatever
// it shows) drawn as the dock's hologram icons are: as light, over its own
// projector's light. The face is a picture of the widget taken as it
// changes (`picture`), as large as the widget; the light is laid out as an
// icon's of the panel's icon size, its lens in the middle of the widget's
// place, turned away from the screen's edge, and the face floats upright in
// its widening end.
Item {
    id: root

    // The widget's picture, this item's size, and the panel's icon size.
    property url picture
    property real iconSize: 52
    // The panel's settings: the holograms' look, light and motion.
    property var settings: ({})
    property string screenEdge: "bottom"
    property bool reducedMotion: false
    // Each face its own phase, so the panel's holograms do not blink together.
    property real seed: 0
    // Under the pointer it grows as the dock's icons do (Magnification).
    property bool hovered: false
    property real magnification: 1
    property bool magnificationEnabled: false
    property int motionDuration: 170
    readonly property real hoverScale: magnificationEnabled && hovered
        ? MotionChannels.magnificationScale(magnification, 1) : 1

    readonly property var hologram: HologramLook.settings(settings, "Icon")
    readonly property bool drawn: hologram.on === true && String(picture).length > 0
    readonly property var colours: HologramLook.colours(settings, "Icon")
    readonly property var layout: drawn
        ? PlatformGeometry.hologramLayout(settings, iconSize, false, screenEdge) : ({ on: false })
    readonly property bool lightShown: drawn && layout.on === true
    readonly property var direction: PlatformGeometry.standDirection(screenEdge, false)
    // How far the face's middle lies from its place's middle, away from the
    // screen's edge.
    readonly property real raise: lightShown ? Number(layout.raise || 0) : 0
    // Where the face shows itself at rest, in this item's coordinates: its
    // place and, in its light, the face away from the screen's edge.
    readonly property rect restingRect: Qt.rect(direction.x * raise, direction.y * raise, width, height)

    readonly property bool moving: drawn && visible && !reducedMotion
    readonly property real time: moving ? HologramClock.time * hologram.speed : 0
    readonly property bool lightMoving: moving && HologramLook.beamMotionFlags(hologram.beamMotion).moving
    readonly property real floating: drawn && hologram.float
        ? Math.sin(time * 1.3 + seed) * iconSize * 0.025 : 0
    property bool holdingClock: false
    function holdClock(wanted) {
        if (wanted === holdingClock)
            return;
        holdingClock = wanted;
        if (wanted)
            HologramClock.hold();
        else
            HologramClock.release();
    }
    onMovingChanged: holdClock(moving)
    Component.onCompleted: holdClock(moving)
    Component.onDestruction: holdClock(false)

    // The picture, drawn only through the light below.
    Image {
        id: face
        width: root.width
        height: root.height
        source: root.picture
        fillMode: Image.PreserveAspectFit
        cache: false
        smooth: true
        visible: false
    }
    ShaderEffectSource {
        id: faceSource
        sourceItem: root.drawn ? face : null
        hideSource: true
        live: true
        visible: false
        smooth: true
        textureSize: Qt.size(Math.max(1, Math.ceil(root.width * Math.max(1, Screen.devicePixelRatio))),
                             Math.max(1, Math.ceil(root.height * Math.max(1, Screen.devicePixelRatio))))
    }

    Item {
        id: magnified
        anchors.fill: parent
        scale: root.hoverScale
        Behavior on scale {
            enabled: !root.reducedMotion
            NumberAnimation {
                duration: root.motionDuration
                easing.type: Easing.OutCubic
            }
        }

        // The lens and the light, laid out as on a bottom panel (the face toward
        // -y) in an icon-sized frame in the middle of the widget's place, turned
        // away from the screen's edge.
        Item {
            id: frame
            width: root.iconSize
            height: root.iconSize
            x: (root.width - width) / 2
            y: (root.height - height) / 2
            rotation: root.direction.turn
            visible: root.lightShown

            ShaderEffect {
                readonly property var spot: root.layout.opening || ({ y: 0, rx: 1, ry: 1 })
                width: Math.max(2, 2 * spot.rx * 1.6)
                height: Math.max(2, 2 * spot.ry * 1.6)
                x: (frame.width - width) / 2
                y: frame.height / 2 + spot.y - height / 2
                property color tint: root.colours.opening.colour
                property real time: root.time
                property real lens: 1 / 1.6
                property real ring: 0
                property real own: root.colours.opening.own ? 1 : 0
                property real strength: Math.min(1.2, root.hologram.beamStrength)
                property variant source: faceSource
                fragmentShader: "shaders/hologram-opening.frag.qsb"
            }
            ShaderEffect {
                readonly property var beam: root.layout.beam
                    || ({ start: 0, length: 1, startHalf: 1, endHalf: 1, fade: 1.6 })
                readonly property real sheet: 2 * Math.max((root.layout.opening || ({ rx: 1 })).rx * 1.04,
                                                           beam.endHalf, beam.startHalf)
                readonly property var flags: HologramLook.beamMotionFlags(root.hologram.beamMotion)
                width: Math.max(2, sheet)
                height: Math.max(2, beam.length)
                x: (frame.width - width) / 2
                y: frame.height / 2 + beam.start - height
                property color tint: root.colours.light.colour
                property vector4d motion: Qt.vector4d(flags.rise, flags.drift, flags.pulse, flags.sweep)
                property real time: root.lightMoving ? root.time : 0
                property real startHalf: beam.startHalf / Math.max(1, width)
                property real endHalf: beam.endHalf / Math.max(1, width)
                property real streaks: 30
                property real sparks: 0.1
                property real seed: root.seed
                property real fade: beam.fade
                property real strength: root.hologram.beamStrength
                property real own: root.colours.light.own ? 1 : 0
                property variant source: faceSource
                fragmentShader: "shaders/hologram-beam.frag.qsb"
            }
        }

        // The face as light, upright, where it floats.
        HologramEffect {
            visible: root.drawn
            x: root.restingRect.x
            y: root.restingRect.y + root.floating
            width: root.width
            height: root.height
            source: faceSource
            colours: root.colours
            time: root.time
            flicker: root.moving ? root.hologram.flicker : 0
            glow: root.hologram.glow
            glowReach: root.hologram.glowReach
            glowScale: 1 + 0.2 * root.hologram.glowReach
            lineSpacing: root.iconSize / 26
            seed: root.seed
        }
    }
}
