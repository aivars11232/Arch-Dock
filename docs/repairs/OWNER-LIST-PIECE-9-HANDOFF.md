# Owner list, piece 9: Arch Dock's own bars - handoff for the next session

Written 2026-10-21 late, at the owner's word: "And now, commit and Sync
current changes, make a report and handoff for tomorrow and stop". Piece 9
gives Arch Dock bars of its own that stand side by side on a screen edge.
Steps 1 to 3 are closed (bar host `8bdd0dd`, hiding `3926402`, Panel
Studio / profiles / presets / login `24029fd`, each rechecked). **Step 4,
Plasma widgets in bars, is in progress**; this commit is a checkpoint, not
the step's close. The report is `OWNER-LIST-PIECE-9.md` ("Step 4 -
checkpoint"). One update for the owner comes after step 6; the owner chose
to wait for it ("Nah, I can wait").

## Where step 4 stands

**Working, seen on a private Plasma desktop:** Plasma's launcher, digital
clock and system tray in a bar beside its dock, in the record's order, moved
and removed through the backend; a widget's own right-click menu with
"Configure <widget>…" (owner ticked); one Arch Dock panel along each bar's
whole length and as deep as the bar, its icons and widgets inside (owner
ticked, then asked the same for a bar holding only its dock, which is done);
the bar host stopping and starting again empties no bar.

**The owner's rulings tonight:** the clock and the rest "have to be inside
the panel"; "the icons also should be inside the panel" (a bar holding only
its dock too); the glass must reach into the round ends (done: the line by
the Wi-Fi symbol is gone); "there's no need for those corners if ends are
rounded" (open, item 1 below).

## Next, in order

1. **The faint square corners outside the panel's round ends.** Seen in
   `evidence/piece9-bars-step4/open-corners-at-the-round-ends.png` (runs
   `bars-step4-widgets-13` to `-15`): a slightly lighter rectangle, the size
   of the surface box, round the capsule. Ruled out: the look's glow
   (`panelGlow` is 0 by default), the band's own halo (Canvas shadow, now off
   when `geometry.bandWidth` is set), a blur asked of KWin (Arch Dock asks for
   none), a Rectangle in `PanelSurfaceLoader.qml`. Next suspects: the
   procedural renderer's other passes in `onPaint` (depth stroke, multiply
   pass, polished edge, `paintPicture`), layers with `layer.enabled` on the
   surface items, the Canvas itself. Fix only the corners; the owner liked
   the rest.
2. **Panel Studio's Widgets page**, shown for bars only (owner rules: no
   empty tabs, hide what cannot apply): the bar's widgets in order with
   names and icons (`barWidgetList`), move along / remove (`moveBarWidget`,
   `removeBarWidget`; the dock can move but not be removed), "Add widget…"
   listing `availableBarWidgets()` with a search. Show it to the owner as
   pictures.
3. **"Enter Edit Mode" in widget menus.** The corona's own "edit mode"
   action is hidden and disabled when a menu opens (`BarCorona::trimMenus`;
   a debug run showed it `visible=false`), yet the item shows: it comes from
   Plasma's context-menu plugin (`org.kde.contextmenu`, bound to the right
   button in `plasma-bar-shell/contents/defaults`). Look at what the plugin
   adds for a panel containment and turn that entry off.
4. **A widget's settings window** opened from "Configure…" (the four
   plasma-desktop 6.8.0 files in `plasma-bar-shell/contents/configuration/`);
   right-click the clock at 1330,1040 with `eis click … right` in the steps
   file, then click the item.
5. **Side bars and other looks**: a left or right bar with widgets; skins,
   decorations and 3D looks along a bar (the span applies only to looks
   drawn flat along a line - `PanelScene.surfaceSpanned`).
6. **Move Arch Dock's Plasma-held edge panels into bars**, once, with their
   widgets (plugin ids, order, settings via Plasma's scripting), and the same
   in saved profiles. The owner's own three edge panels have no Plasma panel
   and no KDE widgets; this is for other setups.
7. Docs (CURRENT_STATE, KNOWN_LIMITATIONS, INSTALL), the complete serial
   suite, the step's commit and fresh-clone recheck, Report 2. Then step 5
   (each bar its own icons) and step 6 (package, update steps).

## Where the code is

- `src/bars/BarWidgets.*`: the list's rules (keys, plugin ids, the dock once).
- `src/bars/BarRecord.*`: `widgets` in each bar record.
- `src/bars/BarCorona.*`: `followWidgets` (make, remove, order through the
  containment's `AppletOrder`), `rememberWidget`, `watchRemovals` (a widget
  removed through its own menu is reported), `trimMenus`, and
  `m_widgetWatch` (declared last so the watching goes before Plasma's own
  closing).
- `src/bars/BarHost.cpp`: reports a user's removal to the backend
  (`barWidgetRemoved`).
- `src/panel/PanelWindowBars.cpp`: `barWidgetList`, `availableBarWidgets`
  (KPackage plus compiled plugins, the system tray allowed), `addBarWidget`,
  `removeBarWidget`, `moveBarWidget`, `barWidgetRemoved`; `widgets` in
  `barHostRecordsJson`. The panel field is `barWidgets` (`PanelDefinition`,
  schema, Internal).
- `plasma-bar-containment/`: `contents/config/main.xml` (`AppletOrder`);
  `main.qml` places widgets by it and tells the dock the bar's reach and
  depth (`barSurfaceBefore`, `barSurfaceAfter`, `barSurfaceThickness`); the
  dock lies under the widgets.
- `plasma-dock-widget/contents/ui/main.qml`: passes those to its scene
  (`surfaceSpanBefore/After/Thickness`).
- `qml/ArchDock/Rendering/PanelScene.qml`: `surfaceBox`,
  `spannedRendererGeometry` (with `bandWidth`), `spannedMotionTracks`,
  `surfaceIconCentres`.
- `qml/ArchDock/Rendering/renderers/PanelProcedural2D.qml`: the band as
  deep as `geometry.bandWidth`, the material over the whole capsule, no halo
  for a bar's band.
- `plasma-bar-shell/contents/defaults`: right-click bound to Plasma's
  context menu.

## How to check it

- Build: `/mnt/F/Arch Dock/build-codex-adrep/build`, at most `-j4`, one
  heavy step at a time (never scan or move big folders while the suite runs).
- Private desktop: `REPLICA_NAME=<new name> bash
  /mnt/F/archdock-audit-2026-10-08/tools/bar-replica.sh bars-widgets-steps.sh`
  (bar of 1100 px at the bottom, launcher, clock and tray added, menu
  right-click, launcher moved first, clock removed, the bar host stopped and
  started again). Pictures land in `evidence/<name>/`; `crop.py` and
  `sheet.py` in the same tools folder make the close-ups and sheets
  (`crop … 380 990 1160 90 1.0` is the bar strip).
- Tests: `barwidgets-test`, `barrecord-test`, `bar-host-smoke`,
  `panel-window-capability-test barWidgetsFollowTheirOperations`, the scene
  and renderer tests.
- Known test flakes: the Qt QML garbage-collector crash in an older Studio
  test (re-run passes), `wayland-hardening-scale200-test` under disk load.

## Cleanup done tonight (owner's request)

29.6 GB of old test runs and superseded pictures are in the F: drive's
Trash (the owner empties it). The owner's package folders
`/mnt/F/adfix2-pkg-0.1.1-19` to `-24` and the closed task pack `/mnt/F/Arch
Dock LCL repairs` (+ `.zip`) wait for the owner to remove them; the
installed version's package is kept at
`build-codex-adrep/installed-package/arch-dock-0.1.1-24-x86_64.pkg.tar.zst`.
