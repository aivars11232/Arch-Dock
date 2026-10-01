import QtQuick
import QtQuick.Controls
import QtTest
import "../qml/runtime" as Runtime

TestCase {
    id: testCase

    name: "PresetBrowser"
    when: windowShown
    width: 900
    height: 760
    visible: true

    Component {
        id: browserComponent

        Runtime.PresetBrowser {
            width: 860
            height: 720
        }
    }

    Component {
        id: spyComponent

        SignalSpy {}
    }

    // The record a card draws from: a plain procedural dock, which the shared
    // renderer draws without any installed theme or icon style.
    function drawableRecord() {
        return {
            edge: "bottom",
            layout: "horizontal",
            appearance: "glass",
            shape: "pill",
            opacity: 0.9,
            iconShape: "rounded",
            iconSize: 52,
            spacing: 8,
            iconAnimation: "glow",
            animationTrigger: "hover",
            reducedMotion: false,
            effectiveRendererTier: "procedural2d",
            capabilityResolution: { available: true, renderer: { effectiveTier: "procedural2d" } },
            iconStyleDefinition: {},
            animationProfiles: []
        };
    }

    function panelCard(id, changes) {
        const card = {
            kind: "panel",
            id: id,
            name: "Panel " + id,
            description: "A fixture panel preset.",
            builtIn: true,
            revision: 1,
            derivedFromPresetId: "",
            sourceRevision: 0,
            hostKinds: ["native-edge", "free-desktop"],
            orientations: ["horizontal"],
            layouts: ["horizontal"],
            hostKind: "native-edge",
            layout: "horizontal",
            rendererTier: "skinned2d",
            fallbackTier: "procedural2d",
            themeId: "fixture-theme",
            themeName: "Fixture Theme",
            visibilityMode: "always",
            presentationMode: "open",
            presentationTrigger: "hover",
            collapseMechanism: "open",
            motionProfileId: "glow",
            motionProfileName: "Glow",
            motionTrigger: "hover",
            recommendedIconPresetId: "fixture-icons",
            recommendedIconPresetName: "Fixture Icons",
            compatibility: {
                available: true,
                fallbackApplied: false,
                reasonCode: "",
                effectiveRendererTier: "procedural2d"
            },
            preview: {
                panelDefinition: drawableRecord(),
                themeDefinition: {},
                previewMode: "horizontal"
            }
        };
        const keys = Object.keys(changes || {});
        for (let index = 0; index < keys.length; ++index)
            card[keys[index]] = changes[keys[index]];
        return card;
    }

    function iconCard(id, changes) {
        const card = panelCard(id, {
            kind: "icon",
            name: "Icons " + id,
            description: "A fixture icon preset.",
            iconStyleId: "dark-orb",
            iconStyleName: "Dark Orb",
            customized: true,
            rendererTiers: ["procedural2d"],
            reducedMotionSupport: true,
            glyphMode: "original",
            stateMotionOverrides: { urgent: "glow" }
        });
        const keys = Object.keys(changes || {});
        for (let index = 0; index < keys.length; ++index)
            card[keys[index]] = changes[keys[index]];
        return card;
    }

    function userCard(id, changes) {
        const card = panelCard(id, {
            builtIn: false,
            name: "Mine " + id,
            derivedFromPresetId: "panel-a",
            sourceRevision: 1
        });
        const keys = Object.keys(changes || {});
        for (let index = 0; index < keys.length; ++index)
            card[keys[index]] = changes[keys[index]];
        return card;
    }

    function createBrowser(properties) {
        const browser = createTemporaryObject(browserComponent, testCase, properties);
        verify(browser !== null);
        verify(waitForRendering(browser));
        return browser;
    }

    function spyOn(target, signalName) {
        const spy = createTemporaryObject(spyComponent, testCase, {
            target: target,
            signalName: signalName
        });
        verify(spy.valid);
        return spy;
    }

    function card(browser, id) {
        const found = findChild(browser, "preset-card-" + id);
        verify(found !== null, "no card for " + id);
        return found;
    }

    // Every item below `item`, including the list's delegates.
    function descendants(item) {
        const result = [];
        const pending = [item];
        while (pending.length > 0) {
            const current = pending.pop();
            const children = current.children || [];
            for (let index = 0; index < children.length; ++index) {
                result.push(children[index]);
                pending.push(children[index]);
            }
        }
        return result;
    }

    // Clicks a card action that reveals the confirmation row, then waits for
    // the frame that lays the row out. Until that frame its buttons are still
    // stacked at the row's origin, where nobody could click them.
    function clickToReveal(browser, objectName) {
        mouseClick(findChild(browser, objectName));
        verify(findChild(browser, "preset-pending-action").visible);
        verify(waitForRendering(browser));
    }

    function visibleButtons(item) {
        return descendants(item).filter(function(candidate) {
            return candidate instanceof Button && candidate.visible;
        });
    }

    function buttonTexts(item) {
        return visibleButtons(item).map(function(button) {
            return button.text;
        });
    }

    function test_listsOneCardPerPresetInCatalogOrder() {
        const browser = createBrowser({
            kind: "panel",
            scope: "builtin",
            presets: [panelCard("panel-a"), panelCard("panel-b"), panelCard("panel-c")]
        });
        compare(browser.listView.count, 3);
        compare(findChild(browser, "preset-browser-title").text, "Built-in Panel Presets");
        const first = card(browser, "panel-a");
        const second = card(browser, "panel-b");
        verify(first.y < second.y);
        verify(second.y < card(browser, "panel-c").y);
        verify(!findChild(browser, "preset-empty-state").visible);
        verify(!findChild(browser, "preset-catalog-error").visible);
        verify(!findChild(browser, "preset-notice").visible);
        verify(!findChild(browser, "preset-pending-action").visible);
        // A panel card is not an icon card: it says which catalog it is from.
        verify(first.panelPreset);
        verify(first.builtIn);
        compare(first.subtitle(), "Built-in Panel Preset");
    }

    function test_theFourPagesAreTitledAsSeparateCatalogs() {
        const browser = createBrowser({ presets: [] });
        const title = findChild(browser, "preset-browser-title");
        const pages = [
            ["panel", "builtin", "Built-in Panel Presets"],
            ["panel", "user", "My Panel Presets"],
            ["icon", "builtin", "Built-in Icon Presets"],
            ["icon", "user", "My Icon Presets"]
        ];
        for (let index = 0; index < pages.length; ++index) {
            browser.kind = pages[index][0];
            browser.scope = pages[index][1];
            compare(title.text, pages[index][2]);
            compare(browser.listView.Accessible.name, pages[index][2]);
        }
        // An empty user page says how to get a first preset.
        const empty = findChild(browser, "preset-empty-state");
        verify(empty.visible);
        verify(empty.text.indexOf("Duplicate to My Presets") >= 0);
    }

    function test_cardsShowHostRendererBehaviourAndMotion() {
        const browser = createBrowser({
            presets: [panelCard("panel-a", {
                rendererTier: "baked2.5d",
                hostKinds: ["free-desktop"],
                layouts: ["ring", "circular"],
                orientations: ["free"],
                presentationMode: "collapsed",
                collapseMechanism: "split"
            })]
        });
        const lines = card(browser, "panel-a").metadataLines.join("\n");
        verify(lines.indexOf("Host: Free panel") >= 0, lines);
        verify(lines.indexOf("Layout: ring, circular") >= 0, lines);
        verify(lines.indexOf("Orientation: free") >= 0, lines);
        verify(lines.indexOf("Renderer: baked2.5d, falls back to procedural2d") >= 0, lines);
        verify(lines.indexOf("Theme: Fixture Theme") >= 0, lines);
        verify(lines.indexOf("rests collapsed and opens on hover (split)") >= 0, lines);
        verify(lines.indexOf("Motion: Glow on hover") >= 0, lines);
        verify(lines.indexOf("Recommended icons: Fixture Icons") >= 0, lines);

        browser.kind = "icon";
        browser.presets = [iconCard("icons-a")];
        const iconLines = card(browser, "icons-a").metadataLines.join("\n");
        verify(iconLines.indexOf("Icon style: Dark Orb, customised") >= 0, iconLines);
        verify(iconLines.indexOf("Renderer: procedural2d") >= 0, iconLines);
        verify(iconLines.indexOf("Reduced motion supported") >= 0, iconLines);
        verify(iconLines.indexOf("1 per-state override(s)") >= 0, iconLines);
        verify(iconLines.indexOf("the application's own icon") >= 0, iconLines);
        compare(card(browser, "icons-a").subtitle(), "Built-in Icon Preset");
    }

    function test_everyUsableCardDrawsThroughTheSharedRenderer() {
        const browser = createBrowser({
            presets: [panelCard("panel-a"), panelCard("panel-b")]
        });
        const first = card(browser, "panel-a");
        compare(first.presetState, "ready");
        const preview = first.livePreview;
        verify(preview !== null);
        compare(preview.objectName, "preset-live-preview-panel-a");
        // The preview is the shared scene, not a picture standing in for it.
        verify(preview.panelSceneItem !== null);
        compare(preview.activeRendererTier, "procedural2d");
        verify(preview.width > 0 && preview.height > 0);
        verify(!findChild(browser, "preset-no-preview-panel-a").visible);
        // A card is a still picture: the look is kept and the motion stopped.
        compare(first.rendererCandidate.reducedMotion, true);
        compare(first.rendererCandidate.iconAnimation, "glow");
        compare(preview.panelSceneItem.reducedMotion, true);
        first.motionEnabled = true;
        compare(first.rendererCandidate.reducedMotion, false);
        compare(preview.panelSceneItem.reducedMotion, false);
    }

    function test_selectionIsReportedAndNeverAppliedToAnything() {
        const browser = createBrowser({
            presets: [panelCard("panel-a"), panelCard("panel-b")]
        });
        const selected = spyOn(browser, "presetSelected");
        const first = card(browser, "panel-a");
        const second = card(browser, "panel-b");
        verify(!first.selected && !second.selected);

        mouseClick(first, 40, 20);
        compare(selected.count, 1);
        compare(selected.signalArguments[0][0], "panel-a");
        // The browser shows what it is told is selected; it keeps no state
        // of its own that could disagree with Panel Studio.
        verify(!first.selected);
        browser.selectedPresetId = "panel-a";
        verify(first.selected);
        verify(!second.selected);
        verify(first.Accessible.selected);

        // The keyboard selects too, and the arrows move between cards.
        verify(first.activeFocus);
        keyClick(Qt.Key_Down);
        verify(second.activeFocus);
        keyClick(Qt.Key_Return);
        compare(selected.count, 2);
        compare(selected.signalArguments[1][0], "panel-b");
        keyClick(Qt.Key_Space);
        compare(selected.count, 3);
        keyClick(Qt.Key_Up);
        verify(first.activeFocus);
        keyClick(Qt.Key_Up);
        verify(first.activeFocus);
    }

    function test_builtInCardsAreReadOnlyAndOnlyOfferDuplicate() {
        const browser = createBrowser({
            presets: [panelCard("panel-a")]
        });
        const duplicated = spyOn(browser, "duplicateRequested");
        const renamed = spyOn(browser, "renameRequested");
        const removed = spyOn(browser, "removeRequested");
        verify(!findChild(browser, "preset-rename-panel-a").visible);
        verify(!findChild(browser, "preset-delete-panel-a").visible);
        compare(buttonTexts(card(browser, "panel-a")).join("|"), "Duplicate to My Presets");

        mouseClick(findChild(browser, "preset-duplicate-panel-a"));
        compare(duplicated.count, 1);
        compare(duplicated.signalArguments[0][0], "panel-a");
        compare(duplicated.signalArguments[0][1], "Panel panel-a copy");
        compare(renamed.count, 0);
        compare(removed.count, 0);
        // Duplicating is not selecting.
        compare(spyOn(browser, "presetSelected").count, 0);
    }

    function test_noCardOffersToApplyOrAuditionAPreset() {
        const browser = createBrowser({
            scope: "user",
            presets: [
                userCard("user-000000000001"),
                panelCard("panel-b", {
                    compatibility: { available: false, reasonCode: "theme-package-unavailable" },
                    preview: undefined
                })
            ]
        });
        browser.beginRename(userCard("user-000000000001"));
        const texts = buttonTexts(browser);
        verify(texts.length >= 6, texts.join("|"));
        for (let index = 0; index < texts.length; ++index) {
            verify(!/apply|preview on desktop|set as default|audition|use preset/i.test(texts[index]),
                "unexpected action: " + texts[index]);
        }
        compare(texts.filter(function(text) {
            return ["Rename", "Duplicate", "Duplicate to My Presets", "Delete", "Cancel"].indexOf(text) < 0;
        }).length, 0, texts.join("|"));
    }

    function test_userCardsCanBeRenamedDuplicatedAndDeleted() {
        const browser = createBrowser({
            scope: "user",
            presets: [userCard("user-000000000001")]
        });
        const duplicated = spyOn(browser, "duplicateRequested");
        const renamed = spyOn(browser, "renameRequested");
        const removed = spyOn(browser, "removeRequested");
        const id = "user-000000000001";
        compare(card(browser, id).subtitle(), "My Panel Preset · derived from panel-a, revision 1");
        compare(buttonTexts(card(browser, id)).join("|"), "Rename|Duplicate|Delete");
        const pending = findChild(browser, "preset-pending-action");
        const field = findChild(browser, "preset-rename-field");
        const confirm = findChild(browser, "preset-confirm-action");

        // Rename asks for the new name first and starts from the current one.
        clickToReveal(browser, "preset-rename-" + id);
        verify(field.visible);
        compare(field.text, "Mine " + id);
        compare(renamed.count, 0);
        field.text = "   ";
        verify(!confirm.enabled);
        field.text = "  Desk Ring  ";
        verify(confirm.enabled);
        mouseClick(confirm);
        compare(renamed.count, 1);
        compare(renamed.signalArguments[0][0], id);
        compare(renamed.signalArguments[0][1], "Desk Ring");
        verify(!pending.visible);

        // Cancelling a rename asks for nothing.
        clickToReveal(browser, "preset-rename-" + id);
        mouseClick(findChild(browser, "preset-cancel-action"));
        verify(!pending.visible);
        compare(renamed.count, 1);

        // The keyboard confirms and cancels a rename as well.
        clickToReveal(browser, "preset-rename-" + id);
        verify(field.activeFocus);
        field.text = "Keyboard Name";
        keyClick(Qt.Key_Return);
        compare(renamed.count, 2);
        compare(renamed.signalArguments[1][1], "Keyboard Name");
        clickToReveal(browser, "preset-rename-" + id);
        keyClick(Qt.Key_Escape);
        verify(!pending.visible);
        compare(renamed.count, 2);

        mouseClick(findChild(browser, "preset-duplicate-" + id));
        compare(duplicated.count, 1);
        compare(duplicated.signalArguments[0][1], "Mine " + id + " copy");

        // Delete cannot be undone, so it is confirmed before it is requested.
        clickToReveal(browser, "preset-delete-" + id);
        verify(!field.visible);
        compare(confirm.text, "Delete");
        compare(removed.count, 0);
        mouseClick(findChild(browser, "preset-cancel-action"));
        compare(removed.count, 0);
        clickToReveal(browser, "preset-delete-" + id);
        mouseClick(confirm);
        compare(removed.count, 1);
        compare(removed.signalArguments[0][0], id);
        verify(!pending.visible);

        // A pending question never outlives the list it was asked about.
        clickToReveal(browser, "preset-delete-" + id);
        browser.presets = [];
        verify(!pending.visible);
        compare(removed.count, 1);
    }

    function test_fallbackAndIncompatibleCardsSayWhatTheyAre() {
        const browser = createBrowser({
            presets: [
                panelCard("panel-a", {
                    compatibility: {
                        available: true,
                        fallbackApplied: true,
                        reasonCode: "theme-package-unavailable",
                        effectiveRendererTier: "procedural2d"
                    }
                }),
                panelCard("panel-b", {
                    compatibility: {
                        available: false,
                        fallbackApplied: false,
                        reasonCode: "required-capability-unavailable"
                    },
                    preview: undefined
                })
            ]
        });
        const fallback = card(browser, "panel-a");
        compare(fallback.presetState, "fallback");
        compare(findChild(browser, "preset-state-panel-a").text, "Safe fallback");
        const fallbackDetail = findChild(browser, "preset-state-detail-panel-a");
        verify(fallbackDetail.visible);
        verify(fallbackDetail.text.indexOf("its theme package cannot be loaded") >= 0);
        verify(fallbackDetail.text.indexOf("procedural2d") >= 0);
        verify(fallback.livePreview !== null);

        // An incompatible preset is marked, explains itself and draws nothing:
        // no scene exists for it and no picture stands in for one.
        const incompatible = card(browser, "panel-b");
        compare(incompatible.presetState, "incompatible");
        compare(findChild(browser, "preset-state-panel-b").text, "Incompatible");
        const detail = findChild(browser, "preset-state-detail-panel-b");
        verify(detail.visible);
        verify(detail.text.indexOf("a capability it requires is not available") >= 0);
        compare(incompatible.livePreview, null);
        compare(findChild(browser, "preset-live-preview-panel-b"), null);
        verify(findChild(browser, "preset-no-preview-panel-b").visible);
        compare(buttonTexts(incompatible).join("|"), "Duplicate to My Presets");
        // A ready card has no detail line at all.
        browser.presets = [panelCard("panel-c")];
        verify(!findChild(browser, "preset-state-detail-panel-c").visible);
        compare(findChild(browser, "preset-state-panel-c").text, "Ready");
    }

    function test_rejectedCatalogOffersNoCards() {
        const browser = createBrowser({
            presets: [panelCard("panel-a")],
            catalogStatus: { valid: false, errorCode: "invalid-json" }
        });
        const error = findChild(browser, "preset-catalog-error");
        verify(error.visible);
        verify(error.text.indexOf("invalid-json") >= 0);
        compare(browser.listView.count, 0);
        compare(findChild(browser, "preset-card-panel-a"), null);
        // It is an error, not an empty catalog.
        verify(!findChild(browser, "preset-empty-state").visible);
        browser.catalogStatus = { valid: true, errorCode: "" };
        compare(browser.listView.count, 1);
        verify(!error.visible);
    }

    function test_storeOutcomeIsShownAsANotice() {
        const browser = createBrowser({ presets: [panelCard("panel-a")] });
        const notice = findChild(browser, "preset-notice");
        verify(!notice.visible);
        browser.noticeText = "Saved to My Panel Presets.";
        verify(notice.visible);
        compare(notice.text, "Saved to My Panel Presets.");
        browser.noticeIsError = true;
        browser.noticeText = "The preset store was not changed (store-unwritable).";
        compare(notice.text, "The preset store was not changed (store-unwritable).");
    }

    function test_cardsAndActionsAreAccessible() {
        const browser = createBrowser({
            scope: "user",
            presets: [
                userCard("user-000000000001"),
                panelCard("panel-b", {
                    compatibility: { available: false, reasonCode: "theme-not-found" },
                    preview: undefined
                })
            ]
        });
        compare(browser.listView.Accessible.role, Accessible.List);
        compare(findChild(browser, "preset-browser-title").Accessible.role, Accessible.Heading);

        const first = card(browser, "user-000000000001");
        compare(first.Accessible.role, Accessible.ListItem);
        verify(first.Accessible.focusable);
        verify(first.Accessible.selectable);
        verify(first.activeFocusOnTab);
        // The name carries what a sighted user reads at a glance: the preset,
        // its catalog and whether it can be used.
        verify(first.Accessible.name.indexOf("Mine user-000000000001") >= 0);
        verify(first.Accessible.name.indexOf("My Panel Preset") >= 0);
        verify(first.Accessible.name.indexOf("Ready") >= 0);
        verify(first.Accessible.description.indexOf("Host: Screen edge, Free panel") >= 0);
        verify(first.Accessible.description.indexOf("Motion: Glow on hover") >= 0);

        const second = card(browser, "panel-b");
        verify(second.Accessible.name.indexOf("Incompatible") >= 0);
        verify(second.Accessible.description.indexOf("its theme is not installed") >= 0);

        // Every action names the preset it acts on.
        const buttons = visibleButtons(browser);
        verify(buttons.length >= 4);
        for (let index = 0; index < buttons.length; ++index) {
            verify(buttons[index].Accessible.name.length > 0, buttons[index].text);
            verify(buttons[index].activeFocusOnTab, buttons[index].text);
        }
        compare(findChild(browser, "preset-delete-user-000000000001").Accessible.name,
            "Delete Mine user-000000000001");
        compare(findChild(browser, "preset-duplicate-panel-b").Accessible.name,
            "Duplicate Panel panel-b to my presets");

        // The assistive-technology press action selects, like a click.
        const selected = spyOn(browser, "presetSelected");
        first.Accessible.pressAction();
        compare(selected.count, 1);
        compare(selected.signalArguments[0][0], "user-000000000001");
        compare(findChild(browser, "preset-rename-field").Accessible.name, "New preset name");
    }
}
