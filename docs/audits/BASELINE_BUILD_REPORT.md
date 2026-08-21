# TASK-0004 Fresh Build Baseline Report

**Status:** COMPLETE
**Evidence date:** 2026-08-21, Europe/Amsterdam
**Repository:** `/mnt/F/Arch Dock`
**Disposable task root:** `/tmp/arch-dock-task-0004.E6fLD1`

## Scope and result

The current Arch Dock executable inputs were configured, built, tested, and
stage-installed from a new out-of-tree build directory. The repository's
ignored `build/` directory and its bundled executable were not used as
validation evidence. No source, build-system, test, runtime-resource, or live
desktop state was changed.

The only TASK-0004 repository change is this report. The disposable task root
is retained for review; it can be removed after the evidence is no longer
needed.

## Repository checkpoint

| Field | Observed value |
| --- | --- |
| Root | `/mnt/F/Arch Dock` |
| Branch | `main` |
| HEAD when configure/build/test began | `529004f32929ea0d6b37e1f7d224964c1995879c` |
| HEAD before this report was written | `e1c8b613c90a15020d2262c7c397e9081f2102b2` (`task3`) |
| Upstream state before this report | `main...origin/main [ahead 2]`; 0 behind, 2 ahead |
| Working tree before this report | clean |

HEAD advanced while the checks were running because the pre-existing
TASK-0003 documentation was committed externally. The range
`529004f32929ea0d6b37e1f7d224964c1995879c..e1c8b613c90a15020d2262c7c397e9081f2102b2`
changes only these nine documentation files:

- `AUTONOMOUS_PROGRESS.md`
- `docs/CURRENT_STATE.md`
- `docs/MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md`
- `docs/MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN_V2.md`
- `docs/PRESET_SYSTEM_SPEC.md`
- `docs/RELEASE_CHECKLIST.md`
- `docs/TARGET_STRUCTURE_TREE_V2.md`
- `docs/implementation-audit.md`
- `docs/plasma-lifecycle.md`

No CMake, C++, QML, test, package, or installed-resource input changed in that
range, so the fresh executable evidence still represents current HEAD.

## Host and toolchain

| Component | Observed value |
| --- | --- |
| OS | Arch Linux rolling release |
| Kernel | Linux 7.1.8-arch1-3 x86_64 |
| Session | KDE Plasma on Wayland; `XDG_SESSION_TYPE=wayland`, `WAYLAND_DISPLAY=wayland-0` |
| Plasma Shell / KWin | 6.7.4 / 6.7.4 |
| CMake / CTest | 4.4.2 / 4.4.2 |
| Ninja | 1.13.2 |
| GCC | 16.2.1 |
| Qt base / declarative | 6.11.2-2 / 6.11.2-1 |
| Kirigami | 6.29.0-1 |
| KCoreAddons / KConfig / KService | 6.29.0-1 |

The fresh cache confirms `BUILD_TESTING=ON`, `CMAKE_BUILD_TYPE=Debug`, Ninja,
`CMAKE_INSTALL_PREFIX=/usr`, source `/mnt/F/Arch Dock`, and Qt 6/KF6 package
configuration under `/usr/lib/cmake`.

## Fresh configure and build

The dedicated paths were:

| Purpose | Path |
| --- | --- |
| Source | `/mnt/F/Arch Dock` |
| Build | `/tmp/arch-dock-task-0004.E6fLD1/build` |
| DESTDIR stage | `/tmp/arch-dock-task-0004.E6fLD1/stage` |
| Logical install prefix inside the stage | `/usr` |

Configure command:

```bash
cmake -S '/mnt/F/Arch Dock' \
  -B '/tmp/arch-dock-task-0004.E6fLD1/build' \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCMAKE_INSTALL_PREFIX=/usr
```

Result: exit 0. Configuration and generation completed. Qt emitted one
non-fatal optional dependency warning because `WrapVulkanHeaders` was not
found; it did not prevent Qt GUI discovery, generation, compilation, linking,
or test execution.

Build command:

```bash
cmake --build '/tmp/arch-dock-task-0004.E6fLD1/build' --parallel 2
```

Result: exit 0; 68 of 68 Ninja steps completed. This built and linked
`arch-dock`, `panel-registry-test`, and `plasma-script-result-test`, generated
the QML type registration and cache sources, and compiled the embedded QML and
theme resources. The installed ELF exposes the expected Qt resource
initializers for the ArchDock QML module, its QML cache, raw QML resources, and
the theme catalog.

## Test inventory and results

The registered test inventory was captured before execution with:

```bash
ctest --test-dir '/tmp/arch-dock-task-0004.E6fLD1/build' -N
ctest --test-dir '/tmp/arch-dock-task-0004.E6fLD1/build' --show-only=json-v1
```

Both commands exited 0 and listed exactly nine tests. Generated CTest commands
use test executables in the disposable build tree, `/usr/lib/qt6/bin/qmltestrunner`,
and test inputs in the current source checkout. No command uses
`/mnt/F/Arch Dock/build`.

Full-suite command:

```bash
ctest --test-dir '/tmp/arch-dock-task-0004.E6fLD1/build' --output-on-failure
```

Result: exit 0; 9 passed, 0 failed, 0 skipped; total real time 1.09 seconds.

| Test | Result | Time |
| --- | --- | ---: |
| `panel-registry-test` | Passed | 0.50 s |
| `plasma-script-result-test` | Passed | 0.01 s |
| `dock-geometry-test` | Passed | 0.06 s |
| `motion-policy-test` | Passed | 0.05 s |
| `free-entry-policy-test` | Passed | 0.05 s |
| `bootstrap-coordinator-test` | Passed | 0.29 s |
| `plasma-template-contract-test` | Passed | 0.01 s |
| `studio-navigation-test` | Passed | 0.06 s |
| `studio-draft-test` | Passed | 0.06 s |

### Test not registered with CTest

`tests/run-plasma-lifecycle.sh` is a separate manual integration harness. It
starts an isolated KWin/Plasma Wayland session, changes virtual outputs inside
that session, and is not part of the registered CTest suite. It was not run
under the approved TASK-0004 plan, so no Plasma lifecycle result is claimed.
The exact later command against this build is:

```bash
ARCHDOCK_BUILD_DIR='/tmp/arch-dock-task-0004.E6fLD1/build' \
  ./tests/run-plasma-lifecycle.sh
```

No registered test was skipped or unable to run on this host.

## Stage installation

Install command:

```bash
DESTDIR='/tmp/arch-dock-task-0004.E6fLD1/stage' \
  cmake --install '/tmp/arch-dock-task-0004.E6fLD1/build'
```

Result: exit 0. Nothing was installed into the live `/usr`, user data
directories, Plasma session, or KWin session.

The generated install manifest has 37 entries, and it matches the 37 regular
files below the stage exactly. The stage has zero symlinks and zero broken
links.

| Installed resource group | Files |
| --- | ---: |
| `usr/bin/arch-dock` | 1 |
| Theme catalog | 1 |
| Desktop entry | 1 |
| D-Bus service | 1 |
| systemd user service | 1 |
| KWin script package | 2 |
| Control Plasma applet | 5 |
| Dock Plasma applet | 15 |
| Five Plasma layout-template packages | 10 |
| **Total** | **37** |

All twelve declared source-to-install mappings compare byte-for-byte equal:
the theme catalog; desktop, D-Bus, and systemd descriptors; KWin package; two
Plasma applet packages; and five layout-template packages. CMake intentionally
rewrites the executable's install RUNPATH, so the staged ELF is not expected
to be byte-identical to the build-tree ELF.

## Staged-artifact validation

The following checks completed successfully:

- every staged JSON file parsed with `jq empty`;
- `desktop-file-validate` exited 0, with a non-fatal hint that the desktop
  entry has two main categories (`Utility` and `System`);
- `systemd-analyze --user verify` exited 0 for the staged user unit;
- isolated `kpackagetool6 --show` discovery succeeded for both Plasma applets,
  the KWin script, and all five layout templates;
- `file`, `readelf`, and `ldd` confirmed a 64-bit x86-64 PIE ELF and resolved
  all shared libraries;
- the build-tree and staged ELF have the same build ID and `NEEDED` libraries;
  the staged RUNPATH is the intended `$ORIGIN:$ORIGIN/../lib`;
- the executable mode is 0755; and
- the exact scan for `/mnt/F/Arch Dock/build` returned no match in either the
  fresh generated tree or the staged tree.

The native package lookup used an isolated KDE data search without installing
anything:

```bash
XDG_DATA_HOME='/tmp/arch-dock-task-0004.E6fLD1/xdg-data-home' \
XDG_DATA_DIRS='/tmp/arch-dock-task-0004.E6fLD1/stage/usr/share:/usr/local/share:/usr/share' \
  kpackagetool6 --type '<package-type>' --show '<plugin-id>'
```

An initial `kpackagetool6 --show --packageroot ...` diagnostic returned exit 3.
KPackage handles `--show` before resolving `--packageroot`, so the isolated
`XDG_DATA_HOME`/`XDG_DATA_DIRS` route above was used and all eight lookups then
exited 0. This was a validator-invocation issue, not a package defect.

An initial byte comparison of the build-tree and staged executables also
returned exit 1. The focused ELF diagnostic confirmed the expected CMake
install-time RUNPATH rewrite and otherwise identical build IDs and direct
library requirements; no build or install failure remained.

## Absolute-path findings

Neither the staged tree nor the fresh generated build tree contains the old
bundled-build path `/mnt/F/Arch Dock/build`. The generated CTest file names the
fresh `/tmp/arch-dock-task-0004.E6fLD1/build` directory and current source test
files only.

The Debug ELF does contain the *current* checkout path in debug information.
It also contains
`/mnt/F/Arch Dock/kwin-script/contents/code/main.js` from the existing
`ARCHDOCK_SOURCE_KWIN_SCRIPT_PATH` fallback. These are not references to the
old bundled build directory and were not used to locate the executable or
installed packages. They are nevertheless absolute current-source coupling
and are recorded here rather than silently treated as relocatable packaging.

## Existing adjacent issues, unchanged

- `data/arch-dock.service` starts `%h/.local/bin/arch-dock`, while this
  `/usr`-prefix stage installs `usr/bin/arch-dock` and the D-Bus service uses
  `/usr/bin/env arch-dock`. TASK-0082 owns unifying those startup paths.
- The current-source KWin fallback and Debug source paths remain as disclosed
  above; release/Arch packaging still needs its later dedicated work.
- The desktop category hint and optional Vulkan-header warning are non-fatal
  baseline observations.
- The isolated Plasma lifecycle harness remains unexecuted in this task.

No adjacent issue was changed to make this baseline green.

## Acceptance mapping

| TASK-0004 acceptance criterion | Evidence | Result |
| --- | --- | --- |
| Fresh configuration completes | New Ninja/Debug/BUILD_TESTING configuration in the disposable task root exited 0. | PASS — directly executed |
| Full build completes | All 68 default Ninja steps completed and all targets linked. | PASS — directly executed |
| Exact CTest results, including tests outside Plasma | Nine registered tests: 9 passed, 0 failed, 0 skipped; the separate Plasma lifecycle harness is explicitly recorded as not executed with its exact command. | PASS — automated tests plus explicit manual-test boundary |
| Stage contains all declared runtime resources and no old absolute path | 37 manifest entries exactly match 37 staged files; source mappings, package discovery, descriptors, ELF, links, and the old bundled-build-path scan were checked. Current Debug/source-fallback paths are disclosed separately. | PASS — stage execution and inspection |
| No bundled binary used as evidence | Configure, build, CTest executables, and staged ELF all came from `/tmp/arch-dock-task-0004.E6fLD1/build`; `/mnt/F/Arch Dock/build/arch-dock` was never invoked. | PASS — directly observed |

No live application, live Plasma panel, live KWin script, or global install was
started or mutated. TASK-0004 establishes build and automated-test truth; it
does not claim live desktop behavior.

**Suggested commit message:** `Verify clean Arch Dock baseline build`
