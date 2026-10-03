# Arch Dock installation and startup

Arch Dock targets Arch Linux, KDE Plasma 6 and Wayland. The application,
Plasma applets, KWin watcher, shared QML renderer and resource catalogs must
be installed together. The supported Arch package route is the checksum-pinned
local source archive and PKGBUILD described below.

## Session startup

**Direct session D-Bus activation is authoritative.** The installed service
descriptor owns `org.archdock.ArchDock` and runs the installed executable.
The native Plasma applet's service watcher can request that activation when
the applet needs its backend. There is no desktop autostart entry or automatic
systemd enablement.

The desktop application entry and manual `arch-dock` command use the same
executable and atomic D-Bus name guard. Repeating the command keeps the
existing owner; `arch-dock --settings` opens Panel Studio through that owner.
The process remains resident when its settings window closes.

An explicit activation request in the user's Plasma Wayland session is:

```bash
gdbus call --session --dest org.freedesktop.DBus \
    --object-path /org/freedesktop/DBus \
    --method org.freedesktop.DBus.StartServiceByName org.archdock.ArchDock 0
```

The native reply is `1` for activation or `2` for an existing owner. Either
route converges on the same service. A disconnected bus, unsuccessful name
registration or failed command forwarding produces an error and a failing
exit status. If D-Bus reports `Spawn.ExecFailed`, inspect the installed
descriptor's `Exec` value and verify that executable and its dependencies.

## Optional manual systemd control

The installed `/usr/lib/systemd/user/arch-dock.service` uses `Type=dbus` and
`BusName=org.archdock.ArchDock`. It has no `[Install]` section. Starting it is
an explicit alternative entry into the same executable/name guard:

```bash
systemctl --user daemon-reload
systemctl --user start arch-dock.service
```

Use the D-Bus route normally. If an owner was already started independently,
systemd cannot adopt that process as its main process. D-Bus activation and
the executable guard still prevent a second backend; use systemd control
when it is the initial launcher. Installed applets may reactivate a needed
backend after it stops, so stopping a manual unit is not an applet-disable
mechanism. Installation never starts or enables this unit.

## Configuration recovery and upgrades

Before upgrading, stop the Arch Dock backend and ensure installed applets will
not immediately reactivate it. Recovery commands acquire the same session
D-Bus name as the running service and refuse a live owner. They use Qt Core
and do not require a display:

```bash
arch-dock --backup-config
arch-dock --list-config-backups
arch-dock --restore-config-backup BACKUP_ID
```

Capture prints an ID; listing prints valid IDs in creation order. Restore
validates the whole snapshot and saves the current configuration before any
replacement. A failed write rolls back; an interrupted restore leaves a
private journal that is recovered before normal startup. An unresolved profile
apply journal must be recovered through Profiles before configuration recovery.
Never remove a recovery journal to force a restore.

Version-1 snapshots live in `AppDataLocation/config-backups` (normally
`~/.local/share/Arch Dock/Arch Dock/config-backups`). They include registry
settings, user profiles and managed assets, user panel/icon presets and defaults,
managed theme sources, and profile shortcuts. Installed immutable built-ins,
generated render caches, hidden temporary data and executable files are excluded.
Symbolic links are refused. Copies are bounded to 256 MiB and 4096 files,
owner-only, and verified with SHA256 before publishing the manifest. External
source artwork is preserved in place during legacy migration; migration writes
its managed copy only after a pre-migration snapshot succeeds.

The `backup/retentionCount` QSettings key defaults to 5 and is clamped to 1–20.
Cleanup preserves the newest valid snapshots and any copies pinned by pending
restore recovery. Invalid/incomplete snapshot directories are removed by the
same cleanup. Restore is data-only and does not restore native Plasma host
ownership; the existing profile transaction owns host rollback. Package removal
continues to preserve all user configuration and backups.

Automatic snapshots precede destructive registry migration, unmarked user-preset
store adoption, actual legacy-profile rewrites, and every profile apply. Loading
a profile remains read-only. Future schema versions remain untouched and
rejected; use a compatible application version or an explicitly selected older
backup for downgrade recovery.

## Build and disposable installation

For a local production build, use a fresh directory and one compile job:

```bash
cmake -S . -B build-local -DCMAKE_INSTALL_PREFIX=/usr \
    -DARCHDOCK_ENABLE_QUICK3D=AUTO -DBUILD_TESTING=OFF
cmake --build build-local --parallel 1
DESTDIR="$PWD/build-local/stage" cmake --install build-local
```

`DESTDIR` stages a logical `/usr` installation. Its metadata must keep
`/usr/bin/arch-dock`, without the staging directory. For a different logical
prefix, `cmake --install build-local --prefix /absolute/prefix` generates
metadata using that prefix. The binary destination follows
`CMAKE_INSTALL_BINDIR`. A staged prefix needs its own private session's data
and QML search paths; the runtime test below supplies them. Copying a staged
tree to an unrelated prefix does not rewrite its startup metadata.

Qt Quick 3D is optional. `AUTO` includes support when the build module is
available; `OFF` excludes it; `ON` requires it. A build that includes 3D still
checks runtime capability and falls back safely when the module is missing.

## Arch package

For a developer package, export the approved current source into a new
directory, then use native makepkg:

```bash
python tools/prepare-arch-source.py build-codex-arch-package
cd build-codex-arch-package
makepkg --verifysource
makepkg --cleanbuild --noconfirm
```

The exporter records the Git HEAD and every included file's bytes and mode
in `SOURCE_CHECKPOINT.json`. It includes approved working-tree changes and
produces a normalized source archive plus a PKGBUILD pinned to its SHA256;
it does not create a commit. Build output, the outer recipe and the operational
`CURRENT_STATE.md`/`RELEASE_CHECKLIST.md` and
`POST_TASK_0045_CORRECTIVE_REPORT.md` records are excluded from the archive
to avoid circular hashes. The recipe never uses `SKIP`. The checkout recipe's
checksum identifies the verified task checkpoint; export again after source
changes instead of mixing that checksum with a different archive.

### Tagged release source

Official release assets must come from the exact finalized annotated tag,
using a clean checkout with no untracked source inputs. A later commit with
similar source bytes cannot substitute for the recorded checkpoint HEAD or
epoch. Keep the published `v0.1.0` tag fixed at
`c3b3a0b7771b313c45f843f49a503b45b0d1ada0`.

From the current tooling checkout, use new external work/export directories:

```bash
release_checkout="../arch-dock-v0.1.0-source"
release_export="../arch-dock-v0.1.0-export"
git worktree add --detach "$release_checkout" 'refs/tags/v0.1.0^{commit}'
python "$release_checkout/tools/prepare-arch-source.py" "$release_export"
python tools/verify-tagged-arch-source.py "$release_export" \
    --source-root "$release_checkout" --tag v0.1.0 \
    --expected-head c3b3a0b7771b313c45f843f49a503b45b0d1ada0
```

The release verifier must pass before the output is eligible for publication.
It checks clean source, exact HEAD/tag target, checkpoint identity/epoch in
both receipts, source inventory bytes/modes, archive timestamps and the recipe
and checksum digest pin. Repeat the clean export into another new directory
and require identical archive bytes. Complete extraction/inventory and fresh
package/install verification before publication; run the verifier again against
the final asset directory. Replacing existing published assets needs explicit
owner authorization and does not move the tag or change prerelease status.

The existing `v0.1.0` release is a published prerelease candidate; its initial
precommit-source discrepancy and corrected candidate status are recorded in
[CURRENT_STATE.md](CURRENT_STATE.md). Build the tag-matched generated recipe:

```bash
cd "$release_export"
makepkg --verifysource
makepkg --cleanbuild --noconfirm
```

After retaining the verified artifacts, remove the owned checkout with
`git worktree remove "$release_checkout"` from the tooling checkout and remove
only the positively identified temporary export/build directories. Main's
later public-documentation corrections do not rewrite the fixed tag's
historical source documents.

The root `SOURCE_CHECKPOINT.json` archive path is reserved for generated
metadata. A tracked or nonignored untracked source file at that path is refused
before archive publication; move or explicitly resolve the input collision
before retrying. The exporter never silently discards that file. Ordinary
nested files with the same name and legitimate compressed fixtures remain
supported. Source symbolic links, including dangling links, are refused.

The recipe uses one compile job, disables LTO/debug splitting, and installs
under `/usr`. Required dependencies cover Qt Quick/Wayland, SVG, KDE service
and KIO integration, global shortcuts, Plasma and KWin. Qt Quick 3D is an
explicit build dependency so the package includes its optional renderer. It
is an **optional runtime dependency**: the executable does not link against
Qt Quick 3D, and ordinary rendering works when the QML module is absent.
KPipeWire is optional for live window thumbnails. Install the recipe's
declared build dependencies before running makepkg; it does not install them.

After reviewing the resulting package, the owner can install or remove it:

```bash
sudo pacman -U arch-dock-0.1.0-2-x86_64.pkg.tar.zst
```

Installation places the executable, direct D-Bus descriptor, manual systemd
unit, desktop entry, Plasma packages/layout templates, KWin watcher, shared
QML module, themes, icon styles, animation profiles and both 15-entry built-in
preset catalogs together. It neither starts nor enables a service. Launch
Arch Dock through the desktop entry, `arch-dock`, or the documented D-Bus
request. Removal deletes package-owned files and preserves user configuration
and user presets. It does not remove panel records from the user's Plasma
configuration. No cache rebuild or Plasma restart was needed in the verified
private install/startup flow; there are no package hooks to perform either.

## Upgrade

The current candidate is application `0.1.0`, package release `2`. The retained
previous package is `0.1.0-1`; native pacman performs the version upgrade:

```bash
sudo pacman -U arch-dock-0.1.0-2-x86_64.pkg.tar.zst
```

Cancel an active audition and stop the backend before replacing its executable;
installed applets must not reactivate it during offline configuration recovery.
Use the backup commands above before an owner-controlled upgrade. Start the
installed service normally after upgrading. Destructive schema migration saves
a validated configuration snapshot first; future schemas remain untouched.

Package upgrade preserves user configuration, presets, profiles and backups.
The package gate checks native version readback and payload bytes/modes, then
uses the existing migration/restore fixture with the upgraded `/usr/bin/arch-dock`.

## Uninstall

1. Cancel or revert any active desktop audition.
2. Remove managed native/free panels through Arch Dock's ownership-checked
   removal actions while the service and applets are still installed.
3. Stop the backend, then remove the package:

```bash
sudo pacman -R arch-dock
```

Package removal preserves user configuration, user presets, profiles and
backups. It does not rewrite personal Plasma configuration or remove unrelated
panels/widgets. Removing managed panels before the package prevents a retained
Plasma record from referring to an uninstalled Arch Dock applet. Reinstallation
can reuse the preserved user data. No global cache rebuild, Plasma restart or
automatic service enablement is part of the package route.

Existing component and asset license declarations are preserved. A
project-wide license has not been selected; the package's
`LicenseRef-Arch-Dock-Unspecified` and installed licensing notice record that
fact. See `packaging/LICENSING.md` in the checkout, installed as
`/usr/share/licenses/arch-dock/LICENSING.md`.

## Private verification

With the project's test dependencies available, use a fresh test build and a
short disk-backed temporary root. The native fixtures use PySide6 plus the
system GI/GTK bindings; this checkpoint uses PySide6 `6.11.2`. Keep that
dependency environment local to the verification task:

```bash
task_build="$PWD/build-codex-verification"
task_python="$PWD/build-codex-verification-python"
task_tmp="$(mktemp -d /var/tmp/ad.XXXXXX)"
uv venv --system-site-packages --python /usr/bin/python "$task_python"
uv pip install --no-cache --python "$task_python/bin/python" PySide6==6.11.2
export PATH="$task_python/bin:$PATH"
export TMPDIR="$task_tmp"
cmake -S . -B "$task_build" -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_AUTOGEN_PARALLEL=1 -DARCHDOCK_ENABLE_QUICK3D=ON \
    -DARCHDOCK_TEST_TMPDIR="$task_tmp"
cmake --build "$task_build" --parallel 1
ctest --test-dir "$task_build" --parallel 1 --stop-on-failure \
    --output-on-failure --no-tests=error -R '^session-startup-'
```

The runtime check uses a disposable D-Bus session, virtual KWin Wayland and
private PlasmaShell configuration. Bubblewrap hides the source checkout,
leaving the installed resources and copied test helpers visible. The check
requires `bwrap`, `gdbus`, the existing lifecycle harness dependencies and
Python with PySide6; these are test dependencies. It verifies the installed
owner, single activation, repeated launch/settings forwarding and unrelated
native/free sentinels, then removes its private session and installation.
Run the complete available CTest suite serially before packaging. Enumerate
the configured tests with `ctest --test-dir "$task_build" -N`, use small
`-I first,last` batches, and run each private/heavy runtime check alone. Reconcile
all configured names with the final results so no test is skipped. The source
checkout's [current state](CURRENT_STATE.md) and [release checklist](RELEASE_CHECKLIST.md)
record acceptance evidence.

After building the package, test it without installing into the host system:

```bash
mkdir -p "$PWD/build-codex-package-evidence"
ARCHDOCK_BUILD_DIR="$task_build" \
ARCHDOCK_PACKAGE_INSTALL_MANIFEST="$PWD/build-codex-arch-package/src/build/install_manifest.txt" \
ARCHDOCK_PACKAGE_EVIDENCE_DIR="$PWD/build-codex-package-evidence" \
TMPDIR="$task_tmp" \
bash tests/run-arch-package-smoke.sh \
    build-codex-arch-package/arch-dock-0.1.0-2-x86_64.pkg.tar.zst \
    /absolute/path/to/arch-dock-0.1.0-1-x86_64.pkg.tar.zst
```

The second package argument is optional for install/uninstall verification;
release upgrade evidence requires it and a strictly older package version.
Create the evidence and temporary directories first. The test also requires
native pacman/bsdtar, desktop-file validation and systemd metadata validation.
It copies the host's native dependency records into its disposable root and
uses pacman there with dependency checks enabled. Read-only Bubblewrap overlays
provide host KDE dependencies and package-owned `/usr` resources while hiding
the checkout. Existing preset tests validate references and render all 30 cards;
the private Wayland session activates the installed executable. Both checks
also run with Qt Quick 3D hidden. Native removal is followed by byte checks of
user configuration and an audit that every package file is gone. With the
previous package supplied, the same native namespace installs it, upgrades to
the candidate, audits obsolete/new payload files and configuration preservation,
executes installed migration/recovery and startup, then uninstalls again.
The test removes its root and private sessions on exit. Remove only the
verification-owned build, Python and temporary roots after preserving its
deliberate evidence. This verifies the package
against the installed Arch/KDE dependency versions, rather than claiming
an independent distribution-image or physical GPU acceptance test.
