# Owner list, piece 6: the river and the magma

Owner's words (2026-10-15, while judging the relief): "There could even be
like tha panel is the river, with animations"; and (2026-10-18, while this
piece was being built): "Since we have water, mind as well we can add
flowing magma too". Its own piece, queued after piece 5
([OWNER-LIST-PIECE-5.md](OWNER-LIST-PIECE-5.md)), run ADREP-style:
implement, focused tests, the complete serial suite, this report, one commit
on main and push, a fresh-clone recheck built with one job, Report 2.

Methods reused (the owner, 2026-10-18: "don't try to reinvent the code ...
reuse that material"): the surface is a Qt Quick 3D custom material as
Qt's own custom-material examples build one; the ripples are the look's
relief map in two layers flowing at different speeds and bending the
light, and the sky is mirrored by Schlick's approximation of Fresnel, as
water shaders commonly do; the ripple map itself is a wind-driven wave
spectrum (Tessendorf, "Simulating Ocean Water", 2001; the Phillips
spectrum); the lava's light is the usual emissive-by-mask way of lava
shaders (a heat mask from the texel's colour drives how much of the
texel's own colour is emitted). Formulas only, no code copied.

Real material, not drawn (the owner, 2026-10-18, on seeing drawn textures:
"I mean real magma and real water, not whatever u made there ... looks more
like some sort of cybernetic plasma"; "internet is full of real magma/lava
and real water, why are u trying to reinvent it?"): the looks are composed
from ambientCG's real texture sets, CC0 1.0 (public domain), which may
ship in a GPL-3.0-or-later package: Lava003 (Magma), Lava002 (Ember),
Lava005 (Blaze), Lava004 (Molten), Gravel021 and Gravel020 (river beds);
the owner ticked their colour and glow pictures. The water over the bed is
computed from a real wave spectrum (Tessendorf, "Simulating Ocean Water",
2001). Credits in `qml/ArchDock/Rendering/materials/CREDITS.md`.

## Report 1 - implementation

### What changed for the owner

- **A River panelpaper** in the Panelpaper list (and in the stands' and
  tiles' look lists, since those lists are one): real river water flowing
  along the panel, its pebble bed showing through the shallows at the
  banks, deeper in the middle, the sky mirrored in its ripples. On the
  Colour row it has five moods: River (the default), Clear, Deep, Jade and
  Sunset.
- **A Magma panelpaper** beside it: real lava, dark ropy crust with the
  lava glowing between its plates. On the Colour row it has four heats:
  Magma (the default), Ember (darker, mostly crust), Blaze (hotter) and
  Molten (fully molten).
- **In 3D the band is water or lava that flows in a loop** (the owner,
  2026-10-19: "No need for waves just regular looped flow is enough", so
  the waves and their Wave height slider were taken out again). The water
  runs over its still pebble bed: two layers of ripples slide along at
  different speeds and bend the light, the bed wobbles under them, the
  sky is mirrored where the surface tilts toward the viewer, and the
  lights glitter on it. The lava creeps along with its crust; the crust
  stays dark and rough, the lava between the plates glows with its own
  light in a slow shimmer, and a second layer of lava shows dimly through
  the crust. **Motion speed** sets how fast each flows; lava creeps (about
  three times slower than water).
- **Flat panels** show a strip of the real surface at its natural size
  (the owner, 2026-10-19, on the first flat pictures: "it's just some
  bunch of" circles, each repeat squeezed into the band), the water with
  pale gleams sliding along the band and the lava with two broad hot
  streaks creeping along it that add their light to the crust. Both keep
  their own colours on a flat panel, as a picture does.
- Water does not glow (as a picture does not): the Glow rows are not
  shown for it, and a look whose Glow rows are hidden now draws no glow
  halo either (before, a halo set earlier kept showing round it). Magma
  does glow, so its Glow rows stay; stands and tiles wearing magma glow
  with its lava.

### How it works (for the code)

See `docs/CURRENT_STATE.md`, piece 6.

### Verification

Evidence in `build-codex-adrep/evidence/OWNER-LIST-PIECE-6/` (logs numbered
in order; sheets in `sheets/`, desktop pictures in
`/mnt/F/archdock-audit-2026-10-08/evidence/piece6-{river,magma,flat-flow}-3/`).

- **Focused tests, final code** (`16-tests-no-waves.log`): 7/7 passed:
  panel-settings-schema-test, panel-window-capability-test (554 s),
  layout-engine-visual-test, panel-materials-test (both looks paint, their
  light slides along, every look distinct under one tint),
  panel-scene-test, rendering-import-smoke (the 3D renderer test draws
  the river and the magma on a ring with the flowing-surface material and
  tells them apart; `scene/flow-river.png`, `scene/flow-magma.png`),
  studio-truth-matrix-smoke. Earlier rounds of the same tests on the way
  here: `10-quick-tests.log` (9/9), `11`-`15` (the river textures missing
  from the build, the wave-height test harness, and the custom material
  getting no texture from an item-rendered Texture were found and fixed
  there), `17-studio-pictures-3d.log` (the Studio picture function on its
  own, passed).
- **Complete serial suite** (`30-complete-suite.log`, 1603 s): 100 of 121
  passed. The 21 failures had one cause outside the code: the harness's
  Python scripts need the PySide6 test environment on PATH
  (`/mnt/F/archdock-audit-2026-10-08/venv-pyside6/bin`, as the recheck
  scripts set it; the system PySide6 was removed last month), and this run
  was started without it. Those 21 rerun with it
  (`31-suite-rerun-with-pyside-venv.log`, 1203 s): 20 of 21 passed. The
  last, `preset-audition-matrix-defaults`, failed in its own script at the
  known restart race: it crashed the service by the process id it had
  remembered, after an earlier phase had crashed the service and D-Bus had
  started it anew (`32-audition-defaults-rerun.log`, the same twice). The
  matrix script now crashes the process that owns the name
  (`tests/run-preset-audition-matrix.sh`, as the lifecycle runner stops it
  since piece 5); it passes (`33-audition-defaults-fixed.log`, 60 s). So
  every one of the 121 tests passed in these runs.
- **Pictures** (sheets 01-05): the composed textures (01, the owner:
  "Much better ... all of them"); the 3D ring before (glass) and wearing
  the river, a moment later, in its Clear and Sunset moods and edge-on
  (02); the same with the magma, Blaze and Molten (03); the flat ring
  before and wearing both, a moment later (04); the Studio's 3D page for
  the ring wearing each, from the private KWin session (05; the
  Panelpaper row with River and Magma lies further down that page, below
  the picture's edge; the page's layout is unchanged by this piece, the
  two looks are new entries in its lists and "Water"/"Lava" are the
  Colour row's names for them).
- The owner's rulings on the way: the first drawn textures were rejected
  ("cybernetic plasma"), the real sets ticked; the waves were taken out
  ("regular looped flow is enough"); the first flat pictures were rejected
  (squeezed repeats, "circles"), the natural-size strip is what shows now.

### Questions for the owner

- Should the water mirror the icons standing on it (a reflection), or is
  the light's sheen enough?
- Should the magma's crust plates break apart and drift separately over
  the lava (a heavier effect), or is the heaving, creeping crust enough?

### Known limitations

In `docs/KNOWN_LIMITATIONS.md`: no waves in the band; the water mirrors a
plain sky, not the desktop; the water and the lava flow one way along the
band; the crust drifts with the lava as one texture.
