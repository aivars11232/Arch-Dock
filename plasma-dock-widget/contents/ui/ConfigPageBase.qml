import QtQuick
import org.kde.kcmutils as KCM
import org.kde.plasma.workspace.dbus as PlasmaDBus

KCM.SimpleKCM {
    id: root

    // Plasma injects every key from main.xml into each configuration page.
    // Declaring them keeps the Plasma 6 KCM loader from rejecting the page.
    property string cfg_panelId: ""
    property string cfg_panelIdDefault: ""
    property string cfg_panelType: "hybrid"
    property string cfg_panelTypeDefault: "hybrid"
    property bool cfg_bootstrapFreeDock: false
    property bool cfg_bootstrapFreeDockDefault: false
    property var values: ({})
    readonly property string panelId: plasmoid.configuration.panelId || ""

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

    function callDock(member, arguments, resolved) {
        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: member
        });
        message.iface = "local.PanelWindow";
        message.arguments = arguments || [];
        const reply = PlasmaDBus.SessionBus.asyncCall(message)
            as PlasmaDBus.DBusPendingReply;
        reply.finished.connect(function() {
            if (resolved) {
                const value = JSON.parse(JSON.stringify(reply.value));
                resolved(root.normalizeReply(value));
            }
            reply.destroy();
        });
    }

    function refresh() {
        if (panelId.length === 0)
            return;
        callDock("dockConfiguration", [panelId], function(reply) {
            if (reply && typeof reply === "object")
                root.values = reply;
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
