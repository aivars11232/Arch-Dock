# Changelog

## 0.1.1 candidate — unreleased, 2026-10-04

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
  remains immediate. Late native popup resizing is constrained to the
  anchor screen so compact contents remain reachable.
- Appearance explains the current 2D surface and offers a route to 3D themes;
  compatible themes retain the existing 3D switch, quality and tilt controls.
- Panels > Behavior can keep folder item names visible without hovering.
  This preference uses the existing draft, Apply/Cancel and persistence path.
- Current-facing installation/version wording follows the corrected candidate;
  historical package checkpoints and release assets retain their identities.
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
