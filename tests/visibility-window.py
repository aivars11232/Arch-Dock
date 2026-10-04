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
    insert("main.qml", "function publishPresentationState() {", '''
        console.warn("ArchDockInteraction " + JSON.stringify({kind: "presentation", panel: root.panelId,
            at: Date.now(), state: root.reportedPresentation, hostConcealed: root.hostConcealed}));
''')
    insert("main.qml", "const accepted = dropBackend.addUrls(panelId, freeSurface, values);", '''
        console.warn("ArchDockInteraction " + JSON.stringify({kind: "drop-result", panel: root.panelId,
            accepted: accepted, error: dropBackend.lastError, urls: values, at: Date.now()}));
''')
    insert("main.qml", "function publishHostConcealed() {", '''
                console.warn("ArchDockInteraction " + JSON.stringify({kind: "host-change", panel: root.panelId,
                    at: Date.now(), authoritative: authoritativeHost, concealed: hostConcealed,
                    itemVisible: visible, itemOpacity: opacity, windowVisible: Window.window ? Window.window.visible : null,
                    windowVisibility: Window.visibility, state: root.reportedPresentation}));
''')
    insert("main.qml", "id: panelScene", '''
                Timer {
                    interval: 100; running: true; repeat: true
                    property int sample: 0
                    onTriggered: {
                        if (!representation.authoritativeHost) return;
                        const host = panelScene.Window.window;
                        console.warn("ArchDockInteraction " + JSON.stringify({kind: "host", panel: root.panelId,
                            profile: String((root.configuration.presentationProfile || {}).id || ""),
                            at: Date.now(), itemVisible: representation.visible, itemOpacity: representation.opacity,
                            windowVisible: host.visible, windowVisibility: panelScene.Window.visibility,
                            nativeState: root.nativeHostState,
                            rect: [host.x, host.y, host.width, host.height], concealed: representation.hostConcealed,
                            dropPoint: (() => { const p = panelDropArea.mapToItem(null,
                                panelDropArea.width / 2, panelDropArea.height / 2); return [p.x, p.y]; })(),
                            dropEnabled: panelDropArea.enabled && panelDropArea.visible,
                            rotation: {angle: panelScene.sceneRotationAngle, active: panelScene.sceneRotationActive,
                                wheelAvailable: panelScene.wheelRotationAvailable},
                            collapseProgress: panelScene.collapseProgress,
                            revealPoint: (() => { const r = panelScene.revealHandle;
                                const p = panelScene.mapToItem(null, r.x + r.width / 2, r.y + r.height / 2);
                                return [p.x, p.y]; })(),
                            state: root.reportedPresentation}));
                        if (!panelScene.segmentedScene) return;
                        const segments = [];
                        for (let i = 0; i < panelScene.segmentSurfaces.count; ++i) {
                            const segment = panelScene.segmentSurfaces.itemAt(i);
                            const center = segment.mapToItem(null, segment.width / 2, segment.height / 2);
                            segments.push({id: segment.definition.id, expanded: segment.expanded,
                                color: segment.definition.color, corners: segment.definition.corners,
                                center: [center.x, center.y], width: segment.width, height: segment.height,
                                motion: segment.visualMotion, reducedMotion: segment.reducedMotion});
                        }
                        const entries = [];
                        for (let i = 0; i < panelScene.entryCount; ++i) {
                            const item = panelScene.entryItemAt(i);
                            const visual = item.meshVisualItem;
                            entries.push({app: item.sceneEntry.appId, segment: item.sceneEntry.segmentId,
                                input: item.sceneInputEnabled, visible: item.visible,
                                badge: visual ? visual.badgeText : "",
                                badgeVisible: visual && visual.badgeItem.visible,
                                progress: visual ? visual.progress : -1,
                                progressVisible: visual && visual.progressItem.visible,
                                attention: visual && visual.attentionItem.visible,
                                temporary: visual && visual.temporaryStatusItem.visible,
                                status: visual ? visual.statusTextItem.text : "",
                                statusVisible: visual && visual.statusTextItem.visible});
                        }
                        console.warn("ArchDockInteraction " + JSON.stringify({kind: "segments", panel: root.panelId,
                            sample: ++sample, segments: segments, entries: entries,
                            hostSize: [panelScene.Window.window.width, panelScene.Window.window.height]}));
                    }
                }
''')
    import json
    insert("FolderExpansionHost.qml", "id: root", '\n    property string observedPanelId: ""\n    property bool observedActive: false\n    property string observedCaptureDirectory: '
           + json.dumps(str(stage.parent / 'logs')))
    insert("FolderExpansionHost.qml", "id: content", '''
        Timer {
            interval: 100; running: true; repeat: true
            property string captured: ""
            property var captureStatus: ({})
            onTriggered: {
                if (!root.observedActive) return;
                const items = {};
                const names = {};
                function visit(item) {
                    if (!item.visible) return;
                    if (item.objectName.startsWith("folder-child-")) {
                        const point = item.mapToItem(null, item.width / 2, 28);
                        items[item.objectName] = [point.x, point.y];
                    }
                    if (item.objectName.startsWith("folder-name-"))
                        names[item.objectName] = item.text;
                    for (const child of item.children) visit(child);
                }
                if (root.visible) visit(content);
                const viewport = content.contentItem.children.find(item => item.objectName === "folderViewport");
                const captureKey = root.observedPanelId + "-" + content.geometry.layout
                    + "-" + content.entries.length + "-" + root.showNames;
                if (!root.visible) { captured = ""; captureStatus = {}; }
                if (root.visible && !content.openingInProgress && captured !== captureKey) {
                    captured = captureKey;
                    const path = root.observedCaptureDirectory + "/folder-" + captureKey + ".png";
                    // Qt's QML overload requires a QML-owned item; the
                    // window's C++ contentItem has no QML engine. Capture
                    // the actual mainItem and check native background/color
                    // independently below, without changing its rendering.
                    const started = content.grabToImage(result => {
                        const saved = result.saveToFile(path);
                        captureStatus = {path: path, saved: saved};
                    });
                    captureStatus = {path: path, started: started,
                        width: content.width, height: content.height};
                }
                console.warn("ArchDockInteraction " + JSON.stringify({kind: "folder", panel: root.observedPanelId,
                    active: root.active, focused: content.activeFocus,
                    visible: root.visible, snapshot: root.snapshot, layout: content.geometry.layout,
                    selected: content.selectedChildId, reducedMotion: content.reducedMotion,
                    opening: content.openingInProgress, progress: content.openingProgress,
                    origin: [content.expansionOrigin.x, content.expansionOrigin.y],
                    capture: captureStatus,
                    background: root.backgroundHints, colorAlpha: root.color.a,
                    showNames: root.showNames, names: names,
                    viewport: viewport ? {x: viewport.x, y: viewport.y,
                        width: viewport.width, height: viewport.height,
                        contentX: viewport.contentX, contentY: viewport.contentY,
                        contentWidth: viewport.contentWidth, contentHeight: viewport.contentHeight,
                        dragging: viewport.dragging, moving: viewport.moving} : {},
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
            const value = JSON.stringify({kind: "entry", panel: root.observedPanelId, sample: Date.now(),
                app: root.entry.appId, center: [center.x, center.y],
                iconSource: root.meshVisualItem.resolvedIconSource,
                glyphSize: [root.meshVisualItem.glyphItem.width, root.meshVisualItem.glyphItem.height],
                glyphValid: root.meshVisualItem.glyphItem.valid, meshActive: root.meshVisualActive,
                dragging: root.dragging,
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
        "device_scroll_delta": (None, [pointer, c.c_double, c.c_double]),
        "device_scroll_discrete": (None, [pointer, c.c_int32, c.c_int32]),
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
    host_trace = []
    log = (root / "logs/plasmashell.log").open()
    log_pending = ""
    pongs = 0

    def pump():
        nonlocal pongs, log_pending
        main_context = GLib.MainContext.default()
        while main_context.pending():
            main_context.iteration(False)
        lib.ei_dispatch(context)
        while event := lib.ei_get_event(context):
            kind = lib.ei_event_get_type(event)
            if kind == 2:
                raise AssertionError("private KWin disconnected the input fixture")
            if kind == 3:
                lib.ei_seat_bind_capabilities(lib.ei_event_get_seat(event), 2, 4, 16, 32, pointer())
            if kind == 8:
                device = lib.ei_event_get_device(event)
                for capability in (2, 4, 16):
                    if lib.ei_device_has_capability(device, capability):
                        assert capability not in devices, "unexpected replacement EIS device"
                        devices[capability] = lib.ei_device_ref(device)
                        lib.ei_device_start_emulating(device, 1)
            if kind == 90:
                pongs += 1
            lib.ei_event_unref(event)
        # A live file can reach EOF halfway through a logger write. Parse only
        # newline-terminated records, retaining the unfinished suffix verbatim.
        lines = (log_pending + log.read()).split("\n")
        log_pending = lines.pop()
        for line in lines:
            if "ArchDockInteraction " in line:
                value = json.loads(line.split("ArchDockInteraction ", 1)[1])
                if value["kind"] in ("host-change", "presentation"):
                    host_trace.append(value)
                    host_trace[:] = host_trace[-80:]
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
        folders = [key for key, row in observations.items()
                   if key[0] == "folder" and row.get("visible")]
        if folders:
            # QWindow visibility precedes activation and content focus on
            # Wayland. A key sent during that interval targets the old focus.
            wait_for(lambda: all(observations[key].get("active") and observations[key].get("focused")
                                 for key in folders), "folder keyboard focus ready")
        for pressed in (True, False):
            lib.ei_device_keyboard_key(devices[4], code, pressed)
            lib.ei_device_frame(devices[4], lib.ei_now(context))
        sync_input()

    def panel_call(method, signature="()", args=()):
        return call("org.archdock.ArchDock", "/Control", "local.PanelWindow", method, signature, args)

    def motion(point):
        assert 0 <= point[0] < 1280 and 0 <= point[1] < 720, point
        lib.ei_device_pointer_motion_absolute(devices[2], *point)
        lib.ei_device_frame(devices[2], lib.ei_now(context))
        sync_input()

    def wheel(point, x, y, discrete=False, shift=False):
        motion(point)
        if shift:
            lib.ei_device_keyboard_key(devices[4], 42, True)
            lib.ei_device_frame(devices[4], lib.ei_now(context))
            sync_input()
        try:
            function = lib.ei_device_scroll_discrete if discrete else lib.ei_device_scroll_delta
            function(devices[16], x, y)
            lib.ei_device_frame(devices[16], lib.ei_now(context))
            sync_input()
        finally:
            if shift:
                lib.ei_device_keyboard_key(devices[4], 42, False)
                lib.ei_device_frame(devices[4], lib.ei_now(context))
                sync_input()

    def drag(start, end):
        motion(start)
        lib.ei_device_button_button(devices[2], 272, True)
        lib.ei_device_frame(devices[2], lib.ei_now(context))
        sync_input()
        try:
            for step in range(1, 21):
                motion([start[axis] + (end[axis] - start[axis]) * step / 20 for axis in (0, 1)])
                pump()
                time.sleep(0.015)
        finally:
            lib.ei_device_button_button(devices[2], 272, False)
            lib.ei_device_frame(devices[2], lib.ei_now(context))
            sync_input()

    geometry = {"sample": 0, "value": {}, "close": "", "registration": 0}
    geometry_plugin = "org.archdock.interaction-geometry"
    geometry_script = root / "geometry-probe.js"

    def kwin_geometry(close_studio_id="", activate_drag_source=False):
        if not geometry["registration"]:
            interface = Gio.DBusNodeInfo.new_for_xml('''<node><interface name="org.archdock.InteractionProbe">
                <method name="observe"><arg type="s" direction="in"/><arg type="s" direction="out"/></method>
                </interface></node>''').interfaces[0]

            def observed(_connection, _sender, _path, _interface, _method, parameters, invocation):
                geometry["value"] = json.loads(parameters.unpack()[0])
                geometry["sample"] += 1
                invocation.return_value(GLib.Variant("(s)", (geometry["close"],)))
                geometry["close"] = ""

            geometry["registration"] = bus.register_object("/InteractionProbe", interface, observed, None, None)
            # KWin allocates script IDs from its live count. Keep one observer
            # throughout the matrix rather than repeatedly recycling IDs.
            geometry_script.write_text('''function observe() {
                callDBus(''' + json.dumps(bus.get_unique_name()) + ''',
                "/InteractionProbe", "org.archdock.InteractionProbe", "observe",
                JSON.stringify({cursor: workspace.cursorPos, windows: workspace.windowList().map(function(w) {
                    return {id: w.internalId.toString(), caption: w.caption, app: w.resourceClass, layer: w.layer,
                            popup: w.popupWindow, hidden: w.hidden, dock: w.dock, active: w.active,
                            x:w.frameGeometry.x, y:w.frameGeometry.y,
                            width:w.frameGeometry.width, height:w.frameGeometry.height,
                            buffer:[w.bufferGeometry.x,w.bufferGeometry.y,w.bufferGeometry.width,w.bufferGeometry.height]};
                })}), function(closeId) {
                    if (!closeId) return;
                    if (closeId === "focus-owned-drag-source") {
                        const source = workspace.windowList().find(function(w) {
                            return w.resourceClass === "org.archdock.visibilityfixture"
                                && w.caption === "Runtime drag source";
                        });
                        if (source) workspace.activeWindow = source;
                        return;
                    }
                    const studio = workspace.windowList().find(function(w) {
                        return w.internalId.toString() === closeId && w.resourceClass === "arch-dock"
                            && w.caption === "Arch Dock Panel Studio";
                    });
                    if (studio) studio.closeWindow();
                });
            }
            const timer = new QTimer();
            timer.interval = 50;
            timer.timeout.connect(observe);
            timer.start();
            observe();''')
            script_id = call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting",
                             "loadScript", "(ss)", (str(geometry_script), geometry_plugin))
            assert script_id >= 0
            call("org.kde.KWin", "/Scripting/Script" + str(script_id), "org.kde.kwin.Script", "run")
        geometry["close"] = "focus-owned-drag-source" if activate_drag_source else close_studio_id
        previous = geometry["sample"]
        wait_for(lambda: geometry["sample"] > previous, "native geometry observation")
        return geometry["value"]

    def values(mapping):
        def variant(value):
            if isinstance(value, dict):
                return GLib.Variant("a{sv}", values(value))
            if isinstance(value, list):
                return GLib.Variant("av", [variant(item) for item in value])
            return GLib.Variant("b" if isinstance(value, bool) else "i" if isinstance(value, int) else "s", value)
        return {key: variant(value) for key, value in mapping.items()}

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
        if changes:
            revision = current["settingsRevision"]
            result = panel_call("applyPanelSettingsTransaction", "(sta{sv}a{sv})",
                                (panel, revision, values(changes), {}))
            assert result["success"], result
        # The backend transaction precedes the applet's asynchronous refresh.
        # An old "open" report cannot acknowledge the new presentation profile:
        # applying that profile resets the controller and can interrupt input.
        expected = panel_call("panelRendererConfiguration", "(s)", (panel,))["presentationProfile"]["id"]
        wait_for(lambda: observations.get(("host", panel, ""), {}).get("profile") == expected,
                 "applet applied the configured presentation profile")

    def visibility_discriminator():
        def evidence():
            configuration = panel_call("dockConfiguration", "(s)", ("bottom",))
            native_id = int(configuration["nativePanelId"])
            native = call("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript", "(s)",
                (f"var p = panelById({native_id}); p.currentConfigGroup = ['ArchDock']; print(JSON.stringify({{hiding:p.hiding, "
                 "temporaryHidden:p.readConfig('temporaryHidden','0'), widgets:p.widgets().map(w => w.id).sort()}));",))
            return {"time": time.monotonic(), "savedVisible": configuration["visible"],
                    "native": native, "compositor": kwin_geometry(),
                    "host": observations.get(("host", "bottom", "")),
                    "report": panel_call("panelPresentationState", "(s)", ("bottom",)),
                    "visibility": panel_call("nativePanelVisibilityStatus", "(s)", ("bottom",)),
                    "content": panel_call("contentRuntimeSnapshot", "(s)", ("bottom",))}

        before = evidence()
        host_trace.clear()
        succeeded = panel_call("setPanelVisible", "(sb)", ("bottom", False))
        try:
            assert succeeded
            wait_for(lambda: not panel_call("contentRuntimeSnapshot", "(s)", ("bottom",))["visible"]
                     and panel_call("panelPresentationState", "(s)", ("bottom",)).get("hostPhase") == "concealed",
                     "last status consumer hidden by native host")
        finally:
            after = evidence()
            print("Visibility discriminator: " + json.dumps({"before": before, "apiSucceeded": succeeded,
                  "after": after, "trace": host_trace}), flush=True)
        observed = after["host"]["nativeState"]
        assert observed["available"]
        actual = next(window for window in after["compositor"]["windows"] if window["id"] == observed["windowId"])
        assert actual["dock"] and actual["hidden"], "lifecycle reported concealment before KWin actually hid the dock"
        assert json.loads(before["native"])["widgets"] == json.loads(after["native"])["widgets"]
        concealed_reports = [row for row in host_trace if row["kind"] == "presentation"
                             and row["panel"] == "bottom" and row["state"]["hostPhase"] == "concealed"]
        assert len(concealed_reports) == 1, concealed_reports
        print(f"PASS: native hide -> KWin hidden -> one concealed report in {after['time'] - before['time']:.3f}s; widget IDs unchanged", flush=True)
        return observed["windowId"]

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
            # native_point() pumps compositor/QML observations before the
            # movement. Only a sample after the EIS acknowledgement can prove
            # this pointer target is hovered, particularly after an edge move.
            acknowledged_sample = entry(panel).get("sample", 0)
            observed = wait_for(lambda: entry(panel) if entry(panel).get("sample", 0) > acknowledged_sample else None,
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
        import shutil
        from PySide6.QtGui import QImage
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

        def verify_capture(panel, count, names):
            layout = folder_popup(panel)["layout"]
            path = root / "logs" / f"folder-{panel}-{layout}-{count}-{str(names).lower()}.png"
            wait_for(lambda: path.exists()
                and folder_popup(panel).get("capture", {}).get("path") == str(path)
                and folder_popup(panel).get("capture", {}).get("saved") is True,
                "native folder pixels captured and saved")
            image = QImage(str(path))
            assert not image.isNull() and image.hasAlphaChannel(), path
            transparent = sum(image.pixelColor(x, y).alpha() == 0
                for y in range(image.height()) for x in range(image.width()))
            painted = sum(image.pixelColor(x, y).alpha() > 0
                for y in range(image.height()) for x in range(image.width()))
            assert transparent > image.width() * image.height() / 3 and painted > 100, (path, transparent, painted)
            evidence = os.environ.get("ARCHDOCK_SCENE_EVIDENCE_DIR")
            if evidence:
                target = pathlib.Path(evidence)
                target.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, target / path.name)
            return path

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
                    and not folder_popup(panel).get("opening")
                    and folder_popup(panel).get("layout") == layout
                    and len(folder_popup(panel).get("items", {})) == 5 else None, "five real folder children")
                assert current["reducedMotion"] == panel_call("panelRendererConfiguration", "(s)", (panel,))["reducedMotion"]
                assert current["background"] == 0 and current["colorAlpha"] == 0, current
                origin = folder_point(panel)
                assert 0 <= origin[0] <= 1280 - current["rect"][2], (origin, current["rect"])
                assert 0 <= origin[1] <= 720 - current["rect"][3], (origin, current["rect"])
                assert current["showNames"] and len(current["names"]) == 5, current
                verify_capture(panel, 5, True)
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
        for index in range(5, 48):
            (folder / f"Document {index}.txt").write_text("Private folder scroll document.\n")
        wait_for(lambda: 16 in devices, "native folder scroll capability ready")
        for panel in ("bottom", free_panel):
            configure(panel, {"folderLayout": "fan", "folderShowNames": True})
            assert panel_call("requestPanelPresentation", "(ss)", (panel, "open"))
            wait_for(lambda: opened(panel), "dense folder owner open")
            click_entry(panel, button=272)
            current = wait_for(lambda: folder_popup(panel) if folder_popup(panel).get("visible")
                and not folder_popup(panel).get("opening") and len(folder_popup(panel).get("items", {})) == 48
                else None, "48 real folder children open")
            assert current["viewport"]["contentWidth"] <= current["viewport"]["width"], current
            assert current["viewport"]["contentHeight"] > current["viewport"]["height"], current
            verify_capture(panel, 48, True)
            point = folder_point(panel, "folder-child-0")
            # EIS positive Y scrolls down, like the existing Studio probe;
            # QtTest's synthetic angleDelta uses the opposite sign.
            wheel(point, 0, 120, discrete=True)
            wait_for(lambda: folder_popup(panel)["viewport"]["contentY"] > 0, "real downward folder wheel")
            wheel(point, 0, -120, discrete=True)
            wait_for(lambda: folder_popup(panel)["viewport"]["contentY"] == 0, "real upward folder wheel")
            start = folder_point(panel, "folder-child-0")
            drag(start, [start[0], start[1] - 100])
            wait_for(lambda: folder_popup(panel)["viewport"]["contentY"] > 0, "real held-pointer folder drag")
            assert not marker.exists() and folder_popup(panel)["visible"], "scroll drag opened a child"
            escape()
            wait_for(lambda: not folder_popup(panel).get("visible"), "dense folder dismissed")
            configure(panel, {"folderShowNames": False})
            click_entry(panel, button=272)
            current = wait_for(lambda: folder_popup(panel) if folder_popup(panel).get("visible")
                and not folder_popup(panel).get("opening") and not folder_popup(panel).get("showNames") else None,
                "persisted names off reaches the real folder host")
            assert not current["names"], current
            verify_capture(panel, 48, False)
            assert panel_call("dockConfiguration", "(s)", (panel,))["folderShowNames"] is False
            escape()
            wait_for(lambda: not folder_popup(panel).get("visible"), "names-off folder dismissed")
            configure(panel, {"folderShowNames": True})
            assert not marker.exists()
            print(f"PASS: {panel}: transparent native folder pixels, 48-item vertical wheel/held drag, no launch, names on/off", flush=True)
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

        other = root / "Other segment folder"
        other.mkdir()
        assert panel_call("pinDockUrls", "(as)", ([other.as_uri()],))
        assert panel_call("pinPanelUrls", "(sas)", (free_panel, [other.as_uri()]))
        for panel in ("bottom", free_panel):
            baseline = {"id": "main", "source": "inherited", "order": 0, "entryIds": [],
                        "background": "solid", "color": "#226688", "corners": "square",
                        "padding": 8, "spacing": 4, "presentation": "open", "motionProfile": ""}
            files = {"id": "files", "source": "custom", "order": 1,
                     "entryIds": [folder_app_ids[panel]], "background": "solid", "color": "#883322",
                     "corners": "capsule", "padding": 20, "spacing": 12,
                     "presentation": "closed", "motionProfile": "pulse"}
            configure(panel, {"layout": "horizontal", "rendererTier": "procedural2d",
                              "panelThemeId": "", "completeThemeId": "", "presentationMode": "open",
                              "collapseMechanism": "open", "segments": [baseline, files]})
            assert panel_call("requestPanelPresentation", "(ss)", (panel, "open"))

            def segmented():
                return observations.get(("segments", panel, ""), {})

            state = wait_for(lambda: segmented() if len(segmented().get("segments", [])) == 2 else None,
                             "native/free independent segment surfaces")
            assert [item["id"] for item in state["segments"]] == ["main", "files"]
            assert [item["color"] for item in state["segments"]] == ["#226688", "#883322"]
            assert len({item["app"] for item in state["entries"]}) == len(state["entries"])
            assert next(item for item in state["entries"] if item["app"] == folder_app_ids[panel])["segment"] == "files"
            click([1200, 500])
            wait_for(lambda: not segmented()["segments"][1]["expanded"], "closed segment rests independently")
            assert not next(item for item in segmented()["entries"] if item["app"] == folder_app_ids[panel])["input"]

            def segment_target():
                state = segmented()
                return state["hostSize"], state["segments"][1]["center"]

            point = native_point(*segment_target(), current_target=segment_target)
            previous_entry_sample = entry(panel).get("sample", 0)
            lib.ei_device_pointer_motion_absolute(devices[2], *point)
            lib.ei_device_frame(devices[2], lib.ei_now(context))
            sync_input()
            wait_for(lambda: segmented()["segments"][1]["expanded"], "native pointer opens the closed segment")
            wait_for(lambda: next(item for item in segmented()["entries"]
                if item["app"] == folder_app_ids[panel])["input"], "opened segment enables its entry")
            wait_for(lambda: entry(panel).get("sample", 0) > previous_entry_sample,
                     "fresh entry geometry after segment opened")
            click_entry(panel, button=272)
            wait_for(lambda: folder_popup(panel).get("visible"), "segmented folder opens through real input")
            lib.ei_device_pointer_motion_absolute(devices[2], 1200, 500)
            lib.ei_device_frame(devices[2], lib.ei_now(context))
            sync_input()
            pump()
            assert segmented()["segments"][1]["expanded"], "popup must hold its segment open"
            escape()
            wait_for(lambda: not folder_popup(panel).get("visible"), "segmented folder dismissed")
            wait_for(lambda: not segmented()["segments"][1]["expanded"], "segment guard releases after dismissal")
            files["order"], baseline["order"] = 0, 1
            files["color"] = "#447722"
            configure(panel, {"segments": [files, baseline]})
            wait_for(lambda: segmented()["segments"][0]["id"] == "files"
                     and segmented()["segments"][0]["color"] == "#447722", "segment reorder and style applied")
            snapshot = panel_call("panelSettingsEditorSnapshot", "(ss)", (panel, "studio"))
            assert snapshot["panelValues"]["segments"][0]["id"] == "files"
            before = snapshot["revision"]
            invalid = dict(files, entryIds=["foreign-entry"])
            rejected = panel_call("applyPanelSettingsTransaction", "(sta{sv}a{sv})",
                                  (panel, before, values({"segments": [invalid, baseline]}), {}))
            assert not rejected["success"], rejected
            assert panel_call("panelSettingsEditorSnapshot", "(ss)", (panel, "studio"))["revision"] == before
            assert not marker.exists(), "segment changes launched a folder root"
            print(f"PASS: segments {panel}: independent surfaces, ownership, native hover, popup guard, reorder, rejection", flush=True)

    def run_content_matrix():
        import subprocess

        panels = ("bottom", free_panel)
        native_id = int(panel_call("dockConfiguration", "(s)", ("bottom",))["nativePanelId"])

        def native_widgets():
            return call("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript", "(s)",
                        (f"print(JSON.stringify(panelById({native_id}).widgets().map(w => w.id).sort()));",))

        widgets_before = native_widgets()
        desktop = root / "data/applications/org.archdock.overlayfixture.desktop"
        desktop.write_text("[Desktop Entry]\nType=Application\nName=Private overlay fixture\n"
                           "Exec=/usr/bin/true\nIcon=applications-system\n")
        subprocess.run(["kbuildsycoca6", "--noincremental"], check=True, stdout=subprocess.DEVNULL)
        assert panel_call("pinDockUrls", "(as)", ([desktop.as_uri()],))
        assert panel_call("pinPanelUrls", "(sas)", (free_panel, [desktop.as_uri()]))
        app_ids = {}

        def snapshot(panel):
            return panel_call("panelSettingsEditorSnapshot", "(ss)", (panel, "studio"))

        def fields(panel):
            return {row["key"]: row for row in snapshot(panel)["panelFields"]}

        def runtime(panel="bottom"):
            return panel_call("contentRuntimeSnapshot", "(s)", (panel,))

        def rows(panel):
            return observations.get(("segments", panel, ""), {}).get("entries", [])

        def overlay(panel):
            return next((row for row in rows(panel) if row["app"] == app_ids[panel]), {})

        for panel in panels:
            assert "showBadges" not in fields(panel), "unsupported source control advertised"
            entries = panel_call("dockEntriesForPanel", "(ss)", (panel, "hybrid"))
            assert not any(row.get("isStatus") for row in entries), "status must be explicitly selected"
            app_ids[panel] = next(row["appId"] for row in entries if row["displayName"] == "Private overlay fixture")
            supported = runtime(panel)["availableSources"]
            assert {"status:cpu", "status:memory"} <= set(supported)
            offered = {row["appId"] for row in fields(panel)["segments"]["availableEntries"] if row.get("isStatus")}
            assert offered == set(supported), "unavailable hardware was advertised"
            segments = snapshot(panel)["panelValues"]["segments"]
            segments[0]["presentation"] = "open"
            segments.append(dict(segments[1], id="readings", source="status", order=2,
                                 entryIds=["status:cpu", "status:memory"]))
            configure(panel, {"segments": segments})

        sender = Gio.DBusConnection.new_for_address_sync(address,
            Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION,
            None, None)

        def update(count, progress, urgent=True):
            sender.emit_signal(None, "/Overlay", "com.canonical.Unity.LauncherEntry", "Update",
                GLib.Variant("(sa{sv})", ("application://" + desktop.name, {
                    "count": GLib.Variant("x", count), "count-visible": GLib.Variant("b", True),
                    "progress": GLib.Variant("d", progress), "progress-visible": GLib.Variant("b", True),
                    "urgent": GLib.Variant("b", urgent)})))

        try:
            update(7, 0.42)
            sender.flush_sync(None)
            for panel in panels:
                wait_for(lambda: overlay(panel).get("badge") == "7" and overlay(panel).get("badgeVisible")
                         and overlay(panel).get("progress") == 0.42 and overlay(panel).get("progressVisible")
                         and overlay(panel).get("attention"), "native/free live overlay layers")
                wait_for(lambda: len([row for row in rows(panel) if row["app"].startswith("status:")
                         and row["statusVisible"] and row["status"].endswith("%")]) == 2, "real status readings rendered")
                assert len({row["app"] for row in rows(panel)}) == len(rows(panel)), "duplicate segment ownership"
                assert next(row for row in rows(panel) if row["app"] == folder_app_ids[panel])["segment"] == "files"
                click_entry(panel, button=272)
                wait_for(lambda: observations.get(("folder", panel, ""), {}).get("visible"),
                         "folder opens alongside status and application overlays")
                escape()
                wait_for(lambda: not observations.get(("folder", panel, ""), {}).get("visible"), "combined folder dismissed")
            assert native_widgets() == widgets_before, "status selection duplicated native applets"

            outcome = panel_call("activateDockEntryOutcome", "(s)", (app_ids["bottom"],))
            assert outcome["outcome"] == "succeeded", outcome
            for panel in panels:
                wait_for(lambda: overlay(panel).get("temporary"), "actual launch outcome rendered")
                preferences = {"showBadges": False, "showProgress": False, "showTemporaryStatus": False}
                configure(panel, preferences)
                wait_for(lambda: overlay(panel) and not any(overlay(panel).get(key)
                         for key in ("badgeVisible", "progressVisible", "temporary")), "overlay preferences applied")
                assert all(snapshot(panel)["panelValues"][key] is False for key in preferences)
                configure(panel, {key: True for key in preferences})

            before, started = runtime()["contentRevision"], time.monotonic()
            for index in range(100):
                update(index, index / 100.0)
            sender.flush_sync(None)
            wait_for(lambda: all(overlay(panel).get("badge") == "99" and overlay(panel).get("progress") == 0.99
                     for panel in panels), "burst coalesced to latest supported source values")
            changes = runtime()["contentRevision"] - before
            assert 0 < changes < 100 and changes <= (time.monotonic() - started) * 10 + 3, changes
            print(f"PASS: native/free combined content, persisted overlay controls, no applet duplicates; 100 updates/{changes} revisions", flush=True)

            click([1200, 500])
            # Procedural panels support linear collapse, but not split/shutter
            # mechanisms. Keep that rejection, then exercise native host hiding. The visible
            # free host remains an overlay consumer but opts out of status.
            current = snapshot("bottom")
            rejected = panel_call("applyPanelSettingsTransaction", "(sta{sv}a{sv})", ("bottom", current["revision"],
                values({"presentationMode": "collapsed", "collapseMechanism": "split"}), {}))
            assert not rejected["success"] and rejected["errorCode"] == "capability-unavailable", rejected
            assert snapshot("bottom")["revision"] == current["revision"]
            free_segments = snapshot(free_panel)["panelValues"]["segments"]
            configure(free_panel, {"segments": free_segments[:2]})
            visibility_discriminator()
            # Allow an already queued worker result to settle before measuring.
            time.sleep(0.25)
            concealed = runtime()["sampleCount"]
            update(155, 0.55, False)
            sender.flush_sync(None)
            until = time.monotonic() + 2.2
            while time.monotonic() < until:
                pump()
                time.sleep(0.02)
            assert runtime()["sampleCount"] == concealed, "concealed status polling continued"
            assert overlay("bottom")["badge"] == "99", "concealed host consumed overlay refreshes"
            assert overlay(free_panel)["badge"] == "155", "visible free host lost live updates"
            assert panel_call("setPanelVisible", "(sb)", ("bottom", True))
            configure(free_panel, {"segments": free_segments})
            wait_for(lambda: all(opened(panel) and overlay(panel).get("badgeVisible")
                     and overlay(panel).get("badge") == "155" and overlay(panel).get("progress") == 0.55
                     and not overlay(panel).get("attention") for panel in panels), "reveal renders latest content")
            wait_for(lambda: runtime()["sampleCount"] > concealed, "revealed status polling resumed")
            print("PASS: hidden native host defers overlays while visible free host updates; no visible status consumer pauses sampling; reveal resumes", flush=True)
        finally:
            sender.close_sync(None)
        for panel in panels:
            wait_for(lambda: overlay(panel) and not overlay(panel).get("badgeVisible")
                     and not overlay(panel).get("progressVisible") and not overlay(panel).get("attention"),
                     "disconnected source clears real overlay layers")
            assert not {"showBadges", "showProgress"} & fields(panel).keys(), "unavailable controls remain exposed"
            wait_for(lambda: not overlay(panel).get("temporary"), "temporary backend outcome expires")
        print("PASS: source disconnect removes native/free feedback and controls; temporary status expires", flush=True)

    def run_runtime_ui_matrix():
        import subprocess
        from urllib.parse import unquote
        gi.require_version("Gdk", "4.0")
        from gi.repository import Gdk
        from PySide6.QtGui import QColor, QImage

        wait_for(lambda: 16 in devices, "native scroll capability ready")
        probe_log = (root / "logs/ui-input-probe.log").open("w")
        environment = dict(os.environ, ARCHDOCK_NATIVE_UI_PROBE="1", QT_QUICK_CONTROLS_STYLE="org.kde.desktop",
                           QT_NO_XDG_DESKTOP_PORTAL="1", XDG_DATA_HOME=str(root / "ui-data"))
        with (root / "logs/icon-tiles-native.log").open("w") as tile_log:
            tiles = subprocess.run([str(pathlib.Path(os.environ["ARCHDOCK_BUILD_DIR"]) / "panel-window-capability-test"),
                                    "studioIconTiles", "studioFolderItemNames", "studioPlainSurfaceExplains3D", "studioPanelMotionControls", "tiltEditorsFollowTheSelectedRenderer"],
                                   env=environment, stdout=tile_log, stderr=subprocess.STDOUT, timeout=60)
            assert tiles.returncode == 0, ("native Icon Tiles edit/Cancel/Apply/persistence failed\n"
                + (root / "logs/icon-tiles-native.log").read_text())
        probe = subprocess.Popen([str(pathlib.Path(os.environ["ARCHDOCK_BUILD_DIR"]) / "panel-window-capability-test"),
                                  "studioPageWheelInput"], env=environment, stdout=probe_log, stderr=subprocess.STDOUT)
        def ui():
            path = root / "ui-probe.json"
            return json.loads(path.read_text()) if path.exists() else {}
        def ui_point(local):
            window = wait_for(lambda: next((w for w in kwin_geometry()["windows"]
                if w["caption"] == "Arch Dock UI input probe"), None), "native Studio probe window")
            assert window["buffer"][2:] == ui()["size"], (window, ui())
            return [window["buffer"][axis] + local[axis] for axis in (0, 1)]
        try:
            wait_for(lambda: ui().get("controls"), "production Studio controls observed")
            before = ui()
            page_start = before
            wheel(ui_point(before["header"]), 0, 32)
            wait_for(lambda: ui()["outerY"] > before["outerY"] or ui()["innerY"] > before["innerY"],
                     "Wayland vertical page wheel")
            for _ in range(4):
                wheel(ui_point(ui()["header"]), 0, -120, discrete=True)
                time.sleep(0.08)
                if ui()["outerY"] == page_start["outerY"] and ui()["innerY"] == page_start["innerY"]:
                    break
            wait_for(lambda: ui()["outerY"] == page_start["outerY"] and ui()["innerY"] == page_start["innerY"],
                     "reverse Wayland page wheel restores the visible tab strip")
            for name, direction in (("nextTabs", 1), ("previousTabs", -1)):
                before = ui()
                assert before[name]["enabled"], before
                motion(ui_point(before[name]["point"]))
                lib.ei_device_button_button(devices[2], 272, True)
                lib.ei_device_frame(devices[2], lib.ei_now(context))
                sync_input()
                lib.ei_device_button_button(devices[2], 272, False)
                lib.ei_device_frame(devices[2], lib.ei_now(context))
                sync_input()
                wait_for(lambda: direction * (ui()["tabX"] - before["tabX"]) > 0,
                         "Wayland tab navigation arrow " + name)
                assert ui()["selection"] == 0, ui()
            before = ui()
            wheel(ui_point(before["tabs"]), 120, 0, discrete=True)
            wait_for(lambda: ui()["tabX"] > before["tabX"], "Wayland horizontal tab wheel")
            before = ui()
            wheel(ui_point(before["tabs"]), 0, 120, discrete=True, shift=True)
            wait_for(lambda: ui()["tabX"] > before["tabX"], "Wayland Shift-wheel tab scrolling")
            assert ui()["selection"] == 0, ui()
            for prefix in ("studio-spin-", "studio-combo-"):
                def visible_control():
                    state = ui()
                    vx, vy, vw, vh = state["viewport"]
                    return next((row for row in state["controls"] if row["name"].startswith(prefix)
                        and vx <= row["point"][0] < vx + vw and vy <= row["point"][1] < vy + vh), None)
                for _ in range(10):
                    if visible_control(): break
                    wheel(ui_point(ui()["header"]), 0, 35)
                    pump(); time.sleep(0.07)
                control = wait_for(visible_control, "visible native " + prefix)
                before = ui()
                wheel(ui_point(control["point"]), 0, 120, discrete=True)
                wait_for(lambda: ui()["outerY"] != before["outerY"] or ui()["innerY"] != before["innerY"],
                         "native control wheel scrolls the page")
                assert next(row for row in ui()["controls"] if row["name"] == control["name"])["value"] == control["value"]
            (root / "ui-probe.done").touch()
            assert probe.wait(timeout=5) == 0, (root / "logs/ui-input-probe.log").read_text()
            print("PASS: real Wayland tab arrows, vertical, horizontal, Shift-wheel and SpinBox/ComboBox input preserve settings", flush=True)
        finally:
            print("Native Studio final state:", json.dumps(ui()), flush=True)
            if probe.poll() is None:
                probe.terminate()
                try: probe.wait(timeout=3)
                except subprocess.TimeoutExpired: probe.kill(); probe.wait(timeout=3)
            probe_log.close()

        desktop = root / "data/applications" / (app_id + ".desktop")
        desktop.parent.mkdir(exist_ok=True)
        desktop.write_text("[Desktop Entry]\nType=Application\nName=Runtime drag fixture\nExec=/usr/bin/true\nIcon=applications-system\n")
        folder = root / "Custom folder"
        folder.mkdir()
        icon = root / "custom icon.png"
        # Qt writes this trusted fixture without a nested Glycin sandbox,
        # which cannot start inside the installed-payload overlay namespace.
        image = QImage(64, 64, QImage.Format.Format_RGBA8888)
        image.fill(QColor("#ff22cc"))
        assert image.save(str(icon), "PNG"), "custom folder icon fixture written"
        (folder / ".directory").write_text("[Desktop Entry]\nIcon=" + str(icon) + "\n")
        offered, outcomes = [desktop.as_uri()], []
        failed_drags = set()
        drag_starts = []
        source = Gtk.DragSource.new()
        source.set_actions(Gdk.DragAction.COPY)
        source.connect("prepare", lambda *_: Gdk.ContentProvider.new_for_bytes(
            "text/uri-list", GLib.Bytes.new((offered[0] + "\r\n").encode())))
        source.connect("drag-cancel", lambda _, transfer, reason: failed_drags.add(transfer) or False)
        source.connect("drag-end", lambda _, transfer, delete: outcomes.append(
            transfer not in failed_drags and bool(transfer.get_selected_action())))
        source.connect("drag-begin", lambda *_: drag_starts.append(offered[0]))
        label = Gtk.Label(label="Native URI drag source")
        label.add_controller(source)
        first.set_child(label)
        first.set_title("Runtime drag source")
        first.set_default_size(180, 140)
        first.present()
        wait_for(lambda: first.get_mapped(), "native drag source mapped")
        # Wayland clients cannot place their own windows. Use the private KWin
        # scripting API to keep this owned source clear of the desktop applet.
        placement = root / "drag-source-placement.js"
        placement.write_text('const w = workspace.windowList().find(w => w.caption === "Runtime drag source"'
            ' && w.resourceClass === "' + app_id + '"); if (w) w.frameGeometry = {x:1040,y:20,width:180,height:140};')
        plugin = "org.archdock.runtime-drag-placement"
        script = call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "loadScript", "(ss)", (str(placement), plugin))
        assert script >= 0
        try: call("org.kde.KWin", "/Scripting/Script" + str(script), "org.kde.kwin.Script", "run")
        finally: call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript", "(s)", (plugin,))
        def source_point():
            w = next(w for w in kwin_geometry()["windows"] if w["caption"] == "Runtime drag source")
            return [w["x"] + w["width"] / 2, w["y"] + w["height"] / 2]
        configure(free_panel, {"type": "launcher", "layout": "ring", "layoutRadius": 120,
                              "rendererTier": "procedural2d", "acceptDrops": True})
        native_before = panel_call("dockEntriesForPanel", "(ss)", ("bottom", "launcher"))
        def rows(): return panel_call("dockEntriesForPanel", "(ss)", (free_panel, "launcher"))
        def observed(app): return observations.get(("entry", free_panel, app), {})
        def entry_point(app):
            data = wait_for(lambda: observed(app), "real dropped entry")
            return native_point(data["hostSize"], data["center"])
        def drop(uri, target, accepted=True):
            # A refused drop can leave the private Plasma desktop focused.
            # Restore the owned source through KWin before the next real drag;
            # a popup dismissal must not consume that drag's first press.
            wait_for(lambda: source.get_drag() is None, "previous native drag fully released")
            kwin_geometry(activate_drag_source=True)
            wait_for(lambda: any(w["app"] == app_id and w["caption"] == "Runtime drag source"
                and w["active"] for w in geometry["value"]["windows"]), "native drag source activated")
            offered[0] = uri
            previous = len(outcomes)
            previous_starts = len(drag_starts)
            previous_result = observations.get(("drop-result", free_panel, ""), {}).get("at", 0)
            drag(source_point(), target)
            wait_for(lambda: len(drag_starts) == previous_starts + 1 and drag_starts[-1] == uri,
                     "owned source began exactly one native URI drag")
            wait_for(lambda: len(outcomes) == previous + 1 and source.get_drag() is None,
                     "Wayland drop completed and source drag cleared")
            if accepted:
                assert outcomes[-1], (uri, outcomes)
            if panel_call("dockConfiguration", "(s)", (free_panel,))["acceptDrops"]:
                receipt = wait_for(lambda: observations.get(("drop-result", free_panel, ""), {}).get("at", 0) > previous_result
                    and observations[("drop-result", free_panel, "")], "Arch Dock drop result observed")
                assert receipt["accepted"] == accepted and [unquote(value) for value in receipt["urls"]] == [unquote(uri)], (receipt, rows())
            if not accepted:
                print("Private desktop fallback after refused drop:", json.dumps(kwin_geometry()), flush=True)
                escape()
                wait_for(lambda: not any(w["popup"] for w in kwin_geometry()["windows"]),
                         "private desktop fallback menu dismissed")
        host = wait_for(lambda: observations.get(("host", free_panel, ""), {}).get("dropEnabled")
            and observations[("host", free_panel, "")], "empty free launcher drop area ready")
        target = native_point(host["rect"][2:], host["dropPoint"])
        drop(desktop.as_uri(), target)
        wait_for(lambda: len(rows()) == 1, "empty free launcher drop persisted")
        app = rows()[0]["appId"]
        drop(folder.as_uri(), entry_point(app))
        wait_for(lambda: len(rows()) == 2, "folder drop on existing entry persisted")
        folder_id = next(row["appId"] for row in rows() if row.get("isFolder"))
        drop(folder.as_uri(), entry_point(app))
        assert len(rows()) == 2, rows()
        drop("https://example.invalid/unsupported", entry_point(app), False)
        assert len(rows()) == 2, rows()
        configure(free_panel, {"acceptDrops": False})
        drop(desktop.as_uri(), entry_point(app), False)
        assert len(rows()) == 2, rows()
        configure(free_panel, {"acceptDrops": True})
        wait_for(lambda: observations.get(("host", free_panel, ""), {}).get("dropEnabled"),
                 "live free launcher re-enabled its drop area")
        for desktop_name in ("org.kde.dolphin.desktop", "org.kde.konsole.desktop"):
            path = pathlib.Path("/usr/share/applications") / desktop_name
            assert path.is_file(), path
            count = len(rows())
            drop(path.as_uri(), entry_point(folder_id))
            wait_for(lambda: len(rows()) == count + 1, "real KDE application drop")
        before_order = [row["appId"] for row in rows()]
        drag(entry_point(before_order[0]), entry_point(before_order[-1]))
        wait_for(lambda: [row["appId"] for row in rows()] != before_order, "native pointer reorder committed")
        assert set(row["appId"] for row in rows()) == set(before_order)
        wait_for(lambda: not panel_call("panelInteractionGuards", "(s)", (free_panel,))["dragActive"]
            and all(not observed(row["appId"]).get("dragging", True) for row in rows()), "drag guard cleared")
        assert panel_call("dockEntriesForPanel", "(ss)", ("bottom", "launcher")) == native_before
        for tier, theme in (("procedural2d", ""), ("true3d", "mesh-platform-cyan")):
            configure(free_panel, {"rendererTier": tier, "panelThemeId": theme, "completeThemeId": theme})
            for row in rows():
                wait_for(lambda: observed(row["appId"]).get("iconSource") == row["iconName"]
                    and observed(row["appId"]).get("glyphValid")
                    and all(v > 0 for v in observed(row["appId"])["glyphSize"])
                    and observed(row["appId"]).get("meshActive") == (tier == "true3d"),
                    "native application/folder glyph in " + tier + ": " + row["appId"])
            def host_state(): return observations.get(("host", free_panel, ""), {})
            before = wait_for(lambda: host_state().get("rotation", {}).get("wheelAvailable")
                and host_state(), "live wheel rotation ready in " + tier)
            angle = before["rotation"]["angle"]
            wheel(entry_point(app), 0, -120, discrete=True)
            wait_for(lambda: abs(host_state()["rotation"]["angle"] - (angle + 15) % 360) < 0.01,
                "Wayland wheel turns free panel clockwise in " + tier)
            wheel(entry_point(app), 0, 120, discrete=True)
            wait_for(lambda: abs(host_state()["rotation"]["angle"] - angle) < 0.01,
                "Wayland wheel reverses free panel in " + tier)
            for mode, direction in (("clockwise", 1), ("counter-clockwise", -1)):
                configure(free_panel, {"panelRotationMode": mode, "panelRotationSpeed": 90,
                    "panelRotationTrigger": "idle"})
                before = wait_for(lambda: host_state()["rotation"]["active"] and host_state(), "continuous motion active")
                angle = before["rotation"]["angle"]
                wait_for(lambda: 0.1 < (direction * (host_state()["rotation"]["angle"] - angle)) % 360 < 90,
                    "continuous " + mode + " motion in " + tier)
            configure(free_panel, {"panelRotationMode": "none"})
            wait_for(lambda: not host_state()["rotation"]["active"], "continuous rotation disabled")
        configuration = panel_call("dockConfiguration", "(s)", (free_panel,))
        record = configuration["panel"]
        def free_geometry(command=None, clear=False):
            write = ""
            if command is not None:
                payload = dict(command, panelId=free_panel, ownerToken=record["freeOwnershipToken"])
                write = "widget.writeConfig(\"auditionRestoreGeometry\", " + json.dumps(json.dumps(payload)) + ");"
            elif clear:
                write = "widget.writeConfig(\"auditionRestoreGeometry\", \"\");"
            script = f'''const desktop = desktopById({record["freeDesktopContainmentId"]});
                const widget = desktop ? desktop.widgetById({record["freeDockAppletId"]}) : null;
                if (!widget || widget.type !== "org.archdock.dock") throw new Error("missing owned widget");
                widget.currentConfigGroup = ["General"];
                if (String(widget.readConfig("panelId", "")) !== {json.dumps(free_panel)} ||
                    String(widget.readConfig("ownerToken", "")) !== {json.dumps(record["freeOwnershipToken"])})
                    throw new Error("ownership mismatch");
                {write}
                const g = widget.geometry;
                print("ARCHDOCK_GEOMETRY:" + JSON.stringify({{x:g.x, y:g.y, width:g.width, height:g.height}}));'''
            reply = call("org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell", "evaluateScript", "(s)", (script,))
            assert reply.strip().startswith("ARCHDOCK_GEOMETRY:"), reply
            return json.loads(reply.strip().split(":", 1)[1])
        actual = free_geometry()
        configure(free_panel, {"x": round(actual["x"]) + 40, "y": round(actual["y"]) + 30})
        moved = free_geometry()
        assert moved == dict(actual, x=round(actual["x"]) + 40, y=round(actual["y"]) + 30), (actual, moved)
        drifted = dict(moved, x=moved["x"] + 5, y=moved["y"] + 4)
        free_geometry(command=drifted)
        wait_for(lambda: free_geometry() == drifted, "owned widget moved independently of saved intent")
        free_geometry(clear=True)
        # A bounded invalid command must restore the actual moved widget,
        # not the older saved intent or a different desktop applet.
        before = panel_call("dockConfiguration", "(s)", (free_panel,))
        rejected = panel_call("applyPanelSettingsTransaction", "(sta{sv}a{sv})",
            (free_panel, int(before["settingsRevision"]), values({"x": 1000001}), {}))
        assert not rejected["success"] and rejected["rolledBack"], rejected
        assert free_geometry() == drifted
        after = panel_call("dockConfiguration", "(s)", (free_panel,))
        assert (after["x"], after["y"]) == (before["x"], before["y"])
        motion([1200, 500])
        configure(free_panel, {"rendererTier": "procedural2d", "panelThemeId": "", "completeThemeId": "",
            "presentationMode": "collapsed", "collapseMechanism": "collapse-horizontal", "presentationTrigger": "hover"})
        wait_for(lambda: host_state().get("collapseProgress") == 1, "procedural panel rests closed")
        host = host_state()
        motion(native_point(host["rect"][2:], host["revealPoint"]))
        wait_for(lambda: opened(free_panel) and host_state().get("collapseProgress") == 0,
            "native hover opens the procedural panel")
        motion([1200, 500])
        wait_for(lambda: host_state().get("collapseProgress") == 1,
            "native pointer leave closes the procedural panel")
        configure(free_panel, {"presentationMode": "open", "collapseMechanism": "open"})
        print("PASS: real Wayland wheel and optional continuous rotation in 2D/3D; owned free position read-back/rollback; procedural hover open/close", flush=True)
        print("PASS: actual Wayland URI/application/folder drops, deduplication, refusals, pointer reorder, native ownership and 2D/3D glyphs", flush=True)

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
        if os.environ.get("ARCHDOCK_RUNTIME_UI") == "1":
            run_runtime_ui_matrix()
            return
        if os.environ.get("ARCHDOCK_VISIBILITY_DISCRIMINATOR") == "1":
            configure("bottom", {"layout": "horizontal", "rendererTier": "procedural2d",
                                 "panelThemeId": "", "completeThemeId": ""})
            click([1200, 500])
            wait_for(lambda: opened("bottom") and observations.get(("host", "bottom", "")), "native host ready")
            window_id = visibility_discriminator()
            assert panel_call("setPanelVisible", "(sb)", ("bottom", True))
            wait_for(lambda: opened("bottom") and any(window["id"] == window_id and not window["hidden"]
                     for window in kwin_geometry()["windows"]), "same native window revealed and reported")
            print("PASS: native reveal restores the same compositor window and lifecycle", flush=True)
            return
        if os.environ.get("ARCHDOCK_RENDERING_FOLDERS") == "1":
            run_folder_matrix()
            run_content_matrix()
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
        if geometry["registration"]:
            call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting", "unloadScript",
                 "(s)", (geometry_plugin,))
            wait_for(lambda: not call("org.kde.KWin", "/Scripting", "org.kde.kwin.Scripting",
                                     "isScriptLoaded", "(s)", (geometry_plugin,)), "geometry probe unloaded")
            bus.unregister_object(geometry["registration"])
            geometry_script.unlink()
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
