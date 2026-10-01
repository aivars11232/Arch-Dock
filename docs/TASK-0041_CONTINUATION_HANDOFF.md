# TASK-0041 paused checkpoint — 2026-10-01

The owner explicitly requested: "Commit and sync current changes, pause and
freeze everything else" so the PC can be switched off. TASK-0041 remains
**incomplete and paused**. Do not resume implementation, builds or gates until
the owner instructs it. The earlier approval to implement the exact TASK-0041
plan is retained; this checkpoint does not replace that plan or mark a phase
complete.

Continuation baseline: `2905761e3dd7fe935d91187e51b1d276e78ece9e`
(`Arch Dock - Add transactional Arch Dock preset audition`). Requirements
remain the consolidated task pack's TASK-0041, execution contract and
PRESET_SYSTEM_SPEC_V3.md. This continuation is a separate checkpoint commit.

## Current changes

- Repair the browser QML assertions and preserve disabled incompatible-card
  actions. The browser and new audition QML tests now pass.
- Add five sequential, disposable private KWin/Plasma runtime matrix groups:
  existing, temporary, icons, recovery and defaults. Cover preview/revert,
  apply, custom save/reuse, defaults, service/Plasma restart and exact cleanup.
- Repair ownership-checked temporary-panel removal, native renderer scripting,
  full preset validation with unchanged inactive source fields, custom snapshot
  renderer-tier inference and new native panel screen-ID handling.
- Restore an existing free widget's exact host geometry through its own
  ownership-checked applet-container command and independent readback. Plasma
  6.7's scripting `Widget::setGeometry` is a no-op; see the
  [native implementation](https://github.com/KDE/plasma-workspace/blob/Plasma/6.7/shell/scripting/widget.cpp#L158).
  The command is bounded and cleared after use; failed restoration retains
  recovery state.
- Share authoritative theme-candidate preparation with audition definitions,
  including unregistered temporary drafts. Active audition theme actions use
  that live projection and filter unavailable controls. The existing public
  `/Control` interface remains unchanged.
- Import the staged renderer module into private Plasma and stop private
  Plasma before the service during teardown to prevent service reactivation.

## Verification captured before pausing

All builds and gates ran sequentially. Builds used at most two jobs; CTest
used one job. The private matrix used virtual outputs and disposable D-Bus,
KWin and Plasma sessions, without changing the personal desktop. These checks
do not establish personal-desktop, GPU, hardware, hotplug or release acceptance.

| Check | Last captured result | Scope of evidence |
| --- | --- | --- |
| Application build | Passed | Includes the latest theme projection changes |
| Focused QML contract, browser, audition and Studio draft | 4/4 passed, 1.53 s | Latest changes |
| Panel registry | Passed, 3.12 s | Includes unregistered theme-draft regression |
| Existing-panel matrix | Passed, 15.86 s | Includes real custom-panel save/reuse; before the final theme projection addition |
| Temporary-panel matrix | Passed, 23.27 s | Latest addition: live temporary-draft theme change, filtered scalar candidate and cancel cleanup; finished before shutdown |
| Icon matrix | Passed, 12.61 s | Before the final theme projection addition |
| Recovery matrix | Passed, 22.06 s | Before the final theme projection addition |
| Defaults matrix | Passed, 35.21 s | Includes exact existing-free geometry/cancel/crash restoration and independent icon defaults; before the final theme projection addition |
| Phase A CTest tests 1–10 | 10/10 passed, 23.34 s | Before the final registry/theme projection changes |
| Phase A CTest tests 11–20 | 10/10 passed, 1.16 s | Before the final registry/theme projection changes |

The suite configures **87 tests**. No full current-tree suite has been run.
Earlier failed matrix attempts were repaired and rerun; they are not completion
evidence. In particular, filenames containing "final" do not establish success.

## Work remaining after an explicit resume

1. Build the remaining test targets in small groups with at most two jobs:
   renderer/capability/settings/content/icon-override transactions; registry,
   folder/window-preview/dock models; panel-window capability separately;
   placement/visibility/Plasma adapter/script-result tests.
2. Complete the full Phase A current-tree build and all 87 available CTests.
   Run batches of at most ten tests, with heavy and private runtime gates
   individually. Use `--parallel 1 --stop-on-failure`; repair any failure and
   rerun its gate before advancing. The partial earlier batches are not full
   closure of the latest source.
3. Complete the task documentation in PRESET_PACKAGE.md, plasma-lifecycle.md,
   CURRENT_STATE.md and RELEASE_CHECKLIST.md, with accurate evidence boundaries.
4. Run the required separate **fresh consolidated build and full CTest gate**
   from a newly configured task-owned build directory, using the same resource
   limits. Do not substitute incremental or historical results.
5. Verify all TASK-0041 acceptance criteria and cleanup before marking complete.

## Environment and cleanup

The session used Plasma/KWin 6.7.5, Qt 6.11.2 and Python 3.14.7. Global PySide6
was missing. A disposable PySide6 6.11.2 environment was created inside ignored
`build-codex-task-0041`; nothing was installed globally. Recreate it on resume
if still needed:

```sh
uv venv --python /usr/bin/python build-codex-task-0041/test-python
UV_CONCURRENT_DOWNLOADS=1 UV_CONCURRENT_INSTALLS=1 UV_CONCURRENT_BUILDS=1 \
  uv pip install --no-cache \
  --python build-codex-task-0041/test-python/bin/python 'PySide6>=6.10,<6.12'
```

Run Python-dependent gates with the venv's bin directory prepended to PATH and
`QT_FORCE_STDERR_LOGGING=1`. Do not run concurrent gates or large build bursts
on the owner's 16 GB machine.

Task-owned runtime sessions, temporary roots, build output and this disposable
environment are removed at this paused checkpoint. Recorded timings above
are retained observations; raw disposable logs are not shipped in Git.
Pre-existing and tracked build directories, including `build-codex-task-0014`,
are preserved. Commit and sync are authorized by the owner's latest request;
further task work is frozen.
