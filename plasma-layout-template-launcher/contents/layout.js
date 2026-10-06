// Add Panel template "Arch Dock Launcher Panel": a native bottom panel
// holding the Arch Dock applet with pinned launchers.

const panel = new Panel;
panel.location = "bottom";
panel.height = Math.round(gridUnit * 2);
panel.alignment = "center";

const dock = panel.addWidget("org.archdock.dock");
dock.currentConfigGroup = ["General"];
dock.writeConfig("panelId", "template-" + panel.id);
dock.writeConfig("panelType", "launcher");
dock.reloadConfig();