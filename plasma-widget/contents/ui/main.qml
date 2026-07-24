import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PlasmaComponents
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as PlasmaDBus

PlasmoidItem {
    id: root

    Plasmoid.title: qsTr("Arch Dock Control")
    Plasmoid.icon: "preferences-desktop-theme-global"
    preferredRepresentation: compactRepresentation
    switchWidth: Kirigami.Units.gridUnit * 20
    switchHeight: Kirigami.Units.gridUnit * 6

    readonly property string panelId: Plasmoid.configuration.panelId || ""
    readonly property string bootstrapAction: Plasmoid.configuration.bootstrapAction || ""
    readonly property string bootstrapToken: Plasmoid.configuration.bootstrapToken || ""
    readonly property int bootstrapPanelId: Plasmoid.configuration.bootstrapPanelId
    property bool bootstrapRequested: false

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

    function openPanelSettings() {
        if (panelId.length > 0)
            callDock("showPanelSettings", [panelId]);
        else
            callDock("showSettings");
    }

    function createPanel(edge, type) {
        callDock("createNativePanel", [edge, type]);
    }

    function tryBootstrap() {
        if (bootstrapRequested || !dockService.registered ||
            bootstrapAction !== "create-circular-free-panel" ||
            bootstrapPanelId < 0 || bootstrapToken.length === 0)
            return;
        bootstrapRequested = true;
        callDock("createFreePanelFromTemplate", [bootstrapPanelId, bootstrapToken],
            function(result) {
                if (!result || result.length === 0)
                    bootstrapRequested = false;
            },
            function() {
                bootstrapRequested = false;
            });
    }

    Component.onCompleted: Qt.callLater(tryBootstrap)

    PlasmaDBus.DBusServiceWatcher {
        id: dockService

        busType: PlasmaDBus.BusType.Session
        watchedService: "org.archdock.ArchDock"
        onRegisteredChanged: root.tryBootstrap()
    }

    compactRepresentation: MouseArea {
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: implicitWidth
        hoverEnabled: true
        enabled: dockService.registered
        onClicked: root.openPanelSettings()

        Kirigami.Icon {
            anchors.centerIn: parent
            width: Kirigami.Units.iconSizes.medium
            height: width
            source: "preferences-desktop-theme-global"
            opacity: parent.enabled ? 1 : 0.42
        }

        QQC2.ToolTip.visible: parent.containsMouse
        QQC2.ToolTip.text: dockService.registered
            ? qsTr("Configure Arch Dock")
            : qsTr("Start Arch Dock to configure it")
    }

    fullRepresentation: ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents.Button {
            Layout.fillWidth: true
            text: panelId.length > 0 ? qsTr("Configure Panel") : qsTr("Configure")
            icon.name: "settings-configure"
            enabled: dockService.registered
            onClicked: root.openPanelSettings()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.ToolButton {
                icon.name: "view-hidden"
                enabled: dockService.registered
                onClicked: root.callDock("toggleAutoHide")

                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: qsTr("Toggle dock auto-hide")
            }

            PlasmaComponents.ToolButton {
                icon.name: "list-add"
                enabled: dockService.registered
                onClicked: addPanelMenu.open()

                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: qsTr("Add an Arch Dock panel")

                QQC2.Menu {
                    id: addPanelMenu

                    Repeater {
                        model: [
                            { label: qsTr("Empty panel"), type: "empty" },
                            { label: qsTr("Launcher panel"), type: "launcher" },
                            { label: qsTr("Tasks panel"), type: "tasks" },
                            { label: qsTr("Hybrid panel"), type: "hybrid" }
                        ]

                        delegate: QQC2.Menu {
                            id: typeMenu

                            property string dockType: modelData.type

                            title: modelData.label

                            Repeater {
                                model: ["top", "bottom", "left", "right"]

                                delegate: QQC2.MenuItem {
                                    property string edge: modelData

                                    text: edge.charAt(0).toUpperCase() + edge.slice(1)
                                    onTriggered: root.createPanel(edge, typeMenu.dockType)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
