import QtQuick
import ArchDock.Rendering 1.0

Item {
    id: root

    // These facts belong to this consumer's engine and window. No panel or
    // theme setting can assert a successful import or a usable graphics API.
    readonly property bool buildAvailable: RendererBuildConfig.quick3DBuilt
    readonly property bool sceneBuilt: RendererBuildConfig.scene3DBuilt
    readonly property int graphicsApi: GraphicsInfo.api
    readonly property bool backendSupported:
        graphicsApi === GraphicsInfo.OpenGL || graphicsApi === GraphicsInfo.Vulkan
    readonly property bool importAvailable:
        importComponent !== null && importComponent.status === Component.Ready
    readonly property bool moduleAvailable:
        buildAvailable && importAvailable && backendSupported
    readonly property bool rendererAvailable: moduleAvailable && sceneBuilt
    readonly property string diagnosticCode: {
        if (!buildAvailable)
            return "renderer-not-installed"
        if (importComponent === null || importComponent.status === Component.Loading)
            return "renderer-import-loading"
        if (!importAvailable)
            return "renderer-import-unavailable"
        if (graphicsApi === GraphicsInfo.Unknown)
            return "renderer-backend-uninitialized"
        if (!backendSupported)
            return "renderer-backend-unsupported"
        if (!sceneBuilt)
            return "renderer-scene-unavailable"
        return "available"
    }
    readonly property string importDiagnostic:
        importComponent !== null && importComponent.status === Component.Error
        ? importComponent.errorString() : ""
    readonly property var capability: ({
        buildAvailable: buildAvailable,
        importAvailable: importAvailable,
        backendSupported: backendSupported,
        graphicsApi: graphicsApi,
        moduleAvailable: moduleAvailable,
        sceneBuilt: sceneBuilt,
        rendererAvailable: rendererAvailable,
        reasonCode: diagnosticCode,
        importDiagnostic: importDiagnostic
    })

    property var importComponent: null

    Component.onCompleted: {
        if (buildAvailable) {
            // A fixed application resource, never a package-provided URL.
            // Compiling the probe creates no scene or render target.
            importComponent = Qt.createComponent(
                Qt.resolvedUrl("optional3d/RendererImportProbe.qml"),
                Component.PreferSynchronous, root)
        }
    }
}
