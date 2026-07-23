import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
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
    readonly property int cellSize: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
    readonly property bool plasmaEditMode: {
        const containment = Plasmoid.containment;
        return containment && containment.corona ? containment.corona.editMode : false;
    }
    property var entries: []

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

    function normalizeEntries(reply) {
        if (!Array.isArray(reply))
            return [];
        if (reply.length === 1 && Array.isArray(reply[0]))
            return reply[0];
        return reply;
    }

    function refreshEntries() {
        if (!dockService.registered) {
            entries = [];
            return;
        }
        callDock("dockEntries", [panelType], function(reply) {
            entries = normalizeEntries(reply);
        }, function() {
            entries = [];
        });
    }

    function invokeEntry(methodName, appId) {
        callDock(methodName, [appId], refreshEntries);
    }

    compactRepresentation: Kirigami.Icon {
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: implicitWidth
        source: "applications-system"
    }

    fullRepresentation: Item {
        implicitWidth: root.vertical
            ? root.cellSize + Kirigami.Units.largeSpacing * 2
            : Math.max(root.cellSize + Kirigami.Units.largeSpacing * 2,
                       root.entries.length * root.cellSize
                           + Math.max(0, root.entries.length - 1) * Kirigami.Units.smallSpacing
                           + Kirigami.Units.largeSpacing * 2)
        implicitHeight: root.vertical
            ? Math.max(root.cellSize + Kirigami.Units.largeSpacing * 2,
                       root.entries.length * root.cellSize
                           + Math.max(0, root.entries.length - 1) * Kirigami.Units.smallSpacing
                           + Kirigami.Units.largeSpacing * 2)
            : root.cellSize + Kirigami.Units.largeSpacing * 2
        Layout.minimumWidth: implicitWidth
        Layout.minimumHeight: implicitHeight

        component DockEntry: Item {
            required property var entry

            width: root.cellSize
            height: root.cellSize

            Kirigami.Icon {
                anchors.centerIn: parent
                width: Kirigami.Units.iconSizes.medium
                height: width
                source: entry.iconName || "application-x-executable"
                opacity: entry.minimized ? 0.52 : 1
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2
                width: entry.active ? Kirigami.Units.gridUnit : Kirigami.Units.smallSpacing
                height: Kirigami.Units.smallSpacing
                radius: height / 2
                color: Kirigami.Theme.highlightColor
                visible: entry.running
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                hoverEnabled: true
                enabled: dockService.registered && !root.plasmaEditMode
                onClicked: mouse => {
                    if (mouse.button === Qt.RightButton)
                        contextMenu.open();
                    else
                        root.invokeEntry("activateDockEntry", entry.appId);
                }
            }

            QQC2.ToolTip.visible: mouseArea.containsMouse
            QQC2.ToolTip.text: entry.windowCount > 1
                ? qsTr("%1 (%2 windows)").arg(entry.displayName).arg(entry.windowCount)
                : entry.displayName

            QQC2.Menu {
                id: contextMenu

                QQC2.MenuItem {
                    text: entry.pinned ? qsTr("Unpin") : qsTr("Pin")
                    onTriggered: root.invokeEntry("togglePinnedDockEntry", entry.appId)
                }

                QQC2.MenuItem {
                    text: entry.minimized ? qsTr("Restore") : qsTr("Minimize")
                    visible: entry.running
                    onTriggered: root.invokeEntry("minimizeDockEntry", entry.appId)
                }

                QQC2.MenuItem {
                    text: qsTr("Close")
                    visible: entry.running
                    onTriggered: root.invokeEntry("closeDockEntry", entry.appId)
                }
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing
            visible: !root.vertical

            Repeater {
                model: root.entries
                delegate: DockEntry {
                    entry: modelData
                }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: Kirigami.Units.smallSpacing
            visible: root.vertical

            Repeater {
                model: root.entries
                delegate: DockEntry {
                    entry: modelData
                }
            }
        }
    }

    PlasmaDBus.DBusServiceWatcher {
        id: dockService

        busType: PlasmaDBus.BusType.Session
        watchedService: "org.archdock.ArchDock"

        onRegisteredChanged: root.refreshEntries()
    }

    PlasmaDBus.Properties {
        busType: PlasmaDBus.BusType.Session
        service: "org.archdock.ArchDock"
        path: "/Control"
        iface: "local.PanelWindow"

        onPropertiesChanged: function(interfaceName, changedProperties) {
            if (changedProperties.dockRevision !== undefined)
                root.refreshEntries();
        }
        onRefreshed: root.refreshEntries()
    }

    Timer {
        interval: 5000
        repeat: true
        running: dockService.registered
        onTriggered: root.refreshEntries()
    }

    Component.onCompleted: root.refreshEntries()
}