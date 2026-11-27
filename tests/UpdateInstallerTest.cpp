// SPDX-License-Identifier: GPL-3.0-or-later
// The Update button's whole flow (src/updates/ArchDockUpdates) with
// stand-ins: a local server plays GitHub; a script plays pkexec and the root
// helper and records what it is given; the runner answers for pacman and
// systemd-run, while vercmp and openssl are the real ones (they only
// compute). The processes the updater looks for, the stop signal and the
// notices are stand-ins too, so nothing here reaches the password window,
// pacman, systemd or the desktop.

#include "ArchDockVersion.h"
#include "IntentionalStop.h"
#include "persistence/ConfigurationBackup.h"
#include "updates/ArchDockUpdates.h"
#include "updates/CommandRunner.h"
#include "updates/ReleaseSource.h"
#include "updates/UpdateDownload.h"
#include "updates/UpdateHold.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <memory>
#include <unistd.h>
#include <utility>

using namespace ArchDock::Updates;
using ArchDock::ConfigurationBackup;

namespace
{
const QString Running = QStringLiteral("0.1.1-55");
const QString Newer = QStringLiteral("0.1.1-56");
const QString Repository = QStringLiteral("aivars11232/Arch-Dock");
const QString Helper = QStringLiteral("/usr/lib/arch-dock/arch-dock-install-update");
const QString Finisher = QStringLiteral("/usr/lib/arch-dock/arch-dock-finish-update");

// pkexec and the root helper in one script. It records its arguments, its
// environment as it was started, the marker at its start and its standard
// input, then follows the steps of its plan file:
//   wait:<tenths of a second>, started, split-started (in two pieces),
//   read, install:<version> (what
//   `pacman -Q` shows from then on), cut:<code> (exits so when the input
//   was cut), pacman-error (a line of pacman's, passed on to the output),
//   exit:<code>, killed.
const char StandIn[] = R"script(#!/bin/sh
set -f
dir=${0%/*}
. "$dir/plan"
tr '\0' '\n' < /proc/$$/environ > "$dir/pkexec-environment"
: > "$dir/pkexec-arguments"
for argument in "$@"; do printf '%s\n' "$argument" >> "$dir/pkexec-arguments"; done
[ -f "$marker" ] && cat "$marker" > "$dir/marker-at-start"
[ -n "$errors" ] && printf '%s\n' "$errors" >&2
for step in $steps; do
    case $step in
    wait:*) n=${step#wait:}; while [ "$n" -gt 0 ]; do sleep 0.1; n=$((n - 1)); done ;;
    started) echo started ;;
    split-started) printf sta; sleep 0.3; printf 'rted\n' ;;
    read) cat > "$dir/pkexec-input"; : > "$dir/read-done" ;;
    install:*) printf '%s\n' "${step#install:}" > "$dir/installed" ;;
    pacman-error) echo "error: failed to commit transaction (conflicting files)" ;;
    cut:*) [ "$(wc -c < "$dir/pkexec-input")" -lt "$full" ] && exit "${step#cut:}" ;;
    exit:*) exit "${step#exit:}" ;;
    killed) kill -KILL $$ ;;
    esac
done
exit 0
)script";

QByteArray readBytes(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QJsonObject json(const QByteArray &bytes)
{
    return QJsonDocument::fromJson(bytes).object();
}

void writeJson(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    file.close();
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
}

QString packageFile(const QString &version)
{
    return QStringLiteral("arch-dock-%1-x86_64.pkg.tar.zst").arg(version);
}

// GitHub: the newest release and its files, one request per connection.
class GitHubStandIn final : public QTcpServer
{
public:
    GitHubStandIn()
    {
        connect(this, &QTcpServer::newConnection, this, [this]
        {
            while (QTcpSocket *socket = nextPendingConnection())
            {
                ++connections;
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, this, [this, socket]
                {
                    QByteArray &request = m_requests[socket];
                    request += socket->readAll();
                    if (!request.contains("\r\n\r\n"))
                        return;
                    const QByteArray path = request.split(' ').value(1);
                    m_requests.remove(socket);
                    requests.append(QString::fromLatin1(path));
                    if (held.contains(path))
                        m_held.append({QPointer<QTcpSocket>(socket), path});
                    else
                        answer(socket, path);
                });
            }
        });
        listen(QHostAddress::LocalHost);
    }

    QString address() const { return QStringLiteral("http://127.0.0.1:%1").arg(serverPort()); }

    void reset()
    {
        latestStatus = "404 Not Found";
        latest.clear();
        files.clear();
        requests.clear();
        onRequest = {};
        held.clear();
        m_held.clear();
    }

    // Whether a request for `path` waits for releaseHeld().
    bool waiting(const QByteArray &path) const
    {
        for (const auto &[socket, heldPath] : m_held)
            if (socket && heldPath == path)
                return true;
        return false;
    }

    void releaseHeld()
    {
        const auto waitingNow = std::exchange(m_held, {});
        for (const auto &[socket, path] : waitingNow)
            if (socket)
                answer(socket, path);
    }

    QByteArray latestStatus = "404 Not Found";
    QByteArray latest;
    QHash<QByteArray, QByteArray> files;
    std::function<void(const QByteArray &path)> onRequest;
    QStringList requests;
    // Requests answered only when released: a look or a download under way.
    QSet<QByteArray> held;
    int connections = 0;

private:
    void answer(QTcpSocket *socket, const QByteArray &path)
    {
        if (onRequest)
            onRequest(path);
        QByteArray status = "404 Not Found";
        QByteArray body;
        if (path == "/repos/" + Repository.toLatin1() + "/releases/latest")
        {
            status = latestStatus;
            body = latest;
        }
        else if (files.contains(path))
        {
            status = "200 OK";
            body = files.value(path);
        }
        socket->write("HTTP/1.1 " + status + "\r\nContent-Length: " + QByteArray::number(body.size())
                      + "\r\nConnection: close\r\n\r\n" + body);
        socket->disconnectFromHost();
    }

    QHash<QTcpSocket *, QByteArray> m_requests;
    QList<std::pair<QPointer<QTcpSocket>, QByteArray>> m_held;
};

// pacman and systemd-run answer from the test; vercmp and openssl are real.
// Anything else is refused and recorded, so no other program ever runs.
class StandInRunner final : public CommandRunner
{
public:
    Result run(const QString &program, const QStringList &arguments, const QByteArray &input = {}) override
    {
        calls.append(QStringList{program} + arguments);
        Result result;
        result.started = true;
        result.exitCode = 0;
        const QString name = program.section(u'/', -1);
        const bool tool = program == tools + QLatin1Char('/') + name;
        if (tool && name == QLatin1String("pacman"))
        {
            if (arguments == QStringList{QStringLiteral("-Q"), QStringLiteral("arch-dock")})
            {
                const QString version = installed();
                result.exitCode = version.isEmpty() ? 1 : 0;
                result.output = version.isEmpty() ? QByteArray() : "arch-dock " + version.toUtf8() + '\n';
                return result;
            }
            if (arguments.value(0) == QLatin1String("-T"))
            {
                QStringList missing;
                for (const QString &need : arguments.mid(2))
                    if (unmet.contains(need) || (arguments.size() == 3 && absent.contains(need)))
                        missing.append(need);
                result.exitCode = missing.isEmpty() ? 0 : 127;
                result.output = missing.join(u'\n').toUtf8() + (missing.isEmpty() ? "" : "\n");
                return result;
            }
            if (arguments.value(0) == QLatin1String("-U") && arguments.contains(QStringLiteral("--print")))
            {
                result.output = printed.toUtf8() + '\n';
                return result;
            }
        }
        if (tool && (name == QLatin1String("vercmp") || name == QLatin1String("openssl")))
            return m_real.run(QStringLiteral("/usr/bin/") + name, arguments, input);
        result.started = false;
        unexpected.append(program + QLatin1Char(' ') + arguments.join(u' '));
        return result;
    }

    // systemd-run --user, which, like the real one, reaches the service
    // manager only with this login's XDG_RUNTIME_DIR (or its session bus).
    Result runInSession(const QString &program, const QStringList &arguments, const QStringList &session) override
    {
        calls.append(QStringList{program} + arguments);
        lastSession = session;
        Result result;
        if (program != systemdRun)
        {
            unexpected.append(program + QLatin1Char(' ') + arguments.join(u' '));
            return result;
        }
        result.started = true;
        const bool reachable = std::any_of(session.cbegin(), session.cend(), [](const QString &entry)
        {
            return (entry.startsWith(QLatin1String("XDG_RUNTIME_DIR=")) || entry.startsWith(QLatin1String("DBUS_SESSION_BUS_ADDRESS=")))
                && !entry.endsWith(u'=');
        });
        if (!reachable)
        {
            result.exitCode = 1;
            result.errors = "Failed to connect to user scope bus via local transport: $DBUS_SESSION_BUS_ADDRESS and "
                            "$XDG_RUNTIME_DIR not defined";
            return result;
        }
        if (onSystemdRun)
            onSystemdRun(arguments);
        result.exitCode = systemdRunExit;
        if (systemdRunExit != 0)
            result.errors = "Failed to start transient service unit";
        return result;
    }

    // What `pacman -Q arch-dock` shows: the stand-in helper's install, or the start value.
    QString installed() const
    {
        const QByteArray written = readBytes(installedFile).trimmed();
        return written.isEmpty() ? installedVersion : QString::fromUtf8(written);
    }

    int systemdRuns() const
    {
        int count = 0;
        for (const QStringList &call : calls)
            count += call.first() == systemdRun;
        return count;
    }

    QString tools;
    QString systemdRun;
    QString installedFile;
    QString installedVersion = Running;
    QString printed;
    QStringList unmet;  // needs `pacman -T` reports
    QStringList absent; // names not installed at all
    int systemdRunExit = 0;
    std::function<void(const QStringList &arguments)> onSystemdRun;
    QStringList lastSession;
    QList<QStringList> calls;
    QStringList unexpected;

private:
    SystemCommandRunner m_real;
};

QByteArray openssl(const QStringList &arguments)
{
    QProcess process;
    process.setStandardInputFile(QProcess::nullDevice());
    process.start(QStringLiteral("/usr/bin/openssl"), arguments);
    process.waitForFinished(30000);
    return process.exitCode() == 0 ? process.readAllStandardOutput() : QByteArray();
}

struct Release
{
    QString version;
    QByteArray manifest;
    QByteArray signature;
    QByteArray package;

    // What pkexec's standard input must carry: the two lines, then the package.
    QByteArray input() const { return manifest.toBase64() + '\n' + signature.toBase64() + '\n' + package; }
};
} // namespace

class UpdateInstallerTest : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_base;
    QString m_key;
    QByteArray m_spki;
    QJsonObject m_valid;
    std::unique_ptr<QTemporaryDir> m_folder;
    GitHubStandIn m_server;
    StandInRunner m_runner;
    QSet<QString> m_running;
    QStringList m_looked;
    QStringList m_notices;
    QList<int> m_signals;
    bool m_profileBusy = false;
    bool m_previewActive = false;
    bool m_studioOpen = true;
    bool m_studioDraft = false;
    int m_studioOpened = 0;

    QString path(const QString &name) const { return m_folder->filePath(name); }
    QString markerPath() const { return path(QStringLiteral("runtime/arch-dock-update.json")); }
    QString resultPath() const { return path(QStringLiteral("runtime/arch-dock-update-result.json")); }
    bool pkexecRan() const { return QFile::exists(path(QStringLiteral("bin/pkexec-arguments"))); }

    Release release(const QString &version, qint64 size = 3 * 1024 * 1024 + 77)
    {
        Release made;
        made.version = version;
        made.package.resize(size);
        for (qint64 at = 0; at < size; ++at)
            made.package[at] = char((at * 31 + 7) & 0xff);
        QJsonObject manifest = m_valid;
        manifest.insert(QStringLiteral("version"), version);
        manifest.insert(QStringLiteral("release_tag"), QStringLiteral("v") + version);
        QJsonObject package = manifest.value(QStringLiteral("package")).toObject();
        package.insert(QStringLiteral("file"), packageFile(version));
        package.insert(QStringLiteral("size"), size);
        package.insert(QStringLiteral("sha256"),
                       QString::fromLatin1(QCryptographicHash::hash(made.package, QCryptographicHash::Sha256).toHex()));
        manifest.insert(QStringLiteral("package"), package);
        made.manifest = QJsonDocument(manifest).toJson();
        QFile file(m_base->filePath(QStringLiteral("manifest")));
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(made.manifest);
        file.close();
        made.signature = openssl({QStringLiteral("dgst"), QStringLiteral("-sha256"), QStringLiteral("-sign"), m_key, file.fileName()});
        return made;
    }

    // GitHub's newest release from now on.
    void publish(const Release &made)
    {
        const QString tag = QStringLiteral("v") + made.version;
        const QByteArray base = '/' + Repository.toLatin1() + "/releases/download/" + tag.toLatin1() + '/';
        const auto asset = [](const QString &name, qint64 size)
        { return QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("size"), size}}; };
        m_server.files.clear();
        m_server.files.insert(base + "update-manifest.json", made.manifest);
        m_server.files.insert(base + "update-manifest.sig", made.signature);
        m_server.files.insert(base + packageFile(made.version).toLatin1(), made.package);
        m_server.latestStatus = "200 OK";
        m_server.latest = QJsonDocument(QJsonObject{
            {QStringLiteral("tag_name"), tag}, {QStringLiteral("draft"), false}, {QStringLiteral("prerelease"), false},
            {QStringLiteral("assets"), QJsonArray{asset(QStringLiteral("update-manifest.json"), made.manifest.size()),
                                                  asset(QStringLiteral("update-manifest.sig"), made.signature.size()),
                                                  asset(packageFile(made.version), made.package.size())}}})
                              .toJson(QJsonDocument::Compact);
        m_runner.printed = QStringLiteral("arch-dock %1 x86_64").arg(made.version);
    }

    // What the stand-in pkexec and helper do next time.
    void plan(const QString &steps, const QString &errors = {}, qint64 full = 0)
    {
        QFile file(path(QStringLiteral("bin/plan")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QStringLiteral("steps='%1'\nerrors='%2'\nmarker='%3'\nfull=%4\n")
                       .arg(steps, errors, markerPath())
                       .arg(full)
                       .toUtf8());
    }

    ArchDockUpdates::Environment environment()
    {
        ArchDockUpdates::Environment setup;
        setup.runner = &m_runner;
        setup.runtimeFolder = path(QStringLiteral("runtime"));
        setup.cacheFolder = path(QStringLiteral("cache"));
        setup.stateFolder = path(QStringLiteral("state"));
        setup.toolFolder = path(QStringLiteral("tools"));
        setup.pkexec = path(QStringLiteral("bin/stand-in-pkexec"));
        setup.systemdRun = path(QStringLiteral("tools/systemd-run"));
        setup.helperPath = Helper;
        setup.finisherPath = Finisher;
        setup.pacmanLock = path(QStringLiteral("db.lck"));
        setup.release = Running;
        setup.repository = Repository;
        setup.keyId = QStringLiteral("arch-dock-update-1");
        setup.keySpki = m_spki;
        setup.testBuild = true;
        setup.api = m_server.address();
        setup.downloads = m_server.address();
        setup.offered = true;
        setup.processRuns = [this](const QString &name, bool ownOnly)
        {
            m_looked.append(ownOnly ? name + QStringLiteral("@own") : name);
            return m_running.contains(name);
        };
        setup.notice = [this](const QString &event, const QString &title, const QString &text, const QString &action)
        { m_notices.append(QStringList{event, title, text, action}.join(QStringLiteral(" | "))); };
        setup.waitLimitMs = 1500;
        return setup;
    }

    std::unique_ptr<ArchDockUpdates> updater(ArchDockUpdates::Environment setup)
    {
        ArchDockUpdates::Hooks hooks;
        hooks.profileBusy = [this] { return m_profileBusy; };
        hooks.previewActive = [this] { return m_previewActive; };
        hooks.studioOpen = [this] { return m_studioOpen; };
        hooks.studioDraft = [this] { return m_studioDraft; };
        hooks.openStudioOnUpdates = [this] { ++m_studioOpened; };
        return std::make_unique<ArchDockUpdates>(hooks, std::move(setup));
    }

    std::unique_ptr<ArchDockUpdates> updater() { return updater(environment()); }

    static bool waitFor(const std::function<bool()> &condition, int ms = 10000)
    {
        return QTest::qWaitFor(condition, ms);
    }

    static bool found(const ArchDockUpdates &updates) { return updates.state() == QLatin1String("found"); }
    static bool installing(const ArchDockUpdates &updates)
    {
        return updates.state() == QLatin1String("updating") && updates.step() == QLatin1String("installing");
    }

    // A look that finds the published release.
    static bool findUpdate(ArchDockUpdates &updates)
    {
        updates.check();
        return waitFor([&] { return found(updates); }) && updates.canUpdate();
    }

    void writeMarker(const QString &phase, const QString &to, const QString &session = ArchDock::IntentionalStop::sessionIdentity())
    {
        writeJson(markerPath(), {{QStringLiteral("format"), 1}, {QStringLiteral("from"), Running}, {QStringLiteral("to"), to},
                                 {QStringLiteral("phase"), phase}, {QStringLiteral("session"), session}});
    }

    void writeResult(const QString &status, const QString &version)
    {
        writeJson(resultPath(), {{QStringLiteral("format"), 1}, {QStringLiteral("version"), version},
                                 {QStringLiteral("status"), status}, {QStringLiteral("at"), QStringLiteral("2026-11-27T12:00:00Z")}});
    }

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void theGuardsComeBeforeTheNetwork();
    void theLockStopsBeforeThePasswordWindow();
    void oneBackupPerVersion();
    void thePasswordWindowGetsExactlyItsInput();
    void noTimeLimitAfterStarted();
    void stopDuringTheCheck();
    void stopBeforeThePasswordIsAccepted_data();
    void stopBeforeThePasswordIsAccepted();
    void tooLateToStop_data();
    void tooLateToStop();
    void theHelpersCodes_data();
    void theHelpersCodes();
    void pacmanDecidesAfterAnUnclearEnd_data();
    void pacmanDecidesAfterAnUnclearEnd();
    void afterTheInstall_data();
    void afterTheInstall();
    void restartArchDockNeedsNoPassword();
    void theNextStart_data();
    void theNextStart();
    void noticesOncePerVersionAndLogin();
    void aBuildWithoutReleaseAndKeyStaysFenced();
    void theUpdaterNeverQuitsArchDock();
    void quitBeforeThePasswordEndsTheUpdate_data();
    void quitBeforeThePasswordEndsTheUpdate();
    void stopDuringTheSecondLook();
    void anInstallingCardDoesNotOutliveTheInstall();
    void aRestartIsUnderWayFromThePress();
    void aLookKeepsARestartToDo();
    void doneShowsWhatItFixed();
    void anAnswerFromTheFutureCountsAsOld();
    void theRestartStepShowsWhileItWaits();
    void openingStudioChecksRestartNeeded();
    void aReleaseTheSystemCannotTakeIsAProblem();
};

void UpdateInstallerTest::initTestCase()
{
    // In the build tree rather than /tmp, so the stand-in script may run.
    const QString testData = QCoreApplication::applicationDirPath() + QStringLiteral("/test-data");
    QVERIFY(QDir().mkpath(testData));
    m_base = std::make_unique<QTemporaryDir>(testData + QStringLiteral("/update-installer-XXXXXX"));
    QVERIFY(m_base->isValid());
    // Everything the updater and the settings backup write stays here, also
    // when the test is run by hand.
    for (const char *name : {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME", "XDG_RUNTIME_DIR"})
    {
        const QString folder = m_base->filePath(QString::fromLatin1(name).toLower());
        QVERIFY(QDir().mkpath(folder));
        QVERIFY(QFile::setPermissions(folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        qputenv(name, QFile::encodeName(folder));
    }
    for (const char *name : {"http_proxy", "https_proxy", "all_proxy", "HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY"})
        qunsetenv(name);
    QCoreApplication::setOrganizationName(QStringLiteral("ArchDockTests"));
    QCoreApplication::setApplicationName(QStringLiteral("update-installer-test"));

    m_key = m_base->filePath(QStringLiteral("test.key"));
    openssl({QStringLiteral("ecparam"), QStringLiteral("-name"), QStringLiteral("prime256v1"), QStringLiteral("-genkey"),
             QStringLiteral("-noout"), QStringLiteral("-out"), m_key});
    m_spki = openssl({QStringLiteral("pkey"), QStringLiteral("-in"), m_key, QStringLiteral("-pubout"), QStringLiteral("-outform"),
                      QStringLiteral("DER")});
    QVERIFY(!m_spki.isEmpty());
    QFile valid(QStringLiteral(UPDATE_VECTORS_DIR "/valid.json"));
    QVERIFY(valid.open(QIODevice::ReadOnly));
    m_valid = QJsonDocument::fromJson(valid.readAll()).object();
    QVERIFY(!m_valid.isEmpty());
    QVERIFY(m_server.isListening());
}

void UpdateInstallerTest::init()
{
    m_folder = std::make_unique<QTemporaryDir>(m_base->filePath(QStringLiteral("case-XXXXXX")));
    QVERIFY(m_folder->isValid());
    for (const char *folder : {"runtime", "cache", "state", "tools", "bin"})
        QVERIFY(QDir(m_folder->path()).mkpath(QString::fromLatin1(folder)));
    QVERIFY(QFile::setPermissions(path(QStringLiteral("runtime")),
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    QFile script(path(QStringLiteral("bin/stand-in-pkexec")));
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write(StandIn);
    script.close();
    QVERIFY(script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    plan(QStringLiteral("exit:126"));

    m_runner.tools = path(QStringLiteral("tools"));
    m_runner.systemdRun = path(QStringLiteral("tools/systemd-run"));
    m_runner.installedFile = path(QStringLiteral("bin/installed"));
    m_runner.installedVersion = Running;
    m_runner.printed.clear();
    m_runner.unmet.clear();
    m_runner.absent.clear();
    m_runner.lastSession.clear();
    m_runner.systemdRunExit = 0;
    m_runner.onSystemdRun = {};
    m_runner.calls.clear();
    m_runner.unexpected.clear();
    m_server.reset();
    m_running.clear();
    m_looked.clear();
    m_notices.clear();
    m_signals.clear();
    m_profileBusy = false;
    m_previewActive = false;
    m_studioOpen = true;
    m_studioDraft = false;
    m_studioOpened = 0;
    QSettings().remove(QStringLiteral("updates"));
}

void UpdateInstallerTest::cleanup()
{
    const bool held = UpdateHold::held();
    if (held)
        UpdateHold::release();
    QVERIFY2(!held, "the update hold was left held");
    QCOMPARE(UpdateHold::changesUnderWay(), 0);
    QCOMPARE(m_runner.unexpected, QStringList());
    // The updater never quits Arch Dock: no case leaves a stop record.
    QVERIFY(!QFile::exists(ArchDock::IntentionalStop::statePath()));
    m_folder.reset();
}

// The guards come before any network use; Update Now then looks again, and a
// release withdrawn since is never installed.
void UpdateInstallerTest::theGuardsComeBeforeTheNetwork()
{
    publish(release(Newer));
    auto updates = updater();
    QVERIFY(findUpdate(*updates));
    const qsizetype asked = m_server.requests.size();
    m_previewActive = true;
    updates->update();
    QCOMPARE(updates->state(), QStringLiteral("found"));
    QCOMPARE(updates->lineCode(), QStringLiteral("L28"));
    m_previewActive = false;
    m_profileBusy = true;
    updates->update();
    QCOMPARE(updates->lineCode(), QStringLiteral("L29"));
    QVERIFY(!UpdateHold::held());
    QTest::qWait(200);
    QCOMPARE(m_server.requests.size(), asked);

    m_profileBusy = false;
    m_server.latestStatus = "404 Not Found";
    m_server.latest.clear();
    updates->update();
    QVERIFY(UpdateHold::held());
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("up-to-date"); }));
    QCOMPARE(m_server.requests.size(), asked + 1);
    QVERIFY(m_server.requests.last().endsWith(QLatin1String("/releases/latest")));
    QVERIFY(!UpdateHold::held());
    QVERIFY(!pkexecRan());
}

// pacman's lock as this account sees it, before the download and again
// right before the password window.
void UpdateInstallerTest::theLockStopsBeforeThePasswordWindow()
{
    publish(release(Newer));
    auto updates = updater();
    QFile lock(path(QStringLiteral("db.lck")));
    QVERIFY(lock.open(QIODevice::WriteOnly));
    lock.close();
    const auto attempt = [&](const QSet<QString> &running, const QString &line)
    {
        m_running = running;
        if (!findUpdate(*updates))
            return QStringLiteral("not found");
        const qsizetype asked = m_server.requests.size();
        updates->update();
        if (!waitFor([&] { return updates->state() == QLatin1String("problem"); }))
            return QStringLiteral("no problem shown");
        for (const QString &request : m_server.requests.mid(asked))
            if (request.endsWith(QLatin1String(".pkg.tar.zst")))
                return QStringLiteral("downloaded");
        if (updates->lineCode() != line)
            return QStringLiteral("line %1").arg(updates->lineCode());
        if ((line == QLatin1String("L15")) != updates->details().contains(QLatin1String("sudo rm /var/lib/pacman/db.lck")))
            return QStringLiteral("details %1").arg(updates->details());
        return QString();
    };
    QCOMPARE(attempt({QStringLiteral("pacman")}, QStringLiteral("L14")), QString());
    QCOMPARE(attempt({QStringLiteral("packagekitd")}, QStringLiteral("L14")), QString());
    QCOMPARE(attempt({}, QStringLiteral("L15")), QString());
    QVERIFY(!pkexecRan());
    QVERIFY(!UpdateHold::held());

    // A lock that appears during the download stops before the password window.
    QVERIFY(lock.remove());
    m_server.onRequest = [&](const QByteArray &requested)
    {
        if (requested.endsWith(".pkg.tar.zst") && lock.open(QIODevice::WriteOnly))
            lock.close();
    };
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("problem"); }));
    QCOMPARE(updates->lineCode(), QStringLiteral("L15"));
    QVERIFY(m_server.requests.last().endsWith(QLatin1String(".pkg.tar.zst")));
    QVERIFY(!pkexecRan());
    QVERIFY(!QFile::exists(markerPath()));
    QVERIFY(!UpdateHold::held());
}

// One copy of the settings per new version: a second press makes none,
// nothing is pruned, and a copy that cannot be made stops the update.
void UpdateInstallerTest::oneBackupPerVersion()
{
    ConfigurationBackup backups;
    for (int copy = 0; copy < 6; ++copy)
        QVERIFY(!backups.capture(QStringLiteral("older-%1").arg(copy)).isEmpty());
    const QStringList older = backups.backups();
    QVERIFY(older.size() >= 6);

    publish(release(Newer));
    plan(QStringLiteral("exit:126"));
    auto updates = updater();
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return pkexecRan() && found(*updates) && !UpdateHold::held(); }));
    const QStringList after = backups.backups();
    QCOMPARE(after.size(), older.size() + 1);
    for (const QString &id : older)
        QVERIFY(after.contains(id));
    const QString kept = QSettings().value(QStringLiteral("updates/backupFor")).toString();
    QVERIFY2(kept.startsWith(Newer + QLatin1Char(' ')), qPrintable(kept));
    QVERIFY(after.contains(kept.section(u' ', 1)));

    QVERIFY(QFile::remove(path(QStringLiteral("bin/pkexec-arguments"))));
    updates->update();
    QVERIFY(waitFor([&] { return pkexecRan() && found(*updates) && !UpdateHold::held(); }));
    QCOMPARE(backups.backups(), after);

    // The copy cannot be made: nothing installed, no password window.
    QSettings().remove(QStringLiteral("updates/backupFor"));
    QVERIFY(QFile::remove(path(QStringLiteral("bin/pkexec-arguments"))));
    QLockFile busy(QDir(ConfigurationBackup::defaultRoot()).filePath(QStringLiteral("backup.lock")));
    QVERIFY(busy.tryLock(0));
    updates->update();
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("problem"); }));
    QCOMPARE(updates->lineCode(), QStringLiteral("L13"));
    QVERIFY(!pkexecRan());
    QVERIFY(!QFile::exists(markerPath()));
    QVERIFY(!UpdateHold::held());
    busy.unlock();
}

// The marker first; then pkexec, from the main thread, with its exact line,
// environment and input, closed after the last byte; "started" shows
// Installing; after 0 the marker says installed and the finisher starts with
// its exact line. Without the finisher's TERM the page ends on L24.
void UpdateInstallerTest::thePasswordWindowGetsExactlyItsInput()
{
    const Release made = release(Newer);
    publish(made);
    plan(QStringLiteral("started read wait:5 install:%1 exit:0").arg(Newer));
    QByteArray markerAtFinisher;
    QStringList finisherLine;
    m_runner.onSystemdRun = [&](const QStringList &arguments)
    {
        finisherLine = arguments;
        markerAtFinisher = readBytes(markerPath());
    };
    auto updates = updater();
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return updates->findChild<QProcess *>() != nullptr; }));
    QCOMPARE(updates->findChild<QProcess *>()->thread(), QCoreApplication::instance()->thread());
    QVERIFY(waitFor([&] { return installing(*updates); }));
    QVERIFY(UpdateHold::held());
    QVERIFY(updates->quitRefused());
    QVERIFY(!updates->canStop());
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("restarting"); }));
    QCOMPARE(updates->line(), QStringLiteral("Restarting Arch Dock and Plasma's panels…"));

    QCOMPARE(QString::fromUtf8(readBytes(path(QStringLiteral("bin/pkexec-arguments")))),
             QStringLiteral("--disable-internal-agent\n") + Helper + QLatin1Char('\n'));
    QStringList variables = QString::fromUtf8(readBytes(path(QStringLiteral("bin/pkexec-environment")))).split(u'\n', Qt::SkipEmptyParts);
    variables.sort();
    QCOMPARE(variables, QStringList({QStringLiteral("LC_ALL=C"), QStringLiteral("PATH=/usr/bin")}));
    QVERIFY(readBytes(path(QStringLiteral("bin/pkexec-input"))) == made.input());

    const QJsonObject atStart = json(readBytes(path(QStringLiteral("bin/marker-at-start"))));
    QCOMPARE(atStart.value(QStringLiteral("format")).toInt(), 1);
    QCOMPARE(atStart.value(QStringLiteral("phase")).toString(), QStringLiteral("installing"));
    QCOMPARE(atStart.value(QStringLiteral("from")).toString(), Running);
    QCOMPARE(atStart.value(QStringLiteral("to")).toString(), Newer);
    QCOMPARE(atStart.value(QStringLiteral("session")).toString(), ArchDock::IntentionalStop::sessionIdentity());

    QCOMPARE(json(markerAtFinisher).value(QStringLiteral("phase")).toString(), QStringLiteral("installed"));
    QCOMPARE(finisherLine.size(), 10);
    QCOMPARE(finisherLine.mid(0, 3), QStringList({QStringLiteral("--user"), QStringLiteral("--collect"), QStringLiteral("--quiet")}));
    QVERIFY2(QRegularExpression(QStringLiteral("^--unit=arch-dock-finish-update-[0-9]+-[0-9]+$")).match(finisherLine.at(3)).hasMatch(),
             qPrintable(finisherLine.at(3)));
    // systemd-run itself gets this login's runtime folder, so it reaches the
    // user's service manager; PATH and LC_ALL stay fixed.
    QVERIFY2(m_runner.lastSession.contains(QStringLiteral("XDG_RUNTIME_DIR=") + path(QStringLiteral("runtime"))),
             qPrintable(m_runner.lastSession.join(u' ')));
    QCOMPARE(finisherLine.mid(4), QStringList({QStringLiteral("-E"), QStringLiteral("PATH=/usr/bin"), QStringLiteral("--"), Finisher,
                                               Newer, QString::number(::getpid())}));
    QVERIFY(updates->quitRefused());

    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("finish"); }));
    QCOMPARE(updates->lineCode(), QStringLiteral("L24"));
    QVERIFY(!updates->quitRefused());
    QVERIFY(!UpdateHold::held());
    QVERIFY(m_notices.isEmpty());
}

// After "started" nothing ends the install: not Stop, not closing Studio,
// not a look, not time (it runs well past every wait the updater has).
void UpdateInstallerTest::noTimeLimitAfterStarted()
{
    publish(release(Newer));
    plan(QStringLiteral("split-started read wait:40 install:%1 exit:0").arg(Newer));
    ArchDockUpdates::Environment setup = environment();
    setup.waitLimitMs = 1000;
    setup.signalProcess = [this](qint64, int signal)
    {
        m_signals.append(signal);
        return 0;
    };
    auto updates = updater(std::move(setup));
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return installing(*updates); }));
    updates->stop();
    updates->studioClosing();
    updates->check();
    QVERIFY(installing(*updates));
    QTest::qWait(2500);
    QVERIFY(installing(*updates));
    QVERIFY(m_signals.isEmpty());
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("restarting"); }));
    QCOMPARE(m_runner.systemdRuns(), 1);
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("finish"); }));
}

// The page shows Check before the check runs; a Stop then ends the update
// quietly, before any copy, marker or password window.
void UpdateInstallerTest::stopDuringTheCheck()
{
    publish(release(Newer));
    auto updates = updater();
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return updates->step() == QLatin1String("check"); }));
    QVERIFY(updates->canStop());
    updates->stop();
    QVERIFY(waitFor([&] { return found(*updates); }));
    QVERIFY(updates->line().isEmpty());
    QTest::qWait(300);
    QVERIFY(!pkexecRan());
    QVERIFY(!QFile::exists(markerPath()));
    QVERIFY(!UpdateHold::held());
    for (const QStringList &call : m_runner.calls)
        QVERIFY2(!call.contains(QStringLiteral("--print")), qPrintable(call.join(u' ')));
}

void UpdateInstallerTest::stopBeforeThePasswordIsAccepted_data()
{
    QTest::addColumn<QString>("how");
    QTest::newRow("Stop") << QStringLiteral("stop");
    QTest::newRow("closing Studio") << QStringLiteral("studio");
    QTest::newRow("Quit") << QStringLiteral("quit");
}

// Before "started": sending stops, the input is closed, then TERM. The page
// shows "Stopping the update…" until pkexec has ended, then Found, silently.
void UpdateInstallerTest::stopBeforeThePasswordIsAccepted()
{
    QFETCH(QString, how);
    publish(release(Newer));
    plan(QStringLiteral("wait:100 started read exit:0"));
    ArchDockUpdates *watched = nullptr;
    QString stateAtSignal;
    ArchDockUpdates::Environment setup = environment();
    setup.signalProcess = [&](qint64 pid, int signal)
    {
        m_signals.append(signal);
        stateAtSignal = watched->state();
        return ::kill(static_cast<pid_t>(pid), signal) == 0 ? 0 : errno;
    };
    auto updates = updater(std::move(setup));
    watched = updates.get();
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return updates->step() == QLatin1String("password") && pkexecRan(); }));
    QVERIFY(updates->canStop());
    int quitAnswers = 0;
    bool quitRefused = true;
    if (how == QLatin1String("stop"))
        updates->stop();
    else if (how == QLatin1String("studio"))
        updates->studioClosing();
    else
        updates->stopForQuit([&](bool refused)
        {
            ++quitAnswers;
            quitRefused = refused;
        });
    QCOMPARE(updates->state(), QStringLiteral("stopping"));
    QCOMPARE(updates->line(), QStringLiteral("Stopping the update…"));
    QCOMPARE(m_signals, QList<int>{SIGTERM});
    QCOMPARE(stateAtSignal, QStringLiteral("stopping"));
    QVERIFY(waitFor([&] { return found(*updates); }));
    QVERIFY(updates->line().isEmpty());
    QVERIFY(!QFile::exists(path(QStringLiteral("bin/read-done"))));
    QVERIFY(!QFile::exists(markerPath()));
    QVERIFY(!UpdateHold::held());
    QVERIFY(m_notices.isEmpty());
    if (how == QLatin1String("quit"))
    {
        QCOMPARE(quitAnswers, 1);
        QVERIFY(!quitRefused);
    }
}

void UpdateInstallerTest::tooLateToStop_data()
{
    QTest::addColumn<QString>("steps");
    QTest::addColumn<bool>("refusedSignal");
    QTest::addColumn<bool>("afterEveryByte");
    QTest::addColumn<QString>("end");
    // The TERM answers EPERM: pkexec already runs as root.
    QTest::newRow("EPERM, the input cut") << QStringLiteral("wait:10 started read cut:12 install:%1 exit:0") << true << false
                                          << QStringLiteral("found");
    QTest::newRow("EPERM, every byte sent") << QStringLiteral("read wait:20 install:%1 exit:0") << true << true
                                            << QStringLiteral("restarting");
    // The TERM went out, but "started" came after it.
    QTest::newRow("started after the Stop") << QStringLiteral("wait:10 started read cut:12 install:%1 exit:0") << false << false
                                            << QStringLiteral("found");
}

// Too late to stop: the page shows Installing and keeps reading. The cut
// input gives the helper's 12 and a silent Found; with every byte sent, the
// install goes on to the restart. A Quit that waited is refused.
void UpdateInstallerTest::tooLateToStop()
{
    QFETCH(QString, steps);
    QFETCH(bool, refusedSignal);
    QFETCH(bool, afterEveryByte);
    QFETCH(QString, end);
    const Release made = release(Newer);
    publish(made);
    plan(steps.arg(Newer), {}, made.input().size());
    ArchDockUpdates::Environment setup = environment();
    setup.signalProcess = [&](qint64, int signal)
    {
        m_signals.append(signal);
        return refusedSignal ? EPERM : 0;
    };
    auto updates = updater(std::move(setup));
    QVERIFY(findUpdate(*updates));
    updates->update();
    if (afterEveryByte)
        QVERIFY(waitFor([&] { return QFile::exists(path(QStringLiteral("bin/read-done"))); }));
    else
        QVERIFY(waitFor([&] { return updates->step() == QLatin1String("password") && pkexecRan(); }));
    int quitAnswers = 0;
    bool quitRefused = false;
    updates->stopForQuit([&](bool refused)
    {
        ++quitAnswers;
        quitRefused = refused;
    });
    QCOMPARE(m_signals, QList<int>{SIGTERM});
    if (refusedSignal)
        QVERIFY(installing(*updates));
    else
        QCOMPARE(updates->state(), QStringLiteral("stopping"));
    QVERIFY(waitFor([&] { return installing(*updates); }));
    QCOMPARE(quitAnswers, 1);
    QVERIFY(quitRefused);
    QVERIFY(waitFor([&] { return updates->state() == end; }));
    QVERIFY(updates->line() == (end == QLatin1String("found") ? QString() : QStringLiteral("Restarting Arch Dock and Plasma's panels…")));
    QCOMPARE(m_runner.systemdRuns(), end == QLatin1String("found") ? 0 : 1);
    QVERIFY(m_notices.isEmpty());
    QCOMPARE(quitAnswers, 1);
    if (end == QLatin1String("found"))
    {
        QCOMPARE(m_runner.installed(), Running);
        QVERIFY(QFileInfo(path(QStringLiteral("bin/pkexec-input"))).size() < made.input().size());
        QVERIFY(!QFile::exists(markerPath()));
    }
    QVERIFY(waitFor([&] { return !UpdateHold::held(); }));
}

void UpdateInstallerTest::theHelpersCodes_data()
{
    QTest::addColumn<QString>("steps");
    QTest::addColumn<QString>("errors");
    QTest::addColumn<QString>("line");
    QTest::addColumn<bool>("notice");
    QTest::newRow("126") << QStringLiteral("exit:126") << QString() << QString() << false;
    QTest::newRow("127, not authorized") << QStringLiteral("exit:127")
                                         << QStringLiteral("Error executing command as another user: Not authorized")
                                         << QStringLiteral("L16") << false;
    QTest::newRow("127, anything else") << QStringLiteral("exit:127")
                                        << QStringLiteral("Error executing command as another user: No authentication agent found.")
                                        << QStringLiteral("L17") << false;
    const QList<std::pair<int, const char *>> codes = {{10, "L14"}, {11, "L8"},  {12, "L18"}, {13, "L20"}, {15, "L23"},
                                                       {16, "L19"}, {17, "L12"}, {18, "L15"}, {20, "L21"}, {19, "L22"},
                                                       {21, "L22"}, {14, "L22"}, {22, "L22"}, {99, "L22"}};
    for (const auto &[code, line] : codes)
        QTest::newRow(qPrintable(QString::number(code))) << QStringLiteral("started read pacman-error exit:%1").arg(code)
                                                         << QStringLiteral("arch-dock-install-update: why it stopped")
                                                         << QString::fromLatin1(line) << true;
    QTest::newRow("a crash") << QStringLiteral("started read killed") << QString() << QStringLiteral("L22") << true;
}

// pkexec's and the helper's codes give their lines on the Updates page. A failure after
// the password, with Studio closed, also gives the notice "Arch Dock was not updated".
void UpdateInstallerTest::theHelpersCodes()
{
    QFETCH(QString, steps);
    QFETCH(QString, errors);
    QFETCH(QString, line);
    QFETCH(bool, notice);
    publish(release(Newer));
    plan(steps, errors);
    auto updates = updater();
    QVERIFY(findUpdate(*updates));
    // What the page was told last: the details must be set before it is.
    QString detailsTold;
    connect(updates.get(), &ArchDockUpdates::changed, this, [&] { detailsTold = updates->details(); });
    updates->update();
    if (steps.startsWith(QLatin1String("started")))
    {
        QVERIFY(waitFor([&] { return installing(*updates); }));
        // Closing Studio after "started" changes nothing.
        m_studioOpen = false;
        updates->studioClosing();
    }
    QVERIFY(waitFor([&] { return updates->state() != QLatin1String("updating"); }));
    if (line.isEmpty())
    {
        QCOMPARE(updates->state(), QStringLiteral("found"));
        QVERIFY(updates->line().isEmpty());
    }
    else
    {
        QCOMPARE(updates->state(), QStringLiteral("problem"));
        QCOMPARE(updates->lineCode(), line);
        QVERIFY(!updates->line().isEmpty());
    }
    if (line == QLatin1String("L15"))
        QVERIFY(updates->details().contains(QLatin1String("sudo rm /var/lib/pacman/db.lck")));
    else if (!errors.isEmpty())
    {
        QVERIFY2(updates->details().contains(errors), qPrintable(updates->details()));
        // pacman's own lines, which the helper passes on, are in Copy Details.
        if (steps.contains(QLatin1String("pacman-error")))
            QVERIFY2(updates->details().contains(QLatin1String("error: failed to commit transaction")), qPrintable(updates->details()));
    }
    QCOMPARE(detailsTold, updates->details());
    QCOMPARE(m_notices, notice ? QStringList{QStringLiteral("UpdateProblem | Arch Dock was not updated | %1 | Open Updates")
                                                 .arg(updates->line())}
                               : QStringList());
    QCOMPARE(m_runner.systemdRuns(), 0);
    QVERIFY(!QFile::exists(markerPath()));
    QVERIFY(!UpdateHold::held());
}

void UpdateInstallerTest::pacmanDecidesAfterAnUnclearEnd_data()
{
    QTest::addColumn<QString>("end");
    QTest::addColumn<bool>("pacmanRuns");
    QTest::newRow("14, while a pacman runs") << QStringLiteral("exit:14") << true;
    for (const char *end : {"exit:19", "exit:21", "exit:22", "exit:99", "killed"})
        QTest::newRow(end) << QString::fromLatin1(end) << false;
}

// For 14, 19, 21, 22, an unknown code or a crash the page waits while a
// pacman runs, then `pacman -Q` decides: the new version goes on as for 0.
void UpdateInstallerTest::pacmanDecidesAfterAnUnclearEnd()
{
    QFETCH(QString, end);
    QFETCH(bool, pacmanRuns);
    publish(release(Newer));
    plan(QStringLiteral("started read install:%1 %2").arg(Newer, end));
    if (pacmanRuns)
        m_running = {QStringLiteral("pacman")};
    auto updates = updater();
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return m_looked.contains(QStringLiteral("pacman")); }));
    if (pacmanRuns)
    {
        QTest::qWait(2500);
        QVERIFY(installing(*updates));
        QCOMPARE(m_runner.systemdRuns(), 0);
        m_running.clear();
    }
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("restarting"); }));
    QCOMPARE(m_runner.systemdRuns(), 1);
    QCOMPARE(json(readBytes(markerPath())).value(QStringLiteral("phase")).toString(), QStringLiteral("installed"));
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("finish"); }));
}

void UpdateInstallerTest::afterTheInstall_data()
{
    QTest::addColumn<QString>("steps");
    QTest::addColumn<QString>("busy");
    QTest::addColumn<QString>("finisher");
    QTest::addColumn<QString>("state");
    QTest::addColumn<QString>("line");
    QTest::addColumn<int>("finisherRuns");
    const QString install = QStringLiteral("started read wait:5 install:%1 exit:0").arg(Newer);
    QTest::newRow("another version afterwards") << QStringLiteral("started read install:0.1.1-57 exit:0") << QString() << QString()
                                                << QStringLiteral("restart-needed")
                                                << QStringLiteral("Arch Dock 0.1.1-57 is installed, but this account still runs 0.1.1-55.")
                                                << 0;
    QTest::newRow("a profile apply") << install << QStringLiteral("profile") << QString() << QStringLiteral("restart-needed")
                                     << QStringLiteral("L30") << 0;
    QTest::newRow("a preview") << install << QStringLiteral("preview") << QString() << QStringLiteral("restart-needed")
                               << QStringLiteral("L30") << 0;
    QTest::newRow("Studio's draft") << install << QStringLiteral("draft") << QString() << QStringLiteral("restart-needed")
                                    << QStringLiteral("L30") << 0;
    QTest::newRow("a change still under way") << install << QStringLiteral("change") << QString()
                                              << QStringLiteral("restart-needed") << QStringLiteral("L30") << 0;
    QTest::newRow("a change that ends in time") << install << QStringLiteral("short change") << QString()
                                                << QStringLiteral("finish") << QStringLiteral("L24") << 1;
    QTest::newRow("systemd-run fails") << install << QString() << QStringLiteral("fails") << QStringLiteral("finish")
                                       << QStringLiteral("L24") << 1;
    QTest::newRow("the finisher: not-installed") << install << QString() << QStringLiteral("not-installed")
                                                 << QStringLiteral("finish") << QStringLiteral("L24") << 1;
    QTest::newRow("the finisher: log-out-needed") << install << QString() << QStringLiteral("log-out-needed")
                                                  << QStringLiteral("finish") << QStringLiteral("L25") << 1;
    // The finisher writes "starting" only after this process ended; seen
    // while it still runs, the restart did not happen.
    QTest::newRow("the finisher: starting, but no TERM came") << install << QString() << QStringLiteral("starting")
                                                              << QStringLiteral("finish") << QStringLiteral("L24") << 1;
}

// After 0: the updater's own `pacman -Q`, the guards again with Studio's
// draft and the changes under way, then the finisher and its result.
void UpdateInstallerTest::afterTheInstall()
{
    QFETCH(QString, steps);
    QFETCH(QString, busy);
    QFETCH(QString, finisher);
    QFETCH(QString, state);
    QFETCH(QString, line);
    QFETCH(int, finisherRuns);
    publish(release(Newer));
    plan(steps);
    ArchDockUpdates::Environment setup = environment();
    if (finisher == QLatin1String("not-installed") || finisher == QLatin1String("log-out-needed")
        || finisher == QLatin1String("starting"))
    {
        // The result must end the watch long before its limit.
        setup.waitLimitMs = 20000;
        m_runner.onSystemdRun = [&](const QStringList &) { writeResult(finisher, Newer); };
    }
    if (finisher == QLatin1String("fails"))
        m_runner.systemdRunExit = 1;
    auto updates = updater(std::move(setup));
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return installing(*updates); }));
    m_profileBusy = busy == QLatin1String("profile");
    m_previewActive = busy == QLatin1String("preview");
    m_studioDraft = busy == QLatin1String("draft");
    const bool change = busy.endsWith(QLatin1String("change"));
    if (change)
        UpdateHold::beginChange();
    if (busy == QLatin1String("short change"))
    {
        QVERIFY(waitFor([&] { return json(readBytes(markerPath())).value(QStringLiteral("phase")).toString() == QLatin1String("installed"); }));
        UpdateHold::endChange();
    }
    QElapsedTimer timer;
    timer.start();
    QVERIFY(waitFor([&] { return updates->state() == state; }, 15000));
    if (finisher == QLatin1String("not-installed") || finisher == QLatin1String("log-out-needed")
        || finisher == QLatin1String("starting"))
        QVERIFY2(timer.elapsed() < 10000, "the result did not end the watch");
    if (busy == QLatin1String("change"))
        UpdateHold::endChange();
    if (line.startsWith(u'L'))
        QCOMPARE(updates->lineCode(), line);
    else
        QCOMPARE(updates->line(), line);
    QCOMPARE(m_runner.systemdRuns(), finisherRuns);
    QVERIFY(!UpdateHold::held());
    QVERIFY(!updates->quitRefused());
}

// Restart Arch Dock: no password, the same guards, and the marker
// "installed" before the finisher. A Restart the guards refuse while Studio
// is closed answers with one "Arch Dock was not updated" notice carrying L30.
void UpdateInstallerTest::restartArchDockNeedsNoPassword()
{
    m_runner.installedVersion = Newer;
    m_studioOpen = false;
    writeResult(QStringLiteral("done"), Newer);
    QByteArray markerAtFinisher;
    bool resultAtFinisher = true;
    m_runner.onSystemdRun = [&](const QStringList &)
    {
        markerAtFinisher = readBytes(markerPath());
        resultAtFinisher = QFile::exists(resultPath());
    };
    auto updates = updater();
    QCOMPARE(updates->state(), QStringLiteral("restart-needed"));
    QCOMPARE(m_notices.size(), 1);
    QVERIFY2(m_notices.first().startsWith(QStringLiteral("RestartNeeded | Arch Dock was updated | Restart it to use %1.").arg(Newer)),
             qPrintable(m_notices.first()));

    m_previewActive = true;
    updates->restartArchDock();
    QCOMPARE(updates->state(), QStringLiteral("restart-needed"));
    QCOMPARE(updates->lineCode(), QStringLiteral("L30"));
    QCOMPARE(m_notices.size(), 2);
    QCOMPARE(m_notices.last(), QStringLiteral("UpdateProblem | Arch Dock was not updated | %1 | Open Updates").arg(updates->line()));
    QCOMPARE(m_runner.systemdRuns(), 0);
    QVERIFY(!UpdateHold::held());

    m_previewActive = false;
    updates->restartArchDock();
    QCOMPARE(updates->state(), QStringLiteral("restarting"));
    QVERIFY(UpdateHold::held());
    QVERIFY(updates->quitRefused());
    const QJsonObject marker = json(markerAtFinisher);
    QCOMPARE(marker.value(QStringLiteral("phase")).toString(), QStringLiteral("installed"));
    QCOMPARE(marker.value(QStringLiteral("from")).toString(), Running);
    QCOMPARE(marker.value(QStringLiteral("to")).toString(), Newer);
    QVERIFY(!resultAtFinisher);
    QVERIFY(!pkexecRan());
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("finish"); }));
    QCOMPARE(updates->lineCode(), QStringLiteral("L24"));
}

void UpdateInstallerTest::theNextStart_data()
{
    QTest::addColumn<QString>("phase");
    QTest::addColumn<bool>("otherSession");
    QTest::addColumn<QString>("installed");
    QTest::addColumn<QString>("release");
    QTest::addColumn<QStringList>("running");
    QTest::addColumn<QString>("status");
    QTest::addColumn<QString>("state");
    QTest::addColumn<QString>("line");
    QTest::addColumn<bool>("notice");
    QTest::addColumn<int>("studioOpened");
    QTest::addColumn<bool>("kept");
    const QString finish = QStringLiteral("Arch Dock %1 was installed, but Arch Dock and Plasma's panels were not restarted afterwards.").arg(Newer);
    const QString done = QStringLiteral("Arch Dock %1 is installed and running.").arg(Newer);
    // A new login runs the new version already.
    QTest::newRow("another login session") << "installing" << true << Newer << Newer << QStringList() << QString()
                                           << "not-looked" << QString() << false << 0 << false;
    QTest::newRow("installing, the helper runs") << "installing" << false << Running << Running << QStringList{"arch-dock-insta"}
                                                 << QString() << "updating" << QString() << false << 0 << true;
    QTest::newRow("installing, a pacman runs") << "installing" << false << Running << Running << QStringList{"pacman"} << QString()
                                               << "updating" << QString() << false << 0 << true;
    QTest::newRow("installing, it finished") << "installing" << false << Newer << Running << QStringList() << QString() << "finish"
                                             << finish << true << 0 << true;
    QTest::newRow("installing, it finished, this runs it") << "installing" << false << Newer << Newer << QStringList() << QString()
                                                           << "finish" << finish << true << 0 << true;
    QTest::newRow("installing, the old version") << "installing" << false << Running << Running << QStringList() << QString()
                                                 << "not-looked" << QString() << false << 0 << false;
    QTest::newRow("installed, the finisher runs") << "installed" << false << Newer << Running << QStringList{"arch-dock-finis"}
                                                  << QString() << "restarting" << QString() << false << 0 << true;
    for (const char *status : {"starting", "quit", "not-started", "done"})
        QTest::newRow(qPrintable(QStringLiteral("installed, %1").arg(QLatin1String(status))))
            << "installed" << false << Newer << Newer << QStringList() << QString::fromLatin1(status) << "done" << done << false << 1
            << false;
    QTest::newRow("installed, done, but this is the old Arch Dock") << "installed" << false << Newer << Running << QStringList()
                                                                    << "done" << "finish" << finish << true << 0 << true;
    QTest::newRow("installed, plasma-not-back") << "installed" << false << Newer << Newer << QStringList() << "plasma-not-back"
                                                << "finish" << "L27" << false << 1 << true;
    for (const char *status : {"log-out-needed", "not-installed", ""})
        QTest::newRow(qPrintable(QStringLiteral("installed, %1").arg(*status ? QLatin1String(status) : QLatin1String("no result"))))
            << "installed" << false << Newer << Newer << QStringList() << QString::fromLatin1(status) << "finish" << finish << true << 0
            << true;
}

// The next start reads the marker and the finisher's result; the first rule
// that fits decides.
void UpdateInstallerTest::theNextStart()
{
    QFETCH(QString, phase);
    QFETCH(bool, otherSession);
    QFETCH(QString, installed);
    QFETCH(QString, release);
    QFETCH(QStringList, running);
    QFETCH(QString, status);
    QFETCH(QString, state);
    QFETCH(QString, line);
    QFETCH(bool, notice);
    QFETCH(int, studioOpened);
    QFETCH(bool, kept);
    m_studioOpen = false;
    m_runner.installedVersion = installed;
    m_running = QSet<QString>(running.cbegin(), running.cend());
    writeMarker(phase, Newer, otherSession ? QStringLiteral("another-boot/7") : ArchDock::IntentionalStop::sessionIdentity());
    if (!status.isEmpty())
        writeResult(status, Newer);
    const QString download = path(QStringLiteral("cache/") + packageFile(Newer));
    QFile old(download);
    QVERIFY(old.open(QIODevice::WriteOnly));
    old.close();

    ArchDockUpdates::Environment setup = environment();
    setup.release = release;
    auto updates = updater(std::move(setup));
    QTest::qWait(50);
    QCOMPARE(updates->state(), state);
    if (state == QLatin1String("updating"))
        QCOMPARE(updates->step(), QStringLiteral("installing"));
    if (line.startsWith(u'L') && line.size() <= 3)
        QCOMPARE(updates->lineCode(), line);
    else if (!line.isEmpty())
        QCOMPARE(updates->line(), line);
    if (notice)
        QCOMPARE(m_notices, QStringList{QStringLiteral("RestartNeeded | Arch Dock %1 was installed | Arch Dock and Plasma's panels "
                                                       "were not restarted yet. Restart them to finish. | Restart Arch Dock")
                                            .arg(Newer)});
    else
        QCOMPARE(m_notices, QStringList());
    QCOMPARE(m_studioOpened, studioOpened);
    QCOMPARE(QFile::exists(markerPath()), kept);
    QCOMPARE(QFile::exists(download), kept);
    if (!kept)
        QVERIFY(!QFile::exists(resultPath()));
    if (state == QLatin1String("restarting"))
        QVERIFY(m_looked.contains(QStringLiteral("arch-dock-finis@own")));
    QCOMPARE(m_runner.systemdRuns(), 0);
}

// The Restart needed and Finish notices at most once per version in each
// login: not again at the hourly tick, an account switch back, or another
// start in the same login.
void UpdateInstallerTest::noticesOncePerVersionAndLogin()
{
    m_studioOpen = false;
    m_runner.installedVersion = Newer;
    {
        auto updates = updater();
        QCOMPARE(m_notices.size(), 1);
        QVERIFY(m_notices.first().startsWith(QLatin1String("RestartNeeded | Arch Dock was updated")));
        QVERIFY(QMetaObject::invokeMethod(updates.get(), "checkRestartNeeded"));
        const QVariantMap active{{QStringLiteral("Active"), true}};
        QVERIFY(QMetaObject::invokeMethod(updates.get(), "sessionPropertiesChanged",
                                          Q_ARG(QString, QStringLiteral("org.freedesktop.login1.Session")),
                                          Q_ARG(QVariantMap, active), Q_ARG(QStringList, QStringList())));
        QCOMPARE(updates->state(), QStringLiteral("restart-needed"));
        QCOMPARE(m_notices.size(), 1);
    }
    {
        auto again = updater();
        QCOMPARE(m_notices.size(), 1);
    }
    m_runner.installedVersion = QStringLiteral("0.1.1-57");
    {
        auto newer = updater();
        QCOMPARE(m_notices.size(), 2);
        QVERIFY(m_notices.last().contains(QLatin1String("0.1.1-57")));
    }

    m_runner.installedVersion = Newer;
    writeMarker(QStringLiteral("installing"), Newer);
    {
        auto finish = updater();
        QCOMPARE(finish->state(), QStringLiteral("finish"));
        QCOMPARE(m_notices.size(), 3);
        QVERIFY(m_notices.last().startsWith(QStringLiteral("RestartNeeded | Arch Dock %1 was installed").arg(Newer)));
    }
    {
        auto finishAgain = updater();
        QCOMPARE(finishAgain->state(), QStringLiteral("finish"));
        QCOMPARE(m_notices.size(), 3);
    }
}

// A build without a release and a key refuses pkexec, systemd-run,
// `pacman -U` and the network, and offers no updates.
void UpdateInstallerTest::aBuildWithoutReleaseAndKeyStaysFenced()
{
    QVERIFY(!ArchDock::Version::updatesConfigured());
    QVERIFY(!BackendCommandRunner::allowed(QStringLiteral("/usr/bin/pkexec"), {QStringLiteral("--disable-internal-agent"), Helper}));
    QVERIFY(!BackendCommandRunner::allowed(QStringLiteral("/usr/bin/systemd-run"), {QStringLiteral("--user"), QStringLiteral("--"), Finisher}));
    QVERIFY(!BackendCommandRunner::allowed(QStringLiteral("/usr/bin/pacman"), {QStringLiteral("-U"), QStringLiteral("--"), packageFile(Newer)}));
    QVERIFY(BackendCommandRunner::allowed(QStringLiteral("/usr/bin/pacman"), {QStringLiteral("-Q"), QStringLiteral("arch-dock")}));
    QVERIFY(QFile::copy(path(QStringLiteral("bin/stand-in-pkexec")), path(QStringLiteral("bin/pkexec"))));
    BackendCommandRunner fenced;
    QVERIFY(!fenced.run(path(QStringLiteral("bin/pkexec")), {}).started);
    QVERIFY(!pkexecRan());
    QVERIFY(!networkAllowed(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(Repository)));

    // Neither the look nor the download opens a connection.
    const int connections = m_server.connections;
    const QString https = QStringLiteral("https://127.0.0.1:%1").arg(m_server.serverPort());
    QNetworkAccessManager network;
    ReleaseSetup releaseSetup;
    releaseSetup.api = https;
    releaseSetup.downloads = https;
    releaseSetup.repository = Repository;
    ReleaseSource source(&network, m_runner, releaseSetup);
    QSignalSpy looked(&source, &ReleaseSource::finished);
    source.look();
    QVERIFY(looked.wait(5000));
    const LookResult look = looked.first().first().value<LookResult>();
    QCOMPARE(look.outcome, LookResult::Outcome::Problem);
    QCOMPARE(look.line, QStringLiteral("L2"));
    UpdateDownload download(&network);
    QSignalSpy downloaded(&download, &UpdateDownload::finished);
    download.start(QUrl(https + QStringLiteral("/package")), path(QStringLiteral("cache/") + packageFile(Newer)), 10, QString(),
                   "ArchDock/test");
    QVERIFY(downloaded.wait(5000));
    QCOMPARE(downloaded.first().first().value<UpdateDownload::Result>(), UpdateDownload::Result::Refused);
    QTest::qWait(200);
    QCOMPARE(m_server.connections, connections);

    // Arch Dock's own updater in this build offers nothing and does nothing.
    {
        ArchDockUpdates updates({}, ArchDockUpdates::Environment::system());
        QVERIFY(!updates.offered());
        updates.check();
        updates.update();
        updates.restartArchDock();
        QCOMPARE(updates.state(), QStringLiteral("not-looked"));
        QVERIFY(!UpdateHold::held());
    }

    // Through the whole flow, the real pkexec's name is refused before it starts.
    publish(release(Newer));
    ArchDockUpdates::Environment setup = environment();
    setup.pkexec = path(QStringLiteral("bin/pkexec"));
    auto updates = updater(std::move(setup));
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("problem"); }));
    QCOMPARE(updates->lineCode(), QStringLiteral("L22"));
    QCOMPARE(updates->details(), QStringLiteral("this build does not install updates"));
    QVERIFY(!pkexecRan());
    QVERIFY(!QFile::exists(markerPath()));
    QVERIFY(!UpdateHold::held());
}

// The updater never quits Arch Dock or records a stop: it has no way to
// reach quit() or stopIntentionally(), and cleanup() finds no stop record
// after any case.
void UpdateInstallerTest::theUpdaterNeverQuitsArchDock()
{
    for (const char *file : {"/src/updates/ArchDockUpdates.cpp", "/src/updates/ArchDockUpdates.h"})
    {
        QFile source(QStringLiteral(ARCHDOCK_SOURCE_DIR) + QLatin1String(file));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY(!text.contains(QRegularExpression(QStringLiteral(R"(\bquit\s*\()"))));
        QVERIFY(!text.contains(QLatin1String("stopIntentionally")));
        QVERIFY(!text.contains(QLatin1String("IntentionalStop::record")));
        QVERIFY(!text.contains(QLatin1String("PanelWindow")));
    }
}

void UpdateInstallerTest::quitBeforeThePasswordEndsTheUpdate_data()
{
    QTest::addColumn<QString>("during");
    QTest::newRow("Update Now's look") << QStringLiteral("look");
    QTest::newRow("the download") << QStringLiteral("download");
    QTest::newRow("the check") << QStringLiteral("check");
}

// A Quit before the password ends the update and then quits, once, wherever
// the update stands; nothing of it stays armed to quit Arch Dock later.
void UpdateInstallerTest::quitBeforeThePasswordEndsTheUpdate()
{
    QFETCH(QString, during);
    publish(release(Newer));
    auto updates = updater();
    QVERIFY(findUpdate(*updates));
    const QByteArray latest = "/repos/" + Repository.toLatin1() + "/releases/latest";
    const QByteArray package = '/' + Repository.toLatin1() + "/releases/download/v" + Newer.toLatin1() + '/'
        + packageFile(Newer).toLatin1();
    if (during == QLatin1String("look"))
        m_server.held.insert(latest);
    else if (during == QLatin1String("download"))
        m_server.held.insert(package);
    updates->update();
    if (during == QLatin1String("look"))
        QVERIFY(waitFor([&] { return m_server.waiting(latest); }));
    else if (during == QLatin1String("download"))
        QVERIFY(waitFor([&] { return m_server.waiting(package); }));
    else
        QVERIFY(waitFor([&] { return updates->step() == QLatin1String("check"); }));
    int answers = 0;
    bool refused = true;
    updates->stopForQuit([&](bool refusedNow)
    {
        ++answers;
        refused = refusedNow;
    });
    m_server.releaseHeld();
    QVERIFY(waitFor([&] { return answers == 1; }));
    QVERIFY(!refused);
    QVERIFY(waitFor([&] { return found(*updates); }));
    QVERIFY(!UpdateHold::held());
    QVERIFY(!pkexecRan());
    QVERIFY(!QFile::exists(markerPath()));

    // A later update whose password window is dismissed quits nothing.
    m_server.held.clear();
    plan(QStringLiteral("exit:126"));
    updates->update();
    QVERIFY(waitFor([&] { return pkexecRan(); }));
    QVERIFY(waitFor([&] { return found(*updates) && !UpdateHold::held(); }));
    QCOMPARE(answers, 1);
}

// Stop while Update Now looks on GitHub again: "Stopping the update…" at
// once, and the update ends when the look answers, before any download.
void UpdateInstallerTest::stopDuringTheSecondLook()
{
    publish(release(Newer));
    auto updates = updater();
    QVERIFY(findUpdate(*updates));
    const QByteArray latest = "/repos/" + Repository.toLatin1() + "/releases/latest";
    m_server.held.insert(latest);
    updates->update();
    QVERIFY(waitFor([&] { return m_server.waiting(latest); }));
    QVERIFY(updates->canStop());
    updates->stop();
    QCOMPARE(updates->state(), QStringLiteral("stopping"));
    QCOMPARE(updates->line(), QStringLiteral("Stopping the update…"));
    QVERIFY(UpdateHold::held());
    m_server.releaseHeld();
    QVERIFY(waitFor([&] { return found(*updates); }));
    QTest::qWait(300);
    for (const QString &request : std::as_const(m_server.requests))
        QVERIFY2(!request.endsWith(QLatin1String(".pkg.tar.zst")), qPrintable(request));
    QVERIFY(!UpdateHold::held());
}

// After a restart while the helper ran, a helper that ends with the old
// version leaves no "Installing…" card behind.
void UpdateInstallerTest::anInstallingCardDoesNotOutliveTheInstall()
{
    m_studioOpen = false;
    m_running = {QStringLiteral("arch-dock-insta")};
    writeMarker(QStringLiteral("installing"), Newer);
    auto updates = updater();
    QVERIFY(installing(*updates));
    m_running.clear();
    QVERIFY(waitFor([&] { return !installing(*updates); }, 8000));
    QCOMPARE(updates->state(), QStringLiteral("not-looked"));
    QVERIFY(!QFile::exists(markerPath()));
}

// Restart Arch Dock is under way from the press: Studio is told the hold at
// once, the page shows Restarting, and a second press, a look or a Quit
// wait; one finisher starts once the draft is gone.
void UpdateInstallerTest::aRestartIsUnderWayFromThePress()
{
    m_runner.installedVersion = Newer;
    auto updates = updater();
    QCOMPARE(updates->state(), QStringLiteral("restart-needed"));
    m_studioDraft = true;
    bool heldWhenTold = false;
    connect(updates.get(), &ArchDockUpdates::changed, this, [&] { heldWhenTold = heldWhenTold || updates->holdsSettings(); });
    updates->restartArchDock();
    QVERIFY(heldWhenTold);
    QCOMPARE(updates->state(), QStringLiteral("restarting"));
    QVERIFY(updates->quitRefused());
    updates->restartArchDock();
    updates->check();
    QCOMPARE(updates->state(), QStringLiteral("restarting"));
    QCOMPARE(m_runner.systemdRuns(), 0);
    m_studioDraft = false;
    QVERIFY(waitFor([&] { return m_runner.systemdRuns() == 1; }));
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("finish"); }));
    QCOMPARE(m_runner.systemdRuns(), 1);
}

// A look never hides a restart still to do: Finish stays while GitHub has
// nothing newer, and Restart needed comes back after "up to date".
void UpdateInstallerTest::aLookKeepsARestartToDo()
{
    publish(release(Newer));
    {
        m_runner.installedVersion = Newer;
        writeMarker(QStringLiteral("installed"), Newer);
        ArchDockUpdates::Environment setup = environment();
        setup.release = Newer;
        auto updates = updater(std::move(setup));
        QCOMPARE(updates->state(), QStringLiteral("finish"));
        const QString line = updates->line();
        const qsizetype asked = m_server.requests.size();
        updates->check();
        QCOMPARE(updates->state(), QStringLiteral("finish"));
        QVERIFY(waitFor([&] { return m_server.requests.size() >= asked + 3 && !updates->lastLooked().isEmpty(); }));
        QTest::qWait(200);
        QCOMPARE(updates->state(), QStringLiteral("finish"));
        QCOMPARE(updates->line(), line);
    }
    QFile::remove(markerPath());
    {
        // This account runs the older version: after "up to date" the page
        // is back on Restart needed.
        m_runner.installedVersion = Newer;
        auto updates = updater();
        QCOMPARE(updates->state(), QStringLiteral("restart-needed"));
        updates->check();
        QVERIFY(waitFor([&] { return updates->state() != QLatin1String("looking"); }));
        QCOMPARE(updates->state(), QStringLiteral("restart-needed"));
    }
}

// Done shows what the update fixed: the verified release is kept for the
// next start, checked again there, and removed once read.
void UpdateInstallerTest::doneShowsWhatItFixed()
{
    publish(release(Newer));
    plan(QStringLiteral("started read install:%1 exit:0").arg(Newer));
    m_runner.onSystemdRun = [&](const QStringList &) { writeResult(QStringLiteral("done"), Newer); };
    QStringList titles;
    {
        auto updates = updater();
        QVERIFY(findUpdate(*updates));
        titles = updates->noteTitles();
        QVERIFY(!titles.isEmpty());
        updates->update();
        QVERIFY(waitFor([&] { return updates->state() == QLatin1String("finish"); }, 15000));
    }
    QVERIFY(QFile::exists(path(QStringLiteral("state/installing.json"))));
    ArchDockUpdates::Environment next = environment();
    next.release = Newer;
    auto restarted = updater(std::move(next));
    QCOMPARE(restarted->state(), QStringLiteral("done"));
    QCOMPARE(restarted->noteTitles(), titles);
    QVERIFY(!QFile::exists(path(QStringLiteral("state/installing.json"))));
    QVERIFY(!QFile::exists(path(QStringLiteral("state/installing.sig"))));
}

// The last answer counts for a day; one dated in the future (a clock set
// back since) counts as old, so the page looks again.
void UpdateInstallerTest::anAnswerFromTheFutureCountsAsOld()
{
    publish(release(Newer));
    QSettings().setValue(QStringLiteral("updates/lastAnswer"), QDateTime::currentDateTimeUtc().addSecs(-3600));
    {
        auto updates = updater();
        updates->pageOpened();
        QTest::qWait(300);
        QCOMPARE(updates->state(), QStringLiteral("not-looked"));
    }
    QSettings().setValue(QStringLiteral("updates/lastAnswer"), QDateTime::currentDateTimeUtc().addDays(2));
    auto updates = updater();
    updates->pageOpened();
    QVERIFY(waitFor([&] { return found(*updates); }));
}

// Installed, the restart waits for Studio's draft: the page shows the
// Restart step, not "Installing…".
void UpdateInstallerTest::theRestartStepShowsWhileItWaits()
{
    publish(release(Newer));
    plan(QStringLiteral("started read install:%1 exit:0").arg(Newer));
    // The finisher's answer ends the watch at once.
    m_runner.onSystemdRun = [&](const QStringList &) { writeResult(QStringLiteral("not-installed"), Newer); };
    ArchDockUpdates::Environment setup = environment();
    setup.waitLimitMs = 20000;
    auto updates = updater(std::move(setup));
    m_studioDraft = false;
    QVERIFY(findUpdate(*updates));
    updates->update();
    QVERIFY(waitFor([&] { return installing(*updates); }));
    m_studioDraft = true;
    QVERIFY(waitFor([&] { return updates->step() == QLatin1String("restart"); }));
    QCOMPARE(updates->state(), QStringLiteral("updating"));
    QCOMPARE(m_runner.systemdRuns(), 0);
    m_studioDraft = false;
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("finish"); }, 15000));
    QCOMPARE(m_runner.systemdRuns(), 1);
}

// Opening Studio checks Restart needed, so its sidebar dot is current.
void UpdateInstallerTest::openingStudioChecksRestartNeeded()
{
    auto updates = updater();
    QCOMPARE(updates->state(), QStringLiteral("not-looked"));
    QVERIFY(!updates->attention());
    m_runner.installedVersion = Newer;
    updates->studioOpened();
    QCOMPARE(updates->state(), QStringLiteral("restart-needed"));
    QVERIFY(updates->attention());
}

// A release the system cannot take yet is a problem with its own way out,
// and a look once the system is ready offers it.
void UpdateInstallerTest::aReleaseTheSystemCannotTakeIsAProblem()
{
    publish(release(Newer));
    m_runner.unmet = {QStringLiteral("qt6-base>=6.12")};
    auto updates = updater();
    updates->check();
    QVERIFY(waitFor([&] { return updates->state() == QLatin1String("problem"); }));
    QCOMPARE(updates->lineCode(), QStringLiteral("L8"));
    QVERIFY(!updates->canUpdate());
    QVERIFY(!updates->attention());
    m_runner.unmet = {QStringLiteral("polkit")};
    m_runner.absent = {QStringLiteral("polkit")};
    updates->check();
    QVERIFY(waitFor([&] { return updates->lineCode() == QLatin1String("L9"); }));
    QCOMPARE(updates->state(), QStringLiteral("problem"));
    QCOMPARE(updates->installLine(), QStringLiteral("sudo pacman -Syu --needed polkit"));
    m_runner.unmet.clear();
    m_runner.absent.clear();
    updates->check();
    QVERIFY(waitFor([&] { return found(*updates); }));
    QVERIFY(updates->canUpdate());
}

QTEST_GUILESS_MAIN(UpdateInstallerTest)
#include "UpdateInstallerTest.moc"
