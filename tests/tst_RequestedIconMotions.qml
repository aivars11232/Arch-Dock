import QtQuick
import QtQuick.Window
import QtTest
import ArchDock.Rendering 1.0
import "../plasma-dock-widget/contents/ui" as DockUi

// TASK-0031 acceptance for the motions the master plan asks for by name.
// Every case drives the production DockEntry with the shipped catalog, so a
// preset is proved as the user would get it rather than as a fixture.
TestCase {
    id: testCase

    name: "RequestedIconMotions"
    when: windowShown
    width: 320
    height: 260

    readonly property var catalog: loadCatalog()

    Component {
        id: dockEntryComponent

        DockUi.DockEntry {}
    }

    // Pointer delivery needs a real rendered surface, so the click cases run
    // inside their own window rather than against a detached item.
    Component {
        id: hostWindowComponent

        Window {
            width: 220
            height: 220
            visible: true
            color: "#202833"
        }
    }

    function loadCatalog() {
        const request = new XMLHttpRequest()
        request.open("GET", Qt.resolvedUrl(
            "../data/animation-profiles/builtin-animation-profiles.json"),
            false)
        request.send(null)
        return JSON.parse(request.responseText)
    }

    function catalogMap() {
        const result = ({})
        const profiles = catalog.animationProfiles || []
        for (let index = 0; index < profiles.length; ++index)
            result[profiles[index].id] = profiles[index]
        return result
    }

    function profileFor(id) {
        const profiles = catalog.animationProfiles || []
        for (let index = 0; index < profiles.length; ++index) {
            if (profiles[index].id === id)
                return profiles[index]
        }
        return null
    }

    function geometryFor(layout, edge) {
        const metrics = LayoutEngine.metrics(
            layout, 3, 60, 8, 1, 150, 2, 18, layout === "vertical", 0, 6)
        return LayoutEngine.entryGeometry(
            layout, 1, 3, metrics, 0, 6, "upright", "canonical", edge)
    }

    function createHostedEntry(properties) {
        const window = createTemporaryObject(hostWindowComponent, testCase)
        verify(window !== null)
        verify(waitForRendering(window.contentItem))
        const item = createEntry(properties, window.contentItem)
        item.anchors.centerIn = window.contentItem
        wait(0)
        return item
    }

    function createEntry(properties, host) {
        const values = {
            entry: {
                appId: "org.example.app",
                stableIdentity: "application.org.example.app",
                displayName: "Example",
                iconName: "application-x-executable",
                pinned: true,
                iconPropertiesSupported: true,
                running: false,
                active: false,
                minimized: false,
                attention: false,
                windowCount: 1
            },
            entryIndex: 1,
            vertical: false,
            baseSize: 60,
            magnification: 1.7,
            magnificationEnabled: true,
            hoveredIndex: -1,
            tileShape: "rounded",
            appearance: "glass",
            showReflection: false,
            showIndicator: true,
            showTooltip: false,
            motion: "none",
            motionTrigger: "hover",
            motionIntensity: 1,
            motionDuration: 170,
            motionSpeed: 1,
            reducedMotion: false,
            inputEnabled: true,
            editMode: false,
            acceptDrops: true,
            invoke: function() {},
            reorder: function() {},
            pinUrls: function() {},
            setHoveredIndex: function() {},
            openPanelStudio: function() {},
            openIconProperties: function() {}
        }
        const additions = properties || ({})
        for (const key of Object.keys(additions))
            values[key] = additions[key]
        const item = createTemporaryObject(
            dockEntryComponent, host || testCase, values)
        verify(item !== null)
        wait(0)
        return item
    }

    function hoveredEntry(profileId, extra) {
        const profile = profileFor(profileId)
        verify2(profile !== null, "missing profile " + profileId)
        const values = {
            motion: profileId,
            motionTrigger: "hover",
            animationProfiles: [profile],
            animationCatalog: catalogMap()
        }
        const additions = extra || ({})
        for (const key of Object.keys(additions))
            values[key] = additions[key]
        const item = createEntry(values)
        item.motionController.hovered = true
        wait(0)
        return item
    }

    function verify2(condition, message) {
        if (!condition)
            fail(message)
        verify(true)
    }

    // --- slow-y-turn ------------------------------------------------------

    // The decisive difference from the pre-existing idle-rotate: this turns
    // around the vertical axis and never touches the flat Z rotation.
    function test_yTurnRotatesAroundTheVerticalAxisAndNotAroundZ() {
        const item = hoveredEntry("slow-y-turn")
        verify2(item.motionController.hasChannel("glyph", "rotate-y"),
                "slow-y-turn does not claim glyph/rotate-y")
        verify2(!item.motionController.hasChannel("glyph", "rotate-z"),
                "slow-y-turn claims a flat Z spin")
        verify2(!item.motionController.hasChannel("icon", "rotate-z"),
                "slow-y-turn claims a flat Z spin on the icon")
        compare(item.motionController.conflicts.length, 0)

        tryVerify(function() {
            return Math.abs(item.glyphMotion.rotateY) > 1
        }, 3000, "the glyph never turned")
        compare(item.glyphMotion.rotateZ, 0)
        compare(item.iconMotion.x, 0)
        compare(item.iconMotion.y, 0)
    }

    // Left-to-right: the turn advances through increasing angles rather than
    // oscillating around zero.
    function test_yTurnAdvancesInOneDirection() {
        const item = hoveredEntry("slow-y-turn")
        tryVerify(function() { return item.glyphMotion.rotateY > 5 },
                  3000, "turn did not start")
        const first = item.glyphMotion.rotateY
        tryVerify(function() { return item.glyphMotion.rotateY > first + 5 },
                  3000, "turn did not advance left-to-right")
    }

    function test_yTurnKeepsTheHitAreaStable() {
        const item = hoveredEntry("slow-y-turn")
        tryVerify(function() {
            return Math.abs(item.glyphMotion.rotateY) > 1
        }, 3000, "the glyph never turned")
        compare(item.logicalInputRegion.x, 0)
        compare(item.logicalInputRegion.y, 0)
        compare(item.logicalInputRegion.width, 60)
        compare(item.logicalInputRegion.height, 60)
        compare(item.width, 60)
        compare(item.height, 60)
    }

    // --- jump -------------------------------------------------------------

    function test_jumpMovesAlongTheOutwardNormalOfEveryNativeEdge() {
        const cases = [
            { edge: "bottom", layout: "horizontal", axis: "y", sign: -1 },
            { edge: "top", layout: "horizontal", axis: "y", sign: 1 },
            { edge: "left", layout: "vertical", axis: "x", sign: 1 },
            { edge: "right", layout: "vertical", axis: "x", sign: -1 }
        ]
        for (let index = 0; index < cases.length; ++index) {
            const item = hoveredEntry("jump", {
                entryGeometry: geometryFor(cases[index].layout,
                                           cases[index].edge)
            })
            const axis = cases[index].axis
            const sign = cases[index].sign
            tryVerify(function() {
                return item.iconMotion[axis] * sign > 1
            }, 3000, cases[index].edge + " panel did not jump outward")
            const other = axis === "y" ? "x" : "y"
            verify2(Math.abs(item.iconMotion[other]) < 0.001,
                    cases[index].edge + " jump drifted sideways")
        }
    }

    function test_jumpFollowsRadialNormalsInAFreeLayout() {
        const geometry = geometryFor("circular", "free")
        const item = hoveredEntry("jump", { entryGeometry: geometry })
        tryVerify(function() {
            return Math.hypot(item.iconMotion.x, item.iconMotion.y) > 1
        }, 3000, "ring entry did not jump")
        // The displacement follows the entry's own outward normal.
        const length = Math.hypot(item.iconMotion.x, item.iconMotion.y)
        const dot = (item.iconMotion.x * geometry.outwardNormal.x
                     + item.iconMotion.y * geometry.outwardNormal.y) / length
        verify2(dot > 0.999, "jump did not follow the radial normal: " + dot)
    }

    // --- shake ------------------------------------------------------------

    function test_shakeStaysBoundedAndReturnsToItsAnchor() {
        const item = hoveredEntry("shake-tangent", {
            entryGeometry: geometryFor("horizontal", "bottom")
        })
        tryVerify(function() { return Math.abs(item.iconMotion.x) > 0.5 },
                  3000, "shake never moved")
        // Bounded: 0.06 logical units of a 60 px entry, plus nothing else.
        for (let sample = 0; sample < 40; ++sample) {
            verify2(Math.abs(item.iconMotion.x) <= 3.7,
                    "shake exceeded its bound: " + item.iconMotion.x)
            verify2(Math.abs(item.iconMotion.rotateZ) <= 2.05,
                    "shake tilt exceeded its bound: " + item.iconMotion.rotateZ)
            wait(16)
        }
        // Terminates cleanly: withdraw the trigger and the entry is back on
        // its anchor exactly, with nothing left running.
        item.motionController.hovered = false
        wait(0)
        compare(item.motionController.activeTracks.length, 0)
        compare(item.iconMotion.x, 0)
        compare(item.iconMotion.y, 0)
        compare(item.iconMotion.rotateZ, 0)
    }

    function test_shakeStopsWhenItsTriggerEnds() {
        const item = hoveredEntry("shake-tangent")
        tryVerify(function() { return item.motionController.activeTracks.length > 0 },
                  3000, "shake did not start")
        item.motionController.hovered = false
        wait(0)
        compare(item.motionController.activeTracks.length, 0)
        compare(item.iconMotion.x, 0)
        compare(item.iconMotion.y, 0)
        compare(item.iconMotion.rotateZ, 0)
    }

    // --- bounds -----------------------------------------------------------

    // A theme that reserves two pixels gets two pixels of motion, not more.
    function test_noRequestedMotionEscapesTheDeclaredEffectBounds() {
        const allowance = { left: 2, right: 2, top: 2, bottom: 2 }
        const presets = ["jump", "shake-tangent"]
        for (let index = 0; index < presets.length; ++index) {
            const geometry = geometryFor("horizontal", "bottom")
            geometry.effectAllowance = allowance
            const item = hoveredEntry(presets[index], {
                entryGeometry: geometry,
                motionIntensity: 2.5
            })
            for (let sample = 0; sample < 30; ++sample) {
                verify2(item.iconMotion.x >= -2.0001
                        && item.iconMotion.x <= 2.0001,
                        presets[index] + " escaped horizontally: "
                        + item.iconMotion.x)
                verify2(item.iconMotion.y >= -2.0001
                        && item.iconMotion.y <= 2.0001,
                        presets[index] + " escaped vertically: "
                        + item.iconMotion.y)
                wait(16)
            }
        }
    }

    // --- enlarge ----------------------------------------------------------

    // Enlarge is depth-aware: the glyph in front grows more than the tile
    // behind it, and neither touches the entry's logical size.
    function test_enlargeGrowsLayersWithoutTouchingTheHitArea() {
        const item = hoveredEntry("enlarge")
        tryVerify(function() { return item.tileMotion.scale > 1.05 },
                  3000, "the tile never grew")
        tryVerify(function() { return item.glyphMotion.scale > 1.1 },
                  3000, "the glyph never grew")
        verify2(item.glyphMotion.scale > item.tileMotion.scale,
                "enlarge is not depth-aware")

        compare(item.width, 60)
        compare(item.height, 60)
        compare(item.logicalInputRegion.x, 0)
        compare(item.logicalInputRegion.y, 0)
        compare(item.logicalInputRegion.width, 60)
        compare(item.logicalInputRegion.height, 60)
        // The entry itself is not displaced, so the layout underneath it is
        // exactly where it was.
        compare(item.iconMotion.x, 0)
        compare(item.iconMotion.y, 0)
    }

    // Neighbour influence is a drawing decision, never a layout one.
    function test_neighbourInfluenceIsConfigurableAndNeverMovesTheLayout() {
        const near = createEntry({
            entryIndex: 1, hoveredIndex: 2,
            magnificationRadius: 2.4, magnificationFalloff: "linear"
        })
        const far = createEntry({
            entryIndex: 1, hoveredIndex: 2,
            magnificationRadius: 1.2, magnificationFalloff: "linear"
        })
        verify2(near.influence > far.influence,
                "a wider reach did not influence the neighbour more")
        verify2(near.hoverScale > far.hoverScale, "reach did not reach")

        const cosine = createEntry({
            entryIndex: 1, hoveredIndex: 2,
            magnificationRadius: 2.4, magnificationFalloff: "cosine"
        })
        verify2(Math.abs(cosine.influence - near.influence) > 0.001,
                "the falloff setting changed nothing")

        // Whatever the influence, the logical entry is untouched.
        const items = [near, far, cosine]
        for (let index = 0; index < items.length; ++index) {
            compare(items[index].width, 60)
            compare(items[index].height, 60)
            compare(items[index].logicalInputRegion.width, 60)
            compare(items[index].logicalInputRegion.height, 60)
        }
    }

    // --- spiral and orbit -------------------------------------------------

    function test_pathMotionsStayBoundedAndReturnToTheAnchor() {
        const presets = [
            { id: "spiral", bound: 0.185 },
            { id: "orbit", bound: 0.225 }
        ]
        for (let index = 0; index < presets.length; ++index) {
            const preset = presets[index]
            const item = hoveredEntry(preset.id, {
                entryGeometry: geometryFor("horizontal", "bottom")
            })
            tryVerify(function() {
                return Math.hypot(item.iconMotion.x, item.iconMotion.y) > 0.5
            }, 3000, preset.id + " never moved")

            for (let sample = 0; sample < 45; ++sample) {
                const reach = Math.hypot(item.iconMotion.x, item.iconMotion.y)
                verify2(reach <= preset.bound * 60 + 0.01,
                        preset.id + " left its bound: " + reach)
                wait(16)
            }

            // Withdraw the trigger and the icon is exactly on its anchor: the
            // offset is computed from absolute channel values, so repeated
            // cycles cannot accumulate drift.
            item.motionController.hovered = false
            wait(0)
            compare(item.iconMotion.x, 0)
            compare(item.iconMotion.y, 0)
            compare(item.iconMotion.scale, 1)
            compare(item.iconMotion.opacity, 1)
        }
    }

    function test_orbitTracesACircleAroundItsAnchor() {
        const item = hoveredEntry("orbit")
        let sawWide = false
        let sawTall = false
        for (let sample = 0; sample < 60; ++sample) {
            const x = Math.abs(item.iconMotion.x)
            const y = Math.abs(item.iconMotion.y)
            if (x > 10) sawWide = true
            if (y > 10) sawTall = true
            // A circle of radius 0.22 x 60 px never exceeds that radius.
            verify2(Math.hypot(item.iconMotion.x, item.iconMotion.y) <= 13.3,
                    "orbit left its circle")
            wait(16)
        }
        verify2(sawWide && sawTall, "orbit did not travel in both axes")
    }

    // --- concealment ------------------------------------------------------

    // An icon nobody can see should not be animating.
    function test_effectsPauseWhileTheSceneIsConcealed() {
        const presets = ["spiral", "orbit", "jump", "slow-y-turn"]
        for (let index = 0; index < presets.length; ++index) {
            const item = hoveredEntry(presets[index])
            tryVerify(function() {
                return item.motionController.activeTracks.length > 0
            }, 3000, presets[index] + " never started")

            item.motionController.sceneVisible = false
            wait(0)
            compare(item.motionController.activeTracks.length, 0)
            compare(item.iconMotion.x, 0)
            compare(item.iconMotion.y, 0)
            compare(item.iconMotion.scale, 1)
            compare(item.glyphMotion.rotateY, 0)

            // And it resumes when the panel comes back.
            item.motionController.sceneVisible = true
            wait(0)
            verify2(item.motionController.activeTracks.length > 0,
                    presets[index] + " did not resume")
        }
    }

    // --- launch truth -----------------------------------------------------

    // The decisive TASK-0031 criterion: clicking an icon is not a successful
    // launch, and neither is an activation the platform could not confirm.
    function test_clickAloneNeverRunsASuccessAnimation() {
        let captured = null
        const item = createHostedEntry({
            motion: "jump",
            motionTrigger: "launch-succeeded",
            animationProfiles: [profileFor("jump")],
            animationCatalog: catalogMap(),
            invoke: function(method, appId, onOutcome) { captured = onOutcome }
        })
        const events = []
        item.motionController.eventDispatched.connect(
            function(name) { events.push(name) })

        mouseMove(item, 30, 30)
        mouseClick(item, 30, 30)
        wait(0)
        verify2(events.indexOf("click") >= 0, "the click was not dispatched")
        verify2(events.indexOf("launch-requested") >= 0,
                "the launch request was not dispatched")
        compare(events.indexOf("launch-succeeded"), -1)
        compare(item.motionController.activeTracks.length, 0)

        // An activation the compositor never confirms stays unreported.
        verify2(captured !== null, "no outcome callback was passed")
        captured({ outcome: "requested",
                   reason: "activation-not-verifiable" })
        wait(0)
        compare(events.indexOf("launch-succeeded"), -1)
        compare(events.indexOf("launch-failed"), -1)
        compare(item.motionController.activeTracks.length, 0)

        // A verified start, and only that, runs the success animation.
        captured({ outcome: "succeeded", reason: "" })
        wait(0)
        verify2(events.indexOf("launch-succeeded") >= 0,
                "a verified launch ran no animation")
        verify2(item.motionController.activeTracks.length > 0,
                "the success profile did not run")
    }

    function test_aProvedFailureRunsTheFailureAnimation() {
        let captured = null
        const item = createHostedEntry({
            motion: "shake-tangent",
            motionTrigger: "launch-failed",
            animationProfiles: [profileFor("shake-tangent")],
            animationCatalog: catalogMap(),
            invoke: function(method, appId, onOutcome) { captured = onOutcome }
        })
        const events = []
        item.motionController.eventDispatched.connect(
            function(name) { events.push(name) })

        mouseMove(item, 30, 30)
        mouseClick(item, 30, 30)
        wait(0)
        compare(item.motionController.activeTracks.length, 0)
        verify2(captured !== null, "no outcome callback was passed")

        captured({ outcome: "failed", reason: "launch-failed" })
        wait(0)
        verify2(events.indexOf("launch-failed") >= 0,
                "a proved failure ran no animation")
        compare(events.indexOf("launch-succeeded"), -1)
        verify2(item.motionController.activeTracks.length > 0,
                "the failure profile did not run")
    }

    // --- transition safety ------------------------------------------------

    // Swapping presets or catalogs mid-flight must never leave a half-applied
    // transform or a stale binding behind.
    function test_changingProfileMidAnimationLeavesAValidRestingState() {
        const item = hoveredEntry("spiral")
        tryVerify(function() {
            return Math.hypot(item.iconMotion.x, item.iconMotion.y) > 0.5
        }, 3000, "spiral never started")

        item.animationProfiles = [profileFor("pulse")]
        wait(0)
        // The spiral's channels are gone, not frozen at their last frame.
        compare(item.iconMotion.x, 0)
        compare(item.iconMotion.y, 0)
        compare(item.iconMotion.opacity, 1)
        compare(item.motionController.conflicts.length, 0)
        verify2(item.motionController.activeTracks.length > 0,
                "the replacement profile did not start")

        // A catalog swap mid-flight is equally safe.
        item.animationCatalog = catalogMap()
        wait(0)
        compare(item.motionController.conflicts.length, 0)

        item.animationProfiles = [profileFor("none")]
        wait(0)
        compare(item.motionController.activeTracks.length, 0)
        compare(item.iconMotion.scale, 1)
        compare(item.iconMotion.x, 0)
        compare(item.iconMotion.y, 0)
        compare(item.glyphMotion.rotateY, 0)
    }

    function test_reducedMotionSwitchMidAnimationRests() {
        const item = hoveredEntry("orbit")
        tryVerify(function() {
            return Math.hypot(item.iconMotion.x, item.iconMotion.y) > 0.5
        }, 3000, "orbit never started")
        item.reducedMotion = true
        wait(0)
        compare(item.motionController.activeTracks.length, 0)
        compare(item.iconMotion.x, 0)
        compare(item.iconMotion.y, 0)
        verify2(Math.max(item.iconMotion.glow, item.glyphMotion.glow) > 0,
                "reduced motion left no feedback at all")
    }

    // --- resource behaviour -----------------------------------------------

    // Repeated trigger cycles must not accumulate runners: the same profile
    // always ends up with exactly its own track count, and never more.
    function test_runnerCountStaysBoundedAcrossRepeatedTriggerCycles() {
        const item = hoveredEntry("spiral")
        const expected = (profileFor("spiral").tracks || []).length
        verify(expected > 0)
        for (let cycle = 0; cycle < 12; ++cycle) {
            item.motionController.hovered = false
            wait(0)
            compare(item.motionController.activeTracks.length, 0)
            item.motionController.hovered = true
            wait(0)
            compare(item.motionController.activeTracks.length, expected)
        }
        compare(item.motionController.conflicts.length, 0)
    }

    // Concealment cycles must be just as clean, since a hidden panel is the
    // common case for a long-lived dock.
    function test_concealmentCyclesLeaveNothingRunning() {
        const item = hoveredEntry("orbit")
        const expected = (profileFor("orbit").tracks || []).length
        for (let cycle = 0; cycle < 12; ++cycle) {
            item.motionController.sceneVisible = false
            wait(0)
            compare(item.motionController.activeTracks.length, 0)
            item.motionController.sceneVisible = true
            wait(0)
            compare(item.motionController.activeTracks.length, expected)
        }
        compare(item.motionController.conflicts.length, 0)
    }

    // --- reduced motion ---------------------------------------------------

    // Reduced motion must still confirm the state, so every requested motion
    // falls back to static glow rather than to nothing at all.
    function test_requestedMotionsSubstituteStaticGlowUnderReducedMotion() {
        const presets = ["slow-y-turn", "jump", "shake-tangent", "enlarge",
                         "spiral"]
        for (let index = 0; index < presets.length; ++index) {
            const item = hoveredEntry(presets[index], { reducedMotion: true })
            compare(item.motionController.activeTracks.length, 0)
            verify2(item.motionController.holds.length === 1,
                    presets[index] + " declares no reduced-motion feedback")
            compare(item.iconMotion.x, 0)
            compare(item.iconMotion.y, 0)
            compare(item.iconMotion.rotateZ, 0)
            compare(item.iconMotion.scale, 1)
            compare(item.tileMotion.scale, 1)
            compare(item.glyphMotion.rotateY, 0)
            // The feedback actually reaches the renderer.
            verify2(Math.max(item.iconMotion.glow, item.glyphMotion.glow) > 0,
                    presets[index] + " reduced-motion glow never rendered")
        }
    }
}
