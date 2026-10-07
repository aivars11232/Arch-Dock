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
// Layouts whose entries stand on a curved track: closed shapes and the open
// paths of the sweep table. A folder on one can open along the dock.
function curvedLayout(layout) {
    const name = String(layout || "");
    return ["circular", "ring", "ellipse", "polygon", "triangle", "square",
            "pentagon", "hexagon", "octagon"].includes(name)
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
    const side = Math.max(1, Math.ceil(
        2 * (source.radius + source.iconSize / 2 + source.padding)));
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
    "semicircle", "fan", "spiral"
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

function starPoint(progress, points, radius) {
    const vertices = Math.max(3, points) * 2;
    const segmentPosition = progress * vertices;
    const segment = Math.floor(segmentPosition) % vertices;
    const fraction = segmentPosition - Math.floor(segmentPosition);
    function vertex(index) {
        const vertexRadius = index % 2 === 0 ? radius : radius * 0.46;
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
    const closedProgress = placement && track.closed ? placement.progress
        : safeIndex / safeCount;
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
        "pentagon", "hexagon", "octagon", "star"
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

    if (["circular", "ring", "ellipse", "polygon", "triangle", "square",
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

function themeStyle(appearance, customColor, iconSize) {
    const preset = appearance || "glass";
    const styles = {
        "glass": {
            stroke: "rgba(64, 91, 118, 0.78)",
            shadow: "rgba(0, 0, 0, 0.78)", width: 0.48, blur: 16
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
        "lime": {
            stroke: "#7dff58", shadow: "#7dff58", width: 0.30, blur: 22
        },
        "floating-glass": {
            stroke: "rgba(109, 181, 225, 0.58)",
            shadow: "rgba(67, 152, 218, 0.62)", width: 0.50, blur: 20
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
            width: 0.32, blur: 9
        },
        "plate": {
            stroke: "rgba(76, 89, 102, 0.36)",
            shadow: "rgba(0, 0, 0, 0.45)", width: 0.14, blur: 4
        },
        "pedestal": {
            stroke: "rgba(83, 105, 122, 0.32)",
            shadow: "rgba(0, 0, 0, 0.48)", width: 0.12, blur: 5
        }
    };
    const result = styles[preset] || styles.glass;
    return {
        stroke: customColor && customColor.length > 0
            ? customColor : result.stroke,
        shadow: result.shadow,
        lineWidth: Math.max(3, iconSize * result.width),
        blur: result.blur,
        trackVisible: preset !== "plate" && preset !== "pedestal"
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
    const requested = String(layout || "fan");
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

// A free panel's folder contents stand on a path (ADREP-TASK-003): "Along
// the dock" is the dock's own curve beside the folder, the other shapes come
// from folderShape(). `samples` trace the path on screen, in order, as
// { x, y, scale }. Neighbours keep `pitch` pixels apart along it.
//
// An open path holds as many slots as fit, at most `options.capacity`,
// centred on its middle sample - the folder faces it - or, with
// `options.start` "start", from its first sample. Fewer children than slots
// stand centred among them. `offset` moves every child along the path by
// that many slots (PD-10): the wheel and keys set it; it is never saved. The
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
function folderTrackLayout(samples, count, pitch, offset, options) {
    const settings = options || {};
    const closed = settings.closed === true;
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
        lengths.push(lengths[index - 1] + Math.hypot(points[index].x - points[index - 1].x,
                                                     points[index].y - points[index - 1].y));
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
    const middle = fromStart ? 0 : lengths[Math.floor((points.length - 1) / 2)];
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
//
// A shape opens along the folder's outward direction, `outward`, and turns
// away from it only as far as the screen requires. Its radius - a ring's
// distance - grows until no place a child passes through covers an icon of
// the dock (`obstacles`, centres of `obstacleSize` squares) or the folder.
// `cellWidth` and `cellHeight` are one child with its name, its icon
// `iconSize` square at the top; `outlineMargin` is how far the drawn outline
// reaches beyond its line.
function folderShape(shape, options) {
    const value = options || {};
    const kind = ["fan", "arc", "stack", "ring"].includes(shape) ? shape : "fan";
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
    const obstacles = [{ x: folder.x, y: folder.y }];
    for (const point of value.obstacles || [])
        if (point && isFinite(point.x) && isFinite(point.y))
            obstacles.push({ x: Number(point.x), y: Number(point.y) });
    const outlineMargin = Math.max(0, finite(value.outlineMargin, 0));
    // The screen, less the margin the window keeps round the shape.
    const inset = Math.max(0, finite(value.screenMargin, 0));
    const screen = value.screen && finite(value.screen.width, 0) > 2 * inset
        ? { x: finite(value.screen.x, 0) + inset, y: finite(value.screen.y, 0) + inset,
            width: finite(value.screen.width, 0) - 2 * inset,
            height: finite(value.screen.height, 0) - 2 * inset }
        : null;

    function cell(point) {
        return { x: point.x - cellWidth / 2, y: point.y - size / 2,
                 width: cellWidth, height: cellHeight };
    }
    // Whether a child standing at `point` keeps clear of every dock icon.
    function clear(point) {
        const child = cell(point);
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
    const largest = 640;
    // How many children a small ring holds at once: all of them, then three
    // quarters, half, a third and a quarter where the screen is short, never
    // fewer than three. A ring the size of the panel keeps its radius.
    const ringShown = [];
    if (kind === "ring" && value.ringSize !== "panel") {
        for (const share of [1, 0.75, 0.5, 1 / 3, 0.25]) {
            const shown = Math.max(Math.min(count, 3), Math.ceil(count * share));
            if (ringShown.indexOf(shown) < 0)
                ringShown.push(shown);
        }
    }
    if (ringShown.length === 0)
        ringShown.push(count);

    // `fewer` children than it would show at once, where the screen is short.
    function build(direction, fewer) {
        const result = { shape: kind, direction: direction, closed: false, start: "centre",
                         pitch: curvePitch, capacity: 0, outline: null };
        if (kind === "fan") {
            const opening = clamp(finite(value.fanOpening, 90), 40, 160) * Math.PI / 180;
            const apex = along(folder, direction, squareReach(size, direction) + 6);
            // A small panel: a few children at once, more the wider it opens.
            const shown = Math.max(2, Math.min(count, clamp(Math.round(
                opening / (Math.PI / 2) * (named ? 4 : 6)), 3, 12)) - fewer);
            let radius = Math.max(curvePitch, 1.2 * size, (shown - 1) * curvePitch / opening);
            while (radius < largest && !allClear(arcSamples(apex, direction, radius, opening, 32)))
                radius += 4;
            const arc = arcSamples(apex, direction, radius, opening, 48);
            result.samples = arc;
            result.outline = { closed: true, points: [apex].concat(arc) };
            result.apex = apex;
            result.radius = radius;
            result.opening = opening * 180 / Math.PI;
        } else if (kind === "arc") {
            const span = 150 * Math.PI / 180;
            const shown = Math.max(2, Math.min(count, named ? 5 : 7) - fewer);
            let radius = Math.max(1.6 * size, (shown - 1) * curvePitch / span);
            while (radius < largest && !allClear(arcSamples(folder, direction, radius, span, 32)))
                radius += 4;
            result.samples = arcSamples(folder, direction, radius, span, 48);
            result.centre = folder;
            result.radius = radius;
            result.span = 150;
        } else if (kind === "stack") {
            // Neighbours along the line just clear each other's cells.
            const pitch = Math.min(cellWidth / Math.max(Math.abs(direction.x), 1e-6),
                                   cellHeight / Math.max(Math.abs(direction.y), 1e-6)) + gap;
            const length = clamp(Math.round(finite(value.stackLength, 5)), 2, 12);
            const slots = Math.max(1, Math.min(count, length) - fewer);
            let distance = squareReach(size, direction);
            while (distance < largest && !allClear([along(folder, direction, distance)]))
                distance += 2;
            const line = [];
            for (let index = 0; index < Math.max(2, slots); ++index)
                line.push(along(folder, direction, distance + index * pitch));
            while (distance < largest && !allClear(line)) {
                distance += 4;
                for (let index = 0; index < line.length; ++index)
                    line[index] = along(folder, direction, distance + index * pitch);
            }
            result.samples = line;
            result.start = "start";
            result.pitch = pitch;
            result.capacity = slots;
            result.stackLength = length;
            result.first = line[0];
        } else {
            const panelRadius = Math.max(0, finite(value.panelRadius, 0));
            // Neighbours on a circle stand a chord apart, a little less than
            // the arc between them: the small ring is just wide enough for
            // that chord to be a pitch between all its children, or between
            // fewer of them where the screen is short.
            const shown = ringShown[Math.min(fewer, ringShown.length - 1)];
            const fitting = Math.max(1.2 * size, shown >= 2
                ? curvePitch / (2 * Math.sin(Math.PI / shown)) : 0);
            const radius = Math.min(largest / 2, value.ringSize === "panel" && panelRadius > 0
                ? Math.max(1.2 * size, panelRadius) : fitting);
            // The circle from its point nearest the folder, clockwise.
            function circle(distance) {
                const centre = along(folder, direction, distance);
                const back = { x: -direction.x, y: -direction.y };
                const points = [];
                for (let index = 0; index < 96; ++index)
                    points.push(along(centre, turned(back, 2 * Math.PI * index / 96), radius));
                return { centre: centre, points: points };
            }
            let distance = squareReach(size, direction) + gap + radius;
            let ring = circle(distance);
            while (distance < largest + radius && !allClear(ring.points)) {
                distance += 4;
                ring = circle(distance);
            }
            result.samples = ring.points;
            result.closed = true;
            // As many children at once as keep a chord of a pitch apart.
            result.capacity = Math.max(1, Math.floor(Math.PI / Math.asin(
                Math.min(1, curvePitch / (2 * radius))) + 1e-6));
            result.outline = { closed: true, points: ring.points };
            result.centre = ring.centre;
            result.radius = radius;
            result.ringSize = value.ringSize === "panel" && panelRadius > 0 ? "panel" : "small";
        }
        // The room it needs: every place a child passes through, a pitch past
        // an open path's ends where one fades, the outline and the folder.
        const places = result.samples.slice();
        if (!result.closed) {
            const s = result.samples;
            const head = { x: s[0].x - s[1].x, y: s[0].y - s[1].y };
            const tail = { x: s[s.length - 1].x - s[s.length - 2].x, y: s[s.length - 1].y - s[s.length - 2].y };
            const headLength = Math.hypot(head.x, head.y) || 1, tailLength = Math.hypot(tail.x, tail.y) || 1;
            places.push(along(s[0], { x: head.x / headLength, y: head.y / headLength }, result.pitch));
            places.push(along(s[s.length - 1], { x: tail.x / tailLength, y: tail.y / tailLength }, result.pitch));
        }
        const rectangles = places.map(cell);
        if (result.outline)
            for (const point of result.outline.points)
                rectangles.push({ x: point.x - outlineMargin, y: point.y - outlineMargin,
                                  width: 2 * outlineMargin, height: 2 * outlineMargin });
        rectangles.push({ x: folder.x - size / 2, y: folder.y - size / 2, width: size, height: size });
        result.bounds = unitedBounds(rectangles);
        return result;
    }

    function placed(degrees, fewer) {
        const candidate = build(turned(outward, degrees * Math.PI / 180), fewer);
        candidate.turn = degrees;
        candidate.fewer = fewer;
        candidate.overflow = 0;
        if (screen) {
            const b = candidate.bounds;
            const inside = Math.max(0, Math.min(b.x + b.width, screen.x + screen.width) - Math.max(b.x, screen.x))
                * Math.max(0, Math.min(b.y + b.height, screen.y + screen.height) - Math.max(b.y, screen.y));
            candidate.overflow = b.width * b.height - inside;
        }
        return candidate;
    }
    // Along the outward direction where the screen has room: a fan, an arc
    // or a stack first holds fewer children at once, then the shape turns a
    // little more each time towards either side, and only last opens the
    // other way.
    const shrinks = [0];
    const fewest = kind === "fan" || kind === "arc" ? 4
        : kind === "stack" ? clamp(Math.round(finite(value.stackLength, 5)), 2, 12) - 2
        : ringShown.length - 1;
    for (let fewer = 1; fewer <= fewest; ++fewer)
        shrinks.push(fewer);
    let best = null;
    for (const degrees of [0, 15, -15, 30, -30, 45, -45, 60, -60, 75, -75, 90, -90, 180]) {
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
