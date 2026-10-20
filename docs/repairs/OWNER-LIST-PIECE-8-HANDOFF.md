# Owner list, piece 8: panel decorations - handoff for the next session

**Status (2026-10-20, second session):** steps 1 to 7 below are done and
reported in `OWNER-LIST-PIECE-8.md` (part one); step 8 (3D platform
decorations, then holographic icons) is next. This file stays as the
record of the checkpoint.

Written 2026-10-20 at the owner's word: "Commit and Sync current changes,
make a handoff for tomorrow and stop". Piece 8 is Phase 5 of the owner's
plan: decorations that remake a panel's outline (any panel shape, any
edge), then holographic icons. It is **in progress**; this commit is a
checkpoint, not the piece's close.

## Where it stands

**The designs are finished and approved.** 145 decorations, every one
ticked by the owner in its final look:

- 90 first designs (Alienware style, starships, cyber and neon, wizardry,
  gothic, creatures, runes, precious metals, cut gems, banded stones,
  world styles);
- 55 realistic ones made after the owner's references: starship fronts in
  the M11x-R2 look (black chrome, red-lens pods, hoses), film consoles
  after Oblivion's light table, hulls after Star Trek's cube ship and
  raptor-like warships, the Starship Viewport (one long window to space in
  a thick mechanical frame), Alienware's own themes (Invader, Darkstar,
  Breed, HUD), plasma blades, laser gate, plasma arc, force field, five
  Webb nebulae, Harry Potter-inspired tools (racing broom, knotted wand,
  gilded grimoire, floating candles, wizard's map, four house banners),
  vines, branches, trunks, stone ruins.

**The approved look is lit per pixel**: real height, studio reflections
on metal and lacquer, shadows, glass lenses, glowing lights with their
halo. The owner ticked this look over the earlier flat drawings.

**Colour settings are designed and approved**: every decoration can change
colour (owner rule). Light colour, main colour (keeps the material's
grain), palette for multi-colour ones, picture hue, strength and
brightness for the nebulae, metal for trims, and the finishes chameleon,
iridescent, rainbow and metallic rainbow. Rainbow also works for lights.

**Rulings to keep**: arches stand only over icons and centre on them; no
leaf or flower sprites; nothing named or marked after a film or brand;
every decoration faces into the screen on every edge; default animations
are on; a decoration may have a launcher seat (the Raptor's head holds
the app launcher). The full list is in the owner rulings in memory
(`owner-list-piece-8`, `decorations-look-like-their-name`,
`decoration-colour-settings`).

## What this commit contains

- `qml/ArchDock/Rendering/ContourFrames.js`: the decoration engine. It
  shapes the band (edges, ends, segments), the parts (pods, hilts,
  branches, lenses, flames and the rest) and the lines (lights, runes,
  machinery and the rest), on any path. Edit this file directly from now
  on.
- `qml/ArchDock/Rendering/DecorationCatalogue.js`: all 145 decorations,
  the 101 named materials and the tree Studio will show (group, subgroup,
  item), with each decoration's default animations.
- `qml/ArchDock/Rendering/DecorationScene.js`: turns a decoration on a
  panel's path into the lit scene, with the colour choices, the animation
  groups and the area it covers. It is the app's copy of the design
  pipeline's exporter. It is written but **not yet run or tested in the
  app**.
- `qml/ArchDock/Rendering/materials/deco-*`: 83 textures (photographs as
  JPEG, height maps as grey PNG, 10 MB), credited in `materials/CREDITS.md`
  (ambientCG CC0 sets; NASA, ESA, CSA, STScI for the Webb pictures).
- `tools/generate-material-textures.py`: the `--decorations` and
  `--nebulae` stages that make those textures.
- `src/themes/ThemePackage.cpp`, `src/model/ThemeDefinition.*`: an imported
  package may carry a `contour`. The checks cover only the first engine
  vocabulary; the realistic members (parts, layers, body, the new line
  kinds, launcher) still need adding before packages can use them.
- `CMakeLists.txt`: installs the three modules and the textures.

Nothing in the app uses the decorations yet: no setting, no renderer, no
Studio page. This commit changes nothing the owner can see.

## The design pipeline (outside the repo)

`/mnt/F/archdock-audit-2026-10-08/decorations-design-p8/` holds the tools
that made every picture the owner approved, and the reference images:

- `lit.py` is the approved lighting, **the reference for the C++ port**.
  It is Python with numpy and scipy, both in the audit venv
  `/mnt/F/archdock-audit-2026-10-08/venv-pyside6`.
- `export-scene.js` is the exporter that `DecorationScene.js` copies.
- `out/lit/<id>.png` are the approved renders of every decoration, to
  compare the app's output against. `out/scenes/<id>.json` are their
  scenes.
- `write-catalogue.js` wrote the catalogue from the design sources:
  `catalogue-*.js`, `redesign5.js`, `mech.js`, `groups.js` and
  `materials.js`.
- To run them from that folder:
  `SCALE=2 node export-scene.js <id>`, then
  `<venv>/bin/python lit.py $PWD out/scenes/<id>.json`.
  Colour choices go in with
  `node export-scene.js --choices <id> <name> '<json>'`.

Research notes:
`/mnt/F/archdock-audit-2026-10-08/samples-research/PHASE5-DECORATIONS-RESEARCH.md`
(sources, licences, Alienware themes).
`/mnt/F/archdock-audit-2026-10-08/scifi-ui-research/` (GMUNK's Oblivion
pictures).
`/mnt/F/archdock-audit-2026-10-08/alienware-research/` (KDE Store ports of
the AlienGUIse themes).
Source sets: `ambientcg/`, `nasa-webb/`.

## Next steps, in order

1. **C++ lighting.** Port `lit.py` to a new C++ QML plugin,
   `ArchDock.Decorations`, built like `ArchDock.Input` so plasmashell can
   load it. Use QPainter coverage per element box, an exact distance
   transform (Felzenszwalb), separable max and blur filters, the same
   profiles, studio light, coat, emission rules, shadows, AO and halos.
   Bake off the GUI thread into a base image plus one light layer per
   group: lights, stars, blinkA and blinkB, arcs. Lay strips (nebulae) and
   upright grain along the band's path, as `paintWrappedMaterial` does.
   Test it against `out/lit/*.png`.
2. **Renderer.** Add `renderers/PanelDecoration2D.qml` and list it in
   CMake again. It takes the path from `LayoutEngine.surface`, the
   half-width as icon size / 1.7, and the icons' places as slots
   (`DecorationScene.slotsOnPath`). It builds the scene (WorkerScript if
   the GUI thread stalls), shows the layers at `scene.bounds`, and runs the
   animations. Light kinds become layer opacity or hue. Motion kinds
   (waves, drift, flow, sway, walk, spin) become an 8 to 12 frame flipbook
   baked at phases; this needs a `phase` option in the engine. Wire it into
   `PanelSurfaceLoader.qml` for the 2D tiers. Add the decoration's reach
   to `PanelScene`'s effect margins, as `pedestalHeadroom` does.
3. **Settings.** In `PanelSettingsSchema`, `PanelDefinition` and
   `PanelWindowSettings`, add `panelDecoration` (id),
   `panelDecorationLight` and `panelDecorationLightFinish`,
   `panelDecorationMain` and `panelDecorationFinish`,
   `panelDecorationPalette`, `panelDecorationMetal`, `panelDecorationHue`,
   `panelDecorationStrength`, `panelDecorationBrightness` and
   `panelDecorationAnimation` (on by default). New editor capabilities:
   decoration, decoration-colours. Update the truth matrix and the
   capability tests.
4. **Studio.** On Appearance, under "Panel decorations", add the tree
   (group, subgroup, item with a baked thumbnail). Show only the colour
   controls a decoration has (`DecorationScene.colourControls`), with
   palette editing and an animation switch. Give materials the same tree
   (seven groups).
5. **Launcher seat.** Where `contour.launcher` exists, pin the panel's
   app launcher there.
6. **Imported packages.** Extend the `ThemePackage` contour checks to the
   whole vocabulary.
7. **Tests, pictures, close.** Engine and scene unit tests, a lighting
   test, and pictures on all four edges plus ring and arc. Then the full
   serial suite, `docs/repairs/OWNER-LIST-PIECE-8.md`, a commit and push,
   and a fresh-clone recheck with one job, ADREP-style.
8. Then 3D platform decorations, then **part two: holographic icons**.
   After that: Phase 6 (Studio redesign), the settings pass, KDE widgets
   (ask the owner first), the cleanup pass, and one package at the very
   end.

## How this commit was verified

- The incremental build (four jobs) passed. The changed C++ files compile
  into every target that uses them.
- 14 tests that touch packages, materials and the rendering module ran one
  at a time and all passed: theme-package, source-asset-catalog,
  theme-asset-processor, both renderer-capability tests,
  layout-engine-visual, panel-materials, renderer-parity,
  rendering-module-host-neutral, theme-v2-fixture-scaffold,
  rendering-module-import, rendering-import-smoke, studio-themes and
  preset-catalog.
- The full serial suite and a fresh-clone recheck were not run, because
  this is a checkpoint at the owner's word. Run both when the piece
  closes.
