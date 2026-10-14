# Owner list, piece 1: folders open at once, the 3D star, panels that stay removed, one panel to a Studio window

The owner's list of 2026-10-13 (bugs, Studio fixes, UI themes, Phases 3 to 5)
is done piece by piece, the way the ADREP repairs were: implement, verify
(focused tests, then the complete suite once, serially), Report 1, commit and
push, a recheck on a fresh clone built with one job, Report 2. The owner chose
this ("ADREP-style") and added: "follow the rules that only apply for these
tasks", plain code comments that "read as if human would wrote the code", and
a cleanup after all phases. Piece 1 is the bugs, with the corrections the
owner sent while it was under way.

## Report 1 - implementation

### What changed for the owner

- **Folders open at once.** A 26-item folder on the owner's free-2 panel
  (Along the dock, glowing spinning star stands) held Plasma for 5.5 s before
  its first frame; it now shows in 80 ms, and every later opening in about
  25 ms. Free-1's fan opened after half a second each time; now 18-44 ms.
- **Folder stands look like the panel's.** In a glowing look (plasma, neon,
  futuristic) the stands under a folder's items were nearly black; they are
  drawn as light now, green stars like the 3D panel's own.
- **A star on a 3D panel works**, as a 3D star platform. The Layout list of a
  3D panel offers only the layouts its look can draw in 3D.
- **Removing a panel removes it.** Right-click, Remove: once Plasma has
  deleted it (when its Undo is no longer possible), Arch Dock deletes its
  settings too; the built-in Bottom, Top and Side are hidden instead.
  Profiles keep their own copies. The 22 old panels that were no longer on
  the desktop are cleared at the next start; Put back and the orange note are
  gone.
- **Panel Studio edits one panel at a time**, the one whose "Configure Arch
  Dock…" opened it. Configure on another panel moves the window there and
  asks first (Apply, Discard, Keep editing) when changes are not applied.
- **Adding a panel**: Panel Studio opened on its own, or a panel's new "Add
  panel…", shows the new panel page - Free panel, Top, Bottom, Left or Right
  edge, the bare panel in the preview, Add to desktop - and then edits the
  new panel. While a panel is edited, its place is shown there, locked. An
  edge that already has a panel (Arch Dock's, Plasma's or another dock's)
  cannot be picked: one panel to an edge.
- **Icons face you on a tilted panel.** The 3D panel's icons and an opened
  folder's items stay upright, facing you; the ones on the far side of the
  panel are only smaller. The folder's stands are now seen at the panel's
  angle, like the stars on the ring, instead of from a little above.
- **Arrow keys in an open folder** keep their selection while the pointer
  rests over the folder.

### Owner findings

| Finding | Status | How | Proof |
|---|---|---|---|
| "Folder opening on any option is super glitchy, opens with huge delay and freezes Arch Dock" | Fixed | Stand glow and fan outline blurred on the GPU (`GlowEffect`/`MultiEffect`), a moving stand repainted only at its next step and shared by stands drawn alike (`StandFrames.js`), the 3D track projected once per opening, one affine screen map | `10-before-*` (first frame 5523 ms), `20-after-foldertime.log` (80 ms first, 22-33 ms later; fan 18-44 ms, ring 25-45 ms), `11`/`12` first-show probe (worst stall 5.4 s → 0.27 s) |
| Folder stands black in a glowing look (seen in the pictures) | Fixed | `IconScene.paintLight`: the stand's colour with the look's texture screened over it | `40-stand-look-installed-0.1.1-24.png` (black), `40-stand-look-after.png`, `20-after-free2-folder-open.png` |
| "star" on a 3D panel: "Draft validation failed: validation-failed (no-safe-renderer-fallback)" | Fixed | Star platform (`PlatformGeometry`), `genericScene3D` lists it, Layout offers only 3D-drawable layouts (`layoutsDrawnIn3D`), a 3D record with another layout falls back to its look's flat renderer | `threeDPanelTakesEveryLayout`, `tst_PlatformGeometry` stars, `20-after-free2-star-3d.png` |
| A new panel came with the last edited panel's folders | Fixed | It was "+ <name>" putting a detached panel back; Put back is removed, a new panel is added from the new panel page | `studioEditsOnePanelAtATime` |
| The orange "Its widget was removed from Plasma…" box | Removed | General shows only "Desktop widget: Not on the desktop / On the desktop" | `studioEditsOnePanelAtATime` |
| "the old off-desktop panels are useless"; "remove panel ... should remove panel entirely, except the ones saved in Profiles" | Done | `panelRemovalRequested` from the widget, deletion once Plasma has deleted the widget or Plasma panel; missing widgets confirmed twice 30 s apart; detached records removed | `30-panel-lifecycle-steps.log` (records gone 5 s after Plasma deleted them), `20-after-steps.log`/`20-after-removal-lines.txt` (22 old records removed, the five kept) |
| "The Arch Dock Studio shouldn't contain anything about native panels ... only plasma edge panels must be gone" | Done | The Studio lists and makes only Arch Dock panels; an edge panel of the "empty" type (a plain Plasma panel) is not created or offered | `occupiedScreenEdges`, Content choices |
| "one panel studio window that coresponds to one panel" | Done | `editPanel`, `openNewPanelPage`, the Apply / Discard / Keep editing question | `studioEditsOnePanelAtATime`, `20-after-studio-*.png` |
| "Icon tilt is not in sync with panels tilt in here" (free-2, folder open); then "the icons should allways face the user ... no icon would be squeshed" | Done | Icons keep facing the viewer (`iconFacing`); folder items scale with depth (`folderTrackBase`); the folder's stands use the panel's own stand angle (`PanelScene.standSeenAngle`, `pedestalSeenAngle`) | `tst_PlatformGeometry` seen-angle checks, renderer test (icons facing the viewer at 45 degrees), `50-final-free2-folder-open.png` |
| Arrow-key selection in a folder jumped back to the item under a resting pointer (found by a complete suite run) | Fixed | `FolderTrack` ignores hover without pointer movement while keys select (`pointerAt`) | `keysKeepTheirSelectionUnderAStillPointer` (fails without the fix after 3 keys) |
| Panels on all four sides plus free panels; then "only on sides were there isn't any panel yet u could add it" | Done | `PanelWindow::occupiedScreenEdges` (KWin dock windows, Plasma panels, Arch Dock's own) disables taken edges | `20-after-steps.log` ("edges with a panel: bottom"), `20-after-studio-new-panel-page.png` |

### Owner decisions applied

- ADREP-style procedure for the list; no approval phrase.
- No Put back; a removed panel's settings go with it; Profiles are never
  changed by a removal.
- Keep all three built-in edge panels (Bottom, Top, Side); "Add panel…" on an
  empty edge brings the built-in one back when it is hidden there.
- One panel to an edge; the new panel page keeps its five choices (asked:
  "Keep five choices").
- Icons always face the user, on the panel and in folders; only perspective
  makes the far ones smaller. The owner first chose "Icons tilt with panel"
  (icons lying in the panel's plane); the picture showed them as thin
  slivers at the owner's tilts, and the owner corrected it.
- The owner's Plasma layout is not read or copied by any test; private
  sessions start a plain Plasma desktop with copies of Arch Dock's own files.
- Code comments explain the code; quotes and task codes stay in documents.

### Measurements

Plasma (private session, plain desktop, the owner's Arch Dock settings copied,
widgets for free-1 and free-2 added by script), AMD Radeon 610M:

| | Before | After |
|---|---|---|
| free-2 folder, first opening: first frame after the path search | 5523 ms | 80-86 ms |
| free-2 folder, path search (`openFolder`) | 101-145 ms (63 track projections) | 36-48 ms (7) |
| free-2 folder, later openings: first frame | 25-55 ms | 22-33 ms |
| free-1 fan folder: first frame | 510-561 ms | 18-63 ms |
| Ring folder: first frame | - | 25-45 ms |
| First-show probe, worst main-thread stall (26 stands, glow, spin) | 5354-5458 ms | 267-278 ms (item creation) |
| Studio edit (draft projection), p50 | 49-55 ms on earlier days | 55.6 ms; the new layout check costs 0.18-0.32 ms |
| Final build, last replica run (`50-final-foldertime.log`) | - | free-2 path search 37-46 ms, first frame 38-55 ms; fan 22-51 ms |

### Tests

All on the final code, built with one job (`build-codex-adrep/build`), the
session tests with the private bus and the test Python on `PATH`.

- Focused: `platform-geometry-test` (stars of every point count, the seen
  angle of a 3D panel's folder stands), `dock-geometry-test`,
  `folder-expansion-test` (`keysKeepTheirSelectionUnderAStillPointer`),
  `icon-tiles-test`, `renderer-parity-test`, `folder-interaction-smoke`,
  `folder-anchor-smoke` - all passed. The renderer test in a private KWin
  session on the owner's GPU: 31 passed, 1 skipped, with the test file
  unchanged from the last commit (its checks: icons facing the viewer at
  45 degrees, their stands under them). `PanelRegistryTest` and
  `PanelWindowCapabilityTest` (`studioEditsOnePanelAtATime`,
  `threeDPanelTakesEveryLayout`) passed in the complete run.
- Complete suite, serially (`03-full-suite.log`): 119 of 120 in 2384 s.
  `preset-audition-matrix-defaults` failed in its restart step: after the
  harness killed Arch Dock on purpose, the private bus had started a new
  instance in the gap, the harness kept the old process's number, and its
  next kill and stop missed ("Timed out stopping the private Arch Dock
  service"). No crash, no core dump. Alone it passed (59.9 s,
  `04-rerun-preset-audition-defaults.log`), as on the earlier days recorded
  in the test notes.
- A second complete run was stopped at test 37 when the owner found the
  folder openings still out of step with the panel's tilt
  (`superseded/05-full-suite-second-stopped.log`); the folder work below came
  after it.
- Private replica on a plain Plasma desktop (`50-final-*`): the owner's
  settings copied, free-1 and free-2 added by script; the owner's files
  hashed before and after, unchanged.

### Files changed

- `qml/ArchDock/Rendering/IconScene.qml`, `StandFrames.js` (new),
  `FolderTrack.qml`, `PlatformGeometry.js`, `LayoutEngine.js`,
  `PanelScene.qml`, `optional3d/PanelScene3D.qml`; `CMakeLists.txt`,
  `tests/ValidateRenderingModule.cmake`.
- `plasma-dock-widget/contents/ui/FolderTrackHost.qml`, `main.qml`.
- `qml/runtime/SettingsPopup.qml`.
- `src/PanelRegistry.{h,cpp}`, `src/panel/PanelWindow.{h,cpp}`,
  `PanelWindowFreePanels.cpp`, `PanelWindowNativePanels.cpp`,
  `PanelWindowScreens.cpp`, `PanelWindowSettings.cpp`,
  `FreePanelController.{h,cpp}`, `src/WindowWatcher.{h,cpp}`.
- Tests: `PanelWindowCapabilityTest.cpp`, `PanelRegistryTest.cpp`,
  `tst_PlatformGeometry.qml`, `tst_FolderExpansion.qml`.
- Documents: `CHANGELOG.md`, `docs/CURRENT_STATE.md`,
  `docs/KNOWN_LIMITATIONS.md`, `docs/repairs/README.md`, this report.

### Open points

- Two dozen folder items are created when the folder opens (about 0.1 s on
  the owner's PC), the one pause left; creating them ahead, or reusing them,
  is possible later.
- Plasma lets panels on one edge cover each other; Arch Dock keeps to one
  panel an edge rather than arranging several.
- The backend still accepts an "empty" edge panel (a plain Plasma panel) from
  older records and its D-Bus calls; the cleanup after all phases can remove
  that path.
- Labels of neighbouring folder items can overlap on a crowded track
  (seen in `20-after-free2-folder-open.png`); not changed in this piece.
- Next, at the owner's word after "The box still cuts off": a panel holds
  at most as many icons as its size allows ("the max amount u can calculate
  by talking a look at my native panel icon spacing"), and "nothing needs to
  be cut off when resized"; "that space should be there no matter of the
  icons size, there for the bigger the icons the lesser of them fits on
  panel". Measured from the owner's own screenshot of their top Plasma
  panel ("Look at top panel"): icons about 30 px, centres 46 px apart, so a
  16 px gap; an icon's slot is its size plus that gap. On closed shapes the
  layout now spreads every icon evenly whatever the count, so too many
  overlap. Free-1 itself is not cut: its octagon looks flat on the right
  only from its -10.85 degree turn (at turn 0 both ends are points).
- A folder opened along the dock shows what its curve holds and scrolls the
  rest with the wheel; a second lap was tried and dropped (the owner: "No,
  no second lap").

### The owner's own checks

After installing the final package (only once the whole list is done): open
a folder on each panel; remove a panel from its right-click menu and see it
gone from the Studio; Add panel… on an empty edge and on a taken one. The 24
free panels the earlier versions had left off the desktop are removed at the
first start (the owner chose "Delete all 24"); Profiles keep their copies. A
free panel still on the desktop comes back by itself if Plasma loses it at a
login.

## Paused - 2026-10-14, at the owner's word

"Commit and Sync and pause. We'll continue tomorrow." Piece 1 is committed
as it stands; it is not closed. The complete serial suite above ran before
the folder work below, and the fresh-clone recheck has not been done.

### Folder openings follow a 3D panel's tilt (after Report 1's tests)

The owner circled free-2's track and free-1's fan: "The tilt is still wrong,
it's not in sync with panels tilt in both cases"; confirmed on a sketch
("Yes, like the sketch"), then corrected step by step:

- Along the dock: the curve lies on the panel's own plane (`PanelScene3D`
  `folderTrackSamples`/`folderTrackBase`, traced once per opening through one
  affine map to the camera), starts at the platform's rim, and moves out only
  as far as keeps every child clear of the panel as drawn - its outline and
  icons, 6 pixels apart ("it shouldn't overlay the panel at all, nor even
  Touch it"; `PanelScene3D.folderKeepOut`, `LayoutEngine.rectTouchesKeepOut`).
  It spans a quarter to three quarters of the way round ("3 of them should
  be the max length and one of the should be min length"); the rest scroll.
  A second lap was tried and dropped ("No, no second lap").
- Fan, arc, stack and ring on a 3D panel lie on the panel's plane
  (`PanelScene3D.folderPlane`, `LayoutEngine.folderShape` `project`), drawn
  as the panel's middle is seen about where they open (`alignedAt`), never
  opening back across the panel. Fan, arc and stack open at their smallest
  (the owner chose "Smallest size, scroll rest"): three named children at
  once; a new panel's Stack length is 3.
- Ring: six at its smallest; more get a second circle round the first, the
  inner well inside the outer with a 30 pixel gap, the two leaning 10
  degrees opposite ways on a 3D panel (the owner chose "B: gimbal"; "one
  should be bigger and one should be smaller inside the bigger one").
  Scrolling carries children round both circles (`ringedTrackLayout`).
- Approved by the owner on pictures: along the dock clear of the panel, the
  aligned circle with names, free-1's flat fan.

Tests on this code: `tst_FolderExpansion` 194/194 (plane shapes, gimbal
circles, nesting), `tst_DockGeometry` 560/560 (two-circle layout),
`tst_PlatformGeometry` 58/58, `tst_FolderEasing` 33/33,
`panel-settings-schema-test` 22/22 (Stack length default 3); the private
replica `tilt-plane`, `two-circles`, `gimbal` runs (owner files unchanged).

The open points this section listed are done below.

## Resumed - 2026-10-14

### Free panels lost at a Plasma start

- Finding: after the morning's login the owner's free-1 and free-2 were gone
  from Plasma (asked, the owner: "No, both are gone"). The installed 0.1.1-24
  looked for free-1's widget for 90 s, found it nowhere and marked both
  detached at 08:40:27; Arch Dock's own log shows the same after the Plasma
  restarts of the day before (14:20 for free-1 and free-2, 18:13 for free-4).
  All 24 free records in the owner's settings were detached that way
  (`owned-host-not-found`). The owner's Plasma layout is not read, so the
  cause there stays unknown; a private plain Plasma keeps the widget through
  a graceful restart and Arch Dock finds it again 4 s later, with this build
  (`60-restart-this-build-*`) and with the installed version after Put back
  (`61-restart-installed-putback-steps.log`).
- The code committed in a2adecb would have turned that into a loss: it
  deleted a free panel whose widget stayed missing (90 s after the backend's
  start, 30 s apart), and the grace did not start again when Plasma did.
- Now (the owner accepted "Arch Dock puts such a panel back by itself"): a
  missing widget never removes or detaches a panel. After the grace - 90 s
  from the backend's start, or from Plasma's return on the bus - and 30 s of
  being missing, `FreePanelController::restore` gives the panel a new widget
  with its own token, at its place and size, once per Plasma start; it never
  adds one while any widget still shows the panel (`freePanelWidgetCount`).
  Only the owner's own removal deletes a free panel. The 24 detached records
  are deleted at the first start (the owner: "Delete all 24", Profiles keep
  the copies).
- Proof, private plain Plasma (`62-lost-panel-*`): the 24 old records went
  at the start; a new panel's widget was dropped from the private layout
  while Plasma was down; 91 s after Plasma came back it was there again,
  same place, size and token; removed with Plasma's own removal it stayed
  gone. `PanelRegistryTest::restoresFreePanelPlasmaLost`: put back with the
  settings kept, refused while a widget shows the panel, a failed try leaves
  the record as it was, a detached record is not put back.

### Folder openings on a 3D panel

The pictures last night and this morning (`70-openings-before.png`) showed
three faults besides "too far": a fan with three named children showed two,
the stack one fewer than its length, and the inner circle of two was crowded.
On a tilted plane the track spaced children in flat units along a path drawn
in perspective. Changes, each checked on pictures of last night's free-2:

- Children on a plane are a pitch apart at the size each is drawn
  (`trackRun`, the track's `unscaled`), and every shape is as large as its
  smallest count needs that way: three named children on a fan, an arc and a
  stack, six on a ring (`71-*`). A shape holds fewer only where none fits the
  screen; then it stands as close to the panel as it can. The room a child
  fading past an end passes through no longer counts against the screen.
- No child is drawn larger than the folder's own icon (`largestScale`): ones
  opening towards the viewer had grown to twice its size, labels too.
- Two circles: the small one inside the big one with 16 pixels between their
  lines as drawn, a glow not counted (`outlineWidth`); where the screen is
  short the first circle holds four, then three, before the ring keeps one
  circle. Their sizes are worked out once per opening: 174 ms instead of
  1.1 s (`71-two-circles.png`).
- Grid on the plane: two rows of three, moving a row at a time
  (`gridTrackLayout`); first drawn on a light board, which the owner crossed
  out ("Grid layout should not have any visible borders, meaning don't box
  it in"): no border now (`73-grid-no-border.png`).
- Stack: the owner drew "−0" crossed and "|0" ticked - "the stack goes up and
  down ... bottom - right and left, sides up and down". It lies along the
  panel's side nearest the folder, centred on it and no longer than it, a
  little way out of the rim (`sideOut`); the flat panels' rule.
- Fan: the owner drew a line round free-2's right side ("it needs to hug it a
  bit"). The fan follows the rim round the folder, each point just clear of
  the panel as drawn, as long as its children need and at most half way
  round (`hug`); a band along the children, nothing filled
  (`72-fan-hugs-stack-along-side.png`).
- Timings in plasmashell (path search, then first frame): fan 16-29 ms /
  41-59, stack 9-11 / 18-38, grid 67-88 / 36-86, two circles 156-177 /
  35-61, ring with names 452-548 / 25-46.

Tests: `tst_FolderExpansion` 197/197 (`folderShapesLieOnATiltedPlane` with
the grid and the drawn spacing, `ringCirclesLeanLikeAGimbal` with the gap
all round, `planeShapesFollowThePanel`, `gridMovesRowByRow`),
`tst_DockGeometry` 560/560, `PanelRegistryTest` 72/72.

Complete suite, serially, on this code (`80-full-suite.log`): 118 of 120 in
2337 s. The two failures were both of piece 1 itself, never run since the
folder work of the night before:

- `panel-window-capability-test` `studioFolderShapeRows` still expected Stack
  length 5 by default; a new panel's stack is 3 since that night. The test
  now expects 3.
- `folder-anchor-smoke`: on the baked ring Along the dock opened 150 px from
  the folder (allowed: about 105). The night's change let the curve step out
  by up to four icons on every panel, and start at a quarter of the way
  round; that was meant for a tilted 3D panel. Flat and baked panels again
  step out at most 64 pixels and start at a third of the way round (within
  the owner's quarter to three quarters), as before. The matrix's 3D rows
  still measured the shapes as if they were drawn flat on the screen -
  circles, equal distances, the shape's own direction taken as a screen
  direction - none of which holds on a tilted plane. They now check what is
  seen there: no child over the dock or on one of its icons, the shape
  beside the folder and away from the dock, the fan following the rim with
  its band, the stack a straight line, the grid with no border and at most
  two rows of three, the ring drawn beside the dock on the side with more of
  the screen (either side for a dock in the middle), and Along the dock
  starting a little further out where it keeps clear of the drawn panel. A
  stack along a short side holds two at least, as on a flat panel.

Reruns on the fixed code: `panel-window-capability-test` passed
(`81-rerun-two-fixed.log`); `folder-anchor-smoke` passed, all 90 openings
(`84-rerun-anchor.log`); `folder-interaction-smoke`, `tst_FolderExpansion`,
`tst_DockGeometry`, `tst_PlatformGeometry` and `tst_FolderEasing` passed
again after the last changes (`85-rerun-folder-tests.log`).

### Free panels keep apart

The owner, after openings from free-2 lay over free-1: "Folder openings
should not overlay other free docks, so there should be minimal aloud
distance between free docks depending on their size". Asked, the owner
chose "Push it out" (a panel dropped too close moves straight away from the
other, just far enough) and "Room for an opening" (one folder child - icon,
stand, name - at the bigger panel's drawn size, plus a small gap).

- Each free panel's widget reports where it is seen and the room its
  smallest opening needs (`reportFreePanelArea`); the backend hands the
  other panels' areas to every folder snapshot. On a 3D panel the area is
  the box round the platform and icons as drawn (`PanelScene.seenBounds`):
  the 3D view itself covered most of the screen (1024 x 640 and 1320 x 1000
  in the first try, `apart-2`).
- Every opening keeps its children clear of the other panels (`avoid`); a
  ring tries the dock's other side, a stack the next side when another
  panel would push it far out.
- After the owner moves a panel - on the desktop, in Panel Studio, with the
  3D handles - it checks the distance once the move has settled and moves
  straight away from the panel it is too close to (`LayoutEngine.keepApart`),
  within the screen. Profiles, previews, a panel put back and the first
  seconds after starting move nothing.
- Proof (`apart-4`, private plain Plasma, last night's free-1 and free-2):
  97 px apart at the start, left as they were; free-1 moved in Panel Studio
  to 10 px from free-2 stood 157.2 px away half a second later (minimum
  157.1: free-2's room 149 and 8 px). Openings from free-2 - two circles,
  fan, stack, grid - cleared free-1 (`87-keep-apart.png`).
- Tests: `tst_FolderExpansion` `openingsKeepClearOfOtherPanels` (every shape
  gives way to a panel where it would open), `freePanelsKeepApart` (straight
  away, just far enough, on the screen, nothing for a panel standing clear);
  `PanelWindowCapabilityTest::freePanelAreasAreShared`; the focused run
  `86-apart-focused.log` 7/7.
- Complete suite, serially, on this code (`88-full-suite-keep-apart.log`):
  119 of 120 in 2385 s. `preset-audition-matrix-defaults` failed in its
  restart step as in the first run ("Timed out stopping the private Arch
  Dock service", the harness's old process number gone after the private bus
  had started a new instance); alone it passed in 55.9 s
  (`89-rerun-preset-defaults.log`).

### Still open

- The ring with names takes about half a second to open on free-2.
- Why the owner's Plasma drops Arch Dock's widgets at a start.
- Next after piece 1: the owner's icon limit by panel size (a 16 pixel gap
  measured on the owner's top panel; nothing cut off when resized), a Möbius
  strip opening, editing a saved profile (the owner: "not only profiles but
  also editing it" - "Edit a saved profile").


## Report 2 - recheck

- Source: `9a97a92` on `main`, pushed; the remote and the local branch were
  the same before the clone.
- Fresh clone of `origin/main` (`90-recheck-clone.log`), nothing copied
  from the working build: Debug with Quick 3D configured (`91`) and built
  with one job, exit 0 in 1359 s (`92-recheck-build.log`).
- Ten focused tests serially on the clone's build, all passed
  (`93-recheck-tests.log`): `panel-settings-schema-test`,
  `panel-registry-test`, `folder-content-model-test`,
  `panel-window-capability-test`, `dock-geometry-test`,
  `platform-geometry-test`, `folder-expansion-test`, `folder-easing-test`,
  `folder-interaction-smoke`, `folder-anchor-smoke` (all 90 openings).
- All 652 committed files are the same in the clone and in the workspace
  (`94-recheck-source-compare.txt`).
- The complete suite ran on this source before the push: 119 of 120, the one
  failure the known restart race of `preset-audition-matrix-defaults`, which
  passed alone.

Piece 1 is closed. Next on the owner's list: the icon limit by panel size,
the Möbius strip opening, piece 2 (the Studio fixes, editing a saved
profile).
