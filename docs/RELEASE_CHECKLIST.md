# Arch Dock release checklist

This checklist is derived from section 22 of the
[master architecture and implementation plan](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
An item remains unchecked until its owning task records fresh, reproducible
evidence. Source presence, a historical result, or this checklist itself is not
release evidence.

See [CURRENT_STATE.md](CURRENT_STATE.md) for the current implementation boundary,
[PRESET_SYSTEM_SPEC.md](PRESET_SYSTEM_SPEC.md) for preset and audition gates, and
[TARGET_STRUCTURE_TREE_V2.md](TARGET_STRUCTURE_TREE_V2.md) for the logical target.

The original post-TASK-0045 corrections and subsequent AD-04-R1 namespace repair
have passing corrective evidence in the
[corrective report](POST_TASK_0045_CORRECTIVE_REPORT.md). The release-closure
assignment supplies all four boundary definitions. The owner subsequently
authorized cleanup/commit/sync and separately tagging/publication. Physical
acceptance remains separate; the owner now selected GPL-3.0-or-later. Outside the explicit
corrective sections, build/runtime entries retain historical candidate evidence.

## Icon Tiles closure — 0.1.1-3, 2026-10-04

- [x] Unavailable placeholder replaced with selected-panel controls for visibility, style/custom mode, five shapes, fill/opacity and border.
- [x] Existing typed schema, revision-checked transaction, runtime configuration and profile persistence reused; per-entry visibility and original glyphs preserved.
- [x] Actual draft preview, Cancel, Apply and fresh persisted reload verified through native/free Studio controls.
- [x] Native 2D shape pixels and real RHI 3D tile color/shape/visibility checked without changing logical entry geometry.
- [x] One-job complete Debug/Quick3D-ON build and fresh 111/111 configured CTests across 37 serial batches; all four native Wayland gates PASS.
- [x] Earlier control/scroll/drag fixture failures diagnosed and corrected without weakening acceptance assertions; failed evidence retained.
- [x] Source freeze f51f239c83fd7667f71ce0351887f80f2921c844; two canonical exports identical; disposable annotated verification only; 527 source files / 528 members; extraction/bytes/modes/GPL/reference-image exclusions pass.
- [x] Source SHA-256 71dfcb9ea8cd43a9d9c7aa74fb75d950730db785e7a40518c63a1db7c0186d66; canonical pinned root recipe and fresh one-job Release/Quick3D-ON 0.1.1-3 package.
- [x] Package SHA-256 0f632369f5f0a8131b62d7fd60a3f22226e79b87537453532a34305520ab0f31; 216/216 installed payload bytes/modes; licensing/resources, 15+15 cards, startup with/without 3D and real installed UI pass after source removal/masking.
- [x] Native 0.1.1-2 -> 0.1.1-3 upgrade, recovery, both removals and configuration preservation PASS.
- [x] Final owned build/venv/fixtures removed; 162 compact evidence entries retained; all 928 protected historical files unchanged; no task processes/new dump records or payloads.
- [ ] Owner PC 0.1.1-3 update and running-backend/new tile field verification; pending operational Git sync.
- [ ] Owner manual visual/physical acceptance; separate from private runtime evidence.

## First owner-observed runtime/UI correction — 0.1.1-2, 2026-10-04

**FIRST OWNER-OBSERVED RUNTIME/UI ISSUES CORRECTED AND VERIFIED.** This section
supersedes the older candidate identity and runtime reuse boundaries below.

- [x] Actual starting main f7b72859b5410f57669c5c56dbda1dfd2861706b and installed 0.1.1-1 reconciled; owner folder/circular-panel/Panel-page observations reproduced.
- [x] Drag delivery/receipt, internal drag completion, guards and backend rejection corrected without changing native/free ownership or Empty panel semantics.
- [x] KFileItem resolves native folder artwork; real RHI glyph-pixel regression corrects platform occlusion without replacing original icons/styles.
- [x] Natural page wheel routing preserves control values; real horizontal overflow supports pixels/angles/Shift, existing bars/keys and bounds without artificial overflow.
- [x] Complete one-job build; 22/22 affected CTests; fresh full 110/110 configured CTests; no failures/skips/missing names.
- [x] Final 4/4 native Wayland checks, 131.60 s, after fixture source-readiness and final private teardown repairs; no new crash.
- [x] Final source-exporter CTest 1/1, 6.58 s, all 11 unittest groups after recipe release bump.
- [x] Exact source freeze 85614f7f6ffed1bdba73528483b5e1089ba53f2f; annotated verification tag only in disposable clone; two canonical exports identical; 525 source files / 526 regular members; GPL/modes/epochs/reference exclusions pass.
- [x] Source SHA-256 daab73ef0dcd0f5fb940762a3e89f6ce7d346c0c95ff55ee8a8556a7e4c03607; exact root canonical recipe; fresh one-job Release/Quick3D-ON 0.1.1-2 package.
- [x] Package SHA-256 a5020f57699660f5f1e2e5caf1179412cd93047d73f0b244263db1c147726d0e; 215/215 payload bytes/modes and all license/resource coverage.
- [x] Real installed drag/icon/scroll matrix hides checkout/build; copied probes/UI fixtures are explicit; installed applet/modules/themes/executable used.
- [x] Native fresh install, installed cards/startup with/without 3D, 0.1.1-1 -> 0.1.1-2 upgrade, configuration recovery, both removals and byte-preserved configuration pass after removing compiled/extracted source.
- [x] Owned workspace/build/venv/fixtures removed; 243 compact evidence entries retained; all 912 protected historical files byte/mode identical; no task processes/dump payloads.
- [x] Exact private Mesa teardown dump removed with diagnostic evidence retained; 458 baseline journal identities remain; one older payload externally became missing, consistent with native retention.
- [x] Full 28-field corrective report, exact changed paths and short owner manual checks recorded. Final operational Git and owner update receipts are in build-codex-first-runtime-ui-0.1.1-2/STATE.json.
- [x] Owner PC upgraded after cleanup/commit/sync and fresh private configuration backup; 0.1.1-2, actual 215 payload bytes/modes, pacman Qkk, new executable owner, D-Bus startup and Panel Studio launch PASS.
- [x] Owner-approved desktop refresh completed through managed Plasma service recovery after native detached replacement aborted; exact incident diagnostics retained and its system/DrKonqi dump payloads removed.
- [x] Owner Arch Dock configuration byte-identical; other Plasma logical keys unchanged except native panelWidgets cache and slideshow current-image state. Detached saved free records and Empty free-2 preserved; no hosted-widget acceptance claimed.
- [ ] Owner Dolphin gesture, physical touchpad/GPU and manual restart/persistence acceptance; remains distinct from private Wayland verification.
- [ ] New real v0.1.1 tag/publication; not performed during this runtime pass. Historical v0.1.0 is unchanged.

## Historical RC-03, RC-04 and licensing acceptance — 0.1.1 candidate, 2026-10-04

**All supplied code/export/source/package/installed licensing and owned cleanup
gates PASS. Next candidate prepared; no new real tag/publication authorized.**

- [x] Actual clean starting main `4d65678d53dd5d0e9f388bbda6bf43117427b1ea` reconciled with audited `b6fb0b4`; newer publication-closure documents preserved.
- [x] Historical annotated `v0.1.0` object/target and current six assets/body recorded for preservation; no move or replacement permitted in this pass.
- [x] RC-03/RC-04 before-repair reproductions confirm accepted recipe/tar/gzip tampering and checkout `0666` mode from clean source.
- [x] Verifier regenerates and requires exact archive, external checkpoint, PKGBUILD and SHA256SUMS bytes; semantic checks retained.
- [x] All 11 exporter/release unittest groups pass, including valid source, 17 artifact-tampering subcases, repaired receipts, wrong HEAD/tag, lightweight tag and dirty source.
- [x] Git canonical mode matrix covers ordinary 0600/0644/0664/0666 and executable 0700/0755/0775/0777; all four artifacts remain identical, checkpoint/extracted modes are canonical. Developer-only policy and namespace/symlink controls pass.
- [x] Owner selected GPL-3.0-or-later; complete official GNU text added byte-for-byte, verified against GNU web/FTP copies. Source header conventions preserved.
- [x] License matrix audits original C++/QML, eight preserved MIT Plasma/KWin components, 15+15 presets, all 17 original asset packages, system dependencies and excluded unknown-rights references. Each asset declaration change has its own provenance reason.
- [x] Application/package identity is 0.1.1/0.1.1-1; native `vercmp` confirms upgrade ordering. CMake installs GPL text and complete MIT/matrix notice; package declares GPL-3.0-or-later and separately bundled MIT.
- [x] Finalized candidate source commit `96f0e4f60d024b5cb1a44af1402401656ded1372` recorded; annotated verification tag was created only in a disposable repository, with expected HEAD exactly that commit, and removed during owned cleanup.
- [x] Two complete exports match byte-for-byte; verifier, real extraction, complete inventory/canonical modes, normalized ownership/tar/gzip epochs, exact recipe/sums and included LICENSE pass. Reference images remain absent.
- [x] `makepkg --verifysource` and fresh one-job Release/Quick3D-ON package build pass; exact source/package hashes, metadata and license recorded.
- [x] All 5/5 affected asset/export CTests (11 exporter groups) and fresh installed-test target pass, preserving reference rights and asset output hash controls.
- [x] Existing scoped native package gate passes: complete 210-file payload/bytes/modes, GPL/MIT metadata/notices, 17 original asset declarations, eight MIT components, 15+15 actual catalogs/cards, installed QtTest 8/8 and Quick3D-hidden 4/4, native 0.1.0-2 → 0.1.1-1 upgrade/removals and preserved configuration. No obsolete paths expected; absence assertion retained.
- [x] Current public links/licensing/version references and historical section boundaries verified.
- [x] Owned scratch/build/fixtures/extractions removed, 61 prior evidence files and 135 core records preserved, no task-owned process/core; 12 compact candidate/evidence files retained before final Git closure. Separate RELEASE_SHA256SUMS preserves the canonical one-entry exporter receipt.
- [ ] New `v0.1.1` tag separately authorized and created; remains unexecuted.
- [ ] New candidate publication separately authorized and performed; remains unexecuted.
- [ ] Physical acceptance performed; R-01 remains NOT EXECUTED.
- [ ] External artwork sample executed; required archive remains unavailable.

R-02's project-wide license decision is complete; historical `v0.1.0` remains
unchanged and the next corrected candidate carries GPL-3.0-or-later. R-03's
native fault mechanism remains unproved; R-04 labels these checks corrective
verification rather than independent review. Exact tested candidate identity,
source hash `d76c9e45b30123f9064fca3837207b77e9f82f83d21304c9bae1069ba7fff525`,
package hash `22b44078a8e9b9cbe95e81a51da590d3dcbcc43df25a0c80bc17d63f2ec2cb7d`,
gates, cleanup and final Git closure belong to `build-codex-release-integrity-0.1.1/STATE.json`.

The tested source commit remains the eventual tag target even after main
advances for excluded operational documents and the source digest pin. Runtime
evidence includes five fresh CTest names and 102 reused names, not a new full
suite/private session. Initial asset-hash failures and their ten-pin repair
remain recorded. The supplied assignment ends mid-section 24; its missing
remainder was requested and not supplied.

## RC-01 and RC-02 acceptance — 2026-10-04

Historical preceding correction/publication record. Its then-unselected
project-wide license is superseded by the owner's 0.1.1 GPL decision above.

**OWNER-APPROVED ASSET REPLACEMENT AND FINAL REMOTE PROVENANCE PASS.**
The latest supplied independent audit found zero new runtime bugs and closed
AD-04-R1. Local release-provenance correction and all affected candidate gates
pass. All six replacement assets and the reviewed release body are published
and verified; the original tag and prerelease status are preserved.

- [x] Starting clean `main` `29e9f22e32d8381d95b0d7ca81c4e7fde1c57281` recorded; actual local/remote annotated `v0.1.0` object `50812852c4dc2726411a1c73452296955852c50a` and fixed target `c3b3a0b7771b313c45f843f49a503b45b0d1ada0` verified.
- [x] Actual prerelease ID `402697537`, publication timestamp 2026-10-03 21:23:54 UTC, draft/prerelease state, original asset IDs/sizes/digests and downloaded bytes recorded.
- [x] RC-01 reproduced: two clean tag exports identical but different from the original published source; checkpoint/tag HEAD and epoch mismatch confirmed, with canonical catalog-mode difference recorded and owner file untouched.
- [x] Corrected normalized source comes from the exact clean tagged commit: HEAD/epoch match, 519 files/520 unique members, one regular checkpoint, successful extraction, full hashes/modes/normalization, recipe and checksum pins.
- [x] Small official-path verifier and existing synthetic fixtures cover clean exact annotated tag, wrong HEAD, lightweight tag, tracked/untracked dirt, precommit identity, external relabelling and a dirty export after restoration; all eight unittest groups and registered exporter CTest pass.
- [x] Fresh source verification, one-job Release/Quick3D-ON package build and existing installed-test target build pass; root recipe matches the generated tag-matched recipe.
- [x] Fresh existing scoped native package checks pass: 209 installed payload files/bytes/modes, 15+15 actual catalogs/cards, QtTest 8/8 and Quick3D-hidden 4/4, install/upgrade/obsolete files/removal and configuration preservation.
- [x] Runtime reuse justified by 349 unchanged inputs, identical toolchain/dependencies and 208 resource payloads plus fresh installed checks. Executable bytes differ; 107 configured names have one refreshed and 106 reused results, including reused 18 native CTests/37 lifecycle phases/private startup-recovery. No new full-suite/private invocation or binary identity claim.
- [x] Current README/changelog/limitations/platform/state/checklist prose reflects the existing prerelease and fixed tag; historical checkpoint records and all R-01–R-04 qualifiers preserved.
- [x] Exact old/new hashes/sizes for all six replacement assets and the reviewed release body retained under `build-codex-final-release-provenance/`; before/after remote evidence preserved in the local log ZIP.
- [x] Safe task-owned cleanup passes for both corrective and publication-only scratch: tagged worktrees removed with native Git, scratch/build/extraction/fixtures/downloads removed, 50 earlier files byte-unchanged, all 135 baseline core records preserved, no task-owned process/core, eleven deliberate files retained.
- [x] Owner's subsequent “Approved” explicitly authorizes replacing the six existing prerelease assets and applying the reviewed body; original tag, release identity and prerelease status preserved.
- [x] Replacement executed; all six final downloads match reviewed bytes/sizes/SHA-256 and GitHub digests, all five checksum entries pass, checkpoint HEAD/epoch match the tag, and published source equals both fresh clean-tag exports. Real extraction, 519-file/520-member inventory, hashes/modes/normalization, published verification receipt and exact release body pass at 2026-10-04 00:23:09 CEST.
- [ ] Physical acceptance performed; R-01 remains NOT EXECUTED.
- [ ] Project-wide license selected; R-02 retains existing unspecified/component/asset declarations.
- [ ] External artwork-sample subcase executed; required archive remains unavailable.

Published replacement source SHA-256:
`34abdc7fca9efcc1989a02abb47e774330c6f490c61715a2c82c63ce04295e9c`.
Published replacement package SHA-256:
`91db462260602e539beb6e21f18eff0456ae97e70121826491df1b453cd388ea`.
These current uploaded hashes also match every retained reviewed file and final
remote download. Use the source with its matching recipe/checksums. The
[corrective report](POST_TASK_0045_CORRECTIVE_REPORT.md#rc-01-and-rc-02-release-provenance-closure--2026-10-04)
records all six old/new digests. R-03 keeps the historical Mesa-worker mechanism
unproved and teardown as mitigation; R-04 preserves separate independent audit
provenance and labels this agent's results corrective/candidate verification.
Exact reviewed Git paths, subsequent authorized commit/sync and remote parity
are recorded in the current `STATE.json`; they do not move the release tag or
authorize asset replacement by themselves.

## AD-04-R1 release-closure acceptance

Historical preceding candidate verification and initial publication; RC-01's
tag-reconstruction finding and prepared correction above supersede its source
provenance conclusion without reopening the passing runtime/namespace checks.

- [x] Clean starting HEAD `5a183b0f77bfa35d0ac2a3358951a90b07654697` recorded.
- [x] Tracked/untracked direct and deep descendants reproduced before production repair; real extraction failed with errno 21, while exact-file controls remained rejected.
- [x] Generated root namespace reserved by first path component; conflicting input preserved, valid nested names and symlink/path checks retained.
- [x] Targeted regressions, six complete exporter unittest groups and registered checkout exporter CTest pass.
- [x] Two byte-identical normalized exports; 519 source files/520 unique members, one regular generated checkpoint, successful extraction, inventory bytes/modes, identities/timestamps, digest and recipe pin verified.
- [x] Fresh source verification, one-job Release package and existing installed-test target builds pass.
- [x] Fresh native install/upgrade/payload/removal and preserved configuration pass; 209 installed files, 15+15 actual catalogs/cards, installed QtTest 8/8 and Quick3D-hidden 4/4.
- [x] Source/toolchain/resource identity supports explicit runtime reuse: 107 configured names with one exporter CTest refreshed and 106 unchanged checks reused; no new full-suite/private invocation or executable byte-identity claim.
- [x] Historical actual candidates locally checked consistent; earlier failures and native-disposal uncertainty preserved.
- [x] R-01–R-04 defined; physical cells remain NOT EXECUTED, licensing stays unselected, teardown is a harness mitigation, self-run verification is not independent review.
- [x] Safe task-owned cleanup complete, with 40 protected earlier files byte-unchanged and ten compact deliberate artifacts retained; no task-owned process/core remains and all 135 baseline core records are preserved.

| Boundary | Current disposition |
| --- | --- |
| R-01 | Physical second-monitor/connector/scanout, another GPU and owner-desktop observations remain NOT EXECUTED. |
| R-02 | Owner-authorized correction commit/sync, annotated `v0.1.0` and prerelease publication are executed and verified below. All six downloaded assets match. Project-wide licensing remains unselected. |
| R-03 | Two historical TASK-0044 private PlasmaShell Mesa-worker disposal faults retain their unproved mechanism. Teardown is a harness mitigation, not a proved native fault fix. |
| R-04 | Self-run results are corrective/candidate verification. Separate independent review has not been performed by this correction agent. |

Fresh source SHA-256:
`367c3314260bd31ddf268a49ef861472f25b800966f361681f375bf5f8f5b077`.
Fresh package SHA-256:
`7ef5cd2131bdedd63765c711f8148dd6ea4c7ba42510f8c30ed7970786333bcc`.
Raw scope/reuse evidence and actual closure proof belong to
`build-codex-post-task-0045-release-closure/`. The external artwork-sample
subcase remains unexecuted without its archive.

## Preceding corrective acceptance — historical `5a183b0` checkpoint

- [x] AD-01 through AD-05 accounted for with executed focused evidence.
- [x] Fresh one-job Debug application/affected-test and exported-source Release builds.
- [x] 107/107 configured CTest names have applicable passing evidence: 65 refreshed, 42 unchanged initial checks reused; no claim of one new full-suite invocation.
- [x] All 18 native CTests pass for their relevant final source.
- [x] Repeat normalized exports identical: 519 source files, 520 unique members, matching bytes/modes/identities/timestamps; root recipe pins the verified checksum.
- [x] Complete installed-package gate passes: 209 payload files, 15+15 rendered catalogs, QtTest 8/8 and Quick3D-hidden 4/4, private startup in both modes, native install/upgrade/recovery/removal and preserved disposable configuration.
- [x] Original standalone lifecycle passes 37/37 phases with unchanged 900-second bound and assertions.
- [x] Task-owned cleanup complete; twenty deliberate corrective files retained, including ten preliminary files unchanged; all 20 older protected artifacts byte-unchanged.
- [x] Previously unavailable R-01 through R-04 definitions supplied and reconciled in the later release-closure pass; this does not mark physical cells or license selection complete.
- [ ] External source-sample QtTest row; its archive was not supplied (CTest itself passes).

## Repository

- [x] Preceding corrective source/resources included in owner-authorized commit `5a183b0`.
- [x] Current documentation distinguishes final corrective results from historical receipts.
- [x] No stale current-state source of truth.
- [x] Previous `5a183b0` commit/sync verified in its retained `FINAL_STATE.json`.
- [x] Current eight-path correction committed/synced as `c3b3a0b7771b313c45f843f49a503b45b0d1ada0` under the new owner instruction, with exact paths, clean tree and remote parity verified; final documentation closure is recorded in current `STATE.json`.
- [x] Annotated `v0.1.0` created at that correction commit; local/remote tag object `50812852c4dc2726411a1c73452296955852c50a` and peeled target verified under the separate tagging approval.
- [x] [Prerelease candidate published](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0) at 2026-10-03 21:23:54 UTC under the separate publication approval; six uploaded assets match downloaded SHA-256/size, downloaded checksum entries pass, and explicit `latest=false` is recorded.
- [ ] Project-wide license selected by the owner; existing unspecified/component/asset declarations remain preserved.

## Build and installation

- [x] Clean build from clone.
- [x] All tests pass.
- [x] Staged install verified.
- [x] Service starts through the documented mechanism.
- [x] Uninstall leaves no broken Plasma configuration.

## Panel lifecycle

- [x] Native and free panels create, recover, hide/show, and remove safely.
- [x] Unrelated Plasma panels remain untouched.
- [x] No orphan free host.
- [ ] Physical multi-monitor recovery observed; private virtual-output recovery passes.

## Interface

- [x] Every visible control works or is explicitly experimental.
- [x] No UI-only 3D setting.
- [x] Native configuration and Panel Studio use the same schema.
- [x] Preview matches live renderer.
- [x] Built-in Panel and Icon preset browsers are separate and use actual renderer output.
- [x] Exactly 15 valid built-in Panel Presets and 15 valid built-in Icon Presets are installed.
- [x] Built-ins are immutable and customized derivatives are reusable.

## Rendering

- [x] Procedural 2D works independently.
- [x] At least one production skinned-2D family works.
- [x] Icon styles and per-icon overrides work.
- [x] Requested icon animations work.
- [x] Regular panel collapsed/open behavior works.
- [x] Free-panel rotation and glow work.
- [x] 2.5D ring/arc works.
- [x] True 3D, when included, has a safe fallback.

## Interaction

- [x] Grouped windows can be individually controlled.
- [x] Context menus expose implemented actions only.
- [x] Edit mode and drag/drop do not conflict with normal launching.
- [x] Reduced motion works.
- [x] Desktop audition changes only the selected/temporary owned host and rolls back exactly on Cancel.
- [x] Icon Preset audition does not change panel theme/layout/placement.
- [x] No canceled preview leaves an applet, record, default, or ownership token.

## TASK-0035 verification record — 2026-09-09

The optional base true-3D renderer passed separate OFF/AUTO/ON Debug builds
and 65/65 CTests in each mode. OFF explicitly disabled Quick3D discovery and
omitted optional resources; AUTO found the installed module; ON required it.
The core executable did not link to Quick3D in any mode.

Required staged installation and private D-Bus/KWin Wayland/PlasmaShell checks
passed with real `org.archdock.dock` applets. Enabled modes rendered the
original mesh theme, changed quality low/high/low within its target bounds,
and exercised missing mesh/texture fallback. OFF selected procedural 2D and
hid 3D controls. Studio Apply, Cancel and reopen passed. Unsupported base-scene
rotation is gated; TASK-0036 motion is not implemented by this task.

This task evidence does not close the broader release checklist above.
Personal desktop, physical GPU/monitor, hardware hotplug, clean-clone release
packaging, and desktop audition acceptance are not claimed by these private
checks. See [CURRENT_STATE.md](CURRENT_STATE.md#task-0035--optional-true-3d-capability-and-base-scene-renderer)
for exact results and [shared-renderer.md](shared-renderer.md#base-true-3d-renderer)
for the implemented limits.

## TASK-0036 resumed verification — 2026-09-19

Historical run; the original glyph-motion failure is superseded by the
blocker-repair verification below.

**BLOCKED in Phase A; Phase B has not started.** Commit `0211331` is the
owner's partial implementation checkpoint, not task completion. A fresh ON
Debug configure and full serial build passed. Private Wayland rendering
reproduced unchanged pixels while the mesh glyph's Y angle changed and its
icon source was valid. The focused diagnostic pass also observed unchanged
full-window captures; a safe production correction was not established.

The pixel assertion remains enforced. No complete CTest run or Phase B
OFF/AUTO/ON, live fallback or service-restart acceptance was performed in the
resumed session. No release checkbox is closed by these results. See the
[TASK-0036 current-state record](CURRENT_STATE.md#task-0036--resumed-phase-a-investigation-2026-09-19)
for commands, diagnostic observations and acceptance statuses.

## TASK-0037 partial Phase A verification — 2026-09-19

Historical implementation run, now committed as `4cdb234`. The blocker-repair
run below reached a later failure in its grouped-window fixture.

**BLOCKED in Phase A; Phases B and C have not started.** The owner explicitly
requested and approved TASK-0037 after the TASK-0036 blocker record at
`5f41ced`; this did not waive either task's verification gates.

The partial implementation adds grouped-window rows, a shared title-list popup,
an optional KDE/PipeWire thumbnail adapter, Plasma popup hosting, hover/menu
entry points and presentation guards. It reuses the existing watcher, models,
snapshots, selection path and private runtime harness. Individual minimize,
restore, close, New Instance and desktop actions remain Phase B work.

A fresh Debug AUTO configure and complete serial Make build passed. The final
CTest invocation stopped with 54 passed, 1 failed and 12 not run out of 67
registered tests. The failure is the unchanged TASK-0036 mesh-glyph pixel
assertion in `rendering-import-smoke`. New model and offscreen QML tests passed;
the new private multi-window fixture was not reached. Actual thumbnail streams,
live popup positioning and the complete action/edge/free-layout matrix are
unverified. No release checkbox is closed by these results. See the
[TASK-0037 current-state record](CURRENT_STATE.md#task-0037--partial-preview-implementation-2026-09-19)
for exact commands, file purposes and all inherited acceptance statuses.

## TASK-0036 blocker-repair verification — 2026-09-19

Historical repair, committed by the owner as `d7c6021`. The arrival failure
below is superseded by the TASK-0038 prerequisite verification that follows.

**Original glyph-motion blocker resolved; integrated acceptance remains
BLOCKED.** Baseline and final HEAD are
`4cdb234946d0689bd7d0da2a16501bf6c58a65d9`; repair changes remain unstaged.

The private session could not load the test's desktop icon name. Kirigami
reported `Error` while retaining a non-null, fully transparent fallback image.
The existing renderer test now reuses a repository image fixture and requires
both native `Ready` status and visible glyph pixels. Its original assertion
that actual mesh rotation changes viewport pixels remains enforced.

The newly reachable cleanup checks also exposed an unsafe scene-input binding
during theme removal and a test that checked a Repeater3D delegate before
deferred deletion. The loader now guards absent scene data, and the test waits
for actual delegate destruction.

- Fresh ON Debug configure and complete serial build: **PASS**.
- Focused PanelScene CTest: **1/1 PASS**.
- Targeted private motion/fallback case: **PASS**.
- Staged private renderer suite: **6/6 PASS**, including visible glyph motion,
  parts, concealment, reduced motion, active fallback and bounded recovery.
- Full CTest: **54 passed, 1 failed, 12 not run**, out of 67 registered.
- First remaining failure: unchanged TASK-0037
  `groupedWindowsFollowLiveKWinUpdates`, at
  `tests/PanelWindowCapabilityTest.cpp:157`; WindowModel did not receive the
  first fixture window's title.
- Later private popup/editor/applet checks and the TASK-0036 Phase B build and
  service-restart matrix: **NOT EXECUTED** in this repair.

No release checkbox or consolidated task-completion gate is closed. The
original blocker has positive runtime evidence; the new integrated failure
must be resolved before claiming full acceptance. See the
[blocker-repair evidence](CURRENT_STATE.md#task-0036--glyph-motion-blocker-repair-2026-09-19)
for the cause, exact commands, cleanup and next boundary.

## TASK-0038 prerequisite verification — 2026-09-19

Historical watcher-interface repair, committed by the owner as `7c16581`.
The restore failure below is superseded by the green verification that follows.

**BLOCKED before TASK-0038 Phase A.** Its implementation approval is retained,
but the approved predecessor gate is still open. Baseline and final HEAD are
`d7c6021f58437e11725fba7bdabf271815d4b3a3`. The only functional change is the
explicit existing `local.WindowWatcher` D-Bus interface in `WindowWatcher.h`.

The original private trace proved that KWin sent both fixture windows and Qt
rejected the calls with `UnknownInterface`. The unchanged test passed after
the declaration; its assertions and timeout were preserved.

- Pack integrity: **174/174 PASS**.
- Fresh ON Debug configure and complete serial build: **PASS**.
- Standalone grouped-window test after correction: **3/3 PASS**, including
  live grouping, title updates, minimize/restore/activation and removal.
- Full CTest: **54 passed, 1 failed, 12 not run**, of 67 registered.
- Staged private renderer suite: **6/6 PASS**.
- Staged grouped-window test: arrivals/grouping/title/minimize now pass, but
  restore after the accepted activation request fails at
  `tests/PanelWindowCapabilityTest.cpp:184`.
- The standalone restore pass does **not** establish integrated reliability.
  The new failure's cause is unproved; later smoke stages were not executed.
- TASK-0036 Phase B, TASK-0037 Phases B/C and TASK-0038 Phases A/B remain
  **NOT EXECUTED**. Their earlier phase gates remain unclosed.

No release checkbox is closed. No runtime test, assertion or acceptance
criterion was bypassed. See the
[prerequisite evidence](CURRENT_STATE.md#task-0038--approval-and-prerequisite-watcher-repair-2026-09-19)
for the exact cause, commands, remaining boundary and approval status.

## Restore and fallback repair verification — 2026-09-19

**Failure repair COMPLETE; current suite green.** Baseline and final HEAD are
`7c16581485a3164ff379f7bca3c651a7bc267c58`; the two code/test files and two
evidence documents remain modified and unstaged.

KWin's global script start reapplied package enablement and unloaded the
installed watcher. The bridge now runs and cleans up only its own action
script, preserving live window updates. The renderer test also needed a
persistent resource-limit fault: its old C++ property write left a QML
binding active, allowing rotation to restore ordinary geometry. The corrected
write detaches that binding. Original behavior assertions remain enforced.

- Active pack integrity: **174/174 PASS**.
- Fresh ON Debug configure and complete serial build: **PASS**.
- Corrected staged private Wayland smoke: **1/1 PASS**, 82.07 seconds.
- Final full serial CTest: **67/67 PASS**, zero failures or unrun tests,
  139.28 seconds; its staged smoke also passed, in 82.19 seconds.
- Private renderer: **6/6 PASS**; grouped windows: **3/3 PASS**; preview popup:
  **13/13 PASS**; Icon Properties/mesh editor: **4/4 PASS**; energy pixels and
  surface integration: **3/3 and 19/19 PASS**.
- Actual staged native/free Plasma applets, theme/icon-style changes, quality,
  rotation, memory bound and owned-host cleanup: **PASS**.
- TASK-0036 Phase B, TASK-0037 Phases B/C and TASK-0038 Phases A/B remain
  **NOT EXECUTED**. Existing test success does not implement these phases.

No failed check remains in the final suite. No release checkbox is closed by
this bounded repair, and private virtual KWin evidence does not establish
personal-desktop, physical GPU/monitor or release acceptance. See the
[restore and fallback repair record](CURRENT_STATE.md#task-0038-prerequisite--restore-and-fallback-repair-2026-09-19)
for native research, diagnostic proof, exact commands and cleanup.

## TASK-0036 Phase B and final matrix — 2026-09-23

**Phase A, Phase B and consolidated closure PASS.** The implementation baseline
was `a7f366506315e4009db2332ea8ddc6edc9cdf5ba`. Its `Arch Dock task 38` subject
describes the earlier prerequisite repair. The owner committed the verified
TASK-0036 work as `80d0820b76f9c816be75f19fb7450aab1b5a1f0e`; the subsequently
authorized administrative cleanup is now complete. Test results below are
from the preceding implementation run, not a new cleanup-only test run.

The main 3D switch reuses `rendererTier`, detailed controls follow effective
availability, and the backend rejects inactive quality edits. Existing fallback
and saved-intent metadata remain authoritative. Real fallback/recovery and an
identity-checked private service restart now have integrated coverage. The
expanded Studio test also exposed a texture lifetime crash: its source had left
the window while the 3D consumer retained the released layer. The renderer now
clears the source on inactivity or window detachment. Actual switch clicks use
Qt's native polish wait, with form recreation retained as regression coverage.

| Final gate | Result |
| --- | --- |
| Pack integrity | PASS, 174/174 |
| Fresh ON configure/build and full serial CTest | PASS, 67/67, 136.11 seconds |
| Fresh OFF configure/build and full serial CTest | PASS, 67/67, 125.93 seconds |
| Fresh AUTO configure/build and full serial CTest | PASS, 67/67, 137.79 seconds |
| Private staged smoke within ON / OFF / AUTO | PASS, 80.76 / 71.06 / 81.39 seconds |
| TASK-0068 inherited motion/parts/reduced-motion/cleanup criteria | PASS, 4/4 |
| TASK-0069 inherited fallback/intent/control/ordinary-renderer criteria | PASS, 4/4 |
| Temporary diagnostic cleanup | PASS: task-owned artifacts and both exact OS crash dumps removed; absence verified |

No build or test failure remains in the final matrix. The grouped-window and
energy-pixel cases skipped in ordinary offscreen invocations both passed in
each private smoke. The optional external source-archive comparison was not
executed; it is outside this renderer task's acceptance. No release checkbox
is closed by these private virtual KWin/Plasma results, and personal-desktop,
physical GPU/monitor, hotplug and release packaging acceptance are not claimed.

The initial noninteractive cleanup required administrator authentication. The
owner authorized authenticated removal of the two exact diagnostic dumps,
and their absence was verified. The
[current TASK-0036 evidence record](CURRENT_STATE.md#task-0036--resumed-phase-b-implementation-2026-09-23)
contains cleanup evidence, file purposes, native research, diagnosis and
reproduction commands. At that closure, TASK-0037 Phases B/C and
TASK-0038 Phases A/B remained unimplemented; their prior approvals were retained.

## TASK-0037 consolidated verification — 2026-09-23

**Phases A, B, C and consolidated closure PASS.** The retained plan and
approval were reused from baseline `80d0820`; no Git closure was performed.

| Gate | Result |
| --- | --- |
| Phase A fresh AUTO build, original private smoke, full CTest | PASS, 67/67, 141.19 seconds |
| Phase B fresh build, exact-ID action/native launcher checks, full CTest | PASS, 67/67, 138.18 seconds |
| Phase C final fresh build and full serial suite | PASS, 68/68, 203.89 seconds |
| Final original staged renderer/editor smoke | PASS, 82.37 seconds; native preview suite 21/21 |
| Final real applet EIS interaction matrix | PASS, 64.00 seconds; four native edges and free ring/arc |
| Removal/update/guard acceptance | PASS: rapid replacements, exact activation/minimize/restore/close, remaining/last window, stale ID, dismissal and no accidental icon launch |
| Private runtime cleanup | PASS: all 41 recorded session roots absent; no process using their private environment roots |

The matrix exposed and verified corrections for watcher lifetime across KWin
reload, thin-panel menu clipping, compact-only representation routing,
Wayland popup stacking and menu-owner destruction during window-state updates.
Fixture synchronization and the synthetic Qt editor fixture were corrected
without weakening their assertions. The final gate has no remaining failure.
The [current-state record](CURRENT_STATE.md#phase-c-and-consolidated-closure)
contains the detailed evidence and reproduction command.

These results do not close personal-desktop, hardware, GPU, monitor/hotplug or
release acceptance. TASK-0038 folder and segment phases remain the next
approved work; TASK-0039 implementation remains out of scope.

## TASK-0038 Phase A verification — 2026-09-23

**Folder phase COMPLETE; consolidated TASK-0038 INCOMPLETE.** Phase B segments
and the final integrated gate remain required before TASK-0039 may begin.
The retained plan and approval apply; no re-planning or repeated approval is needed.

| Gate | Current evidence |
| --- | --- |
| Fresh ON Debug build | PASS in `/tmp/archdock-task0038-resume.lR1Az5/phase-a` |
| Full serial CTest | PASS, 71/71, 266.85 s |
| Staged rendering smoke | PASS, 82.10 s |
| Grouped-window regression | PASS, 63.99 s |
| Folder native/free interaction | PASS, 64.01 s; all five layouts, four native edges, free ring/arc hosts, controlled document opening |
| Folder bounds, empty/deleted/unavailable paths, unsafe/stale IDs | PASS in model/backend/QML tests |
| Selection/dismissal, no accidental root launch, popup guards | PASS in actual private applets |
| Reduced motion | PASS in shared QML tests; live matrix observes the saved runtime value |
| Whitespace | `git diff --check` PASS |
| Private session cleanup | Five roots named in retained logs absent; no process retained them |

The live gate found and verified a fix for KJob/Qt quit-lock completion stopping
the windowless backend. The fixture also now refreshes popup size and child
coordinates while matching KWin geometry after layout changes. Earlier failures
are superseded by the final full passing run. Details and reproduction paths are
in `CURRENT_STATE.md`. The task build/log root remains for active continuation;
final consolidated cleanup is still pending. No staging, commit, push, global
installation, personal-desktop mutation or delegated agent was used. These
private virtual-session results do not constitute hardware or release acceptance.

## TASK-0038 consolidated closure — 2026-09-27

**COMPLETE in the working tree; owner-controlled Git closure pending.** This
section supersedes the September 23 Phase B/final-gate pending status above.
TASK-0039 has not been planned or implemented.

| Gate | Final evidence |
| --- | --- |
| Phase A | Historical fresh ON build and 71/71 tests, 266.85 s; committed by owner in `032420af` |
| Phase B | Fresh ON build and 71/71 tests, 269.13 s |
| Separate fresh integrated ON configure/build | PASS |
| Final full serial CTest | PASS, 71/71, 272.29 s |
| Final staged rendering and Studio | PASS, 84.86 s; segment preview ownership, reorder, Cancel, Apply and removal included |
| Final grouped-window regression | PASS, 63.95 s |
| Final native/free folder and segment interaction | PASS, 64.05 s; five folder layouts, exact document selection, guards/dismissal, independent surfaces/order, closed-segment hover/input and ownership rejection |
| Optional-3D OFF | Fresh configure and application/shared-module build PASS; folder expansion, segment scene and preview checks 3/3 PASS, 2.56 s; full OFF suite was not repeated |
| Model, persistence, rollback and capability contracts | PASS in both full ON suites |
| Reduced motion and visible segment motion | PASS in shared QML tests, including rendered-frame assertions |
| Final whitespace/consistency | `git diff --check` PASS; current-state acceptance matrix records all eight inherited criteria PASS |
| Cleanup | 12 recorded private runtime roots absent, no retained processes, no remaining rendering-session roots; task build/log root and one diagnostic crash dump removed |

The final gate includes corrections for animated segment-to-icon hover handoff,
nested D-Bus segment values, absent optional descriptor bounds, private KWin
probe lifetime and a verified service-name disappearance during cleanup. The
last correction accepts only the exact cleanup race after confirming the name
is still absent; capture/check/restart and process-identity checks remain strict.
All live assertions had already passed in the interrupted cleanup run, and the
corrected complete suite passed afterward. No test was skipped or weakened.

`CURRENT_STATE.md` contains the acceptance matrix, changed-file groups and exact
verification commands. Historical temporary paths were removed after recording
the results. Baseline/final HEAD is
`032420afeee8ac2453f4896a57da7c2e60cf213e`; changes are unstaged. No agents, Git
writes, global installation or personal-desktop mutation were used. These are
private virtual KWin/Plasma Wayland results, not physical GPU, monitor/hotplug,
personal-desktop or release acceptance. Status providers remain TASK-0039 scope.

## TASK-0039 consolidated closure — 2026-09-27

**COMPLETE in the working tree; Git closure remains owner-controlled.**
This record supersedes the historical TASK-0039 pending statements above.
Entry/final HEAD is `6892c13e7827f4990c71ed2fe49a6462273ccc72`; the owner had
committed the prior blocked checkpoint before this continuation.

| Gate | Final evidence |
| --- | --- |
| Current Debug/AUTO build | PASS using the validated retained task build; source/resource changes rebuilt |
| Segment binding-loop regression | RED before repair, GREEN for animated/reduced motion; adjacent presentation/scene/guard CTests 4/4 PASS |
| Native concealment discriminator | PASS, 64.05 s; actual KWin hide, one terminal report, unchanged widgets and same-window reveal |
| Full serial CTest, no early stop | **75/75 PASS, 285.59 s**; JUnit: zero failures/errors/skips/disabled, zero not run |
| Staged rendering/Studio | PASS, 89.24 s |
| Native/free window interaction | PASS, 64.00 s |
| Combined folders/three segments/content | PASS, 64.00 s; all five folder layouts, persisted switches, badges/progress/attention, real status and no duplicate applets |
| Source lifecycle and demand | PASS: expiry/disconnect, unavailable data, asynchronous status, hidden overlay deferral, status pause and reveal |
| Coalescing | 100 updates -> 2 global revisions; focused range 1–2, original checkpoint 2; unchanged frequency bounds pass |
| QML runtime checks | Zero binding-loop messages and zero QML-error gate failures in the complete final detailed log |
| Cleanup | All 28 referenced private runtime roots absent; no task processes or remaining rendering-session directories; task build/log/probe root removed after evidence was recorded |

The production fix preserves segment state across content refreshes by using
the existing guarded requests instead of resetting its presentation controller.
Mandatory-gate diagnosis also corrected test configuration acknowledgement,
partial log-record parsing and folder keyboard-focus readiness. All were
reproduced before repair; assertions, timeout bounds and the QML-error gate
remain strict. The owner explicitly authorized bounded repair cycles for this
task. Earlier blocked records remain historical evidence.

`CURRENT_STATE.md` contains the dependency trace, commands, failure accounting,
eight inherited acceptance results and final cleanup. The old handoff is marked
retired without deleting its checkpoint. Temporary log paths now identify
historical runs, not retained artifacts. No Git writes, agents, dependencies,
external network, global installation or personal-desktop mutation were used.
These private virtual Wayland results do not establish physical GPU,
monitor/hotplug, personal-desktop or general release acceptance; the broader
release checkboxes remain unchanged.

## TASK-0040 verification record — 2026-10-01

Panel Preset and Icon Preset definitions, catalogs, user store, Studio pages
and the exact 15 + 15 built-in libraries are implemented in the working tree on
`3ab470c`. A fresh configure and build has no compiler warning and the full
suite passes **80/80 (321.17 s)** with the parent session bus unreachable. The
earlier incremental Phase A gate also passed 80/80 (317.25 s).

- `preset-catalog-test` proves the exact id and name lists, every reference,
  and that each preset resolves as declared.
- `preset-staged-preview-smoke` installs into a temporary prefix, counts
  15 + 15 definitions and runs the library test against the staged data and the
  staged `ArchDock.Rendering` module.
- `preset-library-test` draws all 30 real cards through the shared renderer and
  proves that browsing and selecting change no panel.
- `panel-window-capability-test` drives the real Studio popup's six new pages.

`CURRENT_STATE.md` holds the acceptance table, the three diagnosed failures,
the deviations from the approved plan and one pre-existing renderer warning
that this task did not change. No agents, dependencies, external network,
global installation or personal-desktop mutation were used. The implementation
made no Git writes; the single commit and push were made afterwards on the
owner's explicit instruction. These
results do not establish physical GPU, monitor/hotplug, personal-desktop or
general release acceptance; the broader release checkboxes remain unchanged.

## TASK-0041 verification record — 2026-10-02

**TASK-0041 is COMPLETE in the working tree**, resumed from `a3a522b`.
The phase and separate fresh consolidated builds each used nine bounded target
groups at two jobs. Both suites pass **87/87 CTests**: successful batch times
sum to **431.70 s** for the phase and **427.05 s** for the consolidated gate.
One CTest worker, batches of at most ten and private/heavy checks separately
were used throughout. The fresh consolidated run has no failed/retried batch;
JUnit verifies all 87 names with no failure/error/skip/disabled/not-run result.
All five private audition matrix groups pass on the final source.

- The browser and audition QML tests verify explicit actions, incompatible
  disabled controls, state/interaction guards, keyboard and accessibility.
- The session, store, registry and Studio tests verify normalized preparation,
  atomic revisions, independent icon data, reusable custom resources, bounded
  journal/default files and recoverable failure states.
- Five real private Plasma matrix groups verify existing native preview,
  exact Cancel/Revert, temporary native/free cleanup and one-host conversion,
  icon-only isolation, immutable built-in bytes, customized panel/icon reuse,
  combined and independent defaults on future native/free creation, service and
  PlasmaShell interruption, exact existing-free geometry recovery and unrelated
  native/free fixture preservation.
- Staged renderer, window/folder interaction and preset installation checks
  pass against the current executable/resources and all 30 built-in definitions.

The initial GTK fixture preflight failed because an isolated venv hid the
system `gi` module; system-site access repaired the disposable environment.
The defaults crash scenario exposed a harness name/PID readiness race; pinning
and confirming a live unique D-Bus owner repaired it within the existing startup
bound. The failed checks and all affected matrix groups were rerun successfully;
assertions and native ownership boundaries remain enforced.

[CURRENT_STATE.md](CURRENT_STATE.md#task-0041-implementation--2026-10-02)
records the full acceptance mapping, commands, failure accounting and final
closure. [plasma-lifecycle.md](plasma-lifecycle.md#preset-audition-matrix) documents
the isolated reproduction and the native free-widget geometry workaround.
Cleanup is verified: all 21 recorded private runtime roots are absent, no
task-owned process remains, and task build/dependency/evidence output is
removed. Pre-existing build directories remain intact. Git closure is
owner-controlled; no agents, global package installation or personal Plasma
mutation were used in this resumed continuation.
These private virtual Wayland checks do not close personal-desktop, physical
GPU/monitor, hardware hotplug or general release acceptance. Broader release
checkboxes above remain unchanged.

## TASK-0042 profile management and shortcuts verification — 2026-10-02

**COMPLETE.** All inherited criteria for legacy TASK-0079, TASK-0080 and
TASK-0081 passed in order. Phase A passed 88/88 CTests, Phase B 92/92 and
Phase C 94/94, with green builds. A separate empty final configure and seven
bounded one-job build groups passed, followed by the all-target build and
94/94 fresh CTests in 23 serial batches (478.24 s summed test time).

Profiles retain complete durable panel sets while excluding live/runtime
state. Persistent CRUD, safe data-only import/export, declared-set apply,
verified native/free rollback and explicit interrupted recovery passed.
Real private GlobalAccel registration/readback, stable-ID activation through
the same transaction, visible local/KDE conflicts, invalid-target rejection,
disable and deletion cleanup passed. Unrelated native panel and desktop-applet
sentinels remained unchanged. The profile and shortcut controls have QML and
backend coverage.

All 15 recorded private runtime roots are absent, no task-owned process
remains, and both task builds/local dependency environment were removed.
Pre-existing build directories were preserved. No agents, concurrent gates,
global installation or personal Plasma mutation were used. The owner requested
commit and sync after completion. This task does not establish physical key,
GPU/monitor, hotplug, clean-clone packaging or release acceptance; the broader
checkboxes remain open. The pre-existing PanelScene sequence warning remains
outside this task.

See [the TASK-0042 current-state record](CURRENT_STATE.md#task-0042--complete-profile-management-and-kde-shortcuts--2026-10-02),
[PROFILE_PACKAGE.md](PROFILE_PACKAGE.md) and
[the private runtime contract](plasma-lifecycle.md#complete-managed-profile-arrangements)
for criteria, exact observations, corrections, commands and scope limits.

## TASK-0043 partial startup verification — 2026-10-02

Historical boundary, superseded by the resumed verification record below.

**BLOCKED in Phase A; Phase B has not started.** Baseline and final HEAD
are `2816be6d0d0a631829c4249327c73fbc7c473a94`; partial changes remain
unstaged. A fresh one-job application/module/backend build, prefix and
`DESTDIR` metadata checks, desktop/systemd validation, and focused
disconnected-bus/missing-executable diagnostics passed.

The new installed startup smoke observed exactly one native D-Bus activation,
the staged executable as owner, repeated manual launch/settings forwarding
without a new owner or panel set, and unchanged unrelated sentinels. Its
single corrected rerun then failed on the pre-existing
`PanelScene.qml:196` `some` TypeError. The renderer is byte-identical to HEAD.
Execution-contract section 3 stops further work at this out-of-scope failure;
the runtime assertion remains enforced.

The complete all-target build, full CTest gate, source-independent rendering,
Arch package build/file-list audit and disposable package installation/removal
remain **NOT EXECUTED**. No release checkbox is closed. Private sessions and
disposable installations were cleaned up; ignored build/dependency output and
failure logs remain for reuse, so consolidated artifact cleanup is incomplete.
No global installation, personal Plasma mutation, Git closure, agent or
concurrent gate was used. See
[the TASK-0043 blocker record](CURRENT_STATE.md#task-0043--partial-startup-implementation-and-runtime-blocker--2026-10-02)
for the exact failed command, all inherited criteria and retained evidence.

## TASK-0043 startup and Arch packaging verification — 2026-10-02

**Phases A and B COMPLETE; consolidated cleanup closure pending one system-owned
crash dump.** Baseline/final HEAD remains
`2816be6d0d0a631829c4249327c73fbc7c473a94`; changes are unstaged.

The preview-copy prerequisite is repaired, direct D-Bus startup is authoritative,
and install-time metadata agrees with the effective executable prefix. Both full
fresh-build CTest gates passed **97/97** (501.15 s and 498.64 s summed test time).
Prefix/DESTDIR checks, native metadata validation and failure diagnostics pass.

Native makepkg built the clean 508-file source checkpoint. Native pacman installed
it into a disposable root with dependency checks enabled. **209/209** installed
files match package bytes/modes and CMake coverage. Installed tests validate
exactly **15+15** catalogs and render all 30 cards: **8/8 QtTests PASS**, plus
**4/4 PASS** with optional Qt Quick 3D hidden. Both private native Wayland startup
runs pass with the source hidden, installed executable-byte identity, one service
owner and unchanged unrelated sentinels. Native removal deleted every package
file and preserved user configuration hashes. All nine inherited criteria pass.

Only deliberate source/package/checkpoint/verification deliverables remain in
`build-codex-task-0043/package-output/`; task builds, dependencies, installations,
probes and raw logs were removed. No task runtime process remains. The proved
Unix-socket path failure generated one root-owned KWin core; `sudo -n` cannot
remove it without the owner's password. Its exact path and final cleanup action
are recorded in [the Phase B cleanup record](CURRENT_STATE.md#task-0043--phase-b-package-verification-and-final-cleanup-boundary--2026-10-02).
The consolidated no-diagnostic-artifact gate remains open until it is removed.

No global package operation, personal Plasma mutation, service enablement, Git
closure, agent or concurrent gate was used. Physical GPU/monitor/hotplug evidence
remains TASK-0044; release/tag acceptance remains TASK-0045. The project-wide
license remains unspecified; existing component/asset declarations are preserved.
Broader release checkboxes remain open. See [INSTALL.md](INSTALL.md) for the
supported owner-controlled package route.

## TASK-0044 recovery implementation boundary — 2026-10-02

TASK-0043's exact pending system-owned KWin dump was removed under owner
authorization and its absence verified. This closes that predecessor's cleanup
boundary; its package evidence remains historical and retained.

TASK-0044 Phase A is implemented, with **7/7** distinct focused CTests passing,
including disposable legacy upgrade, future-schema refusal, blocked-backup
preservation, offline restore, and refusal while a live D-Bus owner exists.
Versioned snapshots precede destructive migrations and profile apply, cover
user resources/defaults/shortcuts, exclude installed built-ins and temporary or
executable content, and preserve the newest valid copy under bounded retention.
Interrupted restore rolls back through its own journal; pending native profile
recovery remains authoritative and cannot be bypassed by configuration restore.

The fresh one-job all-target build and all **99/99 Phase A CTests PASS** in
22 serial batches (512.56 s summed test time), with no skips or failed tests.
All four Phase A criteria pass. Phase B display/accessibility/resource work
follows the same approved plan. No TASK-0045 release box is
closed. The owner authorized commit/sync only after TASK-0044 verification,
then cleanup. No global install, personal Plasma mutation, delegated agent or
concurrent gate was used. See the
[recovery record](CURRENT_STATE.md#task-0044--configuration-recovery-phase-a--2026-10-02).

## TASK-0044 historical frozen checkpoint — 2026-10-03

**Historical owner-requested pause; superseded by the resumed verification record below.**
Phase A's fresh build and **99/99 CTests** passed. Phase B's all-target build
and **106/106 CTests** passed, including the four native Wayland scale groups,
hotplug/audition/restart recovery, resources, accessibility, upgrade and installed
startup. The energy-card, native fixed-length readback, browser presentation,
output-topology and lost-audition-output defects are resolved without weakening
their acceptance assertions. [PLATFORM_MATRIX.md](PLATFORM_MATRIX.md) records
measured results and the physical environment cells not executed.

The separate fresh final configure and build groups 1–4 passed. Group 5 was
stopped on request (exit 143); group 6, the final all-target build, fresh final
106-test suite, optional-3D installed runtime and final artifact cleanup remain
pending. No active task process remains. Task builds, local dependency
environment, logs and diagnostic evidence are retained for reuse.

The latest owner instruction explicitly authorizes checkpoint commit/sync now,
superseding the earlier Git-only completion precondition. The recorded commit
message remains `Harden Arch Dock recovery and Wayland behavior`. This does not
close the consolidated TASK-0044 completion gate or any TASK-0045 release box.
The approved plan is unchanged, no successor task started, and no global install,
personal Plasma mutation, delegated agent or overlapping gate occurred.

## TASK-0044 resumed verification — 2026-10-03

**TASK-0044 COMPLETE; executable verification and final cleanup PASS.**
The owner resumed the same approved task from synced checkpoint
`c225dc6ae0abbeff21425d517546e67831ebc74b`. The retained fresh final configure,
remaining one-job build groups and all-target build passed. Its complete fresh
suite passed **106/106 CTests** in bounded serial batches (778.26 seconds summed
individual times), following Phase A **99/99** and Phase B **106/106**.

The private-disposal correction described in
[plasma-lifecycle.md](plasma-lifecycle.md#task-0044-display-and-resource-hardening)
passed its focused shortcut regression, one-job all-target build, **14/14 affected
runtime checks** (392.85 seconds) and optional-3D-absent installed startup.
The 92 unaffected fresh results are reused with their original receipts.
No additional task-owned native crash was recorded. Eight audition/cancel cycles
grew RSS by **8.43 MiB**, below the 64 MiB limit, with zero stale hosts.

| Inherited acceptance criterion | Result and evidence |
| --- | --- |
| A: Failed migration restores previous working configuration | PASS; configuration-backup, registry/store migration, transaction and disposable upgrade/restore checks |
| A: Backups exclude executable and untrusted temporary data | PASS; data-only snapshot validation, executable/symlink/temp/builtin exclusions and bounded-copy tests |
| A: Bounded cleanup preserves latest valid copy | PASS; configurable 1–20 retention, clock-change/latest-valid and interrupted-recovery pin regressions |
| A: Upgrade tests pass | PASS; legacy upgrade, future-schema refusal, blocked-backup preservation, offline restore and live-owner refusal |
| B: Deterministic screen recovery after hotplug | PASS in private Wayland; stable output identity, actual audition-output removal, fallback/return and service-restart checks |
| B: UI usable at tested scales | PASS at 100/125/150/200%; separated outputs, presented native dimensions on all four edges, browser selection and compact editing |
| B: Core keyboard/focus/accessibility tests pass | PASS; resource-backed Studio/icon controls, core accessibility and native Wayland keyboard delivery |
| B: No known unbounded cache or hidden continuous workload | PASS; protected generated-history budget, Canvas mask release, idle overlay expiry, rendering/resource gates and eight-cycle RSS check |
| B: No runtime QML errors in matrix | PASS; real private runtime diagnostic scans with lifetime-safe callbacks and native property corrections |
| B: All 15+15 cards usable; audition leaves no stale host | PASS; all 30 cards at each scale, exact energy controls, hotplug/restart rollback and zero stale hosts |

Physical connector/scanout, another GPU and owner-desktop interaction cells are
**NOT EXECUTED** for proved environment reasons: only one physical output is
connected, no alternate GPU host is available, and the owner's active desktop
must remain untouched. Exact native target commands and observed private
results are in [PLATFORM_MATRIX.md](PLATFORM_MATRIX.md).

Ordinary task cleanup removed both build trees, staged/private roots, task-local
Python and raw diagnostic workareas. Process/path inspection found no task-owned
process retaining them. All six TASK-0043 package deliverables were preserved
with matching pre/post hashes. Required original/control images, renderer states,
native journal evidence and phase/final gate receipts remain in the four verified
files under `build-codex-task-0044/verification-output`.

The consolidated no-diagnostic-artifact gate is **PASS**. The owner explicitly
requested cleanup; KDE's native authentication agent completed removal of only
the two recorded task-owned PlasmaShell dumps, and both paths are verified
absent. The command, ownership evidence and closure are recorded in
[CURRENT_STATE.md](CURRENT_STATE.md#task-0044-verification-and-cleanup-checkpoint--2026-10-03).
The verified checkpoint is `96d608a8440ddcb7bb8f886523fc7131b2b53e59`.
The retained receipt now records completed cleanup while preserving the
historical post-sync audit and all original gate/diagnostic evidence. The final
closure documentation changes are authorized for owner-controlled commit/sync.
No build or runtime gate was repeated for this authentication boundary. No
TASK-0045 release box is closed, and no personal Plasma mutation, global install,
delegated agent or overlapping gate occurred.

## TASK-0045 release verification — 2026-10-03

**TASK-0045 COMPLETE; Phase A verification and cleanup PASS, Git closure
owner-authorized after review.** TASK-0044 is complete at the synced baseline
`9584d204ecfcbf503ccd7e4322ac945424b5d610`. All previously failed executable
checks are resolved with focused, proved native Qt/KDE corrections. There is
no unresolved build, CTest, installed-startup, package or lifecycle failure.
The owner reviewed the fourteen-path result and instructed `Commit and sync`.
This release-evidence commit tracks all approved resources; its exact HEAD,
clean tree and remote parity are verified in the retained TASK-0045 receipt.
This checklist does not declare a published or fully accepted release.

| TASK-0086 / TASK-0045 acceptance | Final evidence and remaining boundary |
| --- | --- |
| Clean repository and tracked release resources | PASS at owner-authorized Git closure: all fourteen approved paths, including three new documents, tracked; clean tree and remote parity verified in the final receipt |
| All mandatory tests and environment observations | PASS: fresh configure/build and 106/106 available CTests; physical second-monitor/scanout/other-GPU observations are NOT EXECUTED for the proved environment/owner-desktop reasons, with exact commands in the platform matrix |
| Unrelated Plasma panels remain untouched | PASS: ownership tests, all affected private matrices, installed startup and final 37/37-phase full native/free lifecycle, with exact native/free sentinel state/token/size preservation |
| No visible UI-only setting | PASS: 57 presented schema fields, capability gates/runtime consumers and non-schema actions reviewed; schema/UI/rendering regressions and installed checks pass; physical visual signoff remains separately unexecuted |
| Exactly 15+15 independent immutable built-ins | PASS: installed Panel/Icon catalogs and actual shared renderer, independent loading, immutability and customized derivative regressions; 8/8 installed QtTest cases and 4/4 with Quick3D masked, including init/cleanup |
| Audition commits/restores exactly and Cancel leaves no residue | PASS: all five audition matrices plus resource-cycle checks; exact restoration, defaults, tokens and temporary host removal assertions remain enforced |
| Reproducible package install/upgrade/uninstall | PASS: two byte-identical normalized exports, native one-job Release package build, 209/209 installed payload files, disposable fresh install, 0.1.0-1 to 0.1.0-2 upgrade, migration/future-version refusal/backup/restore/offline recovery and both removals |
| Release blockers listed explicitly | PASS: commit/sync authorized after review; remaining tag creation, physical observations and licensing/publication decisions are explicit; no executable failure is waived or deferred |

The fresh Debug build uses the audited clone and Quick3D ON. A fresh configure
and all-target build after the production repair passed (1.60 and 6.36 seconds).
The final full available CTest coverage is **106 PASS, 0 FAIL, 0 skipped**,
694.78 seconds summed selected individual times, one worker. All 106 executed
after the production QML repair. The final fixture correction refreshes all
25 shared-lifecycle consumers while reusing 81 unaffected results. Three
checks were refreshed after the final helper parameterization; native command
bytes for the remaining passing matrices are proved unchanged. This provenance
is explicit in the receipt; no extra all-suite rerun is claimed.

The separate final full native/free lifecycle passed **37/37 phases in 659.44
seconds**, using the default Qt render loop and a 900-second bound. Assertions
were retained. Passing native observations include unrelated host preservation,
placement/visibility, virtual-output recovery, real private Plasma and backend
restarts, free-host content rotation and final removal. Runtime uses private
KWin/Plasma Wayland on Radeon 610M/Mesa 26.2.4, Plasma/KWin 6.7.5 and Qt 6.11.2.
The final full logs have no callback TypeError, ReferenceError or binding loop.
Personal-desktop/physical display behavior is not established by these checks.

The package gate passed in **56.77 seconds**, with owner/clone/package source
roots hidden from installed runtime checks. Three installed startup contexts
(normal, Quick3D absent and upgraded) verified the native installed executable
bytes/PID and unrelated hosts. All 209 package members match required resources,
CMake install-manifest bytes and modes. Both native pacman uninstall scenarios
preserve user configuration and remove package-owned content. The existing
migration/backup/restore harness supplies configuration recovery; no parallel
recovery implementation was introduced. No host-global installation occurred.

The original startup problem was Qt QTemporaryDir inheriting a read-only TMPDIR;
the installed fixture now supplies native writable `/tmp`. The lifecycle fixture
pauses only its tracked private Plasma writer during offline edits, isolates
native KDESYCOCA cache use, monitors exact completion signals with bounded waits,
and parameterizes its single unrelated free-host geometry helper for the two
existing native grid contexts. One legacy-control QML callback now uses the
native `connect(root, callback)` lifetime pattern already used by the dock applet.
No C++, CMake, exporter or project dependency changed. Earlier failures and
native Qt/KDE source investigations are preserved in the archive; no assertion
was weakened and compositor CPU load is not claimed eliminated.

Prepared documents are [README](../README.md), [installation and uninstall](INSTALL.md),
[changelog](../CHANGELOG.md) and [known limitations](KNOWN_LIMITATIONS.md).
Authority/master body, preset sequence and logical target structure reflect V3.
The final documentation file/link/anchor audit, shell syntax checks and
`git diff --check` pass. [CURRENT_STATE.md](CURRENT_STATE.md#task-0045-release-verification--2026-10-03)
and [PLATFORM_MATRIX.md](PLATFORM_MATRIX.md#task-0045-release-regression-checkpoint--historical-2026-10-03)
record exact scope, observations and reuse. Physical cells retain proved reasons
and [exact target commands](PLATFORM_MATRIX.md#physical-cells-not-executed).
The licensing decision remains documented in [packaging/LICENSING.md](../packaging/LICENSING.md).

The historical TASK-0045 version plan was application **0.1.0**, Arch package
**0.1.0-2**, proposed annotated tag **v0.1.0** with message `Arch Dock 0.1.0`.
That tag was absent locally and on origin at this checkpoint. The proposed
release-evidence commit target was recorded in its final receipt; commit/sync
approval did not authorize tag creation or publication. The actual later
owner-authorized correction tag and prerelease supersede this proposal, as
recorded in the Repository section above.

The existing normalized exporter produced two byte-identical copies, each with
518 source files and a SOURCE_CHECKPOINT manifest (519 archive members).
Manifest bytes, modes, UID/GID and baseline epoch match every member. The source
catalog's existing original `0600` versus canonical Git/clone `0644` mode is
recorded, with matching bytes and no original mode change. Historical tracked
build entries (117) are excluded by the existing exporter. Final operational
state/checklist documents and PKGBUILD are excluded by its existing rules;
all source-included paths remain frozen after the successful export/build.

| Final artifact | SHA-256 |
| --- | --- |
| `arch-dock-0.1.0.tar.gz` | `bc462317d2e43103e2260df50f8e20e93d87944c978a9677452a6b864149dcb5` |
| `arch-dock-0.1.0-2-x86_64.pkg.tar.zst` | `448a854b860b69fc2b35df66a45d0085c64e918baaabf238bd0fa9ebb621d07a` |

PKGBUILD pins the final source digest. Native `makepkg --verifysource` passed;
`makepkg --cleanbuild --noconfirm` passed in 368.28 seconds with one compile job.
Compact package and verification deliverables remain under ignored
`build-codex-task-0045/package-output/` and `verification-output/`; their JSON
receipts, checkpoint, SHA256SUMS and verified raw-log archive retain the evidence.

The owner-approved commit message is `Prepare Arch Dock release evidence`. Its exact
fourteen-path contents, in first-touch order, are:

```text
docs/MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md
docs/PRESET_SYSTEM_SPEC.md
docs/TARGET_STRUCTURE_TREE_V2.md
tests/run-arch-package-smoke.sh
PKGBUILD
README.md
docs/INSTALL.md
CHANGELOG.md
docs/KNOWN_LIMITATIONS.md
docs/PLATFORM_MATRIX.md
docs/RELEASE_CHECKLIST.md
docs/CURRENT_STATE.md
tests/run-plasma-lifecycle.sh
plasma-widget/contents/ui/main.qml
```

That historical commit contains only TASK-0045. The owner authorized staging,
commit and sync after reviewing its implementation result. Tag creation and
publication required separate owner instruction at that checkpoint; the latest
release-closure section records the subsequent approval and execution. The
contract's owner-controlled Git review was fulfilled by `Commit and sync`.

Cleanup passes. No task-owned process, private session, receiver, applet, panel,
staged installation, probe or generated style cache remains. The exact workarea
`/mnt/F/a45.jclt575i` was removed after raw evidence and final artifacts were
verified. Only six package deliverables, three verification deliverables and
STATE.json remain for TASK-0045. All six TASK-0043 package and four TASK-0044
verification deliverables remain byte-for-byte unchanged.

Two dumps from failed diagnostic setup/teardown were attributed to private
Plasma PID 159706 (QtDBus/libdbus assertion) and Arch Dock PID 171652
(QGuiApplication startup fatal in the invalid setup). Their journal records,
stacks, origin logs and attribution are archived, without claiming the precise
internal assertion mechanism is proved. KDE authentication removed only those
exact root-owned files. All 127 baseline dump files remain present and untouched by this task; no other
new dump remains, and final successful gates produced no additional dump.
No owner-desktop mutation, unrelated cleanup, delegated agent or concurrent gate
was used. Every build/test/runtime gate was serial, with one compile job and
one CTest worker on the 16 GB machine.
