import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    property string cfg_panelId: ""
    property string cfg_panelIdDefault: ""
    property string cfg_panelType: "hybrid"
    property string cfg_panelTypeDefault: "hybrid"
    property string cfg_ownerToken: ""
    property string cfg_ownerTokenDefault: ""
    property bool cfg_bootstrapFreeDock: false
    property bool cfg_bootstrapFreeDockDefault: false

    Kirigami.FormLayout {
        QQC2.Label {
            Kirigami.FormData.isSection: true
            text: qsTr("Animation is not registered as a native configuration page. Open Panel Studio for schema-defined animation settings.")
            wrapMode: Text.WordWrap
        }
    }
}
