// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.core as PlasmaCore
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
    readonly property Item dockApplet: {
        for (const child of row.children)
            if (child.dock)
                return child.applet
        return null
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
    readonly property Item launcherSlot: endCaps.start > 0 ? slotWith(launcherPlugins) : null
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

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    GridLayout {
        id: row
        anchors.fill: parent
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rowSpacing: 0
        columnSpacing: 0
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
            z: dock ? -1 : 0
            // Edit mode: a widget is dragged along the bar to another place,
            // where the line shows; its own menu (the right button) stays.
            MouseArea {
                anchors.fill: parent
                z: 1000
                enabled: root.editing && !slot.dock
                acceptedButtons: Qt.LeftButton
                cursorShape: enabled ? Qt.SizeAllCursor : Qt.ArrowCursor
                function placeAt(mouse) { return root.dropPlace(mapToItem(root, mouse.x, mouse.y), slot) }
                onPressed: function(mouse) { root.movingSlot = slot; root.moveLineAt = placeAt(mouse).at }
                onPositionChanged: function(mouse) { if (root.movingSlot === slot) root.moveLineAt = placeAt(mouse).at }
                onReleased: function(mouse) {
                    if (root.movingSlot !== slot)
                        return
                    root.movingSlot = null
                    if (Plasmoid.corona && typeof Plasmoid.corona.moveWidget === "function")
                        Plasmoid.corona.moveWidget(Plasmoid, slot.plasmaApplet, placeAt(mouse).index)
                }
                onCanceled: root.movingSlot = null
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
            Rectangle {
                z: 1001
                visible: root.editing && !slot.dock && slot.settingsAction !== null && slot.settingsAction.enabled
                width: 18
                height: 18
                radius: 9
                // In the widget's strip, below its remove button (beside it
                // on a side bar).
                x: root.vertical ? parent.width / 2 + 2 : parent.width + 2
                y: root.vertical ? parent.height + 2 : parent.height / 2 + 2
                color: Kirigami.Theme.highlightColor
                Kirigami.Icon {
                    anchors.fill: parent
                    anchors.margins: 3
                    source: "configure"
                    isMask: true
                    color: "white"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: slot.settingsAction.trigger()
                }
            }
            // Edit mode: takes the widget out of the bar.
            Rectangle {
                z: 1001
                visible: root.editing && !slot.dock
                width: 18
                height: 18
                radius: 9
                // In the widget's strip, above its settings button (before it
                // on a side bar).
                x: root.vertical ? parent.width / 2 - 20 : parent.width + 2
                y: root.vertical ? parent.height + 2 : parent.height / 2 - 20
                color: Kirigami.Theme.negativeTextColor
                Kirigami.Icon {
                    anchors.fill: parent
                    anchors.margins: 3
                    source: "window-close"
                    isMask: true
                    color: "white"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (Plasmoid.corona && typeof Plasmoid.corona.removeWidget === "function")
                            Plasmoid.corona.removeWidget(Plasmoid, slot.plasmaApplet)
                    }
                }
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
                : Math.max(Number(room.start || 0), root.launcherSlot ? Number(root.endCaps.start || 0) : 0)
            readonly property real roomAfter: root.lastSlot !== slot ? 0
                : Math.max(Number(room.end || 0), root.trayApplet ? Number(root.endCaps.end || 0) : 0)
            readonly property real roomSideStart: dock ? 0 : Number(room.sideStart || 0)
            readonly property real roomSideEnd: dock ? 0 : Number(room.sideEnd || 0)
            Layout.leftMargin: root.vertical ? roomSideStart : roomBefore
            // Edit mode: a strip beside each widget (after it along the
            // bar) holds its buttons, so they never cover the widget.
            readonly property real editRoom: root.editing && !dock ? 22 : 0
            Layout.rightMargin: root.vertical ? roomSideEnd : roomAfter + editRoom
            Layout.topMargin: root.vertical ? roomBefore : roomSideStart
            Layout.bottomMargin: root.vertical ? roomAfter + editRoom : roomSideEnd
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
    }

    Connections {
        target: Plasmoid
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

    // Edit mode, Plasma's own: the bar enters and leaves it with Plasma's
    // desktop and panels. The bar is outlined, and a widget dragged from
    // Plasma's widget explorer is dropped where the line shows, between the
    // bar's widgets; the application menu stays first.
    readonly property bool editing: Plasmoid.corona ? Plasmoid.corona.editMode : false
    // Where a widget dropped at `point` goes: how many of the bar's widgets
    // stand before it, and where along the bar the line between them is.
    property Item movingSlot: null
    property real moveLineAt: 0
    function dropPlace(point, moving) {
        const along = root.vertical ? point.y : point.x
        const spans = []
        for (const child of row.children) {
            if (!child.plasmaApplet || child === moving)
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
        const order = root.appletOrder.filter(id => !moving || String(id) !== String(moving.plasmaApplet.id))
        const next = index < spans.length ? order.indexOf(String(spans[index].slot.plasmaApplet.id)) : -1
        return { index: next >= 0 ? next : order.length,
                 at: index === 0 && spans.length > 0 && isFinite(spans[0].start)
                     ? spans[0].start : index === spans.length ? before : (before + after) / 2 }
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
    Rectangle {
        z: 1001
        visible: widgetDrop.containsDrag || root.movingSlot !== null
        enabled: false
        color: Kirigami.Theme.highlightColor
        readonly property real at: root.movingSlot !== null ? root.moveLineAt : widgetDrop.lineAt
        width: root.vertical ? root.width - 8 : 3
        height: root.vertical ? 3 : root.height - 8
        x: root.vertical ? 4 : at - width / 2
        y: root.vertical ? at - height / 2 : 4
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
            Plasmoid.corona.dropWidget(Plasmoid, plugin, root.dropPlace(Qt.point(drop.x, drop.y)).index)
            drop.acceptProposedAction()
        }
    }
}
