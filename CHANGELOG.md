# Changelog

## 0.1.1-50 — 2026-10-26

- **Icon animations play again.** Every icon animation had stood still: when
  a panel's animation changed, the dock reset the motion runners it had just
  made for the new animation a moment after they started, and nothing
  started them again. A runner that is still wanted now starts over from its
  beginning when it is reset.
- **Idle rotate belongs to 3D panels.** It turns each icon on its upright
  axis over its stand, which a 3D panel draws. On a flat panel it could only
  spin the whole icon flat, stand and light included, like a carousel, so it
  is offered on 3D panels only; a flat panel that already has it keeps it
  listed, and its icons stay still.
- **Folder openings keep their names.** On a flat or baked free panel the
  window an opening is drawn in held the children but not their names, so
  the lowest names were cut off; it holds the names too now.
- **Side panels no longer cut their decorations off.** A decoration on an
  Arch Dock bar or a Plasma-held edge panel was laid out over the dock's own
  drawing area, which is deeper than the panel (room for magnification) and,
  in a bar, reaches past the bar's ends; the panel's window then sliced the
  decoration along its long edges and at its ends (a hull's ends, the
  arches' shields, the raptor's dome). A decoration now fits the panel's own
  thickness, and along a bar it runs from one end of the bar to the other,
  its own end shapes inside the bar.
- **Edge panels can be drawn in 3D.** On a Plasma-held edge panel or an
  Arch Dock bar, Panel Studio's 3D page offers the 3D switch and one
  setting, Tilt, 0 to 60 degrees (25 to begin with). The panel is drawn as
  a straight 3D platform as large as the panel, tilted from its screen edge
  toward the middle: a bottom panel's upper side, a top panel's lower
  side, a left panel's right side and a right panel's left side go into
  the screen. Its icons stay upright, centred on their places, smaller as
  the platform takes them away, and the platform and its icons stay inside
  the panel. The panel's own colour still colours the platform. An edge
  panel that opens and closes is not offered 3D (a generated platform has
  no parts to open and close with), nor is one wearing a skin. Free panels
  are unchanged.
- **No Plasma panel behind an edge panel.** Plasma drew its theme's own
  panel background behind a Plasma-held edge panel (a white or grey bar in
  the default theme). While the dock stands in a Plasma panel it now turns
  that background off, and gives it back if it leaves; Plasma's saved panel
  settings are not changed.

## 0.1.1-49 — 2026-10-26

- **The Star layout is the Star of David.** Two equilateral triangles laid
  over each other: six tips, the notches where the triangles' edges cross.
  Only the shape changed; the star is always six-pointed now, so Polygon
  sides is offered for the Polygon layout alone. Flat, baked and 3D panels
  draw the same star and stand the icons on it.
- **Stack removed.** The Stack folder layout and its Stack length setting are
  gone from free and edge panels alike, on the owner's decision; a panel
  saved with a stack opens its folders as a fan.
- **Along the dock on a flat or baked panel** opens as it did in 0.1.1-47:
  one step out of the dock, the children's cells clear of the dock's icons.
  0.1.1-48 had added the names to that test, which no step within its range
  could pass, so the children fell back to the base distance, too close to
  the ring.
- **A 3D panel's decoration while the dock turns.** A turning 3D panel
  (Panels > Motion) showed no decoration until the turning stopped: the
  decoration was worked out round the icons' drawn places, which move
  every frame as the icons travel, so its bake started over sixty times a
  second and never ended, and the baker's thread worked flat out meanwhile.
  The decoration is now worked out round the icons' rest places, as on a
  flat panel, and the icons travel over it.
- **Along the dock on a 3D panel:** a child's standing point glides round
  the ring. Behind the dock's middle a child stands by its stand's base, its
  body rising behind the ring; before the middle it carries its icon on the
  curve with its stand below, as anything standing before the ring does, so
  the near-side children hug the ring instead of dropping a body's height
  below it; between, the point glides with the child's height across the
  dock, so a child scrolled round the ring never jumps.

## 0.1.1-48 — 2026-10-26

- **Crash protection.** Arch Dock's two programs (`arch-dock`, `arch-dock-bars`)
  run the QML garbage collector to completion in one go instead of in time
  slices (`QV4_GC_TIMELIMIT=0`, read as each engine starts and taken out of
  the environment again, so programs started from a panel do not inherit
  it). The sliced collector of Qt 6.8 and later crashed inside itself while
  Panel Studio pages were being built; a limit you set yourself is kept.
  Plasma's own shell, which draws the edge panels, is outside Arch Dock's
  reach.
- **3D icons on 3D panels.** The 3D icons switch now reaches a true-3D
  panel's scene; since 0.1.1-27 it had turned the solids on only for flat
  panels, so a 3D panel kept flat pictures whatever the switch said.
- **Folder openings on tilted panels.** "Along the dock" hugs a 3D panel
  as drawn: each point of the dock's curve steps out on the panel's plane
  only as far as the child standing there needs to clear the panel and its
  icons on the screen. On a steeply tilted ring the children stand just
  past the rim at the sides, above the far icons' tiles with their names
  above them, away from the ring, and before the near band with their
  icons below the near icons' tiles, on a curve that bends without kinks.
  Before, one step served the whole curve: on a steep ring the far side
  stepped out on the plane but hardly moved on the screen, so every curve
  failed and the children were put back on the rim, over the icons. The
  panel's outline now comes from a panel decoration's own solid while one
  is shown (the platform under it is not drawn, so the children had only
  the icons to keep off), and a child in front of the dock is drawn no
  bigger than the dock's nearest icon. On a drawn band (a fan's arc, an
  arc, a ring) each child stands with its stand's base on the band and the
  icon above it, at every tilt; the names sit below the stand as drawn and
  keep the dock's own letter size wherever they stand along the depth.
- **A hover moves the hovered icon alone.** The magnification's reach is
  1 icon by default (it was 2.4, so two neighbours on each side grew, lit
  up and their pedestals glowed with every hover); a stored reach of 2.4,
  the old default saved into every panel, is read as the new one.
  "Magnification reach" in Icons > Behavior still spreads it to the
  neighbours on request.
- **Icon size with thickness, both ways.** An icon size that filled the
  thickness keeps filling it when the thickness changes back; other sizes
  scale with the thickness as before, within 24 to 128.
- **Icon drops.** An icon dragged along a panel lands before the icon
  nearest the pointer that can take it; icons that cannot (status entries,
  icons without input, icons kept still in edit mode) are passed over.
- Code comments rewritten in plain words for the community; test checks
  brought up to date (the 3D icon solids switch, folder shapes as drawn).

## 0.1.1-41 to 0.1.1-47 — released 2026-10-23 to 2026-10-25

Published as GitHub pre-releases with a one-line install (see README).

- **Bars have their own edit mode.** Enter Edit Mode in a bar's menu opens
  Panel Studio for the bar and shows each widget's remove (✕) and settings
  (⚙) buttons; right-clicks never end it. The + outside the bar opens
  Plasma's Add New menu (Spacer, Margins Separator, Widgets…); Widgets…
  shows Plasma's widget list for the bar without putting Plasma's desktop
  in edit mode.
- **Everything on a bar moves.** Widgets are dragged to any free place, in
  edit mode or not; the dock's icons stand in groups (one per segment) laid
  out among the widgets, so widgets and spacers go between groups; a group
  is dragged by its grip in edit mode. Icons move by dragging on every
  panel, landing before the icon nearest the pointer.
- **Remove Panel** in every Arch Dock panel's right-click menu; Add panel…
  left it (new panels come from Panel Studio opened on its own).
- **Sizes.** Fit content is on the Size page (the length field hides while
  it is on); a length set by hand ends fitting; a bar is never shorter than
  its icons with the look's ends; thick bars keep full round ends; icons
  follow the thickness.
- **Folder icons** gained an Icons menu: Sort By, Icon Size, Show Previews
  (thumbnails made by KIO), kept per folder.
- A launcher on a bar always shows its icon; the bar host is replaced after
  an update; CI runs on release tags and pull requests, not every push.

## 0.1.1 candidate — unreleased, 2026-10-05

Application version: `0.1.1`; the current Arch package candidate and verification
status are recorded in [current state](docs/CURRENT_STATE.md).
The owner selected GPL-3.0-or-later for original Arch Dock project work.
The proposed `v0.1.1` tag and publication need separate authorization.

- 3D tiles as bubbles (Phase 4, part two; the owner, 2026-10-19: "3D icon
  tiles should be more like glass, transparent, and yet the outlines are
  visible enough to tell a difference"): on a 3D panel, Icon Tiles > 3D
  tile form set to Bubble puts, in the tile's place, a closed glass shell
  in the tile's outline (rounded, square, squircle, circle, hexagon or
  diamond) round its icon, domed toward the viewer and away, clear and
  glossy in a pale tint of the tile's colour (as clear as the tile's
  opacity allows), with its outline drawn round it in the tile's border
  colour and width so the bubble shows for all its clearness. The bubble
  holds the icon whole and centred whatever the shape of either (the
  owner, 2026-10-19): it grows to the smallest width of its shape round
  the icon's whole picture, so a circle comes out a little wider than the
  cell and a diamond wider still; its middle lies on the icon's own
  middle, so the icon sits at its centre from every side; its depth
  follows the icon's depth and its rings' reach; and inside it a round
  icon keeps a flat back instead of rising as a ball, so there is no
  bubble in the bubble. The icon keeps its own shape; nothing is clipped
  to the tile's. Flat keeps the tile as it is.
- Real 3D icons (Phase 4; the owner, 2026-10-13: "real 3D icons, where it
  renders my current icon in 3D not changes the icon but renders it", with
  the Sketchfab "Firefox logo 3D" model as the reference): on a 3D panel
  each icon's own picture becomes a volume modelled from the picture
  itself, in the kind the picture asks for. A plate-shaped icon (its
  outline fills its box like a rounded square: a sign, a badge, a tile)
  becomes a block: just thickness, the picture on its front and back, the
  walls in its edge colours. Any other icon (a round picture of a thing,
  a figure, a star) is read for the shapes it is drawn from, as a
  modelled logo is built: a thin band round an ellipse is a ring seen at
  a tilt and becomes a real ring through the icon, passing behind it
  where the picture hides it and in front where the picture shows it; a
  round outline is a globe the parts ride on; a part with few corners (a
  triangle, a letter) rises as a prism with bevelled edges; every other
  part of one colour rises as a rounded volume of its own from its own
  edge, a half round at half the depth, with a crease to the next part,
  a part inside another a step up; and the light rounds every part; the
  picture stays the colour of it all, front and back, so the icon stays
  the user's own. Icon depth (a
  share of the icon's size, 0 keeps the flat picture), Icon turn (how far
  the icon turns so its depth shows), Icon relief (Auto, Rounded or
  Block for every icon of the panel) and Icon lighting (how much the
  light models the volume: 0 keeps the picture's own brightness, 1 lets
  the light alone shape it) on the Panels > 3D page. The icons' motions
  turn and move the volume as they did the picture.
- Two flowing panelpapers: River (the owner, 2026-10-15: "There could even
  be like tha panel is the river, with animations") and Magma (the owner,
  2026-10-18: "Since we have water, mind as well we can add flowing magma
  too"). The river is water flowing along the band, in five moods (River,
  Clear, Deep, Jade, Sunset) on the Colour row; the magma is dark crust
  with the lava glowing between its plates, in four heats (Magma, Ember,
  Blaze, Molten). Both are made from real texture sets (ambientCG, CC0:
  lava sets and river beds, see `qml/ArchDock/Rendering/materials/CREDITS.md`),
  the water composed over its bed from a real wave spectrum, with the bed
  showing through the shallows and the sky mirrored in the ripples. Both
  flow along the band in a loop (the owner: "No need for waves just
  regular looped flow is enough"). On a 3D panel the water runs over its
  still bed in two layers of ripples that bend the light and mirror the
  sky, with the lights glittering on it, and it mirrors the icons standing
  on it: their mirror images move with them at once, scrolling and
  animations included (the owner: "the reflection should move same time
  when icon moved"); the lava creeps along with its
  crust, lighting itself in a slow shimmer, so the Glow rows stay for it
  (water takes no glow, like a picture, and a look that takes no glow now
  draws none). Motion speed sets the flow. A flat panel shows the water
  with pale gleams sliding along it and the lava with hot streaks creeping
  along it, each in its own colours unless a colour is chosen. Stands and
  tiles can wear them too, still; stands and tiles of magma glow with its
  lava.
- Pictures can be imported (the owner: "for Icons, pedestals, icon tiles and
  panels, there should be button or option to import a picture"): an Import
  picture… button on Panels > Appearance and on Pedestals (the icons' stands
  and the folder items' stands) puts a picture of the owner's own on the
  panel or its stands, and a Picture choice joins their look lists. Arch
  Dock keeps its own copy, at most 2048 pixels on its longest side. A flat
  panel wears the picture once over its shape; a 3D panel round its band, at
  the picture's own proportions, in its own colours and still. Icon tiles
  take no picture (the owner: "for icon tiles, picture is not needed, not
  practicall, colors can be changed and glow and shine, and transparency and
  shape"), and icons themselves are never coloured (the owner: "those colors
  should not affect icons at all"); every existing look draws as before. A photo its camera stored on its side, as phones do, is
  now read upright, here and in theme artwork, where it was refused.
- A 3D panel wears its picture in real relief (the owner: "if the picture
  is rocks, I should be able to see rocks same as I see them in real
  world", "I don't need 2D disguised as 3D. I need real 3D"): the panel is
  cut finely and its surface itself moves with the picture's brightness, a
  generated band and a theme's own 3D platform alike. Relief depth sets how
  far; Relief direction Inward keeps the band's outline, the raised parts
  at its surface and the rest sunk in, like stones in a gabion's mesh, and
  Outward raises them out of the band like little mountains; Raised parts
  says whether the picture's light parts or its dark parts are the raised
  ones. A panel in relief has no glowing inner and outer edge, and a panel
  or a stand wearing a picture draws no glow (the owner: "Glow not
  practicall for rocks"). A picture is worn in its own colours, so a
  panel's and a stand's Colour setting is put away while they wear one.
  A 3D stand wearing a picture of its own stands out in relief the same
  way, with its own Relief depth, Raised parts and Relief direction, its
  stones standing out of the thin stand by default (the owner: "Add the
  same thing for ... pedestals too").
- Panelpaper and panel decorations (the owner: "For pictures that cover
  panels surface, the name should be panelpaper and for contours, outlines
  and stuff name should be panel decorations"; "Panelpaper and Panel
  decorations should be under appierance"). A panel's surface is its
  panelpaper: Panels > Appearance opens with a Panelpaper section, the
  built-in panelpapers (Glass, Crystal, Neon and the rest) and "Imported
  picture" in its list; choosing Imported picture shows the row with the
  picture and Import panelpaper… (the owner: "those import picture
  settings appiers only when I select imported picture") and, with no
  picture imported yet, opens the chooser at once; a 3D panel's "Default"
  is the surface its own design gives it. A panel's outlines, contours and parts are its
  decoration: the built-in decorations (the sixteen theme packages
  installed with Arch Dock, flat and platform ones) are cards under Panel
  decorations on the same page, with Import…, Re-render and Clear for a
  decoration package of your own; the Presets page holds presets only. A
  colour not set reads "Default".
- A flat panel's picture stands out in relief as well (the owner: "some
  option to change texture or relief, like with slide, the more the higher
  is the relief ... that function can be in both 3D and 2D"): the same
  Relief depth slider embosses the picture by its brightness, lit from the
  top left, and Raised parts says whether its light or its dark parts
  stand out; Relief direction stays a 3D panel's choice.
- The relief has the regulators a relief generator has (the owner, with a
  depth-map workflow: "That's how i want them on my panels and pedestals.
  That's what i mean with real relief"; "imported picture should be
  rendered like that with some extra settings to adjust to users needs"):
  the picture is the depth map, its brightness the height, and beside
  Relief depth (the height scale), Raised parts (invert) and Relief
  direction, there are now Relief level (the brightness that is half
  height, the map's midtone), Relief contrast (how steeply the height
  climbs through it: gentle slopes or sharp steps) and Relief smoothing (a
  blur of the map), for panels and for stands, in 3D and flat. The mesh is
  cut finely, so there is no terracing to smooth away; the edge-on view is
  the side check.
- Panel Studio has UI themes (the owner: "make multiple UI themes", "make a
  separate side tab with themes and their previews"): Classic, Teal Glass,
  Teal HUD, Amber HUD, Alien White, Alien Black and Alien Green, each shown
  in a small preview on the new UI Themes side tab, which wears Arch Dock's
  own icon. The choice is remembered; panels on the desktop are not changed,
  and the window is always solid, so the wallpaper never shows through. The
  three alien themes (the owner asked for "Alienware" white, black and green
  looks, without marks or logos) have honeycomb vents, glowing light strips
  under the title and on the chosen section and tab, a neon edge and thin
  wide capitals; Alien White is white and black two tone. Panel Studio's
  buttons, lists, sliders and fields now take the theme's colours.
- Panels nobody named are called exactly as their shape is listed (the
  owner: "Ring", "Möbius Strip"): "Ring", "Möbius Strip", "Star", "Ring 2"
  for a second one; edge panels stay "Top panel" and so on. Names from
  earlier versions, such as "Ring panel", are renamed once.
- Panels > Layout > Dock layout lists plain shape names instead of code
  words, and Triangle, Square, Pentagon, Hexagon and Octagon are gone from
  it (the owner: "Since in settings I can just increase or decrease the
  corners of polygons, the rest of them in that list are sort of useless"):
  a panel that was one of them is a Polygon with that many corners and looks
  the same. Preset cards name their layouts the same way.
- A 3D panel's icon limit counts the panel at its size as drawn, however it
  is tilted (the owner chose "Count the size as drawn"): a steeply tilted
  platform that took one icon as seen now takes thirteen.
- Every change made in Panel Studio shows on the desktop panel at once
  (the owner's request of 2026-10-13): its look, shape, size, glow and
  icons, the settings shared by all panels, an edge panel's Plasma size and
  place, a free panel's position. Apply or OK keeps it; Cancel or closing the
  window puts the saved panel back as it was, with its size and place.
  Nothing is saved before Apply. A change Apply would refuse is not drawn,
  and Panel Studio says why. A panel moved by hand meanwhile keeps the
  Studio's changes.
- Saved profiles can be edited (the owner: "And not only profiles but also
  editing it"): Profiles lists the panels a profile shows; each can be
  renamed, removed (a built-in edge panel hidden) or edited with Edit…,
  which opens Panel Studio's own pages on the profile's copy. Apply saves the
  profile; the desktop changes when the profile is applied.
- Panels have proper names (the owner: "names shouldn't be just free panel
  1,2, etc."): a panel is named for what it is, "Ring panel", "Möbius strip
  panel", "Top panel", numbered when the name is taken, and follows its shape
  until it is given a name in Panels > General > Name. The old "Free panel N"
  names are renamed that way once.
- Panel Themes / Skins and Built-in Panel Presets are one Presets tab (the
  owner: "Presets and panel themes are same thing, so they can be merged"),
  and its previews show the bare panel, without icons ("the preview there
  shouldn't be any icons, only bare panel").
- Folders open at once and stay smooth (2026-10-13, the owner: "Folder
  opening on any option is super glitchy, opens with huge delay and freezes
  Arch Dock while it opens"): the stands' glow and a fan's outline are blurred
  on the graphics card instead of Plasma's main thread, a spinning stand is
  drawn once for each step of its turn and shared by every icon with the same
  stand, and the curve "Along the dock" is worked out once per opening. A
  folder that took more than five seconds to appear now shows within a tenth
  of a second. Stands in a glowing look are light, as on a 3D panel: green
  stars instead of black ones.
- A star on a 3D panel (the owner's "Draft validation failed" with Layout:
  star): 3D panels draw a star platform, and their Layout list offers only
  layouts their look can draw in 3D.
- A removed panel is gone (the owner: "right click on panel and select remove
  panel, that should remove panel entirely, except the ones saved in
  Profiles"): once Plasma has deleted a removed panel (Undo is no longer
  possible), Arch Dock removes its settings too; panels left behind earlier
  are cleared. Put back on the desktop and the orange note about missing
  widgets are gone; profiles keep their own copies.
- A free panel stays on the desktop until you remove it (2026-10-14: after a
  login the owner's two free panels were gone from Plasma, as after each
  Plasma restart the day before): when Plasma has lost a free panel's widget
  without its owner removing it, Arch Dock puts the panel back at its place,
  with all its settings, about a minute and a half after Plasma starts. It
  never adds a second widget for a panel that still has one, and a panel you
  remove from its right-click menu stays removed. The 24 panels earlier
  versions had left off the desktop are cleared at the first start (the
  owner: "Delete all 24").
- Free panels keep apart (the owner: "Folder openings should not overlay
  other free docks, so there should be minimal aloud distance between free
  docks depending on their size"). A folder opening keeps clear of every
  other free panel as drawn, not only its own. A free panel moved closer to
  another than the room the smallest folder opening needs - one child at the
  bigger panel's drawn size, and 8 pixels - moves straight away from it,
  just far enough, once the move ends; on the desktop, in Panel Studio or
  with the 3D handles. Starting up, a profile or a preview moves nothing.
- Panel Studio edits one panel at a time, as Plasma does: the one opened with
  "Configure Arch Dock…"; Configure on another panel moves the window there,
  asking first about changes not applied yet. Opened on its own, or with a
  panel's new "Add panel…", it adds a panel - a free one, or one on a screen
  edge where no panel stands yet - and then edits it.
- On a tilted 3D panel the icons keep facing you, the panel's own and an
  opened folder's (the owner: "the icons should allways face the user ... no
  icon would be squeshed"); the far ones are only smaller. The folder's
  stands are seen at the panel's angle, like the stands on the panel,
  instead of from a little above.
- Arrow keys in an open folder keep their selection while the pointer rests
  over the folder.
- On a 3D panel a folder opens on the panel's own plane, tilted with it, and
  never covers or touches the panel; along the dock it reaches a quarter to
  three quarters of the way round. Every opening holds its smallest number
  at once, the rest come in with the wheel: three named children on a fan,
  an arc or a stack, two rows of three in a grid, six in a ring. Children
  are spaced at the size they are drawn, far ones smaller, and none larger
  than the folder's own icon. The fan follows the panel's rim round the
  folder; a stack lies along the panel's side nearest the folder, up and
  down beside a side, left and right under or over the panel; the grid lies
  on the plane too, with no border round it. A ring with more children grows
  a second circle round the first, the small one inside the big one with a
  clear gap all round, the two leaning opposite ways.
- A free panel holds as many icons as its size has room for (2026-10-14, the
  owner: "there should be limited amount of how many icons I can put on that
  panel depending on it's size", "nothing needs to be cut off when resized",
  measured from the 16-pixel gap between the icons of the owner's own top
  Plasma panel, "no matter of the icons size"). Each icon needs its own size
  and 16 pixels along the panel's ring, star or arc; a row grows with its
  icons. On a 3D panel the room is what is seen: its platform's size as
  drawn and its tilt, which shortens the ring's ends as seen, so a tilted
  ring holds fewer. A drop past the limit is refused and the panel says why
  for four seconds; Panel Studio's Radius and Icon size stop where the icons
  would no longer fit, as do the 3D handles on the desktop; a preset whose
  icons would not fit makes the panel just large enough. A panel already
  holding more keeps its icons and can only be given more room.
- A Möbius strip, as the owner's reference pictures show one (the owner:
  "Maybe add there another shape - Möbius strip", then many corrections: one
  band, "NO VISIBLE GAPS, NOT EVEN SEPERATORS", "in 2D it's just infinity
  symbol, in 3D it's visible Möbius strip"):
  - a folder opening on free panels: a band along an ∞ beside the folder,
    one seamless shape, its children half on each loop and none where the
    loops cross; flat it is the ∞ itself; on a 3D panel's plane it turns
    half a turn out of the plane, its two loops leaning opposite ways, the
    strand over the crossing facing the viewer. An edge panel's popup shows
    it as a ring.
  - a free panel layout, "Möbius strip": flat, its icons stand on an ∞
    track; in 3D on a generated twisted Möbius platform, one band closed on
    itself, its strands passing the crossing apart in height. Its icons ride
    the band's one face: turning the dock carries them through the twist to
    what looks like the other side and back. Each icon and its stand stay as
    on any panel, the stand on the band's face and the icon upright above
    it; on the face turned toward the viewer the two are in front of the
    band, on the face turned away behind it, so the band hides only what of
    them it covers and never cuts through them, and an icon comes out from
    behind the band's edge gradually. The band's glow is drawn behind the
    icons in front of it. It holds two icons fewer than its length has room
    for, for the gaps where its loops cross.
- A ring or Möbius opening starts on the side of the panel with more room,
  counting other free panels, and keeps the side where it stands nearer and
  holds more at once; beside a large flat panel it took 1.4 s to find a
  place far away, now 0.16 s beside its own panel.

- A 3D panel turns freely all the way round (2026-10-13, the owner: "freely
  in 360 every direction"): Pitch goes past its edge and over, like Yaw and
  Roll, in Panel Studio and on the desktop's handles; tilted 90 degrees it
  lies level whatever its turn (the panel is tilted, then turned about the
  view's up: "I still should be able to tilt 3D panel fully horizontal").
  A key light from behind lights its underside.
- Scaling has no practical limit and works each way (the owner: "for
  scaling there shouldn't be a limit", "scale it also sideways or top or
  down"): Scale from 0.05 to 20 times, and Scale sideways and Scale up and
  down on their own, which stretch the platform and where its icons stand,
  never the icons; Platform thickness, the third way, as far. On the
  desktop the red and green scale arms stretch their own way, the blue one
  the thickness, and the white cube in the middle scales all ways; wide
  sliders move through the ratio, 1 in the middle.
- Moving and scaling are done by hand (Edit on desktop); the 3D page keeps
  the exact turn (Pitch, Yaw, Roll) and Reset 3D transform (the owner:
  "those controls I don't really need, since I can do it physically on my
  desktop").
- One Look for each surface (the owner: material and texture "are exactly
  the same thing ... it's enough with just one of them ... pedestals and
  other stuff too"): the 3D platform, the stands, the folder stands and the
  icon tiles each have one Look that sets both, with names instead of raw
  words. A 3D panel's look - Look, Platform colour, and the look's own
  background, colours and motion - is on Appearance (the owner: "no neons
  settings, no lightning, nothing is there"); the 3D page keeps where and
  how the platform stands.
- Panel Studio's title shows the Arch Dock logo (the owner: "there should
  be Arch Dock logo").
- A free panel has no box of its own (2026-10-13, the owner: "the only
  edge is edge of the screen"): a 3D panel scaled, tilted or moved, an
  icon magnified at its edge, the editing handles, are drawn as far past
  the widget's box as they reach - the 3D view grows round the box with
  its field of view widened by as much, so nothing moves on the screen -
  and the pointer, the wheel and drops reach them there. The Studio's
  preview fits the whole drawing. Picking and input map through the
  camera's own frame: Qt's viewport mapping kept a projection from before
  the view last changed shape.
- Glow is light (the owner: "the glow isn't really glow, it's just intense
  color"): a 3D panel spreads what is brighter than white round it over
  the desktop - a real bloom, smooth at every quality (the look the owner
  ticked) - and a flat item draws its own shape blurred in the glow's
  colour behind it. The panel, its icons, its tiles and its stands each
  have their own Glow (intensity, reach, colour) on their own page, shown
  only where light round the look makes sense - not on grass, marble or a
  lime - with its reach and colour only while it shines. The view keeps
  room for the light even round a shape that fits its box (on the desktop
  it ended in a straight line along the box), and a glow's light shows
  round what it wraps, never through a see-through look (a lightning ring
  had turned white). The panel's glow keeps its look up to the middle of
  its range and, from three quarters up, lays its light over the shape's
  faces, white-hot at full intensity with the edge still showing (the
  owner's rule: white "on max brightness", not "closer to mid"; the four
  levels ticked).
- In 3D an icon under the pointer is found through the camera's own frame,
  not Qt's picking, whose projection lags a view that has just changed
  shape until it renders again: the wheel over the icons of a panel whose
  view had grown round it did nothing.
- Panel Studio shows no green notes and no raw placement results (the
  owner: "these green things can be removed, they serve no purpose"); a
  placement or visibility Plasma refused, and a free panel's missing
  widget, are still reported.
- Looks: Plasma is glowing gas instead of jagged lines; Lightning is a look
  of its own, bolts grown as a discharge crackles, striking at random and
  flickering, on a clear ground with its storm as strong as chosen
  (half by default); Aurora is new, curtains of light in the night sky.
  Lightning comes in eight palettes and Aurora in six, Plasma, Neon and
  Circuit in a rainbow, the metals in a faint Chameleon or a stronger
  Iridescent finish; Lime is a lime's peel. A glowing look's own light, its motion and its colours are
  named for it on the page (Lightning glow, Strike rate, Aurora colour).
- Folders open along the dock on a star too, and a panel whose shape has
  no curve opens Along the dock as a fan instead of a popup saying "This
  saved layout uses Fan." Panel Themes / Skins lists only the looks made
  for the picked panel: no horizontal chassis skins on a free panel.
- A setting that cannot apply is not shown (the owner's rule for every
  setting): the new ones carry their rules, an icon's motion settings are
  unseen with no motion chosen, and the icon's glow colour is offered
  whenever its glow shines; the rest is the next audit.
- A free panel's flat stands stand on its path again on the desktop: the
  desktop icons read the setting from out of their reach and drew the
  stands as in an edge panel's row (since the stands redesign).

- Panel Studio is organized by what it edits, one page per setting
  (2026-10-13, the owner: "the option to add a panel should be only in one
  place", "make sure there aren't any duplicate settings", "icon settings
  belong to icons, same goes for tiles and pedestals", "option to add and
  select panel belongs to the panels section, not overview"). Overview
  shows the panel's preview and what is applied to it: Panel, Icons, and
  Icon Tiles and Pedestals only when the panel draws them. Panels holds the
  panel list, whose last entry "New free panel" shows the bare new panel
  that the button beside the list adds, and the panel's settings, with no
  preview of its own. The shape under the icons is on Icon Tiles alone;
  Show tooltips is "Show names on hover" on Icons > Behavior; the icon
  style's pedestal and the Auto stand's height are on Pedestals. General
  has no buttons: a panel is removed with its right-click menu, an edge
  panel hidden with Behavior > Visible, and applications are added by
  dropping them on the panel from the application menu. A group of looks
  with none for the panel is not shown.
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
