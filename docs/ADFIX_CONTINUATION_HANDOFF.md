# ADFIX continuation handoff — 2026-10-06

Work on the owner's correction pack
`/mnt/F/Arch_Dock_Full_Correction_LCL_3Task_v1.0.1/` (ADFIX-TASK-001, -002, -003)
stopped on the owner's instruction ("Finish current run, commit and sync, make a
report with handoff, and stop") about a third of the way through. This file is
the place to resume from. `docs/CURRENT_STATE.md` carries a short pointer here.

Nothing was installed on the owner's PC: it still runs **arch-dock 0.1.1-9**.
No tag or release was made.

## 1. Owner instructions in force

- Run ADFIX-TASK-001 through -003 in order; when all three are done, commit and
  sync and update Arch Dock on the owner's PC. Report progress in percent.
- The pack pre-authorizes all three tasks (no approval pauses). Its rules still
  apply: one session, no subagents, one heavy process at a time, one build job
  (`--parallel 1`), one test worker, reproduce before fixing, never weaken a
  test, no tag or release publication. `./validation/verify_all.sh` in the pack
  passed at the start of this session.
- Added by the owner during the session, not started yet:
  1. **New icon and logo.** "Add this Icon to Arch Dock, it'll be Icon and Logo"
     (transparent background). The file is
     `/home/aivars/.claude/uploads/422a9e74-4c7e-4751-a1a9-270e4b115d12/205a2a37-image.png`
     (1254 × 1254 RGBA, real transparency, SHA-256
     `40cce92ccb42919184e4fe77a743f1a92c9d8e7120460a6544734359bef16c34`).
     Copy it into the repository first; the upload folder is not permanent.
     Plan: hicolor theme sizes (16 to 512) installed as `org.archdock.ArchDock`,
     `Icon=` in `data/org.archdock.ArchDock.desktop.in`, both applets'
     `metadata.json` and `Plasmoid.icon`, the Studio window icon, the README
     logo, and a provenance line in `packaging/LICENSING.md`.
  2. **Comments.** "add commenting to rest of the Arch Dock code too … what the
     code's purpose is, what it does, necessary comments". Plan: measure comment
     density per file, then add file purpose headers and class/function comments
     where missing; no comment on every line.
  3. **Readability.** "look if code is messy and make it good, readable, so that
     community can help later on". Plan: behaviour-preserving cleanup proved by
     the full suite; split `src/panel/PanelWindow.cpp` (8,220 lines) only by
     moving whole member functions into topic files. The pack had deferred this
     (AUD-12); the owner's request now includes it.
- **Folder design decision (owner's answers, 2026-10-06):** curved free panels
  get a new folder layout **"Along the dock"**: the folder's contents grow out of
  the folder along an outer curve that follows the dock's own shape, tilt and
  perspective, with no visible platform ("invisible"). It is the default for
  curved free panels. The five existing layouts (fan, grid, stack, arc, ring)
  stay selectable and open as outward-facing popups; straight native edge
  panels keep the popup. Not implemented yet (see 4.2).

## 2. Done and verified in this commit

### 2.1 Intentional Quit (UF-10, UF-11) — complete

Root cause, reproduced from the owner's journal: the KWin script
`org.archdock.windowwatcher` sends `callDBus("org.archdock.ArchDock", …)` on
every window event, and a call to the activatable name starts the backend.
After the owner killed it at 23:02:35 and 23:11:14 it was back within 3 to 4
seconds (`@2`, `@4` activation units) with no Arch Dock applet on the desktop.
The owner's user also lingers (`Linger=yes`), so `/run/user/1000` outlives a
login.

- `src/IntentionalStop.{h,cpp}`: the stop request lives in
  `$XDG_RUNTIME_DIR/arch-dock-intentional-stop.json` and names its login
  session (boot id + `XDG_SESSION_ID`), so it ends with the session even for a
  lingering user. Only this user's small regular file is trusted; links and
  foreign entries are removed, never followed.
- `arch-dock --quit` and the D-Bus method `local.PanelWindow.quit` record the
  stop, cancel an active preset/3D audition (the desktop returns to the saved
  state), unload both KWin watcher instances and leave the event loop. Quit is
  refused while a profile is being applied.
- The D-Bus service file now runs `arch-dock --dbus-activated`. Such a start
  during a recorded stop exits with status 75 at once (callers get
  `Spawn.ChildExited` instead of waiting for a timeout) and unloads the watcher.
- Any explicit start (`arch-dock`, menu entry, `--settings`, the systemd unit)
  clears the stop. TERM and INT stop Arch Dock exactly like Quit (self-pipe
  handler). KILL runs no code: it is a crash, and activation recovers it.
- The application entry gained a **Quit Arch Dock** action.

Verification: `intentional-stop-test` (4 cases); `session-startup-metadata-test`
and `session-startup-diagnostics-test` with the new exact `Exec` contract;
a private-bus check of ten steps; and `session-startup-runtime-test` in the
disposable Plasma session with the installed layout and the source hidden:
KILL → recovered through KWin's activation request; `--quit` → stayed stopped
through a plasmashell restart, whose applets requested activation twice and
were refused; explicit start → normal; TERM → stayed stopped. PASS.

### 2.2 Folder contents open from the clicked folder, outward (UF-01, UF-02) — mostly done

- `LayoutEngine.expansionGeometry()` takes `side` (`left`, `right`, `top`,
  `bottom`) and `lean` and lays every layout out in that side's frame: the
  compact fan/arc half circle bulges away from the folder, ring and stack start
  beside it, and an `anchor` says where the folder stands on the near edge.
  Without a side the original frame (opening right) is unchanged.
- `FolderExpansionHost.qml` chooses the side when the folder opens: a native
  panel opens away from its screen edge; a free panel opens the way the folder
  faces out of the dock and flips only without screen room. It attaches the
  popup to a small item on the folder icon, computed once from the popup's
  final size, so the contents' anchor lands on the folder.
- True-3D folders take their outward direction from the projected platform
  centre (`PanelScene3D.projectedCentre`).

Verification: `dock-geometry-test` (20 new side × layout rows), `folder-expansion-test`
51/51 including 30 new host rows, no binding loops. New native gate
`folder-anchor-smoke`: before the change **0 of 35** openings were anchored
(every layout opened back over the dock); now **29 of 35** pass, every
true-3D case included.

## 3. Known failures at this commit (diagnosed, not fixed)

1. **Scrollable layouts start at the far end.** `FolderExpansion.onOpenedChanged`
   resets `contentX`/`contentY` to 0. For a popup on the `left` side the folder's
   end is the right end of the contents, on the `top` side the bottom. Fix: on
   opening, scroll to `contentWidth - width` (left) or `contentHeight - height`
   (top). Fails today: `folder-interaction-smoke` at the native bottom panel's
   ring layout (`AssertionError: ([640, 420], [363.58, 484.67])`: child 0 below
   the visible popup) and `folder-anchor-smoke` baked `left/ring`, `top/ring`.
2. **Baked ring, diagonal folder (315°).** The popup opens up-left although the
   folder is up-right of the dock: the host received an outward direction with
   a negative x. The baked track computes it analytically in
   `LayoutEngine.trackEntryGeometry()` from the flat track; true 3D, which
   measures it from the drawn centre, passes. Fix to try first: derive the
   outward direction of every curved free panel from the drawn entry position
   relative to the drawn dock centre (as `PanelScene.entryGeometryAt()` now does
   for true 3D). Fails today: `folder-anchor-smoke` baked `diagonal/fan`,
   `/grid`, `/arc`, `/ring`.
3. `folder-interaction-smoke`'s `assert_half_circle` expects the original
   right-opening frame. After fix 1, check that every fan/arc case still passes;
   a popup now opens away from the panel edge, so the assertion must take the
   popup's side into account (same half-circle contract, oriented).

## 4. Remaining work, in order

### 4.1 ADFIX-TASK-001
1. Fix section 3 items 1 to 3; rerun `folder-expansion-test`, `dock-geometry-test`,
   `folder-anchor-smoke`, `folder-interaction-smoke` one at a time.
2. **"Along the dock" folder layout** (owner decision above). Design worked out:
   a transparent full-screen Plasma dialog (the pattern of Plasma's Application
   Dashboard) on the folder's screen, closed by Esc or a click outside, drawing
   the children at exact screen positions: an outer track concentric with the
   dock (radius = track radius + one pitch) centred on the folder's angle,
   through the dock's own projection (2D circle, baked ellipse with tilt, true 3D
   via `View3D.mapFrom3DScene` with depth scale), children unfolding from the
   folder icon, wheel moving along the track when they do not fit. New schema
   value for `folderLayout`, the default for new curved free panels only:
   existing saved panels keep their layout (tell the owner how to switch).
   Extend `folder-anchor-smoke` to it.
3. **Wheel with continuous animation None (UF-04).** Not reproduced yet. Every
   code path read is mode-independent and the existing native wheel test passes
   with mode none on Cyan mesh, Blue ring baked and a 2D ring. Reproduce with the
   owner's own panels: free-9 (circular, `arc-platform-orange`, true3d, pitch 60,
   radius 300, Arc folders) and free-4 (circular procedural, mode none). Note
   that open arcs (free-6, free-7) neither rotate nor browse unless overcrowded.
4. **Opening mechanisms (UF-05).** Live capability reports: procedural panels
   offer collapse-horizontal/-vertical (also on circular layouts), baked themes
   offer only open, true 3D offers collapse-radial. Build the per-renderer matrix
   with real pixels and hide or refuse every offered mechanism that draws
   nothing; correct the collapse-radial documentation (AUD-10).
5. Task 001 docs (`CHANGELOG.md`, `docs/CURRENT_STATE.md`, `docs/shared-renderer.md`),
   affected gates, close Task 001.

### 4.2 ADFIX-TASK-002 and -003
As in the pack. Facts gathered so far: the owner's peach ring in screenshots
03 to 05 is free-9, Orange's own true-3D tier; "baked blue ring" is free-8
(`ring-platform-blue`, baked, clockwise). Generic 3D still substitutes the
generic mesh look (UF-03). A true-3D icon's projected rectangle changes size
with its hover motion, so its input rectangle "breathes" under the pointer;
look at this with the icon/pedestal work (UF-09). Task 003 owns the full
suite, canonical exports, the package (0.1.1-10 if the baseline is still
0.1.1-9), the installed-package harness and the live install. On upgrade, run
`busctl --user call org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus ReloadConfig`
so the session bus reads the new service file (`--dbus-activated`) before the
next activation.

## 5. Environment

- Build: `build-codex-adfix/build` (Debug, Quick3D ON, configured with
  `-DARCHDOCK_TEST_TMPDIR=/mnt/F/adfx-t`). Fresh one-job build took 1322 s.
  Build with `cmake --build build-codex-adfix/build --parallel 1`; QML module
  changes need a build too (tests import the copy in `build/qml-imports`).
- Native gates need PySide6: `PATH="$PWD/build-codex-adfix/python/bin:$PATH"`
  (venv created from the local uv cache, PySide6 6.11.2).
  Example: `ctest --test-dir build-codex-adfix/build --parallel 1 -V -R '^folder-anchor-smoke$'`.
  `ARCHDOCK_SCENE_EVIDENCE_DIR=<dir>` keeps the anchor geometry JSON and logs.
- Run native gates one at a time; never with a build running.
- Evidence and logs, numbered in order: `build-codex-adfix/evidence/` (git-ignored):
  `02` baseline build, `03` UF-10 before (owner journal), `12` private-bus Quit
  check, `14` Plasma-session Quit test and its logs, `16` folder anchors before
  (35/35 fail), `19` folder anchors after (29/35 pass), `20` folder interaction
  smoke (fails at bottom/ring, section 3.1).
- Task-owned leftovers to remove at the very end: `build-codex-adfix/build`,
  `build-codex-adfix/python`, `/mnt/F/adfx-t`. Keep `build-codex-adfix/evidence`.
