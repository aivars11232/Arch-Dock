import QtQuick
import org.kde.kirigami as Kirigami

Rectangle {
    id: root

    property string iconName;
    property string shape: "roundedSquare"
    property color backgroundColor: "#40ffffff"
    property color hoverBackgroundColor: "#60ffffff"
    property color borderColor: "#60ffffff"
    property int borderWidth: 1
    property bool showBackground: true
    property bool showBorder: true
    property int cornerRadius: 10
    property int iconSize: 26
    property int containerSize: 40
    property int hoverLift: 4
    property int hoverAnimationDuration: 120

    signal clicked()
    signal rightClicked(point scenePosition)

    width: containerSize
    height: containerSize
    y: hoverHandler.hovered ? -hoverLift : 0
    radius: {
        switch (shape) {
        case "circle":
            return width / 2;
        case "square":
            return 0;
        default:
            return cornerRadius;
        }
    }
    color: showBackground ? (hoverHandler.hovered ? hoverBackgroundColor : backgroundColor) : "transparent"
    border.width: showBorder ? borderWidth : 0
    border.color: borderColor

    Kirigami.Icon {
        anchors.centerIn: parent
        width: iconSize
        height: iconSize
        source: root.iconName
    }

    HoverHandler {
        id: hoverHandler
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: root.clicked()
    }

    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: function(eventPoint) {
            root.rightClicked(eventPoint.scenePosition);
        }
    }

    Behavior on y {
        NumberAnimation {
            duration: hoverAnimationDuration
        }

    }

    Behavior on color {
        ColorAnimation {
            duration: hoverAnimationDuration
        }

    }

}
