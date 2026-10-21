// SPDX-License-Identifier: GPL-3.0-or-later
#include "BarCorona.h"

#include <KConfigGroup>
#include <KConfigLoader>
#include <Plasma/Applet>
#include <Plasma/Containment>

#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

namespace ArchDock::Bars
{
namespace
{
// Where a containment keeps which bar it is.
constexpr const char *BarIdKey = "archDockBar";

Plasma::Types::Location location(const QString &edge)
{
    if (edge == QStringLiteral("top"))
        return Plasma::Types::TopEdge;
    if (edge == QStringLiteral("left"))
        return Plasma::Types::LeftEdge;
    if (edge == QStringLiteral("right"))
        return Plasma::Types::RightEdge;
    return Plasma::Types::BottomEdge;
}

Plasma::Applet *dockApplet(const Plasma::Containment *containment)
{
    const QList<Plasma::Applet *> applets = containment->applets();
    for (Plasma::Applet *applet : applets)
        if (applet->pluginName() == QLatin1String(BarCorona::DockPlugin) && !applet->destroyed())
            return applet;
    return nullptr;
}

// The dock widget learns which panel it draws from its own settings, the
// same two the backend writes when it puts the dock in a Plasma panel.
void configureDock(Plasma::Applet *applet, const BarRecord &record)
{
    KConfigLoader *settings = applet->configScheme();
    if (!settings)
        return;
    bool changed = false;
    const auto set = [settings, &changed](const QString &name, const QString &value)
    {
        KConfigSkeletonItem *item = settings->findItemByName(name);
        if (item && item->property().toString() != value)
        {
            item->setProperty(value);
            changed = true;
        }
    };
    set(QStringLiteral("panelId"), record.id);
    set(QStringLiteral("panelType"), record.type);
    if (changed)
        settings->save();
}
}

BarCorona::BarCorona(QObject *parent)
    : Plasma::Corona(parent)
{
    const auto screensChanged = [this]
    {
        for (uint id = 0; id < numScreens(); ++id)
            Q_EMIT screenGeometryChanged(id);
    };
    connect(qGuiApp, &QGuiApplication::screenAdded, this, screensChanged);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, screensChanged);
}

uint BarCorona::numScreens() const
{
    return uint(QGuiApplication::screens().size());
}

QRect BarCorona::screenGeometry(uint id) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    return id < uint(screens.size()) ? screens.at(int(id))->geometry() : QRect();
}

QList<Plasma::Containment *> BarCorona::barContainments() const
{
    QList<Plasma::Containment *> bars;
    const QList<Plasma::Containment *> all = containments();
    for (Plasma::Containment *containment : all)
        if (!containment->destroyed() && !barId(containment).isEmpty())
            bars.append(containment);
    return bars;
}

QString BarCorona::barId(const Plasma::Containment *containment)
{
    return containment ? containment->config().readEntry(BarIdKey, QString()) : QString();
}

Plasma::Containment *BarCorona::barContainment(const BarRecord &record)
{
    Plasma::Containment *found = nullptr;
    const QList<Plasma::Containment *> bars = barContainments();
    for (Plasma::Containment *containment : bars)
        if (barId(containment) == record.id)
            found = containment;

    const bool made = !found;
    if (made)
    {
        found = createContainment(QLatin1String(ContainmentPlugin));
        if (!found || found->pluginName() != QLatin1String(ContainmentPlugin))
        {
            qWarning() << "The Arch Dock bar containment" << ContainmentPlugin << "could not be loaded.";
            if (found)
                found->destroy();
            return nullptr;
        }
        KConfigGroup settings = found->config();
        settings.writeEntry(BarIdKey, record.id);
    }
    followRecord(found, record);
    // A new bar is written down at once; later changes are saved by Plasma
    // as they come.
    if (made)
        requireConfigSync();
    return found;
}

void BarCorona::followRecord(Plasma::Containment *containment, const BarRecord &record)
{
    containment->setFormFactor(record.vertical() ? Plasma::Types::Vertical : Plasma::Types::Horizontal);
    containment->setLocation(location(record.edge));

    Plasma::Applet *dock = dockApplet(containment);
    const bool wantsDock = record.type != QStringLiteral("empty");
    if (wantsDock && !dock)
        dock = containment->createApplet(QLatin1String(DockPlugin));
    else if (!wantsDock && dock)
    {
        dock->destroy();
        dock = nullptr;
    }
    if (dock)
        configureDock(dock, record);
}

void BarCorona::removeBar(Plasma::Containment *containment)
{
    if (!containment)
        return;
    // Plasma deletes a containment a moment after it is destroyed, and only
    // then is it gone from the layout: the layout is written once it is.
    connect(containment, &QObject::destroyed, this, [this] { requireConfigSync(); });
    containment->destroy();
}
}
