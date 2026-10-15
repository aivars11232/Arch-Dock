.pragma library

// Panel Studio's own looks: the colours and shapes of its window, pages and
// controls, one set per theme. They change only how Panel Studio looks;
// panels on the desktop are not touched. Every colour is solid, so nothing
// behind the window shows through it.
//
// Each theme has an id, a name and a one-line summary for the UI Themes
// page, and these roles:
//   window*        the window's frame: its gradient and outline
//   title*         the strip across its top
//   sidebar*       the list of sections on the left
//   heading, description, hint
//                  a page's title, its line under the title, its small print
//   text, textSecondary, textMuted
//                  ordinary labels, from strongest to faintest
//   separator, footerText
//                  the lines between groups, the sidebar's small print
//   row*           a setting's line on a page
//   card*          preset and look cards, and the profile and theme cards
//   bar*           the desktop preview bar on Presets
//   preview*       the area holding the panel preview
//   accent         the theme's own colour (selected things, links)
//   warning, error, ready
//                  orange notes about real problems, red errors, a "Ready" badge
//   control*       buttons, fields, lists and switches
//   radius, cardRadius, controlRadius
//                  corner rounding; chamfer cuts the corners instead
//   upperCase, letterSpacing, headingWeight
//                  capitals and wide lettering for headings and labels, and
//                  how heavy headings are (300 thin, 600 semi-bold)
//   honeycomb      the faint hexagon vents over the window, fading from its
//                  lower right corner (fully transparent for none)
//   glow, edgeGlow the light strips under the title and on the chosen section
//                  and tab, and how far frames glow in that colour (0 none)

const definitions = [
    {
        id: "classic",
        name: "Classic",
        summary: "The deep blue Panel Studio has always worn.",
        windowTop: "#2a3848", windowBottom: "#081018", windowBorder: "#75d9edf2",
        // Inside the window Classic keeps its see-through layers: they lie
        // on the window's own solid background, not on the desktop.
        titleStrip: "#e8182b3b", titleBorder: "#5e77d6e8", titleText: "#d5e5ed", titleName: "#f4f8fb",
        titleMuted: "#a9bfcb", titleDivider: "#4d9cb4c1", closeHover: "#b94b526f",
        sidebar: "#4a101a23", sidebarBorder: "#263d5361", sidebarHeading: "#718995",
        sidebarText: "#b2c1c8", sidebarTextActive: "#f4fbff",
        sidebarIcon: "#9db0ba", sidebarIconActive: "#7ce7fa",
        sidebarActive: "#4b23627a", sidebarHover: "#26384a56", sidebarActiveBorder: "#5e73cfe7",
        heading: "#f3f8fb", description: "#91a8b5", hint: "#728995",
        text: "#d8e5ec", textSecondary: "#90a7b4", textMuted: "#8195a1",
        row: "#172c3b46", rowHover: "#223c4d59", rowBorder: "#263f5260", rowBorderHover: "#426f8a99",
        separator: "#31526472",
        card: "#1b2831", cardSelected: "#22394a", cardBorder: "#3a5868", cardBorderSelected: "#73cfe7",
        cardFocus: "#f4fbff", cardTitle: "#edf7fa", cardText: "#b9cad3",
        bar: "#22394a", barBorder: "#5e73cfe7",
        preview: "#32121f2a", previewBorder: "#3d587080", previewTitle: "#e9f5fa", previewInfo: "#86dff2",
        accent: "#73cfe7", accentText: "#06212a",
        warning: "#ffc66d", error: "#ff8c8c", ready: "#80de70",
        footerText: "#69808d",
        controlBase: "#14232c", controlButton: "#1f3442", controlBorder: "#3d5d70",
        controlText: "#e3eef3", controlPlaceholder: "#6f8794",
        radius: 12, cardRadius: 8, controlRadius: 6, chamfer: 0,
        upperCase: false, letterSpacing: 0,
        honeycomb: "#00000000", glow: "#00000000", edgeGlow: 0, headingWeight: 600
    },
    {
        id: "teal-glass",
        name: "Teal Glass",
        summary: "Soft teal glass with rounded corners.",
        windowTop: "#0d1a20", windowBottom: "#050b0f", windowBorder: "#34a99b",
        titleStrip: "#0b171c", titleBorder: "#1f5f58", titleText: "#cfeeea", titleName: "#eaf7f6",
        titleMuted: "#93aeb0", titleDivider: "#2b5552", closeHover: "#5a2a33",
        sidebar: "#0b171c", sidebarBorder: "#1a2a2f", sidebarHeading: "#5f7a7d",
        sidebarText: "#93aeb0", sidebarTextActive: "#eaf7f6",
        sidebarIcon: "#7d9a9c", sidebarIconActive: "#34e7d4",
        sidebarActive: "#103430", sidebarHover: "#13262a", sidebarActiveBorder: "#34e7d4",
        heading: "#eaf7f6", description: "#93aeb0", hint: "#5f7a7d",
        text: "#eaf7f6", textSecondary: "#93aeb0", textMuted: "#5f7a7d",
        row: "#0e1d23", rowHover: "#13272e", rowBorder: "#17282e", rowBorderHover: "#235550",
        separator: "#1a2c31",
        card: "#0e1d23", cardSelected: "#113130", cardBorder: "#1f3439", cardBorderSelected: "#34e7d4",
        cardFocus: "#eaf7f6", cardTitle: "#eaf7f6", cardText: "#b6d0cf",
        bar: "#0f2a29", barBorder: "#2a8d83",
        preview: "#0a171b", previewBorder: "#1f3a3c", previewTitle: "#eaf7f6", previewInfo: "#7fd9cf",
        accent: "#34e7d4", accentText: "#03211d",
        warning: "#ffb54d", error: "#ff8a8a", ready: "#7ff3e6",
        footerText: "#5f7a7d",
        controlBase: "#0f1f24", controlButton: "#14302e", controlBorder: "#237a71",
        controlText: "#eaf7f6", controlPlaceholder: "#5f7a7d",
        radius: 12, cardRadius: 10, controlRadius: 7, chamfer: 0,
        upperCase: false, letterSpacing: 0,
        honeycomb: "#00000000", glow: "#00000000", edgeGlow: 0, headingWeight: 600
    },
    {
        id: "teal-hud",
        name: "Teal HUD",
        summary: "A dark heads-up display in teal, with cut corners.",
        windowTop: "#05100f", windowBottom: "#020708", windowBorder: "#26a89b",
        titleStrip: "#061412", titleBorder: "#1f6c64", titleText: "#2ff3df", titleName: "#e2fffb",
        titleMuted: "#86b8b2", titleDivider: "#1f4a45", closeHover: "#4a1d24",
        sidebar: "#061412", sidebarBorder: "#145049", sidebarHeading: "#2ff3df",
        sidebarText: "#86b8b2", sidebarTextActive: "#e2fffb",
        sidebarIcon: "#4f8f88", sidebarIconActive: "#2ff3df",
        sidebarActive: "#0b2c28", sidebarHover: "#09201d", sidebarActiveBorder: "#2ff3df",
        heading: "#2ff3df", description: "#86b8b2", hint: "#4f7a75",
        text: "#e2fffb", textSecondary: "#86b8b2", textMuted: "#4f7a75",
        row: "#071513", rowHover: "#0a201d", rowBorder: "#12403b", rowBorderHover: "#1f7a70",
        separator: "#0f3532",
        card: "#071513", cardSelected: "#0b2a26", cardBorder: "#175650", cardBorderSelected: "#2ff3df",
        cardFocus: "#e2fffb", cardTitle: "#e2fffb", cardText: "#a9d5cf",
        bar: "#0a2522", barBorder: "#26a89b",
        preview: "#041010", previewBorder: "#145049", previewTitle: "#e2fffb", previewInfo: "#2ff3df",
        accent: "#2ff3df", accentText: "#001a17",
        warning: "#ffb54d", error: "#ff8a8a", ready: "#7dfcef",
        footerText: "#4f7a75",
        controlBase: "#06171a", controlButton: "#0a2522", controlBorder: "#1f9488",
        controlText: "#e2fffb", controlPlaceholder: "#4f7a75",
        radius: 0, cardRadius: 0, controlRadius: 0, chamfer: 12,
        upperCase: true, letterSpacing: 1.6,
        honeycomb: "#00000000", glow: "#00000000", edgeGlow: 0, headingWeight: 600
    },
    {
        id: "amber-hud",
        name: "Amber HUD",
        summary: "A dark heads-up display in amber, with cut corners.",
        windowTop: "#100b05", windowBottom: "#070502", windowBorder: "#b07a33",
        titleStrip: "#140f07", titleBorder: "#6f4d1f", titleText: "#ffb54d", titleName: "#fff4e4",
        titleMuted: "#bba98e", titleDivider: "#4a3618", closeHover: "#4a1d14",
        sidebar: "#140f07", sidebarBorder: "#553c17", sidebarHeading: "#ffb54d",
        sidebarText: "#bba98e", sidebarTextActive: "#fff4e4",
        sidebarIcon: "#8f7854", sidebarIconActive: "#ffb54d",
        sidebarActive: "#2e2010", sidebarHover: "#21180b", sidebarActiveBorder: "#ffb54d",
        heading: "#ffb54d", description: "#bba98e", hint: "#7d6d55",
        text: "#fff4e4", textSecondary: "#bba98e", textMuted: "#7d6d55",
        row: "#151008", rowHover: "#20180c", rowBorder: "#3f2d12", rowBorderHover: "#7a5521",
        separator: "#35270f",
        card: "#151008", cardSelected: "#2a1e0d", cardBorder: "#5a4018", cardBorderSelected: "#ffb54d",
        cardFocus: "#fff4e4", cardTitle: "#fff4e4", cardText: "#d9c6a8",
        bar: "#251a0b", barBorder: "#b07a33",
        preview: "#0f0b05", previewBorder: "#553c17", previewTitle: "#fff4e4", previewInfo: "#ffd28f",
        accent: "#ffb54d", accentText: "#221402",
        warning: "#ffd28f", error: "#ff8a8a", ready: "#2ff3df",
        footerText: "#7d6d55",
        controlBase: "#17110a", controlButton: "#2a1d0c", controlBorder: "#9a6a2a",
        controlText: "#fff4e4", controlPlaceholder: "#7d6d55",
        radius: 0, cardRadius: 0, controlRadius: 0, chamfer: 12,
        upperCase: true, letterSpacing: 1.6,
        honeycomb: "#00000000", glow: "#00000000", edgeGlow: 0, headingWeight: 600
    },
    // Three looks after gaming machines: their honeycomb vents, the light
    // strips along their edges and thin wide capitals. Alien White is two
    // tone, white with black like the white machines; Alien Black is dark
    // metal lit in cool blue; Alien Green glows the old alien green.
    {
        id: "alien-white",
        name: "Alien White",
        summary: "White and black two tone, with a cool blue light strip.",
        windowTop: "#f5f7f9", windowBottom: "#e4e8ec", windowBorder: "#1a1d21",
        titleStrip: "#0d0f12", titleBorder: "#0d0f12", titleText: "#4fd5ff", titleName: "#ffffff",
        titleMuted: "#a9b2bc", titleDivider: "#3a4048", closeHover: "#5a2a30",
        sidebar: "#121418", sidebarBorder: "#121418", sidebarHeading: "#4fd5ff",
        sidebarText: "#b9c2cc", sidebarTextActive: "#ffffff",
        sidebarIcon: "#8d97a2", sidebarIconActive: "#4fd5ff",
        sidebarActive: "#1d2228", sidebarHover: "#191d22", sidebarActiveBorder: "#2a3038",
        heading: "#0d0f12", description: "#4f5a66", hint: "#6f7a86",
        text: "#14181d", textSecondary: "#4f5a66", textMuted: "#6f7a86",
        row: "#ffffff", rowHover: "#f3f6f9", rowBorder: "#d5dbe2", rowBorderHover: "#8fd3f0",
        separator: "#cfd6de",
        card: "#ffffff", cardSelected: "#eaf6fc", cardBorder: "#d3dae2", cardBorderSelected: "#0090c8",
        cardFocus: "#0d0f12", cardTitle: "#0d0f12", cardText: "#3f4a56",
        bar: "#e9eef3", barBorder: "#0090c8",
        preview: "#121418", previewBorder: "#2a3038", previewTitle: "#f4f7fa", previewInfo: "#4fd5ff",
        accent: "#006aab", accentText: "#ffffff",
        warning: "#a85d00", error: "#c4262e", ready: "#1f7a3a",
        footerText: "#8d97a2",
        controlBase: "#ffffff", controlButton: "#eef2f6", controlBorder: "#b7c1cc",
        controlText: "#14181d", controlPlaceholder: "#78838f",
        radius: 16, cardRadius: 10, controlRadius: 8, chamfer: 0,
        upperCase: true, letterSpacing: 2,
        honeycomb: "#14000000", glow: "#14c8ff", edgeGlow: 0, headingWeight: 300
    },
    {
        id: "alien-black",
        name: "Alien Black",
        summary: "Dark metal with honeycomb vents and cool blue light strips.",
        windowTop: "#1a1c20", windowBottom: "#0b0c0e", windowBorder: "#2b3037",
        titleStrip: "#0f1013", titleBorder: "#1f2329", titleText: "#2ec8ff", titleName: "#f2f5f8",
        titleMuted: "#8a95a1", titleDivider: "#2b3037", closeHover: "#4a1d24",
        sidebar: "#0f1013", sidebarBorder: "#22262c", sidebarHeading: "#2ec8ff",
        sidebarText: "#9aa5b1", sidebarTextActive: "#f2f5f8",
        sidebarIcon: "#6d7885", sidebarIconActive: "#2ec8ff",
        sidebarActive: "#171b20", sidebarHover: "#14171b", sidebarActiveBorder: "#2b3037",
        heading: "#eef2f6", description: "#8a95a1", hint: "#66717d",
        text: "#e8ecf0", textSecondary: "#9aa5b1", textMuted: "#66717d",
        row: "#121418", rowHover: "#171a1f", rowBorder: "#23272d", rowBorderHover: "#2a6f8f",
        separator: "#23272d",
        card: "#121418", cardSelected: "#10202b", cardBorder: "#262b32", cardBorderSelected: "#2ec8ff",
        cardFocus: "#f2f5f8", cardTitle: "#f2f5f8", cardText: "#b4bec9",
        bar: "#10181f", barBorder: "#2a8fbf",
        preview: "#08090a", previewBorder: "#22262c", previewTitle: "#f2f5f8", previewInfo: "#7fd8ff",
        accent: "#2ec8ff", accentText: "#001a26",
        warning: "#ffb54d", error: "#ff8a8a", ready: "#79e8a5",
        footerText: "#66717d",
        controlBase: "#0d0e11", controlButton: "#181b20", controlBorder: "#2f3640",
        controlText: "#e8ecf0", controlPlaceholder: "#66717d",
        radius: 0, cardRadius: 0, controlRadius: 0, chamfer: 10,
        upperCase: true, letterSpacing: 2,
        honeycomb: "#2e2ec8ff", glow: "#2ec8ff", edgeGlow: 5, headingWeight: 300
    },
    {
        id: "alien-green",
        name: "Alien Green",
        summary: "Black with honeycomb vents glowing the old alien green.",
        windowTop: "#0a0f09", windowBottom: "#020402", windowBorder: "#2f7a1c",
        titleStrip: "#060a05", titleBorder: "#163a0e", titleText: "#62ff3c", titleName: "#efffe6",
        titleMuted: "#93ad86", titleDivider: "#1d3a12", closeHover: "#4a1d14",
        sidebar: "#060a05", sidebarBorder: "#173d0f", sidebarHeading: "#62ff3c",
        sidebarText: "#93ad86", sidebarTextActive: "#efffe6",
        sidebarIcon: "#5f8a4b", sidebarIconActive: "#62ff3c",
        sidebarActive: "#0e1e09", sidebarHover: "#0b1607", sidebarActiveBorder: "#235c14",
        heading: "#62ff3c", description: "#93ad86", hint: "#5f7d54",
        text: "#efffe6", textSecondary: "#93ad86", textMuted: "#5f7d54",
        row: "#081006", rowHover: "#0c1809", rowBorder: "#173310", rowBorderHover: "#3b7a1f",
        separator: "#152d0e",
        card: "#081006", cardSelected: "#0f2309", cardBorder: "#1f4416", cardBorderSelected: "#62ff3c",
        cardFocus: "#efffe6", cardTitle: "#efffe6", cardText: "#bcd8ae",
        bar: "#0d1f08", barBorder: "#4fae27",
        preview: "#040803", previewBorder: "#173d0f", previewTitle: "#efffe6", previewInfo: "#a8ff8a",
        accent: "#62ff3c", accentText: "#0a1a03",
        warning: "#ffc04d", error: "#ff8a8a", ready: "#a8ff8a",
        footerText: "#5f7d54",
        controlBase: "#091206", controlButton: "#0f2009", controlBorder: "#2f7a1c",
        controlText: "#efffe6", controlPlaceholder: "#5f7d54",
        radius: 0, cardRadius: 0, controlRadius: 0, chamfer: 12,
        upperCase: true, letterSpacing: 2,
        honeycomb: "#2e62ff3c", glow: "#62ff3c", edgeGlow: 6, headingWeight: 300
    }
];

// Every theme, in the order the UI Themes page shows them.
function list() {
    return definitions;
}

// The theme with this id, or Classic when there is none.
function theme(id) {
    for (let index = 0; index < definitions.length; ++index) {
        if (definitions[index].id === id)
            return definitions[index];
    }
    return definitions[0];
}

// A label as the theme letters it: in capitals where the theme asks for them.
function label(themeValue, text) {
    return themeValue && themeValue.upperCase ? String(text).toUpperCase() : String(text);
}
