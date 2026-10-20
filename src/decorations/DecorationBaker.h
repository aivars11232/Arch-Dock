#pragma once

#include "DecorationLighting.h"

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
// the rest of the loop follows.
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
    Q_PROPERTY(QString error READ error NOTIFY baked)

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

    bool busy() const { return m_busy; }
    int revision() const { return m_revision; }
    QRectF bounds() const { return m_bounds; }
    QStringList layers() const { return m_layers; }
    int frames() const { return int(m_frames.size()); }
    int frameCount() const { return m_frameCount; }
    qreal loop() const { return m_loop; }
    qreal drawnHalfWidth() const { return m_drawnHalfWidth; }
    QVariant launcher() const { return m_launcher; }
    QString error() const { return m_error; }

    // A layer's picture ("base", "lights", ...) in a frame, empty when the
    // frame has none; the frames past those lit so far show the last.
    QImage layerImage(const QString &name, int frame = 0) const;
    // Where that picture lies in the whole, in path pixels from the
    // bounds' corner.
    QRectF layerRect(const QString &name, int frame = 0) const;
    // The pictures a layer has over all frames, by their cache keys, so a
    // layer can drop what it no longer shows.
    QList<qint64> layerKeys(const QString &name) const;

    // A frame lit on the worker's thread.
    void acceptFrame(int generation, int index, int count, qreal loop, qreal halfWidth, const QVariant &launcher,
                     const ArchDock::DecorationBake &bake);

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
    QString m_error;
    QList<ArchDock::DecorationBake> m_frames;
    QTimer m_delay;
};
