import QtQuick

Item {
    id: root

    property url source: ""
    property size sourceSize: Qt.size(0, 0)
    property rect sourceRect: Qt.rect(0, 0, sourceSize.width, sourceSize.height)
    property real fixedStart: 0
    property real fixedEnd: 0
    property string centerMode: "stretch"
    property real threshold: 0.5

    readonly property bool ready: cacheReady && loadError.length === 0
    readonly property string errorReason: loadError
    readonly property alias containmentObject: hitTest
    readonly property int rasterWidth: Math.max(0, Math.round(width))
    readonly property int rasterHeight: Math.max(0, Math.round(height))

    property bool cacheReady: false
    property string loadError: "mask-source-missing"
    property var alphaBytes: null
    property int loadPolls: 0

    function resetCache() {
        cacheReady = false
        alphaBytes = null
        loadPolls = 0
        const value = String(source || "")
        if (value.length === 0) {
            loadError = "mask-source-missing"
            return
        }
        if (sourceSize.width <= 0 || sourceSize.height <= 0
                || sourceRect.width <= 0 || sourceRect.height <= 0
                || sourceRect.x < 0 || sourceRect.y < 0
                || sourceRect.x + sourceRect.width > sourceSize.width
                || sourceRect.y + sourceRect.height > sourceSize.height
                || rasterWidth <= 0 || rasterHeight <= 0
                || rasterWidth * rasterHeight > 16777216) {
            loadError = "mask-size-invalid"
            return
        }
        const verticalScale = rasterHeight / sourceRect.height
        if (fixedStart < 0 || fixedEnd < 0
                || fixedStart + fixedEnd >= sourceRect.width
                || fixedStart * verticalScale
                    + fixedEnd * verticalScale >= rasterWidth) {
            loadError = "mask-slice-invalid"
            return
        }
        loadError = ""
        if (cache.isImageLoaded(source))
            cache.requestPaint()
        else
            cache.loadImage(source)
    }

    function contains(point) {
        if (!ready || !point || width <= 0 || height <= 0)
            return false
        const pointX = Number(point.x)
        const pointY = Number(point.y)
        if (pointX < 0 || pointY < 0 || pointX >= width || pointY >= height)
            return false
        const rasterX = Math.max(0, Math.min(
            rasterWidth - 1, Math.floor(pointX * rasterWidth / width)))
        const rasterY = Math.max(0, Math.min(
            rasterHeight - 1, Math.floor(pointY * rasterHeight / height)))
        const alphaIndex = (rasterY * rasterWidth + rasterX) * 4 + 3
        const alpha = alphaBytes && alphaIndex < alphaBytes.length
            ? Number(alphaBytes[alphaIndex]) / 255 : 0
        return alpha >= Math.max(0, Math.min(1, Number(threshold)))
    }

    QtObject {
        id: hitTest

        function contains(point: point): bool {
            return root.contains(point)
        }
    }

    onSourceChanged: resetCache()
    onSourceSizeChanged: resetCache()
    onSourceRectChanged: resetCache()
    onFixedStartChanged: resetCache()
    onFixedEndChanged: resetCache()
    onCenterModeChanged: resetCache()
    onRasterWidthChanged: resetCache()
    onRasterHeightChanged: resetCache()
    Component.onCompleted: resetCache()

    Timer {
        interval: 20
        repeat: true
        running: String(root.source || "").length > 0
            && !root.cacheReady && root.loadError.length === 0

        onTriggered: {
            if (cache.isImageError(root.source)) {
                root.loadError = "mask-load-failed"
                return
            }
            if (cache.isImageLoaded(root.source)) {
                cache.requestPaint()
                return
            }
            ++root.loadPolls
            if (root.loadPolls >= 250)
                root.loadError = "mask-load-timeout"
        }
    }

    Canvas {
        id: cache

        width: Math.max(1, root.rasterWidth)
        height: Math.max(1, root.rasterHeight)
        visible: root.rasterWidth > 0 && root.rasterHeight > 0
        opacity: 0.001
        x: -width - 1
        y: -height - 1
        renderStrategy: Canvas.Immediate
        renderTarget: Canvas.Image

        onImageLoaded: {
            if (isImageLoaded(root.source))
                requestPaint()
        }

        onPaint: {
            if (String(root.source || "").length === 0)
                return
            const context = getContext("2d")
            context.reset()
            context.clearRect(0, 0, width, height)
            const source = root.sourceRect
            const verticalScale = height / source.height
            const startWidth = root.fixedStart * verticalScale
            const endWidth = root.fixedEnd * verticalScale
            const sourceCenterWidth = source.width
                - root.fixedStart - root.fixedEnd
            const destinationCenterWidth = width - startWidth - endWidth

            if (root.fixedStart > 0) {
                context.drawImage(
                    root.source,
                    source.x, source.y, root.fixedStart, source.height,
                    0, 0, startWidth, height)
            }
            if (root.centerMode === "tile") {
                const tileWidth = sourceCenterWidth * verticalScale
                for (let x = startWidth; x < startWidth + destinationCenterWidth;
                        x += tileWidth) {
                    const drawnWidth = Math.min(
                        tileWidth, startWidth + destinationCenterWidth - x)
                    const drawnSourceWidth = sourceCenterWidth
                        * drawnWidth / tileWidth
                    context.drawImage(
                        root.source,
                        source.x + root.fixedStart, source.y,
                        drawnSourceWidth, source.height,
                        x, 0, drawnWidth, height)
                }
            } else {
                context.drawImage(
                    root.source,
                    source.x + root.fixedStart, source.y,
                    sourceCenterWidth, source.height,
                    startWidth, 0, destinationCenterWidth, height)
            }
            if (root.fixedEnd > 0) {
                context.drawImage(
                    root.source,
                    source.x + source.width - root.fixedEnd, source.y,
                    root.fixedEnd, source.height,
                    width - endWidth, 0, endWidth, height)
            }
            const pixels = context.getImageData(0, 0, width, height)
            root.alphaBytes = pixels.data
            root.cacheReady = root.alphaBytes
                && root.alphaBytes.length === width * height * 4
            root.loadError = root.cacheReady ? "" : "mask-cache-failed"
        }
    }
}
