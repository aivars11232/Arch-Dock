import QtQuick
import ArchDock.Rendering 1.0
import org.kde.kirigami as Kirigami
import "../plasma-dock-widget/contents/ui" as DockUi
import "../plasma-dock-widget/contents/ui/SceneDefinition.js" as SceneDefinition

// ADREP-TASK-001 Studio truth matrix: one panel drawn the way the Arch Dock
// applet draws it (plasma-dock-widget/contents/ui/main.qml), from the runtime
// configuration the backend hands that applet. The scene definition is the
// applet's own (SceneDefinition.js). Every line marked "// applet" is copied
// from main.qml, and panel-window-capability-test fails when main.qml no
// longer contains it, so this harness cannot drift from the panel it stands
// for. What it leaves out is input, popups and the Plasma host.
Item {
    id: root

    property var configuration: ({})
    property bool freeSurface: false
    property bool vertical: false
    property int hoveredIndex: -1
    readonly property bool plasmaEditMode: false
    // Two applications, one running with a window, and a folder: enough for
    // tiles, indicators, styles and folders to have something to draw on.
    // Their glyphs are files, as a test session has no icon theme.
    property var entries: [
        { appId: "truth-files", stableIdentity: "application.truth-files", displayName: "Files",
          iconName: Qt.resolvedUrl("data/truth-glyph-files.svg").toString(),
          running: true, windowCount: 1, pinned: true },
        { appId: "truth-terminal", stableIdentity: "application.truth-terminal", displayName: "Terminal",
          iconName: Qt.resolvedUrl("data/truth-glyph-terminal.svg").toString(),
          running: false, windowCount: 0, pinned: true },
        { appId: "truth-folder", stableIdentity: "folder.truth-folder", displayName: "Folder",
          iconName: Qt.resolvedUrl("data/truth-glyph-folder.svg").toString(),
          isFolder: true, running: false, windowCount: 0, pinned: true }
    ]

    readonly property real iconSize: Number(configuration.iconSize || 52) // applet
    readonly property real spacing: Number(configuration.spacing || 8) // applet
    readonly property real baseCellSize: iconSize + Math.max(4, spacing) // applet
    readonly property real magnification: Number(configuration.magnification || 1.65) // applet
    readonly property int motionDuration: configuration.reducedMotion // applet
        ? 0 : Math.max(80, Math.min(1200, // applet
            Number(configuration.animationDuration || 170) // applet
            / Math.max(0.2, Number(configuration.animationSpeed || 1)))) // applet
    readonly property var capabilityResolution: // applet
        configuration.capabilityResolution || ({}) // applet
    readonly property var activeAnimationProfiles: { // applet
        const profile = root.configuration.animationProfile // applet
        return profile && profile.id ? [profile] : [] // applet
    }
    readonly property var animationCatalogMap: { // applet
        const result = ({}) // applet
        const list = root.configuration.animationProfiles || [] // applet
        for (let index = 0; index < list.length; ++index) { // applet
            const profile = list[index] // applet
            if (profile && profile.id) // applet
                result[profile.id] = profile // applet
        }
        return result // applet
    }
    readonly property real motionSpeedScale: // applet
        170 / Math.max(80, Math.min(1200, Number(motionDuration) || 170)) // applet
    readonly property string effectiveRendererTier: // applet
        String(configuration.effectiveRendererTier || "") // applet
    readonly property real motionHeadroom: configuration.reducedMotion // applet
        ? 0 // applet
        : baseCellSize * MotionChannels.maximumDisplacement( // applet
            activeAnimationProfiles, // applet
            Number(configuration.animationIntensity || 1)) // applet
    readonly property real nativeScenePadding: Kirigami.Units.largeSpacing // applet
        + (configuration.magnificationEnabled // applet
            ? baseCellSize * Math.max(0, magnification - 1) : 0) // applet
        + motionHeadroom // applet
    readonly property var scenePanelDefinition: SceneDefinition.build(configuration, {
        freeSurface: freeSurface, // applet
        vertical: vertical, // applet
        effectiveRendererTier: effectiveRendererTier, // applet
        nativeScenePadding: nativeScenePadding, // applet
        baseCellSize: baseCellSize // applet
    })
    // A panel resting open; the pointer reaches entries through their own
    // input, as on the desktop.
    readonly property var sceneRuntimeState: ({
        hovered: hoveredIndex >= 0,
        hoveredEntry: hoveredIndex,
        editMode: false,
        dragInProgress: false,
        popupOpen: false,
        modalPopupOpen: false,
        rendererFallback: "", // applet
        presentationState: "open",
        transitionState: "idle",
        presentationProgress: 1,
        hostPhase: "revealed"
    })
    readonly property alias scene: panelScene

    // Where the pointer goes to hover an entry, in window coordinates.
    function entryPoint(index) {
        const item = panelScene.entryItemAt(index)
        if (!item)
            return Qt.point(-1, -1)
        return item.mapToItem(null, item.width / 2, item.height / 2)
    }

    // The host the panel lives in: a fixed size with the scene centred, as a
    // Plasma panel or desktop widget holds it. Zero follows the scene.
    property real hostWidth: 0
    property real hostHeight: 0
    width: hostWidth > 0 ? hostWidth : Math.max(1, panelScene.width)
    height: hostHeight > 0 ? hostHeight : Math.max(1, panelScene.height)

    PanelScene {
        id: panelScene

        anchors.centerIn: parent // applet
        panelDefinition: root.scenePanelDefinition // applet
        runtimeState: root.sceneRuntimeState // applet
        orderedEntries: root.entries // applet
        hostCapabilities: root.capabilityResolution
        themeDefinition: root.configuration.themeDefinition || ({}) // applet
        iconStyleDefinition: // applet
            root.configuration.iconStyleDefinition || ({}) // applet
        animationProfiles: ({ // applet
            reducedMotion: root.configuration.reducedMotion, // applet
            duration: root.motionDuration // applet
        })
        entryDelegate: liveEntryDelegate // applet
        sceneConcealed: false
        geometryCompatibilityProfile: root.freeSurface // applet
            ? "live" : "canonical" // applet
        entryDelegateContext: ({ // applet
            hostKind: root.freeSurface ? "free" : "native", // applet
            vertical: root.vertical // applet
        })
    }

    Component {
        id: liveEntryDelegate // applet

        DockUi.DockEntry {
            anchors.centerIn: parent // applet
            entry: parent.sceneEntry // applet
            entryIndex: parent.sceneIndex // applet
            iconStyleDefinition: // applet
                parent.sceneIconStyleDefinition || ({}) // applet
            iconOverrideResolution: // applet
                parent.sceneEntry.iconOverrideResolution || ({}) // applet
            vertical: root.freeSurface ? false : root.vertical // applet
            baseSize: Number(parent.sceneGeometry.iconSize || root.baseCellSize) // applet
            entryGeometry: parent.sceneGeometry // applet
            magnification: root.magnification // applet
            magnificationEnabled: root.configuration.magnificationEnabled // applet
            magnificationRadius: Number( // applet
                root.configuration.magnificationRadius || 2.4) // applet
            magnificationFalloff: String( // applet
                root.configuration.magnificationFalloff || "linear") // applet
            hoveredIndex: root.hoveredIndex // applet
            tileShape: root.configuration.iconShape // applet
            tileSettings: root.configuration // applet
            appearance: root.configuration.appearance // applet
            showReflection: root.configuration.showReflections // applet
            showIndicator: root.configuration.showIndicators // applet
            showTooltip: root.configuration.showTooltips // applet
            motion: root.configuration.iconAnimation // applet
            motionTrigger: root.configuration.animationTrigger // applet
            motionIntensity: root.configuration.animationIntensity // applet
            motionDuration: root.motionDuration // applet
            motionSpeed: root.motionSpeedScale // applet
            animationProfiles: root.activeAnimationProfiles // applet
            animationCatalog: root.animationCatalogMap // applet
            reducedMotion: root.configuration.reducedMotion // applet
            inputEnabled: parent.sceneInputEnabled // applet
            sceneVisible: parent.sceneVisible // applet
            meshVisualActive: parent.sceneMeshActive // applet
            motionContextKey: parent.sceneMotionContextKey // applet
            editMode: root.plasmaEditMode // applet
            acceptDrops: root.configuration.acceptDrops // applet
            folderExpandOnClick: root.configuration.folderExpandOnClick !== false // applet
            invoke: function() {}
            reorder: function() {}
            pinUrls: function() { return false }
            setHoveredIndex: function(value) { root.hoveredIndex = value } // applet
            setEntryGuard: function() {}
            openPanelStudio: function() {}
            openIconProperties: function() {}
        }
    }
}
