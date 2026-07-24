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
            id: material
            Kirigami.FormData.label: qsTr("Material")
            model: [
                { label: qsTr("Glass"), value: "glass" },
                { label: qsTr("Minimal"), value: "minimal" },
                { label: qsTr("Floating glass"), value: "floating-glass" },
                { label: qsTr("Platform"), value: "platform" },
                { label: qsTr("Individual plates"), value: "plate" }
            ]
            textRole: "label"
            currentIndex: root.optionIndex(model, root.values.appearance || "glass")
            onActivated: root.setValue("appearance", model[currentIndex].value)
        }
        QQC2.ComboBox {
            id: iconShape
            Kirigami.FormData.label: qsTr("Icon plate shape")
            model: ["rounded", "square", "circle", "squircle"]
            currentIndex: Math.max(0, model.indexOf(root.values.iconShape || "rounded"))
            onActivated: root.setValue("iconShape", currentText)
        }
        QQC2.Slider {
            Kirigami.FormData.label: qsTr("Opacity")
            from: 0.1
            to: 1
            stepSize: 0.05
            value: root.values.opacity === undefined ? 0.9 : Number(root.values.opacity)
            onMoved: root.setValue("opacity", value)
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: qsTr("Reflections")
            text: qsTr("Show icon reflections")
            checked: root.values.showReflections || false
            onToggled: root.setValue("showReflections", checked)
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: qsTr("Indicators")
            text: qsTr("Show running application indicators")
            checked: root.values.showIndicators === undefined ? true : root.values.showIndicators
            onToggled: root.setValue("showIndicators", checked)
        }
    }
}
