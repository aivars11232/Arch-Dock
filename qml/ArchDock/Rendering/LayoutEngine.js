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
// between neighbours instead and shows a window of them, centred on the track.
// `offset` says which entry the window starts at. It is transient browsing
// state and is clamped here, so a caller never has to know the capacity first.
// A closed track is never windowed: it turns.
function trackPlacement(index, count, length, closed, iconSize, spacing,
                        spacingReference, offset) {
    const safeCount = Math.max(1, Math.round(finite(count, 0)));
    const safeIndex = clamp(Math.round(finite(index, 0)), 0, safeCount - 1);
    const size = Math.max(1, finite(iconSize, 1));
    const gap = Math.max(0, finite(spacing, 0));
    const reference = Math.max(0, finite(spacingReference, 0));
    const trackLength = Math.max(0, finite(length, 0));
    const spans = closed ? safeCount : safeCount - 1;
    const result = {
        progress: spans > 0 ? safeIndex / spans : 0.5,
        onTrack: true,
        visibility: 1,
        pitch: spans > 0 ? trackLength / spans : 0,
        windowed: false,
        capacity: safeCount,
        maximumOffset: 0
    };
    if (!closed && spans > 0 && trackLength > 0 && result.pitch < size) {
        const pitch = size + gap;
        const capacity = Math.max(1, Math.min(
            safeCount - 1, Math.floor(trackLength / pitch) + 1));
        const first = clamp(Math.round(finite(offset, 0)), 0, safeCount - capacity);
        const slot = safeIndex - first;
        const margin = (trackLength - (capacity - 1) * pitch) / 2;
        result.windowed = true;
        result.capacity = capacity;
        result.maximumOffset = safeCount - capacity;
        result.pitch = pitch;
        result.onTrack = slot >= 0 && slot < capacity;
        result.visibility = result.onTrack ? 1 : 0;
        // An entry outside the window waits at the end it will enter from.
        result.progress = clamp(
            (margin + clamp(slot, 0, capacity - 1) * pitch) / trackLength, 0, 1);
        return result;
    }
    const evenGap = result.pitch - size;
    if (spans <= 0 || reference <= 0 || gap >= reference || evenGap <= 0)
        return result;
    // Never tighter than the spacing itself, never wider than the even spread.
    const tightened = Math.min(evenGap, Math.max(gap, evenGap * gap / reference));
    const contraction = (size + tightened) / result.pitch;
    result.pitch = size + tightened;
    result.progress = 0.5 + (result.progress - 0.5) * contraction;
    return result;
}

// Whether an open-path layout currently shows a window of its entries, how
// many it holds and how far the window can move.
function pathWindow(layout, count, rawGeometry) {
    const geometry = safeGeometry(rawGeometry, layout);
    const track = curvedTrack(geometry.layout, geometry, geometry.sides);
    const placement = track
        ? trackPlacement(0, count, track.length, track.closed, geometry.iconSize,
                         geometry.spacing, geometry.spacingReference, 0)
        : null;
    return {
        windowed: placement ? placement.windowed : false,
        capacity: placement ? placement.capacity
                            : Math.max(0, Math.round(finite(count, 0))),
        maximumOffset: placement ? placement.maximumOffset : 0
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
        // Which entry an overcrowded open path starts its window at. Runtime
        // browsing state supplied by the scene; never a saved setting.
        browseOffset: Math.max(0, finite(source.browseOffset, 0)),
        rows: clamp(Math.round(finite(source.rows, 1)), 1, 8),
        sides: clamp(Math.round(finite(source.sides, 6)), 3, 12),
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
    // spacing regulates every one of them alike.
    const track = curvedTrack(resolvedLayout, geometry, polygonSides);
    const placement = track
        ? trackPlacement(safeIndex, safeCount, track.length, track.closed, size,
                         geometry.spacing, geometry.spacingReference,
                         geometry.browseOffset)
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
        const radians = -Math.PI / 2 + safeIndex * 1.25;
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
    if (profile !== "live") {
        if (orientation === "tangent")
            rotation = tangent + safeAngle;
        else if (orientation === "radial")
            rotation = orientationRadial + safeAngle;
        else if (profile === "runtime"
                 && (resolvedLayout === "fan" || resolvedLayout === "ribbon"))
            rotation += safeAngle * 0.35;
    }
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
// every scaled icon, so nothing the theme positions is cut off.
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
        browseOffset: Math.max(0, finite(regulation.browseOffset, 0)),
        trackLength: 0,
        windowed: false,
        capacity: safeCount,
        maximumOffset: 0,
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
    metrics.maximumOffset = window.maximumOffset;

    let left = platform.x;
    let top = platform.y;
    let right = platform.x + platform.width;
    let bottom = platform.y + platform.height;
    // A turning ring puts icons at angles no resting entry occupies, so the
    // box is measured around the whole closed path instead of the entries the
    // panel happens to hold. Otherwise the host would be asked to resize as
    // the scene rotated, which is exactly what the envelope exists to avoid.
    const wholePath = rotating && metrics.closed && safeCount > 0;
    const samples = wholePath ? Math.max(safeCount, 72) : safeCount;
    for (let index = 0; index < samples; ++index) {
        const point = wholePath
            ? trackPointAt(metrics, index / samples, 0)
            : trackPoint(metrics, index, samples, 0);
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
// numbers the entries will use.
function trackPoint(metrics, index, count, rotationDegrees) {
    const placement = trackPlacement(
        index, count, metrics.trackLength, metrics.closed, metrics.iconSize,
        metrics.spacing, metrics.spacingReference, metrics.browseOffset);
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
                            pathOrientation, tiltDegrees) {
    const resolved = metrics && metrics.center
        ? metrics
        : trackMetrics(track, 0, 0, count, 0, 0, 0, tiltDegrees);
    const safeCount = Math.max(1, Math.round(finite(count, 0)));
    const point = trackPoint(resolved, index, safeCount, rotationDegrees);
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
        for (let index = 0; index < samples; ++index) {
            const progress = index / (samples - 1);
            const radians = -Math.PI / 2 + progress * Math.PI * 4.5;
            const distance = radius * (0.16 + progress * 0.84);
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

// "Along the dock": a folder's contents stand on a curve beside the dock,
// centred on the folder. `samples` trace that curve on screen, in order, as
// { x, y, scale }; the folder faces the middle sample. Neighbours keep `pitch`
// pixels apart along the curve. When they do not all fit, `offset` children
// have moved along it, and a child beyond either end fades out, as on an
// overcrowded dock; it is on the track, and may be pressed, only between the
// ends.
function folderTrackLayout(samples, count, pitch, offset) {
    // A list read back from a QML property is indexable but not an Array.
    const points = [];
    const source = samples || [];
    for (let index = 0; index < Math.floor(finite(source.length, 0)); ++index) {
        const sample = source[index];
        if (sample && isFinite(sample.x) && isFinite(sample.y))
            points.push(sample);
    }
    const total = clamp(Math.floor(finite(count, 0)), 0, 48);
    const empty = { capacity: 0, windowed: false, maximumOffset: 0, entries: [] };
    if (points.length < 2 || total === 0)
        return empty;
    const lengths = [0];
    for (let index = 1; index < points.length; ++index)
        lengths.push(lengths[index - 1] + Math.hypot(points[index].x - points[index - 1].x,
                                                     points[index].y - points[index - 1].y));
    const length = lengths[lengths.length - 1];
    const middle = lengths[Math.floor((points.length - 1) / 2)];
    const spacing = Math.max(1, finite(pitch, 1));
    const reach = Math.min(middle, length - middle);
    const capacity = Math.max(1, Math.min(total, Math.floor(2 * reach / spacing) + 1));
    const maximumOffset = Math.max(0, total - capacity);
    const shift = clamp(finite(offset, 0), 0, maximumOffset);
    const first = middle - (capacity - 1) * spacing / 2;
    function pointAt(position) {
        const target = clamp(position, 0, length);
        let segment = 0;
        while (segment < lengths.length - 2 && lengths[segment + 1] < target)
            ++segment;
        const span = lengths[segment + 1] - lengths[segment];
        const t = span > 0 ? (target - lengths[segment]) / span : 0;
        const a = points[segment], b = points[segment + 1];
        const scaleA = finite(a.scale, 1), scaleB = finite(b.scale, 1);
        return { x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t,
                 scale: scaleA + (scaleB - scaleA) * t };
    }
    const entries = [];
    for (let index = 0; index < total; ++index) {
        const slot = index - shift;
        const beyond = Math.max(0, -slot, slot - (capacity - 1));
        const point = pointAt(first + clamp(slot, -1, capacity) * spacing);
        entries.push({ index: index, x: point.x, y: point.y, scale: point.scale,
                       onTrack: beyond < 1e-6, visibility: clamp(1 - beyond, 0, 1) });
    }
    return { capacity: capacity, windowed: total > capacity,
             maximumOffset: maximumOffset, entries: entries };
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
