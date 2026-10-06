pragma Singleton

import QtQml

// Lets a consumer check that the ArchDock.Rendering module it imported is
// the one it expects.
QtObject {
    readonly property string uri: "ArchDock.Rendering"
    readonly property int majorVersion: 1
    readonly property int minorVersion: 0
    readonly property bool ready: true
}
