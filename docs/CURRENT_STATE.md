# Arch Dock current state

> **Status authority:** This file is the sole current-state source of truth for
> the repository. The architecture requirements are authoritative in
> [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
> Older progress and audit narratives are historical evidence, not current
> implementation claims.

**Evidence snapshot:** 2026-08-21T18:01:21+02:00 (Europe/Amsterdam). Static
implementation statements come from repository and source inspection. Build,
test, and stage-install statements come from the fresh TASK-0004 evidence
identified below; no live Plasma runtime evidence is claimed.

## Repository state

- Repository root: `/mnt/F/Arch Dock`
- Branch: `main`
- Pre-checkpoint `HEAD`: `2fb46c8a65c7c2ce7e5f25296b0812d75a08e058`
  (`Task4`)
- Locally recorded `origin/main`:
  `11d66e7304cca3875640afa55c80d846424b9d4a`
- `HEAD...origin/main` count: three local commits ahead, zero behind. No network
  fetch was performed, so this describes the locally recorded remote reference.
- TASK-0005 began with a clean non-ignored working tree. Its expected working
  tree changes are exactly `docs/BASELINE_CHECKPOINT.md` and this file. Codex
  did not stage, commit, push, globally install, or mutate the live Plasma
  session while creating this checkpoint.

## Inspected platform

- Arch Linux, rolling release
- Kernel `7.1.8-arch1-3`
- KDE Plasma and KWin `6.7.4`
- Wayland KDE session (`WAYLAND_DISPLAY=wayland-0`)
- Qt base `6.11.2-2`
- KDE Frameworks Core Addons and Kirigami `6.29.0-1`

These versions describe the inspection host only. TASK-0004 used a disposable
build and staging root but did not globally install, restart PlasmaShell, or run
Arch Dock in the live desktop session.

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
- Canonical baseline checkpoint and task handoff:
  [BASELINE_CHECKPOINT.md](BASELINE_CHECKPOINT.md)
- Fresh configure, build, test, and stage-install evidence from TASK-0004:
  [audits/BASELINE_BUILD_REPORT.md](audits/BASELINE_BUILD_REPORT.md)
- Baseline evidence from TASK-0001 and TASK-0002:
  [audits/BASELINE_AUDIT.md](audits/BASELINE_AUDIT.md)
- [AUTONOMOUS_PROGRESS.md](../AUTONOMOUS_PROGRESS.md) and
  [implementation-audit.md](implementation-audit.md) are preserved historical
  snapshots and are explicitly marked as superseded.

`MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN_V2.md` is only a compatibility
pointer to the canonical architecture document; it is not a second copy or a
second authority.

## Verified current implementation

The following implementation statements were verified by inspecting the source
at the pre-checkpoint commit above. Unless a bullet explicitly cites build or
test evidence, it is not a claim that the behavior was exercised in a live
Plasma session.

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
  templates, D-Bus and systemd metadata, and nine tests. TASK-0004 completed a
  fresh 68-step build, passed all nine declared CTests with zero failures or
  skips, and produced a 37-file stage-install manifest; see the
  [build report](audits/BASELINE_BUILD_REPORT.md) for commands and limitations.

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
- No live Plasma lifecycle run has been performed for this baseline. The exact
  deferred harness command and the distinction between automated and live
  evidence are recorded in the build report and canonical checkpoint.

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

TASK-0004 supplies the fresh automated configure, build, nine-test CTest, and
stage-install evidence. TASK-0005 is documentation-only and indexes that
evidence in the canonical checkpoint, with documentation-link, source/resource,
and Git-scope checks rerun against the checkpoint working tree. No acceptance
criterion was directly observed in a running Arch Dock or Plasma session.

## Next task boundary

The task pack identifies **TASK-0006 — Define the native panel lifecycle
contract** as the next sequential planning target. TASK-0005's approval does not
authorize TASK-0006 implementation. TASK-0006 may begin only after the user
creates the exact TASK-0005 checkpoint commit, verifies its hash and a clean
working tree, then completes TASK-0006's own read-only Stage A plan and exact
implementation approval gate.
