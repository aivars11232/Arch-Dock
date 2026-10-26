#pragma once

#include "DecorationBaker.h"

#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dgeometry.h>
#include <QtQuick3D/qquick3dtexturedata.h>

#include <memory>

// A decoration made solid (DecorationBaker with `relief`), as Qt Quick 3D
// geometry: its upper surface, walls, underside and their normals, the
// surface maps' coordinates in the first texture coordinates, and in the
// second's x 1 on the upper surface and 0 elsewhere, its y how far round a
// rounded edge's curve the point lies (1 where it has ended or there is
// none) (`part` "relief"); or
// its tubes (`part` "tubes": a trunk and its branches, the bark's texture
// coordinates in the first, and in the second's x 1 on the bark and 2 on a
// cut end).
class DecorationSolid : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT
    // The DecorationBaker whose solid this is.
    Q_PROPERTY(QObject *baker READ baker WRITE setBaker NOTIFY bakerChanged)
    Q_PROPERTY(QString part READ part WRITE setPart NOTIFY partChanged)

public:
    explicit DecorationSolid(QQuick3DObject *parent = nullptr);

    QObject *baker() const { return m_baker; }
    void setBaker(QObject *baker);
    QString part() const { return m_part; }
    void setPart(const QString &part);
    // The solid's vertices, at most about `most` of them spread evenly
    // through the mesh, as one flat list of x, y, z in the solid's own
    // frame: enough for the outline of the decoration as drawn, which a
    // folder's opening keeps clear of. Empty for the tubes.
    Q_INVOKABLE QVariantList outline(int most) const;

signals:
    void bakerChanged();
    void partChanged();

private:
    void refresh();

    QPointer<DecorationBaker> m_baker;
    QMetaObject::Connection m_baked;
    QString m_part = QStringLiteral("relief");
    std::shared_ptr<const void> m_shown;
    std::shared_ptr<const ArchDock::ReliefMesh> m_relief;
};

// One of a solid decoration's surface maps as a texture: "albedo",
// "finish" and "shape" (DecorationSurface), a group of lights' own
// ("lights", "stars", "starsB", "blinkA", "blinkB", "arcs"), which covers
// only DecorationBaker::lightRect of the surface, or its tubes' bark
// ("tubeBark", "tubeRelief", "tubeCap"). A map the decoration does not have
// is one dark pixel.
class DecorationMap : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *baker READ baker WRITE setBaker NOTIFY bakerChanged)
    Q_PROPERTY(QString map READ map WRITE setMap NOTIFY mapChanged)

public:
    explicit DecorationMap(QQuick3DObject *parent = nullptr);

    QObject *baker() const { return m_baker; }
    void setBaker(QObject *baker);
    QString map() const { return m_map; }
    void setMap(const QString &map);

signals:
    void bakerChanged();
    void mapChanged();

private:
    void refresh();

    QPointer<DecorationBaker> m_baker;
    QMetaObject::Connection m_baked;
    QString m_map;
    std::shared_ptr<const void> m_shown;
    QString m_shownMap;
    bool m_empty = false;
};
