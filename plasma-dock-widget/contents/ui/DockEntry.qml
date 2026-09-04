import QtQuick
import QtQuick.Controls as QQC2
import ArchDock.Rendering 1.0
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
    // Validated animation profiles bound to this entry, plus the catalog used
    // to resolve reduced-motion substitutes.
    property var animationProfiles: []
    property var animationCatalog: ({})
    // False while the panel cannot be seen. Continuous motion is pointless then
    // and costs frames, so the controller withdraws every track.
    property bool sceneVisible: true
    property real motionSpeed: 1
    required property bool inputEnabled
    required property bool editMode
    required property bool acceptDrops
    required property var invoke
    required property var reorder
    required property var pinUrls
    required property var setHoveredIndex
    required property var openPanelStudio
    required property var openIconProperties
    // Reports this entry's interaction guards to the panel, which is what
    // stops the panel closing underneath an open menu or an active drag.
    // Defaulted rather than required so an existing host that does not yet
    // collect guards keeps working unchanged.
    property var setEntryGuard: function(index, name, active) {}
    property var iconStyleDefinition: ({})
    property var iconOverrideResolution:
        entry && entry.iconOverrideResolution
        ? entry.iconOverrideResolution : ({})

    // How far magnification reaches and how it decays. Defaults are the
    // historical curve, so a panel that configures neither behaves as before.
    property real magnificationRadius: 2.4
    property string magnificationFalloff: "linear"
    readonly property int indexDistance: hoveredIndex < 0 ? 99 : Math.abs(hoveredIndex - entryIndex)
    readonly property real influence: !magnificationEnabled || hoveredIndex < 0
        ? 0 : MotionChannels.magnificationInfluence(
            indexDistance, magnificationRadius, magnificationFalloff)
    readonly property real hoverScale: MotionChannels.magnificationScale(
        magnification, influence)
    property bool clickPulse: false
    property bool dragging: false
    readonly property int cycleDuration: Math.max(80, motionDuration)
    readonly property real visualScale: magnificationLayer.scale
    readonly property var logicalInputRegion: ({
        x: 0,
        y: 0,
        width: baseSize,
        height: baseSize
    })
    // The effect and the event that starts it are separate user settings, so
    // the configured trigger overrides the profile's nominal one.
    readonly property var boundAnimationProfiles: {
        const source = animationProfiles || []
        const trigger = AnimationProfileRuntime.normalizeTrigger(motionTrigger)
        const result = []
        for (let index = 0; index < source.length; ++index) {
            const profile = source[index]
            if (!profile || !profile.id)
                continue
            const bound = ({})
            for (const key of Object.keys(profile))
                bound[key] = profile[key]
            if (trigger !== "")
                bound.trigger = trigger
            result.push(bound)
        }
        return result
    }
    // This entry's place in its panel: which way is out, which way is along the
    // run, and how much room an effect may use. Supplied by PanelScene; the
    // safe default is an upright, unbounded entry so a host that supplies
    // nothing still renders.
    property var entryGeometry: ({})
    readonly property var motionContext: ({
        size: baseSize,
        normal: entryGeometry && entryGeometry.outwardNormal
            ? entryGeometry.outwardNormal : ({ x: 0, y: -1 }),
        tangentAngle: entryGeometry && entryGeometry.tangentAngle !== undefined
            ? Number(entryGeometry.tangentAngle) : 0,
        allowance: entryGeometry && entryGeometry.effectAllowance
            ? entryGeometry.effectAllowance : null
    })
    readonly property var iconMotion: MotionChannels.motionFor(
        motionController.channels, "icon", motionContext)
    readonly property var glyphMotion: MotionChannels.motionFor(
        motionController.channels, "glyph", motionContext)
    readonly property var tileMotion: MotionChannels.motionFor(
        motionController.channels, "tile", motionContext)
    readonly property var indicatorMotion: MotionChannels.motionFor(
        motionController.channels, "indicator", motionContext)
    readonly property alias motionController: motionController
    readonly property var motionConflicts: motionController.conflicts
    readonly property var activeMotionProfiles: motionController.activeProfileIds
    readonly property string iconVisualState: visual.visualState
    readonly property string resolvedIconStyleId:
        visual.resolvedIconStyle.styleId || "plain-original"
    readonly property string resolvedIconSource: visual.resolvedIconSource
    readonly property bool tileRenderingEnabled: visual.tileRenderingEnabled
    readonly property bool iconPropertiesSupported: Boolean(
        entry && entry.iconPropertiesSupported === true
        && String(entry.stableIdentity || "").length > 0)
    readonly property bool contextInteractionAllowed: inputEnabled
        && !editMode && !dragging
    readonly property bool contextMenuVisible: contextMenu.visible
    readonly property bool iconPropertiesActionVisible:
        iconPropertiesSupported
    readonly property var sceneEntry: {
        const result = ({})
        const source = root.entry || ({})
        const keys = Object.keys(source)
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = source[keys[index]]
        result.iconOverrideResolution = root.iconOverrideResolution || ({})
        return result
    }

    // Delivers a discrete motion event. State-driven events (hover hold,
    // running, urgent, drop) are derived from properties instead.
    function dispatchMotionEvent(event) {
        return motionController.dispatch(event);
    }

    // What the activation actually did. A verified start is a success and a
    // proved failure is a failure; anything the platform could not confirm is
    // reported as neither, so no success animation can run without a success.
    function reportLaunchOutcome(result) {
        const outcome = String((result && result.outcome) || "requested")
        if (outcome === "succeeded")
            return dispatchMotionEvent("launch-succeeded")
        if (outcome === "failed")
            return dispatchMotionEvent("launch-failed")
        return false
    }

    function openEntryContextMenu() {
        if (!contextInteractionAllowed)
            return false
        contextMenu.open()
        return true
    }

    function requestIconProperties() {
        if (!contextInteractionAllowed || !iconPropertiesSupported)
            return false
        contextMenu.close()
        openIconProperties(entry)
        return true
    }

    // Running is a continuous state, so `running-started` is satisfied by the
    // controller's own `running` property. Only stopping is a discrete event.
    property bool entryRunning: Boolean(entry && entry.running)
    onEntryRunningChanged: if (!entryRunning) dispatchMotionEvent("running-stopped")

    onEditModeChanged: if (editMode) contextMenu.close()
    onDraggingChanged: {
        if (dragging)
            contextMenu.close();
        setEntryGuard(entryIndex, "drag", dragging);
    }
    onContextMenuVisibleChanged:
        setEntryGuard(entryIndex, "menu", contextMenuVisible)
    onInputEnabledChanged: {
        if (!inputEnabled) {
            dragging = false;
            clickPulse = false;
            setHoveredIndex(-1);
        }
    }
    // An entry that goes away must not leave a guard behind holding the panel
    // open forever.
    Component.onDestruction: {
        setEntryGuard(entryIndex, "drag", false);
        setEntryGuard(entryIndex, "menu", false);
    }

    width: baseSize
    height: width
    z: hoverArea.containsMouse || dragging ? 10 : influence

    IconMotionController {
        id: motionController

        profiles: root.boundAnimationProfiles
        catalog: root.animationCatalog
        entryIndex: root.entryIndex
        intensity: root.motionIntensity
        speed: root.motionSpeed
        reducedMotion: root.reducedMotion
        sceneVisible: root.sceneVisible
        hovered: hoverArea.containsMouse
        pressed: hoverArea.pressed || root.clickPulse
        running: Boolean(root.entry.running)
        urgent: Boolean(root.entry.attention || root.entry.urgent)
        dropActive: root.dragging || entryDropArea.containsDrag
        revealed: root.inputEnabled
    }

    Item {
        id: magnificationLayer

        anchors.centerIn: parent
        width: root.baseSize
        height: root.baseSize
        scale: root.hoverScale

        Behavior on scale {
            enabled: !root.reducedMotion
            NumberAnimation {
                duration: root.motionDuration
                easing.type: Easing.OutCubic
            }
        }

        // Driven only by the motion controller, which composes every effect
        // from validated animation profiles. It transforms the visual only:
        // `root` keeps its logical size and input region.
        Item {
            id: profileMotionLayer

            width: parent.width
            height: parent.height
            x: root.iconMotion.x
            y: root.iconMotion.y
            rotation: root.iconMotion.rotateZ
            opacity: root.iconMotion.opacity
            transform: [
                Matrix4x4 {
                    matrix: Math.abs(root.iconMotion.rotateY) > 0.01
                        ? MotionChannels.turnMatrix(
                            root.iconMotion.rotateY, profileMotionLayer.width,
                            profileMotionLayer.height, root.baseSize * 2.4)
                        : Qt.matrix4x4()
                },
                Scale {
                    origin.x: profileMotionLayer.width / 2
                    origin.y: profileMotionLayer.height / 2
                    xScale: root.iconMotion.scale * root.iconMotion.scaleX
                    yScale: root.iconMotion.scale * root.iconMotion.scaleY
                }
            ]

            IconScene {
                id: visual

                anchors.fill: parent
                entry: root.sceneEntry
                iconStyleDefinition: root.iconStyleDefinition
                logicalSize: root.baseSize
                tileShape: root.tileShape
                appearance: root.appearance
                vertical: root.vertical
                hovered: hoverArea.containsMouse
                pressed: hoverArea.pressed || root.clickPulse
                active: Boolean(root.entry.active)
                running: Boolean(root.entry.running)
                minimized: Boolean(root.entry.minimized)
                urgent: Boolean(root.entry.attention || root.entry.urgent)
                launching: Boolean(root.entry.launching)
                disabled: Boolean(root.entry.disabled)
                dropTarget: root.dragging || entryDropArea.containsDrag
                editMode: root.editMode
                windowCount: Math.max(1, root.entry.windowCount || 1)
                showReflection: root.showReflection
                showIndicator: root.showIndicator
                reducedMotion: root.reducedMotion
                glyphMotion: root.glyphMotion
                tileMotion: root.tileMotion
                indicatorMotion: root.indicatorMotion
                glowAmount: Math.max(
                    root.iconMotion.glow,
                    Math.max(root.glyphMotion.glow, root.tileMotion.glow))
                // The profile supplies the pulse; the renderer no longer runs
                // an animation of its own for it.
                glowAnimating: false
                glowDuration: root.cycleDuration
            }
        }
    }

    MouseArea {
        id: hoverArea

        objectName: "dockEntryPointerTarget"
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        enabled: root.inputEnabled
        drag.target: root.acceptDrops ? dragProxy : null
        drag.threshold: Kirigami.Units.gridUnit / 2

        onEntered: {
            root.setHoveredIndex(root.entryIndex);
            root.dispatchMotionEvent("hover-enter");
        }
        onExited: {
            if (!root.dragging)
                root.setHoveredIndex(-1);
            root.dispatchMotionEvent("hover-exit");
        }
        onPressed: {
            root.clickPulse = true;
            root.dispatchMotionEvent("press");
        }
        onCanceled: root.clickPulse = false
        onClicked: mouse => {
            if (root.dragging)
                return;
            root.dispatchMotionEvent("click");
            if (mouse.button === Qt.RightButton) {
                root.openEntryContextMenu();
                return;
            }
            // A launch was asked for. Whether it succeeded is a separate,
            // verified outcome, reported back through reportLaunchOutcome.
            root.dispatchMotionEvent("launch-requested");
            root.invoke("activateDockEntry", root.entry.appId,
                        root.reportLaunchOutcome);
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
        id: entryDropArea

        anchors.fill: parent
        enabled: root.acceptDrops && root.inputEnabled
        keys: ["application/x-archdock-app", "text/uri-list"]
        onEntered: drag => {
            root.dispatchMotionEvent("drop-entered");
            if (drag.source && drag.source.entry)
                root.reorder(drag.source.entry.appId, root.entry.appId);
        }
        onDropped: drop => {
            if (drop.hasUrls)
                root.pinUrls(drop.urls);
            root.dispatchMotionEvent("drop-committed");
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
            id: iconPropertiesAction

            objectName: "iconPropertiesAction"
            text: qsTr("Icon Properties…")
            icon.name: "document-properties"
            visible: root.iconPropertiesSupported
            enabled: root.contextInteractionAllowed
            onTriggered: root.requestIconProperties()
        }
        QQC2.MenuSeparator {
            visible: root.iconPropertiesSupported
        }
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
