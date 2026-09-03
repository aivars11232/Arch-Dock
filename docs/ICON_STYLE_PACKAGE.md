# Arch Dock icon-style package format

Status: version 1 production contract for TASK-0029.

An icon style is a reusable visual resource. It is not a panel theme, an icon
preset, an animation profile, or a replacement for the system-wide icon theme.
Selecting a panel theme does not implicitly replace the selected icon style.

## Package layout

An installed package is rooted below `share/arch-dock/icon-styles/<id>` and has
one manifest named `archdock-icon-style.json`. Optional files are relative to
that package root.

```text
<id>/
├── archdock-icon-style.json
├── assets/
└── metadata/
    ├── production-record.json
    └── visual-review.json
```

Built-in package discovery is driven by
`share/arch-dock/icon-styles/builtin-icon-styles.json`. Catalog entries declare
an ID, display metadata, a safe relative package manifest, capabilities, and a
deterministic preview input. Catalog identity and package identity must match.

## Manifest identity

The root object declares:

- `format`: exactly `org.archdock.icon-style`;
- `version`: exactly `1`;
- `id`: a lower-case stable ID using letters, numbers, dots and hyphens;
- non-empty `name`;
- optional `description`, `author`, `revision`, and `license` metadata;
- `glyphPolicy`, `safeGlyphInset`, `layers`, `states`, `capabilities`,
  `preview`, and optional `mappedReplacements` and `threeD`.

Unknown extension keys are retained only under the explicit `extensions`
object. Executable package code, scripts and plugins are forbidden.

## Glyph policy

`glyphPolicy.mode` is one of:

- `original`: render the application's normal glyph unchanged;
- `tinted`: apply a declared tint only when the glyph is declared compatible;
- `monochrome`: use a declared monochrome-compatible glyph treatment;
- `mapped-replacement`: replace a glyph only when an exact stable application
  identity exists in `mappedReplacements`.

`original` is the default and safe fallback. An absent mapped-replacement key
never guesses an asset: the original application glyph is rendered. Colorful
application glyphs are never blindly recolored.

### Compatibility gate

`tinted` and `monochrome` both require a declared `tint`. When
`compatibleOnly` is `true` — the default — the treatment is applied only to a
glyph that is proved safe to recolor. An entry proves this by setting
`glyphCompatible` (or `symbolicGlyph`), and the symbolic icon-naming
convention (`*-symbolic`) is accepted as an equivalent declaration.

When the gate is not satisfied the renderer falls back to the unmodified
original glyph and reports the reason through `glyphTreatmentReason`:

| Reason | Meaning |
| --- | --- |
| `glyph-not-compatible` | `compatibleOnly` held and the glyph is not declared safe to recolor |
| `missing-tint` | `tinted`/`monochrome` was requested without a tint |
| `no-application-mapping` | `mapped-replacement` had no exact identity match |

`monochrome` renders through Kirigami's native icon mask path. `tinted`
instantiates a `MultiEffect` colorization only when a tint is actually
applied. Neither effect is created for the default `original` policy.

### Fail-closed rendering

Validation decode-probes every renderable layer and mapped-replacement asset,
so an undecodable file never enters the package asset table, the content
digest, or the runtime projection. If a style asset nevertheless fails at load
time, the **entire** style treatment is withdrawn and the plain original glyph
is rendered. A partially drawn style is never shown.

## Layers

`layers` contains the stable roles `rear`, `base`, and `front`. Each role is an
ordered array of bounded declarations. A declaration has a unique `id`, a
`kind` of `procedural` or `asset`, and normalized opacity. Asset declarations
use a safe package-relative `asset` path. Procedural declarations use bounded
shape, radius, inset, color, border and gradient parameters.

The application glyph and running indicator are host-owned layers between
these package layers. Styles cannot move or resize the logical pointer target.
The optional `mask`, `reflection`, `shadow`, and `glow` fields use the same
asset-or-procedural declaration rules.

`reflection`, `shadow`, and `glow` are drawn as visible layers. `mask` is not
a visible layer: an asset-backed `mask` shapes the tile composite through a
`MultiEffect` mask and leaves the application glyph and the running indicator
unclipped. A `mask` therefore never makes a style renderable on its own.

`safeGlyphInset` has finite `left`, `top`, `right`, and `bottom` values from
`0.0` through `0.45`. It constrains visual glyph placement only.

## Explicit states

Every version-1 package declares all of:

`normal`, `hover`, `pressed`, `active`, `running`, `minimized`, `urgent`,
`launching`, `drop`, `edit`, and `disabled`.

Each state contains finite, bounded layer opacity, glyph opacity/scale, border,
glow, reflection and indicator parameters. State resolution is deterministic:

```text
edit > disabled > drop > urgent > pressed > hover > launching > active
     > minimized > running > normal
```

Reduced-motion policy remains host-owned. A style declares supported animation
capability names but cannot execute code or disable reduced motion.

## Optional 3D reference

`threeD` may declare safe relative `mesh` and `material` references plus a
`fallbackStyleId`. Version 1 validates and reports these references but the 2D
renderer remains first-class. Missing or unavailable 3D support resolves to the
declared 2D package or `plain-original`; it never makes the icon disappear.

## Path and resource safety

Manifest and asset loading fails closed:

- paths are non-empty, relative, clean and contain no `..` segment;
- canonical files must stay beneath the canonical package root;
- missing, non-regular, unreadable, oversized, or escaping files are rejected;
- catalog ID, manifest ID and directory ID must agree;
- bounded counts and file sizes prevent unbounded package input;
- no executable package content is accepted.

An invalid selected package produces structured diagnostics and renders
`plain-original`. Partial package projection is forbidden.

## Selection and overrides

`PanelDefinition.iconStyle.styleReference` is the authoritative panel-default
style ID. `iconThemeId` is retained only as a legacy compatibility mirror.
Panel-theme `iconStyleRef` metadata is a recommendation; loading the panel layer
does not change the selected icon style.

Per-entry overrides are keyed by stable desktop-entry or canonical free-entry
identity. Their deterministic resolution is:

```text
validated entry override -> panel icon style/base glyph -> plain original glyph
```

An override may hold a custom glyph, custom label, tile state, style reference,
and a future animation-profile reference. Reset removes only the selected
entry's override. The animation reference is persistence-only until TASK-0030.

The durable `iconOverrides` map uses bounded, namespaced keys. Desktop entries
use `desktop.<lower-case-desktop-file-id>`, fallback application identities use
`application.<id>`, and free local URLs use `free.sha256-<canonical-url-digest>`.
The desktop-entry key takes precedence over transient Wayland/X11 application
identifiers, so pinned and running representations of one application converge
on the same record without exposing free-entry filesystem paths in map keys.

Each override record accepts only:

- `customGlyph`: a KDE icon name, absolute local path, or local `file:` URL;
- `customLabel`: the entry-local display label;
- `tileEnabled`: an optional boolean controlling tile/pedestal layers only; it
  does not disable the selected style's safe glyph inset, glyph opacity/scale,
  state resolution, or running indicator;
- `styleReference`: an exact installed icon-style ID;
- `animationProfileReference`: a validated future profile ID;
- `extensions`: an explicit forward-compatible map.

The backend commits a complete record at the expected panel settings revision.
Replacing one identity leaves every other record untouched; reset removes only
that identity. A stale revision, absent live entry, unknown style, malformed
field, or persistence/readback failure leaves the panel unchanged. A missing
local custom-glyph file remains persisted for repair but resolves to the base
glyph with `custom-glyph-unavailable`; an invalid stored style resolves through
the panel default and then `plain-original`. Existing `dock/customIcons` data is
retained as read-only base-glyph input during migration rather than being
discarded or rewritten by later dock saves.

## Live Icon Properties editor

Pinned application entries and free-panel entries expose **Icon Properties…**
from the normal live icon context menu. The action carries only the current
panel ID and the backend-issued stable entry identity. The backend resolves a
fresh entry snapshot before opening the editor; running-only or otherwise
transient entries are marked unsupported and do not expose the action.

The editor exposes only behavior implemented by this task: custom glyph,
custom label, optional tile state, and an exact installed icon-style override.
The future animation-profile field and extension data remain hidden, and an
ordinary Apply preserves them. Apply submits one complete override through the
same revision-checked atomic panel transaction as the backend API. Reset uses a
dedicated transaction that removes only this identity. Cancel and window close
discard the local draft without writing. A failed or stale transaction keeps
the draft visible with its structured error.

The context menu is closed before the editor window is requested. Edit mode,
active drag, disabled input, and unsupported identity guards prevent accidental
opening. A successful commit advances the normal dock revision, causing the
live Plasma scene to fetch the resolved entry again. Broader application menu
actions remain owned by AD-0016.

## Built-in production families and renderer behavior

Version 1 ships `metallic-blue`, `metallic-red`, `neon-green`, `neon-orange`,
and `dark-orb` in addition to the `plain-original` fail-closed fallback. These
packages are asset-free procedural recipes created from the clean-room record
under `assets/source-samples/icon-styles`; no screenshot pixels enter the
packages or staged install.

`IconStyleResolver.js` validates the backend-owned package projection and
selects the exact state using the precedence above. `IconStyle2D.qml` draws the
ordered procedural or asset layers around the real application glyph in
`IconScene.qml`. Rear/base, glyph, front, indicator, and status layers remain
independently addressable. An absent or invalid projection, or a missing exact
mapped replacement, keeps the original glyph and falls back safely.

The same `IconScene` path is used by live native/free Plasma hosts and Studio
previews. Automated visual regression records deterministic live-QML layer,
state, role-opacity, and glyph-geometry signatures for all five families and
all required states. Pixel capture is not claimed: the available offscreen Qt
backend returned blank frames.

The public interaction regression composes the production `DockEntry` and
production Icon Properties window, right-clicks the real entry pointer target,
selects the real context-menu action, types into the editor, and exercises
Apply, Cancel, window-manager close, and Reset with Qt pointer/key events. It
runs both offscreen and inside the disposable private Wayland/KWin session.
This proves the public QML interaction path and the real revision-checked
`PanelWindow` transactions; it does not claim cross-process input injection
into a user's desktop.

The staged-runtime portion independently installs the production artifacts
under a temporary prefix, starts private KWin/Plasma, verifies every production
family on both native and free hosts, and checks live-scene refresh after the
staged backend applies and resets one stable-identity override. All runtime
work is confined to the disposable session and never targets the personal
desktop.

## Provenance

Reference screenshots and collages are never installed. Production packages
must contain original Arch Dock geometry or independently redistributable
assets, deterministic production records, and visual-review evidence. A
filename, screenshot crop, search result, or visual similarity is not license
evidence.
