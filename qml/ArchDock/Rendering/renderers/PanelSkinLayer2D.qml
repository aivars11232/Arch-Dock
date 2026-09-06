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
        sourceClipRect: root.sourceRectangle()
        fillMode: root.splitCenter && root.sliceDefinition
                && String(root.sliceDefinition.centerMode || "stretch") === "tile"
            ? Image.TileHorizontally : Image.Stretch
        smooth: true
        asynchronous: false
        cache: true
        layer.enabled: root.tintActive
        layer.effect: MultiEffect {
            colorization: 1
            colorizationColor: root.tintColor
            blurEnabled: false
            shadowEnabled: false
        }
    }
}
