#pragma once

#include "DecorationLighting.h"
#include "DecorationRelief.h"
#include "DecorationTubes.h"

#include <QImage>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

// Works out a panel decoration on a path and lights it, off the panel's own
// thread (ArchDock::DecorationWorker), and keeps the pictures: per frame of
// its motion a base and one picture per group of lights. A decoration that
// does not move has one frame. New settings replace a bake still running;
// its pictures are dropped. The first frame is shown as soon as it is lit,
// the rest of the loop follows. For a 3D scene (`relief`) it keeps the
// decoration as a solid with its surface maps instead, unlit, for the scene
// to light as it turns.
class DecorationBaker : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // ArchDock.Rendering's folder: the decoration scripts and, in its
    // materials folder, the materials' pictures.
    Q_PROPERTY(QUrl directory READ directory WRITE setDirectory NOTIFY inputChanged)
    // The decoration: a catalogue id (DecorationCatalogue.js), or its data
    // (a decoration as the catalogue writes one; a material's swatch, a
    // package's decoration), which wins when it is set.
    Q_PROPERTY(QString decoration READ decoration WRITE setDecoration NOTIFY inputChanged)
    Q_PROPERTY(QVariant decorationData READ decorationData WRITE setDecorationData NOTIFY inputChanged)
    // The panel's path: { points: [{x, y}], closed, outward }.
    Q_PROPERTY(QVariant path READ path WRITE setPath NOTIFY inputChanged)
    // Half the band's width, in the path's pixels.
    Q_PROPERTY(qreal halfWidth READ halfWidth WRITE setHalfWidth NOTIFY inputChanged)
    // The icons' places along the path (pixels of its length).
    Q_PROPERTY(QVariant iconPlaces READ iconPlaces WRITE setIconPlaces NOTIFY inputChanged)
    // The viewer's colours (DecorationScene.scene's choices).
    Q_PROPERTY(QVariant choices READ choices WRITE setChoices NOTIFY inputChanged)
    // Draw the frames of the decoration's motion.
    Q_PROPERTY(bool animated READ animated WRITE setAnimated NOTIFY inputChanged)
    // How far (half-widths) an open band runs on past its first and last
    // icons.
    Q_PROPERTY(qreal ends READ ends WRITE setEnds NOTIFY inputChanged)
    // { out, in }: the room across the band an edge panel has; the band is
    // drawn narrower if the decoration would not fit. Unset: as asked.
    Q_PROPERTY(QVariant fit READ fit WRITE setFit NOTIFY inputChanged)
    // { x, y, width, height, fade }: the window the panel is seen through;
    // the pictures fade out at its edge. Unset: no window.
    Q_PROPERTY(QVariant clip READ clip WRITE setClip NOTIFY inputChanged)
    // Device pixels per path pixel to make the pictures at.
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY inputChanged)
    // Make the decoration a solid for a 3D scene: its surface maps and its
    // mesh (ReliefMesh), its underside `underside` path pixels below its
    // ground, with at most about `triangles` triangles. Its motion is not
    // drawn; its lights still have maps of their own.
    Q_PROPERTY(bool relief READ relief WRITE setRelief NOTIFY inputChanged)
    Q_PROPERTY(qreal underside READ underside WRITE setUnderside NOTIFY inputChanged)
    Q_PROPERTY(int triangles READ triangles WRITE setTriangles NOTIFY inputChanged)

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // Counts what was lit, so a layer knows its pictures changed.
    Q_PROPERTY(int revision READ revision NOTIFY baked)
    // Where the pictures lie, in the path's pixels.
    Q_PROPERTY(QRectF bounds READ bounds NOTIFY baked)
    // The groups of lights that have pictures of their own.
    Q_PROPERTY(QStringList layers READ layers NOTIFY baked)
    // The frames lit so far, and how many the loop has (1 when still).
    Q_PROPERTY(int frames READ frames NOTIFY baked)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY baked)
    // One loop of the motion, in seconds (0 when still).
    Q_PROPERTY(qreal loop READ loop NOTIFY baked)
    // The half-width the band was drawn at (smaller than asked when it had
    // to fit an edge panel).
    Q_PROPERTY(qreal drawnHalfWidth READ drawnHalfWidth NOTIFY baked)
    // Where the decoration's launcher seat is, in the path's pixels:
    // { x, y, size }, or null when it has none.
    Q_PROPERTY(QVariant launcher READ launcher NOTIFY baked)
    // Its round lights (lenses), which brighten under the pointer:
    // [{ x, y, radius, colour, group }] in the path's pixels.
    Q_PROPERTY(QVariantList lightSpots READ lightSpots NOTIFY baked)
    Q_PROPERTY(QString error READ error NOTIFY baked)
    // As a solid: how high the decoration stands under each icon (path
    // pixels above its ground), and how many triangles its mesh has.
    Q_PROPERTY(QVariantList iconHeights READ iconHeights NOTIFY baked)
    Q_PROPERTY(int triangleCount READ triangleCount NOTIFY baked)
    // The surface maps' size in their pixels, and the band's half width in
    // them (the maps may be made smaller than asked for a long panel).
    Q_PROPERTY(QSize mapSize READ mapSize NOTIFY baked)
    Q_PROPERTY(qreal mapHalfWidth READ mapHalfWidth NOTIFY baked)
    // Its tubes' bark: how deep its relief is (path pixels), how rough and
    // how metallic it is.
    Q_PROPERTY(qreal tubeReliefDepth READ tubeReliefDepth NOTIFY baked)
    Q_PROPERTY(qreal tubeRoughness READ tubeRoughness NOTIFY baked)
    Q_PROPERTY(qreal tubeMetalness READ tubeMetalness NOTIFY baked)

public:
    explicit DecorationBaker(QObject *parent = nullptr);
    ~DecorationBaker() override;

    QUrl directory() const { return m_directory; }
    void setDirectory(const QUrl &directory);
    QString decoration() const { return m_decoration; }
    void setDecoration(const QString &decoration);
    QVariant decorationData() const { return m_decorationData; }
    void setDecorationData(const QVariant &data);
    QVariant path() const { return m_path; }
    void setPath(const QVariant &path);
    qreal halfWidth() const { return m_halfWidth; }
    void setHalfWidth(qreal halfWidth);
    QVariant iconPlaces() const { return m_iconPlaces; }
    void setIconPlaces(const QVariant &places);
    QVariant choices() const { return m_choices; }
    void setChoices(const QVariant &choices);
    bool animated() const { return m_animated; }
    void setAnimated(bool animated);
    qreal ends() const { return m_ends; }
    void setEnds(qreal ends);
    QVariant fit() const { return m_fit; }
    void setFit(const QVariant &fit);
    QVariant clip() const { return m_clip; }
    void setClip(const QVariant &clip);
    qreal scale() const { return m_scale; }
    void setScale(qreal scale);
    bool relief() const { return m_relief; }
    void setRelief(bool relief);
    qreal underside() const { return m_underside; }
    void setUnderside(qreal underside);
    int triangles() const { return m_triangles; }
    void setTriangles(int triangles);

    bool busy() const { return m_busy; }
    int revision() const { return m_revision; }
    QRectF bounds() const { return m_bounds; }
    QStringList layers() const { return m_layers; }
    int frames() const { return m_solid ? 1 : int(m_frames.size()); }
    int frameCount() const { return m_frameCount; }
    qreal loop() const { return m_loop; }
    qreal drawnHalfWidth() const { return m_drawnHalfWidth; }
    QVariant launcher() const { return m_launcher; }
    QVariantList lightSpots() const { return m_lightSpots; }
    QString error() const { return m_error; }
    QVariantList iconHeights() const { return m_iconHeights; }
    int triangleCount() const
    {
        return (m_solid ? int(m_solid->indices.size() / 3) : 0) + (m_tubes ? int(m_tubes->mesh.indices.size() / 3) : 0);
    }
    QSize mapSize() const { return m_surface ? m_surface->size : QSize(); }
    qreal mapHalfWidth() const { return m_surface ? qreal(m_surface->halfWidth) : 0; }
    qreal tubeReliefDepth() const { return m_tubes ? qreal(m_tubes->reliefDepth) : 0; }
    qreal tubeRoughness() const { return m_tubes ? qreal(m_tubes->roughness) : 0.7; }
    qreal tubeMetalness() const { return m_tubes ? qreal(m_tubes->metalness) : 0; }

    // A layer's picture ("base", "lights", ...) in a frame, empty when the
    // frame has none; the frames past those lit so far show the last.
    QImage layerImage(const QString &name, int frame = 0) const;
    // Where that picture lies in the whole, in path pixels from the
    // bounds' corner.
    QRectF layerRect(const QString &name, int frame = 0) const;
    // The pictures a layer has over all frames, by their cache keys, so a
    // layer can drop what it no longer shows.
    QList<qint64> layerKeys(const QString &name) const;

    // As a solid: its surface maps and its mesh, shared with the scene's
    // geometry and textures (null until made, and when not a solid).
    std::shared_ptr<const ArchDock::DecorationSurface> surface() const { return m_surface; }
    std::shared_ptr<const ArchDock::ReliefMesh> solid() const { return m_solid; }
    // What is round all the way round (a trunk, its branches), as tubes.
    std::shared_ptr<const ArchDock::DecorationTubes> tubes() const { return m_tubes; }
    // Where a group's light map lies on the surface maps, 0 to 1 across
    // them; empty when the group has none.
    Q_INVOKABLE QRectF lightRect(const QString &name) const;

    // A frame lit on the worker's thread.
    void acceptFrame(int generation, int index, int count, qreal loop, qreal halfWidth, const QVariant &launcher,
                     const QVariantList &lightSpots, const ArchDock::DecorationBake &bake);
    // A solid made on the worker's thread.
    void acceptSolid(int generation, qreal halfWidth, const QVariant &launcher, const QVariantList &lightSpots,
                     const std::shared_ptr<const ArchDock::DecorationSurface> &surface,
                     const std::shared_ptr<const ArchDock::ReliefMesh> &solid,
                     const std::shared_ptr<const ArchDock::DecorationTubes> &tubes, const QVariantList &iconHeights);

signals:
    void inputChanged();
    void busyChanged();
    void baked();

private:
    void schedule();
    void start();
    void setBusy(bool busy);
    void clearPictures(const QString &error);

    QUrl m_directory;
    QString m_decoration;
    QVariant m_decorationData, m_path, m_iconPlaces, m_choices, m_fit, m_clip;
    qreal m_halfWidth = 24;
    bool m_animated = true;
    qreal m_ends = 0;
    qreal m_scale = 1;
    bool m_relief = false;
    qreal m_underside = 0;
    int m_triangles = 60000;

    QString m_input;
    bool m_busy = false;
    int m_revision = 0;
    int m_generation = 0;
    // The generation still wanted, shared with the jobs on the worker.
    std::shared_ptr<std::atomic<int>> m_wanted = std::make_shared<std::atomic<int>>(0);
    QRectF m_bounds;
    qreal m_pictureScale = 1;
    QStringList m_layers;
    int m_frameCount = 0;
    qreal m_loop = 0;
    qreal m_drawnHalfWidth = 0;
    QVariant m_launcher;
    QVariantList m_lightSpots;
    QString m_error;
    QList<ArchDock::DecorationBake> m_frames;
    std::shared_ptr<const ArchDock::DecorationSurface> m_surface;
    std::shared_ptr<const ArchDock::ReliefMesh> m_solid;
    std::shared_ptr<const ArchDock::DecorationTubes> m_tubes;
    QVariantList m_iconHeights;
    QTimer m_delay;
};
