# Post-TASK-0045 corrective pass

Assignment date: 2026-10-03. Initial clean `main` checkout:
`2889cae61deb4ab4ff255319e64098cc80d9a684` (audit baseline).
The standalone assignment authorizes implementation and local verification;
the task-pack planning approval and one-correction stopping rules do not apply
to this pass. All other ownership, serial execution and preservation rules apply.
The owner subsequently instructed “Approved, also commit and Sync after it's
finished”, authorizing the task-local PySide6 prerequisite and Git closure.
One primary agent; one file/change at a time;
one build job and test worker; disposable native runtime only.

The supplied attachment ends mid-sentence in section 9 after
`capability-driven fallback, and .bl`. Sections 10–11 (including the exact
R-01–R-04 definitions and final gates/cleanup) were requested from the owner.
Their absence does not prevent the fully specified issue work below.

Final specified corrective boundary: **AD-01 through AD-05 fixed; native and
full installed-package checks PASS**. All 107 configured CTest names have
applicable passing evidence, combining 65 refreshed names and 42 unchanged
checks from the initial source-matched pass. The full 37-phase lifecycle passes.
The missing assignment tail still prevents exact disposition of R-01 through
R-04 and any additional requirements it contains. Physical acceptance, license
selection, tagging and publication remain open owner boundaries.

This commit records the owner-authorized corrective source/documentation
closure. Exact commit, clean-tree and remote parity are verified after push in
`build-codex-post-task-0045-corrective/FINAL_STATE.json`. The older `STATE.json`
and preliminary receipts retain the initial blocked checkpoint.

## Issue register

| ID | Current applicability and requirement | Disposition/evidence |
| --- | --- | --- |
| AD-02 | Rollback committed restored physical IDs/revisions without journaling that set. Recovery must finalize its own verified durable set and refuse external edits. | Fixed at coordinator boundary; 17/17 QtTest cases and integrated coordinator test pass. Private native matrices and the full lifecycle pass. |
| AD-01 | Import discarded checked-save failure; Clear had no outcome. Artwork and Studio must report persistence failure and retain the draft. | Fixed; real settings-write/SettingsPopup regressions and complete registry/backend tests pass. Private native matrices and the full lifecycle pass. |
| AD-03 | Profile transfer published assets before later validation/final save. Rejected imports must clean only newly owned, unadopted resources and report cleanup failure. | Fixed; 24/24 profile-store QtTest cases and integrated store/manager tests pass. Private native matrices and the full lifecycle pass. |
| AD-04 | Generated root metadata was also eligible input. Export must refuse collisions before publication and preserve normalized valid inputs. | Fixed; four exporter tests and registered CTest pass. Old candidate independently checked consistent. |
| AD-05 | Theme documentation made obsolete current-tense renderer/presentation claims. Guidance must match capabilities, build/runtime options and source. | Fixed; source/contract comparison, links/anchors and whitespace checks pass. |

Each completed entry below records requirement, cause, files, red/green
and control commands/results, and remaining boundaries. Historical 106-test,
37-phase and package receipts are references, not fresh corrective acceptance.

## Release boundaries

R-01 through R-04 are retained as release boundaries, not additional proven
bugs; exact definitions await the missing assignment section. Physical release
acceptance, license selection, tagging and publication remain owner-controlled.
The later explicit approval authorizes corrective commit/sync; it does not
resolve the missing R definitions or grant release publication.

| Boundary | Current disposition |
| --- | --- |
| R-01 | Exact definition unavailable: assignment section 10 missing. |
| R-02 | Exact definition unavailable: assignment section 10 missing. |
| R-03 | Exact definition unavailable: assignment section 10 missing. |
| R-04 | Exact definition unavailable: assignment section 10 missing. |

## Approved native and package continuation

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
receipt hashes. Root `PKGBUILD` now pins this verified checksum.

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
collisions before publication, preserving the original file. Required symlink
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
Quick3D-ON configure also independently confirms build inclusion; that is not
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
