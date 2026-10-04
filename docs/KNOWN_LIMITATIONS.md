# Arch Dock known limitations

This document records the `0.1.1` candidate's supported boundaries. Fresh
acceptance belongs to [the release checklist](RELEASE_CHECKLIST.md), current
implementation evidence to [CURRENT_STATE.md](CURRENT_STATE.md), and display
observations to [the platform matrix](PLATFORM_MATRIX.md).

## Platform and host capabilities

The production target is Arch Linux, KDE Plasma 6 and Wayland. Other operating
systems, desktop environments and X11 are outside this verification record.

Native edge panels retain real Plasma containments and the standard Plasma
editing model. Their available layouts are adaptive, horizontal and vertical;
their renderer tiers are procedural/skinned 2D. Whole-panel free rotation,
ring/arc layouts, baked 2.5D, radial presentation and true 3D belong to supported
desktop-hosted free panels. The shared capability resolver controls availability
and explanatory UI; a draft value alone is not evidence that a host supports it.

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
