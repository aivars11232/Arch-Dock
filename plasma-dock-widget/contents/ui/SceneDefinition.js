.pragma library

// The panel definition an applet hands to its PanelScene. The applet and the
// Studio truth matrix test (ADREP-TASK-001) both build it here, so what the
// test draws is what a panel draws.
//
// host: { freeSurface, vertical, effectiveRendererTier, nativeScenePadding,
//         baseCellSize }
function build(configuration, host) {
    const definition = {};
    const source = configuration || {};
    for (const key of Object.keys(source)) {
        if (key !== "capabilityResolution")
            definition[key] = source[key];
    }
    definition.edge = host.freeSurface ? "free" : host.vertical ? "left" : "bottom";
    definition.rendererTier = String(
        source.rendererTier || host.effectiveRendererTier || "procedural2d");
    if (host.freeSurface) {
        definition.layout = source.layout || "circular";
    } else {
        // An edge panel lays its row out along the edge it is on, at its own
        // icon cell size, scale and padding, whatever the record holds.
        definition.layout = host.vertical ? "vertical" : "horizontal";
        definition.layoutScale = 1;
        definition.layoutAngle = 0;
        definition.layoutPadding = host.nativeScenePadding;
        definition.iconSize = host.baseCellSize;
    }
    return definition;
}
