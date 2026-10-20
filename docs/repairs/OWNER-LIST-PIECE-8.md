# Owner list, piece 8: panel decorations (Phase 5, part one)

Owner's words (2026-10-18): decorations that remake a panel's contours,
its inner and outer edges, "with theme I could make an ordinary panel into
Alienware panel, or start trek theme panel, or Harry Potter theme panel",
with no marks or logos; the 145 designs and the lit look were ticked in the
first session (the checkpoint, f21b18a, and its handoff,
`OWNER-LIST-PIECE-8-HANDOFF.md`). This part brings them into the app: the
lighting, the renderer, the settings, Panel Studio, the launcher seat and
the package checks, and, at the owner's word on the day, fifteen pieces of
jewellery. Run ADREP-style: implement, focused tests, the complete serial
suite, this report, one commit on main and push, a fresh-clone recheck
built with one job, Report 2.

Methods reused, all by their published formulas, no code copied:

- **Lighting** (`src/decorations/DecorationLighting.cpp`, a port of the
  design pipeline's `lit.py`): the exact Euclidean distance transform by
  lower envelopes of parabolas (Felzenszwalb and Huttenlocher, "Distance
  Transforms of Sampled Functions", 2012) for every shape's height
  profile; separable Gaussian blur, three box passes for wide radii
  (Kovesi, "Fast Almost-Gaussian Filtering", 2010); the sliding-window
  maximum with a monotonic queue for cast shadows; Blinn-Phong highlights
  normalised by (n + 8) / 8 pi with Schlick's Fresnel approximation
  (1994); bloom blurred from one downsampled copy and scaled back up once,
  as post-processing bloom is usually made (Jimenez, "Next Generation Post
  Processing in Call of Duty: Advanced Warfare", 2014).
- **Frames** (`ContourFrames.js`, from the first session): Arwes' frames
  (MIT, Romel Perez), the LCARS proportions of the R package lcars (MIT),
  the equilateral pointed arch of gothic tracery, midpoint displacement for
  lightning (Fournier, Fussell and Carpenter, 1982).
- **Jewellery**: the running (Vitruvian) scroll of classical ornament for
  filigree; the round brilliant's facet plan (a table, eight crown facets,
  eight star facets; Tolkowsky, "Diamond Design", 1919) and the step cut's
  frames for the stones; milgrain, guilloché under translucent enamel,
  channel, halo and bezel settings as jewellers make them.
- **Materials**: ambientCG's CC0 sets and the Webb telescope's pictures
  (NASA, ESA, CSA, STScI), credited in `materials/CREDITS.md`.

## Report 1 - implementation

### What changed for the owner

- **A panel can wear a decoration** (Panels > Appearance > Panel
  decorations). It remakes the flat panel's outline on every shape (a row,
  a column, a ring, an arc, a polygon, a star) and is lit as the design
  pictures were: real height, studio reflections on metal and lacquer,
  shadows, glass lenses, lights with their halo. The icons stand on it bare,
  with no tiles (the owner's rule of the day); a tile the owner chooses for
  the icons still shows.
- **Studio shows the decorations as a tree**: six groups, their
  subgroups, and each decoration as a picture card of itself, lit, like a
  preset; the one the panel wears is marked and its branch open. Below it,
  only the settings that decoration has: its lights' colour and finish, its
  body's colour and finish, its palette (each colour clicked to change it),
  its trims' metal, a picture's hue, strength and brightness, and its
  motion. The flat look's own settings (Panelpaper, its colour, glow and
  motion) are hidden while a decoration is worn, since they draw nothing.
- **Materials are colours too** (the owner: "the rest add to colors, but
  as they are right now, not just mear colors"): under Main colour, "Or
  made of" offers every material as a lit swatch in its group (gems,
  stones, metals, organic, synthetics, space), and the body is made of it.
  The precious metals left that list: each is a decoration of its own.
- **Jewellery** (the owner: "give them some decorations, like juverlier
  makes juvelry for ritch people", "Make it as if professional designer did
  it"): fifteen pieces, one for each precious metal, in a Jewellery
  subgroup under Precious Metals - Yellow Gold Filigree, Polished Gold
  Tennis Bracelet, Bright Gold Guilloché, White Gold Diamond Tiara (one arc
  over each icon, centred on it, low), Gold Leaf Byzantine Chain, Rose Gold
  Torsade, Red Gold Garnet Cabochons, Green Gold Art Nouveau, Blue Gold
  Sapphire Channel, Purple Gold Amethyst Halo, Black Gold Onyx Deco, Grey
  Gold Engine-Turned, Silver Bright-Cut, Platinum Deco Baguettes and Copper
  Torc. Stones are cut (brilliant, princess, baguette, cabochon) and
  sparkle; gold is lit as jewellery is photographed, warm and bright, with
  no white glaze over it (the owner: "No metallic shine, gold should
  shine").
- **Decorations move as they were designed to, by default**: lights pulse,
  flicker, twinkle, blink or crackle; a nebula's outline swells and settles
  in waves along the band while its picture drifts; light flows along
  conduits and waves; vines, branches, banners and a broom's tail sway;
  candles float; footprints walk the wizard's map; gauges spin; the
  Alienware chassis cycles its colours. Animation (on by default) and
  Animation speed are in Studio.
- **On an edge panel** the decoration faces into the screen on every edge,
  fits the panel's thickness, and fades out where the panel's window ends;
  the panel grows along its edge by the room the decoration's ends need
  (the owner ticked both edge sheets: "Good"). On a free panel it reaches
  out as far as it is drawn.
- **The launcher seat** (the Raptor Hull's head): an icon sits in it from
  its right-click menu ("Sit in the decoration's seat", "Leave the
  decoration's seat") or by dragging it onto the seat; an application or a
  file dragged there from elsewhere joins the panel and sits there. The row
  closes up behind it.
- **Imported decoration packages** may carry a contour in the whole
  vocabulary: parts, strips, body, sway, the launcher seat, every line kind,
  stones and their cuts, materials, animations; every member is checked and
  nothing unknown is let through.

Pictures (in `/mnt/F/archdock-audit-2026-10-08/evidence/piece8-decorations/`):
`before-after-red-core-edges.png` (the plain panel and the Red Core on the
bottom, top, left and right edges, a ring and an arc),
`free-row-six-decorations.png`, `bottom-edge-six-decorations.png`,
`bottom-edge-four-more.png`, `jewellery-fifteen.png`, `launcher-seat.png`,
and Studio's section in `decorations-starship-red-core.png`,
`decorations-carina-nebula.png`, `decorations-crimson-gold-banner.png` and
`decorations-materials.png`.

### How it works

- **ArchDock.Decorations**, a C++ QML plugin built like ArchDock.Input so
  plasmashell loads it: `DecorationLighting` lights a scene into a base
  picture and one picture per group of lights (lights, stars and starsB,
  blinkA and blinkB, arcs), each cropped to where its light reaches;
  `DecorationBaker` works a decoration out and lights it on one worker
  thread shared by every panel (`DecorationWorker`, with its own JavaScript
  engine running `DecorationJob.js`, so the panel never waits), cancels
  work a newer setting has replaced, and keeps a moving decoration's frames;
  `DecorationLayer` shows one picture of one frame, uploaded once.
- **`renderers/PanelDecoration2D.qml`** draws the panel's path with the
  decoration, the icons' resting places as its slots; lights animate as
  their layers' opacity, motion as a loop of 12 lit frames (24 for spinning
  gauges) blended into one another. `PanelSurfaceLoader` puts it in the flat
  surface's place; `PanelScene` grows its effect margins by its reach.
- **Settings**: `panelDecoration`, `panelDecorationAnimation`,
  `panelDecorationSpeed`, `panelDecorationLight`, `panelDecorationLightFinish`,
  `panelDecorationMain`, `panelDecorationFinish`, `panelDecorationPalette`,
  `panelDecorationMetal`, `panelDecorationMaterial`, `panelDecorationHue`,
  `panelDecorationStrength`, `panelDecorationBrightness` and
  `panelDecorationSeat`; editor capabilities `decoration` and
  `decoration-colours`.
- **Engine additions** (`ContourFrames.js`, `DecorationScene.js`): a motion
  phase for every moving kind, filigree scrolls, engraved lines, enamel
  strips, round wires, pearls, cut stones with halos, a light-tent gleam
  for polished metal, a chosen body material. Every one of the 145
  decorations the owner approved builds exactly the scene it built at the
  checkpoint, on a bar and on a ring.

### Verification

Evidence logs in `build-codex-adrep/evidence/OWNER-LIST-PIECE-8/`.

- **Lighting unit tests** (`decoration-lighting-test`): every case passes
  (an empty scene says so, a body covers its shape and casts a shadow, a
  lens gives its own light and halo, each group of lights has its own
  picture, chased lights give less light, a bake no longer wanted stops,
  a picture follows a ring without holes, drift moves the picture along
  the band, a recolour reaches the chosen hue, a large scene is made
  smaller); the bake on request, for comparing a scene with the design
  pictures by eye, is skipped when no scene is asked for.
- **Scene unit tests** (`decoration-scene-test`, in QML's own engine):
  all 160 decorations build on a bar and on a ring; a moving one's loop
  meets itself and moves between; each offers the colour settings it has
  (an openwork piece of jewellery offers no body colour); icons' places land
  where the icons are; stones are cut into facets that twinkle, a
  diamond's in greys; chosen colours and a chosen material reach the
  scene.
- **Renderer unit tests** (`panel-decoration-test`): a still decoration
  is lit with its lights; a moving one plays its loop; on an edge panel
  it fits the panel's thickness and faces into the screen; the panel's
  scene draws the chosen decoration in the flat surface's place; icons
  stand on it bare; an icon sits in the launcher seat; an unknown
  decoration draws nothing.
- **The approved designs** (`12-approved-scenes-unchanged.log`): every
  one of the 145 decorations the owner approved builds exactly the scene
  it built at the checkpoint, on a bar and on a ring.
- **Settings and Studio** (`panel-window-capability-test`): a
  decoration's settings follow the decoration (worn, the flat look's
  settings hide and only the decoration's own show; its colours and
  palette are kept as chosen; a metal it does not offer is never kept;
  with its motion off it has no speed to set; taken off, the flat look
  is back with its settings); the seat takes an icon (dropped on it, an
  item the panel did not have is added and seated, freed it goes back to
  the row, seated again it is not added twice); Studio's tree opens at
  the decoration worn, its picture cards are lit off the window's
  thread, and the materials' tree opens too. `studio-truth-matrix-smoke`
  holds the decoration row with its effect.
- **Package checks** (`theme-package-test`): every one of the 160
  built-in decorations, carried as a package would carry it, passes the
  checks; a mistake in a contour (an unknown member, cut, line kind,
  strip side or animation, a gleam out of range, a material that is not
  a name, too many parts) is refused at its place.
- **Complete serial suite**: the first run passed 122 of 125
  (`10-complete-suite-first.log`, 2183 s). The three icon preset groups
  (`preset-audition-matrix-icons`, `-icons-middle`, `-icons-last`)
  stopped silently at one check, also when run alone
  (`11-icons-alone-before-fix.log`; the "Wayland connection broke" lines
  there are only the test session closing after the stop): the check
  counted the settings a saved icon preset holds as 24, but since piece
  7 part two (`f55e7d8`, the 3D tile form) it holds 25. Piece 7's last
  complete suite ran before that commit and its rechecks ran focused
  tests without these groups, so the stale count went unseen until now.
  The test now counts 25, names the tile form, and lets an icon preset
  change it in its icon-only comparisons; the three groups pass
  (`13-icons-after-fix.log`). Then the complete serial suite again, on
  the final code (`14-build-final.log`, `15-complete-suite.log`,
  2688 s): **125 of 125 passed**.
- **Warnings**: the scene builder's two "used before its declaration"
  notes, printed in every session, are gone (each wrapper now follows
  the function it calls; the 145 scenes are unchanged). Two older notes
  stay as they were, both in the suite logs of pieces 3 to 7: Studio's
  binding loop on the edited panel's name, and the test session's
  settings file's escape-sequence note.

### The owner's words on the pictures (2026-10-20)

- The decorations on a free row and on a bottom edge panel, fitted to its
  thickness and grown along for their ends: ticked, "Good".
- Stand-in squares under the icons in a first picture: "Don't put there
  tiles by default", "Same here": the app now leaves icons on a decoration
  bare (and later pictures use real icons).
- Studio's tree with a picture card for each decoration: "Good, this is
  what I meant"; every item shows how it looks, "like a presset".
- The material swatches: ticked, then "for now they're just colors, so to
  make them into themes, give them some decorations, like juverlier makes
  juvelry for ritch people" (the fifteen pieces of jewellery) and "the rest
  add to colors, but as they are right now, not just mear colors" (the
  swatches under Main colour).
- "Don't forget that free panels belongs to free panels not side panels":
  the looks made for free panels stay offered only on free panels.
- The first four pieces of jewellery: Filigree and Tennis Bracelet ticked,
  Guilloché and the Tiara crossed, "Make it as if professional designer did
  it"; redesigned, all four ticked, "Much better, but those cars should be
  only as many as there are icons, and they should be centered to icons
  too", "Yes I meant arcs", "Make them, I mean arcs not so High": one low arc
  over each icon, centred on it, no arc at the band's ends. The guilloché
  close-up ticked, "Polish it", and "No metallic shine, gold should shine":
  gold lit warm and bright, no white glaze.
- The launcher seat: "Right click or drag and drop"; the seat in use on a
  free and an edge panel, and the approved seat design: ticked, "Good".
- The Red Core on the bottom, top, left and right edges, a ring and an arc:
  ticked, "Good".
- The material swatches under the colours in Studio: ticked, "Good".
- All fifteen pieces of jewellery (the final sheet, and each sheet on the
  way): ticked, "Good".

### Known limitations

Also in `docs/KNOWN_LIMITATIONS.md`, with what a decoration costs (lit
once per change on the shared worker thread; its lights and motion
redraw the panel only while it is shown; not yet measured).

- Decorations are drawn on flat panels. A panel drawn in 3D or with a
  theme's skin or baked platform keeps its own look; 3D platform
  decorations are next.
- While icons travel along a free panel's path (continuous motion), the
  decoration stays where the icons rest: re-lighting it on every step would
  load the processor without end.
- On a thin edge panel the Raptor Hull shrinks to fit its tall head, and
  its seat with it; a thicker panel gives the seat room.
- An imported package's contour is checked but not yet drawn by the
  renderer; the built-in decorations are.

## Report 2 - recheck

(Filled in after the fresh-clone recheck.)
