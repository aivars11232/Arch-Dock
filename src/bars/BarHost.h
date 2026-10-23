// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BarRecord.h"
#include "PanelVisibility.h"

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUrl>

class QDBusServiceWatcher;
class QScreen;

namespace ArchDock::Bars
{
class BarCorona;
class BarWindow;
class ReservedStrip;
class ScreenEdgeManager;

// Keeps Arch Dock's bars on the screen as the backend lists them: one window
// per bar, the bars of each screen edge placed side by side, and one strip
// per edge reserving their room. A bar that hides is hidden and shown as
// Plasma's panels are, by the same rules as Arch Dock's edge panels
// (decidePanelVisibility): an auto-hiding bar unless it is in use, a dodging
// one while a window lies over it, one hiding for maximized windows while the
// active window is maximized or full screen; in use means the pointer on it
// or one of its popups open. The bars and the windows
// come from the backend over the session bus and again whenever it says they
// changed, or, for tests, from the files named by ARCHDOCK_BARS_RECORDS_FILE
// and ARCHDOCK_BARS_WINDOWS_FILE, read again when they change.
class BarHost : public QObject
{
    Q_OBJECT

public:
    BarHost(BarCorona *corona, const QUrl &barView, QObject *parent = nullptr);
    ~BarHost() override;

    void start();

    [[nodiscard]] QList<BarRecord> records() const { return m_records; }

private Q_SLOTS:
    void requestRecords();
    void requestWindows();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void readRecordsFile();
    void readWindowsFile();
    void applyRecords(const QList<BarRecord> &records);
    void applyWindows(const QByteArray &json);
    void scheduleLayout();
    void layoutBars();
    void evaluateBars();
    void addDroppedWidget(const QString &barId, const QString &plugin, int index);
    [[nodiscard]] bool popupOpen(const BarWindow *bar) const;
    [[nodiscard]] QScreen *screenFor(const BarRecord &record) const;

    BarCorona *m_corona = nullptr;
    QUrl m_barView;
    QList<BarRecord> m_records;
    QHash<QString, BarWindow *> m_bars;
    QHash<QString, ReservedStrip *> m_strips;
    QTimer m_layoutTimer;
    QTimer m_evaluateTimer;
    QString m_recordsFile;
    QString m_windowsFile;
    QFileSystemWatcher m_fileWatcher;
    QDBusServiceWatcher *m_backendWatcher = nullptr;
    ScreenEdgeManager *m_edges = nullptr;
    QList<ArchDock::WindowOcclusion> m_windows;
    // Plasma's own panels and side windows shown now: a bar keeps out of the
    // way of a tall one (the widget explorer), as Plasma's panels do.
    QList<QRect> m_plasmaSides;
    // A bar's own widget window, kept clear of the same way.
    QRect m_widgetWindow;
    bool m_fetching = false;
    bool m_fetchAgain = false;
    bool m_fetchingWindows = false;
    bool m_fetchWindowsAgain = false;
};
}
