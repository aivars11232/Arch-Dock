#include "IconSolid3D.h"

IconModelGeometry::IconModelGeometry(QQuick3DObject *parent)
    : QQuick3DGeometry(parent)
{
}

void IconModelGeometry::setModel(QObject *model)
{
    auto *next = qobject_cast<IconModel *>(model);
    if (m_model == next) return;
    disconnect(m_modelled);
    m_model = next;
    if (next) m_modelled = connect(next, &IconModel::modelled, this, &IconModelGeometry::refresh);
    emit modelChanged();
    refresh();
}

void IconModelGeometry::refresh()
{
    const auto mesh = m_model ? m_model->mesh() : nullptr;
    if (mesh == m_shown && mesh) return;
    m_shown = mesh;
    clear();
    if (mesh)
    {
        const auto &v = mesh->vertices;
        const auto &i = mesh->indices;
        setVertexData(QByteArray(reinterpret_cast<const char *>(v.data()), qsizetype(v.size() * sizeof(float))));
        setIndexData(QByteArray(reinterpret_cast<const char *>(i.data()), qsizetype(i.size() * sizeof(quint32))));
        setStride(int(ArchDock::IconObjectMesh::stride * sizeof(float)));
        setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
        addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0, QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::TexCoord0Semantic, 6 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::ColorSemantic, 8 * sizeof(float), QQuick3DGeometry::Attribute::F32Type);
        addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0, QQuick3DGeometry::Attribute::U32Type);
        setBounds(mesh->minimum, mesh->maximum);
    }
    update();
}
