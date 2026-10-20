.pragma library

// Holograms: the icons, the panel and its stands drawn as light. One place
// for their colour sets and for reading each part's settings from a panel's
// values, shared by the flat painter, the 3D scene and Panel Studio.
//
// The look follows open-source hologram shaders: the see-through body edged
// brighter, moving scan lines and a signal's blink from Anderson Mancini's
// holographic material for three.js (MIT) and the Godot "SciFi Hologram"
// shader (MIT); the projector's light, soft at its sides and fading as it
// rises, from the Godot "Spatial Light Shaft" shader (CC0), its rays
// drifting sideways in two layers as in the Godot "God Rays" shader (CC0).

// Each set's colours: the outline (the edges), the inline (the inner
// lines), the volume (the body), the glow round it, and the projector's
// light and opening. Original keeps each part's own colours instead: an
// icon's, the panel's look's, a stand's material's, and the light under an
// icon takes the icon's main colour.
var colourSets = {
    cyan: { outline: "#ffffff", inline: "#e6fdff", volume: "#5aeaff", glow: "#22d8ff",
            light: "#22d8ff", opening: "#22d8ff" },
    ice: { outline: "#ffffff", inline: "#f4fbff", volume: "#e8f6ff", glow: "#3fb0ff",
           light: "#3fb0ff", opening: "#3fb0ff" },
    emerald: { outline: "#f2fff7", inline: "#c8ffe0", volume: "#4cf5a2", glow: "#14d070",
               light: "#18e07a", opening: "#2fe08a" },
    amber: { outline: "#fffaf0", inline: "#ffe8b8", volume: "#ffc04a", glow: "#ff9410",
             light: "#ff9d1a", opening: "#ffb23a" },
    ruby: { outline: "#fff2f4", inline: "#ffc8d0", volume: "#ff5a70", glow: "#ff1a3a",
            light: "#ff2a48", opening: "#ff3a58" },
    violet: { outline: "#faf5ff", inline: "#e4d0ff", volume: "#b080ff", glow: "#7a3dff",
              light: "#8a4dff", opening: "#9a5cff" },
    rose: { outline: "#fff5fa", inline: "#ffd0ec", volume: "#ff70c8", glow: "#ff2a9a",
            light: "#ff3aa8", opening: "#ff5ac0" }
}
var setNames = ["original", "cyan", "ice", "emerald", "amber", "ruby", "violet", "rose"]
var beamMotions = ["rise-drift", "rise", "pulse", "sweep", "still"]
var turnings = ["turn", "sway", "still"]

// The parts with a Hologram tab, by their settings' prefix.
var parts = ["Icon", "Panel", "Pedestal"]

function hexColour(value) {
    const text = String(value || "").trim()
    return /^#(?:[0-9a-f]{3}|[0-9a-f]{6}|[0-9a-f]{8})$/i.test(text) ? text : ""
}

function number(values, key, fallback, minimum, maximum) {
    const value = Number(values[key] === undefined ? fallback : values[key])
    return Math.max(minimum, Math.min(maximum, isFinite(value) ? value : fallback))
}

function choice(values, key, choices, fallback) {
    const value = String(values[key] || fallback).toLowerCase()
    return choices.indexOf(value) >= 0 ? value : fallback
}

// Whether a part is drawn as a hologram ("Icon", "Panel" or "Pedestal").
function partOn(values, part) {
    const v = values || ({})
    if (part === "Icon") return v.hologramIcons === true
    if (part === "Panel") return v.hologramPanel === true
    return v.hologramPedestals === true
        && String(v.pedestalShape || "none").toLowerCase() !== "none"
}

// A part's colours, each { colour, own }: `own` where the part keeps its own
// colours there (Original with no colour chosen for it), `colour` the one
// it is drawn in otherwise. The icons' also give the projector's light and
// opening.
function colours(values, part) {
    const v = values || ({})
    const prefix = "hologram" + part
    const setName = choice(v, prefix + "Colours", setNames, "original")
    const set = colourSets[setName] || null
    const pick = function(key, role) {
        const chosen = hexColour(v[key])
        if (chosen.length) return { colour: chosen, own: false }
        return set ? { colour: set[role], own: false } : { colour: "#ffffff", own: true }
    }
    const result = {
        set: setName,
        outline: pick(prefix + "OutlineColor", "outline"),
        inline: pick(prefix + "InlineColor", "inline"),
        volume: pick(prefix + "VolumeColor", "volume"),
        glow: pick(prefix + "GlowColor", "glow")
    }
    if (part === "Icon") {
        result.light = pick("hologramLightColor", "light")
        result.opening = pick("hologramOpeningColor", "opening")
    }
    return result
}

// A part's settings: whether it is a hologram, its glow, flicker and speed;
// the icons' also their float, their turning on a 3D panel and the
// projector's light under them (`beam`: drawn; its strength, height and
// motion).
function settings(values, part) {
    const v = values || ({})
    const prefix = "hologram" + part
    const result = {
        on: partOn(v, part),
        glow: number(v, prefix + "Glow", 1, 0, 1.5),
        glowReach: number(v, prefix + "GlowReach", 0.75, 0, 1),
        flicker: number(v, prefix + "Flicker", 1, 0, 2),
        speed: number(v, prefix + "Speed", 1, 0.1, 3)
    }
    if (part === "Icon") {
        result.float = v.hologramIconFloat !== false
        result.turning = choice(v, "hologramIconTurning", turnings, "turn")
        result.beam = result.on && v.hologramBeam !== false
        result.beamStrength = number(v, "hologramBeamStrength", 1, 0.2, 1.6)
        result.beamHeight = number(v, "hologramBeamHeight", 1, 0.2, 2)
        result.beamMotion = choice(v, "hologramBeamMotion", beamMotions, "rise-drift")
    }
    return result
}

// The light's motion for its shader: whether its streaks rise, its rays
// drift sideways, rings of light rise through it, a bright band sweeps
// across it; all four still for Still.
function beamMotionFlags(motion) {
    const word = String(motion || "rise-drift")
    return {
        rise: word === "rise-drift" || word === "rise" ? 1 : 0,
        drift: word === "rise-drift" ? 1 : 0,
        pulse: word === "pulse" ? 1 : 0,
        sweep: word === "sweep" ? 1 : 0,
        moving: word !== "still"
    }
}
