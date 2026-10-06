# Changelog

## 0.1.1 candidate — unreleased, 2026-10-05

Application version: `0.1.1`; the current Arch package candidate and verification
status are recorded in [current state](docs/CURRENT_STATE.md).
The owner selected GPL-3.0-or-later for original Arch Dock project work.
The proposed `v0.1.1` tag and publication need separate authorization.

- The official source verifier regenerates all four canonical export artifacts
  and rejects any byte difference, including recipe commands/metadata, archive
  metadata/member ordering and manually repaired checksum receipts.
- Tracked source modes follow Git's `100644`/`100755` executable intent,
  producing `0644`/`0755` independently of checkout permission bits.
- The complete official GPLv3 text and GPL-3.0-or-later declaration are added.
  The package ships the GPL text, preserved MIT component notices and a license
  matrix; audited original asset declarations carry the owner's new selection.
- Existing MIT components retain their license. Unknown-rights reference
  material remains NOASSERTION, non-installable and excluded from distribution.
- Icon mask load failures follow their asset URL. Switching the same icon scene
  from a broken mask to a valid mask or a plain style recovers; a currently
  broken mask still uses the original glyph fallback.
- Folder contents unfold from the clicked dock icon on a transparent native
  Plasma surface. Dense fan and grid layouts fit horizontally and scroll with
  the wheel or a held-pointer drag; dragging does not open a child.
- Fan and Arc folder contents move along a compact curve with wheel or held
  dragging, without visible scrollbars or selection boxes. Reduced motion
  remains immediate. Free-panel popups choose the side with more screen space
  and fit that space before native Wayland placement.
- Appearance explains the current 2D surface and offers a route to 3D themes;
  compatible themes retain the existing 3D switch, quality and tilt controls.
- 3D icons sit on the platform in world coordinates. Rotation changes their
  perspective, depth and input bounds. Free-panel surfaces support held-pointer
  rotation as well as wheel and continuous clockwise/counterclockwise rotation.
- Orange Arc includes a volumetric ring option. Enabling 3D switches its draft
  to a circular path; the existing baked artwork remains available.
- 3D orientation, platform thickness and pedestal height join the tilt and
  layout size controls. Opacity is available for themed surfaces. A loaded
  baked surface no longer draws the procedural fallback arc over its artwork.
- Switching 3D off retains backend validation while omitting untouched
  controls exposed only by the 3D draft. Orange On/Off and Cancel are verified.
- Panels > Behavior can keep folder item names visible without hovering.
  This preference uses the existing draft, Apply/Cancel and persistence path.
- Baked 2.5D platform artwork fills the platform its icons stand on at every
  layout radius. The artwork was decoded at the drawn size but cropped in
  natural pixels, which drew a ring shrunk into the corner of its layer while
  the icons kept the full-size track. Icons also stand on the drawn track: the
  track's scene offset is applied once, not twice.
- Spacing regulates separation along curved tracks (circle, ring, radial,
  polygon, arc, semicircle, fan and baked platforms) in every renderer. At or
  above the reference spacing of 8 the entries are spread evenly; below it they
  draw together about the middle of the track, down to touching at zero.
- An open curve with more entries than fit shows the entries that stand on it
  at the configured spacing and moves the others along the curve with the
  wheel, one entry per notch in both directions. Order is kept and no straight
  tail is appended; closed rings keep turning. The wheel works on the bare
  surface of a platform as well as on its icons.
- True-3D icons stand on the platform's own track for every layout shape, so a
  square or polygon path no longer places icons over the platform's hole.
- Fan and Arc folder contents stand on an exact half circle that opens away
  from the dock, or a half ellipse when the popup is short. A larger folder shows the
  children that fit and moves the others along the same curve by wheel, held
  drag or keys, one child per wheel notch; a small folder uses a smaller circle.
- Theme cards resolve each theme with its own renderer tier through the same
  candidate Load applies. A theme whose style names no tier adopts its declared
  preferred tier, so a 2D theme can be loaded over a platform theme. Cards are
  drawn in a fixed context, without the selected panel's angle, rotation, tilt,
  collapse or radius, at the height of a preset card.
- A free panel with a ring, circle or polygon layout can be drawn in 3D
  without changing its theme. A look that ships no 3D scene of its own stands
  on the generic 3D platform, coloured by the look itself; turning 3D off
  returns the same theme in its own 2D or baked 2.5D renderer. Arcs and
  semicircles stay flat unless their theme brings its own 3D platform.
- Panel Studio has one Panels > 3D page for every 3D setting: Enable 3D,
  pitch, yaw and roll, scene position X, Y and Z inside the panel, scale, field
  of view, platform thickness, pedestal height, quality, key and fill light,
  animated orientation changes, a gentle float that is off by default, the
  same Spacing control, Reset 3D transform and Edit on desktop. Appearance
  points to it instead of to 3D themes.
- Edit on desktop shows Blender-style move, rotate and scale handles on the
  panel itself. Ctrl snaps, Shift is fine, Esc or the right button cancels a
  drag. Each finished drag joins a desktop draft; Apply as Active saves it in
  one transaction, Cancel restores the panel, and an interrupted edit is
  recovered. While editing, the panel takes no application presses.
- Reduced motion turns the 3D float and the animated orientation changes off.
- Panel Studio and Icon Properties open on the screen of the panel they edit;
  without one they open under the pointer, and on the primary screen only as a
  last resort.
- Preset cards and Panel Studio messages give reasons in plain language and
  name renderers as 2D, baked 2.5D or 3D. The internal code stays available on
  hover for reports. Studio messages use the footer's width on two lines, show
  the whole text on hover and can be copied. The title-bar close button has an
  accessible name and tooltip, and a long panel name no longer runs under the
  title-bar controls.
- On a curved layout the Spacing control explains its two ranges.
- A scene that switches to or from independent segments no longer logs type
  errors for one evaluation.
- The rendering gate's resource check measures what repeated theme changes add
  after a warm-up pass, from settled readings, with its limit unchanged. Its
  earlier single reading included the first load of every family and transient
  peaks the shell releases seconds later, so it failed at random.
- The repository no longer tracks the stale `build-codex-task-0014/` build tree,
  the master plan's three requirement-mapping files are restored under
  `docs/task-pack-v3/`, and a GitHub workflow runs the source-only gates.
- Current-facing installation/version wording follows the corrected candidate;
  historical package checkpoints and release assets retain their identities.
- **Quit Arch Dock** (an action of the application entry, or `arch-dock --quit`)
  now stops Arch Dock for the rest of the login session. Installed panels and
  the KWin window watcher no longer start it again on demand; an on-demand
  start during the stop is refused at once. Starting Arch Dock yourself (the
  menu, `arch-dock`, Panel Studio, the user service) clears the stop. TERM and
  INT stop it like Quit; a killed process is a crash and restarts on demand.
- A folder's contents open from the clicked folder, away from the dock. A
  native panel's folder opens away from its screen edge; a free panel's folder
  opens the way it faces out of the dock and turns to the other side only when
  the screen leaves no room. Ring and stack contents begin at the folder.
- New folder layout **Along the dock** for curved free panels, the default for
  new curved free panels: the folder's contents stand on an invisible track
  just outside the dock that follows its curve, tilt and perspective, unfold
  from the folder, and move along the track with the wheel or the arrow keys
  when they do not all fit. Existing panels keep their layout; choose it in
  Panel Studio under **Panels > Behavior > Folder layout**. The other five
  layouts stay available as popups, and straight edge panels keep the popup.
- A wide folder popup whose span covers the middle of the screen stays on its
  folder. Plasma moves such applet popups to the middle of the screen when
  they are attached to a narrow point; the popup is now attached along a
  stretch longer than itself, centred where it belongs.
- The wheel keeps turning a free panel while an icon's window preview is
  shown; turning the panel closes the preview.
- Panel Studio no longer reloads its whole editor for each backend change:
  changes that arrive together reload it once, and an unchanged panel is not
  reloaded. Those reloads stalled the dock for about a second during drops.
- Every opening mechanism Panel Studio offers now visibly closes and reopens
  the panel. A drawn ring or bar is squeezed onto its handle along the chosen
  axis instead of staying full size behind hidden icons, and a 3D platform
  closes toward its centre like an iris, down to a small ring, while its
  theme's own parts move. Mechanisms a panel's renderer cannot draw are not
  offered and are refused.
- 3D draws a panel's own look. A look without a 3D platform of its own (the
  baked Blue ring and Steel octagon, and every procedurally drawn look) stands
  on a platform generated along the panel's own layout (a circle, an ellipse,
  a triangle to octagon or other regular polygon, or the 300-degree radial
  arc) in the look's own colours, read from its artwork, with its rim glowing
  in the look's glow colour. It no longer borrows the Cyan theme's octagonal
  mesh and recolours it. Arcs, semicircles and fans stay flat in 3D.
- Orange Arc's own 3D platform is drawn in the dark copper of its 2D artwork
  instead of peach, and its pedestals and icon collars take the same colours.
- Panels > 3D has a Shape section: the panel's shape among those 3D draws
  exactly, the platform's width and a bend that tilts its top outward up or
  down. A theme with its own 3D platform keeps the shape it brings, and the
  page says so.
- 3D icons stand upright and face you at their real size, on solid pedestals
  in the platform's colour. They no longer lie flat inside dark rings or float
  away from short pedestals, and an icon's click area no longer grows and
  shrinks with its hover animation.
- In **Edit on desktop**, dragging the platform itself tilts it (up and down)
  and turns it (left and right); Ctrl snaps to 15 degrees and Shift is fine.
  The move, rotate and scale handles remain. An arrow that points straight at
  you moves with an up or down drag instead of doing nothing, and the editor
  says so.
- When 3D is not available, the 3D page names the actual reason: the kind of
  panel, the renderer, imported artwork, a flat skin, missing resources or the
  layout.
- Panel Studio and Apply are much lighter. Every edit used to read the panel's
  theme package from disk again, once for each 3D setting (about 70 reads per
  edit, each hashing every asset and parsing the 3D mesh). A package is now
  read once while its manifest and every asset it was verified from are
  unchanged, and an edit builds the theme's projection once. Measured on a free Orange 3D panel (Debug build, median):
  a Studio edit 792 → 44 ms, opening the Studio page 305 → 34 ms, Apply
  495 → 20 ms, the live panel's renderer configuration 39 → 4 ms.
- A theme package that changes on disk is verified again. The package cache
  used to trust a package while only its manifest was unchanged, so an asset
  rewritten, removed or turned into a link out of the package could still be
  served as verified. It now also checks every asset's file identity, size and
  times (nanoseconds) before reusing a package, and keeps a package only once
  its files have settled. An unchanged package is still read once.
- Arch Dock has a new icon and logo. It is installed in every standard size as
  `org.archdock.ArchDock` and used by the menu entry, both applets, Arch Dock's
  windows and the README.
- For contributors: source files say what they are for, the backend's largest
  file (`PanelWindow.cpp`, about 8,300 lines) is split by topic into eight
  files with every function moved unchanged (`PanelWindow.h` lists them), and
  two unused QML helpers (`MotionPolicy.js`, `StudioDraft.js`) and their tests
  are removed.
- Runtime corrections and candidate gates are recorded in
  [the release checklist](docs/RELEASE_CHECKLIST.md) and
  [corrective report](docs/POST_TASK_0045_CORRECTIVE_REPORT.md).

## 0.1.0 candidate — 2026-10-03

Historical record: the source/license state below predates the 0.1.1 selection.
The existing tag and six current prerelease assets remain unchanged.

Application version: `0.1.0`; Arch package candidate: `0.1.0-2`.
Annotated `v0.1.0` exists at `c3b3a0b7771b313c45f843f49a503b45b0d1ada0`.
The [GitHub prerelease candidate](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
was published on 2026-10-03; it is not a final/stable/latest release.
Physical acceptance remains incomplete and project-wide licensing is unselected.
The tag-matched artifact correction and acceptance are tracked in
[the release checklist](docs/RELEASE_CHECKLIST.md).

- Managed native Plasma and desktop-hosted free panels with ownership-checked
  creation, hide/show, recovery, placement, visibility and removal.
- A shared schema and renderer for native configuration, Panel Studio,
  embedded preview and live applets, with capability-driven controls.
- Procedural/skinned 2D, production chassis and energy themes, icon styles,
  per-icon overrides, baked ring/arc/polygon artwork, and optional true 3D
  with fallback and reduced-motion/resource limits.
- Requested icon motions, panel presentation mechanisms, transformed input
  geometry, window actions/previews, folders, segments, overlays and status.
- Exactly 15 immutable built-in Panel Presets and 15 immutable Icon Presets,
  separate rendered browsers, reusable custom derivatives and future defaults.
- Journaled desktop audition with exact Apply, Cancel/Revert, custom-save,
  default and interrupted-session recovery semantics.
- Persistent profile import/export and transactional multi-panel apply,
  rollback/recovery, plus opt-in KDE global shortcuts with conflict reporting.
- Direct D-Bus startup, a single backend owner and optional manual systemd
  control, with prefix-aware startup metadata and Arch packaging.
- Versioned data-only configuration backups, bounded retention, migration
  snapshots, offline restore and refusal of conflicting live/recovery owners.
- Fractional-scale, screen-identity, keyboard/accessibility and resource
  hardening, with isolated native Wayland regression fixtures.
- Release documentation and a native disposable package upgrade scenario
  from `0.1.0-1` to `0.1.0-2`, retaining the existing install/uninstall checks.
- The legacy control applet's pending D-Bus callback now uses the same root
  object lifetime as the dock applet, avoiding callbacks after removal.

The earlier `0.1.0-1` package is the retained TASK-0043 verification checkpoint.
Package release `2` incorporates the recovery/hardening implementation and
release-preparation changes. Historical task receipts remain evidence of their
recorded checkpoints; fresh candidate results belong to the release checklist.
