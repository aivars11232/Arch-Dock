import QtQuick
import org.kde.taskmanager as TaskManager
import org.kde.pipewire as PipeWire

// Optional platform adapter, loaded only through the Plasma preview host.
Item {
    id: root
    property string windowId: ""
    readonly property bool ready: capture.nodeId > 0 && stream.ready
        && stream.state !== PipeWire.PipeWireSourceItem.Error

    TaskManager.ScreencastingRequest {
        id: capture
        uuid: root.windowId
    }
    PipeWire.PipeWireSourceItem {
        id: stream
        anchors.fill: parent
        nodeId: capture.nodeId
        objectSerial: capture.objectSerial
    }
}
