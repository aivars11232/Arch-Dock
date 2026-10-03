# Arch Dock known limitations

This document records the `0.1.0` candidate's supported boundaries. Fresh
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

Application `0.1.0`, package `0.1.0-2` and proposed tag `v0.1.0` identify the
candidate. The release commit/tag and publication remain owner-controlled.
A working tree with approved release changes is not a clean published release.
Under **R-02**, a project-wide license, tag creation, GitHub Release and package
publication each require separate owner instruction. Commit/sync authorization
does not grant publication authorization.

The repository has no selected project-wide license. The package records
`LicenseRef-Arch-Dock-Unspecified`; existing component and asset declarations
remain unchanged. [The licensing notice](../packaging/LICENSING.md) describes
their scope. Release preparation does not select a license for the owner.

## Verification provenance

Under **R-04**, checks run by the implementation/correction agent are local
regression or candidate verification, not an independent audit of its own
changes. The supplied independent findings retain their original provenance;
independent closure review requires a separately performed review.
