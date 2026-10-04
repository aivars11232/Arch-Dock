# Paused 3D correction handoff — 2026-10-04

The owner requested pause, commit and sync. Work is approximately 58% complete.
The last verified PC installation remains **arch-dock 0.1.1-7**. No package update
or Plasma restart was performed for this correction. The edits below are WIP;
their runtime behavior has not been verified.

Saved changes:

- Hide the procedural fallback when baked artwork is ready.
- Move 3D icon cards and pedestals into platform world coordinates; project
  their bounds back into the shared input/anchor geometry and use native picking.
- Add held-pointer rotation on the free-panel platform and typed controls for
  camera yaw, thickness and pedestal height. Expose themed-surface opacity.
- Add an Orange Arc mesh variant with a 64-segment beveled circular platform.
  Its 3D toggle selects a circular draft path; baked mode restores the arc.
- Extend existing schema, settings, renderer and rotation tests for the changes.

The single-job Release build completed the application, Integration plugin,
schema test, renderer test and settings test. It was stopped during the final
`preset-library-test` build at the owner's request (exit 130). Later standalone
QML edits require a refreshed build. **No new tests or native gates have run.**

Retained resume workspace: `/mnt/F/depth8` (owned marker and build/logs).
Retained checkpoint: `build-codex-depth-motion-0.1.1-8/STATE.json`, copied closure
helpers and protected-file/core baselines. These ignored build/evidence paths
stay local; the source and this handoff are committed and synced.

Resume in this order, with one build job, one test worker and no subagents:

1. Finish/refresh the existing Release build, including the package-test fixture.
2. Run affected schema/QML checks, settings transaction checks and native
   Wayland/RHI checks for Cyan and Orange depth, picking, wheel and drag input.
   Diagnose failures before committing a verification claim.
3. Bump the Arch recipe to package release 8, freeze verified source and regenerate
   the canonical source/checkpoint/recipe/checksum artifacts. The root recipe
   still pins the last verified release 7; no release-8 checksum exists yet.
4. Build and verify the fresh package, retain evidence and remove only this
   task's disposable work. Preserve all older evidence and existing core records.
5. Commit/sync, then perform the already-authorized backed-up PC package update
   and Plasma refresh. Verify installed bytes/modes, live backend identity and
   preserved user settings; record closure and sync the final documentation.

The intermittent slow Apply report remains unreproduced. Do not claim it fixed.
Tagging/publication are outside this paused runtime correction's remaining work.
