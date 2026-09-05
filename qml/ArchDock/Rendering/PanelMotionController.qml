import QtQuick

// The panel's opening and closing tracks, expressed as pure geometry.
//
// PanelPresentationController owns *what* state the panel is in. This owns
// *how* that state is drawn: given a mechanism, an axis and a progress, it
// returns the offset, scale, clip and opacity every theme part should take.
// It renders nothing, reads no host and touches no configuration, so the live
// applet, the Studio preview and a preset card all move identically.
//
// The capability vocabulary is coarse - open, collapse-horizontal,
// collapse-vertical, collapse-radial, split, shutter - because that is what a
// theme manifest declares and what the resolver can gate. The concrete track
// is chosen by that mechanism together with the collapse axis:
//
//   mechanism            axis         track form        also known as
//   open                 any          identity          always open
//   collapse-horizontal  (implied h)  center-slide      extend-track, reversed
//   collapse-vertical    (implied v)  thickness-reveal  vertical slide, reversed
//   split                horizontal   split-horizontal  split left/right
//   split                vertical     split-vertical    split top/bottom
//   shutter              horizontal   shutter-horizontal
//   shutter              vertical     lid               front plate
//   collapse-radial      any          radial-interface  iris/fan - 2.5D and 3D only
//
// A form and its reverse are one track, not two: extend-track is center-slide
// run from collapsed to open, and a vertical slide is thickness-reveal run the
// same way. Implementing them twice would let the two directions drift apart.
//
// Radial is deliberately not implemented in 2D. It reports the tier it needs
// and falls back to the safe clip/opacity track, because an iris drawn by
// scaling a flat image is a decoration pretending to be a mechanism.
Item {
    id: root

    // The box the tracks are computed in: the panel's own visual size.
    property real surfaceWidth: 0
    property real surfaceHeight: 0

    // The area icons occupy inside that box. The content clip is derived from
    // it so entries are hidden by the same track that closes the shell.
    property var contentBounds: ({ x: 0, y: 0, width: 0, height: 0 })

    // Fixed end caps declared by the theme slice, in pixels. They are what
    // survives a full collapse, which is what the user hovers to reopen.
    property real handleExtent: 0

    // Requested presentation, straight from the panel definition.
    property string mechanism: "open"
    property string axis: ""

    // Mechanism names the resolver reported as available for this panel. An
    // empty list means "not resolved yet" and is treated as permissive; an
    // explicitly non-empty list that omits the request is a refusal.
    property var availableMechanisms: []

    // Live presentation, straight from PanelPresentationController.
    property string surfaceState: "open"
    property string transitionState: "idle"
    property real presentationProgress: -1
    property bool reducedMotion: false

    readonly property string normalizedMechanism:
        normalizeMechanism(mechanism)
    readonly property string normalizedAxis:
        normalizeAxis(normalizedMechanism, axis)
    readonly property bool mechanismAvailable:
        isMechanismAvailable(normalizedMechanism)

    // How closed the panel is: 0 fully open, 1 fully collapsed. A transition
    // reads its direction from the transition state rather than from the
    // surface state, which names the destination and would therefore report a
    // panel as closed the instant it began closing.
    readonly property real collapseProgress: computeCollapseProgress()

    readonly property string trackForm: resolveTrackForm()
    readonly property string resolvedMechanism:
        trackForm === "identity" && normalizedMechanism !== "open"
            ? "open" : normalizedMechanism
    readonly property string requiresRendererTier:
        normalizedMechanism === "collapse-radial" ? "baked25d" : ""
    readonly property string fallbackReason: resolveFallbackReason()
    readonly property bool fallbackApplied: fallbackReason.length > 0

    // Per-role tracks. Every role a theme layer can carry is present, so a
    // renderer never has to guess what an absent entry meant.
    readonly property var tracks: buildTracks()
    readonly property var contentClip: buildContentClip()

    // The extent the shell keeps when fully collapsed, along the axis it
    // collapses on. Never zero: a panel with no handle cannot be reopened.
    readonly property real collapsedExtent: computeCollapsedExtent()

    function values(value) {
        return value && value.length !== undefined ? value : []
    }

    function finite(value, fallback) {
        const number = Number(value)
        return isFinite(number) ? number : Number(fallback || 0)
    }

    function clamp01(value) {
        return Math.max(0, Math.min(1, finite(value, 0)))
    }

    function normalizeMechanism(value) {
        const name = String(value || "").trim().toLowerCase()
        return ["open", "collapse-horizontal", "collapse-vertical",
                "collapse-radial", "split", "shutter"].includes(name)
            ? name : "open"
    }

    // collapse-horizontal and collapse-vertical carry their axis in the name,
    // so a stored axis that disagrees with them is ignored rather than obeyed.
    function normalizeAxis(resolvedMechanismName, value) {
        if (resolvedMechanismName === "collapse-horizontal")
            return "horizontal"
        if (resolvedMechanismName === "collapse-vertical")
            return "vertical"
        const name = String(value || "").trim().toLowerCase()
        return name === "vertical" ? "vertical" : "horizontal"
    }

    function isMechanismAvailable(name) {
        const declared = values(availableMechanisms)
        if (declared.length === 0)
            return true
        for (let index = 0; index < declared.length; ++index) {
            if (String(declared[index] || "").trim().toLowerCase() === name)
                return true
        }
        return false
    }

    function computeCollapseProgress() {
        const phase = String(transitionState || "idle").toLowerCase()
        const raw = finite(presentationProgress, -1)
        const inFlight = (phase === "opening" || phase === "closing")
            && raw >= 0 && raw <= 1
        if (!inFlight || reducedMotion) {
            // Reduced motion still changes state; it simply arrives at an
            // endpoint instead of travelling to one.
            return String(surfaceState || "open").toLowerCase() === "collapsed"
                ? 1 : 0
        }
        return phase === "closing" ? clamp01(raw) : 1 - clamp01(raw)
    }

    function resolveTrackForm() {
        const name = normalizedMechanism
        if (name === "open")
            return "identity"
        if (!mechanismAvailable)
            return "safe-fade"
        if (name === "collapse-radial")
            return "radial-interface"
        if (name === "collapse-horizontal")
            return "center-slide"
        if (name === "collapse-vertical")
            return "thickness-reveal"
        if (name === "split")
            return normalizedAxis === "vertical"
                ? "split-vertical" : "split-horizontal"
        return normalizedAxis === "vertical" ? "lid" : "shutter-horizontal"
    }

    function resolveFallbackReason() {
        if (normalizedMechanism === "open")
            return ""
        if (!mechanismAvailable)
            return "mechanism-unavailable"
        if (normalizedMechanism === "collapse-radial")
            return "mechanism-requires-baked25d"
        if (surfaceWidth <= 0 || surfaceHeight <= 0)
            return "surface-geometry-invalid"
        return ""
    }

    function identityTrack() {
        return {
            offsetX: 0,
            offsetY: 0,
            scaleX: 1,
            scaleY: 1,
            opacity: 1,
            clip: fullClip()
        }
    }

    function fullClip() {
        return {
            x: 0,
            y: 0,
            width: Math.max(0, finite(surfaceWidth, 0)),
            height: Math.max(0, finite(surfaceHeight, 0))
        }
    }

    function trackWith(changes) {
        const track = identityTrack()
        const keys = Object.keys(changes || ({}))
        for (let index = 0; index < keys.length; ++index)
            track[keys[index]] = changes[keys[index]]
        return track
    }

    function horizontalExtent() {
        return Math.max(0, finite(surfaceWidth, 0))
    }

    function verticalExtent() {
        return Math.max(0, finite(surfaceHeight, 0))
    }

    // The travelling part of the panel: everything except the caps the user
    // must keep in order to hover the panel back open.
    function travelExtent() {
        const extent = normalizedAxis === "vertical"
            ? verticalExtent() : horizontalExtent()
        return Math.max(0, extent - computeCollapsedExtent())
    }

    function computeCollapsedExtent() {
        const extent = normalizedAxis === "vertical"
            ? verticalExtent() : horizontalExtent()
        if (extent <= 0)
            return 0
        const declared = Math.max(0, finite(handleExtent, 0))
        // A theme that declares no caps still keeps a hoverable sliver rather
        // than collapsing to nothing.
        const minimum = Math.min(extent, Math.max(2, extent * 0.04))
        return Math.min(extent, Math.max(declared, minimum))
    }

    function buildTracks() {
        const progress = collapseProgress
        const form = trackForm
        const result = {
            "surface": identityTrack(),
            "split-start": identityTrack(),
            "split-center": identityTrack(),
            "split-end": identityTrack(),
            "glow": identityTrack(),
            "overlay": identityTrack()
        }
        if (progress <= 0 || form === "identity")
            return result

        const vertical = normalizedAxis === "vertical"
        const travel = travelExtent()
        const kept = computeCollapsedExtent()
        const width = horizontalExtent()
        const height = verticalExtent()

        if (form === "center-slide" || form === "thickness-reveal") {
            // The caps converge on the middle and the centre part gives up its
            // whole travel, so the shell ends exactly as wide as its handle.
            const shift = travel / 2 * progress
            const centreScale = travel > 0
                ? Math.max(0, 1 - progress) : 1
            result["split-start"] = trackWith(
                vertical ? { offsetY: shift } : { offsetX: shift })
            result["split-end"] = trackWith(
                vertical ? { offsetY: -shift } : { offsetX: -shift })
            result["split-center"] = trackWith(
                vertical ? { scaleY: centreScale } : { scaleX: centreScale })
            const surfaceScale = travel > 0 && (vertical ? height : width) > 0
                ? Math.max(0, (kept + travel * (1 - progress))
                    / (vertical ? height : width))
                : 1
            result["surface"] = trackWith(
                vertical ? { scaleY: surfaceScale } : { scaleX: surfaceScale })
            result["glow"] = result["surface"]
            result["overlay"] = result["surface"]
            return result
        }

        if (form === "split-horizontal" || form === "split-vertical") {
            // The two halves leave the frame in opposite directions and the
            // centre fades: nothing is scaled, so declared artwork keeps its
            // exact proportions all the way out.
            const half = (vertical ? height : width) / 2
            const shift = half * progress
            result["split-start"] = trackWith(
                vertical ? { offsetY: -shift } : { offsetX: -shift })
            result["split-end"] = trackWith(
                vertical ? { offsetY: shift } : { offsetX: shift })
            result["split-center"] = trackWith(
                { opacity: Math.max(0, 1 - progress) })
            result["surface"] = trackWith(
                { opacity: Math.max(0, 1 - progress) })
            result["glow"] = result["surface"]
            result["overlay"] = result["surface"]
            return result
        }

        if (form === "shutter-horizontal") {
            // A shutter closes the visible window rather than moving artwork:
            // the parts stay put and the clip narrows onto the handle.
            const remaining = kept + travel * (1 - progress)
            const clip = {
                x: (width - remaining) / 2,
                y: 0,
                width: Math.max(0, remaining),
                height: height
            }
            result["surface"] = trackWith({ clip: clip })
            result["split-start"] = result["surface"]
            result["split-center"] = result["surface"]
            result["split-end"] = result["surface"]
            result["glow"] = result["surface"]
            result["overlay"] = result["surface"]
            return result
        }

        if (form === "lid") {
            // A lid closes from the top edge onto the front plate, so the
            // surviving strip stays anchored at the bottom of the shell.
            const remaining = kept + travel * (1 - progress)
            const clip = {
                x: 0,
                y: Math.max(0, height - remaining),
                width: width,
                height: Math.max(0, remaining)
            }
            result["surface"] = trackWith({ clip: clip })
            result["split-start"] = result["surface"]
            result["split-center"] = result["surface"]
            result["split-end"] = result["surface"]
            result["glow"] = result["surface"]
            result["overlay"] = result["surface"]
            return result
        }

        // radial-interface and safe-fade: the honest 2D answer.
        //
        // A radial mechanism needs a renderer that owns depth. Until one is
        // installed the panel still has to close, so it closes with a centred
        // clip and a fade - visibly a fallback, never mistakable for an iris.
        const remainingWidth = width - (width - kept) * progress
        const remainingHeight = height - (height - kept) * progress
        const safeClip = {
            x: Math.max(0, (width - remainingWidth) / 2),
            y: Math.max(0, (height - remainingHeight) / 2),
            width: Math.max(0, remainingWidth),
            height: Math.max(0, remainingHeight)
        }
        const safe = trackWith({
            opacity: Math.max(0, 1 - progress),
            clip: safeClip
        })
        result["surface"] = safe
        result["split-start"] = safe
        result["split-center"] = safe
        result["split-end"] = safe
        result["glow"] = safe
        result["overlay"] = safe
        return result
    }

    // Where entries may still be drawn. It follows the surface track, so an
    // icon is never left visible outside a shell that has already closed.
    function buildContentClip() {
        const source = contentBounds && typeof contentBounds === "object"
            ? contentBounds : ({})
        const base = {
            x: finite(source.x, 0),
            y: finite(source.y, 0),
            width: Math.max(0, finite(source.width, 0)),
            height: Math.max(0, finite(source.height, 0))
        }
        const progress = collapseProgress
        if (progress <= 0 || trackForm === "identity")
            return base

        const vertical = normalizedAxis === "vertical"
        if (vertical) {
            const height = Math.max(0, base.height * (1 - progress))
            return {
                x: base.x,
                y: base.y + (base.height - height) / 2,
                width: base.width,
                height: height
            }
        }
        const width = Math.max(0, base.width * (1 - progress))
        return {
            x: base.x + (base.width - width) / 2,
            y: base.y,
            width: width,
            height: base.height
        }
    }

    width: 0
    height: 0
    visible: false
}
