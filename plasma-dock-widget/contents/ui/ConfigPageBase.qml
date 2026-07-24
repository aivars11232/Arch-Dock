import QtQuick
import org.kde.kcmutils as KCM
import org.kde.plasma.workspace.dbus as PlasmaDBus

KCM.SimpleKCM {
    id: root

    property var values: ({})
    readonly property string panelId: plasmoid.configuration.panelId || ""

    function callDock(member, arguments, resolved) {
        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: member
        });
        message.iface = "local.PanelWindow";
        message.arguments = arguments || [];
        PlasmaDBus.SessionBus.asyncCall(message, resolved || function() {});
    }

    function refresh() {
        if (panelId.length === 0)
            return;
        callDock("dockConfiguration", [panelId], function(reply) {
            const value = Array.isArray(reply) && reply.length === 1 ? reply[0] : reply;
            if (value && typeof value === "object")
                root.values = value;
        });
    }

    function setValue(key, value) {
        const next = Object.assign({}, root.values);
        next[key] = value;
        root.values = next;
        let member = "setDockStringConfiguration";
        if (typeof value === "boolean")
            member = "setDockBooleanConfiguration";
        else if (typeof value === "number")
            member = Number.isInteger(value)
                ? "setDockIntegerConfiguration" : "setDockRealConfiguration";
        callDock(member, [panelId, key, value], root.refresh);
    }

    Component.onCompleted: refresh()
}
