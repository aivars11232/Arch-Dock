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

## Optional spatial renderer capability

`ARCHDOCK_ENABLE_QUICK3D` accepts `AUTO` (the default), `OFF`, or `ON`.
`AUTO` packages the optional QML resources when `Qt6Quick3D` is found; `OFF`
does not search for or package them; `ON` requires the development module at
configure time. None of these modes links the core service to Quick 3D.
Explicit build/install lists keep optional files out of disabled builds,
including when an existing build is reconfigured from ON to OFF.

Generated `RendererBuildConfig` metadata records build facts in C++ and QML.
`RendererCapabilityProbe` compiles a fixed application-owned import probe in
the consuming QML engine, without creating a spatial scene. It reads
`GraphicsInfo.api` from that consumer's window. On the supported Linux target,
OpenGL and Vulkan are eligible; the Qt Quick software and null backends are
not. A window without an initialized graphics backend fails closed until its
actual API is known. This capability is never read from saved panel settings.

`PanelScene.true3DCapability`, also exposed as
`runtimeCapabilityStatus.true3d`, contains `buildAvailable`, `importAvailable`,
`backendSupported`, `graphicsApi`, `moduleAvailable`, `sceneBuilt`,
`rendererAvailable`, `reasonCode`, and `importDiagnostic`. Reasons distinguish
`renderer-not-installed`, `renderer-import-loading`,
`renderer-import-unavailable`, `renderer-backend-uninitialized`,
`renderer-backend-unsupported`, and `renderer-scene-unavailable`.

TASK-0035 Phase A established these facts with `sceneBuilt` false. Phase B
implements and packages the actual scene, so enabled builds now report both
build facts true. OFF reports both false. Runtime eligibility and the selected
theme's validated scene support are required before detailed controls appear;
saved settings cannot grant either capability. `surface3D` remains internal.

The software and missing-module tests run in separate processes. The latter
blocks optional import URLs in its engine, preventing the installed system
module or another engine's cache from satisfying the negative case. The
staged import smoke checks generated metadata and probes a real graphics
backend in its disposable KWin Wayland session. Full OFF and ON builds and
CTest suites are the phase gate; successful import alone is not visual proof.

## Base true-3D renderer

`optional3d/PanelScene3D.qml` owns a native Qt Quick 3D `View3D`, perspective
camera, two directional lights, textured/emissive materials, one platform mesh,
and mesh icon bases. `optional3d/IconStyle3D.qml` uses Qt's `ProceduralMesh`
with validated numeric vertices, normals, UVs and triangle indices. The import
probe compiles the same native helper type used by the renderer. Only these
optional files import Quick 3D; the core service does not link to it.

Scene inputs are explicit: `sceneDefinition`, `resources`, `textureSource`,
`entryGeometry`, `quality`, `panelOpacity`, and `sceneConcealed`. Mesh/material
data comes from the bounded ThemePackage parser, not executable package code.
The [Theme v2 scene contract](THEME_PACKAGE_V2.md#5a-optional-3d-scene)
defines resource IDs, camera/light parameters and numeric limits. A package
cannot supply QML, shaders or scripts to this renderer.

The original `mesh-platform-cyan` theme has a beveled octagonal ring with side
walls and an underside: 192 vertices and 96 triangles. A material and UV
texture are applied to real geometry. It is the sixteenth built-in theme and
the eleventh packaged theme. Its asset record identifies original authorship
and the user's redistribution authorization; it does not invent a public
license identifier.

Logical entry rectangles, glyphs, pointer handling, keyboard order and
accessibility stay in the shared 2D delegates. Mesh nodes are not pickable.
Camera-local icon mesh positions project onto the same logical pixel centers;
changing camera orientation or quality does not move those input rectangles.

### Fallback and diagnostics

The selected backend tier is further checked in each consumer's QML engine
and graphics window. If the optional module/backend or validated scene
resources are unavailable, the surface loader follows the theme's declared
`fallbackRendererTiers` in order. Procedural 2D terminates that search. A baked
fallback uses its anchor track, depth ordering and platform input mask; a
skinned fallback uses its declared content bounds. Their geometry matches a
direct selection of the same renderer. Invalid/unavailable themes retain the
procedural safety surface and real entries.

Scene failures distinguish `scene3d-resources-unavailable`,
`scene3d-mesh-unavailable`, `scene3d-texture-unavailable`, `scene3d-load-failed`
and loading state. Missing textures make the scene unavailable without
crashing; restoring valid inputs can select the mesh scene again.

### Quality and editor boundaries

| Quality | Render scale | Maximum target axis | Antialiasing |
| --- | ---: | ---: | --- |
| low | 0.5 | 1024 | off |
| medium | 0.75 | 1536 | 2 samples |
| high | 1.0 | 2048 | 4 samples |

Both target axes are at least one pixel and retain the scene's aspect ratio
under the cap. Texture decoding is capped at 1024 by 1024; texture caching is
disabled for that source. Quality changes reuse the same logical geometry.
`scene3DQuality` is the schema-backed editor field stored in
`surface.parameters3D.quality`; invalid choices normalize to `medium`, and
other persisted members of that parameter map survive.

Studio offers renderer selection only when the backend, theme and actual
preview consumer support 3D. Detailed quality controls additionally require
the active preview to be true 3D. Apply uses the ordinary revisioned settings
transaction. Unchanged formerly available fields may survive a renderer
switch; changing an unavailable field or submitting protected state is still
rejected. Cancel closes the draft, and reopening reloads the saved renderer.

The base renderer is available on the free desktop host only. Native edge
hosts do not gain true 3D. Whole-panel rotation is separately gated by the
selected renderer: the base mesh scene reports `renderer-rotation-unavailable`,
while supported 2D fallbacks keep rotation. True Y-axis icon motion, pedestal
motion, emissive hover and 3D part animation belong to TASK-0036 and are not
claimed here. Item-level input geometry does not claim compositor-wide
nonrectangular click-through.

### Verification boundary

The 2026-09-09 final gates passed 65/65 CTests in each of OFF, AUTO and ON.
OFF disables dependency discovery and omits optional QML resources. Separate
tests block optional imports and use the software backend. Enabled private
Wayland checks render mesh pixels, verify projected icon centers and camera
changes, exercise low/high/low quality and missing resources, and drive Studio
Apply/Cancel/reopen. The staged smoke creates real `org.archdock.dock` applets
and observes rendered frames and bounded quality targets in the actual applet.
These are disposable private D-Bus/KWin/PlasmaShell results, not personal
desktop or physical GPU/monitor acceptance. Exact results are recorded in
[CURRENT_STATE.md](CURRENT_STATE.md).

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
- `PanelBaked25D` is the Theme v2 baked 2.5D renderer. It draws a perspective
  platform as layered artwork and supplies the foreground layers the scene
  interleaves with real icons. It is not a mesh scene and must not be described
  as true 3D.
- `ThemeStateSelection` is the single Theme v2 state and layer selection
  contract: which declared layers are active, in what order, and at what
  opacity. Both `PanelSkin2D` and `PanelBaked25D` read it, so a state, a hover
  and an opening crossfade cannot mean two different things depending on which
  renderer drew the panel.
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

## Geometry hardening, upright icons, and whole-scene rotation

TASK-0033 hardened `LayoutEngine` and added whole-scene rotation for free
radial panels.

- Every numeric input to `metrics()`, `entryGeometry()` and `surface()` is
  coerced through one finite guard. NaN, Infinity, strings, negative sizes,
  zero scale and absurd radii yield a finite, non-empty panel with unit
  normals and progress in `[0, 1]`; the geometry property test exercises every
  layout at counts 0, 1, 7 and 24 against five hostile input sets.
- The open paths (`arc`, `semicircle`, `fan`, `radial`) read one sweep table,
  so the first and last entries sit exactly on the end points of the surface
  drawn under them. The fan keeps its historical angle frame so existing
  tangent and normal values are unchanged.
- `pathOrientation: "upright"` now means exactly that for the canonical and
  live profiles: the decorative tilt the fan, ribbon and floating paths carried
  is applied only by the frozen `runtime` compatibility profile. `tangent`
  follows the path; `radial` faces outward.
- The `diagonal` layout is sized by `(count - 1)` steps plus one icon, so its
  last entry no longer overhangs the panel once several entries are present.
- `SceneRotationController` turns a free radial scene. It yields one angle
  offset from the configured mode (`none`, `clockwise`, `counter-clockwise`),
  speed in degrees per second and trigger (`idle`, `hover`); it runs only when
  the resolver reports whole-panel rotation available, the layout is radial,
  and nothing pauses it: a drag, Plasma Edit Mode, configuration, concealment
  and reduced motion all stop it. A pause holds the angle; switching rotation
  off, losing the capability or enabling reduced motion returns to the
  configured layout angle. The offset is runtime state and is never persisted.
- `PanelScene` adds the offset to the configured layout angle and passes the
  sum to every geometry call, so entries, hover targets, drop targets, popup
  anchors and the drawn surface turn together. While rotation is enabled the
  scene keeps the square envelope every angle fits in
  (`LayoutEngine.rotationEnvelope`) and open paths are centred on it, so the
  host is never asked to resize on every frame and an arc pivots on its own
  circle centre.
- `GeometryHitRegion` is installed as the scene's `containmentMask` for free,
  non-skinned radial scenes. It accepts input on the band the surface draws and
  on the entries themselves, at the same effective angle, so the empty
  interior and corners of a ring or arc pass through. This narrows Qt Quick
  item hit testing only; a Plasma desktop applet is still a rectangle to the
  compositor, and the `nonrectangular-input` host capability is not claimed.
  `activeInputRegionKind` reports `alpha-mask`, `platform-mask`,
  `geometry-band` or `rectangle`. The predicate is annotated
  `contains(point): bool` so Qt actually consults it; an unannotated signature
  is silently ignored and falls back to the plain rectangle.

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
| `entryDelegateContext` | Host-neutral context values mirrored to delegates. `hostKind` (`free` or `native`) selects the geometry hit region for free radial scenes. |
| `rotationAnimationEnabled` | Previews set this false to show the configured angle without turning. |

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
| `entryGeometryAt(index)` | Full canonical geometry output for an ordered entry at the effective angle. |
| `sceneRotationEnabled` / `sceneRotationActive` / `sceneRotationAngle` | Whether whole-scene rotation is configured and permitted, whether it is advancing right now, and the current offset in degrees. |
| `effectiveLayoutAngle` | Configured layout angle plus the rotation offset; the angle every geometry consumer receives. |
| `activeInputRegionKind` / `containsInputPoint(point)` | Which input region is active and the predicate it applies. |
| `activeTrackMetrics` | Baked 2.5D scene geometry, or `null` when the scene is not laid out on a theme track. |
| `occlusionDepth` / `foregroundOcclusionItem` | The declared depth at which foreground layers cut across the entries, and the instantiated layer. |

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
status. True 3D is optional and capability-gated as described above; baked
2.5D is available on the free desktop host and is described below.

The fallback always retains deterministic geometry, visible entries, bounds,
input-region output, and popup/reveal anchors.

## Baked 2.5D

TASK-0034 adds the `baked2.5d` tier. It renders a perspective ring, polygon or
arc platform as layered artwork with real application icons standing on it. It
requires no Qt Quick 3D module, declares no mesh, and is never described as
true 3D; the module validator rejects 3D imports outside the dynamically
loaded `optional3d` directory.

The tier is installed and enabled for the **free desktop host only**. A Plasma
edge panel is a fixed rectangle and cannot present a perspective platform, so a
native panel resolves `renderer-host-unsupported` and falls back through the
package's declared fallback tiers.

Geometry comes from the theme's declared `tracks`, not from the configured
path. `LayoutEngine.trackMetrics()` scales the artwork so the track's own
radius matches the configured layout radius, applies the theme's bounded tilt
to the track and the drawn platform together, and returns a scene box that is
the union of the platform and every scaled icon. `trackEntryGeometry()` returns
the same output contract every other layout returns, plus a normalized `depth`,
the interpolated `scaleFactor`, and `inFront`.

Depth is normalized: `0` at the far edge of the path, `1` at the near edge. An
entry's `z` is its depth, and the theme's `occlusionDepth` becomes the `z` of
the foreground layers the scene instantiates among the entries. An entry nearer
than that depth paints over the platform rim; a further one paints under it.
That ordering is the only reason a real icon can pass behind the artwork. The
logical order the keyboard and accessibility tree walk is the entry order and
is unaffected: only paint order changes.

A closed track (`ellipse`, `polygon`) supports whole-scene rotation and is
measured around its whole path so a turning ring never asks its host to resize.
An open `arc` does not rotate, because sweeping it would carry entries off the
platform drawn beneath them.

Input is the package's own alpha mask, positioned at the platform rectangle,
combined with the entry rectangles; `activeInputRegionKind` reports
`platform-mask`. The empty desktop inside and around a ring passes through,
while an icon standing proud of the rim stays clickable. As with every other
tier this narrows Qt Quick item hit testing only: a Plasma desktop applet is
still a rectangle to the compositor, and `nonrectangular-input` remains
unclaimed.

A missing or undecodable platform layer fails closed to procedural 2D with a
specific reason and leaves every entry rendered; a decorative layer that fails
is skipped and counted instead, because losing a reflection is not worth losing
the dock. The bounded glow modulation runs only when the package declares
`dynamic-glow`, a glow layer is present, reduced motion is off, and the panel
can be seen.

### Resource policy

A perspective platform is drawn far larger than a rail skin, so its layers
carry a raster budget. `PanelSkinLayer2D.rasterBudget` is zero for skins, which
keeps Qt's own behaviour: the asset is decoded at its natural size and every
size it has been drawn at stays in the shared pixmap cache. `PanelBaked25D`
sets a budget of 2048 pixels per axis and `cacheImage: false`, so each layer is
rasterised at the size it is actually drawn, capped, and released when the
layer goes away.

That is what bounds resource use across repeated theme changes: switching
families destroys the previous platform's layer delegates and its textures with
them, instead of retaining one cache entry per size each platform was ever
drawn at. The staged smoke measures it directly — sixteen theme changes across
the three perspective families and the cyan energy skin, with the private
PlasmaShell's resident memory required not to grow.

### Supported hosts and limitations

- The tier is available on the free desktop host only. A native edge panel
  resolves `renderer-host-unsupported` and falls back.
- A baked package declares no end caps, so a collapsed baked panel keeps no
  handle of its own; the presentation controller supplies the bounded minimum.
- `collapse-radial` remains an interface only. It reports the tier it needs
  (`baked2.5d`) and falls back to a centred clip and fade; no shipped
  perspective package declares it, and implementing a real iris is not part of
  TASK-0034.
- Input narrowing is Qt Quick item hit testing only. A Plasma desktop applet
  is still a rectangle to the compositor, so `nonrectangular-input` stays
  unclaimed.
- Tilt is read from the internal `surface.parameters2_5D` map and clamped to
  the theme's declared range. No editor exposes it yet, so no visible control
  claims it.

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
