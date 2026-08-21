> **Architecture source of truth:** This is the sole authoritative Arch Dock
> architecture and implementation roadmap. The versioned compatibility file
> points here and does not contain a second copy.
>
> **Current-state boundary:** Statements in the preserved source plan about the
> then-current branch, working tree, build, or implementation are historical
> planning inputs. [CURRENT_STATE.md](CURRENT_STATE.md) is the sole authority for
> current implementation status and evidence.
>
> **Normative supplements:** See [PRESET_SYSTEM_SPEC.md](PRESET_SYSTEM_SPEC.md)
> for Panel/Icon presets and transactional audition, and
> [TARGET_STRUCTURE_TREE_V2.md](TARGET_STRUCTURE_TREE_V2.md) for the logical
> version-2 source tree. Release gates are tracked in
> [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md).
>
> **Preservation:** Everything beginning with the source-plan heading below is
> retained verbatim from `SOURCE_MASTER_PLAN_V2.md` in the version-2 task pack.

---

# Arch Dock Master Architecture and Implementation Plan — Version 2

**Project:** Arch Dock  
**Plan basis:** `Arch Dock(10).zip`, `Arch Dock Icon⁄panel samples.zip`, verified Arch Dock continuation summaries, and the latest animation and behavior requirements supplied by the user.  
**Plan status:** Architecture and implementation roadmap. No source files are modified by this document.  
**Revision 2:** Adds separate built-in Panel Presets and Icon Presets, exact initial 15+15 catalogs, shared-renderer preset cards, transactional live-desktop audition, active/custom/default actions, and Arch Linux/KDE Plasma 6/Wayland acceptance rules.  
**Codex decomposition:** 86 dependency-ordered tasks across the existing 18 work packages.  
**Canonical project location for later implementation:** `/mnt/F/Arch Dock`  

---

## 1. Purpose of this plan

This document defines:

- the verified current state of Arch Dock;
- the functionality that should be preserved;
- the defects and misleading controls that must be fixed;
- the intended product architecture;
- how panel skins, icon styles, 2D rendering, 2.5D rendering, and true 3D rendering should coexist;
- how icon and panel animations should be represented and executed;
- how native Plasma panels and free/multi-shape panels should differ;
- where each subsystem belongs in the source tree;
- an ordered implementation sequence with completion gates;
- the test and release criteria for a finished Arch Dock.

The plan is deliberately dependency-ordered. Visual features must not be layered on top of broken panel lifecycle, unsynchronized settings, or duplicated rendering logic.

---

# 2. Executive conclusion

Arch Dock already has a meaningful native Plasma foundation. It is not a mock-up and should not be restarted. The existing service, native containment ownership, panel templates, launcher/task model, window tracking, basic visual applet, free-panel experiment, theme import groundwork, and several 2D icon animations should be retained.

The present problem is architectural incompleteness:

1. **The backend, Panel Studio, native Plasma configuration, and live renderer do not share one complete configuration contract.**
2. **Several controls exist without functioning implementations.** The clearest case is the current 3D interface: the controls neither persist correctly nor drive a 3D renderer.
3. **Native panels and free panels have different capabilities, but the interface does not consistently express those differences.**
4. **The sample artwork is not yet a theme system.** It is a collection of references and candidate source images that must be normalized, masked, sliced, described, licensed, and packaged.
5. **Animations are currently encoded as a growing set of conditions inside the icon delegate.** That will not scale to combined icon, panel, 2D, and 3D animation behavior.
6. **The lifecycle layer still contains defects capable of losing or orphaning panels.** Those are higher priority than new rendering work.

The target is one Arch Dock system with:

- a reliable state and lifecycle core;
- two host families: native edge panels and free desktop-hosted panels;
- one shared scene contract;
- four renderer tiers: procedural 2D, skinned 2D, baked 2.5D, and true 3D;
- data-driven panel themes, icon styles, and animation profiles;
- a capability-aware editor that never presents a non-working control;
- safe fallback from 3D to 2.5D or 2D;
- native KDE behavior preserved at all times.

---

# 3. Non-negotiable product rules

These rules govern every implementation phase.

## 3.1 KDE and Plasma ownership

- Arch Dock must coexist with ordinary Plasma panels and widgets.
- Native edge panels remain real Plasma panel containments.
- Arch Dock may modify or remove only containments it can prove it owns.
- Ownership tokens remain mandatory.
- Standard Plasma Edit Mode remains available and functional.
- Experimental free-panel behavior must never mutate unrelated Plasma panels.
- A platform-specific workaround must be isolated behind an adapter rather than spread through the renderer or editor.

## 3.2 Development workflow

- Verify the current source before every task.
- Work on one file at a time and one meaningful change at a time during implementation.
- Build after each meaningful change.
- Run relevant tests after each meaningful change.
- Do not continue through build errors, runtime QML errors, or failed tests.
- Do not reconstruct unknown code from memory.
- Do not perform a large directory reorganization in one operation.
- Introduce the target structure incrementally while preserving a runnable application.

## 3.3 Interface truthfulness

Every visible setting must be in one of three states:

1. **Working:** it is persisted, applied, rendered, and tested.
2. **Explicitly experimental:** it is clearly marked, isolated, and has a safe fallback.
3. **Absent:** it is hidden until the implementation exists.

A control must not appear to work merely because its draft value changes in Panel Studio.

## 3.4 Rendering and accessibility

- 2D must remain a first-class renderer, not a degraded afterthought.
- 3D must be optional and capability-gated.
- Reduced-motion behavior must be respected across icon, panel, preview, and 3D animation.
- Visual transforms must not accidentally move or shrink the logical pointer target unless that behavior is explicitly intended.
- Invalid or unavailable themes must fall back to a safe built-in theme without making the panel unusable.

---


## 3.5 Target operating environment

- Production target is **Arch Linux with KDE Plasma 6 on Wayland**.
- Qt 6, KDE Frameworks 6/Kirigami, Plasma containment/applet APIs, KWin integration and D-Bus are the platform foundation.
- Native panel behavior must be verified in a real Plasma 6 Wayland session; a generic Qt window, X11 session or another desktop environment is not equivalent evidence.
- The implementation must coexist with normal Plasma panels, widgets, Edit Mode, multi-monitor layouts and fractional scaling.
- The Arch packaging route and final runtime dependency list must remain authoritative for release.

## 3.6 Preset-system truthfulness

- Panel themes/skins, icon styles, Panel Presets, Icon Presets and Profiles are distinct resource types.
- The first release installs exactly 15 working built-in Panel Presets and exactly 15 working built-in Icon Presets.
- Built-in presets are immutable originals; customization changes an active draft or creates a user-owned derived preset.
- Every preset card uses the shared renderer and must be loadable; placeholder cards are forbidden.
- Embedded preview and live-desktop audition are separate modes.
- Live-desktop audition is transactional: Apply commits, Save as Custom creates a reusable user resource, Set as Default affects only new panels/icons, and Cancel/Revert restores the exact previous state.
- No preview may mutate an unrelated Plasma panel or leave an orphan temporary host.

# 4. Verified current architecture

## 4.1 Runtime ownership

The current architecture has three principal runtime parts:

### A. `arch-dock` service

The Qt 6 executable owns:

- persistent panel records;
- screen assignments;
- native panel ownership and recovery;
- launcher persistence;
- window and application models;
- KWin action bridging;
- theme import and conversion groundwork;
- Panel Studio and Icon Properties utility windows;
- D-Bus control methods.

The main façade is currently concentrated in:

- `src/panel/PanelWindow.h`
- `src/panel/PanelWindow.cpp`

### B. Native/free Plasma applet renderer

The visible dock renderer is:

- `plasma-dock-widget/contents/ui/main.qml`

Its important components include:

- `DockEntry.qml`
- `IconVisual.qml`
- `RunningIndicator.qml`
- `DockGeometry.js`

Native edge panels host this applet inside a real Plasma panel containment. Current free panels also use the same applet, but as a Plasma desktop widget.

### C. Panel Studio

Panel Studio is primarily implemented in:

- `qml/runtime/SettingsPopup.qml`
- `qml/runtime/StudioForm.qml`
- `qml/runtime/StudioDraft.js`
- `qml/runtime/StudioNavigation.js`

Panel Studio currently manages substantially more configuration than Plasma’s native applet configuration interface.

---

# 5. What is already done and should be preserved

The following work is valuable and should be extended rather than replaced.

## 5.1 Build and service foundation — KEEP

- Qt 6/C++20 project.
- D-Bus service and control object.
- CMake installation of the service, Plasma applets, templates, D-Bus service data, and systemd data.
- Existing test framework using CTest, Qt Test, and QML Test.

## 5.2 Native Plasma containment safety — KEEP AND EXTEND

- Native panel creation.
- Per-panel ownership token.
- Registry association with containment and applet IDs.
- Ownership verification before removal or mutation.
- PlasmaShell restart recovery groundwork.
- Stable screen identity and fallback groundwork.

## 5.3 Panel templates — KEEP

- Empty panel.
- Launcher panel.
- Tasks panel.
- Hybrid panel.
- Free circular panel bootstrap route.

The genuinely empty panel behavior must remain empty.

## 5.4 Application and window models — KEEP AND EXTEND

- Pinned applications.
- Running applications.
- Hybrid entry filtering.
- Launch/activate/minimize/restore/close operations.
- Pin/unpin operations.
- Window IDs and titles in the backend.
- Specific-window activation support.
- Application drag-and-drop groundwork.

## 5.5 Basic visual applet — KEEP AND REFACTOR INTO SHARED RENDERER

- Horizontal and vertical layouts.
- Existing free-layout geometry groundwork.
- Magnification.
- Plates/pedestals/basic appearance modes.
- Reflections.
- Glow groundwork.
- Running indicators.
- Tooltips.
- Current icon animation implementations.
- Reduced-motion helper logic.

## 5.6 Theme import groundwork — KEEP AND UPGRADE

- Versioned theme package concept.
- Managed copy of imported source assets.
- Source analysis.
- Preview generation.
- Optional ImageMagick/Blender detection.
- Existing raster and `.blend` import path.

The current version-1 package is too small for the planned product, but its safety checks and managed-asset concept are useful.

## 5.7 Panel Studio draft model — KEEP AND UNIFY

- Draft-versus-applied settings concept.
- Navigation structure.
- Overview/panel settings separation.
- Apply/cancel behavior foundation.

The draft mechanism should become the single transactional settings path rather than being bypassed by unrelated configuration routes.

---

# 6. What must be fixed before major visual expansion

These are correctness defects, not optional polish.

## 6.1 Repository and build truth

### Current condition

- The branch is 29 commits ahead of `origin/main`.
- Several files are modified but uncommitted.
- `data/themes/builtin-themes.json` is untracked even though CMake references it.
- `Arch dock features.txt` is deleted.
- `AUTONOMOUS_PROGRESS.md` describes an older architecture in which free panels were retired.
- The included `build/` directory was generated from a different absolute path and cannot certify the current source.

### Required correction

- Establish a clean source checkpoint from the present ZIP.
- Preserve all 29 local commits.
- Commit or deliberately discard each current working-tree change; do not leave an ambiguous mixed state.
- Track the built-in theme catalog if it remains part of the design.
- Replace the deleted/stale requirements source with this master plan under `docs/`.
- Delete or ignore the bundled stale build directory and perform a fresh out-of-tree build.
- Update documentation to describe both native edge panels and current free desktop-hosted panels.

### Completion gate

A clean clone of the checkpoint must configure, build, test, stage-install, and launch in the intended Arch/KDE environment.

---

## 6.2 Built-in panel hide/restore lifecycle

### Current defect

A native panel marked invisible can be removed and have its native association cleared. When made visible again, recovery can skip it instead of recreating it.

### Required implementation

- Separate **record visibility** from **host existence**.
- Never use deletion as the normal implementation of temporary visibility.
- Define explicit lifecycle commands:
  - create host;
  - attach renderer;
  - show host;
  - hide host;
  - remove host permanently.
- If a saved visible native panel has no valid host, recovery must recreate it.
- If a saved hidden panel has a valid host, apply its hiding mode rather than deleting it.
- Permanent removal remains ownership-verified and explicit.

### Primary locations

- `src/panel/PanelWindow.cpp`
- `src/NativeContainmentLifecycle.*`
- planned `src/panel/NativePanelController.*`
- tests in `tests/PanelRegistryTest.cpp` and new native lifecycle tests.

---

## 6.3 Free-panel creation and removal

### Current defects

- Panel Studio’s free-panel creation route can create only a registry record, while the Plasma template route creates the actual desktop host.
- Free-panel removal does not reliably remove the Plasma desktop applet host.
- The current registry does not persist a complete, ownership-verifiable free-host association.

### Required implementation

Add a free-host association containing:

- desktop containment ID;
- applet ID;
- ownership token;
- screen ID/index;
- host mode, initially `desktop`;
- optional future host mode, such as `overlay`, only after a separate design decision.

Both creation routes must call one backend transaction:

1. allocate panel record;
2. create desktop applet;
3. write and verify ownership/configuration;
4. store host IDs and token;
5. remove the temporary bootstrap bridge if one was used;
6. roll back both record and host if any step fails.

Removal must:

1. verify ownership;
2. remove the host applet;
3. verify removal;
4. clear association;
5. remove the registry record.

### Primary locations

- `src/panel/PanelWindow.cpp`
- `src/PanelRegistry.*`
- planned `src/panel/FreePanelController.*`
- `plasma-layout-template-free/contents/layout.js`
- new free-host lifecycle tests.

---

## 6.4 Free-panel content semantics and reordering

### Current defects

- Free-panel entries are based primarily on local dropped URLs.
- Launcher/tasks/hybrid selection does not produce the same semantics as native panels.
- Free entry IDs such as `free-url:` are sent into the normal application reorder method, which cannot reorder them.

### Required implementation

Define a unified content model:

- **Empty:** no Arch Dock entries.
- **Launcher:** panel-specific pinned application/URL list.
- **Tasks:** running application list.
- **Hybrid:** panel-specific pinned list plus running-only entries.

Each panel record needs a panel-specific ordered content collection. Global pinning may remain available as an optional profile, but it must not be the only source of launcher content.

Add explicit entry operations:

- `movePanelEntryBefore(panelId, entryId, beforeEntryId)`
- `addPanelEntries(panelId, urls/appIds)`
- `removePanelEntry(panelId, entryId)`
- `setPanelEntryOrder(panelId, orderedIds)`

The backend must choose the correct storage adapter for application entries versus free URL/folder entries.

### Primary locations

- `src/DockModel.*`
- `src/PanelRegistry.*`
- `src/panel/PanelWindow.*`
- `plasma-dock-widget/contents/ui/main.qml`
- `DockEntry.qml`
- current and new free-entry tests.

---

## 6.5 Native panel placement synchronization

### Current defects

Existing native panel synchronization does not fully apply:

- edge;
- alignment;
- offset;
- length;
- thickness;
- dynamic/fixed length;
- floating behavior;
- visibility mode.

Creation also hardcodes alignment and fixed length in several paths.

### Required implementation

Create one native placement adapter. It accepts a normalized placement definition and translates it into supported Plasma containment properties.

Normalized placement fields:

- screen stable ID and fallback index;
- edge;
- alignment: start/center/end;
- offset;
- thickness;
- length mode: fit/fixed/fill;
- minimum/maximum length;
- floating margin where supported;
- visibility mode;
- exclusive/reserved-space behavior where supported.

Every mutation must:

1. verify ownership;
2. apply only supported properties;
3. read back values where possible;
4. update runtime state with the actual applied result;
5. report unsupported fields rather than silently pretending success.

### Primary locations

- `src/PanelPlacement.*`
- `src/panel/PanelWindow.*`
- planned `src/panel/NativePanelController.*`
- planned `src/integration/PlasmaPanelAdapter.*`

---

## 6.6 Visibility behavior

### Current defect

The backend stores visibility modes, and an overlap helper exists, but `PanelWindow::shouldConcealPanel()` currently returns false. The modes presented in Panel Studio therefore do not produce the promised behavior.

### Required implementation

Visibility must be split into two independent concepts:

### Host visibility mode

- always visible;
- auto-hide;
- dodge active window;
- hide under maximized/fullscreen window;
- optional manual hide.

### Panel presentation state

- open;
- opening;
- collapsed;
- collapsing;
- concealed;
- revealing.

A panel can therefore be **host-visible but visually collapsed**, which is required for the user’s “visible but closed, then opens on hover” behavior.

The visibility controller must consume:

- panel geometry;
- screen;
- active/minimized/maximized/fullscreen window state;
- pointer/reveal-zone state;
- user interaction locks;
- popup state;
- edit mode;
- configured delays.

### Primary locations

- `src/PanelVisibility.*`
- planned `src/panel/PanelVisibilityController.*`
- `src/WindowWatcher.*`
- shared QML `PanelPresentationController.qml`
- native/free host adapters.

---

## 6.7 Remove duplicated geometry logic

### Current defect

There are two diverging `DockGeometry.js` copies. One contains free-surface geometry behavior that the other does not, and their data shapes differ.

### Required implementation

Create one canonical geometry engine with a stable output contract:

- icon position;
- tangent angle;
- outward normal;
- depth order;
- scale factor;
- path progress;
- panel bounds;
- safe input region;
- optional 3D position/orientation.

Both live rendering and Panel Studio preview must use the same engine.

### Primary locations

- current `qml/runtime/DockGeometry.js`
- current `plasma-dock-widget/contents/ui/DockGeometry.js`
- planned shared `ArchDock.Rendering/LayoutEngine.js`
- `tests/tst_DockGeometry.qml`

---

## 6.8 Disconnect or hide non-working 3D controls until the 3D contract exists

### Current defects

The current 3D controls:

- are excluded from the editable/apply allowlist;
- are not normalized or persisted in the registry;
- are not returned in the live dock configuration;
- have no renderer;
- remain visible while disabled instead of disappearing;
- have no renderer capability check.

### Required interim behavior

Before the true 3D phase:

- either hide the entire 3D section behind an experimental feature flag;
- or keep only a non-interactive “3D renderer not installed” status page.

When 3D is implemented:

- the main 3D toggle is shown only when the renderer is available;
- detailed 3D controls are hidden when 3D is off;
- unsupported controls are hidden based on the selected theme’s capability declaration;
- values are persisted, validated, rendered, and tested.

---

# 7. Findings from the icon and panel sample archive

The archive contains:

- **122 panel images**;
- **16 icon-style reference sheets**.

All 138 PNG files contain an alpha channel in their file mode, but their pixels are fully opaque. None is ready to function as a transparent panel or icon asset without processing.

## 7.1 Sample classes

### Class A — desktop and product references

**Panel samples 1–9** show complete desktops, commercial themes, launchers, and interface references. They are useful for style, composition, color, material, and behavior analysis. They are not direct panel skins.

**Implementation use:** reference only.

### Class B — strong horizontal 2D skin candidates

**Panel samples 10–17** are long science-fiction chassis/frame designs. Several have a usable empty center and can become direct panel skins after:

- removing background;
- removing embedded placeholder symbols where present;
- defining fixed end caps and stretchable center slices;
- defining icon-safe content regions;
- producing horizontal and vertical orientation variants;
- producing open and collapsed variants where relevant.

**Implementation use:** first 2D skin family.

### Class C — legacy dock/shelf references and mixed candidates

**Panel samples 18–44** include reflective shelves, pedestals, flame effects, old dock products, and narrow visual strips. Some small strips may be extractable; many are screenshots containing existing icons.

**Implementation use:** primarily renderer recipes and visual references. Individual assets can be promoted only after manual inspection, cleanup, and provenance confirmation.

### Class D — energy/glow frame candidates

**Panel samples 45–72** contain blue, cyan, green, orange, red, purple, smoky, holographic, and energy-frame designs with mostly empty centers.

These are suitable for:

- layered 2D themes;
- hover-glow states;
- reveal/open effects;
- color variants;
- animated texture or emissive masks;
- horizontal split/open mechanisms.

They still require background removal and alpha masks. Many should be decomposed into:

- base rail;
- left cap;
- right cap;
- glow layer;
- smoke/energy overlay;
- optional highlight mask.

**Implementation use:** second 2D skin family and first animated-skin family.

### Class E — circular launcher references

**Panel samples 73–80** show circular launcher products/screenshots.

**Implementation use:** geometry and interaction references, not direct assets.

### Class F — full ring and polygon platforms

**Panel samples 81–110** show 3D rings, octagonal platforms, circular pedestals, and separated icon bases.

They can be used in two ways:

1. **Baked 2.5D theme:** remove the background and placeholder icons, retain a perspective-rendered platform, and position real application icons on declared anchor points.
2. **True 3D theme:** model an original mesh/material scene based on the design language; a flat PNG is not itself a true 3D model.

**Implementation use:** first 2.5D free-panel family and later true-3D reference family.

### Class G — arcs, semicircles, and corner/freeform platforms

**Panel samples 111–122** show open arcs and semicircular platform structures.

They are appropriate for:

- free/multi-shape panels;
- corner docks;
- semicircle layouts;
- whole-panel clockwise rotation;
- fold, fan, or radial open/close behavior;
- baked 2.5D and later true 3D.

**Implementation use:** second 2.5D/freeform family and later 3D family.

### Class H — icon-style reference sheets

**Icon samples 1–16** are screenshots or collages of blue, red, green, orange, metallic, orb, pedestal, and Alienware-style icon packs. They are not a complete set of isolated application icon files ready for packaging.

They should define original Arch Dock icon-style families:

- metallic blue;
- metallic red;
- neon green;
- neon orange;
- dark orb;
- pedestal/orb hybrid;
- beveled science-fiction tile.

The safe first implementation is to preserve each application’s real icon glyph and apply an Arch Dock base/tile/frame around it. Complete icon replacement can be supported later through an explicit application-ID-to-asset mapping.

---

# 8. Asset intake and theme-production pipeline

No sample image should be copied straight into the installed theme directory. Every accepted sample goes through an asset-production pipeline.

## 8.1 Intake record

For each source image, record:

- source filename;
- content hash;
- source class;
- intended use;
- provenance/licensing status;
- target renderer tier;
- supported orientations;
- whether embedded icons or text must be removed;
- whether a closed/open state pair exists;
- whether it is production-ready, reference-only, or rejected.

Only assets the user owns or has permission to redistribute should be included in a public package.

## 8.2 Image preparation

For direct 2D and 2.5D candidates:

1. crop external whitespace/background;
2. remove solid or photographic background;
3. create a clean alpha mask;
4. remove placeholder icons, labels, logos, and browser/interface remnants;
5. repair the surface below removed material;
6. separate base, frame, glow, reflection, and overlay layers where useful;
7. normalize color-neutral masks when tinting is intended;
8. create a preview;
9. test at multiple panel lengths and display scales.

## 8.3 Skin geometry metadata

Each panel skin must declare:

- natural source size;
- fixed left/top cap;
- fixed right/bottom cap;
- stretchable or tileable center region;
- safe content rectangle or path;
- icon baseline/anchor;
- visual overflow for glow/shadow;
- input mask;
- open-state bounds;
- collapsed-state bounds;
- orientation support;
- minimum and maximum sensible size;
- optional panel parts used by open/close animation.

## 8.4 Icon-style metadata

Each icon style must declare:

- glyph policy: original, tinted, monochrome, or mapped replacement;
- tile/base asset;
- mask;
- border;
- reflection;
- shadow;
- glow/emissive mask;
- hover state;
- active state;
- running state;
- urgent state;
- minimized state;
- optional 3D mesh/material;
- safe glyph inset;
- supported animation capabilities.

## 8.5 Production asset directories

Planned structure:

```text
assets/
  source-samples/                 # Not installed; provenance and processing input
  themes/
    sci-fi-chassis-blue/
    sci-fi-chassis-red/
    energy-frame-cyan/
    energy-frame-green/
    energy-frame-orange/
    ring-platform-blue/
    arc-platform-orange/
  icon-styles/
    metallic-blue/
    metallic-red/
    neon-green/
    neon-orange/
    dark-orb/
  common/
    masks/
    shaders/
    materials/
```

Source screenshots should not be installed with the product unless they are deliberately licensed and necessary.

---

# 9. Target logical architecture

This is the dependency-aware logical target for the 86-task plan. It is not authorization for a one-step repository rewrite. Codex introduces each branch only in the task that owns it and keeps the application buildable after every meaningful change.

The complete version-2 tree is installed as `docs/TARGET_STRUCTURE_TREE_V2.md`. The principal integrated target is:

```text
Arch Dock/
├── CMakeLists.txt
├── PKGBUILD                              # Added in packaging task
├── docs/
│   ├── MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN_V2.md
│   ├── CURRENT_STATE.md
│   ├── TARGET_STRUCTURE_TREE_V2.md
│   ├── PRESET_SYSTEM_SPEC.md
│   ├── THEME_PACKAGE_V2.md
│   ├── ICON_STYLE_PACKAGE.md
│   ├── ANIMATION_PROFILE.md
│   ├── PLASMA_LIFECYCLE.md
│   ├── ARCH_KDE_RUNTIME_MATRIX.md
│   └── RELEASE_CHECKLIST.md
│
├── src/
│   ├── main.cpp
│   │
│   ├── model/
│   │   ├── PanelDefinition.*
│   │   ├── PanelRuntimeState.*
│   │   ├── ThemeDefinition.*
│   │   ├── IconStyleDefinition.*
│   │   ├── AnimationProfile.*
│   │   ├── PanelPresetDefinition.*
│   │   ├── IconPresetDefinition.*
│   │   ├── PresetOrigin.*
│   │   └── SettingsMigration.*
│   │
│   ├── panel/
│   │   ├── PanelWindow.*                # Stable D-Bus/UI façade
│   │   ├── PanelManager.*
│   │   ├── NativePanelController.*
│   │   ├── FreePanelController.*
│   │   ├── PanelVisibilityController.*
│   │   └── PanelPresentationPolicy.*
│   │
│   ├── integration/
│   │   ├── PlasmaPanelAdapter.*
│   │   ├── PlasmaDesktopHostAdapter.*
│   │   ├── KWinActionBridge.*
│   │   ├── ScreenIdentity.*
│   │   └── SystemStatus.*
│   │
│   ├── content/
│   │   ├── DockModel.*
│   │   ├── PanelContentModel.*
│   │   ├── WindowModel.*
│   │   ├── WindowWatcher.*
│   │   └── WindowPreviewModel.*
│   │
│   ├── themes/
│   │   ├── ThemeCatalog.*
│   │   ├── ThemePackage.*
│   │   ├── ThemeAssetProcessor.*
│   │   ├── ThemeRenderCache.*
│   │   └── ThemeCapabilityResolver.*
│   │
│   ├── presets/
│   │   ├── PanelPresetCatalog.*
│   │   ├── IconPresetCatalog.*
│   │   ├── BuiltInPresetLoader.*
│   │   ├── UserPresetStore.*
│   │   ├── PresetPackage.*
│   │   ├── PresetCapabilityResolver.*
│   │   ├── PresetApplicationService.*
│   │   ├── PresetPreviewSession.*
│   │   ├── PresetPreviewRecovery.*
│   │   └── PresetDefaultStore.*
│   │
│   └── persistence/
│       ├── PanelRegistry.*
│       ├── DockSettings.*
│       └── ProfileStore.*
│
├── qml/
│   ├── ArchDock/Rendering/
│   │   ├── qmldir
│   │   ├── PanelScene.qml
│   │   ├── PanelSurfaceLoader.qml
│   │   ├── PanelPresentationController.qml
│   │   ├── IconScene.qml
│   │   ├── IconMotionController.qml
│   │   ├── PanelMotionController.qml
│   │   ├── RunningIndicator.qml
│   │   ├── LayoutEngine.js
│   │   ├── renderers/
│   │   │   ├── PanelProcedural2D.qml
│   │   │   ├── PanelSkin2D.qml
│   │   │   ├── PanelBaked25D.qml
│   │   │   ├── PanelScene3D.qml
│   │   │   ├── IconStyle2D.qml
│   │   │   └── IconStyle3D.qml
│   │   ├── effects/
│   │   ├── previews/
│   │   │   ├── LivePanelPreview.qml
│   │   │   ├── PresetCardPreview.qml
│   │   │   └── WindowPreviewPopup.qml
│   │   └── inputs/
│   │
│   └── studio/
│       ├── SettingsPopup.qml
│       ├── StudioForm.qml
│       ├── ThemeBrowser.qml
│       ├── IconStyleBrowser.qml
│       ├── AnimationProfileEditor.qml
│       ├── LivePanelPreview.qml
│       └── presets/
│           ├── PanelPresetBrowser.qml
│           ├── IconPresetBrowser.qml
│           ├── PresetCard.qml
│           ├── PresetDetails.qml
│           ├── PresetActionBar.qml
│           ├── DesktopAuditionStatus.qml
│           └── UserPresetSaveDialog.qml
│
├── plasma-dock-widget/
│   └── contents/
│       ├── config/
│       └── ui/
│           ├── main.qml                 # Plasma host/data bridge
│           ├── DockEntry.qml            # Entry interaction adapter
│           └── native configuration pages
│
├── data/
│   ├── presets/
│   │   ├── panels/                      # Exactly 15 valid built-ins at first release
│   │   └── icons/                       # Exactly 15 valid built-ins at first release
│   ├── themes/
│   ├── icon-styles/
│   ├── dbus/
│   └── systemd/
│
├── assets/
│   ├── source-samples/                  # Not installed by default
│   ├── themes/
│   ├── icon-styles/
│   └── common/
│       ├── masks/
│       ├── shaders/
│       └── materials/
│
└── tests/
    ├── unit/
    │   ├── panel lifecycle
    │   ├── schema/migration
    │   ├── theme/icon style
    │   ├── preset definitions/catalogs
    │   └── preview session/rollback
    ├── qml/
    │   ├── geometry/rendering
    │   ├── preset browsers/cards
    │   └── animation/presentation
    ├── integration/
    │   ├── native/free host lifecycle
    │   ├── profile transactions
    │   └── preset apply/audition/recovery
    ├── visual/
    │   ├── renderer tiers
    │   └── preset preview parity
    └── plasma-session/
        ├── Arch Linux KDE Plasma 6 Wayland matrix
        ├── preset audition matrix
        └── install/upgrade/uninstall matrix
```

## 9.1 Important boundary

`PanelWindow` should remain the stable D-Bus façade during migration. New logic should be delegated out of it one subsystem at a time. This avoids a disruptive rewrite and preserves external calls from the Plasma applet and Panel Studio.

## 9.2 Shared renderer deployment

The renderer must be packaged as a QML module that is discoverable by:

- the Plasma applet running inside `plasmashell`;
- the Arch Dock Panel Studio process;
- QML unit tests;
- the visual preview harness.

A build/install test must verify this import in a disposable Plasma session. The current duplicated QML approach must not continue.

---

# 10. Core data model

## 10.1 Versioned panel definition

The current registry uses many flat keys. A big-bang persistence rewrite is unnecessary and risky. Introduce a typed versioned definition while preserving migration from existing records.

Logical version-2 structure:

```text
PanelDefinition
├── schemaVersion
├── identity
│   ├── id
│   ├── name
│   └── builtIn
├── host
│   ├── kind: native-edge | free-desktop
│   ├── containmentId
│   ├── appletId
│   ├── ownershipToken
│   └── screen identity
├── content
│   ├── type: empty | launcher | tasks | hybrid
│   ├── ordered entries
│   └── KDE widgets
├── placement
│   ├── edge
│   ├── alignment
│   ├── offset
│   ├── x/y
│   ├── width/height
│   ├── dynamic/fixed/fill
│   └── floating margin
├── visibility
│   ├── host mode
│   ├── reveal zone
│   ├── open/close delays
│   └── window-overlap policy
├── presentation
│   ├── open/collapsed mode
│   ├── collapse axis
│   ├── collapse mechanism
│   ├── reveal handle
│   └── current runtime state
├── layout
│   ├── path type
│   ├── scale/angle/radius
│   ├── rows/padding
│   ├── polygon sides
│   ├── orientation
│   └── anchor
├── surface
│   ├── renderer tier
│   ├── theme reference
│   ├── color/opacity
│   ├── border/glow/shadow/blur
│   └── 2D/2.5D/3D parameters
├── iconStyle
│   ├── style reference
│   ├── global defaults
│   └── per-entry overrides
└── motion
    ├── icon profile
    ├── panel profile
    ├── reveal profile
    └── reduced-motion policy
```

## 10.2 Runtime state must not be persisted as configuration

Transient state belongs in `PanelRuntimeState`, not in saved settings:

- hover status;
- edit mode;
- popup open;
- drag in progress;
- current open/collapsed transition;
- window overlap;
- current renderer fallback;
- current frame/performance quality;
- current screen geometry.

Persisting transient state causes recovery errors and stale behavior after crashes.

## 10.3 Capability model

Every host and theme declares capabilities.

### Host capability examples

- supports native auto-hide;
- supports arbitrary XY placement;
- supports arbitrary rotation;
- supports non-rectangular input mask;
- supports ring layout;
- supports containment thickness changes;
- supports always-on-top behavior;
- supports third-party Plasma widgets.

### Theme capability examples

- supports 2D;
- supports 2.5D;
- supports true 3D;
- supports horizontal/vertical/radial collapse;
- supports split parts;
- supports dynamic tint;
- supports panel rotation;
- supports icon tile states;
- supports specific layouts/orientations.

Panel Studio should generate its visible controls from the intersection of host and theme capabilities.

---


## 10.4 Panel and icon preset definitions

The preset system adds typed resources without replacing `PanelDefinition`, themes, icon styles or profiles.

- `PanelPresetDefinition` describes one reusable panel setup: host/content defaults, placement, layout, theme, visibility, presentation, motion and an optional recommended Icon Preset.
- `IconPresetDefinition` describes one reusable icon setup: icon-style reference, visual/state overrides, glyph policy and motion profile.
- Built-ins are read-only installed packages.
- User presets contain a complete normalized snapshot plus `derivedFromPresetId` and source revision metadata.
- Active panels remain complete `PanelDefinition` records and may carry optional preset lineage; they do not depend on catalog availability to load.
- Profiles remain complete multi-panel arrangements and are not aliases for presets.

The normative field contract, catalog count, built-in IDs and preview transaction are defined in `docs/PRESET_SYSTEM_SPEC.md`.

# 11. Renderer architecture

## 11.1 One scene contract

`PanelScene.qml` is the single visual contract. It receives:

- normalized panel definition;
- runtime state;
- ordered entries;
- host capabilities;
- theme definition;
- icon style definition;
- animation profiles;
- screen and available bounds.

It outputs:

- visual panel;
- icon delegates;
- content and effect bounds;
- input region;
- reveal handle;
- preview/popup anchors;
- runtime capability/fallback status.

Both the live dock and Panel Studio preview instantiate this same scene.

## 11.2 Four renderer tiers

### Tier 1 — Procedural 2D

For simple glass, neon, metallic, minimal, pill, rectangle, plate, and pedestal styles.

Advantages:

- fastest;
- color-customizable;
- scalable;
- minimal asset dependency;
- safe fallback.

This tier should replace the current loose collection of appearance conditionals with a defined renderer interface.

### Tier 2 — Skinned 2D

For panel samples such as 10–17 and 45–72.

Features:

- layered PNG/SVG assets;
- fixed caps and scalable center;
- separate glow/overlay masks;
- orientation variants;
- open and collapsed state assets;
- animated tint, opacity, glow, or texture offset;
- content-safe region metadata.

### Tier 3 — Baked 2.5D

For ring, polygon, and arc samples that look three-dimensional but are rendered as perspective artwork.

Features:

- perspective platform image;
- real icons positioned over declared track anchors;
- front/back depth ordering;
- icon scale based on path depth;
- optional foreground occlusion layer;
- baked shadow/reflection;
- limited visual tilt/rotation;
- lower cost than a true 3D scene.

### Tier 4 — True 3D

For original mesh/material scenes.

Features:

- mesh-based platform and icon bases;
- camera and perspective;
- physical Y-axis icon rotation;
- real depth ordering;
- emissive glow and lighting;
- 3D panel part opening/closing;
- true ring/arc/platform rotation;
- renderer-quality levels and 2.5D fallback.

A PNG reference cannot be declared true 3D merely because it looks 3D. True 3D requires a mesh/scene or an explicitly limited generated extrusion.

## 11.3 Renderer fallback

Fallback order:

```text
true 3D → baked 2.5D → skinned 2D → procedural 2D safe theme
```

Fallback occurs when:

- the required module is unavailable;
- a mesh or texture fails to load;
- the theme does not support the selected host;
- the requested quality exceeds configured capability;
- a safe-mode startup is requested.

The user should be told which fallback is active, but the panel must remain usable.

---

# 12. Theme package version 2

The current version-1 package describes essentially one surface asset and a fit mode. Version 2 must describe a complete panel presentation.

## 12.1 Required conceptual fields

```json
{
  "format": "org.archdock.theme",
  "version": 2,
  "id": "energy-frame-cyan",
  "name": "Energy Frame Cyan",
  "capabilities": {
    "hosts": ["native-edge", "free-desktop"],
    "renderers": ["2d-skin"],
    "orientations": ["horizontal", "vertical"],
    "layouts": ["adaptive", "horizontal", "vertical"],
    "presentation": ["open", "collapsed", "slide-horizontal"],
    "dynamicTint": true
  },
  "surface": {
    "layers": {},
    "slices": {},
    "contentRegion": {},
    "effectMargins": {},
    "inputMask": {},
    "states": {
      "open": {},
      "collapsed": {},
      "hover": {}
    }
  },
  "iconStyle": "metallic-blue",
  "animationProfiles": {
    "icon": "slow-y-turn",
    "panel": "hover-energy-glow",
    "presentation": "horizontal-shutter"
  }
}
```

The exact serialized schema should be finalized in its own specification and tested before mass-producing theme packages.

## 12.2 Compatibility

- Continue reading version-1 packages.
- Wrap a version-1 asset as a simple Tier-2 theme.
- Never overwrite the original imported source.
- Migrate metadata into a managed version-2 package only after successful validation.

---

# 13. Icon-style system

Panel themes and icon styles are related but independent.

## 13.1 Why they must be separate

A user may want:

- one panel skin with several icon styles;
- the same icon style on multiple panel themes;
- a plain panel with 3D icon pedestals;
- an energy panel with original unframed application icons;
- per-icon overrides.

## 13.2 Icon visual layers

Each icon entry should be composed from stable layers:

```text
logical interaction area
└── visual motion layer
    ├── rear glow/shadow
    ├── base/pedestal/tile
    ├── application glyph
    ├── front frame/highlight
    ├── running/active/urgent indicator
    └── badge/progress overlay
```

Animations can target one layer without forcing the entire icon to move. For example, the glyph can rotate on Y while the pedestal remains stationary.

## 13.3 Glyph policies

- **Original:** preserve the normal application icon. Default and safest.
- **Styled original:** apply tint, contrast, monochrome, mask, or bevel effect.
- **Mapped replacement:** use an application-ID-specific asset from a complete icon pack.
- **Custom per icon:** user-selected file stored through Icon Properties.

## 13.4 Per-state styling

Support:

- normal;
- hover;
- pressed;
- active window;
- running but inactive;
- minimized;
- urgent/attention;
- launching;
- drop target;
- disabled/edit mode.

## 13.5 Connect Icon Properties

The current Icon Properties backend should become reachable from the live icon context menu. It should eventually manage:

- custom glyph;
- custom label;
- tile enabled/disabled;
- tile/style override;
- animation profile override;
- launch command/desktop entry where safe;
- reset to panel default.

---


# 13A. Built-in preset catalog and live desktop audition

## 13A.1 Separate libraries

Panel Studio must expose:

```text
Panels
├── Built-in Panel Presets
├── My Panel Presets
├── Panel Themes / Skins
└── Import

Icons
├── Built-in Icon Presets
├── My Icon Presets
├── Icon Styles
└── Import
```

The initial release contains exactly 15 functional built-in Panel Presets and exactly 15 functional built-in Icon Presets. A preset card renders through the same `PanelScene`/`IconScene` used on the desktop; it must not be a static screenshot pretending to be a live preview.

## 13A.2 Built-in customization

A built-in preset is immutable. Selecting it creates a normalized draft. The user may:

- apply the draft to the selected active panel;
- save the customized draft as a new user-owned preset;
- set it as the default for future panels or icons;
- cancel and restore the previous configuration;
- restore the built-in defaults without modifying the installed package.

## 13A.3 Live desktop audition

Embedded preview does not mutate the desktop. `Preview on Desktop` starts one `PresetPreviewSession`:

- compatible existing panel: snapshot, apply ephemeral override, update with draft changes, then commit or restore;
- new/incompatible panel: create a temporary preview-owned host, then convert it atomically on Apply or remove it completely on Cancel;
- icon audition: change icon layers only and preserve panel theme/layout/placement/content.

The preview state machine, exact actions, lineage rules, built-in IDs and tests are normative in `docs/PRESET_SYSTEM_SPEC.md`.

## 13A.4 Initial catalog composition

The 15 Panel Presets are composed from the procedural, chassis, energy, closed-shell, free-ring, polygon, arc, 2.5D and optional-3D/fallback resources implemented in preceding work packages. The 15 Icon Presets are composed from the original, glass, metallic, neon, orb, pedestal, holographic, beveled and motion-enabled icon resources.

Raw samples do not become cards merely because they exist. Only processed, validated and redistributable resources are eligible.

# 14. Animation architecture

## 14.1 Replace one-effect-per-icon logic with animation profiles

The current `iconAnimation` string is useful as an early preset selector but insufficient for the requested product. A complete profile must describe:

- target;
- trigger;
- one or more animation tracks;
- timing;
- direction;
- repetitions;
- easing;
- intensity;
- phase/stagger;
- renderer requirements;
- reduced-motion substitute.

## 14.2 Animation targets

- icon glyph;
- icon tile/pedestal;
- entire icon visual;
- running indicator;
- badge/progress overlay;
- panel surface;
- panel glow layer;
- panel content track;
- panel segment;
- free-panel whole scene;
- window preview.

## 14.3 Triggers

- idle;
- hover enter;
- hover hold;
- hover exit;
- press;
- click completed;
- launch requested;
- launch succeeded;
- launch failed;
- application started running;
- application stopped;
- urgent/attention;
- drop entered;
- drop committed;
- panel reveal;
- panel conceal;
- panel open;
- panel collapse;
- profile/theme change;
- explicit user command.

Click and successful launch must be separate events.

## 14.4 Primitive tracks

- translate along X/Y/Z;
- translate along panel tangent or normal;
- scale X/Y/uniform;
- rotate X/Y/Z;
- orbit or spiral on a path;
- opacity;
- glow/emissive intensity;
- color/tint;
- blur amount;
- shadow offset/intensity;
- material/reflection parameter;
- panel path rotation;
- clip/open amount;
- part offset for split/shutter mechanisms.

## 14.5 Exact icon animation presets requested

### Slow vertical-axis rotation

**Requirement:** icon rotates left-to-right around its vertical axis.

- Logical name: `slow-y-turn`.
- Target: glyph by default; optionally entire tile.
- 2D/skinned implementation: flat-card Y-axis transform with horizontal compression, perspective, and lighting/highlight shift.
- True-3D implementation: mesh rotation around Y.
- Direction: left-to-right by default, reversible.
- Trigger: usually idle or hover-hold.
- Duration: user-configurable and intentionally slow.
- Hit area remains stable.

This is different from the current flat Z-axis `idle-rotate` behavior.

### Jump

- Move along the outward normal of the panel.
- Bottom panel jumps upward; top panel downward; left panel rightward; right panel leftward.
- Free/ring panel uses the path’s local outward normal.
- Optional squash/stretch.

### Shake

- Short tangent-axis translation and/or small Z rotation.
- Used for hover, click feedback, launch failure, or urgent state.
- Reduced-motion substitute: brief glow or color pulse.

### Enlarge

- Uniform scale or depth-aware scale.
- May influence neighboring icons through a configurable radius.
- Does not change the logical layout unless “physical rearrangement” is separately enabled.

### Spiral

- Icon follows a short polar/spiral trajectory around its anchor.
- Can combine translation, scale, rotation, and opacity.
- Must stay bounded so it does not collide excessively with neighboring icons or escape the host bounds.
- Particularly appropriate for free/ring layouts.

### Existing effects to retain as presets

Retain, normalize, and test current useful effects such as bounce, elastic, pulse, breathe, float, wave, spring, spin, swing, wobble, wiggle, orbit, ripple, magnetic, and glow. They should become profile definitions rather than hardcoded branches inside one delegate.

## 14.6 Exact panel animation presets requested

### Auto-hide/reveal

- Host visibility behavior, with a matching surface reveal animation.
- Native panel uses the Plasma host adapter where possible.
- Free panel uses its presentation controller and reveal zone inside the desktop host.

### Clockwise free-panel rotation

- Target: entire free/multi-shape scene or only its surface/content track.
- Intended for circles, rings, polygons, arcs, and similar layouts.
- Input region and hover detection must follow the transformed geometry.
- Native edge panels do not expose whole-panel rotation unless a theme explicitly supports rotation within fixed panel bounds.

### Hover glow

- 2D: glow mask, blurred layer, or shader-driven emissive effect.
- 2.5D: layered glow around baked surface and icon bases.
- 3D: emissive material/light intensity.
- Theme declares which layers glow.

### Visible collapsed panel that opens on hover

- Panel remains visually present as a rail, capsule, chassis, handle, orb, or closed mechanical form.
- Content is hidden, clipped, compressed, or moved into the shell.
- Hover, click, edge approach, or another configured trigger opens it.
- Closing waits until pointer, popup, drag, and keyboard focus have left.

### Horizontal opening/closing

Mechanisms:

- slide from center;
- split left/right;
- shutter panels;
- extend central track while caps remain fixed;
- unfold segments.

### Vertical opening/closing

Mechanisms:

- slide up/down;
- split top/bottom;
- expand thickness;
- lift a lid or front plate;
- reveal icons from behind the shell.

### Radial/3D opening

For rings and 3D themes:

- iris;
- radial fan;
- rotating ring segments;
- lifting icon pedestals;
- expanding concentric rings.

## 14.7 Composition rules

A profile may combine several tracks, for example:

```text
Hover:
  icon glyph → Y-axis turn
  icon tile → enlarge 1.12×
  tile glow → 0.2 to 1.0
  neighboring icons → smaller scale influence
```

Composition must be deterministic. Two profiles cannot both write the same property without an explicit priority or blend rule.

## 14.8 Motion safety

- Allocate effect margins to prevent clipping.
- Keep logical and visual transforms separate.
- Stop continuous animations when the panel is offscreen or concealed.
- Pause expensive 3D animation when not visible.
- Provide reduced-motion alternatives.
- Ensure animations terminate cleanly when themes or profiles change mid-transition.

---

# 15. Panel open/closed and visibility state machine

## 15.1 State model

```text
OPEN
  ├── request collapse → COLLAPSING → COLLAPSED
  └── host conceal → CONCEALING → CONCEALED

COLLAPSED
  ├── hover/click/reveal → OPENING → OPEN
  └── host conceal → CONCEALING → CONCEALED

CONCEALED
  └── reveal condition → REVEALING → COLLAPSED or OPEN
```

## 15.2 Guards

The panel must not close or conceal while:

- a context menu is open;
- a window preview is open and in use;
- an item is being dragged;
- the pointer is in the panel or reveal zone;
- keyboard focus is inside;
- Plasma Edit Mode is active;
- the panel is in a configuration preview that requests it remain open.

## 15.3 Native panel implementation

Use two layers:

1. **Plasma host visibility:** actual panel auto-hide/dodge/maximized behavior.
2. **Arch Dock surface presentation:** open versus collapsed shell inside the applet.

The first safe collapsed implementation should keep containment geometry stable and animate/clip the internal visual surface. Changing native containment length or thickness during every hover transition should be a later capability-gated enhancement because it can affect work areas and neighboring Plasma components.

## 15.4 Free panel implementation

The free desktop host can animate its scene within its widget bounds. The widget bounds must include:

- maximum open state;
- glow/shadow overflow;
- animation travel;
- rotated path bounds.

The input controller then exposes only the active shape/reveal region, preventing a large invisible rectangle from blocking unrelated desktop interaction.

---

# 16. Native panels versus free/multi-shape panels

| Capability | Native edge panel | Free/multi-shape desktop panel |
|---|---|---|
| Real Plasma panel containment | Yes | No; Plasma desktop applet host |
| Top/bottom/left/right edge | Yes | Arbitrary XY within desktop host |
| Standard Plasma widgets | Yes | Only if deliberately supported by free host |
| Native auto-hide | Yes, through host adapter | Visual conceal within desktop host |
| Dodge/maximized behavior | Yes, after visibility wiring | Limited by desktop-host visibility; visual behavior only initially |
| Arbitrary ring/spiral/arc layout | Constrained by panel bounds | Yes |
| Whole-panel rotation | Normally no | Yes |
| Non-rectangular input region | Limited by host | Required in free renderer |
| 2D skins | Yes | Yes |
| Baked 2.5D | Yes within bounds | Yes, preferred host |
| True 3D | Yes within host bounds | Yes, preferred host |
| Open/closed shell | Yes | Yes |

Panel Studio must hide settings that the selected host cannot support.

---

# 17. Settings and editing architecture

## 17.1 Native Plasma configuration

Register the existing configuration pages in `plasma-dock-widget/contents/config/config.qml`:

- General/Panel Studio entry;
- Layout;
- Appearance;
- Behavior;
- Animation.

These pages should show only settings relevant to the current panel and call the same backend transaction used by Panel Studio.

## 17.2 Panel Studio role

Panel Studio remains the advanced cross-panel manager for:

- panel creation/removal;
- screen and host management;
- theme browser/import;
- icon style browser;
- animation profiles;
- free-panel layout;
- 3D settings;
- profiles;
- diagnostics and fallback status.

It must not implement a parallel renderer or parallel setting semantics.

## 17.3 Live preview

Replace generic theme rectangles and mock controls with `LivePanelPreview.qml`, which instantiates the same `PanelScene` as the live applet using preview data.

Preview modes:

- horizontal native panel;
- vertical native panel;
- free desktop panel;
- open state;
- collapsed state;
- hover state;
- active/running/urgent icons;
- 2D/2.5D/3D renderer status.

## 17.4 Capability-driven controls

Examples:

- 3D controls hidden when 3D is disabled or unavailable.
- Whole-panel rotation shown only for free/multi-shape hosts or explicitly supported themes.
- Ring radius shown only for radial layouts.
- 9-slice settings shown only to theme developers, not ordinary users.
- Horizontal shutter shown only if the theme has separate parts or a compatible procedural mechanism.
- Unsupported settings are not silently accepted.

## 17.5 Transactional apply

Applying a draft should:

1. normalize values;
2. resolve capabilities;
3. report conflicts;
4. persist one atomic panel revision;
5. apply host changes;
6. apply renderer changes;
7. read back actual host state;
8. roll back or fall back safely if a required step fails.

---


## 17.6 Preset browsers

Panel Studio adds separate Panel Preset and Icon Preset browsers. Built-in and user-owned resources are visually separated. Each card shows real renderer output, compatibility, renderer tier/fallback, behavior and motion metadata. A card with unresolved required references must not expose a working Apply action.

## 17.7 Desktop audition transaction

Desktop audition uses the same normalized draft as embedded preview but applies it through a runtime-only `PresetPreviewSession`. The active configuration is written only on `Apply as Active`. `Save as Custom Preset` writes a new user-owned preset. `Set as Default` affects only future panel/icon creation. `Cancel` and `Revert` restore the exact previous state or remove a temporary preview host. A crash-recovery record may exist only to clean up preview-owned hosts.

# 18. Interaction features still required

These are not the first dependency block, but they belong in the complete product plan.

## 18.1 Window previews

Use existing window IDs/titles to build:

- hover preview popup;
- individual window thumbnails where available;
- window title list fallback;
- activate individual window;
- close individual window;
- minimize/restore individual window;
- grouped application window count.

Add planned:

- `src/content/WindowPreviewModel.*`
- `qml/ArchDock/Rendering/previews/WindowPreviewPopup.qml`

## 18.2 Complete application context menu

Add:

- New instance;
- individual windows;
- close/minimize/restore individual window;
- desktop-file actions where available;
- Pin/Unpin;
- Icon Properties;
- Edit Launcher where safe;
- Activities/virtual desktop actions only after reliable KDE integration.

## 18.3 Folder expansion

The current folder settings need an actual renderer:

- fan;
- grid;
- stack;
- arc;
- ring.

Folder expansion uses the same layout engine and animation profiles. It should not be a separate geometry implementation.

## 18.4 Segments

Segments are independent panel surface groups such as:

- launcher segment;
- running tasks segment;
- status segment;
- clock/system segment.

Each segment needs:

- content source;
- background/style;
- spacing/padding;
- corner rules;
- open/closed behavior;
- optional independent animation.

## 18.5 Notifications and progress

Add an overlay model for:

- unread badge;
- attention marker;
- task progress;
- download/install progress;
- temporary status indicator.

The source must be a supported application/system integration, not fabricated values.

## 18.6 System status modules

The current C++ `SystemStatus` data can drive optional modules for:

- battery;
- network;
- CPU;
- memory;
- disk;
- GPU.

User-facing desktop-suite elements such as Bluetooth, Wi-Fi, sound, time/date, battery, and notifications should preferably use or integrate with standard Plasma applets when hosted in a native panel. Arch Dock-native status widgets are appropriate mainly for free panels or custom unified themes.

## 18.7 Profiles and shortcuts

A profile stores:

- panel set;
- visibility/presentation rules;
- theme/icon style references;
- layouts;
- animation profile references;
- screen assignments.

Implement save/load/rename/delete/import/export first. Add global shortcuts only after the profile store is reliable.

---

# 19. Exact source-file implementation map

| Existing location | Required action |
|---|---|
| `CMakeLists.txt` | Track all required assets; create/install shared rendering module; add optional 3D dependency only in the 3D phase; add new tests; fix install/session-start consistency. |
| `src/panel/PanelWindow.*` | Keep D-Bus API stable; move lifecycle, visibility, theme, and content logic into controllers incrementally. |
| `src/PanelRegistry.*` | Add schema version, typed normalization, host association fields, animation/theme references, migrations, and atomic updates. |
| `src/NativeContainmentLifecycle.*` | Extend beyond simple association reconciliation to explicit create/show/hide/recreate/remove lifecycle tests. |
| `src/PanelPlacement.*` | Convert into normalized placement policy; delegate Plasma mutations to adapter. |
| `src/PanelVisibility.*` | Wire active-window overlap rules and presentation state; stop returning a permanent false result through the façade. |
| `src/DockModel.*` | Add panel-specific content/order behavior and individual-window actions needed by previews. |
| `src/WindowModel.*`, `WindowWatcher.*` | Provide reliable updated window geometry/state and startup synchronization. |
| `src/SystemStatus.*` | Keep backend; expose only when a real module consumes it. |
| `qml/runtime/SettingsPopup.qml` | Remove hardcoded allowlist drift; bind controls to schema/capabilities; hide non-working 3D sections; move toward `qml/studio`. |
| `qml/runtime/StudioForm.qml` | Capability-driven visibility, not merely disabled controls; use real preview renderer. |
| both `DockGeometry.js` copies | Replace with one shared layout engine. |
| `plasma-dock-widget/contents/ui/main.qml` | Become host/data bridge into shared `PanelScene`; retain Plasma-specific edit-mode and containment handling. |
| `DockEntry.qml` | Reduce hardcoded animation branches; keep interaction and delegate data; use shared motion/icon scene. |
| `IconVisual.qml` | Replace with layered shared icon renderer while preserving current behavior during migration. |
| `RunningIndicator.qml` | Move into shared renderer and add style contract. |
| `contents/config/config.qml` | Register all working native configuration pages. |
| `configLayout/Appearance/Behavior/Animation.qml` | Bind to the same schema and capability rules as Panel Studio. |
| `data/themes/builtin-themes.json` | Track it; do not claim capabilities with empty implementations; migrate to complete versioned definitions. |
| `docs/theme-packages.md` | Replace/extend with version-2 schema and compatibility rules. |
| `AUTONOMOUS_PROGRESS.md` | Replace with accurate `CURRENT_STATE.md` or update completely. |
| `data/arch-dock.service` | Align executable path and installation/activation strategy; document session startup. |

| planned `src/model/PanelPresetDefinition.*`, `IconPresetDefinition.*` | Define separate typed loadable presets; do not merge them into themes, icon styles or profiles. |
| planned `src/presets/*` | Load immutable built-ins, persist user presets/defaults, apply presets and own transactional desktop audition/rollback. |
| planned `qml/studio/presets/*` | Provide separate built-in/user Panel and Icon preset browsers, shared-renderer cards and audition action/status UI. |
| planned `data/presets/panels`, `data/presets/icons` | Install exactly 15 validated built-in definitions in each catalog for the first release. |

---

# 20. Ordered implementation roadmap

Each work package must leave the project buildable and testable.

## AD-0001 — Canonical baseline and repository truth

### Goal

Create one clean, reproducible starting point before changing behavior.

### Work

- Inventory all 29 local commits.
- Save the current working tree diff.
- Decide and commit the current built-in theme and 3D UI work separately.
- Add this master plan under `docs/`.
- Replace stale architecture status documentation.
- Remove the stale build directory from verification.
- Configure a fresh build directory.
- Run existing tests and record actual results.
- Stage-install into an isolated prefix.

### Completion gate

- Clean Git status.
- Clean build from source.
- Current test result recorded.
- No missing untracked runtime resource.
- The exact commit becomes the canonical baseline.

---

## AD-0002 — Native panel lifecycle repair

### Goal

No native panel is lost because it was hidden, Plasma restarted, or an association became stale.

### Work

- Define create/show/hide/recreate/remove transitions.
- Repair visible-without-host recovery.
- Stop using host deletion for ordinary visibility.
- Preserve ownership verification.
- Add regression tests for hide → show, Plasma restart, stale ID, and screen recovery.

### Completion gate

A managed native panel can be hidden, shown, restarted, recovered, and permanently removed without touching an unrelated Plasma panel.

---

## AD-0003 — Free-panel host lifecycle repair

### Goal

Panel Studio and Plasma template creation produce the same reliable free host.

### Work

- Persist free containment/applet/token association.
- Unify creation transaction.
- Add rollback.
- Remove the actual host on deletion.
- Recover or safely detach stale free records.
- Test PlasmaShell restart and screen changes.

### Completion gate

No free panel is record-only and no removed free panel leaves an orphan desktop widget.

---

## AD-0004 — Native placement and visibility synchronization

### Goal

Every currently exposed placement/visibility value either works or is removed from the interface.

### Work

- Implement placement adapter.
- Apply edge, screen, alignment, offset, thickness, length mode, and supported floating values.
- Wire always/auto-hide/dodge/maximized behavior.
- Read back actual applied host values.
- Add failure reporting.

### Completion gate

Changing an enabled control produces the verified corresponding Plasma behavior, and unsupported controls are absent.

---

## AD-0005 — Versioned panel schema and capability model

### Goal

Create one authoritative configuration contract shared by backend, live renderer, native config, and Panel Studio.

### Work

- Add schema version.
- Add typed panel definition and runtime state.
- Add migrations from current flat records.
- Add host/theme capability resolver.
- Replace manual editable-key allowlist with schema-driven validation.
- Add atomic update/revision semantics.

### Completion gate

Every saved value has one definition, type, default, validation rule, capability rule, and migration path.

---

## AD-0006 — Shared renderer foundation

### Goal

The live applet and Panel Studio preview use the same scene and geometry engine.

### Work

- Create shared QML module.
- Move/copy behavior incrementally behind compatibility wrappers.
- Consolidate geometry engine.
- Introduce `PanelScene`, surface loader, icon scene, and input region contract.
- Keep current 2D appearance visually equivalent during migration.
- Add QML import/install tests.

### Completion gate

One renderer implementation powers both live and preview output, with no duplicate geometry file.

---

## AD-0007 — Theme package version 2 and sample catalog

### Goal

Turn samples into managed production inputs rather than loose screenshots.

### Work

- Finalize version-2 manifest.
- Add asset provenance/catalog file.
- Add alpha-mask, crop, slice, safe-region, and state metadata.
- Maintain version-1 import compatibility.
- Add package validator and migration tests.
- Classify all 122 panel and 16 icon source images.

### Completion gate

A theme package can fully describe a procedural, 2D skin, 2.5D, or 3D-capable theme and fail safely when invalid.

---

## AD-0008 — First production 2D panel-skin family

### Goal

Implement real usable skins from the strongest horizontal samples.

### Initial sample targets

- panel samples 10–13 as a blue/red/dark chassis family;
- selected 14–17 as original adapted variants, after removing embedded content.

### Work

- Prepare transparent layered assets.
- Define cap/center slices.
- Define content-safe region.
- Support horizontal and vertical orientation where visually valid.
- Add open/collapsed visual states.
- Add tint and glow masks where appropriate.
- Test dynamic length and scaling.

### Completion gate

At least one sample-derived theme works as a real native and free 2D panel skin at multiple lengths and scale factors.

---

## AD-0009 — Energy/glow skin family

### Goal

Implement samples 45–72 as layered animated-skin families rather than static screenshots.

### Work

- Prepare base/frame/glow/overlay layers.
- Create color variants through masks where possible.
- Implement hover glow.
- Implement horizontal open/close mechanisms for compatible themes.
- Add reduced-motion static state.
- Verify no excessive clipping or invisible input blocking.

### Completion gate

A cyan, green, orange/red, and purple energy family exists with working hover and open/closed states.

---

## AD-0010 — Icon-style system and Icon Properties connection

### Goal

Implement original Arch Dock icon-pad families based on the icon references.

### Work

- Build metallic blue/red, neon green/orange, and orb/pedestal styles.
- Preserve real app glyphs by default.
- Add per-state assets/parameters.
- Add per-icon override storage.
- Add Icon Properties to the live context menu.
- Add reset-to-panel-default.

### Completion gate

The user can select a global icon style and override one icon without affecting others; all states remain legible.

---

## AD-0011 — Data-driven 2D animation engine

### Goal

Replace the growing conditional animation block with reusable animation profiles.

### Work

- Add profile model and validator.
- Add trigger dispatcher.
- Add track composition and conflict rules.
- Reimplement existing effects as presets.
- Implement requested slow Y-axis turn, jump, shake, enlarge, and spiral.
- Separate click from successful launch.
- Wire reveal trigger.
- Add reduced-motion substitutions.

### Completion gate

The same icon can combine multiple tracks, effects run on correct events, and no preset requires a new branch in `DockEntry.qml`.

---

## AD-0012 — Panel presentation and animation engine

### Goal

Implement open, collapsed, concealed, and revealed panel behavior.

### Work

- Add presentation state machine.
- Add horizontal slide/split/shutter presets.
- Add vertical slide/split presets.
- Add hover glow.
- Add panel-open guards for popup/drag/focus/edit mode.
- Connect host visibility with visual presentation.
- Expose only supported theme mechanisms.

### Completion gate

A regular panel can remain as a closed visible shell, open on hover, remain open while in use, and close reliably afterward.

---

## AD-0013 — Free/multi-shape renderer hardening

### Goal

Make free layouts reliable enough to support rings, arcs, polygons, and whole-panel rotation.

### Work

- Complete launcher/tasks/hybrid semantics.
- Implement free-entry reordering.
- Use canonical geometry output.
- Add transformed input region.
- Add clockwise/counter-clockwise panel rotation.
- Add ring/arc/semicircle/fan/spiral layout tests.
- Define desktop-host visibility limitations clearly.

### Completion gate

A free ring or arc panel can be created, populated, reordered, rotated, moved, recovered, and removed without orphaning its host.

---

## AD-0014 — Baked 2.5D ring and arc themes

### Goal

Use samples 81–122 as functional perspective themes before true 3D modeling is complete.

### Work

- Remove source backgrounds and placeholder icons.
- Separate rear platform, icon track, and foreground occlusion layer.
- Define anchor paths and depth order.
- Scale icons according to path depth.
- Add hover glow and whole-panel rotation.
- Produce ring, octagonal, and arc families.

### Completion gate

Real application icons sit correctly on a perspective ring/arc and pass in front of or behind the platform according to depth.

---

## AD-0015 — Optional true-3D renderer

### Goal

Add actual mesh/material rendering without compromising the 2D product.

### Work

- Add optional 3D rendering module and build capability detection.
- Define 3D theme scene contract.
- Add camera, lighting, materials, and quality presets.
- Implement true Y-axis icon rotation.
- Implement 3D pedestal/ring rotation and emissive hover.
- Implement 3D open/close part animation.
- Add 2.5D and 2D fallbacks.
- Hide all detailed 3D controls when unavailable or disabled.

### Completion gate

A true 3D theme can be enabled, rendered, animated, disabled, and safely downgraded without breaking the panel or requiring 3D for ordinary users.

---

## AD-0016 — Window previews and complete context menus

### Goal

Finish grouped-window interaction.

### Work

- Add preview model and popup.
- Add title fallback when thumbnails are unavailable.
- Add per-window activate/minimize/restore/close.
- Add New Instance and Icon Properties.
- Add appropriate desktop-file actions.

### Completion gate

Multiple windows from one application can be individually seen and controlled from the dock.

---

## AD-0017 — Folders, segments, notifications, and status modules

### Goal

Implement the remaining advertised panel-content systems.

### Work

- Folder fan/grid/stack/ring expansion.
- Segment model and renderer.
- Badge/progress overlay model.
- Optional system-status modules.
- Native Plasma applet integration strategy for standard system controls.

### Completion gate

No placeholder page remains for these features; each visible control has a tested live result.

---

## AD-0018 — Presets, profiles, shortcuts, packaging, and release

### Goal

Deliver the built-in preset experience, then produce a clean, installable, recoverable Arch Dock release.

### Work

- Add separate `PanelPresetDefinition` and `IconPresetDefinition` schemas, immutable built-in catalogs and user preset stores.
- Install exactly 15 working Panel Presets and 15 working Icon Presets with shared-renderer cards.
- Add transactional live-desktop audition with Apply as Active, Save as Custom Preset, Set as Default, Cancel and Revert.
- Profile store and import/export, preserving resolved configurations and optional preset lineage.
- Global shortcuts after profile reliability.
- Clean systemd/D-Bus startup strategy.
- PKGBUILD or agreed Arch packaging route.
- Install/uninstall documentation.
- Versioning/changelog.
- Multi-monitor, scaling, accessibility and preset-audition validation on Arch Linux/KDE Plasma 6/Wayland.
- Safe rollback and configuration/user-preset backup.
- Remove debug logs and resolve genuine QML diagnostics.

### Completion gate

A clean clone can be built, packaged, installed, started and used on Arch Linux/KDE Plasma 6/Wayland; all 15+15 built-ins render and load; desktop audition commits or rolls back exactly; customized derivatives persist; and the product can be upgraded and uninstalled without manual source-tree intervention.

---

# 21. Testing strategy

## 21.1 Unit tests

- panel schema normalization;
- version migrations;
- ownership association;
- native lifecycle transitions;
- free-host lifecycle transitions;
- panel placement mapping;
- visibility decisions;
- theme manifest validation;
- animation profile validation;
- content ordering;
- renderer capability resolution;
- fallback selection.
- panel/icon preset parsing, lineage and built-in immutability;
- preset catalog count/reference validation;
- PreviewSession commit, cancel, rollback and crash-cleanup decisions.

## 21.2 QML tests

- canonical geometry for every path;
- icon motion triggers;
- panel presentation state machine;
- reduced motion;
- input-region behavior;
- capability-driven control visibility;
- live preview/live renderer equivalence;
- theme and icon-style state switching.
- separate Panel/Icon preset browsers and card actions;
- embedded preview versus live-audition state;
- user-derived preset save/default behavior.

## 21.3 Visual regression tests

Render controlled scenes for:

- horizontal and vertical native panels;
- free ring/arc panels;
- each renderer tier;
- open and collapsed states;
- icon normal/hover/active/running/urgent states;
- multiple scale factors.

Compare against approved reference images with a controlled tolerance. Visual tests must not replace interaction tests.

## 21.4 Plasma-session integration tests

In an isolated disposable session:

- create each native panel type;
- create a free panel;
- verify ownership markers;
- hide/show/recreate;
- restart PlasmaShell;
- add/remove screens;
- change edge/alignment/length/visibility;
- preserve unrelated panels;
- remove managed panels;
- verify no orphan applet remains.
- audition a preset on an owned existing panel and restore it exactly;
- create/cancel a temporary preview panel and verify no host/record remains;
- commit a preview panel and verify ownership conversion;
- audition an Icon Preset without changing panel settings.

## 21.5 Manual Wayland matrix

- one monitor;
- multiple monitors;
- monitor hot-plug;
- 100%, 125%, 150%, and 200% scale where available;
- top/bottom/left/right native panels;
- free ring/arc panel;
- Plasma Edit Mode;
- maximized/fullscreen windows;
- reduced motion;
- 2D fallback;
- 3D enabled and disabled;
- service and PlasmaShell restart.
- all 15 Panel Presets and 15 Icon Presets;
- preset audition Apply/Cancel/Save Custom/Set Default workflows.

## 21.6 Performance and resource safety

- Continuous animations stop when not visible.
- Texture/mesh caches have limits.
- Theme changes release old resources.
- 3D failure cannot crash the 2D panel.
- Free-panel transparent bounds do not consume unrelated desktop clicks.
- A malformed theme cannot escape its managed directory or execute arbitrary code.

---

# 22. Release definition of done

Arch Dock is release-ready only when all of the following are true:

## Repository

- clean Git state;
- all required resources tracked;
- current documentation accurate;
- no stale source-of-truth file;
- versioned release commit/tag.

## Build and installation

- clean build from clone;
- all tests pass;
- staged install verified;
- service starts through the documented mechanism;
- uninstall leaves no broken Plasma configuration.

## Panel lifecycle

- native and free panels create, recover, hide/show, and remove safely;
- unrelated Plasma panels remain untouched;
- no orphan free host;
- multi-monitor recovery works.

## Interface

- every visible control works or is explicitly experimental;
- no UI-only 3D setting;
- native configuration and Panel Studio use the same schema;
- preview matches live renderer.
- built-in Panel and Icon preset browsers are separate and use actual renderer output;
- exactly 15 valid built-in Panel Presets and 15 valid built-in Icon Presets are installed;
- built-ins are immutable and customized derivatives are reusable.

## Rendering

- procedural 2D works independently;
- at least one production skinned-2D family works;
- icon styles and per-icon overrides work;
- requested icon animations work;
- regular panel collapsed/open behavior works;
- free-panel rotation and glow work;
- 2.5D ring/arc works;
- true 3D, when included, has a safe fallback.

## Interaction

- grouped windows can be individually controlled;
- context menus expose implemented actions only;
- edit mode and drag/drop do not conflict with normal launching;
- reduced motion works.
- desktop audition changes only the selected/temporary owned host and rolls back exactly on Cancel;
- Icon Preset audition does not change panel theme/layout/placement;
- no canceled preview leaves an applet, record, default or ownership token.

---

# 23. Recommended immediate next step

The next implementation activity should **not** be skin rendering or new animation code. It should be:

## Start with AD-0001, then AD-0002 and AD-0003

Reason:

- the source has an unclean and partly untracked state;
- build artifacts are stale;
- native hide/restore can lose a panel association;
- free-panel creation/removal can create record-only or orphan states;
- any visual implementation added before those fixes would be tested on an unreliable lifecycle foundation.

After AD-0001 through AD-0005 are complete, the first visible milestone should be AD-0006 through AD-0009: one shared renderer, one real science-fiction chassis skin family, and one real energy/glow family.

That produces visible progress without prematurely committing the project to a fragile 3D implementation.

---

# 24. Final architecture decision summary

1. **Keep the existing project. Do not restart it.**
2. **Repair lifecycle and configuration truth before adding skins.**
3. **Use one shared renderer for live panels and previews.**
4. **Support four renderer tiers: 2D procedural, 2D skin, 2.5D, and true 3D.**
5. **Treat panel skins, icon styles, and animation profiles as separate reusable resources.**
6. **Convert sample images into validated theme packages; never use raw opaque screenshots directly.**
7. **Use data-driven animation profiles rather than expanding hardcoded branches.**
8. **Separate host visibility from the panel’s open/collapsed visual state.**
9. **Allow whole-panel rotation primarily on free/multi-shape panels.**
10. **Preserve Plasma ownership and native Edit Mode.**
11. **Hide unsupported settings.**
12. **Make 3D optional and always provide a safe 2D/2.5D fallback.**
13. **Ship separate immutable built-in Panel Preset and Icon Preset catalogs, exactly 15 of each for the initial release.**
14. **Use transactional live-desktop audition; Apply commits and Cancel restores/removes the preview exactly.**
15. **Keep themes, icon styles, presets and complete multi-panel profiles as distinct resource types.**

This structure supports the current product and the larger visual direction without forcing every panel sample, icon style, or animation into a separate custom implementation.


---

# Appendix A — Version-2 task decomposition

The implementation is decomposed into **86 tasks**. TASK-0001 through TASK-0076 retain the previous dependency order, with wording strengthened where needed. Two new tasks are inserted before profiles:

- `TASK-0077` — Panel/Icon preset definitions, catalogs, browsers and exact 15+15 built-in libraries.
- `TASK-0078` — Transactional live-desktop audition and active/custom/default workflows.

The previous profile/release tail shifts by two task numbers:

```text
old TASK-0077 → new TASK-0079
old TASK-0078 → new TASK-0080
old TASK-0079 → new TASK-0081
old TASK-0080 → new TASK-0082
old TASK-0081 → new TASK-0083
old TASK-0082 → new TASK-0084
old TASK-0083 → new TASK-0085
old TASK-0084 → new TASK-0086
```

The revised Codex pack is the authoritative task-number source. Do not combine the 84-task and 86-task packs in one execution sequence.
