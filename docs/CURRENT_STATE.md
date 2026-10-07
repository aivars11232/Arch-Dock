# Arch Dock current state

<!-- ADREP_TASK_003_BEGIN -->
## ADREP-TASK-003 — Folder layouts as the owner defines them (2026-10-07)

The third task of the owner's video audit (package `/mnt/F/Arch Dock LCL
repairs/`) is closed: implemented, verified, pushed (`047bdc5`, documents
follow-up `3b63b47`) and rechecked on a fresh clone; its report is
[docs/repairs/ADREP-TASK-003.md](repairs/ADREP-TASK-003.md). Nothing was
packaged or installed: the PC still runs 0.1.1-11, and the package for this
work comes with ADREP-TASK-005.

- A free panel opens Fan, Arc, Stack and Ring on a path of their own outside
  the dock, in the transparent window Along the dock already used
  (`FolderTrackHost`, `FolderTrack`); `LayoutEngine.folderShape()` builds them
  from the folder and the way it faces out of the dock: a sector drawn in the
  panel's look (Fan opening 40 to 160 degrees), an arc centred on the folder,
  a straight stack (Stack length 2 to 12) and a second ring (Small or Same as
  panel). They keep clear of the dock's icons, and where the screen is short
  they first hold fewer children, then turn. Grid stays the popup; edge panels
  keep their popups.
- One path model for all of them and Along the dock
  (`LayoutEngine.folderTrackLayout()`): open paths are a loop one slot longer
  than their places, so a child that leaves one end comes back at the other;
  rings carry their children round.
- The wheel gathers into whole steps: one notch, or one pitch of touchpad
  travel, per child, one row per notch in Grid, times Scroll sensitivity on
  free panels with a curved layout.
- `MotionChannels.folderEasing()`: outCubic, outBack, outElastic and a damped
  spring; folders open on a linear clock over their duration and close by
  running it back.
- Along the dock steps further out of a tilted dock where a child would stand
  on one of its icons.
- New test `folder-easing-test`; `folder-anchor-smoke` runs every layout on
  three looks (90 openings) with neighbours on the dock;
  `folder-interaction-smoke` covers every layout on both panel kinds,
  Expand folders on click included.
- Complete suite: 117 of 117 in one serial run on the final source (1548 s).
- Open: a Grid popup on a tilted baked ring can cover a neighbouring icon.

Evidence: `build-codex-adrep/evidence/ADREP-TASK-003/`.
<!-- ADREP_TASK_003_END -->

<!-- ADREP_TASK_002_BEGIN -->
## ADREP-TASK-002 — Free-panel icons travel along the panel's own path (2026-10-07)

The second task of the owner's video audit (package `/mnt/F/Arch Dock LCL
repairs/`) is closed: implemented, verified, pushed (`e5dadc2`, documents
follow-up `1841b4e`) and rechecked on two fresh clones; its report is
[docs/repairs/ADREP-TASK-002.md](repairs/ADREP-TASK-002.md). Nothing was
packaged or installed: the PC still runs 0.1.1-11, and the package for this
work comes with ADREP-TASK-005.

- The wheel and a drag on a free panel with a curved layout move its icons
  along the panel's own outline (circle, ellipse, polygon edges, star points,
  spiral) while the panel stays still. One notch is one place, eased out over
  100 ms from the wheel event (arrived 73 to 104 ms after it); the next frame
  already moves, a spin queues nothing. Open paths (fan, arc, semicircle, radial, spiral) are a loop one
  place longer than the path, so an icon that leaves one end fades out and
  comes back at the other. Where the icons are is never saved.
- `LayoutEngine.trackPlacement()` has a travel phase in entry slots, used by
  every curved layout, the star, the spiral and theme tracks; hit regions,
  tooltips, drops, reorder, popup anchors and keyboard focus follow it.
- Animations: Continuous motion (Off, Clockwise, Counterclockwise), Continuous
  motion moves (Items along the path, Whole panel, Both), Item travel speed,
  Panel rotation speed, Motion runs and Scroll sensitivity (0.25x to 4x).
  Saved panels keep their motion: one that rotated becomes Whole panel, with a
  backup first. Layout: Direction (Up, Down, Left, Right) for fan, arc,
  semicircle and radial, beside Layout angle.
- A whole flat panel that turns no longer shows its outline behind its icons
  (it was painted again 74 to 143 ms after the wheel event): the outline is
  painted at its resting angle and turned as one picture. The spiral's line is
  drawn where its icons stand.
- New session test `path-travel-smoke`: real wheel input on twelve layouts,
  every resting state captured and every frame recorded.
- Complete suite: 116 of 116 in one serial run on the final source (1430 s).
- Open: folder scrolling uses Scroll sensitivity in ADREP-TASK-003; passing
  behind and in front of baked and 3D platforms is ADREP-TASK-004.

Evidence: `build-codex-adrep/evidence/ADREP-TASK-002/`.
<!-- ADREP_TASK_002_END -->

<!-- ADREP_TASK_001_BEGIN -->
## ADREP-TASK-001 — Panel Studio shows only what works; the menu opens it (2026-10-07)

The first task of the owner's video audit (package `/mnt/F/Arch Dock LCL
repairs/`, ADREP 1.0.0) is closed: implemented, verified, pushed (`eeed2f5`,
documents follow-up `82e3d7e`) and rechecked on a fresh clone; its report is
[docs/repairs/ADREP-TASK-001.md](repairs/ADREP-TASK-001.md). Nothing was
packaged or installed: the PC still runs 0.1.1-11, and the package for this
work comes with ADREP-TASK-005.

- The application menu entry runs `arch-dock --settings`: Panel Studio opens
  when Arch Dock is stopped, after Quit, while it runs and after Studio was
  closed. D-Bus activation, applets, the KWin watcher, crash recovery and a
  plain start never open it (`session-startup-runtime-test`).
- Every Studio field has one availability rule (`EditorCapability`, a `switch`
  the compiler checks); an unknown capability is never offered. A field the
  panel does not have is refused when changed; a field it has that draws
  nothing in its present state is hidden and kept (`inactive`).
- Free panels: no edge placement, alignment, Dynamic, visibility, Width,
  Height, indicators or opening and closing (PD-01; saved collapsed or hidden
  panels are shown open, with a backup). Edge panels: no Dock layout, Layout
  scale or Panel padding; a theme is judged by the row the panel draws.
- Pages: opening and closing and rotation on Animations only, one icon style
  selector (Icons > Appearance), no Appearance > Shape, empty tabs hidden,
  preset lists for the selected panel only, one sentence for Panel padding and
  each Notifications item.
- Studio truth matrix: `studioTruthMatrix` (21 host, layout and renderer
  combinations, drawn the applet's way by `tests/TruthMatrixPanel.qml`) in
  `panel-window-capability-test`, and again with a real graphics backend in
  `studio-truth-matrix-smoke`, where shader effects and true 3D are drawn.
- Complete suite: 115 of 115 in one serial run on the final source (1307 s).
- Open: an edge panel's reveal handle can lie outside its window (seen in a
  private session; edge panels are out of this task's scope, PD-23).

Evidence: `build-codex-adrep/evidence/ADREP-TASK-001/`.
<!-- ADREP_TASK_001_END -->

<!-- FULL_AUDIT_BEGIN -->
## Full audit and corrections — 0.1.1-11, 2026-10-06

Package **arch-dock 0.1.1-11** (source archive SHA256
`9cddfd60f4422d6976de7a8837b83886800fe77d13e6ac0199df93ba134f0b85`, package
SHA256 `77c24273a06d50410b177b62c8067cfd546efd8e0cd9ff1a95b809076fa266ed`)
carries the theme package cache revalidation below and the corrections from
the [full audit of 2026-10-06](audits/FULL_AUDIT_2026-10-06.md). It is
installed on the owner's PC and verified:

- installed with the owner's sudo password;
- all 232 payload files match, and `pacman -Qkk` reports 0 altered files;
- the new backend runs, idle at 0.0% of a CPU core with 0.4 wake-ups a second
  and 72 MB;
- Quit stopped it, an activation request was refused in 0.07 s, and the
  menu entry started it again;
- Panel Studio opened;
- Arch Dock's configuration, the Plasma layout and `plasmashellrc` are
  byte-identical after the upgrade, and Plasma was not restarted because it
  hosts no Arch Dock applet at the moment.

A private backup (mode 0600) and the installation receipt are in
`~/.local/state/arch-dock/`. Before the upgrade, the live 0.1.1-10 service held
496 MB and averaged 2.6% of a CPU core over 4.5 hours, the D-01 symptom.

What changed for the owner:

- Closing Panel Studio frees it. A closed Studio used to stay loaded with its
  animations running (689 MB and about 2% of a core on this PC).
- Moving or resizing a window no longer reloads the dock. Before, each frame
  cost 5.9 ms in the Release package and reloaded every panel; now a frame
  costs 0.4 ms even in a Debug build, with no reloads.
- Clicking a pinned application starts it through KDE's launcher. Terminal
  programs get their terminal, and the program no longer belongs to Arch Dock.
  A desktop file KDE does not trust is refused instead of run, and the dock
  shows "Application started" or "Launch failed".
- Names with a comma or semicolon show in full, and translated names are used.
- Names in tooltips and right-click actions are shown as typed. A name with an
  image tag could make the dock fetch that image from the network.
- A hidden skinned 2D panel stops animating, and each panel's entries are 44%
  smaller.

Verification (one session, no subagents, one build job, one test worker):

- Every audit defect has a test that fails on `faed409` and passes now.
- Complete configured suite **114/114** in one serial run on the final source
  (845.5 s). Native gates in that run: folder anchors 50/50; 10 offered
  opening mechanisms collapse and reopen, 25 are refused. The log scan is
  clean apart from the intended missing-Quick3D probe and missing-file
  fixtures.
- Two canonical exports identical (563 files); every archive member checked
  against the working tree; `makepkg --verifysource` PASS; one-job Release
  package built in 417 s.
- Installed-package harness PASS on the unmodified source: 232 payload files
  byte-identical with modes, licensing, hidden-source rendering, 15+15
  catalogs, startup with and without Quick3D, runtime UI and folder
  interactions, upgrade from 0.1.1-10 with configuration recovery, and removal
  with user configuration preserved.
  Two earlier runs each failed once on timing. In the first, the content step
  saw 0 batched content revisions instead of 1; it did not recur, and a
  diagnostic run showed both panels visible and 1 revision. In the second,
  Dolphin's desktop file dropped onto the free panel's folder got `NoReply`,
  because the applet waits at most 1 s for the service. 0.1.1-10's harness
  recorded the same drop and error once.
- Before the push, the commit passed the tagged-source verifier in a throwaway
  clone with a local tag only. Its export lists exactly the packaged files.
- Not changed: branch protection, CI build job, Studio theming, R-01 and the
  trademark question. The audit report lists them.

Evidence (untracked): `build-codex-audit/evidence/` (audit logs and
measurements) and `build-codex-audit-0.1.1-11/` (source, package, harness and
owner logs).

Owner checklist (only what automation cannot see):

1. Open Panel Studio, then close it. In System Monitor, Arch Dock's CPU use
   drops back to 0%.
2. Pin a terminal program (for example `htop`) and click it. It opens in a
   terminal window.
3. Hover over a few dock icons. Every name shows as plain text.
<!-- FULL_AUDIT_END -->

<!-- THEME_CACHE_BEGIN -->
## Theme package cache revalidation — 2026-10-06

Source correction on top of `9525257`, packaged and installed in
**arch-dock 0.1.1-11** (above).

The theme package cache added for ADFIX UF-08 reused a verified package while
only its manifest's path, modification time (milliseconds) and size were
unchanged. A package's assets can change on their own, so a rewritten, deleted
or replaced asset, or one turned into a link out of the package, could still be
served as verified.

Contract now: a cached package is reused only while its manifest and every
declared asset are the very same files — for each, the path the package names
(not following a link) and the file it reaches, compared by device, inode,
size and nanosecond modification and change times. Any difference sends the
package through `ThemePackage::load()` again, with all its path, containment,
size and digest rules, and only a valid result is kept. A package whose files
changed less than a second before it was read is read again next time instead
of being kept. Unchanged packages still take the fast path; managed
(content-addressed) packages and built-ins work the same way.

Verification (one build job, one test worker):

- Six new `panel-registry-test` cases. Before the fix five failed and the
  unchanged-package case passed; after it all pass: an unchanged package is
  read once; a fresh package is kept only once settled; an asset rewritten in
  place with the manifest's bytes, size and time untouched is read again,
  refused by its digest and accepted again when restored (also for a managed
  imported package); a deleted asset is refused; a replacement with the old
  size and time is read again and judged by the digest; an asset turned into a
  link out of the package is refused as unsafe, and one turned into a
  directory as not a regular file.
- Studio latency on the owner's Orange 3D panel, three runs, 0 package reads:
  edit p50 44.8–45.7 ms, page 34.3–35.2 ms, renderer configuration 4.3–4.5 ms,
  Apply 20.7–21.3 ms (ADFIX: 44.4 / 33.9 / 4.3 / 20.4 ms).
- Focused theme, registry, capability and preset gates 15/15; complete
  configured suite **114/114** in one serial run on the final source (848.7 s),
  log scan clean apart from the intended missing-Quick3D probe and corrupt-mask
  fixture; native gates inside it: folder anchors 50/50, 10 mechanisms
  collapse and reopen with 25 refused. `git diff --check` clean.

Evidence (untracked): `build-codex-cache/evidence/`.
<!-- THEME_CACHE_END -->

<!-- ADFIX_BEGIN -->
## Folders, Quit, true-3D fidelity and performance — ADFIX pack, 0.1.1-10, 2026-10-06

Package **arch-dock 0.1.1-10** (source archive SHA256
`922acbf151808aec2105044348152cfe9732f76bd63230b5137f7bd3e6800db1`, package
SHA256 `65523172fdd7e92086648ff2e72da564885d2e68a3c6f23507d1b593edc2dd20`)
closes the owner's correction pack ADFIX-TASK-001 to -003 and the owner's added
requests (icon and logo, comments, readability). It is installed on the
owner's PC and verified: installed with the owner's sudo password, all 232
payload files match and `pacman -Qkk` reports 0 altered files, the new backend
runs, and Panel Studio opened. Quit was observed on the real desktop:
`arch-dock --quit` stopped Arch Dock, an activation request was refused in
0.07 s and nothing restarted it for 8 s; starting it from the menu entry
cleared the stop. Arch Dock's configuration, the Plasma layout and
`plasmashellrc` are byte-identical after the upgrade. Plasma was not restarted:
it hosts no Arch Dock applet at the moment. A private backup (mode 0600) and
the installation receipt are in `~/.local/state/arch-dock/`.

What changed for the owner:

- **Quit Arch Dock** (menu action, `arch-dock --quit`, TERM or INT) stops Arch
  Dock for the rest of the login session; installed applets and the KWin window
  watcher no longer start it again. Starting it yourself clears the stop; a
  killed process is a crash and restarts on demand.
- Folders open from the clicked folder, away from the dock, in every layout and
  renderer. Curved free panels get the **Along the dock** folder layout (the
  default for new curved free panels): the contents stand on an invisible track
  just outside the dock that follows its curve, tilt and perspective. The other
  five layouts stay popups; straight edge panels keep the popup.
- The wheel turns a free panel while a window preview is shown; Panel Studio
  reloads its editor once per batch of changes, which removed ~1 s dock stalls
  during drops.
- Every opening mechanism Studio offers visibly closes and reopens the panel;
  mechanisms a renderer cannot draw are not offered and are refused.
- 3D keeps the panel's look. A look without its own 3D platform stands on a
  platform generated in the panel's exact shape (circle, ring, ellipse, 3 to 12
  sided polygon, radial arc) in colours read from the look, its rim glowing in
  the look's glow colour; Orange's own platform is redrawn in its 2D colours.
  Panels > 3D has a Shape section (layout, platform width, bend). Icons stand
  upright and face the viewer on solid pedestals. In **Edit on desktop**,
  dragging the platform tilts and turns it, and an arrow seen end-on moves with
  vertical drags. When 3D is unavailable the page names the actual reason.
- Studio edits, page loads and Apply are 10 to 25 times faster (median: edit
  792 → 44 ms, page 305 → 34 ms, Apply 495 → 20 ms on a free Orange 3D panel).
- New icon and logo (`org.archdock.ArchDock`). Source files explain their
  purpose, and `PanelWindow.cpp` is split into eight topic files.

Verification (one session, no subagents, one build job, one test worker, one
heavy process at a time):

- Complete configured suite **114/114** in one serial run on the final source
  (828.6 s). Log scan: no binding loop, type, reference, assignment or
  component error; the only import/decode errors are the intended
  missing-Quick3D probe and the corrupt-mask fixture. Native gates in that run:
  folder anchors 50/50 (was 0/35); 10 offered mechanisms collapse and reopen,
  25 refused; the wheel with animation off; folder and window interactions;
  staged preset previews; real RHI for generated platforms of every exact
  shape and both baked looks, gizmo, platform drag, end-on arrow, icons and
  pedestals; and, in a disposable Plasma session with the installed layout,
  KILL recovers while Quit and TERM stay stopped through a Plasma restart and
  activation requests.
- Memory: eight audition create/cancel cycles grew the backend by 9.6 MiB
  (unchanged limit 64 MiB), no stale hosts.
- Two canonical exports identical (562 files); every archive member checked
  against the working tree; `makepkg --verifysource` PASS; one-job Release
  package built in 412 s.
- Installed-package harness PASS: 232 payload files byte-identical with
  modes, licensing, hidden-source rendering, 15+15 catalogs, startup with and
  without Quick3D, runtime UI and folder interactions, upgrade from 0.1.1-9
  with configuration recovery, removal with user configuration preserved.
  Earlier runs found a stale pointer target in the harness's folder step (it
  now follows a native panel that settles to its applet's thickness, 92 → 108
  px) and once saw a drop refused by the applet's 1 s backend timeout, which
  did not recur.
- Before the push, the commit was cloned, tagged locally in that throwaway
  clone (no tag in this repository) and passed the tagged-source verifier; its
  export lists exactly the packaged files (562 paths, hashes and modes).
- AUD-09: `main` has no branch protection. That is repository governance,
  outside this package, and was left unchanged. No tag or release was made.

Evidence (untracked): `build-codex-adfix/evidence/` (TASK-001/002/003 closure
notes, logs `91`–`93`) and `build-codex-adfix-0.1.1-10/` (source, package,
harness and owner logs).

Owner checklist (only what automation cannot see):

1. Panel Studio > Panels > General: **Add free panel** (a new circle), set its
   **Content** to Launcher, Apply, and drop a folder from Dolphin onto it.
   Click the folder: its contents stand along the dock, following its curve
   and tilt.
2. On that panel, Panels > 3D > Enable 3D, Apply: a platform in the look's
   colours, icons upright on pedestals. Try Shape (width, bend) and **Edit on
   desktop**: drag the platform itself to tilt and turn it.
3. Pick an opening mechanism in Studio and collapse the panel: it visibly
   closes and reopens.
4. Right-click Arch Dock in the application menu > **Quit Arch Dock**: it stays
   closed until you start it again.
5. The new icon in the application menu and on Panel Studio's window.
6. On a second monitor: Studio opens on the screen of the panel it edits
   (physical cell R-01, not executed here).
<!-- ADFIX_END -->

<!-- AD3D_TASK_002_BEGIN -->
## Generic 3D, Panels > 3D page and desktop 3D editing — AD3D-TASK-002, 2026-10-05

Package **arch-dock 0.1.1-9** from source `669d18ef9e213b13008411108934ad2868804488`
(freeze `d501baa` plus the recipe). Installed on the owner's PC and verified:
the package was installed through the owner's own authentication, the 220
payload files match, `pacman -Qkk` reports 0 altered files, the new backend
runs, Plasma was refreshed and Panel Studio opened. A private configuration
backup (mode 0600) and an installation receipt are in
`~/.local/state/arch-dock/`. Arch Dock's own configuration is byte-identical
after the upgrade; in the Plasma layout only the wallpaper slideshow's current
image and the third-party Panel Colorizer's record of the tray's widgets
changed. The panels briefly re-activated the old backend while files were
replaced; that exact process was stopped before the new one started.

What changed for the owner:

- A free panel with a ring, circle or polygon layout can switch to 3D on
  **Panels > 3D > Enable 3D** without changing its theme. A look without a 3D
  scene of its own stands on the generic 3D platform in its own colours;
  turning 3D off returns the same theme in its own 2D or baked 2.5D renderer.
  Arcs and semicircles stay flat unless their theme brings a 3D platform.
- The 3D page holds every 3D setting: pitch, yaw and roll, scene position X, Y
  and Z inside the panel (desktop position stays on General), scale, field of
  view, thickness, pedestal height, quality, key and fill light, animated
  orientation changes, an optional gentle float, Spacing, Reset 3D transform
  and **Edit on desktop**.
- Edit on desktop shows move, rotate and scale handles on the panel itself.
  Ctrl snaps, Shift is fine, Esc or the right button cancels a drag. Apply as
  Active in Panel Studio saves the edit in one transaction; Cancel restores
  the panel; an interrupted edit is recovered. No application starts from a
  press while editing.
- Reduced motion turns the float and the animated changes off.
- No built-in Panel Preset is relabelled 3D (see known limitations).

Verification (one session, one build job, one test worker):

- Complete configured suite 112/112: 111 in the full serial run and the
  runtime UI gate after its 3D-page test was made to expect 3D where the
  session has it. Log scan: no QML type, reference, binding-loop, import or
  texture errors beyond the intended missing-Quick3D probe.
- Real RHI: transform, roll, input and anchors move together; easing and
  float respect reduced motion; gizmo move, Ctrl snap, rotate, scale and
  right-button cancel with real mouse events; the Blue Ring baked panel
  switches to 3D and back on the 3D page with its theme kept.
- Private Plasma audition matrix: desktop 3D edit with exact Cancel, single
  Apply and recovery after the service is killed mid-edit.
- Two canonical exports identical (537 files, archive SHA256
  `0d7b0f849c2b5a5b726fe33cb2d0b2c003c7531384036a1774df2706de5f9da5`); the
  tagged verifier passes in a disposable clone (no tag in this repository);
  `makepkg --verifysource` passes; one-job Release package built in 381 s,
  SHA256 `5619fbe851b6b5150f80b56f3046445419c68e5e8dea4d5fdbd0a52f26cb8db4`.
- Installed-package harness PASS: 220 payload files byte-identical, licensing,
  hidden-source rendering, 15+15 preset catalogs, startup with and without
  Quick3D, upgrade from 0.1.1-8 with configuration recovery, removal with user
  configuration preserved. A first attempt could not start its interaction
  check because the sandbox hides the project folder, where the test Python
  lived; the rerun used the same Python outside it.
- GitHub's Source gates workflow did not run for the two Task 002 pushes:
  GitHub could not assign a hosted runner during its Actions incident of
  2026-10-05 and cancelled both jobs after 15 minutes without running a step.
  The workflow's exact commands pass on a fresh clone of `669d18e`.

Owner acceptance checklist (not yet done by the owner):

1. Panel Studio > Panels, select the free blue-ring panel > **3D** > Enable 3D,
   Apply: a 3D platform in the ring's colours, icons on it, the theme unchanged.
2. Change pitch, yaw, roll, scene position and scale; the preview follows;
   Apply keeps them, Cancel restores. Reset 3D transform returns the neutral
   pose.
3. **Edit on desktop**: handles on the panel; Move, Rotate and Scale at its
   top; drag them, try Ctrl and Shift, cancel one drag with Esc or the right
   button; Apply as Active saves, Cancel restores.
4. Turn 3D off: the baked blue ring returns.
5. Task 001: crowded semicircle scrolls along the curve, the wheel works on
   the bare platform, folders open on a half circle, theme cards show each
   theme in its own renderer.
6. On a second monitor: Panel Studio opens on the screen of the panel it
   edits (physical cell R-01).
<!-- AD3D_TASK_002_END -->

<!-- AUDIT_CORRECTIONS_BEGIN -->
## Independent audit corrections — 2026-10-05

Source only, on top of AD3D-TASK-001; the installed package is still
**arch-dock 0.1.1-8**. One session, no subagents, one build job, one test worker.

Corrected:

- Panel Studio and Icon Properties open on the edited panel's screen, then the
  pointer's, then the primary screen (AUD-F05).
- Preset cards and Studio messages give plain-language reasons; internal codes
  stay available on hover (AUD-F06). Studio messages wrap, show the whole text on
  hover and can be copied (AUD-F09). The close button has an accessible name and
  tooltip (AUD-F10); a long panel title is elided before the title-bar controls
  (AUD-F11). Spacing explains its two ranges on curved layouts (AUD-F01 follow-up).
- The segment bindings read the segment layout itself, removing the eight
  `TypeError` warnings the native logs carried since 0.1.1-8.
- `build-codex-task-0014/` is untracked (its local copy is left in place); the
  three requirement-mapping files are restored in `docs/task-pack-v3/` with the
  task pack's SHA-256 sums (AUD-F14, AUD-F15); a GitHub workflow runs the
  source-only gates (AUD-F16); the 3D handoff document is current (AUD-F17).

The rendering gate's resource check is repaired, not loosened. A 24-cycle
measurement showed PlasmaShell memory moving within a bounded band, about 557 to
720 MB, with no upward trend, and dropping back within seconds of the last
change. The old single reading compared a transient peak with memory measured
before the first load of any perspective family, so it failed at random (three
of four runs at 146 to 278 MB). The check now takes settled readings (the lowest
over 10 quiet seconds) after a warm-up pass and after 16 further changes, and
keeps its 128 MB limit: memory that is never released still grows with every
pass and still fails. Five uncontested runs passed with growth between -39
and +10 MB, in about 82 seconds of the gate's 120-second budget; one more run
that overlapped a rebuild also passed and is excluded.

Measured, not changed (AUD-F07): Panel Studio's Apply in the backend, offscreen,
median of 15 rounds: 138 ms on the native bottom panel and 163 ms on a free ring
panel (transaction 16 to 26 ms, then two editor snapshots of 61 to 68 ms, of which
about 57 ms resolves the 16 theme cards). An edit's draft projection takes 65 to
83 ms. Plasma's native placement round trips are not part of these numbers, so
the intermittent slow Apply report remains unreproduced.

Recorded in [known limitations](KNOWN_LIMITATIONS.md#panel-studio-presentation):
fixed Studio colours and text size (AUD-F12), the compact small-screen layout
(AUD-F13), and the large-file split after release (AUD-F18).

Owner-only: visual and interaction acceptance (AUD-F03) and the physical
second-monitor and other-GPU cells (AUD-F04, R-01).
<!-- AUDIT_CORRECTIONS_END -->

<!-- AD3D_TASK_001_BEGIN -->
## Source-verified geometry, spacing, folder and theme-card correction — AD3D-TASK-001, 2026-10-05

Source only. The installed package is still **arch-dock 0.1.1-8**; this
correction built no package and installed, tagged and published nothing.

Corrected, each with a regression that fails on 0.1.1-8 source:

- Baked 2.5D artwork fills the platform its icons stand on, and the icons stand
  on the drawn track (the artwork was cropped in natural pixels after being
  decoded at the drawn size; the track's scene offset was applied twice).
- Spacing regulates separation on every curved track in every renderer, down to
  touching at zero; 8 and above keep the even spread.
- An overcrowded open curve shows the entries that fit and moves the rest along
  the curve with the wheel, in order, with no straight tail.
- True-3D icons stand on the platform's own track for every layout shape.
- Fan and Arc folder contents stand on an exact half circle (a half ellipse in
  a short popup) and move along it by wheel, held drag or keys.
- Theme cards resolve each theme with its own renderer through the candidate
  Load applies; a theme with no tier of its own can be loaded over a platform
  theme; cards are drawn in a fixed context at preset-card height.

Verification used one session, no subagents, one build job and one test worker.
The complete configured suite passed on this source, 111 of 111: 99 in the full
serial run and the other 12 in a serial rerun after these corrections.

- Seven lifecycle tests (`wayland-hardening-*`, `session-startup-runtime-test`)
  could not start their private KWin: its socket path under the task build
  folder was 110 characters. They pass with `-DARCHDOCK_TEST_TMPDIR` set to a
  short path. Environment only.
- Four test expectations predated 0.1.1-8 and had not been rerun since: Orange
  declares the mesh tier, the ring layouts, radial collapse and three mesh
  assets; opacity is offered on every surface. `PanelRegistryTest`,
  `PanelWindowCapabilityTest`, `Baked25DAssetTest` and the audition matrix's
  S12 refusal now state the shipped contract.
- The audition harness handed a draft reply to Python as one argument. More
  theme candidates now resolve, the reply passed 128 KB, and it is read from a
  file descriptor instead.
- The runtime UI harness now waits for the new surface and fresh entry
  positions before it sends a wheel to a tier it has just selected.

Native Wayland and RHI gates pass: `rendering-import-smoke` (with the real-RHI
track, spacing and wheel-surface checks for Cyan and Orange),
`window-interaction-smoke`, `folder-interaction-smoke` and
`runtime-ui-interaction-smoke` (wheel on the bare surface both ways and an
empty interior that passes through, on procedural 2D, baked 2.5D and true 3D).

Open, not caused by this correction:

- The resident-memory check at the end of `rendering-import-smoke` is not
  reliable. Unchanged, it failed twice on this source (146.9 and 146.1 MB
  against its 128 MB limit) and then passed with 5.9 MB. Its sample on 0.1.1-8
  renderer code ranged from 87 to 223 MB. Eight-cycle measurements grow at the
  same rate with and without the artwork correction, entirely in anonymous
  memory, and still grow with garbage collection forced every second. The
  growth predates this work and needs a heap profiler to locate; the limit was
  not changed.
- Eight `TypeError` warnings from the segment bindings in `PanelScene.qml`
  appear in the native logs, as they do in the 0.1.1-8 logs.
- Owner visual and interaction acceptance, and the physical second-monitor and
  other-GPU cells, remain the owner's.

Evidence is retained, untracked, in `build-codex-ad3d-task-001/evidence/`.
<!-- AD3D_TASK_001_END -->

<!-- WORLD_DEPTH_PAUSED_BEGIN -->
## Verified installed world-space 3D correction — 0.1.1-8, 2026-10-05

Source verification PASS: nine affected CTests, isolated typed Studio controls
and transactions, and full staged native Wayland/RHI. Icons and themed pedestals
share the 3D platform; depth, input, wheel and held drag match the projected path.
Orange supports a beveled 3D ring without the fallback arc. Orientation,
thickness and elevation join tilt, size and opacity controls. Orange On/Off and
Cancel preserve saved settings; inactive defaults no longer block switching back.
See [source verification handoff](3D_CORRECTION_HANDOFF.md).

Fresh single-job Release package and installed-package gate PASS: 219/219 payload
bytes/modes, presets/startup with and without Quick3D, native UI/folder interaction,
0.1.1-7 -> 0.1.1-8 upgrade/recovery and removal/configuration preservation.
Two independently regenerated canonical source/checkpoint/recipe/checksum sets
match; extracted bytes/modes match the frozen source. Root PKGBUILD pins it.
Source freeze: `78eceedddb9d6c508b592c027c3e49a2fb26feee`. Package SHA256:
`f241db8640aa1cdedc041fcf6d27bc91e8b1c33fea1e3e04d16bc8908ad3e45e`.

Cleanup PASS: disposable `/mnt/F/depth8` removed; package, source artifacts and
verification ZIP retained in `build-codex-depth-motion-0.1.1-8/`. Protected
1044 files remain byte/mode identical; existing 480 core records
remain, zero new core records, zero task processes and zero task dump payloads.
Removed 1006518272 allocated bytes across owned work only.
One session, no subagents, one build job and one test worker. Failed native cycling
and startup attempts are retained with the complete passing repeats; the startup
timeout did not recur under diagnostics. The broad suite was not rerun.

Actual owner PC now runs **arch-dock 0.1.1-8**. Fresh private configuration
ZIP/manifest are mode 0600; Plasma was refreshed and Panel Studio opened.
Installed payload: 219/219 bytes/modes PASS; native pacman Qkk reports zero
altered files. Stable live backend executable SHA256:
`d7319b38bfa47d249950a70f497aea7e79815cf78d749db04d629379fa77f497`.
Saved panel user settings are preserved; the only Plasma configuration change
is its slideshow's current wallpaper image. Private installation receipt:
`/home/aivars/.local/state/arch-dock/installation-0.1.1-8.json`.
Final cleanup audit: protected 1,044 files unchanged, original 480 core records
preserved, zero new core records, zero task processes/dumps, owned scratch gone.
The owner's visual/interaction acceptance remains pending. To test Orange 3D,
select that free panel, enable **Panels > Appearance > 3D rendering** and Apply;
tilt/orientation/size controls are on Layout, thickness/elevation/opacity on
Appearance, and automatic rotation on Animations. Wheel and platform drag
provide manual rotation. Existing saved renderer preferences are preserved.
Intermittent slow Apply remains unreproduced. The version-specific records
below are historical.
<!-- WORLD_DEPTH_PAUSED_END -->

<!-- FOLDER_PATH_CURRENT_BEGIN -->
**Previous verified correction — 2026-10-04: application 0.1.1; Arch package 0.1.1-7.**

Fan and Arc folder contents follow a compact invisible curve with wheel or
held-pointer dragging. Folder scrollbars and selection boxes are removed; original
glyphs, names preference, keyboard navigation and reduced motion remain. Free-panel
popups measure the clicked anchor's current screen when opening, select the side
with more space and fit that space before native Wayland placement.

Appearance now explains a plain 2D surface and offers **Choose a 3D theme**.
**Panel Themes / Skins -> Cyan Mesh Platform -> Load -> Apply** selects an existing
compatible theme; its existing 3D switch/quality and Layout tilt controls remain.
No arbitrary mesh-depth control was added. Slow Apply was not reproduced: read-only
renderer/Studio snapshots measured approximately 15/80 ms; these are not Apply timings.

Pack integrity 174/174 PASS. One-job build and six focused QML checks PASS;
seven Studio QtTest cases PASS, including the 3D route, native/free Apply/Cancel,
names, motion and tilt. Final folder regression PASS. Native Wayland five-layout,
two-host folder matrix PASS (53.88 s), including
screen bounds, transparency, names, 48-item wheel/held drag and no accidental launch.
The complete 111-test suite was not rerun for this bounded QML correction.

Installed package gate PASS (154.67 s):
216 payload bytes/modes, licensing/resources, optional 3D startup, actual installed
Studio/folder interactions, native 0.1.1-6 -> 0.1.1-7 upgrade, recovery and removals
with private configuration preservation. Earlier failed native placement attempts
and setup diagnostics are retained; no acceptance assertion was weakened.
Post-show window movement did not fix the native failure. Measuring the current
anchor and sizing before native placement passed the unchanged bounds assertions.

Source freeze `366f9f25be4699eaf084acee56db8f55ed7f44f2`; 527 source files/528 regular archive members.
Two exports and independent four-artifact regeneration, extracted bytes/Git modes,
ownership/epochs, GPL/reference exclusions and exact root recipe pin PASS.
Source SHA-256: `9a47ee361acacd088157485953de040c4be086d6bb6141957cb4968602c05869`.
Package SHA-256: `847a85c88d19d10f690667d132999888c8174d89f56a036b0a535e2be6749755`.
The fresh one-job Release compilation was reused after proving compiled/embedded
inputs identical. Unmodified native makepkg/CMake packaging produced the final
package; 215 payload files matched the previous package exactly and only the
standalone FolderExpansionHost.qml changed. Disposable source/build extracts were
removed and original checkout/Debug paths masked for installed verification.

Owned cleanup PASS: 1,869,230,080 allocated and
1,785,329,062 apparent bytes removed. All 999
protected historical files retain identical bytes/modes; all 478
baseline core identities remain, zero new core records, zero task dump payloads
and zero task processes. Compact evidence and exact source/package artifacts remain
in `build-codex-path-motion-0.1.1-7/`. One session, no subagents, one build job and
one test worker throughout.

Owner PC runs verified **arch-dock 0.1.1-7**: 216/216 payload bytes/modes, native pacman Qkk (325 files, zero altered), stable D-Bus owner/live installed executable and Panel Studio startup PASS. Saved panel user settings are preserved. Free panel 5 had no live desktop applet or backed-up desktop host before the update; recovery corrected only its stale ownership IDs/token/state/error and incremented the settings revision. Icons, appearance, position and all other panel values remain unchanged. Configuration differences are recorded in the private installation receipt; the fresh configuration ZIP/manifest remain mode 0600. Plasma was refreshed to reload the corrected widget code.

Historical version-specific checkpoints follow. The 0.1.1-7 record above is the previous installed checkpoint. The existing v0.1.0 tag/publication are untouched; no new release was published.
<!-- FOLDER_PATH_CURRENT_END -->

<!-- FOLDER_CONTENTS_CURRENT_BEGIN -->
**Historical verified owner correction — 2026-10-04: application 0.1.1; Arch package 0.1.1-6.**

Folder contents now unfold from the clicked rendered icon in KDE's native
anchored popup, with no themed Dialog background or opaque Pane. The existing
AppletPopup role, focus/deactivation, guards, selection revalidation and
five layouts remain. Dense Fan fits one vertical scroll axis; wheel up/down
and held pointer dragging scroll without opening a child. Arrow navigation
keeps the selected child in view. Labels are on by default; **Panel Studio ->
Panels -> Behavior -> Always show folder item names** saves through the existing
Apply/Cancel transaction, model/schema, renderer projection and persisted record.
Names off retains hover labels. The 48-item cap, original glyph/style/tile path,
scrollbars, horizontal overflow for applicable layouts, empty/unavailable notices
and reduced motion remain. Hidden close resets the explicit opening animation
immediately, so rapid reopening unfolds from the icon again.

Fresh one-job Debug/Quick3D build PASS (1508.93 s),
with the final incremental rebuild PASS. Six focused model/schema/transaction,
Studio, geometry and folder checks PASS. Complete final-source CTest
**111/111 PASS**, 53 serial bounded batches,
601.97 s summed batch wall time; zero failed/skipped/missing
names and all 644 tracked input bytes unchanged.
Four native Wayland gates PASS (170.47 s): actual
rendering, windows, folder/segment/status interactions and Studio/panel controls.
QML scan: zero unexpected errors; existing deliberate negative diagnostics
are classified in the retained receipt. All checks are self-run corrective
verification; owner visual/physical acceptance remains separate.

Source freeze `332037ba9839e48a858735366bb0000cf0cd1c95`. Two canonical exports, independent
regeneration of all four artifacts, extracted bytes/Git modes, unique checkpoint,
normalized ownership/epochs, GPL and reference-image exclusion PASS:
527 source files/528
regular archive members. Exact root recipe pin PASS.
Source SHA-256: `dff87e9a8145834c5800bf867526be39bfbfc1b445e7af1e071b25013fa96ee3`.
Package SHA-256: `e230ca026631845b387d911199ff690afc8bed7ca21954c6e81ccf7e10396be2`.
Fresh one-job Release/makepkg build PASS (395.16 s).
Installed package gate PASS (154.75 s):
216 payload bytes/modes, 17 asset declarations/eight MIT components,
15+15 preset catalogs/cards, startup with/without optional 3D, actual installed
UI and folder interaction matrices, native **0.1.1-5 -> 0.1.1-6** upgrade,
recovery and removals with configuration preserved. Compiled/extracted source
and the disposable verification clone were removed before installed verification;
the original checkout/debug paths were masked.

The first installed gate stopped when an existing Dolphin drop exceeded the 1000 ms synchronous D-Bus deadline although the backend persisted it. The identical package and unchanged assertions passed on a fresh isolated rerun. The failed attempt is retained; no drop-path production change or acceptance shortcut was introduced. Cold owner-desktop behavior remains part of manual acceptance.

Owned cleanup PASS: removed 1,899,859,968 allocated/
1,854,447,115 apparent bytes of disposable work.
All 972 protected historical files retain identical bytes/modes;
all 478 baseline core identities remain, zero new cores, zero
task dump payloads and zero task processes. 450 compact
evidence ZIP entries and exact source/package artifacts remain in
`build-codex-folder-contents-0.1.1-6/`. One primary session, no subagents,
one build job and one test worker throughout.

Owner PC now runs verified **arch-dock 0.1.1-6**. Actual 216/216 payload bytes/modes, native pacman Qkk, stable D-Bus owner/live installed executable hash, new folder names projection and Panel Studio startup PASS. Owner file modes and all saved panel user settings, entries, appearance and position are preserved. Configuration bytes changed: free-4 had no actual desktop applet before the update and its stale host association is now correctly marked detached; unrelated Panel Colorizer cached inventory and slideshow current-image keys changed during desktop refresh. No other keys or panel user values changed. The fresh private ZIP/manifest remain mode 0600. Plasma was refreshed to reload cached Arch Dock widget code.

The historical v0.1.0 tag and its published assets are untouched. No real new tag/publication occurred. Final clean Git/live remote/owner receipts are in local STATE.json.

**Preserved historical checkpoints follow. Their bodies and original version-specific outcomes remain unchanged; the 0.1.1-6 record above is current.**
<!-- FOLDER_CONTENTS_CURRENT_END -->

> **Status authority:** This file is the sole current-state source of truth for
> the repository. The architecture requirements are authoritative in
> [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md).
> Older progress and audit narratives are historical evidence, not current
> implementation claims.

<!-- AUD_01_02_CURRENT_BEGIN -->
**Current verified candidate — 2026-10-04: application 0.1.1, Arch package
0.1.1-5; AUD-01 AND AUD-02 CORRECTED.**
Starting audited clean main: `333fbc2aa8c906523a14a5cf1d168fbe45a91013`; starting owner package 0.1.1-4.

Current-facing package prose now follows this record; exact install/upgrade
commands use 0.1.1-5. Historical checkpoint bodies below remain unchanged.
IconScene computes mask failure against the current resolved URL and remembers
failed URLs locally. Its Image source remains independent of fallback state,
removing the self-clearing binding loop. Missing/corrupt -> valid -> broken ->
plain -> valid transitions recover in the same scene/Image; current broken
masks still fail closed without losing the original glyph. Other asset errors,
resolver behavior, Icon Tiles, overrides and existing panel controls are retained.

One-job Debug/Quick3D-ON build PASS (1315.76 s).
Focused style/renderer/native gate **11/11 PASS**. Complete configured CTest
**111/111 PASS**, 53 serial bounded batches,
571.02 s summed batch wall time, zero CTest skips or
missing names; all 644 tracked input bytes unchanged.
Four native Wayland gates PASS (151.85 s), including mask recovery,
real 2D/3D glyphs/tiles, drops, scrolling/arrows, rotation, position and collapse.
QML scan: zero unexpected errors; six known non-mask negative-fixture diagnostics.
Mask negative cases stay in their dedicated native log with strict shader,
texture, binding and import guards.

Source freeze `fd23810f156b7303e634c2533d421dd414e13d12`. Two independent canonical exports and all
four regenerated artifacts match byte-for-byte; extracted 527
source files/528 regular members, normalized epochs/ownership,
Git modes, GPL and reference-image exclusion PASS. Root recipe is the exact pin.
Source SHA-256: `22124187d0014e50064fb838e4782aaf9280e3c518f4b6fc9146ef640c3c57d8`.
Package SHA-256: `59708bab963c43eaa667019029ee83a97f2f3ddb8431692bf8eb99a11e1eee55`.
Fresh one-job Release/package build PASS (393.70 s).
Installed gate PASS (99.43 s): 216 payload files,
17 asset declarations/eight MIT components, 15+15 catalogs/cards, startup with
and without 3D, installed mask/UI/glyph tests, 0.1.1-4 -> 0.1.1-5 upgrade,
recovery and removals with configuration preserved. Compiled/extracted source
was removed before verification; original checkout/debug paths were masked.

Owned cleanup complete: final root removal freed 1,667,784,704
allocated/1,639,671,345 apparent bytes, in addition to earlier
compiled/export disposal. 263 compact ZIP entries and exact source/package
artifacts remain in `build-codex-aud01-aud02-0.1.1-5/`. All 956 historical files
are byte/mode identical; all 478 core identities remain, zero new core records,
zero task dump payloads and zero task processes. One primary session; no subagents.

Owner PC now runs verified **0.1.1-5** after a fresh private configuration backup. Actual 216/216 payload bytes/modes, native pacman Qkk, stable D-Bus owner/live executable hash and Panel Studio startup PASS. Both configuration files are byte-identical and changed logical keys are empty. No active Arch Dock applet/integration module required a Plasma-wide refresh; only the backend was restarted. The previous binary reactivated during package installation; its exact UID, D-Bus owner, PID/start identity and old executable hash were verified before stopping only that process. D-Bus activation then loaded the new installed hash and remained stable through Panel Studio startup.
Final clean Git/remote/owner receipts belong to local STATE.json. Owner visual
and physical acceptance and proposed v0.1.1 publication remain separate;
historical v0.1.0 and its six assets are untouched.
<!-- AUD_01_02_CURRENT_END -->

## Historical verified checkpoints

<!-- PANEL_MOTION_CURRENT_BEGIN -->
**Latest verified boundary — 2026-10-04: PANEL CONTROLS AND MOTION VERIFIED; OWNER PC UPDATED.**
Starting main was `08e24ce7b32bcc057dc62c7d41d2697f563ef724`.

Panels tabs now have bounded navigation arrows alongside natural wheel scrolling.
Unattempted placement/visibility observations are neutral and remain unverified;
actual failures/fallbacks still produce diagnostics. Animations exposes the
existing collapse/reveal mechanisms, triggers, delays, duration and saved
Open/Close actions. Default procedural panels now declare their implemented
horizontal/vertical collapse. Free radial panels turn clockwise/counterclockwise
with vertical wheel input; optional continuous rotation and its Studio preview
use the existing controller. Input/glyph geometry follows the same angle.

General exposes free X/Y, using the existing ownership-checked desktop-container
command, live geometry read-back and rollback to the actual previous pose.
Resizing precedes positioning to avoid a Plasma relayout replacing the move.
Layout exposes perspective tilt for compatible baked or actual 3D themes,
through the existing parameter maps and renderer. Theme tilt bounds, native/free
authority, original glyphs, future opaque map values and reduced-motion guards
are preserved. Public operation steps are in [INSTALL.md](INSTALL.md).

Complete one-job Debug/Quick3D-ON build and final **111/111
configured CTests PASS**, 590.96 s for the passing batches across
37 serial batches; zero final skips/missing names. All four native Wayland gates
PASS in 146.34 s: actual tab/wheel controls, 2D/3D rotation,
owned free move/read-back/rollback, hover open/close, drops and glyphs. Real RHI
captures prove changed 3D tilt pixels with unchanged logical input geometry.

Earlier failures and diagnostics remain in the evidence: Fusion scrollbar
coverage of an arrow, the obsolete procedural-collapse rejection fixture,
profile resize/move ordering, a missing task capture directory, an early bounded
drop NoReply and one final private startup scripting timeout. Diagnostic tracing
also polluted captured readiness/log scans; uninstrumented final gates pass.
No drop/startup timeout or acceptance assertion was weakened. The initial source
verifier refused local tool settings from an isolated-HOME developer export;
canonical exports were regenerated from the clean frozen tracked checkout.

Source freeze `d1cb20a6f533e044e37c37170a3275b51cc17eae`; two canonical exports are byte-identical. Disposable
annotated verification, actual extraction, 527 source files/528 members,
bytes/modes, GPL and reference-image exclusion checks PASS. Real v0.1.0 is
unchanged. Source SHA-256: `c53bd92345d29ba3325c48cac5894853650523acc737bacc3623bce524c42642`.
Package SHA-256: `fb31f07927691614b9199e33c9f550ae0e79d3ee7de94c1b6d368cb0241c13f9`. Fresh one-job Release/Quick3D-ON **0.1.1-4** and native
installed gates PASS: **216/216 payload files**, licensing/resources,
15+15 catalogs/cards, startup with/without 3D, real installed UI, 0.1.1-3 ->
0.1.1-4 upgrade, recovery and removals with saved configuration preserved.
Compiled/extracted source was removed before installed gates; checkout/build
paths were masked. Gate wall time: 94.26 s.

Owned cleanup PASS: disposable build/venv/fixtures removed
(1,775,763,456 allocated bytes,
1,742,262,560 apparent bytes at final disposal).
483 compact evidence entries and canonical source/package
receipts remain under `build-codex-panel-motion-0.1.1-4/`. All **941 protected
historical files** remain byte/mode identical, including prior Icon Tiles STATE.
All 460 baseline coredump journal identities remain. The one blocked test-probe
abort is retained as a journal record; its exact system dump and DrKonqi metadata
were removed with diagnostics retained. Seventeen new records from concurrent
Nexees work were identified by cwd/environment and preserved. No task dump
payloads or task processes remain; corrected installed gates added no task crash. One primary serial session; no subagents.

Owner PC now runs verified **0.1.1-4** after a fresh private configuration backup. Actual 216/216 installed payload bytes/modes, pacman Qkk (325 files, zero altered), stable new D-Bus owner/executable hash, the new projected settings and `arch-dock --settings` PASS. Both Arch Dock and Plasma configuration files are byte-identical, with no changed logical keys. No active Arch Dock applet or loaded integration module needed a Plasma-wide refresh; the verified backend was restarted and Panel Studio is open. Desktop activation briefly restarted the old mapped executable during package installation; the post-install restart was accepted only after its live executable hash matched the new package. Candidate closure was committed/synced at `e364a1729030319d38221a70a0473b461890bf97` before this update. The final operational Git receipt belongs to local STATE.json.

Owner visual/physical acceptance remains separate from private Plasma evidence.
No new real tag/publication was performed in this correction.
<!-- PANEL_MOTION_CURRENT_END -->

**Previous verified boundary — 2026-10-04: ICON TILES IMPLEMENTED AND VERIFIED.**
The previously unavailable Icon Tiles page now edits the selected panel's tile
visibility default, style/custom appearance, five shapes, fill/opacity and border.
Draft preview, Apply, Cancel and persisted reload use the existing settings
transaction. Explicit per-entry visibility takes precedence; original application
and folder glyphs, logical hit regions and panel geometry are preserved.

Six validated panel fields travel through the typed model, persisted settings,
profiles, runtime configuration and preview. Custom tiles use native Qt Quick
shapes in 2D and the same alpha texture behind the original glyph in true 3D.
Default style rendering is preserved. Tile edits mark preset lineage customized.

Fresh one-job Debug/Quick3D-ON build and all **111/111 configured CTests PASS**
in 588.98 s across 37 serial batches, with no CTest skips or missing
names. All four native Wayland gates PASS in 138.65 s, including real
Studio controls for native/free records, Cancel/Apply/reload and custom 3D tile
color/shape/visibility pixels. Earlier fixture failures and their corrections
remain in the evidence: visual-tree delegate lookup, actual ComboBox keyboard
activation, waiting for a completed Kirigami scroll step, and waiting for the live
drop area after re-enabling drops. Acceptance assertions remain in place.

Verified source freeze: `f51f239c83fd7667f71ce0351887f80f2921c844`. Two canonical exports
are byte-identical; disposable annotated-tag verification, actual extraction,
527 source files / 528 regular members, bytes/modes, GPL and reference-image
exclusion checks PASS. Only the eight reference-rights documents/original recipes
are included from source-samples. Real v0.1.0 was not changed by this pass.
Source SHA-256: `71dfcb9ea8cd43a9d9c7aa74fb75d950730db785e7a40518c63a1db7c0186d66`.
Package SHA-256: `0f632369f5f0a8131b62d7fd60a3f22226e79b87537453532a34305520ab0f31`.

Fresh one-job Release/Quick3D-ON package **0.1.1-3** and actual installed gates
PASS: **216/216 payload files**, bytes/modes, complete licensing/resources,
15+15 catalogs/cards, installed startup with/without 3D, real installed UI,
0.1.1-2 -> 0.1.1-3 upgrade, configuration recovery and both removals with saved
configuration preserved. Compiled/extracted source was removed before these gates;
five embedded development fallback paths are recorded, and installed behavior
passes without those paths. Installed-package gate wall time: 83.47 s.

Owned cleanup PASS: final disposable tree removed (1,730,564,096 allocated bytes,
1,700,625,523 apparent bytes); 162 compact evidence entries plus canonical source,
package and payload/closure receipts retained locally under
`build-codex-icon-tiles-0.1.1-3/`. All **928 protected historical files** are
byte/mode identical. All 460 baseline coredump journal identities remain, with
zero new dump records, task dump payloads or task processes. One primary serial
session was used; no subagents. Operational Git closure and owner PC update are
recorded separately from the source freeze. Source/candidate closure was committed
and synced at `2c4c0d81822120c7cc98ace3fefc30879aa6a1d3` before the PC update.

Owner PC now runs verified **0.1.1-3** after a fresh private configuration backup.
Actual 216/216 installed bytes/modes, `pacman -Qkk` (325 files, zero altered),
stable new D-Bus owner/executable hash, all six new tile editor fields and
`arch-dock --settings` PASS. Both owner Arch Dock and Plasma configuration files
are byte-identical, with no changed logical keys. No active Arch Dock applet or
loaded integration module required a Plasma restart; the verified backend was
restarted and Panel Studio is open. Post-update audit still preserves all 928
historical files and 460 baseline core journal identities, with zero new dumps.
The private backup/installation receipt and final operational Git closure are
recorded in the local candidate STATE.json. Owner visual/physical acceptance
remains separate from the private Wayland evidence.

**Previous verified boundary — 2026-10-04: FIRST OWNER-OBSERVED RUNTIME/UI ISSUES
CORRECTED AND VERIFIED. Starting main `f7b72859b5410f57669c5c56dbda1dfd2861706b`;
finalized source `85614f7f6ffed1bdba73528483b5e1089ba53f2f`. One-job complete build,
22/22 affected CTests, fresh full 110/110 configured CTests, final 4/4 native
Wayland CTests and actual Release-package installation gates PASS. Application
0.1.1, package 0.1.1-2. Owned cleanup PASS; final operational Git closure and
owner installation receipts are recorded separately from the source freeze.
Owner PC now runs verified 0.1.1-2; Panel Studio is open.**

The owner resumed the synced pause checkpoint `cd05eb39ff3027e8c4ed8074fadb8a1b9c5c9b46`.
The requested interruption at 79%, exit 130, was followed by successful completion
of every target. Fresh full CTest command wall time is 533.02 s (532.89 s summed
CTest-reported durations), zero failures/skips/missing names. Package-only harness
extensions came afterward; final affected native rerun is 4/4 in 131.60 s.
The source exporter was rerun after the recipe bump: 1/1, 6.58 s, all 11 groups.

Native checks exercise real GTK Wayland URI/application/folder drops, drop
receipts and backend commits, deduplication/refusals, internal pointer reorder,
native/free ownership, original folder glyphs and every launcher row in 2D/3D.
Production Qt events cover raw pixel/angle deltas; KWin EIS covers real vertical,
horizontal and Shift-wheel delivery without changing SpinBox/ComboBox values.
This does not establish owner Dolphin gestures or physical touchpad/GPU acceptance.
The owner's initially Empty free-2 panel is preserved; Empty deliberately shows
no launchers and now truthfully directs the owner to Launcher/Hybrid.

The first full-suite attempt caught source focus/readiness after a refused drag
fell through to the private desktop. Native KWin activation of the fixture,
exactly one drag start/end and released source fix the fixture without retrying
or weakening drop assertions. Isolated installed checks also required copied
installed theme assets and a Qt-written trusted PNG: native Glycin's nested
sandbox cannot start inside the read-only installed-payload overlay.

Final cleanup found one private PlasmaShell Mesa worker dump after passing
interaction assertions. Final private shell disposal now reuses the existing
lifecycle harness's SIGKILL path; ordinary restart semantics are unchanged.
All four native checks passed again with no new dump. Diagnostics are retained;
only the exact 31,394,377-byte task dump was removed. All 458 baseline journal
identities remain. One older payload independently became missing, consistent
with native two-week retention; the deletion actor was not established.

Two exports from the clean finalized source are byte-identical. Exact annotated
tag verification used only an owned disposable clone; real v0.1.0 is preserved.
Source audit: 525 source files / 526 regular members, canonical modes, normalized
ownership/order, matching commit/tar/gzip epochs, complete GPL and excluded
reference images. Source SHA-256:
`daab73ef0dcd0f5fb940762a3e89f6ce7d346c0c95ff55ee8a8556a7e4c03607`.
Release package SHA-256:
`a5020f57699660f5f1e2e5caf1179412cd93047d73f0b244263db1c147726d0e`.
Root PKGBUILD matches the exact canonical recipe. Prior 0.1.1-1 hashes below
are historical and STALE for this corrected candidate; a future v0.1.1 source
tag must use the new finalized source checkpoint above.

Fresh one-job Release/Quick3D-ON/BUILD_TESTING-OFF package build passes. Compiled
and extracted source were removed before native installed checks. Those checks
pass 215/215 payload bytes/modes, full GPL/MIT notices, all 17 asset declarations,
eight MIT components, 15+15 catalogs/cards, installed startup with/without 3D,
real installed UI matrix, 0.1.1-1 -> 0.1.1-2 upgrade, configuration recovery,
both removals and byte-preserved user configuration. The UI matrix masks source
and original build and explicitly copies test probes/UI fixtures; executable,
applet, shared modules and themes are installed payload bytes.

Owned cleanup removes 2,349,432,832 allocated workspace bytes plus the exact
private dump. 243 compact evidence entries and the canonical candidate remain
in `build-codex-first-runtime-ui-0.1.1-2/`; private evidence ZIP is mode 0600.
912 protected older files retain bytes/modes. No task process or dump payload
remains. STATE.json records final Git parity, cleanup and subsequent owner
installation status. The owner upgrade was performed after commit/sync and a
new verified private two-file backup: native pacman 0.1.1-1 -> 0.1.1-2, actual
215/215 installed bytes/modes and pacman Qkk PASS, new running backend executable
hash matches the package, normal D-Bus activation and Panel Studio launch PASS.

The owner explicitly approved the broader Plasma refresh. KDE's detached
refreshCurrentShell replacement aborted in a libtaskmanager icon-read thread
(QPixmap without live QGuiApplication). Starting the installed managed
plasma-plasmashell.service restored the desktop; the backend was reactivated
afterward. Exact native cause is unproved; the trace and successful workaround
are recorded. Only the resulting identified system dump, crash dialog/debugger
and matching DrKonqi cache were cleaned; diagnostic evidence remains.

Arch Dock configuration stayed byte-identical. Native Plasma refresh changed
only panelWidgets cache and slideshow current-image state; all other logical
Plasma configuration keys match the backup. Current free-1/free-2 records are
detached, and free-2 remains Empty. No hosted Arch Dock widget currently exists
on the owner desktops. Installed widget bytes are available to a newly hosted
panel; no live-widget glyph acceptance is claimed. For actual drop testing use
Panel Studio's existing free-panel creation path and choose Launcher/Hybrid.
Owner desktop/Dolphin/physical input acceptance remains manual.
No new real tag/publication was performed in this runtime pass.

**Historical previous boundary — 2026-10-04: RC-03 exact artifact verification, RC-04 canonical
modes and GPL-3.0-or-later licensing correction PASS. Finalized 0.1.1-1 source,
package, installed licensing and owned cleanup gates PASS. Candidate prepared;
new real tag/publication NOT EXECUTED.**

The latest supplied independent audit covers `b6fb0b472d9ea7428494239102a0877b591755eb`,
reports zero new runtime defects and closes AD-04-R1 and RC-02. Actual clean
starting main was `4d65678d53dd5d0e9f388bbda6bf43117427b1ea`; its newer publication
closure is preserved. This pass does not reopen production C++ or QML behavior.
The fixing agent's reproductions and checks are corrective verification,
not independent closure review of RC-03/RC-04.

The verifier independently regenerates archive, external checkpoint, PKGBUILD
and SHA256SUMS from clean finalized source and requires exact bytes. Semantic
checks remain as defense in depth. Git 100644/100755 exports as 0644/0755;
developer-only inputs use deterministic executable intent. All **11** exporter
unittest groups pass, including 17 artifact-tampering subcases, repaired pins,
tag/dirty controls, namespace/symlink checks and all eight required tracked
permission variants with identical four-artifact exports.

Application **0.1.1**, Arch package **0.1.1-1**, uses exact finalized source
commit **`96f0e4f60d024b5cb1a44af1402401656ded1372`**, epoch **1791069742**.
Annotated verification tags existed only in an owned disposable repository.
Two exports, including a tracked catalog at checkout modes 0600 and 0666, are
byte-identical across all four outputs. Complete real extraction verifies
**521 source files / 522 unique regular members**, canonical modes, one root
checkpoint without descendants, normalized ownership, deterministic order and
matching tar/gzip epochs. LICENSE and the GPL declaration are included;
reference images are absent.

Final source SHA-256:
`d76c9e45b30123f9064fca3837207b77e9f82f83d21304c9bae1069ba7fff525`.
Final package SHA-256:
`22b44078a8e9b9cbe95e81a51da590d3dcbcc43df25a0c80bc17d63f2ec2cb7d`.
The root recipe matches the exact canonical generated recipe for this source.
An eventual real v0.1.1 tag must target this tested source commit, independently
of main's later operational-documentation and recipe-pin closure commit.

The owner selected **GPL-3.0-or-later** for original project work. [LICENSE](../LICENSE)
is the complete official GNU GPLv3 text. All **11 themes and six icon styles**
have audited original-work provenance and GPL declarations. Eight existing
MIT components retain MIT and their complete notice. Unknown-rights references
remain NOASSERTION/redistribution-unknown, reference-only, non-installable and
outside relicensing. The [license matrix](../packaging/LICENSING.md) records each
asset reason and system dependency terms. The 33 changed asset JSON files
contain licensing and necessary digest-pin updates only; artwork/geometry/QML
and reference/visual evidence are unchanged.

Fresh gates pass: source verification, one-job Release/Quick3D-ON package build
**380.51 s**, focused test target build **212.52 s**, affected CTests **5/5**
(**6.97 s**), and scoped native installed gate **27.07 s**. Native checks verify
**210/210** payload bytes/modes against 209 CMake paths plus the package install
notice, complete GPL/MIT notices, 17 original asset declarations, eight MIT
components, **15+15** actual catalogs/cards, QtTest **8/8**, Quick3D-hidden **4/4**,
zero failures/skips, **0.1.0-2 → 0.1.1-1** upgrade, both removals and byte-preserved
configuration. No obsolete paths are expected; the existing absence assertion
is retained. Compiled package source/build and extracted-source fallbacks were
removed first; the owner checkout is masked during installed checks.

Prior runtime evidence is explicitly reused: **102 configured CTest names**,
18 native CTests, 37 lifecycle phases and private installed startup/recovery.
Five affected CTest names are fresh; this is not a fresh full 107-test suite or
new private runtime session. Of 349 prior recorded source/test/harness inputs,
342 are identical; changed inputs are version/license install rules, notices,
asset test license expectations and package license checks. Toolchain/dependency
BUILDINFO matches after removing identity/date/path metadata. **173** previous
package payloads are unchanged, 33 asset JSON files have audited licensing/digest
updates, executable/two documents differ and GPL text is added. No binary
identity or equivalence is claimed.

Focused asset tests initially caught ten stale production-record digest pins
introduced by the licensing edits; those pins were refreshed without changing
artwork or weakening assertions. One stale current version sentence was also
corrected. The earlier passing package was superseded and the source/package
gates were repeated from the final commit; failures and repairs remain archived.
Makepkg's fakeroot diagnostic and source-directory warning are retained;
package root ownership, payload/metadata and native installed gates pass.

Historical annotated **v0.1.0** remains object
`50812852c4dc2726411a1c73452296955852c50a`, targeting
`c3b3a0b7771b313c45f843f49a503b45b0d1ada0`. Its six current asset IDs, sizes,
digests, metadata and release body/timestamps are unchanged in fresh remote
checks. No new real or fixture tag exists on main or the remote.

Owned `/mnt/F/ri.poxzomwj` was removed, including disposable repository/tags,
builds, extractions, superseded packages and fixtures. Measured cleanup removed
**310,685,696 allocated bytes / 292,351,197 apparent bytes**. All **61** protected
prior files and **135** pre-existing core records are preserved; no task-owned
process or core remains. Twelve deliberate candidate/evidence files remain
under `build-codex-release-integrity-0.1.1/`, including a separate
`RELEASE_SHA256SUMS` so the exporter's one-entry SHA256SUMS stays canonical.
`STATE.json` and the local ZIP record final Git commit/sync and cleanup proof.

R-01 physical observations remain **NOT EXECUTED**. R-02 records the completed
GPL decision and prepared next candidate; new v0.1.1 tagging/publication require
separate later authorization. R-03's historical Mesa-worker mechanism remains
unproved and teardown a mitigation. R-04 retains independent finding provenance
and corrective-check labels. The external artwork sample remains unexecuted
without its archive. The supplied assignment truncates mid-section 24; its
missing remainder was requested and not received, so these results cover the
provided requirements.

**Historical previous boundary — 2026-10-04: RC-01 tag-matched source/package verification
PASS; RC-02 current public prose corrected. OWNER-APPROVED ASSET REPLACEMENT
AND FINAL REMOTE PROVENANCE VERIFICATION PASS.**

The pass started on clean `main` at
`29e9f22e32d8381d95b0d7ca81c4e7fde1c57281`. The supplied separate independent
audit reports zero confirmed new runtime bugs and closes AD-04-R1. Its two
remaining findings concern release identity and current public documentation.
All preceding implementation corrections remain intact.

Application **0.1.0**, Arch package **0.1.0-2**, annotated **v0.1.0** and the
[published GitHub prerelease](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
exist. The tag is preserved at
`c3b3a0b7771b313c45f843f49a503b45b0d1ada0`, tag object
`50812852c4dc2726411a1c73452296955852c50a`. Release ID `402697537` was published
at **2026-10-03 21:23:54 UTC**, `draft=false`, `prerelease=true`; it is not a
final/stable/latest release. The owner approved replacing its six assets and
applying the prepared notes. Replacement and final downloads are verified at
**2026-10-04 00:23:09 CEST**; the release was updated at 00:21:58 CEST.
The original publication timestamp, release ID, title and annotated tag remain
unchanged; explicit `latest=false` and prerelease status are preserved.

RC-01 was reproduced: two clean exports of that exact tag were identical
but differed from the original published source. The replacement checkpoint
HEAD equals the tag and its normalized epoch equals **1791062435**. The original
checkpoint used precommit HEAD `5a183b0` and epoch **1791058948**. All 519 source-file byte hashes
match; the catalog has canonical Git checkout mode `0644` rather than the
earlier owner's `0600`. Similar source bytes did not establish tag provenance.
The official path now uses a small release verifier with clean source, exact
annotated tag/HEAD, checkpoint, inventory and digest assertions. The general
developer exporter still accepts approved working-tree changes.

Verified published replacement source SHA-256:
`34abdc7fca9efcc1989a02abb47e774330c6f490c61715a2c82c63ce04295e9c`.
Verified published replacement package SHA-256:
`91db462260602e539beb6e21f18eff0456ae97e70121826491df1b453cd388ea`.
The root recipe pins the replacement source. Do not mix it with the original
archive retained as historical evidence; each asset set has its own matching
recipe/checksums. All six final downloads match reviewed local sizes/SHA-256
and GitHub digests; all five downloaded checksum entries pass. Two new clean
tag exports exactly match the downloaded source. Its checkpoint HEAD/epoch,
real extraction, 519 source hashes/modes, 520 unique members and verification
receipt source/package identities all pass. The remote body exactly matches
the approved notes.
Main's later public-documentation/tooling correction does not change the fixed
tag's historical source documents or the exact reconstructed candidate.

Fresh checks pass: **8 exporter unittest groups**, registered exporter CTest
**1/1**, two byte-identical exports, **519 files/520 unique members**, real
extraction and all hashes/modes/normalized metadata, generated recipe and sums,
native source verification, one-job Release/Quick3D-ON build **385.65 s**,
installed-test target **147.38 s**, scoped installed package gate **27.29 s**.
The latter verifies **209 payloads**, **15+15** actual catalogs/cards, QtTest
**8/8**, Quick3D-hidden **4/4**, upgrade/removal and preserved configuration.
All **349 runtime/CMake/harness inputs**, toolchain/dependencies and **208
resource payloads** match; executable bytes differ. Effective CTest evidence is
**107/107 names: one refreshed, 106 reused**. Prior 18 native CTests, 37 lifecycle
phases and private installed startup/recovery are explicitly **reused results**.
No production runtime/CMake/harness semantics changed or new private session ran.

RC-02 updates README, changelog, known limitations and platform current prose;
[the official generation route](INSTALL.md#tagged-release-source) and
[corrective report](POST_TASK_0045_CORRECTIVE_REPORT.md#rc-01-and-rc-02-release-provenance-closure--2026-10-04)
record the provenance guard and exact old/new hashes. Compact published assets,
approved release notes, cleanup, Git closure and before/after remote proofs belong to
`build-codex-final-release-provenance/STATE.json` and its local log ZIP.
Existing commit/sync authorization applies to the final closure documents.
The owner's subsequent “Approved” supplies section 14's specific authorization
for replacing all six assets and applying the reviewed notes.

R-01 physical acceptance remains **NOT EXECUTED**. R-02 records completed
tag/publication and verified authorized asset replacement, with project-wide
licensing still unselected. R-03 retains the two historical TASK-0044 Mesa-worker faults,
unproved mechanism and teardown mitigation. R-04 preserves the independent
audit's provenance and labels fixing-agent checks corrective verification.
The external artwork-sample subcase remains unexecuted without its archive.

**Previous release-closure boundary — 2026-10-03: AD-04-R1 software correction
COMPLETE; affected source/package verification PASS; R-01 through R-04 recorded.**

The release-closure pass started on clean `main` at
`5a183b0f77bfa35d0ac2a3358951a90b07654697`. The remaining exporter defect
admitted tracked/untracked descendants of its generated root checkpoint,
creating a directory/file extraction conflict. Direct/deep cases were reproduced
with export exit 0 and real extraction errno 21; the original exact-file guard
still rejected both controls. The exporter now reserves the first-component
namespace through a small explicit set, preserving valid nested same-name
files/directories, layout, input bytes and path/symlink safeguards.

Fresh targeted tests and all **6 exporter unittest groups** pass. The registered
checkout exporter CTest passes **1/1**. Two normalized exports are identical:
**519 source files + one regular generated checkpoint = 520 unique members**,
successful real extraction, matching inventory bytes/modes, normalized ownership
and timestamps, digest and recipe pin. Source SHA-256:
`367c3314260bd31ddf268a49ef861472f25b800966f361681f375bf5f8f5b077`.
Root `PKGBUILD` matches the generated recipe and pins this archive.

`makepkg --verifysource`, a fresh one-job Release/Quick3D-ON package build
(428.76 s) and the existing installed-test target build (169.19 s) pass.
Package SHA-256:
`7ef5cd2131bdedd63765c711f8148dd6ea4c7ba42510f8c30ed7970786333bcc`.
Fresh scoped installation checks pass in 27.19 s: **209/209** payload bytes/modes,
native install/upgrade/removal, obsolete-file checks and byte-preserved disposable
configuration; actual **15+15** installed catalogs/cards, QtTest **8/8** and
Quick3D-hidden **4/4**, zero failures/skips. Source extractions and the Release
build were removed first; installed checks also mask the checkout.

Production C++/QML, CMake and runtime harness sources are unchanged. **515 source
records and 349 runtime/harness inputs** match the previous candidate, as do
toolchain/dependency build records and **208 resource payloads**. The executable
digest differs, including its five build-specific development fallback paths;
no binary byte-identity or independent equivalence claim is made. Runtime reuse
rests on source/toolchain/resource identity and refreshed installed checks with
source fallbacks absent. Effective coverage remains **107/107 configured names**:
one refreshed exporter CTest and 106 unaffected results reused. The previous
18 native CTests, 37-phase lifecycle and private installed startup/recovery
are reused; no new full-suite/private runtime invocation is claimed. The
external artwork-sample subcase remains unexecuted without its archive.

R-01 physical second-monitor/connector/scanout, another GPU and owner-desktop
observations remain **NOT EXECUTED**. R-02 tagging and publication are now
separately owner-authorized, in addition to cleanup followed by commit/sync.
Correction commit `c3b3a0b7771b313c45f843f49a503b45b0d1ada0` is synced, with
its clean-tree/remote parity verified. Annotated `v0.1.0` points to that commit
locally and remotely. The
[GitHub prerelease candidate](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
was published at **2026-10-03 21:23:54 UTC** with six assets, explicit
`latest=false`, and all downloaded sizes/SHA-256/checksums verified. A
project-wide license remains unselected, with `LicenseRef-Arch-Dock-Unspecified` and existing
component/asset declarations preserved. R-03 retains the two historical
TASK-0044 Mesa-worker disposal faults and their unproved mechanism; verified
teardown is a harness mitigation. R-04 labels these self-run checks corrective
verification, not independent review. Physical acceptance is not inferred
from publication.

The [corrective report](POST_TASK_0045_CORRECTIVE_REPORT.md#ad-04-r1-release-closure-verification--2026-10-03)
records cause, red/green, setup/discrimination and reuse. Fresh compact artifacts
belong to `build-codex-post-task-0045-release-closure/`; its `STATE.json` records
task-owned cleanup and actual authorized Git/tag/publication proof, including
the subsequent documentation closure commit/sync. That closure leaves the tag
on the reviewed correction commit and the verified asset bytes unchanged.
The source checkpoint records starting HEAD `5a183b0` and all 519 approved
precommit inputs; existing exclusions for operational documents/recipe prevent
circular hashes. All 40 preceding protected artifact files are preserved. No dependency
download/install, live-desktop mutation or delegated agent/review occurred.

**Previous specified corrective boundary — 2026-10-03: AD-01 through AD-05
COMPLETE; native, full package and cleanup PASS; committed/synced as `5a183b0`.**

The standalone pass preserves the completed 45-task implementation on baseline
`2889cae61deb4ab4ff255319e64098cc80d9a684`. Recovery journals the verified
restored host IDs/revisions before committing; artwork import/Clear and Studio
propagate persistence outcomes; rejected profile imports clean newly owned
resources while protecting shared/uncertain ownership; the exporter reserves
generated metadata; theme guidance matches implemented optional 3D/presentation.
The complete issue register and executed evidence are in
[the corrective report](POST_TASK_0045_CORRECTIVE_REPORT.md).

The owner approved task-local PySide6 and instructed commit/sync after completion.
Native verification diagnosed and repaired a Canvas GUI-thread stall, asynchronous
preset capture and a throttled 3D frame observation. Existing private harnesses
also clean exact activation waiters and require desktop readiness, using Qt's
direct desktop-service route only in their synthetic session. Production startup
metadata and all mandatory assertions/timeouts remain intact.

Fresh one-job Debug application/test builds and a clean exported-source
Release/Quick3D-ON package build pass. **107/107 configured CTest names** have
applicable passing evidence: **65 refreshed checks and 42 unchanged initial
checks reused** against source/dependency identity. This is not a new single
full-suite invocation. All **18 native CTests** pass for the relevant final
source; the original standalone lifecycle passes **37/37 phases**, 113.08 s.
The offscreen private-KWin row's live counterpart passes in native testing; the
existing external artwork-sample row remains unexecuted without its archive.

Two normalized exports are byte-identical: **519 source files + one generated
checkpoint = 520 unique members**, exact current bytes/modes and normalized
identities/timestamps. Verified source SHA-256:
`539f46e318bba0f6fc6993235cf1fb350176fac9e077c5efb9582ceefa03fc36`;
root `PKGBUILD` pinned it at that checkpoint. Final package SHA-256:
`172a780e95f5bc17ac9c10c69ffbf0c416e1b4cb94068f8895adaa308ae1dfe0`.
The complete original package gate passes: **209/209 installed files**,
**15+15** rendered catalogs, QtTest **8/8** and Quick3D-hidden **4/4**, private
installed startup in both modes, native install/upgrade/recovery/removal and
byte-preserved disposable user configuration. Extracted source was verified and
removed, and the checkout masked, before installed-resource checks.

The current receipt is `build-codex-post-task-0045-corrective/FINAL_STATE.json`,
with final package files in `verified-package-output/` and continuation evidence
in `native-verification-output/`. Twenty deliberate corrective files remain;
ten preliminary files and all 20 older protected artifacts are byte-unchanged.
Task-owned builds, source extractions, Python/cache, diagnostics, private
sessions and fixtures are removed. The owner-authorized source/evidence commit
records closure; its exact HEAD, clean tree and remote parity are verified in
the final receipt after push. No host dependency installation or personal
desktop mutation occurred.

That assignment ended mid-sentence in section 9, so R-01 through R-04 definitions
were unavailable during the preceding pass. The later release-closure assignment
supplies them explicitly, as recorded above. The older blocked `STATE.json` and
preliminary subset receipts remain historical.

**Previous boundary — 2026-10-03: TASK-0045 COMPLETE; verification and cleanup PASS, Git closure owner-authorized.**

TASK-0044 is COMPLETE. Its owner-authorized cleanup documentation was committed
and synced as `9584d204ecfcbf503ccd7e4322ac945424b5d610`
(`Close TASK-0044 cleanup evidence`); HEAD and origin/main matched, with a clean
tree before TASK-0045. The exact approval
`APPROVED: IMPLEMENT TASK-0045 EXACTLY AS PLANNED.` authorized Phase A.
All executable Phase A checks and cleanup pass. After reviewing the result,
the owner instructed `Commit and sync`, authorizing Git closure of the fourteen
approved paths, including three new documents. This commit records that closure;
its exact HEAD, clean/tracked tree and remote parity are verified in the retained
TASK-0045 receipt. Source/package hashes and passing runtime gates are reused.
Physical release observations, tag creation and licensing/publication remain
explicit owner release decisions; task completion does not publish a release.

### TASK-0045 release verification — 2026-10-03

The V3 architecture authority/body, consolidated preset sequence and logical
target structure were refreshed first. README, installation/recovery/uninstall,
changelog, known limitations, platform matrix and release checklist now describe
the verified candidate. The existing native package harness adds an optional
previous-package argument and reuses disposable pacman operations, complete
payload checks and installed migration/backup/restore fixtures. Application
version remains `0.1.0`; package revision is `0.1.0-2`.

The original startup failure was traced to Qt temporary-directory creation
inheriting a read-only task TMPDIR inside the installed sandbox. Supplying the
sandbox's native writable `/tmp` restored configuration migration and startup;
no production configuration workaround was added. The shared lifecycle harness
also resolves proved offline-writer races by pausing only its tracked private
Plasma PID before offline edits, isolates the compositor's native KDESYCOCA
cache, and waits for exact native completion signals within bounded timeouts.
The complete 37-phase run has a 900-second bound; selected matrices retain
their existing shorter bounds. A single sentinel-creation helper takes native
grid geometry appropriate to each existing fixture context. Exact snapshots,
ownership tokens, unrelated-host size/state and completion assertions remain
enforced. These changes reuse the existing harness rather than duplicate it.

One production QML line attaches the legacy control applet's asynchronous reply
callback to `root`, following the existing dock applet's native QObject-lifetime
pattern. This prevents the observed callback from accessing a deleted applet.
No C++, CMake, exporter or project dependency changed. Earlier failures and
focused native Qt/KDE investigations remain archived; passing receipts do not
conceal them or claim that compositor CPU load was eliminated.

A fresh local clone was audited at the baseline, then its inputs were updated
with the approved source bytes. The final audit covers 635 tracked/input files;
117 historical tracked build entries are excluded by the existing exporter.
The source catalog's existing private `0600` mode versus canonical Git/clone
`100644`/`0644` is explicitly recorded, with identical bytes and the original
mode untouched. Task-local PySide6 6.11.2 matches system Qt and reuses system GI.
Fresh Debug/Quick3D-ON configure, six one-job target groups and the final
all-target build passed. After the QML repair, fresh configure and all-target
build also passed (1.60 and 6.36 seconds). Existing configure warnings remain
recorded separately from successful runtime import/rendering evidence.

The full available CTest coverage is **106 PASS, 0 FAIL, 0 skipped**, with one
worker and at most ten light tests per batch; every heavy/private test ran alone.
The final receipt selects 694.78 seconds of passing individual results. All
106 ran after the production QML repair. After the sentinel fixture correction,
the 81 unaffected results were reused and all 25 shared-lifecycle consumers
were refreshed. Three affected checks were refreshed again after parameterizing
the helper; the native commands used by the other passing matrices were proved
byte-identical. This is recorded reuse, not a claim of an additional single
full-suite invocation. Coverage includes all five audition groups, profiles,
100/125/150/200% scales, virtual-output recovery, resources, 2D/2.5D/optional 3D,
windows, folders, segments, overlays, status and UI accessibility.

A separate final **37/37-phase** native/free lifecycle run passed in **659.44
seconds**, using the default Qt render loop and unchanged assertions. It proves
creation/removal/recovery, ownership, exact unrelated native/free host
preservation, placement, visibility, fallback/restoration, conflict handling,
recreation, identities, real private shell/service restarts, content rotation
and idempotent detach. Its final logs contain no callback TypeError,
ReferenceError or binding loop. These are isolated KWin/Plasma Wayland checks
on Radeon 610M hardware with Mesa 26.2.4, Plasma/KWin 6.7.5 and Qt 6.11.2.
They do not claim personal-desktop or physical monitor/hotplug acceptance.

Two independent normalized exports are byte-identical: **518 source files,
519 archive members including SOURCE_CHECKPOINT.json**. Member bytes, modes,
UID/GID and baseline epoch match the manifest. The PKGBUILD is pinned to source
SHA-256 `bc462317d2e43103e2260df50f8e20e93d87944c978a9677452a6b864149dcb5`.
Native `makepkg --verifysource` and one-job Release `makepkg --cleanbuild
--noconfirm` passed; the latter took 368.28 seconds. Package SHA-256 is
`448a854b860b69fc2b35df66a45d0085c64e918baaabf238bd0fa9ebb621d07a`.
The final operational state/checklist documents are excluded from the source
export by its existing rules; all included source bytes remain frozen.

The disposable native package gate passed in **56.77 seconds**. All **209
installed files** match the CMake install manifest, bytes, modes and required
resource coverage on fresh install and upgrade. Installed actual-renderer tests
pass with **8/8** QtTest cases and **4/4** cases with Quick3D masked, including
initialization/cleanup. Exactly **15 Panel Presets and 15 Icon Presets** load
independently and remain immutable. Three installed private Wayland startups
(normal, Quick3D absent and upgraded) prove the installed executable bytes/PID
and unrelated-host preservation. Native pacman `0.1.0-1` to `0.1.0-2` upgrade,
obsolete-payload removal, configuration preservation, migration, future-version
refusal, backup/restore, offline recovery and uninstall all pass. Source trees
were hidden during installed checks; no host-global installation occurred.

The 57 visible schema editor fields, capability gates, runtime consumers and
non-schema actions retain their inspected references and passing UI/runtime
coverage. Those references are review aids; the passing tests and installed
checks provide the executable evidence. Documentation file/link/anchor checks,
shell syntax and `git diff --check` pass. The full acceptance mapping, exact
fourteen-path commit contents and version/tag plan are in
[RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md#task-0045-release-verification--2026-10-03).
[PLATFORM_MATRIX.md](PLATFORM_MATRIX.md#task-0045-release-regression-checkpoint--historical-2026-10-03)
records runtime observations and exact commands for physical cells marked
NOT EXECUTED because a second monitor/other GPU is absent or the owner desktop
cannot be mutated under this approval. Licensing/publication remains an
explicit owner decision; no executable failure is deferred to another task.

The earlier failed diagnostic setup/teardown produced two attributed native
SIGABRT dumps: private Plasma PID 159706 (QtDBus/libdbus assertion during
profiling teardown) and private Arch Dock PID 171652 (QGuiApplication startup
fatal in the invalid setup run). Their journals, stacks, process attribution
and surrounding logs are retained; the precise internal assertion mechanism
is not presented as established. Both exact root-owned files were removed
through KDE authentication. All 127 pre-existing dump files remain present and untouched by this task,
and no other new dump remains. Final passing gates produced no additional dump.

Final inspection found no task-owned private process. The exact temporary
workarea `/mnt/F/a45.jclt575i`, including clone, Debug/Release builds, task Python,
exports, probes and generated style caches, was removed after verified evidence
retention. Only six package deliverables, three compact verification deliverables
and the final STATE.json remain under ignored `build-codex-task-0045/`.
The verified raw-log archive preserves initial failures and the final successful
receipts; the two diagnostic heaps were removed. All six TASK-0043 package and
four TASK-0044 verification deliverables are byte-for-byte preserved.

At the historical TASK-0045 checkpoint, proposed `v0.1.0` was verified absent
locally and on origin, and commit/sync approval did not create a tag. The later
separate tagging/publication approval and actual correction target are recorded
in the latest release-closure boundary above.
The source-verification baseline remains `9584d204ecfcbf503ccd7e4322ac945424b5d610`.
The final release-evidence commit and remote parity are recorded in the retained
verification receipt rather than embedding this document's own commit hash.
All gates were serial with one build job and one test worker. No sub-agent,
parallel gate, owner-desktop mutation or unrelated cleanup was used.

### Previous completed boundary — TASK-0044

TASK-0043 is committed at
`a5bdd7933591bf75bf030cd144f7c9cea5e7fbae` and COMPLETE. Its exact system-owned
KWin crash dump was removed using the owner's authorized authenticated cleanup;
absence was verified before TASK-0044 planning. Its retained package deliverables
and historical **97/97** gates are unchanged. TASK-0044 Phase A is COMPLETE
with a fresh all-target build and **99/99** CTests. Phase B is COMPLETE with
a green all-target build and **106/106** CTests. The consolidated task is
**COMPLETE**: the owner authorized removal of the two recorded root-owned native
crash dumps, KDE authentication completed, and both files are verified absent.
The fresh final build and **106/106 CTests
passed**, followed by **14/14 affected private runtime checks** after a bounded
disposal-helper correction and a passing optional-3D-absent installed check.
The verified checkpoint is committed and synced as
`96d608a8440ddcb7bb8f886523fc7131b2b53e59`. The earlier
`c225dc6ae0abbeff21425d517546e67831ebc74b` checkpoint remains historical.
This final cleanup changes only closure documentation and the retained receipt;
the passing builds and runtime gates are reused without repetition. The owner
explicitly authorized committing and syncing these three closure documents.
Task cleanup is complete and required evidence is retained in four
deliberate verification files. See the
[TASK-0044 recovery record](#task-0044--configuration-recovery-phase-a--2026-10-02).
Earlier snapshots below retain their historical dates and results.

### TASK-0044 verification and cleanup checkpoint — 2026-10-03

The disposable upgrade logging blocker is resolved by the native Qt
`QT_FORCE_STDERR_LOGGING=1` fixture environment. Phase A's complete fresh build
and **99/99 CTests** passed in bounded serial batches (512.56 seconds summed
batch elapsed times; 512.52 seconds summed individual test times).

Phase B changes in this checkpoint include screen geometry/work-area/DPI
coalescing with profile-transaction deferral, utility-window work-area fitting,
compact scrollable Studio and icon editing, schema field accessibility, real
DockEntry keyboard actions, Canvas mask-image release, bounded generated render
history, idle overlay expiry shutdown, and the duplicate native Item `enabled`
property correction. Narrow checks passed for PanelWindow (including real
resource-backed Studio loading), Studio navigation/contracts, IconProperties,
DockEntry/motion, core accessibility, PanelSkin2D (including image release),
PanelRegistry (including protected references and the 64 MiB history limit),
OverlayModel (including idle/recovery), scene rotation and panel scenes.
The complete Phase B result is recorded below separately from these narrow checks.

`CMakeLists.txt` registers **106 CTests**: the previous 99, one core
accessibility test, and six private Wayland hardening groups. The owner explicitly
overrode the one-correction/one-rerun stopping rule for mandatory TASK-0044 gates:
unclear or pre-existing failures require bounded diagnosis and repair. Builds
and gates remain serial with one build job and one test at a time.

The energy-frame-cyan discrepancy is **Case B: unequal native scene-graph atlas
state**. The original 300x110 RGBA8888-premultiplied captures differed at exactly
one pixel (251,66), red 37 versus 36; green, blue and alpha were identical.
The differing fraction was 0.0030303%, maximum channel delta 1 and mean absolute
channel delta 0.000007575757575757576. Matching item geometry, renderer data,
DPR and frozen animation state and repeated stable captures ruled out timing
and translation. Replaying preceding cards produced A,B,A,A. A native Qt atlas
allocation control removed the difference. Reusing the engine/component while
creating a fresh QQuickWindow per card equalizes atlas construction state and
preserves exact equality and all energy effects. Four separate cyan and green
cards, expected active energy surfaces and cyan-versus-green positive controls
passed in three focused private Wayland runs with the normal atlas.

The original 100% matrix subsequently passed with all 30 cards, browser,
keyboard, Canvas release, native placement and audition checks. Genuine applet
callback errors found by its QML scan were repaired using Qt's explicit QObject
receiver context; the optional acceptDrops boolean is now normalized. The
resources group passed eight audition/cancel cycles with no orphan hosts and
8.37 MiB RSS growth, plus generated-history and idle-overlay regressions.

The 125% native discriminator proved that equal custom bounds of 320 produce
a 320x80 presented panel even when Plasma's raw content-length getter is 174.
The adapter reads fixed length from those native equal bounds; unequal-bound
readback still fails and rolls back. Browser selection failures came from a
pre-layout 121x0 ListView inside an oversized fixture. Actual client sizing,
queued GUI-thread frame notifications and verified click bounds repair that
fixture without relaxing selection or non-mutation assertions.

The matrix now resolves KScreen names from Qt stable identities and explicitly
separates both outputs. It verifies presented native dimensions and output on
all edges, rather than accepting overlapping-output or stored-intent evidence.
Removing the actual audition output exposed a surviving temporary host;
screen reconciliation now reuses the existing journaled cancel/rollback path.
A connected scale change remains ACTIVE as its positive control.

On 2026-10-03 the complete one-job Phase B build and **106/106 CTests passed**
in bounded serial batches (778.28 seconds summed individual test times).
The four scale groups passed at 100/125/150/200%, followed by hotplug/restart,
resources, diagnostics and installed startup. No mandatory test failure remains.

The owner resumed the same approved task from checkpoint
`c225dc6ae0abbeff21425d517546e67831ebc74b`. The fresh final configure used
Debug, Quick 3D ON, one autogen worker and the short task TMPDIR. The earlier
group-5 interruption (exit 143) was an owner-requested pause, not a compiler
failure. Its remaining objects, group 6 and the final all-target build passed.
The complete fresh final **106/106 CTests passed** in bounded serial batches
(778.26 seconds summed individual test times). Installed startup also passed
with the native Quick 3D module deliberately unavailable.

Cleanup inspection then found two native PlasmaShell dumps. Journal CWDs prove
TASK-0044 ownership, and native Mesa worker stacks place the faults after live
assertions completed, during final private-session disposal. The precise Mesa
memory-fault mechanism is not proved. The existing stop/reap helper now uses
SIGKILL only for final private PlasmaShell disposal; in-session restart,
Arch Dock and KWin retain SIGTERM. No live assertion, QML diagnostic scan or
crash reporting was weakened. The focused shortcuts regression passed, then
the all-target build and **14/14 affected private-session CTests passed**
(392.85 seconds summed). The other **92** fresh final results remain valid
because their executable inputs and helpers did not change. Optional-3D-absent
installed startup passed again. No additional task-owned crash was recorded.
The refreshed resource group measured 63,112 to 71,740 KiB RSS across eight
create/cancel cycles: **8.43 MiB growth**, below 64 MiB, and zero stale hosts.

Ordinary cleanup removed `phase-a`, `final`, `energy-evidence`, `test-python`,
`t`, `final-configure.log` and the raw teardown journal export. Process/path
inspection found no task-owned process retaining these paths. All six retained
TASK-0043 package files were checked byte-for-byte against their pre-cleanup
hashes and preserved. Required evidence remains only under
`build-codex-task-0044/verification-output`:

- `VERIFICATION.json`: source/build identity, complete test receipts, environment
  limits, native teardown evidence and cleanup status.
- `gate-logs.zip`: phase/final build and CTest logs, affected refresh, focused
  regressions and private runtime logs.
- `energy-evidence.zip`: original/control PNGs, renderer-state records and
  measured comparisons.
- `SHA256SUMS`: hashes of these three deliberate verification deliverables.

Both archives passed CRC checks and all retained file hashes verified. The
earlier administrator-cleanup boundary is now closed. After the owner explicitly
requested cleanup, `sudo -n rm` required authentication. The existing KDE
authentication agent completed the exact removal through
`pkexec --disable-internal-agent /usr/bin/rm -- <the two recorded paths>`
(exit 0). Both recorded files are verified absent:

- `/var/lib/systemd/coredump/core.plasmashell.1000.c4278a150c7e49efa5be334cee8f3140.12862.1791019615000000.zst`
- `/var/lib/systemd/coredump/core.plasmashell.1000.dd07aa413ed141a3960ee681d1112c26.1478932.1790972240000000.zst`

The verified Git checkpoint is
`96d608a8440ddcb7bb8f886523fc7131b2b53e59`; it matched the local upstream
tracking reference before this documentation-only closure update. The retained
receipt records the completed cleanup separately from the historical post-sync
audit. The original build/runtime receipts and diagnostic archives are preserved;
no passing build, phase gate or runtime check was repeated for this
authentication boundary. The no-diagnostic-artifact completion gate is PASS.
Physical multi-monitor/manual cells remain
**NOT EXECUTED** for the proved environment reasons and exact target commands
in [PLATFORM_MATRIX.md](PLATFORM_MATRIX.md). No global install, personal Plasma
mutation, delegated agent or overlapping gate occurred. TASK-0045 has not started.

**Evidence snapshot:** 2026-09-27 (Europe/Amsterdam). TASK-0036 Phase B has
been implemented under its retained approval and original plan. Both internal
phase gates now pass. Fresh ON, OFF and AUTO builds each passed **67/67 CTests**
after the final corrections, including private runtime fallback, Studio
interaction and service restart. No build or test failure remains in that
matrix. TASK-0036 is **COMPLETE**: the owner committed the verified work as
`80d0820`, and the two OS-managed diagnostic crash dumps have now been removed
under the owner's explicit authorization. Their absence was verified.

TASK-0035 completion is committed as `7b4706e`. The partial TASK-0036 and
TASK-0037 implementations and earlier repairs remain historical checkpoints.
TASK-0037 is **COMPLETE**: all three resumed phase gates pass, with a final
**68/68 CTests** including both private runtime gates. TASK-0038 Phase A is
**COMPLETE**: the bounded provider, panel-aware opening, five shared layouts,
native popup adapter and editor controls pass a fresh build and **71/71 CTests**,
including real native/free folder interaction. Phase B implementation is complete
and its fresh ON build/full **71/71 CTest gate passed (269.13 s)**. The separate
fresh final ON build and full suite passed **71/71 (272.29 s)**. The OFF
application/module build and three focused shared-QML checks passed. Cleanup
is verified. **TASK-0038 is COMPLETE**, committed by the owner as `983e5d7`.
The owner committed the initial TASK-0039 implementation as `4502565` and its
native-concealment repair/blocked checkpoint as `6892c13`. **TASK-0039 is COMPLETE
in the working tree**: both phases and the final current-tree **75/75 CTest gate
pass (285.59 s)**, including all private runtime checks with zero binding-loop
diagnostics. The segment cycle and required test synchronization defects are
resolved. Documentation and task-owned artifact/session cleanup are complete.
The earlier blocked reports remain historical evidence; see
[TASK-0039 final closure](#task-0039-final-closure--2026-09-27).
The [old handoff](TASK-0039_CONTINUATION_HANDOFF.txt) is explicitly retired.
The owner committed the TASK-0039 closure as `3ab470c`.

**TASK-0040 is COMPLETE** (2026-10-01): Panel Preset and
Icon Preset definitions, catalogs, user store, Studio pages and the exact
15 + 15 built-in libraries. A fresh build and the full **80/80 CTest gate pass
(321.17 s)**. See
[TASK-0040 implementation](#task-0040-implementation--2026-10-01). On the
owner's instruction the agent committed and pushed it.

**TASK-0041 is COMPLETE in the working tree** (2026-10-02), resumed from the
committed pause checkpoint `a3a522b`. The phase gate passes **87/87 CTests
(431.70 s)** and the separate fresh consolidated build/full suite passes
**87/87 (427.05 s)**, including all five private native/free audition matrix
groups. Cleanup is verified. Git closure remains owner-controlled. See
[TASK-0041 implementation](#task-0041-implementation--2026-10-02).
The [paused handoff](TASK-0041_CONTINUATION_HANDOFF.md) is historical.

Earlier context, retained because it explains two mislabelled commits: the
session that produced `f61c9ab` began from a tree whose subject `Task33` is
misleading, because that commit carries TASK-0032 Phases C and D rather than
TASK-0033. An independent audit against the consolidated task pack found the
TASK-0032 closure gaps recorded under "TASK-0032 corrective closure" below;
those gaps were closed and TASK-0033 was then implemented, as its own section
describes.

The TASK-0035 figures below come from fresh external Debug build directories;
none reuse the in-tree `build/` or `build-codex-task-0014` directories. Its
runtime evidence uses disposable private D-Bus, virtual KWin Wayland, and
private PlasmaShell sessions. The personal Plasma session was not mutated.
Earlier task sections retain their historical evidence and boundaries.

## Repository state

- Repository root: `/mnt/F/Arch Dock`
- Branch: `main`. The owner authorized committing and syncing the TASK-0041
  paused checkpoint on 2026-10-01. Earlier Git-state entries below are
  historical evidence.
- TASK-0036 implementation baseline:
  `a7f366506315e4009db2332ea8ddc6edc9cdf5ba`, subject `Arch Dock task 38`.
  That subject labels the prerequisite repair, not TASK-0038 implementation.
- Previous continuation `HEAD`: `80d0820b76f9c816be75f19fb7450aab1b5a1f0e`, subject
  `Arch Dock task 36 repair`, committed by the owner. The working tree was
  clean at the cleanup continuation, matching the preceding passing TASK-0036
  matrix. The owner subsequently committed TASK-0037 closure, TASK-0038 Phase A and the
  initial segment parser as `032420afeee8ac2453f4896a57da7c2e60cf213e`.
  That was the TASK-0038 continuation baseline, now historical.
- Previous TASK-0039 continuation entry and unchanged final `HEAD` (historical):
  `45025655bbcd7369b44b07c26549e0be6657f76d`, subject
  `Arch Dock - Complete Arch Dock content systems`. The entry tree was clean;
  the owner's commit preserves the preceding 30-file TASK-0039 implementation.
  The owner then committed that continuation's repair/tests/blocked-result
  documentation as `6892c13`.
- Final TASK-0039 continuation entry and unchanged final `HEAD`:
  `6892c13e7827f4990c71ed2fe49a6462273ccc72`, subject
  `Arch Dock - Complete Arch Dock content systems`. Entry was clean. The
  unstaged changes contain the segment correction, bounded test-harness fixes,
  regression and verified closure documentation. No Git writes were performed.
- `build-codex-task-0014/` is still tracked at `HEAD`. It is a build
  directory committed by mistake in `75232e5` and must be removed with
  `git rm -r build-codex-task-0014`; `.gitignore` now excludes every
  `build-codex-*/` directory so the mistake cannot recur.

### Commit-to-task mapping

Some commit subjects differ from their content, so this table, not the
subject line, records which commit carries which task.

| Commit | Subject | Actual content |
| --- | --- | --- |
| `f30d5fa` | `task22` | TASK-0022 |
| `c55a89b` | `task23` | TASK-0023 |
| `2ed1a91` | `task24` | TASK-0024 |
| `a7f818e` | `task25` | TASK-0025 |
| `c2473ff` | `task26` | TASK-0026 |
| `c857fd7` | `task27` | TASK-0027 |
| `dfb315c` | `task28` | TASK-0028 |
| `a34fbd9` | `task28` | TASK-0029 Phases A to D |
| `ef84d86` | `task29` | TASK-0029 corrective session |
| `8e54b2b` | `TASK29.` | TASK-0029 documentation closure |
| `c9fbbbf` | `task30` | TASK-0030 |
| `c400364` | `task31` | TASK-0031 |
| `026b8b3` | `task32` | TASK-0032 Phases A and B |
| `f61c9ab` | `Task33` | TASK-0032 Phases C and D |
| `b13c1c9` | `Arch Dock Codex task package audit` | TASK-0032 corrective closure and TASK-0033 |
| `197a515` | `Arch Dock task 34` | TASK-0034 Phase A, first part |
| `abc6dfd` | `Implement baked 2.5D ring, octagonal, and arc themes` | TASK-0034 completion |
| `e84e1bf` | `Arch Dock task35` | TASK-0035 Phase A and partial Phase B checkpoint, committed by the owner |
| `7b4706e` | `Arch Dock task 35 full` | TASK-0035 completion |
| `0211331` | `Arch dock task 36` | TASK-0036 partial Phase A; visual-motion failure unresolved, Phase B not started |
| `5f41ced` | `Arch Dock - Record TASK-0036 visual-motion blocker` | TASK-0036 resumed diagnostics and blocked-state documentation; no closure |
| `a7f3665` | `Arch Dock task 38` | Prerequisite watcher/action and fallback-test repairs; green ON suite, no TASK-0038 folder/segment implementation |
| `80d0820` | `Arch Dock task 36 repair` | TASK-0036 Phase B, final texture-lifetime repair and passing ON/OFF/AUTO matrix; administrative cleanup subsequently verified |

The TASK-0036 checkpoint contains the previous session's 29 changed files,
956 insertions and 64 deletions. It does not close that session's BLOCKED result.
- Codex did not stage, commit, push, globally install, or mutate the personal
  Plasma session.

## Inspected platform

- Arch Linux, rolling release
- Kernel `7.2.6-arch2-1` for the TASK-0036 resume
- KDE Plasma and KWin `6.7.5-1`
- Runtime verification: private virtual KWin Wayland and real PlasmaShell applets
- Qt base `6.11.2-3`
- Qt Declarative `6.11.2-2`; optional Quick3D `6.11.2-1`
- KDE Frameworks Core Addons, Kirigami and KIO `6.30.0-1`

These versions describe the inspection and verification host. TASK-0028 and
TASK-0029 used external build directories and disposable staged private
sessions; neither task globally installed, restarted the live PlasmaShell, or
ran Arch Dock against the personal desktop session.

## Documentation authority map

- Architecture and implementation requirements:
  [MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md](MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN.md)
- Current implementation status: this file
- Preset contract: [PRESET_SYSTEM_SPEC.md](PRESET_SYSTEM_SPEC.md)
- Target repository structure:
  [TARGET_STRUCTURE_TREE_V2.md](TARGET_STRUCTURE_TREE_V2.md)
- Release gates: [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md)
- Theme Package v2 normative contract:
  [THEME_PACKAGE_V2.md](THEME_PACKAGE_V2.md)
- Source sample provenance and installation boundary:
  [SOURCE_ASSET_CATALOG.md](SOURCE_ASSET_CATALOG.md)
- Version-1 compatibility and installed theme-package behavior:
  [theme-packages.md](theme-packages.md)
- Shared rendering module, geometry, scene, and fallback contract:
  [shared-renderer.md](shared-renderer.md)
- Icon Style Package v1, selection, overrides, and live editor contract:
  [ICON_STYLE_PACKAGE.md](ICON_STYLE_PACKAGE.md)
- Animation profile v1, motion vocabulary, requested presets, frozen ids, and
  motion safety: [ANIMATION_PROFILE.md](ANIMATION_PROFILE.md)
- Native and free Plasma ownership, recovery, and rollback safeguards:
  [plasma-lifecycle.md](plasma-lifecycle.md)
- Canonical baseline checkpoint and task handoff:
  [BASELINE_CHECKPOINT.md](BASELINE_CHECKPOINT.md)
- Fresh configure, build, test, and stage-install evidence from TASK-0004:
  [audits/BASELINE_BUILD_REPORT.md](audits/BASELINE_BUILD_REPORT.md)
- Baseline evidence from TASK-0001 and TASK-0002:
  [audits/BASELINE_AUDIT.md](audits/BASELINE_AUDIT.md)
- [AUTONOMOUS_PROGRESS.md](../AUTONOMOUS_PROGRESS.md) and
  [implementation-audit.md](implementation-audit.md) are preserved historical
  snapshots and are explicitly marked as superseded.

`MASTER_ARCHITECTURE_AND_IMPLEMENTATION_PLAN_V2.md` is only a compatibility
pointer to the canonical architecture document; it is not a second copy or a
second authority.

## Verified current implementation

The following implementation statements use the evidence snapshots identified
above. Lifecycle statements marked as runtime-verified were exercised in a
disposable private Plasma Wayland session, not inferred from inspection.

- The installed `ArchDock.Rendering` 1.0 module contains the canonical
  `LayoutEngine`, host-neutral `PanelScene`, layered `IconScene`, shared
  `RunningIndicator`, `LivePanelPreview`, surface loader, `PanelSkin2D`, alpha
  hit mask, and safe procedural fallback. Geometry contract, boundary,
  deterministic state, orientation, fixed-cap scaling, content-safe placement,
  mask, preview-state, snapshot, and offscreen visual tests cover the shared
  engine.
- `PanelScene` accepts normalized definition, runtime state, ordered entries,
  host capabilities, theme/icon/motion inputs, and screen/work-area bounds. It
  exposes visual/effect bounds, safe input, reveal and popup/preview anchors,
  renderer status, and entry geometry. Missing or invalid themes and unavailable
  renderer tiers use procedural 2D with a truthful fallback reason.
- The production native and free `org.archdock.dock` hosts instantiate one
  `PanelScene` with the existing interactive `DockEntry` as host delegate.
  Launch, context-menu, drag/drop, reorder, edit-mode, and free/native input
  policy remain intact. Separate Canvas and layout branches and the dormant
  service-side `FreePanelWindow` are gone.
- `DockEntry` now uses one shared `IconScene` inside independent magnification
  and motion layers while its outer logical root, pointer region, and drop area
  remain fixed. The former applet-local `IconVisual` and `RunningIndicator`
  implementations are removed.
- `IconStyleDefinition`, `IconStylePackage`, and `IconStyleStore` implement the
  non-executable version-1 icon-style contract with bounded metadata, explicit
  states, canonical path containment, catalog/package identity checks, and
  deterministic fail-closed fallback to `plain-original`. Icon-style selection
  is independent of the panel theme.
- Six built-in packages are installed: the `plain-original` fallback plus
  `metallic-blue`, `metallic-red`, `neon-green`, `neon-orange`, and `dark-orb`.
  The five production families are original, asset-free procedural recipes
  with stable IDs, capability declarations, and deterministic preview inputs.
  They frame the real application glyph and do not claim a mapped replacement
  pack or a user-wide icon theme.
- `IconScene` resolves the package projection and explicit icon state, then
  keeps rear/base, glyph, front, indicator, and status roles independently
  addressable. Disabling an entry's tile hides tile/pedestal layers without
  discarding the selected style's safe glyph treatment, state, or indicator.
  The live native/free hosts and Studio previews use this same rendering path.
- `PanelDefinition` persists bounded per-entry overrides keyed by stable
  desktop-entry, application, or canonical free-entry identity. The backend
  atomically resolves override, panel default, and safe fallback; pinned and
  running representations converge on one desktop identity; Reset removes only
  that identity's override; existing legacy custom-icon input remains readable.
- Supported live entries expose **Icon Properties…** through the production
  `DockEntry` context menu. The production editor applies and resets through
  revision-checked `PanelWindow` transactions, preserves hidden future fields,
  reports transaction errors, and discards its draft on Cancel or window close.
  Running-only/transient entries, edit mode, active drag, and disabled input do
  not open the editor.
- Panel Studio renders its active transaction draft and all available built-in
  theme cards through `LivePanelPreview`. Horizontal-native, vertical-native,
  free, open/collapsed, hover, and explicit icon states are supported. Renderer
  tier and fallback reason are shown from `PanelScene`; preview controls have no
  desktop-audition or persistence path, and Apply remains the only transaction.
- `src/main.cpp` creates a Qt Quick/Kirigami application, owns the session-bus
  name `org.archdock.ArchDock`, and delegates panel behavior to `PanelManager`.
- Native edge panels use Plasma containments. Creation records an Arch Dock
  ownership token and panel id, attaches an `org.archdock.dock` visual applet,
  removes matching legacy `org.archdock.control` applets, and checks ownership
  before moving or presenting a containment. Missing visible hosts are recreated,
  hidden missing hosts remain detached, token-bound stale ids are rediscovered,
  and repeated recovery converges without duplication. These paths are
  runtime-verified in the isolated lifecycle harness.
- Permanent native removal requires both the exact containment ownership token
  and the expected dock-applet association. The destructive Plasma script
  rechecks both immediately before removal and verifies absence afterward.
  Wrong-token and wrong-renderer runtime cases preserve the containment, applet,
  and full registry record; `removePanel` also preserves the record on refusal.
- Temporary hide/show changes verified Plasma presentation on the existing
  containment and keeps the containment id, dock applet id, and ownership token
  stable. It does not implement visibility by deleting and recreating the host.
- Free panels are **not retired**. Panel Studio and the Plasma layout-template
  route converge on one backend transaction that creates a real
  `org.archdock.dock` desktop applet, verifies its exact panel id/token/type and
  non-bootstrap state, persists and reads back its containment/applet/token and
  screen association, removes any verified temporary bridge, and completes the
  record only after a final host readback. Both routes are runtime-verified and
  do not produce record-only success.
- Free creation rollback covers record allocation, bridge verification, host
  preflight and mutation, host verification, association persistence/readback,
  bridge cleanup, final readback, and completion persistence. A verified removed
  candidate permits record discard; uncertain rollback retains a token-bound
  recoverable record instead of losing ownership evidence.
- Free-host recovery distinguishes zero, one, and multiple exact token matches.
  Zero safely detaches the record; one verifies and rebinds it; multiple matches
  preserve every applet and record a non-mutating conflict. Repeated detached or
  unique synchronization is idempotent. Output disconnect/restore and a real
  private PlasmaShell restart preserve one verified host per record.
- Free removal rediscovers and re-verifies the unique owned applet, removes it,
  verifies absence, and only then removes the record. Missing detached hosts can
  remove their records without a Plasma mutation. Failed adoption, conflict, and
  removal paths preserve unrelated desktop applets; these paths are
  runtime-verified with an explicit unrelated free-host sentinel.
- `ThemeDefinition` and `ThemePackage` implement the version-2 manifest model,
  strict resource and path bounds, typed capabilities/assets/states/layers/
  slices/regions/masks/references, structured diagnostics, and deterministic
  version-1 adaptation. Invalid or incompatible packages fail closed to the
  safe procedural renderer; package data never executes code.
- Three original production chassis packages are installed with stable IDs:
  `sci-fi-chassis-dark`, `sci-fi-chassis-red`, and `sci-fi-chassis-blue`. They
  have separate surface/glow/mask assets, fixed caps and scalable centers,
  content/effect/input bounds, horizontal normal/open/collapsed states, exact
  native/free capability metadata, default icon-style references, and
  deterministic Studio preview inputs. The package-backed catalog entries are
  selectable through the same atomic settings transaction as procedural themes.
- Four original production energy packages are installed with stable IDs:
  `energy-frame-cyan`, `energy-frame-green`, `energy-frame-orange`, and
  `energy-frame-purple`. Their blank-canvas SVG packages separate surface,
  frame, glow, energy-overlay, highlight, and input masks; record exact output
  hashes and review evidence; expose deterministic preview seeds; and contain
  no source-screenshot pixels. The four broad-concept references remain
  non-installable and redistribution-unknown.
- `PanelSkin2D` renders each energy state's manifest layers in deterministic
  order, colorizes only declared masks, bounds tint and glow inputs, supports
  explicit open/closing interpolation without owning the presentation state
  machine, and stops/reset its one overlay phase when hidden or under reduced
  motion. Hover selects a distinct visible glow/overlay state. Missing optional
  layers skip safely; invalid required inputs fall back to procedural 2D.
- Energy input remains bound to the package alpha mask rather than the visual
  glow rectangle. Direct `PanelScene` and `LivePanelPreview` energy scenes share
  state, layer order, glow, reduced-motion, containment, and surface-pixel
  output for normal, hover, open, and collapsed cases.
- The source-asset catalog contains exactly 138 independently hashed reviewed
  records: 122 panel screenshots and 16 icon reference sheets. Every record is
  source-only, reference-only, redistribution-unknown, opaque, and blocked from
  installation. Strong 2D, energy, ring/polygon, arc, and icon-reference groups
  are stable catalog classifications, not license or production-readiness
  claims.
- `ThemeAssetProcessor` produces bounded deterministic managed derivatives and
  metadata without overwriting source files. It verifies real pixel alpha,
  prevents source/output aliasing and path escape, validates requested scales,
  records hashes, and preserves manual-cleanup requirements instead of claiming
  automated logo or placeholder removal.
- Theme capabilities now participate in the existing host/theme/renderer/
  platform intersection. Unsupported layouts, rotation, presentation features,
  and renderer tiers remain hidden or fall back with an explicit reason; a flat
  image cannot satisfy true-3D capability.
- The embedded catalog contains five procedural, three chassis, four energy,
  three perspective, and one mesh theme: 16 themes in total. The
  later Panel/Icon preset system and its exact 15+15 libraries remain a
  separate contract.
- CMake declares the application, QML, theme, and icon-style resources, all
  eleven installed Theme v2 packages, all six installed Icon Style v1 packages,
  Plasma applets and templates, D-Bus and systemd metadata, source-only catalog
  exclusions, and the CTest suite whose exact count is recorded with the most
  recent task gate below.

## Known defects and incomplete behavior

- The installed systemd user unit starts `%h/.local/bin/arch-dock`, while the
  application and D-Bus metadata invoke `arch-dock` from `PATH`. The startup and
  installation strategy is not yet aligned.
- The optional base mesh renderer and its quality controls are implemented.
  Studio exposes them only with a supported free host, validated scene theme,
  and available consumer backend. `surface3D` remains internal. Whole-panel
  3D rotation, true Y-axis icon motion, emissive hover, and 3D part animation
  remain TASK-0036 work; unsupported rotation controls are hidden and rejected.
- Per-entry `animationProfileReference` is validated and persisted but remains
  intentionally hidden. The animation engine delivered by TASK-0030 and
  TASK-0031 selects one profile per panel; routing a per-entry reference into it
  is not implemented and no active task in the pack claims it.
- The shipped icon-style families preserve the original application glyph. No
  complete mapped-replacement icon pack or global icon-theme mutation is
  implemented by TASK-0029.
- Physical monitor disconnect/reconnect behavior and the personal desktop
  session were not exercised. Output fallback and restoration were verified on
  two virtual KWin Wayland outputs in the disposable lifecycle session.
- The chassis and energy families are horizontal only. Vertical or other
  layouts fall back to procedural 2D; no vertical package artwork is
  fabricated.
- The alpha mask narrows Qt Quick item containment. It is not evidence of
  compositor-wide click-through outside the applet's enclosing window, so the
  native/free host profiles do not claim `nonrectangular-input` from this work.

## Historical planning boundary after TASK-0029

The master plan and preset specification describe target behavior. The
following named systems remain outside TASK-0029:

- version-2 panel and icon preset catalogs, including the required 15 panel and
  15 icon presets
- `PreviewSession` audition/rollback semantics
- `ProfileStore` profile persistence and switching
- the complete true-3D, notification, accessibility, performance, packaging,
  migration, and release-validation work required by the architecture plan

Chassis, energy, and icon-style states are deterministic renderer inputs only.
TASK-0028 adds bounded energy-layer motion and explicit interpolation input;
TASK-0029 adds icon-state styling and a future animation-profile reference.
The hover/open interaction controller these tasks deferred is delivered by
TASK-0032 and described in its own section below; host geometry animation and
the true-3D renderer remain outside it, and the animation-profile engine
arrived with TASK-0030.

## Verification boundary

TASK-0029 Phase A passed its icon-style package/store and panel-registry gate
2/2, then all 46 CTests. Phase B passed its package, asset, scene, visual,
preview, parity, and private rendering smoke gate 7/7, then all 46 CTests.
Phase C passed its model, stable-identity, transaction, registry, dock-model,
panel-window, and QML override-resolution gate 7/7, then all 46 CTests. Phase D
passed its menu/editor, popup/edit/drag guard, state, template, public
interaction, and private runtime gate 6/6, then all 46 CTests. The public
interaction regression uses Qt pointer/key events against the production
`DockEntry` and production
editor, including Apply, Cancel, window-manager close, and Reset; the staged
private host independently verifies installed native/free rendering and
live-scene refresh after backend apply/reset.

The private runtime uses a temporary install prefix, XDG roots, D-Bus daemon,
virtual KWin Wayland compositor, and PlasmaShell. Harness traps own cleanup,
and none of this evidence claims a physical desktop or hardware acceptance
test. A separate final fresh Debug configure/build passed, followed by all 46
CTests including the 65.87-second staged private runtime smoke. The generated
repair and final build directories were removed after verification.

### 2026-09-03 corrective session

Two corrections were applied on top of `a34fbd9` and were committed by the
owner as `ef84d86` (`task29`).

Phase A closed a contract gap in package validation. Renderable layer assets
and mapped-replacement assets were accepted on path, size and digest alone
and were never decode-probed, so an undecodable file could enter the asset
table, the content digest and the runtime projection. `IconStylePackage` now
decode-probes those assets with `QImageReader` before insertion into
`assetPaths`; optional 3D `mesh`/`material` resources remain bounded
path-only. `icon-style-package-test` and `icon-style-asset-test` gained the
`Qt6::Gui` link this requires.

Phase B closed a renderer truthfulness gap. The parser accepted `tinted`,
`monochrome` and `mask`, and the documentation described all three, but the
renderer honoured none of them. `IconStyleResolver` now resolves the effective
treatment behind an explicit compatibility gate and reports a named fallback
reason; `IconScene` renders monochrome through Kirigami's native mask path and
instantiates `MultiEffect` only when a tint or an asset-backed mask is
actually requested. Two defects were fixed with it: the plain fallback left
the `disabled` state completely undimmed, and a style asset that failed at
load time vanished silently while the remaining layers still drew. Any style
asset failure now withdraws the whole treatment in favour of the plain
original glyph.

Phase C and Phase D required no source change and were verified only.

Fresh evidence from this session: baseline full CTest 46/46 before any edit;
Phase A focused 19/19 with a fresh clean configure/build and full 46/46;
Phase B gate full 46/46; Phase C focused 7/7; Phase D focused 5/6. A single
consolidated fresh configure/build then passed with full 46/46, including the
65.55-second staged private runtime smoke.

`rendering-import-smoke` is flaky in this environment and must not be read as
unconditionally green: across this session it passed four times and failed
twice, in two different modes. One failure was a timeout-margin miss against
its `TIMEOUT 120` budget at a typical runtime near 66 seconds. The other was a
`stale-revision` conflict while selecting an icon style on the private native
host. That second failure is a race in the harness, not the product: the
script reads `settingsRevision` and then calls
`applyPanelSettingsTransaction` with it, while applying a setting triggers an
asynchronous `renderer-notification` publish that can advance the revision
between the two calls. The product rejected the stale draft correctly. The
same unguarded read-then-write pattern appears in the script's Icon Properties
section. The cause was not introduced by these corrections and was left
unmodified, since weakening or rewriting the test was out of scope.

The retained TASK-0028 dependency evidence follows.

Corrective closure first reconstructed a clean `dfb315c` source snapshot so the
then-unaccepted TASK-0029 work could not influence TASK-0028 results. Phase A
reparsed every energy JSON file, validated every SVG as XML, matched every
production-record output hash, reproduced the review artifact hash, completed
a fresh serial Debug build, passed the focused provenance/package tests 3/3,
and passed all 38 CTests.

The energy-family contact sheet covers 352x64, 720x96, 1200x160, and 1500x200.
Its freshly reproduced SHA-256 is
`0a9bc4a12cbfe59e858e37c50ae36e65783f34b7ac2ef4ba83c2ab8f49459ea7`.
Visual inspection found distinct cyan, green, orange, and purple packages with
stable caps, open content regions, inset effects, no UI remnants, and no
third-party marks. Automated scenes cover every variant at 100%, 150%, and
200%, unsafe tint fallback, normal/hover/open/collapsed layers, explicit
interpolation, paused hidden overlays, and static reduced motion.

Phase B used a second fresh serial Debug build. Its focused gate passed 11/11
and the complete suite passed 38/38. The isolated renderer smoke installed into
a disposable prefix and used its own D-Bus daemon, virtual KWin, PlasmaShell,
XDG roots, native panel, and free desktop applet. Before starting the service,
the exact Wayland-only pixel case rendered the staged module and proved visible
normal/hover/open/collapsed differences, inset effect pixels, a distinct static
reduced-motion frame, and item-level active-region containment. The private
native/free hosts then resolved the cyan package through normal atomic settings
transactions.

The TASK-0028 Phase C staged smoke requires all four exact installed packages
and rejects source/reference files. It selects green, orange, purple, and
finally cyan on both private hosts, requiring `skinned2d`, ready projection,
`dynamic-glow`, the exact package ID, and exact staged manifest on every
iteration. All private resources are trap-owned and removed by the harness.
Its fresh Phase C and final build/test results remain dependency evidence; they
do not substitute for the TASK-0029 gates above.

This evidence is representative of the required Arch Linux, Plasma 6, Qt 6,
Wayland integration, but it remains an isolated virtual session. It does not
claim hardware-specific monitor behavior or mutation of a user's live desktop.

## TASK-0030 — animation-profile engine

AD-0011 is now data-driven. Icon motion is described by validated animation
profiles rather than by a growing conditional block in `DockEntry.qml`.

### Phase A — schema and validator

`src/model/AnimationProfile.*` defines the profile, its track model and the
target, trigger and property vocabularies from master plan sections 14.2 to
14.4. `src/animation/AnimationProfileCatalog.*` parses and validates catalogs
fail-closed: any error loads no profiles at all. Renderer requirements are
checked against the capability resolver's own tier vocabulary rather than a
second copy of it.

`click` and `launch-succeeded` are separate triggers. The pre-migration
umbrella value `launch` is not in the vocabulary and is mapped to
`launch-requested` by the compatibility layer, so a click can never present
itself as a verified launch. Every profile must declare a reduced-motion
behaviour; an absent declaration is a `missing-reduced-motion` error, a
substitute must exist, and a substitute may not itself substitute.

Nine fixtures in `tests/fixtures/animation-profile-v1/` cover one validator
outcome each and are indexed by `fixture-index.json`. `animation-profile-test`
asserts every fixture produces exactly its declared diagnostic code and that a
conflict diagnostic points at the second writer (`/tracks/1`).

### Phase B — dispatch and composition

`qml/ArchDock/Rendering/AnimationProfileRuntime.js` holds the event vocabulary
and the composition rules, with no QML types, so composition is testable
without a window. `IconMotionController.qml` runs the accepted tracks and is
the only track runner; `MotionTrackRunner.qml` animates one track each.

A runner animates only its own `progress` property and never writes to a scene
item, so two runners cannot race. The controller composes their values into
per-target channels and the host binds the result. Two writers on the same
target and property are permitted only when the outcome is deterministic —
both additive, or distinct priorities. Anything else is rejected, both
claimants are withdrawn, and the clash is reported through `conflictDetected`.

Composition is order-independent: presenting the same profiles in reverse
order produces an identical track list. `DockEntry` dispatches hover enter and
exit, press, click, launch-requested, drop-entered, drop-committed and
running-stopped from the real pointer and state handlers; hover-hold, running,
urgent, drop and reveal are carried as state.

The controller writes only visual transforms on a layer nested inside the
entry, so `root` keeps its logical size and input region. Two regression tests
drive a travelling translation and a 2x scale and assert the entry's width,
height, position and `logicalInputRegion` never move.

### Phase C — migration

`data/animation-profiles/builtin-animation-profiles.json` carries 18 built-in
profiles covering all 19 selectable `iconAnimation` values; `scale` is a legacy
alias of `pulse`. Amplitude, cycle length and per-entry stagger are preserved
exactly. Translation is expressed in logical units, reproducing the previous
`baseSize * 0.22` arithmetic at any icon size, and intensity scales endpoints
about each property's resting value so a symmetric tilt narrows towards its
centre exactly as before. Two multi-leg effects are expressed as a single
alternating track carrying the same shape: `elastic` through `out-elastic` and
`spring` through `out-back`. The full migration table is in
[ANIMATION_PROFILE.md](ANIMATION_PROFILE.md).

Seven effect families were migrated one at a time, each verified before its
legacy branch was deleted: orbit, oscillating rotation, continuous rotation,
scale, staggered translation, translation, and glow. `DockEntry.qml` fell from
392 to 350 lines and now contains no effect name, no per-effect conditional
and no animation of its own. `dock-entry-motion-contract-test` fails the build
if any of those reappear; it was confirmed non-vacuous by running it against
the pre-migration file, which it rejects.

The catalog reaches QML through `PanelRegistry` and the panel configuration
that `PanelWindow` publishes, and is compiled in as a Qt resource so a
stage-install without data files still resolves every profile. Because the
effect and the event that starts it remain separate user settings, `DockEntry`
overrides each bound profile's nominal trigger with the configured one.

`animation-profile-test` asserts that the catalog's profile ids plus legacy
names are exactly the set of values the `iconAnimation` settings field offers,
in both directions, so the editor cannot list an unvalidated preset and the
catalog cannot hold one the editor cannot select.

### Verification boundary

Baseline before any edit: fresh configure, build and 46/46 CTest. Phase A gate:
fresh build and 47/47. Phase B gate: fresh build, 48/48, and the isolated
`rendering-import-smoke` at 65.75 s. Consolidated gate figures are recorded
with the final result.

One focused correction was made during Phase B after a diagnostic probe proved
the cause. Binding the controller's `sceneVisible` to the entry's `visible`
made the controller inert in any headless host and imported concealment gating
that TASK-0031 owns, so the binding was removed; the controller keeps the
capability and it remains covered by a test. In the same correction,
`running-started` was confirmed to be a state trigger, matching the legacy
`trigger === "running"` meaning, so only the stop transition is dispatched as
a discrete event.

The known `rendering-import-smoke` flakiness recorded for TASK-0029 was not
modified and remains a property of that harness, not of this work.

Reduced motion currently rests every icon effect, which is exactly the
pre-migration behaviour; each profile declares `mode: "none"` explicitly.
Richer per-preset reduced-motion substitutes, the requested new motions, and
verified launch-succeeded and launch-failed events from `DockModel` are owned
by TASK-0031 and were deliberately not implemented here.

## TASK-0031 — requested icon motions, launch truth, and animation safety

AD-0011 is closed. The motions the master plan asks for by name exist as
catalog data, launch events report only what the platform actually confirmed,
and continuous motion stops when nobody can see it.

### Phase A — slow Y turn, jump, and shake

`qml/ArchDock/Rendering/MotionChannels.js` is the single mapping from composed
channels to concrete transforms. It reads the entry's own geometry, so
`translate-normal` and `translate-tangent` have a meaning rather than a fixed
screen direction, and it is free of QML types so the mapping is provable
without a window.

`LayoutEngine.entryGeometry` gained an optional trailing `edge`. A linear row
has no outward side of its own, so the edge supplies one: a bottom panel jumps
up, a top panel down, a left panel right, a right panel left, while radial
layouts keep their own path-derived normals. Omitting the edge reproduces the
previous values exactly, which is why every existing geometry, scene and parity
test passed unchanged; `PanelScene` supplies the real edge, so popup anchors on
top and right panels became edge-correct as a side effect.

`slow-y-turn` is a turn, not a spin. `MotionChannels.turnMatrix` composes a
translation to the centre, a rotation about Y, a perspective divide and a
translation back, so the card narrows and its receding edge foreshortens; the
highlight is derived from the same angle and cannot fall out of step. No mesh
is involved. `IconScene` applies motion per layer through `glyphMotion`,
`tileMotion` and `indicatorMotion`, all defaulting to an exact identity.

`PanelScene` publishes a per-entry `effectAllowance` from the effect bounds.
Every translation contribution is summed and the sum is clamped once, so no
contribution is silently dropped and nothing draws outside the reserved margin.
The live host additionally reserves headroom equal to the furthest the bound
profile can travel.

### Phase B — enlarge, neighbour influence, spiral, and orbit

`path-radius` joins the property vocabulary as the companion distance for
`orbit` and `spiral`, so a path motion is an angle and a radius rather than two
hand-synchronised sweeps. Both rest at zero and every offset is computed from
absolute channel values, never accumulated, so no number of cycles can leave an
icon drifted off its anchor. `orbit` was re-expressed on those primitives at
its original 0.22 amplitude and 340 ms cycle.

`motion.magnifyRadius` and `motion.magnifyFalloff` make neighbour influence
configurable; the defaults, 2.4 and `linear`, are the historical curve, so an
existing panel magnifies exactly as before. Every falloff peaks at 1, decreases
monotonically and is exactly 0 at and beyond its reach. The influence is
visual only: it never changes an entry's logical size, position or hit area, and
no user-facing physical-rearrangement setting was introduced, so there is no
control that claims to move icons and does not.

### Phase C — launch truth, reduced motion, and safety

`DockModel::activateApplicationOutcome` separates a verified start from an
unverifiable request. Starting a program either succeeds or fails and QProcess
reports which; raising an existing window is handed to the compositor, which
never answers, so that outcome is `requested` and runs nothing. The new
`activateDockEntryOutcome` D-Bus method carries the answer to the applet, which
dispatches `launch-succeeded` or `launch-failed` only from a proved outcome.
The bool-returning `activateDockEntry` is unchanged.

Every built-in profile that moves something now substitutes a static glow under
reduced motion instead of resting silently; only `none` rests. The renderer
takes the glow from whichever layer declares it, so a glyph-targeted preset
reaches the same feedback an icon-targeted one does.

`PanelScene.sceneConcealed` reports that the panel cannot be seen, derived in
the live host from item visibility and opacity and from window visibility, which
is what a Plasma auto-hide panel changes. Nothing is inferred from focus. While
it holds, the controller withdraws every track. Trigger and concealment cycles
return to exactly the profile's own track count, so runners are never
accumulated.

The 23 built-in profile ids are frozen and asserted, because the built-in Panel
and Icon Presets that TASK-0040 must deliver will name motion by id.

### Verification boundary

Baseline before any edit: fresh configure, build and 50/50 CTest. Phase A gate:
clean configure, build and 52/52. Phase B gate: clean configure, build and
52/52. Consolidated gate: clean configure, build and 52/52, with
`rendering-import-smoke` at 65.68 s in a disposable private D-Bus, virtual KWin
Wayland and private PlasmaShell session, and a `DESTDIR` staged install that
carries `MotionChannels.js` and the updated catalog. The staged prefix was
removed afterwards. No personal desktop session was contacted, nothing was
installed globally and PlasmaShell was not restarted.

Two focused corrections were made, each after its cause was proved. A test
assertion required `turnMatrix` to be literally the identity at rest; the matrix
carries an inert perspective row at `z = 0`, so the assertion was replaced by
the stronger one that every point of the card maps to itself. Separately, the
controller's `channels` binding was invalidated by revision bumps fired from
`Instantiator` object creation during its own evaluation; Qt broke that loop and
left a stale channel, which withdrew a track without clearing its value. The
bumps are now deferred and `channels` depends on the track set directly. The
binding-loop warning is gone from the whole suite.

Two runtime facts are proved by automated tests rather than by observation on a
live desktop: that the Y turn reads as a vertical-axis turn and that a jump is
seen to leave the panel edge. The isolated session renders both but asserts
neither. Observing them on the personal desktop would require
`plasmashell --replace` against the live session, which the contract forbids
without explicit authorisation.

## TASK-0032 — panel presentation states, guards, opening mechanisms, and host integration

Phases A and B are the owner's commit `026b8b3`. Phases C and D are this
session's work and are described here.

### Closure gap that reopened the task

`026b8b3` delivered the state machine (`PresentationStates.js`,
`PanelPresentationController.qml`) and the interaction guards, with 30 + 18 + 6
tests covering them. It did not deliver Phase C or Phase D: there was no
`PanelMotionController.qml`, `collapseMechanism` and `collapseAxis` had no
consumer anywhere in the tree, and the controller instantiated in
`plasma-dock-widget/contents/ui/main.qml` had no reader — `sceneRuntimeState`
never carried its output, so the live surface was permanently `open` whatever
the panel was configured to do. The gap was found while verifying predecessor
closure for TASK-0033 and was reported before any planning of that task.

### Phase C — opening mechanisms and panel glow

- `PanelMotionController.qml` is the single source of presentation geometry.
  Given a mechanism, an axis and a progress it returns per-role offset, scale,
  clip and opacity for `surface`, `split-start`, `split-center`, `split-end`,
  `glow` and `overlay`, plus a content clip. It reads no host and mutates
  nothing, so the live applet, the Studio preview and a future preset card
  produce identical motion.
- Track forms: `center-slide` (also extend-track, reversed),
  `thickness-reveal` (also vertical slide, reversed), `split-horizontal`,
  `split-vertical`, `shutter-horizontal`, `lid` (front plate), `identity`, and
  `radial-interface`. A form and its reverse are one track, not two.
- (Superseded by ADFIX-TASK-001: true 3D now draws `collapse-radial` as an
  iris; see `docs/shared-renderer.md`, Opening mechanisms.)
- Radial/iris/fan is **declared, not implemented in 2D**. It reports
  `requiresRendererTier: "baked25d"` and
  `fallbackReason: "mechanism-requires-baked25d"`, then falls back to a centred
  clip and a fade. Nothing scales or rotates in a way that could be mistaken
  for a real iris. AD-0014 owns the renderer that can perform it.
- A collapsed shell always keeps a hoverable handle: the theme's declared end
  caps, or a bounded minimum when none are declared. A panel that collapsed to
  nothing could never be reopened.
- Both production host profiles now declare the mechanisms their surface can
  actually run. The free desktop host declares all six; the native edge host
  declares all but `collapse-radial`, because a Plasma edge panel is a
  rectangle. Theme declaration still gates every mechanism on top of that: the
  seven packaged themes declare `open`, `collapse-horizontal` and `split`, and
  the five procedural themes declare none.
- `open` is explicitly **not** a declarable capability. It is what a panel does
  when it is not collapsed, so it resolves available regardless of host and
  theme declarations. Treating it as declarable made every panel with a
  procedural theme resolve unavailable and refused every settings transaction
  on it; that is now pinned by a regression test.
- `PanelSkin2D` applies the tracks to the theme's own `split-*` parts, and to
  the parts it synthesises from the slice when a theme declares none. Clip
  tracks narrow the drawn window without moving artwork. `PanelProcedural2D`
  honours a mechanism the only way a drawn shape truthfully can: clip and fade.
- Input remains bound to the package alpha mask. Raising the glow changes what
  is drawn and never what the panel accepts a click on; this is asserted
  directly rather than inferred.
- `LivePanelPreview` no longer fades and shrinks its own card. It reads the
  scene's track, so a preview cannot advertise a collapse the desktop would not
  perform.
- `presentationMode`, `presentationTrigger`, `collapseMechanism`,
  `collapseAxis` and `revealHandle` are editor fields in the `panels-behavior`
  section, gated by the `presentation-mechanism` capability. The mechanism
  choice list is narrowed to what the resolver allowed for that panel, and the
  whole group is withheld when the panel has no way to collapse at all.

### Phase D — host visibility integrated with surface presentation

- The applet publishes `presentationState`, `transitionState`,
  `presentationProgress` and `hostPhase` into `sceneRuntimeState`. This is the
  wiring whose absence made the whole Phase A/B state machine invisible.
- Host concealment is reported *to* the controller through
  `applyHostVisibility()` rather than used directly, and the scene's
  `sceneConcealed` is driven by the controller's `hostVisible`. There is one
  authority for concealment, and a conceal is never reported to a renderer as
  a collapse.
- Hover, click and edge triggers request open and collapse. The reveal zone is
  the strip the panel keeps when collapsed, deliberately inside the applet's
  own bounds: an edge-approach detector outside the widget is not something a
  Plasma desktop applet can honestly provide.
- Native panel geometry is unchanged by a collapse. The scene's size comes from
  the panel's entries and its theme slice, never from the presentation track,
  so a hover does not renegotiate Plasma panel geometry. Animating a real
  panel's length remains a later capability-gated change.
- Free panels size to the theme's declared effect margins, so an open-state
  overhang or glow is not clipped by the applet drawing it.
- The applet reports its interaction guards to the backend through
  `reportPanelInteractionGuards`. `decidePanelVisibility` already refused to
  conceal a locked panel, but nothing had ever populated those locks, so the
  decision always ran with every guard false and a native auto-hide panel could
  conceal under an open context menu.
- `panelRendererConfiguration` publishes a `presentationProfile` record with a
  derived stable id, the resolved values and the available mechanisms, so a
  later Panel Preset can capture and restore presentation without knowing which
  individual settings keys composed it.

### Verification boundary

Baseline before any edit: fresh configure, build and 55/55 CTest. Phase C gate:
build green and 56/56. Consolidated: 57/57, including
`panel-motion-tracks-test` (16 cases), `panel-surface-integration-test` (14
cases) and the 66.69 s `rendering-import-smoke` in a disposable private D-Bus,
virtual KWin Wayland and private PlasmaShell session.

The isolated native/free Plasma lifecycle matrix
(`tests/run-plasma-lifecycle.sh`, which is **not** registered in CTest and is
run out of band) was executed four times against the task build directory. It
succeeded three times and failed once, on the first run of the session, at
`setNativePanelType <panel> hybrid` returning `(false,)`. That failure did not
reproduce in three subsequent runs, and the same harness succeeded against a
clean `026b8b3` worktree. The failing run also logged unrelated session noise
(`org.kde.KSplash exited with status 1`, a broken X display pipe, and a missing
`libcec.so.7` for `plasma-bigscreen-inputhandler`). The failure is therefore
recorded as observed and unexplained rather than attributed to either the
change or the environment. The exact command is:

```bash
ARCHDOCK_BUILD_DIR="$PWD/build-codex-task-0032cd" bash tests/run-plasma-lifecycle.sh
```

One focused correction was made during implementation, after its cause was
proved by a temporary diagnostic that was then removed: giving
`collapseMechanism` a real default routed `open` through the capability gate,
where no theme profile declares it, so every panel with a procedural theme
resolved unavailable and refused every settings transaction. The rule that
`open` is the baseline rather than a declarable mechanism is now a test.

Two behaviours are proved by automated tests rather than by observation on a
live desktop: that a collapsed chassis panel reads as a closed shell, and that
the reveal handle is large enough to hover comfortably. The isolated session
renders both but asserts neither, and observing them on the personal desktop
would require `plasmashell --replace` against the live session, which the
contract forbids without explicit authorisation.

### Known limitations of this work

- Radial, iris and fan mechanisms are interfaces only. They report the renderer
  tier they need and fall back to clip-and-fade in 2D. (Superseded by
  ADFIX-TASK-001: true 3D draws `collapse-radial` as an iris.)
- The vertical mechanism family is implemented but no shipped theme declares
  `collapse-vertical`, so it is reachable only through a user-supplied Theme
  Package v2 manifest.
- The chassis and energy families remain horizontal only; a vertical panel
  still falls back to procedural 2D.
- The reveal zone is inside the applet's own bounds. Compositor-level edge
  approach outside the widget is not claimed.
- `windowPreviewOpen` still has no producer. It remains a first-class guard fed
  a truthful `false` until TASK-0037 delivers grouped window previews.

## TASK-0032 corrective closure

An independent audit of `f61c9ab` against the consolidated task pack found
that TASK-0032 could not be proved closed. The gaps and their corrections:

- **Click trigger never closed.** `main.qml` requested a collapse on pointer
  leave only for the hover and edge triggers, so a click-opened panel stayed
  open forever. Every non-manual trigger now requests a collapse when the
  pointer leaves; the controller still holds it until the guards clear.
- **The `manual` trigger had no producer.** A panel resting collapsed with
  that trigger could never open. `PanelWindow` gained two additive D-Bus
  channels: `requestPanelPresentation(panelId, open|collapse)` queues one
  request per panel and publishes `presentationRequestRevision`, and the
  applet takes it exactly once through `takePanelPresentationRequest`.
  Explicit requests are honoured for every trigger; `manual` is driven by
  nothing else.
- **The live applet's presentation state was unobservable.** The applet now
  reports `surfaceState`, `transitionState` and `hostPhase` through
  `reportPanelPresentationState`, readable as `panelPresentationState`.
  Progress is deliberately excluded. This is runtime state only.
- **No live integration scenario existed.** `run-rendering-import-smoke.sh`
  now runs `tst_PanelSurfaceIntegration.qml` under the private Wayland
  scenegraph and then, on both the private native and free hosts, collapses
  the panel into a manual shell through the settings transaction, asserts the
  applet's own report reaches `collapsed`, opens and collapses it again
  through explicit requests, and restores an open hover panel.
- **`git diff --check` had failed since `c9fbbbf`** on a blank line at the
  end of `AnimationProfileRuntime.js`. Removed.
- **Free desktop hosts were reported concealed forever.** The new live check
  exposed it: Plasma instantiates the dock representation twice for a desktop
  applet, and the hidden compact instance wrote `hostConcealed = true` last.
  Every free panel therefore ran with its host phase `concealed`, which also
  withdrew icon motion on free hosts. Only the `fullRepresentationItem` may
  report host facts now, and a free host publishes its guards and presentation
  state as soon as bootstrap assigns its panel id. There is no offscreen
  reproduction because the double instantiation is Plasma's; the smoke's
  free-host assertion is the regression test.
- **The smoke's applet-error check had been vacuous.** Qt routes messages to
  journald when stderr is not a console, so `plasmashell.log` was always
  empty. The applet host now runs with `QT_FORCE_STDERR_LOGGING=1` and the
  check also matches `error when loading applet`. The service keeps default
  routing: Panel Studio has a pre-existing binding loop on `StudioForm.rows`
  at `SettingsPopup.qml:1047`, a genuine QML diagnostic owned by the TASK-0044
  cleanup and outside this closure.
- **Test hygiene.** `ValidateThemeV2Fixtures.cmake` is registered as
  `theme-v2-fixture-scaffold-test`; `PanelRegistryTest::cleanup()` refuses to
  delete application data unless CTest redirected `XDG_DATA_HOME`;
  `THEME_PACKAGE_V2.md` states how adapted v1 packages are actually resolved.

Evidence: fresh configure and build in `build-codex-task-0033`, 57 of 57
CTests excluding the smoke, then `rendering-import-smoke` passed in 68.8 s
against its 120 s budget with the new scenario, `git diff --check` clean. The
closure was reached with two proved corrections after the live check failed,
each recorded above; the temporary diagnostics used to prove the free-host
cause were removed.

## TASK-0033 — free and multi-shape content, geometry, transformed input, rotation, and lifecycle verification

AD-0013 is closed under the evidence boundary below. Its four phases follow the
TASK-0032 corrective closure in this working tree.

### Phase A — free content semantics and panel-specific order

- A free panel's content type is its record's `type`. The applet's
  `panelType=empty` is an ownership marker and is no longer consulted for what
  a free host shows. `empty` shows nothing, `launcher` the panel's own ordered
  entries, `tasks` the running applications, and `hybrid` the panel's entries
  followed by running-only applications; a running instance of a pinned
  desktop entry is merged into that entry, which keeps its identity and gains
  the running state plus `runningAppId` for window actions.
- `PanelContent.entryOrder` (`contentOrder`) is one canonical ordered list of
  `free-url:` ids across the panel's entries, derived deterministically for
  legacy records and repaired when a stored order names unknown or repeated
  ids. It is Internal: it changes only through the content operations.
- `PanelContentTransaction` (`src/panel/`) is the pure add/remove/move/set-order
  transaction; `PanelWindow` exposes `addPanelEntries`, `removePanelEntry`,
  `movePanelEntryBefore`, `setPanelEntryOrder` and `panelEntryOrder`, each
  committed as the next settings revision through the same registry path as
  every other change. Native panels refuse them; `moveDockEntryBefore` refuses
  free ids; `pinPanelUrls` and `removePanelContent` are compatibility
  wrappers. An application id is pinned to a free panel as its desktop file,
  so the free panel never references the global pin list.
- The applet routes free reorder, unpin, running-only pin and drops to the
  panel operations by `panelEntryId`, targets window actions at
  `runningAppId`, and follows task-model churn only for `tasks` and `hybrid`
  free panels (`FreeEntryPolicy.followsTaskModel`).

### Phase B — geometry hardening

- `LayoutEngine` coerces every numeric input through one finite guard, shares
  one sweep table between open-path entries and their surface, keeps the fan's
  historical angle frame, sizes the `diagonal` layout so its last entry stays
  inside the panel, and makes `upright` mean upright for the canonical and live
  profiles while the frozen `runtime` profile keeps its legacy tilt.
- The geometry test now runs a property matrix of every layout at counts 0, 1,
  7 and 24 against five hostile input sets, and asserts deterministic order and
  direction, upright versus tangent orientation, and entry-to-surface
  agreement; the visual harness checks sparse and dense radial layouts stay
  inside their bounds.

### Phase C — transformed input and whole-scene rotation

- `panelRotationMode`, `panelRotationSpeed` and `panelRotationTrigger` are
  editor fields in `panels-layout`, gated by `whole-panel-rotation` (never
  available on a native host) and offered only for radial layouts. The
  configured values persist; the running angle (`sceneRotation`) is transient.
  The trigger vocabulary is `idle` and `hover`; a `manual` rotation trigger was
  not added because nothing would produce it.
- `SceneRotationController` yields one angle offset and pauses for drag, Edit
  Mode, configuration, concealment and reduced motion; `PanelScene` adds the
  offset to the configured layout angle and feeds the sum to every geometry
  call, keeps a square envelope while rotation is enabled, and centres open
  paths on it so an arc pivots on its own circle centre.
- `GeometryHitRegion` is the scene's `containmentMask` for free, non-skinned
  radial scenes: input is accepted on the drawn band and on the entries at the
  effective angle; the empty interior and corners pass through. This narrows
  Qt Quick item hit testing only, and `nonrectangular-input` remains
  unclaimed because a Plasma desktop applet is still a rectangle to the
  compositor.
- Previews report rotation but never animate it.

### Phase D — lifecycle and integration verification

- The isolated lifecycle matrix (`tests/run-plasma-lifecycle.sh`, out of band)
  exercises free content semantics, panel-specific reorder, native and shared
  refusals, the ring-plus-rotation configuration, and persistence of order and
  rotation through the real PlasmaShell restart and a service restart, on the
  template free host, before the existing verified removal.
- The staged smoke configures a rotating procedural ring on the private free
  host through the ordinary transaction, requires the backend to publish the
  rotation with the capability available and the applet host to stay alive
  without QML errors, then restores the cyan energy panel.

### Verification boundary

- Fresh configure and build in `build-codex-task-0033` on `f61c9ab` plus this
  working tree. Phase gates: Phase A 58 of 58 CTests excluding the smoke and
  `rendering-import-smoke` in 67.7 s; Phase B 58 of 58 and 68.4 s; Phase C 60
  of 60 and 68.3 s; Phase D smoke with the rotation scenario in 69.0 s, all
  against the 120 s budget. Final gate on the finished tree: the incremental
  build rebuilt nothing, and 61 of 61 CTests passed including the smoke in
  68.0 s. `git diff --check` is clean.
- The isolated lifecycle matrix, run out of band as
  `ARCHDOCK_BUILD_DIR="$PWD/build-codex-task-0033" bash tests/run-plasma-lifecycle.sh`,
  completed all 35 phases on its fifth attempt in 3 min 57 s, including the
  free content, order and rotation phase and both persistence phases after
  the real PlasmaShell restart and the service restart. The first attempt died
  silently on a `grep` exit status under `set -e` in the new phase, which was
  corrected. Attempts two and three passed the new phase and failed later in
  pre-existing native placement phases (`setNativePanelType hybrid` returned
  false; `readback-mismatch` on `fixedLength`), and attempt four failed at
  native panel creation with `readback-mismatch` (observed 88, requested 720).
  The same creation failure with the same values reproduced on an untouched
  export of `f61c9ab` built and run from a scratch directory, so the
  intermittent read-back failures are environmental and predate this task.
  They are not corrected here.
- Whole-scene turning is proved offscreen by `scene-rotation-test`; the live
  smoke proves the rotating configuration reaches the applet and the applet
  keeps running. No desktop observation was made and the live desktop was
  not touched.
- `build-codex-task-0014` remains tracked from an earlier session; removing
  it needs `git rm -r build-codex-task-0014`, a Git-state change reserved to
  the owner.

### Known limitations of this work

- The input region is item-level. Outside the applet's rectangle nothing is
  claimed; inside it, the compositor still delivers events to the applet
  window and Qt Quick declines them off the band.
- The live smoke proves the rotating configuration reaches the applet and that
  the applet keeps running; that the scene visibly turns is proved by the
  offscreen scene test, not by observation on a desktop.
- Free entries are local URLs. A running application pinned to a free panel
  becomes its desktop file; an application the model cannot locate a desktop
  file for cannot be pinned to a free panel.

## TASK-0034 — baked 2.5D ring, octagonal, and arc themes

AD-0014 is closed under the evidence boundary below. Its three phases follow
TASK-0033.

### Phase A — the baked 2.5D renderer contract

- Theme Package v2 gains `tracks`: anchor paths in the artwork's own
  coordinate space that say where real icons stand and how far they shrink.
  A track declares a shape (`ellipse`, `polygon`, `arc`), a centre, two radii,
  a start and sweep, optional polygon sides, a required `depth` block
  (`farScale`, `nearScale`, `occlusionDepth`) and an optional bounded `tilt`.
  Layer roles gain `rear` and `foreground`. A package that declares
  `baked2.5d` without a track is rejected as `renderer-asset-mismatch`:
  artwork alone is a flat picture, not a depth renderer.
- `LayoutEngine` gains `trackMetrics()`, `trackEntryGeometry()`,
  `trackPoint()`, `trackTiltFactor()` and `trackSupportsRotation()`. The
  configured layout radius drives one uniform artwork scale; the declared tilt
  is applied to the track and the drawn platform together so icons keep
  sitting on the artwork; the scene box is the union of the platform and every
  scaled icon, measured around the whole closed path while the scene rotates
  so a turning ring never asks its host to resize.
- Depth is normalized: 0 at the far edge, 1 at the near edge. An entry's `z`
  is its depth and the theme's `occlusionDepth` becomes the `z` of the
  foreground layers `PanelScene` instantiates among the entries. That ordering
  is the only reason a real icon can pass behind a platform rim. Logical entry
  order is untouched; only paint order changes.
- `PanelBaked25D.qml` draws the platform: rear, shadow, reflection, glow and
  overlay behind the entries, and a `foregroundComponent` the scene
  interleaves with them. It requires no Qt Quick 3D module and declares no
  mesh; `ValidateRenderingModule.cmake` now fails the build if any shared
  rendering source imports one.
- `ThemeStateSelection.js` is the single Theme v2 state and layer selection
  contract. `PanelSkin2D` was refactored onto it rather than the baked
  renderer growing a second copy, so a state, a hover and an opening crossfade
  cannot mean two different things depending on which renderer drew the panel.
- The baked renderer is installed and enabled for the **free desktop host
  only**; a native edge panel resolves `renderer-host-unsupported` and falls
  back. The radial mechanism's required-tier string was canonicalised from
  `baked25d` to `baked2.5d` so it can be compared against the real tier.
- Input is the package alpha mask positioned at the platform rectangle,
  combined with the entry rectangles; `activeInputRegionKind` reports
  `platform-mask`.

### Phase B — ring, octagonal, and arc families

- Three original clean-room packages ship: `ring-platform-blue`,
  `octagon-platform-steel` and `arc-platform-orange`. Each is a perspective
  annulus with separable rear, foreground rim, shadow, reflection and neutral
  glow layers, a transparent centre on the closed families, and an input mask
  that matches the stroked drawn silhouette. Each declares one track, four
  states, four input masks, and `procedural2d` as its only fallback.
- The 42 class-F and class-G screenshots remain opaque, redistribution
  unknown and non-installable. Three were consulted as broad-concept
  references only; the rights boundary and the deterministic recipe are in
  `assets/source-samples/perspective/`. No screenshot pixel entered a package,
  and no package draws a placeholder icon, because real icons are placed by
  the track instead.
- The built-in catalog grows from 12 to 15 themes. Each perspective entry
  carries `presetIntent` naming its future Panel Preset id
  (`circular-blue-ring`, `octagonal-platform`, `orange-arc-dock`) and a safe
  fallback theme and tier, which is the lineage TASK-0040 consumes.

### Phase C — depth, fallback, resources and documentation

- `PanelSkinLayer2D` gains a raster budget. Skins keep Qt's behaviour;
  baked layers rasterise at the drawn size capped at 2048 pixels per axis and
  are not kept in the shared pixmap cache, so switching families releases the
  previous platform's textures.
- Fallback is truthful for every failure mode: a missing platform or mask
  falls back to procedural 2D with a reason while keeping every entry, and a
  missing decorative layer is skipped and counted without costing the tier.

### Verification boundary

- Fresh configure and build in `build-codex-task-0034`. Phase gates: Phase A
  62 of 62 CTests including the private-session smoke; Phase B 63 of 63;
  Phase C 63 of 63. `git diff --check` is clean.
- `panel-baked-25d-test` (44 cases) proves the renderer contract offscreen,
  including an A/B pixel proof that the same icon at the same place is hidden
  by the foreground band at `occlusionDepth` 0.5 and drawn over it at 0, that
  depth order is stable across repeated reads, that logical order survives
  depth sorting, and that the glow pulse stops when concealed and under
  reduced motion.
- `baked-25d-asset-test` (9 cases) proves provenance and asset hygiene,
  including a pixel comparison that every pixel the package paints lies inside
  its input mask.
- The staged smoke selects all three families on the private free host through
  the ordinary settings transaction; each resolves `baked2.5d` with its own
  staged manifest. It then performs 16 theme changes across the three families
  and the cyan energy skin: the private PlasmaShell's resident memory went
  from 653580 kB to 649684 kB, a decrease of 3896 kB, so repeated theme
  changes release their textures rather than accumulating them.
- One intermittent failure was observed: on the first run of the extended
  smoke, the pre-existing in-session
  `PanelWindowCapabilityTest::iconPropertiesPublicInteractionIsTransactional`
  failed amid xdg-desktop-portal and PipeWire registration warnings. Four
  consecutive re-runs passed. The failure is in an interaction test this task
  did not touch and is treated as environmental.
- The isolated Plasma lifecycle matrix (`tests/run-plasma-lifecycle.sh`) was
  not run: no TASK-0034 phase requires it, and this task changes no panel
  lifecycle behaviour.

### Defects found and corrected in adjacent code

- `GeometryHitRegion.contains()` was declared without type annotations, so Qt
  never accepted it as a `containmentMask` and logged "Object set as mask does
  not have an invokable contains method" while silently falling back to the
  plain rectangle. TASK-0033's `geometry-band` input narrowing was therefore
  reported but not applied. The signature is now `contains(point): bool` and
  the warning is gone from the rotation suite as well.
- `layoutRadius` was offered for every radial layout except the polygon
  family, although `LayoutEngine` sizes those layouts from that radius. An
  octagonal panel could not be sized at all and the settings transaction
  refused the field. The polygon layouts were added to its editor gate.

### Known limitations of this work

- `collapse-radial` is still an interface only. It reports the tier it needs
  and falls back to a centred clip and fade; no shipped perspective package
  declares it. (Stale since the Cyan and Orange 3D packages declared it with
  mesh parts; ADFIX-TASK-001 draws it as an iris in true 3D.) A real iris mechanism is not part of TASK-0034 and remains
  open for the owner to schedule.
- Tilt is read from the internal `surface.parameters2_5D` map and clamped to
  the theme's declared range. No editor exposes it, so no visible control
  claims it.
- Input narrowing remains item-level. Outside the applet's rectangle nothing
  is claimed, and `nonrectangular-input` stays unclaimed.
- The perspective families are free-desktop only. A native edge panel cannot
  present them and falls back to procedural 2D.
- Frame timing was not measured. The performance evidence is bounded resident
  memory across repeated theme changes plus a surviving, error-free applet.

## TASK-0035 — optional true-3D capability and base scene renderer

### Phase A and implementation boundary

Phase A completed before Phase B began. `ARCHDOCK_ENABLE_QUICK3D` supports
OFF/AUTO/ON, with generated C++ and QML facts, optional resource packaging,
consumer import/backend probing, precise diagnostics and safe fallback.
During Phase A the scene-built fact was false. Phase B now supplies the real
scene, so ON and discovered AUTO report both build facts true; OFF reports
both false. Saved settings cannot override them. The final combined gates
below reverify every Phase A acceptance criterion against the completed code.

### Phase B implementation

- `PanelScene3D.qml` and `IconStyle3D.qml` use native Qt Quick 3D meshes,
  perspective camera, two lights, materials, emission and a validated texture.
  The original cyan platform has bevels, side walls and an underside, with
  192 vertices and 96 triangles. Each icon base uses actual mesh geometry.
- Theme v2 now has an optional strict `scene3D` contract. Mesh/material JSON
  is bounded and validated; executable package shaders, scripts and QML are
  not accepted. Missing resources fail safely. The core remains independent
  of the optional native module.
- The loader follows the declared fallback order. Baked fallback uses its
  anchor track, and skinned fallback uses its content bounds. Real entries,
  logical hit targets and accessibility remain in the shared 2D delegates.
- Low/medium/high quality uses render scales 0.5/0.75/1, axis caps
  1024/1536/2048, and off/2/4-sample antialiasing. Persisted quality is bounded
  and reversible. Studio hides detail controls when unavailable or off.
- Whole-panel rotation is a separate renderer capability. The base scene
  reports `renderer-rotation-unavailable`; supported 2D fallback keeps its
  rotation behavior. No TASK-0036 motion implementation is claimed.

See [shared-renderer.md](shared-renderer.md#base-true-3d-renderer) and
[THEME_PACKAGE_V2.md](THEME_PACKAGE_V2.md#5a-optional-3d-scene) for contracts.

### Blockers diagnosed and repaired

| Boundary | Proved cause and bounded repair | Verification |
| --- | --- | --- |
| Private service ownership | KWin requests Arch Dock during the Icon Properties startup check, activating the staged service before the explicit launcher. The launcher forwards settings and exits. The smoke now tracks the actual unique D-Bus owner/PID, validates staged executable and private XDG/bus identity, detects replacement, and uses pidfd-safe cleanup. | Narrow ownership pass; every final private smoke pass. AUTO owner 214193, forwarder 214747 exited successfully. |
| Private desktop creation | Calling `desktopForScreen(0)` during startup could create a metadata-free desktop before the activity/containment existed, yielding AppletError. The smoke waits for and selects the initialized desktop, with guarded failed-creation cleanup. | Real native and free applets created in every final mode. |
| Private native startup | Manual fixture creation raced normal service recovery and its revision change. The harness waits for the actual recovered, ownership-verified native panel instead. A cleanup exit race is accepted only when the pidfd confirms exit. | Full private startup, revision and owner checks pass. |
| Qt sequence projections | Qt sequential containers are array-like but `Array.isArray()` returns false. Snapshot copying discarded their shape. SettingsEditorModel and CapabilityModel now preserve nested sequences; loader tier lists are copied explicitly. | Native sequence regressions and private Studio pass. |
| Mesh icon alignment | Construction-time projection read uninitialized camera matrices. Camera-local mesh positions now map to logical pixel centers. | Two projected icon centers verified within one pixel; camera and quality checks pass. |
| Studio renderer switch | Full snapshots carry unchanged formerly available fields, which candidate validation rejected. Only unchanged previously available values may now survive a capability switch; unavailable changes and protected state still fail. | Public transaction regression; private 3D/2D/3D Apply and Cancel/reopen pass. |
| Cancel test | The new assertion inspected a closed preview instead of reopening saved state. The test now checks closure, saved 2D state and the reopened UI. | Private interaction pass. |
| Runtime fallback geometry/order | Baked and skin fallback selected artwork while retaining procedural geometry; the loader also ignored procedural-first ordering. Geometry now follows the selected fallback, in declared order. | 45/45 baked QML checks and 14/14 shared scene checks. |
| Catalog fixtures | Old assertions expected 15 themes, ten packages and no 3D profile. They now assert 16 themes, eleven packages and exactly one mesh profile, retaining prior family checks. | All four affected targets pass; full suites pass. |
| Renderer rotation gate | Host/theme rotation was exposed without renderer support. An explicit renderer fact now gates the resolution, runtime and editor. | Public API regression and final ON/OFF/AUTO suites pass. |

### Final verification — 2026-09-09

All modes used separate fresh out-of-tree Debug configurations under
`/tmp/archdock-task0035-resume.JvaKHyp4/`. ON and OFF were rebuilt after the
focused corrections. AUTO was configured and built from scratch after the
last code change. All builds and CTests ran serially.

| Mode | Configure | Build facts: module / scene | Full CTest |
| --- | --- | --- | --- |
| OFF | PASS; `-DARCHDOCK_ENABLE_QUICK3D=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Quick3D=TRUE` | 0 / 0 | 65/65, 126.74 s |
| AUTO | PASS; discovered `/usr/lib/cmake/Qt6Quick3D` | 1 / 1 | 65/65, 132.36 s |
| ON | PASS; required installed Quick3D | 1 / 1 | 65/65, 132.33 s |

The commands were `cmake -S . -B <fresh-directory>` with each mode above and
`-DCMAKE_BUILD_TYPE=Debug`, `cmake --build <directory> --parallel 1`, then
`ctest --test-dir <directory> --output-on-failure --parallel 1`. The parent
test environment used an intentionally nonexistent session-bus address; the
smoke creates its own private bus. No mode links the core executable to
Quick3D. Staged install and optional-resource presence/absence passed.

Enabled scene tests observed 33,674 nontransparent mesh pixels, verified two
mesh icon centers, camera changes, quality reversal and missing mesh/texture
fallback. ON and AUTO captures were byte-identical and the ON capture was
visually inspected. The real staged free applet reported rendered frames,
96 platform triangles, and quality targets 344×344 → 688×688 → 344×344 for
low/high/low. OFF reported requested `true3d`, effective `procedural2d`, reason
`renderer-not-installed`, no 3D frame and no 3D quality control.

The first broad ON gate exposed four catalog targets with stale expectations;
those failures were diagnosed before correction. They are not counted as a
pass. The final totals above are complete reruns after corrections. Required
module, fallback, editor, package, geometry, staged applet, ownership and
resource-cycle checks all passed. Personal desktop, physical GPU/monitor and
hardware hotplug acceptance were not performed or claimed.

### Acceptance criteria

| Phase | Criterion | Result |
| --- | --- | --- |
| A | Core configures/builds without 3D | PASS — OFF clean configure/build and full suite |
| A | Availability is independent of saved settings | PASS — generated facts and consumer probe tests |
| A | 3D controls absent when unavailable | PASS — OFF private Studio and missing-module/software tests |
| A | Deterministic fallback | PASS — declared-order, baked/skin geometry and real OFF applet checks |
| B | Actual mesh scene renders with capability | PASS — pixel checks and real staged applet frames |
| B | Missing mesh/texture falls back without crashing | PASS — package and private scene tests |
| B | Quality bounded and reversible | PASS — schema persistence, pixels and live low/high/low targets |
| B | 2D build independent | PASS — OFF build, omitted optional files, core dependency check and 65/65 CTest |

## TASK-0036 — resumed Phase A investigation, 2026-09-19

Historical run: the glyph-motion failure below is superseded by the
TASK-0036 blocker-repair record later in this document.

**Status: BLOCKED in Phase A. Phase B has not started.** The earlier approved
plan and implementation were reused after the owner explicitly requested
completion of TASK-0036. No renderer workaround was applied without proof.

The inspected platform was Arch Linux, Plasma/KWin 6.7.5-1, Qt base 6.11.2-3,
Qt Declarative 6.11.2-2, Qt Quick 3D 6.11.2-1, and KCoreAddons 6.30.0-1.
The complete fresh ON Debug build passed using the installed Make generator.
An initial Ninja configuration invocation failed because Ninja is absent;
the approved default-generator command succeeded in a separate fresh directory.

The first private Wayland reproduction measured a mesh glyph Y angle change
from 11.6471 to 46.9412 degrees, a valid `application-x-executable` icon, and
28.8 by 28.8 source dimensions, but identical before/after viewport pixels.
The captures showed the platform without the glyph. The focused diagnostic
pass additionally checked provider sizing, source/texture captures, material
sampling, and full-window capture. Full-window captures were also unchanged.
These observations do not establish a production correction or blame KDE/Qt.
Temporary state-changing diagnostic code was removed; the retained test adds
non-null image checks, optional viewport/full-window evidence captures and an
informative failure message, preserving the visible-motion requirement.

| Fresh verification | Result |
| --- | --- |
| Pack SHA-256 integrity | PASS, 174/174 manifest entries |
| ON configure and complete serial build | PASS |
| Targeted renderer rebuilds after diagnostic changes | PASS |
| Private rendering smoke | FAIL in each of four diagnostic invocations at the unchanged-pixels assertion; private renderer QtTest reports 5 passed, 1 failed per invocation |
| Full CTest | NOT RUN in this resumed session; the unresolved runtime gate stopped later verification |
| Phase B OFF/AUTO/ON and service-restart matrix | NOT EXECUTED |
| Final diff and cleanup | PASS — `git diff --check`; four private session roots absent, no process retaining their private environment, task-owned temporary build/logs/captures removed |

Exact build and initial reproduction commands (the temporary root was task-owned):

```bash
cmake -S '/mnt/F/Arch Dock' -B /tmp/archdock-task0036-resume.yEoOWWXB/phase-a-on \
  -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=ON
cmake --build /tmp/archdock-task0036-resume.yEoOWWXB/phase-a-on --parallel 1
env DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0036-resume.yEoOWWXB/no-parent-bus \
  ARCHDOCK_SCENE_EVIDENCE_DIR=/tmp/archdock-task0036-resume.yEoOWWXB/evidence \
  ctest --test-dir /tmp/archdock-task0036-resume.yEoOWWXB/phase-a-on \
  -R '^rendering-import-smoke$' --output-on-failure --parallel 1
```

The diagnostic directory must exist when supplying `ARCHDOCK_SCENE_EVIDENCE_DIR`.
The smoke staged the build and used a disposable private D-Bus and virtual
KWin Wayland session. It failed before the subsequent Studio, Plasma applet,
live fallback and recovery stages. No personal-desktop acceptance is claimed.

| Acceptance criterion | Current result |
| --- | --- |
| A: 3D motion corresponds to logical profiles | FAIL at visual verification; numeric Y mapping passes |
| A: No conflict/orphan after theme change | NOT EXECUTED to completion |
| A: Reduced-motion static/low-motion states | NOT EXECUTED in the new 3D runtime path |
| A: Unsupported themes hide part controls | Prior backend tests passed; private UI acceptance NOT EXECUTED |
| B: Failure cannot crash or remove the panel | NOT EXECUTED |
| B: Saved intent remains truthful when unavailable | NOT EXECUTED to Phase B acceptance |
| B: Detailed controls disappear when off/unsupported | NOT EXECUTED to Phase B acceptance |
| B: 2D/2.5D remain usable | Phase B matrix NOT EXECUTED |

## TASK-0037 — partial preview implementation, 2026-09-19

Historical implementation run: its changes are now committed as `4cdb234`.
The current full-suite stopping point is recorded in the blocker repair below.

**Status: BLOCKED in Phase A. Phases B and C have not started.** The owner
explicitly requested continuation from the existing TASK-0037 plan after the
TASK-0036 investigation, then supplied
`APPROVED: IMPLEMENT TASK-0037 EXACTLY AS PLANNED.` A subsequent pause was
lifted before implementation resumed. This sequencing exception does not
close TASK-0036 or waive a phase gate. The existing audit, plan and verified
pack inventory were reused; no replacement plan or delegated agent was used.

The platform for this work was Arch Linux, Plasma/KWin 6.7.5-1, Qt base
6.11.2-3, Qt Declarative 6.11.2-2, KCoreAddons/KService/KIO 6.30.0-1 and
KPipeWire 6.7.5-1. No dependency was installed. KService/KIO launch and desktop
actions remain planned Phase B work, with no CMake dependency added yet.

### Implemented Phase A behavior and reuse

Window capability flags and caption changes flow through the existing KWin
watcher and WindowModel. A small `WindowPreviewModel` projection derives
per-window IDs, titles, active/minimized states and action availability from
that authoritative model. DockModel exposes those rows through its existing
model roles and snapshots; free-panel pinned entries reuse the same snapshot
merge. No second window tracker or new PanelWindow D-Bus method was added.

The shared `WindowPreviewPopup` provides title/state rows, keyboard navigation,
specific-ID selection, live row updates and safe empty-list dismissal. The
Plasma host uses `PlasmaCore.Dialog`, existing PanelScene anchors and the
presentation controller's preview guard. Hover or the new Windows menu item
opens it; menu handoff keeps the guard held. Edit/drag/input restrictions and
backend disappearance close or prevent the preview. Phase A adds selection
only; minimize, restore, close, New Instance and desktop actions remain Phase B.

The optional KDE adapter uses `TaskManager.ScreencastingRequest` and
`PipeWireSourceItem`. Missing imports or unavailable frames leave the title
list usable. Installed QML types and upstream KPipeWire source were inspected:
the stream must be visible to obtain its first frame, so it is not hidden
behind its own readiness property. A focused QML fixture checks that lifecycle;
it does not prove a live thumbnail stream. Actual thumbnail availability in
the target session remains unverified.

The private multi-window fixture reuses the existing C++ Qt/PanelWindow test
target because PySide6 is absent. It creates two real QQuickWindows in the
existing disposable KWin session and checks watcher updates and ID-based
selection. This changes fixture placement only. The fixture is integrated but
was not reached at the failed runtime gate; no live acceptance is claimed.

Files first changed in implementation order (subsequent focused corrections
remain within these files):

| Order | File(s) | Purpose |
| --- | --- | --- |
| 1 | `src/WindowItem.h` | Per-window capability facts |
| 2 | `src/WindowModel.h`, `src/WindowModel.cpp` | Append roles and update notifications |
| 3 | `src/WindowWatcher.cpp` | Parse capabilities through existing payloads |
| 4 | `kwin-script/contents/code/main.js` | Native capability facts and caption updates |
| 5 | `src/content/WindowPreviewModel.h`, `src/content/WindowPreviewModel.cpp` | Reusable projection with title fallback |
| 6 | `tests/WindowPreviewModelTest.cpp` | Projection, update and removal coverage |
| 7 | `CMakeLists.txt` | Source/test registration and shared-QML packaging |
| 8 | `src/DockModel.h`, `src/DockModel.cpp` | Preview roles and snapshots |
| 9 | `tests/DockModelTest.cpp` | Group updates and exact-ID selection |
| 10 | `src/panel/PanelWindow.cpp` | Free-panel snapshot merge |
| 11 | `tests/PanelWindowCapabilityTest.cpp` | Merge assertions and private KWin fixture |
| 12 | `qml/ArchDock/Rendering/previews/WindowPreviewPopup.qml` | Shared preview content |
| 13 | `qml/ArchDock/Rendering/qmldir` | Register shared component |
| 14 | `tests/tst_WindowPreviewPopup.qml` | Selection, updates, fallback, guards and anchor mapping |
| 15 | `plasma-dock-widget/contents/ui/WindowPreviewHost.qml` | Plasma popup lifecycle and anchoring |
| 16 | `plasma-dock-widget/contents/ui/KdeWindowThumbnail.qml` | Optional KDE/PipeWire adapter |
| 17 | `plasma-dock-widget/contents/ui/DockEntry.qml` | Hover/menu entry points and guard handoff |
| 18 | `plasma-dock-widget/contents/ui/main.qml` | Bind snapshots, anchors, guards and selection |
| 19 | `tests/tst_DockEntry.qml` | Menu routing, input restrictions and guard handoff |
| 20 | `tests/ValidateRenderingModule.cmake` | Include popup in host-neutral boundary check |
| 21 | `tests/run-rendering-import-smoke.sh` | Reuse private harness for new runtime fixtures |
| 22 | `docs/CURRENT_STATE.md`, `docs/RELEASE_CHECKLIST.md` | Record partial implementation and blocked acceptance |

### Fresh verification and exact stopping boundary

| Verification | Result |
| --- | --- |
| Consolidated pack integrity at planning | PASS, 174/174 manifest entries; reused during implementation |
| Fresh Debug AUTO configure, Unix Makefiles | PASS |
| Complete serial build and final refresh after corrections | PASS |
| Focused model/backend/QML/guard/host-neutral selection | PASS, 7/7 CTest entries |
| Final full available CTest invocation | FAIL, 54 passed, 1 failed, 12 not run; 67 registered, stopped at first failure |
| Window preview model unit cases | PASS, 4 passed, 0 failed |
| Dock model unit cases | PASS, 9 passed, 0 failed |
| PanelWindow headless cases | 23 passed, 0 failed, 1 skipped; private grouped-window case requires the disposable session |
| Window preview QML cases | PASS, 13 passed, 0 failed; offscreen evidence only |
| Guard interaction / DockEntry QML cases | PASS, 8 / 13 passed, 0 failed |
| C++/JavaScript/shell syntax and QML lint checks | PASS; lint warnings are not live acceptance |
| Private renderer capability cases | FAIL, 5 passed, 1 failed at unchanged mesh-glyph pixels |
| New private grouped-window and popup checks | NOT EXECUTED; earlier renderer assertion stopped the smoke |
| Full native-edge/free-layout action matrix | NOT EXECUTED; Phase C not started |

The exact fresh build and final phase gate were:

```bash
cmake -S '/mnt/F/Arch Dock' -B /tmp/archdock-task0037.obs1WOQt/build \
  -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=AUTO
cmake --build /tmp/archdock-task0037.obs1WOQt/build --parallel 1
env DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0037.obs1WOQt/no-parent-bus \
  ctest --test-dir /tmp/archdock-task0037.obs1WOQt/build \
  --output-on-failure --parallel 1 --stop-on-failure
```

CTest stopped at test 55, `rendering-import-smoke`, after 58.21 seconds total.
Staged module imports and the existing preview/parity checks passed. Inside
the private KWin session, `RendererCapabilityTest::realScenePixelsQualityAndFallback`
then failed at `tests/RendererCapabilityTest.cpp:259`:

```text
'turning != advanced' returned FALSE.
Mesh glyph angle 11.6471 -> 46.9412 produced unchanged pixels;
glyph source=application-x-executable valid=1 size=28.8x28.8
```

The focused diagnostic comparison confirmed both that test and
`qml/ArchDock/Rendering/optional3d/PanelScene3D.qml` are unchanged from `HEAD`.
This reproduces the documented TASK-0036 blocker. No speculative renderer
change, test bypass or repeated smoke was attempted. The contract requires
stopping here rather than continuing into TASK-0037 Phase B.

The pending live target remains the existing harness command below, after a
proved TASK-0036 correction and a fresh build. It was not invoked separately
after the full suite failed, and is not an environment waiver:

```bash
env DBUS_SESSION_BUS_ADDRESS="unix:path=$task37_root/no-parent-bus" \
  ctest --test-dir "$task37_build" \
  -R '^rendering-import-smoke$' --output-on-failure --parallel 1
```

### Inherited acceptance status

`NOT EXECUTED` below means the full acceptance check remains outstanding;
passing unit/offscreen subsets do not close required live checks.

| Phase / criterion | Result and evidence boundary |
| --- | --- |
| A: Multiple windows individually show correct titles/states | NOT EXECUTED to live acceptance; model/snapshot/QML subsets pass |
| A: No-thumbnail environments remain fully usable | NOT EXECUTED to live acceptance; offscreen title-list selection and fallback pass |
| A: Last-window removal safely dismisses/updates the popup | NOT EXECUTED to live acceptance; model removal and QML dismissal/guard release pass |
| A: Positioning works on all edges/free layouts | NOT EXECUTED to live acceptance; QML location mapping covers four edges and representative ring/arc directions |
| B: Every shown action has a backend path | NOT EXECUTED; Phase B not started |
| B: Specific actions target the selected ID | NOT EXECUTED for the full action set; existing selection path has passing unit coverage |
| B: Running-only/transient entries hide invalid actions | NOT EXECUTED; Phase B not started |
| B: Menu dismissal and guards remain correct | NOT EXECUTED for Phase B; Phase A guard/handoff subsets pass |
| C: Multiple windows individually seen and controlled | NOT EXECUTED; Phase C not started |
| C: No stale action remains after removal | NOT EXECUTED for the full action set; Phase A selection rejects removed IDs |
| C: Popup/menu clicks do not launch the underlying icon | NOT EXECUTED to live acceptance; Phase A menu routing subset passes |
| C: All targeted/full tests pass | FAIL; mandatory full suite stopped at the pre-existing renderer failure |

No release checkbox or task-completion gate is closed. Phase B individual
actions and Phase C full regression/runtime coverage remain owned by TASK-0037;
they have not been deferred to another task. TASK-0036 owns the renderer blocker.

Final cleanup and whitespace checks passed. The smoke's private root
`/tmp/archdock-rendering-import.KMSQ5o` is absent, no process retained its private
environment or the task build's executable/working directory, and the owned
`/tmp/archdock-task0037.obs1WOQt` build/log/probe root was removed after recording
the evidence above. The source changes remain unstaged for owner review.

## TASK-0036 — glyph-motion blocker repair, 2026-09-19

Historical repair, committed by the owner as `d7c6021`. Its grouped-window
arrival failure is superseded by the prerequisite investigation below.

**Original blocker: RESOLVED. Integrated gate: BLOCKED at TASK-0037.** The owner
requested resolving the predecessor blocker before TASK-0038 planning. The
existing TASK-0036 plan, implementation and earlier diagnostics were reused.
This repair does not close TASK-0036 Phase B or complete TASK-0037.

The inspected platform was Arch Linux, Plasma/KWin 6.7.5-1, Qt base 6.11.2-3,
Qt Declarative 6.11.2-2, Qt Quick 3D 6.11.2-1 and Kirigami 6.30.0-1. No new
dependency or KWin window rule was needed.

### Proven cause and retained changes

The unchanged renderer test reproduced the original failure in a fresh private
KWin Wayland session. The glyph's named icon, `application-x-executable`, had
Kirigami `status: Error` despite `valid: true`. Both its own image capture and
its layer capture were entirely transparent. A correctly typed diagnostic
assignment removing the texture made the cube visible. Readback showed that
the earlier `QObject *` null assignment had not actually cleared the typed
texture property; its result could not exclude the texture source.

KDE's [Icon implementation](https://github.com/KDE/kirigami/blob/v6.30.0/src/primitives/icon.cpp)
can retain a non-null transparent image after an unsuccessful icon lookup.
The retained test therefore reuses `tests/fixtures/icon-style-v1/assets/base.svg`
as a deterministic glyph source. It requires native `Ready` status and more
than 100 nontransparent source pixels before asserting that actual mesh
rotation changes viewport pixels. The existing motion/channel, input geometry,
concealment, reduced-motion and fallback assertions remain enforced.

Once motion passed, the same test reached two cleanup boundaries:

- `PanelSurfaceLoader.qml` dereferenced `scene3D.texture` while a theme was
  being removed. Its three scene-input bindings now supply empty/null values
  during teardown, avoiding the QML error while preserving fallback reasons.
- The resource-limit check inspected a removed Repeater3D delegate before
  deferred deletion. It now waits for actual absence. This follows Qt's
  [Repeater3D ownership and deletion contract](https://doc.qt.io/qt-6/qml-qtquick3d-repeater3d.html#objectRemoved-signal);
  it does not remove or destroy the delegate manually.

File change order: renderer test diagnostics and fixture/precondition repair;
surface-loader teardown guard; renderer test deletion wait; this evidence
document; release checklist. Temporary diagnostic mutations were removed.
The production mesh, texture-provider and motion implementation was preserved.

### Fresh evidence

| Check | Result |
| --- | --- |
| Active pack integrity | PASS, 174/174 manifest entries |
| Fresh ON Debug configure and complete serial build | PASS |
| Focused PanelScene CTest | PASS, 1/1 |
| Repaired motion/fallback case in private KWin Wayland | PASS, 3 QtTest cases including setup/cleanup |
| Staged private renderer suite inside `rendering-import-smoke` | PASS, 6/6; 34,878 visible mesh pixels; physical glyph motion, parts, concealment, reduced motion, quality, active fallback and bounded recovery |
| Full CTest, stop on first failure | FAIL, 54 passed, 1 failed, 12 not run of 67 registered; 76.47 seconds |
| First remaining failure | `PanelWindowCapabilityTest::groupedWindowsFollowLiveKWinUpdates`, `tests/PanelWindowCapabilityTest.cpp:157`; the first window title did not appear in WindowModel |
| Later private popup/editor/applet checks | NOT EXECUTED after the grouped-window failure |
| TASK-0036 Phase B OFF/AUTO/ON and service-restart completion matrix | NOT EXECUTED by this blocker repair |

The new failure is in the unchanged TASK-0037 fixture, after the renderer suite
has passed. No later phase, independent rerun of later smoke stages, or test
bypass was used. TASK-0036's supported-part UI acceptance and its complete
Phase A/full-suite gate remain unclosed; the targeted 3D motion, termination,
reduced-motion and resource-cleanup behavior above was directly observed.

The fresh build directory was
`/tmp/archdock-task0036-fix.3N1HXeqJ/on`. The integrated commands were:

```bash
cmake -S '/mnt/F/Arch Dock' -B "$task36_root/on" \
  -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=ON
cmake --build "$task36_root/on" --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$task36_root/no-parent-bus" \
  ctest --test-dir "$task36_root/on" \
  --output-on-failure --parallel 1 --stop-on-failure
```

The pending integrated runtime target, after resolving the grouped-window
failure and making a fresh build, remains:

```bash
env DBUS_SESSION_BUS_ADDRESS="unix:path=$task36_root/no-parent-bus" \
  ctest --test-dir "$task36_root/on" \
  -R '^rendering-import-smoke$' --output-on-failure --parallel 1
```

All rendering evidence used private D-Bus/XDG state and virtual KWin Wayland;
it does not establish personal-desktop, hardware or monitor acceptance. The
smoke cleaned `/tmp/archdock-rendering-import.yYQk54`; no process retained the
private diagnostic environments. The task-owned build/log/capture root was
removed after recording these results. The final diff passed whitespace
checks. Source and evidence changes remain unstaged for owner review.

## TASK-0038 — approval and prerequisite watcher repair, 2026-09-19

Historical repair, committed by the owner as `7c16581`. Its remaining restore
failure is superseded by the verified repair below.

**Status: BLOCKED before Phase A.** The owner supplied
`APPROVED: IMPLEMENT TASK-0038 EXACTLY AS PLANNED.` The approved plan explicitly
retains predecessor closure as a gate. That approval is retained; another
TASK-0038 plan or repeated approval is not required. The earlier instruction
to resolve the blocker first authorized this bounded prerequisite repair.
No folder or segment implementation was started. An execution-order question
about completing the remaining predecessor phases versus an explicit sequencing
exception was pending when the integrated test reached another failure; no
exception or test waiver is assumed.

### Proven cause and correction

The active task pack passed all 174 manifest entries. Baseline and final HEAD
are `d7c6021f58437e11725fba7bdabf271815d4b3a3`, on clean-at-start `main`, matching
the local `origin/main` reference without fetching. The platform was Arch Linux,
Plasma/KWin 6.7.5-1 and Qt base 6.11.2-3 / Declarative 6.11.2-2.

A fresh targeted build reproduced the original grouped-window assertion at
line 157 in private virtual KWin Wayland. A private D-Bus trace showed actual
`windowAdded` messages for both fixture windows, addressed to
`local.WindowWatcher`, followed by `org.freedesktop.DBus.Error.UnknownInterface`:
`No such interface 'local.WindowWatcher' at object path '/WindowWatcher'`.

Without explicit class metadata, [Qt's interface-name generation](https://github.com/qt/qtbase/blob/v6.11.2/src/dbus/qdbusmisc.cpp)
depends on the application name. The fixture has a different application name
from the main executable. `src/WindowWatcher.h` now declares
`Q_CLASSINFO("D-Bus Interface", "local.WindowWatcher")`, preserving the existing
KWin script contract independently of executable identity. This is the only
functional change: two added lines including its explanatory comment. No
window rule, script, action implementation, assertion or timeout was changed.

### Verification and first unresolved boundary

| Check | Result |
| --- | --- |
| Fresh ON Debug configure and complete serial build | PASS |
| Original standalone grouped-window reproduction | FAIL at line 157; 2 passed, 1 failed including setup/cleanup; 15,298 ms |
| Unchanged standalone test after the interface correction | PASS, 3/3 QtTest cases, 446 ms; grouping, title update, minimize/restore/activation and removal; no UnknownInterface reply |
| Full CTest with stop on first failure | FAIL, 54 passed, 1 failed, 12 not run of 67 registered; 75.37 seconds |
| Staged private renderer suite inside the full run | PASS, 6/6 QtTest cases |
| Staged grouped-window fixture | Both window arrivals, grouping, title update and minimized readback passed; activation request accepted, but the second window remained minimized |
| First remaining failure | `PanelWindowCapabilityTest::groupedWindowsFollowLiveKWinUpdates`, `tests/PanelWindowCapabilityTest.cpp:184`, `!rowForId(secondId).value("minimized").toBool()` |
| Later private popup/editor/applet stages | NOT EXECUTED after the restore failure |
| TASK-0038 Phase A: layouts, unavailable paths, shared geometry, open-panel guard | NOT EXECUTED; implementation not started |
| TASK-0038 Phase B: single-segment equivalence, independent persistence/rendering, capabilities, entry ownership | NOT EXECUTED; implementation not started |

The standalone restore success does not establish integrated reliability.
The staged failure's cause is unproved. Read-only inspection traced the request
through `PanelWindow::activateDockWindow`, DockModel and
`KWinActionBridge::runScript`; no speculative change, retry of the failed suite
or later-phase execution followed. TASK-0036's remaining Phase A acceptance
and Phase B matrix, and TASK-0037's Phase A completion and Phases B/C remain open.

The task-owned root was `/tmp/archdock-task0038-prerequisite.wJ0qaf`. Commands
used its `build` subdirectory, created fresh for this investigation:

```bash
cmake -S '/mnt/F/Arch Dock' -B "$task38_root/build" \
  -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=ON
cmake --build "$task38_root/build" --target panel-window-capability-test --parallel 1
# The private diagnostic ran this unchanged QtTest function before/after repair:
# panel-window-capability-test groupedWindowsFollowLiveKWinUpdates
cmake --build "$task38_root/build" --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$task38_root/no-parent-bus" \
  ctest --test-dir "$task38_root/build" \
  --output-on-failure --parallel 1 --stop-on-failure
```

After a proved correction to the restore failure, the pending integrated target
is the same full CTest command; a focused staged reproduction is:

```bash
env DBUS_SESSION_BUS_ADDRESS="unix:path=$task38_root/no-parent-bus" \
  ctest --test-dir "$task38_root/build" \
  -R '^rendering-import-smoke$' --output-on-failure --parallel 1
```

All execution used private D-Bus/XDG state and virtual KWin Wayland. The staged
smoke used `/tmp/archdock-rendering-import.ucAHrp`. These results do not prove
personal-desktop or hardware acceptance. File change order was WindowWatcher
interface declaration, this current-state record, then the release checklist.
The staged root is absent, no process retains either private environment or
task-owned executable/working directory, and the task-owned build, traces and
diagnostic harness were removed after recording this evidence. Whitespace
and local-document-link checks passed. Only the three stated files remain
modified and unstaged.

## TASK-0038 prerequisite — restore and fallback repair, 2026-09-19

**Status: COMPLETE for the failure repair.** The owner's instruction, "Some
things have failed, don't leave task with failed things", authorized resolving
the outstanding failures and continuing the existing verification sequence.
No replacement TASK-0038 plan, test waiver or sequencing exception was used.
Baseline and final HEAD are `7c16581485a3164ff379f7bca3c651a7bc267c58`.
The active pack passed all 174 manifest entries. Platform: Arch Linux,
Plasma/KWin 6.7.5-1, Qt base 6.11.2-3, Declarative 6.11.2-2 and
KCoreAddons 6.30.0-1.

### Causes and changes, in file order

1. `src/KWinActionBridge.cpp`: the bridge previously called KWin's global
   `start` after loading each action. [KWin's implementation](https://github.com/KDE/kwin/blob/Plasma/6.7/src/scripting/scripting.cpp)
   reapplies installed package enablement during that call. A private native
   probe using the actual watcher package metadata proved that its loaded
   state changed from true to false after global `start`. This removed window
   updates after activation and left the model's minimized state stale.
   Per-script `run` plus action-only cleanup preserved the watcher in the
   same probe. The bridge now uses that existing watcher API pattern, checks
   the returned script ID and execution reply, and cleans its own script
   after KWin's completion reply instead of a fixed 500 ms timer.
2. `tests/RendererCapabilityTest.cpp`: the first focused smoke then exposed
   an intermittent resource-limit test failure before reaching restore.
   Temporary readback logging proved that the injected geometry changed from
   1,000 entries / budget exceeded / resource-limit fallback to two entries /
   within budget / no fallback after rotation updated. `QObject::setProperty`
   retains a QML binding; [Qt documents that `QQmlProperty::write` detaches it](https://doc.qt.io/qt-6/qtqml-cppintegration-interactqmlfromcpp.html).
   Fault injection now uses the latter and checks the retained 1,000-entry
   value. The original fallback and deferred delegate-destruction checks
   remain. Temporary logging and its diagnostic delay were removed.
3. `docs/CURRENT_STATE.md`: current status, causes, fresh results and remaining
   phase boundaries.
4. `docs/RELEASE_CHECKLIST.md`: matching verification record without claiming
   completion of unimplemented phases or release acceptance.

### Verification

| Check | Result |
| --- | --- |
| Active pack manifest | PASS, 174/174 |
| Native KWin probe | Global start unloads the staged watcher; per-script run and cleanup preserve it |
| Fresh ON Debug configure and complete serial build | PASS |
| Initial focused smoke | FAIL at renderer delegate cleanup; restore not reached |
| Focused diagnostic | Proved that the live geometry binding overwrote the resource-limit fault |
| Complete build after the test correction | PASS |
| Corrected staged smoke | PASS, 1/1 CTest, 82.07 seconds |
| Final full serial CTest | PASS, 67/67, zero failed, zero not run; 139.28 seconds |
| Staged smoke inside final full suite | PASS, 82.19 seconds |
| Private renderer suite | PASS, 6/6 QtTest cases; 34,878 visible pixels, motion, parts, concealment, reduced motion, fallback and bounded recovery |
| Private grouped-window fixture | PASS, 3/3 QtTest cases; arrivals, grouping, title, minimize, restore, active state and removal; original assertions unchanged |
| Private preview popup | PASS, 13/13 QtTest cases |
| Private Icon Properties and mesh editor interaction | PASS, 4/4 QtTest cases |
| Private energy pixels / surface integration | PASS, 3/3 and 19/19 QtTest cases |
| Real staged native/free applets | PASS; energy/perspective themes, icon styles, quality changes, rotation and owned-host cleanup |
| Repeated theme changes | PASS; 16 changes, private PlasmaShell RSS growth -124 kB in the final run |

The fresh task-owned directory was `/tmp/archdock-restore-repair.Cy0bUp`.
Commands used its `build` subdirectory:

```bash
cmake -S '/mnt/F/Arch Dock' -B "$repair_root/build" \
  -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=ON
cmake --build "$repair_root/build" --parallel 1
# After the narrow diagnostic, rebuild the corrected test and all targets:
cmake --build "$repair_root/build" --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$repair_root/no-parent-bus" \
  ctest --test-dir "$repair_root/build" -R '^rendering-import-smoke$' \
  --output-on-failure --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$repair_root/no-parent-bus" \
  ctest --test-dir "$repair_root/build" \
  --output-on-failure --parallel 1 --stop-on-failure
```

Runtime checks used disposable private D-Bus/XDG, virtual KWin Wayland and
real staged Plasma applets. No personal desktop, physical GPU, monitor/hotplug
or full release acceptance is claimed. OFF/AUTO builds and the remaining
TASK-0036 Phase B restart matrix were not run for this bounded repair.

Both previously failing boundaries now have positive integrated evidence;
there is no failed check left in the final suite. TASK-0038 Phase A's four
criteria (live layouts, unavailable-path safety, shared geometry, open-panel
guard) and Phase B's four criteria (single-segment equivalence, independent
render/order/persistence, capability filtering, entry ownership) remain
**NOT EXECUTED**, because those features have not been implemented.

All private session roots and processes were cleaned. The task-owned build,
diagnostic scripts and logs were removed after recording the evidence.
Whitespace and local-document-link checks passed. The four files above remain
modified and unstaged; the owner controls Git closure.

## TASK-0036 — resumed Phase B implementation, 2026-09-23

### Authority, scope and reuse

The owner requested resuming after quota interruption. The original
TASK-0036 plan and literal implementation approval were recovered from local
session history, together with the later approvals for TASK-0037 and TASK-0038.
The active consolidated pack's 174 manifest entries passed integrity checking.
Current source and the clean `a7f3665` checkout confirmed that Phase B remained
the earliest incomplete predecessor. No replacement plan was written.

The existing native Qt Quick `Loader`, Qt Quick 3D textures, capability resolver,
revisioned editor transaction and private Plasma harness were reused. The
loader already implements the declared baked/skinned/procedural fallback chain
and reports the requested/effective tier and reason; duplicating it was not
needed. This phase adds no persisted enable flag, renderer or host controller.

### Implemented behavior

- Studio's main **3D rendering** switch edits `rendererTier`. It is shown only
  for a supported renderer, valid 3D theme, capable preview consumer and
  declared ordinary fallback. Turning it off chooses available baked 2.5D,
  skinned 2D, then procedural 2D.
- The backend removes `scene3DQuality` from the editable projection while 3D
  is off and rejects attempts to change it. Unchanged inactive values retain
  the existing transaction semantics. Apply, Cancel and reopening remain
  revisioned operations.
- Runtime fallback hides quality and unsupported mesh-part mechanisms while
  preserving supported ordinary presentation controls. Existing metadata
  preserves saved 3D intent independently of effective tier and fallback reason.
- The real renderer test now keeps a panel alive through missing 3D resources,
  valid baked 2.5D fallback, missing baked artwork, procedural fallback and 3D
  recovery. It checks saved intent, entry geometry, rendered frames and
  destruction of the replaced mesh renderer.
- The private harness restarts only its identity-verified staged service,
  captures the replacement D-Bus owner and checks the same native/free host
  IDs and tokens. It rejects duplicate free hosts, checks saved renderer intent
  and exercises rendered quality changes after enabled-renderer recovery.

### Failure diagnosis and corrections during this resume

The first expanded smoke stopped because its new switch lookup walked
`QObject` ownership. A diagnostic proved that the visible `QQuickSwitch` was
in the visual tree but absent from `findChildren()` results. The test now walks
`QQuickWindow::contentItem()` and visual children/parents, as described by
[Qt's visual-parent documentation](https://doc.qt.io/qt-6/qtquick-visualcanvas-visualparent.html).
Temporary diagnostics were removed; the real mouse clicks and transaction
assertions remain.

That correction exposed a separate SIGSEGV on the real 3D-off click. The
render-thread stack and installed-library disassembly identified
`QQuick3DTexture::updateSpatialNode`'s `afterSynchronizing` callback, casting a
released `QSGDynamicTexture`. Arch Dock disabled the mesh-only
`ShaderEffectSource` while the 3D texture still referenced its provider.
[Qt 6.11.2's texture implementation](https://github.com/qt/qtquick3d/blob/v6.11.2/src/quick3d/qquick3dtexture.cpp)
disconnects those callbacks when `sourceItem` changes. `PanelScene3D.qml` now
clears glyph and tile source items whenever their mesh visual becomes inactive.
This uses the native lifetime API; it adds no delay, retry or alternate renderer.
The same private click/fallback test then passed, followed by the full ON suite.

AUTO later exposed a missed mouse click in the new fixture. The first
instrumented run passed, so it was not treated as proof of a correction. A
deterministic Appearance-form recreation then reproduced the original failure:
the switch moved from `(198, 340)` to `(896, 399)` during click delivery and
remained checked. Its visual object existed before Qt completed layout.
The fixture now uses [Qt Quick Test's native polish wait](https://doc.qt.io/qt-6/qquicktest.html)
before both clicks and retains the form recreation as regression coverage.
`Qt6::QuickTest` is linked only to the test target. Diagnostic logging was
removed; no fixed sleep, repeat click or assertion relaxation was added.

The stronger form-recreation case then exposed the missing part of texture
teardown. A core inspection with the installed Qt debug symbols proved that
the `QQuickShaderEffectSource` still existed but had `m_texture = nullptr`,
`m_provider = nullptr` and `QQuickItemPrivate::window = nullptr`. Its 3D
texture still referenced it, and the 3D scene manager still had the Studio
window. The callback reached a freed scene-graph node through the old layer.
Gating only on `meshVisualActive` was insufficient. `PanelScene3D.qml` now also
requires each source and its renderer to share the same non-null
[native attached window](https://doc.qt.io/qt-6/qml-qtquick-window.html#window-attached-prop).
Detachment clears `Texture.sourceItem` and disconnects the native callback
before the next sync. No provider-retention workaround or fixed teardown delay
is used. The corrected focused smoke and all three final full suites passed.

### Verification record

All builds used fresh task-owned directories below
`/tmp/archdock-task0036-resume.8GqtDb`, with serial compilation and CTest.
The parent test environment has a nonexistent D-Bus socket; runtime harnesses
create their own private session, XDG roots, staged install and virtual KWin.

| Gate | Result |
| --- | --- |
| Active pack integrity | PASS, 174/174 |
| Focused backend gating/transaction cases | PASS, 4/4 QtTest results |
| Capability QML cases | PASS, 18/18 |
| Static Studio contract, shell syntax, whitespace | PASS |
| Fresh ON, OFF and AUTO configure and complete serial builds | PASS in all three modes |
| Final corrected focused private smoke (AUTO) | PASS, 1/1, 80.92 seconds |
| Final full ON CTest | PASS, 67/67, 136.11 seconds; private smoke 80.76 seconds |
| Final full OFF CTest, optional dependency discovery disabled | PASS, 67/67, 125.93 seconds; private smoke 71.06 seconds |
| Final full AUTO CTest | PASS, 67/67, 137.79 seconds; private smoke 81.39 seconds |
| Temporary cleanup | PASS: private roots/processes/builds removed; both OS diagnostic cores removed and absence verified in the authorized continuation |

The ON and AUTO private checks passed real renderer pixels/motion/parts/fallback (6/6),
grouped windows (3/3), popup interaction (13/13), Icon Properties and mesh
editor interaction (4/4), energy pixels (3/3), and surface integration (19/19).
Actual native/free applets, service restart, quality recovery, theme cycles,
resource bounds and owned-host cleanup passed. These are private virtual
Wayland results, not personal-desktop, physical GPU/monitor or release acceptance.

The OFF private run confirmed that the real Studio switch and details are
absent, while saved `true3d` intent resolves to `procedural2d` with
`renderer-not-installed`. Restart retained the same native/free hosts and saved
intent. It does not claim to render a 3D frame in an OFF build.

All 67 CTest entries ran in each final suite. Within the ordinary offscreen
invocations, the KWin grouped-window and Wayland energy-pixel cases skip by
design; both ran and passed inside each private smoke. The separate live source
archive/catalog comparison was not executed because that optional external
archive/root was not supplied. That historical asset-source check is outside
TASK-0036's renderer acceptance; it is not claimed as a pass here.

### Inherited acceptance criteria

| Phase / legacy criterion | Final status and evidence |
| --- | --- |
| A / TASK-0068: same logical motion profiles | PASS: shared motion/controller tests and enabled private mesh pixel, platform, emission and part checks |
| A / TASK-0068: no property conflicts or orphan animation after theme change | PASS: active fallback, loader/delegate destruction, bounded recovery and repeated live theme/resource checks |
| A / TASK-0068: reduced motion uses static states | PASS: motion policy and private renderer/presentation reduced-motion checks |
| A / TASK-0068: unsupported themes hide part controls | PASS: backend capability tests, runtime mechanism filtering and real Studio interaction |
| B / TASK-0069: 3D failure preserves the panel | PASS for exercised missing-module/backend/resource, invalid mesh, active fallback and source-detachment paths; real panel geometry/frames and recovery remain valid |
| B / TASK-0069: saved intent and availability stay truthful | PASS: requested/effective/reason metadata, OFF fallback and service restart persistence |
| B / TASK-0069: detailed controls disappear off/unsupported | PASS: backend field rejection, QML filtering and enabled/disabled real Studio switch interaction |
| B / TASK-0069: ordinary 2D/2.5D remain usable | PASS: full OFF suite, baked-to-procedural live fallback, ordinary applets and recovery |

Both internal phase gates and the consolidated completion gate are PASS.
The results above are retained from the preceding implementation run, whose
source and tests the owner committed unchanged as `80d0820`. The cleanup
continuation verifies artifact removal; it does not claim a new test run.

Reproduction commands (each build completes before its CTest starts):

```bash
ulimit -c 0
root=$(mktemp -d /tmp/archdock-task0036-verify.XXXXXX)
cmake -S . -B "$root/on" -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=ON
cmake --build "$root/on" --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$root/no-parent-bus" \
  ctest --test-dir "$root/on" --output-on-failure --parallel 1 --stop-on-failure
cmake -S . -B "$root/off" -DCMAKE_BUILD_TYPE=Debug \
  -DARCHDOCK_ENABLE_QUICK3D=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Qt6Quick3D=TRUE
cmake --build "$root/off" --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$root/no-parent-bus" \
  ctest --test-dir "$root/off" --output-on-failure --parallel 1 --stop-on-failure
cmake -S . -B "$root/auto" -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=AUTO
cmake --build "$root/auto" --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$root/no-parent-bus" \
  ctest --test-dir "$root/auto" --output-on-failure --parallel 1 --stop-on-failure
```

The final verification disabled OS core generation with `ulimit -c 0`; this
does not mask crashes or change CTest failure handling.

### Changed files and rollback

The final change contains only TASK-0036 behavior, its regression coverage and
evidence. Implementation placement was adapted to existing code; no inherited
requirement was removed. The file purposes, in implementation order, are:

1. `src/panel/PanelWindow.cpp`: reject edits to inactive 3D quality.
2. `tests/PanelWindowCapabilityTest.cpp`: backend gating, actual switch clicks,
   Apply/Cancel/reopen and deterministic form-recreation coverage.
3. `qml/runtime/CapabilityModel.js`: reuse resolved choices for the off tier
   and runtime part-control filtering.
4. `tests/tst_CapabilityModel.qml`: declared fallback order and mechanism cases.
5. `qml/runtime/SettingsPopup.qml`: main switch and effective-tier detail gates.
6. `tests/RendererCapabilityTest.cpp`: rendered baked/procedural failure and
   recovery checks, retaining existing resource assertions.
7. `tests/run-rendering-import-smoke.sh`: identity-checked private restart and
   host/persistence/frame observations.
8. `qml/ArchDock/Rendering/optional3d/PanelScene3D.qml`: release texture consumers
   when the source becomes inactive or leaves the renderer window.
9. `CMakeLists.txt`: native Qt Quick Test polish support for the existing test
   target only; production dependencies are unchanged.
10. `docs/shared-renderer.md`, `docs/CURRENT_STATE.md` and
    `docs/RELEASE_CHECKLIST.md`: behavior, evidence and closure limits.

The owner can review and reverse this bounded diff against baseline `a7f3665`,
after preserving any subsequent work. No schema migration or live desktop
rollback is needed. No automatic restore, staging or commit was performed.
The suggested commit `Complete optional Arch Dock 3D support` would contain
only this consolidated task's changes.

### Closure and continuation boundary

All nine recorded disposable private-session roots are absent. A final
executable/environment identity scan found no task-owned runtime or build
process. After recording results, the exact owned
`/tmp/archdock-task0036-resume.8GqtDb` tree was removed and its absence verified;
this removed all three builds, staged artifacts, logs, diagnostics, extracted
core and downloaded debug-symbol cache. No unrelated temporary directory or
tracked build tree was removed.

The initial noninteractive removal of the two OS-managed diagnostic cores
failed because administrator authentication was required. The owner then
authorized authenticated cleanup. Only the two exact previously identified
`panel-window-ca` dumps for PIDs `96035` and `204088` were removed; both paths
were verified absent. No credential was written to repository or task files.
The administrative blocker is resolved and TASK-0036 is complete.

At TASK-0036 closure, TASK-0037 Phases B/C and TASK-0038 Phases A/B remained
unimplemented. Their approved plans were retained. No delegated or parallel
agent, staging, commit, push, global installation or personal Plasma mutation
was used.

## TASK-0037 — resumed window interaction, 2026-09-23

Baseline: clean `80d0820b76f9c816be75f19fb7450aab1b5a1f0e`, matching the local
`origin/main`. No fetch was performed. The retained plan and literal approval
were recovered from the September 19 local session; no replacement plan or
repeated approval was requested. The active pack again passed all 174 checksums.

Phase A's existing implementation was reused. Its popup test now also opens
native `PlasmaCore.Dialog` windows for top, bottom, left, right, free-ring-right
and free-arc-up directions. It checks outward placement and screen bounds.
The initial fixture read a QML `Window.screen` property that the native Dialog
type does not expose. Using Qt's attached `Screen` on the content item fixed
that test API error without changing any placement assertion.

- Fresh AUTO configure and complete serial build: PASS.
- Corrected private staged smoke: PASS, 1/1, 82.14 seconds; all six placement
  cases passed, with 19/19 popup results and 3/3 live grouped-window results.
- Final Phase A full CTest: PASS, 67/67, 141.19 seconds.
- Phase A inherited criteria: PASS for individual titles/states, usable
  no-thumbnail fallback, last-window dismissal and tested edge/free directions.

Phase B reuses the existing exact-ID KWin bridge and adds state/capability
validation for activate, minimize, restore and close. The bridge also checks
KWin's current capabilities at dispatch. New Instance and named desktop-file
actions use [KServiceAction](https://api.kde.org/kserviceaction.html) and
[KIO::ApplicationLauncherJob](https://api.kde.org/kio-applicationlauncherjob.html),
as the retained plan specified. KDE's native desktop-file authorization is
checked before exposing or dispatching launches. Hidden, empty and unknown
actions are refused. Existing activation outcomes are unchanged; the new
asynchronous launcher return means accepted, not verified application startup.

Launcher dispatch checks current panel membership. Free desktop entries keep
their own identity while window actions use the merged running application ID.
Shared preview buttons target exact IDs, keep ordinary title fallback, and do
not activate their parent row. Menus preserve Pin/Unpin and Icon Properties,
hide invalid transient actions, and release their guards on dismissal.

Current Phase B targeted evidence:

- Native dependencies configure and affected backend builds: PASS.
- Dock model: 11/11 PASS, including actual harmless native launches, stale
  actions, wrong application IDs and changed window capabilities.
- Panel backend: 24 PASS, 1 private-only grouped-window case skipped in this
  ordinary offscreen invocation; the private live case passed 3/3.
- DockEntry: 15/15 PASS, including actual menu clicks, route arguments and guards.
- Shared popup action checks and host-neutral renderer contract: PASS.
- Two new test-fixture errors were corrected from direct evidence: signal
  arguments need element comparisons, and visibility must be tested with the
  menu open. The corrected targeted checks pass; production assertions remain.

- Phase B full build: PASS; private staged smoke: PASS, 1/1, 82.31 seconds,
  including 20/20 popup results and exact-ID live minimize/restore/activate/close.
- The first full run stopped at an older panel-registry fixture that requested
  activation while leaving `canActivate` false. The fixture now explicitly
  grants the capabilities its unchanged assertions require. The corrected
  panel-registry check passed 1/1; the final full suite passed **67/67 in
  138.18 seconds**. No Phase B failure remains.

### Phase C and consolidated closure

**PASS: TASK-0037 is complete.** The final fresh AUTO build passes, and the
final serial full suite passes **68/68 in 203.89 seconds**. The original
`rendering-import-smoke` passed in **82.37 seconds**; the new
`window-interaction-smoke` passed in **64.00 seconds**. The latter also passed
its focused corrected gate in 64.59 seconds. The complete original popup
suite passed **21/21** under private Wayland, including six placement cases.

The new gate extends the existing private-session harness and fixture. It
uses GTK4 windows and KWin's native EIS input API through the already installed
Python GObject and libei libraries. It verifies the captured compositor PID,
executable, private D-Bus and runtime/display environment before input. The
virtual interaction compositor uses `setpriv --no-new-privs` so Arch's KWin
file capability does not prevent those identity reads. No ambient desktop
input or user KWin rule/configuration change is used. Observation hooks exist
only in disposable staged applets. Private Studio is closed by its exact
observed ID, application and title before testing desktop-widget input.

Final native results:

- All four native edges and free ring/arc layouts pass actual menu opening,
  grouped rows, exact minimize/restore, other-window isolation, popup guards,
  Escape, outside dismissal and absence of accidental icon launch.
- Actual row activation restores and selects the intended window. Closing one
  window leaves the other; closing the last dismisses the popup and releases
  its guard. The removed ID is rejected using its canonical application ID.
- The watcher survives an explicit private KWin reconfiguration and forwards
  the subsequent title update.
- Unit/QML coverage includes rapid replacement, title/thumbnail fallback,
  current capabilities, safe desktop actions, stable interaction owners and
  stale-ID rejection. Existing host-neutral module checks were reused.

The runtime matrix exposed five production gaps, all corrected and covered:

1. KWin unloads a manually loaded script whose ID matches a disabled installed
   package when it reloads configuration. The existing watcher now uses its
   own runtime ID, `org.archdock.windowwatcher.runtime`, and cleans the legacy
   instance before loading. It still runs only its own script. The lifecycle
   helper's identity checks were updated; its separate full scenario was not
   rerun or claimed as additional evidence.
2. Thin native panels clip an in-scene menu. DockEntry now uses Qt's native
   popup window so its actions remain reachable.
3. Native panels can instantiate only the compact representation. Preview
   routing and host reports now use the actual visible representation.
4. Plasma's PopupMenu type supplied only the X11 window hint in the inspected
   implementation. AppletPopup provides the Wayland role that keeps preview
   controls above their owning dock.
5. Replacing the JavaScript entry snapshot destroyed every Repeater delegate,
   closing a context menu during active-window updates. PanelScene now rebuilds
   delegates only when entry identity/order changes and binds their state to
   the current snapshot. A focused regression failed before this correction
   and passed afterward.

Native research: [KWin scripting lifecycle](https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/scripting/scripting.cpp),
[KWin EIS backend](https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/plugins/eis/eisbackend.cpp),
[Plasma Dialog roles](https://raw.githubusercontent.com/KDE/libplasma/v6.7.5/src/plasmaquick/dialog.cpp),
[Qt popup types](https://doc.qt.io/qt-6/qml-qtquick-controls-popup.html#popup-type),
and [Qt Repeater lifetime](https://doc.qt.io/qt-6/qml-qtquick-repeater.html).

Fixture corrections preserved the required assertions: native geometry and
fresh hover observations synchronize input after panel relocation; menu
bounds determine outside-click points; only the visible representation is
observed; only supported settings deltas are sent. The existing Icon
Properties transaction test uses synthetic Qt events, which have no Wayland
input serial. Its test-owned menu now uses Popup.Item, retaining all actual
Apply/Cancel/Reset assertions. Production native menus are independently
exercised with EIS. The corrected original smoke passed 81.45 seconds before
the final complete run. Earlier failing runs are superseded by the final
68/68 gate; no current build or test failure remains.

Reproduction (with a fresh external configured build):

```bash
cmake --build "$task37_build" --parallel 1
ulimit -c 0
env DBUS_SESSION_BUS_ADDRESS="unix:path=$task37_root/no-parent-bus" \
  ctest --test-dir "$task37_build" --output-on-failure --parallel 1 --stop-on-failure
```

The 41 private session roots recorded by the resume logs are absent. No
process retained those private environment roots at closure. The exact owned
build/log root `/tmp/archdock-task0037-resume.mlDbVP` was then removed after
checking executable/working-directory identities; its absence was verified. Private virtual KWin/Plasma acceptance
is not personal-desktop, physical GPU/monitor, hotplug or release acceptance.

The TASK-0038 plan and literal approval were recovered from the September 19
local session (plan record 620, approval record 627). They remain authoritative
for folder Phase A followed by segment Phase B. At TASK-0037 closure,
TASK-0038 code had not yet started; its continuation is recorded below. No new plan, approval, agent, staging, commit, push or global install
was used for this predecessor closure.


## TASK-0038 — Phase A complete; Phase B pending, 2026-09-23

The retained September 19 plan and literal approval are reused. Predecessors
TASK-0036 and TASK-0037 are now closed; the latter's final current-source gate
passed 68/68 before this implementation began. HEAD remains `80d0820`; no
staging, commit or push was performed. The working diff includes predecessor
closure and TASK-0038 work, so a future owner commit must account for both.

The fresh ON Debug Make build is
`/tmp/archdock-task0038-resume.lR1Az5/phase-a`. The full serial build passed;
its log is `phase-a-build.log` in the parent directory. Initial focused results:

- A1: `FolderContentModel` provides one nonrecursive Qt `QDirListing` snapshot
  for both hosts. At most 48 rows and one truncation probe are inspected;
  only that bounded page is sorted. Empty/unavailable/error states are
  explicit. IDs encode each immediate child URL. Links, executable entries,
  special files and unavailable children are blocked from selection.
  `folder-content-model-test`: PASS, 1/1, 0.02 seconds.
- A2: the existing DockModel folder API reuses the provider. Additive
  `panelFolderSnapshot` and `openPanelFolderChild` methods resolve against
  authoritative panel content and revalidate children at dispatch. KIO's
  `OpenUrlJob` has executable launching and execute-choice dialogs disabled;
  a positive asynchronous result means accepted, not observed app startup.
  DockModel routing test: PASS, 1/1, 0.21 seconds. Panel backend routing test:
  PASS, 1/1, 2.22 seconds, covering shared/free parity, cross-panel rejection,
  unsafe entries, child removal and deleted folders. The existing registry
  folder snapshot/reorder test is retained for the full gate.

Research uses the installed Qt/KF headers and native
[QDirListing](https://doc.qt.io/qt-6/qdirlisting.html),
[OpenUrlJob](https://api.kde.org/kio-openurljob.html) and
[Plasma Dialog](https://api.kde.org/plasmaquick-dialog.html) APIs. The KIO
build dependency was already supplied by TASK-0037 and is reused.

A3-A6 are implemented. `LayoutEngine.expansionGeometry` bounds all five
layouts and exposes a selectable strip for stacked children. `FolderExpansion`
reuses `IconScene`, `IconMotionController` and canonical positions, limits its
scrolling viewport to available space, supports keyboard/Escape/pointer input,
and preserves stable child identity. Legacy layout names remain durable but
explicitly fall back to Fan; editor choices are exactly fan/grid/stack/arc/ring.
`FolderExpansionHost` reuses Plasma's AppletPopup role and PanelScene anchors.
`main.qml` routes panel-scoped asynchronous snapshots with request invalidation,
entry/host authority and popup guards. Folder clicks never fall through to
root launching when expansion is requested. Duration/easing/expand-on-click
controls use the existing schema and transactional editor.

Focused checks after these changes: geometry PASS (0.75 s), visual geometry
PASS (0.16 s), shared folder content/native adapter PASS (0.76 s), DockEntry
PASS (0.62 s), schema round trips PASS (0.02 s), backend/editor PASS (1.94 s).
The first new dense-popup test found a height overflow caused by the combined
fallback/truncation labels. The viewport now subtracts actual visible label
heights; the same test passes. A standalone `qmllint --bare` invocation lacked
Qt import paths and is not validation evidence; runtime QML tests load the
component successfully. No current targeted build/test failure remains.

A7's `folder-interaction-smoke` passed in the final full suite (64.01 s). It reuses the
private KWin EIS matrix in `visibility-window.py`, with real temporary folders
and a private document handler. It covers five layouts on native/free hosts,
four native edges, keyboard/pointer selection, no accidental root launch,
empty folders, dismissal and presentation guards. Reduced motion is covered
by the shared component test; live observations check the actual saved setting.
The matrix uses `ARCHDOCK_RENDERING_INTERACTIONS=1` plus
`ARCHDOCK_RENDERING_FOLDERS=1`; no personal desktop input or application handler
is used. Python syntax and shell syntax checks pass.

The first live run exposed a resident-service lifetime defect: after the
controlled handler received the selected file, destruction of KJob's last
QEventLoopLocker initiated application shutdown even though Studio's window
closure was already disabled. `src/main.cpp` now also calls
`setQuitLockEnabled(false)`. This necessary placement adaptation preserves the
approved KIO opening behavior and persistent backend lifetime; no feature scope
was added. The live matrix now requires the same D-Bus owner after each launch.
Native references: [Qt quit locking](https://doc.qt.io/qt-6/qcoreapplication.html#quitLockEnabled-prop)
and [KJob private lifetime](https://raw.githubusercontent.com/KDE/kcoreaddons/v6.30.0/src/lib/jobs/kjob_p.h).
A later fixture run intermittently waited for a stale popup size during a
layout change. The geometry waiter now refreshes QML size/child coordinates
while matching the actual KWin window; native bounds and identity assertions
remain intact. The diagnostic run passed (64.26 s), and the final corrected
matrix passed in the complete suite.

Final Phase A gate: fresh ON Debug build PASS; **71/71 CTests PASS, 266.85 s**.
The staged rendering gate passed in 82.10 s, grouped-window regression in
63.99 s, folder interaction in 64.01 s. The final log is `phase-a-ctest.log`.
All five layouts opened the exact selected document through the private
handler on native and free hosts; all four native edges, stack keyboard
selection, popup guards, outside/Escape dismissal and empty folders passed.
Unavailable/deleted roots and stale/unsafe child rejection are model/backend
proof; reduced motion is shared-QML proof. No current build/test failure remains.
`git diff --check` passes. The five private runtime roots named by retained
logs are absent and no process retains those roots. This is private virtual
KWin/Plasma proof, not physical GPU/monitor or personal-desktop acceptance.
An additional filesystem check found no remaining
`/tmp/archdock-rendering-import.*` directory, including the diagnostic pass
whose terse CTest result did not retain its temporary root name.

Next is the retained Phase B plan: typed bounded segments, equivalent single-
segment migration, revisioned ownership-safe transactions, capability-filtered
sources, independent shared surfaces/input geometry, transactional Studio
controls, persistence/native/free runtime acceptance and a fresh full gate.
Phase B and TASK-0039 have not started; **TASK-0038 remains incomplete** until
segments and the consolidated final gate pass. No replacement plan or approval
is needed. The task build/log root is retained for this active continuation.


## TASK-0038 Phase B continuation — 2026-09-27

The owner saved the preceding work as `032420afeee8ac2453f4896a57da7c2e60cf213e`
(`Arch Dock  task 36-38  repairs part 1`). `main` and local `origin/main` match;
the tree was clean on resume. No fetch was performed. The retained approval
and Phase B plan are reused. The pack's SHA256 manifest passes again.

The September 23 Phase A gate above is historical, verified evidence. The
old `/tmp/archdock-task0038-resume.lR1Az5` directory no longer exists.
The fresh ON Debug Phase B build is
`/tmp/archdock-task0038-phase-b-mczyks7j/build`; configuration passed.
KWin/Plasma remain 6.7.5, Qt is 6.11.2-3, and KF6/KIO are 6.30.0.

The preceding quota interruption occurred after adding `PanelSegmentDefinition`
and its parser/validation to `PanelDefinition.h/.cpp`. Those files are saved
in the new commit. This continuation implemented list persistence and an
idempotent migration of existing version-2 records to one inherited segment.
The revisioned backend now partitions authoritative entries, rejects foreign
and duplicate claims, persists/reorders/removes segments, and rejects unsupported
sources and renderer combinations. Native shared pins and free panel content
remain their existing separate authorities.

Independent linear procedural surfaces now compose through `LayoutEngine`,
`PanelScene` and `PanelSegment`, reusing existing presentation and motion
controllers. Default inherited segments retain the original rendering path.
Studio has nested draft controls and real entry projections; Cancel/Apply use
the existing editor transaction. Nonlinear/artwork surfaces and status providers
remain unavailable for segment customization; corner overrides require solid
backgrounds. No TASK-0039 status implementation was added.

Fresh narrow checks pass: model, content transaction, settings schema/settings
transaction, backend capability, registry persistence/rollback, geometry,
scene, preview, editor drafts, entry input and presentation guards. The new
scene tests prove unchanged inherited pixels, independent input ownership,
actual motion pixels in a visible window, and still reduced-motion frames.
The Phase B build and full serial gate passed: **71/71 CTests, 269.13 s**.
Staged rendering/Studio passed in 84.33 s; grouped-window interaction in 63.97 s.
The folder matrix also verifies native/free independent surfaces, exact entry
ownership, closed/open pointer behavior, popup guards, reorder, persistence and
foreign-claim rejection. The real Studio component verifies nested drafts,
preview ownership, reorder, Cancel, Apply and removal through its existing
handlers in private Wayland. Physical clicks are used for applet interaction;
Studio transaction handlers are invoked by the existing Qt test harness.

Corrections proven during this phase:

- Explicit claims reserve ownership when an entry temporarily stops matching
  its launcher/task source; the focused regression passes.
- Nested D-Bus segment payloads are decoded before strict model validation.
- Animated hover transfers from the segment surface to its owning icon without
  collapsing the segment; a visible-window regression reproduced and fixed it.
- Optional schema bounds are omitted when absent, preventing invalid QVariant
  values from aborting a D-Bus Studio snapshot reply. Descriptor regression and
  the native/free snapshot calls now pass. The single resulting OS crash dump
  (PID 23777) was removed and its absence verified.
- The private geometry probe stays loaded for the matrix and unloads during
  cleanup, avoiding KWin's script-ID reuse during repeated probe creation.
  Native reference: https://raw.githubusercontent.com/KDE/kwin/v6.7.5/src/scripting/scripting.cpp
- A final-run cleanup race occurred after every live assertion passed: the
  service name disappeared between NameHasOwner and GetNameOwner. Cleanup now
  accepts only NameHasNoOwner plus a second confirmed absence; capture/check/
  restart and process-identity assertions remain strict. Shell/Python syntax
  checks pass. No assertion was weakened or test skipped.

The separate fresh `final-on` configure/build passed. Its initial full run
stopped at test 59 solely on the cleanup race above (58 passed, 12 not run).
The corrected final full run passed **71/71, 272.29 s**; staged rendering/Studio
84.86 s, window interaction 63.95 s, folder/segment interaction 64.05 s. The log
was `final-on-ctest-corrected.log`. The separate fresh OFF configure and
`arch-dock`/`archdock-rendering-module` build passed, followed by **3/3** focused
checks: panel scene, folder expansion and live panel preview (2.56 s). This was
an OFF dependency/resource check, not another full OFF suite.

Final cleanup: all **12** private runtime roots identified in retained logs are
absent; no process retained their environment roots or the task build root.
No `/tmp/archdock-rendering-import.*` directory remains. The single diagnostic
core is absent. After recording the results, the task-owned
`/tmp/archdock-task0038-phase-b-mczyks7j` root (Phase B/final ON/final OFF builds,
logs and probes) was removed and its absence verified. Historical paths above
identify verification runs; they are not retained artifacts.

### TASK-0038 consolidated acceptance — COMPLETE, 2026-09-27

| Inherited criterion | Result and proof |
| --- | --- |
| A: Every visible folder layout works live | PASS: fan/grid/stack/arc/ring on native/free applets through real EIS pointer/keyboard interaction and controlled KIO document opening |
| A: Unavailable/deleted folders fail safely | PASS: model/backend rejection; empty popup is safely dismissible in the live matrix |
| A: Expansion shares geometry | PASS: LayoutEngine expansion outputs and QML geometry/input tests |
| A: Panel stays open during expansion | PASS: real popup guards on both hosts, dismissal releases the guard |
| B: One inherited segment matches prior behavior | PASS: idempotent migration, scene dimensions/anchors and equal rendered frames |
| B: Multiple segments render/order/persist independently | PASS: typed model, atomic persistence/reload/rollback, shared surfaces, Studio draft/Apply/Cancel and native/free runtime matrix |
| B: Unsupported features are hidden by capabilities | PASS: editor/backend capability tests; nonlinear/artwork/true-3D custom segments and status without a real provider are unavailable; corner overrides require solid backgrounds |
| B: Segments cannot consume each other's entries | PASS: duplicate/foreign claims and cross-segment reorder rejected; explicit reservations survive temporary source mismatch; runtime ownership remains exclusive |

Both internal phases are complete. The fresh final configure/build/full CTest,
staged install/private Plasma gates, bounded OFF check, whitespace validation
and artifact cleanup pass. All earlier failures described above are superseded
by the final passing gates; no build, test or runtime failure remains open.
The environment is Arch Linux, Plasma/KWin 6.7.5, Qt 6.11.2-3 and KF6/KIO 6.30.0.
Private virtual Wayland evidence does not claim personal-desktop, physical GPU,
monitor/hotplug or release acceptance. No manual personal-desktop checks ran.

Implementation placement follows the retained approved plan: existing content,
transaction, geometry, presentation, animation and Studio machinery is reused.
The bounded serialization and test-lifecycle corrections were needed to pass
this task's native verification; no feature scope or successor work was added.
Status providers belong to TASK-0039; no fabricated status or plugin API exists.

Changed files, in implementation groups/order, relative to the September 27
baseline:

1. `src/model/PanelDefinition.cpp`, `PanelSettingsSchema.h/.cpp`,
   `SettingsMigration.cpp`, `tests/PanelModelTest.cpp`: typed persistence,
   schema/list projection and equivalent migration.
2. `src/panel/PanelContentTransaction.h/.cpp`,
   `tests/PanelContentTransactionTest.cpp`: authoritative partition, exclusive
   identity claims and safe reorder/removal.
3. `src/model/PanelCapabilityResolver.h/.cpp`, `src/panel/PanelWindow.cpp`,
   `tests/PanelWindowCapabilityTest.cpp`: host capabilities, revisioned validation,
   D-Bus segment decoding and Studio/backend checks.
4. `qml/ArchDock/Rendering/LayoutEngine.js`, new `PanelSegment.qml`,
   `PanelScene.qml`, `tests/tst_DockGeometry.qml`, `tests/tst_PanelScene.qml`:
   shared runs/surfaces/input geometry, hover handoff and rendered motion proof.
5. `qml/ArchDock/Rendering/qmldir`, `CMakeLists.txt`: module/install registration.
6. `qml/runtime/SettingsEditorModel.js`, `SettingsPopup.qml`,
   `qml/ArchDock/Rendering/previews/LivePanelPreview.qml`,
   `plasma-dock-widget/contents/ui/main.qml`, `tests/tst_SettingsEditorModel.qml`:
   nested drafts, real entry preview, transaction controls and host integration.
7. `tests/PanelRegistryTest.cpp`, `tests/PanelSettingsSchemaTest.cpp`,
   `tests/visibility-window.py`, `tests/run-rendering-import-smoke.sh`:
   persistence/rollback, serializable descriptors and existing private runtime
   harness extensions/corrections.
8. `docs/shared-renderer.md`, this file, `docs/RELEASE_CHECKLIST.md`: behavior,
   acceptance, verification and cleanup evidence.

Verification commands used (the now-removed run root was
`/tmp/archdock-task0038-phase-b-mczyks7j`):

```bash
cmake -S . -B "$task38_root/final-on" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=ON
cmake --build "$task38_root/final-on" --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$task38_root/no-parent-bus" ctest --test-dir "$task38_root/final-on" --parallel 1 --stop-on-failure --output-on-failure
cmake -S . -B "$task38_root/final-off" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug -DARCHDOCK_ENABLE_QUICK3D=OFF
cmake --build "$task38_root/final-off" --target arch-dock archdock-rendering-module --parallel 1
env DBUS_SESSION_BUS_ADDRESS="unix:path=$task38_root/no-parent-bus" ctest --test-dir "$task38_root/final-off" -R '^(folder-expansion|panel-scene|live-panel-preview)-test$' --parallel 1 --stop-on-failure --output-on-failure
git diff --check
```

A future rerun must create a new task root. Baseline and final HEAD are both
`032420afeee8ac2453f4896a57da7c2e60cf213e`; all continuation changes remain
unstaged. No agent, staging, commit, push, global installation or personal-
desktop operation was used. The suggested commit contains TASK-0038 Phase B
and its required verification corrections only. The owner controls Git closure.
Suggested commit: `Complete TASK-0038 independent panel segments`.

## TASK-0039 continuation — BLOCKED at final gate, 2026-09-27

This resumes the approved implementation and preserves the owner's checkpoint
`45025655bbcd7369b44b07c26549e0be6657f76d`. No replanning, successor work or Git
writes were performed. Phase A remains complete on retained evidence
(`phase-a-ctest.log`: 75/75, 283.47 s). The combined Phase-B scenario now passes;
Phase B and consolidated closure remain incomplete because the full gate fails.

### Original blocker: Case A, production lifecycle reporting

The discriminator traced `setPanelVisible("bottom", false)` through the native
adapter result, owned Plasma configuration/widget IDs, actual KWin window,
QML host state, presentation publication, D-Bus report and content demand.
The API and native hiding succeeded: KWin reported `hidden=true`. Qt's
`QWindow.visible`, `Window.visibility` and the applet's visibility remained
visible, so no concealment event reached the existing presentation controller.
The old backend report therefore stayed `revealed`. The eight-second oracle
was correct; neither the timeout nor lifecycle semantics was changed.

The repair reuses the KWin watcher and existing native-visibility revision
transport. It observes Plasma dock frames and their native `hidden` state,
correlates exactly one frame to the host bounds, and feeds that observation
into the existing host-concealment/controller/report path. Matching requires
equal dimensions within one pixel and majority area overlap, allowing the
observed floating-panel client/compositor offset. Missing/ambiguous matches
remain unavailable; the cache is bounded at 128 frames. This read-only
correlation grants no mutation authority. Existing ownership-token checks
still govern panel changes. The attempted window-title correlation was
discarded after the discriminator showed an empty KWin caption despite the
Qt window title; it is absent from the final code.

Changed implementation files: `kwin-script/contents/code/main.js`,
`src/WindowWatcher.h/.cpp`, `src/panel/PanelWindow.h/.cpp`, and
`plasma-dock-widget/contents/ui/main.qml`. Existing badge/progress/status,
persistence, content transactions and coalescing implementations were reused.
`tests/PanelWindowCapabilityTest.cpp` covers observation identity, deduplication,
ambiguity, removal and bounds. `tests/visibility-window.py` records the native
visibility trace and verifies actual hidden state, one terminal concealment
publication, unchanged widgets and reveal of the same native window.

### Verification and first unresolved boundary

All logs/builds below are retained under `/tmp/archdock-task0039.IZhTqK`.
Each runtime CTest used the existing disposable virtual KWin/Plasma harness
and `DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0039.IZhTqK/no-parent-bus`.

| Check | Result |
| --- | --- |
| Native-frame unit case | PASS, 3 QtTest entries including init/cleanup; `frame-test.log` |
| Targeted visibility discriminator | PASS, 1/1, 63.82 s; concealment in 0.377 s; `visibility-frame-green.log` and `visibility-frame-green-details.log` |
| Exact previously blocked combined scenario | PASS, 1/1, 64.40 s; `phase-b-resumed.log` and `phase-b-resumed-details.log` |
| First final full suite | FAIL, exit 8; 61 passed, one failed, 13 not run, 219.26 s; `final-ctest.log` |
| Focused window-interaction correction | PASS, 1/1, 64.03 s; `window-input-corrected.log` and detailed companion log |
| Final incremental build | PASS, exit 0; `final-build-verified.log` |
| Latest final full suite | FAIL, exit 8; 62 passed, one failed, 12 not run, 283.49 s; `final-verified-ctest.log` |

The first final run exposed a test synchronization defect: the pointer helper
could accept a QML sample collected before EIS movement acknowledgement.
Its freshness baseline now follows that acknowledgement; existing hover,
geometry and timeout assertions remain. The targeted window gate passed,
and the latest full run passed that gate again (64.00 s), after the rendering
gate passed (88.92 s).

In the latest run, every combined folder/content/visibility assertion passed,
including all five folder layouts on native/free hosts, independent segments,
persisted controls, no duplicate widgets, hidden-update deferral, demand-paused
status sampling, reveal, source disconnect and temporary-status expiry.
**100 source updates produced one content revision**, compared with the
previous checkpoint's two. Native concealment published once in 1.397 s.

The mandatory post-scenario QML error check then failed:

```text
PanelSegment.qml:41:5: QML PanelPresentationController:
Binding loop detected for property "surfaceState"
```

`PanelSegment.qml` and `PanelPresentationController.qml` are unchanged from
the TASK-0038 checkpoint. The source chain inspected is the segment
controller's `surfaceState` -> segment `expanded` -> scene entry visibility/
hover -> segment presentation requests. The actual cycle and whether this
continuation triggers it are **unproved**. No assertion was bypassed, no
speculative controller change was made and the failed command was not blindly
retried. Execution contract section 3 requires BLOCKED at this unclear boundary.
The failure occurs after live assertions pass, so the combined CTest result
in this final run is still FAIL.

Exact successful targeted command and failed final command:

```bash
env DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0039.IZhTqK/no-parent-bus ctest --test-dir /tmp/archdock-task0039.IZhTqK/build --parallel 1 --stop-on-failure --output-on-failure -R '^folder-interaction-smoke$'
env DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0039.IZhTqK/no-parent-bus ctest --test-dir /tmp/archdock-task0039.IZhTqK/build --parallel 1 --stop-on-failure --output-on-failure
```

### Remaining closure and cleanup

The first unresolved boundary is the segment-controller binding loop. A
subsequent authorized continuation must prove its cause before correction,
then pass the exact failed gate and full suite. Shared-renderer/release
documentation closure and final artifact removal remain pending; no failed
criterion is delegated to a successor task. The completion acceptance remains
unchecked despite the passing targeted behavior.

All 11 private runtime roots referenced by retained task logs are absent,
and no process retains the task/runtime paths in its environment. The harness
performed cleanup; no personal desktop was touched. The retained build and
diagnostic logs deliberately remain for blocked-task resumption, so artifact
cleanup is not claimed complete. `git diff --check` passes. HEAD is unchanged;
all continuation changes are unstaged. No external network, dependencies,
global installation, background/delegated/parallel agent, staging, commit or
push was used. Private virtual Wayland evidence does not establish physical
GPU, monitor/hotplug, personal-desktop or release acceptance.

Suggested commit message after verified closure remains:
`Complete Arch Dock content systems`. The owner controls Git closure.

## TASK-0039 final closure — 2026-09-27

**COMPLETE; the owner committed this closure as `3ab470c`.** This section supersedes
the blocked checkpoint above without rewriting its historical evidence. The
owner explicitly authorized bounded diagnosis/repair cycles for mandatory
TASK-0039 gate failures, overriding execution-contract section 3 for this task.
The prior concealment repair and all Phase-A content work were preserved.

Entry HEAD was `6892c13e7827f4990c71ed2fe49a6462273ccc72`, with a clean tree.
The owner had committed the preceding continuation's ten files (+562/-11),
including the blocked report, as `Arch Dock - Complete Arch Dock content
systems`. HEAD remains unchanged. The subject did not establish completion;
the final current-tree evidence below does. No Git writes were performed.

### Proved binding graph and production correction

The retained and current `PanelSegment.qml`, `PanelPresentationController.qml`
and `PresentationStates.js` matched by SHA-256 before repair. The warning was
not caused by a stale generated copy. Minimal segment construction passed.
A small real `PanelScene` reproduction then failed in 83 ms when replacing
`orderedEntries` with an identical snapshot while a closed segment was hovered.
It required neither status data nor an application C++/D-Bus notification.

The closed edge, confirmed with stack traces in a disposable module copy, was:

```text
orderedEntries -> segmentLayout/run.definition -> onDefinitionChanged
  -> presentation.reset() / requestOpen() -> controller.machine
  -> surfaceStateOf(machine) -> segment.expanded -> entry visibility/input
  -> MouseArea.onExited -> runtimeState.hoveredEntry / pointerInside
  -> guardsActive -> evaluateDeferred() -> deliver(collapse) -> machine
```

The nested writes were real opening/collapsing transitions, not no-op
notifications. The reset discarded the guarded state during binding evaluation.
This is a runtime transition cycle exposed by content refresh, not an initial
construction cycle. TASK-0039 makes these refreshes frequent, but its status
provider is not necessary to reproduce the underlying segment defect. Native
Qt item/hover notifications carry the feedback; no Arch Dock C++ NOTIFY change
was needed.

`PanelSegment.qml` now routes definition updates through `updatePresentation()`
without resetting the controller. The existing state machine remains the sole
owner. The new `tst_PanelScene.qml` regression fails on binding-loop warnings and
asserts zero transient surface changes across identical content/configuration
refreshes, preserved entry hover, guarded preference changes, and collapse when
the pointer leaves, in both animated and reduced-motion modes. Both cases failed
before the production correction and pass afterward. No timer, deferred callback,
warning exception, duplicate controller or changed product semantics was added.

### Additional mandatory-gate harness corrections

Three distinct test defects were reproduced and corrected in
`tests/visibility-window.py` under the owner override:

1. **Configuration acknowledgement:** after a bottom-to-top move, compositor
   coordinates were correct, but the click trace still showed the previous
   `open:open` presentation profile. The requested collapsed profile arrived
   afterward and reset presentation during input. `configure()` now waits for
   the authoritative applet's observed profile to match the backend projection
   before proceeding. A fresh pointer sample alone was insufficient.
2. **Partial log records:** both interaction tests raised `JSONDecodeError`
   when the reader reached EOF during a logger write. A split-write probe against
   the actual `pump()` reproduced the exact unterminated-string error. The reader
   now retains incomplete lines until their newline. The probe verifies split
   prefixes/JSON, one delivery, and continued rejection of malformed complete
   records. Errors are not swallowed.
3. **Keyboard readiness:** the first complete final run passed 74 tests but
   failed folder dismissal. A focused discriminator reproduced Escape sent when
   the popup window was active but its content lacked focus; focus arrived later
   and the popup stayed open. Folder key delivery now waits for both native
   window activation and content focus. It uses the original eight-second bound.

Temporary pointer/stack tracing was removed. The small profile and focus
observations remain in the existing staged-only harness. No assertion, QML-error
scan or timeout was weakened; no arbitrary sleep or retry was added.

### Final verification accounting

The existing task-specific Debug/AUTO build at
`/tmp/archdock-task0039.IZhTqK/build` was reused. Source/build identity was checked;
the QML change triggered module reconfiguration/copying. Fresh incremental full
builds passed against the final source. The final suite deliberately omitted
`--stop-on-failure` and emitted JUnit accounting for all tests.

| Gate | Evidence and result |
| --- | --- |
| Phase A | Retained 75/75, 283.47 s; all corresponding checks also pass in the final current-tree suite |
| Segment regression | RED: both motion cases warn and make three unintended surface changes; GREEN: 4/4 QtTest entries including init/cleanup, zero warnings |
| Adjacent scene/presentation/guard CTests | 4/4 PASS, 5.17 s |
| Native concealment discriminator | 1/1 PASS, 64.05 s; actual hide -> one terminal report in 0.379 s; unchanged widgets and same-window reveal |
| Corrected window plus combined content gates | 2/2 PASS, 128.17 s (64.16 / 64.00 s) |
| First complete final run | 74 PASS, 1 FAIL, zero not run, 281.24 s; folder focus race described above |
| Corrected folder-focus gate | 1/1 PASS, 64.34 s; combined content/coalescing/visibility/disconnect/expiry all pass |
| Final build | PASS, exit 0, `closure-verified-build.log` |
| Final full suite | **75/75 PASS, 285.59 s, exit 0**, `closure-verified-ctest.log` |
| Final JUnit accounting | 75 cases, zero failures/errors/skips/disabled; zero not run |
| Final staged rendering/Studio | PASS, 89.24 s |
| Final window interaction | PASS, 64.00 s; four native edges, free ring/arc, exact actions and guards |
| Final combined folder/content | PASS, 64.00 s; all five folder layouts on both hosts, three segments, content controls and source lifecycle |
| Final QML diagnostics | Zero `Binding loop detected` messages and zero runtime QML-error gate failures in the complete detailed log |

The final combined scenario measured **100 updates -> 2 global content
revisions**, compared with one or two in focused runs and two at the original
checkpoint. The global counter also includes status-provider updates; the
100 ms coalescing implementation and existing update-frequency bounds were
unchanged and all performance assertions passed. This remains within the
previously verified range, with 98% fewer revisions than source updates.
Native concealment produced exactly one terminal report in **1.561 s** in the
final suite. Hidden native overlays deferred while the free host updated,
sampling stopped with no visible status consumer, and reveal/disconnect/
temporary-status expiry behaved correctly. Native widget IDs stayed unchanged.

Exact final commands (the task-owned run root was removed after recording results):

```bash
cmake --build /tmp/archdock-task0039.IZhTqK/build --parallel 1
env QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qmltestrunner -input tests/tst_PanelScene.qml -import /tmp/archdock-task0039.IZhTqK/build/qml-imports PanelScene::test_contentRefreshPreservesHoveredSegment
env DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0039.IZhTqK/no-parent-bus ctest --test-dir /tmp/archdock-task0039.IZhTqK/build --parallel 1 --output-on-failure -R '^(panel-scene|panel-presentation|panel-presentation-guards|panel-guard-interaction)-test$'
env ARCHDOCK_VISIBILITY_DISCRIMINATOR=1 DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0039.IZhTqK/no-parent-bus ctest --test-dir /tmp/archdock-task0039.IZhTqK/build --parallel 1 --output-on-failure -R '^folder-interaction-smoke$'
env DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0039.IZhTqK/no-parent-bus ctest --test-dir /tmp/archdock-task0039.IZhTqK/build --parallel 1 --output-on-failure -R '^(window|folder)-interaction-smoke$'
env DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/archdock-task0039.IZhTqK/no-parent-bus ctest --test-dir /tmp/archdock-task0039.IZhTqK/build --parallel 1 --output-on-failure --output-junit /tmp/archdock-task0039.IZhTqK/closure-verified-junit.xml
git diff --check
```

### Consolidated acceptance

| Inherited criterion | Final proof |
| --- | --- |
| A: Badges/progress render in the correct layer/state | PASS: overlay/source and shared-QML tests; native/free badge, progress, attention and temporary status in live integration |
| A: Stale/disconnected data expires or is unavailable | PASS: overlay model/source checks plus actual native/free sender disconnect and temporary expiry |
| A: System status updates without blocking the UI | PASS: worker-based collection and unavailable-data tests; live CPU/memory readings and demand suspension |
| A: No duplicate native applets without opt-in | PASS: status uses explicitly selected shared segments; exact native widget IDs remain unchanged |
| B: No advertised content placeholder page remains | PASS: persisted content controls and source/capability-driven absence verified in Studio/backend/shared UI |
| B: Visible controls persist, apply, render and are tested | PASS: model/schema/transaction/editor/QML checks plus native/free switches, folders and three segments |
| B: Hidden/concealed content throttles appropriately | PASS: actual KWin hidden state, correct host phase, one report, hidden-overlay deferral, status pause and reveal |
| B: Full suite passes | PASS: all 75 executed, no failures/skips/not-run tests |

Phase A and Phase B verification are complete; no mandatory failure remains.
Current continuation changes are limited to `PanelSegment.qml`,
`tests/tst_PanelScene.qml`, `tests/visibility-window.py`, and closure documentation
in this file, `shared-renderer.md`, `RELEASE_CHECKLIST.md` and the retired handoff.
Earlier overlay/status/content/native-observation implementation remains in the
owner's commits. No successor, dependency, external network, global installation,
personal-desktop mutation or delegated/background/parallel agent was used.

Final cleanup PASS: all **28** private runtime roots named in the task logs are
absent, and no process retains those paths or the task root in its environment.
Disposable instrumentation probes removed their own roots. After the evidence
was recorded, the task-owned `/tmp/archdock-task0039.IZhTqK` build/log/probe root
was removed and its absence verified. No `/tmp/archdock-rendering-import.*`
directory remains. Temporary paths/log names above now identify historical runs,
not retained artifacts. Documentation closure and `git diff --check` pass; all
continuation changes remain unstaged and HEAD is unchanged.

The environment is Arch Linux, Qt 6.11.2, KWin/Plasma 6.7.5 and KF6 6.30.
Private virtual Wayland evidence does not claim physical GPU, monitor/hotplug,
personal-desktop or release acceptance. General release boxes remain unchecked.
The suggested owner-controlled commit remains `Complete Arch Dock content systems`;
it contains TASK-0039 closure only, with no Git writes performed by the agent.

## TASK-0040 implementation — 2026-10-01

**COMPLETE.** The work was implemented and verified on baseline `3ab470c` with
nothing staged. On 2026-10-01 the owner instructed "commit and sync now"; the
agent then made one commit, `Add Arch Dock built-in preset catalogs`, on `main`
and pushed it to `origin/main`. One primary session did the work; no
background, delegated or parallel agent was used.

### Delivered

- **Models** (`src/model/`): `PresetIdentity`, `IconPresetDefinition`,
  `PanelPresetDefinition`. Strict parsers: unknown fields and any value the
  settings schema would change are errors with a JSON pointer.
- **Catalogs and store** (`src/presets/`): `PresetCatalog` (two immutable,
  all-or-nothing installed catalogs), `UserPresetStore` (versioned, user ids
  only, `QSaveFile`), `PresetCapabilityResolver`, `PresetLibrary`.
- **Lineage on the active panel** was already present as `PanelPresetOrigin`
  and is unchanged.
- **Data**: `data/presets/panels/` and `data/presets/icons/`, each an index plus
  15 definitions, installed to `share/arch-dock/presets/`.
- **Studio**: six pages. Panels gains Panel Themes / Skins, Built-in Panel
  Presets and My Panel Presets; Icons gains Icon Styles (real cards), Built-in
  Icon Presets and My Icon Presets. Existing tab indices are unchanged.
  `PresetBrowser.qml` and `PresetCard.qml` draw every card through
  `LivePanelPreview`.
- **Backend surface**: `PanelWindow` gained one member and one context
  property, `presetLibrary`. No slot was added, so the D-Bus surface is
  unchanged.
- **Docs**: [PRESET_PACKAGE.md](PRESET_PACKAGE.md) and a preset-card section in
  [shared-renderer.md](shared-renderer.md).

### Approved adaptations

- Five icon presets are a shipped style plus declared overrides: `glass-tile`,
  `blue-pedestal`, `red-pedestal`, `holographic-tile`, `beveled-sci-fi`.
- `holographic-semicircle` is a procedural 2D free semicircle with no theme. No
  built-in preset uses `true3d`.
- `octagonal-platform` and `orange-arc-dock` fall back to their theme's own
  procedural tier, then the procedural surface. `builtin-themes.json` is
  unchanged; its `fallbackThemeId: holographic-ring` for those two themes cannot
  draw their layouts.
- Studio QML stays in `qml/runtime/`. Card actions are select, Duplicate, and
  for user presets Rename and Delete. There is no Apply, Preview on Desktop or
  Set as Default; those belong to TASK-0041.

### Deviations from the approved plan

- `tests/PresetTestSupport.h` was added to share fixtures between the three
  preset test programs.
- `tests/PanelWindowCapabilityTest.cpp` gained one slot,
  `studioPresetPagesBrowseWithoutChangingAnyPanel`, and its target gained the
  animation-profile catalog path. This is the only test that drives the real
  Studio popup's new pages.
- `PresetCatalog.cpp` was edited again in the resolver step to call the
  resolver.
- New source and test files number 22, as planned, but the set differs by the
  support header.

### Verification

| Gate | Result |
|---|---|
| Baseline at `3ab470c` | 74/75, then `rendering-import-smoke` passed on its single permitted re-run (the documented environmental flake) |
| Phase A gate, incremental build | **80/80**, 317.25 s |
| Consolidated gate, fresh configure and build | 0 compiler warnings; **80/80**, 321.17 s |
| `git diff --check` | clean |

New tests: `preset-definition-test` (89 passed), `preset-catalog-test` (73),
`preset-library-test` (12), `preset-browser-test` (14),
`preset-staged-preview-smoke`. Extended: `studio-navigation-test` (10),
`settings-editor-model-test` (21), `studio-preview-contract-test`,
`panel-window-capability-test`.

Three failures occurred during implementation. Each was diagnosed before one
correction and one re-run:

1. A browser test clicked a button in the frame it became visible, before the
   layout pass. The test now waits for that frame.
2. Preset records reached `LivePanelPreview` with Qt sequences, which its copy
   turns into keyed objects. `presetRendererCandidate()` now copies with
   `copyValue`, as `rendererCandidate()` does for its nested data.
3. The capability test target had no animation-profile catalog, so the preset
   library correctly reported its resources unavailable there.

### Acceptance

| Criterion | Result | Proof |
|---|---|---|
| Exactly 15 + 15 valid built-ins installed | PASS | `preset-catalog-test` exact id and name lists; staged smoke counts 15 + 15 |
| Catalogs and resource types separate | PASS | Two catalog classes, formats and directories; cross-type files rejected |
| Every card renders real shared-renderer output | PASS | `preset-library-test`: 30 real cards, non-empty, repeatable, at the reported tier, no QML warning |
| Built-ins cannot be overwritten; derivative gets a new id and round-trips | PASS | Store and library tests; installed tree digest unchanged |
| Missing capability selects the fallback or marks incompatible, no fake Apply | PASS | Resolver and library tests; contract test forbids an Apply action |
| Selecting cards mutates no panel | PASS | Registry revision, panel records and settings unchanged, in the library test and in the real Studio popup |

### Limitations and observations

- A user preset's fields cannot be edited in Studio yet. The store API
  supports it; the editing UI arrives with TASK-0041's draft.
- Free-layout presets are small in a card because the whole scene is scaled to
  fit. Selecting one shows it in the larger Studio preview.
- **Pre-existing defect, not changed:** the Studio's own panel preview and its
  theme cards log `PanelScene.qml:196 TypeError` for a procedural linear panel
  whose segments have not been edited, because `rendererCandidate()` passes the
  `segments` list as a Qt sequence. Details are in
  [shared-renderer.md](shared-renderer.md).
- During the consolidated gate a fresh build was started without a job limit
  and the owner's machine crashed. The gate was repeated one step at a time
  with four build jobs. No repository file was lost.
- Private virtual Wayland results do not establish physical GPU,
  monitor/hotplug, personal-desktop or release acceptance. Release boxes stay
  unchecked.

Cleanup PASS: the task build directory and every temporary staged prefix were
removed, and no `/tmp/archdock-*` root or task process remains. The commit is
`Add Arch Dock built-in preset catalogs`; the next task records its hash.

## TASK-0041 implementation — 2026-10-02

**COMPLETE in the working tree. Phase A and fresh consolidated gate: PASS.**
The owner approved the exact TASK-0041 plan, subsequently required completion
of all remaining gates with workarounds where needed, paused work after the
2026-10-01 checkpoint, and explicitly resumed it on 2026-10-02. No replanning
or new scope replaced that approval.

TASK-0040 predecessor closure is `a7552054d79e80795282746d38da049ebe158d6b`.
The initial TASK-0041 implementation is `2905761e3dd7fe935d91187e51b1d276e78ece9e`;
the committed/synced repair checkpoint and this continuation's entry HEAD are
`a3a522b46cca63e81176611d563f06047acdc73b`. Entry was clean on `main`.
The active consolidated task pack's `SHA256SUMS.txt` passed verification.
Earlier TASK-0040 statements that desktop actions/editing were future work
describe its historical boundary and are superseded by this section.

### Implemented behavior and file order

The approved file placements were adapted to the existing shared models,
transactions, renderer bridge, Studio and disposable Plasma harness:

1. `PresetApplication.*`, `PanelRegistry.*`: shared pure preparation, independent
   icon isolation, full normalized custom snapshots, lineage and atomic preset
   commit/adoption through the existing transaction/revision boundary.
2. `PresetDefaultStore.*`, `PresetPreviewRecovery.*`: independent future-creation
   defaults and a bounded write-ahead recovery journal, with atomic writes,
   strict validation and retained unsafe/unrecoverable records.
3. `PresetPreviewSession.*`: one active session, all seven normative states,
   streamed ephemeral drafts, explicit apply/custom/default actions, exact
   rollback, temporary-host conversion and recoverable `BLOCKED` failures.
4. `PanelWindow.*`: ownership-checked native/free preparation, snapshots,
   renderer overrides, host restoration, one-revision commit, verified token
   conversion/adoption, interruption recovery and defaults on new creation.
   `/PresetAudition` is a separate interface; `/Control` remains unchanged.
5. `PresetCard.qml`, `PresetBrowser.qml`, `PresetAuditionBar.qml`,
   `SettingsPopup.qml`: explicit desktop actions, active draft customization,
   status/errors, default selection, keyboard/accessibility and interaction
   guards. Hover/selection continue to affect only embedded previews.
6. The dock applet's `main.xml`/`main.qml`: a bounded, ownership-checked command
   restoring only its own free desktop container, with independent backend
   readback and command clearing.
7. Unit/QML/Studio contracts, CMake and `run-preset-audition-matrix.sh` plus the
   shared lifecycle harness: five isolated runtime groups, real staged renderer
   loading, exact host rollback, custom reuse, crash cleanup and unrelated
   native/free fixture preservation.
8. This continuation repairs one proved readiness race in
   `run-plasma-lifecycle.sh` and updates the preset/lifecycle/state/release
   documentation. It reuses the checkpoint's production implementation.

### Native constraints and corrections

Plasma 6.7's native scripting geometry setter is a no-op, as confirmed against
the [KDE source](https://github.com/KDE/plasma-workspace/blob/Plasma/6.7/shell/scripting/widget.cpp#L158).
Free rollback therefore restores the owned applet's actual layout container
after refreshing the original renderer and verifies the exact native readback.
Failed restoration retains the recovery journal. Free scene bounds follow
their layout; native width/height placement hints do not resize a free host.

The checkpoint already corrected deferred applet removal/readback, structured
native renderer replies, immutable inactive preset values, renderer-tier
inference for saved custom panels, stable screen identity for new native
defaults and live theme candidates for temporary drafts. Browser assertions
check action sets independently of QML child traversal order and verify that
incompatible Apply/Preview buttons remain disabled.

Two failures occurred during this resumed phase gate; both stopped later
batches and were diagnosed before correction:

- `window-interaction-smoke` failed in its preflight because the isolated
  PySide6 environment hid the installed system `gi` binding. The system GTK 4,
  PyGObject and libei probe passed. Enabling standard system-site access in
  that disposable venv fixed the combined dependency probe; the unchanged
  runtime test then passed in 63.73 s. No global package was installed.
- The defaults matrix failed after its deliberate service crash because
  `gdbus wait` returned before a subsequent PID lookup saw an owner. Private
  applet activation can race broker disconnection. The harness now pins a
  unique name, resolves and confirms its live PID/owner inside the original
  20-second startup bound. Only observed disappearance/change races retry;
  permanent errors and malformed replies fail. A deterministic fixture
  reproduced the old failure, passed transient and owner-switch cases with
  the correction, and retained immediate permanent/malformed-error failures.
  Defaults passed in 38.06 s; all four other affected matrix groups were
  rerun and passed. No ownership, geometry, revision or orphan assertion was
  weakened, and no timeout was increased.

### Verification

Actual platform: Arch Linux kernel `7.2.7-arch1-1`, Plasma/KWin `6.7.5-1`,
Qt base `6.11.2-3`, declarative `6.11.2-2`, Quick3D `6.11.2-1`,
KF6 KConfig/KCoreAddons `6.30.0-1`, Python `3.14.7`, PySide6 `6.11.2`,
GTK `4.22.5`, PyGObject `3.56.3` and libei `1.6.0`.
Fresh configuration uses the repository default `ARCHDOCK_ENABLE_QUICK3D=AUTO`,
which discovers Quick3D on this machine. No optional renderer requirement is
made mandatory in production.

The fresh phase build passed nine separate target groups at **two jobs**.
CTest ran at **one job**, in these ranges, with heavy/private checks alone:
`1–10`, `11–20`, `21–30`, `31`, `32–40`, `41–50`, `51–60`, `61–64`, `65`,
`66`, `67`, `68–72`, `73`, `74–75`, `76`, `77`, `78`, `79`, `80`, `81–87`.
JUnit coverage confirms all 87 available names executed, with no outstanding
failures, errors, skips, disabled or not-run tests. The sum of successful batch
elapsed times is **431.70 s**; it excludes the recorded failed attempts and
superseded matrix runs, rather than claiming one uninterrupted CTest command.

```bash
cmake -S . -B build-codex-task-0041
cmake --build build-codex-task-0041 --parallel 2 --target <target-group>
env PATH="$PWD/build-codex-task-0041/test-python/bin:$PATH" \
  QT_FORCE_STDERR_LOGGING=1 CMAKE_BUILD_PARALLEL_LEVEL=2 \
  DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/archdock-task0041-parent-bus \
  ctest --test-dir build-codex-task-0041 --parallel 1 --stop-on-failure \
    --output-on-failure --output-junit <batch.xml> -I <first,last>
```

Build groups: application alone; overlay/system/panel/preset definition;
preset catalog/session/library/icon identity; theme/icon/chassis/energy assets;
baked/source/processed assets/settings schema/animation; renderer/capability
and settings/content/icon transactions; registry/folder/window-preview/dock
models; panel-window capability alone; placement/visibility/Plasma adapter/
script result. No gates overlapped and no agents were used.

### Acceptance evidence

| Required criterion | Result | Fresh proof |
| --- | --- | --- |
| Existing owned panel preview and exact Cancel/Revert | PASS | Session unit tests; existing matrix and defaults group's existing-free renderer/geometry rollback |
| Temporary new/free/incompatible host; Cancel leaves no applet/record/token/default change | PASS | Temporary matrix, native/free ownership counts, unchanged registry/defaults and no journal |
| Exactly one revision or exactly one converted managed host on Apply | PASS | Existing/icon revision assertions, temporary token conversion and one-record/one-host audit |
| Reusable custom normalized copy; built-in bytes unchanged | PASS | Session/store tests, real panel and icon custom save/reopen, staged built-in SHA256 checks |
| Defaults affect future creation only | PASS | Unit/Studio tests and combined/independent icon defaults on new native/free instances; earlier records unchanged |
| Icon-only isolation | PASS | Pure preparation rejection tests and complete renderer/registry/host comparison in icons matrix |
| Interruption restores or retains recoverable BLOCKED; never silently commits | PASS | Session/journal tests, temporary/placement/existing-free service crashes, conversion-before-adoption and PlasmaShell restart |
| Unrelated panels/widgets preserved | PASS | Unrelated native and free fixtures checked after every matrix scenario |

Private D-Bus, real KWin Wayland/PlasmaShell and virtual outputs establish the
task's disposable-session acceptance. They do not establish personal-desktop,
physical GPU/monitor, hardware hotplug or release acceptance. The known
pre-existing `PanelScene.qml:196` Qt-sequence warning remains outside this
task, as specified in the approved plan. Profiles/import/export/shortcuts
remain TASK-0042; startup/Arch packaging TASK-0043; platform/performance
hardening TASK-0044; release regression TASK-0045. No later task is started.

### Fresh consolidated closure and cleanup

After the phase gate and documentation updates, all project build output was
removed. A separate fresh configure and all nine two-job target groups passed
(summed build-command elapsed time **560.97 s**). The disposable Python
dependency environment was parked without executing it elsewhere, then
restored to its original absolute path; no compiled project output or CMake
cache was reused. The fresh phase build had passed the same groups in 586.27 s.

The final suite ran the same 20 sequential ranges once, with no failed or
retried consolidated batch. Successful test-command times sum to **427.05 s**.
JUnit independently confirms exactly the 87 configured names, each executed
once with status `run`, and zero failure/error/skip/disabled/not-run result.

| Final check | Result | Batch elapsed time |
| --- | --- | --- |
| Full available consolidated CTest suite | **87/87 PASS** | **427.05 s** |
| Rendering/Studio staged runtime | PASS | 83.05 s |
| Native/free window interaction | PASS | 64.00 s |
| Folder/segment interaction | PASS | 63.99 s |
| Staged preset library, 15 + 15 installed definitions | PASS | 16.98 s |
| Browser and audition QML | 2/2 PASS | 1.12 s |
| Existing native audition/custom copy/Apply | PASS | 13.09 s |
| Temporary free/native audition and live theme draft | PASS | 24.73 s |
| Icon-only audition/custom copy/Apply | PASS | 13.96 s |
| Service/conversion/PlasmaShell interruption | PASS | 19.57 s |
| Defaults and exact existing-free Cancel/crash recovery | PASS | 36.03 s |

Cleanup PASS: all **21 recorded private runtime roots** are absent and no
task-owned process remains. `build-codex-task-0041`, its PySide6 environment
and the small `build-codex-task-0041-evidence` archive were removed after
recording results. Pre-existing build directories, including tracked
`build-codex-task-0014`, were preserved. Raw disposable logs are historical
observations now; the commands, versions, counts and failure accounting above
are the retained evidence. Internal document file links and new task anchors
were checked; `git diff --check` passes.

Entry/final HEAD remains `a3a522b46cca63e81176611d563f06047acdc73b` on `main`.
This continuation changes six files: the shared lifecycle harness, followed
by PRESET_PACKAGE.md, plasma-lifecycle.md, CURRENT_STATE.md,
RELEASE_CHECKLIST.md and the historical handoff marker. No staging, commit or
push was performed in this resumed continuation. Suggested closure commit:
`Complete TASK-0041 audition verification and documentation`.
The owner-requested checkpoint commit/push on 2026-10-01 remains intact.
No agents, global installation or personal-desktop mutation were used.

## TASK-0042 — complete profile management and KDE shortcuts — 2026-10-02

**COMPLETE: Phases A, B, C and the separate fresh consolidated gate PASS.**
Entry was clean `main` at `96ea9e070a9805026b01ded008dfbf11c85c0b4a`, with
TASK-0041 closed by current source and its 87/87 verification record. The
consolidated reference pack was inventoried (175 files); all 174 entries in
`SHA256SUMS.txt` passed. Contracts, tied specifications and legacy tasks
0079–0081 governed one approved plan. The owner approved exact implementation
and subsequently requested commit and sync after completion.

This record supersedes earlier statements that profiles and shortcuts are
future work. TASK-0043 packaging/startup, TASK-0044 platform hardening and
TASK-0045 release regression remain outside this task.

### Implemented behavior and reuse

- **A — store/schema:** `ProfileStore.*` stores version-1 named complete panel
  sets, revisions, screen policy, metadata and optional preset lineage.
  Existing panel serialization/migration retains durable settings and strips
  live associations and transient state. Missing references resolve safely on
  an effective copy. Listing/loading never applies a profile. Explicit capture
  supplies the current arrangement rather than inventing a default panel set.
- **B — management/transaction:** `ProfileManager.*`, `ProfileApplyTransaction.*`
  and full-set `PanelRegistry` operations provide CRUD, data-only import/export,
  one checked registry publication, a write-ahead owned-host backup, rollback
  and explicit interrupted recovery. Import generates new local identities.
  Assets reuse `ThemePackage` validation/materialization with managed-path,
  bounded-transfer and strict non-executable SVG checks. Import/export share
  portable path validation, including literal Unicode and spaces.
- `PanelWindow` reuses existing owned native/free capture, creation, geometry,
  readback and cleanup helpers. Native visibility uses the existing Plasma
  adapter without an independent settings write during a transaction.
  Restoration checks exact tokens and full-set revisions. Uncertain ownership,
  concurrent changes or partial restoration retain a precise BLOCKED record.
  A moved free panel receives a verified replacement before old-host removal.
- **C — shortcuts:** `ProfileShortcutManager.*` uses KDE GlobalAccel, stable
  profile-ID QActions and `NoAutoloading`. Exact/prefix conflict checks and
  native readback prevent stealing keys. Explicit opt-in and assignments live
  separately from portable profiles. Activation resolves the latest saved
  revision and uses the same apply transaction. Delete/off/shutdown remove
  owned registrations; failed cleanup exposes actual remaining action IDs and
  keys and allows a retry. Invalid profiles cannot register or activate.
- `ProfilePage.qml` and `SettingsPopup.qml` provide explicit profile and shortcut
  controls, revision-aware requests, conflicts, transaction/recovery status and
  draft/audition guards. No selection or import implicitly applies a profile.
  The legacy `/Control.applyProfile` appearance selector keeps its contract;
  complete arrangements and shortcuts use `/Profiles`, `org.archdock.Profiles`.
- CMake shares the profile backend across application/tests, packages the page
  and links `KF6::GlobalAccel`. New tests reuse the existing disposable Plasma
  launcher and unrelated-object sentinels. No second compositor harness,
  privileged key hook, preset/audition implementation or successor feature
  was introduced. See [PROFILE_PACKAGE.md](PROFILE_PACKAGE.md) and
  [plasma-lifecycle.md](plasma-lifecycle.md#complete-managed-profile-arrangements).

### Gates and fresh evidence

All builds used **one compile job**. CTests ran serially in bounded name batches;
each private runtime check ran alone. No agents or concurrent gates were used.

| Gate | Result | Summed successful test seconds |
| --- | --- | --- |
| Phase A, legacy TASK-0079 | 88/88 PASS; fresh build green | 443.36 |
| Phase B, legacy TASK-0080 | 92/92 PASS; boundary build and final four profile checks green | 444.89 |
| Phase C, legacy TASK-0081 | 94/94 PASS; full build green | 461.12 |
| Separate fresh consolidated build/suite | 94/94 PASS | 478.24 |

The fresh final configure used a separate empty `build-codex-task-0042-final`,
default AUTO Quick3D and the installed Qt module. Seven bounded target groups
(36 test executables plus application/rendering module) passed in **1053.41 s**
of summed build-command time; the subsequent all-target build also passed.
No compiled project output or CMake cache was reused. Only the local test
Python environment was reused. Final JUnit was matched against the configured
94 unique names: every CTest ran, with zero failures/errors/CTest skips. Some
older offscreen QtTest cases deliberately defer private-only behavior to their
separate runtime companions; the CTest count does not assert zero internal
QtTest skips across the entire project.

Final new-backend QtTest totals (including initialization/cleanup): store 16,
transaction 10, manager 7, shortcut adapter 10, all PASS. The profile page has
explicit management, guard and shortcut-control coverage. Real private runtime
observations include:

| Final runtime check | Result | Seconds |
| --- | --- | --- |
| Staged rendering/Studio | PASS | 82.56 |
| Native/free window interaction | PASS | 64.16 |
| Folder/segment interaction | PASS | 64.00 |
| Staged preset resources | PASS | 18.14 |
| Existing/temporary/icon/recovery/default preset scenarios | All five PASS | 116.85 |
| Profile matrix apply | PASS | 22.28 |
| Profile matrix shortcuts | PASS | 15.59 |

Profile P1 imported a native/free arrangement without applying it, then applied
the declared logical set with verified native auto-hide and owned free geometry.
P2 refused a read-only registry destination and restored exact previous native
placement/visibility, free geometry and registry. P3 restarted with a durable
interrupted record: no automatic restoration occurred, and explicit recovery
restored the verified backup. Unrelated panel and desktop-applet sentinels
remained unchanged through every scenario.

Shortcut S1 checked explicit opt-in and actual native key registration. S2
refused another profile's key and an occupied KDE key without changing either
native owner. S3 invoked the real native component action for a renamed stable
ID and waited for its authoritative completed full-set transaction. S4 proved
invalid IDs create no action, disable removes keys, re-enable restores the
saved assignment and deletion removes it. Unit adapters also prove stale
revision/interaction rejection, failed readback, configuration-write rollback
and visible, retryable incomplete native cleanup.

### Inherited criteria

| Phase / inherited criterion | Status | Evidence |
| --- | --- | --- |
| A: round-trip complete panel data | PASS | Store parser/round-trip tests |
| A: safe missing-reference fallback | PASS | Effective reference tests and preparation |
| A: exclude runtime hover/transition state | PASS | Portable serializer and runtime-claim rejection |
| A: load does not apply | PASS | Store/manager tests; private capture/import |
| B: persistent CRUD/import/export controls | PASS | Manager, store and QML controls |
| B: declared managed panel set | PASS | Transaction tests and private P1 |
| B: usable previous state or precise recovery | PASS | Failure/partial-record tests and P2/P3 |
| B: preserve unrelated Plasma objects | PASS | Exact-token guards and all private sentinels |
| C: correct profile transaction on activation | PASS | Manager adapter and native S3 |
| C: visible non-destructive conflicts | PASS | Adapter/UI tests and native S2 |
| C: deletion removes shortcut | PASS | Manager tests and native S4 readback |
| C: invalid profile has no registration | PASS | Adapter/store tests and native S4 action list |

### Corrections, reproducibility and cleanup

Focused failures were resolved before continuing: the Phase A ID parser needed
absolute anchors to reject a trailing newline; Phase B's artwork fixture needed
the existing stable desktop-entry identity and its import/export path needed
strict SVG rejection beyond the reused legacy validator. A Phase B compile
call needed the existing publication boolean. The initial private fixture read
the registry association before readiness; it now waits for both the owned host
and authoritative association. Each proved cause received one focused
correction and a passing rerun. Filename symmetry and native failure reporting
were then checked before the full Phase C and final gates. No unresolved
failure, waived assertion or successor-task workaround remains.

Commands used: `cmake -S . -B build-codex-task-0042-final`; per-group
`cmake --build build-codex-task-0042-final --parallel 1 --target <targets>`;
then `cmake --build build-codex-task-0042-final --parallel 1`. CTests used
`--parallel 1 --stop-on-failure --output-on-failure`, exact name-regex batches
and one JUnit file per batch. The task-local dependency setup was
`uv venv --system-site-packages --python /usr/bin/python build-codex-task-0042/test-python`
and bounded `uv pip install --no-cache --python build-codex-task-0042/test-python/bin/python PySide6==6.11.2`.
No package was installed globally.

Equivalent bounded full-suite reproduction for the current 94-test ordering:

```bash
export PATH="$PWD/build-codex-task-0042/test-python/bin:$PATH"
export QT_FORCE_STDERR_LOGGING=1 CMAKE_BUILD_PARALLEL_LEVEL=1
export DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/archdock-task0042-parent-bus
for range in 1,8 10,11 12,21 22,31 9,9 32,34 35,35 36,45 46,55 56,65 \
             66,68 69,69 70,70 71,71 72,76 77,77 78,80 83,83 84,84 \
             85,85 86,86 87,87 88,94 81,81 82,82; do
    ctest --test-dir build-codex-task-0042-final --parallel 1 \
        --stop-on-failure --output-on-failure -I "$range" || break
done
```

Platform: Qt base 6.11.2-3, declarative 6.11.2-2, Quick3D 6.11.2-1;
KWin/Plasma 6.7.5-1; GlobalAccel 6.30.0-1 and daemon 6.7.5-1. Disposable
D-Bus/KWin Wayland/PlasmaShell and native component activation establish task
acceptance. Physical keyboard/GPU/monitor, hardware hotplug, personal-desktop
and release acceptance are not established. The pre-existing
`PanelScene.qml:196` sequence warning remains outside the approved task.

Cleanup PASS: all 15 recorded private runtime roots are absent and no
task-owned process remains. Both task build directories, local Python environment,
staged installations, raw diagnostics and temporary runners were removed after
recording the results. Pre-existing `build-codex-task-0014` was preserved.
Raw logs are historical observations now; reproducible commands and results
above are the retained evidence. Only the 28 approved task source/test/document
paths changed. Document links and `git diff --check` pass. No personal Plasma
session was restarted or modified. Owner-authorized Git closure uses
`Complete Arch Dock profile management`; this record does not claim a release.

## TASK-0043 — partial startup implementation and runtime blocker — 2026-10-02

Historical stopped run; superseded by the resumed verification below.

**Status: BLOCKED in Phase A.** The owner approved exact implementation of
the consolidated plan. Entry and final HEAD are both
`2816be6d0d0a631829c4249327c73fbc7c473a94`, on `main`; the entry tree was clean.
The changes below remain unstaged. No phase is complete, and Phase B remains
unstarted. No package build, package-manager installation or uninstall has run.

### Partial implementation, in change order

1. The systemd, D-Bus and desktop `.in` templates in `data/` define one bus
   identity and generated executable paths. Direct session D-Bus activation
   is the planned authority; the optional manual systemd unit uses `Type=dbus`
   and the same `BusName`, with no automatic enablement.
2. `cmake/InstallStartupMetadata.cmake.in` generates metadata at install time
   using the effective prefix, preserving logical paths under `DESTDIR`.
   `CMakeLists.txt` installs the executable through `CMAKE_INSTALL_BINDIR`
   and installs those generated descriptors. The three obsolete static
   descriptors were removed after switching the install rules.
3. `src/main.cpp` retains the existing atomic bus-name guard and forwarding,
   while reporting connection, registration and forwarding failures with a
   failing exit status rather than treating every failure as an existing owner.
4. `tests/ValidateSessionStartup.cmake` checks prefix-with-spaces and
   `DESTDIR=/...`, logical `/usr` installations, metadata, executable paths
   and install-manifest membership. `tests/run-session-startup-smoke.sh` adds
   focused diagnostics and installed activation checks; the existing
   `tests/run-plasma-lifecycle.sh` supplies private KWin/Plasma lifecycle and
   ownership safeguards through a bounded startup dispatch.
5. This current-state record and `RELEASE_CHECKLIST.md` record the failure
   boundary. Successful-startup installation documentation and CTest
   registration of the new checks remain unfinished.

### Verification and first unresolved boundary

All executable gates ran serially. Compilation used one job in the fresh,
disk-backed `build-codex-task-0043/phase-a` directory; neither the old bundled
build nor a predecessor build supplied verification.

| Check | Result |
| --- | --- |
| Consolidated pack integrity | PASS, 174/174 entries; inspection reused the approved planning evidence |
| Fresh Quick3D-ON configure; application/module/backend target build | PASS; complete all-target build remains NOT EXECUTED |
| Incremental startup-diagnostic C++ build | PASS |
| Prefix-with-spaces installation and `DESTDIR` installation | PASS |
| Desktop and manual systemd-unit validation; manifest membership | PASS |
| Disconnected-bus and missing-installed-executable diagnostics | PASS after one proved correction of the new diagnostic fixture |
| Private installed startup smoke | FAIL; its one permitted corrected rerun reached the pre-existing QML error below |
| Full available CTest suite | NOT EXECUTED; stopped at the mandatory runtime failure |
| Phase B and consolidated completion gates | NOT EXECUTED |

The first startup invocation expected the explicit request to activate a new
owner. Its private D-Bus log instead proved that Plasma's native watcher had
already activated that installed owner. One focused fixture correction now
accepts either initial activation or reuse, checks `/proc/<pid>/exe` against
the installed binary, and still requires exactly one successful activation.
The single rerun passed those assertions, repeated manual launch and
`--settings` forwarding, identical owner/panel identities, and unrelated
native/free sentinel comparisons. It then failed its runtime-error assertion:

```text
PanelScene.qml:196: TypeError: Property 'some' of object [object Object] is not a function
Installed startup produced a runtime resource error.
```

`segmentDefinitions.some(...)` is the failing expression. The renderer file
is byte-identical to entry HEAD (SHA256
`c27276eadf80e839ed2db3c952d4dba02ec2fc11bc1ba8322f2aad78d12c7100`), and
the preceding TASK-0042 record already documents this sequence warning.
This is a pre-existing renderer failure, not a proved regression introduced
by the startup changes. Execution-contract section 3 therefore requires
stopping; neither patching the renderer nor suppressing the assertion is
authorized by the exact TASK-0043 plan. A separately scoped predecessor
repair is the next boundary before resuming Phase A. No further runtime
attempt or later implementation followed this failure.

Exact failed rerun, from the repository root (exit status 1):

```bash
env PATH="$PWD/build-codex-task-0043/test-python/bin:$PATH" \
    ARCHDOCK_BUILD_DIR="$PWD/build-codex-task-0043/phase-a" \
    ARCHDOCK_QML_INSTALL_DIR=lib/qt6/qml \
    TMPDIR="$PWD/build-codex-task-0043/tmp" \
    DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/archdock-task0043-parent-bus \
    QT_FORCE_STDERR_LOGGING=1 \
    bash tests/run-session-startup-smoke.sh runtime \
    >build-codex-task-0043/evidence/phase-a-startup-runtime-rerun.log 2>&1
```

Other executed gate commands:

```bash
cmake -S . -B build-codex-task-0043/phase-a \
    -DCMAKE_INSTALL_PREFIX=/usr -DARCHDOCK_ENABLE_QUICK3D=ON
cmake --build build-codex-task-0043/phase-a --parallel 1 --target arch-dock
cmake -DBUILD_DIR="$PWD/build-codex-task-0043/phase-a" \
    -DINSTALL_BINDIR=bin -DINSTALL_LIBDIR=lib \
    -P tests/ValidateSessionStartup.cmake
env ARCHDOCK_BUILD_DIR="$PWD/build-codex-task-0043/phase-a" \
    TMPDIR="$PWD/build-codex-task-0043/tmp" \
    bash tests/run-session-startup-smoke.sh diagnostics
bash -n tests/run-session-startup-smoke.sh tests/run-plasma-lifecycle.sh
git diff --check
```

### Inherited acceptance and retained evidence

| Phase / criterion | Status | Evidence or missing gate |
| --- | --- | --- |
| A: executable matches startup metadata | PASS | Both install modes; installed runtime PID executable comparison |
| A: one activation starts one owner | PASS | Exactly one successful native activation; repeated launch/settings preserve PID and panels |
| A: missing dependency/path diagnostics | PASS | Missing installed executable identifies service/failure; generated descriptor identifies path; disconnected bus fails explicitly |
| A: no source-tree path required | NOT EXECUTED | Metadata paths pass; complete source-independent installed rendering remains unproved |
| B: clean-source package build | NOT EXECUTED | Phase B not started |
| B: installed package starts and renders | NOT EXECUTED | Phase B not started |
| B: uninstall removes files and preserves user configuration | NOT EXECUTED | Phase B not started |
| B: optional 3D remains optional | NOT EXECUTED | Package declarations and verification not implemented |
| B: installed exact 15+15 catalogs and valid references/fallbacks | NOT EXECUTED | Package file-list/catalog audit not executed |

The copied private logs are retained in the ignored
`build-codex-task-0043/phase-a/session-startup-smoke/`, including the QML
failure in `session.log` at lines 86–87. The complete rerun transcript is
`build-codex-task-0043/evidence/phase-a-startup-runtime-rerun.log`.
The ignored build, local Python dependency environment and evidence remain
available for reuse after the blocker is resolved. They are not a completed
cleanup gate. Disposable installed prefixes, metadata probes and private
session temporary directories have been removed; no task-owned private
process remains. No global package installation or personal Plasma change
was performed. No agent was spawned, no concurrent gate ran, and nothing
was staged, committed or pushed. TASK-0044 and TASK-0045 remain unstarted;
this renderer failure is not silently deferred to either task.

## TASK-0043 — resumed startup and Arch packaging — 2026-10-02

The owner authorized resolving the blockers and finding workarounds before
finishing TASK-0043. This permits the bounded prerequisite repair without
replacing the existing consolidated plan or reopening unrelated features.
Baseline and current HEAD remain `2816be6d0d0a631829c4249327c73fbc7c473a94`.

**Phase A COMPLETE.** A native Qt probe and the real private D-Bus reply
confirmed that Qt sequences retain their array methods. The failure was
introduced by `LivePanelPreview.copied()`: its `Array.isArray()` test did not
recognize a native sequence, and its object-copy branch discarded length and
array semantics. A new native-sequence case in `tst_LivePanelPreview.qml`
reproduced the exact `PanelScene.qml:196` error before repair. The preview
copier now reuses the native-sequence recognition condition already present
in `SettingsEditorModel.js`. The renderer expression is unchanged. All seven
preview cases pass, including copy isolation. Qt documents this distinction
in [its sequence conversion reference](https://doc.qt.io/qt-6/qtqml-cppintegration-data.html#qvariantlist-and-qvariantmap-to-javascript-array-like-and-object).

The startup runtime additionally masks the entire source checkout in a
private mount namespace, retaining only the installed prefix and copied test
helpers. The executable is absent from PATH; native D-Bus activation chooses
the installed owner, and the backend reports installed icon-style resources.
Repeated launch/settings forwarding preserves owner and panel identity;
unrelated native/free sentinels are unchanged. No runtime TypeError remains.
The namespace's initial read-only `/tmp` setup error was proved and corrected
with a private writable temporary mount, after which the check passed.

`docs/INSTALL.md` documents direct D-Bus authority, the manual unit and prefix
semantics. Three serial CTests now cover metadata, diagnostics and private
startup. All ten bounded one-job build groups and the all-target build passed.
The complete available suite ran in 27 serial batches: **97/97 PASS**, zero
failures/errors/CTest skips, **501.15 s** summed test time. No predecessor build
was reused. The existing task build supplied fresh verification and remains
available for Phase B's required gate.

All four inherited Phase A acceptance criteria are **PASS**: executable/path
agreement, one owner, explicit missing-path/bus diagnostics and installed
runtime operation with the source checkout hidden. Phase B remains unstarted
at this record's initial write; its package and install/uninstall gates follow
sequentially. No new project-wide license has been selected.

## TASK-0043 — Phase B package verification and final cleanup boundary — 2026-10-02

Both internal phases and all nine inherited acceptance criteria are **PASS**.
The consolidated cleanup gate remains pending only for the system-owned crash
dump identified below. Baseline/final HEAD is unchanged at
`2816be6d0d0a631829c4249327c73fbc7c473a94`, branch `main`; all changes remain
unstaged. No TASK-0044/0045 implementation, agent, concurrent gate, personal
Plasma mutation or global package installation was performed.

### Phase B behavior and file order

1. `PKGBUILD` defines the native Arch route, `/usr` destinations, required Qt/KDE
   dependencies and optional Qt Quick 3D/KPipeWire runtime dependencies. Quick
   3D is required at build time to include its optional renderer, while the
   executable has no Quick 3D linkage. Compilation/autogen use one job; LTO
   and debug splitting are disabled.
2. `packaging/LICENSING.md` preserves the existing component/asset declarations
   and records the unspecified project-wide license. It assigns no new license.
3. `tools/prepare-arch-source.py` exports the current tracked and unignored
   source, with per-file bytes/modes and baseline HEAD recorded in
   `SOURCE_CHECKPOINT.json`. Normalized archive timestamps/ownership make the
   archive stable for that checkpoint. The outer recipe and operational
   current-state/release records are excluded to avoid circular hashes.
4. `tests/PresetLibraryTest.cpp` accepts a copied QML fixture directory. Its
   existing catalog/reference and real-pixel tests are reused against the
   installed shared renderer; production resource lookup is unchanged.
5. `tests/run-plasma-lifecycle.sh` and `run-session-startup-smoke.sh` extend the
   existing startup mode to a pacman-owned `/usr` tree, read-only native
   overlays, executable-byte identity and optional Quick 3D masking. Ordinary
   lifecycle modes remain covered by the full CTest suite.
6. `tests/run-arch-package-smoke.sh` uses native pacman dependency checks in a
   disposable user namespace/root, audits all installed file bytes/modes and
   CMake manifest coverage, reuses the preset and private Wayland harnesses,
   then removes the package and checks user configuration bytes.
7. `docs/INSTALL.md` documents startup authority, prefix/DESTDIR semantics,
   source export/makepkg, owner-controlled installation/removal, dependencies
   and private verification. `PKGBUILD` is pinned to the final source digest.
   This record and `RELEASE_CHECKLIST.md` report the final evidence separately
   from historical blocked records and later production/release acceptance.

### Native verification and proved workarounds

The fresh one-job all-target build passed. The full pre-package suite ran in
the same 27 bounded serial batches: **97/97 PASS**, zero failures/errors/CTest
skips, **498.64 s** summed test time. Phase A's full gate was **97/97 PASS**,
501.15 s. Internal QtTest deferrals to private-session fixtures are distinct
from CTest skips. No passing broad gate was repeated without a phase requirement
or changed executable/test inputs.

Native user/mount namespaces and Bubblewrap read-only overlays were available.
The clean source archive contained **508 source files** plus its internal
checkpoint manifest; every source byte/mode and archive exclusion was audited.
Native `makepkg --verifysource` and `makepkg --cleanbuild --noconfirm` passed,
producing `arch-dock-0.1.0-1-x86_64.pkg.tar.zst`.

The initial disposable install passed dependency resolution and the complete
209-file audit. Native systemd verification then needed a writable temporary
directory inside the read-only namespace. A focused native probe passed with
private `/tmp`; the package test wrapper now supplies that. Installed catalog
and real-pixel checks subsequently passed 8/8, but KWin rejected the nested
runtime socket path because `sockaddr_un.sun_path` is 108 bytes. Reusing the
Phase A disk-backed temporary base shortened the socket path to **94 bytes**;
both subsequent private KWin startup runs passed. These are test-environment
workarounds; no KDE API, native ownership check or runtime assertion was waived.

Only `tests/run-arch-package-smoke.sh` changed between the first clean source
archive and the final archive. Every production and CTest input remained
byte-identical. Native makepkg refreshed its extracted source and reran the
one-job CMake build/package stages, reusing verified unchanged compilation.
All **209 final runtime payload hashes equal the first clean-source package**.
The final checkpoint and its pinned recipe were independently verified again.

The final native install/render/uninstall gate passed:

- Pacman installed into its own root, using copied native dependency records
  and dependency checks enabled. No `--nodeps` or global package operation.
- **209/209** package files match CMake coverage plus the two documented
  license/install notices; all installed bytes and modes match the package.
  Coverage includes executable/startup metadata, both applets, five layout
  templates, KWin watcher, shared QML, themes, icon styles and animation data.
- Installed catalogs contain exactly **15 Panel Presets + 15 Icon Presets**.
  Existing tests validate every reference and compatibility/fallback record;
  all 30 cards render deterministic, non-flat pixels through installed QML.
  The ordinary installed checks passed **8/8 QtTests**; catalog/pixel checks
  with the native Qt Quick 3D QML module hidden passed **4/4 QtTests**.
- Both private virtual KWin/Plasma startup runs passed, including missing
  Quick 3D. The original checkout and clean-export source were hidden. Native
  D-Bus activation selected the package's executable bytes, one owner and one
  successful activation; repeated launch/settings forwarding preserved the
  owner, panel IDs and unrelated native/free sentinels. No resource/TypeError
  assertion was weakened. Desktop/systemd metadata and dynamic linkage pass.
- Native `pacman -R arch-dock` removed **all 209 files** and its database
  record. Plasma and Arch Dock user configuration sentinel hashes remained
  identical. No cache rebuild, restart or automatic service enablement was
  needed in the verified flow.

| Inherited acceptance criterion | Result |
| --- | --- |
| A: staged/installed executable agrees with startup metadata | PASS |
| A: one activation path starts one service owner | PASS |
| A: missing path/dependency or bus failures produce diagnostics | PASS |
| A: installed runtime needs no source-tree path | PASS |
| B: package builds from a clean source checkpoint | PASS |
| B: installed package starts and renders through the documented route | PASS |
| B: uninstall removes package files and preserves user configuration | PASS |
| B: optional 3D remains optional | PASS |
| B: exactly 15+15 valid built-ins and resolved/declared-fallback references | PASS |

Commands used include the fresh `cmake --build ... --parallel 1`, serial
`ctest --parallel 1 --stop-on-failure --output-on-failure --no-tests=error -I
first,last` batches, `python tools/prepare-arch-source.py output`, native
`makepkg --verifysource`, the initial clean build and subsequent bounded
`makepkg --force --config ../makepkg-task.conf --noconfirm` refreshes.
The complete installed test invocation is documented in `INSTALL.md`, using
`ARCHDOCK_BUILD_DIR`, `ARCHDOCK_PACKAGE_INSTALL_MANIFEST`,
`ARCHDOCK_PACKAGE_EVIDENCE_DIR`, disk-backed `TMPDIR`, and
`bash tests/run-arch-package-smoke.sh package-file`.

### Deliverables and final cleanup boundary

Only six deliberate deliverables remain under
`build-codex-task-0043/package-output/`: the package, source archive, pinned
PKGBUILD, `SOURCE_CHECKPOINT.json`, `SHA256SUMS` and `VERIFICATION.json`.
The verification receipt retains acceptance counts and all 209 payload hashes;
raw diagnostics are removed. Source SHA256 is
`566a0ad7fbaa489764f5299dd88d215b6a572d0352c696ceae35b236867b589c`;
package SHA256 is
`29a9038ce40a0c8b581af61cf0a2acfa49bb61e16b9c8e49c14e24d1e69dcfad`.

Task builds, local Python dependency environment, extracted build/source,
temporary pacman installations, namespace probes and raw logs were removed.
The `/proc` environment audit found **zero** remaining task runtime processes;
pre-existing builds and unrelated panels/files were preserved. One diagnostic
artifact remains outside the workspace: the system-owned **3.9 MB** KWin core
from the proved socket-path failure:

`/var/lib/systemd/coredump/core.kwin_wayland.1000.dd07aa413ed141a3960ee681d1112c26.1368188.1790966006000000.zst`

An exact-file cleanup using `sudo -n rm -- path` was refused because sudo
requires the owner's password. The agent cannot supply that authentication.
TASK-0043's consolidated completion checkbox explicitly requires no diagnostic
artifact to remain; final closure is pending the owner's removal of this exact
file and subsequent read-only verification. No other core dump should be removed.

This evidence uses native virtual KWin/Wayland with the installed host Arch/KDE
dependency versions. Physical GPU/monitor/hotplug and broader platform evidence
belong to TASK-0044; release/tag acceptance belongs to TASK-0045. Existing
license declarations remain unchanged and project-wide licensing is unspecified.
Suggested owner-controlled commit: `Package Arch Dock for Arch Linux`.

## TASK-0044 — Configuration recovery Phase A — 2026-10-02

Baseline HEAD: `a5bdd7933591bf75bf030cd144f7c9cea5e7fbae`. The consolidated
pack and all checksums were inspected; the existing approved plan covers both
phases without reopening predecessor work. TASK-0043's exact pending core at
the path recorded above was removed and its absence verified. The owner has
authorized TASK-0044 commit/sync after verification, followed by task cleanup.

The shared version-1 data-only backup service uses QSaveFile, SHA256, private
permissions, trusted application roots and bounded copies. It snapshots settings,
profiles/assets, user presets/defaults, themes and profile-shortcut data before
destructive registry migration, unmarked preset-store adoption, actual legacy
profile rewrite, or profile apply. Installed built-ins and generated/temporary
or executable content are excluded. Retention is configurable 1–20 (default 5),
preserves the newest valid copy despite clock changes, removes corrupt/incomplete
copies and pins interrupted-restore copies. Offline Core-only capture/list/restore
commands use the existing D-Bus ownership lock and refuse conflicting profile
recovery. Migration stages managed assets/settings and leaves original bytes
unchanged on failure; native-host rollback retains its existing ownership rules.

Fresh focused checks pass: configuration-backup, panel-registry, preset-catalog,
profile-store, profile-apply-transaction, profile-manager, and the disposable
configuration-upgrade test (**7/7** distinct CTests). The fixture initially wrote
a Python object rather than native QByteArray; corrected encoding passed that
boundary. Its next failure was a logging-capture error: Qt sent the expected
live-owner refusal to journald. A focused trace and exact journal entry proved
the cause; forcing stderr inside the disposable fixture fixed capture, and the
single rerun passed. Assertions and live-owner refusal remain intact.

The fresh Phase A configure, eight bounded one-job build groups, final all-target
build and all **99/99 CTests PASS**, with no failed, skipped, disabled or not-run
tests. The suite ran in small serial batches; rendering, window/folder interaction,
staged presets, profile apply/shortcuts, all five audition groups and installed
startup each ran alone. This closes all four Phase A acceptance criteria and
allows the approved Phase B work to begin.

The task-local system-site Python environment supplies pinned PySide6 6.11.2
alongside existing system GI; no global dependency was installed. Builds use
one job and tests one worker, short disk-backed TMPDIR, bounded batches and
individual private runtime gates. Commands and recovery limits are in
[INSTALL.md](INSTALL.md#configuration-recovery-and-upgrades).
