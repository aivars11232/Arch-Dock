.pragma library

// The shared layout engine: pure functions that turn a panel's layout and
// settings into geometry. It places entries along straight, curved, polygon
// and baked tracks, computes the surface path and the procedural looks'
// colours, and lays out folder contents (outward popups and the track along
// the dock). Every renderer (and so Panel Studio's previews) and the dock
// applet use it, so they agree on where everything stands.

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
}

// Every numeric input is coerced through here. A NaN, Infinity, string or
// undefined from a half-loaded configuration must never reach the trigonometry
// below, because one non-finite coordinate poisons the whole scene.
function finite(value, fallback) {
    const number = Number(value);
    return isFinite(number) ? number : fallback;
}

function finiteAtLeast(value, minimum, fallback) {
    const number = finite(value, fallback);
    return number >= minimum ? number : fallback;
}

// Open-path layouts share one sweep table so an entry and the surface drawn
// under it are computed from the same numbers. Degrees; `start` is where the
// path begins in the surface's angular frame.
var pathSweeps = {
    "arc": { sweep: 130, start: 205 },
    "semicircle": { sweep: 180, start: 180 },
    // The fan keeps its historical frame (-148 to -32 degrees), which is the
    // same direction as 212 to 328 but preserves the numeric tangent and
    // normal values existing consumers were built against.
    "fan": { sweep: 116, start: -148 },
    "radial": { sweep: 300, start: -150 }
};

function pathSweep(layout) {
    return pathSweeps[layout] || null;
}

// The schema default of the canonical `spacing` control, in pixels at layout
// scale 1. A curved track reads the control relative to it: see below.
var canonicalSpacing = 8;

// Where entry `index` of `count` sits along a curved track `length` pixels
// long, as a fraction of the track: 0 at its start, 1 at its end (a closed
// track ends where it starts).
//
// A track spreads its entries evenly, as every curved layout always has, and
// that is still what the canonical spacing means at its default or above. A
// smaller spacing closes the entries up in proportion about the middle of the
// track - the front of a ring or platform, the apex of an arc - down to
// touching at zero, and never closer. `spacingReference` is the default at the
// panel's layout scale; a caller that passes none keeps the even spread.
//
// An open track that cannot hold its entries side by side keeps icon + spacing
// between neighbours instead and shows `capacity` of them, centred on the
// track.
//
// `travel` moves every entry along the track by that many entry slots
// (ADREP-TASK-002). The wheel, a drag or continuous motion sets it; it is
// transient browsing state, never saved. Positive travel moves the entries
// towards the end of the track, which is clockwise on every layout drawn
// here. A closed track carries its entries round: one slot puts each entry
// where its neighbour stood. An open track is a loop longer than the path:
// its entries rest in `capacity` slots on the path and the loop has at least
// one more off it, so an entry that leaves one end fades out a little past
// it and comes back at the other end, whether or not every entry fits.
// Only an entry wholly on the path is `onTrack`, which is what may be drawn
// solid and pressed; `visibility` fades one that is leaving or arriving.
function trackPlacement(index, count, length, closed, iconSize, spacing,
                        spacingReference, travel) {
    const safeCount = Math.max(1, Math.round(finite(count, 0)));
    const safeIndex = clamp(Math.round(finite(index, 0)), 0, safeCount - 1);
    const size = Math.max(1, finite(iconSize, 1));
    const gap = Math.max(0, finite(spacing, 0));
    const reference = Math.max(0, finite(spacingReference, 0));
    const trackLength = Math.max(0, finite(length, 0));
    const phase = finite(travel, 0);
    const spans = closed ? safeCount : safeCount - 1;
    const result = {
        progress: spans > 0 ? safeIndex / spans : 0.5,
        slot: safeIndex,
        onTrack: true,
        visibility: 1,
        pitch: spans > 0 ? trackLength / spans : 0,
        windowed: false,
        capacity: safeCount,
        loop: safeCount,
        startProgress: 0,
        endProgress: 1
    };
    if (spans <= 0 || trackLength <= 0)
        return result;
    if (!closed && result.pitch < size) {
        result.pitch = size + gap;
        result.capacity = Math.max(1, Math.min(
            safeCount - 1, Math.floor(trackLength / result.pitch) + 1));
        result.windowed = true;
    } else {
        const evenGap = result.pitch - size;
        // Never tighter than the spacing itself, never wider than the even
        // spread; closed up about the middle of the track.
        if (reference > 0 && gap < reference && evenGap > 0)
            result.pitch = size + Math.min(
                evenGap, Math.max(gap, evenGap * gap / reference));
    }
    const pitch = result.pitch;
    const capacity = result.capacity;
    const margin = (trackLength - (closed ? safeCount : capacity - 1) * pitch) / 2;
    if (closed) {
        const slot = wrapped(safeIndex + phase, safeCount);
        result.slot = slot;
        result.progress = wrapped(margin + slot * pitch, trackLength) / trackLength;
        return result;
    }
    // How far past an end a leaving entry goes before it has faded: one
    // slot, or one icon where the slots are wider than an icon.
    const reach = Math.min(1, size / pitch);
    const loop = Math.max(safeCount, capacity + 1);
    const position = wrapped(safeIndex + phase, loop);
    let slot = position;
    let visibility = 1;
    if (position > capacity - 1) {
        if (position < capacity) {
            const share = position - (capacity - 1);
            slot = capacity - 1 + share * reach;
            visibility = 1 - share;
        } else if (position > loop - 1) {
            const share = loop - position;
            slot = -share * reach;
            visibility = 1 - share;
        } else {
            // Waiting off the path for its turn to come back: not drawn, and
            // kept at the end slot so it never stands outside the panel.
            slot = capacity - 1;
            visibility = 0;
        }
    }
    result.loop = loop;
    result.slot = slot;
    result.visibility = visibility;
    result.onTrack = position <= capacity - 1 + 1e-9;
    result.progress = (margin + slot * pitch) / trackLength;
    result.startProgress = (margin - reach * pitch) / trackLength;
    result.endProgress = (margin + (capacity - 1 + reach) * pitch) / trackLength;
    return result;
}

// `value` brought into [0, period).
function wrapped(value, period) {
    const result = value % period;
    return result < 0 ? result + period : result;
}

// The track a layout's entries travel along (see trackPlacement), or null
// for a layout whose entries stand in rows. The star and the spiral keep
// their own even spread, which the canonical spacing does not regulate.
function travelTrack(resolvedLayout, geometry, polygonSides, count) {
    const track = curvedTrack(resolvedLayout, geometry, polygonSides);
    if (track) {
        return { closed: track.closed, length: track.length,
                 spacing: geometry.spacing,
                 spacingReference: geometry.spacingReference };
    }
    if (resolvedLayout === "star")
        return { closed: true, length: 1, spacing: 0, spacingReference: 0 };
    if (resolvedLayout === "spiral") {
        const entries = Math.max(1, Math.round(finite(count, 0)));
        return { closed: false,
                 length: Math.max(1, entries - 1) * geometry.iconSize,
                 spacing: 0, spacingReference: 0 };
    }
    return null;
}

// Whether a layout's entries travel, whether an open path shows only some of
// them, how many it holds and how many slots its loop has: the period after
// which travel brings every entry back to where it started.
function pathWindow(layout, count, rawGeometry) {
    const geometry = safeGeometry(rawGeometry, layout);
    const entries = Math.max(0, Math.round(finite(count, 0)));
    const track = travelTrack(geometry.layout, geometry, geometry.sides, entries);
    const placement = track
        ? trackPlacement(0, entries, track.length, track.closed,
                         geometry.iconSize, track.spacing,
                         track.spacingReference, 0)
        : null;
    return {
        travels: placement !== null && entries > 1,
        closed: track ? track.closed : false,
        windowed: placement ? placement.windowed : false,
        capacity: placement ? placement.capacity : entries,
        loop: placement ? placement.loop : entries
    };
}

// The track a curved layout lays its entries along: its length at the
// layout's radius and whether it closes on itself. Null for a layout that is
// not one track; the star and the spiral keep their own paths.
function curvedTrack(resolvedLayout, geometry, polygonSides) {
    const radius = geometry.trackRadius > 0 ? geometry.trackRadius : geometry.radius;
    if (["circular", "ring", "ellipse"].includes(resolvedLayout))
        return { closed: true, length: 2 * Math.PI * radius };
    if (resolvedLayout === "mobius")
        return { closed: true, length: mobiusLength() * radius * mobiusReach };
    if (["polygon", "triangle", "square", "pentagon", "hexagon",
         "octagon"].includes(resolvedLayout)) {
        const sides = shapeSides(resolvedLayout, polygonSides);
        return { closed: true,
                 length: sides * 2 * radius * Math.sin(Math.PI / sides) };
    }
    const path = pathSweep(resolvedLayout);
    return path
        ? { closed: false, length: path.sweep * Math.PI / 180 * radius } : null;
}

// Whole-scene rotation is offered only where a turning scene is meaningful:
// the radial layouts. A straight row has no centre to turn about.
// Layouts whose entries stand on a curved track: closed shapes, the star
// among them, and the open paths of the sweep table. A folder on one can
// open along the dock, on the curve round the dock through the folder.
function curvedLayout(layout) {
    const name = String(layout || "");
    return ["circular", "ring", "ellipse", "polygon", "triangle", "square",
            "pentagon", "hexagon", "octagon", "star"].includes(name)
        || pathSweep(name) !== null;
}

function supportsWholeSceneRotation(layout) {
    return radialLayoutNames.includes(String(layout || ""));
}

// The square box a radial scene fits inside at every angle. Entries sit at
// most `radius + iconSize / 2` from the centre, so this is the diameter of
// that circle plus the padding on both sides; the scene keeps this size while
// it turns instead of resizing its host every frame.
function rotationEnvelope(geometry) {
    const source = safeGeometry(geometry, geometry ? geometry.layout : "");
    const reach = source.radius * (source.layout === "mobius" ? mobiusReach : 1);
    const side = Math.max(1, Math.ceil(
        2 * (reach + source.iconSize / 2 + source.padding)));
    const result = {};
    for (const key of Object.keys(source))
        result[key] = source[key];
    result.width = side;
    result.height = side;
    result.rotationEnvelope = true;
    return result;
}

function normalizedLayout(layout, vertical) {
    if (layout === "adaptive")
        return vertical ? "vertical" : "horizontal";
    return layout || "circular";
}

function shapeSides(layout, fallback) {
    if (layout === "triangle") return 3;
    if (layout === "square") return 4;
    if (layout === "pentagon") return 5;
    if (layout === "hexagon") return 6;
    if (layout === "octagon") return 8;
    return clamp(Math.round(Number(fallback || 6)), 3, 12);
}

// Layouts whose geometry defines its own outward direction. Everything else is
// a straight run of icons that has no intrinsic outward side.
var radialLayoutNames = [
    "circular", "ring", "ellipse", "radial", "polygon", "triangle",
    "square", "pentagon", "hexagon", "octagon", "star", "arc",
    "semicircle", "fan", "spiral", "mobius"
];

// Outward normal for a panel docked to a host edge, in degrees.
//
// A linear row has no outward side of its own, so the edge supplies it: a
// bottom panel points up, a top panel down, a left panel right, a right panel
// left. Returns null when the caller gave no edge or the layout already
// carries its own normal, which reproduces the historical path-derived value.
function hostEdgeNormalAngle(edge, resolvedLayout) {
    var name = String(edge || "");
    if (name === "" || radialLayoutNames.includes(resolvedLayout))
        return null;
    if (name === "bottom") return -90;
    if (name === "top") return 90;
    if (name === "left") return 0;
    if (name === "right") return 180;
    return null;
}

function metrics(layout, count, iconSize, spacing, scale, radius, rows,
                 padding, vertical, angle, polygonSides) {
    const resolvedLayout = normalizedLayout(layout, vertical);
    const safeCount = Math.max(1, Math.round(finite(count, 0)));
    const safeScale = finiteAtLeast(scale, 0.05, 1);
    const size = Math.max(16, finiteAtLeast(iconSize, 1, 52) * safeScale);
    const gap = Math.max(0, finite(spacing, 0) * safeScale);
    const safePadding = Math.max(0, finite(padding, 0));
    const safeRadius = Math.max(size * 0.75, finite(radius, 0) * safeScale);
    const safeRows = clamp(Math.round(finite(rows, 1)), 1, 8);
    const slot = size + gap;
    const linearLength = safeCount * size + Math.max(0, safeCount - 1) * gap;
    let width = linearLength + safePadding * 2;
    let height = size + safePadding * 2;

    if (resolvedLayout === "vertical") {
        width = size + safePadding * 2;
        height = linearLength + safePadding * 2;
    } else if (resolvedLayout === "diagonal") {
        // The last entry ends at (count - 1) steps plus its own size on both
        // axes; sizing the width by count whole steps left the final icon
        // hanging past the panel once a few entries were present.
        width = Math.max(size, size + Math.max(0, safeCount - 1) * slot * 0.78)
            + safePadding * 2;
        height = Math.max(size, size + Math.max(0, safeCount - 1) * slot * 0.34)
            + safePadding * 2;
    } else if (["circular", "ellipse", "ring", "radial", "polygon",
                "triangle", "square", "pentagon", "hexagon", "octagon",
                "star", "spiral", "vertical-curve"].includes(resolvedLayout)) {
        width = safeRadius * 2 + size + safePadding * 2;
        height = width;
    } else if (resolvedLayout === "mobius") {
        const reach = safeRadius * mobiusReach;
        width = reach * 2 + size + safePadding * 2;
        height = reach * 2 * mobiusHalfHeight() + size + safePadding * 2;
    } else if (resolvedLayout === "arc" || resolvedLayout === "semicircle"
               || resolvedLayout === "fan") {
        width = safeRadius * 2 + size + safePadding * 2;
        height = safeRadius + size * 1.7 + safePadding * 2;
    } else if (resolvedLayout === "ribbon"
               || resolvedLayout === "horizontal-curve") {
        width = linearLength + safePadding * 2;
        height = size * 2.15 + safePadding * 2;
    } else if (resolvedLayout === "grid" || resolvedLayout === "floating") {
        const columns = Math.ceil(safeCount / safeRows);
        width = columns * size + Math.max(0, columns - 1) * gap
            + safePadding * 2;
        height = safeRows * size + Math.max(0, safeRows - 1) * gap
            + safePadding * 2;
    }

    const radians = finite(angle, 0) * Math.PI / 180;
    const cosine = Math.abs(Math.cos(radians));
    const sine = Math.abs(Math.sin(radians));
    return {
        width: Math.max(1, Math.ceil(width * cosine + height * sine)),
        height: Math.max(1, Math.ceil(width * sine + height * cosine)),
        iconSize: size,
        spacing: gap,
        spacingReference: canonicalSpacing * safeScale,
        radius: safeRadius,
        rows: safeRows,
        sides: shapeSides(resolvedLayout, polygonSides),
        padding: safePadding,
        // How many entries the panel was laid out for: the spiral draws the
        // path that many entries stand on.
        count: safeCount,
        layout: resolvedLayout
    };
}

// A geometry object handed back in by a caller may be stale or partial; every
// field the path math reads is coerced once here.
function safeGeometry(geometry, layout) {
    const source = geometry || {};
    const size = Math.max(1, finite(source.iconSize, 52));
    return {
        width: Math.max(1, finite(source.width, size)),
        height: Math.max(1, finite(source.height, size)),
        iconSize: size,
        spacing: Math.max(0, finite(source.spacing, 0)),
        // Zero keeps the even spread: a hand-built geometry asks for nothing.
        spacingReference: Math.max(0, finite(source.spacingReference, 0)),
        radius: Math.max(size * 0.75, finite(source.radius, size * 0.75)),
        // The radius the entries are finally drawn at, when a renderer scales
        // the path onto its own track. Zero means the layout radius.
        trackRadius: Math.max(0, finite(source.trackRadius, 0)),
        // How many entry slots the entries have travelled along their path
        // (see trackPlacement). Runtime state supplied by the scene; never a
        // saved setting.
        travel: finite(source.travel, 0),
        rows: clamp(Math.round(finite(source.rows, 1)), 1, 8),
        sides: clamp(Math.round(finite(source.sides, 6)), 3, 12),
        count: Math.max(0, Math.round(finite(source.count, 0))),
        padding: Math.max(0, finite(source.padding, 0)),
        layout: source.layout || normalizedLayout(layout, false),
        // Set by rotationEnvelope(): open paths are then centred on the box
        // so a turning scene pivots on its own circle centre.
        rotationEnvelope: Boolean(source.rotationEnvelope)
    };
}

function rotate(point, centerX, centerY, degrees) {
    const radians = Number(degrees || 0) * Math.PI / 180;
    const cosine = Math.cos(radians);
    const sine = Math.sin(radians);
    const x = point.x - centerX;
    const y = point.y - centerY;
    return {
        x: centerX + x * cosine - y * sine,
        y: centerY + x * sine + y * cosine
    };
}

function polygonPoint(progress, sides, radius) {
    const segmentPosition = progress * sides;
    const segment = Math.floor(segmentPosition) % sides;
    const fraction = segmentPosition - Math.floor(segmentPosition);
    const startAngle = -Math.PI / 2 + segment * Math.PI * 2 / sides;
    const endAngle = startAngle + Math.PI * 2 / sides;
    const start = {
        x: Math.cos(startAngle) * radius,
        y: Math.sin(startAngle) * radius
    };
    const end = {
        x: Math.cos(endAngle) * radius,
        y: Math.sin(endAngle) * radius
    };
    const x = start.x + (end.x - start.x) * fraction;
    const y = start.y + (end.y - start.y) * fraction;
    return {
        x: x,
        y: y,
        tangent: Math.atan2(end.y - start.y, end.x - start.x) * 180 / Math.PI,
        radial: Math.atan2(y, x) * 180 / Math.PI
    };
}

// A star's notches lie at this share of its tips' radius. A 3D panel's
// generated platform follows the same star (PlatformGeometry.js).
var starInnerRadius = 0.46;

function starPoint(progress, points, radius) {
    const vertices = Math.max(3, points) * 2;
    const segmentPosition = progress * vertices;
    const segment = Math.floor(segmentPosition) % vertices;
    const fraction = segmentPosition - Math.floor(segmentPosition);
    function vertex(index) {
        const vertexRadius = index % 2 === 0 ? radius : radius * starInnerRadius;
        const radians = -Math.PI / 2 + index * Math.PI * 2 / vertices;
        return {
            x: Math.cos(radians) * vertexRadius,
            y: Math.sin(radians) * vertexRadius
        };
    }
    const start = vertex(segment);
    const end = vertex((segment + 1) % vertices);
    const x = start.x + (end.x - start.x) * fraction;
    const y = start.y + (end.y - start.y) * fraction;
    return {
        x: x,
        y: y,
        tangent: Math.atan2(end.y - start.y, end.x - start.x) * 180 / Math.PI,
        radial: Math.atan2(y, x) * 180 / Math.PI
    };
}

// A Möbius strip, as its pictures show it: a band along an ∞ with one half
// twist. Its middle line is a lemniscate of Bernoulli made a little fuller,
// 1 from its middle to each loop's far end; the loops cross in the middle.
// It is measured from where they cross (`0`), round the right loop to the
// crossing again (`0.5`) and round the left loop back. On the first pass
// through the crossing the band twists, seen edge on; on the second it
// passes flat over the first.
var mobiusFullness = 1.15;
var mobiusTable = null;
function mobiusLine(turn) {
    const s = Math.sin(turn), c = Math.cos(turn), d = 1 + s * s;
    return { x: c / d, y: mobiusFullness * s * c / d };
}
function mobiusSamples() {
    if (mobiusTable)
        return mobiusTable;
    const steps = 720;
    const points = [];
    let length = 0;
    for (let step = 0; step <= steps; ++step) {
        // From the crossing (a quarter turn), down into the right loop.
        const turn = Math.PI / 2 - 2 * Math.PI * step / steps;
        const point = mobiusLine(turn);
        if (points.length)
            length += Math.hypot(point.x - points[points.length - 1].x, point.y - points[points.length - 1].y);
        points.push({ x: point.x, y: point.y, turn: turn, along: length });
    }
    mobiusTable = { points: points, length: length };
    return mobiusTable;
}
// The length of the middle line for a reach of 1.
function mobiusLength() {
    return mobiusSamples().length;
}
// The point `progress` (0 to 1) of the way along the middle line, for a
// reach of `reach`, with the line's direction there (`tangent`, degrees) and
// the way from the middle (`radial`). `width` is how wide the band is drawn
// there against its full width: 0 where it twists, full where it passes
// flat over the crossing and nearly so round the loops. Its sign changes
// once a lap, so an edge traced over two laps (`progress` 0 to 2) is the
// band's one edge. `lift` is how far the band stands above its plane, -1 to
// 1: the strand passing flat over the crossing is above the twisting one.
function mobiusPoint(progress, reach) {
    const table = mobiusSamples();
    const target = wrapped(finite(progress, 0), 1) * table.length;
    let low = 0, high = table.points.length - 1;
    while (high - low > 1) {
        const middle = (low + high) >> 1;
        if (table.points[middle].along <= target)
            low = middle;
        else
            high = middle;
    }
    const a = table.points[low], b = table.points[high];
    const share = b.along > a.along ? (target - a.along) / (b.along - a.along) : 0;
    const x = (a.x + (b.x - a.x) * share) * reach;
    const y = (a.y + (b.y - a.y) * share) * reach;
    const twist = Math.sin(Math.PI * finite(progress, 0));
    return {
        x: x,
        y: y,
        tangent: Math.atan2(b.y - a.y, b.x - a.x) * 180 / Math.PI,
        radial: Math.atan2(y, x) * 180 / Math.PI,
        width: (twist < 0 ? -1 : 1) * Math.pow(Math.abs(twist), 0.35),
        lift: -Math.cos(2 * Math.PI * finite(progress, 0))
    };
}

// How far a Möbius panel's ∞ reaches from its middle against the panel's
// radius: wider than a circle of that radius, about half as tall.
var mobiusReach = 1.25;
// How far the middle line reaches up and down for a reach of 1.
function mobiusHalfHeight() {
    let most = 0;
    for (const point of mobiusSamples().points)
        most = Math.max(most, Math.abs(point.y));
    return most;
}
// Where the `slot`th of `count` entries of a Möbius band stands, as a share
// of its middle line from where the loops cross (mobiusPoint): each loop
// holds half of them, evenly, with a slot's gap at each of its ends, so none
// stands where the loops cross; a slot between two whole ones is on its way
// from one to the next, through the crossing at a loop's end.
function lobeProgress(slot, count) {
    return wrapped(lobeTravel(slot, count), 1);
}
// The same counted in laps and not wrapped: how far round the band an entry
// has gone. A Möbius strip has one face: a lap on, an entry stands at the
// same place on what looks like the other face, and two laps on it is back.
function lobeTravel(slot, count) {
    const total = Math.max(1, Math.round(finite(count, 1)));
    const first = Math.ceil(total / 2), second = total - first;
    const at = function(index) {
        return index < first ? (index + 1) * 0.5 / (first + 1)
            : 0.5 + (index - first + 1) * 0.5 / (second + 1);
    };
    const place = finite(slot, 0);
    const whole = Math.floor(place), share = place - whole;
    const laps = Math.floor(whole / total), index = wrapped(whole, total);
    const from = at(index) + laps;
    const to = (index + 1 < total ? at(index + 1) : at(0) + 1) + laps;
    return from + (to - from) * share;
}

function entryGeometry(layout, index, count, rawGeometry, angle, polygonSides,
                       pathOrientation, compatibilityProfile, edge) {
    const geometry = safeGeometry(rawGeometry, layout);
    const resolvedLayout = geometry.layout;
    const safeCount = Math.max(1, Math.round(finite(count, 0)));
    const safeIndex = clamp(Math.round(finite(index, 0)), 0, safeCount - 1);
    const size = geometry.iconSize;
    const slot = size + geometry.spacing;
    const centerX = geometry.width / 2;
    const centerY = geometry.height / 2;
    // Curved tracks place their entries through one rule, so the canonical
    // spacing regulates every one of them alike, and every one of them
    // travels the same way.
    const track = travelTrack(resolvedLayout, geometry, polygonSides, safeCount);
    const placement = track
        ? trackPlacement(safeIndex, safeCount, track.length, track.closed, size,
                         track.spacing, track.spacingReference, geometry.travel)
        : null;
    const progress = placement && !track.closed ? placement.progress
        : safeCount === 1 ? 0.5 : safeIndex / (safeCount - 1);
    let closedProgress = placement && track.closed ? placement.progress
        : safeIndex / safeCount;
    let surfaceProgress = 0;
    const profile = compatibilityProfile || "canonical";
    let x = geometry.padding;
    let y = geometry.padding;
    let rotation = 0;
    let tangent = 0;
    let radial = -90;

    if (resolvedLayout === "horizontal") {
        x += safeIndex * slot;
        tangent = 0;
        radial = -90;
    } else if (resolvedLayout === "vertical") {
        y += safeIndex * slot;
        tangent = 90;
        radial = 0;
    } else if (resolvedLayout === "diagonal") {
        x += safeIndex * slot * 0.78;
        y += safeIndex * slot * 0.34;
        tangent = Math.atan2(0.34, 0.78) * 180 / Math.PI;
        radial = tangent - 90;
    } else if (resolvedLayout === "circular" || resolvedLayout === "ring") {
        const radians = -Math.PI / 2 + closedProgress * Math.PI * 2;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
    } else if (resolvedLayout === "ellipse") {
        const radians = -Math.PI / 2 + closedProgress * Math.PI * 2;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius * 0.62 - size / 2;
        radial = Math.atan2(y + size / 2 - centerY,
                            x + size / 2 - centerX) * 180 / Math.PI;
        tangent = Math.atan2(Math.cos(radians) * geometry.radius * 0.62,
                             -Math.sin(radians) * geometry.radius) * 180 / Math.PI;
    } else if (resolvedLayout === "radial") {
        const radians = (-150 + progress * 300) * Math.PI / 180;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
    } else if (["polygon", "triangle", "square", "pentagon", "hexagon",
                "octagon", "star"].includes(resolvedLayout)) {
        const sides = shapeSides(resolvedLayout, polygonSides);
        const local = resolvedLayout === "star"
            ? starPoint(closedProgress, sides, geometry.radius)
            : polygonPoint(closedProgress, sides, geometry.radius);
        x = centerX + local.x - size / 2;
        y = centerY + local.y - size / 2;
        tangent = local.tangent;
        radial = local.radial;
    } else if (resolvedLayout === "mobius") {
        // Each loop holds half the entries, none where the loops cross; the
        // dock's turning carries them on through it, and round the band's
        // one face: `surfaceProgress` (0 to 2) is where on it an entry is.
        const travelled = lobeTravel(safeIndex + finite(geometry.travel, 0), safeCount);
        closedProgress = wrapped(travelled, 1);
        surfaceProgress = wrapped(travelled, 2);
        const local = mobiusPoint(closedProgress, geometry.radius * mobiusReach);
        x = centerX + local.x - size / 2;
        y = centerY + local.y - size / 2;
        tangent = local.tangent;
        radial = local.radial;
    } else if (resolvedLayout === "arc" || resolvedLayout === "semicircle"
               || resolvedLayout === "fan") {
        // The same sweep table draws the surface, so an entry always sits on
        // the path drawn under it.
        const path = pathSweep(resolvedLayout);
        const pathDegrees = path.start + progress * path.sweep;
        const radians = pathDegrees * Math.PI / 180;
        const runtimeProfile = profile === "runtime";
        const verticalFactor = resolvedLayout === "fan"
            ? (runtimeProfile ? 0.48 : 0.58)
            : (runtimeProfile ? 0.64 : 0.58);
        const pathCenterY = geometry.rotationEnvelope
            ? centerY
            : geometry.height - geometry.padding - size * verticalFactor;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = pathCenterY + Math.sin(radians) * geometry.radius - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
        if (resolvedLayout === "fan")
            rotation = (pathDegrees + 90) * 0.18;
    } else if (resolvedLayout === "spiral") {
        // Each slot is a fixed turn further round, so a travelling entry
        // follows the spiral's own curve, inwards or outwards.
        const radians = -Math.PI / 2 + (placement ? placement.slot : safeIndex) * 1.25;
        const distance = geometry.radius * (0.26 + 0.74 * progress);
        x = centerX + Math.cos(radians) * distance - size / 2;
        y = centerY + Math.sin(radians) * distance - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
    } else if (resolvedLayout === "ribbon"
               || resolvedLayout === "horizontal-curve") {
        x += safeIndex * slot;
        y = centerY - size / 2
            + Math.sin(progress * Math.PI * 2) * size * 0.36;
        rotation = Math.cos(progress * Math.PI * 2) * 8;
        tangent = Math.atan2(
            Math.cos(progress * Math.PI * 2) * size * 0.36 * Math.PI * 2,
            Math.max(slot, 1)) * 180 / Math.PI;
        radial = tangent - 90;
    } else if (resolvedLayout === "vertical-curve") {
        x = centerX - size / 2
            + Math.sin(progress * Math.PI * 2) * size * 0.6;
        y = geometry.padding + progress
            * Math.max(0, geometry.height - geometry.padding * 2 - size);
        tangent = 90 - Math.atan2(
            Math.cos(progress * Math.PI * 2) * size * 0.6 * Math.PI * 2,
            Math.max(slot, 1)) * 180 / Math.PI;
        radial = tangent - 90;
    } else if (resolvedLayout === "grid" || resolvedLayout === "floating") {
        const columns = Math.ceil(safeCount / geometry.rows);
        const row = Math.floor(safeIndex / columns);
        const column = safeIndex % columns;
        x += column * slot;
        y += row * slot;
        if (resolvedLayout === "floating") {
            x += Math.sin((safeIndex + 1) * 1.71) * geometry.spacing * 0.42;
            y += Math.cos((safeIndex + 1) * 1.29) * geometry.spacing * 0.42;
            rotation = Math.sin((safeIndex + 1) * 1.37) * 5;
        }
    }

    const safeAngle = finite(angle, 0);
    const rotatedPoint = rotate(
        { x: x + size / 2, y: y + size / 2 },
        centerX, centerY, safeAngle);
    x = rotatedPoint.x - size / 2;
    y = rotatedPoint.y - size / 2;

    const orientation = pathOrientation || "upright";
    const legacyRuntimeRadial = profile === "runtime"
        && !radialLayoutNames.includes(resolvedLayout);
    const orientationRadial = legacyRuntimeRadial ? 0 : radial;
    // A chosen orientation turns each icon with its path in every profile,
    // the free applet's live one included: Panel Studio offers it there
    // (ADREP-TASK-001, PD-08), so it has to act there.
    if (orientation === "tangent")
        rotation = tangent + safeAngle;
    else if (orientation === "radial")
        rotation = orientationRadial + safeAngle;
    else if (profile === "runtime"
             && (resolvedLayout === "fan" || resolvedLayout === "ribbon"))
        rotation += safeAngle * 0.35;
    // Upright means upright. The fan, ribbon and floating paths carry a small
    // decorative tilt that only the frozen runtime profile keeps; the canonical
    // and live scenes draw a configured-upright icon with no rotation, which is
    // what lets a rotated free panel keep its glyphs readable.
    if (orientation === "upright" && profile !== "runtime")
        rotation = 0;
    if (!isFinite(rotation))
        rotation = 0;
    if (!isFinite(tangent))
        tangent = 0;
    if (!isFinite(radial))
        radial = -90;

    const edgeNormal = hostEdgeNormalAngle(edge, resolvedLayout);
    const normalAngle = (edgeNormal === null ? radial : edgeNormal) + safeAngle;
    const normalRadians = normalAngle * Math.PI / 180;
    const closedLayouts = [
        "circular", "ellipse", "ring", "polygon", "triangle", "square",
        "pentagon", "hexagon", "octagon", "star", "mobius"
    ];
    const panelBounds = {
        x: 0,
        y: 0,
        width: geometry.width,
        height: geometry.height
    };
    const entryBounds = {
        x: x,
        y: y,
        width: size,
        height: size
    };
    return {
        position: { x: x, y: y },
        x: x,
        y: y,
        rotation: rotation,
        tangentAngle: tangent + safeAngle,
        outwardNormal: {
            x: Math.cos(normalRadians),
            y: Math.sin(normalRadians),
            angle: normalAngle
        },
        depthOrder: y + size / 2,
        scaleFactor: 1,
        pathProgress: closedLayouts.includes(resolvedLayout)
            ? closedProgress : progress,
        surfaceProgress: surfaceProgress,
        bounds: panelBounds,
        panelBounds: panelBounds,
        entryBounds: entryBounds,
        safeInputRegion: panelBounds,
        onTrack: placement ? placement.onTrack : true,
        trackVisibility: placement ? placement.visibility : 1,
        position3D: null,
        orientation3D: null
    };
}

// Compose independently padded runs using the same metrics and entry outputs
// as an ordinary panel. Entries already carry backend-validated segment IDs.
function segmentGeometry(segments, entries, layout, size, spacing, scale,
                         padding, vertical, angle, orientation, profile, edge) {
    const records = (segments || []).slice(0, 16).sort(function(a, b) {
        return Number(a.order || 0) - Number(b.order || 0);
    });
    const runs = [];
    const outputs = [];
    let length = 0;
    let thickness = 0;
    const gap = Math.max(0, finite(spacing, 8));
    for (const segment of records) {
        const indices = [];
        for (let i = 0; i < entries.length; ++i)
            if (String(entries[i].segmentId || "main") === String(segment.id))
                indices.push(i);
        const local = metrics(layout, indices.length, size,
            Number(segment.spacing) >= 0 ? segment.spacing : spacing, scale,
            0, 1, Number(segment.padding) >= 0 ? segment.padding : padding,
            vertical, angle, 6);
        runs.push({ id: String(segment.id), definition: segment, geometry: local,
            indices: indices, x: vertical ? 0 : length, y: vertical ? length : 0,
            width: local.width, height: local.height });
        length += (vertical ? local.height : local.width) + gap;
        thickness = Math.max(thickness, vertical ? local.width : local.height);
    }
    const combined = metrics(layout, entries.length, size, spacing, scale,
        0, 1, padding, vertical, 0, 6);
    combined.width = Math.max(1, vertical ? thickness : length - gap);
    combined.height = Math.max(1, vertical ? length - gap : thickness);
    for (const run of runs) {
        if (vertical) run.x = (thickness - run.width) / 2;
        else run.y = (thickness - run.height) / 2;
        for (let i = 0; i < run.indices.length; ++i) {
            const output = entryGeometry(layout, i, run.indices.length,
                run.geometry, angle, 6, orientation, profile, edge);
            output.x += run.x;
            output.y += run.y;
            output.position = { x: output.x, y: output.y };
            output.entryBounds = { x: output.x, y: output.y,
                width: run.geometry.iconSize, height: run.geometry.iconSize };
            output.bounds = output.panelBounds = output.safeInputRegion = {
                x: run.x, y: run.y, width: run.width, height: run.height };
            output.depthOrder += run.y;
            output.segmentId = run.id;
            outputs[run.indices[i]] = output;
        }
    }
    return { geometry: combined, segments: runs, entries: outputs };
}

function position(layout, index, count, geometry, angle, polygonSides,
                  pathOrientation, compatibilityProfile, edge) {
    const result = entryGeometry(
        layout, index, count, geometry, angle, polygonSides,
        pathOrientation, compatibilityProfile, edge);
    return {
        x: result.position.x,
        y: result.position.y,
        rotation: result.rotation
    };
}

// ---------------------------------------------------------------------------
// Baked 2.5D anchor tracks
//
// A track is theme metadata: it says where real application icons sit on a
// perspective platform and how far they shrink at the far side. Everything
// below is pure geometry. Nothing here draws, reads a host, or makes a
// renderer available; a track without an installed baked renderer is simply
// unused data.
//
// Depth is normalized: 0 at the far edge of the path, 1 at the near edge. The
// icon scale is interpolated between the theme's declared far and near scales,
// and the theme's occlusion depth decides which entries pass behind the
// declared foreground layers.
// ---------------------------------------------------------------------------

function trackShape(track) {
    const name = String(track && track.shape ? track.shape : "ellipse");
    return ["ellipse", "polygon", "arc"].includes(name) ? name : "ellipse";
}

function trackClosed(track) {
    const shape = trackShape(track);
    return shape === "ellipse" || shape === "polygon";
}

// Only a closed track can turn. Sweeping an open arc would carry its entries
// off the platform drawn under them.
function trackSupportsRotation(track) {
    return trackClosed(track);
}

// The artwork's own viewing angle is encoded in the ratio of its declared
// radii. A tilt request moves that angle by a bounded amount, and the same
// factor is applied to the drawn platform, so icons and artwork stay aligned.
function trackTiltFactor(track, requestedDegrees) {
    const source = track || {};
    const tilt = source.tilt;
    if (!tilt || typeof tilt !== "object")
        return 1;
    const minimum = finite(tilt.minimumDegrees, 0);
    const maximum = finite(tilt.maximumDegrees, 0);
    if (maximum < minimum)
        return 1;
    const fallback = clamp(finite(tilt.defaultDegrees, 0), minimum, maximum);
    const requested = requestedDegrees === undefined || requestedDegrees === null
        ? fallback
        : clamp(finite(requestedDegrees, fallback), minimum, maximum);
    const radiusX = Math.max(1, finite(source.radiusX, 1));
    const radiusY = Math.max(0, finite(source.radiusY, 0));
    const baseAngle = Math.asin(clamp(radiusY / radiusX, 0, 1));
    if (baseAngle <= 0.0001)
        return 1;
    const tilted = Math.sin(baseAngle + requested * Math.PI / 180);
    const factor = tilted / Math.sin(baseAngle);
    return isFinite(factor) ? clamp(factor, 0.05, 4) : 1;
}

// The scene box for a baked panel. The user's configured radius drives one
// uniform artwork scale, and the box is the union of the drawn platform and
// every scaled icon, so nothing the theme positions is cut off. `rotating`
// says the entries move round the track (the whole panel turns, or the
// entries travel), so the box must hold every place they can pass through.
function trackMetrics(track, artworkWidth, artworkHeight, count, iconSize,
                      padding, layoutRadius, tiltDegrees, rotating, placement) {
    const source = track || {};
    const regulation = placement && typeof placement === "object"
        ? placement : {};
    const size = Math.max(1, finiteAtLeast(iconSize, 1, 52));
    const safePadding = Math.max(0, finite(padding, 0));
    const safeCount = Math.max(0, Math.round(finite(count, 0)));
    const artwork = {
        width: Math.max(1, finite(artworkWidth, 1)),
        height: Math.max(1, finite(artworkHeight, 1))
    };
    const trackRadiusX = Math.max(1, finite(source.radiusX, 1));
    const trackRadiusY = Math.max(0, finite(source.radiusY, 0));
    const requestedRadius = finite(layoutRadius, 0);
    const scale = requestedRadius > 0
        ? clamp(requestedRadius / trackRadiusX, 0.02, 50) : 1;
    const tiltFactor = trackTiltFactor(source, tiltDegrees);
    const depth = source.depth && typeof source.depth === "object"
        ? source.depth : {};
    const farScale = clamp(finite(depth.farScale, 1), 0.01, 4);
    const nearScale = clamp(finite(depth.nearScale, 1), farScale, 4);
    const occlusionDepth = clamp(finite(depth.occlusionDepth, 0.5), 0, 1);

    const centerSource = source.center && typeof source.center === "object"
        ? source.center : {};
    const centerX = finite(centerSource.x, artwork.width / 2) * scale;
    const centerY = finite(centerSource.y, artwork.height / 2) * scale;
    // The platform is squashed about the track centre by the tilt factor, the
    // same factor the track's own vertical radius takes.
    const platform = {
        x: 0,
        y: centerY - centerY * tiltFactor,
        width: artwork.width * scale,
        height: artwork.height * scale * tiltFactor
    };

    const metrics = {
        shape: trackShape(source),
        closed: trackClosed(source),
        sides: clamp(Math.round(finite(source.sides, 8)), 3, 12),
        startDegrees: finite(source.startDegrees, 0),
        sweepDegrees: trackClosed(source) ? 360 : finite(source.sweepDegrees, 360),
        scale: scale,
        tiltFactor: tiltFactor,
        farScale: farScale,
        nearScale: nearScale,
        occlusionDepth: occlusionDepth,
        iconSize: size,
        padding: safePadding,
        count: safeCount,
        // The canonical spacing reads the track as the circle the artwork
        // shows in perspective, so its horizontal radius is the true one.
        spacing: Math.max(0, finite(regulation.spacing, 0)),
        spacingReference: Math.max(0, finite(regulation.spacingReference, 0)),
        trackLength: 0,
        windowed: false,
        capacity: safeCount,
        loop: safeCount,
        center: { x: centerX, y: centerY * tiltFactor + platform.y },
        radiusX: trackRadiusX * scale,
        radiusY: trackRadiusY * scale * tiltFactor,
        platform: platform,
        offsetX: 0,
        offsetY: 0,
        width: Math.max(1, Math.ceil(platform.width)),
        height: Math.max(1, Math.ceil(platform.height))
    };

    metrics.trackLength = metrics.shape === "polygon"
        ? metrics.sides * 2 * metrics.radiusX * Math.sin(Math.PI / metrics.sides)
        : metrics.sweepDegrees * Math.PI / 180 * metrics.radiusX;
    const window = trackPlacement(
        0, safeCount, metrics.trackLength, metrics.closed, size, metrics.spacing,
        metrics.spacingReference, 0);
    metrics.windowed = window.windowed;
    metrics.capacity = window.capacity;
    metrics.loop = window.loop;

    let left = platform.x;
    let top = platform.y;
    let right = platform.x + platform.width;
    let bottom = platform.y + platform.height;
    // A turning ring puts icons at angles no resting entry occupies, so the
    // box is measured around the whole closed path instead of the entries the
    // panel happens to hold. Otherwise the host would be asked to resize as
    // the scene rotated, which is exactly what the envelope exists to avoid.
    // An open track is measured at its resting slots and as far past each end
    // as a leaving entry goes, so travel never resizes it either.
    const wholePath = rotating && metrics.closed && safeCount > 0;
    const samples = wholePath ? Math.max(safeCount, 72) : safeCount;
    const points = [];
    for (let index = 0; index < samples; ++index) {
        points.push(wholePath
            ? trackPointAt(metrics, index / samples, 0)
            : trackPoint(metrics, index, samples, 0, 0));
    }
    if (!metrics.closed && safeCount > 1) {
        points.push(trackPointAt(metrics, window.startProgress, 0));
        points.push(trackPointAt(metrics, window.endProgress, 0));
    }
    for (const point of points) {
        const extent = size * point.scaleFactor / 2;
        left = Math.min(left, point.x - extent);
        top = Math.min(top, point.y - extent);
        right = Math.max(right, point.x + extent);
        bottom = Math.max(bottom, point.y + extent);
    }
    left -= safePadding;
    top -= safePadding;
    right += safePadding;
    bottom += safePadding;

    metrics.offsetX = -left;
    metrics.offsetY = -top;
    metrics.center = { x: metrics.center.x - left, y: metrics.center.y - top };
    metrics.platform = {
        x: platform.x - left,
        y: platform.y - top,
        width: platform.width,
        height: platform.height
    };
    metrics.width = Math.max(1, Math.ceil(right - left));
    metrics.height = Math.max(1, Math.ceil(bottom - top));
    return metrics;
}

// One anchor point in the coordinate space of the metrics it is given: the
// artwork's own while trackMetrics is still sizing the box, the scene box once
// those metrics are returned. Kept separate so the box is sized from the same
// numbers the entries will use. `travel` is the entries' travel in slots.
function trackPoint(metrics, index, count, rotationDegrees, travel) {
    const placement = trackPlacement(
        index, count, metrics.trackLength, metrics.closed, metrics.iconSize,
        metrics.spacing, metrics.spacingReference, travel);
    const point = trackPointAt(metrics, placement.progress, rotationDegrees);
    point.onTrack = placement.onTrack;
    point.visibility = placement.visibility;
    return point;
}

// The point a given fraction of the way along the track.
function trackPointAt(metrics, progress, rotationDegrees) {
    const closed = metrics.closed;
    const degrees = metrics.startDegrees + progress * metrics.sweepDegrees
        + (closed ? finite(rotationDegrees, 0) : 0);
    const radians = (degrees - 90) * Math.PI / 180;

    let unitX = Math.cos(radians);
    let unitY = Math.sin(radians);
    if (metrics.shape === "polygon") {
        const local = polygonPoint(
            ((degrees % 360) + 360) % 360 / 360, metrics.sides, 1);
        unitX = local.x;
        unitY = local.y;
    }
    const x = metrics.center.x + unitX * metrics.radiusX;
    const y = metrics.center.y + unitY * metrics.radiusY;
    // Depth follows how far down the path the point sits: the top of a
    // perspective ring is its far side.
    const depth = clamp((unitY + 1) / 2, 0, 1);
    const scaleFactor = metrics.farScale
        + (metrics.nearScale - metrics.farScale) * depth;
    return {
        x: x,
        y: y,
        unitX: unitX,
        unitY: unitY,
        radians: radians,
        degrees: degrees,
        progress: progress,
        depth: depth,
        scaleFactor: isFinite(scaleFactor) ? scaleFactor : 1
    };
}

// The full geometry contract for one entry on a track, in the same shape the
// linear and radial layouts return, so a scene consumes either without knowing
// which produced it.
function trackEntryGeometry(track, index, count, metrics, rotationDegrees,
                            pathOrientation, tiltDegrees, travel) {
    const resolved = metrics && metrics.center
        ? metrics
        : trackMetrics(track, 0, 0, count, 0, 0, 0, tiltDegrees);
    const safeCount = Math.max(1, Math.round(finite(count, 0)));
    const point = trackPoint(resolved, index, safeCount, rotationDegrees, travel);
    const size = resolved.iconSize;
    // trackMetrics() has already moved the track centre into the scene box,
    // so a point on the track is a scene point. Adding the box offset again
    // would push every entry off the platform by the panel's padding.
    const x = point.x - size / 2;
    const y = point.y - size / 2;
    const visualExtent = size * point.scaleFactor;

    // The outward direction of an ellipse is its gradient, not the ray from
    // the centre; a popup anchored on the ray would drift at the flanks.
    const normalX = resolved.radiusX > 0
        ? point.unitX / resolved.radiusX : point.unitX;
    const normalY = resolved.radiusY > 0
        ? point.unitY / resolved.radiusY : point.unitY;
    const normalLength = Math.hypot(normalX, normalY);
    const outwardX = normalLength > 0 ? normalX / normalLength : 0;
    const outwardY = normalLength > 0 ? normalY / normalLength : -1;
    const normalAngle = Math.atan2(outwardY, outwardX) * 180 / Math.PI;
    const tangentAngle = Math.atan2(
        point.unitX * resolved.radiusY,
        -point.unitY * resolved.radiusX) * 180 / Math.PI;

    const orientation = pathOrientation || "upright";
    let rotation = 0;
    if (orientation === "tangent")
        rotation = tangentAngle;
    else if (orientation === "radial")
        rotation = normalAngle;
    if (!isFinite(rotation))
        rotation = 0;

    const panelBounds = {
        x: 0,
        y: 0,
        width: resolved.width,
        height: resolved.height
    };
    const entryBounds = {
        x: point.x - visualExtent / 2,
        y: point.y - visualExtent / 2,
        width: visualExtent,
        height: visualExtent
    };
    return {
        position: { x: x, y: y },
        x: x,
        y: y,
        rotation: rotation,
        tangentAngle: isFinite(tangentAngle) ? tangentAngle : 0,
        outwardNormal: {
            x: outwardX,
            y: outwardY,
            angle: isFinite(normalAngle) ? normalAngle : -90
        },
        depth: point.depth,
        depthOrder: point.depth,
        inFront: point.depth >= resolved.occlusionDepth,
        scaleFactor: point.scaleFactor,
        pathProgress: point.progress,
        bounds: panelBounds,
        panelBounds: panelBounds,
        entryBounds: entryBounds,
        safeInputRegion: panelBounds,
        onTrack: point.onTrack !== false,
        trackVisibility: point.visibility === undefined ? 1 : point.visibility,
        position3D: null,
        orientation3D: null
    };
}

function surface(layout, rawGeometry, rawAngle, polygonSides) {
    const geometry = safeGeometry(rawGeometry, layout);
    const resolvedLayout = geometry.layout;
    const angle = finite(rawAngle, 0);
    const centerX = geometry.width / 2;
    const centerY = geometry.height / 2;
    const radius = geometry.radius;
    const points = [];
    let closed = false;
    let samples = 72;
    if (resolvedLayout === "mobius") {
        // The ∞ the band follows, drawn as one track like a ring's.
        closed = true;
        for (let index = 0; index < 192; ++index) {
            const at = mobiusPoint(index / 192, radius * mobiusReach);
            points.push(rotate({ x: centerX + at.x, y: centerY + at.y }, centerX, centerY, angle));
        }
    } else if (["circular", "ring", "ellipse", "polygon", "triangle", "square",
         "pentagon", "hexagon", "octagon", "star"].includes(resolvedLayout)) {
        closed = true;
        const sides = shapeSides(resolvedLayout, polygonSides);
        samples = ["polygon", "triangle", "square", "pentagon", "hexagon",
                   "octagon"].includes(resolvedLayout) ? sides
            : resolvedLayout === "star" ? sides * 2 : 72;
        for (let index = 0; index < samples; ++index) {
            const progress = index / samples;
            let local;
            if (resolvedLayout === "star")
                local = starPoint(progress, sides, radius);
            else if (["polygon", "triangle", "square", "pentagon", "hexagon",
                      "octagon"].includes(resolvedLayout))
                local = polygonPoint(progress, sides, radius);
            else {
                const radians = -Math.PI / 2 + progress * Math.PI * 2;
                local = {
                    x: Math.cos(radians) * radius,
                    y: Math.sin(radians) * radius
                        * (resolvedLayout === "ellipse" ? 0.62 : 1)
                };
            }
            points.push(rotate(
                { x: centerX + local.x, y: centerY + local.y },
                centerX, centerY, angle));
        }
    } else if (resolvedLayout === "arc" || resolvedLayout === "semicircle"
               || resolvedLayout === "fan" || resolvedLayout === "radial") {
        const path = pathSweep(resolvedLayout);
        const sweep = path.sweep;
        const start = path.start;
        for (let index = 0; index < samples; ++index) {
            const radians = (start + index / (samples - 1) * sweep)
                * Math.PI / 180;
            const yCenter = resolvedLayout === "radial" || geometry.rotationEnvelope
                ? centerY : geometry.height - geometry.padding
                    - geometry.iconSize * 0.58;
            points.push(rotate({
                x: centerX + Math.cos(radians) * radius,
                y: yCenter + Math.sin(radians) * radius
            }, centerX, centerY, angle));
        }
    } else if (resolvedLayout === "spiral") {
        // The path the entries stand on (entryGeometry): a fixed turn per
        // entry, outwards from a quarter of the radius, so every entry sits on
        // the drawn line and travels along it (ADREP-TASK-002). A geometry
        // that does not say how many entries it holds keeps the older figure.
        const span = Math.max(1, geometry.count - 1);
        for (let index = 0; index < samples; ++index) {
            const progress = index / (samples - 1);
            const radians = geometry.count > 0
                ? -Math.PI / 2 + progress * span * 1.25
                : -Math.PI / 2 + progress * Math.PI * 4.5;
            const distance = geometry.count > 0
                ? radius * (0.26 + progress * 0.74)
                : radius * (0.16 + progress * 0.84);
            points.push(rotate({
                x: centerX + Math.cos(radians) * distance,
                y: centerY + Math.sin(radians) * distance
            }, centerX, centerY, angle));
        }
    } else if (resolvedLayout === "vertical"
               || resolvedLayout === "vertical-curve") {
        for (let index = 0; index < samples; ++index) {
            const progress = index / (samples - 1);
            const wave = resolvedLayout === "vertical-curve"
                ? Math.sin(progress * Math.PI * 2) * geometry.iconSize * 0.6 : 0;
            points.push(rotate({
                x: centerX + wave,
                y: geometry.padding + geometry.iconSize / 2
                    + progress * Math.max(0, geometry.height
                        - geometry.padding * 2 - geometry.iconSize)
            }, centerX, centerY, angle));
        }
    } else if (resolvedLayout === "grid" || resolvedLayout === "floating") {
        const inset = geometry.padding + geometry.iconSize * 0.16;
        points.push(
            { x: inset, y: inset },
            { x: geometry.width - inset, y: inset },
            { x: geometry.width - inset, y: geometry.height - inset },
            { x: inset, y: geometry.height - inset });
        closed = true;
    } else {
        for (let index = 0; index < samples; ++index) {
            const progress = index / (samples - 1);
            const x = geometry.padding + geometry.iconSize / 2
                + progress * Math.max(0, geometry.width
                    - geometry.padding * 2 - geometry.iconSize);
            let y = centerY;
            if (resolvedLayout === "diagonal")
                y += (progress - 0.5) * Math.max(0, geometry.height
                    - geometry.padding * 2 - geometry.iconSize);
            else if (resolvedLayout === "ribbon"
                     || resolvedLayout === "horizontal-curve")
                y += Math.sin(progress * Math.PI * 2)
                    * geometry.iconSize * 0.36;
            points.push(rotate({ x: x, y: y }, centerX, centerY, angle));
        }
    }
    return { points: points, closed: closed };
}

// The texture a look is drawn with in a palette: the glowing looks come in
// palettes of their own (materials/<look>-<palette>.png); "original", or a
// palette the look does not have, is the look's own file.
function lookTexture(look, palette) {
    const name = String(look || "");
    const chosen = String(palette || "");
    const palettes = {
        "lightning": ["ice", "crimson", "toxic", "solar", "arcane", "void", "aurora", "rainbow"],
        "aurora": ["emerald", "australis", "cosmic", "solar", "rainbow"],
        "plasma": ["rainbow"],
        "neon": ["rainbow"],
        "futuristic": ["rainbow"],
        // Polished, heat-tinted metal shows a faint rainbow (the owner,
        // 2026-10-13: "chameleon"); lime stays a lime.
        "metallic": ["chameleon", "iridescent"],
        "plate": ["chameleon", "iridescent"]
    };
    const list = palettes[name];
    return list && list.indexOf(chosen) >= 0 ? name + "-" + chosen : name;
}

function themeStyle(appearance, customColor, iconSize) {
    const preset = appearance || "glass";
    const styles = {
        "glass": {
            stroke: "#b5d3e0",
            shadow: "rgba(28, 56, 74, 0.18)", width: 0.48, blur: 10
        },
        "crystal": {
            stroke: "rgba(158, 238, 255, 0.90)",
            shadow: "rgba(158, 238, 255, 0.72)", width: 0.38, blur: 17
        },
        "neon": {
            stroke: "#50e6ff", shadow: "#35cfff", width: 0.25, blur: 25
        },
        "minimal": {
            stroke: "rgba(76, 89, 102, 0.88)",
            shadow: "transparent", width: 0.18, blur: 0
        },
        "plasma": {
            stroke: "#895cff", shadow: "#d94cff", width: 0.40, blur: 23
        },
        // Bolts in a dark storm: a white-blue light, flickering and striking
        // anew (the owner, 2026-10-13).
        "lightning": {
            stroke: "#c4d0ff", shadow: "#7d8cff", width: 0.32, blur: 26
        },
        // Curtains of polar light in a night sky (the owner, 2026-10-13).
        "aurora": {
            stroke: "#68ffb0", shadow: "#7a5cff", width: 0.36, blur: 24
        },
        // A lime's peel: a natural citrus green, lit, not glowing (the owner,
        // 2026-10-13: lime is named for the fruit).
        "lime": {
            stroke: "#86c93b", shadow: "rgba(38, 82, 16, 0.45)", width: 0.30, blur: 12
        },
        "floating-glass": {
            stroke: "#caeafb",
            shadow: "rgba(38, 70, 89, 0.28)", width: 0.50, blur: 18
        },
        "metallic": {
            stroke: "#aeb9c4", shadow: "rgba(0, 0, 0, 0.72)",
            width: 0.52, blur: 8
        },
        "futuristic": {
            stroke: "#b030d8", shadow: "#35cfff", width: 0.38, blur: 28
        },
        "organic": {
            stroke: "#57c98b", shadow: "rgba(44, 133, 83, 0.74)",
            width: 0.46, blur: 13
        },
        "platform": {
            stroke: "#6e7884", shadow: "rgba(0, 0, 0, 0.72)",
            width: 0.72, blur: 9
        },
        "plate": {
            stroke: "#9ea9b4",
            shadow: "rgba(0, 0, 0, 0.45)", width: 0.64, blur: 4
        },
        "pedestal": {
            stroke: "#8395a6",
            shadow: "rgba(0, 0, 0, 0.48)", width: 0.82, blur: 5
        },
        // A picture the user imported, filling a wide band.
        "picture": {
            stroke: "#c8d4de",
            shadow: "rgba(0, 0, 0, 0.5)", width: 0.86, blur: 8
        }
    };
    const result = styles[preset] || styles.glass;
    return {
        stroke: customColor && customColor.length > 0
            ? customColor : result.stroke,
        shadow: result.shadow,
        lineWidth: Math.max(3, iconSize * result.width),
        blur: result.blur,
        trackVisible: true,
        material: styles[preset] ? preset : "glass",
        alpha: preset === "glass" ? 0.56 : preset === "floating-glass" ? 0.50 : 1,
        depth: preset === "pedestal" ? 9 : preset === "platform" ? 6
            : preset === "plate" ? 3 : preset === "floating-glass" ? 5 : 0,
        animated: preset === "futuristic" || preset === "plasma" || preset === "lightning"
            || preset === "aurora",
        // Lightning flickers and strikes instead of flowing.
        flicker: preset === "lightning",
        // Light on a clear ground, the look's dark sky drawn behind it at
        // the chosen strength: lightning and aurora.
        clearGround: preset === "lightning" || preset === "aurora",
        skyColour: preset === "aurora" ? "6, 10, 20" : "12, 10, 31",
        sparkle: preset === "crystal" || preset === "plasma",
        // Drawn once over the whole shape, not run along the track.
        picture: preset === "picture"
    };
}

function nearestIndex(layout, count, geometry, angle, pointX, pointY,
                      polygonSides, pathOrientation, compatibilityProfile) {
    let nearest = 0;
    let nearestDistance = Number.MAX_VALUE;
    for (let index = 0; index < count; ++index) {
        const candidate = position(
            layout, index, count, geometry, angle, polygonSides,
            pathOrientation, compatibilityProfile);
        const centerX = candidate.x + geometry.iconSize / 2;
        const centerY = candidate.y + geometry.iconSize / 2;
        const distance = Math.pow(centerX - pointX, 2)
            + Math.pow(centerY - pointY, 2);
        if (distance < nearestDistance) {
            nearest = index;
            nearestDistance = distance;
        }
    }
    return nearest;
}

function anchorOffset(anchor, containerWidth, containerHeight, geometry) {
    const availableWidth = Math.max(0, containerWidth - geometry.width);
    const availableHeight = Math.max(0, containerHeight - geometry.height);
    let x = availableWidth / 2;
    let y = availableHeight / 2;

    if (anchor === "top-left" || anchor === "left" || anchor === "bottom-left")
        x = 0;
    else if (anchor === "top-right" || anchor === "right"
             || anchor === "bottom-right")
        x = availableWidth;

    if (anchor === "top-left" || anchor === "top" || anchor === "top-right")
        y = 0;
    else if (anchor === "bottom-left" || anchor === "bottom"
             || anchor === "bottom-right")
        y = availableHeight;

    return { x: x, y: y };
}

function expansionOffset(layout, index, count, iconSize, spacing, radius, rows) {
    const safeCount = clamp(Math.floor(finite(count, 1)), 1, 48);
    index = clamp(Math.floor(finite(index, 0)), 0, safeCount - 1);
    iconSize = clamp(finite(iconSize, 48), 16, 128);
    spacing = clamp(finite(spacing, 8), 0, 32);
    const safeRadius = clamp(finite(radius, 120), iconSize * 1.2, 4096);
    const safeRows = Math.min(safeCount,
        clamp(Math.round(finite(rows, Math.ceil(Math.sqrt(safeCount)))), 1, 8));
    const slot = iconSize + Math.max(0, spacing);
    const progress = safeCount === 1 ? 0.5 : index / (safeCount - 1);
    if (layout === "grid") {
        const columns = Math.ceil(safeCount / safeRows);
        return {
            x: (index % columns - (columns - 1) / 2) * slot,
            y: (Math.floor(index / columns) - (safeRows - 1) / 2) * slot
        };
    }
    if (layout === "stack")
        return {
            x: index * Math.max(iconSize * 0.65, spacing),
            y: -index * Math.max(iconSize * 0.2, spacing * 0.5)
        };
    if (layout === "vertical")
        return { x: 0, y: (index - (safeCount - 1) / 2) * slot };
    if (layout === "horizontal")
        return { x: (index - (safeCount - 1) / 2) * slot, y: 0 };
    if (layout === "spiral") {
        const radians = index * 1.82 - Math.PI / 2;
        const distance = iconSize * 1.1 + index * Math.max(iconSize * 0.33, spacing);
        return { x: Math.cos(radians) * distance, y: Math.sin(radians) * distance };
    }
    if (layout === "ring" || layout === "circular" || layout === "radial") {
        const radians = -Math.PI / 2 + index * Math.PI * 2 / safeCount;
        return { x: Math.cos(radians) * safeRadius, y: Math.sin(radians) * safeRadius };
    }
    if (layout === "arc" || layout === "fan" || layout === "elastic"
        || layout === "physics") {
        const degrees = -72 + progress * 144;
        const radians = (degrees - 90) * Math.PI / 180;
        const distance = layout === "fan" ? safeRadius * 0.82 : safeRadius;
        return {
            x: Math.cos(radians) * distance,
            y: Math.sin(radians) * distance + safeRadius * 0.35
        };
    }
    return { x: (index - (safeCount - 1) / 2) * slot, y: 0 };
}

// The side of the clicked folder a popup opens on. Its contents are laid out
// in that side's frame: "out" leads away from the icon and the dock, "across"
// runs beside them. Without a side a popup keeps its original frame and opens
// to the right.
function expansionSide(side) {
    return ["left", "right", "top", "bottom"].includes(side) ? side : "right";
}

function expansionVertical(side) {
    return side === "top" || side === "bottom";
}

// Folder expansion shares the canonical positions and their measured bounds.
// Large pages may scroll; they never shrink icons into unreadable hit targets.
// `anchor` is the point of the contents the clicked folder stands against.
function expansionGeometry(layout, count, iconSize, spacing, radius, rows, options) {
    // A popup draws no band: a Möbius opening is its ring there.
    const requested = layout === "mobius" ? "ring" : String(layout || "fan");
    const resolved = ["fan", "grid", "stack", "arc", "ring"].includes(requested)
        ? requested : "fan";
    const safeCount = clamp(Math.floor(finite(count, 0)), 0, 48);
    const size = clamp(finite(iconSize, 48), 16, 128);
    const gap = clamp(finite(spacing, 8), 0, 32);
    const presentation = options || {};
    const oriented = ["left", "right", "top", "bottom"].includes(presentation.side);
    const side = expansionSide(presentation.side);
    const vertical = expansionVertical(side);
    // How far the folder's outward direction leans along "across".
    const lean = clamp(finite(presentation.lean, 0), -1, 1);
    const cellWidth = Math.max(size, clamp(finite(presentation.labelWidth, 0), 0, 256));
    const cellHeight = size + clamp(finite(presentation.labelHeight, 0), 0, 128);
    const cellAcross = vertical ? cellWidth : cellHeight;
    const availableWidth = Math.max(cellWidth, finite(presentation.maximumWidth, 0));
    const bounded = finite(presentation.maximumWidth, 0) > 0;
    const followsPath = bounded && presentation.compactPath === true
        && (resolved === "fan" || resolved === "arc");
    const columns = Math.max(1, Math.min(safeCount,
        Math.floor((availableWidth + gap) / (cellWidth + gap))));
    const extent = cellWidth === size && cellHeight === size
        ? size : Math.sqrt(cellWidth * cellWidth + cellHeight * cellHeight);
    let distance = clamp(finite(radius, 120), size * 1.2, 4096);
    if (safeCount > 1) {
        const step = resolved === "ring" ? 2 * Math.PI / safeCount
            : 144 * Math.PI / 180 / (safeCount - 1);
        const needed = (extent + gap) / (2 * Math.sin(step / 2));
        if (resolved === "ring" || resolved === "arc" || resolved === "fan")
            distance = Math.max(distance, needed / (resolved === "fan" ? 0.82 : 1));
    }
    // A compact Fan or Arc is an exact half circle that opens away from the
    // folder. It keeps the folder's own radius, shrinks for the few children
    // that need less, and gives way to the room it has. It is never enlarged
    // to hold every child and never straightened: expansionPath() says how
    // many children stand on it at once, and the rest are reached along the
    // curve. pathRadiusX runs out from the folder, pathRadiusY across it.
    const pathPitch = extent + gap;
    const pathClearance = cellAcross + gap;
    const heightLimit = finite(presentation.maximumHeight, 0);
    const outLimit = vertical ? (heightLimit > 0 ? heightLimit - cellHeight : Infinity)
        : availableWidth - cellWidth;
    const acrossLimit = vertical ? (availableWidth - cellWidth) / 2
        : (heightLimit > 0 ? (heightLimit - cellHeight) / 2 : Infinity);
    let pathRadiusX = 0, pathRadiusY = 0;
    if (followsPath && safeCount > 1) {
        const nominal = clamp(finite(radius, 120), size * 1.2, 4096);
        const wanted = Math.min(nominal, Math.max(size * 1.2, pathClearance / 2,
            (safeCount - 1) * pathPitch / Math.PI));
        pathRadiusX = Math.max(0, Math.min(wanted, outLimit));
        pathRadiusY = Math.max(0, Math.min(wanted, acrossLimit));
    }
    const pathWidth = Math.ceil(vertical ? 2 * pathRadiusY + cellWidth : pathRadiusX + cellWidth);
    const pathHeight = Math.ceil(vertical ? pathRadiusX + cellHeight : 2 * pathRadiusY + cellHeight);
    const pathShape = {
        count: safeCount, side: side, cellWidth: cellWidth, cellHeight: cellHeight,
        width: pathWidth, height: pathHeight, pathPitch: pathPitch,
        pathClearance: pathClearance, pathRadiusX: pathRadiusX,
        pathRadiusY: pathRadiusY
    };
    const restingPath = followsPath ? expansionPath(pathShape, pathHeight) : null;
    // A frame direction in screen axes. Offsets are scaled by the cell, as
    // the canonical positions always were.
    const frame = vertical
        ? { out: { x: 0, y: side === "top" ? -1 : 1 }, across: { x: 1, y: 0 } }
        : { out: { x: side === "left" ? -1 : 1, y: 0 }, across: { x: 0, y: 1 } };
    const points = [];
    let minimumX = Infinity, minimumY = Infinity;
    let maximumX = -Infinity, maximumY = -Infinity;
    for (let index = 0; index < safeCount; ++index) {
        let point;
        if (followsPath) {
            // Where the child rests before any scrolling; a child that is not
            // on the path yet waits at its far end.
            point = expansionPathPoint(pathShape, restingPath,
                Math.min(index, restingPath.capacity - 1), 0);
            point = { x: point.x, y: point.y };
        } else if (bounded && resolved === "fan") {
            // A long fan bends within the view and grows vertically. Every
            // child remains reachable without a second scrolling axis.
            const progress = safeCount <= 1 ? 0 : index / (safeCount - 1);
            const bend = Math.min(finite(radius, 140), availableWidth - cellWidth);
            point = { x: Math.sin(progress * Math.PI / 2) * Math.max(0, bend),
                      y: index * (cellHeight + gap) };
        } else if (bounded && resolved === "grid") {
            point = { x: (index % columns) * (cellWidth + gap),
                      y: Math.floor(index / columns) * (cellHeight + gap) };
        } else if (oriented && resolved === "ring") {
            // A ring begins beside the folder and turns once around.
            const turn = Math.PI + index * 2 * Math.PI / Math.max(1, safeCount);
            const out = distance * Math.cos(turn), across = distance * Math.sin(turn);
            point = { x: (frame.out.x * out + frame.across.x * across) * cellWidth / size,
                      y: (frame.out.y * out + frame.across.y * across) * cellHeight / size };
        } else if (oriented && resolved === "stack") {
            // A stack rises from the folder, drifting the way the folder leans.
            const offset = expansionOffset(resolved, index, safeCount, size, gap, distance, rows);
            const across = lean > 0.38 ? -offset.y : offset.y;
            point = { x: (frame.out.x * offset.x + frame.across.x * across) * cellWidth / size,
                      y: (frame.out.y * offset.x + frame.across.y * across) * cellHeight / size };
        } else {
            point = expansionOffset(resolved, index, safeCount, size, gap, distance, rows);
            point = { x: point.x * cellWidth / size, y: point.y * cellHeight / size };
        }
        points.push(point);
        minimumX = Math.min(minimumX, point.x);
        minimumY = Math.min(minimumY, point.y);
        maximumX = Math.max(maximumX, point.x);
        maximumY = Math.max(maximumY, point.y);
    }
    if (safeCount === 0) {
        minimumX = minimumY = maximumX = maximumY = 0;
    }
    const width = followsPath ? pathWidth : Math.ceil(maximumX - minimumX + cellWidth);
    const height = followsPath ? pathHeight : Math.ceil(maximumY - minimumY + cellHeight);
    const entries = points.map(function(point, index) {
        return followsPath ? { index: index, x: point.x, y: point.y }
            : { index: index, x: point.x - minimumX, y: point.y - minimumY };
    });
    // The folder stands against the edge nearest it. Across that edge it is
    // centred, or toward the side its outward direction leans, so the
    // contents lean away from the dock with it; a stack starts at the folder.
    const acrossExtent = vertical ? width : height;
    let anchorAcross = acrossExtent / 2 - lean * Math.max(0, acrossExtent / 2 - cellAcross / 2);
    if (followsPath)
        anchorAcross = pathRadiusY + cellAcross / 2 - lean * pathRadiusY;
    else if (resolved === "stack" && entries.length > 0)
        anchorAcross = (vertical ? entries[0].x : entries[0].y) + cellAcross / 2;
    const anchorOut = side === "left" ? width : side === "top" ? height : 0;
    return {
        layout: resolved,
        requestedLayout: requested,
        fallbackApplied: resolved !== requested,
        fallbackReason: resolved !== requested ? "unsupported-folder-layout" : "",
        count: safeCount,
        side: side,
        iconSize: size,
        cellWidth: cellWidth,
        cellHeight: cellHeight,
        followsPath: followsPath,
        pathPitch: pathPitch,
        pathClearance: pathClearance,
        pathRadiusX: pathRadiusX,
        pathRadiusY: pathRadiusY,
        width: width,
        height: height,
        anchor: vertical ? { x: anchorAcross, y: anchorOut } : { x: anchorOut, y: anchorAcross },
        origin: followsPath ? { x: size / 2, y: size / 2 }
                            : { x: size / 2 - minimumX, y: size / 2 - minimumY },
        entries: entries
    };
}

// How far to move a free panel drawn at `rect` on the screen so that it
// stands at least `gap` from every other panel's drawn rectangle in `others`
// ({ x, y, width, height, gap }): straight away from the one it is most too
// close to, no further than that needs, again for the next one, and kept on
// `screen` where given. Returns the move { x, y }; nothing to do is 0, 0.
function keepApart(rect, others, screen) {
    const box = { x: finite(rect && rect.x, 0), y: finite(rect && rect.y, 0),
                  width: Math.max(0, finite(rect && rect.width, 0)),
                  height: Math.max(0, finite(rect && rect.height, 0)) };
    const near = (others || []).filter(function(other) {
        return other && isFinite(other.x) && isFinite(other.y) && finite(other.width, 0) > 0
            && finite(other.height, 0) > 0 && finite(other.gap, 0) > 0;
    });
    // The distance between two rectangles' nearest edges, 0 where they meet.
    function apart(a, b) {
        const dx = Math.max(0, b.x - (a.x + a.width), a.x - (b.x + b.width));
        const dy = Math.max(0, b.y - (a.y + a.height), a.y - (b.y + b.height));
        return Math.hypot(dx, dy);
    }
    function onScreen(move) {
        if (!screen || !(finite(screen.width, 0) > box.width) || !(finite(screen.height, 0) > box.height))
            return move;
        return { x: clamp(box.x + move.x, screen.x, screen.x + screen.width - box.width) - box.x,
                 y: clamp(box.y + move.y, screen.y, screen.y + screen.height - box.height) - box.y };
    }
    let move = { x: 0, y: 0 };
    for (let round = 0; round < 4; ++round) {
        const at = { x: box.x + move.x, y: box.y + move.y, width: box.width, height: box.height };
        let worst = null;
        for (const other of near) {
            const short = finite(other.gap, 0) - apart(at, other);
            if (short > 0.5 && (!worst || short > worst.short))
                worst = { other: other, short: short };
        }
        if (!worst)
            break;
        const other = worst.other;
        let ux = at.x + at.width / 2 - (other.x + other.width / 2);
        let uy = at.y + at.height / 2 - (other.y + other.height / 2);
        const length = Math.hypot(ux, uy);
        if (length < 1e-6) {
            ux = 1;
            uy = 0;
        } else {
            ux /= length;
            uy /= length;
        }
        // Moving straight away keeps adding distance: the least that is
        // enough, to a tenth of a pixel.
        let low = 0, high = other.gap + Math.hypot(at.width, at.height) + Math.hypot(other.width, other.height);
        for (let step = 0; step < 32 && high - low > 0.1; ++step) {
            const middle = (low + high) / 2;
            if (apart({ x: at.x + ux * middle, y: at.y + uy * middle, width: at.width, height: at.height }, other)
                    >= other.gap)
                high = middle;
            else
                low = middle;
        }
        move = onScreen({ x: move.x + ux * high, y: move.y + uy * high });
    }
    return move;
}

// How far apart two samples of a folder's path are: on the screen, or with
// `unscaled` as if drawn at scale 1 (their scales' mean).
function trackRun(a, b, unscaled) {
    const run = Math.hypot(b.x - a.x, b.y - a.y);
    return unscaled ? run / Math.max(0.05, (finite(a.scale, 1) + finite(b.scale, 1)) / 2) : run;
}

// A free panel's folder contents stand on a path: "Along the dock" is the
// dock's own curve beside the folder, the other shapes come from
// folderShape(). `samples` trace the path on screen, in order, as
// { x, y, scale }. Neighbours keep `pitch` pixels apart along it.
//
// An open path holds as many slots as fit, at most `options.capacity`,
// centred on its middle sample - the folder faces it - or, with
// `options.start` "start", from its first sample. Fewer children than slots
// stand centred among them. `offset` moves every child along the path by
// that many slots: the wheel and keys set it; it is never saved. The
// path is a loop one slot longer than it is, or as long as the folder, so a
// child that leaves one end fades out a pitch past it and comes back at the
// other end, whether or not every child fits.
//
// A closed path (`options.closed`, a ring) spreads its children evenly round
// it. A folder longer than the ring holds shows that many at once; the others
// come in where the path starts as one leaves there.
//
// Only a child wholly on the path is `onTrack`, which is what may be drawn
// solid and pressed; `visibility` fades one that is leaving or arriving.
//
// A closed path may have further circles round it (`options.rings`, a ring
// folder's second circle; see ringedTrackLayout()).
//
// With `options.unscaled` (a shape on a 3D panel's plane, whose children are
// drawn at their samples' scale) the path is measured as if drawn at scale
// 1: neighbours stand a pitch apart at the size each is drawn, near or far.
//
// A Möbius band's path (`options.lobes`) is laid out by lobedTrackLayout().
function folderTrackLayout(samples, count, pitch, offset, options) {
    const settings = options || {};
    if (settings.lobes === true)
        return lobedTrackLayout(samples, count, pitch, offset, settings);
    const closed = settings.closed === true;
    const unscaled = settings.unscaled === true;
    const rings = closed ? Array.from(settings.rings || []).filter(function(ring) {
        return ring && ring.length >= 3;
    }) : [];
    if (rings.length > 0)
        return ringedTrackLayout([samples].concat(rings), count, pitch, offset,
                                 { unscaled: unscaled, limits: settings.ringLimits });
    // A list read back from a QML property is indexable but not an Array.
    const points = [];
    const source = samples || [];
    for (let index = 0; index < Math.floor(finite(source.length, 0)); ++index) {
        const sample = source[index];
        if (sample && isFinite(sample.x) && isFinite(sample.y))
            points.push(sample);
    }
    const total = clamp(Math.floor(finite(count, 0)), 0, 48);
    const empty = { capacity: 0, loop: 0, closed: closed, windowed: false,
                    maximumOffset: 0, entries: [] };
    if (points.length < 2 || total === 0)
        return empty;
    if (closed)
        points.push(points[0]);
    const lengths = [0];
    for (let index = 1; index < points.length; ++index)
        lengths.push(lengths[index - 1] + trackRun(points[index - 1], points[index], unscaled));
    const length = lengths[lengths.length - 1];
    if (!(length > 0))
        return empty;
    const spacing = Math.max(1, finite(pitch, 1));
    const travel = finite(offset, 0);
    const limit = Math.floor(finite(settings.capacity, 0));
    // The point `position` pixels along the path. Past an open path's ends a
    // child carries on the way the path was going there.
    function pointAt(position) {
        const target = closed ? wrapped(position, length) : position;
        let segment = 0;
        while (segment < lengths.length - 2 && lengths[segment + 1] < target)
            ++segment;
        const span = lengths[segment + 1] - lengths[segment];
        const t = span > 0 ? (target - lengths[segment]) / span : 0;
        const a = points[segment], b = points[segment + 1];
        const scaleA = finite(a.scale, 1), scaleB = finite(b.scale, 1);
        return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t,
                 scale: scaleA + (scaleB - scaleA) * clamp(t, 0, 1) };
    }
    const entries = [];
    if (closed) {
        // A sampled curve is a hair shorter than the curve it traces.
        const fit = Math.max(1, Math.floor(length / spacing + 0.01));
        const capacity = Math.max(1, Math.min(total, fit, limit > 0 ? limit : fit));
        const step = length / capacity;
        for (let index = 0; index < total; ++index) {
            const position = wrapped(index + travel, total);
            let slot = position;
            let visibility = 1;
            if (total > capacity && position > capacity - 1) {
                if (position < capacity) {
                    // Leaving: on towards the start, fading out.
                    visibility = capacity - position;
                } else if (position > total - 1) {
                    // Arriving: from one slot before the start, fading in.
                    slot = position - total;
                    visibility = 1 - (total - position);
                } else {
                    slot = 0;
                    visibility = 0;
                }
            }
            const point = pointAt(slot * step);
            entries.push({ index: index, x: point.x, y: point.y, scale: point.scale, slot: slot,
                           position: position,
                           onTrack: total <= capacity || position <= capacity - 1 + 1e-9,
                           visibility: clamp(visibility, 0, 1) });
        }
        return { capacity: capacity, loop: total, closed: true, windowed: total > capacity,
                 maximumOffset: Math.max(0, total - capacity), spacing: step, entries: entries };
    }
    const fromStart = settings.start === "start";
    // Centred on the middle sample, or on a plane on the middle of the path
    // as drawn, whose two halves perspective makes unequal.
    const middle = fromStart ? 0 : unscaled ? length / 2 : lengths[Math.floor((points.length - 1) / 2)];
    const usable = fromStart ? length : 2 * Math.min(middle, length - middle);
    let capacity = Math.floor(usable / spacing + 0.01) + 1;
    if (limit > 0)
        capacity = Math.min(capacity, limit);
    // A short folder stands centred on whole slots, so a child never rests
    // half past an end.
    if (!fromStart && total < capacity)
        capacity -= (capacity - total) % 2;
    capacity = Math.max(1, capacity);
    const first = fromStart ? 0 : middle - (capacity - 1) * spacing / 2;
    const loop = Math.max(total, capacity + 1);
    const rest = fromStart || total >= capacity ? 0 : (capacity - total) / 2;
    for (let index = 0; index < total; ++index) {
        const position = wrapped(index + rest + travel, loop);
        let slot = position;
        let visibility = 1;
        if (position > capacity - 1) {
            if (position < capacity) {
                visibility = capacity - position;
            } else if (position > loop - 1) {
                slot = position - loop;
                visibility = 1 - (loop - position);
            } else {
                // Waiting off the path for its turn: not drawn, kept at the
                // last slot.
                slot = capacity - 1;
                visibility = 0;
            }
        }
        const point = pointAt(first + slot * spacing);
        entries.push({ index: index, x: point.x, y: point.y, scale: point.scale, slot: slot,
                       position: position, onTrack: position <= capacity - 1 + 1e-9,
                       visibility: clamp(visibility, 0, 1) });
    }
    return { capacity: capacity, loop: loop, closed: false, windowed: total > capacity,
             maximumOffset: Math.max(0, total - capacity), spacing: spacing, entries: entries,
             // Where a child leaving or arriving stands as it fades out.
             ends: [pointAt(first - spacing), pointAt(first + capacity * spacing)] };
}

// A Möbius band's children (folderShape "mobius"). Its path starts where
// its two loops cross and crosses again halfway; each loop holds half the
// children, evenly, with a slot's gap at each of its ends, so at rest none
// stands where the loops cross. The wheel moves them round, from one loop
// into the other through the crossing. As on a closed path, a folder with
// more than it holds shows that many; the others come in where the path
// starts as one leaves there.
function lobedTrackLayout(samples, count, pitch, offset, options) {
    const settings = options || {};
    const unscaled = settings.unscaled === true;
    const points = [];
    const source = samples || [];
    for (let index = 0; index < Math.floor(finite(source.length, 0)); ++index) {
        const sample = source[index];
        if (sample && isFinite(sample.x) && isFinite(sample.y))
            points.push(sample);
    }
    const total = clamp(Math.floor(finite(count, 0)), 0, 48);
    const empty = { capacity: 0, loop: 0, closed: true, windowed: false, maximumOffset: 0, entries: [] };
    if (points.length < 3 || total === 0)
        return empty;
    points.push(points[0]);
    const lengths = [0];
    for (let index = 1; index < points.length; ++index)
        lengths.push(lengths[index - 1] + trackRun(points[index - 1], points[index], unscaled));
    const length = lengths[lengths.length - 1];
    if (!(length > 0))
        return empty;
    const spacing = Math.max(1, finite(pitch, 1));
    const limit = Math.floor(finite(settings.capacity, 0));
    const fit = Math.max(1, Math.floor(length / spacing + 0.01) - 2);
    const capacity = Math.max(1, Math.min(total, fit, limit > 0 ? limit : fit));
    // A slot's place along the path (lobeProgress), between slots for a
    // child on its way.
    const along = function(slot) { return lobeProgress(slot, capacity) * length; };
    const first = Math.ceil(capacity / 2);
    function pointAt(target) {
        let segment = 0;
        while (segment < lengths.length - 2 && lengths[segment + 1] < target)
            ++segment;
        const span = lengths[segment + 1] - lengths[segment];
        const t = span > 0 ? (target - lengths[segment]) / span : 0;
        const a = points[segment], b = points[segment + 1];
        const scaleA = finite(a.scale, 1), scaleB = finite(b.scale, 1);
        return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t,
                 scale: scaleA + (scaleB - scaleA) * clamp(t, 0, 1) };
    }
    const travel = finite(offset, 0);
    const entries = [];
    for (let index = 0; index < total; ++index) {
        const position = wrapped(index + travel, total);
        let slot = position;
        let visibility = 1;
        if (total > capacity && position > capacity - 1) {
            if (position < capacity) {
                visibility = capacity - position;
            } else if (position > total - 1) {
                slot = position - total;
                visibility = 1 - (total - position);
            } else {
                slot = 0;
                visibility = 0;
            }
        }
        const point = pointAt(along(slot));
        entries.push({ index: index, x: point.x, y: point.y, scale: point.scale, slot: slot,
                       position: position,
                       onTrack: total <= capacity || position <= capacity - 1 + 1e-9,
                       visibility: clamp(visibility, 0, 1) });
    }
    return { capacity: capacity, loop: total, closed: true, windowed: total > capacity,
             maximumOffset: Math.max(0, total - capacity), spacing: length / 2 / (first + 1), entries: entries };
}

// folderTrackLayout() for a ring folder with circles round it (`loops`, the
// ring first): the children fill the ring, then the next circle out, each
// circle's spread evenly round it; travel moves them on round the ring, out
// across to the next circle and from the last back to the ring's start. A
// folder longer than the circles hold shows that many at once, and the
// others come in where the ring starts as one leaves there, as on one ring.
function ringedTrackLayout(loops, count, pitch, offset, options) {
    const total = clamp(Math.floor(finite(count, 0)), 0, 48);
    const spacing = Math.max(1, finite(pitch, 1));
    const travel = finite(offset, 0);
    const settings = options || {};
    const unscaled = settings.unscaled === true;
    // At most this many at once on each circle, where the shape says so.
    const limits = Array.from(settings.limits || []);
    const circles = [];
    for (const source of loops) {
        const points = [];
        for (let index = 0; index < Math.floor(finite(source && source.length, 0)); ++index) {
            const sample = source[index];
            if (sample && isFinite(sample.x) && isFinite(sample.y))
                points.push(sample);
        }
        if (points.length < 3)
            continue;
        points.push(points[0]);
        const lengths = [0];
        for (let index = 1; index < points.length; ++index)
            lengths.push(lengths[index - 1] + trackRun(points[index - 1], points[index], unscaled));
        const length = lengths[lengths.length - 1];
        const limit = Math.floor(finite(limits[circles.length], 0));
        const fit = Math.max(1, Math.floor(length / spacing + 0.01));
        if (length > 0)
            circles.push({ points: points, lengths: lengths, length: length,
                           fit: limit > 0 ? Math.min(fit, limit) : fit });
    }
    if (!circles.length || total === 0)
        return { capacity: 0, loop: 0, closed: true, windowed: false, maximumOffset: 0, entries: [], rings: 0 };
    // How many each circle holds at once: the ring full first, then the next.
    const shares = [];
    let left = total;
    for (const circle of circles) {
        const share = Math.min(circle.fit, left);
        if (share <= 0)
            break;
        shares.push(share);
        left -= share;
    }
    const slots = shares.reduce(function(sum, share) { return sum + share; }, 0);
    function pointOn(circle, distance) {
        const target = wrapped(distance, circle.length);
        let segment = 0;
        while (segment < circle.lengths.length - 2 && circle.lengths[segment + 1] < target)
            ++segment;
        const span = circle.lengths[segment + 1] - circle.lengths[segment];
        const t = span > 0 ? (target - circle.lengths[segment]) / span : 0;
        const a = circle.points[segment], b = circle.points[segment + 1];
        const scaleA = finite(a.scale, 1), scaleB = finite(b.scale, 1);
        return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t,
                 scale: scaleA + (scaleB - scaleA) * clamp(t, 0, 1) };
    }
    // Where slot `slot` is, whole or between two: round its circle, or
    // straight across from one circle's last slot to the next one's first.
    function slotPoint(slot) {
        const whole = Math.floor(slot), part = slot - whole;
        const index = wrapped(whole, slots);
        let circle = 0, first = 0;
        while (circle < shares.length - 1 && index >= first + shares[circle]) {
            first += shares[circle];
            ++circle;
        }
        const along = index - first, step = circles[circle].length / shares[circle];
        if (part <= 1e-9)
            return pointOn(circles[circle], along * step);
        if (along + 1 < shares[circle])
            return pointOn(circles[circle], (along + part) * step);
        const here = pointOn(circles[circle], along * step), next = slotPoint(whole + 1);
        return { x: here.x + (next.x - here.x) * part, y: here.y + (next.y - here.y) * part,
                 scale: here.scale + (next.scale - here.scale) * part };
    }
    const entries = [];
    for (let index = 0; index < total; ++index) {
        const position = wrapped(index + travel, total);
        let slot = position;
        let visibility = 1;
        if (total > slots && position > slots - 1) {
            if (position < slots) {
                // Leaving: on towards the ring's start, fading out.
                visibility = slots - position;
            } else if (position > total - 1) {
                // Arriving: from one slot before the start, fading in.
                slot = position - total;
                visibility = 1 - (total - position);
            } else {
                slot = 0;
                visibility = 0;
            }
        }
        const point = slotPoint(slot);
        entries.push({ index: index, x: point.x, y: point.y, scale: point.scale, slot: slot,
                       position: position,
                       onTrack: total <= slots || position <= slots - 1 + 1e-9,
                       visibility: clamp(visibility, 0, 1) });
    }
    return { capacity: slots, loop: total, closed: true, windowed: total > slots,
             maximumOffset: Math.max(0, total - slots), spacing: spacing, entries: entries,
             rings: shares.length };
}

// folderTrackLayout() for a folder laid out as a grid (`grid`, from
// folderShape(): its columns, its rows and its cells as drawn, with a row
// more before the first and after the last): the children fill it row by
// row. Travel moves them a row at a time, each straight on to its place in
// the next row; the last row's go out past it, fading, and new ones come in
// before the first. `capacity` and `position` count rows, for
// folderTravelTo().
function gridTrackLayout(grid, count, offset) {
    const total = clamp(Math.floor(finite(count, 0)), 0, 48);
    const columns = Math.max(1, Math.floor(finite(grid && grid.columns, 1)));
    const rows = Math.max(1, Math.floor(finite(grid && grid.rows, 1)));
    const cells = grid && grid.cells ? Array.from(grid.cells) : [];
    const empty = { capacity: 0, loop: 0, closed: false, windowed: false, maximumOffset: 0, entries: [] };
    if (total === 0 || cells.length < rows + 2)
        return empty;
    // A cell, or where one is out of sight the nearest in its column.
    function cellAt(row, column) {
        for (let step = 0; step <= rows + 1; ++step) {
            for (const near of [row - step, row + step]) {
                const line = cells[clamp(near, -1, rows) + 1];
                const point = line ? line[Math.min(column, line.length - 1)] : null;
                if (point && isFinite(point.x) && isFinite(point.y))
                    return point;
            }
        }
        return null;
    }
    function at(row, column) {
        const lower = clamp(Math.floor(row), -1, rows), upper = Math.min(rows, lower + 1);
        const t = clamp(row - lower, 0, 1);
        const a = cellAt(lower, column), b = cellAt(upper, column);
        if (!a || !b)
            return null;
        const scaleA = finite(a.scale, 1), scaleB = finite(b.scale, 1);
        return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t, scale: scaleA + (scaleB - scaleA) * t };
    }
    const travel = finite(offset, 0);
    const totalRows = Math.ceil(total / columns);
    const loop = Math.max(totalRows, rows + 1);
    const entries = [];
    for (let index = 0; index < total; ++index) {
        const column = index % columns;
        const position = wrapped(Math.floor(index / columns) + travel, loop);
        let row = position;
        let visibility = 1;
        if (position > rows - 1) {
            if (position < rows) {
                // Going out past the last row, fading out.
                visibility = rows - position;
            } else if (position > loop - 1) {
                // Coming in before the first row, fading in.
                row = position - loop;
                visibility = 1 - (loop - position);
            } else {
                // Waiting for its turn: not drawn, kept at the last row.
                row = rows - 1;
                visibility = 0;
            }
        }
        const point = at(row, column) || { x: 0, y: 0, scale: 1 };
        entries.push({ index: index, x: point.x, y: point.y, scale: point.scale, slot: row * columns + column,
                       position: position, onTrack: position <= rows - 1 + 1e-9,
                       visibility: clamp(visibility, 0, 1) });
    }
    return { capacity: rows, loop: loop, closed: false, windowed: totalRows > rows,
             maximumOffset: Math.max(0, totalRows - rows), spacing: 0, entries: entries,
             rows: rows, columns: columns };
}

// The travel that brings child `index` onto the path with the least motion:
// the given travel when it is on already, otherwise the nearer of moving it
// to the first or the last slot.
function folderTravelTo(layout, index, travel) {
    const track = layout || {};
    const current = finite(travel, 0);
    const entry = (track.entries || [])[index];
    if (!entry || entry.onTrack === true || !(track.loop > 0))
        return current;
    const forward = track.loop - entry.position;
    const backward = entry.position - (Math.max(1, finite(track.capacity, 1)) - 1);
    return forward <= backward ? current + forward : current - backward;
}

// The convex outline round `points` ({ x, y }): its corners in order,
// turning the same way at each, without repeating the first.
function convexHull(points) {
    const sorted = (points || []).filter(function(point) {
        return point && isFinite(point.x) && isFinite(point.y);
    }).slice().sort(function(a, b) { return a.x - b.x || a.y - b.y; });
    if (sorted.length < 3)
        return sorted;
    function turn(o, a, b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }
    const lower = [], upper = [];
    for (const point of sorted) {
        while (lower.length >= 2 && turn(lower[lower.length - 2], lower[lower.length - 1], point) <= 0)
            lower.pop();
        lower.push(point);
    }
    for (let index = sorted.length - 1; index >= 0; --index) {
        const point = sorted[index];
        while (upper.length >= 2 && turn(upper[upper.length - 2], upper[upper.length - 1], point) <= 0)
            upper.pop();
        upper.push(point);
    }
    return lower.slice(0, -1).concat(upper.slice(0, -1));
}

// Whether `rect` ({ x, y, width, height }) comes within `margin` pixels of
// what a folder's children keep clear of: `keepOut.hull`, a convex outline
// (the platform as drawn), and `keepOut.rects` (its icons with their
// stands). A missing rect touches everything.
function rectTouchesKeepOut(rect, keepOut, margin) {
    if (!rect)
        return true;
    if (!keepOut)
        return false;
    const m = Math.max(0, finite(margin, 0));
    const r = { left: rect.x - m, top: rect.y - m, right: rect.x + rect.width + m, bottom: rect.y + rect.height + m };
    for (const other of keepOut.rects || []) {
        if (r.left < other.x + other.width && r.right > other.x
                && r.top < other.y + other.height && r.bottom > other.y)
            return true;
    }
    const hull = keepOut.hull || [];
    if (hull.length < 3)
        return false;
    const box = keepOut.hullBox || (keepOut.hullBox = unitedBounds(hull.map(function(point) {
        return { x: point.x, y: point.y, width: 0, height: 0 };
    })));
    if (r.right < box.x || r.left > box.x + box.width || r.bottom < box.y || r.top > box.y + box.height)
        return false;
    // Inside a convex outline: on the inner side of every edge.
    function inside(x, y) {
        for (let index = 0; index < hull.length; ++index) {
            const a = hull[index], b = hull[(index + 1) % hull.length];
            if ((b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x) < 0)
                return false;
        }
        return true;
    }
    if (inside(r.left, r.top) || inside(r.right, r.top) || inside(r.left, r.bottom) || inside(r.right, r.bottom)
            || inside((r.left + r.right) / 2, (r.top + r.bottom) / 2))
        return true;
    for (const point of hull)
        if (point.x >= r.left && point.x <= r.right && point.y >= r.top && point.y <= r.bottom)
            return true;
    // An edge of the outline crossing the rectangle between its corners.
    function crosses(a, b) {
        const t0 = Math.max(Math.min(a.x, b.x), r.left), t1 = Math.min(Math.max(a.x, b.x), r.right);
        if (t0 > t1)
            return false;
        if (Math.abs(b.x - a.x) < 1e-9)
            return Math.max(Math.min(a.y, b.y), r.top) <= Math.min(Math.max(a.y, b.y), r.bottom);
        const y0 = a.y + (b.y - a.y) * (t0 - a.x) / (b.x - a.x), y1 = a.y + (b.y - a.y) * (t1 - a.x) / (b.x - a.x);
        return Math.max(Math.min(y0, y1), r.top) <= Math.min(Math.max(y0, y1), r.bottom);
    }
    for (let index = 0; index < hull.length; ++index)
        if (crosses(hull[index], hull[(index + 1) % hull.length]))
            return true;
    return false;
}

// One rectangle round all of `rectangles`, each { x, y, width, height }.
function unitedBounds(rectangles) {
    let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity;
    for (const rectangle of rectangles) {
        left = Math.min(left, rectangle.x);
        top = Math.min(top, rectangle.y);
        right = Math.max(right, rectangle.x + rectangle.width);
        bottom = Math.max(bottom, rectangle.y + rectangle.height);
    }
    return isFinite(left) ? { x: left, y: top, width: right - left, height: bottom - top }
                          : { x: 0, y: 0, width: 0, height: 0 };
}

// The free-panel folder shapes other than "Along the dock" (ADREP-TASK-003),
// in screen coordinates: the path the children stand on (for
// folderTrackLayout), the outline drawn under them and the room they need.
//
//   fan    PD-11  a sector: its apex just outside the folder, its edges
//                 `fanOpening` degrees apart about the outward direction,
//                 the children on its arc.
//   arc    PD-13  the children at one distance from the folder, facing it,
//                 symmetric about the outward direction.
//   stack  PD-12  a straight line from the folder outward, `stackLength`
//                 children at a time.
//   ring   PD-14  a circle beside the folder along the outward direction,
//                 "small" to fit the children or "panel" at the dock's radius.
//   mobius        a Möbius strip beside the folder along the outward
//                 direction: a band along an ∞ with one half twist (see
//                 mobiusPoint), each loop holding half the children.
//
// A shape opens along the folder's outward direction, `outward`, and turns
// away from it only as far as the screen requires. Its radius - a ring's
// distance - grows until no place a child passes through covers an icon of
// the dock (`obstacles`, centres of `obstacleSize` squares) or the folder.
// `cellWidth` and `cellHeight` are one child with its name, its icon
// `iconSize` square at the top; `outlineMargin` is how far the drawn outline
// reaches beyond its line.
//
// With `project`, the shape lies on a plane - a 3D panel's own, round the
// folder - and `project(point)` is where a child standing at a point of that
// plane has its middle on the screen, with its `scale` there, or null where
// the plane is out of sight. `folder`, `outward` and the sizes are then the
// plane's own, and a child keeps clear of the dock's icons where they stand
// on the plane (`planeObstacles`), as things standing on a table do - or,
// given `keepOut`, of the panel as drawn on the screen, by `keepOutMargin`
// pixels; `screen` stays on the screen, where every child's cell is
// measured at its scale. The samples (each with its
// `scale`), the outline, the bounds and the apex or centre come back on the
// screen, so the shape keeps the panel's tilt.
function folderShape(shape, options) {
    const value = options || {};
    const kind = ["fan", "arc", "stack", "ring", "grid", "mobius"].includes(shape) ? shape : "fan";
    // A Möbius band opens where a ring does and shows as many at once as a
    // ring of one circle.
    const ringLike = kind === "ring" || kind === "mobius";
    const panelSized = kind === "ring" && value.ringSize === "panel";
    const folder = { x: finite(value.folder && value.folder.x, 0),
                     y: finite(value.folder && value.folder.y, 0) };
    const facing = { x: finite(value.outward && value.outward.x, 0),
                     y: finite(value.outward && value.outward.y, -1) };
    const facingLength = Math.hypot(facing.x, facing.y);
    const outward = facingLength > 1e-6
        ? { x: facing.x / facingLength, y: facing.y / facingLength } : { x: 0, y: -1 };
    const size = clamp(finite(value.iconSize, 48), 8, 256);
    const cellWidth = Math.max(size, finite(value.cellWidth, size));
    const cellHeight = Math.max(size, finite(value.cellHeight, size));
    const gap = clamp(finite(value.gap, 6), 0, 64);
    const count = clamp(Math.floor(finite(value.count, 0)), 0, 48);
    const named = cellWidth > size + 0.5 || cellHeight > size + 0.5;
    // Neighbours on a curve stand a name's diagonal apart, so no two names
    // touch whichever way the curve runs; icons alone keep the dock's pitch.
    const curvePitch = named ? Math.hypot(cellWidth, cellHeight) + gap : size + 14;
    const obstacleSize = Math.max(0, finite(value.obstacleSize, size));
    const obstacles = value.fromEdge === true ? [] : [{ x: folder.x, y: folder.y }];
    for (const point of value.obstacles || [])
        if (point && isFinite(point.x) && isFinite(point.y))
            obstacles.push({ x: Number(point.x), y: Number(point.y) });
    const outlineMargin = Math.max(0, finite(value.outlineMargin, 0));
    // The outline's own line, without the glow round it.
    const outlineWidth = Math.max(0, finite(value.outlineWidth, 2 * outlineMargin));
    // `folder` is already an edge the shape opens from (a 3D platform's
    // rim), not the folder icon's centre: a fan's apex and a stack's first
    // child stand on it rather than a square's reach beyond it.
    const fromEdge = value.fromEdge === true;
    // The screen, less the margin the window keeps round the shape.
    const inset = Math.max(0, finite(value.screenMargin, 0));
    const screen = value.screen && finite(value.screen.width, 0) > 2 * inset
        ? { x: finite(value.screen.x, 0) + inset, y: finite(value.screen.y, 0) + inset,
            width: finite(value.screen.width, 0) - 2 * inset,
            height: finite(value.screen.height, 0) - 2 * inset }
        : null;

    const project = typeof value.project === "function" ? value.project : null;
    // On a plane a child nearer than the folder is drawn no larger than the
    // folder's own icon (`largestScale`); one farther away, smaller.
    const largestScale = finite(value.largestScale, 0) > 0.35 ? finite(value.largestScale, 0) : Infinity;
    // How far each circle of a two-circle ring leans on a plane.
    const gimbalLean = 10 * Math.PI / 180;
    // Where a point is seen on the screen, and how large a child is drawn
    // there; a flat shape is its own picture, at size 1.
    function seen(point) {
        if (!project)
            return { x: point.x, y: point.y, scale: 1 };
        const shown = project(point);
        if (!shown || !isFinite(shown.x) || !isFinite(shown.y))
            return null;
        const at = { x: Number(shown.x), y: Number(shown.y),
                     scale: Math.min(largestScale, Math.max(0.35, finite(shown.scale, 1))) };
        // A ring's two circles lean opposite ways on the screen, about their
        // middle, and cross like the rings of a gimbal.
        if (point.lean && point.about) {
            const middle = project(point.about);
            if (!middle || !isFinite(middle.x) || !isFinite(middle.y))
                return null;
            const turn = point.lean * gimbalLean;
            const c = Math.cos(turn), s = Math.sin(turn);
            const dx = at.x - middle.x, dy = at.y - middle.y;
            at.x = middle.x + dx * c - dy * s;
            at.y = middle.y + dx * s + dy * c;
        }
        return at;
    }
    // A child's cell on the screen; null where it is out of sight.
    function cell(point) {
        const p = seen(point);
        return p ? { x: p.x - cellWidth * p.scale / 2, y: p.y - size * p.scale / 2,
                     width: cellWidth * p.scale, height: cellHeight * p.scale } : null;
    }
    // Where the dock's icons stand on a plane, and how close a child may
    // stand to one there: their two halves and a gap.
    const footprints = project ? (value.planeObstacles || []).filter(function(point) {
        return point && isFinite(point.x) && isFinite(point.y);
    }) : [];
    const footprintReach = (obstacleSize + size) / 2 + gap;
    // On a plane: the panel as drawn on the screen (`keepOut`, see
    // rectTouchesKeepOut()), which no child may cover or touch.
    const keepOut = project && value.keepOut ? value.keepOut : null;
    const keepOutMargin = Math.max(0, finite(value.keepOutMargin, 8));
    // Other free panels as drawn on the screen (`avoid`): no child covers
    // one, on a plane or flat.
    const avoid = (value.avoid || []).filter(function(rect) {
        return rect && isFinite(rect.x) && isFinite(rect.y) && finite(rect.width, 0) > 0
            && finite(rect.height, 0) > 0;
    });
    function overOther(rect) {
        for (const other of avoid)
            if (rect.x < other.x + other.width && rect.x + rect.width > other.x
                    && rect.y < other.y + other.height && rect.y + rect.height > other.y)
                return true;
        return false;
    }
    // Whether a child standing at `point` keeps clear of every dock icon.
    function clear(point) {
        if (project) {
            if (!seen(point))
                return false;
            if (overOther(cell(point)))
                return false;
            if (keepOut)
                return !rectTouchesKeepOut(cell(point), keepOut, keepOutMargin);
            for (const icon of footprints)
                if (Math.hypot(point.x - icon.x, point.y - icon.y) < footprintReach)
                    return false;
            return true;
        }
        const child = cell(point);
        if (!child || overOther(child))
            return false;
        const reach = obstacleSize / 2 + 4;
        for (const obstacle of obstacles) {
            if (child.x < obstacle.x + reach && child.x + child.width > obstacle.x - reach
                    && child.y < obstacle.y + reach && child.y + child.height > obstacle.y - reach)
                return false;
        }
        return true;
    }
    function turned(direction, radians) {
        const c = Math.cos(radians), s = Math.sin(radians);
        return { x: direction.x * c - direction.y * s, y: direction.x * s + direction.y * c };
    }
    function along(origin, direction, distance) {
        return { x: origin.x + direction.x * distance, y: origin.y + direction.y * distance };
    }
    // How far a square of `side`, or a child's cell, reaches from its
    // centre along `direction`.
    function squareReach(side, direction) {
        return side / 2 / Math.max(Math.abs(direction.x), Math.abs(direction.y), 1e-6);
    }
    // An arc of `radius` about `centre`, `span` radians wide about `direction`.
    function arcSamples(centre, direction, radius, span, steps) {
        const result = [];
        for (let index = 0; index <= steps; ++index)
            result.push(along(centre, turned(direction, -span / 2 + span * index / steps), radius));
        return result;
    }
    function allClear(points) {
        for (const point of points)
            if (!clear(point)) return false;
        return true;
    }

    // How long a path on the plane is as drawn, measured at scale 1 (see
    // trackRun()); 0 where part of it is out of sight.
    function drawnLength(points) {
        let total = 0;
        let previous = null;
        for (const point of points) {
            const shown = seen(point);
            if (!shown)
                return 0;
            if (previous)
                total += trackRun(previous, shown, true);
            previous = shown;
        }
        return total;
    }
    // How far out a shape is looked for at most.
    const largest = 640;
    // The least distance out, to 2 pixels, at which `fits(distance)` holds:
    // coarse steps, then halving between the last miss and the first fit.
    // `largest` when nothing up to it fits.
    function leastOut(fits) {
        if (fits(0))
            return 0;
        let miss = 0, hit = -1;
        for (let out = 16; out <= largest; out += 16) {
            if (fits(out)) {
                hit = out;
                break;
            }
            miss = out;
        }
        if (hit < 0)
            return largest;
        while (hit - miss > 2) {
            const middle = (hit + miss) / 2;
            if (fits(middle))
                hit = middle;
            else
                miss = middle;
        }
        return hit;
    }
    // The least size from `from` up to `limit`, to 2 pixels, at which
    // `fits(size)` holds, for a test that keeps holding once it does:
    // doubling steps, then halving between the last miss and the first fit.
    // `limit` when nothing up to it fits.
    function leastSize(from, limit, fits) {
        if (from >= limit || fits(from))
            return Math.min(from, limit);
        let miss = from, hit = -1;
        for (let step = 8; ; step *= 2) {
            const size = Math.min(limit, from + step);
            if (fits(size)) {
                hit = size;
                break;
            }
            miss = size;
            if (size >= limit)
                return limit;
        }
        while (hit - miss > 2) {
            const middle = (hit + miss) / 2;
            if (fits(middle))
                hit = middle;
            else
                miss = middle;
        }
        return hit;
    }
    // A ring's circles do not depend on which way it opens: worked out once
    // for each number of children it holds.
    const ringSizes = {};
    // A small ring at its smallest holds six children; a folder with more
    // has a second circle round it, a child's cell further out, for the
    // rest (only the ring does). Where the screen is short the first circle
    // holds four, then three, still inside a second one; then the ring does
    // without the second circle and holds six, four, then three; the others
    // come round with the wheel. A ring the size of the panel keeps its
    // radius and one circle.
    const ringSteps = [];
    if (ringLike && !panelSized) {
        const inner = Math.min(count, 6);
        if (kind === "ring" && count > inner)
            for (const shown of [inner, 4, 3])
                if (shown <= inner && !ringSteps.some(function(step) { return step.shown === shown; }))
                    ringSteps.push({ shown: shown, second: true });
        for (const shown of [inner, 4, 3]) {
            const held = Math.max(Math.min(count, 3), Math.min(inner, shown));
            if (!ringSteps.some(function(step) { return !step.second && step.shown === held; }))
                ringSteps.push({ shown: held, second: false });
        }
    }
    if (ringSteps.length === 0)
        ringSteps.push({ shown: count, second: false });

    // `fewer` children than it would show at once, where the screen is short.
    function build(direction, fewer) {
        const result = { shape: kind, direction: direction, closed: false, start: "centre",
                         pitch: curvePitch, capacity: 0, outline: null };
        if (kind === "fan") {
            const opening = clamp(finite(value.fanOpening, 90), 40, 160) * Math.PI / 180;
            const apex = fromEdge ? { x: folder.x, y: folder.y }
                : along(folder, direction, squareReach(size, direction) + 6);
            // A small panel at its smallest: three named children at once at
            // a right angle (four without names), more the wider it opens;
            // the rest come in with the wheel.
            const shown = Math.max(2, Math.min(count, clamp(Math.round(
                opening / (Math.PI / 2) * (named ? 3 : 4)), 2, 12)) - fewer);
            const hug = project && value.hug && typeof value.hug.rimAt === "function" ? value.hug : null;
            if (hug) {
                // On a plane the fan hugs the panel: its children stand on a
                // curve a little way out of the rim, bending as the rim does
                // round the folder, as long as holds them a pitch apart as
                // drawn and at most half way round. It moves out only as far
                // as keeps every child clear of the panel.
                const middle = finite(hug.turn, 0);
                const rimAt = function(turn) { return Math.max(0, finite(hug.rimAt(turn), 0)); };
                const pointAt = function(turn, out) {
                    const reach = rimAt(turn) + out;
                    return { x: reach * Math.cos(turn), y: reach * Math.sin(turn) };
                };
                // How far out of the rim a child keeps clear of the panel as
                // drawn, every 2 degrees up to half way round either side, a
                // little more where a neighbour needs more so the curve stays
                // smooth.
                const grid = 2 * Math.PI / 180, half = 45;
                const needed = [];
                for (let index = -half; index <= half; ++index)
                    needed.push(leastOut(function(distance) {
                        return clear(pointAt(middle + index * grid, distance));
                    }));
                const smooth = needed.map(function(value, index) {
                    let most = value;
                    for (let near = Math.max(0, index - 2); near <= Math.min(needed.length - 1, index + 2); ++near)
                        most = Math.max(most, needed[near]);
                    return most;
                });
                const outAt = function(turn) {
                    const at = clamp((turn - middle) / grid + half, 0, 2 * half);
                    const low = Math.floor(at), high = Math.min(2 * half, low + 1);
                    return smooth[low] + (smooth[high] - smooth[low]) * (at - low);
                };
                const curveAt = function(wide) {
                    const points = [];
                    for (let index = 0; index <= 48; ++index) {
                        const turn = middle - wide / 2 + wide * index / 48;
                        points.push(pointAt(turn, outAt(turn)));
                    }
                    return points;
                };
                const need = (shown - 1) * curvePitch;
                let low = 0, high = Math.PI;
                if (drawnLength(curveAt(high)) >= need - 0.5) {
                    for (let round = 0; round < 14; ++round) {
                        const wide = (low + high) / 2;
                        if (drawnLength(curveAt(wide)) >= need - 0.5)
                            high = wide;
                        else
                            low = wide;
                    }
                }
                const wide = high;
                const curve = curveAt(wide);
                const out = outAt(middle);
                result.samples = curve;
                result.out = out;
                result.outline = { closed: false, points: curve };
                result.radius = Math.max(0, finite(hug.rimAt(middle), 0)) + out;
                result.opening = wide * 180 / Math.PI;
                result.hugs = true;
            } else {
                // On a plane the fan is as small as its children need along the
                // arc, and its tip stays at the panel's rim: the whole fan moves
                // out only as far as keeps every child clear of the panel.
                let radius = project ? Math.max(1.2 * size, (shown - 1) * curvePitch / opening)
                    : Math.max(curvePitch, 1.2 * size, (shown - 1) * curvePitch / opening);
                // On a plane the children along the arc stand a pitch apart at
                // the size each is drawn there.
                const roomy = function(tip, around) {
                    return !project
                        || drawnLength(arcSamples(tip, direction, around, opening, 48)) >= (shown - 1) * curvePitch - 0.5;
                };
                if (project)
                    radius = leastSize(radius, largest, function(around) { return roomy(apex, around); });
                const out = project ? leastOut(function(distance) {
                    return allClear(arcSamples(along(apex, direction, distance), direction, radius, opening, 32));
                }) : 0;
                const apexAt = along(apex, direction, out);
                result.out = out;
                while (radius < largest && (!allClear(arcSamples(apexAt, direction, radius, opening, 32))
                                            || !roomy(apexAt, radius)))
                    radius += 4;
                const arc = arcSamples(apexAt, direction, radius, opening, 48);
                result.samples = arc;
                // The sector's two straight edges are only the faint fill's border;
                // the stroke follows the arc the children stand on.
                result.outline = { closed: true, points: [apexAt].concat(arc), strokeFrom: 1 };
                result.apex = apexAt;
                result.radius = radius;
                result.opening = opening * 180 / Math.PI;
            }
        } else if (kind === "arc") {
            const span = 150 * Math.PI / 180;
            // At its smallest: three children at once, the rest by the wheel.
            const shown = Math.max(2, Math.min(count, 3) - fewer);
            let radius = Math.max(1.6 * size, (shown - 1) * curvePitch / span);
            const roomy = function(centre, around) {
                return !project
                    || drawnLength(arcSamples(centre, direction, around, span, 48)) >= (shown - 1) * curvePitch - 0.5;
            };
            if (project)
                radius = leastSize(radius, largest, function(around) { return roomy(folder, around); });
            // On a tilted plane the arc moves out first, at its own size.
            const out = project ? leastOut(function(distance) {
                return allClear(arcSamples(along(folder, direction, distance), direction, radius, span, 32));
            }) : 0;
            const centreAt = along(folder, direction, out);
            result.out = out;
            while (radius < largest && (!allClear(arcSamples(centreAt, direction, radius, span, 32))
                                        || !roomy(centreAt, radius)))
                radius += 4;
            result.samples = arcSamples(centreAt, direction, radius, span, 48);
            result.centre = centreAt;
            result.radius = radius;
            result.span = 150;
        } else if (kind === "stack") {
            // Neighbours along the line just clear each other's cells.
            // On a plane the line runs any way across the screen: a name's
            // diagonal apart.
            const pitch = project ? curvePitch : Math.min(cellWidth / Math.max(Math.abs(direction.x), 1e-6),
                                   cellHeight / Math.max(Math.abs(direction.y), 1e-6)) + gap;
            // On a plane, the step along the line from `at` that keeps two
            // neighbours a pitch apart at the size each is drawn there.
            const stepFrom = function(at) {
                let step = pitch;
                for (let round = 0; project && round < 3; ++round) {
                    const drawn = drawnLength([along(folder, direction, at), along(folder, direction, at + step)]);
                    if (!(drawn > 1e-6))
                        break;
                    step *= pitch / drawn;
                }
                return step;
            };
            const length = clamp(Math.round(finite(value.stackLength, 3)), 2, 12);
            // Against a dock's side (`stackSpan`: the side's length, from
            // `folder` along `direction`), the slots are centred within it
            // and never outnumber what fits, so the stack is never longer
            // than the dock beside it; the rest of the children scroll in.
            const span = Math.max(0, finite(value.stackSpan, 0));
            // On a plane against the panel's side, the way out of the panel
            // there (`sideOut`): the line runs along the side and moves out
            // from it only as far as keeps every child clear.
            const sideOut = project && span > 0 && value.sideOut ? (function() {
                const x = finite(value.sideOut.x, 0), y = finite(value.sideOut.y, 0), l = Math.hypot(x, y);
                return l > 1e-6 ? { x: x / l, y: y / l } : null;
            })() : null;
            // The side, `out` from where it lies, finely.
            const sideAt = function(out) {
                const points = [];
                for (let index = 0; index <= 32; ++index)
                    points.push(along(along(folder, direction, span * index / 32), sideOut, out));
                return points;
            };
            const fitting = sideOut ? Math.max(2, Math.floor(drawnLength(sideAt(0)) / pitch + 0.01) + 1)
                : span > 0 ? Math.max(2, Math.floor(span / pitch) + 1) : length;
            const slots = Math.max(1, Math.min(count, length, fitting) - fewer);
            // The slots centred on the side as drawn, a pitch apart; a slot
            // more past the last for a stack of one, which the path needs.
            const sideSlots = function(out) {
                const points = sideAt(out);
                const runs = [0];
                for (let index = 1; index < points.length; ++index)
                    runs.push(runs[index - 1] + drawnLength([points[index - 1], points[index]]));
                const total = runs[runs.length - 1];
                const first = total / 2 - (slots - 1) * pitch / 2;
                const placed = [];
                for (let index = 0; index < Math.max(2, slots); ++index) {
                    const target = first + index * pitch;
                    let segment = 0;
                    while (segment < runs.length - 2 && runs[segment + 1] < target)
                        ++segment;
                    const a = points[segment], b = points[segment + 1];
                    const piece = runs[segment + 1] - runs[segment];
                    const t = piece > 1e-9 ? (target - runs[segment]) / piece : 0;
                    placed.push({ x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t });
                }
                return placed;
            };
            let distance = span > 0 ? Math.max(0, (span - (Math.max(2, slots) - 1) * pitch) / 2)
                : fromEdge ? 0 : squareReach(size, direction);
            const lineAt = function(from) {
                const points = [along(folder, direction, from)];
                let at = from;
                for (let index = 1; index < Math.max(2, slots); ++index) {
                    at += stepFrom(at);
                    points.push(along(folder, direction, at));
                }
                return points;
            };
            let line;
            if (sideOut) {
                const out = leastOut(function(distance) { return allClear(sideSlots(distance).slice(0, slots)); });
                line = sideSlots(out);
                distance = out;
            } else if (project && span <= 0) {
                // On a plane the whole line moves out only as far as keeps
                // every child clear of the panel.
                const from = distance;
                distance = from + leastOut(function(out) { return allClear(lineAt(from + out)); });
                line = lineAt(distance);
            } else {
                while (span <= 0 && distance < largest && !allClear([along(folder, direction, distance)]))
                    distance += 2;
                while (span <= 0 && distance < largest && !allClear(lineAt(distance)))
                    distance += 4;
                line = lineAt(distance);
            }
            result.samples = line;
            result.out = distance;
            result.start = "start";
            result.pitch = pitch;
            result.planePitch = Math.hypot(line[1].x - line[0].x, line[1].y - line[0].y);
            result.capacity = slots;
            result.stackLength = length;
            result.first = line[0];
        } else if (kind === "grid") {
            // At its smallest a grid holds two rows of three; the others come
            // in a row at a time with the wheel. Where the screen is short it
            // holds one row of three, then of two.
            const sizes = [{ columns: 3, rows: 2 }, { columns: 3, rows: 1 }, { columns: 2, rows: 1 }];
            const fitted = sizes[Math.min(fewer, sizes.length - 1)];
            const columns = Math.max(1, Math.min(fitted.columns, count));
            const rows = Math.max(1, Math.min(fitted.rows, Math.ceil(count / columns)));
            // Rows run out from the folder, its columns along the panel's rim.
            const across = turned(direction, Math.PI / 2);
            // The grid with its first row `from` out: neighbours in a row and
            // between rows a pitch apart, on a plane at the size each is drawn
            // there; with a row more on either side, where a row coming in or
            // going out passes.
            function gridAt(from) {
                let columnStep = curvePitch, rowStep = curvePitch;
                const place = function(row, column) {
                    return along(along(folder, direction, from + row * rowStep), across,
                                 (column - (columns - 1) / 2) * columnStep);
                };
                for (let round = 0; project && round < 3; ++round) {
                    let nearestAcross = Infinity, nearestOut = Infinity;
                    for (let row = 0; row < rows; ++row) {
                        for (let column = 0; column < columns; ++column) {
                            const here = place(row, column);
                            if (column + 1 < columns)
                                nearestAcross = Math.min(nearestAcross,
                                                         drawnLength([here, place(row, column + 1)]) || Infinity);
                            nearestOut = Math.min(nearestOut, drawnLength([here, place(row + 1, column)]) || Infinity);
                        }
                    }
                    if (isFinite(nearestAcross) && nearestAcross > 1e-6)
                        columnStep *= curvePitch / nearestAcross;
                    if (isFinite(nearestOut) && nearestOut > 1e-6)
                        rowStep *= curvePitch / nearestOut;
                }
                const cells = [];
                for (let row = -1; row <= rows; ++row) {
                    const line = [];
                    for (let column = 0; column < columns; ++column)
                        line.push(place(row, column));
                    cells.push(line);
                }
                return { cells: cells, columnStep: columnStep, rowStep: rowStep };
            }
            // The cells children rest in, row by row.
            const resting = function(grid) {
                const points = [];
                for (let row = 1; row <= rows; ++row)
                    for (const point of grid.cells[row])
                        points.push(point);
                return points;
            };
            // It moves out only as far as keeps every child clear of the panel.
            const from = fromEdge ? 0 : squareReach(size, direction) + gap;
            const out = leastOut(function(distance) { return allClear(resting(gridAt(from + distance))); });
            const grid = gridAt(from + out);
            result.out = out;
            result.samples = resting(grid);
            result.start = "start";
            result.pitch = curvePitch;
            result.capacity = columns * rows;
            // No border round it: the children alone.
            result.grid = { columns: columns, rows: rows, cells: grid.cells };
        } else if (kind === "mobius") {
            // The ∞ lies along the way out, its near loop's end a cell from
            // the folder. Each loop holds half the children, a slot's gap at
            // each of its ends where the loops cross; the band is just long
            // enough for that, on a plane as drawn there.
            const step = ringSteps[Math.min(fewer, ringSteps.length - 1)];
            const slots = step.shown + 2;
            const across = { x: -direction.y, y: direction.x };
            const placeLocal = function(centre, x, y) {
                return { x: centre.x + direction.x * x + across.x * y,
                         y: centre.y + direction.y * x + across.y * y };
            };
            // The middle line once, for a reach of 1; each try only moves
            // and sizes it.
            const unit = [];
            for (let index = 0; index < 192; ++index)
                unit.push(mobiusPoint(index / 192, 1));
            const band = function(centre, reach) {
                return unit.map(function(at) { return placeLocal(centre, at.x * reach, at.y * reach); });
            };
            // Every two children a pitch apart in a straight line - round a
            // loop's end that takes more than a pitch along the band - and
            // on a plane the band as long as drawn as they need, as a ring is.
            const bandFits = function(around) {
                const points = band(folder, around);
                const shown = lobedTrackLayout(points, step.shown, curvePitch, 0,
                                               { capacity: step.shown }).entries;
                for (let a = 0; a < shown.length; ++a)
                    for (let b = a + 1; b < shown.length; ++b)
                        if (Math.hypot(shown[a].x - shown[b].x, shown[a].y - shown[b].y) < curvePitch - 0.5)
                            return false;
                return !project || drawnLength(points.concat([points[0]])) >= slots * curvePitch - 0.5;
            };
            let reach = Math.max(1.2 * size, slots * curvePitch / mobiusLength());
            const known = ringSizes[fewer];
            reach = known ? known.radius : leastSize(reach, largest / 2, bandFits);
            ringSizes[fewer] = { radius: reach, outerRadius: 0 };
            // A child's cell is wider than six of the band's steps, so on a
            // plane, where each point is looked up in the view, every sixth
            // tells whether it keeps clear.
            const thinned = function(points) {
                return project ? points.filter(function(point, index) { return index % 6 === 0; }) : points;
            };
            let distance = squareReach(size, direction) + gap + reach;
            const nearest = distance;
            if (project) {
                const from = distance;
                distance = from + leastOut(function(out) {
                    return allClear(thinned(band(along(folder, direction, from + out), reach)));
                });
            } else {
                while (distance < largest + reach && !allClear(band(along(folder, direction, distance), reach)))
                    distance += 4;
            }
            result.out = distance - nearest;
            const centre = along(folder, direction, distance);
            result.samples = band(centre, reach);
            result.closed = true;
            // Children stand on each loop apart from where they cross.
            result.lobes = true;
            result.capacity = step.shown;
            // The band is drawn as one solid ribbon
            // between its two sides (`ribbon`), from the far end of its
            // second loop, so the strand passing over the crossing is drawn
            // over the other. Flat it is the ∞ itself. On a 3D panel's plane
            // it is a Möbius strip as one is seen in space: across its width
            // it turns half a turn out of the plane along its length, so its
            // two loops lean opposite ways. Where it passes over the crossing
            // it faces the viewer, seen at its widest; under the crossing it
            // is turned as near edge on as it can be seen; the half turn
            // between is shared by the two loops so both hold. It is one
            // colour, one shape, with nothing drawn between its parts. Its
            // outline is its two sides.
            // The band is a little more than half an icon wide, so its
            // loops stay open round their middles.
            const halfBand = 0.3 * size;
            const sides = function(at, twist) {
                const turn = at.tangent * Math.PI / 180;
                const across = halfBand * Math.cos(twist), upward = halfBand * Math.sin(twist);
                const raise = function(point, by) {
                    return project ? { x: point.x, y: point.y, height: by } : point;
                };
                return [raise(placeLocal(centre, at.x - Math.sin(turn) * across, at.y + Math.cos(turn) * across), upward),
                        raise(placeLocal(centre, at.x + Math.sin(turn) * across, at.y - Math.cos(turn) * across), -upward)];
            };
            // The turn across a strand at which it is seen widest or
            // narrowest.
            const seenTurn = function(progress, widest) {
                const at = mobiusPoint(progress, reach);
                let best = widest ? -1 : Infinity, chosen = 0;
                for (let step = 0; step < 36; ++step) {
                    const ends = sides(at, Math.PI * step / 36).map(seen);
                    if (!ends[0] || !ends[1])
                        continue;
                    const apart = Math.hypot(ends[0].x - ends[1].x, ends[0].y - ends[1].y);
                    if (widest ? apart > best : apart < best) {
                        best = apart;
                        chosen = Math.PI * step / 36;
                    }
                }
                return chosen;
            };
            const under = project ? seenTurn(0, false) : 0, over = project ? seenTurn(0.5, true) : 0;
            // From under to over round the first loop, over to under turned
            // half round on through the second.
            const firstTurn = ((over - under) % Math.PI + Math.PI) % Math.PI;
            const left = [], right = [];
            for (let index = 0; index < 192; ++index) {
                const at = mobiusPoint(index / 192, reach);
                const share = index / 192;
                const twist = !project ? 0
                    : share <= 0.5 ? under + firstTurn * share * 2
                    : over + (Math.PI - firstTurn) * (share - 0.5) * 2;
                const ends = sides(at, twist);
                left.push(ends[0]);
                right.push(ends[1]);
            }
            result.outline = { closed: true, points: left.concat(right), loops: [], evenOdd: true,
                               ribbon: { left: left, right: right } };
            result.band = true;
            result.centre = centre;
            result.radius = reach;
            result.ringSize = "small";
        } else {
            const panelRadius = Math.max(0, finite(value.panelRadius, 0));
            // Neighbours on a circle stand a chord apart, a little less than
            // the arc between them: the small ring is just wide enough for
            // that chord to be a pitch between all its children, or between
            // fewer of them where the screen is short.
            const step = ringSteps[Math.min(fewer, ringSteps.length - 1)];
            const fitting = Math.max(1.2 * size, step.shown >= 2
                ? curvePitch / (2 * Math.sin(Math.PI / step.shown)) : 0);
            let radius = Math.min(largest / 2, panelSized && panelRadius > 0
                ? Math.max(1.2 * size, panelRadius) : fitting);
            // A circle of `around` from its point nearest the folder, clockwise.
            function circle(centre, around) {
                const back = { x: -direction.x, y: -direction.y };
                const points = [];
                for (let index = 0; index < 96; ++index)
                    points.push(along(centre, turned(back, 2 * Math.PI * index / 96), around));
                return points;
            }
            // On a plane the two circles of a ring lean opposite ways.
            function leaning(points, centre, lean) {
                if (!project || !step.second)
                    return points;
                return points.map(function(point) {
                    return { x: point.x, y: point.y, lean: lean, about: centre };
                });
            }
            // On a plane the small ring is as wide as holds its children a
            // pitch apart at the size each is drawn there.
            const loopLength = function(around, lean) {
                const points = leaning(circle(folder, around), folder, lean);
                return drawnLength(points.concat([points[0]]));
            };
            const known = ringSizes[fewer];
            if (known) {
                radius = known.radius;
            } else if (project && !panelSized) {
                radius = leastSize(radius, largest / 2, function(around) {
                    return loopLength(around, -1) >= step.shown * curvePitch - 0.5;
                });
            }
            let outerRadius = known ? known.outerRadius
                : step.second ? radius + Math.max(cellWidth, cellHeight) + gap : 0;
            // The first circle stays inside the second with a clear gap all
            // round, as drawn: 16 pixels between the two circles' lines.
            // Leaning opposite ways would otherwise bring the ends of the
            // first circle against the second.
            function inside(point, polygon) {
                let within = false;
                for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
                    const a = polygon[i], b = polygon[j];
                    if ((a.y > point.y) !== (b.y > point.y)
                            && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
                        within = !within;
                }
                return within;
            }
            function gapTo(point, polygon) {
                let nearest = Infinity;
                for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i++) {
                    const a = polygon[j], b = polygon[i];
                    const dx = b.x - a.x, dy = b.y - a.y;
                    const t = clamp(((point.x - a.x) * dx + (point.y - a.y) * dy)
                                    / Math.max(1e-9, dx * dx + dy * dy), 0, 1);
                    nearest = Math.min(nearest, Math.hypot(a.x + dx * t - point.x, a.y + dy * t - point.y));
                }
                return nearest;
            }
            function nested(around) {
                const first = leaning(circle(folder, radius), folder, -1).map(seen);
                const second = leaning(circle(folder, around), folder, 1).map(seen);
                if (first.some(function(point) { return !point; })
                        || second.some(function(point) { return !point; }))
                    return true;
                return first.every(function(point) {
                    return inside(point, second) && gapTo(point, second) >= outlineWidth + 16;
                });
            }
            if (!known && project && step.second)
                outerRadius = leastSize(outerRadius, 4 * radius, nested);
            ringSizes[fewer] = { radius: radius, outerRadius: outerRadius };
            const reach = Math.max(radius, outerRadius);
            function rings(distance) {
                const centre = along(folder, direction, distance);
                return { centre: centre, points: leaning(circle(centre, radius), centre, -1),
                         outer: step.second ? leaning(circle(centre, outerRadius), centre, 1) : null };
            }
            // A child's cell is wider than a third of the circle's steps, so
            // every third point tells whether the circles keep clear.
            const thinned = function(points) {
                return project ? points.filter(function(point, index) { return index % 3 === 0; }) : points;
            };
            const clearRings = function(set) {
                return allClear(thinned(set.points)) && (!set.outer || allClear(thinned(set.outer)));
            };
            let distance = squareReach(size, direction) + gap + reach;
            const nearest = distance;
            if (project) {
                const from = distance;
                distance = from + leastOut(function(out) { return clearRings(rings(from + out)); });
            } else {
                while (distance < largest + reach && !clearRings(rings(distance)))
                    distance += 4;
            }
            // How far it moved out to keep clear.
            result.out = distance - nearest;
            const ring = rings(distance);
            result.samples = ring.points;
            result.closed = true;
            // As many children at once as keep a chord of a pitch apart.
            const chordFit = function(around) {
                return Math.max(1, Math.floor(Math.PI / Math.asin(Math.min(1, curvePitch / (2 * around))) + 1e-6));
            };
            result.capacity = project && !panelSized ? step.shown : chordFit(radius);
            result.outline = { closed: true, points: ring.points, loops: ring.outer ? [ring.outer] : [] };
            if (ring.outer) {
                result.rings = [ring.outer];
                // The second circle holds as many as fit there as drawn.
                result.ringCapacities = [result.capacity, project ? 0 : chordFit(outerRadius)];
                result.outerRadius = outerRadius;
            }
            result.centre = ring.centre;
            result.radius = radius;
            result.ringSize = panelSized && panelRadius > 0 ? "panel" : "small";
        }
        // The room it needs: every place a child passes through, a pitch past
        // an open path's ends where one fades, the outline and the folder.
        const places = result.samples.slice();
        for (const loop of result.rings || [])
            for (const point of loop)
                places.push(point);
        // On a plane a child fading out past an end is drawn where the
        // screen has room there, but takes no room from the shape at rest.
        const passing = [];
        if (result.grid) {
            const coming = result.grid.cells[0], going = result.grid.cells[result.grid.cells.length - 1];
            for (const point of coming.concat(going))
                (project ? passing : places).push(point);
        } else if (!result.closed) {
            const s = result.samples;
            const head = { x: s[0].x - s[1].x, y: s[0].y - s[1].y };
            const tail = { x: s[s.length - 1].x - s[s.length - 2].x, y: s[s.length - 1].y - s[s.length - 2].y };
            const headLength = Math.hypot(head.x, head.y) || 1, tailLength = Math.hypot(tail.x, tail.y) || 1;
            const past = result.planePitch || result.pitch;
            const ends = project ? passing : places;
            ends.push(along(s[0], { x: head.x / headLength, y: head.y / headLength }, past));
            ends.push(along(s[s.length - 1], { x: tail.x / tailLength, y: tail.y / tailLength }, past));
        }
        const rectangles = places.map(cell);
        const outline = result.outline ? result.outline.points.map(seen) : [];
        const loops = result.outline ? (result.outline.loops || []).map(function(loop) { return loop.map(seen); }) : [];
        for (const point of outline.concat(...loops))
            if (point)
                rectangles.push({ x: point.x - outlineMargin, y: point.y - outlineMargin,
                                  width: 2 * outlineMargin, height: 2 * outlineMargin });
        const folderSeen = seen(folder);
        if (folderSeen)
            rectangles.push({ x: folderSeen.x - size * folderSeen.scale / 2, y: folderSeen.y - size * folderSeen.scale / 2,
                              width: size * folderSeen.scale, height: size * folderSeen.scale });
        // A shape that reaches out of sight cannot be drawn.
        result.hidden = rectangles.some(function(rectangle) { return rectangle === null; })
            || outline.concat(...loops).some(function(point) { return point === null; });
        result.restBounds = unitedBounds(rectangles.filter(Boolean));
        result.bounds = unitedBounds(rectangles.filter(Boolean).concat(passing.map(cell).filter(Boolean)));
        if (project && !result.hidden) {
            result.projected = true;
            result.samples = result.samples.map(seen);
            if (result.outline) {
                const ribbon = result.outline.ribbon;
                result.outline = { closed: result.outline.closed, points: outline, loops: loops,
                                   strokeFrom: result.outline.strokeFrom, evenOdd: result.outline.evenOdd,
                                   ribbon: ribbon ? thickRibbon(ribbon.left.map(seen), ribbon.right.map(seen))
                                                  : undefined };
            }
            if (result.rings)
                result.rings = result.rings.map(function(loop) { return loop.map(seen); });
            if (result.grid)
                result.grid = { columns: result.grid.columns, rows: result.grid.rows,
                                cells: result.grid.cells.map(function(line) { return line.map(seen); }) };
            for (const key of ["apex", "centre", "first"])
                if (result[key])
                    result[key] = seen(result[key]);
        }
        return result;
    }

    // A twisting band seen edge on still shows its thickness: where its two
    // sides come closer than a quarter of an icon as drawn, they part that
    // far across the band's way there.
    function thickRibbon(left, right) {
        if (left.some(function(point) { return !point; }) || right.some(function(point) { return !point; }))
            return { left: left, right: right };
        const count = left.length;
        const middle = left.map(function(point, index) {
            return { x: (point.x + right[index].x) / 2, y: (point.y + right[index].y) / 2,
                     scale: (finite(point.scale, 1) + finite(right[index].scale, 1)) / 2 };
        });
        const thick = [], other = [];
        for (let index = 0; index < count; ++index) {
            const a = left[index], b = right[index], centre = middle[index];
            const least = size / 4 * centre.scale;
            const apart = Math.hypot(a.x - b.x, a.y - b.y);
            if (apart >= least) {
                thick.push(a);
                other.push(b);
                continue;
            }
            // Parted the way its sides already lie, so it widens smoothly;
            // across the band's way where they meet in one point.
            let nx = 0, ny = 0;
            if (apart > 1e-3) {
                nx = (a.x - b.x) / apart;
                ny = (a.y - b.y) / apart;
            } else {
                const before = middle[(index - 1 + count) % count], after = middle[(index + 1) % count];
                const dx = after.x - before.x, dy = after.y - before.y, length = Math.hypot(dx, dy) || 1;
                nx = -dy / length;
                ny = dx / length;
            }
            thick.push({ x: centre.x + nx * least / 2, y: centre.y + ny * least / 2, scale: a.scale });
            other.push({ x: centre.x - nx * least / 2, y: centre.y - ny * least / 2, scale: b.scale });
        }
        return { left: thick, right: other };
    }

    function placed(degrees, fewer) {
        const candidate = build(turned(outward, degrees * Math.PI / 180), fewer);
        candidate.turn = degrees;
        candidate.fewer = fewer;
        candidate.overflow = candidate.hidden ? Infinity : 0;
        if (screen && !candidate.hidden) {
            const b = candidate.restBounds;
            const inside = Math.max(0, Math.min(b.x + b.width, screen.x + screen.width) - Math.max(b.x, screen.x))
                * Math.max(0, Math.min(b.y + b.height, screen.y + screen.height) - Math.max(b.y, screen.y));
            candidate.overflow = b.width * b.height - inside;
        }
        // Every way tried, for whoever wants to see why a shape opened where
        // it did.
        if (typeof value.tried === "function")
            value.tried({ turn: degrees, fewer: fewer, hidden: candidate.hidden === true,
                          overflow: candidate.overflow, out: finite(candidate.out, 0),
                          radius: candidate.radius || 0, bounds: candidate.restBounds || null });
        return candidate;
    }
    // Along the outward direction where the screen has room: a fan, an arc
    // or a stack first holds fewer children at once, then the shape turns a
    // little more each time towards either side, and only last opens the
    // other way.
    const shrinks = [0];
    const fewest = kind === "fan" || kind === "arc" ? 4
        : kind === "stack" ? clamp(Math.round(finite(value.stackLength, 3)), 2, 12) - 2
        : kind === "grid" ? 2
        : ringSteps.length - 1;
    for (let fewer = 1; fewer <= fewest; ++fewer)
        shrinks.push(fewer);
    let best = null;
    // On a plane a shape never opens back across the panel: it stays on the
    // folder's side, turned at most a right angle, where it is drawn the
    // size its own place gives it.
    const follows = project && ((kind === "fan" && value.hug && typeof value.hug.rimAt === "function")
                                || (kind === "stack" && value.sideOut && finite(value.stackSpan, 0) > 0));
    // A fan hugging the rim, or a stack along the panel's side, follows the
    // panel and does not turn.
    const turns = follows ? [0]
        : [0, 15, -15, 30, -30, 45, -45, 60, -60, 75, -75, 90, -90].concat(project ? [] : [180]);
    // A ring with a second circle first turns towards a side where both
    // circles fit, then makes its first circle smaller; only then does it
    // do without the second circle, or open the other way.
    if (kind === "ring" && ringSteps[0].second) {
        for (let fewer = 0; fewer < ringSteps.length && ringSteps[fewer].second; ++fewer) {
            for (const degrees of turns.filter(function(turn) { return Math.abs(turn) <= 90; })) {
                const candidate = placed(degrees, fewer);
                if (candidate.overflow <= 0.5)
                    return candidate;
                if (!best || candidate.overflow < best.overflow - 0.5)
                    best = candidate;
            }
        }
    }
    // On a plane a fan, an arc or a stack holds the children its smallest
    // size shows at once, and stands as close to the panel as it can: of
    // every way it fits the screen, the one that moved out least (then the
    // one turned least). It holds fewer only where none fits.
    if (project && !ringLike) {
        for (const fewer of shrinks) {
            let closest = null;
            for (const degrees of turns) {
                const candidate = placed(degrees, fewer);
                if (!best || candidate.overflow < best.overflow - 0.5)
                    best = candidate;
                if (candidate.overflow > 0.5)
                    continue;
                const out = finite(candidate.out, 0);
                if (!closest || out < closest.out - 0.5)
                    closest = { candidate: candidate, out: out };
                // Turned no more than needed: nothing stands nearer than
                // right at the rim.
                if (out <= 0.5)
                    break;
            }
            if (closest)
                return closest.candidate;
        }
        return best;
    }
    for (const degrees of turns) {
        for (const fewer of shrinks) {
            const candidate = placed(degrees, fewer);
            if (candidate.overflow <= 0.5)
                return candidate;
            if (!best || candidate.overflow < best.overflow - 0.5)
                best = candidate;
        }
    }
    return best;
}

// Length of the half ellipse x = radiusX cos(t), y = radiusY sin(t) between
// two angles, by Simpson's rule. A circle has a constant integrand, so its
// result is exact.
function expansionPathLength(radiusX, radiusY, from, to) {
    const intervals = 8;
    const width = (to - from) / intervals;
    let total = 0;
    for (let step = 0; step <= intervals; ++step) {
        const angle = from + step * width;
        total += Math.hypot(radiusX * Math.sin(angle), radiusY * Math.cos(angle))
            * (step === 0 || step === intervals ? 1 : step % 2 ? 4 : 2);
    }
    return total * width / 3;
}

// The compact folder path inside the height it is really given. The radius
// that runs up and down the screen gives way first, so a short popup shows a
// half ellipse. Children are spread over exactly 180 degrees, as many as keep
// one pitch between neighbours along the path and one cell between its ends.
function expansionPath(geometry, viewportHeight) {
    const value = geometry || {};
    const count = Math.max(0, Math.floor(finite(value.count, 0)));
    const cellHeight = Math.max(0, finite(value.cellHeight, 0));
    const pitch = Math.max(1, finite(value.pathPitch, 1));
    const height = finite(viewportHeight, finite(value.height, 0));
    let radiusX = Math.max(0, finite(value.pathRadiusX, 0));
    let radiusY = Math.max(0, finite(value.pathRadiusY, 0));
    if (expansionVertical(value.side))
        radiusX = Math.max(0, Math.min(radiusX, height - cellHeight));
    else
        radiusY = Math.max(0, Math.min(radiusY, (height - cellHeight) / 2));
    let capacity = Math.min(1, count);
    if (2 * radiusY >= finite(value.pathClearance, 0) - 1e-9) {
        for (let slots = 2; slots <= count; ++slots) {
            const step = Math.PI / (slots - 1);
            let shortest = Infinity;
            for (let index = 0; index < slots - 1; ++index)
                shortest = Math.min(shortest, expansionPathLength(radiusX, radiusY,
                    -Math.PI / 2 + index * step, -Math.PI / 2 + (index + 1) * step));
            if (shortest < pitch * (1 - 1e-9))
                break;
            capacity = slots;
        }
    }
    return {
        radiusX: radiusX,
        radiusY: radiusY,
        capacity: capacity,
        windowed: count > capacity,
        maximumOffset: Math.max(0, count - capacity),
        step: capacity > 1 ? Math.PI / (capacity - 1) : 0,
        startAngle: -Math.PI / 2,
        viewHeight: height
    };
}

// Top-left of one child on the compact folder path when the folder has moved
// offset children along it. A child leaving either end keeps to the curve
// while it fades; it is on the path, and may be pressed, only between the ends.
function expansionPathPoint(geometry, path, index, offset) {
    const shape = path || {};
    const last = Math.max(0, finite(shape.capacity, 1) - 1);
    const slot = finite(index, 0) - finite(offset, 0);
    const beyond = Math.max(0, -slot, slot - last);
    const angle = finite(shape.startAngle, -Math.PI / 2)
        + clamp(slot, -1, last + 1) * finite(shape.step, 0);
    // Out from the near edge and across it, then into the popup's own axes.
    const out = finite(shape.radiusX, 0) * Math.cos(angle);
    const across = finite(shape.radiusY, 0) * (1 + Math.sin(angle));
    const value = geometry || {};
    const side = expansionSide(value.side);
    const cellWidth = finite(value.cellWidth, 0), cellHeight = finite(value.cellHeight, 0);
    let x = out, y = across;
    if (side === "left") {
        x = Math.max(0, finite(value.width, 0) - cellWidth) - out;
    } else if (side === "bottom") {
        x = across; y = out;
    } else if (side === "top") {
        x = across;
        y = Math.max(0, Math.min(finite(shape.viewHeight, Infinity), finite(value.height, 0))
                     - cellHeight) - out;
    }
    return {
        x: x,
        y: y,
        onPath: beyond < 1e-6,
        visibility: clamp(1 - beyond, 0, 1)
    };
}
