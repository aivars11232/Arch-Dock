# Owner list, piece 7: real 3D icons (Phase 4)

Owner's words (2026-10-13): "And don't forget about real 3D icons, where it
renders my current icon in 3D not changes the icon but renders it"; the
earlier complaint that the icons on 3D panels "are not real 3D as i asked"
(flat pictures on quads), with a 3D-icon website picture as the idea of it.
Phase 4 of the owner's six-phase plan (real 3D icons, then 3D tiles as a
bubble round each icon). Run ADREP-style: implement, focused tests, the
complete serial suite, this report, one commit on main and push, a
fresh-clone recheck built with one job, Report 2.

The reference (the owner, 2026-10-19, "This is what I was talking about"):
the Sketchfab "Firefox logo 3D" model by EllyMarche, seen from five sides;
every part of it (each flame, the fox, the globe) is a rounded volume of
its own, lit, and the whole turns. Screenshots kept at
`/mnt/F/archdock-audit-2026-10-08/evidence/references/sketchfab-firefox-logo-3d/`.

Methods reused, all textbook methods by their formulas, no code copied:
the picture's parts of one colour by region growing (a pixel joins its
neighbour's part when its colour is close enough to the part's first
colour or to the neighbour's own); the distance of every pixel to the
edge of its part by the two-pass chamfer distance transform with the 3-4
weights (Borgefors, 1986); the rounding by a half-circle profile over
that distance (a cross-section of a cylinder), so an edge meets the ground
steeply and the middle lies flat; the layers by breadth-first walking of
the parts' neighbourhoods from the rim inward; for the Block relief, the
outline of the opaque pixels traced along the pixel edges and linked into
loops, simplified with the Douglas-Peucker method, triangulated by ear
clipping, and extruded as caps and walls.

## Report 1 - implementation

### What changed for the owner

- **Each icon on a 3D panel is now a volume modelled from its own
  picture, in the kind the picture asks for** (the owner, 2026-10-19:
  "some icons need to be like LCL, meaning give it just depth/thickness
  and other one more like Arch Dock/Firefox, more like spherical so that
  needs to be distinguished and app should know how to do it"). The app
  reads the outline: an outline that fills its box like a rounded square
  (a sign, a badge, a tile such as LCL) is a plate and becomes a block,
  just thickness, the picture on its front and back and the walls in its
  edge colours. Any other outline (a round picture of a thing such as
  Firefox, a figure such as the Arch Dock logo, a star) has its parts of
  one colour found, and each part rises as a rounded volume of its own
  from its own edge, a full half round at half the depth, with a crease
  to the next part; a part inside another stands a step above it, the
  whole rises a little like a cushion, and the light rounds every part;
  the back is the same, mirrored. The picture itself stays the colour of
  it all: the icon stays the user's own icon, nothing is replaced or
  restyled.
- **Icon depth** (Panels > 3D) sets how deep the volume is, as a share of
  the icon's size; 0 keeps the flat picture as before (a block is half as
  thick as the rounded parts at the same depth, after "1 don't need to be
  that thick"). **Icon turn** sets how far the icon turns about its
  upright axis, so its depth shows; the icon still faces the user, turned
  that much. **Icon relief** is Auto (each icon its own kind), or Rounded
  or Block for every icon of the panel.
- The icons' motions (spin, swing, hover growth) move the volume as they
  moved the picture.
- On the desktop the dock's icons come through the Plasma widget's own
  entry item, which passed the 3D scene's mesh flag down but not the new
  depth, so the first pictures showed flat icons at every setting; the
  widget and the Studio's live preview now pass the depth and the relief
  the same way.

### Verification

Evidence in `build-codex-adrep/evidence/OWNER-LIST-PIECE-7/` (logs 10-37;
sheets in `sheets/`; desktop pictures in
`/mnt/F/archdock-audit-2026-10-08/evidence/piece7-solid-4/`).

- **Geometry unit tests** (`icon-solid-test`, `tests/tst_IconSolid.qml`):
  16/16. The block: a square's outline has sixteen pixel-edge corners and
  simplifies to four; its solid has two caps and four walls, fifty units
  thick, unit normals, texture coordinates in the picture; a disc keeps
  many corners; two parts give two blocks; a hole is filled; the walls
  face outward and read the picture inside the edge. The relief: a disc
  is a cushion (one part on the ground, rising past twenty units, its top
  near the middle, the back mirroring the front, front normals forward
  and back normals backward, two triangles per pixel per side); every
  triangle winds with its normal; a part inside another stands a step
  up; a shaded part stays whole; a single pixel of another colour is no
  part; the chamfer distance is one at an edge and two in the middle of
  a four-pixel square. The kind: a rounded square is a plate (block), a
  disc and a star are rounded, and the one entry point tells the kind it
  chose.
- **Renderer smoke** (`36-focused-tests-auto.log`, rendering-import-smoke,
  180 s): passed; the pixel test's icon is the test data's folder, the
  scene starts flat, and the solid block asks for the rounded relief by
  name (the folder is a plate), checks it draws differently from the
  flat pictures, then the Block relief differently from the rounded one,
  on both themes (`scene3/icons-flat.png`, `icons-rounded.png`,
  `icons-block.png`, and `orange/`; sheet
  `sheets/18-test-scene-relief-final.png`). The way here is in logs
  10-29: the
  first solids hid the flat picture and broke the hit tests, the entry
  travel and the water mirrors (now the picture stays, drawn clear); a
  grabbed picture loads into the canvas at once, so no loaded signal
  follows and the paint must be asked for right after (the orange theme
  never cut a solid before that); the glyph check had been looking for
  the plate fixture's blue; the relief word was dropped by the loader's
  number filter until it joined the word list.
- **Focused tests** (`36-focused-tests-auto.log`): 5/5:
  panel-settings-schema-test, panel-window-capability-test (416 s; its
  3D transaction carries Icon depth 0.45, Icon turn 20 and Icon relief
  block, and the parameters come back so), icon-solid-test,
  rendering-import-smoke, studio-truth-matrix-smoke (377 s; the matrix
  holds Icon depth, Icon turn and Icon relief on the 3D page with frame
  probes). The same tests passed on the build before the automatic kind
  (`30-focused-tests.log`, 4/4).
- **Desktop pictures** (private replica desktop with real icons, the ring
  free-2; `35-replica-solid-4.log`; sheets `17-desktop-auto-ring.png` and
  `16-desktop-auto-close.png`): before = flat pictures; each icon in its
  own kind at the defaults (depth 0.3, turn 15: the folders and LCL as
  blocks, the Arch Dock logo, the round camera icon and the stars
  rounded), at depth 0.5 and from a low view; then every icon rounded at
  0.5 and every icon a block at 0.3. Earlier runs: sheets 09-10 (flat at
  every setting: the Plasma widget's entry item did not pass the depth;
  found with the probe, `21-`, `22-replica-solid-probe-*.log`, and
  fixed), 11-12 and 14-15 (the first relief, before the owner's rulings
  on thickness and on the two kinds).
- **Studio's 3D page** (`32-studio-pictures.log`, private KWin,
  `studio-kwin/panels-3d-<look>.png`): the Icon depth, Icon turn and
  Icon relief rows on Panels > 3D.
- **Complete serial suite** (`37-complete-suite.log`, 2583 s): 121/122
  passed. The one failure, wayland-hardening-scale100-test, died 9 s in
  at the private Plasma session's start (the compositor went away while
  the bottom panel was being placed: "containment-missing", "Message
  recipient disconnected from message bus"), a session flake of the kind
  seen in piece 6; rerun alone it passed (`38-wayland-scale100-rerun.log`).

### Questions for the owner, answered

The owner (2026-10-19): "Default depth should be 0.3, rather pictures
edge, and yes, more sculpted lightning slider". So the default depth
stays 0.3, the Block relief's walls keep the picture's edge colours, and
the addition after Report 2 adds the slider.

### The owner's words on the pictures (2026-10-19)

On the test-scene sheet, whose icons are the test's stand-in picture (the
icon style's base plate, since the private test session has no icon
theme): "Those are icon tiles not icons", and "3D icon tiles should be
more like glass, transparent, and yet the outlines are visible enough to
tell a difference". So: the test now draws its icons from the test data's
folder picture, an icon's own kind of outline; the owner's pictures of
this piece come from the replica desktop with real icons; and the second
part of Phase 4, the 3D tiles round each icon, is to be glass-like,
see-through with visible outlines, while the icon itself stays solid.

### Known limitations

In `docs/KNOWN_LIMITATIONS.md`: a hole in a picture is filled in its
block; only the picture becomes a solid, badges and overlays stay flat.

## Report 2 - recheck

A fresh clone of origin/main at `5cd7e5c` (logs 40-44), configured with
Quick 3D and a short private temp folder, built with one job in 1491 s,
ran 27 focused tests serially (the capability, registry, schema,
transaction, model, theme-asset, icon tiles, icon scene, icon solid,
materials, platform geometry, renderer smoke, truth matrix, settings
editor, Studio preview contract, live preview, runtime UI interaction,
profile store, configuration upgrade, Studio navigation, session startup,
profile matrix, renderer software, preset library, layout engine visual,
panel scene and Studio themes tests): 27/27 passed in 1246 s. Every one
of the 691 committed files is identical in the clone and the workspace.

## Addition: Icon lighting

- **Icon lighting** (Panels > 3D, 0 to 1, default 0.5 = the look of the
  pictures above) sets how much the light models an icon's volume: at 0
  the picture keeps its own brightness and the volume shows by its shape
  alone; at 1 the light alone shapes it, with stronger highlights and
  deeper shade, like a modelled object under a lamp.

## Addition 2: the shapes in the picture

The owner (2026-10-19 evening), with eight screenshots of neural4d.com
turning the Arch Dock logo into a full model: "So for real 3D Icons u can
reverse engeneer this site ... i also gave u some samples on how Arch
Dock should turn 2D Icons into 3D"; then: "But it have to be light and
fast I mean 2-3 seconds fast if not faster." That site runs a trained
image-to-3D model on its servers; on this machine (an AMD Radeon 610M,
no CUDA, 14 GB) the one open model that runs on the processor takes
minutes per icon, so no model. Instead the app reads the shapes a logo
is drawn from, in about a tenth of a second per icon:

- **A ring.** A thin band lying on an ellipse, at least half the way
  round, is a ring seen at a tilt (the tilt from the ellipse's axes). It
  becomes a real ring through the icon, passing in front where the
  picture shows it over the figure and behind where the figure hides it,
  wearing the picture's colours. The figure the ring crosses is whole
  again under it. Read from the whole outline's thin strokes, so a ring
  drawn in the figure's own colours (the Arch Dock logo's) is found too;
  a rounded-square frame (LCL's) is not taken for one.
- **A globe.** A round outline (Firefox's) rises as a ball the parts ride
  on.
- **A prism.** A part with three to six corners (the Arch Dock A, a
  letter) rises with a flat top and bevelled edges.
- **Rounded parts** as before for everything else.

### Verification of the addition 2

Evidence logs 60-68 in `build-codex-adrep/evidence/OWNER-LIST-PIECE-7/`.

- **Geometry unit tests** (`icon-solid-test`, 20/20): the earlier sixteen,
  and: a drawn logo (a triangle with a ring round it, the ring's lower
  half drawn over the triangle) is read into one ring and one whole
  triangle, a polygon, with the lower half of the ring in front; its
  mesh is a prism with a flat top and a ring reaching in front of and
  behind the body, the depth the body's top and the reach the ring's; a
  round two-colour icon is a globe; a lying ellipse is a disc.
- **Real icons** (scratch runs on this machine, logged in the session):
  the Arch Dock logo's picture gives one ring (semi-axes 27 and 8 px at
  64 px, a band of 3 px) and its A as a polygon; LCL gives no ring (its
  rounded-square frame is rejected by the hull test), Firefox none and a
  globe, the Breeze folder a block; about 80 ms per icon.
- **Renderer smoke** (`61-shapes-smoke.log`, 183 s): passed with the
  logo as the test's icon (`scene5/icons-flat.png`, `icons-rounded.png`:
  the prism with its ring, `icons-block.png`, `icons-sculpted.png`;
  sheets `22-test-scene-shapes.png`, `26-test-scene-logo-close.png`).
- **Focused tests** (`68-shapes-focused-tests.log`): 3/3 passed:
  panel-settings-schema-test, panel-window-capability-test (411 s) and
  studio-truth-matrix-smoke (373 s).
- **Desktop pictures** (`62-shapes-replica.log`, replica `piece7-solid-6`;
  sheets `23-desktop-shapes-close.png`, `24-desktop-shapes-ring.png`,
  `25-desktop-logo-close.png`): the Arch Dock logo on the ring as a
  bevelled prism with a real ring through it and a disc at its base,
  from its flat picture; the other icons as before.

### Verification of the addition

Evidence logs 50-53 in `build-codex-adrep/evidence/OWNER-LIST-PIECE-7/`.

- **Focused tests** (`51-lighting-focused-tests.log`): panel-settings-schema-test,
  icon-solid-test, rendering-import-smoke (184 s; the solid block now
  also checks that Icon lighting at its full draws the rounded relief
  differently from the half-lit default: `scene4/icons-sculpted.png`,
  sheet `sheets/21-test-scene-lighting.png`) and studio-truth-matrix-smoke
  (411 s; the matrix holds Icon lighting with a frame probe at 1.0 and
  0.0) passed. panel-window-capability-test crashed 52 s in, inside the
  QML engine's garbage collector while a string property was being
  written during the Studio page pictures (`coredumpctl` 19:15:39, the
  trace in Qt's QV4 memory manager); rerun alone it passed
  (`53-capability-rerun.log`, 7 min), and it had passed twice before on
  the same code paths, so it is taken for a one-off of the engine, to be
  watched; its 3D transaction carries Icon lighting 0.8 and the parameter
  comes back so.
- **Desktop pictures** (`52-lighting-replica.log`, replica
  `piece7-solid-5`; sheets `19-desktop-lighting-close.png` and
  `20-desktop-lighting-ring.png`): the ring's icons in their own kind at
  depth 0.3 with Icon lighting 0.5 (the look of the earlier pictures),
  1.0 (the light alone: deeper shade, stronger highlights) and 0 (the
  picture's own brightness, the volume by its shape alone).

## Report 2 of the addition - recheck

A fresh clone of origin/main at `c5ebc6a` (logs 45-49), built with one
job in 1538 s, ran the same 27 focused tests serially: 27/27 passed.
Every one of the 691 committed files is identical in the clone and the
workspace.

## Report 2 of the addition 2 - recheck

A fresh clone of origin/main at `dafd9fe` (logs 63-67), built with one
job in 1454 s, ran the same 27 focused tests serially: 27/27 passed.
Every one of the 692 committed files is identical in the clone and the
workspace.
