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
