.pragma library

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
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
    const safeCount = Math.max(1, count);
    const size = Math.max(16, iconSize * scale);
    const gap = Math.max(0, spacing * scale);
    const safePadding = Math.max(0, padding);
    const safeRadius = Math.max(size * 0.75, radius * scale);
    const safeRows = clamp(Math.round(rows), 1, 8);
    const slot = size + gap;
    const linearLength = safeCount * size + Math.max(0, safeCount - 1) * gap;
    let width = linearLength + safePadding * 2;
    let height = size + safePadding * 2;

    if (resolvedLayout === "vertical") {
        width = size + safePadding * 2;
        height = linearLength + safePadding * 2;
    } else if (resolvedLayout === "diagonal") {
        width = Math.max(size, safeCount * slot * 0.78) + safePadding * 2;
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

    const radians = Number(angle || 0) * Math.PI / 180;
    const cosine = Math.abs(Math.cos(radians));
    const sine = Math.abs(Math.sin(radians));
    return {
        width: Math.ceil(width * cosine + height * sine),
        height: Math.ceil(width * sine + height * cosine),
        iconSize: size,
        spacing: gap,
        radius: safeRadius,
        rows: safeRows,
        sides: shapeSides(resolvedLayout, polygonSides),
        padding: safePadding,
        layout: resolvedLayout
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

function entryGeometry(layout, index, count, geometry, angle, polygonSides,
                       pathOrientation, compatibilityProfile, edge) {
    const resolvedLayout = geometry.layout || normalizedLayout(layout, false);
    const safeCount = Math.max(1, count);
    const safeIndex = clamp(Math.round(Number(index || 0)), 0, safeCount - 1);
    const size = geometry.iconSize;
    const slot = size + geometry.spacing;
    const centerX = geometry.width / 2;
    const centerY = geometry.height / 2;
    const progress = safeCount === 1 ? 0.5 : safeIndex / (safeCount - 1);
    const closedProgress = safeIndex / safeCount;
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
        const sweep = resolvedLayout === "semicircle" ? 180
            : resolvedLayout === "fan" ? 116 : 130;
        const pathDegrees = resolvedLayout === "fan"
            ? -58 + progress * 116
            : 270 - sweep / 2 + progress * sweep;
        const radians = (resolvedLayout === "fan"
            ? pathDegrees - 90 : pathDegrees) * Math.PI / 180;
        const runtimeProfile = profile === "runtime";
        const verticalFactor = resolvedLayout === "fan"
            ? (runtimeProfile ? 0.48 : 0.58)
            : (runtimeProfile ? 0.64 : 0.58);
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = geometry.height - geometry.padding - size * verticalFactor
            + Math.sin(radians) * geometry.radius - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
        if (resolvedLayout === "fan")
            rotation = pathDegrees * 0.18;
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

    const safeAngle = Number(angle || 0);
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
        position3D: null,
        orientation3D: null
    };
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

function surface(layout, geometry, angle, polygonSides) {
    const resolvedLayout = geometry.layout || normalizedLayout(layout, false);
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
        const sweep = resolvedLayout === "semicircle" ? 180
            : resolvedLayout === "fan" ? 116
            : resolvedLayout === "radial" ? 300 : 130;
        const start = resolvedLayout === "radial" ? -150 : 270 - sweep / 2;
        for (let index = 0; index < samples; ++index) {
            const radians = (start + index / (samples - 1) * sweep)
                * Math.PI / 180;
            const yCenter = resolvedLayout === "radial"
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
    const safeCount = Math.max(1, count);
    const safeRadius = Math.max(iconSize * 1.2, radius);
    const safeRows = clamp(Math.round(rows), 1, 8);
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
            x: index * Math.max(4, spacing * 0.72),
            y: -index * Math.max(5, spacing * 0.92)
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
    if (layout === "circular" || layout === "radial") {
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
