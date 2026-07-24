import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    required property bool vertical
    required property bool active
    required property bool urgent
    required property int windowCount

    width: vertical ? Math.max(3, Kirigami.Units.smallSpacing / 2)
                    : active ? Kirigami.Units.gridUnit : Math.max(5, windowCount * 4)
    height: vertical ? (active ? Kirigami.Units.gridUnit : Math.max(5, windowCount * 4))
                     : Math.max(3, Kirigami.Units.smallSpacing / 2)
    radius: Math.min(width, height) / 2
    color: urgent ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.highlightColor

    SequentialAnimation on opacity {
        running: urgent
        loops: Animation.Infinite
        NumberAnimation { from: 1; to: 0.35; duration: 420 }
        NumberAnimation { from: 0.35; to: 1; duration: 420 }
    }
}
