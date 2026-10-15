import QtQuick
import QtQuick.Shapes

// A frame for Panel Studio's window, rows and cards in its theme's shape:
// rounded corners, or, in the HUD themes, the top left and bottom right
// corners cut off. Filled with one colour, or from `color` at the top to
// `colorBottom` at the bottom. A strip along an edge shapes only the corners
// on that edge (topCorners, bottomCorners). The alien themes add a neon glow
// along the outline, inside and out (glowColor, reaching glowSize).
Item {
    id: root

    property color color: "transparent"
    property color colorBottom: color
    property color borderColor: "transparent"
    property real borderWidth: 1
    property real radius: 0
    property real chamfer: 0
    property bool topCorners: true
    property bool bottomCorners: true
    property color glowColor: "transparent"
    property real glowSize: 0
    readonly property bool cut: chamfer > 0
    // The cut, never more than half the frame.
    readonly property real cutSize: Math.max(0, Math.min(chamfer, width / 2, height / 2))
    readonly property real topCut: topCorners ? cutSize : 0
    readonly property real bottomCut: bottomCorners ? cutSize : 0
    readonly property real inset: borderWidth / 2
    readonly property bool outlined: borderColor.a > 0
    readonly property bool glowing: glowColor.a > 0 && glowSize > 0
    // The outline as points, for the glow: the cut corners, or the rounded
    // ones in short steps.
    readonly property var outline: {
        if (!glowing || width <= 0 || height <= 0)
            return [];
        const right = width - inset;
        const bottom = height - inset;
        if (cut) {
            return [Qt.point(inset + topCut, inset), Qt.point(right, inset),
                    Qt.point(right, bottom - bottomCut), Qt.point(right - bottomCut, bottom),
                    Qt.point(inset, bottom), Qt.point(inset, inset + topCut), Qt.point(inset + topCut, inset)];
        }
        const round = Math.max(0, Math.min(radius, width / 2, height / 2));
        const top = topCorners ? round : 0;
        const low = bottomCorners ? round : 0;
        // Each corner's centre and radius, clockwise from the top left, and
        // the angle its arc starts at (y grows downwards).
        const corners = [[inset + top, inset + top, top, 180], [right - top, inset + top, top, 270],
                         [right - low, bottom - low, low, 0], [inset + low, bottom - low, low, 90]];
        const points = [];
        for (const corner of corners) {
            for (let step = 0; step <= 6; ++step) {
                const angle = (corner[3] + 15 * step) * Math.PI / 180;
                points.push(Qt.point(corner[0] + corner[2] * Math.cos(angle),
                                     corner[1] + corner[2] * Math.sin(angle)));
            }
        }
        points.push(points[0]);
        return points;
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.cut
        topLeftRadius: root.topCorners ? root.radius : 0
        topRightRadius: root.topCorners ? root.radius : 0
        bottomLeftRadius: root.bottomCorners ? root.radius : 0
        bottomRightRadius: root.bottomCorners ? root.radius : 0
        border.width: root.outlined ? root.borderWidth : 0
        border.color: root.borderColor
        gradient: Gradient {
            GradientStop { position: 0; color: root.color }
            GradientStop { position: 1; color: root.colorBottom }
        }
    }

    Shape {
        anchors.fill: parent
        visible: root.cut
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.outlined ? root.borderColor : "transparent"
            strokeWidth: root.outlined ? root.borderWidth : -1
            joinStyle: ShapePath.MiterJoin
            fillGradient: LinearGradient {
                x1: 0; y1: 0
                x2: 0; y2: root.height
                GradientStop { position: 0; color: root.color }
                GradientStop { position: 1; color: root.colorBottom }
            }
            startX: root.inset + root.topCut
            startY: root.inset
            PathLine { x: root.width - root.inset; y: root.inset }
            PathLine { x: root.width - root.inset; y: root.height - root.inset - root.bottomCut }
            PathLine { x: root.width - root.inset - root.bottomCut; y: root.height - root.inset }
            PathLine { x: root.inset; y: root.height - root.inset }
            PathLine { x: root.inset; y: root.inset + root.topCut }
            PathLine { x: root.inset + root.topCut; y: root.inset }
        }
    }

    // The glow: from a wide, faint stroke to a narrow, brighter one over the
    // edge.
    Shape {
        anchors.fill: parent
        visible: root.glowing
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.alpha(root.glowColor, 0.07)
            strokeWidth: root.glowSize * 2
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: root.outline }
        }
        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.alpha(root.glowColor, 0.14)
            strokeWidth: root.glowSize
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: root.outline }
        }
        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.alpha(root.glowColor, 0.45)
            strokeWidth: Math.max(1, root.glowSize / 3)
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: root.outline }
        }
    }
}
