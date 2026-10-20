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
            "Pedestals",
            "Profiles",
            "UI Themes"
        ])
    }

    function test_exposesExactDirectSubtabs() {
        compareList(StudioNavigation.subtabsFor(0), ["Panel", "Icons", "Icon Tiles", "Pedestals"])
        compareList(StudioNavigation.subtabsFor(1), [
            "General",
            "Size",
            "Appearance",
            "Behavior",
            "Layout",
            "Segments",
            "Presets",
            "Built-in Panel Presets",
            "My Panel Presets",
            "Animations",
            "3D",
            "Hologram"
        ])
        // The icon style is chosen only on Icons > Appearance, so the Icon
        // Styles tab that repeated it is gone.
        compareList(StudioNavigation.subtabsFor(2), [
            "Appearance",
            "Behavior",
            "Indicators",
            "Notifications",
            "Built-in Icon Presets",
            "My Icon Presets",
            "Hologram"
        ])
        compareList(StudioNavigation.subtabsFor(3), [])
        // The stands' own page, and the stands drawn as a hologram.
        compareList(StudioNavigation.subtabsFor(4), ["Appearance", "Hologram"])
        compareList(StudioNavigation.subtabsFor(5), [
            "Quick Profile",
            "Manage",
            "Shortcuts"
        ])
        compareList(StudioNavigation.subtabsFor(6), [])
    }

    function test_clampsMainSectionIndices() {
        compare(StudioNavigation.clampSectionIndex(-4), 0)
        compare(StudioNavigation.clampSectionIndex(2), 2)
        compare(StudioNavigation.clampSectionIndex(99), 6)
        compare(StudioNavigation.clampSectionIndex("invalid"), 0)
    }

    function test_clampsSubtabIndices() {
        compare(StudioNavigation.clampSubtabIndex(0, -1), 0)
        compare(StudioNavigation.clampSubtabIndex(1, 99), 11)
        compare(StudioNavigation.clampSubtabIndex(2, 3), 3)
        compare(StudioNavigation.clampSubtabIndex(2, 99), 6)
        compare(StudioNavigation.clampSubtabIndex(3, 99), 0)
        compare(StudioNavigation.clampSubtabIndex(4, 99), 1)
        compare(StudioNavigation.clampSubtabIndex(5, 2.9), 2)
        compare(StudioNavigation.clampSubtabIndex(6, 99), 0)
    }

    // The settings pages that existed before the preset pages keep their
    // positions, so a remembered tab still opens the page it named. Studio
    // remembers a tab only while it is open, and the one caller outside it
    // opens the first page, so removing Icon Styles moved only the two icon
    // preset pages after it.
    function test_newPagesAreAppendedAfterTheExistingOnes() {
        compare(StudioNavigation.subtabsFor(1)[2], "Appearance")
        compare(StudioNavigation.subtabsFor(1)[5], "Segments")
        compare(StudioNavigation.subtabsFor(1)[9], "Animations")
        compare(StudioNavigation.subtabsFor(1)[10], "3D")
        compare(StudioNavigation.presetPage(1, 10), null)
        compare(StudioNavigation.subtabsFor(2)[3], "Notifications")
        compare(StudioNavigation.subtabsFor(2).indexOf("Icon Styles"), -1)
    }

    // Panels > Presets holds the panel presets, built-in and your own, with
    // the looks; the two panel preset tabs after it are no pages of their
    // own.
    function test_presetPagesNameSeparateCatalogs() {
        compareList(StudioNavigation.presetPage(1, 6), { kind: "panel", scope: "all" })
        compare(StudioNavigation.presetPage(1, 7), null)
        compare(StudioNavigation.presetPage(1, 8), null)
        compareList(StudioNavigation.presetPage(2, 4), { kind: "icon", scope: "builtin" })
        compareList(StudioNavigation.presetPage(2, 5), { kind: "icon", scope: "user" })
        compare(StudioNavigation.subtabsFor(1)[6], "Presets")
        compare(StudioNavigation.subtabsFor(2)[4], "Built-in Icon Presets")
        compare(StudioNavigation.subtabsFor(2)[5], "My Icon Presets")
    }

    // The icon settings pages and profiles are not presets.
    function test_onlyTheThreePresetPagesArePresetPages() {
        let presetPages = 0;
        for (let section = 0; section < StudioNavigation.mainLabels().length; ++section) {
            const count = Math.max(1, StudioNavigation.subtabsFor(section).length);
            for (let subtab = 0; subtab < count; ++subtab) {
                if (StudioNavigation.presetPage(section, subtab) !== null)
                    ++presetPages
            }
        }
        compare(presetPages, 3)
        compare(StudioNavigation.presetPage(1, 5), null)
        compare(StudioNavigation.presetPage(2, 3), null)
        compare(StudioNavigation.presetPage(4, 0), null)
        compare(StudioNavigation.presetPage(3, 0), null)
        compare(StudioNavigation.presetPage(6, 0), null)
        // An out-of-range index clamps to the last page of its section.
        compare(StudioNavigation.presetPage(1, 99), null)
        // The lookup hands out copies, never its own table.
        const page = StudioNavigation.presetPage(1, 6);
        page.kind = "icon"
        compare(StudioNavigation.presetPage(1, 6).kind, "panel")
    }

    function test_genericClampHandlesEmptyAndInvalidRanges() {
        compare(StudioNavigation.clampIndex(4, 0), 0)
        compare(StudioNavigation.clampIndex(4, -8), 0)
        compare(StudioNavigation.clampIndex(NaN, 3), 0)
        compare(StudioNavigation.clampIndex(1.9, 3), 1)
    }
}
