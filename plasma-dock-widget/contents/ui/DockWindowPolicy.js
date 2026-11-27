.pragma library

// Which window the dock is drawn in, and how a free panel's place is set.

// Stands and holograms reach past a panel, which would cut them. In a Plasma
// edge panel that is always shown the dock is drawn in Arch Dock's own
// see-through window over the panel; Plasma hides an auto-hiding panel through
// the compositor, unseen by its widgets, so such a panel draws its dock in
// itself. An Arch Dock bar never uses that window: the bar's own window
// reaches past the bar instead (barReachWanted).
function overlayWanted(inPlasmaPanel, standsOrHolograms, visibilityMode) {
    return Boolean(inPlasmaPanel) && Boolean(standsOrHolograms)
        && String(visibilityMode || "always") === "always";
}

// A bar's dock fills the bar under the bar's widgets, so a second window over
// it took the clicks of one or the other. Every bar with stands or holograms
// reaches past itself in its own window, made that much deeper, whether it
// hides at times or not.
function barReachWanted(inBar, standsOrHolograms) {
    return Boolean(inBar) && Boolean(standsOrHolograms);
}

// Where a free panel's widget goes in Plasma's widget area, for a place asked
// for on the screen: Plasma reports a widget's place on the screen, and its
// widget area starts below a top panel and beside a side one (origin, where
// the widget area's top left lies on the screen, wherever Plasma put it: on a
// right-to-left desktop it is not after a left panel). The widget is kept
// inside the area, as Plasma keeps its widgets out of its panels' room. A
// command from an older Arch Dock carries no space: its numbers were already
// in the widget area.
function freeTarget(requested, origin, space) {
    const x = Number(requested && requested.x) || 0;
    const y = Number(requested && requested.y) || 0;
    if (String(space || "") !== "screen")
        return { x: x, y: y };
    return { x: Math.max(0, x - (Number(origin && origin.x) || 0)),
             y: Math.max(0, y - (Number(origin && origin.y) || 0)) };
}
