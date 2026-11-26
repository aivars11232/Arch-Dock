pragma Singleton
import QtQuick

// The holograms' one clock: every hologram's scan lines, flicker, float and
// light, and the panel decorations' moving lights, run on it, so a panel full
// of them redraws together (thirty times a second while the pointer is over
// a panel, fifteen while it is away) rather than once for each. It runs only
// while something holds it, and starts again from nought every half hour,
// which shows as no more than one of a hologram's flickers.
QtObject {
    id: clock

    property int holders: 0
    property real time: 0
    // Panels and openings the pointer is over (HologramPointer): while it is
    // over one, the holograms move thirty steps a second; while it is away
    // from all of them, fifteen, which halves their redraws.
    property int pointedAt: 0
    function pointerEntered() {
        pointedAt += 1
    }
    function pointerLeft() {
        pointedAt = Math.max(0, pointedAt - 1)
    }
    readonly property double started: Date.now()

    function now() {
        return ((Date.now() - started) / 1000) % 1800
    }

    function hold() {
        // Started again after every holder let it go: the time of now at
        // once, so what is shown again does not first show where it stopped.
        if (holders === 0)
            time = now()
        holders += 1
    }

    function release() {
        holders = Math.max(0, holders - 1)
    }

    property Timer ticker: Timer {
        interval: clock.pointedAt > 0 ? 33 : 66
        repeat: true
        running: clock.holders > 0
        onTriggered: clock.time = clock.now()
    }
}
