# Full audit of Arch Dock after ADREP tasks 1–5, 2026-10-11

## Scope and method

- **Audited source:** `main` at `fda8801` (ADREP-TASK-005 recheck record),
  with package `0.1.1-13` installed on the owner's PC (Arch Linux, KDE Plasma
  6.7.5 on Wayland, Qt 6.12).
- **What the owner reported:** free panels, 3D panels and the horizontal and
  vertical edge panels "don't work at all"; some settings don't work; later,
  a folder opened on a 3D panel is not aligned to the panel while a flat
  panel's is.
- **Read-only phase:** the owner's settings and Plasma layout were read, never
  changed; the complete suite ran twice; a private copy of the owner's desktop
  (their layout, activity database and Arch Dock settings, the installed
  package) was driven through private KWin and Plasma sessions; every look,
  tilt, bend, material, texture and motion setting was applied to a free
  panel in a private session and photographed; the shell's CPU was sampled.
- **Fix phase:** on the owner's instruction every confirmed defect was fixed
  in the working tree and verified in private sessions.

The full report with every measurement is kept outside the repository, in
`/mnt/F/archdock-audit-2026-10-08/REPORT-2026-10-11.md`, with its evidence
beside it. This document records what it found, what was corrected in the
report itself, and what was fixed.

## Result

On the owner's desktop **no Arch Dock panel existed any more**: all 17 free
panel records were "detached" (their desktop widgets gone from Plasma), the
three edge panels were switched off, and Plasma's layout held no Arch Dock
widget. Everything the owner changed in Panel Studio was applied to panels
that were not on screen, which explains all four complaints at once. On a
private copy of the owner's desktop the product created, drew and kept both
kinds of panel across a backend restart and a Plasma restart. How the widgets
were lost could not be proved from the logs; the three most likely mechanisms
were tested and excluded.

| ID | Finding | Severity | Status |
| --- | --- | --- | --- |
| A-01 | The owner's desktop hosts no Arch Dock panel (17 free panels detached, 3 edge panels off) | High | Cause open; A-02 gives the way back |
| A-02 | A detached free panel was a dead record: never re-hosted, shown in Studio like a live panel, detached silently | High | Fixed |
| A-03 | "Arch Dock" from Plasma's widget list was an inert widget with a "drop here" invitation | Medium | Fixed (it explains itself) |
| A-04 | The bottom panel is auto-hide by migration; switched on, it is invisible until the pointer touches the edge | Medium | Fixed (Studio says so) |
| A-05 | No user instructions for creating or placing a panel | Medium | Fixed (INSTALL.md) |
| A-06 | Control widget's tooltip never showed (undefined binding) | Low | Fixed |
| B-01 | The suite is not deterministic: 5 of 120 failed once, all pass alone; two are render races | Medium | Open |
| B-02 | Folder popup on a 3D free panel once stayed open after a click beside it | Low | Seen once |
| B-03 | One wheel step took 150 ms against the 120 ms limit, under load | Low | Seen once |
| C-01 | Task 5's re-entrant wait on Plasma during recovery leaves other changes unblocked | Observation | No failure seen |
| F-01 | ~~The Orange Arc 3D panel vanishes during rotation, float and the Plasma pulse~~ | Withdrawn | Audit artifact, see below |
| F-02 | ~~A resting true-3D panel keeps Plasma at ~99 % of a core~~ | Withdrawn | Audit artifact, see below |
| F-03 | ~~The placeholder shows for seconds after a look or style change~~ | Withdrawn | Not reproduced |
| F-04 | A 3D material or texture change dropped the panel to its flat look for 140–340 ms | Low | Fixed |
| F-05 | Every dock widget was built twice: Plasma instantiated the compact and the full representation of the same dock | Medium (resources) | Fixed |
| F-06 | "Along the dock" folder contents on a 3D panel left the ring in a straight line (the owner's report) | Medium | Fixed |

### The three withdrawn 3D findings

The first version of the report blamed the 3D renderer for a whole CPU core at
rest and for vanishing in motion. Both came from the audit's own tooling:

- The test harness (`tests/visibility-window.py`) inserts a 100 ms timer into
  every icon that hit-tests the icon through the panel's input mask. In true
  3D that test ray-casts every triangle of the platform in JavaScript, so six
  icons cost 150–220 full-mesh tests a second: 99 % of a core on the
  768-triangle Orange Arc, about 30 % on the 96-triangle Cyan Mesh. Arch Dock
  itself hit-tests only on pointer events. Without that timer a resting panel
  costs 2 % (flat), 3–4 % (Cyan Mesh 3D) and 3–15 % (Orange Arc 3D) of a core
  in the Debug build; whole-panel rotation 44–45 %, item travel 58–61 %, the
  animated Plasma material 27–33 %. Any CPU figure measured through that
  harness, including earlier task gates, includes this sampling cost.
- The audit's capture hook was answered by both widgets of the private
  session (the free panel and the bottom edge panel's widget), which wrote
  the same files; the "placeholder" frames were the other widget's picture.
  Compositor screenshots taken at the same moments show the 3D panel drawn
  throughout.

## What was fixed

- **A-02, detached free panels** — `PanelRegistry::beginFreePanelRehost` and
  `abandonFreePanelRehost`, `FreePanelController::rehost` and the
  `rehostFreePanel` D-Bus call create a new desktop widget for a detached
  record with its saved settings and geometry. Panel Studio's General page
  shows a **Desktop widget** row (On the desktop, Missing, …) and offers
  **Put back on the desktop** for a missing one; the backend logs a warning
  when it notices a widget is gone. `PanelRegistryTest::rehostsDetachedFreePanel`
  covers success, failure and retry.
- **A-03** — a widget added from Plasma's widget list says where panels are
  created instead of inviting drops.
- **A-04** — Studio notes under a shown edge panel's actions that Auto-hide
  keeps it out of sight until the pointer touches the edge.
- **A-05** — [INSTALL.md](../INSTALL.md#panels-on-the-desktop) gains "Panels on
  the desktop".
- **A-06** — the control widget's tooltip binds to its own `containsMouse`.
- **F-04** — a 3D scene that has drawn once stays ready while a material or
  texture file is replaced (`PanelScene3D.qml`, `shownOnce`), instead of
  falling back to the flat renderer, with the icons jumping to the flat
  ring, until the file has loaded.
- **F-05** — the dock widget no longer declares compact-switch sizes. With
  them, Plasma chose the representation by the widget's size, ignored the
  preference for the full one, and in compact mode built the full
  representation as well for its expander popup: every free panel narrower
  than 24 grid units and every thin edge panel held two complete docks, two
  3D scenes and two sets of textures, one of them invisible. The dock is now
  built once per widget.
- **F-06** — the 3D "Along the dock" curve is the drawn icon track projected
  like the icons and then moved outward on screen, across the track's own
  direction. Before, a step out of the platform was measured in scene units
  at the folder, where the far side of a tilted platform shows a few pixels
  per unit, so the whole curve swept far out of the dock elsewhere. The host
  also widens the curve (two thirds of the way round the dock, then further,
  up to all but a sixth) while a shorter curve would hold only some of the
  children, which the foreshortened far side of a tilted platform does.
- **F-07 (2026-10-12, owner's screenshots)** — on a 3D panel a folder's fan,
  arc, stack or ring opened from the folder icon's centre, flat on screen,
  so a stack ran straight away from the ring and a fan started on the
  platform. Now a fan opens from the platform's rim beside the folder, an
  arc and a ring are centred there, and a stack lies along the rim beside
  the folder (the owner's red lines), running the way that has more of the
  screen; the fan's two straight edges are only its faint fill's border.
- **F-08 (2026-10-12, owner's screenshots)** — every texture was a small
  vector gradient, so a "textured" platform showed a colour mix, projected
  from above over the whole platform. The thirteen material looks are now
  512 px raster textures that depict their names (grass, flowing plasma,
  marble, brushed metal, riveted plate, stone, frost, circuits, tubes),
  generated deterministically from the vector swatches
  (`tools/generate-material-textures.py --raster`), and a generated platform
  wraps its texture round its band on every face. Theme packages keep their
  vector surfaces (the provenance rule allows no raster file in them).
- **Crash reports on 2026-10-12** — the eight tray icons and the
  "kwin_wayland closed unexpectedly" notice were KDE replaying, at the owner's
  login, old dumps from the audit's private test sessions (the picture-sheet
  tool, a test portal, a test shell, test compositors from 10-05 and 10-07);
  the live Arch Dock did not crash. The audit's tooling now runs with core
  dumps disabled.
- **Hit testing** — the 3D hit test keeps its answers in a memo dropped when
  anything it depends on changes, walks the platform's triangles as one flat
  array built once per mesh, and the 2D scene reuses the entry item's own
  geometry instead of recomputing the placement for every pointer move.
- The 3D scene's "frame rendered" flag no longer binds to the renderer's
  statistics, which re-evaluated it on every frame for nothing.

## Verification

- Private picture and CPU runs through the audit's harness copy, on the
  working tree, Debug build: every flat look, icon style, 3D tilt, bend,
  material, texture and motion case drawn; after the fixes no renderer
  fallback at a material change and one 3D scene per panel.
- Folder check in a private session: the same folder opened Along the dock
  and as a fan on a flat panel, on Cyan Mesh at camera pitch 60 and on Orange
  Arc at pitch 20, with compositor screenshots; all five children stand on a
  curve outside the drawn ring on both 3D platforms (before: two on the
  curve, the rest in a line off screen).
- Tests: see the "Full audit corrections" section of
  [CURRENT_STATE.md](../CURRENT_STATE.md).

## Left open

- A-01: why the owner's widgets were lost. Backend restarts, Plasma restarts
  and interrupted preset previews do not reproduce it. The owner can now put
  the 17 panels back from Studio.
- B-01: the two render races in the suite (truth matrix, preset card) and the
  teardown timeout are unchanged.
- Pedestals (owner's request, 2026-10-12): a Panel Studio page of their own
  where the small stands under the icons can be given a shape or theme
  (plate, spaceship, …), a size, a texture and animations, floating between
  the panel and the icon in 2D and in 3D. Today the 3D pedestals are plain
  columns in the platform's material and only the Dark Orb icon style has a
  2D pedestal. This is a new feature for the next task, not a repair.
- The package was not rebuilt; the owner installs packages.
