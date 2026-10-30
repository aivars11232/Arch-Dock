import QtQuick
import QtTest
import ArchDock.Rendering 1.0

// A Plasma widget's face drawn as a hologram (WidgetFace): it floats in its
// own light away from the screen's edge on every edge, stays in its place
// without holograms, and grows under the pointer as the dock's icons do.
TestCase {
    id: testCase
    name: "WidgetFace"
    width: 400
    height: 400
    when: windowShown

    Component {
        id: faceComponent
        WidgetFace {
            width: 120
            height: 60
            iconSize: 52
            picture: "file:///usr/share/icons/hicolor/scalable/apps/org.kde.konsole.svg"
        }
    }

    function makeFace(properties) {
        const face = createTemporaryObject(faceComponent, testCase, properties)
        verify(face !== null)
        return face
    }

    function test_floatsAwayFromTheScreenEdge_data() {
        return [
            { tag: "top", edge: "top", across: "y", sign: 1 },
            { tag: "bottom", edge: "bottom", across: "y", sign: -1 },
            { tag: "left", edge: "left", across: "x", sign: 1 },
            { tag: "right", edge: "right", across: "x", sign: -1 }
        ]
    }

    function test_floatsAwayFromTheScreenEdge(data) {
        const face = makeFace({ screenEdge: data.edge, settings: { hologramIcons: true, hologramBeam: true } })
        verify(face.drawn, "a picture with holograms on is drawn")
        verify(face.lightShown, "over its own light")
        verify(face.raise > 52, "floating out of its place, as an icon's hologram does")
        const rect = face.restingRect
        compare(Math.sign(rect[data.across]), data.sign, "away from the screen's edge")
        compare(rect[data.across === "x" ? "y" : "x"], 0, "straight out, not sideways")
        compare(rect.width, 120, "as large as the widget")
        compare(rect.height, 60)
    }

    function test_withoutHologramsItStaysInItsPlace() {
        const face = makeFace({ screenEdge: "top", settings: { hologramIcons: false } })
        verify(!face.drawn)
        compare(face.restingRect.x, 0)
        compare(face.restingRect.y, 0)
    }

    function test_growsUnderThePointer() {
        const face = makeFace({ screenEdge: "top", settings: { hologramIcons: true },
                                magnification: 1.65, magnificationEnabled: true })
        compare(face.hoverScale, 1, "at rest")
        face.hovered = true
        verify(face.hoverScale > 1.5, "under the pointer it grows by the panel's magnification")
        face.magnificationEnabled = false
        compare(face.hoverScale, 1, "unless magnification is off")
    }
}
