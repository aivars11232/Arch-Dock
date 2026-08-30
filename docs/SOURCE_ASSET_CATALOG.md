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

An empty SPDX field or `redistribution: unknown` never implies permission. A
filename, visual style, apparent brand, or similarity to another theme is not
license evidence. The source catalog has `sourceOnly: true` and
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

Future clean artwork must enter as a new content-hashed record with its own
provenance evidence. It must not silently inherit approval from the screenshot
that inspired it.
