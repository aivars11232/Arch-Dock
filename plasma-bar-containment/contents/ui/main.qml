// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

// An Arch Dock bar: the widgets it holds in a row along its screen edge (a
// column on a side edge). The bar host sizes the bar's window, as long as
// this row wants to be or as long as the owner set.
ContainmentItem {
    id: root

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    GridLayout {
        id: row
        anchors.fill: parent
        flow: root.vertical ? GridLayout.TopToBottom : GridLayout.LeftToRight
        rowSpacing: 0
        columnSpacing: 0
    }

    // A widget's place in the bar: as long along the bar as the widget asks,
    // and as thick across it as the bar whatever the widget's own minimum
    // there, as a Plasma panel holds its widgets. Arch Dock's dock takes the
    // bar's whole length and centres its drawing in it.
    Component {
        id: appletSlot

        Item {
            property var plasmaApplet: null
            property Item applet: null
            property bool dock: false
            readonly property var hints: applet ? applet.Layout : null
            readonly property real minimumAlong: !hints || dock ? 0
                : Math.max(0, root.vertical ? hints.minimumHeight : hints.minimumWidth)
            readonly property real preferredAlong: {
                if (!hints)
                    return 0
                const preferred = root.vertical ? hints.preferredHeight : hints.preferredWidth
                const own = root.vertical ? applet.implicitHeight : applet.implicitWidth
                const minimum = root.vertical ? hints.minimumHeight : hints.minimumWidth
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
            dock: applet.pluginName === "org.archdock.dock"
        })
        item.parent = slot
        item.anchors.fill = slot
        item.visible = true
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
}
