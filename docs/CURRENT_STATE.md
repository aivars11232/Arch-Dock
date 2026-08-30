# Arch Dock current state

> **Status authority:** This file is the sole current-state source of truth for
> the repository. The architecture requirements are authoritative in
> [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
> Older progress and audit narratives are historical evidence, not current
> implementation claims.

**Evidence snapshot:** 2026-08-30 (Europe/Amsterdam). Static implementation
statements come from the current checkout. TASK-0026 build and test statements
come from a fresh external configure/build and all 35 registered tests. Runtime
claims come only from a disposable private D-Bus, virtual KWin Wayland, and
private PlasmaShell session; no personal desktop session was contacted. Older
lifecycle details below retain their earlier isolated-session evidence.

## Repository state

- Repository root: `/mnt/F/Arch Dock`
- Branch: `main`
- TASK-0026 baseline `HEAD`: `a7f818e91e8d261e4be7b18e55e9ce814f81ec42`
  (`task25`).
- The locally recorded `origin/main` is the same commit; `HEAD...origin/main`
  reports zero ahead and zero behind. No network fetch was performed.
- TASK-0026 changes are intentionally unstaged. They comprise the Theme Package
  v2 contract/parser, v1 compatibility, provenance catalog, non-destructive
  processor, sample classification, capability integration, tests, source-only
  catalog data, and the bounded isolated-runtime harness repair required to
  complete the inherited lifecycle gate.
- Codex did not stage, commit, push, globally install, or mutate the personal
  Plasma session.

## Inspected platform

- Arch Linux, rolling release
- Kernel `7.1.11-arch1-1`
- KDE Plasma and KWin `6.7.4`
- Wayland KDE session (`WAYLAND_DISPLAY=wayland-0`)
- Qt base `6.11.2-3`
- KDE Frameworks Core Addons and Kirigami `6.29.0-1`

These versions describe the inspection and verification host. TASK-0026 used
external build directories and disposable staged private sessions; it did not
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
- Theme Package v2 normative contract:
  [THEME_PACKAGE_V2.md](THEME_PACKAGE_V2.md)
- Source sample provenance and installation boundary:
  [SOURCE_ASSET_CATALOG.md](SOURCE_ASSET_CATALOG.md)
- Version-1 compatibility and installed theme-package behavior:
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
  `LayoutEngine`, host-neutral `PanelScene`, layered `IconScene`, shared
  `RunningIndicator`, `LivePanelPreview`, surface loader, and safe procedural
  2D renderer. Geometry contract, boundary, deterministic, orientation, frozen
  compatibility, preview-state, snapshot, and offscreen visual parity tests
  cover the shared engine.
- `PanelScene` accepts normalized definition, runtime state, ordered entries,
  host capabilities, theme/icon/motion inputs, and screen/work-area bounds. It
  exposes visual/effect bounds, safe input, reveal and popup/preview anchors,
  renderer status, and entry geometry. Missing or invalid themes and unavailable
  renderer tiers use procedural 2D with a truthful fallback reason.
- The production native and free `org.archdock.dock` hosts instantiate one
  `PanelScene` with the existing interactive `DockEntry` as host delegate.
  Launch, context-menu, drag/drop, reorder, edit-mode, and free/native input
  policy remain intact. Separate Canvas and layout branches and the dormant
  service-side `FreePanelWindow` are gone.
- `DockEntry` now uses one shared `IconScene` inside independent magnification
  and motion layers while its outer logical root, pointer region, and drop area
  remain fixed. The former applet-local `IconVisual` and `RunningIndicator`
  implementations are removed.
- Panel Studio renders its active transaction draft and all available built-in
  theme cards through `LivePanelPreview`. Horizontal-native, vertical-native,
  free, open/collapsed, hover, and explicit icon states are supported. Renderer
  tier and fallback reason are shown from `PanelScene`; preview controls have no
  desktop-audition or persistence path, and Apply remains the only transaction.
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
- `ThemeDefinition` and `ThemePackage` implement the version-2 manifest model,
  strict resource and path bounds, typed capabilities/assets/states/layers/
  slices/regions/masks/references, structured diagnostics, and deterministic
  version-1 adaptation. Invalid or incompatible packages fail closed to the
  safe procedural renderer; package data never executes code.
- The source-asset catalog contains exactly 138 independently hashed reviewed
  records: 122 panel screenshots and 16 icon reference sheets. Every record is
  source-only, reference-only, redistribution-unknown, opaque, and blocked from
  installation. Strong 2D, energy, ring/polygon, arc, and icon-reference groups
  are stable catalog classifications, not license or production-readiness
  claims.
- `ThemeAssetProcessor` produces bounded deterministic managed derivatives and
  metadata without overwriting source files. It verifies real pixel alpha,
  prevents source/output aliasing and path escape, validates requested scales,
  records hashes, and preserves manual-cleanup requirements instead of claiming
  automated logo or placeholder removal.
- Theme capabilities now participate in the existing host/theme/renderer/
  platform intersection. Unsupported layouts, rotation, presentation features,
  and renderer tiers remain hidden or fall back with an explicit reason; a flat
  image cannot satisfy true-3D capability.
- The embedded version-1 catalog remains available with five themes: Obsidian
  Glass, Neon Segments, Metallic Shelf, Holographic Ring, and Minimal Underline.
- CMake declares the application, QML and theme resources, Plasma applets and
  templates, D-Bus and systemd metadata, source-only catalog exclusions, and 35
  tests. TASK-0026 passed a fresh external configure and serial build, all 35
  CTests, and the complete staged isolated Plasma native/free lifecycle.

## Known defects and incomplete behavior

- The installed systemd user unit starts `%h/.local/bin/arch-dock`, while the
  application and D-Bus metadata invoke `arch-dock` from `PATH`. The startup and
  installation strategy is not yet aligned.
- Settings expose panel and icon 3D values, but the shared true-3D scene and
  renderer architecture required by the master plan is not present.
- Physical monitor disconnect/reconnect behavior and the personal desktop
  session were not exercised. Output fallback and restoration were verified on
  two virtual KWin Wayland outputs in the disposable lifecycle session.

## Planned but not implemented

The master plan and preset specification describe target behavior. The
following named systems remain outside TASK-0026:

- version-2 panel and icon preset catalogs, including the required 15 panel and
  15 icon presets
- `PreviewSession` audition/rollback semantics
- `ProfileStore` profile persistence and switching
- the complete true-3D, notification, accessibility, performance, packaging,
  migration, and release-validation work required by the architecture plan

The version-2 package/parser/processor and classified reference catalog must not
be interpreted as a production skin family, redistribution approval, or a
true-3D renderer.

## Verification boundary

TASK-0026 supplies fresh evidence from the external build at
`/tmp/archdock-task0026-regression.na337X` (removed during closure): configure
passed, the full serial build passed, and all 35 CTests reported Passed with zero
failures. The new focused coverage includes v2 package parsing and fixtures,
version-1 adaptation, source catalog count/hash/path/install boundaries,
deterministic asset processing, theme-aware capability resolution, registry
round trips, and PanelWindow renderer configuration.

The offscreen visual harness captured safe procedural scenes for horizontal,
vertical, and free layouts. It compared preview and direct `PanelScene`
geometry/bounds/anchors and exact procedural surface pixels from the same
deterministic definitions, including capability fallback.

The isolated lifecycle installed only into a disposable prefix and used its own
D-Bus daemon, virtual KWin, PlasmaShell, XDG roots, outputs, applets, and
fixtures. It completed free-host adoption/recovery/conflict/removal, native
settings rollback, exact visibility modes including dodge and maximized/
fullscreen cover, renderer reconciliation, placement and persistence rollback,
output fallback/restore, host loss, PlasmaShell restart, guarded removal, and
final unrelated-panel checks. The repaired KWin probe emitted both required
markers and observed real buffered Wayland fixture windows. This is runtime
evidence for the staged private environment, not the personal desktop or
hardware-specific behavior.

This evidence is representative of the required Arch Linux, Plasma 6, Qt 6,
Wayland integration, but it remains an isolated virtual session. It does not
claim hardware-specific monitor behavior or mutation of a user's live desktop.

## Next task boundary

TASK-0026 closes AD-0007 Theme Package v2 and sample classification: package
inputs are typed, bounded, provenance-tracked, non-destructively processed, and
capability-aware without promoting reference screenshots to production assets.
It does not implement a production skin family, grant redistribution rights,
or add a true-3D renderer.

The task pack identifies **TASK-0027 — Implement the production
science-fiction chassis skin family** as the next sequential planning target.
It may start only through its own read-only inspection and exact approval gate.
TASK-0027 has not been started here.
