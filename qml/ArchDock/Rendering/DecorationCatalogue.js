.pragma library

// The built-in panel decorations, the named materials they are made of, and
// the tree Panel Studio shows them in (group, subgroup, item).
//
// A decoration's `contour` is drawn by ContourFrames.js on any panel shape;
// DecorationScene.js turns it into the lit scene the decoration lighting
// draws. `material` is the body's material (an id from `materials`, with an
// optional tint and relief); `room` is the length a decoration needs past
// the icons at its start and end (a hilt, a broom's straw), in half-widths
// of the band; `folds` folds a paper body; `classic` marks the first
// designs, whose textures keep their own grain direction and scale.
// A material's `metal`, `rough` and `coat` say how it answers light
// (metalness, roughness, a clear coat); `grain: "vertical"` marks a picture
// photographed upright, which is turned to run along the band; `strip` a
// long picture laid along the band (the nebulae).
// `animation` lists what moves by default: lights pulse, flicker, twinkle,
// blink, crackle or cycle through colours; the outline waves, the picture
// drifts, light flows, parts sway (or float, with `mode: "float"`: rise and
// sink), footprints walk, rings spin.
//
// The materials are photographed real surfaces: ambientCG's CC0 1.0 sets
// (https://ambientcg.com), turned into each material's colour and height
// maps by tools/generate-material-textures.py; the nebulae are the James
// Webb Space Telescope's pictures (NASA, ESA, CSA, STScI), which STScI
// releases for free use as in the public domain with credit requested.
// materials/CREDITS.md lists every set.
//
// The frames follow Arwes (MIT, Romel Perez), the console bars the LCARS
// proportions of the R package lcars (MIT); the looks were drawn after the
// owner's references: film and game interfaces (Oblivion's screens by
// GMUNK, Territory Studio's work), Alienware's own desktop themes, the
// Harry Potter films' props, Star Trek's ships. No names, logos or crests
// of any of them are used.

var decorations = [
    { id: "alien-pods-green", name: "Alien Pods Green", family: "alien", classic: true,
      material: {id: "dark-honeycomb", relief: 0.55},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 1.5, depth: -0.55, chamfer: 0.5, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 1.5, depth: -0.55, chamfer: 0.5, anchor: "between"}]},
        ends: {kind: "chamfer", size: 1},
        lines: [{kind: "loop", inset: 0.22, width: 0.09}],
        pattern: "honeycomb",
        colours: {body: "#11161c", edge: "#36434f", light: "#3dff7a", accent: "#c8ffd9"}
      } },
    { id: "alien-pods-white", name: "Alien Pods White", family: "alien", classic: true,
      material: {id: "white-honeycomb", relief: 0.5},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 1.5, depth: -0.55, chamfer: 0.5, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 1.5, depth: -0.55, chamfer: 0.5, anchor: "between"}]},
        ends: {kind: "chamfer", size: 1},
        lines: [{kind: "loop", inset: 0.22, width: 0.09}],
        pattern: "honeycomb",
        colours: {body: "#e9edf1", edge: "#aeb8c2", light: "#19c6ff", accent: "#ffffff"}
      } },
    { id: "alien-pods-black", name: "Alien Pods Black", family: "alien", classic: true,
      material: {id: "dark-honeycomb", tint: "#6a7078", relief: 0.55},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 1.5, depth: -0.55, chamfer: 0.5, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 1.5, depth: -0.55, chamfer: 0.5, anchor: "between"}]},
        ends: {kind: "chamfer", size: 1},
        lines: [{kind: "loop", inset: 0.22, width: 0.09}],
        pattern: "honeycomb",
        colours: {body: "#07090c", edge: "#262b31", light: "#b44dff", accent: "#ffffff"}
      } },
    { id: "alien-legend", name: "Alien Legend", family: "alien", classic: true,
      material: {id: "hull-metal", tint: "#5d6672", relief: 0.35},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {taper: [1.15, 0.85]},
        inner: {taper: [0.85, 1.15]},
        ends: {start: {kind: "chamfer", size: 1.2}, end: {kind: "point", size: 2}},
        lines: [{kind: "loop", inset: 0.25, width: 0.1}],
        colours: {body: "#1c2127", edge: "#3b444f", light: "#00e5ff", accent: "#b2f5ff"}
      } },
    { id: "alien-area", name: "Alien Area", family: "alien", classic: true,
      material: {id: "carbon-weave", relief: 0.45},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.25},
        ends: {kind: "chamfer", size: 1, cut: {kind: "chamfer", size: 0.8}},
        lines: [{kind: "loop", inset: 0.18, width: 0.08}],
        pattern: "honeycomb",
        colours: {body: "#101317", edge: "#2a3038", light: "#ff3d71", accent: "#ffc1d3"}
      } },
    { id: "alien-talon", name: "Alien Talon", family: "alien", classic: true,
      material: {id: "obsidian", relief: 0.3},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 1.6, depth: 0.6, anchor: "between"}]},
        ends: {kind: "point", size: 1.6},
        lines: [
            {kind: "edge", side: "outer", width: 0.08},
            {kind: "middle", width: 0.04, dash: [0.5, 0.4], offset: -0.55, margin: 0.8}
        ],
        colours: {body: "#0f1112", edge: "#2b3034", light: "#39ff14", accent: "#b9ff9e"}
      } },
    { id: "alien-vents", name: "Alien Vents", family: "alien", classic: true,
      material: {id: "corrugated-steel", tint: "#7a8590", relief: 0.6},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 1.2, depth: -0.25, chamfer: 0.4, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 1.2, depth: -0.25, chamfer: 0.4, anchor: "between"}]},
        ends: {kind: "chamfer", size: 0.9},
        lines: [
            {kind: "slats", period: 0.45, amp: 0.25, size: 0.75, width: 0.05, colour: "#2c343c", margin: 0.9},
            {kind: "loop", inset: 0.15, width: 0.08}
        ],
        colours: {body: "#181c20", edge: "#323a43", light: "#ff6d00", accent: "#ffd0a6"}
      } },
    { id: "alien-core", name: "Alien Core", family: "alien", classic: true,
      material: {id: "carbon-weave", tint: "#555555", relief: 0.4},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {base: 0.6},
        inner: {base: 0.6},
        ends: {kind: "point", size: 1.8},
        lines: [
            {kind: "middle", width: 0.16, margin: 0.4},
            {kind: "edge", side: "outer", width: 0.05, colour: "#0b6b7a"},
            {kind: "edge", side: "inner", width: 0.05, colour: "#0b6b7a"}
        ],
        colours: {body: "#05080b", edge: "#0d2a30", light: "#18ffff", accent: "#c4ffff"}
      } },
    { id: "cyber-frame", name: "Cyber Frame", family: "sci-fi", classic: true,
      material: {id: "carbon-weave", tint: "#6f9aa6", relief: 0.4},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2.4, depth: 0.35, chamfer: 0.35, anchor: "between"}]},
        ends: {start: {kind: "chamfer", size: 0.8, inner: false}, end: {kind: "chamfer", size: 0.8, outer: false}},
        lines: [
            {kind: "brackets", inset: 0.12, width: 0.07, long: 3, short: 0.9},
            {kind: "edge", side: "inner", width: 0.05, dash: [0.5, 0.35]}
        ],
        pattern: "grid",
        colours: {body: "#06141a", edge: "#0f3a44", light: "#00f0ff", accent: "#9ff8ff"}
      } },
    { id: "neon-grid", name: "Neon Grid", family: "sci-fi", classic: true,
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {base: 0.8},
        inner: {base: 0.8},
        lines: [
            {kind: "edge", side: "outer", width: 0.08},
            {kind: "edge", side: "inner", width: 0.08},
            {kind: "middle", width: 0.06, dash: [0.6, 0.6], colour: "#ff8a00", margin: 0.3}
        ],
        pattern: "grid",
        colours: {body: "#02060a", edge: "#0b2630", light: "#18f0ff", accent: "#ff8a00"}
      } },
    { id: "mecha-plate", name: "Mecha Plate", family: "sci-fi", classic: true,
      material: {id: "diamond-plate", relief: 0.7},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.3},
        ends: {kind: "chamfer", size: 0.8, cut: {kind: "chamfer", size: 0.45}},
        lines: [
            {kind: "dots", side: "both", inset: 0.22, period: 1, width: 0.09, colour: "#c9ced3"},
            {kind: "edge", side: "outer", width: 0.06}
        ],
        pattern: "stripes",
        colours: {body: "#2b2f33", edge: "#14171a", light: "#ffcc00", accent: "#c9ced3"}
      } },
    { id: "holo-rail", name: "Holo Rail", family: "sci-fi", classic: true,
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {base: 0.45},
        inner: {base: 0.45},
        ends: {kind: "point", size: 2},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "dots", side: "middle", period: 0.6, width: 0.07}
        ],
        colours: {body: "#660a2a3a", edge: "#2a7d8f", light: "#7df9ff", accent: "#e0ffff"}
      } },
    { id: "circuit-trace", name: "Circuit Trace", family: "sci-fi", classic: true,
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 1.1, depth: 0.35, chamfer: 0, period: 2.2, anchor: "period"}]},
        inner: {features: [{kind: "step", length: 1.1, depth: 0.35, chamfer: 0, period: 2.2, phase: 0.5, anchor: "period"}]},
        lines: [
            {kind: "middle", offset: 0.5, width: 0.05},
            {kind: "middle", offset: -0.5, width: 0.05},
            {kind: "dots", side: "middle", period: 2.2, width: 0.13, colour: "#e8c547"}
        ],
        pattern: "grid",
        colours: {body: "#0b2b1a", edge: "#145c36", light: "#2bff88", accent: "#e8c547"}
      } },
    { id: "quantum-wave", name: "Quantum Wave", family: "sci-fi", classic: true,
      material: {id: "iridescent-shell", relief: 0.35},
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "wave", length: 2.3, depth: 0.3, period: 2.3, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 2.3, depth: 0.3, period: 2.3, phase: 0.5, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [
            {kind: "wave", amp: 0.45, period: 2.3, width: 0.06},
            {kind: "wave", amp: 0.45, period: 2.3, phase: 0.5, width: 0.06, colour: "#40c4ff"}
        ],
        colours: {body: "#120a24", edge: "#3a2470", light: "#b388ff", accent: "#40c4ff"}
      } },
    { id: "hex-hive", name: "Hex Hive", family: "sci-fi", classic: true,
      material: {id: "white-honeycomb", tint: "#c98a1a", relief: 0.6},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 1.2, depth: -0.45, chamfer: 0.6, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 1.2, depth: -0.45, chamfer: 0.6, anchor: "between"}]},
        ends: {kind: "chamfer", size: 1},
        lines: [{kind: "loop", inset: 0.18, width: 0.08}],
        pattern: "honeycomb",
        colours: {body: "#1d1405", edge: "#4a3510", light: "#ffb300", accent: "#ffe082"}
      } },
    { id: "plasma-conduit", name: "Plasma Conduit", family: "sci-fi", classic: true,
      animation: [{kind: "flow", speed: 0.5}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {base: 0.7},
        inner: {base: 0.7},
        ends: {kind: "pill"},
        lines: [
            {kind: "wave", amp: 0.45, period: 4.2, width: 0.08},
            {kind: "wave", amp: 0.45, period: 4.2, phase: 0.5, width: 0.07, colour: "#7c4dff"},
            {kind: "edge", side: "outer", width: 0.05, colour: "#5c2d91"},
            {kind: "edge", side: "inner", width: 0.05, colour: "#5c2d91"}
        ],
        colours: {body: "#0d0618", edge: "#3a1d5c", light: "#ff4dff", accent: "#7c4dff"}
      } },
    { id: "starship-console", name: "Starship Console", family: "starship", classic: true,
      material: {id: "console-plastic", relief: 0.2},
      animation: [{kind: "blink", period: 1.2}],
      contour: {
        segments: {kind: "icons", gap: 0.26, colours: ["#ff9c00", "#cc99cc", "#9999ff", "#ffcc99", "#cc6666", "#ffaa90"]},
        inner: {base: 0.75},
        ends: {cut: {kind: "square"}, start: {kind: "block", size: 1.6, depth: 1.9}, end: {kind: "pill"}},
        colours: {body: "#ff9c00", edge: "#000000", light: "#ff9c00", accent: "#cc99cc"}
      } },
    { id: "starship-bridge", name: "Starship Bridge", family: "starship", classic: true,
      material: {id: "console-plastic", relief: 0.2},
      animation: [{kind: "blink", period: 1.2}],
      contour: {
        segments: {kind: "icons", gap: 0.26, colours: ["#5577ff", "#33ccff", "#99ccff", "#6688cc", "#88aaff"]},
        inner: {base: 0.75},
        ends: {cut: {kind: "square"}, start: {kind: "block", size: 1.6, depth: 1.9}, end: {kind: "block", size: 1.6, depth: 1.9}},
        colours: {body: "#5577ff", edge: "#000000", light: "#5577ff", accent: "#33ccff"}
      } },
    { id: "starship-alert", name: "Starship Alert", family: "starship", classic: true,
      material: {id: "console-plastic", relief: 0.2},
      animation: [{kind: "blink", period: 1.2}],
      contour: {
        segments: {kind: "icons", gap: 0.26, colours: ["#cc3333", "#ff6666", "#993333", "#ff9999"]},
        inner: {base: 0.75},
        ends: {cut: {kind: "square"}, kind: "pill"},
        colours: {body: "#cc3333", edge: "#000000", light: "#cc3333", accent: "#ff6666"}
      } },
    { id: "starship-deck", name: "Starship Deck", family: "starship", classic: true,
      material: {id: "console-plastic", relief: 0.2},
      animation: [{kind: "blink", period: 1.2}],
      contour: {
        segments: {kind: "icons", gap: 0.26, colours: ["#ff9966", "#cc99cc", "#ffcc66", "#9999cc"]},
        inner: {base: 0.6},
        ends: {cut: {kind: "square"}, start: {kind: "block", size: 1.8, depth: 2.1}, end: {kind: "block", size: 1.8, depth: 2.1}},
        colours: {body: "#ff9966", edge: "#000000", light: "#ff9966", accent: "#cc99cc"}
      } },
    { id: "starship-hull", name: "Starship Hull", family: "starship", classic: true,
      material: {id: "hull-metal", relief: 0.4},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        ends: {kind: "pill"},
        lines: [
            {kind: "dots", side: "both", inset: 0.15, period: 2, width: 0.08, colour: "#ffffff"},
            {kind: "middle", width: 0.03, offset: 0.62, margin: 0.6}
        ],
        pattern: "grid",
        colours: {body: "#8a96a3", edge: "#4b5563", light: "#c6d0db", accent: "#ffffff"}
      } },
    { id: "starship-warp", name: "Starship Warp", family: "starship", classic: true,
      material: {id: "hull-metal", tint: "#3e4e62", relief: 0.35},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {base: 0.7},
        inner: {base: 0.7},
        ends: {kind: "pill"},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "ends", inset: 0.6, width: 0.4, colour: "#ff3b30"}
        ],
        colours: {body: "#1b2430", edge: "#2d3b4d", light: "#4fc3ff", accent: "#ff3b30"}
      } },
    { id: "starship-saucer", name: "Starship Saucer", family: "starship", classic: true,
      material: {id: "hull-metal", relief: 0.35},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "wave", length: 3.2, depth: 0.55, anchor: "icons"}]},
        inner: {features: [{kind: "wave", length: 3.2, depth: 0.55, anchor: "icons"}]},
        ends: {kind: "pill"},
        lines: [
            {kind: "edge", side: "outer", width: 0.06},
            {kind: "ends", inset: 0.5, width: 0.35, colour: "#ff5252"}
        ],
        colours: {body: "#b0bec5", edge: "#607d8b", light: "#9ad0ff", accent: "#ff5252"}
      } },
    { id: "wizard-arches", name: "Wizard Arches", family: "magic", classic: true,
      material: {id: "dark-walnut", relief: 0.45},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.5, depth: 1.25, anchor: "icons"}]},
        inner: {features: []},
        ends: {kind: "point", size: 1.6},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "spiral", width: 0.08, turns: 1.3, size: 1.5}
        ],
        colours: {body: "#2a1710", edge: "#5c3a1e", light: "#d4af37", accent: "#e9c86a"}
      } },
    { id: "wizard-emerald", name: "Wizard Emerald", family: "magic", classic: true,
      material: {id: "velvet", tint: "#2f7a5f", relief: 0.4},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.5, depth: 1.25, anchor: "icons"}]},
        inner: {features: []},
        ends: {kind: "point", size: 1.6},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "spiral", width: 0.08, turns: 1.3, size: 1.5}
        ],
        colours: {body: "#0b3d2e", edge: "#1f6b52", light: "#d9e2ec", accent: "#f0f4f8"}
      } },
    { id: "wizard-scarlet", name: "Wizard Scarlet", family: "magic", classic: true,
      material: {id: "red-leather", relief: 0.45},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.8, depth: 1.1, anchor: "icons"}]},
        inner: {features: [{kind: "arch", length: 2.8, depth: 0.6, anchor: "icons"}]},
        ends: {kind: "point", size: 1.4},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "spiral", width: 0.07, turns: 1.3, size: 1.3}
        ],
        colours: {body: "#5c0a0a", edge: "#8e1b1b", light: "#f2c14e", accent: "#ffe08a"}
      } },
    { id: "wizard-sapphire", name: "Wizard Sapphire", family: "magic", classic: true,
      material: {id: "velvet", tint: "#24387e", relief: 0.4},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.5, depth: 1.25, anchor: "icons"}]},
        inner: {features: []},
        ends: {kind: "point", size: 1.6},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "spiral", width: 0.08, turns: 1.3, size: 1.5},
            {kind: "stars", period: 1.2, width: 0.07, colour: "#e3ecff"}
        ],
        colours: {body: "#0d1b4c", edge: "#22357a", light: "#cd7f32", accent: "#e8b07a"}
      } },
    { id: "spellbook", name: "Spellbook", family: "magic", classic: true,
      material: {id: "parchment", relief: 0.5},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        ends: {kind: "square"},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "spiral", width: 0.07, turns: 1.6, size: 1.2},
            {kind: "middle", offset: -0.55, width: 0.03, dash: [0.9, 0.3], margin: 0.9}
        ],
        colours: {body: "#e8d9b0", edge: "#8b6b3d", light: "#7a5c2e", accent: "#7a5c2e"}
      } },
    { id: "magic-wand", name: "Magic Wand", family: "magic", classic: true,
      material: {id: "cherry-wood", relief: 0.45},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {taper: [1.25, 0.35]},
        inner: {taper: [1.25, 0.35]},
        ends: {start: {kind: "pill"}, end: {kind: "point", size: 1.2}},
        lines: [
            {kind: "stars", period: 1.2, width: 0.08},
            {kind: "edge", side: "outer", width: 0.04, colour: "#8d6e63"}
        ],
        colours: {body: "#3e2723", edge: "#6d4c41", light: "#fff59d", accent: "#ffffff"}
      } },
    { id: "potion-glass", name: "Potion Glass", family: "magic", classic: true,
      material: {id: "green-ice", tint: "#7fd8b0", relief: 0.45},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "wave", length: 1, depth: 0.25, period: 1, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 1, depth: 0.25, period: 1, phase: 0.5, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [{kind: "stars", period: 1.5, width: 0.08}, {kind: "edge", side: "outer", width: 0.05}],
        colours: {body: "#cc004d40", edge: "#26a69a", light: "#b9f6ca", accent: "#64ffda"}
      } },
    { id: "starry-night", name: "Starry Night", family: "magic", classic: true,
      material: {id: "velvet", tint: "#1d1850", relief: 0.35},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 3, depth: 0.6, anchor: "icons"}]},
        ends: {kind: "point", size: 1.4},
        lines: [
            {kind: "stars", period: 2, width: 0.07},
            {kind: "edge", side: "outer", width: 0.05, colour: "#c5cae9"}
        ],
        colours: {body: "#1a1446", edge: "#2f2a6b", light: "#fff9c4", accent: "#c5cae9"}
      } },
    { id: "runestone", name: "Runestone", family: "magic", classic: true,
      material: {id: "dark-slate", relief: 0.7},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.14},
        outer: {base: 1.08},
        inner: {base: 1.08},
        ends: {kind: "chamfer", size: 0.6, cut: {kind: "chamfer", size: 0.3}},
        lines: [{kind: "middle", width: 0.07, dash: [0.35, 0.5], margin: 0.8, offset: -0.62}],
        colours: {body: "#4a4f55", edge: "#2c3035", light: "#4fc3f7", accent: "#b3e5fc"}
      } },
    { id: "celtic-knot", name: "Celtic Knot", family: "magic", classic: true,
      material: {id: "brown-leather", relief: 0.45},
      animation: [{kind: "flow", speed: 0.5}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        ends: {kind: "pill"},
        lines: [
            {kind: "wave", amp: 0.6, period: 2, width: 0.07},
            {kind: "wave", amp: 0.6, period: 2, phase: 0.5, width: 0.07},
            {kind: "edge", side: "outer", width: 0.05},
            {kind: "edge", side: "inner", width: 0.05}
        ],
        colours: {body: "#2e1e10", edge: "#5a3d22", light: "#d7a35a", accent: "#f0c987"}
      } },
    { id: "owl-feather", name: "Owl Feather", family: "magic", classic: true,
      material: {id: "velvet", tint: "#8d6e63", relief: 0.35},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 0.45, depth: 0.18, period: 0.45, anchor: "period"}]},
        inner: {features: [{kind: "tooth", length: 0.45, depth: 0.18, period: 0.45, phase: 0.5, anchor: "period"}]},
        ends: {start: {kind: "pill"}, end: {kind: "point", size: 2}},
        lines: [{kind: "middle", width: 0.06, margin: 0.2, colour: "#f5f5f5"}],
        colours: {body: "#8d6e63", edge: "#5d4037", light: "#f5f5f5", accent: "#ffffff"}
      } },
    { id: "dragon-scale", name: "Dragon Scale", family: "magic", classic: true,
      material: {id: "serpent-skin", tint: "#b8432f", relief: 0.6},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 1, depth: 0.32, period: 1, anchor: "period"}]},
        inner: {features: []},
        ends: {start: {kind: "point", size: 1.2}, end: {kind: "point", size: 2.6}},
        lines: [{kind: "edge", side: "outer", width: 0.06}],
        pattern: "scales",
        colours: {body: "#3e0d0d", edge: "#7a1c1c", light: "#ff7043", accent: "#ffab91"}
      } },
    { id: "gothic-cathedral", name: "Gothic Cathedral", family: "magic", classic: true,
      material: {id: "azure-tiles", relief: 0.45},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.8, depth: 1.1, anchor: "icons"}]},
        inner: {features: [{kind: "arch", length: 2.8, depth: 0.6, anchor: "icons"}]},
        ends: {kind: "point", size: 1.4},
        lines: [{kind: "edge", side: "outer", width: 0.06}, {kind: "edge", side: "inner", width: 0.06}],
        colours: {body: "#1a237e", edge: "#3949ab", light: "#cfd8dc", accent: "#ffffff"}
      } },
    { id: "art-deco", name: "Art Deco", family: "style", classic: true,
      material: {id: "obsidian", relief: 0.25},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2, depth: 0.3, chamfer: 0, anchor: "between"}, {kind: "step", length: 1, depth: 0.6, chamfer: 0, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 2, depth: 0.3, chamfer: 0, anchor: "between"}, {kind: "step", length: 1, depth: 0.6, chamfer: 0, anchor: "between"}]},
        ends: {kind: "chamfer", size: 0.5},
        lines: [
            {kind: "edge", side: "outer", width: 0.06},
            {kind: "edge", side: "inner", width: 0.06},
            {kind: "middle", width: 0.03, margin: 0.4}
        ],
        colours: {body: "#0d0d0d", edge: "#3a3226", light: "#e6c26e", accent: "#f5e2b0"}
      } },
    { id: "steampunk-brass", name: "Steampunk Brass", family: "style", classic: true,
      material: {id: "polished-copper", relief: 0.45},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.2},
        ends: {kind: "pill", cut: {kind: "pill"}},
        lines: [
            {kind: "dots", side: "both", inset: 0.22, period: 0.9, width: 0.1, colour: "#f5d08a"},
            {kind: "edge", side: "outer", width: 0.06}
        ],
        colours: {body: "#5d3a1a", edge: "#2e1c0c", light: "#b87333", accent: "#f5d08a"}
      } },
    { id: "ocean-tide", name: "Ocean Tide", family: "style", classic: true,
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "wave", length: 1.4, depth: 0.35, period: 1.4, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [{kind: "wave", amp: 0.25, period: 2.8, width: 0.06, offset: 0.35}],
        colours: {body: "#01579b", edge: "#0277bd", light: "#e0f7fa", accent: "#b2ebf2"}
      } },
    { id: "candy-pop", name: "Candy Pop", family: "style", classic: true,
      material: {id: "console-plastic", relief: 0.2},
      animation: [{kind: "blink", period: 1.2}],
      contour: {
        segments: {kind: "icons", gap: 0.3, colours: ["#ff8fab", "#ffd166", "#06d6a0", "#8ecae6", "#c77dff"]},
        ends: {kind: "pill", cut: {kind: "pill"}},
        colours: {body: "#ff8fab", edge: "#ffffff", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "bamboo-grove", name: "Bamboo Grove", family: "style", classic: true,
      material: {id: "bamboo", relief: 0.6},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.12},
        outer: {base: 0.9},
        inner: {base: 0.9},
        ends: {kind: "square", cut: {kind: "chamfer", size: 0.15}},
        lines: [{kind: "edge", side: "outer", width: 0.05}, {kind: "middle", offset: 0.45, width: 0.04}],
        pattern: "stripes",
        colours: {body: "#6b8e23", edge: "#3d5214", light: "#d7e8a8", accent: "#eef6d5"}
      } },
    { id: "royal-crown", name: "Royal Crown", family: "style", classic: true,
      material: {id: "velvet", tint: "#5a1a9c", relief: 0.4},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 2.2, depth: 0.95, anchor: "icons"}]},
        ends: {kind: "point", size: 1.2},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "dots", side: "middle", offset: -0.62, period: 1.15, width: 0.1, colour: "#fff3e0"}
        ],
        colours: {body: "#4a148c", edge: "#7b1fa2", light: "#ffd54f", accent: "#fff3e0"}
      } },
    { id: "castle-battlement", name: "Castle Battlement", family: "style", classic: true,
      material: {id: "castle-stone", relief: 0.75},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 0.8, depth: 0.55, chamfer: 0, period: 1.6, anchor: "period"}]},
        lines: [{kind: "dots", side: "middle", offset: -0.6, period: 2.3, width: 0.1, colour: "#ffcc80"}],
        pattern: "grid",
        colours: {body: "#6d6a64", edge: "#3e3b37", light: "#ffcc80", accent: "#ffe0b2"}
      } },
    { id: "elven-vine", name: "Elven Vine", family: "style", classic: true,
      material: {id: "moss", tint: "#8fbf8f", relief: 0.5},
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "wave", length: 1.6, depth: 0.22, period: 1.6, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 1.6, depth: 0.22, period: 1.6, phase: 0.5, anchor: "period"}]},
        ends: {kind: "point", size: 1.4},
        lines: [
            {kind: "wave", amp: 0.55, period: 3, width: 0.06},
            {kind: "spiral", width: 0.06, turns: 1.2, size: 1.2}
        ],
        colours: {body: "#1b3a24", edge: "#2e5e3a", light: "#a5d6a7", accent: "#c5e1a5"}
      } },
    { id: "frost-crystal", name: "Frost Crystal", family: "magic", classic: true,
      material: {id: "green-ice", relief: 0.5},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 0.7, depth: 0.35, period: 0.7, anchor: "period"}, {kind: "tooth", length: 0.5, depth: 0.6, period: 1.3, phase: 0.3, anchor: "period"}]},
        inner: {features: [{kind: "tooth", length: 0.6, depth: 0.4, period: 0.9, anchor: "period"}]},
        ends: {kind: "point", size: 1.2},
        lines: [
            {kind: "edge", side: "outer", width: 0.05},
            {kind: "stars", period: 1, width: 0.07, colour: "#ffffff"}
        ],
        colours: {body: "#9fd8e0", edge: "#5fa8b8", light: "#e0ffff", accent: "#ffffff"}
      } },
    { id: "gilded-porcelain", name: "Gilded Porcelain", family: "magic", classic: true,
      material: {id: "gilded-porcelain", relief: 0.35},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.2, depth: 0.9, anchor: "icons"}]},
        inner: {features: []},
        ends: {kind: "pill"},
        lines: [
            {kind: "edge", side: "outer", width: 0.07},
            {kind: "edge", side: "inner", width: 0.07},
            {kind: "spiral", width: 0.07, turns: 1.3, size: 1.3}
        ],
        colours: {body: "#f5f2ea", edge: "#c9b98f", light: "#d4af37", accent: "#e9c86a"}
      } },
    { id: "enchanted-moss", name: "Enchanted Moss", family: "magic", classic: true,
      material: {id: "moss", relief: 0.7},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}],
      contour: {
        outer: {features: [{kind: "wave", length: 1.2, depth: 0.3, period: 1.2, anchor: "period"}, {kind: "wave", length: 0.7, depth: 0.15, period: 0.7, phase: 0.4, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 1, depth: 0.25, period: 1, phase: 0.5, anchor: "period"}]},
        ends: {kind: "point", size: 1.3},
        lines: [{kind: "stars", period: 1, width: 0.08}],
        colours: {body: "#2e5d2a", edge: "#1d3d1a", light: "#f4ff81", accent: "#ccff90"}
      } },
    { id: "serpent-coil", name: "Serpent Coil", family: "magic", classic: true,
      material: {id: "serpent-skin", tint: "#6fbf5f", relief: 0.6},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {taper: [1.1, 0.4], features: [{kind: "wave", length: 2.4, depth: 0.4, period: 2.4, anchor: "period"}]},
        inner: {taper: [1.1, 0.4], features: [{kind: "wave", length: 2.4, depth: 0.4, period: 2.4, phase: 0.5, anchor: "period"}]},
        ends: {start: {kind: "pill"}, end: {kind: "point", size: 2.2}},
        lines: [{kind: "edge", side: "outer", width: 0.04}],
        colours: {body: "#3f7d3a", edge: "#244a21", light: "#c6ff00", accent: "#e6ee9c"}
      } },
    { id: "agate-flow", name: "Agate Flow", family: "exotic", classic: true,
      material: {id: "blue-agate", relief: 0.35},
      animation: [],
      contour: {
        outer: {features: [{kind: "wave", length: 3, depth: 0.35, period: 3, anchor: "period"}, {kind: "wave", length: 1.1, depth: 0.12, period: 1.1, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 3, depth: 0.3, period: 3, phase: 0.3, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [],
        colours: {body: "#5d8aa8", edge: "#3b5b70", light: "#ffcc80", accent: "#ffffff"}
      } },
    { id: "volcanic-rift", name: "Volcanic Rift", family: "exotic", classic: true,
      material: {id: "volcanic-rock", relief: 0.65},
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "tooth", length: 0.9, depth: 0.35, period: 0.9, anchor: "period"}]},
        inner: {features: [{kind: "tooth", length: 0.8, depth: 0.3, period: 0.8, phase: 0.5, anchor: "period"}]},
        ends: {kind: "point", size: 1.2},
        lines: [{kind: "wave", amp: 0.12, period: 0.7, width: 0.08, offset: -0.6}],
        colours: {body: "#1b1210", edge: "#3a2620", light: "#ff6d00", accent: "#ffab40"}
      } },
    { id: "moroccan-mosaic", name: "Moroccan Mosaic", family: "exotic", classic: true,
      material: {id: "moroccan-mosaic", relief: 0.45},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.6, depth: 0.8, anchor: "icons"}]},
        inner: {features: [{kind: "arch", length: 2.6, depth: 0.4, anchor: "icons"}]},
        ends: {kind: "point", size: 1},
        lines: [{kind: "edge", side: "outer", width: 0.06}, {kind: "edge", side: "inner", width: 0.06}],
        colours: {body: "#e8c98a", edge: "#8a5a2b", light: "#c9a227", accent: "#f3d77a"}
      } },
    { id: "azure-palace", name: "Azure Palace", family: "exotic", classic: true,
      material: {id: "azure-tiles", relief: 0.5},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.1},
        outer: {features: [{kind: "step", length: 3, depth: 0.25, chamfer: 0.3, anchor: "icons"}]},
        ends: {kind: "square", cut: {kind: "square"}},
        lines: [{kind: "edge", side: "outer", width: 0.05}],
        colours: {body: "#3f6fb5", edge: "#1f3f75", light: "#ffffff", accent: "#e3f2fd"}
      } },
    { id: "starship-battle", name: "Starship Battle", family: "starship", classic: true,
      material: {id: "battle-plates", relief: 0.6},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.2},
        ends: {kind: "chamfer", size: 0.8, cut: {kind: "chamfer", size: 0.4}},
        lines: [
            {kind: "dots", side: "both", inset: 0.2, period: 0.85, width: 0.09, colour: "#cfd8dc"},
            {kind: "ends", inset: 0.55, width: 0.35, colour: "#ff3b30"}
        ],
        colours: {body: "#4b4b4b", edge: "#262626", light: "#ff8a65", accent: "#ff3b30"}
      } },
    { id: "starship-deckplate", name: "Starship Deck Plate", family: "starship", classic: true,
      material: {id: "perforated-deck", relief: 0.7},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 1, depth: -0.2, chamfer: 0.2, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 1, depth: -0.2, chamfer: 0.2, anchor: "between"}]},
        lines: [{kind: "edge", side: "outer", width: 0.06, dash: [0.4, 0.3]}],
        colours: {body: "#6d4c41", edge: "#3e2723", light: "#ffb74d", accent: "#ffe0b2"}
      } },
    { id: "obsidian-shard", name: "Obsidian Shard", family: "alien", classic: true,
      material: {id: "obsidian", relief: 0.3},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2.5, depth: 0.4, chamfer: 1.2, anchor: "icons"}]},
        inner: {features: [{kind: "step", length: 2.5, depth: 0.4, chamfer: 1.2, anchor: "icons"}]},
        ends: {kind: "point", size: 1.6},
        lines: [{kind: "edge", side: "outer", width: 0.04}, {kind: "edge", side: "inner", width: 0.04}],
        colours: {body: "#101014", edge: "#2a2a33", light: "#e0e0ff", accent: "#ffffff"}
      } },
    { id: "iridescent-shell", name: "Iridescent Shell", family: "sci-fi", classic: true,
      material: {id: "iridescent-shell", relief: 0.4},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.18},
        ends: {kind: "pill", cut: {kind: "pill"}},
        lines: [{kind: "edge", side: "outer", width: 0.04}],
        pattern: "scales",
        colours: {body: "#5b3fa0", edge: "#2d1b5c", light: "#80deea", accent: "#e1bee7"}
      } },
    { id: "golden-throne", name: "Golden Throne", family: "precious", classic: true,
      material: {id: "polished-gold"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 2.2, depth: 0.95, anchor: "icons"}]},
        ends: {kind: "point", size: 1.2},
        lines: [{kind: "edge", side: "outer", width: 0.06}, {kind: "edge", side: "inner", width: 0.06}],
        colours: {body: "#000000", edge: "#00000055", light: "#fff1b8", accent: "#fff1b8"}
      } },
    { id: "white-gold-halo", name: "White Gold Halo", family: "precious", classic: true,
      material: {id: "white-gold"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        ends: {kind: "pill"},
        lines: [
            {kind: "edge", side: "outer", width: 0.05},
            {kind: "edge", side: "inner", width: 0.05},
            {kind: "middle", offset: 0.7, width: 0.03},
            {kind: "middle", offset: -0.7, width: 0.03}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "rose-gold-bloom", name: "Rose Gold Bloom", family: "precious", classic: true,
      material: {id: "rose-gold"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "wave", length: 2.6, depth: 0.6, anchor: "icons"}]},
        inner: {features: [{kind: "wave", length: 2.6, depth: 0.3, anchor: "icons"}]},
        ends: {kind: "pill"},
        lines: [{kind: "edge", side: "outer", width: 0.05}],
        colours: {body: "#000000", edge: "#00000055", light: "#ffe0d8", accent: "#ffe0d8"}
      } },
    { id: "blue-gold-frost", name: "Blue Gold Frost", family: "precious", classic: true,
      material: {id: "blue-gold"},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}],
      contour: {
        outer: {features: [{kind: "tooth", length: 0.7, depth: 0.35, period: 0.7, anchor: "period"}, {kind: "tooth", length: 0.5, depth: 0.6, period: 1.3, phase: 0.3, anchor: "period"}]},
        inner: {features: [{kind: "tooth", length: 0.6, depth: 0.4, period: 0.9, anchor: "period"}]},
        ends: {kind: "point", size: 1.2},
        lines: [{kind: "stars", period: 0.9, width: 0.07, colour: "#ffffff"}],
        colours: {body: "#000000", edge: "#00000055", light: "#e3f0ff", accent: "#ffffff"}
      } },
    { id: "purple-gold-mystic", name: "Purple Gold Mystic", family: "precious", classic: true,
      material: {id: "purple-gold"},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.5, depth: 1.15, anchor: "icons"}]},
        inner: {features: []},
        ends: {kind: "point", size: 1.6},
        lines: [
            {kind: "edge", side: "outer", width: 0.06},
            {kind: "stars", period: 1, width: 0.07, colour: "#ffffff"},
            {kind: "spiral", width: 0.07, turns: 1.3, size: 1.4}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#f1dcff", accent: "#f1dcff"}
      } },
    { id: "black-gold-noir", name: "Black Gold Noir", family: "precious", classic: true,
      material: {id: "black-gold"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2, depth: 0.3, chamfer: 0, anchor: "between"}, {kind: "step", length: 1, depth: 0.6, chamfer: 0, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 2, depth: 0.3, chamfer: 0, anchor: "between"}, {kind: "step", length: 1, depth: 0.6, chamfer: 0, anchor: "between"}]},
        ends: {kind: "chamfer", size: 0.5},
        lines: [{kind: "edge", side: "outer", width: 0.05}, {kind: "edge", side: "inner", width: 0.05}],
        colours: {body: "#000000", edge: "#00000055", light: "#d9b25f", accent: "#d9b25f"}
      } },
    { id: "green-gold-leaf", name: "Green Gold Leaf", family: "precious", classic: true,
      material: {id: "green-gold"},
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "wave", length: 1.6, depth: 0.22, period: 1.6, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 1.6, depth: 0.22, period: 1.6, phase: 0.5, anchor: "period"}]},
        ends: {kind: "point", size: 1.4},
        lines: [
            {kind: "wave", amp: 0.55, period: 3, width: 0.05},
            {kind: "spiral", width: 0.06, turns: 1.2, size: 1.2}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#f4f7c4", accent: "#f4f7c4"}
      } },
    { id: "red-gold-ember", name: "Red Gold Ember", family: "precious", classic: true,
      material: {id: "red-gold"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        ends: {kind: "chamfer", size: 0.9},
        outer: {features: [{kind: "step", length: 2.4, depth: 0.3, chamfer: 0.3, anchor: "between"}]},
        lines: [{kind: "middle", offset: -0.62, width: 0.08, margin: 0.8}],
        colours: {body: "#000000", edge: "#00000055", light: "#ffb38a", accent: "#ffb38a"}
      } },
    { id: "grey-gold-frame", name: "Grey Gold Frame", family: "precious", classic: true,
      material: {id: "grey-gold"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2.4, depth: 0.3, chamfer: 0.3, anchor: "between"}]},
        ends: {start: {kind: "chamfer", size: 0.8, inner: false}, end: {kind: "chamfer", size: 0.8, outer: false}},
        lines: [{kind: "brackets", inset: 0.12, width: 0.06, long: 3, short: 0.9}],
        colours: {body: "#000000", edge: "#00000055", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "silver-moon", name: "Silver Moon", family: "precious", classic: true,
      material: {id: "silver"},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 3, depth: 0.5, anchor: "icons"}]},
        ends: {kind: "point", size: 2},
        lines: [
            {kind: "stars", period: 1.4, width: 0.07, colour: "#ffffff"},
            {kind: "edge", side: "outer", width: 0.04}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "platinum-edge", name: "Platinum Edge", family: "precious", classic: true,
      material: {id: "brushed-platinum"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2.4, depth: 0.3, chamfer: 0.3, anchor: "between"}]},
        ends: {start: {kind: "chamfer", size: 0.8, inner: false}, end: {kind: "chamfer", size: 0.8, outer: false}},
        lines: [{kind: "brackets", inset: 0.12, width: 0.06, long: 3, short: 0.9}],
        colours: {body: "#000000", edge: "#00000055", light: "#dff6ff", accent: "#dff6ff"}
      } },
    { id: "gold-leaf-scroll", name: "Gold Leaf Scroll", family: "precious", classic: true,
      material: {id: "gold-leaf"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        ends: {kind: "square"},
        lines: [
            {kind: "edge", side: "outer", width: 0.06},
            {kind: "edge", side: "inner", width: 0.06},
            {kind: "spiral", width: 0.07, turns: 1.6, size: 1.2}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#fff1b8", accent: "#fff1b8"}
      } },
    { id: "bright-gold-crown", name: "Bright Gold Crown", family: "precious", classic: true,
      material: {id: "bright-gold"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 2.2, depth: 0.95, anchor: "icons"}]},
        ends: {kind: "point", size: 1.2},
        lines: [{kind: "edge", side: "outer", width: 0.06}, {kind: "edge", side: "inner", width: 0.06}],
        colours: {body: "#000000", edge: "#00000055", light: "#fff8d0", accent: "#fff8d0"}
      } },
    { id: "diamond-facet", name: "Diamond Facet", family: "gem", classic: true,
      material: {id: "diamond"},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2.5, depth: 0.45, chamfer: 1.2, anchor: "icons"}]},
        inner: {features: [{kind: "step", length: 2.5, depth: 0.45, chamfer: 1.2, anchor: "icons"}]},
        ends: {kind: "point", size: 1.6},
        lines: [
            {kind: "edge", side: "outer", width: 0.04},
            {kind: "edge", side: "inner", width: 0.04},
            {kind: "stars", period: 1.2, width: 0.07, colour: "#ffffff"}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "crystal-spire", name: "Crystal Spire", family: "gem", classic: true,
      material: {id: "rock-crystal"},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 0.9, depth: 0.9, anchor: "between"}, {kind: "tooth", length: 0.6, depth: 0.5, period: 0.8, anchor: "period"}]},
        ends: {kind: "point", size: 1.4},
        lines: [
            {kind: "edge", side: "outer", width: 0.04},
            {kind: "stars", period: 1, width: 0.07, colour: "#ffffff"}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#eaf6ff", accent: "#ffffff"}
      } },
    { id: "amethyst-geode", name: "Amethyst Geode", family: "gem", classic: true,
      material: {id: "amethyst"},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        inner: {base: 1, features: [{kind: "tooth", length: 0.5, depth: 0.35, period: 0.5, anchor: "period"}]},
        outer: {features: [{kind: "wave", length: 3, depth: 0.35, anchor: "icons"}]},
        ends: {kind: "pill"},
        lines: [
            {kind: "edge", side: "outer", width: 0.05},
            {kind: "stars", period: 1.2, width: 0.07, colour: "#ffffff"}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#e6ccff", accent: "#ffffff"}
      } },
    { id: "sapphire-crown", name: "Sapphire Crown", family: "gem", classic: true,
      material: {id: "sapphire"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 2.2, depth: 0.95, anchor: "icons"}]},
        ends: {kind: "point", size: 1.2},
        lines: [{kind: "edge", side: "outer", width: 0.06}, {kind: "edge", side: "inner", width: 0.06}],
        colours: {body: "#000000", edge: "#00000055", light: "#cfe0ff", accent: "#cfe0ff"}
      } },
    { id: "ruby-cathedral", name: "Ruby Cathedral", family: "gem", classic: true,
      material: {id: "ruby"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.8, depth: 1.1, anchor: "icons"}]},
        inner: {features: [{kind: "arch", length: 2.8, depth: 0.6, anchor: "icons"}]},
        ends: {kind: "point", size: 1.4},
        lines: [{kind: "edge", side: "outer", width: 0.05}, {kind: "edge", side: "inner", width: 0.05}],
        colours: {body: "#000000", edge: "#00000055", light: "#ffd0d8", accent: "#ffd0d8"}
      } },
    { id: "emerald-arches", name: "Emerald Arches", family: "gem", classic: true,
      material: {id: "emerald"},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.5, depth: 1.15, anchor: "icons"}]},
        inner: {features: []},
        ends: {kind: "point", size: 1.6},
        lines: [
            {kind: "edge", side: "outer", width: 0.06},
            {kind: "stars", period: 1, width: 0.07, colour: "#ffffff"},
            {kind: "spiral", width: 0.07, turns: 1.3, size: 1.4}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#d0ffe6", accent: "#d0ffe6"}
      } },
    { id: "topaz-rays", name: "Topaz Rays", family: "gem", classic: true,
      material: {id: "topaz"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 0.8, depth: 0.7, period: 0.8, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [{kind: "edge", side: "outer", width: 0.04}],
        colours: {body: "#000000", edge: "#00000055", light: "#fff0c8", accent: "#fff0c8"}
      } },
    { id: "aquamarine-wave", name: "Aquamarine Wave", family: "gem", classic: true,
      material: {id: "aquamarine"},
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "wave", length: 1.4, depth: 0.35, period: 1.4, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [{kind: "wave", amp: 0.25, period: 2.8, width: 0.05, offset: 0.3}],
        colours: {body: "#000000", edge: "#00000055", light: "#e8fdff", accent: "#e8fdff"}
      } },
    { id: "pink-tourmaline-bloom", name: "Pink Tourmaline Bloom", family: "gem", classic: true,
      material: {id: "pink-tourmaline"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "wave", length: 2.6, depth: 0.6, anchor: "icons"}]},
        inner: {features: [{kind: "wave", length: 2.6, depth: 0.3, anchor: "icons"}]},
        ends: {kind: "pill"},
        lines: [{kind: "edge", side: "outer", width: 0.05}],
        colours: {body: "#000000", edge: "#00000055", light: "#ffe0f0", accent: "#ffe0f0"}
      } },
    { id: "peridot-vine", name: "Peridot Vine", family: "gem", classic: true,
      material: {id: "peridot"},
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "wave", length: 1.6, depth: 0.22, period: 1.6, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 1.6, depth: 0.22, period: 1.6, phase: 0.5, anchor: "period"}]},
        ends: {kind: "point", size: 1.4},
        lines: [
            {kind: "wave", amp: 0.55, period: 3, width: 0.05},
            {kind: "spiral", width: 0.06, turns: 1.2, size: 1.2}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#f4ffd0", accent: "#f4ffd0"}
      } },
    { id: "citrine-sun", name: "Citrine Sun", family: "gem", classic: true,
      material: {id: "citrine"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 0.8, depth: 0.7, period: 0.8, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [{kind: "edge", side: "outer", width: 0.04}],
        colours: {body: "#000000", edge: "#00000055", light: "#fff6cc", accent: "#fff6cc"}
      } },
    { id: "garnet-shard", name: "Garnet Shard", family: "gem", classic: true,
      material: {id: "garnet"},
      animation: [{kind: "twinkle", period: 2.4, amount: 0.7}, {kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2.5, depth: 0.45, chamfer: 1.2, anchor: "icons"}]},
        inner: {features: [{kind: "step", length: 2.5, depth: 0.45, chamfer: 1.2, anchor: "icons"}]},
        ends: {kind: "point", size: 1.6},
        lines: [
            {kind: "edge", side: "outer", width: 0.04},
            {kind: "edge", side: "inner", width: 0.04},
            {kind: "stars", period: 1.2, width: 0.07, colour: "#ffffff"}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#ffd0d8", accent: "#ffffff"}
      } },
    { id: "opal-dream", name: "Opal Dream", family: "gem", classic: true,
      material: {id: "opal-swirl"},
      animation: [],
      contour: {
        outer: {features: [{kind: "wave", length: 3, depth: 0.35, period: 3, anchor: "period"}, {kind: "wave", length: 1.1, depth: 0.12, period: 1.1, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 3, depth: 0.3, period: 3, phase: 0.3, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [],
        colours: {body: "#000000", edge: "#00000055", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "chrysocolla-ripple", name: "Chrysocolla Ripple", family: "gem", classic: true,
      material: {id: "chrysocolla"},
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "wave", length: 1.4, depth: 0.35, period: 1.4, anchor: "period"}]},
        ends: {kind: "pill"},
        lines: [{kind: "wave", amp: 0.25, period: 2.8, width: 0.05, offset: 0.3}],
        colours: {body: "#000000", edge: "#00000055", light: "#e0fff8", accent: "#e0fff8"}
      } },
    { id: "carnelian-band", name: "Carnelian Band", family: "gem", classic: true,
      material: {id: "carnelian"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.2},
        ends: {kind: "pill", cut: {kind: "pill"}},
        lines: [{kind: "edge", side: "outer", width: 0.04}],
        colours: {body: "#000000", edge: "#00000055", light: "#ffe0c8", accent: "#ffe0c8"}
      } },
    { id: "honey-onyx-talon", name: "Honey Onyx Talon", family: "gem", classic: true,
      material: {id: "honey-onyx"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "tooth", length: 1.6, depth: 0.6, anchor: "between"}]},
        ends: {kind: "point", size: 1.6},
        lines: [{kind: "edge", side: "outer", width: 0.05}],
        colours: {body: "#000000", edge: "#00000055", light: "#fff0c8", accent: "#fff0c8"}
      } },
    { id: "red-jasper-totem", name: "Red Jasper Totem", family: "gem", classic: true,
      material: {id: "red-jasper"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        segments: {kind: "icons", gap: 0.2},
        ends: {kind: "chamfer", size: 0.8, cut: {kind: "chamfer", size: 0.45}},
        lines: [{kind: "dots", side: "both", inset: 0.22, period: 1, width: 0.08, colour: "#ffe0d0"}],
        colours: {body: "#000000", edge: "#00000055", light: "#ffe0d0", accent: "#ffe0d0"}
      } },
    { id: "moss-agate-grove", name: "Moss Agate Grove", family: "gem", classic: true,
      material: {id: "moss-agate"},
      animation: [{kind: "flow", speed: 0.5}],
      contour: {
        outer: {features: [{kind: "wave", length: 1.6, depth: 0.22, period: 1.6, anchor: "period"}]},
        inner: {features: [{kind: "wave", length: 1.6, depth: 0.22, period: 1.6, phase: 0.5, anchor: "period"}]},
        ends: {kind: "point", size: 1.4},
        lines: [
            {kind: "wave", amp: 0.55, period: 3, width: 0.05},
            {kind: "spiral", width: 0.06, turns: 1.2, size: 1.2}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#e8ffe0", accent: "#e8ffe0"}
      } },
    { id: "nero-portoro", name: "Nero Portoro", family: "precious", classic: true,
      material: {id: "black-and-gold-marble"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "step", length: 2, depth: 0.3, chamfer: 0, anchor: "between"}, {kind: "step", length: 1, depth: 0.6, chamfer: 0, anchor: "between"}]},
        inner: {features: [{kind: "step", length: 2, depth: 0.3, chamfer: 0, anchor: "between"}, {kind: "step", length: 1, depth: 0.6, chamfer: 0, anchor: "between"}]},
        ends: {kind: "chamfer", size: 0.5},
        lines: [{kind: "edge", side: "outer", width: 0.05}, {kind: "edge", side: "inner", width: 0.05}],
        colours: {body: "#000000", edge: "#00000055", light: "#e6c26e", accent: "#e6c26e"}
      } },
    { id: "black-marble-cathedral", name: "Black Marble Cathedral", family: "gem", classic: true,
      material: {id: "black-marble"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "arch", length: 2.8, depth: 1.1, anchor: "icons"}]},
        inner: {features: [{kind: "arch", length: 2.8, depth: 0.6, anchor: "icons"}]},
        ends: {kind: "point", size: 1.4},
        lines: [{kind: "edge", side: "outer", width: 0.05}, {kind: "edge", side: "inner", width: 0.05}],
        colours: {body: "#000000", edge: "#00000055", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "white-marble-halo", name: "White Marble Halo", family: "gem", classic: true,
      material: {id: "white-marble"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.3}],
      contour: {
        ends: {kind: "pill"},
        lines: [
            {kind: "edge", side: "outer", width: 0.05},
            {kind: "edge", side: "inner", width: 0.05},
            {kind: "middle", offset: 0.7, width: 0.03},
            {kind: "middle", offset: -0.7, width: 0.03}
        ],
        colours: {body: "#000000", edge: "#00000055", light: "#d4af37", accent: "#d4af37"}
      } },
    { id: "warp-nacelle", name: "Warp Nacelle", family: "starship-inside",
      material: {id: "hull-metal", tint: "#d4d9e0"},
      animation: [{kind: "flow", speed: 1.5}, {kind: "pulse", period: 2.2, amount: 0.3}],
      contour: {
        outer: {base: 0.95},
        inner: {base: 0.95},
        ends: {start: {kind: "pill"}, end: {kind: "point", size: 2.4}},
        body: {round: true},
        layers: [
            {side: "outer", inset: 0.12, width: 0.24, fill: "#7fd8ff", glow: "#2fb4ff", dash: [1.2, 0.12], margin: 1.8},
            {side: "inner", inset: 0.12, width: 0.16, fill: "#7fd8ff", glow: "#2fb4ff", dash: [1.2, 0.12], margin: 1.8, opacity: 0.8}
        ],
        parts: [{kind: "ellipse", at: "start", s: 0.85, w: 1.6, h: 1.85, gradient: ["#ffe2cc", "#d81e10"], glow: "#ff3b1f", z: "front"}],
        lines: [
            {kind: "slats", period: 2.4, amp: 0, size: 0.95, width: 0.03, colour: "#00000044", margin: 1.8},
            {kind: "fins", span: [0.06, 0.28], period: 0.14, width: 0.05, material: {id: "hull-metal", tint: "#b8bec6"}, avoid: 0.95, margin: 2},
            {kind: "fins", span: [0.72, 0.94], period: 0.14, width: 0.05, material: {id: "hull-metal", tint: "#b8bec6"}, avoid: 0.95, margin: 2},
            {kind: "pipes", count: 1, offset: 0, width: 0.07, period: 1.2, material: {id: "black-metal"}, avoid: 0.95, margin: 2}
        ],
        colours: {body: "#c9ced6", edge: "#00000055", light: "#7fd8ff", accent: "#ff3b1f"}
      } },
    { id: "plasma-blade-blue", name: "Blue Plasma Blade", family: "energy",
      room: [4.7, 0.6],
      animation: [{kind: "flicker", period: 0.09, amount: 0.12}, {kind: "pulse", period: 2.4, amount: 0.15}],
      contour: {
        outer: {base: 0.4},
        inner: {base: 0.4},
        ends: {start: {kind: "square"}, end: {kind: "pill"}},
        body: {fill: "none", glow: "#2e8dff"},
        layers: [
            {side: "middle", width: 0.8, fill: "#2e8dff", opacity: 0.75, glow: "#2e8dff"},
            {side: "middle", width: 0.4, fill: "#f2fbff", glow: "#2e8dff"}
        ],
        parts: [
            {kind: "box", at: "start", s: -0.55, w: 1.1, h: 1.2, r: 0.1, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -0.06, w: 0.12, h: 0.78, fill: "#0d0f12", z: "front"},
            {kind: "box", at: "start", s: -2.45, w: 2.7, h: 0.92, r: 0.06, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.55, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.97, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -2.39, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -2.81, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -3.23, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.25, t: 0.36, w: 0.34, h: 0.18, r: 0.05, fill: "#ff4136", glow: "#ff4136", z: "front"},
            {kind: "box", at: "start", s: -4.15, w: 0.95, h: 1.06, r: 0.32, material: {id: "black-metal"}, stroke: "#000000aa", shade: "cylinder", z: "front"}
        ],
        colours: {body: "#2e8dff", edge: "#00000000", light: "#f2fbff", accent: "#2e8dff"}
      } },
    { id: "plasma-blade-green", name: "Green Plasma Blade", family: "energy",
      room: [4.7, 0.6],
      animation: [{kind: "flicker", period: 0.09, amount: 0.12}, {kind: "pulse", period: 2.4, amount: 0.15}],
      contour: {
        outer: {base: 0.4},
        inner: {base: 0.4},
        ends: {start: {kind: "square"}, end: {kind: "pill"}},
        body: {fill: "none", glow: "#2fd84a"},
        layers: [
            {side: "middle", width: 0.8, fill: "#2fd84a", opacity: 0.75, glow: "#2fd84a"},
            {side: "middle", width: 0.4, fill: "#f4fff2", glow: "#2fd84a"}
        ],
        parts: [
            {kind: "box", at: "start", s: -0.55, w: 1.1, h: 1.2, r: 0.1, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -0.06, w: 0.12, h: 0.78, fill: "#0d0f12", z: "front"},
            {kind: "box", at: "start", s: -2.45, w: 2.7, h: 0.92, r: 0.06, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.55, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.97, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -2.39, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -2.81, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -3.23, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.25, t: 0.36, w: 0.34, h: 0.18, r: 0.05, fill: "#ff4136", glow: "#ff4136", z: "front"},
            {kind: "box", at: "start", s: -4.15, w: 0.95, h: 1.06, r: 0.32, material: {id: "black-metal"}, stroke: "#000000aa", shade: "cylinder", z: "front"}
        ],
        colours: {body: "#2fd84a", edge: "#00000000", light: "#f4fff2", accent: "#2fd84a"}
      } },
    { id: "plasma-blade-red", name: "Red Plasma Blade", family: "energy",
      room: [4.7, 0.6],
      animation: [{kind: "flicker", period: 0.09, amount: 0.12}, {kind: "pulse", period: 2.4, amount: 0.15}],
      contour: {
        outer: {base: 0.4},
        inner: {base: 0.4},
        ends: {start: {kind: "square"}, end: {kind: "pill"}},
        body: {fill: "none", glow: "#ff2a1f"},
        layers: [
            {side: "middle", width: 0.8, fill: "#ff2a1f", opacity: 0.75, glow: "#ff2a1f"},
            {side: "middle", width: 0.4, fill: "#fff4f2", glow: "#ff2a1f"}
        ],
        parts: [
            {kind: "box", at: "start", s: -0.55, w: 1.1, h: 1.2, r: 0.1, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -0.06, w: 0.12, h: 0.78, fill: "#0d0f12", z: "front"},
            {kind: "box", at: "start", s: -2.45, w: 2.7, h: 0.92, r: 0.06, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.55, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.97, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -2.39, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -2.81, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -3.23, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.25, t: 0.36, w: 0.34, h: 0.18, r: 0.05, fill: "#ff4136", glow: "#ff4136", z: "front"},
            {kind: "box", at: "start", s: -4.15, w: 0.95, h: 1.06, r: 0.32, material: {id: "black-metal"}, stroke: "#000000aa", shade: "cylinder", z: "front"}
        ],
        colours: {body: "#ff2a1f", edge: "#00000000", light: "#fff4f2", accent: "#ff2a1f"}
      } },
    { id: "plasma-blade-violet", name: "Violet Plasma Blade", family: "energy",
      room: [4.7, 0.6],
      animation: [{kind: "flicker", period: 0.09, amount: 0.12}, {kind: "pulse", period: 2.4, amount: 0.15}],
      contour: {
        outer: {base: 0.4},
        inner: {base: 0.4},
        ends: {start: {kind: "square"}, end: {kind: "pill"}},
        body: {fill: "none", glow: "#a03cff"},
        layers: [
            {side: "middle", width: 0.8, fill: "#a03cff", opacity: 0.75, glow: "#a03cff"},
            {side: "middle", width: 0.4, fill: "#fbf2ff", glow: "#a03cff"}
        ],
        parts: [
            {kind: "box", at: "start", s: -0.55, w: 1.1, h: 1.2, r: 0.1, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -0.06, w: 0.12, h: 0.78, fill: "#0d0f12", z: "front"},
            {kind: "box", at: "start", s: -2.45, w: 2.7, h: 0.92, r: 0.06, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.55, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.97, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -2.39, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -2.81, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -3.23, w: 0.24, h: 1.02, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "start", s: -1.25, t: 0.36, w: 0.34, h: 0.18, r: 0.05, fill: "#ff4136", glow: "#ff4136", z: "front"},
            {kind: "box", at: "start", s: -4.15, w: 0.95, h: 1.06, r: 0.32, material: {id: "black-metal"}, stroke: "#000000aa", shade: "cylinder", z: "front"}
        ],
        colours: {body: "#a03cff", edge: "#00000000", light: "#fbf2ff", accent: "#a03cff"}
      } },
    { id: "twin-plasma-blade", name: "Twin Plasma Blade", family: "energy",
      animation: [{kind: "flicker", period: 0.08, amount: 0.12}, {kind: "pulse", period: 2, amount: 0.15}],
      contour: {
        outer: {base: 0.4},
        inner: {base: 0.4},
        ends: {kind: "pill"},
        body: {fill: "none", glow: "#ff2a1f"},
        layers: [
            {side: "middle", width: 0.8, fill: "#ff2a1f", opacity: 0.75, glow: "#ff2a1f"},
            {side: "middle", width: 0.4, fill: "#fff4f2", glow: "#ff2a1f"}
        ],
        parts: [
            {kind: "box", at: "middle", w: 3.6, h: 0.95, r: 0.08, material: {id: "black-metal"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "middle", s: -1.75, w: 0.7, h: 1.2, r: 0.1, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "middle", s: 1.75, w: 0.7, h: 1.2, r: 0.1, material: {id: "brushed-platinum"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "middle", s: -1.18, w: 0.22, h: 1.04, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"},
            {kind: "box", at: "middle", s: 1.18, w: 0.22, h: 1.04, r: 0.05, material: {id: "rubber-grip"}, stroke: "#000000aa", shade: "cylinder", z: "front"}
        ],
        colours: {body: "#ff2a1f", edge: "#00000000", light: "#fff4f2", accent: "#ff2a1f"}
      } },
    { id: "laser-gate", name: "Laser Gate", family: "energy",
      room: [1.2, 1.2],
      animation: [{kind: "flicker", period: 0.12, amount: 0.25}, {kind: "pulse", period: 0.9, amount: 0.3}],
      contour: {
        outer: {base: 0.9},
        inner: {base: 0.9},
        ends: {kind: "square"},
        body: {glow: "#ff2a2a", opacity: 0.32},
        parts: [
            {kind: "box", at: "both", s: -0.45, w: 0.9, h: 2.9, r: 0.1, material: {id: "black-metal"}, stroke: "#00000099", shade: "cylinder", z: "front"},
            {kind: "box", at: "both", s: -0.04, w: 0.14, h: 2.2, fill: "#ff8080", glow: "#ff2a2a", z: "front"}
        ],
        lines: [
            {kind: "middle", offset: 0.6, width: 0.06, colour: "#ffc0c0", margin: 0},
            {kind: "middle", offset: 0, width: 0.06, colour: "#ffc0c0", margin: 0},
            {kind: "middle", offset: -0.6, width: 0.06, colour: "#ffc0c0", margin: 0}
        ],
        colours: {body: "#ff3030", edge: "#ff6060", light: "#ffc0c0", accent: "#ff2a2a"}
      } },
    { id: "plasma-arc", name: "Plasma Arc", family: "energy",
      room: [1.3, 1.3],
      animation: [{kind: "crackle", period: 0.11}, {kind: "flicker", period: 0.07, amount: 0.35}],
      contour: {
        outer: {base: 0.9},
        inner: {base: 0.9},
        ends: {kind: "pill"},
        body: {fill: "none"},
        layers: [{side: "middle", width: 1.7, fill: "#3b7bff", opacity: 0.12}],
        parts: [
            {kind: "box", at: "both", s: -0.45, on: "inner", t: -0.2, w: 0.36, h: 1.35, material: {id: "black-metal"}, stroke: "#00000099", z: "front"},
            {kind: "ellipse", at: "both", s: -0.45, w: 1.6, h: 1.6, material: {id: "brushed-platinum"}, stroke: "#00000088", shade: "dome", z: "front"}
        ],
        lines: [
            {kind: "lightning", bolts: 4, amp: 0.75, detail: 7, forks: 3, width: 0.11, colour: "#e8f0ff", margin: 0.2, keep: 1.25},
            {kind: "lightning", bolts: 2, amp: 0.9, detail: 6, forks: 1, width: 0.2, colour: "#7aa8ff80", margin: 0.2, keep: 1.25, seed: 61}
        ],
        colours: {body: "#3b7bff", edge: "#00000000", light: "#dbe8ff", accent: "#3b7bff"}
      } },
    { id: "force-field", name: "Force Field", family: "energy",
      animation: [{kind: "pulse", period: 2.2, amount: 0.3}],
      contour: {
        outer: {features: [{kind: "wave", anchor: "middle", length: 34, depth: 0.2}]},
        inner: {features: [{kind: "wave", anchor: "middle", length: 34, depth: 0.2}]},
        ends: {kind: "pill"},
        body: {opacity: 0.25, glow: "#4fc3ff"},
        parts: [{kind: "ellipse", at: "both", s: 0.55, w: 0.6, h: 0.6, fill: "#dff6ff", glow: "#4fc3ff", z: "front"}],
        lines: [
            {kind: "hexgrid", size: 0.32, width: 0.03, colour: "#9fe0ff"},
            {kind: "edge", side: "outer", width: 0.06, colour: "#cfeeff"},
            {kind: "edge", side: "inner", width: 0.06, colour: "#cfeeff"}
        ],
        colours: {body: "#2a7fd4", edge: "#9fe0ff", light: "#cfeeff", accent: "#4fc3ff"}
      } },
    { id: "carina-nebula", name: "Carina Nebula", family: "galactic",
      material: {id: "carina-nebula"},
      animation: [{kind: "waves", speed: 0.3}, {kind: "drift", speed: 0.12}, {kind: "twinkle", period: 2.6, amount: 0.7}],
      contour: {
        outer: {base: 0.82, rough: {amp: 0.13, scale: 1.2}, features: [{kind: "wave", period: 6.2, length: 5.5, depth: 0.42, vary: 0.7}, {kind: "wave", period: 3.1, length: 3, depth: 0.25, phase: 0.31, vary: 0.8}, {kind: "wave", period: 1.6, length: 1.5, depth: 0.12, phase: 0.2, vary: 0.9}, {kind: "wave", period: 7.7, length: 3.2, depth: -0.28, phase: 0.55, vary: 0.5}]},
        inner: {base: 0.78, rough: {amp: 0.12, scale: 1.5, seed: 4}, features: [{kind: "wave", period: 5.1, length: 4.6, depth: 0.34, phase: 0.15, vary: 0.7}, {kind: "wave", period: 2.3, length: 2, depth: 0.14, phase: 0.6, vary: 0.85}, {kind: "wave", period: 8.3, length: 3.4, depth: -0.24, phase: 0.1, vary: 0.5}]},
        ends: {kind: "pill"},
        body: {feather: 0.62},
        lines: [{kind: "stars", period: 0.45, width: 0.045, colour: "#ffffff"}],
        colours: {body: "#101320", edge: "#00000000", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "pillars-of-creation", name: "Pillars of Creation", family: "galactic",
      material: {id: "pillars-of-creation"},
      animation: [{kind: "waves", speed: 0.3}, {kind: "drift", speed: 0.12}, {kind: "twinkle", period: 2.6, amount: 0.7}],
      contour: {
        outer: {base: 0.9, rough: {amp: 0.1, scale: 1.1}, features: [{kind: "round", anchor: "at", at: [0.22, 0.41, 0.69], length: 1.5, depth: 0.5, chamfer: 0.7, vary: 0.4}, {kind: "wave", period: 3, length: 3, depth: 0.15, vary: 0.8}]},
        inner: {base: 0.78, rough: {amp: 0.12, scale: 1.5, seed: 4}, features: [{kind: "wave", period: 5.1, length: 4.6, depth: 0.34, phase: 0.15, vary: 0.7}, {kind: "wave", period: 2.3, length: 2, depth: 0.14, phase: 0.6, vary: 0.85}, {kind: "wave", period: 8.3, length: 3.4, depth: -0.24, phase: 0.1, vary: 0.5}]},
        ends: {kind: "pill"},
        body: {feather: 0.62},
        lines: [{kind: "stars", period: 0.45, width: 0.045, colour: "#ffffff"}],
        colours: {body: "#101320", edge: "#00000000", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "southern-ring-nebula", name: "Southern Ring Nebula", family: "galactic",
      material: {id: "southern-ring-nebula"},
      animation: [{kind: "waves", speed: 0.3}, {kind: "drift", speed: 0.12}, {kind: "twinkle", period: 2.6, amount: 0.7}],
      contour: {
        outer: {base: 0.82, rough: {amp: 0.13, scale: 1.2}, features: [{kind: "wave", period: 6.2, length: 5.5, depth: 0.42, vary: 0.7}, {kind: "wave", period: 3.1, length: 3, depth: 0.25, phase: 0.31, vary: 0.8}, {kind: "wave", period: 1.6, length: 1.5, depth: 0.12, phase: 0.2, vary: 0.9}, {kind: "wave", period: 7.7, length: 3.2, depth: -0.28, phase: 0.55, vary: 0.5}]},
        inner: {base: 0.78, rough: {amp: 0.12, scale: 1.5, seed: 4}, features: [{kind: "wave", period: 5.1, length: 4.6, depth: 0.34, phase: 0.15, vary: 0.7}, {kind: "wave", period: 2.3, length: 2, depth: 0.14, phase: 0.6, vary: 0.85}, {kind: "wave", period: 8.3, length: 3.4, depth: -0.24, phase: 0.1, vary: 0.5}]},
        ends: {kind: "pill"},
        body: {feather: 0.62},
        lines: [{kind: "stars", period: 0.45, width: 0.045, colour: "#ffffff"}],
        colours: {body: "#101320", edge: "#00000000", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "tarantula-nebula", name: "Tarantula Nebula", family: "galactic",
      material: {id: "tarantula-nebula"},
      animation: [{kind: "waves", speed: 0.3}, {kind: "drift", speed: 0.12}, {kind: "twinkle", period: 2.6, amount: 0.7}],
      contour: {
        outer: {base: 0.85, rough: {amp: 0.18, scale: 0.8}, features: [{kind: "wave", period: 2.4, length: 2.6, depth: 0.3, vary: 0.85}, {kind: "tooth", period: 3.7, length: 1.6, depth: 0.25, vary: 0.8, curve: 1.6}]},
        inner: {base: 0.78, rough: {amp: 0.12, scale: 1.5, seed: 4}, features: [{kind: "wave", period: 5.1, length: 4.6, depth: 0.34, phase: 0.15, vary: 0.7}, {kind: "wave", period: 2.3, length: 2, depth: 0.14, phase: 0.6, vary: 0.85}, {kind: "wave", period: 8.3, length: 3.4, depth: -0.24, phase: 0.1, vary: 0.5}]},
        ends: {kind: "pill"},
        body: {feather: 0.62},
        lines: [{kind: "stars", period: 0.45, width: 0.045, colour: "#ffffff"}],
        colours: {body: "#101320", edge: "#00000000", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "deep-field", name: "Deep Field", family: "galactic",
      material: {id: "deep-field"},
      animation: [{kind: "waves", speed: 0.25}, {kind: "drift", speed: 0.06}, {kind: "twinkle", period: 3, amount: 0.6}],
      contour: {
        outer: {base: 0.82, rough: {amp: 0.13, scale: 1.2}, features: [{kind: "wave", period: 6.2, length: 5.5, depth: 0.42, vary: 0.7}, {kind: "wave", period: 3.1, length: 3, depth: 0.25, phase: 0.31, vary: 0.8}, {kind: "wave", period: 1.6, length: 1.5, depth: 0.12, phase: 0.2, vary: 0.9}, {kind: "wave", period: 7.7, length: 3.2, depth: -0.28, phase: 0.55, vary: 0.5}]},
        inner: {base: 0.78, rough: {amp: 0.12, scale: 1.5, seed: 4}, features: [{kind: "wave", period: 5.1, length: 4.6, depth: 0.34, phase: 0.15, vary: 0.7}, {kind: "wave", period: 2.3, length: 2, depth: 0.14, phase: 0.6, vary: 0.85}, {kind: "wave", period: 8.3, length: 3.4, depth: -0.24, phase: 0.1, vary: 0.5}]},
        ends: {kind: "pill"},
        body: {feather: 0.62},
        lines: [{kind: "stars", period: 0.2, width: 0.035, colour: "#ffffff"}],
        colours: {body: "#101320", edge: "#00000000", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "alien-chassis-lunar", name: "Alien Chassis Lunar", family: "alien",
      material: {id: "white-honeycomb"},
      room: [0, 3.6],
      animation: [{kind: "pulse", period: 3, amount: 0.45}],
      contour: {
        outer: {base: 1.05},
        inner: {base: 1.05},
        ends: {kind: "pill"},
        body: {round: true},
        layers: [
            {side: "inner", inset: 0.14, width: 0.3, material: {id: "dark-honeycomb"}, margin: 1.2},
            {side: "outer", inset: 0.1, width: 0.06, fill: "#5ff0ff", glow: "#21d4ff", margin: 1.2}
        ],
        parts: [
            {kind: "capsule", at: "end", s: 1.7, w: 2.9, h: 1.85, fill: "#c7f8ff", glow: "#21d4ff", z: "front"},
            {kind: "capsule", at: "end", s: 1.7, w: 2.5, h: 1.45, material: {id: "dark-honeycomb"}, stroke: "#000000aa", z: "front"}
        ],
        colours: {body: "#e9edf2", edge: "#00000044", light: "#5ff0ff", accent: "#21d4ff"}
      } },
    { id: "alien-chassis-dark", name: "Alien Chassis Dark", family: "alien",
      material: {id: "dark-honeycomb"},
      room: [0, 3.6],
      animation: [{kind: "pulse", period: 3, amount: 0.45}, {kind: "cycle", period: 8}],
      contour: {
        outer: {base: 1.05},
        inner: {base: 1.05},
        ends: {kind: "pill"},
        body: {round: true},
        layers: [
            {side: "inner", inset: 0.14, width: 0.3, material: {id: "carbon-weave"}, margin: 1.2},
            {side: "outer", inset: 0.1, width: 0.06, fill: "#c070ff", glow: "#8a2dff", margin: 1.2}
        ],
        parts: [
            {kind: "capsule", at: "end", s: 1.7, w: 2.9, h: 1.85, fill: "#e2b8ff", glow: "#8a2dff", z: "front"},
            {kind: "capsule", at: "end", s: 1.7, w: 2.5, h: 1.45, material: {id: "carbon-weave"}, stroke: "#000000aa", z: "front"}
        ],
        colours: {body: "#22252b", edge: "#00000066", light: "#c070ff", accent: "#8a2dff"}
      } },
    { id: "runic-stone", name: "Runic Stone", family: "runes",
      material: {id: "dark-slate"},
      animation: [{kind: "pulse", period: 3.2, amount: 0.55}],
      contour: {
        outer: {rough: {amp: 0.08, scale: 1}},
        inner: {rough: {amp: 0.06, scale: 1.3, seed: 5}},
        ends: {kind: "broken", size: 1},
        lines: [
            {kind: "runes", size: 0.85, period: 0.75, width: 0.07, colour: "#6fe7ff", avoid: 1, margin: 0.9},
            {kind: "cracks", count: 6, length: 1.2, width: 0.04, colour: "#0b0d10"}
        ],
        colours: {body: "#3c4046", edge: "#00000088", light: "#6fe7ff", accent: "#6fe7ff"}
      } },
    { id: "twisted-vine", name: "Twisted Vine", family: "vines",
      animation: [{kind: "sway", period: 4.5, amount: 0.2}],
      contour: {
        outer: {base: 0.9},
        inner: {base: 0.9},
        ends: {kind: "pill"},
        body: {fill: "none"},
        lines: [
            {kind: "plait", strands: 3, amp: 0.55, period: 6.5, width: 0.42, material: {id: "brown-bark"}, colour: "#6b4a2e", outline: "#1f140b", outlineWidth: 0.05, margin: 0.2, gather: 2.5},
            {kind: "tendrils", period: 3.1, size: 0.9, width: 0.05, colour: "#6f6a3a", margin: 1.5}
        ],
        colours: {body: "#4a3423", edge: "#00000000", light: "#6f6a3a", accent: "#6f6a3a"}
      } },
    { id: "grapevine", name: "Grapevine", family: "vines",
      material: {id: "cedar-bark"},
      room: [0.4, 1],
      animation: [{kind: "sway", period: 4, amount: 0.2}],
      contour: {
        outer: {base: 0.55, rough: {amp: 0.09, scale: 0.9}},
        inner: {base: 0.55, rough: {amp: 0.09, scale: 1.1, seed: 9}},
        sway: {amp: 0.32, period: 7},
        ends: {start: {kind: "broken", size: 0.8}, end: {kind: "point", size: 2}},
        body: {round: true},
        parts: [{kind: "branch", at: "between", on: "outer", alternate: true, angle: 35, length: 1.7, width: 0.28, tip: 0.04, bend: 25, wobble: 1, material: true, stroke: "#00000055"}],
        lines: [{kind: "tendrils", period: 2.3, size: 0.85, width: 0.045, colour: "#7d7440", margin: 1.2}],
        colours: {body: "#6a5636", edge: "#00000077", light: "#7d7440", accent: "#7d7440"}
      } },
    { id: "jungle-liana", name: "Jungle Liana", family: "vines",
      animation: [{kind: "sway", period: 5, amount: 0.12}],
      contour: {
        outer: {base: 0.95},
        inner: {base: 0.95},
        ends: {kind: "pill"},
        body: {fill: "none"},
        layers: [{side: "middle", offset: 0.35, width: 0.22, material: {id: "moss"}, dash: [1.4, 2.6], opacity: 0.9}],
        lines: [{kind: "plait", strands: 2, amp: 0.45, period: 9, width: 0.6, material: {id: "grey-bark"}, colour: "#6d6a5e", outline: "#1d1c18", outlineWidth: 0.05, margin: 0.2, gather: 3}],
        colours: {body: "#55524a", edge: "#00000000", light: "#6d6a5e", accent: "#6d6a5e"}
      } },
    { id: "oak-branch", name: "Oak Branch", family: "branches",
      material: {id: "oak-bark"},
      room: [0.5, 2.8],
      animation: [{kind: "sway", period: 4.2, amount: 0.08}],
      contour: {
        outer: {base: 0.62, taper: [1, 0.9], rough: {amp: 0.06, scale: 1.6}},
        inner: {base: 0.62, taper: [1, 0.9], rough: {amp: 0.06, scale: 1.9, seed: 7}},
        ends: {start: {kind: "square"}, end: {kind: "point", size: 1.4}},
        body: {round: true},
        parts: [
            {kind: "branch", at: "between", on: "outer", alternate: true, angle: 48, length: 2.6, width: 0.32, tip: 0.04, bend: 18, wobble: 1, material: true, stroke: "#00000066", forks: [{at: 0.55, angle: -40, length: 1}, {at: 0.8, angle: 30, length: 0.7}]},
            {kind: "branch", at: "end", s: 0.4, angle: -75, length: 2.3, width: 0.3, tip: 0.03, bend: -20, wobble: 1, material: true, stroke: "#00000066"},
            {kind: "branch", at: "end", s: 0.4, angle: -110, length: 1.8, width: 0.26, tip: 0.03, bend: 25, wobble: 1, material: true, stroke: "#00000066"},
            {kind: "ellipse", at: "start", s: 0.02, w: 0.5, h: 1.24, material: {id: "sawn-log-end", fit: true}, stroke: "#3a2a1a", z: "front"},
            {kind: "ellipse", at: "at", where: [0.31, 0.64], t: 0.15, w: 0.55, h: 0.38, fill: "#2d2116", stroke: "#6b5440", strokeWidth: 0.05, z: "front"}
        ],
        colours: {body: "#5a4a3a", edge: "#00000088", light: "#6b5440", accent: "#6b5440"}
      } },
    { id: "birch-branch", name: "Birch Branch", family: "branches",
      material: {id: "birch-bark"},
      room: [0.5, 2.6],
      animation: [{kind: "sway", period: 3.8, amount: 0.1}],
      contour: {
        outer: {base: 0.55, taper: [1, 0.9], rough: {amp: 0.03, scale: 2}},
        inner: {base: 0.55, taper: [1, 0.9], rough: {amp: 0.03, scale: 2.2, seed: 8}},
        ends: {start: {kind: "square"}, end: {kind: "point", size: 1.4}},
        body: {round: true},
        parts: [
            {kind: "branch", at: "between", on: "outer", alternate: true, angle: 52, length: 2.4, width: 0.18, tip: 0.02, bend: 22, wobble: 1.4, fill: "#4a3b2e", stroke: "#00000066", forks: [{at: 0.5, angle: -45, length: 0.9}, {at: 0.75, angle: 35, length: 0.6}]},
            {kind: "branch", at: "end", s: 0.3, angle: -80, length: 2.2, width: 0.2, tip: 0.02, bend: -15, wobble: 1.4, fill: "#4a3b2e", stroke: "#00000066"},
            {kind: "ellipse", at: "start", s: 0.02, w: 0.45, h: 1.1, material: {id: "pale-log-end", fit: true}, stroke: "#3a2a1a", z: "front"},
            {kind: "ellipse", at: "at", where: [0.2, 0.47, 0.78], t: -0.1, w: 0.9, h: 0.22, fill: "#1e1a17", opacity: 0.85, z: "front"}
        ],
        lines: [
            {kind: "windows", offset: 0.3, size: [0.32, 0.045], period: 0.85, lit: 0.55, colour: "#2b2a28", avoid: 0, margin: 0.5},
            {kind: "windows", offset: -0.25, size: [0.26, 0.04], period: 0.7, lit: 0.5, colour: "#2b2a28", avoid: 0, margin: 0.5, seed: 21}
        ],
        colours: {body: "#cfcbb8", edge: "#00000066", light: "#2b2a28", accent: "#2b2a28"}
      } },
    { id: "plane-tree-branch", name: "Plane Tree Branch", family: "branches",
      material: {id: "plane-tree-bark"},
      room: [0.5, 2.6],
      animation: [{kind: "sway", period: 4.4, amount: 0.08}],
      contour: {
        outer: {base: 0.6, taper: [1, 0.9], rough: {amp: 0.04, scale: 1.8}},
        inner: {base: 0.6, taper: [1, 0.9], rough: {amp: 0.04, scale: 2.1, seed: 2}},
        ends: {start: {kind: "broken", size: 0.9}, end: {kind: "point", size: 1.4}},
        body: {round: true},
        parts: [
            {kind: "branch", at: "between", on: "outer", alternate: true, angle: 40, length: 2.2, width: 0.3, tip: 0.04, bend: 14, wobble: 0.8, material: true, stroke: "#00000055", forks: [{at: 0.6, angle: 38, length: 0.9}]},
            {kind: "branch", at: "end", s: 0.4, angle: -85, length: 2, width: 0.3, tip: 0.03, bend: 18, wobble: 1, material: true, stroke: "#00000055"}
        ],
        colours: {body: "#b9b48e", edge: "#00000066", light: "#7a7350", accent: "#7a7350"}
      } },
    { id: "driftwood", name: "Driftwood", family: "branches",
      material: {id: "grey-bark", tint: "#e2ddd3"},
      room: [0.4, 0.8],
      animation: [],
      contour: {
        outer: {base: 0.62, rough: {amp: 0.1, scale: 2.2}},
        inner: {base: 0.62, rough: {amp: 0.1, scale: 2.6, seed: 12}},
        sway: {amp: 0.15, period: 14},
        ends: {start: {kind: "broken", size: 1.2}, end: {kind: "broken", size: 1.6}},
        body: {round: true},
        parts: [{kind: "ellipse", at: "at", where: [0.36, 0.71], t: 0.1, w: 0.7, h: 0.4, fill: "#5e574d", stroke: "#9b9284", strokeWidth: 0.05, z: "front"}],
        lines: [{kind: "cracks", count: 9, length: 2.2, width: 0.035, colour: "#57514a"}],
        colours: {body: "#c8c2b6", edge: "#00000066", light: "#57514a", accent: "#57514a"}
      } },
    { id: "mossy-trunk", name: "Mossy Trunk", family: "trunks",
      material: {id: "mossy-bark"},
      room: [2.4, 0.8],
      animation: [],
      contour: {
        outer: {base: 1.15, rough: {amp: 0.07, scale: 2}, features: [{kind: "wave", anchor: "at", at: [0], length: 5, depth: 0.35}]},
        inner: {base: 1.15, rough: {amp: 0.07, scale: 2.3, seed: 3}, features: [{kind: "wave", anchor: "at", at: [0], length: 5, depth: 0.35}]},
        ends: {kind: "square"},
        body: {round: true},
        layers: [{side: "outer", inset: -0.06, width: 0.32, material: {id: "moss"}, dash: [2.5, 1.2], free: true}],
        parts: [
            {kind: "branch", at: "start", s: 0.9, on: "inner", angle: -35, length: 2.1, width: 0.6, tip: 0.05, bend: -40, wobble: 1, material: true, stroke: "#00000066"},
            {kind: "branch", at: "start", s: 1.7, on: "inner", angle: -8, length: 1.5, width: 0.5, tip: 0.05, bend: -50, wobble: 1, material: true, stroke: "#00000066"},
            {kind: "branch", at: "start", s: 0.5, on: "outer", angle: -55, length: 1.4, width: 0.5, tip: 0.05, bend: -30, wobble: 1, material: true, stroke: "#00000066"},
            {kind: "branch", at: "start", s: 0.2, angle: -90, length: 1.2, width: 0.9, tip: 0.1, bend: 0, material: true, stroke: "#00000066"},
            {kind: "ellipse", at: "end", s: 0.3, w: 0.8, h: 2.35, material: {id: "sawn-log-end", fit: true}, stroke: "#3a2a1a", z: "front"}
        ],
        colours: {body: "#5d6250", edge: "#00000088", light: "#7a8a50", accent: "#7a8a50"}
      } },
    { id: "old-oak-trunk", name: "Old Oak Trunk", family: "trunks",
      material: {id: "oak-bark"},
      room: [2.2, 1.2],
      animation: [],
      contour: {
        outer: {base: 1.2, rough: {amp: 0.09, scale: 1.7}, features: [{kind: "wave", anchor: "at", at: [0], length: 5, depth: 0.4}]},
        inner: {base: 1.2, rough: {amp: 0.09, scale: 2.1, seed: 6}, features: [{kind: "wave", anchor: "at", at: [0], length: 5, depth: 0.4}]},
        ends: {start: {kind: "square"}, end: {kind: "broken", size: 1.6}},
        body: {round: true},
        parts: [
            {kind: "branch", at: "start", s: 1, on: "inner", angle: -30, length: 2, width: 0.65, tip: 0.06, bend: -45, wobble: 1, material: true, stroke: "#00000066"},
            {kind: "branch", at: "start", s: 0.6, on: "outer", angle: -60, length: 1.6, width: 0.55, tip: 0.06, bend: -30, wobble: 1, material: true, stroke: "#00000066"},
            {kind: "ellipse", at: "at", where: [0.46], t: 0.05, w: 1, h: 1.3, fill: "#140e09", stroke: "#4a3a2a", strokeWidth: 0.12, z: "front"},
            {kind: "dome", at: "at", where: [0.27, 0.72], on: "outer", t: -0.12, w: 1.3, h: 0.42, fill: "#c9a46a", stroke: "#6b5030", shade: "dome", z: "front"}
        ],
        colours: {body: "#5a4a3a", edge: "#00000088", light: "#c9a46a", accent: "#c9a46a"}
      } },
    { id: "cedar-log", name: "Cedar Log", family: "trunks",
      material: {id: "cedar-bark"},
      room: [0.6, 0.6],
      animation: [],
      contour: {
        outer: {base: 1.05, rough: {amp: 0.04, scale: 2}},
        inner: {base: 1.05, rough: {amp: 0.04, scale: 2.4, seed: 4}},
        ends: {kind: "square"},
        body: {round: true},
        parts: [
            {kind: "ellipse", at: "both", s: 0, w: 0.75, h: 2.15, material: {id: "sawn-log-end", fit: true}, stroke: "#3a2a1a", z: "front"},
            {kind: "ellipse", at: "at", where: [0.33, 0.58], t: 0.35, w: 0.5, h: 0.32, fill: "#3a2717", stroke: "#7a5a3a", strokeWidth: 0.05, z: "front"}
        ],
        lines: [{kind: "cracks", count: 4, length: 1, width: 0.03, colour: "#2a1a0e"}],
        colours: {body: "#7a5a3a", edge: "#00000088", light: "#7a5a3a", accent: "#7a5a3a"}
      } },
    { id: "cracked-pillar", name: "Cracked Stone Pillar", family: "stone",
      material: {id: "travertine"},
      room: [1.3, 2.4],
      animation: [],
      contour: {
        outer: {base: 0.9, rough: {amp: 0.03, scale: 0.6}},
        inner: {base: 0.9, rough: {amp: 0.03, scale: 0.7, seed: 4}},
        ends: {start: {kind: "square"}, end: {kind: "broken", size: 1.4}},
        body: {round: true},
        layers: [
            {side: "middle", offset: -0.6, width: 0.07, fill: "#00000030", margin: 0.2},
            {side: "middle", offset: -0.3, width: 0.07, fill: "#00000030", margin: 0.2},
            {side: "middle", offset: 0, width: 0.07, fill: "#00000030", margin: 0.2},
            {side: "middle", offset: 0.3, width: 0.07, fill: "#00000030", margin: 0.2},
            {side: "middle", offset: 0.6, width: 0.07, fill: "#00000030", margin: 0.2},
            {side: "middle", offset: -0.54, width: 0.04, fill: "#ffffff30", margin: 0.2},
            {side: "middle", offset: -0.24, width: 0.04, fill: "#ffffff30", margin: 0.2},
            {side: "middle", offset: 0.06, width: 0.04, fill: "#ffffff30", margin: 0.2},
            {side: "middle", offset: 0.36, width: 0.04, fill: "#ffffff30", margin: 0.2},
            {side: "middle", offset: 0.66, width: 0.04, fill: "#ffffff30", margin: 0.2}
        ],
        parts: [
            {kind: "polygon", at: "start", points: [[0, 1.28], [0.15, 1.28], [0.9, 0.95], [0.9, -0.95], [0.15, -1.28], [0, -1.28]], material: true, stroke: "#00000055", shade: "cylinder"},
            {kind: "box", at: "start", s: -0.55, w: 1.1, h: 2.75, r: 0.04, material: true, stroke: "#00000055", shade: "cylinder"},
            {kind: "ellipse", at: "at", where: [0.42, 0.7], on: "outer", t: -0.25, w: 1.6, h: 0.4, material: {id: "moss"}, opacity: 0.9, z: "front"},
            {kind: "polygon", at: "end", s: -1.4, t: -0.5, points: [[-0.8, -0.4], [0.5, -0.5], [0.7, 0.2], [0.1, 0.45], [-0.6, 0.3]], material: true, stroke: "#00000055", shade: "dome"}
        ],
        lines: [{kind: "cracks", count: 7, length: 1.5, width: 0.05, colour: "#2b2620"}],
        colours: {body: "#d8ccb4", edge: "#00000066", light: "#2b2620", accent: "#2b2620"}
      } },
    { id: "marble-column", name: "Marble Column", family: "stone",
      material: {id: "polished-white-marble", relief: 0.05},
      room: [1.4, 1.4],
      animation: [],
      contour: {
        outer: {base: 0.9},
        inner: {base: 0.9},
        ends: {kind: "square"},
        body: {round: true},
        layers: [
            {side: "middle", offset: -0.6, width: 0.07, fill: "#00000022", margin: 0.2},
            {side: "middle", offset: -0.3, width: 0.07, fill: "#00000022", margin: 0.2},
            {side: "middle", offset: 0, width: 0.07, fill: "#00000022", margin: 0.2},
            {side: "middle", offset: 0.3, width: 0.07, fill: "#00000022", margin: 0.2},
            {side: "middle", offset: 0.6, width: 0.07, fill: "#00000022", margin: 0.2}
        ],
        parts: [
            {kind: "box", at: "start", s: -0.5, w: 1, h: 3, r: 0.04, material: true, stroke: "#00000044", shade: "cylinder"},
            {kind: "box", at: "end", s: -0.35, w: 0.7, h: 2.3, r: 0.25, material: true, stroke: "#00000044", shade: "cylinder"},
            {kind: "box", at: "end", s: -0.95, w: 0.5, h: 2.7, r: 0.04, material: true, stroke: "#00000044", shade: "cylinder"}
        ],
        lines: [{kind: "spiral", at: "start", width: 0.07, turns: 1.6, size: 1.2, colour: "#bdb6a8"}],
        colours: {body: "#f2f0ec", edge: "#00000044", light: "#bdb6a8", accent: "#bdb6a8"}
      } },
    { id: "broken-concrete-beam", name: "Broken Concrete Beam", family: "stone",
      material: {id: "concrete", relief: 0.04},
      room: [0.3, 2.4],
      animation: [],
      contour: {
        outer: {base: 0.95, rough: {amp: 0.02, scale: 1}},
        inner: {base: 0.95},
        ends: {start: {kind: "square"}, end: {kind: "broken", size: 1.6}},
        parts: [
            {kind: "strut", at: "end", from: [1.2, 0.5], to: [-1.9, 0.75], width: 0.1, fill: "#7a3f1d", stroke: "#3b1a08"},
            {kind: "strut", at: "end", from: [1.2, 0], to: [-1.4, -0.25], width: 0.1, fill: "#7a3f1d", stroke: "#3b1a08"},
            {kind: "strut", at: "end", from: [1.2, -0.5], to: [-2.1, -0.95], width: 0.1, fill: "#7a3f1d", stroke: "#3b1a08"}
        ],
        lines: [{kind: "cracks", count: 6, length: 1.3, width: 0.05, colour: "#3a3a3a"}],
        colours: {body: "#a8a8a4", edge: "#00000066", light: "#3a3a3a", accent: "#3a3a3a"}
      } },
    { id: "river-pebbles", name: "River Pebbles", family: "stone",
      material: {id: "river-pebbles"},
      animation: [{kind: "pulse", period: 3, amount: 0.15}],
      contour: {
        outer: {features: [{kind: "wave", period: 1.3, length: 1.4, depth: 0.18, vary: 0.8}]},
        inner: {features: [{kind: "wave", period: 1.5, length: 1.5, depth: 0.16, vary: 0.8, phase: 0.4}]},
        ends: {kind: "pill"},
        parts: [
            {kind: "ellipse", at: "between", on: "outer", t: -0.35, w: 0.9, h: 0.55, material: true, stroke: "#00000055", shade: "dome", every: 2},
            {kind: "ellipse", at: "between", on: "inner", t: -0.3, w: 0.8, h: 0.5, material: true, stroke: "#00000055", shade: "dome", every: 2, skip: 1}
        ],
        colours: {body: "#7d7d78", edge: "#00000066", light: "#ffffff", accent: "#ffffff"}
      } },
    { id: "starship-red-core", name: "Starship Red Core", family: "starship-front",
      material: {id: "black-gloss-plastic"},
      room: [1.6, 1.6],
      animation: [{kind: "pulse", period: 2.2, amount: 0.35}, {kind: "blink", period: 0.7}],
      contour: {
        outer: {features: [{kind: "wave", anchor: "between", length: 2.4, depth: 0.42}]},
        inner: {features: [{kind: "wave", anchor: "between", length: 2.4, depth: 0.3}]},
        ends: {kind: "pill"},
        body: {round: "chrome"},
        layers: [
            {side: "inner", inset: -0.2, width: 0.34, fill: "#18181b", free: true},
            {side: "inner", inset: -0.14, width: 0.05, fill: "#ffffff2e", free: true}
        ],
        parts: [
            {align: "centre", z: "front", at: "between", on: "outer", t: -0.3, kind: "ellipse", w: 0.95, h: 0.95, material: {id: "silver"}, stroke: "#00000099", shade: "chrome", bevel: 0.05},
            {align: "centre", z: "front", at: "between", on: "outer", t: -0.3, kind: "lens", w: 0.646, h: 0.646, colour: "#e3170a", dim: 0.3},
            {align: "centre", z: "front", at: "both", s: -0.2, kind: "ellipse", w: 2.3, h: 2.3, material: {id: "silver"}, stroke: "#00000099", shade: "chrome", bevel: 0.05},
            {align: "centre", z: "front", at: "both", s: -0.2, kind: "lens", w: 1.564, h: 1.564, colour: "#e3170a", dim: 0.3},
            {kind: "box", at: "between", on: "outer", t: -0.42, w: 0.16, h: 0.4, r: 0.04, material: {id: "silver"}, shade: "chrome"},
            {kind: "box", at: "between", on: "outer", t: -0.5, w: 0.26, h: 0.12, r: 0.03, material: {id: "black-metal"}}
        ],
        lines: [
            {kind: "chips", offset: -0.42, height: 0.13, lengths: [0.25, 0.9], gap: 0.06, colours: ["#ff2a1f", "#ff5a3c", "#ff2a1f", "#ffb0a0"], under: 1.05, margin: 1.4, square: true},
            {kind: "pipes", count: 2, offset: -0.66, width: 0.05, gap: 0.025, period: 1, material: {id: "black-metal"}, under: 1.05, margin: 1.4}
        ],
        colours: {body: "#0b0b0d", edge: "#000000aa", light: "#ff2a1f", accent: "#ff5a3c"}
      } },
    { id: "starship-biomech", name: "Starship Biomech", family: "starship-front",
      room: [2.2, 2.2],
      animation: [{kind: "pulse", period: 2.2, amount: 0.35}, {kind: "blink", period: 0.7}],
      contour: {
        outer: {base: 0.95},
        inner: {base: 0.95},
        ends: {kind: "pill"},
        body: {fill: "none"},
        parts: [
            {align: "centre", z: "front", at: "between", kind: "ellipse", w: 1, h: 1, material: {id: "silver"}, stroke: "#00000099", shade: "chrome", bevel: 0.05},
            {align: "centre", z: "front", at: "between", kind: "lens", w: 0.68, h: 0.68, colour: "#e3170a"},
            {align: "centre", z: "front", at: "both", s: -0.4, kind: "ellipse", w: 2, h: 2, material: {id: "silver"}, stroke: "#00000099", shade: "chrome", bevel: 0.05},
            {align: "centre", z: "front", at: "both", s: -0.4, kind: "lens", w: 1.36, h: 1.36, colour: "#e3170a"},
            {kind: "box", at: "period", period: 1.3, w: 0.13, h: 1.25, r: 0.05, align: "centre", material: {id: "silver"}, shade: "chrome", z: "front"}
        ],
        lines: [{kind: "plait", strands: 2, amp: 0.5, period: 5.2, width: 0.55, colour: "#1a1a1e", material: {id: "black-gloss-plastic"}, outline: "#000000", outlineWidth: 0.05, margin: 0.1, gather: 2}],
        colours: {body: "#0b0b0d", edge: "#00000000", light: "#ff2a1f", accent: "#ff5a3c"},
        layers: []
      } },
    { id: "starship-command-deck", name: "Starship Command Deck", family: "starship-front",
      material: {id: "black-gloss-plastic"},
      room: [1.4, 1.4],
      animation: [{kind: "pulse", period: 2.2, amount: 0.35}, {kind: "blink", period: 0.7}],
      contour: {
        outer: {base: 1.1, features: [{kind: "wave", anchor: "middle", length: 34, depth: 0.25}]},
        inner: {base: 1.1},
        ends: {kind: "chamfer", size: 1},
        body: {round: "chrome"},
        layers: [{side: "outer", inset: 0.12, width: 0.36, fill: "#2a0604", avoid: 0.95, margin: 1.2}],
        parts: [
            {align: "centre", z: "front", at: "both", s: 0.2, kind: "ellipse", w: 2, h: 2, material: {id: "silver"}, stroke: "#00000099", shade: "chrome", bevel: 0.05},
            {align: "centre", z: "front", at: "both", s: 0.2, kind: "lens", w: 1.36, h: 1.36, colour: "#e3170a"}
        ],
        lines: [
            {kind: "chips", side: "outer", inset: 0.3, height: 0.13, lengths: [0.2, 0.7], gap: 0.05, colours: ["#ff2a1f", "#ff7a5c", "#ff2a1f", "#ffd0c0"], avoid: 0.95, margin: 1.4, square: true},
            {kind: "dots", side: "inner", inset: 0.35, period: 0.45, width: 0.05, colour: "#ff2a1f", avoid: 0.95},
            {kind: "grilles", offset: -0.28, height: 0.3, count: 8, share: 0.55, light: "#ff2a1f", louvre: "#2a2a2e", avoid: 0.95, margin: 1.2},
            {kind: "dots", side: "outer", inset: 0.08, period: 0.3, width: 0.035, colour: "#5a5d62", avoid: 0.92},
            {kind: "dots", side: "inner", inset: 0.08, period: 0.3, width: 0.035, colour: "#5a5d62", avoid: 0.92}
        ],
        colours: {body: "#0b0b0d", edge: "#000000aa", light: "#ff2a1f", accent: "#ff5a3c"}
      } },
    { id: "alien-invader-chrome", name: "Alien Invader Chrome", family: "alien",
      material: {id: "black-gloss-plastic"},
      room: [2, 2],
      animation: [{kind: "pulse", period: 2.6, amount: 0.4}, {kind: "spin", period: 6}],
      contour: {
        outer: {base: 1},
        inner: {base: 1},
        ends: {kind: "pill"},
        body: {round: "chrome"},
        layers: [
            {side: "outer", width: 0.17, material: {id: "silver"}},
            {side: "inner", width: 0.17, material: {id: "silver"}},
            {side: "middle", offset: -0.58, width: 0.07, fill: "#8fe3ff", glow: "#1e8fff", margin: 1}
        ],
        parts: [
            {kind: "branch", at: "both", on: "outer", s: 0.4, angle: 62, length: 3.6, width: 0.14, tip: 0.01, bend: 55, fill: "#9fe8ff", glow: "#1e8fff"},
            {kind: "ellipse", at: "both", s: -0.4, w: 2.4, h: 2.4, align: "centre", material: {id: "silver"}, stroke: "#000000cc", shade: "chrome", z: "front"},
            {kind: "ellipse", at: "both", s: -0.4, w: 1.9, h: 1.9, align: "centre", fill: "#05070a", z: "front"},
            {kind: "arc", at: "both", s: -0.4, w: 1.65, h: 1.65, hole: 0.8, from: 6, to: 48, align: "centre", fill: "#7fdcff", glow: "#1e8fff", z: "front"},
            {kind: "arc", at: "both", s: -0.4, w: 1.65, h: 1.65, hole: 0.8, from: 66, to: 108, align: "centre", fill: "#7fdcff", glow: "#1e8fff", z: "front"},
            {kind: "arc", at: "both", s: -0.4, w: 1.65, h: 1.65, hole: 0.8, from: 126, to: 168, align: "centre", fill: "#7fdcff", glow: "#1e8fff", z: "front"},
            {kind: "arc", at: "both", s: -0.4, w: 1.65, h: 1.65, hole: 0.8, from: 186, to: 228, align: "centre", fill: "#7fdcff", glow: "#1e8fff", z: "front"},
            {kind: "arc", at: "both", s: -0.4, w: 1.65, h: 1.65, hole: 0.8, from: 246, to: 288, align: "centre", fill: "#7fdcff", glow: "#1e8fff", z: "front"},
            {kind: "arc", at: "both", s: -0.4, w: 1.65, h: 1.65, hole: 0.8, from: 306, to: 348, align: "centre", fill: "#7fdcff", glow: "#1e8fff", z: "front"},
            {kind: "ellipse", at: "both", s: -0.4, w: 0.8, h: 0.8, align: "centre", gradient: ["#e6f8ff", "#1e8fff"], glow: "#1e8fff", z: "front"}
        ],
        colours: {body: "#07090c", edge: "#000000aa", light: "#7fdcff", accent: "#1e8fff"}
      } },
    { id: "alien-darkstar", name: "Alien Darkstar", family: "alien",
      material: {id: "black-metal"},
      animation: [{kind: "pulse", period: 3, amount: 0.35}],
      contour: {
        outer: {base: 1.05},
        inner: {base: 1.05},
        ends: {kind: "chamfer", size: 0.9},
        body: {round: "chrome"},
        layers: [
            {side: "middle", width: 1.5, material: {id: "black-gloss-plastic"}, margin: 0.6},
            {side: "outer", inset: 0.1, width: 0.05, fill: "#ff3b2f", glow: "#e3170a", margin: 0.9},
            {side: "inner", inset: 0.1, width: 0.03, fill: "#ff3b2f", glow: "#e3170a", margin: 0.9, opacity: 0.7}
        ],
        parts: [{kind: "box", at: "both", s: 0.95, w: 0.9, h: 1.3, r: 0.08, align: "centre", material: {id: "dark-honeycomb"}, stroke: "#000000cc", z: "front"}],
        lines: [{kind: "ends", inset: 0.35, width: 0.12, colour: "#ff3b2f"}],
        colours: {body: "#1a1b1e", edge: "#000000cc", light: "#ff3b2f", accent: "#e3170a"}
      } },
    { id: "alien-hud", name: "Alien HUD", family: "alien",
      animation: [{kind: "spin", period: 5}, {kind: "pulse", period: 2.4, amount: 0.3}],
      contour: {
        outer: {base: 1.05},
        inner: {base: 1.05},
        ends: {kind: "chamfer", size: 0.9},
        body: {opacity: 0.82},
        parts: [
            {kind: "ring", at: "between", w: 1.15, h: 1.15, hole: 0.9, align: "centre", fill: "#3fe0ff", glow: "#00b4ff", z: "front"},
            {kind: "arc", at: "between", w: 0.9, h: 0.9, hole: 0.72, from: 30, to: 250, align: "centre", fill: "#9ff0ff", glow: "#00b4ff", z: "front"}
        ],
        lines: [
            {kind: "loop", inset: 0.15, width: 0.04, colour: "#3fe0ff"},
            {kind: "brackets", inset: 0.32, width: 0.06, long: 2.2, short: 0.7, colour: "#9ff0ff"},
            {kind: "slats", period: 0.28, amp: 0, size: 0.1, offset: 0.62, width: 0.035, colour: "#3fe0ff", avoid: 1, margin: 1}
        ],
        colours: {body: "#020c14", edge: "#3fe0ff66", light: "#3fe0ff", accent: "#00b4ff"}
      } },
    { id: "alien-vortex", name: "Alien Vortex", family: "alien",
      material: {id: "black-gloss-plastic"},
      room: [2.4, 2.4],
      animation: [{kind: "spin", period: 2.4}, {kind: "flow", speed: 1.5}],
      contour: {
        outer: {base: 0.85},
        inner: {base: 0.85},
        ends: {kind: "pill"},
        body: {round: "chrome"},
        parts: [
            {kind: "arc", at: "both", s: -0.9, w: 2.6, h: 2.6, hole: 0.82, from: 0, to: 200, align: "centre", fill: "#8fe0ff", glow: "#1e8fff", z: "front", opacity: 1},
            {kind: "arc", at: "both", s: -0.9, w: 2.18, h: 2.18, hole: 0.82, from: 70, to: 270, align: "centre", fill: "#2f9bff", glow: "#1e8fff", z: "front", opacity: 0.88},
            {kind: "arc", at: "both", s: -0.9, w: 1.76, h: 1.76, hole: 0.82, from: 140, to: 340, align: "centre", fill: "#8fe0ff", glow: "#1e8fff", z: "front", opacity: 0.76},
            {kind: "arc", at: "both", s: -0.9, w: 1.34, h: 1.34, hole: 0.82, from: 210, to: 410, align: "centre", fill: "#2f9bff", glow: "#1e8fff", z: "front", opacity: 0.64},
            {kind: "arc", at: "both", s: -0.9, w: 0.92, h: 0.92, hole: 0.82, from: 280, to: 480, align: "centre", fill: "#8fe0ff", glow: "#1e8fff", z: "front", opacity: 0.52}
        ],
        lines: [{kind: "wave", amp: 0.25, period: 4, width: 0.06, colour: "#8fe0ff"}],
        colours: {body: "#05070b", edge: "#000000aa", light: "#8fe0ff", accent: "#1e8fff"}
      } },
    { id: "alien-green-gauges", name: "Alien Green Gauges", family: "alien",
      material: {id: "black-gloss-plastic"},
      room: [2.2, 2.2],
      animation: [{kind: "spin", period: 4}, {kind: "blink", period: 0.8}],
      contour: {
        outer: {base: 1},
        inner: {base: 1},
        ends: {kind: "pill"},
        body: {round: "chrome"},
        parts: [
            {kind: "ellipse", at: "both", s: -0.5, w: 2.3, h: 2.3, align: "centre", material: {id: "black-metal"}, stroke: "#3dff6e66", shade: "chrome", z: "front"},
            {kind: "ring", at: "both", s: -0.5, w: 1.95, h: 1.95, hole: 0.93, align: "centre", fill: "#3dff6e", glow: "#19c94a", z: "front"},
            {kind: "arc", at: "both", s: -0.5, w: 1.55, h: 1.55, hole: 0.78, from: 120, to: 400, align: "centre", fill: "#8dffaa", glow: "#19c94a", z: "front"}
        ],
        lines: [{kind: "chips", offset: -0.5, height: 0.12, lengths: [0.15, 0.6], gap: 0.05, colours: ["#3dff6e", "#19c94a", "#8dffaa"], avoid: 0.95, margin: 1, square: true}],
        colours: {body: "#050806", edge: "#000000aa", light: "#3dff6e", accent: "#19c94a"}
      } },
    { id: "starship-light-table", name: "Starship Light Table", family: "starship-console",
      material: {id: "black-gloss-plastic", tint: "#1a2c44"},
      animation: [{kind: "blink", period: 0.6}, {kind: "pulse", period: 2.4, amount: 0.25}],
      contour: {
        outer: {base: 1.15},
        inner: {base: 1.15},
        ends: {kind: "chamfer", size: 0.5},
        body: {round: "chrome", bevel: 0.04},
        layers: [
            {side: "outer", width: 0.05, fill: "#d8f6ff", glow: "#7fdcff", opacity: 0.9},
            {side: "inner", width: 0.03, fill: "#d8f6ff", opacity: 0.5}
        ],
        lines: [
            {kind: "dotgrid", period: 0.22, width: 0.022, colour: "#7fdcff44"},
            {kind: "widgets", colours: ["#7fdcff", "#d8f6ff", "#3fa8ff"], accent: "#ffffff", width: 0.03, height: 0.78, avoid: 0.92, margin: 0.4},
            {kind: "ticks", side: "outer", inset: 0.1, period: 0.14, major: 6, length: 0.08, long: 0.16, width: 0.025, colour: "#d8f6ff", avoid: 0.92}
        ],
        colours: {body: "#0b1624", edge: "#7fdcff55", light: "#d8f6ff", accent: "#ffffff"}
      } },
    { id: "starship-console-amber", name: "Starship Console Amber", family: "starship-console",
      material: {id: "black-gloss-plastic", tint: "#3a2410"},
      animation: [{kind: "blink", period: 0.6}, {kind: "pulse", period: 2.4, amount: 0.25}],
      contour: {
        outer: {base: 1.15},
        inner: {base: 1.15},
        ends: {kind: "chamfer", size: 0.5},
        body: {round: "chrome", bevel: 0.04},
        layers: [
            {side: "outer", width: 0.05, fill: "#ffe0b0", glow: "#ffb347", opacity: 0.9},
            {side: "inner", width: 0.03, fill: "#ffe0b0", opacity: 0.5}
        ],
        lines: [
            {kind: "dotgrid", period: 0.22, width: 0.022, colour: "#ffb34744"},
            {kind: "widgets", colours: ["#ffb347", "#ffe0b0", "#ff7a1a"], accent: "#ffffff", width: 0.03, height: 0.78, avoid: 0.92, margin: 0.4},
            {kind: "ticks", side: "outer", inset: 0.1, period: 0.14, major: 6, length: 0.08, long: 0.16, width: 0.025, colour: "#ffe0b0", avoid: 0.92}
        ],
        colours: {body: "#1a1208", edge: "#ffb34755", light: "#ffe0b0", accent: "#ffffff"}
      } },
    { id: "starship-tactical", name: "Starship Tactical Display", family: "starship-console",
      animation: [{kind: "spin", period: 8}, {kind: "blink", period: 0.5}],
      contour: {
        outer: {base: 1.15},
        inner: {base: 1.15},
        ends: {kind: "chamfer", size: 0.7},
        body: {opacity: 0.9, bevel: 0.03},
        parts: [
            {kind: "arc", at: "icons", w: 2, h: 2, hole: 0.965, from: 200, to: 340, align: "centre", fill: "#cfe9f5", opacity: 0.85, z: "front"},
            {kind: "arc", at: "icons", w: 2, h: 2, hole: 0.965, from: 20, to: 160, align: "centre", fill: "#cfe9f5", opacity: 0.85, z: "front"},
            {kind: "arc", at: "icons", w: 1.75, h: 1.75, hole: 0.93, from: 250, to: 290, align: "centre", fill: "#ff4a3a", glow: "#ff2a1f", z: "front"}
        ],
        lines: [
            {kind: "dotgrid", period: 0.2, width: 0.02, colour: "#a8d8ee33"},
            {kind: "ticks", side: "outer", inset: 0.08, period: 0.12, major: 5, length: 0.07, long: 0.15, width: 0.022, colour: "#cfe9f5", avoid: 1.05},
            {kind: "ticks", side: "inner", inset: 0.08, period: 0.12, major: 5, length: 0.07, long: 0.15, width: 0.022, colour: "#cfe9f5", avoid: 1.05},
            {kind: "chips", offset: 0, height: 0.07, lengths: [0.1, 0.45], gap: 0.06, colours: ["#cfe9f5", "#cfe9f5", "#ff4a3a", "#cfe9f5"], avoid: 1.15, margin: 0.6, square: true}
        ],
        colours: {body: "#05080b", edge: "#cfe9f566", light: "#cfe9f5", accent: "#ff4a3a"}
      } },
    { id: "collective-hull", name: "Collective Hull", family: "starship-hull",
      material: {id: "black-metal", tint: "#7d8a80"},
      animation: [{kind: "pulse", period: 2.8, amount: 0.45}],
      contour: {
        outer: {base: 1.15},
        inner: {base: 1.15},
        ends: {kind: "square"},
        body: {bevel: 0.05},
        lines: [
            {kind: "panels", rows: 6, lengths: [0.25, 1.1], shades: ["#00000000"], seam: "#000000aa", width: 0.016},
            {kind: "greebles", rows: 4, sizes: [0.15, 0.9], shades: ["#ffffff10", "#00000030", "#00000012", "#ffffff06"], gap: 0.04},
            {kind: "greebles", rows: 2, sizes: [0.5, 1.7], shades: ["#00000025", "#ffffff08", "#00000040"], gap: 0.14, seed: 89},
            {kind: "middle", offset: 0.55, width: 0.035, colour: "#45ff6a", dash: [0.7, 1.9], margin: 0.2},
            {kind: "middle", offset: -0.48, width: 0.035, colour: "#45ff6a", dash: [0.4, 2.6], margin: 0.2},
            {kind: "stars", period: 0.7, width: 0.035, colour: "#6dff86", seed: 5},
            {kind: "pipes", count: 3, offset: 0.62, width: 0.05, gap: 0.02, period: 0.9, material: {id: "black-metal"}},
            {kind: "pipes", count: 2, offset: -0.6, width: 0.07, gap: 0.03, period: 1.3, material: {id: "black-metal"}},
            {kind: "grilles", offset: 0, height: 0.36, count: 9, share: 0.45, light: "#38ff5a", louvre: "#2a302b", avoid: 0.95}
        ],
        colours: {body: "#2c332e", edge: "#000000aa", light: "#45ff6a", accent: "#45ff6a"},
        parts: [],
        layers: []
      } },
    { id: "raptor-hull", name: "Raptor Hull", family: "starship-hull",
      material: {id: "battle-plates", tint: "#93a07a", relief: 0.55},
      room: [2.4, 2.4],
      animation: [{kind: "pulse", period: 2.2, amount: 0.4}],
      contour: {
        outer: {base: 0.6, features: [{kind: "wave", anchor: "middle", length: 40, depth: 0.55}]},
        inner: {base: 0.6, features: [{kind: "wave", anchor: "middle", length: 40, depth: 0.32}, {kind: "tooth", period: 0.9, length: 0.9, depth: 0.2, curve: 1.5}]},
        ends: {kind: "point", size: 1.2},
        body: {bevel: 0.09},
        layers: [{side: "outer", inset: 0.14, width: 0.05, material: true, margin: 1}],
        launcher: {at: "middle", on: "outer", t: 1.15, size: 1.32},
        parts: [
            {kind: "trapezoid", at: "middle", on: "outer", t: -0.14, w: 2.2, top: 1.2, h: 0.75, material: true},
            {kind: "ellipse", at: "middle", on: "outer", t: 1.12, w: 2.9, h: 2.05, align: "centre", material: true, shade: "dome"},
            {kind: "ring", at: "middle", on: "outer", t: 1.15, w: 1.82, h: 1.82, hole: 0.84, align: "centre", material: {id: "black-metal"}, shade: "chrome"},
            {kind: "ring", at: "middle", on: "outer", t: 1.15, w: 1.58, h: 1.58, hole: 0.93, align: "centre", fill: "#ff5a2a", glow: "#ff3b1f"},
            {kind: "ellipse", at: "middle", on: "outer", t: 1.15, w: 1.47, h: 1.47, align: "centre", fill: "#160807", shade: "chrome"},
            {kind: "lens", at: "middle", on: "outer", s: -1.2, t: 0.62, w: 0.2, h: 0.2, align: "centre", colour: "#ff3b1f"},
            {kind: "lens", at: "middle", on: "outer", s: 1.2, t: 0.62, w: 0.2, h: 0.2, align: "centre", colour: "#ff3b1f"},
            {kind: "box", at: "at", where: [0.36], t: -0.22, w: 1.4, h: 0.07, align: "centre", fill: "#ff8a3a", glow: "#ff5a1f", z: "front"},
            {kind: "box", at: "at", where: [0.64], t: -0.22, w: 1.4, h: 0.07, align: "centre", fill: "#ff8a3a", glow: "#ff5a1f", z: "front"},
            {kind: "strut", at: "both", from: [0.9, 0], to: [-1.9, 0], width: 0.16, material: {id: "black-metal"}, shade: "cylinder", z: "front"},
            {kind: "box", at: "both", s: 0.6, w: 0.5, h: 0.34, r: 0.06, align: "centre", material: {id: "black-metal"}, shade: "cylinder", z: "front"},
            {kind: "lens", at: "both", s: -1.95, w: 0.28, h: 0.28, align: "centre", colour: "#ff3b1f", z: "front"},
            {kind: "ring", at: "both", s: -1, w: 0.3, h: 0.3, hole: 0.55, align: "centre", material: {id: "black-metal"}, shade: "chrome", z: "front"},
            {kind: "ring", at: "both", s: -1.5, w: 0.3, h: 0.3, hole: 0.55, align: "centre", material: {id: "black-metal"}, shade: "chrome", z: "front"}
        ],
        lines: [
            {kind: "plates", rows: 3, period: 0.95, shades: ["#ffffff14", "#0000002a", "#0000000c", "#ffffff06", "#3a2a0012"]},
            {kind: "windows", offset: 0.42, size: [0.16, 0.06], period: 1.1, lit: 0.5, colour: "#ff8a3a", avoid: 1, glow: true, margin: 1.4},
            {kind: "grilles", offset: -0.12, height: 0.42, count: 6, share: 0.5, light: "#ff4a1a", louvre: "#3e4436", avoid: 0.95, margin: 1.4},
            {kind: "pipes", count: 2, offset: 0.36, width: 0.05, gap: 0.025, period: 1.1, material: {id: "battle-plates", tint: "#6f785a"}, avoid: 0.95, margin: 1.6},
            {kind: "dots", side: "outer", inset: 0.09, period: 0.3, width: 0.035, colour: "#7d8466", avoid: 0.92},
            {kind: "dots", side: "inner", inset: 0.09, period: 0.3, width: 0.035, colour: "#7d8466", avoid: 0.92}
        ],
        colours: {body: "#4a5244", edge: "#000000aa", light: "#ff8a3a", accent: "#ff3b1f"}
      } },
    { id: "star-cruiser-hull", name: "Star Cruiser Hull", family: "starship-hull",
      material: {id: "hull-metal", tint: "#d9dde2", relief: 0.22},
      room: [0.4, 1],
      animation: [{kind: "pulse", period: 1.8, amount: 0.25}],
      contour: {
        outer: {base: 0.95, taper: [0.66, 1.1]},
        inner: {base: 0.95, taper: [0.66, 1.1]},
        ends: {start: {kind: "point", size: 3.4}, end: {kind: "square"}},
        body: {bevel: 0.1},
        layers: [{side: "middle", width: 0.3, fill: "#2a2e34", recess: true, margin: 1.4}],
        parts: [
            {kind: "trapezoid", at: "end", on: "outer", s: 2.4, t: -0.02, w: 3.4, top: 2.6, h: 0.42, material: true, z: "back"},
            {kind: "box", at: "end", on: "outer", s: 2.3, t: 0.38, w: 1.5, h: 0.36, chamfer: 0.1, material: true, z: "back"},
            {kind: "box", at: "end", on: "outer", s: 2.3, t: 0.52, w: 1.1, h: 0.05, align: "centre", fill: "#eaf6ff", glow: "#9fd8ff", z: "back"},
            {kind: "dome", at: "end", on: "outer", s: 1.75, t: 0.72, w: 0.44, h: 0.22, material: true, z: "back"},
            {kind: "dome", at: "end", on: "outer", s: 2.85, t: 0.72, w: 0.44, h: 0.22, material: true, z: "back"},
            {kind: "ellipse", at: "end", s: 0.02, t: 0.62, w: 0.78, h: 0.62, align: "centre", material: {id: "black-metal"}, shade: "dome", z: "front"},
            {kind: "ellipse", at: "end", s: 0.02, t: 0, w: 0.78, h: 0.62, align: "centre", material: {id: "black-metal"}, shade: "dome", z: "front"},
            {kind: "ellipse", at: "end", s: 0.02, t: -0.62, w: 0.78, h: 0.62, align: "centre", material: {id: "black-metal"}, shade: "dome", z: "front"},
            {kind: "lens", at: "end", s: 0.02, t: 0.62, w: 0.52, h: 0.46, align: "centre", colour: "#4fb4ff", z: "front"},
            {kind: "lens", at: "end", s: 0.02, t: 0, w: 0.52, h: 0.46, align: "centre", colour: "#4fb4ff", z: "front"},
            {kind: "lens", at: "end", s: 0.02, t: -0.62, w: 0.52, h: 0.46, align: "centre", colour: "#4fb4ff", z: "front"},
            {kind: "box", at: "between", on: "outer", t: -0.06, w: 0.5, h: 0.12, r: 0.03, material: {id: "black-metal"}},
            {kind: "dome", at: "between", on: "outer", t: 0.04, w: 0.36, h: 0.2, material: {id: "hull-metal", tint: "#b8bec6"}, shade: "dome"},
            {kind: "strut", at: "between", on: "outer", from: [0.05, 0.14], to: [0.42, 0.24], width: 0.045, material: {id: "black-metal"}},
            {kind: "strut", at: "between", on: "outer", from: [-0.05, 0.12], to: [0.32, 0.22], width: 0.045, material: {id: "black-metal"}},
            {kind: "ring", at: "end", s: 0.02, t: 0.62, w: 0.86, h: 0.7, hole: 0.78, align: "centre", material: {id: "hull-metal", tint: "#b8bec6"}, shade: "chrome", z: "front"},
            {kind: "ring", at: "end", s: 0.02, t: 0, w: 0.86, h: 0.7, hole: 0.78, align: "centre", material: {id: "hull-metal", tint: "#b8bec6"}, shade: "chrome", z: "front"},
            {kind: "ring", at: "end", s: 0.02, t: -0.62, w: 0.86, h: 0.7, hole: 0.78, align: "centre", material: {id: "hull-metal", tint: "#b8bec6"}, shade: "chrome", z: "front"},
            {kind: "strut", at: "end", on: "outer", s: 2.3, from: [0, 0.7], to: [0, 1.25], width: 0.035, material: {id: "black-metal"}},
            {kind: "strut", at: "end", on: "outer", s: 2.3, from: [-0.18, 1], to: [0.18, 1], width: 0.03, material: {id: "black-metal"}}
        ],
        lines: [
            {kind: "panels", rows: 6, lengths: [0.5, 2], shades: ["#ffffff14", "#00000016", "#ffffff08", "#0000000a", "#00000000"], seam: "#00000070", width: 0.018},
            {kind: "greebles", rows: 1, span: [0.43, 0.57], sizes: [0.1, 0.5], gap: 0.03, avoid: 0.92, margin: 1.4},
            {kind: "greebles", rows: 1, span: [0.06, 0.24], sizes: [0.4, 1.6], gap: 0.12, avoid: 0.95, seed: 91, shades: ["#ffffff0c", "#00000018", "#00000008"]},
            {kind: "greebles", rows: 1, span: [0.76, 0.94], sizes: [0.4, 1.6], gap: 0.12, avoid: 0.95, seed: 97, shades: ["#ffffff0c", "#00000018", "#00000008"]},
            {kind: "windows", offset: 0.68, size: [0.07, 0.04], period: 0.2, lit: 0.55, colour: "#eaf6ff", avoid: 0.95, margin: 3, glow: true},
            {kind: "windows", offset: -0.68, size: [0.07, 0.04], period: 0.2, lit: 0.5, colour: "#eaf6ff", avoid: 0.95, margin: 3, seed: 7, glow: true},
            {kind: "pipes", count: 2, offset: 0, width: 0.065, gap: 0.03, period: 1.3, material: {id: "hull-metal", tint: "#b8bec6"}, margin: 1.6},
            {kind: "dots", side: "outer", inset: 0.1, period: 0.32, width: 0.035, colour: "#9aa0a8", avoid: 0.92},
            {kind: "dots", side: "inner", inset: 0.1, period: 0.32, width: 0.035, colour: "#9aa0a8", avoid: 0.92}
        ],
        colours: {body: "#9aa1a9", edge: "#000000aa", light: "#eaf6ff", accent: "#8fd8ff"}
      } },
    { id: "racing-broom", name: "Racing Broom", family: "wizard-tools",
      material: {id: "dark-walnut", relief: 0.18},
      room: [0.9, 7.4],
      animation: [{kind: "sway", period: 3.5, amount: 0.08}],
      contour: {
        outer: {base: 0.42, taper: [0.72, 1]},
        inner: {base: 0.42, taper: [0.72, 1]},
        ends: {start: {kind: "pill"}, end: {kind: "square"}},
        body: {round: "chrome", bevel: 0.03},
        parts: [
            {kind: "polygon", at: "end", h: 2.1, points: [[0.2, -0.6], [0.2, 0.6], [-1.6, 1], [-3.6, 0.9], [-5.7, 0.5], [-7.1, 0.1], [-7.2, -0.08], [-5.7, -0.46], [-3.6, -0.88], [-1.6, -0.98]], material: {id: "brown-bark", tint: "#6a5440"}, shade: "cylinder"},
            {kind: "box", at: "end", s: -0.1, w: 0.12, h: 1.28, r: 0.05, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"},
            {kind: "box", at: "end", s: -0.3, w: 0.12, h: 1.28, r: 0.05, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"},
            {kind: "box", at: "end", s: -0.5, w: 0.12, h: 1.28, r: 0.05, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"},
            {kind: "capsule", at: "start", s: 0.12, w: 0.85, h: 0.62, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"},
            {kind: "box", at: "start", s: 0.68, w: 0.07, h: 0.66, r: 0.03, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"}
        ],
        lines: [{kind: "twigs", at: "end", count: 130, length: 7, root: 0.6, bulge: 0.98, tip: 0.08, width: 0.045, colours: ["#3a2a1c", "#5a4430", "#6e5538", "#2a1e14", "#7a6244", "#4a3826", "#8a7050"], inset: 0.55}],
        colours: {body: "#3a2014", edge: "#000000aa", light: "#c9a45a", accent: "#c9a45a"}
      } },
    { id: "knotted-wand", name: "Knotted Wand", family: "wizard-tools",
      material: {id: "brown-bark", tint: "#b5a490", relief: 0.35},
      room: [0.3, 1.4],
      animation: [{kind: "pulse", period: 2.2, amount: 0.5}],
      contour: {
        outer: {base: 0.42, taper: [1, 0.38], features: [{kind: "wave", anchor: "between", length: 0.85, depth: 0.2}, {kind: "wave", anchor: "icons", length: 0.5, depth: 0.06}]},
        inner: {base: 0.42, taper: [1, 0.38], features: [{kind: "wave", anchor: "between", length: 0.85, depth: 0.2}, {kind: "wave", anchor: "icons", length: 0.5, depth: 0.06}]},
        ends: {start: {kind: "pill"}, end: {kind: "point", size: 1.4}},
        body: {round: true, bevel: 0.04},
        parts: [{kind: "star", at: "end", s: -0.25, w: 1, h: 1, points: 4, hole: 0.18, fill: "#fff6e0", glow: "#ffe6a8", z: "front"}],
        colours: {body: "#8a7a68", edge: "#000000aa", light: "#ffe6a8", accent: "#ffe6a8"}
      } },
    { id: "floating-candles", name: "Floating Candles", family: "wizard-hall",
      animation: [{kind: "flicker", period: 0.12, amount: 0.25}, {kind: "sway", period: 4, amount: 0.06, mode: "float"}, {kind: "twinkle", period: 2.2, amount: 0.7}],
      contour: {
        outer: {base: 0.95, rough: {amp: 0.12, scale: 1.3}, features: [{kind: "wave", period: 5, length: 4.6, depth: 0.3, vary: 0.7}, {kind: "wave", period: 2.6, length: 2.4, depth: 0.15, phase: 0.3, vary: 0.8}]},
        inner: {base: 0.9, rough: {amp: 0.1, scale: 1.6, seed: 4}, features: [{kind: "wave", period: 4.4, length: 4, depth: 0.25, phase: 0.2, vary: 0.7}]},
        ends: {kind: "pill"},
        body: {feather: 0.6},
        parts: [
            {kind: "box", at: "between", t: -0.25, w: 0.3, h: 1.25, r: 0.04, align: "centre", fill: "#f2e9d2", shade: "cylinder", z: "front"},
            {kind: "ellipse", at: "between", t: 0.355, w: 0.3, h: 0.08, align: "centre", fill: "#fff6e0", z: "front"},
            {kind: "flame", at: "between", t: 0.395, w: 0.24, h: 0.62, align: "base", fill: "#ffd36b", glow: "#ffb347", z: "front"}
        ],
        lines: [{kind: "stars", period: 0.9, width: 0.035, colour: "#e8eeff"}],
        colours: {body: "#101a3a", edge: "#00000000", light: "#ffd36b", accent: "#ffb347"}
      } },
    { id: "wizards-map", name: "Wizard's Map", family: "wizard-hall",
      material: {id: "aged-paper", relief: 0.14},
      folds: {period: 2.6, height: 0.16},
      animation: [{kind: "walk", period: 0.45}],
      contour: {
        outer: {base: 1.12, rough: {amp: 0.03, scale: 0.5}},
        inner: {base: 1.12, rough: {amp: 0.03, scale: 0.6, seed: 3}},
        ends: {kind: "square"},
        body: {bevel: 0.02},
        layers: [
            {side: "outer", width: 0.34, fill: "#6b4520", opacity: 0.22, ink: true},
            {side: "inner", width: 0.34, fill: "#6b4520", opacity: 0.22, ink: true},
            {side: "outer", width: 0.1, fill: "#4a2c12", opacity: 0.3, ink: true},
            {side: "inner", width: 0.1, fill: "#4a2c12", opacity: 0.3, ink: true}
        ],
        parts: [
            {kind: "star", at: "start", s: 0.95, w: 1.5, h: 1.5, points: 4, hole: 0.3, align: "centre", ink: true, stroke: "#2b1a0ee0", strokeWidth: 0.03, z: "front"},
            {kind: "star", at: "start", s: 0.95, w: 1, h: 1, points: 8, hole: 0.42, angle: 22.5, align: "centre", ink: true, stroke: "#2b1a0ec0", strokeWidth: 0.025, z: "front"},
            {kind: "ring", at: "start", s: 0.95, w: 1.15, h: 1.15, hole: 0.96, align: "centre", ink: true, stroke: "#2b1a0eb0", strokeWidth: 0.02, z: "front"}
        ],
        lines: [
            {kind: "plan", width: 0.03, colour: "#2b1a0ed0", avoid: 0.95, dash: [0.35, 0.12], margin: 2},
            {kind: "footprints", period: 0.5, size: 0.3, offset: -0.12, colour: "#2b1a0ee0", avoid: 0.95, labels: 5, width: 0.022, margin: 2}
        ],
        colours: {body: "#e6cf9f", edge: "#00000055", light: "#2b1a0e", accent: "#2b1a0e"}
      } },
    { id: "crimson-gold-banner", name: "Crimson and Gold Banner", family: "wizard-houses",
      material: {id: "velvet", tint: "#8a1c1c"},
      room: [0.8, 0.8],
      animation: [{kind: "sway", period: 3, amount: 0.12}],
      contour: {
        outer: {base: 1},
        inner: {base: 1},
        ends: {kind: "square"},
        body: {round: true, bevel: 0.04},
        layers: [
            {side: "outer", inset: 0.14, width: 0.05, material: {id: "polished-gold"}},
            {side: "inner", inset: 0.2, width: 0.05, material: {id: "polished-gold"}},
            {side: "outer", inset: -0.14, width: 0.2, free: true, material: {id: "polished-gold"}}
        ],
        parts: [
            {kind: "ellipse", at: "both", on: "outer", s: -0.35, t: -0.14, w: 0.5, h: 0.5, align: "centre", material: {id: "polished-gold"}, shade: "chrome", bevel: 0.04, z: "front"},
            {kind: "crystal", at: "both", on: "outer", s: -0.85, t: -0.04, angle: 90, w: 0.32, h: 0.8, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"}
        ],
        lines: [
            {kind: "fringe", side: "inner", length: 0.42, period: 0.075, width: 0.024, colour: "#e6be5a", cord: "#e6be5a"},
            {kind: "dots", side: "outer", inset: 0.42, period: 1.6, width: 0.05, colour: "#e6be5a", avoid: 0.95}
        ],
        colours: {body: "#8a1c1c", edge: "#00000088", light: "#e6be5a", accent: "#e6be5a"}
      } },
    { id: "emerald-silver-banner", name: "Emerald and Silver Banner", family: "wizard-houses",
      material: {id: "velvet", tint: "#1d5a34"},
      room: [0.8, 0.8],
      animation: [{kind: "sway", period: 3, amount: 0.12}],
      contour: {
        outer: {base: 1},
        inner: {base: 1},
        ends: {kind: "square"},
        body: {round: true, bevel: 0.04},
        layers: [
            {side: "outer", inset: 0.14, width: 0.05, material: {id: "silver"}},
            {side: "inner", inset: 0.2, width: 0.05, material: {id: "silver"}},
            {side: "outer", inset: -0.14, width: 0.2, free: true, material: {id: "silver"}}
        ],
        parts: [
            {kind: "ellipse", at: "both", on: "outer", s: -0.35, t: -0.14, w: 0.5, h: 0.5, align: "centre", material: {id: "silver"}, shade: "chrome", bevel: 0.04, z: "front"},
            {kind: "crystal", at: "both", on: "outer", s: -0.85, t: -0.04, angle: 90, w: 0.32, h: 0.8, align: "centre", material: {id: "silver"}, shade: "chrome", z: "front"}
        ],
        lines: [
            {kind: "fringe", side: "inner", length: 0.42, period: 0.075, width: 0.024, colour: "#d8dde3", cord: "#d8dde3"},
            {kind: "dots", side: "outer", inset: 0.42, period: 1.6, width: 0.05, colour: "#d8dde3", avoid: 0.95}
        ],
        colours: {body: "#1d5a34", edge: "#00000088", light: "#d8dde3", accent: "#d8dde3"}
      } },
    { id: "sapphire-bronze-banner", name: "Sapphire and Bronze Banner", family: "wizard-houses",
      material: {id: "velvet", tint: "#1c3170"},
      room: [0.8, 0.8],
      animation: [{kind: "sway", period: 3, amount: 0.12}],
      contour: {
        outer: {base: 1},
        inner: {base: 1},
        ends: {kind: "square"},
        body: {round: true, bevel: 0.04},
        layers: [
            {side: "outer", inset: 0.14, width: 0.05, material: {id: "polished-copper"}},
            {side: "inner", inset: 0.2, width: 0.05, material: {id: "polished-copper"}},
            {side: "outer", inset: -0.14, width: 0.2, free: true, material: {id: "polished-copper"}}
        ],
        parts: [
            {kind: "ellipse", at: "both", on: "outer", s: -0.35, t: -0.14, w: 0.5, h: 0.5, align: "centre", material: {id: "polished-copper"}, shade: "chrome", bevel: 0.04, z: "front"},
            {kind: "crystal", at: "both", on: "outer", s: -0.85, t: -0.04, angle: 90, w: 0.32, h: 0.8, align: "centre", material: {id: "polished-copper"}, shade: "chrome", z: "front"}
        ],
        lines: [
            {kind: "fringe", side: "inner", length: 0.42, period: 0.075, width: 0.024, colour: "#c8925a", cord: "#c8925a"},
            {kind: "dots", side: "outer", inset: 0.42, period: 1.6, width: 0.05, colour: "#c8925a", avoid: 0.95}
        ],
        colours: {body: "#1c3170", edge: "#00000088", light: "#c8925a", accent: "#c8925a"}
      } },
    { id: "amber-black-banner", name: "Amber and Black Banner", family: "wizard-houses",
      material: {id: "velvet", tint: "#d9a320"},
      room: [0.8, 0.8],
      animation: [{kind: "sway", period: 3, amount: 0.12}],
      contour: {
        outer: {base: 1},
        inner: {base: 1},
        ends: {kind: "square"},
        body: {round: true, bevel: 0.04},
        layers: [
            {side: "outer", inset: 0.14, width: 0.05, material: {id: "black-metal"}},
            {side: "inner", inset: 0.2, width: 0.05, material: {id: "black-metal"}},
            {side: "outer", inset: -0.14, width: 0.2, free: true, material: {id: "black-metal"}}
        ],
        parts: [
            {kind: "ellipse", at: "both", on: "outer", s: -0.35, t: -0.14, w: 0.5, h: 0.5, align: "centre", material: {id: "black-metal"}, shade: "chrome", bevel: 0.04, z: "front"},
            {kind: "crystal", at: "both", on: "outer", s: -0.85, t: -0.04, angle: 90, w: 0.32, h: 0.8, align: "centre", material: {id: "black-metal"}, shade: "chrome", z: "front"}
        ],
        lines: [
            {kind: "fringe", side: "inner", length: 0.42, period: 0.075, width: 0.024, colour: "#202020", cord: "#202020"},
            {kind: "dots", side: "outer", inset: 0.42, period: 1.6, width: 0.05, colour: "#202020", avoid: 0.95}
        ],
        colours: {body: "#d9a320", edge: "#00000088", light: "#202020", accent: "#202020"}
      } },
    { id: "gilded-grimoire", name: "Gilded Grimoire", family: "wizard-tools",
      material: {id: "red-leather", tint: "#b0503f", relief: 0.6},
      room: [0.3, 1.9],
      animation: [{kind: "pulse", period: 3, amount: 0.45}],
      contour: {
        outer: {base: 1.15},
        inner: {base: 1.15},
        ends: {kind: "square"},
        body: {bevel: 0.1},
        layers: [
            {side: "outer", inset: 0.15, width: 0.04, material: {id: "polished-gold"}},
            {side: "outer", inset: 0.25, width: 0.018, material: {id: "polished-gold"}},
            {side: "inner", inset: 0.15, width: 0.04, material: {id: "polished-gold"}},
            {side: "inner", inset: 0.25, width: 0.018, material: {id: "polished-gold"}}
        ],
        parts: [
            {kind: "polygon", at: "both", on: "outer", points: [[0, 0.04], [1.45, 0.04], [1.45, 0], [1.446, -0.043], [1.435, -0.086], [1.417, -0.127], [1.391, -0.167], [1.36, -0.204], [1.322, -0.239], [1.28, -0.272], [1.233, -0.301], [1.182, -0.326], [1.129, -0.349], [1.074, -0.368], [1.018, -0.384], [1.045, -0.431], [1.069, -0.48], [1.089, -0.53], [1.105, -0.581], [1.116, -0.632], [1.121, -0.682], [1.12, -0.731], [1.113, -0.778], [1.1, -0.822], [1.081, -0.863], [1.056, -0.9], [1.025, -0.933], [0.989, -0.961], [0.948, -0.984], [0.903, -1.002], [0.854, -1.014], [0.803, -1.02], [0.749, -1.02], [0.694, -1.016], [0.638, -1.006], [0.582, -0.992], [0.527, -0.973], [0.474, -0.951], [0.422, -0.927], [0.404, -0.978], [0.383, -1.028], [0.359, -1.076], [0.33, -1.122], [0.298, -1.165], [0.263, -1.204], [0.224, -1.238], [0.183, -1.267], [0.14, -1.29], [0.094, -1.306], [0.047, -1.317], [0, -1.32]], material: {id: "polished-gold"}, shade: "chrome", bevel: 0.05, z: "front"},
            {kind: "polygon", at: "both", on: "inner", points: [[0, 0.04], [1.45, 0.04], [1.45, 0], [1.446, -0.043], [1.435, -0.086], [1.417, -0.127], [1.391, -0.167], [1.36, -0.204], [1.322, -0.239], [1.28, -0.272], [1.233, -0.301], [1.182, -0.326], [1.129, -0.349], [1.074, -0.368], [1.018, -0.384], [1.045, -0.431], [1.069, -0.48], [1.089, -0.53], [1.105, -0.581], [1.116, -0.632], [1.121, -0.682], [1.12, -0.731], [1.113, -0.778], [1.1, -0.822], [1.081, -0.863], [1.056, -0.9], [1.025, -0.933], [0.989, -0.961], [0.948, -0.984], [0.903, -1.002], [0.854, -1.014], [0.803, -1.02], [0.749, -1.02], [0.694, -1.016], [0.638, -1.006], [0.582, -0.992], [0.527, -0.973], [0.474, -0.951], [0.422, -0.927], [0.404, -0.978], [0.383, -1.028], [0.359, -1.076], [0.33, -1.122], [0.298, -1.165], [0.263, -1.204], [0.224, -1.238], [0.183, -1.267], [0.14, -1.29], [0.094, -1.306], [0.047, -1.317], [0, -1.32]], material: {id: "polished-gold"}, shade: "chrome", bevel: 0.05, z: "front"},
            {kind: "ellipse", at: "both", on: "outer", s: 0.3, t: -0.32, w: 0.3, h: 0.3, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"},
            {kind: "ellipse", at: "both", on: "inner", s: 0.3, t: -0.32, w: 0.3, h: 0.3, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"},
            {kind: "ring", at: "between", w: 0.95, h: 0.95, hole: 0.84, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"},
            {kind: "ellipse", at: "between", w: 0.62, h: 0.62, align: "centre", material: {id: "polished-gold"}, shade: "chrome", z: "front"},
            {kind: "box", at: "end", s: 1, w: 0.75, h: 2.7, r: 0.1, align: "centre", material: {id: "brown-leather"}, z: "front"},
            {kind: "box", at: "end", s: 1, w: 0.62, h: 0.78, r: 0.12, align: "centre", material: {id: "polished-gold"}, shade: "chrome", bevel: 0.05, z: "front"},
            {kind: "lens", at: "end", s: 1, w: 0.42, h: 0.42, align: "centre", colour: "#1fa85a", z: "front"}
        ],
        colours: {body: "#7a2a20", edge: "#000000aa", light: "#e3c46a", accent: "#e3c46a"}
      } },
    { id: "starship-viewport", name: "Starship Viewport", family: "starship-hull",
      material: {id: "hull-metal", tint: "#c3c9d0"},
      room: [0.2, 0.2],
      animation: [{kind: "drift", speed: 0.2}, {kind: "pulse", period: 1.6, amount: 0.9}],
      contour: {
        outer: {base: 1.75},
        inner: {base: 1.75},
        ends: {kind: "chamfer", size: 1},
        body: {bevel: 0.12},
        layers: [
            {side: "middle", width: 1.66, material: {id: "deep-field"}, window: true, z: "over", margin: 0.9},
            {side: "middle", offset: 0.88, width: 0.08, fill: "#0b0c0e", z: "over", margin: 0.9},
            {side: "middle", offset: -0.88, width: 0.08, fill: "#0b0c0e", z: "over", margin: 0.9},
            {side: "middle", offset: 0.99, width: 0.14, material: {id: "hull-metal", tint: "#c3c9d0"}, z: "over", margin: 0.8},
            {side: "middle", offset: -0.99, width: 0.14, material: {id: "hull-metal", tint: "#c3c9d0"}, z: "over", margin: 0.8}
        ],
        parts: [
            {kind: "box", at: "both", s: 0.6, w: 1.6, h: 3.8, chamfer: 0.32, align: "centre", material: {id: "black-metal"}, z: "front"},
            {kind: "box", at: "both", s: 0.6, w: 1.15, h: 2.9, chamfer: 0.2, align: "centre", material: {id: "hull-metal", tint: "#c3c9d0"}, z: "front"},
            {kind: "box", at: "both", s: 0.6, t: -1.05, w: 0.72, h: 0.12, r: 0.03, align: "centre", material: {id: "black-metal"}, z: "front"},
            {kind: "box", at: "both", s: 0.6, t: -0.75, w: 0.72, h: 0.12, r: 0.03, align: "centre", material: {id: "black-metal"}, z: "front"},
            {kind: "box", at: "both", s: 0.6, t: -0.45, w: 0.72, h: 0.12, r: 0.03, align: "centre", material: {id: "black-metal"}, z: "front"},
            {kind: "box", at: "both", s: 0.6, t: 0.45, w: 0.72, h: 0.12, r: 0.03, align: "centre", material: {id: "black-metal"}, z: "front"},
            {kind: "box", at: "both", s: 0.6, t: 0.75, w: 0.72, h: 0.12, r: 0.03, align: "centre", material: {id: "black-metal"}, z: "front"},
            {kind: "box", at: "both", s: 0.6, t: 1.05, w: 0.72, h: 0.12, r: 0.03, align: "centre", material: {id: "black-metal"}, z: "front"},
            {kind: "ellipse", at: "both", s: 0.15, t: 1.62, w: 0.16, h: 0.16, align: "centre", material: {id: "hull-metal", tint: "#c3c9d0"}, shade: "dome", z: "front"},
            {kind: "ellipse", at: "both", s: 1.05, t: 1.62, w: 0.16, h: 0.16, align: "centre", material: {id: "hull-metal", tint: "#c3c9d0"}, shade: "dome", z: "front"},
            {kind: "ellipse", at: "both", s: 0.15, t: -1.62, w: 0.16, h: 0.16, align: "centre", material: {id: "hull-metal", tint: "#c3c9d0"}, shade: "dome", z: "front"},
            {kind: "ellipse", at: "both", s: 1.05, t: -1.62, w: 0.16, h: 0.16, align: "centre", material: {id: "hull-metal", tint: "#c3c9d0"}, shade: "dome", z: "front"},
            {kind: "lens", at: "start", s: 0.6, w: 0.34, h: 0.34, align: "centre", colour: "#ff3b30", z: "front"},
            {kind: "lens", at: "end", s: 0.6, w: 0.34, h: 0.34, align: "centre", colour: "#45ff7a", z: "front"},
            {kind: "polygon", at: "at", where: [0.18], points: [[-0.5, -0.8], [0.1, -0.8], [0.7, 0.8], [0.1, 0.8]], fill: "#ffffff", opacity: 0.09, sheen: true, z: "front"},
            {kind: "polygon", at: "at", where: [0.47], points: [[-0.5, -0.8], [0.1, -0.8], [0.7, 0.8], [0.1, 0.8]], fill: "#ffffff", opacity: 0.09, sheen: true, z: "front"},
            {kind: "polygon", at: "at", where: [0.79], points: [[-0.5, -0.8], [0.1, -0.8], [0.7, 0.8], [0.1, 0.8]], fill: "#ffffff", opacity: 0.09, sheen: true, z: "front"},
            {kind: "polygon", at: "at", where: [0.2], points: [[0.25, -0.8], [0.4, -0.8], [1, 0.8], [0.85, 0.8]], fill: "#ffffff", opacity: 0.06, sheen: true, z: "front"},
            {kind: "polygon", at: "at", where: [0.49], points: [[0.25, -0.8], [0.4, -0.8], [1, 0.8], [0.85, 0.8]], fill: "#ffffff", opacity: 0.06, sheen: true, z: "front"},
            {kind: "polygon", at: "at", where: [0.81], points: [[0.25, -0.8], [0.4, -0.8], [1, 0.8], [0.85, 0.8]], fill: "#ffffff", opacity: 0.06, sheen: true, z: "front"}
        ],
        lines: [
            {kind: "panels", rows: 2, lengths: [0.8, 2.4], shades: ["#ffffff12", "#00000018", "#0000000a"], seam: "#00000070", width: 0.02},
            {kind: "pipes", count: 2, offset: 1.24, width: 0.08, gap: 0.03, period: 1.2, material: {id: "black-metal"}, margin: 1.5},
            {kind: "pipes", count: 2, offset: -1.24, width: 0.08, gap: 0.03, period: 1.6, material: {id: "hull-metal", tint: "#c3c9d0"}, margin: 1.5},
            {kind: "grilles", offset: 1.55, height: 0.24, count: 8, share: 0.45, louvre: "#5a6068", avoid: 0.95, margin: 1.5},
            {kind: "grilles", offset: -1.55, height: 0.24, count: 8, share: 0.45, louvre: "#5a6068", light: "#ff8a3a", avoid: 0.95, margin: 1.5},
            {kind: "greebles", rows: 1, span: [0, 0.06], sizes: [0.3, 1.2], gap: 0.08, seed: 101, margin: 1.5},
            {kind: "greebles", rows: 1, span: [0.94, 1], sizes: [0.3, 1.2], gap: 0.08, seed: 103, margin: 1.5},
            {kind: "dots", side: "middle", offset: 1.07, period: 0.36, width: 0.035, colour: "#a7adb5"},
            {kind: "dots", side: "middle", offset: -1.07, period: 0.36, width: 0.035, colour: "#a7adb5"}
        ],
        colours: {body: "#9ea6ae", edge: "#000000aa", light: "#ffffff", accent: "#45ff7a"}
      } },
    // Jewellery: each precious metal worked as a jeweller works it - wire
    // filigree, beaded milgrain edges, engine-turned engraving under
    // enamel, stones in their settings, rope twists and chains - in the
    // metal it is named for, polished and lit as jewellery is photographed.
    { id: "yellow-gold-filigree", name: "Yellow Gold Filigree", family: "jewellery",
      material: {id: "yellow-gold"},
      room: [0.2, 0.2],
      animation: [{kind: "twinkle", period: 2.8, amount: 0.45}],
      contour: {
        outer: {base: 0.92},
        inner: {base: 0.92},
        ends: {kind: "pill"},
        body: {fill: "none"},
        parts: [
            {kind: "ring", at: "between", w: 0.8, h: 0.8, hole: 0.76, align: "centre", material: {id: "polished-gold"}, shade: "chrome", gleam: 1.8, z: "front"},
            {kind: "gem", at: "between", w: 0.6, h: 0.6, align: "centre", material: {id: "diamond"}, z: "front"},
            {kind: "ellipse", at: "between", s: 0.42, t: 0.42, w: 0.12, h: 0.12, align: "centre", material: {id: "polished-gold"}, shade: "chrome", gleam: 1.8, z: "front"},
            {kind: "ellipse", at: "between", s: -0.42, t: -0.42, w: 0.12, h: 0.12, align: "centre", material: {id: "polished-gold"}, shade: "chrome", gleam: 1.8, z: "front"}
        ],
        lines: [
            {kind: "edge", side: "outer", pipe: true, width: 0.17, material: {id: "yellow-gold"}, gleam: 1.8},
            {kind: "edge", side: "inner", pipe: true, width: 0.17, material: {id: "yellow-gold"}, gleam: 1.8},
            {kind: "scrolls", period: 1.5, amp: 0.62, size: 0.24, width: 0.06, material: {id: "yellow-gold"}, gleam: 1.8, margin: 0.3},
            {kind: "dots", side: "outer", inset: 0.16, period: 0.15, width: 0.04, colour: "#f0cd62", gleam: 1.8},
            {kind: "dots", side: "inner", inset: 0.16, period: 0.15, width: 0.04, colour: "#f0cd62", gleam: 1.8}
        ],
        colours: {body: "#b8902f", edge: "#00000055", light: "#fff4d6", accent: "#e9c35a"}
      } },
    { id: "polished-gold-tennis", name: "Polished Gold Tennis Bracelet", family: "jewellery",
      material: {id: "polished-gold"},
      room: [0.2, 0.2],
      animation: [{kind: "twinkle", period: 2.4, amount: 0.5}],
      contour: {
        outer: {base: 0.6},
        inner: {base: 0.6},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1.8},
        parts: [
            {kind: "ellipse", at: "period", period: 0.86, w: 0.8, h: 0.8, align: "centre", material: {id: "polished-gold"}, shade: "chrome", gleam: 1.8, z: "front"},
            {kind: "gem", at: "period", period: 0.86, w: 0.64, h: 0.64, align: "centre", material: {id: "diamond"}, z: "front"}
        ],
        lines: [
            {kind: "dots", side: "middle", offset: 0.43, period: 0.43, width: 0.08, colour: "#f6d985", gleam: 1.8},
            {kind: "dots", side: "middle", offset: -0.43, period: 0.43, width: 0.08, colour: "#f6d985", gleam: 1.8}
        ],
        colours: {body: "#c9a13f", edge: "#00000055", light: "#ffffff", accent: "#f4d27c"}
      } },
    { id: "bright-gold-guilloche", name: "Bright Gold Guilloché", family: "jewellery",
      material: {id: "bright-gold"},
      room: [0.3, 0.3],
      animation: [{kind: "twinkle", period: 3, amount: 0.4}],
      contour: {
        outer: {base: 1},
        inner: {base: 1},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1.8, bevel: 0.1},
        layers: [
            {side: "middle", width: 1.32, fill: "#3456c4", opacity: 0.74, gloss: true, z: "over"}
        ],
        parts: [
            {kind: "ring", at: "between", w: 0.68, h: 0.68, hole: 0.76, align: "centre", material: {id: "bright-gold"}, shade: "chrome", gleam: 1.8, z: "front"},
            {kind: "gem", at: "between", w: 0.52, h: 0.52, align: "centre", material: {id: "diamond"}, z: "front"}
        ],
        lines: [
            {kind: "slats", period: 0.11, amp: 0.28, size: 0.66, width: 0.016, engrave: true, colour: "#6b4c10"},
            {kind: "slats", period: 0.11, amp: -0.28, size: 0.66, width: 0.016, engrave: true, colour: "#6b4c10"},
            {kind: "wave", period: 0.9, amp: 0.42, width: 0.026, engrave: true, colour: "#6b4c10"},
            {kind: "dots", side: "outer", inset: 0.13, period: 0.13, width: 0.06, colour: "#f6f0e4", metal: 0},
            {kind: "dots", side: "inner", inset: 0.13, period: 0.13, width: 0.06, colour: "#f6f0e4", metal: 0}
        ],
        colours: {body: "#d9ad2f", edge: "#00000066", light: "#ffffff", accent: "#f7d76a"}
      } },
    { id: "white-gold-tiara", name: "White Gold Diamond Tiara", family: "jewellery",
      material: {id: "white-gold"},
      room: [0.3, 0.3],
      animation: [{kind: "twinkle", period: 2.2, amount: 0.55}],
      contour: {
        outer: {base: 0.42, features: [{kind: "arch", length: 2.0, depth: 0.56, anchor: "icons"}]},
        inner: {base: 0.66},
        ends: {kind: "square"},
        body: {fill: "none"},
        parts: [
            {kind: "gem", at: "period", period: 0.36, on: "inner", t: -0.02, w: 0.3, h: 0.3, align: "centre", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "period", period: 0.26, on: "outer", t: -0.1, w: 0.2, h: 0.2, align: "centre", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "icons", on: "outer", t: 0.06, w: 0.38, h: 0.38, align: "base", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "icons", on: "outer", t: 0.46, w: 0.2, h: 0.2, align: "base", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "both", on: "inner", t: 0.3, w: 0.32, h: 0.32, align: "centre", material: {id: "diamond"}, z: "front"}
        ],
        lines: [
            {kind: "edge", side: "inner", pipe: true, width: 0.07, material: {id: "white-gold"}, gleam: 1.6},
            {kind: "edge", side: "outer", pipe: true, width: 0.06, material: {id: "white-gold"}, gleam: 1.6}
        ],
        colours: {body: "#d8d6d0", edge: "#00000055", light: "#ffffff", accent: "#eef2f8"}
      } },
    { id: "gold-leaf-byzantine", name: "Gold Leaf Byzantine Chain", family: "jewellery",
      material: {id: "gold-leaf"},
      room: [0.2, 0.2],
      animation: [{kind: "pulse", period: 3.6, amount: 0.15}],
      contour: {
        outer: {base: 0.8},
        inner: {base: 0.8},
        ends: {kind: "pill"},
        body: {fill: "none"},
        parts: [
            {kind: "ring", at: "period", period: 0.62, w: 0.86, h: 0.92, hole: 0.6, align: "centre", material: {id: "gold-leaf"}, shade: "chrome", gleam: 1.9, z: "front"},
            {kind: "capsule", at: "period", period: 0.62, phase: 0.5, w: 0.5, h: 0.22, align: "centre", material: {id: "polished-gold"}, shade: "chrome", gleam: 2, z: "front"}
        ],
        colours: {body: "#c9a54a", edge: "#00000055", light: "#fff2cc", accent: "#f2cf6a"}
      } },
    { id: "rose-gold-torsade", name: "Rose Gold Torsade", family: "jewellery",
      material: {id: "rose-gold"},
      room: [0.2, 0.2],
      animation: [{kind: "twinkle", period: 2.8, amount: 0.45}],
      contour: {
        outer: {base: 0.66},
        inner: {base: 0.66},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1.8},
        parts: [
            {kind: "ring", at: "between", w: 0.78, h: 0.78, hole: 0.76, align: "centre", material: {id: "rose-gold"}, shade: "chrome", gleam: 1.9, z: "front"},
            {kind: "gem", at: "between", w: 0.58, h: 0.58, align: "centre", material: {id: "pink-tourmaline"}, z: "front"},
            {kind: "gem", at: "between", s: 0.52, w: 0.2, h: 0.2, align: "centre", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "between", s: -0.52, w: 0.2, h: 0.2, align: "centre", material: {id: "diamond"}, z: "front"}
        ],
        lines: [
            {kind: "slats", period: 0.3, amp: 0.42, size: 0.68, width: 0.035, engrave: true, colour: "#6a3a2e", margin: 0.4}
        ],
        colours: {body: "#d89a86", edge: "#00000055", light: "#ffe2da", accent: "#f2b8a8"}
      } },
    { id: "red-gold-cabochon", name: "Red Gold Garnet Cabochons", family: "jewellery",
      material: {id: "red-gold"},
      room: [0.2, 0.2],
      animation: [{kind: "pulse", period: 3.4, amount: 0.2}],
      contour: {
        outer: {base: 0.92},
        inner: {base: 0.92},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1.7},
        parts: [
            {kind: "ring", at: "between", w: 0.98, h: 0.74, hole: 0.84, align: "centre", material: {id: "red-gold"}, shade: "chrome", gleam: 1.9, z: "front"},
            {kind: "gem", at: "between", cut: "cabochon", w: 0.82, h: 0.58, align: "centre", material: {id: "garnet"}, z: "front"}
        ],
        lines: [
            {kind: "middle", offset: 0.74, width: 0.025, engrave: true, colour: "#5a2a18"},
            {kind: "middle", offset: -0.74, width: 0.025, engrave: true, colour: "#5a2a18"},
            {kind: "dots", side: "outer", inset: 0.08, period: 0.14, width: 0.04, colour: "#f0a888", gleam: 1.8},
            {kind: "dots", side: "inner", inset: 0.08, period: 0.14, width: 0.04, colour: "#f0a888", gleam: 1.8}
        ],
        colours: {body: "#c86a50", edge: "#00000055", light: "#ffd0c0", accent: "#f0a888"}
      } },
    { id: "green-gold-nouveau", name: "Green Gold Art Nouveau", family: "jewellery",
      material: {id: "green-gold"},
      room: [0.2, 0.2],
      animation: [{kind: "twinkle", period: 3, amount: 0.4}],
      contour: {
        outer: {base: 0.9},
        inner: {base: 0.9},
        ends: {kind: "pill"},
        body: {fill: "none"},
        parts: [
            {kind: "ring", at: "between", w: 0.72, h: 0.72, hole: 0.76, align: "centre", material: {id: "green-gold"}, shade: "chrome", gleam: 1.8, z: "front"},
            {kind: "gem", at: "between", cut: "cabochon", w: 0.56, h: 0.56, align: "centre", material: {id: "emerald"}, z: "front"}
        ],
        lines: [
            {kind: "edge", side: "outer", pipe: true, width: 0.1, material: {id: "green-gold"}, gleam: 1.8},
            {kind: "edge", side: "inner", pipe: true, width: 0.1, material: {id: "green-gold"}, gleam: 1.8},
            {kind: "wave", period: 2.4, amp: 0.58, width: 0.11, pipe: true, material: {id: "green-gold"}, gleam: 1.8},
            {kind: "wave", period: 2.4, amp: 0.58, phase: 0.5, width: 0.07, pipe: true, material: {id: "green-gold"}, gleam: 1.8}
        ],
        colours: {body: "#b8b56a", edge: "#00000055", light: "#e8ffe0", accent: "#d7d58a"}
      } },
    { id: "blue-gold-channel", name: "Blue Gold Sapphire Channel", family: "jewellery",
      material: {id: "blue-gold"},
      room: [0.2, 0.2],
      animation: [{kind: "twinkle", period: 2.6, amount: 0.45}],
      contour: {
        outer: {base: 0.82},
        inner: {base: 0.82},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1.7},
        parts: [
            {kind: "gem", at: "period", period: 0.46, cut: "baguette", w: 0.4, h: 0.86, align: "centre", material: {id: "sapphire"}, z: "front"}
        ],
        lines: [
            {kind: "middle", offset: 0.5, pipe: true, width: 0.1, material: {id: "blue-gold"}, gleam: 1.9},
            {kind: "middle", offset: -0.5, pipe: true, width: 0.1, material: {id: "blue-gold"}, gleam: 1.9},
            {kind: "dots", side: "outer", inset: 0.1, period: 0.14, width: 0.04, colour: "#c8d8f4", gleam: 1.8},
            {kind: "dots", side: "inner", inset: 0.1, period: 0.14, width: 0.04, colour: "#c8d8f4", gleam: 1.8}
        ],
        colours: {body: "#8aa6d6", edge: "#00000055", light: "#d8e6ff", accent: "#c8d8f4"}
      } },
    { id: "purple-gold-halo", name: "Purple Gold Amethyst Halo", family: "jewellery",
      material: {id: "purple-gold"},
      room: [0.2, 0.2],
      animation: [{kind: "twinkle", period: 2.6, amount: 0.5}],
      contour: {
        outer: {base: 0.94},
        inner: {base: 0.94},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1.7},
        parts: [
            {kind: "gem", at: "between", w: 0.5, h: 0.5, halo: 12, align: "centre", material: {id: "amethyst"}, z: "front"}
        ],
        lines: [
            {kind: "scrolls", period: 1.6, amp: 0.66, size: 0.24, width: 0.035, engrave: true, grains: false, colour: "#3e2450"},
            {kind: "dots", side: "outer", inset: 0.1, period: 0.14, width: 0.04, colour: "#e2c8f4", gleam: 1.8},
            {kind: "dots", side: "inner", inset: 0.1, period: 0.14, width: 0.04, colour: "#e2c8f4", gleam: 1.8}
        ],
        colours: {body: "#a87ac8", edge: "#00000055", light: "#f0dcff", accent: "#e2c8f4"}
      } },
    { id: "black-gold-deco", name: "Black Gold Onyx Deco", family: "jewellery",
      material: {id: "black-gold"},
      room: [0.3, 0.3],
      animation: [{kind: "twinkle", period: 2.4, amount: 0.5}],
      contour: {
        outer: {base: 0.92},
        inner: {base: 0.92},
        ends: {kind: "chamfer", size: 0.5},
        body: {round: "chrome", coat: 0, gleam: 1.6, bevel: 0.1},
        layers: [
            {side: "middle", width: 1.1, material: {id: "obsidian"}, gloss: true}
        ],
        parts: [
            {kind: "gem", at: "period", period: 0.22, on: "outer", t: -0.12, w: 0.16, h: 0.16, align: "centre", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "period", period: 0.22, on: "inner", t: -0.12, w: 0.16, h: 0.16, align: "centre", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "between", cut: "princess", w: 0.46, h: 0.46, align: "centre", material: {id: "diamond"}, z: "front"}
        ],
        colours: {body: "#4d4743", edge: "#00000088", light: "#ffffff", accent: "#e8e4dc"}
      } },
    { id: "grey-gold-engine-turned", name: "Grey Gold Engine-Turned", family: "jewellery",
      material: {id: "grey-gold"},
      room: [0.3, 0.3],
      animation: [{kind: "twinkle", period: 3, amount: 0.4}],
      contour: {
        outer: {base: 1},
        inner: {base: 1},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1.5, bevel: 0.1},
        parts: [
            {kind: "ring", at: "between", w: 0.66, h: 0.66, hole: 0.76, align: "centre", material: {id: "grey-gold"}, shade: "chrome", gleam: 1.8, z: "front"},
            {kind: "gem", at: "between", w: 0.5, h: 0.5, align: "centre", material: {id: "diamond"}, z: "front"}
        ],
        lines: [
            {kind: "slats", period: 0.12, amp: 0.3, size: 0.68, width: 0.02, engrave: true, colour: "#4a4236"},
            {kind: "slats", period: 0.12, amp: -0.3, size: 0.68, width: 0.02, engrave: true, colour: "#4a4236"},
            {kind: "wave", period: 0.9, amp: 0.42, width: 0.03, engrave: true, colour: "#4a4236"},
            {kind: "dots", side: "outer", inset: 0.13, period: 0.13, width: 0.055, colour: "#f4eee2", metal: 0},
            {kind: "dots", side: "inner", inset: 0.13, period: 0.13, width: 0.055, colour: "#f4eee2", metal: 0}
        ],
        colours: {body: "#bdb8af", edge: "#00000066", light: "#ffffff", accent: "#ece6da"}
      } },
    { id: "silver-bright-cut", name: "Silver Bright-Cut", family: "jewellery",
      material: {id: "silver"},
      room: [0.2, 0.2],
      animation: [{kind: "pulse", period: 3.6, amount: 0.15}],
      contour: {
        outer: {base: 0.94},
        inner: {base: 0.94},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1},
        parts: [
            {kind: "ring", at: "between", w: 0.86, h: 0.66, hole: 0.82, align: "centre", material: {id: "silver"}, shade: "chrome", gleam: 1.1, z: "front"},
            {kind: "gem", at: "between", cut: "cabochon", w: 0.72, h: 0.52, align: "centre", colour: "#1e1d22", z: "front"},
            {kind: "gem", at: "period", period: 0.24, on: "outer", t: -0.13, w: 0.17, h: 0.17, align: "centre", colour: "#4a4e55", z: "front"},
            {kind: "gem", at: "period", period: 0.24, on: "inner", t: -0.13, w: 0.17, h: 0.17, align: "centre", colour: "#4a4e55", z: "front"}
        ],
        lines: [
            {kind: "scrolls", period: 1.6, amp: 0.56, size: 0.22, width: 0.04, engrave: true, grains: false, colour: "#3a3f46"}
        ],
        colours: {body: "#c8ccd2", edge: "#00000055", light: "#ffffff", accent: "#e6e9ee"}
      } },
    { id: "platinum-deco-baguettes", name: "Platinum Deco Baguettes", family: "jewellery",
      material: {id: "brushed-platinum"},
      room: [0.4, 0.4],
      animation: [{kind: "twinkle", period: 2.4, amount: 0.5}],
      contour: {
        outer: {base: 0.82},
        inner: {base: 0.82},
        ends: {kind: "chamfer", size: 0.5},
        body: {round: "chrome", coat: 0, gleam: 1.4, bevel: 0.1},
        parts: [
            {kind: "gem", at: "period", period: 0.6, cut: "baguette", w: 0.24, h: 0.7, align: "centre", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "period", period: 0.6, phase: 0.5, w: 0.36, h: 0.36, align: "centre", material: {id: "diamond"}, z: "front"},
            {kind: "gem", at: "both", s: 0.34, cut: "princess", w: 0.62, h: 0.62, align: "centre", material: {id: "diamond"}, z: "front"}
        ],
        lines: [
            {kind: "middle", offset: 0.45, pipe: true, width: 0.07, material: {id: "brushed-platinum"}, gleam: 1.6},
            {kind: "middle", offset: -0.45, pipe: true, width: 0.07, material: {id: "brushed-platinum"}, gleam: 1.6},
            {kind: "middle", offset: 0.66, width: 0.02, engrave: true, colour: "#3a3e44"},
            {kind: "middle", offset: -0.66, width: 0.02, engrave: true, colour: "#3a3e44"}
        ],
        colours: {body: "#d9dde2", edge: "#00000055", light: "#ffffff", accent: "#eef1f4"}
      } },
    { id: "copper-torc", name: "Copper Torc", family: "jewellery",
      material: {id: "polished-copper"},
      room: [0.7, 0.7],
      animation: [{kind: "pulse", period: 3.6, amount: 0.15}],
      contour: {
        outer: {base: 0.64},
        inner: {base: 0.64},
        ends: {kind: "pill"},
        body: {round: "chrome", coat: 0, gleam: 1.6},
        parts: [
            {kind: "ellipse", at: "both", s: 0.05, w: 1.0, h: 1.34, align: "centre", material: {id: "polished-copper"}, shade: "chrome", gleam: 1.8, z: "front"},
            {kind: "ring", at: "both", s: 0.05, w: 0.6, h: 0.82, hole: 0.62, align: "centre", material: {id: "polished-copper"}, shade: "chrome", gleam: 2, z: "front"}
        ],
        lines: [
            {kind: "slats", period: 0.36, amp: 0.5, size: 0.66, width: 0.045, engrave: true, colour: "#4a2410", margin: 0.7},
            {kind: "slats", period: 0.36, amp: 0.5, size: 0.66, width: 0.02, offset: 0.0, engrave: true, colour: "#6a3418", margin: 0.88}
        ],
        colours: {body: "#b87333", edge: "#00000055", light: "#ffd8b8", accent: "#e8a070"}
      } }
]

var decorationGroups = [
    {
        "name": "Sci-Fi",
        "subgroups": [
            {
                "name": "Alienware Style",
                "decorations": [
                    "alien-pods-green",
                    "alien-pods-white",
                    "alien-pods-black",
                    "alien-legend",
                    "alien-area",
                    "alien-talon",
                    "alien-vents",
                    "alien-core",
                    "obsidian-shard",
                    "alien-chassis-lunar",
                    "alien-chassis-dark",
                    "alien-invader-chrome",
                    "alien-darkstar",
                    "alien-hud",
                    "alien-vortex",
                    "alien-green-gauges"
                ]
            },
            {
                "name": "Starship Fronts",
                "decorations": [
                    "starship-red-core",
                    "starship-biomech",
                    "starship-command-deck"
                ]
            },
            {
                "name": "Starship Interiors",
                "decorations": [
                    "warp-nacelle"
                ]
            },
            {
                "name": "Starship Consoles",
                "decorations": [
                    "starship-light-table",
                    "starship-console-amber",
                    "starship-tactical"
                ]
            },
            {
                "name": "Starship Hulls",
                "decorations": [
                    "starship-viewport",
                    "collective-hull",
                    "raptor-hull",
                    "star-cruiser-hull"
                ]
            },
            {
                "name": "Starships",
                "decorations": [
                    "starship-console",
                    "starship-bridge",
                    "starship-alert",
                    "starship-deck",
                    "starship-hull",
                    "starship-warp",
                    "starship-saucer",
                    "starship-battle",
                    "starship-deckplate"
                ]
            },
            {
                "name": "Energy",
                "decorations": [
                    "plasma-blade-blue",
                    "plasma-blade-green",
                    "plasma-blade-red",
                    "plasma-blade-violet",
                    "twin-plasma-blade",
                    "laser-gate",
                    "plasma-arc",
                    "force-field"
                ]
            },
            {
                "name": "Galactic",
                "decorations": [
                    "carina-nebula",
                    "pillars-of-creation",
                    "southern-ring-nebula",
                    "tarantula-nebula",
                    "deep-field"
                ]
            },
            {
                "name": "Cyber and Neon",
                "decorations": [
                    "cyber-frame",
                    "neon-grid",
                    "holo-rail",
                    "circuit-trace",
                    "quantum-wave",
                    "plasma-conduit"
                ]
            },
            {
                "name": "Machines and Hives",
                "decorations": [
                    "mecha-plate",
                    "hex-hive",
                    "iridescent-shell"
                ]
            }
        ]
    },
    {
        "name": "Magic and Fantasy",
        "subgroups": [
            {
                "name": "Wizardry",
                "decorations": [
                    "wizard-arches",
                    "wizard-emerald",
                    "wizard-scarlet",
                    "wizard-sapphire",
                    "spellbook",
                    "magic-wand",
                    "potion-glass",
                    "starry-night",
                    "gilded-porcelain"
                ]
            },
            {
                "name": "Wizard's Tools",
                "decorations": [
                    "racing-broom",
                    "knotted-wand",
                    "gilded-grimoire"
                ]
            },
            {
                "name": "Wizard's Hall",
                "decorations": [
                    "floating-candles",
                    "wizards-map"
                ]
            },
            {
                "name": "House Banners",
                "decorations": [
                    "crimson-gold-banner",
                    "emerald-silver-banner",
                    "sapphire-bronze-banner",
                    "amber-black-banner"
                ]
            },
            {
                "name": "Gothic and Royal",
                "decorations": [
                    "gothic-cathedral",
                    "royal-crown",
                    "castle-battlement"
                ]
            },
            {
                "name": "Creatures",
                "decorations": [
                    "dragon-scale",
                    "serpent-coil",
                    "owl-feather"
                ]
            },
            {
                "name": "Runes and Wild Magic",
                "decorations": [
                    "runestone",
                    "celtic-knot",
                    "frost-crystal",
                    "enchanted-moss",
                    "elven-vine",
                    "runic-stone"
                ]
            }
        ]
    },
    {
        "name": "Nature",
        "subgroups": [
            {
                "name": "Vines",
                "decorations": [
                    "twisted-vine",
                    "grapevine",
                    "jungle-liana"
                ]
            },
            {
                "name": "Branches",
                "decorations": [
                    "oak-branch",
                    "birch-branch",
                    "plane-tree-branch",
                    "driftwood"
                ]
            },
            {
                "name": "Trunks and Logs",
                "decorations": [
                    "mossy-trunk",
                    "old-oak-trunk",
                    "cedar-log"
                ]
            },
            {
                "name": "Stone and Ruins",
                "decorations": [
                    "cracked-pillar",
                    "marble-column",
                    "broken-concrete-beam",
                    "river-pebbles"
                ]
            },
            {
                "name": "Plants and Water",
                "decorations": [
                    "bamboo-grove",
                    "ocean-tide"
                ]
            }
        ]
    },
    {
        "name": "Precious Metals",
        "subgroups": [
            {
                "name": "Jewellery",
                "decorations": [
                    "yellow-gold-filigree",
                    "polished-gold-tennis",
                    "bright-gold-guilloche",
                    "white-gold-tiara",
                    "gold-leaf-byzantine",
                    "rose-gold-torsade",
                    "red-gold-cabochon",
                    "green-gold-nouveau",
                    "blue-gold-channel",
                    "purple-gold-halo",
                    "black-gold-deco",
                    "grey-gold-engine-turned",
                    "silver-bright-cut",
                    "platinum-deco-baguettes",
                    "copper-torc"
                ]
            },
            {
                "name": "Gold",
                "decorations": [
                    "golden-throne",
                    "bright-gold-crown",
                    "gold-leaf-scroll",
                    "white-gold-halo",
                    "rose-gold-bloom",
                    "red-gold-ember",
                    "green-gold-leaf",
                    "blue-gold-frost",
                    "purple-gold-mystic",
                    "black-gold-noir",
                    "grey-gold-frame"
                ]
            },
            {
                "name": "Silver and Platinum",
                "decorations": [
                    "silver-moon",
                    "platinum-edge"
                ]
            }
        ]
    },
    {
        "name": "Gems and Stones",
        "subgroups": [
            {
                "name": "Cut Gems",
                "decorations": [
                    "diamond-facet",
                    "crystal-spire",
                    "amethyst-geode",
                    "sapphire-crown",
                    "ruby-cathedral",
                    "emerald-arches",
                    "topaz-rays",
                    "aquamarine-wave",
                    "pink-tourmaline-bloom",
                    "peridot-vine",
                    "citrine-sun",
                    "garnet-shard"
                ]
            },
            {
                "name": "Banded Stones and Marble",
                "decorations": [
                    "opal-dream",
                    "chrysocolla-ripple",
                    "carnelian-band",
                    "honey-onyx-talon",
                    "red-jasper-totem",
                    "moss-agate-grove",
                    "agate-flow",
                    "nero-portoro",
                    "black-marble-cathedral",
                    "white-marble-halo"
                ]
            }
        ]
    },
    {
        "name": "World Styles",
        "subgroups": [
            {
                "name": "Exotic",
                "decorations": [
                    "volcanic-rift",
                    "moroccan-mosaic",
                    "azure-palace"
                ]
            },
            {
                "name": "Classic Styles",
                "decorations": [
                    "art-deco",
                    "steampunk-brass"
                ]
            },
            {
                "name": "Fun",
                "decorations": [
                    "candy-pop"
                ]
            }
        ]
    }
]

var materials = [
    {id: "yellow-gold", name: "Yellow Gold", texture: "gold", group: "precious", relief: 0.35, metal: 1, rough: 0.22},
    {id: "polished-gold", name: "Polished Gold", texture: "gold-polished", group: "precious", relief: 0.3, metal: 1, rough: 0.22},
    {id: "bright-gold", name: "Bright Gold", texture: "gold-bright", group: "precious", relief: 0.3, metal: 1, rough: 0.22},
    {id: "gold-leaf", name: "Gold Leaf", texture: "gold-foil", group: "precious", relief: 0.55, metal: 1, rough: 0.22},
    {id: "white-gold", name: "White Gold", texture: "gold-neutral", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#efe9dc"},
    {id: "rose-gold", name: "Rose Gold", texture: "gold-neutral", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#f1b3a3"},
    {id: "red-gold", name: "Red Gold", texture: "gold-neutral", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#e98b6f"},
    {id: "green-gold", name: "Green Gold", texture: "gold-neutral", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#d7d58a"},
    {id: "blue-gold", name: "Blue Gold", texture: "gold-neutral", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#9ab6e6"},
    {id: "purple-gold", name: "Purple Gold", texture: "gold-neutral", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#b98ad8"},
    {id: "black-gold", name: "Black Gold", texture: "gold-neutral", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#4d4743"},
    {id: "grey-gold", name: "Grey Gold", texture: "gold-neutral", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#bdb8af"},
    {id: "silver", name: "Silver", texture: "silver", group: "precious", relief: 0.3, metal: 1, rough: 0.22},
    {id: "brushed-platinum", name: "Brushed Platinum", texture: "platinum", group: "precious", relief: 0.35, metal: 1, rough: 0.22, tint: "#e3e6ea"},
    {id: "polished-copper", name: "Polished Copper", texture: "copper", group: "precious", relief: 0.45, metal: 1, rough: 0.22},
    {id: "diamond", name: "Diamond", texture: "crystal-neutral", group: "gem", relief: 0.75, metal: 0, rough: 0.08, tint: "#f3f8ff"},
    {id: "rock-crystal", name: "Rock Crystal", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#e2eef7"},
    {id: "amethyst", name: "Amethyst", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#a066dc"},
    {id: "sapphire", name: "Sapphire", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#3d6be0"},
    {id: "ruby", name: "Ruby", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#e0304a"},
    {id: "emerald", name: "Emerald", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#2fbf74"},
    {id: "topaz", name: "Topaz", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#f5b041"},
    {id: "aquamarine", name: "Aquamarine", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#86e0ee"},
    {id: "pink-tourmaline", name: "Pink Tourmaline", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#f07ab0"},
    {id: "peridot", name: "Peridot", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#b5d84a"},
    {id: "citrine", name: "Citrine", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#f6cf4f"},
    {id: "garnet", name: "Garnet", texture: "crystal-neutral", group: "gem", relief: 0.7, metal: 0, rough: 0.08, tint: "#9a2033"},
    {id: "blue-agate", name: "Blue Agate", texture: "agate", group: "stone", relief: 0.35, metal: 0, rough: 0.25},
    {id: "carnelian", name: "Carnelian", texture: "carnelian", group: "stone", relief: 0.35, metal: 0, rough: 0.25},
    {id: "honey-onyx", name: "Honey Onyx", texture: "amber-band", group: "stone", relief: 0.35, metal: 0, rough: 0.25},
    {id: "opal-swirl", name: "Opal Swirl", texture: "opal", group: "stone", relief: 0.35, metal: 0, rough: 0.25},
    {id: "red-jasper", name: "Red Jasper", texture: "red-jasper", group: "stone", relief: 0.35, metal: 0, rough: 0.25},
    {id: "chrysocolla", name: "Chrysocolla", texture: "malachite", group: "stone", relief: 0.35, metal: 0, rough: 0.25},
    {id: "moss-agate", name: "Moss Agate", texture: "moss-agate", group: "stone", relief: 0.35, metal: 0, rough: 0.25},
    {id: "obsidian", name: "Obsidian", texture: "obsidian", group: "stone", relief: 0.3, metal: 0, rough: 0.25},
    {id: "black-and-gold-marble", name: "Black and Gold Marble", texture: "black-gold-vein", group: "stone", relief: 0.3, metal: 1, rough: 0.25},
    {id: "black-marble", name: "Black Marble", texture: "black-marble", group: "stone", relief: 0.3, metal: 0, rough: 0.25},
    {id: "white-marble", name: "White Marble", texture: "marble", group: "stone", relief: 0.3, metal: 0, rough: 0.25},
    {id: "green-ice", name: "Green Ice Crystal", texture: "ice-crystal", group: "stone", relief: 0.5, metal: 0, rough: 0.08},
    {id: "dark-slate", name: "Dark Slate Rock", texture: "dark-rock", group: "stone", relief: 0.7, metal: 0, rough: 0.85},
    {id: "castle-stone", name: "Castle Stone", texture: "castle-stone", group: "stone", relief: 0.75, metal: 0, rough: 0.85, scale: 2},
    {id: "volcanic-rock", name: "Volcanic Rock", texture: "volcanic", group: "stone", relief: 0.65, metal: 0, rough: 0.85},
    {id: "dark-honeycomb", name: "Dark Honeycomb Metal", texture: "alien-metal", group: "metal", relief: 0.55, metal: 0.25, rough: 0.35, coat: 1},
    {id: "white-honeycomb", name: "White Honeycomb Shell", texture: "alien-plastic", group: "metal", relief: 0.5, metal: 0, rough: 0.18},
    {id: "diamond-plate", name: "Diamond Plate Steel", texture: "diamond-plate", group: "metal", relief: 0.7, metal: 1, rough: 0.55},
    {id: "corrugated-steel", name: "Corrugated Steel", texture: "corrugated", group: "metal", relief: 0.6, metal: 1, rough: 0.55},
    {id: "hull-metal", name: "Scratched Hull Metal", texture: "hull-metal", group: "metal", relief: 0.4, metal: 1, rough: 0.55},
    {id: "battle-plates", name: "Battle-worn Plates", texture: "battle-plates", group: "metal", relief: 0.6, metal: 1, rough: 0.55},
    {id: "perforated-deck", name: "Perforated Deck", texture: "perforated", group: "metal", relief: 0.7, metal: 1, rough: 0.55},
    {id: "carbon-weave", name: "Carbon Weave", texture: "carbon", group: "metal", relief: 0.45, metal: 0.25, rough: 0.35, coat: 1},
    {id: "console-plastic", name: "Console Plastic", texture: "console-plastic", group: "metal", relief: 0.2, metal: 0, rough: 0.18},
    {id: "iridescent-shell", name: "Iridescent Shell", texture: "iridescent", group: "metal", relief: 0.4, metal: 0, rough: 0.55},
    {id: "dark-walnut", name: "Dark Walnut", texture: "walnut", group: "natural", relief: 0.45, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "cherry-wood", name: "Cherry Wood", texture: "warm-wood", group: "natural", relief: 0.45, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "bamboo", name: "Bamboo", texture: "bamboo", group: "natural", relief: 0.6, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "brown-leather", name: "Brown Leather", texture: "leather-brown", group: "natural", relief: 0.45, metal: 0, rough: 0.75},
    {id: "red-leather", name: "Red Leather", texture: "leather-red", group: "natural", relief: 0.45, metal: 0, rough: 0.75},
    {id: "serpent-skin", name: "Serpent Skin", texture: "serpent", group: "natural", relief: 0.6, metal: 0, rough: 0.75},
    {id: "parchment", name: "Parchment", texture: "parchment", group: "natural", relief: 0.5, metal: 0, rough: 0.75},
    {id: "velvet", name: "Velvet", texture: "velvet", group: "natural", relief: 0.4, metal: 0, rough: 0.75},
    {id: "moss", name: "Moss", texture: "moss", group: "natural", relief: 0.7, metal: 0, rough: 0.75},
    {id: "gilded-porcelain", name: "Gilded Porcelain", texture: "gilded-porcelain", group: "natural", relief: 0.35, metal: 0, rough: 0.55},
    {id: "moroccan-mosaic", name: "Moroccan Mosaic", texture: "mosaic", group: "natural", relief: 0.45, metal: 0, rough: 0.85},
    {id: "azure-tiles", name: "Azure Tiles", texture: "azure-tiles", group: "natural", relief: 0.5, metal: 0, rough: 0.85},
    {id: "oak-bark", name: "Oak Bark", texture: "bark-oak", group: "natural", relief: 0.85, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "grey-bark", name: "Grey Bark", texture: "bark-grey", group: "natural", relief: 0.75, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "mossy-bark", name: "Mossy Bark", texture: "bark-mossy", group: "natural", relief: 0.75, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "birch-bark", name: "Birch Bark", texture: "bark-birch", group: "natural", relief: 0.45, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "plane-tree-bark", name: "Plane Tree Bark", texture: "bark-plane", group: "natural", relief: 0.4, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "cedar-bark", name: "Cedar Bark", texture: "bark-cedar", group: "natural", relief: 0.7, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "brown-bark", name: "Brown Bark", texture: "bark-brown", group: "natural", relief: 0.7, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "sawn-log-end", name: "Sawn Log End", texture: "log-end", group: "natural", relief: 0.5, metal: 0, rough: 0.55},
    {id: "pale-log-end", name: "Pale Log End", texture: "log-end-pale", group: "natural", relief: 0.45, metal: 0, rough: 0.55},
    {id: "straw", name: "Straw", texture: "straw", group: "natural", relief: 0.7, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "old-straw", name: "Old Straw", texture: "straw-grey", group: "natural", relief: 0.7, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "hemp-rope", name: "Hemp Rope", texture: "rope", group: "natural", relief: 0.85, metal: 0, rough: 0.75},
    {id: "pine-wood", name: "Pine Wood", texture: "wood-pine", group: "natural", relief: 0.4, metal: 0, rough: 0.75, grain: "vertical"},
    {id: "aged-paper", name: "Aged Paper", texture: "paper-aged", group: "natural", relief: 0.15, metal: 0, rough: 0.75},
    {id: "steel-cable", name: "Steel Cable", texture: "steel-rope", group: "metal", relief: 0.8, metal: 1, rough: 0.55},
    {id: "black-metal", name: "Black Metal", texture: "metal-black", group: "metal", relief: 0.3, metal: 1, rough: 0.55},
    {id: "hazard-stripes", name: "Hazard Stripes", texture: "paint-hazard", group: "metal", relief: 0.5, metal: 0, rough: 0.55},
    {id: "worn-white-paint", name: "Worn White Paint", texture: "paint-white", group: "metal", relief: 0.45, metal: 0.3, rough: 0.5},
    {id: "worn-red-paint", name: "Worn Red Paint", texture: "paint-red", group: "metal", relief: 0.4, metal: 0.3, rough: 0.5},
    {id: "worn-green-paint", name: "Worn Green Paint", texture: "paint-green", group: "metal", relief: 0.45, metal: 0.3, rough: 0.5},
    {id: "rubber-grip", name: "Rubber Grip", texture: "rubber", group: "synthetic", relief: 0.5, metal: 0, rough: 0.55},
    {id: "grained-rubber", name: "Grained Rubber", texture: "rubber-grain", group: "synthetic", relief: 0.5, metal: 0, rough: 0.55},
    {id: "black-gloss-plastic", name: "Black Gloss Plastic", texture: "plastic-black", group: "synthetic", relief: 0, metal: 0, rough: 0.18},
    {id: "yellow-plastic", name: "Yellow Plastic", texture: "plastic-yellow", group: "synthetic", relief: 0.2, metal: 0, rough: 0.18},
    {id: "travertine", name: "Travertine", texture: "travertine", group: "stone", relief: 0.45, metal: 0, rough: 0.85},
    {id: "banded-travertine", name: "Banded Travertine", texture: "travertine-banded", group: "stone", relief: 0.45, metal: 0, rough: 0.85},
    {id: "blue-travertine", name: "Blue Travertine", texture: "travertine-blue", group: "stone", relief: 0.45, metal: 0, rough: 0.85},
    {id: "concrete", name: "Concrete", texture: "concrete", group: "stone", relief: 0.5, metal: 0, rough: 0.85},
    {id: "old-plaster", name: "Old Plaster", texture: "plaster", group: "stone", relief: 0.55, metal: 0, rough: 0.85},
    {id: "polished-white-marble", name: "Polished White Marble", texture: "marble-polished", group: "stone", relief: 0.25, metal: 0, rough: 0.25},
    {id: "river-pebbles", name: "River Pebbles", texture: "pebbles", group: "stone", relief: 0.9, metal: 0, rough: 0.85},
    {id: "gravel", name: "Gravel", texture: "gravel", group: "stone", relief: 0.7, metal: 0, rough: 0.85},
    {id: "carina-nebula", name: "Carina Nebula", texture: "nebula-carina", group: "space", relief: 0.12, metal: 0, rough: 0.18, strip: true, gloss: true},
    {id: "pillars-of-creation", name: "Pillars of Creation", texture: "nebula-pillars", group: "space", relief: 0.12, metal: 0, rough: 0.18, strip: true, gloss: true},
    {id: "southern-ring-nebula", name: "Southern Ring Nebula", texture: "nebula-southern-ring", group: "space", relief: 0.12, metal: 0, rough: 0.18, strip: true, gloss: true},
    {id: "tarantula-nebula", name: "Tarantula Nebula", texture: "nebula-tarantula", group: "space", relief: 0.12, metal: 0, rough: 0.18, strip: true, gloss: true},
    {id: "deep-field", name: "Deep Field", texture: "deep-field", group: "space", relief: 0.08, metal: 0, rough: 0.18, strip: true, gloss: true}
]

var materialGroups = [
    {
        "name": "Gems",
        "subgroups": [
            {
                "name": "Cut Gems",
                "materials": [
                    "diamond",
                    "rock-crystal",
                    "amethyst",
                    "sapphire",
                    "ruby",
                    "emerald",
                    "topaz",
                    "aquamarine",
                    "pink-tourmaline",
                    "peridot",
                    "citrine",
                    "garnet"
                ]
            },
            {
                "name": "Banded Gems",
                "materials": [
                    "blue-agate",
                    "carnelian",
                    "honey-onyx",
                    "opal-swirl",
                    "red-jasper",
                    "chrysocolla",
                    "moss-agate"
                ]
            }
        ]
    },
    {
        "name": "Stones",
        "subgroups": [
            {
                "name": "Marble and Obsidian",
                "materials": [
                    "obsidian",
                    "black-and-gold-marble",
                    "black-marble",
                    "white-marble",
                    "polished-white-marble"
                ]
            },
            {
                "name": "Building Stone",
                "materials": [
                    "travertine",
                    "banded-travertine",
                    "blue-travertine",
                    "concrete",
                    "old-plaster",
                    "castle-stone"
                ]
            },
            {
                "name": "Rock and Ice",
                "materials": [
                    "green-ice",
                    "dark-slate",
                    "volcanic-rock",
                    "river-pebbles",
                    "gravel"
                ]
            },
            {
                "name": "Tiles and Porcelain",
                "materials": [
                    "gilded-porcelain",
                    "moroccan-mosaic",
                    "azure-tiles"
                ]
            }
        ]
    },
    {
        "name": "Metals",
        "subgroups": [
            {
                "name": "Sci-Fi Metal",
                "materials": [
                    "dark-honeycomb",
                    "white-honeycomb",
                    "iridescent-shell"
                ]
            },
            {
                "name": "Industrial",
                "materials": [
                    "diamond-plate",
                    "corrugated-steel",
                    "hull-metal",
                    "battle-plates",
                    "perforated-deck",
                    "black-metal",
                    "steel-cable"
                ]
            },
            {
                "name": "Painted Metal",
                "materials": [
                    "hazard-stripes",
                    "worn-white-paint",
                    "worn-red-paint",
                    "worn-green-paint"
                ]
            }
        ]
    },
    {
        "name": "Precious Metals",
        "subgroups": [
            {
                "name": "Gold",
                "materials": [
                    "yellow-gold",
                    "polished-gold",
                    "bright-gold",
                    "gold-leaf",
                    "white-gold",
                    "rose-gold",
                    "red-gold",
                    "green-gold",
                    "blue-gold",
                    "purple-gold",
                    "black-gold",
                    "grey-gold"
                ]
            },
            {
                "name": "Silver and Platinum",
                "materials": [
                    "silver",
                    "brushed-platinum"
                ]
            },
            {
                "name": "Copper",
                "materials": [
                    "polished-copper"
                ]
            }
        ]
    },
    {
        "name": "Organic",
        "subgroups": [
            {
                "name": "Wood and Bamboo",
                "materials": [
                    "dark-walnut",
                    "cherry-wood",
                    "pine-wood",
                    "bamboo"
                ]
            },
            {
                "name": "Bark and Logs",
                "materials": [
                    "oak-bark",
                    "grey-bark",
                    "mossy-bark",
                    "birch-bark",
                    "plane-tree-bark",
                    "cedar-bark",
                    "brown-bark",
                    "sawn-log-end",
                    "pale-log-end"
                ]
            },
            {
                "name": "Leather and Skin",
                "materials": [
                    "brown-leather",
                    "red-leather",
                    "serpent-skin"
                ]
            },
            {
                "name": "Straw, Rope and Fabric",
                "materials": [
                    "straw",
                    "old-straw",
                    "hemp-rope",
                    "parchment",
                    "aged-paper",
                    "velvet"
                ]
            },
            {
                "name": "Living",
                "materials": [
                    "moss"
                ]
            }
        ]
    },
    {
        "name": "Synthetics",
        "subgroups": [
            {
                "name": "Plastics",
                "materials": [
                    "console-plastic",
                    "black-gloss-plastic",
                    "yellow-plastic"
                ]
            },
            {
                "name": "Carbon and Rubber",
                "materials": [
                    "carbon-weave",
                    "rubber-grip",
                    "grained-rubber"
                ]
            }
        ]
    },
    {
        "name": "Space",
        "subgroups": [
            {
                "name": "Nebulae",
                "materials": [
                    "carina-nebula",
                    "pillars-of-creation",
                    "southern-ring-nebula",
                    "tarantula-nebula"
                ]
            },
            {
                "name": "Galaxies",
                "materials": [
                    "deep-field"
                ]
            }
        ]
    }
]

var byId = ({})
for (var index = 0; index < decorations.length; ++index)
    byId[decorations[index].id] = decorations[index]
var materialById = ({})
for (var k = 0; k < materials.length; ++k)
    materialById[materials[k].id] = materials[k]

// The decoration with this id, or null.
function decoration(id) {
    return byId[String(id || "")] || null
}

// The named material with this id, or null.
function material(id) {
    return materialById[String(id || "")] || null
}
