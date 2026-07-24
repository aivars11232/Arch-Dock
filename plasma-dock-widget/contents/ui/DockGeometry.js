.pragma library

function metrics(count, iconSize, spacing, scale, radius, padding) {
    const size = Math.max(16, iconSize * scale);
    const safeRadius = Math.max(size * 0.75, radius * scale);
    return {
        width: Math.ceil(safeRadius * 2 + size + padding * 2),
        height: Math.ceil(safeRadius * 2 + size + padding * 2),
        iconSize: size,
        radius: safeRadius,
        spacing: Math.max(0, spacing * scale),
        count: Math.max(1, count)
    };
}

function polygonPoint(progress, sides, radius) {
    const segmentPosition = progress * sides;
    const segment = Math.floor(segmentPosition) % sides;
    const fraction = segmentPosition - Math.floor(segmentPosition);
    const startAngle = -Math.PI / 2 + segment * Math.PI * 2 / sides;
    const endAngle = startAngle + Math.PI * 2 / sides;
    const sx = Math.cos(startAngle) * radius;
    const sy = Math.sin(startAngle) * radius;
    const ex = Math.cos(endAngle) * radius;
    const ey = Math.sin(endAngle) * radius;
    return { x: sx + (ex - sx) * fraction, y: sy + (ey - sy) * fraction };
}

function position(layout, index, count, geometry, angle, polygonSides) {
    const centerX = geometry.width / 2;
    const centerY = geometry.height / 2;
    const size = geometry.iconSize;
    const safeCount = Math.max(1, count);
    const progress = index / safeCount;
    let point;
    if (["polygon", "triangle", "square", "pentagon", "hexagon", "octagon", "star"].includes(layout)) {
        let sides = layout === "triangle" ? 3 : layout === "square" ? 4
            : layout === "pentagon" ? 5 : layout === "hexagon" ? 6
            : layout === "octagon" ? 8 : Math.max(3, polygonSides);
        if (layout === "star") {
            sides *= 2;
            point = polygonPoint(progress, sides, geometry.radius);
            if (index % 2)
                point = { x: point.x * 0.5, y: point.y * 0.5 };
        } else {
            point = polygonPoint(progress, sides, geometry.radius);
        }
    } else {
        const radians = -Math.PI / 2 + index * Math.PI * 2 / safeCount;
        const yScale = layout === "ellipse" ? 0.62 : 1;
        point = {
            x: Math.cos(radians) * geometry.radius,
            y: Math.sin(radians) * geometry.radius * yScale
        };
    }
    const radians = Number(angle || 0) * Math.PI / 180;
    const x = point.x * Math.cos(radians) - point.y * Math.sin(radians);
    const y = point.x * Math.sin(radians) + point.y * Math.cos(radians);
    return { x: centerX + x - size / 2, y: centerY + y - size / 2 };
}
