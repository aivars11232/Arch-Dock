import QtQuick
import QtQuick.Effects

Item {
    id: root

    property var layerDefinition: ({})
    property var assetDefinition: ({})
    property var sliceDefinition: ({})
    property string source: ""
    property real fixedStartPixels: 0
    property real fixedEndPixels: 0
    property real layerOpacity: 1
    property bool tintEnabled: false
    property color tintColor: "#ffffff"
    property real motionOffset: 0
    property real motionOverflow: 0

    // Raster budget, in pixels per axis. Zero keeps Qt's own behaviour: the
    // asset is decoded at its natural size and every size it has been drawn at
    // stays in the shared pixmap cache. A positive budget rasterises the layer
    // at the size it is actually drawn, capped, and drops it from that cache
    // when the layer goes away - which is what lets a panel switch between
    // large perspective platforms without accumulating textures.
    property int rasterBudget: 0
    property bool cacheImage: true

    readonly property int rasterWidth: rasterBudget > 0
        ? Math.max(1, Math.min(rasterBudget, Math.ceil(outputWidth()))) : 0
    readonly property int rasterHeight: rasterBudget > 0
        ? Math.max(1, Math.min(rasterBudget, Math.ceil(height))) : 0

    // With a raster budget the asset is decoded at the size it is drawn, and
    // Qt applies sourceClipRect to that scaled image, not to the asset's own
    // pixels. The declared rectangle is therefore scaled the same way. Left in
    // natural pixels it cuts a natural-sized piece out of a smaller picture,
    // and the artwork is drawn shrunk into the top-left corner of its layer.
    readonly property var rasterSource: {
        const rect = sourceRectangle()
        if (rasterBudget <= 0 || rect.width <= 0 || rect.height <= 0)
            return { width: 0, height: 0, clip: rect }
        const natural = assetDefinition && assetDefinition.naturalSize
            ? assetDefinition.naturalSize : ({})
        const scaleX = rasterWidth / rect.width
        const scaleY = rasterHeight / rect.height
        return {
            width: Math.max(1, Math.round(Math.max(
                numeric(natural.width, 0), rect.x + rect.width) * scaleX)),
            height: Math.max(1, Math.round(Math.max(
                numeric(natural.height, 0), rect.y + rect.height) * scaleY)),
            clip: Qt.rect(Math.round(rect.x * scaleX), Math.round(rect.y * scaleY),
                          rasterWidth, rasterHeight)
        }
    }

    // Presentation track for this layer's role, supplied by PanelSkin2D from
    // PanelMotionController. It moves and scales the whole drawn part; the
    // slice geometry inside it is untouched, so a declared cap keeps its
    // proportions however far the track carries it.
    property real motionTranslateX: 0
    property real motionTranslateY: 0
    property real motionScaleX: 1
    property real motionScaleY: 1

    readonly property bool motionActive:
        motionTranslateX !== 0 || motionTranslateY !== 0
        || motionScaleX !== 1 || motionScaleY !== 1

    readonly property string layerId: String(
        layerDefinition ? layerDefinition.id || "" : "")
    readonly property string role: String(
        layerDefinition ? layerDefinition.role || "" : "")
    readonly property bool splitStart: role === "split-start"
    readonly property bool splitCenter: role === "split-center"
    readonly property bool splitEnd: role === "split-end"
    readonly property bool splitLayer: splitStart || splitCenter || splitEnd
    // A rear platform is the baked 2.5D equivalent of a surface: without it
    // there is nothing for icons to stand on, so it fails rather than being
    // skipped like a decorative layer.
    readonly property bool requiredLayer:
        role === "surface" || role === "rear" || splitLayer
    readonly property bool assetAvailable:
        source.length > 0 && assetDefinition
        && typeof assetDefinition === "object"
        && String(assetDefinition.id || "").length > 0
    readonly property bool tintActive:
        tintEnabled && assetAvailable
        && String(assetDefinition.kind || "") === "mask"
    readonly property bool settled:
        !assetAvailable ? !requiredLayer
        : rawImage.status === Image.Ready || rawImage.status === Image.Error
    readonly property bool ready:
        requiredLayer ? assetAvailable && rawImage.status === Image.Ready
                      : settled
    readonly property bool failed:
        requiredLayer && (!assetAvailable || rawImage.status === Image.Error)
    readonly property bool skipped:
        !requiredLayer && (!assetAvailable || rawImage.status === Image.Error)
    readonly property alias imageItem: rawImage

    function numeric(value, fallback) {
        const candidate = Number(value)
        return isFinite(candidate) ? candidate : fallback
    }

    function sourceRectangle() {
        const sourceRect = layerDefinition && layerDefinition.sourceRect
            ? layerDefinition.sourceRect : null
        if (sourceRect) {
            return Qt.rect(numeric(sourceRect.x, 0),
                           numeric(sourceRect.y, 0),
                           Math.max(0, numeric(sourceRect.width, 0)),
                           Math.max(0, numeric(sourceRect.height, 0)))
        }
        const size = assetDefinition && assetDefinition.naturalSize
            ? assetDefinition.naturalSize : ({})
        return Qt.rect(0, 0,
                       Math.max(0, numeric(size.width, 0)),
                       Math.max(0, numeric(size.height, 0)))
    }

    function outputX() {
        if (splitStart)
            return 0
        if (splitCenter)
            return fixedStartPixels
        if (splitEnd)
            return width - fixedEndPixels
        return -Math.max(0, motionOverflow) + motionOffset
    }

    function outputWidth() {
        if (splitStart)
            return Math.max(0, fixedStartPixels)
        if (splitCenter)
            return Math.max(0, width - fixedStartPixels - fixedEndPixels)
        if (splitEnd)
            return Math.max(0, fixedEndPixels)
        return width + Math.max(0, motionOverflow) * 2
    }

    opacity: Math.max(0, Math.min(1, layerOpacity))
    visible: assetAvailable && rawImage.status === Image.Ready

    // The scale is taken about the part's own drawn centre, not the panel's,
    // so an asymmetric pair of end caps still converges on the middle of the
    // part that is actually shrinking.
    transform: [
        Scale {
            origin.x: root.outputX() + root.outputWidth() / 2
            origin.y: root.height / 2
            xScale: Math.max(0, root.motionScaleX)
            yScale: Math.max(0, root.motionScaleY)
        },
        Translate {
            x: root.motionTranslateX
            y: root.motionTranslateY
        }
    ]

    Image {
        id: rawImage

        x: root.outputX()
        y: 0
        width: root.outputWidth()
        height: root.height
        source: root.source
        sourceClipRect: root.rasterSource.clip
        fillMode: root.splitCenter && root.sliceDefinition
                && String(root.sliceDefinition.centerMode || "stretch") === "tile"
            ? Image.TileHorizontally : Image.Stretch
        sourceSize.width: root.rasterSource.width
        sourceSize.height: root.rasterSource.height
        smooth: true
        asynchronous: false
        cache: root.cacheImage
        layer.enabled: root.tintActive
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: root.tintColor
            blurEnabled: false
            shadowEnabled: false
        }
    }
}
