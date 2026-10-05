#pragma once

#include <QObject>
#include <memory>

class QQmlApplicationEngine;
class PanelWindow;

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