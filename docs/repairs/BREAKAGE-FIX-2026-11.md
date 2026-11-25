# Arch Dock breakage fixes, November 2026

The owner's request of 25 November 2026, after the read-only breakage audit
(`/mnt/F/Arch_Dock_BREAKAGE_REPORT_2026-11-25.md`, every finding in
`/mnt/F/archdock-audit-2026-10-08/evidence/BREAKAGE-2026-11-25/findings.md`):
"proceed with the plan, and follow the rules". The plan has six steps. On the
owner's choice Step 6 (clean up `main`) goes first, then Steps 1 to 5 in
order. The owner's other answers: the AI "Modelled" 3D icons stay as a
choice per panel, off by default; the free panel deleted on 31 October is
not rebuilt (the owner makes a new one).

Each step: implement, focused tests, the complete suite once (serially),
Report 1 here, one commit on `main` and push, a fresh-clone recheck built
with one job, Report 2 here. Evidence (logs, pictures, measurements) is in
`/mnt/F/archdock-audit-2026-10-08/evidence/BREAKAGE-FIX-S<n>/`.

## Step 6: the unfinished 30 October work leaves `main`

### Report 1

**What changed.** The four commits made after the published 0.1.1-54
(`812081d`) are taken off `main` with one revert commit; no history is
rewritten. They are kept whole on the branch `widgets-3d-wip` (at
`6619984`), to be finished and tested later:

- `b4d4a3e` A bar's icon and status widgets are drawn as its icons
- `2db8255` A bar's widgets drawn as its icons: 3D, holographic and animated;
  faces as holograms
- `ba0225e` A bar's widget faces as 3D icons; the dock keeps its widget
  drawings
- `6619984` Arch Dock commit after sub ended (unfinished: a diagnostic timer
  writing to the log every 3 s from every dock, every face widget
  photographed and saved as a picture each second, Plasma's own panel
  widgets hidden while the dock draws them)

After the revert the source tree is the published 0.1.1-54 byte for byte
(`git diff --cached 812081d` was empty before this report and the state note
were added). None of this work ever reached the owner's PC, which runs
0.1.1-53.

**Why first.** The audit's findings 20, 24, 26, 30, 31, 34, 41, 43, 45, 65
and 70 are this code: constant load (log lines every 3 s, a picture and a
picture file per face widget every second, the AI icon maker run again for
every status icon) that would have distorted every measurement of Step 2,
in bar files that Steps 2 and 4 change.

**Checks.**

- Tree identity with the published 0.1.1-54: `git diff --cached 812081d`
  empty.
- Build: `build-codex-adrep/build` rebuilt from this tree (the system update
  of 25 November had replaced Qt's files, so every part was rebuilt).
- Complete suite, one test at a time, the test Python first on PATH:
  135 of 136 passed in 3464 s (`evidence/BREAKAGE-FIX-S6/suite.log`). The one
  failure is `path-travel-smoke`, the Star layout check that has failed since
  26 October: the same assertion ("star: an entry left the outline between
  slots"), at 15.8 this time against 13.8 on 27 and 28 October, on the same
  source; the distance it measures depends on frame timing. This run is also
  the baseline for Steps 1 to 5 on the updated system (Mesa 26.3.1, KWin
  6.8.3, Qt 6.12.0).

**Open points.** The widget drawing work on `widgets-3d-wip` comes back only
when it is finished and tested. The audit lists what it needs: no diagnostic
timer, face pictures taken only when a face changes, and Plasma's widgets
hidden only while their drawing is on screen.

### Report 2

Fresh clone of `origin/main` at `de4ca02`, built with one job (2110 s), the
step's tests one at a time: 11 of 11 passed in 256 s (bar-host-smoke,
bar-view-test, barlayout-test, barwidgets-test, icon-solid-test,
panel-scene-test, icon-scene-test, dock-entry-test, rendering-import-smoke,
decoration-3d-smoke, source-exporter-test). All 990 committed files are the
same in the clone and the workspace. Logs:
`evidence/BREAKAGE-FIX-S6/recheck/`.

## Step 1: panels are not lost, removed panels are kept, no silent "off" (paused)

Committed on 25 November at the owner's request to pause for the day,
before this step's cycle was finished: the complete suite was stopped after
91 of 137 tests (all 91 passed, `evidence/BREAKAGE-FIX-S1/suite.log`), and
the second read-only review was stopped after its documentation part (its
four wording points are fixed in this commit). Report 1 and the fresh-clone
recheck (Report 2) follow when the step is finished.

Checked so far:

- Focused tests: `removed-panel-store-test`, `intentional-stop-test`,
  `panel-registry-test`, `configuration-backup-test`,
  `session-startup-runtime-test` passed; `panel-window-capability-test`
  105 passed, 4 skipped, and `barsFollowTheirRecords` failed once on its own
  setup (the built-in Plasma Bottom panel stood on the bar's edge, where
  Bring back follows Panel Studio's rule); the test now checks both cases
  and passes.
- Private test desktops with the owner's 29 October free panel (icons and
  empty), the same steps as on 0.1.1-54 (`evidence/BREAKAGE-FIX-S1/`,
  sheets in `sheets/`):

  | | 0.1.1-54 | Step 1 |
  |---|---|---|
  | Ended with TERM | stop kept, still off 10 s later | no stop kept, back within 10 s |
  | Quit, panel with icons | says nothing | "Arch Dock is stopped" + Start over the icons |
  | Quit, empty panel | "Arch Dock service is unavailable" + Retry | "Arch Dock is stopped" + Start |
  | Two copies, Remove Panel | returns nothing, both stay | both go, settings kept |
  | Panel Studio, new panel page | nothing kept | "Removed panels" + Bring back; Bring back works |
  | Plasma removes the widget | record gone within 5 s, nothing kept | 60 s after start: not acted on, widget put back; 211 s after start: record still there at 30 s, gone at 45 s, settings kept |

- `tests/run-plasma-lifecycle.sh` (not in CTest) passed in full in 205 s,
  with its screen-off phase left out (see below). It had not run in full
  since October: two checks were out of date since `36ce959` (Empty
  panels) and the template phases raced the built-in Bottom panel's
  start-up recovery; the code before this step (a build of `2d70771`)
  failed the same way, and the test now follows the current behaviour.
- Two read-only reviews; the first found four points beyond the bar test
  (a refused removal could drop the oldest kept panel; a failed last step
  of a confirmed removal put the panel back; Remove Panel was silent for a
  panel no longer known and when Arch Dock itself did not answer), all
  fixed. A removal reported in the first 90 s after a start is now looked
  at again, so a widget that is really gone is put back.

Open:

- The lifecycle test's screen-off phase crashes the private test session's
  plasmashell 6.8.3 (segmentation fault in `KConfigGroup` under its script
  engine) with the code before this step as well (three core dumps on
  25 November). Not part of this step; it needs its own look, since a
  screen going away on the owner's desktop may meet the same crash.
- On the test desktop only, Panel Studio showed "Panel settings could not
  be loaded (panel-not-found)" after the last panel was removed: that
  desktop's panel list has no built-in panels, which the owner's has.
