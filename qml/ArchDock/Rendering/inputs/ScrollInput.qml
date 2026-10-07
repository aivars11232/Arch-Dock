import QtQuick

// A wheel-only overlay: clicks, touch dragging, keys and scrollbar controls
// remain on the existing views. Supply nested Flickables in priority order.
Item {
    id: root
    property var flickables: []
    property bool horizontalOnly: false
    property bool consumeAtBounds: true
    property var excludedItems: []
    // How far one wheel notch (120 angle units) scrolls each way, and how many
    // pixels one pixel of touchpad travel scrolls; Scroll sensitivity scales
    // both (PD-16).
    readonly property real lineNotch: 20 * Qt.styleHints.wheelScrollLines
    property real verticalNotch: lineNotch
    property real horizontalNotch: lineNotch
    property real verticalPixelScale: 1
    property real horizontalPixelScale: 1
    property real sensitivity: 1
    anchors.fill: parent
    z: 10000

    // Exclude regions before pointer acceptance; onWheel runs after a blocking
    // WheelHandler has already accepted its event point.
    containmentMask: QtObject {
        function contains(position: point): bool {
            if (position.x < 0 || position.y < 0 || position.x >= root.width || position.y >= root.height)
                return false
            for (const item of root.excludedItems) {
                if (!item || !item.visible)
                    continue
                const point = root.mapToItem(item, position.x, position.y)
                if (point.x >= 0 && point.y >= 0 && point.x < item.width && point.y < item.height)
                    return false
            }
            return true
        }
    }

    function scroll(event) {
        const pixels = event.pixelDelta
        const angles = event.angleDelta
        const scale = Math.max(0.25, Math.min(4, Number(sensitivity) || 1))
        let dx = (pixels.x !== 0 ? -pixels.x * horizontalPixelScale
                                 : -angles.x * horizontalNotch / 120) * scale
        let dy = (pixels.y !== 0 ? -pixels.y * verticalPixelScale
                                 : -angles.y * verticalNotch / 120) * scale
        if ((event.modifiers & Qt.ShiftModifier) && dx === 0) {
            dx = dy / Math.max(1e-6, verticalNotch) * horizontalNotch
            dy = 0
        }
        if (horizontalOnly)
            dy = 0
        let moved = false
        for (const view of flickables) {
            if (!view || !view.visible || !view.enabled)
                continue
            const minX = Number(view.originX || 0)
            const minY = Number(view.originY || 0)
            const x = Math.max(minX, Math.min(minX + Math.max(0, view.contentWidth - view.width), view.contentX + dx))
            const y = Math.max(minY, Math.min(minY + Math.max(0, view.contentHeight - view.height), view.contentY + dy))
            if (dx !== 0 && x !== view.contentX) {
                view.contentX = x
                dx = 0
                moved = true
            }
            if (dy !== 0 && y !== view.contentY) {
                view.contentY = y
                dy = 0
                moved = true
            }
            if (dx === 0 && dy === 0)
                break
        }
        event.accepted = moved || consumeAtBounds
    }

    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function(event) {
            if (event.pixelDelta.y !== 0 || event.angleDelta.y !== 0)
                root.scroll(event)
        }
    }
    WheelHandler {
        target: null
        orientation: Qt.Horizontal
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function(event) {
            if (event.pixelDelta.y === 0 && event.angleDelta.y === 0)
                root.scroll(event)
        }
    }
}
