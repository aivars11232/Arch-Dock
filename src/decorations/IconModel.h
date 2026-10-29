#pragma once

#include "IconObject.h"

#include <QObject>
#include <QRectF>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <memory>

// An icon made into a 3D object (IconObject.h) for the 3D scenes, which draw
// it with IconModelGeometry (ArchDock.Decorations3D). Objects are made by
// the icon helper (arch-dock-icon-object), once for each picture, and kept
// in the user's cache folder, so the same picture anywhere (another panel,
// another of the dock's programs, the next session) is not made again.
// While one is being made, and where objects cannot be made (no helper or
// no models installed), there is none, and the scene shows the picture's
// own relief. What it tells of the object has the names the solids made in
// QML (IconSolid.js) use, so what reads one reads the other.
class IconModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // What the icon shows: a theme icon's name, or a picture's file or its
    // URL. Empty for none.
    Q_PROPERTY(QString iconSource READ iconSource WRITE setIconSource NOTIFY iconSourceChanged)
    // Where the icon's picture lies in the square the scene draws it in (0
    // to 1 across and down): the object is laid there, as large.
    Q_PROPERTY(QRectF frame READ frame WRITE setFrame NOTIFY frameChanged)
    // Whether objects can be made here at all.
    Q_PROPERTY(bool available READ available CONSTANT)
    // Whether there is an object, and whether one is being made.
    Q_PROPERTY(bool ready READ ready NOTIFY modelled)
    Q_PROPERTY(bool making READ making NOTIFY makingChanged)
    // Drawn by IconModelGeometry, not from lists of points.
    Q_PROPERTY(bool native READ native CONSTANT)
    Q_PROPERTY(QString kind READ kind CONSTANT)
    // How far its front reaches out of its middle, in the scene's units (its
    // square 100 across): its back reaches as far behind, so its reach is
    // the same. The Icon depth it shows true to its shape at (the scene
    // stretches it by Icon depth over that). Its mean colour,
    // [red, green, blue, 1], and its triangles.
    Q_PROPERTY(qreal depth READ depth NOTIFY modelled)
    Q_PROPERTY(qreal reach READ depth NOTIFY modelled)
    Q_PROPERTY(qreal builtDepth READ builtDepth CONSTANT)
    Q_PROPERTY(QVariantList colour READ colour NOTIFY modelled)
    Q_PROPERTY(int triangles READ triangles NOTIFY modelled)

public:
    explicit IconModel(QObject *parent = nullptr);
    ~IconModel() override;

    QString iconSource() const { return m_source; }
    void setIconSource(const QString &source);
    QRectF frame() const { return m_frame; }
    void setFrame(const QRectF &frame);

    static bool available();
    bool ready() const { return m_mesh != nullptr; }
    bool making() const { return m_making; }
    bool native() const { return true; }
    QString kind() const { return QStringLiteral("modelled"); }
    qreal depth() const { return m_mesh ? m_mesh->depth : 0; }
    qreal builtDepth() const { return 0.3; }
    QVariantList colour() const;
    int triangles() const { return m_mesh ? int(m_mesh->indices.size() / 3) : 0; }

    // The object as the scene draws it: laid in the frame.
    std::shared_ptr<const ArchDock::IconObjectMesh> mesh() const { return m_mesh; }

    // From the helper's queue: the object for `name` was made, or could not
    // be.
    void objectMade(const QString &name);
    void objectFailed(const QString &name);

signals:
    void iconSourceChanged();
    void frameChanged();
    void makingChanged();
    void modelled();

private:
    void restart();
    void load(const QString &name, int generation);
    void show(std::shared_ptr<const ArchDock::IconObjectMesh> object);
    void setMaking(bool making);

    QString m_source;
    QRectF m_frame = QRectF(0, 0, 1, 1);
    // The picture's object as made (laid in the picture's own square), and
    // as the scene draws it.
    std::shared_ptr<const ArchDock::IconObjectMesh> m_object;
    std::shared_ptr<const ArchDock::IconObjectMesh> m_mesh;
    QString m_name;
    bool m_making = false;
    std::shared_ptr<std::atomic<int>> m_wanted = std::make_shared<std::atomic<int>>(0);
};
