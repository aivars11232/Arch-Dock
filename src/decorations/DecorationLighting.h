#pragma once

#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QRect>
#include <QString>

#include <functional>

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

} // namespace ArchDock
