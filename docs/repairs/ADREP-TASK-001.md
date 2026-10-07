# ADREP-TASK-001: Panel Studio shows only what works, and the menu entry opens it

Repository: `/mnt/F/Arch Dock/` (origin `https://github.com/aivars11232/Arch-Dock`, branch `main`)
Package: `/mnt/F/Arch Dock LCL repairs/` (ADREP 1.0.0)
Base commit: `d18cbb8` (a work-in-progress commit `b06349f` was pushed when the
owner paused the work on 2026-10-07)

Evidence: `build-codex-adrep/evidence/ADREP-TASK-001/` (index in `INDEX.txt`).

## Report 1 - implementation

### What changed for the owner

- Arch Dock in the application menu opens Panel Studio every time: when Arch
  Dock is stopped, after Quit Arch Dock, while it runs, and after Studio was
  closed. Starts in the background (D-Bus, the applets, the KWin watcher, a
  plain `arch-dock`) still never open it.
- Panel Studio offers a setting only where it changes the selected panel, and
  hides a tab that would have nothing to change ("If the tab is empty, there's
  no need for that tab").
- Your free circular panel no longer shows Alignment, Edge, Dynamic, Visibility
  mode, Visible, Width, Height or Indicators, and no opening or closing
  settings anywhere. Its Layout page says that Radius and Layout scale set its
  size.
- Edge panels no longer show Dock layout, Layout scale or Panel padding, which
  their applet never used. Alignment, Edge, Dynamic, Visibility mode and the
  opening and closing mechanisms their theme can draw are still there.
- Appearance > Shape is gone: nothing drew it.
- Panel padding appears only on a skinned free panel, with one sentence: the
  space between the icons and the edge of the theme's artwork.
- Each Notifications item says in one sentence what appears on the dock; the
  tab is hidden where no item applies.
- No setting is on two pages: opening and closing, and panel rotation, are on
  Animations only; the icon style is chosen only on Icons > Appearance, and the
  Icon Styles tab is gone.
- Built-in and own panel presets list only the presets made for the selected
  panel ("if I make free panel why would I need to see horizontal and vertical
  panel presets"). The tabs for your own presets appear once they hold one.
- Free panels saved collapsed or hidden by an earlier version are shown open
  after the upgrade, with an automatic backup first.
- Icon path orientation now turns the icons of a free panel; it did nothing
  there before.

### Owner findings

| Finding | Status | How it was fixed | Proof |
|---|---|---|---|
| OF-01 | Closed | The menu entry ran `arch-dock` without `--settings`, so a second start only logged "already running". It now runs `arch-dock --settings`; startup metadata and its tests follow. | `06-of01-reproduction-installed-0.1.1-11.log` (reproduced first); `session-startup-runtime-test` 3 of 3 (`07-*`) and in the complete suite |
| OF-02 | Closed | Field availability fell through to "available" for unknown capabilities. Every capability now has one rule per host (`EditorCapability`, a `switch` the compiler checks) and unknown ones are never offered. | `everyEditorCapabilityHasAnAvailabilityRule`; `ownersFreeCircleOffersOnlyWhatWorks`; truth matrix forbidden lists |
| OF-03 | Closed | Width and Height reach no renderer of a free panel, which sizes itself from its layout; they are absent there, the Size tab is hidden, and the Layout page says what sets the size. | `ownersFreeCircleOffersOnlyWhatWorks`; `capture-comparison.txt` (Size: width, height before, tab not offered after) |
| OF-04 | Closed | Panel padding only where it draws (skinned free panels) with its sentence; rotation fields moved from Layout to Animations. | truth matrix (padding frame change on the skinned free row only; `homes`); `capture-comparison.txt` |
| OF-05 | Closed | Appearance > Shape was read by no renderer; it is no longer an editor field. | `ownersFreeCircleOffersOnlyWhatWorks`; truth matrix forbidden list |
| OF-06 | Closed | Dynamic is offered on edge panels only. | `ownersFreeCircleOffersOnlyWhatWorks`; truth matrix |
| OF-07 | Closed | The free host offers only Open (PD-01), with a migration of panels saved collapsed; the group is shown on Animations only, and its dependent settings are hidden while the mechanism is Open. | `presentation-mechanism-smoke`, `rendering-import-smoke`, `runtime-ui-interaction-smoke` (a free panel refuses to collapse); `configuration-upgrade-test` |
| OF-08 | Closed | Indicators only on edge panels whose content shows running applications; the tab is hidden otherwise. | truth matrix; `capture-comparison.txt` (Indicators not offered) |
| OF-09 | Closed | Each Notifications item has its sentence; the tab is hidden where no item applies. | `ownersFreeCircleOffersOnlyWhatWorks` (each item ends in a sentence) |
| OF-10 | Closed | The Icon Styles tab is removed; Icons > Appearance holds the one selector. | `tst_StudioNavigation.qml`; `studioPresetPagesBrowseWithoutChangingAnyPanel`; `ownersFreeCircleOffersOnlyWhatWorks` |

### Product decisions applied

- PD-01: free panels show no opening or closing settings and cannot collapse;
  saved collapsed ones are shown open. Edge panels offer the mechanisms their
  host and theme draw; with Open, only the mechanism and resting state show.
- PD-02: Width and Height are absent on free panels (no free layout draws from
  them). The one sentence about where size is set is on the Layout page,
  because the owner's later rule hides the empty Size tab.
- PD-03: Panel padding only on skinned free panels, with one sentence.
- PD-04: Appearance > Shape is offered nowhere, since nothing draws it.
- PD-05: one icon style selector, on Icons > Appearance.
- PD-06: Indicators only on edge panels with tasks or hybrid content.
- PD-07: each Notifications item only where it acts, with its sentence; the
  tab hides when none applies.
- PD-08: rotation on Animations; Layout keeps geometry only.
- PD-23: edge panels change only where this task names them: no Dock layout,
  Layout scale or Panel padding, and the mechanism settings hidden while the
  mechanism is Open; their session smokes pass.
- PD-26: the menu entry always shows Panel Studio; background starts never do.

What the truth matrix found, settled under the same rules: Visible did nothing
on a free panel (now edge panels only; free panels saved hidden are shown);
Circle and Ring, and Adaptive and Horizontal on a free panel, draw the same
(Dock layout offers one of each pair); a baked look stands its icons on its
artwork's own track (Dock layout hidden there but kept; layout angle and
rotation only on a closed track); a theme on an edge panel was checked against
the stored Dock layout its applet ignores (now against the row it draws).
Settings that belong to the panel but draw nothing in its present state are
hidden and keep their values; a setting the panel does not have is refused.

### Acceptance criteria

| # | Criterion (from the task document) | Result | Proof |
|---|---|---|---|
| 1 | The menu opens Panel Studio when stopped, running, after Studio was closed and after Quit; D-Bus activation, applets, the KWin watcher and login start do not | Met | `session-startup-runtime-test` (menu matrix; no Studio after D-Bus activation, repeated start, crash recovery, plain start) |
| 2 | Owner's free circular panel: no Alignment, no Dynamic or opening/closing on Behavior, no Opening and closing on Animations, no dead Size control, no Shape | Met | `ownersFreeCircleOffersOnlyWhatWorks`; `capture-comparison.txt`; `after-captures-final/` |
| 3 | Edge panels keep Alignment, Dynamic, Edge, Visibility mode and drawable mechanisms, and they work | Met | truth matrix edge rows (placement and visibility probes); `presentation-mechanism-smoke`; `folder-interaction-smoke` (edge moves) |
| 4 | Every visible field in the truth matrix changes the panel; a field without effect is absent | Met | `studioTruthMatrix` (21 combinations, `truth-matrix-table-offscreen.txt`); `studio-truth-matrix-smoke` (real graphics backend, `truth-matrix-table-native.txt`): 0 fields without effect |
| 5 | Unknown capabilities unavailable; a test fails for a capability without a rule | Met | `everyEditorCapabilityHasAnAvailabilityRule`; compiler-checked `switch` |
| 6 | No setting offered on two pages | Met | truth matrix check (c) and `homes`; `test_fieldPagesComeFromTheServer`; `tst_StudioNavigation.qml` |
| 7 | Indicators and Notifications only where they act; each item explains itself | Met | truth matrix forbidden lists; `ownersFreeCircleOffersOnlyWhatWorks` |
| 8 | Panel padding explained with a visible effect, or absent | Met | truth matrix: shown only on the skinned free row, frame changed |
| 9 | Free panels saved collapsed open safely after the upgrade | Met | `configuration-upgrade-test` (collapsed and hidden free panel shown open, other values kept, backup holds the original) |
| 10 | `docs/repairs/` holds this report; source exports exclude it | Met | this file; `source-exporter-test` |
| 11 | Complete suite passes in one serial run on the final source; no test weakened, skipped or retimed | Met | 115 of 115 in one serial run, 1307 s (`76-full-suite-final.log`); the one skipped capability function was skipped before this task and runs inside `rendering-import-smoke` |

### Tests

- Focused tests: `panel-window-capability-test` (truth matrix included) 73
  passed, 1 skipped (skipped before this task; it runs inside
  `rendering-import-smoke`);
  the 90 tests that need no private session passed; every changed session
  smoke passed on its own; `studio-truth-matrix-smoke` passed with all 21
  combinations, nothing deferred. One `runtime-ui-interaction-smoke` run failed
  on a D-Bus NoReply for a drop the backend had applied; the next run passed.
- Complete configured suite: 115 of 115 passed in one serial run on the final
  source, 1307 s. A first complete run (114 of 115) failed in
  `preset-audition-matrix-defaults`, whose refused-edit example used
  Appearance > Shape; it now uses the Theme of a baked look, and the run was
  repeated.
- New tests: `studioTruthMatrix` and `studioTruthMatrixHarnessIsTheApplet`
  (fixture `tests/data/studio-truth-matrix.json`, harness
  `tests/TruthMatrixPanel.qml`), `studio-truth-matrix-smoke`,
  `studioPresetListsMatchTheSelectedPanel`, `ownersFreeCircleOffersOnlyWhatWorks`,
  `everyEditorCapabilityHasAnAvailabilityRule`,
  `edgePanelThemesAreJudgedByTheDrawnLayout`, `test_fieldPagesComeFromTheServer`,
  the free-panel phase of `configuration-upgrade-test`, the `docs/repairs/` case
  of `source-exporter-test`, the menu matrix of `session-startup-runtime-test`.
- Contract changes cited to product decisions in the tests: resolver tests
  (PD-01: free host offers only Open, no host offers radial collapse); schema
  test (OF-07: presentation on Animations); capability tests (edge panel Dock
  layout, theme application, motion controls, wheel-input page, 3D page,
  preset pages, PD-04, PD-05); Studio navigation (PD-05);
  `presentation-mechanism-smoke`, `rendering-import-smoke`,
  `runtime-ui-interaction-smoke`, `folder-interaction-smoke`,
  `window-interaction-smoke` (PD-01, no Dock layout on edge panels);
  `preset-audition-matrix-defaults` and `studio-preview-contract-test`
  (PD-04, presets per panel).

### Files changed

- `data/org.archdock.ArchDock.desktop.in`: the menu entry opens Panel Studio.
- `src/model/PanelSettingsSchema.{h,cpp}`: capability per field, pages, Shape removed.
- `src/panel/PanelWindowSettings.cpp`: one rule per capability, inactive fields, layout choices.
- `src/panel/PanelWindowPresets.cpp`, `src/panel/PanelWindow.h`: theme candidates for Studio.
- `src/model/PanelCapabilityResolver.cpp`: free host open-only; edge themes judged by the drawn row.
- `src/PanelRegistry.cpp`: migration of free panels saved collapsed or hidden.
- `qml/runtime/SettingsPopup.qml`, `StudioForm.qml`, `StudioNavigation.js`: pages, tabs, presets, sentences.
- `qml/ArchDock/Rendering/LayoutEngine.js`: path orientation on free panels.
- `plasma-dock-widget/contents/ui/main.qml`, `SceneDefinition.js` (new): the applet's scene definition, shared with the test harness.
- `tools/prepare-arch-source.py`: `docs/repairs/` left out of exports.
- `CMakeLists.txt`: `studio-truth-matrix-smoke`.
- Tests: `tests/PanelWindowCapabilityTest.cpp`, `PanelCapabilityResolverTest.cpp`,
  `PanelSettingsSchemaTest.cpp`, `TruthMatrixPanel.qml`, `data/` (fixture and
  three glyphs), `tst_StudioNavigation.qml`, `tst_SettingsEditorModel.qml`,
  `tst_DockGeometry.qml`, `tst_PanelScene.qml`, `visibility-window.py`,
  `run-rendering-import-smoke.sh`, `run-session-startup-smoke.sh`,
  `run-configuration-upgrade-smoke.sh`, `run-preset-audition-matrix.sh`,
  `ValidateSessionStartup.cmake`, `ValidateStudioPreview.cmake`,
  `test_prepare_arch_source.py`.
- Documents: `README.md`, `docs/INSTALL.md`, `docs/KNOWN_LIMITATIONS.md`,
  `docs/CURRENT_STATE.md`, `CHANGELOG.md`, `docs/repairs/README.md`, this report.

### Open points and limits

- Observed, not changed (edge panels, PD-23): in a private session a collapsed
  bottom panel drew its reveal handle below its 92-pixel window, and resting
  the pointer on the window did not open it (hover trigger). Inferred cause:
  the scene, with its magnification headroom, is taller than the panel.
- Rotation speed and when the rotation runs act while the panel turns; the
  matrix proves them by the values the scene reads. ADREP-TASK-002 reworks
  that section.
- Shader-drawn effects (tint and glow on skinned and baked looks) and true 3D
  are drawn only with a real graphics backend; the offscreen matrix leaves
  them to `studio-truth-matrix-smoke`.
- Not installed: the PC still runs 0.1.1-11. The package with this work comes
  with ADREP-TASK-005, so the owner checks below wait for it.

### Owner checks for this task (only what automation cannot see)

1. Open Arch Dock from the application menu: Panel Studio opens, also after it
   was closed and after Quit Arch Dock.
2. Select your free circular panel: no Alignment, no Dynamic, no opening or
   closing settings, no Size control that does nothing, no Appearance Shape.
3. Icons: one place chooses the icon style; no Indicators on the free panel;
   each Notifications item explains itself.
