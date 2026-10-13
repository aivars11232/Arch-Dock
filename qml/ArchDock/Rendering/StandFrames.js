.pragma library

// Painted pictures of moving stands, shared by every stand drawn the same way
// (see the stand layer in IconScene.qml). A spinning or wobbling stand is
// painted once for each step of its motion; every other icon with the same
// stand, on the dock or in an open folder, copies that picture instead of
// painting its own. Canvas painting runs on the main thread, so a folder of
// two dozen spinning stands, each painting itself every frame, used most of
// each frame and made the whole dock stutter.

// About ten turns of a stand at 120 steps a turn.
const limit = 1200
const frames = new Map()

// The picture painted for `key`, or null.
function frame(key) {
    const found = frames.get(key)
    return found === undefined ? null : found
}

// Keeps `picture` for `key`; the oldest pictures go past the limit.
function keep(key, picture) {
    if (!picture || frames.has(key)) return
    frames.set(key, picture)
    while (frames.size > limit)
        frames.delete(frames.keys().next().value)
}

function count() {
    return frames.size
}
