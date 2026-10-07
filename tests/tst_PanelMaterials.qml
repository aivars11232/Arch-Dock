import QtQuick
import QtQuick.Window
import QtTest
import ArchDock.Rendering 1.0

// ADREP-TASK-004, PD-19: material texture survives a common colour tint.
// Sample inside the stroke, away from its edges: antialiasing and shadows
// cannot turn a flat coloured line into a passing textured material.
TestCase {
    id: testCase
    name: "PanelMaterials"
    when: windowShown
    visible: true
    width: 1100
    height: 1100

    Window { id: captureWindow; visible: true; width: 1100; height: 1100; color: "white" }

    readonly property var looks: ["glass", "crystal", "neon", "minimal",
        "plasma", "lime", "floating-glass", "metallic", "futuristic",
        "organic", "platform", "plate", "pedestal"]

    Component {
        id: sceneComponent
        PanelScene {
            animationProfiles: ({ reducedMotion: true })
            entryDelegateContext: ({hostKind: "free"})
            hostCapabilities: ({rotation: {available: true}})
        }
    }

    SignalSpy { id: paintSpy; signalName: "painted" }

    function sceneFor(look, colour, opacity) {
        const scene = createTemporaryObject(sceneComponent, captureWindow.contentItem, {
            panelDefinition: { layout: "circular", layoutRadius: 300,
                iconSize: 52, appearance: look, color: colour,
                opacity: opacity, rendererTier: "procedural2d" }
        })
        verify(scene)
        verify(scene.width <= captureWindow.width && scene.height <= captureWindow.height,
            "the owner's radius-300 panel must fit without cropping")
        verify(waitForRendering(scene))
        const hasMaterials = scene.visualPanel.materialReady !== undefined
        if (hasMaterials) tryCompare(scene.visualPanel, "materialReady", true)
        const canvas = findChild(scene, "procedural-surface-canvas")
        verify(canvas)
        tryCompare(canvas, "available", true)
        paintSpy.target = canvas
        paintSpy.clear()
        canvas.requestPaint()
        paintSpy.wait()
        captureWindow.update()
        verify(waitForRendering(scene))
        if (hasMaterials) tryVerify(function() {
            captureWindow.update()
            return variance(ringSamples(scene, grabImage(scene.visualPanel))) > 4
        }, 2000, "the worker's textured material must reach the window")
        return scene
    }

    function ringSamples(scene, picture) {
        const surface = scene.visualPanel
        const radius = Number(surface.geometry.radius)
        const values = []
        for (let i = 0; i < 96; ++i) {
            const angle = i * 2 * Math.PI / 96
            // The vertically displaced polished edge crosses the centre of
            // the track close to its left and right extrema. Exclude those
            // crossings so edge highlights cannot masquerade as texture.
            if (Math.abs(Math.sin(angle)) < 0.35) continue
            const x = Math.round(picture.width / 2 + radius * Math.cos(angle))
            const y = Math.round(picture.height / 2 + radius * Math.sin(angle))
            values.push((picture.red(x, y) + picture.green(x, y) + picture.blue(x, y)) / 3)
        }
        return values
    }

    function variance(values) {
        const mean = values.reduce((a, b) => a + b, 0) / values.length
        return values.reduce((a, b) => a + (b - mean) * (b - mean), 0) / values.length
    }

    function test_tintKeepsTexture_data() {
        return looks.map(look => ({ tag: look, look: look }))
    }

    function test_tintKeepsTexture(data) {
        const original = sceneFor(data.look, "", 1)
        const before = grabImage(original.visualPanel)
        before.save("material-" + data.look + "-default.png")
        original.visible = false
        const tinted = sceneFor(data.look, "#668899", 1)
        const picture = grabImage(tinted.visualPanel)
        picture.save("material-" + data.look + "-tinted.png")
        const textureVariance = variance(ringSamples(tinted, picture))
        console.info("MATERIAL " + JSON.stringify({look: data.look, variance: textureVariance}))
        verify(ringSamples(tinted, picture).some(value => value < 245),
            data.look + ": the capture must contain a drawn material")
        verify(textureVariance > 4, data.look + ": a tinted material needs texture inside its track; variance " + textureVariance)
        verify(!before.equals(picture), "colour tints the material")
        tinted.panelDefinition = Object.assign({}, tinted.panelDefinition, { opacity: 0.35 })
        wait(60)
        verify(!grabImage(tinted.visualPanel).equals(picture), "opacity changes what is drawn")
    }

    function test_materialsAreDistinctUnderTheSameTint() {
        const samples = []
        for (const look of looks) {
            const scene = sceneFor(look, "#668899", 1)
            const picture = grabImage(scene.visualPanel)
            picture.save("distinct-" + look + ".png")
            const values = ringSamples(scene, picture)
            verify(variance(values) > 4, look + ": material detail must be drawn before comparison")
            samples.push({look: look, values: values})
            scene.visible = false
        }
        for (let i = 0; i < samples.length; ++i) {
            for (let j = i + 1; j < samples.length; ++j) {
                const delta = samples[i].values.reduce((total, value, index) =>
                    total + Math.abs(value - samples[j].values[index]), 0) / samples[i].values.length
                verify(delta > 1, samples[i].look + " and " + samples[j].look
                    + " need different material detail, mean difference " + delta)
            }
        }
    }

    function test_animationStopsWhenHiddenAndStaticMaterialsStayIdle() {
        const scene = sceneFor("organic", "#668899", 1)
        scene.animationProfiles = {reducedMotion: false}
        const material = scene.visualPanel
        verify(!material.materialAnimationActive)
        const rested = material.energyPhase
        wait(400)
        compare(material.energyPhase, rested)
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, {appearance: "plasma"})
        tryCompare(material, "materialAnimationActive", true)
        tryVerify(function() { return material.energyPhase !== rested })
        scene.visible = false
        tryCompare(material, "materialAnimationActive", false)
        const hidden = material.energyPhase
        wait(400)
        compare(material.energyPhase, hidden)
        scene.visible = true
        tryCompare(material, "materialAnimationActive", true)
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, {opacity: 0})
        tryCompare(material, "materialAnimationActive", false)
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, {opacity: 1})
        tryCompare(material, "materialAnimationActive", true)
        scene.animationProfiles = {reducedMotion: true}
        tryCompare(material, "materialAnimationActive", false)
        const reduced = material.energyPhase
        wait(400)
        compare(material.energyPhase, reduced)
    }

    function test_onlyDeclaredSparkleChangesPixels_data() {
        return [{tag: "glass", look: "glass", sparkle: false},
                {tag: "floating-glass", look: "floating-glass", sparkle: false},
                {tag: "crystal", look: "crystal", sparkle: true},
                {tag: "plasma", look: "plasma", sparkle: true}]
    }

    function test_onlyDeclaredSparkleChangesPixels(data) {
        const scene = sceneFor(data.look, "#668899", 1)
        compare(scene.visualPanel.sparkleIntensity, 0)
        compare(Boolean(scene.visualPanel.surfaceStyle.sparkle), data.sparkle)
        const before = grabImage(scene.visualPanel)
        paintSpy.target = findChild(scene, "procedural-surface-canvas")
        paintSpy.clear()
        scene.panelDefinition = Object.assign({}, scene.panelDefinition, {sparkleIntensity: 1})
        tryCompare(scene.visualPanel, "sparkleIntensity", 1)
        paintSpy.wait()
        if (data.sparkle) {
            tryVerify(function() {
                captureWindow.update()
                return !before.equals(grabImage(scene.visualPanel))
            }, 2000, "declared sparkle must reach the drawn window")
        } else {
            wait(200)
            captureWindow.update()
            verify(waitForRendering(scene))
            verify(before.equals(grabImage(scene.visualPanel)))
        }
    }
}
