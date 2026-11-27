// SPDX-License-Identifier: GPL-3.0-or-later
#include "ArchDockUpdates.h"

#include "ArchDockVersion.h"
#include "IntentionalStop.h"
#include "persistence/ConfigurationBackup.h"
#include "updates/UpdateHold.h"

#include <KNotification>

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>

#include <cerrno>
#include <csignal>
#include <unistd.h>

namespace ArchDock::Updates
{
namespace
{
constexpr qint64 DayMs = 24 * 3600 * 1000LL;
constexpr int HourMs = 3600 * 1000;
constexpr int FirstLookMs = 5 * 60 * 1000;
constexpr qint64 PendingInputLimit = 1024 * 1024;
constexpr qint64 ExtraRoom = 64 * 1024 * 1024;
constexpr int CheckShownMs = 150;
// The newest part of what the helper passes on, for Copy Details.
constexpr qsizetype HelperOutputKept = 64 * 1024;

bool safeFile(const QFileInfo &info)
{
    return !info.isSymLink() && info.isFile() && info.ownerId() == static_cast<uint>(::getuid()) && info.size() <= 4096;
}

std::optional<QJsonObject> readRuntimeJson(const QString &path)
{
    if (path.isEmpty())
        return std::nullopt;
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return std::nullopt;
    if (!safeFile(info))
    {
        QFile::remove(path);
        return std::nullopt;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonDocument document = QJsonDocument::fromJson(file.read(4096));
    if (!document.isObject())
        return std::nullopt;
    return document.object();
}

bool writeRuntimeJson(const QString &path, const QJsonObject &object)
{
    if (path.isEmpty())
        return false;
    const QFileInfo existing(path);
    if ((existing.exists() || existing.isSymLink()) && !safeFile(existing) && !QFile::remove(path))
        return false;
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    return file.open(QIODevice::WriteOnly)
        && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        && file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) >= 0 && file.commit();
}

// Whether a process with this name runs; with `ownOnly`, only this account's.
bool processRuns(const QString &name, bool ownOnly)
{
    const QDir proc(QStringLiteral("/proc"));
    for (const QString &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        bool numeric = false;
        entry.toUInt(&numeric);
        if (!numeric)
            continue;
        QFile comm(proc.filePath(entry + QStringLiteral("/comm")));
        if (!comm.open(QIODevice::ReadOnly) || QString::fromLatin1(comm.readAll()).trimmed() != name)
            continue;
        if (!ownOnly || QFileInfo(proc.filePath(entry)).ownerId() == static_cast<uint>(::getuid()))
            return true;
    }
    return false;
}

// "qt6-base>=6.13" reads "qt6-base 6.13 or newer".
QString neededPart(const QString &need)
{
    const qsizetype at = need.indexOf(QLatin1String(">="));
    if (at > 0)
        return QStringLiteral("%1 %2 or newer").arg(need.left(at), need.mid(at + 2));
    return need;
}
} // namespace

struct ArchDockUpdates::Marker
{
    QString from;
    QString to;
    QString phase;
    QString session;
};

#ifdef ARCHDOCK_TESTING
namespace
{
std::optional<ArchDockUpdates::Environment> &systemForTesting()
{
    static std::optional<ArchDockUpdates::Environment> environment;
    return environment;
}
} // namespace

void ArchDockUpdates::setSystemForTesting(std::optional<Environment> environment)
{
    systemForTesting() = std::move(environment);
}
#endif

ArchDockUpdates::Environment ArchDockUpdates::Environment::system()
{
#ifdef ARCHDOCK_TESTING
    if (systemForTesting())
        return *systemForTesting();
#endif
    Environment environment;
    const QString stop = IntentionalStop::statePath();
    environment.runtimeFolder = stop.isEmpty() ? QString() : QFileInfo(stop).absolutePath();
    environment.cacheFolder = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/arch-dock/update");
    environment.stateFolder = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/arch-dock/update");
    environment.toolFolder = Version::toolFolder();
    environment.pkexec = environment.toolFolder + QStringLiteral("/pkexec");
    environment.systemdRun = environment.toolFolder + QStringLiteral("/systemd-run");
    environment.helperPath = Version::helperPath();
    environment.finisherPath = Version::finisherPath();
    environment.pacmanLock = Version::pacmanLock();
    environment.release = Version::release();
    environment.repository = Version::repository();
    environment.keyId = Version::keyId();
    environment.keySpki = Version::keySpki();
    environment.testBuild = Version::isTestBuild();
    environment.updaterProtocol = Version::updaterProtocol();
    environment.api = Version::apiAddress();
    environment.downloads = Version::downloadsAddress();
    return environment;
}

ArchDockUpdates::ArchDockUpdates(Hooks hooks, QObject *parent)
    : ArchDockUpdates(std::move(hooks), Environment::system(), parent)
{
}

ArchDockUpdates::ArchDockUpdates(Hooks hooks, Environment environment, QObject *parent)
    : QObject(parent)
    , m_hooks(std::move(hooks))
    , m_env(std::move(environment))
{
    QSettings settings;
    m_checkAutomatically = settings.value(QStringLiteral("updates/checkAutomatically"), true).toBool();
    m_lastAnswer = settings.value(QStringLiteral("updates/lastAnswer")).toDateTime();

    // Offered only by an installed release build with every part in place. A
    // rehearsal build has no password-window action; its pkexec is a stand-in.
    const QString exe = QFile::symLinkTarget(QStringLiteral("/proc/self/exe")).remove(QStringLiteral(" (deleted)"));
    m_offered = m_env.offered || (installsUpdates() && exe == Version::backendPath()
        && installedArchDock(runner(), m_env.toolFolder).has_value()
        && QFileInfo::exists(m_env.helperPath) && QFileInfo::exists(m_env.finisherPath)
        && (QFileInfo::exists(Version::policyPath()) || !Version::rehearsalRoot().isEmpty()) && QFileInfo::exists(m_env.pkexec)
        && QFileInfo::exists(m_env.systemdRun) && QFileInfo::exists(m_env.toolFolder + QStringLiteral("/openssl")));
    if (!m_offered)
        return;

    QNetworkProxyFactory::setUseSystemConfiguration(true);
    m_network = std::make_unique<QNetworkAccessManager>();
    QDir().mkpath(stateFolder());
    QFile::setPermissions(stateFolder(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    ReleaseSetup setup;
    setup.api = m_env.api;
    setup.downloads = m_env.downloads;
    setup.repository = m_env.repository;
    setup.userAgent = QByteArrayLiteral("ArchDock/") + m_env.release.toLatin1();
    setup.toolFolder = m_env.toolFolder;
    setup.keyId = m_env.keyId;
    setup.keySpki = m_env.keySpki;
    setup.testBuild = m_env.testBuild;
    setup.updaterProtocol = m_env.updaterProtocol;
    setup.workFolder = stateFolder();
    m_source = std::make_unique<ReleaseSource>(m_network.get(), runner(), setup);
    connect(m_source.get(), &ReleaseSource::finished, this, &ArchDockUpdates::lookedAt);
    m_download = std::make_unique<UpdateDownload>(m_network.get(), nullptr, m_env.testBuild && !m_env.downloads.startsWith(QLatin1String("https://")));
    connect(m_download.get(), &UpdateDownload::progress, this, [this](qint64 received, qint64)
    {
        m_received = received;
        Q_EMIT progressChanged();
    });
    connect(m_download.get(), &UpdateDownload::finished, this, &ArchDockUpdates::downloaded);

    m_resultWatch.setInterval(500);
    connect(&m_resultWatch, &QTimer::timeout, this, [this] { watchResult(m_restartTo); });
    m_tick.setInterval(HourMs);
    connect(&m_tick, &QTimer::timeout, this, &ArchDockUpdates::dailyTick);
    m_tick.start();
    QTimer::singleShot(FirstLookMs, this, &ArchDockUpdates::dailyTick);

    // An account switch back to this one: the other account may have
    // updated Arch Dock. logind's Active property of this account's display
    // session says when.
    QDBusConnection system = QDBusConnection::systemBus();
    const QString login1 = QStringLiteral("org.freedesktop.login1");
    QDBusInterface manager(login1, QStringLiteral("/org/freedesktop/login1"), QStringLiteral("org.freedesktop.login1.Manager"), system);
    const QDBusReply<QDBusObjectPath> user = manager.call(QStringLiteral("GetUser"), static_cast<uint>(::getuid()));
    if (user.isValid())
    {
        QDBusMessage get = QDBusMessage::createMethodCall(login1, user.value().path(), QStringLiteral("org.freedesktop.DBus.Properties"),
                                                          QStringLiteral("Get"));
        get << QStringLiteral("org.freedesktop.login1.User") << QStringLiteral("Display");
        const QDBusMessage reply = system.call(get);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
        {
            const QDBusArgument display = reply.arguments().at(0).value<QDBusVariant>().variant().value<QDBusArgument>();
            QString id;
            QDBusObjectPath session;
            display.beginStructure();
            display >> id >> session;
            display.endStructure();
            if (!session.path().isEmpty() && session.path() != QLatin1String("/"))
                system.connect(login1, session.path(), QStringLiteral("org.freedesktop.DBus.Properties"),
                               QStringLiteral("PropertiesChanged"), this,
                               SLOT(sessionPropertiesChanged(QString,QVariantMap,QStringList)));
        }
    }

    readAfterRestart();
    if (m_state == QLatin1String("not-looked"))
        checkRestartNeeded();
}

ArchDockUpdates::~ArchDockUpdates()
{
    // After "started" pkexec runs as root: this account cannot end it, and
    // QProcess's destructor would wait for it for up to 30 s. It is let go
    // and finishes the install on its own.
    if (m_pkexec && m_started && m_pkexec->state() != QProcess::NotRunning)
    {
        m_pkexec->disconnect(this);
        m_pkexec->setParent(nullptr);
        m_pkexec = nullptr;
    }
}

CommandRunner &ArchDockUpdates::runner() const
{
    return m_env.runner ? *m_env.runner : m_runner;
}

bool ArchDockUpdates::running(const QString &name, bool ownOnly) const
{
    return m_env.processRuns ? m_env.processRuns(name, ownOnly) : processRuns(name, ownOnly);
}

QString ArchDockUpdates::runtimeFile(const QString &name) const
{
    return m_env.runtimeFolder.isEmpty() ? QString() : QDir(m_env.runtimeFolder).filePath(name);
}

void ArchDockUpdates::sessionPropertiesChanged(const QString &, const QVariantMap &changed, const QStringList &)
{
    if (changed.value(QStringLiteral("Active")).toBool())
        checkRestartNeeded();
}

QString ArchDockUpdates::runningVersion() const
{
    return Version::displayVersion();
}

QString ArchDockUpdates::lastLooked() const
{
    return m_lastAnswer.isValid() ? QLocale().toString(m_lastAnswer.toLocalTime(), QStringLiteral("d MMM 'at' HH:mm")) : QString();
}

QString ArchDockUpdates::releasedOn() const
{
    const QDateTime at = QDateTime::fromString(m_found.manifest.publishedAt, Qt::ISODate);
    return at.isValid() ? QLocale().toString(at.toLocalTime(), QStringLiteral("d MMM yyyy")) : QString();
}

QStringList ArchDockUpdates::noteTitles() const
{
    // Each item's bold title ("- **Title.** text"), as plain text; notes
    // written without titles give their lines.
    static const QRegularExpression bold(QStringLiteral(R"(^\s*[-*]\s+\*\*(.+?)\*\*)"));
    const QStringList lines = m_found.manifest.releaseNotes.split(u'\n');
    QStringList titles;
    for (const QString &line : lines)
        if (const QRegularExpressionMatch match = bold.match(line); match.hasMatch())
            titles.append(match.captured(1).trimmed());
    if (!titles.isEmpty())
        return titles;
    for (QString line : lines)
    {
        line = line.trimmed();
        while (line.startsWith(u'-') || line.startsWith(u'*'))
            line = line.mid(1).trimmed();
        if (!line.isEmpty())
            titles.append(line.remove(QStringLiteral("**")));
    }
    return titles;
}

bool ArchDockUpdates::canUpdate() const
{
    return m_state == QLatin1String("found") && m_found.tooOld.isEmpty() && m_found.notInstalled.isEmpty();
}

bool ArchDockUpdates::canStop() const
{
    return m_state == QLatin1String("updating") && !m_started
        && (m_step == QLatin1String("download") || m_step == QLatin1String("check") || m_step == QLatin1String("password"));
}

QString ArchDockUpdates::installLine() const
{
    return m_found.notInstalled.isEmpty() ? QString()
                                          : QStringLiteral("sudo pacman -Syu --needed ") + m_found.notInstalled.join(u' ');
}

QString ArchDockUpdates::releasesPage() const
{
    const QString tag = m_found.manifest.releaseTag;
    return QStringLiteral("https://github.com/%1/releases").arg(m_env.repository)
        + (tag.isEmpty() ? QString() : QStringLiteral("/tag/") + tag);
}

bool ArchDockUpdates::holdsSettings() const
{
    return UpdateHold::held();
}

void ArchDockUpdates::setCheckAutomatically(bool on)
{
    if (on == m_checkAutomatically)
        return;
    m_checkAutomatically = on;
    QSettings().setValue(QStringLiteral("updates/checkAutomatically"), on);
    Q_EMIT changed();
}

bool ArchDockUpdates::attention() const
{
    return m_state == QLatin1String("found") || m_state == QLatin1String("restart-needed")
        || m_state == QLatin1String("finish");
}

QString ArchDockUpdates::lineText(const QString &code) const
{
    const QString version = m_found.manifest.version;
    const QString to = m_restartTo.isEmpty() ? version : m_restartTo;
    static const QHash<QString, QString> fixed = {
        {QStringLiteral("L1"), QStringLiteral("Could not reach GitHub. Check that this PC is online, then try again.")},
        {QStringLiteral("L2"), QStringLiteral("GitHub is not answering right now. Try again later.")},
        {QStringLiteral("L4"), QStringLiteral("Arch Dock can no longer check for updates this way. Look on the releases page for a newer Arch Dock.")},
        {QStringLiteral("L5"), QStringLiteral("The newest release on GitHub is not complete, so nothing can be installed from it. Try again later.")},
        {QStringLiteral("L6"), QStringLiteral("The newest release on GitHub is not signed with the Arch Dock key, so it is not offered.")},
        {QStringLiteral("L10"), QStringLiteral("The download stopped before it finished. Try again.")},
        {QStringLiteral("L11"), QStringLiteral("The downloaded file is not the one the signed update describes, so nothing was installed. Try again later.")},
        {QStringLiteral("L12"), QStringLiteral("There is not enough free disk space for the update.")},
        {QStringLiteral("L13"), QStringLiteral("Arch Dock could not save a copy of your settings first, so nothing was installed. Check that your home folder has free space, then try again.")},
        {QStringLiteral("L14"), QStringLiteral("Another program is installing software right now. Press Update Now again when it has finished.")},
        {QStringLiteral("L15"), QStringLiteral("An earlier software install was cut off and left its lock behind, so nothing was installed. It also blocks your own system updates. Copy Details has the steps that clear it.")},
        {QStringLiteral("L16"), QStringLiteral("Nothing was installed: the password was not accepted. After three wrong tries your password is locked for 10 minutes (also for sudo and the lock screen). This also happens if you switch to another account during the update.")},
        {QStringLiteral("L17"), QStringLiteral("The password window could not open, so nothing was installed. Log out and in, then try again.")},
        {QStringLiteral("L18"), QStringLiteral("The update file did not pass Arch Dock's checks, so nothing was installed. Try again later.")},
        {QStringLiteral("L19"), QStringLiteral("This update is not signed with the Arch Dock key, so nothing was installed.")},
        {QStringLiteral("L22"), QStringLiteral("The update could not be installed. Nothing was changed.")},
        {QStringLiteral("L27"), QStringLiteral("Plasma's panels did not come back after the Arch Dock update. Log out and in.")},
        {QStringLiteral("L28"), QStringLiteral("Finish or cancel the preview on the desktop first.")},
        {QStringLiteral("L29"), QStringLiteral("A profile is being applied. Update when it has finished.")},
        {QStringLiteral("L32"), QStringLiteral("Arch Dock is installing its update. Quit again when it has finished.")},
        {QStringLiteral("L34"), QStringLiteral("Arch Dock could not check the release on this PC. Check that your home folder has free space, then try again.")},
    };
    if (fixed.contains(code))
        return fixed.value(code);
    if (code == QLatin1String("L3"))
        return QStringLiteral("GitHub asked Arch Dock to wait. Try again after %1.")
            .arg(QLocale().toString(m_retryAfter.toLocalTime().time(), QStringLiteral("HH:mm")));
    if (code == QLatin1String("L7") || code == QLatin1String("L21"))
        return QStringLiteral("Arch Dock %1 has to be installed by hand once. The releases page says how.")
            .arg(m_found.byHandVersion.isEmpty() ? version : m_found.byHandVersion);
    if (code == QLatin1String("L8"))
    {
        QStringList parts;
        for (const QString &need : m_found.tooOld)
            parts.append(neededPart(need));
        return parts.isEmpty()
            ? QStringLiteral("Arch Dock %1 needs newer system parts. Update your system first, then press Check for Updates again.").arg(version)
            : QStringLiteral("Arch Dock %1 needs newer system parts (%2). Update your system first, then press Check for Updates again.")
                  .arg(version, parts.join(QStringLiteral(", ")));
    }
    if (code == QLatin1String("L9"))
        return QStringLiteral("Arch Dock %1 needs %2, which is not installed. Install it first, then press Check for Updates again.")
            .arg(version, m_found.notInstalled.join(QStringLiteral(", ")));
    if (code == QLatin1String("L20"))
        return QStringLiteral("Arch Dock %1 or newer is already installed. Nothing was changed.").arg(version);
    if (code == QLatin1String("L23"))
        return QStringLiteral("The install finished, but Arch Dock %1 is installed now, not %2. Press Check for Updates.")
            .arg(installedArchDock(runner(), m_env.toolFolder).value_or(QString()), version);
    if (code == QLatin1String("L24"))
        return QStringLiteral("Arch Dock %1 is installed, but the restart did not start. Press Restart Arch Dock, or log out and in.").arg(to);
    if (code == QLatin1String("L25"))
        return QStringLiteral("Arch Dock %1 is installed. Log out and in to finish the update.").arg(to);
    if (code == QLatin1String("L26"))
        return QStringLiteral("Arch Dock %1 is installed, but it did not start again. Open Arch Dock from the application menu. If it still does not start, log out and in.").arg(to);
    if (code == QLatin1String("L30"))
        return QStringLiteral("Arch Dock %1 is installed. Finish the preview, the profile or the panel change first, then press Restart Arch Dock.").arg(to);
    if (code == QLatin1String("L31"))
        return UpdateHold::refusal();
    return {};
}

void ArchDockUpdates::setState(const QString &state, const QString &line, const QString &details)
{
    m_state = state;
    const bool code = line.startsWith(u'L') && line.size() <= 3;
    m_lineCode = code ? line : QString();
    m_line = code ? lineText(line) : line;
    m_details = details;
    if (state != QLatin1String("updating"))
        m_step.clear();
    Q_EMIT changed();
}

void ArchDockUpdates::endUpdate(const QString &state, const QString &line, const QString &details)
{
    m_updateRunning = false;
    m_input.clear();
    UpdateHold::release();
    setState(state, line, details);
    quitAfterStop();
}

void ArchDockUpdates::quitAfterStop()
{
    // Before "started" a Quit waits for the Stop; the update has ended, so it
    // goes on (after "started" passwordAccepted() refused it already).
    if (m_afterStop)
        std::exchange(m_afterStop, {})(false);
}

// The last answer counts for a day. One dated in the future (the clock was
// set back since) counts as old.
bool ArchDockUpdates::answerFresh() const
{
    if (!m_lastAnswer.isValid())
        return false;
    const qint64 age = m_lastAnswer.msecsTo(QDateTime::currentDateTimeUtc());
    return age >= 0 && age < DayMs;
}

void ArchDockUpdates::pageOpened()
{
    checkRestartNeeded();
    if (m_checkAutomatically && m_state == QLatin1String("not-looked") && !answerFresh())
        check();
}

void ArchDockUpdates::studioOpened()
{
    checkRestartNeeded();
}

void ArchDockUpdates::check()
{
    if (!m_offered || m_updateRunning || m_restarting || !m_source || m_source->busy())
        return;
    // A restart still to do stays on the page while Arch Dock looks.
    if (m_state != QLatin1String("finish"))
        setState(QStringLiteral("looking"));
    m_source->look();
}

void ArchDockUpdates::lookedAt(const LookResult &result)
{
    const bool partOfUpdate = m_updateRunning;
    if (result.outcome != LookResult::Outcome::Problem || result.line == QLatin1String("L3"))
    {
        m_lastAnswer = QDateTime::currentDateTimeUtc();
        QSettings().setValue(QStringLiteral("updates/lastAnswer"), m_lastAnswer);
    }
    m_retryAfter = result.retryAfter;
    if (partOfUpdate)
    {
        // Update Now asks again: a release withdrawn or replaced since is
        // never installed, and a Stop pressed while it asked ends the update.
        if (m_stopRequested || result.outcome != LookResult::Outcome::Found
            || result.manifest.version != m_found.manifest.version)
        {
            m_updateRunning = false;
            m_input.clear();
            UpdateHold::release();
            lookedAt(result);
            return quitAfterStop();
        }
        m_found = result;
        keepVerifiedRelease();
        QString line;
        if (lockSeen(&line))
            return endUpdate(QStringLiteral("problem"), line, line == QLatin1String("L15") ? lockSteps() : QString());
        if (!roomFor(m_found.manifest.package.size))
            return endUpdate(QStringLiteral("problem"), QStringLiteral("L12"));
        startDownload();
        return;
    }
    // A restart still to do outweighs a look: Finish stays unless a newer
    // release was found, whose update restarts everything anyway.
    if (m_state == QLatin1String("finish") && result.outcome != LookResult::Outcome::Found)
        return Q_EMIT changed();
    m_found = result;
    switch (result.outcome)
    {
    case LookResult::Outcome::UpToDate:
        setState(QStringLiteral("up-to-date"));
        break;
    case LookResult::Outcome::Found:
        // A release the system cannot take yet is a problem with its own way
        // out: Try Again looks again once the system is ready.
        if (!result.tooOld.isEmpty())
            setState(QStringLiteral("problem"), QStringLiteral("L8"), result.tooOld.join(u'\n'));
        else if (!result.notInstalled.isEmpty())
            setState(QStringLiteral("problem"), QStringLiteral("L9"), result.notInstalled.join(u'\n'));
        else
            setState(QStringLiteral("found"));
        if (m_hooks.studioOpen && !m_hooks.studioOpen() && m_checkAutomatically)
        {
            QSettings settings;
            if (settings.value(QStringLiteral("updates/notifiedVersion")).toString() != result.manifest.version)
            {
                settings.setValue(QStringLiteral("updates/notifiedVersion"), result.manifest.version);
                QStringList titles = noteTitles();
                const QString text = titles.isEmpty() ? QString()
                    : titles.size() == 1 ? titles.first()
                                         : QStringLiteral("%1 and %2 more fixes.").arg(titles.first()).arg(titles.size() - 1);
                notify(QStringLiteral("UpdateFound"), QStringLiteral("Arch Dock %1 is ready").arg(result.manifest.version), text,
                       QStringLiteral("Update…"));
            }
        }
        break;
    case LookResult::Outcome::InstallByHand:
        setState(QStringLiteral("problem"), QStringLiteral("L7"), result.detail);
        break;
    case LookResult::Outcome::Problem:
        setState(QStringLiteral("problem"), result.line, result.detail);
        break;
    }
    // Restart needed outweighs "up to date" and a problem.
    if (m_state != QLatin1String("found"))
        checkRestartNeeded();
}

void ArchDockUpdates::dailyTick()
{
    checkRestartNeeded();
    if (!m_checkAutomatically || m_updateRunning || m_restarting)
        return;
    if (answerFresh())
        return;
    if (m_retryAfter.isValid() && QDateTime::currentDateTimeUtc() < m_retryAfter)
        return;
    check();
}

void ArchDockUpdates::checkRestartNeeded()
{
    if (!m_offered || m_updateRunning || m_restarting)
        return;
    if (m_state == QLatin1String("finish") || m_state == QLatin1String("done") || m_state == QLatin1String("restarting"))
        return;
    const std::optional<QString> installed = installedArchDock(runner(), m_env.toolFolder);
    if (!installed)
        return;
    const std::optional<int> order = compareVersions(runner(), m_env.toolFolder, *installed, m_env.release);
    if ((order && *order > 0) || IntentionalStop::executableReplaced())
    {
        m_restartTo = *installed;
        if (m_state != QLatin1String("restart-needed"))
            setState(QStringLiteral("restart-needed"),
                     QStringLiteral("Arch Dock %1 is installed, but this account still runs %2.").arg(*installed, m_env.release));
        notifyOncePerLogin(QStringLiteral("restart"), *installed, QStringLiteral("RestartNeeded"),
                           QStringLiteral("Arch Dock was updated"),
                           QStringLiteral("Restart it to use %1. Arch Dock and Plasma's panels restart; no password.").arg(*installed));
    }
}

bool ArchDockUpdates::guardsAllow(QString *line) const
{
    if (m_hooks.previewActive && m_hooks.previewActive())
    {
        *line = QStringLiteral("L28");
        return false;
    }
    if (m_hooks.profileBusy && m_hooks.profileBusy())
    {
        *line = QStringLiteral("L29");
        return false;
    }
    return true;
}

void ArchDockUpdates::update()
{
    if (!canUpdate() || m_updateRunning)
        return;
    // 1. The guards first; nothing has used the network yet.
    QString line;
    if (!guardsAllow(&line))
        return setState(QStringLiteral("found"), line);
    UpdateHold::take();
    m_updateRunning = true;
    m_started = false;
    m_stopRequested = false;
    m_received = 0;
    m_helperOutput.clear();
    m_step = QStringLiteral("download");
    m_state = QStringLiteral("updating");
    m_line.clear();
    Q_EMIT changed();
    // 2. GitHub once more; the rest follows in lookedAt.
    m_source->look();
}

QString ArchDockUpdates::lockSteps() const
{
    return QStringLiteral(
        "An earlier software install was cut off (for example by a power cut) and left pacman's lock file behind. "
        "Until it is removed, no software can be installed, not even with sudo pacman -Syu.\n"
        "1. First make sure nothing is installing software: close Discover and any Konsole window that is installing.\n"
        "2. Paste this line into Konsole and type your password: sudo rm /var/lib/pacman/db.lck\n"
        "3. Run your usual system update (sudo pacman -Syu) before pressing Update Now again.");
}

bool ArchDockUpdates::lockSeen(QString *line) const
{
    // This account cannot see who holds pacman's lock; the helper decides for real.
    if (!QFileInfo::exists(m_env.pacmanLock))
        return false;
    *line = running(QStringLiteral("pacman"), false) || running(QStringLiteral("packagekitd"), false)
        ? QStringLiteral("L14") : QStringLiteral("L15");
    return true;
}

bool ArchDockUpdates::roomFor(qint64 size) const
{
    QDir().mkpath(cacheFolder());
    const QStorageInfo cache(cacheFolder());
    const QStorageInfo run(QStringLiteral("/run"));
    return cache.bytesAvailable() >= size + ExtraRoom && run.bytesAvailable() >= size + ExtraRoom / 4;
}

void ArchDockUpdates::startDownload()
{
    m_step = QStringLiteral("download");
    Q_EMIT changed();
    m_packagePath = cacheFolder() + QLatin1Char('/') + m_found.manifest.package.file;
    const QUrl url(assetAddress(m_env.downloads, m_env.repository, m_found.manifest.releaseTag, m_found.manifest.package.file));
    m_download->start(url, m_packagePath, m_found.manifest.package.size, m_found.manifest.package.sha256,
                      QByteArrayLiteral("ArchDock/") + m_env.release.toLatin1());
}

void ArchDockUpdates::downloaded(UpdateDownload::Result result, const QString &details)
{
    if (m_stopRequested || result == UpdateDownload::Result::Stopped)
        return endUpdate(QStringLiteral("found"));
    if (result != UpdateDownload::Result::Done)
    {
        const QString line = result == UpdateDownload::Result::WrongBytes ? QStringLiteral("L11")
            : result == UpdateDownload::Result::NoSpace                   ? QStringLiteral("L12")
            : result == UpdateDownload::Result::NoConnection              ? QStringLiteral("L1")
                                                                          : QStringLiteral("L10");
        return endUpdate(QStringLiteral("problem"), line, details);
    }
    m_step = QStringLiteral("check");
    Q_EMIT changed();
    // The check runs pacman and copies the settings while Arch Dock waits, so
    // the page first gets a moment to show the Check step.
    QTimer::singleShot(CheckShownMs, this, &ArchDockUpdates::checkDownload);
}

void ArchDockUpdates::checkDownload()
{
    if (m_stopRequested)
        return endUpdate(QStringLiteral("found"));
    // 6. What pacman would do with this file.
    const auto printed = runner().run(m_env.toolFolder + QStringLiteral("/pacman"),
                                      {QStringLiteral("-U"), QStringLiteral("--print"), QStringLiteral("--print-format"),
                                       QStringLiteral("%n %v %a"), QStringLiteral("--"), m_packagePath});
    if (!printed.started || printed.exitCode != 0
        || QString::fromUtf8(printed.output).trimmed() != QStringLiteral("arch-dock %1 x86_64").arg(m_found.manifest.version))
        return endUpdate(QStringLiteral("problem"), QStringLiteral("L8"), QString::fromUtf8(printed.output + printed.errors));
    // 7. One backup per new version.
    if (!backup())
        return endUpdate(QStringLiteral("problem"), QStringLiteral("L13"));
    // 8. The marker, before the password window.
    const QJsonObject marker{{QStringLiteral("format"), 1}, {QStringLiteral("from"), m_env.release},
                             {QStringLiteral("to"), m_found.manifest.version}, {QStringLiteral("phase"), QStringLiteral("installing")},
                             {QStringLiteral("session"), IntentionalStop::sessionIdentity()}};
    writeRuntimeJson(markerPath(), marker);
    // 9. The lock once more, then the password window.
    QString line;
    if (lockSeen(&line))
    {
        QFile::remove(markerPath());
        return endUpdate(QStringLiteral("problem"), line, line == QLatin1String("L15") ? lockSteps() : QString());
    }
    startPkexec();
}

bool ArchDockUpdates::backup()
{
    QSettings settings;
    const QStringList kept = settings.value(QStringLiteral("updates/backupFor")).toString().split(u' ');
    ConfigurationBackup backups;
    if (kept.size() == 2 && kept.at(0) == m_found.manifest.version && backups.backups().contains(kept.at(1)))
        return true;
    QString error;
    const QString id = backups.capture(QStringLiteral("before-update-%1").arg(m_found.manifest.version), &error);
    if (id.isEmpty())
        return false;
    // No prune here: the update never pushes out other copies.
    settings.setValue(QStringLiteral("updates/backupFor"), m_found.manifest.version + QLatin1Char(' ') + id);
    return true;
}

void ArchDockUpdates::startPkexec()
{
    const QStringList arguments{QStringLiteral("--disable-internal-agent"), m_env.helperPath};
    if (!BackendCommandRunner::allowed(m_env.pkexec, arguments))
    {
        QFile::remove(markerPath());
        return endUpdate(QStringLiteral("problem"), QStringLiteral("L22"), QStringLiteral("this build does not install updates"));
    }
    QFile package(m_packagePath);
    if (!package.open(QIODevice::ReadOnly))
    {
        QFile::remove(markerPath());
        return endUpdate(QStringLiteral("problem"), QStringLiteral("L10"));
    }
    m_input = m_found.manifestBytes.toBase64() + '\n' + m_found.signatureBytes.toBase64() + '\n' + package.readAll();
    package.close();
    m_sent = 0;
    m_pkexecErrors.clear();
    m_pkexecOutput.clear();
    m_step = QStringLiteral("password");
    Q_EMIT changed();
    // Started from the main thread: pkexec asks the kernel to end it when the
    // thread that started it ends.
    m_pkexec = new QProcess(this);
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin"));
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    m_pkexec->setProcessEnvironment(environment);
    m_pkexec->setProgram(m_env.pkexec);
    m_pkexec->setArguments(arguments);
    connect(m_pkexec, &QProcess::readyReadStandardOutput, this, &ArchDockUpdates::pkexecOutput);
    connect(m_pkexec, &QProcess::readyReadStandardError, this, [this] { m_pkexecErrors += m_pkexec->readAllStandardError(); });
    connect(m_pkexec, &QProcess::bytesWritten, this, [this](qint64)
    {
        // At most 1 MiB pending; the input closes right after the last byte.
        while (m_pkexec && !m_stopRequested && m_sent < m_input.size() && m_pkexec->bytesToWrite() < PendingInputLimit)
        {
            const qint64 chunk = qMin<qint64>(256 * 1024, m_input.size() - m_sent);
            m_pkexec->write(m_input.constData() + m_sent, chunk);
            m_sent += chunk;
        }
        if (m_pkexec && m_sent >= m_input.size() && m_pkexec->bytesToWrite() == 0)
            m_pkexec->closeWriteChannel();
    });
    connect(m_pkexec, &QProcess::finished, this, &ArchDockUpdates::pkexecEnded);
    connect(m_pkexec, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
    {
        if (error == QProcess::FailedToStart)
            pkexecEnded(127, QProcess::NormalExit);
    });
    m_pkexec->start();
    // A start that fails inside start() (no pipe, no fork) has ended the
    // update already, through errorOccurred.
    if (!m_pkexec)
        return;
    const qint64 first = qMin<qint64>(PendingInputLimit, m_input.size());
    m_pkexec->write(m_input.constData(), first);
    m_sent = first;
}

void ArchDockUpdates::pkexecOutput()
{
    if (!m_pkexec)
        return;
    const QByteArray got = m_pkexec->readAllStandardOutput();
    m_helperOutput += got;
    if (m_helperOutput.size() > HelperOutputKept)
        m_helperOutput = m_helperOutput.right(HelperOutputKept);
    const QByteArray seen = m_pkexecOutput + got;
    if (!m_started && seen.contains("started\n"))
        passwordAccepted();
    m_pkexecOutput = seen.right(16);
}

QString ArchDockUpdates::helperDetails(const QString &errors) const
{
    QByteArray passed = m_helperOutput;
    if (passed.startsWith("started\n"))
        passed.remove(0, 8);
    const QString lines = QString::fromUtf8(passed).trimmed();
    if (lines.isEmpty())
        return errors.trimmed();
    return errors.trimmed().isEmpty() ? lines : errors.trimmed() + u'\n' + lines;
}

void ArchDockUpdates::passwordAccepted()
{
    // No Stop and no time limit from here, and a Quit that waited for a
    // Stop is refused.
    m_started = true;
    m_state = QStringLiteral("updating");
    m_step = QStringLiteral("installing");
    m_line.clear();
    m_lineCode.clear();
    Q_EMIT changed();
    if (m_afterStop)
        std::exchange(m_afterStop, {})(true);
}

void ArchDockUpdates::stop()
{
    if (!m_updateRunning || m_started)
        return;
    m_stopRequested = true;
    if (m_download && m_download->running())
        return m_download->stop();
    // While Update Now looks on GitHub again, or the check waits to run,
    // nothing runs that could be ended: the look or the check sees the Stop
    // and ends the update quietly.
    if (!m_pkexec)
        return setState(QStringLiteral("stopping"), QStringLiteral("Stopping the update…"));
    // Stop sending, close pkexec's input, TERM, then wait for its end.
    m_pkexec->closeWriteChannel();
    setState(QStringLiteral("stopping"), QStringLiteral("Stopping the update…"));
    const qint64 pid = m_pkexec->processId();
    if (pid <= 0)
    {
        // Not started yet: nothing has run, and nothing to signal.
        m_pkexec->kill();
        return;
    }
    const int answer = m_env.signalProcess ? m_env.signalProcess(pid, SIGTERM)
                                           : (::kill(static_cast<pid_t>(pid), SIGTERM) == 0 ? 0 : errno);
    // pkexec already runs as root: the password was accepted.
    if (answer == EPERM)
        passwordAccepted();
}

void ArchDockUpdates::studioClosing()
{
    if (m_updateRunning && !m_started)
        stop();
}

bool ArchDockUpdates::quitRefused() const
{
    return (m_updateRunning && m_started) || m_restarting;
}

void ArchDockUpdates::stopForQuit(std::function<void(bool refused)> then)
{
    if (quitRefused())
        return then(true);
    if (!m_updateRunning)
        return then(false);
    m_afterStop = std::move(then);
    stop();
}

void ArchDockUpdates::tellQuitRefused()
{
    notify(QStringLiteral("QuitRefused"), QStringLiteral("Arch Dock did not quit"), lineText(QStringLiteral("L32")), {}, true);
}

void ArchDockUpdates::pkexecEnded(int exitCode, QProcess::ExitStatus status)
{
    if (!m_pkexec)
        return;
    m_pkexecErrors += m_pkexec->readAllStandardError();
    pkexecOutput();
    const QString pkexecText = QString::fromUtf8(m_pkexecErrors);
    const bool stopped = m_stopRequested;
    m_pkexec->deleteLater();
    m_pkexec = nullptr;
    m_input.clear();
    endAfterHelper(status == QProcess::NormalExit ? exitCode : -1, pkexecText, stopped);
}

void ArchDockUpdates::waitForPacman(std::function<void()> then)
{
    if (!running(QStringLiteral("pacman"), false))
        return then();
    QTimer::singleShot(2000, this, [this, then] { waitForPacman(then); });
}

void ArchDockUpdates::endAfterHelper(int code, const QString &pkexecText, bool stopped)
{
    const auto newVersionInstalled = [this]
    {
        const auto now = installedArchDock(runner(), m_env.toolFolder);
        return now && *now == m_found.manifest.version;
    };
    const auto quietFound = [this]
    {
        QFile::remove(markerPath());
        endUpdate(QStringLiteral("found"));
    };
    const QString details = helperDetails(pkexecText);
    if (stopped && code != 0 && !newVersionInstalled())
        return quietFound();
    if (code == 0)
    {
        if (m_afterStop)
            std::exchange(m_afterStop, {})(true);
        return installed();
    }
    if (code == 126)
        return quietFound();
    if (code == 127)
    {
        QFile::remove(markerPath());
        return endUpdate(QStringLiteral("problem"),
                         pkexecText.contains(QLatin1String("Not authorized")) ? QStringLiteral("L16") : QStringLiteral("L17"), details);
    }
    static const QHash<int, QString> lines = {
        {10, QStringLiteral("L14")}, {11, QStringLiteral("L8")},  {12, QStringLiteral("L18")}, {13, QStringLiteral("L20")},
        {15, QStringLiteral("L23")}, {16, QStringLiteral("L19")}, {17, QStringLiteral("L12")}, {18, QStringLiteral("L15")},
        {20, QStringLiteral("L21")}};
    if (lines.contains(code))
    {
        const QString line = lines.value(code);
        return problemAfterPassword(line, line == QLatin1String("L15") ? lockSteps() : details);
    }
    // 14, 19, 21, 22, an unknown code or a crash: what pacman leaves behind decides.
    waitForPacman([this, newVersionInstalled, details]
    {
        if (newVersionInstalled())
            return installed();
        problemAfterPassword(QStringLiteral("L22"), details);
    });
}

void ArchDockUpdates::problemAfterPassword(const QString &line, const QString &details)
{
    QFile::remove(markerPath());
    const bool started = m_started;
    endUpdate(QStringLiteral("problem"), line, details);
    if (started && m_hooks.studioOpen && !m_hooks.studioOpen())
        notify(QStringLiteral("UpdateProblem"), QStringLiteral("Arch Dock was not updated"), m_line, QStringLiteral("Open Updates"));
}

void ArchDockUpdates::installed()
{
    const auto now = installedArchDock(runner(), m_env.toolFolder);
    if (!now || *now != m_found.manifest.version)
    {
        m_restartTo = now.value_or(QString());
        return endUpdate(QStringLiteral("restart-needed"),
                         QStringLiteral("Arch Dock %1 is installed, but this account still runs %2.").arg(m_restartTo, m_env.release));
    }
    m_restartTo = *now;
    QJsonObject marker{{QStringLiteral("format"), 1}, {QStringLiteral("from"), m_env.release}, {QStringLiteral("to"), *now},
                       {QStringLiteral("phase"), QStringLiteral("installed")},
                       {QStringLiteral("session"), IntentionalStop::sessionIdentity()}};
    writeRuntimeJson(markerPath(), marker);
    // Installed: the page shows the Restart step while the restart waits.
    m_step = QStringLiteral("restart");
    Q_EMIT changed();
    startFinisher(*now);
}

void ArchDockUpdates::restartArchDock()
{
    if (!m_offered || m_updateRunning || m_restarting)
        return;
    const auto now = installedArchDock(runner(), m_env.toolFolder);
    if (!now)
        return;
    QString line;
    if (!guardsAllow(&line))
    {
        setState(QStringLiteral("restart-needed"), QStringLiteral("L30"));
        if (m_hooks.studioOpen && !m_hooks.studioOpen())
            notify(QStringLiteral("UpdateProblem"), QStringLiteral("Arch Dock was not updated"), m_line, QStringLiteral("Open Updates"));
        return;
    }
    // From the press on the restart is under way: Studio holds its settings,
    // a second press, a look and Quit wait for it.
    UpdateHold::take();
    m_restarting = true;
    m_restartTo = *now;
    QJsonObject marker{{QStringLiteral("format"), 1}, {QStringLiteral("from"), m_env.release}, {QStringLiteral("to"), *now},
                       {QStringLiteral("phase"), QStringLiteral("installed")},
                       {QStringLiteral("session"), IntentionalStop::sessionIdentity()}};
    writeRuntimeJson(markerPath(), marker);
    QFile::remove(resultPath());
    setState(QStringLiteral("restarting"), QStringLiteral("Restarting Arch Dock and Plasma's panels…"));
    startFinisher(*now);
}

void ArchDockUpdates::startFinisher(const QString &version)
{
    // The guards once more, with Studio's draft and the changes still under
    // way, which are waited for (up to 30 s). Until the restart has an
    // answer it is under way.
    m_restarting = true;
    constexpr int StepMs = 1000;
    // Each waiting step holds the attempt; the attempt only looks itself up,
    // so it goes once nothing waits any more.
    const auto attempt = std::make_shared<std::function<void(int)>>();
    *attempt = [this, version, self = std::weak_ptr<std::function<void(int)>>(attempt)](int waited)
    {
        QString line;
        const bool busy = !guardsAllow(&line) || (m_hooks.studioDraft && m_hooks.studioDraft())
            || UpdateHold::changesUnderWay() > 0;
        if (busy && waited * StepMs < m_env.waitLimitMs)
        {
            QTimer::singleShot(StepMs, this, [next = self.lock(), waited] { if (next) (*next)(waited + 1); });
            return;
        }
        if (busy)
        {
            m_restarting = false;
            return endUpdate(QStringLiteral("restart-needed"), QStringLiteral("L30"));
        }
        // systemd-run --user reaches this login's service manager through
        // XDG_RUNTIME_DIR (or the session bus); the fixed environment alone
        // has neither. Each restart gets a unit name of its own.
        QStringList session;
        if (!m_env.runtimeFolder.isEmpty())
            session.append(QStringLiteral("XDG_RUNTIME_DIR=") + m_env.runtimeFolder);
        if (const QString bus = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"); !bus.isEmpty())
            session.append(QStringLiteral("DBUS_SESSION_BUS_ADDRESS=") + bus);
        const QString unit = QStringLiteral("arch-dock-finish-update-%1-%2")
                                 .arg(QDateTime::currentMSecsSinceEpoch())
                                 .arg(++m_finisherRuns);
        const auto started = runner().runInSession(m_env.systemdRun,
            {QStringLiteral("--user"), QStringLiteral("--collect"), QStringLiteral("--quiet"), QStringLiteral("--unit=") + unit,
             QStringLiteral("-E"), QStringLiteral("PATH=/usr/bin"), QStringLiteral("--"), m_env.finisherPath, version,
             QString::number(::getpid())}, session);
        if (!started.started || started.exitCode != 0)
        {
            m_restarting = false;
            return endUpdate(QStringLiteral("finish"), QStringLiteral("L24"), QString::fromUtf8(started.errors));
        }
        setState(QStringLiteral("restarting"), QStringLiteral("Restarting Arch Dock and Plasma's panels…"));
        m_restartClock.start();
        m_resultWatch.start();
    };
    (*attempt)(0);
}

void ArchDockUpdates::watchResult(const QString &version)
{
    // The finisher stops this process with TERM before it writes anything
    // but not-installed or log-out-needed. So any status while this process
    // still runs, or none within 30 s, leaves "Restarting…".
    const auto result = readRuntimeJson(resultPath());
    const QString status = result && result->value(QStringLiteral("version")).toString() == version
        ? result->value(QStringLiteral("status")).toString() : QString();
    // Measured on a clock that a clock change or a suspend does not move.
    if (status.isEmpty() && m_restartClock.elapsed() < m_env.waitLimitMs)
        return;
    m_resultWatch.stop();
    m_restarting = false;
    endUpdate(QStringLiteral("finish"), status == QLatin1String("log-out-needed") ? QStringLiteral("L25") : QStringLiteral("L24"));
}

void ArchDockUpdates::readAfterRestart()
{
    std::optional<QJsonObject> marker = readRuntimeJson(markerPath());
    if (!marker)
        return;
    const QString to = marker->value(QStringLiteral("to")).toString();
    const QString phase = marker->value(QStringLiteral("phase")).toString();
    m_restartTo = to;
    m_found.manifest.version = to;
    const auto cleanUp = [this]
    {
        QFile::remove(markerPath());
        QFile::remove(resultPath());
        QFile::remove(keptFile(QStringLiteral("installing.json")));
        QFile::remove(keptFile(QStringLiteral("installing.sig")));
        QDir cache(cacheFolder());
        for (const QString &old : cache.entryList({QStringLiteral("*.pkg.tar.zst")}, QDir::Files))
            cache.remove(old);
    };
    // 1. Another login session: a new login already runs everything new.
    if (marker->value(QStringLiteral("session")).toString() != IntentionalStop::sessionIdentity())
        return cleanUp();
    const auto installedNow = installedArchDock(runner(), m_env.toolFolder);
    if (phase == QLatin1String("installing"))
    {
        // 2. The root helper or a pacman still runs: Installing, checked again.
        if (running(QStringLiteral("arch-dock-insta"), false) || running(QStringLiteral("pacman"), false))
        {
            m_updateRunning = true;
            m_started = true;
            m_state = QStringLiteral("updating");
            m_step = QStringLiteral("installing");
            Q_EMIT changed();
            QTimer::singleShot(3000, this, [this]
            {
                m_updateRunning = false;
                m_started = false;
                readAfterRestart();
                // Nothing was installed after all, or the marker went: the
                // card must not keep saying "Installing…".
                if (!m_updateRunning && m_state == QLatin1String("updating"))
                {
                    setState(QStringLiteral("not-looked"));
                    checkRestartNeeded();
                }
            });
            return;
        }
        // 3. It finished, but its Arch Dock ended before the restart: Plasma
        // still has the old widgets loaded.
        if (installedNow && *installedNow == to)
        {
            setState(QStringLiteral("finish"),
                     QStringLiteral("Arch Dock %1 was installed, but Arch Dock and Plasma's panels were not restarted afterwards.").arg(to));
            notifyOncePerLogin(QStringLiteral("finish"), to, QStringLiteral("RestartNeeded"),
                               QStringLiteral("Arch Dock %1 was installed").arg(to),
                               QStringLiteral("Arch Dock and Plasma's panels were not restarted yet. Restart them to finish."));
            return;
        }
        // 4. Nothing was installed.
        return cleanUp();
    }
    if (phase != QLatin1String("installed"))
        return cleanUp();
    // 5. The finisher still runs.
    if (running(QStringLiteral("arch-dock-finis"), true))
    {
        m_restarting = true;
        setState(QStringLiteral("restarting"), QStringLiteral("Restarting Arch Dock and Plasma's panels…"));
        QTimer::singleShot(1000, this, [this] { m_restarting = false; readAfterRestart(); });
        return;
    }
    const auto result = readRuntimeJson(resultPath());
    const QString status = result && result->value(QStringLiteral("version")).toString() == to
        ? result->value(QStringLiteral("status")).toString() : QString();
    // 6. The finisher really restarted Plasma, and this is the new Arch Dock.
    static const QStringList restarted = {QStringLiteral("starting"), QStringLiteral("quit"), QStringLiteral("not-started"),
                                          QStringLiteral("done")};
    if (restarted.contains(status) && m_env.release == to)
    {
        loadKeptRelease(to);
        setState(QStringLiteral("done"), QStringLiteral("Arch Dock %1 is installed and running.").arg(to));
        cleanUp();
        if (m_hooks.openStudioOnUpdates)
            QTimer::singleShot(0, this, [this] { m_hooks.openStudioOnUpdates(); });
        return;
    }
    // 7. Plasma's panels did not come back.
    if (status == QLatin1String("plasma-not-back"))
    {
        setState(QStringLiteral("finish"), QStringLiteral("L27"));
        if (m_hooks.openStudioOnUpdates)
            QTimer::singleShot(0, this, [this] { m_hooks.openStudioOnUpdates(); });
        return;
    }
    // 8. Any other result, or none.
    setState(QStringLiteral("finish"),
             QStringLiteral("Arch Dock %1 was installed, but Arch Dock and Plasma's panels were not restarted afterwards.").arg(to));
    notifyOncePerLogin(QStringLiteral("finish"), to, QStringLiteral("RestartNeeded"), QStringLiteral("Arch Dock %1 was installed").arg(to),
                       QStringLiteral("Arch Dock and Plasma's panels were not restarted yet. Restart them to finish."));
}

QString ArchDockUpdates::keptFile(const QString &name) const
{
    return QDir(stateFolder()).filePath(name);
}

void ArchDockUpdates::keepVerifiedRelease()
{
    // In the state folder, which is this account's alone (0700); the next
    // start checks the signature again before it shows anything from it.
    const auto keep = [](const QString &path, const QByteArray &bytes)
    {
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
            && file.write(bytes) == bytes.size() && file.commit();
    };
    keep(keptFile(QStringLiteral("installing.json")), m_found.manifestBytes);
    keep(keptFile(QStringLiteral("installing.sig")), m_found.signatureBytes);
}

void ArchDockUpdates::loadKeptRelease(const QString &version)
{
    QFile manifest(keptFile(QStringLiteral("installing.json")));
    QFile signature(keptFile(QStringLiteral("installing.sig")));
    if (!manifest.open(QIODevice::ReadOnly) || !signature.open(QIODevice::ReadOnly))
        return;
    const ManifestReading reading = readVerifiedManifest(runner(), m_env.toolFolder, m_env.keyId, m_env.keySpki,
                                                         manifest.read(MaxManifestBytes + 1), signature.read(MaxSignatureBytes + 1),
                                                         m_env.testBuild, stateFolder());
    if (reading.manifest && reading.manifest->version == version)
        m_found.manifest = *reading.manifest;
}

void ArchDockUpdates::notify(const QString &event, const QString &title, const QString &text, const QString &action,
                             bool whileStudioOpen)
{
    // Notices appear only while Studio is closed, except one that answers
    // something the owner did outside it.
    if (!whileStudioOpen && m_hooks.studioOpen && m_hooks.studioOpen())
        return;
    if (m_env.notice)
        return m_env.notice(event, title, text, action);
    auto *notification = new KNotification(event, KNotification::CloseOnTimeout);
    notification->setComponentName(QStringLiteral("arch-dock"));
    notification->setTitle(title);
    notification->setText(text);
    if (!action.isEmpty())
    {
        KNotificationAction *button = notification->addAction(action);
        connect(button, &KNotificationAction::activated, this, [this, event]
        {
            if (event == QLatin1String("RestartNeeded"))
                restartArchDock();
            else if (m_hooks.openStudioOnUpdates)
                m_hooks.openStudioOnUpdates();
        });
    }
    notification->sendEvent();
}

void ArchDockUpdates::notifyOncePerLogin(const QString &kind, const QString &version, const QString &event,
                                         const QString &title, const QString &text)
{
    // Remembered in the runtime folder, which a logout clears: neither the
    // hourly tick nor an account switch repeats it.
    const QString path = runtimeFile(QStringLiteral("arch-dock-update-notices.json"));
    QJsonObject told = readRuntimeJson(path).value_or(QJsonObject());
    if (told.value(kind).toString() == version)
        return;
    told.insert(kind, version);
    writeRuntimeJson(path, told);
    notify(event, title, text, QStringLiteral("Restart Arch Dock"));
}
} // namespace ArchDock::Updates
