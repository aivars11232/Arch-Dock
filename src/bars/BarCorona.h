// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BarRecord.h"

#include <Plasma/Corona>

#include <QHash>
#include <QSet>

namespace Plasma
{
class Applet;
class Containment;
}

namespace ArchDock::Bars
{
// The Plasma side of Arch Dock's bars: a corona of the bar host's own, as
// Plasma's plasmoidviewer (plasma-sdk) has one, whose containments are the
// bars (org.archdock.bar), each holding Arch Dock's dock widget unless it is
// an empty bar, and the Plasma widgets its record lists, in the record's
// order. Its layout lives in the bar host's own file, never in the owner's
// Plasma layout.
class BarCorona : public Plasma::Corona
{
    Q_OBJECT

public:
    static constexpr const char *ContainmentPlugin = "org.archdock.bar";
    static constexpr const char *DockPlugin = "org.archdock.dock";

    explicit BarCorona(QObject *parent = nullptr);

    uint numScreens() const override;
    QRect screenGeometry(uint id) const override;

    // The containment of this bar, made the first time it is asked for. Its
    // edge, its dock (whether there is one, and what it shows) and its
    // widgets follow the record each time. Nothing when the bar's containment
    // plugin is missing.
    Plasma::Containment *barContainment(const BarRecord &record);
    // Every bar this corona holds, and which bar a containment is.
    QList<Plasma::Containment *> barContainments() const;
    static QString barId(const Plasma::Containment *containment);
    // Removes a bar for good, with what it holds.
    void removeBar(Plasma::Containment *containment);
    // A Plasma widget dropped on a bar (from Plasma's widget explorer) at a
    // place along it, counted in the bar's widgets before the drop.
    Q_INVOKABLE void dropWidget(QObject *containment, const QString &plugin, int index);
    // A bar's widget dragged to another place along it in edit mode.
    Q_INVOKABLE void moveWidget(QObject *containment, QObject *applet, int index);
    // A bar's widget taken out with its remove button in edit mode.
    Q_INVOKABLE void removeWidget(QObject *containment, QObject *applet);

Q_SIGNALS:
    // A widget was removed from a bar through its own menu, not because the
    // bar's record no longer lists it.
    void widgetRemovedByUser(const QString &barId, const QString &key);
    // A widget dropped on a bar, for Arch Dock to add to the bar's record.
    void widgetDropped(const QString &barId, const QString &plugin, int index);
    // A bar's widget moved along it, for Arch Dock to move in the record.
    void widgetMoved(const QString &barId, const QString &key, int index);
    // A bar's widget to take out of the record.
    void widgetRemoveRequested(const QString &barId, const QString &key);

private:
    void followRecord(Plasma::Containment *containment, const BarRecord &record);
    void followWidgets(Plasma::Containment *containment, Plasma::Applet *dock, const BarRecord &record);
    void rememberWidget(Plasma::Applet *applet, const QString &key);
    void trimMenus(Plasma::Applet *applet);
    void watchRemovals(Plasma::Containment *containment);

    // Widgets this corona is removing itself, which are no user's removal.
    QSet<Plasma::Applet *> m_removing;
    QSet<Plasma::Containment *> m_watched;
    QSet<Plasma::Applet *> m_trimmed;
    // The key each bar widget was made with: a widget's own settings go with
    // it when it is removed.
    QHash<Plasma::Applet *, QString> m_widgetKeys;
    // What the widget watching above is connected through. Plasma deletes
    // the bars and their widgets as the corona closes, after this corona's
    // own parts are gone; being last, this goes first and takes the watching
    // with it, so closing removes nothing from any bar's record.
    QObject m_widgetWatch;
};
}
