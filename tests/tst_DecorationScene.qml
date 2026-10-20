import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// The decoration scene builder (DecorationScene.js) in QML's own engine:
// every decoration builds on a bar and on a ring, a moving one's loop meets
// itself, each offers the colour settings it has, icons' places land where
// the icons are, and stones are cut.
TestCase {
    name: "DecorationScene"

    function bar() {
        const points = []
        for (let k = 0; k <= 80; ++k) points.push({ x: 40 + k * 10, y: 100 })
        return { points: points, closed: false, outward: { x: 0, y: -1 } }
    }

    function ring() {
        const points = []
        for (let k = 0; k < 72; ++k) {
            const a = k / 72 * 2 * Math.PI
            points.push({ x: 300 + 160 * Math.cos(a), y: 300 + 160 * Math.sin(a) })
        }
        return { points: points, closed: true }
    }

    function test_everyDecorationBuildsOnABarAndARing() {
        verify(DecorationCatalogue.decorations.length >= 160)
        for (const decoration of DecorationCatalogue.decorations) {
            for (const path of [bar(), ring()]) {
                const scene = DecorationScene.scene(decoration, path, 26, [60, 240, 420, 600], { step: 4 })
                verify(scene.elements.length > 0, decoration.id)
                verify(scene.bounds.width > 0 && scene.bounds.height > 0, decoration.id)
            }
        }
    }

    function test_motionLoopsMeetThemselves() {
        compare(DecorationScene.motionFrames(DecorationCatalogue.decoration("starship-red-core")), 0)
        compare(DecorationScene.motionFrames(DecorationCatalogue.decoration("carina-nebula")), 12)
        compare(DecorationScene.motionFrames(DecorationCatalogue.decoration("alien-hud")), 24)
        verify(DecorationScene.motionLoop(DecorationCatalogue.decoration("carina-nebula")) > 0)
        for (const id of ["carina-nebula", "quantum-wave", "oak-branch", "wizards-map", "alien-hud", "floating-candles"]) {
            const decoration = DecorationCatalogue.decoration(id)
            const at = function(phase) {
                return JSON.stringify(DecorationScene.scene(decoration, bar(), 26, [60, 240, 420, 600],
                                                            { step: 4, phase: phase }).elements)
            }
            compare(at(0), at(1), id + " meets itself")
            verify(at(0) !== at(0.25), id + " moves")
        }
    }

    function test_eachDecorationOffersTheColoursItHas() {
        const controls = function(id) { return DecorationScene.colourControls(DecorationCatalogue.decoration(id)) }
        const red = controls("starship-red-core")
        verify(red.light !== undefined && red.main !== undefined && red.palette !== undefined)
        const nebula = controls("carina-nebula")
        verify(nebula.picture !== undefined)
        verify(nebula.light === undefined && nebula.main === undefined)
        const banner = controls("crimson-gold-banner")
        verify(banner.main !== undefined && banner.metal !== undefined && banner.material !== undefined)
        // An openwork piece of jewellery has no body to colour or make of
        // anything else.
        const filigree = controls("yellow-gold-filigree")
        verify(filigree.main === undefined && filigree.material === undefined)
    }

    function test_iconPlacesLandWhereTheIconsAre() {
        const places = DecorationScene.slotsOnPath(bar(), [{ x: 40, y: 90 }, { x: 440, y: 112 }, { x: 840, y: 100 }])
        compare(places.length, 3)
        fuzzyCompare(places[0], 0, 0.01)
        fuzzyCompare(places[1], 400, 0.01)
        fuzzyCompare(places[2], 800, 0.01)
    }

    function test_stonesAreCut() {
        const scene = DecorationScene.scene(DecorationCatalogue.decoration("polished-gold-tennis"), bar(), 26,
                                            [60, 240, 420, 600], { step: 4 })
        const facets = scene.elements.filter(function(e) { return e.kind === "gem" })
        verify(facets.length > 100, String(facets.length))
        verify(facets.some(function(e) { return e.group === "stars" || e.group === "starsB" }), "a glint that twinkles")
        // A diamond's facets are greys, not a colour.
        const girdle = facets[0].albedo.colour
        const channel = function(k) { return parseInt(girdle.slice(1 + 2 * k, 3 + 2 * k), 16) }
        verify(Math.max(channel(0), channel(1), channel(2)) - Math.min(channel(0), channel(1), channel(2)) < 32, girdle)
    }

    function test_chosenColoursReachTheScene() {
        const decoration = DecorationCatalogue.decoration("starship-red-core")
        const plain = DecorationScene.scene(decoration, bar(), 26, [60, 240, 420, 600], { step: 4 })
        const blue = DecorationScene.scene(decoration, bar(), 26, [60, 240, 420, 600],
                                           { step: 4, choices: { light: "#2060ff", material: "polished-gold" } })
        verify(JSON.stringify(plain.elements) !== JSON.stringify(blue.elements))
        verify(blue.elements.some(function(e) { return e.role === "body" && e.albedo.texture === "gold-polished" }))
    }
}
