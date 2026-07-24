# Arch Dock Implementation Audit

Updated: 2026-07-24

## Current Architecture

`arch-dock` is a Qt 6 service on `org.archdock.ArchDock`. It owns the window
model, launcher persistence, panel registry, theme processing, KWin action
bridge, screen recovery, and utility configuration windows. The visible
edge-attached dock is `org.archdock.dock`, a Plasma 6 applet hosted by a native
panel containment. Arch Dock does not currently create a primary standalone
dock window.

## Existing Panel Families

The three content-bearing families are launcher, tasks, and hybrid. They use
the same applet renderer with filtered entries. An empty native panel is also
available as a Plasma layout template but has no Arch Dock renderer. Every
native family can be created on a Plasma-supported top, bottom, left, or right
edge. Registry ownership tokens prevent Arch Dock from modifying unrelated
containments.

## Edit Mode And KDE Integration

The applet reads `Plasmoid.containment.corona.editMode` and disables entry
interaction while Plasma edits the panel. Panel creation and ownership use
Plasma scripting and native containments. Before this audit the applet shipped
only one configuration page, so Plasma's Configure action exposed panel type
but not Arch Dock's existing layout, appearance, behavior, or motion controls.
The separate Panel Studio remained functional but was a disconnected second
entry point.

Plasma 6.7 provides the supported multi-page applet configuration mechanism via
`contents/config/config.qml`, `org.kde.plasma.configuration.ConfigModel`, and
KCM-backed category pages. Arch Dock should use that mechanism for normal panel
editing and retain Panel Studio for advanced cross-panel management.

## Rendering And Animation

The live applet now has reusable `DockEntry`, `IconVisual`, and
`RunningIndicator` components. It consumes per-panel values through DBus,
supports horizontal and vertical magnification, bounded motion, reduced motion,
plates, reflections, indicators, tooltips, dropping, and launcher reordering.
The geometry and motion policy helpers cover more layouts and effects than the
live edge applet currently renders.

## Theme Support

`PanelRegistry` defines a versioned `org.archdock.theme` package, validates and
copies source assets, analyzes raster/vector/scene inputs, renders adaptive 2D
previews, and detects optional Blender/ImageMagick conversion tools. The live
applet does not yet render all processed theme metadata, masks, safe regions,
or 3D scenes.

## Supplied Visual Reference Families

The supplied ObjectDock, RocketDock, Circle Dock, and experimental desktop-dock
samples establish capability targets, not assets to redistribute:

- edge shelves and reflective platforms with magnified center icons;
- individual pedestals and launch pads beneath each icon;
- circular and ring launchers with one or more concentric tracks;
- arcs, curved vertical paths, cascades, and stack/fan expansion;
- layered science-fiction platforms with animated light or hologram treatments.

These map to reusable Arch Dock primitives: panel surface, per-icon base,
indicator/attachment point, path geometry, layered decoration, and bounded
animation. Original themes must be created or imported through the versioned
theme pipeline; copyrighted reference artwork is not bundled.

## Paths, Build, And Runtime

The Git root and only project location is `/mnt/F/Arch Dock`. No source,
documentation, build configuration, or editor setting contains the obsolete
`/run/media/aivars/F` path. CMake builds the service and tests with Qt 6.11 and
the installed Plasma 6.7/KWin 6.7 stack. The registry, dock-geometry, and motion
tests pass. A user installation lives under `~/.local`.

## KDE And Wayland Limitations

- Public Plasma panel scripting supports edge containments, not arbitrary
  desktop coordinates or non-rectangular containment geometry.
- Wayland task identifiers exposed by `TaskManager` are process-local. The
  existing KWin script exports stable internal UUIDs to the Arch Dock service.
- Arbitrary free panels cannot safely reserve space or participate as native
  panels. A future free-panel adapter must use an isolated KWin/Wayland surface,
  remain optional, expose Plasma edit state through the bridge, avoid struts by
  default, and fail without affecting native panels.
- Live thumbnails and some panel-shell behavior rely on Plasma/KWin private
  implementation details. Version-specific use must stay behind adapters.

## Staged Plan

1. Add a native Plasma multi-category editor backed by a restricted, validated
   settings bridge. Keep Panel Studio for panel creation, duplication, imports,
   and advanced controls.
2. Complete native visibility mappings, alignment, length, margins, floating
   appearance, monitor handling, and isolated lifecycle tests.
3. Formalize shared panel/render interfaces and add settings-to-renderer tests.
4. Prototype the free-panel geometry engine in an isolated test surface,
   beginning with circle, arc, and polygon paths.
5. Add the optional free-panel KWin/Wayland adapter with explicit feature
   gating and rollback.
6. Expand reusable animations and theme rendering, then implement adaptive 2D
   imports and optional 3D conversion/presentation.
7. Finish performance measurements, accessibility, packaging, installation,
   rollback, and multi-monitor Wayland validation.

## Safety And Rollback

Before each live integration checkpoint, validate QML and metadata, run the
automated suites, commit a buildable source checkpoint, stage the installation,
and back up relevant Arch Dock/Plasma configuration. Native containment changes
must continue to verify ownership tokens. Experimental free-panel code must
never replace or mutate existing Plasma panels.
