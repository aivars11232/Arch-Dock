import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

ConfigPageBase {
    id: root

    Kirigami.FormLayout {
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
        QQC2.Label {
            Kirigami.FormData.isSection: true
            text: qsTr("Panel position, alignment, floating mode, length, and visibility are managed by Plasma's panel Edit Mode.")
            wrapMode: Text.WordWrap
        }
    }
}
