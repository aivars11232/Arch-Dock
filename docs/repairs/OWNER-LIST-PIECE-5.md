# Owner list, piece 5: Import picture for panels, stands and icon tiles (Phase 3)

Piece 5 of the owner's list of 2026-10-13, run the way pieces 1 to 4 were
("ADREP-style"): implement, verify (focused tests, then the complete suite
once, serially), Report 1, commit and push, a recheck on a fresh clone built
with one job, Report 2. Panel Studio pictures come from a private KWin with
KDE's default look; desktop pictures from a private Plasma desktop with
copies of Arch Dock's own settings and data only, the owner's files checked
unchanged after every run (`owner files untouched`). The sample picture is a
KDE wallpaper that ships with Plasma (Colorful Cups), copied into the private
home; it is a test picture only, nothing of it is added to Arch Dock.
Pictures and logs are in `build-codex-adrep/evidence/OWNER-LIST-PIECE-5/`;
the desktop runs in `/mnt/F/archdock-audit-2026-10-08/evidence/piece5-pictures-*`.

The owner's requests: Phase 3 of the plan of 2026-10-13, "for Icons,
pedestals, icon tiles and panels, there should be button or option to import
a picture and the Arch Dock renders that picture properly for panel, icon,
pedestal or icon tile theme". On icons (2026-10-15): "for Icons all I need is
to have them animations and for them to be real 3D icons, not to color them.
Icon tiles can be colored, but not just simply color, but some glow, texture
and stuff, and Icon it self must be clearly visible, so those colors should
not affect icons at all." And: "don't take away neon, plasma and other themes
that I already have there, if there's nothing wrong with them, don't even
touch them".

## Report 1 - implementation

### What changed for the owner

- **An Import picture… button** in Panel Studio for the panel (Panels >
  Appearance), the icons' stands (Pedestals), the stands of an opened
  folder's items (Pedestals, under Folder item stands) and the icon tiles
  (Icon Tiles). It opens a
  file chooser for pictures (PNG, JPEG, WebP, BMP, GIF, SVG, TIFF, AVIF,
  JPEG XL). Arch Dock keeps its own copy in its data folder
  (`~/.local/share/Arch Dock/Arch Dock/pictures`), no larger than 2048
  pixels on its longest side, so the original can be moved or deleted later;
  the original file is only read. The same picture imported twice is kept
  once. A file that is not a picture, or one too large (over 64 MB, 16384
  pixels on a side or 16 million pixels), is refused with a message. A
  photo its camera stored on its side (as phones do) comes in upright; it
  was refused before, here and when importing theme artwork.
- **A Picture choice** joins the end of each surface's look list: a flat
  panel's Theme (Panels > Appearance), a 3D panel's Look (Panels > 3D), and
  the Look of the stands, of the folder item stands and of the tiles. It is
  listed only once that part has a picture. Importing puts the picture on at once; a small preview shows
  it; the × beside it removes the picture, and the part gets back the look
  it had before (a Neon panel is Neon again). Once Panel Studio has been
  closed in between, it gets a plain one instead (Glass for the panel, its
  material for a stand, no texture for a tile). As every Studio change,
  nothing is saved before Apply.
- **Flat panels** wear the picture once over their own shape (a ring, star,
  Möbius strip...), scaled to cover all of it; the glow stays behind it.
- **3D panels** wear it round the band, repeated at its own proportions, in
  its own colours, solid and still (`02-desktop-pictures-before-after.png`).
  The first version laid the look's own colour over it: the octagon
  platform (Futuristic, teal) showed the picture only as faint marks in teal
  (`01-3d-picture-own-colours-before-after.png`). Now the look's colour,
  metal sheen, glow and flow are left off while a picture is worn, and the
  band's rim is a plain light edge round it (the ring's pink glow in
  "Before" became a white one).
- **3D panels wear the picture in real relief.** The owner on the pictures
  above: "For 3D panels there shouldn't be mere picture, if the picture is
  rocks, I should be able to see rocks same as I see them in real world,
  meaning 3D with some step-ups and downs ... if I would put my hand into PC
  I should be able to feel the texture of that picture in 3D" (and: "for
  tiles it's okay as is"), and later: "I don't need 2D disguised as 3D. I
  need real 3D". The picture's brightness is the surface's height, and the
  surface itself moves: a panel wearing a picture is cut finely enough for
  a stone or a peak to be a bump of its own (a generated band, and a
  theme's own platform such as Mesh Platform Cyan's octagon alike), its
  top, underside, walls and edges moving with the picture, the light
  falling on every bump, the stones' shadows falling into the gaps away
  from the light. As deep as the new **Relief depth** (Panels >
  Appearance, shown only on a 3D panel wearing a picture; 0 is a flat
  picture, the default 0.8 the look the owner approved, "With ✓ is good",
  1 a little deeper; the first scale went on to twice that, which the owner
  rejected).
  The way there, in the owner's words: a first version rose mostly at the
  band's sides ("relief can come out on top and bottom too, not just
  side"); the next looked "a bit to chaotic and unfinished" and needed
  "some order and polishing, to look more realistic", "like, putting those
  rocks in an invisible box that's shaped like tunneled ring ... like in
  these days people have at homes and offices rocks in mesh as a fence": a
  gabion. So by default (**Relief direction: Inward**) the picture's raised
  parts (a stone's face) stay at the band's own surface, as stones pressed
  against a gabion's mesh, and the rest (the gaps) sinks in: the band keeps
  its clean outline, the stones lie in order within it, a faint polish over
  them. A per-pixel tracing of the relief, which an intermediate version
  added on top of the moving mesh, is gone: it was a picture made to look
  deep, not depth.
- **Relief direction: Outward, and Raised parts: Light or Dark.** On the
  Mountain wallpaper the owner wanted "a little mountains come out on top
  of the panel". With **Outward** the raised parts rise out of the band
  instead, as little mountains do; **Raised parts** says which parts those
  are, the light ones (the default) or the dark ones, since a picture's
  brightness alone cannot tell (in a picture of stones the stones are
  light, in Mountain the mountains are dark under a lighter sky). A photo
  taken from the side, as Mountain is, holds no heights of its own; a
  picture seen from above (stones, bark, bricks, a terrain) turns into its
  real shape. The owner judged the panels laid fully horizontal ("if Panel
  would be fully horizontal, I could tell better"), with a test terrain
  seen from above made for this (`11-3d-relief-level.png`).
- **No edge lines with a relief.** The owner: "when relief is applied, those
  contours can be gone too ... since it's rocks, it should be more like back
  in old days when we had terass out of rocks, so the contours weren't that
  visible" ("By contours I mean inner and outer edges of the panel"). A
  panel wearing a picture in relief has no glowing inner and outer edge: the
  stones run over its rounded edges as on a stone terrace. The soft halo a
  panel's own Glow setting drew round it went as well (the owner: "Glow not
  practicall for rocks, unless it's uranium"): a panel or a stand wearing a
  picture draws no glow, and its Glow settings are not shown, as on
  Organic.
- **Stands** (3D and flat) wear it in its own colours; their Glow setting
  still applies.
- **Icon tiles** wear it behind the icon, under a light veil in the tile's
  own colour (35 %), so a busy picture never makes the icon hard to see
  (`03-tiles-icons-clear-before-after.png`).
- **Icons are never coloured.** A first version also coloured the icons
  with the picture; the owner rejected it ("I never gave u that task at
  all"), and it is gone without a trace. Icon animations and real 3D icons
  are Phase 4.
- **Every look the owner already has is untouched.** Picture is one more
  choice; Neon, Plasma, Lightning, Aurora and the rest draw exactly as
  before (every change in the renderers applies only while a part wears a
  picture).

### How it works (for the code)

- Settings (`PanelSettingsSchema`): `panelPicture` (`surface.picture`),
  `pedestalPicture` and `folderPedestalPicture`
  (`surface.parameters3D`), `iconTilePicture` (`iconStyle.tilePicture`).
  Their normalization `PictureUrl` keeps only a local file URL with an
  absolute path; anything else, a web address among others, reads as no
  picture. "picture" joins the choices of `appearance`, `scene3DTexture`,
  `pedestalTexture`, `folderPedestalTexture` and `iconTileTexture`. The
  panel's picture has its own capability, `panel-picture`, offered on flat
  and 3D panels.
- Import: `PanelRegistry::importPicture(url)` refuses anything but a local
  plain file, checks the size before decoding (turned upright when the
  file says so), and hands the file to
  `ThemeAssetProcessor` (decode, scale to 2048 pixels at most, write PNG,
  content-addressed, so an equal picture is the same file). It returns the
  copy's file URL and size, or a message.
- Panel Studio (`SettingsPopup.qml`, `StudioForm.qml`): a "picture" row
  (preview, Import picture…, and × while there is a picture); importing
  sets the picture, gives stands a Disc shape when they had none, folder
  item stands their own settings and tiles their switch, then sets each
  Look the picture is worn as (`pictureLooks`: `appearance` and
  `scene3DMaterial` for the panel, `pedestalMaterial`,
  `folderPedestalMaterial`, `iconTileTexture`) to "picture" wherever the
  panel offers it, with the setting paired with that Look as choosing it in
  its list does (`setLook`: the 3D texture "picture", a stand's texture its
  material, a tile's 3D finish Minimal). The previous Look is remembered per
  panel and setting while the window is open (`putOnPicture`,
  `takeOffPicture`). A Picture choice without a picture is not listed.
- Drawing: `PanelProcedural2D.paintPicture` draws the band, then the
  picture once, cover-fitted, only where the band is (`source-atop`), then
  the glow behind. `PanelScene3D`: `lookTextureSource` gives the picture as
  the texture; `wearPicture` sets a white, unshaded, non-metal, solid
  material with no glow over it, for the platform and the stands; a
  generated platform repeats the picture round the band by its own
  proportions and does not flow it; no relief map for a picture. Picture
  as a Look or texture without a picture falls back to the look it stands
  for: a 3D panel's to its theme's own, a stand's to the panel's.
  `IconScene` paints a stand's picture cover-fitted and untinted, and a
  stand in the panel's look on a panel wearing a picture wears the panel's
  picture (an opened folder's stands get it from `folderPedestalSettings`);
  `IconTile` paints the tile's picture cover-fitted and clipped to the tile,
  then the 35 % veil in the tile's colour, then the icon on top, unchanged.
- Relief: `PlatformGeometry.platform({relief: true})` builds the platform
  with `reliefPlatform`: the band's whole cross-section is one strip of
  points shared from face to face (no rim range, so no edge is drawn on its
  own), its corners rounded, its top and underside cut into 18 strips, its
  walls into 8 and its rims into 3, its runs into steps of a 72nd of the
  cross-section's length, coarser only where a long band would pass 30000
  points (`refineRun`). The texture runs on round a polygon's or a star's
  corners, whose points take the way out between their two sides. A
  theme's own mesh is cut by `reliefMesh`: every triangle alike, so
  neighbours share every point of their edge, at most 24000 triangles, a
  point faces share moving the one way between theirs. Each point carries
  in uv1's x how far it may move (all the way, nothing at an arc's ends or
  where a Möbius strip meets itself; nothing on a folded platform) and the
  mesh says its step length (`meshStep`). A generated band stays within
  15000 points. `optional3d/PictureRelief.qml` is
  a Qt Quick 3D custom material (`picture-relief.vert` and `.frag`,
  compiled by Qt when it is first drawn): its vertex part moves each point
  along its normal by the picture's brightness (or darkness, Raised parts),
  blurred to about two of the picture's points per step of the mesh, inward
  or outward (Relief direction), and on a band leans the normal with the
  slope; its fragment part takes the picture's colours (made linear, as the
  built-in materials do), bends the light by the picture's grain finer than
  the mesh, shadows it where the relief rises toward the key light
  (`keyLightDirection`, the key light's own) and darkens the low parts. A
  theme's mesh gets the picture laid on each face from the side it faces
  (`boxScale`). `IconStyle3D.reliefMaterial` draws the body with it. Relief
  depth is `panelPictureDepth` (`surface.parameters3D`, capability
  `picture-relief`; the default 0.8 moves the surface 0.08 of the scene's
  units, 1 moves it 0.1, the platform being 0.18 thick); Raised parts is
  `panelPictureRise` ("light" or "dark", the shaders' `reliefInvert`);
  Relief direction is `panelPictureDirection` ("inward" or "outward",
  `reliefOutward`). A picture's look fits no glow (`glowFitsLook`), and the
  renderers draw none for it (`PanelScene3D.panelGlowAmount`,
  `pedestalGlowAmount`, `PanelSurfaceLoader.flatGlowActive`).
- `ThemeAssetProcessor` expects a picture stored on its side to come out
  with its sides swapped (Qt turns it upright as it reads it), instead of
  refusing it as "not decoded deterministically".

### Verification

- Tests (new or extended):
  - `PanelRegistryTest`: `importsPicturesIntoArchDocksOwnFolder` (a 4096 by
    1024 picture becomes a 2048 by 512 copy in Arch Dock's pictures folder,
    the original unchanged, the same picture kept once, a small one kept at
    its size; a text file, a missing file and a web address refused with a
    message), `keepsPicturesAsLocalFiles` (a web address is no picture; the
    pictures stay through a reload), `importsTurnedPhotosUpright` (a
    picture stored on its side comes in upright, 100 by 400; a folder is
    refused).
  - `PanelRegistryTest::importsGreyPicturesInColour`: a black and white
    picture comes in as a colour copy, which the relief reads.
  - `ThemeAssetProcessorTest::readsTurnedPhotosUpright`.
  - `tst_PlatformGeometry::test_pictureReliefMesh`, for every shape: the
    relief's platform is still one closed surface facing outward, cut more
    than twice as finely and within its point limit, every point two runs
    share moving alike, nothing fading at a polygon's corners, no rim drawn
    on its own; without a relief the platform is exactly as before; folded,
    it keeps its light and does not move. `test_themeMeshCutForRelief`: a
    theme-like octagon cut for a relief stays closed and facing outward,
    within its triangle limit, every shared point moving alike, its corners
    staying put.
  - `tst_IconTiles`: the picture covers the tile, both its halves showing
    through the veil; Picture without a picture draws the plain tile.
  - `tst_PanelMaterials::test_pictureFillsThePanelOnce`: a flat ring wears
    the left half of the picture on its left and the right half on its right.
  - `tst_IconScene::test_standPictureLook`: a stand's own picture, Picture
    without one falling back to the panel's look, and a stand in the panel's
    look wearing the panel's picture.
  - `PanelWindowCapabilityTest::studioPicturesComeAndGo`: importing through
    Panel Studio puts the copy on as the panel's Theme, × gives the Neon
    panel its Neon back; tiles that were off are turned on and wear it; a
    stand without a shape gets a Disc and Picture as its Look; a file that
    is no picture is refused with a message and changes nothing; nothing is
    saved before Apply; Picture chosen in the tiles' Look list applies, with
    their 3D finish a plain one. `studioPagePictures` takes the three pages
    with pictures, scrolled to their Picture rows.
  - `RendererCapabilityTest::generatedPlatformsFollowTheLayoutAndLook`, on
    real graphics under a private KWin (in `rendering-import-smoke`): a 3D
    ring wearing a red and blue picture shows red and blue between its
    icons and no cyan of its Neon look, solid and still, with no tint,
    metal or glow over it (the plain ring and a theme package's ring);
    Picture as its Look wears it the same way; without the picture that Look
    is the theme's own and the scene stays drawn. The ring wears it in
    relief: its mesh rises (`risesReady`), its body is drawn with the relief
    material, and Relief depth 0 and 1, Raised parts Light and Dark, and
    Relief direction Inward and Outward each draw it differently. A theme's
    own mesh wearing a picture is cut finely and rises too, and Relief depth
    changes it.
    `packagedBuildFacts` finds the relief's three files in the module.
  - The Studio truth matrix: the four picture settings reach their
    renderers.
- What the checks caught, fixed:
  - The private desktop showed the octagon platform in plain teal: the 3D
    platform laid the look's own colours, metal sheen and glow over the
    picture. A picture is now worn in its own colours
    (`01-3d-picture-own-colours-before-after.png`). A picture would also have
    flowed round the band with an energy look, and a wide one was squeezed
    into a square on each repeat; it now holds still at its own proportions.
  - With tiles switched off, or a stand without a shape, importing turned
    them on but did not put the picture on, since their Look is offered only
    once they show (found by the new Studio test). The part is shown first.
  - Panel Studio shows one Look per surface. The first version put a 3D
    panel's and a stand's picture on the texture setting that Look hides,
    so their Look list neither showed nor offered Picture, and Picture
    chosen in the tiles' Look list set a 3D finish no tile has, which Apply
    would refuse. Picture is now a choice of each Look, with valid paired
    settings.
  - A flat stand in the panel's look on a panel wearing a picture looked for
    a stand picture of its own; it wears the panel's now.
  - A photo stored on its side was refused by the picture processor that
    imports pictures and theme artwork; it now comes in upright.
  - The × was drawn, greyed, even with no picture to remove; it is shown only
    while there is one.
  - The owner rejected the 3D panel's flat picture (see above); the relief
    replaced it. In the first relief run the Grey wallpaper came out red
    and black: the test copy made for it was a one-colour (grey) PNG, which
    the relief material reads as red. Arch Dock's import always writes a
    colour copy (now pinned by `importsGreyPicturesInColour`); the test
    pictures were saved again the same way.
  - Earlier in the piece: 3D panels refused the panel's picture settings
    (they now have a capability of their own), a ring with Picture but no
    picture turned plain white (it falls back to its look), stands wearing
    a picture were tinted (now untinted), and the icon colouring the owner
    rejected was removed.
- Private desktop: five runs (`piece5-pictures-1` to `-5`), the octagon
  platform and the ring of the owner's copied settings wearing the test
  picture step by step; owner files untouched after every run; no crash
  reporter left. The stands' green glow, the ring's own setting, was turned
  off for the pictures so the picture shows plainly.
- Builds: every build with one job.
- Complete suite: not run for the final state. Work stopped on 2026-10-15
  at the owner's request (weekly usage limit: "commit and Sync, handoff and
  stop"); see Handoff below.

## Handoff - where piece 5 stopped (2026-10-15)

Committed as work in progress, not yet closed ADREP-style.

- Verified on the last full build: `platform-geometry-test`,
  `rendering-import-smoke` (real graphics, 170 s, within its 180 s session),
  and earlier in the piece `panel-window-capability-test`,
  `studio-truth-matrix-smoke`, `icon-scene-test`, `icon-tiles-test`,
  `panel-materials-test`, `panel-registry-test`,
  `theme-asset-processor-test`, `panel-settings-schema-test`.
- The very last change is NOT yet built or checked on real graphics: a
  theme's own mesh (`reliefMesh`) now moves its edges the one way between
  their faces instead of keeping them still, so the octagon (Mesh Platform
  Cyan) gets its relief over its whole top; only `platform-geometry-test`
  ran on it (passed). The owner had marked that octagon with stones as not
  visible enough ("X don't have relief visible enough"); the ring is
  approved in every view.
- Approved by the owner: the ring wearing stones inward (gabion), terrain,
  Mountain and Shell; the edge-on view (panels at pitch 90) for judging.
- Next steps, in order: build with one job; run `rendering-import-smoke`
  and `platform-geometry-test`; the private desktop run
  `/mnt/F/archdock-audit-2026-10-08/tools/picture-relief-steps.sh` (pitch
  90; test pictures in the session scratchpad: stones, terrain, shell,
  mountain, grey - regenerate if gone); show the owner the octagon before
  and after; the Studio pictures (`studio-pictures.sh`, including
  `pictures-panels-appearance-3d` with Relief depth, Raised parts and
  Relief direction); the complete serial suite; then close the piece
  (commit, push, fresh-clone recheck with
  `/mnt/F/archdock-audit-2026-10-08/tools/piece5-recheck.sh`, Report 2).
- Queued after piece 5: the owner's river idea ("the panel is the river,
  with animations"), Phase 4 (real 3D icons and icon animations).

### Questions for the owner

None new. Still open from earlier: a KDE widgets phase; a 3D underside drawn
like the top; removing the Perspective tilt on 2D and baked panels.

### Known limitations

In `docs/KNOWN_LIMITATIONS.md`: a flat panel shows one picture through its
shape while a 3D panel lays the picture round its band, so the two look
different; an imported picture stays in Arch Dock's data folder when it is
removed from a panel; an animated picture shows its first frame; a theme
package's own 3D mesh (the two bundled 3D themes) maps the picture the way
it maps its own surface.
