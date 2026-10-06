// PanelWindow: native Plasma panels. Arch Dock's edge panels are real Plasma
// panels, carrying its dock applet unless they are empty; this runs the Plasma
// scripts that create, find, adopt and remove them. A panel is changed or
// removed only after its ownership token proves Arch Dock made it.

#include "PanelWindow.h"
#include "PanelWindowHelpers.h"

#include "../NativeContainmentLifecycle.h"
#include "../PanelPlacement.h"
#include "../PlasmaScriptResult.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QSettings>
#include <QUuid>

using PanelWindowHelpers::plasmaScriptStringLiteral;
using PanelWindowHelpers::isNativeDockPanelType;
using PanelWindowHelpers::isNativeDockPanelEdge;
using PanelWindowHelpers::panelTypeNeedsDockApplet;

QString PanelWindow::createNativePanel(const QString &edge, const QString &type)
{
    if (profileBusy()) return {};

    const QString normalizedEdge = edge.trimmed().toLower();
    const QString normalizedType = type.trimmed().toLower();
    if (!isNativeDockPanelEdge(normalizedEdge) || !isNativeDockPanelType(normalizedType))
    {
        return {};
    }

    const QString panelId = m_panelRegistry.addPanel(normalizedEdge, normalizedType);
    if (panelId.isEmpty())
    {
        return {};
    }

    if (createNativeKdePanel(panelId))
    {
        QString defaultError;
        if (!applyPresetDefaultsToNewPanel(panelId, &defaultError))
            qWarning() << "Could not apply preset defaults to the new native panel" << panelId << defaultError;
        return panelId;
    }

    m_panelRegistry.removePanel(panelId);
    return {};
}

bool PanelWindow::setNativePanelType(const QString &panelId, const QString &type)
{
    if (profileBusy()) return false;

    const QString normalizedType = type.trimmed().toLower();
    if (!m_panelRegistry.panelIds().contains(panelId) ||
        !isNativeDockPanelEdge(m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString()) ||
        !isNativeDockPanelType(normalizedType))
    {
        return false;
    }

    m_panelRegistry.setPanelValue(panelId, QStringLiteral("type"), normalizedType);
    return createNativeKdePanel(panelId);
}

QStringList PanelWindow::availableKdeWidgets() const
{
    QSet<QString> ids;
    const QStringList roots = QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("plasma/plasmoids"),
        QStandardPaths::LocateDirectory);
    for (const QString &root : roots)
    {
        const QDir directory(root);
        const QFileInfoList entries = directory.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &entry : entries)
        {
            const QString jsonPath = entry.filePath() + QStringLiteral("/metadata.json");
            const QString desktopPath = entry.filePath() + QStringLiteral("/metadata.desktop");
            QString pluginId;
            QFile jsonFile(jsonPath);
            if (jsonFile.open(QIODevice::ReadOnly))
            {
                const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll());
                pluginId = document.object().value(QStringLiteral("KPlugin")).toObject()
                    .value(QStringLiteral("Id")).toString();
                if (pluginId.isEmpty())
                {
                    pluginId = document.object().value(QStringLiteral("Id")).toString();
                }
            }
            if (pluginId.isEmpty() && QFileInfo::exists(desktopPath))
            {
                QSettings metadata(desktopPath, QSettings::IniFormat);
                pluginId = metadata.value(QStringLiteral("Desktop Entry/X-KDE-PluginInfo-Name")).toString();
            }
            if (!pluginId.isEmpty())
            {
                ids.insert(pluginId);
            }
        }
    }

    QStringList result = ids.values();
    result.sort(Qt::CaseInsensitive);
    return result;
}

int PanelWindow::nativePanelId(const QString &panelId) const
{
    const QVariant value = m_panelRegistry.panelValue(panelId, QStringLiteral("nativePanelId"));
    return value.isValid() ? value.toInt() : -1;
}

int PanelWindow::nativeControlAppletId(const QString &panelId) const
{
    const QVariant value = m_panelRegistry.panelValue(
        panelId,
        QStringLiteral("nativeControlAppletId"));
    return value.isValid() ? value.toInt() : -1;
}

int PanelWindow::nativeDockAppletId(const QString &panelId) const
{
    const QVariant value = m_panelRegistry.panelValue(
        panelId,
        QStringLiteral("nativeDockAppletId"));
    return value.isValid() ? value.toInt() : -1;
}

QString PanelWindow::nativeOwnershipToken(const QString &panelId) const
{
    return m_panelRegistry.panelValue(panelId, QStringLiteral("nativeOwnershipToken")).toString();
}

int PanelWindow::nativePanelOffset(const QString &panelId) const
{
    return ArchDock::edgeOffset(edgePanels(), panelId);
}

int PanelWindow::evaluatePlasmaScript(const QString &script) const
{
    QDBusInterface shell(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"),
        QDBusConnection::sessionBus());
    if (!shell.isValid())
    {
        return -1;
    }

    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
    if (!reply.isValid())
    {
        qWarning() << "Plasma panel script failed:" << reply.error().message();
        return -1;
    }

    const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("(-?\\d+)"))
        .match(reply.value());
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

std::optional<int> PanelWindow::evaluatePlasmaScriptResultOptional(const QString &script) const
{
    QDBusInterface shell(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"),
        QDBusConnection::sessionBus());
    if (!shell.isValid())
    {
        return std::nullopt;
    }

    const QDBusReply<QString> reply = shell.call(QStringLiteral("evaluateScript"), script);
    if (!reply.isValid())
    {
        qWarning() << "Plasma panel script failed:" << reply.error().message();
        return std::nullopt;
    }

    const std::optional<int> result = ArchDock::parsePlasmaScriptResult(reply.value());
    if (!result.has_value())
    {
        qWarning() << "Plasma panel script returned an unverified result:"
                   << reply.value().trimmed();
        return std::nullopt;
    }
    return result;
}

int PanelWindow::evaluatePlasmaScriptResult(const QString &script) const
{
    return evaluatePlasmaScriptResultOptional(script).value_or(-1);
}

std::optional<bool> PanelWindow::nativePanelExistence(int panelId) const
{
    if (panelId < 0)
    {
        return false;
    }

    const std::optional<int> result = evaluatePlasmaScriptResultOptional(
        QStringLiteral("print('ARCHDOCK_RESULT:' + String(panelById(%1) ? 1 : 0));")
            .arg(panelId));
    if (!result.has_value() || (*result != 0 && *result != 1))
    {
        return std::nullopt;
    }
    return *result == 1;
}

bool PanelWindow::nativePanelIsOwned(const QString &panelId, int containmentId) const
{
    return nativePanelIsOwned(panelId, containmentId, nativeOwnershipToken(panelId));
}

bool PanelWindow::nativePanelIsOwned(const QString &panelId,
                                     int containmentId,
                                     const QString &ownershipToken) const
{
    if (containmentId < 0 || ownershipToken.isEmpty())
    {
        return false;
    }

    return evaluatePlasmaScriptResult(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print('ARCHDOCK_RESULT:0'); }"
            "else {"
            "panel.currentConfigGroup = ['ArchDock'];"
            "var owned = panel.readConfig('ownerToken', '') === %2 && "
            "panel.readConfig('panelId', '') === %3;"
            "print('ARCHDOCK_RESULT:' + String(owned ? 1 : 0));"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(ownershipToken))
            .arg(plasmaScriptStringLiteral(panelId))) == 1;
}

PanelWindow::NativePanelDiscoveryResult PanelWindow::discoverNativePanel(
    const QString &panelId,
    const QString &ownershipToken,
    const QString &panelType) const
{
    if (panelId.isEmpty() || ownershipToken.isEmpty() || !isNativeDockPanelType(panelType))
    {
        return {};
    }

    const std::optional<int> hostQuery = evaluatePlasmaScriptResultOptional(
        QStringLiteral(
            "var result = (function() {"
            "try {"
            "var candidates = panels();"
            "var matches = 0;"
            "var matchedId = -1;"
            "for (var index = 0; index < candidates.length; ++index) {"
            "var candidate = candidates[index];"
            "candidate.currentConfigGroup = ['ArchDock'];"
            "if (String(candidate.readConfig('ownerToken', '')) === %1 && "
            "String(candidate.readConfig('panelId', '')) === %2) {"
            "++matches; matchedId = candidate.id;"
            "}"
            "}"
            "return matches === 0 ? -1 : (matches === 1 ? matchedId : -2);"
            "} catch (error) { return -3; }"
            "})();"
            "print('ARCHDOCK_RESULT:' + String(result));")
            .arg(plasmaScriptStringLiteral(ownershipToken))
            .arg(plasmaScriptStringLiteral(panelId)));
    const ArchDock::NativeContainmentMatch hostMatch =
        ArchDock::classifyNativeContainmentMatch(hostQuery);
    switch (hostMatch.status)
    {
    case ArchDock::NativeContainmentMatchStatus::QueryFailed:
        return {};
    case ArchDock::NativeContainmentMatchStatus::Missing:
        return {NativePanelDiscoveryStatus::Missing, -1, -1};
    case ArchDock::NativeContainmentMatchStatus::Conflict:
        return {NativePanelDiscoveryStatus::HostConflict, -1, -1};
    case ArchDock::NativeContainmentMatchStatus::Unique:
        break;
    }

    const QString rendererQuery = panelTypeNeedsDockApplet(panelType)
        ? QStringLiteral(
              "var result = (function() {"
              "try {"
              "var panel = panelById(%1);"
              "if (!panel) { return -3; }"
              "var docks = panel.widgets('org.archdock.dock');"
              "if (docks.length === 0) { return -1; }"
              "if (docks.length !== 1) { return -2; }"
              "var dock = docks[0];"
              "dock.currentConfigGroup = ['General'];"
              "return String(dock.readConfig('panelId', '')) === %2 && "
              "String(dock.readConfig('panelType', '')) === %3 ? dock.id : -2;"
              "} catch (error) { return -3; }"
              "})();"
              "print('ARCHDOCK_RESULT:' + String(result));")
              .arg(hostMatch.containmentId)
              .arg(plasmaScriptStringLiteral(panelId))
              .arg(plasmaScriptStringLiteral(panelType))
        : QStringLiteral(
              "var result = (function() {"
              "try {"
              "var panel = panelById(%1);"
              "if (!panel) { return -3; }"
              "return panel.widgets('org.archdock.dock').length === 0 ? -1 : -2;"
              "} catch (error) { return -3; }"
              "})();"
              "print('ARCHDOCK_RESULT:' + String(result));")
              .arg(hostMatch.containmentId);
    const ArchDock::NativeContainmentMatch rendererMatch =
        ArchDock::classifyNativeContainmentMatch(
            evaluatePlasmaScriptResultOptional(rendererQuery));
    switch (rendererMatch.status)
    {
    case ArchDock::NativeContainmentMatchStatus::QueryFailed:
        return {};
    case ArchDock::NativeContainmentMatchStatus::Conflict:
        return {NativePanelDiscoveryStatus::RendererConflict,
                hostMatch.containmentId,
                -1};
    case ArchDock::NativeContainmentMatchStatus::Missing:
        return {NativePanelDiscoveryStatus::Unique, hostMatch.containmentId, -1};
    case ArchDock::NativeContainmentMatchStatus::Unique:
        return {NativePanelDiscoveryStatus::Unique,
                hostMatch.containmentId,
                rendererMatch.containmentId};
    }

    return {};
}

int PanelWindow::createNativePanelCandidate(const QString &panelId,
    const QString &ownershipToken, QString *errorCode) const
{
    const auto definition = m_panelRegistry.panelDefinition(panelId);
    if (!definition)
    {
        if (errorCode) *errorCode = QStringLiteral("invalid-recovery-record");
        return -1;
    }
    return createNativePanelCandidate(*definition, ownershipToken, errorCode);
}

int PanelWindow::createNativePanelCandidate(const ArchDock::PanelDefinition &definition,
                                            const QString &ownershipToken,
                                            QString *errorCode) const
{
    if (errorCode)
    {
        errorCode->clear();
    }
    const auto fail = [errorCode](const QString &code)
    {
        if (errorCode)
        {
            *errorCode = code;
        }
        return -1;
    };

    const QString panelId = definition.identity.id;
    if (panelId.isEmpty() || ownershipToken.isEmpty())
    {
        return fail(QStringLiteral("invalid-recovery-record"));
    }

    const QString edge = definition.placement.edge;
    const QString type = definition.content.type;
    if (!isNativeDockPanelEdge(edge) || !isNativeDockPanelType(type))
    {
        return fail(QStringLiteral("invalid-native-panel-definition"));
    }

    if (definition.host.screenIndex < 0 || definition.host.screenIndex >= QGuiApplication::screens().size())
    {
        return fail(QStringLiteral("screen-unavailable"));
    }

    const bool needsRenderer = panelTypeNeedsDockApplet(type);
    const QString rendererPreflight = needsRenderer
        ? QStringLiteral(
              "if (knownWidgetTypes.indexOf('org.archdock.dock') < 0) { return -2; }")
        : QString{};
    const QString hostSetup = QStringLiteral(
        "panel = new Panel;"
        "if (!panel || panel.id < 0) { return -1; }"
        "panel.currentConfigGroup = ['ArchDock'];"
        "panel.writeConfig('ownerToken', %1);"
        "panel.writeConfig('panelId', %2);"
        "panel.reloadConfig();"
        "if (panel.readConfig('ownerToken', '') !== %1 || "
        "panel.readConfig('panelId', '') !== %2) "
        "{ panel.remove(); panel = null; return -3; }"
        "panel.writeConfig('temporaryHidden', '0');"
        "panel.hiding = 'none';"
        "panel.reloadConfig();"
        "if (panel.readConfig('ownerToken', '') !== %1 || "
        "panel.readConfig('panelId', '') !== %2 || "
        "String(panel.readConfig('temporaryHidden', '0')) !== '0' || "
        "panel.hiding !== 'none') "
        "{ panel.remove(); panel = null; return -6; }")
        .arg(plasmaScriptStringLiteral(ownershipToken))
        .arg(plasmaScriptStringLiteral(panelId));
    const QString rendererAttachment = needsRenderer
        ? QStringLiteral(
              "var dock = panel.addWidget('org.archdock.dock');"
              "if (!dock) { panel.remove(); panel = null; return -4; }"
              "dock.currentConfigGroup = ['General'];"
              "dock.writeConfig('panelId', %1);"
              "dock.writeConfig('panelType', %2);"
              "dock.reloadConfig();"
              "if (dock.id < 0 || dock.type !== 'org.archdock.dock' || "
              "dock.readConfig('panelId', '') !== %1 || "
              "dock.readConfig('panelType', '') !== %2) "
              "{ panel.remove(); panel = null; return -5; }")
              .arg(plasmaScriptStringLiteral(panelId))
              .arg(plasmaScriptStringLiteral(type))
        : QString{};
    const QString script = QStringLiteral(
        "var result = (function() {"
        "var panel = null;"
        "try {"
        "%1"
        "%2"
        "%3"
        "return panel.id;"
        "} catch (error) {"
        "if (panel) { panel.remove(); }"
        "return -1;"
        "}"
        "})();"
        "print('ARCHDOCK_RESULT:' + String(result));")
        .arg(rendererPreflight, hostSetup, rendererAttachment);
    const int result = evaluatePlasmaScriptResult(script);
    if (result >= 0)
    {
        return result;
    }

    switch (result)
    {
    case -2:
        return fail(QStringLiteral("renderer-unavailable"));
    case -3:
        return fail(QStringLiteral("ownership-verification-failed"));
    case -4:
        return fail(QStringLiteral("renderer-attachment-failed"));
    case -5:
        return fail(QStringLiteral("renderer-verification-failed"));
    case -6:
        return fail(QStringLiteral("host-configuration-failed"));
    default:
        return fail(QStringLiteral("host-creation-failed"));
    }
}

std::optional<int> PanelWindow::verifiedNativeDockAppletId(
    const QString &panelId,
    int containmentId,
    const QString &panelType) const
{
    if (containmentId < 0 || !isNativeDockPanelType(panelType))
    {
        return std::nullopt;
    }

    if (!panelTypeNeedsDockApplet(panelType))
    {
        const int dockCount = evaluatePlasmaScriptResult(
            QStringLiteral(
                "var panel = panelById(%1);"
                "var count = panel ? panel.widgets('org.archdock.dock').length : -1;"
                "print('ARCHDOCK_RESULT:' + String(count));")
                .arg(containmentId));
        return dockCount == 0 ? std::optional<int>{-1} : std::nullopt;
    }

    const int dockId = evaluatePlasmaScriptResult(
        QStringLiteral(
            "var panel = panelById(%1);"
            "var docks = panel ? panel.widgets('org.archdock.dock') : [];"
            "var matches = 0;"
            "var result = -1;"
            "for (var index = 0; index < docks.length; ++index) {"
            "var dock = docks[index];"
            "dock.currentConfigGroup = ['General'];"
            "if (dock.readConfig('panelId', '') === %2 && "
            "dock.readConfig('panelType', '') === %3) "
            "{ ++matches; result = dock.id; }"
            "}"
            "print('ARCHDOCK_RESULT:' + String(matches === 1 ? result : -1));")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(panelId))
            .arg(plasmaScriptStringLiteral(panelType)));
    return dockId >= 0 ? std::optional<int>{dockId} : std::nullopt;
}

bool PanelWindow::rollbackNativePanelCandidate(const QString &panelId,
                                               int containmentId,
                                               const QString &ownershipToken) const
{
    if (containmentId < 0 || ownershipToken.isEmpty())
    {
        return false;
    }

    const int removed = evaluatePlasmaScriptResult(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print('ARCHDOCK_RESULT:1'); }"
            "else {"
            "panel.currentConfigGroup = ['ArchDock'];"
            "var owned = panel.readConfig('ownerToken', '') === %2 && "
            "panel.readConfig('panelId', '') === %3;"
            "if (!owned) { print('ARCHDOCK_RESULT:0'); }"
            "else { panel.remove(); print('ARCHDOCK_RESULT:1'); }"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(ownershipToken))
            .arg(plasmaScriptStringLiteral(panelId)));
    const std::optional<bool> containmentExists = nativePanelExistence(containmentId);
    return removed == 1 && containmentExists.has_value() && !*containmentExists;
}

bool PanelWindow::nativeControlAppletIsOwned(const QString &panelId,
                                              int containmentId,
                                              int appletId) const
{
    if (containmentId < 0 || appletId < 0)
    {
        return false;
    }

    return evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "var control = panel ? panel.widgetById(%2) : null;"
            "if (!control || control.type !== 'org.archdock.control') { print(0); }"
            "else {"
            "control.currentConfigGroup = ['General'];"
            "print(control.readConfig('panelId', '') === %3 ? 1 : 0);"
            "}")
            .arg(containmentId)
            .arg(appletId)
            .arg(plasmaScriptStringLiteral(panelId))) == 1;
}

bool PanelWindow::nativeDockAppletIsOwned(const QString &panelId,
                                           int containmentId,
                                           int appletId) const
{
    if (containmentId < 0 || appletId < 0)
    {
        return false;
    }

    return evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "var dock = panel ? panel.widgetById(%2) : null;"
            "if (!dock || dock.type !== 'org.archdock.dock') { print(0); }"
            "else {"
            "dock.currentConfigGroup = ['General'];"
            "print(dock.readConfig('panelId', '') === %3 ? 1 : 0);"
            "}")
            .arg(containmentId)
            .arg(appletId)
            .arg(plasmaScriptStringLiteral(panelId))) == 1;
}

bool PanelWindow::adoptNativePanelOwnership(const QString &panelId, int containmentId)
{
    if (profileBusy()) return false;

    if (nativePanelIsOwned(panelId, containmentId))
    {
        return true;
    }

    if (containmentId < 0 || !nativeOwnershipToken(panelId).isEmpty() ||
        (!nativeControlAppletIsOwned(panelId, containmentId, nativeControlAppletId(panelId)) &&
         !nativeDockAppletIsOwned(panelId, containmentId, nativeDockAppletId(panelId))))
    {
        return false;
    }

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const int adopted = evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print(0); }"
            "else {"
            "panel.currentConfigGroup = ['ArchDock'];"
            "panel.writeConfig('ownerToken', %2);"
            "panel.writeConfig('panelId', %3);"
            "panel.reloadConfig();"
            "print(panel.readConfig('ownerToken', '') === %2 && "
            "panel.readConfig('panelId', '') === %3 ? 1 : 0);"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(token))
            .arg(plasmaScriptStringLiteral(panelId)));
    if (adopted != 1)
    {
        return false;
    }

    m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeOwnershipToken"), token);
    return true;
}

bool PanelWindow::removeLegacyControlApplets(const QString &panelId, int containmentId)
{
    if (!nativePanelIsOwned(panelId, containmentId))
    {
        return false;
    }

    const int removed = evaluatePlasmaScript(
        QStringLiteral(
        "var panel = panelById(%1);"
        "if (!panel) { print(-1); }"
        "else {"
        "var widgets = panel.widgets();"
        "var removed = 0;"
        "for (var index = 0; index < widgets.length; ++index) {"
        "var widget = panel.widgetById(widgets[index].id);"
        "if (widget && widget.type === 'org.archdock.control') {"
        "widget.currentConfigGroup = ['General'];"
        "if (widget.readConfig('panelId', '') === %2) { widget.remove(); ++removed; }"
        "}"
        "}"
        "print(removed);"
        "}")
        .arg(containmentId)
        .arg(plasmaScriptStringLiteral(panelId)));
    if (removed < 0)
    {
        return false;
    }

    if (nativeControlAppletId(panelId) >= 0)
    {
        m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeControlAppletId"), -1);
    }
    return true;
}

bool PanelWindow::attachNativeDockApplet(const QString &panelId, int containmentId)
{
    if (!nativePanelIsOwned(panelId, containmentId))
    {
        return false;
    }
    if (!removeLegacyControlApplets(panelId, containmentId))
    {
        return false;
    }

    const QString type = m_panelRegistry.panelValue(panelId, QStringLiteral("type")).toString();
    const int storedDockId = nativeDockAppletId(panelId);
    const bool storedDockIsOwned = nativeDockAppletIsOwned(panelId, containmentId, storedDockId);

    if (!panelTypeNeedsDockApplet(type))
    {
        if (storedDockIsOwned)
        {
            const int removed = evaluatePlasmaScript(
                QStringLiteral(
                    "var panel = panelById(%1);"
                    "var dock = panel ? panel.widgetById(%2) : null;"
                    "if (dock) { dock.remove(); }"
                    "print(1);")
                    .arg(containmentId)
                    .arg(storedDockId));
            if (removed != 1)
            {
                return false;
            }
        }
        if (storedDockId >= 0)
        {
            m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeDockAppletId"), -1);
        }
        return type == QStringLiteral("empty");
    }

    if (!isNativeDockPanelType(type))
    {
        return false;
    }

    if (storedDockIsOwned)
    {
        return evaluatePlasmaScript(
            QStringLiteral(
                "var panel = panelById(%1);"
                "var dock = panel ? panel.widgetById(%2) : null;"
                "if (!dock) { print(0); }"
                "else {"
                "dock.currentConfigGroup = ['General'];"
                "dock.writeConfig('panelType', %3);"
                "dock.reloadConfig();"
                "print(1);"
                "}")
                .arg(containmentId)
                .arg(storedDockId)
                .arg(plasmaScriptStringLiteral(type))) == 1;
    }

    if (storedDockId >= 0)
    {
        m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeDockAppletId"), -1);
    }

    const int dockId = evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print(-1); }"
            "else {"
            "var existing = panel.widgets('org.archdock.dock');"
            "if (existing.length !== 0) { print(-2); }"
            "else {"
            "var dock = panel.addWidget('org.archdock.dock');"
            "if (!dock) { print(-1); }"
            "else {"
            "dock.currentConfigGroup = ['General'];"
            "dock.writeConfig('panelId', %2);"
            "dock.writeConfig('panelType', %3);"
            "dock.reloadConfig();"
            "print(dock.id);"
            "}"
            "}"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(panelId))
            .arg(plasmaScriptStringLiteral(type)));
    if (dockId < 0)
    {
        qWarning() << (dockId == -2
                           ? "Refusing to attach another Arch Dock visual applet to panel"
                           : "Could not attach the Arch Dock visual applet to panel")
                   << panelId;
        return false;
    }

    m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeDockAppletId"), dockId);
    return true;
}

bool PanelWindow::createNativeKdePanel(const QString &panelId)
{
    if (profileBusy()) return false;

    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
    if (!isNativeDockPanelEdge(edge))
    {
        return false;
    }

    int storedContainmentId = nativePanelId(panelId);
    const QString panelType = m_panelRegistry.panelValue(
        panelId, QStringLiteral("type")).toString();
    const QString storedOwnershipToken = nativeOwnershipToken(panelId).trimmed();
    if (!storedOwnershipToken.isEmpty() &&
        !nativePanelIsOwned(panelId, storedContainmentId, storedOwnershipToken))
    {
        const NativePanelDiscoveryResult discovery = discoverNativePanel(
            panelId, storedOwnershipToken, panelType);
        switch (discovery.status)
        {
        case NativePanelDiscoveryStatus::QueryFailed:
            qWarning() << "Could not query owned native panels before creation for" << panelId;
            return false;
        case NativePanelDiscoveryStatus::HostConflict:
            if (!m_panelRegistry.recordNativePanelRecoveryConflict(
                    panelId, storedOwnershipToken, QStringLiteral("multiple-owned-hosts")))
            {
                qWarning() << "Could not persist native host conflict for" << panelId;
            }
            return false;
        case NativePanelDiscoveryStatus::RendererConflict:
            if (!m_panelRegistry.recordNativePanelRecoveryConflict(
                    panelId,
                    storedOwnershipToken,
                    QStringLiteral("multiple-or-unverified-renderers")))
            {
                qWarning() << "Could not persist native renderer conflict for" << panelId;
            }
            return false;
        case NativePanelDiscoveryStatus::Missing:
        {
            const bool visible = m_panelRegistry.panelValue(
                panelId, QStringLiteral("visible")).toBool();
            if (!m_panelRegistry.detachMissingNativePanelAssociation(
                    panelId, storedOwnershipToken, visible))
            {
                return false;
            }
            if (!visible)
            {
                return true;
            }
            storedContainmentId = -1;
            break;
        }
        case NativePanelDiscoveryStatus::Unique:
            if (!m_panelRegistry.rebindRecoveredNativePanelAssociation(
                    panelId,
                    discovery.containmentId,
                    discovery.dockAppletId,
                    storedOwnershipToken))
            {
                return false;
            }
            storedContainmentId = discovery.containmentId;
            break;
        }
    }

    const std::optional<bool> storedContainmentExists = nativePanelExistence(
        storedContainmentId);
    if (!storedContainmentExists.has_value())
    {
        qWarning() << "Could not query the stored native panel before creation for" << panelId;
        return false;
    }
    if (*storedContainmentExists)
    {
        if (!adoptNativePanelOwnership(panelId, storedContainmentId))
        {
            qWarning() << "Refusing to replace a present but unverified native panel for"
                       << panelId;
            return false;
        }
        return synchronizeNativePanelPlacement(panelId) &&
            attachNativeDockApplet(panelId, storedContainmentId);
    }

    if (m_panelRegistry.panelValue(
            panelId, QStringLiteral("nativeRecoveryError")).toString() ==
        QStringLiteral("candidate-rollback-failed"))
    {
        qWarning() << "Refusing another native panel candidate while rollback is unresolved for"
                   << panelId;
        return false;
    }

    const auto recordFailure = [this, &panelId](const QString &errorCode)
    {
        if (!m_panelRegistry.recordNativePanelRecoveryFailure(panelId, errorCode))
        {
            qWarning() << "Could not persist native panel recovery failure" << errorCode
                       << "for" << panelId;
        }
        return false;
    };
    const auto rollbackAndRecordFailure =
        [this, &panelId, &recordFailure](int containmentId,
                                         const QString &ownershipToken,
                                         const QString &errorCode)
    {
        if (!rollbackNativePanelCandidate(panelId, containmentId, ownershipToken))
        {
            qWarning() << "Could not roll back the verified native panel candidate for"
                       << panelId;
            return recordFailure(QStringLiteral("candidate-rollback-failed"));
        }
        return recordFailure(errorCode);
    };

    const QString ownershipToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString creationError;
    const int createdId = createNativePanelCandidate(panelId, ownershipToken, &creationError);
    if (createdId < 0)
    {
        return recordFailure(creationError.isEmpty()
                ? QStringLiteral("host-creation-failed")
                : creationError);
    }

    if (!nativePanelIsOwned(panelId, createdId, ownershipToken))
    {
        return rollbackAndRecordFailure(
            createdId, ownershipToken, QStringLiteral("ownership-verification-failed"));
    }

    QString placementError;
    if (!applyNativePanelPlacement(
            panelId, createdId, ownershipToken, &placementError))
    {
        return rollbackAndRecordFailure(
            createdId,
            ownershipToken,
            placementError.isEmpty()
                ? QStringLiteral("placement-apply-failed")
                : placementError);
    }

    const std::optional<int> dockAppletId = verifiedNativeDockAppletId(
        panelId, createdId, panelType);
    if (!dockAppletId.has_value())
    {
        return rollbackAndRecordFailure(
            createdId, ownershipToken, QStringLiteral("renderer-verification-failed"));
    }

    if (!m_panelRegistry.commitVerifiedNativePanelAssociation(
            panelId, createdId, *dockAppletId, ownershipToken))
    {
        return rollbackAndRecordFailure(
            createdId, ownershipToken, QStringLiteral("registry-persistence-failed"));
    }

    if (!m_nativePanelRecoveryActive)
    {
        scheduleNativePanelRecovery();
    }
    return true;
}

bool PanelWindow::addKdeWidget(const QString &panelId, const QString &appletId)
{
    if (profileBusy()) return false;

    const QString pluginId = appletId.trimmed();
    if (!m_panelRegistry.panelIds().contains(panelId) || pluginId.isEmpty())
    {
        return false;
    }

    for (const QChar character : pluginId)
    {
        if (!character.isLetterOrNumber() && character != QLatin1Char('.') &&
            character != QLatin1Char('_') && character != QLatin1Char('-'))
        {
            return false;
        }
    }

    if (!createNativeKdePanel(panelId))
    {
        return false;
    }

    const int containmentId = nativePanelId(panelId);
    const QString script = QStringLiteral(
        "var panel = panelById(%1);"
        "if (!panel) { print(-1); }"
        "else { var widget = panel.addWidget('%2'); print(widget ? widget.id : -1); }")
        .arg(containmentId)
        .arg(pluginId);
    const int widgetId = evaluatePlasmaScript(script);
    if (widgetId < 0)
    {
        return false;
    }

    QStringList widgets = m_panelRegistry.panelValue(panelId, QStringLiteral("kdeWidgets")).toStringList();
    if (!widgets.contains(pluginId))
    {
        widgets.append(pluginId);
        m_panelRegistry.setPanelValue(panelId, QStringLiteral("kdeWidgets"), widgets);
    }
    return true;
}

bool PanelWindow::removeNativeKdePanel(const QString &panelId)
{
    if (profileBusy()) return false;

    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    const QString edge = m_panelRegistry.panelValue(
        panelId, QStringLiteral("edge")).toString();
    const QString panelType = m_panelRegistry.panelValue(
        panelId, QStringLiteral("type")).toString();
    if (!isNativeDockPanelEdge(edge) || !isNativeDockPanelType(panelType))
    {
        return false;
    }

    const auto clearNativeAssociation = [this, &panelId]
    {
        m_panelRegistry.updatePanel(
            panelId,
            {{QStringLiteral("nativePanelId"), -1},
             {QStringLiteral("nativeControlAppletId"), -1},
             {QStringLiteral("nativeDockAppletId"), -1},
             {QStringLiteral("nativeOwnershipToken"), QString{}}});
    };

    const int containmentId = nativePanelId(panelId);
    if (containmentId < 0)
    {
        clearNativeAssociation();
        return true;
    }

    const std::optional<bool> containmentExists = nativePanelExistence(containmentId);
    if (!containmentExists.has_value())
    {
        qWarning() << "Could not query the native panel before permanent removal for"
                   << panelId;
        return false;
    }
    if (!*containmentExists)
    {
        clearNativeAssociation();
        return true;
    }

    if (!adoptNativePanelOwnership(panelId, containmentId))
    {
        qWarning() << "Refusing to permanently remove an unowned native panel for"
                   << panelId;
        return false;
    }

    const QString ownershipToken = nativeOwnershipToken(panelId).trimmed();
    if (ownershipToken.isEmpty() ||
        !nativePanelIsOwned(panelId, containmentId, ownershipToken))
    {
        qWarning() << "Refusing permanent removal without a verified ownership token for"
                   << panelId;
        return false;
    }

    const int storedDockAppletId = nativeDockAppletId(panelId);
    const std::optional<int> verifiedDockAppletId = verifiedNativeDockAppletId(
        panelId, containmentId, panelType);
    const bool rendererAttached = verifiedDockAppletId.has_value() &&
        (panelTypeNeedsDockApplet(panelType)
             ? storedDockAppletId >= 0 && *verifiedDockAppletId == storedDockAppletId
             : storedDockAppletId == -1 && *verifiedDockAppletId == -1);
    const ArchDock::NativeContainmentLifecycleState state{
        false,
        ArchDock::NativeContainmentHostStatus::Owned,
        rendererAttached,
        ArchDock::NativeContainmentPresentation::Hidden,
    };
    if (ArchDock::nativeContainmentLifecycleIntent(
            ArchDock::NativeContainmentLifecycleRequest::RemovePermanently,
            state) != ArchDock::NativeContainmentLifecycleIntent::RemoveHostPermanently)
    {
        qWarning() << "Refusing permanent removal without the expected native renderer for"
                   << panelId;
        return false;
    }

    const QString rendererVerification = panelTypeNeedsDockApplet(panelType)
        ? QStringLiteral(
              "var docks = panel.widgets('org.archdock.dock');"
              "if (docks.length !== 1) { return 0; }"
              "var dock = panel.widgetById(%1);"
              "if (!dock || dock.id !== %1 || dock.type !== 'org.archdock.dock') "
              "{ return 0; }"
              "dock.currentConfigGroup = ['General'];"
              "if (String(dock.readConfig('panelId', '')) !== %2 || "
              "String(dock.readConfig('panelType', '')) !== %3) { return 0; }")
              .arg(storedDockAppletId)
              .arg(plasmaScriptStringLiteral(panelId))
              .arg(plasmaScriptStringLiteral(panelType))
        : QStringLiteral(
              "if (panel.widgets('org.archdock.dock').length !== 0) { return 0; }");
    const int removed = evaluatePlasmaScriptResult(
        QStringLiteral(
            "var result = (function() {"
            "try {"
            "var panel = panelById(%1);"
            "if (!panel) { return 1; }"
            "panel.currentConfigGroup = ['ArchDock'];"
            "if (String(panel.readConfig('ownerToken', '')) !== %2 || "
            "String(panel.readConfig('panelId', '')) !== %3) { return 0; }"
            "%4"
            "panel.remove();"
            "return 1;"
            "} catch (error) { return -1; }"
            "})();"
            "print('ARCHDOCK_RESULT:' + String(result));")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(ownershipToken))
            .arg(plasmaScriptStringLiteral(panelId))
            .arg(rendererVerification));
    if (removed != 1)
    {
        qWarning() << "Plasma refused the verified permanent native panel removal for"
                   << panelId;
        return false;
    }

    const std::optional<bool> removedContainmentExists = nativePanelExistence(containmentId);
    if (!removedContainmentExists.has_value() || *removedContainmentExists)
    {
        qWarning() << "Could not verify permanent native panel removal for" << panelId;
        return false;
    }

    clearNativeAssociation();
    return true;
}

void PanelWindow::removePanel(const QString &panelId)
{
    if (profileBusy()) return;

    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    if (m_panelRegistry.isBuiltIn(panelId))
    {
        if (!setPanelVisible(panelId, false))
        {
            qWarning() << "Could not hide built-in native panel" << panelId;
        }
        return;
    }

    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() ==
        QStringLiteral("free"))
    {
        ArchDock::FreePanelController controller(
            m_panelRegistry, freePanelHostOperations());
        const ArchDock::FreePanelLifecycleResult result = controller.remove(panelId);
        if (!result.success)
        {
            qWarning() << "Preserving the free-panel record after host removal was refused for"
                       << panelId << result.errorCode;
            return;
        }
        updateDesktopSuite();
        return;
    }

    if (!removeNativeKdePanel(panelId))
    {
        qWarning() << "Preserving the panel record after native removal was refused for"
                   << panelId;
        return;
    }
    m_panelRegistry.removePanel(panelId);
    updateDesktopSuite();
}
