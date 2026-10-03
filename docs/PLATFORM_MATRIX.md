# Arch Dock platform verification

TASK-0044 verification checkpoint, 2026-10-03. Source baseline:
`a5bdd7933591bf75bf030cd144f7c9cea5e7fbae` plus the approved TASK-0044 changes.
This record describes observed private runtime behavior. It does not claim
physical connector, scanout, other GPU, or release acceptance.

Phase A passed its fresh all-target build and **99/99 CTests**. Phase B passed
its all-target build and complete **106/106 CTests** (778.28 seconds summed
individual test times). The owner resumed from synced checkpoint
`c225dc6ae0abbeff21425d517546e67831ebc74b`; the retained fresh final configure,
remaining one-job build groups and all-target build passed. The fresh final
**106/106 CTests passed** (778.26 seconds summed individual times). After a
private-disposal helper change, its all-target build and **14/14 affected runtime
checks passed** (392.85 seconds); the other 92 fresh results are unaffected.
Installed startup with optional Quick 3D unavailable passed before and after
that change. All executable verification is green. TASK-0044 is COMPLETE:
ordinary task cleanup and the owner-authorized authenticated removal of both
recorded root-owned dumps are verified. The passing build/runtime receipts are
reused; cleanup required no executable change or gate rerun. The verified
checkpoint is `96d608a8440ddcb7bb8f886523fc7131b2b53e59`; the owner explicitly
authorized committing and syncing this final closure documentation update.

The installed runtime is Qt 6.11.2, Plasma/KWin 6.7.5 and KF 6.30.0. The
private compositor uses native Wayland and virtual outputs; rendering uses
the host AMD Radeon 610M with Mesa 26.2.4. Every build uses one job; every
CTest invocation uses one worker. Each private session has its own bus,
configuration, installation and short disk-backed runtime root.

## Display, input and resource observations

| Check | Recorded result |
| --- | --- |
| Original 100% failing gate after energy repair | PASS, 44.54 s; all 30 cards, browser selection, core keyboard controls, mask release and audition cleanup |
| 125% original matrix after native readback/browser repairs | PASS, 54.11 s |
| 150%, two separated outputs, presented native surfaces on all four edges | PASS, 46.99 s; all 30 cards, browser, keyboard, mask release and audition cleanup |
| 200%, same presented-surface and UI coverage | PASS, 52.96 s |
| One/two outputs, removal of the actual audition output, re-enable and service restart | PASS, 20.81 s; stable identity retained, temporary host removed, no orphan; connected scale change preserves ACTIVE audition |
| Completed Phase B scale groups | PASS: 100% 44.17 s, 125% 49.03 s, 150% 46.21 s, 200% 50.25 s; preceding rows retain earlier focused receipts |
| Completed Phase B hotplug/restart and resource groups | PASS: 21.65 s and 23.24 s respectively |
| Fresh final scale groups before disposal-helper change | PASS: 100% 47.26 s, 125% 47.29 s, 150% 52.72 s, 200% 55.50 s |
| Affected scale refresh with current disposal helper | PASS: 100% 42.91 s, 125% 44.80 s, 150% 44.52 s, 200% 45.38 s |
| Affected hotplug/restart and resource refresh | PASS: 22.41 s and 21.42 s respectively |
| Eight audition create/cancel cycles | PASS; refreshed final run measured 63,112 to 71,740 KiB RSS, 8.43 MiB growth, below the 64 MiB growth discriminator; zero stale hosts |
| Generated render history | PASS; four unreferenced versions per panel and a shared 64 MiB history budget; live/profile/preset/backup/recovery references remain protected outside that budget |
| Canvas image lifetime and overlay demand | PASS; replaced masks unload, empty sources stop hit testing, idle expiry stops, accepted hidden sources retain their required recovery/expiry behavior |
| Studio, icon editing, core keyboard/focus/accessibility | PASS in focused resource-backed and QML tests; native scale groups deliver keyboard events through Wayland |
| Complete Phase B suite | PASS, 106/106; no missing tests or unresolved failures |
| Fresh consolidated suite | PASS, 106/106; complete numbered coverage, no missing, skipped or failed tests |
| Affected private runtime refresh | PASS, 14/14; profile apply/shortcuts, five audition groups, four scales, hotplug, resources and installed startup |
| Optional-3D-absent installed runtime | PASS before and after disposal-helper change; live single-owner startup and hidden unavailable controls |

The completed phase suites also cover the existing free ring/arc geometry, effect
bounds, Plasma Edit Mode, visibility/fullscreen behavior, profile transactions,
reduced motion, 3D quality/resource limits, and fallback contracts. Current
receipts combine the 92 unchanged fresh results with the 14 affected refresh
results; this records reuse explicitly rather than claiming another full run.

## Energy-frame-cyan: proved construction-state difference

The first unequal pair was captured inside Qt's `QQuickWindow::grabWindow()`;
it was not a KWin compositor screenshot. Both images were 300x110, 33,000
pixels, QImage format 18 (RGBA8888 premultiplied), with image/window/effective
DPR 1. Direct pixel comparison gave:

| Measurement | Value |
| --- | --- |
| Different pixels | 1, or 0.0030303% |
| Bounding box | (251,66) through (251,66), lower energy-frame edge |
| RGBA difference | Red 37 versus 36; green 113, blue 126 and alpha 255 unchanged |
| Maximum channel delta | 1 |
| Mean absolute channel delta | 0.000007575757575757576 |
| Alpha difference | None |
| Translation discriminator | Best at (0,0); zero-offset mean pixel delta approximately 0.00000797 versus approximately 3.5373 for a one-pixel horizontal shift |

The original PNG SHA256 values were:

```text
A 2f7fadba356a77cdd9abd9dcfba7a0223e467835df85a213c87a58f41ac45c35
B 38f3d16f2c58ebe3d002fa951dc7bfa924d06f10d923cb95b996fd79b0f97d8e
```

The concrete input chain is:

1. `data/presets/panels/energy-frame-cyan.json`: seed
   `energy-frame-cyan-v1`, skinned2d/procedural2d fallback, 720x76 horizontal
   example, padding 18, icon size 52, spacing 8, color `#44ddea`, opacity 0.9,
   glow 1.15 and pulse/hover motion parameters.
2. Preset definitions/catalog/library supply the validated compatibility and
   renderer data. `SettingsEditorModel.presetRendererCandidate()` creates the
   still candidate used by `PresetCard` with `motionEnabled: false` and reduced
   motion. No construction timestamp or generated identifier changes its look.
3. `LivePanelPreview` feeds the shared scene, canonical example entries and
   preview state. The two instances had matching definition, renderer tier,
   resolved assets, hover state, item geometry, transforms, clipping, opacity
   and scale. The scene fit scale was approximately 0.405555.
4. The shared surface loader selects
   `qml/ArchDock/Rendering/renderers/PanelSkin2D.qml`. Its asset layers, dynamic
   tint/glow and alpha-mask Canvas remain active. Reduced motion stops the
   overlay animation and captures phase zero. Status/provider inputs matched.
5. Qt uploads images into each window's scene graph. The original fixture
   reused one window across preceding cards, giving successive cards different
   texture-atlas allocation history. Changed fractional sampling coordinates
   explain the one-level delta under Qt's documented atlas design; this
   mechanism is an inference from the controls, not a GPU-coordinate readback.

Three later captures of each original instance were exact and stable. Replaying
the five preceding presets produced **A,B,A,A** for four cyan constructions.
A native Qt control using a 1x1 atlas instead of the normal 1024x512 atlas made
all four exact without changing the GPU, effects or preset data. A fresh native
window per card with the normal atlas also made all four exact. This establishes
**Case B: hidden scene-graph construction state**, rather than unsettled
animation, translation, stale source, or compositor capture timing.

The fixture now reuses its engine/component while creating a fresh window for
each card. It waits for the promised renderer tier and four equal frames and
uses the captured image's DPR for the crop. Exact equality is preserved. The
focused regression warms the five preceding presets, separately instantiates
four cyan and four green cards, checks actual energy-surface readiness and
non-flat output, and requires cyan and green to differ. Three consecutive
focused native Wayland runs passed. No production animation/effect was disabled
and no image tolerance was introduced. Temporary capture/state instrumentation
was removed after diagnosis.

Native references:
[Qt scene-graph atlas behavior](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph-renderer.html),
[Qt 6.11.2 threaded grab and image DPR](https://github.com/qt/qtdeclarative/blob/v6.11.2/src/quick/scenegraph/qsgthreadedrenderloop.cpp).

## Other mandatory-gate repairs

- Plasma custom length is determined by native equal minimum/maximum bounds.
  Its QML binding can replace the raw content-length getter. At 125%, the
  discriminator observed `64|custom|320|320|174` while KWin presented a 320x80
  panel surface (including native padding). The adapter now reads fixed length
  from equal custom bounds and retains checks on every bound, mode and owned
  field. A before/after regression reproduces raw content length 174 and
  positively verifies that unequal bounds still fail and roll back.
  [KDE 6.7.5 PanelView](https://github.com/KDE/plasma-workspace/blob/v6.7.5/shell/panelview.cpp)
  and [native Panel.qml binding](https://github.com/KDE/plasma-desktop/blob/v6.7.5/desktoppackage/contents/views/Panel.qml)
  define this boundary.
- A browser click was sent before its ListView had a presented layout: recorded
  list size 121x0, with a 720-pixel fixture inside a native 484-pixel client.
  The fixture now follows actual client size, waits for `frameSwapped`, and
  checks that each click is inside both the viewport and window. All selection,
  selected-state and no-panel/no-settings-mutation assertions remain enforced.
- KScreen enumeration differed from Qt screen order. Moving only one returned
  output left their normalized rectangles largely overlapping. The matrix now
  resolves names from Qt stable identities, places both outputs explicitly
  apart, verifies that topology, and checks the presented native surfaces with
  the existing KWin probe. Its hotplug target is the actual audition output.
- An active audition survived removal of its requested output. Screen
  reconciliation now reuses the session's existing journaled cancel/rollback
  path when that stable identity disappears. Connected geometry/DPI changes
  preserve the session; failure retains the existing recoverable session state.
- An applet reply callback outlived its root. `reply.finished.connect(root, ...)`
  uses Qt's native QObject receiver lifetime, preserving reply cleanup and
  error handling. The optional `acceptDrops` value is normalized to a boolean.
  The matrix scans real QML errors; none are silenced.

## Private-session disposal and retained evidence

Two native PlasmaShell crash dumps were found during final cleanup inspection.
Their journal CWDs are the TASK-0044 final build and its private runtime root;
their stacks fault in native Mesa worker threads after completed live assertions,
during final SIGTERM disposal. The exact internal Mesa memory-fault mechanism
is not established by those stacks. Final disposal now reuses the stop/reap
helper with SIGKILL only for the private PlasmaShell; normal in-session restarts,
Arch Dock and KWin retain SIGTERM. All live assertions, QML scans and crash
reporting remain active. The focused shortcut regression passed, followed by
the affected 14-test refresh and optional-3D-absent installed runtime; no new
task-owned dump was recorded. See [the lifecycle boundary](plasma-lifecycle.md#task-0044-display-and-resource-hardening)
and native [signal(7)](https://man7.org/linux/man-pages/man7/signal.7.html).

Task builds, staged installations, temporary roots, local Python environment
and raw diagnostic workareas were removed after verification. Process/path
inspection found no task-owned process retaining them. All six TASK-0043 package
deliverables were preserved with matching pre/post-cleanup hashes. Four compact
deliverables remain in `build-codex-task-0044/verification-output`:
`VERIFICATION.json`, `gate-logs.zip`, `energy-evidence.zip` and `SHA256SUMS`.
Archive CRC checks and retained file hashes pass. These preserve the original
images/state, native controls, complete phase/final receipts and teardown journal
evidence. The authenticated dump removal and completed cleanup boundary are
recorded in [CURRENT_STATE.md](CURRENT_STATE.md#task-0044-verification-and-cleanup-checkpoint--2026-10-03).

## Physical cells not executed

Only `/sys/class/drm/card1-eDP-1/status` reports a connected physical display.
There is no second physical monitor or alternate GPU test host. The owner's
active Plasma/DRM session must remain untouched. Physical plug/unplug, scanout,
other GPU and owner-desktop interaction cells are therefore **NOT EXECUTED**
under the task contract's proved-environment exception. Private virtual output
removal and native host-GPU rendering are recorded above; they do not substitute
for those physical observations.

For a separately available disposable physical Wayland session, exact native
entry points are:

```sh
kscreen-doctor --outputs
kscreen-doctor output.eDP-1.scale.1
kscreen-doctor output.eDP-1.scale.1.25
kscreen-doctor output.eDP-1.scale.1.5
kscreen-doctor output.eDP-1.scale.2
gdbus call --session --dest org.archdock.ArchDock --object-path /Control \
  --method local.PanelWindow.showSettings
gdbus call --session --dest org.archdock.ArchDock --object-path /Control \
  --method local.PanelWindow.applyNativePanelPlacementDraft bottom \
  "{'edge': <'left'>, 'dynamic': <false>, 'width': <int32 64>, 'height': <int32 320>}"
gdbus call --session --dest org.archdock.ArchDock --object-path /Control \
  --method local.PanelWindow.availableScreens
```

After connecting a second physical output, use its name from `--outputs` with
`kscreen-doctor output.NAME.disable` and `kscreen-doctor output.NAME.enable`;
observe the stable requested identity, fallback, return and audition cleanup.
The top/right/bottom native edge drafts and physical keyboard/Plasma Edit Mode,
maximized/fullscreen and rendered effects require the same recorded observation.
No such commands were run against the owner's desktop.
