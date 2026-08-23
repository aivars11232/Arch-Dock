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
    property var placementStatus: ({})
    property string placementError: ""

    readonly property string panelId: String(cfg_panelId
        || plasmoid.configuration.panelId || "")

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

    function mapText(map) {
        if (!map || typeof map !== "object")
            return qsTr("Unavailable");
        const keys = Object.keys(map).sort();
        if (keys.length === 0)
            return qsTr("Unavailable");
        return keys.map(function(key) {
            return key + "=" + String(map[key]);
        }).join("; ");
    }

    function statusText(status) {
        switch (String(status || "")) {
        case "applied": return qsTr("Applied and verified");
        case "unsupported": return qsTr("Unsupported");
        case "rolled-back": return qsTr("Failed; host restored");
        case "rollback-failed": return qsTr("Failed; host restoration failed");
        case "failed": return qsTr("Failed before completion");
        default: return qsTr("No placement result");
        }
    }

    function diagnosticText(result) {
        if (!result || typeof result !== "object")
            return placementError;
        const details = [];
        if (String(result.errorCode || "").length > 0)
            details.push(String(result.errorCode));
        const groups = ["unsupported", "failed"];
        for (let groupIndex = 0; groupIndex < groups.length; ++groupIndex) {
            const group = groups[groupIndex];
            const entries = Array.isArray(result[group]) ? result[group] : [];
            for (let index = 0; index < entries.length; ++index) {
                const entry = entries[index];
                let detail = group + " " + String(entry.field || "unknown")
                    + ": requested=" + String(entry.requested || "")
                    + ", error=" + String(entry.errorCode || "unknown");
                if (entry.observed !== undefined)
                    detail += ", observed=" + String(entry.observed);
                details.push(detail);
            }
        }
        if (result.rollbackAttempted === true
                && result.rollbackSucceeded !== true) {
            details.push("rollback="
                + String(result.rollbackErrorCode || "unknown"));
        }
        return details.join(" | ");
    }

    function refreshPlacementStatus() {
        if (panelId.length === 0) {
            placementStatus = {};
            placementError = qsTr("This widget has no Arch Dock panel identity.");
            return;
        }
        if (!dockService.registered) {
            placementError = qsTr("Arch Dock service is unavailable.");
            return;
        }

        const message = new PlasmaDBus.dbusMessage({
            service: "org.archdock.ArchDock",
            path: "/Control",
            member: "nativePanelPlacementStatus"
        });
        message.iface = "local.PanelWindow";
        message.arguments = [panelId];
        const reply = PlasmaDBus.SessionBus.asyncCall(message)
            as PlasmaDBus.DBusPendingReply;
        reply.finished.connect(function() {
            try {
                if (reply.isError) {
                    placementError = reply.error.name + ": "
                        + reply.error.message;
                    return;
                }
                const normalized = normalizeReply(reply.value);
                placementStatus = Array.isArray(normalized)
                        && normalized.length === 1
                    ? normalized[0] : normalized;
                placementError = "";
            } finally {
                reply.destroy();
            }
        });
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
            text: qsTr("Layout, themes, animations, behavior, and panel content are managed in Panel Studio. KDE Edit Mode continues to manage the widget's desktop position and size.")
            wrapMode: Text.WordWrap
        }

        QQC2.Label {
            visible: root.panelId.length > 0
            Kirigami.FormData.label: qsTr("Native placement")
            text: root.statusText(root.placementStatus.status)
            wrapMode: Text.WordWrap
        }

        QQC2.Label {
            visible: root.panelId.length > 0
            Kirigami.FormData.label: qsTr("Saved intent")
            text: root.mapText(root.placementStatus.savedIntent)
            wrapMode: Text.WrapAnywhere
        }

        QQC2.Label {
            visible: root.panelId.length > 0
            Kirigami.FormData.label: qsTr("Actual Plasma host")
            text: root.mapText(root.placementStatus.hostState)
            wrapMode: Text.WrapAnywhere
        }

        QQC2.Label {
            visible: root.panelId.length > 0
                && (root.placementError.length > 0
                    || root.placementStatus.success !== true)
            Kirigami.FormData.label: qsTr("Placement detail")
            text: root.placementError.length > 0
                ? root.placementError
                : root.diagnosticText(root.placementStatus)
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.WrapAnywhere
        }
    }

    PlasmaDBus.DBusServiceWatcher {
        id: dockService
        busType: PlasmaDBus.BusType.Session
        watchedService: "org.archdock.ArchDock"
        onRegisteredChanged: root.refreshPlacementStatus()
    }

    PlasmaDBus.Properties {
        busType: PlasmaDBus.BusType.Session
        service: "org.archdock.ArchDock"
        path: "/Control"
        iface: "local.PanelWindow"
        onPropertiesChanged: function(interfaceName, changedProperties) {
            if (changedProperties.nativePlacementRevision !== undefined)
                root.refreshPlacementStatus();
        }
        onRefreshed: root.refreshPlacementStatus()
    }

    Component.onCompleted: refreshPlacementStatus()
}
