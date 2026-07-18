#pragma once

#include <QObject>

class QQmlApplicationEngine;

class PanelManager final : public QObject
{
    Q_OBJECT

public:
    explicit PanelManager(QQmlApplicationEngine& engine, QObject* parent = nullptr);

private:
    QQmlApplicationEngine& m_engine;
};
