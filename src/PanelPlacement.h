#pragma once

#include <QList>
#include <QString>

namespace ArchDock
{
struct EdgePanel
{
    QString id;
    QString edge;
    int screenIndex = -1;
    int thickness = 0;
    bool visible = false;
};

int edgeReserve(const QList<EdgePanel> &panels, const QString &edge, int screenIndex);
int edgeOffset(const QList<EdgePanel> &panels, const QString &panelId);
}