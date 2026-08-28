import QtQuick
import QtTest
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "RenderingModuleImport"

    Component {
        id: panelSceneComponent

        PanelScene {}
    }

    Component {
        id: iconSceneComponent

        IconScene {}
    }

    Component {
        id: livePanelPreviewComponent

        LivePanelPreview {}
    }

    function test_installedModuleIdentity() {
        compare(RenderingModuleProbe.uri, "ArchDock.Rendering")
        compare(RenderingModuleProbe.majorVersion, 1)
        compare(RenderingModuleProbe.minorVersion, 0)
        compare(RenderingModuleProbe.ready, true)
    }

    function test_panelSceneContractLoads() {
        const scene = createTemporaryObject(panelSceneComponent, testCase, {
            panelDefinition: {
                layout: "horizontal",
                iconSize: 32,
                spacing: 6,
                layoutPadding: 10,
                rendererTier: "procedural2d"
            },
            orderedEntries: [
                { id: "one", displayName: "One" },
                { id: "two", displayName: "Two" }
            ]
        })
        verify(scene !== null)
        compare(scene.entryCount, 2)
        compare(scene.effectiveRendererTier, "procedural2d")
        verify(scene.visualPanel !== null)
    }

    function test_iconSceneContractLoads() {
        const scene = createTemporaryObject(iconSceneComponent, testCase, {
            entry: {
                iconName: "application-x-executable",
                running: true,
                windowCount: 2
            },
            logicalSize: 36,
            running: true,
            showIndicator: true,
            reducedMotion: true
        })
        verify(scene !== null)
        compare(scene.width, 36)
        compare(scene.visualState, "running")
        compare(scene.indicatorItem.objectName, "running-indicator")
    }

    function test_livePanelPreviewContractLoads() {
        const preview = createTemporaryObject(
            livePanelPreviewComponent, testCase, {
                width: 320,
                height: 160,
                previewMode: "vertical",
                panelDefinition: {
                    layout: "adaptive",
                    iconSize: 32,
                    spacing: 6,
                    rendererTier: "procedural2d"
                }
            })
        verify(preview !== null)
        compare(preview.panelSceneItem.layoutPath, "vertical")
        compare(preview.activeRendererTier, "procedural2d")
        verify(preview.panelSceneItem.visualPanel !== null)
    }
}
