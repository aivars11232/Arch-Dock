.pragma library

var sections = [
    {
        label: "Overview",
        subtabs: ["Panel", "Icons"]
    },
    {
        label: "Panels",
        subtabs: ["General", "Size", "Appearance", "Behavior", "Layout", "Segments"]
    },
    {
        label: "Icons",
        subtabs: ["Appearance", "Behavior", "Indicators", "Notifications", "Icon Style"]
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
