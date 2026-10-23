# Full audit — 23 October 2026

Package 0.1.1-32 (`326a29a`). The owner asked: "Check literally every single
setting, layout and more. Report on errors, mistakes and so on." This is a
report: nothing below was fixed during the audit.

## How it was checked

- **The complete test suite**, all 133 tests one at a time (2550 s):
  129 passed, 4 failed. Log: `build-codex-adrep/evidence/FULL-AUDIT-2026-10-23/complete-suite.log`.
- **A settings audit** (new, on request only: `ARCHDOCK_SETTINGS_AUDIT_FILE`,
  `PanelWindowCapabilityTest::settingsAudit`). On a flat free ring, a 3D free
  ring and the bottom panel, every setting Panel Studio offers was given each
  of its other values: every choice of every list (every dock layout, folder
  opening, look, material, stand shape, easing…), the other state of every
  switch and a new value for every slider. After each one Arch Dock's
  settings were read back from disk. **642 values** were tried. Result:
  `settings-audit.json` in the same folder.
- **Source checks:** all 311 panel settings against the lists that save and
  load a panel; all 24 settings sections against the Studio pages; every
  Studio page visited on all three panels, watching for QML errors.
- **The owner's restart logs** (Arch Dock's own lines only) from today's
  updates.

## Errors and mistakes found

### 1. Out-of-date tests (my mistakes, not faults in Arch Dock)

These tests were not updated when the owner's decisions changed what Arch
Dock does; I ran only the quick tests then.

- `folder-interaction-smoke` and `folder-anchor-smoke` still open folders as
  Stack and Arc, and Along the dock on every panel. Since 22 October a
  Stack opens as a fan and an Arc along the dock, so the tests wait for a
  shape that never comes and fail.
- `rendering-import-smoke` (`realScenePixelsQualityAndFallback`, run only on
  the real graphics card) expects 3D icons on a 3D panel without the
  "3D icons" switch. Since 22 October a 3D panel's icons stay flat pictures
  until the switch is on.

Fix: bring the three tests up to date. Small.

### 2. Rare crashes inside Qt Quick 3D (a real risk to Plasma)

- `studio-truth-matrix-smoke` crashed in 2 of its last 7 runs inside Qt
  Quick 3D refreshing a 3D panel's live icon picture (`QSGRhiLayer::grab`).
  A 3D panel's icons still use live pictures; on a desktop the same crash
  would take Plasma down with it, rarely. The 3D icons on flat panels no
  longer use live pictures, and have not crashed since.
- A private test Plasma aborted once (15:29) inside Qt Quick 3D while a 3D
  view set itself up (an assertion in `QSSGRenderRoot::reindex`). Its test
  still passed; which view it was is not traced yet.

Fix: give the 3D panel's icons the same saved pictures the flat panels'
3D icons use, then run the truth matrix many times over; trace the
assertion.

### 3. Panel Studio hides all of Panels > 3D if its preview cannot draw 3D

Panels > 3D shows its settings only while the Studio's own preview is
drawing in 3D. Should the preview fail to load 3D for any reason, every 3D
setting vanishes from the Studio although the panel itself is still drawn
in 3D and uses them. On the owner's PC the preview draws 3D, so this does
not show today.

Fix: decide from the panel's renderer, not from the preview's state.

### 4. Updates left the panels bare (fixed today)

Found from the owner's report and logs, fixed in 0.1.1-31 and 0.1.1-32:
the update's `pkill` counted as Quit, and Retry could not undo it. A
session that still holds the old mark needs one press of Retry.

## What is right

- **No setting is lost.** All 642 values were accepted and came back
  unchanged after reloading from disk, on all three panels.
- **Every setting is saved and loaded.** All 311 panel settings are in both
  lists (`extensions` is saved whole on its own, by design). Yesterday's
  lost 3D icons switch was the last one missing.
- **Every choice has a plain name** in every list on all three panels.
- **Every settings section has its Studio page**; segments have their own
  editor. Visiting every page on all three panels gave no QML error.
- **A panel is removed only after Plasma deleted its widget**: the "remove"
  messages widgets sent during today's restarts could not delete the
  owner's Circle.
- Settings the audit found on no page are hidden by design until they
  apply: the 3D icons' depth, turn, relief and lighting until "3D icons" is
  on; tile fill, border and opacity until "Custom tile"; the tile material
  inside the Look list; the 3D panel settings in an off-screen test window
  that cannot draw 3D (see 3).

## Not covered by this audit

- Free text (names), pictures, the decoration catalogue and segment lists
  were not given other values.
- Settings that are inactive in a panel's default state (holograms while
  off, stands with Shape None, decoration colours without a decoration) were
  checked in the source, not tried while running.
- How each look and layout looks: no pictures were taken in this audit.
- Arch Dock's own bars were not in the settings audit.

## Still open from earlier

- The light line along a material ring's edge (owner circled it).
- Hologram icons in 3D on flat panels.
- Stands drawn as real 3D solids on flat panels.
- A size setting for the Grid and Möbius openings.
- The clock on the narrow console decoration touching its edge by 2–3 px.
- The known Qt garbage-collector crash in the capability test
  (`studioMotionAndDirectionRows`); that function passes alone.

## Suggested order

1. Update the three out-of-date tests (small).
2. Saved pictures for the 3D panel's icons, and trace the Qt Quick 3D
   assertion: the only findings that can crash Plasma.
3. Panels > 3D shown from the panel's renderer.
