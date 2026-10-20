.pragma library
.import "ContourFrames.js" as Frames
.import "DecorationCatalogue.js" as Catalogue

// A panel decoration as the scene the decoration lighting draws: every
// element in drawing order, each with its outline or stroke, its albedo (a
// colour, a gradient or a real material with its tint, relief and grain),
// the height profile its shape gives it (a cylinder for a round body, a dome
// for a pod, a tube for a cable or a twig, a bevelled plateau for a plate, a
// groove for a carving, folds for paper), how its surface answers light
// (metalness, roughness, a clear coat), whether it gives light (and into
// which group, so a group can pulse or blink on its own), and the viewer's
// colour choices. Geometry comes from ContourFrames.js, so a decoration fits
// every panel shape and faces into the screen on every edge.
//
// scene(decoration, path, halfWidth, slots, options) -> { hw, path, elements,
//     launcher, bounds, groups }
//   path: { points: [{x, y}], closed, outward }, in the panel's pixels;
//   halfWidth: half the plain band's width in pixels; slots: the icons'
//   places along the path (pixels of arc length); options: { choices,
//   step, phase }, `phase` the share (0 to 1) of the decoration's motion
//   loop to draw (motionLoop, motionFrames).
// choices: { light, lightFinish, main, mainFinish, palette, metal, material,
//   hue, saturation, brightness }: every decoration can change colour, and
//   its body can be made of another material; empty means as designed.

function paint(colour) {
    const c = String(colour || "#000000")
    if (/^#[0-9a-fA-F]{8}$/.test(c)) return { c: c.slice(0, 7), o: parseInt(c.slice(7, 9), 16) / 255 }
    return { c: c, o: 1 }
}

// Two colours multiplied, channel by channel.
function mix(a, b) {
    const pa = paint(a).c, pb = paint(b).c
    const channel = function(hex, k) { return parseInt(hex.slice(1 + 2 * k, 3 + 2 * k), 16) }
    let out = "#"
    for (let k = 0; k < 3; ++k) {
        const v = Math.round(channel(pa, k) * channel(pb, k) / 255).toString(16)
        out += v.length < 2 ? "0" + v : v
    }
    return out
}

function hexToHsl(hex) {
    const c = paint(hex).c
    const r = parseInt(c.slice(1, 3), 16) / 255, g = parseInt(c.slice(3, 5), 16) / 255, b = parseInt(c.slice(5, 7), 16) / 255
    const max = Math.max(r, g, b), min = Math.min(r, g, b), l = (max + min) / 2
    let h = 0, s = 0
    if (max !== min) {
        const d = max - min
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min)
        h = max === r ? (g - b) / d + (g < b ? 6 : 0) : max === g ? (b - r) / d + 2 : (r - g) / d + 4
        h /= 6
    }
    return [h, s, l]
}

function hslToHex(h, s, l) {
    const f = function(n) {
        const k = (n + h * 12) % 12, a = s * Math.min(l, 1 - l)
        const v = Math.round(255 * (l - a * Math.max(-1, Math.min(k - 3, 9 - k, 1)))).toString(16)
        return v.length < 2 ? "0" + v : v
    }
    return "#" + f(0) + f(8) + f(4)
}

// A light's colour moved to the chosen hue, keeping how bright it is; a
// white-hot or grey light stays as it is.
function recolourLight(hex, target) {
    if (!target || !hex) return hex
    const p = paint(hex)
    const hsl = hexToHsl(p.c)
    if (hsl[1] < 0.15 || hsl[2] > 0.96 || hsl[2] < 0.04) return hex
    const t = hexToHsl(target)
    const out = hslToHex(t[0], Math.min(1, Math.max(t[1], hsl[1] * 0.8)), hsl[2])
    if (p.o >= 1) return out
    const alpha = Math.round(p.o * 255).toString(16)
    return out + (alpha.length < 2 ? "0" + alpha : alpha)
}

// The chosen main colour as hue (degrees), strength and brightness (HSV).
function mainColour(hex) {
    const c = paint(hex).c
    const r = parseInt(c.slice(1, 3), 16) / 255, g = parseInt(c.slice(3, 5), 16) / 255, b = parseInt(c.slice(5, 7), 16) / 255
    const max = Math.max(r, g, b), min = Math.min(r, g, b)
    return { hue: hexToHsl(c)[0] * 360, saturation: max > 0 ? (max - min) / max : 0, value: max }
}

// The metals a trim can be swapped for.
const METALS = ["polished-gold", "bright-gold", "yellow-gold", "gold-leaf", "white-gold", "rose-gold", "red-gold",
                "silver", "brushed-platinum", "polished-copper"]
function isMetal(id) { return METALS.indexOf(String(id || "")) >= 0 }

// The decoration with the viewer's palette, metal and body material applied
// to its data.
function withChoices(decoration, choices) {
    if (!choices) return decoration
    if (choices.material && Catalogue.material(choices.material) && decoration.material)
        decoration = Object.assign({}, decoration, { material: { id: String(choices.material) } })
    const contour = JSON.parse(JSON.stringify(decoration.contour))
    if (choices.palette && choices.palette.length) {
        if (contour.segments && contour.segments.colours) contour.segments.colours = choices.palette.slice()
        for (const line of contour.lines || []) if (line.colours && line.colours.length > 2) line.colours = choices.palette.slice()
    }
    const swapMetal = function(spec) {
        return spec && spec.id && isMetal(spec.id) && choices.metal ? Object.assign({}, spec, { id: choices.metal }) : spec
    }
    if (choices.metal) {
        for (const part of contour.parts || []) part.material = swapMetal(part.material)
        for (const layer of contour.layers || []) layer.material = swapMetal(layer.material)
    }
    // A body material chosen on purpose stays what it is; the metal choice
    // is for the trims.
    return Object.assign({}, decoration, { contour: contour,
                                           material: choices.material ? decoration.material : swapMetal(decoration.material) })
}

function round1(v) { return Math.round(v * 10) / 10 }

// The places along the path closest to the given points (icon centres), as
// arc lengths.
function slotsOnPath(path, centres) {
    const points = (path && path.points) || []
    const closed = path && path.closed === true
    const list = closed && points.length > 1 ? points.concat([points[0]]) : points
    const at = [0]
    for (let i = 1; i < list.length; ++i)
        at.push(at[i - 1] + Math.hypot(list[i].x - list[i - 1].x, list[i].y - list[i - 1].y))
    const result = []
    for (const c of centres || []) {
        let best = Infinity, place = 0
        for (let i = 1; i < list.length; ++i) {
            const a = list[i - 1], b = list[i]
            const dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy
            const u = len2 > 0 ? Math.max(0, Math.min(1, ((c.x - a.x) * dx + (c.y - a.y) * dy) / len2)) : 0
            const px = a.x + dx * u, py = a.y + dy * u
            const d = Math.hypot(c.x - px, c.y - py)
            if (d < best) { best = d; place = at[i - 1] + (at[i] - at[i - 1]) * u }
        }
        result.push(place)
    }
    return result
}

// Which animation group an element's light belongs to. Stars come in two
// groups that twinkle out of step.
function lightGroup(kind, index, blinking) {
    if (kind === "stars") return index % 2 === 0 ? "stars" : "starsB"
    if (kind === "lightning") return "arcs"
    if (blinking && ["chips", "windows", "widgets", "slats", "dots", "segment"].indexOf(kind) >= 0)
        return index % 2 === 0 ? "blinkA" : "blinkB"
    return "lights"
}

// The kinds of animation that move a decoration's shape or picture, drawn
// as a loop of frames; the others (pulse, flicker, twinkle, blink, crackle)
// brighten and dim its lights as they are.
var MOTIONS = ["waves", "drift", "flow", "sway", "walk", "spin", "cycle"]
// The footprints a walk sets down before it starts again at the first.
var STRIDES = 6

// How long one turn of a moving kind takes as designed (seconds).
function motionSeconds(animation) {
    if (animation.kind === "walk") return Math.max(0.1, Number(animation.period) || 0.45) * STRIDES
    if (animation.speed !== undefined) return 1 / Math.max(0.01, Number(animation.speed) || 0.01)
    return Math.max(0.1, Number(animation.period) || 4)
}

function movingKinds(decoration) {
    return ((decoration && decoration.animation) || []).filter(function(a) { return MOTIONS.indexOf(a.kind) >= 0 })
}

// One loop of a decoration's motion (seconds; 0 when nothing moves): every
// moving kind turns once in it, so the loop meets itself; its length is
// the kinds' own lengths averaged.
function motionLoop(decoration) {
    const kinds = movingKinds(decoration)
    if (!kinds.length) return 0
    let total = 0
    for (const a of kinds) total += motionSeconds(a)
    return Math.round(total / kinds.length * 100) / 100
}

// The frames a loop is drawn in: twelve, twice as many for spinning arcs,
// which move farthest from frame to frame.
function motionFrames(decoration) {
    const kinds = movingKinds(decoration)
    if (!kinds.length) return 0
    return kinds.some(function(a) { return a.kind === "spin" }) ? 24 : 12
}

// Each moving kind's angle at `phase` (0 to 1 through the loop), for the
// decoration engine (ContourFrames.flatDecoration's `motion`).
function motionAt(decoration, phase) {
    const kinds = movingKinds(decoration)
    if (!kinds.length || phase === undefined || phase === null || !isFinite(Number(phase))) return null
    const angle = 2 * Math.PI * (Number(phase) - Math.floor(Number(phase)))
    const motion = ({ strides: STRIDES })
    for (const a of kinds) {
        motion[a.kind] = angle
        if (a.kind === "sway") {
            motion.swayAmount = Math.max(0, Number(a.amount) || 0.1)
            motion.swayMode = a.mode === "float" ? "float" : "swing"
        }
    }
    return motion
}

// A colour's hue turned on by `share` of the colour wheel, its strength,
// lightness and opacity kept.
function turnHue(hex, share) {
    if (!hex) return hex
    const p = paint(hex)
    const hsl = hexToHsl(p.c)
    if (hsl[1] < 0.15) return hex
    const out = hslToHex(((hsl[0] + share) % 1 + 1) % 1, hsl[1], hsl[2])
    if (p.o >= 1) return out
    const alpha = Math.round(p.o * 255).toString(16)
    return out + (alpha.length < 2 ? "0" + alpha : alpha)
}

function scene(decoration, path, halfWidth, slots, options) {
    const settings = options || ({})
    const choices = settings.choices || ({})
    decoration = withChoices(decoration, choices)
    const hw = halfWidth
    const animations = decoration.animation || []
    const blinking = animations.some(function(a) { return a.kind === "blink" })
    const motion = motionAt(decoration, settings.phase)
    const d = Frames.flatDecoration(decoration.contour, path, hw, slots,
                                    { step: settings.step || Math.max(1, hw / 16), motion: motion })
    const rings = function(list) {
        return list.map(function(ring) { return ring.map(function(p) { return [round1(p.x), round1(p.y)] }) })
    }
    const albedoOf = function(spec, fallback, extraTint) {
        if (spec && (spec.body || spec === true)) spec = decoration.material
        if (spec && spec.id) {
            const m = Catalogue.material(spec.id)
            if (m) {
                let tint = spec.tint || m.tint || null
                if (extraTint) tint = tint ? mix(tint, extraTint) : paint(extraTint).c
                return { texture: m.texture, tint: tint, relief: spec.relief !== undefined ? spec.relief : m.relief,
                         strip: !!m.strip, fit: !!spec.fit, grain: !decoration.classic && m.grain === "vertical",
                         scale: decoration.classic ? 1 : (spec.scale || m.scale || 1),
                         surface: { metal: m.metal || 0, rough: m.rough === undefined ? 0.55 : m.rough, coat: m.coat || 0 } }
            }
        }
        const p = paint(fallback)
        return { colour: p.c, alpha: p.o, surface: { metal: 0, rough: 0.45, coat: 0 } }
    }
    const elements = []
    const body = d.body || ({})
    const segmentColours = (decoration.contour.segments || {}).colours || []
    // The body: a round one is a cylinder across, chrome a polished one, any
    // other a bevelled slab; a soft one (a nebula) fades at its edges.
    if (body.fill !== "none")
        d.shapes.forEach(function(s, index) {
            const albedo = albedoOf(decoration.material, s.colour,
                                    segmentColours.length ? segmentColours[index % segmentColours.length] : null)
            const surface = albedo.surface
            const space = !!albedo.strip
            const segment = blinking && segmentColours.length > 2
            elements.push({ type: "fill", role: "body", rings: rings(s.rings), evenodd: true, albedo: albedo,
                            emissive: space ? 0.6 : segment ? 0.25 : 0,
                            group: space ? "" : segment ? lightGroup("segment", index, true) : "",
                            profile: decoration.folds ? "folds" : body.round ? "cylinder" : "bevel",
                            period: decoration.folds ? decoration.folds.period : undefined,
                            height: decoration.folds ? decoration.folds.height : body.round ? 0.55 : 0.32,
                            bevel: decoration.folds ? 0.06 : 0.22,
                            metal: surface.metal,
                            rough: body.round === "chrome" && surface.metal > 0.5 ? Math.min(surface.rough, 0.16) : surface.rough,
                            coat: body.coat !== null && body.coat !== undefined ? body.coat
                                : body.round === "chrome" || albedo.strip || surface.coat ? 1 : 0,
                            opacity: body.opacity === undefined ? 1 : body.opacity, feather: (body.feather || 0) * hw,
                            glow: body.glow ? paint(body.glow).c : null })
            if (body.gleam !== undefined && body.gleam !== 1) elements[elements.length - 1].gleam = body.gleam
        })
    // A part, strip or line lit in a light tent keeps that (only where it is
    // asked for, so the designs as they were stay as they were).
    const shining = function(element, gleam) {
        if (typeof gleam === "number" && gleam !== 1) element.gleam = gleam
        return element
    }
    const partElementPlain = function(part, index) {
        if (part.sheen) {
            const p = paint(part.fill)
            return { type: "fill", role: "part", rings: rings(part.rings), albedo: { colour: p.c, alpha: p.o },
                     profile: "flat", height: 0, stack: true, metal: 0, rough: 0.1, opacity: part.opacity, emissive: 0.6,
                     sheen: true, group: "" }
        }
        const bodyMaterial = !!part.material && (part.material === true || part.material.body === true)
        const albedo = part.material ? albedoOf(part.material, part.fill)
            : part.gradient ? { gradient: [paint(part.gradient.inner).c, paint(part.gradient.outer).c],
                                centre: [part.centre.x, part.centre.y], radius: part.radius,
                                surface: { metal: 0, rough: 0.3, coat: 0 } }
            : albedoOf(null, part.fill)
        const round = ["ellipse", "dome", "capsule", "lens"].indexOf(part.kind) >= 0
        const tube = part.kind === "branch" || part.shade === "cylinder"
        const light = !!part.glow || part.kind === "lens" || part.kind === "flame"
        let metal = albedo.surface.metal, rough = albedo.surface.rough, coat = albedo.surface.coat || 0
        if (part.shade === "chrome") {
            if (metal > 0.5) rough = Math.min(rough, 0.14)
            else coat = 1
        }
        return { type: "fill", role: "part", kind: part.kind, rings: rings(part.rings), evenodd: true, albedo: albedo,
                 spines: (part.spines || []).map(function(spine) {
                     return spine.map(function(p) { return [round1(p.x), round1(p.y), round1(p.width)] }) }),
                 profile: part.kind === "flame" ? "flat" : round ? "dome" : tube ? "tube" : "bevel",
                 height: part.kind === "lens" ? 0.5 : round ? 0.55 : tube ? 0.45 : 0.22, bevel: 0.1,
                 stack: true, metal: metal, rough: rough, coat: coat, opacity: part.opacity,
                 emissive: part.kind === "lens" ? 0.95 : part.kind === "flame" ? 1 : light ? 0.85 : 0,
                 group: light ? lightGroup(part.kind, index, false) : "",
                 glow: part.glow ? paint(part.glow).c : part.kind === "lens" ? paint(part.colour).c : null,
                 lens: part.kind === "lens" ? { colour: paint(part.colour).c, centre: [round1(part.centre.x), round1(part.centre.y)],
                                                radius: part.radius } : null,
                 flame: part.kind === "flame", glowGain: part.kind === "flame" ? 3.0 : part.kind === "star" ? 3.5 : 1,
                 bodyMaterial: bodyMaterial }
    }
    const partElement = function(part, index) {
        return shining(partElementPlain(part, index), part.gleam)
    }
    // A cut stone seen from above, as a jeweller's render draws one, in its
    // colour (or its material's): a round brilliant (a dark girdle, eight
    // crown facets in alternating light and shade round an octagonal
    // table, eight star facets at the table's edge), a step cut - square
    // (princess) or long (baguette) - as frames of facets lit from the top
    // left, or a cabochon, a polished dome glowing from within; sparks of
    // the stone's fire and a white glint that twinkles; and, round it, a
    // halo of small diamonds where it has one.
    const gemElements = function(part, index) {
        const spec = part.material && part.material.id ? Catalogue.material(part.material.id) : null
        const base = paint(part.colour && part.colour !== decoration.contour.colours.light ? part.colour
                           : spec && spec.tint ? spec.tint : "#f3f8ff").c
        const hsl = hexToHsl(base)
        // A colourless stone (a diamond, rock crystal) by how much colour
        // it has, not its saturation, which is high for any very pale tint;
        // its facets are greys with only a breath of its tint.
        const channel = function(k) { return parseInt(base.slice(1 + 2 * k, 3 + 2 * k), 16) / 255 }
        const chroma = Math.max(channel(0), channel(1), channel(2)) - Math.min(channel(0), channel(1), channel(2))
        const pale = chroma < 0.2
        const tint = pale ? 0.12 : 1
        const tone = function(lightness, strength) {
            return hslToHex(hsl[0], Math.min(1, hsl[1] * strength) * tint, Math.max(0, Math.min(1, lightness)))
        }
        const shade = function(paleLevel, factor, plus, most) {
            return tone(pale ? paleLevel : Math.min(most, hsl[2] * factor + plus), 1.05)
        }
        const cx = part.centre.x, cy = part.centre.y, r = part.radius
        // The stone's own frame: along the band and out from it.
        const nx = part.normal ? part.normal.x : 0, ny = part.normal ? part.normal.y : -1
        const local = function(u, v) { return [cx - ny * u + nx * v, cy + nx * u + ny * v] }
        const polygon = function(list) { return [list.map(function(p) { return [round1(p[0]), round1(p[1])] })] }
        const at = function(angle, radius) { return [cx + Math.cos(angle) * radius, cy + Math.sin(angle) * radius] }
        const facet = function(rings, colour, emissive, group) {
            return { type: "fill", role: "part", kind: "gem", rings: rings, albedo: { colour: colour, alpha: 1 },
                     profile: "flat", height: 0.04, stack: true, metal: 0, rough: 0.04, coat: 1, opacity: part.opacity,
                     emissive: emissive || 0, group: group || "", glow: null }
        }
        const glint = function(x, y, size, colour, gain) {
            return { type: "dot", role: "part", kind: "gem", centre: [round1(x), round1(y)], radius: size,
                     albedo: { colour: colour, alpha: 1 }, profile: "flat", height: 0, stack: true, metal: 0, rough: 0.1,
                     emissive: 0.95, glow: colour, glowGain: gain || 1, group: lightGroup("stars", index, false) }
        }
        const out = []
        const fire = ["#ff6a5a", "#ffd25a", "#5affb4", "#5ac8ff"]
        const cut = part.cut || "brilliant"
        if (cut === "princess" || cut === "baguette") {
            const hu = Math.max(1, (part.size ? part.size.width : 2 * r) / 2), hv = Math.max(1, (part.size ? part.size.height : 2 * r) / 2)
            const rect = function(scale) {
                return [local(-hu * scale, hv * scale), local(hu * scale, hv * scale), local(hu * scale, -hv * scale), local(-hu * scale, -hv * scale)]
            }
            out.push(Object.assign(facet(polygon(rect(1)), shade(0.42, 0.45, 0, 1), 0.1), { profile: "bevel", height: 0.2, bevel: 0.3 }))
            // Two frames of steps, the light falling the other way on the
            // inner one, then the table.
            const frames = [[1, 0.74, [0.95, 0.8, 0.6, 0.72]], [0.74, 0.5, [0.62, 0.74, 0.94, 0.82]]]
            for (const frame of frames) {
                const a = rect(frame[0]), b = rect(frame[1])
                for (let k = 0; k < 4; ++k) {
                    const level = frame[2][k]
                    out.push(facet(polygon([a[k], a[(k + 1) % 4], b[(k + 1) % 4], b[k]]),
                                   shade(level, level * 0.9, 0.04, 0.72), level > 0.8 ? 0.45 : 0.2))
                }
            }
            out.push(facet(polygon(rect(0.5)), shade(0.85, 1.0, 0.02, 0.62), 0.3))
            const g = local(-hu * 0.3, hv * 0.3)
            out.push(glint(g[0], g[1], Math.max(0.7, Math.min(hu, hv) * 0.22), "#ffffff", 2.2))
            const f = local(hu * 0.45, -hv * 0.4)
            out.push(glint(f[0], f[1], Math.max(0.6, Math.min(hu, hv) * 0.12), fire[index % fire.length], 1))
        } else if (cut === "cabochon") {
            const hu = Math.max(1, (part.size ? part.size.width : 2 * r) / 2), hv = Math.max(1, (part.size ? part.size.height : 2 * r) / 2)
            const ring = function(scale, du, dv) {
                const list = []
                for (let k = 0; k < 40; ++k) {
                    const a = 2 * Math.PI * k / 40
                    list.push(local(du + Math.cos(a) * hu * scale, dv + Math.sin(a) * hv * scale))
                }
                return list
            }
            out.push(Object.assign(facet(polygon(ring(1, 0, 0)), shade(0.55, 0.62, 0, 1), 0.12),
                                   { profile: "dome", height: 0.45, rough: 0.03 }))
            out.push(Object.assign(facet(polygon(ring(0.58, hu * 0.12, -hv * 0.14)), shade(0.8, 1.15, 0.05, 0.7), 0.35),
                                   { feather: Math.max(1, Math.min(hu, hv) * 0.35), stack: true }))
            const g = local(-hu * 0.32, hv * 0.38)
            out.push(glint(g[0], g[1], Math.max(0.7, Math.min(hu, hv) * 0.2), "#ffffff", 1.6))
        } else {
            const girdle = []
            for (let k = 0; k < 32; ++k) girdle.push(at(2 * Math.PI * k / 32, r))
            out.push(Object.assign(facet(polygon(girdle), tone(pale ? 0.42 : hsl[2] * 0.42, 1), 0.1),
                                   { profile: "dome", height: 0.3, rough: 0.06 }))
            const table = 0.56 * r, turn = Math.PI / 8
            for (let k = 0; k < 8; ++k) {
                const a0 = 2 * Math.PI * k / 8 + turn, a1 = a0 + Math.PI / 4
                const light = k % 2 === 0
                out.push(facet(polygon([at(a0, table), at(a0, r * 0.97), at((a0 + a1) / 2, r * 0.97), at(a1, r * 0.97), at(a1, table)]),
                               light ? tone(pale ? 0.93 : Math.min(0.66, hsl[2] * 1.12 + 0.04), 1.1) : tone(pale ? 0.66 : hsl[2] * 0.62, 1),
                               light ? 0.45 : 0.15))
            }
            const tableRing = []
            for (let k = 0; k < 8; ++k) tableRing.push(at(2 * Math.PI * k / 8 + turn, table))
            out.push(facet(polygon(tableRing), tone(pale ? 0.86 : Math.min(0.6, hsl[2] * 1.02), 1.05), 0.3))
            for (let k = 0; k < 8; ++k) {
                const a = 2 * Math.PI * k / 8 + turn + Math.PI / 8
                out.push(facet(polygon([at(a - Math.PI / 8, table), at(a, table * 1.32), at(a + Math.PI / 8, table)]),
                               tone(pale ? 0.97 : Math.min(0.74, hsl[2] * 1.25 + 0.06), 1), 0.5))
            }
            // The stone's fire: sparks of the spectrum (a dark stone, onyx or
            // marcasite, has none), and its glint.
            for (let k = 0; k < (pale || hsl[2] > 0.3 ? 2 : 0); ++k) {
                const a = 0.7 + k * 2.4 + index * 1.3
                const colour = fire[(index + k) % fire.length]
                out.push(Object.assign(glint(cx + Math.cos(a) * r * 0.66, cy + Math.sin(a) * r * 0.66, Math.max(0.6, r * 0.09), colour, 1),
                                       { emissive: 0.9, group: lightGroup("stars", index + k, false) }))
            }
            out.push(glint(cx - r * 0.28, cy - r * 0.3, Math.max(0.7, r * 0.13), "#ffffff", 2.2))
        }
        // A halo: small diamonds all round the stone.
        if (part.halo > 0) {
            const reach = Math.max(r, part.size ? Math.max(part.size.width, part.size.height) / 2 : r)
            const small = Math.max(1.2, reach * 0.2)
            for (let k = 0; k < part.halo; ++k) {
                const a = 2 * Math.PI * k / part.halo
                const c = [cx + Math.cos(a) * (reach + small * 1.15), cy + Math.sin(a) * (reach + small * 1.15)]
                const disc = []
                for (let j = 0; j < 12; ++j) disc.push([c[0] + Math.cos(2 * Math.PI * j / 12) * small, c[1] + Math.sin(2 * Math.PI * j / 12) * small])
                out.push(Object.assign(facet(polygon(disc), "#7f858d", 0.1), { profile: "dome", height: 0.25 }))
                const tableDisc = []
                for (let j = 0; j < 8; ++j) tableDisc.push([c[0] + Math.cos(2 * Math.PI * j / 8 + 0.4) * small * 0.55, c[1] + Math.sin(2 * Math.PI * j / 8 + 0.4) * small * 0.55])
                out.push(facet(polygon(tableDisc), "#e9edf2", 0.45))
                if (k % 2 === 0) out.push(Object.assign(glint(c[0] - small * 0.3, c[1] - small * 0.3, Math.max(0.5, small * 0.25), "#ffffff", 1.4),
                                                        { group: lightGroup("stars", index + k, false) }))
            }
        }
        return out
    }
    const inkOutline = function(part) {
        if (!part.ink || !part.stroke) return []
        return [{ type: "stroke", role: "line", points: part.rings[0].map(function(p) { return [round1(p.x), round1(p.y)] }),
                  closed: true, width: Math.max(0.7, part.strokeWidth), cap: "round", ink: true,
                  strength: paint(part.stroke).o, albedo: { colour: paint(part.stroke).c, alpha: 1 } }]
    }
    // Parts behind the band first, then the body over them.
    d.parts.forEach(function(part, index) {
        if (part.z === "front") return
        if (part.kind === "gem") { for (const e of gemElements(part, index)) elements.push(e) }
        else elements.push(partElement(part, index))
    })
    if (elements.length && d.parts.some(function(p) { return p.z !== "front" })) {
        const bodies = elements.filter(function(e) { return e.role === "body" })
        for (const b of bodies) elements.splice(elements.indexOf(b), 1)
        for (const b of bodies) elements.push(b)
    }
    const layerElementPlain = function(layer) {
        if (layer.ink) {
            const p = paint(layer.fill)
            return { type: "fill", role: "layer", rings: rings(layer.rings), ink: true, strength: p.o * layer.opacity,
                     albedo: { colour: p.c, alpha: 1 } }
        }
        const albedo = layer.material ? albedoOf(layer.material, layer.fill) : albedoOf(null, layer.fill)
        if (layer.window)
            return { type: "fill", role: "layer", rings: rings(layer.rings), albedo: Object.assign({}, albedo, { relief: 0 }),
                     profile: "groove", height: 0.22, stack: false, metal: 0, rough: 0.04, coat: 1, emissive: 0.7,
                     opacity: layer.opacity, group: "", along: layer.along }
        if (layer.recess)
            return { type: "fill", role: "layer", rings: rings(layer.rings), albedo: albedo, profile: "groove", height: 0.14,
                     stack: true, metal: albedo.surface.metal, rough: albedo.surface.rough, opacity: layer.opacity }
        const light = !!layer.glow
        return { type: "fill", role: "layer", rings: rings(layer.rings), albedo: albedo,
                 profile: light ? "flat" : "bevel", height: light ? 0.02 : 0.12, bevel: 0.05,
                 stack: true, metal: albedo.surface.metal, rough: layer.gloss ? 0.06 : albedo.surface.rough,
                 coat: layer.gloss ? 1 : albedo.surface.coat || 0,
                 opacity: layer.opacity, emissive: light ? 0.9 : 0, group: light ? "lights" : "",
                 glow: layer.glow ? paint(layer.glow).c : null, along: layer.along }
    }
    const layerElement = function(layer) {
        return shining(layerElementPlain(layer), layer.gleam)
    }
    for (const layer of d.layers) if (layer.z !== "over") elements.push(layerElement(layer))
    // Lines: lights glow, carvings are grooves, drawn things stand up a
    // little, ink soaks into paper.
    const carved = ["cracks", "panels"]
    const inked = ["plan", "footprints"]
    const raisedDetail = ["greebles", "plates"]
    const tubes = ["plait", "twigs", "bristles", "tendrils", "fringe", "scrolls"]
    const counters = ({})
    for (const l of d.lines) {
        const p = paint(l.colour)
        const index = counters[l.kind] = (counters[l.kind] || 0) + 1
        // An engraved line is cut into the surface: never a light.
        const lit = !l.engrave && carved.indexOf(l.kind) < 0 && raisedDetail.indexOf(l.kind) < 0 && tubes.indexOf(l.kind) < 0
            && l.kind !== "dotgrid"
        const base = shining({ role: "line", kind: l.kind, stack: true, opacity: (l.opacity === undefined ? 1 : l.opacity) * p.o,
                               along: l.along }, l.gleam)
        if (l.dots) {
            for (let k = 0; k < l.points.length; ++k) {
                const pt = l.points[k], r = Math.max(0.8, l.width) * (l.sizes ? l.sizes[k] : 1) * (l.plain ? 1 : 1.1)
                const glowing = !(l.kind === "dots" || l.plain)
                // Studs are metal unless they say otherwise (pearls, enamel
                // beads); a bead that is not metal is glossy.
                const beadMetal = l.metal !== undefined ? l.metal : l.kind === "dots" ? 0.8 : 0
                const pearl = l.kind === "dots" && beadMetal < 0.5
                elements.push(Object.assign({}, base, pearl ? { coat: 1 } : {}, {
                    type: "dot", centre: [round1(pt.x), round1(pt.y)], radius: r, albedo: { colour: p.c, alpha: 1 },
                    profile: l.kind === "dots" ? "dome" : "flat", height: 0.1, metal: beadMetal,
                    rough: pearl ? 0.18 : 0.3,
                    emissive: glowing ? 0.9 : 0, glow: glowing ? p.c : null, glowGain: l.kind === "stars" ? 2.6 : 1,
                    group: glowing ? lightGroup(l.kind, k, blinking) : "" }))
            }
            continue
        }
        if (l.points.length < 2) continue
        const pts = l.points.map(function(pt) { return [round1(pt.x), round1(pt.y)] })
        if (inked.indexOf(l.kind) >= 0) {
            elements.push(Object.assign({}, base, l.fill
                ? { type: "fill", rings: [pts], ink: true, strength: p.o * (l.opacity === undefined ? 1 : l.opacity),
                    albedo: { colour: p.c, alpha: 1 }, step: l.step }
                : { type: "stroke", points: pts, closed: !!l.closed, width: Math.max(0.7, l.width), cap: l.cap || "round", ink: true,
                    dash: l.dash ? l.dash.map(function(v) { return v * hw }) : null, strength: p.o, albedo: { colour: p.c, alpha: 1 } }))
            continue
        }
        if (l.fill && (l.raised || l.recess)) {
            // Machinery: clamps, louvres and fins stand up, openings are cut in.
            const albedo = l.material ? albedoOf(l.material, l.colour) : { colour: p.c, alpha: 1, surface: { metal: 0.7, rough: 0.35, coat: 0 } }
            elements.push(Object.assign({}, base, { type: "fill", rings: [pts], albedo: albedo,
                profile: l.recess ? "groove" : "bevel", height: l.recess ? 0.16 : 0.12, bevel: 0.03,
                metal: albedo.surface.metal, rough: albedo.surface.rough }))
            continue
        }
        if (l.fill) {
            const glowing = !!(l.glow || l.kind === "chips" || l.kind === "windows" || l.kind === "widgets")
            elements.push(Object.assign({}, base, { type: "fill", rings: [pts], albedo: { colour: p.c, alpha: 1 },
                profile: raisedDetail.indexOf(l.kind) >= 0 ? "bevel" : "flat", height: raisedDetail.indexOf(l.kind) >= 0 ? 0.1 : 0,
                bevel: 0.04, metal: 0, rough: 0.5, overlay: raisedDetail.indexOf(l.kind) >= 0 || l.kind === "panels",
                emissive: glowing ? 0.9 : 0, glow: glowing ? p.c : null,
                group: glowing ? lightGroup(l.kind, index, blinking) : "" }))
            continue
        }
        const stroke = Object.assign({}, base, { type: "stroke", points: pts, closed: !!l.closed, width: Math.max(0.6, l.width),
            cap: l.cap || "round", dash: l.dash ? l.dash.map(function(v) { return v * hw }) : null, taper: !!l.taper, clip: !!l.clip,
            albedo: l.material ? albedoOf(l.material, l.colour) : { colour: p.c, alpha: 1, surface: { metal: 0, rough: 0.4, coat: 0 } } })
        if (l.pipe) {
            const albedo = l.material ? albedoOf(l.material, l.colour) : { colour: p.c, alpha: 1, surface: { metal: 0.85, rough: 0.3, coat: 0 } }
            elements.push(Object.assign({}, stroke, { albedo: albedo, profile: "tube", height: 0.3,
                                                      metal: albedo.surface.metal, rough: albedo.surface.rough }))
            continue
        }
        if (tubes.indexOf(l.kind) >= 0 && !l.engrave) {
            if (l.outline) {
                const o = paint(l.outline)
                elements.push(Object.assign({}, stroke, { width: l.width + 2 * (l.outlineWidth || 1), albedo: { colour: o.c, alpha: 1 },
                    profile: "tube", height: 0.3, metal: 0, rough: 0.6, cap: "butt" }))
            }
            const surface = stroke.albedo.surface || { metal: 0, rough: 0.5 }
            elements.push(Object.assign({}, stroke, { profile: "tube", height: l.kind === "plait" ? 0.35 : 0.2,
                metal: l.kind === "fringe" ? 0.6 : surface.metal, rough: l.kind === "fringe" ? 0.35 : surface.rough,
                lift: l.kind === "fringe" ? 0.3 : undefined, noshadow: l.kind === "fringe" }))
        } else if (carved.indexOf(l.kind) >= 0 || (!lit && l.kind !== "dotgrid")) {
            elements.push(Object.assign({}, stroke, { profile: "groove", height: 0.08, metal: 0, rough: 0.8 }))
        } else {
            const glows = !(l.kind === "ticks" || l.kind === "dotgrid" || (l.kind === "widgets" && !l.glow))
            elements.push(Object.assign({}, stroke, { profile: "flat", height: 0, metal: 0, rough: 0.4, emissive: 0.9,
                glow: glows ? p.c : null, group: lightGroup(l.kind, index, blinking) }))
        }
    }
    for (const layer of d.layers) if (layer.z === "over") elements.push(layerElement(layer))
    d.parts.forEach(function(part, index) {
        if (part.z !== "front") return
        if (part.ink) { for (const e of inkOutline(part)) elements.push(e) }
        else if (part.kind === "gem") { for (const e of gemElements(part, index)) elements.push(e) }
        else elements.push(partElement(part, index))
    })
    // The viewer's colours: lights to the chosen hue, the body to the chosen
    // colour (its grain kept), a picture's hue, strength and brightness, and
    // the finishes (chameleon, iridescent, rainbow, metallic rainbow).
    for (const element of elements) {
        const lit = element.emissive > 0 || element.glow || element.lens || element.flame
        const picture = element.albedo && element.albedo.strip
        if (choices.light && lit && !picture) {
            if (element.glow) element.glow = recolourLight(element.glow, choices.light)
            if (element.lens) element.lens.colour = recolourLight(element.lens.colour, choices.light)
            if (element.albedo && element.albedo.colour) element.albedo.colour = recolourLight(element.albedo.colour, choices.light)
            if (element.albedo && element.albedo.gradient)
                element.albedo.gradient = element.albedo.gradient.map(function(c) { return recolourLight(c, choices.light) })
            if (element.flame) element.flameHue = hexToHsl(choices.light)[0] * 360
        }
        if (choices.main && (element.role === "body" || element.bodyMaterial) && !picture)
            element.recolour = mainColour(choices.main)
        if (choices.mainFinish && choices.mainFinish !== "solid" && (element.role === "body" || element.bodyMaterial) && !picture)
            element.finish = choices.mainFinish
        if (choices.lightFinish && choices.lightFinish !== "solid" && lit && !picture)
            element.lightFinish = choices.lightFinish
        if (picture && (choices.hue || choices.saturation !== undefined || choices.brightness !== undefined))
            element.adjust = { hue: choices.hue || 0, saturation: choices.saturation === undefined ? 1 : choices.saturation,
                               brightness: choices.brightness === undefined ? 1 : choices.brightness }
    }
    // Moving lights and pictures: the colours cycling round the wheel, the
    // picture drifting along the band and back, light chasing along strips
    // and lines (a wave or a plait already flows by its shape).
    if (motion) {
        const flowing = ["wave", "plait"]
        const chases = Math.max(1, d.length / (10 * hw))
        for (const element of elements) {
            const lit = element.emissive > 0 || element.glow || element.lens || element.flame
            const picture = element.albedo && element.albedo.strip
            if (motion.cycle !== undefined && lit && !picture) {
                const share = motion.cycle / (2 * Math.PI)
                if (element.glow) element.glow = turnHue(element.glow, share)
                if (element.lens) element.lens.colour = turnHue(element.lens.colour, share)
                if (element.albedo && element.albedo.colour) element.albedo.colour = turnHue(element.albedo.colour, share)
                if (element.albedo && element.albedo.gradient)
                    element.albedo.gradient = element.albedo.gradient.map(function(c) { return turnHue(c, share) })
                if (element.flame) element.flameHue = ((element.flameHue === undefined ? 30 : element.flameHue) + 360 * share) % 360
            }
            if (motion.drift !== undefined && picture)
                element.albedo = Object.assign({}, element.albedo, { shift: round1(hw * 1.2 * (1 - Math.cos(motion.drift)) / 2) })
            if (motion.flow !== undefined && lit && !picture && element.along !== undefined && flowing.indexOf(element.kind) < 0) {
                const wave = 0.5 + 0.5 * Math.cos(2 * Math.PI * chases * element.along - motion.flow)
                element.lit = 0.3 + 0.7 * wave * wave * wave
            }
        }
    }
    // What the decoration covers: its elements, their halo and the shadow
    // it casts, in the panel's pixels.
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity
    const extend = function(x, y, r) {
        x0 = Math.min(x0, x - r); y0 = Math.min(y0, y - r); x1 = Math.max(x1, x + r); y1 = Math.max(y1, y + r)
    }
    for (const e of elements) {
        if (e.type === "fill") for (const ring of e.rings) for (const p of ring) extend(p[0], p[1], 0)
        else if (e.type === "stroke") for (const p of e.points) extend(p[0], p[1], e.width)
        else extend(e.centre[0], e.centre[1], e.radius)
    }
    const groups = []
    for (const e of elements) if (e.group && groups.indexOf(e.group) < 0) groups.push(e.group)
    const reach = hw * 1.4
    const bounds = isFinite(x0) ? { x: Math.floor(x0 - reach), y: Math.floor(y0 - reach),
                                    width: Math.ceil(x1 - x0 + 2 * reach), height: Math.ceil(y1 - y0 + 2 * reach) }
                                : { x: 0, y: 0, width: 0, height: 0 }
    return { hw: hw, path: { points: (path.points || []).map(function(p) { return [round1(p.x), round1(p.y)] }), closed: path.closed === true },
             elements: elements, launcher: d.launcher, bounds: bounds, groups: groups }
}

// The colour settings that fit a decoration (the owner: every decoration
// recolourable, a multi-colour one with settings that suit it): light
// colour, main colour, palette, picture colours, metal; each with what the
// decoration is as designed.
function colourControls(decoration) {
    if (!decoration) return ({})
    const sample = []
    for (let k = 0; k <= 40; ++k) sample.push({ x: 40 + k * 20, y: 100 })
    const s = scene(decoration, { points: sample, closed: false, outward: { x: 0, y: -1 } }, 24,
                    [80, 200, 320, 440, 560, 680, 800], { step: 4 })
    const result = ({})
    const lightColours = ({})
    let pictures = false, mains = false, metals = false
    for (const e of s.elements) {
        const picture = e.albedo && e.albedo.strip
        if (picture) pictures = true
        if ((e.role === "body" || e.bodyMaterial) && !picture) mains = true
        if ((e.emissive > 0 || e.glow || e.lens || e.flame) && !picture) {
            const colour = e.lens ? e.lens.colour : e.glow || (e.albedo && e.albedo.colour)
            if (colour) {
                const hsl = hexToHsl(colour)
                if (hsl[1] >= 0.15 && hsl[2] < 0.96) lightColours[paint(colour).c] = (lightColours[paint(colour).c] || 0) + 1
            }
        }
    }
    const walk = function(value) {
        if (Array.isArray(value)) { for (const v of value) walk(v) }
        else if (value && typeof value === "object") {
            if (typeof value.id === "string" && isMetal(value.id)) metals = true
            for (const key in value) walk(value[key])
        }
    }
    walk(decoration)
    const lights = Object.keys(lightColours).sort(function(a, b) { return lightColours[b] - lightColours[a] })
    if (lights.length) result.light = { colour: lights[0] }
    if (mains) result.main = { colour: String(((decoration.contour || {}).colours || {}).body || "#808080") }
    const palette = ((decoration.contour || {}).segments || {}).colours
        || ((decoration.contour || {}).lines || []).reduce(function(found, l) {
            return found || (l.colours && l.colours.length > 2 ? l.colours : null) }, null)
    if (palette && palette.length > 2) result.palette = { colours: palette.slice() }
    if (pictures) result.picture = ({})
    if (metals) result.metal = ({})
    // A body made of a material can be made of another.
    if (decoration.material && decoration.material.id && ((decoration.contour || {}).body || {}).fill !== "none")
        result.material = { id: String(decoration.material.id) }
    return result
}
