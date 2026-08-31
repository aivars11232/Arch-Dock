import QtQuick
import QtQuick.Window
import "../plasma-dock-widget/contents/ui" as DockUi

Window {
    id: root

    objectName: "iconPropertiesInteractionHarness"

    property string panelId: "bottom"
    property var interactionEntry: ({})
    property var lastOpenResult: ({})
    property int hoveredEntry: -1

    width: 320
    height: 220
    visible: true
    color: "#202833"
    title: "Arch Dock Icon Properties interaction harness"

    DockUi.DockEntry {
        id: liveEntry

        objectName: "liveDockEntry"
        anchors.centerIn: parent
        entry: root.interactionEntry
        entryIndex: 0
        vertical: false
        baseSize: 72
        magnification: 1.4
        magnificationEnabled: true
        hoveredIndex: root.hoveredEntry
        tileShape: "rounded"
        appearance: "glass"
        showReflection: false
        showIndicator: true
        showTooltip: false
        motion: "scale"
        motionTrigger: "hover"
        motionIntensity: 1
        motionDuration: 0
        reducedMotion: true
        inputEnabled: true
        editMode: false
        acceptDrops: true
        invoke: function() {}
        reorder: function() {}
        pinUrls: function() {}
        setHoveredIndex: function(index) {
            root.hoveredEntry = index
        }
        openPanelStudio: function() {}
        openIconProperties: function(candidate) {
            root.lastOpenResult = panelController.showIconProperties(
                root.panelId, String(candidate.stableIdentity || ""))
        }
    }
}
