import QtQuick
import ArchDock.Rendering 1.0

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

    // The safe presentation track. A procedural surface has no declared parts
    // to slide or split, so it honours a mechanism the only truthful way a
    // drawn shape can: it clips and fades. PanelMotionController has already
    // decided the geometry; this only applies it.
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

        x: -motionClipper.x
        y: -motionClipper.y
        width: root.width
        height: root.height
        opacity: Math.max(0, Math.min(1, root.panelOpacity))
            * root.motionOpacity
        renderStrategy: Canvas.Cooperative

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
