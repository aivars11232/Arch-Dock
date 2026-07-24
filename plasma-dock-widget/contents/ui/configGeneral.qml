import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as PlasmaDBus

KCM.SimpleKCM {
    id: root

    property string cfg_panelId: ""
    property string cfg_panelIdDefault: ""
    property string cfg_panelType: "hybrid"
    property string cfg_panelTypeDefault: "hybrid"
    property bool cfg_bootstrapFreeDock: false
    property bool cfg_bootstrapFreeDockDefault: false

    function typeIndex(panelType) {
        return ["launcher", "tasks", "hybrid"].indexOf(panelType)
    }

    function synchronizeManagedPanelType(panelType) {
        const panelId = plasmoid.configuration.panelId || "";
        if (panelId.length === 0)
            return;
        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: "setNativePanelType"
        });
        message.iface = "local.PanelWindow";
        message.arguments = [panelId, panelType];
        PlasmaDBus.SessionBus.asyncCall(message);
    }

    function openPanelStudio() {
        const panelId = plasmoid.configuration.panelId || "";
        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: panelId.length > 0 ? "showPanelSettings" : "showSettings"
        });
        message.iface = "local.PanelWindow";
        message.arguments = panelId.length > 0 ? [panelId] : [];
        PlasmaDBus.SessionBus.asyncCall(message);
    }

    Kirigami.FormLayout {
        QQC2.Button {
            Kirigami.FormData.label: qsTr("Arch Dock")
            text: qsTr("Open Panel Studio")
            icon.name: "configure"
            onClicked: root.openPanelStudio()
        }

        QQC2.Label {
            Kirigami.FormData.isSection: true
            text: qsTr("Native panel integration")
        }

        QQC2.ComboBox {
            id: panelType

            Kirigami.FormData.label: qsTr("Panel type")
            model: [
                { label: qsTr("Launcher"), value: "launcher" },
                { label: qsTr("Tasks"), value: "tasks" },
                { label: qsTr("Hybrid"), value: "hybrid" }
            ]
            textRole: "label"
            valueRole: "value"
            currentIndex: Math.max(0, root.typeIndex(root.cfg_panelType))
            onActivated: {
                root.cfg_panelType = currentValue;
                root.synchronizeManagedPanelType(currentValue);
            }
        }
    }
}
