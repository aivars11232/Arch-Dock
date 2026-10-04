# Post-TASK-0045 corrective pass

<!-- FOLDER_CONTENTS_REPORT_BEGIN -->
## Folder contents transparency, scrolling and names — 0.1.1-6, 2026-10-04

Starting clean main `d759c28fd28ed03edc4e7ed3681f128c50c3cebe` and owner package 0.1.1-5. Pack integrity 174/174 PASS.

Folder contents now unfold from the clicked rendered icon in KDE's native
anchored popup, with no themed Dialog background or opaque Pane. The existing
AppletPopup role, focus/deactivation, guards, selection revalidation and
five layouts remain. Dense Fan fits one vertical scroll axis; wheel up/down
and held pointer dragging scroll without opening a child. Arrow navigation
keeps the selected child in view. Labels are on by default; **Panel Studio ->
Panels -> Behavior -> Always show folder item names** saves through the existing
Apply/Cancel transaction, model/schema, renderer projection and persisted record.
Names off retains hover labels. The 48-item cap, original glyph/style/tile path,
scrollbars, horizontal overflow for applicable layouts, empty/unavailable notices
and reduced motion remain. Hidden close resets the explicit opening animation
immediately, so rapid reopening unfolds from the icon again.

Fresh one-job Debug/Quick3D build PASS (1508.93 s),
with the final incremental rebuild PASS. Six focused model/schema/transaction,
Studio, geometry and folder checks PASS. Complete final-source CTest
**111/111 PASS**, 53 serial bounded batches,
601.97 s summed batch wall time; zero failed/skipped/missing
names and all 644 tracked input bytes unchanged.
Four native Wayland gates PASS (170.47 s): actual
rendering, windows, folder/segment/status interactions and Studio/panel controls.
QML scan: zero unexpected errors; existing deliberate negative diagnostics
are classified in the retained receipt. All checks are self-run corrective
verification; owner visual/physical acceptance remains separate.

Source freeze `332037ba9839e48a858735366bb0000cf0cd1c95`. Two canonical exports, independent
regeneration of all four artifacts, extracted bytes/Git modes, unique checkpoint,
normalized ownership/epochs, GPL and reference-image exclusion PASS:
527 source files/528
regular archive members. Exact root recipe pin PASS.
Source SHA-256: `dff87e9a8145834c5800bf867526be39bfbfc1b445e7af1e071b25013fa96ee3`.
Package SHA-256: `e230ca026631845b387d911199ff690afc8bed7ca21954c6e81ccf7e10396be2`.
Fresh one-job Release/makepkg build PASS (395.16 s).
Installed package gate PASS (154.75 s):
216 payload bytes/modes, 17 asset declarations/eight MIT components,
15+15 preset catalogs/cards, startup with/without optional 3D, actual installed
UI and folder interaction matrices, native **0.1.1-5 -> 0.1.1-6** upgrade,
recovery and removals with configuration preserved. Compiled/extracted source
and the disposable verification clone were removed before installed verification;
the original checkout/debug paths were masked.

The first installed gate stopped when an existing Dolphin drop exceeded the 1000 ms synchronous D-Bus deadline although the backend persisted it. The identical package and unchanged assertions passed on a fresh isolated rerun. The failed attempt is retained; no drop-path production change or acceptance shortcut was introduced. Cold owner-desktop behavior remains part of manual acceptance.

Primary native references: [KDE Dialog](https://api-staging.kde.org/plasmaquick-dialog.html),
[Qt Flickable](https://doc.qt.io/qt-6/qml-qtquick-flickable.html),
[Qt native grab implementation](https://raw.githubusercontent.com/qt/qtdeclarative/v6.11.2/src/quick/items/qquickitemgrabresult.cpp).
NoBackground and transparent window color reuse KDE's native support; the
existing Wayland AppletPopup role avoids the known generic-tool stacking problem.
Default left-button Flickable dragging preserves Qt 6.8 compatibility.
Native capture uses the QML-created mainItem because the window's C++ contentItem
has no QML engine; native background/color are independently asserted. The EIS
wheel sign follows the proven Studio probe, and PNG decoding waits for the real
save acknowledgement. Initial reproductions and failed fixture attempts are
retained; no product assertion was weakened.

Owned cleanup PASS: removed 1,899,859,968 allocated/
1,854,447,115 apparent bytes of disposable work.
All 972 protected historical files retain identical bytes/modes;
all 478 baseline core identities remain, zero new cores, zero
task dump payloads and zero task processes. 450 compact
evidence ZIP entries and exact source/package artifacts remain in
`build-codex-folder-contents-0.1.1-6/`. One primary session, no subagents,
one build job and one test worker throughout.

Owner still runs 0.1.1-5; the authorized 0.1.1-6 update follows verified cleanup and candidate commit/sync, with a fresh private configuration backup.

Exact changed paths: `CHANGELOG.md`, `PKGBUILD`, `docs/INSTALL.md`, `plasma-dock-widget/contents/ui/FolderExpansionHost.qml`, `plasma-dock-widget/contents/ui/main.qml`, `qml/ArchDock/Rendering/FolderExpansion.qml`, `qml/ArchDock/Rendering/LayoutEngine.js`, `src/model/PanelDefinition.cpp`, `src/model/PanelDefinition.h`, `src/model/PanelSettingsSchema.cpp`, `tests/PanelSettingsSchemaTest.cpp`, `tests/PanelWindowCapabilityTest.cpp`, `tests/run-arch-package-smoke.sh`, `tests/tst_DockGeometry.qml`, `tests/tst_FolderExpansion.qml`, `tests/visibility-window.py`, `docs/CURRENT_STATE.md`, `docs/RELEASE_CHECKLIST.md`, `docs/POST_TASK_0045_CORRECTIVE_REPORT.md`.

The historical v0.1.0 tag and its published assets are untouched. No real new tag/publication occurred. Final clean Git/live remote/owner receipts are in local STATE.json.

**Preserved historical checkpoints follow. Their bodies and original version-specific outcomes remain unchanged; the 0.1.1-6 record above is current.**
<!-- FOLDER_CONTENTS_REPORT_END -->

<!-- AUD_01_02_REPORT_BEGIN -->
## AUD-01 + AUD-02 correction — 0.1.1-5, 2026-10-04

1. **STARTING HEAD:** `333fbc2aa8c906523a14a5cf1d168fbe45a91013`, clean main

2. **FINAL HEAD / WORKTREE:** Source freeze `fd23810f156b7303e634c2533d421dd414e13d12`. Operational closure follows as separate commits; final exact clean main/live remote parity is recorded in local STATE.json after final sync.

3. **STARTING INSTALLED PACKAGE:** arch-dock 0.1.1-4, verified

4. **AUD-01 ROOT CAUSE:** Manually repeated package identities/current command filenames were not advanced with the authoritative pkgrel.

5. **CURRENT-FACING FILES CORRECTED:** README, CHANGELOG, KNOWN_LIMITATIONS, PLATFORM_MATRIX, INSTALL, LICENSING, CURRENT_STATE, RELEASE_CHECKLIST and this report. Shared prose points to CURRENT_STATE; runnable filenames use 0.1.1-5.

6. **HISTORICAL REFERENCES PRESERVED:** Prior checkpoint bodies and explicit old versions/upgrade provenance remain unchanged; the previous owner package is explicitly 0.1.1-4.

7. **AUD-02 REPRODUCTION:** Missing/corrupt mask -> valid mask in one persistent scene failed on old code: valid Image stayed Null, fallback stayed latched and the source/failure cycle logged a binding loop. Owned test setup corrected the mask object schema and enabled the visible TestCase.

8. **AUD-02 ROOT CAUSE:** styleMaskFailed was an unconditional lifetime bool; fallback also cleared the Image source responsible for its own error.

9. **AUD-02 EXACT FIX:** One authoritative current URL from raw resolved mask, local failed-URL map and computed current failure; Image uses that source independently of fallback. No global error manager/reset or suppression of other asset errors.

10. **INVALID -> VALID SAME SCENE:** Both missing/corrupt rows PASS: same scene/Image, Error observed, valid source Ready, styled layers active, mask effect enabled and original glyph/source retained.

11. **INVALID -> NO MASK:** PASS: current Image becomes Null/empty, old mask error no longer applies and plain/original glyph renders; subsequent valid source recovers again.

12. **CURRENT INVALID MASK:** PASS: valid -> broken returns Image.Error and complete safe original-glyph fallback, with mask effect disabled.

13. **FOCUSED TEST RESULTS:** 11/11 style/package/asset/tile/override/renderer/native checks PASS; initial five QML checks PASS. Final pin/exporter check PASS (6.63 s).

14. **COMPLETE CTEST RESULT:** 111/111 PASS, 53 serial bounded batches, 571.02 s, zero CTest skips/missing names; 644 tracked input bytes unchanged.

15. **NATIVE WAYLAND RESULT:** Four affected gates PASS (151.85 s): staged/current glyphs, actual 2D/3D textures/tiles, same-scene masks, current UI, drops, scrolling/arrows and panel controls. Actual RHI images retained.

16. **QML ERROR SCAN:** Zero unexpected TypeError/ReferenceError/binding/shader/texture/import errors. Six expected prior non-mask negative-fixture diagnostics classified; new mask negatives are confined to their dedicated native regression log.

17. **FINAL PACKAGE VERSION:** Application 0.1.1; Arch package 0.1.1-5; proposed future v0.1.1 uncreated/unpublished.

18. **SOURCE SHA-256:** `22124187d0014e50064fb838e4782aaf9280e3c518f4b6fc9146ef640c3c57d8`

19. **PACKAGE SHA-256:** `59708bab963c43eaa667019029ee83a97f2f3ddb8431692bf8eb99a11e1eee55`

20. **CANONICAL EXPORT RESULT:** PASS: two independent tracked exports, 527 source files/528 regular members; bytes/modes/extraction/GPL/reference exclusion/unique checkpoint/normalized UID/GID/names/tar+gzip epochs.

21. **RELEASE VERIFIER RESULT:** PASS: independently regenerates/archive/checkpoint/PKGBUILD/SHA256SUMS and byte-compares all four. Annotated verification tag/repository owned/disposable only; historical v0.1.0 unchanged.

22. **INSTALLED PACKAGE RESULT:** PASS (99.43 s): 216 actual payload bytes/modes, fresh install, licensing/resources, 15+15 presets/cards, startup with/without 3D, real installed glyph/UI/mask recovery. Filename count remains 216; no payload files added/removed. Source/compiled export removed and original checkout/debug masked; copied test probes/runtime UI/negative SVG fixtures are explicit.

23. **UPGRADE RESULT:** Native 0.1.1-4 -> 0.1.1-5, recovery and both removals PASS; configuration preserved.

24. **LICENSING RESULT:** GPL-3.0-or-later complete official text, eight MIT components/notices and all 17 original asset declarations PASS; unknown-rights reference material remains excluded/non-installable.

25. **CONFIGURATION PRESERVATION:** Both actual owner Arch Dock and Plasma configuration files remain byte-identical to the private backup; zero changed logical keys.

26. **OWNER PC UPDATE STATUS:** Owner PC now runs verified **0.1.1-5** after a fresh private configuration backup. Actual 216/216 payload bytes/modes, native pacman Qkk, stable D-Bus owner/live executable hash and Panel Studio startup PASS. Both configuration files are byte-identical and changed logical keys are empty. No active Arch Dock applet/integration module required a Plasma-wide refresh; only the backend was restarted. The previous binary reactivated during package installation; its exact UID, D-Bus owner, PID/start identity and old executable hash were verified before stopping only that process. D-Bus activation then loaded the new installed hash and remained stable through Panel Studio startup.

27. **CLEANUP RESULT:** Owned root removed: 1,667,784,704 allocated/1,639,671,345 apparent bytes at final disposal, plus earlier compiled/export disposal. 263 compact ZIP entries retained. 956 protected file bytes/modes and 478 core identities preserved; zero new records, zero task dump payloads/processes. Wrong-interpreter disposable environment was recreated with explicit system Python/system-site access; no product or acceptance assertion was weakened.

28. **GIT DIFF --CHECK:** PASS; exact staged paths reviewed for each logical source/pin/operational commit. Final owner/Git receipts are recorded after sync.

29. **EXACT CHANGED FILES:** `CHANGELOG.md`, `PKGBUILD`, `README.md`, `docs/CURRENT_STATE.md`, `docs/INSTALL.md`, `docs/KNOWN_LIMITATIONS.md`, `docs/PLATFORM_MATRIX.md`, `docs/POST_TASK_0045_CORRECTIVE_REPORT.md`, `docs/RELEASE_CHECKLIST.md`, `packaging/LICENSING.md`, `qml/ArchDock/Rendering/IconScene.qml`, `tests/run-arch-package-smoke.sh`, `tests/run-rendering-import-smoke.sh`, `tests/tst_IconScene.qml`

30. **OWNER MANUAL ACCEPTANCE:** Still required: switch valid styles and reproducible unavailable-mask fallback without restart; launcher/folder glyphs, true 3D, tiles, drops, vertical scrolling, horizontal tabs/arrows and rotation/panel controls. No physical/owner visual acceptance or all-bugs-fixed claim. One primary session; no subagents.

Primary native references: [Qt Image status/source](https://doc.qt.io/qt-6/qml-qtquick-image.html), [Qt MultiEffect masks](https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html). Source-sensitive semantics reuse the existing IconStyle2D URL failure pattern; no KWin/window-rule workaround is required for this local QML state cycle.
<!-- AUD_01_02_REPORT_END -->

<!-- PANEL_MOTION_REPORT_BEGIN -->
## Panel controls and motion correction — 0.1.1-4, 2026-10-04

1. **Task boundary:** Owner-requested Panels overflow/status, opening/closing, optional free rotation, wheel turning, position and tilt; prior Icon Tiles behavior is retained.

2. **Starting state:** main 08e24ce7b32bcc057dc62c7d41d2697f563ef724 and installed 0.1.1-3 were verified; task pack integrity 174/174 PASS.

3. **Authority/resources:** Owner authorized verification, cleanup, commit/sync and PC update. One primary session, one build/test worker and no subagents; old evidence is protected.

4. **Native research:** Qt TabBar is a horizontal flickable and overlay styles can cover the last arrow. KDE Plasma 6.7 Widget.setGeometry is a no-op; the existing owned container route is reused. Resizing before moving is supported by native container/grid code and actual read-back failures/pass.

5. **Tab navigation:** Native arrows bound/clamp the existing tab flickable. Effective scrollbar space not already reserved by the style is excluded from the row.

6. **Status truthfulness:** Unattempted is neutral Not checked yet, success/verified remain false; failed/unsupported/fallback diagnostics are preserved.

7. **Missing animation cause:** The procedural capability profile omitted implemented linear collapse and Studio previews froze rotation. Their existing runtime paths are now reachable from the editor.

8. **Opening/closing:** Animations groups resting state, mechanism/axis, trigger, reveal handle, delays and duration. Draft coherence, Cancel/Apply/reload and saved Open/Close actions reuse the transaction/presentation request.

9. **Manual wheel turn:** Interactive free radial panels turn 15 degrees per notch, up clockwise/down counterclockwise; touchpad pixels are proportional. Angle is transient and uses the actual shared entry/hit/drop/popup geometry.

10. **Continuous rotation:** Optional None/Clockwise/Counterclockwise, speed and idle/hover trigger use SceneRotationController. Animations preview plays it; other cards remain deterministic.

11. **Free position:** General X/Y goes through exact ownership/type/token checks, live geometry read-back and required host-apply/rollback. Rollback restores actual prior geometry, including independent desktop drift.

12. **Resize/move correction:** Profile apply reproduced a resized free widget at the wrong position. Existing main.qml callback now resizes before positioning; targeted and final profile/native move tests pass.

13. **True 3D tilt:** Layout camera pitch uses parameters3D.cameraPitch, bounded to -60..60, and the existing camera. Actual RHI captures at 10/-35 differ while logical rectangles and glyph centers remain aligned.

14. **Baked tilt:** Layout tilt follows the selected theme/state declared range through parameters2_5D.tilt; an out-of-theme-range edit is refused before persistence.

15. **Data compatibility:** Typed scalar aliases preserve opaque renderer map fields and profile/runtime persistence; no parallel settings store or owner host adoption is introduced.

16. **Interaction guards:** Native arbitrary XY/rotation is unavailable. Drag/edit/popup/collapsed/concealed states pause rotation; reduced motion suppresses animated transitions/continuous motion while retaining state changes.

17. **Focused controls:** Actual Studio SpinBox/ComboBox and keyboard/click controls verify draft preview, Cancel, Apply, persistent reload, native refusal and both control styles.

18. **Complete configured verification:** Final 111/111 configured CTests PASS in 37 serial batches, 590.96 s summed passing batch wall time. Zero final skips/missing names; final source inputs were unchanged.

19. **Native/RHI verification:** Four native Wayland gates PASS, 146.34 s, with real EIS arrows/wheels, 2D/3D rotation, live move/rollback, hover open/close, original glyphs and URI/app/folder drops. Actual 3D captures are retained.

20. **Diagnostic limits:** One early bounded drop NoReply and one final three-second private Plasma startup scripting timeout were not reproduced by the acceptance reruns. Tracing exposed its own readiness/log contamination; all uninstrumented final gates pass with original timeouts/assertions. No speculative transport repair was made.

21. **Other gate corrections:** Fusion arrow/scrollbar coverage was fixed in production. Folder fixture now tests unsupported split instead of newly supported linear collapse; rejection/revision assertions remain. A missing task screenshot directory was created without changing tests. Installed verification needed a pre-existing systemd runtime subdirectory; the early renderer probe HOME/XDG paths were then placed inside the same writable package fixture, with its outer session bus disconnected. Native mktemp is delegated for all other fixtures. Source masks, package bytes and assertions are unchanged.

22. **Canonical source:** Freeze d1cb20a6f533e044e37c37170a3275b51cc17eae; two canonical tracked-checkout exports identical; annotated verification only in disposable clone. Initial isolated-HOME export was refused for local tool settings and those owned export bytes removed. Final extraction/inventory/modes/GPL/reference exclusions PASS: 527 source files/528 members, SHA256 c53bd92345d29ba3325c48cac5894853650523acc737bacc3623bce524c42642.

23. **Package identity:** Pinned root recipe is byte-identical to canonical PKGBUILD; one-job Release/Quick3D-ON 0.1.1-4 build PASS. Package SHA256 fb31f07927691614b9199e33c9f550ae0e79d3ee7de94c1b6d368cb0241c13f9; 216 payload files.

24. **Installed gate:** Actual native package root, installed executable/applet/renderers/themes, complete payload bytes/modes/licenses/resources and 15+15 cards pass. Compiled/extracted source was removed; checkout/build masked. Copied UI probes are explicit; actual backend is the package executable. Wall 94.26 s.

25. **Upgrade/recovery/removal:** Native 0.1.1-3 -> 0.1.1-4 upgrade, data-only recovery and both removals pass while saved configuration remains byte-identical.

26. **Cleanup/preservation:** Owned disposable root removed (1,775,763,456 allocated/1,742,262,560 apparent bytes at final disposal), 483 compact ZIP entries retained. All 941 protected historical files and 460 core journal identities preserved; one owned probe abort journal retained with raw payload/metadata removed; 17 new unrelated Nexees records preserved; zero remaining task dump payloads/processes.

27. **Owner installation:** Owner PC now runs verified **0.1.1-4** after a fresh private configuration backup. Actual 216/216 installed payload bytes/modes, pacman Qkk (325 files, zero altered), stable new D-Bus owner/executable hash, the new projected settings and `arch-dock --settings` PASS. Both Arch Dock and Plasma configuration files are byte-identical, with no changed logical keys. No active Arch Dock applet or loaded integration module needed a Plasma-wide refresh; the verified backend was restarted and Panel Studio is open. Desktop activation briefly restarted the old mapped executable during package installation; the post-install restart was accepted only after its live executable hash matched the new package. Candidate closure was committed/synced at `e364a1729030319d38221a70a0473b461890bf97` before this update. The final operational Git receipt belongs to local STATE.json.

28. **Git/publication boundary:** Source freeze and canonical pin/operational closure are separate. Candidate cleanup/commit/sync precedes PC update; final owner receipt is committed/synced afterward and exact clean main/remote parity is recorded in STATE.json. Real v0.1.0 is unchanged; no new real tag/publication or physical acceptance is claimed.

Exact changed paths in this correction:

- `PKGBUILD`
- `docs/CURRENT_STATE.md`
- `docs/INSTALL.md`
- `docs/POST_TASK_0045_CORRECTIVE_REPORT.md`
- `docs/RELEASE_CHECKLIST.md`
- `plasma-dock-widget/contents/ui/main.qml`
- `qml/ArchDock/Rendering/PanelScene.qml`
- `qml/ArchDock/Rendering/PanelSurfaceLoader.qml`
- `qml/ArchDock/Rendering/previews/LivePanelPreview.qml`
- `qml/runtime/PlacementStatus.js`
- `qml/runtime/SettingsPopup.qml`
- `qml/runtime/StudioNavigation.js`
- `qml/runtime/VisibilityStatus.js`
- `src/model/PanelCapabilityResolver.cpp`
- `src/model/PanelDefinition.cpp`
- `src/model/PanelSettingsSchema.cpp`
- `src/panel/PanelSettingsTransaction.h`
- `src/panel/PanelWindow.cpp`
- `tests/PanelCapabilityResolverTest.cpp`
- `tests/PanelSettingsSchemaTest.cpp`
- `tests/PanelWindowCapabilityTest.cpp`
- `tests/RendererCapabilityTest.cpp`
- `tests/tst_LivePanelPreview.qml`
- `tests/tst_PanelBaked25D.qml`
- `tests/tst_PlacementStatus.qml`
- `tests/tst_SceneRotation.qml`
- `tests/tst_StudioNavigation.qml`
- `tests/tst_VisibilityStatus.qml`
- `tests/visibility-window.py`

Primary native references: [Qt TabBar](https://doc.qt.io/qt-6/qml-qtquick-controls-tabbar.html), [Qt ScrollView](https://doc.qt.io/qt-6/qml-qtquick-controls-scrollview.html), [Qt WheelHandler](https://doc.qt.io/qt-6/qml-qtquick-wheelhandler.html), [Plasma scripting](https://develop.kde.org/docs/plasma/scripting/), [KConfig writable checks](https://api.kde.org/kconfig.html), [Plasma 6.7 Widget implementation](https://raw.githubusercontent.com/KDE/plasma-workspace/Plasma/6.7/shell/scripting/widget.cpp). Container/grid relayout ordering is an inference supported by the inspected primary KDE source, installed container code and the failing/passing actual-geometry gates.

Owner quick checks: use a populated free circular panel and scroll up/down; choose optional continuous rotation on Animations; select Collapsed with a supported mechanism and hover/click its reveal handle; Apply X/Y on General; adjust Perspective tilt on Layout with a compatible theme; narrow Studio and use the tab arrows. Cancel should discard unapplied settings. Owner physical visual acceptance remains pending.
<!-- PANEL_MOTION_REPORT_END -->

## First real-PC runtime/UI correction — 0.1.1-2, 2026-10-04

**FIRST OWNER-OBSERVED RUNTIME/UI ISSUES CORRECTED AND VERIFIED.** These are
corrective checks by the implementing agent. This pass follows the owner's
Dolphin folder/free-circular-panel and Panel-page observations; it is not
TASK-0046 and does not claim all UI is bug-free. Older sections below are
historical candidate evidence.

1. **Starting HEAD:** clean main/origin f7b72859b5410f57669c5c56dbda1dfd2861706b. Owner-requested pause was committed/synced at cd05eb39ff3027e8c4ed8074fadb8a1b9c5c9b46, then explicitly resumed.
2. **Final HEAD / worktree:** source freeze 85614f7f6ffed1bdba73528483b5e1089ba53f2f; later closure changes only the excluded recipe pin and operational documents. Exact final main/remote SHA, clean tree and parity are recorded after commit/sync in `build-codex-first-runtime-ui-0.1.1-2/STATE.json`.
3. **Installed build tested:** owner started with `/usr/bin/arch-dock`, package 0.1.1-1 (210 verified payload files). Fresh native isolated gates test the actual Release 0.1.1-2 package (215 files), built from the exact source freeze. Owner upgrade completed after cleanup/commit/sync and a new two-file private backup: actual 0.1.1-2, 215/215 payload bytes/modes and pacman Qkk PASS; running backend executable hash matches the package; normal D-Bus activation and Panel Studio launch PASS. STATE.json and the private installation receipt record current owner identity.
4. **OBS-01 reproduction:** event regressions fail real internal drag/reorder completion and backend refusal; Qt drop delivery exposes acceptance before asynchronous mutation results. Owner free-2 was Empty, which intentionally displays no launcher records even if older drop handling stored them.
5. **OBS-01 cause:** proxy hotspot/smoothing and absent terminal Drag.drop prevented actual target drops; mutation ran during drag-enter. Asynchronous Plasma QML D-Bus results arrived after the drop event's lifetime, while unconditional acceptance claimed success. Empty/non-launcher invitation was misleading.
6. **OBS-01 fix:** pressed-position hotspot, unsmoothed proxy, MIME keys, terminal drop/reset/cancel; commit only on actual drop with edit/disabled/input/ownership guards. Native Qt D-Bus bridge addresses the existing unique owner, uses bounded calls, and returns the actual bool before acceptance/success animation. Launcher/Hybrid accept supported URLs; Empty gives truthful instructions and remains Empty. No native/free content restriction is relaxed.
7. **Drag/drop results:** DockEntry focused QtTest 21/21; native Qt drag/drop delivery 7/7; registered policy/transaction tests pass. Real private GTK Wayland URI transfers cover application/folder drops on entry/surface/empty Launcher, duplication, refusal, edit/disabled behavior, pointer reorder, persisted order and native ownership. This is a real Wayland source, not an actual owner Dolphin gesture.
8. **OBS-02 reproduction:** native custom-folder metadata test returns generic `folder` instead of the fixture's actual artwork; real RHI mesh entry has zero visible glyph pixels because the platform occludes it. Owner's existing folder metadata resolves an absolute PNG path containing spaces/Unicode; that private artwork is not included in source/package.
9. **OBS-02 cause:** three folder entry paths hard-code `folder`, losing native .directory/special-folder icons. Camera-local glyph geometry was at world-platform depth and covered by the mesh.
10. **OBS-02 fix:** KDE KFileItem resolves folder metadata for native dock, free entries and nested folders. Scale glyph position and dimensions together at depth ratio 0.25, placing artwork in front while retaining its projected input position/size, original source, style and motion.
11. **Icon results:** DockModel 12/12, FolderContentModel 6/6 and focused panel/input 5/5; SVG/theme/custom PNG/space paths and launcher/running merge source identity pass. Real RHI per-entry glyph-pixel assertions, motion/camera/fallback checks and all-row native 2D/3D glyph validity/size pass. Private owner-artwork captures are retained; physical GPU/display acceptance remains manual.
12. **OBS-03 cause:** nested Flickable accepted wheel only over its narrow viewport; KDE SpinBox/ComboBox/Slider wheel handling stole page input and changed values. KDE implicit-width bindings also looped in the Studio controls.
13. **Vertical fix:** shared wheel-only overlay routes natural header/content input through nested-to-outer scroll priority, clamps origins/bounds and disables wheel value changes on settings controls. Exclude the tab strip before pointer acceptance. Studio controls use a finite implicit-width baseline; clicks, touch, keys and direct editing continue through their original controls.
14. **OBS-04 cause:** overflowing tab/folder content had no applicable horizontal wheel route; vertical handling could block tab delivery or process Shift twice. Genuine horizontal overflow needed its own routing.
15. **Horizontal fix:** pixel delta first, angle fallback, Shift + vertical conversion once, horizontal-only tab handler and existing folder viewport routing; no movement or invented content width without real overflow. Existing scrollbar dragging and keyboard navigation remain available.
16. **UI input results:** StudioScroll 6/6 and FolderExpansion 12/12; production Qt QWheelEvent pixel/angle cases verify precise content offsets and unchanged values; existing bars/keys/bounds pass. Real KWin EIS vertical/horizontal/Shift gestures over natural content and SpinBox/ComboBox pass. EIS continuous scrolling reaches Qt as angle delta on this compositor; physical touchpad pixels remain manual acceptance.
17. **Affected CTests:** fresh 22/22 registered names in three serial bounded batches, zero failures/skips. After package release/recipe changes, final exporter CTest 1/1 in 6.58 s, all 11 unittest groups.
18. **Full CTest:** fresh 110/110 configured names, eleven serial batches; 533.02 s command wall time / 532.89 s summed CTest-reported durations; no failed/skipped/missing names. Initial source-readiness failure is retained. Native KWin source focus, one drag start/end and released prior source fix the fixture; the full suite then passed again from test one. Package-only fixture extensions and final disposal correction occurred afterward and have their own targeted native/install gates; this is not a second full suite after those harness edits.
19. **Native Wayland:** final affected four CTests 4/4 in 131.60 s after final teardown fix, zero new dump. Earlier native 4/4 in 133.37 s also passed runtime assertions but later cleanup exposed a Mesa worker teardown dump. Existing lifecycle SIGKILL-only final private shell disposal is reused; normal restart semantics are unchanged.
20. **Installed package:** one-job Release, Quick3D ON, BUILD_TESTING OFF, makepkg verification/build PASS. Compiled/extracted source removed before native gates. All 215 payload bytes/modes/resources, full GPL/MIT notices, 17 original asset declarations and eight MIT components pass. Native fresh install, 15+15 catalogs/cards, optional-3D-hidden checks, installed startup, real installed UI matrix, 0.1.1-1 -> 0.1.1-2 upgrade, configuration recovery and both removals preserve configuration. Package matrix masks checkout/original build; copied test probes/UI fixtures are explicit, while executable/applet/modules/themes come from installed payload. Database-only removal affects only the cloned existing Arch Dock record. Installed theme copies fix relative fixture URLs; Qt writes the trusted PNG because native Glycin nested sandboxing cannot start in the overlay. Assertions remain intact.
21. **New source SHA-256:** `daab73ef0dcd0f5fb940762a3e89f6ce7d346c0c95ff55ee8a8556a7e4c03607`. Two exact canonical exports and annotated disposable-tag verifier pass; 525 source files / 526 regular members, unique root checkpoint, canonical modes/ownership/order/epochs, GPL and reference-image exclusions. Old source hashes are stale for this corrected candidate.
22. **New package SHA-256:** `a5020f57699660f5f1e2e5caf1179412cd93047d73f0b244263db1c147726d0e`. Root recipe exactly matches canonical generated bytes; separate RELEASE_SHA256SUMS preserves the exporter's one-entry SHA256SUMS.
23. **Cleanup:** disposable root/build/venv/clones/fixtures/partial Release outputs removed: 2,349,432,832 allocated bytes (2.349 GB / 2.188 GiB), 2,304,012,939 apparent bytes. Exact identified 31,394,377-byte private Mesa dump removed after diagnostics; 243 compact evidence entries retained in mode-0600 ZIP. All 912 protected older files retain bytes/modes; zero task processes or dump payloads. All 458 baseline journal identities remain; one older payload independently became missing, consistent with configured two-week retention, but its deletion actor was not independently established. No unrelated dump was deleted by the agent. Interrupted owned pkg directory permission was restored only for disposal.
24. **git diff --check:** PASS before staging; staged exact-path check is required again at final commit.
25. **Exact changed files:** listed below relative to starting HEAD; source freeze and excluded closure paths are kept explicit.
26. **Remaining observed issues:** no failing automated cell in these four groups. Owner Dolphin, physical touchpad/GPU and manual restart/persistence acceptance remain distinct. The owner-approved native Plasma refresh replacement aborted in a libtaskmanager QPixmap icon-read thread; managed plasma-plasmashell.service startup restored the desktop, followed by backend reactivation. Exact native cause is unproved; the successful managed-service workaround and trace are retained. Current saved free-1/free-2 are Detached and no hosted Arch Dock widget exists; the installed widget package is ready for the existing Panel Studio free-panel creation path. Preserve Empty semantics: choose Launcher/Hybrid explicitly if dropped entries should appear. The owner explicitly approved that broader desktop refresh under section 15. Arch Dock configuration is byte-identical; all other Plasma logical keys match the backup except native panelWidgets cache and slideshow current-image state. Only the resulting exact system dump and identified DrKonqi cache/dialog/debugger were cleaned; diagnostics remain. No active-owner-widget glyph acceptance is claimed.
27. **Short owner manual list:** below. The owner configuration is backed up before upgrade; no automatic panel conversion or unrelated Plasma change is part of the correction.
28. **Git/publication still required:** source/candidate closure was committed and synced before owner update; the final operational documentation receipt is committed/synced afterward, with clean main/remote parity recorded in STATE.json. No new real tag or release publication was performed during this runtime pass; preserved v0.1.0 is unchanged. Candidate artifacts are local. Manual owner acceptance remains required.

Exact changed paths:

```
CMakeLists.txt
PKGBUILD
docs/CURRENT_STATE.md
docs/INSTALL.md
docs/POST_TASK_0045_CORRECTIVE_REPORT.md
docs/RELEASE_CHECKLIST.md
packaging/LICENSING.md
plasma-dock-widget/contents/ui/DockEntry.qml
plasma-dock-widget/contents/ui/main.qml
qml/ArchDock/Rendering/FolderExpansion.qml
qml/ArchDock/Rendering/inputs/ScrollInput.qml
qml/ArchDock/Rendering/optional3d/PanelScene3D.qml
qml/ArchDock/Rendering/qmldir
qml/runtime/PresetBrowser.qml
qml/runtime/SettingsPopup.qml
qml/runtime/StudioForm.qml
src/DockModel.cpp
src/integration/DropBackend.h
src/model/FolderContentModel.cpp
src/panel/PanelWindow.cpp
tests/DockModelTest.cpp
tests/DropDeliveryTest.cpp
tests/FolderContentModelTest.cpp
tests/PanelWindowCapabilityTest.cpp
tests/RendererCapabilityTest.cpp
tests/run-arch-package-smoke.sh
tests/run-rendering-import-smoke.sh
tests/tst_DockEntry.qml
tests/tst_FolderExpansion.qml
tests/tst_StudioScroll.qml
tests/visibility-window.py
```

Owner acceptance after installation and widget reload:

1. Create a hosted free panel through the existing Panel Studio control; choose Launcher/Hybrid, then drop an app and the original Dolphin folder onto panel space and an entry. The saved free-2 record remains Empty and detached.
2. Reorder two entries; confirm one move, cancellation/refusal and no accidental click launch.
3. Compare application and custom folder glyphs with Dolphin in 2D and true 3D; confirm original artwork remains recognizable.
4. On Panel page, wheel over normal content, labels and controls; the page should scroll without changing control values.
5. On real horizontal overflow, check touchpad/wheel sideways, Shift-wheel, the existing scrollbar and keys; ordinary fitted content should stay fitted.
6. Restart Arch Dock normally.
7. Confirm dropped entries, order, icons and settings persist.

## Historical RC-03, RC-04 and GPL licensing — 0.1.1 candidate, 2026-10-04

**All supplied source/export/package/licensing/installed corrective gates and
owned cleanup PASS. Corrected next candidate prepared; new real tag/publication
NOT EXECUTED. No runtime implementation is reopened.**

The supplied independent audit covers `b6fb0b472d9ea7428494239102a0877b591755eb`,
reports zero new runtime defects and closes AD-04-R1 and RC-02. Actual clean
starting main was `4d65678d53dd5d0e9f388bbda6bf43117427b1ea`; its newer approved
publication-closure documents are preserved. These fixing-agent checks are
corrective/candidate verification, not independent closure review.

Before repair, five synthetic controls passed incorrectly: modified PKGBUILD
build command retaining a digest, regular-member ownership, root checkpoint
mode 0600, wrong gzip epoch and clean tracked ordinary checkout mode 0666.
No candidate recipe was executed. One reproduction helper initially matched a
legitimate nested checkpoint; it was corrected to the exact generated root
member. The provenance helper's existing “Original” predicate was made
case-insensitive. Both diagnostics and the unchanged product assertions remain
in the evidence.

RC-03 now independently regenerates all four exporter outputs in owned
temporary storage and requires exact bytes for archive, external checkpoint,
PKGBUILD and SHA256SUMS. Existing semantic checks remain; ownership, generated
checkpoint mode and gzip epoch are explicitly checked. RC-04 maps tracked
Git index 100644/100755 to archive 0644/0755. Developer-only inputs retain
executable intent with canonical permissions; official inputs must be clean
and tracked. All **11 unittest groups PASS**: valid candidate, 17 artifact
mutations, manually repaired receipts, wrong HEAD/tag target, lightweight tag,
tracked/untracked dirty source, precommit identity, all eight required tracked
permission variants, developer policy and unchanged namespace/symlink controls.

The owner selected **GPL-3.0-or-later** for original project work. Root LICENSE
is the complete official **35,149-byte** GNU GPLv3 text, byte-identical from GNU
web and FTP primary sources, SHA-256
`3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986`.
The [license matrix and notices](../packaging/LICENSING.md) cover original
C++/QML, Plasma/KWin, 15+15 presets, all production artwork, separate components
and system dependencies. Eight MIT components retain MIT and their complete
notice. Unknown-rights material remains NOASSERTION/redistribution-unknown,
reference-only/non-installable/outside relicensing; images are absent from the
normalized source and package. No incompatible distributed component was found.

Each of **11 theme and six icon packages** has recorded independent original
creation/redistribution evidence. Their 17 manifests and 16 production records
now carry the owner-selected GPL declaration. Six icon manifest output hashes
and ten theme production-record asset hashes were refreshed because the license
edits change those recorded files. Every other JSON field and all artwork,
geometry, SVG, texture, QML and reference/visual-review evidence are unchanged.
The final audit checks **81 declared theme asset hashes** and **57 production
output hashes**, and package comparison independently checks all 33 changed
asset JSON projections. Existing source-pixel, reference-rights, asset-hash and
renderer assertions are preserved.

The first affected CTest run caught the omitted ten theme digest updates:
ChassisAssetTest, EnergyAssetTest and Baked25DAssetTest refused packages with
`asset-hash-mismatch`. The narrow correction refreshed their record hashes;
no loader, artwork or assertion was changed. The first finalization also exposed
one stale limitations opening sentence naming 0.1.0. It was corrected before
refinalizing. An earlier passing package from source `278542a14ffc1cf959ad3f9d95a9e656cad40324`
is superseded, with its raw build/verification and failed-test evidence retained.
The final source/export/package gates were repeated, not relabelled.

Application **0.1.1**, package **0.1.1-1**, uses finalized source commit
**`96f0e4f60d024b5cb1a44af1402401656ded1372`**, epoch **1791069742**.
Native vercmp confirms it is newer than 0.1.0-2. Annotated verification tags
existed only in a normal disposable repository. Final exports from checkout
catalog modes 0600 and 0666 are byte-identical across all four outputs; exact
verifier, successful extraction and full inventory pass: **521 source files,
522 unique regular members**, canonical Git modes, one root checkpoint without
descendants, deterministic order/JSON, normalized ownership and tar/gzip epochs,
exact recipe/sums, included GPL text/declaration and absent reference images.
The retained candidate also passes the exact verifier before clone cleanup.

Final source SHA-256:
`d76c9e45b30123f9064fca3837207b77e9f82f83d21304c9bae1069ba7fff525`.
Package `arch-dock-0.1.1-1-x86_64.pkg.tar.zst`, **2,252,307 bytes**, SHA-256:
`22b44078a8e9b9cbe95e81a51da590d3dcbcc43df25a0c80bc17d63f2ec2cb7d`.
PKGBUILD SHA-256:
`3485f555e5f2ebddafa23261189500417cb0eec141b79e8a77345db65a2b489f`.
External checkpoint SHA-256:
`b8a818e1cd81ab0afd947db08ec0a45a8f1cc8fa01174456549ad6b2286ab97e`.
Canonical exporter SHA256SUMS SHA-256:
`de742a471431285776f705446ae15101a712312082e613f7a5dd515eb6b2e77f`.
The root recipe pins this exact archive. An eventual annotated v0.1.1 tag must
target the tested source commit above; main's later operational-documentation
and recipe-pin closure commit is recorded separately in STATE.json.

Fresh `makepkg --verifysource` passes in **1.10 s**. Fresh one-job,
one-compression-worker, no-LTO Release/Quick3D-ON build passes in **380.51 s**.
Makepkg initially refused to enter source verification while the owned
superseded package still occupied its output filename; that package was moved
within owned scratch before rerunning unchanged required flags. The actual
build retains `libfakeroot internal error: payload not recognized!` and the
source-directory-reference warning (the latter also existed in the preceding
build). Built archive ownership is root:root and unique; metadata, exact payload
and all native operations pass. The unchanged desktop categories produce a
validator hint with exit zero. These diagnostics are retained, not hidden.

Fresh focused target build passes in **212.52 s**. All **5/5 affected CTests
PASS**, **6.97 s**, including the 11 exporter groups. All 307 source/test/CMake
files are unchanged between the focused binary build and final source commit;
changed asset data is freshly loaded by the successful tests. The existing
scoped native helper retains install/upgrade/removal/configuration/catalog/card
assertions; only current license intake/audit assertions were copied from the
repository harness. Private startup/recovery blocks stay explicitly reused.

Fresh scoped installed gate passes in **27.07 s**: **210/210** payload
bytes/modes on fresh install and upgrade, against 209 CMake install paths plus
INSTALL.md; GPL/MIT metadata, complete official GPL and MIT notices, 17 original
asset declarations and eight preserved MIT components; **15 Panel + 15 Icon
Presets and actual cards**, QtTest **8/8**, Quick3D-hidden **4/4**, zero
failures/skips. Native **0.1.0-2 → 0.1.1-1** upgrade and both removals pass;
configuration bytes are preserved. No obsolete paths are expected between
these payload sets; their absence assertion remains enforced. Compiled package
source/build and full extractions were removed first, and the owner checkout
is masked during installed checks. Disposable install roots are removed by the
existing harness trap. Host package state and owner desktop are not modified.

Runtime reuse is explicit: of **349** earlier recorded inputs, **342** are
unchanged; the seven changed inputs are CMake version/license installation,
licensing notice, four asset-license expectations and native package license
assertions. Production C++ and QML are byte-unchanged. Toolchain/dependencies
match BUILDINFO excluding pkgver/date/recipe-hash/build-path metadata.
**173** previous package payloads remain byte/mode-identical; 33 asset JSON
files change only licenses/digest pins, executable/two documents differ and
LICENSE is added. No binary identity/equivalence is claimed. Configured names
remain **107: five fresh, 102 explicitly reused**. Prior 18 native CTests,
37 lifecycle phases and private installed startup/recovery are reused results;
no fresh full suite or private runtime matrix ran.

Historical v0.1.0 remains object `50812852c4dc2726411a1c73452296955852c50a`,
target `c3b3a0b7771b313c45f843f49a503b45b0d1ada0`, locally and remotely.
Fresh API checks preserve release ID/body/title/timestamps/prerelease status
and all six asset IDs/names/sizes/digests/metadata. No new real v0.1.1 or fixture
tag exists on main/remote. Real tagging/publication requires separate later
authorization and is not performed in this pass.

Cleanup removed owned `/mnt/F/ri.poxzomwj`: builds, normal disposable clone and
test tags, extractions, inspections, superseded candidate files and fixtures.
Measured removal is **310,685,696 allocated bytes / 292,351,197 apparent bytes**;
61 protected earlier files and all 135 baseline core metadata records remain
unchanged. No task-owned process/core remains; owner catalog mode stays 0600.
Twelve deliberate artifacts remain in `build-codex-release-integrity-0.1.1/`:
canonical four exporter files, native package, public receipt, separate complete
RELEASE_SHA256SUMS; proposed notes, local receipt, unique/CRC-checked log ZIP and
local checksum receipt; STATE.json. The separate full receipt avoids mutating
the exporter's exact one-entry SHA256SUMS. Final cleanup and Git commit/sync
proofs belong to STATE.json and the local ZIP.

R-01 physical observations and the unavailable external artwork sample remain
NOT EXECUTED. R-02's owner license decision is complete; historical release
stays fixed and next candidate is prepared. R-03 retains unproved Mesa-worker
mechanism/teardown mitigation; R-04 retains independent finding provenance and
corrective verification labels. The supplied assignment stops mid-section 24
at “If expected payload”; the missing remainder was requested once and not
provided. These results cover the supplied requirements only.

## RC-01 and RC-02 release-provenance closure — 2026-10-04

Historical preceding correction/publication record; the license was still
unselected at this checkpoint, before the owner's 0.1.1 GPL decision above.

**Release-provenance correction and affected source/package gates PASS.
Owner-approved asset replacement and final remote provenance verification PASS.**
Starting clean `main`: `29e9f22e32d8381d95b0d7ca81c4e7fde1c57281`.
The latest supplied independent audit reports zero confirmed new runtime bugs,
closes AD-04-R1, and supplies RC-01/RC-02. These fresh fixing-agent checks are
corrective/candidate verification, not a separate independent review.

RC-01 is confirmed by actual clean-tag reconstruction. Annotated `v0.1.0`
remains tag object `50812852c4dc2726411a1c73452296955852c50a`, targeting
`c3b3a0b7771b313c45f843f49a503b45b0d1ada0` locally and remotely. Two exports
from a disposable clean checkout of that exact commit are byte-identical.
Their checkpoint HEAD equals the tag target and their checkpoint, gzip and
archive epochs equal the tagged commit epoch **1791062435**. The original
published source recorded `5a183b0` and epoch **1791058948**; its digest differed.
All 519 source-file byte hashes match the earlier candidate. One catalog's
canonical checkout mode is `0644`, rather than the earlier owner's `0600`;
the owner file is unchanged. Both Git provenance and canonical modes matter.

The root cause is the release generation boundary: source was exported before
the finalized correction commit and published afterward. Earlier uploaded-byte
checks did not establish reconstruction from the tag. Keeping similar source
bytes does not preserve checkpoint identity or normalized epoch. The fix keeps
the tag fixed and rebuilds source/package from its clean checkout; it does not
delete or falsify Git identity or alter the developer exporter.

[The small release verifier](../tools/verify-tagged-arch-source.py) checks clean
source, exact annotated tag/expected HEAD, checkpoint identity/epoch in both
copies, unique inventory, source bytes/modes, archive timestamps and recipe/sum
pins. Existing synthetic exporter fixtures now cover valid release source,
wrong HEAD, lightweight tag, tracked/untracked dirt, precommit checkpoint,
external checkpoint relabelling, and a dirty export after source restoration.
All **8 exporter unittest groups PASS**, and the registered exporter CTest is
**1/1 PASS**, 2.52 s. The guard accepts the corrected candidate and refuses
the downloaded original asset's checkpoint. Namespace, symlink, positive
nested-name, extraction and reproducibility controls remain enforced.

Fresh candidate gates: **519 source files, 520 unique members**, one regular
checkpoint without descendants, successful real extraction, full hashes/modes,
normalized metadata, two identical exports and matching recipe digest.
`makepkg --verifysource` passes. Fresh one-job Release/Quick3D-ON package build
passes in **385.65 s**; existing installed-test target build in **147.38 s**.
The unchanged scoped package helper passes in **27.29 s**: native install,
**209/209** bytes/modes against the 207-path manifest plus two package documents,
15+15 catalogs/actual cards, QtTest **8/8**, Quick3D-hidden **4/4**, zero
failures/skips, revision-1 upgrade, obsolete-file checks, both removals and
configuration preservation. Compiled package source/build fallbacks were
removed first; the owner checkout is masked during installed checks.

All **349 production/runtime/harness inputs**, toolchain/dependencies and
**208 resource payloads** match the prior verified candidate. Executable bytes
differ; no binary identity/equivalence claim is made. Effective configured
CTest coverage remains **107/107 names: one refreshed, 106 reused**. The prior
18 native CTests, 37 lifecycle phases and private installed startup/recovery
are **reused results**, not fresh invocations. No production C++/QML/CMake or
lifecycle harness changed, and no new private session ran. The external
artwork-sample subcase remains unexecuted without its supplied archive.

The preserved release identity is ID **402697537**, originally published at
**2026-10-03 21:23:54 UTC**, `draft=false`, `prerelease=true`, with the existing
title and fixed tag. All six original downloads matched GitHub's digests/sizes
before diagnosis. The owner's subsequent **“Approved”** authorized replacing
all six assets and applying the reviewed release body. Both actions completed;
the body was updated at **2026-10-04 00:21:58 CEST**.

Final remote verification at **2026-10-04 00:23:09 CEST** downloaded all six
replacement assets. Their bytes, sizes and SHA-256 values match the reviewed
files and GitHub digests; all five checksum entries pass. The downloaded
source equals both fresh clean-tag exports byte-for-byte. Its checkpoint HEAD
and epoch match the unchanged tag, and real extraction verifies all 519 source
files/520 unique members, hashes, modes and normalized metadata. The published
verification receipt's source/package identity and the exact reviewed body
also pass. The six published files and applied body are retained under
`build-codex-final-release-provenance/`; its `STATE.json` and local log ZIP
record before/after remote metadata, verification, cleanup and Git closure.

| Asset | Original published SHA-256 | Published replacement SHA-256 |
| --- | --- | --- |
| Source archive | `367c3314260bd31ddf268a49ef861472f25b800966f361681f375bf5f8f5b077` | `34abdc7fca9efcc1989a02abb47e774330c6f490c61715a2c82c63ce04295e9c` |
| Arch package | `7ef5cd2131bdedd63765c711f8148dd6ea4c7ba42510f8c30ed7970786333bcc` | `91db462260602e539beb6e21f18eff0456ae97e70121826491df1b453cd388ea` |
| PKGBUILD | `c99f79d3c90b276c37a2c884cce81a379aefdf2a143e2ebce152bffd37a20ac8` | `e24f6273cc8b71c44ca3366a37b5770d95c1391d455397a494a3aaeff1908b3e` |
| SOURCE_CHECKPOINT.json | `badbcc0c4771eb3cdb51d8fa380553471cbb8f3d80b956bc54f63673331f64a8` | `3db424ece8254c7cb6dc2a5ef6194baeb719a62562199034ca49ec48684de48d` |
| VERIFICATION.json | `4e22d5dbef634340d8ccff22857d1e9b0c618d7181471bd4e6162558e3b0947b` | `bb15f782b1f07f60b4e6d136654c0e164fef37d58ea5117baee99a6244732ca9` |
| SHA256SUMS | `d08ce467287cfa65dc55023620f88f84932ea3559091d45a3225536d06cdb667` | `a97b613f0623a01e98bb1680ae887449869be3043f96532298d69522bc122578` |

RC-02 corrects current README, changelog, limitations and platform prose;
current state/checklist record the verified published replacement. The
installation guide makes tagged verification mandatory
for the official path. Later main documentation/tooling changes do not rewrite
the fixed tag's historical source documents. R-01 remains **NOT EXECUTED**;
R-02 records completed tag/publication but unselected project-wide licensing
and completed owner-approved asset replacement; R-03 retains the two historical
TASK-0044 Mesa-worker disposal faults and unproved mechanism, with mitigation
wording; R-04 preserves independent finding provenance and self-run check labels.

## Earlier corrective and publication records

**Historical release-closure correction, 2026-10-03: AD-04-R1 software correction
and affected source/package verification PASS; R-01 through R-04 recorded.**
The owner subsequently authorized cleanup followed by commit/sync, then tagging
and publication. The correction is committed/synced as `c3b3a0b`, and annotated
`v0.1.0` points to that commit locally and remotely. The
[prerelease candidate](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
was published with six assets; downloaded bytes and checksums all match.
Physical acceptance and a project-wide license remain separate. Actual
Git/tag/release proof is recorded in
`build-codex-post-task-0045-release-closure/STATE.json`.

Assignment date: 2026-10-03. Initial clean `main` checkout:
`2889cae61deb4ab4ff255319e64098cc80d9a684` (initial corrective baseline).
The standalone assignment authorizes implementation and local verification;
the task-pack planning approval and one-correction stopping rules do not apply
to this pass. All other ownership, serial execution and preservation rules apply.
The owner subsequently instructed “Approved, also commit and Sync after it's
finished”, authorizing the task-local PySide6 prerequisite and Git closure.
One primary agent; one file/change at a time;
one build job and test worker; disposable native runtime only.

The original supplied attachment ended mid-sentence in section 9 after
`capability-driven fallback, and .bl`. Sections 10–11 (including the exact
R-01–R-04 definitions and final gates/cleanup) were requested from the owner.
Their absence did not prevent the fully specified issue work below. The subsequent
release-closure assignment now supplies R-01 through R-04 explicitly.

Previous specified corrective boundary: **AD-01 through AD-05 fixed; native and
full installed-package checks PASS**. All 107 configured CTest names have
applicable passing evidence, combining 65 refreshed names and 42 unchanged
checks from the initial source-matched pass. The full 37-phase lifecycle passes.
The release-closure assignment resolves the boundary definitions recorded below.
At that checkpoint, physical acceptance, license selection, tagging and
publication remained open owner boundaries. AD-04-R1 addresses the independently supplied descendant
namespace finding without reopening the other repairs.

The preceding corrective source/documentation closure was committed and synced
as `5a183b0f77bfa35d0ac2a3358951a90b07654697`. Its exact commit, clean-tree
and remote parity were verified after push in
`build-codex-post-task-0045-corrective/FINAL_STATE.json`. The older `STATE.json`
and preliminary receipts retain the initial blocked checkpoint.

## Issue register

| ID | Current applicability and requirement | Disposition/evidence |
| --- | --- | --- |
| AD-02 | Rollback committed restored physical IDs/revisions without journaling that set. Recovery must finalize its own verified durable set and refuse external edits. | Fixed at coordinator boundary; 17/17 QtTest cases and integrated coordinator test pass. Private native matrices and the full lifecycle pass. |
| AD-01 | Import discarded checked-save failure; Clear had no outcome. Artwork and Studio must report persistence failure and retain the draft. | Fixed; real settings-write/SettingsPopup regressions and complete registry/backend tests pass. Private native matrices and the full lifecycle pass. |
| AD-03 | Profile transfer published assets before later validation/final save. Rejected imports must clean only newly owned, unadopted resources and report cleanup failure. | Fixed; 24/24 profile-store QtTest cases and integrated store/manager tests pass. Private native matrices and the full lifecycle pass. |
| AD-04 | An exact root metadata file was also eligible input. Export must refuse exact-file collisions before publication and preserve normalized valid inputs. | Original file case fixed; four exporter tests and registered CTest passed. Old candidate locally checked consistent. The subsequent AD-04-R1 correction reserves descendants too. |
| AD-05 | Theme documentation made obsolete current-tense renderer/presentation claims. Guidance must match capabilities, build/runtime options and source. | Fixed; source/contract comparison, links/anchors and whitespace checks pass. |

Each completed entry below records requirement, cause, files, red/green
and control commands/results, and remaining boundaries. Historical 106-test,
37-phase and package receipts are references, not fresh corrective acceptance.

## Release boundaries

R-01 through R-04 are explicit release boundaries, not additional software
defects. The release-closure assignment supplies their definitions. The prior
approval authorized its corrective commit/sync, not publication. This run began
with project Git read-only. The owner then instructed “Also when u're done,
before report, do cleanup and commit and sync” and “U have my approval on tagging
and publication too”, authorizing those operations for the current candidate.

| Boundary | Current disposition |
| --- | --- |
| R-01 — physical acceptance | **NOT EXECUTED:** physical second-monitor connection/removal, connector/scanout, another GPU and owner-desktop observations. Private virtual-output checks do not close these cells. |
| R-02 — license/tag/publication | **AUTHORIZED TAG/PUBLICATION EXECUTED:** correction commit `c3b3a0b` is synced; annotated `v0.1.0` targets it locally/remotely, and the prerelease is published with all six asset downloads byte-verified. The project-wide license remains unselected; `LicenseRef-Arch-Dock-Unspecified` and existing component/asset declarations are preserved. The separate publication approval, not commit/sync alone, authorized publication. |
| R-03 — native fixture disposal | **HISTORICAL UNCERTAINTY RETAINED:** two TASK-0044 private PlasmaShell processes faulted in native Mesa workers during disposal. The underlying mechanism remains unproved. The verified teardown change is a harness mitigation; later passing fixtures do not prove the fault eliminated. |
| R-04 — independent review | **PROVENANCE RECORDED:** self-run tests are implementation/corrective/candidate verification. Independent review requires a separate review. The supplied independent findings retain their original provenance; this correction is not its own independent audit. |

See [physical observations and disposal history](PLATFORM_MATRIX.md) and
[known release limitations](KNOWN_LIMITATIONS.md).

## AD-04-R1 release-closure verification — 2026-10-03

Starting HEAD: `5a183b0f77bfa35d0ac2a3358951a90b07654697`, clean `main`.
The supplied independent finding concerned root descendants, not a regression
of the already repaired exact-file case. Git lists child source paths rather
than their parent directory. The exact-string guard admitted
`SOURCE_CHECKPOINT.json/notes.txt`, then generated the root checkpoint as a
regular file, requiring that archive path to be both a directory and a file.

Before editing production code, disposable repositories using the exact current
exporter reproduced both tracked and untracked direct/deep descendants: export
exit 0, followed by real `tarfile.extractall(..., filter="data")` failure with
`IsADirectoryError`, errno 21. Both exact-file controls remained rejected with
exit 2. All conflicting input bytes were preserved. Four new descendant subcases
then failed against the unrepaired exporter for its incorrect successful exit.

`tools/prepare-arch-source.py` adds a small `reserved_root_members` set and
checks the existing normalized first path component. It explicitly refuses the
root file and descendants before archive publication. Same-name paths below
other roots remain valid. The layout, generated filename, deleted-input handling
and symlink/path checks remain intact; there is no global basename ban.
Native Python path and extraction APIs were inspected before the change:
[path components](https://docs.python.org/3.14/library/pathlib.html#pathlib.PurePath.parts),
[tar extraction](https://docs.python.org/3.14/library/tarfile.html#tarfile.TarFile.extractall).

`tests/test_prepare_arch_source.py` preserves its existing controls and adds
tracked/untracked direct and deep descendants, positive
`test-data/SOURCE_CHECKPOINT.json` and nested same-name directories, including
`docs/SOURCE_CHECKPOINT.json/notes.txt`. The normal archive is actually extracted;
its single regular checkpoint matches external metadata, has no child members,
and the complete extracted inventory matches hashes/modes. Unique names,
deterministic ownership/epoch, repeated bytes, digest/recipe pin and existing
symlink/nonempty-output/source-change controls remain enforced.

- Targeted exact-file/descendant regression: **2/2 groups PASS**.
- Complete exporter Python suite: **6/6 groups PASS**, 1.36 s.
- Registered checkout `source-exporter-test`: **1/1 PASS**, 1.43 s.
- An initial optional CTest configure pointed at the extracted tree, which
  intentionally excludes `PKGBUILD`; fixture setup failed before exporter
  execution. Correcting the invocation to the actual checkout passed. Both
  setup failure and corrected result are retained; no production workaround.
- Two independent export operations produce identical bytes: **519 source
  files + one generated checkpoint = 520 unique members**. Real extraction,
  current bytes/modes, normalized ownership/epoch, inventory, digest and recipe
  pin all pass. This is local corrective verification, not independent review.
- Source SHA-256:
  `367c3314260bd31ddf268a49ef861472f25b800966f361681f375bf5f8f5b077`.
- `makepkg --verifysource`: **PASS**. Fresh one-job Release/Quick3D-ON package
  build: **PASS**, 428.76 s. Fresh existing installed-test target: **PASS**,
  169.19 s, one job.
- Package SHA-256:
  `7ef5cd2131bdedd63765c711f8148dd6ea4c7ba42510f8c30ed7970786333bcc`.
- Fresh scoped package checks reuse the unchanged full harness's blocks:
  **PASS**, 27.19 s; **209/209** payload bytes/modes, native install,
  `0.1.0-1 -> 0.1.0-2` upgrade, obsolete-file checks, both removals and exact
  configuration preservation. Installed **15+15** catalogs/actual cards pass:
  QtTest **8/8**, Quick3D-hidden **4/4**, zero failures/skips. Extracted source
  and Release build roots were verified then removed before those checks;
  the namespace also hides the checkout.

Only the exporter/test and two source-included boundary documents changed among
the 519 records; **515 source records and 349 runtime/harness inputs match**
the preceding verified candidate. Toolchain/dependency `.BUILDINFO` entries
match. All **208 resource payloads** match bytes/modes; executable bytes differ
and are not claimed identical. Its five compiled development fallback paths
name the new build root, with associated ELF layout variation. Simple raw
constant normalization was insufficient; no independent binary equivalence
claim is made. Reuse rests on actual production source/toolchain identity,
unchanged resources and fresh installed-resource checks after removing source
fallbacks. Private installed startup/recovery and native matrices remain
explicitly reused, including **18 native CTests and 37 lifecycle phases**.
Effective configured coverage is **107/107 names: one refreshed exporter CTest
and 106 unaffected results reused**, not a new full-suite/private invocation.
The external artwork-sample subcase remains unexecuted without its archive.

The actual historical TASK-0045 and preceding corrective archives were locally
revalidated: respectively 519 and 520 unique members, one regular checkpoint,
no reserved descendants, and matching manifest bytes/modes. They are not
claimed corrupt. Prior failure logs/receipts remain unchanged. Source and
package versions remain `0.1.0` / `0.1.0-2`; no dependency download/install,
personal-desktop mutation or delegated review occurred. Source-directory and
desktop-category warnings remain visible.

Fresh deliverables are deliberately retained under
`build-codex-post-task-0045-release-closure/package-output/` and
`verification-output/`; `STATE.json` records cleanup and subsequent authorized
Git/tag/publication closure. The earlier 40 protected artifact files retain
their original hashes. Raw reproduction, red/green, setup/discrimination,
source/package, scope/reuse and cleanup receipts remain in the local log ZIP.

### Authorized candidate publication and closure

The reviewed eight-path correction was committed and synced as
`c3b3a0b7771b313c45f843f49a503b45b0d1ada0`, with a clean worktree and matching
`origin/main` and remote `main`. Annotated `v0.1.0` has tag object
`50812852c4dc2726411a1c73452296955852c50a` and peels to that correction commit
both locally and on origin. Subsequent documentation closure does not move it.

The owner-authorized
[GitHub prerelease](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
was published at **2026-10-03 21:23:54 UTC**, release ID `402697537`, with
`draft=false`, `prerelease=true` and explicit `latest=false`. Its six assets
are the normalized source, checkpoint, pinned recipe, Arch package,
`VERIFICATION.json` and `SHA256SUMS`. Every downloaded asset matches local
SHA-256 and size; the downloaded checksum file verifies its five entries.
The public release notes exactly match the reviewed notes. Raw local gate
logs are deliberately retained locally rather than published as release assets.

The normalized source checkpoint intentionally records starting HEAD `5a183b0`
and hashes all 519 approved precommit source inputs. Operational closure
documents and the root recipe are excluded by existing exporter rules to avoid
circular hashes. Publication does not alter the verified source/package bytes
or establish physical acceptance, select a license, prove the historical
native fault eliminated, or constitute independent review. The current
`STATE.json` and archived closing receipts record the final documentation
commit/sync, remote parity and removal of the publication-only scratch root.

## Approved native and package continuation

Historical checkpoint from the preceding corrective pass; the hashes and full
runtime/package results below identify that candidate.

The approved disposable Python environment used existing `uv`, system Python
3.14.7 and GI, and binary-only PySide6/shiboken6 6.11.2 matching host Qt 6.11.2.
Downloads/installs were serial, with a task-owned cache. No host dependency or
personal desktop configuration was installed or changed. A fresh Debug/Quick3D-ON
application and five needed test targets built in four one-job groups; affected
compiled fixture changes were rebuilt individually. Shared QML module copies
were refreshed after each renderer change.

The first 18 native CTests passed, then the mandatory full lifecycle failed.
Diagnostic reruns traced slow native D-Bus calls to Plasma's GUI thread replaying
Canvas shadow painting (`ShadowImageMaker::paintShapeAndShadow`). The traced
service waits were consequences of that work. `PanelProcedural2D.qml` now uses
`Canvas.Threaded` for the same image commands. Qt documents that Cooperative may
use the GUI thread, whereas Threaded defers commands to a private worker.
[Qt Canvas contract](https://doc.qt.io/qt-6/qml-qtquick-canvas.html).

The existing preset oracle could capture four stable icon frames before the
asynchronous surface finished. The discriminating image pair showed an absent
rail/glow in the first image and the completed surface in the second.
`PresetLibraryTest.cpp` now observes the real procedural Canvas `painted()`
signal before its unchanged four-frame settlement and exact repeated-pixel
comparison. Color diversity, distinct cards, energy controls, all cases, the
100-attempt loop and 20 ms interval remain. Failed and intermediate attempts are
retained; final full QtTest is 13 PASS, zero failures/skips, and installed
controls also pass. Optional failure images/counts aid diagnosis.

A subsequent native mesh check reported no frame despite a visible, ready scene.
A temporary probe in an owned staged applet proved raw `frameTime = 5.963714 ms`
while the bound flag was false; it did not force frames. Qt 6.11.2 throttles
statistics notifications to a 200 ms interval; a static scene can stop
rendering before that notification. `PanelScene3D.qml` additionally checks real statistics after
window `frameSwapped`, using `Qt.callLater` after frame completion. Readiness
alone never sets the flag. The original native gate then observed actual
low/high/low frames, restart frames and the required triangle/target budgets.
[Qt 6.11.2 statistics implementation](https://raw.githubusercontent.com/qt/qtquick3d/v6.11.2/src/quick3d/qquick3drenderstats.cpp).

That exact gate next timed out after printing success: a private KDE
`plasma_waitforname org.kde.KSplash` activation waiter retained CTest's pipe.
`run-rendering-import-smoke.sh` reuses its owner-verification helper to clean
only waiters with matching UID, executable, private bus and all seven private
directory identities, using pidfd signalling. The source-matched CTest passed
in 61.64 s and logged the exact waiter cleanup; no timeout changed.

The shortcut startup fixture also exposed premature scripting after name
registration. The trace returned a sentinel panel with screen -1 before
Plasma's desktop initialized; another startup failed before Arch Dock launched.
`run-plasma-lifecycle.sh` now queries a real current activity and desktop before
fixture mutation, sharing its existing 20-second startup deadline. Only the
idempotent readiness query may retry a startup timeout; mutation calls retain
their three-second limit. A readiness failure then distinguished the synthetic
desktop's slow GTK portal fallback, which delayed activity initialization.
Qt's documented-in-source `QT_NO_XDG_DESKTOP_PORTAL` route passed the discriminator
and is set only inside the disposable fixture. Production startup metadata is
unchanged; real Plasma/KWin/D-Bus/GlobalAccel assertions remain.
[Qt 6.11.2 desktop-service control](https://raw.githubusercontent.com/qt/qtbase/v6.11.2/src/gui/platform/unix/qdesktopunixservices.cpp).

Two chronological shortcut results and the complete failed private-session log
remain. One transient failed readiness CTest stdout/JUnit filename was reused
before it could be copied; this retention limitation is explicit in
`receipt-retention-note.json`. It does not affect the 30 protected earlier
artifacts, the retained failure session, or final acceptance receipts.

Final source-dependent verification:

- 41 QML CTests, full preset/backend UI targets and host-neutral module check: PASS.
- All 18 native CTests: PASS for the final relevant source. Includes windows, folders, profile apply/shortcuts, five audition groups, upgrade, four scales, hotplug, resources and startup.
- Original full standalone lifecycle: exit 0, **37/37 phases**, 113.08 s, original 900-second bound and assertions; no tracer, diagnostic preload or staged probe.
- Final exporter plus software/missing-module fallback checks: **3/3 PASS**.
- Effective configured coverage: **107/107 CTests**, 65 refreshed, 42 initial checks reused against unchanged dependencies. This is not a claim of one new full-suite invocation.

Only five of the initial 519 source records changed in this continuation:
`PanelProcedural2D.qml`, `PanelScene3D.qml`, `PresetLibraryTest.cpp`, and the two
private harnesses. The other 514 bytes/modes match the preliminary checkpoint;
initial AD coordinator/registry/store regressions and unaffected model/static
checks remain applicable. Full offscreen backend QtTest retains its existing
private-KWin row skip; that row passes in the actual native smoke. The existing
external source-sample catalog row remains unexecuted because no sample archive
was supplied. CTest itself has no failed/skipped/not-run final names.

Two new exports are byte-identical, with **519 source files + one generated
checkpoint = 520 unique members**, exact current bytes/modes, normalized owners
and timestamps. Source SHA-256:
`539f46e318bba0f6fc6993235cf1fb350176fac9e077c5efb9582ceefa03fc36`.
The checkpoint explicitly records approved working-tree inputs at baseline HEAD;
operational report/checklist/current-state/PKGBUILD exclusions avoid circular
receipt hashes. At that checkpoint, root `PKGBUILD` pinned this verified checksum.

`makepkg --verifysource` and a fresh one-job Release/Quick3D-ON package build pass.
All 519 extracted source hashes/modes were verified, then the owned extracted
source and Release build were removed before the unchanged complete
`run-arch-package-smoke.sh` executed; its namespace also hides the checkout.
Full package gate: exit 0, **56.65 s**.

- **209/209** installed payload bytes/modes, 207 CMake paths plus installation/licensing documents.
- **15+15** catalogs and actual shared-renderer cards; installed QtTest **8/8**, Quick3D-hidden **4/4**, no failures/skips.
- Installed private D-Bus startup with and without Quick3D, repeated owner/host checks and upgraded startup: PASS.
- Native disposable pacman install, `0.1.0-1 -> 0.1.0-2` upgrade, obsolete-file checks, real installed configuration migration/backup/restore/recovery/live-owner refusal, both removals and exact configuration sentinels: PASS.

Final package SHA-256:
`172a780e95f5bc17ac9c10c69ffbf0c416e1b4cb94068f8895adaa308ae1dfe0`.
Source-directory-reference and desktop-category warnings remain visible.
These are private runtime/installed-candidate results, not physical release
acceptance or proof of portal-mediated desktop behavior.

The final compact package is in `verified-package-output/`; full continuation
receipts/logs are in `native-verification-output/`, under the existing corrective
artifact directory. Ten preliminary files remain byte-unchanged; ten final
files are added, for twenty deliberate corrective artifacts. All 20 older
protected project artifacts remain byte-unchanged. Task-owned builds, extracted
source, Python/cache, diagnostic helpers, fixtures and private session roots are
removed; cleanup and subsequent Git parity are recorded in `FINAL_STATE.json`.

## Initial corrective pass receipts

The sections below preserve the first pass's focused red/green work and its
then-unavailable native prerequisite. Those blocked/subset statements are
historical; the continuation above supplies final native/package evidence.

## AD-02 executed evidence

Cause: `rollback()` published restored host IDs/revisions while durable recovery
still named the original backup. Added `ROLLBACK_COMMITTING`/`ROLLED_BACK`
phases using the existing version-1 structure and candidates. The exact restored
set is journaled before committing; recovery requires an exact registry match
and verified owned hosts. Cleanup failure retains the phase. No guard removed.
Files: `src/panel/ProfileApplyTransaction.cpp`,
`tests/ProfileApplyTransactionTest.cpp`, `docs/PROFILE_PACKAGE.md`.

Fresh Make Debug/Quick3D-ON build: `/mnt/F/ac.RAKP7rXA/build`, one job.
Initial Ninja configure failed because Ninja is absent; Make configure passed
with the existing Qt/Kirigami plugin-target warnings. No dependency installed.

- `cmake --build /mnt/F/ac.RAKP7rXA/build --parallel 1 --target profile-apply-transaction-test`: passed after each compiled slice.
- Baseline `profile-apply-transaction-test restoredCommitRecovery`: 4 pass,
  2 fail (QtTest exit 2), both restart/cleanup cases rejected their own durable
  restoration with `profile-registry-revision-conflict`. The shell initially
  returned the following `cat` status; the QtTest failures are preserved.
- First repair: 13 pass, 2 fail. Diagnosed an unnecessary post-commit journal
  write and a fixture oracle that included registry revision in physical
  verification. Production `PanelWindow::profileOperations` verifies native
  capture, not registry revision; corrected only that fixture distinction,
  retaining all physical ID/ownership comparisons. Intermediate failures retained.
- Final `profile-apply-transaction-test`: exit 0, **17 pass, 0 fail/skip**.
  Durable post-commit interruption and actual journal-removal refusal recover
  without duplicate hosts. Pre-commit failure finalizes the verified prepared
  restored set. Repeated/new-coordinator recovery is idempotent. Ordinary and
  partial rollback, external revision and ownership controls pass. Supported
  earlier phases recover; malformed/future records remain byte-preserved.
- `git diff --check -- docs/PROFILE_PACKAGE.md`: exit 0.

## Verification and cleanup

All 20 protected TASK-0043/0044/0045 package/evidence files remain byte-unchanged.
The new deliberate output is `build-codex-post-task-0045-corrective/`: six package
files, three verification files and `STATE.json`. The verification ZIP contains
110 raw log/receipt entries, including failed red/diagnostic attempts, JUnit,
inventory, installed payload and the exact temporary package-control script.
All ten deliberate files are retained; the entire owned `/mnt/F/ac.RAKP7rXA`
workarea is removed. No task process was active before removal; the package
fixture's native root was already removed by its trap. No retained build,
source extraction, Python environment, native session or temporary fixture.
No personal desktop mutation, dependency installation or host-global install.

## AD-01 executed evidence

Cause: Import discarded `setPanelValuesChecked()`'s false result; Clear and
Studio treated a void invocation as completed. Import now checks adoption and
cleans only a newly published package (including analysis previews), preserving
`reusedExisting` packages. Clear returns its checked outcome and retains render
requests on failure. The existing theme status exposes a transient useful
error without attempting another failed persistence operation. QSettings'
pending serialized value is restored on save failure so it cannot adopt the
rejected candidate on a later sync. Studio retains the artwork draft and stays
open, and distinguishes artwork-only failure from committed settings followed
by failed artwork. `renderTheme()` still accepts requests independently of
render completion.

Files: `src/PanelRegistry.cpp`, `src/PanelRegistry.h`,
`qml/runtime/SettingsPopup.qml`, `tests/PanelRegistryTest.cpp`,
`tests/PanelWindowCapabilityTest.cpp`. Declaration/definition of Clear's bool
result were edited sequentially as one API slice, then rebuilt together;
existing callers that ignore the result remain compatible.

- Fresh one-job `panel-registry-test` builds passed. Baseline three regressions:
  QtTest exit 3, **3 failures** (both imports falsely true after QSettings
  rejection; Clear's void result cannot satisfy a bool invocation).
- Final `panel-registry-test artworkImportPersistenceFailure artworkClearPersistenceFailure`
  with `QT_QPA_PLATFORM=offscreen` and scratch `XDG_DATA_HOME`: exit 0,
  **5/5 cases pass**, including init/cleanup. Real owner-read-only settings
  refusal leaves prior live references/revision and exact durable bytes;
  a new registry loads the old theme. New rejected packages are removed,
  existing shared packages survive; successful import/Clear retries pass.
- Fresh one-job `panel-window-capability-test` builds passed. The first Studio
  import control was an unchanged-theme no-op (no write rejection); changing
  persisted status made the test discriminate a real adoption write. Corrected
  baseline: QtTest exit 3, three failures (Clear closes/discards; import and
  partial settings/artwork omit useful persistence detail).
- Final actual `SettingsPopup` test `studioArtworkPersistenceFailure`: exit 0,
  **5/5 cases pass**. Three workflows verify visible window, retained draft,
  useful save error, durable previous theme and successful retry; the partial
  case verifies the settings opacity remains committed across a new registry.
  This is executed offscreen backend/UI evidence, not native Plasma evidence.
- `cmake -DSOURCE_DIR='/mnt/F/Arch Dock' -P tests/ValidateStudioPreview.cmake`:
  exit 0. QSettings rejection warnings are expected failure evidence; existing
  missing animation-catalog and deliberately absent private D-Bus warnings are
  recorded in logs, not suppressed.

Native prerequisites: default Python has no PySide6, and local environment/cache
inspection found no usable copy. Task-local pinned PySide6 installation was
requested under assignment section 3; no installation has been performed.

## AD-03 executed evidence

Cause: transfer published each package without retaining ownership until the
profile's final save. `ProfileStore::importProfile()` now preflights the safe
store, capacity and final local identity/name/definition, and carries newly
published roots through transfer and adoption. It reuses the materializer's
`reusedExisting` result. Rejection removes only new roots after examining
durable profile references; shared or uncertain ownership and filesystem
cleanup failure produce an explicit error with original cause/retained paths.
No asset-root deletion or general garbage collector was introduced.

Files: `src/persistence/ProfileStore.cpp`, `src/persistence/ProfileStore.h`,
`tests/ProfileStoreTest.cpp`, `docs/PROFILE_PACKAGE.md`. The optional checkpoint
controls real filesystem permissions/other durable writes after preparation;
tests still execute production transfer, materialization, save and cleanup.

- Fresh one-job `profile-store-test` builds passed after each compiled slice.
- Baseline `profile-store-test rejectedImportsCleanOnlyNewArtwork`: exit 4,
  four failures. The full-store, later-invalid-asset and actual final-save
  rejection each left a new package; mixed shared/new transfer left the new
  package alongside the existing one.
- Final full `profile-store-test`: exit 0, **24 pass, 0 fail/skip**. All four
  rejected-import managed-directory comparisons pass; existing shared bytes,
  valid package loading and owning profile remain unchanged. Successful imports
  load with usable resources. Cleanup permission failure is explicit; another
  durable owner and malformed-record uncertainty protect the package and report
  retained paths. The genuine final-save refusal uses read-only store-parent
  permissions while its existing assets directory remains writable.
- Profile management's existing `finish(profile, code)` propagates the false
  outcome and precise code; no second manager/import framework was required.
- `git diff --check -- docs/PROFILE_PACKAGE.md`: exit 0.

## AD-04 executed evidence

Cause: root `SOURCE_CHECKPOINT.json` could be input and generated output at the
same archive path. The current exporter refuses admitted tracked/untracked
exact-file collisions before publication, preserving the original file. Required symlink
controls also proved dangling links were treated as removed files; symlink
refusal now precedes the missing-file check. Existing exclusions and approved
working-tree/deletion inputs remain supported. The new corrective report uses
the existing narrowly named operational-document exclusion policy, avoiding
circular source/evidence hashes.

Files: `tools/prepare-arch-source.py`, `tests/test_prepare_arch_source.py`,
`CMakeLists.txt`, `docs/INSTALL.md`. Tests copy fresh current exporter bytes into
owned synthetic repositories; no archived faulty copy is executed. Git init,
add and commits occur only inside these test fixtures with fixed test identities
and epoch; project Git remains read-only.

- Baseline isolated exporter tests: exit 1, four subcase failures (tracked and
  untracked reserved metadata exported with exit zero; both dangling-link
  placements were silently accepted). The valid normalized-export control passed.
- Final `TMPDIR=/mnt/F/ac.RAKP7rXA PYTHONDONTWRITEBYTECODE=1 python3 tests/test_prepare_arch_source.py`:
  exit 0, **4/4 unittest groups pass**. Controls include both admitted metadata
  placements, all four tracked/untracked regular/dangling symlink cases, unique
  member paths, exact inventory/source bytes and modes, normalized ownership and
  epoch, exact repeat archive bytes, pinned recipe/checksum, source/deletion and
  compressed-fixture support, exclusions, legitimate nested metadata, and
  preservation of nonempty output directories.
- Fresh CMake configure: exit 0; `ctest --parallel 1 --output-on-failure
  -R '^source-exporter-test$'`: exit 0, **1/1 pass**. Available registration is
  now 107 CTests, not the historical 106.
- Retained TASK-0045 archive inspection: **519 unique members, 518 source
  entries, exactly one generated metadata member**; every inventory byte/mode
  matches the archive, with normalized ownership/epoch. SHA-256 remains
  `bc462317d2e43103e2260df50f8e20e93d87944c978a9677452a6b864149dcb5`.
  The historical actual artifact is consistent; no corruption claim is made.
- `git diff --check -- docs/INSTALL.md`: exit 0.

## Native mechanism research

Official upstream references inspected before code changes:
[Plasma scripting API](https://develop.kde.org/docs/plasma/scripting/api/) and
[Qt QSaveFile](https://doc.qt.io/qt-6/qsavefile.html). Plasma creation/removal
uses native containment/widget identities; atomic configuration replacement is
per file, so the existing coordinator must journal the intended restored set
before a separate registry commit. Keep atomic QSaveFile behavior without
direct-write fallback for application configuration.

## AD-05 executed evidence

`docs/theme-packages.md` now describes implemented optional true 3D, separate
build inclusion/runtime capability, declared safe fallback, detailed editor
gating, native/free host restrictions and the working shared presentation
controller. Offline `.blend` retention/non-execution remains unchanged.
Compared with `KNOWN_LIMITATIONS.md`, `INSTALL.md`, `THEME_PACKAGE_V2.md`,
`shared-renderer.md`, `PKGBUILD`, `PanelCapabilityResolver::productionRenderers`,
the optional scene and shared presentation state contract. The current
Quick3D-ON configure also separately confirms build inclusion; that is not
claimed as new runtime/physical acceptance.

The initial documentation patch failed to match an exact line and made no
change; re-reading the paragraph allowed a precise correction. `git diff
--check -- docs/theme-packages.md` and the four local file/link/anchor checks
exit 0. Both obsolete current-tense claims are absent. No renderer behavior or
physical acceptance requirement was changed to reconcile documentation.

## Fresh integrated and source checks

The fresh Make Debug/Quick3D-ON application build, eight bounded test-target
groups and final all-target build all exit 0 with `--parallel 1`. No source
implementation changed after these builds. The complete configured inventory
is **107 CTests**. Fifteen serial invocations cover **89 distinct PASS, zero
CTest FAIL/skipped**, with **141.16 seconds** summed selected test times.
The union of those executed names and the 18 prerequisite-blocked names equals
the configured inventory exactly; none is silently omitted. This is incomplete
107-test acceptance, not a passing full-suite claim.

The full Studio/backend target reports 36 QtTest PASS and one existing internal
skip, `groupedWindowsFollowLiveKWinUpdates`, which requires private KWin
interaction. The source catalog target reports eight PASS and one existing
internal skip for an external sample-artwork archive/root not supplied to that
invocation. Neither case is new or weakened. The actual new Studio persistence
cases execute; the live KWin counterpart remains among the blocked native gates.
The complete registry, profile store, coordinator, preset rendering, QML,
contract, staged install and startup diagnostics checks pass.

Executed native preflight:
`ARCHDOCK_BUILD_DIR=/mnt/F/ac.RAKP7rXA/build TMPDIR=/mnt/F/ac.RAKP7rXA/t
bash tests/run-plasma-lifecycle.sh` exits 1 with `ModuleNotFoundError:
No module named 'PySide6'` and the harness's explicit QtCore/QtGui prerequisite
diagnostic, before its private-state directory is created. No native runtime
result is substituted. Blocked names are rendering-import, window-interaction
and folder-interaction smoke; both profile matrices; all five preset-audition
matrices; configuration upgrade; the six Wayland-hardening matrices; and
session-startup runtime. Full standalone lifecycle and installed package runtime
also need this module. Installation authorization requested under assignment
section 3 remains pending; no Python dependency installed.

Current exporter commands, both exit 0:

```bash
python3 tools/prepare-arch-source.py /mnt/F/ac.RAKP7rXA/package
python3 tools/prepare-arch-source.py /mnt/F/ac.RAKP7rXA/export-repeat
```

The actual current working tree exports **519 source files plus one generated
metadata member**, **520 unique archive paths**. Both normalized gzip archives
are byte-identical. All archive bytes/modes match inventory and current source;
ownership and epoch are normalized; outer/generated checkpoint bytes agree.
Archive SHA-256:
`438cbabfb746b85f0e549926c66de0d3a6ace62d975d82784571b2aabd57835f`.
The additional source file is the exporter regression; no count was forced.
Metadata retains baseline HEAD and separately identifies actual working-tree
bytes. The outer local recipe pins this new archive; the checkout recipe still
identifies the earlier candidate. `makepkg --verifysource` and the fresh
one-job Release `makepkg --cleanbuild --noconfirm` both exit 0, isolated from
all earlier outputs. Application/package remain `0.1.0`/`0.1.0-2`; this is a
local corrective working-tree candidate, without a release version decision.

Raw batch commands/results, JUnit files, inventory and source verification are
retained in `build-codex-post-task-0045-corrective/verification-output/`.

## Package controls and remaining acceptance

The owned task-local `package-controls.sh` reuses the unchanged prologue,
native pacman, inventory/metadata, rendering, user-configuration preservation,
upgrade and removal blocks from `tests/run-arch-package-smoke.sh`. Its only
binding adjustment is the absolute checkout root for the temporary script.
The original full harness is unchanged. This explicitly labeled subset lets
safe checks after the blocked startup boundary execute; it neither removes
required cases from the original harness nor certifies that full gate.
Provenance, exact subset bytes and results are archived.

Before these controls, all 519 extracted package source hashes were compared
with the checkpoint, then only the owned extracted source directory was
removed. The existing namespace also hides the checkout. Installed-resource
controls therefore cannot fall back to either compiled source location.

- Native makepkg Release/Quick3D-ON package build: exit 0.
- Dependency-enabled native pacman install: PASS in the disposable user
  namespace; **209/209** payload bytes/modes and complete CMake resource
  inventory match. No host installation or database mutation.
- Installed preset/reference/rendering QtTest: **8 PASS, 0 FAIL/skip**,
  including all 15 Panel and 15 Icon cards. With Quick3D hidden:
  **4 PASS, 0 FAIL/skip**. These are installed offscreen rendering controls,
  without a physical/native Wayland rendering claim.
- Native removal: PASS; all package files gone and both disposable user
  configuration sentinels byte-preserved.
- Native previous-package install and upgrade `0.1.0-1 -> 0.1.0-2`: PASS;
  version comparison, candidate payload/obsolete-file audit, sentinels and
  final native removal all pass.
- Makepkg's source-directory-reference warning and desktop metadata's
  multiple-main-category hint remain visible in receipts; no suppression.
  The owned extracted source was absent during installed resource checks.

Package SHA-256:
`badb2b509881879715d142fedacbf34d59aeed5143c23a6e614ebb20ad562b9a`.
The full installed startup (normal/Quick3D-hidden), installed configuration
upgrade/recovery, upgraded startup, 18 blocked CTests and standalone 37-phase
lifecycle remain **not executed**, requiring PySide6. The executed lifecycle
preflight failure is preserved separately. Assignment section 3 requires
explicit authorization for new dependency installation; the requested
task-local pinned PySide6 installation has not been authorized or performed.
The missing sections 10–11 also prevent precise closure of R-01 through R-04
and any additional acceptance/cleanup instructions. Existing safe task-owned
cleanup is executed rather than left pending. No physical acceptance, license,
tag, publication or owner Git closure is inferred from these subset results.
