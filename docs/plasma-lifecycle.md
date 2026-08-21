# Plasma Lifecycle And Rollback

## Ownership Boundary

Arch Dock treats a numeric Plasma containment id as a locator, not proof of
ownership. Each native panel created by Arch Dock stores the following values in
the containment's `ArchDock` configuration group:

- `ownerToken`: a generated per-association token.
- `panelId`: the matching Arch Dock registry panel id.

The same token is stored as `nativeOwnershipToken` in the registry. Before Arch
Dock changes a native panel's screen, attaches or repairs its visual dock
applet, or removes the panel, it reads both containment values and requires an
exact match. This prevents a recycled Plasma containment id from targeting a
user-owned panel.

Legacy associations without a token are adopted only when the recorded applet
is either an `org.archdock.dock` visual applet or an `org.archdock.control`
legacy widget whose `General/panelId` matches the registry panel. Any other
mismatch remains unchanged and is not used as a mutation or replacement target;
TASK-0009 owns token-based discovery and stale-association repair.

## Native Panel Lifecycle Decision Contract

The lifecycle helper defines a pure decision contract. `PanelWindow` executes
the `ShowHost` and `HideHost` intents for temporary native presentation and the
`RecreateMissingHost` intent for verified missing-host recovery. TASK-0009 and
TASK-0010 own stale-association recovery and the remaining lifecycle closure.

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
| `RemovePermanently` with an `Owned` host | `RemoveHostPermanently` |
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
requires an explicit removal request plus verified ownership, and remains a
separate operation. `CreateHost` and `RecreateMissingHost` may create a new
owned host only when no existing host is being targeted.

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
association untouched for TASK-0009.

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
- When Plasma Shell acquires a new D-Bus owner, Arch Dock retries recovery of
  stored native associations after the shell has rebuilt its layout. A later
  shell disappearance cancels pending retries, and recovery first verifies that
  the PlasmaShell service is still available.
- Theme rendering subprocesses are cancelled and reaped during registry
  shutdown, avoiding a running ImageMagick or Blender child during application
  teardown.

## Controlled Validation

Run native Plasma lifecycle checks only in a disposable Plasma user/session or
after backing up the Arch Dock QSettings file. Do not use existing personal
Plasma panels as test targets.

The opt-in virtual-session harness stages the current build, creates a private
XDG and D-Bus environment, and starts two virtual KWin outputs:

```bash
bash tests/run-plasma-lifecycle.sh
```

It verifies cold recovery of a visible record with missing ids, native
containment creation, exact ownership markers, visual dock applet attachment,
temporary hide/show with stable containment/applet ids, safe rejection of an
unsupported temporary transition, legacy-control cleanup, fallback during
virtual-output removal, stable-id restoration after KWin reorders outputs,
missing-host replacement, repeated synchronization without duplication,
PlasmaShell restart recovery, verified removal, and preservation of pre-existing
non-Arch-Dock panel ids. It deletes its temporary state after a normal exit and
never contacts the running desktop session.

The registry test suite also uses a controlled long-running renderer to verify
that registry teardown kills and reaps it. Use a physical disposable Wayland
session when hardware-specific output disconnect/reconnect behavior must be
validated beyond the virtual KWin backend.

## Rollback

Stop the Arch Dock user service and restore the backed-up Arch Dock QSettings
file. Use Arch Dock's native-panel removal path only for a panel whose ownership
marker it verifies. A present but unverified association is left unchanged for
TASK-0009 rather than being cleared or replaced; remove a non-Arch-Dock panel
through Plasma's own edit mode instead.
