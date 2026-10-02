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
#include <memory>

#include "panel/PanelManager.h"
#include "persistence/ConfigurationBackup.h"
#include "panel/ProfileApplyTransaction.h"

int main(int argc, char *argv[])
{
    bool recoveryCommand = false;
    for (int i = 1; i < argc; ++i)
    {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        recoveryCommand = recoveryCommand || argument == QStringLiteral("--backup-config") ||
            argument == QStringLiteral("--list-config-backups") ||
            argument == QStringLiteral("--restore-config-backup") ||
            argument.startsWith(QStringLiteral("--restore-config-backup="));
    }
    std::unique_ptr<QCoreApplication> application;
    if (recoveryCommand) application = std::make_unique<QCoreApplication>(argc, argv);
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
    parser.process(app);
    if (recoveryCommand && (int(parser.isSet(backupOption)) + int(parser.isSet(listOption)) +
        int(parser.isSet(restoreOption)) != 1 || parser.isSet(settingsOption) || parser.isSet(autoHideOption)))
    { qCritical() << "Select exactly one configuration recovery command."; return EXIT_FAILURE; }

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    if (!sessionBus.isConnected())
    {
        qCritical() << "Arch Dock session D-Bus connection failed:"
                    << sessionBus.lastError().message();
        return EXIT_FAILURE;
    }
    if (!sessionBus.registerService(QStringLiteral("org.archdock.ArchDock")))
    {
        if (recoveryCommand)
        { qCritical() << "Configuration recovery requires Arch Dock to be stopped and its D-Bus name to be available."; return EXIT_FAILURE; }
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

    return app.exec();
}
