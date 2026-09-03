import QtQuick
import QtTest
import ArchDock.Rendering 1.0

TestCase {
    id: testCase

    name: "IconStyleVisual"
    when: windowShown
    width: 900
    height: 560

    readonly property var styleIds: [
        "metallic-blue", "metallic-red", "neon-green", "neon-orange",
        "dark-orb"
    ]
    readonly property var stateIds: [
        "normal", "running", "minimized", "active", "launching",
        "hover", "pressed", "urgent", "drop", "disabled", "edit"
    ]

    Component {
        id: iconSceneComponent

        IconScene {
            logicalSize: 96
            entry: ({
                id: "org.example.browser",
                appId: "org.example.browser",
                desktopEntryId: "org.example.browser.desktop",
                displayName: "Example Browser",
                iconName: "applications-internet",
                running: false,
                active: false,
                minimized: false
            })
            tileShape: "rounded"
            appearance: "glass"
            showReflection: true
            showIndicator: true
            reducedMotion: true
        }
    }

    Component {
        id: previewComponent

        LivePanelPreview {
            width: 760
            height: 300
            orderedEntries: [
                {
                    id: "org.example.launcher",
                    displayName: "Launcher",
                    iconName: "start-here-kde"
                },
                {
                    id: "org.example.browser",
                    displayName: "Browser",
                    iconName: "applications-internet"
                },
                {
                    id: "org.example.files",
                    displayName: "Files",
                    iconName: "system-file-manager"
                }
            ]
            panelDefinition: ({
                schemaVersion: 2,
                edge: "bottom",
                layout: "horizontal",
                layoutPadding: 12,
                iconSize: 64,
                spacing: 8,
                appearance: "glass",
                rendererTier: "procedural2d"
            })
            hostCapabilities: ({
                available: true,
                renderer: {
                    effectiveTier: "procedural2d",
                    fallbackApplied: false
                }
            })
            animationProfiles: ({ reducedMotion: true })
        }
    }

    function loadStyle(styleId) {
        const request = new XMLHttpRequest()
        const source = Qt.resolvedUrl(
            "../assets/icon-styles/" + styleId
                + "/archdock-icon-style.json")
        request.open("GET", source, false)
        request.send()
        verify(request.status === 0 || request.status === 200,
               "could not read " + source + ": " + request.status)
        const style = JSON.parse(request.responseText)
        style.valid = true
        style.loadable = true
        style.status = "valid"
        style.validationStatus = "valid"
        style.assetPaths = ({})
        return style
    }

    function createScene(styleId) {
        const scene = createTemporaryObject(iconSceneComponent, testCase, {
            iconStyleDefinition: loadStyle(styleId)
        })
        verify(scene !== null)
        wait(0)
        return scene
    }

    function clearState(scene) {
        scene.editMode = false
        scene.disabled = false
        scene.dropTarget = false
        scene.urgent = false
        scene.pressed = false
        scene.hovered = false
        scene.launching = false
        scene.active = false
        scene.minimized = false
        scene.running = false
    }

    function applyState(scene, stateId) {
        clearState(scene)
        if (stateId === "edit")
            scene.editMode = true
        else if (stateId === "disabled")
            scene.disabled = true
        else if (stateId === "drop")
            scene.dropTarget = true
        else if (stateId === "urgent")
            scene.urgent = true
        else if (stateId === "pressed")
            scene.pressed = true
        else if (stateId === "hover")
            scene.hovered = true
        else if (stateId === "launching")
            scene.launching = true
        else if (stateId === "active")
            scene.active = true
        else if (stateId === "minimized")
            scene.minimized = true
        else if (stateId === "running")
            scene.running = true
        wait(0)
    }

    function previewIconAt(preview, index) {
        const slot = preview.panelSceneItem.iconDelegates.itemAt(index)
        verify(slot !== null)
        verify(slot.delegateItem !== null)
        return slot.delegateItem
    }

    function createPreview(styleId, stateId) {
        const preview = createTemporaryObject(previewComponent, testCase, {
            iconStyleDefinition: loadStyle(styleId),
            iconState: stateId || "normal",
            stateEntry: 1
        })
        verify(preview !== null)
        wait(0)
        return preview
    }

    function visualSignature(icon) {
        return JSON.stringify({
            styleId: icon.resolvedIconStyle.styleId,
            stateId: icon.visualState,
            state: icon.styleState,
            safeGlyphInset: icon.resolvedIconStyle.safeGlyphInset,
            glyph: {
                source: icon.resolvedIconSource,
                width: icon.glyphItem.width,
                height: icon.glyphItem.height,
                scale: icon.glyphItem.scale,
                opacity: icon.glyphItem.opacity
            },
            layerDefinitions: icon.iconStyleDefinition.layers,
            renderedLayerIds: {
                rear: icon.styleRearItem.renderedLayerIds,
                base: icon.styleBaseItem.renderedLayerIds,
                front: icon.styleFrontItem.renderedLayerIds
            },
            roleOpacity: {
                rear: icon.styleRearItem.roleOpacity,
                base: icon.styleBaseItem.roleOpacity,
                front: icon.styleFrontItem.roleOpacity
            }
        })
    }

    function test_loadedManifestProjectionIsUsable() {
        const style = loadStyle("metallic-blue")
        compare(style.id, "metallic-blue")
        compare(style.valid, true)
        compare(style.loadable, true)
        compare(style.format, "org.archdock.icon-style")
        compare(style.version, 1)
        compare(style.states.length, 11)
        compare(IconStyleResolver.usableDefinition(style), true)

        const scene = createScene("metallic-blue")
        compare(scene.iconStyleDefinition.id, "metallic-blue")
        compare(scene.iconStyleDefinition.valid, true)
        compare(scene.iconStyleDefinition.loadable, true)
        compare(IconStyleResolver.usableDefinition(
                    scene.iconStyleDefinition), true)
        compare(scene.resolvedIconStyle.valid, true)
        compare(scene.resolvedIconStyle.styleId, "metallic-blue")
    }

    function test_declaredFamiliesRenderAroundTheRealGlyph() {
        for (let index = 0; index < styleIds.length; ++index) {
            const scene = createScene(styleIds[index])
            compare(scene.resolvedIconStyle.styleId, styleIds[index])
            compare(scene.resolvedIconStyle.valid, true)
            compare(scene.styledLayersActive, true)
            compare(scene.resolvedIconSource, "applications-internet")
            compare(scene.resolvedIconStyle.replacementApplied, false)
            compare(scene.resolvedIconStyle.glyphPolicy, "original")
            verify(scene.styleBaseItem.layerCount > 0)
            verify(scene.styleFrontItem.layerCount > 0)
            verify(scene.glyphItem.width > 32)
            verify(scene.glyphItem.height > 32)
            verify(scene.tileTransformItem !== scene.glyphTransformItem)
            verify(scene.tileTransformItem !== scene.indicatorTransformItem)
            verify(scene.glyphTransformItem !== scene.indicatorTransformItem)

            wait(20)
            const image = grabImage(scene)
            compare(image.width, 96)
            compare(image.height, 96)
            verify(image.alpha(48, 48) > 0,
                   styleIds[index] + " did not render its center")
        }
    }

    function styleWithPolicy(styleId, policy, extras) {
        const style = loadStyle(styleId)
        style.glyphPolicy = policy
        const additions = extras || ({})
        for (const key of Object.keys(additions))
            style[key] = additions[key]
        return style
    }

    function sceneWithStyle(style, properties) {
        const values = { iconStyleDefinition: style }
        const additions = properties || ({})
        for (const key of Object.keys(additions))
            values[key] = additions[key]
        const scene = createTemporaryObject(
            iconSceneComponent, testCase, values)
        verify(scene !== null)
        wait(0)
        return scene
    }

    function test_shippedFamiliesNeverRecolorTheApplicationGlyph() {
        // Identity preservation is the default across every shipped family
        // and every declared state, not just the normal state.
        for (let styleIndex = 0; styleIndex < styleIds.length; ++styleIndex) {
            const scene = createScene(styleIds[styleIndex])
            for (let index = 0; index < stateIds.length; ++index) {
                applyState(scene, stateIds[index])
                const label = styleIds[styleIndex] + ":" + stateIds[index]
                compare(scene.glyphTreatment, "original", label)
                compare(scene.glyphIsMask, false, label)
                compare(scene.glyphItem.isMask, false, label)
                compare(scene.glyphItem.layer.enabled, false, label)
                compare(scene.resolvedIconSource, "applications-internet",
                        label)
            }
        }
    }

    function test_declaredTintAndMonochromeTreatmentsActuallyRender() {
        // A tint declared for an incompatible glyph must not be applied.
        const guarded = sceneWithStyle(styleWithPolicy("metallic-blue", {
            mode: "tinted", tint: "#3355ff", compatibleOnly: true
        }))
        compare(guarded.resolvedIconStyle.glyphPolicy, "tinted")
        compare(guarded.glyphTreatment, "original")
        compare(guarded.glyphItem.layer.enabled, false)

        // Declared compatible: the tint is genuinely applied.
        const tinted = sceneWithStyle(styleWithPolicy("metallic-blue", {
            mode: "tinted", tint: "#3355ff", compatibleOnly: false
        }))
        compare(tinted.glyphTreatment, "tinted")
        compare(tinted.glyphTint, "#3355ff")
        compare(tinted.glyphItem.layer.enabled, true)

        // Monochrome uses Kirigami's native mask path.
        const mono = sceneWithStyle(styleWithPolicy("dark-orb", {
            mode: "monochrome", tint: "#22ddaa", compatibleOnly: false
        }))
        compare(mono.glyphTreatment, "monochrome")
        compare(mono.glyphIsMask, true)
        compare(mono.glyphItem.isMask, true)
        compare(mono.glyphItem.color.toString(), "#22ddaa")
        compare(mono.glyphItem.layer.enabled, false)

        wait(20)
        const image = grabImage(mono)
        verify(image.alpha(48, 48) > 0, "monochrome glyph did not render")
    }

    function test_declaredMaskShapesTheTileComposite() {
        const maskUrl = Qt.resolvedUrl(
            "fixtures/icon-style-v1/assets/base.svg")
        const style = styleWithPolicy("metallic-blue",
                                      { mode: "original", compatibleOnly: true },
                                      { assetPaths: ({ "mask.svg": maskUrl }) })
        style.layers.mask = { id: "tile-mask", kind: "asset",
                              asset: "mask.svg", opacity: 1 }

        const scene = sceneWithStyle(style)
        verify(scene.styleMaskLayer !== null)
        compare(scene.styleMaskActive, true)
        tryVerify(function() {
            return scene.baseLayerItem.layer.enabled
        }, 3000, "declared mask did not shape the tile composite")

        // The mask shapes the tile only; the glyph keeps its own identity.
        compare(scene.styleAssetsFailed, false)
        compare(scene.glyphTreatment, "original")
        compare(scene.resolvedIconSource, "applications-internet")
    }

    function test_unloadableStyleAssetFallsBackToTheSafeOriginalGlyph() {
        const style = styleWithPolicy("neon-green", {
            mode: "monochrome", tint: "#22ddaa", compatibleOnly: false
        }, {
            assetPaths: ({
                "broken.png": "file:///nonexistent/archdock-broken.png"
            })
        })
        style.layers.base = [{ id: "broken-base", kind: "asset",
                               asset: "broken.png", opacity: 1 }]

        const scene = sceneWithStyle(style)
        tryVerify(function() { return scene.styleAssetsFailed }, 3000)
        compare(scene.styledLayersActive, false)
        compare(scene.glyphTreatment, "original")
        compare(scene.glyphItem.isMask, false)
        compare(scene.resolvedIconSource, "applications-internet")

        wait(20)
        const image = grabImage(scene)
        verify(image.alpha(48, 48) > 0,
               "safe original glyph did not render after asset failure")
    }

    function test_statePrecedenceIsDeterministic() {
        const scene = createScene("metallic-blue")
        scene.running = true
        scene.minimized = true
        scene.active = true
        scene.launching = true
        scene.hovered = true
        scene.pressed = true
        scene.urgent = true
        scene.dropTarget = true
        scene.disabled = true
        scene.editMode = true
        compare(scene.visualState, "edit")
        scene.editMode = false
        compare(scene.visualState, "disabled")
        scene.disabled = false
        compare(scene.visualState, "drop")
        scene.dropTarget = false
        compare(scene.visualState, "urgent")
        scene.urgent = false
        compare(scene.visualState, "pressed")
        scene.pressed = false
        compare(scene.visualState, "hover")
        scene.hovered = false
        compare(scene.visualState, "launching")
        scene.launching = false
        compare(scene.visualState, "active")
        scene.active = false
        compare(scene.visualState, "minimized")
        scene.minimized = false
        compare(scene.visualState, "running")
    }

    function test_allDeclaredStatesRemainLegible() {
        for (let styleIndex = 0; styleIndex < styleIds.length; ++styleIndex) {
            const scene = createScene(styleIds[styleIndex])
            for (let stateIndex = 0; stateIndex < stateIds.length;
                 ++stateIndex) {
                const requested = stateIds[stateIndex]
                applyState(scene, requested)
                compare(scene.visualState, requested)
                verify(Number(scene.styleState.glyphOpacity) >= 0.3)
                verify(Number(scene.styleState.glyphScale) >= 0.5)
                verify(scene.glyphItem.opacity >= 0.3)
                verify(scene.glyphItem.width > 0)
                verify(scene.glyphItem.height > 0)
                const image = grabImage(scene)
                verify(image.alpha(48, 48) > 0,
                       styleIds[styleIndex] + ":" + requested
                           + " lost its visible center")
            }
        }
    }

    function test_visualRegressionSignaturesAreDeterministicAndStateSensitive() {
        const first = createPreview("metallic-blue", "normal")
        const second = createPreview("metallic-blue", "normal")
        wait(20)
        const firstIcon = previewIconAt(first, 1)
        const secondNormalIcon = previewIconAt(second, 1)
        const firstNormal = visualSignature(firstIcon)
        const secondNormal = visualSignature(secondNormalIcon)
        compare(firstNormal, secondNormal)
        compare(firstIcon.visualState, "normal")
        compare(firstIcon.styleFrontItem.roleOpacity, 0.68)
        const firstBevel = findChild(
            firstIcon, "icon-style-layer-blue-bevel")
        verify(firstBevel !== null)
        fuzzyCompare(firstBevel.opacity, 0.82 * 0.68, 0.0001)

        second.iconState = "hover"
        wait(20)
        const secondIcon = previewIconAt(second, 1)
        compare(secondIcon.visualState, "hover")
        compare(secondIcon.styleState.id, "hover")
        compare(secondIcon.styleFrontItem.roleOpacity, 1)
        verify(secondIcon.glyphItem.scale > firstIcon.glyphItem.scale)
        const secondBevel = findChild(
            secondIcon, "icon-style-layer-blue-bevel")
        verify(secondBevel !== null)
        fuzzyCompare(secondBevel.opacity, 0.82, 0.0001)
        const secondHover = visualSignature(secondIcon)
        verify(firstNormal !== secondHover)

        const familySignatures = []
        for (let index = 0; index < styleIds.length; ++index) {
            const preview = createPreview(styleIds[index], "active")
            wait(20)
            familySignatures.push(visualSignature(previewIconAt(preview, 1)))
        }
        for (let left = 0; left < familySignatures.length; ++left) {
            for (let right = left + 1; right < familySignatures.length;
                 ++right)
                verify(familySignatures[left] !== familySignatures[right])
        }
    }

    function test_livePreviewUsesEveryDeclaredFamily() {
        for (let index = 0; index < styleIds.length; ++index) {
            const preview = createTemporaryObject(previewComponent, testCase, {
                iconStyleDefinition: loadStyle(styleIds[index]),
                iconState: "hover",
                stateEntry: 1
            })
            verify(preview !== null)
            wait(0)
            const icon = previewIconAt(preview, 1)
            compare(icon.resolvedIconStyle.styleId, styleIds[index])
            compare(icon.resolvedIconSource, "applications-internet")
            compare(icon.visualState, "hover")
            compare(icon.styledLayersActive, true)

            preview.previewMode = "free"
            wait(0)
            const freeIcon = previewIconAt(preview, 1)
            compare(freeIcon.resolvedIconStyle.styleId, styleIds[index])
            compare(freeIcon.styledLayersActive, true)
        }
    }
}
