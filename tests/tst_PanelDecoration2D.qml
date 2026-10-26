import QtQuick
import QtQuick.Window
import QtTest
import ArchDock.Rendering 1.0

// The decoration renderer on a flat panel: a decoration is worked out and
// lit off the panel's thread, shows its lights in their own layers, plays
// its motion as a loop of frames, fits an edge panel's thickness and
// reaches out of a free panel's box as far as it is drawn.
TestCase {
    id: testCase
    name: "PanelDecoration2D"
    when: windowShown
    width: 1000
    height: 400

    Component {
        id: rendererComponent
        PanelDecoration2D {}
    }

    Component {
        id: sceneComponent
        PanelScene {}
    }

    // A window it is seen in (a TestCase itself is not shown).
    Component {
        id: windowComponent
        Window { width: 1000; height: 400; visible: true; color: "#1b2333" }
    }

    function rowGeometry(count) {
        return LayoutEngine.metrics("horizontal", count, 52, 8, 1, 150, 2, 18, false, 0, 6)
    }

    // The icons' centres spread along a row's path, end to end.
    function centresOn(geometry, count) {
        const path = LayoutEngine.surface("horizontal", geometry, 0, 6).points
        const first = path[0], last = path[path.length - 1]
        const centres = []
        for (let k = 0; k < count; ++k)
            centres.push({ x: first.x + (last.x - first.x) * k / Math.max(1, count - 1), y: first.y })
        return centres
    }

    function made(properties) {
        const window = createTemporaryObject(windowComponent, testCase)
        verify(window !== null)
        const renderer = createTemporaryObject(rendererComponent, window.contentItem,
                                               Object.assign({ x: 60, y: 120 }, properties))
        verify(renderer !== null)
        return renderer
    }

    function test_stillDecorationIsLitWithItsLights() {
        const geometry = rowGeometry(7)
        const renderer = made({ decoration: "starship-red-core", geometry: geometry,
                                iconCentres: centresOn(geometry, 7), animated: false })
        tryVerify(function() { return renderer.rendererReady }, 30000, "the decoration is lit")
        compare(renderer.errorReason, "")
        compare(renderer.baker.frameCount, 1)
        verify(renderer.baker.layers.indexOf("lights") >= 0, JSON.stringify(renderer.baker.layers))
        // Its end pods stand past the panel's box; the box grows by that.
        verify(renderer.reach.left > 0 && renderer.reach.right > 0, JSON.stringify(renderer.reach))
        compare(renderer.baker.drawnHalfWidth, renderer.halfWidth)
    }

    function test_movingDecorationPlaysItsLoop() {
        const geometry = rowGeometry(5)
        const renderer = made({ decoration: "carina-nebula", geometry: geometry,
                                iconCentres: centresOn(geometry, 5) })
        tryVerify(function() { return renderer.rendererReady }, 30000)
        // The first frame shows at once; the loop follows.
        tryVerify(function() { return renderer.baker.frames === renderer.baker.frameCount }, 120000,
                  "every frame of the loop is lit")
        compare(renderer.baker.frameCount, 12)
        verify(renderer.baker.loop > 0)
        verify(renderer.looping)
        verify(renderer.playing)
        // Its stars twinkle in two groups out of step.
        verify(renderer.baker.layers.indexOf("stars") >= 0 && renderer.baker.layers.indexOf("starsB") >= 0,
               JSON.stringify(renderer.baker.layers))
        const before = renderer.clock
        tryVerify(function() { return renderer.clock > before + 0.2 }, 5000, "the clock runs while it plays")
        verify(renderer.lightLevel("stars", 0.3) !== renderer.lightLevel("starsB", 0.3))
    }

    function test_edgePanelFitsItsThickness() {
        // A row as thin as a panel 64 pixels thick: the raptor's tall hull
        // is drawn smaller to fit, facing into the screen.
        const geometry = Object.assign({}, rowGeometry(5), { height: 64 })
        const renderer = made({ decoration: "raptor-hull", geometry: geometry, edge: "bottom",
                                iconCentres: centresOn(geometry, 5), animated: false })
        tryVerify(function() { return renderer.rendererReady }, 30000)
        verify(renderer.baker.drawnHalfWidth < renderer.halfWidth,
               renderer.baker.drawnHalfWidth + " < " + renderer.halfWidth)
        compare(renderer.outward.y, -1)
    }

    function test_aBarKeepsItsDecorationInsideIt() {
        // An Arch Dock bar 64 pixels deep inside a scene twice as deep that
        // reaches 30 pixels past each end of the bar: the decoration is laid
        // along the bar and fitted to it, nothing of it past the bar on any
        // side, and it runs from one end of the bar to the other. The hull's
        // ends are square; the raptor's come to a point with struts past it.
        const geometry = Object.assign({}, rowGeometry(5), { height: 128 })
        const bar = { x: 30, y: 32, width: geometry.width - 60, height: 64 }
        for (const decoration of ["collective-hull", "raptor-hull"]) {
            const renderer = made({ decoration: decoration, geometry: geometry, edge: "bottom", barWindow: bar,
                                    iconCentres: centresOn(geometry, 5), animated: false })
            tryVerify(function() { return renderer.rendererReady }, 30000, decoration)
            verify(renderer.inBar)
            verify(renderer.baker.drawnHalfWidth < renderer.halfWidth, decoration + " is fitted to the bar's depth")
            // The scene's bounds hold its elements and their halo and shadow.
            const halo = renderer.baker.drawnHalfWidth * 1.4
            const box = renderer.baker.bounds
            const left = box.x + halo, right = box.x + box.width - halo
            const top = box.y + halo, bottom = box.y + box.height - halo
            const said = decoration + " " + JSON.stringify(box) + " halo " + halo
            verify(left >= bar.x - 2 && right <= bar.x + bar.width + 2, "along the bar: " + said)
            verify(top >= bar.y - 2 && bottom <= bar.y + bar.height + 2, "across the bar: " + said)
            verify(left <= bar.x + renderer.barMargin + 3 && right >= bar.x + bar.width - renderer.barMargin - 3,
                   "from end to end: " + said)
        }
    }

    function test_aPlasmaSidePanelKeepsItsDecorationInsideIt() {
        // A Plasma-held panel 64 pixels thick inside a scene twice as deep:
        // the raptor's dome, which reaches far into the screen, is drawn
        // smaller so all of it stays inside the panel's thickness.
        const geometry = Object.assign({}, rowGeometry(5), { height: 128 })
        const renderer = made({ decoration: "raptor-hull", geometry: geometry, edge: "bottom", panelThickness: 64,
                                iconCentres: centresOn(geometry, 5), animated: false })
        tryVerify(function() { return renderer.rendererReady }, 30000)
        compare(renderer.windowNear, 32)
        compare(renderer.windowFar, 96)
        const halo = renderer.baker.drawnHalfWidth * 1.4
        const box = renderer.baker.bounds
        verify(box.y + halo >= 32 - 2 && box.y + box.height - halo <= 96 + 2,
               JSON.stringify(box) + " halo " + halo)
    }

    function sceneDefinition(overrides) {
        return Object.assign({
            schemaVersion: 2, id: "free-test", edge: "bottom", layout: "horizontal", layoutScale: 1,
            layoutAngle: 0, layoutRadius: 100, layoutRows: 2, layoutPadding: 12, pathSides: 6,
            pathOrientation: "upright", rendererTier: "procedural2d", panelThemeId: "",
            appearance: "glass", color: "#33566d", opacity: 0.9, iconSize: 36, spacing: 6,
            iconShape: "rounded", revealZone: 8, revealHandle: "edge-strip"
        }, overrides || ({}))
    }

    function sceneEntries() {
        const list = []
        for (let k = 0; k < 6; ++k) list.push({ id: "org.example." + k, displayName: "Entry " + k, active: false })
        return list
    }

    function test_panelSceneDrawsTheChosenDecoration() {
        const window = createTemporaryObject(windowComponent, testCase)
        const plain = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 40, y: 140, panelDefinition: sceneDefinition(), orderedEntries: sceneEntries(),
            entryDelegateContext: { hostKind: "free" } })
        const decorated = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 40, y: 260, panelDefinition: sceneDefinition({ panelDecoration: "starship-red-core" }),
            orderedEntries: sceneEntries(), entryDelegateContext: { hostKind: "free" } })
        verify(plain && decorated)
        tryVerify(function() {
            return decorated.visualPanel && decorated.visualPanel.objectName === "panel-decoration"
                && decorated.visualPanel.rendererReady
        }, 30000, "the decoration takes the flat surface's place")
        compare(decorated.effectiveRendererTier, "procedural2d")
        // A free panel's decoration faces its own way, and the scene's box
        // grows to hold its end pods.
        compare(decorated.visualPanel.edge, "")
        compare(decorated.visualPanel.iconCentres.length, 6)
        verify(decorated.effectBounds.width > plain.effectBounds.width,
               decorated.effectBounds.width + " > " + plain.effectBounds.width)
        compare(decorated.width, plain.width)
        // Its lights pulse by default; with its animation off they hold still.
        verify(decorated.visualPanel.playing)
        decorated.panelDefinition = sceneDefinition({ panelDecoration: "starship-red-core", panelDecorationAnimation: false })
        tryVerify(function() { return !decorated.visualPanel.playing })
    }

    function test_iconsStandBareOnADecoration() {
        // The first entry's app is active: on plain glass its icon has a
        // highlight tile; on a decoration it stands bare.
        const window = createTemporaryObject(windowComponent, testCase)
        const entries = sceneEntries()
        entries[0].active = true
        const plain = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 40, y: 20, panelDefinition: sceneDefinition(), orderedEntries: entries })
        tryVerify(function() { return findChild(plain, "icon-highlight-tile") !== null })
        tryVerify(function() { return findChild(plain, "icon-highlight-tile").visible })
        const decorated = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 40, y: 200, panelDefinition: sceneDefinition({ panelDecoration: "starship-red-core" }),
            orderedEntries: entries })
        tryVerify(function() { return decorated.visualPanel && decorated.visualPanel.rendererReady }, 30000)
        const tile = findChild(decorated, "icon-highlight-tile")
        verify(tile !== null)
        verify(!tile.visible, "no tile under an icon on a decoration")
    }

    function test_edgePanelDecorationFacesIntoTheScreen() {
        const window = createTemporaryObject(windowComponent, testCase)
        const scene = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 40, y: 140, panelDefinition: sceneDefinition({ panelDecoration: "wizard-arches", edge: "top" }),
            orderedEntries: sceneEntries() })
        tryVerify(function() { return scene.visualPanel && scene.visualPanel.rendererReady }, 30000)
        compare(scene.visualPanel.edge, "top")
        compare(scene.visualPanel.outward.y, 1)
        // The dock widget lays a top panel's row out as a bottom one and
        // says where it really stands.
        scene.panelDefinition = sceneDefinition({ panelDecoration: "wizard-arches", edge: "bottom", screenEdge: "top" })
        tryCompare(scene.visualPanel, "edge", "top")
        compare(scene.visualPanel.outward.y, 1)
    }

    function test_anEntrySitsInTheLauncherSeat() {
        const window = createTemporaryObject(windowComponent, testCase)
        const scene = createTemporaryObject(sceneComponent, window.contentItem, {
            x: 40, y: 160, panelDefinition: sceneDefinition({ panelDecoration: "raptor-hull", panelDecorationSeat: "org.example.2" }),
            orderedEntries: sceneEntries(), entryDelegateContext: { hostKind: "free" } })
        verify(scene)
        tryVerify(function() { return scene.decorationSeatAvailable }, 5000)
        compare(scene.seatedIndex, 2)
        compare(scene.rowEntryCount, 5)
        // Lit, the seat is known: the seated entry sits in it, at its size,
        // and the row closes up without it.
        tryVerify(function() { return scene.decorationSeat !== null }, 30000)
        const seat = scene.decorationSeat
        const seated = scene.entryGeometryAt(2)
        compare(seated.trackVisibility, 1)
        fuzzyCompare(seated.entryBounds.x + seated.entryBounds.width / 2, seat.x, 0.5)
        fuzzyCompare(seated.entryBounds.y + seated.entryBounds.height / 2, seat.y, 0.5)
        fuzzyCompare(seated.scaleFactor, seat.size / scene.layoutGeometry.iconSize, 0.001)
        const before = scene.entryGeometryAt(1).entryBounds, after = scene.entryGeometryAt(3).entryBounds
        verify(after.x > before.x, "the row runs on past the seated entry's old place")
        compare(scene.decorationIconCentres.length, 5)
        // A decoration without a seat seats nobody.
        scene.panelDefinition = sceneDefinition({ panelDecoration: "starship-red-core", panelDecorationSeat: "org.example.2" })
        tryCompare(scene, "seatedIndex", -1)
        compare(scene.rowEntryCount, 6)
    }

    function test_aLightBrightensUnderThePointer() {
        const geometry = rowGeometry(4)
        const renderer = made({ decoration: "starship-red-core", geometry: geometry, animated: false,
                                iconCentres: centresOn(geometry, 4) })
        tryVerify(function() { return renderer.rendererReady }, 30000)
        verify(renderer.lightSpots.length > 2, "the red lenses are known")
        const first = findChild(renderer, "panel-decoration-light-spot-0")
        const second = findChild(renderer, "panel-decoration-light-spot-1")
        verify(first && second)
        compare(first.opacity, 0)
        // Over the first lens it brightens; the others stay as they are.
        const spot = renderer.lightSpots[0]
        const window = renderer.Window.window
        const at = renderer.mapToItem(window.contentItem, Number(spot.x), Number(spot.y))
        mouseMove(window.contentItem, at.x, at.y)
        tryCompare(first, "opacity", 1, 2000)
        verify(first.scale > 1.05, "it swells a little")
        compare(second.opacity, 0)
        // Away from it, it fades back.
        mouseMove(window.contentItem, at.x, at.y + 200)
        tryCompare(first, "opacity", 0, 2000)
    }

    function test_unknownDecorationDrawsNothing() {
        const geometry = rowGeometry(3)
        const renderer = made({ decoration: "no-such-decoration", geometry: geometry,
                                iconCentres: centresOn(geometry, 3) })
        wait(200)
        verify(!renderer.rendererReady)
        compare(renderer.baker.frames, 0)
    }
}
