#include "DockModel.h"

#include "WindowModel.h"
#include "model/IconEntryIdentity.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>

namespace
{
    QString desktopFilePath(const QString &desktopFileName)
    {
        if (desktopFileName.isEmpty())
        {
            return {};
        }

        if (QFileInfo::exists(desktopFileName))
        {
            return desktopFileName;
        }

        return QStandardPaths::locate(
            QStandardPaths::GenericDataLocation,
            QStringLiteral("applications/") + desktopFileName);
    }

}

DockModel::DockModel(WindowModel &windowModel, QObject *parent)
    : QAbstractListModel(parent), m_windowModel(windowModel)
{
    loadPinnedApplications();

    const auto refresh = [this]
    {
        rebuild();
    };

    connect(&m_windowModel, &QAbstractItemModel::modelReset, this, refresh);
    connect(&m_windowModel, &QAbstractItemModel::rowsInserted, this, refresh);
    connect(&m_windowModel, &QAbstractItemModel::rowsRemoved, this, refresh);
    connect(&m_windowModel, &QAbstractItemModel::dataChanged, this, refresh);

    rebuild();
}

int DockModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant DockModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
    {
        return {};
    }

    const DockApplication &application = m_items.at(index.row());
    const bool running = !application.windows.isEmpty();
    const bool active = std::any_of(
        application.windows.cbegin(),
        application.windows.cend(),
        [](const WindowItem &window)
        {
            return window.active;
        });
    const bool minimized = running && std::all_of(
        application.windows.cbegin(),
        application.windows.cend(),
        [](const WindowItem &window)
        {
            return window.minimized;
        });

    switch (role)
    {
    case AppIdRole:
        return application.appId;
    case StableIdentityRole:
        return ArchDock::IconEntryIdentity::forApplication(
            application.appId, application.desktopFileName);
    case DesktopFileNameRole:
        return application.desktopFileName;
    case BaseIconNameRole:
        return application.iconName;
    case IconNameRole:
        return application.iconName;
    case BaseDisplayNameRole:
        return application.displayName;
    case DisplayNameRole:
        return application.displayName;
    case PinnedRole:
        return application.pinned;
    case RunningRole:
        return running;
    case ActiveRole:
        return active;
    case MinimizedRole:
        return minimized;
    case WindowCountRole:
        return application.windows.size();
    case WindowIdsRole:
    {
        QStringList ids;
        ids.reserve(application.windows.size());
        for (const WindowItem &window : application.windows)
        {
            ids.append(window.internalId);
        }
        return ids;
    }
    case WindowTitlesRole:
    {
        QStringList titles;
        titles.reserve(application.windows.size());
        for (const WindowItem &window : application.windows)
        {
            titles.append(window.caption.isEmpty() ? application.displayName : window.caption);
        }
        return titles;
    }
    case FolderRole:
        return !folderPath(application).isEmpty();
    default:
        return {};
    }
}

QHash<int, QByteArray> DockModel::roleNames() const
{
    return {
        {AppIdRole, "appId"},
        {StableIdentityRole, "stableIdentity"},
        {DesktopFileNameRole, "desktopFileName"},
        {BaseIconNameRole, "baseIconName"},
        {IconNameRole, "iconName"},
        {BaseDisplayNameRole, "baseDisplayName"},
        {DisplayNameRole, "displayName"},
        {PinnedRole, "pinned"},
        {RunningRole, "running"},
        {ActiveRole, "active"},
        {MinimizedRole, "minimized"},
        {WindowCountRole, "windowCount"},
        {WindowIdsRole, "windowIds"},
        {WindowTitlesRole, "windowTitles"},
        {FolderRole, "isFolder"}};
}

void DockModel::activate(int row)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }

    const DockApplication &application = m_items.at(row);
    if (application.windows.isEmpty())
    {
        launch(row);
        return;
    }

    const int windowIndex = preferredWindowIndex(application);
    const WindowItem &window = application.windows.at(windowIndex);
    requestAction(window.internalId, window.active && !window.minimized
                                        ? QStringLiteral("minimize")
                                        : QStringLiteral("activate"));
}

void DockModel::activateWindow(int row, int windowIndex)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }

    const DockApplication &application = m_items.at(row);
    if (windowIndex < 0 || windowIndex >= application.windows.size())
    {
        return;
    }

    requestAction(application.windows.at(windowIndex).internalId,
                  QStringLiteral("activate"));
}

void DockModel::toggleMinimized(int row)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }

    const DockApplication &application = m_items.at(row);
    if (application.windows.isEmpty())
    {
        return;
    }

    const int windowIndex = preferredWindowIndex(application);
    const WindowItem &window = application.windows.at(windowIndex);
    requestAction(window.internalId,
                  window.minimized ? QStringLiteral("activate")
                                   : QStringLiteral("minimize"));
}

bool DockModel::launch(int row)
{
    if (row < 0 || row >= m_items.size())
    {
        return false;
    }

    const DockApplication &application = m_items.at(row);
    const QStringList arguments = launchArguments(application.launchCommand);
    if (arguments.isEmpty())
    {
        return false;
    }

    QStringList launchArguments = arguments;
    const QString program = launchArguments.takeFirst();
    return QProcess::startDetached(program, launchArguments);
}

void DockModel::close(int row, int windowIndex)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }

    const DockApplication &application = m_items.at(row);
    if (application.windows.isEmpty())
    {
        return;
    }

    if (windowIndex < 0 || windowIndex >= application.windows.size())
    {
        windowIndex = preferredWindowIndex(application);
    }

    requestAction(application.windows.at(windowIndex).internalId,
                  QStringLiteral("close"));
}

void DockModel::closeAll(int row)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }

    for (const WindowItem &window : m_items.at(row).windows)
    {
        requestAction(window.internalId, QStringLiteral("close"));
    }
}

void DockModel::togglePinned(int row)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }

    if (m_items.at(row).pinned)
    {
        unpin(row);
    }
    else
    {
        pin(row);
    }
}

void DockModel::pin(int row)
{
    if (row < 0 || row >= m_items.size() || m_items.at(row).pinned)
    {
        return;
    }

    const DockApplication &application = m_items.at(row);
    m_pinnedApplications.append(
        {application.appId,
         application.desktopFileName,
         application.defaultIconName.isEmpty()
             ? application.iconName
             : application.defaultIconName,
         application.displayName,
         application.launchCommand});
    if (!m_order.contains(application.appId))
    {
        m_order.append(application.appId);
    }

    savePinnedApplications();
    rebuild();
}

void DockModel::unpin(int row)
{
    if (row < 0 || row >= m_items.size())
    {
        return;
    }

    const QString appId = m_items.at(row).appId;
    for (int index = 0; index < m_pinnedApplications.size(); ++index)
    {
        if (m_pinnedApplications.at(index).appId == appId)
        {
            m_pinnedApplications.removeAt(index);
            break;
        }
    }

    savePinnedApplications();
    rebuild();
}

bool DockModel::pinUrl(const QUrl &url)
{
    if (!url.isLocalFile())
    {
        return false;
    }

    const QFileInfo fileInfo(url.toLocalFile());
    if (!fileInfo.exists())
    {
        return false;
    }

    PinnedApplication application;
    if (fileInfo.suffix().compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0)
    {
        QSettings desktopEntry(fileInfo.absoluteFilePath(), QSettings::IniFormat);
        desktopEntry.beginGroup(QStringLiteral("Desktop Entry"));
        application.appId = normalizedDesktopId(fileInfo.absoluteFilePath());
        application.desktopFileName = fileInfo.absoluteFilePath();
        application.iconName = desktopEntry.value(QStringLiteral("Icon")).toString();
        application.displayName = desktopEntry.value(QStringLiteral("Name"), fileInfo.completeBaseName()).toString();
        application.launchCommand = desktopEntry.value(QStringLiteral("Exec")).toString();
        desktopEntry.endGroup();
    }
    else
    {
        application.appId = QStringLiteral("file:") + fileInfo.canonicalFilePath();
        application.displayName = fileInfo.fileName();
        application.launchCommand = QStringLiteral("xdg-open ") + url.toString(QUrl::FullyEncoded);
        if (fileInfo.isDir())
        {
            application.iconName = QStringLiteral("folder");
        }
        else
        {
            QMimeDatabase mimeDatabase;
            application.iconName = mimeDatabase.mimeTypeForFile(fileInfo).iconName();
        }
    }

    if (application.appId.isEmpty() || application.launchCommand.isEmpty())
    {
        return false;
    }

    for (const PinnedApplication &existing : std::as_const(m_pinnedApplications))
    {
        if (existing.appId == application.appId)
        {
            return true;
        }
    }

    m_pinnedApplications.append(application);
    if (!m_order.contains(application.appId))
    {
        m_order.append(application.appId);
    }
    savePinnedApplications();
    rebuild();
    return true;
}

QString DockModel::applicationIdForUrl(const QUrl &url) const
{
    if (!url.isLocalFile())
    {
        return {};
    }
    const QFileInfo fileInfo(url.toLocalFile());
    if (!fileInfo.exists())
    {
        return {};
    }
    if (fileInfo.suffix().compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0)
    {
        return normalizedDesktopId(fileInfo.absoluteFilePath());
    }
    return QStringLiteral("file:") + fileInfo.canonicalFilePath();
}

QUrl DockModel::urlForApplicationId(const QString &appId) const
{
    if (appId.startsWith(QStringLiteral("file:")))
    {
        return QUrl::fromLocalFile(appId.mid(5));
    }
    for (const PinnedApplication &application : m_pinnedApplications)
    {
        if (application.appId == appId)
        {
            const QString path = desktopFilePath(application.desktopFileName);
            return path.isEmpty() ? QUrl{} : QUrl::fromLocalFile(path);
        }
    }
    return {};
}

void DockModel::removePinnedApplications(const QStringList &appIds)
{
    const QSet<QString> removed(appIds.cbegin(), appIds.cend());
    const qsizetype oldSize = m_pinnedApplications.size();
    m_pinnedApplications.removeIf(
        [&removed](const PinnedApplication &application)
        {
            return removed.contains(application.appId);
        });
    if (m_pinnedApplications.size() == oldSize)
    {
        return;
    }
    for (const QString &appId : removed)
    {
        m_order.removeAll(appId);
    }
    savePinnedApplications();
    rebuild();
}

bool DockModel::isFolder(int row) const
{
    return row >= 0 && row < m_items.size() && !folderPath(m_items.at(row)).isEmpty();
}

QVariantList DockModel::folderEntries(int row) const
{
    if (row < 0 || row >= m_items.size())
    {
        return {};
    }

    const QString path = folderPath(m_items.at(row));
    if (path.isEmpty())
    {
        return {};
    }

    const QDir directory(path);
    const QFileInfoList entries = directory.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Readable,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    QMimeDatabase mimeDatabase;
    QVariantList result;
    result.reserve(qMin(entries.size(), 48));
    for (const QFileInfo &entry : entries)
    {
        if (result.size() == 48)
        {
            break;
        }

        QVariantMap item;
        item.insert(QStringLiteral("url"), QUrl::fromLocalFile(entry.absoluteFilePath()));
        item.insert(QStringLiteral("name"), entry.fileName());
        item.insert(QStringLiteral("isDirectory"), entry.isDir());
        item.insert(
            QStringLiteral("iconName"),
            entry.isDir()
                ? QStringLiteral("folder")
                : mimeDatabase.mimeTypeForFile(entry).iconName());
        result.append(item);
    }
    return result;
}

bool DockModel::openUrl(const QUrl &url) const
{
    return url.isValid() && QDesktopServices::openUrl(url);
}

bool DockModel::panelEntryMatches(int row, const QString &panelType) const
{
    return row >= 0 && row < m_items.size() && matchesPanelType(m_items.at(row), panelType);
}

int DockModel::panelEntryPosition(int row, const QString &panelType) const
{
    if (row < 0 || row >= m_items.size() || !matchesPanelType(m_items.at(row), panelType))
    {
        return -1;
    }

    int position = 0;
    for (int index = 0; index < row; ++index)
    {
        if (matchesPanelType(m_items.at(index), panelType))
        {
            ++position;
        }
    }
    return position;
}

int DockModel::panelEntryCount(const QString &panelType) const
{
    int count = 0;
    for (const DockApplication &application : m_items)
    {
        if (matchesPanelType(application, panelType))
        {
            ++count;
        }
    }
    return count;
}

QVariantList DockModel::panelEntries(const QString &panelType) const
{
    QVariantList entries;
    for (const DockApplication &application : m_items)
    {
        if (!matchesPanelType(application, panelType))
        {
            continue;
        }

        const bool running = !application.windows.isEmpty();
        const bool active = std::any_of(
            application.windows.cbegin(),
            application.windows.cend(),
            [](const WindowItem &window)
            {
                return window.active;
            });
        const bool minimized = running && std::all_of(
            application.windows.cbegin(),
            application.windows.cend(),
            [](const WindowItem &window)
            {
                return window.minimized;
            });

        QStringList windowIds;
        QStringList windowTitles;
        windowIds.reserve(application.windows.size());
        windowTitles.reserve(application.windows.size());
        for (const WindowItem &window : application.windows)
        {
            windowIds.append(window.internalId);
            windowTitles.append(window.caption.isEmpty() ? application.displayName : window.caption);
        }

        entries.append(QVariantMap{
            {QStringLiteral("appId"), application.appId},
            {QStringLiteral("stableIdentity"),
             ArchDock::IconEntryIdentity::forApplication(
                 application.appId, application.desktopFileName)},
            {QStringLiteral("desktopFileName"), application.desktopFileName},
            {QStringLiteral("baseIconName"), application.iconName},
            {QStringLiteral("iconName"), application.iconName},
            {QStringLiteral("baseDisplayName"), application.displayName},
            {QStringLiteral("displayName"), application.displayName},
            {QStringLiteral("pinned"), application.pinned},
            {QStringLiteral("running"), running},
            {QStringLiteral("active"), active},
            {QStringLiteral("minimized"), minimized},
            {QStringLiteral("windowCount"), application.windows.size()},
            {QStringLiteral("windowIds"), windowIds},
            {QStringLiteral("windowTitles"), windowTitles},
            {QStringLiteral("isFolder"), !folderPath(application).isEmpty()}});
    }
    return entries;
}

QVariantList DockModel::panelEntriesForIds(const QStringList &appIds) const
{
    const QSet<QString> requested(appIds.cbegin(), appIds.cend());
    QVariantList entries;
    for (const QVariant &entry : panelEntries(QStringLiteral("launcher")))
    {
        if (requested.contains(entry.toMap().value(QStringLiteral("appId")).toString()))
        {
            entries.append(entry);
        }
    }
    return entries;
}

bool DockModel::activateApplication(const QString &appId)
{
    const int row = indexForApplication(appId);
    if (row < 0)
    {
        return false;
    }

    if (m_items.at(row).windows.isEmpty())
    {
        return launch(row);
    }
    activate(row);
    return true;
}

bool DockModel::activateApplicationWindow(const QString &appId, const QString &windowId)
{
    const int row = indexForApplication(appId);
    if (row < 0 || windowId.isEmpty())
    {
        return false;
    }

    const DockApplication &application = m_items.at(row);
    for (const WindowItem &window : application.windows)
    {
        if (window.internalId == windowId)
        {
            requestAction(windowId, QStringLiteral("activate"));
            return true;
        }
    }
    return false;
}

bool DockModel::minimizeApplication(const QString &appId)
{
    const int row = indexForApplication(appId);
    if (row < 0 || m_items.at(row).windows.isEmpty())
    {
        return false;
    }

    toggleMinimized(row);
    return true;
}

bool DockModel::closeApplication(const QString &appId)
{
    const int row = indexForApplication(appId);
    if (row < 0 || m_items.at(row).windows.isEmpty())
    {
        return false;
    }

    close(row);
    return true;
}

bool DockModel::closeAllApplication(const QString &appId)
{
    const int row = indexForApplication(appId);
    if (row < 0 || m_items.at(row).windows.isEmpty())
    {
        return false;
    }

    closeAll(row);
    return true;
}

bool DockModel::togglePinnedApplication(const QString &appId)
{
    const int row = indexForApplication(appId);
    if (row < 0)
    {
        return false;
    }

    togglePinned(row);
    return true;
}

bool DockModel::moveApplicationBefore(const QString &appId, const QString &beforeAppId)
{
    const int from = indexForApplication(appId);
    if (from < 0)
    {
        return false;
    }

    int to = beforeAppId.isEmpty() ? m_items.size() - 1 : indexForApplication(beforeAppId);
    if (to < 0)
    {
        return false;
    }

    if (!beforeAppId.isEmpty() && from < to)
    {
        --to;
    }

    move(from, to);
    return true;
}

QVariantList DockModel::folderEntriesForApplication(const QString &appId) const
{
    return folderEntries(indexForApplication(appId));
}

void DockModel::move(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_items.size() || to >= m_items.size() || from == to)
    {
        return;
    }

    QStringList order;
    order.reserve(m_items.size());
    for (const DockApplication &application : m_items)
    {
        order.append(application.appId);
    }

    order.move(from, to);
    for (const QString &appId : std::as_const(m_order))
    {
        if (!order.contains(appId))
        {
            order.append(appId);
        }
    }

    m_order = order;
    savePinnedApplications();
    rebuild();
}

QString DockModel::applicationIdForWindow(const WindowItem &window)
{
    const QString desktopId = normalizedDesktopId(window.desktopFileName);
    if (!desktopId.isEmpty())
    {
        return desktopId;
    }

    const QString resourceClass = window.resourceClass.trimmed().toLower();
    const QString mappedDesktopId = desktopIdForResourceClass(resourceClass);
    if (!mappedDesktopId.isEmpty())
    {
        return mappedDesktopId;
    }

    if (!resourceClass.isEmpty())
    {
        return resourceClass;
    }

    return window.resourceName.trimmed().toLower();
}

QString DockModel::folderPath(const DockApplication &application)
{
    static const QString prefix = QStringLiteral("file:");
    if (!application.appId.startsWith(prefix))
    {
        return {};
    }

    const QFileInfo fileInfo(application.appId.mid(prefix.size()));
    if (!fileInfo.isDir())
    {
        return {};
    }
    return fileInfo.canonicalFilePath();
}

QString DockModel::normalizedDesktopId(const QString &identifier)
{
    QString desktopId = QFileInfo(identifier.trimmed()).fileName();
    if (desktopId.isEmpty())
    {
        return {};
    }

    if (!desktopId.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive))
    {
        desktopId += QStringLiteral(".desktop");
    }

    return desktopId.toLower();
}

QString DockModel::desktopIdForResourceClass(const QString &resourceClass)
{
    const QString normalizedResourceClass = resourceClass.trimmed().toLower();
    if (normalizedResourceClass.isEmpty())
    {
        return {};
    }

    static QHash<QString, QString> resourceClassDesktopIds;
    const auto cached = resourceClassDesktopIds.constFind(normalizedResourceClass);
    if (cached != resourceClassDesktopIds.cend())
    {
        return cached.value();
    }

    const QStringList applicationDirectories = QStandardPaths::locateAll(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("applications"),
        QStandardPaths::LocateDirectory);
    for (const QString &applicationDirectory : applicationDirectories)
    {
        QDirIterator entries(
            applicationDirectory,
            {QStringLiteral("*.desktop")},
            QDir::Files,
            QDirIterator::Subdirectories);
        while (entries.hasNext())
        {
            const QString desktopFilePath = entries.next();
            QSettings desktopEntry(desktopFilePath, QSettings::IniFormat);
            desktopEntry.beginGroup(QStringLiteral("Desktop Entry"));
            const QString startupWmClass = desktopEntry.value(
                QStringLiteral("StartupWMClass")).toString().trimmed().toLower();
            desktopEntry.endGroup();

            if (startupWmClass != normalizedResourceClass)
            {
                continue;
            }

            const QString desktopId = normalizedDesktopId(desktopFilePath);
            resourceClassDesktopIds.insert(normalizedResourceClass, desktopId);
            return desktopId;
        }
    }

    resourceClassDesktopIds.insert(normalizedResourceClass, {});
    return {};
}

void DockModel::hydrateDesktopEntry(DockApplication &application)
{
    if (application.desktopFileName.isEmpty() &&
        application.appId.endsWith(QStringLiteral(".desktop")))
    {
        application.desktopFileName = application.appId;
    }

    const QString path = desktopFilePath(application.desktopFileName);
    if (path.isEmpty())
    {
        return;
    }

    QSettings desktopEntry(path, QSettings::IniFormat);
    desktopEntry.beginGroup(QStringLiteral("Desktop Entry"));
    const QString desktopEntryName = desktopEntry.value(QStringLiteral("Name")).toString();
    if (!desktopEntryName.isEmpty())
    {
        application.displayName = desktopEntryName;
    }
    if (application.defaultIconName.isEmpty())
    {
        application.defaultIconName = desktopEntry.value(QStringLiteral("Icon")).toString();
    }
    if (application.iconName.isEmpty())
    {
        application.iconName = application.defaultIconName;
    }
    if (application.launchCommand.isEmpty())
    {
        application.launchCommand = desktopEntry.value(QStringLiteral("Exec")).toString();
    }
    desktopEntry.endGroup();
}

QStringList DockModel::launchArguments(const QString &command)
{
    QStringList arguments = QProcess::splitCommand(command);
    for (auto iterator = arguments.begin(); iterator != arguments.end();)
    {
        if (iterator->startsWith(QLatin1Char('%')))
        {
            iterator = arguments.erase(iterator);
        }
        else
        {
            iterator->replace(QStringLiteral("%%"), QStringLiteral("%"));
            ++iterator;
        }
    }
    return arguments;
}

void DockModel::loadPinnedApplications()
{
    QSettings settings;
    m_order = settings.value(QStringLiteral("dock/order")).toStringList();

    const QJsonDocument customIconsDocument = QJsonDocument::fromJson(
        settings.value(QStringLiteral("dock/customIcons")).toByteArray());
    if (customIconsDocument.isObject())
    {
        const QJsonObject customIcons = customIconsDocument.object();
        for (auto iterator = customIcons.constBegin(); iterator != customIcons.constEnd(); ++iterator)
        {
            const QString iconName = iterator.value().toString().trimmed();
            if (!iconName.isEmpty())
            {
                m_legacyCustomIcons.insert(iterator.key(), iconName);
            }
        }
    }

    const QJsonDocument document = QJsonDocument::fromJson(
        settings.value(QStringLiteral("dock/pinnedApplications")).toByteArray());
    if (!document.isArray())
    {
        return;
    }

    for (const QJsonValue &value : document.array())
    {
        const QJsonObject object = value.toObject();
        const QString appId = object.value(QStringLiteral("appId")).toString();
        if (appId.isEmpty())
        {
            continue;
        }

        m_pinnedApplications.append(
            {appId,
             object.value(QStringLiteral("desktopFileName")).toString(),
             object.value(QStringLiteral("iconName")).toString(),
             object.value(QStringLiteral("displayName")).toString(),
             object.value(QStringLiteral("launchCommand")).toString()});
    }
}

void DockModel::savePinnedApplications() const
{
    QJsonArray applications;
    for (const PinnedApplication &application : m_pinnedApplications)
    {
        applications.append(
            QJsonObject{
                {QStringLiteral("appId"), application.appId},
                {QStringLiteral("desktopFileName"), application.desktopFileName},
                {QStringLiteral("iconName"), application.iconName},
                {QStringLiteral("displayName"), application.displayName},
                {QStringLiteral("launchCommand"), application.launchCommand}});
    }

    QSettings settings;
    settings.setValue(
        QStringLiteral("dock/pinnedApplications"),
        QJsonDocument(applications).toJson(QJsonDocument::Compact));
    settings.setValue(QStringLiteral("dock/order"), m_order);
    settings.sync();
}

void DockModel::rebuild()
{
    QHash<QString, DockApplication> applications;
    for (const WindowItem &window : m_windowModel.windows())
    {
        const QString appId = applicationIdForWindow(window);
        if (appId.isEmpty())
        {
            continue;
        }

        DockApplication &application = applications[appId];
        if (application.appId.isEmpty())
        {
            application.appId = appId;
            application.desktopFileName = normalizedDesktopId(window.desktopFileName);
            application.defaultIconName = window.iconName;
            application.iconName = window.iconName;
            application.displayName = window.caption;
            hydrateDesktopEntry(application);
        }
        application.windows.append(window);
    }

    for (const PinnedApplication &pinnedApplication : m_pinnedApplications)
    {
        DockApplication &application = applications[pinnedApplication.appId];
        if (application.appId.isEmpty())
        {
            application.appId = pinnedApplication.appId;
            application.desktopFileName = pinnedApplication.desktopFileName;
            application.defaultIconName = pinnedApplication.iconName;
            application.iconName = pinnedApplication.iconName;
            application.displayName = pinnedApplication.displayName;
            application.launchCommand = pinnedApplication.launchCommand;
            hydrateDesktopEntry(application);
        }
        application.pinned = true;
    }

    for (auto iterator = applications.begin(); iterator != applications.end(); ++iterator)
    {
        const auto customIcon = m_legacyCustomIcons.constFind(iterator.key());
        if (customIcon != m_legacyCustomIcons.cend())
        {
            iterator->iconName = customIcon.value();
        }
    }

    QStringList orderedIds = m_order;
    for (const PinnedApplication &application : m_pinnedApplications)
    {
        if (!orderedIds.contains(application.appId))
        {
            orderedIds.append(application.appId);
        }
    }
    for (auto iterator = applications.cbegin(); iterator != applications.cend(); ++iterator)
    {
        if (!orderedIds.contains(iterator.key()))
        {
            orderedIds.append(iterator.key());
        }
    }

    QList<DockApplication> items;
    items.reserve(applications.size());
    for (const QString &appId : std::as_const(orderedIds))
    {
        const auto iterator = applications.constFind(appId);
        if (iterator != applications.cend())
        {
            items.append(iterator.value());
        }
    }

    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    emit countChanged();
}

void DockModel::requestAction(const QString &internalId, const QString &action)
{
    if (!internalId.isEmpty())
    {
        emit windowActionRequested(internalId, action);
    }
}

int DockModel::preferredWindowIndex(const DockApplication &application)
{
    if (application.windows.size() < 2)
    {
        return 0;
    }

    for (int index = 0; index < application.windows.size(); ++index)
    {
        if (application.windows.at(index).active)
        {
            return index;
        }
    }

    const int next = m_nextWindowByApplication.value(application.appId, 0) % application.windows.size();
    m_nextWindowByApplication.insert(application.appId, (next + 1) % application.windows.size());
    return next;
}

int DockModel::indexForApplication(const QString &appId) const
{
    for (int index = 0; index < m_items.size(); ++index)
    {
        if (m_items.at(index).appId == appId)
        {
            return index;
        }
    }
    return -1;
}

bool DockModel::matchesPanelType(const DockApplication &application, const QString &panelType)
{
    const QString normalized = panelType.trimmed().toLower();
    const bool running = !application.windows.isEmpty();
    if (normalized == QStringLiteral("empty"))
    {
        return false;
    }
    if (normalized == QStringLiteral("launcher"))
    {
        return application.pinned;
    }
    if (normalized == QStringLiteral("tasks"))
    {
        return running;
    }
    return application.pinned || running;
}
