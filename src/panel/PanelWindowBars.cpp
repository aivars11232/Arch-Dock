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
            widgets.append(widget.dock()
                ? QJsonObject{{QStringLiteral("key"), widget.key}}
                : QJsonObject{{QStringLiteral("key"), widget.key}, {QStringLiteral("plugin"), widget.plugin}});
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
