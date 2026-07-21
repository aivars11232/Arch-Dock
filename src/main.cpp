#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

#include "panel/PanelManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QGuiApplication::setApplicationName(QStringLiteral("Arch Dock"));
    QGuiApplication::setOrganizationName(QStringLiteral("Arch Dock"));

    QQmlApplicationEngine engine;
    PanelManager panelManager(engine);

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
