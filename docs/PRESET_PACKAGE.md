# Arch Dock preset definitions

This document describes the Panel Preset and Icon Preset formats introduced by
TASK-0040 and the transactional desktop workflows added by TASK-0041. The normative
product requirements are in [PRESET_SYSTEM_SPEC.md](PRESET_SYSTEM_SPEC.md).

## Terminology

Five resource types stay separate. A file of one type is never accepted as
another.

| Type | What it is | Where it is edited |
|---|---|---|
| Panel theme / skin | How one panel surface looks | Panels → Panel Themes / Skins |
| Icon style | A reusable icon appearance set | Icons → Icon Styles |
| **Panel Preset** | A complete one-panel starting configuration | Panels → Built-in / My Panel Presets |
| **Icon Preset** | An icon style plus overrides and motion | Icons → Built-in / My Icon Presets |
| Profile | A complete multi-panel desktop (TASK-0042) | Profiles |

A Panel Preset may *recommend* an Icon Preset. The two remain independently
selectable.

## Directories

| Location | Content | Writable |
|---|---|---|
| `share/arch-dock/presets/panels/` | `builtin-panel-presets.json` and 15 definitions | No |
| `share/arch-dock/presets/icons/` | `builtin-icon-presets.json` and 15 definitions | No |
| `<AppDataLocation>/presets/` | `user-presets.json`, `panels/`, `icons/` | Yes |
| `<AppDataLocation>/presets/defaults.json` | Independent panel and icon defaults | Yes |
| `<AppDataLocation>/preset-preview-journal.json` | Bounded interruption-recovery record | Yes, while recovery is needed |

The installed catalog is located through `GenericDataLocation`
(`arch-dock/presets/panels/builtin-panel-presets.json`). An uninstalled build
uses the source tree's `data/presets/`. The user store is created on the first
save, never by browsing.

## Catalog index

Each built-in directory holds one index naming every preset in display order,
and one `<id>.json` file per preset:

```json
{
  "format": "org.archdock.panel-preset-catalog",
  "version": 1,
  "presets": ["obsidian-glass-dock", "metallic-shelf-dock"]
}
```

The icon index uses `org.archdock.icon-preset-catalog`. A catalog is accepted
whole or rejected whole. It is rejected when an entry is missing, duplicated,
unlisted (a `*.json` file the index does not name), not a regular file, larger
than 256 KiB, reached through a path that leaves the directory, or when the
definition's own id differs from its file name.

## Shared header

Both formats start with the same header:

| Field | Rule |
|---|---|
| `format` | `org.archdock.panel-preset` or `org.archdock.icon-preset` |
| `schemaVersion` | `1` |
| `identity.id` | `[a-z0-9][a-z0-9.-]{0,63}`; user presets start with `user-` |
| `identity.name` | Required, at most 256 bytes, no surrounding space |
| `identity.description` | Optional, at most 2048 bytes |
| `identity.builtIn` | `true` exactly when the id is not a user id |
| `identity.revision` | Integer ≥ 1 |
| `identity.derivedFromPresetId`, `identity.sourceRevision` | Lineage of a user preset; never present on a built-in |

Unknown fields are errors. Values are never clamped, defaulted or lower-cased
on the way in: a value is either exactly valid or reported with its JSON
pointer.

## Panel Preset

| Section | Fields |
|---|---|
| `preview` | `previewMode` (`horizontal`, `vertical`, `free`), `rendererTier`, `fallbackTier`, `deterministicPreviewSeed` |
| `compatibility` | `hostKinds`, `orientations`, `layouts`, `requiredCapabilities`, `optionalCapabilities` |
| `panel.host` | `edge` |
| `panel.content` | `type` |
| `panel.placement` | `alignment`, `dynamic`, `width`, `height` |
| `panel.visibility` | `visibilityMode` |
| `panel.presentation` | `presentationMode`, `presentationTrigger`, `collapseMechanism`, `collapseAxis`, `revealHandle` |
| `panel.layout` | `layout`, `layoutScale`, `layoutAngle`, `layoutRadius`, `layoutRows`, `layoutPadding`, `pathSides`, `pathOrientation`, `panelRotationMode`, `panelRotationSpeed`, `panelRotationTrigger`, `iconShape`, `iconSize`, `spacing` |
| `panel.theme` | `completeThemeId`, `rendererTier` |
| `panel.surface` | `appearance`, `shape`, `opacity`, `color`, `glowIntensity`, `scene3DQuality` |
| `panel.motion` | `iconAnimation`, `animationTrigger`, `animationSpeed`, `animationIntensity`, `magnificationRadius`, `magnificationFalloff` |
| `panel.recommendedIconPresetId` | An Icon Preset id |
| `fallback` | `themeId` (empty names the built-in procedural surface), `iconPresetId`, `unsupportedFieldPolicy` (`reject` or `drop`) |

Every `panel.*` key is a user-editable field of the panel settings schema, and
its value must already be what the schema would normalise it to. That is the
safety boundary: host ids, ownership tokens, screen identity and content lists
are not editable schema fields, so no preset can carry them. The body becomes
an ordinary `PanelDefinition` and must pass `isValid()`.

A preset is a complete snapshot. Every value its theme would set is written
out, so a user copy stays loadable whatever later happens to the theme's
defaults or to the preset it came from.

Catalog-time checks beyond the schema:

- the theme, recommended and fallback Icon Presets exist;
- the motion profile exists and supports both declared renderer tiers;
- with every renderer present the preset resolves without a fallback, at
  exactly `preview.rendererTier`;
- `preview.fallbackTier` is a tier the theme really falls back to;
- a different `fallback.themeId` can really draw the preset.

## Icon Preset

| Section | Fields |
|---|---|
| `compatibility` | `rendererTiers`, `requiredStyleCapabilities`, `reducedMotionSupport` |
| `icon.iconStyleId` | An installed icon style |
| `icon.visualOverrides` | Layer id → `shape`, `color`, `secondaryColor`, `borderColor`, `opacity`, `inset`, `radius`, `borderWidth` |
| `icon.stateOverrides` | Icon state id → state fields |
| `icon.glyphPolicy` | The icon-style glyph policy |
| `icon.motion` | `profileId`, `animationTrigger`, `animationSpeed`, `animationIntensity`, `magnificationRadius`, `magnificationFalloff` |
| `icon.perStateAnimationOverrides` | Icon state id → motion profile id |
| `fallback` | `iconStyleId`, `motionProfileId` |

Overrides are merged into the referenced style's own manifest and the result
is validated by the icon-style package parser, exactly as an installed style
is. An override can therefore never produce a style the renderer has not been
proven to draw. Five built-ins (`glass-tile`, `blue-pedestal`, `red-pedestal`,
`holographic-tile`, `beveled-sci-fi`) are a shipped style plus such overrides.

## Compatibility and fallback

`PresetCapabilityResolver` answers through the same `PanelCapabilityResolver`
every panel uses:

| State | Meaning | Card |
|---|---|---|
| Ready | Drawn exactly as declared | Preview |
| Safe fallback | Drawn through the declared fallback tier or theme | Preview, with the reason |
| Incompatible | A required capability or the fallback is unavailable | No preview, no action that applies it |

## User presets

`UserPresetStore` writes only inside `<AppDataLocation>/presets/` and only
user ids (`user-` plus 12 hexadecimal digits). It has no path to the installed
catalog, so a built-in cannot be overwritten through it.

- **Duplicate** copies any preset into the store under a new id and records
  `derivedFromPresetId` and `sourceRevision`.
- **Rename** replaces the user preset and advances its `revision`.
- **Delete** removes one user file.
- A store written by a newer version (`user-presets.json` version above 1) is
  neither listed nor written.
- A damaged file is reported and skipped; it never hides the other presets.

## Built-in catalog

Panel Presets, in order: `obsidian-glass-dock`, `metallic-shelf-dock`,
`sci-fi-chassis-blue`, `sci-fi-chassis-red`, `sci-fi-chassis-dark`,
`energy-frame-cyan`, `energy-frame-green`, `energy-frame-orange`,
`energy-frame-purple`, `minimal-neon-rail`, `mechanical-collapsible-rail`,
`circular-blue-ring`, `octagonal-platform`, `orange-arc-dock`,
`holographic-semicircle`.

Icon Presets, in order: `original-clean`, `glass-tile`, `metallic-blue`,
`metallic-red`, `neon-green`, `neon-orange`, `dark-orb`, `blue-pedestal`,
`red-pedestal`, `holographic-tile`, `minimal-glow`, `beveled-sci-fi`,
`metallic-blue-slow-turn`, `neon-green-enlarge`, `dark-orb-spiral`.

Recorded adaptations:

- `holographic-semicircle` is a procedural 2D free semicircle with no theme,
  because no installed 2.5D or 3D resource supports a semicircle. No built-in
  preset uses the `true3d` tier.
- `octagonal-platform` and `orange-arc-dock` fall back to their theme's own
  procedural tier and then to the procedural surface. `holographic-ring`
  supports only a ring, so it is the fallback theme of `circular-blue-ring`
  alone.

## Backend and Studio

`PresetLibrary` is exposed to QML as `presetLibrary`. It lists cards, reports
catalog status and performs the three user-store actions. Library browsing
does not mutate a panel. `PresetPreviewSession`, exposed as `presetAudition`,
owns desktop actions through the separate `/PresetAudition` D-Bus object and
`org.archdock.PresetAudition` interface. The existing `/Control` interface is
preserved. Card previews are described in
[shared-renderer.md](shared-renderer.md).

## Transactional desktop audition

Selecting or hovering over a card changes only the embedded shared-renderer
preview. **Preview on Desktop** explicitly starts one audition. A compatible
owned panel keeps its durable definition while an ephemeral normalized draft
drives its live renderer. Native placement and existing free-widget geometry
are captured through verified host readback before mutation.

A new panel or incompatible host/layout uses a temporary native or free host
with a unique `archdock-preview-` token. It is absent from ordinary durable
panel records. Customizations stream through the shared schema, resource and
capability validation; controls unavailable for the current draft are absent.
Theme choices are prepared from that draft, including temporary free hosts.
Edit Mode, an open panel popup or dragging prevents conflicting mutations.
An existing audition must be rolled back before a replacement begins.

| Action | Result |
| --- | --- |
| Apply as Active | Commit one valid settings revision, or verify token conversion and adopt exactly one temporary host |
| Save as Custom Preset | Save a complete normalized panel/icon snapshot with a new user ID and lineage; keep audition active without applying |
| Set as Default | Change only future creation, with independent panel and icon selections |
| Cancel / Revert | Restore the original normalized configuration and verified host state, or remove the temporary host; Revert leaves the browser open |
| Restore Built-in Defaults | Reload the immutable built-in into the current draft; commit still requires Apply |

Panel Presets preserve independently chosen icon data unless recommended icons
are explicitly requested. Icon Preset audition changes only icon style,
overrides and motion, preserving panel theme, layout, placement, visibility,
presentation and content. Built-in package bytes are never overwritten.

## Defaults and recovery

Defaults are applied through the same preset preparation before a new native
or free panel is adopted. Selecting or removing a default never rewrites
existing panels. The defaults file is version 1, limited to 4096 bytes, and
written atomically; invalid or newer stores are rejected rather than replaced.

The session states are `IDLE`, `PREPARING`, `ACTIVE`, `COMMITTING`,
`ROLLING_BACK`, `COMMITTED` and `BLOCKED`. A recovery journal, limited to
65536 bytes, records the original normalized definition, verified host state,
ownership identity and mutation/conversion progress. It is a cleanup record,
not active configuration. Startup recovery verifies ownership and either
restores the original host or removes an unadopted preview host. It never
silently commits an interrupted draft.

Failed or unverifiable restoration retains the journal and reports `BLOCKED`.
Cancel in that state requests recovery; successful rollback or commit clears
the journal. Corrupt or unsafe recovery records are retained and reported.
See [private Plasma verification](plasma-lifecycle.md#preset-audition-matrix)
for the runtime commands and the native geometry workaround.

## Tests

| Test | Covers |
|---|---|
| `preset-definition-test` | Both parsers, every rejection, round trips |
| `preset-catalog-test` | Loaders, references, user store, resolver, exact 15 + 15 audit |
| `preset-library-test` | Backend cards, fallback, store actions, all 30 cards rendered |
| `preset-browser-test` | Browser and card behaviour, keyboard, accessibility |
| `preset-staged-preview-smoke` | The same library test against a staged install |
| `preset-preview-session-test` | Preparation, independent icons, normalized custom copies, defaults, journal, all session states and failure paths |
| `panel-registry-test` | Atomic preset adoption/commit, preview separation, lineage and unregistered draft theme candidates |
| `panel-window-capability-test` | Real Studio integration, Edit Mode/popup/drag refusal and defaults preserving existing instances |
| `preset-audition-test` | Action routing, state/guard availability, Escape, labels and accessibility |
| `preset-audition-matrix-{existing,temporary,icons,recovery,defaults}` | Real owned native/free hosts, revision counts, immutable custom copies, exact rollback/crash cleanup and unrelated fixtures |
