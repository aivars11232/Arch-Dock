# Shared renderer foundation

`ArchDock.Rendering` is the installed, host-neutral QML module introduced by
TASK-0024. It is shared infrastructure for plasmashell, the Arch Dock service,
Panel Studio, and tests. The module does not own Plasma containments, D-Bus
mutation, panel lifecycle, or visibility policy.

The module URI is `ArchDock.Rendering`, version `1.0`. CMake installs it below
the relative `ARCHDOCK_QML_INSTALL_DIR` and tests it from a disposable staged
prefix. Consumers import it normally:

```qml
import ArchDock.Rendering 1.0
```

No source-tree-relative or absolute repository path is part of the module
contract.

## Exported foundation types

- `RenderingModuleProbe` is the harmless module identity/import probe.
- `LayoutEngine` is the canonical geometry engine. `entryGeometry()` returns
  position, tangent angle, outward normal, depth order, scale factor, path
  progress, panel and entry bounds, a safe input region, and nullable 3D fields.
- `PanelScene` is the shared visual contract.
- `PanelSurfaceLoader` selects the safe available surface and reports fallback.
- `PanelProcedural2D` is the initial dependency-free renderer and the fallback
  for missing, invalid, or unavailable theme renderers.

`LayoutEngine.position()` remains as an incremental compatibility wrapper.
The `live` profile preserves the pre-TASK-0024 applet placement, and the
`runtime` profile preserves the former service-side free-window placement.
New shared-scene code uses the `canonical` profile.

## PanelScene inputs

| Property | Contract |
|---|---|
| `panelDefinition` | Normalized schema-v2 panel definition. The current flat backend map and the planned logically sectioned form are both accepted. |
| `runtimeState` | Transient hover, popup, drag, transition, quality, and renderer-fallback state. |
| `orderedEntries` | Ordered entry objects to render. Identity, label, active, and minimized fields are consumed without mutating the model. |
| `hostCapabilities` | Backend-resolved host/capability and renderer-selection result. |
| `themeDefinition` | Resolved theme metadata. Missing, invalid, mismatched, or explicitly unloadable themes fail safely. |
| `iconStyleDefinition` | Resolved icon size, spacing, shape, and optional foreground color overrides. |
| `animationProfiles` | Resolved motion-profile input reserved for shared motion components. TASK-0024 does not add animation behavior. |
| `screenBounds` | Current logical screen rectangle supplied by the host. |
| `availableBounds` | Logical work-area rectangle supplied by the host. |

The scene accepts these inputs as values. It does not query or mutate a Plasma
host and does not call the Arch Dock service.

## PanelScene outputs

| Property or function | Contract |
|---|---|
| `layoutGeometry` | Canonical panel metrics from `LayoutEngine`. |
| `contentBounds` / `visualBounds` | Local visual content rectangle. |
| `effectBounds` | Content rectangle expanded by the procedural stroke, blur, and shadow margin. |
| `inputRegion` | Stable local safe-input rectangle; visual effects do not enlarge it. |
| `revealHandle` | Host-neutral edge, mode, and local rectangle for a future presentation controller. |
| `popupAnchors` | Per-entry outward popup anchors plus a primary anchor. |
| `previewAnchors` | Scene center and the same deterministic per-entry anchor list for previews. |
| `runtimeCapabilityStatus` | Requested, resolved, and effective renderer tiers plus fallback status and reason. |
| `effectiveRendererTier` | Renderer actually instantiated by this foundation; currently `procedural2d`. |
| `fallbackApplied` / `fallbackReason` | Truthful safe-fallback result. |
| `visualPanel` | Instantiated surface item. |
| `iconDelegates` / `entryItemAt(index)` | Rendered entry-delegate access for hosts and tests. |
| `entryGeometryAt(index)` | Full canonical geometry output for an ordered entry. |

## Safe fallback

TASK-0024 intentionally implements only procedural 2D. A non-procedural
renderer request reports `renderer-unavailable` and uses procedural 2D. A named
theme that is absent, mismatched, invalid, failed, unavailable, or explicitly
not loadable reports `theme-unavailable` and uses procedural 2D. A backend
capability fallback or runtime fallback is also reflected in the scene status.

The fallback always retains deterministic geometry, visible entries, bounds,
input-region output, and popup/reveal anchors.

## Integration boundary

TASK-0024 does not replace the production applet body or the Panel Studio mock
preview with `PanelScene`. TASK-0025 owns those bridges, the layered
`IconScene`, and shared live-preview integration. Until then, the live applet
uses `LayoutEngine` directly through its compatibility profile while importing
the same installed module.
