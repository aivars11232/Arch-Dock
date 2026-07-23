import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.plasma.workspace.dbus as PlasmaDBus

KCM.SimpleKCM {
    id: root

    readonly property string panelId: plasmoid.configuration.panelId || ""

    function callDock(methodName, parameters) {
        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: methodName
        });
        message.iface = "local.PanelWindow";
        if (parameters !== undefined)
            message.arguments = parameters;
        PlasmaDBus.SessionBus.asyncCall(message);
    }

    Kirigami.FormLayout {
        QQC2.Label {
            Kirigami.FormData.label: qsTr("Arch Dock panel")
            text: root.panelId.length > 0 ? root.panelId : qsTr("Unlinked control")
        }

        QQC2.Button {
            text: qsTr("Open Panel Editor")
            icon.name: "settings-configure"
            onClicked: {
                if (root.panelId.length > 0)
                    root.callDock("showPanelSettings", [root.panelId]);
                else
                    root.callDock("showSettings");
            }
        }

        QQC2.Button {
            text: qsTr("Enter Plasma Edit Mode")
            icon.name: "document-edit"
            onClicked: {
                const containment = plasmoid.containment;
                if (containment && containment.corona)
                    containment.corona.editMode = true;
            }
        }
    }
}