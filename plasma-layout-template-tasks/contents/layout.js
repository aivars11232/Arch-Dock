// Add Panel template "Arch Dock Tasks Panel": a native bottom panel holding
// the Arch Dock applet with running tasks.

const panel = new Panel;
panel.location = "bottom";
panel.height = Math.round(gridUnit * 2);
panel.alignment = "center";

const dock = panel.addWidget("org.archdock.dock");
dock.currentConfigGroup = ["General"];
dock.writeConfig("panelId", "template-" + panel.id);
dock.writeConfig("panelType", "tasks");
dock.reloadConfig();