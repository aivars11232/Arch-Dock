# ADREP-TASK-003: Folder layouts as the owner defines them, with sensible scrolling and working easing

Repository: `/mnt/F/Arch Dock/` (origin `https://github.com/aivars11232/Arch-Dock`, branch `main`)
Package: `/mnt/F/Arch Dock LCL repairs/` (ADREP 1.0.0)
Base commit: `62eaeba`

Evidence: `build-codex-adrep/evidence/ADREP-TASK-003/` (index in `INDEX.txt`).

## Report 1 - implementation

### What changed for the owner

- On a free panel a folder opens in the shape you described, outside the dock
  and along the way the folder faces out of it:
  - **Fan** is a slice-shaped small panel drawn in your panel's look: two
    straight edges from the folder joined by an arc, the children standing on
    the arc. **Fan opening** (40 to 160 degrees, 90 by default) sets how wide
    it opens.
  - **Stack** is a straight line of children from the folder outward. **Stack
    length** (2 to 12, 5 by default) sets how many show at once.
  - **Arc** stands the children on an arc centred on the folder, all at one
    distance from it, symmetric about the way the folder faces.
  - **Ring** is a second circle beside the folder with the children on it, in
    your panel's look. **Ring size** is **Small** (just big enough for the
    children) or **Same as panel** (the dock's own radius).
  - **Along the dock** keeps its curve beside the dock; **Grid** stays a popup.
- Scrolling moves the children along their shape: one wheel notch moves them
  one place, however finely the wheel reports it, a child that leaves one end
  comes back at the other, and on a ring they go round. Along the dock moved
  one child for every wheel event, however small, so a fine-grained wheel or a
  touchpad moved several children per notch; Grid now moves one row per notch
  (it moved 60 pixels, less than a row). On your free circle
  **Scroll sensitivity** (Animations) scales folder scrolling as it scales the
  dock's own icons.
- **Folder easing** gives four different motions, opening and closing:
  outCubic glides out, outBack goes a little past its place and comes back,
  outElastic snaps out and wobbles, and spring starts softly and swings past
  its place once (before, it was the same curve as outElastic). A folder now
  folds back into its icon when it closes. **Folder animation duration**
  applies to every layout; reduced motion opens and closes folders at once.
- Fan opening, Stack length and Ring size stand on Behavior right under Folder
  layout, each only while its layout is chosen, and only on free panels; Ring
  size only where the dock has a radius.
- Where the screen leaves no room in the outward direction, a fan, an arc, a
  stack or a small ring first shows fewer children at once, and only then
  turns towards the side with room. No child of a fan, an arc, a stack, a ring
  or Along the dock stands on an icon of the dock.
- Edge panels' folders open in their popups as before, with one child or row
  per notch and the four easings.

### Owner findings

| Finding | Status | How it was fixed | Proof |
|---|---|---|---|
| OF-16 (folder part) | Closed | Folder scrolling takes the panel's Scroll sensitivity from ADREP-TASK-002 where Panel Studio offers it (free panels with a curved layout); elsewhere one child or row per notch. | `tst_FolderExpansion::test_folderWheelGathersNotches` (2x: two children a notch; 0.5x: one every two notches); `test_gridScrollsOneRowPerNotch` (2x: two rows); session wheel table in `after-summary-table.txt` (1x, 2x, 0.5x on five shapes and Grid) |
| OF-17 | Closed | CF-07: Along the dock moved one child per wheel event, so high-resolution wheels and touchpads raced. Wheel input now gathers into whole steps: 120 angle units, or one pitch of touchpad pixels, per child, times Scroll sensitivity, eased over 100 ms; the children wrap round (PD-10). | `test_folderWheelGathersNotches` (seven eighths of a notch move nothing, the eighth moves one child); `test_trackMovesAnOvercrowdedFolderAlongTheCurve` (wrap both ways, keys); session: one notch 1 child, eight eighths 1 child, 20 px of smooth scrolling (240 angle units) 2 children |
| OF-18 | Closed | CF-09: Fan was the compact half-circle popup. It is now a sector (`LayoutEngine.folderShape("fan")`): apex just outside the folder, edges Fan opening apart about the folder's outward direction, drawn in the panel's look, the children on its arc, wheel along it with wrap-around. | `test_folderShapes(fan/*)` (30 rows: apex, radius, edges at +-45 degrees, outline); `test_fanDrawsItsPanelAndKeepsPressesOnIt`; folder-anchor-smoke fan (15 openings, 3 looks); folder-interaction-smoke 48-item fan (the last child comes round to the first place); `before-sheet-fan.png`, `after-sheet-fan.png` |
| OF-19 | Closed | CF-07: one notch scrolled 20 x wheelScrollLines = 60 px, less than a row of named children. The popup's wheel now scrolls one row per notch in Grid (one child along a path or a stack). | `test_gridScrollsOneRowPerNotch`; session: one notch scrolled 90.7 px for a 90.7 px row, 181.3 px at 2x, 45.3 px at 0.5x |
| OF-20 | Closed | Stack rose from the folder drifting with its lean. It is now a straight line from the folder along its outward direction, Stack length children at once, with wrap-around. | `test_folderShapes(stack/*)` (on the line, one pitch apart, length 5); `test_folderShapeSettings` (2, 5, 12); folder-anchor-smoke stack (every child within 4 px of the line); `stack-length-2.png`, `stack-length-12.png` |
| OF-21 | Closed | Arc was the same half circle as Fan, centred on the popup's edge. The children now stand on an arc centred on the folder, at one distance from it, 150 degrees about its outward direction. | `test_folderShapes(arc/*)` (one distance, ends at +-75 degrees, symmetric); folder-anchor-smoke arc (centre within 6 px of the folder, radii within 1.5 px, symmetric) |
| OF-22 | Closed | Ring was a large circle starting beside the folder, mostly scrolled out of its popup. It is now a second circle beside the folder along its outward direction, drawn in the panel's look; Small fits the children by the straight distance between neighbours, Same as panel has the dock's radius; the wheel turns the children round it. | `test_folderShapes(ring/*)`; `test_folderShapeSettings` (small, panel, a dock without a radius); `test_folderPathsWrapAround(ring/*)`; folder-anchor-smoke ring; `ring-panel-8.png`, `ring-panel-24.png`; `after-sheet-ring.png` |
| OF-23 | Closed | CF-08: spring used the same Qt OutElastic as outElastic, and a folder vanished when it closed. `MotionChannels.folderEasing()` has four curves, spring a damped spring from rest; every layout opens on a linear clock over the folder duration and closes by running that clock back. | folder-easing-test (ten layouts: no two easings within 4 px; the child stands curve(t) of the way out; duration timed opening and closing; 1200 ms cap; reduced motion); `easing-curves.png` (measured child positions, ten layouts); `easing-strip-*.png` |

### Product decisions applied

- PD-10: every free-panel folder shape wraps: an open path is a loop one slot
  longer than its places, or as long as the folder, so a child leaving one end
  fades out past it and comes back at the other, also when all children fit; a
  ring carries them round, and a long folder swaps children in beside the
  folder as one leaves there.
- PD-11: Fan is a sector drawn in the panel's look, apex at the folder, edges
  40 to 160 degrees apart (90 by default), children on its arc.
- PD-12: Stack is a straight line outward; Stack length 2 to 12 (5).
- PD-13: Arc is centred on the folder, symmetric about its outward direction.
- PD-14: Ring is a second circle beside the folder; Small or Same as panel;
  the option is kept.
- PD-15: four motions in every layout, opening and closing, over the folder
  duration; reduced motion is immediate.
- PD-16: one notch, or one pitch of touchpad travel, is one child (one Grid
  row); the panel's Scroll sensitivity scales it where Studio offers it.
- PD-23: edge panels keep their popup shapes; only the shared sensitivity and
  easing changes reach them.

### Acceptance criteria

| # | Criterion (from the task document) | Result | Proof |
|---|---|---|---|
| 1 | Along the dock: one notch moves one child, the speed feels normal, and the children wrap around. | Met | session wheel table (1 notch = 1 child; eight eighths = 1 child); `test_trackMovesAnOvercrowdedFolderAlongTheCurve` (wrap both ways); `test_folderWheelGathersNotches` |
| 2 | Grid: one notch moves one row, and the scrolling is no longer weak. | Met | `test_gridScrollsOneRowPerNotch`; session: 90.7 px per notch for a 90.7 px row (was 60 px by the code) |
| 3 | Fan: a sector-shaped small panel opens from the folder with two straight edges and an arc; the children stand on the arc and scroll along it with wrap-around. | Met | `test_folderShapes(fan/*)`, `test_fanDrawsItsPanelAndKeepsPressesOnIt`, folder-anchor-smoke (fan, 15 openings: apex at the folder, children on the arc, outline drawn), folder-interaction-smoke (48-item fan wraps), `after-sheet-fan.png` |
| 4 | Stack: a straight line outward from the folder; Stack length works; the children wrap around. | Met | `test_folderShapes(stack/*)`, `test_folderShapeSettings`, `test_folderPathsWrapAround(stack/*)`, folder-anchor-smoke (stack), `after-sheet-stack.png`, `after-sheet-settings.png` |
| 5 | Arc: centred on the parent folder, facing it, opening away from the dock. | Met | `test_folderShapes(arc/*)`, folder-anchor-smoke (arc: centre within 6 px of the folder on all three looks), `after-sheet-arc.png` |
| 6 | Ring: a second circle with the folder's children; Small and Same as panel both work; the children turn around it with the wheel. | Met | `test_folderShapes(ring/*)`, `test_folderShapeSettings`, `test_folderPathsWrapAround(ring/*)`, folder-anchor-smoke (ring), session wheel table (ring: 1 notch = 1 child), `ring-panel-8.png`, `ring-panel-24.png` |
| 7 | The four easings give four visibly different motions in every layout; the duration applies; reduced motion is respected. | Met | folder-easing-test (33 checks over ten layouts), `easing-curves.png`, `easing-strip-path-fan.png`, `easing-strip-popup-grid.png`, `easing-strip-path-ring.png` |
| 8 | Expand folders on click and Always show folder item names work in every layout. | Met | folder-interaction-smoke: on the edge panel and the free panel, each of the five layouts: with Expand folders on click off a click opens the folder itself and "Show contents..." opens the layout; every layout shows its five names; the 48-item folders switch names off and on; `after-sheet-names-off.png` (every layout without names); `test_fanDrawsItsPanelAndKeepsPressesOnIt` |
| 9 | In every layout the folder opens from the clicked folder at top, bottom, left, right and diagonal positions, away from the dock, and the children's hit targets follow what is shown. | Met | folder-anchor-smoke: 90 openings (six layouts x five positions x flat, baked and 3D) with six neighbour icons on the dock: every shape opens along the folder's outward direction from the folder, every child at least 47.9 px from every dock icon; one Grid popup on the baked ring covers a neighbour (recorded, see open points); folder-interaction-smoke clicks a child in each layout; `test_fanDrawsItsPanelAndKeepsPressesOnIt` (hover and click on the drawn places) |
| 10 | Fan opening, Stack length and Ring size appear only for their own layout. | Met | `PanelWindowCapabilityTest::studioFolderShapeRows` (each right under Folder layout only for its layout; none on an edge panel; Ring size hidden on a row of icons); `studioTruthMatrix` (21 panels) |
| 11 | The complete configured suite passes in one serial run on the final source. | Met | `40-full-suite.log`: 117 of 117 in 1548 s, one job, no evidence directory set |

### Tests

- Focused tests: `tst_FolderExpansion` (187 checks: 120 shape rows, settings,
  screen limits, wrap-around, wheel accumulation, the fan's panel, Grid rows),
  `tst_FolderEasing` (new, folder-easing-test, 33 checks), `panel-model-test`,
  `panel-settings-schema-test`, `panel-registry-test`,
  `panel-capability-resolver-test`, `panel-window-capability-test` (75 passed,
  1 skipped needing a live session; truth matrix included), and the session
  tests `folder-interaction-smoke` (80 s), `folder-anchor-smoke` (90 openings,
  103 s) passed on the final source; the evidence runs `folder-layouts/before`
  and `folder-layouts/after` (90 openings each) passed.
- Complete configured suite: 117 of 117 passed in one serial run on the
  final source, 1548 s (`40-full-suite.log`). Before it, a run on the
  same source but for one test passed 115 of 117 in 1557 s
  (`39-full-suite-115-of-117.log`): `dock-geometry-test` still expected the
  along-the-dock travel to stop at the last child and to run the other way (an
  older test of the folder track that the search for this task's tests had
  missed; updated, see the contract changes); and
  `preset-audition-matrix-defaults` timed out stopping its private service
  after that session's private Plasma shell crashed in Qt's threaded canvas
  painter while shutting down (the dock's flat surface canvas, which this task
  does not change; the folder's drawing paints on the main thread). It passed
  on its own (`42-preset-audition-matrix-defaults.log`).
- Contract changes cited to product decisions in the tests: the along-the-dock
  overcrowding tests (`tst_FolderExpansion`, `tst_DockGeometry`) check
  wrap-around and the wheel direction of the dock instead of a travel that
  stops at the last child (PD-10); a free panel's Fan, Arc, Stack and Ring are checked by their own
  geometry instead of the popup's half circle (PD-11 to PD-14); the session
  48-item check on the free panel checks the fan's arc and wrap-around
  (PD-11, PD-10); folder-anchor-smoke runs every layout on three looks, on a
  1920 x 1080 screen with the dock centred and six neighbours, where the old
  run had the folder alone on the dock (ADFIX contract extended, as the task
  asks).

### Files changed

- `qml/ArchDock/Rendering/LayoutEngine.js`: `folderTrackLayout()` with
  wrap-around on open and closed paths, `folderTravelTo()`, `folderShape()`
  (fan, arc, stack, ring; obstacles and screen limits), `unitedBounds()`.
- `qml/ArchDock/Rendering/FolderTrack.qml`: every free-panel path shape; the
  drawn small panel; notch-gathering wheel; keys; opening and closing motion.
- `qml/ArchDock/Rendering/FolderExpansion.qml`, `inputs/ScrollInput.qml`:
  the four motions and the closing for popups; one row or child per notch.
- `qml/ArchDock/Rendering/MotionChannels.js`: `folderEasing()`.
- `qml/ArchDock/Rendering/PanelScene.qml`: folder scroll sensitivity, the dock
  centre and the dock's icon positions for the shapes.
- `plasma-dock-widget/contents/ui/FolderTrackHost.qml`,
  `FolderExpansionHost.qml`, `main.qml`: free panels open Fan, Arc, Stack and
  Ring on a path; closing through the motion; the new settings.
- `qml/runtime/SettingsPopup.qml`: Scroll sensitivity says it moves folders too.
- `src/model/PanelSettingsSchema.cpp`, `PanelDefinition.{h,cpp}`,
  `src/panel/PanelWindowSettings.cpp`: Fan opening, Stack length, Ring size,
  their rules and labels.
- `CMakeLists.txt`: folder-easing-test; folder-anchor-smoke on 1920 x 1080
  (limit 900 s); folder-interaction-smoke limit 300 s.
- Tests: `tests/tst_FolderExpansion.qml`, `tests/tst_FolderEasing.qml` (new),
  `tests/tst_DockGeometry.qml`,
  `PanelSettingsSchemaTest.cpp`, `PanelWindowCapabilityTest.cpp`,
  `data/studio-truth-matrix.json`, `visibility-window.py` (folder-layout
  picture mode, extended folder checks, screen size option),
  `run-rendering-import-smoke.sh` (screen size, session limits).
- Documents: `docs/INSTALL.md`, `docs/KNOWN_LIMITATIONS.md`,
  `docs/CURRENT_STATE.md`, `CHANGELOG.md`, this report.

### Open points and limits

- Grid keeps the popup beside the folder (the task keeps Grid as it was). On
  the tilted baked ring a neighbouring icon stands on that side at one of the
  five positions and the popup covers it; the anchoring test records it in
  `folder-anchor/folder-anchor-notes.json`. On a flat panel it would fail the
  test; it does not happen there.
- Found while extending the test: on tilted baked and 3D docks an Along the
  dock child could stand 31 to 43 px from a neighbouring icon (centre to
  centre). The curve now steps further out of the dock where that would
  happen; the least distance measured is 47.9 px.
- The fan's sector and the ring are drawn in the panel's flat look (its
  appearance's colour, width and glow, or its custom colour); a baked or 3D
  look's artwork is not drawn on them.
- Along the dock, Fan, Arc, Stack and Ring scroll with the wheel, the touchpad
  and the keys; there is no drag scrolling on them. A press beside the
  children in their window closes the folder.
- A touchpad was not driven for real: KWin's input emulation delivers smooth
  scrolling as angle steps (12 per pixel), counted as notch-equivalents; the
  pixel path is proven by the unit test.
- Folder children have no drag sources, so there was nothing to make follow
  the new geometry.
- Not installed: the PC still runs 0.1.1-11. The package with this work comes
  with ADREP-TASK-005, so the owner checks below wait for it.

### Owner checks for this task (only what automation cannot see)

1. Open a folder on your free circle with each layout: Along the dock and Grid
   scroll at a normal speed, one notch at a time.
2. Fan is a slice-shaped small panel; Stack is a straight line you can lengthen
   (Stack length); Arc faces the folder; Ring is a second circle (try Small and
   Same as panel).
3. Switch Folder easing between the four choices: each opening and closing
   looks different.

