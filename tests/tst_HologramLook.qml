import QtQuick
import QtTest
import "../qml/ArchDock/Rendering/HologramLook.js" as HologramLook

// Holograms' colour sets and settings: each part's own colours by default
// (Original), a set's colours, a part's chosen colour over either; the
// settings read with their defaults and bounds.
TestCase {
    name: "HologramLook"

    function test_originalKeepsEachPartsOwnColours() {
        for (const part of HologramLook.parts) {
            const colours = HologramLook.colours({}, part)
            compare(colours.set, "original", part + " is in its own colours by default")
            for (const role of ["outline", "inline", "volume", "glow"])
                verify(colours[role].own, part + " " + role + " is its own")
        }
        const icon = HologramLook.colours({}, "Icon")
        verify(icon.light.own && icon.opening.own, "the light takes the icon's own colour")
        verify(HologramLook.colours({}, "Panel").light === undefined, "only the icons have a projector light")
    }

    function test_aSetGivesEveryPartItsColour() {
        for (const name of HologramLook.setNames) {
            if (name === "original") continue
            const colours = HologramLook.colours({ hologramIconColours: name }, "Icon")
            compare(colours.set, name)
            for (const role of ["outline", "inline", "volume", "glow", "light", "opening"]) {
                verify(!colours[role].own, name + " " + role + " is the set's")
                compare(colours[role].colour, HologramLook.colourSets[name][role])
            }
        }
        // The approved cyan look.
        compare(HologramLook.colourSets.cyan.volume, "#5aeaff")
        compare(HologramLook.colourSets.cyan.glow, "#22d8ff")
    }

    function test_aChosenColourReplacesTheSetsOrItsOwn() {
        const chosen = HologramLook.colours({ hologramPanelColours: "ruby", hologramPanelOutlineColor: "#123456" }, "Panel")
        compare(chosen.outline.colour, "#123456")
        verify(!chosen.outline.own)
        compare(chosen.volume.colour, HologramLook.colourSets.ruby.volume)
        const ownElse = HologramLook.colours({ hologramPedestalVolumeColor: "#abcdef" }, "Pedestal")
        compare(ownElse.volume.colour, "#abcdef")
        verify(ownElse.outline.own, "the rest stays its own")
        verify(HologramLook.colours({ hologramIconColours: "nonsense" }, "Icon").outline.own, "an unknown set is Original")
        verify(HologramLook.colours({ hologramIconOutlineColor: "red" }, "Icon").outline.own, "only #colours are taken")
    }

    function test_settingsHaveTheirDefaultsAndBounds() {
        const icon = HologramLook.settings({}, "Icon")
        verify(!icon.on)
        compare(icon.glow, 1)
        compare(icon.glowReach, 0.75)
        compare(icon.flicker, 1)
        compare(icon.speed, 1)
        verify(icon.float)
        compare(icon.turning, "turn")
        verify(!icon.beam, "no light while the icons are not holograms")
        verify(HologramLook.settings({ hologramIcons: true }, "Icon").beam, "the light is on by default")
        compare(icon.beamHeight, 1)
        compare(icon.beamMotion, "rise-drift")
        const bounded = HologramLook.settings({ hologramIcons: true, hologramIconGlow: 9, hologramIconSpeed: 0,
                                               hologramBeamHeight: 0, hologramIconTurning: "spin" }, "Icon")
        compare(bounded.glow, 1.5)
        compare(bounded.speed, 0.1)
        compare(bounded.beamHeight, 0.2)
        compare(bounded.turning, "turn")
        verify(!HologramLook.settings({ hologramPedestals: true }, "Pedestal").on, "no stand, no holographic stand")
        verify(HologramLook.settings({ hologramPedestals: true, pedestalShape: "star" }, "Pedestal").on)
        verify(HologramLook.settings({ hologramPanel: true }, "Panel").on)
    }

    function test_theLightsMotion() {
        const drifting = HologramLook.beamMotionFlags("rise-drift")
        verify(drifting.rise === 1 && drifting.drift === 1 && drifting.moving)
        compare(HologramLook.beamMotionFlags("pulse").pulse, 1)
        compare(HologramLook.beamMotionFlags("sweep").sweep, 1)
        const still = HologramLook.beamMotionFlags("still")
        verify(!still.moving && still.rise === 0 && still.drift === 0)
    }
}
