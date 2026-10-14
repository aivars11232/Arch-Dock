.pragma library
.import "LayoutEngine.js" as LayoutEngine

// Generated 3D geometry, as plain data in the theme mesh format
// ("org.archdock.mesh": positions, normals, uv0s, indexes) so the same
// IconStyle3D draws it as it draws a theme's own mesh.
//
// Two shapes are generated here:
// - `pedestal()`: the column an icon stands on.
// - `platform(spec)`: the platform a look is drawn on when its theme ships no
//   3D mesh of its own. It follows the panel's layout path exactly, so every
//   icon stands on it: a circle, an ellipse, a regular polygon, a star, the
//   open 300-degree radial arc or a Möbius strip.
//
// Frame: x right, y up (screen up), z out of the platform's top. The layout's
// track becomes the platform's centre line at `trackRadius` (0.84, where the
// icons stand); the outer edge stays within `reach` (1.05), the extent the
// scene fits into its panel. Nothing here knows about the scene, so it can be
// tested on its own.

var format = "org.archdock.mesh"
var trackRadius = 0.84
var reach = 1.05
// How far the band's edges reach past its flat top.
var innerBevel = 0.03
var outerBevel = 0.05
// Half the flat top's width, of the outer radius: never narrower than a
// pedestal's footprint, so every icon stands on the top. The scene's Auto
// width (0) follows the flat look's line down to this floor.
var minimumBand = 0.06
var maximumBand = 0.16
// A star's notches lie at this share of its tips' radius. The flat layout
// places its icons on the same star (LayoutEngine.js, starInnerRadius).
var starInnerRadius = 0.46
// The top face's height and the underside's, as in the shipped theme meshes.
var topHeight = 0.1
var bottomHeight = -0.08

function finite(value, fallback) {
    const number = Number(value)
    return Number.isFinite(number) ? number : fallback
}

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value))
}

// A mesh under construction. Every face is added with the normal it must
// face; its triangles are wound to match, so back-face culling keeps the
// faces the camera can see.
function MeshBuilder() {
    this.positions = []
    this.normals = []
    this.uv0s = []
    this.colors = []
    this.indexes = []
    // Where the texture's u runs on the surface, per vertex, and the
    // across direction with it: a relief map needs both to light its bumps.
    this.tangents = []
    this.binormals = []
}

MeshBuilder.prototype.vertex = function(point, normal, color, uv, tangent) {
    this.positions.push([point[0], point[1], point[2]])
    const n = normalized(normal)
    this.normals.push(n)
    // Given texture coordinates, or planar ones seen from above, where a
    // texture drawn as the top view lands where it is drawn.
    this.uv0s.push(uv ? [uv[0], uv[1]] : [0.5 + point[0] / 2, 0.5 + point[1] / 2])
    if (tangent) {
        const along = normalized(tangent)
        this.tangents.push(along)
        this.binormals.push(normalized(crossProduct(n, along)))
    }
    if (color)
        this.colors.push([color[0], color[1], color[2], color.length > 3 ? color[3] : 1])
    return this.positions.length - 1
}

// One triangle, wound so that it faces `facing`.
MeshBuilder.prototype.triangle = function(a, b, c, facing) {
    const p = this.positions
    const cross = crossProduct(subtract(p[b], p[a]), subtract(p[c], p[a]))
    if (dot(cross, facing) >= 0)
        this.indexes.push(a, b, c)
    else
        this.indexes.push(a, c, b)
}

MeshBuilder.prototype.result = function(extra) {
    const mesh = {
        format: format,
        version: 1,
        positions: this.positions,
        normals: this.normals,
        uv0s: this.uv0s,
        indexes: this.indexes
    }
    if (this.colors.length === this.positions.length && this.colors.length > 0)
        mesh.colors = this.colors
    if (this.tangents.length === this.positions.length && this.tangents.length > 0) {
        mesh.tangents = this.tangents
        mesh.binormals = this.binormals
    }
    const keys = Object.keys(extra || ({}))
    for (let index = 0; index < keys.length; ++index)
        mesh[keys[index]] = extra[keys[index]]
    return mesh
}

function subtract(a, b) { return [a[0] - b[0], a[1] - b[1], a[2] - b[2]] }
function dot(a, b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] }
function crossProduct(a, b) {
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
}
function normalized(v) {
    const length = Math.hypot(v[0], v[1], v[2])
    return length > 1e-12 ? [v[0] / length, v[1] / length, v[2] / length] : [0, 0, 1]
}

// The column an icon stands on: radius 1 about the z axis, from z 0 (its
// foot on the platform) to z 1 (where the icon stands). The foot is hidden
// in the platform, so it has no cap.
function pedestal(segments) {
    const count = Math.max(8, Math.round(finite(segments, 24)))
    const mesh = new MeshBuilder()
    for (let index = 0; index < count; ++index) {
        const a = index / count * Math.PI * 2
        const b = (index + 1) / count * Math.PI * 2
        const na = [Math.cos(a), Math.sin(a), 0], nb = [Math.cos(b), Math.sin(b), 0]
        const middle = [Math.cos((a + b) / 2), Math.sin((a + b) / 2), 0]
        const ua = index / count, ub = (index + 1) / count
        const ta = [-Math.sin(a), Math.cos(a), 0], tb = [-Math.sin(b), Math.cos(b), 0]
        const v0 = mesh.vertex([na[0], na[1], 0], na, null, [ua, 1], ta)
        const v1 = mesh.vertex([nb[0], nb[1], 0], nb, null, [ub, 1], tb)
        const v2 = mesh.vertex([nb[0], nb[1], 1], nb, null, [ub, 0], tb)
        const v3 = mesh.vertex([na[0], na[1], 1], na, null, [ua, 0], ta)
        mesh.triangle(v0, v1, v2, middle)
        mesh.triangle(v0, v2, v3, middle)
        const centre = mesh.vertex([0, 0, 1], [0, 0, 1], null, null, [1, 0, 0])
        const t0 = mesh.vertex([na[0], na[1], 1], [0, 0, 1], null, null, [1, 0, 0])
        const t1 = mesh.vertex([nb[0], nb[1], 1], [0, 0, 1], null, null, [1, 0, 0])
        mesh.triangle(centre, t0, t1, [0, 0, 1])
    }
    return mesh.result()
}

// A solid turned about the z axis from a profile of (radius, height)
// points, bottom to top: a plate, a disc, a saucer. The first and last
// points on the axis close the solid; a profile that starts and ends off
// the axis (`closed`) is a loop, like a ring. Every vertex carries texture
// coordinates (u once round, v along the profile) and tangents, so a
// material's relief map lights it.
function lathe(profile, segments, closed) {
    const count = Math.max(8, Math.round(finite(segments, 32)))
    const points = profile.slice()
    if (closed) points.push(points[0])
    const lengths = [0]
    for (let index = 1; index < points.length; ++index)
        lengths.push(lengths[index - 1] + Math.hypot(points[index][0] - points[index - 1][0],
                                                     points[index][1] - points[index - 1][1]))
    const total = Math.max(1e-6, lengths[lengths.length - 1])
    const reach = Math.max(1e-6, ...points.map(function(point) { return point[0] }))
    const mesh = new MeshBuilder()
    for (let step = 0; step + 1 < points.length; ++step) {
        const a = points[step], b = points[step + 1]
        const dr = b[0] - a[0], dz = b[1] - a[1]
        const length = Math.hypot(dr, dz)
        if (length < 1e-9) continue
        // The profile's outward normal in the (r, z) plane.
        const nr = dz / length, nz = -dr / length
        // A flat top or bottom (a segment reaching the axis) takes the
        // texture flat across it, like a label laid on a lid, instead of
        // wound to a point at its middle, which pinched a swirling texture
        // into a dark eye (the owner's "donut" discs, 2026-10-12).
        const cap = a[0] <= 1e-9 || b[0] <= 1e-9
        for (let index = 0; index < count; ++index) {
            const t0 = index / count * Math.PI * 2, t1 = (index + 1) / count * Math.PI * 2
            const ring = []
            for (const [turn, u] of [[t0, index / count], [t1, (index + 1) / count]]) {
                const c = Math.cos(turn), s = Math.sin(turn)
                const normal = [nr * c, nr * s, nz]
                const tangent = cap ? [1, 0, 0] : [-s, c, 0]
                const uvA = cap ? [0.5 + a[0] * c / (2 * reach), 0.5 + a[0] * s / (2 * reach)]
                    : [u, 1 - lengths[step] / total]
                const uvB = cap ? [0.5 + b[0] * c / (2 * reach), 0.5 + b[0] * s / (2 * reach)]
                    : [u, 1 - lengths[step + 1] / total]
                ring.push(mesh.vertex([a[0] * c, a[0] * s, a[1]], normal, null, uvA, tangent))
                ring.push(mesh.vertex([b[0] * c, b[0] * s, b[1]], normal, null, uvB, tangent))
            }
            const middle = (t0 + t1) / 2
            const facing = [nr * Math.cos(middle), nr * Math.sin(middle), nz]
            // ring: [a0, b0, a1, b1]; degenerate points on the axis still
            // wind consistently, as a triangle with two equal corners is
            // simply dropped by the renderer.
            if (a[0] > 1e-9) mesh.triangle(ring[0], ring[2], ring[3], facing)
            if (b[0] > 1e-9) mesh.triangle(ring[0], ring[3], ring[1], facing)
        }
    }
    return mesh.result()
}

// A faceted solid turned about the z axis: `sides` flat faces round, each
// profile step a flat band, so a cube, a hexagonal plate, a pyramid or a
// crystal keeps sharp edges. The profile runs bottom to top like `lathe`'s;
// `start` turns the first corner (radians). Every face carries its own
// vertices with the face's normal, texture coordinates and a tangent.
function facetedLathe(profile, sides, start) {
    const count = Math.max(3, Math.round(finite(sides, 6)))
    const first = finite(start, 0)
    const points = profile.slice()
    const lengths = [0]
    for (let index = 1; index < points.length; ++index)
        lengths.push(lengths[index - 1] + Math.hypot(points[index][0] - points[index - 1][0],
                                                     points[index][1] - points[index - 1][1]))
    const total = Math.max(1e-6, lengths[lengths.length - 1])
    const reachRadius = Math.max(1e-6, ...points.map(function(point) { return point[0] }))
    const mesh = new MeshBuilder()
    for (let step = 0; step + 1 < points.length; ++step) {
        const a = points[step], b = points[step + 1]
        if (Math.hypot(b[0] - a[0], b[1] - a[1]) < 1e-9) continue
        const cap = a[0] <= 1e-9 || b[0] <= 1e-9
        for (let index = 0; index < count; ++index) {
            const t0 = first + index / count * Math.PI * 2
            const t1 = first + (index + 1) / count * Math.PI * 2
            const corner = function(point, turn) {
                return [point[0] * Math.cos(turn), point[0] * Math.sin(turn), point[1]]
            }
            const a0 = corner(a, t0), a1 = corner(a, t1), b0 = corner(b, t0), b1 = corner(b, t1)
            const middle = (t0 + t1) / 2
            const dr = b[0] - a[0], dz = b[1] - a[1]
            const length = Math.hypot(dr, dz)
            const outward = [dz / length * Math.cos(middle), dz / length * Math.sin(middle), -dr / length]
            // The face's own normal: across its horizontal edge and up its
            // profile edge, turned outward.
            const across = subtract(a[0] > 1e-9 ? a1 : b1, a[0] > 1e-9 ? a0 : b0)
            const up = subtract(a[0] > 1e-9 ? b0 : b1, a[0] > 1e-9 ? a0 : a1)
            let normal = normalized(crossProduct(across, up))
            if (dot(normal, outward) < 0) normal = normal.map(function(v) { return -v })
            const tangent = cap ? [1, 0, 0] : normalized(across)
            const uv = function(point, turn, along) {
                return cap ? [0.5 + point[0] * Math.cos(turn) / (2 * reachRadius),
                              0.5 + point[0] * Math.sin(turn) / (2 * reachRadius)]
                    : [index / count + (turn === t1 ? 1 / count : 0), 1 - along / total]
            }
            const va0 = mesh.vertex(a0, normal, null, uv(a, t0, lengths[step]), tangent)
            const va1 = mesh.vertex(a1, normal, null, uv(a, t1, lengths[step]), tangent)
            const vb0 = mesh.vertex(b0, normal, null, uv(b, t0, lengths[step + 1]), tangent)
            const vb1 = mesh.vertex(b1, normal, null, uv(b, t1, lengths[step + 1]), tangent)
            if (a[0] > 1e-9) mesh.triangle(va0, va1, vb1, normal)
            if (b[0] > 1e-9) mesh.triangle(va0, vb1, vb0, normal)
        }
    }
    return mesh.result()
}

// A star-shaped plate: `points` tips at `outer`, notches at `inner`, from
// z 0 to `height`, with flat top and bottom and a flat face per edge.
function starPrism(outer, inner, points, height) {
    const tips = Math.max(3, Math.round(finite(points, 5)))
    const outline = []
    for (let index = 0; index < tips * 2; ++index) {
        const turn = -Math.PI / 2 + index * Math.PI / tips
        const radius = index % 2 === 0 ? outer : inner
        outline.push([radius * Math.cos(turn), radius * Math.sin(turn)])
    }
    const mesh = new MeshBuilder()
    const flatUv = function(x, y) { return [0.5 + x / (2 * outer), 0.5 + y / (2 * outer)] }
    for (const level of [0, height]) {
        const facing = level > 0 ? [0, 0, 1] : [0, 0, -1]
        for (let index = 0; index < outline.length; ++index) {
            const p = outline[index], q = outline[(index + 1) % outline.length]
            const c = mesh.vertex([0, 0, level], facing, null, flatUv(0, 0), [1, 0, 0])
            const v0 = mesh.vertex([p[0], p[1], level], facing, null, flatUv(p[0], p[1]), [1, 0, 0])
            const v1 = mesh.vertex([q[0], q[1], level], facing, null, flatUv(q[0], q[1]), [1, 0, 0])
            mesh.triangle(c, v0, v1, facing)
        }
    }
    let travelled = 0
    let perimeter = 0
    for (let index = 0; index < outline.length; ++index) {
        const p = outline[index], q = outline[(index + 1) % outline.length]
        perimeter += Math.hypot(q[0] - p[0], q[1] - p[1])
    }
    for (let index = 0; index < outline.length; ++index) {
        const p = outline[index], q = outline[(index + 1) % outline.length]
        const edge = [q[0] - p[0], q[1] - p[1], 0]
        const edgeLength = Math.hypot(edge[0], edge[1])
        // The outline runs counter-clockwise, so the outside is to the right.
        const normal = normalized([edge[1], -edge[0], 0])
        const u0 = travelled / perimeter, u1 = (travelled + edgeLength) / perimeter
        travelled += edgeLength
        const a = mesh.vertex([p[0], p[1], 0], normal, null, [u0, 1], edge)
        const b = mesh.vertex([q[0], q[1], 0], normal, null, [u1, 1], edge)
        const c = mesh.vertex([q[0], q[1], height], normal, null, [u1, 0], edge)
        const d = mesh.vertex([p[0], p[1], height], normal, null, [u0, 0], edge)
        mesh.triangle(a, b, c, normal)
        mesh.triangle(a, c, d, normal)
    }
    return mesh.result()
}

// The stands under an icon (the Pedestals page), one table for the 3D scene
// and the flat painter alike. Each is a solid about its own axis, z up from
// its foot at 0, in units where its widest reach is `reach`:
// - `profile`: (radius, height) points, bottom to top, for a turned solid;
//   `loop` when the profile is a closed loop off the axis (a ring, a halo);
// - `sides`: a faceted solid with that many flat faces round (0: smooth);
// - `star`: a star plate instead of a profile;
// - `scale`: how many icon sizes one unit is at Size 1, so each shape's
//   width under an icon looks right.
// A column is the 3D page's own pedestal: radius 1, height 1, stretched to
// the height it is given.
var pedestalSpecs = (function() {
    const dome = [[0, 0], [1, 0]]
    for (let step = 1; step <= 8; ++step) {
        const turn = step / 8 * Math.PI / 2
        dome.push([Math.cos(turn), 0.62 * Math.sin(turn)])
    }
    dome[dome.length - 1] = [0, 0.62]
    const orb = [[0, 0]]
    for (let step = 1; step < 12; ++step) {
        const turn = step / 12 * Math.PI
        orb.push([Math.sin(turn), 1 - Math.cos(turn)])
    }
    orb.push([0, 2])
    const halo = []
    for (let step = 0; step < 12; ++step) {
        const turn = -Math.PI / 2 + step / 12 * Math.PI * 2
        halo.push([1 + 0.08 * Math.cos(turn), 0.08 + 0.08 * Math.sin(turn)])
    }
    return {
        column: { profile: [[0, 0], [1, 0], [1, 1], [0, 1]], scale: 0.18 },
        plate: { profile: [[0, 0], [1, 0], [1, 0.22], [0.92, 0.26], [0, 0.26]], scale: 0.46 },
        disc: { profile: [[0, 0], [1.25, 0], [1.25, 0.08], [1.08, 0.16], [0, 0.16]], scale: 0.42 },
        ring: { profile: [[0.72, 0], [1.3, 0], [1.3, 0.2], [0.72, 0.2]], loop: true, scale: 0.42 },
        saucer: { profile: [[0, 0], [0.55, 0], [1.4, 0.2], [0.6, 0.44], [0.3, 0.62], [0, 0.68]], scale: 0.4 },
        cube: { profile: [[0, 0], [1, 0], [1, 0.75], [0, 0.75]], sides: 4, scale: 0.44 },
        hexagon: { profile: [[0, 0], [1, 0], [1, 0.28], [0, 0.28]], sides: 6, scale: 0.48 },
        octagon: { profile: [[0, 0], [1, 0], [1, 0.24], [0.9, 0.3], [0, 0.3]], sides: 8, scale: 0.48 },
        star: { star: { outer: 1.2, inner: 0.58, points: 5 }, height: 0.22, scale: 0.42 },
        cone: { profile: [[0, 0], [1.1, 0], [0.42, 0.62], [0, 0.62]], scale: 0.42 },
        pyramid: { profile: [[0, 0], [1.2, 0], [0.45, 0.6], [0, 0.6]], sides: 4, scale: 0.42 },
        dome: { profile: dome, scale: 0.46 },
        orb: { profile: orb, scale: 0.26 },
        bowl: { profile: [[0, 0], [0.42, 0], [0.85, 0.1], [1.12, 0.28], [1.2, 0.38], [0, 0.38]], scale: 0.42 },
        pillar: { profile: [[0, 0], [1, 0], [1, 0.1], [0.72, 0.16], [0.55, 0.2], [0.5, 0.82], [0.62, 0.88],
                            [0.95, 0.94], [1, 1.02], [0, 1.02]], scale: 0.36 },
        tiered: { profile: [[0, 0], [1.2, 0], [1.2, 0.12], [0.92, 0.12], [0.92, 0.24], [0.64, 0.24],
                            [0.64, 0.36], [0, 0.36]], scale: 0.42 },
        goblet: { profile: [[0, 0], [0.72, 0], [0.72, 0.05], [0.24, 0.1], [0.14, 0.3], [0.14, 0.42],
                            [0.45, 0.52], [0.92, 0.72], [1, 0.82], [0, 0.82]], scale: 0.42 },
        hourglass: { profile: [[0, 0], [0.9, 0], [0.9, 0.08], [0.32, 0.42], [0.9, 0.76], [0.9, 0.84],
                               [0, 0.84]], scale: 0.48 },
        crystal: { profile: [[0, 0], [0.62, 0.3], [0.8, 0.5], [0.55, 0.7], [0, 0.74]], sides: 6, scale: 0.5 },
        halo: { profile: halo, loop: true, scale: 0.42 }
    }
})()
var pedestalShapes = Object.keys(pedestalSpecs)

function pedestalSpec(kind) {
    return pedestalSpecs[String(kind || "column")] || pedestalSpecs.column
}

// The stand under an icon, by shape. A faceted shape turns a flat face to
// the front (-y), the side the scene shows the viewer.
function pedestalShape(kind, segments) {
    const spec = pedestalSpec(kind)
    if (spec.star)
        return starPrism(spec.star.outer, spec.star.inner, spec.star.points, spec.height)
    if (spec.sides)
        return facetedLathe(spec.profile, spec.sides, -Math.PI / 2 - Math.PI / spec.sides)
    return lathe(spec.profile, segments, spec.loop === true)
}

// How high a pedestal shape reaches, in its own units.
function pedestalShapeHeight(kind) {
    const spec = pedestalSpec(kind)
    if (spec.star) return spec.height
    return Math.max(...spec.profile.map(function(point) { return point[1] }))
}

// How far a shape reaches from its axis, in its own units.
function pedestalShapeReach(kind) {
    const spec = pedestalSpec(kind)
    if (spec.star) return spec.star.outer
    return Math.max(...spec.profile.map(function(point) { return point[0] }))
}

// How many icon sizes one unit of a shape is, at Size 1.
function pedestalShapeScale(kind) {
    return pedestalSpec(kind).scale
}

// The stand seen a little from above (`tilt` degrees, its top toward the
// viewer), `thickness` stretching its height: how far its outline reaches
// above its foot and below it, in its own units, on the screen's vertical.
// The icon floats above `top`, so no part of the stand covers it.
function pedestalOutline(kind, thickness, tilt) {
    const spec = pedestalSpec(kind)
    const stretch = Math.max(0.01, finite(thickness, 1))
    const turn = finite(tilt, 20) * Math.PI / 180
    const points = spec.star
        ? [[spec.star.outer, 0], [spec.star.outer, spec.height]]
        : spec.profile
    let top = -Infinity, bottom = Infinity
    for (const point of points) {
        const z = point[1] * stretch * Math.cos(turn)
        const across = point[0] * Math.sin(turn)
        top = Math.max(top, z + across)
        bottom = Math.min(bottom, z - across)
    }
    return { top: top, bottom: bottom, reach: spec.star ? spec.star.outer
        : Math.max(...spec.profile.map(function(point) { return point[0] })) }
}

// A flat stand's measures in pixels for an icon of `size` (the flat
// painter's and a folder opening's): the stand seen `pedestalViewAngle`
// degrees from above (default `pedestalTilt2D`), or `pedestalSeenAngle`
// degrees when a folder of a 3D panel gives it, the icon floating its Lift
// above the stand's top, never over it. `onPath` (a free panel's path): the
// stand stands on the path at the icon's place and the icon floats above
// it; otherwise (an edge panel's row, a folder's items) icon and stand are
// centred together on the icon's cell. `raise` is how far the icon rises
// above its cell's middle, `standCentre` where the stand's middle is below
// it, `above`/`below` how far the pair reaches past the cell's top and
// bottom. `values` are the panel's (or a folder's) pedestal settings; an
// icon style's own pedestal keeps Auto.
var pedestalTilt2D = 20
function pedestalLayout(values, size, onPath) {
    const v = values || ({})
    const icon = Math.max(1, finite(size, 52))
    const shape = String(v.pedestalShape || "none").toLowerCase()
    const kind = shape === "auto" ? "column" : pedestalShapes.indexOf(shape) >= 0 ? shape : "none"
    const drawn = kind !== "none" && !(shape === "auto" && v.iconPedestalEnabled === true)
    const number = function(key, fallback, minimum, maximum) {
        return clamp(finite(v[key], fallback), minimum, maximum)
    }
    const scale = number("pedestalSize", 1, 0.2, 4)
    const thickness = number("pedestalThickness", 1, 0.2, 4)
    const lift = number("pedestalLift", 0.3, 0, 2)
    // Stand facing: Panel (the default) draws the stand in the panel's own
    // plane, seen straight on as a flat panel is, fixed to the panel at the
    // icon's place; Viewer and Auto draw it level, View angle degrees from
    // above. A folder opened from a true 3D panel passes the angle its
    // panel's own stands are seen at instead (`pedestalSeenAngle`, not a
    // setting), so its items' stands look like the ones on the panel.
    const facing = String(v.pedestalFacing || "panel").toLowerCase()
    const seen = typeof v.pedestalSeenAngle === "number" ? finite(v.pedestalSeenAngle, NaN) : NaN
    const tilt = Number.isFinite(seen) ? clamp(seen, 0, 90)
        : facing === "panel" ? 90 : number("pedestalViewAngle", pedestalTilt2D, 0, 60)
    const unit = icon * pedestalShapeScale(kind === "none" ? "column" : kind) * scale
    const depth = shape === "auto" ? icon * 0.3
        : kind === "column" ? icon * 0.4 * scale * thickness : unit * thickness
    const outline = pedestalOutline(kind === "none" ? "column" : kind, depth / Math.max(1e-6, unit), tilt)
    if (!drawn)
        return { shape: shape, kind: kind, drawn: false, unit: unit, depth: depth, outline: outline, tilt: tilt,
                 height: 0, width: 0, gap: 0, raise: 0, standCentre: 0, above: 0, below: 0 }
    const height = (outline.top - outline.bottom) * unit
    const gap = shape === "auto" ? icon * 0.02 : icon * (0.03 + lift * 0.3)
    // The icon's visible foot sits 0.07 icon sizes inside its cell's foot.
    const raise = onPath === true ? height / 2 + gap + 0.43 * icon
        : Math.max(0, (gap + height) / 2 - icon * 0.035)
    // The stand's middle, below the cell's middle.
    const standCentre = 0.43 * icon - raise + gap + height / 2
    const below = Math.max(0, standCentre + height / 2 - 0.5 * icon)
    return { shape: shape, kind: kind, drawn: true, unit: unit, depth: depth, outline: outline, tilt: tilt,
             height: height, width: 2 * outline.reach * unit, gap: gap, raise: raise,
             standCentre: standCentre, above: raise, below: below }
}

// A stand turned so its axis is `axis` (a unit vector in the viewer's
// frame: x right, y up the screen, z toward the viewer), `stretch` times
// as tall as its unit width: how far its outline reaches up and down the
// screen, where its top's middle is across it, and how far any of it
// reaches below its foot along a direction given by that frame's own z
// components (`rightZ`, `upZ`, `forwardZ`: how much each of the viewer's
// axes rises off the platform). All in units of its width per unit.
function pedestalExtents(kind, stretch, axis, rightZ, upZ, forwardZ) {
    const spec = pedestalSpec(kind)
    const height = pedestalShapeHeight(kind)
    const a = normalized([finite(axis[0], 0), finite(axis[1], 1), finite(axis[2], 0)])
    // Two directions across the axis.
    let u = crossProduct(a, [0, 0, 1])
    if (Math.hypot(u[0], u[1], u[2]) < 1e-6) u = [1, 0, 0]
    u = normalized(u)
    const w = normalized(crossProduct(a, u))
    const points = spec.star
        ? [[spec.star.outer, 0], [spec.star.outer, spec.height]]
        : spec.profile
    const stretchValue = Math.max(0.01, finite(stretch, 1))
    let top = -Infinity, bottom = Infinity, lowest = Infinity
    for (const point of points) {
        for (let step = 0; step < 16; ++step) {
            const turn = step / 16 * Math.PI * 2
            const across = [u[0] * Math.cos(turn) + w[0] * Math.sin(turn),
                            u[1] * Math.cos(turn) + w[1] * Math.sin(turn),
                            u[2] * Math.cos(turn) + w[2] * Math.sin(turn)]
            const p = [a[0] * point[1] * stretchValue + across[0] * point[0],
                       a[1] * point[1] * stretchValue + across[1] * point[0],
                       a[2] * point[1] * stretchValue + across[2] * point[0]]
            top = Math.max(top, p[1])
            bottom = Math.min(bottom, p[1])
            lowest = Math.min(lowest, p[0] * finite(rightZ, 0) + p[1] * finite(upZ, 0) + p[2] * finite(forwardZ, 1))
        }
    }
    return { top: top, bottom: bottom, topX: a[0] * height * stretchValue,
             topY: a[1] * height * stretchValue, lowest: lowest }
}

// The platform's shape for a layout, or null when the layout has no exact 3D
// platform. `aspect` is the layout's vertical squeeze, `sweep`/`start` an open
// path in the layout's screen degrees (y down), `sides` a polygon's count.
// Arcs, semicircles and fans are drawn about a centre below their panel's
// middle, which a centred 3D platform cannot follow, so they have none.
// PD-20/22, OF-42: one bounded solid behind an upright icon. Its front is
// z=0 and its real depth extends backwards; UVs share the IconScene texture's
// full-cell frame. Reuse MeshBuilder and the existing native mesh renderer.
function tile(spec) {
    const values = spec || ({})
    const depth = clamp(finite(values.thickness, 0), 0, 4)
    if (depth <= 0) return null
    const diameter = clamp(finite(values.diameter, 0.92), 0.1, 1)
    const bevel = clamp(finite(values.bevel, 0), 0, Math.min(depth / 2, diameter * 0.45))
    const shape = String(values.shape || "rounded")
    let outline = []
    if (shape === "diamond") outline = [[1,0], [0,1], [-1,0], [0,-1]]
    else if (shape === "hexagon") outline = [[1,0], [0.5,1], [-0.5,1], [-1,0], [-0.5,-1], [0.5,-1]]
    else {
        const radius = shape === "circle" ? 1 : shape === "square" ? 0 : shape === "squircle" ? 0.64 : 0.44
        const corners = [[1-radius,1-radius], [-1+radius,1-radius],
            [-1+radius,-1+radius], [1-radius,-1+radius]]
        for (let corner = 0; corner < 4; ++corner) {
            for (let step = 0; step <= 8; ++step) {
                const angle = (corner + step / 8) * Math.PI / 2
                const p = [corners[corner][0] + radius * Math.cos(angle),
                    corners[corner][1] + radius * Math.sin(angle)]
                const previous = outline[outline.length-1]
                if (!previous || Math.hypot(p[0]-previous[0], p[1]-previous[1]) > 1e-9) outline.push(p)
            }
        }
        if (outline.length > 1 && Math.hypot(outline[0][0]-outline[outline.length-1][0],
                outline[0][1]-outline[outline.length-1][1]) < 1e-9) outline.pop()
    }
    const rings = bevel > 0
        ? [[diameter-bevel,0], [diameter,-bevel], [diameter,-depth+bevel], [diameter-bevel,-depth]]
        : [[diameter,0], [diameter,-depth]]
    // When the two bevels meet there is no vertical wall; do not emit a
    // zero-area middle face or let it consume the scene's geometry budget.
    if (bevel > 0 && depth - 2 * bevel < 1e-9) rings.splice(2,1)
    const mesh = new MeshBuilder()
    const point = (ring, i) => [outline[i][0] * ring[0], outline[i][1] * ring[0], ring[1]]
    for (let level = 0; level < rings.length - 1; ++level) {
        for (let i = 0; i < outline.length; ++i) {
            const next = (i + 1) % outline.length
            const a = point(rings[level], i), b = point(rings[level], next)
            const c = point(rings[level+1], next), d = point(rings[level+1], i)
            let normal = normalized(crossProduct(subtract(b,a), subtract(d,a)))
            const edge = subtract(b,a), outward = [edge[1], -edge[0], 0]
            if (dot(normal,outward) < 0) normal = normal.map(v => -v)
            const v = [a,b,c,d].map(p => mesh.vertex(p, normal))
            mesh.triangle(v[0],v[1],v[2],normal)
            mesh.triangle(v[0],v[2],v[3],normal)
        }
    }
    let faceOffset = 0
    for (const [ring,normal] of [[rings[rings.length-1],[0,0,-1]], [rings[0],[0,0,1]]]) {
        if (normal[2] > 0) faceOffset = mesh.indexes.length
        const centre = mesh.vertex([0,0,ring[1]],normal)
        for (let i = 0; i < outline.length; ++i) {
            const a = mesh.vertex(point(ring,i),normal)
            const b = mesh.vertex(point(ring,(i+1)%outline.length),normal)
            if (normal[2] > 0) {
                // The front's outline shrinks by the bevel, while its source
                // remains the same full-cell tile, including its border.
                const uv = p => [0.5 + p[0] / 2 * diameter / ring[0],
                    0.5 + p[1] / 2 * diameter / ring[0]]
                mesh.uv0s[a] = uv(mesh.positions[a])
                mesh.uv0s[b] = uv(mesh.positions[b])
            }
            mesh.triangle(centre,a,b,normal)
        }
    }
    return mesh.result({faceOffset: faceOffset, faceCount: mesh.indexes.length - faceOffset})
}

function shapeForLayout(layout, polygonSides) {
    const name = String(layout || "")
    if (name === "circular" || name === "ring")
        return { kind: "ring", aspect: 1 }
    if (name === "ellipse")
        return { kind: "ring", aspect: 0.62 }
    if (name === "radial")
        return { kind: "arc", sweep: 300, start: -150 }
    const sides = { triangle: 3, square: 4, pentagon: 5, hexagon: 6, octagon: 8 }[name]
    if (sides)
        return { kind: "polygon", sides: sides }
    if (name === "polygon")
        return { kind: "polygon", sides: clamp(Math.round(finite(polygonSides, 6)), 3, 12) }
    if (name === "star")
        return { kind: "star", points: clamp(Math.round(finite(polygonSides, 6)), 3, 12) }
    if (name === "mobius")
        return { kind: "mobius" }
    return null
}

// A Möbius strip's band follows the layout's ∞ (LayoutEngine.mobiusPoint)
// with one half twist: its cross-section turns half a turn about the band's
// way along one lap, so its two loops lean opposite ways and a lap ends
// where it began, upside down. Its two strands pass the crossing this far
// apart in height, the second over the first, so the band never runs
// through itself.
var mobiusLift = 0.2
// How far a Möbius band stands above its plane at `progress` (0 to 1) of
// the way round from the crossing; 0 for every other shape.
function standLift(kind, progress) {
    return kind === "mobius" ? -mobiusLift * Math.cos(2 * Math.PI * finite(progress, 0)) : 0
}

// A star's corners on the unit circle, tips and notches in turn, the first
// tip straight up, in the order the layout walks them. Each corner carries
// the outward normals of the edges before and after it, and its mitre `m`:
// a band edge `d` out from the centre line meets its neighbour at c + m * d.
// At a sharp tip the mitre is long, so the band ends in a point there, as the
// star's own outline does.
function starCorners(points) {
    const count = 2 * clamp(Math.round(finite(points, 6)), 3, 12)
    const corners = []
    for (let k = 0; k < count; ++k) {
        const screen = -Math.PI / 2 + k * Math.PI * 2 / count
        const radius = k % 2 === 0 ? 1 : starInnerRadius
        corners.push({ c: [Math.cos(screen) * radius, -Math.sin(screen) * radius] })
    }
    // An edge's outward normal: across the edge, away from the star's middle,
    // which every edge of a star faces.
    const edgeNormal = function(a, b) {
        const n = normalized([b[1] - a[1], a[0] - b[0], 0])
        return n[0] * (a[0] + b[0]) + n[1] * (a[1] + b[1]) >= 0 ? n : [-n[0], -n[1], 0]
    }
    for (let k = 0; k < count; ++k) {
        const before = corners[(k + count - 1) % count].c, here = corners[k].c
        const after = corners[(k + 1) % count].c
        const n0 = edgeNormal(before, here), n1 = edgeNormal(here, after)
        const join = 1 + n0[0] * n1[0] + n0[1] * n1[1]
        corners[k].m = [(n0[0] + n1[0]) / join, (n0[1] + n1[1]) / join]
        corners[k].after = n1
    }
    return corners
}

// The radius of a shape's centre line. A polygon's band edges are mitred, so
// its corners reach further out than its edges; a polygon with few sides
// draws its centre line closer in to keep those corners within reach.
function shapeTrackRadius(shape, half) {
    // The ∞ reaches further than its radius; its loops' far ends stay
    // within reach.
    if (shape && shape.kind === "mobius")
        return Math.min(trackRadius, (reach - half - outerBevel) / LayoutEngine.mobiusReach)
    if (shape && shape.kind === "star") {
        // The tips' mitred points reach furthest out.
        const tip = starCorners(shape.points)[0]
        return Math.min(trackRadius, reach - (half + outerBevel) * Math.hypot(tip.m[0], tip.m[1]))
    }
    if (!shape || shape.kind !== "polygon")
        return trackRadius
    const sides = clamp(Math.round(finite(shape.sides, 6)), 3, 12)
    const mitre = 1 / Math.cos(Math.PI / sides)
    return Math.min(trackRadius, reach - (half + outerBevel) * mitre)
}

// Points along the platform's centre line. Each carries the point `c`, the
// direction `m` its band edges move along (an edge at offset d is at
// c + m * d) and the horizontal outward normal `n` of the faces there.
// A run is drawn as one smooth strip; corners start a new run.
function centreLine(shape, segments, radius, overhang) {
    const runs = []
    const r = finite(radius, trackRadius)
    if (shape.kind === "ring") {
        const count = Math.max(24, Math.round(finite(segments, 96)))
        const a = r, b = r * clamp(finite(shape.aspect, 1), 0.2, 1)
        const run = []
        for (let index = 0; index <= count; ++index) {
            const t = index / count * Math.PI * 2
            // An ellipse's outward normal is its gradient, not its radius.
            const n = normalized([Math.cos(t) / a, Math.sin(t) / b, 0])
            run.push({ c: [a * Math.cos(t), b * Math.sin(t)], m: [n[0], n[1]], n: n })
        }
        runs.push({ closed: true, samples: run })
    } else if (shape.kind === "polygon") {
        const sides = clamp(Math.round(finite(shape.sides, 6)), 3, 12)
        // Vertices where the layout puts them: the first straight up.
        const vertex = function(k) {
            const screen = -Math.PI / 2 + k * Math.PI * 2 / sides
            return [Math.cos(screen), -Math.sin(screen)]
        }
        const mitre = 1 / Math.cos(Math.PI / sides)
        for (let k = 0; k < sides; ++k) {
            const v0 = vertex(k), v1 = vertex(k + 1)
            const n = normalized([v0[0] + v1[0], v0[1] + v1[1], 0])
            runs.push({ closed: false, edge: true, samples: [
                { c: [v0[0] * r, v0[1] * r], m: [v0[0] * mitre, v0[1] * mitre], n: n },
                { c: [v1[0] * r, v1[1] * r], m: [v1[0] * mitre, v1[1] * mitre], n: n }] })
        }
    } else if (shape.kind === "star") {
        // One straight run per edge, as for a polygon; neighbouring runs share
        // the corner between them, so the band stays one closed surface.
        const corners = starCorners(shape.points)
        for (let k = 0; k < corners.length; ++k) {
            const from = corners[k], to = corners[(k + 1) % corners.length]
            runs.push({ closed: false, edge: true, samples: [
                { c: [from.c[0] * r, from.c[1] * r], m: from.m, n: from.after },
                { c: [to.c[0] * r, to.c[1] * r], m: to.m, n: from.after }] })
        }
    } else if (shape.kind === "mobius") {
        // One run once round, its cross-section turning with the twist: each
        // sample carries its frame, the way across the band (`across`) and
        // the way up from its face (`up`), and its height (`z`). Where the
        // run ends it meets where it began, turned over.
        const count = Math.max(96, Math.round(finite(segments, 96) * 2))
        const run = []
        for (let index = 0; index <= count; ++index) {
            const share = index / count
            const at = LayoutEngine.mobiusPoint(share, r * LayoutEngine.mobiusReach)
            const turn = at.tangent * Math.PI / 180
            // The layout's y runs down the screen; this frame's runs up.
            const way = [Math.cos(turn), -Math.sin(turn)]
            const n = [-way[1], way[0], 0]
            const twist = Math.PI * share - Math.PI / 2
            const across = [n[0] * Math.cos(twist), n[1] * Math.cos(twist), Math.sin(twist)]
            const up = [-n[0] * Math.sin(twist), -n[1] * Math.sin(twist), Math.cos(twist)]
            run.push({ c: [at.x, -at.y], z: standLift("mobius", share), across: across, up: up,
                       n: n, m: [n[0], n[1]] })
        }
        runs.push({ closed: false, twisted: true, samples: run })
    } else if (shape.kind === "arc") {
        // An open path's first and last icons stand on its ends: the platform
        // runs on past them by `overhang`, so their pedestals stand on it.
        const extra = Math.max(0, finite(overhang, 0)) / r * 180 / Math.PI
        const sweep = clamp(finite(shape.sweep, 300) + 2 * extra, 10, 359)
        const start = finite(shape.start, -150) - extra
        const count = Math.max(8, Math.round(finite(segments, 96) * sweep / 360))
        const run = []
        for (let index = 0; index <= count; ++index) {
            const screen = (start + sweep * index / count) * Math.PI / 180
            const n = [Math.cos(screen), -Math.sin(screen), 0]
            run.push({ c: [n[0] * r, n[1] * r], m: [n[0], n[1]], n: n })
        }
        runs.push({ closed: false, samples: run })
    }
    return runs
}

// The band's cross-section, from the inner wall's foot over the top to the
// outer wall's foot, as (offset, height) points. `half` is half the flat
// top's width; a bend tilts the whole section about the centre line.
function profile(half, bend) {
    const inner = innerBevel, outer = outerBevel, drop = 0.045
    const slope = clamp(finite(bend, 0), -1, 1) * 0.9
    const at = function(d, z) { return [d, z + slope * d] }
    return [
        at(-half - inner, bottomHeight),
        at(-half - inner, topHeight - drop),
        at(-half, topHeight),
        at(half, topHeight),
        at(half + outer, topHeight - drop),
        at(half + outer, bottomHeight)
    ]
}

// A Möbius band's cross-section: the same turned over (half a turn about
// its middle), so where a lap ends upside down it meets where it began.
function twistedProfile(half) {
    const bevel = (innerBevel + outerBevel) / 2, depth = (topHeight - bottomHeight) / 2, drop = 0.045
    return [[-half - bevel, -depth + drop], [-half - bevel, depth - drop], [-half, depth], [half, depth],
            [half + bevel, depth - drop], [half + bevel, -depth + drop], [half, -depth], [-half, -depth]]
}
// Its faces: one colour on both faces of the band, which are one face.
var twistedFaces = [
    { from: 0, to: 1, part: "wall" },
    { from: 1, to: 2, part: "rim" },
    { from: 2, to: 3, part: "top" },
    { from: 3, to: 4, part: "rim" },
    { from: 4, to: 5, part: "wall" },
    { from: 5, to: 6, part: "rim" },
    { from: 6, to: 7, part: "top" },
    { from: 7, to: 0, part: "rim" }
]

// The faces between consecutive profile points, and which surface each is.
var profileFaces = [
    { from: 0, to: 1, part: "wall" },
    { from: 1, to: 2, part: "rim" },
    { from: 2, to: 3, part: "top" },
    { from: 3, to: 4, part: "rim" },
    { from: 4, to: 5, part: "wall" },
    { from: 5, to: 0, part: "under" }
]

function colour(palette, part) {
    const value = (palette || ({}))[part]
    return value && value.length >= 3 ? value : [1, 1, 1, 1]
}

// A generated platform. `spec`: shape (from shapeForLayout), band (half the
// flat top's width, 0.06 to 0.16 of the outer radius), bend (-1 to 1, the
// top tilting outward up or down about the icons' line), segments, and a
// palette of [r, g, b, a] colours for top, rim, wall and under. The rim faces
// come last, as their own index range (`rimOffset`, `rimCount`), so they can
// carry a glowing material of their own.
function platform(spec) {
    const options = spec || ({})
    const shape = options.shape
    if (!shape || ["ring", "polygon", "star", "arc", "mobius"].indexOf(shape.kind) < 0)
        return null
    const half = clamp(finite(options.band, 0.11), minimumBand, maximumBand)
    const twisted = shape.kind === "mobius"
    const section = twisted ? twistedProfile(half) : profile(half, options.bend)
    const faces = twisted ? twistedFaces : profileFaces
    const track = shapeTrackRadius(shape, half)
    const runs = centreLine(shape, options.segments, track, half + outerBevel)
    const body = new MeshBuilder()
    const rimFaces = []
    // Texture coordinates: the texture wraps round the band the way a label
    // wraps round a tin. Across the band, v runs along the cross-section
    // from the inner wall's foot (0) over the top to the outer wall's foot
    // (1), so every face carries the texture, with its middle on the top.
    // Along the band, u runs 0..1 over each run; the mesh's `textureRepeats`
    // says how many whole repeats of the texture that is (as many as keep
    // its scale the same both ways, so a ring closes without a seam), for
    // the texture's own horizontal scale.
    const sectionLengths = [0]
    for (let index = 1; index < section.length; ++index)
        sectionLengths.push(sectionLengths[index - 1]
            + Math.hypot(section[index][0] - section[index - 1][0], section[index][1] - section[index - 1][1]))
    const sectionLength = Math.max(1e-6, sectionLengths[sectionLengths.length - 1]
        + (twisted ? Math.hypot(section[0][0] - section[section.length - 1][0],
                                section[0][1] - section[section.length - 1][1]) : 0))
    // A twisted band's v repeats on each half of its section, so the
    // texture meets itself where the band turns over.
    const across = function(pointIndex) {
        const share = pointIndex >= section.length ? 1 : sectionLengths[pointIndex] / sectionLength
        return twisted ? 1 - Math.abs(2 * ((2 * share) % 1) - 1) : share
    }
    const runLengths = function(run) {
        const lengths = [0]
        for (let index = 1; index < run.samples.length; ++index) {
            const a = run.samples[index - 1].c, b = run.samples[index].c
            lengths.push(lengths[index - 1] + Math.hypot(b[0] - a[0], b[1] - a[1]))
        }
        return lengths
    }
    const runRepeats = function(run) {
        const lengths = runLengths(run)
        return Math.max(1, Math.round(Math.max(1e-6, lengths[lengths.length - 1]) / sectionLength))
    }
    let textureRepeats = 1
    for (const run of runs)
        textureRepeats = Math.max(textureRepeats, runRepeats(run))
    const addFace = function(mesh, run, face) {
        const from = section[face.from], to = section[face.to]
        // The section runs clockwise (up the inner wall, over the top, down
        // the outer wall, back underneath), so a face's outward normal in
        // the section plane is its direction turned by +90 degrees.
        const along = [to[0] - from[0], to[1] - from[1]]
        const sectionNormal = normalized([-along[1], along[0], 0])
        const color = colour(options.palette, face.part)
        // The underside runs back from the outer foot to the inner one.
        const vFrom = face.part === "under" ? 1 : across(face.from)
        const vTo = face.part === "under" ? 0 : twisted && face.to === 0 ? across(section.length) : across(face.to)
        const lengths = runLengths(run)
        const total = Math.max(1e-6, lengths[lengths.length - 1])
        // A shorter run (a polygon's side) uses that share of the repeats.
        const share = runRepeats(run) / textureRepeats
        const strip = []
        const count = run.samples.length
        for (let index = 0; index < count; ++index) {
            const sample = run.samples[index]
            // A twisted band's section stands in its own frame there.
            const normal = sample.across
                ? normalized([sample.across[0] * sectionNormal[0] + sample.up[0] * sectionNormal[1],
                              sample.across[1] * sectionNormal[0] + sample.up[1] * sectionNormal[1],
                              sample.across[2] * sectionNormal[0] + sample.up[2] * sectionNormal[1]])
                : [sample.n[0] * sectionNormal[0], sample.n[1] * sectionNormal[0], sectionNormal[1]]
            const point = sample.across
                ? function(p) {
                    return [sample.c[0] + sample.across[0] * p[0] + sample.up[0] * p[1],
                            sample.c[1] + sample.across[1] * p[0] + sample.up[1] * p[1],
                            sample.z + sample.across[2] * p[0] + sample.up[2] * p[1]]
                }
                : function(p) {
                    return [sample.c[0] + sample.m[0] * p[0], sample.c[1] + sample.m[1] * p[0], p[1]]
                }
            const u = lengths[index] / total * share
            // The path's direction here, where the texture's u runs.
            const before = run.samples[index > 0 ? index - 1 : (run.closed ? count - 2 : 0)]
            const after = run.samples[index + 1 < count ? index + 1 : (run.closed ? 1 : count - 1)]
            const tangent = [after.c[0] - before.c[0], after.c[1] - before.c[1], (after.z || 0) - (before.z || 0)]
            strip.push([mesh.vertex(point(from), normal, color, [u, vFrom], tangent),
                        mesh.vertex(point(to), normal, color, [u, vTo], tangent), normal])
        }
        for (let index = 0; index + 1 < strip.length; ++index) {
            const a = strip[index], b = strip[index + 1]
            const facing = normalized([a[2][0] + b[2][0], a[2][1] + b[2][1], a[2][2] + b[2][2]])
            mesh.triangle(a[0], b[0], b[1], facing)
            mesh.triangle(a[0], b[1], a[1], facing)
        }
    }
    for (const run of runs)
        for (const face of faces)
            if (face.part === "rim") rimFaces.push({ run: run, face: face })
            else addFace(body, run, face)
    // An open arc is closed at both ends by its cross-section; a twisted
    // band's ends meet each other.
    for (const run of runs) {
        if (run.closed || run.edge || run.twisted) continue
        const ends = [{ sample: run.samples[0], next: run.samples[1], sign: -1 },
                      { sample: run.samples[run.samples.length - 1],
                        next: run.samples[run.samples.length - 2], sign: -1 }]
        for (const end of ends) {
            const tangent = normalized([end.next.c[0] - end.sample.c[0], end.next.c[1] - end.sample.c[1], 0])
            const facing = [tangent[0] * end.sign, tangent[1] * end.sign, 0]
            const color = colour(options.palette, "wall")
            const points = section.map(function(p) {
                return body.vertex([end.sample.c[0] + end.sample.m[0] * p[0],
                                    end.sample.c[1] + end.sample.m[1] * p[0], p[1]], facing, color,
                                   null, [end.sample.m[0], end.sample.m[1], 0])
            })
            for (let index = 1; index + 1 < points.length; ++index)
                body.triangle(points[0], points[index], points[index + 1], facing)
        }
    }
    const rimOffset = body.indexes.length
    for (const item of rimFaces)
        addFace(body, item.run, item.face)
    let reachX = 0, reachY = 0
    for (const point of body.positions) {
        reachX = Math.max(reachX, Math.abs(point[0]))
        reachY = Math.max(reachY, Math.abs(point[1]))
    }
    return body.result({
        reachX: reachX,
        reachY: reachY,
        rimOffset: rimOffset,
        rimCount: body.indexes.length - rimOffset,
        shape: shape.kind,
        band: half,
        track: track,
        textureRepeats: textureRepeats
    })
}

// Where an icon stands on a generated platform: the layout's own position,
// relative to the track's centre and radius, carried onto the centre line
// (`track`, the platform's own). Every generated shape is its layout's path
// scaled to that radius, so the scaled position lies exactly on it.
function standPoint(offsetX, offsetY, layoutRadius, track) {
    const radius = Math.max(1e-6, finite(layoutRadius, 1))
    const centre = finite(track, trackRadius)
    return [offsetX / radius * centre, offsetY / radius * centre]
}

// PD-18 / OF-31: fold the rear half about its top's horizontal diameter.
// Positive is up, negative down, bounded to a right angle. Mesh vertices and
// icon feet use this same transform, so the icons stay on the platform.
function foldPoint(point, fold, hingeHeight) {
    const turn = clamp(finite(fold, 0), -1, 1) * Math.PI / 2
    if (point[1] <= 0 || Math.abs(turn) < 1e-9) return point.slice()
    const hinge = finite(hingeHeight, topHeight), z = point[2] - hinge
    return [point[0], point[1] * Math.cos(turn) - z * Math.sin(turn),
            hinge + point[1] * Math.sin(turn) + z * Math.cos(turn)]
}

function foldVector(vector, atPoint, fold) {
    const turn = clamp(finite(fold, 0), -1, 1) * Math.PI / 2
    if (atPoint[1] <= 0 || Math.abs(turn) < 1e-9) return vector.slice()
    return [vector[0], vector[1] * Math.cos(turn) - vector[2] * Math.sin(turn),
            vector[1] * Math.sin(turn) + vector[2] * Math.cos(turn)]
}

function foldMesh(mesh, fold, hingeHeight) {
    if (!mesh || !mesh.positions || !mesh.indexes) return null
    if (Math.abs(clamp(finite(fold, 0), -1, 1)) < 1e-9) return mesh
    if (mesh.indexes.length > 262144) return null
    const result = Object.assign({}, mesh, {positions: [], normals: [], uv0s: [], indexes: []})
    if (mesh.colors) result.colors = []
    if (mesh.tangents) { result.tangents = []; result.binormals = [] }
    // Each face keeps its winding and gets its actual deformed normal. This
    // also gives the crease a hard edge instead of an interpolated false one.
    for (let i = 0; i < mesh.indexes.length; i += 3) {
        const ids = mesh.indexes.slice(i, i + 3)
        const points = ids.map(index => foldPoint(mesh.positions[index], fold, hingeHeight))
        const normal = normalized(crossProduct(subtract(points[1], points[0]), subtract(points[2], points[0])))
        for (let j = 0; j < 3; ++j) {
            result.positions.push(points[j])
            result.normals.push(normal.slice())
            result.uv0s.push((mesh.uv0s[ids[j]] || [0, 0]).slice())
            if (mesh.colors) result.colors.push(mesh.colors[ids[j]].slice())
            if (mesh.tangents) {
                const tangent = normalized(foldVector(mesh.tangents[ids[j]], mesh.positions[ids[j]], fold))
                result.tangents.push(tangent)
                result.binormals.push(normalized(crossProduct(normal, tangent)))
            }
            result.indexes.push(result.indexes.length)
        }
    }
    return result
}

// A CSS colour as [r, g, b, a] in 0..1: "#rgb", "#rrggbb", "#aarrggbb"
// (Qt's order), "rgb(...)" and "rgba(...)", the forms the look tables use.
// Anything else is null.
function parseColour(text) {
    const value = String(text || "").trim().toLowerCase()
    let match = /^#([0-9a-f]{3})$/.exec(value)
    if (match)
        return [0, 1, 2].map(function(i) { return parseInt(match[1][i] + match[1][i], 16) / 255 }).concat([1])
    match = /^#([0-9a-f]{6})$/.exec(value)
    if (match)
        return [0, 2, 4].map(function(i) { return parseInt(match[1].substr(i, 2), 16) / 255 }).concat([1])
    match = /^#([0-9a-f]{2})([0-9a-f]{6})$/.exec(value)
    if (match)
        return [0, 2, 4].map(function(i) { return parseInt(match[2].substr(i, 2), 16) / 255 })
            .concat([parseInt(match[1], 16) / 255])
    match = /^rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)$/.exec(value)
    if (match)
        return [match[1], match[2], match[3]].map(function(v) { return clamp(Number(v) / 255, 0, 1) })
            .concat([match[4] === undefined ? 1 : clamp(Number(match[4]), 0, 1)])
    return null
}

function scaled(colour, factor) {
    return [clamp(colour[0] * factor, 0, 1), clamp(colour[1] * factor, 0, 1),
            clamp(colour[2] * factor, 0, 1), 1]
}

function mixed(colour, other, share) {
    return [0, 1, 2].map(function(i) { return colour[i] + (other[i] - colour[i]) * share }).concat([1])
}

// The colours of a procedurally drawn look, from the same style its 2D
// surface is drawn with (LayoutEngine.themeStyle): the band takes the
// stroke's colour, its rim a lighter edge of it, and the rim glows in the
// shadow's colour when the 2D look glows (a coloured, blurred shadow).
// Returns { top, rim, wall, under, glow, glowStrength }.
function paletteFromStyle(style) {
    const stroke = parseColour(style && style.stroke) || [0.25, 0.36, 0.46, 1]
    const shadow = parseColour(style && style.shadow)
    const luminous = shadow !== null && shadow[3] > 0.05
        && Math.max(shadow[0], shadow[1], shadow[2]) > 0.2
    const top = [stroke[0], stroke[1], stroke[2], 1]
    return {
        top: top,
        rim: mixed(top, luminous ? shadow : [1, 1, 1], 0.45),
        wall: scaled(top, 0.62),
        under: scaled(top, 0.35),
        glow: luminous ? [shadow[0], shadow[1], shadow[2], 1] : null,
        glowStrength: luminous ? clamp(finite(style.blur, 0) / 25, 0, 1.2) : 0
    }
}
