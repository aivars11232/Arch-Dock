.pragma library

// Pointer arithmetic for the desktop 3D gizmo. Points are view coordinates
// ({x, y}); nothing here knows about the scene, so it can be tested alone.

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value))
}

// How far the pointer moved along an axis that is drawn on screen from
// `origin` to `axisEnd`, in the units of the axis: `axisLength` is the scene
// length the drawn axis stands for. An axis seen end-on has no reliable screen
// direction and gives no travel.
function axisTravel(origin, axisEnd, axisLength, delta) {
    const dx = axisEnd.x - origin.x
    const dy = axisEnd.y - origin.y
    const drawn = dx * dx + dy * dy
    if (drawn < 16)
        return 0
    return (delta.x * dx + delta.y * dy) / drawn * axisLength
}

// The signed angle, in degrees, the pointer swept around `centre` from
// `start` to `current`, the short way round.
function sweptDegrees(centre, start, current) {
    const from = Math.atan2(start.y - centre.y, start.x - centre.x)
    const to = Math.atan2(current.y - centre.y, current.x - centre.x)
    const degrees = (to - from) * 180 / Math.PI
    return ((degrees % 360) + 540) % 360 - 180
}

// Uniform scale from how far the pointer is from `centre` now, compared with
// where the drag started. A start on the centre itself gives no scale.
function scaleRatio(centre, start, current) {
    const from = Math.hypot(start.x - centre.x, start.y - centre.y)
    if (from < 4)
        return 1
    return Math.hypot(current.x - centre.x, current.y - centre.y) / from
}

function snapped(value, step) {
    return step > 0 ? Math.round(value / step) * step : value
}

// Blender's conventions: Ctrl snaps to coarse steps, Shift slows the drag to a
// tenth for fine adjustment.
function modifierSettings(modifiers) {
    return {
        snap: (modifiers & Qt.ControlModifier) !== 0,
        precision: (modifiers & Qt.ShiftModifier) !== 0 ? 0.1 : 1
    }
}
