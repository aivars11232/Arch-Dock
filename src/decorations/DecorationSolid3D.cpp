#include "DecorationSolid3D.h"

#include <QFloat16>

DecorationSolid::DecorationSolid(QQuick3DObject *parent)
    : QQuick3DGeometry(parent)
{
}

void DecorationSolid::setBaker(QObject *baker)
{
    auto *next = qobject_cast<DecorationBaker *>(baker);
    if (m_baker == next) return;
    disconnect(m_baked);
    m_baker = next;
    if (next) m_baked = connect(next, &DecorationBaker::baked, this, &DecorationSolid::refresh);
    emit bakerChanged();
    refresh();
}

void DecorationSolid::setPart(const QString &part)
{
    if (m_part == part) return;
    m_part = part;
    emit partChanged();
    m_shown.reset();
    refresh();
}

void DecorationSolid::refresh()
{
    const bool tubes = m_part == QLatin1String("tubes");
    const auto relief = m_baker && !tubes ? m_baker->solid() : nullptr;
    const auto swept = m_baker && tubes ? m_baker->tubes() : nullptr;
    const std::shared_ptr<const void> now = tubes ? std::shared_ptr<const void>(swept) : std::shared_ptr<const void>(relief);
    if (now == m_shown && now) return;
    m_shown = now;
    clear();
    const std::vector<float> *vertices = relief ? &relief->vertices : swept ? &swept->mesh.vertices : nullptr;
    const std::vector<quint32> *indices = relief ? &relief->indices : swept ? &swept->mesh.indices : nullptr;
    static_assert(ArchDock::ReliefMesh::stride == ArchDock::TubeMesh::stride, "one vertex layout for both");
    if (vertices && indices && !indices->empty())
    {
        setVertexData(QByteArray(reinterpret_cast<const char *>(vertices->data()),
                                 qsizetype(vertices->size() * sizeof(float))));
        setIndexData(QByteArray(reinterpret_cast<const char *>(indices->data()),
                                qsizetype(indices->size() * sizeof(quint32))));
        setStride(int(ArchDock::ReliefMesh::stride * sizeof(float)));
        setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
        addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0, QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic, 6 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::TexCoord1Semantic, 8 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0, QQuick3DGeometry::Attribute::U32Type);
        if (relief) setBounds(relief->minimum, relief->maximum);
        else setBounds(swept->mesh.minimum, swept->mesh.maximum);
    }
    update();
}

DecorationMap::DecorationMap(QQuick3DObject *parent)
    : QQuick3DTextureData(parent)
{
    refresh();
}

void DecorationMap::setBaker(QObject *baker)
{
    auto *next = qobject_cast<DecorationBaker *>(baker);
    if (m_baker == next) return;
    disconnect(m_baked);
    m_baker = next;
    if (next) m_baked = connect(next, &DecorationBaker::baked, this, &DecorationMap::refresh);
    emit bakerChanged();
    refresh();
}

void DecorationMap::setMap(const QString &map)
{
    if (m_map == map) return;
    m_map = map;
    emit mapChanged();
    refresh();
}

void DecorationMap::refresh()
{
    const bool tube = m_map.startsWith(QLatin1String("tube"));
    const auto surface = m_baker && !tube ? m_baker->surface() : nullptr;
    const auto tubes = m_baker && tube ? m_baker->tubes() : nullptr;
    const std::shared_ptr<const void> now = tube ? std::shared_ptr<const void>(tubes) : std::shared_ptr<const void>(surface);
    if (now == m_shown && m_map == m_shownMap && (now || m_empty)) return;
    m_shown = now;
    m_shownMap = m_map;
    const bool colour = tube || m_map == QLatin1String("albedo") || m_map == QLatin1String("finish");
    QByteArray data;
    QSize size;
    if (tubes)
    {
        // The bark, its heights and a cut end's face, as RGBA8 pictures.
        const QImage &picture = m_map == QLatin1String("tubeRelief") ? tubes->relief
            : m_map == QLatin1String("tubeCap") ? tubes->cap : tubes->bark;
        if (!picture.isNull())
        {
            const QImage rgba = picture.convertToFormat(QImage::Format_RGBA8888);
            data = QByteArray(reinterpret_cast<const char *>(rgba.constBits()), rgba.sizeInBytes());
            size = rgba.size();
        }
    }
    else if (surface && colour)
    {
        data = m_map == QLatin1String("albedo") ? surface->albedo : surface->finish;
        size = surface->size;
    }
    else if (surface && m_map == QLatin1String("shape"))
    {
        data = surface->shape;
        size = surface->size;
    }
    else if (surface)
    {
        const auto light = surface->lights.constFind(m_map);
        if (light != surface->lights.constEnd())
        {
            data = light->data;
            size = light->rect.size();
        }
    }
    m_empty = data.isEmpty();
    if (m_empty)
    {
        // One dark pixel: nothing of it shows.
        size = QSize(1, 1);
        if (colour) data = QByteArray(4, '\0');
        else
        {
            const qfloat16 none[4] = {qfloat16(0.0f), qfloat16(0.0f), qfloat16(0.0f), qfloat16(1.0f)};
            data = QByteArray(reinterpret_cast<const char *>(none), sizeof(none));
        }
    }
    setFormat(colour ? QQuick3DTextureData::RGBA8 : QQuick3DTextureData::RGBA16F);
    setSize(size);
    setHasTransparency(false);
    setTextureData(data);
    update();
}
