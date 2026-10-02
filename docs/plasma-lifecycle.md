# Plasma Lifecycle And Rollback

## Ownership Boundary

Arch Dock treats a numeric Plasma containment id as a locator, not proof of
ownership. Each native panel created by Arch Dock stores the following values in
the containment's `ArchDock` configuration group:

- `ownerToken`: a generated per-association token.
- `panelId`: the matching Arch Dock registry panel id.

The same token is stored as `nativeOwnershipToken` in the registry. Before Arch
Dock changes a native panel's screen or attaches or repairs its visual dock
applet, it reads both containment values and requires an exact match. Permanent
removal additionally verifies the exact expected renderer association and
repeats both proofs inside the destructive Plasma script. This prevents a
recycled Plasma containment id from targeting a user-owned panel.

Legacy associations without a token are adopted only when the recorded applet
is either an `org.archdock.dock` visual applet or an `org.archdock.control`
legacy widget whose `General/panelId` matches the registry panel. Any other
mismatch remains unchanged and is not used as a mutation or replacement target.

## Native Panel Lifecycle Decision Contract

The lifecycle helper defines a pure decision contract. `PanelWindow` executes
the `ShowHost` and `HideHost` intents for temporary native presentation and the
`RecreateMissingHost` intent for verified missing-host recovery and the
`RemoveHostPermanently` intent for explicitly verified destructive removal.

The contract uses these state terms:

- **Record visibility** is the saved Arch Dock request: visible or hidden. It is
  independent from whether a Plasma host exists.
- **Host status** is `Missing`, `Owned`, or `UnownedOrUnverified`. `Owned` means
  the current observation verified the exact Arch Dock ownership token and panel
  id. An `UnownedOrUnverified` host is never a mutation target.
- **Renderer state** records whether the owned host has its Arch Dock renderer
  attached.
- **Presentation** is `Hidden` or `Shown`. It describes the configured host
  presentation used by the decision contract, not a momentary auto-hide
  animation.

Callers request `CreateNew`, `Synchronize`, or `RemovePermanently`. The helper
returns exactly one lifecycle intent:

| Request and observed state | Resulting intent |
| --- | --- |
| `RemovePermanently` with an `Owned` host and verified expected renderer | `RemoveHostPermanently` |
| `RemovePermanently` with an `Owned` host but no verified expected renderer | `NoAction` |
| `RemovePermanently` with a `Missing` or `UnownedOrUnverified` host | `NoAction` |
| `CreateNew` with a `Missing` host | `CreateHost` |
| `Synchronize`, visible record, `Missing` host | `RecreateMissingHost` |
| `Synchronize`, hidden record, `Missing` host | `NoAction` |
| Any non-removal request with an `UnownedOrUnverified` host | `NoAction` |
| Existing `Owned` host without its renderer | `AttachRenderer` |
| Visible record with an attached, `Owned`, `Hidden` host | `ShowHost` |
| Hidden record with an attached, `Owned`, `Shown` host | `HideHost` |
| An `Owned` host already matching the record | `NoAction` |

Permanent removal is evaluated first. Unowned or unverified hosts are then
rejected before attachment or presentation decisions. Missing-host creation or
recreation precedes renderer attachment, which precedes show/hide convergence.

`HideHost` is temporary: it must preserve the containment association, renderer
association, and ownership token. `RemoveHostPermanently` is destructive,
requires an explicit removal request, verified ownership, and the expected
renderer association, and remains a separate operation. For an `empty` panel,
renderer verification means proving that no `org.archdock.dock` applet is
attached. `CreateHost` and `RecreateMissingHost` may create a new owned host only
when no existing host is being targeted.

## Temporary Native Presentation

`PanelWindow::setPanelVisible()` applies native presentation before committing
the registry's `visible` value. It targets only the stored containment after
verifying the exact `ownerToken` and `panelId` again inside the Plasma mutation
script.

For an owned panel in Plasma's `none` hiding mode, temporary hide switches the
host to the supported `autohide` mode and records `temporaryHidden=1` in the
containment's `ArchDock` configuration group. The operation succeeds only when
both the Plasma mode and marker read back correctly. Show targets that same
containment, restores `none`, clears the marker, and verifies both values before
the registry is updated. Neither operation changes the containment id, dock or
control applet ids, or ownership token.

Plasma does not expose a non-revealable manual-hidden mode through its panel
scripting API. A temporarily hidden panel therefore retains Plasma's standard
screen-edge reveal behavior. A host already using another hiding mode is
reported as unsupported for this temporary transition and is left unchanged;
TASK-0020 and TASK-0021 own general auto-hide, dodge, maximized/fullscreen, and
capability-driven visibility policy. Temporary-hide failure never falls back to
containment removal.

## Missing Native Host Recovery

A visible native record whose saved containment id is absent or no longer
resolves is recreated through a candidate transaction. A hidden record with no
host remains unhosted. A saved id that resolves to a present but unverified
containment is not a missing host: Arch Dock leaves it and its registry
association untouched for diagnosis.

The candidate transaction generates a fresh ownership token and asks Plasma to
create one real panel containment. Before the association is published, the
transaction configures placement and visible presentation, writes and reads
back `ownerToken` and `panelId`, attaches `org.archdock.dock` for non-empty panel
types, and verifies exactly one renderer with the expected panel id and type.
The registry then commits the containment id, dock applet id, token, and
`nativeRecoveryState=ready` in one persisted batch.

No candidate id or token is written into the active association before those
checks succeed. A failed attempt clears obsolete association fields and stores
`nativeRecoveryState=recoverable-error` plus a stable `nativeRecoveryError`.
If a candidate exists when a later verification or registry commit fails, the
rollback script removes it only after re-reading the same fresh token and panel
id. A rollback that cannot be verified records `candidate-rollback-failed` and
suppresses another automatic candidate rather than risking a duplicate or an
unrelated containment.

## Permanent Native Removal

Permanent removal first validates that the registry record is a native-edge
panel with a supported panel type. A missing stored containment completes
without a Plasma mutation and clears only the stale native association. A
present containment remains untouched unless legacy adoption or the stored
token proves the exact `ArchDock/ownerToken` and `ArchDock/panelId` pair.

For `launcher`, `tasks`, and `hybrid`, the stored dock applet id must resolve to
the sole `org.archdock.dock` applet and its `General/panelId` and
`General/panelType` must match the registry. An `empty` panel must have no such
applet and a stored dock id of `-1`. The removal script rechecks the containment
token, panel id, exact applet id/type/configuration, or verified empty state in
the same script immediately before `panel.remove()`. Arch Dock then verifies the
containment is absent before clearing its association.

Any query failure, ownership mismatch, renderer mismatch, Plasma refusal, or
failed absence check returns failure without clearing the registry evidence.
The higher-level `removePanel` path preserves the complete panel record when
native removal is refused.

## Free Desktop-Host Lifecycle

A free panel is an `org.archdock.dock` applet in a Plasma desktop containment,
not an independent Arch Dock window. Its registry association contains
`freeDesktopContainmentId`, `freeDockAppletId`, `freeOwnershipToken`, `screen`,
`screenId`, `freeHostMode=desktop`, and `freeHostState`. The owned applet stores
the same panel id and token in its `General` configuration group together with
`panelType=empty` and `bootstrapFreeDock=false`. Numeric containment and applet
ids remain locators; the exact plugin, panel id, token, panel type, and bootstrap
state provide the ownership proof.

Panel Studio creation and the Plasma layout-template route converge on the same
backend transaction. The transaction:

1. reserves a pending free-panel record and ownership token;
2. proves that no host already uses that identity;
3. creates or adopts a desktop applet and verifies its configuration;
4. persists and reads back the complete owned association;
5. removes and verifies absence of the temporary template bridge, when present;
6. performs a final host readback before marking creation complete.

A repeated template request returns the one existing verified association. It
does not allocate another record or applet. A missing, stale, unverified, or
multiple-match result is not treated as duplicate success.

Every failure stage enters the same rollback boundary. A known owned candidate
is removed and its absence is established before the pending record is
discarded. A failure before a host exists discards only the pending record. If
host removal, absence verification, or persistence is uncertain, Arch Dock
retains a token-bound `hosted-stale` recovery record with stable creation and
rollback errors; it does not report record-only success or erase the evidence.

Free-host synchronization uses identity match cardinality:

| Exact live matches | Result |
| --- | --- |
| Zero | Clear host ids/token, retain one `detached` record, and record `owned-host-not-found`. Repeated synchronization is a no-op. |
| One | Re-verify the exact applet configuration and rebind current containment, applet, and screen data. |
| More than one | Record `owned-host-conflict` and preserve the record and every applet without mutation. |
| Query or ownership failure | Preserve the association and report the stable recovery error. |

Permanent free-panel removal rediscovers the token identity, requires exactly
one match, verifies the exact applet again, removes that applet, verifies its
absence, and only then removes the registry record. A safely detached record can
be removed without a Plasma mutation. Conflict, query failure, ownership
mismatch, or uncertain absence preserves the record and does not target an
unrelated applet.

## Free-Panel Content Semantics

A free panel's content type is the registry record's `type`, not the applet's
`panelType`, which a free host stores as `empty` purely as an ownership marker.
The backend applies the record:

- `empty` shows nothing;
- `launcher` shows the panel's own ordered entries;
- `tasks` shows running applications from the shared application model;
- `hybrid` shows the panel's own entries first and running-only applications
  after them. A running instance of a pinned desktop entry is merged into that
  entry: it keeps its identity, label and glyph, gains the running state, and
  carries the application id under `runningAppId` so window actions reach the
  application model.

Free entries are panel-specific local URLs stored in `contentUrls` and ordered
by `contentOrder`, one canonical list of `free-url:` ids that is repaired on
load if it names unknown or repeated entries. They change only through
`addPanelEntries`, `removePanelEntry`, `movePanelEntryBefore` and
`setPanelEntryOrder`, each committed as the panel's next settings revision
through `PanelContentTransaction`; an application id is pinned to a free panel
as its desktop file. Native panels refuse these operations, and the shared
application reorder refuses free ids, so a free panel never reads or writes the
global pin list. A launcher-only free panel does not refetch on task-model
churn; `tasks` and `hybrid` free panels do.

## Runtime Behavior

- Screen add/remove signals cause Arch Dock to resolve each saved stable screen
  id first and use its bounded numeric fallback only when that output is gone.
- A native panel missing its visual dock applet can be repaired later as long as
  its containment ownership marker remains valid.
- A visible record whose host is absent creates and commits one verified
  replacement. Later synchronization reuses that association without creating
  another containment or renderer.
- A stale or unverified native association is never removed, reconfigured, or
  used as the target for applet attachment or replacement.
- Repeated recovery converges on one token-bound containment and renderer; it
  does not duplicate either object.
- Free-host recovery applies the same convergence rule to desktop applets:
  zero matches detach, one verified match rebinds, and multiple matches remain a
  non-mutating conflict.
- Plasma can temporarily report screen `-1` for the desktop containment on a
  disconnected output. Arch Dock preserves the verified free host through that
  interval and requires normal registry/live screen convergence after the output
  returns.
- When Plasma Shell acquires a new D-Bus owner, Arch Dock retries recovery of
  stored native and free associations after the shell has rebuilt its layout. A
  later shell disappearance cancels pending retries, and recovery first verifies
  that the PlasmaShell service is still available.
- Theme previews and raster derivatives are produced in-process by the bounded
  Qt asset processor. Registry shutdown therefore has no ImageMagick, Blender,
  or shell renderer child to cancel or reap.

## Controlled Validation

Run native and free Plasma lifecycle checks only in a disposable Plasma
user/session or after backing up the Arch Dock QSettings file. Do not use
existing personal Plasma panels or desktop applets as test targets.

The opt-in virtual-session harness stages the current build, creates a private
XDG and D-Bus environment, and starts two virtual KWin outputs:

```bash
ARCHDOCK_BUILD_DIR=/path/to/fresh-build bash tests/run-plasma-lifecycle.sh
```

## Preset audition matrix

TASK-0041 uses the existing disposable lifecycle harness with five separately
selected groups. Each group stages the current executable, applets, presets
and `ArchDock.Rendering` module, then starts private D-Bus, KWin Wayland and
PlasmaShell with two virtual outputs. The matrix cannot dispatch outside the
private session. It creates unrelated native-panel and free-widget fixtures
and checks them after every scenario.

| Group | Runtime proof |
| --- | --- |
| `existing` | Existing native preview, streamed placement, exact Cancel/Revert, custom snapshot save/reuse, built-in digest and one Apply revision |
| `temporary` | Incompatible/free preview, live draft theme change, exact Cancel cleanup, one managed-host conversion and temporary native Cancel |
| `icons` | Icon-only renderer/registry isolation, exact Cancel, reusable customized copy, built-in restoration and one Apply revision |
| `recovery` | Service crash during temporary preview and placement preview, conversion-before-adoption interruption, PlasmaShell restart and journal recovery |
| `defaults` | Combined and independent icon defaults for new native/free instances; unchanged existing records; inactive-edit refusal; exact existing-free geometry restoration after Cancel and service crash |

The native widget scripting `geometry` property is readable, but Plasma 6.7's
[`Widget::setGeometry`](https://github.com/KDE/plasma-workspace/blob/Plasma/6.7/shell/scripting/widget.cpp#L158)
does nothing. Existing free-widget rollback therefore sends a bounded command
to that owned applet's configuration. The applet verifies its ID/token, finds
its own desktop layout container, restores its bounds and saves the layout
after refreshing the original renderer configuration. The backend independently
reads back the exact geometry, clears and verifies the command, and retains a
`BLOCKED` recovery record if restoration cannot be proved. Native panel
placement continues through the existing verified Plasma adapter.

After deliberately killing the service, private applets can activate a
replacement through D-Bus. Readiness queries pin the unique name, resolve its
PID, confirm that the owner is unchanged and check that the process is alive.
Only name-disappearance, owner-change or process-exit races retry inside the existing
20-second startup bound; malformed replies and permanent D-Bus errors fail.
This follows the native
[D-Bus ownership APIs](https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-get-name-owner).
All geometry, ownership, revision, isolation and orphan assertions remain
strict. Teardown stops private Plasma before the service to prevent applet
calls from activating another instance during shutdown.

Runtime fixtures need PySide6 as well as the system GTK 4/PyGObject/libei
bindings used by interaction checks. When global PySide6 is absent, use a
disposable environment inside the task build. System-site access lets that
environment read the already-installed native bindings; it installs nothing
globally. The verified October 2026 environment used Qt/PySide6 6.11.2:

```bash
uv venv --system-site-packages --python /usr/bin/python build-codex-task-0041/test-python
UV_CONCURRENT_DOWNLOADS=1 UV_CONCURRENT_INSTALLS=1 UV_CONCURRENT_BUILDS=1 \
  uv pip install --no-cache --python build-codex-task-0041/test-python/bin/python PySide6==6.11.2
```

Build in small target groups with `cmake --build build-codex-task-0041
--parallel 2 --target <targets>`. Run each matrix group individually, replacing
`existing` with the selected group, and stop on a failure:

```bash
env PATH="$PWD/build-codex-task-0041/test-python/bin:$PATH" \
  QT_FORCE_STDERR_LOGGING=1 CMAKE_BUILD_PARALLEL_LEVEL=2 \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/archdock-task0041-parent-bus \
  ctest --test-dir build-codex-task-0041 --parallel 1 --stop-on-failure \
    --output-on-failure -R '^preset-audition-matrix-existing$'
```

The matrix groups are also marked `RUN_SERIAL` in CTest. For a full gate use
batches of at most ten tests, with `panel-window-capability-test`, staged
runtime smoke checks and each matrix group separately. Never overlap gates.
The private runner is bounded to 300 seconds by default (allowed range
30–360); each matrix CTest has a 420-second timeout. The harness copies each
group's runtime logs into `build-codex-task-0041/preset-matrix-<group>/` before
removing its private root. Remove task-owned processes and artifacts after
recording results, preserving pre-existing build directories.

These are real Plasma/Wayland integration checks using virtual outputs.
They do not establish personal-desktop, physical GPU/monitor, hardware hotplug
or release acceptance. Completion evidence is recorded in
[CURRENT_STATE.md](CURRENT_STATE.md#task-0041-implementation--2026-10-02).

It creates an explicitly tracked unrelated Plasma panel with a standard digital
clock before Arch Dock starts. After every managed lifecycle phase it compares
that containment's id, location, hiding mode, screen, complete widget id/type
inventory, and empty Arch Dock ownership fields with the original snapshot.

The managed cases cover cold recovery of a visible record with missing ids,
native containment creation, exact ownership markers, visual dock applet
attachment, temporary hide/show with stable containment/applet ids, safe
rejection of an unsupported temporary transition, legacy-control cleanup,
fallback during virtual-output removal, stable-id restoration after KWin
reorders outputs, visible missing-host replacement, hidden missing-host detach,
repeated synchronization without duplication, stale-id token rediscovery,
multiple-token conflict refusal and convergence, PlasmaShell restart recovery,
and verified permanent removal. Destructive negative cases alter the disposable
managed containment's token or renderer type and prove that both direct removal
and the higher-level record-removal path preserve the containment, applet, and
full registry record until the exact association is restored. The harness
deletes its temporary state after a normal exit and never contacts the running
desktop session.

The free-host cases cover verified Studio and template creation, complete
registry/host ownership, explicit template bridge and control-applet absence,
duplicate bootstrap convergence, failed adoption rollback, stale-id recovery,
zero/one/multiple token matches, output disconnect/restore, PlasmaShell restart,
detached-record idempotence, verified applet removal, and final absence. A
separate unrelated free applet is snapshotted and checked throughout alongside
the unrelated native panel.

TASK-0033 added the free-content cases on the template host: an empty panel
shows nothing even with stored entries, `launcher` shows the panel's ordered
entries, a reorder through `movePanelEntryBefore` changes what is shown and
what is persisted, free ids are refused by the shared application reorder and
by a native panel, `tasks` shows no pinned entry, `hybrid` leads with the
panel's own order, and a ring layout with clockwise rotation reaches the
renderer configuration. The saved order and rotation are re-checked after the
real PlasmaShell restart and after a service restart, and the final verified
removal still leaves no host or record.

TASK-0015 was freshly verified on 2026-08-22 using Arch Linux, Plasma/KWin
6.7.4, Qt 6.11.2, and KF6 6.29.0. The focused C++ test, bootstrap-coordinator QML
test, template contract test, and all 9 configured CTests passed. The expanded
two-output private Wayland lifecycle completed with
`Isolated Plasma native/free lifecycle succeeded.` in approximately 204 seconds,
then removed its temporary root and left no process discoverable with that
private session environment.

The registry test suite also uses a controlled long-running renderer to verify
that registry teardown kills and reaps it. Use a physical disposable Wayland
session when hardware-specific output disconnect/reconnect behavior must be
validated beyond the virtual KWin backend.

## Rollback

Stop the Arch Dock user service and restore the backed-up Arch Dock QSettings
file. Use Arch Dock's native-panel removal path only for a panel whose ownership
marker it verifies. A present but unverified association is left unchanged for
diagnosis rather than being cleared or replaced; remove a non-Arch-Dock panel
through Plasma's own edit mode instead.

For a free panel, use the Arch Dock removal path only while the registry retains
its verified token association. Do not delete a desktop applet by a saved
numeric id alone. If Arch Dock reports a conflict or query failure, preserve the
record and inspect the token matches in a disposable session before attempting
manual cleanup.

## Complete managed profile arrangements

TASK-0042 adds an explicit full-set transaction alongside the existing
single-panel settings and preset audition paths. Every previous owned native
or free host is backed up before mutation. New hosts are verified before old
hosts are removed, and the registry publishes one complete checked set only
after host readback succeeds. Native visibility uses the existing adapter
without an independent settings callback; rollback restores its captured
physical hiding mode and temporary visibility marker.

Ownership still requires the logical ID, exact token and verified native
renderer/free applet association. A numeric ID is never sufficient. Rollback
removes provisional hosts by their recorded tokens, and recreates a missing
previous host only under its original token. Changed physical associations
are committed together. Concurrent registry changes and uncertain ownership
retain a BLOCKED recovery record rather than overwriting another operation.

An interrupted profile transaction blocks automatic lifecycle synchronization
at startup. Opening the store or Studio does not apply a profile. The explicit
Recover Interrupted Apply action retries the authoritative record after
interaction guards clear. The backup and diagnostics are described in
[PROFILE_PACKAGE.md](PROFILE_PACKAGE.md).

The private Wayland check reuses this document's existing staged launcher,
unrelated panel and desktop applet sentinels and teardown. Run it alone:

```bash
ctest --test-dir build-codex-task-0042 --parallel 1 --stop-on-failure \
  --output-on-failure -R '^profile-matrix-apply$'
```

The harness requires the existing PySide6/GTK observer dependencies and starts
its own D-Bus/KWin/Plasma session. It covers data-only capture/import,
native/free set replacement, native auto-hide, write-refusal rollback, and
explicit recovery of a durable interrupted-apply fixture. It does not constitute
physical GPU, monitor hotplug or personal-desktop release acceptance.

The separate shortcut group uses the same private launcher and sentinels:

```bash
ctest --test-dir build-codex-task-0042 --parallel 1 --stop-on-failure \
  --output-on-failure -R '^profile-matrix-shortcuts$'
```

It verifies explicit opt-in, real GlobalAccel key readback, a conflict with
another profile and an occupied KDE key, and unchanged native key owners after
both refusals. Native component `invokeShortcut` activates the renamed stable
profile ID, and the test waits for the authoritative completed full-set
transaction and restored panel settings. Invalid targets create no native
action; disable and deletion are checked against native key readback. This is
real native D-Bus activation evidence, without a claim of physical keyboard
delivery. Run the groups sequentially. Neither harness connects to the
personal session or installs globally.

## TASK-0044 display and resource hardening

The six hardening groups reuse this lifecycle launcher, staged installation,
private bus, KWin/Plasma process ownership and unrelated-panel/applet sentinels.
They run separately: `scale100`, `scale125`, `scale150`, `scale200`,
`hotplug-recovery` and `resources`. Each is registered with CTest as
`wayland-hardening-GROUP-test`, with `RUN_SERIAL` and a 420-second limit.
Use a task-specific build, one build job, one test worker and a short
disk-backed test TMPDIR. Run one group at a time, for example:

```bash
ctest --test-dir build-codex-task-0044/phase-a \
  -R '^wayland-hardening-scale125-test$' --parallel 1 \
  --stop-on-failure --output-on-failure --no-tests=error
```

The scale groups resolve native output names from stable Qt identities,
explicitly separate both output positions, verify scale/topology readback, and
exercise all four native edges on both outputs. The existing KWin probe also
checks presented panel dimensions and output, rather than accepting only
stored containment intent. Each group renders all 15 Panel and 15 Icon Preset
cards, selects every populated library page, delivers real core keyboard
events, checks replaced Canvas mask release and cancels a temporary audition.
Browser fixtures use the actual client size and a queued GUI-thread
`frameSwapped` notification before inspecting the polished click viewport.

The hotplug group verifies that the audition and managed panel request the
output it actually removes. A connected scale change preserves the active
audition; removal of its stable requested output reuses the existing journaled
cancel/rollback path. The group checks deterministic fallback, retained stable
identity, re-enable, service crash/recovery, exact temporary-host removal and
unchanged unrelated sentinels. The resources group performs eight create/cancel
cycles with a bounded RSS-growth discriminator and tests reference-protected
generated history plus idle/recovering overlay expiry.

Every group scans retained runtime logs for genuine QML failures. Private
processes and temporary roots are torn down between groups; build-local logs
remain available during diagnosis. Focused `--energy-regression` and
`--browser-regression` modes run one C++ fixture in the same private compositor
without starting the full Plasma matrix. Set `ARCHDOCK_BUILD_DIR` to the
retained build when using those modes.

The original image measurements, native readback repairs, observed matrix and
physical environment limits are recorded in [PLATFORM_MATRIX.md](PLATFORM_MATRIX.md).
The private virtual matrix uses the host GPU; physical connector/scanout and
other hardware observations are reported separately. It neither restarts the
owner's Plasma session nor installs globally.
