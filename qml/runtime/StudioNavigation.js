.pragma library

var sections = [
    {
        label: "Overview",
        subtabs: ["Panel", "Icons"]
    },
    {
        label: "Panels",
        subtabs: ["General", "Size", "Appearance", "Behavior", "Layout", "Segments",
            "Panel Themes / Skins", "Built-in Panel Presets", "My Panel Presets", "Animations"]
    },
    {
        label: "Icons",
        subtabs: ["Appearance", "Behavior", "Indicators", "Notifications", "Icon Styles",
            "Built-in Icon Presets", "My Icon Presets"]
    },
    {
        label: "Icon Tiles",
        subtabs: []
    },
    {
        label: "Profiles",
        subtabs: ["Quick Profile", "Manage", "Shortcuts"]
    }
]

// The pages that browse a preset catalog, by "section:subtab". Panel Presets
// and Icon Presets are separate catalogs, each with a built-in and a user page.
var presetPages = {
    "1:7": { kind: "panel", scope: "builtin" },
    "1:8": { kind: "panel", scope: "user" },
    "2:5": { kind: "icon", scope: "builtin" },
    "2:6": { kind: "icon", scope: "user" }
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
