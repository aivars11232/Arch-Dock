#pragma once

#include "IconModel.h"

#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/qquick3dgeometry.h>

#include <memory>

// An icon made into an object (IconModel) as Qt Quick 3D geometry: each
// point's place, normal, where on the picture it lies and its colour.
class IconModelGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT
    // The IconModel whose solid this is.
    Q_PROPERTY(QObject *model READ model WRITE setModel NOTIFY modelChanged)

public:
    explicit IconModelGeometry(QQuick3DObject *parent = nullptr);

    QObject *model() const { return m_model; }
    void setModel(QObject *model);

signals:
    void modelChanged();

private:
    void refresh();

    QPointer<IconModel> m_model;
    QMetaObject::Connection m_modelled;
    std::shared_ptr<const ArchDock::IconObjectMesh> m_shown;
};
