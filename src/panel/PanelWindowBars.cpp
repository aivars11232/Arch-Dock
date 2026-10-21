// PanelWindow: Arch Dock's own bars. An edge panel whose host is "bar" is
// shown by Arch Dock's bar host (arch-dock-bars) instead of Plasma, so that
// bars can stand side by side on one screen edge. Its record is kept, edited
// and checked as any edge panel's; nothing of it is in Plasma. The bar host
// reads the bars from barHostRecordsJson, is told when they change, and is
// kept running while there are bars.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
        bars.append(QJsonObject{
            {QStringLiteral("id"), panelId},
            {QStringLiteral("type"), definition->content.type},
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
