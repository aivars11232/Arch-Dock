// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Arch Dock's Update button: what Panel Studio's Updates page shows and does.
// It looks on GitHub for a newer signed release (once a day by itself while
// the switch is on, or when asked), installs one with the system's password
// window through the root helper, and then has the finisher restart Arch
// Dock and Plasma's panels by the owner's update rule. It also tells an
// account whose Arch Dock was updated elsewhere to restart it.
//
// update(), stop() and restartArchDock() exist only here, on the QML context
// object: nothing on the bus starts, stops or restarts an update, or sets or
// lifts the hold (UpdateHold). That is tidiness, not security: the helper's
// own signature check is the boundary.

#include "updates/CommandRunner.h"
#include "updates/ReleaseSource.h"
#include "updates/UpdateDownload.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

#include <functional>
#include <memory>
#include <optional>

class QNetworkAccessManager;

namespace ArchDock::Updates
{
class ArchDockUpdates : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool offered READ offered CONSTANT)
    Q_PROPERTY(QString runningVersion READ runningVersion CONSTANT)
    // not-looked, looking, up-to-date, found, updating, stopping,
    // restarting, restart-needed, finish, done, problem
    Q_PROPERTY(QString state READ state NOTIFY changed)
    // While updating: download, check, password, installing, restart.
    Q_PROPERTY(QString step READ step NOTIFY changed)
    Q_PROPERTY(QString line READ line NOTIFY changed)
    // The line's code in the owner-facing texts (L1-L33), or empty.
    Q_PROPERTY(QString lineCode READ lineCode NOTIFY changed)
    Q_PROPERTY(QString details READ details NOTIFY changed)
    Q_PROPERTY(QString lastLooked READ lastLooked NOTIFY changed)
    Q_PROPERTY(QString foundVersion READ foundVersion NOTIFY changed)
    Q_PROPERTY(QString releasedOn READ releasedOn NOTIFY changed)
    Q_PROPERTY(qint64 downloadSize READ downloadSize NOTIFY changed)
    Q_PROPERTY(QStringList noteTitles READ noteTitles NOTIFY changed)
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(qint64 received READ received NOTIFY progressChanged)
    Q_PROPERTY(bool canUpdate READ canUpdate NOTIFY changed)
    Q_PROPERTY(bool canStop READ canStop NOTIFY changed)
    Q_PROPERTY(QString installLine READ installLine NOTIFY changed)
    Q_PROPERTY(QString releasesPage READ releasesPage NOTIFY changed)
    Q_PROPERTY(bool holdsSettings READ holdsSettings NOTIFY changed)
    Q_PROPERTY(bool checkAutomatically READ checkAutomatically WRITE setCheckAutomatically NOTIFY changed)
    // A dot on Studio's sidebar entry: an update is ready, or a restart is needed.
    Q_PROPERTY(bool attention READ attention NOTIFY changed)

public:
    // What the updater asks of the rest of Arch Dock.
    struct Hooks
    {
        std::function<bool()> profileBusy;
        std::function<bool()> previewActive;
        std::function<bool()> studioOpen;
        // Studio is open with changes not applied yet.
        std::function<bool()> studioDraft;
        std::function<void()> openStudioOnUpdates;
    };

    // Where the updater finds everything. Arch Dock uses system(); tests give
    // their own runner, folders, stand-in programs and a local server.
    struct Environment
    {
        CommandRunner *runner = nullptr; // nullptr: the backend's own, with its fence
        QString runtimeFolder;           // empty: the stop record's folder
        QString cacheFolder;
        QString stateFolder;
        QString toolFolder;
        QString pkexec;
        QString systemdRun;
        QString helperPath;
        QString finisherPath;
        QString pacmanLock;
        QString release;
        QString repository;
        QString keyId;
        QByteArray keySpki;
        bool testBuild = false;
        int updaterProtocol = 1;
        QString api;
        QString downloads;
        bool offered = false;            // tests only: skips the installed-build checks
        std::function<bool(const QString &name, bool ownOnly)> processRuns;
        // Sends a signal: 0, or kill(2)'s errno. nullptr: kill(2).
        std::function<int(qint64 pid, int signal)> signalProcess;
        // Shows a notice. nullptr: a KNotification.
        std::function<void(const QString &event, const QString &title, const QString &text, const QString &action)> notice;
        // How long the finisher may take to stop this process, and how long a
        // change under way is waited for before the restart.
        int waitLimitMs = 30000;
        [[nodiscard]] static Environment system();
    };
#ifdef ARCHDOCK_TESTING
    // Tests of the whole backend only: what system() gives from now on, or
    // the real places again with std::nullopt.
    static void setSystemForTesting(std::optional<Environment> environment);
#endif

    explicit ArchDockUpdates(Hooks hooks, QObject *parent = nullptr);
    ArchDockUpdates(Hooks hooks, Environment environment, QObject *parent = nullptr);
    ~ArchDockUpdates() override;

    [[nodiscard]] bool offered() const { return m_offered; }
    [[nodiscard]] QString runningVersion() const;
    [[nodiscard]] QString state() const { return m_state; }
    [[nodiscard]] QString step() const { return m_step; }
    [[nodiscard]] QString line() const { return m_line; }
    [[nodiscard]] QString lineCode() const { return m_lineCode; }
    [[nodiscard]] QString details() const { return m_details; }
    [[nodiscard]] QString lastLooked() const;
    [[nodiscard]] QString foundVersion() const { return m_found.manifest.version; }
    [[nodiscard]] QString releasedOn() const;
    [[nodiscard]] qint64 downloadSize() const { return m_found.manifest.package.size; }
    [[nodiscard]] QStringList noteTitles() const;
    [[nodiscard]] QString notes() const { return m_found.manifest.releaseNotes; }
    [[nodiscard]] qint64 received() const { return m_received; }
    [[nodiscard]] bool canUpdate() const;
    [[nodiscard]] bool canStop() const;
    [[nodiscard]] QString installLine() const;
    [[nodiscard]] QString releasesPage() const;
    [[nodiscard]] bool holdsSettings() const;
    [[nodiscard]] bool checkAutomatically() const { return m_checkAutomatically; }
    void setCheckAutomatically(bool on);
    [[nodiscard]] bool attention() const;

    // The page was opened: a look when the last answer is a day old.
    Q_INVOKABLE void pageOpened();
    Q_INVOKABLE void check();
    // Update Now, once unapplied Studio changes were applied or discarded.
    Q_INVOKABLE void update();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void restartArchDock();
    // Studio is closing: an update before the password was accepted stops.
    Q_INVOKABLE void studioClosing();
    // Studio opened: Restart needed is checked, so its dot is current.
    void studioOpened();

    // Whether Quit must wait or be refused (L32): from "started", and while
    // the finisher restarts Arch Dock. Before "started" Quit stops the update.
    [[nodiscard]] bool quitRefused() const;
    // Quit before "started": the update stops first; `then` runs when it has.
    void stopForQuit(std::function<void(bool refused)> then);
    // A refused Quit (L32) as a notice: the menu's Quit has no window of its own.
    void tellQuitRefused();

Q_SIGNALS:
    void changed();
    void progressChanged();

private Q_SLOTS:
    void checkRestartNeeded();
    void sessionPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    struct Marker;
    QString lockSteps() const;

    void setState(const QString &state, const QString &line = {}, const QString &details = {});
    // The update ends before the restart: the hold goes, the page shows
    // `state`, and a Quit that waited for a Stop goes on.
    void endUpdate(const QString &state, const QString &line = {}, const QString &details = {});
    void quitAfterStop();
    bool answerFresh() const;
    void lookedAt(const LookResult &result);
    void dailyTick();
    void readAfterRestart();
    bool guardsAllow(QString *line) const;
    void startDownload();
    void downloaded(UpdateDownload::Result result, const QString &details);
    void checkDownload();
    bool lockSeen(QString *line) const;
    bool roomFor(qint64 size) const;
    bool backup();
    void startPkexec();
    void pkexecOutput();
    void passwordAccepted();
    void pkexecEnded(int exitCode, QProcess::ExitStatus status);
    void endAfterHelper(int code, const QString &pkexecText, bool stopped);
    // The helper's errors and the lines it passed on (pacman's), for Copy Details.
    QString helperDetails(const QString &errors) const;
    void waitForPacman(std::function<void()> then);
    void installed();
    void startFinisher(const QString &version);
    void watchResult(const QString &version);
    void problemAfterPassword(const QString &line, const QString &details = {});
    // The verified release being installed, kept for the next start's Done.
    void keepVerifiedRelease();
    void loadKeptRelease(const QString &version);
    QString keptFile(const QString &name) const;
    void notify(const QString &event, const QString &title, const QString &text, const QString &action,
                bool whileStudioOpen = false);
    void notifyOncePerLogin(const QString &kind, const QString &version, const QString &event, const QString &title,
                            const QString &text);
    QString lineText(const QString &code) const;
    CommandRunner &runner() const;
    bool running(const QString &name, bool ownOnly) const;
    QString runtimeFile(const QString &name) const;
    QString markerPath() const { return runtimeFile(QStringLiteral("arch-dock-update.json")); }
    QString resultPath() const { return runtimeFile(QStringLiteral("arch-dock-update-result.json")); }
    QString cacheFolder() const { return m_env.cacheFolder; }
    QString stateFolder() const { return m_env.stateFolder; }

    Hooks m_hooks;
    Environment m_env;
    mutable BackendCommandRunner m_runner;
    std::unique_ptr<QNetworkAccessManager> m_network;
    std::unique_ptr<ReleaseSource> m_source;
    std::unique_ptr<UpdateDownload> m_download;
    QPointer<QProcess> m_pkexec;
    QTimer m_tick;
    QTimer m_resultWatch;
    bool m_offered = false;
    bool m_checkAutomatically = true;
    QString m_state = QStringLiteral("not-looked");
    QString m_step;
    QString m_line;
    QString m_lineCode;
    QString m_details;
    LookResult m_found;
    QString m_packagePath;
    qint64 m_received = 0;
    QByteArray m_input;
    qint64 m_sent = 0;
    bool m_started = false;
    bool m_stopRequested = false;
    bool m_updateRunning = false;
    bool m_restarting = false;
    QByteArray m_pkexecErrors;
    QByteArray m_pkexecOutput;
    QByteArray m_helperOutput;
    QElapsedTimer m_restartClock;
    int m_finisherRuns = 0;
    QDateTime m_lastAnswer;
    QDateTime m_retryAfter;
    QString m_restartTo;
    std::function<void(bool)> m_afterStop;
};
} // namespace ArchDock::Updates
