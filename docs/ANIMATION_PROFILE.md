# Arch Dock animation profile — version 1

An animation profile describes motion as validated data: what moves, what makes
it move, and which property tracks run. It replaces the single `iconAnimation`
string, which could only name one hardcoded effect and could not describe the
layer it affected, the events it answered, or its reduced-motion behavior.

Authoritative implementation:

- `src/model/AnimationProfile.h` / `.cpp` — the definition and its vocabularies.
- `src/animation/AnimationProfileCatalog.h` / `.cpp` — parser, validator, catalog.
- `data/animation-profiles/builtin-animation-profiles.json` — built-in catalog.
- `qml/ArchDock/Rendering/AnimationProfileRuntime.js` — composition rules.
- `qml/ArchDock/Rendering/IconMotionController.qml` — the single track runner.
- `qml/ArchDock/Rendering/MotionChannels.js` — channel-to-transform mapping.

Validation is fail-closed. A catalog containing any error loads no profiles at
all, and callers fall back to the declared `fallbackProfileId`.

## 1. Catalog document

```json
{
  "format": "org.archdock.animation-profile-catalog",
  "version": 1,
  "fallbackProfileId": "none",
  "animationProfiles": [ ... ]
}
```

Limits: 512 KiB per catalog, 256 profiles, 32 tracks per profile, 4 KiB per
string, 64 bytes per identifier. Identifiers match `^[a-z0-9][a-z0-9.-]{0,63}$`.

## 2. Profile

| Field | Required | Meaning |
|---|---|---|
| `format` | yes | `org.archdock.animation-profile` |
| `version` | yes | `1` |
| `id` | yes | Stable identifier, unique within the catalog |
| `name` | yes | Human-readable name shown in the editor |
| `description`, `author`, `category` | no | Metadata |
| `target` | yes | Default layer the tracks move (§3) |
| `trigger` | yes | The event that starts the profile (§4) |
| `timing` | yes | `baseDuration` (0–60000 ms), `speedScale` (0.05–10), `startDelay` |
| `tracks` | yes | One or more property tracks (§5) |
| `rendererRequirements` | yes | Renderer tiers the profile needs (§7) |
| `reducedMotion` | yes | Explicit reduced-motion behavior (§8) |
| `legacyNames` | no | Legacy `iconAnimation` values this profile replaces |
| `extensions` | no | Reserved object for forward compatibility |

Unknown members are rejected (`unknown-field`) rather than ignored, so a typo
cannot silently disable a setting.

## 3. Targets

`glyph`, `tile`, `icon`, `indicator`, `badge`, `panel-surface`, `panel-glow`,
`panel-content`, `panel-segment`, `free-scene`, `window-preview`.

Targets are distinct resources, not synonyms. A profile that turns the `glyph`
leaves the `tile` stationary; one that moves `icon` moves the composed visual.
A track may override the profile target, which is how one profile composes
motion across several layers.

## 4. Triggers

`idle`, `hover-enter`, `hover-hold`, `hover-exit`, `press`, `click`,
`launch-requested`, `launch-succeeded`, `launch-failed`, `running-started`,
`running-stopped`, `urgent`, `drop-entered`, `drop-committed`, `panel-reveal`,
`panel-conceal`, `panel-open`, `panel-collapse`, `profile-changed`, `command`.

**`click` and `launch-succeeded` are separate triggers and must stay separate.**
A click is an input event; a successful launch is a verified outcome. A profile
bound to `launch-succeeded` may never run merely because an icon was clicked.
The pre-migration umbrella value `launch` is not part of this vocabulary; it is
mapped to `launch-requested` by the compatibility layer.

**What counts as a verified launch.** `activateDockEntryOutcome` reports one of
three answers. `succeeded` means a program was started and the start was
confirmed by the platform. `failed` means it could not be started, or the entry
does not exist. `requested` means the activation was handed to the compositor,
which never reports back — raising an existing window is the usual case — and it
is deliberately reported as neither success nor failure, so nothing runs. The
bool-returning `activateDockEntry` keeps its original contract for existing
callers.

## 5. Tracks

| Field | Required | Meaning |
|---|---|---|
| `id` | yes | Unique within the profile |
| `target` | no | Overrides the profile target |
| `property` | yes | Primitive property (§6) |
| `from` / `to` | yes (numeric) | Endpoints, range-checked per property |
| `fromColor` / `toColor` | yes (color) | Endpoints for `tint` only |
| `easing` | no | `linear`, `in/out/in-out-quad|cubic|sine|back`, `out-bounce`, `out-elastic` |
| `duration` | yes | 0–60000 ms |
| `delay` | no | Start offset in milliseconds |
| `phase` | no | Per-entry stagger, multiplied by the entry index |
| `direction` | no | `normal`, `reverse`, `alternate` |
| `repeat` | no | `-1` continuous, otherwise 1–10000. `0` is rejected |
| `intensityScale` | no | 0–10, multiplied by the user intensity setting |
| `blend` | no | `replace` (default) or `add` |
| `priority` | no | 0–1000; higher wins under `replace` |

Numeric and color endpoints are mutually exclusive: a `tint` track using
`from`/`to`, or a numeric track using `fromColor`/`toColor`, is rejected.

## 6. Properties and ranges

| Property | Range |
|---|---|
| `translate-x`, `translate-y`, `translate-z`, `translate-tangent`, `translate-normal`, `path-radius`, `part-offset` | −10000 … 10000 |
| `scale`, `scale-x`, `scale-y` | 0 … 16 |
| `rotate-x`, `rotate-y`, `rotate-z`, `orbit`, `spiral`, `path-rotation` | −3600 … 3600 |
| `opacity`, `glow`, `blur`, `shadow-intensity`, `material-reflection`, `clip-open` | 0 … 1 |
| `shadow-offset` | −1000 … 1000 |
| `tint` | color name or `#rrggbb` |

**Paths.** `orbit` and `spiral` carry the polar *angle* in degrees;
`path-radius` is their companion *distance*, in logical units. A path motion is
therefore an angle and a radius rather than a pair of hand-synchronised X and Y
sweeps, which is what lets a profile grow or shrink its own trajectory. Both
rest at zero, so a resting entry sits exactly on its anchor: every offset is
computed from the absolute channel values and never accumulated, so no number
of cycles can leave the icon drifted away from where it started.

**Direction.** `translate-normal` moves the target *away from the panel*, and
`translate-tangent` moves it *along the run of icons*. Neither has a fixed
screen direction: `LayoutEngine` resolves them per entry, so a bottom panel
jumps up, a top panel down, a left panel right and a right panel left, while a
ring entry moves away from the ring's own centre. A linear row has no outward
side of its own, so the host edge supplies it; omit the edge and the historical
path-derived normal is used unchanged.

**Units.** Translation properties are expressed in *logical units*: `1.0` is the
target's own logical size. A `translate-y` of `-0.22` lifts an icon by 22% of
its size at any icon size, which is what the pre-migration
`baseSize * 0.22` arithmetic did. Rotations are degrees; opacity, glow, blur,
clip and reflection are normalised 0–1; scale is a multiplier.

**Timing.** A profile's durations are declared against its own
`timing.baseDuration`. The host passes `speed = baseDuration / cycleDuration`,
so the user's animation-duration setting stays authoritative and every profile
scales with it. All built-in profiles use a 170 ms baseline, matching the
`animationDuration` default.

**Intensity.** The user's motion-intensity setting scales each track's
endpoints about the property's *resting* value (0 for translations and
rotations, 1 for scale and opacity). A symmetric −14°…14° tilt therefore
narrows to −7°…7° at half intensity, and a 1 → 1.16 swell narrows to 1 → 1.08,
reproducing the pre-migration behaviour exactly.

## 7. Renderer requirements

At least one of `procedural2d`, `skinned2d`, `baked2.5d`, `true3d`. The list is
validated against the capability resolver's tier vocabulary, so a profile cannot
require a tier that does not exist. Duplicates are rejected.

## 8. Reduced motion

Every profile must declare `reducedMotion`. There is no implicit default: an
absent declaration is a `missing-reduced-motion` error.

| Mode | Meaning |
|---|---|
| `none` | The profile does not run under reduced motion; the layer rests |
| `static` | Hold one property at one value (`property` plus `value`, or `color` for `tint`) |
| `substitute` | Run another profile named by `substituteProfileId` |

A substitute must exist in the catalog and must not itself be a `substitute`,
so reduced motion always terminates in a resting state rather than a chain.

Every built-in profile that moves something declares a `static` glow hold, so
reduced motion still confirms the state the motion was expressing instead of
falling silent. Only `none`, which has no tracks, rests. The renderer takes the
glow from whichever layer declares it, so a `glyph`-targeted preset reaches the
same glow an `icon`-targeted one does.

## 9. Composition and conflicts

Two tracks may write the same property on the same target only when the outcome
is deterministic:

- both use `blend: "add"` — the contributions sum, and order does not matter; or
- their `priority` values differ — the higher priority wins.

Anything else is a write race and is rejected with `track-conflict`, pointing at
the second writer. This is what prevents two profiles from silently overwriting
each other at runtime.

## 10. Diagnostics

`unknown-field`, `missing-field`, `invalid-type`, `invalid-format`,
`unsupported-version`, `invalid-id`, `invalid-target`, `invalid-trigger`,
`invalid-property`, `invalid-easing`, `invalid-direction`, `invalid-blend`,
`invalid-color`, `value-out-of-range`, `string-too-long`, `duplicate-track`,
`too-many-tracks`, `track-conflict`, `unsupported-renderer`,
`duplicate-renderer`, `missing-reduced-motion`, `invalid-reduced-motion`,
`unknown-substitute`, `duplicate-profile`, `duplicate-legacy-name`,
`unknown-fallback`, `too-many-profiles`, `catalog-unreadable`,
`catalog-too-large`, `invalid-json`.

Each diagnostic carries a JSON pointer to the offending member.

## 11. Compatibility with legacy `iconAnimation`

Existing configurations store a single effect name. Each built-in profile lists
the legacy names it replaces in `legacyNames`, and the catalog builds a
name-to-id map from them. Resolution order:

1. the requested value is a current profile id — use it;
2. the value appears in the legacy map — use the mapped profile;
3. otherwise fall back to `fallbackProfileId` and report `fallbackApplied`.

Two profiles may not claim the same legacy name.

`animation-profile-test` asserts that the catalog's profile ids plus legacy
names are exactly the set of values the `iconAnimation` settings field offers,
in both directions. A preset cannot appear in the editor without a validated
profile, and the catalog cannot carry a profile the editor cannot select.

### Migrated effects

Every effect that previously lived as a conditional branch in `DockEntry.qml`
is now a catalog entry. `dock-entry-motion-contract-test` fails the build if a
branch on an effect name or a per-effect animation reappears there.

| Legacy `iconAnimation` | Profile id | Property | Notes |
|---|---|---|---|
| `none` | `none` | — | No tracks |
| `bounce` | `bounce` | `translate-y` | 0 → −0.22, half-cycle, out-cubic |
| `elastic` | `elastic` | `translate-y` | 0 → −0.275, out-elastic |
| `spring` | `spring` | `translate-y` | 0 → −0.22, out-back |
| `float` | `float` | `translate-y` | 0 → −0.143, in-out-sine |
| `wave` | `wave` | `translate-y` | 0 → −0.198, 45 ms per-entry stagger |
| `orbit` | `orbit` | `orbit`, `path-radius` | An exact circle of radius 0.22, same 340 ms cycle as the migrated sine pair |
| `pulse`, `scale` | `pulse` | `scale` | 1 → 1.16 |
| `breathe` | `breathe` | `scale` | 1 → 1.16 at double cycle |
| `ripple` | `ripple` | `scale` | 1 → 1.208 |
| `magnetic` | `magnetic` | `scale` | 1 → 1.256 |
| `spin` | `spin` | `rotate-z` | 0 → 360 over two cycles |
| `idle-rotate` | `idle-rotate` | `rotate-z` | 0 → 360 over eight cycles |
| `swing` | `swing` | `rotate-z` | ±14° |
| `wobble` | `wobble` | `rotate-z` | ±9° |
| `wiggle` | `wiggle` | `rotate-z` | ±6°, short cycle |
| `shake` | `shake` | `rotate-z` | ±3°, short cycle |
| `glow` | `glow` | `glow` | 0.25 → 1, now driven by the profile rather than by the renderer |

Amplitude, cycle length and stagger are preserved exactly. Two multi-leg
effects are expressed as a single alternating track with an easing curve that
carries the same shape: `elastic` uses `out-elastic` and `spring` uses
`out-back` in place of their previous hand-sequenced legs.

Each built-in profile declares `reducedMotion: {"mode": "none"}`, which
reproduces today's behaviour exactly — reduced motion currently rests every
icon effect. Richer per-preset substitutes are owned by TASK-0031.

## 12. The requested motions

Master plan 14.5 names five motions by requirement rather than by migration.
They are ordinary catalog entries; nothing about them is special-cased in a
delegate.

| Preset | Target | What it does |
|---|---|---|
| `slow-y-turn` | `glyph` | A deliberately slow left-to-right turn around the vertical axis: `rotate-y` 0 → 360 over twelve cycles. |
| `jump` | `icon` | `translate-normal` 0 → 0.28 away from the panel, with a light squash and stretch. |
| `shake-tangent` | `icon` | `translate-tangent` ±0.06 with a ±2° tilt, bounded and continuous while its trigger holds. |
| `enlarge` | `tile`, `glyph` | A depth-aware swell: the tile grows to 1.12, the glyph in front of it to 1.18. |
| `spiral` | `icon` | `spiral` 0 → 360 with `path-radius` winding out to 0.18 and back, combined with a swell and a fade. |

**The Y turn is a turn, not a spin.** `idle-rotate` and `spin` rotate the flat
image in the screen plane (`rotate-z`). `slow-y-turn` rotates the card around
its vertical axis: `MotionChannels.turnMatrix` composes a translation to the
centre, a rotation about Y, a perspective divide and a translation back, so the
card narrows towards its centre and the receding edge foreshortens. The
compression is real perspective, not a scale. No mesh is involved — AD-0015 owns
true 3D — and because the transform is visual only, the pointer target does not
move. Positive angles turn left-to-right; a derived profile reverses it with
`direction: "reverse"`.

**Bounds.** `PanelScene` publishes, per entry, how much room the theme actually
reserved on each side (`effectAllowance`, derived from the effect bounds).
`MotionChannels` sums every translation contribution and then clamps the sum
once, so no single contribution can be silently dropped and no effect can draw
outside the reserved margin. The live host additionally reserves headroom equal
to the furthest the bound profile can travel, so a jump is not clipped by the
panel it lives in.

**Enlarge and its neighbours.** `MotionChannels.magnificationInfluence` decides
how strongly a neighbour follows the hovered entry, from its distance in entry
positions. `motion.magnifyRadius` sets the reach and `motion.magnifyFalloff`
chooses `linear`, `cosine` or `gaussian`; the defaults, 2.4 and `linear`, are
the historical curve. Every falloff peaks at 1 on the hovered entry, decreases
monotonically, and is exactly 0 at and beyond the reach, so influence can never
leak across a whole panel.

This is **visual influence only**. It scales what is drawn; it never changes an
entry's logical size, position or hit area, and it never moves the layout.
Physical rearrangement is a separate capability that this release does not
enable, so there is no setting that claims to move icons and does not.

**Reduced motion.** Each requested motion substitutes a static glow rather than
resting silently, so the state it was confirming is still visible. The renderer
takes the glow from whichever layer declares it, so a glyph-targeted preset
reaches the same glow the icon-targeted ones use.

## 13. Frozen preset identifiers

Built-in Panel and Icon Presets name motion by profile id, so these 23 ids are
frozen:

```text
none          bounce      elastic     spring     float       wave
orbit         pulse       breathe     ripple     magnetic    spin
idle-rotate   swing       wobble      wiggle     shake       glow
slow-y-turn   jump        shake-tangent          enlarge     spiral
```

`animation-profile-test` asserts the shipped catalog holds exactly this set and
that every member resolves to itself rather than to the fallback. Adding an id
is a deliberate act that updates the frozen list, the `iconAnimation` schema
choices and the migration test together; renaming or removing one is a
compatibility break that would silently change a shipped preset.

`scale` remains the one legacy alias, mapping to `pulse`.

## 14. Motion safety

- **Concealment.** `PanelScene.sceneConcealed` reports that the panel cannot be
  seen; the live host derives it from its own item visibility and opacity and
  from the window's visibility, which is what a Plasma auto-hide panel changes.
  While it holds, the controller withdraws every track, so nothing animates
  off-screen. Nothing is inferred from focus: a dock stays visible while another
  window is active. The default is "visible", so a host that reports nothing
  animates rather than freezing silently.
- **Transitions.** Changing profile, catalog or reduced-motion setting tears the
  running tracks down and starts the new set from its own origin, so a swap
  mid-flight leaves a valid resting state rather than a half-applied transform.
- **Resources.** Trigger and concealment cycles return to exactly the profile's
  own track count; runners are never accumulated.

## 15. Fixtures

`tests/fixtures/animation-profile-v1/` holds one file per validator outcome,
indexed by `fixture-index.json`. `animation-profile-test` asserts every fixture
produces exactly its declared diagnostic code, so a validator regression fails
the suite rather than passing quietly.
