#include "DecorationLayer.h"

#include <QHash>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

namespace
{

// The node keeps a texture for each picture of the layer's loop; it is
// deleted on the render thread with them.
class LayerNode : public QSGSimpleTextureNode
{
public:
    ~LayerNode() override { qDeleteAll(textures); }
    QHash<qint64, QSGTexture *> textures;
};

} // namespace

DecorationLayer::DecorationLayer(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void DecorationLayer::setBaker(DecorationBaker *baker)
{
    if (m_baker == baker) return;
    if (m_baker) disconnect(m_baker, nullptr, this, nullptr);
    m_baker = baker;
    if (m_baker) connect(m_baker, &DecorationBaker::baked, this, &DecorationLayer::refresh);
    emit bakerChanged();
    refresh();
}

void DecorationLayer::setPicture(const QString &picture)
{
    if (m_picture == picture) return;
    m_picture = picture;
    emit pictureChanged();
    refresh();
}

void DecorationLayer::setFrame(int frame)
{
    if (m_frame == frame) return;
    m_frame = frame;
    emit frameChanged();
    refresh();
}

void DecorationLayer::refresh()
{
    m_image = m_baker ? m_baker->layerImage(m_picture, m_frame) : QImage();
    m_rect = m_baker ? m_baker->layerRect(m_picture, m_frame) : QRectF();
    m_keys = m_baker ? m_baker->layerKeys(m_picture) : QList<qint64>();
    const bool present = !m_image.isNull();
    if (present != m_present)
    {
        m_present = present;
        emit presentChanged();
    }
    update();
}

QSGNode *DecorationLayer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<LayerNode *>(oldNode);
    if (m_image.isNull() || width() <= 0 || height() <= 0 || !window() || !m_baker)
    {
        delete node;
        return nullptr;
    }
    if (!node)
    {
        node = new LayerNode();
        node->setFiltering(QSGTexture::Linear);
    }
    QSGTexture *texture = node->textures.value(m_image.cacheKey());
    if (!texture)
    {
        texture = window()->createTextureFromImage(m_image, QQuickWindow::TextureHasAlphaChannel);
        node->textures.insert(m_image.cacheKey(), texture);
    }
    node->setTexture(texture);
    // Pictures the baker no longer has (an earlier decoration's) go.
    for (auto it = node->textures.begin(); it != node->textures.end();)
    {
        if (it.value() != texture && !m_keys.contains(it.key()))
        {
            delete it.value();
            it = node->textures.erase(it);
        }
        else
        {
            ++it;
        }
    }
    // The item covers the baker's bounds; the picture its own part of them.
    const QRectF bounds = m_baker->bounds();
    const qreal sx = bounds.width() > 0 ? width() / bounds.width() : 1;
    const qreal sy = bounds.height() > 0 ? height() / bounds.height() : 1;
    node->setRect(QRectF(m_rect.x() * sx, m_rect.y() * sy, m_rect.width() * sx, m_rect.height() * sy));
    return node;
}
