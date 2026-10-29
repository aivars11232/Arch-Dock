#pragma once

#include <QString>
#include <QVector3D>

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

namespace ArchDock
{

// An icon made into a 3D object, as the icon helper (arch-dock-icon-object)
// makes it from TripoSR's object (IconObjectMaker): the part of the object
// in front of its widest slice, and a mirror copy of that part facing the
// other way as its back, so from either side the icon's front shows.
struct IconObjectMesh
{
    // Each point: its place (3), its normal (3), where on the icon's picture
    // it lies as seen straight on (2: 0 to 1 across, and up, the picture)
    // and its colour (4, the fourth 1).
    static constexpr int stride = 12;
    std::vector<float> vertices;
    std::vector<quint32> indices;
    // In the icon's units: its picture 100 across and centred, y up the
    // picture, the front toward the viewer, the middle (where front and
    // back meet) at z 0.
    QVector3D minimum;
    QVector3D maximum;
    // How far the front reaches out of the middle; the back reaches as far
    // behind.
    float depth = 0;
    // Its mean colour, [red, green, blue, 1].
    float colour[4] = {1, 1, 1, 1};
};

// Where the installed models for the icons' objects lie (their folder), or
// empty: ARCHDOCK_ICON_OBJECT_MODELS, or arch-dock/models in the data
// folders (the user's own first), holding triposr-network.onnx and
// triposr-field.onnx (made by tools/icon-objects/export-triposr.py).
QString iconObjectModels();

// Kept in a file of its own (written whole or not at all), read back.
bool saveIconObject(const IconObjectMesh &mesh, const QString &path);
std::optional<IconObjectMesh> loadIconObject(const QString &path);

// The steps the object is made by, each on its own, so tests can try them
// on fields of their own.

// A field's values over a grid `side` points a side (the first index the
// slowest), asked only where its surface (where the values pass `level`)
// lies: the grid is asked every `stride`th point first; the cells where
// inside and outside meet are then asked at every point (the coarse-to-fine
// step of Mescheder et al., "Occupancy Networks", CVPR 2019), and the
// surface is followed from cell to cell wherever it leaves one through a
// face (the continuation of Bloomenthal, "An Implicit Surface Polygonizer",
// Graphics Gems IV, 1994). Every cell the surface passes holds the values
// asking every point would give; a point not asked takes the value of the
// nearest point asked first, which lies on its side of the surface. `ask`
// is given points as (i, j, k) triples and writes one value for each.
// `side - 1` must be a multiple of `stride`.
using AskField = std::function<void(const int *points, std::size_t count, float *values)>;
std::vector<float> trackedField(int side, int stride, float level, const AskField &ask,
                                std::size_t *asked = nullptr);

// Marching cubes over a grid of values (PyMCubes' own, P. M. Neila, BSD
// licence): the surface where they pass `level`, its points in grid steps
// (x, y, z for the first, second and third index), three indexes for each
// triangle; points at the very same place made one, as trimesh makes
// TripoSR's own meshes, and triangles left without area dropped.
void marchingCubes(const std::vector<float> &values, int side, float level, std::vector<float> &points,
                   std::vector<quint32> &triangles);

// A triangle mesh with a colour for each point.
struct ColouredMesh
{
    std::vector<float> points;  // x, y, z
    std::vector<float> colours; // red, green, blue
    std::vector<quint32> triangles;
};

// Where along x a closed mesh's cross-section is widest: the middle of the
// slices within 3 % of the widest.
float widestSlice(const ColouredMesh &mesh);

// The part of a mesh where x >= `cut`, each triangle the cut crosses cut
// too; the points made on the cut lie on it exactly.
ColouredMesh frontOf(const ColouredMesh &mesh, float cut);

// The mesh with fewer triangles (meshoptimizer's simplifier, A. Kapoulkine,
// MIT licence: edge collapses by the quadric error of Garland and Heckbert,
// "Surface Simplification Using Quadric Error Metrics", 1997, its colours
// weighed too): at most `triangles`, never further off than `error` (a
// share of the mesh's size); the open border stays where it is.
ColouredMesh simplified(const ColouredMesh &mesh, std::size_t triangles, float error);

// A front and its mirror copy across x = `cut`, facing the other way, the
// points on the cut shared: closed where the front's open border lay on it.
// Triangles lying in the cut itself are left out.
ColouredMesh twoFronts(const ColouredMesh &front, float cut);

} // namespace ArchDock
