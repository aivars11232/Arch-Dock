# Arch Dock release checklist

This checklist is derived from section 22 of the
[master architecture and implementation plan](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
An item remains unchecked until its owning task records fresh, reproducible
evidence. Source presence, a historical result, or this checklist itself is not
release evidence.

See [CURRENT_STATE.md](CURRENT_STATE.md) for the current implementation boundary,
[PRESET_SYSTEM_SPEC.md](PRESET_SYSTEM_SPEC.md) for preset and audition gates, and
[TARGET_STRUCTURE_TREE_V2.md](TARGET_STRUCTURE_TREE_V2.md) for the logical target.

## Repository

- [ ] Clean Git state.
- [ ] All required resources tracked.
- [ ] Current documentation accurate.
- [ ] No stale source-of-truth file.
- [ ] Versioned release commit/tag.

## Build and installation

- [ ] Clean build from clone.
- [ ] All tests pass.
- [ ] Staged install verified.
- [ ] Service starts through the documented mechanism.
- [ ] Uninstall leaves no broken Plasma configuration.

## Panel lifecycle

- [ ] Native and free panels create, recover, hide/show, and remove safely.
- [ ] Unrelated Plasma panels remain untouched.
- [ ] No orphan free host.
- [ ] Multi-monitor recovery works.

## Interface

- [ ] Every visible control works or is explicitly experimental.
- [ ] No UI-only 3D setting.
- [ ] Native configuration and Panel Studio use the same schema.
- [ ] Preview matches live renderer.
- [ ] Built-in Panel and Icon preset browsers are separate and use actual renderer output.
- [ ] Exactly 15 valid built-in Panel Presets and 15 valid built-in Icon Presets are installed.
- [ ] Built-ins are immutable and customized derivatives are reusable.

## Rendering

- [ ] Procedural 2D works independently.
- [ ] At least one production skinned-2D family works.
- [ ] Icon styles and per-icon overrides work.
- [ ] Requested icon animations work.
- [ ] Regular panel collapsed/open behavior works.
- [ ] Free-panel rotation and glow work.
- [ ] 2.5D ring/arc works.
- [ ] True 3D, when included, has a safe fallback.

## Interaction

- [ ] Grouped windows can be individually controlled.
- [ ] Context menus expose implemented actions only.
- [ ] Edit mode and drag/drop do not conflict with normal launching.
- [ ] Reduced motion works.
- [ ] Desktop audition changes only the selected/temporary owned host and rolls back exactly on Cancel.
- [ ] Icon Preset audition does not change panel theme/layout/placement.
- [ ] No canceled preview leaves an applet, record, default, or ownership token.

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
