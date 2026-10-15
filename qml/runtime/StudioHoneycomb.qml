import QtQuick

// A honeycomb of thin hexagon outlines, like the vents of a gaming machine,
// strongest in one corner and fading out across the area. Drawn again only
// when its size or colour changes; nothing when the colour is transparent.
Canvas {
    id: root

    property color color: "transparent"
    // Centre to corner of one hexagon.
    property real cell: 16
    // Where the honeycomb is strongest, as a share of the width and height.
    property real focusX: 1
    property real focusY: 1

    visible: color.a > 0
    onColorChanged: requestPaint()
    onCellChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onVisibleChanged: if (visible) requestPaint()

    onPaint: {
        const context = getContext("2d");
        context.reset();
        context.clearRect(0, 0, width, height);
        if (!visible || width <= 0 || height <= 0 || cell <= 0)
            return;
        const across = Math.sqrt(3) * cell;
        const down = 1.5 * cell;
        const focusLeft = focusX * width;
        const focusTop = focusY * height;
        const fade = context.createRadialGradient(focusLeft, focusTop, 0, focusLeft, focusTop,
                                                  Math.sqrt(width * width + height * height) * 0.8);
        fade.addColorStop(0, color);
        fade.addColorStop(1, Qt.alpha(color, 0));
        context.strokeStyle = fade;
        context.lineWidth = 1;
        context.beginPath();
        for (let row = 0; row * down <= height + cell; ++row) {
            const shift = row % 2 === 1 ? across / 2 : 0;
            for (let column = -1; column * across <= width + across; ++column) {
                const centreLeft = column * across + shift;
                const centreTop = row * down;
                for (let corner = 0; corner <= 6; ++corner) {
                    const angle = Math.PI / 180 * (60 * corner - 30);
                    const left = centreLeft + cell * Math.cos(angle);
                    const top = centreTop + cell * Math.sin(angle);
                    if (corner === 0)
                        context.moveTo(left, top);
                    else
                        context.lineTo(left, top);
                }
            }
        }
        context.stroke();
    }
}
