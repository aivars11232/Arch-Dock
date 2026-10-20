#pragma once

#include <QImage>
#include <QJsonArray>
#include <QPointF>
#include <QString>
#include <QVector3D>
#include <QtGlobal>

#include <vector>

namespace ArchDock
{

// A decoration's round solids as tubes for a 3D scene: a trunk and its
// branches, round all the way round, the bark wrapped round them with its
// grain along them and its relief standing out of them. In the scene's own
// pixels with y up, as ReliefMesh; a trunk's middle lies on the ground
// (z 0), its branches rising a little as they grow away from it.
struct TubeMesh
{
    // Per vertex: position (3), normal (3), texture coordinate (2), what it
    // is (1 the bark, 2 a cut end's face, 3 a mark on the bark, whose colour
    // is its texture coordinate's red and green and the last value's blue)
    // and that last value (0 but on a mark).
    static constexpr int stride = 10;
    std::vector<float> vertices;
    std::vector<quint32> indices;
    QVector3D minimum;
    QVector3D maximum;
};

struct DecorationTubes
{
    TubeMesh mesh;
    // What the tubes are made of, as the viewer's colours make it (RGBA8,
    // sRGB), its heights (grey; null when it has none) and the face of a cut
    // end (null: the bark's own).
    QImage bark;
    QImage relief;
    QImage cap;
    float metalness = 0;
    float roughness = 0.7f;
    // How far the bark stands out of the tube and sinks into it (the scene's
    // pixels), for the light to follow its finer grain.
    float reliefDepth = 0;
    // Each tube's centre line in the scene's pixels (y down) with its radius
    // and how high its middle stands, to find what stands on it.
    struct Spine
    {
        std::vector<QVector3D> points; // x, y, radius
        std::vector<float> lift;
    };
    std::vector<Spine> spines;
};

// The tubes a decoration's scene describes (DecorationScene's `tubes`: each a
// centre line with the radius at each point, closed or not, a body or a
// branch, what it is made of and the viewer's colours for it); `halfWidth`
// is the band's half width in the scene's pixels. Tubes are swept along
// their centre lines - a ring of points round each, turned with the line -
// and the bark laid on them with whole repeats round and along a closed one,
// so it meets itself without a seam. `marks` (DecorationScene's `tubeMarks`:
// outlines drawn on them in plain colours, a knot hole, a birch's dark
// marks) are laid on the bark under them.
DecorationTubes decorationTubes(const QJsonArray &tubes, const QJsonArray &marks, float halfWidth,
                                const QString &materialDirectory);

// How high a decoration's tubes stand above the ground over a point (the
// scene's pixels, y down): the top of the highest tube there, 0 where none
// is.
float tubeHeightAt(const DecorationTubes &tubes, QPointF point);

} // namespace ArchDock
