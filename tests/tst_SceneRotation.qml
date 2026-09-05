import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// TASK-0033 Phase C: whole-scene rotation for free radial panels.
//
// The controller is proved on its own, then through PanelScene: the offset it
// yields is added to the configured layout angle and every geometry consumer
// receives the sum, so hover targets, popup anchors and the drawn surface turn
// with the icons. A native host, an unsupported layout, an unavailable
// capability, reduced motion, a drag, edit mode and concealment all stop it.
TestCase {
    id: testCase

    name: "SceneRotation"
    when: windowShown
    // TestCase is invisible by default and the controller refuses to turn an
    // invisible scene, so this harness shows its scenes for real.
    visible: true
    width: 480
    height: 480

    readonly property var entries: [
        { id: "one", displayName: "One", iconName: "start-here-kde" },
        { id: "two", displayName: "Two", iconName: "system-file-manager" },
        { id: "three", displayName: "Three", iconName: "utilities-terminal" },
        { id: "four", displayName: "Four", iconName: "applications-internet" }
    ]

    Component {
        id: controllerComponent

        SceneRotationController {}
    }

    Component {
        id: sceneComponent

        // The scene sizes itself from its geometry, as it does in the applet;
        // forcing a size here would move the ring away from the box centre.
        PanelScene {
            orderedEntries: testCase.entries
            panelDefinition: ({
                edge: "free",
                layout: "ring",
                layoutRadius: 120,
                iconSize: 40,
                spacing: 8,
                layoutPadding: 12,
                layoutAngle: 0,
                appearance: "glass",
                rendererTier: "procedural2d",
                panelRotationMode: "clockwise",
                panelRotationSpeed: 90,
                panelRotationTrigger: "idle"
            })
            runtimeState: ({ hovered: false, hoveredEntry: -1, editMode: false,
                             dragInProgress: false, rendererFallback: "" })
            hostCapabilities: ({
                available: true,
                renderer: { effectiveTier: "procedural2d", fallbackApplied: false },
                rotation: { available: true, support: "arbitrary" }
            })
            entryDelegateContext: ({ hostKind: "free" })
        }
    }

    function makeController(properties) {
        const controller = createTemporaryObject(
            controllerComponent, testCase, properties || ({}))
        verify(controller, "controller created")
        return controller
    }

    function makeScene(properties) {
        const scene = createTemporaryObject(
            sceneComponent, testCase, properties || ({}))
        verify(scene, "scene created")
        return scene
    }

    function test_controllerAdvancesInTheConfiguredDirection() {
        const clockwise = makeController({ mode: "clockwise", available: true,
                                          speedDegreesPerSecond: 90 })
        compare(clockwise.enabled, true)
        compare(clockwise.running, true)
        clockwise.advance(500)
        fuzzyCompare(clockwise.angleOffset, 45, 0.001)

        const counter = makeController({ mode: "counter-clockwise",
                                        available: true,
                                        speedDegreesPerSecond: 90 })
        counter.advance(500)
        fuzzyCompare(counter.angleOffset, 315, 0.001)
        counter.advance(4000)
        verify(counter.angleOffset >= 0 && counter.angleOffset < 360,
               "offset stays in [0, 360)")
    }

    function test_controllerStopsForEveryPauseCondition_data() {
        return [
            { tag: "drag", property: "dragActive" },
            { tag: "edit", property: "editMode" },
            { tag: "configuring", property: "configuring" },
            { tag: "concealed", property: "sceneConcealed" }
        ]
    }

    function test_controllerStopsForEveryPauseCondition(data) {
        const controller = makeController({ mode: "clockwise", available: true })
        controller.advance(1000)
        const held = controller.angleOffset
        verify(held > 0)
        controller[data.property] = true
        compare(controller.running, false, data.tag + " pauses the turn")
        // A pause holds the angle so resuming continues smoothly.
        compare(controller.angleOffset, held, data.tag + " keeps the angle")
        controller[data.property] = false
        compare(controller.running, true)
    }

    function test_reducedMotionAndDisablingReturnToTheConfiguredAngle() {
        const controller = makeController({ mode: "clockwise", available: true })
        controller.advance(1000)
        verify(controller.angleOffset > 0)
        controller.reducedMotion = true
        compare(controller.running, false)
        compare(controller.angleOffset, 0, "reduced motion rests at the layout angle")
        controller.reducedMotion = false
        controller.advance(1000)
        verify(controller.angleOffset > 0)
        controller.mode = "none"
        compare(controller.enabled, false)
        compare(controller.angleOffset, 0, "switching off rests at the layout angle")
    }

    function test_unavailableCapabilityNeverRotates() {
        const controller = makeController({ mode: "clockwise", available: false })
        compare(controller.enabled, false)
        compare(controller.running, false)
        controller.advance(1000)
        // advance() is the ticker's callback; the ticker never runs here, and
        // an explicit call still cannot make an unavailable rotation "on".
        controller.available = true
        controller.available = false
        compare(controller.angleOffset, 0)
    }

    function test_hoverTriggerRunsOnlyWhileHovered() {
        const controller = makeController({ mode: "clockwise", available: true,
                                           trigger: "hover" })
        compare(controller.running, false)
        controller.hovered = true
        compare(controller.running, true)
        controller.hovered = false
        compare(controller.running, false)
    }

    function test_unknownVocabularyFallsBackSafely() {
        const controller = makeController({ mode: "sideways", available: true,
                                           trigger: "whenever",
                                           speedDegreesPerSecond: NaN })
        compare(controller.normalizedMode, "none")
        compare(controller.normalizedTrigger, "idle")
        compare(controller.safeSpeed, 12)
        compare(controller.enabled, false)
    }

    // The scene: geometry, anchors and the drawn surface all follow one angle,
    // and the panel keeps one size while it turns.
    function test_sceneGeometryAnchorsAndSurfaceTurnTogether() {
        const scene = makeScene()
        compare(scene.sceneRotationEnabled, true)
        compare(scene.geometryHitRegionActive, true)
        compare(scene.activeInputRegionKind, "geometry-band")
        const width = scene.width
        const height = scene.height
        compare(width, height, "a rotating scene keeps a square envelope")
        const before = scene.entryGeometryAt(0)
        const anchorBefore = scene.popupAnchors.entries[0]

        tryVerify(function() { return scene.sceneRotationAngle > 5 }, 3000)
        compare(scene.width, width, "width does not churn while turning")
        compare(scene.height, height, "height does not churn while turning")
        const after = scene.entryGeometryAt(0)
        verify(Math.abs(after.x - before.x) + Math.abs(after.y - before.y) > 1,
               "entries moved with the rotation")
        const expected = LayoutEngine.entryGeometry(
            "ring", 0, testCase.entries.length, scene.layoutGeometry,
            scene.effectiveLayoutAngle, 6, "upright", "canonical")
        fuzzyCompare(after.position.x - scene.contentBounds.x, expected.position.x, 0.001)
        fuzzyCompare(after.position.y - scene.contentBounds.y, expected.position.y, 0.001)
        const anchorAfter = scene.popupAnchors.entries[0]
        verify(Math.abs(anchorAfter.x - anchorBefore.x)
               + Math.abs(anchorAfter.y - anchorBefore.y) > 1,
               "popup anchors follow the entries")
        // The rendered delegate sits where the geometry says.
        const item = scene.entryItemAt(0)
        verify(item)
        fuzzyCompare(item.x, after.position.x, 0.001)
        fuzzyCompare(item.y, after.position.y, 0.001)
    }

    function test_inputRegionFollowsTheTurningRing() {
        const scene = makeScene({ rotationAnimationEnabled: false })
        compare(scene.sceneRotationEnabled, true)
        const center = Qt.point(scene.width / 2, scene.height / 2)
        compare(scene.containsInputPoint(center), false,
                "the empty ring interior passes through")
        compare(scene.containsInputPoint(Qt.point(1, 1)), false,
                "the corner passes through")
        const entry = scene.entryGeometryAt(0)
        const onEntry = Qt.point(entry.position.x + entry.entryBounds.width / 2,
                                 entry.position.y + entry.entryBounds.height / 2)
        compare(scene.containsInputPoint(onEntry), true, "an entry accepts input")
        // A point on the ring band between entries also accepts input.
        const radius = scene.layoutGeometry.radius
        const band = Qt.point(scene.width / 2 + radius * Math.cos(Math.PI / 5),
                              scene.height / 2 + radius * Math.sin(Math.PI / 5))
        compare(scene.containsInputPoint(band), true, "the ring band accepts input")
    }

    function test_nativeHostsUnsupportedLayoutsAndMissingCapabilityNeverRotate_data() {
        return [
            { tag: "native-host", context: { hostKind: "native" },
              capabilities: null, layout: "ring", expectRegion: false },
            { tag: "capability-unavailable", context: { hostKind: "free" },
              capabilities: { available: true, rotation: { available: false } },
              layout: "ring", expectRegion: true },
            { tag: "linear-layout", context: { hostKind: "free" },
              capabilities: null, layout: "horizontal", expectRegion: false }
        ]
    }

    function test_nativeHostsUnsupportedLayoutsAndMissingCapabilityNeverRotate(data) {
        const properties = { entryDelegateContext: data.context }
        if (data.capabilities)
            properties.hostCapabilities = data.capabilities
        const scene = makeScene(properties)
        const definition = {}
        for (const key of Object.keys(scene.panelDefinition))
            definition[key] = scene.panelDefinition[key]
        definition.layout = data.layout
        scene.panelDefinition = definition
        if (data.tag === "native-host") {
            // A native resolution never reports rotation as available.
            scene.hostCapabilities = { available: true,
                                       rotation: { available: false } }
        }
        compare(scene.sceneRotationEnabled, false, data.tag + " does not rotate")
        compare(scene.sceneRotationAngle, 0)
        compare(scene.geometryHitRegionActive, data.expectRegion)
        wait(120)
        compare(scene.sceneRotationAngle, 0, data.tag + " stays at rest")
        compare(scene.effectiveLayoutAngle, scene.layoutAngle)
    }

    function test_dragEditConcealmentAndReducedMotionStopTheScene_data() {
        return [
            { tag: "drag", runtime: { dragInProgress: true }, reset: false },
            { tag: "edit", runtime: { editMode: true }, reset: false },
            { tag: "concealed", concealed: true, reset: false },
            { tag: "reduced-motion", reducedMotion: true, reset: true }
        ]
    }

    function test_dragEditConcealmentAndReducedMotionStopTheScene(data) {
        const scene = makeScene()
        tryVerify(function() { return scene.sceneRotationAngle > 5 }, 3000)
        if (data.runtime) {
            const runtime = { hovered: false, hoveredEntry: -1, editMode: false,
                              dragInProgress: false, rendererFallback: "" }
            for (const key of Object.keys(data.runtime))
                runtime[key] = data.runtime[key]
            scene.runtimeState = runtime
        }
        if (data.concealed)
            scene.sceneConcealed = true
        if (data.reducedMotion)
            scene.animationProfiles = { reducedMotion: true }
        compare(scene.sceneRotationActive, false, data.tag + " stops the turn")
        const held = scene.sceneRotationAngle
        if (data.reset)
            compare(held, 0, data.tag + " returns to the configured angle")
        else
            verify(held > 0, data.tag + " holds the angle")
        wait(120)
        compare(scene.sceneRotationAngle, held, data.tag + " does not advance")
    }

    function test_previewFreezesRotationButReportsIt() {
        const scene = makeScene({ rotationAnimationEnabled: false })
        compare(scene.sceneRotationEnabled, true)
        compare(scene.sceneRotationActive, false)
        wait(120)
        compare(scene.sceneRotationAngle, 0)
    }
}
