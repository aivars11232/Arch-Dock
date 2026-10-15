import QtQuick
import QtTest
import "../qml/runtime" as Runtime
import "../qml/runtime/StudioThemes.js" as StudioThemes

// Panel Studio's UI themes: each has every colour and shape the Studio asks
// for, its words stay readable, an unknown theme is Classic, and the UI
// Themes page offers each one and marks the one in use.
TestCase {
    id: testCase
    name: "StudioThemes"
    when: windowShown
    width: 900
    height: 700
    visible: true

    readonly property var ids: ["classic", "teal-glass", "teal-hud", "amber-hud",
        "alien-white", "alien-black", "alien-green"]

    Component { id: pageComponent; Runtime.StudioThemesPage { width: 860 } }
    Component { id: spyComponent; SignalSpy {} }

    // "#rrggbb" or "#aarrggbb" as [r, g, b, a].
    function rgba(value) {
        const hex = value.slice(1);
        const part = function(at) { return parseInt(hex.substr(at, 2), 16); };
        return hex.length === 6 ? [part(0), part(2), part(4), 255] : [part(2), part(4), part(6), part(0)];
    }
    // `top` laid over the solid `bottom`.
    function over(top, bottom) {
        const alpha = top[3] / 255;
        return [0, 1, 2].map(function(i) { return top[i] * alpha + bottom[i] * (1 - alpha); }).concat([255]);
    }
    function luminance(colour) {
        const channel = function(value) {
            const v = value / 255;
            return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * channel(colour[0]) + 0.7152 * channel(colour[1]) + 0.0722 * channel(colour[2]);
    }
    function contrast(first, second) {
        const a = luminance(first), b = luminance(second);
        return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
    }

    function test_themesComeInTheirOrder() {
        compare(JSON.stringify(StudioThemes.list().map(function(theme) { return theme.id; })),
                JSON.stringify(ids));
    }

    function test_everyThemeHasEveryRole() {
        const roles = Object.keys(StudioThemes.theme("classic")).sort();
        for (const theme of StudioThemes.list()) {
            compare(JSON.stringify(Object.keys(theme).sort()), JSON.stringify(roles), theme.id);
            // An id Arch Dock's settings keep.
            verify(/^[a-z0-9]+(-[a-z0-9]+)*$/.test(theme.id) && theme.id.length <= 40, theme.id);
            verify(theme.name.length > 0 && theme.summary.length > 0, theme.id);
            for (const role of roles) {
                const value = theme[role];
                if (typeof value === "string" && value.charAt(0) === "#")
                    verify(/^#([0-9a-f]{6}|[0-9a-f]{8})$/i.test(value), theme.id + "." + role + ": " + value);
            }
            // The window itself is solid: nothing behind it shows through.
            compare(theme.windowTop.length, 7, theme.id);
            compare(theme.windowBottom.length, 7, theme.id);
            verify(typeof theme.upperCase === "boolean" && theme.letterSpacing >= 0, theme.id);
            verify(theme.radius >= 0 && theme.cardRadius >= 0 && theme.controlRadius >= 0
                   && theme.chamfer >= 0 && theme.edgeGlow >= 0, theme.id);
            verify(theme.headingWeight >= 100 && theme.headingWeight <= 900, theme.id);
        }
    }

    // Words keep the usual readability minimums against their own background
    // anywhere on the window: 4.5 to 1 for text, 3 to 1 for the faint small
    // print and for the theme's colour against the window.
    function test_wordsAreReadableInEveryTheme() {
        const pairs = [
            ["text", "row", 4.5], ["textSecondary", "row", 4.5], ["textMuted", "row", 3],
            ["heading", "windowTop", 4.5], ["description", "windowTop", 4.5], ["hint", "windowTop", 3],
            ["controlText", "controlBase", 4.5], ["controlText", "controlButton", 4.5],
            ["controlPlaceholder", "controlBase", 3], ["accentText", "accent", 4.5],
            ["sidebarHeading", "sidebar", 3], ["sidebarText", "sidebar", 4.5],
            ["sidebarTextActive", "sidebarActive", 4.5], ["footerText", "sidebar", 3],
            ["titleName", "titleStrip", 4.5], ["titleText", "titleStrip", 4.5],
            ["cardTitle", "card", 4.5], ["cardText", "card", 4.5], ["previewTitle", "preview", 4.5],
            ["warning", "row", 4.5], ["error", "row", 4.5],
            ["accent", "windowTop", 3], ["accent", "windowBottom", 3]
        ];
        for (const theme of StudioThemes.list()) {
            for (const pair of pairs) {
                for (const backdrop of ["windowTop", "windowBottom"]) {
                    const back = over(rgba(theme[pair[1]]), rgba(theme[backdrop]));
                    const ratio = contrast(over(rgba(theme[pair[0]]), back), back);
                    verify(ratio >= pair[2], theme.id + ": " + pair[0] + " on " + pair[1]
                           + " is " + ratio.toFixed(2) + " to 1");
                }
            }
        }
    }

    function test_unknownThemesAreClassic() {
        compare(StudioThemes.theme("no-such-theme").id, "classic");
        compare(StudioThemes.theme(undefined).id, "classic");
        compare(StudioThemes.theme("alien-green").id, "alien-green");
    }

    function test_labelsFollowTheThemesLettering() {
        compare(StudioThemes.label(StudioThemes.theme("classic"), "Panels"), "Panels");
        compare(StudioThemes.label(StudioThemes.theme("teal-glass"), "Panels"), "Panels");
        for (const id of ["teal-hud", "amber-hud", "alien-white", "alien-black", "alien-green"])
            compare(StudioThemes.label(StudioThemes.theme(id), "Panels"), "PANELS", id);
        compare(StudioThemes.label(null, "Panels"), "Panels");
    }

    // Every theme has its card and preview; the one in use says so and
    // cannot be chosen again, any other is chosen with its button.
    function test_pageOffersEveryThemeAndMarksTheOneInUse() {
        const page = createTemporaryObject(pageComponent, testCase, {currentThemeId: "alien-black"});
        verify(page !== null);
        const chosen = createTemporaryObject(spyComponent, testCase, {target: page, signalName: "themeChosen"});
        verify(chosen.valid);
        for (const id of ids) {
            verify(findChild(page, "studio-theme-card-" + id) !== null, id);
            verify(findChild(page, "studio-theme-preview-" + id) !== null, id);
            const use = findChild(page, "studio-theme-use-" + id);
            verify(use !== null, id);
            compare(use.enabled, id !== "alien-black", id);
            compare(use.text, id === "alien-black" ? "In use" : "Use this theme", id);
        }
        compare(page.theme.id, "alien-black");
        findChild(page, "studio-theme-use-alien-white").clicked();
        compare(chosen.count, 1);
        compare(chosen.signalArguments[0][0], "alien-white");
        // The page does not switch by itself; Panel Studio switches and tells it.
        compare(page.theme.id, "alien-black");
        page.currentThemeId = "alien-white";
        compare(findChild(page, "studio-theme-use-alien-white").enabled, false);
        compare(findChild(page, "studio-theme-use-alien-black").enabled, true);
    }
}
