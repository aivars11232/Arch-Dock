import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

Window {
    id: root

    property var settings: dockSettings
    property string selectedPanelId: panelRegistry.activePanelId
    property bool showPanels: false
    readonly property int panelRevision: panelRegistry.revision
    readonly property int screenRevision: panelController.screenRevision
    readonly property var screenOptions: {
        const revision = screenRevision;
        return panelController.availableScreens();
    }
    readonly property var kdeWidgetIds: panelController.availableKdeWidgets()
    readonly property var visibilityOptions: [
        { label: qsTr("Always visible"), value: "always" },
        { label: qsTr("Auto-hide"), value: "auto-hide" },
        { label: qsTr("Dodge active window"), value: "dodge" },
        { label: qsTr("Hide under active window"), value: "cover" }
    ]
    readonly property var pathAnchorOptions: [
        { label: qsTr("Top left"), value: "top-left" },
        { label: qsTr("Top"), value: "top" },
        { label: qsTr("Top right"), value: "top-right" },
        { label: qsTr("Left"), value: "left" },
        { label: qsTr("Center"), value: "center" },
        { label: qsTr("Right"), value: "right" },
        { label: qsTr("Bottom left"), value: "bottom-left" },
        { label: qsTr("Bottom"), value: "bottom" },
        { label: qsTr("Bottom right"), value: "bottom-right" }
    ]
    readonly property var pathOrientationOptions: [
        { label: qsTr("Keep icons upright"), value: "upright" },
        { label: qsTr("Follow path tangent"), value: "tangent" },
        { label: qsTr("Point away from center"), value: "radial" }
    ]
    readonly property var layoutOptions: [
        { label: qsTr("Adaptive row / column"), value: "adaptive" },
        { label: qsTr("Horizontal"), value: "horizontal" },
        { label: qsTr("Vertical"), value: "vertical" },
        { label: qsTr("Diagonal"), value: "diagonal" },
        { label: qsTr("Circular"), value: "circular" },
        { label: qsTr("Ellipse"), value: "ellipse" },
        { label: qsTr("Ring"), value: "ring" },
        { label: qsTr("Radial"), value: "radial" },
        { label: qsTr("Arc"), value: "arc" },
        { label: qsTr("Semicircle"), value: "semicircle" },
        { label: qsTr("Fan"), value: "fan" },
        { label: qsTr("Spiral"), value: "spiral" },
        { label: qsTr("Ribbon"), value: "ribbon" },
        { label: qsTr("Horizontal curve"), value: "horizontal-curve" },
        { label: qsTr("Vertical curve"), value: "vertical-curve" },
        { label: qsTr("Polygon path"), value: "polygon" },
        { label: qsTr("Triangle"), value: "triangle" },
        { label: qsTr("Square"), value: "square" },
        { label: qsTr("Pentagon"), value: "pentagon" },
        { label: qsTr("Hexagon"), value: "hexagon" },
        { label: qsTr("Octagon"), value: "octagon" },
        { label: qsTr("Star"), value: "star" },
        { label: qsTr("Multi-row grid"), value: "grid" },
        { label: qsTr("Floating cluster"), value: "floating" }
    ]
    readonly property var materialOptions: [
        { label: qsTr("Glass"), value: "glass" },
        { label: qsTr("Crystal"), value: "crystal" },
        { label: qsTr("Neon"), value: "neon" },
        { label: qsTr("Minimal"), value: "minimal" },
        { label: qsTr("Plasma"), value: "plasma" },
        { label: qsTr("Lime"), value: "lime" },
        { label: qsTr("Floating glass"), value: "floating-glass" },
        { label: qsTr("Metallic"), value: "metallic" },
        { label: qsTr("Futuristic"), value: "futuristic" },
        { label: qsTr("Organic"), value: "organic" },
        { label: qsTr("Platform bases"), value: "platform" },
        { label: qsTr("Individual plates"), value: "plate" },
        { label: qsTr("Pedestals"), value: "pedestal" }
    ]
    readonly property var motionOptions: [
        { label: qsTr("None"), value: "none" },
        { label: qsTr("Bounce"), value: "bounce" },
        { label: qsTr("Elastic bounce"), value: "elastic" },
        { label: qsTr("Pulse"), value: "pulse" },
        { label: qsTr("Scale"), value: "scale" },
        { label: qsTr("Spin"), value: "spin" },
        { label: qsTr("Slow rotation"), value: "idle-rotate" },
        { label: qsTr("Orbit"), value: "orbit" },
        { label: qsTr("Swing"), value: "swing" },
        { label: qsTr("Wobble"), value: "wobble" },
        { label: qsTr("Wiggle"), value: "wiggle" },
        { label: qsTr("Shake"), value: "shake" },
        { label: qsTr("Glow"), value: "glow" },
        { label: qsTr("Breathing"), value: "breathe" },
        { label: qsTr("Floating"), value: "float" },
        { label: qsTr("Hover wave"), value: "wave" },
        { label: qsTr("Ripple"), value: "ripple" },
        { label: qsTr("Magnetic"), value: "magnetic" },
        { label: qsTr("Spring"), value: "spring" }
    ]
    readonly property var triggerOptions: [
        { label: qsTr("Hover"), value: "hover" },
        { label: qsTr("Click"), value: "click" },
        { label: qsTr("Launch"), value: "launch" },
        { label: qsTr("Running"), value: "running" },
        { label: qsTr("Drag and drop"), value: "drop" },
        { label: qsTr("Reveal"), value: "reveal" },
        { label: qsTr("Always on"), value: "idle" }
    ]
    readonly property var folderLayoutOptions: [
        { label: qsTr("Fan"), value: "fan" },
        { label: qsTr("Grid"), value: "grid" },
        { label: qsTr("Stack"), value: "stack" },
        { label: qsTr("Arc"), value: "arc" },
        { label: qsTr("Spiral"), value: "spiral" },
        { label: qsTr("Circular"), value: "circular" },
        { label: qsTr("Radial"), value: "radial" },
        { label: qsTr("Vertical cascade"), value: "vertical" },
        { label: qsTr("Horizontal cascade"), value: "horizontal" },
        { label: qsTr("Elastic unfold"), value: "elastic" },
        { label: qsTr("Physics spread"), value: "physics" }
    ]

    function panelValue(key, fallback) {
        const revision = panelRevision;
        const candidate = panelRegistry.panelValue(selectedPanelId, key);
        return candidate === undefined || candidate === null ? fallback : candidate;
    }

    function bottomPanelValue(key, fallback) {
        const revision = panelRevision;
        const candidate = panelRegistry.panelValue("bottom", key);
        return candidate === undefined || candidate === null ? fallback : candidate;
    }

    function panelScreenIndex(panelId) {
        const revision = panelRevision;
        const displays = screenRevision;
        return panelController.screenIndexForPanel(panelId);
    }

    function optionIndex(options, value) {
        for (let index = 0; index < options.length; ++index) {
            if (options[index].value === value)
                return index;
        }
        return 0;
    }

    function selectPanel(panelId) {
        if (panelId.length === 0)
            return;
        selectedPanelId = panelId;
        panelRegistry.setActivePanelId(panelId);
        sectionTabs.currentIndex = 4;
    }

    onSelectedPanelIdChanged: {
        if (selectedPanelId.length > 0)
            panelRegistry.setActivePanelId(selectedPanelId);
        sectionTabs.currentIndex = 4;
    }

    onShowPanelsChanged: {
        if (showPanels)
            sectionTabs.currentIndex = 4;
    }

    width: 720
    height: 780
    x: 320
    y: 180
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    title: qsTr("Arch Dock Settings")

    onVisibleChanged: {
        if (!visible)
            return;

        requestActivate();
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        border.width: 1
        border.color: "#75d9edf2"
        gradient: Gradient {
            GradientStop {
                position: 0
                color: "#f62a3848"
            }
            GradientStop {
                position: 1
                color: "#f1081018"
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 1
            height: 1
            radius: 1
            color: "#aaffffff"
        }
    }

    Rectangle {
        id: titleStrip

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 1
        anchors.leftMargin: 1
        anchors.rightMargin: 1
        height: 38
        color: "#e8182b3b"
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: "#5e77d6e8"
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            z: 1

            Kirigami.Icon {
                width: 18
                height: 18
                source: "preferences-desktop-theme-global"
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Arch Dock")
                color: "#f4f8fb"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 17
                color: "#4d9cb4c1"
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Panel Studio")
                color: "#a9bfcb"
                font.pixelSize: 11
            }
        }

        Label {
            anchors.centerIn: parent
            text: {
                const revision = root.panelRevision;
                const panelName = panelRegistry.panelName(root.selectedPanelId);
                return panelName.length > 0 ? panelName : qsTr("Panel Editor");
            }
            color: "#d5e5ed"
            font.pixelSize: 12
            elide: Text.ElideRight
            z: 1
        }

        DragHandler {
            target: null
            onActiveChanged: {
                if (active)
                    root.startSystemMove();
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            z: 2

            ToolButton {
                width: 30
                height: 30
                icon.name: "window-close"
                onClicked: root.close()

                background: Rectangle {
                    radius: 4
                    color: parent.hovered ? "#b94b526f" : "transparent"
                }

                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close settings")
            }
        }
    }

    ColumnLayout {
        anchors.top: titleStrip.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.bottomMargin: 18
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 14

        TabBar {
            id: sectionTabs

            Layout.fillWidth: true
            TabButton {
                text: qsTr("Layout")
            }

            TabButton {
                text: qsTr("Appearance")
            }

            TabButton {
                text: qsTr("Behavior")
            }

            TabButton {
                text: qsTr("Modules")
                visible: false
            }

            TabButton {
                text: qsTr("Panels")
            }
        }

        StackLayout {
            currentIndex: sectionTabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                clip: true
                contentWidth: availableWidth

                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 14

                    Label { text: qsTr("Edge"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: ["bottom", "top", "left", "right"]
                        currentIndex: model.indexOf(root.bottomPanelValue("edge", "bottom"))
                        onActivated: panelRegistry.setPanelValue("bottom", "edge", currentText)
                    }

                    Label { text: qsTr("Alignment"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: ["start", "center", "end"]
                        currentIndex: model.indexOf(root.bottomPanelValue("alignment", "center"))
                        onActivated: panelRegistry.setPanelValue("bottom", "alignment", currentText)
                    }

                    Label { text: qsTr("Display"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.screenOptions
                        textRole: "label"
                        currentIndex: root.panelScreenIndex("bottom")
                        onActivated: panelController.setPanelScreen("bottom", currentIndex)
                    }

                    Label { text: qsTr("Icon size"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 32
                        to: 96
                        stepSize: 2
                        editable: true
                        value: root.bottomPanelValue("iconSize", 52)
                        onValueModified: panelRegistry.setPanelValue("bottom", "iconSize", value)
                    }

                    Label { text: qsTr("Spacing"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 28
                            stepSize: 1
                            value: root.bottomPanelValue("spacing", 8)
                            onMoved: panelRegistry.setPanelValue("bottom", "spacing", value)
                        }

                        Label {
                            text: Math.round(root.bottomPanelValue("spacing", 8))
                            color: "#f4f8fb"
                            Layout.minimumWidth: 24
                        }
                    }
                }
            }

            ScrollView {
                clip: true
                contentWidth: availableWidth

                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 14

                    Label { text: qsTr("Quick profile"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: [
                            qsTr("Aurora desktop"),
                            qsTr("Crystal shelf"),
                            qsTr("RocketDock glass"),
                            qsTr("Midnight Waybar"),
                            qsTr("Neon prism"),
                            qsTr("Plasma Breeze"),
                            qsTr("Lime outline")
                        ]
                        onActivated: {
                            const profiles = ["aurora", "crystal", "rocket", "waybar", "neon", "plasma", "lime"];
                            panelController.applyProfile(profiles[currentIndex]);
                        }
                    }

                    Label { text: qsTr("Surface"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: ["glass", "crystal", "neon", "minimal", "plasma", "lime"]
                        currentIndex: model.indexOf(root.bottomPanelValue("appearance", "glass"))
                        onActivated: panelRegistry.setPanelValue("bottom", "appearance", currentText)
                    }

                    Label { text: qsTr("Panel opacity"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Slider {
                            Layout.fillWidth: true
                            from: 0.35
                            to: 1.0
                            stepSize: 0.05
                            value: root.bottomPanelValue("opacity", 0.9)
                            onMoved: panelRegistry.setPanelValue("bottom", "opacity", value)
                        }

                        Label {
                            text: Math.round(root.bottomPanelValue("opacity", 0.9) * 100) + "%"
                            color: "#f4f8fb"
                            Layout.minimumWidth: 38
                        }
                    }

                    Label { text: qsTr("Magnification"); color: "#cbd8e2" }

                    Switch {
                        checked: root.settings.magnificationEnabled
                        onToggled: root.settings.magnificationEnabled = checked
                    }

                    Label {
                        text: qsTr("Magnification amount")
                        color: root.settings.magnificationEnabled ? "#cbd8e2" : "#647681"
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        enabled: root.settings.magnificationEnabled

                        Slider {
                            Layout.fillWidth: true
                            from: 1.0
                            to: 2.4
                            stepSize: 0.05
                            value: root.settings.magnification
                            onMoved: root.settings.magnification = value
                        }

                        Label {
                            text: root.settings.magnification.toFixed(2) + "x"
                            color: "#f4f8fb"
                            Layout.minimumWidth: 38
                        }
                    }

                    Label { text: qsTr("Reflections"); color: "#cbd8e2" }

                    Switch {
                        checked: root.settings.showReflections
                        onToggled: root.settings.showReflections = checked
                    }

                    Label { text: qsTr("Running indicators"); color: "#cbd8e2" }

                    Switch {
                        checked: root.settings.showIndicators
                        onToggled: root.settings.showIndicators = checked
                    }
                }
            }

            ScrollView {
                clip: true
                contentWidth: availableWidth

                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 14

                    Label { text: qsTr("Auto-hide"); color: "#cbd8e2" }

                    Switch {
                        checked: root.bottomPanelValue("visibilityMode", "always") === "auto-hide"
                        onToggled: panelController.setPanelVisibilityMode(
                            "bottom",
                            checked ? "auto-hide" : "always")
                    }

                    Label { text: qsTr("Tooltips"); color: "#cbd8e2" }

                    Switch {
                        checked: root.settings.showTooltips
                        onToggled: root.settings.showTooltips = checked
                    }

                    Label { text: qsTr("Animation speed"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Slider {
                            Layout.fillWidth: true
                            from: 80
                            to: 500
                            stepSize: 10
                            value: root.settings.animationDuration
                            onMoved: root.settings.animationDuration = value
                        }

                        Label {
                            text: root.settings.animationDuration + qsTr(" ms")
                            color: "#f4f8fb"
                            Layout.minimumWidth: 52
                        }
                    }

                    Label { text: qsTr("Reduce motion"); color: "#cbd8e2" }

                    Switch {
                        checked: root.settings.reducedMotion
                        onToggled: root.settings.reducedMotion = checked
                    }

                }
            }

            ScrollView {
                clip: true
                contentWidth: availableWidth

                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 14

                    Label { text: qsTr("Clock module"); color: "#cbd8e2" }

                    Switch {
                        checked: root.settings.showStatusModule
                        onToggled: root.settings.showStatusModule = checked
                    }

                    Label {
                        text: qsTr("Date in clock")
                        color: root.settings.showStatusModule ? "#cbd8e2" : "#647681"
                    }

                    Switch {
                        enabled: root.settings.showStatusModule
                        checked: root.settings.showDate
                        onToggled: root.settings.showDate = checked
                    }

                    Label {
                        text: qsTr("Network module")
                        color: root.settings.showStatusModule ? "#cbd8e2" : "#647681"
                    }

                    Switch {
                        enabled: root.settings.showStatusModule
                        checked: root.settings.showNetworkModule
                        onToggled: root.settings.showNetworkModule = checked
                    }

                    Label {
                        text: qsTr("Battery module")
                        color: root.settings.showStatusModule ? "#cbd8e2" : "#647681"
                    }

                    Switch {
                        enabled: root.settings.showStatusModule
                        checked: root.settings.showBatteryModule
                        onToggled: root.settings.showBatteryModule = checked
                    }

                    Label {
                        text: qsTr("CPU and memory")
                        color: root.settings.showStatusModule ? "#cbd8e2" : "#647681"
                    }

                    Switch {
                        enabled: root.settings.showStatusModule
                        checked: root.settings.showPerformanceModule
                        onToggled: root.settings.showPerformanceModule = checked
                    }
                }
            }

            ScrollView {
                clip: true
                contentWidth: availableWidth

                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: 18
                    rowSpacing: 14

                    Label { text: qsTr("Panel"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        ComboBox {
                            id: panelSelector

                            Layout.fillWidth: true
                            model: panelRegistry.panelIds
                            currentIndex: Math.max(0, model.indexOf(root.selectedPanelId))
                            displayText: panelRegistry.panelName(currentText)
                            delegate: ItemDelegate {
                                required property string modelData

                                width: panelSelector.width
                                text: panelRegistry.panelName(modelData)
                            }
                            onActivated: root.selectPanel(currentText)
                        }

                        Button {
                            icon.name: "list-add"
                            text: qsTr("Free panel")
                            onClicked: root.selectPanel(panelController.createFreePanel())
                        }
                    }

                    Label { text: qsTr("Visible"); color: "#cbd8e2" }

                    Switch {
                        checked: root.panelValue("visible", true)
                        onToggled: panelRegistry.setPanelValue(root.selectedPanelId, "visible", checked)
                    }

                    Label { text: qsTr("Visibility"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.visibilityOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: root.optionIndex(
                            root.visibilityOptions,
                            root.panelValue("visibilityMode", "always"))
                        onActivated: panelController.setPanelVisibilityMode(
                            root.selectedPanelId,
                            currentValue)
                    }

                    Label { text: qsTr("Edge reveal zone"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 1
                        to: 64
                        value: root.panelValue("revealZone", 10)
                        onValueModified: panelRegistry.setPanelValue(
                            root.selectedPanelId,
                            "revealZone",
                            value)
                    }

                    Label { text: qsTr("Panel edge"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        enabled: !panelRegistry.isBuiltIn(root.selectedPanelId)
                            && root.panelValue("edge", "bottom") !== "free"
                        model: ["top", "bottom", "left", "right", "free"]
                        currentIndex: Math.max(0, model.indexOf(root.panelValue("edge", "bottom")))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "edge", currentText)
                    }

                    Label { text: qsTr("Panel alignment"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: ["start", "center", "end"]
                        currentIndex: model.indexOf(root.panelValue("alignment", "center"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "alignment", currentText)
                    }

                    Label { text: qsTr("Display"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.screenOptions
                        textRole: "label"
                        currentIndex: root.panelScreenIndex(root.selectedPanelId)
                        onActivated: panelController.setPanelScreen(root.selectedPanelId, currentIndex)
                    }

                    Label { text: qsTr("Content"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: ["empty", "launcher", "tasks", "hybrid"]
                        currentIndex: model.indexOf(root.panelValue("type", "hybrid"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "type", currentText)
                    }

                    Label { text: qsTr("Dynamic length"); color: "#cbd8e2" }

                    Switch {
                        checked: root.panelValue("dynamic", true)
                        onToggled: panelRegistry.setPanelValue(root.selectedPanelId, "dynamic", checked)
                    }

                    Label { text: qsTr("Width"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 48
                        to: 4096
                        editable: true
                        value: root.panelValue("width", 720)
                        onValueModified: panelRegistry.setPanelValue(root.selectedPanelId, "width", value)
                    }

                    Label { text: qsTr("Height"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 48
                        to: 4096
                        editable: true
                        value: root.panelValue("height", 76)
                        onValueModified: panelRegistry.setPanelValue(root.selectedPanelId, "height", value)
                    }

                    Label { text: qsTr("Icon size"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 24
                        to: 128
                        editable: true
                        value: root.panelValue("iconSize", 52)
                        onValueModified: panelRegistry.setPanelValue(root.selectedPanelId, "iconSize", value)
                    }

                    Label { text: qsTr("Icon spacing"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 48
                            stepSize: 1
                            value: root.panelValue("spacing", 8)
                            onMoved: panelRegistry.setPanelValue(root.selectedPanelId, "spacing", value)
                        }

                        Label {
                            text: Math.round(root.panelValue("spacing", 8))
                            color: "#f4f8fb"
                            Layout.minimumWidth: 26
                        }
                    }

                    Label { text: qsTr("Opacity"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Slider {
                            Layout.fillWidth: true
                            from: 0.1
                            to: 1
                            stepSize: 0.05
                            value: root.panelValue("opacity", 0.9)
                            onMoved: panelRegistry.setPanelValue(root.selectedPanelId, "opacity", value)
                        }

                        Label {
                            text: Math.round(root.panelValue("opacity", 0.9) * 100) + "%"
                            color: "#f4f8fb"
                            Layout.minimumWidth: 38
                        }
                    }

                    Label { text: qsTr("Surface theme"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.materialOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: root.optionIndex(root.materialOptions, root.panelValue("appearance", "glass"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "appearance", currentValue)
                    }

                    Label { text: qsTr("Dock layout"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.layoutOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: root.optionIndex(root.layoutOptions, root.panelValue("layout", "adaptive"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "layout", currentValue)
                    }

                    Label { text: qsTr("Layout scale"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Slider {
                            Layout.fillWidth: true
                            from: 0.5
                            to: 2.5
                            stepSize: 0.05
                            value: root.panelValue("layoutScale", 1.0)
                            onMoved: panelRegistry.setPanelValue(root.selectedPanelId, "layoutScale", value)
                        }

                        Label {
                            text: Number(root.panelValue("layoutScale", 1.0)).toFixed(2) + "x"
                            color: "#f4f8fb"
                            Layout.minimumWidth: 38
                        }
                    }

                    Label { text: qsTr("Radius"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 48
                        to: 2048
                        editable: true
                        value: root.panelValue("layoutRadius", 150)
                        onValueModified: panelRegistry.setPanelValue(root.selectedPanelId, "layoutRadius", value)
                    }

                    Label { text: qsTr("Layout angle"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: -180
                        to: 180
                        editable: true
                        value: root.panelValue("layoutAngle", 0)
                        onValueModified: panelRegistry.setPanelValue(root.selectedPanelId, "layoutAngle", value)
                    }

                    Label { text: qsTr("Polygon sides"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        enabled: ["polygon", "star"].includes(
                            root.panelValue("layout", "adaptive"))
                        from: 3
                        to: 12
                        value: root.panelValue("pathSides", 6)
                        onValueModified: panelRegistry.setPanelValue(root.selectedPanelId, "pathSides", value)
                    }

                    Label { text: qsTr("Path anchor"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.pathAnchorOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: root.optionIndex(
                            root.pathAnchorOptions,
                            root.panelValue("pathAnchor", "center"))
                        onActivated: panelRegistry.setPanelValue(
                            root.selectedPanelId,
                            "pathAnchor",
                            currentValue)
                    }

                    Label { text: qsTr("Icon path orientation"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.pathOrientationOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: root.optionIndex(
                            root.pathOrientationOptions,
                            root.panelValue("pathOrientation", "upright"))
                        onActivated: panelRegistry.setPanelValue(
                            root.selectedPanelId,
                            "pathOrientation",
                            currentValue)
                    }

                    Label { text: qsTr("Grid rows"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 1
                        to: 8
                        value: root.panelValue("layoutRows", 2)
                        onValueModified: panelRegistry.setPanelValue(root.selectedPanelId, "layoutRows", value)
                    }

                    Label { text: qsTr("Spring rearrangement"); color: "#cbd8e2" }

                    Switch {
                        checked: root.panelValue("physicsEnabled", false)
                        onToggled: panelRegistry.setPanelValue(root.selectedPanelId, "physicsEnabled", checked)
                    }

                    Label { text: qsTr("Icon animation"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.motionOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: root.optionIndex(root.motionOptions, root.panelValue("iconAnimation", "scale"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "iconAnimation", currentValue)
                    }

                    Label { text: qsTr("Animation trigger"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.triggerOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: root.optionIndex(root.triggerOptions, root.panelValue("animationTrigger", "hover"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "animationTrigger", currentValue)
                    }

                    Label { text: qsTr("Motion speed"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Slider {
                            Layout.fillWidth: true
                            from: 0.2
                            to: 3.0
                            stepSize: 0.1
                            value: root.panelValue("animationSpeed", 1.0)
                            onMoved: panelRegistry.setPanelValue(root.selectedPanelId, "animationSpeed", value)
                        }

                        Label {
                            text: Number(root.panelValue("animationSpeed", 1.0)).toFixed(1) + "x"
                            color: "#f4f8fb"
                            Layout.minimumWidth: 34
                        }
                    }

                    Label { text: qsTr("Motion intensity"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Slider {
                            Layout.fillWidth: true
                            from: 0.1
                            to: 2.5
                            stepSize: 0.1
                            value: root.panelValue("animationIntensity", 1.0)
                            onMoved: panelRegistry.setPanelValue(root.selectedPanelId, "animationIntensity", value)
                        }

                        Label {
                            text: Number(root.panelValue("animationIntensity", 1.0)).toFixed(1)
                            color: "#f4f8fb"
                            Layout.minimumWidth: 28
                        }
                    }

                    Label { text: qsTr("Folder expansion"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: root.folderLayoutOptions
                        textRole: "label"
                        valueRole: "value"
                        currentIndex: root.optionIndex(root.folderLayoutOptions, root.panelValue("folderLayout", "fan"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "folderLayout", currentValue)
                    }

                    Label { text: qsTr("Folder animation speed"); color: "#cbd8e2" }

                    SpinBox {
                        Layout.fillWidth: true
                        from: 80
                        to: 1200
                        stepSize: 20
                        value: root.panelValue("folderSpeed", 260)
                        onValueModified: panelRegistry.setPanelValue(root.selectedPanelId, "folderSpeed", value)
                    }

                    Label { text: qsTr("Open folders on click"); color: "#cbd8e2" }

                    Switch {
                        checked: root.panelValue("folderExpandOnClick", true)
                        onToggled: panelRegistry.setPanelValue(root.selectedPanelId, "folderExpandOnClick", checked)
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        Layout.alignment: Qt.AlignLeft
                        text: qsTr("Apply live preview")
                        icon.name: "media-playback-start"
                        onClicked: {
                            panelRegistry.setPanelValue(root.selectedPanelId, "layout", "arc");
                            panelRegistry.setPanelValue(root.selectedPanelId, "layoutRadius", 180);
                            panelRegistry.setPanelValue(root.selectedPanelId, "appearance", "futuristic");
                            panelRegistry.setPanelValue(root.selectedPanelId, "iconAnimation", "glow");
                            panelRegistry.setPanelValue(root.selectedPanelId, "animationTrigger", "idle");
                        }
                    }

                    Label { text: qsTr("Panel shape"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: ["pill", "rounded", "hexagon"]
                        currentIndex: model.indexOf(root.panelValue("shape", "pill"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "shape", currentText)
                    }

                    Label { text: qsTr("Icon tile shape"); color: "#cbd8e2" }

                    ComboBox {
                        Layout.fillWidth: true
                        model: ["rounded", "circle", "hexagon"]
                        currentIndex: model.indexOf(root.panelValue("iconShape", "rounded"))
                        onActivated: panelRegistry.setPanelValue(root.selectedPanelId, "iconShape", currentText)
                    }

                    Label { text: qsTr("Panel color"); color: "#cbd8e2" }

                    TextField {
                        Layout.fillWidth: true
                        text: root.panelValue("color", "")
                        placeholderText: qsTr("#RRGGBB or transparent")
                        selectByMouse: true
                        onEditingFinished: panelRegistry.setPanelValue(root.selectedPanelId, "color", text)
                    }

                    Label { text: qsTr("Drop apps and files"); color: "#cbd8e2" }

                    Switch {
                        checked: root.panelValue("acceptDrops", true)
                        onToggled: panelRegistry.setPanelValue(root.selectedPanelId, "acceptDrops", checked)
                    }

                    Label { text: qsTr("Theme package"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            Layout.fillWidth: true
                            text: root.panelValue("themePackageName", "").toString().length > 0
                                ? qsTr("%1 (v%2)")
                                    .arg(root.panelValue("themePackageName", ""))
                                    .arg(root.panelValue("themePackageVersion", 1))
                                : qsTr("Preset surface")
                            color: "#a9bfcb"
                            elide: Text.ElideMiddle
                        }

                        Button {
                            icon.name: "document-open"
                            text: qsTr("Import package or artwork")
                            onClicked: themeDialog.open()
                        }
                    }

                    Label { text: qsTr("Import analysis"); color: "#cbd8e2" }

                    Label {
                        Layout.fillWidth: true
                        readonly property string sourceKind: root.panelValue("themeSourceKind", "").toString()
                        readonly property string sourceFormat: root.panelValue("themeSourceFormat", "").toString()
                        readonly property int sourceWidth: root.panelValue("themeSourceWidth", 0)
                        readonly property int sourceHeight: root.panelValue("themeSourceHeight", 0)
                        readonly property bool sourceHasAlpha: root.panelValue("themeSourceHasAlpha", false)
                        text: sourceKind.length === 0
                            ? qsTr("No imported source")
                            : qsTr("%1 %2 source, %3 x %4%5")
                                .arg(sourceKind)
                                .arg(sourceFormat.toUpperCase())
                                .arg(sourceWidth)
                                .arg(sourceHeight)
                                .arg(sourceHasAlpha ? qsTr(", alpha") : "")
                        color: "#a9bfcb"
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 118
                        radius: 6
                        color: "#26121b24"
                        border.width: 1
                        border.color: "#40536a78"

                        readonly property string previewSource: root.panelValue("themePreview", "").toString().length > 0
                            ? root.panelValue("themePreview", "")
                            : root.panelValue("themeAsset", "")

                        Image {
                            anchors.fill: parent
                            anchors.margins: 8
                            source: parent.previewSource
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            asynchronous: true
                            visible: source.toString().length > 0
                        }

                        Label {
                            anchors.centerIn: parent
                            width: parent.width - 24
                            text: root.panelValue("themeAnalysisStatus", qsTr("No preview available."))
                            color: "#a9bfcb"
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            visible: parent.previewSource.length === 0
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.panelValue("themeAnalysisStatus", "")
                        color: "#a9bfcb"
                        wrapMode: Text.Wrap
                        visible: text.length > 0
                    }

                    Label {
                        Layout.fillWidth: true
                        readonly property bool isScene: root.panelValue("themeSourceKind", "") === "scene"
                        readonly property bool conversionAvailable: root.panelValue("themeConversionAvailable", false)
                        text: conversionAvailable
                            ? qsTr("Blender rendering is available.")
                            : qsTr("Blender rendering is unavailable; the source remains saved.")
                        color: conversionAvailable ? "#a9bfcb" : "#d9c58a"
                        wrapMode: Text.Wrap
                        visible: isScene
                    }

                    Label { text: qsTr("Artwork fit"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        ComboBox {
                            id: themeFitSelector

                            Layout.fillWidth: true
                            model: ["cover", "contain", "stretch", "tile"]
                            currentIndex: model.indexOf(root.panelValue("themeFit", "cover"))
                            onActivated: {
                                panelRegistry.setPanelValue(root.selectedPanelId, "themeFit", currentText);
                                panelRegistry.renderTheme(
                                    root.selectedPanelId,
                                    root.panelValue("width", 720),
                                    root.panelValue("height", 76),
                                    0,
                                    true);
                            }
                        }

                        Button {
                            icon.name: "view-refresh"
                            text: qsTr("Render")
                            enabled: root.panelValue("themeSource", "").toString().length > 0
                            onClicked: panelRegistry.renderTheme(
                                root.selectedPanelId,
                                root.panelValue("width", 720),
                                root.panelValue("height", 76),
                                0,
                                true)
                        }

                        Button {
                            icon.name: "edit-clear"
                            text: qsTr("Clear")
                            enabled: root.panelValue("themeSource", "").toString().length > 0
                            onClicked: panelRegistry.clearTheme(root.selectedPanelId)
                        }
                    }

                    Button {
                        Layout.alignment: Qt.AlignLeft
                        icon.name: "dialog-ok-apply"
                        text: qsTr("Use suggested fit")
                        enabled: root.panelValue("themeSource", "").toString().length > 0
                        onClicked: {
                            const suggestedFit = root.panelValue("themeSuggestedFit", "cover");
                            panelRegistry.setPanelValue(root.selectedPanelId, "themeFit", suggestedFit);
                            panelRegistry.renderTheme(
                                root.selectedPanelId,
                                root.panelValue("width", 720),
                                root.panelValue("height", 76),
                                0,
                                true);
                        }
                    }

                    Label { text: qsTr("Render status"); color: "#cbd8e2" }

                    Label {
                        Layout.fillWidth: true
                        text: root.panelValue("themeStatus", qsTr("Preset surface active."))
                        color: root.panelValue("themeAsset", "").toString().length > 0
                            ? "#a9bfcb" : "#d9c58a"
                        wrapMode: Text.Wrap
                    }

                    Label { text: qsTr("KDE widget plugin"); color: "#cbd8e2" }

                    RowLayout {
                        Layout.fillWidth: true

                        ComboBox {
                            id: kdeWidgetPlugin

                            Layout.fillWidth: true
                            editable: true
                            model: root.kdeWidgetIds
                            currentIndex: -1
                            displayText: editText.length > 0
                                ? editText : qsTr("Choose a Plasma widget")
                        }

                        Button {
                            icon.name: "media-playback-start"
                            text: qsTr("Add")
                            enabled: kdeWidgetPlugin.currentText.length > 0
                            onClicked: panelController.addKdeWidget(
                                root.selectedPanelId,
                                kdeWidgetPlugin.currentText)
                        }

                        Button {
                            icon.name: "view-preview"
                            enabled: kdeWidgetPlugin.currentText.length > 0
                            onClicked: panelController.openKdeWidgetPreview(
                                root.selectedPanelId,
                                kdeWidgetPlugin.currentText)
                        }
                    }

                    Label { text: qsTr("Native KDE panel"); color: "#cbd8e2" }

                    Button {
                        Layout.alignment: Qt.AlignLeft
                        text: root.panelValue("nativePanelId", -1) >= 0
                            ? qsTr("KDE panel linked") : qsTr("Create KDE panel")
                        onClicked: panelController.createNativeKdePanel(root.selectedPanelId)
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        Layout.alignment: Qt.AlignRight
                        visible: root.panelValue("nativePanelId", -1) >= 0
                        text: qsTr("Remove KDE panel")
                        onClicked: panelController.removeNativeKdePanel(root.selectedPanelId)
                    }

                    Button {
                        Layout.alignment: Qt.AlignRight
                        text: panelRegistry.isBuiltIn(root.selectedPanelId)
                            ? qsTr("Hide panel") : qsTr("Remove panel")
                        onClicked: {
                            panelController.removePanel(root.selectedPanelId);
                            root.selectPanel(panelRegistry.activePanelId);
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 2

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Reset")
                onClicked: panelController.resetSettings()
            }

            Button {
                text: qsTr("Done")
                onClicked: root.close()
            }
        }
    }

    FileDialog {
        id: themeDialog

        title: qsTr("Import theme package or artwork")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("Theme packages (archdock-theme.json *.archdock-theme.json *.json)"),
            qsTr("Design files (*.png *.jpg *.jpeg *.webp *.avif *.heif *.heic *.jxl *.svg *.svgz *.tif *.tiff *.pdf *.psd *.xcf *.blend)"),
            qsTr("All files (*)")
        ]
        onAccepted: panelRegistry.importTheme(root.selectedPanelId, selectedFile)
    }
}
