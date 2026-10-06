#pragma once

#include <QObject>
#include <memory>

class QQmlApplicationEngine;
class PanelWindow;

// What main() owns once this process is the backend: the PanelWindow, and
// the requests main() makes of it (open Panel Studio or toggle auto-hide
// for its own command line, and the stop a TERM or INT signal asks for).
class PanelManager final : public QObject
{
    Q_OBJECT

public:
    explicit PanelManager(QQmlApplicationEngine &engine, QObject *parent = nullptr);
    ~PanelManager();

    void showSettings();
    void toggleAutoHide();
    void stopIntentionally(const QString &reason);

private:
    QQmlApplicationEngine &m_engine;
    std::unique_ptr<PanelWindow> m_panelWindow;
};