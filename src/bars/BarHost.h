// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "BarRecord.h"

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

// Keeps Arch Dock's bars on the screen as the backend lists them: one window
// per bar, the bars of each screen edge placed side by side, and one strip
// per edge reserving their room. The list comes from the backend over the
// session bus and again whenever the backend says it changed, or, for tests,
// from a file named by ARCHDOCK_BARS_RECORDS_FILE, read again when it
// changes.
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

private:
    void readRecordsFile();
    void applyRecords(const QList<BarRecord> &records);
    void scheduleLayout();
    void layoutBars();
    [[nodiscard]] QScreen *screenFor(const BarRecord &record) const;

    BarCorona *m_corona = nullptr;
    QUrl m_barView;
    QList<BarRecord> m_records;
    QHash<QString, BarWindow *> m_bars;
    QHash<QString, ReservedStrip *> m_strips;
    QTimer m_layoutTimer;
    QString m_recordsFile;
    QFileSystemWatcher m_fileWatcher;
    QDBusServiceWatcher *m_backendWatcher = nullptr;
    bool m_fetching = false;
    bool m_fetchAgain = false;
};
}
