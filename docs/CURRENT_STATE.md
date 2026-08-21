# Arch Dock current state

> **Status authority:** This file is the sole current-state source of truth for
> the repository. The architecture requirements are authoritative in
> [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
> Older progress and audit narratives are historical evidence, not current
> implementation claims.

**Evidence snapshot:** 2026-08-21T23:24:36+02:00 (Europe/Amsterdam). Static
implementation statements come from the current checkout. Build and test
statements come from the fresh TASK-0010 build described below. Runtime claims
come only from its disposable private D-Bus, virtual KWin Wayland, and private
PlasmaShell session; no personal desktop session was contacted.

## Repository state

- Repository root: `/mnt/F/Arch Dock`
- Branch: `main`
- TASK-0010 baseline `HEAD`: `e7a764476359ba87176e2638f249297f3576b158`
  (`Task9`)
- Locally recorded `origin/main`:
  `11d66e7304cca3875640afa55c80d846424b9d4a`
- `HEAD...origin/main` count: seven local commits ahead, zero behind. No network
  fetch was performed, so this describes the locally recorded remote reference.
- TASK-0010 began with a clean non-ignored working tree. Its expected source and
  documentation changes are limited to `tests/PanelRegistryTest.cpp`,
  `src/NativeContainmentLifecycle.cpp`, `src/panel/PanelWindow.cpp`,
  `tests/run-plasma-lifecycle.sh`, `docs/plasma-lifecycle.md`, and this file.
  Codex did not stage, commit, push, globally install, or mutate the live Plasma
  session.

## Inspected platform

- Arch Linux, rolling release
- Kernel `7.1.8-arch1-3`
- KDE Plasma and KWin `6.7.4`
- Wayland KDE session (`WAYLAND_DISPLAY=wayland-0`)
- Qt base `6.11.2-2`
- KDE Frameworks Core Addons and Kirigami `6.29.0-1`

These versions describe the inspection and verification host. TASK-0010 used a
fresh build directory and a disposable staged private session; it did not
globally install, restart the live PlasmaShell, or run Arch Dock against the
personal desktop session.

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

The following implementation statements were verified in the TASK-0010
checkout. Native lifecycle statements marked as runtime-verified were exercised
in the disposable private Plasma Wayland session, not inferred from inspection.

- `src/main.cpp` creates a Qt Quick/Kirigami application, owns the session-bus
  name `org.archdock.ArchDock`, and delegates panel behavior to `PanelManager`.
- Native edge panels use Plasma containments. Creation records an Arch Dock
  ownership token and panel id, attaches an `org.archdock.dock` visual applet,
  removes matching legacy `org.archdock.control` applets, and checks ownership
  before moving or presenting a containment. Missing visible hosts are recreated,
  hidden missing hosts remain detached, token-bound stale ids are rediscovered,
  and repeated recovery converges without duplication. These paths are
  runtime-verified in the isolated lifecycle harness.
- Permanent native removal requires both the exact containment ownership token
  and the expected dock-applet association. The destructive Plasma script
  rechecks both immediately before removal and verifies absence afterward.
  Wrong-token and wrong-renderer runtime cases preserve the containment, applet,
  and full registry record; `removePanel` also preserves the record on refusal.
- Temporary hide/show changes verified Plasma presentation on the existing
  containment and keeps the containment id, dock applet id, and ownership token
  stable. It does not implement visibility by deleting and recreating the host.
- Free panels are **not retired**. The Plasma layout-template route verifies a
  temporary bridge, creates an `org.archdock.dock` applet in the target desktop
  containment, configures it as a free/empty panel, and removes the bridge.
- The registry includes the current version-1 theme-package implementation and
  an embedded catalog of five themes: Obsidian Glass, Neon Segments, Metallic
  Shelf, Holographic Ring, and Minimal Underline.
- CMake declares the application, QML and theme resources, Plasma applets and
  templates, D-Bus and systemd metadata, and nine tests. TASK-0010 configured and
  built the current checkout in `build-codex-task-0010`, passed its focused
  lifecycle test, passed all nine CTests with zero failures or skips, and passed
  the staged isolated Plasma lifecycle session.

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
- Physical monitor disconnect/reconnect behavior and the personal desktop
  session were not exercised. Output fallback and restoration were verified on
  two virtual KWin Wayland outputs in the disposable lifecycle session.

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

TASK-0010 supplies fresh evidence from `build-codex-task-0010`: the focused
`panel-registry-test` passed, the complete CTest suite passed 9/9 with no skips,
and the isolated Plasma lifecycle script completed successfully. That runtime
session directly observed hide/show identity stability, visible missing-host
recreation, hidden missing-host detach, screen fallback/restoration, stale-id
rebind, repeated-recovery idempotence, conflict refusal/convergence,
PlasmaShell restart recovery, destructive wrong-token and wrong-renderer
refusal, verified permanent removal, and an unchanged unrelated containment and
digital-clock applet after every managed phase. `git diff --check` is part of
the final task gate.

This evidence is representative of the required Arch Linux, Plasma 6, Qt 6,
Wayland integration, but it remains an isolated virtual session. It does not
claim hardware-specific monitor behavior or mutation of a user's live desktop.

## Next task boundary

The task pack identifies **TASK-0011 — Persist ownership-verifiable free-host
associations** as the next sequential planning target. It begins the separate
AD-0003 free-panel lifecycle work package; TASK-0010 does not authorize it.
TASK-0011 may begin only through its own read-only Stage A inspection and exact
implementation approval gate.
