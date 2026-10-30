import QtQuick
import QtQuick.Window
import QtCore
import "WidgetKinds.js" as WidgetKinds

// A Plasma widget beside an Arch Dock that the dock draws as its own
// (WidgetKinds): its icon, or its face from a picture of it taken each
// second. The widget stays where it stands, unseen while it is drawn, still
// taking its clicks and its popup. A bar keeps one for each of its widgets;
// the dock keeps one for each widget beside it in one of Plasma's panels.
Item {
    id: drawn

    // The widget (Plasma's applet) and the item it is drawn as on its panel.
    property var applet: null
    property Item widget: null
    // Whether the dock draws icon widgets, and the faces of the others.
    property bool drawsIcons: false
    property bool drawsFaces: false
    // No pictures are taken while the widget is not shown.
    property bool shown: widget !== null && widget.visible
    // Where the pointer over the widget is followed, if given: the widget's
    // place on its panel.
    property Item hoverArea: null

    readonly property string plugin: applet ? String(applet.pluginName || "") : ""
    readonly property bool iconDrawn: widget !== null && drawsIcons && WidgetKinds.iconWidget(plugin)
    readonly property bool faceDrawn: widget !== null && drawsFaces && WidgetKinds.faceWidget(plugin)
    readonly property bool hovered: hoverWatch !== null && hoverWatch.hovered
    // The last picture of the widget's face, and how many were taken.
    property string facePicture: ""
    property int faceShots: 0
    // A picture was taken or let go, or what the widget shows changed: its
    // icon, its title, its popup opened or closed, the pointer over it.
    signal updated()

    readonly property string faceFolder: String(StandardPaths.writableLocation(StandardPaths.RuntimeLocation))
        .replace(/^file:\/\//, "")
    // The bars and Plasma number their widgets each on their own: the name
    // says whose widget it is.
    readonly property string faceName: "archdock-widget-face-" + String(Qt.application.name || "dock")
        + "-" + (applet && applet.containment ? String(applet.containment.id) : "0")
        + "-" + (applet ? String(applet.id) : "0")

    function takeFace() {
        if (!drawn.faceDrawn || !drawn.widget || !(drawn.widget.width > 0) || !(drawn.widget.height > 0))
            return
        const scale = Math.max(1, Screen.devicePixelRatio)
        const shot = drawn.faceShots + 1
        // Two pictures in turn, the next written while the last is shown.
        const path = drawn.faceFolder + "/" + drawn.faceName + "-" + (shot % 2) + ".png"
        drawn.widget.grabToImage(function(result) {
            if (!drawn.faceDrawn || !result.saveToFile(path))
                return
            drawn.faceShots = shot
            drawn.facePicture = "file://" + path + "?" + shot
            drawn.updated()
        }, Qt.size(Math.ceil(drawn.widget.width * scale), Math.ceil(drawn.widget.height * scale)))
    }
    Timer {
        interval: 1000
        repeat: true
        triggeredOnStart: true
        running: drawn.faceDrawn && drawn.shown
        onTriggered: drawn.takeFace()
    }
    onFaceDrawnChanged: {
        if (faceDrawn)
            return
        facePicture = ""
        updated()
    }

    // Unseen while the dock draws it: an icon widget at once, a face once
    // its first picture is there.
    Binding {
        when: drawn.widget !== null && (drawn.iconDrawn || (drawn.faceDrawn && drawn.facePicture.length > 0))
        target: drawn.widget
        property: "opacity"
        value: 0
        restoreMode: Binding.RestoreBindingOrValue
    }

    Connections {
        target: drawn.applet
        ignoreUnknownSignals: true
        function onIconChanged() { drawn.updated() }
        function onTitleChanged() { drawn.updated() }
    }
    Connections {
        target: drawn.widget
        ignoreUnknownSignals: true
        function onExpandedChanged() { drawn.updated() }
    }

    // The pointer is followed over the widget's place without taking
    // anything from the widget: a hover handler only watches.
    property HoverHandler hoverWatch: null
    Component {
        id: hoverWatchComponent
        HoverHandler {}
    }
    function watchHover() {
        if (hoverWatch)
            hoverWatch.destroy()
        hoverWatch = hoverArea ? hoverWatchComponent.createObject(hoverArea) : null
    }
    onHoverAreaChanged: watchHover()
    onHoveredChanged: updated()
    Component.onCompleted: watchHover()
    Component.onDestruction: {
        if (hoverWatch)
            hoverWatch.destroy()
    }
}
