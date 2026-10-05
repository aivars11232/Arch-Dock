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

    function test_backgroundDragRotatesBothDirections() {
        const scene = makeScene({ rotationAnimationEnabled: false })
        const center = Qt.point(scene.width / 2, scene.height / 2)
        const radius = scene.layoutGeometry.radius
        function point(angle) {
            return Qt.point(center.x + radius * Math.cos(angle),
                            center.y + radius * Math.sin(angle))
        }
        const start = point(Math.PI / 4)
        const finish = point(Math.PI / 3)
        mousePress(scene, start.x, start.y)
        compare(scene.rotationDragActive, true)
        mouseMove(scene, finish.x, finish.y, 20)
        verify(scene.wheelRotationAngle > 10)
        mouseMove(scene, start.x, start.y, 20)
        verify(Math.min(scene.wheelRotationAngle, 360 - scene.wheelRotationAngle) < 1)
        mouseRelease(scene, start.x, start.y)
        compare(scene.rotationDragActive, false)
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

    function test_wheelTurnsEntriesAndAnchorsInBothDirections() {
        const scene = makeScene({rotationAnimationEnabled: false})
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, {panelRotationMode: "none"})
        const before = scene.entryGeometryAt(0)
        const anchor = scene.popupAnchors.entries[0]
        const width = scene.width
        const height = scene.height
        mouseWheel(scene, before.position.x + before.entryBounds.width / 2,
            before.position.y + before.entryBounds.height / 2, 0, 120)
        compare(scene.effectiveLayoutAngle, 15, "scroll up turns clockwise with automatic motion off")
        compare(scene.width, width)
        compare(scene.height, height)
        const after = scene.entryGeometryAt(0)
        verify(after.position.y > before.position.y, "the right-hand entry moves clockwise")
        verify(scene.popupAnchors.entries[0].y > anchor.y, "popup anchors move with the glyph")
        fuzzyCompare(scene.entryItemAt(0).x, after.position.x, 0.001)
        fuzzyCompare(scene.entryItemAt(0).y, after.position.y, 0.001)
        const point = Qt.point(after.position.x + after.entryBounds.width / 2,
            after.position.y + after.entryBounds.height / 2)
        verify(scene.containsInputPoint(point))
        mouseWheel(scene, point.x, point.y, 0, -120)
        compare(scene.effectiveLayoutAngle, 0, "scroll down reverses the turn")
        mouseWheel(scene, point.x, point.y, 120, 0)
        mouseWheel(scene, point.x, point.y, 0, 120, Qt.NoButton, Qt.ShiftModifier)
        compare(scene.effectiveLayoutAngle, 0, "horizontal and modified scrolling remain available")
    }

    function test_wheelDoesNotTurnDuringInteractionGuards_data() {
        return [
            {tag: "drag", runtime: {dragInProgress: true}},
            {tag: "edit", runtime: {editMode: true}},
            {tag: "popup", runtime: {popupOpen: true}},
            {tag: "preview", input: false},
            {tag: "native", native: true},
            {tag: "concealed", concealed: true}
        ]
    }

    function test_wheelDoesNotTurnDuringInteractionGuards(data) {
        const scene = makeScene({rotationAnimationEnabled: false})
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, {panelRotationMode: "none"})
        if (data.runtime) scene.runtimeState = data.runtime
        if (data.input === false) scene.entryInteractionEnabled = false
        if (data.native) scene.entryDelegateContext = {hostKind: "native"}
        if (data.concealed) scene.sceneConcealed = true
        const entry = scene.entryGeometryAt(0)
        mouseWheel(scene, entry.position.x + entry.entryBounds.width / 2,
            entry.position.y + entry.entryBounds.height / 2, 0, 120)
        compare(scene.effectiveLayoutAngle, 0, data.tag + " preserves the angle")
    }

    // ---- Wheel surface, spacing and overcrowded curves -------------------

    // Wheel events no scene accepted: what the desktop behind a free panel
    // would receive.
    property int passedWheelCount: 0

    WheelHandler {
        target: null
        onWheel: function(event) { testCase.passedWheelCount += 1 }
    }

    function stillScene(overrides, properties) {
        const scene = makeScene(Object.assign(
            { rotationAnimationEnabled: false }, properties || ({})))
        scene.panelDefinition = Object.assign({}, scene.panelDefinition,
            { panelRotationMode: "none" }, overrides || ({}))
        return scene
    }

    // Criterion: the wheel works anywhere on the drawn surface - on the bare
    // surface between two icons too - in both directions, and nowhere else.
    function test_wheelTurnsFromTheSurfaceBetweenEntries() {
        const scene = stillScene()
        const centre = Qt.point(scene.width / 2, scene.height / 2)
        const radius = scene.layoutGeometry.radius
        // Four entries stand at 0, 90, 180 and 270 degrees; 45 is bare ring.
        const surface = Qt.point(centre.x + radius * Math.cos(Math.PI / 4),
                                 centre.y + radius * Math.sin(Math.PI / 4))
        for (let index = 0; index < scene.entryCount; ++index) {
            const bounds = scene.entryGeometryAt(index).entryBounds
            verify(surface.x < bounds.x || surface.x > bounds.x + bounds.width
                   || surface.y < bounds.y || surface.y > bounds.y + bounds.height,
                   "the probe point is not over entry " + index)
        }
        verify(scene.containsInputPoint(surface), "the bare ring takes input")
        testCase.passedWheelCount = 0
        mouseWheel(scene, surface.x, surface.y, 0, 120)
        compare(scene.effectiveLayoutAngle, 15, "wheel up turns from the bare ring")
        mouseWheel(scene, surface.x, surface.y, 0, -120)
        compare(scene.effectiveLayoutAngle, 0, "wheel down turns back")
        mouseWheel(scene, surface.x, surface.y, 0, -120)
        compare(scene.effectiveLayoutAngle, 345, "and on past the start")
        mouseWheel(scene, surface.x, surface.y, 0, 120)
        compare(scene.effectiveLayoutAngle, 0)
        compare(testCase.passedWheelCount, 0, "the ring consumed every event")

        // The empty interior and the corner belong to the desktop.
        mouseWheel(scene, centre.x, centre.y, 0, 120)
        mouseWheel(scene, 1, 1, 0, -120)
        compare(scene.effectiveLayoutAngle, 0, "neither turns the ring")
        compare(testCase.passedWheelCount, 2, "both events passed through")
    }

    // Criterion: the canonical spacing control moves the logical, drawn,
    // popup-anchor and input geometry of a ring together.
    function test_spacingMovesEveryGeometryConsumerTogether() {
        const scene = stillScene()
        const centre = Qt.point(scene.width / 2, scene.height / 2)
        const size = scene.layoutGeometry.iconSize
        const width = scene.width
        const even = []
        for (let index = 0; index < scene.entryCount; ++index)
            even.push(scene.entryGeometryAt(index).position)

        scene.panelDefinition = Object.assign({}, scene.panelDefinition, { spacing: 2 })
        compare(scene.width, width, "spacing does not resize the ring")
        verify(Math.hypot(scene.entryGeometryAt(1).position.x - even[1].x,
                          scene.entryGeometryAt(1).position.y - even[1].y) > 10,
               "a small spacing moves the entry along the ring")
        for (let index = 0; index < scene.entryCount; ++index) {
            const entry = scene.entryGeometryAt(index)
            const middle = Qt.point(entry.position.x + size / 2,
                                    entry.position.y + size / 2)
            fuzzyCompare(Math.hypot(middle.x - centre.x, middle.y - centre.y),
                         scene.layoutGeometry.radius, 0.001)
            const item = scene.entryItemAt(index)
            fuzzyCompare(item.x, entry.position.x, 0.001)
            fuzzyCompare(item.y, entry.position.y, 0.001)
            verify(item.visible && item.enabled, "entry " + index + " stays usable")
            verify(scene.containsInputPoint(middle),
                   "entry " + index + " takes input where it is drawn")
            const anchor = scene.popupAnchors.entries[index]
            fuzzyCompare(anchor.x, middle.x + entry.outwardNormal.x * size / 2, 0.001)
            fuzzyCompare(anchor.y, middle.y + entry.outwardNormal.y * size / 2, 0.001)
        }
        // Neighbours are closer than before, and never overlap.
        const first = scene.entryGeometryAt(1).position
        const second = scene.entryGeometryAt(2).position
        const separation = Math.hypot(first.x - second.x, first.y - second.y)
        verify(separation < Math.hypot(even[1].x - even[2].x, even[1].y - even[2].y) - 10)
        verify(separation >= size - 0.5, "neighbours do not overlap")

        // The default and anything above it restore the even ring.
        for (const spacing of [8, 30]) {
            scene.panelDefinition = Object.assign({}, scene.panelDefinition, { spacing: spacing })
            for (let index = 0; index < scene.entryCount; ++index) {
                fuzzyCompare(scene.entryGeometryAt(index).position.x, even[index].x, 0.001)
                fuzzyCompare(scene.entryGeometryAt(index).position.y, even[index].y, 0.001)
            }
        }
    }

    // Criterion: an overcrowded semicircle keeps its entries on the exact
    // half circle and shows a window of them. The wheel moves the window in
    // both directions; hidden entries take no input; the offset is transient.
    function test_overcrowdedSemicircleBrowsesAlongTheCurve() {
        const many = []
        for (let index = 0; index < 14; ++index)
            many.push({ id: "entry-" + index, displayName: "Entry " + index })
        const scene = stillScene({ layout: "semicircle", layoutRadius: 150,
                                   iconSize: 52, spacing: 8 },
                                 { orderedEntries: many })
        const capacity = Math.floor(Math.PI * 150 / 60) + 1
        compare(capacity, 8)
        compare(scene.trackWindow.windowed, true, "fourteen icons do not fit")
        compare(scene.trackWindow.capacity, capacity)
        compare(scene.wheelBrowseAvailable, true)
        compare(scene.browseOffset, 0)
        const width = scene.width
        const height = scene.height
        const centre = Qt.point(width / 2, height / 2)

        function shown() {
            const result = []
            for (let index = 0; index < scene.entryCount; ++index) {
                if (scene.entryItemAt(index).visible)
                    result.push(index)
            }
            return result
        }
        function range(first) {
            const result = []
            for (let index = first; index < first + capacity; ++index)
                result.push(index)
            return result
        }
        function verifyWindow(first) {
            compare(shown(), range(first))
            for (let index = 0; index < scene.entryCount; ++index) {
                const entry = scene.entryGeometryAt(index)
                const item = scene.entryItemAt(index)
                const middle = Qt.point(entry.position.x + 26, entry.position.y + 26)
                // Every entry, shown or not, is on the half circle.
                fuzzyCompare(Math.hypot(middle.x - centre.x, middle.y - centre.y), 150, 0.001)
                verify(middle.y <= centre.y + 0.001, "entry " + index + " is on the upper half")
                if (index < first || index >= first + capacity) {
                    verify(!item.visible && !item.enabled,
                           "entry " + index + " outside the window takes no input")
                    continue
                }
                verify(item.enabled, "entry " + index + " takes input")
                fuzzyCompare(item.x, entry.position.x, 0.001)
                fuzzyCompare(item.y, entry.position.y, 0.001)
                verify(scene.containsInputPoint(middle))
                const anchor = scene.popupAnchors.entries[index]
                fuzzyCompare(anchor.x, middle.x + entry.outwardNormal.x * 26, 0.001)
                fuzzyCompare(anchor.y, middle.y + entry.outwardNormal.y * 26, 0.001)
            }
            // The two ends of the window are level: a half circle, no tail.
            fuzzyCompare(scene.entryGeometryAt(first).position.y,
                         scene.entryGeometryAt(first + capacity - 1).position.y, 0.001)
            const neighbour = Math.hypot(
                scene.entryGeometryAt(first).position.x - scene.entryGeometryAt(first + 1).position.x,
                scene.entryGeometryAt(first).position.y - scene.entryGeometryAt(first + 1).position.y)
            fuzzyCompare(neighbour, 2 * 150 * Math.sin(60 / 150 / 2), 0.001)
        }

        verifyWindow(0)
        const apex = Qt.point(centre.x, centre.y - 150)
        const slot = scene.entryGeometryAt(1).position
        mouseWheel(scene, apex.x, apex.y, 0, -120)
        compare(scene.browseOffset, 1, "wheel down moves to the next entry")
        verifyWindow(1)
        fuzzyCompare(scene.entryGeometryAt(2).position.x, slot.x, 0.001)
        fuzzyCompare(scene.entryGeometryAt(2).position.y, slot.y, 0.001)
        mouseWheel(scene, apex.x, apex.y, 0, 120)
        compare(scene.browseOffset, 0, "wheel up moves back")
        mouseWheel(scene, apex.x, apex.y, 0, 120)
        compare(scene.browseOffset, 0, "the window stops at the first entry")
        for (let notch = 0; notch < 20; ++notch)
            mouseWheel(scene, apex.x, apex.y, 0, -120)
        compare(scene.browseOffset, 14 - capacity, "the window stops at the last entry")
        verifyWindow(14 - capacity)
        compare(scene.effectiveLayoutAngle, 0, "browsing does not turn the scene")
        compare(scene.width, width, "browsing does not resize the scene")
        compare(scene.height, height)
        verify(scene.panelDefinition.browseOffset === undefined,
               "the offset is not a setting")

        // A curve that holds its entries is as before: the wheel turns it.
        scene.orderedEntries = many.slice(0, 5)
        compare(scene.trackWindow.windowed, false)
        compare(scene.wheelBrowseAvailable, false)
        compare(scene.browseOffset, 0, "the offset is forgotten")
        for (let index = 0; index < 5; ++index)
            verify(scene.entryItemAt(index).visible && scene.entryItemAt(index).enabled)
        mouseWheel(scene, apex.x, apex.y, 0, 120)
        compare(scene.effectiveLayoutAngle, 15)
    }
}
