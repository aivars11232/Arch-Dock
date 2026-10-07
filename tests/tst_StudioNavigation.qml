import QtQuick 2.15
import QtTest 1.3
import "../qml/runtime/StudioNavigation.js" as StudioNavigation

TestCase {
    name: "StudioNavigation"

    function compareList(actual, expected) {
        compare(JSON.stringify(actual), JSON.stringify(expected))
    }

    function test_exposesExactMainSections() {
        compareList(StudioNavigation.mainLabels(), [
            "Overview",
            "Panels",
            "Icons",
            "Icon Tiles",
            "Profiles"
        ])
    }

    function test_exposesExactDirectSubtabs() {
        compareList(StudioNavigation.subtabsFor(0), ["Panel", "Icons"])
        compareList(StudioNavigation.subtabsFor(1), [
            "General",
            "Size",
            "Appearance",
            "Behavior",
            "Layout",
            "Segments",
            "Panel Themes / Skins",
            "Built-in Panel Presets",
            "My Panel Presets",
            "Animations",
            "3D"
        ])
        // ADREP-TASK-001, PD-05: the icon style is chosen only on Icons >
        // Appearance, so the Icon Styles tab that repeated it is gone.
        compareList(StudioNavigation.subtabsFor(2), [
            "Appearance",
            "Behavior",
            "Indicators",
            "Notifications",
            "Built-in Icon Presets",
            "My Icon Presets"
        ])
        compareList(StudioNavigation.subtabsFor(3), [])
        compareList(StudioNavigation.subtabsFor(4), [
            "Quick Profile",
            "Manage",
            "Shortcuts"
        ])
    }

    function test_clampsMainSectionIndices() {
        compare(StudioNavigation.clampSectionIndex(-4), 0)
        compare(StudioNavigation.clampSectionIndex(2), 2)
        compare(StudioNavigation.clampSectionIndex(99), 4)
        compare(StudioNavigation.clampSectionIndex("invalid"), 0)
    }

    function test_clampsSubtabIndices() {
        compare(StudioNavigation.clampSubtabIndex(0, -1), 0)
        compare(StudioNavigation.clampSubtabIndex(1, 99), 10)
        compare(StudioNavigation.clampSubtabIndex(2, 3), 3)
        compare(StudioNavigation.clampSubtabIndex(2, 99), 5)
        compare(StudioNavigation.clampSubtabIndex(3, 99), 0)
        compare(StudioNavigation.clampSubtabIndex(4, 2.9), 2)
    }

    // The settings pages that existed before the preset pages keep their
    // positions, so a remembered tab still opens the page it named. Studio
    // remembers a tab only while it is open, and the one caller outside it
    // opens the first page, so removing Icon Styles (PD-05) moved only the
    // two icon preset pages after it.
    function test_newPagesAreAppendedAfterTheExistingOnes() {
        compare(StudioNavigation.subtabsFor(1)[2], "Appearance")
        compare(StudioNavigation.subtabsFor(1)[5], "Segments")
        compare(StudioNavigation.subtabsFor(1)[9], "Animations")
        compare(StudioNavigation.subtabsFor(1)[10], "3D")
        compare(StudioNavigation.presetPage(1, 10), null)
        compare(StudioNavigation.subtabsFor(2)[3], "Notifications")
        compare(StudioNavigation.subtabsFor(2).indexOf("Icon Styles"), -1)
    }

    function test_presetPagesNameSeparateCatalogs() {
        compareList(StudioNavigation.presetPage(1, 7), { kind: "panel", scope: "builtin" })
        compareList(StudioNavigation.presetPage(1, 8), { kind: "panel", scope: "user" })
        compareList(StudioNavigation.presetPage(2, 4), { kind: "icon", scope: "builtin" })
        compareList(StudioNavigation.presetPage(2, 5), { kind: "icon", scope: "user" })
        compare(StudioNavigation.subtabsFor(1)[7], "Built-in Panel Presets")
        compare(StudioNavigation.subtabsFor(1)[8], "My Panel Presets")
        compare(StudioNavigation.subtabsFor(2)[4], "Built-in Icon Presets")
        compare(StudioNavigation.subtabsFor(2)[5], "My Icon Presets")
    }

    // Themes, the icon settings pages and profiles are not presets.
    function test_onlyTheFourPresetPagesArePresetPages() {
        let presetPages = 0;
        for (let section = 0; section < StudioNavigation.mainLabels().length; ++section) {
            const count = Math.max(1, StudioNavigation.subtabsFor(section).length);
            for (let subtab = 0; subtab < count; ++subtab) {
                if (StudioNavigation.presetPage(section, subtab) !== null)
                    ++presetPages
            }
        }
        compare(presetPages, 4)
        compare(StudioNavigation.subtabsFor(1)[6], "Panel Themes / Skins")
        compare(StudioNavigation.presetPage(1, 6), null)
        compare(StudioNavigation.presetPage(2, 3), null)
        compare(StudioNavigation.presetPage(4, 0), null)
        compare(StudioNavigation.presetPage(3, 0), null)
        // An out-of-range index clamps to the last page of its section.
        compare(StudioNavigation.presetPage(1, 99), null)
        // The lookup hands out copies, never its own table.
        const page = StudioNavigation.presetPage(1, 7);
        page.kind = "icon"
        compare(StudioNavigation.presetPage(1, 7).kind, "panel")
    }

    function test_genericClampHandlesEmptyAndInvalidRanges() {
        compare(StudioNavigation.clampIndex(4, 0), 0)
        compare(StudioNavigation.clampIndex(4, -8), 0)
        compare(StudioNavigation.clampIndex(NaN, 3), 0)
        compare(StudioNavigation.clampIndex(1.9, 3), 1)
    }
}
