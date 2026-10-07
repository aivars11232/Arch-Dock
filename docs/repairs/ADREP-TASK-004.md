# ADREP-TASK-004: Textured looks, platform presentation and editable native materials

Repository: `/mnt/F/Arch Dock/`, origin `https://github.com/aivars11232/Arch-Dock`, branch `main`.
Package: `/mnt/F/Arch Dock LCL repairs/`, ADREP 1.0.0.
Base: `617bd96e8972111c3277c542be89e018e7cfd2f6`.
Evidence: `build-codex-adrep/evidence/ADREP-TASK-004/`, indexed in `INDEX.txt`.

## Paused checkpoint - 2026-10-07

The owner requested: "commit and Sync current changes and pause, we'll continue tomorrow".
This checkpoint saves implementation and verification in progress. It is not
Report 1 or Report 2, and ADREP-TASK-004 is not closed. ADREP-TASK-005 has not
started. The owner explicitly authorized this checkpoint commit and sync before
the ordinary per-task closure sequence finished.

The complete-suite serial run was stopped at the owner's request: **95 of 118
CTests passed, zero failures among completed tests**, with test 96 interrupted
and tests 97-118 not executed in that run. CTest exited 130 after SIGINT; the
remaining private harness received TERM so its owned EXIT cleanup could run.
The paused run is not complete-suite acceptance and must be rerun from the
start. The source was pinned before it and all 59 pinned files still matched
at interruption.

Task 4 progress is approximately 85% complete as a checklist estimate;
full-suite acceptance, formal reports, pushed-source fresh-clone recheck and
closure remain. No package was built or installed for Task 4. The actual
installed baseline is 0.1.1-12.

## Implementation

- All thirteen procedural looks have distinct original bundled textures. Glass has translucency, frost, edge highlights and a refraction-like gradient; Floating Glass adds elevation and shadow. Crystal has facets, Metallic brushed grain, Futuristic seams/circuit lights, Organic soft grain, Neon tubes, Plasma energy waves, Minimal satin, Lime cells, and Platform/Plate/Pedestal solid panel/joint/flute bodies. Tint preserves texture; opacity affects the full material. Sparkle defaults to zero and is offered only for Crystal and Plasma.
- Flat looks and platform looks have separate groups. Loading a platform turns its presentation on and remembers the complete previous flat appearance, layout and visual motion settings. Turning Platform presentation off restores that look exactly through the existing draft/Apply/Cancel path. Legacy platforms without a remembered snapshot use their preserved procedural appearance and clear platform theme IDs.
- Procedural Theme is absent on a look with its own skinned/baked/native material, with a reason and route to Flat looks. The new native Platform colour/Material/Texture controls apply live; Reset material restores the look's own defaults.
- Blue Ring, Steel Octagon and Orange Arc perspective artwork has a retained foreground body mask as well as its front rim. Actual MouseArea and DropArea masks exclude covered rear entries. The alpha-mask object now exposes readiness. Local bounds narrow custom entry masks before the occlusion predicate. A native Qt input trace proved that an entry tooltip intercepted presses over scaled rear glyphs; the entry-owned tooltip now has an empty popup input mask and remains visible. DockEntry 25 cases and all 90 native folder anchor openings pass; native test targets select a fresh exposed point after renderer changes. Orange's occlusion threshold follows its open track. Wheel and continuous item travel keep working, with front/rear ordering and reappearance. Native mesh picking refines Qt's bounding-volume hits against actual platform triangles.
- Bend folds the rear half of generated and numerical native platforms up or down by up to 90 degrees. Positions, normals and icon feet share the fold; texture coordinates, topology and resource bounds remain intact. Fixed perspective artwork explains that it cannot fold and offers true 3D where supported. Items/Whole panel/Both behavior from Task 2 remains intact.
- Static materials have no periodic paint timer. Plasma and Futuristic animate a small cached-body overlay at 8 Hz and stop when hidden, concealed, reduced-motion or zero opacity. The complete flat-look normalization path uses an immutable Qt QHash schema index to avoid repeated linear scans while retaining descriptor ordering and transaction validation.

## Owner findings and evidence

OF-24 through OF-33 are implemented and have passing focused evidence. They
remain pending formal task closure; the rows below do not declare the task
closed.

| Finding | Repair | Proof |
|---|---|---|
| OF-24 | Thirteen materials with texture; declared controlled sparkle rather than generic glitter. | Material distinctness: all 78 common-tint pairs differ, each interior variance >4; 21 material QtTests; comparison.html. |
| OF-25 | A procedural appearance control cannot change a look's own baked/skinned/native material. It is absent with an explicit route; Load and Apply use the same candidate; native materials have their own live controls. | Capability/Studio matrix, transactional native Studio interaction, material/reset GPU pixels and full-flat restore. |
| OF-26 | Translucent Glass and elevated Floating Glass, frost/gradient/highlights. | Matched radius-300 before/after captures, common-tint interior variances 27.89 and 22.23, opacity tests. |
| OF-27 | Futuristic seams, circuit/light lines and slow pulses. | Material SVG, before/after capture; visible/hidden energy and idle CPU evidence. |
| OF-28 | Tint preserves texture; opacity affects material and detached perspective foreground. | All 13 tint/opacity cases; baked zero-opacity visible-entry/input regression; native material color/texture/reset pixels. |
| OF-29 | Loading a platform preserves the selected item/panel/both motion; does not reset travel. | Backend candidate invariants, model Apply/Cancel/reopen; perspective wheel/continuous cases and native matrix. |
| OF-30 | Entries travel behind the far body and return in front; covered areas do not take entry input. | Three perspective capture sequences; native Cyan/Orange/generated and generated Blue/Steel track cases, exact-triangle picks. |
| OF-31 | Actual geometry fold with anchored feet and fixed-artwork unsupported explanation. | Fold geometry tests and native pixel/triangle/projection checks for 12 look/layout rows. |
| OF-32 | Flat/platform groups and exact previous-flat restore; motion target preserved. | Backend complete-flat restore, model draft/Cancel/reopen, native Studio toggle interaction; Task 2 regression tests. |
| OF-33 | Editable native platform colour, material and texture with own-material Reset. | Thirteen native material captures; magenta/texture None/Organic and exact own-pixel Reset; Studio native fields. |

## Product decisions applied

- PD-17: separate Flat looks and Platform looks; loading a platform captures the complete flat look, and Platform presentation off restores it exactly through draft, Cancel, Apply and reopen.
- PD-18: continuous and wheel travel use each platform's own track; foreground artwork or native triangles occlude covered glyphs, and the real entry pointer/drop targets follow the exposed glyphs.
- PD-19: thirteen original materials remain visibly distinct under a common tint, retain texture with colour and opacity, and offer sparkle only for Crystal and Plasma, off by default.
- PD-25: the existing Items/Whole panel/Both target, speeds and trigger survive look changes and native/perspective selection. Fixed open perspective arcs retain their unsupported whole-rotation explanation.

## Acceptance criteria

| # | Criterion | Result | Proof |
|---|---|---|---|
| 1 | Every procedural look shows a distinct textured material; Glass reads as glass and Futuristic has future-tech detail. | Automated checks pass; final suite pending | 21 material QtTests; all 78 common-tint pairs differ; materials/comparison.html (13 radius-300/icon-52 before/after pairs). |
| 2 | Apply changes the panel to the chosen look, or the control is absent with a reason. | Automated checks pass; final suite pending | Backend/editor look transactions, native Studio truth matrix and runtime interaction; inapplicable procedural Theme and flat colour are absent, with a route to the active renderer's material controls. |
| 3 | Colour tints and opacity work on every look and keep its texture. | Automated checks pass; final suite pending | All 13 material tint/opacity rows; baked foreground opacity and input; native colour/texture/reset GPU pixels. |
| 4 | Every platform keeps continuous/wheel travel, behind/front ordering and visible-only entry input. | Automated checks pass; final suite pending | Three perspective travel capture sequences, native Cyan/Orange/generated checks and actual-triangle picks; 25 DockEntry checks; runtime native matrix; 90 folder anchor openings. |
| 5 | Separate flat/platform groups; platform off restores the exact previous flat look. | Automated checks pass; final suite pending | Backend complete-flat snapshot restore; model draft/Cancel/reopen and live Studio interactions. |
| 6 | With 3D on, colour/material/texture edits apply. | Automated checks pass; final suite pending | Native GPU material rows, magenta colour and None/Organic texture changes, exact own-material Reset; live native Studio controls. |
| 7 | Bend works on supported shapes; unsupported looks say so. | Automated checks pass; final suite pending | Numerical fold geometry, native triangle/projection/pixel checks including 12 look/layout rows; perspective artwork explanation in Studio. |
| 8 | Platform rotation follows Continuous motion moves. | Automated checks pass; final suite pending | Preserved motion candidate/model contracts, native Studio probes, perspective/native wheel and continuous travel. |
| 9 | Static/hidden-material CPU and Studio latency meet the contract. | Final focused performance checks pass; final suite pending | 94-latency-limits.log: 45.49/37.18/13.31 ms; resources-final-source/session.log: 0.0% backend CPU, +11.39 MiB for eight cycles, zero stale hosts; 98-material-cpu.json: all eleven static and both hidden animated materials at 0.0%; visible energy materials 3.0%. |
| 10 | Complete configured suite passes once serially on final source. | Pending | 100-full-suite.log stopped after 95/118 passed at the owner's request; no complete-suite acceptance. The earlier 110/118 run remains diagnosis only. |

## Performance and retained diagnostics

- All eleven static procedural materials and both hidden animated materials: 0.0% of one core, five-second /proc samples after settling. Final visible Plasma and Futuristic: 3.0%; 98-material-cpu.json contains all fifteen final-source phases. Initial paint samples and the longer settling diagnostic are retained; see CPU_PROTOCOL.txt.
- Final isolated Studio medians: edit 45.49 ms, page 37.18 ms, Apply 13.31 ms (94-latency-final.log). Limits: 53.76/41.16/24.84 ms. Zero theme-package reads each. Retained earlier measurements exceeded the first two bounds; the schema index repairs that path.
- Final-source private backend: 0.0% idle CPU. Eight audition/cancel cycles: 63652 to 75320 KiB resident memory, +11668 KiB (11.39 MiB), below 64 MiB; zero stale hosts.
- Final-source private native View3D sampled intervals: Cyan 17.69 ms, Orange 18.02 ms, generated circle 17.31 ms; rendering CPU 1.02/0.51/0.02 ms. These are short private-compositor samples, not physical desktop cadence acceptance; physical monitor/GPU/owner-desktop acceptance remains unexecuted.
- Earlier exploratory native capture/projection failures remain in logs 29/32/34/49; the preliminary full-suite failures remain in 62 and were repaired in focused regression checks. A native placement failure (test 94) motivated a narrow owned-container move after Plasma's queued size hints and three consecutive exact geometry readbacks within the original bound. The queued-relayout mechanism is inferred from KDE source, not proved in the original failing trace; PLACEMENT_RESEARCH.txt and 65/67 preserve the diagnostic and passing checks. Frame completion waits and projection diagnostics remain; strict native checks passed in 51, 60 and 96, and in the interrupted full run's completed rendering-import-smoke gate. Do not infer a proven production cause from the exploratory capture failures.

The old native latency fixture carried a baked tint into the true-3D candidate; the OF-33 control boundary requires omitting that inactive legacy field from setup while retaining the native Orange defaults, measurement method and limits. One runtime D-Bus NoReply occurred in run 85 although the Dolphin row committed; a seven-call trace in 86 measured 0.16-12.01 ms replies and the complete native runtime matrix passed in 87. The one-off deadline cause remains unproved; no production RPC deadline or assertion changed.

## Current verification

- Final complete one-job Debug build: 99-final-build.log, exit 0.
- Final-source focused runs: 95 passed 7/7 CTests in 101.84 s; 96 passed 4/4 in 141.86 s, including the native import gate. Counts include 19 schema, 21 material, 25 settings-model, 63 baked QML, 9 asset and 23 native graphics QtTests. 97 passed 5/5 CTests in 254.34 s: 76 backend checks (1 live-session-only skip), 559 dock geometry, 8 geometry-hit-region and 25 DockEntry checks; host-neutral import policy also passes.
- Full suite: 100-full-suite.log stopped on request after 95/118 passed, no completed-test failures. Test 96 interrupted; 97-118 unexecuted. A new complete serial run is required.
- Pushed-commit fresh-clone recheck: PENDING; no Task 4 recheck clone was created.

## Scope and delivery limits

Work inspected the current source, Task 3 Report 2 and live Git baseline rather than relying on completed-task labels. The actual installed baseline is 0.1.1-12, matching the interim Tasks 1-3 recipe at 617bd96; older current-state sections saying 0.1.1-11 are historical. Task 4 does not package/install; final packaging belongs to Task 5. Required Qt/KDE native research preceded edits: KWindowEffects blur is a compositor/window operation; ShaderEffect cannot draw through Qt Quick's software backend, so original maps extend the existing Canvas. Qt's custom-mesh pick is refined with actual triangles. No external textures or replacement renderer stack were added.

Owner checks after Task 5 installs: compare Glass/Floating Glass/Futuristic; load Orange or Cyan and watch item travel behind/in front; switch platform off/on, change native material/color/texture and try Bend. These are the visual/physical checks automation cannot close.

## Files in the checkpoint

- `CMakeLists.txt`: explicit material install/copy list and material capture test.
- `qml/ArchDock/Rendering/materials/*.svg` (13 original maps), `tools/generate-material-textures.py`, `packaging/LICENSING.md`: deterministic original bundled materials and provenance.
- `LayoutEngine.js`, `renderers/PanelProcedural2D.qml`: textured body, tint, opacity, controlled sparkle and bounded visible-only energy animation.
- `PlatformGeometry.js`, `PanelSurfaceLoader.qml`, `optional3d/{PanelScene3D,IconStyle3D}.qml`: native materials, numerical fold, anchored feet, triangle visibility and alpha/depth behavior.
- `PanelScene.qml`, `inputs/{GeometryHitRegion,AlphaHitMask}.qml`, `renderers/PanelBaked25D.qml`, three production perspective theme manifests/records: foreground occlusion, mask readiness, local bounds and visible-entry input.
- `src/model/Panel{Definition,SettingsSchema}.cpp`, `PanelSettingsSchema.h`, `PanelRegistry.cpp`, `src/panel/PanelWindow{Settings,Presets}.cpp`, `qml/runtime/{SettingsEditorModel.js,SettingsPopup.qml}`: durable flat-look restore, grouped looks, native controls, immutable descriptor index and exact owned geometry readback.
- `plasma-dock-widget/contents/ui/{DockEntry,main}.qml`: real pointer/drop predicates, passive owned tooltip, and bounded owned placement restoration.
- C++/QML renderer, schema, material, editor, input and geometry tests; `tests/data/studio-truth-matrix.json`; `tests/{visibility-window.py,run-rendering-import-smoke.sh,run-wayland-hardening-matrix.sh}`: regression proof, native visible input targets, optional diagnostic tracing and idle CPU evidence.
- `README.md`, `docs/shared-renderer.md`, `docs/CURRENT_STATE.md`, `CHANGELOG.md`, `docs/repairs/README.md` and this checkpoint: behavior, scope, verified results and remaining closure.

## Pause cleanup and limits

- The interrupted private session had no surviving process with its XDG homes;
  its logs were retained in `interrupted-full-suite/` and only its inactive
  `/tmp/archdock-plasma-lifecycle.LuP5vz` root was removed.
- Generated `tests/__pycache__` was removed. The shared build, test Python
  environment, benchmark tools and all Task 4 evidence remain for tomorrow.
- No coredumps were found since Task 4 began, checked at pause in
  `101-dump-audit.log`. The owner's installed `/usr/bin/arch-dock` process
  (PID 1008002 at the checkpoint) remained running.
- The Task 3 Grid-popup overlap on the diagonal baked ring remains recorded:
  the final anchor run had 89 ordinary passing rows and one allowed overlap
  note, 90 actual openings in total. It does not broaden Task 4's scope.
- The one-off drop NoReply did not recur in the interrupted full run's completed
  `runtime-ui-interaction-smoke` (66.35 s); its cause remains unproved.
- Private native graphics/input samples do not establish physical GPU, display
  cadence, touchpad or owner-desktop visual acceptance. The owner checks above
  wait for the authorized Task 5 package installation.

## Resume here

1. Read this checkpoint and `build-codex-adrep/evidence/ADREP-TASK-004/INDEX.txt`.
   Reconcile Git/remote, installed package, personal backend and source against
   `102-checkpoint-source-hashes.json` (the earlier full-run pin is
   `100-final-source-hashes.json`); source changes after pause need appropriate
   fresh checks. Run the repair pack's `validation/verify_all.sh` again.
2. Keep one heavy gate at a time, build with one job and test serially. Reuse
   `build-codex-adrep/build`, the retained test Python environment and existing
   passing focused/performance evidence if the relevant source is unchanged.
3. Rerun all 118 configured tests from the start on the final source. Do not
   add the interrupted 95 results to the remaining 23 and call that a full run.
   The command used was:

   ```sh
   env PATH="$PWD/build-codex-adrep/test-python/bin:$PATH" \
     ctest --test-dir build-codex-adrep/build -V --output-on-failure -j 1
   ```

4. Once the complete run passes, finish the ten-criterion evidence map, formal
   Report 1, current-state/changelog documents, and the task-scoped commit/sync.
   This checkpoint is not a substitute for that closure record.
5. Recheck the pushed source in a fresh, short-path clone (for example
   `/mnt/F/ad4.XXXXXX/b`), build with one job, run the required focused/native
   tests and performance checks, audit scope/documents/leftovers, then append,
   commit and push Report 2. Keep the full original assertions and limits.
6. Give Report 2 visibly and complete Task 4 cleanup before starting Task 5.
   Task 5 then covers icon shapes/parameters/logos, real tile depth/materials,
   all 15 icon presets, final canonical exports, package upgrade/recovery/
   removal gates and the authorized final installation. Reconcile the actual
   0.1.1-12 baseline rather than assuming the historical 0.1.1-11 label; no
   Task 5 source, recipe, package, install or closure work was done here.
