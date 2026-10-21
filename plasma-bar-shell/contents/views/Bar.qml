// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// What one Arch Dock bar's window shows: the bar's containment, filling the
// window. The bar host reads how long the bar wants to be along its edge.
Item {
    id: root

    property Item containment
    property bool vertical: false
    readonly property int wantedLength: containment
        ? Math.ceil(vertical ? containment.implicitHeight : containment.implicitWidth) : 0

    onContainmentChanged: {
        if (!containment)
            return
        containment.parent = root
        containment.visible = true
        containment.anchors.fill = root
    }
}
