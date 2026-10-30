import QtQuick
import ArchDock.Rendering 1.0

// A Plasma widget's icon drawn as one of the dock's icons (IconScene), moved
// as they are: the panel's animation profiles on hover and press, its
// magnification, its tiles, 3D icons and holograms. Its place, hover and
// open state are the widget's, which stays under it, unseen, taking its
// clicks; the icon only draws it.
Item {
    id: root

    property var entry: ({})
    property int entryIndex: 0
    property real baseSize: 52
    property var iconStyleDefinition: ({})
    property string tileShape: "rounded"
    property var tileSettings: ({})
    property string appearance: "glass"
    property bool vertical: false
    property string screenEdge: "bottom"
    property bool showReflection: false
    property bool hovered: false
    property bool pressed: false
    property bool active: false
    property bool reducedMotion: false
    // Magnification and the panel's animations, as the dock's icons have them.
    property real magnification: 1
    property bool magnificationEnabled: false
    property var animationProfiles: []
    property var animationCatalog: ({})
    property string motionTrigger: ""
    property real motionIntensity: 1
    property real motionSpeed: 1
    property int motionDuration: 170
    // 3D icons as on a flat panel (Icons > 3D).
    property bool solidOnFlat: false
    property real solidDepth: 0
    property string solidRelief: "modelled"
    property real solidTurn: 15
    property real solidLighting: 0.5
    // A line drawing (a status icon) drawn with a body, for 3D and holograms.
    property bool boldSymbol: false

    width: baseSize
    height: baseSize

    readonly property real hoverScale: magnificationEnabled && hovered
        ? MotionChannels.magnificationScale(magnification, 1) : 1
    // The effect and the event that starts it are separate settings, so the
    // panel's trigger overrides the profile's own, as for the dock's icons.
    readonly property var boundAnimationProfiles: {
        const source = animationProfiles || []
        const trigger = AnimationProfileRuntime.normalizeTrigger(motionTrigger)
        const result = []
        for (let index = 0; index < source.length; ++index) {
            const profile = source[index]
            if (!profile || !profile.id)
                continue
            const bound = ({})
            for (const key of Object.keys(profile))
                bound[key] = profile[key]
            if (trigger !== "")
                bound.trigger = trigger
            result.push(bound)
        }
        return result
    }
    // Out of the panel is away from the screen's edge.
    readonly property var outward: screenEdge === "top" ? ({ x: 0, y: 1 })
        : screenEdge === "left" ? ({ x: 1, y: 0 })
        : screenEdge === "right" ? ({ x: -1, y: 0 }) : ({ x: 0, y: -1 })
    readonly property var motionContext: ({ size: baseSize, normal: ({ x: outward.x, y: outward.y }),
                                            tangentAngle: 0, allowance: null })
    readonly property var iconMotion: MotionChannels.motionFor(motionController.channels, "icon", motionContext)
    readonly property var glyphMotion: MotionChannels.motionFor(motionController.channels, "glyph", motionContext)
    readonly property var tileMotion: MotionChannels.motionFor(motionController.channels, "tile", motionContext)
    readonly property var indicatorMotion: MotionChannels.motionFor(motionController.channels, "indicator", motionContext)
    // Where it shows itself at rest (its cell and, over a stand or in a
    // hologram's light, its icon away from the screen's edge).
    readonly property rect restingIconRect: visual.restingIconRect

    IconMotionController {
        id: motionController
        profiles: root.boundAnimationProfiles
        catalog: root.animationCatalog
        entryIndex: root.entryIndex
        intensity: root.motionIntensity
        speed: root.motionSpeed
        reducedMotion: root.reducedMotion
        sceneVisible: root.visible
        hovered: root.hovered
        pressed: root.pressed
        running: false
        urgent: false
        dropActive: false
        revealed: true
    }

    Item {
        id: magnificationLayer
        anchors.centerIn: parent
        width: root.baseSize
        height: root.baseSize
        scale: root.hoverScale
        Behavior on scale {
            enabled: !root.reducedMotion
            NumberAnimation {
                duration: root.motionDuration
                easing.type: Easing.OutCubic
            }
        }

        Item {
            id: profileMotionLayer
            width: parent.width
            height: parent.height
            x: root.iconMotion.x
            y: root.iconMotion.y
            rotation: root.iconMotion.rotateZ
            opacity: root.iconMotion.opacity
            transform: [
                Matrix4x4 {
                    matrix: Math.abs(root.iconMotion.rotateY) > 0.01
                        ? MotionChannels.turnMatrix(root.iconMotion.rotateY, profileMotionLayer.width,
                                                    profileMotionLayer.height, root.baseSize * 2.4)
                        : Qt.matrix4x4()
                },
                Scale {
                    origin.x: profileMotionLayer.width / 2
                    origin.y: profileMotionLayer.height / 2
                    xScale: root.iconMotion.scale * root.iconMotion.scaleX
                    yScale: root.iconMotion.scale * root.iconMotion.scaleY
                }
            ]

            IconScene {
                id: visual
                anchors.fill: parent
                entry: root.entry
                iconStyleDefinition: root.iconStyleDefinition
                logicalSize: root.baseSize
                tileShape: root.tileShape
                tileSettings: root.tileSettings
                pedestalOnPath: false
                screenEdge: root.screenEdge
                appearance: root.appearance
                vertical: root.vertical
                hovered: root.hovered
                pressed: root.pressed
                active: root.active
                showReflection: root.showReflection
                showIndicator: false
                reducedMotion: root.reducedMotion
                solidOnFlat: root.solidOnFlat
                solidDepth: root.solidDepth
                solidRelief: root.solidRelief
                solidTurn: root.solidTurn
                solidLighting: root.solidLighting
                boldSymbol: root.boldSymbol
                glyphMotion: root.glyphMotion
                tileMotion: root.tileMotion
                indicatorMotion: root.indicatorMotion
                glowAmount: Math.max(root.iconMotion.glow, Math.max(root.glyphMotion.glow, root.tileMotion.glow))
                glowAnimating: false
                glowDuration: Math.max(80, root.motionDuration)
            }
        }
    }
}
