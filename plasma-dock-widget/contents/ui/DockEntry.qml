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
    property bool triggerPulse: false
    property bool dragging: false
    readonly property bool motionActive: !reducedMotion && (
        (motionTrigger === "hover" && hoverArea.containsMouse)
        || (motionTrigger === "running" && entry.running)
        || ((motionTrigger === "click" || motionTrigger === "launch") && triggerPulse)
        || (motionTrigger === "drop" && (dragging || triggerPulse))
        || motionTrigger === "idle")
    readonly property real amplitude: baseSize * 0.22 * motionIntensity
    readonly property real scaleAmplitude: 0.16 * motionIntensity
    readonly property real rotationAmplitude: motion === "swing" ? 14
        : motion === "wobble" ? 9
        : motion === "wiggle" ? 6 : 3
    readonly property int cycleDuration: Math.max(80, motionDuration)

    function resetMotionLayer() {
        motionLayer.x = 0;
        motionLayer.y = 0;
        motionLayer.scale = 1;
        motionLayer.rotation = 0;
    }

    onMotionChanged: resetMotionLayer()
    onMotionActiveChanged: if (!motionActive) resetMotionLayer()

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

    Item {
        id: motionLayer
        width: parent.width
        height: parent.height

        IconVisual {
            id: visual
            anchors.fill: parent
            entry: root.entry
            iconSize: root.baseSize
            tileShape: root.tileShape
            appearance: root.appearance
            hovered: hoverArea.containsMouse
            pressed: hoverArea.pressed || root.clickPulse
            showReflection: root.showReflection
            glowAmount: root.motion === "glow" && root.motionActive
                ? Math.min(1, root.motionIntensity) : 0
            glowAnimating: root.motion === "glow" && root.motionActive
            glowDuration: root.cycleDuration
        }

        SequentialAnimation {
            running: root.motionActive && root.motion === "bounce"
            loops: Animation.Infinite
            YAnimator { target: motionLayer; from: 0; to: -root.amplitude; duration: root.cycleDuration / 2; easing.type: Easing.OutCubic }
            YAnimator { target: motionLayer; from: -root.amplitude; to: 0; duration: root.cycleDuration / 2; easing.type: Easing.InCubic }
        }
        SequentialAnimation {
            running: root.motionActive && root.motion === "elastic"
            loops: Animation.Infinite
            YAnimator { target: motionLayer; from: 0; to: -root.amplitude * 1.25; duration: root.cycleDuration * 0.35; easing.type: Easing.OutCubic }
            YAnimator { target: motionLayer; from: -root.amplitude * 1.25; to: root.amplitude * 0.28; duration: root.cycleDuration * 0.3 }
            YAnimator { target: motionLayer; from: root.amplitude * 0.28; to: 0; duration: root.cycleDuration * 0.35; easing.type: Easing.OutBack }
        }
        SequentialAnimation {
            running: root.motionActive && ["float", "wave"].includes(root.motion)
            loops: Animation.Infinite
            PauseAnimation { duration: root.motion === "wave" ? root.entryIndex * 45 : 0 }
            YAnimator { target: motionLayer; from: 0; to: -root.amplitude * (root.motion === "wave" ? 0.9 : 0.65); duration: root.cycleDuration; easing.type: Easing.InOutSine }
            YAnimator { target: motionLayer; from: -root.amplitude * (root.motion === "wave" ? 0.9 : 0.65); to: 0; duration: root.cycleDuration; easing.type: Easing.InOutSine }
        }
        SequentialAnimation {
            running: root.motionActive && root.motion === "spring"
            loops: Animation.Infinite
            YAnimator { target: motionLayer; from: 0; to: -root.amplitude; duration: root.cycleDuration * 0.3; easing.type: Easing.OutCubic }
            YAnimator { target: motionLayer; from: -root.amplitude; to: root.amplitude * 0.2; duration: root.cycleDuration * 0.35; easing.type: Easing.OutBack }
            YAnimator { target: motionLayer; from: root.amplitude * 0.2; to: 0; duration: root.cycleDuration * 0.35 }
        }
        SequentialAnimation {
            running: root.motionActive && ["pulse", "scale", "breathe", "ripple", "magnetic"].includes(root.motion)
            loops: Animation.Infinite
            ScaleAnimator {
                target: motionLayer
                from: 1
                to: 1 + root.scaleAmplitude * (root.motion === "magnetic" ? 1.6 : root.motion === "ripple" ? 1.3 : 1)
                duration: root.motion === "breathe" ? root.cycleDuration * 2 : root.cycleDuration
                easing.type: Easing.InOutSine
            }
            ScaleAnimator {
                target: motionLayer
                from: 1 + root.scaleAmplitude * (root.motion === "magnetic" ? 1.6 : root.motion === "ripple" ? 1.3 : 1)
                to: 1
                duration: root.motion === "breathe" ? root.cycleDuration * 2 : root.cycleDuration
                easing.type: Easing.InOutSine
            }
        }
        RotationAnimator {
            target: motionLayer
            running: root.motionActive && (root.motion === "spin" || root.motion === "idle-rotate")
            from: 0
            to: 360
            duration: root.motion === "idle-rotate" ? root.cycleDuration * 8 : root.cycleDuration * 2
            loops: Animation.Infinite
        }
        SequentialAnimation {
            running: root.motionActive && ["swing", "wobble", "wiggle", "shake"].includes(root.motion)
            loops: Animation.Infinite
            RotationAnimator {
                target: motionLayer
                from: -root.rotationAmplitude * root.motionIntensity
                to: root.rotationAmplitude * root.motionIntensity
                duration: root.motion === "wiggle" || root.motion === "shake" ? root.cycleDuration * 0.35 : root.cycleDuration
            }
            RotationAnimator {
                target: motionLayer
                from: root.rotationAmplitude * root.motionIntensity
                to: -root.rotationAmplitude * root.motionIntensity
                duration: root.motion === "wiggle" || root.motion === "shake" ? root.cycleDuration * 0.35 : root.cycleDuration
            }
        }
        SequentialAnimation {
            running: root.motionActive && root.motion === "orbit"
            loops: Animation.Infinite
            ParallelAnimation {
                XAnimator { target: motionLayer; from: 0; to: root.amplitude; duration: root.cycleDuration / 2; easing.type: Easing.InOutSine }
                YAnimator { target: motionLayer; from: -root.amplitude; to: 0; duration: root.cycleDuration / 2; easing.type: Easing.InOutSine }
            }
            ParallelAnimation {
                XAnimator { target: motionLayer; from: root.amplitude; to: 0; duration: root.cycleDuration / 2; easing.type: Easing.InOutSine }
                YAnimator { target: motionLayer; from: 0; to: root.amplitude; duration: root.cycleDuration / 2; easing.type: Easing.InOutSine }
            }
            ParallelAnimation {
                XAnimator { target: motionLayer; from: 0; to: -root.amplitude; duration: root.cycleDuration / 2; easing.type: Easing.InOutSine }
                YAnimator { target: motionLayer; from: root.amplitude; to: 0; duration: root.cycleDuration / 2; easing.type: Easing.InOutSine }
            }
            ParallelAnimation {
                XAnimator { target: motionLayer; from: -root.amplitude; to: 0; duration: root.cycleDuration / 2; easing.type: Easing.InOutSine }
                YAnimator { target: motionLayer; from: 0; to: -root.amplitude; duration: root.cycleDuration / 2; easing.type: Easing.InOutSine }
            }
        }
    }

    Timer {
        id: triggerTimer
        interval: Math.max(120, root.cycleDuration * 2)
        onTriggered: root.triggerPulse = false
    }

    function fireTrigger() {
        triggerPulse = false;
        triggerPulse = true;
        triggerTimer.restart();
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
            root.fireTrigger();
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
            if (root.motionTrigger === "drop")
                root.fireTrigger();
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
