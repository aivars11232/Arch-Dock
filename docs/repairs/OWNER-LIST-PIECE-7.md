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

### Questions for the owner

- The default is the rounded relief at depth 0.3 and turn 15°, which is
  subtle on icons of 48 px; depth 0.6 shows the volume clearly. Should
  the default be deeper?
- In the Block relief the walls take the colours of the picture's edge.
  Should they instead be one colour of the owner's choosing, or darker
  than the picture?
- The icon's picture keeps its own brightness on the volume (a little
  self-lit), so its colours stay readable; a more sculpted look (the
  light alone, with stronger highlights) is a slider away if wanted.

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
