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

## Step 1: panels are not lost, removed panels are kept, no silent "off"

### Report 1

**What changed.**

- A report from Plasma that a panel's widget is gone no longer deletes the
  panel within seconds. It counts only when Plasma has loaded its desktops
  and the widget is still missing at two looks at least 30 seconds apart;
  the report stays until the panel is really gone from Arch Dock too (a
  failed step is tried again about 30 seconds later), and a report not
  confirmed and carried out within 10 minutes is dropped. A report from a
  second copy of a panel's widget speaks only for that copy, and a report
  never passes to a newer panel that took the same number.
- Right after Plasma starts (at login or after an update; the owner's
  panel was lost 40 seconds after one on 31 October), a report is not taken
  for the owner's removal: the widget is watched the same way, and if it is
  really gone it is put back. Arch Dock starting by itself next to a Plasma
  that has run for a while takes reports as they come (still only after
  two looks). When only Arch Dock's widget is taken off an edge panel, the
  widget is put back. The built-in Bottom, Top and Side panels are hidden,
  never deleted.
- Before a panel is deleted (Remove Panel, a removal in Plasma, or a
  record left without its widget) its settings are kept, the newest ten,
  in `removed-panels.json`. Panel Studio's page for a new panel lists them
  under "Removed panels" with Bring back, which makes the panel again with
  its settings, where it stood. A removal that is refused leaves the list
  as it was.
- Remove Panel removes every copy of a panel, and says why when it cannot:
  Plasma refused or did not answer, Arch Dock is stopped, the widget is a
  leftover of another panel, or Arch Dock no longer has the panel.
- Ended from System Monitor or with `pkill` (TERM or INT), Arch Dock is
  stopped only for now: it starts again as soon as a panel, a window change
  or the application menu asks for it. Quit still keeps it stopped until
  Start, the application menu or the next login. Every stop writes one
  plain line to the log, and old "stopped by a signal" records are
  ignored.
- A free panel says "Arch Dock is stopped" with a Start button (over its
  icons after 3 seconds), as does an edge panel with nothing on it (its
  sign; the words on hover; a click starts it). While Arch Dock is
  stopped, every panel's menu has Start Arch Dock.

**Why.** The audit's findings on panels lost after Plasma restarts (the
owner's free panel deleted on 31 October, also on 22 and 23 October, with
no copy kept), on a TERM that silently kept Arch Dock off for the whole
login, and on Remove Panel doing nothing when a panel had two copies.

**Checks.**

- Focused tests: `removed-panel-store-test`, `intentional-stop-test`,
  `panel-registry-test`, `configuration-backup-test`,
  `session-startup-runtime-test` and `panel-window-capability-test`
  (including the new `removalReportsAndRefusals`, which also checks that
  a waiting report is dropped when its time is up) passed.
  `barsFollowTheirRecords` had failed once on its own setup (the built-in
  Plasma Bottom panel stood on the bar's edge, where Bring back follows
  Panel Studio's rule); it now checks both cases.
- Private test desktops with the owner's 29 October free panel (icons and
  empty), the same steps as on 0.1.1-54 (`evidence/BREAKAGE-FIX-S1/`,
  before/after sheets in `sheets/`; final run `after-icons-final3`):

  | | 0.1.1-54 | Step 1 |
  |---|---|---|
  | Ended with TERM | stop kept, still off 10 s later | no stop kept, back within 10 s |
  | Quit, free panel with icons | says nothing | "Arch Dock is stopped" + Start over the icons |
  | Quit, empty panel | "Arch Dock service is unavailable" + Retry | "Arch Dock is stopped" + Start |
  | Two copies, Remove Panel | returns nothing, both stay | both go, settings kept |
  | Panel Studio, new panel page | nothing kept | "Removed panels" + Bring back; Bring back works |
  | Plasma restarted (as an update does), the widget removed with Plasma 20 s old | record gone within 5 s, nothing kept | record kept; widget put back within 120 s |
  | The widget removed with Plasma 171 s old | record gone within 5 s, nothing kept | record still there at 30 s, gone at 45 s, settings kept |

- Two more private desktops on the final code: an edge panel
  (`edge-steps-final`) whose whole Plasma panel Plasma removed soon after
  the start keeps its record and gets a new Plasma panel within 120 s, and
  with only Arch Dock's widget taken off it the widget is back within 10 s;
  and Arch Dock restarted next to a Plasma that had run for 3 minutes
  (`restart-next-to-old-plasma`) takes a removal 10 s later as the owner's
  (record gone after the two looks, settings kept), not as a start-up loss.
- `tests/run-plasma-lifecycle.sh` (not in CTest) passed in full on the
  final code (run 17, `evidence/BREAKAGE-FIX-S1/tests/lifecycle-17.log`)
  with `ARCHDOCK_LIFECYCLE_SKIP_OUTPUT_CHANGE=1`, including the new phases (a
  lost widget put back, Remove Panel with two copies, Bring back). It had
  not run in full since October: two checks were out of date since
  `36ce959` (Empty panels) and the template phases raced the built-in
  Bottom panel's start-up recovery; a build of the code before this step
  (`2d70771`) failed the same way, and the test now follows the current
  behaviour. Its "unsupported floating refusal" geometry check fails
  often: in 8 of the 14 runs that reached it on 25 and 26 November,
  including 2 of the 3 runs of the code before this step.
- Reviews (read-only): the first found four points beyond the bar test,
  the second four wording points, the third (the C++ logic of the
  follow-ups) three points (a start-up report given one early look only,
  no pause between retries of a failed step, a report outliving its panel)
  and one colour point (the stopped sign on an empty edge panel drawn
  white; sheet `S1-4-edge-panel-sign.png`), the fourth six points (the
  edge panels' recovery restarted by every look while a report waited; an
  edge panel that lost only Arch Dock's widget held for 10 minutes; a
  removal that itself starts Arch Dock taken for a start-up loss; a test
  that could leave a folder behind; and two claims of this report), the
  fifth four points (an unanswered Plasma query taken for "only the
  widget was removed"; a table row resting on a Plasma just under two
  minutes old, now done after a real Plasma restart; two wordings); all
  fixed. Of the 20 earlier findings about this step, 16 were fixed by
  then; the rest are covered above or listed below.
- Complete suite, one test at a time, on the final code but for the last
  small change (the "unanswered query" fix): 136 of 137 passed in 2953 s
  (`evidence/BREAKAGE-FIX-S1/suite-4.log`); the one failure is the known
  `path-travel-smoke` (the Star layout check, failing since 26 October, at
  13.84 as then). After that change the whole `panel-window-capability-test`
  (`tests/capability-final.log`), the focused tests, both private-desktop
  runs and the lifecycle test passed again. (An earlier full run had
  `wayland-hardening-scale125-test` fail with "containment-missing", as on
  28 October; it passed alone and in the final run.)

**Open points.**

- The lifecycle test's screen-off phase crashes the private test session's
  plasmashell 6.8.3 (segmentation fault in `KConfigGroup` under its script
  engine) with the code before this step as well (three core dumps on
  25 November). It needs its own look, since a screen going away on the
  owner's desktop may meet the same crash; the owner chose to have it
  looked into with Step 3.
- Panel numbers are given out again (lowest free number first): a widget
  left over from a removed panel can no longer remove a newer panel with
  the same number, but it shows that panel's icons until it is removed in
  Plasma's edit mode. Numbers that are never reused end this; the owner
  chose to have that done before the package (it touches profiles, so it
  comes with its own tests).
- An edge panel that keeps its icons while Arch Dock is stopped shows no
  notice over them; its menu has Start Arch Dock.
- The two looks 30 s apart and the put-back after a start are checked on
  the private desktops (free and edge panels), which are not part of the
  suite; the lifecycle test checks that the next Arch Dock puts back a
  widget whose removal report was lost when Arch Dock was stopped; the
  10-minute limit has a quick test with a shortened limit.
- On the test desktop only, Panel Studio showed "Panel settings could not
  be loaded (panel-not-found)" after the last panel was removed: that
  desktop's panel list has no built-in panels, which the owner's has.
