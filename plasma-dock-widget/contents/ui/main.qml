import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as PlasmaDBus

PlasmoidItem {
    id: root

    readonly property string panelId: Plasmoid.configuration.panelId || ""
    readonly property string configuredPanelType: Plasmoid.configuration.panelType || "hybrid"
    readonly property string panelType: ["launcher", "tasks", "hybrid"].includes(configuredPanelType)
        ? configuredPanelType : "hybrid"
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

    Plasmoid.title: qsTr("Arch Dock")
    Plasmoid.icon: "applications-system"
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
        PlasmaDBus.SessionBus.asyncCall(
            message,
            onResolved || function() {},
            onRejected || function() {});
    }

    function normalizeReply(reply) {
        if (Array.isArray(reply) && reply.length === 1)
            return reply[0];
        return reply;
    }

    function synchronizePlasmaEditMode() {
        if (dockService.registered)
            callDock("setPlasmaEditMode", [plasmaEditMode]);
    }

    onPlasmaEditModeChanged: synchronizePlasmaEditMode()

    function refresh() {
        if (!dockService.registered) {
            entries = [];
            requestFailed = false;
            return;
        }
        callDock("dockConfiguration", [panelId], function(reply) {
            const value = normalizeReply(reply);
            if (value && typeof value === "object")
                configuration = value;
        });
        callDock("dockEntries", [panelType], function(reply) {
            const value = normalizeReply(reply);
            entries = Array.isArray(value) ? value : [];
            requestFailed = false;
        }, function() {
            entries = [];
            requestFailed = true;
        });
    }

    function invokeEntry(methodName, appId) {
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
            callDock("pinDockUrls", [values], refresh);
    }

    function openPanelStudio() {
        callDock(panelId.length > 0 ? "showPanelSettings" : "showSettings",
                 panelId.length > 0 ? [panelId] : []);
    }

    compactRepresentation: Kirigami.Icon {
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: implicitWidth
        source: "applications-system"
    }

    fullRepresentation: Item {
        readonly property real magnifiedCell: root.baseCellSize
            * (root.configuration.magnificationEnabled ? Math.max(1, root.magnification) : 1)
        implicitWidth: root.vertical
            ? magnifiedCell + Kirigami.Units.largeSpacing * 2
            : Math.max(root.baseCellSize + Kirigami.Units.largeSpacing * 2,
                       root.entries.length * root.baseCellSize
                           + Math.max(0, root.entries.length - 1) * root.spacing
                           + (magnifiedCell - root.baseCellSize) * 2
                           + Kirigami.Units.largeSpacing * 2)
        implicitHeight: root.vertical
            ? Math.max(root.baseCellSize + Kirigami.Units.largeSpacing * 2,
                       root.entries.length * root.baseCellSize
                           + Math.max(0, root.entries.length - 1) * root.spacing
                           + (magnifiedCell - root.baseCellSize) * 2
                           + Kirigami.Units.largeSpacing * 2)
            : magnifiedCell + Kirigami.Units.largeSpacing * 2
        Layout.minimumWidth: implicitWidth
        Layout.minimumHeight: implicitHeight
        opacity: root.panelOpacity

        Loader {
            anchors.centerIn: parent
            active: root.entries.length > 0
            sourceComponent: root.vertical ? verticalEntries : horizontalEntries
        }

        Component {
            id: horizontalEntries
            Row {
                spacing: root.spacing
                Repeater {
                    model: root.entries
                    delegate: DockEntry {
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

    PlasmaDBus.DBusServiceWatcher {
        id: dockService
        busType: PlasmaDBus.BusType.Session
        watchedService: "org.archdock.ArchDock"
        onRegisteredChanged: {
            root.refresh();
            root.synchronizePlasmaEditMode();
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
        }
        onRefreshed: root.refresh()
    }

    Component.onCompleted: {
        root.refresh();
        Qt.callLater(root.synchronizePlasmaEditMode);
    }
}
