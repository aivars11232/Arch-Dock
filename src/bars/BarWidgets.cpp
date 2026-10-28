// SPDX-License-Identifier: GPL-3.0-or-later
#include "BarWidgets.h"

#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>

namespace ArchDock::Bars
{
bool validWidgetKey(const QString &key)
{
    static const QRegularExpression grammar(QStringLiteral("\\A[A-Za-z0-9_-]{1,64}\\z"));
    return key != BarWidget::dockKey() && grammar.match(key).hasMatch();
}

bool validWidgetPlugin(const QString &plugin)
{
    static const QRegularExpression grammar(QStringLiteral("\\A[A-Za-z0-9._-]{1,128}\\z"));
    return plugin != QLatin1String("org.archdock.dock") && plugin != QLatin1String("org.archdock.bar")
        && grammar.match(plugin).hasMatch();
}

QList<BarWidget> normalizedBarWidgets(const QList<BarWidget> &widgets)
{
    QList<BarWidget> result;
    QSet<QString> keys;
    bool dock = false;
    for (const BarWidget &widget : widgets)
    {
        if (widget.dock())
        {
            if (!dock)
                result.append({BarWidget::dockKey(), {}});
            dock = true;
            continue;
        }
        if (!validWidgetKey(widget.key) || !validWidgetPlugin(widget.plugin) || keys.contains(widget.key))
            continue;
        keys.insert(widget.key);
        result.append(widget);
    }
    if (!dock)
        result.prepend({BarWidget::dockKey(), {}});
    return result;
}

QList<BarWidget> barWidgetsFromEntries(const QStringList &entries)
{
    QList<BarWidget> widgets;
    for (const QString &entry : entries)
    {
        const QString trimmed = entry.trimmed();
        if (trimmed == BarWidget::dockKey())
        {
            widgets.append({trimmed, {}});
            continue;
        }
        const qsizetype colon = trimmed.indexOf(QLatin1Char(':'));
        if (colon > 0)
            widgets.append({trimmed.left(colon), trimmed.mid(colon + 1)});
    }
    return normalizedBarWidgets(widgets);
}

QStringList barWidgetEntries(const QList<BarWidget> &widgets)
{
    QStringList entries;
    for (const BarWidget &widget : widgets)
        entries.append(widget.dock() ? widget.key : widget.key + QLatin1Char(':') + widget.plugin);
    return entries;
}

QString newWidgetKey(const QList<BarWidget> &widgets)
{
    QSet<QString> taken;
    for (const BarWidget &widget : widgets)
        taken.insert(widget.key);
    for (;;)
    {
        const QString key = QStringLiteral("w%1").arg(QRandomGenerator::global()->generate(), 8, 16, QLatin1Char('0'));
        if (!taken.contains(key))
            return key;
    }
}
}
