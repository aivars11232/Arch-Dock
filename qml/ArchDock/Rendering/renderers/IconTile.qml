import QtQuick
import QtQuick.Shapes

// The plain tile behind an icon when the user picks their own tile colour:
// The same native shapes also draw package-declared icon style layers.
Item {
    id: root

    property string shape: "rounded"
    property color fillColor: "#334155"
    property color secondaryColor: fillColor
    property color borderColor: "#94a3b8"
    property real borderWidth: 1
    property real radiusFactor: 0.22
    property string texture: "none"
    property real thickness: 0
    property real bevel: 0
    readonly property url textureSource: ["glass", "crystal", "neon", "minimal", "plasma", "lime",
        "floating-glass", "metallic", "futuristic", "organic", "platform", "plate", "pedestal"].includes(texture)
        ? Qt.resolvedUrl("../materials/" + texture + ".svg") : ""
    readonly property bool extraSurface: String(textureSource).length > 0 || thickness > 0 || bevel > 0
    readonly property bool textureReady: !String(textureSource).length
        || (surfaceLoader.item !== null && surfaceLoader.item.materialLoaded)
    readonly property bool polygon: ["hexagon", "diamond"].includes(shape)
    readonly property bool hasGradient: secondaryColor !== fillColor
    readonly property real radius: shape === "circle" ? Math.min(width, height) / 2
        : shape === "square" ? 0 : Math.min(width, height)
            * (shape === "squircle" ? 0.32 : radiusFactor)

    readonly property Gradient rectangleGradient: Gradient {
        GradientStop { position: 0; color: root.secondaryColor }
        GradientStop { position: 1; color: root.fillColor }
    }
    readonly property LinearGradient polygonGradient: LinearGradient {
        x1: 0; y1: 0; x2: 0; y2: root.height
        GradientStop { position: 0; color: root.secondaryColor }
        GradientStop { position: 1; color: root.fillColor }
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.polygon && !root.extraSurface
        radius: root.radius
        color: root.fillColor
        gradient: root.hasGradient ? root.rectangleGradient : null
        border.color: root.borderColor
        border.width: root.borderWidth
        antialiasing: true
    }

    Shape {
        anchors.fill: parent
        visible: root.polygon && !root.extraSurface
        // Keep the stroke inside the tile bounds, as Rectangle does.
        ShapePath {
            fillColor: root.fillColor
            fillGradient: root.hasGradient ? root.polygonGradient : null
            strokeColor: root.borderColor
            strokeWidth: root.borderWidth
            joinStyle: ShapePath.RoundJoin
            PathPolyline {
                path: {
                    const inset = root.borderWidth / 2
                    const w = Math.max(0, root.width - 2 * inset)
                    const h = Math.max(0, root.height - 2 * inset)
                    // PD-20: a diamond uses the full cell, with the same
                    // inside-stroke convention as every other shape.
                    if (root.shape === "diamond")
                        return [Qt.point(inset + w / 2, inset),
                            Qt.point(inset + w, inset + h / 2),
                            Qt.point(inset + w / 2, inset + h),
                            Qt.point(inset, inset + h / 2),
                            Qt.point(inset + w / 2, inset)]
                    return [Qt.point(inset + w * 0.25, inset), Qt.point(inset + w * 0.75, inset),
                        Qt.point(inset + w, inset + h / 2), Qt.point(inset + w * 0.75, inset + h),
                        Qt.point(inset + w * 0.25, inset + h), Qt.point(inset, inset + h / 2),
                        Qt.point(inset + w * 0.25, inset)]
                }
            }
        }
    }

    // ShapePath.fillItem is ignored by Qt's software Shape renderer (096
    // native probe). Use the same native Canvas/QPainter pattern as the panel
    // surface for this optional, static treatment on both rendering backends.
    Loader {
        id: surfaceLoader
        anchors.fill: parent
        active: root.extraSurface
        sourceComponent: Canvas {
            id: surface
            objectName: "icon-tile-surface"
            renderStrategy: Canvas.Threaded
            property bool materialLoaded: !String(root.textureSource).length
            property string loadedSource: ""

            function loadMaterial() {
                if (!available) return
                const source = String(root.textureSource)
                if (loadedSource.length && loadedSource !== source) unloadImage(loadedSource)
                loadedSource = source
                materialLoaded = !source.length || isImageLoaded(source)
                if (source.length && !materialLoaded) loadImage(source)
                requestPaint()
            }
            onAvailableChanged: if (available) loadMaterial()
            Component.onCompleted: loadMaterial()
            onImageLoaded: {
                materialLoaded = !String(root.textureSource).length || isImageLoaded(root.textureSource)
                requestPaint()
            }
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            function trace(context, inset) {
                const w = Math.max(0, width - inset * 2), h = Math.max(0, height - inset * 2)
                context.beginPath()
                if (root.polygon) {
                    const points = root.shape === "diamond"
                        ? [[0.5,0], [1,0.5], [0.5,1], [0,0.5]]
                        : [[0.25,0], [0.75,0], [1,0.5], [0.75,1], [0.25,1], [0,0.5]]
                    context.moveTo(inset + points[0][0] * w, inset + points[0][1] * h)
                    for (let index = 1; index < points.length; ++index)
                        context.lineTo(inset + points[index][0] * w, inset + points[index][1] * h)
                } else {
                    const r = Math.max(0, Math.min(Math.min(w,h) / 2, root.radius - inset))
                    context.moveTo(inset + r, inset)
                    context.lineTo(inset + w - r, inset)
                    context.arcTo(inset + w, inset, inset + w, inset + r, r)
                    context.lineTo(inset + w, inset + h - r)
                    context.arcTo(inset + w, inset + h, inset + w - r, inset + h, r)
                    context.lineTo(inset + r, inset + h)
                    context.arcTo(inset, inset + h, inset, inset + h - r, r)
                    context.lineTo(inset, inset + r)
                    context.arcTo(inset, inset, inset + r, inset, r)
                }
                context.closePath()
            }
            onPaint: {
                const context = getContext("2d")
                context.reset()
                const stroke = Math.max(0, root.borderWidth)
                const edge = Math.min(Math.min(width,height) * 0.2,
                    Math.max(0, root.thickness) * 0.35 + Math.max(0, root.bevel))
                if (edge > 0) {
                    trace(context, stroke / 2)
                    const gradient = context.createLinearGradient(0,0,0,height)
                    gradient.addColorStop(0, Qt.lighter(root.fillColor, 1.65).toString())
                    gradient.addColorStop(1, Qt.darker(root.fillColor, 1.8).toString())
                    context.fillStyle = gradient
                    context.fill()
                }
                trace(context, stroke / 2 + edge)
                if (String(root.textureSource).length && materialLoaded) {
                    context.fillStyle = context.createPattern(String(root.textureSource), "repeat")
                    context.fill()
                    context.globalCompositeOperation = "qt-multiply"
                    context.fillStyle = root.fillColor
                    context.fill()
                    context.globalCompositeOperation = "source-over"
                } else {
                    const gradient = context.createLinearGradient(0,0,0,height)
                    gradient.addColorStop(0, root.secondaryColor.toString())
                    gradient.addColorStop(1, root.fillColor.toString())
                    context.fillStyle = gradient
                    context.fill()
                }
                if (stroke > 0) {
                    trace(context, stroke / 2)
                    context.lineWidth = stroke
                    context.strokeStyle = root.borderColor
                    context.lineJoin = "round"
                    context.stroke()
                }
            }
            Connections {
                target: root
                function onTextureSourceChanged() { surface.loadMaterial() }
                function onShapeChanged() { surface.requestPaint() }
                function onRadiusChanged() { surface.requestPaint() }
                function onFillColorChanged() { surface.requestPaint() }
                function onSecondaryColorChanged() { surface.requestPaint() }
                function onBorderColorChanged() { surface.requestPaint() }
                function onBorderWidthChanged() { surface.requestPaint() }
                function onThicknessChanged() { surface.requestPaint() }
                function onBevelChanged() { surface.requestPaint() }
            }
        }
    }
}
