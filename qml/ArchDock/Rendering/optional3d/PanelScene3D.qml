import QtQuick
import QtQuick.Window
import QtQuick3D
import "../GizmoMath.js" as GizmoMath
import "../LayoutEngine.js" as LayoutEngine
import "../PlatformGeometry.js" as PlatformGeometry

// The true-3D panel renderer (Qt Quick 3D), loaded only when the system can
// draw it. It draws the theme's own platform mesh, or a platform generated
// along the panel's layout in the look's colours, with every icon standing
// upright on a pedestal and facing the viewer. It projects each icon back to
// the screen for input, popup anchors and the folder curve, runs the 3D
// parts of opening and closing, and in desktop edit mode carries the
// move, rotate and scale handles and lets the body be dragged to tilt it.
Item {
    id: root

    required property var sceneDefinition
    required property var resources
    property url textureSource: ""
    property var entryGeometry: []
    property var entryVisuals: []
    property string quality: "medium"
    property real panelOpacity: 1
    property bool sceneConcealed: false
    property real layoutAngle: 0
    property real collapseProgress: 0
    property string mechanism: "open"
    // A radial collapse closes the platform toward its centre like an iris,
    // while the theme's declared parts make their own moves, and stops at a
    // small ring the pointer can still find to open the panel again.
    readonly property real radialCollapseKept: 0.2
    readonly property real radialCollapseScale: mechanism === "collapse-radial"
        ? 1 - (1 - radialCollapseKept) * Math.max(0, Math.min(1, collapseProgress)) : 1
    property bool hovered: false
    property bool reducedMotion: false
    property real glowIntensity: 1
    readonly property bool motionAllowed: rendererReady && !sceneConcealed && visible && panelOpacity > 0
    readonly property real emissionScale: Math.max(0, Math.min(2, glowIntensity)) * (hovered ? 1.18 : 1)
    readonly property var panelParts: partsForScope("panel")
    readonly property var entryParts: partsForScope("entry")
    readonly property bool partsReady: {
        const definitions = (sceneDefinition || ({})).parts || []
        const data = (resources || ({})).parts || []
        return definitions.length === data.length && data.every(function(part) {
            return part && part.mesh && part.mesh.format === "org.archdock.mesh"
                && part.material && part.material.format === "org.archdock.material"
        })
    }

    readonly property string effectiveQuality:
        ["low", "medium", "high"].includes(quality) ? quality : "medium"
    readonly property real resolutionScale: effectiveQuality === "low" ? 0.5
        : effectiveQuality === "high" ? 1 : 0.75
    readonly property int textureLimit: effectiveQuality === "low" ? 1024
        : effectiveQuality === "high" ? 2048 : 1536
    readonly property real boundedScale: Math.min(resolutionScale,
        textureLimit / Math.max(1, width, height))
    readonly property int targetWidth: Math.max(1, Math.ceil(width * boundedScale))
    readonly property int targetHeight: Math.max(1, Math.ceil(height * boundedScale))
    // A look whose theme ships no 3D mesh stands on a platform generated from
    // the panel's own layout (`sceneDefinition.generated`), in the look's own
    // colours: the palette sampled from its artwork when it is drawn from
    // artwork, or the colours its 2D surface is drawn with.
    property real layoutTrackRadius: 0
    property string appearance: ""
    property string customColor: ""
    readonly property var generatedSpec: (sceneDefinition || ({})).generated || null
    readonly property var generatedShape: generatedSpec
        ? PlatformGeometry.shapeForLayout(generatedSpec.layout, generatedSpec.polygonSides) : null
    readonly property var generatedPalette: generatedSpec && generatedSpec.palette
        ? generatedSpec.palette
        : PlatformGeometry.paletteFromStyle(LayoutEngine.themeStyle(appearance, customColor, 48))
    // The platform is rebuilt only when what it is built from changes: a
    // string compares by value, so an edit elsewhere in the scene's
    // definition leaves the mesh, and the GPU buffers made from it, alone.
    readonly property string generatedKey: generatedShape ? JSON.stringify({
        shape: generatedShape, band: bounded("band", 0.11, PlatformGeometry.minimumBand,
                                             PlatformGeometry.maximumBand),
        bend: bounded("bend", 0, -1, 1), palette: generatedPalette }) : ""
    readonly property var generatedPlatform: generatedKey.length > 0
        ? PlatformGeometry.platform(JSON.parse(generatedKey)) : null
    readonly property var platformMesh: generatedSpec ? generatedPlatform
        : (resources || ({})).mesh || null
    // A generated platform's pedestals take its top's colour, and its rim
    // glows in the look's glow colour.
    readonly property var platformMaterial: {
        const material = Object.assign({}, (resources || ({})).material || ({}))
        if (generatedSpec && generatedPalette && generatedPalette.top) {
            const top = generatedPalette.top
            material.baseColor = Qt.rgba(top[0], top[1], top[2], 1)
        }
        if (generatedSpec && generatedPalette && generatedPalette.glow) {
            const glow = generatedPalette.glow
            material.rimEmissiveColor = Qt.rgba(glow[0], glow[1], glow[2], 1)
            material.rimEmissiveStrength = Number(generatedPalette.glowStrength || 0)
        }
        return material
    }
    // The theme's own surface texture, for everything drawn in its material:
    // its meshes map it from above, so a pedestal or a part takes the colour
    // of the platform where it stands.
    readonly property Texture themeTexture: textureRequired && textureReady ? surfaceTexture : null
    // Turns an icon, from inside the entries' frame, to face the camera: the
    // frame's own rotation undone, the camera's taken.
    readonly property quaternion iconFacing: entryFrame.sceneRotation.inverted()
        .times(camera.sceneRotation)
    // The column every icon stands on; it takes the platform's material.
    readonly property var pedestalMesh: PlatformGeometry.pedestal(24)
    readonly property int pedestalTriangles: pedestalMesh.indexes.length / 3
    readonly property bool resourcesReady: resources !== null
        && resources.material !== undefined
        && resources.material.format === "org.archdock.material"
        && platform.meshReady && partsReady
        && (generatedSpec !== null || (resources.iconMesh !== undefined && iconResource.meshReady))
    readonly property bool textureRequired: Boolean(sceneDefinition && sceneDefinition.texture)
    readonly property bool textureReady: !textureRequired || textureImage.status === Image.Ready
    readonly property bool geometryWithinBudget: triangleCount * 3 <= Math.min(262144,
        Number((resources || ({})).indexBudget || 262144))
    readonly property bool rendererReady: resourcesReady && textureReady && geometryWithinBudget
    readonly property string errorReason: !resourcesReady ? "scene3d-mesh-unavailable"
        : textureRequired && textureImage.status === Image.Error ? "scene3d-texture-unavailable"
        : !geometryWithinBudget ? "scene3d-resource-limit"
        : !textureReady ? "renderer-loading" : ""
    readonly property int entryTriangleCount: {
        let count = 0
        for (let index = 0; index < entryGeometry.length; ++index) {
            const visual = entryVisuals[index] ? entryVisuals[index].meshVisualItem : null
            count += 2 // Glyph.
            if (visual && !visual.tileRenderingEnabled) continue
            count += 2 // Tile.
            if (bounded("iconElevation", 0.3, 0, 2) > 0)
                count += pedestalTriangles + (generatedSpec ? 0 : iconResource.triangleCount)
            if (!visual || !visual.customTileActive) count += partTriangles(entryParts)
        }
        return count
    }
    readonly property int triangleCount: platform.triangleCount
        + partTriangles(panelParts) + entryTriangleCount
    readonly property var qualityState: ({ quality: effectiveQuality,
        targetWidth: targetWidth, targetHeight: targetHeight,
        samples: effectiveQuality === "low" ? 1 : effectiveQuality === "high" ? 4 : 2 })
    readonly property var viewport: view
    readonly property bool frameRendered: completedFrameObserved
        || view.renderStats.frameTime > 0
    property bool completedFrameObserved: false
    property var projectedEntryGeometry: []
    // Where the dock is centred on screen: the platform's middle at its top,
    // the dock a folder opens away from.
    property point projectedCentre: Qt.point(width / 2, height / 2)
    // The platform's reach from its centre across and up, in its own units:
    // 1 for a ring or a theme's mesh; an ellipse is wider than it is tall.
    readonly property real platformReachX: generatedPlatform ? generatedPlatform.reachX : 1
    readonly property real platformReachY: generatedPlatform ? generatedPlatform.reachY : 1
    readonly property real platformScale: Math.min(width / platformReachX, height / platformReachY) * 0.40
    // Where the icons stand: a generated platform's flat top is level along
    // its centre line even when bent; a theme's top is its highest point.
    readonly property real platformTop: (generatedSpec ? PlatformGeometry.topHeight
        : Math.max(0, ...((platformMesh || {}).positions || []).map(p => Number(p[2]))))
        * platformScale * bounded("thickness", 1, 0.1, 4)
    // The ring the icons stand on: the middle of the platform's flat top.
    readonly property real entryTrackRadius: platformScale * (generatedPlatform
        ? generatedPlatform.track : bounded("entryRadius", 0.84, 0.1, 1))

    // The scene's own transform from the Panels > 3D page. Pitch and yaw orbit
    // the camera, as they always have; roll turns it about its view axis.
    // Position moves the platform and its icons by a fraction of the room the
    // panel has around them, and scale resizes them, so no value can carry
    // the platform out of its panel. Everything is drawn, picked and pressed
    // through the same nodes, so input and anchors follow without a copy.
    readonly property bool gizmoDragging: dragStart !== null

    // Desktop 3D editing. In edit mode the platform carries Blender-style
    // handles, one set per operation: arrows move it along X (red), Y (green)
    // and Z (blue); the red and green rings tilt it (pitch and yaw) and the
    // white ring turns the view (roll); the cube handles scale it. Ctrl snaps,
    // Shift slows the drag for fine work, and Esc or the right button cancels.
    // A finished drag is reported once through transformEdited(); the host
    // keeps it as a draft until the edit is applied or cancelled.
    property bool editMode: false
    property string gizmoMode: "move"
    signal transformEdited(var values)
    // While a handle is dragged, and until the panel's settings come back with
    // the result, these values stand in for the saved ones.
    property var dragOverride: null
    property var dragStart: null
    readonly property real gizmoLength: platformScale * 0.55
    readonly property var transformKeys: ({
        cameraPitch: "scene3DCameraPitch", cameraYaw: "scene3DCameraYaw", roll: "scene3DRoll",
        positionX: "scene3DPositionX", positionY: "scene3DPositionY", positionZ: "scene3DPositionZ",
        scale: "scene3DScale"
    })
    function transformValue(key, fallback, minimum, maximum) {
        const override = dragOverride && dragOverride[key] !== undefined ? Number(dragOverride[key]) : NaN
        return Number.isFinite(override) ? Math.max(minimum, Math.min(maximum, override))
            : bounded(key, fallback, minimum, maximum)
    }
    function wrapDegrees(value) { return ((value + 180) % 360 + 360) % 360 - 180 }
    function savedTransform() {
        return {
            cameraPitch: bounded("cameraPitch", 25, -60, 60), cameraYaw: bounded("cameraYaw", 0, -180, 180),
            roll: bounded("roll", 0, -180, 180), positionX: bounded("positionX", 0, -1, 1),
            positionY: bounded("positionY", 0, -1, 1), positionZ: bounded("positionZ", 0, -1, 1),
            scale: bounded("scale", 1, 0.5, 1.25)
        }
    }
    function gizmoHandleAt(x, y) {
        const hits = view.pickAll(x, y)
        for (let index = 0; index < hits.length; ++index) {
            const name = hits[index].objectHit ? String(hits[index].objectHit.objectName) : ""
            const match = /^gizmo-(move|rotate|scale)-(x|y|z|view)/.exec(name)
            if (match && match[1] === gizmoMode)
                return { mode: match[1], axis: match[2] }
        }
        return null
    }
    function beginGizmoDrag(handle, x, y) {
        const origin = gizmo.scenePosition
        const centre = view.mapFrom3DScene(origin)
        const values = savedTransform()
        if (dragOverride)
            for (const key of Object.keys(dragOverride)) values[key] = Number(dragOverride[key])
        dragStart = { mode: handle.mode, axis: handle.axis, pointer: Qt.point(x, y),
                      centre: Qt.point(centre.x, centre.y), origin: origin, values: values,
                      endOn: handle.mode === "move" && gizmoAxisEndOn(handle.axis) }
        dragOverride = Object.assign({}, values)
    }
    function gizmoAxisVector(axis) {
        return axis === "x" ? Qt.vector3d(1, 0, 0)
            : axis === "y" ? Qt.vector3d(0, 1, 0) : Qt.vector3d(0, 0, 1)
    }
    // Whether a move arrow points at the viewer, as drawn now.
    function gizmoAxisEndOn(axis) {
        const origin = gizmo.scenePosition
        const from = view.mapFrom3DScene(origin)
        const to = view.mapFrom3DScene(origin.plus(gizmoAxisVector(axis).times(gizmoLength)))
        return GizmoMath.axisEndOn(Qt.point(from.x, from.y), Qt.point(to.x, to.y))
    }
    // The move arrows drawn end-on, kept current while editing so the
    // toolbar can say how to drag them.
    property var endOnAxes: []
    function updateEndOnAxes() {
        const axes = editMode && rendererReady
            ? ["x", "y", "z"].filter(function(axis) { return gizmoAxisEndOn(axis) }) : []
        if (axes.join() !== endOnAxes.join()) endOnAxes = axes
    }
    function updateGizmoDrag(x, y, modifiers) {
        const start = dragStart
        const settings = GizmoMath.modifierSettings(modifiers)
        const delta = Qt.point((x - start.pointer.x) * settings.precision,
                               (y - start.pointer.y) * settings.precision)
        const pointer = Qt.point(start.pointer.x + delta.x, start.pointer.y + delta.y)
        const next = Object.assign({}, start.values)
        if (start.mode === "orbit") {
            // The platform's body: across turns it, up and down tilts it.
            const turned = GizmoMath.orbit(start.values.cameraPitch, start.values.cameraYaw,
                                           delta, settings.snap)
            next.cameraPitch = turned.pitch
            next.cameraYaw = turned.yaw
        } else if (start.mode === "move") {
            const axis = gizmoAxisVector(start.axis)
            const from = view.mapFrom3DScene(start.origin)
            const to = view.mapFrom3DScene(start.origin.plus(axis.times(gizmoLength)))
            // An arrow seen end-on has no direction on screen: up and down
            // move it instead of a drag that would do nothing.
            const travel = start.endOn ? GizmoMath.endOnTravel(delta, gizmoLength, 120)
                : GizmoMath.axisTravel(Qt.point(from.x, from.y), Qt.point(to.x, to.y),
                                       gizmoLength, delta)
            // Scene units back to the fractions the settings store.
            const perspective = (cameraDistance - targetDepth) / cameraDistance
            const span = start.axis === "x" ? roomX * perspective
                : start.axis === "y" ? roomY * perspective : platformScale * 0.5
            const key = start.axis === "x" ? "positionX" : start.axis === "y" ? "positionY" : "positionZ"
            let value = span > 1e-6 ? start.values[key] + travel / span : start.values[key]
            if (settings.snap) value = GizmoMath.snapped(value, 0.05)
            next[key] = GizmoMath.clamp(value, -1, 1)
        } else if (start.mode === "rotate") {
            if (start.axis === "x") {
                let pitch = start.values.cameraPitch - delta.y * 0.25
                if (settings.snap) pitch = GizmoMath.snapped(pitch, 15)
                next.cameraPitch = GizmoMath.clamp(pitch, -60, 60)
            } else {
                let angle = start.axis === "y"
                    ? start.values.cameraYaw + delta.x * 0.25
                    : start.values.roll + GizmoMath.sweptDegrees(start.centre, start.pointer, pointer)
                if (settings.snap) angle = GizmoMath.snapped(angle, 15)
                next[start.axis === "y" ? "cameraYaw" : "roll"] = wrapDegrees(angle)
            }
        } else {
            let scale = start.values.scale * GizmoMath.scaleRatio(start.centre, start.pointer, pointer)
            if (settings.snap) scale = GizmoMath.snapped(scale, 0.05)
            next.scale = GizmoMath.clamp(scale, 0.5, 1.25)
        }
        dragOverride = next
    }
    function finishGizmoDrag() {
        const start = dragStart
        const result = dragOverride || ({})
        dragStart = null
        const values = {}
        for (const key of Object.keys(transformKeys))
            if (Math.abs(Number(result[key]) - Number(start.values[key])) > 1e-6)
                values[transformKeys[key]] = Number(result[key])
        if (Object.keys(values).length === 0) {
            dragOverride = null
            return
        }
        overrideRelease.restart()
        transformEdited(values)
    }
    function cancelGizmoDrag() {
        dragStart = null
        dragOverride = null
    }
    // The override gives way once the settings carry the edited values, or
    // after a while if they never do.
    onSceneDefinitionChanged: {
        if (dragStart || !dragOverride) return
        const saved = savedTransform()
        if (Object.keys(dragOverride).every(function(key) {
                return Math.abs(Number(saved[key]) - Number(dragOverride[key])) < 1e-3 }))
            dragOverride = null
    }
    onEditModeChanged: if (!editMode) cancelGizmoDrag()
    Timer {
        id: overrideRelease
        interval: 3000
        onTriggered: if (!root.dragStart) root.dragOverride = null
    }
    readonly property bool transitionsEnabled: !reducedMotion && !gizmoDragging
        && (sceneDefinition || ({})).transitions !== false
    readonly property real cameraDistance: Math.max(1, height)
        / (2 * Math.tan(bounded("fieldOfView", 40, 20, 70) * Math.PI / 360))
    // The platform and its icons reach a little past the platform's radius.
    readonly property real sceneExtentX: platformScale * platformReachX * 1.05
    readonly property real sceneExtentY: platformScale * platformReachY * 1.05
    readonly property real targetScale: transformValue("scale", 1, 0.5, 1.25)
    readonly property real fitLimit: Math.min(width / 2 / Math.max(1, sceneExtentX),
                                              height / 2 / Math.max(1, sceneExtentY))
    // Nearer is larger: depth stops where the platform would outgrow the panel.
    readonly property real targetDepth: Math.min(transformValue("positionZ", 0, -1, 1) * platformScale * 0.5,
        cameraDistance * Math.max(0, 1 - targetScale / Math.max(targetScale, fitLimit)))
    readonly property real apparentScale: targetScale * cameraDistance
        / Math.max(1, cameraDistance - targetDepth)
    readonly property real roomX: Math.max(0, width / 2 - sceneExtentX * apparentScale)
    readonly property real roomY: Math.max(0, height / 2 - sceneExtentY * apparentScale)
    readonly property vector3d targetPosition: Qt.vector3d(
        transformValue("positionX", 0, -1, 1) * roomX * (cameraDistance - targetDepth) / cameraDistance,
        transformValue("positionY", 0, -1, 1) * roomY * (cameraDistance - targetDepth) / cameraDistance,
        targetDepth)
    readonly property real targetPitch: transformValue("cameraPitch", 25, -60, 60)
    readonly property real targetYaw: transformValue("cameraYaw", 0, -180, 180)
    readonly property real targetRoll: transformValue("roll", 0, -180, 180)
    // What is drawn. It eases toward the targets when transitions are on and
    // jumps to them otherwise. One frame-driven easing instead of a Behavior
    // per value: Qt creates a Behavior's animation on its first use, which
    // would make the scene's objects depend on what it has been through.
    property real shownPitch: 0
    property real shownYaw: 0
    property real shownRoll: 0
    property vector3d shownPosition: Qt.vector3d(0, 0, 0)
    property real shownScale: 1
    // Exact: the easing lands each value on its target, then stops.
    readonly property bool transformSettled: shownPitch === targetPitch
        && shownYaw === targetYaw && shownRoll === targetRoll && shownScale === targetScale
        && shownPosition.x === targetPosition.x && shownPosition.y === targetPosition.y
        && shownPosition.z === targetPosition.z
    function angleStep(from, to) { return ((to - from) % 360 + 540) % 360 - 180 }
    function settleTransform() {
        shownPitch = targetPitch
        shownYaw = targetYaw
        shownRoll = targetRoll
        shownPosition = targetPosition
        shownScale = targetScale
    }
    function followTransform() { if (!transitionsEnabled) settleTransform() }
    function stepTransform(seconds) {
        // About 95 percent of the way in a quarter of a second, the short way
        // round for angles; within a hair of the target it lands on it.
        const share = 1 - Math.exp(-12 * Math.max(0, seconds))
        const toward = function(from, to, step, epsilon) {
            return Math.abs(step) < epsilon ? to : from + step * share
        }
        shownPitch = toward(shownPitch, targetPitch, targetPitch - shownPitch, 1e-3)
        shownYaw = toward(shownYaw, targetYaw, angleStep(shownYaw, targetYaw), 1e-3)
        shownRoll = toward(shownRoll, targetRoll, angleStep(shownRoll, targetRoll), 1e-3)
        const offset = targetPosition.minus(shownPosition)
        shownPosition = offset.length() < 1e-3 ? targetPosition
            : shownPosition.plus(offset.times(share))
        shownScale = toward(shownScale, targetScale, targetScale - shownScale, 1e-4)
    }
    onTargetPitchChanged: followTransform()
    onTargetYawChanged: followTransform()
    onTargetRollChanged: followTransform()
    onTargetPositionChanged: followTransform()
    onTargetScaleChanged: followTransform()
    onTransitionsEnabledChanged: followTransform()
    Component.onCompleted: settleTransform()
    FrameAnimation {
        running: root.transitionsEnabled && !root.transformSettled
        onTriggered: root.stepTransform(frameTime)
    }
    // A slow rise and fall, off unless chosen, and never under reduced motion.
    readonly property bool floatRunning: (sceneDefinition || ({})).float === true
        && !reducedMotion && motionAllowed && !gizmoDragging
    property real floatOffset: 0
    onFloatRunningChanged: if (!floatRunning) floatOffset = 0
    SequentialAnimation on floatOffset {
        running: root.floatRunning
        loops: Animation.Infinite
        NumberAnimation { to: root.platformScale * 0.03; duration: 1800; easing.type: Easing.InOutSine }
        NumberAnimation { to: -root.platformScale * 0.03; duration: 3600; easing.type: Easing.InOutSine }
        NumberAnimation { to: 0; duration: 1800; easing.type: Easing.InOutSine }
    }

    function containsInputPoint(point) {
        if (!rendererReady) return false
        const hit = view.pick(point.x, point.y).objectHit
        if (!hit) return false
        if (hit !== platform) return true
        // Qt picks custom geometry against its bounding volume. Refine that
        // native coarse hit against the validated mesh so ring holes stay empty.
        const origin = platform.mapPositionFromScene(view.mapTo3DScene(Qt.vector3d(point.x, point.y, 0)))
        const far = platform.mapPositionFromScene(view.mapTo3DScene(Qt.vector3d(point.x, point.y, 1000)))
        const direction = far.minus(origin).normalized()
        const mesh = platformMesh, vertices = mesh.positions, indexes = mesh.indexes
        for (let i = 0; i < indexes.length; i += 3) {
            const a = vertices[indexes[i]], b = vertices[indexes[i+1]], c = vertices[indexes[i+2]]
            const ab = Qt.vector3d(b[0]-a[0], b[1]-a[1], b[2]-a[2])
            const ac = Qt.vector3d(c[0]-a[0], c[1]-a[1], c[2]-a[2])
            const cross = direction.crossProduct(ac), determinant = ab.dotProduct(cross)
            if (Math.abs(determinant) < 1e-8) continue
            const offset = origin.minus(Qt.vector3d(a[0], a[1], a[2]))
            const u = offset.dotProduct(cross) / determinant
            if (u < 0 || u > 1) continue
            const q = offset.crossProduct(ab), v = direction.dotProduct(q) / determinant
            if (v < 0 || u + v > 1) continue
            if (ac.dotProduct(q) / determinant >= 0) return true
        }
        return false
    }
    // "Along the dock": the platform's own icon track at the folder's height,
    // moved `outward` screen pixels out of the dock and projected like the
    // icons, as `count` samples spanning `span` radians around the folder.
    // `scale` is the perspective size there relative to the folder's own.
    function folderTrackSamples(entryIndex, outward, span, count) {
        const node = worldEntries.objectAt(entryIndex)
        if (!rendererReady || !node || !node.inputAnchor || count < 2) return []
        const anchor = node.inputAnchor
        // An icon stands toward the camera's up from where it stands on the
        // track, by the same offset for every icon: the icons' centres lie on
        // the track moved by that offset, and so does the outer curve.
        const foot = sceneContent.mapPositionFromScene(node.scenePosition)
        const local = sceneContent.mapPositionFromScene(anchor.scenePosition)
        const lift = local.minus(foot)
        const radius = Math.hypot(foot.x, foot.y)
        if (radius <= 0) return []
        // How far a step out of the dock appears on screen at the folder:
        // foreshortened where the platform tilts away from the viewer.
        const here = view.mapFrom3DScene(anchor.scenePosition)
        const step = radius * 0.05
        const further = view.mapFrom3DScene(sceneContent.mapPositionToScene(Qt.vector3d(
            foot.x * (radius + step) / radius, foot.y * (radius + step) / radius, foot.z).plus(lift)))
        const pixelsPerUnit = Math.hypot(further.x - here.x, further.y - here.y) / step
        const outer = radius + Math.max(0, outward) / Math.max(0.01, pixelsPerUnit)
        const angle = Math.atan2(foot.y, foot.x)
        const depth = here.z
        const result = []
        for (let index = 0; index < count; ++index) {
            const turn = angle - span / 2 + span * index / (count - 1)
            const point = view.mapFrom3DScene(sceneContent.mapPositionToScene(
                Qt.vector3d(outer * Math.cos(turn), outer * Math.sin(turn), foot.z).plus(lift)))
            if (!Number.isFinite(point.x) || !Number.isFinite(point.y) || point.z <= 0) continue
            result.push({ x: point.x, y: point.y, scale: depth / point.z })
        }
        return result
    }
    function updateProjection() {
        if (!rendererReady || width <= 0 || height <= 0) return
        const result = []
        for (let i = 0; i < worldEntries.count; ++i) {
            const node = worldEntries.objectAt(i)
            if (!node || !node.inputAnchor) return
            // The icon's resting square, as the camera sees it.
            const anchor = node.inputAnchor, half = node.size / 2
            const points = [[-half, -half], [half, -half], [half, half], [-half, half]].map(p =>
                view.mapFrom3DScene(anchor.mapPositionToScene(Qt.vector3d(p[0], p[1], 0))))
            const left = Math.min(...points.map(p => p.x)), top = Math.min(...points.map(p => p.y))
            const right = Math.max(...points.map(p => p.x)), bottom = Math.max(...points.map(p => p.y))
            if (![left, top, right, bottom].every(Number.isFinite) || right <= left || bottom <= top) return
            const center = view.mapFrom3DScene(anchor.scenePosition)
            result.push({x: left, y: top, width: right-left, height: bottom-top,
                centerX: center.x, centerY: center.y, depth: center.z})
        }
        if (JSON.stringify(result) !== JSON.stringify(projectedEntryGeometry)) projectedEntryGeometry = result
        // The dock's centre as drawn: the platform's middle at its top. A
        // folder's contents open away from it, not from the icons' heads.
        const centre = view.mapFrom3DScene(sceneContent.mapPositionToScene(Qt.vector3d(0, 0, platformTop)))
        if (Number.isFinite(centre.x) && Number.isFinite(centre.y)
                && (centre.x !== projectedCentre.x || centre.y !== projectedCentre.y))
            projectedCentre = Qt.point(centre.x, centre.y)
    }

    // RenderStats throttles change notifications. A static scene can stop
    // before that interval, so also inspect it after an actual window frame.
    function observeCompletedFrame() {
        if (view.visible && view.renderStats.frameTime > 0)
            completedFrameObserved = true
    }

    Connections {
        target: root.Window.window
        function onFrameSwapped() {
            if (!root.completedFrameObserved)
                Qt.callLater(root.observeCompletedFrame)
            Qt.callLater(root.updateProjection)
            if (root.editMode)
                Qt.callLater(root.updateEndOnAxes)
        }
    }

    function bounded(key, fallback, minimum, maximum) {
        const value = Number((sceneDefinition || ({}))[key])
        return Number.isFinite(value) ? Math.max(minimum, Math.min(maximum, value)) : fallback
    }

    function partsForScope(scope) {
        const definitions = (sceneDefinition || ({})).parts || []
        const data = (resources || ({})).parts || []
        const result = []
        for (let index = 0; index < definitions.length; ++index) {
            if (definitions[index].scope === scope)
                result.push({ definition: definitions[index], resources: data[index] || ({}) })
        }
        return result
    }
    function partTriangles(parts) {
        return parts.reduce(function(count, part) {
            return count + Number((part.resources.mesh || ({})).indexes?.length || 0) / 3
        }, 0)
    }
    function partOpenAmount(part) {
        return part.mechanism === mechanism ? Math.max(0, Math.min(1, 1 - collapseProgress)) : 1
    }
    function number(object, key, fallback) {
        const value = Number((object || ({}))[key])
        return Number.isFinite(value) ? value : fallback
    }

    Image {
        id: textureImage
        visible: false
        source: root.textureSource
        asynchronous: true
        sourceSize: Qt.size(1024, 1024)
        cache: false
    }

    Row {
        objectName: "mesh-gizmo-toolbar"
        visible: root.editMode && root.rendererReady
        anchors.top: parent.top
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4
        z: 3
        Repeater {
            model: [{ mode: "move", label: qsTr("Move") }, { mode: "rotate", label: qsTr("Rotate") },
                    { mode: "scale", label: qsTr("Scale") }]
            delegate: Rectangle {
                id: modeButton
                required property var modelData
                objectName: "mesh-gizmo-mode-" + modelData.mode
                width: modeLabel.implicitWidth + 16
                height: modeLabel.implicitHeight + 8
                radius: 4
                color: root.gizmoMode === modelData.mode ? "#2f6f88" : "#cc1b2831"
                border.color: "#73cfe7"
                Accessible.role: Accessible.Button
                Accessible.name: modelData.label
                Accessible.onPressAction: root.gizmoMode = modelData.mode
                Text {
                    id: modeLabel
                    anchors.centerIn: parent
                    text: modeButton.modelData.label
                    color: "#f4f8fb"
                    font.pixelSize: 11
                }
                TapHandler { onTapped: root.gizmoMode = modeButton.modelData.mode }
            }
        }
    }
    // How to drag what is not obvious: the body itself, and an arrow that
    // points at the viewer.
    Text {
        objectName: "mesh-gizmo-hint"
        visible: root.editMode && root.rendererReady
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(implicitWidth, parent.width - 8)
        z: 3
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        color: "#f4f8fb"
        style: Text.Outline
        styleColor: "#cc1b2831"
        font.pixelSize: 11
        text: root.gizmoMode === "move" && root.endOnAxes.length > 0
            ? qsTr("The %1 arrow points at you: drag it up or down.")
                .arg(root.endOnAxes.map(function(axis) { return axis.toUpperCase() }).join(", "))
            : qsTr("Drag the platform to tilt and turn it.")
    }

    MouseArea {
        objectName: "mesh-gizmo-input"
        anchors.fill: parent
        z: 2
        // While editing, the whole panel belongs to the handles: a press that
        // misses them does nothing, so no application starts by accident.
        enabled: root.editMode && root.rendererReady
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        onPressed: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                root.cancelGizmoDrag()
                return
            }
            // A handle first; otherwise a press on the platform or its icons
            // takes the body, to tilt and turn it.
            const handle = root.gizmoHandleAt(mouse.x, mouse.y)
            if (handle)
                root.beginGizmoDrag(handle, mouse.x, mouse.y)
            else if (root.containsInputPoint(Qt.point(mouse.x, mouse.y)))
                root.beginGizmoDrag({ mode: "orbit", axis: "body" }, mouse.x, mouse.y)
        }
        onPositionChanged: function(mouse) {
            if (root.dragStart)
                root.updateGizmoDrag(mouse.x, mouse.y, mouse.modifiers)
        }
        onReleased: function(mouse) {
            if (mouse.button !== Qt.LeftButton || !root.dragStart)
                return
            // The keys held when the button is let go decide the result.
            root.updateGizmoDrag(mouse.x, mouse.y, mouse.modifiers)
            root.finishGizmoDrag()
        }
        onCanceled: root.cancelGizmoDrag()
    }
    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: root.dragStart !== null
        onActivated: root.cancelGizmoDrag()
    }

    View3D {
        id: view
        anchors.fill: parent
        visible: root.rendererReady && !root.sceneConcealed
        opacity: root.panelOpacity
        explicitTextureWidth: root.targetWidth
        explicitTextureHeight: root.targetHeight
        camera: camera
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: root.effectiveQuality === "low"
                ? SceneEnvironment.NoAA : SceneEnvironment.MSAA
            antialiasingQuality: root.effectiveQuality === "high"
                ? SceneEnvironment.High : SceneEnvironment.Medium
        }
        Texture {
            id: surfaceTexture
            sourceItem: root.Window.window && textureImage.Window.window === root.Window.window
                ? textureImage : null
            generateMipmaps: true
            mipFilter: Texture.Linear
        }
        Node {
            objectName: "mesh-camera-rig"
            eulerRotation.x: root.shownPitch
            eulerRotation.y: root.shownYaw
            PerspectiveCamera {
                id: camera
                fieldOfView: root.bounded("fieldOfView", 40, 20, 70)
                z: root.cameraDistance
                eulerRotation.z: root.shownRoll
                clipNear: 1
                clipFar: Math.max(100, z * 5)
                // Camera-local coordinates preserve the logical pixel centers
                // without querying viewport matrices during scene construction.

            }
        }
        Node {
            id: sceneContent
            objectName: "mesh-scene-content"
            position: Qt.vector3d(root.shownPosition.x, root.shownPosition.y,
                                  root.shownPosition.z + root.floatOffset)
            scale: Qt.vector3d(root.shownScale * root.radialCollapseScale,
                               root.shownScale * root.radialCollapseScale,
                               root.shownScale * root.radialCollapseScale)
        Node {
            id: entryFrame
            eulerRotation.z: -root.layoutAngle
        Repeater3D {
            id: worldEntries
            // A count model preserves nodes and textures during rotation.
            model: root.geometryWithinBudget ? root.entryGeometry.length : 0
            delegate: Node {
                id: entryNode
                required property int index
                objectName: "mesh-entry-" + index
                readonly property var rect: root.entryGeometry[index] || ({})
                readonly property var entry: root.entryVisuals[index] || null
                readonly property var visual: entry ? entry.meshVisualItem : null
                readonly property var iconMotion: root.motionAllowed && entry ? entry.iconMotion : ({})
                readonly property var glyphMotion: root.motionAllowed && entry ? entry.glyphMotion : ({})
                readonly property var tileMotion: root.motionAllowed && entry ? entry.tileMotion : ({})
                readonly property real size: Number(rect.width || 1)
                readonly property real hoverScale: entry ? Number(entry.visualScale || 1) : 1
                readonly property real elevation: root.bounded("iconElevation", 0.3, 0, 2)
                // The pedestal's height, and the small gap that keeps an icon
                // without one off the platform's top.
                readonly property real pedestalHeight: size * elevation
                readonly property real baseLift: size * 0.02
                readonly property bool tileShown: visual !== null && visual.tileRenderingEnabled
                // Where the entry stands. On a generated platform the layout's
                // own position, carried onto the platform's centre line: the
                // platform is that layout's path. On a theme's mesh, its
                // direction from the scene centre on the theme's own track;
                // scaling a layout that is not the mesh's shape could put an
                // icon in the hole.
                readonly property real reachX: Number(rect.centerX) - root.width / 2
                readonly property real reachY: root.height / 2 - Number(rect.centerY)
                readonly property real reach: Math.hypot(reachX, reachY)
                readonly property var standPoint: root.generatedPlatform
                    ? PlatformGeometry.standPoint(reachX, reachY, root.layoutTrackRadius,
                                                  root.generatedPlatform.track).map(v => v * root.platformScale)
                    : [(reach > 0.001 ? reachX / reach : 0) * root.entryTrackRadius,
                       (reach > 0.001 ? reachY / reach : 1) * root.entryTrackRadius]
                property alias glyphModel: glyphModel
                property alias inputAnchor: inputAnchor
                readonly property real glow: Math.max(root.number(iconMotion, "glow", 0),
                    root.number(glyphMotion, "glow", 0), root.number(tileMotion, "glow", 0))
                // Outside the window of an overcrowded open curve the entry is
                // not drawn, exactly as its 2D delegate is not.
                visible: rect.onTrack !== false
                // The entry's origin is where its icon stands: on the platform's
                // track, on top of its pedestal. Motion offsets move it along.
                position: Qt.vector3d(standPoint[0] + root.number(iconMotion, "x", 0),
                    standPoint[1] - root.number(iconMotion, "y", 0),
                    root.platformTop + pedestalHeight + baseLift)
                opacity: root.number(iconMotion, "opacity", 1)

                // The pedestal: a solid column from the platform's top up to
                // the icon, in the platform's own material.
                IconStyle3D {
                    objectName: "mesh-pedestal-" + entryNode.index
                    meshData: root.pedestalMesh
                    materialData: root.platformMaterial
                    surfaceTexture: root.themeTexture
                    visible: entryNode.tileShown && entryNode.elevation > 0
                    z: -(entryNode.pedestalHeight + entryNode.baseLift)
                    scale: Qt.vector3d(entryNode.size * 0.18, entryNode.size * 0.18,
                        entryNode.pedestalHeight + entryNode.baseLift)
                }
                // A theme's own icon base, lying on the platform around the
                // pedestal's foot.
                IconStyle3D {
                    objectName: "mesh-icon-base-" + entryNode.index
                    meshData: iconResource.meshData
                    materialData: iconResource.materialData
                    surfaceTexture: root.themeTexture
                    visible: !root.generatedSpec && entryNode.tileShown && entryNode.elevation > 0
                    z: -(entryNode.pedestalHeight + entryNode.baseLift)
                    scale: Qt.vector3d(entryNode.size * 0.3, entryNode.size * 0.3, entryNode.size * 0.15)
                    emissionScale: root.emissionScale * (1 + entryNode.glow)
                }
                // A theme's own parts for each entry (such as a ring that sinks
                // as the panel closes), at the pedestal's foot.
                Node {
                    objectName: "mesh-tile-parts-" + entryNode.index
                    visible: !entryNode.visual || (entryNode.visual.tileRenderingEnabled
                        && !entryNode.visual.customTileActive)
                    z: -(entryNode.pedestalHeight + entryNode.baseLift)
                    scale: Qt.vector3d(entryNode.size * 0.55, entryNode.size * 0.55, entryNode.size * 0.55)
                    Repeater3D {
                        model: root.entryParts.length
                        delegate: IconStyle3D {
                            required property int index
                            objectName: "mesh-entry-part-" + entryNode.index + "-" + index
                            partDefinition: root.entryParts[index].definition
                            visible: partDefinition.mechanism === root.mechanism
                            meshData: root.entryParts[index].resources.mesh || null
                            materialData: root.entryParts[index].resources.material || ({})
                            surfaceTexture: root.themeTexture
                            openAmount: root.partOpenAmount(partDefinition)
                            emissionScale: root.emissionScale * (1 + entryNode.glow)
                        }
                    }
                }
                Texture {
                    id: glyphTexture
                    readonly property Item candidateSource:
                        entryNode.visual ? entryNode.visual.glyphTextureItem : null
                    // A detached source releases its layer before QObject destruction.
                    sourceItem: candidateSource && root.Window.window
                        && candidateSource.Window.window === root.Window.window
                        && entryNode.visual.meshVisualActive ? candidateSource : null
                }
                Texture {
                    id: tileTexture
                    readonly property Item candidateSource:
                        entryNode.visual ? entryNode.visual.tileTextureItem : null
                    sourceItem: candidateSource && root.Window.window
                        && candidateSource.Window.window === root.Window.window
                        && entryNode.visual.meshVisualActive ? candidateSource : null
                }
                // The icon faces the viewer at every pitch, yaw and roll, so it
                // keeps the application's own glyph readable instead of lying
                // flat on the platform; it stands on its pedestal.
                Node {
                    objectName: "mesh-icon-" + entryNode.index
                    rotation: root.iconFacing
                    // The icon's resting square, untouched by hover and motion:
                    // its projection is the entry's input rectangle, so the
                    // rectangle does not breathe under the pointer.
                    Node {
                        id: inputAnchor
                        objectName: "mesh-input-anchor-" + entryNode.index
                        y: entryNode.size / 2
                    }
                    Node {
                        objectName: "mesh-icon-body-" + entryNode.index
                        // Hover magnification and icon motion grow the icon about
                        // its foot, so it stays standing on the pedestal.
                        eulerRotation: Qt.vector3d(0, root.number(entryNode.iconMotion, "rotateY", 0),
                            -root.number(entryNode.iconMotion, "rotateZ", 0) - Number(entryNode.rect.rotation || 0))
                        scale: Qt.vector3d(entryNode.hoverScale * root.number(entryNode.iconMotion, "scale", 1)
                                * root.number(entryNode.iconMotion, "scaleX", 1),
                            entryNode.hoverScale * root.number(entryNode.iconMotion, "scale", 1)
                                * root.number(entryNode.iconMotion, "scaleY", 1), 1)
                        Model {
                            objectName: "mesh-tile-" + entryNode.index
                            source: "#Rectangle"
                            pickable: false
                            visible: entryNode.tileShown && root.collapseProgress < 1
                            position: Qt.vector3d(root.number(entryNode.tileMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.tileMotion, "y", 0), 0)
                            eulerRotation: Qt.vector3d(0, root.number(entryNode.tileMotion, "rotateY", 0),
                                -root.number(entryNode.tileMotion, "rotateZ", 0))
                            scale: Qt.vector3d(entryNode.size / 100 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleX", 1),
                                entryNode.size / 100 * root.number(entryNode.tileMotion, "scale", 1)
                                    * root.number(entryNode.tileMotion, "scaleY", 1), 1)
                            opacity: root.number(entryNode.tileMotion, "opacity", 1) * (1 - root.collapseProgress)
                            materials: PrincipledMaterial {
                                lighting: PrincipledMaterial.NoLighting
                                alphaMode: PrincipledMaterial.Blend
                                baseColorMap: tileTexture
                                cullMode: Material.NoCulling
                            }
                        }
                        Model {
                            id: glyphModel
                            pickable: true
                            objectName: "mesh-glyph-" + entryNode.index
                            source: "#Rectangle"
                            visible: entryNode.visual !== null && root.collapseProgress < 1
                            // In front of its tile.
                            position: Qt.vector3d(root.number(entryNode.glyphMotion, "x", 0),
                                entryNode.size / 2 - root.number(entryNode.glyphMotion, "y", 0),
                                entryNode.size * 0.01)
                            eulerRotation: Qt.vector3d(0, root.number(entryNode.glyphMotion, "rotateY", 0),
                                -root.number(entryNode.glyphMotion, "rotateZ", 0))
                            scale: Qt.vector3d(entryNode.size / 100 * root.number(entryNode.glyphMotion, "scale", 1)
                                    * root.number(entryNode.glyphMotion, "scaleX", 1),
                                entryNode.size / 100 * root.number(entryNode.glyphMotion, "scale", 1)
                                    * root.number(entryNode.glyphMotion, "scaleY", 1), 1)
                            opacity: root.number(entryNode.glyphMotion, "opacity", 1) * (1 - root.collapseProgress)
                            materials: PrincipledMaterial {
                                lighting: PrincipledMaterial.NoLighting
                                alphaMode: PrincipledMaterial.Blend
                                baseColorMap: glyphTexture
                                cullMode: Material.NoCulling
                            }
                        }
                    }
                }
            }
        }
        }
        Node {
            objectName: "mesh-platform-motion"
            eulerRotation.z: -root.layoutAngle
            scale: Qt.vector3d(root.platformScale, root.platformScale,
                               root.platformScale * root.bounded("thickness", 1, 0.1, 4))
            IconStyle3D {
                id: platform
                objectName: "mesh-platform"
                pickable: true
                meshData: root.platformMesh
                materialData: root.platformMaterial
                surfaceTexture: root.themeTexture
                emissionScale: root.emissionScale
            }
            Repeater3D {
                model: root.panelParts.length
                delegate: IconStyle3D {
                    required property int index
                    objectName: "mesh-panel-part-" + index
                    partDefinition: root.panelParts[index].definition
                    visible: partDefinition.mechanism === root.mechanism
                    meshData: root.panelParts[index].resources.mesh || null
                    materialData: root.panelParts[index].resources.material || ({})
                    surfaceTexture: root.themeTexture
                    openAmount: root.partOpenAmount(partDefinition)
                    emissionScale: root.emissionScale
                }
            }
        }
        }
        Node {
            id: gizmo
            objectName: "mesh-gizmo"
            visible: root.editMode
            position: Qt.vector3d(root.shownPosition.x, root.shownPosition.y,
                root.shownPosition.z + root.floatOffset + root.platformTop * root.shownScale)
            Repeater3D {
                // Arrows to move, cube-tipped arms to scale.
                model: [
                    { axis: "x", color: "#ff5a5a", turn: Qt.vector3d(0, 0, -90) },
                    { axis: "y", color: "#7be37b", turn: Qt.vector3d(0, 0, 0) },
                    { axis: "z", color: "#5aa8ff", turn: Qt.vector3d(90, 0, 0) }
                ]
                delegate: Node {
                    id: arm
                    required property var modelData
                    readonly property string handleMode: root.gizmoMode === "scale" ? "scale" : "move"
                    visible: root.gizmoMode !== "rotate"
                    eulerRotation: modelData.turn
                    Model {
                        objectName: "gizmo-" + arm.handleMode + "-" + arm.modelData.axis + "-shaft"
                        source: "#Cylinder"
                        pickable: true
                        position: Qt.vector3d(0, root.gizmoLength / 2, 0)
                        scale: Qt.vector3d(root.gizmoLength * 0.0006, root.gizmoLength / 100,
                                           root.gizmoLength * 0.0006)
                        materials: PrincipledMaterial {
                            lighting: PrincipledMaterial.NoLighting
                            baseColor: arm.modelData.color
                        }
                    }
                    Model {
                        objectName: "gizmo-" + arm.handleMode + "-" + arm.modelData.axis + "-tip"
                        source: arm.handleMode === "scale" ? "#Cube" : "#Cone"
                        pickable: true
                        position: Qt.vector3d(0, root.gizmoLength, 0)
                        scale: Qt.vector3d(root.gizmoLength * 0.0016, root.gizmoLength * 0.0022,
                                           root.gizmoLength * 0.0016)
                        materials: PrincipledMaterial {
                            lighting: PrincipledMaterial.NoLighting
                            baseColor: arm.modelData.color
                        }
                    }
                }
            }
            Repeater3D {
                // Rings to turn: pitch about X, yaw about Y, roll about the view.
                model: [
                    { axis: "x", color: "#ff5a5a", turn: Qt.vector3d(0, 90, 0) },
                    { axis: "y", color: "#7be37b", turn: Qt.vector3d(90, 0, 0) },
                    { axis: "view", color: "#f4f8fb", turn: Qt.vector3d(0, 0, 0) }
                ]
                delegate: Node {
                    id: ring
                    required property var modelData
                    visible: root.gizmoMode === "rotate"
                    readonly property real radius: root.gizmoLength * (modelData.axis === "view" ? 1.15 : 0.9)
                    // The view ring always faces the camera.
                    rotation: modelData.axis === "view" ? camera.sceneRotation : Qt.quaternion(1, 0, 0, 0)
                    eulerRotation: modelData.axis === "view" ? Qt.vector3d(0, 0, 0) : modelData.turn
                    Repeater3D {
                        model: 32
                        delegate: Model {
                            required property int index
                            readonly property real angle: index * Math.PI * 2 / 32
                            objectName: "gizmo-rotate-" + ring.modelData.axis + "-" + index
                            source: "#Cube"
                            pickable: true
                            position: Qt.vector3d(ring.radius * Math.cos(angle), ring.radius * Math.sin(angle), 0)
                            eulerRotation: Qt.vector3d(0, 0, angle * 180 / Math.PI + 90)
                            scale: Qt.vector3d(ring.radius * 2 * Math.PI / 32 / 100 * 0.9,
                                               root.gizmoLength * 0.0005, root.gizmoLength * 0.0005)
                            materials: PrincipledMaterial {
                                lighting: PrincipledMaterial.NoLighting
                                baseColor: ring.modelData.color
                            }
                        }
                    }
                }
            }
        }
        // Lights stay in world space; only the platform and its icons move.
        DirectionalLight {
            objectName: "mesh-key-light"
            eulerRotation: Qt.vector3d(-35, -35, 0)
            brightness: root.bounded("keyLightBrightness", 1, 0, 4)
            ambientColor: "#202630"
        }
        DirectionalLight {
            objectName: "mesh-fill-light"
            eulerRotation: Qt.vector3d(25, 140, 0)
            brightness: root.bounded("fillLightBrightness", 0.4, 0, 2)
            color: "#78c8ff"
        }
        // The shared geometry is retained even with no entries. Its readiness
        // participates in the whole scene's safe fallback decision.
        IconStyle3D {
            id: iconResource
            meshData: (root.resources || ({})).iconMesh || null
            materialData: (root.resources || ({})).material || ({})
            visible: false
        }
    }
}
