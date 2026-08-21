# Arch Dock current state

> **Status authority:** This file is the sole current-state source of truth for
> the repository. The architecture requirements are authoritative in
> [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
> Older progress and audit narratives are historical evidence, not current
> implementation claims.

**Evidence snapshot:** 2026-08-21T16:38:12+02:00 (Europe/Amsterdam). Unless a
statement is explicitly described as runtime evidence, it comes from repository
and source inspection during TASK-0003.

## Repository state

- Repository root: `/mnt/F/Arch Dock`
- Branch: `main`
- Baseline and current `HEAD`: `529004f32929ea0d6b37e1f7d224964c1995879c`
  (`task2`)
- Locally recorded `origin/main`:
  `11d66e7304cca3875640afa55c80d846424b9d4a`
- `HEAD...origin/main` count: one local commit ahead, zero behind. No network
  fetch was performed, so this describes the locally recorded remote reference.
- TASK-0003 began with a clean working tree. Its working tree changes are
  limited to the nine documentation paths named in this task's completion
  report. Nothing was staged, committed, pushed, or installed.

## Inspected platform

- Arch Linux, rolling release
- Kernel `7.1.8-arch1-3`
- KDE Plasma and KWin `6.7.4`
- Wayland KDE session (`WAYLAND_DISPLAY=wayland-0`)
- Qt base `6.11.2-2`
- KDE Frameworks Core Addons and Kirigami `6.29.0-1`

These versions describe the inspection host only. TASK-0003 did not build,
install, restart PlasmaShell, or run Arch Dock in the live desktop session.

## Documentation authority map

- Architecture and implementation requirements:
  [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md)
- Current implementation status: this file
- Preset contract: [PRESET_SYSTEM_SPEC.md](PRESET_SYSTEM_SPEC.md)
- Target repository structure:
  [TARGET_STRUCTURE_TREE_V2.md](TARGET_STRUCTURE_TREE_V2.md)
- Release gates: [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md)
- Current version-1 theme-package behavior:
  [theme-packages.md](theme-packages.md)
- Native Plasma ownership and recovery safeguards:
  [plasma-lifecycle.md](plasma-lifecycle.md)
- Baseline evidence from TASK-0001 and TASK-0002:
  [audits/BASELINE_AUDIT.md](audits/BASELINE_AUDIT.md)
- [AUTONOMOUS_PROGRESS.md](../AUTONOMOUS_PROGRESS.md) and
  [implementation-audit.md](implementation-audit.md) are preserved historical
  snapshots and are explicitly marked as superseded.

`MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN_V2.md` is only a compatibility
pointer to the canonical architecture document; it is not a second copy or a
second authority.

## Verified current implementation

The following statements were verified by inspecting the current source at the
commit above. They are not claims that TASK-0003 re-exercised the behavior at
runtime.

- `src/main.cpp` creates a Qt Quick/Kirigami application, owns the session-bus
  name `org.archdock.ArchDock`, and delegates panel behavior to `PanelManager`.
- Native edge panels use Plasma containments. Creation records an Arch Dock
  ownership token and panel id, attaches an `org.archdock.dock` visual applet,
  removes matching legacy `org.archdock.control` applets, and checks ownership
  before moving or removing a containment.
- Free panels are **not retired**. The Plasma layout-template route verifies a
  temporary bridge, creates an `org.archdock.dock` applet in the target desktop
  containment, configures it as a free/empty panel, and removes the bridge.
- The registry includes the current version-1 theme-package implementation and
  an embedded catalog of five themes: Obsidian Glass, Neon Segments, Metallic
  Shelf, Holographic Ring, and Minimal Underline.
- CMake declares the application, QML and theme resources, Plasma applets and
  templates, D-Bus and systemd metadata, and nine tests. Their declaration is
  verified; their current build and pass status is not.

## Known defects and incomplete behavior

- Free-panel creation has two paths. `createFreePanelFromTemplate()` creates the
  Plasma desktop-hosted applet, while `createFreePanel()` only creates a registry
  record and opens settings. The paths are not behaviorally equivalent.
- The C++ free-panel removal branch deletes any legacy utility window and the
  registry record, but does not explicitly remove the desktop-hosted Plasma
  applet. TASK-0003 did not test the resulting live-desktop behavior.
- `setPanelVisibilityMode()` stores the requested mode, but
  `shouldConcealPanel()` currently always returns `false`; the planned
  visibility-policy behavior is therefore incomplete.
- `qml/runtime/DockGeometry.js` and
  `plasma-dock-widget/contents/ui/DockGeometry.js` are different copies of dock
  geometry logic and currently have different SHA-256 hashes.
- The installed systemd user unit starts `%h/.local/bin/arch-dock`, while the
  application and D-Bus metadata invoke `arch-dock` from `PATH`. The startup and
  installation strategy is not yet aligned.
- Settings expose panel and icon 3D values, but the shared true-3D scene and
  renderer architecture required by the master plan is not present.
- No fresh configure, build, CTest run, stage install, or live Plasma lifecycle
  run was performed for this snapshot. Bundled or earlier build artifacts are
  not accepted as current validation evidence.

## Planned but not implemented

The master plan and preset specification describe target behavior. Source
inspection found no completed v2 implementation of the following named systems:

- `PanelDefinition`, `PanelRuntimeState`, `PanelScene`, or `IconScene`
- a shared panel/icon renderer used by runtime, settings, and previews
- version-2 panel and icon preset catalogs, including the required 15 panel and
  15 icon presets
- `PreviewSession` audition/rollback semantics
- `ProfileStore` profile persistence and switching
- the complete true-3D, notification, accessibility, performance, packaging,
  migration, and release-validation work required by the architecture plan

The existing version-1 theme-package support and visible settings fields must
not be interpreted as completion of those v2 systems.

## Verification boundary

TASK-0003 is documentation-only. Its acceptance evidence consists of static
source inspection, exact-content comparisons, documentation-link validation,
contradiction searches, filename checks, and Git scope checks. No acceptance
criterion was directly observed in a running Arch Dock or Plasma session, and
no CTest result is claimed by this task.

## Next task boundary

The task pack identifies **TASK-0004 — Fresh configure, build, test, and
stage-install baseline** as the next sequential planning target. TASK-0003's
approval does not authorize TASK-0004 implementation. TASK-0004 must begin with
its own read-only Stage A plan and must receive its own exact implementation
approval before any build directory or report is created.
