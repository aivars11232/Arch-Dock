pragma Singleton
import QtQuick

// The holograms' one clock: every hologram's scan lines, flicker, float and
// light run on it, so a panel full of them redraws together, thirty times a
// second, rather than once for each. It runs only while a hologram holds
// it, and starts again from nought every half hour, which shows as no more
// than one of a hologram's flickers.
QtObject {
    id: clock

    property int holders: 0
    property real time: 0
    readonly property double started: Date.now()

    function hold() {
        holders += 1
    }

    function release() {
        holders = Math.max(0, holders - 1)
    }

    property Timer ticker: Timer {
        interval: 33
        repeat: true
        running: clock.holders > 0
        onTriggered: clock.time = ((Date.now() - clock.started) / 1000) % 1800
    }
}
