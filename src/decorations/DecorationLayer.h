#pragma once

#include "DecorationBaker.h"

#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

// Shows one of a DecorationBaker's pictures in one frame of its loop,
// stretched over the item, which covers the baker's bounds. Each picture
// is uploaded once and kept while the baker has it, so stepping through a
// loop or pulsing the item's opacity costs nothing; it works on the
// software renderer too.
class DecorationLayer : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(DecorationBaker *baker READ baker WRITE setBaker NOTIFY bakerChanged)
    // Which of the baker's pictures: "base", or a group of lights.
    Q_PROPERTY(QString picture READ picture WRITE setPicture NOTIFY pictureChanged)
    Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
    // Whether the baker has that picture in this frame.
    Q_PROPERTY(bool present READ present NOTIFY presentChanged)

public:
    explicit DecorationLayer(QQuickItem *parent = nullptr);

    DecorationBaker *baker() const { return m_baker; }
    void setBaker(DecorationBaker *baker);
    QString picture() const { return m_picture; }
    void setPicture(const QString &picture);
    int frame() const { return m_frame; }
    void setFrame(int frame);
    bool present() const { return m_present; }

signals:
    void bakerChanged();
    void pictureChanged();
    void frameChanged();
    void presentChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private:
    void refresh();

    QPointer<DecorationBaker> m_baker;
    QString m_picture = QStringLiteral("base");
    int m_frame = 0;
    QImage m_image;
    QRectF m_rect;
    QList<qint64> m_keys;
    bool m_present = false;
};
