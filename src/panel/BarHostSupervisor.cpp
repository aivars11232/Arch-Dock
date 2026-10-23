#include "BarHostSupervisor.h"

#include "BarAutostart.h"
#include "../IntentionalStop.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include <unistd.h>

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
    if (m_stopped || (wanted == m_wanted && m_autostartKnown))
        return;
    const bool changed = wanted != m_wanted;
    m_wanted = wanted;
    syncAutostart();
    if (!changed)
        return;
    if (wanted)
    {
        // Wanted again, it starts afresh; a bar host left from before an
        // update makes way for the new one first.
        m_starts.clear();
        if (!replaceStaleHost())
            startHost();
        // Arch Dock may not own its name yet as the bars are first wanted:
        // the check for a bar host left from before an update is made again.
        for (const int delay : {3000, 10000})
            QTimer::singleShot(delay, this, [this] { if (m_wanted && !m_stopped) replaceStaleHost(); });
    }
    else
    {
        m_restart.stop();
        m_startCheck.stop();
        askToQuit();
    }
}

void BarHostSupervisor::syncAutostart()
{
    // Only the session's backend keeps the entry, starting itself.
    if (!isTheBackend())
        return;
    m_autostartKnown = true;
    const QString self = QFileInfo(QStringLiteral("/proc/self/exe")).canonicalFilePath();
    QString error;
    if (!ArchDock::BarAutostart::sync(m_wanted, ArchDock::BarAutostart::entryPath(),
                                      self.isEmpty() ? QCoreApplication::applicationFilePath() : self, &error))
        qWarning() << "Arch Dock's start at login could not be set for its bars:" << error;
}

void BarHostSupervisor::barsChanged()
{
    // A bar host left from before an update is replaced whenever the bars
    // change, too: at start Arch Dock may not yet own its name.
    if (!m_stopped && m_wanted && running())
        replaceStaleHost();
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

bool BarHostSupervisor::replaceStaleHost()
{
    // An update replaces the bar host's program while it runs: the kernel
    // then names the running file "(deleted)", and the bars keep the old
    // program's ways. It is asked to quit; once it is gone the new one starts
    // (hostGone).
    if (!isTheBackend() || !running())
        return false;
    const QDBusReply<uint> pid = QDBusConnection::sessionBus().interface()->servicePid(QString::fromLatin1(ServiceName));
    if (!pid.isValid() || pid.value() == 0)
        return false;
    char buffer[4096];
    const QByteArray link = QStringLiteral("/proc/%1/exe").arg(pid.value()).toLocal8Bit();
    const ssize_t length = ::readlink(link.constData(), buffer, sizeof buffer - 1);
    if (length <= 0 || !ArchDock::IntentionalStop::replacedLink(QString::fromLocal8Bit(buffer, int(length))))
        return false;
    qInfo() << "Arch Dock's bar host is from before an update; it is started anew.";
    askToQuit();
    return true;
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
