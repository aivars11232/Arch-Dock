# Full audit of Arch Dock, 2026-10-06

## Scope and method

- **Audited source:** `main` at `faed409` (theme package cache correction), with
  package `0.1.1-10` installed on the owner's PC (Arch Linux, KDE Plasma 6.7.5
  on Wayland, Qt 6.11).
- **Read-only phase:**
  - Review of the backend (C++), the shared QML module, both applets, the
    scripts and the packaging.
  - Static analysis: clang-tidy on all 63 C++ sources, qmllint on every QML
    file, shellcheck on every script.
  - The complete test suite in one serial run.
  - A read-only look at the installed service on the owner's PC.
  - Load measurements on private session buses with private configuration
    directories, so nothing reached the owner's desktop or settings.
- **Fix phase:** on the owner's instruction, every confirmed defect was then
  fixed. Each fix has a test that fails on the old code, except the property
  renames, which a linter check covers.

Local evidence (logs, measurement scripts and JSON results) is kept outside the
repository in `build-codex-audit/evidence/`.

## Result

The audit found **eight confirmed defects in `faed409`**. Six affect running
behaviour, one is in the data sent to the panels and one is in the code
structure. All eight are fixed. The suite passed 114/114 before the fixes.

| ID | Defect | Severity | Status |
| --- | --- | --- | --- |
| D-01 | Closed Panel Studio stayed loaded and animating | Medium (resources) | Fixed |
| D-02 | Every window move or resize reloaded every panel | Medium (resources) | Fixed |
| D-03 | Pinned launches bypassed KDE's launcher and its trust check | Medium (security) | Fixed |
| D-04 | Names and markup in tooltips and menus fetched network images | Low–medium (privacy) | Fixed |
| D-05 | Desktop files read with INI rules lost names | Low (correctness) | Fixed |
| D-06 | A hidden skinned 2D panel kept its glow animation running | Low (resources) | Fixed |
| D-07 | Each panel entry carried its icon style definition twice | Low (resources) | Fixed |
| D-08 | QML properties shadowed Qt's own `resources` and `data` | Low (maintenance) | Fixed |

### Comparison with ChatGPT's audit of the same commit

ChatGPT's audit of `faed409` reported **no confirmed runtime or source
defects**, and seven release and governance findings. This audit agrees with
all seven:

- **F-01** (cache fix not packaged or installed): agreed. Package `0.1.1-11`
  carries it.
- **F-02** (no release checklist block for the cache fix): agreed. The
  `0.1.1-11` block records it.
- **F-03** (`main` unprotected): agreed. Governance only, left to the owner.
- **F-04** (owner and physical R-01 acceptance open): agreed. Not automatable.
- **F-05** (Studio does not follow KDE colours and font scale): agreed. See
  [known limitations](../KNOWN_LIMITATIONS.md#panel-studio-presentation).
- **F-06** (historical Mesa teardown fault unexplained): agreed. Unchanged.
- **F-07** (Arch name and logo trademark question): agreed. External to the
  code.

The difference is in runtime behaviour. Each of D-01 to D-04 was reproduced and
measured before it was fixed. ChatGPT's security score (9.3) did not account
for D-03 or D-04.

## Findings

### D-01: Closed Panel Studio stayed loaded and animating

**What happened.** Closing Panel Studio only hid its window. The window, its
QML and its running animations stayed in the service. Qt keeps animating a
hidden window, so the service kept using CPU with Studio closed.

**Measured.**

- On the owner's PC, after Studio had been opened and closed, the live service
  held about 689 MB of memory and used about 2% of a CPU core with nothing
  open.
- In a private session (Release build): the service idled at 0% CPU and 53 MB.
  With Studio open and idle it used 6.9% of a core and 180 MB.

**Fix.** A hidden Studio window is deleted, and the next opening creates a
fresh one (`PanelWindow::settingsWindow`).

**Test.** `PanelWindowCapabilityTest::closedStudioIsReleased` (failed before).

**After.** A temporary probe in the Debug test process measured 0.41% CPU before
opening, 10.0% with Studio open and 0.44% after closing. The memory stays with
the process for reuse: it varied within about 10 MB over five open and close
cycles and did not grow steadily.

### D-02: Every window move or resize reloaded every panel

**What happened.** KWin reports every geometry change of a window. Each report
rebuilt the dock model and announced a new entry revision. Every panel then
fetched all its entries again over D-Bus, although the dock does not show
window geometry.

**Measured (600 geometry updates, a 5-second drag at 120 Hz):**

| Build | Service time per update | Entry announcements | Panel fetches |
| --- | --- | --- | --- |
| Installed `0.1.1-10` (Release) | 5.9 ms | 599 | 599 |
| `faed409` (Debug) | 8.6 ms | 599 | 599 |
| After the fix (Debug) | 0.42 ms | 0 | 0 |

**Fix.**

- `WindowModel` reports only the roles that changed.
- `DockModel` ignores changes to geometry, screen, maximized and full screen.
- Desktop-file fields are cached by file path and read again when the file's
  time or size changes.
- A window's icon name is looked up once per desktop name and window class for
  the session. A window with no icon still gets its desktop file's current
  icon from the dock model. An icon renamed in a desktop file while Arch Dock
  runs shows after a restart.

**Test.** `DockModelTest::onlyEntryRelevantWindowChangesRebuildTheDock` (failed
before).

### D-03: Pinned launches bypassed KDE's launcher and its trust check

**What happened.** A click on a pinned application split its `Exec=` line with
`QProcess::splitCommand`, dropped the field codes and started the program with
`QProcess::startDetached`. New Instance and the desktop actions already used
KDE's launcher (`KIO::ApplicationLauncherJob`); the plain click did not.

**Effects.**

- **No trust check.** KDE refuses to run a desktop file outside the system
  directories unless it is marked executable, so a downloaded `.desktop` file
  cannot run a command by accident. Arch Dock ran any pinned desktop file's
  command on click.
- **Ignored keys.** `Terminal=true` programs started without a terminal.
  `Path=` and D-Bus activation were ignored, and the Desktop Entry quoting
  rules were not followed.
- **Wrong owner.** The started program stayed inside Arch Dock's own systemd
  unit, so systemd counted it as part of Arch Dock and stopping that unit ended
  it. Under the shipped `arch-dock.service`, systemd's default `ExitType=main`
  meant an ordinary Quit of Arch Dock ended every program started from the
  dock.
- **Wrong report.** A D-Bus caller was told the launch had happened when only
  the process start had been attempted.

**Fix.**

- A pinned application starts through `KIO::ApplicationLauncherJob`, which runs
  the trust check, honours `Terminal=`, `Path=` and D-Bus activation, and gives
  the program its own systemd scope.
- A missing program or an untrusted file fails at once.
- A D-Bus caller gets the launcher's real result through a delayed reply. The
  dock shows "Application started" or "Launch failed".
- A pinned file opens with its default application.

**Test.** `DockModelTest::activationOutcomeSeparatesVerifiedLaunchFromRequest`
was rewritten for the new contract. A trusted launcher runs and reports a
start. A broken one fails. An untrusted one fails and its command does not run.

### D-04: Names and markup in tooltips and menus fetched network images

**What happened.** The dock's tooltips and the folder tooltips showed names
through Qt's automatic text format. So did the right-click entries for desktop
actions, whose KDE menu style always uses rich text. A name that looks like
markup is therefore drawn as rich text, and rich text loads the images it
names. Names come from:

- window captions, used for applications without a desktop name;
- file names in pinned folders, where entities can stand in for `/`;
- desktop files.

**Measured.** With a local HTTP server, a name containing
`<img src="http://127.0.0.1:…/x.png">` produced a `GET` request:

- from a plain QML `Text`, as the tooltip uses;
- from a KDE-style menu item.

The escaped name produced no request. With Qt's Basic and Fusion styles, the
same menu path crashed (SIGSEGV in `QQuickTextPrivate::setLineGeometry`). This
is a Qt defect, and the test runner hit it when the menu fix was removed.

**Fix.** A zero-width space follows every `<` in these names, so Qt never
detects a tag. The names look the same. The window list and the folder labels
already used plain text and were correct.

**Tests.**

- `tst_DockEntry::test_tooltipShowsNamesAsPlainText`
- `tst_DockEntry::test_desktopActionsUseTheNamedBackendRouteAndReleaseTheMenu`:
  an action named with an image tag.

### D-05: Desktop files read with INI rules lost names

**What happened.** `QSettings` in INI format read desktop files. INI treats a
comma as a list separator and a semicolon as the start of a comment. A pinned
`Name=Foo, the Bar; and more` showed an empty name, and translated names
(`Name[de]=`) were never used.

**Fix.** Pinning, the dock model and the window watcher read desktop files with
`KDesktopFile`, which follows the Desktop Entry rules and the user's language.

**Test.** `DockModelTest::desktopEntriesAreReadWithDesktopEntryRules` (failed
before).

### D-06: A hidden skinned 2D panel kept its glow animation running

**What happened.** The panel surface tells each renderer when the host has
concealed the panel, for example on auto-hide. The skinned 2D renderer was not
told, so its overlay animation kept running behind a hidden panel.

**Fix.** `PanelSkin2D` takes `sceneConcealed` and stops the overlay while
concealed.

**Test.** `tst_PanelSkin2D` concealment assertions.

### D-07: Each panel entry carried its icon style definition twice

**What happened.** Every entry sent to a panel carried the resolved icon style
definition at the top level and again inside `iconOverrideResolution`.

**Measured.** Nine entries: 135,296 bytes per fetch before the fix, 76,182
after (44% smaller).

**Fix.** The inner copy is removed. Renderers read the top-level one.

**Test.** `PanelWindowCapabilityTest` asserts the resolution no longer carries
it.

### D-08: QML properties shadowed Qt's own `resources` and `data`

**What happened.** qmllint reported three property overrides:

- `PanelScene3D.resources` hid `Item.resources`;
- two `PrincipledMaterial.data` properties hid `QQuick3DObject.data`.

These are Qt's child-object lists. Shadowing them breaks any child object
declared later.

**Fix.** The properties are renamed to `sceneResources` and `values`.

**Check.** qmllint reports no property overrides. The 21 rendering, 3D and
entry tests pass.

## Resource use

Measured with the scripts in local evidence, in private sessions:

| State | Service CPU | Memory | Wake-ups |
| --- | --- | --- | --- |
| Idle, no windows changing | 0% | 53 MB | 0 per second |
| Window drag, before / after D-02 | 5.9 ms / 0.4 ms per update | — | — |
| Panel Studio open (Release) | 6.9% of one core | 180 MB | 2.5 per second |
| Studio closed, before / after D-01 | about 2% / idle level | stays for reuse | — |

A 20-second capture of the session bus on the owner's PC showed no messages
to or from the idle service.

**Window title changes.** A window title change still makes each panel fetch
its entries, because entries carry window titles. A temporary timing probe
split one fetch (Debug, nine entries) as follows:

- reading the panel's settings: 1.5 ms;
- building the entries: 1.8 ms;
- the per-entry loop: 1.2 ms, of which icon styles take 0.6 ms;
- sending the 76 KB reply: about 2.6 ms.

The icon style resolution was made once per fetch instead of once per entry
(`PanelRegistry::IconStyleBatchCache`). That brought a fetch from 8.4 to 7.6 ms
and is covered by `PanelRegistryTest`. The rest needs incremental entry
updates and is recorded in [known limitations](../KNOWN_LIMITATIONS.md#resource-use).

## Static analysis

- **clang-tidy**, bugprone and performance checks, 63 files:
  - 92 `unchecked-optional-access` warnings were each checked. Every access is
    guarded by a state the checker cannot follow, for example
    `isValid()`, `active()` or an earlier `has_value()`.
  - 28 `throwing-static-initialization` warnings concern static string
    constants.
  - The rest are small copy and move suggestions.
  - No defect was confirmed.
- **qmllint**, 300 warnings: the three property overrides (D-08); the rest are
  unqualified-access style and types the linter cannot see. A
  `missing-property` warning for `sceneConcealed` came from the linter reading
  the older installed module in `/usr/lib/qt6/qml`. It goes away when that
  module is left out of the path.
- **shellcheck**, 24 warnings in test scripts: unused variables, dynamic
  `source` and combined declarations. None affects behaviour.

## Not changed by this audit

These are recorded as recommendations or owner decisions:

- **Title-change fetches:** incremental entry updates (see Resource use).
- **CI:** it runs the source gates but does not compile or run the suite.
  Adding a build job would catch build breaks before review.
- **Waiting on Plasma and KWin:** panel creation, placement, removal and some
  preset auditions call Plasma's `evaluateScript` and KWin's scripting
  interface synchronously. One audition step waits in a nested event loop.
  These happen only during user-started actions.
- **Glow animations:** glow and energy layers animate continuously while shown,
  by design. Reduced motion and hiding stop them.
- **Governance and release:** branch protection (F-03), physical R-01 checks
  (F-04), the Studio theming redesign (F-05), the historical Mesa fault (F-06)
  and the trademark question (F-07).

## Verification of the fixes

- Each defect's test failed on `faed409` and passes with the fix.
- The complete configured suite runs once, serially, on the final source before
  packaging. Its result, the package build and checks, and the installation on
  the owner's PC are recorded in [current state](../CURRENT_STATE.md) and the
  [release checklist](../RELEASE_CHECKLIST.md) under `0.1.1-11`.
