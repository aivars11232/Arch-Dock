// Plasma exposes only native containment templates in its Add Panel menu.
// This tiny containment is an authenticated bridge: Arch Dock creates the
// requested free surface and immediately removes this temporary panel.
const panel = new Panel;
panel.location = "bottom";
panel.height = Math.round(gridUnit);
panel.lengthMode = "fit";

const token = "archdock-free-template-" + panel.id;
const bridge = panel.addWidget("org.archdock.control");
bridge.currentConfigGroup = ["General"];
bridge.writeConfig("bootstrapAction", "create-circular-free-panel");
bridge.writeConfig("bootstrapToken", token);
bridge.writeConfig("bootstrapPanelId", panel.id);
bridge.reloadConfig();
