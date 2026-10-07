import QtQuick
import ArchDock.Rendering 1.0

// The dependency-free fallback surface. Original vector material maps are
// painted on the existing track, including the software scene-graph backend
// where ShaderEffect is unavailable. Geometry and rotation stay shared.
Item {
    id: root

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
    // The configured angle without the scene's turn. A turning free panel is
    // drawn in a square centred on its path (LayoutEngine.rotationEnvelope),
    // so the path is painted at its resting angle and the turn rotates the
    // painting as a whole: the threaded canvas would otherwise show each turn
    // after the icons had already moved (ADREP-TASK-002, OF-11).
    property real restingAngle: layoutAngle
    readonly property bool turnsAsDrawn:
        Boolean(geometry && geometry.rotationEnvelope === true)
    readonly property real drawnAngle: turnsAsDrawn ? restingAngle : layoutAngle
    readonly property real canvasTurn: turnsAsDrawn ? layoutAngle - restingAngle : 0
    property int polygonSides: 6
    property string appearance: "glass"
    property string customColor: ""
    property real panelOpacity: 0.9
    property real sparkleIntensity: 0
    property bool reducedMotion: false
    property bool sceneConcealed: false

    // The presentation track. A procedural surface has no declared parts to
    // slide or split, so it takes the whole-surface track: a collapse squeezes
    // the drawn shape onto its handle along the collapse axis, other
    // mechanisms clip and fade it. PanelMotionController has already decided
    // the geometry; this only applies it.
    property var motionTracks: null

    readonly property var surfaceMotionTrack: {
        const source = motionTracks && typeof motionTracks === "object"
            ? motionTracks : null
        const track = source ? source["surface"] : null
        return track && typeof track === "object" ? track : null
    }
    readonly property real motionOpacity: {
        const value = Number(surfaceMotionTrack
                             && surfaceMotionTrack.opacity !== undefined
                             ? surfaceMotionTrack.opacity : 1)
        return isFinite(value) ? Math.max(0, Math.min(1, value)) : 1
    }
    readonly property var motionClipRect: {
        const clip = surfaceMotionTrack && surfaceMotionTrack.clip
                && typeof surfaceMotionTrack.clip === "object"
            ? surfaceMotionTrack.clip : null
        function number(value, fallback) {
            const candidate = Number(value)
            return isFinite(candidate) ? candidate : fallback
        }
        if (!clip)
            return Qt.rect(0, 0, Math.max(0, width), Math.max(0, height))
        return Qt.rect(
            Math.max(0, number(clip.x, 0)),
            Math.max(0, number(clip.y, 0)),
            Math.max(0, number(clip.width, width)),
            Math.max(0, number(clip.height, height)))
    }
    readonly property bool motionClipActive:
        motionClipRect.width < width - 0.0001
        || motionClipRect.height < height - 0.0001
        || motionClipRect.x > 0.0001 || motionClipRect.y > 0.0001
    function motionNumber(key, fallback) {
        const value = Number(surfaceMotionTrack && surfaceMotionTrack[key] !== undefined
                             ? surfaceMotionTrack[key] : fallback)
        return isFinite(value) ? value : fallback
    }

    readonly property bool rendererReady: width > 0 && height > 0
    readonly property var surfacePath: LayoutEngine.surface(
        layout, geometry, drawnAngle, polygonSides)
    readonly property var surfaceStyle: LayoutEngine.themeStyle(
        appearance, customColor, Number(geometry.iconSize || 40))
    readonly property url materialSource: Qt.resolvedUrl(
        "../materials/" + surfaceStyle.material + ".svg")
    readonly property bool materialReady: canvas.materialLoaded
    readonly property bool materialAnimationActive: visible && !sceneConcealed
        && !reducedMotion && surfaceStyle.animated && motionOpacity > 0 && panelOpacity > 0
    property real energyPhase: 0

    function trace(context) {
        context.beginPath()
        const points = surfacePath.points
        context.moveTo(points[0].x, points[0].y)
        for (let i = 1; i < points.length; ++i)
            context.lineTo(points[i].x, points[i].y)
        if (surfacePath.closed) context.closePath()
    }

    Timer {
        interval: 125
        repeat: true
        running: root.materialAnimationActive
        onTriggered: {
            root.energyPhase = (root.energyPhase + 0.025) % 1
            energy.requestPaint()
        }
    }

    width: Number(geometry.width || 0)
    height: Number(geometry.height || 0)

    Item {
        id: motionClipper

        x: root.motionClipRect.x
        y: root.motionClipRect.y
        width: root.motionClipRect.width
        height: root.motionClipRect.height
        clip: root.motionClipActive

    Canvas {
        id: canvas
        objectName: "procedural-surface-canvas"
        property bool materialLoaded: false

        function loadMaterial() {
            if (!isImageLoaded(root.materialSource)) loadImage(root.materialSource)
            materialLoaded = isImageLoaded(root.materialSource)
            requestPaint()
        }
        onAvailableChanged: if (available) loadMaterial()
        Component.onCompleted: if (available) loadMaterial()
        onImageLoaded: {
            materialLoaded = isImageLoaded(root.materialSource)
            requestPaint()
        }

        x: -motionClipper.x
        y: -motionClipper.y
        width: root.width
        height: root.height
        opacity: Math.max(0, Math.min(1, root.panelOpacity))
            * root.motionOpacity * root.surfaceStyle.alpha
        rotation: root.canvasTurn
        // A collapse along an axis squeezes the drawn shape about its centre
        // onto the handle the controller keeps.
        transform: [
            Scale {
                origin.x: canvas.width / 2
                origin.y: canvas.height / 2
                xScale: Math.max(0, root.motionNumber("scaleX", 1))
                yScale: Math.max(0, root.motionNumber("scaleY", 1))
            },
            Translate {
                x: root.motionNumber("offsetX", 0)
                y: root.motionNumber("offsetY", 0)
            }
        ]
        // Cooperative can replay costly shadow painting on Plasma's GUI
        // thread. Keep the same image commands on Canvas's private worker.
        renderStrategy: Canvas.Threaded

        onPaint: {
            const context = getContext("2d")
            context.reset()
            materialLoaded = isImageLoaded(root.materialSource)
            if (!root.rendererReady || root.surfacePath.points.length === 0
                    || !root.surfaceStyle.trackVisible)
                return

            context.lineWidth = root.surfaceStyle.lineWidth
            context.strokeStyle = root.surfaceStyle.stroke
            context.shadowColor = root.surfaceStyle.shadow
            context.shadowBlur = root.surfaceStyle.blur
            context.lineCap = "round"
            context.lineJoin = "round"
            if (root.surfaceStyle.depth > 0) {
                context.save()
                context.translate(0, root.surfaceStyle.depth)
                context.strokeStyle = root.appearance === "floating-glass"
                    ? "rgba(84,131,156,0.45)" : "#28343f"
                root.trace(context)
                context.stroke()
                context.restore()
            }
            root.trace(context)
            // Keep the cast shadow separate from the textured, tinted body.
            context.strokeStyle = root.surfaceStyle.stroke
            context.stroke()
            context.shadowBlur = 0
            context.shadowColor = "transparent"
            context.strokeStyle = materialLoaded
                ? context.createPattern(String(root.materialSource), "repeat")
                : root.surfaceStyle.stroke
            context.stroke()
            // Multiply preserves the map's grain and facets under every tint.
            context.shadowBlur = 0
            context.shadowColor = "transparent"
            context.globalCompositeOperation = "qt-multiply"
            context.strokeStyle = root.surfaceStyle.stroke
            context.stroke()
            context.globalCompositeOperation = "source-over"
            // Fine polished edge, rather than random glitter.
            context.save()
            context.translate(0, -root.surfaceStyle.lineWidth / 2 + 1)
            context.lineWidth = 1.1
            context.strokeStyle = "rgba(255,255,255,0.45)"
            root.trace(context)
            context.stroke()
            context.restore()
            if (root.surfaceStyle.sparkle && root.sparkleIntensity > 0) {
                context.strokeStyle = "rgba(255,255,255," + Math.min(1, root.sparkleIntensity) + ")"
                context.lineWidth = 1
                const points = root.surfacePath.points
                for (let i = 3; i < points.length; i += 13) {
                    const p = points[i]
                    context.beginPath()
                    context.moveTo(p.x - 3, p.y); context.lineTo(p.x + 3, p.y)
                    context.moveTo(p.x, p.y - 3); context.lineTo(p.x, p.y + 3)
                    context.stroke()
                }
            }
        }

        Connections {
            target: root

            function onLayoutChanged() { canvas.requestPaint() }
            function onGeometryChanged() { canvas.requestPaint() }
            function onDrawnAngleChanged() { canvas.requestPaint() }
            function onPolygonSidesChanged() { canvas.requestPaint() }
            function onAppearanceChanged() { canvas.requestPaint() }
            function onCustomColorChanged() { canvas.requestPaint() }
            function onPanelOpacityChanged() { canvas.requestPaint() }
            function onSparkleIntensityChanged() { canvas.requestPaint() }
            function onMaterialSourceChanged() { canvas.loadMaterial() }
        }
    }

    // Only the small light overlay repaints at 8 Hz. Static textured bodies
    // remain cached; neither canvas paints periodically while hidden.
    Canvas {
        id: energy
        objectName: "procedural-material-energy"
        anchors.fill: canvas
        rotation: canvas.rotation
        transform: [
            Scale {
                origin.x: energy.width / 2
                origin.y: energy.height / 2
                xScale: Math.max(0, root.motionNumber("scaleX", 1))
                yScale: Math.max(0, root.motionNumber("scaleY", 1))
            },
            Translate {
                x: root.motionNumber("offsetX", 0)
                y: root.motionNumber("offsetY", 0)
            }
        ]
        visible: root.surfaceStyle.animated
        opacity: canvas.opacity
        renderStrategy: Canvas.Threaded
        onPaint: {
            if (!root.surfaceStyle.animated) return
            const context = getContext("2d")
            context.reset()
            const points = root.surfacePath.points
            if (points.length < 2) return
            context.lineWidth = root.appearance === "plasma" ? 3 : 1.6
            for (let i = 1; i < points.length; ++i) {
                const wave = (i / points.length - root.energyPhase + 1) % 1
                context.strokeStyle = "rgba(225,250,255," +
                    (root.appearance === "plasma" ? Math.max(0, 1 - wave * 4)
                        : 0.2 + 0.4 * (1 + Math.sin(wave * Math.PI * 6))) + ")"
                context.beginPath()
                context.moveTo(points[i-1].x, points[i-1].y)
                context.lineTo(points[i].x, points[i].y)
                context.stroke()
            }
        }
        Connections {
            target: root
            function onSurfacePathChanged() { if (root.surfaceStyle.animated) energy.requestPaint() }
            function onAppearanceChanged() { if (root.surfaceStyle.animated) energy.requestPaint() }
        }
    }
    }
}
