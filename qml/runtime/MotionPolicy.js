.pragma library

function clamp(value, minimum, maximum) {
    return Math.max(minimum, Math.min(maximum, value));
}

function transitionDuration(duration, reducedMotion) {
    if (reducedMotion)
        return 0;
    return Math.round(clamp(Number(duration || 0), 80, 1200));
}

function iconDuration(baseDuration, speed, reducedMotion) {
    if (reducedMotion)
        return 0;
    const safeSpeed = clamp(Number(speed || 1), 0.2, 3);
    return Math.round(clamp(Number(baseDuration || 170) / safeSpeed, 80, 1200));
}

function shouldRunContinuous(trigger, hovered, running, dropActive, revealed, reducedMotion) {
    if (reducedMotion)
        return false;
    if (trigger === "hover")
        return hovered;
    if (trigger === "running")
        return running;
    if (trigger === "drop")
        return dropActive;
    if (trigger === "reveal")
        return revealed;
    return trigger === "idle";
}