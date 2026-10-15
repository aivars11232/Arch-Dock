# Owner list, piece 4: shape names, one Polygon, the icon limit as drawn, Panel Studio UI themes

Piece 4 of the owner's list of 2026-10-13, run the way pieces 1 to 3 were
("ADREP-style"): implement, verify (focused tests, then the complete suite
once, serially), Report 1, commit and push, a recheck on a fresh clone built
with one job, Report 2. Panel Studio pictures come from a private KWin with
KDE's default look; desktop pictures from a private Plasma desktop with
copies of Arch Dock's own settings and data only, the owner's files checked
unchanged after every run (`owner files untouched`). Pictures and logs are in
`build-codex-adrep/evidence/OWNER-LIST-PIECE-4/`; the desktop runs in
`/mnt/F/archdock-audit-2026-10-08/evidence/piece4-icon-limit-*`.

The owner's requests: the answers of 2026-10-15 to piece 3's questions - a
panel is named exactly as its shape is listed ("Ring", "Möbius Strip", "Ring
2"), the Dock layout list shows proper shape names, the named polygons go
("Since in settings I can just increase or decrease the corners of polygons,
the rest of them in that list are sort of useless"), the icon limit on a
tilted 3D panel counts its size as drawn, hiding and showing a panel stay on
Apply; and Panel Studio UI themes: "Yes, make multiple UI themes", "make a
separate side tab with themes and their previews", the window solid ("my
desktop wallpaper shouldn't be there"), Arch Dock's icon before the section's
name ("the section should be - icon, UI themes"), and "Add 3 Alienware themes
too, like white, black and Green Alienware theme" ("No need for marks and
logos").

## Report 1 - implementation

### What changed for the owner

- **Panels are named as their shape is listed.** A free panel nobody named
  is "Ring", "Möbius Strip", "Star", "Circle", with a number when the name is
  taken ("Ring 2"); an edge panel stays "Top panel", "Left panel". The names
  earlier versions gave ("Ring panel", "Free panel 3") are renamed that way
  once; a name typed in Panels > General > Name stays.
- **The Dock layout list shows proper names, and one Polygon**
  (`02-dock-layout-list-before-after.png`, approved: "Good"). The list reads
  Row, Column, Diagonal, Ellipse, Ring, Radial, Arc, Semicircle, Fan, Spiral,
  Ribbon, Vertical curve, Horizontal curve, Polygon, Star, Grid, Floating,
  Möbius Strip. Triangle, Square, Pentagon, Hexagon and Octagon are gone: a
  panel that was one of them is a Polygon with that many corners and looks
  the same (its settings are backed up before they are rewritten). As
  before, a shape that draws exactly like the one in use is left out (with
  the panel a Ring, Circle; on a free panel, Adaptive is listed as the Row
  it draws). Preset cards name their layouts the same way: "Layout: Ring,
  Circle", "Layout: Polygon" (`07-preset-layout-names-before-after.png`).
- **The icon limit counts a 3D panel at its size as drawn**
  (`05-icon-limit-before-after.png`). On the private desktop the steeply
  tilted platform free-1 had room for 1 icon as seen and held 7, so a folder
  added to it was refused; now its room is 13 at its drawn size, the folder
  was added and the platform draws 7 icons (one of its 8 pinned folders,
  `/mnt/F/Nexees_PostCore_LCL_Implementation_Pack_v1.1.0`, no longer exists
  and is not drawn). When a panel is full, its notice now says "Make it
  larger or its icons smaller in Panel Studio."
- **Panel Studio has UI themes** (`03-studio-themes-before-after.png`,
  `06-ui-themes-page.png`). Seven looks: Classic (the deep blue Panel Studio
  had), Teal Glass, Teal HUD, Amber HUD, Alien White, Alien Black and Alien
  Green. The new UI Themes side tab shows each one drawn small, with "Use
  this theme"; the choice changes Panel Studio at once and is remembered.
  Panels on the desktop are not touched, and the window is solid in every
  theme. Arch Dock's own icon stands before "UI Themes" in the sidebar
  (`01-ui-themes-icon-before-after.png`, approved: "Yes, now it's good").
  Panel Studio's buttons, lists, sliders, fields, tabs and the Icon
  properties window now take the theme's colours (KDE's own controls kept
  their white faces, see "Before" in sheet 03). The Profiles page and the
  preset and look cards follow the theme too.
- **The alien themes** were first only recoloured, and the owner rejected
  them ("They don't look so alien to me"). They now carry the look of gaming
  machines: honeycomb vents faintly over the window, glowing light strips
  under the title and on the chosen section and tab, a neon glow along the
  frame (Alien Black and Alien Green), thin wide capitals for headings;
  Alien White is two tone, a black title strip and sidebar with white pages,
  as on the white machines. No maker's marks or logos. All three approved.
- Smaller fixes found on the pictures: the dark block at the end of the tab
  row is gone; a chosen button's text ("Free panel") is readable in every
  theme; the tab labels in Alien White were light grey on white
  (`04-alien-white-tabs-before-after.png`, approved: "Good"); the Presets
  page's title follows the theme's lettering.

### How it works (for the code)

- Names: `src/model/PanelNaming.*` - `panelShapeName` gives the list's word
  for a layout, `panelBaseName` a panel's plain name, and the earlier
  automatic names (`"<shape> panel"`, "Free panel N", "Dock panel") count as
  automatic, so `PanelRegistry::followShapeName` renames them.
- Polygons: `namedPolygonCorners` (`src/model/PanelDefinition.*`) maps
  triangle to octagon to "polygon" with 3 to 8 corners when a record is read;
  the registry backs up and rewrites such a record on load. The layout
  choices (`PanelWindowSettings.cpp`) skip the named polygons and label each
  layout with `panelShapeName`; `PresetLibrary` names card layouts the same way.
- Icon limit: the widget reports `panelScene.seenTrackFactor(16, false)`,
  the share of its track a 3D panel's icons get at its size as drawn, instead
  of as the camera shows it.
- Themes: `qml/runtime/StudioThemes.js` holds each theme's roles (colours,
  corner shapes, lettering, honeycomb, light strips, edge glow, heading
  weight); `DockSettings::studioTheme` keeps the choice as `studio/theme`,
  accepting only an id of lower-case words joined by dashes, on load and on
  save. Panel Studio's files use Qt's Basic controls, which follow the window
  palette set from the theme (KDE's own style ignores it).
  `StudioFrame.qml` draws frames rounded or with cut corners and the neon
  glow; `StudioHoneycomb.qml` (a canvas, drawn once per size) and
  `StudioLightStrip.qml` draw the alien details; `StudioThemesPage.qml` and
  `StudioThemePreview.qml` are the side tab and its previews.
- KDE's feel kept under the Basic controls: the Studio's three scroll views
  carry KDE's `Kirigami.WheelHandler`, every Studio tooltip waits before
  showing, and the place buttons shrink in a narrow window
  (`Layout.minimumWidth`). `PanelScene` starts the wheel's travel afresh
  when a polygon or star gets another number of corners, as it does for a
  new layout.

### Verification

- Tests: `studio-themes-test` (new, `tests/tst_StudioThemes.qml`): every
  theme has every role, valid ids and colours, a solid window, and words
  that meet 4.5 to 1 against their background anywhere on the window (3 to 1
  for faint small print and the theme's colour); an unknown theme is Classic;
  the lettering; the UI Themes page offers each theme, marks the one in use
  and reports the one chosen. `PanelRegistryTest::persistsStudioThemeChoice`
  (kept, normalized, refused ids, a damaged stored value reads as Classic).
  `tst_StudioNavigation` (the seventh section). `PanelWindowCapabilityTest`:
  `panelsNobodyNamedFollowTheirShape` and `PanelRegistryTest::namesPanelsByWhatTheyAre`
  with the new names and an octagon read back as Polygon with 8 corners;
  `studioPagePictures` takes every page in all seven themes and the open
  Dock layout list.
- What the checks caught, fixed: measuring every theme's contrast, done
  before the readability test was written, found Alien White's first blue
  too light (3.4 to 1 as text on white, 3.5 with white text on it), now a
  deeper blue at 5.4 and 5.75; the pictures showed the faint tab labels in
  the two-tone theme (the tabs used the sidebar's text colour on the page's
  rows), the dark end of the tab row, unreadable text on a chosen button, and
  the theme previews still saying "Ring panel".
- Private desktop: before, free-1 (room 1 as seen, 7 pinned) refused the
  folder; after, room 13 at its drawn size, the folder accepted, 8 pinned
  and 7 drawn. My first comparison sheet was cropped on the other free panel
  (the purple ring, free-2, which was not changed), so it showed no
  difference, and I said so to the owner. Three more runs settled it: the
  backend served 6 entries before and 7 after; debug lines added to private
  copies of the widget and scene showed the widget and its scene holding 7;
  and the picture with the right crop shows the seventh icon. Owner files
  untouched in every run.
- Builds: every build with one job.
- First complete suite, serially (`superseded/30-full-suite-first-run.log`):
  113 of 121 in 2460 s. Five failures were tests still expecting the old
  names or controls: the Octagonal Platform preset now reads as a Polygon with 8 corners
  (`preset-catalog-test`), the list says "Möbius Strip"
  (`panel-window-capability-test`), a hexagon panel is now a Polygon whose
  Corners setting is offered (the truth matrix, in the capability test and
  `studio-truth-matrix-smoke`; its case is now a polygon with 6 corners), the
  travel smoke's triangle, square and hexagon read back as "polygon"
  (`path-travel-smoke`), and the preset browser test looked for buttons of
  the test's own style (`preset-browser-test`, now any style's). Two showed
  real faults this piece brought, and a third appeared once the travel smoke
  got past its old name; all three fixed:
  - `studio-scroll-test`: Qt's Basic scroll views move 24 pixels per wheel
    line at once, KDE's 20, smoothly; Panel Studio's three scroll views have
    KDE's wheel handler again, so they scroll as before.
  - `runtime-ui-interaction-smoke`: in a narrow Studio window the arrow that
    scrolls the tabs did nothing. Measured in the private session: the page
    was 520 pixels wide in a 492-pixel view, because the five place buttons
    (Free panel ... Right edge) are wider in the Basic style, which pushed the
    arrow outside the visible part of the page. The buttons now shrink, their
    words cut short, instead. Panel Studio's tooltips also wait as KDE's do
    before showing (the Basic style shows them at once).
  - `path-travel-smoke`, once it read "polygon" back: the icons moved by the
    wheel on the triangle were still moved when the panel became a square,
    because both are now one layout, a polygon, and only a new layout started
    the icons afresh. A polygon or star given another number of corners now
    starts afresh too. (Its rerun also missed a 120 ms timing limit once, by
    10 ms, on a slow frame; two full reruns passed.)
  The eighth, `preset-audition-matrix-defaults`, is the test harness's
  restart race recorded in pieces 1 to 3 ("Timed out stopping the private
  Arch Dock service").
- After the fixes, each failed test was run again on its own and passes:
  `preset-catalog-test`, `studio-scroll-test`, `preset-browser-test`,
  `panel-window-capability-test`, `studio-truth-matrix-smoke`,
  `runtime-ui-interaction-smoke`, `path-travel-smoke` (twice), with
  `studio-themes-test` and `studio-navigation-test`; the audition matrix
  failed again at its known step S13.
- Complete suite on the committed files (`30-full-suite.log`, 96489f9): 119
  of 121 in 2535 s. An earlier run was stopped when the owner paused the work,
  while `panel-window-capability-test` was running; one of its tests had saved
  a profile ("Work", 03:38:36) in the tests' own data folder and was killed
  before its cleanup. That leftover made `profileServicePersistsAndHonorsPanelGuards`
  find a profile at its start in this run. With the file removed, the whole
  capability test passes again and leaves no profile behind
  (`31-capability-rerun.log`). The other failure is the known restart race of
  `preset-audition-matrix-defaults` in step S13.

### Questions for the owner

None new. Still open from earlier: a KDE widgets phase; a 3D underside drawn
like the top; removing the Perspective tilt on 2D and baked panels. Before
the phases that draw on the owner's sample pictures, those samples are
researched first (how each one behaves, responds, looks and feels), as the
owner asked on 2026-10-15.

### Known limitations

In `docs/KNOWN_LIMITATIONS.md`: KDE's own dialogs opened from Panel Studio
(the colour picker, file choosers) keep the system's look; the honeycomb is
drawn again when Panel Studio is resized; a pinned file or folder that no
longer exists is not drawn but keeps its place in the icon limit, so a drive
that is unmounted for a while cannot let a panel overfill when it returns.

## Report 2 - recheck

- A fresh clone of origin/main at `96489f9`, configured and built with one
  job (1463 s, `41`/`42-recheck-*.log`), passed the 21 focused tests
  (`43-recheck-tests.log`): panel-model, profile-store,
  profile-apply-transaction, profile-manager, preset-preview-session,
  preset-library, panel-settings-schema, panel-settings-transaction,
  panel-registry, panel-window-capability, live-panel-preview,
  studio-truth-matrix-smoke, runtime-ui-interaction-smoke,
  studio-preview-contract, preset-browser, preset-audition, profile-page,
  studio-navigation, studio-themes, settings-editor-model and
  configuration-upgrade.
- All 667 committed files are the same in the clone and in the workspace
  (`44-recheck-source-compare.txt`).
- No crash reporter was left by any private test session.

Piece 4 is closed. Work stops here at the owner's request of 2026-10-15;
the next piece on the list is Phase 3, importing a picture for icons, icon
tiles and stands. The samples research the owner asked for before any
sample-based change is done (outside the repository, in the audit folder).
