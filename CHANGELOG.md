# Changelog

## 0.1.1 candidate — unreleased, 2026-10-05

Application version: `0.1.1`; the current Arch package candidate and verification
status are recorded in [current state](docs/CURRENT_STATE.md).
The owner selected GPL-3.0-or-later for original Arch Dock project work.
The proposed `v0.1.1` tag and publication need separate authorization.

- Panel Studio follows the panel you pick, and no longer crashes on it
  (2026-10-13, the owner's report "no matter which panel I click, it's
  always same free panel"): picking the Top or Side panel while a 3D free
  panel was previewed killed Arch Dock, and the Studio came back on the
  same free panel. The 3D preview's icon textures now let go of the icons
  as soon as the scene or an entry is told it is going, before Qt Quick 3D
  can refresh a freed layer. The button beside the panel list carries the
  picked panel's name and adds that panel to the desktop (a hidden edge
  panel is shown, a free panel whose widget is gone is put back, a panel
  already there shows a check mark) instead of adding a new empty free
  panel each time; a refused switch shows the panel that stays. The
  preview card holds only the panel: its orientation, icon-state and
  open/collapsed controls are gone, and an edge panel with nothing on it is
  drawn bare instead of not at all. A flat panel's material runs once along
  the whole panel, as one continuous colour, instead of repeating in
  sections (a ring mirrors it so its ends meet without a seam). Settings
  pages keep their rows when a value changes: the page no longer jumps to
  its top and the control in use is no longer rebuilt under the pointer
  (a drag low on the page was lost), and a slider's drag checks the draft
  once it pauses instead of on every step.
- Stands face the viewer, new panels have none, and the Pedestals page
  has far more to choose (2026-10-12 night, the owner's report with five
  screenshots and "the pedestals, plates and other stuff should be
  orientated properly, horizontal not flat from my perspective, and items
  on them centered while floating on top of them", "positioned right
  accordingly to the panel's position in 3D and 2D space", "why are they
  outside the dock"): a stand stands on the panel at its icon's place, on a
  free panel's path and on a 3D platform's band, the icon centred over it
  and floating its Float height above the stand's top. On a 3D panel, Stand
  facing turns the stands: Panel, the default (the owner's picture of it,
  2026-10-13: as if the stands were one more layer on the panel, in
  pieces), fixes each stand to the panel at its icon's place, lying on a
  3D ring and carried by its every tilt, turn, move and rotation, and drawn
  in a flat panel's own plane; Auto lies them on a panel tilted back
  and keeps them level on one seen face-on; Viewer keeps them level and
  facing the viewer; View angle is how far from above a level stand is
  seen. Stand and icon move toward the
  viewer just enough never to sink into the platform at any camera angle,
  and a flat free panel grows to hold an icon raised over its stand. Flat
  panels and opened folders paint the same shapes, shaded and textured, an
  item's name below its stand. A new panel has no stands (Auto stays a
  column). Twenty shapes; Size and Float height reach further;
  Thickness, Texture, Opacity and Glow are new; Sway, Wobble, Glow and
  Float motions; Folder item stands gain Own settings with their own size,
  thickness, float height, material, texture, colour, opacity, glow and
  motion. The 3D page's Pedestal height shows only where it draws
  something. The Profiles page counts the panels a profile shows, not the
  hidden built-in edge panels every arrangement keeps.
- Profiles hold only the panels on the desktop (2026-10-12, owner's report
  "with just one apply it kept on opening multiple docks and nearly crashed
  my PC"): a profile had captured every free panel record in the registry,
  21 of them long removed from the desktop and kept only for "Put back on
  the desktop", and one Apply created 13 widgets before the backend was
  killed. Create and Save now capture native panels and only the free panels
  that are linked to a widget on the desktop. The Profiles page says how many free
  panels an Apply would create, and Apply refuses to create more than four at
  once, in plain words, with the way out (put the wanted panels on the
  desktop, then Save Current to Profile). A profile that was saved before
  this change still holds the dead records until it is saved again.
- Profiles, presets and re-hosting on a desktop with panels (2026-10-12,
  owner's report "the panel shows up for a second and disappears"): Plasma
  keeps a free panel's widget out of a panel's own area and on the screen,
  so the widget settled lower than the saved position and the strict
  geometry read-back rolled the whole profile apply back; the read-back
  now accepts the position Plasma chose once the size is right and the
  widget holds still, and logs the difference. The backend also no longer
  detaches a free panel whose widget it cannot find during its first
  ninety seconds; it checks again after 15 s, since plasmashell may still
  be restoring its layout after a login or a restart (that is how a free
  panel was lost on 2026-10-12).
- The owner's afternoon report (2026-10-12, six screenshots): true 3D now
  defaults to **high** quality (full size, supersampled; medium is full size
  with multisampling, low three quarters without) and icon glyph textures
  are four times the icon's size, against pixelly platforms and stands. An
  icon's in-plane turn (Spin, Wobble, Swing, Shake) in 3D turns the icon
  about the vertical axis, the way a stand spins, instead of rolling the
  picture. The Pedestals page gains **Folder item stands** (the panel's own
  shape, none, or a shape of their own) for the items of an opened folder.
  A stack folder lies against the dock's side nearest the folder, upright
  or level, centred on the dock and no longer than it (the rest scroll
  in). A ring folder opens beside the dock on the side with more of the
  screen, from the dock's rim there, lowered or raised to stay on the
  screen. A turned stand's flat top takes its texture flat, so a disc in a
  swirling material no longer looks like a ring, and on a bent band a
  stand rises by the band's rise under its footprint instead of sinking
  into the rim.
- Flat design kept in 3D (2026-10-12, owner's report "turning 3D on changes
  everything"): a look designed flat and switched to true 3D keeps its ring.
  Platform width 0 (Auto, the new default) makes the generated platform's
  band as wide as the flat look's line at the panel's radius, and as deep in
  proportion; Material and Texture on Theme take the flat look's own surface
  (translucency, metal, glow, texture) even for a look with a sampled
  palette. Colour, layout, tiles and opacity were already kept. Panel Studio
  says so on Panels > 3D; the Pedestals section gets its own icon in the
  side list (Profiles had lost its own).
- Pedestals, Auto on a flat panel (2026-10-12, owner's report "none of the
  pedestal settings work"): Auto now draws a column stand on flat panels as
  it does in 3D (an icon style with its own pedestal keeps it), so the
  page's size, lift, material, colour and animation apply at once instead
  of waiting for a shape to be chosen; None still removes the stand.
- Edge panels (2026-10-12, owner's report "they don't work",
  [audit record](docs/audits/FULL_AUDIT_2026-10-11.md)): a collapsed edge
  panel's reveal strip now lies on the panel's own edge inside its window;
  it was placed at the outer edge of the thicker scene behind the panel,
  outside the window, so with the Edge trigger a collapsed panel could never
  be opened again. Panel Studio's General page gains **Add applications…**
  for launcher and hybrid panels (an edge panel had no way to get launchers
  but a drop), and an empty panel says where to add them. Studio notes
  Plasma's same-edge rule (a panel is drawn over an Auto-hide or Dodge
  windows panel, stacked above an Always visible one); INSTALL.md explains
  edge panels' content, placement and limits.
- Pedestals (2026-10-12, owner's request): a Panel Studio page of their own
  for the stands under the icons, on flat and 3D panels alike. Shape (Auto,
  None, Column, Plate, Disc, Ring, Saucer), Size, Lift (how high the stand
  floats between the panel and the icon), Material (the panel's own or any
  of the thirteen looks), Colour, Animation (None, Bob, Spin, Pulse) and
  Animation speed. In 3D the shapes are turned solids with the look's
  texture and relief; on a flat panel they are horizontal platforms painted
  in the look's texture under the raised icon. A stand travels with its
  icon; its animation and the icon's stay separate. Auto keeps each
  renderer's own pedestal. The icon tile treats a texture already in the
  image cache as loaded.
- Full audit corrections (2026-10-11, [audit record](docs/audits/FULL_AUDIT_2026-10-11.md)):
  a free panel whose desktop widget is gone is shown as missing in Panel
  Studio and can be put back on the desktop with its saved settings and
  position (`rehostFreePanel`); the backend logs a warning when it notices a
  widget is gone; Studio says when a shown edge panel is auto-hide; the
  widget-list "Arch Dock" entry explains where panels are created; INSTALL.md
  gains "Panels on the desktop"; the control widget's tooltip shows again.
- The dock widget declares no compact-switch sizes any more: Plasma built
  every dock twice (compact and full representation, one invisible) for
  widgets below those sizes, with two 3D scenes and two sets of textures.
- A 3D scene that has drawn once stays ready while a material or texture file
  is replaced, instead of dropping to the flat renderer for the load time.
- "Along the dock" folder contents on a 3D panel follow the drawn ring: the
  curve is the projected icon track moved outward on screen, and it widens
  round the dock while a shorter curve would hold only some of the children.
- 3D hit tests are memoised and allocation-free; the 2D scene reuses each
  entry's own geometry for hit tests; the 3D "frame rendered" flag no longer
  re-evaluates on every frame.
- Folder shapes on a 3D panel (2026-10-12): a fan opens from the platform's
  rim beside the folder, an arc and a ring are centred there, and a stack
  lies along the rim beside the folder, running the way that has more of the
  screen; all follow the platform's plane as drawn. A fan's two straight
  edges are only its faint fill's border; the stroke follows the arc.
- Real textures (2026-10-12): the thirteen material looks are 512 px raster
  textures that depict their names (grass for Organic, flowing plasma for
  Plasma, marble for Pedestal, a lime's peel for Lime, brushed metal,
  riveted plate, stone, frost, circuits, tubes), generated deterministically
  from the vector swatches by `tools/generate-material-textures.py --raster`;
  the flat looks, icon tiles and the 3D platform load them. The texture
  wraps round the panel: a generated 3D platform maps it round its band on
  every face, inner wall, top and outer wall, with whole repeats along the
  ring, and a flat panel paints it along its track, turned to the track's
  direction, so a ring closes without a seam. In 3D each look also has a
  relief map (`<look>-normal.png`, from the texture's brightness) that the
  scene lights as a real surface on the platform and the pedestals, the
  energy looks glow through their texture and flow round the band. Theme
  packages keep their vector surfaces.
- ADREP-TASK-005: all six icon styles follow Rounded, Square, Squircle,
  Circle, Hexagon and Diamond through package declarations, retaining their
  signature defaults. Diameter, Logo size, outline and colours are saved beside
  Style. Real application glyphs use continuous sizing and measured share and
  graphical contrast; Neon frames preserve the original glyph colour.
- Dark Orb has no pedestal by default. Its optional pedestal has saved height
  and colour; Blue Pedestal and Red Pedestal revision 2 enable it.
- Icon Presets carry a bounded appearance block and My Icon Presets retain it
  after reload. Applying a preset restores its complete icon/tile defaults
  while preserving panel geometry, content, icon size, spacing and ownership.
- Tiles retain custom shape/fill/border/opacity and add thirteen textures,
  thickness and glyph offsets/scale. Native 3D uses closed tile solids with
  bevel, material and elevation. Tiles travel with upright glyphs during orbit;
  the reset action preserves the existing custom-tile values.
- Native panel recovery can receive a committed application/folder drop while
  it awaits Plasma's geometry reply. Recovery dispatches incoming backend
  calls and guards overlapping/stale passes; drop delivery retains its
  synchronous result and existing deadline.
- Background native placement stops when newer panel intent commits during
  its Plasma call. An obsolete pass preserves the newer position and result
  instead of rolling its old host snapshot over the foreground change.
- Native 3D frame-projection updates belong to the scene and stop during
  replacement. Static scenes settle without continuously requesting frames;
  input, tile rendering and upright path travel retain their existing checks.

- The official source verifier regenerates all four canonical export artifacts
  and rejects any byte difference, including recipe commands/metadata, archive
  metadata/member ordering and manually repaired checksum receipts.
- Tracked source modes follow Git's `100644`/`100755` executable intent,
  producing `0644`/`0755` independently of checkout permission bits.
- The complete official GPLv3 text and GPL-3.0-or-later declaration are added.
  The package ships the GPL text, preserved MIT component notices and a license
  matrix; audited original asset declarations carry the owner's new selection.
- Existing MIT components retain their license. Unknown-rights reference
  material remains NOASSERTION, non-installable and excluded from distribution.
- Icon mask load failures follow their asset URL. Switching the same icon scene
  from a broken mask to a valid mask or a plain style recovers; a currently
  broken mask still uses the original glyph fallback.
- Folder contents unfold from the clicked dock icon on a transparent native
  Plasma surface. Dense fan and grid layouts fit horizontally and scroll with
  the wheel or a held-pointer drag; dragging does not open a child.
- Fan and Arc folder contents move along a compact curve with wheel or held
  dragging, without visible scrollbars or selection boxes. Reduced motion
  remains immediate. Free-panel popups choose the side with more screen space
  and fit that space before native Wayland placement.
- Appearance explains the current 2D surface and offers a route to 3D themes;
  compatible themes retain the existing 3D switch, quality and tilt controls.
- 3D icons sit on the platform in world coordinates. Rotation changes their
  perspective, depth and input bounds. Free-panel surfaces support held-pointer
  rotation as well as wheel and continuous clockwise/counterclockwise rotation.
- Orange Arc includes a volumetric ring option. Enabling 3D switches its draft
  to a circular path; the existing baked artwork remains available.
- 3D orientation, platform thickness and pedestal height join the tilt and
  layout size controls. Opacity is available for themed surfaces. A loaded
  baked surface no longer draws the procedural fallback arc over its artwork.
- Switching 3D off retains backend validation while omitting untouched
  controls exposed only by the 3D draft. Orange On/Off and Cancel are verified.
- Panels > Behavior can keep folder item names visible without hovering.
  This preference uses the existing draft, Apply/Cancel and persistence path.
- Baked 2.5D platform artwork fills the platform its icons stand on at every
  layout radius. The artwork was decoded at the drawn size but cropped in
  natural pixels, which drew a ring shrunk into the corner of its layer while
  the icons kept the full-size track. Icons also stand on the drawn track: the
  track's scene offset is applied once, not twice.
- Spacing regulates separation along curved tracks (circle, ring, radial,
  polygon, arc, semicircle, fan and baked platforms) in every renderer. At or
  above the reference spacing of 8 the entries are spread evenly; below it they
  draw together about the middle of the track, down to touching at zero.
- An open curve with more entries than fit shows the entries that stand on it
  at the configured spacing and moves the others along the curve with the
  wheel, one entry per notch in both directions. Order is kept and no straight
  tail is appended; closed rings keep turning. The wheel works on the bare
  surface of a platform as well as on its icons.
- True-3D icons stand on the platform's own track for every layout shape, so a
  square or polygon path no longer places icons over the platform's hole.
- Fan and Arc folder contents stand on an exact half circle that opens away
  from the dock, or a half ellipse when the popup is short. A larger folder shows the
  children that fit and moves the others along the same curve by wheel, held
  drag or keys, one child per wheel notch; a small folder uses a smaller circle.
- Theme cards resolve each theme with its own renderer tier through the same
  candidate Load applies. A theme whose style names no tier adopts its declared
  preferred tier, so a 2D theme can be loaded over a platform theme. Cards are
  drawn in a fixed context, without the selected panel's angle, rotation, tilt,
  collapse or radius, at the height of a preset card.
- A free panel with a ring, circle or polygon layout can be drawn in 3D
  without changing its theme. A look that ships no 3D scene of its own stands
  on the generic 3D platform, coloured by the look itself; turning 3D off
  returns the same theme in its own 2D or baked 2.5D renderer. Arcs and
  semicircles stay flat unless their theme brings its own 3D platform.
- Panel Studio has one Panels > 3D page for every 3D setting: Enable 3D,
  pitch, yaw and roll, scene position X, Y and Z inside the panel, scale, field
  of view, platform thickness, pedestal height, quality, key and fill light,
  animated orientation changes, a gentle float that is off by default, the
  same Spacing control, Reset 3D transform and Edit on desktop. Appearance
  points to it instead of to 3D themes.
- Edit on desktop shows Blender-style move, rotate and scale handles on the
  panel itself. Ctrl snaps, Shift is fine, Esc or the right button cancels a
  drag. Each finished drag joins a desktop draft; Apply as Active saves it in
  one transaction, Cancel restores the panel, and an interrupted edit is
  recovered. While editing, the panel takes no application presses.
- Reduced motion turns the 3D float and the animated orientation changes off.
- Panel Studio and Icon Properties open on the screen of the panel they edit;
  without one they open under the pointer, and on the primary screen only as a
  last resort.
- Preset cards and Panel Studio messages give reasons in plain language and
  name renderers as 2D, baked 2.5D or 3D. The internal code stays available on
  hover for reports. Studio messages use the footer's width on two lines, show
  the whole text on hover and can be copied. The title-bar close button has an
  accessible name and tooltip, and a long panel name no longer runs under the
  title-bar controls.
- On a curved layout the Spacing control explains its two ranges.
- A scene that switches to or from independent segments no longer logs type
  errors for one evaluation.
- The rendering gate's resource check measures what repeated theme changes add
  after a warm-up pass, from settled readings, with its limit unchanged. Its
  earlier single reading included the first load of every family and transient
  peaks the shell releases seconds later, so it failed at random.
- The repository no longer tracks the stale `build-codex-task-0014/` build tree,
  the master plan's three requirement-mapping files are restored under
  `docs/task-pack-v3/`, and a GitHub workflow runs the source-only gates.
- Current-facing installation/version wording follows the corrected candidate;
  historical package checkpoints and release assets retain their identities.
- **Quit Arch Dock** (an action of the application entry, or `arch-dock --quit`)
  now stops Arch Dock for the rest of the login session. Installed panels and
  the KWin window watcher no longer start it again on demand; an on-demand
  start during the stop is refused at once. Starting Arch Dock yourself (the
  menu, `arch-dock`, Panel Studio, the user service) clears the stop. TERM and
  INT stop it like Quit; a killed process is a crash and restarts on demand.
- A folder's contents open from the clicked folder, away from the dock. A
  native panel's folder opens away from its screen edge; a free panel's folder
  opens the way it faces out of the dock and turns to the other side only when
  the screen leaves no room. Ring and stack contents begin at the folder.
- New folder layout **Along the dock** for curved free panels, the default for
  new curved free panels: the folder's contents stand on an invisible track
  just outside the dock that follows its curve, tilt and perspective, unfold
  from the folder, and move along the track with the wheel or the arrow keys
  when they do not all fit. Existing panels keep their layout; choose it in
  Panel Studio under **Panels > Behavior > Folder layout**. The other five
  layouts stay available as popups, and straight edge panels keep the popup.
- A wide folder popup whose span covers the middle of the screen stays on its
  folder. Plasma moves such applet popups to the middle of the screen when
  they are attached to a narrow point; the popup is now attached along a
  stretch longer than itself, centred where it belongs.
- The wheel keeps turning a free panel while an icon's window preview is
  shown; turning the panel closes the preview.
- Panel Studio no longer reloads its whole editor for each backend change:
  changes that arrive together reload it once, and an unchanged panel is not
  reloaded. Those reloads stalled the dock for about a second during drops.
- Every opening mechanism Panel Studio offers now visibly closes and reopens
  the panel. A drawn ring or bar is squeezed onto its handle along the chosen
  axis instead of staying full size behind hidden icons, and a 3D platform
  closes toward its centre like an iris, down to a small ring, while its
  theme's own parts move. Mechanisms a panel's renderer cannot draw are not
  offered and are refused.
- 3D draws a panel's own look. A look without a 3D platform of its own (the
  baked Blue ring and Steel octagon, and every procedurally drawn look) stands
  on a platform generated along the panel's own layout (a circle, an ellipse,
  a triangle to octagon or other regular polygon, or the 300-degree radial
  arc) in the look's own colours, read from its artwork, with its rim glowing
  in the look's glow colour. It no longer borrows the Cyan theme's octagonal
  mesh and recolours it. Arcs, semicircles and fans stay flat in 3D.
- Orange Arc's own 3D platform is drawn in the dark copper of its 2D artwork
  instead of peach, and its pedestals and icon collars take the same colours.
- Panels > 3D has a Shape section: the panel's shape among those 3D draws
  exactly, the platform's width and a bend that tilts its top outward up or
  down. A theme with its own 3D platform keeps the shape it brings, and the
  page says so.
- 3D icons stand upright and face you at their real size, on solid pedestals
  in the platform's colour. They no longer lie flat inside dark rings or float
  away from short pedestals, and an icon's click area no longer grows and
  shrinks with its hover animation.
- In **Edit on desktop**, dragging the platform itself tilts it (up and down)
  and turns it (left and right); Ctrl snaps to 15 degrees and Shift is fine.
  The move, rotate and scale handles remain. An arrow that points straight at
  you moves with an up or down drag instead of doing nothing, and the editor
  says so.
- When 3D is not available, the 3D page names the actual reason: the kind of
  panel, the renderer, imported artwork, a flat skin, missing resources or the
  layout.
- Panel Studio and Apply are much lighter. Every edit used to read the panel's
  theme package from disk again, once for each 3D setting (about 70 reads per
  edit, each hashing every asset and parsing the 3D mesh). A package is now
  read once while its manifest and every asset it was verified from are
  unchanged, and an edit builds the theme's projection once. Measured on a free Orange 3D panel (Debug build, median):
  a Studio edit 792 → 44 ms, opening the Studio page 305 → 34 ms, Apply
  495 → 20 ms, the live panel's renderer configuration 39 → 4 ms.
- A theme package that changes on disk is verified again. The package cache
  used to trust a package while only its manifest was unchanged, so an asset
  rewritten, removed or turned into a link out of the package could still be
  served as verified. It now also checks every asset's file identity, size and
  times (nanoseconds) before reusing a package, and keeps a package only once
  its files have settled. An unchanged package is still read once.
- Arch Dock has a new icon and logo. It is installed in every standard size as
  `org.archdock.ArchDock` and used by the menu entry, both applets, Arch Dock's
  windows and the README.
- For contributors: source files say what they are for, the backend's largest
  file (`PanelWindow.cpp`, about 8,300 lines) is split by topic into eight
  files with every function moved unchanged (`PanelWindow.h` lists them), and
  two unused QML helpers (`MotionPolicy.js`, `StudioDraft.js`) and their tests
  are removed.
- Closing Panel Studio releases it. The closed window used to stay loaded with
  its animations running: after Studio had been opened and closed, the
  background service on the owner's PC held about 689 MB and kept using about
  2% of a CPU core. An open, idle Studio uses about 7%. Closing it now returns
  the service to its idle CPU use.
- Moving or resizing a window no longer makes the dock reload. Every frame of
  a window drag used to rebuild the dock and send all entries to every panel
  again: about 6 ms per frame in the installed Release package, and 600
  reloads for a 5-second drag. The dock now reacts only to what it shows. In
  the same Debug build a frame went from 8.6 ms to 0.4 ms, with no reloads.
- Clicking a pinned application starts it through KDE's launcher, as New
  Instance and desktop actions already did. Terminal=, Path= and D-Bus
  activation are honoured. The program gets its own systemd scope, so it no
  longer belongs to Arch Dock's service, and a desktop file KDE does not trust
  is refused: a downloaded `.desktop` file that is not marked executable used
  to run its command on click. The dock shows "Application started" or "Launch
  failed" from the real result.
- Desktop files are read by KDE's Desktop Entry rules. A name with a comma
  showed as empty and a semicolon cut it short; translated names are now used.
- Names are shown exactly as typed in dock and folder tooltips and in
  right-click actions. Markup in a name (a window title, file name or desktop
  file) was read as rich text, so a name containing an image tag made the dock
  fetch that image from the network when the tooltip or menu appeared.
- A hidden skinned 2D panel stops its glow animation, like the other renderers.
- Each panel's entries are 44% smaller (135 KB to 76 KB for nine entries): the
  icon style definition is no longer sent twice for every entry.
- The [full audit of 2026-10-06](docs/audits/FULL_AUDIT_2026-10-06.md) records
  these findings, the measurements and what remains open.
- Opening Arch Dock from the application menu always shows Panel Studio:
  when Arch Dock is stopped, after Quit, while it runs and after Panel Studio
  was closed. Background starts still never open it (ADREP-TASK-001).
- Panel Studio offers a setting only where it changes the panel, and hides a
  tab with nothing to change. Free panels lose Alignment, Dynamic, Edge,
  Visibility, Width, Height, Indicators and opening and closing; edge panels
  lose Dock layout, Layout scale and Panel padding, which their applet never
  used. Appearance > Shape, drawn by nothing, is gone. A setting the panel does
  not have is refused if changed through D-Bus; one it has that draws nothing
  in its present state is hidden and keeps its value.
- Opening and closing, and panel rotation, are on Animations only. The icon
  style is chosen in one place, Icons > Appearance; the Icon Styles tab is gone.
- Built-in and own panel presets list only the presets made for the selected
  panel: free panels see free-panel presets, edge panels those of their
  orientation. The tabs for your own presets appear once they hold one.
- Free panels saved collapsed or hidden are shown open after the upgrade, with
  an automatic backup first; nothing could collapse or hide a free panel.
- Edge panels judge a theme by the row they draw, so a theme made for
  horizontal panels applies to a bottom panel whatever its old stored layout.
- Icon path orientation now turns the icons of a free panel; it did nothing
  there.
- A Studio truth matrix test draws 21 panels, the way the applet does, and
  checks every shown setting against the drawn result or the runtime value it
  drives.
- Scrolling over a free panel with a curved layout moves its icons along the
  panel's own outline while the panel stays still (ADREP-TASK-002): one notch
  moves every icon one place, eased in within 120 ms, and a fast spin queues
  nothing. On circles, ellipses, polygons and stars the icons go round; on
  fans, arcs, semicircles, radial paths and spirals an icon that leaves one end
  fades out and comes back at the other, even when every icon fits. Dragging
  the panel moves them too. Edge panels are unchanged.
- Animations gains Continuous motion moves (Items along the path, Whole panel
  or Both), Item travel speed and Scroll sensitivity (a quarter of a place to
  four places per notch, counted the same way for wheels, high-resolution
  wheels and touchpads); the wheel and dragging move what continuous motion
  moves. A panel that rotated keeps turning as a whole after the upgrade, with
  a backup first; any other moves its items.
- A whole flat panel that turns no longer shows its outline behind its
  icons: the outline was painted again 74 to 143 ms after the wheel event,
  while the icons moved at once; it is now turned as one picture.
- Fan, arc, semicircle and radial panels offer Direction (Up, Down, Left,
  Right) on Layout, beside the fine Layout angle.
- A spiral panel's line is drawn where its icons stand.
- A free panel's folders open in the shapes the owner defined
  (ADREP-TASK-003): Fan is a slice-shaped small panel in the panel's look, its
  point at the folder and the children on its arc (Fan opening 40 to 160
  degrees); Stack is a straight line out from the folder (Stack length 2 to
  12); Arc stands the children at one distance from the folder, facing it;
  Ring is a second circle beside the folder (Ring size Small or Same as panel).
  Each opens along the way the folder faces out of the dock and keeps clear of
  the dock's icons; the three settings appear only for their own layout.
- Folder scrolling moves one child, or one Grid row, per wheel notch,
  however finely the wheel or touchpad reports it; along a path a child that
  leaves one end comes back at the other. On free panels with a curved layout
  Scroll sensitivity scales it. Along the dock moved one child per wheel event
  before, far too fast; Grid moved 60 pixels a notch, less than a row.
- Folder easing gives four different motions, opening and closing: spring is
  now a damped spring, not the same curve as outElastic, and a folder folds
  back into its icon when it closes. Reduced motion still opens and closes
  folders at once. Edge-panel folders keep their popup shapes.
- Runtime corrections and candidate gates are recorded in
  [the release checklist](docs/RELEASE_CHECKLIST.md) and
  [corrective report](docs/POST_TASK_0045_CORRECTIVE_REPORT.md).
- Task 4 (2026-10-08): thirteen original bundled panel textures preserve
  colour tint and opacity. Glass is frosted/highlighted, Floating Glass adds
  elevation, Futuristic has seams/light lines, and sparkle defaults to zero.
  Static materials have no periodic paint timer; visible energy animates at
  8 Hz and stops while hidden.
- Flat and platform looks are grouped separately. Platform presentation off
  restores the complete previous flat look. Native platforms expose live
  colour/material/texture, own-material Reset and supported numerical Bend.
- Perspective body/rim masks and native triangle picks hide covered rear glyphs
  and route pointer/drop input to exposed parts. Wheel and continuous travel
  keep running behind the platform and back in front.
- Qt 6.12 verification adds public Camera projection mapping and native wheel
  event-source routing. Matching bounded baked pixmaps are shared, pixel vars
  clear on disposal, and KDE's single-shot D-Bus callback API releases the
  completed reply chain while retaining current conversion/error behavior.
- Studio's immutable descriptor index meets the original latency limits:
  37.46/32.14/12.05 ms medians for edit/page/Apply. Static and hidden animated
  materials measured 0.0% CPU; audition/cancel resource growth is 11.10 MiB
  with zero stale hosts and 0.0% backend idle CPU. Theme switching stays below
  the unchanged 128 MiB bound (53.66 MiB in the full run).
- Complete final-source verification: **118/118 serial CTests, zero failures,
  1607.15 s**, including 24 native renderer cases, all 90 folder anchors,
  Studio truth, real runtime input and recovery/resource gates. Formal
  [Reports 1 and 2](docs/repairs/ADREP-TASK-004.md) record internal Qt skips,
  retained diagnosis and physical limits. The pushed-source fresh clone built
  with one job and passed 33/33 required focused/native/resource tests in
  1173.30 s; all 550 pinned source contents match. Round 1 needed no repair.
  Task 5 packaging and installation have not started; the installed baseline
  remains 0.1.1-12.

## 0.1.0 candidate — 2026-10-03

Historical record: the source/license state below predates the 0.1.1 selection.
The existing tag and six current prerelease assets remain unchanged.

Application version: `0.1.0`; Arch package candidate: `0.1.0-2`.
Annotated `v0.1.0` exists at `c3b3a0b7771b313c45f843f49a503b45b0d1ada0`.
The [GitHub prerelease candidate](https://github.com/aivars11232/Arch-Dock/releases/tag/v0.1.0)
was published on 2026-10-03; it is not a final/stable/latest release.
Physical acceptance remains incomplete and project-wide licensing is unselected.
The tag-matched artifact correction and acceptance are tracked in
[the release checklist](docs/RELEASE_CHECKLIST.md).

- Managed native Plasma and desktop-hosted free panels with ownership-checked
  creation, hide/show, recovery, placement, visibility and removal.
- A shared schema and renderer for native configuration, Panel Studio,
  embedded preview and live applets, with capability-driven controls.
- Procedural/skinned 2D, production chassis and energy themes, icon styles,
  per-icon overrides, baked ring/arc/polygon artwork, and optional true 3D
  with fallback and reduced-motion/resource limits.
- Requested icon motions, panel presentation mechanisms, transformed input
  geometry, window actions/previews, folders, segments, overlays and status.
- Exactly 15 immutable built-in Panel Presets and 15 immutable Icon Presets,
  separate rendered browsers, reusable custom derivatives and future defaults.
- Journaled desktop audition with exact Apply, Cancel/Revert, custom-save,
  default and interrupted-session recovery semantics.
- Persistent profile import/export and transactional multi-panel apply,
  rollback/recovery, plus opt-in KDE global shortcuts with conflict reporting.
- Direct D-Bus startup, a single backend owner and optional manual systemd
  control, with prefix-aware startup metadata and Arch packaging.
- Versioned data-only configuration backups, bounded retention, migration
  snapshots, offline restore and refusal of conflicting live/recovery owners.
- Fractional-scale, screen-identity, keyboard/accessibility and resource
  hardening, with isolated native Wayland regression fixtures.
- Release documentation and a native disposable package upgrade scenario
  from `0.1.0-1` to `0.1.0-2`, retaining the existing install/uninstall checks.
- The legacy control applet's pending D-Bus callback now uses the same root
  object lifetime as the dock applet, avoiding callbacks after removal.

The earlier `0.1.0-1` package is the retained TASK-0043 verification checkpoint.
Package release `2` incorporates the recovery/hardening implementation and
release-preparation changes. Historical task receipts remain evidence of their
recorded checkpoints; fresh candidate results belong to the release checklist.
