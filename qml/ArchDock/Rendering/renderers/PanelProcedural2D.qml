import QtQuick
import ArchDock.Rendering 1.0

// The dependency-free 2D surface: the panel's track drawn as a stroked path
// in the colours of its appearance (glass, neon, metallic...). It is the
// fallback whenever a theme's own renderer cannot be used.
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
    property int polygonSides: 6
    property string appearance: "glass"
    property string customColor: ""
    property real panelOpacity: 0.9

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
        layout, geometry, layoutAngle, polygonSides)
    readonly property var surfaceStyle: LayoutEngine.themeStyle(
        appearance, customColor, Number(geometry.iconSize || 40))

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

        x: -motionClipper.x
        y: -motionClipper.y
        width: root.width
        height: root.height
        opacity: Math.max(0, Math.min(1, root.panelOpacity))
            * root.motionOpacity
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
            if (!root.rendererReady || root.surfacePath.points.length === 0
                    || !root.surfaceStyle.trackVisible)
                return

            context.lineWidth = root.surfaceStyle.lineWidth
            context.strokeStyle = root.surfaceStyle.stroke
            context.shadowColor = root.surfaceStyle.shadow
            context.shadowBlur = root.surfaceStyle.blur
            context.lineCap = "round"
            context.lineJoin = "round"
            context.beginPath()
            context.moveTo(root.surfacePath.points[0].x,
                           root.surfacePath.points[0].y)
            for (let index = 1;
                 index < root.surfacePath.points.length; ++index) {
                context.lineTo(root.surfacePath.points[index].x,
                               root.surfacePath.points[index].y)
            }
            if (root.surfacePath.closed)
                context.closePath()
            context.stroke()
        }

        Connections {
            target: root

            function onLayoutChanged() { canvas.requestPaint() }
            function onGeometryChanged() { canvas.requestPaint() }
            function onLayoutAngleChanged() { canvas.requestPaint() }
            function onPolygonSidesChanged() { canvas.requestPaint() }
            function onAppearanceChanged() { canvas.requestPaint() }
            function onCustomColorChanged() { canvas.requestPaint() }
            function onPanelOpacityChanged() { canvas.requestPaint() }
        }
    }
    }
}
