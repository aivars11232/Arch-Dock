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
  the light's sheen enough? Answered 2026-10-19: "They should mirror it
  yes, but also the reflection should move same time when icon moved,
  that includes scrolling and animations" - done in the addition below.
- Should the magma's crust plates break apart and drift separately over
  the lava (a heavier effect), or is the heaving, creeping crust enough?

### Known limitations

In `docs/KNOWN_LIMITATIONS.md`: no waves in the band; the water mirrors a
plain sky, not the desktop; the water and the lava flow one way along the
band; the crust drifts with the lava as one texture.

## Report 2 - fresh-clone recheck

A fresh clone of origin/main at `52fda6a` (the piece 6 commit), configured
and built with one job (1443 s), the focused tests run serially with the
PySide6 test environment on PATH, and every committed file compared with
the workspace (`tools/piece6-recheck.sh` in the audit folder; evidence
`build-codex-adrep/evidence/OWNER-LIST-PIECE-6/40`-`44-recheck-*`): 26 of
26 tests passed (panel-model, profile-store, preset-library,
theme-asset-processor, panel-settings-schema, renderer-capability-software,
panel-settings-transaction, panel-registry, panel-window-capability (407 s),
icon-tiles, layout-engine-visual, platform-geometry, panel-materials,
panel-scene, live-panel-preview, icon-scene, rendering-import-smoke,
studio-truth-matrix-smoke, runtime-ui-interaction-smoke,
studio-preview-contract, profile-matrix-apply, studio-navigation,
studio-themes, settings-editor-model, configuration-upgrade,
session-startup-runtime); 688 files compared, 0 differing. The first run of
this recheck on 2026-10-19 was stopped at the owner's request mid-build and
rerun whole on 2026-10-19.

Piece 6 is closed. Next: the water mirrors the icons (the owner's answer
to the question above), then Phase 4, real 3D icons with their animations.

## Addition: the water mirrors the icons

Owner's answer (2026-10-19) to the question above: "They should mirror it
yes, but also the reflection should move same time when icon moved, that
includes scrolling and animations"; and (2026-10-19): "The icon reflection
should be like in real life, it's fainter than the real object it
reflects". Run ADREP-style as piece 6's addition: implement, focused tests,
the complete serial suite, this report, one commit on main and push, a
fresh-clone recheck built with one job, Report 2.

### Report 1 - implementation

- **In 3D the water mirrors the icons standing on it.** Each icon's own
  picture appears again under it on the water, upside down, where its
  mirror image in the band's plane lies, fainter and darker than the icon
  itself, wobbled by the ripples. The mirror images are bound to the
  icons' positions and to the camera's angles, so they move at once when
  the icons scroll, animate or the panel turns.
- Only the icons' pictures are mirrored, not their stands or tiles; the
  magma mirrors nothing (dull crust); flat panels mirror nothing, since
  their icons lie on the band and there is no water beside them.
- Seen straight on, an icon's mirror image lies mostly behind the icon
  itself, as it does in real water; it shows when the panel is tilted so
  one looks across the water (sheets 06 and 07; the owner, 2026-10-19,
  ticked the low-view pictures: "I marked a good ones").
- How it works (for the code): see `docs/CURRENT_STATE.md`, piece 6. A
  second 3D view drawing the icons from the camera's mirror image was
  built first and works as a picture, but any second 3D view inside the
  scene, even a bare one, crashes the scene's teardown when a panel goes
  from 3D to flat (the Studio's "Platform presentation" switch), so the
  mirror images are flat copies of the icons' pictures projected onto a
  sheet instead, which needs no second view.

### Verification

Evidence in `build-codex-adrep/evidence/OWNER-LIST-PIECE-6/` (logs 50-82;
sheets 06 and 07 in `sheets/`; desktop pictures in
`/mnt/F/archdock-audit-2026-10-08/evidence/piece7-mirror-2/`).

- **Focused tests** (`70-mirror-focused-tests.log`): 7/7 passed:
  panel-settings-schema-test, panel-window-capability-test (550 s; its
  mesh editor check turns a river panel from 3D to flat, which crashed
  with the second-view design and passes with the sheet),
  layout-engine-visual-test, panel-materials-test, panel-scene-test,
  rendering-import-smoke (the renderer test checks the sheet holds a
  sprite per icon and that the water seen from low over it changes when
  the mirrors are taken away; pictures `scene/mirror-river.png`,
  `scene/mirror-off.png`, `scene/mirror-view.png`), and
  studio-truth-matrix-smoke. The smoke ran again after the last change
  (sampling by projection) and passed (`79-mirror-smoke-projected.log`).
  The way here is in logs 50-69 (the second view: empty texture, then a
  stale first frame, then the teardown crash; experiments E1-E6 isolated
  the crash to any second 3D view in the scene) and 74-78 (the desktop
  showed nothing until a probe in a private copy of the scene
  (`mirror-probe.py` in the audit tools) showed the sheet full and the
  sampling off: the high quality's supersampling).
- **Complete serial suite on the final code**
  (`81-complete-suite-mirror-final.log`, 2797 s, with the PySide6 test
  environment on PATH): 120 of 121 passed; `profile-matrix-apply` failed
  with "Isolated Plasma lifecycle session failed" (a private session that
  did not come up, the harness's known flake) and passed alone
  (`82-profile-matrix-apply-rerun.log`, 19 s). An earlier complete run on
  the sheet before the projection fix (`72`) was 120/121 with the
  Wayland hardening scale150 session failing once and passing alone
  (`73`). So every one of the 121 tests passed in these runs.
- **Pictures**: sheet 06, the river before (no mirror images) and
  mirroring the icons, the icons scrolling with their mirror images
  moving along, a moment later, and the low view across the water; sheet
  07, close: straight on the mirror image lies behind the icon, from the
  low view the LCL icon's mirror image lies under the star, faint and
  upside down. The owner ticked the low-view pictures.

### Report 2 - fresh-clone recheck

A fresh clone of origin/main at `40d905b` (the addition's commit),
configured and built with one job (1600 s), the focused tests run serially
with the PySide6 test environment on PATH, and every committed file
compared with the workspace (`tools/piece6-addition-recheck.sh` in the
audit folder; evidence `45`-`49-addition-recheck-*`): 25 of 26 tests
passed on the first run and 688 files compared, 0 differing. The 26th,
`session-startup-runtime-test`, failed in 0.8 s three times in the clone
while passing in the workspace build: its private KWin crashed before
creating its socket, because the clone's longer folder name
(`recheck-piece6-addition`) pushed the socket's path to 118 characters,
past the 108 a unix socket path may have (`48b`, `48c`). The clone
reconfigured with a short temp folder for its sessions (`48d`) passes it
(`48e`, 39 s); the script now uses a short folder. So all 26 pass.

The addition is closed. Next: Phase 4, real 3D icons with their
animations.
