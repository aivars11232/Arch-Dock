.pragma library

function normalizedLayout(layout, vertical) {
    if (layout === "adaptive")
        return vertical ? "vertical" : "horizontal";
    return layout;
}

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
}

function metrics(layout, count, iconSize, spacing, scale, radius, rows, padding, vertical, angle, polygonSides) {
    const resolvedLayout = normalizedLayout(layout, vertical);
    const safeCount = Math.max(1, count);
    const size = Math.max(16, iconSize * scale);
    const gap = Math.max(0, spacing * scale);
    const safePadding = Math.max(0, padding);
    const safeRadius = Math.max(size * 0.75, radius * scale);
    const safeRows = clamp(Math.round(rows), 1, 8);
    const safePolygonSides = clamp(Math.round(Number(polygonSides || 6)), 3, 12);
    const slot = size + gap;
    const linearLength = safeCount * size + Math.max(0, safeCount - 1) * gap;
    let width = linearLength + safePadding * 2;
    let height = size + safePadding * 2;

    if (resolvedLayout === "vertical") {
        width = size + safePadding * 2;
        height = linearLength + safePadding * 2;
    } else if (resolvedLayout === "diagonal") {
        width = Math.max(size, safeCount * slot * 0.78) + safePadding * 2;
        height = Math.max(size, size + Math.max(0, safeCount - 1) * slot * 0.34) + safePadding * 2;
    } else if (["circular", "ellipse", "ring", "radial", "polygon", "triangle",
                "square", "pentagon", "hexagon", "octagon", "star", "spiral",
                "vertical-curve"].includes(resolvedLayout)) {
        width = safeRadius * 2 + size + safePadding * 2;
        height = width;
    } else if (resolvedLayout === "arc" || resolvedLayout === "semicircle" ||
               resolvedLayout === "fan") {
        width = safeRadius * 2 + size + safePadding * 2;
        height = safeRadius + size * 1.7 + safePadding * 2;
    } else if (resolvedLayout === "ribbon" || resolvedLayout === "horizontal-curve") {
        width = linearLength + safePadding * 2;
        height = size * 2.15 + safePadding * 2;
    } else if (resolvedLayout === "grid" || resolvedLayout === "floating") {
        const columnCount = Math.ceil(safeCount / safeRows);
        width = columnCount * size + Math.max(0, columnCount - 1) * gap + safePadding * 2;
        height = safeRows * size + Math.max(0, safeRows - 1) * gap + safePadding * 2;
    }

    const safeAngle = Number(angle || 0) * Math.PI / 180;
    const cosine = Math.abs(Math.cos(safeAngle));
    const sine = Math.abs(Math.sin(safeAngle));
    const rotatedWidth = width * cosine + height * sine;
    const rotatedHeight = width * sine + height * cosine;
    return {
        width: Math.ceil(rotatedWidth),
        height: Math.ceil(rotatedHeight),
        iconSize: size,
        spacing: gap,
        radius: safeRadius,
        rows: safeRows,
        sides: safePolygonSides,
        padding: safePadding,
        layout: resolvedLayout
    };
}

function rotatePoint(point, centerX, centerY, degrees) {
    if (Math.abs(degrees) < 0.001)
        return point;
    const radians = degrees * Math.PI / 180;
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
    const segmentProgress = segmentPosition - Math.floor(segmentPosition);
    const startAngle = -Math.PI / 2 + segment * Math.PI * 2 / sides;
    const endAngle = startAngle + Math.PI * 2 / sides;
    const start = { x: Math.cos(startAngle) * radius, y: Math.sin(startAngle) * radius };
    const end = { x: Math.cos(endAngle) * radius, y: Math.sin(endAngle) * radius };
    return {
        x: start.x + (end.x - start.x) * segmentProgress,
        y: start.y + (end.y - start.y) * segmentProgress,
        tangent: Math.atan2(end.y - start.y, end.x - start.x) * 180 / Math.PI,
        radial: Math.atan2(start.y + (end.y - start.y) * segmentProgress,
                           start.x + (end.x - start.x) * segmentProgress) * 180 / Math.PI
    };
}

function shapeSides(layout, fallback) {
    if (layout === "triangle") return 3;
    if (layout === "square") return 4;
    if (layout === "pentagon") return 5;
    if (layout === "hexagon") return 6;
    if (layout === "octagon") return 8;
    return fallback;
}

function starPoint(progress, points, outerRadius) {
    const vertices = Math.max(3, points) * 2;
    const segmentPosition = progress * vertices;
    const segment = Math.floor(segmentPosition) % vertices;
    const fraction = segmentPosition - Math.floor(segmentPosition);
    function vertex(vertexIndex) {
        const vertexRadius = vertexIndex % 2 === 0 ? outerRadius : outerRadius * 0.46;
        const radians = -Math.PI / 2 + vertexIndex * Math.PI * 2 / vertices;
        return { x: Math.cos(radians) * vertexRadius, y: Math.sin(radians) * vertexRadius };
    }
    const start = vertex(segment);
    const end = vertex((segment + 1) % vertices);
    const x = start.x + (end.x - start.x) * fraction;
    const y = start.y + (end.y - start.y) * fraction;
    return {
        x: x, y: y,
        tangent: Math.atan2(end.y - start.y, end.x - start.x) * 180 / Math.PI,
        radial: Math.atan2(y, x) * 180 / Math.PI
    };
}

function position(layout, index, count, geometry, angle, polygonSides, pathOrientation) {
    const safeCount = Math.max(1, count);
    const size = geometry.iconSize;
    const slot = size + geometry.spacing;
    const centerX = geometry.width / 2;
    const centerY = geometry.height / 2;
    const progress = safeCount === 1 ? 0.5 : index / (safeCount - 1);
    const polygonProgress = index / safeCount;
    const safePolygonSides = shapeSides(geometry.layout,
        clamp(Math.round(Number(polygonSides || geometry.sides || 6)), 3, 12));
    const orientation = pathOrientation || "upright";
    let x = geometry.padding;
    let y = geometry.padding;
    let rotation = 0;
    let tangent = 0;
    let radial = 0;

    if (geometry.layout === "horizontal") {
        x += index * slot;
        tangent = 0;
    } else if (geometry.layout === "vertical") {
        y += index * slot;
        tangent = 90;
    } else if (geometry.layout === "diagonal") {
        x += index * slot * 0.78;
        y += index * slot * 0.34;
        tangent = Math.atan2(0.34, 0.78) * 180 / Math.PI;
    } else if (geometry.layout === "circular" || geometry.layout === "ring") {
        const radians = -Math.PI / 2 + index * (Math.PI * 2 / safeCount);
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
    } else if (geometry.layout === "ellipse") {
        const radians = -Math.PI / 2 + index * (Math.PI * 2 / safeCount);
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius * 0.62 - size / 2;
        radial = Math.atan2(y + size / 2 - centerY, x + size / 2 - centerX) * 180 / Math.PI;
        tangent = Math.atan2(Math.cos(radians) * geometry.radius * 0.62,
                             -Math.sin(radians) * geometry.radius) * 180 / Math.PI;
    } else if (geometry.layout === "radial") {
        const radians = (-150 + progress * 300) * Math.PI / 180;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
    } else if (["polygon", "triangle", "square", "pentagon", "hexagon",
                "octagon", "star"].includes(geometry.layout)) {
        const polygon = geometry.layout === "star"
            ? starPoint(polygonProgress, safePolygonSides, geometry.radius)
            : polygonPoint(polygonProgress, safePolygonSides, geometry.radius);
        x = centerX + polygon.x - size / 2;
        y = centerY + polygon.y - size / 2;
        tangent = polygon.tangent;
        radial = polygon.radial;
    } else if (geometry.layout === "arc" || geometry.layout === "semicircle") {
        const sweep = geometry.layout === "semicircle" ? 180 : 130;
        const radians = (270 - sweep / 2 + progress * sweep) * Math.PI / 180;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = geometry.height - geometry.padding - size * 0.64 + Math.sin(radians) * geometry.radius - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
    } else if (geometry.layout === "fan") {
        const fanDegrees = -58 + progress * 116;
        const radians = (fanDegrees - 90) * Math.PI / 180;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = geometry.height - geometry.padding - size * 0.48 + Math.sin(radians) * geometry.radius - size / 2;
        rotation = fanDegrees * 0.18;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
    } else if (geometry.layout === "spiral") {
        const radians = (-Math.PI / 2) + index * 1.25;
        const distance = geometry.radius * (0.26 + 0.74 * progress);
        x = centerX + Math.cos(radians) * distance - size / 2;
        y = centerY + Math.sin(radians) * distance - size / 2;
        radial = radians * 180 / Math.PI;
        tangent = radial + 90;
    } else if (geometry.layout === "ribbon" || geometry.layout === "horizontal-curve") {
        x += index * slot;
        y = centerY - size / 2 + Math.sin(progress * Math.PI * 2) * size * 0.36;
        rotation = Math.cos(progress * Math.PI * 2) * 8;
        tangent = Math.atan2(
            Math.cos(progress * Math.PI * 2) * size * 0.36 * Math.PI * 2,
            Math.max(slot, 1)) * 180 / Math.PI;
    } else if (geometry.layout === "vertical-curve") {
        x = centerX - size / 2 + Math.sin(progress * Math.PI * 2) * size * 0.6;
        y = geometry.padding + progress * Math.max(0, geometry.height - geometry.padding * 2 - size);
        tangent = 90 - Math.atan2(
            Math.cos(progress * Math.PI * 2) * size * 0.6 * Math.PI * 2,
            Math.max(slot, 1)) * 180 / Math.PI;
    } else if (geometry.layout === "grid") {
        const columnCount = Math.ceil(safeCount / geometry.rows);
        const row = Math.floor(index / columnCount);
        const column = index % columnCount;
        x += column * slot;
        y += row * slot;
    } else if (geometry.layout === "floating") {
        const columnCount = Math.ceil(Math.sqrt(safeCount));
        const row = Math.floor(index / columnCount);
        const column = index % columnCount;
        x += column * slot + Math.sin((index + 1) * 1.71) * geometry.spacing * 0.42;
        y += row * slot + Math.cos((index + 1) * 1.29) * geometry.spacing * 0.42;
        rotation = Math.sin((index + 1) * 1.37) * 5;
    }

    const safeAngle = Number(angle || 0);
    const point = rotatePoint({ x: x + size / 2, y: y + size / 2 }, centerX, centerY, safeAngle);
    if (orientation === "tangent")
        rotation = tangent + safeAngle;
    else if (orientation === "radial")
        rotation = radial + safeAngle;
    return {
        x: point.x - size / 2,
        y: point.y - size / 2,
        rotation: rotation + safeAngle * (orientation === "upright" &&
            (geometry.layout === "fan" || geometry.layout === "ribbon") ? 0.35 : 0)
    };
}

function nearestIndex(layout, count, geometry, angle, pointX, pointY, polygonSides, pathOrientation) {
    let nearest = 0;
    let nearestDistance = Number.MAX_VALUE;
    for (let index = 0; index < count; ++index) {
        const candidate = position(layout, index, count, geometry, angle, polygonSides, pathOrientation);
        const centerX = candidate.x + geometry.iconSize / 2;
        const centerY = candidate.y + geometry.iconSize / 2;
        const distance = Math.pow(centerX - pointX, 2) + Math.pow(centerY - pointY, 2);
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
    else if (anchor === "top-right" || anchor === "right" || anchor === "bottom-right")
        x = availableWidth;

    if (anchor === "top-left" || anchor === "top" || anchor === "top-right")
        y = 0;
    else if (anchor === "bottom-left" || anchor === "bottom" || anchor === "bottom-right")
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
        return { x: (index % columns - (columns - 1) / 2) * slot, y: (Math.floor(index / columns) - (safeRows - 1) / 2) * slot };
    }
    if (layout === "stack")
        return { x: index * Math.max(4, spacing * 0.72), y: -index * Math.max(5, spacing * 0.92) };
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
    if (layout === "arc" || layout === "fan" || layout === "elastic" || layout === "physics") {
        const degrees = -72 + progress * 144;
        const radians = (degrees - 90) * Math.PI / 180;
        const distance = layout === "fan" ? safeRadius * 0.82 : safeRadius;
        return { x: Math.cos(radians) * distance, y: Math.sin(radians) * distance + safeRadius * 0.35 };
    }
    return { x: (index - (safeCount - 1) / 2) * slot, y: 0 };
}
