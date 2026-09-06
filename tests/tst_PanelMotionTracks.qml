import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// TASK-0032 Phase C: the opening and closing tracks.
//
// PanelPresentationController decides what state the panel is in and is proved
// by tst_PanelPresentation. This proves the geometry that state turns into:
// that every declared mechanism produces a real track, that a track lands on
// exactly the resting geometry at both ends, that reduced motion arrives
// without travelling, and - the point of the phase - that a mechanism the 2D
// renderer cannot honestly perform reports what it needs instead of faking it.
TestCase {
    id: testCase

    name: "PanelMotionTracks"

    readonly property real surfaceWidth: 400
    readonly property real surfaceHeight: 80
    readonly property real handleExtent: 40

    Component {
        id: controllerComponent

        PanelMotionController {
            surfaceWidth: testCase.surfaceWidth
            surfaceHeight: testCase.surfaceHeight
            handleExtent: testCase.handleExtent
            contentBounds: ({ x: 20, y: 8, width: 360, height: 64 })
        }
    }

    function makeController(properties) {
        const controller = createTemporaryObject(
            controllerComponent, testCase, properties || ({}))
        verify(controller, "controller created")
        return controller
    }

    function fuzzy(actual, expected, message) {
        verify(Math.abs(Number(actual) - Number(expected)) <= 0.0001,
               message + ": actual=" + actual + " expected=" + expected)
    }

    function verifyIdentity(track, message) {
        fuzzy(track.offsetX, 0, message + " offsetX")
        fuzzy(track.offsetY, 0, message + " offsetY")
        fuzzy(track.scaleX, 1, message + " scaleX")
        fuzzy(track.scaleY, 1, message + " scaleY")
        fuzzy(track.opacity, 1, message + " opacity")
        fuzzy(track.clip.x, 0, message + " clip x")
        fuzzy(track.clip.y, 0, message + " clip y")
        fuzzy(track.clip.width, testCase.surfaceWidth, message + " clip width")
        fuzzy(track.clip.height, testCase.surfaceHeight, message + " clip height")
    }

    readonly property var roles: [
        "surface", "split-start", "split-center", "split-end", "glow", "overlay"
    ]

    function test_everyMechanismResolvesToADeclaredTrackForm_data() {
        return [
            { tag: "open", mechanism: "open", axis: "horizontal",
              form: "identity" },
            { tag: "collapse-horizontal", mechanism: "collapse-horizontal",
              axis: "horizontal", form: "center-slide" },
            { tag: "collapse-vertical", mechanism: "collapse-vertical",
              axis: "vertical", form: "thickness-reveal" },
            { tag: "split-h", mechanism: "split", axis: "horizontal",
              form: "split-horizontal" },
            { tag: "split-v", mechanism: "split", axis: "vertical",
              form: "split-vertical" },
            { tag: "shutter-h", mechanism: "shutter", axis: "horizontal",
              form: "shutter-horizontal" },
            { tag: "shutter-v", mechanism: "shutter", axis: "vertical",
              form: "lid" },
            { tag: "radial", mechanism: "collapse-radial", axis: "horizontal",
              form: "radial-interface" }
        ]
    }

    function test_everyMechanismResolvesToADeclaredTrackForm(data) {
        const controller = makeController({
            mechanism: data.mechanism,
            axis: data.axis
        })
        compare(controller.trackForm, data.form)
    }

    // An axis carried in the mechanism name wins over a stored axis that
    // disagrees with it, so a stale value cannot produce a track nobody chose.
    function test_anImpliedAxisOverridesAStoredOne_data() {
        return [
            { tag: "horizontal-implied", mechanism: "collapse-horizontal",
              axis: "vertical", expected: "horizontal" },
            { tag: "vertical-implied", mechanism: "collapse-vertical",
              axis: "horizontal", expected: "vertical" },
            { tag: "split-honours-axis", mechanism: "split",
              axis: "vertical", expected: "vertical" },
            { tag: "unknown-axis-defaults", mechanism: "shutter",
              axis: "diagonal", expected: "horizontal" }
        ]
    }

    function test_anImpliedAxisOverridesAStoredOne(data) {
        const controller = makeController({
            mechanism: data.mechanism,
            axis: data.axis
        })
        compare(controller.normalizedAxis, data.expected)
    }

    // Criterion: open/close tracks end in exact stable states. A track that
    // lands a fraction of a pixel away from its resting geometry produces a
    // visible jump when the transition hands over to the resting state.
    function test_tracksLandOnExactRestingGeometry_data() {
        return test_everyMechanismResolvesToADeclaredTrackForm_data()
    }

    function test_tracksLandOnExactRestingGeometry(data) {
        const open = makeController({
            mechanism: data.mechanism,
            axis: data.axis,
            surfaceState: "open",
            transitionState: "idle",
            presentationProgress: -1
        })
        compare(open.collapseProgress, 0, data.tag + " open progress")
        for (const role of testCase.roles)
            verifyIdentity(open.tracks[role], data.tag + " open " + role)

        const closed = makeController({
            mechanism: data.mechanism,
            axis: data.axis,
            surfaceState: "collapsed",
            transitionState: "idle",
            presentationProgress: -1
        })
        compare(closed.collapseProgress, 1, data.tag + " closed progress")

        // Landing at the end of a closing transition must produce the same
        // frame as resting collapsed, or the handover is visible.
        const landing = makeController({
            mechanism: data.mechanism,
            axis: data.axis,
            surfaceState: "collapsed",
            transitionState: "closing",
            presentationProgress: 1
        })
        compare(JSON.stringify(landing.tracks),
                JSON.stringify(closed.tracks),
                data.tag + " closing landing matches rest")

        // And the start of an opening transition must equal resting collapsed.
        const departing = makeController({
            mechanism: data.mechanism,
            axis: data.axis,
            surfaceState: "open",
            transitionState: "opening",
            presentationProgress: 0
        })
        compare(JSON.stringify(departing.tracks),
                JSON.stringify(closed.tracks),
                data.tag + " opening departure matches collapsed rest")
    }

    // Closing and opening are one track read in opposite directions. If they
    // were two implementations they could drift; this pins them together.
    function test_openingIsClosingReversed_data() {
        return [
            { tag: "center-slide", mechanism: "collapse-horizontal",
              axis: "horizontal" },
            { tag: "split", mechanism: "split", axis: "horizontal" },
            { tag: "shutter", mechanism: "shutter", axis: "horizontal" },
            { tag: "lid", mechanism: "shutter", axis: "vertical" }
        ]
    }

    function test_openingIsClosingReversed(data) {
        for (const point of [0.25, 0.5, 0.75]) {
            const closing = makeController({
                mechanism: data.mechanism,
                axis: data.axis,
                surfaceState: "collapsed",
                transitionState: "closing",
                presentationProgress: point
            })
            const opening = makeController({
                mechanism: data.mechanism,
                axis: data.axis,
                surfaceState: "open",
                transitionState: "opening",
                presentationProgress: 1 - point
            })
            fuzzy(opening.collapseProgress, closing.collapseProgress,
                  data.tag + " progress at " + point)
            compare(JSON.stringify(opening.tracks),
                    JSON.stringify(closing.tracks),
                    data.tag + " reversed track at " + point)
        }
    }

    // A panel that collapses to nothing can never be hovered open again.
    function test_aCollapsedShellKeepsAHoverableHandle_data() {
        return [
            { tag: "declared-caps", handle: 40, expected: 40 },
            { tag: "no-declared-caps", handle: 0, expected: 16 }
        ]
    }

    function test_aCollapsedShellKeepsAHoverableHandle(data) {
        const controller = makeController({
            mechanism: "collapse-horizontal",
            axis: "horizontal",
            handleExtent: data.handle,
            surfaceState: "collapsed",
            transitionState: "idle"
        })
        fuzzy(controller.collapsedExtent, data.expected, data.tag)
        verify(controller.collapsedExtent > 0, data.tag + " is hoverable")
    }

    // Criterion: reduced motion changes state without travelling.
    function test_reducedMotionArrivesWithoutTravelling_data() {
        return [
            { tag: "mid-close", surfaceState: "collapsed",
              transitionState: "closing", progress: 0.5, expected: 1 },
            { tag: "mid-open", surfaceState: "open",
              transitionState: "opening", progress: 0.5, expected: 0 }
        ]
    }

    function test_reducedMotionArrivesWithoutTravelling(data) {
        const controller = makeController({
            mechanism: "collapse-horizontal",
            axis: "horizontal",
            reducedMotion: true,
            surfaceState: data.surfaceState,
            transitionState: data.transitionState,
            presentationProgress: data.progress
        })
        compare(controller.collapseProgress, data.expected)
    }

    // Criterion: a mechanism 2D cannot honestly perform is reported, not faked.
    function test_radialReportsTheTierItNeedsInsteadOfFakingAnIris() {
        const controller = makeController({
            mechanism: "collapse-radial",
            axis: "horizontal",
            surfaceState: "collapsed",
            transitionState: "idle"
        })
        compare(controller.trackForm, "radial-interface")
        compare(controller.requiresRendererTier, "baked2.5d")
        compare(controller.fallbackReason, "mechanism-requires-baked2.5d")
        verify(controller.fallbackApplied)

        // The fallback is a centred clip and a fade. No rotation, no per-part
        // depth: nothing that could be mistaken for a real iris.
        const surface = controller.tracks["surface"]
        fuzzy(surface.opacity, 0, "radial fallback fades out")
        fuzzy(surface.offsetX, 0, "radial fallback does not translate")
        fuzzy(surface.offsetY, 0, "radial fallback does not translate")
        fuzzy(surface.scaleX, 1, "radial fallback does not scale")
        fuzzy(surface.scaleY, 1, "radial fallback does not scale")
    }

    // Criterion: unsupported themes use a safe fallback rather than a
    // mechanism the resolver refused.
    function test_anUnavailableMechanismFallsBackSafely() {
        const controller = makeController({
            mechanism: "split",
            axis: "horizontal",
            availableMechanisms: ["open", "collapse-horizontal"],
            surfaceState: "collapsed",
            transitionState: "idle"
        })
        compare(controller.trackForm, "safe-fade")
        compare(controller.fallbackReason, "mechanism-unavailable")
        fuzzy(controller.tracks["surface"].opacity, 0, "safe fade")
    }

    // An empty resolution is "not resolved yet", not "nothing is allowed".
    function test_anUnresolvedCapabilityListIsPermissive() {
        const controller = makeController({
            mechanism: "split",
            axis: "horizontal",
            availableMechanisms: []
        })
        compare(controller.trackForm, "split-horizontal")
        compare(controller.fallbackReason, "")
    }

    function test_unknownMechanismsDegradeToAlwaysOpen() {
        const controller = makeController({
            mechanism: "trapdoor",
            axis: "horizontal",
            surfaceState: "collapsed",
            transitionState: "idle"
        })
        compare(controller.normalizedMechanism, "open")
        compare(controller.trackForm, "identity")
        compare(controller.resolvedMechanism, "open")
        for (const role of testCase.roles)
            verifyIdentity(controller.tracks[role], "unknown " + role)
    }

    // Split moves artwork without scaling it: a declared cap that stretched on
    // its way out would visibly deform.
    function test_splitTranslatesWithoutDeformingArtwork() {
        const controller = makeController({
            mechanism: "split",
            axis: "horizontal",
            surfaceState: "collapsed",
            transitionState: "closing",
            presentationProgress: 0.5
        })
        const start = controller.tracks["split-start"]
        const end = controller.tracks["split-end"]
        fuzzy(start.scaleX, 1, "start not scaled")
        fuzzy(end.scaleX, 1, "end not scaled")
        fuzzy(start.offsetX, -testCase.surfaceWidth / 2 * 0.5, "start travel")
        fuzzy(end.offsetX, testCase.surfaceWidth / 2 * 0.5, "end travel")
    }

    // A lid closes onto the front plate, so the strip that survives stays
    // anchored at the bottom rather than floating in the middle.
    function test_aLidClosesOntoItsFrontPlate() {
        const controller = makeController({
            mechanism: "shutter",
            axis: "vertical",
            surfaceState: "collapsed",
            transitionState: "idle"
        })
        const clip = controller.tracks["surface"].clip
        fuzzy(clip.height, controller.collapsedExtent, "lid keeps the plate")
        fuzzy(clip.y, testCase.surfaceHeight - controller.collapsedExtent,
              "lid anchored at the bottom")
        fuzzy(clip.width, testCase.surfaceWidth, "lid keeps full width")
    }

    // Entries must not stay visible outside a shell that has closed.
    function test_contentClipFollowsTheShell_data() {
        return [
            { tag: "horizontal", mechanism: "collapse-horizontal",
              axis: "horizontal", shrinks: "width" },
            { tag: "vertical", mechanism: "collapse-vertical",
              axis: "vertical", shrinks: "height" }
        ]
    }

    function test_contentClipFollowsTheShell(data) {
        const open = makeController({
            mechanism: data.mechanism,
            axis: data.axis,
            surfaceState: "open"
        })
        compare(open.contentClip.width, 360, data.tag + " open width")
        compare(open.contentClip.height, 64, data.tag + " open height")

        const closed = makeController({
            mechanism: data.mechanism,
            axis: data.axis,
            surfaceState: "collapsed"
        })
        fuzzy(closed.contentClip[data.shrinks], 0,
              data.tag + " collapsed content is hidden")
        // The clip stays centred on the content it replaced.
        verify(closed.contentClip.x >= open.contentClip.x,
               data.tag + " clip stays inside the content box")
        verify(closed.contentClip.y >= open.contentClip.y,
               data.tag + " clip stays inside the content box")
    }

    // Every output must be a real number. A NaN reaching a renderer silently
    // removes the item from the scene graph instead of failing loudly.
    function test_degenerateInputsNeverProduceNonFiniteGeometry_data() {
        return [
            { tag: "zero-surface", width: 0, height: 0, progress: 0.5 },
            { tag: "negative-surface", width: -10, height: -10, progress: 0.5 },
            { tag: "non-numeric-progress", width: 400, height: 80,
              progress: Number.NaN }
        ]
    }

    function test_degenerateInputsNeverProduceNonFiniteGeometry(data) {
        for (const mechanism of ["collapse-horizontal", "collapse-vertical",
                                 "split", "shutter", "collapse-radial"]) {
            const controller = makeController({
                mechanism: mechanism,
                axis: "horizontal",
                surfaceWidth: data.width,
                surfaceHeight: data.height,
                surfaceState: "collapsed",
                transitionState: "closing",
                presentationProgress: data.progress
            })
            verify(isFinite(controller.collapseProgress),
                   data.tag + " " + mechanism + " progress is finite")
            for (const role of testCase.roles) {
                const track = controller.tracks[role]
                for (const key of ["offsetX", "offsetY", "scaleX", "scaleY",
                                   "opacity"]) {
                    verify(isFinite(Number(track[key])),
                           data.tag + " " + mechanism + " " + role + "." + key)
                }
                for (const key of ["x", "y", "width", "height"]) {
                    verify(isFinite(Number(track.clip[key])),
                           data.tag + " " + mechanism + " " + role
                           + " clip." + key)
                }
                verify(Number(track.clip.width) >= 0,
                       data.tag + " " + mechanism + " " + role
                       + " clip width is not negative")
                verify(Number(track.clip.height) >= 0,
                       data.tag + " " + mechanism + " " + role
                       + " clip height is not negative")
            }
        }
    }

    function test_tracksAreDeterministic() {
        const first = makeController({
            mechanism: "collapse-horizontal",
            axis: "horizontal",
            surfaceState: "collapsed",
            transitionState: "closing",
            presentationProgress: 0.37
        })
        const second = makeController({
            mechanism: "collapse-horizontal",
            axis: "horizontal",
            surfaceState: "collapsed",
            transitionState: "closing",
            presentationProgress: 0.37
        })
        compare(JSON.stringify(first.tracks), JSON.stringify(second.tracks))
        compare(JSON.stringify(first.contentClip),
                JSON.stringify(second.contentClip))
    }
}
