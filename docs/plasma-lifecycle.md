# Plasma Lifecycle And Rollback

## Ownership Boundary

Arch Dock treats a numeric Plasma containment id as a locator, not proof of
ownership. Each native panel created by Arch Dock stores the following values in
the containment's `ArchDock` configuration group:

- `ownerToken`: a generated per-association token.
- `panelId`: the matching Arch Dock registry panel id.

The same token is stored as `nativeOwnershipToken` in the registry. Before Arch
Dock changes a native panel's screen, attaches a control applet, or removes the
panel, it reads both containment values and requires an exact match. This
prevents a recycled Plasma containment id from targeting a user-owned panel.

Legacy associations without a token are adopted only when the recorded applet
is an `org.archdock.control` widget whose `General/panelId` matches the registry
panel. Any other mismatch clears Arch Dock's stored association without changing
the Plasma containment.

## Runtime Behavior

- Screen add/remove signals cause Arch Dock to resolve each saved stable screen
  id first and use its bounded numeric fallback only when that output is gone.
- A native panel missing its control applet can be repaired later as long as its
  containment ownership marker remains valid.
- A stale or unverified native association is never removed, reconfigured, or
  used as the target for a control applet.
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

It verifies native containment creation, exact ownership markers, control
applet attachment, fallback during virtual-output removal, stable-id restoration
after KWin reorders outputs, stale-containment replacement, PlasmaShell restart
recovery, verified removal, and preservation of pre-existing non-Arch-Dock
panel ids. It deletes its temporary state after a normal exit and never contacts
the running desktop session.

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