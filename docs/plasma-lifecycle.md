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
mismatch clears Arch Dock's stored association without changing the Plasma
containment.

## Native Panel Lifecycle Decision Contract

The lifecycle helper defines a pure decision contract. `PanelWindow` does not
yet execute these intents; TASK-0007 through TASK-0010 own the corresponding
Plasma mutations, recovery transactions, and runtime evidence.

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

## Runtime Behavior

- Screen add/remove signals cause Arch Dock to resolve each saved stable screen
  id first and use its bounded numeric fallback only when that output is gone.
- A native panel missing its visual dock applet can be repaired later as long as
  its containment ownership marker remains valid.
- A stale or unverified native association is never removed, reconfigured, or
  used as the target for applet attachment.
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

It verifies native containment creation, exact ownership markers, visual dock
applet attachment, legacy-control cleanup, fallback during virtual-output
removal, stable-id restoration after KWin reorders outputs,
stale-containment replacement, PlasmaShell restart recovery, verified removal,
and preservation of pre-existing non-Arch-Dock panel ids. It deletes its
temporary state after a normal exit and never contacts the running desktop
session.

The registry test suite also uses a controlled long-running renderer to verify
that registry teardown kills and reaps it. Use a physical disposable Wayland
session when hardware-specific output disconnect/reconnect behavior must be
validated beyond the virtual KWin backend.

## Rollback

Stop the Arch Dock user service and restore the backed-up Arch Dock QSettings
file. Use Arch Dock's native-panel removal path only for a panel whose ownership
marker it verifies. When a registry association is stale, Arch Dock clears that
association automatically and leaves the existing Plasma panel intact; remove a
non-Arch-Dock panel through Plasma's own edit mode instead.
