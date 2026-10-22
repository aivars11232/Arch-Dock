import QtQuick
import ArchDock.Rendering 1.0

// The dependency-free fallback surface. Original vector material maps are
// painted on the existing track, including the software scene-graph backend
// where ShaderEffect is unavailable. Geometry and rotation stay shared.
Item {
    id: root

    property string layout: "horizontal"
    property var geometry: ({
        width: 0,
        height: 0,
        iconSize: 40,
        padding: 0,
        radius: 40,
        layout: "horizontal"
    })
    property real layoutAngle: 0
    // The configured angle without the scene's turn. A turning free panel is
    // drawn in a square centred on its path (LayoutEngine.rotationEnvelope),
    // so the path is painted at its resting angle and the turn rotates the
    // painting as a whole: the threaded canvas would otherwise show each turn
    // after the icons had already moved (ADREP-TASK-002, OF-11).
    property real restingAngle: layoutAngle
    readonly property bool turnsAsDrawn:
        Boolean(geometry && geometry.rotationEnvelope === true)
    readonly property real drawnAngle: turnsAsDrawn ? restingAngle : layoutAngle
    readonly property real canvasTurn: turnsAsDrawn ? layoutAngle - restingAngle : 0
    property int polygonSides: 6
    property string appearance: "glass"
    // Lightning's bolts are drawn on a clear band; this much of the dark
    // storm shows behind them (Lightning background, the owner 2026-10-13).
    property real lightningBackground: 0
    // An energy look's own light (its Lightning, Plasma, Neon, Lime or
    // Circuit glow): 1 as the look is drawn, 0 none, 2 twice as much; and how
    // fast it moves.
    property real glowIntensity: 1
    property real lookMotionSpeed: 1
    readonly property bool energyLook: ["neon", "plasma", "lightning", "aurora", "futuristic"]
        .includes(String(surfaceStyle.material))
    readonly property real lookGlow: energyLook ? Math.max(0, Math.min(2, glowIntensity)) : 1
    readonly property real motionPace: Math.max(0.1, Math.min(3, lookMotionSpeed))
    property string customColor: ""
    property real panelOpacity: 0.9
    property real sparkleIntensity: 0
    property bool reducedMotion: false
    property bool sceneConcealed: false

    // The presentation track. A procedural surface has no declared parts to
    // slide or split, so it takes the whole-surface track: a collapse squeezes
    // the drawn shape onto its handle along the collapse axis, other
    // mechanisms clip and fade it. PanelMotionController has already decided
    // the geometry; this only applies it.
    property var motionTracks: null

    readonly property var surfaceMotionTrack: {
        const source = motionTracks && typeof motionTracks === "object"
            ? motionTracks : null
        const track = source ? source["surface"] : null
        return track && typeof track === "object" ? track : null
    }
    readonly property real motionOpacity: {
        const value = Number(surfaceMotionTrack
                             && surfaceMotionTrack.opacity !== undefined
                             ? surfaceMotionTrack.opacity : 1)
        return isFinite(value) ? Math.max(0, Math.min(1, value)) : 1
    }
    readonly property var motionClipRect: {
        const clip = surfaceMotionTrack && surfaceMotionTrack.clip
                && typeof surfaceMotionTrack.clip === "object"
            ? surfaceMotionTrack.clip : null
        function number(value, fallback) {
            const candidate = Number(value)
            return isFinite(candidate) ? candidate : fallback
        }
        if (!clip)
            return Qt.rect(0, 0, Math.max(0, width), Math.max(0, height))
        return Qt.rect(
            Math.max(0, number(clip.x, 0)),
            Math.max(0, number(clip.y, 0)),
            Math.max(0, number(clip.width, width)),
            Math.max(0, number(clip.height, height)))
    }
    readonly property bool motionClipActive:
        motionClipRect.width < width - 0.0001
        || motionClipRect.height < height - 0.0001
        || motionClipRect.x > 0.0001 || motionClipRect.y > 0.0001
    function motionNumber(key, fallback) {
        const value = Number(surfaceMotionTrack && surfaceMotionTrack[key] !== undefined
                             ? surfaceMotionTrack[key] : fallback)
        return isFinite(value) ? value : fallback
    }

    readonly property bool rendererReady: width > 0 && height > 0
    readonly property var surfacePath: LayoutEngine.surface(
        layout, geometry, drawnAngle, polygonSides)
    // The look's band, as wide as a bar holding widgets asks (bandWidth) so
    // that they stand inside it.
    readonly property var surfaceStyle: {
        const style = LayoutEngine.themeStyle(appearance, customColor, Number(geometry.iconSize || 40))
        const band = Number(geometry.bandWidth || 0)
        return band > 0 ? Object.assign({}, style, { lineWidth: band }) : style
    }
    // A clear-ground look (lightning, aurora) in its chosen palette.
    property string lookPalette: ""
    // The picture of the "picture" look, Arch Dock's copy of it.
    property string picture: ""
    // The picture's relief: how far it stands out (0 flat, 1 tallest) and
    // whether its light or its dark parts do.
    property real pictureDepth: 0.8
    property bool pictureRaisesDark: false
    // The depth map's regulators: the brightness that is half height, how
    // steeply the height climbs through it, and how far apart the pixels
    // compared lie (a blur of the map).
    property real pictureLevel: 0.38
    property real pictureContrast: 0.35
    property real pictureSmoothing: 0
    // One repeat of the picture across the band: twice as tall as the band
    // is wide, at the picture's own proportions.
    readonly property int tileHeight: Math.max(48, Math.round(surfaceStyle.lineWidth * 2))
    readonly property int tileWidth: pictureSize.implicitHeight > 0
        ? Math.max(1, Math.round(tileHeight * pictureSize.implicitWidth / pictureSize.implicitHeight)) : 1
    // That repeat shaded for its relief (pixels the canvas can draw), made
    // once per picture and setting in a small software canvas: the panel's
    // own canvas draws on the graphics card and cannot read its pixels back.
    property var shadedData: null
    readonly property bool reliefWanted: surfaceStyle.picture && pictureDepth > 0.001
        && String(materialSource).length > 0
    onReliefWantedChanged: embossTile.refresh()
    onPictureDepthChanged: embossTile.refresh()
    onPictureRaisesDarkChanged: embossTile.refresh()
    onPictureLevelChanged: embossTile.refresh()
    onPictureContrastChanged: embossTile.refresh()
    onPictureSmoothingChanged: embossTile.refresh()
    onTileWidthChanged: embossTile.refresh()
    onTileHeightChanged: embossTile.refresh()
    onShadedDataChanged: canvas.requestPaint()
    readonly property url materialSource: surfaceStyle.picture ? picture
        : Qt.resolvedUrl("../materials/" + LayoutEngine.lookTexture(surfaceStyle.material, lookPalette) + ".png")
    readonly property bool materialReady: canvas.materialLoaded
    readonly property bool materialAnimationActive: visible && !sceneConcealed
        && !reducedMotion && surfaceStyle.animated && motionOpacity > 0 && panelOpacity > 0
    property real energyPhase: 0

    // The material wraps along the track the way a label wraps round a tin:
    // each piece of the path is painted in its own frame, the texture turned
    // to the path's direction there, its height the track's width and its
    // phase the distance travelled along the path, so it runs on from piece
    // to piece. It runs once along the whole track: repeated every track
    // width, its picture showed as sections along the panel instead of one
    // continuous colour (the owner, 2026-10-13). A closed track carries it
    // round its first half and back, mirrored, round its second, so the two
    // ends meet without a seam.
    function paintWrappedMaterial(context) {
        const points = surfacePath.points
        const count = points.length
        if (count < 2) return
        const closed = surfacePath.closed === true
        const last = closed ? count : count - 1
        const lengths = [0]
        for (let i = 1; i <= last; ++i) {
            const a = points[i - 1], b = points[i % count]
            lengths.push(lengths[i - 1] + Math.hypot(b.x - a.x, b.y - a.y))
        }
        const total = lengths[last]
        const width = surfaceStyle.lineWidth
        if (!(total > 0) || !(width > 0)) return
        const texture = 512
        // A bar's band, as deep as the bar, is a capsule along a straight
        // line: its round ends carry the material as its middle does, the
        // picture laid once over the whole capsule.
        const first = points[0], end = points[count - 1]
        const straight = !closed && points.every(point =>
            Math.abs((end.x - first.x) * (point.y - first.y) - (end.y - first.y) * (point.x - first.x))
                <= 0.5 * Math.max(1, total))
        if (straight && Number(geometry.bandWidth || 0) > 0) {
            context.save()
            context.translate(first.x, first.y)
            context.rotate(Math.atan2(end.y - first.y, end.x - first.x))
            // The capsule's outline is fixed as it is drawn; the picture
            // follows the scale the fill is made with, so it is stretched over
            // the capsule while the round ends stay round.
            const radius = width / 2
            context.beginPath()
            context.moveTo(0, -radius)
            context.lineTo(total, -radius)
            context.arc(total, 0, radius, -Math.PI / 2, Math.PI / 2, false)
            context.lineTo(0, radius)
            context.arc(0, 0, radius, Math.PI / 2, Math.PI * 3 / 2, false)
            context.closePath()
            context.translate(-radius, -radius)
            context.scale((total + width) / texture, width / texture)
            context.fillStyle = context.createPattern(String(materialSource), "repeat")
            context.fill()
            context.restore()
            // Saving and restoring keeps no path: the passes after this one
            // stroke the band's own line, not the capsule's outline.
            trace(context)
            return
        }
        // Pieces overlap by `margin` texels so no hairline shows between
        // them; the run keeps that far inside the picture, so an overlap at
        // either end never wraps round to its other edge.
        const margin = 2
        const half = closed ? total / 2 : total
        const scaleX = half / (texture - margin * 2)
        const scaleY = width / texture
        const pattern = context.createPattern(String(materialSource), "repeat")
        // One stretch of the path [from, to] of the piece that starts at
        // `start`: the texture runs on with the distance travelled, or back
        // from the far end when mirrored.
        function paintStretch(a, angle, start, from, to, mirrored) {
            if (!(to > from)) return
            context.save()
            context.translate(a.x, a.y)
            context.rotate(angle)
            context.translate(mirrored ? total - start + margin * scaleX : -start - margin * scaleX,
                              -width / 2)
            context.scale(mirrored ? -scaleX : scaleX, scaleY)
            context.fillStyle = pattern
            const near = (mirrored ? total - to : from) / scaleX
            context.fillRect(near, 0, (to - from) / scaleX + margin * 2, texture)
            context.restore()
        }
        for (let i = 1; i <= last; ++i) {
            const a = points[i - 1], b = points[i % count]
            const start = lengths[i - 1], end = lengths[i]
            if (!(end > start)) continue
            const angle = Math.atan2(b.y - a.y, b.x - a.x)
            paintStretch(a, angle, start, start, Math.min(end, half), false)
            if (closed)
                paintStretch(a, angle, start, Math.max(start, half), end, true)
        }
    }

    // A picture fills the panel's own shape once, scaled to cover the whole
    // of it, rather than running along the track as a texture does. The band
    // is drawn first without its glow, the picture only where the band is,
    // then the glow behind both, so no picture shows in the glow.
    function paintPicture(context) {
        context.shadowBlur = 0
        context.shadowColor = "transparent"
        trace(context)
        context.stroke()
        const points = surfacePath.points
        const half = surfaceStyle.lineWidth / 2
        let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity
        for (let i = 0; i < points.length; ++i) {
            left = Math.min(left, points[i].x)
            right = Math.max(right, points[i].x)
            top = Math.min(top, points[i].y)
            bottom = Math.max(bottom, points[i].y)
        }
        const width = right - left + half * 2
        const height = bottom - top + half * 2
        const pictureWidth = pictureSize.implicitWidth
        const pictureHeight = pictureSize.implicitHeight
        // Water and lava are laid at their natural size: the band shows a
        // strip of the real surface, never a whole repeat squeezed into it.
        const natural = surfaceStyle.ownColours
        const repeatWidth = natural ? 512 : tileWidth
        const repeatHeight = natural ? 512 : tileHeight
        if (canvas.materialLoaded && width > 0 && height > 0 && (natural || (pictureWidth > 0 && pictureHeight > 0))) {
            // The picture at its own proportions, repeated across the shape
            // as the 3D band repeats it along, so a texture keeps its grain
            // whatever the panel's size: each repeat twice as tall as the
            // band is wide, the band showing the middle of it; in relief,
            // the repeat already shaded for it.
            const source = reliefWanted && shadedData !== null ? shadedData : String(materialSource)
            const x0 = left - half, y0 = top - half
            context.save()
            context.globalCompositeOperation = "source-atop"
            for (let y = y0 - repeatHeight / 2; y < y0 + height; y += repeatHeight)
                for (let x = x0; x < x0 + width; x += repeatWidth)
                    context.drawImage(source, x, y, repeatWidth, repeatHeight)
            context.restore()
        }
        if (lookGlow > 0) {
            context.save()
            context.globalCompositeOperation = "destination-over"
            context.shadowColor = surfaceStyle.shadow
            context.shadowBlur = surfaceStyle.blur * lookGlow
            trace(context)
            context.stroke()
            context.restore()
        }
    }

    // The picture's relief on a flat panel, shaded into one repeat of it
    // the way normal maps are made from height maps (GIMP's normal map
    // filter and its descendants): its
    // brightness is its height, a Sobel filter over the heights around
    // each pixel (farther apart the more smoothing) gives its slope, and
    // normalize(-dx, -dy, 1 / strength) its surface normal, lit here by a
    // light from the top left and the viewer's own reflection, so a
    // stone's upper edge is lit and its lower edge shadowed. Relief depth
    // is the strength; with the dark parts raised the map is turned round.
    // A level stretch keeps its colour.
    function shadeHeights(context, w, h) {
        if (w < 3 || h < 3)
            return null
        const data = context.getImageData(0, 0, w, h)
        const pixels = data.data
        // The height each pixel's brightness stands for: half height at
        // Relief level, the climb as steep as Relief contrast says.
        const heights = new Float32Array(w * h)
        const span = Math.max(0.02, 1 - pictureContrast)
        const low = pictureLevel - span / 2
        const flip = pictureRaisesDark ? -1 : 1
        for (let i = 0, p = 0; i < heights.length; ++i, p += 4) {
            let t = ((pixels[p] * 0.299 + pixels[p + 1] * 0.587 + pixels[p + 2] * 0.114) / 255 - low) / span
            t = Math.max(0, Math.min(1, t))
            heights[i] = flip * t * t * (3 - 2 * t)
        }
        const reach = Math.max(1, Math.round(1 + pictureSmoothing * 3))
        // The Sobel filter's sum is 8 heights wide over 2 * reach pixels.
        const scale = (3 + pictureDepth * 45) / (8 * reach)
        // The light, from the top left and above; the half-way vector
        // between it and the viewer for the shine.
        const lx = -0.55, ly = -0.55, lz = 0.63
        const hl = Math.hypot(lx, ly, lz + 1)
        const hx = lx / hl, hy = ly / hl, hz = (lz + 1) / hl
        const ambient = 0.3, diffuse = 0.8, shine = 0.45
        // What a level stretch gets, so that it keeps its colour.
        const level = ambient + diffuse * lz + shine * Math.pow(hz, 24)
        for (let row = 0; row < h; ++row) {
            const up = Math.max(0, row - reach) * w, mid = row * w, down = Math.min(h - 1, row + reach) * w
            for (let col = 0; col < w; ++col) {
                const i = mid + col, p = i * 4
                if (pixels[p + 3] === 0)
                    continue
                const left = Math.max(0, col - reach), right = Math.min(w - 1, col + reach)
                // Sobel: the derivative across, smoothed along (1, 2, 1).
                const sx = (heights[up + right] + 2 * heights[mid + right] + heights[down + right]
                            - heights[up + left] - 2 * heights[mid + left] - heights[down + left]) * scale
                const sy = (heights[down + left] + 2 * heights[down + col] + heights[down + right]
                            - heights[up + left] - 2 * heights[up + col] - heights[up + right]) * scale
                const length = Math.hypot(sx, sy, 1)
                const nx = -sx / length, ny = -sy / length, nz = 1 / length
                const lit = Math.max(0, nx * lx + ny * ly + nz * lz)
                const glint = Math.pow(Math.max(0, nx * hx + ny * hy + nz * hz), 24)
                const shade = (ambient + diffuse * lit + shine * glint) / level
                pixels[p] = Math.max(0, Math.min(255, pixels[p] * shade))
                pixels[p + 1] = Math.max(0, Math.min(255, pixels[p + 1] * shade))
                pixels[p + 2] = Math.max(0, Math.min(255, pixels[p + 2] * shade))
            }
        }
        return data
    }

    // The light sliding along a flowing look on a flat panel: the river's
    // three pale gleams, or the magma's two broad hot streaks adding their
    // light to the crust. Each is a soft stretch of light that laps the
    // band a whole number of times per round of the phase, so nothing
    // jumps when the phase wraps.
    function paintFlowingLight(context, points) {
        const lava = surfaceStyle.lava
        context.lineCap = "round"
        context.lineJoin = "round"
        if (lava) context.globalCompositeOperation = "lighter"
        const width = surfaceStyle.lineWidth
        const count = points.length
        const bands = lava ? 2 : 3
        for (let band = 0; band < bands; ++band) {
            const speed = band + 1
            const spread = lava ? 0.2 + band * 0.08 : 0.09 + band * 0.03
            const offset = band * 0.37
            const peak = lava ? 0.3 - band * 0.08 : 0.28 - band * 0.05
            context.lineWidth = Math.max(1, width * (lava ? 0.8 - band * 0.25 : 0.5 - band * 0.12))
            for (let i = 1; i < count; ++i) {
                let place = (i / count - (energyPhase * speed + offset)) % 1
                if (place < 0) place += 1
                const near = Math.min(place, 1 - place)
                const light = Math.exp(-(near / spread) * (near / spread)) * peak
                if (light < 0.01) continue
                context.strokeStyle = (lava ? "rgba(255,170,60," : "rgba(220,240,255,") + light.toFixed(3) + ")"
                context.beginPath()
                context.moveTo(points[i - 1].x, points[i - 1].y)
                context.lineTo(points[i].x, points[i].y)
                context.stroke()
            }
        }
    }

    // The picture's own size, for fitting it.
    Image {
        id: pictureSize
        visible: false
        asynchronous: true
        source: root.surfaceStyle.picture ? root.picture : ""
        onStatusChanged: if (status === Image.Ready) { canvas.requestPaint(); embossTile.refresh() }
    }


    function trace(context) {
        context.beginPath()
        const points = surfacePath.points
        context.moveTo(points[0].x, points[0].y)
        for (let i = 1; i < points.length; ++i)
            context.lineTo(points[i].x, points[i].y)
        if (surfacePath.closed) context.closePath()
    }

    Timer {
        interval: Math.round(125 / root.motionPace)
        repeat: true
        running: root.materialAnimationActive && !root.surfaceStyle.flicker
        onTriggered: {
            root.energyPhase = (root.energyPhase + 0.025) % 1
            energy.requestPaint()
        }
    }
    // Lightning (the owner, 2026-10-13): at random moments a bolt strikes
    // along part of the panel and dies away in a fifth of a second; between
    // strikes it flickers faintly. Each strike has its own shape (its seed),
    // kept while it fades.
    property real lightningFlash: 0
    property int lightningSeed: 1
    onLightningFlashChanged: if (root.surfaceStyle.flicker) energy.requestPaint()
    Timer {
        interval: 120
        repeat: true
        running: root.materialAnimationActive && root.surfaceStyle.flicker === true
        onTriggered: {
            interval = Math.round((60 + Math.random() * 520) / root.motionPace)
            if (Math.random() < 0.28) {
                root.lightningSeed = 1 + Math.floor(Math.random() * 2147483646)
                lightningFade.stop()
                root.lightningFlash = 0.75 + Math.random() * 0.25
                lightningFade.start()
            } else if (!lightningFade.running) {
                root.lightningFlash = Math.random() * 0.25
            }
        }
        onRunningChanged: if (!running) { lightningFade.stop(); root.lightningFlash = 0 }
    }
    NumberAnimation {
        id: lightningFade
        target: root
        property: "lightningFlash"
        to: 0
        duration: Math.round(200 / Math.sqrt(root.motionPace))
        easing.type: Easing.OutQuad
    }
    // A bolt along `span` points of the track from `start`: the track's own
    // line, pushed across it by a jagged amount at every point.
    function paintLightning(context, points) {
        const flash = Math.min(1, root.lightningFlash * root.lookGlow)
        if (flash <= 0.01 || points.length < 3) return
        let seed = root.lightningSeed
        const random = function() {
            seed = (seed * 48271) % 2147483647
            return seed / 2147483647
        }
        const count = points.length
        const span = Math.max(3, Math.round(count * (0.2 + 0.35 * random())))
        const start = Math.floor(random() * count)
        const across = root.surfaceStyle.lineWidth * 0.45
        const path = []
        for (let k = 0; k <= span; ++k) {
            const at = start + k
            if (!root.surfacePath.closed && at >= count) break
            const point = points[at % count], before = points[(at - 1 + count) % count]
            const after = points[(at + 1) % count]
            const dx = after.x - before.x, dy = after.y - before.y
            const length = Math.hypot(dx, dy) || 1
            const offset = (random() - 0.5) * 2 * across * Math.sin(Math.PI * k / span)
            path.push({ x: point.x - dy / length * offset, y: point.y + dx / length * offset })
        }
        context.lineCap = "round"
        context.lineJoin = "round"
        context.shadowColor = "rgba(125, 140, 255, 0.9)"
        context.shadowBlur = 14 * root.lookGlow
        for (const pass of [{ width: 4.5, alpha: 0.32 }, { width: 1.6, alpha: 1 }]) {
            context.lineWidth = pass.width
            context.strokeStyle = "rgba(228, 234, 255," + (pass.alpha * flash) + ")"
            context.beginPath()
            context.moveTo(path[0].x, path[0].y)
            for (let k = 1; k < path.length; ++k) context.lineTo(path[k].x, path[k].y)
            context.stroke()
        }
    }

    width: Number(geometry.width || 0)
    height: Number(geometry.height || 0)

    Item {
        id: motionClipper

        x: root.motionClipRect.x
        y: root.motionClipRect.y
        width: root.motionClipRect.width
        height: root.motionClipRect.height
        clip: root.motionClipActive

    Canvas {
        id: canvas
        objectName: "procedural-surface-canvas"
        property bool materialLoaded: false

        function loadMaterial() {
            if (!String(root.materialSource).length) {
                materialLoaded = false
                requestPaint()
                return
            }
            if (!isImageLoaded(root.materialSource)) loadImage(root.materialSource)
            materialLoaded = isImageLoaded(root.materialSource)
            requestPaint()
        }
        onAvailableChanged: if (available) loadMaterial()
        Component.onCompleted: if (available) loadMaterial()
        onImageLoaded: {
            materialLoaded = isImageLoaded(root.materialSource)
            requestPaint()
        }

        x: -motionClipper.x
        y: -motionClipper.y
        width: root.width
        height: root.height
        opacity: Math.max(0, Math.min(1, root.panelOpacity))
            * root.motionOpacity * root.surfaceStyle.alpha
        rotation: root.canvasTurn
        // A collapse along an axis squeezes the drawn shape about its centre
        // onto the handle the controller keeps.
        transform: [
            Scale {
                origin.x: canvas.width / 2
                origin.y: canvas.height / 2
                xScale: Math.max(0, root.motionNumber("scaleX", 1))
                yScale: Math.max(0, root.motionNumber("scaleY", 1))
            },
            Translate {
                x: root.motionNumber("offsetX", 0)
                y: root.motionNumber("offsetY", 0)
            }
        ]
        // Cooperative can replay costly shadow painting on Plasma's GUI
        // thread. Keep the same image commands on Canvas's private worker.
        renderStrategy: Canvas.Threaded

        onPaint: {
            const context = getContext("2d")
            context.reset()
            materialLoaded = isImageLoaded(root.materialSource)
            if (!root.rendererReady || root.surfacePath.points.length === 0
                    || !root.surfaceStyle.trackVisible)
                return

            context.lineWidth = root.surfaceStyle.lineWidth
            context.strokeStyle = root.surfaceStyle.stroke
            // A bar's band fills the bar's depth: a halo round it would only
            // be cut square at the bar's edges, so it has none.
            context.shadowColor = root.lookGlow > 0 && !(Number(root.geometry.bandWidth || 0) > 0)
                ? root.surfaceStyle.shadow : "transparent"
            context.shadowBlur = root.surfaceStyle.blur * root.lookGlow
            context.lineCap = "round"
            context.lineJoin = "round"
            if (root.surfaceStyle.depth > 0) {
                context.save()
                context.translate(0, root.surfaceStyle.depth)
                context.strokeStyle = root.appearance === "floating-glass"
                    ? "rgba(84,131,156,0.45)" : "#28343f"
                root.trace(context)
                context.stroke()
                context.restore()
            }
            if (root.surfaceStyle.picture || root.surfaceStyle.ownColours) {
                root.paintPicture(context)
                return
            }
            root.trace(context)
            if (root.surfaceStyle.clearGround) {
                // Lightning, aurora: their dark sky as strong as chosen, then
                // their light over it as it is, neither tinted nor edged.
                context.shadowBlur = 0
                context.shadowColor = "transparent"
                context.strokeStyle = "rgba(" + root.surfaceStyle.skyColour + ", "
                    + Math.max(0, Math.min(1, root.lightningBackground)) + ")"
                context.stroke()
                if (materialLoaded)
                    root.paintWrappedMaterial(context)
                return
            }
            // Keep the cast shadow separate from the textured, tinted body.
            context.strokeStyle = root.surfaceStyle.stroke
            context.stroke()
            context.shadowBlur = 0
            context.shadowColor = "transparent"
            if (materialLoaded) {
                root.paintWrappedMaterial(context)
            } else {
                context.strokeStyle = root.surfaceStyle.stroke
                context.stroke()
            }
            // Multiply preserves the map's grain and facets under every tint.
            context.shadowBlur = 0
            context.shadowColor = "transparent"
            context.globalCompositeOperation = "qt-multiply"
            context.strokeStyle = root.surfaceStyle.stroke
            context.stroke()
            context.globalCompositeOperation = "source-over"
            // Fine polished edge, rather than random glitter.
            context.save()
            context.translate(0, -root.surfaceStyle.lineWidth / 2 + 1)
            context.lineWidth = 1.1
            context.strokeStyle = "rgba(255,255,255,0.45)"
            root.trace(context)
            context.stroke()
            context.restore()
            if (root.surfaceStyle.sparkle && root.sparkleIntensity > 0) {
                context.strokeStyle = "rgba(255,255,255," + Math.min(1, root.sparkleIntensity) + ")"
                context.lineWidth = 1
                const points = root.surfacePath.points
                for (let i = 3; i < points.length; i += 13) {
                    const p = points[i]
                    context.beginPath()
                    context.moveTo(p.x - 3, p.y); context.lineTo(p.x + 3, p.y)
                    context.moveTo(p.x, p.y - 3); context.lineTo(p.x, p.y + 3)
                    context.stroke()
                }
            }
        }

        Connections {
            target: root

            function onLayoutChanged() { canvas.requestPaint() }
            function onGeometryChanged() { canvas.requestPaint() }
            function onDrawnAngleChanged() { canvas.requestPaint() }
            function onPolygonSidesChanged() { canvas.requestPaint() }
            function onAppearanceChanged() { canvas.requestPaint() }
            function onCustomColorChanged() { canvas.requestPaint() }
            function onPanelOpacityChanged() { canvas.requestPaint() }
            function onSparkleIntensityChanged() { canvas.requestPaint() }
            function onMaterialSourceChanged() { canvas.loadMaterial() }
        }
    }

    // Only the small light overlay repaints at 8 Hz. Static textured bodies
    // remain cached; neither canvas paints periodically while hidden.
    Canvas {
        id: energy
        objectName: "procedural-material-energy"
        anchors.fill: canvas
        rotation: canvas.rotation
        transform: [
            Scale {
                origin.x: energy.width / 2
                origin.y: energy.height / 2
                xScale: Math.max(0, root.motionNumber("scaleX", 1))
                yScale: Math.max(0, root.motionNumber("scaleY", 1))
            },
            Translate {
                x: root.motionNumber("offsetX", 0)
                y: root.motionNumber("offsetY", 0)
            }
        ]
        visible: root.surfaceStyle.animated
        opacity: canvas.opacity
        renderStrategy: Canvas.Threaded
        onPaint: {
            if (!root.surfaceStyle.animated) return
            const context = getContext("2d")
            context.reset()
            const points = root.surfacePath.points
            if (points.length < 2) return
            if (root.surfaceStyle.flicker) {
                root.paintLightning(context, points)
                return
            }
            if (root.surfaceStyle.water || root.surfaceStyle.lava) {
                root.paintFlowingLight(context, points)
                return
            }
            // Aurora's light is all in its curtains.
            if (root.surfaceStyle.clearGround) return
            context.lineWidth = root.appearance === "plasma" ? 3 : 1.6
            for (let i = 1; i < points.length; ++i) {
                const wave = (i / points.length - root.energyPhase + 1) % 1
                context.strokeStyle = "rgba(225,250,255," + Math.min(1, root.lookGlow *
                    (root.appearance === "plasma" ? Math.max(0, 1 - wave * 4)
                        : 0.2 + 0.4 * (1 + Math.sin(wave * Math.PI * 6)))) + ")"
                context.beginPath()
                context.moveTo(points[i-1].x, points[i-1].y)
                context.lineTo(points[i].x, points[i].y)
                context.stroke()
            }
        }
        Connections {
            target: root
            function onSurfacePathChanged() { if (root.surfaceStyle.animated) energy.requestPaint() }
            function onAppearanceChanged() { if (root.surfaceStyle.animated) energy.requestPaint() }
        }
    }
    }
    // One repeat of the picture, shaded for its relief, drawn in software
    // (its pixels can be read back) and handed to the panel's canvas as
    // pixels it draws like a picture.
    Canvas {
        id: embossTile
        visible: false
        width: root.tileWidth
        height: root.tileHeight
        renderTarget: Canvas.Image
        renderStrategy: Canvas.Immediate
        function refresh() {
            if (!root.reliefWanted) {
                root.shadedData = null
                return
            }
            if (!isImageLoaded(root.materialSource)) {
                loadImage(root.materialSource)
                return
            }
            requestPaint()
        }
        onImageLoaded: refresh()
        onPaint: {
            const context = getContext("2d")
            context.reset()
            if (!root.reliefWanted || !isImageLoaded(root.materialSource) || width < 3 || height < 3)
                return
            context.drawImage(String(root.materialSource), 0, 0, width, height)
            root.shadedData = root.shadeHeights(context, Math.floor(width), Math.floor(height))
        }
    }

}
