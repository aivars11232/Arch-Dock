# Owner list, piece 3: Panel Studio fixes - one Presets page, panel names, live desktop preview, editing profiles

Piece 3 of the owner's list of 2026-10-13, run the way pieces 1 and 2 were
("ADREP-style"): implement, verify (focused tests, then the complete suite
once, serially), Report 1, commit and push, a recheck on a fresh clone built
with one job, Report 2. Panel Studio pictures come from a private KWin with
KDE's default look; desktop pictures from a private Plasma desktop with
copies of Arch Dock's own settings and data only, the owner's files checked
unchanged after every run (`owner files untouched`). Pictures and logs are in
`build-codex-adrep/evidence/OWNER-LIST-PIECE-3/`.

The owner's requests (2026-10-13 and 14): "Presets and panel themes are same
thing, so they can be merged. Also in presets and theme section, the preview
there shouldn't be any icons, only bare panel"; "names shouldn't be just free
panel 1,2, etc. there should be proper names for those panels"; every change
made in Panel Studio shown on the desktop panel at once, kept with Apply or
OK and dropped with Cancel; and a saved profile edited, "not only profiles
but also editing it".

## Report 1 - implementation

### What changed for the owner

- **One Presets page.** Panels > Panel Themes / Skins and Panels > Built-in
  Panel Presets are one tab, Presets: the panel presets made for the panel
  (built-in first, then the owner's own, under "My presets"), then the
  looks, flat and platform, each with Load. Duplicating a preset puts the
  copy under "My presets" on the same page (`02-presets-merged-before-after.png`).
- **Previews show the bare panel.** Preset and look cards draw the panel's
  own shape and look only, no icons or places for icons
  (`03-preset-previews-zoom.png`, `04-look-previews-zoom.png`, zoomed in).
- **Panels have proper names.** A new panel is named for what it is: "Ring
  panel", "Circle panel", "Möbius strip panel", "Row panel", "Top panel",
  "Left panel", with a number when another panel already has that name
  ("Star panel 2"). Panels nobody named follow their shape: made a Möbius
  strip, "Ring panel" becomes "Möbius strip panel". The old "Free panel 1",
  "Free panel 2"... names are renamed that way once, when Arch Dock starts.
  Panels > General has a **Name** field: a name typed there stays whatever
  the shape becomes; emptied, the panel takes its automatic name again
  (`01-names-before-after.png`). The window title shows the name as edited.
- **Every change shows on the desktop at once.** What is changed in Panel
  Studio is drawn on the desktop panel straight away: its look, shape, size,
  glow, icons, the settings shared by all panels; an edge panel's Plasma
  panel takes its new size and place, and a free panel moves to a new
  position. Apply or OK keeps it, Cancel or closing the window puts the
  saved panel back - its look, its size and where it stood
  (`05-live-preview-before-after.png`: the ring made a Möbius strip, then
  Cancel; `06-live-position-before-after.png`: the octagon given another
  place, then Cancel). Nothing is saved before Apply: if Arch Dock stops
  in between, the saved panel is what comes back (an edge panel's Plasma
  size and place are put back from the saved settings at every start).
  - A change Apply would refuse (one that leaves the icons too little room)
    is not drawn; the Studio says so in orange: "Not shown on the desktop,
    and Apply refuses it: ...".
  - Moving a panel by hand while Panel Studio has changes does not lose
    them: the changes stay, on top of the new place. A setting changed
    elsewhere meanwhile keeps that newer value.
  - The Studio's preview line says "On the desktop - not saved until
    Apply"; its side note "Changes show on the desktop at once; Apply or OK
    keeps them, Cancel drops them".
- **Saved profiles can be edited** (`07-profiles-before-after.png`,
  `08-profile-panel-in-studio.png`). Profiles lists the panels the chosen
  profile shows. Each one can be:
  - renamed in its name field, in the profile only;
  - edited with **Edit…**: Panel Studio opens that panel's own pages
    (General, Appearance, Layout, Behavior, Animations, 3D, Icons, Icon
    Tiles, Pedestals, and the looks on Presets) on the profile's copy, under
    a line naming the profile and a **Back to Profiles** button. Apply saves
    the profile; the panels on the desktop change only when the profile is
    applied;
  - **removed** from the profile, after a question; a built-in edge panel,
    which every arrangement keeps, is **hidden** in it instead.

### How it works (for the code)

- The look cards are their own component (`qml/runtime/ThemeSampleCard.qml`);
  `PresetBrowser` lists presets and looks in one list with group headings.
- Names: `src/model/PanelNaming.*` (the words are the layout names Panel
  Studio shows) and one rule, `PanelRegistry::followShapeName`, used by
  every settings change and every direct update of a panel. The name is an
  identity value, not one the desktop draws.
- The live preview is a draft held by the backend
  (`PanelWindow::previewPanelSettingsEditorDraft`, `clearPanelSettingsPreview`):
  every Studio change already ran the backend's full check of the draft; the
  draft is now also what the panel is drawn with while the saved settings it
  was made on are current. Plasma's side follows a moment later, outside the
  Studio's call. A free panel's place and size are noted before the first
  draft and put back when it is dropped. The Studio's own Apply commits that
  same draft; any other change to the panel leaves the draft for the Studio
  to make again on the new settings (`SettingsEditorModel.rebase`). This was
  chosen over running each Studio session as a desktop audition, which
  breaks when the panel is moved by hand meanwhile.
- Profile editing: `ProfileManager::profilePanels`, `renameProfilePanel`,
  `removeProfilePanel`, `replaceProfilePanel`, and the editor calls
  `PanelWindow::profilePanelEditorSnapshot`, `resolveProfilePanelEditorDraft`,
  `applyProfilePanelSettings`, `profilePanelThemeCandidate`, which run the
  same checks as for a desktop panel on the profile's copy (the editor
  functions now work on a panel definition, from the desktop or a profile).
  A profile's revision guards its edits.

### Verification

- Tests added: `PanelWindowCapabilityTest` -
  `studioDraftsShowOnTheDesktopAtOnce` (the draft drawn, nothing saved,
  Cancel, a refused draft not drawn, Apply, another change meanwhile, shared
  settings), `studioChangesShowOnTheDesktopAtOnce` (through Panel Studio
  itself: Cancel, Apply after a change made elsewhere, closing the window),
  `panelsNobodyNamedFollowTheirShape`, `profilePanelsAreEditedInTheProfileOnly`,
  `studioEditsAProfilesPanel`; `PanelRegistryTest::namesPanelsByWhatTheyAre`;
  `tst_SettingsEditorModel` (`test_rebaseKeepsTheEditorsChangesOnNewSettings`);
  `tst_ProfilePage` (`test_profilePanelsAreRenamedEditedAndRemovedInTheProfile`);
  `tst_PresetBrowser` (`test_presetsPageListsPresetsAndLooks`);
  `tst_StudioNavigation` (one Presets page). The Studio truth matrix
  documents the Name field, and its restore check now prints the values that
  differ.
- What the tests caught, fixed: the truth matrix showed a panel renamed only
  through Panel Studio's changes, not through direct updates, so a panel
  could carry a name that did not fit its shape (the rule now lives in the
  registry, for both); the preset library test showed the name travelling
  in the drawing values of preset previews (it is no longer one); the
  preview contract named the look card's old file.
- Private desktop (`replica-live-preview/`, the replica settings of piece 2
  with free-1 and free-2 on the desktop): a Studio draft making free-2 a
  Möbius strip was drawn at once and Cancel put the ring back; a position
  draft moved free-1 (to 1321, 520: asked 1100, 420, it kept its distance
  from free-2) and Cancel put it back at 160, 176. The first run caught a
  real fault: free-2's widget grew for the Möbius strip (560 to 656 pixels)
  and stayed grown and moved after Cancel. A free panel's place and size are
  now noted before the first draft and put back; the second run had free-2
  at 688, 176, 560 x 624 before and after. Owner files untouched in both.
- Panel Studio pictures (`before-kde/`, `after-kde/`, sheets 01-04 and
  07-08) come from a private KWin: pictures taken without a graphics driver
  cannot draw a flat look's glow layer and showed the look cards empty.
  Every sheet was checked zoomed in; that check caught a false orange
  "Placement detail" note and dark labels on the Profiles page, both fixed.
- Builds: every build with one job.
- Complete suite, serially (`superseded/30-full-suite-first-run.log`): 119
  of 120, before the last fixes for profile mode (Overview's name, the desktop
  open and close buttons) and the wording of four comments. On the final
  files (`30-full-suite.log`): 119 of 120 in 2447 s. The one failure in both
  is `preset-audition-matrix-defaults` in step S13, the test harness's
  restart race recorded in pieces 1 and 2 ("Arch Dock is already running.",
  "Timed out stopping the private Arch Dock service"), left for the cleanup
  pass.

### Questions for the owner

1. **What still waits for Apply.** Hiding or showing a panel, and how it
   hides (always visible, auto-hide, dodge windows), change on Apply, as in
   a preset's desktop preview: a panel being edited stays in view. Should
   those show at once too?
2. **The names.** Panels are named for their shape ("Ring panel", "Möbius
   strip panel") or their edge ("Top panel"). Is that what you meant by
   proper names, or should they carry their look's name (for example
   "Lightning ring")?
3. Still open from piece 2: the icon limit on tilted 3D panels (count what
   the camera shows, or the size as drawn?) and whether the riding icons on
   the Möbius strip (piece 2, pictures 22-25) are as you meant.
4. Earlier and still open: a KDE widgets phase; a 3D underside drawn like the
   top; removing the Perspective tilt on 2D and baked panels.

### Known limitations

In `docs/KNOWN_LIMITATIONS.md`: showing or hiding a panel, and its hiding
mode, still change on Apply; artwork import, re-render and clear are file
operations done on Apply; a free panel given a new position keeps its
distance from the other free panels and may stand a little off the place
asked for; a change Apply would refuse is not drawn; while a preset's desktop
preview runs, it is what the panel shows. A profile's panel is edited
without presets, artwork, the desktop's 3D handles or shared settings, and
panels are not added to a profile on that page. The desktop pictures were
made with the same backend calls Panel Studio makes on each change, because
nothing can click inside Panel Studio on the private test desktop; the
Studio's own path is covered by its tests.

## Report 2 - recheck

- A fresh clone of origin/main at `bacafff`, configured and built with one
  job (1593 s, `41`/`42-recheck-*.log`), passed the 19 focused tests
  (`43-recheck-tests.log`): panel-model, profile-store,
  profile-apply-transaction, profile-manager, preset-preview-session,
  preset-library, panel-settings-schema, panel-settings-transaction,
  panel-registry, panel-window-capability, live-panel-preview,
  runtime-ui-interaction-smoke, studio-preview-contract, preset-browser,
  preset-audition, profile-page, studio-navigation, settings-editor-model and
  configuration-upgrade.
- All 659 committed files are the same in the clone and in the workspace
  (`44-recheck-source-compare.txt`).
- The complete suite ran twice on this piece before the push, 119 of 120 each
  time; the one failure is the known restart race of
  `preset-audition-matrix-defaults` in the test harness.

Piece 3 is closed. The owner's answers of 2026-10-15: hiding and showing a
panel stay on Apply; the Möbius strip belongs in Panels > Layout > Dock
layout, as it is; that list is to show proper shape names, and a panel is
to be named exactly as its shape is listed ("Ring", "Möbius Strip", "Ring
2"); the icon limit on a tilted 3D panel is to count its size as drawn; the
riding icons on the Möbius strip are as meant. The names, the list and the
icon limit change in piece 4, with the UI themes.

