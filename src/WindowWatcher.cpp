#include "WindowWatcher.h"

#include "WindowModel.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
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

    int dbusInteger(const QString &value, int fallback = 0)
    {
        bool valid = false;
        const double number = value.toDouble(&valid);
        return valid ? qRound(number) : fallback;
    }
}

WindowWatcher::WindowWatcher(WindowModel &windowModel,
                             QObject *parent)
    : QObject(parent), m_windowModel(windowModel)
{
    auto sessionBus = QDBusConnection::sessionBus();

    if (!sessionBus.registerObject(
            QStringLiteral("/WindowWatcher"),
            this,
            QDBusConnection::ExportAllSlots))
    {
        qWarning() << "Failed to register WindowWatcher D-Bus object:"
                   << sessionBus.lastError().message();
    }

    loadKWinScript();
}

void WindowWatcher::windowAdded(const QString &internalId,
                                const QString &desktopFileName,
                                const QString &resourceClass,
                                const QString &resourceName,
                                const QString &caption,
                                bool active,
                                bool minimized,
                                const QString &frameX,
                                const QString &frameY,
                                const QString &frameWidth,
                                const QString &frameHeight,
                                const QString &screenIndex,
                                bool maximized,
                                bool fullScreen)
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

    if (resourceClass == QStringLiteral("org.kde.plasmashell") ||
        resourceClass == QStringLiteral("plasmashell"))
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
    window.frameGeometry = QRect(dbusInteger(frameX),
                                 dbusInteger(frameY),
                                 qMax(0, dbusInteger(frameWidth)),
                                 qMax(0, dbusInteger(frameHeight)));
    window.screenIndex = qMax(0, dbusInteger(screenIndex));
    window.active = active;
    window.minimized = minimized;
    window.maximized = maximized;
    window.fullScreen = fullScreen;

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
                                  bool minimized,
                                  const QString &frameX,
                                  const QString &frameY,
                                  const QString &frameWidth,
                                  const QString &frameHeight,
                                  const QString &screenIndex,
                                  bool maximized,
                                  bool fullScreen)
{
    WindowItem window;
    window.internalId = internalId;
    window.desktopFileName = desktopFileName;
    window.iconName = resolveIconName(desktopFileName, resourceClass);
    window.resourceClass = resourceClass;
    window.resourceName = resourceName;
    window.caption = caption;
    window.frameGeometry = QRect(dbusInteger(frameX),
                                 dbusInteger(frameY),
                                 qMax(0, dbusInteger(frameWidth)),
                                 qMax(0, dbusInteger(frameHeight)));
    window.screenIndex = qMax(0, dbusInteger(screenIndex));
    window.active = active;
    window.minimized = minimized;
    window.maximized = maximized;
    window.fullScreen = fullScreen;

    m_windowModel.updateWindow(window);
}

void WindowWatcher::loadKWinScript()
{
    const QString installedScriptPath = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation,
        QStringLiteral("kwin/scripts/org.archdock.windowwatcher/contents/code/main.js"));
    const QString scriptPath = installedScriptPath.isEmpty()
                                   ? QString::fromUtf8(ARCHDOCK_SOURCE_KWIN_SCRIPT_PATH)
                                   : installedScriptPath;
    if (!QFileInfo::exists(scriptPath))
    {
        qWarning() << "Arch Dock KWin script is unavailable:" << scriptPath;
        return;
    }

    QDBusInterface scripting(
        QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Scripting"),
        QStringLiteral("org.kde.kwin.Scripting"),
        QDBusConnection::sessionBus());
    if (!scripting.isValid())
    {
        qWarning() << "KWin scripting D-Bus interface is unavailable.";
        return;
    }

    const QString pluginName = QStringLiteral("org.archdock.windowwatcher");
    scripting.call(QStringLiteral("unloadScript"), pluginName);
    const QDBusMessage loadReply = scripting.call(
        QStringLiteral("loadScript"),
        scriptPath,
        pluginName);
    if (loadReply.type() == QDBusMessage::ErrorMessage)
    {
        qWarning() << "Failed to load Arch Dock KWin script:"
                   << loadReply.errorMessage();
        return;
    }

    scripting.call(QStringLiteral("start"));
}
