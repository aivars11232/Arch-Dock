import QtQuick
import QtQuick.Window
import ArchDock.Rendering 1.0
import ArchDock.Decorations 1.0

// A panel decoration, or a material, drawn small and still on a short
// straight band, lit as on a panel: Panel Studio's picture of it in its
// lists. A material is shown as a thick round bar made of it, which
// catches the light as a polished rod does. The
// picture is scaled to fit, whatever the decoration reaches out to (a
// broom's tail, a ship's end pods).
Item {
    id: root

    // A decoration's id, or a material's (DecorationCatalogue.js); the
    // material is drawn when it is set.
    property string decoration: ""
    property string material: ""

    readonly property bool ready: thumbnailBaker.frames > 0
    readonly property real fit: thumbnailBaker.bounds.width > 0 && thumbnailBaker.bounds.height > 0
        ? Math.min(width / thumbnailBaker.bounds.width, height / thumbnailBaker.bounds.height) : 1

    implicitWidth: 168
    implicitHeight: 52

    DecorationBaker {
        id: thumbnailBaker
        directory: Qt.resolvedUrl("..")
        decoration: root.material.length ? "" : root.decoration
        // A material's swatch: a round bar of it with round ends.
        decorationData: root.material.length ? ({
            id: "material-swatch", name: "", family: "swatch", material: { id: root.material }, animation: [],
            contour: { outer: { base: 1 }, inner: { base: 1 }, ends: { kind: "pill" }, body: { round: true } }
        }) : undefined
        path: ({ points: [{ x: 0, y: 0 }, { x: 120, y: 0 }], closed: false, outward: { x: 0, y: -1 } })
        halfWidth: root.material.length ? 16 : 10
        // Three icons' places, so arches and segments stand where icons would.
        iconPlaces: [16, 60, 104]
        animated: false
        ends: root.material.length ? 0.5 : 2.5
        // Sharp at the size it is shown: the band is drawn as many device
        // pixels across as the thumbnail shows it.
        scale: Math.max(1, (root.Window.window ? root.Window.window.devicePixelRatio : Screen.devicePixelRatio)
            * Math.max(0.5, root.height / 52))
    }

    Item {
        id: pictures
        anchors.centerIn: parent
        width: thumbnailBaker.bounds.width
        height: thumbnailBaker.bounds.height
        scale: root.fit

        DecorationLayer {
            anchors.fill: parent
            baker: thumbnailBaker
            picture: "base"
        }
        Repeater {
            model: thumbnailBaker.layers
            delegate: DecorationLayer {
                required property string modelData
                anchors.fill: parent
                baker: thumbnailBaker
                picture: modelData
            }
        }
    }
}
