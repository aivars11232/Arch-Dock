# ADREP-TASK-004: Textured looks, platform presentation and editable native materials

Repository: `/mnt/F/Arch Dock/`, origin `https://github.com/aivars11232/Arch-Dock`, branch `main`.
Package: `/mnt/F/Arch Dock LCL repairs/`, ADREP 1.0.0.
Base commit: `617bd96e8972111c3277c542be89e018e7cfd2f6`.
Evidence: `build-codex-adrep/evidence/ADREP-TASK-004/`.

## Report 1 - implementation

### What changed for the owner

- All thirteen procedural looks have distinct original bundled textures. Glass has translucency, frost, highlights and a refraction-like gradient; Floating Glass adds elevation and shadow; Futuristic has seams and circuit lights. Crystal has facets, Metallic brushed grain, Organic soft grain, Neon tubes, Plasma energy, Minimal satin, Lime cells, and Platform/Plate/Pedestal solid bodies. Tint preserves texture and opacity scales the material. Sparkle is offered only for Crystal and Plasma and defaults to zero.
- Flat looks and Platform looks are separate in Studio. Loading a platform remembers the complete previous flat appearance, layout and visual motion; turning Platform presentation off restores it exactly through draft, Apply, Cancel and reopen. Legacy panels without a snapshot return to their retained procedural appearance.
- A procedural Theme control cannot change a look's own skinned/baked/native material. It is absent with a reason and a route to Flat looks. Native Platform colour, Material and Texture controls change the drawn 3D surface live; Reset material restores the look's own defaults.
- Blue Ring, Steel Octagon and Orange Arc perspective artwork hides covered rear entries beneath its body and rim. Actual entry pointer/drop targets use the exposed glyph region. Numerical platforms refine picks against their actual triangles. Wheel and continuous travel keep entries moving behind the platform and back in front.
- Bend folds the rear half of numerical platforms up or down by up to 90 degrees, with icon feet anchored to the same surface. Fixed perspective artwork explains why it cannot fold. Items, Whole panel and Both keep their existing motion behavior.
- Static looks have no periodic paint timer; animated Plasma/Futuristic overlays run at 8 Hz only while visible. An immutable settings-schema index removes repeated linear scans from Studio's complete-flat projection path without changing validation or descriptor order.
- After the overnight Qt 6.12 update, public Camera mapping replaced View3D mapping that recalculates a shared render projection. Physical wheel notches also now keep their angle steps when Wayland supplies both angle and pixel deltas; synthesized smooth input retains pixels. A small Qt-only window observer exposes the missing native event-source metadata to the existing ScrollInput and its existing reducers.

### Owner findings

All ten findings have passing automated repair evidence. Physical visual and interaction acceptance remains for the owner after Task 5 installs the package.

| Finding | Status | Repair and proof |
|---|---|---|
| OF-24 | Closed in automated checks | Thirteen textured materials; controlled declared sparkle. `panel-materials-test`: all 78 common-tint pairs differ by mean absolute pixel difference >1; every texture variance >4. Thirteen matched before/after pairs in `materials/comparison-qt612.html`. |
| OF-25 | Closed in automated checks | Inapplicable procedural controls are absent with a reason; native material controls target the active surface. Backend/editor transactions, native Studio truth/input and native material/reset pixel checks in the final suite. |
| OF-26 | Closed in automated checks | Translucent frosted Glass and elevated Floating Glass. Matched owner-circle captures and texture/tint/opacity checks; common-tint variance 27.89 and 22.23. |
| OF-27 | Closed in automated checks | Futuristic seams, circuit/light lines and slow pulses. Original SVG, matched captures, visible/hidden animation assertions and CPU measurements. |
| OF-28 | Closed in automated checks | Tint retains texture; opacity scales material and detached foreground. All thirteen material rows, baked zero-opacity visible-input case and native colour/texture/reset pixels. |
| OF-29 | Closed in automated checks | Platform candidates preserve item/panel/both motion and travel. Backend/model Apply/Cancel/reopen invariants, perspective wheel/continuous cases, native renderer and path-travel gates. |
| OF-30 | Closed in automated checks | Body/rim and numerical triangles hide covered rear glyphs; exposed glyph input follows travel. Three perspective capture sequences, native Cyan/Orange/generated rows, exact triangle picks and real folder-anchor input. |
| OF-31 | Closed in automated checks | Numerical fold changes positions, normals and feet together; fixed artwork gives an unsupported explanation. Geometry tests and native pixel/projection/triangle checks across twelve look/layout rows. |
| OF-32 | Closed in automated checks | Separate groups and exact complete-flat restore; selected motion target survives platform changes. Backend snapshot restore, editor draft/Cancel/reopen, native Studio interactions and Task 2 regressions. |
| OF-33 | Closed in automated checks | Native colour/material/texture editing and own-material Reset. Thirteen native material captures; magenta, None/Organic texture and exact own-pixel Reset; live native Studio fields. |

### Product decisions applied

- PD-17: separate flat/platform groups and exact complete-flat restoration through the existing settings transaction.
- PD-18: wheel and continuous entry travel keep running on every platform; actual foreground masks/triangles limit input to exposed entries.
- PD-19: thirteen original, distinct, tinted textures with working opacity and declared, default-off sparkle.
- PD-23: edge-panel behavior retained and included in the native input/Studio regression gates.
- PD-25: Items/Whole panel/Both and their existing speeds/trigger survive look changes. Fixed open perspective arcs retain their unsupported whole-rotation explanation.
- PD-16 preserved: physical notches use angle steps, smooth sources use pixels. The Qt 6.12 regression changes metadata routing, not movement expectations, thresholds or deadlines.

### Acceptance criteria

| # | Criterion | Result | Proof |
|---|---|---|---|
| 1 | Distinct textured materials; Glass reads as glass and Futuristic has future-tech detail. | Met in automated checks | 21 material cases; 78 distinct pairs; per-look variance >4; thirteen same-Qt before/after pairs. Owner visual judgement pending. |
| 2 | Apply changes the chosen look, or the control is absent with a reason. | Met | Backend/editor transactions, native Studio truth and runtime input; native material/reset RHI pixels. |
| 3 | Colour tints and opacity work on every look and keep texture. | Met | Thirteen tint/opacity rows, baked foreground opacity/input, native colour/texture/reset. |
| 4 | Every platform keeps motion/travel, behind/front ordering and visible-only entry input. | Met | Perspective capture sequences; native Cyan/Orange/generated matrix; real path-travel and folder-anchor gates. |
| 5 | Separate flat/platform groups; platform off restores the exact previous flat look. | Met | Backend complete-flat snapshot, editor draft/Cancel/reopen and real native Studio interaction. |
| 6 | Native colour/material/texture edits apply. | Met | Native GPU material/texture/reset captures and live Studio controls. |
| 7 | Bend works on supported shapes; unsupported looks explain why. | Met | 33 geometry cases, twelve native look/layout rows and Studio explanation. |
| 8 | Platform rotation follows Continuous motion moves. | Met | Preserved motion candidates/model contracts, native Studio and perspective/native travel. |
| 9 | Static/hidden CPU and Studio latency meet the contract. | Met | `148-final-material-cpu.json`, `205-full-final-latency-limits.log`, final-suite resource log. |
| 10 | Complete configured suite passes once serially on final source. | Met | `203-full-suite-final.log`; 550-file hash/mode pin and post-run comparison. |

### Tests and performance

- Final complete one-job Debug build: `200-single-shot-build.log`, exit 0, Qt 6.12.0.
- Prior input-focused batch: 8/8 CTests in 11.94 s (`147-final-input-focused.log`): software/missing-module renderer, dock geometry, Studio scrolling, folder expansion, renderer parity, host-neutral policy and module import.
- Prior native import plus path travel: 2/2 in 213.59 s (`146-native-wheel-platforms.log`), including 24 native renderer cases. Real folder input: passed in 77.83 s (`144-native-wheel-folder-after.log`); native traces prove one-child notch movement despite the seat being labelled a touchpad.
- Intermediate mask-disposal checks: 4/4 CTests in 136.35 s (`162-mask-disposal-native.log`), including all 24 native renderer cases. Normal theme-switching RSS growth was 33652 KiB (32.86 MiB), below the unchanged 128 MiB cap; hit-region, skin and baked tests also passed.
- Prior native folder/content gate: 1/1 in 79.78 s (`167-content-probe-native.log`); 100 actual D-Bus updates coalesce into one published revision, with latest badges/progress drawn on both panels. Folder, wheel, segment, conceal/reveal and disconnect assertions pass.
- Prior affected native batch: 9/9 CTests in 469.10 s (`179-shared-pixmap-native.log`): asset contract, geometry-hit-region, baked platform, renderer parity, native rendering/import, folder interaction, all 90 folder anchors, runtime UI and path travel. Native RHI matrix: 24 cases; repeated-theme RSS +34064 KiB (33.27 MiB), below the unchanged 128 MiB cap.
- Final runtime readiness gate: 1/1 in 63.93 s (`187-runtime-readiness-native.log`), including both owner free-9/free-4 wheel cases after the fresh-sample repair.
- Final reply-lifetime native batch: 2/2 in 192.83 s (`201-single-shot-native.log`); all 24 native graphics cases, repeated-theme RSS -2384 KiB, complete real runtime input and owner wheel cases pass.
- Complete configured suite: 118/118 CTests, zero failures in one serial run on final source, 1607.15 s. Required native Studio, runtime, window/folder/anchor, resource and renderer gates are included. There are ten internal Qt SKIP rows in initial offscreen/no-fixture invocations: overlay D-Bus, live source archive, live grouped windows, energy pixels and six live popup-placement rows. Nine are exercised in dedicated D-Bus/native gates in this run; the live source archive fixture was not supplied here and remains for Task 5 packaging. No CTest was skipped or disabled.
- Full-run Studio medians: edit 37.46 ms, page 32.14 ms, Apply 12.05 ms; original limits 53.76/41.16/24.84 ms pass. Zero theme-package reads each.
- Fifteen-phase CPU probe: all eleven static and both hidden animated materials 0.0% of one core; visible Plasma/Futuristic 3.4%/3.6%. Five-second samples after 4.5 s settling, using the retained /proc tick protocol (`CPU_PROTOCOL.txt`).
- Final-suite resources: eight audition/cancel cycles: backend RSS 65776 to 77140 KiB, +11364 KiB (11.10 MiB), below the unchanged 64 MiB bound; zero stale hosts; backend settled idle CPU 0.0% of one core over five seconds (`resources-full-single-shot/session.log`). Native theme-switching PlasmaShell: 632644 to 687596 KiB, +54952 KiB (53.66 MiB), below the unchanged 128 MiB bound.
- Native sampled frame intervals/render CPU: Cyan/Orange/generated circle intervals 12.7573/12.5426/19.581 ms, rendering CPU 0.68806/0.453979/1.29687 ms (`203-full-suite-final.log`, NATIVE_FRAME_TIME).. These are bounded private-compositor samples, not physical desktop cadence acceptance.
- Contract expectation changes explicitly implement PD-17/18/19: complete-flat restoration, native material controls, thirteen texture cases, extra baked body layers and exposed-only input. Original numeric variance, response, resource and latency bounds remain intact. The new wheel-source regression sends actual Qt QWheelEvents, including physical and synthesized smooth events with NoScrollPhase and an opaque Wayland touchpad seat.

### Root-cause evidence and retained diagnosis

The reproduced Apply case was a procedural appearance control on a surface with its own material; the control boundary and native material fields now match the actual renderer. Readiness, local bounds and foreground masks repair occluded entry input. A native trace proved that an entry-owned tooltip intercepted a press over a scaled rear glyph; its popup now has an empty input mask while retaining visible text. Exact owned geometry restoration uses a 25 ms move after Plasma size hints and three consecutive readbacks within the original bound; the queued-placement cause is inferred from source, not proved in the original trace (`PLACEMENT_RESEARCH.txt`).

After Qt upgraded from 6.11.2 to 6.12.0, threaded captures were blank before item grabbing and projected coordinates became inconsistent; the unchanged mapping passed under Qt's basic loop. Official Qt source shows View3D mapping recalculates the shared projection. Public one-argument Camera mapping avoids those writes and passes with the normal threaded loop. Shared projection mutation is the supported inferred cause; no debugger captured simultaneous writes (`CAMERA_MAPPING_RESEARCH.txt`, 107/109/110/112).

The Qt 6.12 wheel failure is reproduced and its delta cause confirmed: physical angle -120 plus pixel -15 followed the pixel path, yielding 6.2838 rather than the expected 60 pixels. A mouse-only QML source filter failed because Wayland identified the seat as a touchpad. Qt/Kirigami native APIs and source were researched before the small Qt-only observer was added; native event source distinguishes physical and synthesized input without a device/phase heuristic. Actual-event and private Wayland regressions pass (`WHEEL_SOURCE_RESEARCH.txt`, 119/122/123/127/136/141/143/144/146).

The first resumed final suite (150) passed 77 tests before the native import resource check exceeded its unchanged 128 MiB cap: +134436 KiB. A disposable trace (158) reproduced +137052 KiB and showed mask QObject destruction rather than accumulation. Qt Canvas owns the pixel QImage until collection, and QML property storage can outlive the QObject; a disposal discriminator (160) clearing only the pixel var passed at +85672 KiB. `AlphaHitMask` now explicitly clears that var before unloading its image on destruction. The normal native repair check (162), without mask tracing or forced GC, passed at +33652 KiB. Delayed pixel-cache retention is the supported inferred cause; no trace captured the exact allocations for all RSS growth (`MASK_MEMORY_RESEARCH.txt`). The initial supplied-prefix diagnostic (156) failed executable identity because its copied D-Bus launcher retained an absolute prefix path; it was corrected to use the existing fresh-install path and left no backend behind. No numeric cap, deadline, rendering quality or garbage-collection policy changed.

The cleanup-only repair was insufficient: run 169 again exceeded the theme-switching RSS bound (+145804 KiB), despite passing the native renderer assertions. Native Qt source confirms that cache-disabled matching image instances each decode the same artwork; the retained readiness and drawn foreground/body instances were doing that. Qt's unused-pixmap cache is bounded (2 MiB in this Qt build). A disposable shared-cache discriminator (177), with no forced collection, stopped the previous steady per-pass climb and passed at +61756 KiB. Baked layers now share Qt's matching URL/size/crop rasters while retaining the same decode-size cap, package validation and visible geometry. The normal nine-test native batch (179) passed at +34064 KiB. Allocation sharing is the supported repair; the exact individual allocations in each failed RSS sample were not captured. Diagnostic 171 did not invoke collection because Qt.gc was undefined and is invalid collection evidence; 174 verifies the global API, and 175 actually invoked it only in a disposable prefix. Production has no forced collection or permanent quality reduction.

Run 164 passed 79 tests, including the repaired native resource gate (+32852 KiB), then its folder gate reached latest rendered badge/progress on both panels before the separate content-publication revision advanced. The source uses a separate 100 ms publication timer. The probe now requires both drawn latest values and that published revision within the same eight-second wait; the original 0<changes<100 and elapsed*10+3 rate assertions are unchanged. No production publication behavior changed. The normal folder/content gate passed in 167 with 100 updates/one publication. The exact independent host refresh trigger was not captured (`CONTENT_PUBLICATION_RESEARCH.txt`).

Run 181 passed 83 tests, including the shared-pixmap resource check (+50192 KiB), then missed the owner flat free-4 icon wheel. The rest predicate compared the same cached log record twice; diagnostic 185 confirms identical sample timestamps. It now requires the requested renderer after the settings request and advancing observations across the original 350 ms interval. Eight-second deadlines, movement expectations and tolerances are unchanged. The exact failing native target was not captured, so stale readiness is an inferred cause rather than a proved sole cause. Diagnostic 184 stopped earlier with Dolphin-drop NoReply under verbose pointer logging. Bus-traced 185 passed all runtime cases, and all captured content additions replied within 0.011931 s. That earlier timeout cause remains unproved; no timeout changed (`WHEEL_TARGET_RESEARCH.txt`). Focused 187 and the new complete run provide final-source evidence.

The memory gate recurred in run 189: native assertions passed but theme-switch RSS grew +132488 KiB, so mask cleanup and pixmap sharing did not close it. Heaptrack's same-run warm/cycled comparison (198) showed +21.68 MB decimal of live heap, dominated in the largest allocation stacks by native KDE QML D-Bus decoding and value serialization. The exact normal-run RSS excess was not reproduced under profiler overhead. KDE's own callback API documents a single-shot connection to prevent callback/reply reference cycles. The applet now uses that overload, preserving JSON conversion, errors and explicit destruction, and ignoring replies after its applet is destroyed. Disposable discriminator 199 decreased RSS by 9212 KiB; normal native import/runtime 201 passed 2/2 in 192.83 s, with theme-switch RSS -2384 KiB. Retained replies are the supported inferred remaining cause; no forced GC, bounds or wait times changed (`DBUS_REPLY_MEMORY_RESEARCH.txt`). Invalid profiler setup 192 was a space-split LD_PRELOAD path; corrected 194 captures the trace. Optional detailed reporter 196 was stopped as too expensive; complete normal rankings 197/198 reuse the same trace and retain unresolved transient plugin symbols.

Earlier failures and the owner's interrupted 95/118 run are retained as diagnosis; they are not counted toward final acceptance. The old native latency fixture omitted an inactive baked tint in true 3D while preserving Orange's own material, measurement method and limits. A one-off Dolphin-drop NoReply did not recur in later complete native interaction checks; its deadline cause remains unproved and no production deadline changed.

The standalone material CPU probe (148) is retained because the later alpha-mask, pixmap-sharing and applet reply changes do not alter its procedural renderer or its standalone probe. The full suite reruns structural animation/hidden-state assertions on current source.

### Files changed

68 exact paths from the Task 4 base, grouped by purpose: original material assets/generator/licensing; existing layout and 2D/3D renderers; platform geometry and visible-entry masks; schema/model/registry/Studio transactions; host input/owned placement; native wheel metadata; focused/native tests and operational documentation. The exact list is:

```text
CHANGELOG.md
CMakeLists.txt
README.md
assets/themes/arc-platform-orange/archdock-theme.json
assets/themes/arc-platform-orange/metadata/production-record.json
assets/themes/octagon-platform-steel/archdock-theme.json
assets/themes/octagon-platform-steel/metadata/production-record.json
assets/themes/ring-platform-blue/archdock-theme.json
assets/themes/ring-platform-blue/metadata/production-record.json
docs/CURRENT_STATE.md
docs/INSTALL.md
docs/KNOWN_LIMITATIONS.md
docs/repairs/ADREP-TASK-004.md
docs/repairs/README.md
docs/shared-renderer.md
packaging/LICENSING.md
plasma-dock-widget/contents/ui/DockEntry.qml
plasma-dock-widget/contents/ui/main.qml
qml/ArchDock/Rendering/FolderTrack.qml
qml/ArchDock/Rendering/LayoutEngine.js
qml/ArchDock/Rendering/PanelScene.qml
qml/ArchDock/Rendering/PanelSurfaceLoader.qml
qml/ArchDock/Rendering/PlatformGeometry.js
qml/ArchDock/Rendering/inputs/AlphaHitMask.qml
qml/ArchDock/Rendering/inputs/GeometryHitRegion.qml
qml/ArchDock/Rendering/inputs/ScrollInput.qml
qml/ArchDock/Rendering/materials/crystal.svg
qml/ArchDock/Rendering/materials/floating-glass.svg
qml/ArchDock/Rendering/materials/futuristic.svg
qml/ArchDock/Rendering/materials/glass.svg
qml/ArchDock/Rendering/materials/lime.svg
qml/ArchDock/Rendering/materials/metallic.svg
qml/ArchDock/Rendering/materials/minimal.svg
qml/ArchDock/Rendering/materials/neon.svg
qml/ArchDock/Rendering/materials/organic.svg
qml/ArchDock/Rendering/materials/pedestal.svg
qml/ArchDock/Rendering/materials/plasma.svg
qml/ArchDock/Rendering/materials/plate.svg
qml/ArchDock/Rendering/materials/platform.svg
qml/ArchDock/Rendering/optional3d/IconStyle3D.qml
qml/ArchDock/Rendering/optional3d/PanelScene3D.qml
qml/ArchDock/Rendering/renderers/PanelBaked25D.qml
qml/ArchDock/Rendering/renderers/PanelProcedural2D.qml
qml/runtime/SettingsEditorModel.js
qml/runtime/SettingsPopup.qml
src/PanelRegistry.cpp
src/inputs/WheelSource.h
src/model/PanelDefinition.cpp
src/model/PanelSettingsSchema.cpp
src/model/PanelSettingsSchema.h
src/panel/PanelWindowPresets.cpp
src/panel/PanelWindowSettings.cpp
tests/Baked25DAssetTest.cpp
tests/PanelSettingsSchemaTest.cpp
tests/PanelWindowCapabilityTest.cpp
tests/RendererCapabilityTest.cpp
tests/data/studio-truth-matrix.json
tests/run-rendering-import-smoke.sh
tests/run-wayland-hardening-matrix.sh
tests/tst_DockEntry.qml
tests/tst_DockGeometry.qml
tests/tst_GeometryHitRegion.qml
tests/tst_PanelBaked25D.qml
tests/tst_PanelMaterials.qml
tests/tst_PlatformGeometry.qml
tests/tst_SettingsEditorModel.qml
tests/visibility-window.py
tools/generate-material-textures.py
```

### Open points and limits

- Physical owner-desktop visual, GPU/display cadence and hardware input acceptance are unexecuted. Automated native evidence uses disposable D-Bus/KWin/Plasma sessions.
- Task 4 builds no package and installs nothing. The owner runs 0.1.1-12; final packaging/install belongs to Task 5, which has not started. Older 0.1.1-11 documentation sections are historical.
- The inherited Task 3 diagonal baked-ring Grid popup overlap remains recorded. The final 90-opening anchor gate records one baked diagonal Grid note: gap 29.5 px, facing 0.725, one covered icon. This is the inherited Grid placement limitation, not a failed new Fan/Arc/Stack/Ring assertion.
- One orphaned private backend (PID 117670, deleted staged executable and verified private XDG root) was stopped after the full run; the owner backend PID 941 remains. No coredumps were found since the resume preflight. Remaining task-owned profiler/clone/scratch artifacts will be disposed during closure (`207-orphan-cleanup.json`).
- No texture download or replacement renderer/settings/geometry/recovery framework was added. Original bundled material provenance is in `packaging/LICENSING.md`.
- Report 1 precedes commit/sync and the fresh-clone recheck. Report 2 is pending.

### Owner checks for this task (after Task 5 installs)

1. Compare Glass, Floating Glass and Futuristic; check the texture remains when tint/opacity change and Apply always changes the active look.
2. Load Orange Arc or Cyan Mesh; watch icons move behind the platform and return, and wheel around it.
3. Check separate Flat/Platform groups, turn Platform presentation off/on, edit native colour/material/texture and try Bend.

### Checkpoint history

The owner explicitly paused on 2026-10-07. `fe0ec4a7c35c0acce337b32bfbae791ff539bd23` saved the implementation before formal closure; its suite stopped on request after 95/118 passed, no completed failures. The private interrupted session was disposed with its logs retained. On resume 2026-10-08 the pack preflight passed again, installed state and predecessor closure were reconciled, and the Qt minor-version drift required fresh verification. All 550 non-document source/fixture/asset/recipe files are pinned in `202-final-source-hashes.json` including the newly added wheel-source header.


## Report 2 - recheck

- Task source commit: `2882d462ce4e9065ddd4b714e0885bcc9714e8ea`, on top of the owner-authorized `fe0ec4a` checkpoint; no source follow-up was needed after the push. Recheck round: **1 of at most 3**.
- Remote `origin/main` and local HEAD were verified equal to that source commit after a successful non-force push and fetch (`212-source-sync.json`). The recheck record is carried by the subsequent documentation-only commit titled `ADREP-TASK-004: record the recheck`; its actual hash and final remote parity are given in the visible Report 2 and retained in the sync receipt. A document cannot contain its own commit hash without changing that hash.
- Fresh clone from `origin/main`, with no copied binaries: one-job Debug/Quick3D configure and full build **passed**, exit 0, 1342.09 s (`214-recheck-clone.log`, `215-recheck-configure.log`, `216-recheck-build.log`, `217-recheck-build-receipt.json`).
- All **33/33 planned focused, regression and required native/resource CTests passed serially**, zero failures, 1173.30 s (`204-recheck-test-plan.txt`, `219-recheck-tests.log`, `220-recheck-tests-receipt.json`). This includes native rendering/import, real Studio/runtime input, window/folder interaction, all 90 folder-anchor openings, presentation, path travel, staged presets, existing preset matrix and resources.
- All **550 pinned non-document source/fixture/asset/recipe file contents** match both the original full-suite source and the fresh clone. Git executable intent matches. One existing permissions difference is recorded: `data/source-assets/source-asset-catalog.json` is 0600 in the original workspace and Git 100644/0644 in the clone; its bytes match and both are non-executable. The owner's permissions were preserved (`218-recheck-source-scope.json`, `222-recheck-final-receipt.json`).
- The original complete configured suite remains **118/118 in one serial run**, 1607.15 s on this exact source (`203-full-suite-final.log`). No CTest was disabled or skipped. In the focused recheck, two internal Qt rows request a native session (grouped windows and energy pixels); both execute in the dedicated native rendering gate.

### Every criterion and owner finding rechecked

| Criterion | Findings | Pushed-source proof and result |
|---|---|---|
| 1: distinct texture; Glass and Futuristic details | OF-24, OF-26, OF-27 | Material capture/statistics test passes again; retained thirteen same-Qt before/after pairs and original licensed SVG details match the byte-identical source. Visual owner judgement remains unexecuted. |
| 2: Apply changes the look, or an inapplicable control has a reason | OF-25 | Backend/window/editor transactions, native Studio truth matrix and real runtime Apply pass. |
| 3: tint and opacity retain texture | OF-28 | Material, skin, baked, surface/parity and native colour/texture/reset pixel checks pass. |
| 4: platforms keep motion, behind/front ordering and visible-only input | OF-29, OF-30 | All 24 native renderer cases, baked/geometry hit-region tests, native wheel/runtime input, path travel and all 90 folder anchors pass. |
| 5: separate flat/platform groups and exact flat restoration | OF-32 | Preset/backend/editor restore contracts and native Studio switching/Apply/Cancel/reopen pass. |
| 6: editable native colour/material/texture | OF-33 | Native material/reset pixels and real Studio fields pass. |
| 7: supported Bend; unsupported reason | OF-31 | Geometry, twelve native look/layout rows and Studio reason/field checks pass. |
| 8: rotation follows the selected motion target | OF-29, OF-32 | Preserved preset/model motion contracts, presentation, native Studio/runtime and path travel pass. |
| 9: idle/hidden CPU and Studio latency | OF-24, OF-27 | Fresh resource/latency gates pass. Retained fifteen-phase material CPU measurements use unchanged procedural source; current full/focused tests recheck static/hidden animation assertions. |
| 10: complete configured suite once on final source | OF-24 to OF-33 | 118/118 original final suite, plus 550-file byte pin against the pushed source and 33/33 fresh-clone recheck. |

All **10/10 criteria** and **OF-24 through OF-33** retain passing automated proof. No new implementation gap was found; no recheck repair commit was required. PD-17/18/19/23/25 behavior and the preserved physical/smooth wheel rule remain as in Report 1.

### Fresh performance, documentary audit and limits

- Studio medians: edit **39.85 ms**, page **34.25 ms**, Apply **12.60 ms**; unchanged limits **53.76/41.16/24.84 ms** pass, with zero package reads each (`221-recheck-latency-limits.log`, `recheck-latency/`).
- Eight audition/cancel cycles: backend RSS **65872 to 75524 KiB**, **+9652 KiB (9.43 MiB)**, below 64 MiB; zero stale hosts; settled idle CPU **0.0%** of one core over five seconds (`resources-recheck/session.log`).
- Native theme switching: PlasmaShell **649936 to 704060 KiB**, **+54124 KiB (52.86 MiB)**, below the unchanged 128 MiB bound.
- Native sampled frame interval/render CPU, Cyan/Orange/generated circle: **12.917/25.2862/19.2867 ms**, **0.547167/0.874739/1.33499 ms**. These private-compositor samples do not establish physical desktop cadence.
- Documents were checked against the raw full-suite and clone logs, captures, measurements, source pin and exact base diff. The **68 paths** from base `617bd96` match the Report 1 list and the task's renderer/settings/input/test/asset/document scope. No LCL repository, successor implementation or unrelated file was edited.
- The inherited diagonal baked-ring Grid note remains identical: gap 29.5 px, facing 0.725, one covered icon, no reported new Fan/Arc/Stack/Ring problem. Physical owner-desktop visuals, hardware input and GPU/display acceptance remain unexecuted. The live source-archive fixture remains for Task 5 packaging.
- Post-recheck process audit found **no running task backend, private Plasma/KWin session, profiler, build or test process**, and no `archdock-rendering-import.*` temporary root. Owner backend **PID 941** and desktop PlasmaShell **PID 1042** were preserved. No coredump was found since the 2026-10-08 04:35 UTC resume preflight (`223-recheck-process-audit.json`).
- The inactive fresh clone, raw profiler scratch root, extracted profiler, diagnostic stage and scratch Python environment are scheduled for the mandatory cleanup immediately after this record is pushed and given. Detailed evidence, captures, interpreted profiles and the shared build are retained for Task 5. The cleanup receipt records actual removals; they are not claimed as already removed here.
- Installed owner package remains **0.1.1-12**. Task 4 made no package and installed nothing. Task 5 starts only after this Report 2 and cleanup.
