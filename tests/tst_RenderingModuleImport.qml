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
        id: iconStyleRendererComponent

        IconStyle2D {}
    }

    Component {
        id: livePanelPreviewComponent

        LivePanelPreview {}
    }

    Component {
        id: presentationControllerComponent

        PanelPresentationController {}
    }

    Component {
        id: motionControllerComponent

        PanelMotionController {}
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

    function test_iconStyleRendererContractLoads() {
        const renderer = createTemporaryObject(
            iconStyleRendererComponent, testCase, {
                width: 48,
                height: 48,
                role: "base",
                styleDefinition: ({
                    layers: {
                        base: [{
                            id: "probe",
                            kind: "procedural",
                            shape: "circle",
                            color: "#123456"
                        }]
                    }
                })
            })
        verify(renderer !== null)
        compare(renderer.layerCount, 1)
        compare(renderer.renderedLayerIds[0], "probe")
        compare(IconStyleResolver.stateId({ disabled: true, edit: false }),
                "disabled")
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

    function test_presentationControllerContractLoads() {
        const controller = createTemporaryObject(
            presentationControllerComponent, testCase, {
                restingState: "collapsed"
            })
        verify(controller !== null)
        compare(controller.phase, "collapsed")
        compare(controller.surfaceState, "collapsed")
        compare(controller.transitionState, "idle")
        compare(controller.hostPhase, "revealed")
        compare(controller.hostVisible, true)
        compare(controller.progress, -1)
        compare(PresentationStates.isRestingState("collapsed"), true)
        compare(PresentationStates.isRestingState("collapsing"), false)
    }

    function test_motionControllerContractLoads() {
        const controller = createTemporaryObject(
            motionControllerComponent, testCase, {
                surfaceWidth: 300,
                surfaceHeight: 60,
                handleExtent: 30,
                mechanism: "collapse-horizontal",
                axis: "horizontal",
                surfaceState: "collapsed",
                transitionState: "idle"
            })
        verify(controller !== null)
        compare(controller.trackForm, "center-slide")
        compare(controller.resolvedMechanism, "collapse-horizontal")
        compare(controller.fallbackReason, "")
        compare(controller.collapseProgress, 1)
        verify(controller.tracks["split-center"] !== undefined)
        verify(controller.contentClip !== undefined)
    }
}
