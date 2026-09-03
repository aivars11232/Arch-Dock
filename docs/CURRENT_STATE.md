# Arch Dock current state

> **Status authority:** This file is the sole current-state source of truth for
> the repository. The architecture requirements are authoritative in
> [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
> Older progress and audit narratives are historical evidence, not current
> implementation claims.

**Evidence snapshot:** 2026-09-03 (Europe/Amsterdam). The earlier TASK-0029
implementation described below was committed as `a34fbd9`, which carries the
subject `task28`; the working tree was clean at the start of this session.
The statements here therefore describe `a34fbd9` plus the TASK-0029 Phase A
and Phase B corrections listed under "Verification boundary", which the owner
committed as `ef84d86`.

All figures below come from external Debug build directories created during
this session; none reuse the in-tree `build/` or `build-codex-task-0014`
directories. Runtime claims come only from disposable private D-Bus, virtual
KWin Wayland, and private PlasmaShell sessions; no personal desktop session
was contacted. Older lifecycle details below retain their earlier
isolated-session evidence.

## Repository state

- Repository root: `/mnt/F/Arch Dock`
- Branch: `main`
- Current `HEAD`: `ef84d86d5371bac3459f52fdb9bcb229d0531f91`, subject
  `task29`. The owner committed the TASK-0029 Phase A and Phase B corrections
  from this session as that commit.
- `a34fbd92d4a7dcb76d43538dee7598bf69972245`, subject `task28`, is its
  parent. Despite that subject it contains the bulk of the earlier TASK-0029
  implementation together with the TASK-0028 corrective closure.
- `dfb315c45adc8252ac2fef4cac49b8ba8146e973` (`task28`) is the TASK-0028
  baseline and is an ancestor of `HEAD`.
- TASK-0027 is committed at `c857fd70209646028fc710ef100f49c716384762`
  and is an ancestor of the TASK-0028 baseline.
- The working tree was clean at the start of this session and now contains
  only this file. The TASK-0029 implementation therefore spans two commits,
  `a34fbd9` and `ef84d86`; neither alone is the whole consolidated task.
- Codex did not stage, commit, push, globally install, or mutate the personal
  Plasma session.

## Inspected platform

- Arch Linux, rolling release
- Kernel `7.1.11-arch1-1`
- KDE Plasma and KWin `6.7.4`
- Wayland KDE session (`WAYLAND_DISPLAY=wayland-0`)
- Qt base `6.11.2-3`
- KDE Frameworks Core Addons and Kirigami `6.29.0-1`

These versions describe the inspection and verification host. TASK-0028 and
TASK-0029 used external build directories and disposable staged private
sessions; neither task globally installed, restarted the live PlasmaShell, or
ran Arch Dock against the personal desktop session.

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
- Icon Style Package v1, selection, overrides, and live editor contract:
  [ICON_STYLE_PACKAGE.md](ICON_STYLE_PACKAGE.md)
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
  `RunningIndicator`, `LivePanelPreview`, surface loader, `PanelSkin2D`, alpha
  hit mask, and safe procedural fallback. Geometry contract, boundary,
  deterministic state, orientation, fixed-cap scaling, content-safe placement,
  mask, preview-state, snapshot, and offscreen visual tests cover the shared
  engine.
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
- `IconStyleDefinition`, `IconStylePackage`, and `IconStyleStore` implement the
  non-executable version-1 icon-style contract with bounded metadata, explicit
  states, canonical path containment, catalog/package identity checks, and
  deterministic fail-closed fallback to `plain-original`. Icon-style selection
  is independent of the panel theme.
- Six built-in packages are installed: the `plain-original` fallback plus
  `metallic-blue`, `metallic-red`, `neon-green`, `neon-orange`, and `dark-orb`.
  The five production families are original, asset-free procedural recipes
  with stable IDs, capability declarations, and deterministic preview inputs.
  They frame the real application glyph and do not claim a mapped replacement
  pack or a user-wide icon theme.
- `IconScene` resolves the package projection and explicit icon state, then
  keeps rear/base, glyph, front, indicator, and status roles independently
  addressable. Disabling an entry's tile hides tile/pedestal layers without
  discarding the selected style's safe glyph treatment, state, or indicator.
  The live native/free hosts and Studio previews use this same rendering path.
- `PanelDefinition` persists bounded per-entry overrides keyed by stable
  desktop-entry, application, or canonical free-entry identity. The backend
  atomically resolves override, panel default, and safe fallback; pinned and
  running representations converge on one desktop identity; Reset removes only
  that identity's override; existing legacy custom-icon input remains readable.
- Supported live entries expose **Icon Properties…** through the production
  `DockEntry` context menu. The production editor applies and resets through
  revision-checked `PanelWindow` transactions, preserves hidden future fields,
  reports transaction errors, and discards its draft on Cancel or window close.
  Running-only/transient entries, edit mode, active drag, and disabled input do
  not open the editor.
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
- Three original production chassis packages are installed with stable IDs:
  `sci-fi-chassis-dark`, `sci-fi-chassis-red`, and `sci-fi-chassis-blue`. They
  have separate surface/glow/mask assets, fixed caps and scalable centers,
  content/effect/input bounds, horizontal normal/open/collapsed states, exact
  native/free capability metadata, default icon-style references, and
  deterministic Studio preview inputs. The package-backed catalog entries are
  selectable through the same atomic settings transaction as procedural themes.
- Four original production energy packages are installed with stable IDs:
  `energy-frame-cyan`, `energy-frame-green`, `energy-frame-orange`, and
  `energy-frame-purple`. Their blank-canvas SVG packages separate surface,
  frame, glow, energy-overlay, highlight, and input masks; record exact output
  hashes and review evidence; expose deterministic preview seeds; and contain
  no source-screenshot pixels. The four broad-concept references remain
  non-installable and redistribution-unknown.
- `PanelSkin2D` renders each energy state's manifest layers in deterministic
  order, colorizes only declared masks, bounds tint and glow inputs, supports
  explicit open/closing interpolation without owning the presentation state
  machine, and stops/reset its one overlay phase when hidden or under reduced
  motion. Hover selects a distinct visible glow/overlay state. Missing optional
  layers skip safely; invalid required inputs fall back to procedural 2D.
- Energy input remains bound to the package alpha mask rather than the visual
  glow rectangle. Direct `PanelScene` and `LivePanelPreview` energy scenes share
  state, layer order, glow, reduced-motion, containment, and surface-pixel
  output for normal, hover, open, and collapsed cases.
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
- The embedded catalog contains the five retained procedural themes, three
  package-backed chassis themes, and four package-backed energy themes. The
  later Panel/Icon preset system and its exact 15+15 libraries remain a
  separate contract.
- CMake declares the application, QML, theme, and icon-style resources, all
  seven installed Theme v2 packages, all six installed Icon Style v1 packages,
  Plasma applets and templates, D-Bus and systemd metadata, source-only catalog
  exclusions, and 46 tests.

## Known defects and incomplete behavior

- The installed systemd user unit starts `%h/.local/bin/arch-dock`, while the
  application and D-Bus metadata invoke `arch-dock` from `PATH`. The startup and
  installation strategy is not yet aligned.
- Settings expose panel and icon 3D values, but the shared true-3D scene and
  renderer architecture required by the master plan is not present.
- Per-entry `animationProfileReference` is validated and persisted but remains
  intentionally hidden and has no runtime animation engine until TASK-0030.
- The shipped icon-style families preserve the original application glyph. No
  complete mapped-replacement icon pack or global icon-theme mutation is
  implemented by TASK-0029.
- Physical monitor disconnect/reconnect behavior and the personal desktop
  session were not exercised. Output fallback and restoration were verified on
  two virtual KWin Wayland outputs in the disposable lifecycle session.
- The chassis and energy families are horizontal only. Vertical or other
  layouts fall back to procedural 2D; no vertical package artwork is
  fabricated.
- The alpha mask narrows Qt Quick item containment. It is not evidence of
  compositor-wide click-through outside the applet's enclosing window, so the
  native/free host profiles do not claim `nonrectangular-input` from this work.

## Planned but not implemented

The master plan and preset specification describe target behavior. The
following named systems remain outside TASK-0029:

- version-2 panel and icon preset catalogs, including the required 15 panel and
  15 icon presets
- `PreviewSession` audition/rollback semantics
- `ProfileStore` profile persistence and switching
- the complete true-3D, notification, accessibility, performance, packaging,
  migration, and release-validation work required by the architecture plan

Chassis, energy, and icon-style states are deterministic renderer inputs only.
TASK-0028 adds bounded energy-layer motion and explicit interpolation input;
TASK-0029 adds icon-state styling and a future animation-profile reference.
Neither task adds the final hover/open interaction controller, host geometry
animation, an animation-profile engine, or a true-3D renderer.

## Verification boundary

TASK-0029 Phase A passed its icon-style package/store and panel-registry gate
2/2, then all 46 CTests. Phase B passed its package, asset, scene, visual,
preview, parity, and private rendering smoke gate 7/7, then all 46 CTests.
Phase C passed its model, stable-identity, transaction, registry, dock-model,
panel-window, and QML override-resolution gate 7/7, then all 46 CTests. Phase D
passed its menu/editor, popup/edit/drag guard, state, template, public
interaction, and private runtime gate 6/6, then all 46 CTests. The public
interaction regression uses Qt pointer/key events against the production
`DockEntry` and production
editor, including Apply, Cancel, window-manager close, and Reset; the staged
private host independently verifies installed native/free rendering and
live-scene refresh after backend apply/reset.

The private runtime uses a temporary install prefix, XDG roots, D-Bus daemon,
virtual KWin Wayland compositor, and PlasmaShell. Harness traps own cleanup,
and none of this evidence claims a physical desktop or hardware acceptance
test. A separate final fresh Debug configure/build passed, followed by all 46
CTests including the 65.87-second staged private runtime smoke. The generated
repair and final build directories were removed after verification.

### 2026-09-03 corrective session

Two corrections were applied on top of `a34fbd9` and were committed by the
owner as `ef84d86` (`task29`).

Phase A closed a contract gap in package validation. Renderable layer assets
and mapped-replacement assets were accepted on path, size and digest alone
and were never decode-probed, so an undecodable file could enter the asset
table, the content digest and the runtime projection. `IconStylePackage` now
decode-probes those assets with `QImageReader` before insertion into
`assetPaths`; optional 3D `mesh`/`material` resources remain bounded
path-only. `icon-style-package-test` and `icon-style-asset-test` gained the
`Qt6::Gui` link this requires.

Phase B closed a renderer truthfulness gap. The parser accepted `tinted`,
`monochrome` and `mask`, and the documentation described all three, but the
renderer honoured none of them. `IconStyleResolver` now resolves the effective
treatment behind an explicit compatibility gate and reports a named fallback
reason; `IconScene` renders monochrome through Kirigami's native mask path and
instantiates `MultiEffect` only when a tint or an asset-backed mask is
actually requested. Two defects were fixed with it: the plain fallback left
the `disabled` state completely undimmed, and a style asset that failed at
load time vanished silently while the remaining layers still drew. Any style
asset failure now withdraws the whole treatment in favour of the plain
original glyph.

Phase C and Phase D required no source change and were verified only.

Fresh evidence from this session: baseline full CTest 46/46 before any edit;
Phase A focused 19/19 with a fresh clean configure/build and full 46/46;
Phase B gate full 46/46; Phase C focused 7/7; Phase D focused 5/6. A single
consolidated fresh configure/build then passed with full 46/46, including the
65.55-second staged private runtime smoke.

`rendering-import-smoke` is flaky in this environment and must not be read as
unconditionally green: across this session it passed four times and failed
twice, in two different modes. One failure was a timeout-margin miss against
its `TIMEOUT 120` budget at a typical runtime near 66 seconds. The other was a
`stale-revision` conflict while selecting an icon style on the private native
host. That second failure is a race in the harness, not the product: the
script reads `settingsRevision` and then calls
`applyPanelSettingsTransaction` with it, while applying a setting triggers an
asynchronous `renderer-notification` publish that can advance the revision
between the two calls. The product rejected the stale draft correctly. The
same unguarded read-then-write pattern appears in the script's Icon Properties
section. The cause was not introduced by these corrections and was left
unmodified, since weakening or rewriting the test was out of scope.

The retained TASK-0028 dependency evidence follows.

Corrective closure first reconstructed a clean `dfb315c` source snapshot so the
then-unaccepted TASK-0029 work could not influence TASK-0028 results. Phase A
reparsed every energy JSON file, validated every SVG as XML, matched every
production-record output hash, reproduced the review artifact hash, completed
a fresh serial Debug build, passed the focused provenance/package tests 3/3,
and passed all 38 CTests.

The energy-family contact sheet covers 352x64, 720x96, 1200x160, and 1500x200.
Its freshly reproduced SHA-256 is
`0a9bc4a12cbfe59e858e37c50ae36e65783f34b7ac2ef4ba83c2ab8f49459ea7`.
Visual inspection found distinct cyan, green, orange, and purple packages with
stable caps, open content regions, inset effects, no UI remnants, and no
third-party marks. Automated scenes cover every variant at 100%, 150%, and
200%, unsafe tint fallback, normal/hover/open/collapsed layers, explicit
interpolation, paused hidden overlays, and static reduced motion.

Phase B used a second fresh serial Debug build. Its focused gate passed 11/11
and the complete suite passed 38/38. The isolated renderer smoke installed into
a disposable prefix and used its own D-Bus daemon, virtual KWin, PlasmaShell,
XDG roots, native panel, and free desktop applet. Before starting the service,
the exact Wayland-only pixel case rendered the staged module and proved visible
normal/hover/open/collapsed differences, inset effect pixels, a distinct static
reduced-motion frame, and item-level active-region containment. The private
native/free hosts then resolved the cyan package through normal atomic settings
transactions.

The TASK-0028 Phase C staged smoke requires all four exact installed packages
and rejects source/reference files. It selects green, orange, purple, and
finally cyan on both private hosts, requiring `skinned2d`, ready projection,
`dynamic-glow`, the exact package ID, and exact staged manifest on every
iteration. All private resources are trap-owned and removed by the harness.
Its fresh Phase C and final build/test results remain dependency evidence; they
do not substitute for the TASK-0029 gates above.

This evidence is representative of the required Arch Linux, Plasma 6, Qt 6,
Wayland integration, but it remains an isolated virtual session. It does not
claim hardware-specific monitor behavior or mutation of a user's live desktop.

## Next task boundary

TASK-0028 is the completed AD-0009 dependency. TASK-0029 has passed its four
sequential phase gates and consolidated completion gate, closing the scoped
AD-0010 implementation under the isolated-runtime evidence boundary above.
Its changes remain unstaged and uncommitted for owner review.

The task pack identifies **TASK-0030 — Implement animation profiles and the
shared animation engine** as the next dependency-bound task. Do not begin it
without the separate planning and owner-approval protocol required by that
task.
