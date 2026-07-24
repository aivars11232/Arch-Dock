import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

ConfigPageBase {
    id: root

    function optionIndex(model, value) {
        for (let index = 0; index < model.length; ++index)
            if (model[index].value === value)
                return index;
        return 0;
    }

    Kirigami.FormLayout {
        QQC2.ComboBox {
            id: effect
            Kirigami.FormData.label: qsTr("Effect")
            model: [
                { label: qsTr("None"), value: "none" },
                { label: qsTr("Scale"), value: "scale" },
                { label: qsTr("Bounce"), value: "bounce" },
                { label: qsTr("Pulse"), value: "pulse" },
                { label: qsTr("Spin"), value: "spin" },
                { label: qsTr("Slow rotation"), value: "idle-rotate" },
                { label: qsTr("Glow"), value: "glow" }
            ]
            textRole: "label"
            currentIndex: root.optionIndex(model, root.values.iconAnimation || "scale")
            onActivated: root.setValue("iconAnimation", model[currentIndex].value)
        }
        QQC2.ComboBox {
            id: trigger
            Kirigami.FormData.label: qsTr("Trigger")
            model: [
                { label: qsTr("Hover"), value: "hover" },
                { label: qsTr("Running"), value: "running" },
                { label: qsTr("Always"), value: "idle" }
            ]
            textRole: "label"
            currentIndex: root.optionIndex(model, root.values.animationTrigger || "hover")
            onActivated: root.setValue("animationTrigger", model[currentIndex].value)
        }
        QQC2.SpinBox {
            Kirigami.FormData.label: qsTr("Duration")
            from: 80
            to: 1200
            stepSize: 10
            value: Number(root.values.animationDuration || 170)
            onValueModified: root.setValue("animationDuration", value)
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: qsTr("Accessibility")
            text: qsTr("Reduce motion")
            checked: root.values.reducedMotion || false
            onToggled: root.setValue("reducedMotion", checked)
        }
    }
}
