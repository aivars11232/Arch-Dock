// KDE's Add Panel menu accepts panel-containment templates only. A short-lived
// native panel securely transfers its screen to Arch Dock; the service creates
// the real desktop widget there and removes this bridge after verification.
const bridgePanel = new Panel;
bridgePanel.height = Math.max(1, Math.round(gridUnit));
bridgePanel.lengthMode = "fit";

const token = "archdock-free-template-" + bridgePanel.id
    + ":" + currentActivity()
    + ":" + Date.now().toString(36)
    + ":" + Math.random().toString(36).slice(2);
const bridge = bridgePanel.addWidget("org.archdock.control");
bridge.currentConfigGroup = ["General"];
bridge.writeConfig("bootstrapAction", "create-circular-free-panel");
bridge.writeConfig("bootstrapToken", token);
bridge.writeConfig("bootstrapPanelId", bridgePanel.id);
bridge.reloadConfig();
