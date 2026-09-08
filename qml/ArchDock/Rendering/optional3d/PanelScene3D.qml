import QtQuick
import QtQuick3D

Item {
    id: root

    required property var sceneDefinition
    required property var resources
    property url textureSource: ""
    property var entryGeometry: []
    property string quality: "medium"
    property real panelOpacity: 1
    property bool sceneConcealed: false

    readonly property string effectiveQuality:
        ["low", "medium", "high"].includes(quality) ? quality : "medium"
    readonly property real resolutionScale: effectiveQuality === "low" ? 0.5
        : effectiveQuality === "high" ? 1 : 0.75
    readonly property int textureLimit: effectiveQuality === "low" ? 1024
        : effectiveQuality === "high" ? 2048 : 1536
    readonly property real boundedScale: Math.min(resolutionScale,
        textureLimit / Math.max(1, width, height))
    readonly property int targetWidth: Math.max(1, Math.ceil(width * boundedScale))
    readonly property int targetHeight: Math.max(1, Math.ceil(height * boundedScale))
    readonly property bool resourcesReady: resources !== null
        && resources.mesh !== undefined && resources.iconMesh !== undefined
        && resources.material !== undefined
        && resources.material.format === "org.archdock.material"
        && platform.meshReady && iconResource.meshReady
    readonly property bool textureRequired: Boolean(sceneDefinition && sceneDefinition.texture)
    readonly property bool textureReady: !textureRequired || textureImage.status === Image.Ready
    readonly property bool rendererReady: resourcesReady && textureReady
    readonly property string errorReason: !resourcesReady ? "scene3d-mesh-unavailable"
        : textureRequired && textureImage.status === Image.Error ? "scene3d-texture-unavailable"
        : !textureReady ? "renderer-loading" : ""
    readonly property int triangleCount: platform.triangleCount
        + iconResource.triangleCount * entryGeometry.length
    readonly property var qualityState: ({ quality: effectiveQuality,
        targetWidth: targetWidth, targetHeight: targetHeight,
        samples: effectiveQuality === "low" ? 1 : effectiveQuality === "high" ? 4 : 2 })
    readonly property var viewport: view
    readonly property bool frameRendered: view.renderStats.frameTime > 0

    function bounded(key, fallback, minimum, maximum) {
        const value = Number((sceneDefinition || ({}))[key])
        return Number.isFinite(value) ? Math.max(minimum, Math.min(maximum, value)) : fallback
    }

    Image {
        id: textureImage
        visible: false
        source: root.textureSource
        asynchronous: true
        sourceSize: Qt.size(1024, 1024)
        cache: false
    }

    View3D {
        id: view
        anchors.fill: parent
        visible: root.rendererReady && !root.sceneConcealed
        opacity: root.panelOpacity
        explicitTextureWidth: root.targetWidth
        explicitTextureHeight: root.targetHeight
        camera: camera
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: root.effectiveQuality === "low"
                ? SceneEnvironment.NoAA : SceneEnvironment.MSAA
            antialiasingQuality: root.effectiveQuality === "high"
                ? SceneEnvironment.High : SceneEnvironment.Medium
        }
        Texture {
            id: surfaceTexture
            sourceItem: textureImage
            generateMipmaps: true
            mipFilter: Texture.Linear
        }
        Node {
            eulerRotation.x: root.bounded("cameraPitch", 25, -60, 60)
            eulerRotation.y: root.bounded("cameraYaw", 0, -180, 180)
            PerspectiveCamera {
                id: camera
                fieldOfView: root.bounded("fieldOfView", 40, 20, 70)
                z: Math.max(1, root.height) / (2 * Math.tan(fieldOfView * Math.PI / 360))
                clipNear: 1
                clipFar: Math.max(100, z * 5)
                // Camera-local coordinates preserve the logical pixel centers
                // without querying viewport matrices during scene construction.
                Repeater3D {
                    model: root.entryGeometry
                    delegate: IconStyle3D {
                        required property var modelData
                        meshData: iconResource.meshData
                        materialData: iconResource.materialData
                        surfaceTexture: root.textureRequired && root.textureReady ? surfaceTexture : null
                        position: Qt.vector3d(Number(modelData.centerX) - root.width / 2,
                            root.height / 2 - Number(modelData.centerY), -camera.z)
                        eulerRotation.x: -20
                        scale: Qt.vector3d(Number(modelData.width) * 0.55,
                                           Number(modelData.height) * 0.55,
                                           Number(modelData.width) * 0.55)
                    }
                }
            }
        }
        DirectionalLight {
            eulerRotation: Qt.vector3d(-35, -35, 0)
            brightness: root.bounded("keyLightBrightness", 1, 0, 4)
            ambientColor: "#202630"
        }
        DirectionalLight {
            eulerRotation: Qt.vector3d(25, 140, 0)
            brightness: root.bounded("fillLightBrightness", 0.4, 0, 2)
            color: "#78c8ff"
        }
        IconStyle3D {
            id: platform
            meshData: (root.resources || ({})).mesh || null
            materialData: (root.resources || ({})).material || ({})
            surfaceTexture: root.textureRequired && root.textureReady ? surfaceTexture : null
            scale: Qt.vector3d(root.width * 0.46, root.height * 0.46,
                               Math.min(root.width, root.height) * 0.46)
        }
        // The shared geometry is retained even with no entries. Its readiness
        // participates in the whole scene's safe fallback decision.
        IconStyle3D {
            id: iconResource
            meshData: (root.resources || ({})).iconMesh || null
            materialData: (root.resources || ({})).material || ({})
            visible: false
        }
    }
}
