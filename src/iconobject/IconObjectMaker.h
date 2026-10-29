#pragma once

#include "IconObject.h"

#include <QImage>
#include <QString>

#include <memory>
#include <optional>

namespace ArchDock
{

// Makes an icon into a 3D object with TripoSR (Tripo AI and Stability AI,
// MIT licence: "TripoSR: Fast 3D Object Reconstruction from a Single
// Image", 2024), run by ONNX Runtime on the processor: its network sees the
// picture and gives the object as three planes of features; its field (a
// small decoder) tells from those how dense the object is, and its colour,
// at any point of the box round it. The surface is found where TripoSR
// finds it (density 25 on a 256-point grid, by marching cubes) but asked
// only near the surface (trackedField); then the part in front of its
// widest slice is kept, made lighter, and mirrored into its back
// (twoFronts), and laid in the icon's frame, each point told where on the
// picture it lies as TripoSR's own camera sees it.
class IconObjectMaker
{
public:
    explicit IconObjectMaker(const QString &models, int threads);
    ~IconObjectMaker();

    bool ready() const { return m_sessions != nullptr; }
    QString error() const { return m_error; }

    // The object for a picture with its own transparency (any size, its
    // object on a clear ground); none when it cannot be made.
    std::optional<IconObjectMesh> make(const QImage &picture);

    // How many triangles the front keeps at most (the back as many).
    std::size_t frontTriangles = 20000;

    // How long the last one took, in milliseconds: the network, the field
    // and the rest.
    double networkTime = 0;
    double fieldTime = 0;
    double meshTime = 0;

private:
    struct Sessions;
    std::unique_ptr<Sessions> m_sessions;
    QString m_error;
};

} // namespace ArchDock
