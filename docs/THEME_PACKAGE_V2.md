# Arch Dock Theme Package Version 2

This document is the normative contract for `org.archdock.theme` manifests with
`version` equal to `2`. The words **MUST**, **MUST NOT**, **SHOULD**, and **MAY**
are requirements in this document. Version 1 compatibility is defined in
[Theme Packages](theme-packages.md).

Theme packages describe appearance and capability inputs. They do not execute
code, authorize a renderer, or prove that an asset may be redistributed. The
runtime capability resolver remains authoritative about which renderer and
controls are actually available on the current host.

## 1. Package boundary

A package is a directory containing one manifest named
`archdock-theme.json` and zero or more declared assets below that directory.
Only the manifest and assets it declares belong to the package.

```text
example-theme/
├── archdock-theme.json
├── assets/
│   ├── panel.png
│   └── glow.png
├── masks/
│   └── input.png
├── meshes/
│   └── panel.gltf
└── materials/
    └── panel-material.json
```

The directory supplied by a user is a source package. A package becomes an
installed package only after validation and a non-destructive managed copy.
Source samples, contact sheets, provenance catalogs, processing work files,
and rejected assets MUST NOT be copied into an installed package implicitly.

### 1.1 Path safety

Every path in a manifest MUST:

- be a non-empty relative path using `/` separators;
- be lexically clean and contain no empty, `.` or `..` component;
- contain no NUL, backslash, drive prefix, URL scheme, or leading `/`;
- resolve to a regular file below the canonical package directory;
- remain below that directory after every symbolic-link resolution; and
- name a file declared in `assets` before another manifest object references it.

The validator MUST reject an unsafe path before copying, decoding, hashing, or
using the target. It MUST validate the managed copy again before activation.
External URLs, data URLs, executable files, scripts, plugins, delegates, and
commands are not package assets and MUST NOT be executed.

## 2. Resource limits

The following limits are part of version 2 and are evaluated before use:

| Resource | Limit |
| --- | ---: |
| Manifest bytes | 262,144 |
| UTF-8 string bytes | 4,096 |
| Identifier bytes | 64 |
| Declared assets | 128 |
| States | 32 |
| Layers | 128 |
| Slices | 64 |
| Content regions | 64 |
| Input masks | 64 |
| One asset | 67,108,864 bytes |
| All declared assets | 268,435,456 bytes |
| Raster width or height | 16,384 pixels |
| Decoded raster area | 16,777,216 pixels |

All numeric values MUST be finite. Counts, dimensions, margins, insets, and
rectangle sizes MUST be non-negative. A rectangle tied to an asset with a
declared `naturalSize` MUST fit inside that size. Implementations MAY reject a
package at a lower operating-system resource limit, but MUST report a
structured diagnostic rather than partially installing it.

## 3. Top-level manifest

Unknown members are rejected. This makes misspellings and unsupported future
semantics fail closed.

| Member | Type | Required | Default |
| --- | --- | --- | --- |
| `format` | string | yes | exactly `org.archdock.theme` |
| `version` | integer | yes | exactly `2` |
| `id` | string | yes | none |
| `name` | string | yes | none |
| `packageRevision` | integer | no | `1` |
| `author` | string | no | empty |
| `description` | string | no | empty |
| `license` | object | no | unknown; not redistributable |
| `capabilities` | object | yes | none |
| `assets` | array | no | empty |
| `states` | array | no | one implicit `normal` state |
| `layers` | array | no | empty |
| `slices` | array | no | empty |
| `contentRegions` | array | no | empty |
| `effectMargins` | object | no | all zero |
| `inputMasks` | array | no | empty |
| `iconStyleRef` | object | no | absent |
| `animationProfileRefs` | array | no | empty |

Identifiers MUST match `^[a-z0-9][a-z0-9._-]{0,63}$`. IDs are compared
case-sensitively and MUST be unique within their namespace.

### 3.1 License metadata

`license` MAY contain:

```json
{
  "spdx": "",
  "redistribution": "unknown",
  "evidence": ""
}
```

`redistribution` is one of `unknown`, `allowed`, or `forbidden`. An empty or
missing SPDX value never implies permission. Arch Dock MUST NOT infer license
or redistribution rights from a filename, appearance, metadata tag, or theme
category.

## 4. Capabilities

`capabilities` declares what the package can describe. It does not make an
uninstalled renderer available.

```json
{
  "hosts": ["native-edge", "free-desktop"],
  "rendererTiers": ["skinned2d", "procedural2d"],
  "preferredRendererTier": "skinned2d",
  "fallbackRendererTiers": ["procedural2d"],
  "layouts": ["adaptive", "horizontal", "vertical"],
  "orientations": ["horizontal", "vertical"],
  "features": ["dynamic-tint", "icon-state-styling"],
  "presentationMechanisms": ["split"],
  "rotation": {
    "mode": "bounded",
    "minimumDegrees": -90,
    "maximumDegrees": 90
  }
}
```

Required arrays MUST be non-empty and contain no duplicate value.

- `hosts`: `native-edge`, `free-desktop`.
- `rendererTiers`: `procedural2d`, `skinned2d`, `baked2.5d`, `true3d`.
- `preferredRendererTier`: one value present in `rendererTiers`.
- `fallbackRendererTiers`: unique values present in `rendererTiers`, in the
  order in which they are to be attempted.
- `layouts`: `adaptive`, `horizontal`, `vertical`, `diagonal`, `circular`,
  `ellipse`, `ring`, `radial`, `arc`, `semicircle`, `fan`, `spiral`, `ribbon`,
  `vertical-curve`, `horizontal-curve`, `polygon`, `triangle`, `square`,
  `pentagon`, `hexagon`, `octagon`, `star`, `grid`, or `floating`.
- `orientations`: `horizontal`, `vertical`, `free`.
- `features`: known capability IDs from the Arch Dock capability resolver.
- `presentationMechanisms`: `open`, `collapse-horizontal`,
  `collapse-vertical`, `collapse-radial`, `split`, or `shutter`.
- `rotation.mode`: `none`, `bounded`, or `free`. Bounded rotation requires a
  finite minimum not greater than its finite maximum.

A procedural theme MAY have no assets. `skinned2d` and `baked2.5d` require at
least one raster or vector surface asset. `true3d` requires a declared mesh
asset and MUST NOT be satisfied by a flat raster or vector image. A package may
declare true-3D capability with a 2D fallback without making true 3D mandatory.

## 5. Assets

Each asset object has this form:

```json
{
  "id": "panel-surface",
  "path": "assets/panel.png",
  "kind": "raster",
  "mimeType": "image/png",
  "sha256": "",
  "naturalSize": {"width": 1200, "height": 160}
}
```

| Member | Type | Required | Default |
| --- | --- | --- | --- |
| `id` | identifier | yes | none |
| `path` | safe relative path | yes | none |
| `kind` | enum | yes | none |
| `mimeType` | string | no | detected conservatively |
| `sha256` | 64 lowercase hexadecimal characters | no | computed at intake |
| `naturalSize` | size object | raster/vector only | decoded/intrinsic size |

Asset kinds are `raster`, `vector`, `mask`, `mesh`, `material`, and
`animation-data`. An extension or MIME label does not override detected content
or turn a raster into a mesh. A supplied digest MUST match the file bytes.

## 6. States, layers, and split parts

States make open/collapsed/hover and other visual variants explicit:

```json
{
  "id": "open",
  "inherits": "normal",
  "layers": ["left-cap", "center", "right-cap"]
}
```

`id` and `layers` are required. `inherits` is optional and MUST reference an
earlier state without forming a cycle. Standard IDs are `normal`, `hover`,
`active`, `urgent`, `open`, `collapsed`, `minimized`, `drop`, and `edit`;
package-specific IDs may use the normal identifier grammar.

Each layer declares one asset and an explicit role:

```json
{
  "id": "left-cap",
  "asset": "panel-surface",
  "role": "split-start",
  "sourceRect": {"x": 0, "y": 0, "width": 80, "height": 160},
  "opacity": 1.0,
  "blendMode": "source-over"
}
```

Layer roles are `surface`, `split-start`, `split-center`, `split-end`, `glow`,
`overlay`, `shadow`, `reflection`, `mask`, `mesh`, and `material`. Opacity is in
the inclusive range `0.0` through `1.0`. Supported blend modes are
`source-over`, `multiply`, `screen`, and `add`. References MUST exist and role
and asset kind MUST be compatible.

## 7. Slices and geometry metadata

A slice describes fixed ends and a repeatable or stretchable center. It is
metadata for a renderer; TASK-0026 does not implement production skin drawing.

```json
{
  "id": "horizontal-open",
  "asset": "panel-surface",
  "state": "open",
  "orientation": "horizontal",
  "sourceRect": {"x": 0, "y": 0, "width": 1200, "height": 160},
  "fixedStart": 80,
  "fixedEnd": 80,
  "centerMode": "stretch"
}
```

`centerMode` is `stretch` or `tile`. Fixed ends MUST fit within the relevant
source dimension. This contract deliberately maps cleanly to a later KDE
frame/slice renderer without requiring that renderer in TASK-0026.

Content regions describe where real icons and panel content may be placed:

```json
{
  "id": "primary",
  "state": "open",
  "orientation": "horizontal",
  "shape": "rect",
  "rect": {"x": 96, "y": 24, "width": 1008, "height": 112},
  "baseline": 104
}
```

`shape` is `rect` or `path`. Version 2 accepts rectangle geometry directly;
path geometry references a declared mask asset rather than executable path
code. `baseline` is optional and MUST fit within the region.

`effectMargins` has finite, non-negative `left`, `top`, `right`, and `bottom`
numbers. These margins expand visual bounds only; they never expand logical
pointer targets automatically.

Input masks declare an asset, state, orientation, and `threshold` from `0.0` to
`1.0`. The mask affects input only when the selected host and renderer report
non-rectangular input support. Otherwise input remains the safe rectangular
region and the fallback status records the limitation.

## 8. Referenced icon styles and animation profiles

An optional `iconStyleRef` has a required `id` and an optional safe relative
`manifest` path. It identifies an icon-style resource; it does not embed an
icon preset or replace application icons implicitly.

`animationProfileRefs` is an array of objects with a required `id`, optional
safe relative `manifest`, and optional `states` array. Reduced-motion policy is
always enforced by the host. A theme cannot disable it.

## 9. Diagnostics

Validation returns an ordered list. Each entry contains:

```json
{
  "code": "unsafe-path",
  "jsonPointer": "/assets/0/path",
  "severity": "error",
  "message": "asset path escapes the package directory"
}
```

Stable error codes are:

- `invalid-json`, `manifest-too-large`, `unsupported-format`,
  `unsupported-version`, `missing-field`, `unknown-field`, `invalid-type`,
  `invalid-id`, `invalid-enum`, `invalid-value`, `limit-exceeded`,
  `duplicate-id`, `duplicate-value`, `invalid-reference`, `reference-cycle`,
  `unsafe-package-root`, `unsafe-path`, `missing-asset`, `asset-not-regular`,
  `asset-too-large`, `asset-hash-mismatch`, `unsupported-asset-format`,
  `invalid-bounds`, and `renderer-asset-mismatch`.

Validation MUST be deterministic for identical manifest bytes and package
contents. Diagnostics are ordered by manifest traversal and then path.

## 10. Fallback and installation behavior

1. Validate the source manifest and every referenced asset without mutating it.
2. Reject the complete candidate on any error diagnostic.
3. Copy only declared files into a new managed temporary directory.
4. Validate the managed copy again.
5. Atomically publish the managed directory under an ID plus content digest.
6. Resolve host, layout, controls, and renderer availability from the validated
   ThemeDefinition.
7. Attempt the preferred renderer and then declared fallbacks in order.
8. Use `procedural2d` only when it is declared as a safe fallback or when an
   invalid/missing persisted theme must fail closed to the built-in safe
   surface.

A failed new import does not replace a previously valid active theme. A missing
or invalid persisted package does not remain active and must expose a structured
fallback reason. Loading a package never makes an unavailable renderer
available.

## 11. Version 1 adapter

A valid version 1 package is represented in memory as a version 2
ThemeDefinition with:

- its normalized v1 ID/name/author;
- one raster or vector asset for `surface.asset`;
- one `normal` state and one `surface` layer;
- `skinned2d` as the preferred tier;
- `procedural2d` as its only fallback;
- native-edge and free-desktop hosts;
- adaptive, horizontal, and vertical layouts;
- horizontal and vertical orientations; and
- no split, 2.5D, true-3D, non-rectangular-input, or animation capability.

The adapter does not rewrite the source manifest, infer capabilities from its
extension, or claim that TASK-0026 supplies a production skinned renderer.

## 12. Complete example

```json
{
  "format": "org.archdock.theme",
  "version": 2,
  "id": "example-split-skin",
  "name": "Example Split Skin",
  "packageRevision": 1,
  "capabilities": {
    "hosts": ["native-edge", "free-desktop"],
    "rendererTiers": ["skinned2d", "procedural2d"],
    "preferredRendererTier": "skinned2d",
    "fallbackRendererTiers": ["procedural2d"],
    "layouts": ["adaptive", "horizontal", "vertical"],
    "orientations": ["horizontal", "vertical"],
    "features": ["dynamic-tint", "icon-state-styling"],
    "presentationMechanisms": ["split"],
    "rotation": {"mode": "none"}
  },
  "assets": [
    {
      "id": "surface",
      "path": "assets/surface.svg",
      "kind": "vector",
      "mimeType": "image/svg+xml",
      "naturalSize": {"width": 1200, "height": 160}
    }
  ],
  "states": [
    {"id": "normal", "layers": ["base"]},
    {"id": "open", "inherits": "normal", "layers": ["base"]},
    {"id": "collapsed", "inherits": "normal", "layers": ["base"]}
  ],
  "layers": [
    {
      "id": "base",
      "asset": "surface",
      "role": "surface",
      "sourceRect": {"x": 0, "y": 0, "width": 1200, "height": 160},
      "opacity": 1.0,
      "blendMode": "source-over"
    }
  ],
  "slices": [
    {
      "id": "main",
      "asset": "surface",
      "state": "normal",
      "orientation": "horizontal",
      "sourceRect": {"x": 0, "y": 0, "width": 1200, "height": 160},
      "fixedStart": 80,
      "fixedEnd": 80,
      "centerMode": "stretch"
    }
  ],
  "contentRegions": [
    {
      "id": "primary",
      "state": "normal",
      "orientation": "horizontal",
      "shape": "rect",
      "rect": {"x": 96, "y": 24, "width": 1008, "height": 112},
      "baseline": 104
    }
  ],
  "effectMargins": {"left": 12, "top": 12, "right": 12, "bottom": 20},
  "inputMasks": [],
  "animationProfileRefs": []
}
```
