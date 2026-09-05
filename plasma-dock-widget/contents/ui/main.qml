import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import ArchDock.Rendering 1.0
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as PlasmaDBus
import "FreeEntryPolicy.js" as FreeEntryPolicy

PlasmoidItem {
    id: root

    readonly property bool renderingModuleReady: RenderingModuleProbe.ready
    readonly property string panelId: Plasmoid.configuration.panelId || ""
    readonly property string configuredPanelType: Plasmoid.configuration.panelType || "hybrid"
    readonly property string panelType: ["empty", "launcher", "tasks", "hybrid"].includes(configuredPanelType)
        ? configuredPanelType : "hybrid"
    readonly property bool bootstrapPending: panelId.length === 0
        && Boolean(Plasmoid.configuration.bootstrapFreeDock)
    readonly property bool freeSurface: panelId.startsWith("free-") || bootstrapPending
    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    readonly property bool plasmaEditMode: {
        const containment = Plasmoid.containment;
        return containment && containment.corona ? containment.corona.editMode : false;
    }
    readonly property real iconSize: Number(configuration.iconSize || 52)
    readonly property real spacing: Number(configuration.spacing || 8)
    readonly property real baseCellSize: iconSize + Math.max(4, spacing)
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
        : dockService.registered && !plasmaEditMode
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
    // The runtime state the scene draws from. Until TASK-0032 Phase D the
    // presentation controller existed but nothing read it, so the live surface
    // was permanently "open" however the panel was configured. These three
    // fields are the wiring: one state machine, one surface.
    readonly property var sceneRuntimeState: ({
        hovered: hoveredIndex >= 0,
        hoveredEntry: hoveredIndex,
        editMode: plasmaEditMode,
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
        magnificationRadius: 2.4,
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
    Plasmoid.icon: "applications-system"
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    preferredRepresentation: fullRepresentation
    switchWidth: Kirigami.Units.gridUnit * 24
    switchHeight: Kirigami.Units.gridUnit * 4

    function callDock(methodName, parameters, onResolved, onRejected) {
        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: methodName
        });
        message.iface = "local.PanelWindow";
        message.arguments = parameters || [];
        const reply = PlasmaDBus.SessionBus.asyncCall(message)
            as PlasmaDBus.DBusPendingReply;
        reply.finished.connect(function() {
            try {
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
        });
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
        const definition = {};
        const source = configuration || {};
        for (const key of Object.keys(source)) {
            if (key !== "capabilityResolution")
                definition[key] = source[key];
        }
        definition.edge = freeSurface ? "free" : vertical ? "left" : "bottom";
        definition.rendererTier = String(
            source.rendererTier || effectiveRendererTier || "procedural2d");
        if (freeSurface) {
            definition.layout = source.layout || "circular";
        } else {
            definition.layout = vertical ? "vertical" : "horizontal";
            definition.layoutScale = 1;
            definition.layoutAngle = 0;
            definition.layoutPadding = nativeScenePadding;
            definition.iconSize = baseCellSize;
        }
        return definition;
    }

    function refreshConfiguration() {
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
            if (value && typeof value === "object")
                configuration = value;
        });
    }

    function refreshEntries() {
        if (panelId.length === 0) {
            entries = [];
            requestFailed = false;
            return;
        }
        if (!dockService.registered) {
            if (!freeSurface)
                entries = [];
            requestFailed = false;
            return;
        }
        callDock("dockEntriesForPanel", [panelId, panelType], function(reply) {
            const value = normalizeReply(reply);
            entries = Array.isArray(value) ? value : [];
            requestFailed = false;
        }, function() {
            if (!FreeEntryPolicy.keepEntriesOnServiceFailure(root.freeSurface))
                entries = [];
            requestFailed = true;
        });
    }

    function refresh() {
        refreshConfiguration();
        refreshEntries();
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
        if (methodName === "activateDockEntry") {
            activateEntry(appId, onOutcome);
            return;
        }
        if (freeSurface && methodName === "togglePinnedDockEntry") {
            callDock("removePanelContent", [panelId, appId], refresh);
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
        if (freeSurface) {
            const targetUrl = FreeEntryPolicy.encodedUrl(appId);
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
        callDock("activateDockEntryOutcome", [appId], function(reply) {
            const value = normalizeReply(reply);
            report(value && value.outcome ? String(value.outcome) : "requested",
                   value && value.reason ? String(value.reason) : "");
            refresh();
        }, function(error) {
            report("failed", error && error.name
                   ? String(error.name) : "call-failed");
        });
    }

    function reorderEntry(appId, beforeAppId) {
        if (appId === beforeAppId)
            return;
        callDock("moveDockEntryBefore", [appId, beforeAppId], refresh);
    }

    function pinDroppedUrls(urls) {
        const values = [];
        for (const url of urls)
            values.push(url.toString());
        if (values.length > 0)
            callDock(root.freeSurface ? "pinPanelUrls" : "pinDockUrls",
                     root.freeSurface ? [panelId, values] : [values], refresh);
    }

    function openPanelStudio() {
        callDock(panelId.length > 0 ? "showPanelSettings" : "showSettings",
                 panelId.length > 0 ? [panelId] : []);
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
        // Grouped window previews are owned by TASK-0037. The guard exists and
        // is fed with a truthful `false` rather than being silently omitted.
        windowPreviewOpen: false
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
        popupOpen: presentationController.popupOpen,
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

    // Opening and closing requests.
    //
    // The pointer entering the panel opens it; the pointer leaving asks it to
    // close, and the controller holds that request until every guard clears.
    // A collapsed panel with no configured trigger stays collapsed until
    // something asks it to open, which is what "manual" means.
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
        if (!root.opensOnHover)
            return;
        if (root.panelPointerInside)
            root.requestPresentationOpen();
        else
            root.requestPresentationCollapse();
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

    Component {
        id: dockRepresentation

        Item {
            id: representation

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

            implicitWidth: root.freeSurface
                ? sceneEffectWidth : panelScene.width
            implicitHeight: root.freeSurface
                ? sceneEffectHeight : panelScene.height
            Layout.minimumWidth: implicitWidth
            Layout.minimumHeight: implicitHeight

            // The panel is concealed when its own item is hidden or fully
            // transparent, or when the window holding it is not on screen -
            // which is what a Plasma auto-hide panel does. Nothing is inferred
            // from focus: a dock stays visible while another window is active.
            //
            // This is reported to the presentation controller rather than used
            // directly: host concealment and surface collapse are separate
            // layers, and only the controller may combine them.
            readonly property bool hostConcealed: !visible || opacity <= 0
                || (Window.window !== null
                    && (!Window.window.visible
                        || Window.visibility === Window.Hidden
                        || Window.visibility === Window.Minimized))

            onHostConcealedChanged: root.hostConcealed = hostConcealed
            Component.onCompleted: root.hostConcealed = hostConcealed

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
            Item {
                id: revealZone

                x: Number(panelScene.revealHandle.x || 0)
                y: Number(panelScene.revealHandle.y || 0)
                width: Number(panelScene.revealHandle.width || 0)
                height: Number(panelScene.revealHandle.height || 0)
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

            onActiveFocusChanged: root.panelKeyboardFocus = activeFocus
            Component.onDestruction: {
                root.panelPointerInside = false;
                root.panelKeyboardFocus = false;
                root.panelDropActive = false;
            }

            PanelScene {
                id: panelScene

                anchors.centerIn: parent
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
                entryInteractionEnabled: root.sceneInputEnabled
                sceneConcealed: !presentationController.hostVisible
                geometryCompatibilityProfile: root.freeSurface
                    ? "live" : "canonical"
                entryDelegateContext: ({
                    hostKind: root.freeSurface ? "free" : "native",
                    vertical: root.vertical
                })
            }

            Column {
                anchors.centerIn: parent
                z: 20
                spacing: Kirigami.Units.smallSpacing
                visible: root.entries.length === 0

                Kirigami.Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Kirigami.Units.iconSizes.medium
                    height: width
                    source: dockService.registered
                        ? (root.requestFailed ? "data-error" : "list-add")
                        : "network-disconnect"
                }
                QQC2.Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: !dockService.registered
                        ? qsTr("Arch Dock service is unavailable")
                        : root.requestFailed
                            ? qsTr("Could not load dock entries")
                            : qsTr("Drop applications here")
                }
                QQC2.Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Retry")
                    visible: !dockService.registered || root.requestFailed
                    onClicked: root.refresh()
                }
            }

            DropArea {
                id: panelDropArea

                anchors.fill: parent
                enabled: root.configuration.acceptDrops && !root.plasmaEditMode
                keys: ["text/uri-list"]
                onContainsDragChanged: root.panelDropActive = containsDrag
                onDropped: drop => {
                    if (drop.hasUrls)
                        root.pinDroppedUrls(drop.urls);
                    drop.acceptProposedAction();
                    root.panelDropActive = false;
                }
            }
        }
    }

    Component {
        id: liveEntryDelegate

        DockEntry {
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
                root.configuration.magnificationRadius || 2.4)
            magnificationFalloff: String(
                root.configuration.magnificationFalloff || "linear")
            hoveredIndex: root.hoveredIndex
            tileShape: root.configuration.iconShape
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
            sceneVisible: parent.sceneVisible
            editMode: root.plasmaEditMode
            acceptDrops: root.configuration.acceptDrops
            invoke: root.invokeEntry
            reorder: root.reorderEntry
            pinUrls: root.pinDroppedUrls
            setHoveredIndex: function(value) { root.hoveredIndex = value }
            setEntryGuard: root.setEntryGuard
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
            else if (changedProperties.dockEntriesRevision !== undefined && !root.freeSurface)
                root.refreshEntries();
        }
        onRefreshed: root.refresh()
    }

    Connections {
        target: Plasmoid.configuration

        function onBootstrapFreeDockChanged() {
            root.bootstrapFreeDock();
        }

        function onPanelIdChanged() {
            root.refresh();
        }
    }

    Component.onCompleted: {
        Plasmoid.setInternalAction("configure", configurePanelStudioAction);
        root.bootstrapFreeDock();
        root.refresh();
    }
}
