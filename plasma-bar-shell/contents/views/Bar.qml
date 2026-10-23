// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// What one Arch Dock bar's window shows: the bar's containment, filling the
// window. The bar host reads how long the bar wants to be along its edge.
Item {
    id: root

    property Item containment
    property bool vertical: false
    // Edit mode: how far the window reaches past the bar's start and end,
    // set by the bar host. The bar's + stands there, outside the bar.
    property int editRoomBefore: 0
    property int editRoomAfter: 0
    readonly property int wantedLength: containment
        ? Math.ceil(vertical ? containment.implicitHeight : containment.implicitWidth) : 0

    onContainmentChanged: {
        if (!containment)
            return
        containment.parent = root
        containment.visible = true
        containment.anchors.fill = root
        containment.anchors.leftMargin = Qt.binding(() => root.vertical ? 0 : root.editRoomBefore)
        containment.anchors.rightMargin = Qt.binding(() => root.vertical ? 0 : root.editRoomAfter)
        containment.anchors.topMargin = Qt.binding(() => root.vertical ? root.editRoomBefore : 0)
        containment.anchors.bottomMargin = Qt.binding(() => root.vertical ? root.editRoomAfter : 0)
    }
}
