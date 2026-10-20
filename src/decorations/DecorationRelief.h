#pragma once

#include "DecorationLighting.h"

#include <QVector3D>
#include <QtGlobal>

#include <vector>

namespace ArchDock
{

// A decoration made solid, for a 3D scene: its upper surface rises as high
// as the decoration stands at each point and an underside closes it: flat
// `underside` below the ground, with walls dropping from the outline to it,
// or, a soft decoration's (roundSurfaceEdges), curving round with the top to
// meet it halfway all round the outline. In the scene's own pixels with y up (x right, y from the bounds' bottom up, z
// out of the decoration toward the viewer); z 0 is the decoration's ground.
// What is round all the way round (a trunk, its branches) is a tube of its
// own (DecorationTubes).
struct ReliefMesh
{
    // Per vertex: position (3), normal (3), the surface maps' texture
    // coordinate (2), 1 on the upper surface and 0 on the walls and the
    // underside, and how far round a rounded edge's curve it lies (0 on the
    // outline, 1 where the curve has ended or there is none).
    static constexpr int stride = 10;
    std::vector<float> vertices;
    std::vector<quint32> indices;
    QVector3D minimum;
    QVector3D maximum;
    // The maps' pixels between two points of its grid.
    float step = 0;
};

// The solid of a decoration's surface, with at most about `maximumTriangles`
// triangles: the grid as fine as that allows, never finer than a pixel. Its
// outline is where the decoration covers half a pixel (marching squares, the
// four-corner case table of Lorensen and Cline's marching cubes in two
// dimensions, a saddle decided by its cell's middle), or, its edge rounded,
// where the curve round it begins.
ReliefMesh reliefMesh(const DecorationSurface &surface, float underside, int maximumTriangles = 48000);

} // namespace ArchDock
