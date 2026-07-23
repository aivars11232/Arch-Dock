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

private:
    QQmlApplicationEngine &m_engine;
    std::unique_ptr<PanelWindow> m_panelWindow;
};