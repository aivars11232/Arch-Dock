import QtQuick
import QtQuick.Window
import ArchDock.Rendering 1.0
import org.kde.plasma.core as PlasmaCore

// A free panel's folder contents on a path outside the dock, in a transparent
// window just large enough for them and the folder they unfold from
// (ADREP-TASK-003): "Along the dock" on the dock's own curve, which follows
// its shape, tilt and perspective, or a fan, an arc, a stack or a ring that
// opens from the folder along the way it faces out of the dock
// (LayoutEngine.folderShape()). The contents are drawn where they belong on
// the screen, relative to wherever the window really lands. A press beside
// them, or leaving the window, closes the folder through its closing motion.
PlasmaCore.Dialog {
    id: root
    property var snapshot: ({ status: "unavailable", entries: [] })
    property string folderTitle: ""
    // "track" (Along the dock), "fan", "arc", "stack", "ring" or "mobius".
    property string folderLayout: "track"
    // A Möbius band opens where a ring would.
    readonly property bool ringLike: folderLayout === "ring" || folderLayout === "mobius"
    property int folderSpeed: 260
    property string folderEasing: "outBack"
    property bool reducedMotion: false
    property bool showNames: true
    property var iconStyleDefinition: ({})
    // The stands under the children (the panel's pedestal settings).
    property var pedestalSettings: ({})
    property int iconSize: 48
    property int fanOpening: 90
    property int stackLength: 3
    property string ringSize: "small"
    // A function returning the dock's centre in the icon's parent's
    // coordinates, for a ring the size of the panel; null where the dock has
    // no radius.
    property var dockCentre: null
    // The panel's look, which the fan's sector and the ring are drawn in.
    property string appearance: "glass"
    property string customColor: ""
    property real panelOpacity: 0.9
    property real scrollSensitivity: 1
    // The clicked folder's drawn icon square and the way it faces out of the
    // dock; the dock's outer curve, a function returning { x, y, scale }
    // samples in the icon's parent's coordinates, the folder facing the
    // middle one, `extra` pixels further out than the dock's own step when
    // given; and the centres of the dock's other icons there.
    property Item folderItem: null
    property var outwardNormal: ({ x: 0, y: -1 })
    property var trackSamples: null
    property var obstacles: null
    // A function returning where a shape opens from on a 3D panel, { start,
    // direction } in the icon's parent's coordinates: the platform's edge
    // beside the folder and the way out of the platform there; null opens
    // the shape from the folder icon.
    property var outwardRay: null
    // A function returning a 3D panel's plane round the folder
    // (PanelScene.folderPlane), for a fan, arc, stack or ring that lies on
    // it; null, or a function returning null, builds the shape flat.
    property var folderPlane: null
    // A function returning what the contents keep clear of on a 3D panel -
    // the platform as drawn and its icons (PanelScene.folderKeepOut) - in
    // the icon's parent's coordinates; null elsewhere.
    property var keepOut: null
    // How far the contents stay from the panel as drawn.
    readonly property real keepOutMargin: 4
    property bool requested: false
    property bool interactionAllowed: true
    // Measured when the folder opens, in screen coordinates.
    property var screenSamples: []
    property var screenOutline: null
    property var screenShape: null
    property point screenOrigin: Qt.point(0, 0)
    property size windowSize: Qt.size(1, 1)
    readonly property real margin: 12
    readonly property var outlineStyle: LayoutEngine.themeStyle(appearance, customColor, iconSize)
    // How much room one child needs at the folder's own size: what the
    // smallest opening needs beside the panel.
    readonly property real childExtent: Math.max(content.cellWidth, content.cellHeight)
    signal childSelected(string childId)
    objectName: "folderTrackHost"
    type: PlasmaCore.Dialog.AppletPopup
    flags: Qt.Tool | Qt.FramelessWindowHint
    backgroundHints: PlasmaCore.Dialog.NoBackground
    color: "transparent"
    // A press elsewhere takes the keyboard from the folder, as Plasma's own
    // popups notice it (the contents' handler below); the folder then closes
    // through its closing motion rather than vanishing at once.
    hideOnWindowDeactivate: false
    onActiveChanged: if (!active && requested) closeFolder()
    visible: requested && interactionAllowed
    // Plasma puts the window on top of the attachment, centred across it:
    // the attachment alone chooses where the window lands. It is the visual
    // parent only while the folder is open: a hidden dialog that follows an
    // item on the dock slows every change of the dock, such as a drop.
    location: PlasmaCore.Types.BottomEdge
    property Item placementItem: null
    // A Dialog's default property is its main item, so the attachment's
    // component is a property.
    readonly property Component placementComponent: Component { Item {} }
    function releaseAttachment() {
        visualParent = null
        if (placementItem) placementItem.destroy()
        placementItem = null
    }
    onFolderItemChanged: releaseAttachment()
    Component.onDestruction: releaseAttachment()

    function local(point) {
        return { x: point.x - root.x, y: point.y - root.y }
    }
    // Where `item`'s points are on the screen. Its items are only moved,
    // turned and scaled, so three points give every other one; mapping each
    // of a curve's samples on its own, for every curve a folder tried, took a
    // good part of its opening. A transform that does more is mapped point
    // by point.
    function screenMapping(item) {
        const o = item.mapToGlobal(0, 0), a = item.mapToGlobal(1000, 0), b = item.mapToGlobal(0, 1000)
        const c = item.mapToGlobal(1000, 1000)
        const affine = function(x, y) {
            return { x: o.x + ((a.x - o.x) * x + (b.x - o.x) * y) / 1000,
                     y: o.y + ((a.y - o.y) * x + (b.y - o.y) * y) / 1000 }
        }
        const check = affine(1000, 1000)
        if (Math.abs(check.x - c.x) < 0.25 && Math.abs(check.y - c.y) < 0.25) return affine
        return function(x, y) {
            const point = item.mapToGlobal(x, y)
            return { x: point.x, y: point.y }
        }
    }
    // How much room there is beside the dock from `edge`, an x on the
    // screen, going `sideways`: to the screen's edge, or to the nearest other
    // panel about level with `y`.
    function roomBeside(edge, y, sideways, area, avoid) {
        let room = sideways > 0 ? area.x + area.width - edge : edge - area.x
        const reach = 2 * Math.max(content.cellWidth, content.cellHeight)
        for (const rect of avoid || []) {
            if (rect.y > y + reach || rect.y + rect.height < y - reach) continue
            const gap = sideways > 0 ? rect.x - edge : edge - (rect.x + rect.width)
            if (gap > -reach) room = Math.min(room, Math.max(0, gap))
        }
        return room
    }
    // Of shapes opening on either side of the dock, the better: seen whole
    // and on the screen, then holding more children at once, then the one
    // that moved out least to keep clear of the panels.
    function nearerShape(a, b) {
        if (!a || a.hidden) return b && !b.hidden ? b : null
        if (!b || b.hidden) return a
        const fitsA = !(a.overflow > 0.5), fitsB = !(b.overflow > 0.5)
        if (fitsA !== fitsB) return fitsA ? a : b
        if (!fitsA) return a.overflow <= b.overflow ? a : b
        if ((a.fewer || 0) !== (b.fewer || 0)) return (a.fewer || 0) < (b.fewer || 0) ? a : b
        return (Number(a.out) || 0) <= (Number(b.out) || 0) ? a : b
    }
    // A fan, arc, stack, grid or ring lying on a 3D panel's plane round the
    // folder (`plane`, from folderPlane), on the screen: `others` are the
    // dock's other icons there, `area` the screen and `avoid` the other free
    // panels as drawn. Null when no such shape can be seen whole.
    function planeShape(plane, others, toScreen, area, keep, avoid) {
        const onScreen = function(map) {
            return function(point) {
                const view = map(point.x, point.y, point.height || 0)
                if (!view) return null
                const at = toScreen(view.x, view.y)
                return { x: at.x, y: at.y, scale: view.scale }
            }
        }
        const viewed = onScreen(plane.toView)
        // Drawn as the panel's middle is seen, about where the shape opens.
        const alignedFrom = function(from) {
            const aligned = plane.alignedAt(from.x, from.y)
            return aligned ? onScreen(aligned) : viewed
        }
        // The folder's own icon there: no child is drawn larger.
        const folderSeen = alignedFrom(plane.origin)(plane.origin)
        // The shape opens from the rim; it moves out only as far as keeps its
        // children clear of the panel as drawn and of the other panels.
        const build = function(from, way, project, extra) {
            const options = {
                folder: from, outward: way, fromEdge: true,
                iconSize: iconSize, cellWidth: content.cellWidth, cellHeight: content.cellHeight,
                gap: 6, count: content.entries.length, fanOpening: fanOpening,
                stackLength: stackLength, stackSpan: 0, ringSize: ringSize, panelRadius: plane.radius,
                obstacles: others, planeObstacles: plane.icons || [], obstacleSize: iconSize,
                keepOut: keep, keepOutMargin: keepOutMargin, avoid: avoid, screen: area, screenMargin: margin,
                outlineMargin: outlineStyle.lineWidth / 2 + outlineStyle.blur, outlineWidth: outlineStyle.lineWidth,
                largestScale: folderSeen ? folderSeen.scale : 0,
                project: project }
            for (const key in extra || {}) options[key] = extra[key]
            return LayoutEngine.folderShape(folderLayout, options)
        }
        // The first of the shapes `tries` gives that is seen whole and fits
        // the screen, else the one that leaves it least.
        const firstFitting = function(tries) {
            let fallback = null
            for (const tryShape of tries) {
                const shape = tryShape()
                if (!shape || shape.hidden) continue
                if (!(shape.overflow > 0.5)) return shape
                if (!fallback || shape.overflow < fallback.overflow) fallback = shape
            }
            return fallback
        }
        const atFolder = alignedFrom(plane.origin)
        // A ring opens beside the dock on the side of the screen with more
        // room - to the right of a dock on the left half, to the left of one
        // on the right half - from the rim there; with other panels about,
        // on whichever side keeps it nearer and fuller.
        const middle = ringLike ? viewed({ x: 0, y: 0 }) : null
        if (middle) {
            const sideStart = function(sideways) {
                let best = null
                for (let degrees = 0; degrees < 360; degrees += 5) {
                    const turn = degrees * Math.PI / 180
                    const rim = plane.rimAt(turn)
                    const point = { x: rim * Math.cos(turn), y: rim * Math.sin(turn) }
                    const at = viewed(point)
                    if (at && (!best || sideways * at.x > sideways * best.at.x))
                        best = { at: at, point: point, turn: turn }
                }
                return best
            }
            const right = sideStart(1), left = sideStart(-1)
            const preferred = right && left
                ? (roomBeside(right.at.x, right.at.y, 1, area, avoid)
                   >= roomBeside(left.at.x, left.at.y, -1, area, avoid) ? 1 : -1)
                : middle.x <= area.x + area.width / 2 ? 1 : -1
            const beside = function(sideways) {
                const start = sideStart(sideways)
                return start ? build(start.point, { x: Math.cos(start.turn), y: Math.sin(start.turn) },
                                     alignedFrom(start.point)) : null
            }
            let chosen = beside(preferred)
            // Opening at once beside the rim, it needs no other side.
            const settled = chosen && !chosen.hidden && !(chosen.overflow > 0.5) && !(chosen.fewer > 0)
                && !(Number(chosen.out) > 2 * Math.max(content.cellWidth, content.cellHeight))
            if (avoid.length && !settled)
                chosen = nearerShape(chosen, beside(-preferred))
            return chosen && !chosen.hidden ? chosen : null
        }
        const origin = plane.origin, facing = plane.outward
        // A fan hugs the panel: its children follow the rim round the folder.
        if (folderLayout === "fan") {
            const shape = build(origin, facing, atFolder,
                                { hug: { rimAt: plane.rimAt, turn: Math.atan2(origin.y, origin.x) } })
            return shape && !shape.hidden ? shape : null
        }
        // A stack lies along the panel's side nearest the folder: up and
        // down beside its left or right side, across along its near or far
        // side, centred on that side and no longer than it, a little way out
        // of the rim; where it is not seen whole there, along the next
        // nearest side.
        if (folderLayout === "stack" && (plane.icons || []).length >= 2) {
            let box = { left: Infinity, right: -Infinity, top: Infinity, bottom: -Infinity }
            for (const point of plane.icons)
                box = { left: Math.min(box.left, point.x), right: Math.max(box.right, point.x),
                        top: Math.min(box.top, point.y), bottom: Math.max(box.bottom, point.y) }
            const sides = [
                { gap: Math.abs(origin.x - box.left), from: { x: -plane.rimAt(Math.PI), y: box.top },
                  way: { x: 0, y: 1 }, span: box.bottom - box.top, out: { x: -1, y: 0 } },
                { gap: Math.abs(box.right - origin.x), from: { x: plane.rimAt(0), y: box.top },
                  way: { x: 0, y: 1 }, span: box.bottom - box.top, out: { x: 1, y: 0 } },
                { gap: Math.abs(origin.y - box.top), from: { x: box.left, y: -plane.rimAt(-Math.PI / 2) },
                  way: { x: 1, y: 0 }, span: box.right - box.left, out: { x: 0, y: -1 } },
                { gap: Math.abs(box.bottom - origin.y), from: { x: box.left, y: plane.rimAt(Math.PI / 2) },
                  way: { x: 1, y: 0 }, span: box.right - box.left, out: { x: 0, y: 1 } }]
            sides.sort(function(a, b) { return a.gap - b.gap })
            // A side where another panel would push the stack far out is
            // passed over for the next one; failing all, the stack that
            // stands nearest.
            const near = 2 * Math.max(content.cellWidth, content.cellHeight)
            let nearest = null
            for (const side of sides) {
                const shape = build(side.from, side.way, atFolder, { stackSpan: side.span, sideOut: side.out })
                if (!shape || shape.hidden || shape.overflow > 0.5) continue
                if (!(Number(shape.out) > near)) return shape
                if (!nearest || shape.out < nearest.out) nearest = shape
            }
            return nearest || firstFitting(sides.map(function(side) {
                return function() { return build(side.from, side.way, atFolder, { stackSpan: side.span, sideOut: side.out }) }
            }))
        }
        const shape = build(origin, facing, atFolder)
        return shape && !shape.hidden ? shape : null
    }
    function openFolder() {
        if (!interactionAllowed || !folderItem) return false
        // mapToGlobal does not notify when the rendered dock moves: measure
        // the dock, the folder and the screen now.
        const scene = folderItem.parent
        const toScreen = screenMapping(scene)
        const centre = folderItem.mapToGlobal(folderItem.width / 2, folderItem.height / 2)
        const screen = folderItem.Screen
        const area = { x: screen.virtualX, y: screen.virtualY, width: screen.width, height: screen.height }
        let samples = []
        let shape = null
        // The panel as drawn, on the screen: its outline and its icons.
        const keepOutView = typeof keepOut === "function" ? keepOut() : null
        const keep = keepOutView ? {
            hull: (keepOutView.hull || []).map(function(point) { return toScreen(point.x, point.y) }),
            rects: (keepOutView.rects || []).map(function(rect) {
                const a = toScreen(rect.x, rect.y), b = toScreen(rect.x + rect.width, rect.y + rect.height)
                return { x: Math.min(a.x, b.x), y: Math.min(a.y, b.y),
                         width: Math.abs(b.x - a.x), height: Math.abs(b.y - a.y) }
            })
        } : null
        const rectangles = [{ x: centre.x - iconSize, y: centre.y - iconSize,
                              width: 2 * iconSize, height: 2 * iconSize }]
        // The other free panels as drawn, which no child covers or touches.
        const avoid = ((snapshot && snapshot.freePanelAreas) || []).filter(function(other) {
            return other && Number(other.width) > 0 && Number(other.height) > 0
        }).map(function(other) {
            return { x: Number(other.x) - keepOutMargin, y: Number(other.y) - keepOutMargin,
                     width: Number(other.width) + 2 * keepOutMargin, height: Number(other.height) + 2 * keepOutMargin }
        })
        const overOther = function(rect) {
            return avoid.some(function(other) {
                return rect.x < other.x + other.width && rect.x + rect.width > other.x
                    && rect.y < other.y + other.height && rect.y + rect.height > other.y
            })
        }
        if (folderLayout === "track") {
            if (typeof trackSamples !== "function") return false
            // The dock's own pitch, not the one a shape opened with before.
            screenShape = null
            screenOutline = null
            const others = typeof obstacles === "function" ? obstacles().map(function(point) {
                return toScreen(point.x, point.y)
            }) : []
            // Whether a child at `place` stays on the screen and keeps clear
            // of the dock: on a 3D panel of the panel as drawn - no child
            // covers or touches it - elsewhere of the dock's other icons.
            function onScreen(place) {
                const scale = Math.max(0.35, Number(place.scale) || 1)
                const left = place.x - content.cellWidth * scale / 2, top = place.y - iconSize * scale / 2
                return left >= area.x && left + content.cellWidth * scale <= area.x + area.width
                    && top >= area.y && top + content.cellHeight * scale <= area.y + area.height
            }
            function clear(place) {
                const scale = Math.max(0.35, Number(place.scale) || 1)
                const reach = iconSize / 2 + 4
                const left = place.x - content.cellWidth * scale / 2, top = place.y - iconSize * scale / 2
                if (!onScreen(place)) return false
                if (overOther({ x: left, y: top, width: content.cellWidth * scale,
                                height: content.cellHeight * scale })) return false
                if (keep) return !LayoutEngine.rectTouchesKeepOut({ x: left, y: top,
                    width: content.cellWidth * scale, height: content.cellHeight * scale }, keep, keepOutMargin)
                return others.every(function(icon) {
                    return left >= icon.x + reach || left + content.cellWidth * scale <= icon.x - reach
                        || top >= icon.y + reach || top + content.cellHeight * scale <= icon.y - reach
                })
            }
            // The curve a step out of the dock, a little further where a
            // tilted or projected dock would bring a child onto one of its
            // icons: every place a child stands or passes keeps clear. On a
            // 3D panel the step lies on the panel's own plane, where it looks
            // short on a steeply tilted panel, and the children stand clear
            // of the panel as drawn, so the curve may move out by up to six
            // icons.
            // The curve reaches as far round the dock as the children need:
            // a quarter of the way at first on a 3D panel, a third on a flat
            // or baked one, then further, up to three quarters; the rest
            // scroll in with the wheel. A curve every child keeps clear of the
            // icons on is preferred. When none does, the nearest and shortest
            // that keeps every child on the screen is used, and failing that
            // the first: never the furthest, which spread the children far
            // from a steeply tilted dock across the screen.
            const reach = keep ? 6 * iconSize : 64, stride = keep ? Math.max(4, iconSize / 4) : 8
            let layout = null, chosen = null, cleared = null, nearest = null, first = null
            spans: for (let span = keep ? Math.PI / 2 : 2 * Math.PI / 3; span <= 3 * Math.PI / 2 + 1e-9;
                        span += Math.PI / 6) {
                for (let extra = 0; extra <= reach + 1e-9; extra += stride) {
                    samples = trackSamples(extra, span).map(function(sample) {
                        const point = toScreen(sample.x, sample.y)
                        return { x: point.x, y: point.y, scale: sample.scale }
                    })
                    if (samples.length < 2) return false
                    layout = LayoutEngine.folderTrackLayout(samples, content.entries.length, content.pitch, 0)
                    const half = LayoutEngine.folderTrackLayout(samples, content.entries.length, content.pitch, 0.5)
                    const places = layout.entries.concat(half.entries).filter(function(placed) {
                        return placed.visibility > 0
                    })
                    chosen = { samples: samples, layout: layout }
                    if (!first) first = chosen
                    if (!nearest && places.every(onScreen)) nearest = chosen
                    if (!places.every(clear)) continue
                    cleared = chosen
                    if (!layout.windowed) break spans
                    break
                }
            }
            chosen = cleared || nearest || first || chosen
            samples = chosen.samples
            layout = chosen.layout
            for (const placed of layout.entries.concat(layout.ends || [])) {
                const scale = Math.max(0.35, Number(placed.scale) || 1)
                rectangles.push({ x: placed.x - content.cellWidth * scale / 2,
                                  y: placed.y - iconSize * scale / 2,
                                  width: content.cellWidth * scale, height: content.cellHeight * scale })
            }
        } else {
            const others = typeof obstacles === "function" ? obstacles().map(function(point) {
                return toScreen(point.x, point.y)
            }) : []
            // On a 3D panel the shape lies on the panel's own plane round the
            // folder, tilted as the panel is; one that cannot be seen whole
            // there is built flat on the screen instead.
            // A fan, an arc and a stack open as on a flat panel, from the
            // platform's edge beside the folder, as if the folder opened a
            // panel of that shape that is not drawn; a ring and a grid lie on
            // the 3D panel's plane.
            const flatShape = ["fan", "arc", "stack"].includes(folderLayout)
            const plane = !flatShape && typeof folderPlane === "function" ? folderPlane() : null
            shape = plane ? planeShape(plane, others, toScreen, area, keep, avoid) : null
            if (shape) {
                samples = shape.samples
            } else {
                // The dock's radius where the folder stands on it.
                const middle = typeof dockCentre === "function" ? dockCentre() : null
                const dock = middle ? scene.mapToGlobal(middle.x, middle.y) : null
                const panelRadius = dock ? Math.hypot(centre.x - dock.x, centre.y - dock.y) : 0
                // On a 3D panel the shape opens from the platform's edge beside
                // the folder, the way out of the platform there; elsewhere from
                // the folder icon, the way it faces out of the dock.
                let origin = { x: centre.x, y: centre.y }
                let facing = outwardNormal || ({ x: 0, y: -1 })
                let fromEdge = false
                let stackSpan = 0
                const ray = typeof outwardRay === "function" ? outwardRay() : null
                if (ray && ray.start && ray.direction) {
                    const from = scene.mapToGlobal(ray.start.x, ray.start.y)
                    const to = scene.mapToGlobal(ray.start.x + ray.direction.x, ray.start.y + ray.direction.y)
                    const length = Math.hypot(to.x - from.x, to.y - from.y)
                    if (length > 1e-6) {
                        origin = { x: from.x, y: from.y }
                        facing = { x: (to.x - from.x) / length, y: (to.y - from.y) / length }
                        fromEdge = true
                    }
                }
                // A stack lies against the dock's side nearest the folder: upright
                // beside its left or right side, level above or below it, centred
                // on the dock and no longer than it, one cell out. The dock's box
                // on the screen is its icons' box, the same on every tier; where
                // the chosen side would leave the screen, the opposite side is
                // used.
                if (folderLayout === "stack" && dock && others.length >= 2) {
                    let box = { left: centre.x, right: centre.x, top: centre.y, bottom: centre.y }
                    for (const point of others)
                        box = { left: Math.min(box.left, point.x), right: Math.max(box.right, point.x),
                                top: Math.min(box.top, point.y), bottom: Math.max(box.bottom, point.y) }
                    box = { left: box.left - iconSize / 2, right: box.right + iconSize / 2,
                            top: box.top - iconSize / 2, bottom: box.bottom + iconSize / 2 }
                    const half = { x: content.cellWidth / 2 + 4, y: content.cellHeight / 2 + 4 }
                    const sides = [
                        { id: "left", gap: Math.abs(centre.x - box.left), vertical: true,
                          x: box.left - half.x, fits: box.left - 2 * half.x >= area.x + margin },
                        { id: "right", gap: Math.abs(box.right - centre.x), vertical: true,
                          x: box.right + half.x, fits: box.right + 2 * half.x <= area.x + area.width - margin },
                        { id: "top", gap: Math.abs(centre.y - box.top), vertical: false,
                          y: box.top - half.y, fits: box.top - 2 * half.y >= area.y + margin },
                        { id: "bottom", gap: Math.abs(box.bottom - centre.y), vertical: false,
                          y: box.bottom + half.y, fits: box.bottom + 2 * half.y <= area.y + area.height - margin }]
                    sides.sort(function(a, b) { return a.gap - b.gap })
                    const side = sides.find(function(candidate) { return candidate.fits }) || sides[0]
                    if (side.vertical) {
                        origin = { x: side.x, y: box.top }
                        facing = { x: 0, y: 1 }
                        stackSpan = box.bottom - box.top
                    } else {
                        origin = { x: box.left, y: side.y }
                        facing = { x: 1, y: 0 }
                        stackSpan = box.right - box.left
                    }
                    fromEdge = true
                }
                // A ring opens beside the dock on the side with more of the
                // screen: to the right of a dock on the left half, to the left of
                // one on the right half, from the dock's rim there; with other
                // panels about, on whichever side keeps it nearer and fuller.
                // Where it would leave the screen above or below, it is lowered
                // or raised to fit.
                const besideDock = ringLike && dock && panelRadius > 0
                const preferred = !dock ? 1
                    : roomBeside(dock.x + panelRadius, dock.y, 1, area, avoid)
                      >= roomBeside(dock.x - panelRadius, dock.y, -1, area, avoid) ? 1 : -1
                if (besideDock) {
                    origin = { x: dock.x + preferred * panelRadius, y: dock.y }
                    facing = { x: preferred, y: 0 }
                    fromEdge = true
                }
                const build = function(from, way) {
                    return LayoutEngine.folderShape(folderLayout, {
                        folder: from, outward: way || facing, fromEdge: fromEdge,
                        iconSize: iconSize, cellWidth: content.cellWidth, cellHeight: content.cellHeight,
                        gap: 6, count: content.entries.length, fanOpening: fanOpening,
                        stackLength: stackLength, stackSpan: stackSpan, ringSize: ringSize, panelRadius: panelRadius,
                        obstacles: others, obstacleSize: iconSize, avoid: avoid, screen: area, screenMargin: margin,
                        outlineMargin: outlineStyle.lineWidth / 2 + outlineStyle.blur })
                }
                const besideAt = function(sideways) {
                    const from = { x: dock.x + sideways * panelRadius, y: dock.y }
                    const way = { x: sideways, y: 0 }
                    const built = build(from, way)
                    const below = built.bounds.y + built.bounds.height + margin - (area.y + area.height)
                    const above = area.y + margin - built.bounds.y
                    return below > 0 || above > 0
                        ? build({ x: from.x, y: from.y + (below > 0 ? -below : above) }, way) : built
                }
                shape = besideDock ? besideAt(preferred) : build(origin)
                if (besideDock && avoid.length && (shape.fewer > 0 || shape.overflow > 0.5
                        || Number(shape.out) > 2 * Math.max(content.cellWidth, content.cellHeight)))
                    shape = nearerShape(shape, besideAt(-preferred)) || shape
                samples = shape.samples.map(function(point) { return { x: point.x, y: point.y, scale: 1 } })
            }
            rectangles.push(shape.bounds)
        }
        const bounds = LayoutEngine.unitedBounds(rectangles)
        // The window stays on the screen, where Plasma puts it as asked: one
        // that reached past an edge would be moved, or flipped to the
        // folder's other side, away from where its contents stand.
        const left = Math.max(area.x, bounds.x - margin)
        const top = Math.max(area.y, bounds.y - margin)
        const right = Math.min(area.x + area.width, bounds.x + bounds.width + margin)
        const bottom = Math.min(area.y + area.height, bounds.y + bounds.height + margin)
        screenSamples = samples
        screenShape = shape
        screenOutline = shape ? shape.outline : null
        screenOrigin = Qt.point(centre.x, centre.y)
        windowSize = Qt.size(Math.max(1, Math.floor(right - left)), Math.max(1, Math.floor(bottom - top)))
        // Against the top of an attachment centred under the window's bottom
        // edge: the window's top-left lands on (left, top). The attachment is
        // twice as wide as the window: Plasma moves a popup whose narrow
        // attachment leaves the middle of the screen inside it to that
        // middle (PlasmaQuick::Dialog::popupPosition).
        const corner = folderItem.mapToGlobal(0, 0)
        if (!placementItem) placementItem = placementComponent.createObject(folderItem)
        placementItem.x = left - windowSize.width / 2 - corner.x
        placementItem.y = top + windowSize.height - corner.y
        placementItem.width = 2 * windowSize.width
        placementItem.height = 1
        visualParent = placementItem
        // Opened again while it was closing: it opens again from where it is.
        if (requested && content.closing) content.reopen()
        requested = true
        requestActivate()
        content.forceActiveFocus()
        return true
    }
    // Plays the closing motion, then hides the window.
    function closeFolder() {
        if (!requested) return
        if (visible && content.opened) content.close()
        else requested = false
    }
    onVisibleChanged: if (!visible) {
        requested = false
        visualParent = null
    }
    onInteractionAllowedChanged: if (!interactionAllowed) requested = false
    mainItem: FolderTrack {
        id: content
        width: root.windowSize.width
        height: root.windowSize.height
        shape: root.folderLayout
        snapshot: root.snapshot
        folderTitle: root.folderTitle
        duration: root.folderSpeed
        easing: root.folderEasing
        reducedMotion: root.reducedMotion
        showNames: root.showNames
        iconStyleDefinition: root.iconStyleDefinition
        pedestalSettings: root.pedestalSettings
        iconSize: root.iconSize
        scrollSensitivity: root.scrollSensitivity
        opened: root.visible
        // The path where the window really is: Plasma may keep the window
        // inside the screen, and the contents still stand where they belong.
        samples: root.screenSamples.map(function(sample) {
            const point = root.local(sample)
            return { x: point.x, y: point.y, scale: sample.scale }
        })
        closedPath: root.screenShape !== null && root.screenShape.closed === true
        startAtFolder: root.screenShape !== null && root.screenShape.start === "start"
        pathCapacity: root.screenShape !== null ? Number(root.screenShape.capacity || 0) : 0
        pathPitch: root.screenShape !== null ? Number(root.screenShape.pitch || 0) : 0
        measureAsDrawn: root.screenShape !== null && root.screenShape.projected === true
        ringLimits: root.screenShape !== null && root.screenShape.ringCapacities ? root.screenShape.ringCapacities : []
        lobedPath: root.screenShape !== null && root.screenShape.lobes === true
        gridCells: root.screenShape !== null && root.screenShape.grid ? ({
            columns: root.screenShape.grid.columns, rows: root.screenShape.grid.rows,
            cells: root.screenShape.grid.cells.map(function(line) {
                return line.map(function(cell) {
                    if (!cell) return null
                    const point = root.local(cell)
                    return { x: point.x, y: point.y, scale: Number(cell.scale) || 1 }
                })
            }) }) : null
        outline: root.screenOutline ? ({ closed: root.screenOutline.closed === true,
            strokeFrom: Number(root.screenOutline.strokeFrom) || 0,
            points: root.screenOutline.points.map(root.local),
            loops: (root.screenOutline.loops || []).map(function(loop) { return loop.map(root.local) }),
            evenOdd: root.screenOutline.evenOdd === true,
            // A Möbius band, drawn as one solid ribbon between its sides.
            ribbon: root.screenOutline.ribbon ? {
                left: root.screenOutline.ribbon.left.map(root.local),
                right: root.screenOutline.ribbon.right.map(root.local),
                from: root.screenOutline.ribbon.from, twist: root.screenOutline.ribbon.twist } : undefined }) : null
        rings: root.screenShape !== null && root.screenShape.rings ? root.screenShape.rings.map(function(loop) {
            return loop.map(function(sample) {
                const point = root.local(sample)
                return { x: point.x, y: point.y, scale: Number(sample.scale) || 1 }
            })
        }) : []
        outlineFilled: root.folderLayout === "fan" && root.screenOutline !== null && root.screenOutline.closed === true
        outlineStyle: root.outlineStyle
        outlineOpacity: root.panelOpacity
        shapeInfo: {
            const shape = root.screenShape
            if (!shape) return null
            return { shape: shape.shape, direction: shape.direction, turn: shape.turn,
                     apex: shape.apex ? root.local(shape.apex) : null,
                     centre: shape.centre ? root.local(shape.centre) : null,
                     radius: shape.radius || 0, opening: shape.opening || 0, span: shape.span || 0,
                     pitch: shape.pitch, stackLength: shape.stackLength || 0,
                     ringSize: shape.ringSize || "", fewer: shape.fewer || 0 }
        }
        expansionOrigin: Qt.point(root.screenOrigin.x - root.x, root.screenOrigin.y - root.y)
        onChildSelected: childId => {
            root.childSelected(childId)
            root.closeFolder()
        }
        onDismissRequested: root.closeFolder()
        onCloseFinished: root.requested = false
        onActiveFocusChanged: if (!activeFocus && root.requested && root.visible) root.closeFolder()
    }
}
