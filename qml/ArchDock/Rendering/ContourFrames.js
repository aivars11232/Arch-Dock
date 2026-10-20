.pragma library

// Panel decorations with contours: a decoration remakes a panel's inner and
// outer edges, how an open band ends, where the band is cut into segments,
// the parts standing on it and the lines drawn on it, on any panel shape.
//
// A panel's band is a ribbon along its own path. Every point of it has a
// place along the path, `s` (0 to the path's length), and a place across
// it, `t` (-1 at the inner edge, 0 in the middle, +1 at the outer edge, in
// half-widths of the plain band). A frame is computed in (s, t) and then
// laid along the path, so every decoration fits every shape: a straight
// edge panel, a ring, a polygon, an arc, a star.
//
// The frame vocabulary follows Arwes' frames (github.com/arwes/arwes,
// packages/frames, MIT licence, copyright (c) 2014 - 2024 Romel Perez): a
// corner cut by a square's size (Octagon), a side stepping out for a long
// run after a short one (Kranox), brackets of a short and a long run round
// a cut corner (Nefrex). The console bars follow the LCARS elbow and
// half-pill proportions of the R package lcars (MIT). The arches are the
// equilateral pointed arch of gothic tracery. Lightning is midpoint
// displacement (Fournier, Fussell and Carpenter, 1982); the runes are the
// Elder Futhark's letters, drawn as their straight carved strokes.
//
// A decoration (a package's `contour` section), every length in
// half-widths of the band so the proportions hold at any size:
//   outer, inner: { base: 1, taper: [start, end], rough: { amp, scale },
//                   features: [feature] }
//               the two edges; `taper` scales an open band's edge from its
//               start to its end (a wand thick at the handle, thin at its
//               tip); `rough` wobbles the edge (bark, chipped stone, cloud)
//   feature: { kind: "step" | "tooth" | "arch" | "wave" | "round", length,
//              depth, chamfer, period, phase, skew, curve, vary,
//              anchor: "period" | "icons" | "between" | "middle" | "ends" |
//              "at", at: [fractions] }
//   ends: { kind: "square" | "chamfer" | "pill" | "point" | "block" |
//           "broken", size, depth, outer: true, inner: true, start, end,
//           cut } how an open band ends; `start` and `end` may each say
//           otherwise for one end, `cut` for the ends where the band is cut
//           into segments
//   segments: { kind: "none" | "icons", gap, colours } the band cut between
//             icons
//   sway: { amp, period, phase } the band's middle swinging from side to
//         side along it (a vine's stem, a serpent)
//   body: { feather, round, fill: "material" | "none", glow, opacity,
//           bevel, coat, gleam } a
//         soft fading outline, a round (log, branch) shading or a chrome
//         one (round: "chrome", a polished sculpted shell), no fill (the
//         lines and parts are the panel), a glow round it; `bevel` the
//         width of the lit and shaded rim along its outline (half-widths)
//   layers: [{ side: "outer" | "inner" | "middle", inset, width, offset,
//              fill, material, glow, opacity, dash, avoid, margin, ink,
//              recess, window, gloss }] strips
//           along the band with their own colour or material (a rail, a
//           stripe, a blade's core)
//   parts: [{ kind: "ellipse" | "dome" | "box" | "trapezoid" | "capsule" |
//             "polygon" | "strut" | "branch" | "crystal" | "star" | "ring" |
//             "arc" | "lens" | "flame" | "gem",
//             at: "middle" | "start" | "end" | "both" | "icons" |
//             "between" | "at" | "period", where, period, every, alternate,
//             s, t, on: "outer" | "inner", align, w, h, top, r, chamfer,
//             angle, points, from, to, width, length, tip, bend, forks,
//             fill, material, glow, stroke, gradient, opacity, z, shade, dim,
//             depth, bevel, colour, ink, sheen }] pieces standing on or round the band (a
//           nacelle, a dome, a twig, a hilt); a "lens" is a glowing glass
//           lens in its bezel, drawn by the renderer from its colour (one
//           resting `dim`, 0 to 1, is darker glass that lights up under the
//           pointer); a
//           "gem" a cut stone, its facets drawn from its colour (or its
//           material's): `cut` "brilliant" (round), "princess" (square),
//           "baguette" (a long step cut) or "cabochon" (a polished dome),
//           and `halo` a ring of that many small diamonds round it
//   lines: [{ kind: "loop" | "middle" | "edge" | "brackets" | "spiral" |
//             "wave" | "dots" | "stars" | "slats" | "ends" | "windows" |
//             "chips" | "streaks" | "plait" | "tendrils" | "bristles" |
//             "runes" | "lightning" | "cracks" | "hexgrid" | "panels" |
//             "twigs" | "footprints" | "fringe" | "plan" | "greebles" |
//             "ticks" | "dotgrid" | "widgets" | "plates" | "pipes" |
//             "grilles" | "fins" | "scrolls", ..., avoid, under, engrave }];
//           `avoid` keeps a line that far (half-widths) from each icon's
//           place, `under` draws it only that far either way of each place,
//           as the icon's stand; `engrave` cuts a line into the surface
//           instead of drawing it on
//   launcher: { at, where, on, s, t, size } a seat for the panel's
//             application launcher (a start button): where on the
//             decoration it is pinned instead of standing in the row
//   colours: { body, edge, light, accent }
//   pattern: "none" | "honeycomb" | "stripes" | "scales" | "grid" drawn
//            faintly over the body

function finite(value, fallback) {
    const number = Number(value)
    return Number.isFinite(number) ? number : fallback
}

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value))
}

// A number in [0, 1) from two numbers, the same every time.
function hash(n, k) {
    const x = Math.sin(n * 127.1 + k * 311.7) * 43758.5453
    return x - Math.floor(x)
}

// Smooth value noise in [-1, 1] over u; with `cells` > 0 it repeats every
// `cells`, so a closed band's wobble meets itself.
function valueNoise(u, cells, seed) {
    const i = Math.floor(u), f = u - i
    const a = cells > 0 ? ((i % cells) + cells) % cells : i
    const b = cells > 0 ? (a + 1) % cells : i + 1
    const w = f * f * (3 - 2 * f)
    return (hash(a, seed) * (1 - w) + hash(b, seed) * w) * 2 - 1
}

// An edge's wobble at x (half-widths along): two octaves of value noise;
// `total` is a closed band's length in half-widths (0 for an open one).
function roughAt(x, rough, total) {
    const cells = total > 0 ? Math.max(1, Math.round(total / rough.scale)) : 0
    const u = total > 0 ? x / total * cells : x / rough.scale
    const u2 = total > 0 ? x / total * cells * 3 : x / rough.scale * 3
    return 0.75 * valueNoise(u, cells, rough.seed) + 0.25 * valueNoise(u2, cells * 3, rough.seed + 7)
}

// The decoration with every member present.
function normalizedContour(raw) {
    if (raw && raw.normalized === true) return raw
    const contour = raw || ({})
    const side = function(value, seed) {
        const source = value || ({})
        const taper = source.taper || [1, 1]
        const rough = source.rough || ({})
        return { base: clamp(finite(source.base, 1), 0.2, 3),
                 taper: [clamp(finite(taper[0], 1), 0.1, 3), clamp(finite(taper[1], 1), 0.1, 3)],
                 rough: { amp: clamp(finite(rough.amp, 0), 0, 1), scale: clamp(finite(rough.scale, 1.5), 0.2, 20),
                          seed: finite(rough.seed, seed) },
                 features: (source.features || []).map(function(feature) {
                     return { kind: String(feature.kind || "step"),
                              length: clamp(finite(feature.length, 2), 0.1, 40),
                              depth: clamp(finite(feature.depth, 0.3), -0.9, 3),
                              chamfer: clamp(finite(feature.chamfer, 0.25), 0, 10),
                              period: clamp(finite(feature.period, 6), 0.5, 80),
                              phase: finite(feature.phase, 0),
                              anchor: String(feature.anchor || "period"),
                              skew: clamp(finite(feature.skew, 0), -0.95, 0.95),
                              curve: clamp(finite(feature.curve, 1), 0.25, 4),
                              vary: clamp(finite(feature.vary, 0), 0, 1),
                              at: (feature.at || []).map(function(value) { return clamp(finite(value, 0.5), 0, 1) }) }
                 }) }
    }
    const ends = contour.ends || ({})
    const segments = contour.segments || ({})
    const body = contour.body || ({})
    const sway = contour.sway || ({})
    const endSpec = function(value, fallback) {
        const source = Object.assign({}, fallback, value || ({}))
        return { kind: String(source.kind || "square"), size: clamp(finite(source.size, 1), 0, 20),
                 depth: clamp(finite(source.depth, 1.6), 0, 4),
                 outer: source.outer !== false, inner: source.inner !== false }
    }
    const both = endSpec(ends, {})
    return {
        normalized: true,
        outer: side(contour.outer, 1),
        inner: side(contour.inner, 2),
        ends: { start: endSpec(ends.start, both), end: endSpec(ends.end, both),
                cut: endSpec(ends.cut, { kind: "square" }) },
        segments: { kind: String(segments.kind || "none"), gap: clamp(finite(segments.gap, 0.3), 0, 4),
                    colours: (segments.colours || []).map(String) },
        sway: { amp: clamp(finite(sway.amp, 0), 0, 2), period: clamp(finite(sway.period, 6), 0.5, 80),
                phase: finite(sway.phase, 0) },
        body: { feather: clamp(finite(body.feather, 0), 0, 1.5),
                round: body.round === "chrome" ? "chrome" : body.round === true,
                fill: String(body.fill || "material"), glow: body.glow ? String(body.glow) : "",
                opacity: clamp(finite(body.opacity, 1), 0, 1),
                bevel: clamp(finite(body.bevel, 0.07), 0, 0.5),
                // a clear glaze over it (unset: as its kind of body has),
                // and how brightly it is lit round about (a light tent)
                coat: body.coat === undefined ? null : clamp(finite(body.coat, 0), 0, 1),
                gleam: clamp(finite(body.gleam, 1), 0.5, 3) },
        layers: (contour.layers || []).map(function(layer) { return Object.assign({ side: "middle" }, layer) }),
        parts: (contour.parts || []).map(function(part) { return Object.assign({ kind: "box", at: "middle" }, part) }),
        lines: (contour.lines || []).map(function(line) { return Object.assign({ kind: "middle" }, line) }),
        colours: Object.assign({ body: "#1b2027", edge: "#3a4654", light: "#66e0ff", accent: "#ffffff" },
                               contour.colours || ({})),
        pattern: String(contour.pattern || "none"),
        launcher: contour.launcher ? Object.assign({ at: "middle", size: 1.2 }, contour.launcher) : null,
        cache: ({})
    }
}

// The runs the band is cut into, as [from, to] along it, in path units.
// `slots` are the icons' places along the band (path units); a band cut
// between icons has one run per icon, its ends half a gap from the cut.
function runsOf(contour, length, closed, slots, halfWidth) {
    const decoration = normalizedContour(contour)
    if (decoration.segments.kind !== "icons" || !slots || slots.length < 2)
        return [{ from: 0, to: length, open: !closed, startCut: false, endCut: false, index: 0 }]
    const gap = decoration.segments.gap * halfWidth
    const sorted = slots.slice().sort(function(a, b) { return a - b })
    const cuts = []
    for (let index = 0; index + 1 < sorted.length; ++index)
        cuts.push((sorted[index] + sorted[index + 1]) / 2)
    if (closed) cuts.push(((sorted[sorted.length - 1] + sorted[0] + length) / 2) % length)
    cuts.sort(function(a, b) { return a - b })
    const runs = []
    if (closed) {
        for (let index = 0; index < cuts.length; ++index) {
            const from = cuts[index] + gap / 2
            let to = cuts[(index + 1) % cuts.length] - gap / 2
            if (index + 1 === cuts.length) to += length
            if (to - from > gap) runs.push({ from: from, to: to, open: true, startCut: true, endCut: true, index: runs.length })
        }
    } else {
        let start = 0
        for (const cut of cuts) {
            if (cut - gap / 2 - start > gap)
                runs.push({ from: start, to: cut - gap / 2, open: true, startCut: start > 0, endCut: true, index: runs.length })
            start = cut + gap / 2
        }
        if (length - start > gap)
            runs.push({ from: start, to: length, open: true, startCut: start > 0, endCut: false, index: runs.length })
    }
    return runs
}

// The equilateral pointed arch over x in [-1, 1]: 0 at the springing
// points, 1 at the apex.
function pointedArch(x) {
    const a = 1, u = Math.abs(clamp(x, -1, 1))
    // The arc of radius twice the half span, centred at the other springing
    // point: height sqrt(4a² - (u + a)²), a√3 at the apex.
    return Math.sqrt(Math.max(0, 4 * a * a - (u + a) * (u + a))) / (a * Math.sqrt(3))
}

// A feature's height at signed distance `d` (half-widths) from its centre.
function featureShape(feature, d) {
    const half = feature.length / 2
    const x = Math.abs(d)
    if (x >= half) return 0
    if (feature.kind === "tooth") {
        // A tooth, its tip moved by `skew` (a claw leaning over) and its
        // sides hollowed by `curve` (a thorn's or a talon's).
        const peak = feature.skew * half
        const v = d <= peak ? (d + half) / Math.max(1e-6, peak + half) : (half - d) / Math.max(1e-6, half - peak)
        return Math.pow(clamp(v, 0, 1), feature.curve)
    }
    if (feature.kind === "arch") return pointedArch(x / half)
    if (feature.kind === "wave") return Math.pow(Math.cos(Math.PI / 2 * x / half), 2)
    if (feature.kind === "round") {
        // A tower with a rounded top: straight sides, its top corners
        // quarter ellipses `chamfer` wide.
        const r = Math.min(Math.max(0.01, feature.chamfer), half)
        const inside = x - (half - r)
        return inside <= 0 ? 1 : Math.sqrt(Math.max(0, 1 - (inside / r) * (inside / r)))
    }
    // A step: full height, ramping down over the chamfer at each side.
    const ramp = Math.min(feature.chamfer, half)
    return ramp <= 0 ? 1 : Math.min(1, (half - x) / ramp)
}

// The icons' places sorted, and the places halfway between them (a closed
// band's last gap wrapping round).
function slotPlaces(slots, length, closed, between) {
    const sorted = (slots || []).slice().sort(function(a, b) { return a - b })
    if (!between) return sorted
    const places = []
    for (let index = 0; index + 1 < sorted.length; ++index) places.push((sorted[index] + sorted[index + 1]) / 2)
    if (closed && sorted.length > 1) places.push(((sorted[sorted.length - 1] + sorted[0] + length) / 2) % length)
    return places
}

// Where a feature's repeats are centred along the band (path units).
function featureCentres(feature, length, closed, slots, halfWidth) {
    if (feature.anchor === "middle") return [closed ? feature.phase * length : length / 2 + feature.phase * length]
    if (feature.anchor === "ends") return closed ? [] : [0, length]
    if (feature.anchor === "at") return (feature.at.length ? feature.at : [0.5]).map(function(f) { return f * length })
    if ((feature.anchor === "icons" || feature.anchor === "between") && slots && slots.length)
        return slotPlaces(slots, length, closed, feature.anchor === "between")
    const period = feature.period * halfWidth
    const count = closed ? Math.max(1, Math.round(length / period)) : Math.max(1, Math.floor(length / period))
    const step = closed ? length / count : period
    const start = closed ? feature.phase * step : (length - (count - 1) * step) / 2 + feature.phase * step
    const centres = []
    for (let index = 0; index < count; ++index) centres.push(start + index * step)
    return centres
}

// How far along a closed band, or an open one, a swell travels in one turn
// of its motion: about fourteen half-widths, a whole number of times round
// a closed band.
function swellLength(length, closed, halfWidth) {
    return closed ? length / Math.max(1, Math.round(length / (14 * halfWidth))) : 14 * halfWidth
}

// The two edges' distances from the middle at `s` (half-widths): the base,
// the features and the wobble, then the run's ends. A moving outline
// (`motion.waves`) swells and settles in a wave travelling along the band,
// its fine wobble rocking with it; `still` leaves the motion out.
function edgesAt(contour, s, run, length, closed, slots, halfWidth, still) {
    const decoration = normalizedContour(contour)
    const motion = still ? null : decoration.motion
    const waves = motion && motion.waves !== undefined ? motion.waves : null
    const swell = waves !== null ? swellLength(length, closed, halfWidth) : 1
    const cache = decoration.cache
    if (cache.length !== length || cache.closed !== closed || cache.slots !== slots || cache.halfWidth !== halfWidth) {
        cache.length = length; cache.closed = closed; cache.slots = slots; cache.halfWidth = halfWidth
        cache.centres = ({})
    }
    const sideAt = function(side, name) {
        let offset = side.base
        if (run && run.open && (side.taper[0] !== 1 || side.taper[1] !== 1)) {
            const share = clamp((s - run.from) / Math.max(1e-9, run.to - run.from), 0, 1)
            offset *= side.taper[0] + (side.taper[1] - side.taper[0]) * share
        }
        for (let index = 0; index < side.features.length; ++index) {
            const feature = side.features[index]
            const key = name + index
            let centres = cache.centres[key]
            if (!centres) {
                centres = featureCentres(feature, length, closed, slots, halfWidth)
                cache.centres[key] = centres
            }
            let best = 0
            for (let k = 0; k < centres.length; ++k) {
                let d = s - centres[k]
                if (closed) {
                    d = ((d % length) + length) % length
                    if (d > length / 2) d -= length
                }
                let shape = featureShape(feature, d / halfWidth)
                if (shape !== 0 && feature.vary > 0) shape *= 1 - feature.vary * hash(k + index * 31, name === "outer" ? 5 : 9)
                if (shape !== 0 && waves !== null) shape *= 1 + 0.4 * Math.sin(waves - 2 * Math.PI * centres[k] / swell)
                if (Math.abs(shape) > Math.abs(best)) best = shape
            }
            offset += feature.depth * best
        }
        if (side.rough.amp > 0)
            offset += side.rough.amp * roughAt(s / halfWidth + (waves !== null ? 0.5 * Math.sin(waves) : 0),
                                               side.rough, closed ? length / halfWidth : 0)
        return offset
    }
    let outer = sideAt(decoration.outer, "outer"), inner = sideAt(decoration.inner, "inner")
    if (decoration.sway.amp > 0) {
        // The middle swings: one edge moves out as the other moves in; a
        // closed band takes a whole number of swings round it.
        const total = length / halfWidth
        const swings = closed ? Math.max(1, Math.round(total / decoration.sway.period)) : total / decoration.sway.period
        // A swaying decoration rocks its swings back and forth.
        const rock = motion && motion.sway !== undefined ? motion.swayAmount * Math.sin(motion.sway) : 0
        const swing = decoration.sway.amp * Math.sin(2 * Math.PI * (swings * s / Math.max(1e-9, length) + decoration.sway.phase + rock))
        outer += swing
        inner -= swing
    }
    outer = Math.max(0, outer)
    inner = Math.max(0, inner)
    if (run && run.open) {
        // Each end of the run shaped as its kind of end says, the nearer
        // end winning.
        const shapeEnd = function(spec, e) {
            const shaped = function(offset, applies) {
                if (!applies) return offset
                if (spec.kind === "chamfer") return Math.min(offset, Math.max(0, offset - spec.size + e))
                if (spec.kind === "point") return Math.min(offset, offset * clamp(e / Math.max(0.05, spec.size), 0, 1))
                if (spec.kind === "pill") {
                    // A half circle as wide as the band at its end.
                    const radius = offset
                    return e >= radius ? offset : Math.sqrt(Math.max(0, radius * radius - (radius - e) * (radius - e)))
                }
                return offset
            }
            if (spec.kind === "broken") {
                if (e < spec.size) {
                    // Broken off: a jagged line rising across the band, or a
                    // jagged point when both sides break.
                    const share = clamp(e / Math.max(0.05, spec.size), 0, 1)
                    const teeth = Math.max(2, Math.round(spec.size * 2.2))
                    const zig = 0.22 * (Math.abs(((share * teeth) % 1) - 0.5) * 2 - 0.5) * (1 - share * 0.5)
                    const thickness = outer + inner
                    if (spec.outer && spec.inner) {
                        const middle = (outer - inner) / 2
                        outer = Math.min(outer, middle + thickness / 2 * share + zig)
                        inner = Math.min(inner, -middle + thickness / 2 * share - zig)
                    } else if (spec.inner) {
                        inner = Math.min(inner, inner - thickness * (1 - share) + zig)
                    } else if (spec.outer) {
                        outer = Math.min(outer, outer - thickness * (1 - share) + zig)
                    }
                }
                return
            }
            outer = shaped(outer, spec.outer)
            inner = shaped(inner, spec.inner)
            if (spec.kind === "block" && e < spec.size) {
                // A heavier block at the end, its outer corner rounded and
                // its step down to the band rounded the other way.
                const radius = Math.min(spec.size, Math.max(0, spec.depth - outer)) * 0.8
                const corner = e < radius ? Math.sqrt(Math.max(0, radius * radius - (radius - e) * (radius - e))) - radius : 0
                outer = Math.max(outer, spec.depth + corner)
            } else if (spec.kind === "block" && e < spec.size + 0.6) {
                const f = (e - spec.size) / 0.6
                outer = Math.max(outer, outer + (spec.depth - outer) * (1 - Math.sqrt(Math.max(0, 1 - (1 - f) * (1 - f)))))
            }
        }
        const ends = decoration.ends
        const fromStart = (s - run.from) / halfWidth, fromEnd = (run.to - s) / halfWidth
        if (fromStart <= fromEnd) shapeEnd(run.startCut ? ends.cut : ends.start, fromStart)
        else shapeEnd(run.endCut ? ends.cut : ends.end, fromEnd)
    }
    // Only a broken end crosses the middle; where its two edges would pass
    // each other the band ends.
    if (outer + inner < 0) {
        const middle = (outer - inner) / 2
        outer = middle
        inner = -middle
    }
    return { outer: outer, inner: inner }
}

// A path laid out for (s, t): its points, the length along it at each, and
// a function from (s, t) to a point (t in path units across, + outward).
// An open path goes on straight past its ends, so parts and bristles can
// stand beyond them.
function ribbon(path) {
    const points = (path && path.points) || []
    const closed = path && path.closed === true
    // For an edge panel the outer side faces into the screen: the path
    // says which way that is (`outward`, a direction), whatever way its
    // points run.
    const hint = path && path.outward ? path.outward : null
    const list = closed && points.length > 1 ? points.concat([points[0]]) : points.slice()
    const at = [0]
    for (let index = 1; index < list.length; ++index)
        at.push(at[index - 1] + Math.hypot(list[index].x - list[index - 1].x, list[index].y - list[index - 1].y))
    const length = at[at.length - 1] || 0
    // The outward side: away from the path's centre for a closed path, the
    // path's left for an open one (up the screen for a left-to-right bar).
    let cx = 0, cy = 0
    for (const point of points) { cx += point.x; cy += point.y }
    cx /= Math.max(1, points.length); cy /= Math.max(1, points.length)
    const pointAt = function(v) {
        let w = closed ? ((v % length) + length) % length : clamp(v, 0, length)
        let k = 1
        while (k < at.length - 1 && at[k] < w) ++k
        const p = list[k - 1], q = list[k]
        const f = clamp((w - at[k - 1]) / Math.max(1e-9, at[k] - at[k - 1]), 0, 1)
        return [p.x + (q.x - p.x) * f, p.y + (q.y - p.y) * f]
    }
    const frame = function(s) {
        const value = closed ? ((s % length) + length) % length : clamp(s, 0, length)
        const here = pointAt(value)
        // The direction here, smoothed over a short stretch so a polygon's
        // corner turns instead of breaking.
        const near = Math.max(1e-6, Math.min(4, length / 200))
        const before = pointAt(closed ? value - near : Math.max(0, value - near))
        const after = pointAt(closed ? value + near : Math.min(length, value + near))
        let dx = after[0] - before[0], dy = after[1] - before[1]
        const norm = Math.hypot(dx, dy) || 1
        dx /= norm; dy /= norm
        // Screen y runs down: the left of the direction is (dy, -dx).
        let nx = dy, ny = -dx
        if (hint) {
            if (nx * hint.x + ny * hint.y < 0) { nx = -nx; ny = -ny }
        } else if (closed && (here[0] - cx) * nx + (here[1] - cy) * ny < 0) { nx = -nx; ny = -ny }
        return { x: here[0], y: here[1], nx: nx, ny: ny, dx: dx, dy: dy }
    }
    return {
        length: length,
        closed: closed,
        frame: frame,
        point: function(s, t) {
            if (!closed && (s < 0 || s > length)) {
                const f = frame(s < 0 ? 0 : length)
                const over = s < 0 ? s : s - length
                return { x: f.x + f.dx * over + f.nx * t, y: f.y + f.dy * over + f.ny * t }
            }
            const f = frame(s)
            return { x: f.x + f.nx * t, y: f.y + f.ny * t }
        }
    }
}

// Sample positions along a run, no further apart than `step`.
function runSamples(run, step) {
    const count = Math.max(2, Math.ceil((run.to - run.from) / Math.max(0.25, step)) + 1)
    const samples = []
    for (let index = 0; index < count; ++index) samples.push(run.from + (run.to - run.from) * index / (count - 1))
    return samples
}

// Whether `s` lies within `radius` of an icon's place.
function nearIcon(s, slots, radius, length, closed) {
    if (!slots || radius <= 0) return false
    for (const slot of slots) {
        let d = Math.abs(s - slot)
        if (closed) d = Math.min(d, length - d)
        if (d < radius) return true
    }
    return false
}

// How far round each icon's place a line keeps away (half-widths); a line
// drawn under the icons as their stands (`under`) asks for the opposite, as
// a distance below 0: it runs that far either way of each place and nowhere
// else.
function avoidOf(line, fallback) {
    return line.under !== undefined ? -Math.max(0.05, finite(line.under, 1)) : finite(line.avoid, fallback)
}

// The stretches of a run clear of the icons: [from + margin, to - margin]
// without `avoid` round each icon's place; or, when `avoid` is below 0, the
// stretches that far either way of each place, the icons' stands.
function freeIntervals(run, margin, slots, avoid, length, closed) {
    if (avoid < 0) {
        // Each stand reaches at most a little under halfway to the next
        // icon, so the stands stay one under each icon on a tight row.
        const from = run.from + margin, to = run.to - margin
        const places = (slots || []).slice().sort(function(p, q) { return p - q })
        const under = []
        for (let k = 0; k < places.length; ++k) {
            let room = Infinity
            if (k > 0) room = Math.min(room, places[k] - places[k - 1])
            if (k + 1 < places.length) room = Math.min(room, places[k + 1] - places[k])
            if (closed && places.length > 1) room = Math.min(room, places[0] + length - places[places.length - 1])
            const reach = Math.min(-avoid, room * 0.42)
            for (const shift of closed ? [-length, 0, length] : [0]) {
                const a = Math.max(from, places[k] + shift - reach), b = Math.min(to, places[k] + shift + reach)
                if (b - a > 1e-6) under.push([a, b])
            }
        }
        under.sort(function(p, q) { return p[0] - q[0] })
        return under
    }
    let intervals = [[run.from + margin, run.to - margin]]
    if (avoid > 0)
        for (const slot of slots || []) {
            for (const shift of closed ? [-length, 0, length] : [0]) {
                const a = slot + shift - avoid, b = slot + shift + avoid
                const next = []
                for (const interval of intervals) {
                    if (b <= interval[0] || a >= interval[1]) { next.push(interval); continue }
                    if (a > interval[0]) next.push([interval[0], a])
                    if (b < interval[1]) next.push([b, interval[1]])
                }
                intervals = next
            }
        }
    return intervals.filter(function(interval) { return interval[1] - interval[0] > 1e-6 })
}

// A rounded bar from a to b along the band (path units), `height` across
// it (half-widths), centred at t: half circles at its two ends.
function pillPoints(a, b, t, height, halfWidth) {
    const rt = height / 2, rs = Math.min((b - a) / 2, rt * halfWidth)
    const points = []
    for (let k = 0; k <= 8; ++k) {
        const angle = Math.PI / 2 + Math.PI * k / 8
        points.push([a + rs + rs * Math.cos(angle), t + rt * Math.sin(angle)])
    }
    for (let k = 0; k <= 8; ++k) {
        const angle = -Math.PI / 2 + Math.PI * k / 8
        points.push([b - rs + rs * Math.cos(angle), t + rt * Math.sin(angle)])
    }
    return points
}

// The Elder Futhark's letters as their straight strokes, in a box from
// (-0.5, -1) to (0.5, 1), y up.
const RUNES = [
    [[[0, -1], [0, 1]], [[0, 0.05], [0.45, 0.5]], [[0, 0.5], [0.45, 0.95]]],
    [[[-0.3, -1], [-0.3, 1], [0.35, 0.45], [0.35, -1]]],
    [[[-0.2, -1], [-0.2, 1]], [[-0.2, 0.5], [0.3, 0], [-0.2, -0.5]]],
    [[[0, -1], [0, 1]], [[0, 1], [0.45, 0.6]], [[0, 0.45], [0.45, 0.05]]],
    [[[-0.25, -1], [-0.25, 1], [0.3, 0.55], [-0.25, 0.1], [0.3, -1]]],
    [[[0.3, 0.6], [-0.3, 0], [0.3, -0.6]]],
    [[[-0.4, -0.9], [0.4, 0.9]], [[0.4, -0.9], [-0.4, 0.9]]],
    [[[-0.2, -1], [-0.2, 1], [0.3, 0.55], [-0.2, 0.1]]],
    [[[-0.3, -1], [-0.3, 1]], [[0.3, -1], [0.3, 1]], [[-0.3, 0.3], [0.3, -0.3]]],
    [[[0, -1], [0, 1]], [[-0.35, 0.35], [0.35, -0.15]]],
    [[[0, -1], [0, 1]]],
    [[[-0.05, 0.85], [-0.4, 0.4], [-0.05, -0.05]], [[0.05, 0.05], [0.4, -0.4], [0.05, -0.85]]],
    [[[0, -1], [0, 1]], [[0, 1], [0.35, 0.65]], [[0, -1], [-0.35, -0.65]]],
    [[[-0.3, -1], [-0.3, 1]], [[-0.3, 1], [0.25, 0.55], [0.25, 0.35]], [[-0.3, -1], [0.25, -0.55], [0.25, -0.35]]],
    [[[0, -1], [0, 1]], [[0, 0.25], [-0.45, 0.85]], [[0, 0.25], [0.45, 0.85]]],
    [[[-0.3, 0.9], [0.25, 0.35], [-0.25, -0.35], [0.3, -0.9]]],
    [[[0, -1], [0, 1]], [[-0.45, 0.55], [0, 1], [0.45, 0.55]]],
    [[[-0.3, -1], [-0.3, 1], [0.3, 0.55], [-0.3, 0.05], [0.3, -0.45], [-0.3, -1]]],
    [[[-0.35, -1], [-0.35, 1], [0, 0.5], [0.35, 1], [0.35, -1]]],
    [[[-0.35, -1], [-0.35, 1], [0.35, 0.3]], [[0.35, -1], [0.35, 1], [-0.35, 0.3]]],
    [[[-0.2, -1], [-0.2, 1], [0.35, 0.5]]],
    [[[0, 0.6], [0.4, 0], [0, -0.6], [-0.4, 0], [0, 0.6]]],
    [[[-0.4, -0.9], [-0.4, 0.9], [0.4, -0.9], [0.4, 0.9], [-0.4, -0.9]]],
    [[[0, 0.9], [0.38, 0.35], [-0.38, -1]], [[0, 0.9], [-0.38, 0.35], [0.38, -1]]]
]

// The lines a decoration draws, in (s, t) for each run: [{ points: [[s, t]],
// closed, width, colour, kind, dash, ... }], s in path units, t and widths
// in half-widths. `fill` marks a closed shape to fill (a window, a display
// block), `clip` one to keep inside the band, `fade` one that fades out
// along its length, `outline` a strand's darker rim.
// A footprint's strength as a walk passes: it appears as the walker sets
// foot there and fades over the next strides; `u` is how far the walk has
// gone past it, in its share of the strides shown at once.
function footfall(u) {
    const v = u - Math.floor(u)
    return v < 0.08 ? 0.2 + 0.8 * v / 0.08 : Math.max(0.2, 1 - (v - 0.08) / 0.7)
}

function linesOf(contour, runs, length, closed, slots, halfWidth, step) {
    const decoration = normalizedContour(contour)
    const motion = decoration.motion
    // Light flowing along a wave or a plait moves it on by one wavelength a
    // turn; a sway rocks a plait's strands back and forth.
    const flow = motion && motion.flow !== undefined ? motion.flow : 0
    const rock = motion && motion.sway !== undefined ? 2 * Math.PI * motion.swayAmount * Math.sin(motion.sway) : 0
    const result = []
    for (const line of decoration.lines) {
        const before = result.length
        const width = finite(line.width, 0.08)
        const colour = line.colour || (line.kind === "spiral" || line.kind === "wave" ? decoration.colours.accent : decoration.colours.light)
        const inset = finite(line.inset, 0.3)
        for (const run of runs) {
            const samples = runSamples(run, step)
            const edge = function(s) { return edgesAt(decoration, s, run, length, closed, slots, halfWidth) }
            const stillEdge = function(s) { return edgesAt(decoration, s, run, length, closed, slots, halfWidth, true) }
            const across = function(s) {
                const e = edge(s)
                return line.side === "outer" ? e.outer - inset : line.side === "inner" ? -(e.inner - inset) : finite(line.offset, 0)
            }
            if (line.kind === "loop") {
                // A loop inset from the edges; round an open run's end it
                // follows the end inset too.
                const outer = samples.map(function(s) { return [s, Math.max(0, edge(s).outer - inset)] })
                const inner = samples.map(function(s) { return [s, -Math.max(0, edge(s).inner - inset)] })
                if (run.open) {
                    const points = outer.filter(function(p) { return p[1] > 0.02 })
                        .concat(inner.slice().reverse().filter(function(p) { return p[1] < -0.02 }))
                    result.push({ kind: line.kind, points: points, closed: true, width: width, colour: colour, dash: line.dash })
                } else {
                    result.push({ kind: line.kind, points: outer, closed: true, width: width, colour: colour, dash: line.dash })
                    result.push({ kind: line.kind, points: inner, closed: true, width: width, colour: colour, dash: line.dash })
                }
            } else if (line.kind === "middle") {
                const shift = finite(line.offset, 0)
                const from = run.open ? finite(line.margin, 0.6) * halfWidth : 0
                const points = samples.filter(function(s) { return s - run.from >= from && run.to - s >= from })
                    .map(function(s) { return [s, shift] })
                result.push({ kind: line.kind, points: points, closed: !run.open, width: width, colour: colour, dash: line.dash })
            } else if (line.kind === "edge") {
                const outerSide = line.side !== "inner"
                const points = samples.map(function(s) {
                    const e = edge(s)
                    return [s, outerSide ? e.outer : -e.inner]
                })
                result.push({ kind: line.kind, points: points, closed: !run.open, width: width, colour: colour, dash: line.dash })
            } else if (line.kind === "wave") {
                const period = finite(line.period, 3) * halfWidth
                const amp = finite(line.amp, 0.3), phase = finite(line.phase, 0)
                const shift = finite(line.offset, 0)
                // A closed band takes a whole number of waves round it.
                const waves = run.open ? (run.to - run.from) / period : Math.max(1, Math.round((run.to - run.from) / period))
                const points = samples.map(function(s) {
                    return [s, shift + amp * Math.sin(2 * Math.PI * (waves * (s - run.from) / (run.to - run.from) + phase) - flow)]
                })
                result.push({ kind: line.kind, points: points, closed: !run.open, width: width, colour: colour, dash: line.dash })
            } else if (line.kind === "scrolls") {
                // Filigree: the running scroll of classical ornament worked
                // in wire - a stem waving along the band whose every crest
                // curls into a spiral, above and below in turn - with a
                // grain of the metal in each spiral's eye.
                const pitch = Math.max(0.6, finite(line.period, 1.8)) * halfWidth
                const amp = finite(line.amp, 0.55), curl = finite(line.size, 0.3)
                const turns = finite(line.turns, 1.35), offset = finite(line.offset, 0)
                const margin = run.open ? finite(line.margin, 0.5) * halfWidth : 0
                const from = run.from + margin, to = run.to - margin
                const count = run.open ? Math.max(1, Math.floor((to - from) / pitch))
                                       : Math.max(1, Math.round((run.to - run.from) / pitch))
                const span = run.open ? count * pitch : run.to - run.from
                const first = run.open ? from + ((to - from) - span) / 2 : run.from
                const step = span / count
                const stem = []
                const pointsPerUnit = Math.max(12, Math.ceil(step / Math.max(1, step / 24)))
                for (let k = 0; k < count; ++k)
                    for (let i = 0; i < pointsPerUnit; ++i) {
                        const u = (k + i / pointsPerUnit)
                        stem.push([first + u * step, offset + amp * 0.62 * Math.sin(Math.PI * u)])
                    }
                if (run.open) stem.push([first + span, offset])
                else stem.push([first + span, offset + amp * 0.62 * Math.sin(Math.PI * count)])
                result.push({ kind: line.kind, points: stem, closed: !run.open, width: width, colour: colour,
                              material: line.material || null, cap: "round" })
                const eyes = [], eyeSizes = []
                for (let k = 0; k < count; ++k) {
                    // At each crest a curl leaves the stem heading on along
                    // it and winds inward round a centre one curl's radius
                    // toward the band's middle, tightening as it goes.
                    const side = k % 2 === 0 ? 1 : -1
                    const crest = first + (k + 0.5) * step
                    const top = offset + side * amp * 0.62
                    const centre = [crest, top - side * curl]
                    const points = []
                    const n = 36
                    for (let i = 0; i <= n; ++i) {
                        const v = i / n
                        const r = curl * (1 - 0.8 * v)
                        const a = Math.PI / 2 - 2 * Math.PI * turns * v
                        points.push([centre[0] + Math.cos(a) * r * halfWidth, centre[1] + side * Math.sin(a) * r])
                    }
                    result.push({ kind: line.kind, points: points, closed: false, width: width * 0.85, colour: colour,
                                  material: line.material || null, cap: "round" })
                    eyes.push(points[points.length - 1])
                    eyeSizes.push(1)
                }
                if (line.grains !== false)
                    result.push({ kind: "dots", points: eyes, closed: false, width: width * 1.25, colour: colour,
                                  dots: true, sizes: eyeSizes })
            } else if (line.kind === "dots") {
                // Round studs or rivets in a row, `period` apart, at `offset`
                // across (or on both edges, inset, with side "both").
                const period = finite(line.period, 1.2) * halfWidth
                const count = run.open ? Math.max(1, Math.floor((run.to - run.from) / period))
                                       : Math.max(1, Math.round((run.to - run.from) / period))
                const spacing = (run.to - run.from) / count
                const margin = run.open ? spacing / 2 : 0
                const rows = line.side === "both" ? ["outer", "inner"] : [line.side || "middle"]
                for (const row of rows) {
                    const points = []
                    for (let index = 0; index < count; ++index) {
                        const s = run.from + margin + index * spacing
                        if (s > run.to) break
                        if (line.avoid && nearIcon(s, slots, finite(line.avoid, 0) * halfWidth, length, closed)) continue
                        const e = edge(s)
                        const t = row === "outer" ? Math.max(0, e.outer - inset)
                                : row === "inner" ? -Math.max(0, e.inner - inset) : finite(line.offset, 0)
                        points.push([s, t])
                    }
                    result.push({ kind: line.kind, points: points, closed: false, width: width, colour: colour, dots: true })
                }
            } else if (line.kind === "stars") {
                // Sparkles scattered over the band, the same every time: a
                // hash of their number gives each its place and size.
                const density = finite(line.period, 0.8)
                const count = Math.max(1, Math.round((run.to - run.from) / halfWidth * density))
                const salt = finite(line.seed, 0)
                const points = [], sizes = []
                for (let index = 0; index < count; ++index) {
                    const s = run.from + hash(index, 1 + salt) * (run.to - run.from)
                    const e = stillEdge(s)
                    const t = -e.inner * 0.8 + hash(index, 2 + salt) * (e.inner + e.outer) * 0.8
                    points.push([s, t])
                    sizes.push(0.4 + 0.9 * hash(index, 3 + salt))
                }
                result.push({ kind: line.kind, points: points, closed: false, width: width, colour: colour, dots: true, sizes: sizes })
            } else if (line.kind === "slats") {
                // Vent slats across the band, `period` apart, each leaning
                // by `amp` along it, spanning `size` of its width.
                const period = finite(line.period, 0.7) * halfWidth
                const lean = finite(line.amp, 0.25) * halfWidth, span = finite(line.size, 0.55)
                const offset = finite(line.offset, 0)
                const margin = run.open ? finite(line.margin, 0.8) * halfWidth : 0
                const count = Math.max(1, Math.floor((run.to - run.from - 2 * margin) / period))
                for (let index = 0; index <= count; ++index) {
                    const s = run.from + margin + index * period
                    if (s > run.to - margin) break
                    if (line.avoid && nearIcon(s, slots, finite(line.avoid, 0) * halfWidth, length, closed)) continue
                    result.push({ kind: line.kind, points: [[s - lean, offset - span], [s + lean, offset + span]], closed: false,
                                  width: width, colour: colour })
                }
            } else if (line.kind === "ends" && run.open) {
                // A round light at each end of the band (not at the cuts);
                // `colours` may give each end its own (a ship's red and
                // green running lights).
                const colours = line.colours && line.colours.length ? line.colours : [colour]
                for (const atStart of [true, false]) {
                    if (atStart ? run.startCut : run.endCut) continue
                    const s = atStart ? run.from + inset * halfWidth : run.to - inset * halfWidth
                    result.push({ kind: line.kind, points: [[s, finite(line.offset, 0)]], closed: false, width: width,
                                  colour: atStart ? colours[0] : colours[colours.length - 1], dots: true })
                }
            } else if (line.kind === "brackets" && run.open) {
                // At each end, a short run down the end and a long run along
                // the outer edge, round the cut corner (Arwes' Nefrex).
                const long = finite(line.long, 4) * halfWidth, short = finite(line.short, 0.6)
                for (const atStart of [true, false]) {
                    const points = []
                    const span = Math.min(long, (run.to - run.from) / 2)
                    const count = Math.max(4, Math.ceil(span / Math.max(0.5, step)))
                    for (let index = count; index >= 0; --index) {
                        const s = atStart ? run.from + span * index / count : run.to - span * index / count
                        points.push([s, Math.max(0, edge(s).outer - inset)])
                    }
                    const endS = atStart ? run.from + inset * halfWidth * 0.5 : run.to - inset * halfWidth * 0.5
                    const top = points[points.length - 1][1]
                    points.push([endS, Math.max(-1, top - short)])
                    result.push({ kind: line.kind, points: points, closed: false, width: width, colour: colour, dash: line.dash })
                }
            } else if (line.kind === "spiral" && run.open) {
                // A scroll curling off each pointed end: a logarithmic
                // spiral starting at the tip, rising outward and turning back
                // over the band, and its mirror on the inner side.
                const turns = finite(line.turns, 1.25), size = finite(line.size, 0.9)
                for (const atStart of [true, false]) {
                    if (atStart ? run.startCut : run.endCut) continue
                    if ((line.at === "start" && !atStart) || (line.at === "end" && atStart)) continue
                    const dir = atStart ? -1 : 1
                    const tip = atStart ? run.from : run.to
                    for (const up of [1, -1]) {
                        const centre = [0.55 * size, 0.75 * size * up]
                        const r0 = Math.hypot(centre[0], centre[1])
                        const phi0 = Math.atan2(-centre[1], -centre[0])
                        const points = []
                        const count = 56
                        for (let index = 0; index <= count; ++index) {
                            const theta = index / count * turns * 2 * Math.PI
                            const r = r0 * Math.exp(-0.42 * theta)
                            const angle = phi0 - up * theta
                            const a = centre[0] + r * Math.cos(angle), b = centre[1] + r * Math.sin(angle)
                            points.push([tip + dir * a * halfWidth, b])
                        }
                        result.push({ kind: line.kind, points: points, closed: false, width: width, colour: colour, dash: line.dash })
                    }
                }
            } else if (line.kind === "windows") {
                // Rows of small lit windows (or a console's buttons) along
                // the band, kept clear of the icons; a hash decides which
                // are lit, the dark ones drawn in `dim` if it is given.
                const size = line.size || [0.18, 0.12]
                const pitch = Math.max(0.05, finite(line.period, 0.45)) * halfWidth
                const w = finite(size[0], 0.18) * halfWidth, h = finite(size[1], 0.12)
                const lit = clamp(finite(line.lit, 0.75), 0, 1)
                const seed = finite(line.seed, 3)
                const margin = run.open ? finite(line.margin, 1.2) * halfWidth : 0
                let index = 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 1.0) * halfWidth, length, closed)) {
                    const count = Math.floor((interval[1] - interval[0]) / pitch)
                    const start = interval[0] + (interval[1] - interval[0] - (count - 1) * pitch) / 2
                    for (let k = 0; k < count; ++k, ++index) {
                        const s = start + k * pitch
                        const on = hash(index, seed) < lit
                        if (!on && !line.dim) continue
                        const t = across(s)
                        result.push({ kind: line.kind, fill: true, closed: true, glow: on, width: 0,
                                      colour: on ? colour : String(line.dim),
                                      points: [[s - w / 2, t - h / 2], [s + w / 2, t - h / 2], [s + w / 2, t + h / 2], [s - w / 2, t + h / 2]] })
                    }
                }
            } else if (line.kind === "chips") {
                // A console's display blocks: rounded bars of several
                // lengths and colours filling the band between the icons.
                const lengths = line.lengths || [0.5, 1.4]
                const shortest = finite(lengths[0], 0.5), longest = finite(lengths[1], 1.4)
                const gap = finite(line.gap, 0.12) * halfWidth
                const height = finite(line.height, 0.22)
                const colours = line.colours && line.colours.length ? line.colours : [colour]
                const seed = finite(line.seed, 7)
                const margin = run.open ? finite(line.margin, 0.8) * halfWidth : 0
                let index = 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 1.0) * halfWidth, length, closed)) {
                    let s = interval[0]
                    while (interval[1] - s > gap * 1.5 && index < 4000) {
                        let span = (shortest + (longest - shortest) * hash(index, seed)) * halfWidth
                        // the last block of a stretch takes what is left, if
                        // what would be left is too short for one of its own
                        if (interval[1] - (s + span) < shortest * halfWidth + gap) span = interval[1] - s
                        const t = across(s + span / 2)
                        if (span > gap)
                            result.push({ kind: line.kind, fill: true, closed: true, width: 0, colour: colours[index % colours.length],
                                          points: line.square ? [[s, t - height / 2], [s + span, t - height / 2], [s + span, t + height / 2], [s, t + height / 2]]
                                                              : pillPoints(s, s + span, t, height, halfWidth) })
                        s += span + gap
                        ++index
                    }
                }
            } else if (line.kind === "streaks") {
                // Stars stretched into streaks rushing out past the band, as
                // seen from a ship at warp: each starts just outside an edge
                // and runs away from the band's middle.
                const count = Math.max(1, Math.round(finite(line.count, 24)))
                const lengths = line.length || [0.6, 2.2]
                const spread = finite(line.spread, 2.5)
                const seed = finite(line.seed, 11)
                const middle = (run.from + run.to) / 2
                for (let k = 0; k < count; ++k) {
                    const up = hash(k, seed) < 0.5
                    const s0 = run.from + hash(k, seed + 1) * (run.to - run.from)
                    const e = edge(s0)
                    const out = 0.2 + hash(k, seed + 2) * spread
                    const t0 = up ? e.outer + out : -(e.inner + out)
                    const ds = (s0 - middle) / halfWidth * 0.08, dt = t0
                    const norm = Math.hypot(ds, dt) || 1
                    const span = finite(lengths[0], 0.6) + (finite(lengths[1], 2.2) - finite(lengths[0], 0.6)) * hash(k, seed + 3)
                    result.push({ kind: line.kind, closed: false, fade: true, colour: colour, width: width * (0.5 + hash(k, seed + 4)),
                                  points: [[s0, t0], [s0 + ds / norm * span * halfWidth, t0 + dt / norm * span]] })
                }
            } else if (line.kind === "plait") {
                // Strands twisted round each other (a woody vine, a cord,
                // knotwork): each strand a sine wave a share of a turn behind
                // the last, cut into short pieces drawn back to front by how
                // near the viewer each lies, so the crossings pass over and
                // under in turn. At an open run's ends the strands gather.
                const strands = Math.max(2, Math.min(4, Math.round(finite(line.strands, 2))))
                const period = Math.max(0.5, finite(line.period, 3)) * halfWidth
                const amp = finite(line.amp, 0.6)
                const offset = finite(line.offset, 0)
                const margin = run.open ? finite(line.margin, 0.3) * halfWidth : 0
                const from = run.from + margin, to = run.to - margin
                if (to - from > 1) {
                    const waves = run.open ? (to - from) / period : Math.max(1, Math.round((to - from) / period))
                    const count = Math.max(8, Math.ceil((to - from) / Math.max(1, step)))
                    const colours = line.colours && line.colours.length ? line.colours : [colour]
                    const gather = function(u) {
                        return run.open ? Math.min(1, Math.min(u, 1 - u) * (to - from) / (finite(line.gather, 1.2) * halfWidth)) : 1
                    }
                    const pieces = []
                    for (let k = 0; k < strands; ++k)
                        for (let i = 0; i < count; ++i) {
                            const u0 = i / count, u1 = (i + 1) / count
                            const pa = 2 * Math.PI * (waves * u0 + k / strands) - flow + rock
                            const pb = 2 * Math.PI * (waves * u1 + k / strands) - flow + rock
                            pieces.push({ z: Math.cos((pa + pb) / 2), colour: colours[k % colours.length],
                                          points: [[from + (to - from) * u0 - (i ? 0.4 : 0), offset + amp * gather(u0) * Math.sin(pa)],
                                                   [from + (to - from) * u1 + (i + 1 < count ? 0.4 : 0), offset + amp * gather(u1) * Math.sin(pb)]] })
                        }
                    pieces.sort(function(a, b) { return a.z - b.z })
                    for (const piece of pieces)
                        result.push({ kind: line.kind, closed: false, points: piece.points, width: width, colour: piece.colour,
                                      outline: line.outline || "#000000aa", outlineWidth: finite(line.outlineWidth, 0.06), cap: "butt",
                                      z: piece.z, material: line.material || null, shine: line.shine !== false })
                }
            } else if (line.kind === "tendrils") {
                // Thin tendrils springing from the edges, each a short stalk
                // ending in a coil, on alternate sides (or one side).
                const pitch = Math.max(0.5, finite(line.period, 2.6)) * halfWidth
                const size = finite(line.size, 0.7)
                const seed = finite(line.seed, 17)
                const margin = run.open ? finite(line.margin, 1.0) * halfWidth : 0
                let k = 0
                for (let s0 = run.from + margin + pitch / 2; s0 < run.to - margin; s0 += pitch, ++k) {
                    if (line.avoid && nearIcon(s0, slots, finite(line.avoid, 0) * halfWidth, length, closed)) continue
                    const up = line.side === "outer" ? true : line.side === "inner" ? false : k % 2 === 0
                    const sign = up ? 1 : -1
                    const e = edge(s0)
                    const base = [s0, sign * Math.max(0, (up ? e.outer : e.inner) - 0.06)]
                    const lean = (hash(k, seed) - 0.3) * 0.9
                    const reach = size * (0.7 + 0.6 * hash(k, seed + 1))
                    const turns = 1.4 + hash(k, seed + 2) * 1.2
                    const stalk = [s0 + lean * reach * 0.6 * halfWidth, base[1] + sign * reach * 0.6]
                    const points = []
                    for (let i = 0; i <= 6; ++i) {
                        const u = i / 6
                        points.push([base[0] + (stalk[0] - base[0]) * u, base[1] + (stalk[1] - base[1]) * u + sign * 0.08 * Math.sin(Math.PI * u)])
                    }
                    const radius = reach * 0.35
                    const spin = hash(k, seed + 3) < 0.5 ? 1 : -1
                    const centre = [stalk[0] + spin * radius * halfWidth, stalk[1]]
                    const start = spin > 0 ? Math.PI : 0
                    for (let i = 1; i <= 40; ++i) {
                        const u = i / 40
                        const r = radius * Math.exp(-1.1 * u * turns)
                        const a = start - spin * sign * 2 * Math.PI * turns * u
                        points.push([centre[0] + Math.cos(a) * r * halfWidth, centre[1] + Math.sin(a) * r])
                    }
                    result.push({ kind: line.kind, closed: false, points: points, width: width, colour: colour, cap: "round" })
                }
            } else if (line.kind === "bristles" && run.open) {
                // A broom's bristles: straws fanning out from the band's end,
                // bound at the root, splayed and uneven at the tips.
                const atStart = line.at === "start"
                if (!(atStart ? run.startCut : run.endCut)) {
                    const count = Math.max(3, Math.round(finite(line.count, 40)))
                    const reach = finite(line.length, 5) * halfWidth
                    const root = finite(line.root, 0.75), spread = finite(line.spread, 1.6)
                    const seed = finite(line.seed, 23)
                    const offset = finite(line.offset, 0)
                    const s0 = atStart ? run.from + finite(line.inset, 0) * halfWidth : run.to - finite(line.inset, 0) * halfWidth
                    const dir = atStart ? -1 : 1
                    const colours = line.colours && line.colours.length ? line.colours : [colour]
                    for (let k = 0; k < count; ++k) {
                        const u = (k + hash(k, seed)) / count * 2 - 1
                        const far = reach * (0.82 + 0.3 * hash(k, seed + 1))
                        const t0 = u * root + offset
                        const t1 = u * spread + (hash(k, seed + 2) - 0.5) * 0.25 + offset
                        const bow = (hash(k, seed + 3) - 0.5) * 0.18
                        const points = []
                        for (let i = 0; i <= 5; ++i) {
                            const v = i / 5
                            points.push([s0 + dir * far * v, t0 + (t1 - t0) * Math.pow(v, 0.8) + bow * Math.sin(Math.PI * v)])
                        }
                        result.push({ kind: line.kind, closed: false, points: points, width: width * (0.7 + 0.6 * hash(k, seed + 4)),
                                      colour: colours[Math.floor(hash(k, seed + 5) * colours.length) % colours.length], cap: "round" })
                    }
                }
            } else if (line.kind === "runes") {
                // Runes carved between the icons: the Elder Futhark's
                // letters, picked by a hash, `size` tall, `period` apart.
                const size = finite(line.size, 0.9)
                const pitch = Math.max(0.2, finite(line.period, 0.8)) * halfWidth
                const margin = run.open ? finite(line.margin, 0.9) * halfWidth : 0
                const seed = finite(line.seed, 29)
                let index = 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 1.0) * halfWidth, length, closed)) {
                    const room = interval[1] - interval[0]
                    const count = Math.floor(room / pitch)
                    if (count < 1) continue
                    const start = interval[0] + (room - (count - 1) * pitch) / 2
                    for (let k = 0; k < count; ++k, ++index) {
                        const glyph = RUNES[Math.floor(hash(index, seed) * RUNES.length) % RUNES.length]
                        const cs = start + k * pitch, ct = across(cs)
                        for (const stroke of glyph)
                            result.push({ kind: line.kind, closed: false, colour: colour, width: width, cap: "round",
                                          points: stroke.map(function(p) { return [cs + p[0] * size * 0.5 * halfWidth, ct + p[1] * size * 0.5] }) })
                    }
                }
            } else if (line.kind === "lightning") {
                // Electric arcs from one end of the band to the other:
                // midpoint displacement, each bolt its own seed, with a few
                // short forks.
                const bolts = Math.max(1, Math.round(finite(line.bolts, 3)))
                const amp = finite(line.amp, 0.55)
                const levels = Math.max(2, Math.min(9, Math.round(finite(line.detail, 7))))
                const margin = run.open ? finite(line.margin, 0.5) * halfWidth : 0
                const from = run.from + margin, to = run.to - margin
                const seed = finite(line.seed, 37)
                const offset = finite(line.offset, 0)
                const keep = clamp(finite(line.keep, 0.92), 0, 3)
                const limit = function(s, t) {
                    const e = edge(clamp(s, run.from, run.to))
                    return clamp(t, -e.inner * keep, e.outer * keep)
                }
                const bolt = function(a, b, roughness, salt) {
                    let points = [a, b]
                    let rough = roughness
                    for (let level = 0; level < levels; ++level) {
                        const next = [points[0]]
                        for (let i = 0; i + 1 < points.length; ++i) {
                            const p = points[i], q = points[i + 1]
                            const s = (p[0] + q[0]) / 2
                            next.push([s, limit(s, (p[1] + q[1]) / 2 + (hash(salt * 7919 + level * 613 + i, seed) - 0.5) * 2 * rough)], q)
                        }
                        points = next
                        rough *= 0.56
                    }
                    return points
                }
                for (let b = 0; b < bolts; ++b) {
                    const main = bolt([from, offset], [to, offset], amp, b + 1)
                    result.push({ kind: line.kind, closed: false, points: main, width: width * (b === 0 ? 1 : 0.65), colour: colour, cap: "round" })
                    const forks = Math.max(0, Math.round(finite(line.forks, 2)))
                    for (let f = 0; f < forks; ++f) {
                        const start = main[Math.floor((0.15 + 0.7 * hash(b * 13 + f, seed + 5)) * (main.length - 1))]
                        const reachS = (hash(b * 17 + f, seed + 6) - 0.5) * 6 * halfWidth
                        const end = [start[0] + reachS, limit(start[0] + reachS, start[1] + (hash(b * 19 + f, seed + 7) < 0.5 ? 1 : -1) * (0.5 + 0.4 * hash(f, seed + 8)))]
                        result.push({ kind: line.kind, closed: false, points: bolt(start, end, amp * 0.5, b * 100 + f + 50),
                                      width: width * 0.45, colour: colour, cap: "round" })
                    }
                }
            } else if (line.kind === "cracks") {
                // Cracks in stone: jagged lines running in from an edge,
                // turning a little at every step, some forking.
                const count = Math.max(1, Math.round(finite(line.count, 5)))
                const reach = finite(line.length, 1.6)
                const seed = finite(line.seed, 43)
                const walk = function(s, t, heading, span, salt) {
                    const steps = 9
                    const points = [[s, t]]
                    for (let i = 1; i <= steps; ++i) {
                        heading += (hash(salt * 31 + i, seed + 3) - 0.5) * 1.1
                        s += Math.cos(heading) * span / steps * halfWidth
                        t += Math.sin(heading) * span / steps
                        const here = edge(clamp(s, run.from, run.to))
                        t = clamp(t, -here.inner, here.outer)
                        points.push([s, t])
                    }
                    return points
                }
                for (let k = 0; k < count; ++k) {
                    const s0 = run.from + (0.08 + 0.84 * hash(k, seed)) * (run.to - run.from)
                    const fromOuter = hash(k, seed + 1) < 0.5
                    const e = edge(s0)
                    const heading = (fromOuter ? -1 : 1) * Math.PI / 2 + (hash(k, seed + 2) - 0.5) * 1.2
                    const points = walk(s0, fromOuter ? e.outer : -e.inner, heading, reach * (0.6 + 0.6 * hash(k, seed + 6)), k + 1)
                    result.push({ kind: line.kind, closed: false, points: points, width: width * (0.6 + 0.6 * hash(k, seed + 4)), colour: colour, taper: true })
                    if (hash(k, seed + 5) < 0.6) {
                        const middle = points[Math.floor(points.length / 2)]
                        result.push({ kind: line.kind, closed: false, taper: true, colour: colour, width: width * 0.45,
                                      points: walk(middle[0], middle[1], heading + (hash(k, seed + 7) < 0.5 ? 0.9 : -0.9), reach * 0.45, k + 101) })
                    }
                }
            } else if (line.kind === "panels") {
                // A hull's plating: rows of panels of uneven lengths across
                // the band, each a slightly different shade (an overlay
                // colour from `shades`), with dark seams between them.
                const rows = Math.max(1, Math.round(finite(line.rows, 3)))
                const lengths = line.lengths || [0.8, 2.6]
                const shades = line.shades && line.shades.length ? line.shades : ["#ffffff14", "#00000014", "#00000000"]
                const seam = line.seam || "#00000055"
                const seed = finite(line.seed, 47)
                const margin = run.open ? finite(line.margin, 0) * halfWidth : 0
                let index = 0
                for (let row = 0; row < rows; ++row) {
                    const a = row / rows, c = (row + 1) / rows
                    let s = run.from + margin - hash(row, seed) * finite(lengths[1], 2.6) * halfWidth * 0.6
                    while (s < run.to - margin && index < 6000) {
                        const span = (finite(lengths[0], 0.8) + (finite(lengths[1], 2.6) - finite(lengths[0], 0.8)) * hash(index, seed + 1)) * halfWidth
                        const s0 = Math.max(run.from + margin, s), s1 = Math.min(run.to - margin, s + span)
                        if (s1 - s0 > 1) {
                            const steps = Math.max(1, Math.ceil((s1 - s0) / Math.max(1, step * 2)))
                            const top = [], bottom = []
                            for (let k = 0; k <= steps; ++k) {
                                const sk = s0 + (s1 - s0) * k / steps
                                const e = edge(sk), thickness = e.outer + e.inner
                                top.push([sk, -e.inner + thickness * c])
                                bottom.push([sk, -e.inner + thickness * a])
                            }
                            const ring = top.concat(bottom.reverse())
                            result.push({ kind: line.kind, fill: true, closed: true, width: 0, points: ring,
                                          colour: shades[Math.floor(hash(index, seed + 2) * shades.length) % shades.length] })
                            result.push({ kind: line.kind, closed: true, points: ring, width: finite(line.width, 0.025), colour: seam, cap: "butt", flat: true })
                        }
                        s += span
                        ++index
                    }
                }
            } else if (line.kind === "twigs" && run.open) {
                // A racing broom's tail: twigs bound tight at the collar,
                // swelling, then drawn into a point, each twig its own lane
                // and length, a few strays standing out.
                const atStart = line.at === "start"
                if (!(atStart ? run.startCut : run.endCut)) {
                    const count = Math.max(4, Math.round(finite(line.count, 90)))
                    const reach = finite(line.length, 6) * halfWidth
                    const root = finite(line.root, 0.7), bulge = finite(line.bulge, 1.15), tip = finite(line.tip, 0.25)
                    const seed = finite(line.seed, 53)
                    const s0 = atStart ? run.from - finite(line.inset, 0) * halfWidth : run.to + finite(line.inset, 0) * halfWidth
                    const dir = atStart ? -1 : 1
                    const colours = line.colours && line.colours.length ? line.colours : [colour]
                    const smooth = function(x) { x = clamp(x, 0, 1); return x * x * (3 - 2 * x) }
                    const radius = function(u) { return u < 0.32 ? root + (bulge - root) * smooth(u / 0.32) : bulge + (tip - bulge) * smooth((u - 0.32) / 0.68) }
                    for (let k = 0; k < count; ++k) {
                        const lane = (hash(k, seed) * 2 - 1) * (0.55 + 0.45 * Math.sqrt(hash(k, seed + 1)))
                        const stray = hash(k, seed + 2) < 0.06
                        const span = (0.72 + 0.3 * hash(k, seed + 3)) * (stray ? 1.08 : 1)
                        const wobble = (hash(k, seed + 4) - 0.5) * 0.12
                        const points = []
                        for (let i = 0; i <= 8; ++i) {
                            const u = i / 8 * span
                            const spread = stray ? 1 + 0.5 * u : 1
                            points.push([s0 + dir * u * reach, finite(line.offset, 0) + lane * radius(Math.min(1, u)) * spread + wobble * Math.sin(u * 3)])
                        }
                        result.push({ kind: line.kind, closed: false, points: points, cap: "round",
                                      width: width * (0.6 + 0.8 * hash(k, seed + 5)) * (stray ? 0.8 : 1),
                                      colour: colours[Math.floor(hash(k, seed + 6) * colours.length) % colours.length], taper: true })
                    }
                }
            } else if (line.kind === "footprints") {
                // Footprints walking along the band, left and right in turn:
                // a sole and a heel, inked, kept off the icons.
                const pitch = Math.max(0.2, finite(line.period, 0.7)) * halfWidth
                const size = finite(line.size, 0.34)
                const margin = run.open ? finite(line.margin, 0.8) * halfWidth : 0
                let index = 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 1.0) * halfWidth, length, closed)) {
                    for (let s = interval[0] + pitch / 2; s < interval[1]; s += pitch, ++index) {
                        const side = index % 2 === 0 ? 1 : -1
                        const t = finite(line.offset, 0) + side * size * 0.38
                        // A walk sets the prints down one after another.
                        const strength = motion && motion.walk !== undefined
                            ? footfall(motion.walk / (2 * Math.PI) - index / finite(motion.strides, 6)) : undefined
                        const oval = function(cs, ct, rs, rt) {
                            const ring = []
                            for (let k = 0; k < 14; ++k) {
                                const a = k / 14 * 2 * Math.PI
                                ring.push([cs + Math.cos(a) * rs * halfWidth, ct + Math.sin(a) * rt])
                            }
                            return ring
                        }
                        result.push({ kind: line.kind, fill: true, closed: true, width: 0, colour: colour, step: index,
                                      opacity: strength, points: oval(s + size * 0.16 * halfWidth, t, size * 0.32, size * 0.17) })
                        result.push({ kind: line.kind, fill: true, closed: true, width: 0, colour: colour, step: index,
                                      opacity: strength, points: oval(s - size * 0.3 * halfWidth, t, size * 0.13, size * 0.13) })
                        const every = Math.round(finite(line.labels, 0))
                        if (every > 0 && index % every === 0) {
                            // a small ribbon banner with forked ends, two
                            // strokes of writing in it, above the steps
                            const bs = s + size * 0.6 * halfWidth, bt = finite(line.offset, 0) + size * 1.25
                            const bw = size * 1.5 * halfWidth, bh = size * 0.42, fork = size * 0.18 * halfWidth
                            result.push({ kind: line.kind, closed: true, width: finite(line.width, 0.025), colour: colour, cap: "butt",
                                          points: [[bs - bw / 2, bt - bh / 2], [bs + bw / 2, bt - bh / 2], [bs + bw / 2 - fork, bt], [bs + bw / 2, bt + bh / 2],
                                                   [bs - bw / 2, bt + bh / 2], [bs - bw / 2 + fork, bt]] })
                            for (const row of [-0.1, 0.1]) {
                                const pts = []
                                for (let k = 0; k <= 10; ++k) {
                                    const u = k / 10
                                    pts.push([bs - bw * 0.3 + bw * 0.6 * u, bt + row * size * 1.1 + 0.025 * Math.sin(u * 19 + index)])
                                }
                                result.push({ kind: line.kind, closed: false, width: finite(line.width, 0.025) * 0.8, colour: colour, cap: "round", points: pts })
                            }
                        }
                    }
                }
            } else if (line.kind === "fringe") {
                // A banner's fringe: short threads hanging past an edge, a
                // cord along the edge they hang from.
                const outerSide = line.side === "outer"
                const pitch = Math.max(0.03, finite(line.period, 0.09)) * halfWidth
                const hang = finite(line.length, 0.45)
                const seed = finite(line.seed, 59)
                const margin = run.open ? finite(line.margin, 0.2) * halfWidth : 0
                const sign = outerSide ? 1 : -1
                let k = 0
                for (let s = run.from + margin; s <= run.to - margin; s += pitch, ++k) {
                    const e = edge(s), base = sign * (outerSide ? e.outer : e.inner)
                    const drop = hang * (0.85 + 0.3 * hash(k, seed))
                    const sway = (hash(k, seed + 1) - 0.5) * 0.08 * halfWidth
                    result.push({ kind: line.kind, closed: false, cap: "round", width: width, colour: colour,
                                  points: [[s, base], [s + sway * 0.5, base + sign * drop * 0.5], [s + sway, base + sign * drop]] })
                }
                const cord = runSamples(run, step).filter(function(s) { return s >= run.from + margin && s <= run.to - margin })
                    .map(function(s) { const e = edge(s); return [s, sign * (outerSide ? e.outer : e.inner)] })
                result.push({ kind: line.kind, closed: false, cap: "round", width: width * 2.6, colour: line.cord || colour, points: cord })
            } else if (line.kind === "plan") {
                // A wizard's map in ink: rooms drawn as rectangles between
                // the icons and corridors running along, joining them.
                const seed = finite(line.seed, 61)
                const margin = run.open ? finite(line.margin, 0.6) * halfWidth : 0
                let index = 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 0.95) * halfWidth, length, closed)) {
                    const room = interval[1] - interval[0]
                    if (room < halfWidth * 0.6) continue
                    const w = room * (0.45 + 0.35 * hash(index, seed)), h = 0.5 + 0.5 * hash(index, seed + 1)
                    const cs = interval[0] + room * (0.25 + 0.5 * hash(index, seed + 2)), ct = (hash(index, seed + 3) - 0.5) * 0.6
                    result.push({ kind: line.kind, closed: true, width: width, colour: colour, cap: "butt",
                                  points: [[cs - w / 2, ct - h / 2], [cs + w / 2, ct - h / 2], [cs + w / 2, ct + h / 2], [cs - w / 2, ct + h / 2]] })
                    if (hash(index, seed + 4) < 0.6)
                        result.push({ kind: line.kind, closed: false, width: width, colour: colour, cap: "butt",
                                      points: [[cs, ct + h / 2], [cs, 0.82]] })
                    ++index
                }
                for (const offset of [-0.62, 0.62]) {
                    const points = runSamples(run, step * 3).filter(function(s) { return s >= run.from + margin && s <= run.to - margin })
                        .map(function(s) { return [s, offset] })
                    result.push({ kind: line.kind, closed: false, width: width * 0.8, colour: colour, cap: "butt", dash: line.dash, points: points })
                }
            } else if (line.kind === "greebles") {
                // A film model's hull detail ("guts on the outside"): rows of
                // small raised blocks of mixed sizes, each lit on its top left
                // edge and shaded on its bottom right, some with vent slots,
                // long pipes between them.
                const rows = Math.max(1, Math.round(finite(line.rows, 3)))
                const across0 = clamp(finite((line.span || [0, 1])[0], 0), 0, 1), across1 = clamp(finite((line.span || [0, 1])[1], 1), 0, 1)
                const sizes = line.sizes || [0.25, 1.1]
                const shades = line.shades && line.shades.length ? line.shades : ["#ffffff10", "#00000018", "#00000000", "#ffffff08"]
                const seed = finite(line.seed, 67)
                const margin = run.open ? finite(line.margin, 0.4) * halfWidth : 0
                const gap = finite(line.gap, 0.05) * halfWidth
                let index = 0
                for (let row = 0; row < rows; ++row) {
                    const a = across0 + (across1 - across0) * row / rows, c = across0 + (across1 - across0) * (row + 1) / rows
                    for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 0) * halfWidth, length, closed)) {
                        let s = interval[0]
                        while (s < interval[1] - gap && index < 8000) {
                            const pipe = hash(index, seed + 7) < 0.12
                            const span = Math.min(interval[1] - s, (pipe ? 2.2 : finite(sizes[0], 0.25) + (finite(sizes[1], 1.1) - finite(sizes[0], 0.25)) * hash(index, seed)) * halfWidth)
                            const e = edge(s + span / 2), thickness = e.outer + e.inner
                            const top = -e.inner + thickness * c, bottom = -e.inner + thickness * a
                            const inset = thickness * (across1 - across0) / rows * (pipe ? 0.32 : 0.08 + 0.12 * hash(index, seed + 1))
                            const t0 = bottom + inset, t1 = top - inset
                            const s0 = s + gap / 2, s1 = s + span - gap / 2
                            if (s1 - s0 > 1 && t1 - t0 > 0.02) {
                                const edgeW = Math.min(0.06, (t1 - t0) * 0.3), edgeS = Math.min(edgeW * halfWidth, (s1 - s0) * 0.3)
                                result.push({ kind: line.kind, fill: true, closed: true, width: 0,
                                              colour: shades[Math.floor(hash(index, seed + 2) * shades.length) % shades.length],
                                              points: [[s0, t0], [s1, t0], [s1, t1], [s0, t1]] })
                                // lit top and left, shaded bottom and right (screen up is outward for an edge panel's outer side)
                                result.push({ kind: line.kind, fill: true, closed: true, width: 0, colour: "#ffffff38", points: [[s0, t1], [s1, t1], [s1 - edgeS, t1 - edgeW], [s0 + edgeS, t1 - edgeW]] })
                                result.push({ kind: line.kind, fill: true, closed: true, width: 0, colour: "#ffffff22", points: [[s0, t0], [s0, t1], [s0 + edgeS, t1 - edgeW], [s0 + edgeS, t0 + edgeW]] })
                                result.push({ kind: line.kind, fill: true, closed: true, width: 0, colour: "#0000005a", points: [[s0, t0], [s1, t0], [s1 - edgeS, t0 + edgeW], [s0 + edgeS, t0 + edgeW]] })
                                result.push({ kind: line.kind, fill: true, closed: true, width: 0, colour: "#00000040", points: [[s1, t0], [s1, t1], [s1 - edgeS, t1 - edgeW], [s1 - edgeS, t0 + edgeW]] })
                                if (!pipe && hash(index, seed + 3) < 0.25 && s1 - s0 > halfWidth * 0.4) {
                                    // vent slots
                                    const slots2 = 3 + Math.floor(hash(index, seed + 4) * 4)
                                    for (let v = 0; v < slots2; ++v) {
                                        const u = s0 + (s1 - s0) * (v + 1) / (slots2 + 1)
                                        result.push({ kind: line.kind, closed: false, width: Math.max(0.6, halfWidth * 0.025) / halfWidth, colour: "#0000007a", cap: "butt",
                                                      points: [[u, t0 + (t1 - t0) * 0.25], [u, t0 + (t1 - t0) * 0.75]] })
                                    }
                                }
                            }
                            s += span
                            ++index
                        }
                    }
                }
            } else if (line.kind === "ticks") {
                // A ruler's ticks along an edge: short ones every `period`,
                // a long one every `major`, kept off the icons.
                const pitch = Math.max(0.05, finite(line.period, 0.18)) * halfWidth
                const major = Math.max(1, Math.round(finite(line.major, 5)))
                const short = finite(line.length, 0.12), long = finite(line.long, 0.26)
                const outerSide = line.side !== "inner"
                const margin = run.open ? finite(line.margin, 0.5) * halfWidth : 0
                let k = 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 0) * halfWidth, length, closed))
                    for (let s = interval[0]; s <= interval[1]; s += pitch, ++k) {
                        const e = edge(s)
                        const base = outerSide ? e.outer - inset : -(e.inner - inset)
                        const reach = (k % major === 0 ? long : short) * (outerSide ? -1 : 1)
                        result.push({ kind: line.kind, closed: false, width: width, colour: colour, cap: "butt", points: [[s, base], [s, base + reach]] })
                    }
            } else if (line.kind === "dotgrid") {
                // A faint grid of dots behind everything, the ground screen
                // designs are laid on.
                const pitch = Math.max(0.08, finite(line.period, 0.3))
                let reachAcross = 0
                for (const s of runSamples(run, step * 4)) { const e = edge(s); reachAcross = Math.max(reachAcross, e.outer, e.inner) }
                const points = []
                for (let t = -Math.floor(reachAcross / pitch) * pitch; t <= reachAcross; t += pitch)
                    for (let s = run.from + pitch * halfWidth / 2; s < run.to; s += pitch * halfWidth) {
                        const e = edge(s)
                        if (t < e.outer - pitch * 0.5 && t > -e.inner + pitch * 0.5) points.push([s, t])
                    }
                result.push({ kind: line.kind, closed: false, dots: true, plain: true, width: width, colour: colour, points: points })
            } else if (line.kind === "widgets") {
                // A film console's screen between the icons: each gap holds
                // a framed module of meter columns, a ring gauge, a grid of
                // buttons, bar graphs or a trace, drawn in thin light.
                const palette = line.colours && line.colours.length ? line.colours : [colour, "#ffffff"]
                const accent = line.accent || palette[0]
                const seed = finite(line.seed, 71)
                const margin = run.open ? finite(line.margin, 0.5) * halfWidth : 0
                const thin = width, frameT = finite(line.height, 0.7)
                let index = 0
                const push = function(points, closed, w, c, fill, extra) {
                    result.push(Object.assign({ kind: line.kind, closed: closed, points: points, width: fill ? 0 : w, colour: c, fill: fill === true, cap: "butt" }, extra || {}))
                }
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 0.92) * halfWidth, length, closed)) {
                    const room = interval[1] - interval[0]
                    if (room < halfWidth * 0.5) continue
                    // one or two modules side by side in the gap
                    const pieces = room > halfWidth * 2.2 && hash(index, seed + 9) < 0.6 ? 2 : 1
                    for (let piece = 0; piece < pieces; ++piece, ++index) {
                        const pad = halfWidth * 0.08
                        const a0 = interval[0] + room * piece / pieces + pad, a1 = interval[0] + room * (piece + 1) / pieces - pad
                        const t0 = -frameT, t1 = frameT
                        // the module's frame: thin, its corners bracketed
                        push([[a0, t0], [a1, t0], [a1, t1], [a0, t1]], true, thin * 0.6, palette[0] + "", false, { opacity: 0.45 })
                        const br = Math.min(0.18 * halfWidth, (a1 - a0) * 0.2), bt = 0.18
                        push([[a0, t1 - bt], [a0, t1], [a0 + br, t1]], false, thin * 1.4, palette[0])
                        push([[a1 - br, t0], [a1, t0], [a1, t0 + bt]], false, thin * 1.4, palette[0])
                        const kind = Math.floor(hash(index, seed) * 5)
                        const ins = 0.12
                        const x0 = a0 + ins * halfWidth, x1 = a1 - ins * halfWidth, y0 = t0 + ins, y1 = t1 - ins
                        if (kind === 0) {
                            // meter columns, their levels set by a hash
                            const bars = Math.max(3, Math.floor((x1 - x0) / (halfWidth * 0.11)))
                            for (let k = 0; k < bars; ++k) {
                                const u0 = x0 + (x1 - x0) * k / bars, u1 = u0 + (x1 - x0) / bars * 0.62
                                const level = 0.15 + 0.85 * hash(index * 31 + k, seed + 1)
                                push([[u0, y0], [u1, y0], [u1, y0 + (y1 - y0) * level], [u0, y0 + (y1 - y0) * level]], true, 0, k % 4 === 0 ? accent : palette[k % palette.length], true, { glow: true, meter: k })
                            }
                        } else if (kind === 1) {
                            // a ring gauge: a thin circle, a bright arc, ticks
                            const cs = (x0 + x1) / 2, ct = (y0 + y1) / 2
                            const r = Math.min((y1 - y0) / 2, (x1 - x0) / 2 / halfWidth) * 0.92
                            const circle = function(from, to, rr) {
                                const pts = []
                                for (let k = 0; k <= 40; ++k) { const ang = from + (to - from) * k / 40; pts.push([cs + Math.cos(ang) * rr * halfWidth, ct + Math.sin(ang) * rr]) }
                                return pts
                            }
                            push(circle(0, 2 * Math.PI, r), false, thin * 0.7, palette[0], false, { opacity: 0.6 })
                            const sweep = 0.4 + 1.4 * hash(index, seed + 2)
                            push(circle(Math.PI / 2, Math.PI / 2 + sweep * Math.PI, r * 0.78), false, thin * 2.2, accent, false, { glow: true })
                            for (let k = 0; k < 24; ++k) {
                                const ang = k / 24 * 2 * Math.PI
                                push([[cs + Math.cos(ang) * r * 0.88 * halfWidth, ct + Math.sin(ang) * r * 0.88], [cs + Math.cos(ang) * r * halfWidth, ct + Math.sin(ang) * r]], false, thin * 0.6, palette[0])
                            }
                            push(circle(0, 2 * Math.PI, r * 0.12), true, 0, "#ffffff", true, { glow: true })
                        } else if (kind === 2) {
                            // a grid of small buttons, some lit
                            const cols = Math.max(2, Math.floor((x1 - x0) / (halfWidth * 0.3))), rowsN = 3
                            for (let r = 0; r < rowsN; ++r)
                                for (let k = 0; k < cols; ++k) {
                                    const u0 = x0 + (x1 - x0) * k / cols, u1 = u0 + (x1 - x0) / cols * 0.72
                                    const v0 = y0 + (y1 - y0) * r / rowsN, v1 = v0 + (y1 - y0) / rowsN * 0.68
                                    const lit = hash(index * 97 + r * 13 + k, seed + 3) < 0.45
                                    push([[u0, v0], [u1, v0], [u1, v1], [u0, v1]], true, thin * 0.6, lit ? palette[(r + k) % palette.length] : palette[0], lit, lit ? { glow: true, opacity: 0.85 } : { opacity: 0.5 })
                                }
                        } else if (kind === 3) {
                            // bar graphs side to side
                            const bars = 4 + Math.floor(hash(index, seed + 4) * 3)
                            for (let k = 0; k < bars; ++k) {
                                const v0 = y0 + (y1 - y0) * k / bars, v1 = v0 + (y1 - y0) / bars * 0.55
                                const reach = 0.2 + 0.8 * hash(index * 17 + k, seed + 5)
                                push([[x0, v0], [x0 + (x1 - x0) * reach, v0], [x0 + (x1 - x0) * reach, v1], [x0, v1]], true, 0, k === 0 ? accent : palette[k % palette.length], true, { glow: true, opacity: 0.9 })
                            }
                        } else {
                            // a trace: a waveform over a faint baseline
                            const pts = []
                            const f1 = 2 + 4 * hash(index, seed + 6), f2 = 7 + 9 * hash(index, seed + 7)
                            for (let k = 0; k <= 40; ++k) {
                                const u = k / 40
                                pts.push([x0 + (x1 - x0) * u, (y0 + y1) / 2 + (y1 - y0) * 0.42 * (0.6 * Math.sin(u * f1 * Math.PI) * Math.sin(u * 3.1) + 0.4 * Math.sin(u * f2 * Math.PI))])
                            }
                            push([[x0, (y0 + y1) / 2], [x1, (y0 + y1) / 2]], false, thin * 0.5, palette[0], false, { opacity: 0.4 })
                            push(pts, false, thin * 1.3, accent, false, { glow: true })
                        }
                    }
                }
            } else if (line.kind === "plates") {
                // Angular plating laid like a bird's feathers (a raptor-like
                // warship's hull): rows of pointed plates overlapping toward
                // the band's end, each a little lighter or darker, a light
                // rim on its lit edge and a dark seam on the other.
                const rows = Math.max(1, Math.round(finite(line.rows, 3)))
                const pitch = Math.max(0.2, finite(line.period, 0.9)) * halfWidth
                const shades = line.shades && line.shades.length ? line.shades : ["#ffffff12", "#00000020", "#00000008", "#ffffff06"]
                const seed = finite(line.seed, 73)
                const margin = run.open ? finite(line.margin, 0.3) * halfWidth : 0
                let index = 0
                for (let row = 0; row < rows; ++row) {
                    const a = row / rows, c = (row + 1) / rows
                    const shift = (row % 2) * pitch / 2
                    for (let s = run.from + margin - shift; s < run.to - margin; s += pitch, ++index) {
                        const s0 = Math.max(run.from + margin, s), tipS = Math.min(run.to - margin, s + pitch * 1.35)
                        const e = edge(Math.min(run.to, s0 + pitch / 2)), thickness = e.outer + e.inner
                        const top = -e.inner + thickness * c, bottom = -e.inner + thickness * a, mid = (top + bottom) / 2
                        const body = [[s0, bottom], [s0 + (tipS - s0) * 0.72, bottom + (top - bottom) * 0.08], [tipS, mid], [s0 + (tipS - s0) * 0.72, top - (top - bottom) * 0.08], [s0, top]]
                        result.push({ kind: line.kind, fill: true, closed: true, width: 0, colour: shades[Math.floor(hash(index, seed) * shades.length) % shades.length], points: body })
                        result.push({ kind: line.kind, closed: false, width: finite(line.width, 0.03), colour: "#ffffff30", cap: "butt", points: [body[4], body[3], body[2]] })
                        result.push({ kind: line.kind, closed: false, width: finite(line.width, 0.03), colour: "#00000070", cap: "butt", points: [body[0], body[1], body[2]] })
                    }
                }
            } else if (line.kind === "pipes") {
                // Pipes along the hull: `count` of them side by side round
                // `offset`, each `width` thick, held by clamps every
                // `period`, broken round the icons when `avoid` says.
                const count = Math.max(1, Math.round(finite(line.count, 1)))
                const pipeW = finite(line.width, 0.1), gapT = finite(line.gap, 0.05)
                const total = count * pipeW + (count - 1) * gapT
                const offset = finite(line.offset, 0)
                const clampPitch = Math.max(0.3, finite(line.period, 1.6)) * halfWidth
                const margin = run.open ? finite(line.margin, 0.6) * halfWidth : 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 0) * halfWidth, length, closed)) {
                    if (interval[1] - interval[0] < halfWidth * 0.4) continue
                    for (let k = 0; k < count; ++k) {
                        const t = offset - total / 2 + pipeW / 2 + k * (pipeW + gapT)
                        const points = runSamples({ from: interval[0], to: interval[1] }, step).map(function(s) { return [s, t] })
                        result.push({ kind: line.kind, closed: false, points: points, width: pipeW, colour: colour, cap: "round",
                                      pipe: true, material: line.material || null })
                    }
                    const cw = finite(line.clampWidth, 0.12) * halfWidth
                    for (let s = interval[0] + clampPitch / 2; s < interval[1] - cw; s += clampPitch)
                        result.push({ kind: line.kind, fill: true, closed: true, width: 0, raised: true, colour: line.clamp || "#3c4046",
                                      material: line.clampMaterial || null,
                                      points: [[s - cw / 2, offset - total / 2 - 0.04], [s + cw / 2, offset - total / 2 - 0.04],
                                               [s + cw / 2, offset + total / 2 + 0.04], [s - cw / 2, offset + total / 2 + 0.04]] })
                }
            } else if (line.kind === "grilles") {
                // Vent grilles between the icons: an opening cut into the
                // hull with louvres across it, a light glowing behind them
                // when `light` names a colour.
                const h = finite(line.height, 0.5), louvres = Math.max(2, Math.round(finite(line.count, 7)))
                const share = clamp(finite(line.share, 0.62), 0.1, 1)
                const ct = finite(line.offset, 0)
                const margin = run.open ? finite(line.margin, 0.6) * halfWidth : 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 0.95) * halfWidth, length, closed)) {
                    const room = interval[1] - interval[0]
                    if (room < halfWidth * 0.6) continue
                    const w = room * share, cs = (interval[0] + interval[1]) / 2
                    const box = function(inset) {
                        return [[cs - w / 2 + inset * halfWidth, ct - h / 2 + inset], [cs + w / 2 - inset * halfWidth, ct - h / 2 + inset],
                                [cs + w / 2 - inset * halfWidth, ct + h / 2 - inset], [cs - w / 2 + inset * halfWidth, ct + h / 2 - inset]]
                    }
                    result.push({ kind: line.kind, fill: true, closed: true, width: 0, recess: true, colour: line.colour || "#141619", points: box(0) })
                    if (line.light)
                        result.push({ kind: line.kind, fill: true, closed: true, width: 0, glow: true, colour: line.light, points: box(0.05) })
                    const pitch = (w - 0.1 * halfWidth) / louvres
                    for (let k = 0; k < louvres; ++k) {
                        const s0 = cs - w / 2 + 0.05 * halfWidth + k * pitch + pitch * 0.18, s1 = s0 + pitch * 0.5
                        result.push({ kind: line.kind, fill: true, closed: true, width: 0, raised: true, colour: line.louvre || "#4a4f57",
                                      material: line.material || null,
                                      points: [[s0, ct - h / 2 + 0.03], [s1, ct - h / 2 + 0.03], [s1, ct + h / 2 - 0.03], [s0, ct + h / 2 - 0.03]] })
                    }
                }
            } else if (line.kind === "fins") {
                // A heat sink: thin raised fins across a stretch of the band
                // (`span`, shares of its thickness), every `period`.
                const pitch = Math.max(0.05, finite(line.period, 0.16)) * halfWidth, finW = finite(line.width, 0.06) * halfWidth
                const span = line.span || [0.2, 0.8]
                const margin = run.open ? finite(line.margin, 0.6) * halfWidth : 0
                for (const interval of freeIntervals(run, margin, slots, avoidOf(line, 0.95) * halfWidth, length, closed)) {
                    for (let s = interval[0] + pitch / 2; s < interval[1] - finW; s += pitch) {
                        const e = edge(s), thickness = e.outer + e.inner
                        const t0 = -e.inner + thickness * finite(span[0], 0.2), t1 = -e.inner + thickness * finite(span[1], 0.8)
                        result.push({ kind: line.kind, fill: true, closed: true, width: 0, raised: true, colour: colour, material: line.material || null,
                                      points: [[s - finW / 2, t0], [s + finW / 2, t0], [s + finW / 2, t1], [s - finW / 2, t1]] })
                    }
                }
            } else if (line.kind === "hexgrid") {
                // A shield's hexagon cells over the band, kept inside it by
                // the renderer.
                const r = Math.max(0.08, finite(line.size, 0.35))
                const dx = Math.sqrt(3) * r * halfWidth, dy = 1.5 * r
                let reachAcross = 0
                for (const s of runSamples(run, step * 4)) { const e = edge(s); reachAcross = Math.max(reachAcross, e.outer, e.inner) }
                const rows = Math.ceil(reachAcross / dy) + 1
                for (let row = -rows; row <= rows; ++row) {
                    const ct = row * dy
                    const shift = (Math.abs(row) % 2) * dx / 2
                    for (let cs = run.from - dx + shift; cs <= run.to + dx; cs += dx) {
                        const ring = []
                        for (let k = 0; k < 6; ++k) {
                            const a = Math.PI / 6 + k * Math.PI / 3
                            ring.push([cs + Math.cos(a) * r * halfWidth, ct + Math.sin(a) * r])
                        }
                        result.push({ kind: line.kind, closed: true, points: ring, width: width, colour: colour, clip: true })
                    }
                }
            }
        }
        // Whether a line is a round wire, cut in, or a bead that is not
        // metal, carried to every stroke it made; what a plain line is made
        // of, too (the machinery kinds give their own parts their own
        // materials).
        const plain = ["edge", "middle", "loop", "wave", "spiral", "brackets", "ends", "scrolls", "slats"]
        for (let index = before; index < result.length; ++index) {
            const made = result[index]
            if (made.material === undefined && line.material && plain.indexOf(line.kind) >= 0) made.material = line.material
            if (made.pipe === undefined && line.pipe) made.pipe = true
            if (made.engrave === undefined && line.engrave) made.engrave = true
            if (made.metal === undefined && line.metal !== undefined) made.metal = finite(line.metal, 0.8)
            if (made.gleam === undefined && line.gleam !== undefined) made.gleam = clamp(finite(line.gleam, 1), 0.5, 3)
        }
    }
    return result
}

// A colour a part or a strip names: one of the decoration's own colours by
// its name, or a colour as it is.
function colourOf(decoration, value, fallback) {
    if (value === undefined || value === null || value === "") return fallback
    const key = String(value)
    return decoration.colours[key] !== undefined ? decoration.colours[key] : key
}

// The places a part stands along the band: [{ s, dir, index }], `dir` the
// way its own x runs (back from the end for a part at the end, so a pair at
// the two ends mirror each other).
function partAnchors(part, runs, length, closed, slots, halfWidth) {
    const at = String(part.at || "middle")
    const first = runs.length ? runs[0].from : 0
    const last = runs.length ? runs[runs.length - 1].to : length
    let list = []
    if (at === "middle") {
        list.push({ s: closed ? finite(part.phase, 0) * length : (first + last) / 2, dir: 1 })
    } else if (at === "start" || at === "end" || at === "both") {
        if (!closed && at !== "end") list.push({ s: first, dir: 1 })
        if (!closed && at !== "start") list.push({ s: last, dir: -1 })
    } else if (at === "at") {
        list = (part.where || [0.5]).map(function(f) { return { s: clamp(finite(f, 0.5), 0, 1) * length, dir: 1 } })
    } else if (at === "icons" || at === "between") {
        list = slotPlaces(slots, length, closed, at === "between").map(function(s) { return { s: s, dir: 1 } })
    } else if (at === "period") {
        // Centred along the band the way an edge feature of the same period
        // is, so a part can stand on every merlon or between them.
        list = featureCentres({ anchor: "period", period: Math.max(0.5, finite(part.period, 4)), phase: finite(part.phase, 0) },
                              length, closed, slots, halfWidth).map(function(s) { return { s: s, dir: 1 } })
    }
    const every = Math.max(1, Math.round(finite(part.every, 1)))
    const skip = Math.max(0, Math.round(finite(part.skip, 0)))
    return list.map(function(anchor, index) { return Object.assign(anchor, { index: index }) })
               .filter(function(anchor) { return anchor.index >= skip && (anchor.index - skip) % every === 0 })
}

// An arc of an ellipse as [x, y] points.
function ellipseArc(cx, cy, rx, ry, from, to, count) {
    const points = []
    for (let k = 0; k <= count; ++k) {
        const a = from + (to - from) * k / count
        points.push([cx + rx * Math.cos(a), cy + ry * Math.sin(a)])
    }
    return points
}

// A limb growing from (x, y): heading `angle` degrees from straight out
// (+y) toward +x and turning by `bend` degrees along its `length`, its width
// tapering from `width` to `tip`, its root sinking a little into whatever it
// grows from. `forks` grow from it the same way: a branch's twigs, a
// trunk's roots, a dragon's spines.
function limbRings(spec, depth) {
    const length = Math.max(0.05, finite(spec.length, 2))
    const width = Math.max(0.01, finite(spec.width, 0.4)), tip = Math.max(0, finite(spec.tip, 0.05))
    const angle = finite(spec.angle, 30) * Math.PI / 180, bend = finite(spec.bend, 0) * Math.PI / 180
    const wobble = finite(spec.wobble, 0)
    const seed = finite(spec.seed, 1)
    const steps = 16
    const centre = [], headings = []
    let x = finite(spec.x, 0), y = finite(spec.y, 0)
    for (let k = 0; k <= steps; ++k) {
        const u = k / steps
        const heading = angle + bend * u + wobble * valueNoise(u * 3, 0, seed) * 0.35
        centre.push([x, y])
        headings.push(heading)
        x += Math.sin(heading) * length / steps
        y += Math.cos(heading) * length / steps
    }
    const left = [], right = []
    for (let k = 0; k <= steps; ++k) {
        const u = k / steps
        const half = (width + (tip - width) * Math.pow(u, 0.85)) / 2
        const nx = Math.cos(headings[k]), ny = -Math.sin(headings[k])
        left.push([centre[k][0] - nx * half, centre[k][1] - ny * half])
        right.push([centre[k][0] + nx * half, centre[k][1] + ny * half])
    }
    const h0 = headings[0], back = 0.12 + width * 0.3
    const root = function(point, side) {
        return [point[0] - Math.sin(h0) * back + side * Math.cos(h0) * width * 0.22,
                point[1] - Math.cos(h0) * back - side * Math.sin(h0) * width * 0.22]
    }
    let rings = [[root(left[0], -1)].concat(left, right.slice().reverse(), [root(right[0], 1)])]
    rings[0].spine = centre.map(function(p, k) { return [p[0], p[1], width + (tip - width) * Math.pow(k / steps, 0.85)] })
    if (depth > 0)
        for (const fork of spec.forks || []) {
            const k = Math.round(clamp(finite(fork.at, 0.6), 0.05, 0.95) * steps)
            const u = k / steps
            rings = rings.concat(limbRings(Object.assign({ seed: seed + 5, tip: tip }, fork, {
                x: centre[k][0], y: centre[k][1],
                angle: headings[k] * 180 / Math.PI + finite(fork.angle, 35),
                width: finite(fork.width, (width + (tip - width) * u) * 0.7),
                length: finite(fork.length, length * 0.45)
            }), depth - 1))
        }
    return rings
}

// A part's outline in its own frame (half-widths; x along the band, y away
// from the edge it stands on, or outward for a part placed across the
// band): one or more rings of [x, y].
function partRings(part) {
    const kind = String(part.kind)
    const w = Math.max(0.01, finite(part.w, 1)), h = Math.max(0.01, finite(part.h, 1))
    const onEdge = part.on === "outer" || part.on === "inner"
    const base = part.align ? part.align === "base" : onEdge
    const y0 = base ? 0 : -h / 2
    // A cut gem's girdle: its facets are drawn by the scene (DecorationScene);
    // a square or step-cut stone is a rectangle.
    if (kind === "gem" && (part.cut === "princess" || part.cut === "baguette"))
        return [[[-w / 2, y0], [w / 2, y0], [w / 2, y0 + h], [-w / 2, y0 + h]]]
    if (kind === "ellipse" || kind === "lens" || kind === "gem") return [ellipseArc(0, y0 + h / 2, w / 2, h / 2, 0, 2 * Math.PI, 48)]
    if (kind === "dome") return [ellipseArc(0, y0, w / 2, h, 0, Math.PI, 28)]
    if (kind === "ring") {
        const share = clamp(finite(part.hole, 0.6), 0.05, 0.95)
        return [ellipseArc(0, y0 + h / 2, w / 2, h / 2, 0, 2 * Math.PI, 40),
                ellipseArc(0, y0 + h / 2, w / 2 * share, h / 2 * share, 2 * Math.PI, 0, 40)]
    }
    if (kind === "arc") {
        // A stretch of a ring from angle `from` to `to` (degrees, 0 along
        // the band, 90 outward): a gauge's arc, a vortex's swirl.
        const share = clamp(finite(part.hole, 0.75), 0.05, 0.98)
        const a0 = finite(part.from, 0) * Math.PI / 180, a1 = finite(part.to, 90) * Math.PI / 180
        const count = Math.max(6, Math.ceil(Math.abs(a1 - a0) / (Math.PI / 24)))
        return [ellipseArc(0, y0 + h / 2, w / 2, h / 2, a0, a1, count)
                .concat(ellipseArc(0, y0 + h / 2, w / 2 * share, h / 2 * share, a1, a0, count))]
    }
    if (kind === "flame") {
        // A candle flame: round at its foot, rising to a point that leans a
        // little, as a still flame does.
        const ring = []
        for (let k = 0; k <= 32; ++k) {
            const u = k / 32, a = u * 2 * Math.PI
            const sin = Math.sin(a), cos = Math.cos(a)
            const rise = (1 - cos) / 2
            const x = w / 2 * sin * Math.pow(1 - rise, 0.35) * (1 - 0.25 * rise)
            ring.push([x + finite(part.lean, 0.06) * w * rise * rise, y0 + h * rise])
        }
        return [ring]
    }
    if (kind === "capsule") {
        const r = Math.min(w, h) / 2
        if (w >= h)
            return [ellipseArc(w / 2 - r, y0 + h / 2, r, r, -Math.PI / 2, Math.PI / 2, 12)
                    .concat(ellipseArc(-w / 2 + r, y0 + h / 2, r, r, Math.PI / 2, 3 * Math.PI / 2, 12))]
        return [ellipseArc(0, y0 + h - r, r, r, 0, Math.PI, 12).concat(ellipseArc(0, y0 + r, r, r, Math.PI, 2 * Math.PI, 12))]
    }
    if (kind === "box") {
        const x0 = -w / 2, x1 = w / 2, y1 = y0 + h
        const c = Math.min(Math.max(0, finite(part.chamfer, 0)), w / 2, h / 2)
        if (c > 0) return [[[x0 + c, y0], [x1 - c, y0], [x1, y0 + c], [x1, y1 - c], [x1 - c, y1], [x0 + c, y1], [x0, y1 - c], [x0, y0 + c]]]
        const r = Math.min(Math.max(0, finite(part.r, 0)), w / 2, h / 2)
        if (r <= 0) return [[[x0, y0], [x1, y0], [x1, y1], [x0, y1]]]
        return [ellipseArc(x1 - r, y0 + r, r, r, -Math.PI / 2, 0, 6)
                .concat(ellipseArc(x1 - r, y1 - r, r, r, 0, Math.PI / 2, 6),
                        ellipseArc(x0 + r, y1 - r, r, r, Math.PI / 2, Math.PI, 6),
                        ellipseArc(x0 + r, y0 + r, r, r, Math.PI, 3 * Math.PI / 2, 6))]
    }
    if (kind === "trapezoid") {
        const top = Math.max(0, finite(part.top, w / 2))
        return [[[-w / 2, y0], [w / 2, y0], [top / 2, y0 + h], [-top / 2, y0 + h]]]
    }
    if (kind === "crystal") {
        // A crystal's long hexagonal outline, pointed at both ends.
        return [[[0, y0], [w / 2, y0 + h * 0.22], [w / 2, y0 + h * 0.72], [0, y0 + h], [-w / 2, y0 + h * 0.72], [-w / 2, y0 + h * 0.22]]]
    }
    if (kind === "star") {
        const points = Math.max(3, Math.round(finite(part.points, 4)))
        const inner = clamp(finite(part.hole, 0.35), 0.05, 1)
        const ring = []
        for (let k = 0; k < points * 2; ++k) {
            const a = Math.PI / 2 + Math.PI * k / points
            const r = k % 2 === 0 ? 1 : inner
            ring.push([Math.cos(a) * w / 2 * r, y0 + h / 2 + Math.sin(a) * h / 2 * r])
        }
        return [ring]
    }
    if (kind === "polygon") return [(part.points || []).map(function(p) { return [finite(p[0], 0), finite(p[1], 0)] })]
    if (kind === "strut") {
        const a = part.from || [0, 0], b = part.to || [0, 1]
        const ax = finite(a[0], 0), ay = finite(a[1], 0), bx = finite(b[0], 0), by = finite(b[1], 1)
        const half = Math.max(0.005, finite(part.width, 0.2)) / 2
        const len = Math.hypot(bx - ax, by - ay) || 1
        const nx = -(by - ay) / len * half, ny = (bx - ax) / len * half
        return [[[ax + nx, ay + ny], [bx + nx, by + ny], [bx - nx, by - ny], [ax - nx, ay - ny]]]
    }
    if (kind === "branch") return limbRings(part, 2)
    return []
}

// The parts laid along the band in screen space.
function placedParts(decoration, runs, lay, length, closed, slots, halfWidth) {
    const result = []
    const motion = decoration.motion
    const spin = motion && motion.spin !== undefined ? motion.spin * 180 / Math.PI : 0
    // Floating things (candles in the air) rise and sink, each in its own
    // rhythm, instead of swinging.
    const floating = motion && motion.sway !== undefined && motion.swayMode === "float"
    for (let partIndex = 0; partIndex < decoration.parts.length; ++partIndex) {
        const part = decoration.parts[partIndex]
        for (const anchor of partAnchors(part, runs, length, closed, slots, halfWidth)) {
            // The edge a part stands on is measured where the part stands,
            // its own place along the band, not at the anchor (a band's
            // pointed tip has no edge to stand on).
            const own = anchor.s + anchor.dir * finite(part.s, 0) * halfWidth
            const sEdge = closed ? ((own % length) + length) % length : clamp(own, 0, length)
            let run = runs[0]
            for (const candidate of runs)
                if (sEdge >= candidate.from - 1e-6 && sEdge <= candidate.to + 1e-6) { run = candidate; break }
            const e = edgesAt(decoration, clamp(sEdge, run.from, run.to), run, length, closed, slots, halfWidth)
            let on = part.on
            if (part.alternate && anchor.index % 2 === 1) on = on === "outer" ? "inner" : on === "inner" ? "outer" : on
            const turn = part.kind === "branch" ? 0 : finite(part.angle, 0) * Math.PI / 180
            const cos = Math.cos(turn), sin = Math.sin(turn)
            const sOff = finite(part.s, 0)
            const tOff = finite(part.t, 0) + (floating ? motion.swayAmount * 2.5 * Math.sin(motion.sway + anchor.index * 1.7) : 0)
            const place = function(p) {
                let x = p[0], y = p[1]
                if (turn) {
                    const rx = x * cos - y * sin, ry = x * sin + y * cos
                    x = rx
                    y = ry
                }
                const s = anchor.s + anchor.dir * (sOff + x) * halfWidth
                const t = on === "outer" ? e.outer + tOff + y : on === "inner" ? -(e.inner + tOff + y) : tOff + y
                return lay.point(s, t * halfWidth)
            }
            const frame = lay.frame(sEdge)
            const onEdge = on === "outer" || on === "inner"
            const middleY = (part.align ? part.align === "base" : onEdge) ? finite(part.h, 1) / 2 : 0
            const gradient = part.gradient
            // A spinning gauge's arcs turn a whole turn a loop, every
            // other one the other way.
            const turned = spin && part.kind === "arc"
                ? { from: finite(part.from, 0) + (partIndex % 2 ? -spin : spin), to: finite(part.to, 90) + (partIndex % 2 ? -spin : spin) }
                : ({})
            const local = partRings(Object.assign({}, part, { on: on }, turned))
            const spines = local.filter(function(ring) { return ring.spine }).map(function(ring) {
                return ring.spine.map(function(p) { const q = place([p[0], p[1]]); return { x: q.x, y: q.y, width: p[2] * halfWidth } })
            })
            const solid = !part.glow && part.kind !== "lens"
            result.push({
                kind: String(part.kind),
                rings: local.map(function(ring) { return ring.map(place) }),
                spines: spines,
                bevel: finite(part.bevel, solid ? 0.06 : 0) * halfWidth,
                colour: colourOf(decoration, part.colour, decoration.colours.light),
                z: part.z === "front" ? "front" : "back",
                fill: colourOf(decoration, part.fill, part.material ? "" : decoration.colours.body),
                material: part.material === true ? { body: true } : (part.material || null),
                glow: part.glow ? colourOf(decoration, part.glow, "") : "",
                stroke: part.stroke ? colourOf(decoration, part.stroke, "") : "",
                strokeWidth: finite(part.strokeWidth, 0.05) * halfWidth,
                // drawn in ink on the band (a map's compass), not built on it
                ink: part.ink === true,
                // a reflection on glass: it brightens what lies under it
                sheen: part.sheen === true,
                gradient: gradient ? { inner: colourOf(decoration, gradient[0], "#ffffff"), outer: colourOf(decoration, gradient[1], "#000000") } : null,
                opacity: clamp(finite(part.opacity, 1), 0, 1),
                shade: String(part.shade || ""),
                depth: finite(part.depth, 1),
                centre: place([0, middleY]),
                radius: Math.max(finite(part.w, 1), finite(part.h, 1)) / 2 * halfWidth,
                // its own width and height (pixels), and how a gem is cut
                size: { width: finite(part.w, 1) * halfWidth, height: finite(part.h, 1) * halfWidth },
                cut: String(part.cut || "brilliant"), halo: Math.max(0, Math.round(finite(part.halo, 0))),
                gleam: clamp(finite(part.gleam, 1), 0.5, 3),
                // a light resting dim (lit up under the pointer)
                dim: part.dim !== undefined ? clamp(finite(part.dim, 1), 0, 1) : undefined,
                // which way is out from the band here, and the part's own
                // height in pixels: a round part's shading runs across it
                normal: on === "inner" ? { x: -frame.nx, y: -frame.ny } : { x: frame.nx, y: frame.ny },
                height: finite(part.h, 1) * halfWidth,
                index: anchor.index
            })
        }
    }
    return result
}

// Strips along the band with their own colour or material, in screen space.
function placedLayers(decoration, runs, lay, length, closed, slots, halfWidth, step) {
    const result = []
    for (const layer of decoration.layers) {
        const inset = finite(layer.inset, 0), width = finite(layer.width, 0.3), offset = finite(layer.offset, 0)
        const margin = finite(layer.margin, 0) * halfWidth
        const avoid = finite(layer.avoid, 0) * halfWidth
        const dash = layer.dash ? [Math.max(0.05, finite(layer.dash[0], 1)) * halfWidth, Math.max(0.01, finite(layer.dash[1], 0.2)) * halfWidth] : null
        for (const run of runs) {
            let intervals = freeIntervals(run, run.open ? margin : 0, slots, avoid, length, closed)
            if (dash) {
                const pieces = []
                for (const interval of intervals)
                    for (let s = interval[0]; s < interval[1]; s += dash[0] + dash[1])
                        pieces.push([s, Math.min(interval[1], s + dash[0])])
                intervals = pieces
            }
            for (const interval of intervals) {
                const count = Math.max(2, Math.ceil((interval[1] - interval[0]) / Math.max(0.25, step)) + 1)
                const top = [], bottom = []
                for (let k = 0; k < count; ++k) {
                    const s = interval[0] + (interval[1] - interval[0]) * k / (count - 1)
                    const e = edgesAt(decoration, s, run, length, closed, slots, halfWidth)
                    let a, b
                    if (layer.side === "outer") { a = e.outer - inset; b = e.outer - inset - width }
                    else if (layer.side === "inner") { a = -e.inner + inset + width; b = -e.inner + inset }
                    else { a = offset + width / 2; b = offset - width / 2 }
                    if (layer.free !== true) {
                        a = clamp(a, -e.inner, e.outer)
                        b = clamp(b, -e.inner, e.outer)
                    }
                    top.push(lay.point(s, a * halfWidth))
                    bottom.push(lay.point(s, b * halfWidth))
                }
                result.push({ rings: [top.concat(bottom.reverse())],
                              fill: colourOf(decoration, layer.fill, layer.material ? "" : decoration.colours.edge),
                              material: layer.material === true ? { body: true } : (layer.material || null),
                              glow: layer.glow ? colourOf(decoration, layer.glow, "") : "",
                              opacity: clamp(finite(layer.opacity, 1), 0, 1),
                              z: layer.z === "over" ? "over" : "under", depth: finite(layer.depth, 0.2),
                              // a stain soaking into the surface, or a channel cut into it
                              ink: layer.ink === true, recess: layer.recess === true,
                              // glass looking out (a viewport): what lies beyond it gives the light
                              window: layer.window === true,
                              // glossy as enamel under a clear glaze
                              gloss: layer.gloss === true,
                              gleam: clamp(finite(layer.gleam, 1), 0.5, 3),
                              // how far along the band its middle is, for lights chasing along it
                              along: shareAlong((interval[0] + interval[1]) / 2, length, closed) })
            }
        }
    }
    return result
}

// A round body's shading (a branch, a log, a column): strips across the
// band darker or lighter by how a cylinder's surface there faces the
// light, which comes from the screen's top left; the band's bends change
// which side is lit, so the strips are cut where the facing turns.
function roundShading(decoration, runs, lay, length, closed, slots, halfWidth, step) {
    if (!decoration.body.round) return []
    const chrome = decoration.body.round === "chrome"
    const strips = chrome ? 20 : 10
    const light = [-0.42, -0.62, 0.66]
    const flat = 0.38 + 0.85 * light[2]
    const result = []
    for (const run of runs) {
        const samples = runSamples(run, step)
        const edges = samples.map(function(s) { return edgesAt(decoration, s, run, length, closed, slots, halfWidth) })
        const facing = samples.map(function(s) { const f = lay.frame(s); return f.nx * light[0] + f.ny * light[1] })
        let start = 0
        while (start < samples.length - 1) {
            let end = start + 1
            while (end < samples.length - 1 && Math.abs(facing[end] - facing[start]) < 0.06) ++end
            const face = (facing[start] + facing[end]) / 2
            for (let strip = 0; strip < strips; ++strip) {
                const a = strip / strips, b = (strip + 1) / strips
                const v = a + b - 1
                let level
                if (chrome) {
                    // How far up the reflected view looks: the lit side
                    // mirrors a bright sky in a narrow streak, the middle
                    // the dark horizon, the far side a softer floor.
                    const up = v * Math.sign(face || 1)
                    const streak = Math.exp(-Math.pow((up - 0.55) / 0.12, 2))
                    const floor = 0.35 * Math.exp(-Math.pow((up + 0.5) / 0.22, 2))
                    level = 0.55 + 1.5 * streak + floor
                    if (Math.abs(v) > 0.85) level *= 0.7
                } else {
                    const lambert = Math.max(0, v * face + Math.sqrt(Math.max(0, 1 - v * v)) * light[2])
                    level = (0.38 + 0.85 * lambert) / flat
                }
                const top = [], bottom = []
                for (let k = start; k <= end; ++k) {
                    const e = edges[k], thickness = e.outer + e.inner
                    top.push(lay.point(samples[k], (-e.inner + thickness * b) * halfWidth))
                    bottom.push(lay.point(samples[k], (-e.inner + thickness * a) * halfWidth))
                }
                result.push({ rings: [top.concat(bottom.reverse())], colour: level < 1 ? "#000000" : "#ffffff",
                              opacity: level < 1 ? Math.min(0.8, (1 - level) * 0.9) : Math.min(chrome ? 0.7 : 0.45, (level - 1) * 0.8) })
            }
            start = end
        }
    }
    return result
}

// A soft outline that fades away (a nebula's cloud): strips inside each
// edge and each open end, every one erasing a share of the body, more the
// nearer the edge.
function featherStrips(decoration, runs, lay, length, closed, slots, halfWidth, step) {
    const feather = decoration.body.feather
    if (feather <= 0) return []
    const levels = 8
    const result = []
    for (const run of runs) {
        const samples = runSamples(run, step)
        const edges = samples.map(function(s) { return edgesAt(decoration, s, run, length, closed, slots, halfWidth) })
        for (let k = 0; k < levels; ++k) {
            const a = feather * k / levels, b = feather * (k + 1) / levels
            const erase = Math.pow(1 - (k + 0.5) / levels, 1.6)
            for (const outerSide of [true, false]) {
                const top = [], bottom = []
                samples.forEach(function(s, i) {
                    const e = edges[i], half = (e.outer + e.inner) / 2
                    const aa = Math.min(a, half), bb = Math.min(b, half)
                    if (outerSide) {
                        top.push(lay.point(s, (e.outer - aa) * halfWidth))
                        bottom.push(lay.point(s, (e.outer - bb) * halfWidth))
                    } else {
                        top.push(lay.point(s, (-e.inner + bb) * halfWidth))
                        bottom.push(lay.point(s, (-e.inner + aa) * halfWidth))
                    }
                })
                result.push({ rings: [top.concat(bottom.reverse())], erase: erase })
            }
            if (run.open)
                for (const atStart of [true, false]) {
                    const s0 = atStart ? run.from + a * halfWidth * 1.6 : run.to - b * halfWidth * 1.6
                    const s1 = atStart ? run.from + b * halfWidth * 1.6 : run.to - a * halfWidth * 1.6
                    const ring = []
                    for (const s of [s0, s1]) {
                        const e = edgesAt(decoration, clamp(s, run.from, run.to), run, length, closed, slots, halfWidth)
                        ring.push(lay.point(s, e.outer * halfWidth))
                    }
                    for (const s of [s1, s0]) {
                        const e = edgesAt(decoration, clamp(s, run.from, run.to), run, length, closed, slots, halfWidth)
                        ring.push(lay.point(s, -e.inner * halfWidth))
                    }
                    result.push({ rings: [ring], erase: erase })
                }
        }
    }
    return result
}

// The lit and shaded rim inside a shape's outline, as a bevel: for every
// edge of every ring a thin strip `width` pixels deep, white where the edge
// faces the light (the screen's top left) and black where it faces away,
// in two depths so the rim softens inward. Rings may be holes; each ring's
// inside is found from its own winding.
function bevelStrips(rings, width) {
    const result = []
    if (!(width > 0.3)) return result
    const light = [-0.6, -0.8]
    for (const ring of rings) {
        const n = ring.length
        if (n < 3) continue
        let area = 0
        for (let i = 0; i < n; ++i) {
            const p = ring[i], q = ring[(i + 1) % n]
            area += p.x * q.y - q.x * p.y
        }
        // Screen y runs down: a positive area winds clockwise on screen,
        // its inside on the right of each edge.
        const side = area > 0 ? 1 : -1
        const normals = []
        for (let i = 0; i < n; ++i) {
            const p = ring[i], q = ring[(i + 1) % n]
            const dx = q.x - p.x, dy = q.y - p.y
            const len = Math.hypot(dx, dy) || 1
            normals.push({ x: -dy / len * side, y: dx / len * side })
        }
        const inside = function(depth) {
            return ring.map(function(p, i) {
                const a = normals[(i + n - 1) % n], b = normals[i]
                let nx = a.x + b.x, ny = a.y + b.y
                const len = Math.hypot(nx, ny) || 1
                nx /= len; ny /= len
                const miter = Math.min(2.5, 1 / Math.max(0.4, nx * b.x + ny * b.y))
                return { x: p.x - nx * depth * miter, y: p.y - ny * depth * miter }
            })
        }
        const half = inside(width * 0.45), full = inside(width)
        for (let i = 0; i < n; ++i) {
            const j = (i + 1) % n
            const facing = normals[i].x * light[0] + normals[i].y * light[1]
            if (Math.abs(facing) < 0.08) continue
            const colour = facing > 0 ? "#ffffff" : "#000000"
            const strength = facing > 0 ? 0.42 * facing : 0.5 * -facing
            result.push({ rings: [[ring[i], ring[j], half[j], half[i]]], colour: colour, opacity: strength })
            result.push({ rings: [[half[i], half[j], full[j], full[i]]], colour: colour, opacity: strength * 0.45 })
        }
    }
    return result
}

// How far along the band `s` is, 0 to 1 (round a closed band, wrapping).
function shareAlong(s, length, closed) {
    if (!(length > 0)) return 0
    return closed ? (((s % length) + length) % length) / length : clamp(s / length, 0, 1)
}

// The band laid out with whatever stands out of it swinging as it sways:
// twigs, tendrils, a fringe, past its edges, along the band, the farther out
// the more, in a ripple so neighbours do not swing together; and with
// `ends`, a broom's tail past an open band's end, across it.
function swayingRibbon(lay, motion, halfWidth, ends) {
    const length = lay.length
    const ripples = lay.closed ? Math.max(1, Math.round(length / (9 * halfWidth))) : length / (9 * halfWidth)
    return Object.assign({}, lay, {
        point: function(s, t) {
            const out = Math.min(4, Math.max(0, Math.abs(t) / halfWidth - 1))
            const beyond = ends && !lay.closed ? Math.min(4, Math.max(0, -s, s - length) / halfWidth) : 0
            const wave = Math.sin(motion.sway + 2 * Math.PI * ripples * s / Math.max(1e-9, length))
            return lay.point(s + motion.swayAmount * out * halfWidth * wave,
                             t + motion.swayAmount * beyond * halfWidth * Math.sin(motion.sway + 1.3))
        }
    })
}

// A flat panel's decoration in screen space: the outline of each run as a
// polygon (a closed band as two rings, the inner one a hole), the strips,
// the parts, the shading, the fading and the lines.
// `path`: { points: [{x, y}], closed, outward }; `halfWidth`: half the
// plain band's width in pixels; `slots`: the icons' places along the path
// in pixels; `options`: { step, motion }, `motion` the angle each moving
// kind has turned to (DecorationScene.motionAt): `waves`, `flow`, `sway`
// with `swayAmount` and `swayMode` ("swing" or "float"), `walk` with
// `strides`, `spin`.
function flatDecoration(contour, path, halfWidth, slots, options) {
    const settings = options || ({})
    const decoration = normalizedContour(contour)
    decoration.motion = settings.motion || null
    const lay = ribbon(path)
    const length = lay.length
    if (length <= 0) return { shapes: [], lines: [], parts: [], layers: [], shading: [], fades: [], length: 0 }
    const step = finite(settings.step, Math.max(1, halfWidth / 6))
    const runs = runsOf(decoration, length, lay.closed, slots, halfWidth)
    const shapes = []
    const colours = decoration.segments.colours
    for (const run of runs) {
        const samples = runSamples(run, step)
        const outer = [], inner = []
        for (const s of samples) {
            const e = edgesAt(decoration, s, run, length, lay.closed, slots, halfWidth)
            outer.push(lay.point(s, e.outer * halfWidth))
            inner.push(lay.point(s, -e.inner * halfWidth))
        }
        const fill = colours.length ? colours[run.index % colours.length] : decoration.colours.body
        if (run.open) shapes.push({ rings: [outer.concat(inner.reverse())], colour: fill, index: run.index })
        else shapes.push({ rings: [outer, inner.reverse()], colour: fill, index: run.index })
    }
    const swaying = decoration.motion && decoration.motion.sway !== undefined
    const lineLay = swaying ? swayingRibbon(lay, decoration.motion, halfWidth, true) : lay
    const lines = linesOf(decoration, runs, length, lay.closed, slots, halfWidth, step).map(function(line) {
        return Object.assign({}, line, {
            // how far along the band it starts, for lights chasing along it
            along: line.points.length ? shareAlong(line.points[0][0], length, lay.closed) : 0,
            points: line.points.map(function(p) { return lineLay.point(p[0], p[1] * halfWidth) }),
            width: line.width * halfWidth,
            outlineWidth: line.outlineWidth !== undefined ? line.outlineWidth * halfWidth : undefined
        })
    })
    const parts = placedParts(decoration, runs, swaying ? swayingRibbon(lay, decoration.motion, halfWidth, false) : lay,
                              length, lay.closed, slots, halfWidth)
    // The launcher's seat, where the decoration has one.
    let launcher = null
    if (decoration.launcher) {
        const spec = decoration.launcher
        const anchor = partAnchors(spec, runs, length, lay.closed, slots, halfWidth)[0]
        if (anchor) {
            const s = anchor.s + anchor.dir * finite(spec.s, 0) * halfWidth
            const sEdge = lay.closed ? ((s % length) + length) % length : clamp(s, 0, length)
            let run = runs[0]
            for (const candidate of runs)
                if (sEdge >= candidate.from - 1e-6 && sEdge <= candidate.to + 1e-6) { run = candidate; break }
            const e = edgesAt(decoration, clamp(sEdge, run.from, run.to), run, length, lay.closed, slots, halfWidth)
            const t = spec.on === "outer" ? e.outer + finite(spec.t, 0) : spec.on === "inner" ? -(e.inner + finite(spec.t, 0)) : finite(spec.t, 0)
            const p = lay.point(s, t * halfWidth)
            launcher = { x: p.x, y: p.y, size: finite(spec.size, 1.2) * halfWidth }
        }
    }
    for (const part of parts) part.bevels = bevelStrips(part.rings, part.bevel)
    const bodyBevel = decoration.body.fill !== "none" && decoration.body.feather <= 0 ? decoration.body.bevel * halfWidth : 0
    return { shapes: shapes, lines: lines, length: length, runs: runs, colours: decoration.colours,
             pattern: decoration.pattern, body: decoration.body,
             bevels: [].concat.apply([], shapes.map(function(shape) { return bevelStrips(shape.rings, bodyBevel) })),
             launcher: launcher,
             layers: placedLayers(decoration, runs, lay, length, lay.closed, slots, halfWidth, step),
             parts: parts,
             shading: roundShading(decoration, runs, lay, length, lay.closed, slots, halfWidth, step),
             fades: featherStrips(decoration, runs, lay, length, lay.closed, slots, halfWidth, step) }
}
