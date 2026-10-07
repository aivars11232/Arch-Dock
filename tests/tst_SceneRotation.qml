import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// TASK-0033 Phase C: whole-scene rotation for free radial panels, and
// ADREP-TASK-002: entries travelling along the panel's own path.
//
// The controller is proved on its own, then through PanelScene: the offset it
// yields is added to the configured layout angle and every geometry consumer
// receives the sum, so hover targets, popup anchors and the drawn surface turn
// with the icons. A native host, an unsupported layout, an unavailable
// capability, reduced motion, a drag, edit mode and concealment all stop it.
// The scenes here turn the whole panel ("Continuous motion moves" Whole
// panel) unless a test asks for the entries to travel.
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
                panelRotationTrigger: "idle",
                panelMotionTarget: "panel"
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
        // A step eases in over 120 ms.
        tryCompare(scene, "effectiveLayoutAngle", 15, 1000,
                   "scroll up turns clockwise with automatic motion off")
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
        tryCompare(scene, "effectiveLayoutAngle", 0, 1000, "scroll down reverses the turn")
        mouseWheel(scene, point.x, point.y, 120, 0)
        mouseWheel(scene, point.x, point.y, 0, 120, Qt.NoButton, Qt.ShiftModifier)
        wait(200)
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
        wait(200)
        compare(scene.effectiveLayoutAngle, 0, data.tag + " preserves the angle")
        compare(scene.wheelRotationTarget, 0, data.tag + " takes no step")
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
        tryCompare(scene, "effectiveLayoutAngle", 15, 1000, "wheel up turns from the bare ring")
        mouseWheel(scene, surface.x, surface.y, 0, -120)
        tryCompare(scene, "effectiveLayoutAngle", 0, 1000, "wheel down turns back")
        mouseWheel(scene, surface.x, surface.y, 0, -120)
        tryCompare(scene, "effectiveLayoutAngle", 345, 1000, "and on past the start")
        mouseWheel(scene, surface.x, surface.y, 0, 120)
        tryCompare(scene, "effectiveLayoutAngle", 0, 1000)
        compare(testCase.passedWheelCount, 0, "the ring consumed every event")

        // The empty interior and the corner belong to the desktop.
        mouseWheel(scene, centre.x, centre.y, 0, 120)
        mouseWheel(scene, 1, 1, 0, -120)
        wait(200)
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

    // ---- ADREP-TASK-002: entries travel along the panel's own path -------

    function travelScene(overrides, properties) {
        return stillScene(Object.assign({ panelMotionTarget: "items" }, overrides || ({})),
                          properties)
    }

    function entryCentres(scene) {
        const result = []
        for (let index = 0; index < scene.entryCount; ++index) {
            const bounds = scene.entryGeometryAt(index).entryBounds
            result.push(Qt.point(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2))
        }
        return result
    }

    function atRest(scene) {
        return scene.wheelTravel === scene.wheelTravelTarget
            && scene.wheelTravel === Math.round(scene.wheelTravel)
    }

    function distanceToOutline(point, outline) {
        const points = outline.points
        let best = Infinity
        const last = outline.closed ? points.length : points.length - 1
        for (let index = 0; index < last; ++index) {
            const a = points[index]
            const b = points[(index + 1) % points.length]
            const dx = b.x - a.x
            const dy = b.y - a.y
            const share = Math.max(0, Math.min(1, ((point.x - a.x) * dx + (point.y - a.y) * dy)
                                                / Math.max(1e-9, dx * dx + dy * dy)))
            best = Math.min(best, Math.hypot(point.x - a.x - share * dx, point.y - a.y - share * dy))
        }
        return best
    }

    // Criterion: on a closed path one notch moves every entry one slot along
    // the panel's own outline, into its neighbour's place, and the panel does
    // not turn. Half way, an entry is on the outline itself: on a polygon's
    // edge or the star's points, never on a circle standing in for them.
    function test_wheelMovesEntriesAlongTheOutlineNotTheSurface_data() {
        return [
            { tag: "circular", layout: "circular" },
            { tag: "ellipse", layout: "ellipse" },
            { tag: "ring", layout: "ring" },
            { tag: "triangle", layout: "triangle" },
            { tag: "square", layout: "square" },
            { tag: "hexagon", layout: "hexagon" },
            { tag: "star", layout: "star" }
        ]
    }

    function test_wheelMovesEntriesAlongTheOutlineNotTheSurface(data) {
        const scene = travelScene({ layout: data.layout })
        compare(scene.motionTarget, "items")
        compare(scene.wheelTravelAvailable, true)
        compare(scene.wheelRotationAvailable, false, "the wheel does not turn the panel")
        compare(scene.trackWindow.loop, scene.entryCount, "a closed path carries every entry round")
        const before = entryCentres(scene)
        const outline = LayoutEngine.surface(data.layout, scene.layoutGeometry, 0, scene.polygonSides)
        const width = scene.width
        mouseWheel(scene, before[0].x, before[0].y, 0, 120)
        compare(scene.wheelTravelTarget, 1, "one notch is one slot")
        tryVerify(function() { return atRest(scene) }, 1000)
        compare(scene.entryTravel, 1)
        compare(scene.effectiveLayoutAngle, 0, "the panel did not turn")
        compare(scene.sceneRotationAngle, 0)
        compare(scene.width, width, "travel does not resize the panel")
        const after = entryCentres(scene)
        const size = scene.layoutGeometry.iconSize
        for (let index = 0; index < scene.entryCount; ++index) {
            const next = before[(index + 1) % scene.entryCount]
            fuzzyCompare(after[index].x, next.x, 0.01)
            fuzzyCompare(after[index].y, next.y, 0.01)
            const item = scene.entryItemAt(index)
            fuzzyCompare(item.x + size / 2, after[index].x, 0.01)
            fuzzyCompare(item.y + size / 2, after[index].y, 0.01)
            verify(item.visible && item.enabled, "entry " + index + " stays usable")
            verify(scene.containsInputPoint(after[index]),
                   "entry " + index + " takes input where it went")
            const entry = scene.entryGeometryAt(index)
            const anchor = scene.popupAnchors.entries[index]
            fuzzyCompare(anchor.x, after[index].x + entry.outwardNormal.x * size / 2, 0.01)
            fuzzyCompare(anchor.y, after[index].y + entry.outwardNormal.y * size / 2, 0.01)
        }
        // Half a slot on: every entry is on the drawn outline.
        scene.wheelTravel = 1.5
        for (const point of entryCentres(scene)) {
            verify(distanceToOutline(Qt.point(point.x - scene.contentBounds.x,
                                              point.y - scene.contentBounds.y), outline) < 0.75,
                   data.tag + ": an entry between slots left the outline")
        }
        scene.wheelTravel = 1
        mouseWheel(scene, after[0].x, after[0].y, 0, -120)
        tryVerify(function() { return atRest(scene) }, 1000)
        const back = entryCentres(scene)
        for (let index = 0; index < scene.entryCount; ++index) {
            fuzzyCompare(back[index].x, before[index].x, 0.01)
            fuzzyCompare(back[index].y, before[index].y, 0.01)
        }
    }

    // Criterion: on an open path the entries form a loop longer than the
    // path, so one leaving one end comes back at the other, whether or not
    // all of them fit. Entries off the path are invisible and take no input.
    function test_openPathsWrapAround_data() {
        return [
            { tag: "semicircle-crowded", layout: "semicircle", count: 14 },
            { tag: "arc-fits", layout: "arc", count: 4 },
            { tag: "fan", layout: "fan", count: 6 },
            { tag: "radial", layout: "radial", count: 6 },
            { tag: "spiral", layout: "spiral", count: 5 }
        ]
    }

    function test_openPathsWrapAround(data) {
        const many = []
        for (let index = 0; index < data.count; ++index)
            many.push({ id: "entry-" + index, displayName: "Entry " + index })
        const scene = travelScene({ layout: data.layout, layoutRadius: 150, iconSize: 52,
                                    spacing: 8 }, { orderedEntries: many })
        const capacity = scene.trackWindow.capacity
        const loop = scene.trackWindow.loop
        compare(scene.trackWindow.travels, true)
        verify(loop > capacity, "the loop is longer than the path")
        verify(capacity >= 3)
        const rest = entryCentres(scene)
        const slots = rest.slice(0, capacity)
        const width = scene.width
        const height = scene.height
        function expectAt(travel) {
            for (let index = 0; index < scene.entryCount; ++index) {
                const slot = ((index + travel) % loop + loop) % loop
                const item = scene.entryItemAt(index)
                const point = entryCentres(scene)[index]
                if (slot < capacity) {
                    verify(item.visible && item.enabled,
                           data.tag + ": entry " + index + " in slot " + slot + " is shown")
                    fuzzyCompare(point.x, slots[slot].x, 0.01)
                    fuzzyCompare(point.y, slots[slot].y, 0.01)
                    verify(scene.containsInputPoint(point))
                } else {
                    verify(!item.visible, data.tag + ": entry " + index + " off the path is invisible")
                    verify(!item.enabled, data.tag + ": entry " + index + " off the path takes no input")
                    compare(scene.entryRects[index].width, 0)
                }
            }
        }
        expectAt(0)
        // The wheel over a slot the entries pass through: down moves them
        // back along the path, so the first leaves the start and comes back
        // at the end; up brings it back the same way.
        const aim = slots[Math.floor(capacity / 2)]
        for (let step = 1; step <= loop + 1; ++step) {
            mouseWheel(scene, aim.x, aim.y, 0, -120)
            tryVerify(function() { return atRest(scene) }, 1000)
            compare(((scene.entryTravel + step) % loop + loop) % loop, 0,
                    data.tag + ": one slot back per notch")
            expectAt(scene.entryTravel)
        }
        for (let step = 1; step <= 2; ++step) {
            mouseWheel(scene, aim.x, aim.y, 0, 120)
            tryVerify(function() { return atRest(scene) }, 1000)
            expectAt(scene.entryTravel)
        }
        compare(scene.effectiveLayoutAngle, 0, "travelling does not turn the scene")
        compare(scene.width, width, "travelling does not resize the scene")
        compare(scene.height, height)
        // Half a slot on, the entry leaving fades and takes no input.
        scene.wheelTravel = Math.round(scene.wheelTravel) + 0.5
        let fading = 0
        for (let index = 0; index < scene.entryCount; ++index) {
            const item = scene.entryItemAt(index)
            if (item.visible && item.opacity < 0.99) {
                ++fading
                verify(!item.enabled, "a fading entry takes no input")
            }
        }
        verify(fading >= 1, data.tag + ": an entry fades out or in at an end")
        verify(scene.panelDefinition.travel === undefined, "travel is not a setting")
    }

    // Criterion: motion starts within a frame of the wheel, and each step
    // eases out in at most 120 ms.
    function test_aStepStartsAtOnceAndEndsWithin120Milliseconds() {
        const scene = travelScene()
        const point = entryCentres(scene)[0]
        const started = Date.now()
        mouseWheel(scene, point.x, point.y, 0, 120)
        compare(scene.wheelTravelTarget, 1)
        compare(scene.stepDuration, 100)
        // One frame later the entries are on their way.
        wait(24)
        verify(scene.wheelTravel > 0, "moving within a frame")
        verify(scene.wheelTravel < 1, "and easing in")
        tryVerify(function() { return atRest(scene) }, 1000)
        const total = Date.now() - started
        verify(total <= 250, "the step took " + total + " ms")
    }

    // Criterion: spinning the wheel builds no backlog. Steps that arrive
    // while one runs retarget it, and the entries rest one step's time after
    // the last notch.
    function test_aFastSpinBuildsNoBacklog() {
        const scene = travelScene()
        const point = entryCentres(scene)[0]
        for (let notch = 0; notch < 10; ++notch)
            mouseWheel(scene, point.x, point.y, 0, 120)
        compare(scene.wheelTravelTarget, 10, "every notch counted")
        const last = Date.now()
        tryVerify(function() { return atRest(scene) }, 1000)
        const settled = Date.now() - last
        verify(settled <= 250, "rested " + settled + " ms after the last notch")
        compare(((scene.entryTravel % scene.entryCount) + scene.entryCount) % scene.entryCount,
                10 % scene.entryCount)
    }

    // Criterion: wheel input gathers into whole steps - 120 angle units are
    // a notch, a slot's distance of touchpad pixels is a step - and Scroll
    // sensitivity scales it from a quarter to four slots a notch.
    function test_wheelInputGathersIntoWholeSteps() {
        const scene = travelScene({}, { animationProfiles: { reducedMotion: true } })
        const loop = scene.trackWindow.loop
        function slot() { return ((scene.wheelTravelTarget % loop) + loop) % loop }
        for (let event = 0; event < 7; ++event)
            compare(scene.takeWheel(15, 0), 0, "a part of a notch takes no step")
        compare(scene.takeWheel(15, 0), 1, "eight high-resolution events make one notch")
        compare(slot(), 1)
        const pitch = scene.travelPitch.pixels
        verify(pitch > 20, "a slot is " + pitch + " px on the ring")
        fuzzyCompare(scene.travelPitch.degrees, 360 / scene.entryCount, 0.01)
        compare(scene.takeWheel(0, pitch / 2), 0)
        compare(scene.takeWheel(0, pitch / 2), 1, "one slot of touchpad travel is one step")
        compare(slot(), 2)
        compare(scene.takeWheel(-240, 0), -2, "and back")
        compare(slot(), 0)
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, { scrollSensitivity: 0.25 })
        for (let notch = 0; notch < 3; ++notch)
            compare(scene.takeWheel(120, 0), 0, "a quarter of a slot per notch")
        compare(scene.takeWheel(120, 0), 1)
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, { scrollSensitivity: 4 })
        compare(scene.takeWheel(120, 0), 4, "four slots per notch")
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, { scrollSensitivity: 9 })
        compare(scene.scrollSensitivity, 4, "sensitivity is held to 4x")
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, { scrollSensitivity: 0.01 })
        compare(scene.scrollSensitivity, 0.25, "and to a quarter")
    }

    // Criterion: "Continuous motion moves" Items, Whole panel and Both each
    // move what they name, at their own speeds, and the wheel follows the
    // same choice. Stopping leaves the entries in slots.
    function test_continuousMotionMovesItemsPanelOrBoth_data() {
        return [
            { tag: "items", target: "items", turns: false, travels: true },
            { tag: "panel", target: "panel", turns: true, travels: false },
            { tag: "both", target: "both", turns: true, travels: true }
        ]
    }

    function test_continuousMotionMovesItemsPanelOrBoth(data) {
        const scene = makeScene()
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, {
            panelMotionTarget: data.target, panelTravelSpeed: 4, panelRotationSpeed: 90 })
        compare(scene.sceneRotationEnabled, data.turns)
        compare(scene.travelMotionEnabled, data.travels)
        compare(scene.wheelRotationAvailable, data.turns, "the wheel turns what motion turns")
        compare(scene.wheelTravelAvailable, data.travels, "the wheel moves what motion moves")
        wait(400)
        compare(scene.sceneRotationAngle > 5, data.turns, data.tag + " turns the panel")
        compare(scene.entryTravel > 0.3, data.travels, data.tag + " moves the entries")
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, { panelRotationMode: "none" })
        compare(scene.sceneRotationActive, false)
        compare(scene.travelMotionActive, false)
        tryVerify(function() { return atRest(scene) }, 1000, "the entries come to rest in slots")
        compare(scene.sceneRotationAngle, 0, "the panel returns to its configured angle")
    }

    // Criterion: reduced motion makes wheel steps jump and keeps continuous
    // motion off.
    function test_reducedMotionJumpsAndHoldsStill() {
        const scene = makeScene({ animationProfiles: { reducedMotion: true } })
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, { panelMotionTarget: "both" })
        compare(scene.travelMotionActive, false, "continuous travel stays off")
        compare(scene.sceneRotationActive, false, "continuous turning stays off")
        const point = entryCentres(scene)[0]
        mouseWheel(scene, point.x, point.y, 0, 120)
        compare(scene.entryTravel, 1, "the step lands at once")
        compare(scene.effectiveLayoutAngle, 15)
        wait(150)
        compare(scene.entryTravel, 1)
    }

    // Criterion: a drag moves the entries with the pointer while the panel
    // stays still; released, they rest in slots.
    function test_dragMovesEntriesAlongThePath() {
        const scene = travelScene()
        const centre = scene.travelCentre()
        const radius = scene.layoutGeometry.radius
        function point(angle) {
            return Qt.point(centre.x + radius * Math.cos(angle), centre.y + radius * Math.sin(angle))
        }
        const start = point(Math.PI / 4)
        mousePress(scene, start.x, start.y)
        compare(scene.rotationDragActive, true)
        // Four entries: a slot is a quarter turn, so an eighth is half a slot.
        const finish = point(Math.PI / 2)
        mouseMove(scene, finish.x, finish.y, 20)
        verify(Math.abs(scene.wheelTravel - 0.5) < 0.05, "travel " + scene.wheelTravel)
        compare(scene.effectiveLayoutAngle, 0, "the panel stays still")
        mouseRelease(scene, finish.x, finish.y)
        compare(scene.rotationDragActive, false)
        tryVerify(function() { return atRest(scene) }, 1000, "released entries rest in slots")
    }

    // Criterion: travel is browsing state: another layout starts at rest, and
    // nothing of it is saved.
    function test_travelIsTransient() {
        const scene = travelScene()
        const point = entryCentres(scene)[0]
        mouseWheel(scene, point.x, point.y, 0, 120)
        tryVerify(function() { return atRest(scene) }, 1000)
        compare(scene.entryTravel, 1)
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, { layout: "hexagon" })
        compare(scene.entryTravel, 0, "another layout starts with its entries in place")
        compare(scene.wheelTravelTarget, 0)
        for (const key of Object.keys(scene.panelDefinition))
            verify(key.toLowerCase().indexOf("travel") < 0 || key === "panelTravelSpeed",
                   key + " is not travel state")
    }

    // An entry delegate that takes keyboard focus the way the applet's does:
    // only while the scene lets it take input.
    Component {
        id: focusableEntry

        FocusScope {
            anchors.fill: parent
            activeFocusOnTab: Boolean(parent && parent.sceneInputEnabled)
            readonly property int entryIndex: parent ? parent.sceneIndex : -1
        }
    }

    // Criterion: keyboard selection follows the travelled entries. Tab moves
    // through the entries on the path only, and the focused one is where it
    // travelled to.
    function test_keyboardFocusFollowsTravelledEntries() {
        const many = []
        for (let index = 0; index < 6; ++index)
            many.push({ id: "entry-" + index, displayName: "Entry " + index })
        const scene = travelScene({ layout: "arc", layoutRadius: 150, iconSize: 52, spacing: 8 },
                                  { orderedEntries: many, entryDelegate: focusableEntry })
        compare(scene.trackWindow.loop, 7)
        scene.takeWheel(120, 0)
        tryVerify(function() { return atRest(scene) }, 1000)
        // Entry 5 waits off the path; the others moved one slot on.
        const hidden = scene.entryItemAt(5)
        compare(hidden.enabled, false)
        const reached = []
        scene.entryItemAt(0).forceActiveFocus()
        for (let press = 0; press < 8; ++press) {
            let focused = -1
            for (let index = 0; index < scene.entryCount; ++index) {
                const delegate = scene.entryItemAt(index).delegateItem
                if (delegate && delegate.activeFocus)
                    focused = index
            }
            if (focused >= 0 && !reached.includes(focused)) {
                reached.push(focused)
                const item = scene.entryItemAt(focused)
                const output = scene.entryGeometryAt(focused)
                fuzzyCompare(item.x, output.position.x, 0.01)
                fuzzyCompare(item.y, output.position.y, 0.01)
                verify(output.onTrack, "a focused entry stands on the path")
            }
            keyClick(Qt.Key_Tab)
        }
        reached.sort()
        compare(reached, [0, 1, 2, 3, 4], "Tab reaches the entries on the path, and only them")
    }

    // Criterion: an edge panel, or a free panel's straight row, neither
    // travels nor turns under the wheel.
    function test_edgePanelsAndRowsDoNotTravel_data() {
        return [
            { tag: "native", context: { hostKind: "native" }, layout: "ring" },
            { tag: "free-row", context: { hostKind: "free" }, layout: "horizontal" }
        ]
    }

    function test_edgePanelsAndRowsDoNotTravel(data) {
        const scene = travelScene({ layout: data.layout }, { entryDelegateContext: data.context })
        compare(scene.travelGeometryAvailable, false)
        compare(scene.wheelTravelAvailable, false)
        const point = entryCentres(scene)[0]
        mouseWheel(scene, point.x, point.y, 0, 120)
        wait(200)
        compare(scene.entryTravel, 0)
        compare(scene.wheelTravelTarget, 0)
        compare(scene.effectiveLayoutAngle, 0)
    }

    // Criterion: Direction Up, Right, Down and Left are layout angles that
    // turn each open shape, and its hit region, to that side. A fan, an arc
    // and a semicircle face up at 0 degrees, a radial path faces right.
    function test_openShapesFaceTheChosenSide_data() {
        const rows = []
        const sides = { up: [0, -1], right: [1, 0], down: [0, 1], left: [-1, 0] }
        const angles = {
            fan: { up: 0, right: 90, down: 180, left: -90 },
            arc: { up: 0, right: 90, down: 180, left: -90 },
            semicircle: { up: 0, right: 90, down: 180, left: -90 },
            radial: { up: -90, right: 0, down: 90, left: 180 }
        }
        for (const layout of Object.keys(angles))
            for (const side of Object.keys(sides))
                rows.push({ tag: layout + "-" + side, layout: layout, angle: angles[layout][side],
                            side: sides[side] })
        return rows
    }

    function test_openShapesFaceTheChosenSide(data) {
        const scene = travelScene({ layout: data.layout, layoutAngle: data.angle })
        const centre = scene.travelCentre()
        // The middle of the drawn path, and of the entries standing on it.
        const path = LayoutEngine.surface(data.layout, scene.layoutGeometry,
                                          scene.effectiveLayoutAngle, scene.polygonSides).points
        const before = path[path.length / 2 - 1]
        const after = path[path.length / 2]
        const offset = { x: (before.x + after.x) / 2 + scene.contentBounds.x - centre.x,
                         y: (before.y + after.y) / 2 + scene.contentBounds.y - centre.y }
        const along = offset.x * data.side[0] + offset.y * data.side[1]
        const across = Math.abs(offset.x * data.side[1] - offset.y * data.side[0])
        verify(along > 20 && across < 1, data.tag + ": the path's middle faces the side "
               + JSON.stringify(offset))
        const points = entryCentres(scene)
        const reach = Math.max.apply(null, points.map(function(point) {
            return (point.x - centre.x) * data.side[0] + (point.y - centre.y) * data.side[1]
        }))
        verify(reach > along * 0.5, data.tag + ": the entries stand on that side")
        const radius = scene.layoutGeometry.radius
        verify(scene.containsInputPoint(Qt.point(centre.x + data.side[0] * radius,
                                                 centre.y + data.side[1] * radius)),
               data.tag + ": the path's middle on that side takes input")
        verify(!scene.containsInputPoint(Qt.point(centre.x - data.side[0] * radius,
                                                  centre.y - data.side[1] * radius)),
               data.tag + ": the opposite side passes through")
    }
}
