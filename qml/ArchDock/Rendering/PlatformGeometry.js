.pragma library

// Generated 3D geometry, as plain data in the theme mesh format
// ("org.archdock.mesh": positions, normals, uv0s, indexes) so the same
// IconStyle3D draws it as it draws a theme's own mesh.
//
// Two shapes are generated here:
// - `pedestal()`: the column an icon stands on.
// - `platform(spec)`: the platform a look is drawn on when its theme ships no
//   3D mesh of its own. It follows the panel's layout path exactly, so every
//   icon stands on it: a circle, an ellipse, a regular polygon or the open
//   300-degree radial arc.
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
// Half the flat top's width: never narrower than a pedestal is wide.
var minimumBand = 0.06
var maximumBand = 0.16
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
}

MeshBuilder.prototype.vertex = function(point, normal, color) {
    this.positions.push([point[0], point[1], point[2]])
    this.normals.push(normalized(normal))
    // Planar, seen from above: a texture drawn as the platform's top view
    // lands where it is drawn.
    this.uv0s.push([0.5 + point[0] / 2, 0.5 + point[1] / 2])
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
        const v0 = mesh.vertex([na[0], na[1], 0], na)
        const v1 = mesh.vertex([nb[0], nb[1], 0], nb)
        const v2 = mesh.vertex([nb[0], nb[1], 1], nb)
        const v3 = mesh.vertex([na[0], na[1], 1], na)
        mesh.triangle(v0, v1, v2, middle)
        mesh.triangle(v0, v2, v3, middle)
        const centre = mesh.vertex([0, 0, 1], [0, 0, 1])
        const t0 = mesh.vertex([na[0], na[1], 1], [0, 0, 1])
        const t1 = mesh.vertex([nb[0], nb[1], 1], [0, 0, 1])
        mesh.triangle(centre, t0, t1, [0, 0, 1])
    }
    return mesh.result()
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
    return null
}

// The radius of a shape's centre line. A polygon's band edges are mitred, so
// its corners reach further out than its edges; a polygon with few sides
// draws its centre line closer in to keep those corners within reach.
function shapeTrackRadius(shape, half) {
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
    if (!shape || ["ring", "polygon", "arc"].indexOf(shape.kind) < 0)
        return null
    const half = clamp(finite(options.band, 0.11), minimumBand, maximumBand)
    const section = profile(half, options.bend)
    const track = shapeTrackRadius(shape, half)
    const runs = centreLine(shape, options.segments, track, half + outerBevel)
    const body = new MeshBuilder()
    const rimFaces = []
    const addFace = function(mesh, run, face) {
        const from = section[face.from], to = section[face.to]
        // The section runs clockwise (up the inner wall, over the top, down
        // the outer wall, back underneath), so a face's outward normal in
        // the section plane is its direction turned by +90 degrees.
        const along = [to[0] - from[0], to[1] - from[1]]
        const sectionNormal = normalized([-along[1], along[0], 0])
        const color = colour(options.palette, face.part)
        const strip = []
        for (let index = 0; index < run.samples.length; ++index) {
            const sample = run.samples[index]
            const normal = [sample.n[0] * sectionNormal[0], sample.n[1] * sectionNormal[0], sectionNormal[1]]
            const point = function(p) {
                return [sample.c[0] + sample.m[0] * p[0], sample.c[1] + sample.m[1] * p[0], p[1]]
            }
            strip.push([mesh.vertex(point(from), normal, color), mesh.vertex(point(to), normal, color), normal])
        }
        for (let index = 0; index + 1 < strip.length; ++index) {
            const a = strip[index], b = strip[index + 1]
            const facing = normalized([a[2][0] + b[2][0], a[2][1] + b[2][1], a[2][2] + b[2][2]])
            mesh.triangle(a[0], b[0], b[1], facing)
            mesh.triangle(a[0], b[1], a[1], facing)
        }
    }
    for (const run of runs)
        for (const face of profileFaces)
            if (face.part === "rim") rimFaces.push({ run: run, face: face })
            else addFace(body, run, face)
    // An open arc is closed at both ends by its cross-section.
    for (const run of runs) {
        if (run.closed || run.edge) continue
        const ends = [{ sample: run.samples[0], next: run.samples[1], sign: -1 },
                      { sample: run.samples[run.samples.length - 1],
                        next: run.samples[run.samples.length - 2], sign: -1 }]
        for (const end of ends) {
            const tangent = normalized([end.next.c[0] - end.sample.c[0], end.next.c[1] - end.sample.c[1], 0])
            const facing = [tangent[0] * end.sign, tangent[1] * end.sign, 0]
            const color = colour(options.palette, "wall")
            const points = section.map(function(p) {
                return body.vertex([end.sample.c[0] + end.sample.m[0] * p[0],
                                    end.sample.c[1] + end.sample.m[1] * p[0], p[1]], facing, color)
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
        track: track
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

function foldMesh(mesh, fold, hingeHeight) {
    if (!mesh || !mesh.positions || !mesh.indexes) return null
    if (Math.abs(clamp(finite(fold, 0), -1, 1)) < 1e-9) return mesh
    if (mesh.indexes.length > 262144) return null
    const result = Object.assign({}, mesh, {positions: [], normals: [], uv0s: [], indexes: []})
    if (mesh.colors) result.colors = []
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
