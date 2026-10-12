import QtQuick
import QtQuick.Effects
import org.kde.kirigami as Kirigami
import "MotionChannels.js" as MotionChannels
import "LayoutEngine.js" as LayoutEngine

// One dock icon, drawn in layers: rear effects, the icon style's base and
// tile, the application's glyph, front treatments, the running indicator,
// badge and progress. Its state (hover, pressed, active, running, urgent,
// launching, disabled, drop target, edit mode) selects how each layer is
// drawn, and motion channels move them. Its input rectangle never changes
// size with hover magnification.
Item {
    id: root

    property var entry: ({})
    property var iconStyleDefinition: ({})
    property var tileSettings: ({})
    readonly property bool customTileActive: tileSettings.iconTileMode === "custom"
    // The Pedestals page: a stand under the icon on a flat panel. Auto is a
    // column, as in 3D, unless the icon style draws its own pedestal; None
    // draws nothing; a chosen shape floats between the panel and the icon
    // in its own material, size and motion.
    readonly property string pedestalShape: String(tileSettings.pedestalShape || "auto").toLowerCase()
    readonly property string pedestalKind: pedestalShape === "auto" ? "column" : pedestalShape
    readonly property bool styleDrawsPedestal: Boolean(tileSettings.iconPedestalEnabled)
    // Drawn here on a flat panel only: a 3D scene draws its own pedestals
    // and takes this scene's glyph and tile as textures.
    readonly property bool pedestalDrawn: pedestalShape !== "none" && !meshVisualActive
        && !(pedestalShape === "auto" && styleDrawsPedestal)
    readonly property real pedestalSize: tileValue("pedestalSize", 1, 0.5, 2)
    readonly property real pedestalLift: tileValue("pedestalLift", 0.3, 0, 1)
    readonly property string pedestalMaterial: String(tileSettings.pedestalMaterial || "panel").toLowerCase()
    readonly property string pedestalColor: String(tileSettings.pedestalColor || "")
    readonly property string pedestalAnimation: String(tileSettings.pedestalAnimation || "none").toLowerCase()
    readonly property real pedestalAnimationSpeed: tileValue("pedestalAnimationSpeed", 1, 0.25, 3)
    // How far the icon rises above its cell's middle to leave room for the
    // stand and the gap under it.
    readonly property real pedestalLiftPx: pedestalDrawn ? logicalSize * (0.06 + pedestalLift * 0.28) : 0
    readonly property bool pedestalMotionActive: pedestalDrawn && pedestalAnimation !== "none"
        && !reducedMotion && visible
    property real pedestalPhase: 0
    NumberAnimation on pedestalPhase {
        running: root.pedestalMotionActive
        loops: Animation.Infinite
        from: 0
        to: 1
        duration: Math.round(4000 / Math.max(0.25, root.pedestalAnimationSpeed))
    }
    property real logicalSize: 52
    property real visualScale: 1
    property string tileShape: "style-default"
    property string appearance: "glass"
    property bool showReflection: false
    property bool showIndicator: true
    property bool vertical: false
    property bool hovered: false
    property bool pressed: false
    property bool active: Boolean(entry && entry.active)
    property bool running: Boolean(entry && entry.running)
    property bool minimized: Boolean(entry && entry.minimized)
    property bool urgent: Boolean(entry && (entry.attention || entry.urgent))
    property bool launching: Boolean(entry && entry.launching)
    property bool disabled: Boolean(entry && entry.disabled)
    property bool dropTarget: false
    property bool editMode: false
    property int windowCount: Math.max(1, Number(
        entry && entry.windowCount || 1))
    property string iconSource: String(
        entry && entry.iconName || "application-x-executable")
    property string badgeText: String(
        entry && (entry.badgeText || entry.badge) || "")
    property real progress: entry && entry.progress !== undefined
        ? Number(entry.progress) : -1
    property var indicatorStyle: ({})
    property bool reducedMotion: false
    property real glowAmount: 0
    property bool glowAnimating: false
    property int glowDuration: 170
    // Each consumer owns its own texture sources. Input, status overlays and
    // accessibility remain on the original entry, outside the mesh scene.
    property bool meshVisualActive: false
    readonly property var meshVisualItem: root
    readonly property alias glyphTextureItem: glyphTexture
    readonly property alias tileTextureItem: tileTexture

    // Per-layer motion, as resolved by MotionChannels. The scene applies these
    // to visual transforms only; `logicalInputRegion` never follows them, so a
    // moving icon keeps a stationary pointer target.
    property var glyphMotion: ({})
    property var tileMotion: ({})
    property var indicatorMotion: ({})
    readonly property var resolvedGlyphMotion: normalizedMotion(meshVisualActive ? null : glyphMotion)
    readonly property var resolvedTileMotion: normalizedMotion(meshVisualActive ? null : tileMotion)
    readonly property var resolvedIndicatorMotion:
        normalizedMotion(indicatorMotion)
    // A flat-card turn is a Y-axis rotation, not the flat Z spin: the card
    // narrows towards its centre and one edge recedes under perspective.
    readonly property real glyphTurnAngle: Number(resolvedGlyphMotion.rotateY)
    readonly property bool glyphTurnActive: Math.abs(glyphTurnAngle) > 0.01

    readonly property var resolvedIconStyle: IconStyleResolver.resolve(
        iconStyleDefinition,
        {
            edit: editMode,
            disabled: disabled,
            drop: dropTarget,
            urgent: urgent,
            pressed: pressed,
            hover: hovered,
            launching: launching,
            active: active,
            minimized: minimized,
            running: running
        },
        resolverEntry(), styleSettings(), logicalSize)
    readonly property string visualState: resolvedIconStyle.stateId
    readonly property var styleState: resolvedIconStyle.state || ({})
    readonly property var styleInset:
        resolvedIconStyle.safeGlyphInset || ({
            left: 0, top: 0, right: 0, bottom: 0
        })
    // Any style asset that fails at load time collapses the whole treatment
    // to the safe original glyph rather than leaving a half-drawn icon.
    readonly property bool styleAssetsFailed:
        styleShadow.assetFailed || styleGlow.assetFailed
        || styleRear.assetFailed || styleBase.assetFailed
        || styleReflection.assetFailed || styleFront.assetFailed
        || styleMaskFailed
    // Resolve identity independently of fallback: clearing the Image source
    // on its own Error both creates a binding loop and loses that identity.
    readonly property string currentStyleMaskSource:
        resolvedIconStyle.maskLayer
        ? String(resolvedIconStyle.maskLayer.source) : ""
    property var failedStyleMaskSources: ({})
    readonly property bool styleMaskFailed:
        currentStyleMaskSource.length > 0
        && failedStyleMaskSources[currentStyleMaskSource] === true

    readonly property bool styledLayersActive:
        Boolean(resolvedIconStyle.renderStyledLayers) && !styleAssetsFailed
    readonly property bool styleGlyphTreatmentActive:
        Boolean(resolvedIconStyle.valid)
        && (Boolean(resolvedIconStyle.parametersActive)
            || String(resolvedIconStyle.styleId || "plain-original") !== "plain-original")
        && !styleAssetsFailed

    // Effective glyph treatment, already compatibility-gated by the resolver.
    readonly property string glyphTreatment: styleAssetsFailed ? "original"
        : String(resolvedIconStyle.glyphTreatment || "original")
    readonly property string glyphTint:
        String(resolvedIconStyle.glyphTint || "")
    readonly property bool glyphIsMask:
        !styleAssetsFailed && Boolean(resolvedIconStyle.glyphIsMask)
        && glyphTint.length > 0
    readonly property var styleMaskLayer:
        styleAssetsFailed ? null : (resolvedIconStyle.maskLayer || null)
    readonly property bool styleMaskActive:
        styledLayersActive && styleMaskLayer !== null
    readonly property bool tileRenderingEnabled:
        resolvedIconStyle.tileEnabled === undefined
        ? true : Boolean(resolvedIconStyle.tileEnabled)
    readonly property var effectiveIconStyleDefinition:
        resolvedIconStyle.styleDefinition || iconStyleDefinition
    readonly property string resolvedIconSource: String(
        resolvedIconStyle.glyphSource || iconSource)
    readonly property var resolvedIndicatorStyle:
        IconStyleResolver.indicatorStyle(
            indicatorStyle, resolvedIconStyle)
    readonly property var logicalInputRegion: ({
        x: 0,
        y: 0,
        width: logicalSize,
        height: logicalSize
    })
    readonly property bool badgeSlotActive: badgeText.length > 0
    readonly property bool progressSlotActive: isFinite(progress)
        && progress >= 0 && progress <= 1
    readonly property alias visualLayerItem: visualLayer
    readonly property alias rearLayerItem: rearLayer
    readonly property alias baseLayerItem: baseLayer
    readonly property alias glyphLayerItem: glyphLayer
    readonly property alias glyphItem: glyph
    readonly property alias frontLayerItem: frontLayer
    readonly property alias indicatorItem: runningIndicator
    readonly property alias tileTransformItem: baseLayer
    readonly property alias glyphTransformItem: glyphLayer
    readonly property alias indicatorTransformItem: runningIndicator
    readonly property alias styleRearItem: styleRear
    readonly property alias styleBaseItem: styleBase
    readonly property alias styleFrontItem: styleFront
    readonly property alias statusLayerItem: statusLayer
    readonly property alias badgeItem: badge
    readonly property alias progressItem: progressTrack
    readonly property alias attentionItem: attentionMarker
    readonly property alias temporaryStatusItem: temporaryNotice
    readonly property alias statusTextItem: statusLabel


    function recordStyleMaskFailure(source) {
        if (source.length === 0)
            return
        const next = ({})
        const keys = Object.keys(failedStyleMaskSources)
        for (let index = 0; index < keys.length; ++index)
            next[keys[index]] = true
        next[source] = true
        failedStyleMaskSources = next
    }

    function alphaColor(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
    }

    function styleSettings() {
        const values = IconStyleResolver.copyMap(tileSettings)
        if (values.iconShape === undefined)
            values.iconShape = tileShape
        return values
    }

    function tileValue(key, fallback, minimum, maximum) {
        const value = Number(tileSettings[key] === undefined ? fallback : tileSettings[key])
        return Math.max(minimum, Math.min(maximum, isFinite(value) ? value : fallback))
    }

    function tileRadius() {
        if (tileShape === "circle")
            return logicalSize / 2
        if (tileShape === "square")
            return 3
        if (tileShape === "squircle")
            return logicalSize * 0.32
        return logicalSize * 0.22
    }

    function normalizedMotion(value) {
        const resting = MotionChannels.restingMotion()
        const source = value || ({})
        const keys = Object.keys(source)
        for (let index = 0; index < keys.length; ++index) {
            const key = keys[index]
            if (resting.hasOwnProperty(key) && source[key] !== undefined
                    && source[key] !== null)
                resting[key] = source[key]
        }
        return resting
    }

    function resolverEntry() {
        const source = entry || ({})
        const result = ({})
        const keys = Object.keys(source)
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = source[keys[index]]
        result.iconSource = iconSource
        return result
    }

    width: logicalSize
    height: logicalSize

    ShaderEffectSource {
        id: glyphTexture
        sourceItem: root.meshVisualActive ? glyphLayer : null
        hideSource: root.meshVisualActive
        live: root.meshVisualActive
        visible: false
        textureSize: Qt.size(Math.min(512, Math.max(1, root.logicalSize * 2)),
                             Math.min(512, Math.max(1, root.logicalSize * 2)))
    }
    ShaderEffectSource {
        id: tileTexture
        sourceItem: root.meshVisualActive ? baseLayer : null
        hideSource: root.meshVisualActive
        live: root.meshVisualActive
        visible: false
        textureSize: glyphTexture.textureSize
    }

    // The stand under the icon on a flat panel (the Pedestals page): a
    // horizontal platform seen a little from above, with a top face and a
    // side, painted in a material texture and tinted, so that an icon
    // stands on it even with the panel itself drawn at no opacity. The
    // stand has a little motion of its own.
    Item {
        id: pedestalLayer
        objectName: "icon-layer-pedestal"
        visible: root.pedestalDrawn
        anchors.horizontalCenter: parent.horizontalCenter
        // Its top face's middle just under the raised icon's foot (the glyph
        // keeps a small margin inside its cell).
        readonly property real topHeight: height * (root.pedestalKind === "column" ? 0.3 : 0.42)
        y: root.height / 2 + root.logicalSize / 2 - root.pedestalLiftPx - root.logicalSize * 0.07 - topHeight / 2
            + (root.pedestalAnimation === "bob" ? Math.sin(root.pedestalPhase * 2 * Math.PI) * root.logicalSize * 0.04 : 0)
        width: root.logicalSize * ({ column: 0.46, plate: 1.0, disc: 1.06, ring: 1.06, saucer: 1.1 }[root.pedestalKind] || 1.0)
            * root.pedestalSize
        height: width * ({ column: 0.9, plate: 0.42, disc: 0.34, ring: 0.36, saucer: 0.55 }[root.pedestalKind] || 0.42)
        opacity: root.pedestalAnimation === "pulse"
            ? 0.72 + 0.28 * (0.5 + 0.5 * Math.sin(root.pedestalPhase * 2 * Math.PI)) : 1
        scale: Math.max(0, root.visualScale)
        readonly property string look: root.pedestalMaterial === "panel" ? root.appearance : root.pedestalMaterial
        readonly property url textureSource: Qt.resolvedUrl("materials/" + look + ".png")
        readonly property string tint: root.pedestalColor.length ? root.pedestalColor
            : String(LayoutEngine.themeStyle(look, "", 48).stroke || "#b5d3e0")
        readonly property real spin: root.pedestalAnimation === "spin" ? root.pedestalPhase * 2 * Math.PI : 0

        Canvas {
            id: pedestalCanvas
            anchors.fill: parent
            property bool textureLoaded: false
            function loadTexture() {
                if (!isImageLoaded(pedestalLayer.textureSource)) loadImage(pedestalLayer.textureSource)
                textureLoaded = isImageLoaded(pedestalLayer.textureSource)
                requestPaint()
            }
            onImageLoaded: { textureLoaded = isImageLoaded(pedestalLayer.textureSource); requestPaint() }
            Component.onCompleted: loadTexture()
            Connections {
                target: pedestalLayer
                function onTextureSourceChanged() { pedestalCanvas.loadTexture() }
                function onTintChanged() { pedestalCanvas.requestPaint() }
                function onSpinChanged() { pedestalCanvas.requestPaint() }
                function onWidthChanged() { pedestalCanvas.requestPaint() }
                function onHeightChanged() { pedestalCanvas.requestPaint() }
            }
            Connections {
                target: root
                function onPedestalShapeChanged() { pedestalCanvas.requestPaint() }
            }
            // The top face: an ellipse (a ring for the ring shape) seen from
            // above at the dock's angle; `inset` shrinks it.
            function traceTop(context, kind, w, top, inset) {
                context.beginPath()
                const x = inset, y = inset, tw = w - 2 * inset, th = Math.max(1, top - 2 * inset)
                if (kind === "ring") {
                    context.ellipse(x, y, tw, th)
                    context.ellipse(x + tw * 0.3, y + th * 0.3, tw * 0.4, th * 0.4)
                } else {
                    context.ellipse(x, y, tw, th)
                }
                context.closePath()
            }
            // The lower half of an ellipse, left to right or back, as curves.
            function lowerHalf(context, cx, cy, rx, ry, leftToRight) {
                const k = 0.5523
                if (leftToRight) {
                    context.bezierCurveTo(cx - rx, cy + ry * k, cx - rx * k, cy + ry, cx, cy + ry)
                    context.bezierCurveTo(cx + rx * k, cy + ry, cx + rx, cy + ry * k, cx + rx, cy)
                } else {
                    context.bezierCurveTo(cx + rx, cy + ry * k, cx + rx * k, cy + ry, cx, cy + ry)
                    context.bezierCurveTo(cx - rx * k, cy + ry, cx - rx, cy + ry * k, cx - rx, cy)
                }
            }
            // The side: the band between the top face's lower rim and the
            // foot's lower rim.
            function traceSide(context, kind, w, h, top, outerInset) {
                const x = outerInset, tw = w - 2 * outerInset
                const rx = tw / 2, ry = top / 2, cx = x + rx
                const topY = ry, footY = h - ry
                context.beginPath()
                context.moveTo(x, topY)
                context.lineTo(x, footY)
                lowerHalf(context, cx, footY, rx, ry, true)
                context.lineTo(x + tw, topY)
                lowerHalf(context, cx, topY, rx, ry, false)
                context.closePath()
            }
            function fillTextured(context, w, h, alpha) {
                if (textureLoaded) {
                    context.save()
                    context.translate(w / 2, h / 2)
                    context.rotate(pedestalLayer.spin)
                    const s = Math.max(w, h) / 512 * 1.1
                    context.scale(s, s)
                    context.fillStyle = context.createPattern(String(pedestalLayer.textureSource), "repeat")
                    context.fillRect(-512, -512, 1024, 1024)
                    context.restore()
                    context.globalCompositeOperation = "qt-multiply"
                }
                context.globalAlpha = alpha
                context.fillStyle = pedestalLayer.tint
                context.fillRect(0, 0, w, h)
                context.globalAlpha = 1
                context.globalCompositeOperation = "source-over"
            }
            onPaint: {
                const context = getContext("2d")
                context.reset()
                const w = width, h = height
                if (!(w > 0 && h > 0)) return
                const kind = root.pedestalKind
                const top = pedestalLayer.topHeight
                // The ground shadow under the foot.
                context.save()
                context.beginPath()
                context.ellipse(w * 0.06, h - top * 0.55, w * 0.88, top * 0.6)
                context.fillStyle = "rgba(0,0,0,0.3)"
                context.fill()
                context.restore()
                // The side, darker, then the top face.
                context.save()
                traceSide(context, kind, w, h, top, kind === "saucer" ? w * 0.12 : 0)
                context.clip()
                fillTextured(context, w, h, 1)
                context.fillStyle = "rgba(0,0,0,0.38)"
                context.fillRect(0, 0, w, h)
                context.restore()
                context.save()
                traceTop(context, kind, w, top, 0)
                context.clip("evenodd")
                fillTextured(context, w, h, 1)
                context.restore()
                if (kind === "saucer") {
                    // The dome on top, in the same material.
                    context.save()
                    context.beginPath()
                    context.ellipse(w * 0.3, top * 0.05, w * 0.4, top * 0.7)
                    context.clip()
                    fillTextured(context, w, h, 1)
                    context.fillStyle = "rgba(255,255,255,0.14)"
                    context.fillRect(0, 0, w, h)
                    context.restore()
                }
                if (kind === "disc") {
                    context.beginPath()
                    context.ellipse(w * 0.2, top * 0.25, w * 0.6, top * 0.5)
                    context.strokeStyle = "rgba(255,255,255,0.3)"
                    context.stroke()
                }
                // Polished rims.
                context.lineWidth = 1
                context.strokeStyle = "rgba(255,255,255,0.45)"
                traceTop(context, kind, w, top, 0)
                context.stroke()
            }
        }
    }

    Item {
        id: visualLayer

        objectName: "icon-visual-layer"
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -root.pedestalLiftPx
        width: root.logicalSize
        height: root.logicalSize
        scale: Math.max(0, root.visualScale)

        Item {
            id: rearLayer

            objectName: "icon-layer-rear"
            anchors.fill: parent

            IconStyle2D {
                id: styleShadow

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "shadow"
                logicalSize: root.logicalSize
                roleOpacity: root.styleState.rearOpacity === undefined
                    ? 1 : Number(root.styleState.rearOpacity)
                visible: root.styledLayersActive
            }

            IconStyle2D {
                id: styleGlow

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "glow"
                logicalSize: root.logicalSize
                roleOpacity: Math.max(
                    Number(root.styleState.glowOpacity || 0),
                    root.glowAmount)
                stateGlowColor: String(
                    root.styleState.glowColor || "transparent")
                visible: root.styledLayersActive
            }

            IconStyle2D {
                id: styleRear

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "rear"
                logicalSize: root.logicalSize
                roleOpacity: root.styleState.rearOpacity === undefined
                    ? 1 : Number(root.styleState.rearOpacity)
                visible: root.styledLayersActive
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 0.98
                height: width
                radius: width / 2
                color: "transparent"
                border.width: root.dropTarget ? 3
                    : root.urgent ? 2
                    : root.glowAmount * 3
                border.color: root.dropTarget
                    ? root.alphaColor(Kirigami.Theme.positiveTextColor, 0.9)
                    : root.urgent
                        ? root.alphaColor(
                            Kirigami.Theme.negativeTextColor, 0.85)
                        : root.alphaColor(
                            Kirigami.Theme.highlightColor,
                            root.glowAmount * 0.75)
                visible: !root.styledLayersActive
                    && (root.dropTarget || root.urgent
                        || root.glowAmount > 0.01)

                SequentialAnimation on opacity {
                    running: root.glowAnimating && !root.reducedMotion
                    loops: Animation.Infinite
                    OpacityAnimator {
                        from: 0.25
                        to: 1
                        duration: root.glowDuration
                    }
                    OpacityAnimator {
                        from: 1
                        to: 0.25
                        duration: root.glowDuration
                    }
                }
            }
        }

        // Mask source for the tile silhouette. Kept outside baseLayer so it
        // is not consumed by the layer it masks.
        Image {
            id: styleMaskImage

            objectName: "icon-style-mask-source"
            anchors.fill: parent
            visible: false
            asynchronous: false
            cache: true
            fillMode: Image.PreserveAspectFit
            source: root.currentStyleMaskSource
            onStatusChanged: {
                if (status === Image.Error)
                    root.recordStyleMaskFailure(source.toString())
            }
        }

        Item {
            id: baseLayer

            objectName: "icon-layer-base"
            anchors.fill: parent
            opacity: root.resolvedTileMotion.opacity
            transform: [
                Scale {
                    origin.x: baseLayer.width / 2
                    origin.y: baseLayer.height / 2
                    xScale: root.resolvedTileMotion.scale
                        * root.resolvedTileMotion.scaleX
                    yScale: root.resolvedTileMotion.scale
                        * root.resolvedTileMotion.scaleY
                },
                Rotation {
                    origin.x: baseLayer.width / 2
                    origin.y: baseLayer.height / 2
                    angle: root.resolvedTileMotion.rotateZ
                },
                Translate {
                    x: root.resolvedTileMotion.x
                    y: root.resolvedTileMotion.y
                }
            ]

            // MultiEffect is instantiated only when a mask is actually
            // declared and its source loaded.
            layer.enabled: root.styleMaskActive
                && styleMaskImage.status === Image.Ready
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: styleMaskImage
            }

            IconStyle2D {
                id: styleBase

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "base"
                tileSettings: Object.assign({}, root.tileSettings, {
                    iconTileThickness: root.meshVisualActive ? 0 : root.tileValue("iconTileThickness", 0, 0, 24),
                    iconTileBevel: 0
                })
                logicalSize: root.logicalSize
                roleOpacity: root.styleState.baseOpacity === undefined
                    ? 1 : Number(root.styleState.baseOpacity)
                visible: root.styledLayersActive
            }

            IconTile {
                objectName: "icon-custom-tile"
                anchors.centerIn: parent
                anchors.alignWhenCentered: false
                width: Math.max(0, root.logicalSize - 2 * Math.max(1, root.logicalSize * 0.04))
                    * Number(root.resolvedIconStyle.diameter || 1)
                height: width
                visible: root.tileRenderingEnabled && (root.customTileActive
                    || (!root.styledLayersActive && (String(root.tileSettings.iconTileTexture || "none") !== "none"
                        || root.tileValue("iconTileThickness", 0, 0, 24) > 0)))
                texture: String(root.tileSettings.iconTileTexture || "none")
                thickness: root.meshVisualActive ? 0 : root.tileValue("iconTileThickness", 0, 0, 24)
                bevel: 0
                shape: root.resolvedIconStyle.iconShape || root.tileShape
                fillColor: String(root.tileSettings.iconTileColor || "#334155")
                borderColor: String(root.tileSettings.iconTileBorderColor || "#94a3b8")
                borderWidth: root.tileSettings.iconTileBorderWidth === undefined ? 1
                    : Math.max(0, Math.min(8, Number(root.tileSettings.iconTileBorderWidth)))
                opacity: root.tileSettings.iconTileOpacity === undefined ? 0.8
                    : Math.max(0, Math.min(1, Number(root.tileSettings.iconTileOpacity)))
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: Math.max(1, root.logicalSize * 0.04)
                radius: root.tileRadius()
                visible: root.tileRenderingEnabled && !root.customTileActive && !root.styledLayersActive
                    && (root.appearance === "plate"
                        || root.appearance === "platform"
                        || root.appearance === "floating-glass"
                        || root.hovered || root.active || root.dropTarget)
                color: root.active
                    ? root.alphaColor(Kirigami.Theme.highlightColor, 0.28)
                    : root.dropTarget
                        ? root.alphaColor(
                            Kirigami.Theme.positiveTextColor, 0.22)
                        : root.alphaColor(
                            Kirigami.Theme.backgroundColor,
                            root.hovered ? 0.42 : 0.22)
                border.width: 1
                border.color: root.alphaColor(
                    Kirigami.Theme.textColor, root.hovered ? 0.28 : 0.12)
                scale: root.pressed ? 0.92 : 1
            }

            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                width: parent.width * 0.92
                height: parent.height * 0.32
                visible: root.tileRenderingEnabled && !root.customTileActive && !root.styledLayersActive
                    && root.appearance === "pedestal"

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: parent.height * 0.52
                    radius: height / 2
                    color: root.alphaColor(
                        Kirigami.Theme.backgroundColor, 0.72)
                    border.width: 1
                    border.color: root.alphaColor(
                        Kirigami.Theme.textColor, 0.28)
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: parent.height * 0.34
                    width: parent.width * 0.38
                    height: parent.height * 0.56
                    radius: width * 0.22
                    color: root.alphaColor(
                        Kirigami.Theme.backgroundColor, 0.62)
                }
            }
        }

        Item {
            id: glyphLayer

            objectName: "icon-layer-glyph"
            anchors.fill: parent
            opacity: root.resolvedGlyphMotion.opacity
            transform: [
                // The turn comes first so scale, spin and displacement act on
                // the already-turned card rather than fighting it.
                Matrix4x4 {
                    matrix: root.glyphTurnActive
                        ? MotionChannels.turnMatrix(
                            root.glyphTurnAngle, glyphLayer.width,
                            glyphLayer.height, root.logicalSize * 2.4)
                        : Qt.matrix4x4()
                },
                Scale {
                    origin.x: glyphLayer.width / 2
                    origin.y: glyphLayer.height / 2
                    xScale: root.resolvedGlyphMotion.scale
                        * root.resolvedGlyphMotion.scaleX
                    yScale: root.resolvedGlyphMotion.scale
                        * root.resolvedGlyphMotion.scaleY
                },
                Rotation {
                    origin.x: glyphLayer.width / 2
                    origin.y: glyphLayer.height / 2
                    angle: root.resolvedGlyphMotion.rotateZ
                },
                Translate {
                    x: root.resolvedGlyphMotion.x
                    y: root.resolvedGlyphMotion.y
                }
            ]

            Kirigami.Icon {
                id: glyph

                anchors.centerIn: parent
                anchors.alignWhenCentered: false
                anchors.horizontalCenterOffset: root.tileRenderingEnabled
                    ? root.tileValue("iconTileIconOffsetX", 0, -40, 40) : 0
                anchors.verticalCenterOffset: root.tileRenderingEnabled
                    ? root.tileValue("iconTileIconOffsetY", 0, -40, 40) : 0
                width: root.styleGlyphTreatmentActive
                    ? root.logicalSize * Math.max(
                        0.1, 1 - Number(root.styleInset.left || 0)
                            - Number(root.styleInset.right || 0))
                    : root.logicalSize * 0.72
                height: root.styleGlyphTreatmentActive
                    ? root.logicalSize * Math.max(
                        0.1, 1 - Number(root.styleInset.top || 0)
                            - Number(root.styleInset.bottom || 0))
                    : width
                source: root.resolvedIconSource
                // PD-20: the owner chooses a continuous logo size. Native
                // bucket rounding shrinks e.g. a 59 px logo to 48 px.
                roundToIconSize: false

                // Kirigami's native monochrome path: only ever reached for a
                // glyph the resolver proved safe to recolor.
                isMask: root.glyphIsMask
                color: root.glyphIsMask ? root.glyphTint : "transparent"

                // State styling applies to the plain fallback too; the
                // resolver supplies per-state values in both cases.
                opacity: Number(root.styleState.glyphOpacity === undefined
                                ? 1 : root.styleState.glyphOpacity)
                scale: Number(root.styleState.glyphScale === undefined
                              ? 1 : root.styleState.glyphScale)
                    * (root.tileRenderingEnabled ? root.tileValue("iconTileIconScale", 100, 25, 150) / 100 : 1)

                // MultiEffect is instantiated only when a tint is requested.
                layer.enabled: root.glyphTreatment === "tinted"
                    && root.glyphTint.length > 0
                layer.effect: MultiEffect {
                    colorization: 1
                    colorizationColor: root.glyphTint
                }
            }

            Kirigami.Icon {
                anchors.horizontalCenter: glyph.horizontalCenter
                anchors.top: glyph.bottom
                anchors.topMargin: -root.logicalSize * 0.08
                width: glyph.width
                height: glyph.height * 0.28
                source: glyph.source
                opacity: !root.styledLayersActive && root.showReflection
                    ? 0.16 : 0
                transform: Scale {
                    yScale: -0.28
                    origin.y: 0
                }
            }

            IconStyle2D {
                id: styleReflection

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "reflection"
                logicalSize: root.logicalSize
                roleOpacity: Number(
                    root.styleState.reflectionOpacity || 0)
                visible: root.styledLayersActive && root.showReflection
            }

            // Lighting for the turn: a soft band that sweeps across the card as
            // it rotates. It is derived from the turn itself, so it cannot fall
            // out of step, and it is completely absent while the card rests.
            Item {
                id: glyphHighlight

                objectName: "icon-layer-glyph-highlight"
                anchors.fill: parent
                visible: root.glyphTurnActive
                opacity: Math.min(0.42, Math.abs(
                    root.resolvedGlyphMotion.highlight) * 0.42)

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    height: parent.height
                    width: Math.max(1, parent.width * 0.5
                        * MotionChannels.turnCompression(root.glyphTurnAngle))
                    x: (parent.width - width) / 2
                        + root.resolvedGlyphMotion.highlight
                            * parent.width * 0.3
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.5; color: "#ffffff" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }
                }
            }
        }

        Item {
            id: frontLayer

            objectName: "icon-layer-front"
            anchors.fill: parent

            IconStyle2D {
                id: styleFront

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "front"
                logicalSize: root.logicalSize
                roleOpacity: root.styleState.frontOpacity === undefined
                    ? 1 : Number(root.styleState.frontOpacity)
                stateBorderColor: String(
                    root.styleState.borderColor || "transparent")
                visible: root.styledLayersActive
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: root.tileRadius()
                color: "transparent"
                border.width: root.editMode || root.dropTarget ? 2 : 0
                border.color: root.editMode
                    ? Kirigami.Theme.neutralTextColor
                    : Kirigami.Theme.positiveTextColor
                visible: !root.styledLayersActive && border.width > 0
            }
        }

        RunningIndicator {
            id: runningIndicator

            visible: root.showIndicator
                && (root.running || root.active || root.launching)
                && Number(root.styleState.indicatorOpacity === undefined
                          ? 1 : root.styleState.indicatorOpacity) > 0
            opacity: Number(root.styleState.indicatorOpacity === undefined
                            ? 1 : root.styleState.indicatorOpacity)
            vertical: root.vertical
            active: root.active
            urgent: root.urgent
            windowCount: root.windowCount
            reducedMotion: root.reducedMotion
            style: root.resolvedIndicatorStyle
            anchors.horizontalCenter: root.vertical
                ? undefined : parent.horizontalCenter
            anchors.verticalCenter: root.vertical
                ? parent.verticalCenter : undefined
            anchors.bottom: root.vertical ? undefined : parent.bottom
            anchors.left: root.vertical ? parent.left : undefined
            transform: [
                Scale {
                    origin.x: runningIndicator.width / 2
                    origin.y: runningIndicator.height / 2
                    xScale: root.resolvedIndicatorMotion.scale
                        * root.resolvedIndicatorMotion.scaleX
                    yScale: root.resolvedIndicatorMotion.scale
                        * root.resolvedIndicatorMotion.scaleY
                },
                Translate {
                    x: root.resolvedIndicatorMotion.x
                    y: root.resolvedIndicatorMotion.y
                }
            ]
        }

        Item {
            id: statusLayer

            objectName: "icon-layer-status"
            anchors.fill: parent

            Rectangle {
                id: attentionMarker
                objectName: "icon-attention-marker"
                visible: root.urgent
                anchors.left: parent.left
                anchors.top: parent.top
                width: Math.max(8, root.logicalSize * 0.17)
                height: width
                radius: width / 2
                color: Kirigami.Theme.neutralTextColor
            }

            Rectangle {
                id: temporaryNotice
                objectName: "icon-temporary-status"
                visible: String(root.entry.temporaryStatus || "").length > 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(14, root.logicalSize * 0.25)
                height: width
                radius: width / 2
                color: Kirigami.Theme.backgroundColor
                Text {
                    anchors.centerIn: parent
                    text: "!"
                    font.bold: true
                    color: Kirigami.Theme.textColor
                }
            }

            Rectangle {
                visible: root.entry.isStatus === true
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: statusLabel.implicitHeight + 4
                radius: 3
                color: Kirigami.Theme.backgroundColor
                Text {
                    id: statusLabel
                    objectName: "icon-status-reading"
                    anchors.fill: parent
                    anchors.margins: 2
                    text: String(root.entry.statusText || "")
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    color: root.entry.statusAvailable === true
                        ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                    font.pixelSize: Math.max(9, root.logicalSize * 0.2)
                }
            }

            Rectangle {
                id: badge

                visible: root.badgeSlotActive
                anchors.top: parent.top
                anchors.right: parent.right
                width: Math.max(height, badgeLabel.implicitWidth
                    + Kirigami.Units.smallSpacing)
                height: Math.max(14, root.logicalSize * 0.28)
                radius: height / 2
                color: Kirigami.Theme.highlightColor

                Text {
                    id: badgeLabel

                    anchors.centerIn: parent
                    text: root.badgeText
                    color: Kirigami.Theme.highlightedTextColor
                    font.bold: true
                    font.pixelSize: Math.max(9, parent.height * 0.64)
                }
            }

            Rectangle {
                id: progressTrack

                visible: root.progressSlotActive
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Math.max(2, root.logicalSize * 0.08)
                height: Math.max(2, root.logicalSize * 0.05)
                radius: height / 2
                color: root.alphaColor(Kirigami.Theme.textColor, 0.2)

                Rectangle {
                    width: parent.width * Math.max(0, Math.min(1,
                        root.progress))
                    height: parent.height
                    radius: parent.radius
                    color: Kirigami.Theme.highlightColor
                }
            }
        }
    }
}
