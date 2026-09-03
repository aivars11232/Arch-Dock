import QtQuick
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
    readonly property real nativeScenePadding: Kirigami.Units.largeSpacing
        + (configuration.magnificationEnabled
            ? baseCellSize * Math.max(0, magnification - 1) : 0)
    readonly property var scenePanelDefinition: buildScenePanelDefinition()
    readonly property var sceneRuntimeState: ({
        hovered: hoveredIndex >= 0,
        hoveredEntry: hoveredIndex,
        editMode: plasmaEditMode,
        rendererFallback: ""
    })
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

    function invokeEntry(methodName, appId) {
        if (freeSurface && methodName === "activateDockEntry") {
            const targetUrl = FreeEntryPolicy.encodedUrl(appId);
            if (targetUrl.length > 0) {
                if (!Qt.openUrlExternally(targetUrl) && dockService.registered)
                    callDock(methodName, [appId]);
                return;
            }
        }
        if (freeSurface && methodName === "togglePinnedDockEntry") {
            callDock("removePanelContent", [panelId, appId], refresh);
            return;
        }
        callDock(methodName, [appId], refresh);
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

            implicitWidth: panelScene.width
            implicitHeight: panelScene.height
            Layout.minimumWidth: implicitWidth
            Layout.minimumHeight: implicitHeight

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
                anchors.fill: parent
                enabled: root.configuration.acceptDrops && !root.plasmaEditMode
                keys: ["text/uri-list"]
                onDropped: drop => {
                    if (drop.hasUrls)
                        root.pinDroppedUrls(drop.urls);
                    drop.acceptProposedAction();
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
            magnification: root.magnification
            magnificationEnabled: root.configuration.magnificationEnabled
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
            editMode: root.plasmaEditMode
            acceptDrops: root.configuration.acceptDrops
            invoke: root.invokeEntry
            reorder: root.reorderEntry
            pinUrls: root.pinDroppedUrls
            setHoveredIndex: function(value) { root.hoveredIndex = value }
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
