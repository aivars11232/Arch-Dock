#!/usr/bin/env python3

import pathlib
import sys

def instrument_interaction_stage(stage):
    """Observe the disposable applet's actual geometry, without changing input.

    Production and the ordinary import smoke stay uninstrumented. These timers
    only report existing items; all actions enter through KWin's EIS devices.
    """
    import os

    stage = pathlib.Path(stage).resolve(strict=True)
    assert stage.name == "stage" and stage.parent.parent == pathlib.Path("/tmp")
    assert stage.parent.name.startswith("archdock-rendering-import.")
    assert stage.stat().st_uid == os.getuid()
    ui = stage / "share/plasma/plasmoids/org.archdock.dock/contents/ui"

    def insert(file, anchor, addition):
        path = ui / file
        source = path.read_text()
        assert source.count(anchor) == 1, (file, anchor)
        path.write_text(source.replace(anchor, anchor + addition))

    insert("main.qml", "id: windowPreview", "\n                observedPanelId: root.panelId\n                observedActive: representation.authoritativeHost")
    insert("main.qml", "id: folderExpansion", "\n                observedPanelId: root.panelId\n                observedActive: representation.authoritativeHost")
    insert("FolderExpansionHost.qml", "id: root", '\n    property string observedPanelId: ""\n    property bool observedActive: false')
    insert("FolderExpansionHost.qml", "id: content", '''
        Timer {
            interval: 100; running: true; repeat: true
            onTriggered: {
                if (!root.observedActive) return;
                const items = {};
                function visit(item) {
                    if (!item.visible) return;
                    if (item.objectName.startsWith("folder-child-")) {
                        const point = item.mapToItem(null, 8, 8);
                        items[item.objectName] = [point.x, point.y];
                    }
                    for (const child of item.children) visit(child);
                }
                if (root.visible) visit(content);
                console.warn("ArchDockInteraction " + JSON.stringify({kind: "folder", panel: root.observedPanelId,
                    visible: root.visible, snapshot: root.snapshot, layout: content.geometry.layout,
                    selected: content.selectedChildId, reducedMotion: content.reducedMotion,
                    rect: [root.x, root.y, root.width, root.height], items: items}));
            }
        }
''')
    insert("main.qml", "DockEntry {", "\n            observedPanelId: root.panelId\n            observedPresentation: root.reportedPresentation\n            observedProfile: root.configuration.presentationProfile || ({})")
    insert("DockEntry.qml", "import QtQuick\n", "import QtQuick.Window\n")
    insert("DockEntry.qml", "id: root", '''
    property string observedPanelId: ""
    property var observedPresentation: ({})
    property var observedProfile: ({})
    function observeEvent(event) {
        console.warn("ArchDockInteraction " + JSON.stringify({kind: "event", panel: observedPanelId,
            event: event, allowed: contextInteractionAllowed, presentation: observedPresentation,
            profile: observedProfile.id}));
    }
    Timer {
        interval: 100; running: true; repeat: true
        property int sample: 0
        onTriggered: {
            if (!root.visible || !root.inputEnabled) return;
            const center = root.mapToItem(null, root.width / 2, root.height / 2);
            const actions = {};
            if (contextMenu.visible) {
                for (let i = 0; i < contextMenu.count; ++i) {
                    const item = contextMenu.itemAt(i);
                    if (!item || !item.visible || !item.objectName) continue;
                    const point = item.mapToItem(null, item.width / 2, item.height / 2);
                    actions[item.objectName] = [point.x, point.y];
                }
            }
            const value = JSON.stringify({kind: "entry", panel: root.observedPanelId, sample: ++sample,
                app: root.entry.appId, center: [center.x, center.y],
                hostSize: [root.Window.window.width, root.Window.window.height],
                menuSize: [contextMenu.width, contextMenu.height],
                windows: root.entry.windowPreviews || [], menu: contextMenu.visible,
                presentation: root.observedPresentation, profile: root.observedProfile.id,
                hovered: hoverArea.containsMouse,
                actions: actions});
            console.warn("ArchDockInteraction " + value);
        }
    }
''')
    insert("DockEntry.qml", "onInputEnabledChanged: {", '\n        observeEvent("input:" + inputEnabled);')
    insert("DockEntry.qml", "function openEntryContextMenu() {", '\n        observeEvent("menu-request");')
    insert("DockEntry.qml", "onClicked: mouse => {", '\n            root.observeEvent("click:" + mouse.button);')
    insert("DockEntry.qml", "id: contextMenu", '\n        onAboutToShow: root.observeEvent("menu-show")\n        onAboutToHide: root.observeEvent("menu-hide")')
    insert("WindowPreviewHost.qml", "id: root", '\n    property string observedPanelId: ""\n    property bool observedActive: false')
    insert("WindowPreviewHost.qml", "id: preview", '''
        Timer {
            interval: 100; running: true; repeat: true
            property string previous: ""
            onTriggered: {
                if (!root.observedActive) { previous = ""; return; }
                const items = {};
                function visit(item) {
                    if (!item.visible) return;
                    if (item.objectName) {
                        const point = item.mapToItem(null, item.width / 2, item.height / 2);
                        items[item.objectName] = [point.x, point.y];
                    }
                    for (const child of item.children) visit(child);
                }
                if (root.visible) visit(preview);
                const value = JSON.stringify({kind: "popup", panel: root.observedPanelId,
                    visible: root.visible, windows: root.windowEntries, items: items,
                    rect: [root.x, root.y, root.width, root.height]});
                if (value !== previous) { previous = value; console.warn("ArchDockInteraction " + value); }
            }
        }
''')


def run_interaction_matrix(free_panel):
    import ctypes as c
    import json
    import os
    import time
    import gi
    gi.require_version("Gtk", "4.0")
    from gi.repository import Gio, GLib, Gtk

    app_id = "org.archdock.visibilityfixture"
    root = pathlib.Path(os.environ["ARCHDOCK_RENDERING_SESSION_ROOT"]).resolve(strict=True)
    assert root.parent == pathlib.Path("/tmp") and root.name.startswith("archdock-rendering-import.")
    assert root.stat().st_uid == os.getuid()
    assert pathlib.Path(os.environ["XDG_RUNTIME_DIR"]).resolve() == root / "runtime"
    address = os.environ["DBUS_SESSION_BUS_ADDRESS"]
    assert address != os.environ.get("ARCHDOCK_RENDERING_PARENT_BUS")
    bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)

    def call(destination, path, interface, method, signature="()", args=()):
        reply = bus.call_sync(destination, path, interface, method,
                              GLib.Variant(signature, args), None,
                              Gio.DBusCallFlags.NONE, 5000, None).unpack()
        return reply[0] if len(reply) == 1 else reply

    pid = call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
               "GetConnectionUnixProcessID", "(s)", ("org.kde.KWin",))
    assert pid == int(os.environ["ARCHDOCK_RENDERING_KWIN_PID"])
    proc = pathlib.Path("/proc") / str(pid)
    executable = (proc / "exe").resolve()
    assert executable.name == "kwin_wayland", str(executable)
    environment = dict(part.split(b"=", 1) for part in (proc / "environ").read_bytes().split(b"\0") if b"=" in part)
    for key in ("XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS", "WAYLAND_DISPLAY"):
        assert environment[key.encode()] == os.environ[key].encode(), key

    app = Gtk.Application(application_id=app_id, flags=Gio.ApplicationFlags.NON_UNIQUE)
    assert app.register(None)

    # KWin 6's native EIS API: never open an ambient socket or a uinput device.
    eis_path, eis_interface = "/org/kde/KWin/EIS/RemoteDesktop", "org.kde.KWin.EIS.RemoteDesktop"
    reply, descriptors = bus.call_with_unix_fd_list_sync(
        "org.kde.KWin", eis_path, eis_interface, "connectToEIS",
        GLib.Variant("(i)", (3,)), None, Gio.DBusCallFlags.NONE, 5000, None, None)
    handle, cookie = reply.unpack()
    lib = c.CDLL("libei.so.1")
    pointer = c.c_void_p
    bindings = {
        "new_sender": (pointer, [pointer]), "configure_name": (None, [pointer, c.c_char_p]),
        "setup_backend_fd": (c.c_int, [pointer, c.c_int]), "dispatch": (None, [pointer]),
        "get_event": (pointer, [pointer]), "event_get_type": (c.c_int, [pointer]),
        "event_get_seat": (pointer, [pointer]), "event_get_device": (pointer, [pointer]),
        "event_unref": (pointer, [pointer]), "seat_bind_capabilities": (None, [pointer]),
        "device_has_capability": (c.c_bool, [pointer, c.c_int]),
        "device_ref": (pointer, [pointer]), "device_unref": (pointer, [pointer]),
        "device_start_emulating": (None, [pointer, c.c_uint32]),
        "device_pointer_motion_absolute": (None, [pointer, c.c_double, c.c_double]),
        "device_button_button": (None, [pointer, c.c_uint32, c.c_bool]),
        "device_keyboard_key": (None, [pointer, c.c_uint32, c.c_bool]),
        "device_frame": (None, [pointer, c.c_uint64]), "now": (c.c_uint64, [pointer]),
        "new_ping": (pointer, [pointer]), "ping": (None, [pointer]),
        "ping_unref": (pointer, [pointer]), "unref": (pointer, [pointer]),
    }
    for name, (result, arguments) in bindings.items():
        function = getattr(lib, "ei_" + name)
        function.restype, function.argtypes = result, arguments
    context = lib.ei_new_sender(None)
    lib.ei_configure_name(context, b"Arch Dock private interaction test")
    assert lib.ei_setup_backend_fd(context, descriptors.get(handle)) == 0
    devices = {}
    observations = {}
    events = []
    log = (root / "logs/plasmashell.log").open()
    pongs = 0

    def pump():
        nonlocal pongs
        main_context = GLib.MainContext.default()
        while main_context.pending():
            main_context.iteration(False)
        lib.ei_dispatch(context)
        while event := lib.ei_get_event(context):
            kind = lib.ei_event_get_type(event)
            if kind == 2:
                raise AssertionError("private KWin disconnected the input fixture")
            if kind == 3:
                lib.ei_seat_bind_capabilities(lib.ei_event_get_seat(event), 2, 4, 32, pointer())
            if kind == 8:
                device = lib.ei_event_get_device(event)
                for capability in (2, 4):
                    if lib.ei_device_has_capability(device, capability):
                        assert capability not in devices, "unexpected replacement EIS device"
                        devices[capability] = lib.ei_device_ref(device)
                        lib.ei_device_start_emulating(device, 1)
            if kind == 90:
                pongs += 1
            lib.ei_event_unref(event)
        for line in log:
            if "ArchDockInteraction " in line:
                value = json.loads(line.split("ArchDockInteraction ", 1)[1])
                if value["kind"] == "event":
                    events.append(value)
                    events[:] = events[-24:]
                    continue
                observations[(value["kind"], value["panel"], value.get("app", ""))] = value

    def wait_for(predicate, description):
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            pump()
            if result := predicate():
                return result
            time.sleep(0.02)
        print("Recent input and presentation events:", json.dumps(events), flush=True)
        raise AssertionError(description + ": " + json.dumps(list(observations.values())))

    def sync_input():
        target = pongs + 1
        ping = lib.ei_new_ping(context)
        lib.ei_ping(ping)
        lib.ei_ping_unref(ping)
        wait_for(lambda: pongs >= target, "KWin input acknowledgement")

    def click(point, button=272):
        assert 0 <= point[0] < 1280 and 0 <= point[1] < 720, point
        device = devices[2]
        lib.ei_device_pointer_motion_absolute(device, *point)
        lib.ei_device_frame(device, lib.ei_now(context))
        sync_input()
        for pressed in (True, False):
            lib.ei_device_button_button(device, button, pressed)
            lib.ei_device_frame(device, lib.ei_now(context))
        sync_input()

    def escape():
        key(1)

    def key(code):
        for pressed in (True, False):
            lib.ei_device_keyboard_key(devices[4], code, pressed)
            lib.ei_device_frame(devices[4], lib.ei_now(context))
        sync_input()

    def panel_call(method, signature="()", args=()):
        return call("org.archdock.ArchDock", "/Control", "local.PanelWindow", method, signature, args)

    def kwin_geometry(close_studio_id=""):
        result = []
        interface = Gio.DBusNodeInfo.new_for_xml('''<node><interface name="org.archdock.InteractionProbe">
            <method name="observe"><arg type="s" direction="in"/></method>
            </interface></node>''').interfaces[0]

        def observed(_connection, _sender, _path, _interface, _method, parameters, invocation):
            result.append(json.loads(parameters.unpack()[0]))
            invocation.return_value(None)

        registration = bus.register_object("/InteractionProbe", interface, observed, None, None)
        script = root / "geometry-probe.js"
        script.write_text("const closeId = " + json.dumps(close_studio_id) + ''';
            if (closeId) {
                const studio = workspace.windowList().find(function(w) {
                    return w.internalId.toString() === closeId && w.resourceClass === "arch-dock"
                        && w.caption === "Arch Dock Panel Studio";
                });
                if (studio) studio.closeWindow();
            }
            callDBus(''' + json.dumps(bus.get_unique_name()) + ''',
            "/InteractionProbe", "org.archdock.InteractionProbe", "observe",
            JSON.stringify({cursor: workspace.cursorPos, windows: workspace.windowList().map(function(w) {
                return {id: w.internalId.toString(), caption: w.caption, app: w.resourceClass, layer: w.layer,
                        popup: w.popupWindow, x:w.frameGeometry.x, y:w.frameGeometry.y,
                        width:w.frameGeometry.width, height:w.frameGeometry.height,
                        buffer:[w.bufferGeometry.x,w.bufferGeometry.y,w.bufferGeometry.width,w.bufferGeometry.height]};
            })}));''')
        plugin = "org.archdock.interaction-geometry"
        try:
            script_id = call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting",
                             "loadScript", "(ss)", (str(script), plugin))
            assert script_id >= 0
            call("org.kde.KWin", "/Scripting/Script" + str(script_id), "org.kde.kwin.Script", "run")
            wait_for(lambda: result, "native geometry observation")
            return result[0]
        finally:
            call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript", "(s)", (plugin,))
            bus.unregister_object(registration)
            script.unlink()

    def values(mapping):
        return {key: GLib.Variant("b" if isinstance(value, bool) else "i" if isinstance(value, int) else "s", value)
                for key, value in mapping.items()}

    def native_point(size, point, is_menu=False, edge=None, current_target=None):
        def mapped_window():
            nonlocal size, point
            # Layout changes can resize a visible Dialog after its first QML
            # observation. Match current QML coordinates to current KWin bounds.
            if current_target:
                target = current_target()
                if not target:
                    return None
                size, point = target
            candidates = [window for window in kwin_geometry()["windows"]
                          if window["app"] in ("plasmashell", "org.kde.plasmashell")
                          and window["popup"] == is_menu
                          and abs(window["width"] - size[0]) < 2
                          and abs(window["height"] - size[1]) < 2]
            assert len(candidates) <= 1, (size, candidates)
            if not candidates:
                return None
            window = candidates[0]
            if edge:
                distances = {"left": window["x"], "top": window["y"],
                             "right": 1280 - window["x"] - window["width"],
                             "bottom": 720 - window["y"] - window["height"]}
                if min(distances, key=distances.get) != edge:
                    return None
            return window

        try:
            window = wait_for(mapped_window, "matching compositor window mapped")
        except AssertionError:
            print("Native target mismatch:", json.dumps({"size": size, "point": point,
                  "menu": is_menu, "edge": edge, "actual": kwin_geometry()}), flush=True)
            raise
        assert 0 <= point[0] < size[0] and 0 <= point[1] < size[1], (size, point)
        return [window["x"] + point[0], window["y"] + point[1]]

    def configure(panel, mapping):
        current = panel_call("dockConfiguration", "(s)", (panel,))
        changes = {key: value for key, value in mapping.items() if current.get(key) != value}
        if not changes:
            return
        revision = current["settingsRevision"]
        result = panel_call("applyPanelSettingsTransaction", "(sta{sv}a{sv})",
                            (panel, revision, values(changes), {}))
        assert result["success"], result

    folder_app_ids = {}

    def entry(panel):
        return next((value for (kind, owner, _), value in observations.items()
                     if kind == "entry" and owner == panel
                     and (value["app"] == folder_app_ids.get(panel)
                          or any(row["title"].startswith("Interaction ") for row in value["windows"]))), {})

    def popup(panel):
        return observations.get(("popup", panel, ""), {})

    def opened(panel):
        state = panel_call("panelPresentationState", "(s)", (panel,))
        return (state.get("surfaceState") == "open"
                and state.get("transitionState") == "idle"
                and state.get("hostPhase") == "revealed")

    def click_entry(panel, edge=None, button=273):
        def hovered_target():
            current = entry(panel)
            point = native_point(current["hostSize"], current["center"], edge=edge)
            lib.ei_device_pointer_motion_absolute(devices[2], *point)
            lib.ei_device_frame(devices[2], lib.ei_now(context))
            sync_input()
            observed = wait_for(lambda: entry(panel) if entry(panel).get("sample", 0) > current["sample"] else None,
                                "fresh applet pointer observation")
            return point if observed.get("hovered") and observed["center"] == current["center"] else None

        try:
            point = wait_for(hovered_target, "target applet icon received pointer hover")
        except AssertionError:
            print("Pointer targeting failure:", kwin_geometry(), flush=True)
            raise
        click(point, button)

    def run_folder_matrix():
        import subprocess
        backend_owner = call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                             "GetNameOwner", "(s)", ("org.archdock.ArchDock",))
        folder = root / "Folder fixture"
        folder.mkdir()
        for index in range(5):
            (folder / f"Document {index}.txt").write_text("Private folder acceptance document.\n")
        marker = root / "logs/opened-document"
        handler = root / "document-handler.py"
        handler.write_text("import pathlib, sys\npathlib.Path(" + repr(str(marker))
                           + ").write_text(sys.argv[1])\n")
        desktop_id = "org.archdock.folderfixture.desktop"
        applications = root / "data/applications"
        applications.mkdir(exist_ok=True)
        (applications / desktop_id).write_text(
            "[Desktop Entry]\nType=Application\nName=Private folder document handler\n"
            f"Exec=/usr/bin/python3 {handler} %u\nMimeType=text/plain;inode/directory;\nNoDisplay=true\n")
        (root / "config/mimeapps.list").write_text(
            f"[Default Applications]\ntext/plain={desktop_id};\ninode/directory={desktop_id};\n")
        subprocess.run(["update-desktop-database", str(applications)], check=True)
        assert panel_call("pinDockUrls", "(as)", ([folder.as_uri()],))
        assert panel_call("pinPanelUrls", "(sas)", (free_panel, [folder.as_uri()]))
        for panel in ("bottom", free_panel):
            configure(panel, {"type": "hybrid", "presentationMode": "open", "collapseMechanism": "open",
                              "folderExpandOnClick": True})
            rows = panel_call("dockEntriesForPanel", "(ss)", (panel, "hybrid"))
            folder_app_ids[panel] = next(row["appId"] for row in rows
                if row.get("isFolder") and row["displayName"] == folder.name)

        def folder_popup(panel):
            return observations.get(("folder", panel, ""), {})

        def folder_point(panel, child_name=None):
            def target():
                current = folder_popup(panel)
                if not current.get("visible") or (child_name and child_name not in current["items"]):
                    return None
                return current["rect"][2:], current["items"][child_name] if child_name else [0, 0]
            return native_point([], [], current_target=target)

        layouts = ["fan", "grid", "stack", "arc", "ring"]
        for panel in ("bottom", free_panel):
            for index, layout in enumerate(layouts):
                edge = ["bottom", "top", "left", "right", "bottom"][index] if panel == "bottom" else None
                if edge:
                    vertical = edge in ("left", "right")
                    placed = panel_call("applyNativePanelPlacementDraft", "(sa{sv})", (panel, values({
                        "edge": edge, "dynamic": False, "width": 92 if vertical else 720,
                        "height": 600 if vertical else 92})))
                    assert placed["success"], placed
                    configure(panel, {"layout": "vertical" if vertical else "horizontal",
                                      "panelThemeId": "", "completeThemeId": "", "rendererTier": "procedural2d"})
                else:
                    shape = "ring" if index % 2 == 0 else "arc"
                    theme = "ring-platform-blue" if shape == "ring" else "arc-platform-orange"
                    configure(panel, {"layout": shape, "layoutRadius": 120, "panelThemeId": theme,
                                      "completeThemeId": theme, "rendererTier": "baked2.5d"})
                configure(panel, {"folderLayout": layout, "folderSpeed": 80, "folderEasing": "outCubic"})
                assert panel_call("requestPanelPresentation", "(ss)", (panel, "open"))
                wait_for(lambda: entry(panel) and opened(panel), "folder entry and owning panel ready")
                click_entry(panel, edge, 272)
                current = wait_for(lambda: folder_popup(panel) if folder_popup(panel).get("visible")
                    and folder_popup(panel).get("layout") == layout
                    and len(folder_popup(panel).get("items", {})) == 5 else None, "five real folder children")
                assert current["reducedMotion"] == panel_call("panelRendererConfiguration", "(s)", (panel,))["reducedMotion"]
                assert not marker.exists(), "expanding a folder launched its root"
                wait_for(lambda: panel_call("panelInteractionGuards", "(s)", (panel,)).get("popupOpen"),
                         "folder popup holds the presentation guard")
                assert panel_call("requestPanelPresentation", "(ss)", (panel, "collapse"))
                assert panel_call("panelPresentationState", "(s)", (panel,))["surfaceState"] == "open"
                selected = current["snapshot"]["entries"][0]
                if layout == "stack":
                    key(106)  # Right, then Enter: stacked entries remain keyboard reachable.
                    selected = current["snapshot"]["entries"][1]
                    wait_for(lambda: folder_popup(panel).get("selected") == selected["id"], "keyboard child selection")
                    key(28)
                else:
                    click(folder_point(panel, "folder-child-0"))
                wait_for(lambda: marker.exists(), "controlled document handler opened selected child")
                assert marker.read_text() in (selected["url"], str(folder / selected["name"])), marker.read_text()
                marker.unlink()
                wait_for(lambda: not folder_popup(panel).get("visible"), "selection dismisses folder")
                wait_for(lambda: not panel_call("panelInteractionGuards", "(s)", (panel,)).get("popupOpen"),
                         "folder selection releases guard")
                assert call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                            "GetNameOwner", "(s)", ("org.archdock.ArchDock",)) == backend_owner, "document launch replaced the backend"
                assert panel_call("requestPanelPresentation", "(ss)", (panel, "open"))
                wait_for(lambda: opened(panel), "folder owner reopened")
                click_entry(panel, edge, 272)
                wait_for(lambda: folder_popup(panel).get("visible"), "folder reopened for dismissal")
                if index % 2:
                    current = folder_popup(panel)
                    origin = folder_point(panel)
                    width, height = folder_popup(panel)["rect"][2:]
                    outside = next(point for point in ([20, 400], [1100, 400], [640, 360])
                        if not (origin[0] <= point[0] < origin[0] + width
                                and origin[1] <= point[1] < origin[1] + height))
                    click(outside)
                else:
                    escape()
                wait_for(lambda: not folder_popup(panel).get("visible"), "folder keyboard/outside dismissal")
                assert not marker.exists(), "dismissal launched a document or folder"
                print(f"PASS: folder {panel}/{layout}/{edge}: native selection, private handler, guards, dismissal", flush=True)
        for child in folder.iterdir():
            child.unlink()
        assert panel_call("requestPanelPresentation", "(ss)", (free_panel, "open"))
        wait_for(lambda: opened(free_panel), "empty folder owner opened")
        click_entry(free_panel, button=272)
        wait_for(lambda: folder_popup(free_panel).get("visible")
                 and folder_popup(free_panel).get("snapshot", {}).get("status") == "empty", "empty folder popup")
        escape()
        wait_for(lambda: not folder_popup(free_panel).get("visible"), "empty folder dismissed")
        assert not marker.exists()
        print("PASS: empty folder remains a dismissible popup without launching its root", flush=True)

    first = Gtk.ApplicationWindow(application=app)
    second = Gtk.ApplicationWindow(application=app)
    try:
        wait_for(lambda: 2 in devices and 4 in devices, "native EIS devices ready")
        studio = wait_for(lambda: next((window for window in kwin_geometry()["windows"]
            if window["app"] == "arch-dock" and window["caption"] == "Arch Dock Panel Studio"), None),
            "free dock setup opened private Studio")
        kwin_geometry(studio["id"])
        wait_for(lambda: not any(window["id"] == studio["id"] for window in kwin_geometry()["windows"]),
                 "private Studio closed before desktop input")
        if os.environ.get("ARCHDOCK_RENDERING_FOLDERS") == "1":
            run_folder_matrix()
            return
        marker = root / "logs/unexpected-icon-launch"
        desktop = root / "data/applications" / (app_id + ".desktop")
        desktop.parent.mkdir(exist_ok=True)
        desktop.write_text("[Desktop Entry]\nType=Application\nName=Interaction fixture\n"
                           f"Exec=/usr/bin/touch {marker}\nIcon=applications-system\n")
        assert panel_call("pinDockUrls", "(as)", ([desktop.as_uri()],))
        assert panel_call("pinPanelUrls", "(sas)", (free_panel, [desktop.as_uri()]))
        for index, window in enumerate((first, second)):
            window.set_title("Interaction " + str(index + 1))
            window.set_default_size(250, 180)
            window.present()
        second.minimize()
        wait_for(lambda: first.get_mapped() and second.get_mapped(), "fixture windows mapped")
        assert call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting",
                    "isScriptLoaded", "(s)", ("org.archdock.windowwatcher.runtime",))
        call("org.kde.KWin", "/KWin", "org.kde.KWin", "reconfigure")
        first.set_title("Interaction reload observed")
        wait_for(lambda: any(row["title"] == "Interaction reload observed"
                            for item in panel_call("dockEntries", "(s)", ("tasks",))
                            for row in item["windowPreviews"]), "watcher survives KWin reconfiguration")
        first.set_title("Interaction 1")
        for panel, layout in [("bottom", edge) for edge in ("bottom", "top", "left", "right")] + [
                (free_panel, "ring"), (free_panel, "arc")]:
            if panel == "bottom":
                placed = panel_call("applyNativePanelPlacementDraft", "(sa{sv})",
                                    (panel, values({"edge": layout, "dynamic": False,
                                                    "width": 92 if layout in ("left", "right") else 720,
                                                    "height": 600 if layout in ("left", "right") else 92})))
                assert placed["success"], placed
                shape = "vertical" if layout in ("left", "right") else "horizontal"
                theme = "energy-frame-cyan" if shape == "horizontal" else ""
            else:
                shape = layout
                theme = "ring-platform-blue" if layout == "ring" else "arc-platform-orange"
            configure(panel, {"type": "hybrid", "layout": shape,
                              **({"layoutRadius": 120} if panel == free_panel else {}),
                              "panelThemeId": theme, "completeThemeId": theme,
                              "rendererTier": "baked2.5d" if panel == free_panel else "skinned2d" if theme else "procedural2d",
                              "presentationMode": "collapsed" if theme == "energy-frame-cyan" else "open",
                              **({"presentationTrigger": "manual"} if panel == "bottom" else {}),
                              **({"collapseMechanism": "collapse-horizontal"}
                                 if theme == "energy-frame-cyan" else {})})
            assert panel_call("requestPanelPresentation", "(ss)", (panel, "open"))
            wait_for(lambda: len(entry(panel).get("windows", [])) == 2, "two grouped applet windows")
            wait_for(lambda: opened(panel), "actual applet finished opening")
            click_entry(panel, layout if panel == "bottom" else None)
            wait_for(lambda: entry(panel).get("menu"), "actual context menu opened")
            assert panel_call("requestPanelPresentation", "(ss)", (panel, "collapse"))
            wait_for(lambda: panel_call("panelInteractionGuards", "(s)", (panel,)).get("popupOpen"), "menu guard")
            assert panel_call("panelPresentationState", "(s)", (panel,))["surfaceState"] == "open"
            click(native_point(entry(panel)["menuSize"], entry(panel)["actions"]["showWindowPreviewAction"], True))
            wait_for(lambda: popup(panel).get("visible") and len(popup(panel)["windows"]) == 2
                     and all("window-state-" + row["windowId"] in popup(panel)["items"]
                             for row in popup(panel)["windows"]), "both native preview rows rendered")
            assert panel_call("panelPresentationState", "(s)", (panel,))["surfaceState"] == "open"
            assert not marker.exists(), "menu click launched underlying icon"
            rows = {row["title"]: row for row in popup(panel)["windows"]}
            first_id, second_id = rows["Interaction 1"]["windowId"], rows["Interaction 2"]["windowId"]
            click(native_point(popup(panel)["rect"][2:], popup(panel)["items"]["window-state-" + first_id]))
            wait_for(lambda: any(row["windowId"] == first_id and row["minimized"]
                                 for row in popup(panel)["windows"]), "selected minimize")
            click(native_point(popup(panel)["rect"][2:], popup(panel)["items"]["window-state-" + first_id]))
            wait_for(lambda: any(row["windowId"] == first_id and not row["minimized"]
                                 for row in popup(panel)["windows"]), "selected restore")
            assert any(row["windowId"] == second_id and row["minimized"]
                       for row in popup(panel)["windows"]), "other window changed"
            escape()
            wait_for(lambda: not popup(panel).get("visible"), "keyboard dismissal")
            assert panel_call("requestPanelPresentation", "(ss)", (panel, "open"))
            wait_for(lambda: opened(panel), "reopen")
            click_entry(panel, layout if panel == "bottom" else None)
            wait_for(lambda: entry(panel).get("menu"), "reopened menu")
            origin = native_point(entry(panel)["menuSize"], [0, 0], True)
            width, height = entry(panel)["menuSize"]
            outside = next(point for point in ([20, 400], [1100, 400], [640, 360])
                           if not (origin[0] <= point[0] < origin[0] + width
                                   and origin[1] <= point[1] < origin[1] + height))
            click(outside)
            wait_for(lambda: not entry(panel).get("menu"), "outside-click dismissal")
            assert not marker.exists(), "popup/menu click launched underlying icon"
            print(f"PASS: real applet {panel}/{layout}: grouped rows, exact actions, guards, dismissal, no launch", flush=True)
            if theme == "energy-frame-cyan":
                configure(panel, {"presentationMode": "open", "collapseMechanism": "open"})
            assert panel_call("requestPanelPresentation", "(ss)", (panel, "collapse"))
        def open_final_preview():
            assert panel_call("requestPanelPresentation", "(ss)", (free_panel, "open"))
            wait_for(lambda: opened(free_panel), "final free applet opened")
            click_entry(free_panel)
            wait_for(lambda: entry(free_panel).get("menu"), "final menu opened")
            current = entry(free_panel)
            click(native_point(current["menuSize"], current["actions"]["showWindowPreviewAction"], True))
            wait_for(lambda: popup(free_panel).get("visible")
                     and "window-close-" + second_id in popup(free_panel)["items"], "final preview rendered")

        def fixture_rows():
            return [row for item in panel_call("dockEntries", "(s)", ("tasks",))
                    for row in item["windowPreviews"] if row["windowId"] in (first_id, second_id)]

        open_final_preview()
        running_app_id = next(item["appId"]
                              for item in panel_call("dockEntries", "(s)", ("tasks",))
                              if any(row["windowId"] == first_id for row in item["windowPreviews"]))
        click(native_point(popup(free_panel)["rect"][2:],
                           popup(free_panel)["items"]["window-preview-" + second_id]))
        wait_for(lambda: any(row["windowId"] == second_id and row["active"] and not row["minimized"]
                             for row in fixture_rows()), "selected row activated and restored")
        wait_for(lambda: not popup(free_panel).get("visible"), "selection dismissed preview")
        open_final_preview()
        click(native_point(popup(free_panel)["rect"][2:],
                           popup(free_panel)["items"]["window-close-" + first_id]))
        wait_for(lambda: len(popup(free_panel)["windows"]) == 1
                 and popup(free_panel)["windows"][0]["windowId"] == second_id,
                 "exact close leaves the other window visible")
        assert not panel_call("requestDockWindowAction", "(sss)", (running_app_id, first_id, "close"))
        click(native_point(popup(free_panel)["rect"][2:],
                           popup(free_panel)["items"]["window-close-" + second_id]))
        wait_for(lambda: not fixture_rows() and not popup(free_panel).get("visible"),
                 "last window closes and dismisses preview")
        wait_for(lambda: not panel_call("panelInteractionGuards", "(s)", (free_panel,)).get("popupOpen"),
                 "last window releases popup guard")
        assert not marker.exists(), "window controls launched underlying icon"
        print("PASS: real applet activation, exact close, one/zero windows, stale-ID rejection and guard release", flush=True)
    finally:
        first.close()
        second.close()
        for device in devices.values():
            lib.ei_device_unref(device)
        lib.ei_unref(context)
        call("org.kde.KWin", eis_path, eis_interface, "disconnect", "(i)", (cookie,))
        log.close()


if len(sys.argv) == 3 and sys.argv[1] == "--instrument-interaction-stage":
    instrument_interaction_stage(sys.argv[2])
    raise SystemExit(0)
if len(sys.argv) == 3 and sys.argv[1] == "--interaction-matrix":
    run_interaction_matrix(sys.argv[2])
    raise SystemExit(0)


from PySide6.QtCore import QRect, QTimer, Qt
from PySide6.QtGui import (
    QBackingStore,
    QColor,
    QGuiApplication,
    QPainter,
    QRegion,
    QWindow,
)

FIXTURE_TITLE = "Arch Dock Visibility Fixture"


class VisibilityWindow(QWindow):
    def __init__(self):
        QWindow.__init__(self)
        self._backing_store = QBackingStore(self)

    def render(self):
        if not self.isExposed() or self.width() <= 0 or self.height() <= 0:
            return

        rectangle = QRect(0, 0, self.width(), self.height())
        region = QRegion(rectangle)
        self._backing_store.resize(self.size())
        self._backing_store.beginPaint(region)
        painter = QPainter(self._backing_store.paintDevice())
        painter.fillRect(rectangle, QColor("#20242b"))
        painter.end()
        self._backing_store.endPaint()
        self._backing_store.flush(region)

    def exposeEvent(self, event):
        QWindow.exposeEvent(self, event)
        self.render()

    def resizeEvent(self, event):
        QWindow.resizeEvent(self, event)
        self._backing_store.resize(event.size())
        self.render()


if len(sys.argv) != 3:
    raise SystemExit("usage: visibility-window.py COMMAND_FILE SCREEN_INDEX")

command_file = pathlib.Path(sys.argv[1])
screen_index = int(sys.argv[2])

application = QGuiApplication(sys.argv)
application.setApplicationName("Arch Dock Visibility Fixture")
application.setDesktopFileName("org.archdock.visibilityfixture")

screens = application.screens()
if screen_index < 0 or screen_index >= len(screens):
    raise SystemExit(f"screen index {screen_index} is unavailable")

screen = screens[screen_index]
window = VisibilityWindow()
window.setScreen(screen)
window.setTitle(FIXTURE_TITLE)
window.setFlags(Qt.WindowType.Window | Qt.WindowType.FramelessWindowHint)

last_serial = ""


def normal_geometry():
    geometry = screen.geometry()
    width = min(520, max(240, geometry.width() // 2))
    height = min(320, max(180, geometry.height() // 2))
    return geometry.x() + 32, geometry.y() + 32, width, height


def overlap_geometry():
    geometry = screen.geometry()
    width = min(700, max(360, geometry.width() - 160))
    height = min(240, max(160, geometry.height() // 3))
    return (
        geometry.x() + max(40, (geometry.width() - width) // 2),
        geometry.y() + geometry.height() - height,
        width,
        height,
    )


def activate():
    window.raise_()
    window.requestActivate()


def request_geometry(state, geometry):
    x, y, width, height = geometry
    window.setTitle(
        f"{FIXTURE_TITLE} [{state}:{x}:{y}:{width}:{height}]"
    )
    window.setGeometry(x, y, width, height)


def apply_command(serial, command):
    if command == "quit":
        print(f"STATE:{serial}:quit", flush=True)
        application.quit()
        return

    if command == "normal":
        window.showNormal()
        request_geometry(command, normal_geometry())
    elif command == "overlap":
        window.showNormal()
        request_geometry(command, overlap_geometry())
    elif command == "maximized":
        window.setTitle(f"{FIXTURE_TITLE} [{command}]")
        window.showMaximized()
    elif command == "fullscreen":
        window.setTitle(f"{FIXTURE_TITLE} [{command}]")
        window.showFullScreen()
    else:
        raise RuntimeError(f"unknown fixture command: {command}")

    QTimer.singleShot(50, activate)
    print(f"STATE:{serial}:{command}", flush=True)


def poll_command():
    global last_serial

    try:
        contents = command_file.read_text(encoding="utf-8").strip()
    except FileNotFoundError:
        return
    if not contents:
        return

    parts = contents.split(maxsplit=1)
    if len(parts) != 2 or parts[0] == last_serial:
        return

    last_serial = parts[0]
    try:
        apply_command(parts[0], parts[1])
    except Exception as error:  # The harness treats any fixture exit as a failure.
        print(f"ERROR:{parts[0]}:{error}", flush=True)
        application.exit(1)


timer = QTimer()
timer.setInterval(50)
timer.timeout.connect(poll_command)
timer.start()
poll_command()

raise SystemExit(application.exec())
