# Arch Dock preset catalog and live desktop audition specification

**Status:** Mandatory architecture supplement for the 86-task Codex plan.  
**Target platform:** Arch Linux, KDE Plasma 6, Wayland, Qt 6/KF6/Kirigami.  
**Relationship to the master plan:** This document adds the built-in panel/icon preset system and live-desktop audition workflow. It does not replace the theme, icon-style, profile, renderer, lifecycle, or ownership architecture.

# 1. Product meaning

Arch Dock must expose two separate, loadable libraries in Panel Studio:

1. **Panel Presets** — complete single-panel starting configurations.
2. **Icon Presets** — reusable icon appearance and motion configurations.

These are not the same as themes, icon styles, or profiles.

| Concept | Meaning |
|---|---|
| Panel theme/skin | Visual surface resource used by a panel renderer. |
| Icon style | Visual layers and state styling applied around or to application glyphs. |
| Panel preset | A loadable one-panel configuration that can combine host defaults, content type, placement, layout, theme, colors, visibility, presentation and motion. |
| Icon preset | A loadable icon configuration that combines an icon style, state overrides and optional motion defaults. |
| Profile | A complete multi-panel desktop arrangement, including multiple panel definitions and screen assignments. |

A panel preset may recommend an icon preset, but the two remain independently selectable. Applying an icon preset must not silently replace the panel theme. Applying a panel preset must not permanently overwrite a user-selected icon preset unless the user explicitly accepts the panel preset's recommended icon pairing.

# 2. Required initial built-in libraries

The first release must install **exactly 15 functional built-in Panel Presets** and **exactly 15 functional built-in Icon Presets**. The number is an acceptance target, not a request for 30 placeholder cards.

Every shipped preset must:

- parse successfully;
- pass schema and reference validation;
- render through the shared renderer;
- expose only supported controls;
- have a deterministic preview;
- load on the intended Arch Linux/KDE Plasma host;
- have a safe renderer fallback;
- be independently selectable;
- be customizable through a derived active configuration or user-owned preset;
- contain no non-working button or fabricated preview.

## 2.1 Built-in Panel Presets

The initial catalog uses these stable IDs and display names. Codex may adjust an implementation detail only when the verified resource names differ, but it must preserve the count, conceptual coverage and independent loadability.

| # | Stable ID | Display name | Intended foundation |
|---:|---|---|---|
| 1 | `obsidian-glass-dock` | Obsidian Glass Dock | Procedural 2D glass/minimal fallback. |
| 2 | `metallic-shelf-dock` | Metallic Shelf Dock | Procedural or skinned 2D shelf. |
| 3 | `sci-fi-chassis-blue` | Blue Sci-Fi Chassis | First chassis skin family. |
| 4 | `sci-fi-chassis-red` | Red Sci-Fi Chassis | First chassis skin family. |
| 5 | `sci-fi-chassis-dark` | Dark Sci-Fi Chassis | First chassis skin family. |
| 6 | `energy-frame-cyan` | Cyan Energy Frame | Energy/glow skin family. |
| 7 | `energy-frame-green` | Green Energy Frame | Energy/glow skin family. |
| 8 | `energy-frame-orange` | Orange Energy Frame | Energy/glow skin family. |
| 9 | `energy-frame-purple` | Purple Energy Frame | Energy/glow skin family. |
| 10 | `minimal-neon-rail` | Minimal Neon Rail | Procedural 2D compact rail. |
| 11 | `mechanical-collapsible-rail` | Collapsible Mechanical Rail | Visible closed shell with hover opening. |
| 12 | `circular-blue-ring` | Circular Blue Ring | Free-panel ring; 2.5D or safe 2D fallback. |
| 13 | `octagonal-platform` | Octagonal Platform | Free-panel polygon/2.5D platform. |
| 14 | `orange-arc-dock` | Orange Arc Dock | Free-panel arc/semicircle family. |
| 15 | `holographic-semicircle` | Holographic Semicircle | Free-panel 2.5D/optional 3D with fallback. |

Panel presets may be composed from implemented theme, layout, presentation and animation resources. They do not require 15 unrelated raw screenshots. Raw sample images remain production inputs and references; only processed, validated, redistributable derivatives may ship.

## 2.2 Built-in Icon Presets

| # | Stable ID | Display name | Intended foundation |
|---:|---|---|---|
| 1 | `original-clean` | Original Clean | Real application glyph, minimal/no tile. |
| 2 | `glass-tile` | Glass Tile | Transparent glass tile. |
| 3 | `metallic-blue` | Metallic Blue | Metallic blue icon style. |
| 4 | `metallic-red` | Metallic Red | Metallic red icon style. |
| 5 | `neon-green` | Neon Green | Neon green icon style. |
| 6 | `neon-orange` | Neon Orange | Neon orange icon style. |
| 7 | `dark-orb` | Dark Orb | Dark orb style. |
| 8 | `blue-pedestal` | Blue Pedestal | Blue pedestal/orb hybrid. |
| 9 | `red-pedestal` | Red Pedestal | Red pedestal/orb hybrid. |
| 10 | `holographic-tile` | Holographic Tile | Holographic frame/tile. |
| 11 | `minimal-glow` | Minimal Glow | Original glyph plus restrained glow. |
| 12 | `beveled-sci-fi` | Beveled Sci-Fi | Beveled science-fiction tile. |
| 13 | `metallic-blue-slow-turn` | Metallic Blue — Slow Turn | Metallic Blue plus `slow-y-turn`. |
| 14 | `neon-green-enlarge` | Neon Green — Enlarge | Neon Green plus enlarge/neighbor influence. |
| 15 | `dark-orb-spiral` | Dark Orb — Spiral | Dark Orb plus bounded spiral profile. |

The real application glyph remains the default glyph policy unless an explicit complete mapped replacement exists.

# 3. Domain models

## 3.1 PanelPresetDefinition

```text
PanelPresetDefinition
├── schemaVersion
├── identity
│   ├── id
│   ├── name
│   ├── description
│   ├── builtIn
│   ├── revision
│   ├── derivedFromPresetId
│   └── sourceRevision
├── preview
│   ├── previewMode
│   ├── rendererTier
│   ├── fallbackTier
│   └── deterministicPreviewSeed
├── compatibility
│   ├── hostKinds
│   ├── orientations
│   ├── layouts
│   ├── requiredCapabilities
│   └── optionalCapabilities
├── panel
│   ├── host defaults
│   ├── content type
│   ├── placement defaults
│   ├── visibility defaults
│   ├── presentation defaults
│   ├── layout definition
│   ├── theme reference
│   ├── surface overrides
│   ├── optional recommended icon preset
│   └── motion profile references
└── fallback
    ├── safe theme reference
    ├── safe icon preset reference
    └── unsupported-field policy
```

A built-in definition is immutable in the installed catalog. A user-derived preset stores a complete normalized snapshot plus lineage metadata. It must not depend on the original built-in continuing to exist in exactly the same revision.

## 3.2 IconPresetDefinition

```text
IconPresetDefinition
├── schemaVersion
├── identity
│   ├── id
│   ├── name
│   ├── description
│   ├── builtIn
│   ├── revision
│   ├── derivedFromPresetId
│   └── sourceRevision
├── compatibility
│   ├── renderer tiers
│   ├── required style capabilities
│   └── reduced-motion support
├── icon
│   ├── icon-style reference
│   ├── visual overrides
│   ├── state overrides
│   ├── glyph policy
│   ├── motion profile reference
│   └── per-state animation overrides
└── fallback
    ├── safe icon-style reference
    └── safe motion profile
```

## 3.3 Active panel provenance

`PanelDefinition` may contain optional non-authoritative lineage metadata:

```text
presetOrigin
├── panelPresetId
├── panelPresetRevision
├── iconPresetId
├── iconPresetRevision
├── customizedAfterApply
└── detachedFromPreset
```

The active panel remains a full normalized configuration. It must not become unusable merely because a catalog entry is later renamed, removed or upgraded.

# 4. Catalog and storage boundaries

```text
Installed built-in catalog
├── read-only PanelPresetDefinition packages
└── read-only IconPresetDefinition packages

User preset store
├── user-created panel presets
├── user-created icon presets
├── derived copies of built-ins
└── imported validated presets

Active panel configuration
├── resolved PanelDefinition
├── optional preset lineage
└── per-instance customizations

Default selection store
├── default panel preset for newly created panels
└── default icon preset for newly created panels/icons
```

Built-in packages must not be edited in place. “Customize” creates or modifies an active draft. “Save as Custom Preset” writes a new user-owned preset with a new stable ID.

# 5. Panel Studio information architecture

```text
Panel Studio
├── Panels
│   ├── Built-in Panel Presets
│   ├── My Panel Presets
│   ├── Panel Themes / Skins
│   └── Import
├── Icons
│   ├── Built-in Icon Presets
│   ├── My Icon Presets
│   ├── Icon Styles
│   └── Import
├── Animations
├── Profiles
└── Diagnostics
```

Each preset card displays:

- shared-renderer preview, not the source screenshot;
- name and description;
- built-in or user-owned status;
- host compatibility;
- panel content type where applicable;
- renderer tier and active fallback;
- supported layout/orientation;
- included visibility/presentation behavior;
- included motion profile;
- 3D requirement/fallback status;
- `Preview on Desktop`, `Apply as Active`, and context-appropriate duplicate/save actions.

Cards that cannot pass validation must not ship. A runtime capability mismatch may display a clear fallback or incompatibility state; it must not present a fake functional Apply action.

# 6. Preview modes

## 6.1 Embedded preview

Selecting a card updates the Panel Studio preview immediately through the shared `PanelScene`. This does not mutate the real desktop.

## 6.2 Live desktop audition

The user explicitly starts `Preview on Desktop`. Only one audition session is active at a time unless a later design explicitly expands this.

### Existing compatible panel

1. Snapshot the complete active definition and host state.
2. Create an ephemeral preview override.
3. Render the preset on the selected real panel without committing durable settings.
4. Stream later customization-draft changes into the session.
5. Commit only through `Apply as Active`.
6. Restore the exact snapshot through `Cancel` or `Revert`.

### New panel or incompatible host/layout

1. Create a temporary managed preview host with a preview-only ownership token.
2. Do not add it as a normal durable panel record until committed.
3. Apply the preset and customization draft to the temporary host.
4. `Apply as Active` atomically converts it into a normal managed panel.
5. `Cancel` removes the host and all preview-only records.

### Icon preset audition

- Changes only the icon preset/style/motion layers of the selected panel.
- Does not change the panel theme, layout, placement, visibility or content type.
- Restores the exact previous icon state on cancel.

# 7. PreviewSession state machine

```text
IDLE
  └── begin → PREPARING
PREPARING
  ├── success → ACTIVE
  └── failure → ROLLING_BACK → IDLE or BLOCKED
ACTIVE
  ├── update draft → ACTIVE
  ├── apply active → COMMITTING → COMMITTED → IDLE
  ├── save custom → ACTIVE
  ├── cancel/revert → ROLLING_BACK → IDLE
  └── host/service failure → ROLLING_BACK → IDLE or BLOCKED
```

The session records:

- session ID;
- target panel ID or temporary host ID;
- preview kind: panel or icon;
- original normalized definition;
- original verified host state;
- draft definition;
- temporary ownership token where used;
- renderer fallback state;
- commit/rollback progress;
- failure evidence.

Preview state is runtime state, not ordinary durable panel configuration. A bounded recovery journal may exist only to clean up a preview host after a crash; it must never make the preview the active configuration by accident.

# 8. User actions and exact semantics

| Action | Required behavior |
|---|---|
| Preview on Desktop | Start or replace one transactional preview session. No durable active-setting write. |
| Apply as Active | Commit the resolved draft to the selected panel or convert the temporary host into a managed panel. Built-in source remains unchanged. |
| Save as Custom Preset | Create a new reusable user-owned preset from the normalized draft, including lineage and a deterministic preview. Does not implicitly apply unless user also chooses Apply. |
| Set as Default | Affect only newly created panels/icons. Does not rewrite existing panels. |
| Cancel | Restore the exact pre-preview state or remove the temporary preview host. |
| Revert | Same state restoration as Cancel while keeping the browser open. |
| Restore Built-in Defaults | Reload the immutable built-in definition into the draft; still does not edit the installed package. |

# 9. Target source structure

```text
src/
├── model/
│   ├── PanelPresetDefinition.*
│   ├── IconPresetDefinition.*
│   └── PresetOrigin.*
├── presets/
│   ├── PanelPresetCatalog.*
│   ├── IconPresetCatalog.*
│   ├── BuiltInPresetLoader.*
│   ├── UserPresetStore.*
│   ├── PresetPackage.*
│   ├── PresetCapabilityResolver.*
│   ├── PresetApplicationService.*
│   ├── PresetPreviewSession.*
│   ├── PresetPreviewRecovery.*
│   └── PresetDefaultStore.*
└── persistence/
    └── ProfileStore.*

qml/studio/presets/
├── PanelPresetBrowser.qml
├── IconPresetBrowser.qml
├── PresetCard.qml
├── PresetDetails.qml
├── PresetActionBar.qml
├── DesktopAuditionStatus.qml
└── UserPresetSaveDialog.qml

data/presets/
├── panels/
│   └── 15 validated built-in definitions
└── icons/
    └── 15 validated built-in definitions

tests/
├── unit/
│   ├── PanelPresetDefinitionTest.*
│   ├── IconPresetDefinitionTest.*
│   ├── PresetCatalogTest.*
│   └── PresetPreviewSessionTest.*
├── qml/
│   ├── tst_PresetBrowser.qml
│   └── tst_PresetCardPreview.qml
├── integration/
│   ├── PresetApplicationTest.*
│   └── PresetPreviewRecoveryTest.*
└── plasma-session/
    └── preset_audition_matrix.*
```

This is a logical target. Codex must introduce files incrementally and may adapt names to verified repository conventions after the required read-only plan.

# 10. Arch Linux/KDE Plasma runtime rules

- Production runtime is Arch Linux with KDE Plasma 6 on Wayland.
- Native panel audition uses real owned Plasma panel containments.
- Free-panel audition uses the verified desktop-hosted applet architecture.
- Plasma ownership tokens are mandatory for temporary and persistent hosts.
- An X11, GNOME, Windows or generic Qt window test is not a substitute for the required Plasma/Wayland acceptance evidence.
- Standard Plasma panels and widgets remain untouched unless Arch Dock proves ownership.
- Standard Plasma Edit Mode remains functional during and after preview.
- Preview sessions must respect PlasmaShell restart, screen identity, scaling and host recovery rules.

# 11. Testing and release gates

## Unit

- panel/icon preset parsing and normalization;
- stable IDs and duplicate rejection;
- built-in immutability;
- user-derived round-trip;
- missing-reference fallback;
- default selection behavior;
- PreviewSession state transitions;
- exact rollback decisions.

## QML

- separate panel and icon browser pages;
- actual shared-renderer card previews;
- capability-driven action visibility;
- built-in versus user-owned affordances;
- keyboard and accessibility behavior;
- no placeholder/non-working cards.

## Integration

- apply a built-in panel preset to an existing compatible panel;
- cancel and verify byte-for-byte/normalized-equivalent restoration;
- preview an incompatible/free preset through a temporary host;
- cancel and verify no orphan applet/record;
- commit temporary host and verify ownership conversion;
- audition an icon preset without changing panel settings;
- save derived panel and icon presets;
- set defaults and verify only newly created panels use them;
- service/PlasmaShell interruption cleanup.

## Release

- exactly 15 built-in panel preset definitions installed and valid;
- exactly 15 built-in icon preset definitions installed and valid;
- every card renders and can be selected independently;
- every preset has working Apply/Cancel semantics;
- built-ins remain immutable;
- customized derivatives persist and reload;
- no live preview can mutate an unrelated Plasma panel;
- no canceled preview leaves a panel, applet, ownership token, record or default change behind.
