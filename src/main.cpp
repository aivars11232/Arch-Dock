#include <QGuiApplication>
#include <QCoreApplication>
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
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>
#include <QElapsedTimer>
#include <QSocketNotifier>
#include <QThread>
#include <QVariantMap>
#include <memory>

#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include "IntentionalStop.h"
#include "WindowWatcher.h"
#include "panel/PanelManager.h"
#include "persistence/ConfigurationBackup.h"
#include "panel/ProfileApplyTransaction.h"

namespace
{
const QString ServiceName = QStringLiteral("org.archdock.ArchDock");
// EX_TEMPFAIL. A refused activation must fail at once: the session bus keeps
// a request pending until its timeout when the process exits successfully.
constexpr int ActivationRefusedExitCode = 75;
int stopSignalPipe[2] = {-1, -1};

// Only async-signal-safe work here; the event loop performs the stop.
void requestStopFromSignal(int)
{
    const char byte = 1;
    [[maybe_unused]] const ssize_t written = ::write(stopSignalPipe[0], &byte, 1);
}

// `arch-dock --quit`: never activates or becomes the backend it stops.
int quitArchDock(QDBusConnection &bus)
{
    QString error;
    const bool recorded = ArchDock::IntentionalStop::record(QStringLiteral("quit"), &error);
    if (bus.interface()->isServiceRegistered(ServiceName).value())
    {
        QDBusMessage call = QDBusMessage::createMethodCall(
            ServiceName, QStringLiteral("/Control"), QStringLiteral("local.PanelWindow"),
            QStringLiteral("quit"));
        call.setAutoStartService(false);
        const QDBusMessage reply = bus.call(call, QDBus::Block, 30000);
        if (reply.type() == QDBusMessage::ErrorMessage)
        {
            qCritical() << "Arch Dock did not accept Quit:" << reply.errorName()
                        << reply.errorMessage();
            return EXIT_FAILURE;
        }
        const QVariantMap result = qdbus_cast<QVariantMap>(reply.arguments().value(0));
        if (!result.value(QStringLiteral("success")).toBool())
        {
            qCritical().noquote() << result.value(QStringLiteral("message")).toString();
            return EXIT_FAILURE;
        }
        QElapsedTimer waiting;
        waiting.start();
        while (bus.interface()->isServiceRegistered(ServiceName).value())
        {
            if (waiting.hasExpired(10000))
            {
                qCritical() << "Arch Dock accepted Quit but is still running.";
                return EXIT_FAILURE;
            }
            QThread::msleep(50);
        }
    }
    WindowWatcher::releaseKWinScripts();
    if (!recorded)
    {
        qCritical().noquote() << "Arch Dock is stopped, but an installed applet may start it again:"
                              << error;
        return EXIT_FAILURE;
    }
    QTextStream(stdout) << "Arch Dock is stopped and stays stopped until it is started again "
                           "or you log in again.\n";
    return EXIT_SUCCESS;
}
}

int main(int argc, char *argv[])
{
    bool recoveryCommand = false;
    bool quitCommand = false;
    bool activated = false;
    for (int i = 1; i < argc; ++i)
    {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        recoveryCommand = recoveryCommand || argument == QStringLiteral("--backup-config") ||
            argument == QStringLiteral("--list-config-backups") ||
            argument == QStringLiteral("--restore-config-backup") ||
            argument.startsWith(QStringLiteral("--restore-config-backup="));
        quitCommand = quitCommand || argument == QStringLiteral("--quit");
        activated = activated || argument == QStringLiteral("--dbus-activated");
    }
    // The session bus starts this process on demand. After the user quit,
    // leave the name unowned and stop the KWin watcher from asking again.
    if (activated && !recoveryCommand && !quitCommand && ArchDock::IntentionalStop::active())
    {
        QCoreApplication stopped(argc, argv);
        WindowWatcher::releaseKWinScripts();
        qInfo() << "Arch Dock was quit for this session; it refuses on-demand activation"
                << "until it is started again.";
        return ActivationRefusedExitCode;
    }
    std::unique_ptr<QCoreApplication> application;
    if (recoveryCommand || quitCommand) application = std::make_unique<QCoreApplication>(argc, argv);
    else
    {
        auto gui = std::make_unique<QGuiApplication>(argc, argv);
        gui->setQuitOnLastWindowClosed(false);
        application = std::move(gui);
    }
    QCoreApplication &app = *application;
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
    const QCommandLineOption backupOption(QStringLiteral("backup-config"), QStringLiteral("Back up user configuration while Arch Dock is stopped."));
    const QCommandLineOption listOption(QStringLiteral("list-config-backups"), QStringLiteral("List valid configuration backups."));
    const QCommandLineOption restoreOption(QStringLiteral("restore-config-backup"), QStringLiteral("Restore a validated configuration backup while Arch Dock is stopped."), QStringLiteral("id"));
    parser.addOption(backupOption);
    parser.addOption(listOption);
    parser.addOption(restoreOption);
    const QCommandLineOption quitOption(QStringLiteral("quit"), QStringLiteral("Stop Arch Dock and keep it stopped until it is started again or you log in again."));
    QCommandLineOption activatedOption(QStringLiteral("dbus-activated"), QStringLiteral("Started on demand by the session bus."));
    activatedOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(quitOption);
    parser.addOption(activatedOption);
    parser.process(app);
    if (recoveryCommand && (int(parser.isSet(backupOption)) + int(parser.isSet(listOption)) +
        int(parser.isSet(restoreOption)) != 1 || parser.isSet(settingsOption) || parser.isSet(autoHideOption)))
    { qCritical() << "Select exactly one configuration recovery command."; return EXIT_FAILURE; }
    if (quitCommand && (recoveryCommand || activated || parser.isSet(settingsOption) || parser.isSet(autoHideOption)))
    { qCritical() << "--quit cannot be combined with another command."; return EXIT_FAILURE; }

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        qCritical() << "Arch Dock session D-Bus connection failed:"
                    << sessionBus.lastError().message();
        return EXIT_FAILURE;
    }
    if (quitCommand) return quitArchDock(sessionBus);
    // Starting Arch Dock explicitly ends a stop requested earlier.
    QString clearError;
    if (!recoveryCommand && !activated && !ArchDock::IntentionalStop::clear(&clearError))
        qWarning().noquote() << clearError;
    if (!sessionBus.registerService(ServiceName))
    {
        if (recoveryCommand)
        { qCritical() << "Configuration recovery requires Arch Dock to be stopped and its D-Bus name to be available."; return EXIT_FAILURE; }
        const QDBusError registrationError = sessionBus.lastError();
        const QDBusReply<bool> existingOwner = sessionBus.interface()->isServiceRegistered(
            ServiceName);
        if (!existingOwner.isValid() || !existingOwner.value())
        {
            qCritical() << "Arch Dock D-Bus service registration failed:"
                        << registrationError.name() << registrationError.message();
            return EXIT_FAILURE;
        }
        QDBusInterface control(
            ServiceName,
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

    ArchDock::ConfigurationBackup backup;
    QString recoveryError;
    if (recoveryCommand && parser.isSet(listOption))
    {
        const auto ids = backup.backups(&recoveryError);
        if (!recoveryError.isEmpty()) { qCritical().noquote() << recoveryError; return EXIT_FAILURE; }
        QTextStream output(stdout);
        for (const QString &id : ids) output << id << '\n';
        return EXIT_SUCCESS;
    }
    const bool profileRecoveryPending = QFileInfo::exists(ArchDock::ProfileApplyTransaction::defaultJournalPath());
    if (profileRecoveryPending && (recoveryCommand || backup.recoveryPending()))
    { qCritical() << "Resolve the existing profile transaction before configuration recovery."; return EXIT_FAILURE; }
    if (!backup.recover(&recoveryError))
    { qCritical().noquote() << "Configuration restore recovery failed:" << recoveryError; return EXIT_FAILURE; }
    if (recoveryCommand)
    {
        QString id;
        const bool success = parser.isSet(restoreOption)
            ? backup.restore(parser.value(restoreOption), &recoveryError)
            : !(id = backup.capture(QStringLiteral("manual"), &recoveryError)).isEmpty();
        if (!success || !backup.prune(QSettings().value(QStringLiteral("backup/retentionCount"), 5).toInt(), &recoveryError))
        { qCritical().noquote() << recoveryError; return EXIT_FAILURE; }
        QTextStream(stdout) << (id.isEmpty() ? QStringLiteral("Configuration restored.") : id) << '\n';
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

    // TERM and INT, as sent by "Quit Application" or Ctrl+C, stop Arch Dock
    // the way Quit does. KILL cannot run any of this: it is treated as a
    // crash, and the next activation request starts Arch Dock again.
    std::unique_ptr<QSocketNotifier> stopNotifier;
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, stopSignalPipe) == 0)
    {
        stopNotifier = std::make_unique<QSocketNotifier>(stopSignalPipe[1], QSocketNotifier::Read);
        QObject::connect(stopNotifier.get(), &QSocketNotifier::activated, &app, [&]()
        {
            char byte = 0;
            [[maybe_unused]] const ssize_t received = ::read(stopSignalPipe[1], &byte, 1);
            stopNotifier->setEnabled(false);
            panelManager.stopIntentionally(QStringLiteral("signal"));
        });
        struct sigaction action = {};
        action.sa_handler = requestStopFromSignal;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        ::sigaction(SIGTERM, &action, nullptr);
        ::sigaction(SIGINT, &action, nullptr);
    }
    else
    {
        qWarning() << "Arch Dock could not watch for termination signals.";
    }

    return app.exec();
}
