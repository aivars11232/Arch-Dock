.pragma library

// Panel Studio's page structure: the sections across the top and the tabs
// under each.

var sections = [
    {
        // What is applied to the selected panel; Icon Tiles and Pedestals
        // only when the panel draws them.
        label: "Overview",
        subtabs: ["Panel", "Icons", "Icon Tiles", "Pedestals"]
    },
    {
        // Presets holds the panel presets, built-in and your own, and the
        // looks: both restyle the panel. The two tabs after it are never
        // offered; their places keep the later tabs' numbers. Widgets is a
        // bar's own: the Plasma widgets in it.
        label: "Panels",
        subtabs: ["General", "Size", "Appearance", "Behavior", "Layout", "Segments",
            "Presets", "Built-in Panel Presets", "My Panel Presets", "Animations", "3D", "Hologram",
            "Widgets"]
    },
    {
        // The icon style is chosen in one place, Icons > Appearance; there
        // is no separate Icon Styles page. Each part drawn as a hologram has
        // its Hologram tab: the panel's, the icons', the stands'.
        label: "Icons",
        subtabs: ["Appearance", "Behavior", "Indicators", "Notifications",
            "Built-in Icon Presets", "My Icon Presets", "Hologram"]
    },
    {
        label: "Icon Tiles",
        subtabs: []
    },
    {
        // The stands under the icons: shape, size, lift, material and
        // motion, on flat and 3D panels; and the stands as a hologram.
        label: "Pedestals",
        subtabs: ["Appearance", "Hologram"]
    },
    {
        label: "Profiles",
        subtabs: ["Quick Profile", "Manage", "Shortcuts"]
    },
    {
        // How Panel Studio itself looks; nothing on the desktop changes.
        label: "UI Themes",
        subtabs: []
    }
]

// The pages that browse a preset catalog, by "section:subtab". Panel Presets
// and Icon Presets are separate catalogs: the panel presets on one page with
// the looks, the icon presets on a built-in and a user page.
var presetPages = {
    "1:6": { kind: "panel", scope: "all" },
    "2:4": { kind: "icon", scope: "builtin" },
    "2:5": { kind: "icon", scope: "user" }
}

function clampIndex(index, count) {
    const safeCount = Math.max(0, Math.floor(Number(count)))
    if (safeCount === 0)
        return 0

    const numericIndex = Number(index)
    const safeIndex = isFinite(numericIndex) ? Math.floor(numericIndex) : 0
    return Math.max(0, Math.min(safeCount - 1, safeIndex))
}

function mainLabels() {
    return sections.map(function(section) {
        return section.label
    })
}

function subtabsFor(sectionIndex) {
    const index = clampIndex(sectionIndex, sections.length)
    return sections[index].subtabs.slice()
}

function clampSectionIndex(sectionIndex) {
    return clampIndex(sectionIndex, sections.length)
}

function clampSubtabIndex(sectionIndex, subtabIndex) {
    return clampIndex(subtabIndex, subtabsFor(sectionIndex).length)
}

// The catalog a page lists as { kind, scope }, or null for every other page.
function presetPage(sectionIndex, subtabIndex) {
    const section = clampSectionIndex(sectionIndex)
    const page = presetPages[section + ":" + clampSubtabIndex(section, subtabIndex)]
    return page ? { kind: page.kind, scope: page.scope } : null
}
