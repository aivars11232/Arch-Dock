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
            text: qsTr("Layout, themes, animations, behavior, and panel content are managed in Panel Studio. KDE Edit Mode continues to manage the widget's desktop position and size.")
            wrapMode: Text.WordWrap
        }
    }
}
