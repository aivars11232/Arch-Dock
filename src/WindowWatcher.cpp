#include "WindowWatcher.h"

#include "WindowModel.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace
{
    QString resolveIconName(const QString &desktopFileName,
                            const QString &resourceClass)
    {
        const QStringList identifiers = {
            desktopFileName.trimmed(),
            resourceClass.trimmed()};

        for (QString identifier : identifiers)
        {
            if (identifier.isEmpty())
            {
                continue;
            }

            QString desktopFilePath;

            if (QFileInfo::exists(identifier))
            {
                desktopFilePath = identifier;
            }
            else
            {
                if (!identifier.endsWith(QStringLiteral(".desktop")))
                {
                    identifier += QStringLiteral(".desktop");
                }

                desktopFilePath = QStandardPaths::locate(
                    QStandardPaths::GenericDataLocation,
                    QStringLiteral("applications/") + identifier);
            }

            if (desktopFilePath.isEmpty())
            {
                continue;
            }

            QSettings desktopEntry(desktopFilePath, QSettings::IniFormat);
            desktopEntry.beginGroup(QStringLiteral("Desktop Entry"));

            const QString iconName =
                desktopEntry.value(QStringLiteral("Icon"))
                    .toString()
                    .trimmed();

            desktopEntry.endGroup();

            if (!iconName.isEmpty())
            {
                return iconName;
            }
        }

        return {};
    }
}

WindowWatcher::WindowWatcher(WindowModel &windowModel,
                             QObject *parent)
    : QObject(parent), m_windowModel(windowModel)
{
    auto sessionBus = QDBusConnection::sessionBus();

    if (!sessionBus.registerService(
            QStringLiteral("org.archdock.ArchDock")))
    {
        qWarning() << "Failed to register Arch Dock D-Bus service:"
                   << sessionBus.lastError().message();
        return;
    }

    if (!sessionBus.registerObject(
            QStringLiteral("/WindowWatcher"),
            this,
            QDBusConnection::ExportAllSlots))
    {
        qWarning() << "Failed to register WindowWatcher D-Bus object:"
                   << sessionBus.lastError().message();
    }
}

void WindowWatcher::windowAdded(const QString &internalId,
                                const QString &desktopFileName,
                                const QString &resourceClass,
                                const QString &resourceName,
                                const QString &caption,
                                bool active,
                                bool minimized)
{
    qDebug() << "Window added:"
             << "id:" << internalId
             << "desktopFileName:" << desktopFileName
             << "resourceClass:" << resourceClass
             << "resourceName:" << resourceName
             << "caption:" << caption;

    if (resourceClass == QStringLiteral("arch-dock"))
    {
        qDebug() << "Ignoring Arch Dock's own window:" << internalId;
        return;
    }

    if (resourceClass == QStringLiteral("org.kde.spectacle"))
    {
        qDebug() << "Ignoring Spectacle window:" << internalId;
        return;
    }

    if (resourceClass == QStringLiteral("org.kde.plasmashell"))
    {
        qDebug() << "Ignoring Plasma shell window:" << internalId;
        return;
    }

    if (desktopFileName.trimmed().isEmpty() && resourceClass.trimmed().isEmpty())
    {
        qDebug() << "Ignoring window without an icon identifier:"
                 << internalId
                 << caption;
        return;
    }

    WindowItem window;
    window.internalId = internalId;
    window.desktopFileName = desktopFileName;
    window.iconName = resolveIconName(desktopFileName, resourceClass);
    window.resourceClass = resourceClass;
    window.resourceName = resourceName;
    window.caption = caption;
    window.active = active;
    window.minimized = minimized;

    m_windowModel.addWindow(window);
}

void WindowWatcher::windowRemoved(const QString &internalId)
{
    m_windowModel.removeWindow(internalId);
}

void WindowWatcher::windowUpdated(const QString &internalId,
                                  const QString &desktopFileName,
                                  const QString &resourceClass,
                                  const QString &resourceName,
                                  const QString &caption,
                                  bool active,
                                  bool minimized)
{
    WindowItem window;
    window.internalId = internalId;
    window.desktopFileName = desktopFileName;
    window.iconName = resolveIconName(desktopFileName, resourceClass);
    window.resourceClass = resourceClass;
    window.resourceName = resourceName;
    window.caption = caption;
    window.active = active;
    window.minimized = minimized;

    m_windowModel.updateWindow(window);
}