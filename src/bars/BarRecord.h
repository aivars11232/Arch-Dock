// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BarWidgets.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace ArchDock::Bars
{
// One Arch Dock bar as the backend describes it to the bar host: which panel
// it is, what it holds, and where and how it stands.
struct BarRecord
{
    QString id;
    // What Arch Dock's dock in it shows; an empty bar's shows the bar's own
    // items.
    QString type = QStringLiteral("hybrid");
    // What it holds, in order along it: the dock and Plasma widgets.
    QList<BarWidget> widgets{BarWidget{BarWidget::dockKey(), {}}};
    // Its screen: by the screen's lasting name, else by its place in the
    // screen list (ScreenIdentity).
    QString screenId;
    int screenIndex = 0;
    QString edge = QStringLiteral("bottom");
    QString alignment = QStringLiteral("center");
    // Its place among the bars with the same alignment on its edge.
    int order = 0;
    int offset = 0;
    // How deep it is, off the edge.
    int thickness = 76;
    // How long it is along the edge: as long as it wants to be within the
    // minimum and maximum ("fit"), as long as set ("fixed"), or as long as
    // the edge ("fill").
    QString lengthMode = QStringLiteral("fixed");
    int fixedLength = 720;
    int minimumLength = 48;
    int maximumLength = 4096;
    // How far it stands off the edge.
    int floatingMargin = 0;
    QString visibilityMode = QStringLiteral("always");
    // Whether windows keep clear of it (maximised ones stop at it).
    bool reservesSpace = true;
    bool visible = true;

    [[nodiscard]] bool vertical() const
    {
        return edge == QStringLiteral("left") || edge == QStringLiteral("right");
    }
    bool operator==(const BarRecord &) const = default;
};

// Reads the backend's list of bars, a JSON array of objects with the fields
// above; "widgets" is a list of {"key", "plugin"} objects, the dock's key
// being "dock". A record without an id, with an id already listed, or with
// no screen edge is left out; an unknown or missing value takes the default
// above, numbers are kept in bounds, a missing order is the record's place in
// the list, and the widgets are kept as normalizedBarWidgets keeps them. On a
// document that is no list, returns nothing and says why in `error`.
QList<BarRecord> parseBarRecords(const QByteArray &json, QString *error = nullptr);
}
