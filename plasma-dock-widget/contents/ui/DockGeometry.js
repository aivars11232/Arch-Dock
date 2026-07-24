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
    return {
        x: start.x + (end.x - start.x) * fraction,
        y: start.y + (end.y - start.y) * fraction
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
    return {
        x: start.x + (end.x - start.x) * fraction,
        y: start.y + (end.y - start.y) * fraction
    };
}

function position(layout, index, count, geometry, angle, polygonSides,
                  pathOrientation) {
    const resolvedLayout = geometry.layout || normalizedLayout(layout, false);
    const safeCount = Math.max(1, count);
    const size = geometry.iconSize;
    const slot = size + geometry.spacing;
    const centerX = geometry.width / 2;
    const centerY = geometry.height / 2;
    const progress = safeCount === 1 ? 0.5 : index / (safeCount - 1);
    const closedProgress = index / safeCount;
    let x = geometry.padding;
    let y = geometry.padding;
    let rotation = 0;

    if (resolvedLayout === "horizontal") {
        x += index * slot;
    } else if (resolvedLayout === "vertical") {
        y += index * slot;
    } else if (resolvedLayout === "diagonal") {
        x += index * slot * 0.78;
        y += index * slot * 0.34;
    } else if (resolvedLayout === "circular" || resolvedLayout === "ring") {
        const radians = -Math.PI / 2 + closedProgress * Math.PI * 2;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius - size / 2;
    } else if (resolvedLayout === "ellipse") {
        const radians = -Math.PI / 2 + closedProgress * Math.PI * 2;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius * 0.62 - size / 2;
    } else if (resolvedLayout === "radial") {
        const radians = (-150 + progress * 300) * Math.PI / 180;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = centerY + Math.sin(radians) * geometry.radius - size / 2;
    } else if (["polygon", "triangle", "square", "pentagon", "hexagon",
                "octagon", "star"].includes(resolvedLayout)) {
        const sides = shapeSides(resolvedLayout, polygonSides);
        const point = resolvedLayout === "star"
            ? starPoint(closedProgress, sides, geometry.radius)
            : polygonPoint(closedProgress, sides, geometry.radius);
        x = centerX + point.x - size / 2;
        y = centerY + point.y - size / 2;
    } else if (resolvedLayout === "arc" || resolvedLayout === "semicircle"
               || resolvedLayout === "fan") {
        const sweep = resolvedLayout === "semicircle" ? 180
            : resolvedLayout === "fan" ? 116 : 130;
        const radians = (270 - sweep / 2 + progress * sweep) * Math.PI / 180;
        x = centerX + Math.cos(radians) * geometry.radius - size / 2;
        y = geometry.height - geometry.padding - size * 0.58
            + Math.sin(radians) * geometry.radius - size / 2;
        if (resolvedLayout === "fan")
            rotation = (-58 + progress * 116) * 0.18;
    } else if (resolvedLayout === "spiral") {
        const radians = -Math.PI / 2 + index * 1.25;
        const distance = geometry.radius * (0.26 + 0.74 * progress);
        x = centerX + Math.cos(radians) * distance - size / 2;
        y = centerY + Math.sin(radians) * distance - size / 2;
    } else if (resolvedLayout === "ribbon"
               || resolvedLayout === "horizontal-curve") {
        x += index * slot;
        y = centerY - size / 2
            + Math.sin(progress * Math.PI * 2) * size * 0.36;
        rotation = Math.cos(progress * Math.PI * 2) * 8;
    } else if (resolvedLayout === "vertical-curve") {
        x = centerX - size / 2
            + Math.sin(progress * Math.PI * 2) * size * 0.6;
        y = geometry.padding + progress
            * Math.max(0, geometry.height - geometry.padding * 2 - size);
    } else if (resolvedLayout === "grid" || resolvedLayout === "floating") {
        const columns = Math.ceil(safeCount / geometry.rows);
        const row = Math.floor(index / columns);
        const column = index % columns;
        x += column * slot;
        y += row * slot;
        if (resolvedLayout === "floating") {
            x += Math.sin((index + 1) * 1.71) * geometry.spacing * 0.42;
            y += Math.cos((index + 1) * 1.29) * geometry.spacing * 0.42;
            rotation = Math.sin((index + 1) * 1.37) * 5;
        }
    }

    const point = rotate(
        { x: x + size / 2, y: y + size / 2 },
        centerX, centerY, angle);
    return {
        x: point.x - size / 2,
        y: point.y - size / 2,
        rotation: rotation
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
            let x = geometry.padding + geometry.iconSize / 2
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
