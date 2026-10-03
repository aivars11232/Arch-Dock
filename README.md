# Arch Dock

Arch Dock provides managed dock panels for **Arch Linux, KDE Plasma 6 and
Wayland**. Native edge panels use Plasma containments; free panels use
desktop-hosted Plasma applets. The service and both hosts share the same
renderer and settings schema.

The current release candidate is application **0.1.0**, Arch package
**0.1.0-2**. Annotated **v0.1.0** points to
`c3b3a0b7771b313c45f843f49a503b45b0d1ada0`, and the
[GitHub prerelease candidate](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
is published. It is a prerelease, not a final/stable/latest release.
Physical acceptance remains incomplete and a project-wide license remains
unselected. [Current state](docs/CURRENT_STATE.md) and the
[release checklist](docs/RELEASE_CHECKLIST.md) record acceptance and the
tag-matched artifact correction; publication alone does not close those limits.

## Features

- Launcher, task and hybrid panels, with managed native/free lifecycle and
  ownership-checked recovery and removal.
- Separate libraries of exactly **15 built-in Panel Presets** and
  **15 built-in Icon Presets**, rendered through the shared preview.
- Transactional desktop audition: Apply, Cancel/Revert, Save as Custom and
  defaults for future panels. Installed built-ins remain immutable.
- Procedural and skinned 2D, baked 2.5D on supported free hosts, and optional
  true 3D with capability checks and a safe fallback.
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

After installation, open the desktop application entry or run:

```bash
arch-dock --settings
```

Direct session D-Bus activation is the normal startup mechanism. Repeated
launches forward to the existing service owner. Installation does not enable
or start a systemd unit automatically.

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
- [Package licensing declarations](packaging/LICENSING.md) preserve component
  and asset declarations. A project-wide license has not been selected.
