# ADREP-TASK-005: Readable, shapeable icons, travelling tiles and final package closure

Repository: `/mnt/F/Arch Dock/` (origin `https://github.com/aivars11232/Arch-Dock`, branch `main`)
Task package: `/mnt/F/Arch Dock LCL repairs/` (ADREP 1.0.0)
Base commit: `73cd1c3faaa8846ad0949a0e0369c1680f318110`
Evidence: `build-codex-adrep/evidence/ADREP-TASK-005/`.

**Reports 1 and 2: implementation, final suite, package, installed harness,
owner installation and pushed-source fresh-clone recheck verified. This
recheck record is synced before the ordered task-owned cleanup. Physical
owner acceptance remains separate.**

## Report 1 - implementation

### What changed for the owner

- Icons > Appearance keeps the single Style selector and adds saved Shape,
  Diameter, Logo size, outline thickness, body/outline/glow colours and the
  supported pedestal controls. Rounded, Square, Squircle, Circle, Hexagon and
  Diamond work for all six built-in styles. Style default preserves metallic
  Rounded, neon Diamond and Dark Orb Circle; an explicitly saved shape remains
  an override.
- Original application logos use continuous native Kirigami sizing. The
  default Logo size is 95% of the shape's inner area; the saved range is
  55–100%. Neon frames preserve the original glyph's colour. No replacement
  application artwork was invented.
- Dark Orb starts without its pedestal. Pedestal on/off, height and colour
  are supported options; Blue Pedestal and Red Pedestal revision 2 enable it.
  Unsupported or currently inactive controls are hidden and retain their saved
  values.
- All fifteen built-in icon presets retain Preview on Desktop, Apply as
  Active, Set as Default, Cancel, Revert, Restore Built-in Defaults and
  Duplicate to My Presets. A preset carries its complete bounded icon/tile
  appearance; applying one preserves panel geometry, content, icon size,
  spacing, per-entry overrides and ownership. My Icon Presets reload and apply.
- Icon Tiles retain custom shape, fill, border and opacity, and add the
  thirteen existing material textures, thickness, glyph offsets and glyph
  scale. Tiles and glyphs travel together; glyphs remain upright by default.
  Native 3D tiles have closed geometry with depth, bevel, material and
  elevation. Reset for the added tile controls preserves the original custom
  tile values.

### Owner findings

| Finding | Status | How it was fixed | Proof |
|---|---|---|---|
| OF-34 | Closed in automated checks | The resolver routes saved Shape into package-declared contour layers; native glyph size no longer snaps to standard icon sizes. | Baseline `008-*`, `009-*`; `050-*`, `054-*`, `180-final-native-readability.json`, `181-final-native-style-shape-grid.png` |
| OF-35 | Closed in automated checks | Rounded and Squircle reach the drawn contour for every built-in style. | `050-all-built-in-shape-pixels.log`; native style × shape captures in `native-final-icons-tiles/` |
| OF-36 | Closed in automated checks | Neon follows the selected shape and uses a frame that leaves the real glyph readable. | `075-neon-front-fill-receipt.json`, `080-style-parameter-pixels.log`, `180-*` |
| OF-37 | Closed in automated checks | Dark Orb pedestal layers are conditional and default off; saved controls and revised pedestal presets turn them on. | Package/parser tests, `080-*`, `160-*`, native Blue/Red Pedestal action captures |
| OF-38 | Closed in automated checks | Diameter, Logo size, outline and colours have schema/model persistence and visible controls beside Style. | `066-*`, `080-*`, `092-studio-appearance-options.log`, `190-final-native-studio-receipt.json` |
| OF-39 | Closed in automated checks | Icon presets project complete icon/tile defaults and capture them in saved custom presets, under existing transaction scope guards. | `160-*`, `163-all-preset-duplicates.log`, `164-*`, `165-*`, `176-all-preset-action-receipt.json` |
| OF-40 | Closed in automated checks | Original custom-tile shape/fill/border/opacity remain; reset of the new controls preserves them. | `116-*`, `143-tile-studio-transaction.log`, `178-final-native-icons-tiles.log`, `190-*` |
| OF-41 | Closed in automated checks | Texture, thickness and glyph placement extend the shared tile scene; orbit carries the tile and seated glyph together. | `119-*`, `143-*`, `178-*`; real-RHI `tileDepthMaterialsPlacementAndOrbitRenderUnderRhi` captures |
| OF-42 | Closed in automated checks | Native 3D panels expose and draw thickness, bevel, material and elevation, with capability-aware availability. | `178-*` six shapes/thirteen textures/thirteen materials/elevation/opacity/orbit; `190-*` solid-tile Studio row |
| OF-43 | Closed within PD-23 scope | Free panels were repaired first; shared renderer/schema changes retain horizontal/vertical host coverage. | Native Studio matrix includes edge-bottom/edge-left and free-horizontal/free-vertical; renderer parity and complete final suite `319-*` pass |

### Product decisions applied

- PD-05: one Style selector on Icons > Appearance, with its parameters.
- PD-20: every built-in style declares shape-following layers and its signature
  default through the package/resolver format. Shape also remains in the
  original custom-tile controls under OF-40; this is a narrow shared-field
  exception, not a second Style selector.
- PD-21: Dark Orb default pedestal off; Blue/Red Pedestal explicitly on.
- PD-22: tiles and their seated glyphs share path travel; upright orientation
  is retained in native 3D orbit.
- PD-23: horizontal and vertical panels retain their host behavior; no new edge
  panel features were added.

### Acceptance criteria

| # | Criterion | Result | Proof |
|---|---|---|---|
| 1 | Every Shape changes every built-in style; signature defaults retained | Met in automated checks | `050-*`, package tests, `181-*` native grid |
| 2 | Real logos readable in every style/shape with measured share and contrast | Met in automated checks | `180-*`: 108 cases, 216 paired captures; minima 0.936657 glyph share, 0.333748 RGB RMS contrast, 0.571429 strong-pixel share |
| 3 | Saved diameter, outline, colours and shape beside the single selector | Met in automated checks | `066-*`, `080-*`, `092-*`, `190-*` |
| 4 | Dark Orb default no pedestal; optional height/colour; pedestal presets show it | Met in automated checks | Package/parser tests, `080-*`, `160-*`, `176-*` |
| 5 | Every built-in preset action works visibly and persistently | Met in automated checks | `176-*`: 15 presets, 105 native action captures, 45 exact image pairs; actual Duplicate/reload/My cards in `163-*`, all fifteen UI action routes in `164-*`/`165-*` |
| 6 | Texture, thickness, placement and seated upright travel | Met in automated checks | `116-*`, `143-*`, `178-*` |
| 7 | Real 3D thickness, bevel, material and elevation drawn | Met in automated checks | `178-*` real RHI; `190-*` native Studio matrix |
| 8 | Existing custom tiles preserved | Met in automated checks | `143-*`, `178-*`, custom-tile Studio matrix |
| 9 | Complete configured suite in one final serial run | Met | `319-*`: 120/120 in 2332.19 s, one worker, exact freeze `317-*`; receipt `320-*` records internal skips and post-suite source equality |
| 10 | Canonical export, Release package, installed harness, 0.1.1-11 upgrade/removal | Met | Exports/post-suite regeneration verify; Release build `323-*`/`324-*` passes; package `326-*`; installed harness `330-*`/`331-*` passes 250 payload files, upgrade/recovery/removal |
| 11 | One password request, owner install, exact payload, backend/Studio/config preservation | Met | `333-*`, `336-*`, `owner-receipt.json`: one native password request, installed 0.1.1-13, 250 exact payload files, zero pacman alterations, new backend/menu/Studio startup, seven owner files byte-identical |
| 12 | Current-state/changelog/release block and owner checklist accurately cover all five tasks | Met for Report 1 | Current-state, changelog, release block and five-task checklist match verified implementation/install evidence; Git/recheck status remains explicitly pending |

### Tests

- Initial one-job Debug build passed (`182-full-final-source-build.log`) on
  the 552-file freeze `189-*`. Full one-job rebuild after the recovery repair
  also passed (`232-full-final-recovery-build.log`); all 552 final bytes/modes
  were frozen in `239-*`. Complete serial run `240-*` passed checks 1–93,
  including the repaired native runtime gate (64.78 s), then stopped at
  profile application (14.37 s; total 1432.26 s). The subsequent profile
  readiness correction and checksum-pinned recipe are frozen in `254-*`;
  two matching canonical exports verify in `253-*`. Run `256-*` passed
  119/120 in 2318.59 s, then failed installed startup; successful final run
  `319-*` below supersedes those historical inputs.
- Native icon/tile gate `178-*`: 3 C++ checks (7627 ms), 26 tile QML checks
  (2080 ms), 3 readability QML checks (11479 ms), with the native captures.
  This includes actual 3D depth, every shape, thirteen textures and materials,
  placement/elevation, upright orbit and unchanged opacity assertions.
- Native Studio matrix `179-*`: passed in 324.20 s; `190-*` records 23 cases,
  1137 tested field changes, zero failures and zero deferred changes.
- Preset native groups `174-*`/`175-*` passed in 148.26/153.48/144.72 s.
  `176-*` records all fifteen presets, 105 captured actions and 45 exact pixel
  comparisons. Cancel/Revert restore the saved image; Restore/Apply match the
  original built-in preview; My copies and saved/new-panel defaults persist.
  Native copies use Save Custom; actual Duplicate for every card is separately
  verified through the production library and all UI routes.
- Package/parser, model/schema, appearance and tile editors, saved preset
  scope/snapshots, Duplicate/My cards, browser/audition routes, geometry and
  module import focused checks pass; their incremental logs are retained.
- Complete configured suite: `319-*` passes 120/120 in 2332.19 s, one worker,
  on the exact 552-file freeze `317-*`. All source hashes/modes remain exact
  after the run; all four regenerated artifacts match (`320-*`, `322-*`).
  The ten internal Qt skips are recorded in `320-*`: nine cells have exact
  passing counterparts in the dedicated native/D-Bus gates of the same suite; the optional
  live reference source-asset archive/root cell remains unexecuted because
  those inputs were not supplied. No expired-context/TypeError marker occurs;
  static native 3D records one frame per 1000 ms. The earlier `193-*` run stopped at a
  native session timeout; a controlled cache discriminator established that
  the disposable outer prefix lacked its native hicolor index/cache. After
  supplying those artifacts, the unchanged native UI checks went from about
  68 s to 15 s (`195-*`/`200-*`/`201-*`), and the original native session gate
  passed (`203-*`). Temporary timing code was removed, with exact source-pin
  restoration before the final suite.
- Run `205-*` passed checks 1–83, then failed a real folder drop with a
  one-second D-Bus NoReply error; total 1281.89 s before stop. The same source,
  assertions and deadlines passed the traced runtime gate in 68.94 s
  (`207-*`). `208-*` records that initially unreproduced failure. The traced
  second complete run `209-*` passed checks 1–83 and then reproduced the same
  failure on a Dolphin launcher drop. `210-*` establishes the circular wait:
  Arch Dock was waiting for Plasma's native maximum-length query while Plasma
  was waiting for the drop mutation. The geometry reply took 959.947 ms;
  the mutation reply took 1004.488 ms, beyond the unchanged 1000 ms deadline.
- A candidate event-processing wait in the drop client passed its IPC
  fixture, but real private Plasma crashed in Qt drag delivery (`219-*`,
  private PID 684352, SIGSEGV; raw stack `221-*`). That approach was rejected
  and `DropBackend.h` restored byte-for-byte to the initial freeze. The
  corrected regression reproduces the actual recovery-first ordering, using
  the same native call helper as the backend: red `227-*`, green `229-*`
  (8 checks in 874 ms, including the original five delivery rows).
- The repair processes incoming backend calls only while background recovery
  awaits Plasma; ordinary native operations and synchronous drop delivery
  retain their original modes and deadlines. Recovery timers cannot reenter
  an active pass or report completion for an obsolete generation. Full build
  `232-*` and disposable install `233-*` pass. The unchanged native runtime
  gate and IPC fixture pass together (`234-*`: 2/2 CTests, 67.19 s; runtime
  gate 66.25 s). `235-*` records seven drop requests at at most 8.254 ms and
  no new dump in that passing run. No failed/interrupted/candidate run counts as final
  acceptance, and the real native gate is required in addition to the IPC
  fixture.
- Profile run `240-*` restored the interrupted transaction's exact native/free
  hosts and journal state, then cleanup was correctly rejected with
  `panel-lifecycle-active`. The event-processing recovery wait exposes an
  overlapping startup recovery that the original harness did not wait for.
  The unchanged guard protects mutations during that lifecycle. The harness
  now observes the existing `nativePanelRecoveryFinished` signal before its
  initial profile operations, after native shortcut activation, and before
  post-recovery cleanup. Original assertions, transaction deadlines and
  production guards remain unchanged. Failed apply/shortcuts logs are
  retained in `243-*`/`247-*`; focused checks are recorded in `250-*`.
- Run `256-*` passed 119/120, including native runtime (69.37 s), both
  profiles, and all three native five-preset groups (162.04/166.03/160.29 s),
  then failed installed startup after 6.44 s. Isolated tracing `261-*` and
  private bus capture `263-*` reproduce a three-second clock-panel fixture
  timeout with KIO local-socket errors. `260-*` is an invalid diagnostic:
  trace output contaminated the readiness parser; subsequent traces used a
  separate descriptor. All temporary diagnostic edits were restored exactly.
  A controlled unchanged-source run under a short disk-backed root succeeds
  (`265-*`); Qt's native local-socket probe rejects a 125-byte path and accepts
  a 101-byte path (`266-*`). The exact failed KIO syscall path was not captured,
  so that OS boundary is retained as environment evidence. Short paths alone
  are insufficient: the configured test still failed (`270-*`, 6.57 s).
  Backend activation/watcher registration precede native host recovery; the
  startup harness now observes the existing recovery-complete signal before
  its independent clock-panel fixture mutation. The configured startup test
  passes (`274-*`, 1/1, 42.43 s), preserving every original harness statement
  in order (`280-*`). The exact sole cause is not claimed proved. The existing
  `ARCHDOCK_TEST_TMPDIR` cache setting uses `/mnt/F/Arch Dock/build-r5`;
  assertions, deadlines and production startup code are unchanged. The new
  552-file source freeze is `278-*`; two canonical exports and source checks
  pass (`277-*`, `279-*`). Those were the inputs to historical run `281-*`;
  complete final run `319-*` supersedes them.
- Run `281-*` passed 79 checks, but native editor scene replacement emitted
  an expired-context warning from deferred `projectScenePoint` evaluation.
  The run was interrupted during check 80 (CTest exit 130); it is not final
  acceptance. Its exact private session logs are retained and leftover
  processes were stopped (`283-*`), preserving owner PIDs 941/1042.
  `PanelScene3D.qml` now owns its single-shot frame-update timer and stops it
  during destruction. The precise lifetime overlap is inferred from the
  warning, not claimed fully proved. Native rendering/editor, real input and
  path travel pass 3/3 in 289.34 s (`286-*`: 133.24/67.69/88.40 s) without that
  warning. The added static-renderer guard passed inside the native gate
  (`288-*`, 137.22 s): one completed frame in 1000 ms, below its four-frame
  maximum, proving the update does not continuously request frames. All prior
  assertions and deadlines remain. `296-*` records both native results;
  final all-target build/install `289-*`/`290-*` pass. Two canonical exports
  and recipe source checks pass (`293-*`, `295-*`); the 552-file final freeze
  is `294-*`; these are historical inputs superseded by the placement repair below.
- Run `297-*` passed checks 1–79, then failed the Top/Grid native folder
  interaction (766.67 s before stop). The retained private D-Bus trace proves
  a background placement rollback wrote Bottom after the foreground Top
  placement had committed (`298-*`, `299-*`). Background placement now pins
  the runtime persisted intent before and after each Plasma call and cancels
  an obsolete adapter before further writes or rollback. The foreground
  operation retains its original blocking mode; its placement result is
  preserved and a fresh recovery pass is scheduled. The shared native
  two-process regression fails before the post-call guard (`302-*`) and passes
  afterward (`304-*`: 9 checks, 856 ms). Full Debug build/install pass
  (`308-*`, `309-*`); all seven focused drop/folder/runtime/path/profile/
  shortcut/startup checks pass in 329.88 s (`310-*`, `311-*`). Original
  assertions, drop delivery, deadlines and lifecycle guards remain.
  The current 552-file freeze is `317-*`; two matching canonical exports and
  source verification pass (`316-*`, `318-*`). Complete serial run `319-*`
  passes 120/120 in 2332.19 s. Post-suite source pins and all four canonical
  artifacts remain identical; `322-*` verifies the regenerated source.
- Release package 0.1.1-13 builds with one compile job in 473.15 s
  (`323-*`, `324-*`). Package SHA256
  `997e8b82a09810fbc0026fd89d3ee99672591143b974f51551d76cc1b3286a13`,
  3002034 bytes; package metadata and the Release install manifest are
  retained in `326-*`. The installed-package harness passes in 295.37 s
  (`330-*`, `331-*`): 250 payload files, exact bytes/modes/resources/licenses,
  15+15 catalogs, startup with/without optional Quick3D, hidden-source native
  runtime/folder interaction, upgrade from 0.1.1-11, configuration recovery,
  and fresh/upgraded removal with user configuration preserved.
  The first harness run stopped because the source-hiding namespace also hid
  the repo-local Python environment and system Python lacked PySide6
  (`327-*`, `328-*`, `package-smoke-python-hidden-failed/`). The native
  namespace discriminator is red with that path and green with the exact
  copied PySide6 6.12.0 dependencies outside the hidden checkout (`329-*`).
  The passing rerun changes only the test dependency path: production source,
  harness source, package bytes and assertions remain unchanged. That owned
  external Python environment is disposable and must be removed at cleanup.
- Owner installation succeeds with the single native password request
  (`333-*`). Installed 0.1.1-13 has 250 byte/mode-exact payload files and
  `pacman -Qkk` reports zero altered files (`336-*`, `owner-receipt.json`).
  After installation, the exact old backend was stopped and the menu entry
  started the installed backend, PID 1087001. Native KWin observed its visible
  Panel Studio window; that task-opened window was closed and the temporary
  observer unloaded after proof. All seven backed-up owner configuration
  files remain byte-identical. The private backup and installation receipt
  are retained with mode 0600. No Arch Dock applet was present, so PlasmaShell
  was not refreshed and its owner PID 1042 was preserved. The exact original
  system crash dump from the rejected private client experiment was removed
  only after its original and retained-copy hashes matched; the compressed
  diagnostic copy remains in evidence with mode 0600.
- Contract expectation changes cite PD-20/21/22: Shape is a saved icon preset
  parameter; Hexagon is supported and Triangle remains invalid; the two
  pedestal presets expect revision 2 and pedestal on. The truth matrix permits
  only the OF-40 shared Shape field on Appearance and custom Tiles, preserving
  its single-home assertions for every other control. Original bounds and
  assertions are retained.

### Files changed

- Package format/catalog: `src/model/IconStyleDefinition.{h,cpp}`,
  `src/iconstyles/IconStylePackage.cpp`, all six built-in style manifests and
  production records, `data/icon-styles/builtin-icon-styles.json`.
- Durable panel settings/availability: `src/model/PanelDefinition.{h,cpp}`,
  `PanelSettingsSchema.cpp`, `src/panel/PanelWindowSettings.cpp`.
- Shared rendering: `IconStyleResolver.js`, `IconScene.qml`,
  `renderers/IconStyle2D.qml`, `renderers/IconTile.qml`,
  `PlatformGeometry.js`, `optional3d/IconStyle3D.qml`,
  `optional3d/PanelScene3D.qml`, all under `qml/ArchDock/Rendering/`.
- Editors: `qml/runtime/SettingsPopup.qml`, `StudioForm.qml`.
- Saved presets: `src/model/IconPresetDefinition.{h,cpp}`,
  `src/presets/PresetApplication.cpp`, Blue/Red Pedestal JSON.
- Tests: package/model/schema/capability/preset/renderer C++ tests; visual,
  tiles, geometry, audition/browser QML; native Studio fixture and existing
  rendering/preset/lifecycle/input harnesses; serial group registration in
  `CMakeLists.txt`.
- Runtime closure repair: `src/panel/PanelWindow.h`, `PanelWindowHelpers.h`,
  `PanelWindowNativePanels.cpp`, `PanelWindowNativePlacement.cpp`, `PanelWindowScreens.cpp` and
  `tests/DropDeliveryTest.cpp`: native background-recovery call dispatch and
  generation/reentry and superseded-placement protection, with recovery-first IPC regression. The
  drop helper itself retains its original bytes.
- Delivery/documents: `PKGBUILD` (pkgrel 13), `README.md`, `CHANGELOG.md`,
  `docs/INSTALL.md`, `ICON_STYLE_PACKAGE.md`, `PRESET_PACKAGE.md`, this report;
  `docs/CURRENT_STATE.md` and `docs/RELEASE_CHECKLIST.md` record the verified
  owner installation and the remaining Git/recheck closure steps.

### Open points and limits

- Scoped Git sync, fresh-clone recheck, Report 2 and task-owned cleanup remain.
- The recovery repair passes real private Plasma verification and the final
  complete suite. One private Plasma crash dump from the rejected client
  approach is retained (`242-*`, 27817485 compressed bytes, SHA256
  `883e4a1c987b091756031c714371c295169fc0823393e2d6acbf5c5f509b4503`,
  mode 0600). The rejected private crash did not affect the owner session.
  The owner backend has since been deliberately upgraded/restarted for
  0.1.1-13, with configuration preserved; owner PlasmaShell was not restarted.
- Native evidence uses disposable private Wayland/KWin/Plasma sessions and real
  RHI. Physical owner visual/input acceptance, second-monitor and other-GPU
  cells remain UNVERIFIED when the hardware is absent.
- Readability uses paired graphical RGB measurements on the actual Firefox,
  Dolphin and Kate SVGs at 52 px/default settings; it is not a WCAG text ratio
  or a guarantee for every application glyph or arbitrary owner palette.
- The original reference source ZIP/extracted root was not supplied. Its
  optional live-source-archive Qt row remains unexecuted; production asset
  hashes and canonical code export verification are separate checks.
- The inherited diagonal baked-ring Grid popup overlap note remains from
  Task 4; no successor or unrelated repair is claimed here.

### Final owner checklist for all five tasks (only what automation cannot see)

1. Task 1: on your desktop, open Arch Dock from the menu after closing Studio
   and after Quit Arch Dock. Confirm Studio comes forward and the offered
   controls make sense for your actual panel.
2. Task 2: on your free curved panel, use your physical wheel/touchpad and a
   held drag; confirm notch feel, direction and continuous travel, with the
   icons staying on their path.
3. Task 3: open your actual folders as Fan, Arc, Stack and Ring; confirm their
   fit, clearance, text readability and input feel on your monitor and content.
4. Task 4: inspect Glass/Floating Glass/Futuristic and your preferred materials;
   confirm platform off restores your flat look and front/behind presentation
   and clicking feel right on your GPU/display.
5. Task 5: inspect all six icon styles with your applications and preferred
   shapes/colours; confirm logo clarity, optional Dark Orb pedestal and tile
   depth/placement/orbit. Preview/apply/duplicate a preferred preset and inspect
   the result in My Icon Presets on your desktop.

## Report 2 - recheck

- Task source commit: `3a40575f917d307f6d41e8f2c5baa3397697a870`.
  Recheck round **1**; no implementation follow-up was required.
- After the non-force push and fetch, local HEAD equals `origin/main` and the
  tree is clean (`338-task-commit-sync.json`). The documentation-only record
  commit is titled `ADREP-TASK-005: record the recheck`; its actual hash and
  final fetched parity are retained in `352-recheck-record-sync.json` and the
  visible Report 2. Its own hash cannot be embedded in this document.
- Fresh clone of that pushed commit, without copied binaries: Debug/Quick3D
  configure, complete one-job build and disposable installation **pass**.
  Build: **1515.98 s** (`339-*`–`343-*`). The disposable prefix has the native
  hicolor index/cache; test dependencies and the short disk-backed runtime
  root match the verified setup. Original assertions/deadlines remain.
- All **52/52 planned focused/regression/native CTests pass serially**,
  **2011.93 s** raw CTest time (`344-*`, `345-*`). This includes native
  rendering, window/folder interaction and anchors, presentation, Studio,
  runtime UI/drop delivery, path travel, all fifteen icon presets,
  profile/shortcuts, resources and actual session startup.
- The separate native tile/readability branch **passes 1/1**, **25.30 s**
  (`346-*`, `347-*`). Real-RHI tile depth/material/elevation/orbit executes:
  3 C++ checks, 26 tile QML checks and 3 readability QML checks, with no
  internal skips in those groups. The two internal Qt skips in the 52-check
  run (live grouped windows and energy pixels) have passing counterparts in
  its dedicated native rendering gate.
- All **552 pinned non-Markdown source files** and **586 canonical source
  files** match the fresh clone byte-for-byte; Git executable intent matches
  (`340-*`, `350-*`). The existing local 0600 versus cloned 0644 non-executable
  source-catalog permission difference is preserved and is not misreported
  as identical permission bits. Both original and cloned trees remain clean.
  The installed Release package hash is unchanged.

### Every criterion and finding rechecked

| Criterion | Findings | Proof and result |
|---|---|---|
| 1: every shape/style; signature defaults | OF-34–36 | Fresh package/assets, visual style/shape and native renderer tests pass; canonical bytes match the captured implementation. |
| 2: readable real application logos | OF-34, OF-36 | Fresh native readability checks pass; retained 108-case/216-frame measured bounds remain exact for Firefox/Dolphin/Kate at the documented defaults. |
| 3: saved parameters beside one selector | OF-38 | Fresh schema/model, capability/editor and real native Studio tests pass. |
| 4: Dark Orb pedestal off; optional controls/presets | OF-37 | Fresh package/catalog, model, native preset actions and Studio tests pass. |
| 5: every preset action, persistence and My Presets | OF-39 | Fresh library/Duplicate, browser/audition and all three native icon groups pass; groups take 150.27/162.00/150.86 s. |
| 6: texture/thickness/placement and seated upright travel | OF-41 | Fresh tile/geometry tests, real-RHI tile branch and native path travel pass. |
| 7: drawn 3D thickness/bevel/material/elevation | OF-42 | Fresh real-RHI branch passes; native Studio capability/field matrix passes. |
| 8: original custom tiles preserved | OF-40 | Fresh tile reset/persistence and native Studio tests pass. |
| 9: complete configured suite on exact final source | OF-34–43 | Original final suite remains 120/120 in one serial run; 552 pins match pushed and fresh-cloned source, followed by 52/52 plus 1/1 fresh checks. |
| 10: export, package, installed harness/upgrade/removal | OF-34–43 | Canonical 586-file manifest matches clone; final package identity unchanged; retained Release/harness receipts prove 250 files and 0.1.1-11 upgrade/recovery/removal. |
| 11: one password, owner install, payload/backend/Studio | Final delivery | `333-*`, `336-*`, `owner-receipt.json`: installed 0.1.1-13, 250 matching files, zero pacman alterations, native menu/Studio startup, seven configuration files byte-identical. |
| 12: accurate five-task documents/checklist | Final delivery | Current-state, changelog, release checklist and five-task owner checklist map to retained raw evidence; physical acceptance stays pending. |

All **12/12 criteria** and **OF-34 through OF-43** retain passing automated
proof within PD-05/20/21/22/23 scope. No new gap or source repair was needed.
Fresh native Studio passes in **339.33 s**, runtime UI/drop in **65.86 s**,
path travel in **89.35 s**, profile apply/shortcuts in **20.14/18.27 s**, and
actual session startup in **40.63 s**. Physical visual/input, second-monitor
and other-GPU acceptance remain unexecuted. The original optional live
reference archive/root cell and inherited diagonal baked-ring Grid note
remain disclosed; no unrelated successor repair is claimed.

### Scope, processes, dump and ordered cleanup

- The **71 changed paths** from base `73cd1c3` exactly match the staged scope
  receipt `337-*`: Task 5 implementation, required recovery repairs/tests,
  packaging and delivery documents. No LCL files, tag, publication, force
  push or history rewrite are part of this task.
- Source/document/evidence audits pass (`340-*`, `349-*`, `350-*`). The
  process audit finds no running task build/test/backend or private
  Plasma/KWin session, including roots proven by the task's native logs
  (`351-*`). The installed owner backend is intentionally retained.
- The rejected private crash's exact original system dump was removed during
  the authorized installation after original/copy hash validation. Its
  **27817485-byte compressed evidence copy, mode 0600**, remains and its hash
  rechecks. Other system/owner dumps are untouched.
- Two inactive native roots, the fresh clone/prefix, obsolete exports/package
  staging and task Python/support scratch are queued for mandatory cleanup
  **after this record is pushed and Report 2 is given** (`348-*`, `351-*`).
  They are not claimed removed at this reporting step. Shared Debug build,
  passing/failed evidence and captures, final canonical four artifacts,
  Release package/install manifest, private dump copy and owner backups/receipt
  remain. `353-task-owned-cleanup-receipt.json` records actual removals and
  allocated disk impact; final Git/process state is recorded in `354-*`.
