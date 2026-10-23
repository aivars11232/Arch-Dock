// SPDX-License-Identifier: GPL-3.0-or-later
#include "BarCorona.h"

#include <KConfigGroup>
#include <KConfigLoader>
#include <Plasma/Applet>
#include <Plasma/Containment>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

namespace ArchDock::Bars
{
namespace
{
// Where a containment keeps which bar it is.
constexpr const char *BarIdKey = "archDockBar";
// Where a widget keeps the key its bar's record knows it by.
constexpr const char *WidgetKeyEntry = "archDockWidget";

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

void hideAction(QAction *action)
{
    if (action)
        action->setVisible(false);
}

// Sets one of an applet's own settings as its settings page would, so that
// its QML follows; whether it changed (and is still to be saved).
bool changeSetting(KConfigLoader *settings, const QString &name, const QString &value)
{
    KConfigSkeletonItem *item = settings ? settings->findItemByName(name) : nullptr;
    if (!item || item->property().toString() == value)
        return false;
    item->setProperty(value);
    return true;
}

// A widget moved to a bar from a Plasma panel keeps its settings: each one
// written as Plasma kept it, then read again by the widget's own settings.
void giveSettings(Plasma::Applet *applet, const QVariantMap &groups)
{
    if (groups.isEmpty())
        return;
    for (auto group = groups.cbegin(); group != groups.cend(); ++group)
    {
        KConfigGroup target = applet->config();
        for (const QString &name : group.key().split(QLatin1Char('/'), Qt::SkipEmptyParts))
            target = target.group(name);
        const QVariantMap entries = group.value().toMap();
        for (auto entry = entries.cbegin(); entry != entries.cend(); ++entry)
        {
            if (entry.value().typeId() == QMetaType::QVariantList)
                target.writeEntry(entry.key(), entry.value().toStringList());
            else
                target.writeEntry(entry.key(), entry.value().toString());
        }
    }
    if (KConfigLoader *settings = applet->configScheme())
        settings->load();
}

// The dock widget learns which panel it draws from its own settings, the
// same two the backend writes when it puts the dock in a Plasma panel.
void configureDock(Plasma::Applet *applet, const BarRecord &record)
{
    KConfigLoader *settings = applet->configScheme();
    const bool panel = changeSetting(settings, QStringLiteral("panelId"), record.id);
    const bool type = changeSetting(settings, QStringLiteral("panelType"), record.type);
    if (panel || type)
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
    trimMenus(containment);

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
    {
        configureDock(dock, record);
        trimMenus(dock);
    }
    followWidgets(containment, dock, record);
}

void BarCorona::followWidgets(Plasma::Containment *containment, Plasma::Applet *dock, const BarRecord &record)
{
    watchRemovals(containment);
    // Each widget made for the bar carries the key its record knows it by;
    // one the record no longer lists, or one with no key, goes.
    QHash<QString, Plasma::Applet *> byKey;
    const QList<Plasma::Applet *> applets = containment->applets();
    for (Plasma::Applet *applet : applets)
    {
        if (applet->destroyed() || applet->pluginName() == QLatin1String(DockPlugin))
            continue;
        const QString key = applet->config().readEntry(WidgetKeyEntry, QString());
        const bool listed = std::any_of(record.widgets.cbegin(), record.widgets.cend(),
                                        [&key, applet](const BarWidget &widget)
                                        { return widget.key == key && widget.plugin == applet->pluginName(); });
        if (listed && !byKey.contains(key))
        {
            byKey.insert(key, applet);
            rememberWidget(applet, key);
            continue;
        }
        m_removing.insert(applet);
        applet->destroy();
    }

    // In the record's order: Plasma's panels keep theirs the same way, as
    // the applet ids in the containment's AppletOrder setting.
    QStringList order;
    for (const BarWidget &widget : record.widgets)
    {
        Plasma::Applet *applet = widget.dock() ? dock : byKey.value(widget.key);
        if (!applet && !widget.dock())
        {
            applet = containment->createApplet(widget.plugin);
            if (applet)
            {
                KConfigGroup settings = applet->config();
                settings.writeEntry(WidgetKeyEntry, widget.key);
                giveSettings(applet, widget.settings);
                rememberWidget(applet, widget.key);
                requireConfigSync();
            }
            else
                qWarning() << "The Plasma widget" << widget.plugin << "could not be added to the bar" << record.id;
        }
        if (applet)
        {
            order.append(QString::number(applet->id()));
            trimMenus(applet);
        }
    }
    KConfigLoader *settings = containment->configScheme();
    if (changeSetting(settings, QStringLiteral("AppletOrder"), order.join(QLatin1Char(';'))))
        settings->save();
}

void BarCorona::dropWidget(QObject *containment, const QString &plugin, int index)
{
    const QString bar = barId(qobject_cast<Plasma::Containment *>(containment));
    if (!bar.isEmpty() && !plugin.trimmed().isEmpty())
        Q_EMIT widgetDropped(bar, plugin.trimmed(), std::max(0, index));
}

void BarCorona::moveWidget(QObject *containment, QObject *applet, int index)
{
    const QString bar = barId(qobject_cast<Plasma::Containment *>(containment));
    auto *widget = qobject_cast<Plasma::Applet *>(applet);
    if (bar.isEmpty() || !widget)
        return;
    const QString key = widget->pluginName() == QLatin1String(DockPlugin) ? QStringLiteral("dock")
                                                                          : m_widgetKeys.value(widget);
    if (!key.isEmpty())
        Q_EMIT widgetMoved(bar, key, std::max(0, index));
}

void BarCorona::removeWidget(QObject *containment, QObject *applet)
{
    const QString bar = barId(qobject_cast<Plasma::Containment *>(containment));
    auto *widget = qobject_cast<Plasma::Applet *>(applet);
    if (bar.isEmpty() || !widget || widget->pluginName() == QLatin1String(DockPlugin))
        return;
    const QString key = m_widgetKeys.value(widget);
    if (!key.isEmpty())
        Q_EMIT widgetRemoveRequested(bar, key);
}

void BarCorona::trimMenus(Plasma::Applet *applet)
{
    // A bar's widgets are its record's: what Plasma's menus offer that the
    // record would not keep stays out of them: a widget's alternatives (an
    // analog clock for the digital one, a widget the bar's record does not
    // hold, which the bar host would take out again), and Remove on the
    // dock, which goes with its bar. Edit mode stays, as on Plasma's own
    // panels, and Add Widgets opens Plasma's own widget explorer, to drag
    // widgets onto the bar. Plasma makes these actions after a widget starts
    // and may show them again, so they are hidden as each menu opens.
    const auto trim = [applet]
    {
        if (!applet->isContainment())
            hideAction(applet->internalAction(QStringLiteral("alternatives")));
        if (applet->pluginName() == QLatin1String(DockPlugin))
            hideAction(applet->internalAction(QStringLiteral("remove")));
    };
    trim();
    if (m_trimmed.contains(applet))
        return;
    m_trimmed.insert(applet);
    if (auto *bar = qobject_cast<Plasma::Containment *>(applet))
        connect(bar, &Plasma::Containment::showAddWidgetsInterface, &m_widgetWatch, []
        {
            QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
                QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                QStringLiteral("org.kde.PlasmaShell"), QStringLiteral("toggleWidgetExplorer")));
        });
    connect(applet, &Plasma::Applet::contextualActionsAboutToShow, &m_widgetWatch, trim);
    connect(applet, &QObject::destroyed, &m_widgetWatch, [this, applet] { m_trimmed.remove(applet); });
}

void BarCorona::rememberWidget(Plasma::Applet *applet, const QString &key)
{
    if (!m_widgetKeys.contains(applet))
    {
        // The containment tells of a removal from the widget's own destroyed
        // signal, connected before this one: the key is still here then.
        connect(applet, &QObject::destroyed, &m_widgetWatch, [this, applet]
        {
            m_widgetKeys.remove(applet);
            m_removing.remove(applet);
        });
    }
    m_widgetKeys.insert(applet, key);
}

void BarCorona::watchRemovals(Plasma::Containment *containment)
{
    if (m_watched.contains(containment))
        return;
    m_watched.insert(containment);
    connect(containment, &QObject::destroyed, &m_widgetWatch, [this, containment] { m_watched.remove(containment); });
    connect(containment, &Plasma::Containment::appletRemoved, &m_widgetWatch, [this, containment](Plasma::Applet *applet)
    {
        const QString key = m_widgetKeys.take(applet);
        if (m_removing.remove(applet) || key.isEmpty())
            return;
        Q_EMIT widgetRemovedByUser(barId(containment), key);
    });
}

void BarCorona::removeBar(Plasma::Containment *containment)
{
    if (!containment)
        return;
    // Its widgets go with it, by no one's own removal.
    const QList<Plasma::Applet *> applets = containment->applets();
    for (Plasma::Applet *applet : applets)
        m_removing.insert(applet);
    // Plasma deletes a containment a moment after it is destroyed, and only
    // then is it gone from the layout: the layout is written once it is.
    connect(containment, &QObject::destroyed, this, [this] { requireConfigSync(); });
    containment->destroy();
}
}
