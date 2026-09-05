import QtQuick
import ArchDock.Rendering 1.0

// The active input region of a free, non-skinned scene.
//
// A ring or arc panel is mostly empty desktop inside its rectangle. This item
// is installed as the scene's containmentMask so Qt Quick only accepts input
// on the band the surface actually draws and on the entries themselves; the
// empty interior and the corners pass through. It reads the same LayoutEngine
// surface as the renderer, at the same effective angle, so a rotating panel's
// input region turns with it.
//
// This narrows Qt Quick item hit testing only. A Plasma desktop applet is a
// rectangle to the compositor, and this item does not claim otherwise.
QtObject {
    id: root

    property string layout: "circular"
    property var geometry: ({})
    property real angle: 0
    property int polygonSides: 6
    // Local rectangles of the rendered entries, in the scene's coordinates.
    property var entryRects: []
    // How far from the drawn path a press still counts, in pixels.
    property real bandWidth: 40
    property real entryMargin: 4
    property bool enabled: true

    readonly property var surface: LayoutEngine.surface(
        layout, geometry, angle, polygonSides)

    function rectContains(rect, x, y, margin) {
        const left = Number(rect.x || 0) - margin
        const top = Number(rect.y || 0) - margin
        return x >= left && y >= top
            && x <= left + Number(rect.width || 0) + margin * 2
            && y <= top + Number(rect.height || 0) + margin * 2
    }

    function distanceToSegment(px, py, ax, ay, bx, by) {
        const dx = bx - ax
        const dy = by - ay
        const lengthSquared = dx * dx + dy * dy
        let t = 0
        if (lengthSquared > 0)
            t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / lengthSquared))
        const cx = ax + t * dx
        const cy = ay + t * dy
        return Math.hypot(px - cx, py - cy)
    }

    function distanceToPath(x, y) {
        const points = surface && surface.points ? surface.points : []
        if (points.length === 0)
            return Infinity
        if (points.length === 1)
            return Math.hypot(x - points[0].x, y - points[0].y)
        let best = Infinity
        const last = surface.closed ? points.length : points.length - 1
        for (let index = 0; index < last; ++index) {
            const a = points[index]
            const b = points[(index + 1) % points.length]
            best = Math.min(best, distanceToSegment(x, y, a.x, a.y, b.x, b.y))
        }
        return best
    }

    // Qt Quick calls this with a point in the scene's local coordinates.
    function contains(point) {
        if (!enabled)
            return true
        const x = Number(point.x)
        const y = Number(point.y)
        if (!isFinite(x) || !isFinite(y))
            return false
        const rects = entryRects && entryRects.length !== undefined
            ? entryRects : []
        for (let index = 0; index < rects.length; ++index) {
            if (rectContains(rects[index], x, y, entryMargin))
                return true
        }
        return distanceToPath(x, y) <= Math.max(1, bandWidth) / 2
    }
}
