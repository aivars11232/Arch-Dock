import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

ConfigPageBase {
    id: root

    Kirigami.FormLayout {
        QQC2.SpinBox {
            Kirigami.FormData.label: qsTr("Icon size")
            from: 24
            to: 128
            value: Number(root.values.iconSize || 52)
            onValueModified: root.setValue("iconSize", value)
        }
        QQC2.SpinBox {
            Kirigami.FormData.label: qsTr("Icon spacing")
            from: 0
            to: 48
            value: Number(root.values.spacing || 8)
            onValueModified: root.setValue("spacing", value)
        }
        QQC2.CheckBox {
            Kirigami.FormData.label: qsTr("Magnification")
            text: qsTr("Magnify icons under the pointer")
            checked: root.values.magnificationEnabled === undefined
                ? true : root.values.magnificationEnabled
            onToggled: root.setValue("magnificationEnabled", checked)
        }
        QQC2.Slider {
            Kirigami.FormData.label: qsTr("Magnification amount")
            from: 1
            to: 2.5
            stepSize: 0.05
            value: Number(root.values.magnification || 1.65)
            onMoved: root.setValue("magnification", value)
        }
    }
}
