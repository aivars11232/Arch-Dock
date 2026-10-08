import QtQuick
import QtQuick.Window
import ArchDock.Input 1.0 as NativeInput

// A wheel-only overlay: clicks, touch dragging, keys and scrollbar controls
// remain on the existing views. Supply nested Flickables in priority order.
Item {
    id: root
    property var flickables: []
    property bool horizontalOnly: false
    property bool verticalOnly: false
    property int acceptedModifiers: Qt.KeyboardModifierMask
    // Panels and free-folder tracks reuse the native source routing while
    // retaining their own bounded travel/rotation reducers.
    property var wheelConsumer: null
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
    NativeInput.WheelSource { id: wheelSource; window: root.Window.window }

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

    function dispatchWheel(event, discrete) {
        const pixels = Qt.point(discrete && event.angleDelta.x !== 0 ? 0 : event.pixelDelta.x,
                                discrete && event.angleDelta.y !== 0 ? 0 : event.pixelDelta.y)
        if (typeof wheelConsumer === "function")
            wheelConsumer(event, pixels)
        else
            scroll(event, pixels)
    }

    function scroll(event, pixels) {
        if (!pixels) pixels = event.pixelDelta
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

    // The native event source remains reliable when Wayland identifies the
    // whole seat as a touchpad. Deltas, device type and phase alone do not.
    WheelHandler {
        target: null
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: root.acceptedModifiers
        onWheel: function(event) {
            if (event.pixelDelta.y !== 0 || event.angleDelta.y !== 0)
                root.dispatchWheel(event, wheelSource.discrete)
        }
    }
    WheelHandler {
        target: null
        orientation: Qt.Horizontal
        enabled: !root.verticalOnly
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: root.acceptedModifiers
        onWheel: function(event) {
            if (event.pixelDelta.y === 0 && event.angleDelta.y === 0)
                root.dispatchWheel(event, wheelSource.discrete)
        }
    }
}
