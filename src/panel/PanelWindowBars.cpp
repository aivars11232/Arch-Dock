// PanelWindow: Arch Dock's own bars. An edge panel whose host is "bar" is
// shown by Arch Dock's bar host (arch-dock-bars) instead of Plasma, so that
// bars can stand side by side on one screen edge. Its record is kept, edited
// and checked as any edge panel's; nothing of it is in Plasma. The bar host
// reads the bars from barHostRecordsJson, is told when they change, and is
// kept running while there are bars.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

#include <KPackage/PackageLoader>
#include <KPluginMetaData>

#include <QCollator>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

using PanelWindowHelpers::isNativeDockPanelEdge;
using PanelWindowHelpers::isNativeDockPanelType;

namespace
{
QString edgeName(ArchDock::NativePanelEdge edge)
{
    switch (edge)
    {
    case ArchDock::NativePanelEdge::Top:
        return QStringLiteral("top");
    case ArchDock::NativePanelEdge::Left:
        return QStringLiteral("left");
    case ArchDock::NativePanelEdge::Right:
        return QStringLiteral("right");
    case ArchDock::NativePanelEdge::Bottom:
        break;
    }
    return QStringLiteral("bottom");
}

QString alignmentName(ArchDock::NativePanelAlignment alignment)
{
    switch (alignment)
    {
    case ArchDock::NativePanelAlignment::Start:
        return QStringLiteral("start");
    case ArchDock::NativePanelAlignment::End:
        return QStringLiteral("end");
    case ArchDock::NativePanelAlignment::Center:
        break;
    }
    return QStringLiteral("center");
}

QString lengthModeName(ArchDock::NativePanelLengthMode mode)
{
    switch (mode)
    {
    case ArchDock::NativePanelLengthMode::Fit:
        return QStringLiteral("fit");
    case ArchDock::NativePanelLengthMode::Fill:
        return QStringLiteral("fill");
    case ArchDock::NativePanelLengthMode::Fixed:
        break;
    }
    return QStringLiteral("fixed");
}

QString visibilityModeName(ArchDock::NativePanelVisibilityMode mode)
{
    switch (mode)
    {
    case ArchDock::NativePanelVisibilityMode::AutoHide:
        return QStringLiteral("auto-hide");
    case ArchDock::NativePanelVisibilityMode::Dodge:
        return QStringLiteral("dodge");
    case ArchDock::NativePanelVisibilityMode::Cover:
        return QStringLiteral("cover");
    case ArchDock::NativePanelVisibilityMode::Always:
        break;
    }
    return QStringLiteral("always");
}
}

bool PanelWindow::barHosted(const QString &panelId) const
{
    return m_panelRegistry.panelValue(panelId, QStringLiteral("edgeHost")).toString() == QLatin1String("bar")
        && isNativeDockPanelEdge(m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString());
}

QString PanelWindow::createBar(const QString &edge, const QString &type)
{
    if (profileBusy()) return {};

    const QString normalizedEdge = edge.trimmed().toLower();
    const QString normalizedType = type.trimmed().toLower();
    if (!isNativeDockPanelEdge(normalizedEdge) || !isNativeDockPanelType(normalizedType))
    {
        return {};
    }
    const QString panelId = m_panelRegistry.addPanel(normalizedEdge, normalizedType, QStringLiteral("bar"));
    if (panelId.isEmpty())
    {
        return {};
    }
    QString defaultError;
    if (!applyPresetDefaultsToNewPanel(panelId, &defaultError))
        qWarning() << "Could not apply preset defaults to the new bar" << panelId << defaultError;
    scheduleBarRecordsUpdate();
    return panelId;
}

QString PanelWindow::barHostRecordsJson() const
{
    QStringList panelIds = m_panelRegistry.panelIds();
    // A new bar a preset is tried on is shown before it is kept.
    if (m_presetAudition && m_presetAudition->active())
    {
        const QString trial = m_presetAudition->status().value(QStringLiteral("panelId")).toString();
        if (!trial.isEmpty() && !panelIds.contains(trial))
            panelIds.append(trial);
    }
    QJsonArray bars;
    int order = 0;
    for (const QString &panelId : std::as_const(panelIds))
    {
        // As drawn: with Panel Studio's draft or a preview, as the dock in
        // the bar is.
        const auto definition = runtimePanelDefinition(panelId);
        if (!definition || definition->host.kind != ArchDock::PanelHostKind::NativeEdge
            || definition->host.edgeHost != QLatin1String("bar"))
            continue;
        const ArchDock::NativePanelPlacementResult normalized =
            normalizedNativePanelPlacement(*definition, {});
        if (!normalized.placement)
        {
            qWarning() << "Bar" << panelId << "has no placement that can be shown; it is left out.";
            continue;
        }
        const ArchDock::NativePanelPlacement &placement = *normalized.placement;
        // Windows keep clear of a bar that is always shown, unless the owner
        // said otherwise; one that hides keeps no room.
        const bool reserves = placement.reservedSpace == ArchDock::NativePanelReservedSpace::Reserve
            || (placement.reservedSpace == ArchDock::NativePanelReservedSpace::Automatic
                && placement.visibilityMode == ArchDock::NativePanelVisibilityMode::Always);
        QJsonArray widgets;
        const auto held = ArchDock::Bars::barWidgetsFromEntries(
            definition->content.barWidgets, definition->content.type != QLatin1String("empty"));
        for (const ArchDock::Bars::BarWidget &widget : held)
        {
            if (widget.dock())
            {
                widgets.append(QJsonObject{{QStringLiteral("key"), widget.key}});
                continue;
            }
            QJsonObject listed{{QStringLiteral("key"), widget.key}, {QStringLiteral("plugin"), widget.plugin}};
            const QVariantMap settings = definition->content.barWidgetSettings.value(widget.key).toMap();
            if (!settings.isEmpty())
                listed.insert(QStringLiteral("settings"), QJsonObject::fromVariantMap(settings));
            widgets.append(listed);
        }
        bars.append(QJsonObject{
            {QStringLiteral("id"), panelId},
            {QStringLiteral("type"), definition->content.type},
            {QStringLiteral("widgets"), widgets},
            {QStringLiteral("screen"), definition->host.screenIndex},
            {QStringLiteral("screenId"), definition->host.screenId},
            {QStringLiteral("edge"), edgeName(placement.edge)},
            {QStringLiteral("alignment"), alignmentName(placement.alignment)},
            {QStringLiteral("order"), order++},
            {QStringLiteral("offset"), definition->placement.offset.value_or(0)},
            {QStringLiteral("thickness"), placement.thickness},
            {QStringLiteral("lengthMode"), lengthModeName(placement.lengthMode)},
            {QStringLiteral("fixedLength"), placement.fixedLength},
            {QStringLiteral("minimumLength"), placement.minimumLength},
            {QStringLiteral("maximumLength"), placement.maximumLength},
            {QStringLiteral("floatingMargin"), placement.floatingMargin},
            {QStringLiteral("visibilityMode"), visibilityModeName(placement.visibilityMode)},
            {QStringLiteral("reservesSpace"), reserves},
            {QStringLiteral("visible"), definition->visibility.visible},
        });
    }
    return QString::fromUtf8(QJsonDocument(bars).toJson(QJsonDocument::Compact));
}

bool PanelWindow::showEdgePanelAsBar(const QString &panelId)
{
    if (profileBusy() || !m_panelRegistry.panelIds().contains(panelId)
        || !isNativeDockPanelEdge(m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString()))
    {
        return false;
    }
    if (!barHosted(panelId) && nativePanelId(panelId) < 0 && nativeOwnershipToken(panelId).trimmed().isEmpty()
        && !m_panelRegistry.panelValue(panelId, QStringLiteral("visible")).toBool())
    {
        if (!m_panelRegistry.updatePanelChecked(panelId, {{QStringLiteral("edgeHost"), QStringLiteral("bar")}}))
            return false;
    }
    return setPanelVisible(panelId, true);
}

QList<ArchDock::Bars::BarWidget> PanelWindow::barWidgetsOf(const QString &panelId) const
{
    return ArchDock::Bars::barWidgetsFromEntries(
        m_panelRegistry.panelValue(panelId, QStringLiteral("barWidgets")).toStringList(),
        m_panelRegistry.panelValue(panelId, QStringLiteral("type")).toString() != QLatin1String("empty"));
}

bool PanelWindow::setBarWidgets(const QString &panelId, const QList<ArchDock::Bars::BarWidget> &widgets)
{
    return m_panelRegistry.updatePanelChecked(
        panelId, {{QStringLiteral("barWidgets"), ArchDock::Bars::barWidgetEntries(widgets)}});
}

const QVariantMap &PanelWindow::installedBarWidgets(bool reread) const
{
    if (m_installedBarWidgetsRead && !reread)
        return m_installedBarWidgets;
    m_installedBarWidgetsRead = true;
    m_installedBarWidgets.clear();
    // Plasma's widgets are packages of QML or compiled plugins; Plasma's own
    // widget explorer lists both.
    QList<KPluginMetaData> found = KPackage::PackageLoader::self()->listPackages(QStringLiteral("Plasma/Applet"));
    found += KPluginMetaData::findPlugins(QStringLiteral("plasma/applets"));
    for (const KPluginMetaData &metadata : std::as_const(found))
    {
        const QString plugin = metadata.pluginId();
        // Containments (but the system tray, a widget holding its own),
        // Plasma's private parts, Arch Dock's own and hidden widgets are no
        // widgets to add; nor is one only for the desktop.
        const QString containment = metadata.value(QStringLiteral("X-Plasma-ContainmentType"));
        const QStringList formFactors = metadata.value(QStringLiteral("X-Plasma-FormFactors"), QStringList{});
        const bool panelWidget = formFactors.isEmpty() || formFactors.contains(QLatin1String("panel"))
            || formFactors.contains(QLatin1String("horizontal")) || formFactors.contains(QLatin1String("vertical"));
        if (!ArchDock::Bars::validWidgetPlugin(plugin) || plugin.startsWith(QLatin1String("org.archdock."))
            || plugin.contains(QLatin1String(".private."))
            || (!containment.isEmpty() && containment != QLatin1String("CustomEmbedded"))
            || metadata.isHidden() || metadata.value(QStringLiteral("NoDisplay"), false) || !panelWidget
            || m_installedBarWidgets.contains(plugin))
            continue;
        m_installedBarWidgets.insert(plugin, QVariantMap{
            {QStringLiteral("plugin"), plugin},
            {QStringLiteral("name"), metadata.name().isEmpty() ? plugin : metadata.name()},
            {QStringLiteral("icon"), metadata.iconName().isEmpty() ? QStringLiteral("plasma") : metadata.iconName()},
            {QStringLiteral("description"), metadata.description()},
            {QStringLiteral("category"), metadata.category()},
        });
    }
    return m_installedBarWidgets;
}

QVariantList PanelWindow::barWidgetList(const QString &panelId) const
{
    if (!barHosted(panelId))
        return {};
    const QVariantMap &installed = installedBarWidgets();
    QVariantList list;
    for (const ArchDock::Bars::BarWidget &widget : barWidgetsOf(panelId))
    {
        if (widget.dock())
        {
            list.append(QVariantMap{{QStringLiteral("key"), widget.key},
                                    {QStringLiteral("plugin"), QStringLiteral("org.archdock.dock")},
                                    {QStringLiteral("name"), tr("Arch Dock icons")},
                                    {QStringLiteral("icon"), QStringLiteral("org.archdock.ArchDock")}});
            continue;
        }
        QVariantMap entry = installed.value(widget.plugin).toMap();
        if (entry.isEmpty())
            entry = {{QStringLiteral("name"), widget.plugin}, {QStringLiteral("icon"), QStringLiteral("plasma")}};
        entry.insert(QStringLiteral("key"), widget.key);
        entry.insert(QStringLiteral("plugin"), widget.plugin);
        list.append(entry);
    }
    return list;
}

QVariantList PanelWindow::availableBarWidgets() const
{
    QVariantList list;
    const QVariantMap &installed = installedBarWidgets(true);
    for (auto it = installed.cbegin(); it != installed.cend(); ++it)
        list.append(it.value());
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(list.begin(), list.end(), [&collator](const QVariant &left, const QVariant &right)
    {
        return collator.compare(left.toMap().value(QStringLiteral("name")).toString(),
                                right.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    return list;
}

QString PanelWindow::addBarWidget(const QString &panelId, const QString &plugin)
{
    if (profileBusy() || !barHosted(panelId) || !installedBarWidgets().contains(plugin))
        return {};
    QList<ArchDock::Bars::BarWidget> widgets = barWidgetsOf(panelId);
    const QString key = ArchDock::Bars::newWidgetKey(widgets);
    widgets.append({key, plugin});
    return setBarWidgets(panelId, widgets) ? key : QString();
}

bool PanelWindow::removeBarWidget(const QString &panelId, const QString &key)
{
    if (profileBusy() || !barHosted(panelId) || key == ArchDock::Bars::BarWidget::dockKey())
        return false;
    QList<ArchDock::Bars::BarWidget> widgets = barWidgetsOf(panelId);
    const qsizetype removed = widgets.removeIf([&key](const ArchDock::Bars::BarWidget &widget)
                                               { return widget.key == key; });
    return removed > 0 && setBarWidgets(panelId, widgets);
}

bool PanelWindow::moveBarWidget(const QString &panelId, const QString &key, int index)
{
    if (profileBusy() || !barHosted(panelId))
        return false;
    QList<ArchDock::Bars::BarWidget> widgets = barWidgetsOf(panelId);
    const auto found = std::find_if(widgets.cbegin(), widgets.cend(),
                                    [&key](const ArchDock::Bars::BarWidget &widget) { return widget.key == key; });
    if (found == widgets.cend())
        return false;
    const qsizetype from = std::distance(widgets.cbegin(), found);
    const qsizetype to = std::clamp<qsizetype>(index, 0, widgets.size() - 1);
    if (from == to)
        return true;
    widgets.move(from, to);
    return setBarWidgets(panelId, widgets);
}

bool PanelWindow::barWidgetRemoved(const QString &panelId, const QString &key)
{
    return removeBarWidget(panelId, key);
}

bool PanelWindow::moveEdgePanelIntoBar(const QString &panelId)
{
    if (profileBusy() || !m_panelRegistry.panelIds().contains(panelId) || barHosted(panelId)
        || !isNativeDockPanelEdge(m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString()))
        return false;
    const int containmentId = nativePanelId(panelId);
    if (containmentId < 0 || !nativePanelIsOwned(panelId, containmentId))
        return false;

    // What the Plasma panel holds, in its order, each widget with its
    // settings as Plasma keeps them: by group under the widget's own settings
    // ("" for the widget's own, "General", ...).
    const QString script = QStringLiteral(R"(
const read = function(widget, path) {
    widget.currentConfigGroup = path;
    const result = {};
    const values = {};
    for (const key of widget.configKeys) values[key] = widget.readConfig(key, "");
    result[path.join("/")] = values;
    for (const name of widget.configGroups) Object.assign(result, read(widget, path.concat([name])));
    widget.currentConfigGroup = path;
    return result;
};
const panel = panelById(%1);
if (!panel) {
    print("ARCHDOCK_JSON:null");
} else {
    panel.currentConfigGroup = ["General"];
    const order = String(panel.readConfig("AppletOrder", "")).split(";").filter(function(id) { return id.length > 0; });
    const widgets = panel.widgets().map(function(widget) {
        return { id: String(widget.id), plugin: String(widget.type), settings: read(widget, []) };
    });
    print("ARCHDOCK_JSON:" + JSON.stringify({ order: order, widgets: widgets }));
})").arg(containmentId);
    QDBusInterface shell(QStringLiteral("org.kde.plasmashell"), QStringLiteral("/PlasmaShell"),
                         QStringLiteral("org.kde.PlasmaShell"), QDBusConnection::sessionBus());
    if (!shell.isValid())
        return false;
    const QDBusReply<QString> reply = PanelWindowHelpers::callPlasmaScript(shell, script, false);
    const qsizetype at = reply.isValid() ? reply.value().lastIndexOf(QStringLiteral("ARCHDOCK_JSON:")) : -1;
    if (at < 0)
    {
        qWarning() << "The widgets of the Plasma panel of" << panelId << "could not be read";
        return false;
    }
    const QJsonObject held = QJsonDocument::fromJson(
        reply.value().mid(at + QStringLiteral("ARCHDOCK_JSON:").size()).trimmed().toUtf8()).object();
    if (held.isEmpty())
        return false;

    // The panel's order first, then any widget its order does not name.
    QHash<QString, QJsonObject> byId;
    QStringList ids;
    for (const QJsonValue &value : held.value(QStringLiteral("widgets")).toArray())
        byId.insert(value.toObject().value(QStringLiteral("id")).toString(), value.toObject());
    for (const QJsonValue &value : held.value(QStringLiteral("order")).toArray())
        if (byId.contains(value.toString()) && !ids.contains(value.toString()))
            ids.append(value.toString());
    for (const QJsonValue &value : held.value(QStringLiteral("widgets")).toArray())
        if (const QString id = value.toObject().value(QStringLiteral("id")).toString(); !ids.contains(id))
            ids.append(id);

    // Arch Dock's dock stays the dock; its other parts in the panel (an old
    // control widget) do not come along.
    const QString dockApplet = QString::number(nativeDockAppletId(panelId));
    QList<ArchDock::Bars::BarWidget> widgets;
    QVariantMap settings;
    for (const QString &id : std::as_const(ids))
    {
        const QJsonObject widget = byId.value(id);
        const QString plugin = widget.value(QStringLiteral("plugin")).toString();
        if (id == dockApplet || plugin == QLatin1String("org.archdock.dock"))
        {
            if (std::none_of(widgets.cbegin(), widgets.cend(),
                             [](const ArchDock::Bars::BarWidget &held) { return held.dock(); }))
                widgets.append({ArchDock::Bars::BarWidget::dockKey(), {}});
            continue;
        }
        if (plugin.startsWith(QLatin1String("org.archdock.")) || !ArchDock::Bars::validWidgetPlugin(plugin))
            continue;
        const QString key = ArchDock::Bars::newWidgetKey(widgets);
        widgets.append({key, plugin});
        // The system tray keeps its choices in a containment of its own,
        // which the bar's tray makes anew; what Plasma reads back for it is
        // that containment's bookkeeping, not its settings.
        const QVariantMap kept = plugin == QLatin1String("org.kde.plasma.systemtray")
            ? QVariantMap{} : widget.value(QStringLiteral("settings")).toObject().toVariantMap();
        if (!kept.isEmpty())
            settings.insert(key, kept);
    }
    const bool holdsDock = m_panelRegistry.panelValue(panelId, QStringLiteral("type")).toString() != QLatin1String("empty");
    const QStringList entries = ArchDock::Bars::barWidgetEntries(ArchDock::Bars::normalizedBarWidgets(widgets, holdsDock));

    // The Plasma panel goes first: a bar has none to remove.
    if (!removeNativeKdePanel(panelId))
        return false;
    if (!m_panelRegistry.updatePanelChecked(panelId, {{QStringLiteral("edgeHost"), QStringLiteral("bar")},
                                                      {QStringLiteral("barWidgets"), entries},
                                                      {QStringLiteral("barWidgetSettings"), settings},
                                                      {QStringLiteral("kdeWidgets"), QStringList{}}}))
    {
        qWarning() << "The Plasma panel of" << panelId << "was removed but the bar could not be made";
        return false;
    }
    return setPanelVisible(panelId, true);
}

QString PanelWindow::barHostWindowsJson() const
{
    QJsonArray windows;
    for (const WindowItem &window : m_windowModel.windows() + m_windowWatcher.untrackedWindows())
    {
        if (window.minimized || !window.onCurrentDesktop || !window.frameGeometry.isValid())
            continue;
        windows.append(QJsonObject{
            {QStringLiteral("x"), window.frameGeometry.x()},
            {QStringLiteral("y"), window.frameGeometry.y()},
            {QStringLiteral("width"), window.frameGeometry.width()},
            {QStringLiteral("height"), window.frameGeometry.height()},
            {QStringLiteral("active"), window.active},
            {QStringLiteral("maximized"), window.maximized},
            {QStringLiteral("fullScreen"), window.fullScreen},
        });
    }
    return QString::fromUtf8(QJsonDocument(windows).toJson(QJsonDocument::Compact));
}

void PanelWindow::scheduleBarWindowsUpdate()
{
    if (m_barRecordsJson.isEmpty() || m_barRecordsJson == QLatin1String("[]"))
        return;
    if (!m_barWindowsTimer.isActive())
        m_barWindowsTimer.start();
}

void PanelWindow::scheduleBarRecordsUpdate()
{
    if (!m_barRecordsTimer.isActive())
        m_barRecordsTimer.start();
}

void PanelWindow::publishBarRecords()
{
    const QString records = barHostRecordsJson();
    m_barHost.setWanted(records != QLatin1String("[]"));
    if (records == m_barRecordsJson)
        return;
    m_barRecordsJson = records;
    m_barHost.barsChanged();
    emit barHostRecordsChanged();
}

bool PanelWindow::persistBarVisibility(const QString &panelId,
                                       std::optional<ArchDock::PanelVisibilityMode> requestedMode,
                                       const QVariantMap &persistValues)
{
    const bool saved = persistValues.isEmpty() || m_panelRegistry.updatePanelChecked(panelId, persistValues);
    const QString mode = ArchDock::panelVisibilityModeToString(requestedMode.value_or(
        ArchDock::panelVisibilityModeFromString(
            m_panelRegistry.panelValue(panelId, QStringLiteral("visibilityMode")).toString())));
    recordNativePanelVisibilityResult(panelId, {
        {QStringLiteral("success"), saved},
        {QStringLiteral("verified"), saved},
        {QStringLiteral("status"), saved ? QStringLiteral("applied") : QStringLiteral("failed")},
        {QStringLiteral("errorCode"), saved ? QString{} : QStringLiteral("persistence-failed")},
        {QStringLiteral("requestedMode"), mode},
        {QStringLiteral("effectiveMode"), mode},
        {QStringLiteral("hostMode"), mode},
        {QStringLiteral("supportedModes"),
         ArchDock::supportedNativeVisibilityModes(nativeVisibilityCapabilities(panelId))},
        {QStringLiteral("watcherAvailable"), m_windowWatcher.available()},
        {QStringLiteral("fallbackAttempted"), false},
        {QStringLiteral("fallbackApplied"), false},
        {QStringLiteral("fallbackReason"), QString{}},
        {QStringLiteral("fallbackErrorCode"), QString{}},
        {QStringLiteral("ownershipVerified"), true},
        {QStringLiteral("rollbackAttempted"), false},
        {QStringLiteral("rollbackSucceeded"), false},
        {QStringLiteral("rollbackErrorCode"), QString{}},
    });
    scheduleBarRecordsUpdate();
    return saved;
}
