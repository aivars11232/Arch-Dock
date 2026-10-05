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

Remaining release closure: canonical source/checksum regeneration, fresh package
build, installed-package verification, owned cleanup, Git sync, backed-up actual
PC update and authorized Plasma refresh. Installed package remains 0.1.1-7 until
that installation receipt is complete. No publication is required for this pass.

The intermittent slow Apply report remains unreproduced; it is not claimed fixed.
