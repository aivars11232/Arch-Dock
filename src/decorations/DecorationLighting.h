#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector2D>
#include <QVector3D>

#include <functional>
#include <vector>

namespace ArchDock
{

// The decoration lighting's result: the pictures of a decoration lit per
// pixel, and where they go.
struct DecorationBake
{
    // Where the pictures lie, in the scene's own pixels (the panel's).
    QRect bounds;
    // Device pixels per scene pixel the pictures were made at.
    qreal scale = 1;
    // "base" (the decoration's surfaces, lit, with the shadow it casts) and
    // one picture per group of lights ("lights", "stars", "starsB",
    // "blinkA", "blinkB", "arcs") holding that group's light and halo, so a
    // group can pulse or blink on its own over the base.
    QHash<QString, QImage> layers;
    // Where each picture lies in the whole (device pixels from the bounds'
    // corner): the base covers it all, a group's lights only as far as
    // their halo reaches.
    QHash<QString, QRect> rects;
    QString error;
};

// Lights a decoration's scene, as DecorationScene.js makes it, per pixel:
// every element is drawn into its own box (its coverage, its albedo - a
// colour, a gradient or a real material laid along the band - its relief
// and the height its shape gives it from its distance to its own edge);
// the elements stack into one height field with its albedo, metalness,
// roughness, clear coat and light; that field is lit by a key light from
// the top left with the shadows the relief casts, a photographer's studio
// mirrored in metal and lacquer (a softbox overhead, a strip light at the
// left), Blinn-Phong highlights with Schlick's Fresnel term, and ambient
// occlusion; lights keep their colour and give a halo (Gaussian bloom at
// several radii). `materialDirectory` holds the materials' pictures
// (deco-<texture>.jpg or .png and deco-<texture>-height.png). The pictures
// are made at `scale` device pixels per scene pixel, smaller if they would
// exceed `maximumPixels`. `cancelled`, when given, is asked now and then
// whether the pictures are still wanted; when it says no, the bake stops
// with the error "cancelled".
DecorationBake bakeDecoration(const QJsonObject &scene, const QString &materialDirectory, qreal scale,
                              qsizetype maximumPixels = 3'000'000,
                              const std::function<bool()> &cancelled = {});

// A decoration's surfaces before any light, for a renderer that lights them
// itself (a 3D scene, as it turns): the same stacked field bakeDecoration
// lights, as maps. Row by row from the maps' top left corner, which lies at
// the bounds' corner.
struct DecorationSurface
{
    // As DecorationBake's: in the scene's own pixels, and the maps' pixels
    // per scene pixel.
    QRect bounds;
    qreal scale = 1;
    // The band's half width, in the maps' pixels.
    float halfWidth = 0;
    QSize size;
    // How much of each pixel the decoration covers (0 to 1), and how high
    // it stands there, in the maps' pixels (below 0 in a groove).
    std::vector<float> cover;
    std::vector<float> rise;
    // RGBA8: the colour (sRGB, with the ink drawn on it) and, in alpha, how
    // much of the room's light reaches it (ambient occlusion, as the flat
    // lighting has it).
    QByteArray albedo;
    // RGBA8: metalness, roughness, clear coat, and the light tent's gleam
    // over 3.
    QByteArray finish;
    // RGBA16F: the height lightly smoothed (the maps' pixels), how much of
    // it is a light's own face, how much of the round about it covers
    // (evened out over a few pixels), and how much of its colour is its own
    // light (a nebula's picture: a 3D scene's light only shades it).
    QByteArray shape;
    // Each group's light ("lights", "stars", "starsB", "blinkA", "blinkB",
    // "arcs"): RGBA16F, unbounded (a light's core is brighter than 1), only
    // over `rect`, the pixels the group's light covers.
    struct Light
    {
        QRect rect;
        QByteArray data;
    };
    QHash<QString, Light> lights;
    // Once its edge is rounded (roundSurfaceEdges), for a solid: how far
    // round the curve along its outline each pixel lies (0 on the outline,
    // 1 where the curve ends and the top lies as the decoration stands, below
    // 0 outside), the top's height in general there (evened out, which the
    // curve starts from), and how deep the solid stands below its ground,
    // all in the maps' pixels.
    std::vector<float> edgeAlong, edgeLevel;
    float edgeDeep = 0;
    QString error;
};

// A material's picture (deco-<texture>.jpg or .png in `materialDirectory`)
// as an element's albedo and the viewer's colours make it, in sRGB: tinted
// (`albedo`'s tint), moved to a chosen colour with its grain kept
// (`recolour`: hue, saturation, value) and its hue, strength and brightness
// changed (`adjust`), as bakeDecoration colours an element. An albedo with
// no picture is its plain colour. And the material's heights
// (deco-<texture>-height.png), grey, or a null picture when it has none.
QImage materialAlbedo(const QString &materialDirectory, const QJsonObject &albedo, const QJsonObject &recolour,
                      const QJsonObject &adjust);
QImage materialHeights(const QString &materialDirectory, const QString &texture);

// The decoration's surfaces as bakeDecoration stacks them, unlit; for a solid
// its outline is smooth where the flat pictures have a fine row of notches
// along an edge (studs, ribs), which would stand as fins down its walls.
DecorationSurface decorationSurface(const QJsonObject &scene, const QString &materialDirectory, qreal scale,
                                    qsizetype maximumPixels = 3'000'000,
                                    const std::function<bool()> &cancelled = {});

// A decoration's surface made ready to be a solid's: past its outline, and
// on its half-covered pixels, the maps carry what lies just inside, so a
// blurrier level of them read at the edge (a wall's, a steep side's) shows
// the decoration, not the dark round it, evenly all along the edge; a
// light's only a pixel or two out, so a star by the edge stays a star.
void fillSurfaceMargins(DecorationSurface &surface);

// A soft decoration's surface (a nebula's) made ready to stand as a solid
// `underside` (the scene's pixels) deep with no straight wall round it (its
// edge* fields): round its outline the top curves down and the underside
// curves up to meet halfway, a quarter ellipse each from the part's middle
// to its outline, the underside's the top's mirrored, so the body rounds
// like a cushion all the way over (its maps' margins filled first,
// fillSurfaceMargins, as the curve reads them right at the outline).
void roundSurfaceEdges(DecorationSurface &surface, float underside);

// A point of a rounded solid's edge: `along` the curve round its outline (0
// to 1, its slope across the maps per pixel, y down), where the decoration
// stands `height` high (and its slope), its top `level` high in general, the
// solid `deep` deep below its ground (the maps' pixels). Its top's height and
// its underside's there, each one's normal (the solid's axes: x right, y up,
// z up out of its top), and how far further in its maps are read: the
// picture laid round the curve evenly, as a label round a rounded edge, not
// stretched down its steep part.
struct EdgePoint
{
    float top = 0;
    float bottom = 0;
    QVector3D upward;
    QVector3D downward;
    float inward = 0;
};
EdgePoint edgePoint(float along, QVector2D alongSlope, float height, QVector2D heightSlope, float level, float deep);

} // namespace ArchDock
