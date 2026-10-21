#pragma once

#include <QDBusServiceWatcher>
#include <QList>
#include <QObject>
#include <QTimer>

// Keeps Arch Dock's bar host (arch-dock-bars, which shows the edge bars that
// Plasma does not) running while there are bars to show, and Arch Dock
// starting at login while there are (BarAutostart). It is started on its
// own, apart from the backend, so that the bars stay on screen if the backend
// stops unexpectedly; a backend started again finds it by its session bus
// name. If it stops on its own it is started again, at most a few times in a
// short while, so that a widget that fails on every start cannot make it
// start over and over. When Arch Dock is stopped, the bars go too.
class BarHostSupervisor final : public QObject
{
    Q_OBJECT

public:
    static constexpr const char *ServiceName = "org.archdock.ArchDock.Bars";

    explicit BarHostSupervisor(QObject *parent = nullptr);

    // Whether there are bars to show: the host is started or asked to quit.
    void setWanted(bool wanted);
    // The bars changed: a host that gave up starting is tried again.
    void barsChanged();
    // Arch Dock is stopping: the host quits and is not started again.
    void stop();

    [[nodiscard]] bool running() const;
    // The program started: ARCHDOCK_BARS_EXECUTABLE, else arch-dock-bars
    // beside the backend, else the one on the PATH.
    [[nodiscard]] static QString program();
    // Whether this process is the session's Arch Dock backend, the only one
    // that starts the bars.
    [[nodiscard]] static bool isTheBackend();

private:
    void startHost();
    void hostGone();
    void askToQuit();

    void syncAutostart();

    bool m_wanted = false;
    bool m_stopped = false;
    bool m_autostartKnown = false;
    QDBusServiceWatcher m_watcher;
    QTimer m_restart;
    // A start that never came up on the session bus counts as a stop.
    QTimer m_startCheck;
    QList<qint64> m_starts;
};
