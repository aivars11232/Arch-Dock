import QtQuick
import QtQuick.Shapes
import QtQuick.Window
import ArchDock.Rendering 1.0
import ArchDock.Decorations 1.0

// A panel decoration on a flat panel: the panel's outline remade by one of
// the decorations (DecorationCatalogue.js) along the panel's own path, on
// any panel shape, lit per pixel off the panel's thread (DecorationBaker).
// Its lights pulse, flicker, twinkle, crackle or blink as the decoration
// says; its motion (an outline in waves, a picture drifting, light flowing,
// parts swaying, footprints walking, gauges spinning, colours cycling)
// plays as a loop of lit frames, each blended into the next. On an edge
// panel it faces into the screen, fits the panel's thickness and fades out
// where the panel ends.
Item {
    id: root
    objectName: "panel-decoration"

    // The decoration's id; empty draws nothing.
    property string decoration: ""
    // The viewer's colours (DecorationScene.scene's choices).
    property var choices: ({})
    // Its animations (on by default), and how fast they run.
    property bool animated: true
    property real motionSpeed: 1
    property string layout: "horizontal"
    property var geometry: ({
        width: 0,
        height: 0,
        iconSize: 40,
        padding: 0,
        radius: 40,
        layout: "horizontal"
    })
    property real layoutAngle: 0
    // A turning free panel is drawn at its resting angle and the drawing
    // turned as a whole, as the flat surface is (PanelProcedural2D).
    property real restingAngle: layoutAngle
    readonly property bool turnsAsDrawn:
        Boolean(geometry && geometry.rotationEnvelope === true)
    readonly property real drawnAngle: turnsAsDrawn ? restingAngle : layoutAngle
    readonly property real drawingTurn: turnsAsDrawn ? layoutAngle - restingAngle : 0
    property int polygonSides: 6
    // The icons' centres at rest, at the resting angle, in this item's
    // pixels: arches stand over them, a band is cut between them.
    property var iconCentres: []
    // The screen edge an edge panel stands on ("bottom", "top", "left",
    // "right"); empty for a free panel.
    property string edge: ""
    // In an Arch Dock bar, the bar's own window in this item's pixels
    // ({x, y, width, height}), or null. Along a bar the band runs down the
    // bar's middle from one end of the bar to the other and stays inside
    // it: its own ends land on the bar's ends, and it is as deep as the bar
    // but for a little room at its sides, as the flat band keeps.
    property var barWindow: null
    readonly property bool inBar: barWindow !== null && barWindow !== undefined
        && Number(barWindow.width) > 0 && Number(barWindow.height) > 0
    readonly property real barMargin: 4
    // On a Plasma-held edge panel, how thick the panel is across its edge;
    // this item is deeper (room for magnification) and centred across it,
    // and the band fits inside the panel. 0: the item's own thickness.
    property real panelThickness: 0
    property real panelOpacity: 0.9
    property var motionTracks: null
    property bool reducedMotion: false
    property bool sceneConcealed: false

    readonly property var entry: decoration.length ? DecorationCatalogue.decoration(decoration) : null
    readonly property bool rendererReady: width > 0 && height > 0 && decorationBaker.frames > 0
    readonly property string errorReason: decorationBaker.error
    readonly property var surfacePath: LayoutEngine.surface(layout, geometry, drawnAngle, polygonSides)
    readonly property real iconSize: Number(geometry.iconSize || 40)
    // The band is as wide as the decorations were designed round an icon.
    readonly property real halfWidth: Math.max(6, iconSize / 1.7)
    // Into the screen, from an edge panel.
    readonly property var outward: {
        if (edge === "bottom") return ({ x: 0, y: -1 })
        if (edge === "top") return ({ x: 0, y: 1 })
        if (edge === "left") return ({ x: 1, y: 0 })
        if (edge === "right") return ({ x: -1, y: 0 })
        return null
    }
    // The bar's middle line, from its start to its end (down a side bar),
    // the same little room in from its ends as from its sides.
    readonly property var barLine: {
        if (!inBar) return []
        const w = barWindow, m = barMargin
        const down = outward ? outward.x !== 0 : Number(w.height) > Number(w.width)
        const points = []
        for (let index = 0; index < 72; ++index) {
            const t = index / 71
            points.push(down ? ({ x: w.x + w.width / 2, y: w.y + m + t * Math.max(1, w.height - 2 * m) })
                             : ({ x: w.x + m + t * Math.max(1, w.width - 2 * m), y: w.y + w.height / 2 }))
        }
        return points
    }
    readonly property var decorationPath: ({
        points: inBar ? barLine : surfacePath.points,
        closed: !inBar && surfacePath.closed === true,
        outward: outward,
        // The band's own ends at the path's ends (a bar's), not past them.
        within: inBar
    })
    readonly property var iconPlaces: DecorationScene.slotsOnPath(decorationPath, iconCentres)
    // An edge panel's room across its band, either side of its path, and
    // the window the panel is seen through.
    // Across the panel, where its window starts and ends: a bar's own
    // window, a Plasma-held panel's thickness centred in this item, or the
    // whole item.
    readonly property bool runsDown: outward ? outward.x !== 0 : false
    readonly property real ownAcross: runsDown ? width : height
    readonly property real windowNear: inBar ? Number(runsDown ? barWindow.x : barWindow.y)
        : panelThickness > 0 ? Math.max(0, (ownAcross - panelThickness) / 2) : 0
    readonly property real windowFar: inBar ? windowNear + Number(runsDown ? barWindow.width : barWindow.height)
        : panelThickness > 0 ? Math.min(ownAcross, windowNear + panelThickness) : ownAcross
    readonly property var fitRoom: {
        const first = decorationPath.points.length ? decorationPath.points[0] : null
        if (!outward || !first) return undefined
        // Clear of the fade at the window's edge; inside a panel's own
        // window (a bar's, or a Plasma-held panel's thickness), as close to
        // its sides as the flat band keeps.
        const margin = inBar || panelThickness > 0 ? barMargin : Math.ceil(halfWidth * 0.35) + 1
        if (outward.y < 0) return ({ out: first.y - windowNear - margin, in: windowFar - first.y - margin })
        if (outward.y > 0) return ({ out: windowFar - first.y - margin, in: first.y - windowNear - margin })
        if (outward.x > 0) return ({ out: windowFar - first.x - margin, in: first.x - windowNear - margin })
        return ({ out: first.x - windowNear - margin, in: windowFar - first.x - margin })
    }
    // A bar's own window, its halo and shadow fading out at its edges; an
    // edge panel's runs on along the panel, which is made longer for the
    // decoration's ends.
    readonly property var panelWindow: {
        if (!outward) return undefined
        if (inBar)
            return ({ x: Number(barWindow.x), y: Number(barWindow.y), width: Number(barWindow.width),
                      height: Number(barWindow.height), fade: barMargin * 0.75 })
        const far = 100000
        const fade = panelThickness > 0 ? barMargin * 0.75 : halfWidth * 0.35
        return outward.y !== 0
            ? ({ x: -far, y: windowNear, width: width + 2 * far, height: windowFar - windowNear, fade: fade })
            : ({ x: windowNear, y: -far, width: windowFar - windowNear, height: height + 2 * far, fade: fade })
    }
    // How far the decoration reaches out of the panel's box on each side
    // (the scene's effect margins make room for it).
    readonly property var reach: decorationBaker.bounds.width > 0 ? ({
        left: Math.max(0, Math.ceil(-decorationBaker.bounds.x)),
        top: Math.max(0, Math.ceil(-decorationBaker.bounds.y)),
        right: Math.max(0, Math.ceil(decorationBaker.bounds.x + decorationBaker.bounds.width - width)),
        bottom: Math.max(0, Math.ceil(decorationBaker.bounds.y + decorationBaker.bounds.height - height))
    }) : ({ left: 0, top: 0, right: 0, bottom: 0 })
    // Where the decoration's launcher seat is, in this item's pixels, or
    // null.
    readonly property var launcherSeat: decorationBaker.launcher
    // Its round lights (lenses), each brightening while the pointer is over
    // it.
    readonly property var lightSpots: decorationBaker.lightSpots
    // A light's colour (a "#rrggbb" name) at an opacity, and the same
    // colour a share of the way to white.
    function tinted(colour, alpha) {
        const c = Qt.color(String(colour || "#ff3b1f"))
        return Qt.rgba(c.r, c.g, c.b, alpha)
    }
    function lighter(colour, share, alpha) {
        const c = Qt.color(String(colour || "#ff3b1f"))
        return Qt.rgba(c.r + (1 - c.r) * share, c.g + (1 - c.g) * share, c.b + (1 - c.b) * share, alpha)
    }

    property alias baker: decorationBaker

    width: Number(geometry.width || 0)
    height: Number(geometry.height || 0)

    DecorationBaker {
        id: decorationBaker
        directory: Qt.resolvedUrl("..")
        decoration: root.entry ? root.decoration : ""
        path: root.decorationPath
        halfWidth: root.halfWidth
        iconPlaces: root.iconPlaces
        choices: root.choices
        animated: root.animated && !root.reducedMotion
        // The band runs on past the end icons as it was designed round them;
        // along a bar it already runs from end to end.
        ends: root.inBar ? 0 : 2.5
        fit: root.fitRoom
        clip: root.panelWindow
        scale: root.Window.window ? root.Window.window.devicePixelRatio : Screen.devicePixelRatio
    }

    // ---- Animation ----------------------------------------------------
    // The kinds of light animation (the rest move the decoration and are
    // in its frames).
    readonly property var lightAnimations: DecorationScene.lightAnimations(entry)
    readonly property bool looping: decorationBaker.frameCount > 1 && decorationBaker.frames === decorationBaker.frameCount
        && decorationBaker.loop > 0
    readonly property bool playing: animated && !reducedMotion && !sceneConcealed && visible
        && panelOpacity > 0 && rendererReady && (looping || lightAnimations.length > 0)
    // The decoration's clock, in seconds, stepped while it plays.
    property real clock: 0
    Timer {
        interval: 33
        repeat: true
        running: root.playing
        onTriggered: root.clock += interval / 1000 * Math.max(0.1, Math.min(3, root.motionSpeed))
    }
    readonly property real loopPosition: looping ? (clock / decorationBaker.loop % 1) * decorationBaker.frameCount : 0
    readonly property int frameA: looping ? Math.floor(loopPosition) % decorationBaker.frameCount : 0
    readonly property int frameB: looping ? (frameA + 1) % decorationBaker.frameCount : 0
    readonly property real blend: looping ? loopPosition - Math.floor(loopPosition) : 0

    // How bright a group of lights is now (1 as lit).
    function lightLevel(group, time) {
        return DecorationScene.lightLevel(lightAnimations, group, time)
    }

    // ---- The collapse track, as the flat surface follows it ------------
    readonly property var surfaceMotionTrack: {
        const source = motionTracks && typeof motionTracks === "object" ? motionTracks : null
        const track = source ? source["surface"] : null
        return track && typeof track === "object" ? track : null
    }
    function motionNumber(key, fallback) {
        const value = Number(surfaceMotionTrack && surfaceMotionTrack[key] !== undefined
                             ? surfaceMotionTrack[key] : fallback)
        return isFinite(value) ? value : fallback
    }
    readonly property real motionOpacity: Math.max(0, Math.min(1, motionNumber("opacity", 1)))
    // A collapse's clip, while it cuts into the panel's box; the open panel
    // reaches out as far as it is drawn.
    readonly property var motionClipRect: {
        const clip = surfaceMotionTrack && surfaceMotionTrack.clip
                && typeof surfaceMotionTrack.clip === "object" ? surfaceMotionTrack.clip : null
        if (!clip) return null
        const number = function(value, fallback) {
            const candidate = Number(value)
            return isFinite(candidate) ? candidate : fallback
        }
        const rect = Qt.rect(Math.max(0, number(clip.x, 0)), Math.max(0, number(clip.y, 0)),
                             Math.max(0, number(clip.width, width)), Math.max(0, number(clip.height, height)))
        const cuts = rect.width < width - 0.0001 || rect.height < height - 0.0001
            || rect.x > 0.0001 || rect.y > 0.0001
        return cuts ? rect : null
    }

    Item {
        id: clipper
        // A collapse clips the decoration to what is left of the panel;
        // otherwise it reaches out as far as it is drawn.
        x: root.motionClipRect ? root.motionClipRect.x : -root.reach.left
        y: root.motionClipRect ? root.motionClipRect.y : -root.reach.top
        width: root.motionClipRect ? root.motionClipRect.width : root.width + root.reach.left + root.reach.right
        height: root.motionClipRect ? root.motionClipRect.height : root.height + root.reach.top + root.reach.bottom
        clip: root.motionClipRect !== null

        Item {
            id: drawing
            objectName: "panel-decoration-drawing"
            x: -clipper.x
            y: -clipper.y
            width: root.width
            height: root.height
            opacity: Math.max(0, Math.min(1, root.panelOpacity)) * root.motionOpacity
            rotation: root.drawingTurn
            transform: [
                Scale {
                    origin.x: drawing.width / 2
                    origin.y: drawing.height / 2
                    xScale: Math.max(0, root.motionNumber("scaleX", 1))
                    yScale: Math.max(0, root.motionNumber("scaleY", 1))
                },
                Translate {
                    x: root.motionNumber("offsetX", 0)
                    y: root.motionNumber("offsetY", 0)
                }
            ]

            Item {
                id: pictures
                x: decorationBaker.bounds.x
                y: decorationBaker.bounds.y
                width: decorationBaker.bounds.width
                height: decorationBaker.bounds.height

                DecorationLayer {
                    objectName: "panel-decoration-base"
                    anchors.fill: parent
                    baker: decorationBaker
                    picture: "base"
                    frame: root.frameA
                }
                DecorationLayer {
                    anchors.fill: parent
                    baker: decorationBaker
                    picture: "base"
                    frame: root.frameB
                    visible: root.looping
                    opacity: root.blend
                }
                Repeater {
                    model: decorationBaker.layers
                    delegate: Item {
                        id: lightLayer
                        required property string modelData
                        readonly property real level: root.playing ? root.lightLevel(modelData, root.clock) : 1
                        anchors.fill: parent
                        DecorationLayer {
                            objectName: "panel-decoration-" + lightLayer.modelData
                            anchors.fill: parent
                            baker: decorationBaker
                            picture: lightLayer.modelData
                            frame: root.frameA
                            opacity: lightLayer.level * (root.looping ? 1 - root.blend : 1)
                        }
                        DecorationLayer {
                            anchors.fill: parent
                            baker: decorationBaker
                            picture: lightLayer.modelData
                            frame: root.frameB
                            visible: root.looping
                            opacity: lightLayer.level * root.blend
                        }
                    }
                }
                // Under the pointer a round light lights up: a glow of its
                // own colour swells over it, brightest at its middle and
                // fading out round it, and fades back as the pointer leaves.
                // The pointer is only watched here, never taken from the
                // icons.
                HoverHandler {
                    id: pointerWatch
                    objectName: "panel-decoration-hover"
                }
                Repeater {
                    model: root.lightSpots
                    delegate: Item {
                        id: spotLight
                        required property var modelData
                        required property int index
                        objectName: "panel-decoration-light-spot-" + index
                        readonly property real radius: Math.max(1, Number(modelData.radius) || 0)
                        readonly property real reach: radius * 2.3
                        readonly property bool hovered: pointerWatch.hovered
                            && Math.hypot(pointerWatch.point.position.x + pictures.x - Number(modelData.x),
                                          pointerWatch.point.position.y + pictures.y - Number(modelData.y)) <= radius * 1.3
                        x: Number(modelData.x) - pictures.x - reach
                        y: Number(modelData.y) - pictures.y - reach
                        width: 2 * reach
                        height: 2 * reach
                        opacity: hovered ? 1 : 0
                        scale: hovered ? 1.06 : 0.9
                        visible: opacity > 0.01
                        Behavior on opacity {
                            enabled: !root.reducedMotion
                            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                        }
                        Behavior on scale {
                            enabled: !root.reducedMotion
                            NumberAnimation { duration: 240; easing.type: Easing.OutBack }
                        }
                        Shape {
                            anchors.fill: parent
                            ShapePath {
                                strokeColor: "transparent"
                                fillGradient: RadialGradient {
                                    centerX: spotLight.reach
                                    centerY: spotLight.reach
                                    centerRadius: spotLight.reach
                                    focalX: spotLight.reach
                                    focalY: spotLight.reach
                                    // The lens lit a brighter red from its
                                    // middle, a soft glow round it.
                                    GradientStop { position: 0; color: root.lighter(spotLight.modelData.colour, 0.4, 0.85) }
                                    GradientStop { position: 0.3; color: root.tinted(spotLight.modelData.colour, 0.6) }
                                    GradientStop { position: 0.6; color: root.tinted(spotLight.modelData.colour, 0.18) }
                                    GradientStop { position: 1; color: root.tinted(spotLight.modelData.colour, 0) }
                                }
                                PathAngleArc {
                                    centerX: spotLight.reach
                                    centerY: spotLight.reach
                                    radiusX: spotLight.reach
                                    radiusY: spotLight.reach
                                    startAngle: 0
                                    sweepAngle: 360
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
