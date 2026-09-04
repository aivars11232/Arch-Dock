.pragma library

// Resolves composed animation channels into concrete transform values.
//
// IconMotionController decides which tracks run and publishes one value per
// target/property. This library turns those neutral values into the transform
// an entry actually applies, using the entry's own geometry: which way is
// "out", which way is "along the panel", and how far an effect may travel
// before it leaves the declared effect bounds.
//
// It is deliberately free of QML types so the mapping can be tested without a
// window, and it is the only place that mapping exists: the live host and the
// shared scene both read it, so a motion cannot mean one thing in the applet
// and another in a preview.

// Properties expressed in logical units, where 1.0 is the target's own size.
var TRANSLATION_PROPERTIES = [
    "translate-x", "translate-y", "translate-normal", "translate-tangent",
    "path-radius"
];

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
}

function number(value, fallback) {
    var parsed = Number(value);
    return isFinite(parsed) ? parsed : fallback;
}

// The value a property rests at when no track claims it. Mirrors
// AnimationProfileRuntime.restingValue so both agree on "no motion".
function restingValue(property) {
    switch (String(property)) {
    case "scale":
    case "scale-x":
    case "scale-y":
    case "opacity":
        return 1;
    default:
        return 0;
    }
}

function channelValue(channels, target, property) {
    var key = String(target) + "/" + String(property);
    var values = channels || {};
    if (values.hasOwnProperty(key))
        return number(values[key], restingValue(property));
    return restingValue(property);
}

function hasChannel(channels, target, property) {
    return Boolean(channels)
        && channels.hasOwnProperty(String(target) + "/" + String(property));
}

// A resting entry: identity transform, nothing claimed.
function restingMotion() {
    return {
        x: 0,
        y: 0,
        scale: 1,
        scaleX: 1,
        scaleY: 1,
        rotateZ: 0,
        rotateY: 0,
        opacity: 1,
        glow: 0,
        highlight: 0,
        clamped: false,
        active: false
    };
}

function defaultContext() {
    return {
        size: 0,
        normal: { x: 0, y: -1 },
        tangentAngle: 0,
        allowance: null
    };
}

function mergeContext(overrides) {
    var context = defaultContext();
    var source = overrides || {};
    for (var key in source) {
        if (source.hasOwnProperty(key) && source[key] !== undefined
                && source[key] !== null)
            context[key] = source[key];
    }
    if (!context.normal)
        context.normal = { x: 0, y: -1 };
    return context;
}

// Clamps a displacement to the space the theme actually reserved. `allowance`
// is the per-side room from PanelScene; a null allowance means unbounded,
// which is what a host without declared effect bounds gets.
function clampOffset(offset, allowance) {
    var room = allowance || null;
    if (!room)
        return { x: offset.x, y: offset.y, clamped: false };
    var minimumX = -Math.max(0, number(room.left, 0));
    var maximumX = Math.max(0, number(room.right, 0));
    var minimumY = -Math.max(0, number(room.top, 0));
    var maximumY = Math.max(0, number(room.bottom, 0));
    var x = clamp(offset.x, minimumX, maximumX);
    var y = clamp(offset.y, minimumY, maximumY);
    return { x: x, y: y, clamped: x !== offset.x || y !== offset.y };
}

// The transform one target should apply for the current channel values.
//
// Translation is composed from four independent contributions, all in logical
// units: the two axis translations, a displacement along the panel's outward
// normal (jump), a displacement along its tangent (shake), and a polar offset
// from the orbit/spiral angle and its companion radius. They sum, then the sum
// is clamped once, so no single contribution can be silently dropped.
function motionFor(channels, target, context) {
    var resolved = mergeContext(context);
    var size = Math.max(0, number(resolved.size, 0));
    var motion = restingMotion();

    var normalOffset = channelValue(channels, target, "translate-normal") * size;
    var tangentOffset = channelValue(channels, target, "translate-tangent") * size;
    var tangentRadians = number(resolved.tangentAngle, 0) * Math.PI / 180;
    var polarRadius = channelValue(channels, target, "path-radius") * size;
    var polarDegrees = channelValue(channels, target, "orbit")
        + channelValue(channels, target, "spiral");
    var polarRadians = polarDegrees * Math.PI / 180;

    var offset = {
        x: channelValue(channels, target, "translate-x") * size
            + number(resolved.normal.x, 0) * normalOffset
            + Math.cos(tangentRadians) * tangentOffset
            + Math.cos(polarRadians) * polarRadius,
        y: channelValue(channels, target, "translate-y") * size
            + number(resolved.normal.y, -1) * normalOffset
            + Math.sin(tangentRadians) * tangentOffset
            + Math.sin(polarRadians) * polarRadius
    };

    var bounded = clampOffset(offset, resolved.allowance);
    motion.x = bounded.x;
    motion.y = bounded.y;
    motion.clamped = bounded.clamped;
    motion.scale = channelValue(channels, target, "scale");
    motion.scaleX = channelValue(channels, target, "scale-x");
    motion.scaleY = channelValue(channels, target, "scale-y");
    motion.rotateZ = channelValue(channels, target, "rotate-z");
    motion.rotateY = channelValue(channels, target, "rotate-y");
    motion.opacity = channelValue(channels, target, "opacity");
    motion.glow = channelValue(channels, target, "glow");
    // A flat card turning away from the viewer catches the light on its
    // leading edge. The highlight follows the turn rather than being animated
    // separately, so it can never drift out of step with the rotation.
    motion.highlight = Math.sin(motion.rotateY * Math.PI / 180);
    motion.active = motion.x !== 0 || motion.y !== 0
        || motion.scale !== 1 || motion.scaleX !== 1 || motion.scaleY !== 1
        || motion.rotateZ !== 0 || motion.rotateY !== 0
        || motion.opacity !== 1 || motion.glow !== 0;
    return motion;
}

// Horizontal compression of a flat card turned around its vertical axis. This
// is the 2D/skinned reading of a Y-axis turn: the card narrows towards its
// centre as it turns, and never inverts.
function turnCompression(rotateY) {
    return Math.max(0.02, Math.abs(Math.cos(number(rotateY, 0) * Math.PI / 180)));
}

// Neighbour influence: how strongly an entry follows the hovered one, as a
// function of how many positions away it sits.
//
// This is visual influence only. It scales what is drawn and never the entry's
// logical size, position or hit area, so magnifying an icon cannot move the
// layout underneath it. Physical rearrangement is a separate capability and is
// deliberately not wired to this.
//
// `linear` with a radius of 2.4 is the historical curve and stays the default.
function magnificationInfluence(indexDistance, radius, falloff) {
    var distance = Math.abs(number(indexDistance, 0));
    var reach = Math.max(0.0001, number(radius, 2.4));
    if (distance >= reach)
        return 0;
    var progress = distance / reach;
    switch (String(falloff || "linear")) {
    case "cosine":
        return 0.5 * (1 + Math.cos(Math.PI * progress));
    case "gaussian":
        // Bounded on purpose: a gaussian never reaches zero, so it is cut off
        // at the declared reach rather than influencing the whole panel.
        var sigma = 0.5;
        return Math.exp(-(progress * progress) / (2 * sigma * sigma));
    default:
        return 1 - progress;
    }
}

// The scale a neighbour draws at for a given influence. Resting is exactly 1,
// and the hovered entry itself reaches the configured magnification.
function magnificationScale(magnification, influence) {
    var peak = Math.max(1, number(magnification, 1));
    return 1 + (peak - 1) * clamp(number(influence, 0), 0, 1);
}

// Flat-card perspective around the vertical axis: translate the centre to the
// origin, turn about Y, divide by the viewing distance, translate back. This is
// the 2D/skinned reading of a Y-axis turn - the card narrows and one edge
// recedes. No mesh is involved; AD-0015 owns true 3D.
function turnMatrix(angle, itemWidth, itemHeight, viewDistance) {
    var radians = number(angle, 0) * Math.PI / 180;
    var cosine = Math.cos(radians);
    var sine = Math.sin(radians);
    var distance = Math.max(1, number(viewDistance, 0)
                            || Math.max(1, number(itemWidth, 1)) * 2.4);
    var centerX = number(itemWidth, 0) / 2;
    var centerY = number(itemHeight, 0) / 2;
    var toOrigin = Qt.matrix4x4(1, 0, 0, -centerX,
                                0, 1, 0, -centerY,
                                0, 0, 1, 0,
                                0, 0, 0, 1);
    var turn = Qt.matrix4x4(cosine, 0, sine, 0,
                            0, 1, 0, 0,
                            -sine, 0, cosine, 0,
                            0, 0, 0, 1);
    var perspective = Qt.matrix4x4(1, 0, 0, 0,
                                   0, 1, 0, 0,
                                   0, 0, 1, 0,
                                   0, 0, -1 / distance, 1);
    var fromOrigin = Qt.matrix4x4(1, 0, 0, centerX,
                                  0, 1, 0, centerY,
                                  0, 0, 1, 0,
                                  0, 0, 0, 1);
    return fromOrigin.times(perspective).times(turn).times(toOrigin);
}

// The largest displacement, in logical units, the given profiles can reach at
// the given intensity. Hosts use it to reserve headroom so an effect is not
// clipped by the panel it lives in.
function maximumDisplacement(profiles, intensity) {
    var list = profiles || [];
    var scale = Math.max(0, number(intensity, 1));
    var largest = 0;
    for (var index = 0; index < list.length; ++index) {
        var profile = list[index];
        if (!profile || profile.valid === false)
            continue;
        var tracks = profile.tracks || [];
        for (var trackIndex = 0; trackIndex < tracks.length; ++trackIndex) {
            var track = tracks[trackIndex];
            if (!track || TRANSLATION_PROPERTIES.indexOf(
                    String(track.property)) < 0)
                continue;
            var trackScale = clamp(number(track.intensityScale, 1), 0, 10);
            var reach = Math.max(Math.abs(number(track.from, 0)),
                                 Math.abs(number(track.to, 0)));
            largest = Math.max(largest, reach * scale * trackScale);
        }
    }
    return largest;
}
