import QtQuick
import QtTest
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "LivePanelPreview"
    width: 720
    height: 420
    when: windowShown

    readonly property var previewEntries: [
        {
            id: "launcher",
            displayName: "Launcher",
            iconName: "start-here-kde",
            running: false
        },
        {
            id: "browser",
            displayName: "Browser",
            iconName: "applications-internet",
            running: false
        },
        {
            id: "files",
            displayName: "Files",
            iconName: "system-file-manager",
            running: false
        }
    ]

    function definition(overrides) {
        const result = {
            schemaVersion: 2,
            edge: "bottom",
            layout: "adaptive",
            layoutPadding: 12,
            iconSize: 42,
            spacing: 7,
            appearance: "glass",
            opacity: 0.92,
            rendererTier: "procedural2d"
        }
        const changes = overrides || {}
        const keys = Object.keys(changes)
        for (let index = 0; index < keys.length; ++index)
            result[keys[index]] = changes[keys[index]]
        return result
    }

    function capabilities(overrides) {
        const renderer = {
            requestedTier: "procedural2d",
            effectiveTier: "procedural2d",
            fallbackApplied: false,
            reasonCode: ""
        }
        const changes = overrides || {}
        const keys = Object.keys(changes)
        for (let index = 0; index < keys.length; ++index)
            renderer[keys[index]] = changes[keys[index]]
        return {
            available: true,
            renderer: renderer
        }
    }

    Component {
        id: previewComponent

        LivePanelPreview {
            width: 680
            height: 360
            orderedEntries: testCase.previewEntries
            panelDefinition: testCase.definition()
            hostCapabilities: testCase.capabilities()
            animationProfiles: ({ reducedMotion: true })
        }
    }

    function createPreview(properties) {
        const preview = createTemporaryObject(
            previewComponent, testCase, properties || {})
        verify(preview !== null)
        wait(0)
        return preview
    }

    function iconAt(preview, index) {
        const slot = preview.panelSceneItem.iconDelegates.itemAt(index)
        verify(slot !== null)
        verify(slot.delegateItem !== null)
        return slot.delegateItem
    }

    function test_modesUseOneResolvedPanelSceneWithoutMutatingTheDraft() {
        const draft = definition()
        const preview = createPreview({ panelDefinition: draft })

        compare(preview.previewMode, "horizontal")
        compare(preview.panelSceneItem.layoutPath, "horizontal")
        compare(preview.panelSceneItem.verticalLayout, false)
        compare(preview.resolvedPanelDefinition.edge, "bottom")

        preview.previewMode = "vertical"
        wait(0)
        compare(preview.panelSceneItem.layoutPath, "vertical")
        compare(preview.panelSceneItem.verticalLayout, true)
        compare(preview.resolvedPanelDefinition.edge, "left")

        preview.previewMode = "free"
        wait(0)
        compare(preview.panelSceneItem.layoutPath, "circular")
        compare(preview.resolvedPanelDefinition.edge, "free")
        compare(draft.layout, "adaptive")
        compare(draft.edge, "bottom")
    }

    function test_openCollapsedHoverAndExplicitIconStates() {
        const preview = createPreview()
        compare(preview.presentationState, "open")
        compare(preview.collapseProgress, 0)
        compare(preview.previewRuntimeState.hovered, false)

        // The preview no longer fades and shrinks its own card. Its collapse
        // is the scene's collapse, so a card cannot advertise a mechanism the
        // desktop would not perform. A procedural panel with no declared
        // mechanism therefore reports a collapse that draws nothing, which is
        // the truthful answer rather than a fabricated shrink.
        preview.presentationState = "collapsed"
        wait(0)
        compare(preview.collapseProgress, 1)
        compare(preview.collapseProgress,
                preview.panelSceneItem.collapseProgress)
        compare(preview.presentationTrackForm,
                preview.panelSceneItem.presentationTrackForm)
        compare(preview.presentationTrackForm, "identity")

        // Given a mechanism the resolver allows, the same preview draws the
        // real track - and it is the scene's, not one of its own.
        preview.panelDefinition = definition({
            collapseMechanism: "collapse-horizontal",
            collapseAxis: "horizontal"
        })
        wait(0)
        compare(preview.presentationTrackForm, "center-slide")
        compare(preview.presentationTrackForm,
                preview.panelSceneItem.presentationTrackForm)
        compare(preview.mechanismFallbackReason, "")
        verify(preview.panelSceneItem.motionTracks["split-center"].scaleX < 1)

        preview.presentationState = "open"
        preview.hoveredEntry = 1
        preview.transitionState = "opening"
        preview.presentationProgress = 0.4
        wait(0)
        compare(iconAt(preview, 1).visualState, "hover")
        compare(preview.previewRuntimeState.hovered, true)
        compare(preview.previewRuntimeState.transitionState, "opening")
        compare(preview.previewRuntimeState.presentationProgress, 0.4)
        compare(preview.panelSceneItem.panelHovered, true)
        compare(preview.panelSceneItem.transitionState, "opening")
        compare(preview.panelSceneItem.presentationProgress, 0.4)

        const states = [
            "pressed", "active", "running", "minimized", "urgent",
            "launching", "drop", "disabled", "edit"
        ]
        preview.hoveredEntry = -1
        for (let index = 0; index < states.length; ++index) {
            preview.iconState = states[index]
            preview.stateEntry = 2
            wait(0)
            compare(iconAt(preview, 2).visualState, states[index])
        }
    }

    function test_rendererTierAndFallbackAreThePanelSceneOutputs() {
        const preview = createPreview({
            panelDefinition: definition({ rendererTier: "skinned2d" }),
            hostCapabilities: capabilities({
                requestedTier: "skinned2d",
                effectiveTier: "procedural2d",
                fallbackApplied: true,
                reasonCode: "renderer-host-unsupported"
            })
        })

        compare(preview.activeRendererTier, "procedural2d")
        compare(preview.fallbackApplied, true)
        compare(preview.fallbackReason, "renderer-host-unsupported")
        compare(preview.rendererStatusText,
                "procedural2d · renderer-host-unsupported")
    }

    function test_safeProceduralVisualSnapshots() {
        const preview = createPreview()
        wait(20)
        const openImage = grabImage(preview.panelSceneItem)
        compare(openImage.width, preview.panelSceneItem.width)
        compare(openImage.height, preview.panelSceneItem.height)
        verify(openImage.alpha(Math.round(openImage.width / 2),
                               Math.round(openImage.height / 2)) > 0)

        preview.previewMode = "vertical"
        wait(20)
        const verticalImage = grabImage(preview.panelSceneItem)
        verify(verticalImage.width < verticalImage.height)
        verify(!openImage.equals(verticalImage))

        preview.previewMode = "free"
        wait(20)
        const freeImage = grabImage(preview.panelSceneItem)
        verify(freeImage.width > verticalImage.width)
        verify(freeImage.height > openImage.height)
        verify(!verticalImage.equals(freeImage))
    }
}
