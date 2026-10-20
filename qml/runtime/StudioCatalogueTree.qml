pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ArchDock.Rendering 1.0
import org.kde.kirigami as Kirigami
import "StudioThemes.js" as StudioThemes

// A catalogue in Panel Studio as a tree: a few big groups that open into
// their subgroups, which open into their items, each drawn small with its
// name (DecorationThumbnail). The chosen item is marked, and its group and
// subgroup are open when the tree is first shown. A card first in the tree
// stands for none (or as designed). Thumbnails are made only while their
// subgroup is open.
ColumnLayout {
    id: root

    required property var theme
    // [{ name, subgroups: [{ name, items: [{ id, name }] }] }]
    property var groups: []
    // "decoration" or "material": what the thumbnails draw.
    property string itemKind: "decoration"
    property string current: ""
    // The first card's name: none, or as designed; empty for no such card.
    property string emptyLabel: ""
    signal chosen(string id)

    property var openGroups: ({})
    property var openSubgroups: ({})
    property bool opened: false

    function subgroupKey(group, subgroup) {
        return group + "/" + subgroup
    }
    function toggle(map, key) {
        const copy = Object.assign({}, map)
        copy[key] = !copy[key]
        return copy
    }
    function opening(map, key) {
        const copy = Object.assign({}, map)
        copy[key] = true
        return copy
    }
    // The chosen item's group and subgroup open on first show. Opening them
    // lays the page out anew, which may ask again: it is marked done first,
    // and opening is not toggling.
    function openCurrent() {
        if (opened || !current.length) return
        for (const group of groups)
            for (const subgroup of group.subgroups)
                if (subgroup.items.some(function(item) { return item.id === root.current })) {
                    opened = true
                    openGroups = opening(openGroups, group.name)
                    openSubgroups = opening(openSubgroups, subgroupKey(group.name, subgroup.name))
                    return
                }
    }
    onGroupsChanged: openCurrent()
    onCurrentChanged: openCurrent()
    Component.onCompleted: openCurrent()

    spacing: 6

    // A card: the item drawn small, its name under it; the chosen one framed.
    component ItemCard: AbstractButton {
        id: card
        required property string itemId
        required property string itemName
        readonly property bool chosenNow: root.current === itemId
        objectName: "studio-tree-item-" + itemId
        implicitWidth: 176
        implicitHeight: 78
        hoverEnabled: true
        Accessible.name: itemName
        onClicked: root.chosen(itemId)

        background: Rectangle {
            radius: root.theme.controlRadius + 1
            color: card.chosenNow ? root.theme.rowHover : card.hovered ? root.theme.row : "transparent"
            border.width: card.chosenNow ? 2 : 1
            border.color: card.chosenNow ? root.theme.accent : card.hovered ? root.theme.rowBorderHover : root.theme.rowBorder
        }
        contentItem: ColumnLayout {
            spacing: 2
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                DecorationThumbnail {
                    anchors.fill: parent
                    anchors.margins: 3
                    visible: card.itemId.length > 0
                    decoration: root.itemKind === "decoration" ? card.itemId : ""
                    material: root.itemKind === "material" ? card.itemId : ""
                }
                Kirigami.Icon {
                    visible: card.itemId.length === 0
                    anchors.centerIn: parent
                    width: 22
                    height: 22
                    source: "edit-clear"
                    color: root.theme.textMuted
                }
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: card.itemName
                color: card.chosenNow ? root.theme.heading : root.theme.text
                font.pixelSize: 11
            }
        }
    }

    Flow {
        visible: root.emptyLabel.length > 0
        Layout.fillWidth: true
        ItemCard {
            itemId: ""
            itemName: root.emptyLabel
        }
    }

    Repeater {
        model: root.groups

        delegate: ColumnLayout {
            id: groupBlock
            required property var modelData
            readonly property bool open: root.openGroups[modelData.name] === true
            Layout.fillWidth: true
            spacing: 4

            Button {
                objectName: "studio-tree-" + root.itemKind + "-group-" + groupBlock.modelData.name
                Layout.fillWidth: true
                flat: true
                Accessible.name: groupBlock.modelData.name
                onClicked: root.openGroups = root.toggle(root.openGroups, groupBlock.modelData.name)
                contentItem: RowLayout {
                    spacing: 6
                    Kirigami.Icon {
                        source: groupBlock.open ? "go-down-symbolic" : "go-next-symbolic"
                        implicitWidth: 16
                        implicitHeight: 16
                        color: root.theme.text
                    }
                    Label {
                        Layout.fillWidth: true
                        text: StudioThemes.label(root.theme, groupBlock.modelData.name)
                        color: root.theme.heading
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                    Label {
                        text: groupBlock.modelData.subgroups.reduce(function(sum, s) { return sum + s.items.length }, 0)
                        color: root.theme.textMuted
                        font.pixelSize: 11
                    }
                }
            }

            Repeater {
                model: groupBlock.open ? groupBlock.modelData.subgroups : []

                delegate: ColumnLayout {
                    id: subgroupBlock
                    required property var modelData
                    readonly property string key: root.subgroupKey(groupBlock.modelData.name, modelData.name)
                    readonly property bool open: root.openSubgroups[key] === true
                    Layout.fillWidth: true
                    Layout.leftMargin: 18
                    spacing: 4

                    Button {
                        objectName: "studio-tree-" + root.itemKind + "-subgroup-" + subgroupBlock.modelData.name
                        Layout.fillWidth: true
                        flat: true
                        Accessible.name: subgroupBlock.modelData.name
                        onClicked: root.openSubgroups = root.toggle(root.openSubgroups, subgroupBlock.key)
                        contentItem: RowLayout {
                            spacing: 6
                            Kirigami.Icon {
                                source: subgroupBlock.open ? "go-down-symbolic" : "go-next-symbolic"
                                implicitWidth: 14
                                implicitHeight: 14
                                color: root.theme.text
                            }
                            Label {
                                Layout.fillWidth: true
                                text: subgroupBlock.modelData.name
                                color: root.theme.text
                                font.pixelSize: 12
                            }
                            Label {
                                text: subgroupBlock.modelData.items.length
                                color: root.theme.textMuted
                                font.pixelSize: 11
                            }
                        }
                    }

                    Flow {
                        visible: subgroupBlock.open
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: subgroupBlock.open ? subgroupBlock.modelData.items : []
                            delegate: ItemCard {
                                required property var modelData
                                itemId: modelData.id
                                itemName: modelData.name
                            }
                        }
                    }
                }
            }
        }
    }
}
