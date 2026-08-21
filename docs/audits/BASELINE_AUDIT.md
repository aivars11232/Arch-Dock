# TASK-0001 Repository Baseline Audit

## Audit status

- Task: TASK-0001 — Read-only repository inventory and baseline audit
- Parent work package: AD-0001 — Canonical baseline and repository truth
- Repository: /mnt/F/Arch Dock
- Evidence capture: 2026-08-21T16:00:25+02:00, Europe/Amsterdam
- Audit implementation: this document only
- Product behavior changed: no
- Build, test, installation, package registration, service activation, Plasma
  restart, KWin mutation, panel mutation, Git staging, commit, fetch, or push:
  not performed

This audit reports facts observed from the current checkout and host. Conclusions
and deferred actions are kept in separate sections. Historical documents and the
bundled build tree are not treated as proof of current behavior.

## Evidence boundaries

### Source facts

- The working tree was inspected without resetting, cleaning, restoring, or
  modifying any pre-existing dirty item.
- Git comparisons use the locally stored refs/remotes/origin/main. No fetch was
  performed, so ahead/behind is not a statement about the current remote server.
- Runtime inspection was read-only: version queries, user-bus name discovery,
  D-Bus introspection, package listing, and service-state queries.
- The ignored build directory was inspected only as a stale artifact. No command
  from that build is accepted as current build or test evidence.
- The external task pack was inspected at
  /mnt/F/Arch_Dock_Codex_Task_Pack_v2. It is not installed into this repository.

### Inspection limitations

- qtpaths6 is not installed or not on PATH. Qt version evidence therefore comes
  from the Arch package database.
- An exploratory find under /mnt/F encountered the protected lost+found
  directory. A non-recursive /mnt/F inventory and explicit target/pack checks
  supplied the relevant archive evidence.
- No network Git operation was authorized. The locally stored origin/main ref
  was last updated on 2026-07-24.
- No live panel lifecycle test was run because TASK-0001 is documentation-only
  and the repository warns that lifecycle checks must use a disposable Plasma
  session or backed-up configuration.

## Task-pack provenance and current integrity

### Pack inventory

The current pack exposes:

| Item | Observed count or state |
| --- | ---: |
| Individual task files | 86 |
| Individual Codex prompt files | 86 |
| Epic files, AD-0001 through AD-0018 | 18 |
| Sections in ALL_TASKS.md | 86 |
| Tasks declared by task_manifest.json | 86 |
| Epics declared by task_manifest.json | 18 |
| Last manifest task | TASK-0086 |

The individual task set, ALL_TASKS.md, and task_manifest.json describe the
86-task sequence. TASK-0001 has no dependency.

### Checksum state

The fresh command sha256sum -c SHA256SUMS.txt reported 115 matches and 88
failures:

- CODEX_EXECUTION_CONTRACT.md differs from the ZIP/checksum copy because the
  current file has appended single-agent, phase-stop, and no-add/commit/push
  bindings. The current file is the governing contract.
- TASK_INDEX.md has 84 rows and ends at TASK-0084, while the manifest has 86
  tasks and ends at TASK-0086.
- TASK_LEDGER.csv has 84 data rows and ends at TASK-0084. Its current header also
  uses planning_approved and commit_hash rather than the ZIP copy's plan_approved
  and commit names.
- Tasks TASK-0002 through TASK-0086, 85 files, each have the same appended
  no-agent and phase A/phase B stop block. Removing that exact suffix makes every
  one of those 85 files byte-identical to its ZIP/checksum version.
- TASK-0001 still matches its checksum. The 86 per-task prompt files retain the
  checksum version and therefore do not include the newly appended task-file
  suffix.

These are current integrity facts, not repairs. This task does not modify the
pack, index, ledger, prompts, or checksum manifest.

## Source-control baseline

### Repository identity

| Field | Observed value |
| --- | --- |
| Repository root | /mnt/F/Arch Dock |
| Current branch | main |
| HEAD | b0b9e779750fac0578162cfa348cab31791093c8 |
| HEAD subject | fix: make free dock bootstrap and interaction reliable |
| Upstream | origin/main |
| Stored origin/main | 8da157ca6bffcef416b5ca2f0d241516f0034a2c |
| Stored ref update | 2026-07-24T18:29:22+02:00 |
| Staged changes | none |
| Tracked files | 93 |

The fresh comparison command returned:

~~~text
$ git rev-list --left-right --count origin/main...main
0	29
~~~

For that command, the left count is commits reachable only from origin/main and
the right count is commits reachable only from main. Therefore this checkout is
0 behind and 29 ahead of the locally stored tracking ref.

### Local-only history

The required local-range graph was:

~~~text
* b0b9e77 (HEAD -> main) fix: make free dock bootstrap and interaction reliable
* b34f4ac fix: apply Panel Studio screenshot corrections
* a62fe5e fix: make Panel Studio overview read-only
* e0f44e6 feat: reorganize Panel Studio navigation
* a0583c7 fix: align free dock icons and apply themes
* 055bcc1 fix: isolate and render multishape panel layouts
* 45e8225 fix: open Panel Studio from dock configure action
* e1a4fca fix: isolate free docks from task model churn
* f697ca1 fix: decouple typing from dock animation refresh
* dec01ec fix: implement all dock animations off the UI thread
* 4dab387 fix: refresh live dock visuals after settings changes
* 266c398 fix: apply free dock themes and animation settings
* d082188 fix: bootstrap transparent free panels correctly
* 303e043 fix: restore multishape dock animations
* 6cd2198 fix: keep themed free panels online
* 3f1451d fix: unpin content from free panels
* 3018e80 fix: isolate free panel drops from edge docks
* 7796a01 fix: keep free docks empty until populated
* 3a55a20 fix: host free docks as native Plasma widgets
* 4e68979 revert: remove unsafe Plasma edit mode integration
* 11108d1 fix: expose free panels through Plasma edit overlay
* 4d65b5e fix: expose unchanged free panels above Plasma edit canvas
* 6400b05 fix: only expose free panels during Plasma edit mode
* 3c3cc51 fix: forward Plasma edit state from dock containments
* 707dd90 feat: integrate free panels with Plasma edit mode
* dc6fab5 feat: add free docks through Plasma edit mode
* 72394f2 feat: add movable freeform dock panels
* e400298 Bundle KWin window state below DBus argument limit
* eab83c1 Stabilize KWin bridge numeric marshalling
~~~

### Complete non-ignored dirty tree

There are no staged changes. Every non-ignored dirty item is classified below.

| Status | Path | Classification | Current observation |
| --- | --- | --- | --- |
| D | Arch dock features.txt | Documentation/requirements | Tracked 2,001-line file is deleted in the working tree. |
| M | CMakeLists.txt | Build configuration | Tracked; adds six lines for the built-in theme resource and its install input. |
| M | qml/runtime/SettingsPopup.qml | QML source | Tracked; adds 66 lines for catalog loading and appearance controls. |
| M | qml/runtime/StudioForm.qml | QML source | Tracked; adds 71 lines for built-in theme cards. |
| M | src/PanelRegistry.cpp | C++ source | Tracked; adds 63 lines for catalog loading and theme application. |
| M | src/PanelRegistry.h | C++ interface | Tracked; adds four lines for themeDefinitions and applyTheme. |
| ?? | data/themes/builtin-themes.json | Runtime theme data; unresolved | One untracked, valid JSON catalog containing five built-in themes. CMake currently references it. |

The tracked diff reports 210 insertions and 2,001 deletions across six tracked
files. The untracked JSON is not included in that diff statistic.

TASK-0001 neither endorses nor rejects these pre-existing changes. It preserves
them as user-owned state for TASK-0002 and later work to resolve.

### Ignored and generated state

git status --short --ignored --untracked-files=all reported 274 ignored files:

| Ignored path group | File count | Classification |
| --- | ---: | --- |
| build/ | 271 | Generated CMake, Ninja, Qt, binary, test, and cache output |
| CMakeFiles/CMakeSystem.cmake | 1 | Generated CMake metadata outside build/ |
| .vscode/settings.json | 1 | Local editor configuration |
| .directory | 1 | Local KDE directory metadata |

The build/ tree contains 271 files and 68 directories. No file under build/ or
CMakeFiles/ is tracked. The repository has one non-ignored untracked file and
274 ignored untracked files.

## Build-system and install-input inventory

### CMake source facts

CMakeLists.txt currently declares:

- CMake 3.21 minimum, project ArchDock 0.1.0, and C++20.
- Qt 6 Core, DBus, Gui, Network, Qml, and Quick.
- KF6 Kirigami.
- qt_standard_project_setup requiring Qt 6.8.
- One executable target, arch-dock.
- One QML module, URI ArchDock version 1.0.
- One theme catalog resource at data/themes/builtin-themes.json.
- Nine registered CTest tests when BUILD_TESTING is enabled.
- Installation of the executable, theme catalog, KWin script, desktop entry,
  two Plasma applets, five layout templates, D-Bus service metadata, and a
  systemd user unit.

All literal install input paths in the current working tree exist. The theme
catalog input exists only as an untracked file, so the current Git source state
does not yet define a reproducible checkpoint.

### QML and package surfaces

The executable QML module includes:

- qml/runtime/DockGeometry.js
- qml/runtime/FreePanelWindow.qml
- qml/runtime/IconProperties.qml
- qml/runtime/IconPropertiesWindow.qml
- qml/runtime/StudioDraft.js
- qml/runtime/StudioForm.qml
- qml/runtime/StudioNavigation.js
- qml/runtime/SettingsPopup.qml

The installed Plasma/KWin packages discovered in source are:

| Source package | KPackage identifier |
| --- | --- |
| kwin-script | org.archdock.windowwatcher |
| plasma-widget | org.archdock.control |
| plasma-dock-widget | org.archdock.dock |
| plasma-layout-template | org.archdock.plasma.desktop.emptyPanel |
| plasma-layout-template-launcher | org.archdock.plasma.desktop.launcherPanel |
| plasma-layout-template-tasks | org.archdock.plasma.desktop.tasksPanel |
| plasma-layout-template-hybrid | org.archdock.plasma.desktop.hybridPanel |
| plasma-layout-template-free | org.archdock.plasma.desktop.circularFreeDock |

The applet has native Plasma configuration files at
plasma-dock-widget/contents/config/main.xml and config.qml.

### Test inventory

CMake registers these nine tests:

1. panel-registry-test
2. plasma-script-result-test
3. dock-geometry-test
4. motion-policy-test
5. free-entry-policy-test
6. bootstrap-coordinator-test
7. plasma-template-contract-test
8. studio-navigation-test
9. studio-draft-test

tests/run-plasma-lifecycle.sh is a separate manual lifecycle harness and is not
registered with CTest. No test was run for this documentation-only task.

## Bundled-build audit

### Source facts

The ignored build cache reports:

| Field or artifact | Observed value |
| --- | --- |
| CMAKE_HOME_DIRECTORY | /mnt/F/Arch Dock |
| CMAKE_BUILD_TYPE | Debug |
| CMAKE_GENERATOR | Ninja |
| CMAKE_INSTALL_PREFIX | /usr/local |
| build/CMakeCache.txt timestamp | 2026-08-01T17:22:55+02:00 |
| build/build.ninja timestamp | 2026-08-01T17:52:38+02:00 |
| build/arch-dock timestamp | 2026-08-01T18:08:20+02:00 |
| Files matching checkout/home absolute paths | 14 |

Examples containing absolute paths include build/build.ninja,
build/cmake_install.cmake, build/CTestTestfile.cmake, generated Qt resource and
module files, moc_predefs.h files, test binaries, and build/arch-dock.

### Conclusion

The pack's older statement that the bundled build came from a different source
directory is not true for the current CMake cache: it names /mnt/F/Arch Dock.
The build is nevertheless stale relative to the 2026-08-21 working tree, is
ignored, embeds machine-specific paths, and uses /usr/local. It is not evidence
that current source configures, builds, tests, or stage-installs successfully.

## Host and runtime-resource state

### Platform versions

| Component | Observed value |
| --- | --- |
| OS/kernel | Arch Linux, Linux 7.1.8-arch1-3 x86_64 |
| Session | KDE on Wayland, WAYLAND_DISPLAY=wayland-0 |
| Plasma Shell | 6.7.4 |
| KWin | 6.7.4 |
| Qt base | 6.11.2-2 |
| Qt declarative | 6.11.2-1 |
| Kirigami | 6.29.0-1 |
| KCoreAddons/KConfig/KService | 6.29.0-1 |
| CMake/CTest | 4.4.2 |
| Ninja | 1.13.2 |

### Read-only runtime observations

- The user bus owns org.kde.KWin and org.kde.plasmashell.
- org.kde.PlasmaShell exposes evaluateScript.
- org.kde.kwin.Scripting exposes start, loadScript, isScriptLoaded, and
  unloadScript.
- No org.archdock bus name was present.
- command -v arch-dock returned no executable.
- kpackagetool6 2.0 listed no org.archdock applet, KWin script, or layout
  template in the queried package types.
- arch-dock.service was inactive and not found by the user systemd manager.

These facts describe only this session. They are not a request to install,
register, start, or mutate anything.

## KDE-native mechanisms and repository architecture

### Native mechanisms confirmed

Official KDE documentation exposes native building blocks relevant to later
tasks:

- Plasma scripting can create, enumerate, configure, and remove Panel objects,
  including location, alignment, size, visibility mode, and widgets:
  https://develop.kde.org/docs/plasma/scripting/api/
- Desktop containments inherit widget placement and expose addWidget with
  geometry:
  https://develop.kde.org/docs/plasma/scripting/api/
- Plasma applets support persisted KConfig schemas and native configuration
  pages through main.xml, config.qml, ConfigModel, and cfg_ properties:
  https://develop.kde.org/docs/plasma/widget/configuration/
- KWin supports packaged scripts and window/edge APIs:
  https://develop.kde.org/docs/plasma/kwin/
  https://develop.kde.org/docs/plasma/kwin/api/

### Current source facts

- PanelWindow::createNativeKdePanel begins at src/panel/PanelWindow.cpp:1535 and
  uses Plasma panels for edge-panel ownership.
- PanelWindow::synchronizeFreePanels at lines 875–881 deletes legacy utility
  windows and states that free docks are hosted by Plasma desktop containments.
- PanelWindow::createFreePanelFromTemplate at lines 996–1105 verifies an
  ownership token, obtains desktopForScreen, calls desktop.addWidget for
  org.archdock.dock, writes panelId/panelType configuration, and removes the
  temporary bridge panel.
- WindowWatcher::loadKWinScript at src/WindowWatcher.cpp:197–215 prefers an
  installed KWin script and falls back to the CMake-compiled source-tree path.
- The native Plasma dock applet has the expected metadata, UI, and KConfig
  configuration files.

### Architecture conclusion

Native edge panels should continue to use Plasma Panel/containment APIs.
Desktop-positioned free docks have a native Plasma Desktop containment route in
the current source. KWin scripting is an appropriate adapter for window state,
window actions, global edges, and compositor behavior, but it is not evidence
that an arbitrary window is a Plasma panel. User-level KWin window rules may be
a fallback for properties that have no project-owned API, but they would mutate
user configuration and therefore require explicit ownership and rollback rules.

## Contradictions and unresolved repository truth

### Free-panel documentation versus source

- AUTONOMOUS_PROGRESS.md:30 says arbitrary-coordinate free panels are retired
  and legacy records migrate to a bottom edge.
- docs/implementation-audit.md:85–88 says free panels are independent Qt Quick
  utility surfaces.
- Current PanelWindow.cpp:875–881 and 996–1105 instead host free docks as
  org.archdock.dock widgets in Plasma desktop containments and remove legacy
  utility windows.

Conclusion: the current source and two status documents describe three
different architectures. TASK-0001 records the contradiction; it does not
select or rewrite an authoritative document.

### Executable and startup-path disagreement

| Consumer | Current executable route |
| --- | --- |
| CMake install | install target to prefix/bin |
| Desktop entry | Exec=arch-dock |
| D-Bus service | Exec=/usr/bin/env arch-dock |
| systemd user unit | ExecStart=%h/.local/bin/arch-dock |

Conclusion: the systemd unit assumes a user-local path while CMake's default
install prefix currently targets /usr/local and the desktop/D-Bus entries rely
on PATH. A later task must establish one authoritative install/start contract.

### Duplicate geometry implementation

qml/runtime/DockGeometry.js and
plasma-dock-widget/contents/ui/DockGeometry.js are not byte-identical:

~~~text
0db9c6d7f8e9fc6d5a4a27003cac5c4765be01e62e6f584720f089e553d6377c  qml/runtime/DockGeometry.js
4d7ab185272eca12cd18b0d8351d117f29e16256f26ab722c7b425e0df56449c  plasma-dock-widget/contents/ui/DockGeometry.js
~~~

Conclusion: geometry logic has two independently changing copies. No
deduplication is authorized in TASK-0001.

## Plans, preset specifications, samples, and catalogs

### Presence facts

- SOURCE_MASTER_PLAN_V2.md, the alternate v2 master-plan copy,
  PRESET_SYSTEM_SPEC.md, TARGET_STRUCTURE_TREE_V2.md, and the task-ID migration
  document exist in the external task pack.
- Those v2 plan/specification/structure files are absent from the target
  repository.
- The inspected /mnt/F top level contains Arch Dock.zip and the task-pack ZIP.
  It does not contain Arch Dock(10).zip or the named Arch Dock icon/panel sample
  archive on which the external plan says it was based.
- No data/presets directory exists in the target.
- The only current built-in catalog is the untracked
  data/themes/builtin-themes.json.
- That JSON is valid format org.archdock.theme-catalog version 1 and contains
  five themes: Obsidian Glass, Neon Segments, Metallic Shelf, Holographic Ring,
  and Minimal Underline.
- docs/theme-packages.md documents versioned panel-theme packages, not the
  separate Panel Preset, Icon Preset, and Profile resources required by the v2
  contract.

### Conclusion

The five-theme catalog must not be counted as the required 15 built-in Panel
Presets plus 15 built-in Icon Presets. The missing sample archives and absent
in-repository v2 specifications are provenance gaps, not permission to invent
assets or schemas.

## Known blockers and deferred work

1. The working tree is not a reproducible source checkpoint because tracked
   source/configuration is modified, tracked requirements documentation is
   deleted, and a CMake input is untracked. TASK-0002 remains necessary.
2. Current architecture/status documentation contradicts current source.
   TASK-0003 remains necessary to establish authoritative project documents.
3. No fresh configure, build, test, or stage install has established current
   build truth. TASK-0004 remains necessary.
4. Startup metadata, KWin source fallback, duplicate geometry logic, preset
   resource types, and sample provenance remain later-task concerns.
5. The task-pack checksum manifest is intentionally or operationally stale
   relative to current binding additions and the 86-task index/ledger state.
   Later task execution must read current task files and the current contract,
   not rely on SHA256SUMS.txt alone.

## Exact clean build and staged-install procedure for TASK-0004

These commands are specified for later execution. They were not run by
TASK-0001. They avoid the repository's ignored build/ directory and stage the
installation under a disposable root instead of writing to /usr or /usr/local.

~~~bash
ARCHDOCK_TASK0004_ROOT="$(mktemp -d --tmpdir arch-dock-task-0004.XXXXXX)"
cmake -S "/mnt/F/Arch Dock" -B "$ARCHDOCK_TASK0004_ROOT/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$ARCHDOCK_TASK0004_ROOT/build" --parallel 2
ctest --test-dir "$ARCHDOCK_TASK0004_ROOT/build" --output-on-failure
DESTDIR="$ARCHDOCK_TASK0004_ROOT/stage" \
  cmake --install "$ARCHDOCK_TASK0004_ROOT/build"
find "$ARCHDOCK_TASK0004_ROOT/stage" -type f -print | sort
~~~

TASK-0004 must record the temporary root, command exit statuses, full failing
output if any, CTest summary, and staged file manifest. It must stop at the
first unresolved configure, build, test, or install failure rather than
reusing the bundled build.

## Acceptance mapping

| TASK-0001 acceptance criterion | Evidence and result |
| --- | --- |
| No source, configuration, theme, test, or build-system changes | Confirmed by the final Git status/diff review; this audit is the only TASK-0001 file. |
| Identify every dirty-tree item and classify it | Confirmed by the seven-row complete non-ignored dirty-tree table and ignored-state table. |
| Record actual ahead/behind rather than ZIP assumption | Confirmed by fresh origin/main...main output: 0 behind, 29 ahead relative to the stored tracking ref. |
| Identify exact TASK-0004 clean-build and stage-install commands | Confirmed by the command block above. Commands are specified but intentionally not executed in this task. |

## Command evidence summary

The material read-only commands used for this audit were:

~~~text
git rev-parse --show-toplevel
git branch --show-current
git rev-parse HEAD
git status --short
git status -sb
git status --short --ignored --untracked-files=all
git rev-list --left-right --count origin/main...main
git log --oneline --decorate --graph origin/main..main
git reflog show -1 refs/remotes/origin/main
git diff --stat
git diff --numstat
git ls-files
git ls-files --others --exclude-standard
git ls-files --others --ignored --exclude-standard
find, rg, sha256sum, cmp, jq, stat, and diff for repository/package evidence
uname, pacman -Q, plasmashell --version, kwin_wayland --version
cmake --version, ctest --version, ninja --version
busctl --user list
gdbus introspect for org.kde.plasmashell and org.kde.KWin
kpackagetool6 package listings
systemctl --user is-active/is-enabled arch-dock.service
~~~

No command in this audit establishes build success, test success, installed
runtime behavior, or live panel behavior. Those claims remain explicitly
unverified until their owning tasks run in the required environment.
