# 3D correction verification handoff — 2026-10-05

Resumed from the owner-authorized paused commit on 2026-10-04. The correction
now passes nine affected CTests, isolated typed Studio controls/transactions,
and the full staged native Wayland rendering gate. Package and actual-PC
installation acceptance are tracked in CURRENT_STATE.md and the retained
build-codex-depth-motion-0.1.1-8 evidence.

Verified changes:

- Icon cards and themed pedestals share the platform's world coordinates.
  Native camera projection supplies matching bounds, input and folder anchors.
  Cyan and Orange native RHI checks prove visible icons, near/far depth and size,
  tilt, wheel rotation and held-pointer rotation in both directions.
- Custom geometry picking refines Qt's bounding-volume hit against the actual
  validated triangles, preserving the ring hole and platform drag surface.
- Baked artwork hides the procedural fallback arc. Normal Open mode excludes
  unrelated mesh mechanism parts. Orange includes a beveled 64-segment 3D ring;
  its 3D switch uses a circular draft and restores the baked arc when disabled.
- Studio exposes orientation, thickness and pedestal height alongside tilt,
  layout size and surface opacity. Theme capabilities match the catalogue.
- Returning from 3D submits only originally available fields and explicitly
  edited newly exposed fields. Unedited projected defaults remain preview data;
  backend validation still rejects unsupported edits. Orange On/Off and Cancel
  preserve the saved panel record.

Verification used one session, no subagents, one compile job and one test worker.
A full native run failed without a reported assertion during rapid theme cycling;
the complete repeat with command failure diagnostics passed, including the same
cycling and recovery assertions. No broad suite was rerun. The earlier software
module declaration and catalogue capability mismatches were corrected before
these final gates. Protected evidence and existing core records remain intact.

That release closure is complete: package 0.1.1-8 was built from source freeze
`78eceedddb9d6c508b592c027c3e49a2fb26feee`, verified, installed on the owner's
PC and recorded in CURRENT_STATE.md on 2026-10-05. Nothing was published.

The intermittent slow Apply report remains unreproduced; it is not claimed fixed.

## AD3D-TASK-001 follow-up — 2026-10-05

The owner then reported, with screenshots, that a baked ring was drawn small in
the corner of its scene while the icons kept the full track, that Spacing did
nothing on curved and 3D tracks, that a crowded semicircle and a large folder
grew a straight tail, and that theme cards showed other themes through the
selected panel's renderer. AD3D-TASK-001 corrects these in source; the
mechanisms are described in [the renderer notes](shared-renderer.md) under
"Track placement, spacing and overflow", "Half-circle path" and "Theme cards".

What the next 3D task builds on:

- `PanelScene3D.entryTrackRadius` is the one radius 3D entries stand on. Each
  entry is placed by its direction from the scene centre, and `PanelScene`
  hands the radius to `LayoutEngine.trackPlacement()` as `trackRadius`, so
  spacing and overflow are decided where the icons really are.
- `PanelScene.browseTravel` is transient browsing state, like
  `wheelRotationAngle`. Neither is saved, and a 3D transform editor must not
  persist them.
- `PanelRegistry::themeCandidateForTheme()` decides what a theme makes of a
  panel for Load, presets and cards alike. A generic 3D mode that keeps the
  selected theme should change the renderer tier through this candidate
  rather than beside it.
- The memory check at the end of `rendering-import-smoke` compares settled
  readings after a warm-up pass and after 16 further theme changes (the audit
  corrections record in CURRENT_STATE.md explains why). A failure of that
  check now means memory that is not released.
