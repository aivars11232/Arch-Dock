# Arch Dock licensing and distribution matrix

The owner selected **GPL-3.0-or-later** on 2026-10-04 for original Arch Dock
project work unless a file or component states a separate license. The root
[LICENSE](../LICENSE) contains the complete official GNU GPL version 3 text.
The project permits version 3 or any later version, at the recipient's option.
The Arch package records `GPL-3.0-or-later` for the project's default license.

The eight existing MIT-declared Plasma/KWin components remain **MIT**, including
their contents. Their declarations are preserved in each `metadata.json`, and
the complete MIT notice below ships with this document. Other explicit
component/resource licenses continue to apply; the GPL default does not
override them or license external application glyphs supplied by the system.

Reference-only material whose rights are unestablished remains **NOASSERTION**,
with redistribution **unknown**, non-installable and outside project-wide
relicensing. No GPL declaration grants copying, derivative or redistribution
rights to that material. Its images are absent from tracked normalized source
and package install rules; catalog records, reference-rights notices and
original-artwork recipes may accompany source as provenance documentation.

Historical `v0.1.0` and its six published assets are unchanged. This selection
is carried by the next candidate, application `0.1.1`, Arch package `0.1.1-1`.

## Distributed components

| Path/component | Origin/ownership | License | Distributed? | Compatibility and action |
| --- | --- | --- | --- | --- |
| `src/` original C++ | Original Arch Dock project code | GPL-3.0-or-later | Source and compiled application | Project default; ship GPL text and corresponding normalized source. |
| `qml/` shared renderer/runtime | Original Arch Dock project QML | GPL-3.0-or-later | Source and installed renderer | Project default; no runtime behavior changed by licensing. |
| `plasma-widget/`, `plasma-dock-widget/` | Existing Arch Dock applets; original metadata names Arch Dock | MIT | Source and installed applets | Preserve separate MIT declaration; compatible with GPL distribution with its notice. |
| `plasma-layout-template/`, `plasma-layout-template-launcher/`, `plasma-layout-template-tasks/`, `plasma-layout-template-hybrid/`, `plasma-layout-template-free/` | Existing Arch Dock layout components | MIT | Source and installed templates | Preserve all five separate MIT declarations and notice. |
| `kwin-script/` | Original Arch Dock integration; metadata names Aivars Rocens | MIT | Source and installed script | Preserve separate MIT declaration and notice. |
| `data/presets/panels/` | Original Arch Dock preset definitions | GPL-3.0-or-later | 15 built-in presets plus schema/version index | Project default; preset identities and behavior unchanged. |
| `data/presets/icons/` | Original Arch Dock preset definitions | GPL-3.0-or-later | 15 built-in presets plus schema/version index | Project default; preset identities and behavior unchanged. |
| `assets/themes/` production packages below | Independently created original artwork/geometry | GPL-3.0-or-later | 11 installed packages | Owner's new selection replaces an earlier absence of a selected public license; artwork bytes unchanged. |
| `assets/icon-styles/` production packages below | Independently authored procedural styles and asset-free fallback | GPL-3.0-or-later | Six installed packages | Same owner decision; external application glyphs retain their own rights. |
| `data/`, `cmake/`, `tools/`, `tests/` outside the separate components/references | Original Arch Dock project work | GPL-3.0-or-later | Source; selected data and startup metadata installed | Project default; preserve any explicit exception. |
| Qt 6 base/declarative/SVG/Wayland and optional Quick3D | External Qt project, separately installed Arch packages | Their GPL/LGPL open-source options and specific third-party declarations | Dynamically used; library files not bundled in Arch Dock | GPLv3 is an available common distribution route; preserve dependency notices/source obligations. A GPLv3-only dependency does not gain an “or later” permission from this project. |
| KDE/KF/Plasma/KWin, glibc, GCC runtime and optional KPipeWire | External projects, separately installed Arch packages | Their own LGPL/GPL/runtime-exception declarations | Used as system dependencies; files not bundled in Arch Dock | Retain their licenses. Use compatible library terms; package-wide metadata does not relicense dependencies. |
| `assets/source-samples/` reference images and rights-unknown catalog entries | Third-party/provenance-unknown reference material | NOASSERTION; redistribution unknown | Images excluded from normalized source and binary package | No compatibility conclusion or ownership claim. Remain reference-only and non-installable. |

No vendored third-party library or unknown-rights artwork is introduced by this
candidate. Qt's open-source licensing routes are described in its
[official licensing documentation](https://doc.qt.io/qt-6/licensing.html);
GNU's [license compatibility guidance](https://www.gnu.org/licenses/license-compatibility.html)
and [license list](https://www.gnu.org/licenses/license-list.html#Expat) describe
GPL/LGPL and MIT compatibility. Installed dependency package declarations and
the complete production-output hash audit are retained with candidate evidence.

## Original asset declaration changes

Each row is justified by existing original-production evidence, not by the
license of a visual reference. For all 17 packages, creation/redistribution was
already authorized and no separate public SPDX license had been selected.
The new owner decision selects GPL-3.0-or-later for that original work.
Manifest/production-record license fields change; icon manifest output hashes
are refreshed when the declaration changes that recorded file. Geometry, SVG,
texture, QML, visual-review evidence and excluded-reference records are retained.

| Production package path | Existing provenance reason |
| --- | --- |
| `assets/themes/sci-fi-chassis-dark/` | TASK-0027 hand-authored SVG; production record says no derivative or source pixels. |
| `assets/themes/sci-fi-chassis-red/` | Same TASK-0027 independent production and excluded-reference evidence. |
| `assets/themes/sci-fi-chassis-blue/` | Same TASK-0027 independent production and excluded-reference evidence. |
| `assets/themes/energy-frame-cyan/` | TASK-0028 hand-authored SVG; production record excludes source pixels/derivatives. |
| `assets/themes/energy-frame-green/` | Same TASK-0028 independent production and excluded-reference evidence. |
| `assets/themes/energy-frame-orange/` | Same TASK-0028 independent production and excluded-reference evidence. |
| `assets/themes/energy-frame-purple/` | Same TASK-0028 independent production and excluded-reference evidence. |
| `assets/themes/ring-platform-blue/` | TASK-0034 hand-authored SVG; production record excludes source pixels/derivatives. |
| `assets/themes/octagon-platform-steel/` | Same TASK-0034 independent production and excluded-reference evidence. |
| `assets/themes/arc-platform-orange/` | Same TASK-0034 independent production and excluded-reference evidence. |
| `assets/themes/mesh-platform-cyan/` | TASK-0035 manifest records geometry/material/texture authored numerically from scratch without external assets; declared asset hashes verify. |
| `assets/icon-styles/plain-original/` | TASK-0029 asset-free schema-authored fallback; production record excludes source pixels/derivatives. |
| `assets/icon-styles/metallic-blue/` | TASK-0029 hand-authored procedural QML; production record excludes source pixels/derivatives. |
| `assets/icon-styles/metallic-red/` | Same TASK-0029 independent procedural production evidence. |
| `assets/icon-styles/neon-green/` | Same TASK-0029 independent procedural production evidence. |
| `assets/icon-styles/neon-orange/` | Same TASK-0029 independent procedural production evidence. |
| `assets/icon-styles/dark-orb/` | Same TASK-0029 independent procedural production evidence. |

## Preserved MIT component notice

Applies to the eight MIT-declared component directories listed above.
Copyright (c) Aivars Rocens and Arch Dock contributors, as identified in
component metadata and project history.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
