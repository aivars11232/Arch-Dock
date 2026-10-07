# Arch Dock

<p align="center">
  <img src="data/icons/hicolor/256x256.png" alt="Arch Dock logo" width="160">
</p>

Arch Dock provides managed dock panels for **Arch Linux, KDE Plasma 6 and
Wayland**. Native edge panels use Plasma containments; free panels use
desktop-hosted Plasma applets. The service and both hosts share the same
renderer and settings schema.

The application version is **0.1.1**. The current Arch package candidate and
its verification status are recorded in [current state](docs/CURRENT_STATE.md).
Its proposed **v0.1.1** tag
and publication require separate authorization. The
[historical v0.1.0 prerelease](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
and its six assets remain unchanged, with the tag fixed at
`c3b3a0b7771b313c45f843f49a503b45b0d1ada0`.

Original Arch Dock work is licensed **GPL-3.0-or-later** under [LICENSE](LICENSE),
unless a component states a separate license. Existing MIT components retain
MIT; unknown-rights references remain non-installable and outside relicensing.
Physical acceptance remains incomplete. [Current state](docs/CURRENT_STATE.md)
and the [release checklist](docs/RELEASE_CHECKLIST.md) record candidate checks
and the remaining publication and physical boundaries.

## Features

- Launcher, task and hybrid panels, with managed native/free lifecycle and
  ownership-checked recovery and removal.
- Separate libraries of exactly **15 built-in Panel Presets** and
  **15 built-in Icon Presets**, rendered through the shared preview.
- Transactional desktop audition: Apply, Cancel/Revert, Save as Custom and
  defaults for future panels. Installed built-ins remain immutable.
- Procedural and skinned 2D, baked 2.5D on supported free hosts, and optional
  true 3D with capability checks and a safe fallback.
- Thirteen textured panel materials with tint and opacity. Studio lists flat
  and platform looks separately; Platform presentation off restores the saved
  flat look. Native 3D platforms offer material editing and bending, with icons
  anchored to the surface and input limited by visible depth.
- Icon styles and per-icon overrides, reduced-motion-aware animation,
  panel presentation, grouped-window actions, folder expansion, segments,
  overlays and status modules.
- Persistent profiles, explicit multi-panel apply/rollback, optional KDE
  global shortcuts, and bounded configuration backups and recovery.

Available controls follow the selected host, theme and installed capabilities.
For example, a native edge containment offers linear layouts and 2D rendering;
free hosts provide ring/arc layouts and the additional renderer tiers.
See [known limitations](docs/KNOWN_LIMITATIONS.md) for platform and dependency
boundaries.

## Install and start

Use the checksum-pinned local Arch package route in
[the installation guide](docs/INSTALL.md#arch-package). It lists dependencies,
source export, native makepkg, upgrade, recovery and uninstall commands.

After installation, open **Arch Dock** from the application menu, which
always shows Panel Studio, or run:

```bash
arch-dock --settings
```

Direct session D-Bus activation is the normal startup mechanism. Repeated
launches forward to the existing service owner. Installation does not enable
or start a systemd unit automatically.

To stop Arch Dock, right-click its application menu entry and choose
**Quit Arch Dock**, or run `arch-dock --quit`. It stays stopped until you start
it again or log in again. See [starting and stopping](docs/INSTALL.md).

Before removing the package, cancel any audition and remove managed panels
through Arch Dock's ownership-checked removal actions. Package removal
preserves user configuration, presets and backups; it does not edit personal
Plasma panel records. See [uninstall](docs/INSTALL.md#uninstall).

## Build and verification

[The installation guide](docs/INSTALL.md#build-and-disposable-installation)
describes a fresh build and disposable staging. The project uses CMake, Qt 6,
KDE Frameworks 6 and CTest. Build with one job on a 16 GB machine and run tests
serially. Private Wayland tests require the additional dependencies documented
in [the lifecycle guide](docs/plasma-lifecycle.md#controlled-validation).

[The platform matrix](docs/PLATFORM_MATRIX.md) separates observed private
Wayland behavior from physical-display and other-hardware checks. Tests must
use their disposable sessions rather than the owner's active desktop.

## Documentation and licensing

- [Current implementation state](docs/CURRENT_STATE.md) is the status and
  verification authority.
- [Architecture plan](docs/MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md)
  defines requirements and the consolidated task sequence.
- [Preset specification](docs/PRESET_SYSTEM_SPEC.md) defines catalog and
  audition semantics; [profile documentation](docs/PROFILE_PACKAGE.md)
  describes durable arrangements and transactions.
- [Changelog](CHANGELOG.md) records the release candidate's implemented scope.
- [Package licensing matrix and notices](packaging/LICENSING.md) define the
  GPL-3.0-or-later default, preserved MIT components and excluded references.
