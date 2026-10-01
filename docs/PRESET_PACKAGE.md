# Arch Dock preset definitions

This document describes the Panel Preset and Icon Preset formats introduced by
TASK-0040, where they live, and the rules the loaders enforce. The normative
product requirements are in [PRESET_SYSTEM_SPEC.md](PRESET_SYSTEM_SPEC.md).
Live desktop audition, Apply and defaults belong to TASK-0041 and are not
described here.

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
catalog status and performs the three user-store actions. It adds no D-Bus
method and no panel mutation. Card previews are described in
[shared-renderer.md](shared-renderer.md).

## Tests

| Test | Covers |
|---|---|
| `preset-definition-test` | Both parsers, every rejection, round trips |
| `preset-catalog-test` | Loaders, references, user store, resolver, exact 15 + 15 audit |
| `preset-library-test` | Backend cards, fallback, store actions, all 30 cards rendered |
| `preset-browser-test` | Browser and card behaviour, keyboard, accessibility |
| `preset-staged-preview-smoke` | The same library test against a staged install |
