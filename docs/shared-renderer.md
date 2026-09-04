# Shared renderer foundation

`ArchDock.Rendering` is the installed, host-neutral QML module introduced by
TASK-0024 and integrated into live and preview consumers by TASK-0025. It is
shared infrastructure for plasmashell, the Arch Dock service, Panel Studio,
and tests. The module does not own Plasma containments, D-Bus mutation, panel
lifecycle, settings persistence, or visibility policy.

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
- `IconScene` is the layered icon contract: rear effects, base/tile, glyph,
  front treatment, running indicator, badge, and progress layers consume
  explicit runtime states while retaining a fixed logical input rectangle.
- `RunningIndicator` is the shared orientation-aware indicator with a style
  contract for thickness, length, color, window steps, and pulse behavior.
- `PanelSurfaceLoader` selects the safe available surface and reports fallback.
- `PanelProcedural2D` is the initial dependency-free renderer and the fallback
  for missing, invalid, or unavailable theme renderers.
- `PanelSkin2D` is the Theme v2 skinned renderer. It composes fixed left/right
  caps with a stretched or tiled center, applies declared surface, frame, glow,
  energy, and highlight layers in manifest order, and exposes package content,
  effect, and input bounds.
- `PanelSkinLayer2D` renders one declared layer and applies colorization only to
  assets typed as masks.
- `AlphaHitMask` samples the package input-mask alpha through a bounded cached
  raster and supplies the scene containment predicate.
- `LivePanelPreview` hosts exactly one `PanelScene` with draft or preset data.
  It supports horizontal-native, vertical-native, and free modes; open and
  collapsed presentation; explicit icon states; and truthful renderer status.

`LayoutEngine.position()` remains as an incremental compatibility wrapper.
The `live` profile preserves free-applet placement compatibility. The
`runtime` profile remains for frozen compatibility coverage after the dormant
service-side free window was removed. Native live scenes, previews, and new
shared-scene code use the `canonical` profile.

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
| `entryDelegate` | Optional host delegate. The live applet supplies `DockEntry`; previews use the shared `IconScene`. |
| `entryInteractionEnabled` | Host-owned input gate mirrored to every delegate without changing scene geometry. |
| `geometryCompatibilityProfile` | Explicit canonical or retained compatibility profile. |
| `entryDelegateContext` | Host-neutral context values mirrored to delegates. |

The scene accepts these inputs as values. It does not query or mutate a Plasma
host and does not call the Arch Dock service.

## PanelScene outputs

| Property or function | Contract |
|---|---|
| `layoutGeometry` | Canonical panel metrics from `LayoutEngine`. |
| `contentBounds` / `visualBounds` | Local safe content and complete surface rectangles. A skinned scene offsets entries into the declared content-safe rectangle. |
| `effectBounds` | Visual rectangle expanded by the active procedural or Theme v2 effect margins. |
| `inputRegion` | Stable local safe-input rectangle. For a valid skin, `containsInputPoint()` also applies the package alpha mask and threshold. Visual effects do not enlarge it. |
| `revealHandle` | Host-neutral edge, mode, and local rectangle for a future presentation controller. |
| `popupAnchors` | Per-entry outward popup anchors plus a primary anchor. |
| `previewAnchors` | Scene center and the same deterministic per-entry anchor list for previews. |
| `runtimeCapabilityStatus` | Requested, resolved, and effective renderer tiers plus fallback status and reason. |
| `effectiveRendererTier` | Renderer actually instantiated by the scene: `procedural2d` or `skinned2d`. |
| `fallbackApplied` / `fallbackReason` | Truthful safe-fallback result. |
| `visualPanel` | Instantiated surface item. |
| `iconDelegates` / `entryItemAt(index)` | Rendered entry-delegate access for hosts and tests. |
| `entryGeometryAt(index)` | Full canonical geometry output for an ordered entry. |

## Skinned 2D and safe fallback

TASK-0027 adds the first production `skinned2d` implementation without changing
the fallback contract. `PanelSkin2D` accepts only a validated Theme Package v2
projection whose package ID, horizontal orientation, state slice, asset URLs,
fixed caps, center mode, content rectangle, effect margins, input mask, alpha
threshold, and blend modes are complete. Only `source-over` composition is
claimed by this Qt Quick image path. Unsupported orientation or blend metadata,
an unavailable asset, invalid geometry, or a mismatched package fails closed to
`PanelProcedural2D` with a specific renderer reason.

The fixed caps scale with panel thickness but not panel length. Only the center
part changes length, using the manifest's `stretch` or `tile` mode. The scene
uses the declared content rectangle for icon layout and expands its root for
the complete skin and effect margins, so glow cannot silently consume icon
space. Open, collapsed, and normal slices are selected deterministically from
`runtimeState.presentationState`; TASK-0027 adds no opening interaction or host
geometry animation.

A named theme that is absent, mismatched, invalid, failed, unavailable, or
explicitly not loadable reports `theme-unavailable` and uses procedural 2D. A
backend capability fallback or runtime fallback is also reflected in the scene
status. Baked 2.5D and true 3D remain unavailable.

The fallback always retains deterministic geometry, visible entries, bounds,
input-region output, and popup/reveal anchors.

The alpha mask filters QML item hit testing through `containmentMask`. It does
not claim compositor-wide click-through outside the applet's enclosing window:
Qt documents a `QWindow` mask as a window-manager hint, so Arch Dock does not
advertise the `nonrectangular-input` host capability on that evidence alone.

## Energy renderer cost and pause policy

An energy state has seven active draw items: three surface slices plus frame,
glow, energy-overlay, and highlight masks. An opening or closing crossfade can
temporarily retain both state-specific glow and energy layers, raising the
maximum to nine draw items. Mask colorization uses Qt Quick `MultiEffect` with
blur and shadow disabled; no custom shader, framebuffer blur, or compositor
effect is required. Image caching remains enabled.

The only continuous panel-skin motion is one 3600 ms energy-overlay phase. It
modulates that layer within a two-pixel vertical range and a six-percent
opacity range. The animation runs only when the package declares
`dynamic-glow`, an energy overlay is available, reduced motion is off, and the
skin is visible, enabled, and non-transparent. Hidden, disabled, transparent,
or reduced-motion skins stop the animation and reset its effective phase to
zero. State selection, tint, and static glow remain visible under reduced
motion, preserving hover/open/collapsed feedback without continuous movement.

## Live and preview integration

The production `org.archdock.dock` applet now contains one `PanelScene`. Its
host delegate is the existing interactive `DockEntry`, so launch, context-menu,
drag/drop, reorder, edit-mode, native-service, and free-host input policy stay
host-owned. The applet no longer has separate Canvas or horizontal, vertical,
and free renderer branches. The obsolete service-side `FreePanelWindow` was
removed after its lack of runtime callers was verified.

`DockEntry` renders one shared `IconScene` inside a transformed visual layer.
Its outer logical root and pointer/drop regions remain fixed at the base icon
size, so magnification and motion do not move the hit target. The former
applet-local `IconVisual` and `RunningIndicator` implementations were removed.

`MotionChannels.js` is the single mapping from composed animation channels to
concrete transforms. It reads the entry's own geometry — outward normal, tangent
and the room the theme reserved — so `translate-normal` and `translate-tangent`
mean the same thing in the applet and in a preview. `IconScene` takes the
resolved result as `glyphMotion`, `tileMotion` and `indicatorMotion` and applies
it to those layers only; all three default to an exact identity, so a host that
supplies nothing renders precisely as before. The glyph layer additionally
carries the flat-card Y-axis turn and its derived highlight.

Panel Studio builds a pure renderer candidate from its loaded editor baseline,
local panel/global changes, and backend capability result. The active editor
preview and every available built-in theme card use `LivePanelPreview`; the old
generic theme strip is gone. Package-backed chassis cards use the installed
Theme v2 projection plus catalog-declared deterministic mode, presentation,
entry-state, icon-state, and seed values. Preview controls mutate only embedded
QML state. Persisted settings still change only through the existing
transactional Apply operation; no desktop audition or `PreviewSession`
semantics are introduced.

The staged-install gate runs module import, preview, and deterministic parity
tests against installed files, then starts the staged service/Studio and real
native/free applet hosts in a disposable private KWin/Plasma session. Before
the service starts, an exact `PanelSkin2D` test runs against the staged QML
module on the private Wayland scenegraph. It captures normal, hover, open,
collapsed, animated, and reduced-motion cyan pixels; requires visible tinted
effect pixels to remain inset from the item boundary; proves reduced-motion
frames remain static; and checks center, corner, and out-of-bounds item-level
containment. This is isolated runtime evidence, not personal-desktop or
compositor-wide click-through acceptance.

The live host smoke then selects `energy-frame-green`,
`energy-frame-orange`, `energy-frame-purple`, and finally
`energy-frame-cyan` for both isolated panel records through atomic settings
transactions. For every variant, both renderer configurations must report
`skinned2d`, a ready projection, `dynamic-glow`, the exact package ID, and its
exact staged manifest path; the service and Plasma shell must remain alive and
free of relevant QML import errors. Finishing on cyan restores the deterministic
default used by later checks.

The staged-install gate also requires all three chassis packages and all four
energy packages, and rejects any installed source-sample or `Screenshot_*`
file. The parity harness feeds the same deterministic definitions to preview
and direct `PanelScene` instances. It compares geometry contracts and safe
procedural pixels for the baseline renderer, then compares direct-versus-preview
energy state, layer order, glow, reduced-motion, input containment, and surface
pixels for normal, hover, open, and collapsed states.
