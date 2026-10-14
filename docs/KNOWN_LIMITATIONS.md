# Arch Dock known limitations

This document records the `0.1.1` candidate's supported boundaries. Fresh
acceptance belongs to [the release checklist](RELEASE_CHECKLIST.md), current
implementation evidence to [CURRENT_STATE.md](CURRENT_STATE.md), and display
observations to [the platform matrix](PLATFORM_MATRIX.md).

## Platform and host capabilities

The production target is Arch Linux, KDE Plasma 6 and Wayland. Other operating
systems, desktop environments and X11 are outside this verification record.

Native edge panels retain real Plasma containments and the standard Plasma
editing model. They lay their row out along their edge, horizontal at the top
and bottom and vertical at the sides; their renderer tiers are procedural/skinned
2D. Whole-panel free rotation, ring/arc layouts, baked 2.5D and true 3D belong
to supported desktop-hosted free panels. Opening and closing mechanisms belong
to edge panels: a free panel stands open. The shared capability resolver
controls availability and explanatory UI; a draft value alone is not evidence
that a host supports it.

Free panels remain Plasma desktop applets and use the existing verified
geometry/recovery integration. Plasma controls the host; Arch Dock renders
and animates within it. Ownership tokens and unrelated-panel/widget safeguards
apply to both native and free hosts.

KDE's own widgets (the clock, the application launcher, Panel Colorizer)
can be added to an edge panel with Plasma's Add Widgets, since it is a
Plasma panel: Plasma sizes them to the panel, beside Arch Dock's widget and
on Plasma's panel background, outside Arch Dock's drawn look. A free panel
holds no other widget: one dropped on it becomes a separate desktop widget.

## Optional rendering and previews

Qt Quick 3D is an optional runtime dependency. The build's `AUTO`, `OFF` and
`ON` modes are described in [INSTALL.md](INSTALL.md#build-and-disposable-installation).
The Arch recipe includes the renderer at build time while allowing runtime
fallback when its module/capability is unavailable. Detailed 3D controls follow
that capability. Baked perspective artwork remains a separate 2.5D tier.

KPipeWire is optional for live window thumbnails. Window actions and ordinary
panel rendering remain separate from live-thumbnail availability. Reduced
motion and fallback behavior remain applicable when optional rendering is off.

Changing a 3D material or texture keeps the scene drawn with the previous
texture until the new file has loaded; the first readiness of a scene still
waits for every resource. The material textures are generated raster
images that depict their names (grass, plasma gas, lightning, aurora,
marble, metal, a lime's peel), not photographs; each glowing look's
palettes are separate files. A real glow (bloom) runs only while something
glows and costs a few passes of every frame; one 3D panel has one glow, so
it reaches as far as the farthest Glow reach lit in it. A flat panel's
lightning strikes along the track, a flat stand's or tile's lightning and
aurora are still; in 3D they move. In 3D they are lit as relief (a normal map per look) and the
energy looks flow round the band, but they remain surfaces: blades of grass
that sway or a plasma volume would be a new rendering feature with its own
geometry and shaders. A generated platform
wraps its texture round its band; the two bundled 3D themes keep their vector
surfaces, since theme packages carry no raster files.

A 3D panel turns all the way round and scales from 0.05 to 20 times, all
ways or sideways and up and down on its own; its view stops at the screen's
size, so a panel scaled past the screen is drawn up to the screen's edge.
Turned past its edge it shows its underside, drawn in the platform's
underside colour, with the icons that stand on top behind it. Moving and
scaling are set by hand (Edit on desktop); Panel Studio keeps the exact turn.

Plasma keeps a free panel's widget out of a Plasma panel's own area and on
the screen, so a free panel put on the desktop from a profile, a preset or
"Put back on the desktop" may settle a little away from its saved position;
the backend accepts that placement.
True 3D defaults to high quality, which supersamples the scene: on a weak
graphics chip choose Medium or Low on Panels > 3D.
Pedestals (the Pedestals page) are solids that face the viewer with their
icon in 3D, and the same shapes painted with flat shading on a flat panel
and in an opened folder; a theme package cannot yet provide its own pedestal
mesh. New panels have none; Auto is a column stand on flat and 3D panels
alike (an icon style with its own pedestal keeps it). A stand and its icon
take more height than the icon alone: on a thin edge panel a tall stand at
a large Size can reach past the panel's edge. The flat painter draws
twenty to a few hundred small faces per stand; a Spin, Float or Wobble
repaints a stand at each step of its turn (120 a turn) or wobble, once for
all the stands drawn alike, and a Bob, Pulse or Sway moves the painted
stand. A free panel's folder opened Along the dock on a tilted 3D platform
follows the projected ring, and the curve widens round the dock as far as
the children need; a folder with more children than fit still scrolls.

## Resource use

The [full audit of 2026-10-06](audits/FULL_AUDIT_2026-10-06.md) measured the
background service in a private session. Idle, it uses no CPU and wakes up
zero times a second, with about 53 MB of memory. An open Panel Studio uses about
7% of one CPU core for its animations and about 130 MB more memory. Closing it
ends that CPU use. The memory stays with the process to be reused, and
repeated opening and closing does not keep adding to it.

A window title change still makes each panel fetch its entries again, since the
entries carry window titles. One fetch costs about 5 ms of service time in a
Debug build: reading the panel's settings, building the entries, resolving
icon styles and sending about 76 KB. The Release package is faster. A window
that changes its title ten times a second therefore costs a few percent of one
core per panel. Sending only the entries that changed would remove this cost.
That is a larger change and is left for later.

Glow and energy layers animate continuously while their panel is shown and
reduced motion is off. They stop when the panel is hidden or reduced motion is
on.

Creating, placing or removing a panel, and some preset auditions, wait for
Plasma's scripting interface or KWin to answer before the service continues.
These waits happen only during those actions the user starts, never in
steady use.

The [full audit of 2026-10-11](audits/FULL_AUDIT_2026-10-11.md) measured the
shell with one free panel in a private session (Debug build, pointer away):
at rest about 2 % of one core for a flat panel, 3–4 % for the Cyan Mesh 3D
theme and 3–15 % for Orange Arc 3D; whole-panel rotation 44–45 %, continuous
item travel 58–61 %, the animated Plasma material 27–33 %. Continuous motion
and animated materials redraw the scene while the panel is shown; they stop
when it is hidden. A true-3D hit test ray-casts the platform's triangles in
QML on each pointer event (memoised between changes), so a pointer moving over
a 3D panel costs more than over a flat one. CPU figures measured through
`tests/visibility-window.py` include the harness's own per-icon hit-test
sampling, which is most of what it reports for a 3D panel.

## Panel Studio presentation

Panel Studio draws its own dark chrome with fixed colours and pixel font sizes.
It does not follow the KDE colour scheme, and the system font scale does not
enlarge its text. Its custom title-bar close button and message copy button
carry accessible names. A full move to Kirigami theme colours and scalable
text is planned after the 0.1.1 release rather than mixed into release
corrections.

On a small screen Panel Studio switches to a compact single-column layout so
that every page stays reachable. It is a fallback for limited space, not a
designed responsive mode.

A free panel removed from the desktop with its right-click menu keeps its
settings: it stays in Panel Studio's panel list, not on the desktop, and the
button with its name beside the list adds it back. Panel Studio has no
button that deletes a panel's settings. An edge panel is taken off the
desktop with Panels > Behavior > Visible; one removed through Plasma's own
menu while meant to be shown is rebuilt.

A change in Panel Studio shows in its own preview at once and reaches the
panel on the desktop with Apply or OK; Cancel drops it. Only a preset's
desktop preview and Panels > 3D's desktop editing show a draft on the
desktop panel itself. The owner asked on 2026-10-13 for every change to show
on the desktop panel at once, kept with Apply or OK; that is the next piece
of work, not part of this candidate.

Some source files are still large (`PanelRegistry.cpp`, `SettingsPopup.qml`,
`LayoutEngine.js`). `PanelWindow.cpp` has been split by topic into the
`src/panel/PanelWindow*.cpp` files (see the list in `PanelWindow.h`), moving
whole functions unchanged; the others are left for later, since splitting them
changes no behaviour.

## What Panel Studio offers

- One panel to a screen edge: Panel Studio adds an edge panel only where no
  panel stands yet (Arch Dock's, Plasma's or another dock's, as KWin and
  Plasma report them). Plasma lets panels on one edge cover each other, and
  places a centred panel without its offset, so Arch Dock does not stand two
  panels side by side on one edge.
- An edge panel of the "empty" type is a plain Plasma panel with no Arch Dock
  in it; Panel Studio does not create or offer it, though records from older
  versions keep it.
- A removed panel's settings go once Plasma has deleted it; a session that
  ends while Plasma's Undo is still possible leaves the decision to the next
  start (a panel whose widget stays missing is removed then).
- A folder's items are created when it opens: about a tenth of a second for
  two dozen items on the owner's PC, the one pause left in an opening.
- Stands in a glowing look are drawn as light in 2D (the look's texture
  screened over the stand's colour), which approaches the 3D scene's emissive
  stands but is not the same rendering.

Panel Studio shows a setting only where it changes the selected panel, and a
tab only when it has something to change (ADREP-TASK-001). The Studio truth
matrix test checks this on edge and free panels, for ten representative
layouts and each renderer tier they offer (21 panels). What follows from it:

- A free panel has no edge, alignment, Dynamic, Plasma visibility mode, Width,
  Height, opening and closing or running indicators. It cannot be hidden
  either; remove it instead. A free panel saved hidden or collapsed by an
  earlier version is shown, and open, after the upgrade; a backup is taken
  first.
- A setting that belongs to the panel but draws nothing in its present state
  is hidden and keeps its value: Opens on, Collapse axis, Reveal handle and the
  delays while the panel cannot close, and Dock layout on a baked look, whose
  icons stand on its artwork's own track. Only a closed track (a ring or a
  polygon) turns, so a baked arc offers no layout angle or whole-panel
  rotation; its icons still travel along it.
- Circle and Ring draw the same, as do Adaptive and Horizontal on a free
  panel, so Dock layout offers only one of each pair beside the others. A look
  made for one layout offers no Dock layout and says so on the Layout page.
- Item travel speed, Panel rotation speed and Motion runs appear only while
  continuous motion runs, each speed only for what it moves. Continuous motion
  moves and Scroll sensitivity are shown whether or not it runs, because the
  wheel follows them; on a baked arc, where only the icons can move,
  Continuous motion moves is not shown.
- On baked 2.5D and true 3D looks the icons travel along the platform's track,
  pass behind its body and front rim, and return in front. Pointer and drop
  targets follow the exposed part of each icon; covered rear glyphs take no
  input. Physical desktop acceptance remains in the owner's checklist.
- Edge panels have no wheel travel or rotation.
- A collapsed edge panel keeps its full-size pill with the icons squeezed
  into a sliver in the middle: a Plasma panel's geometry is unchanged by a
  collapse, only the content clips.
- Edge panels of one content type (Launcher, Hybrid, Tasks) show one shared
  set of entries; only a free panel keeps its own.
- Folder openings start at their smallest: a fan with names shows three
  children at once at the default 90° opening (more as it opens wider), an
  arc three, a new panel's stack three, a grid on a 3D panel two rows of
  three; the rest arrive by scrolling. A small ring holds six and grows a
  second circle round the first for more. On a 3D panel drawn large there
  may be no room on the screen for that: a shape then holds fewer at once,
  and a ring's first circle holds four or three before it keeps one circle.
- On a 3D panel a folder opening lies on the panel's own plane and keeps
  clear of the panel as drawn (its outline and icons, 4 pixels apart); a
  fan's band starts at the rim beside the folder. Its children are drawn no
  larger than the folder's icon, so ones nearer than the panel stay that
  size. A grid there moves a row at a time; on a flat panel it keeps its
  popup.
- A folder opening keeps clear of the other free panels as drawn, not of
  Plasma's own panels or windows. Where free panels stand closer than the
  minimum - as they may from before, since nothing moves at login - an
  opening that finds no clear place shows the fullest one it found.
- A free panel moved too close to another is pushed out once the move ends,
  not held back while it is dragged. The private sessions drove the move
  from Panel Studio; a drag on the desktop goes through the same check (the
  widget's container moves) but could not be driven there.
- A free panel Plasma has lost (over a login or a restart) comes back about a
  minute and a half after Plasma starts, not at once: until then Plasma may
  still be restoring its layout. Why the owner's Plasma drops Arch Dock's
  desktop widgets at a start was not found; a private plain Plasma keeps
  them.
- Whether the pointer at the screen edge reveals an auto-hidden edge panel
  could not be verified in private sessions: KWin's edge trigger ignores the
  harness's virtual pointer. Always keeps a panel in view.
- A free panel's folder Fan and Ring are drawn in the panel's flat look: its
  appearance's colour, line width and glow, or its custom colour. On a baked
  or 3D look they use that look's flat colours, not its artwork; a look whose
  flat surface draws no line (Plate, Pedestal) draws no fan or ring outline.
- Along the dock, Fan, Arc, Stack and Ring scroll with the wheel, the touchpad
  and the keys; they have no drag scrolling. Their window covers the shape's
  whole box, so a press there beside the children and the drawn fan or ring
  closes the folder rather than reaching the dock underneath.
- Folder scrolling follows Scroll sensitivity on free panels with a curved
  layout, where Panel Studio offers it; elsewhere one notch moves one child or
  one row. Edge-panel folders keep their popup shapes (PD-23).
- On a flat or baked free panel Grid keeps its popup beside the folder. On a
  tilted baked ring a neighbouring icon can stand on that side and the popup
  covers it; the anchoring test records it (one of five positions on the
  baked ring). Along the dock steps a little further out of a tilted dock
  where a child would otherwise stand on one of its icons.
- Badges and task progress appear only where the session reports them.

## 3D editing

3D is available for free panels with a ring, circle, ellipse, polygon
(triangle to octagon, or 3 to 12 sides) or radial layout: Arch Dock builds the
platform in that exact shape, in the colours of the panel's look. Arcs,
semicircles and fans stay flat unless their theme ships its own 3D platform
(the built-in orange arc does); a theme's own platform keeps its fixed shape. No built-in Panel Preset is labelled 3D: preset
compatibility and preset preview cards resolve through the theme catalogue,
which does not carry the generic 3D platform, so such a preset could only show
its fallback. Apply a preset, then enable 3D on the Panels > 3D page.

Panel Themes / Skins lists Flat looks and Platform looks separately. Turning
Platform presentation off restores the complete previous flat look. Native
platforms have their own colour, material and texture controls and a Reset
material action. Bend folds supported numerical platforms with their icons
anchored to the surface; fixed perspective artwork cannot fold and explains
that on the page. Procedural materials preserve texture under tint and opacity.
Sparkle defaults to zero; only Crystal and Plasma offer its intensity control.

On the desktop gizmo, the X and Y rings tilt the platform with vertical and
horizontal drags rather than by following the ring, which stays reliable when
a ring is seen edge-on; the white ring follows the pointer around its centre.
Dragging the platform itself turns it (across) and tilts it (up and down). An
arrow seen end-on cannot follow the pointer, so it moves with vertical drags
and the gizmo says so.
Scene position moves the platform within the panel's own area, which is sized
for the platform, so the room to move is small at full scale; scale still
stops at 1.25 and pitch at 60 degrees until the free transform work (the
owner's next phase). Nothing drawn is cut at the panel's box any more: the
3D view grows past it as far as the scene reaches, up to the screen's size,
at the cost of rendering the larger area; beyond about 2048 pixels across
it is drawn at a lower resolution and scaled.

## Hardware evidence

The verified private matrices use native KWin/Plasma Wayland, virtual outputs
and the host Radeon 610M GPU. The recorded host has one connected physical
display. Physical second-monitor plug/unplug, connector/scanout, another GPU
and owner-desktop observations have not been executed by these private tests.

Virtual output removal and fractional-scale checks do not close those physical
cells. Their exact target commands and observation requirements are retained
in [PLATFORM_MATRIX.md](PLATFORM_MATRIX.md#physical-cells-not-executed).
Mandatory unverified release cells remain open.
These unexecuted physical observations are release boundary **R-01**.

Two TASK-0044 private PlasmaShell processes faulted in native Mesa workers
during final disposal after live assertions. The precise internal fault
mechanism remains unproved. The verified fixture uses the existing stop/reap
helper with SIGKILL for final disposable PlasmaShell teardown; ordinary
in-session restart checks retain SIGTERM. Its affected regressions passed,
and both task-owned dumps were removed under owner authorization. This is a
fixture-disposal observation, not physical-desktop acceptance.
Under **R-03**, the teardown change is a verified test-harness mitigation;
later passing fixture behavior does not prove the underlying native fault
eliminated. The historical failures remain part of the evidence.

## Recovery and removal

Configuration restore is data-only. A pending profile-host recovery journal
must be resolved through the existing profile transaction; configuration
restore cannot bypass it. Recovery commands refuse a conflicting live service.
Backups are bounded and exclude installed built-ins, generated caches,
executable content and symbolic links. See
[configuration recovery](INSTALL.md#configuration-recovery-and-upgrades).

A profile holds the panels that are on the desktop when it is created or
saved: native panels always, a free panel only while the backend has
verified its widget. Detached free panel records are not captured. Apply
Profile creates at most four free panels at once and refuses otherwise
(`profile-creates-too-many-free-panels`); a profile saved before 0.1.1-19
may hold every record the registry had and needs one Save Current to
Profile with the wanted panels on the desktop. Detached records themselves
stay in the registry (the Studio lists each as "Missing: this panel is not
on the desktop") until Remove panel on its General page deletes it, or an
Apply Profile of a profile that does not hold it: an apply keeps only the
profile's panels.

A free panel whose desktop widget has disappeared from Plasma keeps its
settings; Panel Studio shows it as missing and can put it back on the desktop.
Why widgets disappear from a desktop is not always recorded by Plasma; a
desktop layout change or a removed widget are known causes.

Package removal preserves user data and does not rewrite personal Plasma
records. Cancel auditions and remove managed panels through their verified
removal actions before uninstalling the applet package. Follow
[the uninstall route](INSTALL.md#uninstall).

## Versioning and licensing

The application version is `0.1.1`; [current state](CURRENT_STATE.md) records
the current Arch package candidate and verification status. Its proposed
`v0.1.1` tag/publication require separate authorization. Historical `v0.1.0`
remains at `c3b3a0b7771b313c45f843f49a503b45b0d1ada0`, and its
[GitHub prerelease](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
was published under separate owner authorization. It is not a final/stable/latest
release. Under **R-02**, the earlier tag/publication occurred and the owner has
now selected GPL-3.0-or-later for original project work. The historical tag and
six current assets remain unchanged; the next candidate carries the selection.

The original published source archive recorded a precommit HEAD rather than
the tag target. [Current state](CURRENT_STATE.md) records the RC-01 correction.
Official normalized release source must reproduce from the exact clean tag
checkout, including canonical Git modes, normalized ownership and tar/gzip
metadata. All four generated artifacts must match regeneration byte-for-byte,
including recipe commands/dependencies and checksum receipts. The
[tagged release verification route](INSTALL.md#tagged-release-source) enforces
that boundary. Approved working-tree exports remain valid developer inputs,
but cannot substitute for a verified tagged release archive.

Original Arch Dock work is GPL-3.0-or-later unless a component states a separate
license; [LICENSE](../LICENSE) contains the official GPL text. The package also
records MIT for its separately declared components and ships their notice.
Unknown-rights references remain NOASSERTION/non-installable and outside
relicensing. [The licensing matrix](../packaging/LICENSING.md) records the scope
and each original asset's provenance reason.

## Verification provenance

Under **R-04**, checks run by the implementation/correction agent are local
regression or candidate verification, not an independent audit of its own
changes. The supplied independent findings retain their original provenance;
independent closure review requires a separately performed review.
