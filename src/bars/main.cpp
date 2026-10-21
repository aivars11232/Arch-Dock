// SPDX-License-Identifier: GPL-3.0-or-later
// arch-dock-bars: Arch Dock's own bars. Each bar is a Plasma containment of
// this program's own corona holding Arch Dock's dock widget, shown in a
// layer-shell window of Arch Dock's own along a screen edge, so that bars can
// stand side by side on one edge. The backend lists the bars and starts this
// program; only one runs in a session, owning org.archdock.ArchDock.Bars.

#include "BarCorona.h"
#include "BarHost.h"

#include <KPackage/PackageLoader>

#include <QApplication>
#include <QDBusConnection>
#include <QDebug>
#include <QQuickWindow>
#include <QSocketNotifier>

#include <csignal>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
const QString ServiceName = QStringLiteral("org.archdock.ArchDock.Bars");
int stopSignalPipe[2] = {-1, -1};

// Only async-signal-safe work here; the event loop does the stopping.
void requestStopFromSignal(int)
{
    const char byte = 1;
    [[maybe_unused]] const ssize_t written = ::write(stopSignalPipe[0], &byte, 1);
}

// What the backend may ask of the bars over the session bus.
class BarHostControl : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.archdock.ArchDock.Bars")

public:
    using QObject::QObject;

public Q_SLOTS:
    // Arch Dock is being stopped: the bars go with it.
    Q_SCRIPTABLE void quit()
    {
        QCoreApplication::quit();
    }
};
}

int main(int argc, char **argv)
{
    // Bars are see-through where their widgets draw nothing.
    QQuickWindow::setDefaultAlphaBuffer(true);
    // The bars keep the places the host gives them on the screen, as
    // plasmashell's panels do (plasma-workspace sets the same): Qt on Wayland
    // otherwise puts every window at its screen's corner, and the dock's
    // popups and its window past the bar place themselves from the bar's
    // place. Only for this program, not for what its widgets start.
    qputenv("QT_WAYLAND_DISABLE_FIXED_POSITIONS", "1");
    QApplication app(argc, argv);
    qunsetenv("QT_WAYLAND_DISABLE_FIXED_POSITIONS");
    app.setApplicationName(QStringLiteral("arch-dock-bars"));
    app.setDesktopFileName(QStringLiteral("org.archdock.ArchDock.Bars"));
    app.setQuitOnLastWindowClosed(false);

    if (!QGuiApplication::platformName().startsWith(QLatin1String("wayland")))
    {
        qCritical() << "Arch Dock's bars need a Wayland session.";
        return EXIT_FAILURE;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
    {
        qCritical() << "Arch Dock's bars could not reach the session bus.";
        return EXIT_FAILURE;
    }
    if (!bus.registerService(ServiceName))
    {
        qInfo() << "Arch Dock's bars are already shown.";
        return EXIT_SUCCESS;
    }
    BarHostControl control;
    bus.registerObject(QStringLiteral("/Bars"), &control, QDBusConnection::ExportScriptableSlots);

    KPackage::Package shell = KPackage::PackageLoader::self()->loadPackage(
        QStringLiteral("Plasma/Shell"), QStringLiteral("org.archdock.barshell"));
    if (!shell.isValid())
    {
        qCritical() << "Arch Dock's bar shell (org.archdock.barshell) is not installed.";
        return EXIT_FAILURE;
    }
    ArchDock::Bars::BarCorona corona;
    corona.setKPackage(shell);
    corona.loadLayout(QStringLiteral("arch-dock-bars-appletsrc"));
    ArchDock::Bars::BarHost host(&corona, shell.fileUrl("views", QStringLiteral("Bar.qml")));
    host.start();

    std::unique_ptr<QSocketNotifier> stopNotifier;
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, stopSignalPipe) == 0)
    {
        stopNotifier = std::make_unique<QSocketNotifier>(stopSignalPipe[1], QSocketNotifier::Read);
        QObject::connect(stopNotifier.get(), &QSocketNotifier::activated, &app, [&]()
        {
            char byte = 0;
            [[maybe_unused]] const ssize_t received = ::read(stopSignalPipe[1], &byte, 1);
            stopNotifier->setEnabled(false);
            QCoreApplication::quit();
        });
        struct sigaction action = {};
        action.sa_handler = requestStopFromSignal;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        ::sigaction(SIGTERM, &action, nullptr);
        ::sigaction(SIGINT, &action, nullptr);
    }

    const int result = app.exec();
    corona.requireConfigSync();
    return result;
}

#include "main.moc"
