import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as PlasmaDBus
import "DockGeometry.js" as DockGeometry

PlasmoidItem {
    id: root

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

    property var entries: []
    property var configuration: ({
        iconSize: 52,
        spacing: 8,
        opacity: 0.9,
        iconShape: "rounded",
        appearance: "glass",
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
        reducedMotion: false
    })
    property int hoveredIndex: -1
    property bool requestFailed: false
    property bool bootstrapRequested: false

    Plasmoid.title: qsTr("Arch Dock")
    Plasmoid.icon: "applications-system"
    Plasmoid.backgroundHints: root.freeSurface
        ? PlasmaCore.Types.NoBackground : PlasmaCore.Types.StandardBackground
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
            if (onResolved) {
                const value = JSON.parse(JSON.stringify(reply.value));
                onResolved(value);
            }
            reply.destroy();
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
        callDock("dockConfiguration", [panelId], function(reply) {
            const value = normalizeReply(reply);
            if (value && typeof value === "object")
                configuration = value;
        });
    }

    function refreshEntries() {
        if (panelId.length === 0 || !dockService.registered) {
            entries = [];
            requestFailed = false;
            return;
        }
        callDock("dockEntriesForPanel", [panelId, panelType], function(reply) {
            const value = normalizeReply(reply);
            entries = Array.isArray(value) ? value : [];
            requestFailed = false;
        }, function() {
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
        bootstrapRequested = true;
        callDock("createFreePanel", [], function(reply) {
            const value = normalizeReply(reply);
            if (typeof value === "string" && value.length > 0) {
                Plasmoid.configuration.panelId = value;
                Plasmoid.configuration.bootstrapFreeDock = false;
                refresh();
            } else {
                bootstrapRequested = false;
            }
        }, function() {
            bootstrapRequested = false;
        });
    }

    function invokeEntry(methodName, appId) {
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

    Component {
        id: dockRepresentation

        Item {
            id: representation

        readonly property string freeLayout: root.configuration.layout || "circular"
        readonly property var freeGeometry: DockGeometry.metrics(
            root.entries.length, root.iconSize, root.spacing,
            Number(root.configuration.layoutScale || 1),
            Number(root.configuration.layoutRadius || 150),
            Number(root.configuration.layoutPadding || 18))
        readonly property real magnifiedCell: root.baseCellSize
            * (root.configuration.magnificationEnabled ? Math.max(1, root.magnification) : 1)
        implicitWidth: root.freeSurface ? freeGeometry.width : root.vertical
            ? magnifiedCell + Kirigami.Units.largeSpacing * 2
            : Math.max(root.baseCellSize + Kirigami.Units.largeSpacing * 2,
                       root.entries.length * root.baseCellSize
                           + Math.max(0, root.entries.length - 1) * root.spacing
                           + (magnifiedCell - root.baseCellSize) * 2
                           + Kirigami.Units.largeSpacing * 2)
        implicitHeight: root.freeSurface ? freeGeometry.height : root.vertical
            ? Math.max(root.baseCellSize + Kirigami.Units.largeSpacing * 2,
                       root.entries.length * root.baseCellSize
                           + Math.max(0, root.entries.length - 1) * root.spacing
                           + (magnifiedCell - root.baseCellSize) * 2
                           + Kirigami.Units.largeSpacing * 2)
            : magnifiedCell + Kirigami.Units.largeSpacing * 2
        Layout.minimumWidth: implicitWidth
        Layout.minimumHeight: implicitHeight
        Loader {
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            active: root.freeSurface || root.entries.length > 0
            sourceComponent: root.freeSurface
                ? freeEntries : root.vertical ? verticalEntries : horizontalEntries
        }

        Component {
            id: freeEntries

            Item {
                width: parent ? parent.width : 0
                height: parent ? parent.height : 0

                Canvas {
                    id: freeSurfaceCanvas

                    anchors.fill: parent
                    opacity: root.panelOpacity
                    onPaint: {
                        const context = getContext("2d");
                        context.reset();
                        const cx = width / 2;
                        const cy = height / 2;
                        const radius = Math.min(
                            Number(root.configuration.layoutRadius || 150)
                                * Number(root.configuration.layoutScale || 1),
                            Math.min(width, height) / 2 - root.iconSize / 2);
                        const layout = representation.freeLayout;
                        const appearance = root.configuration.appearance || "glass";
                        context.lineWidth = Math.max(18, root.iconSize * 0.48);
                        context.strokeStyle = appearance === "neon" ? "#50e6ff"
                            : appearance === "futuristic" ? "#b030d8"
                            : appearance === "metallic" ? "#aeb9c4"
                            : appearance === "organic" ? "#57c98b"
                            : appearance === "platform" ? "#6e7884"
                            : appearance === "crystal" ? "#9eeeff"
                            : appearance === "lime" ? "#7dff58"
                            : appearance === "minimal" ? "#4c5966"
                            : "#334862";
                        context.shadowColor = appearance === "neon"
                            || appearance === "futuristic" ? "#35cfff"
                            : appearance === "lime" ? "#7dff58"
                            : "#000000";
                        context.shadowBlur = appearance === "minimal" ? 0
                            : appearance === "futuristic" ? 28 : 18;
                        context.beginPath();
                        if (layout === "ellipse") {
                            context.ellipse(cx, cy, radius, radius * 0.62, 0, 0, Math.PI * 2);
                        } else {
                            context.arc(cx, cy, radius, 0, Math.PI * 2);
                        }
                        context.stroke();
                    }

                    Connections {
                        target: root
                        function onConfigurationChanged() {
                            freeSurfaceCanvas.requestPaint();
                        }
                    }
                }

                Repeater {
                    model: root.entries

                    delegate: DockEntry {
                        required property var modelData
                        required property int index
                        readonly property var point: DockGeometry.position(
                            representation.freeLayout, index, root.entries.length,
                            representation.freeGeometry,
                            Number(root.configuration.layoutAngle || 0),
                            Number(root.configuration.pathSides || 6))

                        x: (parent.width - representation.freeGeometry.width) / 2 + point.x
                        y: (parent.height - representation.freeGeometry.height) / 2 + point.y
                        entry: modelData
                        entryIndex: index
                        vertical: false
                        baseSize: root.iconSize
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
                        reducedMotion: root.configuration.reducedMotion
                        inputEnabled: dockService.registered && !root.plasmaEditMode
                        acceptDrops: root.configuration.acceptDrops
                        invoke: root.invokeEntry
                        reorder: root.reorderEntry
                        pinUrls: root.pinDroppedUrls
                        setHoveredIndex: function(value) { root.hoveredIndex = value }
                        openPanelStudio: root.openPanelStudio
                    }
                }
            }
        }

        Component {
            id: horizontalEntries
            Row {
                spacing: root.spacing
                Repeater {
                    model: root.entries
                    delegate: DockEntry {
                        required property var modelData
                        required property int index

                        entry: modelData
                        entryIndex: index
                        vertical: false
                        baseSize: root.baseCellSize
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
                        reducedMotion: root.configuration.reducedMotion
                        inputEnabled: dockService.registered && !root.plasmaEditMode
                        acceptDrops: root.configuration.acceptDrops
                        invoke: root.invokeEntry
                        reorder: root.reorderEntry
                        pinUrls: root.pinDroppedUrls
                        setHoveredIndex: function(value) { root.hoveredIndex = value }
                        openPanelStudio: root.openPanelStudio
                    }
                }
            }
        }

        Component {
            id: verticalEntries
            Column {
                spacing: root.spacing
                Repeater {
                    model: root.entries
                    delegate: DockEntry {
                        required property var modelData
                        required property int index

                        entry: modelData
                        entryIndex: index
                        vertical: true
                        baseSize: root.baseCellSize
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
                        reducedMotion: root.configuration.reducedMotion
                        inputEnabled: dockService.registered && !root.plasmaEditMode
                        acceptDrops: root.configuration.acceptDrops
                        invoke: root.invokeEntry
                        reorder: root.reorderEntry
                        pinUrls: root.pinDroppedUrls
                        setHoveredIndex: function(value) { root.hoveredIndex = value }
                        openPanelStudio: root.openPanelStudio
                    }
                }
            }
        }

        Column {
            anchors.centerIn: parent
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
                text: !dockService.registered ? qsTr("Arch Dock service is unavailable")
                    : root.requestFailed ? qsTr("Could not load dock entries")
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
            z: -1
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
        root.bootstrapFreeDock();
        root.refresh();
    }
}
