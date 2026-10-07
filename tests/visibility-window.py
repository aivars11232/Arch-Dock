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
    # Wheel events the scene did not take reach the representation: record
    # where, and what the scene said about that point at that moment.
    insert("main.qml", "id: representation", '''
            WheelHandler {
                target: null
                onWheel: function(event) {
                    const local = representation.mapToItem(panelScene, event.x, event.y)
                    const mask = panelScene.containmentMask
                    console.warn("ArchDockInteraction " + JSON.stringify({kind: "missedWheel", panel: root.panelId,
                        at: Date.now(), point: [local.x, local.y], available: panelScene.wheelRotationAvailable,
                        maskContains: mask ? mask.contains(local) : null,
                        sceneContains: panelScene.containsInputPoint(local),
                        popupOpen: Boolean(root.sceneRuntimeState.popupOpen)}))
                    event.accepted = false
                }
            }
''')
    insert("main.qml", "id: folderTrack", "\n                observedPanelId: root.panelId\n                observedActive: representation.authoritativeHost")
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
    import json
    insert("main.qml", "id: panelScene", '''
                Timer {
                    interval: 150; running: true; repeat: true
                    property var lastConfiguration: null
                    property int configurationSerial: 0
                    // Every arrival at a state is drawn once, even when the
                    // configuration did not change on the way.
                    property string lastSurfaceState: ""
                    property int stateSerial: 0
                    property string captured: ""
                    // A state is drawn once it has held for a few ticks, so a
                    // threaded canvas or a 3D frame has painted it.
                    property string pending: ""
                    property int pendingTicks: 0
                    onTriggered: {
                        if (!representation.authoritativeHost) return;
                        if (root.configuration !== lastConfiguration) {
                            lastConfiguration = root.configuration;
                            ++configurationSerial;
                        }
                        if (presentationController.surfaceState !== lastSurfaceState) {
                            lastSurfaceState = presentationController.surfaceState;
                            ++stateSerial;
                        }
                        if (presentationController.transitionState !== "idle") return;
                        const key = [root.panelId, panelScene.effectiveRendererTier,
                            presentationController.surfaceState, configurationSerial, stateSerial].join("~");
                        if (key === captured) return;
                        if (key !== pending) {
                            pending = key;
                            pendingTicks = 0;
                            return;
                        }
                        if (++pendingTicks < 3) return;
                        captured = key;
                        const path = ''' + json.dumps(str(stage.parent / 'logs')) + ''' + "/panel-"
                            + key.replace(/[^A-Za-z0-9~.-]/g, "_") + ".png";
                        const state = presentationController.surfaceState;
                        const tier = panelScene.effectiveRendererTier;
                        panelScene.grabToImage(result => {
                            const saved = result.saveToFile(path);
                            console.warn("ArchDockInteraction " + JSON.stringify({kind: "panelCapture",
                                panel: root.panelId, key: key, state: state, tier: tier, path: path,
                                entries: panelScene.entryCount, saved: saved, at: Date.now()}));
                        });
                    }
                }
''')
    insert("main.qml", "id: panelScene", '''
                Timer {
                    interval: 100; running: true; repeat: true
                    property int sample: 0
                    onTriggered: {
                        if (!representation.authoritativeHost) return;
                        const host = panelScene.Window.window;
                        // A point on the drawn surface that is not over an icon, and
                        // the scene centre: where a wheel must and must not act.
                        const surfaceProbe = (() => {
                            const mesh = panelScene.effectiveRendererTier === "true3d"
                                ? panelScene.activeSurfaceRenderer : null;
                            const track = panelScene.activeTrackMetrics;
                            const middle = track && track.center ? Qt.point(track.center.x, track.center.y)
                                : Qt.point(panelScene.width / 2, panelScene.height / 2);
                            // A tilted platform's hole is shallow, so the hit margins of the icons
                            // above and below reach its centre. Look along the middle of the hole.
                            const reach = track ? Number(track.radiusX) : Number(panelScene.layoutGeometry.radius);
                            const result = {surface: null, interior: null};
                            for (let step = 0; step <= 6 && !result.interior; ++step) {
                                for (const side of step === 0 ? [0] : [1, -1]) {
                                    const p = Qt.point(middle.x + side * step * reach * 0.1, middle.y);
                                    if (panelScene.containsInputPoint(p)) continue;
                                    const mapped = panelScene.mapToItem(null, p.x, p.y);
                                    result.interior = [mapped.x, mapped.y];
                                    break;
                                }
                            }
                            if (!panelScene.wheelRotationAvailable) return result;
                            const rects = panelScene.entryRects;
                            const overEntry = p => rects.some(r => p.x >= r.x - 24 && p.x <= r.x + r.width + 24
                                && p.y >= r.y - 24 && p.y <= r.y + r.height + 24);
                            for (let degrees = 5; degrees < 360 && !result.surface; degrees += 10) {
                                const turn = degrees * Math.PI / 180;
                                let p;
                                if (mesh && mesh.viewport) {
                                    const radius = Number(mesh.entryTrackRadius || mesh.platformScale * 0.84);
                                    const v = mesh.viewport.mapFrom3DScene(Qt.vector3d(
                                        radius * Math.cos(turn), radius * Math.sin(turn), mesh.platformTop));
                                    p = Qt.point(v.x, v.y);
                                } else if (track) {
                                    p = Qt.point(track.center.x + track.radiusX * Math.cos(turn),
                                        track.center.y + track.radiusY * Math.sin(turn));
                                } else {
                                    p = Qt.point(middle.x + reach * Math.cos(turn), middle.y + reach * Math.sin(turn));
                                }
                                if (overEntry(p) || !panelScene.containsInputPoint(p)) continue;
                                const mapped = panelScene.mapToItem(null, p.x, p.y);
                                result.surface = [mapped.x, mapped.y];
                            }
                            return result;
                        })();
                        const sceneCentre = (() => {
                            const mesh = panelScene.effectiveRendererTier === "true3d"
                                ? panelScene.activeSurfaceRenderer : null;
                            const track = panelScene.activeTrackMetrics;
                            let p = track && track.center ? Qt.point(track.center.x, track.center.y)
                                : Qt.point(panelScene.width / 2, panelScene.height / 2);
                            if (mesh && mesh.viewport) {
                                const v = mesh.viewport.mapFrom3DScene(Qt.vector3d(0, 0, mesh.platformTop));
                                p = Qt.point(v.x, v.y);
                            }
                            const mapped = panelScene.mapToItem(null, p.x, p.y);
                            return [mapped.x, mapped.y];
                        })();
                        console.warn("ArchDockInteraction " + JSON.stringify({kind: "host", panel: root.panelId,
                            surfacePoint: surfaceProbe.surface, interiorPoint: surfaceProbe.interior,
                            center: sceneCentre,
                            inputRegion: panelScene.activeInputRegionKind,
                            profile: String((root.configuration.presentationProfile || {}).id || ""),
                            at: Date.now(), itemVisible: representation.visible, itemOpacity: representation.opacity,
                            windowVisible: host.visible, windowVisibility: panelScene.Window.visibility,
                            nativeState: root.nativeHostState,
                            rect: [host.x, host.y, host.width, host.height], concealed: representation.hostConcealed,
                            dropPoint: (() => { const p = panelDropArea.mapToItem(null,
                                panelDropArea.width / 2, panelDropArea.height / 2); return [p.x, p.y]; })(),
                            dropEnabled: panelDropArea.enabled && panelDropArea.visible,
                            rotation: {angle: panelScene.sceneRotationAngle, active: panelScene.sceneRotationActive,
                                wheelAvailable: panelScene.wheelRotationAvailable,
                                travel: panelScene.entryTravel !== undefined ? panelScene.entryTravel : null,
                                travelAvailable: Boolean(panelScene.wheelTravelAvailable)},
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
    insert("FolderTrackHost.qml", "id: root", '\n    property string observedPanelId: ""\n    property bool observedActive: false')
    insert("FolderTrackHost.qml", "id: content", '''
        Timer {
            interval: 100; running: true; repeat: true
            onTriggered: {
                if (!root.observedActive) return;
                const items = {};
                const shown = [];
                for (let index = 0; index < content.entries.length; ++index) {
                    const child = content.children.find(item => item.objectName === "folder-child-" + index);
                    if (!child) continue;
                    const point = child.mapToItem(null, child.width / 2, content.iconSize * child.drawnScale / 2);
                    items[child.objectName] = [point.x, point.y];
                    if (child.placed.onTrack && child.opacity > 0.99) shown.push(child.objectName);
                }
                console.warn("ArchDockInteraction " + JSON.stringify({kind: "folderTrack", panel: root.observedPanelId,
                    visible: root.visible, opening: content.openingInProgress, layout: "track",
                    selected: content.selectedChildId, snapshot: root.snapshot, pitch: content.pitch,
                    capacity: content.track.capacity, items: items, shown: shown,
                    rect: [root.x, root.y, root.width, root.height]}));
            }
        }
''')
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
                const shown = [];
                function visit(item) {
                    if (!item.visible) return;
                    if (item.objectName.startsWith("folder-child-")) {
                        const point = item.mapToItem(null, item.width / 2, 28);
                        items[item.objectName] = [point.x, point.y];
                        if (item.opacity > 0.999) shown.push(item.objectName);
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
                    lean: root.expansionLean !== undefined ? root.expansionLean : null,
                    anchorAcross: content.anchorAcross !== undefined ? content.anchorAcross : null,
                    contentWidth: content.width,
                    active: root.active, focused: content.activeFocus,
                    visible: root.visible, snapshot: root.snapshot, layout: content.geometry.layout,
                    selected: content.selectedChildId, reducedMotion: content.reducedMotion,
                    opening: content.openingInProgress, progress: content.openingProgress,
                    origin: [content.expansionOrigin.x, content.expansionOrigin.y],
                    capture: captureStatus,
                    background: root.backgroundHints, colorAlpha: root.color.a,
                    placement: {anchorY: root.anchorScreenY,
                        screenHeight: root.anchorScreenHeight, maximumHeight: content.maximumHeight},
                    showNames: root.showNames, names: names,
                    viewport: viewport ? {x: viewport.x, y: viewport.y,
                        width: viewport.width, height: viewport.height,
                        contentX: viewport.contentX, contentY: viewport.contentY,
                        contentWidth: viewport.contentWidth, contentHeight: viewport.contentHeight,
                        dragging: viewport.dragging, moving: viewport.moving} : {},
                    rect: [root.x, root.y, root.width, root.height], items: items, shown: shown,
                    path: {radiusX: content.path.radiusX, radiusY: content.path.radiusY,
                        capacity: content.path.capacity, side: content.geometry.side}}));
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
    if os.environ.get("ARCHDOCK_RENDERING_TRAVEL") != "1":
        return
    # ADREP-TASK-002 travel evidence: each frame that moves an entry or
    # turns the drawn surface, each wheel the scene took, each repaint of a
    # flat surface, the resting state, and one capture per resting state.
    # Written against what both the old and the new scene expose, so the
    # same matrix records the problem before the change and checks it after.
    capture_directory = json.dumps(str(stage.parent / 'logs'))
    insert("main.qml", "id: panelScene", '''
                // Frames the panel's window has prepared since the shell started.
                property int travelFrames: 0
                function travelSnapshot() {
                    const entries = [];
                    for (let i = 0; i < panelScene.entryCount; ++i) {
                        const item = panelScene.entryItemAt(i);
                        if (!item) continue;
                        const mapped = item.mapToItem(null, item.width / 2, item.height / 2);
                        const output = panelScene.entryGeometryAt(i);
                        entries.push({app: String(item.sceneEntry.appId || ""),
                            c: [Math.round(mapped.x * 10) / 10, Math.round(mapped.y * 10) / 10],
                            s: [Math.round((item.x + item.width / 2) * 10) / 10,
                                Math.round((item.y + item.height / 2) * 10) / 10],
                            v: item.visible, e: item.enabled, o: Math.round(item.opacity * 100) / 100,
                            p: Math.round(Number(output.pathProgress || 0) * 10000) / 10000,
                            t: output.onTrack !== false});
                    }
                    return {angle: Math.round(panelScene.sceneRotationAngle * 100) / 100,
                        travel: panelScene.entryTravel !== undefined
                            ? Math.round(panelScene.entryTravel * 1000) / 1000 : null,
                        entries: entries};
                }
                Connections {
                    target: panelScene.Window.window
                    property string previous: ""
                    function onAfterAnimating() {
                        panelScene.travelFrames += 1;
                        if (!representation.authoritativeHost) return;
                        const snapshot = panelScene.travelSnapshot();
                        const value = JSON.stringify(snapshot);
                        if (value === previous) return;
                        previous = value;
                        snapshot.kind = "motionFrame";
                        snapshot.panel = root.panelId;
                        snapshot.frame = panelScene.travelFrames;
                        snapshot.at = Date.now();
                        console.warn("ArchDockInteraction " + JSON.stringify(snapshot));
                    }
                }
                Connections {
                    target: panelScene
                    function onWheelUsed() {
                        if (!representation.authoritativeHost) return;
                        console.warn("ArchDockInteraction " + JSON.stringify({kind: "wheelUsed",
                            panel: root.panelId, at: Date.now(), frame: panelScene.travelFrames,
                            angle: panelScene.sceneRotationAngle,
                            travel: panelScene.entryTravel !== undefined ? panelScene.entryTravel : null,
                            input: panelScene.lastWheelInput !== undefined ? panelScene.lastWheelInput : null}));
                    }
                }
                Timer {
                    interval: 300; running: true; repeat: true
                    property var connected: null
                    onTriggered: {
                        const find = item => {
                            if (!item) return null;
                            if (item.objectName === "procedural-surface-canvas") return item;
                            for (const child of item.children) {
                                const found = find(child);
                                if (found) return found;
                            }
                            return null;
                        };
                        const canvas = find(panelScene);
                        if (!canvas || canvas === connected) return;
                        connected = canvas;
                        canvas.painted.connect(function() {
                            if (!representation.authoritativeHost) return;
                            console.warn("ArchDockInteraction " + JSON.stringify({kind: "surfacePainted",
                                panel: root.panelId, at: Date.now(), angle: panelScene.sceneRotationAngle}));
                        });
                    }
                }
                Timer {
                    interval: 100; running: true; repeat: true
                    property string pending: ""
                    property int stableTicks: 0
                    property string captured: ""
                    property int serial: 0
                    onTriggered: {
                        if (!representation.authoritativeHost) return;
                        const snapshot = panelScene.travelSnapshot();
                        snapshot.kind = "travelState";
                        snapshot.panel = root.panelId;
                        snapshot.at = Date.now();
                        snapshot.layout = panelScene.layoutPath;
                        snapshot.tier = panelScene.effectiveRendererTier;
                        snapshot.hostSize = [panelScene.Window.window.width, panelScene.Window.window.height];
                        snapshot.wheelAvailable = Boolean(panelScene.wheelRotationAvailable
                            || panelScene.wheelBrowseAvailable || panelScene.wheelTravelAvailable);
                        snapshot.motionTarget = panelScene.motionTarget !== undefined ? panelScene.motionTarget : null;
                        snapshot.rotationActive = panelScene.sceneRotationActive;
                        snapshot.pitch = panelScene.travelPitch !== undefined ? panelScene.travelPitch : null;
                        snapshot.sensitivity = panelScene.scrollSensitivity !== undefined
                            ? panelScene.scrollSensitivity : null;
                        snapshot.travelActive = panelScene.travelMotionActive !== undefined
                            ? panelScene.travelMotionActive : null;
                        snapshot.rects = panelScene.entryRects.map(r => [Math.round(r.x), Math.round(r.y),
                            Math.round(r.width), Math.round(r.height)]);
                        const centre = panelScene.mapToItem(null, panelScene.width / 2, panelScene.height / 2);
                        snapshot.centre = [Math.round(centre.x * 10) / 10, Math.round(centre.y * 10) / 10];
                        console.warn("ArchDockInteraction " + JSON.stringify(snapshot));
                        // One capture of every state the scene rests in.
                        const signature = JSON.stringify([snapshot.layout, snapshot.tier, snapshot.angle,
                            snapshot.travel, snapshot.entries.map(e => [e.s, e.v, e.o])]);
                        if (signature !== pending) { pending = signature; stableTicks = 0; return; }
                        if (++stableTicks < 3 || signature === captured) return;
                        captured = signature;
                        const path = ''' + capture_directory + ''' + "/travel-" + (++serial) + ".png";
                        const rested = snapshot;
                        panelScene.grabToImage(result => {
                            const saved = result.saveToFile(path);
                            console.warn("ArchDockInteraction " + JSON.stringify({kind: "travelCapture",
                                panel: root.panelId, path: path, saved: saved, at: Date.now(),
                                signature: signature, angle: rested.angle, travel: rested.travel}));
                        });
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
    # Frame-by-frame motion records of the travel matrix (ADREP-TASK-002).
    motion_trace = []
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
                if value["kind"] in ("motionFrame", "wheelUsed", "surfacePainted"):
                    motion_trace.append(value)
                    motion_trace[:] = motion_trace[-6000:]
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

            # A native panel can still settle to its applet's minimum
            # thickness after a placement change (the folder matrix asks for
            # 92 px, Plasma grows it to 108). Target the entry where it is
            # now, not where the first sample saw it.
            def latest():
                nonlocal current
                current = entry(panel) or current
                return current["hostSize"], current["center"]

            point = native_point(current["hostSize"], current["center"], edge=edge,
                                 current_target=latest)
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
        import math
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

        def run_folder_anchor_matrix():
            # The owner's curved free panel, baked and in true 3D: each layout
            # opens at the clicked folder, away from the dock, wherever the
            # folder stands on the ring, and covers no icon of the dock.
            panel = free_panel
            evidence = pathlib.Path(os.environ.get("ARCHDOCK_SCENE_EVIDENCE_DIR") or str(root / "logs"))
            evidence.mkdir(parents=True, exist_ok=True)
            positions = [("right", 0), ("bottom", 90), ("left", 180), ("top", 270), ("diagonal", 315)]
            scenarios = [("baked2.5d", "ring-platform-blue", {}, layouts + ["track"]),
                         ("true3d", "mesh-platform-cyan", {"scene3DCameraPitch": 60}, ["arc", "grid", "track"]),
                         ("procedural2d", "", {}, ["track"])]
            icon_size = float(panel_call("dockConfiguration", "(s)", (panel,))["iconSize"])
            rows, failures = [], []

            def host():
                return observations.get(("host", panel, ""), {})

            def desktop_origin():
                state = host()
                return native_point(state["rect"][2:], [0, 0])

            def folder_and_centre():
                icon, state = entry(panel), host()
                origin = desktop_origin()
                folder = (origin[0] + icon["center"][0], origin[1] + icon["center"][1])
                centre = (origin[0] + state["center"][0], origin[1] + state["center"][1])
                return folder, centre

            def angle_of(folder, centre):
                return math.degrees(math.atan2(folder[1] - centre[1], folder[0] - centre[0])) % 360

            def wrap(value):
                return (value + 540) % 360 - 180

            def turn_folder_to(target):
                # A tilted platform turns its icons faster or slower on screen
                # than the layout angle changes: measure that response.
                gain = 1.0
                for _ in range(8):
                    wait_for(lambda: entry(panel) and host().get("center"), "folder and dock centre observed")
                    current = angle_of(*folder_and_centre())
                    error = wrap(target - current)
                    angle = float(panel_call("dockConfiguration", "(s)", (panel,))["layoutAngle"])
                    requested = int(round(wrap(angle + max(-170.0, min(170.0, error / gain)))))
                    if abs(error) < 6 or requested == int(round(angle)):
                        return
                    before = entry(panel)["center"]
                    configure(panel, {"layoutAngle": requested})
                    wait_for(lambda: entry(panel).get("center") != before, "folder moved along the ring")
                    time.sleep(0.3)
                    moved = wrap(angle_of(*folder_and_centre()) - current)
                    if abs(moved) > 0.5:
                        gain = moved / wrap(requested - angle)
                # The last move may have landed it: the same criterion, once more.
                if abs(wrap(target - angle_of(*folder_and_centre()))) < 6:
                    return
                raise AssertionError(("folder could not be placed", target, angle_of(*folder_and_centre())))

            def click_folder():
                def hovered_target():
                    current = entry(panel)
                    point = native_point(current["hostSize"], current["center"])
                    lib.ei_device_pointer_motion_absolute(devices[2], *point)
                    lib.ei_device_frame(devices[2], lib.ei_now(context))
                    sync_input()
                    acknowledged = entry(panel).get("sample", 0)
                    observed = wait_for(lambda: entry(panel) if entry(panel).get("sample", 0) > acknowledged
                                        else None, "fresh applet pointer observation")
                    # A true-3D icon's projected rectangle breathes with its
                    # hover motion, so its centre is compared within pixels.
                    near = math.hypot(observed["center"][0] - current["center"][0],
                                      observed["center"][1] - current["center"][1]) < 4
                    return point if observed.get("hovered") and near else None
                click(wait_for(hovered_target, "folder icon hovered"), 272)

            def check_track(tier, position):
                # "Along the dock": the children stand on the dock's own curve
                # outside it, beside the folder, clear of every dock icon.
                state = wait_for(lambda: observations.get(("folderTrack", panel, ""), {})
                    if observations.get(("folderTrack", panel, ""), {}).get("visible")
                    and not observations.get(("folderTrack", panel, ""), {}).get("opening")
                    and len(observations.get(("folderTrack", panel, ""), {}).get("shown", [])) >= 3 else None,
                    "folder open along the dock")
                folder, centre = folder_and_centre()
                left, top = native_point(state["rect"][2:], [0, 0])
                children = [(left + state["items"][name][0], top + state["items"][name][1])
                            for name in sorted(state["shown"], key=lambda n: int(n.rsplit("-", 1)[1]))]
                outward = (folder[0] - centre[0], folder[1] - centre[1])
                length = math.hypot(*outward) or 1
                outward = (outward[0] / length, outward[1] / length)
                nearest = min(math.hypot(c[0] - folder[0], c[1] - folder[1]) for c in children)
                middle = (sum(c[0] for c in children) / len(children) - folder[0],
                          sum(c[1] for c in children) / len(children) - folder[1])
                facing = (middle[0] * outward[0] + middle[1] * outward[1]) / (math.hypot(*middle) or 1)
                origin = desktop_origin()
                icons = [(origin[0] + value["center"][0], origin[1] + value["center"][1])
                         for (kind, owner, _), value in list(observations.items())
                         if kind == "entry" and owner == panel]
                clearance = min(math.hypot(c[0] - i[0], c[1] - i[1]) for c in children for i in icons)
                gaps = [math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(children, children[1:])]
                row = {"tier": tier, "position": position, "layout": "track",
                       "folder": [round(v, 1) for v in folder], "dockCentre": [round(v, 1) for v in centre],
                       "outward": [round(v, 3) for v in outward], "popup": [left, top] + state["rect"][2:],
                       "nearest": round(nearest, 1), "facing": round(facing, 3),
                       "clearance": round(clearance, 1), "gaps": [round(g, 1) for g in gaps],
                       "children": [[round(v, 1) for v in c] for c in children]}
                rows.append(row)
                problems = []
                if nearest > icon_size + 16 + state["pitch"] / 2 + 10:
                    problems.append("the contents do not start at the folder")
                if facing < 0.3:
                    problems.append("the contents do not stand outside the dock")
                if clearance < icon_size * 0.9:
                    problems.append("a child overlaps an icon of the dock")
                if gaps and (max(gaps) > state["pitch"] * 1.15 or min(gaps) < state["pitch"] * 0.6):
                    problems.append("the children are not spaced along the curve")
                if problems:
                    failures.append(dict(row, problems=problems))
                print("FOLDER ANCHOR " + ("FAIL" if problems else "PASS") + ": "
                      + json.dumps({k: row[k] for k in ("tier", "position", "layout", "nearest", "facing", "clearance")}
                                   | {"problems": problems}), flush=True)
                escape()
                wait_for(lambda: not observations.get(("folderTrack", panel, ""), {}).get("visible"),
                         "folder along the dock closed")

            configure(panel, {"x": 430, "y": 150, "folderSpeed": 80, "folderEasing": "outCubic"})
            for tier, theme, extra, scenario_layouts in scenarios:
                configure(panel, dict({"layout": "ring", "layoutRadius": 150, "layoutAngle": 0,
                    "rendererTier": tier, "panelThemeId": theme, "completeThemeId": theme,
                    "panelRotationMode": "none"}, **extra))
                for position, target in positions:
                    turn_folder_to(target)
                    for layout in scenario_layouts:
                        configure(panel, {"folderLayout": layout})
                        click_folder()
                        if layout == "track":
                            check_track(tier, position)
                            continue
                        state = wait_for(lambda: folder_popup(panel) if folder_popup(panel).get("visible")
                            and not folder_popup(panel).get("opening") and folder_popup(panel).get("layout") == layout
                            and len(folder_popup(panel).get("shown", [])) >= 3 else None, "anchored folder open")
                        folder, centre = folder_and_centre()
                        width, height = state["rect"][2:]
                        left, top = native_point([width, height], [0, 0])
                        outward = (folder[0] - centre[0], folder[1] - centre[1])
                        length = math.hypot(*outward) or 1
                        outward = (outward[0] / length, outward[1] / length)
                        nearest = (min(max(folder[0], left), left + width), min(max(folder[1], top), top + height))
                        gap = math.hypot(folder[0] - nearest[0], folder[1] - nearest[1])
                        middle = (left + width / 2 - folder[0], top + height / 2 - folder[1])
                        facing = (middle[0] * outward[0] + middle[1] * outward[1]) / (math.hypot(*middle) or 1)
                        # The children a person sees: on the path and inside the popup.
                        children = [(left + state["items"][name][0], top + state["items"][name][1])
                                    for name in state["shown"]
                                    if 0 <= state["items"][name][0] < width and 0 <= state["items"][name][1] < height]
                        behind = [round((c[0] - folder[0]) * outward[0] + (c[1] - folder[1]) * outward[1], 1)
                                  for c in children]
                        origin = desktop_origin()
                        covered = [value["app"] for (kind, owner, _), value in list(observations.items())
                                   if kind == "entry" and owner == panel and value["app"] != folder_app_ids[panel]
                                   and left <= origin[0] + value["center"][0] <= left + width
                                   and top <= origin[1] + value["center"][1] <= top + height]
                        row = {"tier": tier, "position": position, "layout": layout,
                               "folder": [round(v, 1) for v in folder], "dockCentre": [round(v, 1) for v in centre],
                               "outward": [round(v, 3) for v in outward], "popup": [left, top, width, height],
                               "gap": round(gap, 1), "facing": round(facing, 3), "childDepth": behind,
                               "children": [[round(v, 1) for v in c] for c in children], "coveredIcons": covered}
                        rows.append(row)
                        problems = []
                        if gap > icon_size * 0.75 + 16:
                            problems.append("the popup does not start at the clicked folder")
                        if facing < 0.3:
                            problems.append("the popup does not open away from the dock")
                        if min(behind) < -0.3 * icon_size:
                            problems.append("children stand behind the folder, over the dock")
                        if covered:
                            problems.append("the popup covers icons of the dock")
                        if problems:
                            failures.append(dict(row, problems=problems))
                        print("FOLDER ANCHOR " + ("FAIL" if problems else "PASS") + ": "
                              + json.dumps({k: row[k] for k in ("tier", "position", "layout", "gap", "facing", "coveredIcons")}
                                           | {"problems": problems}), flush=True)
                        escape()
                        wait_for(lambda: not folder_popup(panel).get("visible"), "anchored folder closed")
            (evidence / "folder-anchor-geometry.json").write_text(json.dumps(rows, indent=1))
            assert not failures, "folder anchoring failed in %d of %d cases: %s" % (
                len(failures), len(rows), json.dumps(failures)[:4000])
            print(f"PASS: {len(rows)} folder openings anchored at the clicked icon, outward, covering no dock icon",
                  flush=True)

        if os.environ.get("ARCHDOCK_RENDERING_FOLDER_ANCHORS") == "1":
            run_folder_anchor_matrix()
            return

        for panel in ("bottom", free_panel):
            for index, layout in enumerate(layouts):
                edge = ["bottom", "top", "left", "right", "bottom"][index] if panel == "bottom" else None
                if edge:
                    vertical = edge in ("left", "right")
                    placed = panel_call("applyNativePanelPlacementDraft", "(sa{sv})", (panel, values({
                        "edge": edge, "dynamic": False, "width": 92 if vertical else 720,
                        "height": 600 if vertical else 92})))
                    assert placed["success"], placed
                    # The edge applet lays its row out along the edge it is
                    # on; an edge panel offers no Dock layout (ADREP-TASK-001).
                    configure(panel, {"panelThemeId": "", "completeThemeId": "", "rendererTier": "procedural2d"})
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

            def on_path():
                state = folder_popup(panel)
                names = sorted(state.get("shown", []), key=lambda name: int(name.rsplit("-", 1)[1]))
                return [(name, state["items"][name]) for name in names if name in state.get("items", {})]

            def assert_half_circle(points):
                # First and last on one diameter beside the folder, every child
                # on the half circle the folder declares - a half ellipse when
                # the popup is short - bulging away from the folder by its
                # radius: no straight tail. The popup's side gives the frame.
                shape = folder_popup(panel)["path"]
                out_radius, across_radius = shape["radiusX"], shape["radiusY"]
                assert len(points) >= 3 and len(points) == shape["capacity"], (points, shape)
                vertical = shape.get("side") in ("top", "bottom")
                sign = -1 if shape.get("side") in ("top", "left") else 1
                def frame(point):
                    # (along the diameter, out from it)
                    return (point[0], point[1]) if vertical else (point[1], point[0])
                (a0, o0), (a1, o1) = frame(points[0][1]), frame(points[-1][1])
                middle = (a0 + a1) / 2
                assert abs(o0 - o1) < 1.5 and abs((a1 - a0) / 2 - across_radius) < 1.5, (points, shape)
                assert out_radius > 40 and across_radius > 40, shape
                for _, point in points:
                    along, out = frame(point)
                    assert abs(math.hypot((out - o0) / out_radius, (along - middle) / across_radius) - 1) < 0.02, (points, shape)
                    assert sign * (out - o0) >= -1.5, (points, shape)
                assert max(sign * (frame(point)[1] - o0) for _, point in points) > out_radius * 0.8, (points, shape)

            resting = wait_for(lambda: on_path() if on_path() and on_path()[0][0] == "folder-child-0" else None,
                               "dense folder rests on its first child")
            assert_half_circle(resting)
            point = folder_point(panel, "folder-child-0")
            # EIS positive Y scrolls down, like the existing Studio probe;
            # QtTest's synthetic angleDelta uses the opposite sign.
            wheel(point, 0, 120, discrete=True)
            wait_for(lambda: folder_popup(panel)["viewport"]["contentY"] > 0, "real downward folder wheel")
            moved = wait_for(lambda: on_path() if on_path() and on_path()[0][0] == "folder-child-1"
                             and len(on_path()) == len(resting) else None,
                             "one wheel notch moves the folder one child along the curve")
            assert_half_circle(moved)
            for (_, before), (_, after) in zip(resting, moved):
                assert abs(before[0] - after[0]) < 1.5 and abs(before[1] - after[1]) < 1.5, (resting, moved)
            wheel(point, 0, -120, discrete=True)
            wait_for(lambda: folder_popup(panel)["viewport"]["contentY"] == 0, "real upward folder wheel")
            wait_for(lambda: on_path() and on_path()[0][0] == "folder-child-0", "the folder returns along the curve")
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
            # An edge panel lays its row out along its edge and offers no
            # Dock layout; the free panel is made a row (ADREP-TASK-001).
            configure(panel, {**({"layout": "horizontal"} if panel == free_panel else {}),
                              "rendererTier": "procedural2d",
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

    def run_mechanism_matrix():
        # Every opening mechanism a panel offers visibly changes what it draws
        # and gives it back when the panel reopens; one it does not offer is
        # refused rather than saved as a mechanism that does nothing.
        import shutil
        from PySide6.QtGui import QImage
        evidence = pathlib.Path(os.environ.get("ARCHDOCK_SCENE_EVIDENCE_DIR") or str(root / "logs"))
        evidence.mkdir(parents=True, exist_ok=True)

        def capture(panel, state, after):
            # What the panel draws: painted pixels (every second one), how far
            # they reach across and down, and the entries it holds.
            observation = wait_for(lambda: next((value for (kind, owner, _), value in list(observations.items())
                if kind == "panelCapture" and owner == panel and value.get("state") == state
                and value.get("at", 0) > after and value.get("saved")), None), panel + " drawn " + state)
            image = QImage(observation["path"])
            assert not image.isNull(), observation
            painted, xs, ys = 0, [], []
            for y in range(0, image.height(), 2):
                for x in range(0, image.width(), 2):
                    if image.pixelColor(x, y).alpha() > 16:
                        painted += 1
                        xs.append(x)
                        ys.append(y)
            # Without an evidence directory the capture already lies in the
            # session's logs, which is where it would be copied to.
            kept = evidence / pathlib.Path(observation["path"]).name
            if kept.resolve() != pathlib.Path(observation["path"]).resolve():
                shutil.copy2(observation["path"], kept)
            return {"painted": painted, "width": max(xs) - min(xs) + 1 if xs else 0,
                    "height": max(ys) - min(ys) + 1 if ys else 0, "entries": observation.get("entries")}

        def settled(panel, state):
            wait_for(lambda: panel_call("panelPresentationState", "(s)", (panel,)).get("surfaceState") == state
                     and panel_call("panelPresentationState", "(s)", (panel,)).get("transitionState") == "idle",
                     panel + " rests " + state)

        # Icons on both panels, so a mechanism has entries to hide as well as
        # a surface to close.
        fixtures = []
        for number in range(3):
            desktop = root / f"data/applications/org.archdock.mechanismfixture{number}.desktop"
            desktop.parent.mkdir(exist_ok=True)
            desktop.write_text(f"[Desktop Entry]\nType=Application\nName=Mechanism fixture {number}\n"
                               "Exec=/usr/bin/true\nIcon=applications-system\n")
            fixtures.append(desktop.as_uri())
        import subprocess
        subprocess.run(["kbuildsycoca6", "--noincremental"], check=True, stdout=subprocess.DEVNULL)
        assert panel_call("pinPanelUrls", "(sas)", (free_panel, fixtures))
        configure(free_panel, {"type": "hybrid"})
        wait_for(lambda: len(panel_call("dockEntriesForPanel", "(ss)", (free_panel, "hybrid"))) >= 3,
                 "free panel lists its pinned fixtures")
        scenarios = [
            # An edge panel's row follows its edge; it offers no Dock layout.
            ("native bottom row", "bottom", {"rendererTier": "procedural2d",
                                             "panelThemeId": "", "completeThemeId": ""}),
            ("native skinned row", "bottom", {"rendererTier": "skinned2d",
                                              "panelThemeId": "sci-fi-chassis-blue",
                                              "completeThemeId": "sci-fi-chassis-blue"}),
            ("free skinned row", free_panel, {"layout": "horizontal", "rendererTier": "skinned2d",
                                              "panelThemeId": "energy-frame-cyan",
                                              "completeThemeId": "energy-frame-cyan"}),
            ("procedural ring", free_panel, {"layout": "ring", "layoutRadius": 120, "rendererTier": "procedural2d",
                                             "panelThemeId": "", "completeThemeId": ""}),
            ("baked blue ring", free_panel, {"layout": "ring", "rendererTier": "baked2.5d",
                                             "panelThemeId": "ring-platform-blue", "completeThemeId": "ring-platform-blue"}),
            ("Cyan 3D ring", free_panel, {"layout": "ring", "rendererTier": "true3d",
                                          "panelThemeId": "mesh-platform-cyan", "completeThemeId": "mesh-platform-cyan"}),
            ("Orange 3D circle", free_panel, {"layout": "circular", "rendererTier": "true3d",
                                              "panelThemeId": "arc-platform-orange", "completeThemeId": "arc-platform-orange"}),
        ]
        rows, failures = [], []
        for name, panel, settings in scenarios:
            scenario_start = time.time() * 1000
            configure(panel, dict(settings, presentationMode="open", collapseMechanism="open"))
            resolution = panel_call("resolvePanelCapabilities", "(sa{sv})", (panel, {}))
            tier = resolution["renderer"]["effectiveTier"]
            offered = [m["id"] for m in resolution["presentationMechanisms"] if m["available"] and m["id"] != "open"]
            unoffered = [m["id"] for m in resolution["presentationMechanisms"] if not m["available"]]
            # Contract change, ADREP-TASK-001 (owner decision PD-01): a free
            # panel offers no opening or closing mechanism; every one is
            # refused below. Edge panels keep theirs and must draw each.
            if panel == free_panel:
                assert not offered, (name, offered)
            for mechanism in offered:
              try:
                configure(panel, {"presentationMode": "open", "collapseMechanism": "open"})
                settled(panel, "open")
                # The latest drawing of this scenario's open panel: applying
                # settings it already has draws nothing new.
                open_painted = capture(panel, "open", scenario_start)
                before = time.time() * 1000
                # "Opens on" belongs to a panel that can close, so it is set
                # with the mechanism (PD-01 hides it while the panel is open).
                configure(panel, {"presentationMode": "collapsed", "collapseMechanism": mechanism,
                                  "presentationTrigger": "click"})
                settled(panel, "collapsed")
                collapsed_painted = capture(panel, "collapsed", before)
                before = time.time() * 1000
                assert panel_call("requestPanelPresentation", "(ss)", (panel, "open"))
                settled(panel, "open")
                reopened_painted = capture(panel, "open", before)
                # A collapse closes the panel: fewer pixels, and narrower along the
                # axis it closes on. A theme's end caps may stay as its handle.
                opened, collapsed, reopened = open_painted, collapsed_painted, reopened_painted
                shrinks = {"collapse-horizontal": ("width",), "collapse-vertical": ("height",),
                           "collapse-radial": ("width", "height")}.get(mechanism, ())
                visible = opened["painted"] > 0 and opened["entries"] > 0 \
                    and collapsed["painted"] <= 0.8 * opened["painted"] \
                    and all(collapsed["painted"] == 0 or collapsed[axis] <= 0.85 * opened[axis] for axis in shrinks)
                restored = opened["painted"] > 0 \
                    and abs(reopened["painted"] - opened["painted"]) <= 0.15 * opened["painted"]
                row = {"scenario": name, "panel": panel, "renderer": tier, "mechanism": mechanism,
                       "open": opened, "collapsed": collapsed, "reopened": reopened,
                       "visible": visible, "restored": restored}
                rows.append(row)
                print("MECHANISM " + ("PASS" if visible and restored else "FAIL") + ": " + json.dumps(row), flush=True)
                if not (visible and restored):
                    failures.append(row)
              except AssertionError as error:
                row = {"scenario": name, "panel": panel, "renderer": tier, "mechanism": mechanism,
                       "error": str(error)[:200]}
                rows.append(row)
                failures.append(row)
                print("MECHANISM FAIL: " + json.dumps(row), flush=True)
                assert panel_call("requestPanelPresentation", "(ss)", (panel, "open"))
            for mechanism in unoffered:
                revision = panel_call("dockConfiguration", "(s)", (panel,))["settingsRevision"]
                result = panel_call("applyPanelSettingsTransaction", "(sta{sv}a{sv})", (panel, revision,
                    values({"presentationMode": "collapsed", "collapseMechanism": mechanism}), {}))
                row = {"scenario": name, "panel": panel, "renderer": tier, "mechanism": mechanism,
                       "offered": False, "refused": result.get("success") is not True,
                       "errorCode": result.get("errorCode", "")}
                rows.append(row)
                print("MECHANISM " + ("REFUSED" if row["refused"] else "FAIL") + ": " + json.dumps(row), flush=True)
                if not row["refused"]:
                    failures.append(row)
            configure(panel, {"presentationMode": "open", "collapseMechanism": "open"})
        (evidence / "presentation-mechanism-matrix.json").write_text(json.dumps(rows, indent=1))
        assert not failures, "presentation mechanisms without a visible result: " + json.dumps(failures)
        print(f"PASS: {sum(1 for r in rows if r.get('offered', True))} offered mechanisms visibly collapse and "
              f"reopen; {sum(1 for r in rows if not r.get('offered', True))} unoffered ones are refused", flush=True)

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
            # Input must never change the selected page.
            selection = page_start["selection"]
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
                assert ui()["selection"] == selection, ui()
            before = ui()
            wheel(ui_point(before["tabs"]), 120, 0, discrete=True)
            wait_for(lambda: ui()["tabX"] > before["tabX"], "Wayland horizontal tab wheel")
            before = ui()
            wheel(ui_point(before["tabs"]), 0, 120, discrete=True, shift=True)
            wait_for(lambda: ui()["tabX"] > before["tabX"], "Wayland Shift-wheel tab scrolling")
            assert ui()["selection"] == selection, ui()
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
        for tier, theme in (("procedural2d", ""), ("baked2.5d", "ring-platform-blue"),
                            ("true3d", "mesh-platform-cyan")):
            # The turning checks below ask for the whole panel to move
            # (ADREP-TASK-002, PD-25); the entries' travel is checked after them.
            configure(free_panel, {"rendererTier": tier, "panelThemeId": theme, "completeThemeId": theme,
                                   "panelMotionTarget": "panel"})
            for row in rows():
                wait_for(lambda: observed(row["appId"]).get("iconSource") == row["iconName"]
                    and observed(row["appId"]).get("glyphValid")
                    and all(v > 0 for v in observed(row["appId"])["glyphSize"])
                    and observed(row["appId"]).get("meshActive") == (tier == "true3d"),
                    "native application/folder glyph in " + tier + ": " + row["appId"])
            def host_state(): return observations.get(("host", free_panel, ""), {})
            # The glyph facts above are the same in 2D and baked 2.5D, so they
            # do not show that the new surface has replaced the previous one.
            # Wait for its own input region, then for entry positions sampled
            # on it: a wheel sent to the previous surface proves nothing.
            region = "platform-mask" if tier == "baked2.5d" else "geometry-band"
            before = wait_for(lambda: host_state().get("inputRegion") == region
                and host_state().get("rotation", {}).get("wheelAvailable")
                and host_state(), "live wheel rotation ready in " + tier)
            wait_for(lambda: all(observed(row["appId"]).get("sample", 0) > before["at"] for row in rows()),
                     "entries observed on the " + tier + " surface")
            angle = before["rotation"]["angle"]
            wheel(entry_point(app), 0, -120, discrete=True)
            wait_for(lambda: abs(host_state()["rotation"]["angle"] - (angle + 15) % 360) < 0.01,
                "Wayland wheel turns free panel clockwise in " + tier)
            wheel(entry_point(app), 0, 120, discrete=True)
            wait_for(lambda: abs(host_state()["rotation"]["angle"] - angle) < 0.01,
                "Wayland wheel reverses free panel in " + tier)
            # The wheel belongs to the whole drawn surface, not to the icons:
            # the bare platform between two icons turns the panel both ways,
            # and the empty interior leaves it alone.
            probe = wait_for(lambda: host_state().get("surfacePoint") and host_state().get("interiorPoint")
                and host_state(), "bare dock surface and empty interior located in " + tier)
            surface = native_point(probe["rect"][2:], probe["surfacePoint"])
            interior = native_point(probe["rect"][2:], probe["interiorPoint"])
            wheel(surface, 0, -120, discrete=True)
            wait_for(lambda: abs(host_state()["rotation"]["angle"] - (angle + 15) % 360) < 0.01,
                "Wayland wheel turns from the bare surface in " + tier)
            wheel(surface, 0, 120, discrete=True)
            wait_for(lambda: abs(host_state()["rotation"]["angle"] - angle) < 0.01,
                "Wayland wheel reverses from the bare surface in " + tier)
            stamp = host_state()["at"]
            wheel(interior, 0, -120, discrete=True)
            wait_for(lambda: host_state()["at"] > stamp + 400, "host observed after the interior wheel")
            assert abs(host_state()["rotation"]["angle"] - angle) < 0.01, (
                "the empty interior turned the panel in " + tier, host_state()["rotation"])
            print(f"PASS: {tier}: wheel on the bare surface both ways; empty interior passes through "
                  f"({probe['inputRegion']})", flush=True)
            for mode, direction in (("clockwise", 1), ("counter-clockwise", -1)):
                configure(free_panel, {"panelRotationMode": mode, "panelRotationSpeed": 90,
                    "panelRotationTrigger": "idle"})
                before = wait_for(lambda: host_state()["rotation"]["active"] and host_state(), "continuous motion active")
                angle = before["rotation"]["angle"]
                wait_for(lambda: 0.1 < (direction * (host_state()["rotation"]["angle"] - angle)) % 360 < 90,
                    "continuous " + mode + " motion in " + tier)
            configure(free_panel, {"panelRotationMode": "none"})
            wait_for(lambda: not host_state()["rotation"]["active"], "continuous rotation disabled")
            # "Continuous motion moves" Items: the same wheel moves the entries
            # one slot along the panel's path and the panel stays still.
            configure(free_panel, {"panelMotionTarget": "items"})
            ready = wait_for(lambda: host_state().get("rotation", {}).get("travelAvailable")
                and not host_state()["rotation"]["wheelAvailable"] and host_state(),
                "live wheel travel ready in " + tier)
            angle, travel = ready["rotation"]["angle"], ready["rotation"]["travel"]
            point = entry_point(app)
            wheel(point, 0, -120, discrete=True)
            wait_for(lambda: abs(host_state()["rotation"]["travel"] - (travel + 1)) < 1e-6,
                "Wayland wheel moves the entries along the path in " + tier)
            assert abs(host_state()["rotation"]["angle"] - angle) < 0.01, (
                "the panel turned while its entries travelled in " + tier, host_state()["rotation"])
            wheel(point, 0, 120, discrete=True)
            wait_for(lambda: abs(host_state()["rotation"]["travel"] - travel) < 1e-6,
                "Wayland wheel moves the entries back in " + tier)
            if tier == "procedural2d":
                # A pointer reorder and a URI drop land on the entries where
                # they travelled to.
                wheel(point, 0, -120, discrete=True)
                wait_for(lambda: abs(host_state()["rotation"]["travel"] - (travel + 1)) < 1e-6,
                         "entries travelled before the reorder")
                def observed_since(stamp):
                    return all(observed(row["appId"]).get("sample", 0) > stamp + 50 for row in rows())
                moved_at = time.time() * 1000
                wait_for(lambda: observed_since(moved_at), "entries observed where they travelled")
                order = [row["appId"] for row in rows()]
                drag(entry_point(order[0]), entry_point(order[-1]))
                wait_for(lambda: [row["appId"] for row in rows()] != order,
                         "pointer reorder of travelled entries committed")
                wait_for(lambda: not panel_call("panelInteractionGuards", "(s)", (free_panel,))["dragActive"]
                    and all(not observed(row["appId"]).get("dragging", True) for row in rows()),
                    "drag guard cleared after the travelled reorder")
                travelled = root / "data/applications" / "org.archdock.travelled.desktop"
                travelled.write_text("[Desktop Entry]\nType=Application\nName=Travelled drop\n"
                                     "Exec=/usr/bin/true\nIcon=help-browser\n")
                settled_at = time.time() * 1000
                wait_for(lambda: observed_since(settled_at), "reordered entries observed")
                count = len(rows())
                drop(travelled.as_uri(), entry_point(rows()[0]["appId"]))
                wait_for(lambda: len(rows()) == count + 1, "a URI dropped on a travelled entry is kept")
                wheel(point, 0, 120, discrete=True)
                wait_for(lambda: abs(host_state()["rotation"]["travel"] - travel) < 1e-6,
                         "entries travelled back after the drop")
                print("PASS: procedural2d: a pointer reorder and a URI drop land on travelled entries",
                      flush=True)
            print(f"PASS: {tier}: the wheel moves the entries along the path both ways; the panel stays still",
                  flush=True)
            configure(free_panel, {"panelMotionTarget": "panel"})
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
        # Contract change, ADREP-TASK-001 (owner decision PD-01): a free panel
        # has no opening or closing mechanism. Asking it to rest collapsed is
        # refused and it stays open; this replaces the hover opening and
        # closing of a collapsed free panel that was checked here.
        configure(free_panel, {"rendererTier": "procedural2d", "panelThemeId": "", "completeThemeId": ""})
        revision = panel_call("dockConfiguration", "(s)", (free_panel,))["settingsRevision"]
        refused = panel_call("applyPanelSettingsTransaction", "(sta{sv}a{sv})", (free_panel, revision,
            values({"presentationMode": "collapsed", "collapseMechanism": "collapse-horizontal"}), {}))
        # The free host offers no mechanism, so the resolver refuses it.
        assert not refused["success"] and refused["errorCode"] == "capability-unavailable" \
            and refused["errorMessage"] == "presentation-mechanism-unavailable", refused
        wait_for(lambda: opened(free_panel) and host_state().get("collapseProgress") == 0,
            "free panel stays open")
        print("PASS: real Wayland wheel and optional continuous rotation in 2D/3D; owned free position read-back/rollback; a free panel refuses to collapse and stays open", flush=True)
        # Last: a radius-300 ring grows the desktop widget, which keeps the place
        # it grew to, so no later step may depend on the panel's position.
        # The owner's own free panels, with continuous animation off: free-9
        # (circular, Orange in true 3D, pitch 60, radius 300) and free-4
        # (circular, flat, radius 145). The wheel turns them from an icon
        # and from the bare platform once the scene rests: a changed pitch
        # eases in, and a point measured mid-way is not where the icon is.
        import math
        def host_state(): return observations.get(("host", free_panel, ""), {})
        owner_keys = ("layout", "layoutRadius", "rendererTier", "panelThemeId", "completeThemeId",
                      "scene3DCameraPitch", "scene3DCameraYaw", "panelRotationMode", "panelMotionTarget")
        baseline = {key: value for key, value in panel_call("dockConfiguration", "(s)", (free_panel,)).items()
                    if key in owner_keys}
        def resting(app):
            first = observed(app).get("center")
            time.sleep(0.35)
            second = observed(app).get("center")
            return first and second and math.hypot(first[0] - second[0], first[1] - second[1]) < 0.5
        for name, settings in (
                ("owner free-9", {"layout": "circular", "layoutRadius": 300, "rendererTier": "true3d",
                                  "panelThemeId": "arc-platform-orange", "completeThemeId": "arc-platform-orange",
                                  "scene3DCameraPitch": 60, "scene3DCameraYaw": 5, "panelRotationMode": "none",
                                  "panelMotionTarget": "panel"}),
                ("owner free-4", {"layout": "circular", "layoutRadius": 145, "rendererTier": "procedural2d",
                                  "panelThemeId": "", "completeThemeId": "", "panelRotationMode": "none",
                                  "panelMotionTarget": "panel"})):
            configure(free_panel, settings)
            before = wait_for(lambda: host_state().get("rotation", {}).get("wheelAvailable")
                and not host_state()["rotation"]["active"] and host_state(), name + " wheel ready")
            wait_for(lambda: all(observed(row["appId"]).get("sample", 0) > before["at"] for row in rows()),
                     "entries observed on " + name)
            wait_for(lambda: resting(app), name + " scene at rest")
            start = host_state()["rotation"]["angle"]
            def turned_to(degrees, what):
                wait_for(lambda: abs(host_state()["rotation"]["angle"] - degrees % 360) < 0.01,
                         "Wayland wheel turns " + name + " " + what + " with animation off")
                wait_for(lambda: resting(app), name + " scene at rest")
            def surface_point():
                probe = wait_for(lambda: host_state().get("surfacePoint") and host_state(),
                                 "bare surface located on " + name)
                return native_point(probe["rect"][2:], probe["surfacePoint"]), probe
            # Down at an icon, down on the bare platform, then back up the
            # same way: the panel rests where it started.
            wheel(entry_point(app), 0, -120, discrete=True)
            turned_to(start + 15, "from an icon")
            surface, probe = surface_point()
            wheel(surface, 0, -120, discrete=True)
            try:
                turned_to(start + 30, "from the bare platform")
            except AssertionError:
                print("MISSED WHEEL " + json.dumps({"probe": probe.get("surfacePoint"), "target": surface,
                      "missed": observations.get(("missedWheel", free_panel, ""))}), flush=True)
                raise
            surface, probe = surface_point()
            wheel(surface, 0, 120, discrete=True)
            turned_to(start + 15, "back from the bare platform")
            wheel(entry_point(app), 0, 120, discrete=True)
            turned_to(start, "back from an icon")
            print(f"PASS: {name}: wheel turns the panel both ways from an icon and the bare platform, "
                  "animation off", flush=True)
        configure(free_panel, baseline)

        print("PASS: actual Wayland URI/application/folder drops, deduplication, refusals, pointer reorder, native ownership and 2D/3D glyphs", flush=True)

    def run_travel_matrix():
        """ADREP-TASK-002: on a free panel the wheel moves the entries along
        the panel's own outline while the drawn panel stays still.

        With ARCHDOCK_TRAVEL_PHASE=before the same steps only record what
        the scene does, to show the problem; by default every step is
        checked. Frames, timings, a capture of every resting state and a
        report are kept under ARCHDOCK_SCENE_EVIDENCE_DIR/travel.
        """
        import math
        import shutil
        phase = os.environ.get("ARCHDOCK_TRAVEL_PHASE", "after")
        checking = phase == "after"
        evidence = pathlib.Path(os.environ.get("ARCHDOCK_SCENE_EVIDENCE_DIR") or str(root / "logs")) / "travel"
        evidence.mkdir(parents=True, exist_ok=True)
        report = {"phase": phase, "layouts": {}, "input": {}, "modes": {}, "continuous": {},
                  "directions": {}, "hits": {}}
        wait_for(lambda: 2 in devices and 16 in devices, "native pointer and scroll devices ready")
        icons = ("applications-system", "utilities-terminal", "system-file-manager",
                 "preferences-system", "help-browser", "accessories-text-editor")
        uris = []
        for index, icon in enumerate(icons):
            desktop = root / "data/applications" / ("org.archdock.travel" + str(index) + ".desktop")
            desktop.parent.mkdir(exist_ok=True)
            desktop.write_text("[Desktop Entry]\nType=Application\nName=Travel " + str(index)
                               + "\nExec=/usr/bin/true\nIcon=" + icon + "\n")
            uris.append(desktop.as_uri())
        base = {"type": "launcher", "rendererTier": "procedural2d", "panelThemeId": "",
                "completeThemeId": "", "panelRotationMode": "none", "layout": "circular",
                "layoutRadius": 120, "layoutAngle": 0}
        if checking:
            base.update(panelMotionTarget="items", scrollSensitivity=1)
        configure(free_panel, base)
        assert panel_call("pinPanelUrls", "(sas)", (free_panel, uris))
        def rows(): return panel_call("dockEntriesForPanel", "(ss)", (free_panel, "launcher"))
        wait_for(lambda: len(rows()) == len(uris), "six travel launchers pinned")
        apps = [row["appId"] for row in rows()]
        count = len(apps)

        def state(): return observations.get(("travelState", free_panel, ""), {})
        def frame_key(value):
            return json.dumps([value.get("angle"), value.get("travel"),
                               [[item["s"], item["v"], item["o"]] for item in value.get("entries", [])]])
        def rest(description, minimum=0.5):
            stable = {"key": None, "since": 0.0}
            def settled():
                current = state()
                if len(current.get("entries", [])) != count:
                    return None
                now = time.monotonic()
                if frame_key(current) != stable["key"]:
                    stable.update(key=frame_key(current), since=now)
                    return None
                return current if now - stable["since"] >= minimum else None
            return wait_for(settled, description)
        captured = {"key": None, "name": None}
        def capture(name, rested):
            if frame_key(rested) == captured["key"]:
                return captured["name"]
            shot = wait_for(lambda: (shot := observations.get(("travelCapture", free_panel, ""), {}))
                and shot.get("saved") and shot.get("at", 0) >= rested["at"] - 800
                and shot.get("angle") == rested["angle"] and shot.get("travel") == rested["travel"]
                and shot, "capture of " + name)
            target = evidence / (phase + "-" + name + ".png")
            shutil.copyfile(shot["path"], target)
            captured.update(key=frame_key(rested), name=target.name)
            return target.name
        def shown(item): return item["v"] and item["o"] > 0.99
        def aim(rested):
            for item in rested["entries"]:
                if shown(item) and item["e"]:
                    return native_point(rested["hostSize"], item["c"])
            raise AssertionError("no visible entry to aim the wheel at: " + json.dumps(rested))
        # ei: a negative vertical value scrolls up, which Qt reports as a
        # positive angle delta - forward, clockwise.
        def notches(point, number, value=-120):
            motion(point)
            sent = time.time() * 1000
            for _ in range(number):
                lib.ei_device_scroll_discrete(devices[16], 0, value)
                lib.ei_device_frame(devices[16], lib.ei_now(context))
            sync_input()
            return sent
        def smooth(point, steps, delta):
            motion(point)
            sent = time.time() * 1000
            for _ in range(steps):
                lib.ei_device_scroll_delta(devices[16], 0, delta)
                lib.ei_device_frame(devices[16], lib.ei_now(context))
                sync_input()
            return sent
        def timing(sent, before, after):
            mine = [m for m in motion_trace if m.get("panel") == free_panel and m["at"] >= sent - 20]
            used = [m["at"] for m in mine if m["kind"] == "wheelUsed"]
            moved = [m for m in mine if m["kind"] == "motionFrame" and frame_key(m) != frame_key(before)]
            arrived = [m["at"] for m in moved if frame_key(m) == frame_key(after)]
            painted = [m["at"] for m in mine if m["kind"] == "surfacePainted" and m["at"] <= after["at"]]
            receipt = used[0] if used else None
            first = moved[0]["at"] if moved else None
            used_frames = [m.get("frame") for m in mine if m["kind"] == "wheelUsed"]
            first_frame = moved[0].get("frame") if moved else None
            return {"wheelEventsTaken": len(used),
                    "framesFromWheelToFirstMove": first_frame - used_frames[0]
                        if first_frame is not None and used_frames and used_frames[0] is not None else None,
                    "sentToSceneMs": round(receipt - sent, 1) if receipt else None,
                    "sceneToFirstMovedFrameMs": first - receipt if receipt and first else None,
                    "firstMovedFrameToRestMs": arrived[0] - first if arrived and first else None,
                    "lastWheelToRestMs": arrived[0] - used[-1] if arrived and used else None,
                    "movedFrames": len(moved), "surfaceRepaints": len(painted),
                    "sceneToSurfaceRepaintMs": painted[0] - receipt if painted and receipt else None}
        def frames_between(sent, after):
            return [m for m in motion_trace if m.get("panel") == free_panel and m["kind"] == "motionFrame"
                    and sent - 20 <= m["at"] <= after["at"]]
        def positions(rested): return [item["s"] if shown(item) else None for item in rested["entries"]]
        # The scene keeps travel within one loop at rest, so travel is
        # compared modulo the loop.
        def moved_by(after, before, slots, loop):
            difference = (after - before - slots) % loop
            return min(difference, loop - difference) < 1e-6
        def distance(a, b): return math.hypot(a[0] - b[0], a[1] - b[1])
        def expected_slots(slots, loop, travel):
            # Entry i stands in slot (i + travel) mod loop; slots past the
            # visible ones are off the path.
            return [slots[(i + travel) % loop] if (i + travel) % loop < len(slots) else None
                    for i in range(count)]
        def check_slots(rested, slots, loop, travel, what):
            expected = expected_slots(slots, loop, travel)
            for index, item in enumerate(rested["entries"]):
                if expected[index] is None:
                    assert not item["e"] and (not item["v"] or item["o"] < 0.01), (
                        what + ": entry " + str(index) + " should be off the path", item)
                    assert rested["rects"][index][2] == 0, (what + ": hidden entry has a hit rectangle",
                                                            rested["rects"][index])
                else:
                    assert shown(item) and item["e"], (what + ": entry " + str(index) + " should be shown", item)
                    assert distance(item["s"], expected[index]) < 1.5, (
                        what + ": entry " + str(index) + " is not in slot " + str((index + travel) % loop),
                        item["s"], expected[index])
                    rect = rested["rects"][index]
                    assert rect[0] - 1 <= item["s"][0] <= rect[0] + rect[2] + 1 and \
                        rect[1] - 1 <= item["s"][1] <= rect[1] + rect[3] + 1, (
                        what + ": hit rectangle does not follow entry " + str(index), rect, item["s"])

        def outline(layout, rested, slots):
            # The real outline in window coordinates: polygon vertices from
            # the first slot (a vertex at the top), the star's inner corners
            # at 0.46 of its radius (LayoutEngine.starPoint).
            centre = rested["centre"]
            offset = [rested["entries"][0]["c"][axis] - rested["entries"][0]["s"][axis] for axis in (0, 1)]
            top = [slots[0][0] + offset[0], slots[0][1] + offset[1]]
            radius = distance(top, centre)
            sides = {"triangle": 3, "square": 4, "hexagon": 6, "star": 6}[layout]
            corners = []
            for corner in range(sides * (2 if layout == "star" else 1)):
                step = 360 / (sides * (2 if layout == "star" else 1))
                reach = radius * (0.46 if layout == "star" and corner % 2 else 1)
                angle = math.radians(-90 + corner * step)
                corners.append([centre[0] + reach * math.cos(angle), centre[1] + reach * math.sin(angle)])
            return corners
        def outline_distance(point, corners):
            best = float("inf")
            for index, start in enumerate(corners):
                end = corners[(index + 1) % len(corners)]
                dx, dy = end[0] - start[0], end[1] - start[1]
                share = max(0, min(1, ((point[0] - start[0]) * dx + (point[1] - start[1]) * dy)
                                   / max(1e-9, dx * dx + dy * dy)))
                best = min(best, distance(point, [start[0] + share * dx, start[1] + share * dy]))
            return best

        closed = ("circular", "ellipse", "ring", "triangle", "square", "hexagon", "star")
        layouts = (("circular", "hexagon", "star", "fan", "arc") if not checking else
                   closed + ("spiral", "fan", "arc", "semicircle", "radial"))
        for layout in layouts:
            configure(free_panel, {"layout": layout})
            start = rest(layout + " at rest")
            assert start["layout"] == layout, start
            row = {"capturedAtRest": capture(layout + "-0-rest", start), "steps": []}
            slots = [item["s"] for item in start["entries"] if shown(item)]
            capacity = len(slots)
            loop = count if layout in closed else max(count, capacity + 1)
            row.update(visibleAtRest=capacity, loopSlots=loop)
            assert capacity >= 3, (layout, start)
            # One notch forward.
            sent = notches(aim(start), 1)
            moved = rest(layout + " after one notch")
            step = {"wheel": "one notch forward", "angleBefore": start["angle"], "angleAfter": moved["angle"],
                    "travelBefore": start["travel"], "travelAfter": moved["travel"],
                    "timing": timing(sent, start, moved), "capture": capture(layout + "-1-notch", moved)}
            row["steps"].append(step)
            if checking:
                assert moved["angle"] == start["angle"], (layout + ": the panel surface turned", step)
                assert step["timing"]["surfaceRepaints"] == 0, (layout + ": the surface was repainted", step)
                assert moved_by(moved["travel"], start["travel"], 1, loop), (layout + ": one notch is one slot", step)
                check_slots(moved, slots, loop, 1, layout + " one notch")
                assert step["timing"]["framesFromWheelToFirstMove"] == 1, (
                    layout + ": the first frame after the wheel did not move", step)
                assert step["timing"]["lastWheelToRestMs"] <= 120, (layout + ": the step took too long", step)
                if layout in ("triangle", "square", "hexagon", "star"):
                    corners = outline(layout, start, slots)
                    worst = max(outline_distance(item["c"], corners)
                                for frame in frames_between(sent, moved) for item in frame["entries"]
                                if shown(item))
                    step["worstDistanceFromOutlinePx"] = round(worst, 2)
                    assert worst < 2.5, (layout + ": an entry left the outline between slots", worst)
            # Open paths: go round the whole loop backwards, one notch at a
            # time; every entry leaves one end and comes back at the other.
            if layout not in closed:
                seen = {index: set() for index in range(count)}
                travel = 1
                current = moved
                for number in range(loop + 1):
                    sent = notches(aim(current), 1, 120)
                    after = rest(layout + " after backward notch " + str(number + 1))
                    travel -= 1
                    record = {"wheel": "one notch back", "angleAfter": after["angle"],
                              "travelAfter": after["travel"], "timing": timing(sent, current, after),
                              "visible": [shown(item) for item in after["entries"]],
                              "capture": capture(layout + "-back-" + str(number + 1), after)}
                    row["steps"].append(record)
                    for index, item in enumerate(after["entries"]):
                        seen[index].add(shown(item))
                    if checking:
                        assert after["angle"] == start["angle"], (layout + ": the panel turned", record)
                        assert moved_by(after["travel"], 0, travel, loop), (layout + ": travel", record)
                        check_slots(after, slots, loop, travel % loop, layout + " back " + str(number + 1))
                    current = after
                row["everyEntryLeftAndReturned"] = all(len(values) == 2 for values in seen.values())
                if checking:
                    assert row["everyEntryLeftAndReturned"], (layout + ": wrap-around", seen)
            report["layouts"][layout] = row
            print(("PASS: " if checking else "RECORDED: ") + layout + ": " + json.dumps(row["steps"][0]["timing"]),
                  flush=True)

        # Input devices, on the circle: a fast spin, a high-resolution wheel
        # and touchpad-like smooth deltas.
        configure(free_panel, {"layout": "circular"})
        start = rest("circle at rest for input checks")
        sent = notches(aim(start), 10)
        spun = rest("circle after a fast spin")
        report["input"]["spin10"] = {"angleBefore": start["angle"], "angleAfter": spun["angle"],
                                     "travelBefore": start["travel"], "travelAfter": spun["travel"],
                                     "timing": timing(sent, start, spun), "capture": capture("circle-spin", spun)}
        if checking:
            assert moved_by(spun["travel"], start["travel"], 10, count), report["input"]["spin10"]
            assert report["input"]["spin10"]["timing"]["lastWheelToRestMs"] <= 120, report["input"]["spin10"]
        start = spun
        sent = time.time() * 1000
        for _ in range(8):
            notches(aim(start), 1, -15)
        fine = rest("circle after eight high-resolution steps")
        report["input"]["highResolution8x15"] = {"angleBefore": start["angle"], "angleAfter": fine["angle"],
                                                 "travelBefore": start["travel"], "travelAfter": fine["travel"],
                                                 "timing": timing(sent, start, fine)}
        if checking:
            assert moved_by(fine["travel"], start["travel"], 1, count), report["input"]["highResolution8x15"]
        start = fine
        pitch = 2 * math.pi * distance(start["entries"][0]["c"], start["centre"]) / count
        sent = smooth(aim(start), 12, -pitch / 10)
        touch = rest("circle after touchpad-like deltas")
        # What Qt delivered for these smooth deltas decides what they are
        # worth: pixels count by a slot's distance, angle units by 120.
        delivered = [m.get("input") or {} for m in motion_trace
                     if m["kind"] == "wheelUsed" and m.get("panel") == free_panel and m["at"] >= sent - 20]
        equivalents = sum((item.get("pixelDelta") or 0) / ((start.get("pitch") or {}).get("pixels") or 1)
                          if item.get("pixelDelta") else (item.get("angleDelta") or 0) / 120
                          for item in delivered)
        report["input"]["touchpadLike"] = {"pitchPx": round(pitch, 1), "sentPx": round(pitch * 1.2, 1),
                                           "delivered": delivered, "notchEquivalents": round(equivalents, 3),
                                           "angleBefore": start["angle"], "angleAfter": touch["angle"],
                                           "travelBefore": start["travel"], "travelAfter": touch["travel"],
                                           "timing": timing(sent, start, touch)}
        if checking:
            expected = math.floor(equivalents + 1e-6)
            assert moved_by(touch["travel"], start["travel"], expected, count), report["input"]["touchpadLike"]
        print(("PASS" if checking else "RECORDED") + ": input " + json.dumps(report["input"]), flush=True)

        if checking:
            # The hit-test matrix: the pointer over each travelled entry
            # hovers that entry and no other.
            for layout in ("circular", "hexagon", "star", "fan"):
                configure(free_panel, {"layout": layout})
                start = rest(layout + " at rest for hit tests")
                notches(aim(start), 1)
                moved = rest(layout + " moved for hit tests")
                rows_seen = []
                for index, item in enumerate(moved["entries"]):
                    if not shown(item):
                        continue
                    point = native_point(moved["hostSize"], item["c"])
                    motion(point)
                    moved_at = time.time() * 1000
                    shown_apps = [apps[k] for k, other in enumerate(moved["entries"]) if shown(other)]
                    # Every shown entry has reported since the pointer moved:
                    # this one is under it, and no other.
                    wait_for(lambda: (values := {app: observations.get(("entry", free_panel, app), {})
                                                 for app in shown_apps})
                        and all(value.get("sample", 0) > moved_at + 50 for value in values.values())
                        and values[apps[index]].get("hovered")
                        and not any(value.get("hovered") for app, value in values.items() if app != apps[index]),
                        layout + ": travelled entry " + str(index) + " takes the pointer, alone")
                    rows_seen.append({"entry": index, "at": item["c"], "hovered": apps[index]})
                report["hits"][layout] = rows_seen
                motion([1200, 500])
            print("PASS: hit tests follow the travelled entries " + json.dumps(
                {layout: len(value) for layout, value in report["hits"].items()}), flush=True)

            # What the wheel and continuous motion move.
            configure(free_panel, {"layout": "circular"})
            for target, turns, travels in (("panel", True, False), ("both", True, True), ("items", False, True)):
                configure(free_panel, {"panelMotionTarget": target})
                start = rest("circle at rest, motion moves " + target)
                sent = notches(aim(start), 1)
                moved = rest("circle after a notch, motion moves " + target)
                result = {"angleBefore": start["angle"], "angleAfter": moved["angle"],
                          "travelBefore": start["travel"], "travelAfter": moved["travel"],
                          "timing": timing(sent, start, moved), "capture": capture("circle-" + target, moved)}
                report["modes"][target] = result
                assert (abs((moved["angle"] - start["angle"]) % 360 - 15) < 0.01) == turns, (target, result)
                assert moved_by(moved["travel"], start["travel"], 1, count) == travels, (target, result)
            for target in ("items", "panel", "both"):
                configure(free_panel, {"panelMotionTarget": target, "panelRotationMode": "clockwise",
                                       "panelTravelSpeed": 2, "panelRotationSpeed": 90,
                                       "panelRotationTrigger": "idle"})
                wait_for(lambda: state().get("motionTarget") == target, "continuous " + target)
                # Entries that stop travelling first ease into their slots.
                time.sleep(0.3)
                pump()
                first = state()
                time.sleep(1.0)
                pump()
                second = state()
                change = (second["travel"] - first["travel"] + count / 2) % count - count / 2
                result = {"angleChange": round((second["angle"] - first["angle"]) % 360, 2),
                          "travelChange": round(change, 3),
                          "seconds": round((second["at"] - first["at"]) / 1000, 2)}
                report["continuous"][target] = result
                assert (result["angleChange"] > 5) == (target != "items"), (target, result)
                if target == "panel":
                    assert abs(result["travelChange"]) < 0.01, (target, result)
                else:
                    assert result["travelChange"] > 0.5, (target, result)
            # The shell's processor time with continuous travel running, then
            # off: off, nothing in the scene advances (the probes of this test
            # still poll ten times a second).
            shell = int(call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                             "GetConnectionUnixProcessID", "(s)", ("org.kde.plasmashell",)))
            def processor(seconds):
                def ticks():
                    fields = (pathlib.Path("/proc") / str(shell) / "stat").read_text().rsplit(")", 1)[1].split()
                    return int(fields[11]) + int(fields[12])
                first, started = ticks(), time.monotonic()
                while time.monotonic() - started < seconds:
                    pump()
                    time.sleep(0.05)
                return round(100 * (ticks() - first) / os.sysconf("SC_CLK_TCK") / (time.monotonic() - started), 1)
            configure(free_panel, {"panelMotionTarget": "items"})
            wait_for(lambda: state().get("travelActive"), "continuous travel running")
            report["continuous"]["shellCpuPercentRunning"] = processor(3)
            configure(free_panel, {"panelRotationMode": "none", "panelMotionTarget": "items"})
            stopped = rest("continuous motion off")
            report["continuous"]["off"] = {"angle": stopped["angle"], "travel": stopped["travel"]}
            assert stopped["travel"] == round(stopped["travel"]), ("motion off rests in a slot", stopped)
            assert stopped["travelActive"] is False and stopped["rotationActive"] is False, stopped
            report["continuous"]["shellCpuPercentOff"] = processor(3)
            assert report["continuous"]["shellCpuPercentOff"] < report["continuous"]["shellCpuPercentRunning"], \
                report["continuous"]
            print("PASS: motion moves items, the whole panel or both " + json.dumps(
                {"wheel": report["modes"], "continuous": report["continuous"]}), flush=True)

            # Direction: the open shapes turn to the chosen side.
            configure(free_panel, {"layout": "fan"})
            for name, degrees, axis, sign in (("up", 0, 1, -1), ("down", 180, 1, 1),
                                              ("left", -90, 0, -1), ("right", 90, 0, 1)):
                configure(free_panel, {"layoutAngle": degrees})
                faced = rest("fan facing " + name)
                points = [item["c"] for item in faced["entries"] if shown(item)]
                rects = [rect for rect in faced["rects"] if rect[2] > 0]
                mean = [sum(p[k] for p in points) / len(points) - faced["centre"][k] for k in (0, 1)]
                offset = [faced["entries"][0]["c"][k] - faced["entries"][0]["s"][k] for k in (0, 1)]
                hit = [sum(r[k] + r[k + 2] / 2 for r in rects) / len(rects) + offset[k] - faced["centre"][k]
                       for k in (0, 1)]
                report["directions"][name] = {"meanEntryOffset": [round(v, 1) for v in mean],
                                              "meanHitOffset": [round(v, 1) for v in hit],
                                              "capture": capture("fan-" + name, faced)}
                assert sign * mean[axis] > 20 and abs(mean[1 - axis]) < abs(mean[axis]), (name, mean)
                assert sign * hit[axis] > 20 and abs(hit[1 - axis]) < abs(hit[axis]), (name, hit)
            configure(free_panel, {"layoutAngle": 0})
            print("PASS: the fan and its hit regions face up, down, left and right " + json.dumps(
                report["directions"]), flush=True)

        (evidence / (phase + "-report.json")).write_text(json.dumps(report, indent=1))
        (evidence / (phase + "-frames.json")).write_text(json.dumps(motion_trace))
        configure(free_panel, {"layout": "circular", "panelRotationMode": "none"})

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
        if os.environ.get("ARCHDOCK_RENDERING_TRAVEL") == "1":
            run_travel_matrix()
            return
        if os.environ.get("ARCHDOCK_VISIBILITY_DISCRIMINATOR") == "1":
            configure("bottom", {"rendererTier": "procedural2d",
                                 "panelThemeId": "", "completeThemeId": ""})
            click([1200, 500])
            wait_for(lambda: opened("bottom") and observations.get(("host", "bottom", "")), "native host ready")
            window_id = visibility_discriminator()
            assert panel_call("setPanelVisible", "(sb)", ("bottom", True))
            wait_for(lambda: opened("bottom") and any(window["id"] == window_id and not window["hidden"]
                     for window in kwin_geometry()["windows"]), "same native window revealed and reported")
            print("PASS: native reveal restores the same compositor window and lifecycle", flush=True)
            return
        if os.environ.get("ARCHDOCK_RENDERING_MECHANISMS") == "1":
            run_mechanism_matrix()
            return
        if os.environ.get("ARCHDOCK_RENDERING_FOLDERS") == "1":
            run_folder_matrix()
            if os.environ.get("ARCHDOCK_RENDERING_FOLDER_ANCHORS") != "1":
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
            # An edge panel's row follows its edge; it offers no Dock layout.
            configure(panel, {"type": "hybrid",
                              **({"layout": shape, "layoutRadius": 120} if panel == free_panel else {}),
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
