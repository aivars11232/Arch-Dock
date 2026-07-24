#include "PanelWindow.h"

#include "../NativeContainmentLifecycle.h"
#include "../PanelPlacement.h"
#include "../ScreenIdentity.h"

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegion>
#include <QRegularExpression>
#include <QScreen>
#include <QSet>
#include <QStandardPaths>
#include <QSettings>
#include <QSize>
#include <QTimer>
#include <QUuid>
#include <QWindow>

#include <array>

namespace
{
QString plasmaScriptStringLiteral(const QString &value)
{
    QJsonArray values;
    values.append(value);
    const QString encoded = QString::fromUtf8(
        QJsonDocument(values).toJson(QJsonDocument::Compact));
    return encoded.mid(1, encoded.size() - 2);
}

bool isNativeDockPanelType(const QString &type)
{
    return type == QStringLiteral("empty") ||
        type == QStringLiteral("launcher") ||
        type == QStringLiteral("tasks") ||
        type == QStringLiteral("hybrid");
}

    bool isNativeDockPanelEdge(const QString &edge)
    {
        return edge == QStringLiteral("top") ||
        edge == QStringLiteral("bottom") ||
        edge == QStringLiteral("left") ||
        edge == QStringLiteral("right");
    }

bool panelTypeNeedsDockApplet(const QString &type)
{
    return type == QStringLiteral("launcher") ||
        type == QStringLiteral("tasks") ||
        type == QStringLiteral("hybrid");
}
}

PanelWindow::PanelWindow(QQmlApplicationEngine &engine,
                         QObject *parent)
    : QObject(parent),
      m_engine(engine),
            m_windowModel(this),
            m_dockModel(m_windowModel, this),
    m_settings(this),
        m_panelRegistry(this),
    m_systemStatus(this),
    m_actionBridge(this),
      m_windowWatcher(m_windowModel)
{
    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
            QStringLiteral("free"))
        {
            continue;
        }
        const QStringList legacyIds = m_panelRegistry.panelValue(
            panelId, QStringLiteral("contentAppIds")).toStringList();
        if (legacyIds.isEmpty())
        {
            continue;
        }
        QStringList urls = m_panelRegistry.panelValue(
            panelId, QStringLiteral("contentUrls")).toStringList();
        for (const QString &appId : legacyIds)
        {
            const QUrl url = m_dockModel.urlForApplicationId(appId);
            if (url.isValid() && !urls.contains(url.toString()))
            {
                urls.append(url.toString());
            }
        }
        m_panelRegistry.updatePanel(
            panelId,
            {{QStringLiteral("contentUrls"), urls},
             {QStringLiteral("contentAppIds"), QStringList{}}});
        m_dockModel.removePinnedApplications(legacyIds);
    }

    connect(&m_dockModel,
        &DockModel::windowActionRequested,
        &m_actionBridge,
        &KWinActionBridge::requestAction);

    m_engine.rootContext()->setContextProperty(
        QStringLiteral("windowModel"),
        &m_windowModel);
        m_engine.rootContext()->setContextProperty(
                QStringLiteral("dockModel"),
                &m_dockModel);
        m_engine.rootContext()->setContextProperty(
            QStringLiteral("dockSettings"),
            &m_settings);
        m_engine.rootContext()->setContextProperty(
            QStringLiteral("panelRegistry"),
            &m_panelRegistry);
        m_engine.rootContext()->setContextProperty(
            QStringLiteral("systemStatus"),
            &m_systemStatus);
        m_engine.rootContext()->setContextProperty(
            QStringLiteral("panelController"),
            this);

        QDBusConnection sessionBus = QDBusConnection::sessionBus();
        sessionBus.registerObject(
            QStringLiteral("/Control"),
            this,
            QDBusConnection::ExportAllSlots |
                QDBusConnection::ExportAllSignals |
                QDBusConnection::ExportAllProperties);
        if (QDBusConnectionInterface *interface = sessionBus.interface())
        {
            connect(interface,
                    &QDBusConnectionInterface::serviceOwnerChanged,
                    this,
                    [this](const QString &service,
                           const QString &oldOwner,
                           const QString &newOwner)
                    {
                        if (service != QStringLiteral("org.kde.plasmashell") ||
                            oldOwner == newOwner)
                        {
                            return;
                        }
                        if (newOwner.isEmpty())
                        {
                            ++m_nativePanelRecoveryGeneration;
                            return;
                        }
                        scheduleNativePanelRecovery();
                    });
        }

    connect(&m_settings, &DockSettings::desktopSuiteChanged, this, &PanelWindow::updateDesktopSuite);
    connect(&m_settings, &DockSettings::monitorIndexChanged, this, &PanelWindow::updateDesktopSuite);
    connect(&m_settings, &DockSettings::topLauncherVisibleChanged, this, &PanelWindow::updateDesktopSuite);
    connect(&m_settings, &DockSettings::sideRailVisibleChanged, this, &PanelWindow::updateDesktopSuite);
    connect(&m_dockModel, &DockModel::countChanged, this, [this]
            {
                ++m_dockRevision;
                emit dockRevisionChanged();
            });
    connect(&m_panelRegistry, &PanelRegistry::revisionChanged, this, [this]
            {
                ++m_dockRevision;
                emit dockRevisionChanged();
            });
    const auto notifyGlobalVisualChange = [this]
    {
        ++m_dockRevision;
        emit dockRevisionChanged();
    };
    connect(&m_settings, &DockSettings::magnificationChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::magnificationEnabledChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::showReflectionsChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::showIndicatorsChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::showTooltipsChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::animationDurationChanged, this, notifyGlobalVisualChange);
    connect(&m_settings, &DockSettings::reducedMotionChanged, this, notifyGlobalVisualChange);
    connect(&m_panelRegistry, &PanelRegistry::panelsChanged, this, &PanelWindow::updateDesktopSuite);
    const auto updateVisibility = [this]
    {
        ++m_visibilityRevision;
        emit visibilityRevisionChanged();
    };
    connect(&m_windowModel, &QAbstractItemModel::dataChanged, this, updateVisibility);
    connect(&m_windowModel, &QAbstractItemModel::rowsInserted, this, updateVisibility);
    connect(&m_windowModel, &QAbstractItemModel::rowsRemoved, this, updateVisibility);
    connect(&m_windowModel, &QAbstractItemModel::modelReset, this, updateVisibility);

    if (auto *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
    {
        connect(application, &QGuiApplication::screenAdded, this, [this]
                {
                    handleScreensChanged();
                });
        connect(application, &QGuiApplication::screenRemoved, this, [this]
                {
                    handleScreensChanged();
                });
    }

    synchronizeScreenAssignments();
    scheduleNativePanelRecovery();
}

PanelWindow::~PanelWindow()
{
    qDeleteAll(m_freePanelWindows);
    m_freePanelWindows.clear();
    delete m_settingsWindow;
    delete m_iconPropertiesWindow;
}

int PanelWindow::screenRevision() const
{
    return m_screenRevision;
}

int PanelWindow::visibilityRevision() const
{
    return m_visibilityRevision;
}

qulonglong PanelWindow::dockRevision() const
{
    return m_dockRevision;
}

QVariantMap PanelWindow::dockConfiguration(const QString &panelId) const
{
    const auto panel = [this, &panelId](const char *key, const QVariant &fallback)
    {
        const QVariant value = m_panelRegistry.panelValue(panelId, QString::fromLatin1(key));
        return value.isValid() ? value : fallback;
    };
    return {
        {QStringLiteral("iconSize"), panel("iconSize", m_settings.iconSize())},
        {QStringLiteral("spacing"), panel("spacing", m_settings.spacing())},
        {QStringLiteral("opacity"), panel("opacity", m_settings.panelOpacity())},
        {QStringLiteral("shape"), panel("shape", m_settings.panelShape())},
        {QStringLiteral("iconShape"), panel("iconShape", m_settings.iconTileShape())},
        {QStringLiteral("appearance"), panel("appearance", m_settings.appearancePreset())},
        {QStringLiteral("layout"), panel("layout", QStringLiteral("adaptive"))},
        {QStringLiteral("layoutScale"), panel("layoutScale", 1.0)},
        {QStringLiteral("layoutAngle"), panel("layoutAngle", 0.0)},
        {QStringLiteral("layoutRadius"), panel("layoutRadius", 150)},
        {QStringLiteral("layoutRows"), panel("layoutRows", 2)},
        {QStringLiteral("layoutPadding"), panel("layoutPadding", 18)},
        {QStringLiteral("pathSides"), panel("pathSides", 6)},
        {QStringLiteral("pathOrientation"), panel("pathOrientation", QStringLiteral("upright"))},
        {QStringLiteral("iconAnimation"), panel("iconAnimation", QStringLiteral("scale"))},
        {QStringLiteral("animationTrigger"), panel("animationTrigger", QStringLiteral("hover"))},
        {QStringLiteral("animationSpeed"), panel("animationSpeed", 1.0)},
        {QStringLiteral("animationIntensity"), panel("animationIntensity", 1.0)},
        {QStringLiteral("acceptDrops"), panel("acceptDrops", true)},
        {QStringLiteral("magnification"), m_settings.magnification()},
        {QStringLiteral("magnificationEnabled"), m_settings.magnificationEnabled()},
        {QStringLiteral("showReflections"), m_settings.showReflections()},
        {QStringLiteral("showIndicators"), m_settings.showIndicators()},
        {QStringLiteral("showTooltips"), m_settings.showTooltips()},
        {QStringLiteral("animationDuration"), m_settings.animationDuration()},
        {QStringLiteral("reducedMotion"), m_settings.reducedMotion()}};
}

bool PanelWindow::setDockConfiguration(const QString &panelId,
                                       const QString &key,
                                       const QVariant &value)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    if (key == QStringLiteral("magnification"))
        m_settings.setMagnification(value.toReal());
    else if (key == QStringLiteral("magnificationEnabled"))
        m_settings.setMagnificationEnabled(value.toBool());
    else if (key == QStringLiteral("showReflections"))
        m_settings.setShowReflections(value.toBool());
    else if (key == QStringLiteral("showIndicators"))
        m_settings.setShowIndicators(value.toBool());
    else if (key == QStringLiteral("showTooltips"))
        m_settings.setShowTooltips(value.toBool());
    else if (key == QStringLiteral("animationDuration"))
        m_settings.setAnimationDuration(value.toInt());
    else if (key == QStringLiteral("reducedMotion"))
        m_settings.setReducedMotion(value.toBool());
    else
    {
        static const QSet<QString> panelKeys{
            QStringLiteral("iconSize"),
            QStringLiteral("spacing"),
            QStringLiteral("opacity"),
            QStringLiteral("shape"),
            QStringLiteral("iconShape"),
            QStringLiteral("appearance"),
            QStringLiteral("iconAnimation"),
            QStringLiteral("animationTrigger"),
            QStringLiteral("animationSpeed"),
            QStringLiteral("animationIntensity"),
            QStringLiteral("acceptDrops")};
        if (!panelKeys.contains(key))
            return false;
        m_panelRegistry.setPanelValue(panelId, key, value);
    }
    return true;
}

bool PanelWindow::setDockStringConfiguration(const QString &panelId,
                                             const QString &key,
                                             const QString &value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockIntegerConfiguration(const QString &panelId,
                                              const QString &key,
                                              int value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockRealConfiguration(const QString &panelId,
                                           const QString &key,
                                           double value)
{
    return setDockConfiguration(panelId, key, value);
}

bool PanelWindow::setDockBooleanConfiguration(const QString &panelId,
                                              const QString &key,
                                              bool value)
{
    return setDockConfiguration(panelId, key, value);
}

QVariantList PanelWindow::dockEntries(const QString &panelType) const
{
    return m_dockModel.panelEntries(panelType);
}

QVariantList PanelWindow::dockEntriesForPanel(const QString &panelId,
                                              const QString &panelType) const
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() ==
        QStringLiteral("free"))
    {
        QVariantList entries;
        const QStringList urls = m_panelRegistry.panelValue(
            panelId, QStringLiteral("contentUrls")).toStringList();
        QMimeDatabase mimeDatabase;
        for (const QString &urlString : urls)
        {
            const QUrl url(urlString);
            if (!url.isLocalFile())
            {
                continue;
            }
            const QFileInfo info(url.toLocalFile());
            if (!info.exists())
            {
                continue;
            }
            QString iconName;
            QString displayName = info.fileName();
            if (info.isDir())
            {
                iconName = QStringLiteral("folder");
            }
            else if (info.suffix().compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0)
            {
                QSettings desktopEntry(info.absoluteFilePath(), QSettings::IniFormat);
                desktopEntry.beginGroup(QStringLiteral("Desktop Entry"));
                displayName = desktopEntry.value(QStringLiteral("Name"), info.completeBaseName()).toString();
                iconName = desktopEntry.value(QStringLiteral("Icon"), QStringLiteral("application-x-executable")).toString();
                desktopEntry.endGroup();
            }
            else
            {
                iconName = mimeDatabase.mimeTypeForFile(info).iconName();
            }
            entries.append(QVariantMap{
                {QStringLiteral("appId"), QStringLiteral("free-url:") +
                    QString::fromUtf8(url.toEncoded())},
                {QStringLiteral("desktopFileName"), QString{}},
                {QStringLiteral("iconName"), iconName},
                {QStringLiteral("displayName"), displayName},
                {QStringLiteral("pinned"), true},
                {QStringLiteral("running"), false},
                {QStringLiteral("active"), false},
                {QStringLiteral("minimized"), false},
                {QStringLiteral("windowCount"), 0},
                {QStringLiteral("windowIds"), QStringList{}},
                {QStringLiteral("windowTitles"), QStringList{}},
                {QStringLiteral("isFolder"), info.isDir()}});
        }
        return entries;
    }
    return m_dockModel.panelEntries(panelType);
}

bool PanelWindow::activateDockEntry(const QString &appId)
{
    if (appId.startsWith(QStringLiteral("free-url:")))
    {
        return m_dockModel.openUrl(
            QUrl::fromEncoded(appId.mid(9).toUtf8()));
    }
    return m_dockModel.activateApplication(appId);
}

bool PanelWindow::activateDockWindow(const QString &appId, const QString &windowId)
{
    return m_dockModel.activateApplicationWindow(appId, windowId);
}

bool PanelWindow::minimizeDockEntry(const QString &appId)
{
    return m_dockModel.minimizeApplication(appId);
}

bool PanelWindow::closeDockEntry(const QString &appId)
{
    return m_dockModel.closeApplication(appId);
}

bool PanelWindow::closeAllDockEntry(const QString &appId)
{
    return m_dockModel.closeAllApplication(appId);
}

bool PanelWindow::togglePinnedDockEntry(const QString &appId)
{
    return m_dockModel.togglePinnedApplication(appId);
}

bool PanelWindow::moveDockEntryBefore(const QString &appId, const QString &beforeAppId)
{
    return m_dockModel.moveApplicationBefore(appId, beforeAppId);
}

bool PanelWindow::pinDockUrl(const QString &url)
{
    return pinDockUrls({url});
}

bool PanelWindow::pinDockUrls(const QStringList &urls)
{
    bool pinnedAny = false;
    for (const QString &urlString : urls)
    {
        const QUrl url = QUrl::fromUserInput(urlString);
        if (url.isValid() && !url.isEmpty())
        {
            pinnedAny = m_dockModel.pinUrl(url) || pinnedAny;
        }
    }
    return pinnedAny;
}

bool PanelWindow::pinPanelUrls(const QString &panelId, const QStringList &urls)
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
        QStringLiteral("free"))
    {
        return pinDockUrls(urls);
    }

    QStringList contentUrls = m_panelRegistry.panelValue(
        panelId, QStringLiteral("contentUrls")).toStringList();
    bool addedAny = false;
    for (const QString &urlString : urls)
    {
        const QUrl url = QUrl::fromUserInput(urlString);
        if (!url.isLocalFile() || !QFileInfo::exists(url.toLocalFile()))
        {
            continue;
        }
        const QString normalized = url.toString();
        if (!contentUrls.contains(normalized))
        {
            contentUrls.append(normalized);
        }
        addedAny = true;
    }
    if (addedAny)
    {
        m_panelRegistry.setPanelValue(
            panelId, QStringLiteral("contentUrls"), contentUrls);
    }
    return addedAny;
}

bool PanelWindow::removePanelContent(const QString &panelId, const QString &entryId)
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() !=
            QStringLiteral("free") ||
        !entryId.startsWith(QStringLiteral("free-url:")))
    {
        return false;
    }

    const QUrl url = QUrl::fromEncoded(entryId.mid(9).toUtf8());
    QStringList contentUrls = m_panelRegistry.panelValue(
        panelId, QStringLiteral("contentUrls")).toStringList();
    if (contentUrls.removeAll(url.toString()) == 0)
    {
        return false;
    }
    m_panelRegistry.setPanelValue(
        panelId, QStringLiteral("contentUrls"), contentUrls);
    return true;
}

QVariantList PanelWindow::dockFolderEntries(const QString &appId) const
{
    QVariantList entries = m_dockModel.folderEntriesForApplication(appId);
    for (QVariant &entry : entries)
    {
        QVariantMap value = entry.toMap();
        value.insert(QStringLiteral("url"), value.value(QStringLiteral("url")).toUrl().toString());
        entry = value;
    }
    return entries;
}

bool PanelWindow::openDockUrl(const QString &urlString)
{
    const QUrl url = QUrl::fromUserInput(urlString);
    return url.isValid() && !url.isEmpty() && m_dockModel.openUrl(url);
}

QScreen *PanelWindow::screenForPanel(const QString &panelId) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return nullptr;
    }

    QStringList screenIds;
    screenIds.reserve(screens.size());
    for (const QScreen *screen : screens)
    {
        screenIds.append(ArchDock::persistentScreenId(screen));
    }

    const int requestedScreen = m_panelRegistry.panelValue(panelId, QStringLiteral("screen")).toInt();
    const QString requestedScreenId = m_panelRegistry.panelValue(
        panelId,
        QStringLiteral("screenId")).toString();
    return screens.at(ArchDock::resolvedScreenIndex(screenIds, requestedScreenId, requestedScreen));
}

QString PanelWindow::screenIdForIndex(int screenIndex) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return {};
    }
    return ArchDock::persistentScreenId(screens.at(qBound(0, screenIndex, screens.size() - 1)));
}

QVariantList PanelWindow::availableScreens() const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    QVariantList entries;
    entries.reserve(screens.size());
    for (int index = 0; index < screens.size(); ++index)
    {
        const QScreen *screen = screens.at(index);
        const QString outputName = screen->name().trimmed();
        const QString manufacturer = screen->manufacturer().trimmed();
        const QString model = screen->model().trimmed();
        const QString deviceName = (manufacturer + QLatin1Char(' ') + model).trimmed();
        const QString label = outputName.isEmpty()
            ? tr("Display %1").arg(index + 1)
            : deviceName.isEmpty() || deviceName == outputName
                ? outputName
                : tr("%1 (%2)").arg(outputName, deviceName);
        entries.append(QVariant::fromValue(QVariantMap{
            {QStringLiteral("index"), index},
            {QStringLiteral("id"), ArchDock::persistentScreenId(screen)},
            {QStringLiteral("label"), label}}));
    }
    return entries;
}

int PanelWindow::screenIndexForPanel(const QString &panelId) const
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return -1;
    }

    QStringList screenIds;
    screenIds.reserve(screens.size());
    for (const QScreen *screen : screens)
    {
        screenIds.append(ArchDock::persistentScreenId(screen));
    }
    return ArchDock::resolvedScreenIndex(
        screenIds,
        m_panelRegistry.panelValue(panelId, QStringLiteral("screenId")).toString(),
        m_panelRegistry.panelValue(panelId, QStringLiteral("screen")).toInt());
}

void PanelWindow::setPanelScreen(const QString &panelId, int screenIndex)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return;
    }

    const int boundedIndex = qBound(0, screenIndex, screens.size() - 1);
    m_panelRegistry.updatePanel(
        panelId,
        {{QStringLiteral("screen"), boundedIndex},
         {QStringLiteral("screenId"), ArchDock::persistentScreenId(screens.at(boundedIndex))}});
    if (!synchronizeNativePanelScreen(panelId) && nativePanelId(panelId) >= 0)
    {
        qWarning() << "Could not move the native Plasma panel for" << panelId;
    }
}

void PanelWindow::setPanelVisibilityMode(const QString &panelId, const QString &visibilityMode)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    m_panelRegistry.setPanelValue(panelId, QStringLiteral("visibilityMode"), visibilityMode);
    if (panelId == QStringLiteral("bottom"))
    {
        const bool autoHide = m_panelRegistry.panelValue(
            panelId,
            QStringLiteral("visibilityMode")).toString() == QStringLiteral("auto-hide");
        if (m_settings.autoHide() != autoHide)
        {
            m_settings.setAutoHide(autoHide);
        }
    }
}

QList<ArchDock::EdgePanel> PanelWindow::edgePanels() const
{
    QList<ArchDock::EdgePanel> panels;
    const QStringList panelIds = m_panelRegistry.panelIds();
    panels.reserve(panelIds.size());
    for (const QString &panelId : panelIds)
    {
        const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
        if (!isNativeDockPanelEdge(edge))
        {
            continue;
        }
        const bool vertical = edge == QStringLiteral("left") || edge == QStringLiteral("right");
        panels.append({panelId,
                       edge,
                       screenIndexForPanel(panelId),
                       m_panelRegistry.panelValue(
                           panelId,
                           vertical ? QStringLiteral("width") : QStringLiteral("height")).toInt(),
                       m_panelRegistry.panelValue(panelId, QStringLiteral("visible")).toBool()});
    }
    return panels;
}

bool PanelWindow::shouldConcealPanel(const QString &) const
{
    return false;
}

void PanelWindow::synchronizeScreenAssignments()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty())
    {
        return;
    }

    QStringList screenIds;
    screenIds.reserve(screens.size());
    for (const QScreen *screen : screens)
    {
        screenIds.append(ArchDock::persistentScreenId(screen));
    }

    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        const int storedIndex = m_panelRegistry.panelValue(panelId, QStringLiteral("screen")).toInt();
        const QString storedId = m_panelRegistry.panelValue(panelId, QStringLiteral("screenId")).toString();
        const int resolvedIndex = ArchDock::resolvedScreenIndex(screenIds, storedId, storedIndex);
        QVariantMap updates;
        if (storedIndex != resolvedIndex)
        {
            updates.insert(QStringLiteral("screen"), resolvedIndex);
        }
        if (storedId.isEmpty() && !screenIds.at(resolvedIndex).isEmpty())
        {
            updates.insert(QStringLiteral("screenId"), screenIds.at(resolvedIndex));
        }
        if (!updates.isEmpty())
        {
            m_panelRegistry.updatePanel(panelId, updates);
        }
    }
}

void PanelWindow::handleScreensChanged()
{
    synchronizeScreenAssignments();
    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        if (nativePanelId(panelId) >= 0 && !synchronizeNativePanelScreen(panelId))
        {
            qWarning() << "Could not update the native Plasma panel screen for" << panelId;
        }
    }
    ++m_screenRevision;
    emit screenRevisionChanged();
    updateDesktopSuite();
}

void PanelWindow::scheduleNativePanelRecovery()
{
    const int generation = ++m_nativePanelRecoveryGeneration;
    constexpr std::array<int, 3> recoveryDelays{500, 2000, 5000};
    for (int index = 0; index < static_cast<int>(recoveryDelays.size()); ++index)
    {
        QTimer::singleShot(
            recoveryDelays.at(index),
            this,
            [this, generation, finalAttempt = index == static_cast<int>(recoveryDelays.size()) - 1]
            {
                if (generation != m_nativePanelRecoveryGeneration)
                {
                    return;
                }

                recoverNativePanels();
                if (finalAttempt)
                {
                    emit nativePanelRecoveryFinished();
                }
            });
    }
}

void PanelWindow::recoverNativePanels()
{
    QDBusInterface plasmaShell(
        QStringLiteral("org.kde.plasmashell"),
        QStringLiteral("/PlasmaShell"),
        QStringLiteral("org.kde.PlasmaShell"),
        QDBusConnection::sessionBus());
    if (!plasmaShell.isValid())
    {
        return;
    }

    m_nativePanelRecoveryActive = true;
    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
        if (edge == QStringLiteral("free"))
        {
            continue;
        }
        const bool visible = m_panelRegistry.panelValue(panelId, QStringLiteral("visible")).toBool();
        if (!visible)
        {
            if (nativePanelId(panelId) >= 0)
            {
                removeNativeKdePanel(panelId);
            }
            continue;
        }

        if (!createNativeKdePanel(panelId))
        {
            qWarning() << "Could not recover the native Plasma panel for" << panelId;
        }
    }
    m_nativePanelRecoveryActive = false;
    synchronizeFreePanels();
}

void PanelWindow::updateDesktopSuite()
{
    synchronizeFreePanels();
    scheduleNativePanelRecovery();
    ++m_visibilityRevision;
    emit visibilityRevisionChanged();
}

void PanelWindow::synchronizeFreePanels()
{
    // Free docks are hosted by Plasma desktop containments. Remove any legacy
    // utility windows left alive by an older Arch Dock process.
    qDeleteAll(m_freePanelWindows);
    m_freePanelWindows.clear();
}

void PanelWindow::syncRegistryFromLegacySettings()
{
    const QVariantMap sharedValues{
        {QStringLiteral("screen"), m_settings.monitorIndex()},
        {QStringLiteral("screenId"), screenIdForIndex(m_settings.monitorIndex())},
        {QStringLiteral("iconSize"), m_settings.iconSize()},
        {QStringLiteral("spacing"), m_settings.spacing()},
        {QStringLiteral("appearance"), m_settings.appearancePreset()},
        {QStringLiteral("shape"), m_settings.panelShape()},
        {QStringLiteral("iconShape"), m_settings.iconTileShape()},
        {QStringLiteral("opacity"), m_settings.panelOpacity()}};

    for (const QString &panelId : {QStringLiteral("bottom"),
                                   QStringLiteral("top"),
                                   QStringLiteral("side")})
    {
        if (m_panelRegistry.isBuiltIn(panelId))
        {
            m_panelRegistry.updatePanel(panelId, sharedValues);
        }
    }

    m_panelRegistry.updatePanel(
        QStringLiteral("bottom"),
        {{QStringLiteral("edge"), m_settings.position()},
         {QStringLiteral("alignment"), m_settings.alignment()},
         {QStringLiteral("type"), m_settings.bottomPanelType()},
         {QStringLiteral("visibilityMode"), m_settings.autoHide()
             ? QStringLiteral("auto-hide")
             : QStringLiteral("always")},
         {QStringLiteral("visible"), m_settings.bottomPanelVisible()}});
    m_panelRegistry.updatePanel(
        QStringLiteral("top"),
        {{QStringLiteral("type"), m_settings.topPanelType()},
         {QStringLiteral("visible"), m_settings.desktopSuite() && m_settings.topLauncherVisible()}});
    m_panelRegistry.updatePanel(
        QStringLiteral("side"),
        {{QStringLiteral("type"), m_settings.sidePanelType()},
         {QStringLiteral("visible"), m_settings.desktopSuite() && m_settings.sideRailVisible()}});

    for (const QString &panelId : m_panelRegistry.panelIds())
    {
        if (nativePanelId(panelId) >= 0 && !synchronizeNativePanelScreen(panelId))
        {
            qWarning() << "Could not update the native Plasma panel screen for" << panelId;
        }
    }
}

void PanelWindow::showSettings()
{
    if (!m_settingsWindow)
    {
        m_settingsWindow = createUtilityWindow(
            QUrl(QStringLiteral("qrc:/qt/qml/ArchDock/qml/runtime/SettingsPopup.qml")));
    }

    presentUtilityWindow(m_settingsWindow);
}

void PanelWindow::showPanelSettings(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    m_panelRegistry.setActivePanelId(panelId);
    if (!m_settingsWindow)
    {
        m_settingsWindow = createUtilityWindow(
            QUrl(QStringLiteral("qrc:/qt/qml/ArchDock/qml/runtime/SettingsPopup.qml")));
    }
    if (m_settingsWindow)
    {
        m_settingsWindow->setProperty("selectedPanelId", panelId);
        m_settingsWindow->setProperty("showPanels", true);
    }
    presentUtilityWindow(m_settingsWindow);
}

QString PanelWindow::createNativePanel(const QString &edge, const QString &type)
{
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
        return panelId;
    }

    m_panelRegistry.removePanel(panelId);
    return {};
}

QString PanelWindow::createFreePanel()
{
    const QString panelId = m_panelRegistry.addFreePanel();
    showPanelSettings(panelId);
    return panelId;
}

QString PanelWindow::createFreePanelFromTemplate(int containmentId,
                                                 const QString &ownershipToken)
{
    if (containmentId < 0 ||
        !ownershipToken.startsWith(QStringLiteral("archdock-free-template-")) ||
        ownershipToken.size() > 96)
    {
        qWarning() << "Rejected invalid free-panel template request";
        return {};
    }

    const QString token = plasmaScriptStringLiteral(ownershipToken);
    const QString verifyScript = QStringLiteral(R"JS(
const bridgePanel = panelById(%1);
let verified = 0;
if (bridgePanel) {
    const controls = bridgePanel.widgets("org.archdock.control");
    for (let index = 0; index < controls.length; ++index) {
        controls[index].currentConfigGroup = ["General"];
        if (controls[index].readConfig("bootstrapAction") === "create-circular-free-panel" &&
            controls[index].readConfig("bootstrapToken") === %2) {
            verified = 1;
            break;
        }
    }
}
print(verified);
)JS").arg(containmentId).arg(token);
    if (evaluatePlasmaScript(verifyScript) != 1)
    {
        qWarning() << "Rejected unverified free-panel template bridge" << containmentId;
        return {};
    }

    const QString panelId = m_panelRegistry.addFreePanel();

    const int removed = evaluatePlasmaScript(
        QStringLiteral("const bridgePanel = panelById(%1); "
                       "if (bridgePanel) { bridgePanel.remove(); print(1); } "
                       "else { print(0); }")
            .arg(containmentId));
    if (removed != 1)
    {
        qWarning() << "Free panel created, but its temporary Plasma bridge was not removed"
                   << containmentId;
    }
    showPanelSettings(panelId);
    return panelId;
}

void PanelWindow::saveFreePanelPosition(const QString &panelId, int x, int y)
{
    if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() != QStringLiteral("free"))
        return;
    m_panelRegistry.updatePanel(panelId, {
        {QStringLiteral("x"), qMax(0, x)},
        {QStringLiteral("y"), qMax(0, y)}});
}

bool PanelWindow::setNativePanelType(const QString &panelId, const QString &type)
{
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

bool PanelWindow::nativePanelExists(int panelId) const
{
    if (panelId < 0)
    {
        return false;
    }

    const int result = evaluatePlasmaScript(
        QStringLiteral("print(panelById(%1) ? 1 : 0);").arg(panelId));
    return result == 1;
}

bool PanelWindow::nativePanelIsOwned(const QString &panelId, int containmentId) const
{
    const QString token = nativeOwnershipToken(panelId);
    if (containmentId < 0 || token.isEmpty())
    {
        return false;
    }

    return evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print(0); }"
            "else {"
            "panel.currentConfigGroup = ['ArchDock'];"
            "print(panel.readConfig('ownerToken', '') === %2 && "
            "panel.readConfig('panelId', '') === %3 ? 1 : 0);"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(token))
            .arg(plasmaScriptStringLiteral(panelId))) == 1;
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

bool PanelWindow::synchronizeNativePanelScreen(const QString &panelId) const
{
    const int containmentId = nativePanelId(panelId);
    const int screenIndex = screenIndexForPanel(panelId);
    if (containmentId < 0 || screenIndex < 0)
    {
        return containmentId < 0;
    }
    if (!nativePanelIsOwned(panelId, containmentId))
    {
        return false;
    }

    return evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (!panel) { print(-1); }"
            "else { panel.screen = %2; print(1); }")
            .arg(containmentId)
            .arg(screenIndex)) == 1;
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
            "var dock = panel.addWidget('org.archdock.dock');"
            "if (!dock) { print(-1); }"
            "else {"
            "dock.currentConfigGroup = ['General'];"
            "dock.writeConfig('panelId', %2);"
            "dock.writeConfig('panelType', %3);"
            "dock.reloadConfig();"
            "print(dock.id);"
            "}"
            "}")
            .arg(containmentId)
            .arg(plasmaScriptStringLiteral(panelId))
            .arg(plasmaScriptStringLiteral(type)));
    if (dockId < 0)
    {
        qWarning() << "Could not attach the Arch Dock visual applet to panel" << panelId;
        return false;
    }

    m_panelRegistry.setPanelValue(panelId, QStringLiteral("nativeDockAppletId"), dockId);
    return true;
}

bool PanelWindow::createNativeKdePanel(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return false;
    }

    const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
    if (!isNativeDockPanelEdge(edge))
    {
        return false;
    }

    const int storedContainmentId = nativePanelId(panelId);
    const int storedControlId = nativeControlAppletId(panelId);
    const int storedDockId = nativeDockAppletId(panelId);
    const bool containmentExists = nativePanelExists(storedContainmentId);
    const ArchDock::NativeContainmentAssociation association =
        ArchDock::reconciledNativeContainmentAssociation(
            storedContainmentId,
            storedControlId,
            containmentExists,
            containmentExists && adoptNativePanelOwnership(panelId, storedContainmentId));
    if (association.containmentId != storedContainmentId ||
        association.controlAppletId != storedControlId)
    {
        m_panelRegistry.updatePanel(
            panelId,
            {{QStringLiteral("nativePanelId"), association.containmentId},
             {QStringLiteral("nativeControlAppletId"), association.controlAppletId},
             {QStringLiteral("nativeDockAppletId"), association.containmentId >= 0 ? storedDockId : -1},
             {QStringLiteral("nativeOwnershipToken"), association.containmentId >= 0
                 ? nativeOwnershipToken(panelId) : QString{}}});
    }
    if (association.containmentId >= 0)
    {
        return synchronizeNativePanelScreen(panelId) &&
            attachNativeDockApplet(panelId, association.containmentId);
    }

    const bool vertical = edge == QStringLiteral("left") || edge == QStringLiteral("right");
    const int thickness = qMax(
        24,
        m_panelRegistry.panelValue(
            panelId,
            vertical ? QStringLiteral("width") : QStringLiteral("height")).toInt());
    const int length = qMax(
        48,
        m_panelRegistry.panelValue(
            panelId,
            vertical ? QStringLiteral("height") : QStringLiteral("width")).toInt());
    const int offset = nativePanelOffset(panelId);
    const int screenIndex = screenIndexForPanel(panelId);
    if (screenIndex < 0)
    {
        return false;
    }
    const QString alignment = vertical ? QStringLiteral("top") : QStringLiteral("left");
    const QString ownershipToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString script = QStringLiteral(
        "var panel = new Panel;"
        "panel.location = '%1';"
        "panel.screen = %2;"
        "panel.height = %3;"
        "panel.alignment = '%4';"
        "panel.offset = %5;"
        "panel.minimumLength = %6;"
        "panel.maximumLength = %6;"
        "panel.currentConfigGroup = ['ArchDock'];"
        "panel.writeConfig('ownerToken', %7);"
        "panel.writeConfig('panelId', %8);"
        "panel.reloadConfig();"
        "if (panel.readConfig('ownerToken', '') !== %7 || panel.readConfig('panelId', '') !== %8) "
        "{ panel.remove(); print(-1); } else { print(panel.id); }")
        .arg(edge)
        .arg(screenIndex)
        .arg(thickness)
        .arg(alignment)
        .arg(offset)
        .arg(length)
        .arg(plasmaScriptStringLiteral(ownershipToken))
        .arg(plasmaScriptStringLiteral(panelId));
    const int createdId = evaluatePlasmaScript(script);
    if (createdId < 0)
    {
        return false;
    }

    m_panelRegistry.updatePanel(
        panelId,
        {{QStringLiteral("nativePanelId"), createdId},
         {QStringLiteral("nativeControlAppletId"), -1},
            {QStringLiteral("nativeDockAppletId"), -1},
         {QStringLiteral("nativeOwnershipToken"), ownershipToken}});
        const bool attached = attachNativeDockApplet(panelId, createdId);
    if (attached && !m_nativePanelRecoveryActive)
    {
        scheduleNativePanelRecovery();
    }
    return attached;
}

bool PanelWindow::addKdeWidget(const QString &panelId, const QString &appletId)
{
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
    const int containmentId = nativePanelId(panelId);
    if (containmentId < 0)
    {
        return true;
    }

    if (!adoptNativePanelOwnership(panelId, containmentId))
    {
        m_panelRegistry.updatePanel(
            panelId,
            {{QStringLiteral("nativePanelId"), -1},
             {QStringLiteral("nativeControlAppletId"), -1},
             {QStringLiteral("nativeDockAppletId"), -1},
             {QStringLiteral("nativeOwnershipToken"), QString{}}});
        return true;
    }

    const int removed = evaluatePlasmaScript(
        QStringLiteral(
            "var panel = panelById(%1);"
            "if (panel) { panel.remove(); print(1); } else { print(1); }")
            .arg(containmentId));
    if (removed != 1)
    {
        return false;
    }

    m_panelRegistry.updatePanel(
        panelId,
        {{QStringLiteral("nativePanelId"), -1},
         {QStringLiteral("nativeControlAppletId"), -1},
         {QStringLiteral("nativeDockAppletId"), -1},
         {QStringLiteral("nativeOwnershipToken"), QString{}}});
    return true;
}

void PanelWindow::removePanel(const QString &panelId)
{
    if (!m_panelRegistry.panelIds().contains(panelId))
    {
        return;
    }

    if (!m_panelRegistry.isBuiltIn(panelId))
    {
        if (m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString() == QStringLiteral("free"))
        {
            delete m_freePanelWindows.take(panelId);
        }
        else
        {
            removeNativeKdePanel(panelId);
        }
    }
    m_panelRegistry.removePanel(panelId);
    updateDesktopSuite();
}

bool PanelWindow::openKdeWidgetPreview(const QString &panelId, const QString &appletId)
{
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

    const QString viewer = QStandardPaths::findExecutable(QStringLiteral("plasmoidviewer"));
    if (viewer.isEmpty())
    {
        return false;
    }

    QStringList widgets = m_panelRegistry.panelValue(panelId, QStringLiteral("kdeWidgets")).toStringList();
    if (!widgets.contains(pluginId))
    {
        widgets.append(pluginId);
        m_panelRegistry.setPanelValue(panelId, QStringLiteral("kdeWidgets"), widgets);
    }

    const QString edge = m_panelRegistry.panelValue(panelId, QStringLiteral("edge")).toString();
    const QString location = edge == QStringLiteral("top") ? QStringLiteral("topedge")
        : edge == QStringLiteral("bottom") ? QStringLiteral("bottomedge")
        : edge == QStringLiteral("left") ? QStringLiteral("leftedge")
        : edge == QStringLiteral("right") ? QStringLiteral("rightedge")
        : QStringLiteral("bottomedge");
    const QString formFactor = edge == QStringLiteral("left") || edge == QStringLiteral("right")
        ? QStringLiteral("vertical")
        : QStringLiteral("horizontal");
    const QSize size(
        m_panelRegistry.panelValue(panelId, QStringLiteral("width")).toInt(),
        m_panelRegistry.panelValue(panelId, QStringLiteral("height")).toInt());

    return QProcess::startDetached(
        viewer,
        {QStringLiteral("--applet"), pluginId,
         QStringLiteral("--formfactor"), formFactor,
         QStringLiteral("--location"), location,
         QStringLiteral("--xPosition"), m_panelRegistry.panelValue(panelId, QStringLiteral("x")).toString(),
         QStringLiteral("--yPosition"), m_panelRegistry.panelValue(panelId, QStringLiteral("y")).toString(),
         QStringLiteral("--size"), QStringLiteral("%1x%2").arg(size.width()).arg(size.height())});
}

void PanelWindow::showIconProperties(int row)
{
    const QModelIndex index = m_dockModel.index(row, 0);
    if (!index.isValid())
    {
        return;
    }

    if (!m_iconPropertiesWindow)
    {
        m_iconPropertiesWindow = createUtilityWindow(
            QUrl(QStringLiteral("qrc:/qt/qml/ArchDock/qml/runtime/IconPropertiesWindow.qml")));
    }

    if (!m_iconPropertiesWindow)
    {
        return;
    }

    m_iconPropertiesWindow->setProperty("targetRow", row);
    m_iconPropertiesWindow->setProperty(
        "appName",
        m_dockModel.data(index, DockModel::DisplayNameRole));
    m_iconPropertiesWindow->setProperty(
        "currentIconName",
        m_dockModel.data(index, DockModel::IconNameRole));
    presentUtilityWindow(m_iconPropertiesWindow);
}

QWindow *PanelWindow::createUtilityWindow(const QUrl &source)
{
    QQmlComponent component(&m_engine, source, this);
    if (component.status() != QQmlComponent::Ready)
    {
        qWarning().noquote() << component.errorString();
        return nullptr;
    }

    QObject *rootObject = component.create(m_engine.rootContext());
    auto *window = qobject_cast<QWindow *>(rootObject);
    if (window)
    {
        return window;
    }

    qWarning() << "Unable to create Arch Dock utility window:" << source;
    delete rootObject;
    return nullptr;
}

void PanelWindow::presentUtilityWindow(QWindow *window)
{
    if (!window)
    {
        return;
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen)
    {
        window->setScreen(screen);
        const QRect geometry = screen->availableGeometry();
        window->setPosition(
            geometry.x() + (geometry.width() - window->width()) / 2,
            geometry.y() + (geometry.height() - window->height()) / 2);
    }

    window->show();
    window->raise();
    window->requestActivate();

    const QString title = window->title();
    QTimer::singleShot(
        100,
        this,
        [this, title]
        {
            m_actionBridge.focusWindow(QStringLiteral("arch-dock"), title);
        });
}

void PanelWindow::resetSettings()
{
    m_settings.reset();
    syncRegistryFromLegacySettings();
}

void PanelWindow::toggleAutoHide()
{
    const bool autoHide = m_panelRegistry.panelValue(
        QStringLiteral("bottom"),
        QStringLiteral("visibilityMode")).toString() == QStringLiteral("auto-hide");
    setPanelVisibilityMode(
        QStringLiteral("bottom"),
        autoHide ? QStringLiteral("always") : QStringLiteral("auto-hide"));
}

void PanelWindow::toggleDesktopSuite()
{
    m_settings.setDesktopSuite(!m_settings.desktopSuite());
    syncRegistryFromLegacySettings();
}

void PanelWindow::toggleTopLauncher()
{
    m_settings.setTopLauncherVisible(!m_settings.topLauncherVisible());
    m_panelRegistry.setPanelValue(
        QStringLiteral("top"),
        QStringLiteral("visible"),
        m_settings.desktopSuite() && m_settings.topLauncherVisible());
}

void PanelWindow::toggleSideRail()
{
    m_settings.setSideRailVisible(!m_settings.sideRailVisible());
    m_panelRegistry.setPanelValue(
        QStringLiteral("side"),
        QStringLiteral("visible"),
        m_settings.desktopSuite() && m_settings.sideRailVisible());
}

void PanelWindow::toggleBottomPanel()
{
    const bool visible = !m_panelRegistry.panelValue(
        QStringLiteral("bottom"), QStringLiteral("visible")).toBool();
    m_panelRegistry.setPanelValue(QStringLiteral("bottom"), QStringLiteral("visible"), visible);
    if (m_settings.bottomPanelVisible() != visible)
    {
        m_settings.setBottomPanelVisible(visible);
    }
}

void PanelWindow::applyProfile(const QString &profileName)
{
    m_settings.applyProfile(profileName);
    syncRegistryFromLegacySettings();
}

void PanelWindow::openSystemSettings(const QString &module)
{
    QStringList arguments;
    if (module == QStringLiteral("audio"))
    {
        arguments.append(QStringLiteral("kcm_pulseaudio"));
    }
    else if (module == QStringLiteral("network"))
    {
        arguments.append(QStringLiteral("kcm_networkmanagement"));
    }
    else if (module == QStringLiteral("bluetooth"))
    {
        arguments.append(QStringLiteral("kcm_bluetooth"));
    }
    else if (module == QStringLiteral("keyboard"))
    {
        arguments.append(QStringLiteral("kcm_keyboard"));
    }
    else if (module == QStringLiteral("power"))
    {
        arguments.append(QStringLiteral("kcm_powerdevilprofilesconfig"));
    }

    QProcess::startDetached(QStringLiteral("systemsettings"), arguments);
}
