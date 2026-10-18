# Owner list, piece 5: Import picture for panels and stands (Phase 3)

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
  Appearance), the icons' stands (Pedestals) and the stands of an opened
  folder's items (Pedestals, under Folder item stands); the icon tiles had
  one too until 2026-10-18 (see "Changed after the resume"). It opens a
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
  panel's Look (Panels > Appearance; the row was called Theme until
  2026-10-18), a 3D panel's Look, and the Look of the stands and of the
  folder item stands. It is
  listed only once that part has a picture. Importing puts the picture on at once; a small preview shows
  it; the × beside it removes the picture, and the part gets back the look
  it had before (a Neon panel is Neon again). Once Panel Studio has been
  closed in between, it gets a plain one instead (Glass for the panel, its
  material for a stand, no texture for a tile). As every Studio change,
  nothing is saved before Apply.
- **Flat panels** (until 2026-10-18; since then they repeat it, see "Flat
  panels repeat the picture" below) wore the picture once over their own shape (a ring, star,
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
- **Icon tiles** wore it behind the icon, under a light veil in the
  tile's own colour (`03-tiles-icons-clear-before-after.png`), until the
  owner ruled on 2026-10-18 that tiles take no picture; removed (see
  "Changed after the resume").
- **Icons are never coloured.** A first version also coloured the icons
  with the picture; the owner rejected it ("I never gave u that task at
  all"), and it is gone without a trace. Icon animations and real 3D icons
  are Phase 4.
- **Every look the owner already has is untouched.** Picture is one more
  choice; Neon, Plasma, Lightning, Aurora and the rest draw exactly as
  before (every change in the renderers applies only while a part wears a
  picture).

### Changed after the resume (2026-10-18)

The owner's words on resuming: "The picture and theme are 2 different
things ... the picture gives the panel only it's texture and color with it,
no tiles, not stand's, just texture and relief, while theme can remake
outlines, contours and silhouette of the panels"; "Add the same thing for
Icon tiles and pedestals too, but actually for icon tiles, picture is not
needed, not practicall, colors can be changed and glow and shine, and
transparency and shape and stuff, but there's really no need for picture,
relief or texture"; and on the row called Theme: "This must be fixed too",
"what for now is called theme for flat panels or 3D panel, give it
appropriate name".

- **Icon tiles take no picture.** The Import picture button and the
  Picture choice are gone from the Icon Tiles page, and the tile picture
  code with them (`iconTilePicture`, `IconTile`'s picture and veil). Their
  colours, glow, shine, opacity, shape and existing looks are untouched.
- **Stands in relief.** A 3D stand wearing a picture of its own stands out
  in relief as the panel does: its mesh is cut finely and rises by the
  picture's brightness, with its own Relief depth, Raised parts and Relief
  direction rows on the Pedestals page (and for the folder item stands with
  Own settings), shown only while a 3D stand wears its own picture. A
  stand is small, so its relief is a quarter of its size at the full
  Relief depth, with about ten stones across it, and as a stand is a thin
  plate its stones stand out of it by default (Relief direction Outward).
  The first version set the relief in the stand's own units a tenth deep
  with stones sunk in; the owner crossed it out ("No texture nor relief").
- **Panelpaper and panel decorations.** The owner then named the two
  things: "For pictures that cover panels surface, the name should be
  panelpaper and for contours, outlines and stuff name should be panel
  decorations, in Decorations there should be built in already, which I
  already have, just rearrange properly + import, same goes for
  panelpaper", and "Panelpaper and Panel decorations should be under
  appierance". Panels > Appearance now opens with a Panelpaper section:
  the panelpaper row (the built-in panelpapers, Glass to Pedestal, and
  "Imported picture"; on a 3D panel "Default" is the surface its own
  design gives it) and, only while Imported picture is chosen, the
  Imported picture row with the picture and Import panelpaper… (the owner,
  on seeing the row always there: "those import picture settings appiers
  only when I select imported picture"); choosing Imported picture with no
  picture imported yet opens the chooser at once, and cancelling it puts
  the previous panelpaper back; then the panel's own rows (relief, opacity, the
  look's palettes); then a Panel decorations section with the built-in
  decorations as cards, Flat decorations and Platform decorations (the
  sixteen theme packages that fit the panel, each with Load), and the
  Decoration package actions (Import…, Re-render, Clear); then Glow. The
  Presets page holds presets only, so a profile's panel, which tries no
  presets, has no Presets page any more; its decorations are on
  Appearance. The first naming step of the day, "Look" for the flat row
  and "Own look" for the 3D choice, was superseded within the hour. An
  unset colour reads "Default" (it said "Theme default").
- **Flat panels repeat the picture.** The first flat version covered the
  shape with the picture once, which made a texture's stones huge and
  blurred on a ring (run `piece5-relief-14`); a flat panel now repeats the
  picture across its band at its own proportions, each repeat twice as tall
  as the band is wide, as the 3D band repeats it along, so a texture keeps
  its grain and the relief has edges to shade.
- **Relief on flat panels.** "Some option to change texture or relief,
  like with slide, the more the higher is the relief, the less and the
  lower is the Relief ... that function can be in both 3D and 2D": a flat
  panel wearing a picture now has the Relief depth slider and Raised parts
  too. Its relief is an embossing. The first version only brightened or
  darkened each pixel by its difference to the pixel up and left of it,
  which at panel size was a few percent; the owner, on the flat ring:
  "From top, I really can't say if there's any relief or not". Now the
  painted picture's brightness becomes a height map (through Relief level
  and contrast), each pixel's slope a surface normal, lit from the top
  left with a shine toward the viewer, so a stone's upper edge is lit and
  its lower edge shadowed, Relief depth steepening the slopes; Raised
  parts Dark turns the map round. Relief direction stays a 3D panel's
  choice. The shading first ran on the panel's own canvas, which showed
  nothing on the real desktop (run `piece5-relief-16`; the owner: "U're
  still showing me too view where I can't see any relief"): that canvas
  renders on the graphics card, where reading pixels back gives nothing.
  It now runs once per picture and setting on one repeat of the picture
  in a small software canvas, whose shaded pixels the panel's canvas
  repeats across the band (run `piece5-relief-17`). The owner approved
  that sheet (17), the Studio sheet (16) and the regulators sheet (18)
  together: "Very good, very good".
- **The relief's regulators.** The owner then showed a depth-map relief
  made in Blender from a rock picture, with the settings such tools have
  (height scale, contrast, brightness or midtone, depth cutoff, smoothing,
  invert, and a side view to check): "That's how i want them on my panels
  and pedestals. That's what i mean with real relief"; "imported picture
  should be rendered like that with some extra settings to adjust to users
  needs". The picture already was the depth map (its brightness the
  height), Relief depth the height scale, Raised parts the invert, and the
  edge-on pictures the side check; the mesh is cut finely, so there is no
  terracing. Added, for panels and stands, in 3D and flat: Relief level
  (the brightness that is half height, 0.38 by default, which with Relief
  contrast 0.35 is exactly the climb the owner approved), Relief contrast
  (0 a gentle climb over the whole range, 1 a sharp step) and Relief
  smoothing (a blur of the map). A depth cutoff is the level and contrast
  together: below the climb everything is the flat base. While wiring
  these, the stands' Relief depth, Raised parts and Relief direction were
  found not to reach the 3D scene (the surface loader passed only the
  panel's); they do now.
- **Known methods, not inventions.** The owner: "don't try to reinvent the
  code, there's plenty of that kind of code on internet, plenty of
  opensource programma to take an example from, reuse that material". The
  relief follows them: in 3D it is Blender's Displace modifier (each vertex
  moved along its normal by the texture value minus a midlevel, times a
  strength, on a mesh subdivided finely enough; Blender manual, Modifiers >
  Deform > Displace), with Relief depth the strength and Relief level the
  midpoint of the climb; flat, it is the height-map-to-normal-map method
  of GIMP's normal map filter and its descendants (a Sobel filter over the
  heights gives the slope, normalize(dx, dy, 1 / strength) the normal,
  BumpToNormalMap, MIT), lit with the usual diffuse light and shine. The
  formulas are used, no code is copied, so no licence is carried in.
- **Colour rows hidden under a picture.** A flat panel's Color, the 3D
  Platform colour and a stand's Colour are not shown while that part wears
  a picture, since a picture covers it in its own colours (sheet
  `13-studio-colour-rows-hidden-before-after.png`).
- **Theme octagon in relief over its whole top** (the change left unbuilt
  at the pause), checked on real graphics and shown in sheets 11 and 12.
- **A picture laid on a mesh, one plane per face.** On a mesh not made for
  the picture (a theme's own, a stand's) the picture is laid on each face
  from the side it faces; the side was chosen per point, so the triangles
  round a stand's rim, whose rim points face sideways, were torn between
  two sides and the terrain picture showed a cross of seams over the
  stand's top (run `piece5-relief-12`). The side is now chosen once per
  face when the mesh is cut (`boxFace`, carried in uv1's y) and the shader
  reads it, so a face is never torn; seams stay only where faces meet.
- **Test sessions under KWin 6.8.** KWin and Plasma were upgraded to 6.8.0
  on the evening of 2026-10-15; KWin 6.8 no longer takes its socket name
  from WAYLAND_DISPLAY, so the sixteen isolated Plasma session tests timed
  out waiting for a socket that was never made. `run-plasma-lifecycle.sh`
  now names the socket outright (`--socket`), as the other private
  sessions already did. The rendering import smoke's private session
  allows 210 s (was 180 s, which it reached within 10 s).

### How it works (for the code)

- Settings (`PanelSettingsSchema`): `panelPicture` (`surface.picture`),
  `pedestalPicture` and `folderPedestalPicture`
  (`surface.parameters3D`); `iconTilePicture` existed until 2026-10-18.
  Their normalization `PictureUrl` keeps only a local file URL with an
  absolute path; anything else, a web address among others, reads as no
  picture. "picture" joins the choices of `appearance`, `scene3DTexture`,
  `pedestalTexture` and `folderPedestalTexture`. The
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
  sets the picture, gives stands a Disc shape when they had none and folder
  item stands their own settings, then sets each Look the picture is worn
  as (`pictureLooks`: `appearance` and `scene3DMaterial` for the panel,
  `pedestalMaterial`, `folderPedestalMaterial`) to "picture" wherever the
  panel offers it, with the setting paired with that Look as choosing it in
  its list does (`setLook`: the 3D texture "picture", a stand's texture its
  material). The previous Look is remembered per
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
  picture (an opened folder's stands get it from `folderPedestalSettings`).
  Stands in 3D: `PanelScene3D.pedestalPictureRelief` cuts the stand's mesh
  (`pedestalReliefMesh`, `reliefMesh` with a 6000-triangle limit) and the
  stand models take `standRelief`, a second `PictureRelief` with the stand's
  picture, box-mapped at 0.4 repeats per unit, its depth a quarter of a
  unit at Relief depth 1 (`pedestalPictureDepth`, `pedestalPictureRise`,
  `pedestalPictureDirection`, Outward by default; the folder items' stands
  take theirs through `folderPedestalSettings`).
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
  - `tst_PanelMaterials::test_pictureFillsThePanelOnce`: a flat ring wears
    the left half of the picture on its left and the right half on its right.
  - `tst_IconScene::test_standPictureLook`: a stand's own picture, Picture
    without one falling back to the panel's look, and a stand in the panel's
    look wearing the panel's picture.
  - `PanelWindowCapabilityTest::studioPicturesComeAndGo`: importing through
    Panel Studio puts the copy on as the panel's Look, × gives the Neon
    panel its Neon back; a stand without a shape gets a Disc and Picture as
    its Look, its Colour row is not offered and nor is a relief row on a
    flat panel; the panel's own Color is not offered under its picture and
    is again without it; a file that is no picture is refused with a message and
    changes nothing; nothing is saved before Apply; Apply keeps the stand's
    picture. `studioPagePictures` takes the pages with pictures, scrolled to
    their Picture rows, the 3D panel's Appearance page with the relief rows
    and its Pedestals page with the stands' relief rows.
  - `tst_PlatformGeometry::test_pedestalShapesCutForRelief`: every stand
    shape cut for a relief within 6000 triangles stays a closed solid facing
    outward, every point allowed to move, each face with the one plane its
    picture is laid from.
  - `tst_PanelMaterials::test_pictureReliefOnAFlatPanel`: on a flat ring
    the painted picture changes with Relief depth and again with Raised
    parts Dark.
  - `PanelWindowCapabilityTest`: the decoration cards are found on Panels >
    Appearance (scrolled to, since a card is made only near the visible
    part of the page) with the same checks as before; a profile's panel
    has no Presets page; a flat panel wearing a picture offers Relief
    depth and Raised parts but not Relief direction. `studioPagePictures`
    takes `panels-appearance-decorations.png` and
    `profile-panel-decorations.png` instead of the Presets page's looks.
  - `RendererCapabilityTest` (real graphics): a ring's stand wearing a
    picture has the relief material and its mesh rises (`risesReady`);
    Relief contrast 1 draws the ring's picture differently.
  - `PanelWindowCapabilityTest::studioPicturesComeAndGo`: a flat panel
    wearing a picture offers Relief depth, Raised parts, Relief level,
    Relief contrast and Relief smoothing, not Relief direction; the row for
    importing a picture is on the page only while Imported picture is the
    chosen panelpaper.
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
  - `PanelWindowCapabilityTest::studioPicturesComeAndGo` also checks that a
    stand's Colour row is not offered while Picture is its Look and is
    offered again without the picture; `meshSceneEditorIsGatedAndTransactional`
    that a 3D panel's Platform colour is put away while it wears a picture
    and comes back without it.
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
  - The Studio pictures showed Platform colour and a stand's Colour under a
    Picture look, though a picture is worn in its own colours and the
    colour changes nothing; a flat panel's Color is covered by its picture
    the same way. They are hidden while the part wears a picture (the
    owner's rule: a setting that cannot apply now is not shown).
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
- Private desktop: five picture runs (`piece5-pictures-1` to `-5`), the
  octagon platform and the ring of the owner's copied settings wearing the
  test picture step by step, and eleven relief runs (`piece5-relief-1` to
  `-11`): the first eight on 2026-10-15 steered the relief to the look the
  owner approved (sheets 07 to 10); run 9 (panels at pitch 80) was stopped
  when work paused; runs 10 and 11 on 2026-10-18, on the build with the
  theme mesh's edges moving, show both panels laid flat through the six
  steps (level, stones inward, terrain outward, Mountain with its light
  and with its dark parts raised outward, Shell inward; sheet
  `12-3d-relief-level-six-steps.png`, which the owner approved: "Looks very
  good") and at their own tilt (sheet
  `11-3d-relief-octagon-whole-top-before-after.png`: the octagon's top was
  a painted face with straight edges, now the stones stand over its whole
  top and break its edges up, as on the approved ring). Runs 12 and 13
  (2026-10-18) put the stones and terrain pictures on the icons' stands
  (discs) of both panels: run 12, the first stand relief (a tenth of the
  stand's units, sunk in), which the owner crossed out, with the terrain's
  cross of seams; run 13 after the fix (a quarter of the stand's size,
  standing out, one plane per face), sheet
  `14-3d-stands-relief-before-after.png` with the flat picture (Relief
  depth 0) as the before. Runs 14 and 15 put the stones on the ring drawn
  flat (`rendererTier` procedural2d): run 14 with the first flat version
  (the picture once over the shape: huge, blurred stones), run 15 with the
  picture repeated across the band, at Relief depth 0, 0.8, and 0.8 with
  the dark parts raised (sheet `17-flat-relief-before-after.png`). Run 16
  shows the relief's regulators on the ring laid flat: the defaults, Relief
  contrast 1, Relief level 0.6 and Relief smoothing 1 (sheet
  `18-3d-relief-regulators.png`). Owner files untouched after every run;
  no crash reporter left. The stands' green glow, the ring's own setting, was turned
  off for the pictures so the picture shows plainly.
- Panel Studio pictures (`after-kde/`, a private KWin with the KDE platform
  theme): retaken on 2026-10-18 on the final build; `panels-appearance.png`
  shows the flat panel's row called Look, `pictures-panels-appearance.png`
  a flat panel wearing a picture without its Color row,
  `pictures-panels-appearance-3d.png` the Picture look with Relief depth,
  Raised parts and Relief direction and no Platform colour,
  `pictures-pedestals-3d.png` a 3D panel's stands wearing a picture with
  their relief rows and no Colour row, `panels-appearance-decorations.png`
  the Appearance page scrolled to the decoration cards (sheets 13, 15 and
  16, the last on the final build with the six relief rows). The Icon
  Tiles pictures with a picture and the Presets page's looks pictures are
  superseded.
- Builds: every build with one job.
- Complete suite, serially, on the final build (2026-10-18, 2489 s): 114
  of 121 passed in the run; the seven that failed, and what became of
  them:
  - `preset-library-test` and `preset-staged-preview-smoke` (the same
    check): a built-in preset card drew differently twice. The test spies
    on the first canvas it finds under the flat surface, and the flat
    relief's small helper canvas was declared before the surface's own, so
    it waited for the wrong one. The test now prefers the surface canvas
    by name and the helper canvas is declared after it; both pass.
  - `preset-audition-matrix-defaults`: every phase passed, then stopping
    the private service timed out: its last phase crashes the service and
    lets D-Bus start it anew, and the runner stopped the process it had
    remembered. The runner now stops the process that owns the name; it
    passes (it had failed this way four times today and is the "known
    restart race" of the earlier suites).
  - `wayland-hardening-scale100/125/150/200-test`: the private Plasma
    session lost its compositor or had no containment yet at start ("The
    Wayland connection broke", "containment-missing"), a startup race of
    the isolated session under KWin 6.8; each passed when run again, and
    scale150 then failed once more and passed on the next run.
  - The first complete run of the day (before the Studio and relief work
    that followed) had the sixteen isolated-session tests fail for the
    KWin 6.8 socket name; all passed after the runner fix, with the two
    flakes above. Every test has passed on this build; the fresh-clone
    recheck runs the focused list once more.

## Closing (2026-10-18)

Work paused on 2026-10-15 at the owner's request (weekly usage limit:
"commit and Sync, handoff and stop"), committed as work in progress
(`c305300`) with a handoff in this file. Resumed on 2026-10-18 from that
handoff: the last change (a theme's own mesh moves its edges the one way
between their faces, so the octagon gets its relief over its whole top)
was built with one job and checked on real graphics
(`rendering-import-smoke`, 171 s, within its 180 s session;
`platform-geometry-test`); the private desktop runs 10 and 11 above showed
the octagon before and after to the owner; the Studio pictures were
retaken; the Platform colour and stand Colour rows were hidden under a
picture; then the complete suite, the commit and the fresh-clone recheck
(Report 2).

Queued after piece 5: the owner's river idea ("the panel is the river,
with animations"), then Phase 4 (real 3D icons and icon animations).

### Questions for the owner

- Do the stands in relief read well now (sheet 14, second version: a
  quarter of the stand's size deep, stones standing out)? If not, say
  deeper, bigger stones, or sunk in, and it is a number to change. (The
  flat ring, the Studio and the regulators were approved: "Very good,
  very good".)
- The stands' rows still say "Look" and "Imported picture" with "Import
  picture…"; should a stand's surface be called panelpaper as well?
- Still open from earlier: a KDE widgets phase; a 3D underside drawn like
  the top; removing the Perspective tilt on 2D and baked panels.

### Known limitations

In `docs/KNOWN_LIMITATIONS.md`: a flat panel shows one picture through its
shape while a 3D panel lays the picture round its band, so the two look
different; an imported picture stays in Arch Dock's data folder when it is
removed from a panel; an animated picture shows its first frame; a theme
package's own 3D mesh (the two bundled 3D themes) gets the picture laid on
each face from the side it faces, with a seam where faces meet.

## Report 2 - recheck

- A fresh clone of origin/main at `bd8e7f0`, configured and built with one
  job (1550 s, `41`/`42-recheck-*.log`), passed the 21 focused tests
  (`43-recheck-tests.log`): panel-model, profile-store,
  theme-asset-processor, panel-settings-schema, panel-settings-transaction,
  panel-registry, panel-window-capability (431 s), icon-tiles,
  platform-geometry, panel-materials, live-panel-preview, icon-scene,
  rendering-import-smoke (174 s, real graphics), studio-truth-matrix-smoke
  (391 s), runtime-ui-interaction-smoke, studio-preview-contract,
  profile-matrix-apply, studio-navigation, settings-editor-model,
  configuration-upgrade and session-startup-runtime (the last two session
  tests under the KWin 6.8 runner fix).
- All 672 committed files are the same in the clone and in the workspace
  (`44-recheck-source-compare.txt`).
- No crash reporter from a test session was left (the launchers seen
  during the day were the owner's own file indexer's).

Piece 5 is closed. Next on the list: the river look (the owner: "the panel
is the river, with animations"), then Phase 4, real 3D icons with their
animations. A package can be built at any point the owner asks for one.

