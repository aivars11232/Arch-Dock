import QtQuick
import QtTest
import ArchDock.Rendering 1.0
import "../qml/ArchDock/Rendering/AnimationProfileRuntime.js" as Runtime

TestCase {
    id: testCase

    name: "AnimationProfileRuntime"
    when: windowShown
    width: 200
    height: 200

    property var dispatchedEvents: []
    property var reportedConflicts: []

    Component {
        id: controllerComponent

        IconMotionController {
            onEventDispatched: event => testCase.dispatchedEvents.push(event)
            onConflictDetected: conflict =>
                testCase.reportedConflicts.push(conflict)
        }
    }

    function track(overrides) {
        const result = {
            id: "t",
            property: "translate-y",
            from: 0,
            to: -10,
            easing: "linear",
            duration: 200,
            delay: 0,
            phase: 0,
            direction: "normal",
            repeat: 1,
            intensityScale: 1,
            blend: "replace",
            priority: 0
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    function profile(overrides) {
        const result = {
            id: "p",
            name: "Profile",
            valid: true,
            target: "icon",
            trigger: "hover-hold",
            timing: { baseDuration: 200, speedScale: 1, startDelay: 0 },
            tracks: [track()],
            rendererRequirements: ["procedural2d"],
            reducedMotion: { mode: "none" },
            totalDuration: 200,
            continuous: false
        }
        const additions = overrides || ({})
        for (const key of Object.keys(additions))
            result[key] = additions[key]
        return result
    }

    function createController(properties) {
        dispatchedEvents = []
        reportedConflicts = []
        const item = createTemporaryObject(
            controllerComponent, testCase, properties || ({}))
        verify(item !== null)
        wait(0)
        return item
    }

    function keysOf(tracks) {
        return tracks.map(entry =>
            entry.profileId + ":" + entry.trackId + "@" + entry.key)
    }

    // Determinism is the whole point of a composer: identical input must give
    // an identical, order-independent track list.
    function test_sameEventSequenceProducesTheSameTracks() {
        const profiles = [
            profile({ id: "alpha", tracks: [track({ id: "one" })] }),
            profile({
                id: "beta",
                target: "glyph",
                tracks: [track({ id: "two", property: "rotate-z", to: 30 })]
            })
        ]
        const state = { hovered: true }

        const first = Runtime.compose(profiles, state, {})
        const second = Runtime.compose(profiles, state, {})
        compare(keysOf(first.tracks), keysOf(second.tracks))

        // Presenting the same profiles in the opposite order must not change
        // the result; ordering may never depend on arrival.
        const reversed = Runtime.compose(profiles.slice().reverse(), state, {})
        compare(keysOf(first.tracks), keysOf(reversed.tracks))
        compare(first.tracks.length, 2)
    }

    function test_twoProfilesCannotSilentlyOverwriteTheSameProperty() {
        const profiles = [
            profile({ id: "alpha", tracks: [track({ id: "one", to: -10 })] }),
            profile({ id: "beta", tracks: [track({ id: "two", to: 10 })] })
        ]
        const result = Runtime.compose(profiles, { hovered: true }, {})

        // Fail closed: neither writer runs, and the clash is reported.
        compare(result.tracks.length, 0)
        compare(result.conflicts.length, 1)
        compare(result.conflicts[0].key, "icon/translate-y")
        compare(result.conflicts[0].claimants.length, 2)

        // Distinct priorities resolve it deterministically: highest wins alone.
        const prioritised = [
            profile({ id: "alpha", tracks: [track({ id: "one", to: -10 })] }),
            profile({
                id: "beta",
                tracks: [track({ id: "two", to: 10, priority: 5 })]
            })
        ]
        const resolved = Runtime.compose(prioritised, { hovered: true }, {})
        compare(resolved.conflicts.length, 0)
        compare(resolved.tracks.length, 1)
        compare(resolved.tracks[0].profileId, "beta")

        // Additive writers both contribute without ambiguity.
        const additive = [
            profile({
                id: "alpha",
                tracks: [track({ id: "one", to: -10, blend: "add" })]
            }),
            profile({
                id: "beta",
                tracks: [track({ id: "two", to: -4, blend: "add" })]
            })
        ]
        const summed = Runtime.compose(additive, { hovered: true }, {})
        compare(summed.conflicts.length, 0)
        compare(summed.tracks.length, 2)
    }

    // Legacy TASK-0050: a click may never be delivered as a successful launch.
    function test_clickDoesNotMasqueradeAsSuccessfulLaunch() {
        const clickProfile = profile({
            id: "click-feedback",
            trigger: "click",
            tracks: [track({ id: "press-dip", property: "scale", from: 1, to: 0.9 })]
        })
        const successProfile = profile({
            id: "launch-celebrate",
            trigger: "launch-succeeded",
            tracks: [track({ id: "hop", to: -18 })]
        })
        const controller = createController({
            profiles: [clickProfile, successProfile]
        })

        verify(controller.dispatch("click"))
        wait(0)
        compare(controller.activeProfileIds, ["click-feedback"])
        verify(!controller.isPulseLive("launch-succeeded"))

        // The legacy umbrella value resolves to a request, never to a success.
        compare(Runtime.normalizeTrigger("launch"), "launch-requested")
        verify(Runtime.normalizeTrigger("launch") !== "launch-succeeded")

        // Only a verified outcome activates the success profile.
        verify(controller.dispatch("launch-succeeded"))
        wait(0)
        verify(controller.activeProfileIds.indexOf("launch-celebrate") >= 0)
    }

    function test_stateAndPulseTriggersAreDistinct() {
        verify(Runtime.isStateEvent("idle"))
        verify(Runtime.isStateEvent("hover-hold"))
        verify(Runtime.isPulseEvent("click"))
        verify(Runtime.isPulseEvent("launch-failed"))
        verify(!Runtime.isEvent("launch"))

        const controller = createController({
            profiles: [profile({ id: "hover", trigger: "hover-hold" })]
        })
        // A state trigger cannot be faked through the pulse entry point.
        verify(!controller.dispatch("hover-hold"))
        compare(controller.activeTracks.length, 0)

        controller.hovered = true
        wait(0)
        compare(controller.activeTracks.length, 1)
        controller.hovered = false
        wait(0)
        compare(controller.activeTracks.length, 0)
    }

    function test_changingProfileMidTransitionTerminatesCleanly() {
        const controller = createController({
            hovered: true,
            profiles: [profile({
                id: "long",
                tracks: [track({ id: "slow", duration: 4000, repeat: -1 })]
            })]
        })
        compare(controller.activeTracks.length, 1)
        wait(60)

        // Swap the bound profile while the track is mid-flight.
        controller.profiles = [profile({
            id: "other",
            tracks: [track({ id: "fast", property: "scale", from: 1, to: 1.2 })]
        })]
        wait(0)

        // The old key must be gone, not left holding a stale offset.
        compare(controller.activeTracks.length, 1)
        compare(controller.activeTracks[0].key, "icon/scale")
        compare(controller.channelValue("icon", "translate-y"), 0)

        controller.reset()
        wait(0)
        compare(controller.channelValue("icon", "translate-y"), 0)
        compare(controller.channelValue("icon", "scale"), 1)
    }

    function test_reducedMotionResolvesToAnExplicitSubstitute() {
        const substitute = profile({
            id: "static-glow",
            tracks: [track({
                id: "glow", property: "glow", from: 0.2, to: 0.8, duration: 400
            })]
        })
        const animated = profile({
            id: "big-hop",
            reducedMotion: { mode: "substitute", substituteProfileId: "static-glow" }
        })
        const catalog = ({ "static-glow": substitute })

        const normal = Runtime.compose([animated], { hovered: true }, catalog)
        compare(normal.tracks.length, 1)
        compare(normal.tracks[0].property, "translate-y")

        const reduced = Runtime.compose(
            [animated], { hovered: true, reducedMotion: true }, catalog)
        compare(reduced.tracks.length, 1)
        compare(reduced.tracks[0].property, "glow")

        // `none` rests entirely under reduced motion.
        const resting = Runtime.compose(
            [profile({ id: "plain" })],
            { hovered: true, reducedMotion: true }, {})
        compare(resting.tracks.length, 0)

        // `static` holds one value instead of animating.
        const held = Runtime.compose(
            [profile({
                id: "held",
                reducedMotion: { mode: "static", property: "glow", value: 0.5 }
            })],
            { hovered: true, reducedMotion: true }, {})
        compare(held.tracks.length, 0)
        compare(held.holds.length, 1)
        compare(held.holds[0].value, 0.5)
    }

    function test_timingRespectsSpeedIntensityAndStagger() {
        const staggered = profile({
            id: "wave",
            tracks: [track({ id: "lift", phase: 45, to: -20 })]
        })

        const firstEntry = Runtime.compose(
            [staggered], { hovered: true, entryIndex: 0 }, {})
        const thirdEntry = Runtime.compose(
            [staggered], { hovered: true, entryIndex: 3 }, {})
        compare(firstEntry.tracks[0].delay, 0)
        compare(thirdEntry.tracks[0].delay, 135)

        // Speed divides duration; intensity scales the travelled distance.
        const fast = Runtime.compose(
            [staggered], { hovered: true, speed: 2 }, {})
        compare(fast.tracks[0].duration, 100)

        const strong = Runtime.compose(
            [staggered], { hovered: true, intensity: 1.5 }, {})
        compare(strong.tracks[0].from, 0)
        compare(strong.tracks[0].to, -30)

        // A symmetric oscillation narrows towards its centre rather than
        // drifting off it, matching the pre-migration -A*i .. +A*i arithmetic.
        const tilt = profile({
            id: "swing",
            tracks: [track({
                id: "t", property: "rotate-z", from: -14, to: 14,
                direction: "alternate", repeat: -1
            })]
        })
        const fullTilt = Runtime.compose([tilt], { hovered: true }, {})
        compare(fullTilt.tracks[0].from, -14)
        compare(fullTilt.tracks[0].to, 14)

        const halfTilt = Runtime.compose(
            [tilt], { hovered: true, intensity: 0.5 }, {})
        compare(halfTilt.tracks[0].from, -7)
        compare(halfTilt.tracks[0].to, 7)

        // Scale narrows towards its resting value of 1, not towards zero.
        const halfSwell = Runtime.compose(
            [profile({
                id: "swell",
                tracks: [track({
                    id: "s", property: "scale", from: 1, to: 1.16
                })]
            })],
            { hovered: true, intensity: 0.5 }, {})
        compare(halfSwell.tracks[0].from, 1)
        compare(halfSwell.tracks[0].to, 1.08)

        // The origin is never rescaled, only the distance travelled from it.
        const scaleTrack = Runtime.compose(
            [profile({
                id: "grow",
                tracks: [track({
                    id: "s", property: "scale", from: 1, to: 1.2
                })]
            })],
            { hovered: true, intensity: 2 }, {})
        compare(scaleTrack.tracks[0].from, 1)
        compare(scaleTrack.tracks[0].to, 1.4)
    }

    function test_controllerRunsTracksAndPublishesComposedChannels() {
        const controller = createController({
            hovered: true,
            profiles: [profile({
                id: "lift",
                tracks: [track({
                    id: "up", to: -20, duration: 120, easing: "linear"
                })]
            })]
        })
        compare(controller.channelValue("icon", "translate-y"), 0)

        // The published channel must actually move as the animation advances.
        tryVerify(function() {
            return controller.channelValue("icon", "translate-y") < -1
        }, 2000)

        // An unclaimed property reports its resting value, not undefined.
        compare(controller.channelValue("icon", "scale"), 1)
        compare(controller.channelValue("glyph", "rotate-z"), 0)
        verify(!controller.hasChannel("glyph", "rotate-z"))
    }

    function test_additiveChannelsSumOntoTheRestingValue() {
        const controller = createController({
            hovered: true,
            profiles: [
                profile({
                    id: "alpha",
                    tracks: [track({
                        id: "a", property: "scale", from: 1, to: 1.5,
                        duration: 40, blend: "add"
                    })]
                }),
                profile({
                    id: "beta",
                    tracks: [track({
                        id: "b", property: "scale", from: 1, to: 1.25,
                        duration: 40, blend: "add"
                    })]
                })
            ]
        })
        compare(controller.conflicts.length, 0)
        compare(controller.activeTracks.length, 2)

        // Both displacements land on the resting scale of 1: 1 + 0.5 + 0.25.
        tryVerify(function() {
            return Math.abs(controller.channelValue("icon", "scale") - 1.75)
                < 0.01
        }, 2000)
    }

    function test_conflictsAreReportedToTheHost() {
        const controller = createController({
            hovered: true,
            profiles: [
                profile({ id: "alpha", tracks: [track({ id: "one" })] }),
                profile({ id: "beta", tracks: [track({ id: "two", to: 8 })] })
            ]
        })
        verify(controller.hasConflict)
        compare(controller.activeTracks.length, 0)
        compare(reportedConflicts.length, 1)
        compare(reportedConflicts[0].property, "translate-y")
    }

    function test_invisibleSceneRunsNoTracks() {
        const controller = createController({
            hovered: true,
            profiles: [profile({ id: "idle-spin", trigger: "idle" })]
        })
        compare(controller.activeTracks.length, 1)

        controller.sceneVisible = false
        wait(0)
        compare(controller.activeTracks.length, 0)
        compare(controller.channelValue("icon", "translate-y"), 0)
    }
}
