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
}
