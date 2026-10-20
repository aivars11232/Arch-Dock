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
//   step, phase }.
// choices: { light, lightFinish, main, mainFinish, palette, metal, hue,
//   saturation, brightness } (the owner: every decoration recolourable);
//   empty means as designed.

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

// The decoration with the viewer's palette and metal applied to its data.
function withChoices(decoration, choices) {
    if (!choices) return decoration
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
    return Object.assign({}, decoration, { contour: contour, material: swapMetal(decoration.material) })
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

// Which animation group an element's light belongs to.
function lightGroup(kind, index, blinking) {
    if (kind === "stars") return "stars"
    if (kind === "lightning") return "arcs"
    if (blinking && ["chips", "windows", "widgets", "slats", "dots", "segment"].indexOf(kind) >= 0)
        return index % 2 === 0 ? "blinkA" : "blinkB"
    return "lights"
}

function scene(decoration, path, halfWidth, slots, options) {
    const settings = options || ({})
    const choices = settings.choices || ({})
    decoration = withChoices(decoration, choices)
    const hw = halfWidth
    const animations = decoration.animation || []
    const blinking = animations.some(function(a) { return a.kind === "blink" })
    const d = Frames.flatDecoration(decoration.contour, path, hw, slots, { step: settings.step || Math.max(1, hw / 16) })
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
                            coat: body.round === "chrome" || albedo.strip || surface.coat ? 1 : 0,
                            opacity: body.opacity === undefined ? 1 : body.opacity, feather: (body.feather || 0) * hw,
                            glow: body.glow ? paint(body.glow).c : null })
        })
    const partElement = function(part, index) {
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
    const inkOutline = function(part) {
        if (!part.ink || !part.stroke) return []
        return [{ type: "stroke", role: "line", points: part.rings[0].map(function(p) { return [round1(p.x), round1(p.y)] }),
                  closed: true, width: Math.max(0.7, part.strokeWidth), cap: "round", ink: true,
                  strength: paint(part.stroke).o, albedo: { colour: paint(part.stroke).c, alpha: 1 } }]
    }
    // Parts behind the band first, then the body over them.
    d.parts.forEach(function(part, index) { if (part.z !== "front") elements.push(partElement(part, index)) })
    if (elements.length && d.parts.some(function(p) { return p.z !== "front" })) {
        const bodies = elements.filter(function(e) { return e.role === "body" })
        for (const b of bodies) elements.splice(elements.indexOf(b), 1)
        for (const b of bodies) elements.push(b)
    }
    const layerElement = function(layer) {
        if (layer.ink) {
            const p = paint(layer.fill)
            return { type: "fill", role: "layer", rings: rings(layer.rings), ink: true, strength: p.o * layer.opacity,
                     albedo: { colour: p.c, alpha: 1 } }
        }
        const albedo = layer.material ? albedoOf(layer.material, layer.fill) : albedoOf(null, layer.fill)
        if (layer.window)
            return { type: "fill", role: "layer", rings: rings(layer.rings), albedo: Object.assign({}, albedo, { relief: 0 }),
                     profile: "groove", height: 0.22, stack: false, metal: 0, rough: 0.04, coat: 1, emissive: 0.7,
                     opacity: layer.opacity, group: "" }
        if (layer.recess)
            return { type: "fill", role: "layer", rings: rings(layer.rings), albedo: albedo, profile: "groove", height: 0.14,
                     stack: true, metal: albedo.surface.metal, rough: albedo.surface.rough, opacity: layer.opacity }
        const light = !!layer.glow
        return { type: "fill", role: "layer", rings: rings(layer.rings), albedo: albedo,
                 profile: light ? "flat" : "bevel", height: light ? 0.02 : 0.12, bevel: 0.05,
                 stack: true, metal: albedo.surface.metal, rough: albedo.surface.rough, coat: albedo.surface.coat || 0,
                 opacity: layer.opacity, emissive: light ? 0.9 : 0, group: light ? "lights" : "",
                 glow: layer.glow ? paint(layer.glow).c : null }
    }
    for (const layer of d.layers) if (layer.z !== "over") elements.push(layerElement(layer))
    // Lines: lights glow, carvings are grooves, drawn things stand up a
    // little, ink soaks into paper.
    const carved = ["cracks", "panels"]
    const inked = ["plan", "footprints"]
    const raisedDetail = ["greebles", "plates"]
    const tubes = ["plait", "twigs", "bristles", "tendrils", "fringe"]
    const counters = ({})
    for (const l of d.lines) {
        const p = paint(l.colour)
        const index = counters[l.kind] = (counters[l.kind] || 0) + 1
        const lit = carved.indexOf(l.kind) < 0 && raisedDetail.indexOf(l.kind) < 0 && tubes.indexOf(l.kind) < 0 && l.kind !== "dotgrid"
        const base = { role: "line", kind: l.kind, stack: true, opacity: (l.opacity === undefined ? 1 : l.opacity) * p.o }
        if (l.dots) {
            for (let k = 0; k < l.points.length; ++k) {
                const pt = l.points[k], r = Math.max(0.8, l.width) * (l.sizes ? l.sizes[k] : 1) * (l.plain ? 1 : 1.1)
                const glowing = !(l.kind === "dots" || l.plain)
                elements.push(Object.assign({}, base, {
                    type: "dot", centre: [round1(pt.x), round1(pt.y)], radius: r, albedo: { colour: p.c, alpha: 1 },
                    profile: l.kind === "dots" ? "dome" : "flat", height: 0.1, metal: l.kind === "dots" ? 0.8 : 0, rough: 0.3,
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
        if (tubes.indexOf(l.kind) >= 0) {
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
    return result
}
