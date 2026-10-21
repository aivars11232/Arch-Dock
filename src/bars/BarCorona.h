// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BarRecord.h"

#include <Plasma/Corona>

namespace Plasma
{
class Containment;
}

namespace ArchDock::Bars
{
// The Plasma side of Arch Dock's bars: a corona of the bar host's own, as
// Plasma's plasmoidviewer (plasma-sdk) has one, whose containments are the
// bars (org.archdock.bar), each holding Arch Dock's dock widget unless it is
// an empty bar. Its layout lives in the bar host's own file, never in the
// owner's Plasma layout.
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
    // edge and its dock (whether there is one, and what it shows) follow the
    // record each time. Nothing when the bar's containment plugin is missing.
    Plasma::Containment *barContainment(const BarRecord &record);
    // Every bar this corona holds, and which bar a containment is.
    QList<Plasma::Containment *> barContainments() const;
    static QString barId(const Plasma::Containment *containment);
    // Removes a bar for good, with what it holds.
    void removeBar(Plasma::Containment *containment);

private:
    void followRecord(Plasma::Containment *containment, const BarRecord &record);
};
}
