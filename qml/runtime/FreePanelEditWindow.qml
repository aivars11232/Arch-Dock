import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.kde.layershell as LayerShellQt
import "DockGeometry.js" as DockGeometry

Window {
    id: root

    property string panelId: ""
    readonly property int revision: panelRegistry.revision
    readonly property string panelType: value("type", "hybrid")
    readonly property string layout: value("layout", "circular")
    readonly property int entryCount: dockModel.panelEntryCount(panelType)
    readonly property real iconSize: value("iconSize", 52)
    readonly property real layoutScale: value("layoutScale", 1)
    readonly property real radiusValue: value("layoutRadius", 145)
    readonly property int polygonSides: value("pathSides", 6)
    readonly property var geometry: DockGeometry.metrics(
        layout, entryCount, iconSize, value("spacing", 8), layoutScale,
        radiusValue, value("layoutRows", 2), value("layoutPadding", 18),
        false, value("layoutAngle", 0), polygonSides)

    function value(key, fallback) {
        const currentRevision = revision;
        const candidate = panelRegistry.panelValue(panelId, key);
        return candidate === undefined || candidate === null ? fallback : candidate;
    }

    width: Math.max(160, value("width", Math.ceil(geometry.width)))
    height: Math.max(160, value("height", Math.ceil(geometry.height)))
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    title: "Arch Dock Free Panel Edit Proxy — " + panelId

    LayerShellQt.Window.layer: LayerShellQt.Window.LayerOverlay
    LayerShellQt.Window.anchors: LayerShellQt.Window.AnchorTop | LayerShellQt.Window.AnchorLeft
    LayerShellQt.Window.margins: Qt.margins(value("x", 240), value("y", 180), 0, 0)
    LayerShellQt.Window.exclusionZone: -1
    LayerShellQt.Window.keyboardInteractivity: LayerShellQt.Window.KeyboardInteractivityNone
    LayerShellQt.Window.scope: "arch-dock-edit"

    Canvas {
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
            context.strokeStyle = "#78334862";
            context.shadowColor = "#55000000";
            context.shadowBlur = 18;
            context.beginPath();
            if (root.layout === "ellipse") {
                context.ellipse(centerX, centerY, radius, radius * 0.62, 0, 0, Math.PI * 2);
            } else if (root.layout === "arc" || root.layout === "semicircle" ||
                       root.layout === "fan") {
                const sweep = root.layout === "semicircle" ? Math.PI : Math.PI * 0.72;
                context.arc(centerX, centerY + radius * 0.45, radius,
                            Math.PI * 1.5 - sweep / 2, Math.PI * 1.5 + sweep / 2);
            } else {
                context.arc(centerX, centerY, radius, 0, Math.PI * 2);
            }
            context.stroke();
        }
    }

    Repeater {
        model: dockModel

        delegate: Item {
            required property int index
            required property string iconName
            required property bool active
            readonly property bool included: dockModel.panelEntryMatches(index, root.panelType)
            readonly property int pathIndex: dockModel.panelEntryPosition(index, root.panelType)
            readonly property var point: DockGeometry.position(
                root.layout, pathIndex, root.entryCount, root.geometry,
                root.value("layoutAngle", 0), root.polygonSides,
                root.value("pathOrientation", "upright"))

            visible: included
            width: root.iconSize
            height: root.iconSize
            x: (root.width - root.geometry.width) / 2 + point.x
            y: (root.height - root.geometry.height) / 2 + point.y
            rotation: point.rotation

            Rectangle {
                anchors.fill: parent
                radius: root.value("iconShape", "rounded") === "circle" ? width / 2 : width * 0.24
                color: parent.active ? "#663daee9" : "#28303b48"
                border.width: 1
                border.color: "#3fffffff"
            }

            Kirigami.Icon {
                anchors.centerIn: parent
                width: parent.width * 0.72
                height: width
                source: parent.iconName || "application-x-executable"
            }
        }
    }
}
