#pragma once

#include <QObject>

#include "../WindowModel.h"
#include "../WindowWatcher.h"

class QQmlApplicationEngine;

class PanelWindow final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(Position position READ position CONSTANT)

public:
    enum class Position
    {
        Bottom,
        Top,
        Left,
        Right,
        Free
    };
    Q_ENUM(Position)

    explicit PanelWindow(QQmlApplicationEngine &engine,
                         QObject *parent = nullptr);

    [[nodiscard]] Position position() const
    {
        return m_position;
    }

private:
    QQmlApplicationEngine &m_engine;
    WindowModel m_windowModel;
    WindowWatcher m_windowWatcher;
    Position m_position = Position::Bottom;
};