# Arch Dock repair records

This folder holds one report per task of the Arch Dock LCL repairs package
(ADREP), which turns the owner's video audit of 2026-10-06 into five tasks:

| File | Task |
|---|---|
| `ADREP-TASK-001.md` | Panel Studio shows only what works for the selected panel, and the menu entry opens Panel Studio |
| `ADREP-TASK-002.md` | Free-panel items travel along the panel's own path while the panel stays still |
| `ADREP-TASK-003.md` | Folder layouts as the owner defines them, with sensible scrolling and working easing |
| `ADREP-TASK-004.md` | Panel looks with real texture, reliable Apply, and platform looks that keep motion |
| `ADREP-TASK-005.md` | Icons and tiles the owner can shape, readable logos, working presets, and the final package |

Task 4's formal Reports 1 and 2 record its complete 118/118 serial test pass
and 33/33 pushed-source fresh-clone recheck (2026-10-08), with all ten
criteria and findings accounted for. Recheck round 1 needed no source repair.
Task 5 starts after Report 2 sync and task-owned cleanup. The earlier
owner-authorized pause remains recorded as checkpoint history.

Each completed report has two formal sections:

- **Report 1 - implementation**, written before the task's commit: what
  changed for the owner, every owner finding (OF-nn) with its state and proof,
  the product decisions applied (PD-nn), each acceptance criterion with its
  proof, the tests with their counts, the files changed, open points and the
  owner's own checks.
- **Report 2 - recheck**, appended after the task was pushed and checked again
  on a fresh clone of `origin/main`.

The detailed logs, captures and measurements behind each statement stay in the
untracked evidence folder `build-codex-adrep/evidence/ADREP-TASK-00N/`.

These files are operational records, like `docs/CURRENT_STATE.md`:
`tools/prepare-arch-source.py` leaves everything under `docs/repairs/` out of
the source export and the package, and `source-exporter-test` checks that.
