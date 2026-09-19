#pragma once

#include "../WindowItem.h"

#include <QList>
#include <QVariantList>

namespace ArchDock
{
// The applet receives value models over D-Bus. Project the existing grouped
// windows rather than maintain a second tracker or export a process-local model.
class WindowPreviewModel final
{
public:
    static QVariantList entries(const QList<WindowItem> &windows,
                                const QString &applicationTitle);
};
}
