#pragma once

#include <QObject>

class PanelWindow final : public QObject
{
    Q_OBJECT

public:
    explicit PanelWindow(QObject* parent = nullptr);
};