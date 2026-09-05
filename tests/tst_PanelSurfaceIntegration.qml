import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// TASK-0032 Phase D: host visibility and surface presentation, joined.
//
// tst_PanelPresentation proves the state machine and tst_PanelMotionTracks
// proves the geometry. Both work on their components in isolation, which is
// exactly how TASK-0032 came to ship a presentation controller that nothing
// read: every rule passed while the live panel stayed permanently open.
//
// This file proves the join. It builds the same chain the applet builds -
// controller feeding a runtime state, runtime state feeding a PanelScene - and
// asserts the behaviours the phase is accountable for: that a panel can be
// host-visible and visually collapsed, that a trigger opens it, that guards
// hold it open, that it closes when they clear, and that auto-hide and a
// closed shell can both be true at once without deadlocking each other.
TestCase {
    id: testCase

    name: "PanelSurfaceIntegration"
    when: windowShown
    width: 640
    height: 240

    readonly property var entries: [
        { id: "one", displayName: "One", iconName: "start-here-kde" },
        { id: "two", displayName: "Two", iconName: "system-file-manager" }
    ]

    Component {
        id: panelComponent

        // Mirrors plasma-dock-widget main.qml: one controller, one scene, and
        // the runtime state that carries the controller's answer to the scene.
        Item {
            id: panel

            property alias controller: presentationController
            property alias scene: panelScene

            property string presentationMode: "collapsed"
            property string presentationTrigger: "hover"
            property string collapseMechanism: "collapse-horizontal"
            property string collapseAxis: "horizontal"
            property bool reducedMotion: false
            property int transitionDuration: 40

            // The observed host fact, reported to the controller rather than
            // used directly - the applet's rule.
            property bool hostConcealed: false

            readonly property bool opensOnHover:
                presentationTrigger === "hover" || presentationTrigger === "edge"

            readonly property var runtimeState: ({
                hovered: false,
                hoveredEntry: -1,
                editMode: presentationController.editMode,
                rendererFallback: "",
                presentationState: presentationController.surfaceState,
                transitionState: presentationController.transitionState,
                presentationProgress: presentationController.progress,
                hostPhase: presentationController.hostPhase
            })

            // Mirrors the applet's pointer rule after the TASK-0032 closure
            // correction: hover and edge open on entry, click opens through a
            // tap, every non-manual trigger closes once the pointer leaves,
            // and a manual panel moves only by explicit request.
            function pointerEntered() {
                presentationController.pointerInside = true
                if (presentationTrigger === "manual")
                    return
                if (opensOnHover)
                    presentationController.requestOpen()
            }

            function pointerLeft() {
                presentationController.pointerInside = false
                if (presentationTrigger === "manual")
                    return
                if (presentationMode === "collapsed")
                    presentationController.requestCollapse()
            }

            function tapped() {
                if (presentationTrigger === "click"
                        && panelScene.collapseProgress >= 1)
                    presentationController.requestOpen()
            }

            function deliverRequest(request) {
                if (request === "open")
                    presentationController.requestOpen()
                else if (request === "collapse"
                         && presentationMode === "collapsed")
                    presentationController.requestCollapse()
            }

            onHostConcealedChanged:
                presentationController.applyHostVisibility(!hostConcealed)

            width: 400
            height: 80

            PanelPresentationController {
                id: presentationController

                restingState: panel.presentationMode
                transitionDuration: panel.transitionDuration
                reducedMotion: panel.reducedMotion
            }

            PanelScene {
                id: panelScene

                anchors.fill: parent
                panelDefinition: ({
                    edge: "bottom",
                    layout: "horizontal",
                    iconSize: 40,
                    spacing: 8,
                    layoutPadding: 10,
                    appearance: "glass",
                    rendererTier: "procedural2d",
                    collapseMechanism: panel.collapseMechanism,
                    collapseAxis: panel.collapseAxis
                })
                runtimeState: panel.runtimeState
                orderedEntries: testCase.entries
                hostCapabilities: ({
                    available: true,
                    renderer: {
                        effectiveTier: "procedural2d",
                        fallbackApplied: false
                    },
                    presentationMechanisms: [
                        { id: "open", available: true },
                        { id: "collapse-horizontal", available: true },
                        { id: "split", available: true },
                        { id: "shutter", available: true }
                    ]
                })
            }
        }
    }

    function makePanel(properties) {
        const panel = createTemporaryObject(
            panelComponent, testCase, properties || ({}))
        verify(panel, "panel created")
        return panel
    }

    function settle(panel) {
        // Transitions are short here; wait for the controller to come to rest.
        tryVerify(function() {
            return !panel.controller.transitionActive
        }, 2000)
    }

    // The behaviour the master plan actually asked for: a panel that is on
    // screen and visibly closed at the same time.
    function test_aPanelCanBeHostVisibleAndVisuallyCollapsed() {
        const panel = makePanel()
        settle(panel)

        compare(panel.controller.hostVisible, true, "host is showing it")
        compare(panel.controller.surfaceState, "collapsed", "surface is closed")
        compare(panel.controller.hostPhase, "revealed")
        compare(panel.scene.collapseProgress, 1, "the scene draws it closed")
        compare(panel.scene.presentationTrackForm, "center-slide")
        verify(panel.scene.motionTracks["split-center"].scaleX < 1)
    }

    // The wiring this phase exists to add. Before it, the controller ran and
    // the scene never heard about it.
    function test_theSceneFollowsTheControllerRatherThanItsOwnIdea() {
        const panel = makePanel()
        settle(panel)
        compare(panel.scene.collapseProgress, 1)

        panel.pointerEntered()
        settle(panel)
        compare(panel.controller.surfaceState, "open")
        compare(panel.scene.collapseProgress, 0, "scene opened with it")
        for (const role of ["surface", "split-start", "split-center",
                            "split-end"]) {
            compare(panel.scene.motionTracks[role].scaleX, 1,
                    role + " back at rest")
            compare(panel.scene.motionTracks[role].offsetX, 0,
                    role + " back at rest")
        }
    }

    // The TASK-0032 closure gap: a click could open a panel that then never
    // closed, because only hover asked for a collapse. Every pointer-driven
    // trigger must close once the pointer has left and the guards are clear.
    function test_everyPointerTriggerClosesAfterLeaving_data() {
        return [
            { tag: "hover", trigger: "hover" },
            { tag: "click", trigger: "click" },
            { tag: "edge", trigger: "edge" }
        ]
    }

    function test_everyPointerTriggerClosesAfterLeaving(data) {
        const panel = makePanel({ presentationTrigger: data.trigger })
        settle(panel)
        compare(panel.controller.surfaceState, "collapsed")

        panel.pointerEntered()
        if (data.trigger === "click") {
            compare(panel.controller.surfaceState, "collapsed",
                    "hovering alone does not open a click-triggered panel")
            panel.tapped()
        }
        settle(panel)
        compare(panel.controller.surfaceState, "open", data.tag + " opened")

        panel.pointerLeft()
        settle(panel)
        compare(panel.controller.surfaceState, "collapsed",
                data.tag + " closed after the pointer left")
        compare(panel.scene.collapseProgress, 1)
    }

    // A manual panel is driven by explicit requests only. The pointer never
    // opens or closes it, so it can rest collapsed without becoming
    // unreachable, and it stays open until something asks it to close.
    function test_manualMovesOnlyByExplicitRequest() {
        const panel = makePanel({ presentationTrigger: "manual" })
        settle(panel)
        compare(panel.controller.surfaceState, "collapsed")

        panel.pointerEntered()
        panel.tapped()
        settle(panel)
        compare(panel.controller.surfaceState, "collapsed",
                "the pointer never drives a manual panel")

        panel.deliverRequest("open")
        settle(panel)
        compare(panel.controller.surfaceState, "open")

        panel.pointerLeft()
        settle(panel)
        compare(panel.controller.surfaceState, "open",
                "leaving does not close a manual panel")

        panel.deliverRequest("collapse")
        settle(panel)
        compare(panel.controller.surfaceState, "collapsed")
        compare(panel.scene.collapseProgress, 1)
    }

    function test_hoverOpensAndLeavingCloses() {
        const panel = makePanel()
        settle(panel)

        panel.pointerEntered()
        settle(panel)
        compare(panel.controller.surfaceState, "open")

        panel.pointerLeft()
        settle(panel)
        compare(panel.controller.surfaceState, "collapsed")
        compare(panel.scene.collapseProgress, 1)
    }

    // A dock that closes under an open context menu is the defect the guard
    // vocabulary exists to prevent; this proves it end to end rather than at
    // the controller's own interface.
    function test_itStaysOpenWhileInUseAndClosesWhenGuardsClear_data() {
        return [
            { tag: "menu", guard: "popupOpen" },
            { tag: "drag", guard: "dragActive" },
            { tag: "keyboard", guard: "keyboardFocus" },
            { tag: "preview", guard: "windowPreviewOpen" }
        ]
    }

    function test_itStaysOpenWhileInUseAndClosesWhenGuardsClear(data) {
        const panel = makePanel()
        panel.pointerEntered()
        settle(panel)
        compare(panel.controller.surfaceState, "open")

        panel.controller[data.guard] = true
        panel.pointerLeft()
        settle(panel)
        compare(panel.controller.surfaceState, "open",
                data.tag + " holds the panel open")
        compare(panel.scene.collapseProgress, 0)

        panel.controller[data.guard] = false
        settle(panel)
        compare(panel.controller.surfaceState, "collapsed",
                data.tag + " released, panel closed")
        compare(panel.scene.collapseProgress, 1)
    }

    // Host concealment and surface collapse are separate layers. A conceal must
    // not be reported to the renderer as a collapse, and the surface the user
    // was looking at must survive the round trip.
    function test_concealAndCollapseRemainSeparate_data() {
        return [
            { tag: "from-open", open: true, expected: "open" },
            { tag: "from-collapsed", open: false, expected: "collapsed" }
        ]
    }

    function test_concealAndCollapseRemainSeparate(data) {
        const panel = makePanel()
        settle(panel)
        if (data.open) {
            panel.pointerEntered()
            settle(panel)
        }
        const surfaceBefore = panel.controller.surfaceState
        compare(surfaceBefore, data.expected)

        panel.hostConcealed = true
        settle(panel)
        compare(panel.controller.hostVisible, false, "host took it off screen")
        compare(panel.controller.hostPhase, "concealed")
        // A conceal is not a collapse: the renderer is never told to crossfade.
        compare(panel.controller.transitionState, "idle")

        panel.hostConcealed = false
        settle(panel)
        compare(panel.controller.hostVisible, true)
        compare(panel.controller.surfaceState, surfaceBefore,
                "the surface came back as it was")
    }

    // A host conceal is an observed fact, so it wins over the guards. A panel
    // Plasma has already hidden cannot be reported as visible because a menu
    // happens to be open - that is the deadlock the criterion forbids.
    function test_autoHideAndAClosedShellCoexistWithoutDeadlock() {
        const panel = makePanel()
        panel.pointerEntered()
        settle(panel)
        panel.controller.popupOpen = true
        compare(panel.controller.surfaceState, "open")

        panel.hostConcealed = true
        settle(panel)
        compare(panel.controller.hostVisible, false,
                "the guard did not block an observed conceal")

        panel.hostConcealed = false
        panel.controller.popupOpen = false
        panel.controller.pointerInside = false
        settle(panel)
        compare(panel.controller.hostVisible, true)
        // Nothing is left in flight and nothing is left pending: the panel came
        // to rest at a state it can actually be drawn in.
        verify(!panel.controller.transitionActive)
        verify(!panel.controller.pendingRequestActive)
        verify(["open", "collapsed"].includes(panel.controller.surfaceState))
    }

    // Reduced motion still changes state; it simply arrives.
    function test_reducedMotionArrivesWithoutAnimating() {
        const panel = makePanel({ reducedMotion: true })
        compare(panel.controller.surfaceState, "collapsed")
        compare(panel.scene.collapseProgress, 1)

        panel.pointerEntered()
        // No settle(): with reduced motion the state must already be final.
        compare(panel.controller.transitionActive, false)
        compare(panel.controller.surfaceState, "open")
        compare(panel.scene.collapseProgress, 0)
    }

    // A panel whose resting state is open never collapses on its own.
    function test_anAlwaysOpenPanelIgnoresAPointerLeaving() {
        const panel = makePanel({ presentationMode: "open" })
        settle(panel)
        compare(panel.controller.surfaceState, "open")

        panel.pointerEntered()
        panel.pointerLeft()
        settle(panel)
        compare(panel.controller.surfaceState, "open")
        compare(panel.scene.collapseProgress, 0)
    }

    // The scene must refuse a mechanism the resolution did not allow, even
    // though the panel definition asks for it.
    function test_anUnavailableMechanismIsRefusedAtTheScene() {
        const panel = makePanel({ collapseMechanism: "collapse-radial" })
        settle(panel)
        compare(panel.scene.resolvedPresentationMechanism, "collapse-radial")
        compare(panel.scene.presentationTrackForm, "safe-fade")
        compare(panel.scene.mechanismFallbackReason, "mechanism-unavailable")
    }
}
