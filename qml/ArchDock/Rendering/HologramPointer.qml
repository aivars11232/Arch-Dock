import QtQuick

// Tells the holograms' clock (HologramClock) whether the pointer is over the
// item it sits in, a panel or a folder's opening: holograms move at their
// full rate while it is over one of them, calmer while it is away from all.
// Passive: it takes no event from the icons under it.
HoverHandler {
    id: handler

    property bool told: false
    function tell(over) {
        if (over === told)
            return
        told = over
        if (over)
            HologramClock.pointerEntered()
        else
            HologramClock.pointerLeft()
    }
    onHoveredChanged: tell(hovered)
    Component.onDestruction: tell(false)
}
