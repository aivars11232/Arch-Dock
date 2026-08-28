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

    readonly property bool rendererReady: width > 0 && height > 0
    readonly property var surfacePath: LayoutEngine.surface(
        layout, geometry, layoutAngle, polygonSides)
    readonly property var surfaceStyle: LayoutEngine.themeStyle(
        appearance, customColor, Number(geometry.iconSize || 40))

    width: Number(geometry.width || 0)
    height: Number(geometry.height || 0)

    Canvas {
        id: canvas

        anchors.fill: parent
        opacity: Math.max(0, Math.min(1, root.panelOpacity))
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
