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

### Report 2

Fresh clone of `origin/main` at `9b7e55f`, built with one job (1794 s),
the step's tests one at a time: 6 of 6 passed in 581 s
(`removed-panel-store-test`, `intentional-stop-test`, `panel-registry-test`,
`configuration-backup-test`, `panel-window-capability-test` with the new
`removalReportsAndRefusals`, `session-startup-runtime-test`). All 993
committed files are the same in the clone and the workspace. Logs:
`evidence/BREAKAGE-FIX-S1/recheck/`.

## Step 2: light again

### Report 1

**What changed.**

- AI-made "Modelled" 3D icons are a choice again, off by default. 3D icons
  are made at once from their picture (Auto), as before 0.1.1-53;
  Modelled is offered per panel as "Modelled (AI, made in the
  background)". Panels saved by 0.1.1-53 or -54 go back to Auto and to
  Medium quality once: the panel, both saved copies of the flat look it
  remembers, and saved profiles (after a backup). If a backup cannot be
  taken, the panels load as saved instead of not at all; if an interrupted
  profile apply waits at the first start, this happens once it is put
  right. Chosen again later, Modelled or High stays.
- The AI maker runs apart from Plasma, in a place of its own under the
  user's background group, at the lowest priority and within a memory
  limit the system itself keeps, so running out of memory ends the maker
  and never Plasma. It waits for another time when less than 4.5 GB is
  free, stops at its first failure of its own instead of trying every
  icon, and removes a picture it cannot read only when it is the dock's
  own copy.
- Holograms and panel decorations move on one shared clock: 15 steps a
  second while the pointer is away from every panel, 30 while it is over
  one (the owner's choice, "Calmer when idle"; it was 30 always). Each decoration
  keeps its own running clock on it, so a new speed changes only its pace.
- Holograms rest while their panel cannot be seen: covered by windows (a
  free panel by solid windows on the desktop in use that hold all of it,
  windows side by side and Plasma's own panels included; an edge panel or
  a bar only by a full-screen window in front), auto-hidden, or in a bar
  of Arch Dock's own that has hidden itself. A see-through or minimised
  window, one on another desktop, and Show Desktop cover nothing. Only the
  motion rests: the panel stays drawn and answers at once, and folders
  and previews stay as they are. After KWin restarts, its windows are read
  anew.
- A 3D panel works out where its icons are drawn only when something
  moved (and once a second to be safe), not after every frame. Opening or
  closing a window builds only that window's icon; the flat and 3D icons
  share one list.
- Panel Studio reads the status readings only while a page or the preview
  shows them, never reloads its settings for a reading, and no longer
  writes over what is being typed; its own readings never make the docks
  refresh, and its preview still follows applications started, closed or
  pinned.
- 3D icons on flat panels use two samples a pixel instead of four.

**Why.** The audit's findings on the lag since 0.1.1-53: every 3D icon an
AI object made with all cores, holograms and decorations redrawing 30
times a second even when hidden or covered, every icon rebuilt on any
window change, 3D panels supersampled by default, and Panel Studio waking
Arch Dock every 2 seconds.

**Measured** (private test desktop, the owner's 29 October ring with three
icons, the pointer away; same day, same steps; one core = 100 %):

| | before Step 2 | Step 2 |
|---|---|---|
| Plasma idle | 13.8 %, 757 MB | **6.5 %**, 773 MB |
| Panel Studio open | Plasma 14.7 % + Arch Dock 12.3 % | **6.5 % + 0.0 %** |
| Five windows opened and closed | 5.0 s of processor time | **2.38 s** |
| Arch Dock with Studio open | 661 MB | 645 MB |
| Ring covered by a maximised window | (as idle) | **1.6 %** |
| Under Show Desktop / covered again / uncovered | | 6.4 % / 1.5 % / 6.4 % |
| Ring without holograms (earlier today) | | 1.2 % |

(Yesterday evening's "before" read 20.6 % and 692 MB on the same steps:
the computer was in another state then, so only same-day pairs are
compared.)

**Pictures.** `evidence/BREAKAGE-FIX-S2/calmer-rate-clips/
calmer-rate-before-after.webm` (holograms, 30 against 15 steps a second,
TEST VIEW) and `sheets/quality-before-after.png` (the owner's ring as saved, high
against medium quality, enlarged: the same look; fine lines inside the
surface a touch less smooth). Covered and Show Desktop: `cover-pause-final/`.

**Checks.**

- An independent review (six readers by area, each finding checked by a
  sceptic told to refute it): 20 findings, 18 confirmed, all fixed but one
  older one (below); `evidence/BREAKAGE-FIX-S2/review/review-outcome.md`.
- Focused tests, new and changed: window covering (focus, side by side,
  Plasma's panels, Show Desktop), holograms resting, moved and doubled
  entries keeping their delegates and 3D nodes, the light defaults (panels,
  both flat-look copies, profiles, once only, waiting for recovery), the
  AI maker's pictures and memory floor, Studio status sampling, status
  readings over the preview.
- The complete suite, serially (2911 s): 136 of 138 passed.
  `path-travel-smoke` failed as it has since October (the Star layout's
  travel check: an entry leaves the outline between slots, 11.9 against
  the 2.5 allowed; Step 1 reported the same). `configuration-upgrade-test`
  failed: the saved profiles' once-only step wrote its mark into a
  configuration Arch Dock could not read (a newer version's), which must
  stay byte for byte as it was. Fixed (that step does not run while the
  saved panels cannot be read); it, `profile-manager-test`,
  `panel-registry-test`, `panel-window-capability-test` and the start-up
  tests passed again (`suite-1.log`, `suite-1-rerun.log`).

**Not done in Step 2, and why.**

- With holograms showing, Plasma idles at 6.5 %, above the 5 % aim. The
  rest is drawing the 3D ring 15 times a second for them (about 0.5 ms of
  script a step; the rest is the drawing). Two ways under it: 10 steps a
  second while the pointer is away (a look change, the owner's call), or cheaper
  drawing per frame in the memory and graphics task the owner gave on 26
  November.
- One drawing per 3D icon on flat panels: measured on the owner's panel drawn
  flat, each 3D icon's own view costs about 14 MB and 0.2 % of a core
  (675 against 633 MB for three icons). It is mostly memory, so it moves
  to the memory and graphics task.
- A shared display copy of AI-made icons for the graphics card: matters
  only when Modelled is chosen; also in that task.
- Older, not part of this plan: moving a hologram Speed slider makes the
  holograms jump to another phase while it moves.
- Known limits: a window a program makes see-through itself (KWin does not
  tell) counts as solid; the owner's own saved panel presets and profiles
  exported earlier keep High until saved again; Modelled needs about
  4.5 GB free to start.

### Report 2

Fresh clone of `origin/main` at `3598b1b`, built with one job (1772 s),
the step's tests one at a time: 16 of 16 passed in 765 s
(`icon-model-test`, `system-status-test`, `profile-store-test`,
`profile-manager-test`, `panel-registry-test`, `window-preview-model-test`,
`panel-window-capability-test`, `studio-scroll-test`, `panel-scene-test`,
`icon-scene-test`, `dock-entry-test`, `rendering-import-smoke` with the 3D
checks under a private KWin, `settings-editor-model-test`,
`configuration-upgrade-test`, `core-accessibility-test`,
`icon-helper-pictures-test`). All 995 committed files are the same in the
clone and the workspace. The new check that 3D icons follow their rows
(moved and doubled entries) was also run alone with the private KWin's
graphics and passed (`rows-rhi/`). Logs: `evidence/BREAKAGE-FIX-S2/recheck/`.

## Step 3: stop the crashes

### Report 1

**What was wrong, now proven.**

- **Plasma's four stops on the owner's desktop** (23 and 28 October, each
  on returning from the second account). When a screen comes back, Plasma
  makes new windows for its desktop and panels, and Arch Dock's 3D views
  went along into them. Qt Quick 3D numbers a scene's parts per window and
  keeps a part's old number when it happens to match the new window's
  count, then reads past the end of a list. A new test does exactly this on
  purpose. On the old code it stops Plasma's way in five seconds, with the
  same stack as the owner's four crash records, down to the same places in
  Qt (`evidence/BREAKAGE-FIX-S3/carried-before`). On the new code it passes
  at all 31 counts tried (`carried-after`).
- **Arch Dock's crash in Panel Studio's tests** (once on the owner's desktop,
  8 October): a 3D texture drawn from a live picture went on refreshing the
  picture after it was freed. The Studio test run with freed memory
  overwritten crashed 3 times out of 3 on the old code.
- **Plasma 6.8.3 stopping when a screen is switched off** (on test desktops
  only, three times): with the screen gone, Plasma reads a panel's stored
  settings from nowhere and stops. Arch Dock's own scripts did that read
  while moving the panel to the other screen.

**What changed.**

- A 3D view never goes along into another window. The moment a panel's dock
  leaves its window (Plasma replacing its windows, or an edge panel's dock
  moving into Arch Dock's window over the panel), its 3D views go. They are
  made anew in the new window a moment later. A window that is only hidden
  and shown again keeps them.
- Switching 3D off lets go in two steps. The 3D scene first lets go of every
  live picture it draws from, and the flat look shows; the scene itself
  goes two frames later. Switched straight back on, it stays.
- When any 3D scene goes, every texture drawn from a live picture lets go of
  it first. Before, only the water mirror's and the icons' did.
- No picture is taken of an icon that is not finished, not in a shown window,
  or being taken apart, and a late answer is dropped. A bar's tone is not
  measured while its 3D lets go or changes window, or as it goes.
- Panel Studio and Icon Properties draw on Arch Dock's own thread.
- A flat 3D hologram icon's glow stands beside its solid, not inside it,
  with the same picture. A test now catches a return of the old way, and
  also a 3D file that no longer loads (an extra brace once removed every
  flat 3D icon for five days).
- Before Arch Dock reads or changes one of a Plasma panel's stored settings,
  it asks whether the panel's screen is there, the way Plasma looks it up.
  A panel whose screen went away is first moved to the screen it falls back
  to, as before, but before anything stored is read. A panel is never moved
  to a screen Plasma does not have. With no screen to go to, nothing is read
  or changed, and the panel waits for its screen. Arch Dock writes one plain
  line to its log when it moves a panel off a screen that is gone.

**What looks different.**

- After login, a Plasma restart, a screen coming back or the return from the
  second account, a 3D panel shows its flat look for a moment while its 3D is
  made in the new window, and its 3D icons are made again. It already looks
  this way while 3D first loads.
- When Panel Studio changes Stands, Holograms or Visibility of an
  always-shown edge panel, its 3D shows flat for a moment while the dock
  moves.
- Nothing else is meant to change. The glow, before and after, is the same
  picture to the pixel (0 of 108,000 pixels differ):
  `evidence/BREAKAGE-FIX-S3/glow/glow-before-after.png` (TEST VIEW).

**Measured** (private test desktops; "freed memory overwritten" makes any use
of freed memory crash at once).

| | before Step 3 | Step 3 |
|---|---|---|
| A 3D scene carried into a new window (Plasma's stop) | stops at count 1 | 31 of 31 counts pass |
| A 3D view going along into a new window | yes | never |
| Studio test, freed memory overwritten | 3 of 3 crashed | 5 of 5 pass drawn on a separate thread (as Plasma draws); 2 of 2 drawn on the program's own thread (as Arch Dock now draws Studio) |
| Second screen switched off and on, owner's mix plus two edge panels | (not run: Plasma crashed first in the screen-off test) | 20 of 20 returns, no crash, Plasma's memory steady at about 900 MB |
| Plasma's screen-off test (lifecycle) | 3 of 3 crashed | 4 of 4 pass, no crash |
| An edge panel's dock moved 20 times (Holograms off and on) | survived 20 of 20 (25 Nov) | 20 of 20 |
| Panel Studio opened and closed 30 times, freed memory overwritten | | no crash, the same backend all along |
| Arch Dock's answers on the bus, Studio closed / open | | 7.5 / 7.4 ms (slowest 8.1 / 9.0 ms) |
| Qt drawing thread in Arch Dock with Studio open | yes | none |
| Plasma idle, the owner's ring as saved (old and new QML, same backend, three runs each, alternating) | 6.6 / 5.9 / 5.1 % of a core (mean 5.9), 763 MB | 6.9 / 5.7 / 5.9 % (mean 6.2), 755 MB: the same within run-to-run noise |
| Five windows opened and closed | 2.23 s of processor time (mean) | 2.25 s |
| Arch Dock with Studio open | 630-635 MB, 0.0 % of a core | 630-632 MB, 0.0 % |

**Checks.**

- An independent review (five readers by area, each area's findings checked
  by a sceptic told to refute them): 11 findings, 4 confirmed and fixed,
  7 refuted with reasons. A second check of the four fixes, with every
  Plasma script swept again, found nothing more
  (`evidence/BREAKAGE-FIX-S3/review/review-outcome.md`).
- New and changed tests: the glow and the 3D file loading; every live 3D
  texture asking for its window; 3D views made anew in a new window, never
  carried; the two-step let-go; the carried-scene reproducer; a flat 3D
  icon's view made anew in a new window, keeping its solid and picture; no
  picture of an icon going away; the screen check in every Plasma script
  that touches stored settings (and a scan of the sources for any that does
  not); moving a panel off a gone screen first; never to a screen Plasma
  lacks; visibility left alone while the screen is gone.
- The screen-off lifecycle test had a race of its own that failed its
  "unsupported floating" part on the old code too (two baseline runs, six
  of the runs in Step 1). The test now waits for Arch Dock to finish a
  panel's placement before stopping it.
- Complete suite, run once one test at a time: 138 of 139 passed in 52
  minutes. The one failure is the Star layout's travel check
  (`path-travel-smoke`, "an entry left the outline between slots", 14.1;
  it failed the same way before this step, 11.9 and 13.8)
  (`evidence/BREAKAGE-FIX-S3/suite-1.log`).

**Not done, and why.**

- The plan said 3D panels would use saved icon pictures instead of live
  layers. That would change how 3D panels look: their icons' tiles would no
  longer follow hover, press, active and urgent at once. The crash it was
  meant to stop no longer shows with the releases above (7 clean runs where
  3 of 3 crashed before). So the live pictures stay, and this is left to the
  owner (below).
- Known limits: a picture an icon is taking at the very moment its window
  changes is not taken again in the new window (an old gap, narrower now:
  on 3D panels the icons' pictures are taken afresh after a window change).
  Grabbing an icon that is being destroyed in the same few milliseconds is
  a race inside Qt that Arch Dock cannot close from QML; no crash record
  points to it.
- Plasma's own crash when a script reads a windowless panel's settings is a
  Plasma 6.8.3 bug; reporting it to KDE needs the owner's account.

**For the owner to decide.**

1. Saved icon pictures on 3D panels: keep the live pictures (no look change;
   the crash no longer shows), or switch to saved pictures and accept that
   the tiles follow hover and press a frame or two late? Recommended: keep
   them; switch only if a crash of this kind shows up again.
2. After 0.1.1-55: switch to the second account and back about 11 times with
   a 3D panel or 3D icons on, one switch after another on a quiet desktop,
   and say whether the desktop ever vanishes. No test desktop can switch
   accounts, so this is the real proof on the owner's machine.
