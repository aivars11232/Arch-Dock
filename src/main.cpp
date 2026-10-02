#include <QGuiApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusReply>
#include <QQmlApplicationEngine>
#include <QUrl>
#include <QDebug>

#include "panel/PanelManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    // The backend outlives its optional Studio windows and KIO jobs. Releasing
    // KJob's last QEventLoopLocker must not stop this resident D-Bus service.
    app.setQuitLockEnabled(false);

    QGuiApplication::setApplicationName(QStringLiteral("Arch Dock"));
    QGuiApplication::setOrganizationName(QStringLiteral("Arch Dock"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Glass application dock for KDE Plasma"));
    parser.addHelpOption();
    const QCommandLineOption settingsOption(
        QStringList{QStringLiteral("settings"), QStringLiteral("configure")},
        QStringLiteral("Open the Arch Dock control center."));
    const QCommandLineOption autoHideOption(
        QStringList{QStringLiteral("toggle-auto-hide")},
        QStringLiteral("Toggle Arch Dock auto-hide."));
    parser.addOption(settingsOption);
    parser.addOption(autoHideOption);
    parser.process(app);

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        qCritical() << "Arch Dock session D-Bus connection failed:"
                    << sessionBus.lastError().message();
        return EXIT_FAILURE;
    }
    if (!sessionBus.registerService(QStringLiteral("org.archdock.ArchDock")))
    {
        const QDBusError registrationError = sessionBus.lastError();
        const QDBusReply<bool> existingOwner = sessionBus.interface()->isServiceRegistered(
            QStringLiteral("org.archdock.ArchDock"));
        if (!existingOwner.isValid() || !existingOwner.value())
        {
            qCritical() << "Arch Dock D-Bus service registration failed:"
                        << registrationError.name() << registrationError.message();
            return EXIT_FAILURE;
        }
        QDBusInterface control(
            QStringLiteral("org.archdock.ArchDock"),
            QStringLiteral("/Control"),
            QStringLiteral("local.PanelWindow"),
            sessionBus);
        if (parser.isSet(settingsOption) || parser.isSet(autoHideOption))
        {
            const QDBusMessage result = control.call(parser.isSet(settingsOption)
                ? QStringLiteral("showSettings") : QStringLiteral("toggleAutoHide"));
            if (result.type() == QDBusMessage::ErrorMessage)
            {
                qCritical() << "Arch Dock could not forward the command to its existing instance:"
                            << result.errorName() << result.errorMessage();
                return EXIT_FAILURE;
            }
        }
        else
        {
            qInfo() << "Arch Dock is already running.";
        }
        return EXIT_SUCCESS;
    }

    QQmlApplicationEngine engine;
    PanelManager panelManager(engine);

    if (parser.isSet(settingsOption))
    {
        panelManager.showSettings();
    }
    else if (parser.isSet(autoHideOption))
    {
        panelManager.toggleAutoHide();
    }

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []()
        {
            QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection);

    return app.exec();
}
