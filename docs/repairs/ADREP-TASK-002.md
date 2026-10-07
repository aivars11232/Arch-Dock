# ADREP-TASK-002: Free-panel items travel along the panel's own path while the panel stays still

Repository: `/mnt/F/Arch Dock/` (origin `https://github.com/aivars11232/Arch-Dock`, branch `main`)
Package: `/mnt/F/Arch Dock LCL repairs/` (ADREP 1.0.0)
Base commit: `803d18d`

Evidence: `build-codex-adrep/evidence/ADREP-TASK-002/` (index in `INDEX.txt`).

## Report 1 - implementation

### What changed for the owner

- Scrolling over a free panel with a curved layout moves its icons along the
  panel's own outline while the panel itself stays still: around a circle or
  an ellipse, along a polygon's edges, through a star's points, along a
  spiral. Scrolling up moves them clockwise, down moves them back. Dragging the
  panel moves them too.
- One wheel notch moves every icon one place. The move starts in the very
  next frame and has arrived within about a tenth of a second of the wheel
  event (73 to 104 ms measured); spinning the wheel never queues up a backlog.
- On a fan, arc, semicircle, radial path or spiral, an icon that leaves one end
  fades out and comes back at the other end, also when every icon fits.
- Panel Studio > Animations now says what moves: **Continuous motion** (Off,
  Clockwise, Counterclockwise), **Continuous motion moves** (Items along the
  path, Whole panel, Both), **Item travel speed**, **Panel rotation speed**,
  **Motion runs** (Always, or while the pointer is over the panel) and
  **Scroll sensitivity** (a quarter of a place to four places per notch). The
  wheel and dragging move what continuous motion moves. A speed is shown only
  for what is moving, and only while continuous motion is on.
- Panel Studio > Layout offers **Direction** (Up, Down, Left, Right) for fan,
  arc, semicircle and radial panels; Layout angle below it still fine-tunes it.
- A panel that rotated before keeps turning as a whole after the upgrade, with
  a backup first. Your free circle rotated, so it starts as Whole panel: choose
  Items along the path to make its icons travel.
- A whole flat panel that turns no longer shows its outline behind its icons:
  the icons moved in the next frame, but the outline was painted again 74 to
  143 ms after the wheel event. Now the outline turns in the same frames,
  without being painted again.
- A spiral panel's line is drawn where its icons stand; before, the icons stood
  on a different spiral from the one drawn.
- Edge panels behave as before: no wheel travel, no rotation.

### Owner findings

| Finding | Status | How it was fixed | Proof |
|---|---|---|---|
| OF-11 | Closed | The wheel added 15 degrees to the whole scene's angle, and the flat outline is a threaded canvas that was painted again 74-143 ms after the icons had moved. The wheel now moves the icons by default; a whole-panel turn paints the outline at its resting angle and turns the painting; each step eases out over 100 ms measured from the wheel event and takes the first frame's share at once. | `02-before-summary.txt`, `before-summary-table.txt`; `after-summary-table.txt` (first frame moves on all 12 layouts, rest 73-104 ms after the wheel, 0 outline repaints; a Whole panel turn also moves on the first frame with 0 outline repaints); `tst_PanelScene::test_surfaceKeepsStillWhileEntriesTravelAndTurnsWhole` |
| OF-12 | Closed | `LayoutEngine.trackPlacement()` gives every curved track, the star, the spiral and theme tracks a travel phase in entry slots; the progress is ((index + travel) mod count) / count along the real outline. | `path-travel-smoke` (12 layouts; in every recorded frame between two places, triangle, square, hexagon and star icons stay within 0.06 px of the drawn outline); `tst_DockGeometry::test_closedTracksCarryEntriesRound`; `tst_LayoutEngineVisual::test_travelKeepsEntriesOnTheDrawnPath`; `after-sheet-closed.png` |
| OF-13 | Closed | An open path is a loop one place longer than its icons (or than the places on it, when it is crowded): leaving icons fade out just past one end, invisible and not clickable, and come back at the other. | `path-travel-smoke` open-path loops (spiral, fan, arc, semicircle, radial: every icon left and came back); `tst_SceneRotation::test_openPathsWrapAround`; `tst_DockGeometry::test_overcrowdedOpenCurvesWrapAround`; `after-sheet-open.png` |
| OF-14 | Closed | Direction Up, Down, Left and Right on Layout set the layout angle at which the middle of the path faces that side. | `PanelWindowCapabilityTest::studioMotionAndDirectionRows`; `tst_SceneRotation::test_openShapesFaceTheChosenSide` (16 rows); `path-travel-smoke` fan Direction (icons and hit areas 92 px towards each side); `after-sheet-modes.png` |
| OF-15 | Closed | Continuous motion moves: Items along the path, Whole panel or Both, with Item travel speed and Panel rotation speed and the existing trigger; the wheel and dragging follow the same choice. | `path-travel-smoke` (one notch and one second of continuous motion per choice); `tst_SceneRotation::test_continuousMotionMovesItemsPanelOrBoth`; `wholePanelRotationFieldsAreGatedByTheResolver` |
| OF-16 | Partly closed | Panel travel: wheel input gathers into whole steps (120 angle units a notch, or one place's distance of touchpad pixels), scaled by the saved Scroll sensitivity. Folder scrolling (Along the dock, Grid) takes this setting in ADREP-TASK-003, as that task specifies. | `tst_SceneRotation::test_wheelInputGathersIntoWholeSteps`; `path-travel-smoke` input checks (spin, high-resolution wheel, smooth deltas) |

### Product decisions applied

- PD-08: the motion settings are on Animations only; Direction is geometry and
  is on Layout, beside Layout angle.
- PD-09: the wheel and a drag move the icons along the panel's own path; the
  panel turns only with Whole panel or Both. Travel is scene state, never
  saved.
- PD-10: closed paths carry the icons round; open paths are a longer loop, so
  icons leave one end and come back at the other, whether or not all fit.
- PD-16: one notch or one place's distance of touchpad travel is one place at
  1x; Scroll sensitivity runs from 0.25x to 4x and is saved per panel.
- PD-23: edge panels have no travel, wheel turn or motion setting; their
  smokes and the truth matrix pass unchanged.
- PD-24: Direction Up, Down, Left, Right are layout angles; the fine angle stays.
- PD-25: Continuous motion moves Items along the path (default for new free
  panels), Whole panel or Both, each with its own speed; a saved panel that
  rotated becomes Whole panel, any other Items.

### Acceptance criteria

| # | Criterion (from the task document) | Result | Proof |
|---|---|---|---|
| 1 | On free circular, ellipse, ring, hexagon, triangle, square, star and spiral panels, one wheel notch moves every entry one slot along the panel's own outline, and the panel surface does not turn. | Met | `path-travel-smoke`: on each layout, real wheel input, angle 0 before and after, travel 0 to 1, every icon in its neighbour's resting place within 1.5 px, 0 outline repaints (`after-summary-table.txt`); `tst_SceneRotation::test_wheelMovesEntriesAlongTheOutlineNotTheSurface` |
| 2 | On fan, arc, semicircle and radial panels, entries leave one end and come back at the other; entries off the path are invisible and not clickable. | Met | `path-travel-smoke` open-path loops (every icon left and came back; icons off the path not shown, not enabled, no hit rectangle); `tst_SceneRotation::test_openPathsWrapAround`; `tst_GeometryHitRegion::test_sceneHitsFollowTravel` |
| 3 | Motion starts within one frame of the wheel event and each step completes within 120 ms; a fast spin builds no backlog; touchpads and high-resolution wheels move the same distance per notch-equivalent. | Met | `path-travel-smoke`: the first frame after the wheel moved on all 12 layouts; rest 73-104 ms after the wheel event; ten notches sent at once arrived as two events, every notch counted, rest 91 ms after the last; eight high-resolution events of 15 made one place (rest 87 ms after the last); twelve smooth deltas moved by their 15.1 notch-equivalents. KWin's input emulation delivers smooth scrolling as angle steps, so a touchpad's pixel deltas are proven by `tst_SceneRotation::test_wheelInputGathersIntoWholeSteps` |
| 4 | Scroll sensitivity changes the distance per notch from 0.25x to 4x and is saved. | Met | `tst_SceneRotation::test_wheelInputGathersIntoWholeSteps` (0.25x: one place per four notches; 4x: four places per notch; held to the range); `wholePanelRotationFieldsAreGatedByTheResolver` (saved, held to 0.25-4, read back after a reload) |
| 5 | Direction Up, Down, Left and Right turns each open shape and its hit regions to that side. | Met | `tst_SceneRotation::test_openShapesFaceTheChosenSide` (fan, arc, semicircle, radial x 4 sides: path, icons and hit area); `path-travel-smoke` fan Direction; `studioMotionAndDirectionRows` |
| 6 | 'Continuous motion moves' Items, Whole panel and Both each behave as named, with separate speeds; existing panels keep their previous motion after the upgrade. | Met | `path-travel-smoke` (one notch: Whole panel turns 15 degrees with travel unchanged, Both turns 15 degrees and moves one place, Items moves one place; about one second of continuous motion at 2 places/s and 90 degrees/s: Items 1.95 places and 0 degrees, Whole panel 86 degrees and 0 places, Both 86 degrees and 1.92 places); `tst_SceneRotation::test_continuousMotionMovesItemsPanelOrBoth`; `configuration-upgrade-test` (turning panel to Whole panel, still panel to Items, backup taken, the older legacy backup untouched, `11-configuration-upgrade.log`); `PanelModelTest` |
| 7 | Hit targets, tooltips, drops, reorder, folder anchors, keyboard selection and previews follow the travelled positions; the folder-anchor gates stay 50 of 50. | Met | `path-travel-smoke` hit matrix (the pointer on each travelled icon hovers that icon and no other: circle 6, hexagon 6, star 6, fan 5; hover is what shows a tooltip); `runtime-ui-interaction-smoke` (a pointer reorder and a URI drop land on travelled icons); `tst_SceneRotation` (popup anchors, used by previews and folders, follow; `test_keyboardFocusFollowsTravelledEntries`); `folder-anchor-smoke` 50 of 50 (`28-folder-smokes.log`) |
| 8 | Edge panels behave exactly as before. | Met | `tst_SceneRotation::test_edgePanelsAndRowsDoNotTravel`; truth matrix forbids every motion setting on edge panels; edge phases of `window-interaction-smoke`, `presentation-mechanism-smoke`, `folder-interaction-smoke` and `runtime-ui-interaction-smoke` pass unchanged |
| 9 | Reduced motion is respected, and idle CPU returns to 0% when continuous motion is off. | Met | `tst_SceneRotation::test_reducedMotionJumpsAndHoldsStill`; `26-idle-cpu.txt`: a lone travelling scene used 21.4% of a core, 0.0% once continuous motion was off |
| 10 | The complete configured suite passes in one serial run on the final source. | Met | `40-full-suite.log`: 116 of 116 in 1430 s, one job, no evidence directory set |

### Tests

- Focused tests: `tst_LayoutEngineVisual`, `tst_DockGeometry`, `tst_PanelScene`,
  `tst_SceneRotation` (71 functions and rows), `tst_GeometryHitRegion`,
  `tst_PanelBaked25D`, `tst_LivePanelPreview`, `panel-model-test`,
  `panel-registry-test`, `panel-window-capability-test` (truth matrix
  included), `configuration-upgrade-test`, and the session tests
  `path-travel-smoke` (new), `runtime-ui-interaction-smoke`,
  `folder-interaction-smoke`, `folder-anchor-smoke` (50 of 50),
  `rendering-import-smoke` and `studio-truth-matrix-smoke` all passed on the
  final source.
- Complete configured suite: 116 of 116 passed in one serial run on the
  final source, 1430 s (`40-full-suite.log`). Before it: a first run was
  stopped at its fifteenth test to shorten the wheel step from 120 to 100 ms
  (a spiral step had rested 121 ms after its wheel event); the next passed 115
  of 116 in 1432 s, because `presentation-mechanism-smoke` copied each capture
  onto itself when no evidence directory is set (a test-harness defect from
  ADREP-TASK-001, hidden then because its runs set one; it now copies only to
  a different file); the next, on the final source, passed 115 of 116 in
  1418 s: `runtime-ui-interaction-smoke` got a D-Bus NoReply for Dolphin's
  desktop file dropped on the panel's folder (the applet waits at most one
  second for the backend), the environmental failure recorded on 2026-10-06
  and in ADREP-TASK-001, before any of this task's checks. The same test passed
  in the run before, on its own, and in the final run, which was repeated
  unchanged.
- Contract changes cited to product decisions in the tests: the old
  "overcrowded curve shows a window and stops at its ends" tests now check
  wrap-around (PD-10); the whole-panel wheel tests in `tst_SceneRotation`,
  `tst_PanelBaked25D`, `tst_LivePanelPreview`, `RendererCapabilityTest` and
  `runtime-ui-interaction-smoke` now ask for Whole panel, and the baked and
  session tests also check Items (PD-25); wheel turns are checked after their
  100 ms ease; `ownersFreeCircleOffersOnlyWhatWorks` models the owner's
  upgraded circle as Whole panel (PD-25).

### Files changed

- `qml/ArchDock/Rendering/LayoutEngine.js`: travel phase in `trackPlacement()`,
  `travelTrack()` for the star and the spiral, `pathWindow()` loop, baked track
  travel and box, the spiral drawn where its icons stand.
- `qml/ArchDock/Rendering/PanelScene.qml`: travel, eased steps, wheel
  gathering and sensitivity, drag, what moves.
- `qml/ArchDock/Rendering/SceneRotationController.qml`: continuous item travel.
- `qml/ArchDock/Rendering/renderers/PanelProcedural2D.qml`,
  `PanelSurfaceLoader.qml`: the outline turns as one painting.
- `qml/ArchDock/Rendering/optional3d/PanelScene3D.qml`: travelling 3D icons fade.
- `qml/runtime/SettingsPopup.qml`: the motion group on Animations, Direction on Layout.
- `src/model/PanelSettingsSchema.cpp`, `PanelDefinition.{h,cpp}`,
  `PanelPresetDefinition.cpp`, `src/panel/PanelWindowSettings.cpp`,
  `src/PanelRegistry.cpp`: the three new settings, their labels and rules,
  and the upgrade of saved panels.
- `CMakeLists.txt`: `path-travel-smoke`; `runtime-ui-interaction-smoke` limit 180 s.
- Tests: `tests/tst_SceneRotation.qml`, `tst_DockGeometry.qml`,
  `tst_LayoutEngineVisual.qml`, `tst_GeometryHitRegion.qml`, `tst_PanelScene.qml`,
  `tst_PanelBaked25D.qml`, `tst_LivePanelPreview.qml`, `PanelModelTest.cpp`,
  `PanelWindowCapabilityTest.cpp`, `RendererCapabilityTest.cpp`,
  `data/studio-truth-matrix.json`, `visibility-window.py`,
  `run-rendering-import-smoke.sh`, `run-configuration-upgrade-smoke.sh`.
- Documents: `docs/INSTALL.md`, `docs/KNOWN_LIMITATIONS.md`,
  `docs/CURRENT_STATE.md`, `CHANGELOG.md`, this report.

### Open points and limits

- Folder scrolling (Along the dock, Grid) takes Scroll sensitivity in
  ADREP-TASK-003; this task applies it to panel travel, as specified.
- Baked 2.5D and true 3D icons travel along their platform's track; passing
  behind and in front of the platform is completed by ADREP-TASK-004.
- A touchpad was not driven for real: KWin's input emulation sends smooth
  scrolling as angle steps (about 12 per pixel), which the panel counts as
  notch-equivalents. Pixel deltas, which a touchpad sends, are counted by one
  place's distance; that path is proven by the unit test only.
- Found and closed during the task: a crash reporter (DrKonqi) for a private
  test compositor that crashed while shutting down during the ADREP-TASK-001
  recheck at 10:19 was still running in your session; it was closed. The
  crash dump stays in the system's crash store.
- Not installed: the PC still runs 0.1.1-11. The package with this work comes
  with ADREP-TASK-005, so the owner checks below wait for it.

### Owner checks for this task (only what automation cannot see)

1. On your free circle (it turned before, so it starts as Whole panel): Panel
   Studio > Animations, set Continuous motion moves to Items along the path,
   Apply. Scroll over the circle: the icons move around it and the circle
   itself stays still.
2. Switch to hexagon and star: the icons follow the outline. On fan and arc:
   an icon leaves one end and comes back at the other.
3. Try Direction on the fan (Layout), Scroll sensitivity, and Continuous motion
   moves Items, Whole panel and Both (Animations).

## Report 2 - recheck

- Task commit: `e5dadc2`; follow-up commit: `1841b4e` (two document
  sentences said more than the evidence).
- Remote: `origin/main` equals `1841b4e` (verified with `git fetch` and
  `git rev-parse` before this record).
- Fresh clone of `origin/main`, round 1 at `e5dadc2`: build with one job
  passed (22 min 8 s); 18 of 18 required and focused tests passed in 950 s:
  `dock-geometry-test`, `layout-engine-visual-test`, `scene-rotation-test`,
  `geometry-hit-region-test`, `panel-scene-test`, `panel-baked-25d-test`,
  `live-panel-preview-test`, `panel-model-test`, `panel-registry-test`,
  `panel-window-capability-test` (truth matrix included),
  `configuration-upgrade-test`, `rendering-import-smoke`,
  `presentation-mechanism-smoke`, `folder-interaction-smoke`,
  `folder-anchor-smoke` (50 of 50 anchors, none with a problem),
  `studio-truth-matrix-smoke`, `runtime-ui-interaction-smoke` and
  `path-travel-smoke`.
- Round 2 at `1841b4e`: build with one job passed (22 min 7 s); the task's
  required tests passed, 10 of 10 in 297 s (`dock-geometry-test`,
  `layout-engine-visual-test`, `scene-rotation-test`,
  `geometry-hit-region-test`, `panel-scene-test`, `folder-interaction-smoke`,
  `folder-anchor-smoke` with 50 of 50 anchors, `runtime-ui-interaction-smoke`,
  `path-travel-smoke`, `configuration-upgrade-test`).
- Acceptance criteria rechecked: 10 of 10 with proof. Gaps found: none in the
  code; two sentences corrected in `1841b4e` (Known limitations said
  Continuous motion moves is always shown, but a baked arc hides it; the
  changelog said touchpads move "the same" where they are counted the same
  way and were not driven for real).
- Owner findings rechecked: OF-11, OF-12, OF-13, OF-14 and OF-15 closed;
  OF-16 closed for panel travel, folder scrolling with ADREP-TASK-003.
- Documents against evidence: after `1841b4e` this report, the install
  guide, known limitations, changelog and current state match the evidence.
- Diff from the base touches only in-scope files: 33 files from `803d18d` to
  `1841b4e` - the travel and motion code (layout engine, scene, motion
  controller, flat and 3D renderers), the Studio and the three settings
  (schema, model, presets, editor rules, upgrade), their tests, the session
  harness (including the mechanism smoke's capture copy, which the complete
  suite needs when no evidence directory is set) and the documents.
- Leftovers: no task process, private session, temporary root or crash
  reporter left; the recheck clones and their test folder are removed in
  cleanup.
- Recheck rounds: 2.
- Recheck record commit: this commit (pushed).
