import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

ConfigPageBase {
    id: root

    property var visibilityResult: ({})
    readonly property var visibilityLabels: ({
        "always": qsTr("Always visible"),
        "auto-hide": qsTr("Auto-hide"),
        "dodge": qsTr("Dodge touching windows"),
        "cover": qsTr("Hide for maximized/fullscreen")
    })
    readonly property var visibilityOptions: {
        const source = visibilityResult
            && Array.isArray(visibilityResult.supportedModes)
            ? visibilityResult.supportedModes : [];
        const options = [];
        for (let index = 0; index < source.length; ++index) {
            const mode = String(source[index]);
            if (root.visibilityLabels[mode] !== undefined) {
                options.push({
                    label: root.visibilityLabels[mode],
                    value: mode
                });
            }
        }
        return options;
    }

    function visibilityOptionIndex(mode) {
        for (let index = 0; index < visibilityOptions.length; ++index) {
            if (visibilityOptions[index].value === mode)
                return index;
        }
        return 0;
    }

    function visibilityDetail(result) {
        if (!result)
            return qsTr("No visibility result");
        if (result.fallbackApplied === true) {
            return qsTr(
                "The requested mode could not be verified (%1). Always visible was applied and verified.")
                .arg(String(result.fallbackReason || result.errorCode || "unsupported"));
        }
        if (result.success === true)
            return qsTr("Applied and verified on the owned Plasma panel.");
        let detail = String(result.errorCode || "not-attempted");
        if (result.fallbackAttempted === true
                && String(result.fallbackErrorCode || "").length > 0) {
            detail += qsTr("; Always-visible fallback: %1")
                .arg(String(result.fallbackErrorCode));
        }
        return detail;
    }

    function refreshVisibility() {
        if (panelId.length === 0)
            return;
        callDock("nativePanelVisibilityStatus", [panelId], function(reply) {
            if (reply && typeof reply === "object")
                root.visibilityResult = reply;
        });
    }

    function applyVisibilityMode(mode) {
        callDock("applyNativePanelVisibilityMode", [panelId, String(mode)],
            function(reply) {
                if (reply && typeof reply === "object")
                    root.visibilityResult = reply;
                root.refresh();
            });
    }

    function applyVisible(visible) {
        callDock("setPanelVisible", [panelId, Boolean(visible)], function() {
            root.refresh();
            root.refreshVisibility();
        });
    }

    Kirigami.FormLayout {
        QQC2.CheckBox {
            visible: root.visibilityOptions.length > 0
            Kirigami.FormData.label: qsTr("Native visibility")
            text: qsTr("Panel is enabled")
            checked: root.values.visible === undefined ? true : root.values.visible
            onToggled: root.applyVisible(checked)
        }
        QQC2.ComboBox {
            visible: root.visibilityOptions.length > 0
            Kirigami.FormData.label: qsTr("Visibility mode")
            model: root.visibilityOptions
            textRole: "label"
            valueRole: "value"
            currentIndex: root.visibilityOptionIndex(
                String(root.values.visibilityMode || "always"))
            onActivated: root.applyVisibilityMode(currentValue)
        }
        Kirigami.InlineMessage {
            visible: root.visibilityOptions.length > 0
                && (root.visibilityResult.fallbackApplied === true
                    || root.visibilityResult.success !== true)
            Kirigami.FormData.label: qsTr("Host result")
            text: root.visibilityDetail(root.visibilityResult)
            type: root.visibilityResult.fallbackApplied === true
                ? Kirigami.MessageType.Warning
                : Kirigami.MessageType.Error
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: qsTr("Tooltips")
            text: qsTr("Show application names on hover")
            checked: root.values.showTooltips === undefined ? true : root.values.showTooltips
            onToggled: root.setValue("showTooltips", checked)
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: qsTr("File drops")
            text: qsTr("Allow applications and files to be dropped on the dock")
            checked: root.values.acceptDrops === undefined ? true : root.values.acceptDrops
            onToggled: root.setValue("acceptDrops", checked)
        }
    }

    Component.onCompleted: refreshVisibility()
}
