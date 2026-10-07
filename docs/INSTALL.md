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

Opening **Arch Dock** from the application menu runs `arch-dock --settings`
and always shows Panel Studio: it starts Arch Dock when it is stopped, also
after **Quit Arch Dock** (a menu start clears that stop), and brings Panel
Studio forward when Arch Dock is already running or Panel Studio was closed.
Starts in the background never open Panel Studio: D-Bus activation by the
applets or the KWin window watcher, the systemd unit, and a plain `arch-dock`.
All of them use the same executable and atomic D-Bus name guard, so a repeated
start keeps the existing owner. The process remains resident when Panel Studio
closes.

To stop Arch Dock, run `arch-dock --quit` or choose **Quit Arch Dock** on the
application menu entry (right-click it in the launcher). It then stays stopped,
although installed applets and the KWin window watcher would otherwise start it
on demand, until it is started explicitly again (`arch-dock`, the menu entry,
`arch-dock --settings`) or you log in again. Terminate (TERM or INT, as System
Monitor's **Quit Application** sends) stops it the same way. Kill (KILL) cannot
run any code: it is treated as a crash, and the next activation request starts
Arch Dock again. While it is stopped, on-demand activation is refused with
`org.freedesktop.DBus.Error.Spawn.ChildExited` and exit status 75.

In Panel Studio, select the panel and open **Icon Tiles**. **From icon style**
uses the frame or pedestal of the style chosen on **Icons > Appearance**, the
one place an icon style is chosen; a plain icon shows its tile under the
pointer, in the **Tile shape** chosen here. **Custom tile** adds fill color,
opacity, border color and border width independently of the panel surface.
**Show tiles by default** controls the panel default; an explicit
Icon Properties visibility override takes precedence for that entry. The live
preview shows the draft in both 2D and 3D. Apply saves it, while Cancel discards
changes made since the last Apply. Tiles keep the application's real icon glyph
and its interaction area.

In **Panels**, use the arrows beside the tab strip when all tabs do not fit.
Horizontal wheel input or Shift-wheel also pans the tabs. Ordinary vertical
wheel input scrolls the page, including over spin boxes and combo boxes,
without changing their values.

For a free panel, **General** exposes its X and Y position in desktop pixels.
Apply moves the owned widget and checks the actual geometry; a refused move
restores the widget's previous position and saved settings. **Layout** keeps
the layout angle separate from **Perspective tilt**. Tilt appears for a
compatible baked theme within that theme's declared range, or for an active
3D theme as camera pitch. Controls follow the renderer actually available.
An edge panel lays its row out along its edge, so it has no **Layout** page:
Dock layout, scale and padding belong to free panels. A baked look stands its
icons on its artwork's own track and offers no Dock layout; only a closed track
(a ring or polygon) turns.

On an edge panel, **Animations > Opening and closing** holds the resting
state, mechanism, trigger, reveal handle and delays. The default procedural
renderer supports horizontal and vertical collapse; compatible themes expose
their own supported mechanisms. With the **Open** mechanism nothing opens or
closes, so only the mechanism and resting state are shown. Select
**Collapsed** for the resting state, then use its reveal handle with the
chosen trigger to open it. After Apply, **Open panel** and **Close panel**
operate the saved collapsible panel. Cancel discards the draft. Reduced motion
keeps the state changes while suppressing animated transitions and continuous
rotation. A free panel stands open on the desktop and has no opening or closing
settings; one saved collapsed by an earlier version is shown open.

On a free panel with a curved layout (circle, ellipse, polygon, star,
spiral, fan, arc, semicircle or radial), hover the panel and scroll: the icons
move along the panel's own outline while the panel stays still. Scrolling up
moves them clockwise, down moves them back; one wheel notch moves every icon
one place, eased in within 120 ms, and a fast spin never queues up. On a
closed outline the icons go round; on a fan, arc, semicircle, radial or spiral
an icon that leaves one end fades out and comes back at the other, even when
every icon fits. Dragging the panel moves them too. Where the icons have
moved is temporary: it needs no Apply and is never saved.

On **Animations**, **Continuous motion** (Off, Clockwise or Counterclockwise)
keeps them moving, and **Continuous motion moves** chooses what moves:
**Items along the path** (new free panels), **Whole panel**, or **Both**. The
wheel and dragging follow the same choice. **Item travel speed** (icons per
second) and **Panel rotation speed** (degrees per second) appear for what is
moving, with **Motion runs** (always, or while the pointer is over the
panel). **Scroll sensitivity** sets how far one notch, or one icon's distance
of touchpad scrolling, moves the icons: from a quarter to four places. A panel
that rotated before this version keeps turning as a whole after the upgrade;
choose **Items along the path** to make its icons travel instead. Motion
pauses during dragging, editing and popups, and reduced motion makes wheel
steps jump and keeps continuous motion off.

On **Layout**, a fan, arc, semicircle or radial panel offers **Direction**:
Up, Down, Left or Right turns the shape so the middle of its path faces that
side. Layout angle below it still fine-tunes the angle.

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
when it is the initial launcher. Stopping the unit sends TERM, which keeps
Arch Dock stopped as Quit does; starting the unit starts it again.
Installation never starts or enables this unit.

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
tracked modes use Git's `100644`/`100755` intent as `0644`/`0755`. Developer-only
untracked files use `0755` when executable and `0644` otherwise. Arbitrary
read/write permission bits do not affect export. It does not create a commit.
Build output, the outer recipe and the operational
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
`c3b3a0b7771b313c45f843f49a503b45b0d1ada0` and its six current assets unchanged.
The current correction candidate is recorded in [CURRENT_STATE.md](CURRENT_STATE.md);
it does not create or replace a
published release tag. Before tagging, local gates may use an annotated verification
tag only in a disposable repository at the exact finalized candidate commit.

After the intended release tag has been separately authorized and created,
set `release_head` to the independently verified full candidate commit recorded
in its receipt. From the tooling checkout, use new external directories:

```bash
: "${release_head:?set the independently verified full candidate commit first}"
release_tag=v0.1.1
release_checkout="../arch-dock-v0.1.1-source"
release_export="../arch-dock-v0.1.1-export"
git worktree add --detach "$release_checkout" "$release_head"
python "$release_checkout/tools/prepare-arch-source.py" "$release_export"
python tools/verify-tagged-arch-source.py "$release_export" \
    --source-root "$release_checkout" --tag "$release_tag" \
    --expected-head "$release_head"
```

The release verifier must pass before the output is eligible for publication.
It checks clean source, exact HEAD/tag target, checkpoint identity/epoch in
both receipts, source inventory bytes/canonical modes, normalized ownership,
tar/gzip timestamps and digest pins. It independently regenerates the exporter
outputs and requires exact bytes for the archive, external checkpoint, generated
PKGBUILD and SHA256SUMS. Candidate recipes are never executed by verification.
Repeat the clean export into another new directory and require all four outputs
to be byte-identical. Complete extraction/inventory and fresh
package/install verification before publication; run the verifier again against
the final asset directory. Publication needs its own explicit owner authorization.

The existing `v0.1.0` release is a historical prerelease candidate; its initial
precommit-source discrepancy and later corrected publication are recorded in
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
sudo pacman -U arch-dock-0.1.1-11-x86_64.pkg.tar.zst
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

The current correction uses application `0.1.1`, package `0.1.1-11`; its
verification status is recorded in [CURRENT_STATE.md](CURRENT_STATE.md).
The previous verified owner installation is `0.1.1-10`; native pacman performs
the upgrade after candidate verification:

```bash
sudo pacman -U arch-dock-0.1.1-11-x86_64.pkg.tar.zst
```

Cancel an active audition and stop the backend before replacing its executable;
installed applets must not reactivate it during offline configuration recovery.
`arch-dock --quit` (or **Quit Arch Dock** in the application menu) stops it and
keeps it stopped for the rest of the login session; starting Arch Dock again
clears that.
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

Original Arch Dock work is GPL-3.0-or-later unless separately declared. The
package records GPL-3.0-or-later plus MIT for its preserved MIT components.
The complete GPL text is installed as `/usr/share/licenses/arch-dock/LICENSE`;
the component/resource matrix and MIT notice ship in
`/usr/share/licenses/arch-dock/LICENSING.md`. Unknown-rights references remain
NOASSERTION/non-installable and outside relicensing. See
[packaging/LICENSING.md](../packaging/LICENSING.md) for each declaration's scope.

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
    build-codex-arch-package/arch-dock-0.1.1-11-x86_64.pkg.tar.zst \
    /absolute/path/to/arch-dock-0.1.1-10-x86_64.pkg.tar.zst
```

The second package argument is optional for install/uninstall verification;
release upgrade evidence requires it and a strictly older package version.
Create the evidence and temporary directories first. The test also requires
native pacman/bsdtar, desktop-file validation and systemd metadata validation.
It copies the host's native dependency records into its disposable root and
uses pacman there with dependency checks enabled. If Arch Dock is already
installed on the host, native database-only removal clears its cloned record
before the fresh-install check; host files and records are untouched. Read-only
Bubblewrap overlays provide host KDE dependencies and package-owned `/usr` resources while hiding
the checkout. Existing preset tests validate references and render all 30 cards;
the private Wayland session activates the installed executable. Both checks
also run with Qt Quick 3D hidden. An additional private Wayland matrix uses
the installed executable, applet, shared modules and theme assets to check real
URI drops, internal reorder, custom folder glyphs, 2D/3D visibility and page
wheel input. Copied test probes and runtime UI fixtures are explicit test
inputs; the original source checkout and build are hidden. Native removal is
followed by byte checks of user configuration and an audit that every package file is gone. With the
previous package supplied, the same native namespace installs it, upgrades to
the candidate, audits obsolete/new payload files and configuration preservation,
executes installed migration/recovery and startup, then uninstalls again.
The test removes its root and private sessions on exit. Remove only the
verification-owned build, Python and temporary roots after preserving its
deliberate evidence. This verifies the package
against the installed Arch/KDE dependency versions, rather than claiming
an independent distribution-image or physical GPU acceptance test.

## Folder contents

Click a dock folder icon to open it: its children unfold from that icon on a
transparent surface and fold back into it when the folder closes. An ordinary
click opens the selected child. Escape, clicking beside the contents and
clicking elsewhere close the folder; arrow keys move the selection and Enter
opens it.

On a free panel, **Panel Studio > Panels > Behavior > Folder layout** chooses
the shape the folder opens in, outside the dock and along the way the folder
faces out of it:

- **Along the dock** (curved free panels): the children stand on the dock's own
  curve beside the folder.
- **Fan**: a slice-shaped small panel, drawn in the panel's look, its point at
  the folder and the children on its arc. **Fan opening** (40 to 160 degrees,
  90 by default) sets how wide it opens.
- **Grid**: rows of children in a popup beside the folder.
- **Stack**: a straight line of children from the folder outward. **Stack
  length** (2 to 12, 5 by default) sets how many show at once.
- **Arc**: the children on an arc centred on the folder, all at one distance
  from it.
- **Ring**: a second circle beside the folder with the children on it. **Ring
  size** is **Small** (just big enough for the children) or **Same as panel**
  (the dock's own radius, offered on panels that have one).

Fan opening, Stack length and Ring size appear only while their layout is
chosen. Scrolling over the contents moves them along their path: one wheel
notch, or one child's distance of touchpad scrolling, moves them one place,
and a child that leaves one end comes back at the other; on a ring they go
round. In Grid one notch moves one row. On a free panel with a curved layout,
**Scroll sensitivity** on Animations scales this as it scales the panel's own
icons. Where the screen leaves no room in the outward direction, a fan, an
arc, a stack or a small ring first holds fewer children at once, then a shape
turns towards the side that has room.

An edge panel's folders open in a popup beside the folder, as before, and
scroll one child or one row per notch; in a popup you can also hold the left
mouse button and drag the contents, and releasing that drag opens nothing.

**Folder easing** chooses how a folder opens and closes, over **Folder
animation duration**: outCubic glides out and settles, outBack goes a little
past its place and comes back, outElastic snaps out and wobbles, and spring
starts softly and swings past its place once before it settles. Closing plays
the same motion backwards. Reduced motion opens and closes folders at once.

**Always show folder item names** controls the labels beneath the children;
turned off, a child's name appears as a tooltip. **Expand folders on click**
turned off makes a click open the folder itself; the icon's menu still offers
**Show contents…**. Apply saves these for that panel; Cancel keeps the previous
values. The existing bounded snapshot shows up to 48 children.
