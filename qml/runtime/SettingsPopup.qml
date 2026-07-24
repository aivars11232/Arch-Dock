import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import "StudioDraft.js" as StudioDraft
import "StudioNavigation.js" as StudioNavigation

Window {
    id: root

    property var settings: dockSettings
    property string selectedPanelId: panelRegistry.activePanelId
    property bool showPanels: false
    property int mainTabIndex: 0
    property int subTabIndex: 0
    property var subTabMemory: [0, 0, 0, 0, 0]
    property var panelDrafts: ({})
    property var settingsDraft: ({})
    property var screenDrafts: ({})
    property var themeDrafts: ({})
    property string themeTargetPanelId: ""
    property var colorField: null
    property string colorTargetPanelId: ""
    property string studioError: ""

    readonly property int panelRevision: panelRegistry.revision
    readonly property int screenRevision: panelController.screenRevision
    readonly property bool hasPendingChanges: StudioDraft.isSessionDirty(
        panelDrafts, settingsDraft, screenDrafts, themeDrafts)
    readonly property var editablePanelKeys: [
        "visible",
        "edge",
        "alignment",
        "visibilityMode",
        "dynamic",
        "width",
        "height",
        "type",
        "appearance",
        "shape",
        "iconShape",
        "iconSize",
        "spacing",
        "layout",
        "layoutScale",
        "layoutAngle",
        "layoutRadius",
        "layoutRows",
        "layoutPadding",
        "pathSides",
        "pathOrientation",
        "pathAnchor",
        "iconAnimation",
        "animationTrigger",
        "animationSpeed",
        "animationIntensity",
        "physicsEnabled",
        "folderLayout",
        "folderSpeed",
        "opacity",
        "color",
        "themeFit"
    ]
    readonly property var screenOptions: {
        const revision = screenRevision;
        return panelController.availableScreens();
    }
    readonly property var currentSubtabs: StudioNavigation.subtabsFor(mainTabIndex)
    readonly property var mainTabLabels: StudioNavigation.mainLabels()
    readonly property var mainTabIcons: [
        "view-dashboard",
        "preferences-desktop-display",
        "preferences-desktop-icons",
        "draw-rectangle",
        "document-save"
    ]
    readonly property var visibilityOptions: [
        option(qsTr("Always visible"), "always"),
        option(qsTr("Auto-hide"), "auto-hide"),
        option(qsTr("Dodge active window"), "dodge"),
        option(qsTr("Hide under active window"), "cover")
    ]
    readonly property var panelTypeOptions: [
        option(qsTr("Empty"), "empty"),
        option(qsTr("Launchers"), "launcher"),
        option(qsTr("Tasks"), "tasks"),
        option(qsTr("Hybrid"), "hybrid")
    ]
    readonly property var edgeOptions: [
        option(qsTr("Top"), "top"),
        option(qsTr("Bottom"), "bottom"),
        option(qsTr("Left"), "left"),
        option(qsTr("Right"), "right"),
        option(qsTr("Free"), "free")
    ]
    readonly property var nativeEdgeOptions: [
        option(qsTr("Top"), "top"),
        option(qsTr("Bottom"), "bottom"),
        option(qsTr("Left"), "left"),
        option(qsTr("Right"), "right")
    ]
    readonly property var alignmentOptions: [
        option(qsTr("Start"), "start"),
        option(qsTr("Center"), "center"),
        option(qsTr("End"), "end")
    ]
    readonly property var panelShapeOptions: choices(["pill", "rounded", "hexagon"])
    readonly property var iconShapeOptions: choices(["circle", "rounded", "hexagon"])
    readonly property var pathAnchorOptions: [
        option(qsTr("Top left"), "top-left"),
        option(qsTr("Top"), "top"),
        option(qsTr("Top right"), "top-right"),
        option(qsTr("Left"), "left"),
        option(qsTr("Center"), "center"),
        option(qsTr("Right"), "right"),
        option(qsTr("Bottom left"), "bottom-left"),
        option(qsTr("Bottom"), "bottom"),
        option(qsTr("Bottom right"), "bottom-right")
    ]
    readonly property var pathOrientationOptions: [
        option(qsTr("Keep icons upright"), "upright"),
        option(qsTr("Follow path tangent"), "tangent"),
        option(qsTr("Point away from center"), "radial")
    ]
    readonly property var layoutOptions: [
        option(qsTr("Adaptive row / column"), "adaptive"),
        option(qsTr("Horizontal"), "horizontal"),
        option(qsTr("Vertical"), "vertical"),
        option(qsTr("Diagonal"), "diagonal"),
        option(qsTr("Circular"), "circular"),
        option(qsTr("Ellipse"), "ellipse"),
        option(qsTr("Ring"), "ring"),
        option(qsTr("Radial"), "radial"),
        option(qsTr("Arc"), "arc"),
        option(qsTr("Semicircle"), "semicircle"),
        option(qsTr("Fan"), "fan"),
        option(qsTr("Spiral"), "spiral"),
        option(qsTr("Ribbon"), "ribbon"),
        option(qsTr("Horizontal curve"), "horizontal-curve"),
        option(qsTr("Vertical curve"), "vertical-curve"),
        option(qsTr("Polygon path"), "polygon"),
        option(qsTr("Triangle"), "triangle"),
        option(qsTr("Square"), "square"),
        option(qsTr("Pentagon"), "pentagon"),
        option(qsTr("Hexagon"), "hexagon"),
        option(qsTr("Octagon"), "octagon"),
        option(qsTr("Star"), "star"),
        option(qsTr("Multi-row grid"), "grid"),
        option(qsTr("Floating cluster"), "floating")
    ]
    readonly property var materialOptions: [
        option(qsTr("Glass"), "glass"),
        option(qsTr("Crystal"), "crystal"),
        option(qsTr("Neon"), "neon"),
        option(qsTr("Minimal"), "minimal"),
        option(qsTr("Plasma"), "plasma"),
        option(qsTr("Lime"), "lime"),
        option(qsTr("Floating glass"), "floating-glass"),
        option(qsTr("Metallic"), "metallic"),
        option(qsTr("Futuristic"), "futuristic"),
        option(qsTr("Organic"), "organic"),
        option(qsTr("Platform bases"), "platform"),
        option(qsTr("Individual plates"), "plate"),
        option(qsTr("Pedestals"), "pedestal")
    ]
    readonly property var motionOptions: [
        option(qsTr("None"), "none"),
        option(qsTr("Bounce"), "bounce"),
        option(qsTr("Elastic bounce"), "elastic"),
        option(qsTr("Pulse"), "pulse"),
        option(qsTr("Scale"), "scale"),
        option(qsTr("Spin"), "spin"),
        option(qsTr("Slow rotation"), "idle-rotate"),
        option(qsTr("Orbit"), "orbit"),
        option(qsTr("Swing"), "swing"),
        option(qsTr("Wobble"), "wobble"),
        option(qsTr("Wiggle"), "wiggle"),
        option(qsTr("Shake"), "shake"),
        option(qsTr("Glow"), "glow"),
        option(qsTr("Breathing"), "breathe"),
        option(qsTr("Floating"), "float"),
        option(qsTr("Hover wave"), "wave"),
        option(qsTr("Ripple"), "ripple"),
        option(qsTr("Magnetic"), "magnetic"),
        option(qsTr("Spring"), "spring")
    ]
    readonly property var triggerOptions: [
        option(qsTr("Hover"), "hover"),
        option(qsTr("Click"), "click"),
        option(qsTr("Launch"), "launch"),
        option(qsTr("Running"), "running"),
        option(qsTr("Drag and drop"), "drop"),
        option(qsTr("Reveal"), "reveal"),
        option(qsTr("Always on"), "idle")
    ]
    readonly property var folderLayoutOptions: choices([
        "fan", "grid", "stack", "arc", "spiral", "circular", "radial",
        "vertical", "horizontal", "elastic", "physics"
    ])
    function option(label, value) {
        return { label: label, value: value };
    }

    function titleCase(value) {
        const text = String(value).replace(/-/g, " ");
        return text.length === 0
            ? text
            : text.charAt(0).toUpperCase() + text.slice(1);
    }

    function choices(values) {
        return values.map(function(value) {
            return option(titleCase(value), value);
        });
    }

    function panelValueFor(panelId, key, fallback) {
        const revision = panelRevision;
        const candidate = panelRegistry.panelValue(panelId, key);
        return candidate === undefined || candidate === null ? fallback : candidate;
    }

    function panelValue(key, fallback) {
        return panelValueFor(selectedPanelId, key, fallback);
    }

    function panelDraft(panelId) {
        return StudioDraft.nestedMap(panelDrafts, panelId);
    }

    function effectivePanelValueFor(panelId, key, fallback) {
        return StudioDraft.value(
            panelDraft(panelId),
            key,
            panelValueFor(panelId, key, fallback));
    }

    function effectivePanelValue(key, fallback) {
        return effectivePanelValueFor(selectedPanelId, key, fallback);
    }

    function panelScreenIndex(panelId) {
        const revision = panelRevision;
        const displays = screenRevision;
        return panelController.screenIndexForPanel(panelId);
    }

    function effectivePanelScreenIndex(panelId) {
        const draft = StudioDraft.nestedMap(screenDrafts, panelId);
        if (StudioDraft.keyCount(draft) === 0)
            return panelScreenIndex(panelId);

        const stableId = String(draft.id || "");
        if (stableId.length > 0) {
            for (let index = 0; index < screenOptions.length; ++index) {
                if (String(screenOptions[index].id || "") === stableId)
                    return index;
            }
        }
        return Number(draft.index);
    }

    function optionIndex(options, value) {
        for (let index = 0; index < options.length; ++index) {
            if (options[index].value === value)
                return index;
        }
        return 0;
    }

    function selectPanel(panelId) {
        if (!panelId || panelId.length === 0)
            return;
        selectedPanelId = panelId;
    }

    function openPanelEditor(panelId) {
        selectPanel(panelId);
        setMainTab(1);
        setSubTab(0);
    }

    function setMainTab(index) {
        const next = StudioNavigation.clampSectionIndex(index);
        mainTabIndex = next;
        subTabIndex = StudioNavigation.clampSubtabIndex(
            next,
            subTabMemory[next] || 0);
    }

    function setSubTab(index) {
        const next = StudioNavigation.clampSubtabIndex(mainTabIndex, index);
        subTabIndex = next;
        const memory = subTabMemory.slice();
        memory[mainTabIndex] = next;
        subTabMemory = memory;
    }

    function fieldValue(field) {
        const scope = field.scope || "panel";
        if (scope === "settings")
            return StudioDraft.value(
                settingsDraft, field.key, settings[field.key]);
        if (scope === "screen")
            return effectivePanelScreenIndex(selectedPanelId);
        if (scope === "mode")
            return effectivePanelValue("visibilityMode", "always") === field.mode;
        if (scope === "length") {
            const edge = effectivePanelValue("edge", "bottom");
            const width = Number(effectivePanelValue("width", 720));
            const height = Number(effectivePanelValue("height", 76));
            if (edge === "free")
                return Math.max(width, height);
            return ["left", "right"].includes(edge) ? height : width;
        }
        return effectivePanelValue(field.key, field.fallback);
    }

    function setPanelDraftValue(panelId, key, value, fallback) {
        const updated = StudioDraft.setComparedValue(
            panelDraft(panelId),
            key,
            value,
            panelValueFor(panelId, key, fallback));
        panelDrafts = StudioDraft.setNestedMap(panelDrafts, panelId, updated);
        studioError = "";
    }

    function stagePanelValues(panelId, values) {
        let updated = panelDraft(panelId);
        const keys = Object.keys(values);
        for (let index = 0; index < keys.length; ++index) {
            const key = keys[index];
            updated = StudioDraft.setComparedValue(
                updated,
                key,
                values[key],
                panelValueFor(panelId, key, values[key]));
        }
        panelDrafts = StudioDraft.setNestedMap(panelDrafts, panelId, updated);
        studioError = "";
    }

    function setFieldValue(field, value) {
        const scope = field.scope || "panel";
        if (scope === "settings") {
            settingsDraft = StudioDraft.setComparedValue(
                settingsDraft,
                field.key,
                value,
                settings[field.key]);
        } else if (scope === "screen") {
            const screenIndex = Number(value);
            const draft = screenIndex === panelScreenIndex(selectedPanelId)
                ? {}
                : {
                    index: screenIndex,
                    id: screenIndex >= 0 && screenIndex < screenOptions.length
                        ? String(screenOptions[screenIndex].id || "") : ""
                };
            screenDrafts = StudioDraft.setNestedMap(
                screenDrafts, selectedPanelId, draft);
        } else if (scope === "mode") {
            const current = effectivePanelValue("visibilityMode", "always");
            setPanelDraftValue(
                selectedPanelId,
                "visibilityMode",
                value ? field.mode
                    : (current === field.mode ? "always" : current),
                "always");
        } else if (scope === "length") {
            const edge = effectivePanelValue("edge", "bottom");
            const length = Number(value);
            if (edge === "free") {
                setPanelDraftValue(selectedPanelId, "width", length, 420);
                setPanelDraftValue(selectedPanelId, "height", length, 420);
            } else if (["left", "right"].includes(edge)) {
                setPanelDraftValue(selectedPanelId, "height", length, 420);
            } else {
                setPanelDraftValue(selectedPanelId, "width", length, 720);
            }
        } else {
            setPanelDraftValue(
                selectedPanelId, field.key, value, field.fallback);
        }
        studioError = "";
    }

    function fieldOptionIndex(field) {
        return optionIndex(field.options || [], fieldValue(field));
    }

    function formatFieldValue(field, value) {
        const decimals = field.decimals === undefined ? 0 : field.decimals;
        const numeric = Number(value)
            * (field.displayScale === undefined ? 1 : field.displayScale);
        const text = decimals > 0 ? numeric.toFixed(decimals) : Math.round(numeric);
        return (field.prefix || "") + text + (field.suffix || "");
    }

    function displayFieldValue(field) {
        if (field.value !== undefined)
            return String(field.value);
        return String(fieldValue(field));
    }

    function colorDisplayValue(field) {
        const value = String(fieldValue(field)).trim();
        return value.length > 0 ? value : qsTr("Theme default");
    }

    function colorPreviewValue(field) {
        const value = String(fieldValue(field)).trim();
        return /^#(?:[0-9a-f]{3,4}|[0-9a-f]{6}|[0-9a-f]{8})$/i.test(value)
            ? value : "transparent";
    }

    function openColorEditor(field) {
        colorField = field;
        colorTargetPanelId = selectedPanelId;
        const current = String(fieldValue(field)).trim();
        colorDialog.selectedColor = current.length > 0 ? current : "#334455";
        colorDialog.open();
    }

    function readOnlyRow(label, value, description) {
        return {
            kind: "readonly",
            label: label,
            value: value,
            description: description || ""
        };
    }

    function optionLabel(options, value) {
        const index = optionIndex(options, value);
        if (options.length > 0 && options[index].value === value)
            return options[index].label;
        return titleCase(value);
    }

    function onOff(value) {
        return value ? qsTr("On") : qsTr("Off");
    }

    function selectedScreenLabel() {
        const index = panelScreenIndex(selectedPanelId);
        if (index >= 0 && index < screenOptions.length)
            return screenOptions[index].label;
        return qsTr("Display %1").arg(index + 1);
    }

    function stageThemeAction(panelId, action, sourceUrl) {
        let draft = {};
        if (action === "import") {
            draft = { action: action, sourceUrl: sourceUrl };
        } else if (action === "clear") {
            const hasAppliedTheme =
                String(panelValueFor(panelId, "themeSource", "")).length > 0
                || String(panelValueFor(panelId, "themeAsset", "")).length > 0;
            if (hasAppliedTheme)
                draft = { action: action };
        } else if (action === "render") {
            const existing = StudioDraft.nestedMap(themeDrafts, panelId);
            if (existing.action === "import")
                return;
            if (String(panelValueFor(panelId, "themeSource", "")).length > 0)
                draft = { action: action };
        }
        themeDrafts = StudioDraft.setNestedMap(themeDrafts, panelId, draft);
        studioError = "";
    }

    function themeActionDescription(panelId) {
        const draft = StudioDraft.nestedMap(themeDrafts, panelId);
        if (draft.action === "import")
            return qsTr("Artwork import pending");
        if (draft.action === "clear")
            return qsTr("Artwork removal pending");
        if (draft.action === "render")
            return qsTr("Artwork re-render pending");
        return String(panelValueFor(
            panelId, "themeStatus", qsTr("Preset surface active.")));
    }

    function resetSelectedPanelDraft() {
        const panelId = selectedPanelId;
        const edge = panelValueFor(panelId, "edge", "bottom");
        const freePanel = edge === "free";
        const vertical = ["left", "right"].includes(edge);
        stagePanelValues(panelId, {
            visible: edge === "bottom" || freePanel,
            alignment: "center",
            visibilityMode: "always",
            dynamic: freePanel ? false : edge !== "bottom",
            width: freePanel ? 420 : (vertical ? 76 : 720),
            height: freePanel ? 420 : (vertical ? 420 : 76),
            type: freePanel ? "empty" : "hybrid",
            appearance: "glass",
            shape: "pill",
            iconShape: "rounded",
            iconSize: 52,
            spacing: 8,
            layout: freePanel ? "circular" : "adaptive",
            layoutScale: 1.0,
            layoutAngle: 0.0,
            layoutRadius: freePanel ? 145 : 150,
            layoutRows: 2,
            layoutPadding: 18,
            pathSides: 6,
            pathOrientation: "upright",
            pathAnchor: "center",
            iconAnimation: "scale",
            animationTrigger: "hover",
            animationSpeed: 1.0,
            animationIntensity: 1.0,
            physicsEnabled: false,
            folderLayout: "fan",
            folderSpeed: 260,
            opacity: 0.9,
            color: "",
            themeFit: "cover"
        });
        stageThemeAction(panelId, "clear", "");
    }

    function filteredPanelDraft(draft) {
        const result = {};
        for (let index = 0; index < editablePanelKeys.length; ++index) {
            const key = editablePanelKeys[index];
            if (key !== "visibilityMode" && StudioDraft.hasValue(draft, key))
                result[key] = draft[key];
        }
        return result;
    }

    function draftPanelIds() {
        const found = {};
        const collections = [panelDrafts, screenDrafts, themeDrafts];
        for (let collectionIndex = 0;
             collectionIndex < collections.length;
             ++collectionIndex) {
            const ids = Object.keys(collections[collectionIndex]);
            for (let index = 0; index < ids.length; ++index)
                found[ids[index]] = true;
        }
        return Object.keys(found);
    }

    function discardStudioChanges() {
        panelDrafts = StudioDraft.clear();
        settingsDraft = StudioDraft.clear();
        screenDrafts = StudioDraft.clear();
        themeDrafts = StudioDraft.clear();
        themeTargetPanelId = "";
        colorField = null;
        colorTargetPanelId = "";
        studioError = "";
    }

    function applyStudioChanges() {
        studioError = "";
        const panelIds = draftPanelIds();
        const existingPanelIds = panelRegistry.panelIds;
        for (let index = 0; index < panelIds.length; ++index) {
            if (existingPanelIds.indexOf(panelIds[index]) < 0) {
                studioError = qsTr(
                    "A panel changed outside Panel Studio. Cancel and reopen it before applying.");
                return false;
            }
        }

        for (let index = 0; index < panelIds.length; ++index) {
            const panelId = panelIds[index];
            const draft = panelDraft(panelId);
            const values = filteredPanelDraft(draft);
            if (StudioDraft.keyCount(values) > 0)
                panelRegistry.updatePanel(panelId, values);
            if (StudioDraft.hasValue(draft, "visibilityMode")) {
                panelController.setPanelVisibilityMode(
                    panelId, String(draft.visibilityMode));
            }
        }

        const settingKeys = Object.keys(settingsDraft);
        for (let index = 0; index < settingKeys.length; ++index) {
            const key = settingKeys[index];
            settings[key] = settingsDraft[key];
        }

        const screenPanelIds = Object.keys(screenDrafts);
        for (let index = 0; index < screenPanelIds.length; ++index) {
            const panelId = screenPanelIds[index];
            panelController.setPanelScreen(
                panelId, effectivePanelScreenIndex(panelId));
        }

        let themeSucceeded = true;
        for (let index = 0; index < panelIds.length; ++index) {
            const panelId = panelIds[index];
            const themeDraft = StudioDraft.nestedMap(themeDrafts, panelId);
            const action = String(themeDraft.action || "");
            if (action === "clear") {
                panelRegistry.clearTheme(panelId);
            } else if (action === "import") {
                themeSucceeded = panelRegistry.importTheme(
                    panelId, themeDraft.sourceUrl) && themeSucceeded;
            } else if (action === "render") {
                themeSucceeded = panelRegistry.renderTheme(
                    panelId,
                    Number(panelValueFor(panelId, "width", 720)),
                    Number(panelValueFor(panelId, "height", 76)),
                    Screen.devicePixelRatio,
                    true) && themeSucceeded;
            } else {
                const draft = panelDraft(panelId);
                const geometryChanged =
                    StudioDraft.hasValue(draft, "width")
                    || StudioDraft.hasValue(draft, "height")
                    || StudioDraft.hasValue(draft, "themeFit");
                if (geometryChanged
                        && String(panelValueFor(
                            panelId, "themeSource", "")).length > 0) {
                    themeSucceeded = panelRegistry.renderTheme(
                        panelId,
                        Number(panelValueFor(panelId, "width", 720)),
                        Number(panelValueFor(panelId, "height", 76)),
                        Screen.devicePixelRatio,
                        true) && themeSucceeded;
                }
            }
        }

        panelDrafts = StudioDraft.clear();
        settingsDraft = StudioDraft.clear();
        screenDrafts = StudioDraft.clear();
        themeDrafts = StudioDraft.clear();
        themeTargetPanelId = "";
        colorField = null;
        colorTargetPanelId = "";
        if (!themeSucceeded) {
            studioError = qsTr(
                "The settings were applied, but an artwork operation failed.");
            return false;
        }
        return true;
    }

    function acceptStudioChanges() {
        if ((!hasPendingChanges || applyStudioChanges()))
            close();
    }

    function cancelStudioChanges() {
        discardStudioChanges();
        close();
    }

    function performStudioAction(action, data) {
        if ((action === "create-free" || action === "remove-panel")
                && hasPendingChanges)
            return;
        if (action === "create-free") {
            openPanelEditor(panelController.createFreePanel());
        } else if (action === "remove-panel") {
            const removed = selectedPanelId;
            panelController.removePanel(removed);
            selectPanel(panelRegistry.activePanelId);
        } else if (action === "import-theme") {
            themeTargetPanelId = selectedPanelId;
            themeDialog.open();
        } else if (action === "clear-theme") {
            stageThemeAction(selectedPanelId, "clear", "");
        } else if (action === "render-theme") {
            stageThemeAction(selectedPanelId, "render", "");
        } else if (action === "reset-panel") {
            resetSelectedPanelDraft();
        }
    }

    function section(label, description, first) {
        return {
            kind: "section",
            label: label,
            description: description || "",
            first: first === true
        };
    }

    function notice(text, warning) {
        return { kind: "notice", text: text, warning: warning === true };
    }

    function panelField(kind, label, key, fallback, extra) {
        const row = {
            kind: kind,
            label: label,
            key: key,
            fallback: fallback,
            scope: "panel"
        };
        if (extra) {
            for (const propertyName in extra)
                row[propertyName] = extra[propertyName];
        }
        return row;
    }

    function settingsField(kind, label, key, extra) {
        const row = {
            kind: kind,
            label: label,
            key: key,
            scope: "settings",
            description: qsTr("Applies to all Arch Dock panels")
        };
        if (extra) {
            for (const propertyName in extra)
                row[propertyName] = extra[propertyName];
        }
        return row;
    }

    function screenField() {
        return {
            kind: "combo",
            label: qsTr("Display"),
            key: "screen",
            scope: "screen",
            options: screenOptions.map(function(display, index) {
                return option(display.label, index);
            })
        };
    }

    function overviewPanelRows() {
        const edge = panelValue("edge", "bottom");
        const alignment = panelValue("alignment", "center");
        const freePanel = edge === "free";
        const position = freePanel
            ? qsTr("Free on desktop")
            : optionLabel(edgeOptions, edge) + " · "
                + optionLabel(alignmentOptions, alignment);
        const visibility = panelValue("visible", true)
            ? optionLabel(visibilityOptions,
                panelValue("visibilityMode", "always"))
            : qsTr("Hidden");
        const color = String(panelValue("color", "")).trim();
        const themeSource = String(panelValue("themeSource", ""));
        const themePackage = String(panelValue("themePackageName", "")).trim();
        const artwork = themePackage.length > 0
            ? themePackage
            : (themeSource.length > 0 ? qsTr("Imported artwork") : qsTr("None"));
        const rows = [
            section(qsTr("Panel"),
                qsTr("Settings currently applied to the selected panel. Edit them under Panels."),
                true),
            readOnlyRow(qsTr("Name"), panelRegistry.panelName(selectedPanelId)),
            readOnlyRow(qsTr("Type"),
                optionLabel(panelTypeOptions, panelValue("type", "empty"))),
            readOnlyRow(qsTr("Position"), position)
        ];
        if (!freePanel)
            rows.push(readOnlyRow(qsTr("Display"), selectedScreenLabel()));
        rows.push(
            readOnlyRow(qsTr("Size"),
                panelValue("width", 720) + " × " + panelValue("height", 76)
                    + qsTr(" px")),
            readOnlyRow(qsTr("Sizing"),
                panelValue("dynamic", true) ? qsTr("Dynamic") : qsTr("Static")),
            readOnlyRow(qsTr("Visibility"), visibility),
            section(qsTr("Appearance"),
                qsTr("The active surface and geometry settings.")),
            readOnlyRow(qsTr("Layout"),
                optionLabel(layoutOptions, panelValue("layout", "adaptive"))),
            readOnlyRow(qsTr("Shape"),
                optionLabel(panelShapeOptions, panelValue("shape", "pill"))),
            readOnlyRow(qsTr("Theme"),
                optionLabel(materialOptions, panelValue("appearance", "glass"))),
            readOnlyRow(qsTr("Color"),
                color.length > 0 ? color : qsTr("Theme default")),
            readOnlyRow(qsTr("Opacity"),
                Math.round(Number(panelValue("opacity", 0.9)) * 100) + "%"),
            readOnlyRow(qsTr("Artwork"), artwork));
        return rows;
    }

    function overviewIconRows() {
        const animation = optionLabel(
            motionOptions,
            panelValue("iconAnimation", "scale"));
        const trigger = optionLabel(
            triggerOptions,
            panelValue("animationTrigger", "hover")).toLowerCase();
        const animationSummary = animation + " · " + trigger + " · "
            + Number(panelValue("animationSpeed", 1)).toFixed(1) + "×";
        const appearance = panelValue("appearance", "glass");
        const tileAppearances = ["plate", "platform", "pedestal"];
        return [
            section(qsTr("Icons"),
                qsTr("Icon settings currently applied to the selected panel. Edit them under Icons."),
                true),
            readOnlyRow(qsTr("Size"),
                panelValue("iconSize", 52) + qsTr(" px")),
            readOnlyRow(qsTr("Spacing"),
                Math.round(Number(panelValue("spacing", 8))) + qsTr(" px")),
            readOnlyRow(qsTr("Shape"),
                optionLabel(iconShapeOptions,
                    panelValue("iconShape", "rounded"))),
            readOnlyRow(qsTr("Animation"), animationSummary),
            readOnlyRow(qsTr("Motion intensity"),
                Number(panelValue("animationIntensity", 1)).toFixed(1) + "×"),
            section(qsTr("Appearance"),
                qsTr("Active global and theme-driven icon effects.")),
            readOnlyRow(qsTr("Surface style"),
                optionLabel(materialOptions, appearance)),
            readOnlyRow(qsTr("Reflections"), onOff(settings.showReflections),
                qsTr("Applies to all Arch Dock panels")),
            readOnlyRow(qsTr("Indicators"), onOff(settings.showIndicators),
                qsTr("Applies to all Arch Dock panels")),
            readOnlyRow(qsTr("Magnification"),
                settings.magnificationEnabled
                    ? Number(settings.magnification).toFixed(2) + "×"
                    : qsTr("Off"),
                qsTr("Applies to all Arch Dock panels")),
            readOnlyRow(qsTr("Icon tiles"),
                tileAppearances.includes(appearance)
                    ? optionLabel(materialOptions, appearance)
                    : qsTr("Off"))
        ];
    }

    function panelsGeneralRows() {
        const edge = effectivePanelValue("edge", "bottom");
        const freePanel = edge === "free";
        const edgeEditable = !panelRegistry.isBuiltIn(selectedPanelId) && !freePanel;
        const orientation = freePanel
            ? titleCase(effectivePanelValue("layout", "adaptive"))
            : (["left", "right"].includes(edge)
                ? qsTr("Vertical") : qsTr("Horizontal"));
        const rows = [
            section(qsTr("General"), qsTr("Identity and placement of the selected panel."), true),
            {
                kind: "actions",
                label: qsTr("Panel"),
                actions: [
                    { label: qsTr("Add free panel"), icon: "list-add",
                      action: "create-free", available: !hasPendingChanges },
                    { label: panelRegistry.isBuiltIn(selectedPanelId)
                        ? qsTr("Hide panel") : qsTr("Remove panel"),
                      icon: panelRegistry.isBuiltIn(selectedPanelId)
                        ? "view-hidden" : "edit-delete",
                      action: "remove-panel", available: !hasPendingChanges }
                ]
            },
            panelField("combo", qsTr("Panel Type"), "type", "empty",
                { options: panelTypeOptions }),
            panelField("combo", qsTr("Position"), "edge", "bottom",
                {
                    options: freePanel ? edgeOptions : nativeEdgeOptions,
                    available: edgeEditable,
                    description: edgeEditable ? "" : qsTr("Fixed for this panel type")
                }),
            panelField("combo", qsTr("Alignment"), "alignment", "center",
                { options: alignmentOptions }),
            { kind: "readonly", label: qsTr("Orientation"), value: orientation }
        ];
        if (!freePanel) {
            rows.push(
                screenField(),
                panelField("combo", qsTr("Screen Edge"), "edge", "bottom",
                {
                    options: nativeEdgeOptions,
                    available: edgeEditable,
                    description: edgeEditable ? "" : qsTr("Fixed for this panel type")
                }));
        }
        rows.push(notice(qsTr(
            "Lock Position will be added with live panel-geometry synchronization.")));
        return rows;
    }

    function panelsSizeRows() {
        return [
            section(qsTr("Size"), qsTr("Panel dimensions and dynamic sizing."), true),
            panelField("spin", qsTr("Width"), "width", 720,
                { from: 48, to: 4096, step: 4 }),
            panelField("spin", qsTr("Height"), "height", 76,
                { from: 48, to: 4096, step: 4 }),
            {
                kind: "spin",
                label: qsTr("Length"),
                scope: "length",
                from: 48,
                to: 4096,
                step: 4,
                description: effectivePanelValue("edge", "bottom") === "free"
                    ? qsTr("Sets both width and height")
                    : qsTr("Along the panel orientation")
            },
            panelField("switch", qsTr("Dynamic Size"), "dynamic", true),
            notice(qsTr("Floating Margin needs a placement adapter before it can safely change native and free panels."))
        ];
    }

    function panelsAppearanceRows() {
        return [
            section(qsTr("Appearance"), qsTr("Surface styling for the selected panel."), true),
            panelField("combo", qsTr("Shape"), "shape", "pill",
                { options: panelShapeOptions }),
            panelField("color", qsTr("Color"), "color", ""),
            panelField("slider", qsTr("Opacity"), "opacity", 0.9,
                { from: 0, to: 1, step: 0.05, decimals: 2 }),
            panelField("combo", qsTr("Theme"), "appearance", "glass",
                { options: materialOptions }),
            panelField("combo", qsTr("Artwork fit"), "themeFit", "cover",
                { options: choices(["cover", "contain", "stretch", "tile"]) }),
            {
                kind: "actions",
                label: qsTr("Artwork"),
                description: themeActionDescription(selectedPanelId),
                actions: [
                    { label: qsTr("Import"), icon: "document-import",
                      action: "import-theme" },
                    { label: qsTr("Re-render"), icon: "view-refresh",
                      action: "render-theme" },
                    { label: qsTr("Clear"), icon: "edit-clear",
                      action: "clear-theme" }
                ]
            },
            notice(qsTr("Texture, Border, Shadow, and Blur will appear here once their renderer path is implemented."))
        ];
    }

    function panelsBehaviorRows() {
        return [
            section(qsTr("Behavior"), qsTr("Visibility and interaction rules."), true),
            panelField("switch", qsTr("Visible"), "visible", true),
            {
                kind: "switch",
                label: qsTr("Auto Hide"),
                scope: "mode",
                mode: "auto-hide"
            },
            {
                kind: "switch",
                label: qsTr("Dodge Windows"),
                scope: "mode",
                mode: "dodge"
            },
            panelField("switch", qsTr("Dynamic / Static"), "dynamic", true,
                { description: qsTr("On is dynamic; off is static") }),
            panelField("switch", qsTr("Spring rearrangement"),
                "physicsEnabled", false),
            panelField("combo", qsTr("Folder expansion"), "folderLayout", "fan",
                { options: folderLayoutOptions }),
            panelField("spin", qsTr("Folder animation speed"), "folderSpeed", 260,
                { from: 80, to: 1200, step: 20 })
        ];
    }

    function panelsLayoutRows() {
        return [
            section(qsTr("Layout"), qsTr("Shape geometry and content placement."), true),
            panelField("combo", qsTr("Dock layout"), "layout", "adaptive",
                { options: layoutOptions }),
            panelField("slider", qsTr("Layout scale"), "layoutScale", 1.0,
                { from: 0.5, to: 2.5, step: 0.05, decimals: 2, suffix: "×" }),
            panelField("spin", qsTr("Radius"), "layoutRadius", 150,
                { from: 48, to: 2048, step: 2 }),
            panelField("spin", qsTr("Layout angle"), "layoutAngle", 0,
                { from: -180, to: 180, step: 1 }),
            panelField("spin", qsTr("Polygon sides"), "pathSides", 6,
                { from: 3, to: 12, step: 1 }),
            panelField("combo", qsTr("Content Alignment"), "pathAnchor", "center",
                { options: pathAnchorOptions }),
            panelField("combo", qsTr("Icon path orientation"),
                "pathOrientation", "upright",
                { options: pathOrientationOptions }),
            panelField("spin", qsTr("Grid rows"), "layoutRows", 2,
                { from: 1, to: 8, step: 1 }),
            panelField("spin", qsTr("Panel Padding"), "layoutPadding", 18,
                { from: 0, to: 240, step: 1 }),
            notice(qsTr("Content Margins, Start Offset, and End Offset need geometry support before they can be enabled."))
        ];
    }

    function panelsSegmentsRows() {
        return [
            section(qsTr("Segments"), qsTr("Independent panel surface sections."), true),
            notice(qsTr("Segments are not rendered yet. Enabled / Disabled, spacing, style, width, color, opacity, glow, corner radius, and padding will be added together so the controls cannot silently do nothing."), true)
        ];
    }

    function iconsAppearanceRows() {
        return [
            section(qsTr("Appearance"), qsTr("Icon geometry and live global effects."), true),
            panelField("combo", qsTr("Shape"), "iconShape", "rounded",
                { options: iconShapeOptions }),
            panelField("spin", qsTr("Size"), "iconSize", 52,
                { from: 24, to: 128, step: 2 }),
            panelField("slider", qsTr("Spacing"), "spacing", 8,
                { from: 0, to: 48, step: 1, decimals: 0 }),
            settingsField("switch", qsTr("Reflection"), "showReflections"),
            notice(qsTr("Opacity, Glow, Shadow, Attention Color, and independent Icon Style require the icon renderer work planned for this section."))
        ];
    }

    function iconsBehaviorRows() {
        return [
            section(qsTr("Hover and Click"),
                qsTr("Select an animation and the event that triggers it."), true),
            panelField("combo", qsTr("Animation"), "iconAnimation", "scale",
                { options: motionOptions }),
            panelField("combo", qsTr("Trigger"), "animationTrigger", "hover",
                { options: triggerOptions }),
            panelField("slider", qsTr("Animation Speed"), "animationSpeed", 1.0,
                { from: 0.2, to: 3, step: 0.1, decimals: 1, suffix: "×" }),
            panelField("slider", qsTr("Motion intensity"), "animationIntensity", 1.0,
                { from: 0.1, to: 2.5, step: 0.1, decimals: 1 }),
            section(qsTr("Magnification"),
                qsTr("These controls currently apply to all panels.")),
            settingsField("switch", qsTr("Enabled"), "magnificationEnabled"),
            settingsField("slider", qsTr("Size"), "magnification",
                { from: 1, to: 2.4, step: 0.05, decimals: 2, suffix: "×" }),
            settingsField("slider", qsTr("Speed"), "animationDuration",
                { from: 80, to: 500, step: 10, decimals: 0, suffix: qsTr(" ms") }),
            notice(qsTr("Separate Hover Glow, Click Color/Shade, Lift, and Attention controls need per-state renderer support."))
        ];
    }

    function iconsIndicatorRows() {
        return [
            section(qsTr("Indicators"), qsTr("Running and attention markers."), true),
            settingsField("switch", qsTr("Enabled"), "showIndicators"),
            notice(qsTr("Shape, Color, Size, Style, and Attention styling are currently renderer-defined. They will be exposed together with per-panel indicators."))
        ];
    }

    function iconsNotificationRows() {
        return [
            section(qsTr("Notifications"), qsTr("Badges and transient icon notices."), true),
            notice(qsTr("Notifications are not implemented yet. Enabled, Show on Hover, Duration, and Position will be added with the notification source and renderer."), true)
        ];
    }

    function iconStyleRows() {
        return [
            section(qsTr("Icon Style"), qsTr("Reusable sets of icon appearance settings."), true),
            notice(qsTr("Name, Save, Load, Import, and Export require an Icon Style store. They are intentionally not presented as non-working buttons."), true)
        ];
    }

    function iconTileRows() {
        return [
            section(qsTr("Icon Tiles"), qsTr("The surface behind each icon."), true),
            panelField("combo", qsTr("Shape"), "iconShape", "rounded",
                { options: iconShapeOptions }),
            notice(qsTr("Enabled, Color, and Opacity need an independent tile renderer. Shape remains connected to the current working icon-shape setting."))
        ];
    }

    function quickProfileRows() {
        return [
            section(qsTr("Quick Profile"),
                qsTr("Reusable profiles for the selected panel."), true),
            notice(qsTr("Recent panel profiles and Favorites will appear here after the panel-profile store is implemented."))
        ];
    }

    function manageProfileRows() {
        return [
            section(qsTr("Manage"), qsTr("Saved Panel Studio profiles."), true),
            notice(qsTr("Save, Rename, and Delete will be enabled with the profile store so complete settings can be restored safely.")),
            {
                kind: "action",
                label: qsTr("Reset"),
                buttonText: qsTr("Reset selected panel"),
                icon: "edit-undo",
                action: "reset-panel"
            }
        ];
    }

    function shortcutRows() {
        return [
            section(qsTr("Shortcuts"), qsTr("Apply Profile 1–4."), true),
            notice(qsTr("Profile shortcuts will use KDE GlobalAccel. Arch Dock will not install a keyboard-event watcher because that can interfere with Plasma and animated wallpapers."), true)
        ];
    }

    function rowsForCurrentPage() {
        if (mainTabIndex === 0)
            return subTabIndex === 0 ? overviewPanelRows() : overviewIconRows();
        if (mainTabIndex === 1) {
            const panelPages = [
                panelsGeneralRows,
                panelsSizeRows,
                panelsAppearanceRows,
                panelsBehaviorRows,
                panelsLayoutRows,
                panelsSegmentsRows
            ];
            return panelPages[subTabIndex]();
        }
        if (mainTabIndex === 2) {
            const iconPages = [
                iconsAppearanceRows,
                iconsBehaviorRows,
                iconsIndicatorRows,
                iconsNotificationRows,
                iconStyleRows
            ];
            return iconPages[subTabIndex]();
        }
        if (mainTabIndex === 3)
            return iconTileRows();
        const profilePages = [quickProfileRows, manageProfileRows, shortcutRows];
        return profilePages[subTabIndex]();
    }

    onShowPanelsChanged: {
        if (showPanels)
            setMainTab(1);
    }

    width: 980
    height: 820
    minimumWidth: 820
    minimumHeight: 640
    x: 260
    y: 120
    visible: false
    color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint
    title: qsTr("Arch Dock Panel Studio")

    onVisibleChanged: {
        if (visible)
            requestActivate();
    }
    onClosing: discardStudioChanges()

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.cancelStudioChanges()
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        border.width: 1
        border.color: "#75d9edf2"
        gradient: Gradient {
            GradientStop { position: 0; color: "#f62a3848" }
            GradientStop { position: 1; color: "#f1081018" }
        }
    }

    Rectangle {
        id: titleStrip

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 1
        height: 38
        color: "#e8182b3b"
        clip: true

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: "#5e77d6e8"
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Kirigami.Icon {
                width: 18
                height: 18
                source: "preferences-desktop-theme-global"
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Arch Dock")
                color: "#f4f8fb"
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 1
                height: 17
                color: "#4d9cb4c1"
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Panel Studio")
                color: "#a9bfcb"
                font.pixelSize: 11
            }
        }

        Label {
            anchors.centerIn: parent
            text: {
                const revision = root.panelRevision;
                const name = panelRegistry.panelName(root.selectedPanelId);
                return name.length > 0 ? name : qsTr("Panel Editor");
            }
            color: "#d5e5ed"
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        DragHandler {
            target: null
            onActiveChanged: {
                if (active)
                    root.startSystemMove();
            }
        }

        ToolButton {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 30
            height: 30
            icon.name: "window-close"
            onClicked: root.cancelStudioChanges()

            background: Rectangle {
                radius: 4
                color: parent.hovered ? "#b94b526f" : "transparent"
            }

            ToolTip.visible: hovered
            ToolTip.text: qsTr("Close Panel Studio")
        }
    }

    RowLayout {
        anchors.top: titleStrip.bottom
        anchors.bottom: footer.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 10
        anchors.leftMargin: 10
        anchors.rightMargin: 14
        anchors.bottomMargin: 10
        spacing: 12

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 164
            radius: 8
            color: "#4a101a23"
            border.width: 1
            border.color: "#263d5361"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.topMargin: 4
                    Layout.bottomMargin: 6
                    text: qsTr("STUDIO")
                    color: "#718995"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 1.2
                }

                Repeater {
                    model: root.mainTabLabels

                    delegate: ItemDelegate {
                        required property string modelData
                        required property int index

                        Layout.fillWidth: true
                        Layout.preferredHeight: 48
                        text: modelData
                        icon.name: root.mainTabIcons[index]
                        checkable: true
                        checked: root.mainTabIndex === index
                        onClicked: root.setMainTab(index)

                        contentItem: RowLayout {
                            spacing: 10

                            Kirigami.Icon {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                source: parent.parent.icon.name
                                color: parent.parent.checked ? "#7ce7fa" : "#9db0ba"
                            }

                            Label {
                                Layout.fillWidth: true
                                text: parent.parent.text
                                color: parent.parent.checked ? "#f4fbff" : "#b2c1c8"
                                font.weight: parent.parent.checked
                                    ? Font.DemiBold : Font.Normal
                            }
                        }

                        background: Rectangle {
                            radius: 6
                            color: parent.checked ? "#4b23627a"
                                : (parent.hovered ? "#26384a56" : "transparent")
                            border.width: parent.checked ? 1 : 0
                            border.color: "#5e73cfe7"

                            Rectangle {
                                visible: parent.parent.checked
                                anchors.left: parent.left
                                anchors.leftMargin: 1
                                anchors.verticalCenter: parent.verticalCenter
                                width: 3
                                height: 24
                                radius: 2
                                color: "#70e5f8"
                            }
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Label {
                    Layout.fillWidth: true
                    Layout.margins: 7
                    text: qsTr("Changes are held until Apply or OK")
                    color: "#617985"
                    font.pixelSize: 9
                    wrapMode: Text.Wrap
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: qsTr("Panel")
                    color: "#91a8b5"
                }

                ComboBox {
                    id: panelSelector

                    Layout.fillWidth: true
                    model: panelRegistry.panelIds
                    currentIndex: Math.max(0, model.indexOf(root.selectedPanelId))
                    displayText: panelRegistry.panelName(currentText)
                    delegate: ItemDelegate {
                        required property string modelData

                        width: panelSelector.width
                        text: panelRegistry.panelName(modelData)
                    }
                    onActivated: {
                        focus = false;
                        root.selectPanel(currentText);
                    }
                }

                Button {
                    icon.name: "list-add"
                    text: qsTr("Free panel")
                    enabled: !root.hasPendingChanges
                    onClicked: root.performStudioAction("create-free", {})
                }
            }

            TabBar {
                id: subTabs

                visible: root.currentSubtabs.length > 0
                Layout.fillWidth: true
                currentIndex: root.subTabIndex

                Repeater {
                    model: root.currentSubtabs

                    delegate: TabButton {
                        required property string modelData
                        required property int index

                        text: modelData
                        width: Math.max(104, implicitWidth)
                        onClicked: root.setSubTab(index)
                    }
                }
            }

            Rectangle {
                visible: root.currentSubtabs.length === 0
                Layout.fillWidth: true
                height: 1
                color: "#31526472"
            }

            StudioForm {
                Layout.fillWidth: true
                Layout.fillHeight: true
                studio: root
                rows: root.rowsForCurrentPage()
            }
        }
    }

    RowLayout {
        id: footer

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 12
        anchors.rightMargin: 14
        anchors.bottomMargin: 10
        height: 38

        Label {
            text: root.mainTabLabels[root.mainTabIndex]
                + (root.currentSubtabs.length > 0
                    ? "  /  " + root.currentSubtabs[root.subTabIndex] : "")
            color: "#69808d"
            font.pixelSize: 10
        }

        Label {
            visible: root.studioError.length > 0
            Layout.maximumWidth: 360
            text: root.studioError
            color: "#ff8c8c"
            font.pixelSize: 10
            elide: Text.ElideRight
        }

        Label {
            visible: root.hasPendingChanges && root.studioError.length === 0
            text: qsTr("Pending changes")
            color: "#80de70"
            font.pixelSize: 10
        }

        Item {
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("OK")
            icon.name: "dialog-ok"
            onClicked: root.acceptStudioChanges()
        }

        Button {
            text: qsTr("Apply")
            icon.name: "dialog-ok-apply"
            enabled: root.hasPendingChanges
            onClicked: root.applyStudioChanges()
        }

        Button {
            text: qsTr("Cancel")
            icon.name: "dialog-cancel"
            onClicked: root.cancelStudioChanges()
        }
    }

    FileDialog {
        id: themeDialog

        title: qsTr("Import theme package or artwork")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("Theme packages (archdock-theme.json *.archdock-theme.json *.json)"),
            qsTr("Design files (*.png *.jpg *.jpeg *.webp *.avif *.heif *.heic *.jxl *.svg *.svgz *.tif *.tiff *.pdf *.psd *.xcf *.blend)"),
            qsTr("All files (*)")
        ]
        onAccepted: {
            if (root.themeTargetPanelId.length > 0) {
                root.stageThemeAction(
                    root.themeTargetPanelId, "import", selectedFile);
            }
            root.themeTargetPanelId = "";
        }
        onRejected: root.themeTargetPanelId = ""
    }

    ColorDialog {
        id: colorDialog

        title: qsTr("Choose panel color")
        parentWindow: root
        modality: Qt.WindowModal
        options: ColorDialog.ShowAlphaChannel
        onAccepted: {
            if (root.colorField !== null
                    && root.colorTargetPanelId.length > 0) {
                root.setPanelDraftValue(
                    root.colorTargetPanelId,
                    root.colorField.key,
                    selectedColor.toString(),
                    root.colorField.fallback);
            }
            root.colorField = null;
            root.colorTargetPanelId = "";
        }
        onRejected: {
            root.colorField = null;
            root.colorTargetPanelId = "";
        }
    }
}
