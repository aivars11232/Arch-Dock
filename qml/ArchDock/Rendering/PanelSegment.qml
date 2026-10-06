import QtQuick
import ArchDock.Rendering 1.0

// One independent segment of a segmented panel: its own background,
// padding, corners and open or closed resting state, opened by hover and held
// open by the panel's guards like a whole panel.
Item {
    id: root
    property var definition: ({})
    property var geometry: ({})
    property var runtimeState: ({})
    property var motionCatalog: ({})
    property bool reducedMotion: false
    property bool concealed: false
    property string layout: "horizontal"
    property real layoutAngle: 0
    property string appearance: "glass"
    property string customColor: ""
    property real panelOpacity: 0.9
    property bool forceOpen: false
    property bool entryHovered: false
    readonly property bool guarded: Boolean(runtimeState.popupOpen || runtimeState.dragInProgress
        || runtimeState.editMode || forceOpen)
    readonly property bool hovered: pointer.hovered || entryHovered
    readonly property bool expanded: presentation.surfaceState === "open"
    readonly property real collapseProgress: motion.collapseProgress
    readonly property var contentClip: motion.contentClip
    readonly property var channels: animation.channels
    readonly property var visualMotion: MotionChannels.motionFor(channels, "icon", { size: Math.min(width, height) })
    readonly property real cornerRadius: definition.corners === "square" ? 0
        : definition.corners === "capsule" ? Math.min(width, height) / 2 : 12
    objectName: "panel-segment-" + String(definition.id || "")
    width: Number(geometry.width || 0)
    height: Number(geometry.height || 0)
    function updatePresentation() {
        if (hovered || guarded || definition.presentation !== "closed") presentation.requestOpen()
        else presentation.requestCollapse()
    }
    onHoveredChanged: updatePresentation()
    onGuardedChanged: updatePresentation()
    // Content snapshots also refresh this definition. Preserve the active
    // state and let the controller's requests/guards handle preference changes;
    // resetting here can hide an entry while its hover binding is evaluating.
    onDefinitionChanged: updatePresentation()
    Component.onCompleted: updatePresentation()
    HoverHandler { id: pointer; enabled: !root.concealed }
    PanelPresentationController {
        id: presentation
        restingState: root.definition.presentation === "closed" ? "collapsed" : "open"
        reducedMotion: root.reducedMotion
        pointerInside: root.hovered
        popupOpen: root.guarded
    }
    PanelMotionController {
        id: motion
        surfaceWidth: root.width
        surfaceHeight: root.height
        contentBounds: ({ x: 0, y: 0, width: root.width, height: root.height })
        mechanism: root.layout === "vertical" ? "collapse-vertical" : "collapse-horizontal"
        surfaceState: presentation.surfaceState
        transitionState: presentation.transitionState
        presentationProgress: presentation.progress
        reducedMotion: root.reducedMotion
        handleExtent: Math.min(24, root.layout === "vertical" ? root.height : root.width)
    }
    IconMotionController {
        id: animation
        profiles: root.motionCatalog[String(root.definition.motionProfile || "")]
            ? [root.motionCatalog[String(root.definition.motionProfile)]] : []
        catalog: root.motionCatalog
        reducedMotion: root.reducedMotion
        hovered: root.hovered
        sceneVisible: !root.concealed && root.visible && root.expanded
    }
    Item {
        x: motion.contentClip.x
        y: motion.contentClip.y
        width: motion.contentClip.width
        height: motion.contentClip.height
        clip: root.collapseProgress > 0
        Item {
            x: -motion.contentClip.x
            y: -motion.contentClip.y
            width: root.width
            height: root.height
            scale: root.visualMotion.scale
            opacity: root.visualMotion.opacity
            // Solid segment surfaces use Qt's native rounded rectangle;
            // inherited surfaces keep the panel's existing renderer.
            Rectangle {
                anchors.fill: parent
                visible: root.definition.background === "solid"
                color: String(root.definition.color || "#202b36")
                radius: root.cornerRadius
                opacity: root.panelOpacity
            }
            PanelSurfaceLoader {
                anchors.fill: parent
                visible: !["none", "solid"].includes(String(root.definition.background || "inherited"))
                geometry: root.geometry
                layout: root.layout
                layoutAngle: root.layoutAngle
                appearance: root.appearance
                customColor: root.customColor
                panelOpacity: root.panelOpacity
                reducedMotion: root.reducedMotion
            }
        }
    }
    Rectangle {
        anchors.centerIn: parent
        width: root.layout === "vertical" ? 18 : 6
        height: root.layout === "vertical" ? 6 : 18
        radius: 3
        color: "#aabac7"
        visible: !root.expanded
    }
}
