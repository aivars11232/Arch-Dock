#include <QGuiApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QQmlApplicationEngine>
#include <QUrl>
#include <QDebug>

#include "panel/PanelManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setQuitOnLastWindowClosed(false);

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
    if (!sessionBus.registerService(QStringLiteral("org.archdock.ArchDock")))
    {
        QDBusInterface control(
            QStringLiteral("org.archdock.ArchDock"),
            QStringLiteral("/Control"),
            QStringLiteral("local.PanelWindow"),
            sessionBus);
        if (parser.isSet(settingsOption))
        {
            control.call(QStringLiteral("showSettings"));
        }
        else if (parser.isSet(autoHideOption))
        {
            control.call(QStringLiteral("toggleAutoHide"));
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
