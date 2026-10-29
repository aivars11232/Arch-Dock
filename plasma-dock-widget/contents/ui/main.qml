import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import ArchDock.Rendering 1.0
import ArchDock.Integration 1.0
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as PlasmaDBus
import org.kde.plasma.plasma5support as P5Support
import "FreeEntryPolicy.js" as FreeEntryPolicy
import "SceneDefinition.js" as SceneDefinition

// The Arch Dock Plasma applet. One applet hosts one panel: a native edge
// panel inside a Plasma panel, or a free panel on the desktop. It fetches its
// panel's configuration and entries from the backend over D-Bus, draws them
// with the shared PanelScene, and does what only the host can: pointer,
// wheel and drag-and-drop input, folder popups and window previews, opening,
// collapsing and concealing the panel, and native placement.
PlasmoidItem {
    id: root

    DropBackend { id: dropBackend }

    // Retry on an Arch Dock that does not answer: a stop asked for earlier
    // (Quit, or an older Arch Dock stopped for its update) ends with the
    // click, and the panel asks again; the session bus then starts Arch
    // Dock as usual, outside Plasma.
    P5Support.DataSource {
        id: resumeCommand
        engine: "executable"
        onNewData: function(source) {
            disconnectSource(source)
            root.refresh()
        }
    }

    // Plasma tells a widget when its owner removes it, or the Plasma panel it
    // sits in, and again if the removal is undone. Arch Dock removes the
    // panel entirely once Plasma has deleted it; a profile keeps its own
    // copy.
    function reportRemoval(removing) {
        if (panelId.length > 0)
            callDock("panelRemovalRequested", [panelId, removing]);
    }
    Connections {
        target: Plasmoid
        function onDestroyedChanged(destroyed) { root.reportRemoval(destroyed) }
    }
    Connections {
        target: root.freeSurface ? null : Plasmoid.containment
        ignoreUnknownSignals: true
        function onDestroyedChanged(destroyed) { root.reportRemoval(destroyed) }
    }

    readonly property bool renderingModuleReady: RenderingModuleProbe.ready
    readonly property string panelId: Plasmoid.configuration.panelId || ""
    // In an Arch Dock bar, how far the bar reaches before and after this
    // dock along it and how deep it is, set by the bar: the dock draws the
    // bar's look over all of it, a panel its icons and the bar's other
    // widgets stand inside.
    property real barSurfaceBefore: 0
    property real barSurfaceAfter: 0
    property real barSurfaceThickness: 0
    readonly property bool drawsBarSurface: barSurfaceThickness > 0
    // Read by the bar: where its widgets stand inside the panel drawn along
    // it, and the room this dock keeps where it stands at the bar's end.
    readonly property Item barScene: drawsBarSurface && activeDockRepresentation
        ? activeDockRepresentation.scene || null : null
    readonly property var barWidgetRoom: barScene ? barScene.barWidgetRoom
        : ({ start: 0, end: 0, sideStart: 0, sideEnd: 0 })
    readonly property var barDockRoom: barScene ? barScene.barDockRoom : ({ start: 0, end: 0 })
    readonly property var barEndCaps: barScene ? barScene.barEndCaps : ({ start: 0, end: 0 })
    readonly property real barSurfaceLightness: barScene ? barScene.surfaceLightness : -1
    // Read by the bar: the groups of this dock's icons it lays out among its
    // widgets, and the gap between icons. Set by the bar: where along it
    // each group's first icon stands, by group id.
    readonly property var barGroups: barScene ? barScene.barGroups : []
    readonly property real barIconGap: barScene ? Number(barScene.layoutGeometry.spacing || 0) : 0
    // How far the first icon stands inside the scene: the room a group first
    // along the bar keeps before it, so that the scene starts at the bar's
    // start and the look's end lands on the bar's end, not past it.
    readonly property real barGroupLead: barScene ? Number(barScene.barGroupsStart || 0) : 0
    property var barGroupPlaces: ({})
    // How far the scene moves from the middle of its place in a bar: so that
    // its first group of icons starts where the bar put that group, or, until
    // the bar says, at the start of the place. `lead` is where that group
    // starts inside the scene.
    function barSceneOffset(room, own, lead) {
        let first = Infinity
        for (const group of barGroups) {
            const place = Number((barGroupPlaces || ({}))[group.id])
            if (isFinite(place) && place < first)
                first = place
        }
        if (!isFinite(first))
            return -Math.max(0, room - own) / 2
        // Never before the start of the place: the look's end would reach
        // past the bar and be cut.
        return Math.max(0, first - lead) - (room - own) / 2
    }
    readonly property string configuredPanelType: Plasmoid.configuration.panelType || "hybrid"
    readonly property bool bootstrapPending: panelId.length === 0
        && Boolean(Plasmoid.configuration.bootstrapFreeDock)
    readonly property bool freeSurface: panelId.startsWith("free-") || bootstrapPending
    // A free host's applet stores panelType=empty as an ownership marker, so
    // its content type comes from the panel record the backend publishes.
    // Native applets keep their own configured type.
    readonly property string panelType: freeSurface
        ? String(configuration.type || "empty")
        : ["empty", "launcher", "tasks", "hybrid"].includes(configuredPanelType)
            ? configuredPanelType : "hybrid"
    // A free panel, or an Empty one along an edge, holds the folders, files
    // and applications added to it alone; the other edge panels share their
    // launchers.
    readonly property bool ownsEntries: freeSurface || panelType === "empty"
    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property bool plasmaEditMode: {
        const containment = Plasmoid.containment;
        return containment && containment.corona ? containment.corona.editMode : false;
    }
    readonly property real iconSize: Number(configuration.iconSize || 52)
    readonly property real spacing: Number(configuration.spacing || 8)
    readonly property real baseCellSize: iconSize + Math.max(4, spacing)
    // An edge panel whose icons stand on stands, or float as holograms,
    // reaches past its panel, which Plasma would cut: it is drawn in Arch
    // Dock's own window over the panel (OverflowHost) - while the panel is
    // always shown. Plasma hides an auto-hiding panel through the
    // compositor, unseen by its widgets, and keeps it shown while a window
    // of theirs is; such a panel draws its dock in itself as before.
    readonly property bool dockReachesPastPanel: !freeSurface
        && (String(configuration.pedestalShape || "none") !== "none" || configuration.hologramIcons === true)
        && String(configuration.visibilityMode || "always") === "always"
    // How thick the edge panel is across its edge: the widget's own size
    // there, which Plasma gives it.
    readonly property real panelCrossSize: activeDockRepresentation
        ? (vertical ? activeDockRepresentation.width : activeDockRepresentation.height) : 0
    readonly property real magnification: Number(configuration.magnification || 1.65)
    readonly property real panelOpacity: configuration.opacity === undefined
        ? 0.9 : Number(configuration.opacity)
    readonly property int motionDuration: configuration.reducedMotion
        ? 0 : Math.max(80, Math.min(1200,
            Number(configuration.animationDuration || 170)
            / Math.max(0.2, Number(configuration.animationSpeed || 1))))
    readonly property var capabilityResolution:
        configuration.capabilityResolution || ({})
    // The validated profile selected for this panel, plus the catalog used to
    // resolve reduced-motion substitutes.
    readonly property var activeAnimationProfiles: {
        const profile = root.configuration.animationProfile
        // Idle rotate is a 3D panel's turn of each icon over its stand; a
        // flat panel does not spin its icons flat.
        if (profile && profile.id === "idle-rotate" && root.effectiveRendererTier !== "true3d")
            return []
        return profile && profile.id ? [profile] : []
    }
    readonly property var animationCatalogMap: {
        const result = ({})
        const list = root.configuration.animationProfiles || []
        for (let index = 0; index < list.length; ++index) {
            const profile = list[index]
            if (profile && profile.id)
                result[profile.id] = profile
        }
        return result
    }
    // Profile timings are declared against a 170 ms baseline, so passing the
    // ratio keeps the user's animation-duration setting authoritative.
    readonly property real motionSpeedScale:
        170 / Math.max(80, Math.min(1200, Number(motionDuration) || 170))
    readonly property string effectiveRendererTier:
        String(configuration.effectiveRendererTier || "")
    readonly property bool sceneInputEnabled: freeSurface
        ? FreeEntryPolicy.interactionEnabled(plasmaEditMode)
        : dockService.registered && (!plasmaEditMode || reordersInEditMode)
    // Headroom the panel must reserve so an effect is not clipped by the host
    // it lives in: magnification growth plus the furthest the bound profile can
    // actually travel. Reduced motion holds still, so it reserves nothing.
    readonly property real motionHeadroom: configuration.reducedMotion
        ? 0
        : baseCellSize * MotionChannels.maximumDisplacement(
            activeAnimationProfiles,
            Number(configuration.animationIntensity || 1))
    readonly property real nativeScenePadding: Kirigami.Units.largeSpacing
        + (configuration.magnificationEnabled
            ? baseCellSize * Math.max(0, magnification - 1) : 0)
        + motionHeadroom
    readonly property var scenePanelDefinition: buildScenePanelDefinition()
    // The stands under an opened folder's items: the panel's pedestal
    // settings, their shape chosen on the Pedestals page ("same" keeps the
    // panel's own shape); with "Own settings" on, the folder items' own
    // size, thickness, lift, material, texture, colour, opacity, glow and
    // motion.
    readonly property var folderPedestalSettings: {
        const source = configuration || ({})
        const settings = {}
        const keys = ["Size", "Thickness", "Lift", "ViewAngle", "Material", "Texture", "Picture",
                      "PictureDepth", "PictureRise", "PictureDirection", "PictureLevel", "PictureContrast",
                      "PictureSmoothing", "Color", "Opacity",
                      "Glow", "GlowReach", "Animation", "AnimationSpeed"]
        for (const key of ["pedestalShape"].concat(keys.map(name => "pedestal" + name)))
            if (source[key] !== undefined) settings[key] = source[key]
        // The panel's picture, worn by stands in the panel's look.
        if (source.panelPicture !== undefined) settings.panelPicture = source.panelPicture
        const folderShape = String(source.folderPedestalShape || "same").toLowerCase()
        if (folderShape !== "same") settings.pedestalShape = folderShape
        if (source.folderPedestalOwnStyle === true)
            for (const name of keys)
                if (source["folderPedestal" + name] !== undefined)
                    settings["pedestal" + name] = source["folderPedestal" + name]
        // An opened folder is no panel: its items face the viewer, their
        // stands level, seen View angle degrees from above.
        settings.pedestalFacing = "viewer"
        return settings
    }
    // The folder's stands as drawn: on a panel drawn in 3D they are seen
    // `angle` degrees from above, the way the panel's own stands are
    // (PanelScene.standSeenAngle), so the two match; NaN keeps View angle.
    function folderStandsSeenAt(angle) {
        if (!Number.isFinite(angle)) return folderPedestalSettings
        return Object.assign({}, folderPedestalSettings, { pedestalSeenAngle: angle })
    }
    // The runtime state the scene draws from. Before this, the
    // presentation controller existed but nothing read it, so the live surface
    // was permanently "open" however the panel was configured. These three
    // fields are the wiring: one state machine, one surface.
    readonly property var sceneRuntimeState: ({
        hovered: hoveredIndex >= 0 || panelPointerInside,
        hoveredEntry: hoveredIndex,
        editMode: plasmaEditMode,
        dragInProgress: entryDragActive || panelDropActive,
        popupOpen: presentationController.popupOpen || presentationController.windowPreviewOpen,
        // A hover preview gives way to the wheel; a folder or menu does not.
        modalPopupOpen: presentationController.popupOpen,
        rendererFallback: "",
        presentationState: presentationController.surfaceState,
        transitionState: presentationController.transitionState,
        presentationProgress: presentationController.progress,
        hostPhase: presentationController.hostPhase
    })

    // How the panel is asked to open. "manual" leaves it to an explicit
    // request, which is what a panel driven only by its host wants.
    readonly property string presentationTrigger: {
        const value = String(configuration.presentationTrigger || "hover")
        return ["hover", "click", "edge", "manual"].includes(value)
            ? value : "hover"
    }
    readonly property bool opensOnHover:
        presentationTrigger === "hover" || presentationTrigger === "edge"
    readonly property bool opensOnClick: presentationTrigger === "click"
    readonly property var sceneHostCapabilities:
        capabilityResolution && typeof capabilityResolution === "object"
            && Object.keys(capabilityResolution).length > 0
        ? capabilityResolution : ({
            available: dockService.registered,
            renderer: {
                effectiveTier: effectiveRendererTier || "procedural2d",
                fallbackApplied: false
            }
        })

    property var entries: []
    readonly property bool contentUpdatesVisible: presentationController.hostPhase !== "concealed"
        && presentationController.surfaceState !== "collapsed"
    onContentUpdatesVisibleChanged: {
        if (contentUpdatesVisible) refreshEntries();
    }
    property var nativeHostState: ({})
    readonly property var nativeHostWindow: activeDockRepresentation
        ? activeDockRepresentation.Window.window : null
    readonly property var nativeHostBounds: nativeHostWindow ? ({
        x: nativeHostWindow.x, y: nativeHostWindow.y,
        width: nativeHostWindow.width, height: nativeHostWindow.height
    }) : ({})
    onNativeHostBoundsChanged: refreshNativeHostState()

    function refreshNativeHostState() {
        if (freeSurface || panelId.length === 0 || !nativeHostWindow || !dockService.registered) {
            nativeHostState = ({});
            return;
        }
        const requestedPanel = panelId;
        callDock("nativePanelHostState", [requestedPanel, nativeHostBounds], function(reply) {
            if (root.panelId === requestedPanel)
                root.nativeHostState = normalizeReply(reply) || ({});
        }, function() { root.nativeHostState = ({}); });
    }

    function invalidateContentFeedback() {
        entries = entries.map(function(source) {
            const entry = Object.assign({}, source);
            entry.badgeText = ""; entry.progress = -1; entry.urgent = false;
            entry.temporaryStatus = ""; entry.overlayAvailable = false;
            if (entry.isStatus) {
                entry.statusAvailable = false;
                entry.statusText = qsTr("Unavailable");
            }
            return entry;
        });
    }

    property var configuration: ({
        iconSize: 52,
        spacing: 8,
        opacity: 0.9,
        color: "",
        iconShape: "rounded",
        appearance: "glass",
        themeAsset: "",
        themeFit: "cover",
        themeStatus: "",
        iconStyleDefinition: ({}),
        iconAnimation: "scale",
        animationTrigger: "hover",
        animationSpeed: 1,
        animationIntensity: 1,
        acceptDrops: true,
        magnification: 1.65,
        magnificationEnabled: true,
        magnificationRadius: 1.0,
        magnificationFalloff: "linear",
        showReflections: false,
        showIndicators: true,
        showTooltips: true,
        animationDuration: 170,
        reducedMotion: false,
        animationProfile: ({}),
        animationProfiles: []
    })
    property int hoveredIndex: -1
    property bool requestFailed: false
    property bool bootstrapRequested: false

    // Interaction guards collected from the whole panel. Per-entry guards are
    // reported by the delegates, keyed by entry so two open menus cannot
    // cancel each other out; panel-wide guards come from the representation.
    property var entryGuardFlags: ({})
    property bool panelPointerInside: false
    property bool panelKeyboardFocus: false
    property bool panelDropActive: false

    readonly property bool entryMenuOpen: guardActive("menu")
        || (activeDockRepresentation !== null && activeDockRepresentation.folderExpansionVisible)
    readonly property Item activeDockRepresentation: {
        if (fullRepresentationItem && fullRepresentationItem.visible)
            return fullRepresentationItem;
        if (compactRepresentationItem && compactRepresentationItem.visible)
            return compactRepresentationItem;
        // Retain an owner while concealed so it can report host visibility.
        return fullRepresentationItem || compactRepresentationItem;
    }
    readonly property bool entryPreviewOpen: activeDockRepresentation !== null
        && activeDockRepresentation.windowPreviewVisible === true
    readonly property bool entryDragActive: guardActive("drag")

    function setEntryGuard(index, name, active) {
        const key = String(name) + ":" + String(index);
        const source = entryGuardFlags || ({});
        if (Boolean(source[key]) === Boolean(active))
            return;
        const next = ({});
        for (const existing of Object.keys(source)) {
            if (existing !== key)
                next[existing] = source[existing];
        }
        if (active)
            next[key] = true;
        entryGuardFlags = next;
    }

    function guardActive(name) {
        const prefix = String(name) + ":";
        const source = entryGuardFlags || ({});
        for (const key of Object.keys(source)) {
            if (key.indexOf(prefix) === 0 && source[key])
                return true;
        }
        return false;
    }

    Plasmoid.title: qsTr("Arch Dock")
    Plasmoid.icon: "org.archdock.ArchDock"
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    // A Plasma panel draws its theme's panel background behind its widgets.
    // The dock draws its panel's look itself, so while it stands in a Plasma
    // panel that background is turned off, and given back when the dock
    // leaves. Nothing is saved: Plasma's panel settings stay as they are.
    readonly property var plasmaPanel: !freeSurface && Plasmoid.containment
        && Plasmoid.containment.pluginName === "org.kde.panel" ? Plasmoid.containment : null
    property var heldPlasmaPanel: null
    property int heldPlasmaPanelHints: 0
    function holdPlasmaPanelBackground() {
        if (heldPlasmaPanel === plasmaPanel) return;
        releasePlasmaPanelBackground();
        if (!plasmaPanel) return;
        heldPlasmaPanel = plasmaPanel;
        heldPlasmaPanelHints = plasmaPanel.backgroundHints;
        plasmaPanel.backgroundHints = PlasmaCore.Types.NoBackground;
    }
    function releasePlasmaPanelBackground() {
        if (heldPlasmaPanel) heldPlasmaPanel.backgroundHints = heldPlasmaPanelHints;
        heldPlasmaPanel = null;
    }
    onPlasmaPanelChanged: holdPlasmaPanelBackground()
    // One dock per widget. With switch sizes set, Plasma chooses the
    // representation by the widget's size, ignores this preference, and in
    // its compact mode builds the full representation as well, for the
    // expander popup: every free panel narrower than the switch width and
    // every thin edge panel then held two complete docks, two 3D scenes and
    // two sets of textures, one of them invisible. Without switch sizes the
    // preference decides, and the dock is built once.
    preferredRepresentation: fullRepresentation

    function callDock(methodName, parameters, onResolved, onRejected) {
        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: methodName
        });
        message.iface = "local.PanelWindow";
        message.arguments = parameters || [];
        // KDE's callback overload uses a single-shot connection: a finished
        // callback must not keep its reply and decoded values alive in a cycle.
        function completed(reply) {
            try {
                if (!root)
                    return;
                if (reply.isError) {
                    const error = {
                        name: reply.error.name,
                        message: reply.error.message
                    };
                    if (onRejected)
                        onRejected(error);
                    else
                        console.warn("Arch Dock D-Bus call failed:", methodName,
                                     error.name, error.message);
                    return;
                }
                if (onResolved) {
                    const value = JSON.parse(JSON.stringify(reply.value));
                    onResolved(value);
                }
            } finally {
                reply.destroy();
            }
        }
        PlasmaDBus.SessionBus.asyncCall(message, completed, completed);
    }

    function normalizeReply(reply) {
        if (Array.isArray(reply))
            return reply.map(normalizeReply);
        if (reply && typeof reply === "object") {
            const keys = Object.keys(reply);
            if (keys.length === 1 && keys[0] === "value")
                return normalizeReply(reply.value);
            const result = {};
            for (const key of keys)
                result[key] = normalizeReply(reply[key]);
            return result;
        }
        return reply;
    }

    function buildScenePanelDefinition() {
        return SceneDefinition.build(configuration, {
            freeSurface: freeSurface,
            vertical: vertical,
            effectiveRendererTier: effectiveRendererTier,
            nativeScenePadding: nativeScenePadding,
            baseCellSize: baseCellSize
        });
    }

    function refreshConfiguration(onUpdated) {
        if (panelId.length === 0) {
            requestFailed = false;
            bootstrapFreeDock();
            return;
        }
        if (!dockService.registered) {
            requestFailed = false;
            return;
        }
        callDock("panelRendererConfiguration", [panelId], function(reply) {
            const value = normalizeReply(reply);
            // A panel the backend no longer knows (just removed, its bar or
            // Plasma panel about to go) keeps the look it has until then.
            if (value && typeof value === "object" && Object.keys(value).length > 0) {
                configuration = value;
                if (typeof onUpdated === "function") onUpdated();
            }
        });
    }

    // Plasma 6's scripting geometry setter is a no-op. Restore only our own
    // desktop container, after the backend has removed its preview projection.
    property var pendingAuditionGeometry: null
    // A move the backend asked for is under way, and whether the panel then
    // keeps its distance from the other free panels (a change of place in
    // Panel Studio does; a profile or a preview keeps the place it gives).
    property bool commandedMove: false
    property bool commandedMoveKeepsApart: false
    Timer {
        id: auditionGeometryMove
        interval: 25
        onTriggered: {
            const command = root.pendingAuditionGeometry;
            root.pendingAuditionGeometry = null;
            if (!command || String(Plasmoid.configuration.auditionRestoreGeometry || "") !== command.raw
                    || !root.freeSurface || root.panelId !== command.requested.panelId
                    || Plasmoid.configuration.ownerToken !== command.requested.ownerToken
                    || root.configuredPanelType !== "empty" || Plasmoid.configuration.bootstrapFreeDock)
                return;
            const container = command.container;
            if (!container || !container.applet || !container.applet.plasmoid || !container.layout
                    || container.applet.plasmoid.id !== Plasmoid.id)
                return;
            // Plasma's zero-interval size-hint timer can reposition a newly
            // resized container. Move after that queued resize has settled.
            const position = container.applet.mapToItem(container.layout, 0, 0);
            container.x += command.requested.x - position.x;
            container.y += command.requested.y - position.y;
            container.layout.save();
        }
    }

    function restoreAuditionGeometry() {
        const raw = String(Plasmoid.configuration.auditionRestoreGeometry || "");
        if (!freeSurface || !panelId || !raw || raw.length > 4096) return;
        let requested;
        try { requested = JSON.parse(raw); } catch (error) { return; }
        if (requested.panelId !== panelId ||
            requested.ownerToken !== Plasmoid.configuration.ownerToken ||
            !requested.ownerToken || configuredPanelType !== "empty" ||
            Plasmoid.configuration.bootstrapFreeDock) return;
        for (const key of ["x", "y", "width", "height"])
            if (typeof requested[key] !== "number" || !isFinite(requested[key]) ||
                Math.abs(requested[key]) > 1000000 ||
                ((key === "width" || key === "height") && requested[key] <= 0)) return;
        refreshConfiguration(function() {
            Qt.callLater(function() {
                if (String(Plasmoid.configuration.auditionRestoreGeometry || "") !== raw) return;
                let container = root.parent;
                while (container && !(container.applet && container.layout &&
                       container.applet.plasmoid && container.applet.plasmoid.id === Plasmoid.id))
                    container = container.parent;
                if (!container || typeof container.layout.save !== "function") return;
                // Plasma can reposition a managed container when its size
                // hints grow. Finish resizing before setting the requested
                // position, so that relayout cannot replace the move.
                root.commandedMove = true;
                root.commandedMoveKeepsApart = requested.keepApart === true;
                container.width = requested.width + container.leftPadding + container.rightPadding;
                container.height = requested.height + container.topPadding + container.bottomPadding;
                root.pendingAuditionGeometry = {raw: raw, requested: requested, container: container};
                auditionGeometryMove.restart();
            });
        });
    }

    Connections {
        target: Plasmoid.configuration
        function onAuditionRestoreGeometryChanged() { root.restoreAuditionGeometry(); }
    }

    function refreshEntries() {
        if (panelId.length === 0) {
            entries = [];
            requestFailed = false;
            return;
        }
        if (!dockService.registered) {
            if (!ownsEntries)
                entries = [];
            else invalidateContentFeedback();
            requestFailed = false;
            return;
        }
        callDock("dockEntriesForPanel", [panelId, panelType], function(reply) {
            const value = normalizeReply(reply);
            entries = Array.isArray(value) ? value : [];
            requestFailed = false;
        }, function() {
            if (!FreeEntryPolicy.keepEntriesOnServiceFailure(root.ownsEntries))
                entries = [];
            else invalidateContentFeedback();
            requestFailed = true;
        });
    }

    function refresh() {
        refreshConfiguration();
        refreshEntries();
        refreshNativeHostState();
    }

    function bootstrapFreeDock() {
        if (bootstrapRequested || panelId.length > 0 ||
            !Plasmoid.configuration.bootstrapFreeDock || !dockService.registered)
            return;
        const containment = Plasmoid.containment;
        const containmentId = containment ? Number(containment.id) : -1;
        const appletId = Number(Plasmoid.id);
        if (containmentId < 0 || appletId < 0) {
            requestFailed = true;
            return;
        }
        bootstrapRequested = true;
        callDock("adoptFreePanelApplet", [containmentId, appletId], function(reply) {
            const value = normalizeReply(reply);
            const transaction = Array.isArray(value) && value.length === 1
                ? value[0] : value;
            if (transaction && transaction.success === true
                    && String(transaction.panelId || "").length > 0) {
                Plasmoid.configuration.panelId = String(transaction.panelId);
                Plasmoid.configuration.bootstrapFreeDock = false;
                requestFailed = false;
                refresh();
            } else {
                bootstrapRequested = false;
                requestFailed = true;
            }
        }, function() {
            bootstrapRequested = false;
            requestFailed = true;
        });
    }

    function invokeEntry(methodName, appId, onOutcome) {
        if (methodName === "launchDockEntry") {
            // Launch the selected panel entry's desktop file, including a
            // pinned free entry with a separately identified running instance.
            callDock(methodName, [panelId, appId,
                typeof onOutcome === "string" ? onOutcome : ""], refresh);
            return;
        }
        if (methodName === "activateDockEntry") {
            activateEntry(appId, onOutcome);
            return;
        }
        if (ownsEntries) {
            // Pinning on a panel holding its own entries is panel-specific: an
            // owned entry is removed from this panel only, and a running-only
            // application is added to this panel as its desktop entry. The
            // shared pin list is never touched from such a panel.
            if (methodName === "togglePinnedDockEntry") {
                const ownedId = FreeEntryPolicy.panelEntryId(root.entries, appId);
                if (ownedId.length > 0)
                    callDock("removePanelEntry", [panelId, ownedId], refresh);
                else
                    callDock("addPanelEntries", [panelId, [appId]], refresh);
                return;
            }
            callDock(methodName,
                     [FreeEntryPolicy.actionAppId(root.entries, appId)], refresh);
            return;
        }
        callDock(methodName, [appId], refresh);
    }

    // Activates an entry and reports what actually happened.
    //
    // "succeeded" is only ever reported for an outcome the platform confirmed.
    // When it cannot confirm one - a window handed to the compositor, which
    // never answers - the outcome stays "requested" and the caller must not
    // present it as a success.
    function activateEntry(appId, onOutcome) {
        function report(outcome, reason) {
            if (onOutcome)
                onOutcome({ outcome: outcome, reason: reason || "" });
        }
        let targetAppId = appId;
        if (ownsEntries) {
            // A pinned desktop entry with a running instance activates that
            // instance through the application model; otherwise the entry's
            // own URL is opened.
            targetAppId = FreeEntryPolicy.actionAppId(root.entries, appId);
            const targetUrl = targetAppId === appId
                ? FreeEntryPolicy.encodedUrl(appId) : "";
            if (targetUrl.length > 0) {
                if (Qt.openUrlExternally(targetUrl)) {
                    report("succeeded", "");
                    return;
                }
                if (!dockService.registered) {
                    report("failed", "no-url-handler");
                    return;
                }
            }
        }
        if (!dockService.registered) {
            report("failed", "service-unavailable");
            return;
        }
        callDock("activateDockEntryOutcome", [targetAppId], function(reply) {
            const value = normalizeReply(reply);
            report(value && value.outcome ? String(value.outcome) : "requested",
                   value && value.reason ? String(value.reason) : "");
            refresh();
        }, function(error) {
            report("failed", error && error.name
                   ? String(error.name) : "call-failed");
        });
    }

    // The icon sitting in the panel's decoration's launcher seat.
    function setDecorationSeat(appId) {
        if (dockService.registered)
            callDock("setPanelDecorationSeat", [panelId, String(appId || "")], refresh);
    }

    // A bar's edit mode is Arch Dock's own, with nothing of Plasma's over the
    // dock: its icons are dragged to new places then too. In Plasma's edit
    // mode Plasma moves the whole widget instead.
    readonly property bool reordersInEditMode: drawsBarSurface
    function reorderEntry(appId, beforeAppId) {
        if (!dockService.registered || (plasmaEditMode && !reordersInEditMode) || !configuration.acceptDrops
                || appId === beforeAppId) {
            console.warn("Arch Dock reorder skipped:", appId, "before", beforeAppId, "service",
                dockService.registered, "edit mode", plasmaEditMode, "drops", configuration.acceptDrops);
            return false;
        }
        let movedId = appId;
        let beforeId = beforeAppId;
        if (ownsEntries) {
            // A panel's own entries reorder within it. A running-only entry is
            // not owned by the panel and cannot be ordered.
            movedId = FreeEntryPolicy.panelEntryId(root.entries, appId);
            beforeId = beforeAppId
                ? FreeEntryPolicy.panelEntryId(root.entries, beforeAppId) : "";
            if (movedId.length === 0 || (beforeAppId && beforeId.length === 0)) {
                console.warn("Arch Dock reorder skipped: no panel entry for", appId, "or", beforeAppId);
                return false;
            }
        }
        const accepted = dropBackend.moveEntry(panelId, movedId, beforeId);
        if (accepted) refresh();
        else console.warn("Arch Dock reorder refused:", dropBackend.lastError);
        return accepted;
    }

    // The panels that take applications, folders and files dropped on them:
    // an Empty one, as its own items, and those holding launchers; not one
    // showing open applications alone.
    readonly property bool takesDroppedItems: ["empty", "launcher", "hybrid"].includes(panelType)
    function pinDroppedUrls(urls) {
        if (!dockService.registered || plasmaEditMode || !configuration.acceptDrops || !takesDroppedItems)
            return false;
        const values = [];
        for (const url of urls)
            values.push(url.toString());
        const accepted = dropBackend.addUrls(panelId, ownsEntries, values);
        if (accepted) refresh();
        else {
            console.warn("Arch Dock drop refused:", dropBackend.lastError);
            if (ownsEntries) explainRefusedDrop();
        }
        return accepted;
    }

    // A drop refused because the panel holds as many icons as its shape has
    // room for says so on the panel for a few seconds.
    property string fullNotice: ""
    function explainRefusedDrop() {
        callDock("panelIconRoom", [panelId], function(reply) {
            const room = normalizeReply(reply);
            if (!room || room.success !== true || !(room.capacity >= 0) || room.count < room.capacity) return;
            const ways = qsTr("Make it larger or its icons smaller in Panel Studio.");
            fullNotice = (room.count > room.capacity
                ? qsTr("This panel is full: it holds %1 icons, more than the %2 that fit as it is now.")
                    .arg(room.count).arg(room.capacity)
                : qsTr("This panel is full: %1 icons fit as it is now.").arg(room.capacity)) + " " + ways;
            fullNoticeTimer.restart();
        });
    }
    // A handle let go where the panel's icons would no longer fit: the panel
    // goes back and says why.
    function explainTooSmall() {
        fullNotice = qsTr("Its icons would not fit: this panel cannot be made smaller. Remove icons or make them smaller first.");
        fullNoticeTimer.restart();
    }
    Timer {
        id: fullNoticeTimer
        interval: 4000
        onTriggered: root.fullNotice = ""
    }

    function openPanelStudio() {
        callDock(panelId.length > 0 ? "showPanelSettings" : "showSettings",
                 panelId.length > 0 ? [panelId] : []);
    }

    // How a folder's opening shows its contents, chosen in the folder icon's
    // menu (Icons): Arch Dock keeps it in the icon's own override.
    function setFolderView(appId, values) {
        if (!dockService.registered || panelId.length === 0)
            return;
        callDock("setPanelFolderView", [panelId, String(appId), values], refreshEntries);
    }
    function openIconProperties(entry) {
        const candidate = entry || ({});
        const identity = String(candidate.stableIdentity || "");
        if (!dockService.registered || panelId.length === 0
                || candidate.iconPropertiesSupported !== true
                || identity.length === 0)
            return false;
        callDock("showIconProperties", [panelId, identity], function(reply) {
            const result = normalizeReply(reply);
            if (!result || result.success !== true)
                console.warn("Arch Dock Icon Properties could not open:",
                             result && result.errorCode
                                 ? result.errorCode : "unavailable");
        });
        return true;
    }

    // The panel's own presentation state machine. Phase B feeds it every
    // declared guard; the surface state it publishes is wired into the scene
    // and to host visibility in Phase D.
    PanelPresentationController {
        id: presentationController

        restingState: String(root.configuration.presentationMode || "open")
        openDelay: Number(root.configuration.openDelay || 0)
        closeDelay: Number(root.configuration.closeDelay || 0)
        transitionDuration: root.motionDuration
        reducedMotion: Boolean(root.configuration.reducedMotion)

        popupOpen: root.entryMenuOpen
        dragActive: root.entryDragActive || root.panelDropActive
        pointerInside: root.panelPointerInside
        keyboardFocus: root.panelKeyboardFocus
        editMode: root.plasmaEditMode
        revealZoneActive: root.revealZoneActive
        windowPreviewOpen: root.entryPreviewOpen
        // Panel Studio's preview lock does not apply to the live applet.
        previewLock: false
    }

    // The guards this panel is currently holding, reported to the backend so
    // the host visibility decision cannot conceal a panel that is in use. The
    // controller owns the surface; the backend owns the host; this is the one
    // channel between them.
    readonly property var reportedGuards: ({
        pointerInside: presentationController.pointerInside,
        revealZoneActive: presentationController.revealZoneActive,
        popupOpen: presentationController.popupOpen || presentationController.windowPreviewOpen,
        dragActive: presentationController.dragActive,
        keyboardFocus: presentationController.keyboardFocus,
        editMode: presentationController.editMode
    })

    onReportedGuardsChanged: root.publishGuards()

    function publishGuards() {
        if (panelId.length === 0 || !dockService.registered)
            return;
        callDock("reportPanelInteractionGuards", [panelId, reportedGuards]);
    }

    // The resting surface state and host phase this panel has actually
    // reached, reported to the backend so the live panel can be observed
    // without injecting input. Progress is deliberately excluded: it changes
    // every frame and the observer only needs the endpoints.
    readonly property var reportedPresentation: ({
        surfaceState: presentationController.surfaceState,
        transitionState: presentationController.transitionState,
        hostPhase: presentationController.hostPhase
    })

    onReportedPresentationChanged: root.publishPresentationState()

    LoggingCategory {
        id: rendererLog
        name: "org.archdock.rendering"
        defaultLogLevel: LoggingCategory.Warning
    }

    function publishPresentationState() {
        if (panelId.length === 0 || !dockService.registered)
            return;
        callDock("reportPanelPresentationState", [panelId, reportedPresentation]);
    }

    // Explicit requests are the producer for the "manual" trigger and are
    // honoured for every trigger. A request is taken from the backend exactly
    // once, so a request cannot be replayed after the panel has acted on it.
    function consumePresentationRequest() {
        if (panelId.length === 0 || !dockService.registered)
            return;
        callDock("takePanelPresentationRequest", [panelId], function(reply) {
            const value = normalizeReply(reply);
            if (!value || value.pending !== true)
                return;
            const request = String(value.request || "");
            if (request === "open")
                root.requestPresentationOpen();
            else if (request === "collapse")
                root.requestPresentationCollapse();
        });
    }

    // Opening and closing requests.
    //
    // Hover and edge open the panel when the pointer enters it; click opens it
    // through the tap handler below. Whatever opened it, the pointer leaving
    // asks it to close, and the controller holds that request until every
    // guard clears. "manual" is the one trigger the pointer never drives: such
    // a panel opens and closes only through an explicit request delivered by
    // the backend, so it can rest collapsed without becoming unreachable.
    property bool revealZoneActive: false

    function requestPresentationOpen() {
        presentationController.requestOpen();
    }

    function requestPresentationCollapse() {
        if (String(root.configuration.presentationMode || "open") !== "collapsed")
            return;
        presentationController.requestCollapse();
    }

    onPanelPointerInsideChanged: {
        if (root.presentationTrigger === "manual")
            return;
        if (root.panelPointerInside) {
            if (root.opensOnHover)
                root.requestPresentationOpen();
        } else {
            root.requestPresentationCollapse();
        }
    }

    // The host has actually taken the panel off screen, or put it back. This is
    // an observed fact rather than a request, so it goes straight to the one
    // state machine instead of becoming a second notion of "concealed".
    property bool hostConcealed: false

    onHostConcealedChanged: presentationController.applyHostVisibility(
        !root.hostConcealed)

    Connections {
        target: presentationController

        // A panel reconfigured mid-transition must not keep animating toward a
        // target that no longer exists.
        function onRestingStateChanged() {
            presentationController.reset();
        }
    }

    PlasmaCore.Action {
        id: configurePanelStudioAction
        text: qsTr("Configure Arch Dock…")
        icon.name: "configure"
        onTriggered: root.openPanelStudio()
    }

    // An edge panel Plasma holds for Arch Dock becomes a bar of Arch Dock's
    // own, its Plasma widgets and their settings with it: Arch Dock's looks
    // then reach the whole panel.
    PlasmaCore.Action {
        id: moveIntoBarAction
        text: qsTr("Make it an Arch Dock bar")
        icon.name: "transform-move"
        visible: !root.freeSurface && !root.drawsBarSurface && root.panelId.length > 0
        onTriggered: root.callDock("moveEdgePanelIntoBar", [root.panelId])
    }

    Component {
        id: dockRepresentation

        Item {
            id: representation

            // The scene this dock draws, read where the bar asks for it.
            readonly property Item scene: panelScene

            // Where this free panel is drawn on the screen, and the room its
            // folder's smallest opening needs there, for the other free
            // panels: their folders keep clear of it, and free panels keep
            // that far apart. Once its owner has moved it - on the desktop,
            // in Panel Studio or with the 3D handles - and it stands too
            // close to another, it moves straight away from it, just far
            // enough. Starting up, a profile or a preview moves nothing.
            property var reportedArea: null
            property double reportedSeen: 0
            property double reportedSince: 0
            property bool keepApartArmed: false
            property bool movingItself: false
            property Item placedContainer: null
            property string placeKey: ""
            function seenArea() {
                const bounds = panelScene.seenBounds()
                const a = panelScene.mapToGlobal(bounds.x, bounds.y)
                const b = panelScene.mapToGlobal(bounds.x + bounds.width, bounds.y + bounds.height)
                return { x: Math.min(a.x, b.x), y: Math.min(a.y, b.y),
                         width: Math.abs(b.x - a.x), height: Math.abs(b.y - a.y) }
            }
            // The desktop's container holding this widget.
            function placedItem() {
                let container = root.parent
                while (container && !(container.applet && container.layout && container.applet.plasmoid
                       && container.applet.plasmoid.id === Plasmoid.id))
                    container = container.parent
                return container
            }
            // A change of place: one its owner made arms the distance check,
            // once the panel has stood still a few seconds after starting.
            function placeChanged() {
                const settled = reportedArea !== null && Date.now() - reportedSince > 3000
                if (root.commandedMove)
                    keepApartArmed = keepApartArmed || (settled && root.commandedMoveKeepsApart)
                else if (settled && !movingItself)
                    keepApartArmed = true
                areaReport.restart()
            }
            function reportArea() {
                if (!root.freeSurface || root.panelId.length === 0 || !dockService.registered) return
                if (!placedContainer) placedContainer = placedItem()
                const area = seenArea()
                if (!(area.width > 0 && area.height > 0)) return
                const room = folderTrack.childExtent * panelScene.drawnIconScale()
                root.callDock("reportFreePanelArea", [root.panelId, area.x, area.y, area.width, area.height, room])
                // How much of its track its icons get at its size as drawn,
                // however it is tilted: how many it holds follows it.
                const seen = panelScene.seenTrackFactor(16, false)
                if (Math.abs(seen - reportedSeen) > 0.005) {
                    reportedSeen = seen
                    root.callDock("reportPanelSeenRoom", [root.panelId, seen])
                }
                if (reportedArea === null) reportedSince = Date.now()
                reportedArea = area
                movingItself = false
                root.commandedMove = false
                if (keepApartArmed) {
                    keepApartArmed = false
                    keepApart(area, room)
                }
            }
            function keepApart(area, room) {
                root.callDock("freePanelAreas", [root.panelId], function(reply) {
                    const value = root.normalizeReply(reply)
                    const others = (Array.isArray(value) ? value : []).map(function(other) {
                        return { x: Number(other.x), y: Number(other.y), width: Number(other.width),
                                 height: Number(other.height),
                                 gap: Math.max(room, Number(other.room) || 0) + 8 }
                    })
                    const screen = { x: representation.Screen.virtualX, y: representation.Screen.virtualY,
                                     width: representation.Screen.width, height: representation.Screen.height }
                    const move = LayoutEngine.keepApart(area, others, screen)
                    if (Math.hypot(move.x, move.y) < 0.5) return
                    const container = placedContainer || placedItem()
                    if (!container || !container.layout || typeof container.layout.save !== "function") return
                    movingItself = true
                    container.x += move.x
                    container.y += move.y
                    container.layout.save()
                })
            }
            Timer {
                id: areaReport
                interval: 400
                onTriggered: representation.reportArea()
            }
            Connections {
                target: panelScene
                function onVisibleBoundsChanged() { areaReport.restart() }
            }
            Connections {
                target: representation.placedContainer
                ignoreUnknownSignals: true
                function onXChanged() { representation.placeChanged() }
                function onYChanged() { representation.placeChanged() }
                function onWidthChanged() { representation.placeChanged() }
                function onHeightChanged() { representation.placeChanged() }
            }
            Connections {
                target: dockService
                function onRegisteredChanged() { areaReport.restart() }
            }
            // A 3D panel moved or scaled with its handles or in Panel Studio.
            Connections {
                target: root
                function onConfigurationChanged() {
                    const c = root.configuration || {}
                    const key = [c.scene3DPositionX, c.scene3DPositionY, c.scene3DPositionZ, c.scene3DScale,
                                 c.scene3DScaleX, c.scene3DScaleY].join(",")
                    if (key === representation.placeKey) return
                    const first = representation.placeKey.length === 0
                    representation.placeKey = key
                    if (!first) representation.placeChanged()
                }
            }

            property string previewAppId: ""
            readonly property int previewEntryIndex: {
                for (let index = 0; index < root.entries.length; ++index) {
                    if (String(root.entries[index].appId) === previewAppId)
                        return index
                }
                return -1
            }
            readonly property var previewEntry: previewEntryIndex >= 0
                ? root.entries[previewEntryIndex] : ({})
            readonly property var previewAnchorData: previewEntryIndex >= 0
                ? panelScene.popupAnchors.entries[previewEntryIndex] : null
            readonly property bool windowPreviewVisible: windowPreview.visible
            property string folderAppId: ""
            property int folderRequest: 0
            property bool folderPending: false
            readonly property int folderEntryIndex: root.entries.findIndex(function(entry) {
                return String(entry.appId) === representation.folderAppId && entry.isFolder === true
            })
            readonly property var folderEntry: folderEntryIndex >= 0 ? root.entries[folderEntryIndex] : ({})
            readonly property var folderAnchorData: folderEntryIndex >= 0
                ? panelScene.popupAnchors.entries[folderEntryIndex] : null
            readonly property bool folderExpansionVisible: folderPending || folderExpansion.visible
                || folderTrack.visible
            // A free panel opens its folders on a path outside the dock: a
            // curved one can open them on its own curve, "Along the dock",
            // and any free panel as a fan, an arc or a ring.
            // Grid, and every edge panel, open a popup.
            // Along the dock kept from a curved shape opens as a fan once the
            // panel's shape has no curve (a straight row), never as a popup
            // that cannot draw it.
            // A folder opens as a fan, a grid, a ring or a Möbius band, and
            // on a free panel along the dock, which one saved as an arc does
            // too.
            readonly property string savedFolderLayout: {
                const saved = String(root.configuration.folderLayout || "fan")
                if (saved === "arc" || saved === "track")
                    return root.freeSurface ? "track" : "fan"
                return saved
            }
            readonly property string folderLayout: savedFolderLayout === "track" && root.freeSurface
                && !LayoutEngine.curvedLayout(panelScene.layoutPath) ? "fan" : savedFolderLayout
            // A grid lies on a 3D panel's plane too; elsewhere it is a popup.
            readonly property bool folderOnPath: root.freeSurface
                && (["track", "fan", "arc", "ring", "mobius"].includes(folderLayout)
                    || (folderLayout === "grid" && panelScene.folderPlaneReady))
            onFolderEntryIndexChanged: if (folderEntryIndex < 0) hideFolderExpansion()

            // The folder's opening as its icon asks (Icons in its menu): the
            // children's icon size by its name, and previews.
            readonly property var folderView: folderEntry && folderEntry.folderView ? folderEntry.folderView : ({})
            readonly property int folderChildSize: ({ "tiny": 16, "very-small": 22, "small": 32, "small-medium": 48,
                "medium": 64, "large": 96, "huge": 128 })[String(folderView.iconSize || "small-medium")] || 48
            // Previews come as KIO makes them: the opening asks for its
            // contents again a few times after it opened.
            Timer {
                id: previewRefresh
                property int request: -1
                property int left: 0
                interval: 1500
                onTriggered: {
                    if (request !== representation.folderRequest || representation.folderEntryIndex < 0)
                        return
                    root.callDock("panelFolderSnapshot", [root.panelId, representation.folderAppId], function(reply) {
                        if (previewRefresh.request !== representation.folderRequest)
                            return
                        const value = root.normalizeReply(reply)
                        const snapshot = Array.isArray(value) && value.length === 1 ? value[0] : value
                        const host = representation.folderOnPath && !folderExpansion.requested ? folderTrack : folderExpansion
                        host.snapshot = snapshot
                        if (Number(snapshot.previewsPending || 0) > 0 && --previewRefresh.left > 0)
                            previewRefresh.restart()
                    })
                }
            }
            function hideFolderExpansion() {
                ++folderRequest
                folderPending = false
                folderExpansion.closeFolder()
                folderTrack.closeFolder()
            }
            function showFolderExpansion(entry) {
                if (!folderExpansion.interactionAllowed) return false
                hideWindowPreview()
                folderAppId = String(entry.appId || "")
                if (folderEntryIndex < 0) return false
                const request = ++folderRequest
                folderPending = true
                function show(snapshot) {
                    if (request !== representation.folderRequest || !folderExpansion.interactionAllowed
                            || representation.folderEntryIndex < 0) return
                    const host = representation.folderOnPath ? folderTrack : folderExpansion
                    host.snapshot = snapshot
                    // A path that cannot be traced opens the popup instead.
                    if (!host.openFolder() && host === folderTrack) {
                        folderExpansion.snapshot = snapshot
                        folderExpansion.openFolder()
                    }
                    representation.folderPending = false
                    if (Number(snapshot.previewsPending || 0) > 0) {
                        root.callDock("requestFolderPreviews", [root.panelId, representation.folderAppId])
                        previewRefresh.request = request
                        previewRefresh.left = 3
                        previewRefresh.restart()
                    }
                }
                root.callDock("panelFolderSnapshot", [root.panelId, folderAppId], function(reply) {
                    const value = root.normalizeReply(reply)
                    show(Array.isArray(value) && value.length === 1 ? value[0] : value)
                }, function() { show({ status: "unavailable", entries: [] }) })
                return true
            }

            function showWindowPreview(entry, keyboard) {
                hideFolderExpansion()
                previewAppId = String(entry.appId || "")
                return windowPreview.openPreview(keyboard)
            }
            function hideWindowPreview() { windowPreview.closePreview() }

            // The clicked folder's drawn icon. Its popup opens against this
            // square, on the side that faces out of the dock.
            Item {
                id: folderIcon
                parent: panelScene
                readonly property real size: panelScene.layoutGeometry.iconSize
                x: representation.folderAnchorData ? representation.folderAnchorData.x
                    - representation.folderAnchorData.outwardNormal.x * size / 2 - size / 2 : 0
                y: representation.folderAnchorData ? representation.folderAnchorData.y
                    - representation.folderAnchorData.outwardNormal.y * size / 2 - size / 2 : 0
                width: size; height: size
            }
            FolderExpansionHost {
                id: folderExpansion
                folderItem: folderIcon
                folderTitle: String(representation.folderEntry.displayName || "")
                folderLayout: representation.folderLayout
                folderSpeed: Number(root.configuration.folderSpeed || 260)
                folderEasing: String(root.configuration.folderEasing || "outBack")
                reducedMotion: Boolean(root.configuration.reducedMotion)
                showNames: root.configuration.folderShowNames !== false
                childIconSize: representation.folderChildSize
                iconStyleDefinition: root.configuration.iconStyleDefinition || ({})
                scrollSensitivity: panelScene.folderScrollSensitivity
                panelEdge: root.freeSurface ? "free" : String(root.configuration.edge || "bottom")
                outwardNormal: representation.folderAnchorData
                    ? representation.folderAnchorData.outwardNormal : ({ x: 0, y: -1 })
                interactionAllowed: representation.authoritativeHost && dockService.registered
                    && root.sceneInputEnabled && !root.plasmaEditMode
                    && !root.entryDragActive && !root.requestFailed
                onInteractionAllowedChanged: if (!interactionAllowed) representation.hideFolderExpansion()
                onChildSelected: childId => root.callDock("openPanelFolderChild",
                    [root.panelId, representation.folderAppId, childId], root.refreshEntries)
            }
            FolderTrackHost {
                id: folderTrack
                folderItem: folderIcon
                folderLayout: representation.folderLayout
                trackSamples: function(extra, span) {
                    // One icon and a gap out of the dock, centred on the
                    // folder, `span` radians round it: a quarter of the way
                    // unless the host asks for more.
                    const turn = Number(span) || Math.PI / 2
                    return panelScene.folderTrackSamples(representation.folderEntryIndex,
                        panelScene.layoutGeometry.iconSize + 16 + (Number(extra) || 0), turn,
                        Math.max(31, Math.round(91 * turn / (2 * Math.PI / 3))))
                }
                outwardNormal: representation.folderAnchorData
                    ? representation.folderAnchorData.outwardNormal : ({ x: 0, y: -1 })
                obstacles: function() {
                    return panelScene.folderObstacles(representation.folderEntryIndex)
                }
                outwardRay: function() {
                    // The shape opens from the platform's rim itself.
                    return panelScene.folderOutwardRay(representation.folderEntryIndex, 0)
                }
                // On a 3D panel the shape lies on the panel's own plane.
                folderPlane: function() {
                    return panelScene.folderPlane(representation.folderEntryIndex)
                }
                // What its contents keep clear of on a 3D panel.
                keepOut: function() { return panelScene.folderKeepOut() }
                // A ring the size of the panel needs a dock with a radius.
                dockCentre: function() {
                    return LayoutEngine.supportsWholeSceneRotation(panelScene.layoutPath)
                        ? panelScene.dockCentre() : null
                }
                fanOpening: Number(root.configuration.folderFanOpening || 90)
                ringSize: String(root.configuration.folderRingSize || "small")
                trackReach: String(root.configuration.folderTrackReach || "auto")
                appearance: panelScene.appearance
                customColor: panelScene.customColor
                panelOpacity: panelScene.panelOpacity
                scrollSensitivity: panelScene.folderScrollSensitivity
                // As large as the dock's icons, unless the folder's menu set a size.
                iconSize: representation.folderView.iconSize ? representation.folderChildSize
                    : panelScene.layoutGeometry.iconSize
                folderTitle: String(representation.folderEntry.displayName || "")
                folderSpeed: Number(root.configuration.folderSpeed || 260)
                folderEasing: String(root.configuration.folderEasing || "outBack")
                reducedMotion: Boolean(root.configuration.reducedMotion)
                showNames: root.configuration.folderShowNames !== false
                iconStyleDefinition: root.configuration.iconStyleDefinition || ({})
                pedestalSettings: root.folderStandsSeenAt(panelScene.standSeenAngle)
                interactionAllowed: folderExpansion.interactionAllowed
                onInteractionAllowedChanged: if (!interactionAllowed) representation.hideFolderExpansion()
                onChildSelected: childId => root.callDock("openPanelFolderChild",
                    [root.panelId, representation.folderAppId, childId], root.refreshEntries)
            }

            Item {
                id: previewAnchor
                parent: panelScene
                x: representation.previewAnchorData ? representation.previewAnchorData.x : 0
                y: representation.previewAnchorData ? representation.previewAnchorData.y : 0
                width: 1
                height: 1
            }

            WindowPreviewHost {
                id: windowPreview
                visualParent: previewAnchor
                windowEntries: representation.previewEntry.windowPreviews || []
                applicationTitle: String(representation.previewEntry.displayName || "")
                panelEdge: root.freeSurface ? "free" : String(root.configuration.edge || "bottom")
                outwardNormal: representation.previewAnchorData
                    ? representation.previewAnchorData.outwardNormal : ({ x: 0, y: -1 })
                anchorHovered: root.hoveredIndex === representation.previewEntryIndex
                    && root.panelPointerInside
                interactionAllowed: representation.authoritativeHost && dockService.registered
                    && root.sceneInputEnabled && !root.plasmaEditMode
                    && !root.entryDragActive && !root.requestFailed
                onWindowSelected: windowId => {
                    const appId = FreeEntryPolicy.actionAppId(root.entries, representation.previewAppId)
                    root.callDock("activateDockWindow", [appId, windowId], root.refreshEntries)
                }
                onWindowActionRequested: (windowId, action) => {
                    const appId = FreeEntryPolicy.actionAppId(root.entries, representation.previewAppId)
                    root.callDock("requestDockWindowAction", [appId, windowId, action], root.refreshEntries)
                }
            }

            // Size is deliberately independent of the collapse.
            //
            // Native: the scene's size comes from the panel's entries and its
            // theme slice, never from the presentation track, so collapsing
            // moves artwork inside fixed bounds and the Plasma panel geometry
            // is not renegotiated on every hover. That is the first safe
            // implementation the plan asks for; animating a real panel's length
            // is a later capability-gated change.
            //
            // Free: the widget must additionally cover the theme's declared
            // effect margins, so a glow or an open-state overhang is not
            // clipped by the applet that draws it.
            readonly property real sceneEffectWidth:
                Math.max(panelScene.width,
                         Number(panelScene.effectBounds.width || 0))
            readonly property real sceneEffectHeight:
                Math.max(panelScene.height,
                         Number(panelScene.effectBounds.height || 0))

            // An edge panel is as long again as its decoration's ends reach.
            // Holding nothing, it keeps a square of its thickness, where the
            // sign inviting drops stands, clear of the widgets beside it.
            implicitWidth: root.freeSurface ? sceneEffectWidth
                : Math.max(panelScene.width + Number(panelScene.decorationAlongRoom.width || 0),
                           root.entries.length === 0 && !root.vertical ? height : 0)
            implicitHeight: root.freeSurface ? sceneEffectHeight
                : Math.max(panelScene.height + Number(panelScene.decorationAlongRoom.height || 0),
                           root.entries.length === 0 && root.vertical ? width : 0)
            Layout.minimumWidth: implicitWidth
            Layout.minimumHeight: implicitHeight

            // A free panel has no edge of its own: its 3D scene is drawn
            // out of the widget's box as far as it reaches. The pointer over
            // what is drawn there is over the panel, and presses, the wheel
            // and drops reach it; the empty desktop around it stays the
            // desktop's. The widget's own size stays the box's, so Plasma
            // keeps it where it was put.
            containmentMask: root.freeSurface ? drawnArea : null
            QtObject {
                id: drawnArea
                function contains(point: point): bool {
                    if (point.x >= 0 && point.y >= 0
                            && point.x <= representation.width && point.y <= representation.height)
                        return true
                    const local = representation.mapToItem(panelScene, point.x, point.y)
                    return panelScene.containsInputPoint(Qt.point(local.x, local.y))
                }
            }

            // The panel is concealed when its own item is hidden or fully
            // transparent, or when the window holding it is not on screen -
            // which is what a Plasma auto-hide panel does. Nothing is inferred
            // from focus: a dock stays visible while another window is active.
            //
            // This is reported to the presentation controller rather than used
            // directly: host concealment and surface collapse are separate
            // layers, and only the controller may combine them.
            // Wayland auto-hide can leave QWindow.visible true. The existing
            // KWin watcher supplies actual hidden state for a unique matching
            // native frame; ambiguous frames remain unavailable.
            readonly property bool hostConcealed: !visible || opacity <= 0
                || (!root.freeSurface && root.nativeHostState.available === true
                    && root.nativeHostState.hidden === true)
                || (Window.window !== null
                    && (!Window.window.visible
                        || Window.visibility === Window.Hidden
                        || Window.visibility === Window.Minimized))

            // Desktop applets may instantiate both representations, while a
            // thin native panel may instantiate only the compact one. Only
            // the displayed owner may report host facts or open its preview.
            readonly property bool authoritativeHost:
                root.activeDockRepresentation === representation

            function publishHostConcealed() {
                if (authoritativeHost)
                    root.hostConcealed = hostConcealed;
            }

            onHostConcealedChanged: {
                publishHostConcealed()
                if (hostConcealed) {
                    hideWindowPreview()
                    hideFolderExpansion()
                }
            }
            onAuthoritativeHostChanged: publishHostConcealed()
            Component.onCompleted: publishHostConcealed()

            onActiveFocusChanged: root.panelKeyboardFocus = activeFocus
            Component.onDestruction: {
                root.panelPointerInside = false;
                root.panelKeyboardFocus = false;
                root.panelDropActive = false;
            }

            // The dock as it is drawn and handled: in the panel, or, on an edge
            // panel whose stands and holograms reach past it, in Arch Dock's
            // own clear window over the panel (OverflowHost), laid exactly
            // over this widget, which stays in the panel as the dock's place.
            Item {
                id: dockContent

                parent: overflowHost.hosting ? overflowHost.contentItem : representation
                x: overflowHost.hosting ? overflowHost.dockPlace.x : 0
                y: overflowHost.hosting ? overflowHost.dockPlace.y : 0
                width: representation.width
                height: representation.height

                // Panel-wide interaction guards. The pointer being anywhere over
                // the panel holds it open, not merely the pointer being over an
                // icon, so the gaps between entries are not a way to make a dock
                // close under the user's cursor.
                HoverHandler {
                    id: panelHover

                    onHoveredChanged: root.panelPointerInside = hovered
                }

                // The reveal zone is the strip the panel keeps when it is
                // collapsed. It is deliberately inside the applet's own bounds: an
                // edge-approach detector outside the widget is not something this
                // host can honestly provide, so the handle the user can actually
                // see is what opens the panel.
                //
                // The scene is thicker than a Plasma panel (room for magnification
                // and motion) and centred in it, so the scene's own handle strip
                // lies outside the panel window on every edge (audit E-01). The
                // strip is laid along this item's own edge instead: the whole
                // panel length, as thick as the Reveal zone setting.
                Item {
                    id: revealZone

                    readonly property string edge: root.freeSurface ? "bottom"
                        : String(root.configuration.edge || "bottom")
                    readonly property bool alongWidth: edge === "top" || edge === "bottom"
                    readonly property real thickness: Math.max(1, Number(alongWidth
                        ? panelScene.revealHandle.height : panelScene.revealHandle.width) || 8)

                    x: edge === "right" ? Math.max(0, parent.width - thickness) : 0
                    y: edge === "bottom" ? Math.max(0, parent.height - thickness) : 0
                    width: alongWidth ? parent.width : Math.min(thickness, parent.width)
                    height: alongWidth ? Math.min(thickness, parent.height) : parent.height
                    visible: root.presentationTrigger === "edge"
                        && presentationController.hostVisible
                        && panelScene.collapseProgress > 0
                    z: 30

                    HoverHandler {
                        enabled: revealZone.visible
                        onHoveredChanged: root.revealZoneActive = hovered
                    }
                }

                // Click-to-open applies only while the panel is closed, so it can
                // never intercept a click meant for an entry that is on screen.
                TapHandler {
                    enabled: root.opensOnClick
                        && panelScene.collapseProgress >= 1
                        && !root.plasmaEditMode
                    gesturePolicy: TapHandler.ReleaseWithinBounds
                    onTapped: root.requestPresentationOpen()
                }

                PanelScene {
                    id: panelScene

                    readonly property string rendererObservation: JSON.stringify({
                        panelId: root.panelId, appletId: Number(Plasmoid.id),
                        themeId: String((root.configuration.themeDefinition || ({})).id || ""),
                        requested: requestedRendererTier, effective: effectiveRendererTier,
                        reason: fallbackReason,
                        triangles: activeSurfaceRenderer ? Number(activeSurfaceRenderer.triangleCount || 0) : 0,
                        frameRendered: activeSurfaceRenderer ? Boolean(activeSurfaceRenderer.frameRendered) : false,
                        quality: activeSurfaceRenderer ? activeSurfaceRenderer.qualityState || ({}) : ({})
                    })
                    onRendererObservationChanged: {
                        if (requestedRendererTier === "true3d")
                            console.debug(rendererLog, "ArchDockRenderer " + rendererObservation);
                    }

                    // A decoration's launcher seat takes an icon dropped on it:
                    // one of the panel's own, or an application or a file from
                    // elsewhere, which the panel gains.
                    DropArea {
                        id: seatDropArea
                        readonly property var seat: panelScene.decorationSeat
                        readonly property bool present: seat !== null && seat !== undefined && Number(seat.size) > 0
                        visible: present
                        enabled: present && Boolean(root.configuration.acceptDrops) && !root.plasmaEditMode
                            && root.sceneInputEnabled
                        x: present ? Number(seat.x) - Number(seat.size) * 0.65 : 0
                        y: present ? Number(seat.y) - Number(seat.size) * 0.65 : 0
                        width: present ? Number(seat.size) * 1.3 : 0
                        height: width
                        keys: ["application/x-archdock-app", "text/uri-list"]
                        onDropped: drop => {
                            drop.accepted = false;
                            const source = drop.source;
                            const appId = source && source.entry && source.entry.appId !== undefined
                                ? String(source.entry.appId)
                                : String(drop.getDataAsString("application/x-archdock-app") || "");
                            if (appId.length > 0) {
                                root.setDecorationSeat(appId);
                                drop.acceptProposedAction();
                            } else if (drop.hasUrls && drop.urls.length > 0 && dockService.registered) {
                                root.callDock("seatPanelUrl", [root.panelId, String(drop.urls[0])], root.refresh);
                                drop.acceptProposedAction();
                            }
                        }
                    }

                    anchors.centerIn: parent
                    // An edge panel's decoration may reach farther at one end;
                    // in a bar it spans the whole bar already, and the bar's
                    // reach is measured from where the scene stands. In a bar
                    // its groups of icons stand where the bar laid them out
                    // among its widgets.
                    anchors.horizontalCenterOffset: root.freeSurface ? 0
                        : root.drawsBarSurface ? (root.vertical ? 0 : root.barSceneOffset(parent.width, width, barGroupsStart))
                        : Number(decorationAlongRoom.offsetX || 0)
                    anchors.verticalCenterOffset: root.freeSurface ? 0
                        : root.drawsBarSurface ? (root.vertical ? root.barSceneOffset(parent.height, height, barGroupsStart) : 0)
                        : Number(decorationAlongRoom.offsetY || 0)
                    barVertical: root.vertical
                    segmentPlaces: root.drawsBarSurface ? (root.barGroupPlaces || ({})) : ({})
                    // Along a bar holding other widgets, from where the bar
                    // starts to where it ends (not from the window the dock
                    // overflows into, which is no bar).
                    surfaceSpanBefore: root.drawsBarSurface && !overflowHost.hosting
                        ? Math.max(0, root.barSurfaceBefore + (root.vertical ? y : x)) : 0
                    surfaceSpanAfter: root.drawsBarSurface && !overflowHost.hosting
                        ? Math.max(0, root.barSurfaceAfter + (root.vertical ? dockContent.height - y - height
                                                                            : dockContent.width - x - width)) : 0
                    surfaceSpanThickness: root.drawsBarSurface && !overflowHost.hosting
                        ? root.barSurfaceThickness : 0
                    panelThickness: root.freeSurface || root.drawsBarSurface ? 0 : root.panelCrossSize
                    panelDefinition: root.scenePanelDefinition
                    runtimeState: root.sceneRuntimeState
                    orderedEntries: root.entries
                    hostCapabilities: root.sceneHostCapabilities
                    themeDefinition: root.configuration.themeDefinition || ({})
                    iconStyleDefinition:
                        root.configuration.iconStyleDefinition || ({})
                    animationProfiles: ({
                        reducedMotion: root.configuration.reducedMotion,
                        duration: root.motionDuration
                    })
                    entryDelegate: liveEntryDelegate
                    // While Panel Studio holds a desktop 3D edit, presses belong
                    // to the handles and no application can start.
                    entryInteractionEnabled: root.sceneInputEnabled
                        && !Boolean(root.configuration.sceneEditActive)
                    sceneEditActive: Boolean(root.configuration.sceneEditActive)
                    onSceneTransformEdited: function(values) {
                        root.callDock("updateSceneEditDraft", [root.panelId, values], function(reply) {
                            const result = root.normalizeReply(reply);
                            if (result && result.errorCode === "panel-too-small") root.explainTooSmall();
                        });
                    }
                    // Turning the dock with the wheel closes a hover preview,
                    // which would otherwise stand beside an icon that moved.
                    onWheelUsed: if (root.activeDockRepresentation)
                        root.activeDockRepresentation.hideWindowPreview()
                    sceneConcealed: !presentationController.hostVisible
                    geometryCompatibilityProfile: root.freeSurface
                        ? "live" : "canonical"
                    entryDelegateContext: ({
                        hostKind: root.freeSurface ? "free" : "native",
                        vertical: root.vertical
                    })
                }

                Column {
                    id: emptyHint

                    objectName: "dock-empty-hint"
                    anchors.centerIn: parent
                    // In a bar, in the room the bar keeps for the dock's icons
                    // among its widgets.
                    anchors.horizontalCenterOffset: root.drawsBarSurface && !root.vertical ? barSlotOffset(parent.width) : 0
                    anchors.verticalCenterOffset: root.drawsBarSurface && root.vertical ? barSlotOffset(parent.height) : 0
                    z: 20
                    spacing: Kirigami.Units.smallSpacing
                    visible: root.entries.length === 0
                    // Along an edge or in a bar the panel is too thin for words:
                    // its sign alone, sized to the panel, the words when the
                    // pointer rests on it, and a click on it does what Retry
                    // would.
                    readonly property bool compact: !root.freeSurface
                    readonly property bool retries: !dockService.registered || root.requestFailed
                    // A widget added from Plasma's widget list has no panel
                    // behind it and never gets one by itself (its bootstrap
                    // is off); it must not invite drops.
                    readonly property bool unbound: root.panelId.length === 0
                        && !Plasmoid.configuration.bootstrapFreeDock
                    readonly property string message: !dockService.registered
                        ? qsTr("Arch Dock service is unavailable")
                        : root.requestFailed
                            ? qsTr("Could not load dock entries")
                            : unbound
                                ? qsTr("This widget has no panel. Create one in Panel Studio: Panels, then New free panel at the end of the panel list, or with Add Panel > Arch Dock Circular Free Dock; then remove this widget.")
                                : qsTr("Drop applications or folders here, from the application menu or a file manager")
                    function retry() {
                        if (dockService.registered)
                            root.refresh()
                        else
                            resumeCommand.connectSource("arch-dock --resume")
                    }
                    // From the middle of the dock's room (`room` long) to the
                    // middle of the place the bar keeps for its icons.
                    function barSlotOffset(room) {
                        const group = (root.barGroups || [])[0]
                        const place = group ? Number((root.barGroupPlaces || ({}))[group.id]) : NaN
                        return isFinite(place) ? place + Number(group.length || 0) / 2 - room / 2 : 0
                    }

                    Kirigami.Icon {
                        objectName: "dock-empty-hint-sign"
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: emptyHint.compact
                            ? Math.max(Kirigami.Units.iconSizes.small, Math.min(Kirigami.Units.iconSizes.medium,
                                Math.min(emptyHint.parent.width, emptyHint.parent.height) - 2 * Kirigami.Units.smallSpacing))
                            : Kirigami.Units.iconSizes.medium
                        height: width
                        source: dockService.registered
                            ? (root.requestFailed ? "data-error" : emptyHint.unbound ? "dialog-information" : "list-add")
                            : "network-disconnect"
                        HoverHandler {
                            id: emptyHintHover
                            enabled: emptyHint.compact
                        }
                        TapHandler {
                            enabled: emptyHint.compact && emptyHint.retries
                            onTapped: emptyHint.retry()
                        }
                        QQC2.ToolTip.visible: emptyHint.compact && emptyHintHover.hovered
                        QQC2.ToolTip.text: emptyHint.message
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: !emptyHint.compact
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        width: Math.min(implicitWidth, Math.max(120, parent.width - 2 * Kirigami.Units.largeSpacing))
                        text: emptyHint.message
                    }
                    QQC2.Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Retry")
                        visible: !emptyHint.compact && emptyHint.retries
                        onClicked: emptyHint.retry()
                    }
                }

                Rectangle {
                    id: fullNoticeBox
                    visible: root.fullNotice.length > 0
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 2 * Kirigami.Units.largeSpacing,
                                    fullNoticeText.implicitWidth + 2 * Kirigami.Units.largeSpacing)
                    height: fullNoticeText.implicitHeight + 2 * Kirigami.Units.smallSpacing
                    radius: Kirigami.Units.smallSpacing
                    color: Qt.rgba(0, 0, 0, 0.75)
                    z: 1000
                    QQC2.Label {
                        id: fullNoticeText
                        anchors.centerIn: parent
                        width: parent.width - 2 * Kirigami.Units.largeSpacing
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        color: "white"
                        text: root.fullNotice
                    }
                }

                DropArea {
                    id: panelDropArea

                    anchors.fill: parent
                    containmentMask: root.freeSurface ? drawnArea : null
                    enabled: Boolean(root.configuration.acceptDrops) && !root.plasmaEditMode
                        && root.sceneInputEnabled && root.takesDroppedItems
                    keys: ["text/uri-list"]
                    onContainsDragChanged: root.panelDropActive = containsDrag
                    onDropped: drop => {
                        drop.accepted = false;
                        try {
                            if (drop.hasUrls && root.pinDroppedUrls(drop.urls))
                                drop.acceptProposedAction();
                        } finally {
                            root.panelDropActive = false;
                        }
                    }
                }
            }

            OverflowHost {
                id: overflowHost

                edge: String(root.configuration.edge || "bottom")
                placeholder: representation
                wanted: root.dockReachesPastPanel && representation.authoritativeHost
                // Plasma's edit mode handles the panel and its widgets: the
                // dock is still drawn here, and every press goes to Plasma.
                takesPointer: !root.plasmaEditMode
                // How far the dock is drawn past the panel, away from the
                // screen edge (the scene's own drawn bounds), and where.
                reach: {
                    const drawn = panelScene.drawnBounds
                    const top = panelScene.y + drawn.y, left = panelScene.x + drawn.x
                    const bottom = top + drawn.height, right = left + drawn.width
                    return edge === "top" ? bottom - representation.height
                        : edge === "left" ? right - representation.width
                        : edge === "right" ? -left : -top
                }
                shownRects: (panelScene.entryShownRects || []).map(function(rect) {
                    return Qt.rect(panelScene.x + rect.x, panelScene.y + rect.y, rect.width, rect.height)
                })
            }
        }
    }

    Component {
        id: liveEntryDelegate

        DockEntry {
            id: liveEntry
            anchors.centerIn: parent
            entry: parent.sceneEntry
            entryIndex: parent.sceneIndex
            iconStyleDefinition:
                parent.sceneIconStyleDefinition || ({})
            iconOverrideResolution:
                parent.sceneEntry.iconOverrideResolution || ({})
            vertical: root.freeSurface ? false : root.vertical
            baseSize: Number(parent.sceneGeometry.iconSize || root.baseCellSize)
            entryGeometry: parent.sceneGeometry
            magnification: root.magnification
            magnificationEnabled: root.configuration.magnificationEnabled
            magnificationRadius: Number(
                root.configuration.magnificationRadius || 1.0)
            magnificationFalloff: String(
                root.configuration.magnificationFalloff || "linear")
            hoveredIndex: root.hoveredIndex
            tileShape: root.configuration.iconShape
            tileSettings: root.configuration
            // Given by the scene: this delegate is made outside the
            // representation, where `panelScene` is not in reach (it read as
            // undefined, and a free panel's stands stood as in a row).
            pedestalOnPath: parent.scenePedestalOnPath === true
            screenEdge: String(parent.sceneScreenEdge || "bottom")
            panelThickness: root.freeSurface ? 0 : root.panelCrossSize
            seatAvailable: parent.sceneSeatAvailable === true
            seated: parent.sceneSeated === true
            setSeat: root.setDecorationSeat
            appearance: root.configuration.appearance
            showReflection: root.configuration.showReflections
            showIndicator: root.configuration.showIndicators
            showTooltip: root.configuration.showTooltips
            motion: root.configuration.iconAnimation
            motionTrigger: root.configuration.animationTrigger
            motionIntensity: root.configuration.animationIntensity
            motionDuration: root.motionDuration
            motionSpeed: root.motionSpeedScale
            animationProfiles: root.activeAnimationProfiles
            animationCatalog: root.animationCatalogMap
            reducedMotion: root.configuration.reducedMotion
            inputEnabled: parent.sceneInputEnabled
            inputContainsPoint: function(point) {
                return parent.sceneEntryPointVisible(liveEntry.mapToItem(parent, point.x, point.y));
            }
            sceneVisible: parent.sceneVisible
            meshVisualActive: parent.sceneMeshActive
            solidDepth: Number(parent.sceneIconDepth || 0)
            solidRelief: String(parent.sceneIconRelief || "modelled")
            solidOnFlat: Boolean(parent.sceneIconSolidOnFlat)
            solidTurn: Number(parent.sceneIconTurn || 0)
            solidLighting: Number(parent.sceneIconLighting === undefined ? 0.5 : parent.sceneIconLighting)
            motionContextKey: parent.sceneMotionContextKey
            editMode: root.plasmaEditMode
            reordersInEditMode: root.reordersInEditMode
            acceptDrops: root.configuration.acceptDrops
            invoke: root.invokeEntry
            reorder: root.reorderEntry
            setFolderView: root.setFolderView
            pinUrls: root.pinDroppedUrls
            setHoveredIndex: function(value) { root.hoveredIndex = value }
            setEntryGuard: root.setEntryGuard
            openWindowPreview: function(entry, keyboard) {
                return root.activeDockRepresentation !== null
                    && root.activeDockRepresentation.showWindowPreview(entry, keyboard)
            }
            closeWindowPreview: function() {
                if (root.activeDockRepresentation !== null)
                    root.activeDockRepresentation.hideWindowPreview()
            }
            folderExpandOnClick: root.configuration.folderExpandOnClick !== false
            openFolderExpansion: function(entry) {
                return root.activeDockRepresentation !== null
                    && root.activeDockRepresentation.showFolderExpansion(entry)
            }
            closeFolderExpansion: function() {
                if (root.activeDockRepresentation !== null)
                    root.activeDockRepresentation.hideFolderExpansion()
            }
            openPanelStudio: root.openPanelStudio
            openIconProperties: root.openIconProperties
        }
    }

    compactRepresentation: dockRepresentation
    fullRepresentation: dockRepresentation

    PlasmaDBus.DBusServiceWatcher {
        id: dockService
        busType: PlasmaDBus.BusType.Session
        watchedService: "org.archdock.ArchDock"
        onRegisteredChanged: {
            root.bootstrapFreeDock();
            root.refresh();
            root.publishGuards();
            root.publishPresentationState();
            root.consumePresentationRequest();
        }
    }

    PlasmaDBus.Properties {
        busType: PlasmaDBus.BusType.Session
        service: "org.archdock.ArchDock"
        path: "/Control"
        iface: "local.PanelWindow"
        onPropertiesChanged: function(interfaceName, changedProperties) {
            if (changedProperties.dockRevision !== undefined)
                root.refresh();
            else if (changedProperties.dockEntriesRevision !== undefined
                     && (!root.freeSurface
                         || FreeEntryPolicy.followsTaskModel(root.configuration.type)))
                root.refreshEntries();
            if (changedProperties.contentRevision !== undefined && root.contentUpdatesVisible)
                root.refreshEntries();
            if (changedProperties.nativeVisibilityRevision !== undefined)
                root.refreshNativeHostState();
            if (changedProperties.presentationRequestRevision !== undefined)
                root.consumePresentationRequest();
        }
        onRefreshed: root.refresh()
    }

    Connections {
        target: Plasmoid.configuration

        function onBootstrapFreeDockChanged() {
            root.bootstrapFreeDock();
        }

        // A free host learns its panel id only after bootstrap, so everything
        // that was skipped for an empty id is published now: an unchanged
        // resting state would otherwise never be reported at all.
        function onPanelIdChanged() {
            root.refresh();
            root.publishGuards();
            root.publishPresentationState();
            root.consumePresentationRequest();
        }
    }

    // Takes this panel off the desktop, as Plasma's Delete Panel does: a bar,
    // an edge panel or a free panel, with what it holds (a built-in panel is
    // hidden). A profile that holds the panel keeps its copy.
    PlasmaCore.Action {
        id: removePanelAction
        text: qsTr("Remove Panel")
        icon.name: "delete"
        visible: root.panelId.length > 0
        onTriggered: root.callDock("removePanel", [root.panelId])
    }

    // New panels come from Panel Studio's own page for them, not from a
    // panel that is already there.
    Plasmoid.contextualActions: [moveIntoBarAction, removePanelAction]

    Component.onCompleted: {
        Plasmoid.setInternalAction("configure", configurePanelStudioAction);
        root.bootstrapFreeDock();
        root.refresh();
        root.publishPresentationState();
        root.consumePresentationRequest();
        root.holdPlasmaPanelBackground();
    }
    Component.onDestruction: root.releasePlasmaPanelBackground()
}
