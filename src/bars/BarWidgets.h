// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace ArchDock::Bars
{
// One thing an Arch Dock bar holds, in its place along the bar: Arch Dock's
// dock, or a Plasma widget by its plugin id with a key of its own, under which
// the bar host keeps the widget's settings.
struct BarWidget
{
    QString key;
    QString plugin;
    // Settings a widget moved from a Plasma panel brings with it, by group
    // ("General", "" for the widget's own) and name, as Plasma keeps them;
    // the bar host gives them to the widget it makes.
    QVariantMap settings = {};

    [[nodiscard]] bool dock() const { return key == dockKey(); }
    static QString dockKey() { return QStringLiteral("dock"); }
    bool operator==(const BarWidget &) const = default;
};

// A widget's key: letters, digits, "-" and "_", at most 64 of them, and
// never the dock's.
bool validWidgetKey(const QString &key);
// A Plasma widget's plugin id: letters, digits, ".", "-" and "_", at most 128
// of them, and never Arch Dock's own dock or bar.
bool validWidgetPlugin(const QString &plugin);

// The list as a bar keeps it: widgets with a bad key or plugin id, and later
// ones with a key already used, are left out; the dock is there once, first
// when it was missing. Every bar holds its dock: an Empty bar's shows the
// bar's own items.
QList<BarWidget> normalizedBarWidgets(const QList<BarWidget> &widgets);

// The list read from a panel record's entries, "dock" and "<key>:<plugin
// id>", and written back the same way.
QList<BarWidget> barWidgetsFromEntries(const QStringList &entries);
QStringList barWidgetEntries(const QList<BarWidget> &widgets);

// A key no widget in the list has yet.
QString newWidgetKey(const QList<BarWidget> &widgets);
}
