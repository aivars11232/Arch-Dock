# Source Asset Catalog and Provenance Policy

Arch Dock keeps design references separate from installed theme packages. The
machine-readable source catalog is
`data/source-assets/source-asset-catalog.json`; the referenced PNG files and
their archive are intentionally not stored in the repository, compiled into a
resource, or installed.

The catalog records the supplied archive as observed, not as licensed artwork.
Its archive SHA-256 is
`3e7de49167666758bb569174bfd9cbe7fe03a25ca77c579e0e8858f9b9d68854`.
It contains exactly 122 panel screenshots and 16 icon reference sheets. Each
record has an independent member path, byte size, content hash, sample class,
intended use, provenance and redistribution status, target renderer tier,
orientation coverage, cleanup requirements, state-pair availability, review
status, visual groups, and review notes.

## Status and installation boundary

- `reference-only` means the sample can inform classification or future design
  work but cannot be copied into an installed theme.
- `rejected` means the sample is not a candidate for further production work.
- `production-approved` is accepted only when redistribution is explicitly
  `allowed`, the item is an isolated clean asset rather than a screenshot, and
  no manual-review, isolation, logo, or placeholder blocker remains.

An empty SPDX field, SPDX `NOASSERTION`, or `redistribution: unknown` never
implies permission. A filename, visual style, apparent brand, discovery in
Google Images, or similarity to another theme is not license evidence. The
source catalog has `sourceOnly: true` and
`installByDefault: false`; its validator enforces those values and currently
returns no install-eligible entries.

## Validation

`SourceAssetCatalog` rejects malformed records, duplicate IDs or members,
unsafe paths, count drift, unsupported enums, and unsupported promotion to
production status. `validateSourceRoot()` additionally verifies that every
cataloged PNG is present below a canonical source root and that its byte size
and SHA-256 still match. Extra or missing PNG members are rejected.

Normal CTest runs exercise the catalog without requiring a private user
archive. A task gate can supply `ARCHDOCK_SAMPLE_ARCHIVE` and
`ARCHDOCK_SAMPLE_ROOT` to verify the external archive and an extracted,
disposable source tree. Extraction is analysis-only and never an installation
step.

## Review lifecycle

TASK-0026 Phase E visually reviewed every supplied member and recorded the
visual class, candidate use, renderer tier, orientations, state-pair result,
cleanup detail, searchable groups, and an item-specific review note. The
reviewed class distribution is:

- A — desktop/product references: 9
- B — strong horizontal 2D candidates: 8
- C — legacy dock/shelf references: 27
- D — energy/glow frame candidates: 28
- E — circular launcher references: 8
- F — ring/polygon platform candidates: 30
- G — arc/freeform platform candidates: 12
- H — icon-style reference sheets: 16

The catalog exposes stable `strong-2d`, `energy`, `ring-polygon`, `arc`, and
`icon-reference` group IDs. All 138 records remain `reference-only`, retain
unknown redistribution status, and require manual review, isolated clean
assets, and alpha-mask review. Every supplied image is recorded as an opaque
screenshot; none is production-ready or installable. Visual review may narrow
or reject an item, but it cannot invent authorship, license, or redistribution
evidence.

## TASK-0027 clean-room chassis boundary

Samples 10, 12, and 13 have explicit SPDX `NOASSERTION` metadata because their
third-party creators, authoritative origins, licenses, derivative-work rights,
and redistribution rights remain unknown. Google Images was only the discovery
venue. User cropping did not create a new chain of title. These three records
remain opaque, `reference-only`, redistribution-unknown, and non-installable.
The exact declaration is retained in
`assets/source-samples/chassis/REFERENCE_RIGHTS.md`.

TASK-0027 production themes are separate, original Arch Dock package assets:

- `sci-fi-chassis-dark`
- `sci-fi-chassis-red`
- `sci-fi-chassis-blue`

They were drawn from blank SVG canvases using only broad concepts such as dark
futuristic surfaces, accent lighting, beveled framing, and general
science-fiction styling. Their deterministic recipe marks all reference hashes
as excluded, broad-concept-only references with `pixelInput: false`. Package
production records, asset hashes, alpha/bounds checks, fixed-cap stretch tests,
and visual-review records establish the independent production boundary. No
reference screenshot, crop, logo, text, application glyph, or source pixel is
contained in or installed with those packages.

## TASK-0028 clean-room energy-family boundary

Four class-D screenshots were selected as broad-concept references only. They
remain opaque, redistribution-unknown, `reference-only`, and non-installable:

| Production palette | Catalog ID | SHA-256 |
| --- | --- | --- |
| Cyan/blue | `panel-screenshot-20260811-193105` | `ce30234a578093e5a943c6c2045ff8025c952da4cc08ec2568d2ac445397b427` |
| Green | `panel-screenshot-20260811-192733` | `638662753e315b59ffe295d640699c207bf834d233a82422497ef48794596a2c` |
| Orange/red | `panel-screenshot-20260811-192943` | `a9aab4f4612cf4f1ec7bee06a1c99b6e7fec8a7698f978f62e8979b229ed9588` |
| Purple | `panel-screenshot-20260811-193642` | `bd3268a2607f04a10bff2a606817ae67c8800bc43006ece749e6ca88f51d0e57` |

The exact rights declaration and deterministic blank-canvas recipe are
retained in `assets/source-samples/energy/REFERENCE_RIGHTS.md` and
`assets/source-samples/energy/original-artwork-recipe.json`. The recipe marks
each reference hash as excluded with `pixelInput: false`; no screenshot was
copied, cropped, traced, recolored, segmented, or processed into production
artwork.

TASK-0028 ships four separate original Arch Dock packages:

- `energy-frame-cyan`
- `energy-frame-green`
- `energy-frame-orange`
- `energy-frame-purple`

Each package records its generated SVG outputs, deterministic hashes, fixed-cap
and animation-safe bounds, `sourceDerivative: false`, `pixelInput: false`, and
SPDX `NOASSERTION`. The shared four-size review contact sheet has SHA-256
`0a9bc4a12cbfe59e858e37c50ae36e65783f34b7ac2ef4ba83c2ab8f49459ea7`.
Neither that review artifact nor any reference screenshot is installed.

## TASK-0034 clean-room perspective-family boundary

The 42 class-F ring/polygon and class-G arc screenshots numbered 81 through 122
were reviewed in TASK-0026 as baked 2.5D candidates. All 42 remain opaque,
redistribution-unknown, `reference-only` and non-installable. Three were
selected as broad-concept references only:

| Production family | Catalog ID | SHA-256 |
| --- | --- | --- |
| Ring | `panel-screenshot-20260811-195427` | `20639a8a6f26462e876f357fa8d60d77705ae9aabcb28e3d2b06b93f94d4cd1a` |
| Octagon | `panel-screenshot-20260811-200136` | `c35ea87ab7332c36649c9683f5777707fcfe2fc637565f7cf530d5f6ef2366e2` |
| Arc | `panel-screenshot-20260811-200511` | `4f50a2b8f5998f27d9792edeac96a8603225c0ceee0bea7360937e8b62b62a3e` |

The exact rights declaration and deterministic blank-canvas recipe are retained
in `assets/source-samples/perspective/REFERENCE_RIGHTS.md` and
`assets/source-samples/perspective/original-artwork-recipe.json`. The recipe
marks each reference hash as excluded with `pixelInput: false`; no screenshot
was copied, cropped, traced, recolored, segmented, or processed into production
artwork.

TASK-0034 ships three separate original Arch Dock packages:

- `ring-platform-blue`
- `octagon-platform-steel`
- `arc-platform-orange`

Each is a perspective platform drawn as separable rear, foreground rim, shadow,
reflection and neutral-glow layers, with an input mask that matches the drawn
silhouette. Each records its generated SVG outputs, deterministic hashes,
`sourceDerivative: false`, `pixelInput: false`, `placeholderIcons: false` and
SPDX `NOASSERTION`. The shared four-size review contact sheet has SHA-256
`d3b10f5ec9ef7affa55beb1d7c9dfd0a3cb510f65856f451a108fbfeec4e8740`. Neither
that review artifact nor any reference screenshot is installed.

Because these are baked 2.5D packages, none of them draws a placeholder icon:
real application icons are positioned by the declared track instead. That is
recorded as an explicit prohibition in the recipe and asserted by
`baked-25d-asset-test`.

Future clean artwork must enter as a new content-hashed source record or a
separately validated production package with its own provenance evidence. It
must not silently inherit approval from a screenshot that inspired it.
