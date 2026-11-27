// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import ArchDock.Input 1.0

// What one Arch Dock bar's window shows: the bar's containment, filling the
// window but for the room it may reach past the bar and the gap it may cover
// at the screen's edge (below). The bar host reads how long the bar wants to
// be along its edge, and that room.
Item {
    id: root

    property Item containment
    property bool vertical: false
    property string edge: "bottom"
    // Edit mode: how far the window reaches past the bar's start and end,
    // set by the bar host. The bar's + stands there, outside the bar.
    property int editRoomBefore: 0
    property int editRoomAfter: 0
    // How deep the bar is across its edge, set by the bar host. With stands
    // or holograms its window reaches past the bar's inner side, as far as
    // the bar asks: the containment keeps the bar's depth at the edge.
    property int barThickness: 0
    // While the bar hides at times off the screen's edge, its window also
    // covers that gap, set by the bar host: the containment keeps its place
    // off the edge, and the gap takes the pointer, where KWin shows the bar.
    property int edgeGap: 0
    readonly property int reachPast: barThickness > 0
        ? Math.max(0, (vertical ? width : height) - barThickness - edgeGap) : 0
    readonly property int wantedLength: containment
        ? Math.ceil(vertical ? containment.implicitHeight : containment.implicitWidth) : 0
    readonly property int wantedReach: containment && containment.wantedReach !== undefined
        ? Math.max(0, Math.ceil(Number(containment.wantedReach))) : 0

    onContainmentChanged: {
        if (!containment)
            return
        containment.parent = root
        containment.visible = true
        containment.anchors.fill = root
        containment.anchors.leftMargin = Qt.binding(() => (root.vertical ? 0 : root.editRoomBefore)
            + (root.edge === "right" ? root.reachPast : 0) + (root.edge === "left" ? root.edgeGap : 0))
        containment.anchors.rightMargin = Qt.binding(() => (root.vertical ? 0 : root.editRoomAfter)
            + (root.edge === "left" ? root.reachPast : 0) + (root.edge === "right" ? root.edgeGap : 0))
        containment.anchors.topMargin = Qt.binding(() => (root.vertical ? root.editRoomBefore : 0)
            + (root.edge === "bottom" ? root.reachPast : 0) + (root.edge === "top" ? root.edgeGap : 0))
        containment.anchors.bottomMargin = Qt.binding(() => (root.vertical ? root.editRoomAfter : 0)
            + (root.edge === "top" ? root.reachPast : 0) + (root.edge === "bottom" ? root.edgeGap : 0))
    }

    // Past the bar the window takes the pointer only where the dock shows
    // something (given in the containment's coordinates); everywhere else
    // there it passes through to what lies under it. The bar's own part
    // reaches the window's outer side, the edge gap with it. The outline is
    // set even with nothing past the bar (then the whole window): Plasma
    // places the popups and tooltips of the bar's widgets against its
    // bounds, and without one it put them at the window's corner, over a
    // bar on the left edge.
    WindowInputShape {
        window: root.Window.window
        enabled: true
        rects: {
            const along = root.vertical ? root.height : root.width
            const across = (root.vertical ? root.width : root.height) - root.reachPast
            const bar = root.edge === "top" ? Qt.rect(0, 0, along, across)
                : root.edge === "left" ? Qt.rect(0, 0, across, along)
                : root.edge === "right" ? Qt.rect(root.reachPast, 0, across, along)
                : Qt.rect(0, root.reachPast, along, across)
            if (!root.containment)
                return [bar]
            const place = Qt.point(root.containment.x, root.containment.y)
            return [bar].concat((root.containment.shownPastRects || []).map(rect =>
                Qt.rect(place.x + rect.x, place.y + rect.y, rect.width, rect.height)))
        }
    }
}
