# TASK-0005 Canonical Baseline Checkpoint

## Checkpoint status

- Task: TASK-0005 — Canonical baseline acceptance and checkpoint record
- Parent work package: AD-0001 — Canonical baseline and repository truth
- Evidence capture: 2026-08-21T17:59:38+02:00, Europe/Amsterdam
- Repository: `/mnt/F/Arch Dock`
- Branch: `main`
- Pre-checkpoint HEAD: `2fb46c8a65c7c2ce7e5f25296b0812d75a08e058`
  (`Task4`)
- Locally recorded upstream: `origin/main` at
  `11d66e7304cca3875640afa55c80d846424b9d4a`
- Pre-checkpoint relationship: zero commits behind and three commits ahead of
  the locally recorded upstream
- Pre-checkpoint working tree: clean

No fetch was performed for this checkpoint. The upstream relationship describes
the local remote-tracking reference, not a claim about the current network
repository.

## Canonical evidence chain

The dependency-ordered baseline is recorded by these commits and documents:

| Task | Commit | Evidence |
| --- | --- | --- |
| TASK-0001 | `11d66e7304cca3875640afa55c80d846424b9d4a` | [Repository baseline audit](audits/BASELINE_AUDIT.md) |
| TASK-0002 | `529004f32929ea0d6b37e1f7d224964c1995879c` | Resource disposition in the baseline audit |
| TASK-0003 | `e1c8b613c90a15020d2262c7c397e9081f2102b2` | [Current state](CURRENT_STATE.md), [master architecture plan](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md), and supporting documentation |
| TASK-0004 | `2fb46c8a65c7c2ce7e5f25296b0812d75a08e058` | [Fresh build baseline report](audits/BASELINE_BUILD_REPORT.md) |

The version-2 master plan from the external task pack is preserved verbatim
after the authority preamble in
`docs/MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md`. The version-2
[preset specification](PRESET_SYSTEM_SPEC.md) and
[target structure](TARGET_STRUCTURE_TREE_V2.md) are also present in the
repository. The diff from the locally recorded `origin/main` through the
pre-checkpoint HEAD includes the authoritative master plan, current-state
documentation, TASK-0002 disposition, and TASK-0004 build report.

The external task pack remains planning input rather than repository source. Its
current single-agent and phase-stop bindings are authoritative for execution;
its stale 84-row index and ledger are not used to renumber the version-2
86-task sequence.

## Source and runtime-resource truth

At the pre-checkpoint HEAD:

- the non-ignored working tree is clean;
- every current CMake-referenced project path exists and is tracked;
- all files in the eight installed Plasma/KWin package directories are tracked;
- `data/themes/builtin-themes.json` is tracked, parses as JSON, is embedded in
  the executable resource set, and is installed as runtime data;
- no file below the ignored repository `build/` or root `CMakeFiles/` directory
  is tracked; and
- executable inputs did not change between TASK-0004's configure baseline and
  the pre-checkpoint HEAD.

The current five-theme catalog is version-1 theme-package data. It is not the
separate 15 Panel Presets plus 15 Icon Presets owned by later tasks.

## Build-directory policy

The repository's ignored `/mnt/F/Arch Dock/build` directory is generated local
state and is never acceptance evidence. Reproducible checks must configure an
out-of-tree directory, preferably under a disposable root made with `mktemp -d`.
Staged installation must use `DESTDIR` or another disposable prefix and must not
write into the live system by default.

TASK-0004 used:

- build: `/tmp/arch-dock-task-0004.E6fLD1/build`;
- stage: `/tmp/arch-dock-task-0004.E6fLD1/stage`; and
- logical install prefix: `/usr` inside that stage.

The retained directory is review evidence, not permanent source. A missing
retained directory does not authorize use of the repository `build/`; reproduce
the documented commands in a new disposable root instead.

## Configure and build summary

TASK-0004 configured a new Ninja Debug build with `BUILD_TESTING=ON`, Qt 6 and
KF6, then built all default targets. Configure exited 0. Build exited 0 with all
68 Ninja steps complete, including `arch-dock`, `panel-registry-test`,
`plasma-script-result-test`, QML type/cache generation, and embedded QML/theme
resources.

Qt reported that the optional `WrapVulkanHeaders` package was unavailable. The
warning did not prevent configuration, compilation, linking, or testing.

## Automated-test summary

The fresh build registered exactly nine CTests. The full suite exited 0:

- 9 passed;
- 0 failed; and
- 0 skipped.

The passing tests were `panel-registry-test`, `plasma-script-result-test`,
`dock-geometry-test`, `motion-policy-test`, `free-entry-policy-test`,
`bootstrap-coordinator-test`, `plasma-template-contract-test`,
`studio-navigation-test`, and `studio-draft-test`.

`tests/run-plasma-lifecycle.sh` is a separate integration harness and was not
run by TASK-0004. Its exact later command against the retained build is:

```bash
ARCHDOCK_BUILD_DIR='/tmp/arch-dock-task-0004.E6fLD1/build' \
  ./tests/run-plasma-lifecycle.sh
```

No isolated or live Plasma lifecycle result is claimed by this checkpoint.

## Stage-install summary

TASK-0004 installed into its disposable `DESTDIR` with exit 0. The generated
install manifest contains 37 records and matches the 37 staged regular files;
the stage contains no symlinks or broken links.

The stage contains the executable, theme catalog, desktop entry, D-Bus service,
systemd user service, KWin script package, two Plasma applet packages, and five
Plasma layout-template packages. All twelve declared source-to-install mappings
compare byte-for-byte equal except the executable, whose install-time RUNPATH is
intentionally rewritten by CMake. Package discovery and JSON, desktop-file,
systemd, ELF, dependency, mode, and old-build-path checks passed.

Nothing was installed globally or into the running user's Plasma/KWin session.

## Known limitations and deferred issues

- No live Arch Dock process, live panel, live KWin script, PlasmaShell restart,
  physical output change, or global installation was exercised.
- The isolated Plasma lifecycle harness remains unexecuted for this baseline.
- The staged systemd user unit expects `%h/.local/bin/arch-dock`, while the
  `/usr`-prefix stage and D-Bus metadata use different executable lookup paths.
  TASK-0082 owns that unification.
- The Debug executable contains the current checkout path in debug information
  and in the existing source-tree KWin-script fallback. Later packaging work
  owns removal or replacement of that source coupling.
- `desktop-file-validate` emitted a non-fatal category hint, and configuration
  emitted the non-fatal optional Vulkan-header warning described above.
- Native/free lifecycle defects, shared-renderer work, presets, profiles,
  packaging, and release validation remain assigned to their later tasks.

There are no known failing or skipped registered CTests at this checkpoint.
Environment-dependent and live-desktop behavior is explicitly unverified rather
than represented as passed.

## TASK-0005 working tree and commit definition

The only permitted TASK-0005 repository changes are:

1. addition of `docs/BASELINE_CHECKPOINT.md`; and
2. a current-evidence refresh of `docs/CURRENT_STATE.md`.

No source, QML, CMake, test, runtime-resource, package, task-pack, or live-desktop
change belongs in the TASK-0005 commit.

Suggested commit message:

```text
Record canonical Arch Dock baseline
```

Codex does not stage or create that commit. A Git commit cannot contain its own
hash as file content without changing the hash. After the user commits the exact
two-path TASK-0005 diff, the canonical baseline commit is resolved
deterministically as the commit that added this file:

```bash
git log --diff-filter=A --format='%H' -- docs/BASELINE_CHECKPOINT.md
```

Later tasks must cite the resulting full hash and verify a clean starting tree.

## Acceptance mapping

| TASK-0005 acceptance criterion | Checkpoint evidence |
| --- | --- |
| Reproducible from source without bundled artifacts | Disposable TASK-0004 configure/build/CTest/stage commands and the build-directory policy above |
| Every remaining modified path is intentional | Exact two-path TASK-0005 commit definition above |
| No missing CMake-referenced runtime file | Fresh path/tracking audit and TASK-0004's complete staged-resource comparison |
| All failures and environment-dependent tests are stated | No registered failures; lifecycle and live-desktop checks are explicitly unexecuted, with the exact harness command recorded |
| Later tasks can cite a precise baseline commit | Deterministic creation-commit command above, to be resolved after the user commits |

## Next task boundary

After the user commits this exact checkpoint and later work verifies its full
hash and a clean working tree, the first permitted next task is **TASK-0006 —
Define the native panel lifecycle contract**. TASK-0005 approval does not
authorize TASK-0006 planning or implementation.
