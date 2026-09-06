# TASK-0034 clean-room artwork boundary

Recorded: 2026-09-06

## Reference screenshots

The user-supplied panel screenshots numbered 81 through 122 were reviewed in
TASK-0026 as ring, polygon and arc platform candidates for a future baked 2.5D
renderer. All 42 remain opaque screenshots with unknown authorship and unknown
redistribution rights. Three were selected as broad-concept references only:

| Production family | Catalog ID | SHA-256 |
| --- | --- | --- |
| Ring | `panel-screenshot-20260811-195427` | `20639a8a6f26462e876f357fa8d60d77705ae9aabcb28e3d2b06b93f94d4cd1a` |
| Octagon | `panel-screenshot-20260811-200136` | `c35ea87ab7332c36649c9683f5777707fcfe2fc637565f7cf530d5f6ef2366e2` |
| Arc | `panel-screenshot-20260811-200511` | `4f50a2b8f5998f27d9792edeac96a8603225c0ceee0bea7360937e8b62b62a3e` |

- Creator or rights holder: unknown third-party creator or creators.
- Source: user-supplied reference archive.
- License: not established; SPDX `NOASSERTION`.
- Redistribution: unknown and therefore not permitted by Arch Dock.
- Installation: prohibited. No screenshot or screenshot pixel may enter an
  installed theme package.

Arch Dock must not copy, trace, crop, extract, recolor, segment, package, or
redistribute any screenshot content, third-party artwork, logo, glyph, text,
branding, or distinctive individual design from this source set.

The referenced PNG files are deliberately absent from the repository. Their
recorded hashes exist so a later audit can prove which items were consulted and
that none of them was used as pixel input.

## Authorized clean-room production artwork

For TASK-0034 the user approved the read-only implementation plan, which stated
that the ring, octagon and arc families would be produced as new original Arch
Dock artwork drawn deterministically from blank canvases rather than derived
from the screenshots. Only broad concepts may inform the brief: a perspective
ring platform, a bevelled octagonal slab, an open arc shelf, a raised front
rim, and separable rear/foreground/shadow/reflection/glow layers.

Production assets must:

- contain no pixels extracted from a reference screenshot;
- use original geometry and ornament created for Arch Dock;
- contain no third-party branding, text, application UI, or icon glyphs;
- avoid reproducing any distinctive individual reference design;
- keep rear, foreground, shadow, reflection, glow and input layers separable;
- carry placeholder-free artwork: no drawn stand-in icons of any kind, because
  real application icons are placed on the declared track instead;
- retain deterministic sources, hashes, bounds, and visual-review evidence.

The user authorized redistribution of the newly created original assets as part
of Arch Dock. No general public license was selected, so their SPDX value is
`NOASSERTION`; this authorization does not change the reference screenshots'
unknown and non-redistributable status.
