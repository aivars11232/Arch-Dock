import QtQuick
import QtQuick3D

Item {
    id: root

    required property var sceneDefinition
    required property var resources
    property url textureSource: ""
    property var entryGeometry: []
    property var entryVisuals: []
    property string quality: "medium"
    property real panelOpacity: 1
    property bool sceneConcealed: false
    property real layoutAngle: 0
    property real collapseProgress: 0
    property string mechanism: "open"
    property bool hovered: false
    property bool reducedMotion: false
    property real glowIntensity: 1
    readonly property bool motionAllowed: rendererReady && !sceneConcealed && visible && panelOpacity > 0
    readonly property real emissionScale: Math.max(0, Math.min(2, glowIntensity)) * (hovered ? 1.18 : 1)
    readonly property var panelParts: partsForScope("panel")
    readonly property var entryParts: partsForScope("entry")
    readonly property bool partsReady: {
        const definitions = (sceneDefinition || ({})).parts || []
        const data = (resources || ({})).parts || []
        return definitions.length === data.length && data.every(function(part) {
            return part && part.mesh && part.mesh.format === "org.archdock.mesh"
                && part.material && part.material.format === "org.archdock.material"
        })
    }

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
        && platform.meshReady && iconResource.meshReady && partsReady
    readonly property bool textureRequired: Boolean(sceneDefinition && sceneDefinition.texture)
    readonly property bool textureReady: !textureRequired || textureImage.status === Image.Ready
    readonly property bool geometryWithinBudget: triangleCount * 3 <= Math.min(262144,
        Number((resources || ({})).indexBudget || 262144))
    readonly property bool rendererReady: resourcesReady && textureReady && geometryWithinBudget
    readonly property string errorReason: !resourcesReady ? "scene3d-mesh-unavailable"
        : textureRequired && textureImage.status === Image.Error ? "scene3d-texture-unavailable"
        : !geometryWithinBudget ? "scene3d-resource-limit"
        : !textureReady ? "renderer-loading" : ""
    readonly property int triangleCount: platform.triangleCount
        + (iconResource.triangleCount + 12) * entryGeometry.length
        + partTriangles(panelParts) + partTriangles(entryParts) * entryGeometry.length
    readonly property var qualityState: ({ quality: effectiveQuality,
        targetWidth: targetWidth, targetHeight: targetHeight,
        samples: effectiveQuality === "low" ? 1 : effectiveQuality === "high" ? 4 : 2 })
    readonly property var viewport: view
    readonly property bool frameRendered: view.renderStats.frameTime > 0

    function bounded(key, fallback, minimum, maximum) {
        const value = Number((sceneDefinition || ({}))[key])
        return Number.isFinite(value) ? Math.max(minimum, Math.min(maximum, value)) : fallback
    }

    function partsForScope(scope) {
        const definitions = (sceneDefinition || ({})).parts || []
        const data = (resources || ({})).parts || []
        const result = []
        for (let index = 0; index < definitions.length; ++index) {
            if (definitions[index].scope === scope)
                result.push({ definition: definitions[index], resources: data[index] || ({}) })
        }
        return result
    }
    function partTriangles(parts) {
        return parts.reduce(function(count, part) {
            return count + Number((part.resources.mesh || ({})).indexes?.length || 0) / 3
        }, 0)
    }
    function partOpenAmount(part) {
        return part.mechanism === mechanism ? Math.max(0, Math.min(1, 1 - collapseProgress)) : 1
    }
    function number(object, key, fallback) {
        const value = Number((object || ({}))[key])
        return Number.isFinite(value) ? value : fallback
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
                    // A count model preserves nodes and textures during rotation.
                    model: root.geometryWithinBudget ? root.entryGeometry.length : 0
                    delegate: Node {
                        id: entryNode
                        required property int index
                        objectName: "mesh-entry-" + index
                        readonly property var rect: root.entryGeometry[index] || ({})
                        readonly property var entry: root.entryVisuals[index] || null
                        readonly property var visual: entry ? entry.meshVisualItem : null
                        readonly property var iconMotion: root.motionAllowed && entry ? entry.iconMotion : ({})
                        readonly property var glyphMotion: root.motionAllowed && entry ? entry.glyphMotion : ({})
                        readonly property var tileMotion: root.motionAllowed && entry ? entry.tileMotion : ({})
                        readonly property real size: Number(rect.width || 1)
                        readonly property real hoverScale: entry ? Number(entry.visualScale || 1) : 1
                        readonly property real glow: Math.max(root.number(iconMotion, "glow", 0),
                            root.number(glyphMotion, "glow", 0), root.number(tileMotion, "glow", 0))
                        position: Qt.vector3d(Number(rect.centerX) - root.width / 2 + root.number(iconMotion, "x", 0),
                            root.height / 2 - Number(rect.centerY) - root.number(iconMotion, "y", 0), -camera.z)
                        eulerRotation: Qt.vector3d(0, root.number(iconMotion, "rotateY", 0),
                            -root.number(iconMotion, "rotateZ", 0) - Number(rect.rotation || 0))
                        scale: Qt.vector3d(hoverScale * root.number(iconMotion, "scale", 1) * root.number(iconMotion, "scaleX", 1),
                            hoverScale * root.number(iconMotion, "scale", 1) * root.number(iconMotion, "scaleY", 1), 1)
                        opacity: root.number(iconMotion, "opacity", 1)

                        Texture {
                            id: glyphTexture
                            sourceItem: entryNode.visual ? entryNode.visual.glyphTextureItem : null
                        }
                        Texture {
                            id: tileTexture
                            sourceItem: entryNode.visual ? entryNode.visual.tileTextureItem : null
                        }
                        IconStyle3D {
                            meshData: iconResource.meshData
                            materialData: iconResource.materialData
                            surfaceTexture: entryNode.visual ? tileTexture
                                : root.textureRequired && root.textureReady ? surfaceTexture : null
                            position: Qt.vector3d(root.number(entryNode.tileMotion, "x", 0),
                                -root.number(entryNode.tileMotion, "y", 0), -entryNode.size * 0.58)
                            eulerRotation: Qt.vector3d(-20, root.number(entryNode.tileMotion, "rotateY", 0),
                                -root.number(entryNode.tileMotion, "rotateZ", 0))
                            scale: Qt.vector3d(entryNode.size * 0.55 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleX", 1),
                                entryNode.size * 0.55 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleY", 1), entryNode.size * 0.55)
                            opacity: root.number(entryNode.tileMotion, "opacity", 1)
                            emissionScale: root.emissionScale * (1 + entryNode.glow)
                        }
                        Model {
                            objectName: "mesh-glyph-" + entryNode.index
                            source: "#Cube"
                            pickable: false
                            visible: entryNode.visual !== null && root.collapseProgress < 1
                            position: Qt.vector3d(root.number(entryNode.glyphMotion, "x", 0),
                                -root.number(entryNode.glyphMotion, "y", 0), 0)
                            eulerRotation: Qt.vector3d(0, root.number(entryNode.glyphMotion, "rotateY", 0),
                                -root.number(entryNode.glyphMotion, "rotateZ", 0))
                            scale: Qt.vector3d(entryNode.size / 100 * root.number(entryNode.glyphMotion, "scale", 1)
                                    * root.number(entryNode.glyphMotion, "scaleX", 1),
                                entryNode.size / 100 * root.number(entryNode.glyphMotion, "scale", 1)
                                    * root.number(entryNode.glyphMotion, "scaleY", 1), entryNode.size / 4000)
                            opacity: root.number(entryNode.glyphMotion, "opacity", 1) * (1 - root.collapseProgress)
                            materials: PrincipledMaterial {
                                lighting: PrincipledMaterial.NoLighting
                                alphaMode: PrincipledMaterial.Blend
                                baseColorMap: glyphTexture
                            }
                        }
                        Node {
                            scale: Qt.vector3d(entryNode.size * 0.55, entryNode.size * 0.55, entryNode.size * 0.55)
                            Repeater3D {
                                model: root.entryParts.length
                                delegate: IconStyle3D {
                                    required property int index
                                    objectName: "mesh-entry-part-" + entryNode.index + "-" + index
                                    partDefinition: root.entryParts[index].definition
                                    meshData: root.entryParts[index].resources.mesh || null
                                    materialData: root.entryParts[index].resources.material || ({})
                                    openAmount: root.partOpenAmount(partDefinition)
                                    emissionScale: root.emissionScale * (1 + entryNode.glow)
                                }
                            }
                        }
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
        Node {
            objectName: "mesh-platform-motion"
            eulerRotation.z: -root.layoutAngle
            scale: Qt.vector3d(root.width * 0.46, root.height * 0.46,
                               Math.min(root.width, root.height) * 0.46)
            IconStyle3D {
                id: platform
                meshData: (root.resources || ({})).mesh || null
                materialData: (root.resources || ({})).material || ({})
                surfaceTexture: root.textureRequired && root.textureReady ? surfaceTexture : null
                emissionScale: root.emissionScale
            }
            Repeater3D {
                model: root.panelParts.length
                delegate: IconStyle3D {
                    required property int index
                    objectName: "mesh-panel-part-" + index
                    partDefinition: root.panelParts[index].definition
                    meshData: root.panelParts[index].resources.mesh || null
                    materialData: root.panelParts[index].resources.material || ({})
                    openAmount: root.partOpenAmount(partDefinition)
                    emissionScale: root.emissionScale
                }
            }
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
