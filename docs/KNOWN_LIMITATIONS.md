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

## Optional rendering and previews

Qt Quick 3D is an optional runtime dependency. The build's `AUTO`, `OFF` and
`ON` modes are described in [INSTALL.md](INSTALL.md#build-and-disposable-installation).
The Arch recipe includes the renderer at build time while allowing runtime
fallback when its module/capability is unavailable. Detailed 3D controls follow
that capability. Baked perspective artwork remains a separate 2.5D tier.

KPipeWire is optional for live window thumbnails. Window actions and ordinary
panel rendering remain separate from live-thumbnail availability. Reduced
motion and fallback behavior remain applicable when optional rendering is off.

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

Some source files are still large (`PanelRegistry.cpp`, `SettingsPopup.qml`,
`LayoutEngine.js`). `PanelWindow.cpp` has been split by topic into the
`src/panel/PanelWindow*.cpp` files (see the list in `PanelWindow.h`), moving
whole functions unchanged; the others are left for later, since splitting them
changes no behaviour.

## What Panel Studio offers

Panel Studio shows a setting only where it changes the selected panel, and a
tab only when it has something to change (ADREP-TASK-001). The Studio truth
matrix test checks this for every host, layout and renderer tier. What follows
from it:

- A free panel has no edge, alignment, Dynamic, Plasma visibility mode, Width,
  Height, opening and closing or running indicators. It cannot be hidden
  either; remove it instead. A free panel saved hidden or collapsed by an
  earlier version is shown, and open, after the upgrade; a backup is taken
  first.
- A setting that belongs to the panel but draws nothing in its present state
  is hidden and keeps its value: Opens on, Collapse axis, Reveal handle and the
  delays while the panel cannot close, and Dock layout on a baked look, whose
  icons stand on its artwork's own track. Only a closed track (a ring or a
  polygon) turns, so a baked arc offers no layout angle or rotation.
- Circle and Ring draw the same, as do Adaptive and Horizontal on a free
  panel, so Dock layout offers only one of each pair beside the others. A look
  made for one layout offers no Dock layout and says so on the Layout page.
- Rotation speed and when the rotation runs act while the panel turns;
  ADREP-TASK-002 reworks that section.
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

On the desktop gizmo, the X and Y rings tilt the platform with vertical and
horizontal drags rather than by following the ring, which stays reliable when
a ring is seen edge-on; the white ring follows the pointer around its centre.
Dragging the platform itself turns it (across) and tilts it (up and down). An
arrow seen end-on cannot follow the pointer, so it moves with vertical drags
and the gizmo says so.
Scene position moves the platform within the panel's own area, which is sized
for the platform, so the room to move is small at full scale.

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
