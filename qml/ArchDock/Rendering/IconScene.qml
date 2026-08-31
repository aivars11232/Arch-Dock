import QtQuick
import org.kde.kirigami as Kirigami

Item {
    id: root

    property var entry: ({})
    property var iconStyleDefinition: ({})
    property real logicalSize: 52
    property real visualScale: 1
    property string tileShape: "rounded"
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
        resolverEntry())
    readonly property string visualState: resolvedIconStyle.stateId
    readonly property var styleState: resolvedIconStyle.state || ({})
    readonly property var styleInset:
        resolvedIconStyle.safeGlyphInset || ({
            left: 0, top: 0, right: 0, bottom: 0
        })
    readonly property bool styledLayersActive:
        Boolean(resolvedIconStyle.renderStyledLayers)
    readonly property bool styleGlyphTreatmentActive:
        Boolean(resolvedIconStyle.valid)
        && String(resolvedIconStyle.styleId || "plain-original")
            !== "plain-original"
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

    function alphaColor(color, alpha) {
        return Qt.rgba(color.r, color.g, color.b, alpha)
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

    Item {
        id: visualLayer

        objectName: "icon-visual-layer"
        anchors.centerIn: parent
        width: root.logicalSize
        height: root.logicalSize
        scale: Math.max(0, root.visualScale)

        Item {
            id: rearLayer

            objectName: "icon-layer-rear"
            anchors.fill: parent

            IconStyle2D {
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

        Item {
            id: baseLayer

            objectName: "icon-layer-base"
            anchors.fill: parent

            IconStyle2D {
                id: styleBase

                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "base"
                logicalSize: root.logicalSize
                roleOpacity: root.styleState.baseOpacity === undefined
                    ? 1 : Number(root.styleState.baseOpacity)
                visible: root.styledLayersActive
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: Math.max(1, root.logicalSize * 0.04)
                radius: root.tileRadius()
                visible: root.tileRenderingEnabled && !root.styledLayersActive
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
                visible: root.tileRenderingEnabled && !root.styledLayersActive
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

            Kirigami.Icon {
                id: glyph

                anchors.centerIn: parent
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
                opacity: root.styleGlyphTreatmentActive
                    ? Number(root.styleState.glyphOpacity === undefined
                             ? 1 : root.styleState.glyphOpacity)
                    : root.minimized ? 0.52 : 1
                scale: root.styleGlyphTreatmentActive
                    ? Number(root.styleState.glyphScale === undefined
                             ? 1 : root.styleState.glyphScale)
                    : 1
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
                anchors.fill: parent
                styleDefinition: root.effectiveIconStyleDefinition
                resolvedStyle: root.resolvedIconStyle
                role: "reflection"
                logicalSize: root.logicalSize
                roleOpacity: Number(
                    root.styleState.reflectionOpacity || 0)
                visible: root.styledLayersActive && root.showReflection
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
        }

        Item {
            id: statusLayer

            objectName: "icon-layer-status"
            anchors.fill: parent

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
