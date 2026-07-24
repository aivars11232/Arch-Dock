import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Item {
    id: root

    required property var entry
    required property int entryIndex
    required property bool vertical
    required property real baseSize
    required property real magnification
    required property bool magnificationEnabled
    required property int hoveredIndex
    required property string tileShape
    required property string appearance
    required property bool showReflection
    required property bool showIndicator
    required property bool showTooltip
    required property string motion
    required property string motionTrigger
    required property real motionIntensity
    required property int motionDuration
    required property bool reducedMotion
    required property bool inputEnabled
    required property bool acceptDrops
    required property var invoke
    required property var reorder
    required property var pinUrls
    required property var setHoveredIndex
    required property var openPanelStudio

    readonly property int indexDistance: hoveredIndex < 0 ? 99 : Math.abs(hoveredIndex - entryIndex)
    readonly property real influence: !magnificationEnabled || hoveredIndex < 0
        ? 0 : Math.max(0, 1 - indexDistance / 2.4)
    readonly property real hoverScale: 1 + (Math.max(1, magnification) - 1) * influence
    property bool clickPulse: false
    property bool dragging: false
    readonly property bool motionActive: !reducedMotion && (
        (motionTrigger === "hover" && hoverArea.containsMouse)
        || (motionTrigger === "running" && entry.running)
        || motionTrigger === "idle")
    readonly property real effectScale: motion === "pulse" && motionActive ? 1.07 * motionIntensity
        : motion === "scale" && motionActive ? 1 + 0.08 * motionIntensity : 1

    width: baseSize * hoverScale
    height: width
    z: hoverArea.containsMouse || dragging ? 10 : influence

    Behavior on width {
        enabled: !root.reducedMotion
        NumberAnimation { duration: root.motionDuration; easing.type: Easing.OutCubic }
    }
    Behavior on height {
        enabled: !root.reducedMotion
        NumberAnimation { duration: root.motionDuration; easing.type: Easing.OutCubic }
    }

    IconVisual {
        id: visual
        anchors.fill: parent
        entry: root.entry
        iconSize: root.baseSize
        tileShape: root.tileShape
        appearance: root.appearance
        hovered: hoverArea.containsMouse
        pressed: hoverArea.pressed
        showReflection: root.showReflection
        glowAmount: root.motion === "glow" && root.motionActive ? 1 : 0
        scale: root.effectScale * (root.clickPulse ? 0.84 : 1)
        rotation: 0
        y: root.motion === "bounce" && root.motionActive ? -root.baseSize * 0.12 * root.motionIntensity : 0

        Behavior on scale { NumberAnimation { duration: root.motionDuration; easing.type: Easing.OutBack } }
        Behavior on y { NumberAnimation { duration: root.motionDuration; easing.type: Easing.OutBack } }
        Behavior on glowAmount { NumberAnimation { duration: root.motionDuration } }
        RotationAnimation on rotation {
            running: root.motionActive && (root.motion === "spin" || root.motion === "idle-rotate")
            from: 0
            to: 360
            duration: root.motion === "idle-rotate" ? root.motionDuration * 8 : root.motionDuration * 2
            loops: root.motionTrigger === "idle" ? Animation.Infinite : 1
        }
    }

    RunningIndicator {
        visible: root.showIndicator && root.entry.running
        vertical: root.vertical
        active: root.entry.active
        urgent: root.entry.attention || false
        windowCount: Math.max(1, root.entry.windowCount || 1)
        anchors.horizontalCenter: root.vertical ? undefined : parent.horizontalCenter
        anchors.verticalCenter: root.vertical ? parent.verticalCenter : undefined
        anchors.bottom: root.vertical ? undefined : parent.bottom
        anchors.left: root.vertical ? parent.left : undefined
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        enabled: root.inputEnabled
        drag.target: root.acceptDrops ? dragProxy : null
        drag.threshold: Kirigami.Units.gridUnit / 2

        onEntered: root.setHoveredIndex(root.entryIndex)
        onExited: if (!root.dragging) root.setHoveredIndex(-1)
        onPressed: root.clickPulse = true
        onCanceled: root.clickPulse = false
        onClicked: mouse => {
            if (root.dragging)
                return;
            if (mouse.button === Qt.RightButton)
                contextMenu.open();
            else
                root.invoke("activateDockEntry", root.entry.appId);
        }
        onPositionChanged: {
            if (drag.active)
                root.dragging = true;
        }
        onReleased: mouse => {
            root.clickPulse = false;
            if (root.dragging) {
                root.dragging = false;
                root.setHoveredIndex(-1);
            }
        }
    }

    Item {
        id: dragProxy
        width: 1
        height: 1
        Drag.active: hoverArea.drag.active
        Drag.source: root
        Drag.hotSpot.x: 0
        Drag.hotSpot.y: 0
        Drag.mimeData: ({ "application/x-archdock-app": root.entry.appId })
    }

    DropArea {
        anchors.fill: parent
        enabled: root.acceptDrops && root.inputEnabled
        keys: ["application/x-archdock-app", "text/uri-list"]
        onEntered: drag => {
            if (drag.source && drag.source.entry)
                root.reorder(drag.source.entry.appId, root.entry.appId);
        }
        onDropped: drop => {
            if (drop.hasUrls)
                root.pinUrls(drop.urls);
            drop.acceptProposedAction();
        }
    }

    QQC2.ToolTip.visible: root.showTooltip && hoverArea.containsMouse && !root.dragging
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
    QQC2.ToolTip.text: entry.windowCount > 1
        ? qsTr("%1 (%2 windows)").arg(entry.displayName).arg(entry.windowCount)
        : entry.displayName

    QQC2.Menu {
        id: contextMenu
        QQC2.MenuItem {
            text: qsTr("Open Panel Studio…")
            icon.name: "configure"
            onTriggered: root.openPanelStudio()
        }
        QQC2.MenuSeparator {}
        QQC2.MenuItem {
            text: entry.pinned ? qsTr("Unpin") : qsTr("Pin")
            onTriggered: root.invoke("togglePinnedDockEntry", entry.appId)
        }
        QQC2.MenuItem {
            text: entry.minimized ? qsTr("Restore") : qsTr("Minimize")
            visible: entry.running
            onTriggered: root.invoke("minimizeDockEntry", entry.appId)
        }
        QQC2.MenuItem {
            text: entry.windowCount > 1 ? qsTr("Close all windows") : qsTr("Close")
            visible: entry.running
            onTriggered: root.invoke(entry.windowCount > 1 ? "closeAllDockEntry" : "closeDockEntry",
                                     entry.appId)
        }
    }
}
