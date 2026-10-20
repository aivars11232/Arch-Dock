.pragma library
.import "DecorationScene.js" as Scene
.import "DecorationCatalogue.js" as Catalogue

// What the decoration baker (the ArchDock.Decorations plugin) asks for on
// its own thread, so the panel never waits while a decoration's scenes are
// worked out: how many frames a decoration's motion is drawn in, the band's
// half-width that fits it on an edge panel, and the scene of each frame.
//
// The job's input (JSON text, made by the baker from its properties):
//   decoration: a catalogue id, or a decoration's own data;
//   path: { points: [{x, y}], closed, outward }; halfWidth; slots;
//   choices: the viewer's colours; animated: draw the motion's frames;
//   fit: { out, in } the room (pixels) the band has across, toward the
//        outward side and the other, or null to draw it as large as asked;
//   ends: how far (half-widths) an open band runs on past its first and
//         last icons, as the decorations were designed;
//   clip: { x, y, width, height, fade } the window the panel is seen
//         through, or null.

function inputOf(text) {
    try {
        const input = JSON.parse(String(text || "{}"))
        return input && typeof input === "object" ? input : ({})
    } catch (error) {
        return ({})
    }
}

function decorationOf(input) {
    if (typeof input.decoration === "string") return Catalogue.decoration(input.decoration) || null
    return input.decoration && typeof input.decoration === "object" && input.decoration.contour ? input.decoration : null
}

// An open path run on straight past both its ends by `extra` pixels, and
// the icons' places on it.
function extended(path, slots, extra) {
    const points = path.points
    if (path.closed || !(extra > 0) || points.length < 2) return { path: path, slots: slots }
    const along = function(a, b) {
        const length = Math.hypot(b.x - a.x, b.y - a.y) || 1
        return { x: (b.x - a.x) / length, y: (b.y - a.y) / length }
    }
    const first = points[0], last = points[points.length - 1]
    const head = along(points[1], first), tail = along(points[points.length - 2], last)
    const longer = [{ x: first.x + head.x * extra, y: first.y + head.y * extra }].concat(points,
        [{ x: last.x + tail.x * extra, y: last.y + tail.y * extra }])
    return { path: Object.assign({}, path, { points: longer }),
             slots: (slots || []).map(function(s) { return Number(s) + extra }) }
}

// The path and icon places a decoration is drawn on, at a half-width.
function laidOut(input, halfWidth) {
    return extended(input.path, input.slots || [], Math.max(0, Number(input.ends) || 0) * halfWidth)
}

// How far a scene reaches out from its path, toward the outward side and
// the other (pixels), measured from the path's first point along `outward`:
// an edge panel's path is straight, so that is the distance across it.
function reachAcross(scene, path) {
    const outward = path && path.outward
    const first = path && path.points && path.points[0]
    if (!outward || !first) return { out: 0, in: 0 }
    const length = Math.hypot(outward.x, outward.y) || 1
    const nx = outward.x / length, ny = outward.y / length
    let out = 0, inward = 0
    const see = function(x, y, r) {
        const d = (x - first.x) * nx + (y - first.y) * ny
        out = Math.max(out, d + r)
        inward = Math.max(inward, -d + r)
    }
    for (const e of scene.elements) {
        if (e.type === "fill") { for (const ring of e.rings) for (const p of ring) see(p[0], p[1], 0) }
        else if (e.type === "stroke") { for (const p of e.points) see(p[0], p[1], e.width / 2) }
        else see(e.centre[0], e.centre[1], e.radius)
    }
    return { out: out, in: inward }
}

// What the baker needs before the frames: how many there are (0 for a still
// decoration), the loop's length in seconds, and the half-width the band is
// drawn at - smaller than asked when an edge panel is too thin to hold it.
function prepare(text) {
    const input = inputOf(text)
    const decoration = decorationOf(input)
    if (!decoration) return JSON.stringify({ error: "unknown-decoration" })
    if (!input.path || !input.path.points || input.path.points.length < 2) return JSON.stringify({ error: "no-path" })
    let halfWidth = Math.max(1, Number(input.halfWidth) || 24)
    if (input.fit) {
        const probeLayout = laidOut(input, halfWidth)
        const probe = Scene.scene(decoration, probeLayout.path, halfWidth, probeLayout.slots, { choices: input.choices || ({}) })
        const reach = reachAcross(probe, input.path)
        const room = { out: Math.max(1, Number(input.fit.out) || 0), in: Math.max(1, Number(input.fit.in) || 0) }
        const share = Math.min(1, reach.out > room.out ? room.out / reach.out : 1, reach.in > room.in ? room.in / reach.in : 1)
        halfWidth = Math.max(4, halfWidth * share)
    }
    const frames = input.animated ? Scene.motionFrames(decoration) : 0
    return JSON.stringify({ frames: frames, loop: frames ? Scene.motionLoop(decoration) : 0, halfWidth: halfWidth })
}

// The scene of one frame (`phase` 0 to 1 through the loop; below 0 for a
// still decoration), as JSON text.
function sceneText(text, phase, halfWidth) {
    const input = inputOf(text)
    const decoration = decorationOf(input)
    if (!decoration) return ""
    const options = { choices: input.choices || ({}) }
    if (Number(phase) >= 0) options.phase = Number(phase)
    const layout = laidOut(input, Number(halfWidth))
    const scene = Scene.scene(decoration, layout.path, Number(halfWidth), layout.slots, options)
    if (input.clip) scene.clip = input.clip
    return JSON.stringify(scene)
}
