pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "StudioThemes.js" as StudioThemes

// Panel Studio's Widgets page, for a bar: the dock's icons and Plasma's
// widgets in the order they stand along the bar, each moved along it or
// removed (the dock's icons stay), and Plasma's installed widgets to add,
// found by name. A change reaches the bar at once, as Plasma's own panels
// take theirs.
ColumnLayout {
    id: root
    // Panel Studio's look.
    property var theme: StudioThemes.theme("classic")
    // What the bar holds, in order: {key, plugin, name, icon}.
    property var widgets: []
    // Plasma's installed widgets: {plugin, name, icon, description}.
    property var installed: []
    // A side bar's widgets stand one above another.
    property bool upright: false
    property bool actionsEnabled: true
    property string notice: ""
    // The list of widgets to add is open.
    property bool choosing: false
    signal moveRequested(string key, int index)
    signal removeRequested(string key)
    signal addRequested(string plugin)
    // The installed widgets are read again each time the list opens, so a
    // widget installed meanwhile is there.
    signal installedListOpened()

    // Every word typed is found in the widget's name, description or id.
    readonly property var found: {
        const words = search.text.trim().toLowerCase().split(/\s+/)
            .filter(function(word) { return word.length > 0; });
        return Array.from(root.installed || []).filter(function(widget) {
            const text = [widget.name, widget.description, widget.plugin].join(" ").toLowerCase();
            return words.every(function(word) { return text.indexOf(word) >= 0; });
        });
    }

    function openChooser(open) {
        choosing = open;
        if (!open)
            return;
        search.text = "";
        installedListOpened();
        search.forceActiveFocus();
    }

    spacing: 8

    Label {
        text: StudioThemes.label(root.theme, qsTr("Widgets in this bar"))
        color: root.theme.heading
        font.pixelSize: 14
        font.letterSpacing: root.theme.letterSpacing
        font.weight: root.theme.headingWeight
    }

    Repeater {
        model: root.widgets

        delegate: StudioFrame {
            id: widgetRow
            required property var modelData
            required property int index
            readonly property string key: String(modelData.key || "")
            readonly property string name: String(modelData.name || modelData.plugin || "")
            objectName: "studio-bar-widget-" + key
            Layout.fillWidth: true
            implicitHeight: widgetLine.implicitHeight + 16
            radius: root.theme.controlRadius + 1
            chamfer: root.theme.chamfer / 2
            color: rowHover.hovered ? root.theme.rowHover : root.theme.row
            borderColor: rowHover.hovered ? root.theme.rowBorderHover : root.theme.rowBorder

            HoverHandler {
                id: rowHover
            }

            RowLayout {
                id: widgetLine
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                anchors.topMargin: 8
                anchors.bottomMargin: 8
                spacing: 10

                Kirigami.Icon {
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    source: String(widgetRow.modelData.icon || "plasma")
                    fallback: "plasma"
                }

                Label {
                    Layout.fillWidth: true
                    text: widgetRow.name
                    color: root.theme.text
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }

                // Each button keeps its place when it is hidden, so the
                // buttons stand in columns down the list.
                Item {
                    Layout.preferredWidth: earlier.implicitWidth
                    Layout.preferredHeight: earlier.implicitHeight

                    Button {
                        id: earlier
                        objectName: "studio-bar-widget-earlier-" + widgetRow.key
                        anchors.fill: parent
                        visible: widgetRow.index > 0
                        enabled: root.actionsEnabled
                        text: root.upright ? qsTr("Move up") : qsTr("Move left")
                        icon.name: root.upright ? "go-up" : "go-previous"
                        onClicked: root.moveRequested(widgetRow.key, widgetRow.index - 1)
                        Accessible.name: qsTr("%1: %2").arg(text).arg(widgetRow.name)
                    }
                }

                Item {
                    Layout.preferredWidth: later.implicitWidth
                    Layout.preferredHeight: later.implicitHeight

                    Button {
                        id: later
                        objectName: "studio-bar-widget-later-" + widgetRow.key
                        anchors.fill: parent
                        visible: widgetRow.index < root.widgets.length - 1
                        enabled: root.actionsEnabled
                        text: root.upright ? qsTr("Move down") : qsTr("Move right")
                        icon.name: root.upright ? "go-down" : "go-next"
                        onClicked: root.moveRequested(widgetRow.key, widgetRow.index + 1)
                        Accessible.name: qsTr("%1: %2").arg(text).arg(widgetRow.name)
                    }
                }

                Item {
                    Layout.preferredWidth: remove.implicitWidth
                    Layout.preferredHeight: remove.implicitHeight

                    Button {
                        id: remove
                        objectName: "studio-bar-widget-remove-" + widgetRow.key
                        anchors.fill: parent
                        // The dock's icons stay in the bar.
                        visible: widgetRow.key !== "dock"
                        enabled: root.actionsEnabled
                        text: qsTr("Remove")
                        icon.name: "list-remove"
                        onClicked: root.removeRequested(widgetRow.key)
                        Accessible.name: qsTr("Remove %1").arg(widgetRow.name)
                    }
                }
            }
        }
    }

    Button {
        objectName: "studio-bar-widget-choose"
        Layout.topMargin: 6
        enabled: root.actionsEnabled || root.choosing
        text: root.choosing ? qsTr("Close the widget list") : qsTr("Add widget…")
        icon.name: root.choosing ? "dialog-close" : "list-add"
        onClicked: root.openChooser(!root.choosing)
        Accessible.name: text
    }

    TextField {
        id: search
        objectName: "studio-bar-widget-search"
        visible: root.choosing
        Layout.fillWidth: true
        placeholderText: qsTr("Search widgets…")
        Accessible.name: qsTr("Search Plasma's widgets")
    }

    Flow {
        objectName: "studio-bar-widget-choices"
        visible: root.choosing
        Layout.fillWidth: true
        spacing: 8

        Repeater {
            model: root.choosing ? root.found : []

            delegate: Button {
                id: choice
                required property var modelData
                readonly property string name: String(modelData.name || modelData.plugin || "")
                readonly property string description: String(modelData.description || "")
                objectName: "studio-bar-widget-add-" + String(modelData.plugin || "")
                width: 148
                height: 92
                enabled: root.actionsEnabled
                onClicked: root.addRequested(String(modelData.plugin || ""))
                ToolTip.visible: hovered && description.length > 0
                ToolTip.text: description
                ToolTip.delay: 600
                Accessible.name: qsTr("Add %1").arg(name)
                Accessible.description: description

                contentItem: ColumnLayout {
                    spacing: 6

                    Kirigami.Icon {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        source: String(choice.modelData.icon || "plasma")
                        fallback: "plasma"
                    }

                    Label {
                        Layout.fillWidth: true
                        text: choice.name
                        color: root.theme.text
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                }

                background: StudioFrame {
                    radius: root.theme.cardRadius
                    chamfer: root.theme.chamfer / 2
                    color: choice.hovered ? root.theme.cardSelected : root.theme.card
                    borderColor: choice.hovered || choice.visualFocus
                        ? root.theme.cardBorderSelected : root.theme.cardBorder
                }
            }
        }
    }

    Label {
        objectName: "studio-bar-widget-none-found"
        visible: root.choosing && root.found.length === 0
        Layout.fillWidth: true
        text: search.text.trim().length > 0
            ? qsTr("No widget matches “%1”.").arg(search.text.trim())
            : qsTr("No Plasma widgets were found.")
        color: root.theme.textSecondary
        wrapMode: Text.Wrap
    }

    Label {
        objectName: "studio-bar-widget-notice"
        visible: text.length > 0
        Layout.fillWidth: true
        text: root.notice
        color: root.theme.warning
        wrapMode: Text.Wrap
        Accessible.name: text
    }

    Item {
        Layout.fillHeight: true
    }
}
