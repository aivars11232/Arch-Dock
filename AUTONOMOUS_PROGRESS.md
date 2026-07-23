# Arch Dock Native Plasma Architecture

Updated: 2026-07-23

## Runtime

- `src/main.cpp` starts the Arch Dock service and exports `PanelWindow` on the session bus at `org.archdock.ArchDock` and `/Control`.
- The service owns window discovery, pinning, launch/window actions, persistent panel records, native containment ownership, and recovery after output or PlasmaShell changes.
- Arch Dock does not create a visual root `QWindow`, use LayerShellQt, or render a parallel dock surface. Settings and icon-property dialogs are ordinary utility windows only.
- The visible dock is the `org.archdock.dock` Plasma applet inside a real Plasma panel containment.

## Native Panels

- Native creation accepts an explicit edge and type: `empty`, `launcher`, `tasks`, or `hybrid`.
- Empty panels contain no Arch Dock applets. Launcher, tasks, and hybrid panels contain exactly one owned `org.archdock.dock` applet.
- `PanelRegistry` persists the containment id, ownership token, and owned dock id. Recovery verifies the token before modifying or removing a containment.
- A legacy `org.archdock.control` applet is removed only when it is inside an owned containment and its configured `panelId` matches. New panels never attach that applet automatically.
- The optional `org.archdock.control` package remains a native Plasma management applet. Its add-panel menu chooses type first and edge second.

## Plasma Integration

- `plasma-dock-widget/` declares `General/panelId` and `General/panelType` in its package schema and provides a standard native configuration page.
- Managed applet type changes call `setNativePanelType`, which updates both the registry and the current containment. Template-created panels retain their own Plasma configuration.
- The dock reads `Plasmoid.containment.corona.editMode` directly. While Plasma Edit Mode is active, Arch Dock entry input is disabled so Plasma retains normal applet movement, removal, and configuration controls.
- Four native Add Panel templates are installed: Empty, Launcher, Tasks, and Hybrid. Template panels are owned by Plasma; service-created panels are owned and recovered by Arch Dock.

## Retired Compatibility Paths

- Standalone dock QML, custom edit overlays, LayerShellQt placement, bootstrap widgets, and edge-only default-hybrid creation are removed.
- Arbitrary-coordinate free panels are unsupported by the public native Plasma panel API and are retired. Persisted legacy `free` records migrate to a native bottom edge; obsolete free-panel preferences are removed on the next settings save.

## Validation

- `cmake --build build --target arch-dock panel-registry-test -j2` passes.
- `ctest --test-dir build --output-on-failure` passes the registry, dock-geometry, and motion-policy suites.
- `qmllint` passes the visual applet, its native configuration page, the optional manager applet, and the settings utility QML.
- A staged `cmake --install` contains `org.archdock.dock` with its configuration schema and page, and contains no bootstrap plasmoid.
- `tests/run-plasma-lifecycle.sh build/arch-dock` passes in an isolated two-output KWin/Plasma session. It verifies all panel types, configuration readback and managed type changes, legacy-control cleanup, output disable/restore, containment replacement, PlasmaShell restart recovery, and preservation of unrelated panels.
