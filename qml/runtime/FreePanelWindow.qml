import QtQuick
import QtQuick.Controls
import QtQuick.Window
import org.kde.kirigami as Kirigami
import "DockGeometry.js" as DockGeometry

Window {
    id: root

    property string panelId: ""
    readonly property int revision: panelRegistry.revision
    readonly property string panelType: value("type", "hybrid")
    readonly property string layout: value("layout", "circular")
    readonly property int entryCount: dockModel.panelEntryCount(panelType)
    readonly property real iconSize: value("iconSize", 52)
    readonly property real spacing: value("spacing", 8)
    readonly property real radiusValue: value("layoutRadius", 145)
    readonly property real layoutScale: value("layoutScale", 1)
    readonly property real layoutAngle: value("layoutAngle", 0)
    readonly property int polygonSides: value("pathSides", 6)
    readonly property string pathOrientation: value("pathOrientation", "upright")
    readonly property bool editMode: panelController.plasmaEditMode
    readonly property var geometry: DockGeometry.metrics(
        layout, entryCount, iconSize, spacing, layoutScale, radiusValue,
        value("layoutRows", 2), value("layoutPadding", 18), false,
        layoutAngle, polygonSides)
    property bool initialized: false
    property bool moving: false

    function value(key, fallback) {
        const currentRevision = revision;
        const candidate = panelRegistry.panelValue(panelId, key);
        return candidate === undefined || candidate === null ? fallback : candidate;
    }

    width: Math.max(160, value("width", Math.ceil(geometry.width)))
    height: Math.max(160, value("height", Math.ceil(geometry.height)))
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnBottomHint
    title: "Arch Dock Free Panel — " + panelId

    Component.onCompleted: {
        x = value("x", 240);
        y = value("y", 180);
        initialized = true;
    }

    onXChanged: positionSave.restart()
    onYChanged: positionSave.restart()

    Timer {
        id: positionSave
        interval: 350
        onTriggered: {
            if (root.initialized)
                panelController.saveFreePanelPosition(root.panelId, root.x, root.y);
        }
    }

    Canvas {
        id: surface
        anchors.fill: parent
        opacity: root.value("opacity", 0.9)
        onPaint: {
            const context = getContext("2d");
            context.reset();
            const centerX = width / 2;
            const centerY = height / 2;
            const radius = Math.min(root.radiusValue * root.layoutScale,
                                    Math.min(width, height) / 2 - root.iconSize / 2);
            context.lineWidth = Math.max(18, root.iconSize * 0.48);
            const appearance = root.value("appearance", "glass");
            context.strokeStyle = appearance === "neon" ? "#a850e6ff"
                : appearance === "metallic" ? "#b8aeb9c4"
                : appearance === "futuristic" ? "#b030d8ff"
                : appearance === "organic" ? "#9c57c98b"
                : appearance === "platform" ? "#b86e7884"
                : "#78334862";
            context.shadowColor = appearance === "neon" || appearance === "futuristic"
                ? "#8a35cfff" : "#55000000";
            context.shadowBlur = appearance === "minimal" ? 0 : 18;
            context.beginPath();
            if (root.layout === "arc" || root.layout === "semicircle" || root.layout === "fan") {
                const sweep = root.layout === "semicircle" ? Math.PI : Math.PI * 0.72;
                context.arc(centerX, centerY + radius * 0.45, radius,
                            Math.PI * 1.5 - sweep / 2, Math.PI * 1.5 + sweep / 2);
            } else if (root.layout === "ellipse") {
                context.ellipse(centerX, centerY, radius, radius * 0.62, 0, 0, Math.PI * 2);
            } else if (root.layout === "spiral") {
                for (let step = 0; step <= 80; ++step) {
                    const progress = step / 80;
                    const angle = -Math.PI / 2 + progress * Math.PI * 5;
                    const distance = radius * (0.2 + progress * 0.8);
                    const px = centerX + Math.cos(angle) * distance;
                    const py = centerY + Math.sin(angle) * distance;
                    if (step === 0) context.moveTo(px, py); else context.lineTo(px, py);
                }
            } else if (["polygon", "triangle", "square", "pentagon", "hexagon",
                        "octagon", "star"].includes(root.layout)) {
                const sides = root.layout === "triangle" ? 3
                    : root.layout === "square" ? 4
                    : root.layout === "pentagon" ? 5
                    : root.layout === "hexagon" ? 6
                    : root.layout === "octagon" ? 8 : root.polygonSides;
                const vertices = root.layout === "star" ? sides * 2 : sides;
                for (let side = 0; side <= vertices; ++side) {
                    const angle = -Math.PI / 2 + side * Math.PI * 2 / vertices;
                    const pointRadius = root.layout === "star" && side % 2 === 1
                        ? radius * 0.46 : radius;
                    const px = centerX + Math.cos(angle) * pointRadius;
                    const py = centerY + Math.sin(angle) * pointRadius;
                    if (side === 0) context.moveTo(px, py); else context.lineTo(px, py);
                }
            } else if (root.layout === "ribbon" || root.layout === "horizontal-curve") {
                context.moveTo(root.iconSize / 2, centerY);
                context.bezierCurveTo(width * 0.28, centerY - root.iconSize,
                                      width * 0.72, centerY + root.iconSize,
                                      width - root.iconSize / 2, centerY);
            } else if (root.layout === "vertical-curve") {
                context.moveTo(centerX, root.iconSize / 2);
                context.bezierCurveTo(centerX + root.iconSize, height * 0.28,
                                      centerX - root.iconSize, height * 0.72,
                                      centerX, height - root.iconSize / 2);
            } else {
                context.arc(centerX, centerY, radius, 0, Math.PI * 2);
            }
            context.stroke();

            if (appearance === "platform" || appearance === "pedestal") {
                context.shadowBlur = 8;
                context.fillStyle = appearance === "pedestal" ? "#7037424c" : "#78465462";
                context.beginPath();
                context.ellipse(centerX, height * 0.82, radius * 0.72,
                                Math.max(12, root.iconSize * 0.2), 0, 0, Math.PI * 2);
                context.fill();
            }
        }
        Connections {
            target: root
            function onRevisionChanged() { surface.requestPaint(); }
            function onLayoutChanged() { surface.requestPaint(); }
            function onWidthChanged() { surface.requestPaint(); }
            function onHeightChanged() { surface.requestPaint(); }
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: root.editMode
        color: "transparent"
        radius: 18
        border.width: 2
        border.color: "#78a9dcff"
        opacity: 0.9
    }

    Repeater {
        model: dockModel

        delegate: Item {
            id: entry
            required property int index
            required property string iconName
            required property string displayName
            required property bool active
            required property bool minimized

            readonly property bool included: dockModel.panelEntryMatches(index, root.panelType)
            readonly property int pathIndex: dockModel.panelEntryPosition(index, root.panelType)
            readonly property var point: DockGeometry.position(
                root.layout, pathIndex, root.entryCount, root.geometry,
                root.layoutAngle, root.polygonSides, root.pathOrientation)

            visible: included
            width: root.iconSize
            height: root.iconSize
            x: (root.width - root.geometry.width) / 2 + point.x
            y: (root.height - root.geometry.height) / 2 + point.y
            rotation: point.rotation

            Rectangle {
                anchors.fill: parent
                radius: root.value("iconShape", "rounded") === "circle" ? width / 2 : width * 0.24
                color: entry.active ? "#663daee9" : mouse.containsMouse ? "#49384e60" : "#28303b48"
                border.width: 1
                border.color: mouse.containsMouse ? "#8be2f5ff" : "#3fffffff"
                scale: mouse.containsMouse ? 1.18 : 1
                Behavior on scale {
                    NumberAnimation { duration: 140; easing.type: Easing.OutBack }
                }
            }

            Kirigami.Icon {
                anchors.centerIn: parent
                width: parent.width * 0.72
                height: width
                source: entry.iconName || "application-x-executable"
                opacity: entry.minimized ? 0.55 : 1
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: !root.editMode
                onClicked: dockModel.activate(entry.index)
            }
            ToolTip.visible: mouse.containsMouse
            ToolTip.text: entry.displayName
        }
    }

    Rectangle {
        id: moveHandle
        anchors.centerIn: parent
        width: 54
        height: 54
        radius: width / 2
        color: handleMouse.containsMouse || root.moving ? "#b8324658" : "#78303d49"
        border.width: 1
        border.color: "#86d8efff"
        visible: root.editMode

        Kirigami.Icon {
            anchors.centerIn: parent
            width: 24
            height: width
            source: "transform-move"
        }

        MouseArea {
            id: handleMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeAllCursor
            onPressed: {
                root.moving = true;
                root.startSystemMove();
            }
            onReleased: root.moving = false
            onCanceled: root.moving = false
        }
        ToolTip.visible: handleMouse.containsMouse
        ToolTip.text: qsTr("Drag free panel")
    }

    Column {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 8
        spacing: 8
        visible: root.editMode

        RoundButton {
            icon.name: "settings-configure"
            display: AbstractButton.IconOnly
            onClicked: panelController.showPanelSettings(root.panelId)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Configure free panel")
        }

        RoundButton {
            icon.name: "edit-delete"
            display: AbstractButton.IconOnly
            onClicked: panelController.removePanel(root.panelId)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Remove free panel")
        }
    }
}
