# Arch Dock current state

> **Status authority:** This file is the sole current-state source of truth for
> the repository. The architecture requirements are authoritative in
> [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
> Older progress and audit narratives are historical evidence, not current
> implementation claims.

**Evidence snapshot:** 2026-08-22T18:41:40+02:00 (Europe/Amsterdam). Static
implementation statements come from the current checkout. Build and test
statements come from the fresh external TASK-0015 build described below.
Runtime claims come only from its disposable private D-Bus, virtual KWin
Wayland, and private PlasmaShell session; no personal desktop session was
contacted.

TASK-0024 renderer statements below were refreshed on 2026-08-28 from the
current checkout and its external task-specific build. They do not revise the
older lifecycle evidence snapshot or claim personal-session verification.

## Repository state

- Repository root: `/mnt/F/Arch Dock`
- Branch: `main`
- TASK-0015 baseline `HEAD`: `75232e5a62e4d524c35deb7b2f8f9ef02842db45`
  (`task14`).
- The locally recorded `origin/main` is the same commit; `HEAD...origin/main`
  reports zero ahead and zero behind. No network fetch was performed.
- TASK-0015 began with a clean non-ignored working tree. Its bounded changes are
  limited to `tests/PanelRegistryTest.cpp`,
  `tests/tst_BootstrapCoordinator.qml`, `tests/run-plasma-lifecycle.sh`,
  `docs/plasma-lifecycle.md`, and this file. Production C++, production QML,
  CMake, Plasma packages, D-Bus contracts, registry/controller behavior, and
  runtime resources were not changed.
- Codex did not stage, commit, push, globally install, or mutate the personal
  Plasma session.

## Inspected platform

- Arch Linux, rolling release
- Kernel `7.1.8-arch1-3`
- KDE Plasma and KWin `6.7.4`
- Wayland KDE session (`WAYLAND_DISPLAY=wayland-0`)
- Qt base `6.11.2-2`
- KDE Frameworks Core Addons and Kirigami `6.29.0-1`

These versions describe the inspection and verification host. TASK-0015 used a
fresh external build directory and a disposable staged private session; it did not
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
- Shared rendering module, geometry, scene, and fallback contract:
  [shared-renderer.md](shared-renderer.md)
- Native and free Plasma ownership, recovery, and rollback safeguards:
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

The following implementation statements use the evidence snapshots identified
above. Lifecycle statements marked as runtime-verified were exercised in a
disposable private Plasma Wayland session, not inferred from inspection.

- The installed `ArchDock.Rendering` 1.0 module contains the canonical
  `LayoutEngine`, host-neutral `PanelScene`, surface loader, and safe procedural
  2D renderer. Both former `DockGeometry.js` copies have been removed after the
  service-side free-window and live applet callers migrated with compatibility
  profiles. Geometry contract, boundary, deterministic, orientation, frozen
  compatibility, and offscreen visual parity tests cover the shared engine.
- `PanelScene` accepts normalized definition, runtime state, ordered entries,
  host capabilities, theme/icon/motion inputs, and screen/work-area bounds. It
  exposes visual/effect bounds, safe input, reveal and popup/preview anchors,
  renderer status, and entry geometry. Missing or invalid themes and unavailable
  renderer tiers use procedural 2D with a truthful fallback reason. TASK-0025,
  not TASK-0024, owns switching the production applet and Studio preview to the
  scene.
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
- Free panels are **not retired**. Panel Studio and the Plasma layout-template
  route converge on one backend transaction that creates a real
  `org.archdock.dock` desktop applet, verifies its exact panel id/token/type and
  non-bootstrap state, persists and reads back its containment/applet/token and
  screen association, removes any verified temporary bridge, and completes the
  record only after a final host readback. Both routes are runtime-verified and
  do not produce record-only success.
- Free creation rollback covers record allocation, bridge verification, host
  preflight and mutation, host verification, association persistence/readback,
  bridge cleanup, final readback, and completion persistence. A verified removed
  candidate permits record discard; uncertain rollback retains a token-bound
  recoverable record instead of losing ownership evidence.
- Free-host recovery distinguishes zero, one, and multiple exact token matches.
  Zero safely detaches the record; one verifies and rebinds it; multiple matches
  preserve every applet and record a non-mutating conflict. Repeated detached or
  unique synchronization is idempotent. Output disconnect/restore and a real
  private PlasmaShell restart preserve one verified host per record.
- Free removal rediscovers and re-verifies the unique owned applet, removes it,
  verifies absence, and only then removes the record. Missing detached hosts can
  remove their records without a Plasma mutation. Failed adoption, conflict, and
  removal paths preserve unrelated desktop applets; these paths are
  runtime-verified with an explicit unrelated free-host sentinel.
- The registry includes the current version-1 theme-package implementation and
  an embedded catalog of five themes: Obsidian Glass, Neon Segments, Metallic
  Shelf, Holographic Ring, and Minimal Underline.
- CMake declares the application, QML and theme resources, Plasma applets and
  templates, D-Bus and systemd metadata, and nine tests. TASK-0015 configured and
  built the current checkout in
  `/tmp/archdock-task-0015-build.Z8zoR1`, passed its focused C++/QML/template
  tests, passed all nine CTests with zero failures or skips, and passed the staged
  isolated native/free Plasma lifecycle session.

## Known defects and incomplete behavior

- `setPanelVisibilityMode()` stores the requested mode, but
  `shouldConcealPanel()` currently always returns `false`; the planned
  visibility-policy behavior is therefore incomplete.
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

- `IconScene` and the TASK-0025 production applet/Panel Studio bridges to the
  shared `PanelScene`
- version-2 panel and icon preset catalogs, including the required 15 panel and
  15 icon presets
- `PreviewSession` audition/rollback semantics
- `ProfileStore` profile persistence and switching
- the complete true-3D, notification, accessibility, performance, packaging,
  migration, and release-validation work required by the architecture plan

The existing version-1 theme-package support and visible settings fields must
not be interpreted as completion of those v2 systems.

## Verification boundary

TASK-0015 supplies fresh evidence from
`/tmp/archdock-task-0015-build.Z8zoR1`: the focused `panel-registry-test`,
`bootstrap-coordinator-test`, and `plasma-template-contract-test` each passed;
the complete CTest suite passed 9/9 with no skips; and the expanded isolated
Plasma lifecycle script completed successfully under its 300-second outer
ceiling in approximately 204 seconds.

The private runtime directly observed verified Studio and template free-host
creation, no remaining template bridge/control artifact, duplicate-bootstrap
convergence, failed-adoption rollback without an orphan, stale-id one-match
rebind, two-match conflict preservation and convergence, output
disconnect/restore, PlasmaShell restart recovery, zero-match detach, repeated
detached synchronization, verified applet/record removal, and unchanged
unrelated native and free sentinels through final cleanup. The harness printed
`Isolated Plasma native/free lifecycle succeeded.`, exited zero, removed its
temporary root, and left no process discoverable with its private session
environment.

The same run also re-exercised the native lifecycle cases: hide/show identity
stability, visible missing-host recreation, hidden missing-host detach, screen
fallback/restoration, stale-id rebind, repeated-recovery idempotence, conflict
refusal/convergence, PlasmaShell restart recovery, destructive wrong-token and
wrong-renderer refusal, and verified permanent removal. `git diff --check` and
the final build/CTest run remain part of the task handoff.

This evidence is representative of the required Arch Linux, Plasma 6, Qt 6,
Wayland integration, but it remains an isolated virtual session. It does not
claim hardware-specific monitor behavior or mutation of a user's live desktop.

## Next task boundary

TASK-0015 closes the AD-0003 free-panel host lifecycle scope with unit, QML,
template-contract, full CTest, and isolated Plasma evidence. It does not complete
the wider release checklist or claim physical monitor validation.

The task pack identifies **TASK-0016 — Define a normalized native placement
contract** as the next sequential planning target. It begins AD-0004 and may
start only through its own read-only Stage A inspection and exact implementation
approval gate. TASK-0016 has not been started here.
