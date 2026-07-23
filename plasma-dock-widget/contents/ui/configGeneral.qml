import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as PlasmaDBus

KCM.SimpleKCM {
    id: root

    property string cfg_panelType: "hybrid"

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

    Kirigami.FormLayout {
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