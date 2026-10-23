// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.extras as PlasmaExtras
import org.kde.plasma.plasmoid

// An Arch Dock bar: the widgets it holds in a row along its screen edge (a
// column on a side edge). The bar host sizes the bar's window, as long as
// this row wants to be or as long as the owner set.
ContainmentItem {
    id: root

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical
    // The widgets' applet ids in their order along the bar, as the bar host
    // writes it from the bar's record.
    readonly property var appletOrder: String(Plasmoid.configuration.AppletOrder || "")
        .split(";").filter(id => id.length > 0)
    property int arrivals: 0
    // The dock draws the bar's panel and says where inside it the widgets
    // stand: so far in from the bar's ends and from its long sides. The
    // widget at each end keeps that room there; the dock keeps the room it
    // asks for itself.
    readonly property Item dockSlot: {
        for (const child of row.children)
            if (child.dock)
                return child
        return null
    }
    readonly property Item dockApplet: dockSlot ? dockSlot.applet : null
    // The free room each widget keeps on its side away from the dock, so it
    // stands where it was dropped along the bar.
    readonly property var appletGaps: {
        const gaps = {}
        for (const entry of String(Plasmoid.configuration.AppletGaps || "").split(";")) {
            const parts = entry.split(":")
            if (parts.length === 2 && Number(parts[1]) > 0)
                gaps[parts[0]] = Number(parts[1])
        }
        return gaps
    }
    readonly property var widgetRoom: dockApplet && dockApplet.barWidgetRoom ? dockApplet.barWidgetRoom
        : ({ start: 0, end: 0, sideStart: 0, sideEnd: 0 })
    readonly property var dockRoom: dockApplet && dockApplet.barDockRoom ? dockApplet.barDockRoom
        : ({ start: 0, end: 0 })
    // The panel's ends: its start is the bar's application menu and its end
    // the system tray's button for the icons it hides (^), in any look.
    readonly property var endCaps: dockApplet && dockApplet.barEndCaps ? dockApplet.barEndCaps
        : ({ start: 0, end: 0 })
    readonly property var launcherPlugins: ["org.kde.plasma.kickoff", "org.kde.plasma.kicker",
        "org.kde.plasma.kickerdash"]
    function slotWith(plugins) {
        let found = null
        for (const child of row.children)
            if (child.plasmaApplet && plugins.includes(String(child.plasmaApplet.pluginName))
                    && (!found || child.place < found.place))
                found = child
        return found
    }
    // The application menu on the look's start: a launcher first in the
    // bar's order. Moved further along the bar, it is one of its widgets.
    readonly property Item launcherSlot: {
        if (endCaps.start <= 0)
            return null
        let first = null
        for (const child of row.children)
            if (child.plasmaApplet && (!first || child.place < first.place))
                first = child
        return first && launcherPlugins.includes(String(first.plasmaApplet.pluginName)) ? first : null
    }
    // Edit mode: the strip after a widget that holds its buttons.
    readonly property real editStrip: editing ? 22 : 0
    readonly property Item traySlot: endCaps.end > 0 ? slotWith(["org.kde.plasma.systemtray"]) : null
    // The first and last widgets along the row; the menu on the look's
    // start is out of it.
    function endSlot(last) {
        let found = null
        for (const child of row.children)
            if (child.place !== undefined && child !== launcherSlot
                    && (!found || (last ? child.place > found.place : child.place < found.place)))
                found = child
        return found
    }
    readonly property Item firstSlot: endSlot(false)
    readonly property Item lastSlot: endSlot(true)
    // The widgets' text and symbols: light on a dark panel and dark on a
    // light one, as Plasma's own panels are drawn, in the desktop colour
    // scheme's own light and dark (its complementary colours, the ones
    // Plasma puts on dark panels).
    readonly property real panelLightness: dockApplet && dockApplet.barSurfaceLightness !== undefined
        ? Number(dockApplet.barSurfaceLightness) : -1
    readonly property bool panelDark: panelLightness >= 0 && panelLightness < 0.42
    Item {
        id: complementary
        Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
        Kirigami.Theme.inherit: false
    }
    Kirigami.Theme.inherit: false
    Kirigami.Theme.textColor: panelDark ? complementary.Kirigami.Theme.textColor
        : complementary.Kirigami.Theme.backgroundColor
    Kirigami.Theme.disabledTextColor: panelDark ? complementary.Kirigami.Theme.disabledTextColor
        : Qt.tint(complementary.Kirigami.Theme.backgroundColor, "#80808080")

    // Edit mode: a button in a widget's strip, its remove (✕) or settings.
    component EditButton: Rectangle {
        id: button
        property alias iconName: buttonIcon.source
        signal clicked()
        z: 1001
        width: 18
        height: 18
        radius: 9
        Kirigami.Icon {
            id: buttonIcon
            anchors.fill: parent
            anchors.margins: 3
            isMask: true
            color: "white"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: button.clicked()
        }
    }

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    GridLayout {
        id: row
        anchors.fill: parent
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rowSpacing: 0
        columnSpacing: 0

        // Edit mode: the place of the bar's + after its widgets, when its
        // window has no room for it outside the bar.
        Item {
            id: addSpot
            visible: root.editing && !root.addOutside
            Layout.column: root.vertical ? 0 : 9999
            Layout.row: root.vertical ? 9999 : 0
            Layout.preferredWidth: root.vertical ? -1 : 32
            Layout.preferredHeight: root.vertical ? 32 : -1
            Layout.fillWidth: root.vertical
            Layout.fillHeight: !root.vertical
        }
    }

    // Edit mode: + outside the bar, past its end (past its start where the
    // edge has no room after it), opens what Plasma's panels offer with
    // their Add New: a spacer or a margins separator at the bar's end
    // (Plasma's widget window lists neither), or the bar's widget window,
    // whose widgets are dragged onto the bar.
    readonly property real roomOutsideBefore: parent && parent.editRoomBefore !== undefined ? Number(parent.editRoomBefore) : 0
    readonly property real roomOutsideAfter: parent && parent.editRoomAfter !== undefined ? Number(parent.editRoomAfter) : 0
    readonly property bool addOutside: roomOutsideBefore > 0 || roomOutsideAfter > 0
    Rectangle {
        id: addButton
        z: 1003
        visible: root.editing
        width: 24
        height: 24
        radius: 12
        readonly property real along: root.roomOutsideAfter > 0
            ? (root.vertical ? root.height : root.width) + (root.roomOutsideAfter - width) / 2
            : root.roomOutsideBefore > 0 ? -(root.roomOutsideBefore + width) / 2
            : root.vertical ? addSpot.y + (addSpot.height - height) / 2 : addSpot.x + (addSpot.width - width) / 2
        x: root.vertical ? (root.width - width) / 2 : along
        y: root.vertical ? along : (root.height - height) / 2
        color: Kirigami.Theme.positiveTextColor
        Kirigami.Icon {
            anchors.fill: parent
            anchors.margins: 4
            source: "list-add"
            isMask: true
            color: "white"
        }
        MouseArea {
            anchors.fill: parent
            onClicked: addMenu.status === PlasmaExtras.Menu.Open ? addMenu.close() : addMenu.openRelative()
        }
    }
    PlasmaExtras.Menu {
        id: addMenu
        visualParent: addButton
        placement: Plasmoid.location === PlasmaCore.Types.TopEdge ? PlasmaExtras.Menu.BottomPosedRightAlignedPopup
            : Plasmoid.location === PlasmaCore.Types.LeftEdge ? PlasmaExtras.Menu.RightPosedBottomAlignedPopup
            : Plasmoid.location === PlasmaCore.Types.RightEdge ? PlasmaExtras.Menu.LeftPosedBottomAlignedPopup
            : PlasmaExtras.Menu.TopPosedRightAlignedPopup
        PlasmaExtras.MenuItem {
            text: qsTr("Spacer")
            icon: "distribute-horizontal-x"
            onClicked: root.addAtEnd("org.kde.plasma.panelspacer")
        }
        PlasmaExtras.MenuItem {
            text: qsTr("Margins Separator")
            icon: "distribute-horizontal-margin"
            onClicked: root.addAtEnd("org.kde.plasma.marginsseparator")
        }
        PlasmaExtras.MenuItem {
            text: qsTr("Widgets…")
            icon: "view-group-symbolic"
            onClicked: root.toggleWidgetWindow()
        }
    }
    function addAtEnd(plugin) {
        if (Plasmoid.corona && typeof Plasmoid.corona.dropWidget === "function")
            Plasmoid.corona.dropWidget(Plasmoid, plugin, root.appletOrder.length)
    }

    // The look's end pieces are the bar's application menu and the system
    // tray's button for the icons it hides (^): each stands in the middle of
    // its piece, half as large as the bar is deep, the menu with its own
    // icon, the tray's button the tray's own.
    readonly property real capButton: Math.round(Math.min(root.vertical ? root.width : root.height,
        Math.max(endCaps.start, endCaps.end)) / 2)
    Item {
        id: startCap
        visible: root.launcherSlot !== null
        width: root.capButton
        height: root.capButton
        x: root.vertical ? (root.width - width) / 2 : (root.endCaps.start - width) / 2
        y: root.vertical ? (root.endCaps.start - height) / 2 : (root.height - height) / 2
        z: root.movingSlot !== null && root.movingSlot === root.launcherSlot ? 10 : 0
        transform: Translate {
            x: root.vertical || !root.launcherSlot ? 0 : root.launcherSlot.dragShift
            y: root.vertical && root.launcherSlot ? root.launcherSlot.dragShift : 0
        }
        // Edit mode: the menu is dragged along the bar like any widget.
        MouseArea {
            anchors.fill: parent
            z: 1000
            enabled: root.editing && root.launcherSlot !== null
            acceptedButtons: Qt.LeftButton
            cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor
            onPressed: function(mouse) { root.beginMove(root.launcherSlot, mapToItem(root, mouse.x, mouse.y)) }
            onPositionChanged: function(mouse) { root.dragMove(root.launcherSlot, mapToItem(root, mouse.x, mouse.y)) }
            onReleased: function(mouse) { root.endMove(root.launcherSlot, mapToItem(root, mouse.x, mouse.y)) }
            onCanceled: root.cancelMove()
        }
    }
    // Edit mode: the menu's settings and remove buttons, in the strip
    // right after the start piece.
    EditButton {
        visible: root.editing && root.launcherSlot !== null && root.launcherSlot.settingsAction !== null
            && root.launcherSlot.settingsAction.enabled
        x: root.vertical ? root.width / 2 + 2 : root.endCaps.start + 2
        y: root.vertical ? root.endCaps.start + 2 : root.height / 2 + 2
        color: Kirigami.Theme.highlightColor
        iconName: "configure"
        onClicked: root.launcherSlot.settingsAction.trigger()
    }
    EditButton {
        visible: root.editing && root.launcherSlot !== null
        x: root.vertical ? root.width / 2 - 20 : root.endCaps.start + 2
        y: root.vertical ? root.endCaps.start + 2 : root.height / 2 - 20
        color: Kirigami.Theme.negativeTextColor
        iconName: "window-close"
        onClicked: root.removeSlot(root.launcherSlot)
    }
    Item {
        id: endCap
        visible: root.trayApplet !== null
        width: root.capButton
        height: root.capButton
        x: root.vertical ? (root.width - width) / 2 : root.width - (root.endCaps.end + width) / 2
        y: root.vertical ? root.height - (root.endCaps.end + height) / 2 : (root.height - height) / 2
    }
    function seat(slot) {
        const item = slot.applet
        if (!item)
            return
        item.parent = root.launcherSlot === slot ? startCap : slot
        item.anchors.fill = item.parent
    }
    // Plasma's system tray draws its button as an ExpanderArrow among its
    // icons; it is moved onto the end piece and back. The tray builds it
    // after it loads, so it is looked for a few times.
    property Item trayExpander: null
    property Item trayExpanderHome: null
    function findExpander(item) {
        for (const child of item ? item.children : []) {
            if (String(child).includes("ExpanderArrow"))
                return child
            const found = findExpander(child)
            if (found)
                return found
        }
        return null
    }
    function seatTrayExpander() {
        if (!trayExpander)
            return
        const onCap = root.trayApplet !== null
        trayExpander.anchors.centerIn = undefined
        trayExpander.parent = onCap ? endCap : trayExpanderHome
        if (onCap) {
            // As large as the tray's own icons, in the middle of the end.
            const size = Math.min(endCap.width, Number(trayExpander.iconSize || endCap.width))
            trayExpander.width = size
            trayExpander.height = size
            trayExpander.anchors.centerIn = endCap
        }
    }
    readonly property Item trayApplet: traySlot ? traySlot.applet : null
    onTrayAppletChanged: {
        if (trayExpander && trayApplet === null) {
            seatTrayExpander()
            trayExpander = null
        }
        expanderSearch.tries = 0
        expanderSearch.running = trayApplet !== null && trayExpander === null
    }
    Timer {
        id: expanderSearch
        property int tries: 0
        interval: 500
        repeat: true
        onTriggered: {
            const found = root.findExpander(root.trayApplet)
            if (found) {
                root.trayExpander = found
                root.trayExpanderHome = found.parent
                root.seatTrayExpander()
                running = false
            } else if (++tries >= 10) {
                running = false
                console.warn("Arch Dock bar: the system tray's button for its hidden icons was not found")
            }
        }
    }

    // A widget's place in the bar: as long along the bar as the widget asks,
    // and as thick across it as the panel's inside whatever the widget's own
    // minimum there, as a Plasma panel holds its widgets. Arch Dock's dock
    // takes the bar's whole length and centres its drawing in it.
    Component {
        id: appletSlot

        Item {
            id: slot

            property var plasmaApplet: null
            property Item applet: null
            property bool dock: false
            // The dock draws the bar's look, the panel the bar's widgets and
            // its own icons stand inside, along the whole bar and as deep as
            // it: it is told how far the bar reaches before and after it, and
            // how deep the bar is, and lies under the other widgets.
            z: dock ? -1 : root.movingSlot === slot ? 10 : 0
            // Edit mode: a widget is dragged along the bar, following the
            // pointer as on Plasma's panels, and stands where it is let go;
            // its own menu (the right button) stays.
            property real dragShift: 0
            transform: Translate {
                x: root.vertical ? 0 : slot.dragShift
                y: root.vertical ? slot.dragShift : 0
            }
            MouseArea {
                anchors.fill: parent
                z: 1000
                enabled: root.editing && !slot.dock
                acceptedButtons: Qt.LeftButton
                cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor
                onPressed: function(mouse) { root.beginMove(slot, mapToItem(root, mouse.x, mouse.y)) }
                onPositionChanged: function(mouse) { root.dragMove(slot, mapToItem(root, mouse.x, mouse.y)) }
                onReleased: function(mouse) { root.endMove(slot, mapToItem(root, mouse.x, mouse.y)) }
                onCanceled: root.cancelMove()
            }
            // Edit mode: a spacer shows where it is, and its end is dragged to
            // give it its own size (it then stops filling the bar), as on
            // Plasma's own panels.
            readonly property bool spacer: plasmaApplet !== null && plasmaApplet.pluginName === "org.kde.plasma.panelspacer"
            Rectangle {
                anchors.fill: parent
                z: 999
                visible: root.editing && slot.spacer
                color: Qt.alpha(Kirigami.Theme.highlightColor, 0.25)
                border.color: Kirigami.Theme.highlightColor
                border.width: 1
                radius: 3
            }
            MouseArea {
                z: 1002
                visible: root.editing && slot.spacer
                width: root.vertical ? parent.width : 10
                height: root.vertical ? 10 : parent.height
                x: root.vertical ? 0 : parent.width - width
                y: root.vertical ? parent.height - height : 0
                cursorShape: root.vertical ? Qt.SizeVerCursor : Qt.SizeHorCursor
                property real startAlong: 0
                property real startLength: 0
                onPressed: function(mouse) {
                    const point = mapToItem(root, mouse.x, mouse.y)
                    startAlong = root.vertical ? point.y : point.x
                    startLength = root.vertical ? slot.height : slot.width
                }
                onPositionChanged: function(mouse) {
                    const point = mapToItem(root, mouse.x, mouse.y)
                    const settings = slot.plasmaApplet.configuration
                    settings.expanding = false
                    settings.length = Math.max(4, Math.round(startLength + (root.vertical ? point.y : point.x) - startAlong))
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: root.vertical ? parent.width * 0.6 : 4
                    height: root.vertical ? 4 : parent.height * 0.6
                    radius: 2
                    color: Kirigami.Theme.highlightColor
                }
            }
            // Edit mode: opens the widget's own settings (a clock's format,
            // a launcher's icon), as its menu would.
            readonly property QtObject settingsAction: plasmaApplet ? plasmaApplet.internalAction("configure") : null
            // In the widget's strip, below its remove button (beside it on a
            // side bar).
            EditButton {
                visible: root.editing && !slot.dock && slot.settingsAction !== null && slot.settingsAction.enabled
                x: root.vertical ? parent.width / 2 + 2 : parent.width + 2
                y: root.vertical ? parent.height + 2 : parent.height / 2 + 2
                color: Kirigami.Theme.highlightColor
                iconName: "configure"
                onClicked: slot.settingsAction.trigger()
            }
            // Edit mode: takes the widget out of the bar. In the widget's
            // strip, above its settings button (before it on a side bar).
            EditButton {
                visible: root.editing && !slot.dock
                x: root.vertical ? parent.width / 2 - 20 : parent.width + 2
                y: root.vertical ? parent.height + 2 : parent.height / 2 - 20
                color: Kirigami.Theme.negativeTextColor
                iconName: "window-close"
                onClicked: root.removeSlot(slot)
            }
            Binding {
                when: slot.dock && slot.applet !== null
                target: slot.applet
                property: "barSurfaceBefore"
                value: root.vertical ? slot.y : slot.x
                restoreMode: Binding.RestoreNone
            }
            Binding {
                when: slot.dock && slot.applet !== null
                target: slot.applet
                property: "barSurfaceAfter"
                value: root.vertical ? row.height - slot.y - slot.height : row.width - slot.x - slot.width
                restoreMode: Binding.RestoreNone
            }
            Binding {
                when: slot.dock && slot.applet !== null
                target: slot.applet
                property: "barSurfaceThickness"
                value: root.vertical ? row.width : row.height
                restoreMode: Binding.RestoreNone
            }
            // Its place along the bar: where the bar's order puts it, else
            // after every widget the order names, in the order they came.
            property int arrival: 0
            readonly property int place: {
                const index = plasmaApplet ? root.appletOrder.indexOf(String(plasmaApplet.id)) : -1
                return index >= 0 ? index : root.appletOrder.length + arrival
            }
            Layout.column: root.vertical ? 0 : place
            Layout.row: root.vertical ? place : 0
            // The application menu on the look's start is out of the row.
            visible: root.launcherSlot !== slot
            onVisibleChanged: root.seat(slot)
            // Something stands before the dock: a widget, or the menu on the
            // look's start. Its icons then start right after it.
            readonly property bool widgetBefore: root.firstSlot !== slot || root.launcherSlot !== null
            Binding {
                when: slot.dock && slot.applet !== null
                target: slot.applet
                property: "barWidgetBefore"
                value: slot.widgetBefore
                restoreMode: Binding.RestoreNone
            }
            // Inside the panel the dock draws: clear of its ends, and a
            // widget clear of its sides. A dock first along the bar keeps the
            // room it asks for itself.
            readonly property var room: dock && !widgetBefore ? root.dockRoom : root.widgetRoom
            // The menu and the tray's button on the panel's ends keep their
            // room there.
            readonly property real roomBefore: root.firstSlot !== slot ? 0
                : Math.max(Number(room.start || 0), root.launcherSlot ? Number(root.endCaps.start || 0) + root.editStrip : 0)
            readonly property real roomAfter: root.lastSlot !== slot ? 0
                : Math.max(Number(room.end || 0), root.trayApplet ? Number(root.endCaps.end || 0) : 0)
            readonly property real roomSideStart: dock ? 0 : Number(room.sideStart || 0)
            readonly property real roomSideEnd: dock ? 0 : Number(room.sideEnd || 0)
            // Edit mode: a strip beside each widget (after it along the
            // bar) holds its buttons, so they never cover the widget.
            readonly property real editRoom: dock ? 0 : root.editStrip
            // The free room it keeps where it was dropped: before it ahead of
            // the dock, after it behind the dock, which takes what is left.
            readonly property bool behindDock: !dock && root.dockSlot !== null && place > root.dockSlot.place
            readonly property real gap: dock || !plasmaApplet ? 0 : Number(root.appletGaps[String(plasmaApplet.id)] || 0)
            readonly property real lead: roomBefore + (behindDock ? 0 : gap)
            readonly property real trail: roomAfter + editRoom + (behindDock ? gap : 0)
            Layout.leftMargin: root.vertical ? roomSideStart : lead
            Layout.rightMargin: root.vertical ? roomSideEnd : trail
            Layout.topMargin: root.vertical ? lead : roomSideStart
            Layout.bottomMargin: root.vertical ? trail : roomSideEnd
            readonly property var hints: applet ? applet.Layout : null
            readonly property real minimumAlong: !hints || dock ? 0
                : Math.max(0, root.vertical ? hints.minimumHeight : hints.minimumWidth)
            readonly property real preferredAlong: {
                if (!hints)
                    return 0
                const preferred = root.vertical ? hints.preferredHeight : hints.preferredWidth
                const own = root.vertical ? applet.implicitHeight : applet.implicitWidth
                const minimum = root.vertical ? hints.minimumHeight : hints.minimumWidth
                // A widget that fills (a flexible spacer) takes the room the
                // bar has, as on Plasma's panels: it never makes the bar longer.
                if (!dock && fillsAlong)
                    return Math.max(0, minimum)
                return Math.max(0, minimum, preferred >= 0 ? preferred : own)
            }
            readonly property real maximumAlong: !hints || dock ? Infinity
                : (root.vertical ? hints.maximumHeight : hints.maximumWidth)
            readonly property bool fillsAlong: dock
                || (hints !== null && (root.vertical ? hints.fillHeight : hints.fillWidth))

            Layout.minimumWidth: root.vertical ? 0 : minimumAlong
            Layout.preferredWidth: root.vertical ? -1 : preferredAlong
            Layout.maximumWidth: root.vertical ? Infinity : maximumAlong
            Layout.fillWidth: root.vertical || fillsAlong
            Layout.minimumHeight: root.vertical ? minimumAlong : 0
            Layout.preferredHeight: root.vertical ? preferredAlong : -1
            Layout.maximumHeight: root.vertical ? maximumAlong : Infinity
            Layout.fillHeight: !root.vertical || fillsAlong
        }
    }

    function slotOf(applet) {
        for (const child of row.children)
            if (child.plasmaApplet === applet)
                return child
        return null
    }

    function place(applet) {
        const item = root.itemFor(applet)
        if (!item || root.slotOf(applet))
            return
        const slot = appletSlot.createObject(row, {
            plasmaApplet: applet,
            applet: item,
            dock: applet.pluginName === "org.archdock.dock",
            arrival: root.arrivals++
        })
        item.parent = slot
        item.anchors.fill = slot
        item.visible = true
        root.seat(slot)
        if (root.droppedAt && String(applet.pluginName) === root.droppedAt.plugin)
            droppedSettle.restart()
    }

    // The bar's widget window: Plasma's own widget explorer, opened for this
    // bar by its + or its menu's Add Widgets. Arch Dock keeps the bars clear
    // of it while it shows.
    function toggleWidgetWindow() {
        if (widgetWindow.item) {
            widgetWindow.item.close()
            return
        }
        if (!Plasmoid.corona || typeof Plasmoid.corona.widgetExplorerUrl !== "function")
            return
        widgetWindow.setSource(Qt.resolvedUrl("WidgetWindow.qml"), {
            containment: Plasmoid,
            explorerUrl: Plasmoid.corona.widgetExplorerUrl(),
            screenHeight: Screen.height
        })
    }
    Loader {
        id: widgetWindow
        readonly property rect area: item && item.shown
            ? Qt.rect(Screen.virtualX, Screen.virtualY, item.shownWidth, Screen.height) : Qt.rect(0, 0, 0, 0)
        onAreaChanged: {
            if (Plasmoid.corona && typeof Plasmoid.corona.setWidgetWindow === "function")
                Plasmoid.corona.setWidgetWindow(area)
        }
        Connections {
            target: widgetWindow.item
            function onClosed() { Qt.callLater(() => widgetWindow.source = "") }
        }
    }

    Connections {
        target: Plasmoid
        function onShowAddWidgetsInterface(pos) {
            if (!widgetWindow.item)
                root.toggleWidgetWindow()
        }
        function onAppletAdded(applet, geometry) { root.place(applet) }
        function onAppletRemoved(applet) {
            const slot = root.slotOf(applet)
            if (slot)
                slot.destroy()
        }
    }
    Component.onCompleted: {
        for (const applet of Plasmoid.applets)
            root.place(applet)
    }

    // Edit mode, the bars' own, entered and left from a bar's menu: the bar
    // is outlined, its widgets are dragged along it, and a widget dragged
    // from Plasma's widget window is dropped onto it.
    readonly property bool editing: Plasmoid.corona ? Plasmoid.corona.editMode : false
    // Where a widget dropped at `point` goes: how many of the bar's widgets
    // stand before it, and where along the bar the line between them is.
    function dropPlace(point) {
        const along = root.vertical ? point.y : point.x
        const spans = []
        for (const child of row.children) {
            if (!child.plasmaApplet)
                continue
            if (child === root.launcherSlot) {
                spans.push({ slot: child, start: -Infinity, end: root.vertical ? startCap.y + startCap.height : startCap.x + startCap.width })
                continue
            }
            if (!child.visible)
                continue
            const corner = child.mapToItem(root, 0, 0)
            const start = root.vertical ? corner.y : corner.x
            spans.push({ slot: child, start: start, end: start + (root.vertical ? child.height : child.width) })
        }
        spans.sort((a, b) => a.start - b.start)
        let index = 0
        while (index < spans.length && (spans[index].start + spans[index].end) / 2 < along)
            ++index
        const before = index > 0 ? spans[index - 1].end : 0
        const after = index < spans.length ? spans[index].start : (root.vertical ? root.height : root.width)
        // The place in the bar's record: the record's order is not the
        // drawn one (the application menu is drawn on the start cap
        // wherever it stands), so it is the record's place of the widget
        // drawn next, or the record's end.
        const next = index < spans.length ? root.appletOrder.indexOf(String(spans[index].slot.plasmaApplet.id)) : -1
        return { index: next >= 0 ? next : root.appletOrder.length,
                 at: index === 0 && spans.length > 0 && isFinite(spans[0].start)
                     ? spans[0].start : index === spans.length ? before : (before + after) / 2 }
    }

    // How far in from the bar's ends its widgets stand: clear of the menu
    // and the tray's button on the panel's ends.
    readonly property real alongStart: Math.max(Number(widgetRoom.start || 0),
        launcherSlot ? Number(endCaps.start || 0) + editStrip : 0)
    readonly property real alongEnd: (vertical ? row.height : row.width)
        - Math.max(Number(widgetRoom.end || 0), trayApplet ? Number(endCaps.end || 0) : 0)
    // The bar's widgets but `moving` where they stand along it, each with
    // its edit strip, and the dock as far as its icons reach.
    function drawnSpans(moving) {
        const spans = []
        for (const child of row.children) {
            if (!child.plasmaApplet || child === moving || child === root.launcherSlot || !child.visible)
                continue
            const start = root.vertical ? child.y : child.x
            const length = child.dock ? child.preferredAlong
                : (root.vertical ? child.height : child.width) + child.editRoom
            spans.push({ slot: child, start: start, end: start + length })
        }
        return spans.sort((a, b) => a.start - b.start)
    }
    // Where a widget moved `shift` along the bar lands, the pointer at
    // `pointer`. On free room it stands just there and every other widget
    // stays where it is. Over another widget or the dock's icons it goes
    // next to that one, before or after it by the half the pointer is over,
    // as on Plasma's panels. Where it does not fit, it goes in between and
    // the others make room.
    property Item movingSlot: null
    property var movePlan: null
    property real pressedAt: 0
    function along(point) { return root.vertical ? point.y : point.x }
    function beginMove(slot, point) {
        slot.dragShift = 0
        root.pressedAt = root.along(point)
        root.movingSlot = slot
        root.movePlan = null
    }
    function dragMove(slot, point) {
        if (root.movingSlot !== slot)
            return
        slot.dragShift = root.along(point) - root.pressedAt
        root.movePlan = root.planMove(slot, slot.dragShift, root.along(point))
    }
    function endMove(slot, point) {
        if (root.movingSlot !== slot)
            return
        const shift = root.along(point) - root.pressedAt
        root.cancelMove()
        if (Math.abs(shift) >= Qt.styleHints.startDragDistance)
            root.applyPlan(slot, root.planMove(slot, shift, root.along(point)))
    }
    function cancelMove() {
        if (root.movingSlot)
            root.movingSlot.dragShift = 0
        root.movingSlot = null
        root.movePlan = null
    }
    function removeSlot(slot) {
        if (!slot || !Plasmoid.corona || typeof Plasmoid.corona.removeWidget !== "function")
            return
        const id = String(slot.plasmaApplet.id)
        Plasmoid.configuration.AppletGaps = String(Plasmoid.configuration.AppletGaps || "").split(";")
            .filter(entry => entry.length > 0 && entry.split(":")[0] !== id).join(";")
        Plasmoid.configuration.writeConfig()
        Plasmoid.corona.removeWidget(Plasmoid, slot.plasmaApplet)
    }
    function planMove(moving, shift, pointer) {
        const spans = root.drawnSpans(moving)
        // The menu on the start piece takes its own length once it stands
        // among the widgets.
        const onCap = moving === root.launcherSlot
        const size = (onCap ? moving.preferredAlong : root.vertical ? moving.height : moving.width) + moving.editRoom
        const fits = start => start >= root.alongStart - 0.5 && start + size <= root.alongEnd + 0.5
            && !spans.some(span => start < span.end - 0.5 && start + size > span.start + 0.5)
        const under = spans.find(span => pointer >= span.start && pointer < span.end)
        let start = (onCap ? root.along(Qt.point(startCap.x, startCap.y)) : root.vertical ? moving.y : moving.x) + shift
        if (under) {
            start = pointer < (under.start + under.end) / 2 ? under.start - size : under.end
        } else {
            let before = root.alongStart, after = root.alongEnd
            for (const span of spans) {
                if (span.end <= pointer)
                    before = Math.max(before, span.end)
                else if (span.start >= pointer)
                    after = Math.min(after, span.start)
            }
            start = Math.max(before, Math.min(after - size, start))
        }
        if (fits(start)) {
            // Each widget's free room from where they all stand: ahead of the
            // dock to what is before it, behind the dock to what is after it.
            const sequence = spans.concat([{ slot: moving, start: start, end: start + size }])
                .sort((a, b) => a.start - b.start)
            const dock = sequence.findIndex(span => span.slot.dock)
            const gaps = {}
            sequence.forEach((span, k) => {
                if (span.slot.dock)
                    return
                const gap = dock >= 0 && k > dock
                    ? (k + 1 < sequence.length ? sequence[k + 1].start : root.alongEnd) - span.end
                    : span.start - (k > 0 ? sequence[k - 1].end : root.alongStart)
                gaps[String(span.slot.plasmaApplet.id)] = Math.max(0, Math.round(gap))
            })
            return { free: true, sequence: sequence.map(span => span.slot), gaps: gaps,
                     at: start, length: size - moving.editRoom }
        }
        let index = spans.findIndex(span => pointer < (span.start + span.end) / 2)
        if (index < 0)
            index = spans.length
        const sequence = spans.map(span => span.slot)
        sequence.splice(index, 0, moving)
        const gaps = Object.assign({}, root.appletGaps)
        gaps[String(moving.plasmaApplet.id)] = 0
        return { free: false, sequence: sequence, gaps: gaps,
                 at: index < spans.length ? spans[index].start
                     : spans.length > 0 ? spans[spans.length - 1].end : root.alongStart }
    }
    // Keeps where a moved widget landed: every widget's free room, and the
    // widget's place in the bar's record, before the record's place of the
    // widget drawn next (the record holds the application menu drawn on
    // the start cap too).
    function applyPlan(moving, plan) {
        if (!plan || !moving.plasmaApplet)
            return
        const entries = []
        for (const slot of plan.sequence) {
            const id = String(slot.plasmaApplet.id)
            if (Number(plan.gaps[id] || 0) > 0)
                entries.push(id + ":" + Math.round(plan.gaps[id]))
        }
        Plasmoid.configuration.AppletGaps = entries.join(";")
        Plasmoid.configuration.writeConfig()
        const id = String(moving.plasmaApplet.id)
        const position = plan.sequence.indexOf(moving)
        const next = position + 1 < plan.sequence.length ? plan.sequence[position + 1] : null
        const order = root.appletOrder.filter(other => String(other) !== id)
        const nextIndex = next ? order.indexOf(String(next.plasmaApplet.id)) : -1
        const index = nextIndex >= 0 ? nextIndex : order.length
        if (root.appletOrder.indexOf(id) !== index && Plasmoid.corona && typeof Plasmoid.corona.moveWidget === "function")
            Plasmoid.corona.moveWidget(Plasmoid, moving.plasmaApplet, index)
    }
    // A widget dropped from Plasma's widget window stands where it was let
    // go, once Arch Dock has added it to the bar.
    property var droppedAt: null
    Timer {
        id: droppedSettle
        interval: 600
        onTriggered: {
            const drop = root.droppedAt
            root.droppedAt = null
            let slot = null
            for (const child of row.children)
                if (drop && child.plasmaApplet && child.arrival >= drop.arrivals && child !== root.launcherSlot
                        && String(child.plasmaApplet.pluginName) === drop.plugin)
                    slot = child
            if (!slot)
                return
            const length = root.vertical ? slot.height : slot.width
            const from = root.vertical ? slot.y : slot.x
            root.applyPlan(slot, root.planMove(slot, drop.along - length / 2 - from, drop.along))
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 1000
        visible: root.editing || widgetDrop.containsDrag
        enabled: false
        radius: 4
        color: Qt.alpha(Kirigami.Theme.highlightColor, widgetDrop.containsDrag ? 0.2 : 0.08)
        border.color: Kirigami.Theme.highlightColor
        border.width: 2
    }
    // Where a widget goes: a line between two widgets, or, on free room,
    // the place it will stand in.
    readonly property bool landsFree: movePlan !== null && movePlan.free
    Rectangle {
        z: 1001
        visible: widgetDrop.containsDrag || (root.movePlan !== null && !root.movePlan.free)
        enabled: false
        color: Kirigami.Theme.highlightColor
        readonly property real at: root.movePlan !== null ? root.movePlan.at : widgetDrop.lineAt
        width: root.vertical ? root.width - 8 : 3
        height: root.vertical ? 3 : root.height - 8
        x: root.vertical ? 4 : at - width / 2
        y: root.vertical ? at - height / 2 : 4
    }
    Rectangle {
        z: 1001
        visible: root.landsFree
        enabled: false
        radius: 3
        color: Qt.alpha(Kirigami.Theme.highlightColor, 0.18)
        border.color: Kirigami.Theme.highlightColor
        border.width: 1
        x: root.vertical ? 4 : root.landsFree ? root.movePlan.at : 0
        y: root.vertical ? (root.landsFree ? root.movePlan.at : 0) : 4
        width: root.vertical ? root.width - 8 : root.landsFree ? root.movePlan.length : 0
        height: root.vertical ? (root.landsFree ? root.movePlan.length : 0) : root.height - 8
    }
    DropArea {
        id: widgetDrop
        anchors.fill: parent
        z: 1002
        keys: ["text/x-plasmoidservicename"]
        property real lineAt: 0
        onEntered: function(drag) { lineAt = root.dropPlace(Qt.point(drag.x, drag.y)).at }
        onPositionChanged: function(drag) { lineAt = root.dropPlace(Qt.point(drag.x, drag.y)).at }
        onDropped: function(drop) {
            const plugin = String(drop.getDataAsString("text/x-plasmoidservicename") || "").trim()
            if (!plugin.length || !Plasmoid.corona || typeof Plasmoid.corona.dropWidget !== "function")
                return
            root.droppedAt = { plugin: plugin, along: root.vertical ? drop.y : drop.x, arrivals: root.arrivals }
            Plasmoid.corona.dropWidget(Plasmoid, plugin, root.dropPlace(Qt.point(drop.x, drop.y)).index)
            drop.acceptProposedAction()
        }
    }
}
