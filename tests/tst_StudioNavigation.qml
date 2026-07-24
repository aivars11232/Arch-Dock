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
            "Segments"
        ])
        compareList(StudioNavigation.subtabsFor(2), [
            "Appearance",
            "Behavior",
            "Indicators",
            "Notifications",
            "Icon Style"
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
        compare(StudioNavigation.clampSubtabIndex(1, 99), 5)
        compare(StudioNavigation.clampSubtabIndex(2, 3), 3)
        compare(StudioNavigation.clampSubtabIndex(3, 99), 0)
        compare(StudioNavigation.clampSubtabIndex(4, 2.9), 2)
    }

    function test_genericClampHandlesEmptyAndInvalidRanges() {
        compare(StudioNavigation.clampIndex(4, 0), 0)
        compare(StudioNavigation.clampIndex(4, -8), 0)
        compare(StudioNavigation.clampIndex(NaN, 3), 0)
        compare(StudioNavigation.clampIndex(1.9, 3), 1)
    }
}
