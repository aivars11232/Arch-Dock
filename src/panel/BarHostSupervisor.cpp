#include "BarHostSupervisor.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

namespace
{
// A host that stops this often within this while is not started again
// until the bars change.
constexpr int StartsAllowed = 5;
constexpr qint64 StartWindowMs = 60 * 1000;
constexpr int RestartDelayMs = 1000;
constexpr int StartCheckMs = 15000;
}

BarHostSupervisor::BarHostSupervisor(QObject *parent)
    : QObject(parent)
    , m_watcher(QString::fromLatin1(ServiceName), QDBusConnection::sessionBus(),
                QDBusServiceWatcher::WatchForUnregistration)
{
    m_restart.setSingleShot(true);
    m_restart.setInterval(RestartDelayMs);
    connect(&m_restart, &QTimer::timeout, this, &BarHostSupervisor::startHost);
    m_startCheck.setSingleShot(true);
    m_startCheck.setInterval(StartCheckMs);
    connect(&m_startCheck, &QTimer::timeout, this, [this]
    {
        if (!running())
            hostGone();
    });
    connect(&m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, &BarHostSupervisor::hostGone);
}

QString BarHostSupervisor::program()
{
    const QString chosen = qEnvironmentVariable("ARCHDOCK_BARS_EXECUTABLE");
    if (!chosen.isEmpty())
        return chosen;
    // Beside the backend's own program file, wherever it was started from:
    // Qt names the folder of the name it was started by, which for a link
    // to the backend is the link's.
    QStringList folders;
    const QString realProgram = QFileInfo(QStringLiteral("/proc/self/exe")).canonicalFilePath();
    if (!realProgram.isEmpty())
        folders.append(QFileInfo(realProgram).absolutePath());
    folders.append(QCoreApplication::applicationDirPath());
    for (const QString &folder : std::as_const(folders))
    {
        const QString beside = folder + QStringLiteral("/arch-dock-bars");
        if (QFileInfo(beside).isExecutable())
            return beside;
    }
    return QStringLiteral("arch-dock-bars");
}

bool BarHostSupervisor::running() const
{
    const QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(QString::fromLatin1(ServiceName)).value();
}

void BarHostSupervisor::setWanted(bool wanted)
{
    if (m_stopped || wanted == m_wanted)
        return;
    m_wanted = wanted;
    if (wanted)
    {
        // Wanted again, it starts afresh.
        m_starts.clear();
        startHost();
    }
    else
    {
        m_restart.stop();
        m_startCheck.stop();
        askToQuit();
    }
}

void BarHostSupervisor::barsChanged()
{
    if (m_stopped || !m_wanted || m_starts.size() < StartsAllowed || running())
        return;
    m_starts.clear();
    startHost();
}

void BarHostSupervisor::stop()
{
    m_stopped = true;
    m_restart.stop();
    m_startCheck.stop();
    askToQuit();
}

bool BarHostSupervisor::isTheBackend()
{
    // Only the backend serving the session starts the bars: the process that
    // owns Arch Dock's name on the session bus, never one made by a test.
    const QDBusConnection bus = QDBusConnection::sessionBus();
    const QDBusConnectionInterface *names = bus.interface();
    return bus.isConnected() && names
        && names->serviceOwner(QStringLiteral("org.archdock.ArchDock")).value() == bus.baseService();
}

void BarHostSupervisor::startHost()
{
    if (!m_wanted || m_stopped || running() || !isTheBackend())
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_starts.removeIf([now](qint64 at) { return now - at > StartWindowMs; });
    if (m_starts.size() >= StartsAllowed)
    {
        qWarning() << "Arch Dock's bar host stopped" << m_starts.size()
                   << "times within a minute; it is not started again until the bars change.";
        return;
    }
    m_starts.append(now);
    const QString host = program();
    if (!QProcess::startDetached(host, {}))
        qWarning() << "Arch Dock's bar host" << host << "could not be started.";
    m_startCheck.start();
}

void BarHostSupervisor::hostGone()
{
    if (m_wanted && !m_stopped)
        m_restart.start();
}

void BarHostSupervisor::askToQuit()
{
    if (!isTheBackend() || !running())
        return;
    const QString service = QString::fromLatin1(ServiceName);
    QDBusConnection::sessionBus().send(
        QDBusMessage::createMethodCall(service, QStringLiteral("/Bars"), service, QStringLiteral("quit")));
}
