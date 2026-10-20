import QtQuick
import QtQuick.Window
import org.kde.layershell 1.0 as LayerShell
import ArchDock.Input 1.0

// An edge panel's dock drawn past its panel. Plasma cuts what a panel draws
// at the panel window's edge, so an edge panel whose stands, holograms or
// their light reach past it is drawn in this clear window instead: a strip
// along the same screen edge, as long as the screen and as deep as the dock
// reaches, kept above windows as the panel is. The panel itself stays where
// it is, at its edge, reserving its room; its widget is left as an empty
// placeholder and the dock is laid exactly over it here. The window takes
// the pointer only over the panel and over what the dock shows past it;
// everywhere else the pointer passes through to what lies under it.
Window {
    id: host

    // The screen edge the panel stands on, the placeholder the dock leaves
    // in the panel, and how far past the panel the dock reaches.
    property string edge: "bottom"
    property Item placeholder: null
    property real reach: 0
    // Where the dock shows something past the panel, in the dock's own
    // coordinates (the placeholder's).
    property var shownRects: []
    // Whether the dock is drawn here, and whether it takes the pointer
    // (not while Plasma's own edit mode handles the panel).
    property bool wanted: false
    property bool takesPointer: true

    readonly property var panelWindow: placeholder ? placeholder.Window.window : null
    readonly property bool hosting: wanted && panelWindow !== null && panelWindow.visible
    readonly property bool alongWidth: edge === "top" || edge === "bottom"
    readonly property rect screenRect: screen
        ? Qt.rect(screen.virtualX, screen.virtualY, screen.width, screen.height) : Qt.rect(0, 0, 1920, 1080)
    // The placeholder's place on the screen: the panel window's, which
    // Plasma sets, and the placeholder's within it.
    property point placeholderInPanel: Qt.point(0, 0)
    function placeChanged() {
        if (!placeholder || !panelWindow)
            return
        placeholderInPanel = placeholder.mapToItem(null, 0, 0)
    }
    readonly property real depth: Math.ceil((alongWidth ? (placeholder ? placeholder.height : 0)
        : (placeholder ? placeholder.width : 0)) + Math.max(0, reach))
    // This window's own place on the screen.
    readonly property point origin: edge === "bottom" ? Qt.point(screenRect.x, screenRect.y + screenRect.height - depth)
        : edge === "right" ? Qt.point(screenRect.x + screenRect.width - depth, screenRect.y)
        : Qt.point(screenRect.x, screenRect.y)
    // Where the dock lies in this window: over the placeholder.
    readonly property point dockPlace: panelWindow
        ? Qt.point(panelWindow.x + placeholderInPanel.x - origin.x, panelWindow.y + placeholderInPanel.y - origin.y)
        : Qt.point(0, 0)

    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    // The panel's own screen, found by its name.
    screen: {
        const name = placeholder ? String(placeholder.Screen.name || "") : ""
        const screens = Qt.application.screens
        for (let index = 0; index < screens.length; ++index)
            if (screens[index].name === name) return screens[index]
        return screens.length ? screens[0] : null
    }
    x: origin.x
    y: origin.y
    width: alongWidth ? screenRect.width : Math.max(1, depth)
    height: alongWidth ? Math.max(1, depth) : screenRect.height
    visible: false

    LayerShell.Window.scope: "archdock-dock"
    LayerShell.Window.layer: LayerShell.Window.LayerTop
    LayerShell.Window.anchors: edge === "top" ? (LayerShell.Window.AnchorTop | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight)
        : edge === "left" ? (LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom)
        : edge === "right" ? (LayerShell.Window.AnchorRight | LayerShell.Window.AnchorTop | LayerShell.Window.AnchorBottom)
        : (LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft | LayerShell.Window.AnchorRight)
    // Over the panel, not beside it: the panel's own reserved room is not
    // this window's to keep clear of.
    LayerShell.Window.exclusionZone: -1
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone

    // The pointer is taken over the panel and over what the dock shows past
    // it.
    WindowInputShape {
        window: host
        enabled: host.hosting
        rects: {
            if (!host.takesPointer)
                return []
            const place = host.dockPlace
            const own = host.placeholder
                ? [Qt.rect(place.x, place.y, host.placeholder.width, host.placeholder.height)] : []
            return own.concat((host.shownRects || []).map(function(rect) {
                return Qt.rect(place.x + rect.x, place.y + rect.y, rect.width, rect.height)
            }))
        }
    }

    onHostingChanged: {
        placeChanged()
        visible = hosting
    }
    Component.onCompleted: {
        placeChanged()
        visible = hosting
    }
    Connections {
        target: host.placeholder
        function onWidthChanged() { host.placeChanged() }
        function onHeightChanged() { host.placeChanged() }
        function onXChanged() { host.placeChanged() }
        function onYChanged() { host.placeChanged() }
    }
    Connections {
        target: host.panelWindow
        function onWidthChanged() { host.placeChanged() }
        function onHeightChanged() { host.placeChanged() }
        function onXChanged() { host.placeChanged() }
        function onYChanged() { host.placeChanged() }
    }
}
