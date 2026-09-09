# Arch Dock release checklist

This checklist is derived from section 22 of the
[master architecture and implementation plan](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
An item remains unchecked until its owning task records fresh, reproducible
evidence. Source presence, a historical result, or this checklist itself is not
release evidence.

See [CURRENT_STATE.md](CURRENT_STATE.md) for the current implementation boundary,
[PRESET_SYSTEM_SPEC.md](PRESET_SYSTEM_SPEC.md) for preset and audition gates, and
[TARGET_STRUCTURE_TREE_V2.md](TARGET_STRUCTURE_TREE_V2.md) for the logical target.

## Repository

- [ ] Clean Git state.
- [ ] All required resources tracked.
- [ ] Current documentation accurate.
- [ ] No stale source-of-truth file.
- [ ] Versioned release commit/tag.

## Build and installation

- [ ] Clean build from clone.
- [ ] All tests pass.
- [ ] Staged install verified.
- [ ] Service starts through the documented mechanism.
- [ ] Uninstall leaves no broken Plasma configuration.

## Panel lifecycle

- [ ] Native and free panels create, recover, hide/show, and remove safely.
- [ ] Unrelated Plasma panels remain untouched.
- [ ] No orphan free host.
- [ ] Multi-monitor recovery works.

## Interface

- [ ] Every visible control works or is explicitly experimental.
- [ ] No UI-only 3D setting.
- [ ] Native configuration and Panel Studio use the same schema.
- [ ] Preview matches live renderer.
- [ ] Built-in Panel and Icon preset browsers are separate and use actual renderer output.
- [ ] Exactly 15 valid built-in Panel Presets and 15 valid built-in Icon Presets are installed.
- [ ] Built-ins are immutable and customized derivatives are reusable.

## Rendering

- [ ] Procedural 2D works independently.
- [ ] At least one production skinned-2D family works.
- [ ] Icon styles and per-icon overrides work.
- [ ] Requested icon animations work.
- [ ] Regular panel collapsed/open behavior works.
- [ ] Free-panel rotation and glow work.
- [ ] 2.5D ring/arc works.
- [ ] True 3D, when included, has a safe fallback.

## Interaction

- [ ] Grouped windows can be individually controlled.
- [ ] Context menus expose implemented actions only.
- [ ] Edit mode and drag/drop do not conflict with normal launching.
- [ ] Reduced motion works.
- [ ] Desktop audition changes only the selected/temporary owned host and rolls back exactly on Cancel.
- [ ] Icon Preset audition does not change panel theme/layout/placement.
- [ ] No canceled preview leaves an applet, record, default, or ownership token.

## TASK-0035 verification record — 2026-09-09

The optional base true-3D renderer passed separate OFF/AUTO/ON Debug builds
and 65/65 CTests in each mode. OFF explicitly disabled Quick3D discovery and
omitted optional resources; AUTO found the installed module; ON required it.
The core executable did not link to Quick3D in any mode.

Required staged installation and private D-Bus/KWin Wayland/PlasmaShell checks
passed with real `org.archdock.dock` applets. Enabled modes rendered the
original mesh theme, changed quality low/high/low within its target bounds,
and exercised missing mesh/texture fallback. OFF selected procedural 2D and
hid 3D controls. Studio Apply, Cancel and reopen passed. Unsupported base-scene
rotation is gated; TASK-0036 motion is not implemented by this task.

This task evidence does not close the broader release checklist above.
Personal desktop, physical GPU/monitor, hardware hotplug, clean-clone release
packaging, and desktop audition acceptance are not claimed by these private
checks. See [CURRENT_STATE.md](CURRENT_STATE.md#task-0035--optional-true-3d-capability-and-base-scene-renderer)
for exact results and [shared-renderer.md](shared-renderer.md#base-true-3d-renderer)
for the implemented limits.
